#include "stdafx.h"
#include "object/MCWeaponHitChunk.h"
#include "lib/MCFatal.h"
#include "network/MCMultiPlayer.h"
#include "object/MCBigGameObject.h"
#include "object/MCMasterComponent.h"
#include "object/MCWeaponChunkDebug.h"
#include "object/MCWeaponShotInfo.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"

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

    if (!IsMoverClass(target->ObjectClass))
    {
        switch (target->ObjectClass)
        {
            case MCObjectClass::Building:
            case MCObjectClass::Tree:
            case MCObjectClass::MiscTerrainObject:
            case MCObjectClass::TreeBuilding:
            case MCObjectClass::Turret:
            case MCObjectClass::Gate:
            {
                BuildTerrainTarget(static_cast<MCBigGameObject*>(target), shotDamage);
                return;
            }
            case MCObjectClass::CameraDrone:
            {
                BuildCameraDroneTarget(static_cast<MCBigGameObject*>(target), shotDamage, shotInfo->EntryAngle);
                return;
            }
            case MCObjectClass::TrainCar:
            {
                BuildTrainTarget(static_cast<MCBigGameObject*>(target), shotDamage, shotInfo->EntryAngle);
                return;
            }
            default:
                return;
        }
    }

    // The chunk's cause: 0 for a weapon, -4 for ammo. The shot keeps the change.
    if (shotInfo->MasterId > 0)
    {
        shotInfo->MasterId = MasterComponentList[shotInfo->MasterId].Form == MCComponentForm::Ammo ? -4 : 0;
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
        Assert(TargetId < MultiPlayer()->NumMovers, TargetId, " WeaponHitChunk.unpack: bad targetId ");
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
