#include "stdafx.h"
#include "object/MCGroundVehicleType.h"
#include "object/MCGameSystemReader.h"
#include "object/MCGroundVehicle.h"
#include "object/MCGroundVehicleGameSystem.h"
#include "ai/MCTacticalOrder.h"
#include "camera/MCCamera.h"
#include "iface/MCTacticalInterface.h"
#include "lib/MCFatal.h"
#include "lib/MCFitIniFile.h"
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
#include "object/MCCollisionSystem.h"
#include "object/MCContactSystem.h"
#include "object/MCElemental.h"
#include "object/MCElementalType.h"
#include "object/MCElementalGameSystem.h"
#include "object/MCExplosion.h"
#include "object/MCExplosionType.h"
#include "object/MCMoverGroup.h"
#include "object/MCGroundVehicleDynamics.h"
#include "object/MCJet.h"
#include "object/MCJetType.h"
#include "object/MCLaser.h"
#include "object/MCLaserType.h"
#include "object/MCBattleMech.h"
#include "object/MCBattleMechType.h"
#include "object/MCMechGameSystem.h"
#include "object/MCProjectileLaser.h"
#include "object/MCProjectileLaserType.h"
#include "object/MCNetControl.h"
#include "object/MCSmoke.h"
#include "object/MCSmokeType.h"
#include "object/MCEffectSystem.h"
#include "object/MCForces.h"
#include "object/MCMechWarrior.h"
#include "object/MCMoverGameSystem.h"
#include "sound/MCSoundSystem.h"
#include "object/MCObjectType.h"
#include "object/MCWeaponChunkDebug.h"
#include "object/MCWeaponShotInfo.h"
#include "object/MCMoverMath.h"

namespace
{

    /// <summary>
    /// Hits <paramref name="victim"/> from <paramref name="shooter"/>'s side for <paramref name="damage"/> (attack
    /// source 1).
    /// </summary>
    void CollisionHit(MCGameObject* victim, MCGameObject* shooter, float damage)
    {
        const int32_t hitLocation = victim->CalcHitLocation(shooter, -1, 1, 0);
        const float entryAngle = victim->RelFacingTo(shooter->GetPosition(), -1);
        MCWeaponShotInfo shotInfo;
        shotInfo.Init(shooter, -1, damage, hitLocation, entryAngle);
        victim->HandleWeaponHit(&shotInfo, MPlayer != nullptr);
    }
}

MCGroundVehicleType::MCGroundVehicleType()
    : CrashAvoidSelf(DefaultGroundVehicleCrashAvoidSelf)
    , CrashAvoidPath(DefaultGroundVehicleCrashAvoidPath)
    , CrashBlockSelf(DefaultGroundVehicleCrashBlockSelf)
    , CrashBlockPath(DefaultGroundVehicleCrashBlockPath)
    , CrashYieldTime(DefaultGroundVehicleCrashYieldTime)
{
}

MCGroundVehicleType::~MCGroundVehicleType() = default;

