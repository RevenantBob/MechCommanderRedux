#include "stdafx.h"
#include "object/elemntl.h"
#include "ai/MCMoveSystem.h"
#include "ai/MCTacticalOrder.h"
#include "appear/MCAppearanceType.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/MCCamera.h"
#include "camera/MCCameraList.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCLineElement.h"
#include "engine/MCVfxElement.h"
#include "engine/MCCraterManager.h"
#include "vfx/MCVfxFunctions.h"
#include "iface/iface.h"
#include "lib/MCFatal.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFitIniFile.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/aictrl.h"
#include "object/artlry.h"
#include "object/bullet.h"
#include "object/MCMasterComponent.h"
#include "object/MCContactSystem.h"
#include "object/elemctrl.h"
#include "object/elemdyn.h"
#include "object/MCMoverGroup.h"
#include "object/gvehicl.h"
#include "object/laser.h"
#include "object/mech.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectQueue.h"
#include "object/plyrctrl.h"
#include "object/prjlase.h"
#include "object/MCForces.h"
#include "object/warrior.h"
#include "sound/soundsys.h"
#include "sprite/MCElementalActor.h"
#include "terrain/MCTerrain.h"
#include "object/MCObjectType.h"
#include "object/MCWeaponShotInfo.h"

float ElmDamageOnImpact = 0.0f;
float ElementalTargetNoJumpDistance = 75.0f;
int UseOldProject = 0;

namespace
{
    /// <summary>Half pi, as MCX.EXE stores it.</summary>
    constexpr double HALF_PI = 0x1.921fb5443e88cp+0;
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;
    /// <summary>getWeaponShots' answer for a weapon that needs no ammo.</summary>
    constexpr int32_t UNLIMITED_SHOTS = 9999;
    /// <summary>How far (world units) a missed shot lands from the target, either way.</summary>
    constexpr float MISS_SCATTER = 25.0f;

    /// <summary>Turns a frame about its k axis (MC2's inline frame_of_ref::rotate_about_k).</summary>
    void RotateAboutK(MCFrameOfRef& frame, float s, float c)
    {
        const MCVector3D oldI = frame.I;
        frame.I = frame.I * c + frame.J * s;
        frame.J = frame.J * c - oldI * s;
    }

    /// <summary>Starts a collision's grace period: no more from <paramref name="collider"/> for two seconds.
    /// </summary>
    /// <returns>0 while the last one from <paramref name="collider"/> is still in its grace period.</returns>
    int StartCollision(MCGameObject* collidee, MCGameObject* collider)
    {
        if (collidee->GetCollisionFreeFrom() == collider && ScenarioTime <= collidee->GetCollisionFreeTime())
        {
            return 0;
        }

        collidee->SetCollisionFreeFrom(collider);
        collidee->SetCollisionFreeTime(ScenarioTime + 2.0f);
        return 1;
    }

    /// <summary>Turns <paramref name="collidee"/> by <paramref name="radians"/>.</summary>
    void TurnAway(MCGameObject* collidee, double radians)
    {
        MCFrameOfRef frame = collidee->GetFrame();
        RotateAboutK(frame, static_cast<float>(std::sin(radians)), static_cast<float>(std::cos(radians)));
        collidee->SetFrame(frame);
    }

    /// <summary>A marine's death: it is taken off the interface at once, leaving no wreck.</summary>
    /// <param name="deathTime">The death timer to set (0.8 when it goes quietly, 0 when shot).</param>
    void RemoveMarine(MCElemental* marine, float deathTime)
    {
        marine->DeathTimer = deathTime;
        marine->GetPilot()->TriggerAlarm(7, 0);
        marine->Status = 2;
        marine->DeathExplosionDone = 0;
        TheInterface->RemoveMech(marine->PartId);
    }

    /// <summary>Adds a shot to a bullet, when it has room (5 at most).</summary>
    void AddBulletShot(MCBullet* bullet, MCWeaponShotInfo& shot)
    {
        if (bullet->NumShots != 5)
        {
            bullet->ShotInfo[bullet->NumShots++].Init(shot.Attacker, shot.MasterId, shot.Damage, shot.HitLocation,
                                                      shot.EntryAngle);
        }
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

    /// <summary>A turn of <paramref name="turn"/> degrees as a rotate request, no faster than
    /// <paramref name="maxTurn"/>.</summary>
    int8_t RotateRequest(float turn, float maxTurn)
    {
        return static_cast<int8_t>(static_cast<int32_t>(turn / maxTurn * 64.0f));
    }
}

auto LoadElementalGameSystem(MCFitIniFile* sysFile) -> int32_t
{
    int32_t result = sysFile->SeekBlock("Elemental:Collision");

    if (result != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdFloat("DamageOnImpact", ElmDamageOnImpact)) != 0)
    {
        return result;
    }

