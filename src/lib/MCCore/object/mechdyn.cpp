#include "stdafx.h"
#include "object/mechdyn.h"
#include "lib/MCFitIniFile.h"
#include "main/main.h"
#include "object/MCControl.h"
#include "object/mech.h"
#include "object/mechctrl.h"
#include "sprite/MCMechActor.h"

// The original turns on the x87 stack: the sums below that it keeps at extended precision are done in double.

namespace
{
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;

    /// <summary>A control's signed 1/64 steps.</summary>
    constexpr float CONTROL_STEP = 0.015625f;
}

auto MCMechDynamicsType::Destroy() -> void
{
}

auto MCMechDynamicsType::Init(MCFitIniFile* objFile) -> int32_t
{
    int32_t result = objFile->SeekBlock("MechDynamics");

    if (result != 0)
    {
        return result;
    }

    if ((result = objFile->ReadIdLong("maxTorsoYawRate", MaxTorsoYawRate)) != 0)
    {
        return result;
    }

    if ((result = objFile->ReadIdLong("maxTorsoYaw", MaxTorsoYaw)) != 0)
    {
        return result;
    }

    if ((result = objFile->ReadIdLong("maxArmYaw", MaxArmYaw)) != 0)
    {
        return result;
    }

    if ((result = objFile->ReadIdLong("maxMechYawRate", MaxMechYawRate)) != 0)
    {
        return result;
    }

    if (MaxMechYawRate < 720)
    {
        MaxMechYawRate = 720;
    }

    if (objFile->ReadIdLong("maxMechPivotRate", MaxMechPivotRate) != 0)
    {
        MaxMechPivotRate = static_cast<int32_t>(static_cast<float>(MaxMechYawRate) * 0.25f);
    }

    if ((result = objFile->ReadIdLong("maxLeftArmYawRate", MaxLeftArmYawRate)) != 0)
    {
        return result;
    }

    if ((result = objFile->ReadIdLong("maxRightArmYawRate", MaxRightArmYawRate)) != 0)
    {
        return result;
    }

    if ((result = objFile->ReadIdFloat("maxAccel", MaxAccel)) != 0)
    {
        return result;
    }

    return objFile->ReadIdFloat("maxVelocity", MaxVelocity);
}

auto MCMechDynamicsType::CreateInstance() -> MCDynamics*
{
    return new MCMechDynamics;
}

auto MCMechDynamics::Destroy() -> void
{
}

auto MCMechDynamics::Init(MCDynamicsType* dynType, MCGameObject* object) -> int32_t
{
    return MCDynamics::Init(dynType, object);
}

auto MCMechDynamics::Update() -> int32_t
{
    auto* mech = static_cast<MCBattleMech*>(Me);
    const auto* dynType = static_cast<MCMechDynamicsType*>(Type);
    auto* controlData = static_cast<MCMechControlData*>(mech->Control->ControlData);

    // This frame's turns, in degrees.
    const int32_t yawRate = controlData->Pivot != 0 ? dynType->MaxMechPivotRate : dynType->MaxMechYawRate;
    const float bodyTurn = static_cast<float>(static_cast<double>(FrameLength) *
                                              (static_cast<double>(controlData->Rotate) * CONTROL_STEP * yawRate));
    const float torsoTurn = static_cast<float>(static_cast<double>(controlData->TorsoRotate) * CONTROL_STEP *
                                               dynType->MaxTorsoYawRate * FrameLength);
    float rightArmTurn = static_cast<float>(static_cast<double>(controlData->RightArmRotate) * CONTROL_STEP *
                                            dynType->MaxRightArmYawRate * FrameLength);
    float leftArmTurn = static_cast<float>(static_cast<double>(controlData->LeftArmRotate) * CONTROL_STEP *
                                           dynType->MaxLeftArmYawRate * FrameLength);

    int32_t bodyLocked = 0;
    int32_t upperLocked = 0;
    auto* actor = static_cast<MCMechActor*>(mech->GetAppearance());

    if (actor != nullptr)
    {
        bodyLocked = actor->BodyTurnLocked;
        upperLocked = actor->UpperBodyLocked;
        const int32_t gestureGoal = controlData->GestureGoal;

        if (gestureGoal != -1)
        {
            actor->SetGestureGoal(gestureGoal);
        }
    }

    // The torso turns up to its limit either way.
    if (torsoTurn != 0.0f && upperLocked == 0)
    {
        const float limit = static_cast<float>(dynType->MaxTorsoYaw);
        const float negLimit = static_cast<float>(-dynType->MaxTorsoYaw);
        double current = mech->TorsoRotation;
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

        mech->TorsoRotation = static_cast<float>(current + turn);
    }

    // The arms stop dead at their limits.
    if (rightArmTurn != 0.0f && upperLocked == 0)
    {
        const float limit = static_cast<float>(dynType->MaxArmYaw);
        const float negLimit = static_cast<float>(-dynType->MaxArmYaw);
        double current = mech->RightArmRotation;

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

        mech->RightArmRotation = static_cast<float>(current + rightArmTurn);
    }

    if (leftArmTurn != 0.0f && upperLocked == 0)
    {
        const float negLimit = static_cast<float>(-dynType->MaxArmYaw);
        const float limit = static_cast<float>(dynType->MaxArmYaw);
        double current = mech->LeftArmRotation;

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

        mech->LeftArmRotation = static_cast<float>(current + leftArmTurn);
    }

    // The body turns about its up axis (the sine is stored as a float, the cosine isn't).
    if (bodyLocked == 0)
    {
        const MCFrameOfRef frame = mech->GetFrame();
        const double angle = static_cast<double>(bodyTurn) * DEGREES_TO_RADIANS;
        const float s = static_cast<float>(std::sin(angle));
        const double c = std::cos(angle);
        MCFrameOfRef turned = frame;
        turned.I.X = static_cast<float>(c * frame.I.X + static_cast<double>(frame.J.X) * s);
        turned.I.Y = static_cast<float>(frame.I.Y * c) + frame.J.Y * s;
        turned.I.Z = static_cast<float>(frame.I.Z * c) + frame.J.Z * s;
        turned.J.X = static_cast<float>(frame.J.X * c) - frame.I.X * s;
        turned.J.Y = static_cast<float>(frame.J.Y * c) - frame.I.Y * s;
        turned.J.Z = static_cast<float>(c * frame.J.Z - static_cast<double>(frame.I.Z * s));
        mech->SetFrame(turned);
    }

    return 1;
}
