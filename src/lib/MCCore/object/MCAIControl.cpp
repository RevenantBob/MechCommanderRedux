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
    : MCControl(mech)
    , Pilot(mech.GetPilot())
    , DynamicsType(static_cast<MCBattleMechType*>(mech.GetObjectType())->DynamicsType.get())
{
}

auto MCMechAIControl::Update() -> int32_t
{
    auto* data = static_cast<MCMechControlData*>(ControlData.get());
    data->Reset();
    auto* mech = static_cast<MCBattleMech*>(Me);

    if (mech->GetAwake())
    {
        if (mech->LeftArmBlownThisFrame)
        {
            mech->LeftArmBlownThisFrame = false;
            data->BlowLeftArm = 1;
        }

        if (mech->RightArmBlownThisFrame)
        {
            mech->RightArmBlownThisFrame = false;
            data->BlowRightArm = 1;
        }

        mech->UpdateDamageTakenRate();

        if (!mech->IsDisabled() && Pilot->Wounds < 6.0f && Pilot->Status != 3 && Pilot->Status != 5 &&
            Pilot->Status != 6)
        {
            Pilot->MainDecisionTree();
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
    : MCControl(vehicle)
    , Pilot(vehicle.GetPilot())
    , DynamicsType(static_cast<MCGroundVehicleType*>(vehicle.GetObjectType())->DynamicsType.get())
{
}

auto MCGroundVehicleAIControl::Update() -> int32_t
{
    ControlData->Reset();
    auto* vehicle = static_cast<MCGroundVehicle*>(Me);

    if (vehicle->GetAwake())
    {
        vehicle->UpdateDamageTakenRate();

        if (!vehicle->IsDisabled() && Pilot->Wounds < 6.0f)
        {
            Pilot->MainDecisionTree();
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
    : MCControl(elemental)
    , Pilot(elemental.GetPilot())
    , DynamicsType(static_cast<MCElementalType*>(elemental.GetObjectType())->DynamicsType.get())
{
}

auto MCElementalAIControl::Update() -> int32_t
{
    ControlData->Reset();
    auto* elemental = static_cast<MCElemental*>(Me);

    if (elemental->GetAwake())
    {
        elemental->UpdateDamageTakenRate();

        if (!elemental->IsDisabled() && Pilot->Wounds < 6.0f)
        {
            Pilot->MainDecisionTree();
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
