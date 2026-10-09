#include "stdafx.h"
#include "object/MCGroundVehicle.h"
#include "object/MCGroundVehicleGameSystem.h"
#include "ai/MCTacticalOrder.h"
#include "camera/MCCamera.h"
#include "gui/MCGuiSystem.h"
#include "iface/MCTacticalInterface.h"
#include "lib/MCFatal.h"
#include "main/main.h"
#include "mission/MCScenario.h"
#include "network/multplyr.h"
#include "object/MCAIControl.h"
#include "object/MCArtillery.h"
#include "object/MCArtilleryType.h"
#include "object/MCArtilleryChunk.h"
#include "object/MCCameraDrone.h"
#include "object/MCCameraDroneType.h"
#include "object/MCBuilding.h"
#include "object/MCBuildingType.h"
#include "object/MCBuildingMarines.h"
#include "object/MCObjectDrawing.h"
#include "object/MCMiscTerrainObject.h"
#include "object/MCMiscTerrainObjectType.h"
#include "object/MCBullet.h"
#include "object/MCBulletType.h"
#include "object/MCMasterComponent.h"
#include "object/MCCollisionSystem.h"
#include "object/MCExplosion.h"
#include "object/MCExplosionType.h"
#include "object/MCMoverGroup.h"
#include "object/MCJet.h"
#include "object/MCJetType.h"
#include "object/MCLaser.h"
#include "object/MCLaserType.h"
#include "object/MCBattleMech.h"
#include "object/MCMechGameSystem.h"
#include "object/MCProjectileLaser.h"
#include "object/MCProjectileLaserType.h"
#include "object/MCNetControl.h"
#include "object/MCObjectSystem.h"
#include "object/MCSmoke.h"
#include "object/MCSmokeType.h"
#include "object/MCEffectSystem.h"
#include "object/MCMechWarrior.h"
#include "sound/MCSoundSystem.h"
#include "terrain/MCTerrain.h"
#include "object/MCObjectType.h"
#include "object/MCWeaponChunkDebug.h"
#include "object/MCWeaponFireChunk.h"
#include "object/MCWeaponShotInfo.h"

auto MCGroundVehicle::CalcAttackChance(MCGameObject* target, int32_t aimLocation, float targetTime, int32_t weaponIndex,
                                       float modifiers, int32_t* range, MCVector3D* targetPoint) -> float
{
    if (weaponIndex < NumOther || NumOther + NumWeapons <= weaponIndex)
    {
        return -1000.0f;
    }

    return MCMover::CalcAttackChance(target, aimLocation, targetTime, weaponIndex, modifiers, range, targetPoint);
}

auto MCGroundVehicle::CalcHitLocation(MCGameObject* attacker, int32_t weaponIndex, int32_t attackSource,
                                      int32_t attackType) -> int32_t
{
    if (attackSource == 2)
    {
        return GroundVehicleFront;
    }

    // Nearly a third of hits land on the turret; the rest on the side facing the attacker.
    const int32_t roll = RandomNumber(100);

    if ((RandomNumber(100) + roll) / 2 > 69)
    {
        return GroundVehicleTurret;
    }

    float facing = 0.0f;

    if (attacker != nullptr)
    {
        facing = RelFacingTo(attacker->GetPosition(), -1);
    }

    if (-45.0 <= facing && facing <= 45.0)
    {
        return GroundVehicleFront;
    }

    if (-135.0 < facing && facing < -45.0)
    {
        return GroundVehicleLeft;
    }

    if (45.0 < facing && facing < 135.0f)
    {
        return GroundVehicleRight;
    }

    return GroundVehicleRear;
}

auto MCGroundVehicle::HitInventoryItem(int32_t itemIndex, int setupOnly) -> int
{
    Fatal(0, " Vehicles should never suffer inventory item hit ");
    return 0;
}

auto MCGroundVehicle::DestroyBodyLocation(int32_t location) -> void
{
}

