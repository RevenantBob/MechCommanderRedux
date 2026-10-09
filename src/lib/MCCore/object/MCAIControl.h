#pragma once

#include "object/MCControl.h"

class MCMechWarrior;

/// <summary>A mech driven by its pilot's AI: each update runs the pilot's brain and moves the mech.</summary>
/// <remarks>Original source: <c>object\aictrl.cpp</c>, <c>object\aictrl.h</c>.</remarks>
class MCMechAIControl : public MCControl
{
public:
    /// <summary>Controls <paramref name="mech"/>, remembering its pilot and its type's dynamics type.</summary>
    explicit MCMechAIControl(MCGameObject& mech);

    /// <summary>
    /// Resets the control data; for an awake mech, passes a blown arm on to the control data, updates its
    /// damage-taken rate, then, if it isn't disabled and its pilot is alive (under 6 wounds) and at the controls
    /// (state other than 3, 5, 6), lets the pilot think and moves the mech; otherwise moves it only while it is
    /// shutting down or being disabled this frame.
    /// </summary>
    /// <returns>1.</returns>
    int32_t Update() override;
    uint32_t GetControlClass() override { return 2; }

    /// <summary>The mech's pilot.</summary>
    MCMechWarrior* Pilot = nullptr;
    /// <summary>The mech type's dynamics type.</summary>
    MCDynamicsType* DynamicsType = nullptr;
};

/// <summary>A ground vehicle driven by its pilot's AI.</summary>
/// <remarks>Original source: <c>object\aictrl.cpp</c>, <c>object\aictrl.h</c>.</remarks>
class MCGroundVehicleAIControl : public MCControl
{
public:
    /// <summary>Controls <paramref name="vehicle"/>, remembering its pilot and its type's dynamics type.</summary>
    explicit MCGroundVehicleAIControl(MCGameObject& vehicle);

    /// <summary>As <see cref="MCMechAIControl::Update"/>, without the arms or the pilot state check.</summary>
    /// <returns>1.</returns>
    int32_t Update() override;
    uint32_t GetControlClass() override { return 2; }

    /// <summary>The vehicle's pilot.</summary>
    MCMechWarrior* Pilot = nullptr;
    /// <summary>The vehicle type's dynamics type.</summary>
    MCDynamicsType* DynamicsType = nullptr;
};

/// <summary>An elemental driven by its pilot's AI.</summary>
/// <remarks>Original source: <c>object\aictrl.cpp</c>, <c>object\aictrl.h</c>.</remarks>
class MCElementalAIControl : public MCControl
{
public:
    /// <summary>Controls <paramref name="elemental"/>, remembering its pilot and its type's dynamics type.</summary>
    explicit MCElementalAIControl(MCGameObject& elemental);

    /// <summary>As <see cref="MCGroundVehicleAIControl::Update"/>.</summary>
    /// <returns>1.</returns>
    int32_t Update() override;
    uint32_t GetControlClass() override { return 2; }

    /// <summary>The elemental's pilot.</summary>
    MCMechWarrior* Pilot = nullptr;
    /// <summary>The elemental type's dynamics type.</summary>
    MCDynamicsType* DynamicsType = nullptr;
};
