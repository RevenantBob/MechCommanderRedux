#include "stdafx.h"
#include "object/MCElemental.h"
#include "ai/MCTacticalOrder.h"
#include "camera/MCCamera.h"
#include "iface/MCTacticalInterface.h"
#include "lib/MCFatal.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCDice.h"
#include "main/MCMissionGlobals.h"
#include "mission/MCScenario.h"
#include "network/MCMultiPlayer.h"
#include "object/MCAIControl.h"
#include "object/MCArtillery.h"
#include "object/MCArtilleryType.h"
#include "object/MCArtilleryChunk.h"
#include "object/MCCameraDrone.h"
#include "object/MCCameraDroneType.h"
#include "object/MCBullet.h"
#include "object/MCBulletType.h"
#include "object/MCMasterComponent.h"
#include "object/MCMoverGroup.h"
#include "object/MCGroundVehicleGameSystem.h"
#include "object/MCLaser.h"
#include "object/MCLaserType.h"
#include "object/MCBattleMech.h"
#include "object/MCMechGameSystem.h"
#include "object/MCObjectSystem.h"
#include "object/MCProjectileLaser.h"
#include "object/MCProjectileLaserType.h"
#include "object/MCMechWarrior.h"
#include "object/MCMoverGameSystem.h"
#include "sound/MCSoundSystem.h"
#include "terrain/MCTerrain.h"
#include "object/MCObjectType.h"
#include "object/MCWeaponShotInfo.h"

namespace
{
    /// <summary>getWeaponShots' answer for a weapon that needs no ammo.</summary>
    constexpr int32_t UNLIMITED_SHOTS = 9999;
    /// <summary>How far (world units) a missed shot lands from the target, either way.</summary>
    constexpr float MISS_SCATTER = 25.0f;

    /// <summary>Adds a shot to a bullet.</summary>
    void AddBulletShot(MCBullet* bullet, MCWeaponShotInfo& shot)
    {
        bullet->NewShot().Init(shot.Attacker, shot.MasterId, shot.Damage, shot.HitLocation, shot.EntryAngle);
    }

    /// <summary>
    /// Points a weapon effect from the elemental at <paramref name="target"/>; lasers and projectiles also carry
    /// <paramref name="shot"/> (a bullet's shots are added as it is loaded). Elementals fire from hot spot 0.
    /// </summary>
    void AimWeaponFX(MCElemental* elemental, MCGameObject* fx, MCGameObject* target, MCWeaponShotInfo& shot,
                     int32_t targetHotSpot)
    {
        if (fx->ObjectClass == MCObjectClass::Bullet)
        {
            auto* bullet = static_cast<MCBullet*>(fx);
            bullet->Owner = elemental;
            bullet->Target = target;
            bullet->OwnerHotSpot = 0;
            bullet->TargetHotSpot = targetHotSpot;
        }
        else if (fx->ObjectClass == MCObjectClass::Laser)
        {
            auto* laser = static_cast<MCLaser*>(fx);
            laser->Source.SetWatcher(elemental);
            laser->Target.SetWatcher(target);
            laser->SourceHotSpot = 0;
            laser->TargetHotSpot = targetHotSpot;
            laser->ShotInfo.Init(shot.Attacker, shot.MasterId, shot.Damage, shot.HitLocation, shot.EntryAngle);
        }
        else
        {
            auto* projectile = static_cast<MCProjectileLaser*>(fx);
            projectile->Owner = elemental;
            projectile->Target = target;
            projectile->OwnerHotSpot = 0;
            projectile->TargetHotSpot = targetHotSpot;
            projectile->ShotInfo.Init(shot.Attacker, shot.MasterId, shot.Damage, shot.HitLocation, shot.EntryAngle);
        }
    }