    if ((result = sysFile->SeekBlock("Elemental:Combat")) != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("NoJumpRange", ElementalTargetNoJumpDistance);
    Assert(result == 0 ? 1 : 0, 0, " Unable to find Elemental NoJumpRange in gamesys.fit ");
    return 0;
}

//---------------------------------------------------------------------------
// ElementalType
//---------------------------------------------------------------------------

auto MCElementalType::Init() -> void
{
    CanJump = 1;
    ElementalId = 0;
    Name.clear();
    Alignment = 0;
    MaxHealth = 0;
}

auto MCElementalType::Destroy() -> void
{
    Name.clear();
    delete DynamicsType;
    DynamicsType = nullptr;
    MCObjectType::Destroy();
}

auto MCElementalType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile elementalFile;
    int32_t result = elementalFile.Open(objFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if ((result = elementalFile.SeekBlock("Header")) != 0)
    {
        return result;
    }

    char fileType[128];

    if ((result = elementalFile.ReadIdString("FileType", fileType, 127)) != 0)
    {
        return result;
    }

    if (std::strcmp(fileType, "ElementalType") != 0)
    {
        return -1;
    }

    if ((result = elementalFile.SeekBlock("General")) != 0)
    {
        return result;
    }

    if ((result = elementalFile.ReadIdULong("ID", ElementalId)) != 0)
    {
        return result;
    }

    if (elementalFile.ReadIdBoolean("CanJump", CanJump) != 0)
    {
        CanJump = 1;
    }

    // "Type" 0 is 1, 1 is -1.
    static constexpr uint8_t alignmentMap[2] = {1, 0xff};
    uint8_t fileAlignment = 0;

    if ((result = elementalFile.ReadIdUChar("Type", fileAlignment)) != 0)
    {
        return result;
    }

    // Port fix: the original reads other values from past its two-entry table on the stack.
    Alignment = fileAlignment < 2 ? alignmentMap[fileAlignment] : 0;
    char nameBuffer[128];
    elementalFile.ReadIdString("Name", nameBuffer, 127);
    Name = nameBuffer;

    if ((result = elementalFile.ReadIdUChar("MaxHealth", MaxHealth)) != 0)
    {
        return result;
    }

    if ((result = elementalFile.SeekBlock("Dynamics")) != 0)
    {
        return result;
    }

    uint32_t dynamicsTypeId = 0;

    if ((result = elementalFile.ReadIdULong("Type", dynamicsTypeId)) != 0)
    {
        return result;
    }

    if (dynamicsTypeId != 3)
    {
        return -0x5fffd;
    }

    DynamicsType = new MCElementalDynamicsType;

    if (DynamicsType == nullptr)
    {
        return -0x5fffe;
    }

    if ((result = DynamicsType->Init(&elementalFile)) != 0)
    {
        return result;
    }

    return MCObjectType::Init(&elementalFile);
}

auto MCElementalType::HandleCollision(MCGameObject* collidee, MCGameObject* collider) -> int
{
    auto* elemental = static_cast<MCElemental*>(collidee);

    switch (collider->ObjectClass)
    {
        case MCObjectClass::BattleMech:
        case MCObjectClass::GroundVehicle:
        {
            // An enemy ramming it knocks an elemental aside; a marine is knocked aside by anything, every time.
            int knockedAside = 0;

            if (collidee->GetPilot()->Alignment != collider->GetPilot()->Alignment)
            {
                MCGameObject* collideeRamTarget = collidee->GetPilot()->CurTacOrder.GetRamTarget();
                MCGameObject* colliderRamTarget = collider->GetPilot()->CurTacOrder.GetRamTarget();

                if ((collideeRamTarget == collider || colliderRamTarget == collidee) &&
                    (collidee->GetCollisionFreeFrom() != collider || collidee->GetCollisionFreeTime() < ScenarioTime))
                {
                    knockedAside = 1;
                }
            }

            if (knockedAside == 0 && elemental->ElementalCanJump != 0)
            {
                return 0;
            }

            collidee->SetCollisionFreeFrom(collider);
            collidee->SetCollisionFreeTime(ScenarioTime + 2.0f);
            TurnAway(collidee, HALF_PI);
            collidee->GetVelocity();
            const float entryAngle = collidee->RelFacingTo(collider->GetPosition(), -1);
            MCWeaponShotInfo shotInfo;
            shotInfo.Init(collider, -1, 5.0f, 0, entryAngle);
            collidee->HandleWeaponHit(&shotInfo, 0);
            return 0;
        }

        case MCObjectClass::Building:
        case MCObjectClass::TreeBuilding:
        {
            if (StartCollision(collidee, collider) == 0)
            {
                return 0;
            }

            // A big building turns the elemental further.
            const float angle = ObjectCollisionThreshold < collider->GetObjectType()->ExtentRadius ? 135.0f : 45.0f;
            TurnAway(collidee, angle * DEGREES_TO_RADIANS);
            collidee->GetVelocity();
            const int32_t hitLocation = collidee->CalcHitLocation(collider, -1, 1, 0);
            const float entryAngle = collidee->RelFacingTo(collider->GetPosition(), -1);
            const auto damage = static_cast<int32_t>(collider->GetTonnage() * 0.1 + 0.5);
            MCWeaponShotInfo shotInfo;
            shotInfo.Init(collider, -1, static_cast<float>(damage), hitLocation, entryAngle);
            collidee->HandleWeaponHit(&shotInfo, 0);
            break;
        }

        case MCObjectClass::Tree:
        {
            if (StartCollision(collidee, collider) == 0)
            {
                return 0;
            }

            MCFrameOfRef frame = collidee->GetFrame();
            collider->GetObjectType();
            float deflection = 0.0f;

            if (collidee->GetTonnage() < TonnageCollisionThreshold)
            {
                deflection = static_cast<float>(static_cast<double>(TonnageCollisionThreshold) /
                                                collidee->GetTonnage() * TreeDeflection);
            }

            if (deflection > 0.0)
            {
                RotateAboutK(frame, static_cast<float>(std::sin(deflection * DEGREES_TO_RADIANS)),
                             static_cast<float>(std::cos(deflection * DEGREES_TO_RADIANS)));
                collidee->SetFrame(frame);
            }
            break;
        }

        case MCObjectClass::TrainCar:
        {
            if (collidee->GetCollisionFreeFrom() != collider || collidee->GetCollisionFreeTime() < ScenarioTime)
            {
                collidee->SetCollisionFreeFrom(collider);
                collidee->SetCollisionFreeTime(ScenarioTime + 2.0f);
                TurnAway(collidee, HALF_PI);
                collidee->GetVelocity();
            }

            return 0;
        }
        default:
            return 0;
    }

    SoundSystem->PlayDigitalSample(4, 1, collidee, 0, 0);
    return 0;
}

auto MCElementalType::HandleDestruction(MCGameObject* collidee, MCGameObject* collider) -> int
{
    auto* elemental = static_cast<MCElemental*>(collidee);

    if (elemental->GetPilot() == nullptr)
    {
        Fatal(0, " No Pilot in this elemental! ");
    }

    if (elemental->GetPoint() == elemental)
    {
        elemental->Group->SetPoint(nullptr);
    }

    if (elemental->SensorSystem != nullptr)
    {
        elemental->SensorSystem->Disable();
    }

    if (elemental->Withdrawing == 0)
    {
        elemental->DeathTimer = 0.8f;
        elemental->GetPilot()->TriggerAlarm(7, collider == nullptr ? 0 : collider->IdNumber);
    }
    else
    {
        elemental->DeathTimer = 0.0f;
        elemental->GetPilot()->TriggerAlarm(8, 0);
    }

    elemental->Status = 2;
    elemental->DeathExplosionDone = 0;
    TheInterface->RemoveMech(elemental->PartId);

    // Original behaviour (OB-003): the type's alignment (1 or 0xff) against the home team's (1 or -1), so a clan
    // home team never counts its own.
    if (static_cast<uint32_t>(Alignment) == static_cast<uint32_t>(HomeTeam()->Alignment))
    {
        FriendlyDestroyed = 1;
    }
    else
    {
        EnemyDestroyed = 1;
    }

    return 1;
}

auto MCElementalType::CreateInstance() -> std::unique_ptr<MCBaseObject>
{
    auto newElemental = std::make_unique<MCElemental>();

    if (newElemental == nullptr)
    {
        return nullptr;
    }

    if (newElemental->Init(this) != 0)
    {
        return nullptr;
    }

    newElemental->IdNumber = NextIdNumber++;
    return newElemental;
}

//---------------------------------------------------------------------------
// Elemental
//---------------------------------------------------------------------------

auto MCElemental::GetThrottle() -> int32_t
{
    return static_cast<MCElementalControlData*>(Control->ControlData)->Throttle;
}

auto MCElemental::Init() -> void
{
    ObjectClass = MCObjectClass::Elemental;
    JumpRange = 0.0f;
    JumpTime = -100.0f;
    InJump = 0;
    JumpGoal.Z = 0.0f;
    JumpGoal.Y = 0.0f;
    JumpGoal.X = 0.0f;
    MaxHealth = 11;
    CurHealth = 11;
    ElementalCanJump = 1;
    Transport = nullptr;
}

auto MCElemental::Init(MCObjectType* objType) -> int32_t
{
    int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    auto* elementalType = static_cast<MCElementalType*>(objType);
    Alignment = elementalType->Alignment;
    CollisionsOn = 1;
    MaxHealth = elementalType->MaxHealth;
    Control = nullptr;
    Dynamics = elementalType->DynamicsType->CreateInstance();

    if (Dynamics == nullptr)
    {
        return -0x5fff8;
    }

    if ((result = Dynamics->Init(elementalType->DynamicsType, this)) != 0)
    {
        return result;
    }

    MCAppearanceType* apprType = AppearanceTypeList()->GetAppearance(elementalType->AppearName);

    if (apprType == nullptr)
    {
        return -0x2fff7;
    }

    auto* actor = new MCElementalActor;
    Appearance = actor;

    if (actor == nullptr)
    {
        return -0x2ffff;
    }

    actor->Init(nullptr, nullptr);

    if ((apprType->AppearanceNum & 0xff000000) != 0x8000000)
    {
        return -0x5fff6;
    }

    if ((result = actor->Init(apprType, this)) != 0)
    {
        return result;
    }

    ObjectClass = MCObjectClass::Elemental;
    DistanceSinceMarkSeen = 1000.0f;
    Removed = 0;
    ElementalCanJump = elementalType->CanJump;
    return 0;
}

auto MCElemental::SetControl(uint32_t controlType, uint32_t controlData, int32_t controlParam) -> int32_t
{
    int32_t result = 0;

    switch (controlType)
    {
        case 1:
        {
            delete Control;
            auto* playerControl = new MCPlayerControl;
            Control = playerControl;

            if (playerControl == nullptr)
            {
                return -0x5fffc;
            }

            if ((result = playerControl->Init(this, 0)) != 0)
            {
                return result;
            }
            break;
        }

        case 2:
        {
            delete Control;
            auto* aiControl = new MCElementalAIControl;
            Control = aiControl;

            if (aiControl == nullptr)
            {
                return -0x5fffc;
            }

            if ((result = aiControl->Init(this)) != 0)
            {
                return result;
            }
            break;
        }

        case 3:
            // Keeps the control it has.
            break;
        default:
            return -0x5fffb;
    }

    if (controlData != 3)
    {
        return -0x5fff9;
    }

    auto* elementalControlData = new MCElementalControlData;
    Control->ControlData = elementalControlData;

    if (elementalControlData == nullptr)
    {
        return -0x5fffa;
    }

    return elementalControlData->Init(0);
}

auto MCElemental::Init(MCFitIniFile* elementalFile) -> int32_t
{
    int32_t result = elementalFile->SeekBlock("Header");

    if (result != 0)
    {
        return result;
    }

    char fileType[128];

    if ((result = elementalFile->ReadIdString("FileType", fileType, 127)) != 0)
    {
        return result;
    }

    if (std::strcmp(fileType, "ElementalProfile") != 0)
    {
        return -1;
    }

    if ((result = elementalFile->SeekBlock("General")) != 0)
    {
        return result;
    }

    char nameBuffer[128];
    elementalFile->ReadIdString("Name", nameBuffer, 127);
    DebugStatus = nameBuffer;

    if ((result = elementalFile->ReadIdFloat("CurTonnage", Tonnage)) != 0)
    {
        return result;
    }

    if ((result = elementalFile->ReadIdLong("CurHealth", CurHealth)) != 0)
    {
        return result;
    }

    if ((result = elementalFile->ReadIdString("icon", IconName, 0x13)) != 0)
    {
        return result;
    }

    if ((result = elementalFile->SeekBlock("Engine")) != 0)
    {
        return result;
    }

    uint8_t moveSpeed = 0;

    if ((result = elementalFile->ReadIdUChar("MaxMoveSpeed", moveSpeed)) != 0)
    {
        return result;
    }

    MaxRunSpeed = static_cast<float>(moveSpeed);

    if ((result = elementalFile->ReadIdFloat("JumpRange", JumpRange)) != 0)
    {
        return result;
    }

    if ((result = elementalFile->SeekBlock("InventoryInfo")) != 0)
    {
        return result;
    }

    if ((result = elementalFile->ReadIdUChar("NumOther", NumOther)) != 0)
    {
        return result;
    }

    if ((result = elementalFile->ReadIdUChar("NumWeapons", NumWeapons)) != 0)
    {
        return result;
    }

    if ((result = elementalFile->ReadIdUChar("NumAmmo", NumAmmos)) != 0)
    {
        return result;
    }

    const int32_t firstWeapon = NumOther;
    const int32_t firstAmmo = NumOther + NumWeapons;
    const int32_t numItems = NumOther + NumAmmos + NumWeapons;
    Inventory = std::make_unique<MCInventoryItem[]>(static_cast<size_t>(numItems));

    NumAntiMissileSystems = 0;
    // An anti-missile system joins the list, whether it is listed with the other equipment or the weapons.
    const auto addAntiMissileSystem = [&](int32_t item)
    {
        const int32_t masterID = Inventory[item].MasterID;

        if (masterID != MasterClanAntiMissileSystemID && masterID != MasterInnerSphereAntiMissileSystemID)
        {
            return;
        }

        if (NumAntiMissileSystems == 16)
        {
            Fatal(0, "Too many Anti-Missile Systems");
        }

        AntiMissileSystem[NumAntiMissileSystems] = static_cast<uint8_t>(item);
        NumAntiMissileSystems++;
    };

    char blockName[128];

    for (int32_t item = 0; item < firstWeapon; item++)
    {
        std::sprintf(blockName, "Item:%d", item);

        if ((result = elementalFile->SeekBlock(blockName)) != 0)
        {
            return result;
        }

        MCInventoryItem& other = Inventory[item];

        if ((result = elementalFile->ReadIdUChar("MasterID", other.MasterID)) != 0)
        {
            return result;
        }

        other.Health = MasterComponentList[other.MasterID].Health;
        other.Disabled = 0;
        other.Amount = 1;
        other.AmmoIndex = -1;
        other.ReadyTime = 0.0f;
        other.BodyLocation = 0xff;
        other.RangeRatings = nullptr;

        switch (MasterComponentList[other.MasterID].Form)
        {
            case MCComponentForm::Cockpit:
                Cockpit = static_cast<uint8_t>(item);
                break;
            case MCComponentForm::Sensor:
            {
                Sensor = static_cast<uint8_t>(item);
                SensorSystem = SensorSystemManager()->NewSensor();
                SensorSystem->Owner = this;
                SensorSystem->SetRange(MasterComponentList[Inventory[item].MasterID].RangeOrHeat);
                break;
            }
            case MCComponentForm::Engine:
                Engine = static_cast<uint8_t>(item);
                break;
            case MCComponentForm::WeaponBallistic:
                addAntiMissileSystem(item);
                break;
            case MCComponentForm::LifeSupport:
                LifeSupport = static_cast<uint8_t>(item);
                break;
            case MCComponentForm::Ecm:
                Ecm = static_cast<uint8_t>(item);
                break;
            case MCComponentForm::Probe:
                Probe = static_cast<uint8_t>(item);
                break;
            default:
                break;
        }
    }

    for (int32_t item = firstWeapon; item < firstAmmo; item++)
    {
        std::sprintf(blockName, "Item:%d", item);

        if ((result = elementalFile->SeekBlock(blockName)) != 0)
        {
            return result;
        }

        MCInventoryItem& weapon = Inventory[item];

        if ((result = elementalFile->ReadIdUChar("MasterID", weapon.MasterID)) != 0)
        {
            return result;
        }

        if ((result = elementalFile->ReadIdUChar("FacesForward", weapon.FacesForward)) != 0)
        {
            return result;
        }

        const MCMasterComponent& component = MasterComponentList[weapon.MasterID];
        weapon.Health = component.Health;
        weapon.Disabled = 0;
        weapon.Amount = 1;
        weapon.AmmoIndex = -1;
        weapon.ReadyTime = 0.0f;
        weapon.BodyLocation = 0xff;
        // As BattleMech::init: damage per ten seconds, then scaled by the long range over 24.
        weapon.Effectiveness =
            static_cast<int16_t>(static_cast<int32_t>(component.Damage * 10.0 / component.RecycleTime));
        weapon.Effectiveness = static_cast<int16_t>(static_cast<int32_t>(
            static_cast<double>(component.WeaponRange[3]) * weapon.Effectiveness * static_cast<double>(1.0f / 24.0f)));
        weapon.RangeRatings = new float[NumRangeRatings * 2]();

        if (MasterComponentList[Inventory[item].MasterID].Form == MCComponentForm::WeaponBallistic)
        {
            addAntiMissileSystem(item);
        }

        ObjectTypeManager()->Load(
            static_cast<int32_t>(
                WeaponFXTable[static_cast<int8_t>(MasterComponentList[Inventory[item].MasterID].WeaponEffect)]),
            1);
    }

    for (int32_t item = firstAmmo; item < numItems; item++)
    {
        std::sprintf(blockName, "Item:%d", item);

        if ((result = elementalFile->SeekBlock(blockName)) != 0)
        {
            return result;
        }

        MCInventoryItem& ammo = Inventory[item];

        if ((result = elementalFile->ReadIdUChar("MasterID", ammo.MasterID)) != 0)
        {
            return result;
        }

        int32_t amount = 0;

        if (elementalFile->ReadIdLong("Amount", amount) != 0)
        {
            uint8_t smallAmount = 0;

            if ((result = elementalFile->ReadIdUChar("Amount", smallAmount)) != 0)
            {
                return result;
            }

            amount = smallAmount;
        }

        if (amount == -1)
        {
            amount = MasterComponentList[ammo.MasterID].LongValue;
        }

        ammo.Amount = static_cast<int16_t>(amount);
        ammo.StartAmount = ammo.Amount;
        ammo.AmmoIndex = -1;
        ammo.Health = MasterComponentList[ammo.MasterID].Health;
        ammo.Disabled = 0;
        ammo.ReadyTime = 0.0f;
        ammo.BodyLocation = 0xff;
        ammo.RangeRatings = nullptr;
    }

    CalcAmmoTotals();

    for (int32_t item = firstWeapon; item < firstAmmo; item++)
    {
        for (int32_t ammoType = 0; ammoType < NumAmmoTypes; ammoType++)
        {
            if (static_cast<int32_t>(MasterComponentList[Inventory[item].MasterID].AmmoMasterId) ==
                AmmoTypeTotal[ammoType].MasterId)
            {
                Inventory[item].AmmoIndex = static_cast<int16_t>(ammoType);
                break;
            }
        }
    }

    for (int32_t item = 0; item < firstWeapon; item++)
    {
        const int32_t masterID = Inventory[item].MasterID;

        if (masterID != MasterClanAntiMissileSystemID && masterID != MasterInnerSphereAntiMissileSystemID)
        {
            continue;
        }

        for (int32_t ammoType = 0; ammoType < NumAmmoTypes; ammoType++)
        {
            if (static_cast<int32_t>(MasterComponentList[masterID].AmmoMasterId) == AmmoTypeTotal[ammoType].MasterId)
            {
                Inventory[item].AmmoIndex = static_cast<int16_t>(ammoType);
                break;
            }
        }
    }

    CalcLongestRangeWeapon();
    CalcWeaponEffectiveness(1);
    CalcWeaponEffectiveness(0);
    MaxCV = CalcCV(1);
    CurCV = CalcCV(0);

    if (ElementalCanJump == 0)
    {
        // Marines are worth a fixed amount.
        CurCV = 50000;
        MaxCV = 50000;
    }

    return 0;
}

auto MCElemental::Destroy() -> void
{
}

auto MCElemental::CalcCV(int calcMax) -> int32_t
{
    // Offense: the weapons' ratings, scaled by the top speed.
    double offense = 0.0;
    const int32_t firstWeapon = NumOther;

    for (int32_t item = firstWeapon; item < firstWeapon + NumWeapons; item++)
    {
        if (calcMax != 0 || Inventory[item].Disabled == 0)
        {
            offense += MasterComponentList[Inventory[item].MasterID].BattleRating;
        }
    }

    offense *= (MaxRunSpeed - 18.0) * 0.05555555555555555 + 1.0;

    // Defense: health, tonnage, the speed class and the other equipment.
    double defense = static_cast<double>(calcMax != 0 ? MaxHealth : CurHealth);
    defense += TonnageClass;
    int32_t speedClass = 0;

    while (speedClass < 5 && static_cast<float>(TargetMoveModifierTable[speedClass][0]) < MaxRunSpeed)
    {
        speedClass++;
    }

    // Port fix: past the table (a top speed over 999) the original reads the word after it.
    if (speedClass == 5)
    {
        speedClass = 4;
    }

    defense += TargetMoveModifierTable[speedClass][1] * 10;

    for (int32_t item = 0; item < firstWeapon; item++)
    {
        if (calcMax != 0 || Inventory[item].Disabled == 0)
        {
            defense += MasterComponentList[Inventory[item].MasterID].BattleRating;
        }
    }

    return static_cast<int32_t>(defense + offense);
}

auto MCElemental::IsJumping(MCVector3D* jumpGoalOut) -> int
{
    if (jumpGoalOut != nullptr)
    {
        *jumpGoalOut = JumpGoal;
    }

    return InJump;
}

auto MCElemental::GetJumpRange(int32_t* numOffsets, int32_t* jumpCost) -> float
{
    if (ElementalCanJump == 0)
    {
        return 0.0f;
    }

    if (numOffsets != nullptr)
    {
        *numOffsets = 32;
    }

    if (jumpCost != nullptr)
    {
        *jumpCost = 20;
    }

    return MetersPerWorldUnit * MCTerrain::MetersPerVertex;
}

auto MCElemental::UpdateJump() -> int
{
    if (IsJumping(nullptr) == 0)
    {
        return 0;
    }

    auto* controlData = static_cast<MCElementalControlData*>(Control->ControlData);
    auto* actor = static_cast<MCElementalActor*>(Appearance);

    if (actor->Jumping == 0)
    {
        if (actor->JumpSetup == 0)
        {
            // Landed: on to the step after the jump.
            InJump = 0;
            MCMovePath* path = Pilot->GetMovePath();
            path->NumSteps = path->NumStepsWhenNotPaused;
            path->CurStep++;
            LastValidPosition = Position;
            return 1;
        }

        controlData->Jump = 1;
        controlData->JumpDistance = static_cast<float>(DistanceFrom(JumpGoal));
        controlData->Throttle = 0;
        return 1;
    }

    // In the air: turns toward the landing point.
    auto* dynType = static_cast<MCElementalDynamicsType*>(static_cast<MCElementalType*>(ObjType)->DynamicsType);
    const float facing = RelFacingTo(JumpGoal, -1);
    double maxTurn = static_cast<double>(dynType->MaxElementalYawRate);

    if (maxTurn > 180.0)
    {
        maxTurn = 180.0f;
    }

    if (facing >= -5.0 && facing <= 5.0)
    {
        return 1;
    }

    double turn = -(static_cast<double>(facing) / FrameLength);

    if (turn > maxTurn)
    {
        turn = maxTurn;
    }
    else
    {
        const auto minTurn = static_cast<float>(-maxTurn);

        if (turn < minTurn)
        {
            turn = minTurn;
        }
    }

    controlData->Rotate = static_cast<int8_t>(static_cast<int32_t>(turn / maxTurn * 64.0f));
    return 1;
}

auto MCElemental::PivotTo() -> int
{
    MCMechWarrior* warrior = Pilot;
    MCMovePath* path = warrior->GetMovePath();
    const int32_t moveState = warrior->MoveOrders.MoveState;
    const int32_t moveStateGoal = warrior->MoveOrders.MoveStateGoal;
    auto* controlData = static_cast<MCElementalControlData*>(Control->ControlData);
    auto* dynType = static_cast<MCElementalDynamicsType*>(static_cast<MCElementalType*>(ObjType)->DynamicsType);

    // Starts a turn of <turn> degrees, no faster than the yaw rate allows this frame.
    const auto pivot = [&](float turn) -> int
    {
        float maxTurn = static_cast<float>(dynType->MaxElementalYawRate) * FrameLength;

        if (maxTurn > 180.0)
        {
            maxTurn = 180.0f;
        }

        if (static_cast<float>(std::abs(static_cast<int32_t>(turn))) > maxTurn)
        {
            turn = turn > 0.0f ? maxTurn : -maxTurn;
        }

        controlData->Rotate = RotateRequest(turn, maxTurn);
        return 1;
    };

    const auto hasNextStep = [&]()
    { return path->NumStepsWhenNotPaused > 0 && path->CurStep < path->NumStepsWhenNotPaused; };

    if (moveState == MOVESTATE_PIVOT_FORWARD)
    {
        if (moveStateGoal != MOVESTATE_PIVOT_FORWARD && moveStateGoal != MOVESTATE_FORWARD)
        {
            warrior->MoveOrders.MoveState = MOVESTATE_FORWARD;
        }
        else if (!hasNextStep())
        {
            warrior->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
        }
        else
        {
            const MCVector3D destination = path->StepList[path->CurStep].Destination;
            Appearance->SetGestureGoal(0);
            controlData->Throttle = 0;
            const float facing = RelFacingTo(destination, -1);

            if (facing < -15.0 || facing > 15.0)
            {
                return pivot(-facing);
            }

            Pilot->MoveOrders.MoveState = MOVESTATE_FORWARD;
        }
    }
    else if (moveState == MOVESTATE_PIVOT_REVERSE)
    {
        if (moveStateGoal != MOVESTATE_PIVOT_REVERSE && moveStateGoal != MOVESTATE_REVERSE)
        {
            warrior->MoveOrders.MoveState = MOVESTATE_FORWARD;
        }
        else if (!hasNextStep())
        {
            warrior->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
        }
        else
        {
            const MCVector3D destination = path->StepList[path->CurStep].Destination;
            Appearance->SetGestureGoal(0);
            // Original behaviour (OB-004): it clears the second byte of the jump request, not the throttle.
            controlData->Jump &= ~0xff00;
            const float facing = RelFacingTo(destination, -1);

            if (facing > -165.0 && facing < 165.0)
            {
                return pivot(-(facing < 0.0f ? facing + 180.0f : facing - 180.0f));
            }

            if (moveStateGoal == MOVESTATE_REVERSE)
            {
                Pilot->MoveOrders.MoveState = MOVESTATE_REVERSE;
            }
            else
            {
                Pilot->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
            }
        }
    }
    else if (moveState == MOVESTATE_PIVOT_TARGET)
    {
        if (moveStateGoal != MOVESTATE_PIVOT_TARGET)
        {
            warrior->MoveOrders.MoveState = MOVESTATE_FORWARD;
        }
        else
        {
            MCVector3D targetPosition;
            MCGameObject* target = warrior->GetLastTarget();

            if (target != nullptr)
            {
                targetPosition = target->GetPosition();
            }
            else if (warrior->CurTacOrder.Code == MCTacticalOrderCode::AttackPoint)
            {
                targetPosition = warrior->AttackOrders.TargetPoint;
            }
            else
            {
                warrior->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
                warrior->GetMovePath()->NumSteps = warrior->GetMovePath()->NumStepsWhenNotPaused;
                return 0;
            }

            Appearance->SetGestureGoal(0);
            controlData->Throttle = 0;
            const float facing = RelFacingTo(targetPosition, -1);
            const float fireArc = GetFireArc();

            if (facing < -fireArc || fireArc < facing)
            {
                return pivot(-facing);
            }

            Pilot->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
        }
    }
    else if (moveStateGoal == MOVESTATE_PIVOT_TARGET || moveStateGoal == MOVESTATE_PIVOT_FORWARD ||
             moveStateGoal == MOVESTATE_PIVOT_REVERSE)
    {
        warrior->MoveOrders.MoveState = moveStateGoal;
    }

    warrior = Pilot;

    if (warrior->MoveOrders.YieldTime <= -1.0)
    {
        warrior->GetMovePath()->NumSteps = warrior->GetMovePath()->NumStepsWhenNotPaused;
    }

    return 0;
}

auto MCElemental::UpdateMoveStateGoal() -> void
{
    MCMechWarrior* warrior = Pilot;
    MCMovePath* path = warrior->GetMovePath();

    if (path->NumSteps > 0 || (warrior->MoveOrders.MoveStateGoal != MOVESTATE_PIVOT_TARGET &&
                               warrior->MoveOrders.MoveStateGoal != MOVESTATE_PIVOT_FORWARD))
    {
        warrior->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
    }
}

auto MCElemental::UpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                 int32_t& newGestureStateGoal, int32_t& newMoveState, int32_t& minThrottle,
                                 int32_t& maxThrottle) -> int
{
    MCMechWarrior* warrior = Pilot;
    auto* controlData = static_cast<MCElementalControlData*>(Control->ControlData);
    auto* dynType = static_cast<MCElementalDynamicsType*>(static_cast<MCElementalType*>(ObjType)->DynamicsType);
    MCMovePath* path = warrior->GetMovePath();
    newThrottleSetting = controlData->Throttle;
    newRotatePerSec = 0.0f;

    if (path->NumSteps < 1)
    {
        newGestureStateGoal = 0;
        return 0;
    }

    // The end of the path: done, unless more global steps are to come.
    const auto reachedEnd = [&]() -> int
    {
        int finished = 1;

        if (warrior->MoveOrders.PathType == 2 &&
            warrior->MoveOrders.Path[0]->GlobalStep < warrior->MoveOrders.NumGlobalSteps - 1)
        {
            finished = 0;
        }

        if (warrior->MoveOrders.Path[0] != nullptr)
        {
            warrior->MoveOrders.Path[0]->Clear();
        }

        return finished;
    };

    // A step whose direction is past 7 is a jump.
    const auto isJumpStep = [&](int32_t step) { return static_cast<int8_t>(path->StepList[step].Direction) > 7; };

    int32_t step = path->CurStep;

    if (step == path->NumSteps)
    {
        return reachedEnd();
    }

    newGestureStateGoal = 1;
    MCVector3D destination = path->StepList[step].Destination;
    LastValidPosition = destination;
    const auto distance = static_cast<float>(DistanceFrom(destination));
    // (The original also measures the distance to the path object itself, and drops it.)
    const float margin = step == path->NumSteps - 1 ? MoveMarginOfError[1] : MoveMarginOfError[0];

    if (distance < margin)
    {
        // Reached the step: on to the next.
        step++;
        Pilot->MoveOrders.TimeOfLastStep = ScenarioTime;
        path->CurStep = step;

        if (path->NumSteps <= step)
        {
            return reachedEnd();
        }

        if (isJumpStep(step))
        {
            newGestureStateGoal = 2;
            return 0;
        }

        destination = path->StepList[step].Destination;
    }
    else if (isJumpStep(step))
    {
        newGestureStateGoal = 2;
        return 0;
    }

    const float facing = RelFacingTo(destination, -1);
    warrior = Pilot;
    const int32_t moveState = warrior->MoveOrders.MoveState;
    const int32_t moveStateGoal = warrior->MoveOrders.MoveStateGoal;
    // Stops on the path to change the move state.
    const auto changeState = [&](int32_t state)
    {
        warrior->GetMovePath()->NumSteps = 0;
        newMoveState = state;
        return 0;
    };

    // Turns toward the step, no faster than the yaw rate allows this frame.
    const auto steer = [&]()
    {
        double maxTurn = static_cast<double>(dynType->MaxElementalYawRate) * FrameLength;

        if (maxTurn > 180.0)
        {
            maxTurn = 180.0f;
        }

        if (std::fabs(newRotatePerSec) > maxTurn)
        {
            newRotatePerSec = static_cast<float>(newRotatePerSec <= 0.0 ? -maxTurn : maxTurn);
        }

        newRotate = static_cast<char>(static_cast<int8_t>(static_cast<int32_t>(newRotatePerSec / maxTurn * 64.0f)));
        return 0;
    };

    if (moveState == MOVESTATE_FORWARD)
    {
        switch (moveStateGoal)
        {
            case MOVESTATE_FORWARD:
            {
                newGestureStateGoal = 1;
                newThrottleSetting = 100;

                if (facing >= -5.0 && facing <= 5.0)
                {
                    return 0;
                }

                newRotatePerSec = -facing;
                return steer();
            }
            case MOVESTATE_PIVOT_FORWARD:
                return changeState(MOVESTATE_PIVOT_FORWARD);
            case MOVESTATE_REVERSE:
            case MOVESTATE_PIVOT_REVERSE:
                return changeState(MOVESTATE_PIVOT_REVERSE);
            default:
                return changeState(MOVESTATE_FORWARD);
        }
    }

    if (moveState == MOVESTATE_REVERSE)
    {
        switch (moveStateGoal)
        {
            case MOVESTATE_REVERSE:
            {
                newGestureStateGoal = 1;
                newThrottleSetting = -100;
                newRotatePerSec = facing >= 0.0f ? -(facing - 180.0f) : -(facing + 180.0f);
                return steer();
            }
            case MOVESTATE_FORWARD:
            case MOVESTATE_PIVOT_FORWARD:
                return changeState(MOVESTATE_PIVOT_FORWARD);
            case MOVESTATE_PIVOT_REVERSE:
                return changeState(MOVESTATE_PIVOT_REVERSE);
            default:
                return changeState(MOVESTATE_FORWARD);
        }
    }

    switch (moveStateGoal)
    {
        case MOVESTATE_FORWARD:
        case MOVESTATE_PIVOT_FORWARD:
            return changeState(MOVESTATE_PIVOT_FORWARD);
        case MOVESTATE_REVERSE:
        case MOVESTATE_PIVOT_REVERSE:
            return changeState(MOVESTATE_PIVOT_REVERSE);
        default:
            return 0;
    }
}

