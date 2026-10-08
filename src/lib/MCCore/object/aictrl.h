#pragma once

#include "object/MCControl.h"

class MCMechWarrior;

/// <summary>A mech driven by its pilot's AI: each update runs the pilot's brain and moves the mech.</summary>
/// <remarks>Original source: <c>object\aictrl.cpp</c>, <c>object\aictrl.h</c>; 0x14 bytes.</remarks>
class MCMechAIControl : public MCControl
{
public:
    /// <summary>
    /// Resets the control data; for an awake mech, passes its pending requests (+0x8d0, +0x8d4) to the control
    /// data, updates its damage-taken rate, then, if it isn't disabled and its pilot is alive (under 6 wounds) and at
    /// the controls (state other than 3, 5, 6), lets the pilot think and moves the mech; otherwise moves it only
    /// while Mover +0x17c or +0x184 is set.
    /// </summary>
    /// <returns>1.</returns>
    int32_t Update() override;
    uint32_t GetControlClass() override { return 2; }
    /// <summary>Control::init, then remembers the mech's pilot and its type's dynamics type.</summary>
    virtual int32_t Init(MCGameObject* object);
    using MCControl::Init;

    /// <summary>The mech's pilot.</summary>
    MCMechWarrior* Pilot = nullptr;
    /// <summary>The mech type's dynamics type.</summary>
    MCDynamicsType* DynamicsType = nullptr;
};

/// <summary>A ground vehicle driven by its pilot's AI.</summary>
/// <remarks>Original source: <c>object\aictrl.cpp</c>, <c>object\aictrl.h</c>; 0x14 bytes.</remarks>
class MCGroundVehicleAIControl : public MCControl
{
public:
    /// <summary>As MechAIControl::update, without the pilot state check; clears the vehicle's +0x8b0.</summary>
    /// <returns>1.</returns>
    int32_t Update() override;
    uint32_t GetControlClass() override { return 2; }
    /// <summary>Control::init, then remembers the vehicle's pilot and its type's dynamics type.</summary>
    virtual int32_t Init(MCGameObject* object);
    using MCControl::Init;

    /// <summary>The vehicle's pilot.</summary>
    MCMechWarrior* Pilot = nullptr;
    /// <summary>The vehicle type's dynamics type.</summary>
    MCDynamicsType* DynamicsType = nullptr;
};

/// <summary>An elemental driven by its pilot's AI.</summary>
/// <remarks>Original source: <c>object\aictrl.cpp</c>, <c>object\aictrl.h</c>; 0x14 bytes.</remarks>
class MCElementalAIControl : public MCControl
{
public:
    /// <summary>As GroundVehicleAIControl::update, with no pending requests to pass on.</summary>
    /// <returns>1.</returns>
    int32_t Update() override;
    uint32_t GetControlClass() override { return 2; }
    /// <summary>Control::init, then remembers the elemental's pilot and its type's dynamics type.</summary>
    virtual int32_t Init(MCGameObject* object);
    using MCControl::Init;

    /// <summary>The elemental's pilot.</summary>
    MCMechWarrior* Pilot = nullptr;
    /// <summary>The elemental type's dynamics type.</summary>
    MCDynamicsType* DynamicsType = nullptr;
};
