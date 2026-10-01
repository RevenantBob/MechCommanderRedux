#pragma once

#include "object/dyn.h"

/// <summary>A ground vehicle type's turn rates and speed limits (the FIT's "VehicleDynamics" block).</summary>
/// <remarks>Original source: <c>object\gvehdyn.cpp</c>, <c>object\gvehdyn.h</c>; 0x1c bytes.</remarks>
class GroundVehicleDynamicsType : public DynamicsType
{
public:
    /// <summary>
    /// Reads the "VehicleDynamics" block. The body yaw rate is at least 720; a missing pivot rate is a quarter of
    /// it; the FIT's maxAccel is replaced by five times the top speed.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00668c80</remarks>
    int32_t init(FitIniFile* objFile) override;
    /// <remarks>MCX.EXE @ 0x00668c70</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x00668d50</remarks>
    Dynamics* createInstance() override;
    /// <remarks>MCX.EXE @ 0x00669850 (inline in <c>object\gvehdyn.h</c>)</remarks>
    uint32_t getDynamicsTypeClass() override { return 2; }

    /// <summary>FIT "maxTurretYawRate".</summary>
    int32_t maxTurretYawRate = 0; // +0x04
    /// <summary>FIT "maxVehicleYawRate", at least 720.</summary>
    int32_t maxVehicleYawRate = 0; // +0x08
    /// <summary>FIT "maxVehiclePivotRate" (a quarter of the yaw rate when missing).</summary>
    int32_t maxVehiclePivotRate = 0; // +0x0c
    /// <summary>Five times maxVelocity (the FIT's "maxAccel" is read, then replaced).</summary>
    float maxAccel = 0.0f; // +0x10
    /// <summary>FIT "maxVelocity".</summary>
    float maxVelocity = 0.0f; // +0x14
    /// <summary>FIT "maxTurretYaw": how far the turret turns either way.</summary>
    int32_t maxTurretYaw = 0; // +0x18
};

/// <summary>Turns a ground vehicle and its turret, and accelerates it toward its throttle's speed, each frame.</summary>
/// <remarks>Original source: <c>object\gvehdyn.cpp</c>, <c>object\gvehdyn.h</c>; 0x1c bytes.</remarks>
class GroundVehicleDynamics : public Dynamics
{
public:
    /// <summary>Takes the acceleration from the type.</summary>
    /// <remarks>MCX.EXE @ 0x00668db0</remarks>
    int32_t init(DynamicsType* dynType, GameObject* object) override;
    /// <remarks>MCX.EXE @ 0x00668da0</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x00668de0</remarks>
    int32_t update() override;
    /// <summary>Stops dead.</summary>
    /// <remarks>MCX.EXE @ 0x00669100</remarks>
    int32_t brake() override;
    /// <remarks>MCX.EXE @ 0x00668d90 (inline in <c>object\gvehdyn.h</c>)</remarks>
    uint32_t getDynamicsClass() override { return 2; }
    /// <remarks>MCX.EXE @ 0x00668d80 (inline in <c>object\gvehdyn.h</c>)</remarks>
    float getVelocity() override { return velocity; }

    /// <summary>Zeroed by GroundVehicleDynamicsType::createInstance; not otherwise used.</summary>
    int32_t unknown0C = 0; // +0x0c
    /// <summary>Zeroed by GroundVehicleDynamicsType::createInstance; not otherwise used.</summary>
    int32_t unknown10 = 0; // +0x10
    /// <summary>Acceleration (the type's maxAccel); its sign flips toward the target speed.</summary>
    float accel = 0.0f; // +0x14
    /// <summary>Current speed.</summary>
    float velocity = 0.0f; // +0x18
};

/// <summary>The speed of a vehicle told to walk (<see cref="GroundVehicleControlData::walk"/>).</summary>
extern float gvWalkSpeed;
