#include "stdafx.h"
#include "object/gameobj.h"
#include "ai/move.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "lib/aerror.h"
#include "lib/file.h"
#include "lib/heap.h"
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

int32_t objCellArray[9] = {};
char ChunkDebugMsg[0x1400] = {}; // 0x1408 bytes lie before the next global (MCX.EXE @ 0x007dd100).
float BlockCaptureRange = 0.0f;

namespace
{
    /// <summary>The eighth turn the facing math rotates the frame by (0x3ffec90fdaa21f446000).</summary>
    constexpr double EIGHTH_TURN = 0x1.921fb5443e88cp-1;
    /// <summary>Degrees per radian, at float precision (0x4004e52ee10000000000).</summary>
    constexpr double RADIANS_TO_DEGREES_F = 0x1.ca5dc2p+5;

    /// <summary>Whether <paramref name="object"/> is a mover (mech, vehicle, elemental or plain mover).</summary>
    bool IsMover(const BaseObject* object)
    {
        const ObjectClass objectClass = object->objectClass;
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
    /// camera drone (part id), or map point, or "???" with the terrain objects of the target's vertex listed.
    /// </summary>
    /// <param name="itemNumber">The item number used for the vertex listing (see OB-010).</param>
    /// <param name="cellC">The cell column used for a map point (see OB-010).</param>
    void AppendTargetLine(int8_t targetType, int32_t targetId, uint16_t cellR, uint16_t cellC, int8_t itemNumber,
                          bool hitChunk)
    {
        char line[512];
        BaseObject* target = nullptr;
        bool haveTarget = false;

        if (targetType == 0)
        {
            target = MPlayer->moverRoster[targetId];
            haveTarget = true;
        }
        else if (targetType == 1 || targetType == 2)
        {
            target = objectList->findObjectFromPart(targetId);
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
            const float halfSide = worldUnitsMapSide * 0.5f;
            vector_3d point;
            point.x = static_cast<float>((cellC + 0.5f) * static_cast<double>(MetersPerCell) - halfSide);
            point.y = static_cast<float>((static_cast<double>(halfSide) - cellR * static_cast<double>(MetersPerCell)) -
                                         static_cast<double>(MetersPerCell) * 0.5f);
            point.z = 0.0f;
            const float elevation = GameMap->getTerrainElevation(point);
            std::snprintf(line, sizeof(line), "target point = (%f, %f, %f)\n", static_cast<double>(point.x),
                          static_cast<double>(point.y), static_cast<double>(elevation));
            std::strcat(ChunkDebugMsg, line);
            return;
        }

        if (haveTarget && target != nullptr)
        {
            if (IsMover(target))
            {
                std::snprintf(line, sizeof(line), "target = %s (%d)\n", static_cast<Mover*>(target)->debugStatus,
                              target->partId);
            }
            else
            {
                std::snprintf(line, sizeof(line), "target = objClass %d (%d)\n", static_cast<int>(target->objectClass),
                              target->partId);
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
            BaseObject* object = objectList->findObjectFromPart(firstId + i);

            if (object == nullptr)
            {
                continue;
            }

            numObjects++;
            std::snprintf(line, sizeof(line), "    %d: objClass %d (%d)\n", i, static_cast<int>(object->objectClass),
                          object->partId);
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
    void AppendWeaponFireChunk(const WeaponFireChunk* chunk, const WeaponFireChunk* first)
    {
        // Original behaviour (OB-010): the second chunk's target point and terrain listing use the first chunk's
        // cell column and item number.
        const uint16_t cellC = first->targetCell[1];
        AppendTargetLine(chunk->targetType, chunk->targetId, chunk->targetCell[0], cellC, first->targetItemNumber,
                         false);

        char line[512];
        std::snprintf(line, sizeof(line), "targetType = %d\n", static_cast<int>(chunk->targetType));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetId = %d\n", chunk->targetId);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetBlockOrTrainNumber = %d\n", chunk->targetBlockOrTrainNumber);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetVertexOrCarNumber = %d\n", chunk->targetVertexOrCarNumber);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetItemNumber = %d\n", static_cast<int>(chunk->targetItemNumber));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetCellRC = (%d, %d)\n", static_cast<int>(chunk->targetCell[0]),
                      static_cast<int>(cellC));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "weaponIndex = %d\n", static_cast<int>(chunk->weaponIndex));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "hit = %c\n", chunk->hit != 0 ? 'T' : 'N');
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "entryAngle = %d\n", static_cast<int>(chunk->entryAngle));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "numMissiles = %d\n", static_cast<int>(chunk->numMissiles));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "numMissilesHit = %d\n", static_cast<int>(chunk->numMissilesPastAMS));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "numAntiMissiles = %d\n", static_cast<int>(chunk->numAntiMissileShots));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "hitLocation = %d\n", static_cast<int>(chunk->hitLocation));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "data = %x\n", chunk->data);
        std::strcat(ChunkDebugMsg, line);
    }

    /// <summary>Appends one weapon hit chunk's fields.</summary>
    void AppendWeaponHitChunk(const WeaponHitChunk* chunk)
    {
        AppendTargetLine(chunk->targetType, chunk->targetId, 0, 0, 0, true);

        char line[512];
        std::snprintf(line, sizeof(line), "targetType = %d\n", static_cast<int>(chunk->targetType));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetId = %d\n", chunk->targetId);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetBlockOrTrainNumber = %d\n", chunk->targetBlockOrTrainNumber);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetVertexOrCarNumber = %d\n", chunk->targetVertexOrCarNumber);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetItemNumber = %d\n", static_cast<int>(chunk->targetItemNumber));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "cause = %d\n", static_cast<int>(chunk->cause));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "damage = %f\n", static_cast<double>(chunk->damage));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "hitLocation = %d\n", static_cast<int>(chunk->hitLocation));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "entryAngle = %d\n", static_cast<int>(chunk->entryAngle));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "refit = %s\n", chunk->refit != 0 ? "TRUE" : "FALSE");
        std::strcat(ChunkDebugMsg, line);
    }

    /// <summary>Writes ChunkDebugMsg to <paramref name="fileName"/> and hands it to the crash handler.</summary>
    void SaveChunkDebugMsg(const char* fileName)
    {
        auto* file = new File;
        file->create(fileName);
        file->writeString(ChunkDebugMsg);
        file->close();
        delete file;
        ExceptionGameMsg = ChunkDebugMsg;
    }

    /// <summary>The frame's i and j axes turned by an eighth turn, as the facing math uses them.</summary>
    frame_of_ref TurnedFrame(const frame_of_ref& frame)
    {
        const float s = static_cast<float>(std::sin(EIGHTH_TURN));
        const float c = static_cast<float>(std::cos(EIGHTH_TURN));
        frame_of_ref turned = frame;
        turned.i = frame.i * c + frame.j * s;
        turned.j = frame.j * c - frame.i * s;
        return turned;
    }

    /// <summary>The tile under <paramref name="position"/>.</summary>
    MapTile& TileAt(const vector_3d& position)
    {
        int32_t tileR;
        int32_t tileC;
        int32_t cellR;
        int32_t cellC;
        GameMap->worldToMapPos(position, tileR, tileC, cellR, cellC);
        return GameMap->map[GameMap->width * tileR + tileC];
    }
}

