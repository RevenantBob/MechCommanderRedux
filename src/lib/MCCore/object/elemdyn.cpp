#include "stdafx.h"
#include "object/elemdyn.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "object/control.h"
#include "object/elemctrl.h"
#include "object/elemntl.h"
#include "sprite/lactor.h"

namespace
{
    /// <summary>Degrees to radians, as MCX.EXE stores it (MCX.EXE @ 0x0077c2a0; a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;

    /// <summary>A control's signed 1/64 steps (MCX.EXE @ 0x0077d3f8).</summary>
    constexpr float CONTROL_STEP = 0.015625f;
}

auto ElementalDynamicsType::destroy() -> void
{
}

auto ElementalDynamicsType::init(FitIniFile* objFile) -> int32_t
{
    int32_t result = objFile->seekBlock("ElementalDynamics");

    if (result != 0)
    {
        return result;
    }

    if ((result = objFile->readIdLong("maxElementalYawRate", maxElementalYawRate)) != 0)
    {
        return result;
    }

    if ((result = objFile->readIdFloat("maxAccel", maxAccel)) != 0)
    {
        return result;
    }

    return objFile->readIdFloat("maxVelocity", maxVelocity);
}

auto ElementalDynamicsType::createInstance() -> Dynamics*
{
    return new ElementalDynamics;
}

auto ElementalDynamics::destroy() -> void
{
}

auto ElementalDynamics::init(DynamicsType* dynType, GameObject* object) -> int32_t
{
    const int32_t result = Dynamics::init(dynType, object);
    accel = static_cast<ElementalDynamicsType*>(type)->maxAccel;
    return result;
}

auto ElementalDynamics::update() -> int32_t
{
    auto* elemental = static_cast<Elemental*>(me);
    const auto* dynType = static_cast<ElementalDynamicsType*>(type);
    auto* controlData = static_cast<ElementalControlData*>(elemental->control->controlData);

    // Turn about the up axis (sine and cosine both stored as floats).
    const float turn = static_cast<float>(static_cast<double>(controlData->rotate) * CONTROL_STEP *
                                          dynType->maxElementalYawRate * frameLength);
    const frame_of_ref frame = elemental->getFrame();
    const double angle = static_cast<double>(turn) * DEGREES_TO_RADIANS;
    const float s = static_cast<float>(std::sin(angle));
    const float c = static_cast<float>(std::cos(angle));
    frame_of_ref turned = frame;
    turned.i.x = frame.j.x * s + c * frame.i.x;
    turned.i.y = frame.j.y * s + frame.i.y * c;
    turned.i.z = frame.j.z * s + frame.i.z * c;
    turned.j.x = frame.j.x * c - frame.i.x * s;
    turned.j.y = frame.j.y * c - frame.i.y * s;
    turned.j.z = frame.j.z * c - frame.i.z * s;
    elemental->setFrame(turned);

    // The throttle is all or nothing.
    auto* actor = static_cast<ElementalActor*>(elemental->getAppearance());
    float throttle = static_cast<float>(controlData->throttle);

    if (throttle != 0.0f && throttle != 100.0f)
    {
        controlData->throttle = 0;
        throttle = 0.0f;
    }

    throttle = static_cast<float>(throttle * 0.01);

    // Port fix: the original read the gesture before testing the actor for null.
    if (actor != nullptr && actor->currentGesture != 2)
    {
        actor->setGestureGoal(throttle == 0.0f ? 0 : 1);
    }

    // Accelerate toward the throttle's share of the top speed.
    const float speedChange = static_cast<float>(static_cast<double>(throttle) * dynType->maxVelocity - velocity);

    if ((speedChange < 0.0f && 0.0f < accel) || (0.0f < speedChange && accel < 0.0f))
    {
        accel = -accel;
    }

    double step = static_cast<double>(frameLength) * accel;

    if (std::fabs(speedChange) < std::fabs(step))
    {
        step = speedChange;
    }

    velocity = static_cast<float>(step + velocity);
    return 1;
}
