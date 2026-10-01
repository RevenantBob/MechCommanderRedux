#pragma once

#include "object/control.h"

class MechWarrior;

/// <summary>
/// A remote player's mech in a network game: the pilot's alarms run here, but movement and weapons come from the
/// network chunks.
/// </summary>
/// <remarks>Original source: <c>object\netctrl.cpp</c>, <c>object\netctrl.h</c>; 0x14 bytes.</remarks>
class MechNetControl : public Control
{
public:
    /// <summary>
    /// Resets the control data, passes pending torso/arm requests on, applies the queued weapon fire, critical hit
    /// and radio chunks, then checks a conscious pilot's alarms and runs the mech's network movement.
    /// </summary>
    /// <returns>1.</returns>
    /// <remarks>MCX.EXE @ 0x0068cf30</remarks>
    int32_t update() override;
    /// <remarks>MCX.EXE @ 0x006770b0 (inline in <c>object\netctrl.h</c>)</remarks>
    uint32_t getControlClass() override { return 3; }
    /// <summary>Control::init, then remembers the mech's pilot and its type's dynamics type.</summary>
    /// <remarks>MCX.EXE @ 0x0068cef0</remarks>
    virtual int32_t init(GameObject* object);
    using Control::init;

    /// <summary>The mech's pilot.</summary>
    MechWarrior* pilot = nullptr; // +0x0c
    /// <summary>The mech type's dynamics type.</summary>
    DynamicsType* dynamicsType = nullptr; // +0x10
};

/// <summary>A remote player's ground vehicle in a network game.</summary>
/// <remarks>Original source: <c>object\netctrl.cpp</c>, <c>object\netctrl.h</c>; 0x14 bytes.</remarks>
class GroundVehicleNetControl : public Control
{
public:
    /// <summary>As MechNetControl::update, without the pilot state check; clears the vehicle's +0x8b0.</summary>
    /// <returns>1.</returns>
    /// <remarks>MCX.EXE @ 0x0068d060 (unnamed in Ghidra)</remarks>
    int32_t update() override;
    /// <remarks>MCX.EXE @ 0x0066ab70 (inline in <c>object\netctrl.h</c>)</remarks>
    uint32_t getControlClass() override { return 3; }
    /// <summary>Control::init, then remembers the vehicle's pilot and its type's dynamics type.</summary>
    /// <remarks>MCX.EXE @ 0x0068d020</remarks>
    virtual int32_t init(GameObject* object);
    using Control::init;

    /// <summary>The vehicle's pilot.</summary>
    MechWarrior* pilot = nullptr; // +0x0c
    /// <summary>The vehicle type's dynamics type.</summary>
    DynamicsType* dynamicsType = nullptr; // +0x10
};
