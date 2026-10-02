#include "stdafx.h"
#include "object/gvehdyn.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "object/control.h"
#include "object/gvehctrl.h"
#include "object/gvehicl.h"
#include "sprite/gvactor.h"

// The original turns on the x87 stack: the values it keeps at extended precision are doubles here.

float gvWalkSpeed = 0.0f;

namespace
{
    /// <summary>Degrees to radians, as MCX.EXE stores it (MCX.EXE @ 0x0077c2a0; a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;

    /// <summary>A control's signed 1/64 steps (MCX.EXE @ 0x0077d3f8).</summary>
    constexpr float CONTROL_STEP = 0.015625f;
}

auto GroundVehicleDynamicsType::destroy() -> void
{
}

auto GroundVehicleDynamicsType::init(FitIniFile* objFile) -> int32_t
{
    int32_t result = objFile->seekBlock("VehicleDynamics");

    if (result != 0)
    {
        return result;
    }

    if ((result = objFile->readIdLong("maxTurretYawRate", maxTurretYawRate)) != 0)
    {
        return result;
    }

    if ((result = objFile->readIdLong("maxTurretYaw", maxTurretYaw)) != 0)
    {
        return result;
    }

    if ((result = objFile->readIdLong("maxVehicleYawRate", maxVehicleYawRate)) != 0)
    {
        return result;
    }

    if (maxVehicleYawRate < 720)
    {
        maxVehicleYawRate = 720;
    }

    if (objFile->readIdLong("maxVehiclePivotRate", maxVehiclePivotRate) != 0)
    {
        maxVehiclePivotRate = static_cast<int32_t>(static_cast<float>(maxVehicleYawRate) * 0.25f);
    }

    if ((result = objFile->readIdFloat("maxAccel", maxAccel)) != 0)
    {
        return result;
    }

    if ((result = objFile->readIdFloat("maxVelocity", maxVelocity)) != 0)
    {
        return result;
    }

    maxAccel = maxVelocity * 5.0f;
    return 0;
}

auto GroundVehicleDynamicsType::createInstance() -> Dynamics*
{
    return new GroundVehicleDynamics;
}

auto GroundVehicleDynamics::destroy() -> void
{
}

auto GroundVehicleDynamics::init(DynamicsType* dynType, GameObject* object) -> int32_t
{
    const int32_t result = Dynamics::init(dynType, object);
    accel = static_cast<GroundVehicleDynamicsType*>(type)->maxAccel;
    return result;
}

auto GroundVehicleDynamics::update() -> int32_t
{
    auto* vehicle = static_cast<GroundVehicle*>(me);
    const auto* dynType = static_cast<GroundVehicleDynamicsType*>(type);
    auto* controlData = static_cast<GroundVehicleControlData*>(vehicle->control->controlData);

    // This frame's turns, in degrees.
    const int32_t yawRate = controlData->pivot != 0 ? dynType->maxVehiclePivotRate : dynType->maxVehicleYawRate;
    const float bodyTurn = static_cast<float>(static_cast<double>(frameLength) *
                                              (static_cast<double>(controlData->rotate) * CONTROL_STEP * yawRate));
    double turretTurn =
        static_cast<double>(controlData->turretRotate) * CONTROL_STEP * dynType->maxTurretYawRate * frameLength;

    // The turret turns up to its limit either way.
    if (turretTurn != 0.0)
    {
        const float limit = static_cast<float>(dynType->maxTurretYaw);
        const float negLimit = static_cast<float>(-dynType->maxTurretYaw);
        float current = vehicle->turretRotation;

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

        vehicle->turretRotation = static_cast<float>(current + turretTurn);
    }

    // The body turns about its up axis (the sine is stored as a float, the cosine isn't).
    const frame_of_ref frame = vehicle->getFrame();
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
    vehicle->setFrame(turned);

    // Moving: the appearance animates.
    auto* appearance = static_cast<GVAppearance*>(vehicle->getAppearance());
    const float throttle = static_cast<float>(static_cast<float>(controlData->throttle) * 0.01);

    if (appearance != nullptr && throttle != 0.0f && dynType->maxVelocity != 0.0f)
    {
        appearance->setTypeId(GV_ACTOR_STATE_DAMAGED);
        appearance->update();
    }

    // Accelerate toward the throttle's share of the top (or walking) speed.
    float topSpeed = dynType->maxVelocity;

    if (controlData->walk != 0)
    {
        topSpeed = gvWalkSpeed;
    }

    const float speedChange = static_cast<float>(static_cast<double>(topSpeed) * throttle - velocity);

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

auto GroundVehicleDynamics::brake() -> int32_t
{
    velocity = 0.0f;
    return 0;
}
