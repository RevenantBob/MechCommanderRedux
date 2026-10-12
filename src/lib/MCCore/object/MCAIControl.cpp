#include "stdafx.h"
#include "object/MCAIControl.h"
#include "object/MCBattleMech.h"
#include "object/MCBattleMechType.h"
#include "object/MCElemental.h"
#include "object/MCElementalType.h"
#include "object/MCGroundVehicle.h"
#include "object/MCGroundVehicleType.h"
#include "object/MCMechControlData.h"
#include "object/MCMechWarrior.h"

MCMechAIControl::MCMechAIControl(MCGameObject& mech)
    : MCControl(mech.WeakAs<MCGameObject>())
    , Pilot(mech.GetPilot())
    , DynamicsType(static_cast<MCBattleMechType&>(*mech.GetObjectType()).DynamicsType)
{
}

auto MCMechAIControl::Update() -> int32_t
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

        mech->UpdateDamageTakenRate();

        if (!mech->IsDisabled() && Pilot.lock()->Wounds < 6.0f && Pilot.lock()->Status != 3 &&
            Pilot.lock()->Status != 5 && Pilot.lock()->Status != 6)
        {
            Pilot.lock()->MainDecisionTree();
            mech->UpdateMovement();
            return 1;
        }

        if (mech->ShutDownThisFrame || mech->DisableThisFrame)
        {
            mech->UpdateMovement();
        }
    }

    return 1;
}

MCGroundVehicleAIControl::MCGroundVehicleAIControl(MCGameObject& vehicle)
    : MCControl(vehicle.WeakAs<MCGameObject>())
    , Pilot(vehicle.GetPilot())
    , DynamicsType(static_cast<MCGroundVehicleType&>(*vehicle.GetObjectType()).DynamicsType)
{
}

auto MCGroundVehicleAIControl::Update() -> int32_t
{
    ControlData->Reset();
    auto vehicle = std::static_pointer_cast<MCGroundVehicle>(LockObject(Me));

    if (vehicle->GetAwake())
    {
        vehicle->UpdateDamageTakenRate();

        if (!vehicle->IsDisabled() && Pilot.lock()->Wounds < 6.0f)
        {
            Pilot.lock()->MainDecisionTree();
            vehicle->UpdateMovement();
            return 1;
        }

        if (vehicle->ShutDownThisFrame || vehicle->DisableThisFrame)
        {
            vehicle->UpdateMovement();
        }
    }

    return 1;
}

MCElementalAIControl::MCElementalAIControl(MCGameObject& elemental)
    : MCControl(elemental.WeakAs<MCGameObject>())
    , Pilot(elemental.GetPilot())
    , DynamicsType(static_cast<MCElementalType&>(*elemental.GetObjectType()).DynamicsType)
{
}

auto MCElementalAIControl::Update() -> int32_t
{
    ControlData->Reset();
    auto elemental = std::static_pointer_cast<MCElemental>(LockObject(Me));

    if (elemental->GetAwake())
    {
        elemental->UpdateDamageTakenRate();

        if (!elemental->IsDisabled() && Pilot.lock()->Wounds < 6.0f)
        {
            Pilot.lock()->MainDecisionTree();
            elemental->UpdateMovement();
            return 1;
        }

        if (elemental->ShutDownThisFrame || elemental->DisableThisFrame)
        {
            elemental->UpdateMovement();
        }
    }

    return 1;
}
