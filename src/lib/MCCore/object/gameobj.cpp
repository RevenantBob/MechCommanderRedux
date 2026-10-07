#include "stdafx.h"
#include "object/gameobj.h"
#include "ai/move.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "lib/aerror.h"
#include "lib/file.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/cmponent.h"
#include "object/contact.h"
#include "object/mover.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/objtype.h"
#include "object/team.h"
#include "object/turret.h"
#include "terrain/terrain.h"

int32_t ObjCellArray[9] = {};
char ChunkDebugMsg[0x1400] = {}; // 0x1408 bytes lie before the next global.
float BlockCaptureRange = 0.0f;

namespace
{
    /// <summary>The eighth turn the facing math rotates the frame by (0x3ffec90fdaa21f446000).</summary>
    constexpr double EIGHTH_TURN = 0x1.921fb5443e88cp-1;
    /// <summary>Degrees per radian, at float precision (0x4004e52ee10000000000).</summary>
    constexpr double RADIANS_TO_DEGREES_F = 0x1.ca5dc2p+5;

    /// <summary>Whether <paramref name="object"/> is a mover (mech, vehicle, elemental or plain mover).</summary>
    bool IsMover(const MCBaseObject* object)
    {
        const MCObjectClass objectClass = object->ObjectClass;
        return objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
               objectClass == MOVER;
    }

    /// <summary>
    /// The quadrant an entry angle comes from, as the chunks pack it: 0 front (-45 to 45), 2 left (-135 to -45),
    /// 3 right (45 to 135), 1 rear.
    /// </summary>
    int8_t AngleQuadrant(float angle)
    {
        if (angle >= -45.0f && angle <= 45.0f)
        {
            return 0;
        }

        if (angle > -135.0f && angle < -45.0f)
        {
            return 2;
        }

        if (angle > 45.0f && angle < 135.0f)
        {
            return 3;
        }

        return 1;
    }

    /// <summary>An entry angle snapped to its quadrant's middle (0, -90, 90 or 180), for packed multiplayer damage.</summary>
    float SnapAngle(float angle)
    {
        if (angle >= -45.0f && angle <= 45.0f)
        {
            return 0.0f;
        }

        if (angle > -135.0f && angle < -45.0f)
        {
            return -90.0f;
        }

        if (angle > 45.0f && angle < 135.0f)
        {
            return 90.0f;
        }

        return 180.0f;
    }

    /// <summary>Damage rounded down to quarter points, as the multiplayer chunks carry it.</summary>
    float QuarterPoints(float damage)
    {
        return static_cast<float>(static_cast<int32_t>(static_cast<double>(damage) * 4.0) * 0.25);
    }

    /// <summary>
    /// Appends the target line both debug routines print: the mover (roster index), terrain object, train car or
    /// camera drone (part id), or map point, or else three question marks with the terrain objects of the target's
    /// vertex listed.
    /// </summary>
    /// <param name="itemNumber">The item number used for the vertex listing (see OB-010).</param>
    /// <param name="cellC">The cell column used for a map point (see OB-010).</param>
    void AppendTargetLine(int8_t targetType, int32_t targetId, uint16_t cellR, uint16_t cellC, int8_t itemNumber,
                          bool hitChunk)
    {
        char line[512];
        MCBaseObject* target = nullptr;
        bool haveTarget = false;

        if (targetType == 0)
        {
            target = MPlayer->MoverRoster[targetId];
            haveTarget = true;
        }
        else if (targetType == 1 || targetType == 2)
        {
            target = ObjectList->FindObjectFromPart(targetId);
            haveTarget = true;
        }
        else if (targetType == 3)
        {
            if (hitChunk)
            {
                std::strcat(ChunkDebugMsg, "target point\n");
                return;
            }

            // The middle of the target cell, on the ground.
            const float halfSide = WorldUnitsMapSide * 0.5f;
            MCVector3D point;
            point.X = static_cast<float>((cellC + 0.5f) * static_cast<double>(MetersPerCell) - halfSide);
            point.Y = static_cast<float>((static_cast<double>(halfSide) - cellR * static_cast<double>(MetersPerCell)) -
                                         static_cast<double>(MetersPerCell) * 0.5f);
            point.Z = 0.0f;
            const float elevation = GameMap->GetTerrainElevation(point);
            std::snprintf(line, sizeof(line), "target point = (%f, %f, %f)\n", static_cast<double>(point.X),
                          static_cast<double>(point.Y), static_cast<double>(elevation));
            std::strcat(ChunkDebugMsg, line);
            return;
        }

        if (haveTarget && target != nullptr)
        {
            if (IsMover(target))
            {
                std::snprintf(line, sizeof(line), "target = %s (%d)\n",
                              static_cast<MCMover*>(target)->DebugStatus.c_str(), target->PartId);
            }
            else
            {
                std::snprintf(line, sizeof(line), "target = objClass %d (%d)\n", static_cast<int>(target->ObjectClass),
                              target->PartId);
            }

            std::strcat(ChunkDebugMsg, line);
            return;
        }

        std::strcat(ChunkDebugMsg, "target = ???\n");

        if (hitChunk || targetType != 1)
        {
            return;
        }

        // List the terrain objects on the target's vertex.
        const int32_t firstId = targetId - itemNumber;
        int32_t numObjects = 0;

        for (int32_t i = 0; i < 8; i++)
        {
            MCBaseObject* object = ObjectList->FindObjectFromPart(firstId + i);

            if (object == nullptr)
            {
                continue;
            }

            numObjects++;
            std::snprintf(line, sizeof(line), "    %d: objClass %d (%d)\n", i, static_cast<int>(object->ObjectClass),
                          object->PartId);
            std::strcat(ChunkDebugMsg, line);
        }

        if (numObjects > 0)
        {
            std::snprintf(line, sizeof(line), "    There are %d terrain objects in this tile.\n", numObjects);
            std::strcat(ChunkDebugMsg, line);
        }
    }

