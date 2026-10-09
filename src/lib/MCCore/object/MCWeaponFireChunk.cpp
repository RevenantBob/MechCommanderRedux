#include "stdafx.h"
#include "object/MCWeaponFireChunk.h"
#include "ai/MCMoveSystem.h"
#include "lib/MCFatal.h"
#include "network/multplyr.h"
#include "object/MCBigGameObject.h"
#include "object/MCWeaponChunkDebug.h"
#include "object/MCWeaponShotInfo.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"
#include "object/MCTurret.h"
#include "object/MCTurretType.h"

auto MCWeaponFireChunk::Init() -> void
{
    HitLocation = -1;
    TargetType = 0;
    TargetId = 0;
    TargetBlockOrTrainNumber = 0;
    TargetVertexOrCarNumber = 0;
    TargetItemNumber = 0;
    TargetCell = {};
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
    Assert(partId != -1, static_cast<int32_t>(target->ObjectClass), " WeaponFireChunk.buildTerrainTarget: -1 partId ");
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
        {
            packTarget = false;
            break;
        }
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

    if (IsMoverClass(attacker->ObjectClass))
    {
        auto* mover = static_cast<MCMover*>(attacker);
        missileWeapon = mover->IsWeaponMissile(mover->NumOther + WeaponIndex);
    }
    else if (attacker->ObjectClass == MCObjectClass::Turret)
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
    if (TargetType != chunk->TargetType || TargetId != chunk->TargetId || TargetCell != chunk->TargetCell ||
        WeaponIndex != chunk->WeaponIndex || Hit != chunk->Hit || EntryAngle != chunk->EntryAngle ||
        NumMissiles != chunk->NumMissiles || NumMissilesPastAms != chunk->NumMissilesPastAms ||
        NumAntiMissileShots != chunk->NumAntiMissileShots || HitLocation != chunk->HitLocation)
    {
        DebugWeaponFireChunk(this, chunk, nullptr);
        return 0;
    }

    return 1;
}
