#pragma once

#include "lib/MCFitIniFile.h"
#include "object/MCDynamics.h"

/// <summary>An elemental type's turn rate and speed limits (the FIT's "ElementalDynamics" block).</summary>
/// <remarks>Original source: <c>object\elemdyn.cpp</c>.</remarks>
class MCElementalDynamicsType : public MCDynamicsType
{
public:
    /// <summary>Reads the "ElementalDynamics" block.</summary>
    static std::expected<std::unique_ptr<MCElementalDynamicsType>, MCFitError> Create(MCFitIniFile& objFile);

    std::unique_ptr<MCDynamics> CreateInstance(MCGameObject& object) override;

    /// <summary>FIT "maxElementalYawRate".</summary>
    int32_t MaxElementalYawRate = 0;
    /// <summary>FIT "maxAccel".</summary>
    float MaxAccel = 0.0f;
    /// <summary>FIT "maxVelocity".</summary>
    float MaxVelocity = 0.0f;
};

/// <summary>Turns an elemental and accelerates it toward its throttle's speed each frame.</summary>
/// <remarks>Original source: <c>object\elemdyn.cpp</c>, <c>object\elemdyn.h</c>.</remarks>
class MCElementalDynamics : public MCDynamics
{
public:
    /// <summary>Moves <paramref name="object"/> with the type's acceleration.</summary>
    MCElementalDynamics(MCElementalDynamicsType& type, MCGameObject& object);

    int32_t Update() override;
    uint32_t GetDynamicsClass() override { return 3; }
    float GetVelocity() override { return Velocity; }

    /// <summary>Acceleration (the type's maxAccel); its sign flips toward the target speed.</summary>
    float Accel = 0.0f;
    /// <summary>Current speed.</summary>
    float Velocity = 0.0f;
};
