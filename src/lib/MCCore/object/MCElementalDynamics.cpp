#include "stdafx.h"
#include "object/MCElementalDynamics.h"
#include "main/MCMissionGlobals.h"
#include "object/MCControl.h"
#include "object/MCElementalControlData.h"
#include "object/MCElemental.h"
#include "object/MCElementalType.h"
#include "object/MCElementalGameSystem.h"
#include "sprite/MCElementalActor.h"

namespace
{
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;

    /// <summary>A control's signed 1/64 steps.</summary>
    constexpr float CONTROL_STEP = 0.015625f;
}

auto MCElementalDynamicsType::Create(MCFitIniFile& objFile)
    -> std::expected<std::unique_ptr<MCElementalDynamicsType>, MCFitError>
{
    if (const int32_t result = objFile.SeekBlock("ElementalDynamics"); result != 0)
    {
        return std::unexpected(static_cast<MCFitError>(result));
    }

    auto type = std::make_unique<MCElementalDynamicsType>();
    MCFitReader read(objFile);
    read.Value("maxElementalYawRate", type->MaxElementalYawRate);
    read.Value("maxAccel", type->MaxAccel);
    read.Value("maxVelocity", type->MaxVelocity);

    if (read.Failed())
    {
        return std::unexpected(read.Error());
    }

    return type;
}

auto MCElementalDynamicsType::CreateInstance(MCGameObject& object) -> std::unique_ptr<MCDynamics>
{
    return std::make_unique<MCElementalDynamics>(*this, object);
}

MCElementalDynamics::MCElementalDynamics(MCElementalDynamicsType& type, MCGameObject& object)
    : MCDynamics(type, object), Accel(type.MaxAccel)
{
}

auto MCElementalDynamics::Update() -> int32_t
{
    auto* elemental = static_cast<MCElemental*>(Me);
    const auto* dynType = static_cast<MCElementalDynamicsType*>(Type);
    auto* controlData = static_cast<MCElementalControlData*>(elemental->Control->ControlData.get());

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
