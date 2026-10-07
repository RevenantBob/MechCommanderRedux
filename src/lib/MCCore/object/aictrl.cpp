#include "stdafx.h"
#include "object/aictrl.h"
#include "object/elemntl.h"
#include "object/gvehicl.h"
#include "object/mech.h"
#include "object/mechctrl.h"
#include "object/warrior.h"

//---------------------------------------------------------------------------
// MechAIControl
//---------------------------------------------------------------------------

auto MCMechAIControl::Init(MCGameObject* object) -> int32_t
{
    MCControl::Init(object, 0);
    Pilot = object->GetPilot();
    DynamicsType = static_cast<MCBattleMechType*>(object->GetObjectType())->DynamicsType;
    return 0;
}

auto MCMechAIControl::Update() -> int32_t
{
    auto* data = static_cast<MCMechControlData*>(ControlData);
    data->Reset();
    auto* mech = static_cast<MCBattleMech*>(Me);

    if (mech->GetAwake())
    {
        if (mech->LeftArmBlownThisFrame != 0)
        {
            mech->LeftArmBlownThisFrame = 0;
            data->BlowLeftArm = 1;
        }

        if (mech->RightArmBlownThisFrame != 0)
        {
            mech->RightArmBlownThisFrame = 0;
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

        if (mech->ShutDownThisFrame != 0 || mech->DisableThisFrame != 0)
        {
            mech->UpdateMovement();
        }
    }

    return 1;
}

//---------------------------------------------------------------------------
// GroundVehicleAIControl
//---------------------------------------------------------------------------

auto MCGroundVehicleAIControl::Init(MCGameObject* object) -> int32_t
{
    MCControl::Init(object, 0);
    Pilot = object->GetPilot();
    DynamicsType = static_cast<MCGroundVehicleType*>(object->GetObjectType())->DynamicsType;
    return 0;
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

        if (vehicle->ShutDownThisFrame != 0 || vehicle->DisableThisFrame != 0)
        {
            vehicle->UpdateMovement();
        }
    }

    return 1;
}

//---------------------------------------------------------------------------
// ElementalAIControl
//---------------------------------------------------------------------------

auto MCElementalAIControl::Init(MCGameObject* object) -> int32_t
{
    MCControl::Init(object, 0);
    Pilot = object->GetPilot();
    DynamicsType = static_cast<MCElementalType*>(object->GetObjectType())->DynamicsType;
    return 0;
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

        if (elemental->ShutDownThisFrame != 0 || elemental->DisableThisFrame != 0)
        {
            elemental->UpdateMovement();
        }
    }

    return 1;
}