    /// <summary>Sends a missed weapon effect from the elemental to <paramref name="landing"/>.</summary>
    void ConnectMissFX(MCElemental* elemental, MCGameObject* fx, MCVector3D& landing, MCWeaponShotInfo& shot)
    {
        if (fx->ObjectClass == MCObjectClass::Bullet)
        {
            static_cast<MCBullet*>(fx)->Connect(elemental, landing, 0);
        }
        else if (fx->ObjectClass == MCObjectClass::Laser)
        {
            static_cast<MCLaser*>(fx)->Connect(elemental, landing, &shot, 0);
        }
        else
        {
            static_cast<MCProjectileLaser*>(fx)->Connect(elemental, landing, &shot, 0);
        }
    }

    /// <summary>Where a missed shot lands: up to <see cref="MISS_SCATTER"/> about the target.</summary>
    /// <param name="centred">Missiles scatter both ways; other shots (as the original computes them) only one.</param>
    MCVector3D MissPoint(MCGameObject* target, int centred)
    {
        MCVector3D miss;
        miss.X = MISS_SCATTER;
        miss.Y = MISS_SCATTER;
        miss.Z = 0.0f;
        const auto offsetX = static_cast<float>(RandomNumber(static_cast<int32_t>(miss.X + miss.X)) - miss.X);
        const auto offsetY = static_cast<float>(RandomNumber(static_cast<int32_t>(miss.Y + miss.Y)) - miss.Y);
        const auto offsetZ = static_cast<float>(RandomNumber(static_cast<int32_t>(miss.Z + miss.Z)) - miss.Z);

        if (centred != 0)
        {
            miss.X = offsetX;
            miss.Y = offsetY;
        }
        else
        {
            miss.X = miss.X + offsetX;
            miss.Y = miss.Y + offsetY;
        }

        miss.Z = miss.Z + offsetZ;
        const MCVector3D base = target->GetPosition();
        miss.X += base.X;
        miss.Y += base.Y;
        miss.Z += base.Z;
        return miss;
    }

    /// <summary>
    /// Splits a missile flight into volleys: SRMs and LRMs fly in clusters (<see cref="ClusterSizeSrm"/>,
    /// <see cref="ClusterSizeLrm"/>), anything else in one.
    /// </summary>
    /// <returns>The number of volleys.</returns>
    int32_t MissileVolleys(const MCMasterComponent& weapon, int32_t missiles, int32_t& volleySize)
    {
        volleySize = 1;

        if (weapon.MissileType == 1 || weapon.MissileType == 2)
        {
            volleySize = weapon.MissileType == 1 ? ClusterSizeSrm : ClusterSizeLrm;
            int32_t numVolleys = missiles / volleySize;

            if (missiles % volleySize != 0)
            {
                numVolleys++;
            }

            return numVolleys;
        }

        return 1;
    }
}

auto MCElemental::CalcAttackChance(MCGameObject* target, int32_t aimLocation, float targetTime, int32_t weaponIndex,
                                   float modifiers, int32_t* range, MCVector3D* targetPoint) -> float
{
    if (NumOther <= weaponIndex && weaponIndex < NumOther + NumWeapons)
    {
        return MCMover::CalcAttackChance(target, aimLocation, targetTime, weaponIndex, modifiers, range, targetPoint);
    }

    return -1000.0f;
}

auto MCElemental::CalcHitLocation(MCGameObject* attacker, int32_t weaponIndex, int32_t attackSource, int32_t attackType)
    -> int32_t
{
    return 0;
}

auto MCElemental::HitInventoryItem(int32_t itemIndex, int setupOnly) -> int
{
    return 0;
}