//---------------------------------------------------------------------------
// _WeaponShotInfo
//---------------------------------------------------------------------------

auto _WeaponShotInfo::init(GameObject* shooter, int32_t weaponMasterId, float shotDamage, int32_t shotHitLocation,
                           float shotEntryAngle) -> void
{
    attacker = shooter;

    if (MPlayer == nullptr && shooter != nullptr)
    {
        // The difficulty scales the player's shots, and the enemy's mechs, vehicles, elementals and turrets.
        const ObjectClass shooterClass = shooter->objectClass;
        const int player = shooter->getAlignment() == homeTeam->alignment ? 1 : 0;

        if (player != 0 || shooterClass == BATTLEMECH || shooterClass == GROUNDVEHICLE || shooterClass == ELEMENTAL ||
            shooterClass == TURRET)
        {
            shotDamage = applyDifficultyWeapon(shotDamage, player);
        }
    }

    damage = shotDamage;
    masterId = weaponMasterId;
    hitLocation = shotHitLocation;
    entryAngle = shotEntryAngle;
    Assert(shotDamage >= 0.0 && shotDamage <= 255.0, static_cast<int32_t>(shotDamage),
           " WeaponShotInfo.init: damage out of range ");

    if (MPlayer != nullptr && MPlayer->isServer != 0)
    {
        damage = QuarterPoints(shotDamage);
        entryAngle = SnapAngle(shotEntryAngle);
    }
}

auto _WeaponShotInfo::setDamage(float shotDamage) -> void
{
    damage = shotDamage;

    if (MPlayer != nullptr && MPlayer->isServer != 0)
    {
        damage = QuarterPoints(shotDamage);
    }
}

auto _WeaponShotInfo::setEntryAngle(float shotEntryAngle) -> void
{
    entryAngle = shotEntryAngle;

    if (MPlayer != nullptr && MPlayer->isServer != 0)
    {
        entryAngle = SnapAngle(shotEntryAngle);
    }
}

//---------------------------------------------------------------------------
// WeaponFireChunk
//---------------------------------------------------------------------------

auto WeaponFireChunk::operator new(size_t size) noexcept -> void*
{
    if (systemHeap != nullptr)
    {
        return systemHeap->malloc(static_cast<uint32_t>(size));
    }

    return std::malloc(size);
}

auto WeaponFireChunk::operator delete(void* ptr) -> void
{
    if (systemHeap != nullptr)
    {
        systemHeap->free(ptr);
        return;
    }

    std::free(ptr);
}

auto WeaponFireChunk::init() -> void
{
    hitLocation = -1;
    targetType = 0;
    targetId = 0;
    targetBlockOrTrainNumber = 0;
    targetVertexOrCarNumber = 0;
    targetItemNumber = 0;
    targetCell[0] = 0;
    targetCell[1] = 0;
    weaponIndex = 0;
    hit = 0;
    entryAngle = 0;
    numMissiles = 0;
    numMissilesPastAMS = 0;
    numAntiMissileShots = 0;
    data = 0;
}

auto WeaponFireChunk::buildMoverTarget(BigGameObject* target, int32_t weapon, int hitTarget, float angle,
                                       int32_t missiles, int32_t missilesPastAMS, int32_t antiMissileShots,
                                       int32_t location) -> void
{
    const int32_t rosterIndex = static_cast<Mover*>(target)->netRosterIndex;
    hit = hitTarget;
    targetType = 0;
    targetId = rosterIndex;
    weaponIndex = static_cast<uint8_t>(weapon);
    entryAngle = AngleQuadrant(angle);
    numMissilesPastAMS = static_cast<int8_t>(missilesPastAMS);
    numMissiles = static_cast<int8_t>(missiles);
    numAntiMissileShots = static_cast<int8_t>(antiMissileShots);
    hitLocation = static_cast<int8_t>(location);
    Assert(rosterIndex >= 0 && rosterIndex < MPlayer->numMovers, rosterIndex,
           " WeaponFireChunk.buildMoverTarget: bad targetId ");
    Assert(weaponIndex < 0x20, weaponIndex, " WeaponFireChunk.buildMoverTarget: bad weaponIndex ");
    Assert(numMissiles >= 0 && numMissiles <= 15, numMissiles, " WeaponFireChunk.buildMoverTarget: bad numMissiles ");
    Assert(numMissilesPastAMS >= 0 && numMissilesPastAMS <= 15, numMissilesPastAMS,
           " WeaponFireChunk.buildMoverTarget: bad numMissilesHit ");
    Assert(numAntiMissileShots >= 0 && numAntiMissileShots <= 15, numAntiMissileShots,
           " WeaponFireChunk.buildMoverTarget: bad numAntiMissiles ");
    Assert(hitLocation >= -1 && hitLocation <= 11, hitLocation, " WeaponFireChunk.buildMoverTarget: bad hitLocation ");
    data = 0;
}

auto WeaponFireChunk::buildTerrainTarget(BigGameObject* target, int32_t weapon, int hitTarget, int32_t missiles) -> void
{
    // A terrain object's part id is 0x1000 + (block * 400 + vertex) * 8 + item.
    const int32_t partId = target->partId;
    targetType = 1;
    targetId = partId;
    targetBlockOrTrainNumber = (partId - 0x1000) / 0xc80;
    const int32_t rest = (partId - 0x1000) % 0xc80;
    targetVertexOrCarNumber = rest / 8;
    weaponIndex = static_cast<uint8_t>(weapon);
    numMissiles = static_cast<int8_t>(missiles);
    numMissilesPastAMS = static_cast<int8_t>(missiles);
    targetItemNumber = static_cast<int8_t>(rest - targetVertexOrCarNumber * 8);
    hit = hitTarget;
    Assert(partId != -1, target->objectClass, " WeaponFireChunk.buildTerrainTarget: -1 partId ");
    data = 0;
}

