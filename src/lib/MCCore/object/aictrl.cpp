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

auto MechAIControl::init(GameObject* object) -> int32_t
{
    Control::init(object, 0);
    pilot = object->getPilot();
    dynamicsType = static_cast<BattleMechType*>(object->getObjectType())->dynamicsType;
    return 0;
}

auto MechAIControl::update() -> int32_t
{
    auto* data = static_cast<MechControlData*>(controlData);
    data->reset();
    auto* mech = static_cast<BattleMech*>(me);

    if (mech->getAwake())
    {
        if (mech->leftArmBlownThisFrame != 0)
        {
            mech->leftArmBlownThisFrame = 0;
            data->blowLeftArm = 1;
        }

        if (mech->rightArmBlownThisFrame != 0)
        {
            mech->rightArmBlownThisFrame = 0;
            data->blowRightArm = 1;
        }

        mech->updateDamageTakenRate();

        if (!mech->isDisabled() && pilot->wounds < 6.0f && pilot->status != 3 && pilot->status != 5 &&
            pilot->status != 6)
        {
            pilot->mainDecisionTree();
            mech->updateMovement();
            return 1;
        }

        if (mech->shutDownThisFrame != 0 || mech->disableThisFrame != 0)
        {
            mech->updateMovement();
        }
    }

    return 1;
}

//---------------------------------------------------------------------------
// GroundVehicleAIControl
//---------------------------------------------------------------------------

auto GroundVehicleAIControl::init(GameObject* object) -> int32_t
{
    Control::init(object, 0);
    pilot = object->getPilot();
    dynamicsType = static_cast<GroundVehicleType*>(object->getObjectType())->dynamicsType;
    return 0;
}

auto GroundVehicleAIControl::update() -> int32_t
{
    controlData->reset();
    auto* vehicle = static_cast<GroundVehicle*>(me);

    if (vehicle->getAwake())
    {
        vehicle->updateDamageTakenRate();

        if (!vehicle->isDisabled() && pilot->wounds < 6.0f)
        {
            pilot->mainDecisionTree();
            vehicle->updateMovement();
            return 1;
        }

        if (vehicle->shutDownThisFrame != 0 || vehicle->disableThisFrame != 0)
        {
            vehicle->updateMovement();
        }
    }

    return 1;
}

//---------------------------------------------------------------------------
// ElementalAIControl
//---------------------------------------------------------------------------

auto ElementalAIControl::init(GameObject* object) -> int32_t
{
    Control::init(object, 0);
    pilot = object->getPilot();
    dynamicsType = static_cast<ElementalType*>(object->getObjectType())->dynamicsType;
    return 0;
}

auto ElementalAIControl::update() -> int32_t
{
    controlData->reset();
    auto* elemental = static_cast<Elemental*>(me);

    if (elemental->getAwake())
    {
        elemental->updateDamageTakenRate();

        if (!elemental->isDisabled() && pilot->wounds < 6.0f)
        {
            pilot->mainDecisionTree();
            elemental->updateMovement();
            return 1;
        }

        if (elemental->shutDownThisFrame != 0 || elemental->disableThisFrame != 0)
        {
            elemental->updateMovement();
        }
    }

    return 1;
}
