#pragma once

#include "object/MCDynamics.h"

/// <summary>An elemental type's turn rate and speed limits (the FIT's "ElementalDynamics" block).</summary>
/// <remarks>Original source: <c>object\elemdyn.cpp</c>; 0x10 bytes.</remarks>
class MCElementalDynamicsType : public MCDynamicsType
{
public:
    /// <summary>Reads the "ElementalDynamics" block.</summary>
    int32_t Init(MCFitIniFile* objFile) override;
    void Destroy() override;
    MCDynamics* CreateInstance() override;

    /// <summary>FIT "maxElementalYawRate".</summary>
    int32_t MaxElementalYawRate = 0;
    /// <summary>FIT "maxAccel".</summary>
    float MaxAccel = 0.0f;
    /// <summary>FIT "maxVelocity".</summary>
    float MaxVelocity = 0.0f;
};

/// <summary>Turns an elemental and accelerates it toward its throttle's speed each frame.</summary>
/// <remarks>Original source: <c>object\elemdyn.cpp</c>, <c>object\elemdyn.h</c>; 0x18 bytes.</remarks>
class MCElementalDynamics : public MCDynamics
{
public:
    /// <summary>Takes the acceleration from the type.</summary>
    int32_t Init(MCDynamicsType* dynType, MCGameObject* object) override;
    void Destroy() override;
    int32_t Update() override;
    uint32_t GetDynamicsClass() override { return 3; }
    float GetVelocity() override { return Velocity; }

    /// <summary>Acceleration (the type's maxAccel); its sign flips toward the target speed.</summary>
    float Accel = 0.0f;
    /// <summary>Current speed.</summary>
    float Velocity = 0.0f;
};