auto MCGroundVehicle::CalcCriticalHitV(int32_t& hitLocation) -> int
{
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        return 0;
    }

    int32_t roll = RandomNumber(100);
    hitLocation = 0;

    do
    {
        if (roll < GroundVehicleCriticalHitTable[hitLocation])
        {
            break;
        }

        roll -= GroundVehicleCriticalHitTable[hitLocation];
        hitLocation++;
    } while (hitLocation < 11);

    if (MPlayer != nullptr)
    {
        AddCriticalHitChunk(0, 0, hitLocation);
    }

    switch (hitLocation)
    {
        case 1:
        case 2:
            return 1;
        case 3:
        {
            // The crew is hurt.
            Pilot->Injure(6.0f, 1);
            return 0;
        }
        case 4:
        {
            // The engine is knocked out.
            Inventory[Engine].Health = 0;
            Inventory[Engine].Disabled = 1;
            MovementEnabled = 0;
            return 0;
        }
        case 5:
        {
            // The first weapon jams for ten seconds.
            if (Inventory[NumOther].ReadyTime < ScenarioTime)
            {
                StartWeaponRecycle(NumOther);
            }

            Inventory[NumOther].ReadyTime += 10.0f;
            return 0;
        }
        case 7:
        {
            MovementEnabled = 0;
            return 0;
        }
        case 9:
        {
            // Only chassis 2 takes this one; for the others it is no hit.
            if (Chassis != 2)
            {
                hitLocation = 0;
                return 0;
            }

            [[fallthrough]];
        }
        case 8:
        {
            // Drive damage: 10 off the top speed, immobile at 0.
            MaxRunSpeed -= 10.0f;

            if (MaxRunSpeed <= 0.0f)
            {
                MaxRunSpeed = 0.0f;
                MovementEnabled = 0;
            }

            return 0;
        }
        case 10:
        {
            TurretEnabled = 0;
            return 0;
        }
        default:
            return 0;
    }
}

auto MCGroundVehicle::InjureBodyLocation(int32_t bodyLocation, float damage) -> int
{
    MCBodyLocation& location = BodyAt(bodyLocation);

    if (location.CurInternalStructure <= damage)
    {
        location.CurInternalStructure = 0.0f;
        return 1;
    }

    location.CurInternalStructure -= damage;
    const float structureLeft = location.CurInternalStructure / static_cast<float>(location.MaxInternalStructure);

    if (structureLeft == 0.0)
    {
        location.DamageState = 2;
    }
    else if (structureLeft <= 0.5)
    {
        location.DamageState = 1;
    }
    else
    {
        location.DamageState = 0;
    }

    return 0;
}

auto MCGroundVehicle::WeaponLocked(int32_t weaponIndex, MCVector3D targetPosition) -> float
{
    return RelFacingTo(targetPosition, GroundVehicleTurret);
}

namespace
{
    /// <summary>Whether <paramref name="object"/> is a mover (mech, vehicle, elemental or plain mover).</summary>
    bool IsMoverClass(const MCGameObject* object)
    {
        const MCObjectClass objectClass = object->ObjectClass;
        return objectClass == MCObjectClass::BattleMech || objectClass == MCObjectClass::GroundVehicle ||
               objectClass == MCObjectClass::Elemental || objectClass == MCObjectClass::Mover;
    }
}