    /// <summary>
    /// Appends one weapon fire chunk's fields. <paramref name="first"/> is the first chunk of the pair, whose cell
    /// column and item number the original prints for the second one too.
    /// </summary>
    void AppendWeaponFireChunk(const MCWeaponFireChunk* chunk, const MCWeaponFireChunk* first)
    {
        // Original behaviour (OB-010): the second chunk's target point and terrain listing use the first chunk's
        // cell column and item number.
        const uint16_t cellC = first->TargetCell[1];
        AppendTargetLine(chunk->TargetType, chunk->TargetId, chunk->TargetCell[0], cellC, first->TargetItemNumber,
                         false);

        char line[512];
        std::snprintf(line, sizeof(line), "targetType = %d\n", static_cast<int>(chunk->TargetType));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetId = %d\n", chunk->TargetId);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetBlockOrTrainNumber = %d\n", chunk->TargetBlockOrTrainNumber);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetVertexOrCarNumber = %d\n", chunk->TargetVertexOrCarNumber);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetItemNumber = %d\n", static_cast<int>(chunk->TargetItemNumber));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetCellRC = (%d, %d)\n", static_cast<int>(chunk->TargetCell[0]),
                      static_cast<int>(cellC));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "weaponIndex = %d\n", static_cast<int>(chunk->WeaponIndex));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "hit = %c\n", chunk->Hit != 0 ? 'T' : 'N');
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "entryAngle = %d\n", static_cast<int>(chunk->EntryAngle));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "numMissiles = %d\n", static_cast<int>(chunk->NumMissiles));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "numMissilesHit = %d\n", static_cast<int>(chunk->NumMissilesPastAms));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "numAntiMissiles = %d\n", static_cast<int>(chunk->NumAntiMissileShots));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "hitLocation = %d\n", static_cast<int>(chunk->HitLocation));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "data = %x\n", chunk->Data);
        std::strcat(ChunkDebugMsg, line);
    }

    /// <summary>Appends one weapon hit chunk's fields.</summary>
    void AppendWeaponHitChunk(const MCWeaponHitChunk* chunk)
    {
        AppendTargetLine(chunk->TargetType, chunk->TargetId, 0, 0, 0, true);

        char line[512];
        std::snprintf(line, sizeof(line), "targetType = %d\n", static_cast<int>(chunk->TargetType));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetId = %d\n", chunk->TargetId);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetBlockOrTrainNumber = %d\n", chunk->TargetBlockOrTrainNumber);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetVertexOrCarNumber = %d\n", chunk->TargetVertexOrCarNumber);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetItemNumber = %d\n", static_cast<int>(chunk->TargetItemNumber));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "cause = %d\n", static_cast<int>(chunk->Cause));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "damage = %f\n", static_cast<double>(chunk->Damage));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "hitLocation = %d\n", static_cast<int>(chunk->HitLocation));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "entryAngle = %d\n", static_cast<int>(chunk->EntryAngle));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "refit = %s\n", chunk->Refit != 0 ? "TRUE" : "FALSE");
        std::strcat(ChunkDebugMsg, line);
    }

    /// <summary>Writes ChunkDebugMsg to <paramref name="fileName"/> and hands it to the crash handler.</summary>
    void SaveChunkDebugMsg(const char* fileName)
    {
        auto* file = new MCFile;
        file->Create(fileName);
        file->WriteString(ChunkDebugMsg);
        file->Close();
        delete file;
        ExceptionGameMsg = ChunkDebugMsg;
    }

    /// <summary>The frame's i and j axes turned by an eighth turn, as the facing math uses them.</summary>
    MCFrameOfRef TurnedFrame(const MCFrameOfRef& frame)
    {
        const float s = static_cast<float>(std::sin(EIGHTH_TURN));
        const float c = static_cast<float>(std::cos(EIGHTH_TURN));
        MCFrameOfRef turned = frame;
        turned.I = frame.I * c + frame.J * s;
        turned.J = frame.J * c - frame.I * s;
        return turned;
    }

    /// <summary>The tile under <paramref name="position"/>.</summary>
    MCMapTile& TileAt(const MCVector3D& position)
    {
        int32_t tileR;
        int32_t tileC;
        int32_t cellR;
        int32_t cellC;
        GameMap->WorldToMapPos(position, tileR, tileC, cellR, cellC);
        return GameMap->Map[GameMap->Width * tileR + tileC];
    }
}

//---------------------------------------------------------------------------
// _WeaponShotInfo
//---------------------------------------------------------------------------

auto MCWeaponShotInfo::Init(MCGameObject* shooter, int32_t weaponMasterId, float shotDamage, int32_t shotHitLocation,
                            float shotEntryAngle) -> void
{
    Attacker = shooter;

    if (MPlayer == nullptr && shooter != nullptr)
    {
        // The difficulty scales the player's shots, and the enemy's mechs, vehicles, elementals and turrets.
        const MCObjectClass shooterClass = shooter->ObjectClass;
        const int player = shooter->GetAlignment() == HomeTeam->Alignment ? 1 : 0;

        if (player != 0 || shooterClass == BATTLEMECH || shooterClass == GROUNDVEHICLE || shooterClass == ELEMENTAL ||
            shooterClass == TURRET)
        {
            shotDamage = ApplyDifficultyWeapon(shotDamage, player);
        }
    }

    Damage = shotDamage;
    MasterId = weaponMasterId;
    HitLocation = shotHitLocation;
    EntryAngle = shotEntryAngle;
    Assert(shotDamage >= 0.0 && shotDamage <= 255.0, static_cast<int32_t>(shotDamage),
           " WeaponShotInfo.init: damage out of range ");

    if (MPlayer != nullptr && MPlayer->IsServer != 0)
    {
        Damage = QuarterPoints(shotDamage);
        EntryAngle = SnapAngle(shotEntryAngle);
    }
}

auto MCWeaponShotInfo::SetDamage(float shotDamage) -> void
{
    Damage = shotDamage;

    if (MPlayer != nullptr && MPlayer->IsServer != 0)
    {
        Damage = QuarterPoints(shotDamage);
    }
}

auto MCWeaponShotInfo::SetEntryAngle(float shotEntryAngle) -> void
{
    EntryAngle = shotEntryAngle;

    if (MPlayer != nullptr && MPlayer->IsServer != 0)
    {
        EntryAngle = SnapAngle(shotEntryAngle);
    }
}

//---------------------------------------------------------------------------
// WeaponFireChunk
//---------------------------------------------------------------------------

auto MCWeaponFireChunk::Init() -> void
{
    HitLocation = -1;
    TargetType = 0;
    TargetId = 0;
    TargetBlockOrTrainNumber = 0;
    TargetVertexOrCarNumber = 0;
    TargetItemNumber = 0;
    TargetCell[0] = 0;
    TargetCell[1] = 0;
    WeaponIndex = 0;
    Hit = 0;
    EntryAngle = 0;
    NumMissiles = 0;
    NumMissilesPastAms = 0;
    NumAntiMissileShots = 0;
    Data = 0;
}

auto MCWeaponFireChunk::BuildMoverTarget(MCBigGameObject* target, int32_t weapon, int hitTarget, float angle,
                                         int32_t missiles, int32_t missilesPastAMS, int32_t antiMissileShots,
                                         int32_t location) -> void
{
    const int32_t rosterIndex = static_cast<MCMover*>(target)->NetRosterIndex;
    Hit = hitTarget;
    TargetType = 0;
    TargetId = rosterIndex;
    WeaponIndex = static_cast<uint8_t>(weapon);
    EntryAngle = AngleQuadrant(angle);
    NumMissilesPastAms = static_cast<int8_t>(missilesPastAMS);
    NumMissiles = static_cast<int8_t>(missiles);
    NumAntiMissileShots = static_cast<int8_t>(antiMissileShots);
    HitLocation = static_cast<int8_t>(location);
    Assert(rosterIndex >= 0 && rosterIndex < MPlayer->NumMovers, rosterIndex,
           " WeaponFireChunk.buildMoverTarget: bad targetId ");
    Assert(WeaponIndex < 0x20, WeaponIndex, " WeaponFireChunk.buildMoverTarget: bad weaponIndex ");
    Assert(NumMissiles >= 0 && NumMissiles <= 15, NumMissiles, " WeaponFireChunk.buildMoverTarget: bad numMissiles ");
    Assert(NumMissilesPastAms >= 0 && NumMissilesPastAms <= 15, NumMissilesPastAms,
           " WeaponFireChunk.buildMoverTarget: bad numMissilesHit ");
    Assert(NumAntiMissileShots >= 0 && NumAntiMissileShots <= 15, NumAntiMissileShots,
           " WeaponFireChunk.buildMoverTarget: bad numAntiMissiles ");
    Assert(HitLocation >= -1 && HitLocation <= 11, HitLocation, " WeaponFireChunk.buildMoverTarget: bad hitLocation ");
    Data = 0;
}

