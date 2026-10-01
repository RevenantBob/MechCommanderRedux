#pragma once

#include "object/dyn.h"

/// <summary>An elemental type's turn rate and speed limits (the FIT's "ElementalDynamics" block).</summary>
/// <remarks>Original source: <c>object\elemdyn.cpp</c>; 0x10 bytes.</remarks>
class ElementalDynamicsType : public DynamicsType
{
public:
    /// <summary>Reads the "ElementalDynamics" block.</summary>
    /// <remarks>MCX.EXE @ 0x0065a520</remarks>
    int32_t init(FitIniFile* objFile) override;
    /// <remarks>MCX.EXE @ 0x0065a510</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x0065a580</remarks>
    Dynamics* createInstance() override;

    /// <summary>FIT "maxElementalYawRate".</summary>
    int32_t maxElementalYawRate = 0; // +0x04
    /// <summary>FIT "maxAccel".</summary>
    float maxAccel = 0.0f; // +0x08
    /// <summary>FIT "maxVelocity".</summary>
    float maxVelocity = 0.0f; // +0x0c
};

/// <summary>Turns an elemental and accelerates it toward its throttle's speed each frame.</summary>
/// <remarks>Original source: <c>object\elemdyn.cpp</c>, <c>object\elemdyn.h</c>; 0x18 bytes.</remarks>
class ElementalDynamics : public Dynamics
{
public:
    /// <summary>Takes the acceleration from the type.</summary>
    /// <remarks>MCX.EXE @ 0x0065a5e0</remarks>
    int32_t init(DynamicsType* dynType, GameObject* object) override;
    /// <remarks>MCX.EXE @ 0x0065a5d0</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x0065a610</remarks>
    int32_t update() override;
    /// <remarks>MCX.EXE @ 0x0065a5c0 (inline in <c>object\elemdyn.h</c>)</remarks>
    uint32_t getDynamicsClass() override { return 3; }
    /// <remarks>MCX.EXE @ 0x0065a5b0 (inline in <c>object\elemdyn.h</c>)</remarks>
    float getVelocity() override { return velocity; }

    /// <summary>Zeroed by ElementalDynamicsType::createInstance; not otherwise used.</summary>
    int32_t unknown0C = 0; // +0x0c
    /// <summary>Acceleration (the type's maxAccel); its sign flips toward the target speed.</summary>
    float accel = 0.0f; // +0x10
    /// <summary>Current speed.</summary>
    float velocity = 0.0f; // +0x14
};