auto MCGroundVehicleType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    static constexpr std::array<std::string_view, NumGroundVehicleLocations> locationNames = {"Front", "Left", "Right",
                                                                                              "Rear", "Turret"};

    MCFitIniFile vehicleFile;

    if (const int32_t result = vehicleFile.Open(objFile, fileSize); result != 0)
    {
        return result;
    }

    if (const int32_t result = vehicleFile.SeekBlock("Header"); result != 0)
    {
        return result;
    }

    const MCFitResult<std::string> fileType = vehicleFile.Read<std::string>("FileType");

    if (!fileType.has_value())
    {
        return std::to_underlying(fileType.error());
    }

    if (*fileType != "GroundVehicleType")
    {
        return -1;
    }

    if (const int32_t result = vehicleFile.SeekBlock("General"); result != 0)
    {
        return result;
    }

    MCFitReader read(vehicleFile);
    uint8_t fileAlignment = 0;
    read.Value("ID", VehicleId);
    read.Value("Alignment", fileAlignment);

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    // "Alignment" 0 is 1, 1 is -1. Port fix: the original reads other values from past its two-entry table on the
    // stack.
    static constexpr std::array<uint8_t, 2> alignmentMap = {1, 0xff};
    Alignment = fileAlignment < alignmentMap.size() ? alignmentMap[fileAlignment] : 0;
    Name = vehicleFile.Read<std::string>("Name").value_or("");
    read.Value("Chassis", Chassis);
    read.Value("TonnageClass", TonnageClass);

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    AmmoTruck = vehicleFile.Read<bool>("AmmoTruck").value_or(false);
    RefitPoints = vehicleFile.Read<int32_t>("RefitPoints").value_or(0);
    MineSweeper = vehicleFile.Read<bool>("MineSweeper").value_or(false);
    MinesToLay = vehicleFile.Read<int32_t>("MinesToLay").value_or(0);
    ElementalCarrier = vehicleFile.Read<bool>("ElementalCarrier").value_or(false);
    Seats = vehicleFile.Read<uint8_t>("Seats").value_or(0);
    Assert(Seats <= MaxGroundVehicleSeats ? 1 : 0, Seats, "Too many seats");
    ExplRad = vehicleFile.Read<float>("ExplosionRadius").value_or(0.0f);
    ExplDmg = vehicleFile.Read<float>("ExplosionDamage").value_or(0.0f);

    if (const int32_t result = vehicleFile.SeekBlock("InternalStructure"); result != 0)
    {
        return result;
    }

    for (int32_t location = 0; location < NumGroundVehicleLocations; location++)
    {
        read.Value(locationNames[location], InternalStructure[location]);
    }

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    if (const int32_t result = vehicleFile.SeekBlock("Dynamics"); result != 0)
    {
        return result;
    }

    uint32_t dynamicsTypeId = 0;
    read.Value("Type", dynamicsTypeId);

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    if (dynamicsTypeId != 2)
    {
        return -0x5fffd;
    }

    auto dynamicsType = MCGroundVehicleDynamicsType::Create(vehicleFile);

    if (!dynamicsType.has_value())
    {
        return std::to_underlying(dynamicsType.error());
    }

    DynamicsType = std::move(*dynamicsType);

    if (vehicleFile.SeekBlock("MovementSystem") == 0)
    {
        MCGameSystemReader::Optional(vehicleFile, "CrashAvoidSelf", CrashAvoidSelf);
        MCGameSystemReader::Optional(vehicleFile, "CrashAvoidPath", CrashAvoidPath);
        MCGameSystemReader::Optional(vehicleFile, "CrashBlockSelf", CrashBlockSelf);
        MCGameSystemReader::Optional(vehicleFile, "CrashBlockPath", CrashBlockPath);
        MCGameSystemReader::Optional(vehicleFile, "CrashYieldTime", CrashYieldTime);
    }

    return MCObjectType::Init(&vehicleFile);
}