auto MCWeaponFireChunk::BuildTerrainTarget(MCBigGameObject* target, int32_t weapon, int hitTarget, int32_t missiles)
    -> void
{
    // A terrain object's part id is 0x1000 + (block * 400 + vertex) * 8 + item.
    const int32_t partId = target->PartId;
    TargetType = 1;
    TargetId = partId;
    TargetBlockOrTrainNumber = (partId - 0x1000) / 0xc80;
    const int32_t rest = (partId - 0x1000) % 0xc80;
    TargetVertexOrCarNumber = rest / 8;
    WeaponIndex = static_cast<uint8_t>(weapon);
    NumMissiles = static_cast<int8_t>(missiles);
    NumMissilesPastAms = static_cast<int8_t>(missiles);
    TargetItemNumber = static_cast<int8_t>(rest - TargetVertexOrCarNumber * 8);
    Hit = hitTarget;
    Assert(partId != -1, target->ObjectClass, " WeaponFireChunk.buildTerrainTarget: -1 partId ");
    Data = 0;
}

auto MCWeaponFireChunk::BuildTrainTarget(MCBigGameObject* target, int32_t weapon, int hitTarget, float angle,
                                         int32_t missiles) -> void
{
    // A train car's part id is 0x7d000 + train * 100 + car.
    TargetType = 2;
    TargetId = target->PartId;
    TargetBlockOrTrainNumber = (target->PartId - 0x7d000) / 100;
    WeaponIndex = static_cast<uint8_t>(weapon);
    Hit = hitTarget;
    TargetVertexOrCarNumber = (target->PartId - 0x7d000) % 100;
    EntryAngle = AngleQuadrant(angle);
    NumMissiles = static_cast<int8_t>(missiles);
    NumMissilesPastAms = static_cast<int8_t>(missiles);
    Data = 0;
}

auto MCWeaponFireChunk::BuildCameraDroneTarget(MCBigGameObject* target, int32_t weapon, int hitTarget, float angle,
                                               int32_t missiles) -> void
{
    WeaponIndex = static_cast<uint8_t>(weapon);
    TargetId = target->PartId;
    TargetVertexOrCarNumber = target->PartId - 0x802c8;
    Hit = hitTarget;
    TargetType = 2;
    TargetBlockOrTrainNumber = 0x80;
    EntryAngle = AngleQuadrant(angle);
    NumMissiles = static_cast<int8_t>(missiles);
    NumMissilesPastAms = static_cast<int8_t>(missiles);
    Data = 0;
}

auto MCWeaponFireChunk::BuildLocationTarget(MCVector3D location, int32_t weapon, int hitTarget, int32_t missiles)
    -> void
{
    TargetType = 3;
    int32_t cellR;
    int32_t cellC;
    WorldCoordToMapCell(location, cellR, cellC);
    TargetCell[0] = static_cast<uint16_t>(cellR);
    Hit = hitTarget;
    TargetCell[1] = static_cast<uint16_t>(cellC);
    WeaponIndex = static_cast<uint8_t>(weapon);
    NumMissiles = static_cast<int8_t>(missiles);
    NumMissilesPastAms = static_cast<int8_t>(missiles);
    Data = 0;
}

auto MCWeaponFireChunk::Pack() -> void
{
    // From the low bit: target type (2), weapon index (5), hit (1), then the target, and for a missile weapon the
    // missile counts (4 bits each) in front of it.
    Data = 0;
    uint32_t packed;
    bool packTarget = true;

    switch (TargetType)
    {
        case 0:
        {
            packed = static_cast<uint32_t>((HitLocation + 2) * 0x20) | (static_cast<uint32_t>(EntryAngle) << 9) |
                     static_cast<uint32_t>(TargetId);
            Data = packed;

            if (NumMissiles > 0)
            {
                packed = ((packed << 4 | static_cast<uint32_t>(NumMissiles)) << 4) |
                         static_cast<uint32_t>(NumMissilesPastAms);
                Data = packed << 4 | static_cast<uint32_t>(NumAntiMissileShots);
            }
            break;
        }
        case 1:
        {
            packed =
                ((static_cast<uint32_t>(TargetBlockOrTrainNumber) << 9 | static_cast<uint32_t>(TargetVertexOrCarNumber))
                 << 3) |
                static_cast<uint32_t>(TargetItemNumber);
            Data = packed;

            if (NumMissiles > 0)
            {
                Data = packed << 4 | static_cast<uint32_t>(NumMissiles);
            }
            break;
        }
        case 2:
        {
            packed = ((static_cast<uint32_t>(EntryAngle) << 8 | static_cast<uint32_t>(TargetBlockOrTrainNumber)) << 8) |
                     static_cast<uint32_t>(TargetVertexOrCarNumber);
            Data = packed;

            if (NumMissiles > 0)
            {
                Data = packed << 4 | static_cast<uint32_t>(NumMissiles);
            }
            break;
        }
        case 3:
        {
            packed = static_cast<uint32_t>(TargetCell[0]) << 10 | TargetCell[1];
            Data = packed;

            if (NumMissiles > 0)
            {
                Data = packed << 4 | static_cast<uint32_t>(NumMissiles);
            }
            break;
        }
        default:
            packTarget = false;
            break;
    }

    if (packTarget)
    {
        Data = Data << 1;
    }

    if (Hit != 0)
    {
        Data |= 1;
    }

    Data = ((static_cast<uint32_t>(WeaponIndex) | Data << 5) << 2) | static_cast<uint32_t>(TargetType);
}

