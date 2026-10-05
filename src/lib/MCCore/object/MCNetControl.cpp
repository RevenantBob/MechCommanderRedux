#include "stdafx.h"
#include "object/MCNetControl.h"
#include "object/MCBattleMech.h"
#include "object/MCBattleMechType.h"
#include "object/MCGroundVehicle.h"
#include "object/MCGroundVehicleType.h"
#include "object/MCMechControlData.h"
#include "object/MCMechWarrior.h"

MCMechNetControl::MCMechNetControl(MCGameObject& mech)
    : MCControl(mech.WeakAs<MCGameObject>())
    , Pilot(mech.GetPilot())
    , DynamicsType(static_cast<MCBattleMechType&>(*mech.GetObjectType()).DynamicsType)
{
}

auto MCMechNetControl::Update() -> int32_t
{
    auto& data = static_cast<MCMechControlData&>(*ControlData);
    data.Reset();
    auto mech = std::static_pointer_cast<MCBattleMech>(LockObject(Me));

    if (mech->GetAwake())
    {
        if (mech->LeftArmBlownThisFrame)
        {
            mech->LeftArmBlownThisFrame = false;
            data.BlowLeftArm = 1;
        }

        if (mech->RightArmBlownThisFrame)
        {
            mech->RightArmBlownThisFrame = false;
            data.BlowRightArm = 1;
        }

        mech->UpdateWeaponFireChunks(1);
        mech->UpdateCriticalHitChunks(1);
        mech->UpdateRadioChunks(1);

        if (!mech->IsDisabled() && Pilot.lock()->Wounds < 6.0f && Pilot.lock()->Status != 3 &&
            Pilot.lock()->Status != 5 && Pilot.lock()->Status != 6)
        {
            Pilot.lock()->CheckAlarms();
            mech->NetUpdateMovement();
            return 1;
        }

        if (mech->ShutDownThisFrame || mech->DisableThisFrame)
        {
            mech->NetUpdateMovement();
        }
    }

    return 1;
}

MCGroundVehicleNetControl::MCGroundVehicleNetControl(MCGameObject& vehicle)
    : MCControl(vehicle.WeakAs<MCGameObject>())
    , Pilot(vehicle.GetPilot())
    , DynamicsType(static_cast<MCGroundVehicleType&>(*vehicle.GetObjectType()).DynamicsType)
{
}

auto MCGroundVehicleNetControl::Update() -> int32_t
{
    ControlData->Reset();
    auto vehicle = std::static_pointer_cast<MCGroundVehicle>(LockObject(Me));

    if (vehicle->GetAwake())
    {
        vehicle->UpdateWeaponFireChunks(1);
        vehicle->UpdateCriticalHitChunks(1);
        vehicle->UpdateRadioChunks(1);

        if (!vehicle->IsDisabled() && Pilot.lock()->Wounds < 6.0f)
        {
            Pilot.lock()->CheckAlarms();
            vehicle->NetUpdateMovement();
            return 1;
        }

        if (vehicle->ShutDownThisFrame || vehicle->DisableThisFrame)
        {
            vehicle->NetUpdateMovement();
        }
    }

    return 1;
}