auto WeaponFireChunk::buildTrainTarget(BigGameObject* target, int32_t weapon, int hitTarget, float angle,
                                       int32_t missiles) -> void
{
    // A train car's part id is 0x7d000 + train * 100 + car.
    targetType = 2;
    targetId = target->partId;
    targetBlockOrTrainNumber = (target->partId - 0x7d000) / 100;
    weaponIndex = static_cast<uint8_t>(weapon);
    hit = hitTarget;
    targetVertexOrCarNumber = (target->partId - 0x7d000) % 100;
    entryAngle = AngleQuadrant(angle);
    numMissiles = static_cast<int8_t>(missiles);
    numMissilesPastAMS = static_cast<int8_t>(missiles);
    data = 0;
}

auto WeaponFireChunk::buildCameraDroneTarget(BigGameObject* target, int32_t weapon, int hitTarget, float angle,
                                             int32_t missiles) -> void
{
    weaponIndex = static_cast<uint8_t>(weapon);
    targetId = target->partId;
    targetVertexOrCarNumber = target->partId - 0x802c8;
    hit = hitTarget;
    targetType = 2;
    targetBlockOrTrainNumber = 0x80;
    entryAngle = AngleQuadrant(angle);
    numMissiles = static_cast<int8_t>(missiles);
    numMissilesPastAMS = static_cast<int8_t>(missiles);
    data = 0;
}

auto WeaponFireChunk::buildLocationTarget(vector_3d location, int32_t weapon, int hitTarget, int32_t missiles) -> void
{
    targetType = 3;
    int32_t cellR;
    int32_t cellC;
    worldCoordToMapCell(location, cellR, cellC);
    targetCell[0] = static_cast<uint16_t>(cellR);
    hit = hitTarget;
    targetCell[1] = static_cast<uint16_t>(cellC);
    weaponIndex = static_cast<uint8_t>(weapon);
    numMissiles = static_cast<int8_t>(missiles);
    numMissilesPastAMS = static_cast<int8_t>(missiles);
    data = 0;
}

auto WeaponFireChunk::pack() -> void
{
    // From the low bit: target type (2), weapon index (5), hit (1), then the target, and for a missile weapon the
    // missile counts (4 bits each) in front of it.
    data = 0;
    uint32_t packed;
    bool packTarget = true;

    switch (targetType)
    {
        case 0:
        {
            packed = static_cast<uint32_t>((hitLocation + 2) * 0x20) | (static_cast<uint32_t>(entryAngle) << 9) |
                     static_cast<uint32_t>(targetId);
            data = packed;

            if (numMissiles > 0)
            {
                packed = ((packed << 4 | static_cast<uint32_t>(numMissiles)) << 4) |
                         static_cast<uint32_t>(numMissilesPastAMS);
                data = packed << 4 | static_cast<uint32_t>(numAntiMissileShots);
            }
            break;
        }
        case 1:
        {
            packed =
                ((static_cast<uint32_t>(targetBlockOrTrainNumber) << 9 | static_cast<uint32_t>(targetVertexOrCarNumber))
                 << 3) |
                static_cast<uint32_t>(targetItemNumber);
            data = packed;

            if (numMissiles > 0)
            {
                data = packed << 4 | static_cast<uint32_t>(numMissiles);
            }
            break;
        }
        case 2:
        {
            packed = ((static_cast<uint32_t>(entryAngle) << 8 | static_cast<uint32_t>(targetBlockOrTrainNumber)) << 8) |
                     static_cast<uint32_t>(targetVertexOrCarNumber);
            data = packed;

            if (numMissiles > 0)
            {
                data = packed << 4 | static_cast<uint32_t>(numMissiles);
            }
            break;
        }
        case 3:
        {
            packed = static_cast<uint32_t>(targetCell[0]) << 10 | targetCell[1];
            data = packed;

            if (numMissiles > 0)
            {
                data = packed << 4 | static_cast<uint32_t>(numMissiles);
            }
            break;
        }
        default:
            packTarget = false;
            break;
    }

    if (packTarget)
    {
        data = data << 1;
    }

    if (hit != 0)
    {
        data |= 1;
    }

    data = ((static_cast<uint32_t>(weaponIndex) | data << 5) << 2) | static_cast<uint32_t>(targetType);
}

auto WeaponFireChunk::unpack(BigGameObject* attacker) -> void
{
    const uint32_t packed = data;
    weaponIndex = static_cast<uint8_t>((packed >> 2) & 0x1f);
    targetType = static_cast<int8_t>(packed & 3);
    hit = static_cast<int32_t>((packed >> 7) & 1);
    uint32_t rest = packed >> 8;

    int missileWeapon = 0;

    if (IsMover(attacker))
    {
        auto* mover = static_cast<Mover*>(attacker);
        missileWeapon = mover->isWeaponMissile(mover->numOther + weaponIndex);
    }
    else if (attacker->objectClass == TURRET)
    {
        missileWeapon = static_cast<Turret*>(attacker)->isWeaponMissile();
    }

    const uint8_t low = static_cast<uint8_t>(packed >> 8);

    switch (targetType)
    {
        case 0:
        {
            if (missileWeapon != 0)
            {
                numAntiMissileShots = static_cast<int8_t>(low & 0xf);
                numMissilesPastAMS = static_cast<int8_t>((packed >> 12) & 0xf);
                numMissiles = static_cast<int8_t>((packed >> 16) & 0xf);
                rest = packed >> 20;
            }

            targetId = static_cast<int32_t>(rest & 0x1f);
            hitLocation = static_cast<int8_t>(((rest >> 5) & 0xf) - 2);
            entryAngle = static_cast<int8_t>((rest >> 9) & 3);
            return;
        }
        case 1:
        {
            if (missileWeapon != 0)
            {
                numMissiles = static_cast<int8_t>(low & 0xf);
                rest = packed >> 12;
                numMissilesPastAMS = static_cast<int8_t>(low & 0xf);
            }

            const int8_t item = static_cast<int8_t>(rest & 7);
            const uint32_t block = (rest >> 12) & 0xff;
            targetItemNumber = item;
            const uint32_t vertex = (rest >> 3) & 0x1ff;
            targetBlockOrTrainNumber = static_cast<int32_t>(block);
            targetVertexOrCarNumber = static_cast<int32_t>(vertex);
            targetId = static_cast<int32_t>(item + 0x1000 + (block * 400 + vertex) * 8);
            return;
        }

        case 2:
        {
            if (missileWeapon != 0)
            {
                numMissiles = static_cast<int8_t>(low & 0xf);
                rest = packed >> 12;
                numMissilesPastAMS = static_cast<int8_t>(low & 0xf);
            }

            const uint32_t trainNumber = (rest >> 8) & 0xff;
            targetVertexOrCarNumber = static_cast<int32_t>(rest & 0xff);
            targetBlockOrTrainNumber = static_cast<int32_t>(trainNumber);
            const uint8_t angleBits = static_cast<uint8_t>(rest >> 16);

            if (trainNumber != 0x80)
            {
                entryAngle = static_cast<int8_t>(angleBits & 3);
                targetId = targetVertexOrCarNumber + static_cast<int32_t>((trainNumber * 5 + 0x6400) * 0x14);
                return;
            }

            targetId = targetVertexOrCarNumber + 0x802c8;
            entryAngle = static_cast<int8_t>(angleBits & 3);
            return;
        }

        case 3:
        {
            if (missileWeapon != 0)
            {
                numMissiles = static_cast<int8_t>(low & 0xf);
                rest = packed >> 12;
                numMissilesPastAMS = static_cast<int8_t>(low & 0xf);
            }

            targetCell[1] = static_cast<uint16_t>(rest & 0x3ff);
            targetCell[0] = static_cast<uint16_t>((rest >> 10) & 0x3ff);
            return;
        }
        default:
            return;
    }
}