auto MCWeaponFireChunk::Unpack(MCBigGameObject* attacker) -> void
{
    const uint32_t packed = Data;
    WeaponIndex = static_cast<uint8_t>((packed >> 2) & 0x1f);
    TargetType = static_cast<int8_t>(packed & 3);
    Hit = static_cast<int32_t>((packed >> 7) & 1);
    uint32_t rest = packed >> 8;

    int missileWeapon = 0;

    if (IsMover(attacker))
    {
        auto* mover = static_cast<MCMover*>(attacker);
        missileWeapon = mover->IsWeaponMissile(mover->NumOther + WeaponIndex);
    }
    else if (attacker->ObjectClass == TURRET)
    {
        missileWeapon = static_cast<MCTurret*>(attacker)->IsWeaponMissile();
    }

    const uint8_t low = static_cast<uint8_t>(packed >> 8);

    switch (TargetType)
    {
        case 0:
        {
            if (missileWeapon != 0)
            {
                NumAntiMissileShots = static_cast<int8_t>(low & 0xf);
                NumMissilesPastAms = static_cast<int8_t>((packed >> 12) & 0xf);
                NumMissiles = static_cast<int8_t>((packed >> 16) & 0xf);
                rest = packed >> 20;
            }

            TargetId = static_cast<int32_t>(rest & 0x1f);
            HitLocation = static_cast<int8_t>(((rest >> 5) & 0xf) - 2);
            EntryAngle = static_cast<int8_t>((rest >> 9) & 3);
            return;
        }
        case 1:
        {
            if (missileWeapon != 0)
            {
                NumMissiles = static_cast<int8_t>(low & 0xf);
                rest = packed >> 12;
                NumMissilesPastAms = static_cast<int8_t>(low & 0xf);
            }

            const int8_t item = static_cast<int8_t>(rest & 7);
            const uint32_t block = (rest >> 12) & 0xff;
            TargetItemNumber = item;
            const uint32_t vertex = (rest >> 3) & 0x1ff;
            TargetBlockOrTrainNumber = static_cast<int32_t>(block);
            TargetVertexOrCarNumber = static_cast<int32_t>(vertex);
            TargetId = static_cast<int32_t>(item + 0x1000 + (block * 400 + vertex) * 8);
            return;
        }

        case 2:
        {
            if (missileWeapon != 0)
            {
                NumMissiles = static_cast<int8_t>(low & 0xf);
                rest = packed >> 12;
                NumMissilesPastAms = static_cast<int8_t>(low & 0xf);
            }

            const uint32_t trainNumber = (rest >> 8) & 0xff;
            TargetVertexOrCarNumber = static_cast<int32_t>(rest & 0xff);
            TargetBlockOrTrainNumber = static_cast<int32_t>(trainNumber);
            const uint8_t angleBits = static_cast<uint8_t>(rest >> 16);

            if (trainNumber != 0x80)
            {
                EntryAngle = static_cast<int8_t>(angleBits & 3);
                TargetId = TargetVertexOrCarNumber + static_cast<int32_t>((trainNumber * 5 + 0x6400) * 0x14);
                return;
            }

            TargetId = TargetVertexOrCarNumber + 0x802c8;
            EntryAngle = static_cast<int8_t>(angleBits & 3);
            return;
        }

        case 3:
        {
            if (missileWeapon != 0)
            {
                NumMissiles = static_cast<int8_t>(low & 0xf);
                rest = packed >> 12;
                NumMissilesPastAms = static_cast<int8_t>(low & 0xf);
            }

            TargetCell[1] = static_cast<uint16_t>(rest & 0x3ff);
            TargetCell[0] = static_cast<uint16_t>((rest >> 10) & 0x3ff);
            return;
        }
        default:
            return;
    }
}

auto MCWeaponFireChunk::EqualTo(MCWeaponFireChunk* chunk) -> int
{
    if (TargetType != chunk->TargetType || TargetId != chunk->TargetId || TargetCell[0] != chunk->TargetCell[0] ||
        TargetCell[1] != chunk->TargetCell[1] || WeaponIndex != chunk->WeaponIndex || Hit != chunk->Hit ||
        EntryAngle != chunk->EntryAngle || NumMissiles != chunk->NumMissiles ||
        NumMissilesPastAms != chunk->NumMissilesPastAms || NumAntiMissileShots != chunk->NumAntiMissileShots ||
        HitLocation != chunk->HitLocation)
    {
        DebugWeaponFireChunk(this, chunk, nullptr);
        return 0;
    }

    return 1;
}

//---------------------------------------------------------------------------
// WeaponHitChunk
//---------------------------------------------------------------------------

auto MCWeaponHitChunk::BuildMoverTarget(MCBigGameObject* target, int32_t hitCause, float hitDamage, int32_t location,
                                        float angle, int isRefit) -> void
{
    TargetType = 0;
    TargetId = static_cast<MCMover*>(target)->NetRosterIndex;
    Cause = static_cast<int8_t>(hitCause);
    Damage = hitDamage;
    HitLocation = static_cast<int8_t>(location);
    EntryAngle = AngleQuadrant(angle);
    Refit = isRefit;
    Data = 0;
}

auto MCWeaponHitChunk::BuildTerrainTarget(MCBigGameObject* target, float hitDamage) -> void
{
    TargetType = 1;
    Data = 0;
    const int32_t partId = target->PartId;
    TargetId = partId;
    TargetBlockOrTrainNumber = (partId - 0x1000) / 0xc80;
    const int32_t rest = (partId - 0x1000) % 0xc80;
    TargetVertexOrCarNumber = rest / 8;
    TargetItemNumber = static_cast<int8_t>(rest - TargetVertexOrCarNumber * 8);
    Damage = hitDamage;
}

auto MCWeaponHitChunk::BuildTrainTarget(MCBigGameObject* target, float hitDamage, float angle) -> void
{
    TargetType = 2;
    TargetId = target->PartId;
    TargetBlockOrTrainNumber = (target->PartId - 0x7d000) / 100;
    Damage = hitDamage;
    TargetVertexOrCarNumber = (target->PartId - 0x7d000) % 100;
    EntryAngle = AngleQuadrant(angle);
    Data = 0;
}

auto MCWeaponHitChunk::BuildCameraDroneTarget(MCBigGameObject* target, float hitDamage, float angle) -> void
{
    TargetType = 2;
    TargetId = target->PartId;
    TargetVertexOrCarNumber = target->PartId - 0x802c8;
    TargetBlockOrTrainNumber = 0x80;
    Damage = hitDamage;
    EntryAngle = AngleQuadrant(angle);
    Data = 0;
}

auto MCWeaponHitChunk::Build(MCGameObject* target, MCWeaponShotInfo* shotInfo, int isRefit) -> void
{
    if (target == nullptr)
    {
        Fatal(0, " WeaponHitChunk.build: NULL target ");
    }

    const float shotDamage = shotInfo->Damage;
    Assert(static_cast<double>(shotDamage) == static_cast<int32_t>(static_cast<double>(shotInfo->Damage) * 4.0) * 0.25,
           0, " WeaponHitChunk.build: damage round error ");

    if (!IsMover(target))
    {
        switch (target->ObjectClass)
        {
            case BUILDING:
            case TREE:
            case MISCTERRAINOBJECT:
            case TREEBUILDING:
            case TURRET:
            case GATE:
            {
                BuildTerrainTarget(static_cast<MCBigGameObject*>(target), shotDamage);
                return;
            }
            case CAMERADRONE:
            {
                BuildCameraDroneTarget(static_cast<MCBigGameObject*>(target), shotDamage, shotInfo->EntryAngle);
                return;
            }
            case TRAINCAR:
            {
                BuildTrainTarget(static_cast<MCBigGameObject*>(target), shotDamage, shotInfo->EntryAngle);
                return;
            }
            default:
                return;
        }
    }

    // The chunk's cause: 0 for a weapon, -4 for a component whose form is 10. The shot keeps the change.
    if (shotInfo->MasterId > 0)
    {
        shotInfo->MasterId = MasterComponentList[shotInfo->MasterId].Form == 10 ? -4 : 0;
    }

    BuildMoverTarget(static_cast<MCBigGameObject*>(target), shotInfo->MasterId, shotDamage, shotInfo->HitLocation,
                     shotInfo->EntryAngle, isRefit);
}