auto MCGroundVehicle::HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if ((MPlayer == nullptr && CantHitMe != 0 && Pilot->OnHomeTeam() != 0) || shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->AddWeaponHitChunk(this, shotInfo, 0);
    }

    if (shotInfo->Damage <= 0.0f)
    {
        return 0;
    }

    if (IsDestroyed() != 0)
    {
        return 0;
    }

    const int32_t hitLocation = shotInfo->HitLocation;
    const MCWeaponShotInfo originalShot = *shotInfo;

    if (hitLocation < 0 || hitLocation > 4)
    {
        MCGameObject* attacker = shotInfo->Attacker;
        std::string attackerName;

        if (attacker == nullptr)
        {
            attackerName = "attacker?";
        }
        else if (IsMoverClass(attacker))
        {
            attackerName = static_cast<MCMover*>(attacker)->DebugStatus;
        }
        else
        {
            attackerName = std::format("ID:{}", attacker->PartId);
        }

        Fatal(0, std::format("GVehicle.handleWeaponHit: [{}]{} for {:.2f} at {}", attackerName, shotInfo->MasterId,
                             shotInfo->Damage, hitLocation));
    }

    MCArmorLocation& hitArmor = Armor[hitLocation];

    if (hitArmor.CurArmor > 0.0f)
    {
        if (shotInfo->Damage <= hitArmor.CurArmor)
        {
            hitArmor.CurArmor -= shotInfo->Damage;
            shotInfo->SetDamage(0.0f);
        }
        else
        {
            shotInfo->SetDamage(shotInfo->Damage - hitArmor.CurArmor);
            Armor[shotInfo->HitLocation].CurArmor = 0.0f;
        }

        // A sweeper sweeps with its front: a hit there ends that.
        if (shotInfo->HitLocation == GroundVehicleFront)
        {
            MineSweeper = 0;
        }
    }

    const int wasDisabled = IsDisabled();

    if (shotInfo->Damage > 0.0f && InjureBodyLocation(hitLocation, shotInfo->Damage) != 0)
    {
        Pilot->HandleOwnVehicleIncapacitation(0);
        ObjType->HandleDestruction(this, nullptr);
    }

    CurCV = CalcCV(0);

    MCGameObject* attacker = shotInfo->Attacker;

    if (wasDisabled == 0 && IsDisabled() != 0)
    {
        // The attacker's pilot hears of the kill.
        if (attacker != nullptr && IsMoverClass(attacker))
        {
            attacker->GetPilot()->TriggerAlarm(MCPilotAlarmType::KilledTarget, PartId);
        }
    }
    else if (attacker != nullptr)
    {
        Pilot->TriggerAlarm(shotInfo->MasterId > -1 || shotInfo->MasterId == -4 ? MCPilotAlarmType::HitByWeaponFire
                                                                                : MCPilotAlarmType::Collision,
                            attacker->PartId);
    }
    else
    {
        Pilot->TriggerAlarm(MCPilotAlarmType::HitByWeaponFire, shotInfo->MasterId == -4 || shotInfo->MasterId >= 0
                                                                   ? 0
                                                                   : static_cast<uint32_t>(shotInfo->MasterId));
    }

    shotInfo->Init(originalShot.Attacker, originalShot.MasterId, originalShot.Damage, originalShot.HitLocation,
                   originalShot.EntryAngle);
    return 0;
}

namespace
{
    /// <summary>Ammo count that marks a weapon as never running out.</summary>
    constexpr int32_t UNLIMITED_SHOTS = 9999;

    /// <summary>
    /// Builds, packs and checks the chunk for a shot at <paramref name="target"/> (a mover, train car, camera drone
    /// or terrain object) or, when it is null, at <paramref name="point"/>; then queues and logs it.
    /// </summary>
    void SendTargetFireChunk(MCGroundVehicle* vehicle, MCGameObject* target, MCVector3D* point, int32_t weapon, int hit,
                             float entryAngle, int32_t missiles, int32_t missilesPastAMS, int32_t antiMissileShots,
                             int32_t hitLocation, const char* badChunkMessage)
    {
        MCWeaponFireChunk chunk;
        chunk.Init();
        auto* bigTarget = static_cast<MCBigGameObject*>(target);

        if (target == nullptr)
        {
            chunk.BuildLocationTarget(*point, weapon, hit, missiles);
        }
        else if (IsMoverClass(target))
        {
            chunk.BuildMoverTarget(bigTarget, weapon, hit, entryAngle, missiles, missilesPastAMS, antiMissileShots,
                                   hitLocation);
        }
        else if (target->ObjectClass == MCObjectClass::TrainCar)
        {
            chunk.BuildTrainTarget(bigTarget, weapon, hit, entryAngle, missiles);
        }
        else if (target->ObjectClass == MCObjectClass::CameraDrone)
        {
            chunk.BuildCameraDroneTarget(bigTarget, weapon, hit, entryAngle, missiles);
        }
        else
        {
            chunk.BuildTerrainTarget(bigTarget, weapon, hit, missiles);
        }

        chunk.Pack();
        MCWeaponFireChunk check;
        check.Init();
        check.Data = chunk.Data;
        check.Unpack(vehicle);

        if (chunk.EqualTo(&check) == 0)
        {
            Fatal(0, badChunkMessage);
        }

        vehicle->AddWeaponFireChunk(0, &chunk);
    }