auto WeaponFireChunk::equalTo(WeaponFireChunk* chunk) -> int
{
    if (targetType != chunk->targetType || targetId != chunk->targetId || targetCell[0] != chunk->targetCell[0] ||
        targetCell[1] != chunk->targetCell[1] || weaponIndex != chunk->weaponIndex || hit != chunk->hit ||
        entryAngle != chunk->entryAngle || numMissiles != chunk->numMissiles ||
        numMissilesPastAMS != chunk->numMissilesPastAMS || numAntiMissileShots != chunk->numAntiMissileShots ||
        hitLocation != chunk->hitLocation)
    {
        DebugWeaponFireChunk(this, chunk, nullptr);
        return 0;
    }

    return 1;
}

//---------------------------------------------------------------------------
// WeaponHitChunk
//---------------------------------------------------------------------------

auto WeaponHitChunk::operator new(size_t size) noexcept -> void*
{
    if (systemHeap != nullptr)
    {
        return systemHeap->malloc(static_cast<uint32_t>(size));
    }

    return std::malloc(size);
}

auto WeaponHitChunk::operator delete(void* ptr) -> void
{
    if (systemHeap != nullptr)
    {
        systemHeap->free(ptr);
        return;
    }

    std::free(ptr);
}

auto WeaponHitChunk::buildMoverTarget(BigGameObject* target, int32_t hitCause, float hitDamage, int32_t location,
                                      float angle, int isRefit) -> void
{
    targetType = 0;
    targetId = static_cast<Mover*>(target)->netRosterIndex;
    cause = static_cast<int8_t>(hitCause);
    damage = hitDamage;
    hitLocation = static_cast<int8_t>(location);
    entryAngle = AngleQuadrant(angle);
    refit = isRefit;
    data = 0;
}

auto WeaponHitChunk::buildTerrainTarget(BigGameObject* target, float hitDamage) -> void
{
    targetType = 1;
    data = 0;
    const int32_t partId = target->partId;
    targetId = partId;
    targetBlockOrTrainNumber = (partId - 0x1000) / 0xc80;
    const int32_t rest = (partId - 0x1000) % 0xc80;
    targetVertexOrCarNumber = rest / 8;
    targetItemNumber = static_cast<int8_t>(rest - targetVertexOrCarNumber * 8);
    damage = hitDamage;
}

auto WeaponHitChunk::buildTrainTarget(BigGameObject* target, float hitDamage, float angle) -> void
{
    targetType = 2;
    targetId = target->partId;
    targetBlockOrTrainNumber = (target->partId - 0x7d000) / 100;
    damage = hitDamage;
    targetVertexOrCarNumber = (target->partId - 0x7d000) % 100;
    entryAngle = AngleQuadrant(angle);
    data = 0;
}

auto WeaponHitChunk::buildCameraDroneTarget(BigGameObject* target, float hitDamage, float angle) -> void
{
    targetType = 2;
    targetId = target->partId;
    targetVertexOrCarNumber = target->partId - 0x802c8;
    targetBlockOrTrainNumber = 0x80;
    damage = hitDamage;
    entryAngle = AngleQuadrant(angle);
    data = 0;
}

auto WeaponHitChunk::build(GameObject* target, _WeaponShotInfo* shotInfo, int isRefit) -> void
{
    if (target == nullptr)
    {
        Fatal(0, " WeaponHitChunk.build: NULL target ");
    }

    const float shotDamage = shotInfo->damage;
    Assert(static_cast<double>(shotDamage) == static_cast<int32_t>(static_cast<double>(shotInfo->damage) * 4.0) * 0.25,
           0, " WeaponHitChunk.build: damage round error ");

    if (!IsMover(target))
    {
        switch (target->objectClass)
        {
            case BUILDING:
            case TREE:
            case MISCTERRAINOBJECT:
            case TREEBUILDING:
            case TURRET:
            case GATE:
            {
                buildTerrainTarget(static_cast<BigGameObject*>(target), shotDamage);
                return;
            }
            case CAMERADRONE:
            {
                buildCameraDroneTarget(static_cast<BigGameObject*>(target), shotDamage, shotInfo->entryAngle);
                return;
            }
            case TRAINCAR:
            {
                buildTrainTarget(static_cast<BigGameObject*>(target), shotDamage, shotInfo->entryAngle);
                return;
            }
            default:
                return;
        }
    }

    // The chunk's cause: 0 for a weapon, -4 for a component whose form is 10. The shot keeps the change.
    if (shotInfo->masterId > 0)
    {
        shotInfo->masterId = MasterComponentList[shotInfo->masterId].form == 10 ? -4 : 0;
    }

    buildMoverTarget(static_cast<BigGameObject*>(target), shotInfo->masterId, shotDamage, shotInfo->hitLocation,
                     shotInfo->entryAngle, isRefit);
}