auto MCElemental::SetNextMovePath(char& newThrottleSetting, int32_t& newGestureStateGoal) -> void
{
    MCMechWarrior* warrior = Pilot;
    MCVector3D nextWayPoint;

    if (warrior->GetNextWayPoint(nextWayPoint, 1) != 0)
    {
        warrior->SetMoveGoal(0, &nextWayPoint, nullptr);
        warrior->RequestMovePath(warrior->CurTacOrder.SelectionIndex, 1, 0);
        return;
    }

    warrior->ClearMoveOrders();
    newGestureStateGoal = 0;
}

auto MCElemental::SetControlSettings(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                     int32_t& newGestureStateGoal, int32_t& minThrottle, int32_t& maxThrottle) -> void
{
    auto* actor = static_cast<MCElementalActor*>(Appearance);

    if (InJump != 0 && actor->Jumping == 0)
    {
        // The jump is over: back on the path.
        InJump = 0;
        MCMovePath* path = Pilot->GetMovePath();
        path->NumSteps = path->NumStepsWhenNotPaused;
    }

    if (newGestureStateGoal == 2)
    {
        // A jump step: jump to it.
        MCMovePath* path = Pilot->GetMovePath();
        path->NumSteps = 0;
        JumpGoal = path->StepList[path->CurStep].Destination;
        actor->SetJumpParameters(static_cast<float>(DistanceFrom(JumpGoal)));
    }

    MCMechWarrior* warrior = Pilot;

    if (warrior->CurTacOrder.IsJumpOrder() != 0 && InJump == 0)
    {
        // A jump order: to its first way point.
        newGestureStateGoal = 2;
        const float* point = warrior->CurTacOrder.MoveParams.WayPath.Points;
        JumpGoal.X = point[0];
        JumpGoal.Y = point[1];
        JumpGoal.Z = point[2];
        actor->SetJumpParameters(static_cast<float>(DistanceFrom(JumpGoal)));
    }

    auto* controlData = static_cast<MCElementalControlData*>(Control->ControlData);

    if (newGestureStateGoal != -1)
    {
        switch (newGestureStateGoal)
        {
            case 0:
                newThrottleSetting = 0;
                break;
            case 1:
                newThrottleSetting = 100;
                break;
            case 2:
            {
                InJump = 1;
                newThrottleSetting = 0;
                break;
            }
            default:
                break;
        }

        if (newThrottleSetting < minThrottle)
        {
            newThrottleSetting = static_cast<char>(minThrottle);
        }
        else if (maxThrottle < newThrottleSetting)
        {
            newThrottleSetting = static_cast<char>(maxThrottle);
        }

        controlData->Throttle = newThrottleSetting;
    }

    if (newRotate != 0)
    {
        controlData->Rotate = newRotate;
    }
}

