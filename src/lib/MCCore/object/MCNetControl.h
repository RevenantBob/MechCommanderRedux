#pragma once

#include "object/MCControl.h"

class MCMechWarrior;

/// <summary>
/// A remote player's mech in a network game: the pilot's alarms run here, but movement and weapons come from the
/// network chunks.
/// </summary>
/// <remarks>Original source: <c>object\netctrl.cpp</c>, <c>object\netctrl.h</c>.</remarks>
class MCMechNetControl : public MCControl
{
public:
    /// <summary>Controls <paramref name="mech"/>, remembering its pilot and its type's dynamics type.</summary>
    explicit MCMechNetControl(MCGameObject& mech);

    /// <summary>
    /// Resets the control data, passes a blown arm on, applies the received weapon fire, critical hit and radio
    /// chunks, then checks a conscious pilot's alarms and runs the mech's network movement.
    /// </summary>
    /// <returns>1.</returns>
    int32_t Update() override;
    uint32_t GetControlClass() override { return 3; }

    /// <summary>The mech's pilot.</summary>
    MCMechWarrior* Pilot = nullptr;
    /// <summary>The mech type's dynamics type.</summary>
    MCDynamicsType* DynamicsType = nullptr;
};

/// <summary>A remote player's ground vehicle in a network game.</summary>
/// <remarks>Original source: <c>object\netctrl.cpp</c>, <c>object\netctrl.h</c>.</remarks>
class MCGroundVehicleNetControl : public MCControl
{
public:
    /// <summary>Controls <paramref name="vehicle"/>, remembering its pilot and its type's dynamics type.</summary>
    explicit MCGroundVehicleNetControl(MCGameObject& vehicle);

    /// <summary>As <see cref="MCMechNetControl::Update"/>, without the arms or the pilot state check.</summary>
    /// <returns>1.</returns>
    int32_t Update() override;
    uint32_t GetControlClass() override { return 3; }

    /// <summary>The vehicle's pilot.</summary>
    MCMechWarrior* Pilot = nullptr;
    /// <summary>The vehicle type's dynamics type.</summary>
    MCDynamicsType* DynamicsType = nullptr;
};