auto MCWeaponHitChunk::Pack() -> void
{
    // From the low bit: target type (2), damage in quarter points (10), then the target.
    const uint32_t type = static_cast<uint32_t>(static_cast<int32_t>(TargetType));
    Data = 0;

    if (type == 0)
    {
        if (Refit != 0)
        {
            Data = 1;
        }

        Data = ((static_cast<uint32_t>(EntryAngle) | Data << 2) << 12 | static_cast<uint32_t>(TargetId)) << 10 |
               static_cast<uint32_t>((HitLocation + 2) * 0x40000) | static_cast<uint32_t>((Cause + 7) * 0x8000);
    }
    else if (type == 1)
    {
        Data = ((static_cast<uint32_t>(TargetBlockOrTrainNumber) << 9 | static_cast<uint32_t>(TargetVertexOrCarNumber))
                    << 3 |
                static_cast<uint32_t>(TargetItemNumber))
               << 10;
    }
    else if (type == 2)
    {
        Data = ((static_cast<uint32_t>(EntryAngle) << 8 | static_cast<uint32_t>(TargetBlockOrTrainNumber)) << 8 |
                static_cast<uint32_t>(TargetVertexOrCarNumber))
               << 10;
    }
    else
    {
        Fatal(0, " Bad WeaponHitChunk Target Type ");
    }

    const uint32_t quarters = static_cast<uint32_t>(static_cast<int32_t>(static_cast<double>(Damage) * 4.0));
    Data = (quarters | Data) << 2 | type;
}

auto MCWeaponHitChunk::Unpack() -> void
{
    const uint32_t packed = Data;
    TargetType = static_cast<int8_t>(packed & 3);
    const uint32_t rest = packed >> 12;
    Damage = static_cast<float>(((packed >> 2) & 0x3ff) * 0.25);
    Assert(Damage >= 0.0 && Damage <= 255.0, 0, " WeaponHitChunk.unpack: bad damage ");
    const uint8_t high = static_cast<uint8_t>(packed >> 24);

    if (TargetType == 0)
    {
        TargetId = static_cast<int32_t>(rest & 0x1f);
        Assert(TargetId < MPlayer->NumMovers, TargetId, " WeaponHitChunk.unpack: bad targetId ");
        Cause = static_cast<int8_t>(((packed >> 17) & 7) - 7);
        Assert(Cause >= -7 && Cause <= 0, Cause, " WeaponHitChunk.unpack: bad cause ");
        HitLocation = static_cast<int8_t>(((packed >> 20) & 0xf) - 2);
        Assert(HitLocation >= -1 && HitLocation <= 11, HitLocation, " WeaponHitChunk.unpack: bad hitLocation ");
        EntryAngle = static_cast<int8_t>(high & 3);
        Refit = static_cast<int32_t>((packed >> 26) & 1);
        return;
    }

    if (TargetType == 1)
    {
        const int8_t item = static_cast<int8_t>(rest & 7);
        TargetItemNumber = item;
        const uint32_t vertex = (packed >> 15) & 0x1ff;
        TargetBlockOrTrainNumber = static_cast<int32_t>(packed >> 24);
        TargetVertexOrCarNumber = static_cast<int32_t>(vertex);
        TargetId = static_cast<int32_t>(item + 0x1000 + ((packed >> 24) * 400 + vertex) * 8);
        return;
    }

    if (TargetType == 2)
    {
        const uint32_t trainNumber = (packed >> 20) & 0xff;
        TargetVertexOrCarNumber = static_cast<int32_t>(rest & 0xff);
        TargetBlockOrTrainNumber = static_cast<int32_t>(trainNumber);
        const uint8_t angleBits = high >> 4;

        if (trainNumber != 0x80)
        {
            EntryAngle = static_cast<int8_t>(angleBits & 3);
            TargetId = TargetVertexOrCarNumber + static_cast<int32_t>((trainNumber * 5 + 0x6400) * 0x14);
            return;
        }

        TargetId = static_cast<int32_t>((rest & 0xff) + 0x802c8);
        EntryAngle = static_cast<int8_t>(angleBits & 3);
        return;
    }

    DebugWeaponHitChunk(this, nullptr);
    Fatal(0, " Bad WeaponHitChunk Target Type ");
}

auto MCWeaponHitChunk::EqualTo(MCWeaponHitChunk* chunk) -> int
{
    if (TargetType != chunk->TargetType || TargetId != chunk->TargetId || Cause != chunk->Cause ||
        Damage != chunk->Damage || EntryAngle != chunk->EntryAngle || Refit != chunk->Refit ||
        HitLocation != chunk->HitLocation)
    {
        DebugWeaponHitChunk(this, chunk);
        return 0;
    }

    return 1;
}

//---------------------------------------------------------------------------
// Debug chunk routines
//---------------------------------------------------------------------------

auto DebugWeaponFireChunk(MCWeaponFireChunk* chunk1, MCWeaponFireChunk* chunk2, MCGameObject* attacker) -> void
{
    char line[512];
    ChunkDebugMsg[0] = '\0';

    if (attacker == nullptr)
    {
        std::strcat(ChunkDebugMsg, "attacker = ???\n");
    }
    else
    {
        if (IsMover(attacker))
        {
            std::snprintf(line, sizeof(line), "attacker = %s (%d)\n",
                          static_cast<MCMover*>(attacker)->DebugStatus.c_str(), attacker->PartId);
        }
        else
        {
            std::snprintf(line, sizeof(line), "attacker = objClass %d (%d)\n", static_cast<int>(attacker->ObjectClass),
                          attacker->PartId);
        }

        std::strcat(ChunkDebugMsg, line);
    }

    if (chunk1 != nullptr)
    {
        std::strcat(ChunkDebugMsg, "\nCHUNK1\n");
        AppendWeaponFireChunk(chunk1, chunk1);
    }

    if (chunk2 != nullptr)
    {
        std::strcat(ChunkDebugMsg, "\nCHUNK2\n");
        // Port fix: the original reads chunk1's fields here even when chunk1 is null (no caller passes that).
        AppendWeaponFireChunk(chunk2, chunk1 != nullptr ? chunk1 : chunk2);
    }

    SaveChunkDebugMsg("wfchunk.dbg");
}

auto OpenWeaponFireLog() -> void
{
}

auto LogWeaponFireChunk(MCWeaponFireChunk*, MCGameObject*, MCGameObject*) -> void
{
}

auto DebugWeaponHitChunk(MCWeaponHitChunk* chunk1, MCWeaponHitChunk* chunk2) -> void
{
    ChunkDebugMsg[0] = '\0';

    if (chunk1 != nullptr)
    {
        std::strcat(ChunkDebugMsg, "\nCHUNK1\n");
        AppendWeaponHitChunk(chunk1);
    }

    if (chunk2 != nullptr)
    {
        std::strcat(ChunkDebugMsg, "\nCHUNK2\n");
        AppendWeaponHitChunk(chunk2);
    }

    SaveChunkDebugMsg("whchunk.dbg");
}

//---------------------------------------------------------------------------
// GameObject
//---------------------------------------------------------------------------

MCGameObject::MCGameObject()
{
    ObjectClass = GAMEOBJECT;
    IdNumber = 0;
    Next = nullptr;
    PartId = -1;
    ObjType = nullptr;
    Position.Z = 0.0f;
    Position.Y = 0.0f;
    Position.X = 0.0f;
    Selected = 0;
    CollisionsOn = 0;
    Alignment = 0;
    Status = 0;
}