auto WeaponHitChunk::pack() -> void
{
    // From the low bit: target type (2), damage in quarter points (10), then the target.
    const uint32_t type = static_cast<uint32_t>(static_cast<int32_t>(targetType));
    data = 0;

    if (type == 0)
    {
        if (refit != 0)
        {
            data = 1;
        }

        data = ((static_cast<uint32_t>(entryAngle) | data << 2) << 12 | static_cast<uint32_t>(targetId)) << 10 |
               static_cast<uint32_t>((hitLocation + 2) * 0x40000) | static_cast<uint32_t>((cause + 7) * 0x8000);
    }
    else if (type == 1)
    {
        data = ((static_cast<uint32_t>(targetBlockOrTrainNumber) << 9 | static_cast<uint32_t>(targetVertexOrCarNumber))
                    << 3 |
                static_cast<uint32_t>(targetItemNumber))
               << 10;
    }
    else if (type == 2)
    {
        data = ((static_cast<uint32_t>(entryAngle) << 8 | static_cast<uint32_t>(targetBlockOrTrainNumber)) << 8 |
                static_cast<uint32_t>(targetVertexOrCarNumber))
               << 10;
    }
    else
    {
        Fatal(0, " Bad WeaponHitChunk Target Type ");
    }

    const uint32_t quarters = static_cast<uint32_t>(static_cast<int32_t>(static_cast<double>(damage) * 4.0));
    data = (quarters | data) << 2 | type;
}

auto WeaponHitChunk::unpack() -> void
{
    const uint32_t packed = data;
    targetType = static_cast<int8_t>(packed & 3);
    const uint32_t rest = packed >> 12;
    damage = static_cast<float>(((packed >> 2) & 0x3ff) * 0.25);
    Assert(damage >= 0.0 && damage <= 255.0, 0, " WeaponHitChunk.unpack: bad damage ");
    const uint8_t high = static_cast<uint8_t>(packed >> 24);

    if (targetType == 0)
    {
        targetId = static_cast<int32_t>(rest & 0x1f);
        Assert(targetId < MPlayer->numMovers, targetId, " WeaponHitChunk.unpack: bad targetId ");
        cause = static_cast<int8_t>(((packed >> 17) & 7) - 7);
        Assert(cause >= -7 && cause <= 0, cause, " WeaponHitChunk.unpack: bad cause ");
        hitLocation = static_cast<int8_t>(((packed >> 20) & 0xf) - 2);
        Assert(hitLocation >= -1 && hitLocation <= 11, hitLocation, " WeaponHitChunk.unpack: bad hitLocation ");
        entryAngle = static_cast<int8_t>(high & 3);
        refit = static_cast<int32_t>((packed >> 26) & 1);
        return;
    }

    if (targetType == 1)
    {
        const int8_t item = static_cast<int8_t>(rest & 7);
        targetItemNumber = item;
        const uint32_t vertex = (packed >> 15) & 0x1ff;
        targetBlockOrTrainNumber = static_cast<int32_t>(packed >> 24);
        targetVertexOrCarNumber = static_cast<int32_t>(vertex);
        targetId = static_cast<int32_t>(item + 0x1000 + ((packed >> 24) * 400 + vertex) * 8);
        return;
    }

    if (targetType == 2)
    {
        const uint32_t trainNumber = (packed >> 20) & 0xff;
        targetVertexOrCarNumber = static_cast<int32_t>(rest & 0xff);
        targetBlockOrTrainNumber = static_cast<int32_t>(trainNumber);
        const uint8_t angleBits = high >> 4;

        if (trainNumber != 0x80)
        {
            entryAngle = static_cast<int8_t>(angleBits & 3);
            targetId = targetVertexOrCarNumber + static_cast<int32_t>((trainNumber * 5 + 0x6400) * 0x14);
            return;
        }

        targetId = static_cast<int32_t>((rest & 0xff) + 0x802c8);
        entryAngle = static_cast<int8_t>(angleBits & 3);
        return;
    }

    DebugWeaponHitChunk(this, nullptr);
    Fatal(0, " Bad WeaponHitChunk Target Type ");
}

auto WeaponHitChunk::equalTo(WeaponHitChunk* chunk) -> int
{
    if (targetType != chunk->targetType || targetId != chunk->targetId || cause != chunk->cause ||
        damage != chunk->damage || entryAngle != chunk->entryAngle || refit != chunk->refit ||
        hitLocation != chunk->hitLocation)
    {
        DebugWeaponHitChunk(this, chunk);
        return 0;
    }

    return 1;
}

//---------------------------------------------------------------------------
// Debug chunk routines
//---------------------------------------------------------------------------

auto DebugWeaponFireChunk(WeaponFireChunk* chunk1, WeaponFireChunk* chunk2, GameObject* attacker) -> void
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
            std::snprintf(line, sizeof(line), "attacker = %s (%d)\n", static_cast<Mover*>(attacker)->debugStatus,
                          attacker->partId);
        }
        else
        {
            std::snprintf(line, sizeof(line), "attacker = objClass %d (%d)\n", static_cast<int>(attacker->objectClass),
                          attacker->partId);
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

auto LogWeaponFireChunk(WeaponFireChunk*, GameObject*, GameObject*) -> void
{
}

auto DebugWeaponHitChunk(WeaponHitChunk* chunk1, WeaponHitChunk* chunk2) -> void
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

GameObject::GameObject()
{
    objectClass = GAMEOBJECT;
    idNumber = 0;
    next = nullptr;
    partId = -1;
    objType = nullptr;
    position.z = 0.0f;
    position.y = 0.0f;
    position.x = 0.0f;
    selected = 0;
    unknown2C = 0;
    collisionsOn = 0;
    alignment = 0;
    status = 0;
}

auto GameObject::init(ObjectType* type) -> int32_t
{
    objectClass = GAMEOBJECT;
    objType = type;
    alignment = type->teamId;
    return 0;
}

auto GameObject::init() -> void
{
    objectClass = GAMEOBJECT;
    idNumber = 0;
    next = nullptr;
    partId = -1;
    objType = nullptr;
    position.z = 0.0f;
    position.y = 0.0f;
    position.x = 0.0f;
    selected = 0;
    unknown2C = 0;
    collisionsOn = 0;
    alignment = 0;
    status = 0;
}

auto GameObject::destroy() -> void
{
    objectTypeManager->remove(objType);
}

auto GameObject::getPositionFromHS(uint32_t) -> vector_3d
{
    return position;
}

auto GameObject::getBlockAndVertexNumber(int32_t& blockNumber, int32_t& vertexNumber) -> void
{
    Assert(Terrain::metersPerVertex == 128.0f, 0, " Optimizations now broken ");
    // The original keeps each floored coordinate and block index as a 16-bit value.
    const int32_t vertexCol =
        (static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(position.x)))) >> 7) +
        Terrain::verticesMapSide;
    const int32_t blockCol = static_cast<int16_t>(
        static_cast<int32_t>(std::floor(static_cast<double>(vertexCol) * Terrain::OneOververticesBlockSide)));
    const int32_t vertexRow =
        (Terrain::verticesMapSide -
         (static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(position.y)))) >> 7)) -
        1;
    const int32_t blockRow = static_cast<int16_t>(
        static_cast<int32_t>(std::floor(static_cast<double>(vertexRow) * Terrain::OneOververticesBlockSide)));
    blockNumber = Terrain::blocksMapSide * blockRow + blockCol;
    vertexNumber =
        ((vertexRow - Terrain::verticesBlockSide * blockRow) - blockCol) * Terrain::verticesBlockSide + vertexCol;
}

