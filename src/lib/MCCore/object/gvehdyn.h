#pragma once

#include "object/dyn.h"

/// <summary>A ground vehicle type's turn rates and speed limits (the FIT's "VehicleDynamics" block).</summary>
/// <remarks>Original source: <c>object\gvehdyn.cpp</c>, <c>object\gvehdyn.h</c>; 0x1c bytes.</remarks>
class MCGroundVehicleDynamicsType : public MCDynamicsType
{
public:
    /// <summary>
    /// Reads the "VehicleDynamics" block. The body yaw rate is at least 720; a missing pivot rate is a quarter of
    /// it; the FIT's maxAccel is replaced by five times the top speed.
    /// </summary>
    int32_t Init(MCFitIniFile* objFile) override;
    void Destroy() override;
    MCDynamics* CreateInstance() override;
    uint32_t GetDynamicsTypeClass() override { return 2; }

    /// <summary>FIT "maxTurretYawRate".</summary>
    int32_t MaxTurretYawRate = 0;
    /// <summary>FIT "maxVehicleYawRate", at least 720.</summary>
    int32_t MaxVehicleYawRate = 0;
    /// <summary>FIT "maxVehiclePivotRate" (a quarter of the yaw rate when missing).</summary>
    int32_t MaxVehiclePivotRate = 0;
    /// <summary>Five times maxVelocity (the FIT's "maxAccel" is read, then replaced).</summary>
    float MaxAccel = 0.0f;
    /// <summary>FIT "maxVelocity".</summary>
    float MaxVelocity = 0.0f;
    /// <summary>FIT "maxTurretYaw": how far the turret turns either way.</summary>
    int32_t MaxTurretYaw = 0;
};

/// <summary>Turns a ground vehicle and its turret, and accelerates it toward its throttle's speed, each frame.</summary>
/// <remarks>Original source: <c>object\gvehdyn.cpp</c>, <c>object\gvehdyn.h</c>; 0x1c bytes.</remarks>
class MCGroundVehicleDynamics : public MCDynamics
{
public:
    /// <summary>Takes the acceleration from the type.</summary>
    int32_t Init(MCDynamicsType* dynType, MCGameObject* object) override;
    void Destroy() override;
    int32_t Update() override;
    /// <summary>Stops dead.</summary>
    int32_t Brake() override;
    uint32_t GetDynamicsClass() override { return 2; }
    float GetVelocity() override { return Velocity; }

    /// <summary>Acceleration (the type's maxAccel); its sign flips toward the target speed.</summary>
    float Accel = 0.0f;
    /// <summary>Current speed.</summary>
    float Velocity = 0.0f;
};

/// <summary>The speed of a vehicle told to walk (<see cref="MCGroundVehicleControlData::Walk"/>).</summary>
extern float GvWalkSpeed;