    /// <summary>
    /// Sends a weapon effect on its way from the vehicle, at <paramref name="target"/> or, when it is null, at
    /// <paramref name="point"/>, carrying <paramref name="shot"/>; then adds it to the weapon list. Vehicles fire from
    /// hot spot 0.
    /// </summary>
    void LaunchWeaponFX(MCGroundVehicle* vehicle, std::unique_ptr<MCGameObject> fx, MCGameObject* target,
                        MCVector3D* point, MCWeaponShotInfo& shot, int32_t targetHotSpot)
    {
        if (fx->ObjectClass == MCObjectClass::Bullet)
        {
            auto* bullet = static_cast<MCBullet*>(fx.get());

            bullet->NewShot().Init(shot.Attacker, shot.MasterId, shot.Damage, shot.HitLocation, shot.EntryAngle);

            if (target == nullptr)
            {
                bullet->Connect(vehicle, *point, 0);
            }
            else
            {
                bullet->Owner = vehicle;
                bullet->Target = target;
                bullet->OwnerHotSpot = 0;
                bullet->TargetHotSpot = targetHotSpot;
            }
        }
        else if (fx->ObjectClass == MCObjectClass::Laser)
        {
            auto* laser = static_cast<MCLaser*>(fx.get());

            if (target == nullptr)
            {
                laser->Connect(vehicle, *point, &shot, 0);
            }
            else
            {
                laser->Source.SetWatcher(vehicle);
                laser->Target.SetWatcher(target);
                laser->SourceHotSpot = 0;
                laser->TargetHotSpot = targetHotSpot;
                laser->ShotInfo.Init(shot.Attacker, shot.MasterId, shot.Damage, shot.HitLocation, shot.EntryAngle);
            }
        }
        else
        {
            auto* projectile = static_cast<MCProjectileLaser*>(fx.get());

            if (target == nullptr)
            {
                projectile->Connect(vehicle, *point, &shot, 0);
            }
            else
            {
                projectile->Owner = vehicle;
                projectile->Target = target;
                projectile->OwnerHotSpot = 0;
                projectile->TargetHotSpot = targetHotSpot;
                projectile->ShotInfo.Init(shot.Attacker, shot.MasterId, shot.Damage, shot.HitLocation, shot.EntryAngle);
            }
        }

        WeaponList()->Add(std::move(fx));
    }

    /// <summary>Makes a weapon's effect object (Fatal when it can't).</summary>
    std::unique_ptr<MCGameObject> CreateWeaponFX(const MCMasterComponent& weapon)
    {
        std::unique_ptr<MCGameObject> fx = CreateObject(static_cast<int32_t>(WeaponFXTable[weapon.WeaponEffect]));

        if (fx == nullptr)
        {
            Fatal(-1, " couldnt create weapon FX ");
        }

        return fx;
    }

    /// <summary>The hot spot of the hit location, on a mech target; 0 otherwise.</summary>
    int32_t TargetHotSpotOf(MCGameObject* target, int32_t hitLocation)
    {
        if (target != nullptr && target->ObjectClass == MCObjectClass::BattleMech)
        {
            // Port fix: the original reads body[hitLocation], past the eight body locations for a rear torso hit
            // (8..10); the torso it maps to is read instead.
            return static_cast<MCBattleMech*>(target)->BodyAt(MechArmorToBodyLocation[hitLocation]).HotSpotNumber;
        }

        return 0;
    }

    /// <summary>Firing gives a vehicle away to the other side's mechs within visual range.</summary>
    void RevealFiring(MCGroundVehicle* vehicle)
    {
        MCObjectList* enemies = nullptr;
        uint8_t seenBy = 0;

        if (vehicle->Alignment == 1)
        {
            enemies = ClanMechList();
            seenBy = 2;
        }
        else if (vehicle->Alignment == -1)
        {
            enemies = InnerSphereMechList();
            seenBy = 1;
        }

        if (enemies == nullptr)
        {
            return;
        }

        for (MCBaseObject* enemy : *enemies)
        {
            MCVector3D enemyPosition = static_cast<MCGameObject*>(enemy)->GetPosition();

            if (vehicle->DistanceFrom(enemyPosition) < Scenario()->MaxVisualRange)
            {
                Terrain()->MarkRadiusSeen(vehicle->Position, vehicle->Frame.J, 360.0f, Scenario()->FireVisualRange,
                                          seenBy);
                return;
            }
        }
    }

    /// <summary>Where a missed shot lands: scattered up to <paramref name="scatter"/> about the aim point.</summary>
    /// <param name="centred">Missiles scatter both ways; other shots (as the original computes them) only one.</param>
    MCVector3D MissPoint(MCGameObject* target, MCVector3D* targetPoint, float scatter, int centred)
    {
        MCVector3D miss;
        miss.X = scatter;
        miss.Y = scatter;
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
        const MCVector3D base = target != nullptr ? target->GetPosition() : *targetPoint;
        miss.X += base.X;
        miss.Y += base.Y;
        miss.Z += base.Z;
        return miss;
    }
}

