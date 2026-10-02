#include "stdafx.h"
#include "object/netctrl.h"
#include "object/gvehicl.h"
#include "object/mech.h"
#include "object/mechctrl.h"
#include "object/warrior.h"

//---------------------------------------------------------------------------
// MechNetControl
//---------------------------------------------------------------------------

auto MechNetControl::init(GameObject* object) -> int32_t
{
    Control::init(object, 0);
    pilot = object->getPilot();
    dynamicsType = static_cast<BattleMechType*>(object->getObjectType())->dynamicsType;
    return 0;
}

auto MechNetControl::update() -> int32_t
{
    auto* data = static_cast<MechControlData*>(controlData);
    data->reset();
    auto* mech = static_cast<BattleMech*>(me);

    if (mech->getAwake())
    {
        if (mech->pendingControl8D0 != 0)
        {
            mech->pendingControl8D0 = 0;
            data->unknown14 = 1;
        }

        if (mech->pendingControl8D4 != 0)
        {
            mech->pendingControl8D4 = 0;
            data->unknown18 = 1;
        }

        mech->updateWeaponFireChunks(1);
        mech->updateCriticalHitChunks(1);
        mech->updateRadioChunks(1);

        if (!mech->isDisabled() && pilot->wounds < 6.0f && pilot->status != 3 && pilot->status != 5 &&
            pilot->status != 6)
        {
            pilot->checkAlarms();
            mech->netUpdateMovement();
            return 1;
        }

        if (mech->shutDownThisFrame != 0 || mech->disableThisFrame != 0)
        {
            mech->netUpdateMovement();
        }
    }

    return 1;
}

//---------------------------------------------------------------------------
// GroundVehicleNetControl
//---------------------------------------------------------------------------

auto GroundVehicleNetControl::init(GameObject* object) -> int32_t
{
    Control::init(object, 0);
    pilot = object->getPilot();
    dynamicsType = static_cast<GroundVehicleType*>(object->getObjectType())->dynamicsType;
    return 0;
}

auto GroundVehicleNetControl::update() -> int32_t
{
    controlData->reset();
    auto* vehicle = static_cast<GroundVehicle*>(me);

    if (vehicle->getAwake())
    {
        if (vehicle->unknown8B0 != 0)
        {
            vehicle->unknown8B0 = 0;
        }

        vehicle->updateWeaponFireChunks(1);
        vehicle->updateCriticalHitChunks(1);
        vehicle->updateRadioChunks(1);

        if (!vehicle->isDisabled() && pilot->wounds < 6.0f)
        {
            pilot->checkAlarms();
            vehicle->netUpdateMovement();
            return 1;
        }

        if (vehicle->shutDownThisFrame != 0 || vehicle->disableThisFrame != 0)
        {
            vehicle->netUpdateMovement();
        }
    }

    return 1;
}