auto MCElemental::HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if (shotInfo == nullptr)
    {
        return 0;
    }

    MCGameObject* attacker = shotInfo->Attacker;
    BadGuy = attacker;

    if (shotInfo->Damage <= 0.0f || shotInfo->HitLocation == -1)
    {
        return 0;
    }

    if (IsDestroyed() != 0)
    {
        return 0;
    }

    CurHealth = static_cast<int32_t>(static_cast<double>(CurHealth) - shotInfo->Damage);

    if (CurHealth < 1)
    {
        Pilot->HandleOwnVehicleIncapacitation(0);

        if (ElementalCanJump == 0)
        {
            RemoveMarine(0.0f);
        }
        else
        {
            ObjType->HandleDestruction(this, nullptr);
        }
    }

    DamageRateTally = shotInfo->Damage + DamageRateTally;
    TotalDamageTaken = shotInfo->Damage + TotalDamageTaken;

    if (attacker == nullptr)
    {
        Pilot->TriggerAlarm(MCPilotAlarmType::HitByWeaponFire, 0);
    }
    else if (shotInfo->MasterId < 0)
    {
        Pilot->TriggerAlarm(MCPilotAlarmType::Collision, static_cast<uint32_t>(attacker->PartId));
    }
    else
    {
        Pilot->TriggerAlarm(MCPilotAlarmType::HitByWeaponFire, static_cast<uint32_t>(attacker->PartId));
    }

    CurCV = CalcCV(0);
    return 0;
}