auto MCElemental::UpdateMovement() -> void
{
    auto* controlData = static_cast<MCElementalControlData*>(Control->ControlData);

    if (DisableThisFrame != 0 || ShutDownThisFrame != 0 || StartUpThisFrame != 0)
    {
        // Elementals just stand still for these; the requests clear once the actor stops.
        if (Appearance->SetGestureGoal(0) == 0)
        {
            DisableThisFrame = 0;
            ShutDownThisFrame = 0;
            StartUpThisFrame = 0;
        }

        controlData->Throttle = 0;
        return;
    }

    if (IsCaptured() != 0 || Status == 4 || Status == 5 || Status == 1)
    {
        return;
    }

    if (UpdateJump() != 0)
    {
        return;
    }

    if (PivotTo() != 0)
    {
        return;
    }

    float newRotatePerSec = 0.0f;
    int32_t minThrottle = -100;
    int32_t maxThrottle = 100;
    char newRotate = 0;
    int32_t newMoveState = -1;
    int32_t newGestureStateGoal = -1;
    char newThrottleSetting = controlData->Throttle;

    if (ElementalCanJump == 0)
    {
        MCMechWarrior* warrior = Pilot;

        if (warrior->GetMovePath()->NumSteps == 0)
        {
            // An idle marine wanders. Original behaviour (OB-001): RollDice(100) is always 1, so always toward -x, -y.
            MCVector3D wanderPoint = Position;

            if (static_cast<int>(RollDice(100)) < 51)
            {
                wanderPoint.X = wanderPoint.X - static_cast<float>(RandomNumber(200));
            }
            else
            {
                wanderPoint.X = static_cast<float>(RandomNumber(200)) + wanderPoint.X;
            }

            if (static_cast<int>(RollDice(100)) < 51)
            {
                wanderPoint.Y = wanderPoint.Y - static_cast<float>(RandomNumber(200));
            }
            else
            {
                wanderPoint.Y = static_cast<float>(RandomNumber(200)) + wanderPoint.Y;
            }

            warrior->OrderMoveToPoint(0, 1, MCOrderOrigin::Player, wanderPoint, -1, 1);
        }
    }

    UpdateMoveStateGoal();

    if (UpdateMovePath(newRotate, newThrottleSetting, newRotatePerSec, newGestureStateGoal, newMoveState, minThrottle,
                       maxThrottle) != 0)
    {
        SetNextMovePath(newThrottleSetting, newGestureStateGoal);
    }

    if (newMoveState != -1)
    {
        Pilot->MoveOrders.MoveState = newMoveState;
    }

    SetControlSettings(newRotate, newThrottleSetting, newRotatePerSec, newGestureStateGoal, minThrottle, maxThrottle);
}