auto GameObject::getPosition() -> vector_3d
{
    return position;
}

auto GameObject::relativePosition(float angle, float distance, uint32_t flags) -> vector_3d
{
    // The point distance meters away at the absolute angle (radians), pulled back along the line to the first
    // cell whose passability changes. The x87 keeps the reach at extended precision, done here in double.
    const double reach = -(static_cast<double>(worldUnitsPerMeter) * distance);
    const float x = position.x;
    const float y = position.y;
    const float offsetX = static_cast<float>((std::sin(static_cast<double>(angle)) + 0.0) * reach);
    const float offsetY = static_cast<float>(static_cast<float>(std::cos(static_cast<double>(angle))) * reach);
    const float targetX = offsetX + x;
    const float targetY = offsetY + y;

    // Flag 2 walks from the object out to the point; otherwise from the point back to the object.
    vector_2d start;
    vector_2d end;

    if ((flags & 2) != 0)
    {
        start.x = x;
        start.y = y;
        end.x = targetX;
        end.y = targetY;
    }
    else
    {
        start.x = targetX;
        start.y = targetY;
        end.x = x;
        end.y = y;
    }

    // Half a map cell per step.
    const float deltaX = end.x - start.x;
    const float deltaY = end.y - start.y;
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

    const float stepLength = static_cast<float>(static_cast<double>(Terrain::metersPerVertex) * 0.33333334f * 0.5);
    const float stepX = static_cast<float>(directionX * stepLength);
    const float stepY = directionY * stepLength;

    if (std::sqrt(static_cast<double>(stepX) * stepX + static_cast<double>(stepY) * stepY) == 0.0)
    {
        vector_3d result;
        result.x = x;
        result.y = y;
        result.z = 0.0f;
        return result;
    }

    const vector_2d span = start - end;
    const float maxDistance =
        static_cast<float>(std::sqrt(static_cast<double>(span.y) * span.y + static_cast<double>(span.x) * span.x));
    float traveled = 0.0f;
    vector_2d current = start;

    // Whether the cell under current is passable.
    auto cellPassable = [&]()
    {
        vector_3d point;
        point.x = current.x;
        point.y = current.y;
        point.z = 0.0f;
        int32_t tileR;
        int32_t tileC;
        int32_t cellR;
        int32_t cellC;
        GameMap->worldToMapPos(point, tileR, tileC, cellR, cellC);

        // Port fix: the walk can leave the map, where the original reads outside it. Off the map is impassable.
        if (!GameMap->onMap(tileR, tileC))
        {
            return 0u;
        }

        return GameMap->map[GameMap->width * tileR + tileC].getCellPassable(cellR, cellC);
    };

    uint32_t passable = cellPassable();
    vector_2d previous = start;
    // Walk until the cell changes kind (or the distance runs out); the answer is the step before.
    const uint32_t keepGoingWhile = (flags & 2) != 0 ? 1u : 0u;

    if ((passable != 0) == (keepGoingWhile != 0))
    {
        while (traveled < maxDistance)
        {
            previous = current;
            current.x = stepX + current.x;
            current.y = stepY + current.y;
            const double dx = static_cast<double>(current.x) - start.x;
            const double dy = static_cast<double>(current.y) - start.y;
            traveled = static_cast<float>(std::sqrt(dx * dx + dy * dy));
            passable = cellPassable();

            if ((passable != 0) != (keepGoingWhile != 0))
            {
                break;
            }
        }
    }

    vector_3d ground;
    ground.x = previous.x;
    ground.y = previous.y;
    ground.z = 0.0f;
    vector_3d result;
    result.x = previous.x;
    result.y = previous.y;
    result.z = GameMap->getTerrainElevation(ground);
    return result;
}

auto GameObject::setPosition(vector_3d& newPosition) -> void
{
    position = newPosition;
}

auto GameObject::getVelocity() -> vector_3d
{
    vector_3d velocity;
    velocity.x = 0.0f;
    velocity.y = 0.0f;
    velocity.z = 0.0f;
    return velocity;
}

auto GameObject::getScreenPos(int32_t) -> vector_2d
{
    vector_2d screen;
    screen.x = 0.0f;
    screen.y = 0.0f;
    return screen;
}

auto GameObject::getFrame() -> frame_of_ref
{
    return frame_of_ref(UnitX, UnitY, UnitZ);
}

auto GameObject::distanceFrom(vector_3d& goal) -> float
{
    const float dx = position.x - goal.x;
    const float dy = position.y - goal.y;
    return static_cast<float>(std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy) *
                              metersPerWorldUnit);
}

auto GameObject::lineOfSight(vector_3d point) -> int
{
    const vector_3d start = position;
    setUseMe(0);
    const int result = GameMap->lineOfSight(start, point);
    setUseMe(1);
    return result;
}