auto MCGameObject::Init(MCObjectType* type) -> int32_t
{
    ObjectClass = GAMEOBJECT;
    ObjType = type;
    Alignment = type->TeamId;
    return 0;
}

auto MCGameObject::Init() -> void
{
    ObjectClass = GAMEOBJECT;
    IdNumber = 0;
    Next = nullptr;
    PartId = -1;
    ObjType = nullptr;
    Position.Z = 0.0f;
    Position.Y = 0.0f;
    Position.X = 0.0f;
    Selected = 0;
    CollisionsOn = 0;
    Alignment = 0;
    Status = 0;
}

auto MCGameObject::Destroy() -> void
{
    ObjectTypeManager->Remove(ObjType);
}

auto MCGameObject::GetPositionFromHS(uint32_t) -> MCVector3D
{
    return Position;
}

auto MCGameObject::GetBlockAndVertexNumber(int32_t& blockNumber, int32_t& vertexNumber) -> void
{
    Assert(MCTerrain::MetersPerVertex == 128.0f, 0, " Optimizations now broken ");
    // The original keeps each floored coordinate and block index as a 16-bit value.
    const int32_t vertexCol =
        (static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(Position.X)))) >> 7) +
        MCTerrain::VerticesMapSide;
    const int32_t blockCol = static_cast<int16_t>(
        static_cast<int32_t>(std::floor(static_cast<double>(vertexCol) * MCTerrain::OneOververticesBlockSide)));
    const int32_t vertexRow =
        (MCTerrain::VerticesMapSide -
         (static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(Position.Y)))) >> 7)) -
        1;
    const int32_t blockRow = static_cast<int16_t>(
        static_cast<int32_t>(std::floor(static_cast<double>(vertexRow) * MCTerrain::OneOververticesBlockSide)));
    blockNumber = MCTerrain::BlocksMapSide * blockRow + blockCol;
    vertexNumber =
        ((vertexRow - MCTerrain::VerticesBlockSide * blockRow) - blockCol) * MCTerrain::VerticesBlockSide + vertexCol;
}

auto MCGameObject::GetPosition() -> MCVector3D
{
    return Position;
}

auto MCGameObject::RelativePosition(float angle, float distance, uint32_t flags) -> MCVector3D
{
    // The point distance meters away at the absolute angle (radians), pulled back along the line to the first
    // cell whose passability changes. The x87 keeps the reach at extended precision, done here in double.
    const double reach = -(static_cast<double>(WorldUnitsPerMeter) * distance);
    const float x = Position.X;
    const float y = Position.Y;
    const float offsetX = static_cast<float>((std::sin(static_cast<double>(angle)) + 0.0) * reach);
    const float offsetY = static_cast<float>(static_cast<float>(std::cos(static_cast<double>(angle))) * reach);
    const float targetX = offsetX + x;
    const float targetY = offsetY + y;

    // Flag 2 walks from the object out to the point; otherwise from the point back to the object.
    MCVector2D start;
    MCVector2D end;

    if ((flags & 2) != 0)
    {
        start.X = x;
        start.Y = y;
        end.X = targetX;
        end.Y = targetY;
    }
    else
    {
        start.X = targetX;
        start.Y = targetY;
        end.X = x;
        end.Y = y;
    }

    // Half a map cell per step.
    const float deltaX = end.X - start.X;
    const float deltaY = end.Y - start.Y;
    const float length =
        static_cast<float>(std::sqrt(static_cast<double>(deltaY) * deltaY + static_cast<double>(deltaX) * deltaX));
    // The x87 keeps the x direction unrounded.
    double directionX = deltaX;
    float directionY = deltaY;

    if (length != 0.0)
    {
        directionX = static_cast<double>(deltaX) / length;
        directionY = deltaY / length;
    }

    const float stepLength = static_cast<float>(static_cast<double>(MCTerrain::MetersPerVertex) * 0.33333334f * 0.5);
    const float stepX = static_cast<float>(directionX * stepLength);
    const float stepY = directionY * stepLength;

    if (std::sqrt(static_cast<double>(stepX) * stepX + static_cast<double>(stepY) * stepY) == 0.0)
    {
        MCVector3D result;
        result.X = x;
        result.Y = y;
        result.Z = 0.0f;
        return result;
    }

    const MCVector2D span = start - end;
    const float maxDistance =
        static_cast<float>(std::sqrt(static_cast<double>(span.Y) * span.Y + static_cast<double>(span.X) * span.X));
    float traveled = 0.0f;
    MCVector2D current = start;

    // Whether the cell under current is passable.
    auto cellPassable = [&]()
    {
        MCVector3D point;
        point.X = current.X;
        point.Y = current.Y;
        point.Z = 0.0f;
        int32_t tileR;
        int32_t tileC;
        int32_t cellR;
        int32_t cellC;
        GameMap->WorldToMapPos(point, tileR, tileC, cellR, cellC);

        // Port fix: the walk can leave the map, where the original reads outside it. Off the map is impassable.
        if (!GameMap->OnMap(tileR, tileC))
        {
            return 0u;
        }

        return GameMap->Map[GameMap->Width * tileR + tileC].GetCellPassable(cellR, cellC);
    };

    uint32_t passable = cellPassable();
    MCVector2D previous = start;
    // Walk until the cell changes kind (or the distance runs out); the answer is the step before.
    const uint32_t keepGoingWhile = (flags & 2) != 0 ? 1u : 0u;

    if ((passable != 0) == (keepGoingWhile != 0))
    {
        while (traveled < maxDistance)
        {
            previous = current;
            current.X = stepX + current.X;
            current.Y = stepY + current.Y;
            const double dx = static_cast<double>(current.X) - start.X;
            const double dy = static_cast<double>(current.Y) - start.Y;
            traveled = static_cast<float>(std::sqrt(dx * dx + dy * dy));
            passable = cellPassable();

            if ((passable != 0) != (keepGoingWhile != 0))
            {
                break;
            }
        }
    }

    MCVector3D ground;
    ground.X = previous.X;
    ground.Y = previous.Y;
    ground.Z = 0.0f;
    MCVector3D result;
    result.X = previous.X;
    result.Y = previous.Y;
    result.Z = GameMap->GetTerrainElevation(ground);
    return result;
}

auto MCGameObject::SetPosition(MCVector3D& newPosition) -> void
{
    Position = newPosition;
}

auto MCGameObject::GetVelocity() -> MCVector3D
{
    MCVector3D velocity;
    velocity.X = 0.0f;
    velocity.Y = 0.0f;
    velocity.Z = 0.0f;
    return velocity;
}

auto MCGameObject::GetScreenPos(int32_t) -> MCVector2D
{
    MCVector2D screen;
    screen.X = 0.0f;
    screen.Y = 0.0f;
    return screen;
}

auto MCGameObject::GetFrame() -> MCFrameOfRef
{
    return MCFrameOfRef(UnitX, UnitY, UnitZ);
}

auto MCGameObject::DistanceFrom(MCVector3D& goal) -> double
{
    const double dx = static_cast<double>(Position.X) - goal.X;
    const double dy = static_cast<double>(Position.Y) - goal.Y;
    return std::sqrt(dy * dy + dx * dx) * MetersPerWorldUnit;
}