auto MCElemental::GetPositionFromHS(uint32_t hotSpot) -> MCVector3D
{
    return Position;
}

auto MCElemental::OnScreen() -> int
{
    MCCamera* camera = CameraList()->FindCameraFromIDNumber(1);
    ScreenPos.Y = 0.0f;
    ScreenPos.X = 0.0f;

    if (camera == nullptr || camera->Active == 0)
    {
        return 0;
    }

    float screenY;

    if (UseOldProject == 0)
    {
        MCVector2D screen100;
        MCVector2D screen50;

        if (Terrain() != nullptr)
        {
            Terrain()->ProjectTerrain(Position, screen100, screen50);
        }

        if (camera->CameraScale == 1)
        {
            ScreenPos.X = (screen50.X - camera->ScreenUL50.X) + camera->HalfWidth;
            screenY = screen50.Y - camera->ScreenUL50.Y;
        }
        else
        {
            ScreenPos.X = (screen100.X - camera->ScreenUL.X) + camera->HalfWidth;
            screenY = screen100.Y - camera->ScreenUL.Y;
        }

        screenY = screenY + camera->HalfHeight;
    }
    else
    {
        const float scale = camera->CameraScale != 1 ? 1.0f : 0.5f;
        MCVector3D relative(Position.X - camera->Position.X, Position.Y - camera->Position.Y,
                            Position.Z - camera->Position.Z);
        relative *= scale;
        ScreenPos.X = relative.Y * camera->CosAngle + relative.X * camera->CosAngle + camera->HalfWidth;
        screenY = ((relative.X * camera->SinAngle + camera->HalfHeight) - relative.Y * camera->SinAngle) - relative.Z;
    }

    ScreenPos.Y = screenY;

    if (Appearance != nullptr && Appearance->RecalcBounds(camera) != 0)
    {
        WindowsVisible = Turn;
        return 1;
    }

    return 0;
}

