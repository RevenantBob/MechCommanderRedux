#pragma once

#include "object/dyn.h"

/// <summary>A mech type's turn rates and speed limits (the FIT's "MechDynamics" block).</summary>
/// <remarks>Original source: <c>object\mechdyn.cpp</c>, <c>object\mechdyn.h</c>; 0x28 bytes. Rates are in degrees
/// per second.</remarks>
class MechDynamicsType : public DynamicsType
{
public:
    /// <summary>Reads the "MechDynamics" block. The body yaw rate is at least 720; a missing pivot rate is a
    /// quarter of it.</summary>
    /// <remarks>MCX.EXE @ 0x00683810</remarks>
    int32_t init(FitIniFile* objFile) override;
    /// <remarks>MCX.EXE @ 0x00683800</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x00683910</remarks>
    Dynamics* createInstance() override;
    /// <remarks>MCX.EXE @ 0x00675320 (inline in <c>object\mechdyn.h</c>)</remarks>
    uint32_t getDynamicsTypeClass() override { return 1; }

    /// <summary>FIT "maxTorsoYawRate".</summary>
    int32_t maxTorsoYawRate = 0; // +0x04
    /// <summary>FIT "maxMechYawRate", at least 720.</summary>
    int32_t maxMechYawRate = 0; // +0x08
    /// <summary>FIT "maxMechPivotRate" (a quarter of the yaw rate when missing).</summary>
    int32_t maxMechPivotRate = 0; // +0x0c
    /// <summary>FIT "maxRightArmYawRate".</summary>
    int32_t maxRightArmYawRate = 0; // +0x10
    /// <summary>FIT "maxLeftArmYawRate".</summary>
    int32_t maxLeftArmYawRate = 0; // +0x14
    /// <summary>FIT "maxAccel".</summary>
    float maxAccel = 0.0f; // +0x18
    /// <summary>FIT "maxVelocity".</summary>
    float maxVelocity = 0.0f; // +0x1c
    /// <summary>FIT "maxTorsoYaw": how far the torso turns either way.</summary>
    int32_t maxTorsoYaw = 0; // +0x20
    /// <summary>FIT "maxArmYaw": how far the arms turn either way.</summary>
    int32_t maxArmYaw = 0; // +0x24
};

/// <summary>Turns a mech's body, torso and arms each frame from its <see cref="MechControlData"/>.</summary>
/// <remarks>Original source: <c>object\mechdyn.cpp</c>, <c>object\mechdyn.h</c>; 0x18 bytes.</remarks>
class MechDynamics : public Dynamics
{
public:
    /// <remarks>MCX.EXE @ 0x00683960</remarks>
    int32_t init(DynamicsType* dynType, GameObject* object) override;
    /// <remarks>MCX.EXE @ 0x00683950</remarks>
    void destroy() override;
    /// <summary>
    /// Turns the torso and arms within the type's limits, starts the requested gesture, and (unless jumping or
    /// already set) rotates the mech's frame by the body turn.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00683980</remarks>
    int32_t update() override;
    /// <remarks>MCX.EXE @ 0x00683940 (inline in <c>object\mechdyn.h</c>)</remarks>
    uint32_t getDynamicsClass() override { return 1; }

    /// <summary>Zeroed by MechDynamicsType::createInstance; not otherwise used.</summary>
    int32_t unknown0C = 0; // +0x0c
    /// <summary>Zeroed by MechDynamicsType::createInstance; not otherwise used.</summary>
    int32_t unknown10 = 0; // +0x10
    /// <summary>Zeroed by MechDynamicsType::createInstance; not otherwise used.</summary>
    int32_t unknown14 = 0; // +0x14
};
