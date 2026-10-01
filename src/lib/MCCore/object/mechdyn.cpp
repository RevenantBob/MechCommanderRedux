#include "stdafx.h"
#include "object/mechdyn.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "object/control.h"
#include "object/mech.h"
#include "object/mechctrl.h"
#include "sprite/mactor.h"

// The original turns on the x87 stack: the sums below that it keeps at extended precision are done in double.

namespace
{
    /// <summary>Degrees to radians, as MCX.EXE stores it (MCX.EXE @ 0x0077c2a0; a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;

    /// <summary>A control's signed 1/64 steps (MCX.EXE @ 0x0077d3f8).</summary>
    constexpr float CONTROL_STEP = 0.015625f;
}

auto MechDynamicsType::destroy() -> void
{
}

auto MechDynamicsType::init(FitIniFile* objFile) -> int32_t
{
    int32_t result = objFile->seekBlock("MechDynamics");

    if (result != 0)
    {
        return result;
    }

    if ((result = objFile->readIdLong("maxTorsoYawRate", maxTorsoYawRate)) != 0)
    {
        return result;
    }

    if ((result = objFile->readIdLong("maxTorsoYaw", maxTorsoYaw)) != 0)
    {
        return result;
    }

    if ((result = objFile->readIdLong("maxArmYaw", maxArmYaw)) != 0)
    {
        return result;
    }

    if ((result = objFile->readIdLong("maxMechYawRate", maxMechYawRate)) != 0)
    {
        return result;
    }

    if (maxMechYawRate < 720)
    {
        maxMechYawRate = 720;
    }

    if (objFile->readIdLong("maxMechPivotRate", maxMechPivotRate) != 0)
    {
        maxMechPivotRate = static_cast<int32_t>(static_cast<float>(maxMechYawRate) * 0.25f);
    }

    if ((result = objFile->readIdLong("maxLeftArmYawRate", maxLeftArmYawRate)) != 0)
    {
        return result;
    }

    if ((result = objFile->readIdLong("maxRightArmYawRate", maxRightArmYawRate)) != 0)
    {
        return result;
    }

    if ((result = objFile->readIdFloat("maxAccel", maxAccel)) != 0)
    {
        return result;
    }

    return objFile->readIdFloat("maxVelocity", maxVelocity);
}

auto MechDynamicsType::createInstance() -> Dynamics*
{
    return new MechDynamics;
}

auto MechDynamics::destroy() -> void
{
}

auto MechDynamics::init(DynamicsType* dynType, GameObject* object) -> int32_t
{
    return Dynamics::init(dynType, object);
}

auto MechDynamics::update() -> int32_t
{
    auto* mech = static_cast<BattleMech*>(me);
    const auto* dynType = static_cast<MechDynamicsType*>(type);
    auto* controlData = static_cast<MechControlData*>(mech->control->controlData);

    // This frame's turns, in degrees.
    const int32_t yawRate = controlData->pivot != 0 ? dynType->maxMechPivotRate : dynType->maxMechYawRate;
    const float bodyTurn = static_cast<float>(static_cast<double>(frameLength) *
                                              (static_cast<double>(controlData->rotate) * CONTROL_STEP * yawRate));
    const float torsoTurn = static_cast<float>(static_cast<double>(controlData->torsoRotate) * CONTROL_STEP *
                                               dynType->maxTorsoYawRate * frameLength);
    float rightArmTurn = static_cast<float>(static_cast<double>(controlData->rightArmRotate) * CONTROL_STEP *
                                            dynType->maxRightArmYawRate * frameLength);
    float leftArmTurn = static_cast<float>(static_cast<double>(controlData->leftArmRotate) * CONTROL_STEP *
                                           dynType->maxLeftArmYawRate * frameLength);

    int32_t jumping = 0;
    int32_t locked = 0;
    auto* actor = static_cast<MechActor*>(mech->getAppearance());

    if (actor != nullptr)
    {
        jumping = actor->unknown174;
        locked = actor->unknown178;
        const int32_t gestureGoal = controlData->gestureGoal;

        if (gestureGoal != -1)
        {
            actor->setGestureGoal(gestureGoal);
        }
    }

    // The torso turns up to its limit either way.
    if (torsoTurn != 0.0f && locked == 0)
    {
        const float limit = static_cast<float>(dynType->maxTorsoYaw);
        const float negLimit = static_cast<float>(-dynType->maxTorsoYaw);
        double current = mech->torsoRotation;
        double turn = torsoTurn;

        if (current > limit)
        {
            current = limit;
            turn = 0.0;
        }

        if (current < negLimit)
        {
            current = negLimit;
            turn = 0.0;
        }

        if (current + turn > limit)
        {
            turn = limit - current;
        }

        if (current + turn < negLimit)
        {
            turn = negLimit - current;
        }

        mech->torsoRotation = static_cast<float>(current + turn);
    }

    // The arms stop dead at their limits.
    if (rightArmTurn != 0.0f && locked == 0)
    {
        const float limit = static_cast<float>(dynType->maxArmYaw);
        const float negLimit = static_cast<float>(-dynType->maxArmYaw);
        double current = mech->rightArmRotation;

        if (current > limit)
        {
            current = limit;
            rightArmTurn = 0.0f;
        }

        if (current < negLimit)
        {
            current = negLimit;
            rightArmTurn = 0.0f;
        }

        if (current + rightArmTurn > limit)
        {
            current = limit;
            rightArmTurn = 0.0f;
        }

        if (current + rightArmTurn < negLimit)
        {
            current = negLimit;
            rightArmTurn = 0.0f;
        }

        mech->rightArmRotation = static_cast<float>(current + rightArmTurn);
    }

    if (leftArmTurn != 0.0f && locked == 0)
    {
        const float negLimit = static_cast<float>(-dynType->maxArmYaw);
        const float limit = static_cast<float>(dynType->maxArmYaw);
        double current = mech->leftArmRotation;

        if (current < negLimit)
        {
            current = negLimit;
            leftArmTurn = 0.0f;
        }

        if (current > limit)
        {
            current = limit;
            leftArmTurn = 0.0f;
        }

        if (current + leftArmTurn < negLimit)
        {
            current = negLimit;
            leftArmTurn = 0.0f;
        }

        if (current + leftArmTurn > limit)
        {
            current = limit;
            leftArmTurn = 0.0f;
        }

        mech->leftArmRotation = static_cast<float>(current + leftArmTurn);
    }

    // The body turns about its up axis (the sine is stored as a float, the cosine isn't).
    if (jumping == 0)
    {
        const frame_of_ref frame = mech->getFrame();
        const double angle = static_cast<double>(bodyTurn) * DEGREES_TO_RADIANS;
        const float s = static_cast<float>(std::sin(angle));
        const double c = std::cos(angle);
        frame_of_ref turned = frame;
        turned.i.x = static_cast<float>(c * frame.i.x + static_cast<double>(frame.j.x) * s);
        turned.i.y = static_cast<float>(frame.i.y * c) + frame.j.y * s;
        turned.i.z = static_cast<float>(frame.i.z * c) + frame.j.z * s;
        turned.j.x = static_cast<float>(frame.j.x * c) - frame.i.x * s;
        turned.j.y = static_cast<float>(frame.j.y * c) - frame.i.y * s;
        turned.j.z = static_cast<float>(c * frame.j.z - static_cast<double>(frame.i.z * s));
        mech->setFrame(turned);
    }

    return 1;
}