auto MCElemental::FireWeapon(MCGameObject* target, float targetTime, int32_t weaponIndex, int32_t attackType,
                             int32_t aimLocation, MCVector3D* targetPoint) -> int32_t
{
    if (Status == 5 || Status == 4 || Status == 1 || Status == 2)
    {
        return 1;
    }

    if (IsWeaponReady(weaponIndex) == 0)
    {
        return 3;
    }

    float distance;

    if (target == nullptr)
    {
        if (targetPoint == nullptr || LineOfSight(*targetPoint) == 0)
        {
            return 4;
        }

        distance = static_cast<float>(DistanceFrom(*targetPoint));
    }
    else
    {
        // A camera drone can't be shot for two seconds after launch.
        if (target->ObjectClass == MCObjectClass::CameraDrone &&
            ScenarioTime < static_cast<MCCameraDrone*>(target)->LaunchTime + 2.0)
        {
            return 4;
        }

        if (target->IsDestroyed() != 0)
        {
            return 4;
        }

        if (LineOfSight(target) == 0)
        {
            return 4;
        }

        MCVector3D targetPosition = target->GetPosition();
        distance = static_cast<float>(DistanceFrom(targetPosition));
    }

    const int32_t inRange = WeaponInRange(weaponIndex, distance);

    if ((MultiPlayer() == nullptr || MultiPlayer()->IsServer != 0) && inRange == 0)
    {
        return 4;
    }

    const MCMasterComponent& weapon = MasterComponentList[Inventory[weaponIndex].MasterID];

    if (weapon.MissileType != 2 && weapon.MissileType != 1 && weapon.MissileType != 3)
    {
        // Direct fire needs a clear line.
        if (target == nullptr)
        {
            if (targetPoint == nullptr || LineOfFire(*targetPoint) == 0)
            {
                return 4;
            }
        }
        else if (LineOfFire(target) == 0)
        {
            return 4;
        }
    }

    const int32_t numShots = GetWeaponShots(weaponIndex);

    if (numShots == 0)
    {
        return 4;
    }

    // No aimed missiles.
    if (aimLocation != -1 && weapon.Form == MCComponentForm::WeaponMissile)
    {
        return 4;
    }

    // Port fix: from here the original assumes a target (it dereferences it for the entry angle).
    if (target == nullptr)
    {
        return 4;
    }

    const float entryAngle = target->RelFacingTo(Position, -1);
    const int isStreak = weapon.WeaponFlags & 1;
    int32_t range = 0;
    int32_t hitChance =
        static_cast<int32_t>(CalcAttackChance(target, aimLocation, targetTime, weaponIndex, 0.0f, &range, nullptr));
    const int32_t hitRoll = RandomNumber(100);
    Pilot->NumSkillUses[SkillGunnery][1]++;
    int32_t hitLocation = -1;

    if (hitRoll < hitChance)
    {
        Pilot->NumSkillSuccesses[SkillGunnery][1]++;

        if (aimLocation != -1)
        {
            hitLocation = aimLocation;
        }
    }

    MCMechWarrior* targetPilot = nullptr;
    const MCObjectClass targetClass = target->ObjectClass;

    if (targetClass == MCObjectClass::BattleMech || targetClass == MCObjectClass::GroundVehicle ||
        targetClass == MCObjectClass::Elemental || targetClass == MCObjectClass::Mover)
    {
        targetPilot = target->GetPilot();
        targetPilot->UpdateAttackerStatus(static_cast<uint32_t>(PartId), ScenarioTime);
    }

    // Aimed shots only from a standing elemental.
    if (aimLocation != -1 && 0.0 < GetVelocity().Magnitude())
    {
        hitChance = 0;
    }

    StartWeaponRecycle(weaponIndex);

    MCInventoryItem& item = Inventory[weaponIndex];
    const auto fired = [&]() -> const MCMasterComponent& { return MasterComponentList[item.MasterID]; };
    const auto hotSpotOf = [&](int32_t location)
    {
        if (target->ObjectClass == MCObjectClass::BattleMech)
        {
            // Port fix: the original reads body[location], past the eight body locations for a rear torso hit
            // (8..10); the torso it maps to is read instead.
            const MCBodyLocation& body = static_cast<MCBattleMech*>(target)->BodyAt(MechArmorToBodyLocation[location]);
            return static_cast<int32_t>(body.HotSpotNumber);
        }

        return 0;
    };

    std::unique_ptr<MCGameObject> fx;

    if (hitRoll < hitChance)
    {
        if (numShots != UNLIMITED_SHOTS)
        {
            DeductWeaponShot(weaponIndex, 1);
        }

        if (fired().Form == MCComponentForm::WeaponMissile)
        {
            // Missiles: a streak fires them all, anything else about half; anti-missile systems take some out.
            // The rest fly in volleys, each with its own hit location.
            int32_t missiles = fired().NumMissiles;

            if (isStreak == 0)
            {
                missiles = static_cast<int32_t>(missiles * 0.5 + 0.5);
            }

            int32_t antiMissileShots = 0;
            missiles = target->FireAntiMissileSystem(missiles, antiMissileShots);

            if (antiMissileShots > 0)
            {
                target->ReduceAntiMissileAmmo(antiMissileShots);
            }

            int32_t volleySize = 1;
            const int32_t numVolleys = MissileVolleys(fired(), missiles, volleySize);

            if (numVolleys != 0)
            {
                fx = CreateObject(static_cast<int32_t>(WeaponFXTable[fired().WeaponEffect]));
                int32_t targetHotSpot = 0;
                MCWeaponShotInfo shot;

                for (int32_t volley = 0; volley < numVolleys; volley++)
                {
                    if (missiles < volleySize)
                    {
                        volleySize = missiles;
                    }

                    missiles -= volleySize;

                    if (aimLocation == -1)
                    {
                        hitLocation = target->CalcHitLocation(this, weaponIndex, 0, attackType);
                    }

                    Assert(hitLocation != -1, 0, " Elemental.FireWeapon: Bad Hit Location ");

                    if (volley == 0)
                    {
                        targetHotSpot = hotSpotOf(hitLocation);
                    }

                    shot.Init(this, item.MasterID, fired().Damage * static_cast<float>(volleySize), hitLocation,
                              entryAngle);

                    if (fx->ObjectClass == MCObjectClass::Bullet)
                    {
                        AddBulletShot(static_cast<MCBullet*>(fx.get()), shot);
                    }
                }

                AimWeaponFX(this, fx.get(), target, shot, targetHotSpot);
            }
        }
        else
        {
            if (aimLocation == -1)
            {
                hitLocation = target->CalcHitLocation(this, weaponIndex, 0, attackType);
            }

            Assert(hitLocation != -1, 0, " Elemental.FireWeapon: Bad Hit Location ");
            MCWeaponShotInfo shot;
            shot.Init(this, item.MasterID, fired().Damage, hitLocation, entryAngle);
            fx = CreateObject(static_cast<int32_t>(WeaponFXTable[fired().WeaponEffect]));

            if (fx->ObjectClass == MCObjectClass::Bullet)
            {
                AddBulletShot(static_cast<MCBullet*>(fx.get()), shot);
            }

            AimWeaponFX(this, fx.get(), target, shot, hotSpotOf(hitLocation));
        }
    }
    else if (isStreak == 0)
    {
        // A miss (a streak doesn't fire without a lock): the shot lands somewhere near.
        if (numShots != UNLIMITED_SHOTS)
        {
            DeductWeaponShot(weaponIndex, 1);
        }

        MCWeaponShotInfo shot;

        if (fired().Form == MCComponentForm::WeaponMissile)
        {
            int32_t missiles = static_cast<int32_t>(fired().NumMissiles * 0.5 + 0.5);
            int32_t volleySize = 1;
            const int32_t numVolleys = MissileVolleys(fired(), missiles, volleySize);

            if (numVolleys != 0)
            {
                fx = CreateObject(static_cast<int32_t>(WeaponFXTable[fired().WeaponEffect]));

                for (int32_t volley = 0; volley < numVolleys; volley++)
                {
                    if (missiles < volleySize)
                    {
                        volleySize = missiles;
                    }

                    missiles -= volleySize;
                    shot.Init(this, item.MasterID, fired().Damage * static_cast<float>(volleySize), -1, entryAngle);

                    if (fx->ObjectClass == MCObjectClass::Bullet)
                    {
                        AddBulletShot(static_cast<MCBullet*>(fx.get()), shot);
                    }
                }

                MCVector3D landing = MissPoint(target, 1);
                ConnectMissFX(this, fx.get(), landing, shot);
            }
        }
        else
        {
            shot.Init(this, item.MasterID, fired().Damage, -1, entryAngle);
            fx = CreateObject(static_cast<int32_t>(WeaponFXTable[fired().WeaponEffect]));
            MCVector3D landing = MissPoint(target, 0);

            if (fx->ObjectClass == MCObjectClass::Bullet)
            {
                AddBulletShot(static_cast<MCBullet*>(fx.get()), shot);
            }

            ConnectMissFX(this, fx.get(), landing, shot);
        }
    }

    if (fx != nullptr)
    {
        WeaponList()->Add(std::move(fx));
    }

    if (targetPilot != nullptr)
    {
        targetPilot->TriggerAlarm(MCPilotAlarmType::TargetOfWeaponFire, static_cast<uint32_t>(PartId));
    }

    // Firing gives an unrevealed elemental away to the other side's mechs within visual range.
    MCObjectList* enemies = nullptr;
    uint8_t seenBy = 0;

    if (Alignment == 1 && IsRevealed() == 0)
    {
        enemies = ClanMechList();
        seenBy = 2;
    }
    else if (Alignment == -1 && IsRevealed() == 0)
    {
        enemies = InnerSphereMechList();
        seenBy = 1;
    }

    if (enemies != nullptr)
    {
        for (MCBaseObject* enemy : *enemies)
        {
            MCVector3D enemyPosition = static_cast<MCGameObject*>(enemy)->GetPosition();

            if (DistanceFrom(enemyPosition) < Scenario()->MaxVisualRange)
            {
                Terrain()->MarkRadiusSeen(Position, Frame.J, 360.0f, Scenario()->FireVisualRange, seenBy);
                break;
            }
        }
    }

    if (Group != nullptr)
    {
        Group->HandleMateFiredWeapon(static_cast<uint32_t>(PartId));
    }

    return 0;
}