auto MCGroundVehicleType::HandleCollision(MCGameObject* collidee, MCGameObject* collider) -> int
{
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        return 0;
    }

    switch (collider->ObjectClass)
    {
        case MCObjectClass::BattleMech:
        case MCObjectClass::GroundVehicle:
        case MCObjectClass::Elemental:
        {
            const int friendly = collidee->GetPilot()->Alignment == collider->GetPilot()->Alignment ? 1 : 0;

            if (collider->ObjectClass == MCObjectClass::Elemental &&
                static_cast<MCElemental*>(collider)->ElementalCanJump == 0)
            {
                return 0;
            }

            if (friendly != 0)
            {
                return 0;
            }

            MCGameObject* collideeRamTarget = collidee->GetPilot()->CurTacOrder.GetRamTarget();
            MCGameObject* colliderRamTarget = collider->GetPilot()->CurTacOrder.GetRamTarget();

            if (collideeRamTarget != collider && colliderRamTarget != collidee)
            {
                return 0;
            }

            if (collidee->GetCollisionFreeFrom() == collider && ScenarioTime <= collidee->GetCollisionFreeTime())
            {
                return 0;
            }

            collidee->SetCollisionFreeFrom(collider);
            collidee->SetCollisionFreeTime(ScenarioTime + 2.0f);

            if (static_cast<MCGroundVehicleDynamicsType*>(DynamicsType.get())->MaxVelocity != 0.0f)
            {
                MCFrameOfRef frame = collidee->GetFrame();
                MCMoverMath::RotateAboutK(frame, static_cast<float>(std::sin(MCMoverMath::HalfPi)),
                                          static_cast<float>(std::cos(MCMoverMath::HalfPi)));
                collidee->SetFrame(frame);
                collidee->GetVelocity();
                static_cast<MCMover*>(collidee)->BounceToAdjCell();
            }

            CollisionHit(collidee, collider, 1.0f);
            break;
        }

        case MCObjectClass::Building:
        case MCObjectClass::TreeBuilding:
        {
            if (collidee->GetCollisionFreeFrom() == collider && ScenarioTime <= collidee->GetCollisionFreeTime())
            {
                return 0;
            }

            collidee->SetCollisionFreeFrom(collider);
            collidee->SetCollisionFreeTime(ScenarioTime + 2.0f);
            MCFrameOfRef frame = collidee->GetFrame();
            // A big building turns the vehicle further.
            const float angle = collider->GetObjectType()->ExtentRadius > GvObjectCollisionThreshold ? 135.0f : 45.0f;
            MCMoverMath::RotateAboutK(frame, static_cast<float>(std::sin(angle * MCMoverMath::DegreesToRadians)),
                                      static_cast<float>(std::cos(angle * MCMoverMath::DegreesToRadians)));
            collidee->SetFrame(frame);
            static_cast<MCMover*>(collidee)->BounceToAdjCell();
            CollisionHit(collidee, collider, static_cast<float>(collider->GetTonnage() * 0.01 + 0.5));
            break;
        }

        case MCObjectClass::Tree:
        {
            if (collidee->GetCollisionFreeFrom() == collider && ScenarioTime <= collidee->GetCollisionFreeTime())
            {
                return 0;
            }

            collidee->SetCollisionFreeFrom(collider);
            collidee->SetCollisionFreeTime(ScenarioTime + 2.0f);
            MCFrameOfRef frame = collidee->GetFrame();
            collider->GetObjectType();
            double deflection = 0.0;

            if (TonnageClass < GvTonnageCollisionThreshold)
            {
                deflection = static_cast<double>(GvTonnageCollisionThreshold) / TonnageClass * GvTreeDeflection;
            }

            if (deflection > 0.0)
            {
                MCMoverMath::RotateAboutK(frame,
                                          static_cast<float>(std::sin(deflection * MCMoverMath::DegreesToRadians)),
                                          static_cast<float>(std::cos(deflection * MCMoverMath::DegreesToRadians)));
                collidee->SetFrame(frame);
            }
            break;
        }

        case MCObjectClass::TrainCar:
        {
            if (collidee->GetCollisionFreeFrom() == collider && ScenarioTime <= collidee->GetCollisionFreeTime())
            {
                return 0;
            }

            collidee->SetCollisionFreeFrom(collider);
            collidee->SetCollisionFreeTime(ScenarioTime + 2.0f);

            if (static_cast<MCGroundVehicleDynamicsType*>(DynamicsType.get())->MaxVelocity != 0.0f)
            {
                MCFrameOfRef frame = collidee->GetFrame();
                MCMoverMath::RotateAboutK(frame, static_cast<float>(std::sin(MCMoverMath::HalfPi)),
                                          static_cast<float>(std::cos(MCMoverMath::HalfPi)));
                collidee->SetFrame(frame);
                collidee->GetVelocity();
            }

            static_cast<MCMover*>(collidee)->BounceToAdjCell();
            break;
        }

        default:
            return 0;
    }

    SoundSystem()->PlayDigitalSample(4, 1, collidee, 0, 0);
    return 0;
}

auto MCGroundVehicleType::HandleDestruction(MCGameObject* collidee, MCGameObject* collider) -> int
{
    auto* vehicle = static_cast<MCGroundVehicle*>(collidee);

    if (vehicle->GetPilot() == nullptr)
    {
        Fatal(0, " No Pilot in this vehicle! ");
    }

    if (vehicle->GetPoint() == vehicle)
    {
        vehicle->Group->SetPoint(nullptr);
    }

    if (vehicle->SensorSystem != nullptr)
    {
        vehicle->SensorSystem->Disable();
    }

    vehicle->DeathTimer = 0.0f;

    if (vehicle->Withdrawing == 0)
    {
        vehicle->GetPilot()->TriggerAlarm(MCPilotAlarmType::VehicleDestroyed,
                                          collider == nullptr ? 0 : collider->IdNumber);
        vehicle->DeathExplosionDone = 0;
        vehicle->Status = 2;

        if (vehicle->GetAlignment() == HomeTeam()->Alignment)
        {
            FriendlyDestroyed = 1;
        }
        else
        {
            EnemyDestroyed = 1;
        }
    }
    else
    {
        vehicle->GetPilot()->TriggerAlarm(MCPilotAlarmType::VehicleWithdrawn, 0);
    }

    TacticalInterface()->RemoveMech(vehicle->PartId);
    return 1;
}

auto MCGroundVehicleType::CreateInstance() -> std::unique_ptr<MCBaseObject>
{
    auto newVehicle = std::make_unique<MCGroundVehicle>();

    if (newVehicle->Init(this) != 0)
    {
        return nullptr;
    }

    newVehicle->IdNumber = NextIdNumber++;
    return newVehicle;
}