auto GameObject::lineOfSight(GameObject* target) -> int
{
    // From eye to eye, ten meters up.
    vector_3d start;
    start.x = position.x;
    start.y = position.y;
    start.z = static_cast<float>(static_cast<double>(worldUnitsPerMeter) * 10.0 + position.z);
    const vector_3d targetPosition = target->getPosition();
    vector_3d end;
    end.x = targetPosition.x;
    end.y = targetPosition.y;
    end.z = static_cast<float>(static_cast<double>(worldUnitsPerMeter) * 10.0 + targetPosition.z);
    setUseMe(0);
    target->setUseMe(0);
    const int result = GameMap->lineOfSight(start, end);
    setUseMe(1);
    target->setUseMe(1);
    return result;
}

auto GameObject::lineOfFire(GameObject* target) -> int
{
    vector_3d start;
    start.x = position.x;
    start.y = position.y;
    start.z = static_cast<float>(static_cast<double>(worldUnitsPerMeter) * 10.0 + position.z);
    const vector_3d targetPosition = target->getPosition();
    vector_3d end;
    end.x = targetPosition.x;
    end.y = targetPosition.y;
    end.z = static_cast<float>(static_cast<double>(worldUnitsPerMeter) * 10.0 + targetPosition.z);
    setUseMe(0);
    target->setUseMe(0);
    const int result = GameMap->lineOfFire(start, end);
    setUseMe(1);
    target->setUseMe(1);
    return result;
}

auto GameObject::relFacingTo(vector_3d goal, int32_t) -> float
{
    // The facing is the world frame's -j, turned an eighth.
    const float x = position.x;
    const float y = position.y;
    const frame_of_ref turned = TurnedFrame(frame_of_ref(UnitX, UnitY, UnitZ));
    vector_3d facing;
    facing.x = -turned.j.x;
    facing.y = -turned.j.y;
    facing.z = -turned.j.z;

    vector_3d toGoal;
    toGoal.x = goal.x - x;
    toGoal.y = goal.y - y;
    toGoal.z = 0.0f;
    const float length = toGoal.magnitude();

    if (length != 0.0f)
    {
        toGoal.x = toGoal.x / length;
        toGoal.y = toGoal.y / length;
        toGoal.z = toGoal.z / length;
    }

    const double cosine = static_cast<double>(toGoal.z) * facing.z + static_cast<double>(toGoal.y) * facing.y +
                          static_cast<double>(toGoal.x) * facing.x;
    const float angle = static_cast<float>(std::acos(cosine) * RADIANS_TO_DEGREES_F);

    // Negative to the left.
    if ((facing & toGoal).z >= 0.0f)
    {
        return -angle;
    }

    return angle;
}

auto GameObject::relViewFacingTo(vector_3d goal) -> float
{
    return GameObject::relFacingTo(goal, -1);
}

auto GameObject::getExtentRadius() -> float
{
    return objType->extentRadius;
}

auto GameObject::setExtentRadius(float newRadius) -> void
{
    objType->extentRadius = newRadius;
}

auto GameObject::getCaptureBlocker(int32_t side) -> GameObject*
{
    // Clan movers block only when they aren't marines.
    const bool clan = side == 1;
    ObjectQueueNode* list = clan ? clanMechList : innerSphereMechList;

    for (BaseObject* object = list->head; object != nullptr; object = object->next)
    {
        if (!IsMover(object))
        {
            continue;
        }

        auto* mover = static_cast<Mover*>(object);

        if (clan && mover->isMarine() != 0)
        {
            continue;
        }

        if (mover->numWeapons == 0)
        {
            continue;
        }

        vector_3d moverPosition = mover->getPosition();

        if (distanceFrom(moverPosition) < BlockCaptureRange && mover->isDestroyed() == 0 && mover->isDisabled() == 0 &&
            mover->getAwake() != 0)
        {
            return mover;
        }
    }

    return nullptr;
}

auto GameObject::clearLineOfFire() -> void
{
    // Sets the line-of-sight bit (15 + 2c) of each of the tile's nine cells, saving the old bits.
    MapTile& tile = TileAt(position);

    for (int32_t cell = 0; cell < 9; cell++)
    {
        const uint32_t shift = static_cast<uint32_t>(cell * 2 + 15);
        const uint32_t mask = 1u << shift;
        objCellArray[cell] = static_cast<int32_t>((tile.cells & mask) >> shift);
        tile.cells = (~mask & tile.cells) | mask;
    }
}

auto GameObject::restoreLineOfFire() -> void
{
    MapTile& tile = TileAt(position);

    for (int32_t cell = 0; cell < 9; cell++)
    {
        const uint32_t shift = static_cast<uint32_t>(cell * 2 + 15);
        tile.cells = static_cast<uint32_t>(objCellArray[cell]) << shift | (~(1u << shift) & tile.cells);
    }
}

//---------------------------------------------------------------------------
// BigGameObject
//---------------------------------------------------------------------------

auto BigGameObject::init(ObjectType* type) -> int32_t
{
    objectClass = BIGGAMEOBJECT;
    objType = type;
    return 0;
}

auto BigGameObject::init() -> void
{
    objectClass = BIGGAMEOBJECT;
    idNumber = 0;
    next = nullptr;
    partId = -1;
    objType = nullptr;
    tonnage = 0.0f;
    blipTime = 0.0f;
    position.z = 0.0f;
    position.y = 0.0f;
    position.x = 0.0f;
    objPosition = nullptr;
    team = nullptr;
    potentialContact = nullptr;
    collisionFreeFrom = nullptr;
    collisionFreeTime = 0.0f;
    status = 0;
    damage = 0.0f;
    flags = 5;
    alignment = 0;
    selected = 0;
    unknown2C = 0;
    screenPos.x = 0.0f;
    screenPos.y = 0.0f;
    windowsVisible = 0;
    maxCV = 0;
    curCV = 0;
    collisionsOn = 0;
    explDamage = 0.0f;
    explRadius = 0.0f;
    salvage = nullptr;
    blipFrame = 0;
    numAttackers = 0;
}

auto BigGameObject::destroy() -> void
{
    while (salvage != nullptr)
    {
        SalvageItem* nextItem = salvage->next;
        delete salvage;
        salvage = nextItem;
    }

    if (potentialContact != nullptr)
    {
        potentialContactManager->remove(potentialContact);
    }

    if (objPosition != nullptr)
    {
        GameObjectMap->removeObject(this);
    }
}

