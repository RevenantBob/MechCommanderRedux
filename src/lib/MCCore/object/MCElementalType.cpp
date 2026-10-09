#include "stdafx.h"
#include "object/MCElementalType.h"
#include "object/MCElemental.h"
#include "object/MCElementalGameSystem.h"
#include "ai/MCTacticalOrder.h"
#include "camera/MCCamera.h"
#include "iface/iface.h"
#include "lib/MCFatal.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCFitIniFile.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/MCAIControl.h"
#include "object/MCArtillery.h"
#include "object/MCArtilleryType.h"
#include "object/MCArtilleryChunk.h"
#include "object/MCCameraDrone.h"
#include "object/MCCameraDroneType.h"
#include "object/MCBullet.h"
#include "object/MCBulletType.h"
#include "object/MCContactSystem.h"
#include "object/MCElementalDynamics.h"
#include "object/MCMoverGroup.h"
#include "object/MCGroundVehicle.h"
#include "object/MCGroundVehicleType.h"
#include "object/MCGroundVehicleGameSystem.h"
#include "object/MCLaser.h"
#include "object/MCLaserType.h"
#include "object/MCBattleMech.h"
#include "object/MCBattleMechType.h"
#include "object/MCMechGameSystem.h"
#include "object/MCProjectileLaser.h"
#include "object/MCProjectileLaserType.h"
#include "object/MCForces.h"
#include "object/MCMechWarrior.h"
#include "object/MCMoverGameSystem.h"
#include "sound/soundsys.h"
#include "object/MCObjectType.h"
#include "object/MCWeaponShotInfo.h"
#include "object/MCMoverMath.h"

namespace
{
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
        MCMoverMath::RotateAboutK(frame, static_cast<float>(std::sin(radians)), static_cast<float>(std::cos(radians)));
        collidee->SetFrame(frame);
    }
}

MCElementalType::MCElementalType() = default;

MCElementalType::~MCElementalType() = default;

auto MCElementalType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile elementalFile;

    if (const int32_t result = elementalFile.Open(objFile, fileSize); result != 0)
    {
        return result;
    }

    if (const int32_t result = elementalFile.SeekBlock("Header"); result != 0)
    {
        return result;
    }

    const MCFitResult<std::string> fileType = elementalFile.Read<std::string>("FileType");

    if (!fileType.has_value())
    {
        return std::to_underlying(fileType.error());
    }

    if (*fileType != "ElementalType")
    {
        return -1;
    }

    if (const int32_t result = elementalFile.SeekBlock("General"); result != 0)
    {
        return result;
    }

    MCFitReader read(elementalFile);
    read.Value("ID", ElementalId);

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    CanJump = elementalFile.Read<bool>("CanJump").value_or(true);
    uint8_t fileAlignment = 0;
    read.Value("Type", fileAlignment);

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    // "Type" 0 is 1, 1 is -1. Port fix: the original reads other values from past its two-entry table on the stack.
    static constexpr std::array<uint8_t, 2> alignmentMap = {1, 0xff};
    Alignment = fileAlignment < alignmentMap.size() ? alignmentMap[fileAlignment] : 0;
    Name = elementalFile.Read<std::string>("Name").value_or("");
    read.Value("MaxHealth", MaxHealth);

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    if (const int32_t result = elementalFile.SeekBlock("Dynamics"); result != 0)
    {
        return result;
    }

    uint32_t dynamicsTypeId = 0;
    read.Value("Type", dynamicsTypeId);

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    if (dynamicsTypeId != 3)
    {
        return -0x5fffd;
    }

    auto dynamicsType = MCElementalDynamicsType::Create(elementalFile);

    if (!dynamicsType.has_value())
    {
        return std::to_underlying(dynamicsType.error());
    }

    DynamicsType = std::move(*dynamicsType);
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
            TurnAway(collidee, MCMoverMath::HalfPi);
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
            TurnAway(collidee, angle * MCMoverMath::DegreesToRadians);
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
                MCMoverMath::RotateAboutK(frame,
                                          static_cast<float>(std::sin(deflection * MCMoverMath::DegreesToRadians)),
                                          static_cast<float>(std::cos(deflection * MCMoverMath::DegreesToRadians)));
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
                TurnAway(collidee, MCMoverMath::HalfPi);
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
        elemental->GetPilot()->TriggerAlarm(MCPilotAlarmType::VehicleDestroyed,
                                            collider == nullptr ? 0 : collider->IdNumber);
    }
    else
    {
        elemental->DeathTimer = 0.0f;
        elemental->GetPilot()->TriggerAlarm(MCPilotAlarmType::VehicleWithdrawn, 0);
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

    if (newElemental->Init(this) != 0)
    {
        return nullptr;
    }

    newElemental->IdNumber = NextIdNumber++;
    return newElemental;
}
