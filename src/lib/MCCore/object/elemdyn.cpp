#include "stdafx.h"
#include "object/elemdyn.h"
#include "lib/MCFitIniFile.h"
#include "main/main.h"
#include "object/control.h"
#include "object/elemctrl.h"
#include "object/elemntl.h"
#include "sprite/MCElementalActor.h"

namespace
{
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;

    /// <summary>A control's signed 1/64 steps.</summary>
    constexpr float CONTROL_STEP = 0.015625f;
}

auto MCElementalDynamicsType::Destroy() -> void
{
}

auto MCElementalDynamicsType::Init(MCFitIniFile* objFile) -> int32_t
{
    int32_t result = objFile->SeekBlock("ElementalDynamics");

    if (result != 0)
    {
        return result;
    }

    if ((result = objFile->ReadIdLong("maxElementalYawRate", MaxElementalYawRate)) != 0)
    {
        return result;
    }

    if ((result = objFile->ReadIdFloat("maxAccel", MaxAccel)) != 0)
    {
        return result;
    }

    return objFile->ReadIdFloat("maxVelocity", MaxVelocity);
}

auto MCElementalDynamicsType::CreateInstance() -> MCDynamics*
{
    return new MCElementalDynamics;
}

auto MCElementalDynamics::Destroy() -> void
{
}

auto MCElementalDynamics::Init(MCDynamicsType* dynType, MCGameObject* object) -> int32_t
{
    const int32_t result = MCDynamics::Init(dynType, object);
    Accel = static_cast<MCElementalDynamicsType*>(Type)->MaxAccel;
    return result;
}

auto MCElementalDynamics::Update() -> int32_t
{
    auto* elemental = static_cast<MCElemental*>(Me);
    const auto* dynType = static_cast<MCElementalDynamicsType*>(Type);
    auto* controlData = static_cast<MCElementalControlData*>(elemental->Control->ControlData);

    // Turn about the up axis (sine and cosine both stored as floats).
    const float turn = static_cast<float>(static_cast<double>(controlData->Rotate) * CONTROL_STEP *
                                          dynType->MaxElementalYawRate * FrameLength);
    const MCFrameOfRef frame = elemental->GetFrame();
    const double angle = static_cast<double>(turn) * DEGREES_TO_RADIANS;
    const float s = static_cast<float>(std::sin(angle));
    const float c = static_cast<float>(std::cos(angle));
    MCFrameOfRef turned = frame;
    turned.I.X = frame.J.X * s + c * frame.I.X;
    turned.I.Y = frame.J.Y * s + frame.I.Y * c;
    turned.I.Z = frame.J.Z * s + frame.I.Z * c;
    turned.J.X = frame.J.X * c - frame.I.X * s;
    turned.J.Y = frame.J.Y * c - frame.I.Y * s;
    turned.J.Z = frame.J.Z * c - frame.I.Z * s;
    elemental->SetFrame(turned);

    // The throttle is all or nothing.
    auto* actor = static_cast<MCElementalActor*>(elemental->GetAppearance());
    float throttle = static_cast<float>(controlData->Throttle);

    if (throttle != 0.0f && throttle != 100.0f)
    {
        controlData->Throttle = 0;
        throttle = 0.0f;
    }

    throttle = static_cast<float>(throttle * 0.01);

    // Port fix: the original read the gesture before testing the actor for null.
    if (actor != nullptr && actor->CurrentGesture != 2)
    {
        actor->SetGestureGoal(throttle == 0.0f ? 0 : 1);
    }

    // Accelerate toward the throttle's share of the top speed.
    const float speedChange = static_cast<float>(static_cast<double>(throttle) * dynType->MaxVelocity - Velocity);

    if ((speedChange < 0.0f && 0.0f < Accel) || (0.0f < speedChange && Accel < 0.0f))
    {
        Accel = -Accel;
    }

    double step = static_cast<double>(FrameLength) * Accel;

    if (std::fabs(speedChange) < std::fabs(step))
    {
        step = speedChange;
    }

    Velocity = static_cast<float>(step + Velocity);
    return 1;
}
