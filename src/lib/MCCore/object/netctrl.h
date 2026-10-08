#pragma once

#include "object/MCControl.h"

class MCMechWarrior;

/// <summary>
/// A remote player's mech in a network game: the pilot's alarms run here, but movement and weapons come from the
/// network chunks.
/// </summary>
/// <remarks>Original source: <c>object\netctrl.cpp</c>, <c>object\netctrl.h</c>; 0x14 bytes.</remarks>
class MCMechNetControl : public MCControl
{
public:
    /// <summary>
    /// Resets the control data, passes pending torso/arm requests on, applies the queued weapon fire, critical hit
    /// and radio chunks, then checks a conscious pilot's alarms and runs the mech's network movement.
    /// </summary>
    /// <returns>1.</returns>
    int32_t Update() override;
    uint32_t GetControlClass() override { return 3; }
    /// <summary>Control::init, then remembers the mech's pilot and its type's dynamics type.</summary>
    virtual int32_t Init(MCGameObject* object);
    using MCControl::Init;

    /// <summary>The mech's pilot.</summary>
    MCMechWarrior* Pilot = nullptr;
    /// <summary>The mech type's dynamics type.</summary>
    MCDynamicsType* DynamicsType = nullptr;
};

/// <summary>A remote player's ground vehicle in a network game.</summary>
/// <remarks>Original source: <c>object\netctrl.cpp</c>, <c>object\netctrl.h</c>; 0x14 bytes.</remarks>
class MCGroundVehicleNetControl : public MCControl
{
public:
    /// <summary>As MechNetControl::update, without the pilot state check; clears the vehicle's +0x8b0.</summary>
    /// <returns>1.</returns>
    int32_t Update() override;
    uint32_t GetControlClass() override { return 3; }
    /// <summary>Control::init, then remembers the vehicle's pilot and its type's dynamics type.</summary>
    virtual int32_t Init(MCGameObject* object);
    using MCControl::Init;

    /// <summary>The vehicle's pilot.</summary>
    MCMechWarrior* Pilot = nullptr;
    /// <summary>The vehicle type's dynamics type.</summary>
    MCDynamicsType* DynamicsType = nullptr;
};