auto BigGameObject::kill() -> int32_t
{
    // Port fix: with no type the original used uninitialised ids.
    int32_t explosionId = -1;
    int32_t destroyedId = -1;

    if (getObjectType() != nullptr)
    {
        explosionId = getObjectType()->explosionObject;
        destroyedId = getObjectType()->destroyedObject;
    }

    if (explosionId != -1)
    {
        GameObject* explosion = createObject(explosionId);
        vector_3d here = getPosition();
        explosion->setPosition(here);
        vector_3d velocity = getVelocity();
        explosion->setVelocity(velocity);

        if (objectList->head != nullptr && explosion != nullptr)
        {
            objectList->head->addNode(explosion);
        }
    }

    if (destroyedId != -1)
    {
        GameObject* wreck = createObject(destroyedId);
        vector_3d here = getPosition();
        wreck->setPosition(here);
        vector_3d velocity = getVelocity();
        wreck->setVelocity(velocity);
        frame_of_ref frame = getFrame();
        wreck->setFrame(frame);

        if (objectList->head != nullptr && wreck != nullptr)
        {
            objectList->head->addNode(wreck);
        }
    }

    return static_cast<int32_t>(0xbeaddead);
}

auto BigGameObject::onScreen() -> int
{
    Camera* camera = cameraList->findCameraFromIDNumber(1);

    if (camera == nullptr || camera->active == 0)
    {
        return 0;
    }

    // At camera scale 1 everything is drawn at half size.
    const float scale = camera->cameraScale != 1 ? 1.0f : 0.5f;
    const float dx = (position.x - camera->position.x) * scale;
    const float dy = (position.y - camera->position.y) * scale;
    const float dz = scale * position.z - camera->position.z;
    const float screenX = static_cast<float>(static_cast<double>(dy) * camera->cosAngle +
                                             static_cast<double>(dx) * camera->cosAngle + camera->halfWidth);
    screenPos.x = screenX;
    const float screenY = static_cast<float>(((static_cast<double>(dx) * camera->sinAngle + camera->halfHeight) -
                                              static_cast<double>(dy) * camera->sinAngle) -
                                             static_cast<double>(scale) * dz);
    screenPos.y = screenY;

    if (screenX >= 0.0f && screenY >= 0.0f && screenX <= camera->viewWidth && screenY <= camera->viewHeight)
    {
        windowsVisible = turn;
        return 1;
    }

    return 0;
}

auto BigGameObject::setPotentialContact(int32_t contactType) -> void
{
    // The contact list by alignment: 1 for -1, 0 for 1, else 2.
    int32_t listType;

    if (alignment == -1)
    {
        listType = 1;
    }
    else
    {
        listType = alignment != 1 ? 2 : 0;
    }

    if (contactType == 0)
    {
        potentialContactManager->remove(potentialContact);
        potentialContact = nullptr;
        return;
    }

    if (potentialContact == nullptr)
    {
        potentialContact = potentialContactManager->add(listType, this, static_cast<char>(contactType));
        return;
    }

    potentialContactManager->move(potentialContact, listType, static_cast<char>(contactType));
}

auto BigGameObject::updateContactStatus(Team* contactTeam) -> void
{
    if (potentialContact != nullptr)
    {
        potentialContact->updateStatus(contactTeam);
    }
}

auto BigGameObject::getContactCount(int32_t teamId) -> int32_t
{
    Assert(teamId > -1, -1, " Bad Team Id ");

    if (potentialContact != nullptr)
    {
        return static_cast<uint8_t>(potentialContact->numSensors[teamId]);
    }

    return 0;
}

// The "tagged" flag is the contact's lostVisual byte (+0x0e).

auto BigGameObject::setContactTagged(int32_t teamId, int tagged) -> void
{
    Assert(teamId > -1, -1, " Bad Team Id ");
    Assert(potentialContact != nullptr, -1, " Is Not Potential Contact ");

    if (potentialContact != nullptr)
    {
        potentialContact->lostVisual[teamId] = static_cast<uint8_t>(tagged);
    }
}

auto BigGameObject::getContactTagged(int32_t teamId) -> int
{
    Assert(teamId > -1, -1, " Bad Team Id ");

    if (potentialContact != nullptr)
    {
        return potentialContact->lostVisual[teamId];
    }

    return 0;
}

auto BigGameObject::getContactType(int32_t teamId, int& tagged) -> int32_t
{
    Assert(teamId > -1, -1, " Bad Team Id ");

    if (potentialContact != nullptr)
    {
        tagged = potentialContact->lostVisual[teamId];
        return potentialContact->contactStatus[teamId];
    }

    return 0;
}

auto BigGameObject::getContactType(int32_t teamId) -> int32_t
{
    Assert(teamId > -1, -1, " Bad Team Id ");

    if (potentialContact != nullptr)
    {
        return potentialContact->contactStatus[teamId];
    }

    return 0;
}

auto BigGameObject::getScreenPos(int32_t) -> vector_2d
{
    return screenPos;
}

auto BigGameObject::setAlignment(int32_t newAlignment) -> void
{
    alignment = newAlignment;

    if (potentialContact != nullptr)
    {
        setPotentialContact(potentialContact->visibility);
    }
}

auto BigGameObject::write(File* objFile) -> int32_t
{
    objFile->write(reinterpret_cast<const uint8_t*>(&tonnage), 4);
    objFile->writeLong(status);
    objFile->writeLong(static_cast<int32_t>(damage));
    objFile->writeByte(isCaptured() != 0 ? 1 : 0);
    objFile->write(reinterpret_cast<const uint8_t*>(&explRadius), 4);
    objFile->write(reinterpret_cast<const uint8_t*>(&explDamage), 4);
    return 0;
}

auto BigGameObject::getMechClass() -> MechClass
{
    if (objectClass != BATTLEMECH)
    {
        return MECH_CLASS_NONE;
    }

    if (tonnage < 35.0f)
    {
        return MECH_CLASS_LIGHT;
    }

    if (tonnage < 55.0f)
    {
        return MECH_CLASS_MEDIUM;
    }

    if (tonnage >= 75.0f)
    {
        return MECH_CLASS_ASSAULT;
    }

    return MECH_CLASS_HEAVY;
}

auto BigGameObject::decrementAttackers() -> void
{
    Assert(numAttackers > 0, 0, nullptr);
    numAttackers--;
}

auto BigGameObject::getVitalInfo(void* vitalInfo) -> int32_t
{
    // The original only asks isCaptured and fills nothing.
    if (vitalInfo != nullptr)
    {
        isCaptured();
    }

    return 0x19;
}