auto MCElemental::Update() -> int32_t
{
    if (InTransport() != 0)
    {
        return 1;
    }

    if (IsDestroyed() != 0)
    {
        if (Removed != 0)
        {
            return 1;
        }

        // The body: when the death timer runs low it blows up and leaves a crater.
        DeathTimer -= FrameLength;

        if (DeathTimer < 0.4 && DeathExplosionDone == 0 && Withdrawing == 0)
        {
            ObjType->CreateExplosion(Position, 0.0f, 0.0f);
            CraterManager()->AddCrater(7, Position, 0);
            DeathExplosionDone = 1;
            return 1;
        }

        if (DeathTimer < 0.0)
        {
            return 1;
        }
    }
    else
    {
        if (DeselectTime != 0.0f && DeselectTime < ScenarioTime)
        {
            DeselectTime = 0.0f;
            Selected = 0;
        }

        if (GetAwake() != 0 && IsDisabled() == 0 && MCTerrain::MetersPerVertex <= DistanceSinceMarkSeen)
        {
            // Every vertex travelled, the elemental marks what it sees.
            if (Alignment == 1)
            {
                Terrain()->MarkSeen(Position, Frame.J, 360.0f, GetProbeEffect() + Scenario->MaxVisualRange, 1);
            }
            else if (Alignment == -1)
            {
                Terrain()->MarkSeen(Position, Frame.J, 360.0f, GetProbeEffect() + Scenario->MaxVisualRange, 2);
            }

            DistanceSinceMarkSeen = 0.0f;
        }

        int32_t result = Control->Update();

        if (result != 1)
        {
            return result;
        }

        result = Dynamics->Update();

        if (result != 1)
        {
            return result;
        }

        auto* controlData = static_cast<MCElementalControlData*>(Control->ControlData);
        auto* actor = static_cast<MCElementalActor*>(Appearance);

        if (controlData->Jump != 0)
        {
            actor->SetJumpParameters(controlData->JumpDistance);
            controlData->Throttle = 0;
        }

        // In the air the actor moves it (and nothing collides with it); on the ground the dynamics do.
        float speed = actor->GetVelocityMagnitude();

        if (actor->CurrentGesture == 2)
        {
            CollisionsOn = 0;
        }
        else
        {
            speed = Dynamics->GetVelocity();
            CollisionsOn = 1;
        }

        MCFrameOfRef turned = Frame;
        speed = -speed;
        RotateAboutK(turned, static_cast<float>(std::sin(HALF_PI / 2.0)), static_cast<float>(std::cos(HALF_PI / 2.0)));
        Velocity.Y = turned.J.Y * speed;
        Velocity.X = turned.J.X * speed;
        Velocity.Z = turned.J.Z * speed;
        MCVector3D move;
        move.X = static_cast<float>(static_cast<double>(Velocity.X) * FrameLength * WorldUnitsPerMeter);
        move.Y = Velocity.Y * FrameLength * WorldUnitsPerMeter;
        move.Z = Velocity.Z * FrameLength * WorldUnitsPerMeter;
        MCVector3D newPosition;
        newPosition.X = move.X + Position.X;
        newPosition.Y = move.Y + Position.Y;
        newPosition.Z = move.Z + Position.Z;
        SetPosition(newPosition);
        DistanceSinceMarkSeen =
            static_cast<float>(std::sqrt(static_cast<double>(move.X) * move.X + static_cast<double>(move.Y) * move.Y +
                                         static_cast<double>(move.Z) * move.Z) +
                               DistanceSinceMarkSeen);
        Position.Z = Terrain()->GetTerrainElevation(Position);

        const int visibleNow = OnScreen();
        const int offScreen = visibleNow == 0 ? 1 : 0;

        if (Withdrawing != 0)
        {
            if (visibleNow == 0)
            {
                ObjType->HandleDestruction(this, nullptr);
                Removed = 1;
            }
        }

        // Original behaviour (OB-002): a marine off the screen is removed.
        if (offScreen && ElementalCanJump == 0)
        {
            RemoveMarine(this, 0.8f);
            Removed = 1;
        }

        if (Appearance != nullptr)
        {
            Appearance->Visible = visibleNow;
            Appearance->Update();
        }
    }

    return 1;
}