auto MCGameObject::LineOfSight(MCVector3D point) -> int
{
    const MCVector3D start = Position;
    SetUseMe(0);
    const int result = GameMap->LineOfSight(start, point);
    SetUseMe(1);
    return result;
}

auto MCGameObject::LineOfSight(MCGameObject* target) -> int
{
    // From eye to eye, ten meters up.
    MCVector3D start;
    start.X = Position.X;
    start.Y = Position.Y;
    start.Z = static_cast<float>(static_cast<double>(WorldUnitsPerMeter) * 10.0 + Position.Z);
    const MCVector3D targetPosition = target->GetPosition();
    MCVector3D end;
    end.X = targetPosition.X;
    end.Y = targetPosition.Y;
    end.Z = static_cast<float>(static_cast<double>(WorldUnitsPerMeter) * 10.0 + targetPosition.Z);
    SetUseMe(0);
    target->SetUseMe(0);
    const int result = GameMap->LineOfSight(start, end);
    SetUseMe(1);
    target->SetUseMe(1);
    return result;
}

auto MCGameObject::LineOfFire(MCGameObject* target) -> int
{
    MCVector3D start;
    start.X = Position.X;
    start.Y = Position.Y;
    start.Z = static_cast<float>(static_cast<double>(WorldUnitsPerMeter) * 10.0 + Position.Z);
    const MCVector3D targetPosition = target->GetPosition();
    MCVector3D end;
    end.X = targetPosition.X;
    end.Y = targetPosition.Y;
    end.Z = static_cast<float>(static_cast<double>(WorldUnitsPerMeter) * 10.0 + targetPosition.Z);
    SetUseMe(0);
    target->SetUseMe(0);
    const int result = GameMap->LineOfFire(start, end);
    SetUseMe(1);
    target->SetUseMe(1);
    return result;
}

auto MCGameObject::RelFacingTo(MCVector3D goal, int32_t) -> float
{
    // The facing is the world frame's -j, turned an eighth.
    const float x = Position.X;
    const float y = Position.Y;
    const MCFrameOfRef turned = TurnedFrame(MCFrameOfRef(UnitX, UnitY, UnitZ));
    MCVector3D facing;
    facing.X = -turned.J.X;
    facing.Y = -turned.J.Y;
    facing.Z = -turned.J.Z;

    MCVector3D toGoal;
    toGoal.X = goal.X - x;
    toGoal.Y = goal.Y - y;
    toGoal.Z = 0.0f;
    const double length =
        std::sqrt((static_cast<double>(toGoal.X) * toGoal.X + static_cast<double>(toGoal.Y) * toGoal.Y) +
                  static_cast<double>(toGoal.Z) * toGoal.Z);

    if (length != 0.0)
    {
        toGoal.X = static_cast<float>(toGoal.X / length);
        toGoal.Y = static_cast<float>(toGoal.Y / length);
        toGoal.Z = static_cast<float>(toGoal.Z / length);
    }

    const double cosine = static_cast<double>(toGoal.Z) * facing.Z + static_cast<double>(toGoal.Y) * facing.Y +
                          static_cast<double>(toGoal.X) * facing.X;
    const float angle = static_cast<float>(AcosMatherr(cosine) * RADIANS_TO_DEGREES_F);

    // Negative to the left.
    if ((facing & toGoal).Z >= 0.0f)
    {
        return -angle;
    }

    return angle;
}

auto MCGameObject::RelViewFacingTo(MCVector3D goal) -> float
{
    return MCGameObject::RelFacingTo(goal, -1);
}

auto MCGameObject::GetExtentRadius() -> float
{
    return ObjType->ExtentRadius;
}

auto MCGameObject::SetExtentRadius(float newRadius) -> void
{
    ObjType->ExtentRadius = newRadius;
}

auto MCGameObject::GetCaptureBlocker(int32_t side) -> MCGameObject*
{
    // Clan movers block only when they aren't marines.
    const bool clan = side == 1;
    MCObjectQueueNode* list = clan ? ClanMechList : InnerSphereMechList;

    for (MCBaseObject* object = list->Head; object != nullptr; object = object->Next)
    {
        if (!IsMover(object))
        {
            continue;
        }

        auto* mover = static_cast<MCMover*>(object);

        if (clan && mover->IsMarine() != 0)
        {
            continue;
        }

        if (mover->NumWeapons == 0)
        {
            continue;
        }

        MCVector3D moverPosition = mover->GetPosition();

        if (DistanceFrom(moverPosition) < BlockCaptureRange && mover->IsDestroyed() == 0 && mover->IsDisabled() == 0 &&
            mover->GetAwake() != 0)
        {
            return mover;
        }
    }

    return nullptr;
}

auto MCGameObject::ClearLineOfFire() -> void
{
    // Sets the line-of-sight bit (15 + 2c) of each of the tile's nine cells, saving the old bits.
    MCMapTile& tile = TileAt(Position);

    for (int32_t cell = 0; cell < 9; cell++)
    {
        const uint32_t shift = static_cast<uint32_t>(cell * 2 + 15);
        const uint32_t mask = 1u << shift;
        ObjCellArray[cell] = static_cast<int32_t>((tile.Cells & mask) >> shift);
        tile.Cells = (~mask & tile.Cells) | mask;
    }
}

auto MCGameObject::RestoreLineOfFire() -> void
{
    MCMapTile& tile = TileAt(Position);

    for (int32_t cell = 0; cell < 9; cell++)
    {
        const uint32_t shift = static_cast<uint32_t>(cell * 2 + 15);
        tile.Cells = static_cast<uint32_t>(ObjCellArray[cell]) << shift | (~(1u << shift) & tile.Cells);
    }
}

//---------------------------------------------------------------------------
// BigGameObject
//---------------------------------------------------------------------------

auto MCBigGameObject::Init(MCObjectType* type) -> int32_t
{
    ObjectClass = BIGGAMEOBJECT;
    ObjType = type;
    return 0;
}

auto MCBigGameObject::Init() -> void
{
    ObjectClass = BIGGAMEOBJECT;
    IdNumber = 0;
    Next = nullptr;
    PartId = -1;
    ObjType = nullptr;
    Tonnage = 0.0f;
    BlipTime = 0.0f;
    Position.Z = 0.0f;
    Position.Y = 0.0f;
    Position.X = 0.0f;
    ObjPosition = nullptr;
    Team = nullptr;
    PotentialContact = nullptr;
    CollisionFreeFrom = nullptr;
    CollisionFreeTime = 0.0f;
    Status = 0;
    Damage = 0.0f;
    Flags = 5;
    Alignment = 0;
    Selected = 0;
    ScreenPos.X = 0.0f;
    ScreenPos.Y = 0.0f;
    WindowsVisible = 0;
    MaxCV = 0;
    CurCV = 0;
    CollisionsOn = 0;
    ExplDamage = 0.0f;
    ExplRadius = 0.0f;
    Salvage = nullptr;
    BlipFrame = 0;
    NumAttackers = 0;
}

auto MCBigGameObject::Destroy() -> void
{
    while (Salvage != nullptr)
    {
        MCSalvageItem* nextItem = Salvage->Next;
        delete Salvage;
        Salvage = nextItem;
    }

    if (PotentialContact != nullptr)
    {
        PotentialContactManager->Remove(PotentialContact);
    }

    if (ObjPosition != nullptr)
    {
        GameObjectMap->RemoveObject(this);
    }
}

