#include "stdafx.h"
#include "object/MCWeaponShotInfo.h"
#include "lib/MCFatal.h"
#include "mission/MCScenario.h"
#include "mission/MCDifficultySettings.h"
#include "network/MCMultiPlayer.h"
#include "object/MCForces.h"
#include "object/MCGameObject.h"
#include "object/MCTeam.h"

auto AngleQuadrant(float angle) -> int8_t
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

auto SnapAngle(float angle) -> float
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

auto QuarterPoints(float damage) -> float
{
    return static_cast<float>(static_cast<int32_t>(static_cast<double>(damage) * 4.0) * 0.25);
}

auto MCWeaponShotInfo::Init(MCGameObject* shooter, int32_t weaponMasterId, float shotDamage, int32_t shotHitLocation,
                            float shotEntryAngle) -> void
{
    Attacker = shooter;

    if (MultiPlayer() == nullptr && shooter != nullptr)
    {
        // The difficulty scales the player's shots, and the enemy's mechs, vehicles, elementals and turrets.
        const MCObjectClass shooterClass = shooter->ObjectClass;
        const int player = shooter->GetAlignment() == HomeTeam()->Alignment ? 1 : 0;

        if (player != 0 || shooterClass == MCObjectClass::BattleMech || shooterClass == MCObjectClass::GroundVehicle ||
            shooterClass == MCObjectClass::Elemental || shooterClass == MCObjectClass::Turret)
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

    if (MultiPlayer() != nullptr && MultiPlayer()->IsServer != 0)
    {
        Damage = QuarterPoints(shotDamage);
        EntryAngle = SnapAngle(shotEntryAngle);
    }
}

auto MCWeaponShotInfo::SetDamage(float shotDamage) -> void
{
    Damage = shotDamage;

    if (MultiPlayer() != nullptr && MultiPlayer()->IsServer != 0)
    {
        Damage = QuarterPoints(shotDamage);
    }
}

auto MCWeaponShotInfo::SetEntryAngle(float shotEntryAngle) -> void
{
    EntryAngle = shotEntryAngle;

    if (MultiPlayer() != nullptr && MultiPlayer()->IsServer != 0)
    {
        EntryAngle = SnapAngle(shotEntryAngle);
    }
}