auto MCElemental::Render() -> void
{
    int tagged = 0;

    if (IsDestroyed() == 0)
    {
        if (Alignment == HomeTeam()->Alignment)
        {
            if (WindowsVisible == Turn)
            {
                Appearance->Render(0);
            }
        }
        else
        {
            const int32_t contactType = GetContactType(HomeTeam()->Id, tagged);

            if (contactType == 1)
            {
                if (WindowsVisible == Turn)
                {
                    Appearance->Render(0);
                }
            }
            else if (contactType == 2)
            {
                // A sensor contact: a blip sized by tonnage.
                uint8_t* shape;

                if (50.0f < GetTonnage())
                {
                    shape = Scenario->SensorContactShapes[0];
                }
                else if (35.0f < GetTonnage())
                {
                    shape = Scenario->SensorContactShapes[2];
                }
                else
                {
                    shape = Scenario->SensorContactShapes[4];
                }

                if (shape != nullptr)
                {
                    if (VfxShapeCount(shape) <= BlipFrame)
                    {
                        if (SoundSystem != nullptr && UseSound != 0)
                        {
                            SoundSystem->PlayDigitalSample(0x14, 1, this, 0, 1);
                        }

                        BlipFrame = 0;
                    }

                    ElementList()->OpenGroup(-100000, 1);
                    ElementList()->Add(
                        ElementList()->Make<MCVfxElement>(shape, ScreenPos.X, ScreenPos.Y, BlipFrame, 0, nullptr, 0));
                    BlipFrame++;
                }
            }
            else if (ElementalCanJump == 0)
            {
                // An unseen marine: drawn once revealed. Original behaviour (OB-002): removed when it isn't on the
                // screen.
                if (WindowsVisible == 0)
                {
                    OnScreen();
                }

                if (WindowsVisible == Turn)
                {
                    if (IsRevealed() != 0)
                    {
                        Appearance->Render(0);
                    }
                }
                else
                {
                    RemoveMarine(this, 0.8f);
                    Removed = 1;
                }
            }
        }
    }

    if (DrawTerrainGrid != 0)
    {
        // Debug: the move path's steps as lines.
        MCMovePath* path = Pilot->GetMovePath();
        const int32_t numSteps = path->NumSteps;

        for (int32_t i = 0; i < numSteps; i++)
        {
            if (i == numSteps - 1)
            {
                continue;
            }

            MCVector3D from = path->StepList[i].Destination;
            MCVector3D to = path->StepList[i + 1].Destination;
            from.Z = Terrain()->GetTerrainElevation(from);
            to.Z = Terrain()->GetTerrainElevation(to);
            const auto project = [](const MCVector3D& point)
            {
                const float scale = Eye->CameraScale != 1 ? 1.0f : 0.5f;
                const float sx = (point.X - Eye->Position.X) * scale;
                const float sy = (point.Y - Eye->Position.Y) * scale;
                MCVector2D screen;
                screen.X = sx * Eye->CosAngle + sy * Eye->CosAngle + Eye->HalfWidth;
                screen.Y =
                    ((sx * Eye->SinAngle + Eye->HalfHeight) - sy * Eye->SinAngle) - scale * (point.Z - Eye->Position.Z);
                return screen;
            };

            MCVector2D fromScreen = project(from);
            MCVector2D toScreen = project(to);
            ElementList()->OpenGroup(-100000, 1);
            ElementList()->Add(ElementList()->Make<MCLineElement>(fromScreen, toScreen, 0xfe, nullptr, -100000, -1));
        }
    }
}