auto MCBigGameObject::Kill() -> int32_t
{
    // Port fix: with no type the original used uninitialised ids.
    int32_t explosionId = -1;
    int32_t destroyedId = -1;

    if (GetObjectType() != nullptr)
    {
        explosionId = GetObjectType()->ExplosionObject;
        destroyedId = GetObjectType()->DestroyedObject;
    }

    if (explosionId != -1)
    {
        MCGameObject* explosion = CreateObject(explosionId);
        MCVector3D here = GetPosition();
        explosion->SetPosition(here);
        MCVector3D velocity = GetVelocity();
        explosion->SetVelocity(velocity);

        if (ObjectList->Head != nullptr && explosion != nullptr)
        {
            ObjectList->Head->AddNode(explosion);
        }
    }

    if (destroyedId != -1)
    {
        MCGameObject* wreck = CreateObject(destroyedId);
        MCVector3D here = GetPosition();
        wreck->SetPosition(here);
        MCVector3D velocity = GetVelocity();
        wreck->SetVelocity(velocity);
        MCFrameOfRef frame = GetFrame();
        wreck->SetFrame(frame);

        if (ObjectList->Head != nullptr && wreck != nullptr)
        {
            ObjectList->Head->AddNode(wreck);
        }
    }

    return static_cast<int32_t>(0xbeaddead);
}

auto MCBigGameObject::OnScreen() -> int
{
    MCCamera* camera = CameraList->FindCameraFromIDNumber(1);

    if (camera == nullptr || camera->Active == 0)
    {
        return 0;
    }

    // At camera scale 1 everything is drawn at half size.
    const float scale = camera->CameraScale != 1 ? 1.0f : 0.5f;
    const float dx = (Position.X - camera->Position.X) * scale;
    const float dy = (Position.Y - camera->Position.Y) * scale;
    const float dz = scale * Position.Z - camera->Position.Z;
    const float screenX = static_cast<float>(static_cast<double>(dy) * camera->CosAngle +
                                             static_cast<double>(dx) * camera->CosAngle + camera->HalfWidth);
    ScreenPos.X = screenX;
    const float screenY = static_cast<float>(((static_cast<double>(dx) * camera->SinAngle + camera->HalfHeight) -
                                              static_cast<double>(dy) * camera->SinAngle) -
                                             static_cast<double>(scale) * dz);
    ScreenPos.Y = screenY;

    if (screenX >= 0.0f && screenY >= 0.0f && screenX <= camera->ViewWidth && screenY <= camera->ViewHeight)
    {
        WindowsVisible = Turn;
        return 1;
    }

    return 0;
}

auto MCBigGameObject::SetPotentialContact(int32_t contactType) -> void
{
    // The contact list by alignment: 1 for -1, 0 for 1, else 2.
    int32_t listType;

    if (Alignment == -1)
    {
        listType = 1;
    }
    else
    {
        listType = Alignment != 1 ? 2 : 0;
    }

    if (contactType == 0)
    {
        PotentialContactManager->Remove(PotentialContact);
        PotentialContact = nullptr;
        return;
    }

    if (PotentialContact == nullptr)
    {
        PotentialContact = PotentialContactManager->Add(listType, this, static_cast<char>(contactType));
        return;
    }

    PotentialContactManager->Move(PotentialContact, listType, static_cast<char>(contactType));
}

auto MCBigGameObject::UpdateContactStatus(MCTeam* contactTeam) -> void
{
    if (PotentialContact != nullptr)
    {
        PotentialContact->UpdateStatus(contactTeam);
    }
}

auto MCBigGameObject::GetContactCount(int32_t teamId) -> int32_t
{
    Assert(teamId > -1, -1, " Bad Team Id ");

    if (PotentialContact != nullptr)
    {
        return static_cast<uint8_t>(PotentialContact->NumSensors[teamId]);
    }

    return 0;
}

// The "tagged" flag is the contact's lostVisual byte (+0x0e).

auto MCBigGameObject::SetContactTagged(int32_t teamId, int tagged) -> void
{
    Assert(teamId > -1, -1, " Bad Team Id ");
    Assert(PotentialContact != nullptr, -1, " Is Not Potential Contact ");

    if (PotentialContact != nullptr)
    {
        PotentialContact->LostVisual[teamId] = static_cast<uint8_t>(tagged);
    }
}

auto MCBigGameObject::GetContactTagged(int32_t teamId) -> int
{
    Assert(teamId > -1, -1, " Bad Team Id ");

    if (PotentialContact != nullptr)
    {
        return PotentialContact->LostVisual[teamId];
    }

    return 0;
}

auto MCBigGameObject::GetContactType(int32_t teamId, int& tagged) -> int32_t
{
    Assert(teamId > -1, -1, " Bad Team Id ");

    if (PotentialContact != nullptr)
    {
        tagged = PotentialContact->LostVisual[teamId];
        return PotentialContact->ContactStatus[teamId];
    }

    return 0;
}

auto MCBigGameObject::GetContactType(int32_t teamId) -> int32_t
{
    Assert(teamId > -1, -1, " Bad Team Id ");

    if (PotentialContact != nullptr)
    {
        return PotentialContact->ContactStatus[teamId];
    }

    return 0;
}

auto MCBigGameObject::GetScreenPos(int32_t) -> MCVector2D
{
    return ScreenPos;
}

auto MCBigGameObject::SetAlignment(int32_t newAlignment) -> void
{
    Alignment = newAlignment;

    if (PotentialContact != nullptr)
    {
        SetPotentialContact(PotentialContact->Visibility);
    }
}

auto MCBigGameObject::Write(MCFile* objFile) -> int32_t
{
    objFile->Write(reinterpret_cast<const uint8_t*>(&Tonnage), 4);
    objFile->WriteLong(Status);
    objFile->WriteLong(static_cast<int32_t>(Damage));
    objFile->WriteByte(IsCaptured() != 0 ? 1 : 0);
    objFile->Write(reinterpret_cast<const uint8_t*>(&ExplRadius), 4);
    objFile->Write(reinterpret_cast<const uint8_t*>(&ExplDamage), 4);
    return 0;
}

auto MCBigGameObject::GetMechClass() -> MCMechClass
{
    if (ObjectClass != BATTLEMECH)
    {
        return MECH_CLASS_NONE;
    }

    if (Tonnage < 35.0f)
    {
        return MECH_CLASS_LIGHT;
    }

    if (Tonnage < 55.0f)
    {
        return MECH_CLASS_MEDIUM;
    }

    if (Tonnage >= 75.0f)
    {
        return MECH_CLASS_ASSAULT;
    }

    return MECH_CLASS_HEAVY;
}

auto MCBigGameObject::DecrementAttackers() -> void
{
    Assert(NumAttackers > 0, 0, nullptr);
    NumAttackers--;
}

auto MCBigGameObject::GetVitalInfo(void* vitalInfo) -> int32_t
{
    // The original only asks isCaptured and fills nothing.
    if (vitalInfo != nullptr)
    {
        IsCaptured();
    }

    return 0x19;
}
