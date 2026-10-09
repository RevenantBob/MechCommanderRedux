#pragma once

#include "lib/MCFitIniFile.h"
#include "object/MCDynamics.h"

/// <summary>A mech type's turn rates and speed limits (the FIT's "MechDynamics" block). Rates are in degrees per
/// second.</summary>
/// <remarks>Original source: <c>object\mechdyn.cpp</c>, <c>object\mechdyn.h</c>.</remarks>
class MCMechDynamicsType : public MCDynamicsType
{
public:
    /// <summary>Reads the "MechDynamics" block. The body yaw rate is at least 720; a missing pivot rate is a
    /// quarter of it.</summary>
    static std::expected<std::unique_ptr<MCMechDynamicsType>, MCFitError> Create(MCFitIniFile& objFile);

    std::unique_ptr<MCDynamics> CreateInstance(MCGameObject& object) override;
    uint32_t GetDynamicsTypeClass() override { return 1; }

    /// <summary>FIT "maxTorsoYawRate".</summary>
    int32_t MaxTorsoYawRate = 0;
    /// <summary>FIT "maxMechYawRate", at least 720.</summary>
    int32_t MaxMechYawRate = 0;
    /// <summary>FIT "maxMechPivotRate" (a quarter of the yaw rate when missing).</summary>
    int32_t MaxMechPivotRate = 0;
    /// <summary>FIT "maxRightArmYawRate".</summary>
    int32_t MaxRightArmYawRate = 0;
    /// <summary>FIT "maxLeftArmYawRate".</summary>
    int32_t MaxLeftArmYawRate = 0;
    /// <summary>FIT "maxAccel".</summary>
    float MaxAccel = 0.0f;
    /// <summary>FIT "maxVelocity".</summary>
    float MaxVelocity = 0.0f;
    /// <summary>FIT "maxTorsoYaw": how far the torso turns either way.</summary>
    int32_t MaxTorsoYaw = 0;
    /// <summary>FIT "maxArmYaw": how far the arms turn either way.</summary>
    int32_t MaxArmYaw = 0;
};

/// <summary>Turns a mech's body, torso and arms each frame from its <see cref="MCMechControlData"/>.</summary>
/// <remarks>Original source: <c>object\mechdyn.cpp</c>, <c>object\mechdyn.h</c>.</remarks>
class MCMechDynamics : public MCDynamics
{
public:
    using MCDynamics::MCDynamics;

    /// <summary>
    /// Turns the torso and arms within the type's limits, starts the requested gesture, and (unless jumping or
    /// already set) rotates the mech's frame by the body turn.
    /// </summary>
    int32_t Update() override;
    uint32_t GetDynamicsClass() override { return 1; }
};
