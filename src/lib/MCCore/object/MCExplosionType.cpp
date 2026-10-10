#include "stdafx.h"
#include "object/MCExplosionType.h"
#include "lib/MCFitIniFile.h"
#include "network/MCMultiPlayer.h"
#include "object/MCExplosion.h"
#include "object/MCGateType.h"
#include "object/MCTurretType.h"
#include "object/MCWeaponShotInfo.h"

namespace
{
    /// <summary>
    /// For turrets and gates: false when the explosion's radius doesn't reach the collider's extent (measured
    /// centre to centre).
    /// </summary>
    bool ReachesExtent(MCGameObject* explosion, MCGameObject* collider, float extent)
    {
        const MCVector3D colliderPos = collider->GetPosition();
        const MCVector3D explosionPos = explosion->GetPosition();
        const double dx = static_cast<double>(colliderPos.X) - explosionPos.X;
        const double dy = static_cast<double>(colliderPos.Y) - explosionPos.Y;
        const float dz = colliderPos.Z - explosionPos.Z;
        const auto distance = static_cast<float>(std::sqrt((dx * dx + dy * dy) + static_cast<double>(dz) * dz));
        return !(extent < distance &&
                 static_cast<double>(explosion->GetExtentRadius()) < static_cast<double>(distance) - extent);
    }
} // namespace

auto MCExplosionType::CreateInstance() -> std::unique_ptr<MCBaseObject>
{
    auto newExplosion = std::make_unique<MCExplosion>();

    if (newExplosion->Init(this) != 0)
    {
        return nullptr;
    }

    newExplosion->IdNumber = NextIdNumber++;
    return newExplosion;
}

auto MCExplosionType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile explFile;

    if (const int32_t result = explFile.Open(objFile, fileSize); result != 0)
    {
        return result;
    }

    if (const int32_t result = explFile.SeekBlock("ExplosionData"); result != 0)
    {
        return result;
    }

    MCFitReader read(explFile);
    read.Value("DmgLevel", DmgLevel);
    read.Value("SoundEffectId", SoundEffectId);

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    ExplosionRadius = explFile.Read<int32_t>("ExplosionRadius").value_or(0);
    LightObjectId = explFile.Read<uint32_t>("LightObjectId").value_or(0xffffffff);
    DamageChunkSize = explFile.Read<float>("DamageChunkSize").value_or(5.0f);
    return MCObjectType::Init(&explFile);
}

auto MCExplosionType::HandleCollision(MCGameObject* collidee, MCGameObject* collider) -> int
{
    // Only the server deals explosion damage in multiplayer.
    if (MultiPlayer() != nullptr && MultiPlayer()->IsServer == 0)
    {
        return 0;
    }

    const float damage = collidee->GetExplDmg();

    if (damage == 0.0f)
    {
        return 0;
    }

    // Original behaviour (OB-153): every hit is a whole chunk, the last one too, so the damage dealt is rounded up to
    // a whole number of chunks.
    const float chunk = DamageChunkSize < damage ? DamageChunkSize : damage;
    const int multiplayer = MultiPlayer() != nullptr ? 1 : 0;
    MCWeaponShotInfo shot;

    switch (collider->ObjectClass)
    {
        case MCObjectClass::BattleMech:
        case MCObjectClass::GroundVehicle:
        case MCObjectClass::Elemental:
        case MCObjectClass::Mover:
        {
            // Movers take the damage in chunks, each on a location of its own.
            shot.Init(nullptr, -1, chunk, 0, 0.0f);

            for (float remaining = damage; 0.0f < remaining; remaining -= DamageChunkSize)
            {
                shot.HitLocation = collider->CalcHitLocation(collidee, -1, 4, 0);
                collider->HandleWeaponHit(&shot, multiplayer);
            }

            return 0;
        }

        case MCObjectClass::Turret:
        case MCObjectClass::Gate:
        {
            const float littleExtent = collider->ObjectClass == MCObjectClass::Turret
                                           ? static_cast<MCTurretType*>(collider->ObjType)->LittleExtent
                                           : static_cast<MCGateType*>(collider->ObjType)->LittleExtent;

            if (!ReachesExtent(collidee, collider, littleExtent))
            {
                return 0;
            }

            // The turret's copy reads the damage afresh (the same value).
            shot.Init(nullptr, -1, chunk, 0, 0.0f);

            for (float remaining = collidee->GetExplDmg(); 0.0f < remaining; remaining -= DamageChunkSize)
            {
                shot.HitLocation = 0;
                collider->HandleWeaponHit(&shot, multiplayer);
            }

            return 0;
        }

        default:
        {
            // Anything else takes it all at once.
            shot.Init(nullptr, -1, collidee->GetExplDmg(), 0, 0.0f);
            collider->HandleWeaponHit(&shot, multiplayer);
            return 0;
        }
    }
}