auto MCElemental::GetBodyState() -> int32_t
{
    return 0;
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
            RemoveMarine(this, 0.0f);
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
        Pilot->TriggerAlarm(1, 0);
    }
    else if (shotInfo->MasterId < 0)
    {
        Pilot->TriggerAlarm(10, static_cast<uint32_t>(attacker->PartId));
    }
    else
    {
        Pilot->TriggerAlarm(1, static_cast<uint32_t>(attacker->PartId));
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
    Pilot->NumSkillUses[MWS_GUNNERY][1]++;
    int32_t hitLocation = -1;

    if (hitRoll < hitChance)
    {
        Pilot->NumSkillSuccesses[MWS_GUNNERY][1]++;

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

                    Assert(hitLocation != -1 ? 1 : 0, 0, " Elemental.FireWeapon: Bad Hit Location ");

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

            Assert(hitLocation != -1 ? 1 : 0, 0, " Elemental.FireWeapon: Bad Hit Location ");
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
        targetPilot->TriggerAlarm(0, static_cast<uint32_t>(PartId));
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

            if (DistanceFrom(enemyPosition) < Scenario->MaxVisualRange)
            {
                Terrain()->MarkRadiusSeen(Position, Frame.J, 360.0f, Scenario->FireVisualRange, seenBy);
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

auto MCElemental::GetVitalInfo(void* vitalInfo) -> int32_t
{
    const int32_t size = MCMover::GetVitalInfo(nullptr);

    if (vitalInfo != nullptr)
    {
        MCMover::GetVitalInfo(vitalInfo);
        auto* info = reinterpret_cast<uint8_t*>(vitalInfo) + size;
        std::memcpy(info, &JumpRange, 4);
        const int32_t zero = 0; // the original copied a field nothing ever set
        std::memcpy(info + 4, &zero, 4);
        std::memcpy(info + 8, &MaxHealth, 4);
        std::memcpy(info + 12, &CurHealth, 4);
    }

    return size + 0x10;
}
