#include "stdafx.h"
#include "object/gvehdyn.h"
#include "lib/MCFitIniFile.h"
#include "main/main.h"
#include "object/control.h"
#include "object/gvehctrl.h"
#include "object/gvehicl.h"
#include "sprite/MCGVAppearance.h"

// The original turns on the x87 stack: the values it keeps at extended precision are doubles here.

float GvWalkSpeed = 0.0f;

namespace
{
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;

    /// <summary>A control's signed 1/64 steps.</summary>
    constexpr float CONTROL_STEP = 0.015625f;
}

auto MCGroundVehicleDynamicsType::Destroy() -> void
{
}

auto MCGroundVehicleDynamicsType::Init(MCFitIniFile* objFile) -> int32_t
{
    int32_t result = objFile->SeekBlock("VehicleDynamics");

    if (result != 0)
    {
        return result;
    }

    if ((result = objFile->ReadIdLong("maxTurretYawRate", MaxTurretYawRate)) != 0)
    {
        return result;
    }

    if ((result = objFile->ReadIdLong("maxTurretYaw", MaxTurretYaw)) != 0)
    {
        return result;
    }

    if ((result = objFile->ReadIdLong("maxVehicleYawRate", MaxVehicleYawRate)) != 0)
    {
        return result;
    }

    if (MaxVehicleYawRate < 720)
    {
        MaxVehicleYawRate = 720;
    }

    if (objFile->ReadIdLong("maxVehiclePivotRate", MaxVehiclePivotRate) != 0)
    {
        MaxVehiclePivotRate = static_cast<int32_t>(static_cast<float>(MaxVehicleYawRate) * 0.25f);
    }

    if ((result = objFile->ReadIdFloat("maxAccel", MaxAccel)) != 0)
    {
        return result;
    }

    if ((result = objFile->ReadIdFloat("maxVelocity", MaxVelocity)) != 0)
    {
        return result;
    }

    MaxAccel = MaxVelocity * 5.0f;
    return 0;
}

auto MCGroundVehicleDynamicsType::CreateInstance() -> MCDynamics*
{
    return new MCGroundVehicleDynamics;
}

auto MCGroundVehicleDynamics::Destroy() -> void
{
}

auto MCGroundVehicleDynamics::Init(MCDynamicsType* dynType, MCGameObject* object) -> int32_t
{
    const int32_t result = MCDynamics::Init(dynType, object);
    Accel = static_cast<MCGroundVehicleDynamicsType*>(Type)->MaxAccel;
    return result;
}

auto MCGroundVehicleDynamics::Update() -> int32_t
{
    auto* vehicle = static_cast<MCGroundVehicle*>(Me);
    const auto* dynType = static_cast<MCGroundVehicleDynamicsType*>(Type);
    auto* controlData = static_cast<MCGroundVehicleControlData*>(vehicle->Control->ControlData);

    // This frame's turns, in degrees.
    const int32_t yawRate = controlData->Pivot != 0 ? dynType->MaxVehiclePivotRate : dynType->MaxVehicleYawRate;
    const float bodyTurn = static_cast<float>(static_cast<double>(FrameLength) *
                                              (static_cast<double>(controlData->Rotate) * CONTROL_STEP * yawRate));
    double turretTurn =
        static_cast<double>(controlData->TurretRotate) * CONTROL_STEP * dynType->MaxTurretYawRate * FrameLength;

    // The turret turns up to its limit either way.
    if (turretTurn != 0.0)
    {
        const float limit = static_cast<float>(dynType->MaxTurretYaw);
        const float negLimit = static_cast<float>(-dynType->MaxTurretYaw);
        float current = vehicle->TurretRotation;

        if (current > limit)
        {
            current = limit;
            turretTurn = 0.0;
        }

        if (current < negLimit)
        {
            current = negLimit;
            turretTurn = 0.0;
        }

        if (current + turretTurn > limit)
        {
            turretTurn = static_cast<double>(limit) - current;
        }

        if (current + turretTurn < negLimit)
        {
            turretTurn = static_cast<double>(negLimit) - current;
        }

        vehicle->TurretRotation = static_cast<float>(current + turretTurn);
    }

    // The body turns about its up axis (the sine is stored as a float, the cosine isn't).
    const MCFrameOfRef frame = vehicle->GetFrame();
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
    vehicle->SetFrame(turned);

    // Moving: the appearance animates.
    auto* appearance = static_cast<MCGVAppearance*>(vehicle->GetAppearance());
    const float throttle = static_cast<float>(static_cast<float>(controlData->Throttle) * 0.01);

    if (appearance != nullptr && throttle != 0.0f && dynType->MaxVelocity != 0.0f)
    {
        appearance->SetTypeId(MCGVActorState::Damaged);
        appearance->Update();
    }

    // Accelerate toward the throttle's share of the top (or walking) speed.
    float topSpeed = dynType->MaxVelocity;

    if (controlData->Walk != 0)
    {
        topSpeed = GvWalkSpeed;
    }

    const float speedChange = static_cast<float>(static_cast<double>(topSpeed) * throttle - Velocity);

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

auto MCGroundVehicleDynamics::Brake() -> int32_t
{
    Velocity = 0.0f;
    return 0;
}