auto MCGroundVehicle::FireWeapon(MCGameObject* target, float targetTime, int32_t weaponIndex, int32_t attackType,
                                 int32_t aimLocation, MCVector3D* targetPoint) -> int32_t
{
    if (Status != 0)
    {
        return 1;
    }

    if (IsWeaponIndex(weaponIndex) == 0)
    {
        return 2;
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

    if ((MPlayer == nullptr || MPlayer->IsServer != 0) && inRange == 0)
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

    // A pop-up turret fires only once it is up.
    if (WeaponsDeployed == 0)
    {
        return 5;
    }

    const int32_t numShots = GetWeaponShots(weaponIndex);

    if (numShots == 0)
    {
        return 4;
    }

    MCMechWarrior* targetPilot = nullptr;

    if (target != nullptr && IsMoverClass(target))
    {
        targetPilot = target->GetPilot();
        targetPilot->UpdateAttackerStatus(static_cast<uint32_t>(PartId), ScenarioTime);
    }

    float entryAngle = 0.0f;

    if (target != nullptr)
    {
        entryAngle = target->RelFacingTo(Position, -1);
    }

    const int isStreak = weapon.WeaponFlags & 1;
    int32_t range = 0;
    int32_t hitChance =
        static_cast<int32_t>(CalcAttackChance(target, aimLocation, targetTime, weaponIndex, 0.0f, &range, targetPoint));
    const int32_t hitRoll = RandomNumber(100);

    if (target != nullptr && target->GetAlignment() == -1)
    {
        Pilot->NumSkillUses[SkillGunnery][1]++;
    }

    // Aimed shots only from a standing vehicle.
    if (aimLocation != -1 && 0.0 < GetVelocity().Magnitude())
    {
        hitChance = 0;
    }

    int32_t hitLocation = -1;

    if (target != nullptr && hitRoll < hitChance)
    {
        if (target->GetAlignment() == -1)
        {
            Pilot->NumSkillSuccesses[SkillGunnery][1]++;
        }

        if (aimLocation != -1)
        {
            hitLocation = aimLocation;
        }
    }

    StartWeaponRecycle(weaponIndex);

    const int32_t chunkWeapon = weaponIndex - NumOther;
    const char* const badChunk = " GVehicle.fireWeapon: Bad WeaponFireChunk (save wfchunk.dbg file now) ";
    const char* const badMissChunk = " GVehicl.fireWeapon: Bad WeaponFireChunk (save wfchunk.dbg file now) ";

    if (hitRoll < hitChance)
    {
        if (numShots != UNLIMITED_SHOTS)
        {
            DeductWeaponShot(weaponIndex, 1);
        }

        MCInventoryItem& item = Inventory[weaponIndex];
        const MCMasterComponent& fired = MasterComponentList[item.MasterID];

        if (fired.Form == MCComponentForm::WeaponMissile)
        {
            // Missiles: a streak fires them all, anything else about half; anti-missile systems take some out.
            const int32_t rackSize = fired.NumMissiles;
            int32_t missiles = rackSize;

            if (isStreak == 0)
            {
                missiles = static_cast<int32_t>((rackSize + 1.0) * 0.5);

                if (missiles < 1)
                {
                    missiles = 1;
                }

                if (rackSize < missiles)
                {
                    missiles = rackSize;
                }
            }

            int32_t antiMissileShots = 0;
            int32_t missilesLeft = missiles;

            if (target != nullptr)
            {
                missilesLeft = target->FireAntiMissileSystem(missiles, antiMissileShots);

                if (antiMissileShots > 0)
                {
                    target->ReduceAntiMissileAmmo(antiMissileShots);
                }
            }

            if (missilesLeft != 0)
            {
                std::unique_ptr<MCGameObject> fx = CreateWeaponFX(fired);
                int32_t targetHotSpot = 0;

                if (target == nullptr)
                {
                    hitLocation = -1;
                }
                else
                {
                    if (aimLocation == -1)
                    {
                        hitLocation = target->CalcHitLocation(this, weaponIndex, 0, attackType);
                    }

                    targetHotSpot = TargetHotSpotOf(target, hitLocation);
                }

                Assert(hitLocation != -2 ? 1 : 0, 0, " GroundVehicle.FireWeapon: Bad Hit Location ");
                MCWeaponShotInfo shot;
                shot.Init(this, item.MasterID, fired.Damage * static_cast<float>(missilesLeft), hitLocation,
                          entryAngle);

                if (MPlayer != nullptr && MPlayer->IsServer != 0)
                {
                    SendTargetFireChunk(this, target, targetPoint, chunkWeapon, 1, entryAngle, missiles, missilesLeft,
                                        antiMissileShots, hitLocation, badChunk);
                }

                LaunchWeaponFX(this, std::move(fx), target, targetPoint, shot, targetHotSpot);

                if (target == nullptr)
                {
                    Pilot->ClearCurTacOrder(1, 0);
                }
            }
        }
        else
        {
            if (target == nullptr)
            {
                hitLocation = -1;
            }
            else if (aimLocation == -1)
            {
                hitLocation = target->CalcHitLocation(this, weaponIndex, 0, attackType);
            }

            Assert(hitLocation != -2 ? 1 : 0, 0, " GroundVehicle.FireWeapon: Bad Hit Location ");
            MCWeaponShotInfo shot;
            shot.Init(this, item.MasterID, fired.Damage, hitLocation, entryAngle);

            if (MPlayer != nullptr && MPlayer->IsServer != 0)
            {
                SendTargetFireChunk(this, target, targetPoint, chunkWeapon, 1, entryAngle, 0, 0, 0, hitLocation,
                                    badChunk);
            }

            std::unique_ptr<MCGameObject> fx = CreateWeaponFX(fired);
            LaunchWeaponFX(this, std::move(fx), target, targetPoint, shot, TargetHotSpotOf(target, hitLocation));

            if (target == nullptr)
            {
                Pilot->ClearCurTacOrder(1, 0);
            }
        }
    }
    else if (isStreak == 0)
    {
        // A miss (a streak doesn't fire without a lock): the shot lands somewhere near.
        if (numShots != UNLIMITED_SHOTS)
        {
            DeductWeaponShot(weaponIndex, 1);
        }

        MCInventoryItem& item = Inventory[weaponIndex];
        const MCMasterComponent& fired = MasterComponentList[item.MasterID];
        const float scatter = target != nullptr ? 25.0f : 5.0f;

        if (fired.Form == MCComponentForm::WeaponMissile)
        {
            const int32_t rackSize = fired.NumMissiles;
            int32_t missiles = static_cast<int32_t>(rackSize * 0.5 + 0.5);

            if (missiles < 1)
            {
                missiles = 1;
            }

            if (rackSize < missiles)
            {
                missiles = rackSize;
            }

            if (missiles != 0)
            {
                std::unique_ptr<MCGameObject> fx = CreateWeaponFX(fired);
                MCWeaponShotInfo shot;
                shot.Init(this, item.MasterID, fired.Damage * static_cast<float>(missiles), -1, entryAngle);
                MCVector3D landing = MissPoint(target, targetPoint, scatter, 1);

                if (MPlayer != nullptr && MPlayer->IsServer != 0)
                {
                    SendTargetFireChunk(this, nullptr, &landing, chunkWeapon, 0, 0.0f, missiles, 0, 0, 0, badMissChunk);
                }

                LaunchWeaponFX(this, std::move(fx), nullptr, &landing, shot, 0);
            }
        }
        else
        {
            MCWeaponShotInfo shot;
            shot.Init(this, item.MasterID, fired.Damage, -1, entryAngle);
            std::unique_ptr<MCGameObject> fx = CreateWeaponFX(fired);
            MCVector3D landing = MissPoint(target, targetPoint, scatter, 0);

            if (MPlayer != nullptr && MPlayer->IsServer != 0)
            {
                SendTargetFireChunk(this, nullptr, &landing, chunkWeapon, 0, 0.0f, 0, 0, 0, 0, badMissChunk);
            }

            LaunchWeaponFX(this, std::move(fx), nullptr, &landing, shot, 0);
        }
    }

    if (targetPilot != nullptr)
    {
        targetPilot->TriggerAlarm(MCPilotAlarmType::TargetOfWeaponFire, static_cast<uint32_t>(PartId));
    }

    RevealFiring(this);

    if (Group != nullptr)
    {
        Group->HandleMateFiredWeapon(static_cast<uint32_t>(PartId));
    }

    return 0;
}

auto MCGroundVehicle::HandleWeaponFire(int32_t weaponIndex, MCGameObject* target, MCVector3D* targetPoint, int hit,
                                       float entryAngle, int32_t numMissiles, int32_t missilesPastAMS,
                                       int32_t antiMissileShots, int32_t hitLocation) -> int32_t
{
    const int32_t numShots = GetWeaponShots(weaponIndex);
    StartWeaponRecycle(weaponIndex);
    MCInventoryItem& item = Inventory[weaponIndex];
    const MCMasterComponent& fired = MasterComponentList[item.MasterID];
    const int isStreak = fired.WeaponFlags & 1;
    MCWeaponShotInfo shot;

    if (hit == 0)
    {
        Assert(target == nullptr ? 1 : 0, 0, " GVehicl.handleWeaponFire: target should be NULL with network miss! ");
        Assert(targetPoint != nullptr ? 1 : 0, 0,
               " GVehicl.handleWeaponFire: MUST have targetpoint with network miss! ");

        if (isStreak != 0)
        {
            CurMoverWeaponFireChunk.Unpack(this);
            DebugWeaponFireChunk(&CurMoverWeaponFireChunk, nullptr, this);
            Assert(0, 0, " GVehicl.handleWeaponFire: streaks shouldn't miss! ");
        }

        if (numShots != UNLIMITED_SHOTS)
        {
            DeductWeaponShot(weaponIndex, 1);
        }

        if (fired.Form == MCComponentForm::WeaponMissile)
        {
            if (numMissiles > 0)
            {
                std::unique_ptr<MCGameObject> fx = CreateWeaponFX(fired);
                shot.Init(this, item.MasterID, fired.Damage * static_cast<float>(numMissiles), -1, entryAngle);
                LaunchWeaponFX(this, std::move(fx), nullptr, targetPoint, shot, 0);
            }
        }
        else
        {
            shot.Init(this, item.MasterID, fired.Damage, -1, entryAngle);
            std::unique_ptr<MCGameObject> fx = CreateWeaponFX(fired);
            LaunchWeaponFX(this, std::move(fx), nullptr, targetPoint, shot, 0);
        }
    }
    else
    {
        if (numShots != UNLIMITED_SHOTS)
        {
            DeductWeaponShot(weaponIndex, 1);
        }

        if (fired.Form == MCComponentForm::WeaponMissile)
        {
            if (antiMissileShots > 0)
            {
                target->ReduceAntiMissileAmmo(antiMissileShots);
            }

            if (missilesPastAMS != 0)
            {
                std::unique_ptr<MCGameObject> fx = CreateWeaponFX(fired);
                Assert(hitLocation != -2 ? 1 : 0, static_cast<uint32_t>(TargetRolo),
                       " GroundVehicle.handleWeaponFire: Bad Hit Location ");
                const int32_t targetHotSpot = TargetHotSpotOf(target, hitLocation);
                shot.Init(this, item.MasterID, fired.Damage * static_cast<float>(missilesPastAMS), hitLocation,
                          entryAngle);
                LaunchWeaponFX(this, std::move(fx), target, targetPoint, shot, targetHotSpot);

                if (target == nullptr)
                {
                    Pilot->ClearCurTacOrder(1, 0);
                }
            }
        }
        else
        {
            shot.Init(this, item.MasterID, fired.Damage, hitLocation, entryAngle);
            std::unique_ptr<MCGameObject> fx = CreateWeaponFX(fired);
            LaunchWeaponFX(this, std::move(fx), target, targetPoint, shot, TargetHotSpotOf(target, hitLocation));

            if (target == nullptr)
            {
                Pilot->ClearCurTacOrder(1, 0);
            }
        }
    }

    if (target != nullptr && IsMoverClass(target))
    {
        MCMechWarrior* targetPilot = target->GetPilot();
        targetPilot->UpdateAttackerStatus(static_cast<uint32_t>(PartId), ScenarioTime);
        targetPilot->TriggerAlarm(MCPilotAlarmType::TargetOfWeaponFire, static_cast<uint32_t>(PartId));
    }

    RevealFiring(this);

    if (Group != nullptr)
    {
        Group->HandleMateFiredWeapon(static_cast<uint32_t>(PartId));
    }

    return 0;
}
