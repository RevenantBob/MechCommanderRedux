#include "stdafx.h"
#include "object/netctrl.h"
#include "object/gvehicl.h"
#include "object/mech.h"
#include "object/mechctrl.h"
#include "object/warrior.h"

//---------------------------------------------------------------------------
// MechNetControl
//---------------------------------------------------------------------------

auto MCMechNetControl::Init(MCGameObject* object) -> int32_t
{
    MCControl::Init(object, 0);
    Pilot = object->GetPilot();
    DynamicsType = static_cast<MCBattleMechType*>(object->GetObjectType())->DynamicsType;
    return 0;
}

auto MCMechNetControl::Update() -> int32_t
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

        mech->UpdateWeaponFireChunks(1);
        mech->UpdateCriticalHitChunks(1);
        mech->UpdateRadioChunks(1);

        if (!mech->IsDisabled() && Pilot->Wounds < 6.0f && Pilot->Status != 3 && Pilot->Status != 5 &&
            Pilot->Status != 6)
        {
            Pilot->CheckAlarms();
            mech->NetUpdateMovement();
            return 1;
        }

        if (mech->ShutDownThisFrame != 0 || mech->DisableThisFrame != 0)
        {
            mech->NetUpdateMovement();
        }
    }

    return 1;
}

//---------------------------------------------------------------------------
// GroundVehicleNetControl
//---------------------------------------------------------------------------

auto MCGroundVehicleNetControl::Init(MCGameObject* object) -> int32_t
{
    MCControl::Init(object, 0);
    Pilot = object->GetPilot();
    DynamicsType = static_cast<MCGroundVehicleType*>(object->GetObjectType())->DynamicsType;
    return 0;
}

auto MCGroundVehicleNetControl::Update() -> int32_t
{
    ControlData->Reset();
    auto* vehicle = static_cast<MCGroundVehicle*>(Me);

    if (vehicle->GetAwake())
    {
        vehicle->UpdateWeaponFireChunks(1);
        vehicle->UpdateCriticalHitChunks(1);
        vehicle->UpdateRadioChunks(1);

        if (!vehicle->IsDisabled() && Pilot->Wounds < 6.0f)
        {
            Pilot->CheckAlarms();
            vehicle->NetUpdateMovement();
            return 1;
        }

        if (vehicle->ShutDownThisFrame != 0 || vehicle->DisableThisFrame != 0)
        {
            vehicle->NetUpdateMovement();
        }
    }

    return 1;
}
