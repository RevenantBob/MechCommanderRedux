#include "stdafx.h"
#include "object/MCGateType.h"
#include "lib/MCFitIniFile.h"
#include "object/MCGate.h"
#include "object/MCMover.h"

auto MCGateType::CreateInstance() -> std::unique_ptr<MCBaseObject>
{
    auto newGate = std::make_unique<MCGate>();

    if (newGate->Init(this) != 0)
    {
        return nullptr;
    }

    newGate->IdNumber = NextIdNumber++;
    return newGate;
}

auto MCGateType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile gateFile;

    if (const int32_t result = gateFile.Open(objFile, fileSize); result != 0)
    {
        return result;
    }

    if (const int32_t result = gateFile.SeekBlock("GateData"); result != 0)
    {
        return result;
    }

    MCFitReader read(gateFile);
    read.Value("DmgLevel", DmgLevel);

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    ReadEffectId(gateFile, "BlownEffectId", BlownEffectId);
    ReadEffectId(gateFile, "NormalEffectId", NormalEffectId);
    ReadEffectId(gateFile, "DamageEffectId", DamageEffectId);
    BasePixelOffsetX = gateFile.Read<int32_t>("BasePixelOffsetX").value_or(0);
    BasePixelOffsetY = gateFile.Read<int32_t>("BasePixelOffsetY").value_or(0);
    ExplosionRadius = gateFile.Read<float>("ExplosionRadius").value_or(0.0f);
    ExplosionDamage = gateFile.Read<float>("ExplosionDamage").value_or(0.0f);
    read.Value("OpenRadius", OpenRadius);

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    LittleExtent = gateFile.Read<float>("LittleExtent").value_or(20.0f);
    BuildingName = gateFile.Read<int32_t>("BuildingName").value_or(0xa5);
    BlocksLineOfFire = gateFile.Read<bool>("BlocksLineOfFire").value_or(false);
    return MCObjectType::Init(&gateFile);
}

auto MCGateType::HandleCollision(MCGameObject* collidee, MCGameObject* collider) -> int
{
    // Only mechs, vehicles and elementals open gates or get caught in them.
    if (collider->ObjectClass < MCObjectClass::BattleMech || MCObjectClass::Explosion <= collider->ObjectClass)
    {
        return 1;
    }

    auto* gate = static_cast<MCGate*>(collidee);
    const MCVector3D colliderPos = collider->GetPosition();
    const MCVector3D gatePos = gate->GetPosition();
    const float distanceSq = (gatePos.X - colliderPos.X) * (gatePos.X - colliderPos.X) +
                             (gatePos.Y - colliderPos.Y) * (gatePos.Y - colliderPos.Y);

    // A friendly (or, for a neutral gate, any) live unit within openRadius asks it to open.
    if ((collider->GetAlignment() == gate->GetAlignment() || gate->GetAlignment() == 0) &&
        collider->IsDisabled() == 0 && collider->IsDestroyed() == 0 && distanceSq < OpenRadius * OpenRadius)
    {
        gate->OpenRequested = true;
    }

    // A live unit standing in it (not jumping) is what a closing gate crushes.
    if (distanceSq < 1.2e8f && collider->IsDisabled() == 0 && collider->IsDestroyed() == 0 &&
        static_cast<MCMover*>(collider)->IsJumping(nullptr) == 0)
    {
        gate->OffendingObject = collider;
    }

    return 1;
}
