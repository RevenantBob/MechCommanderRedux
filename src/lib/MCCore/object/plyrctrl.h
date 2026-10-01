#pragma once

#include "object/control.h"

/// <summary>
/// A mover steered straight from the keyboard: a debug control that turns the last key (<c>keySetting</c>) into
/// throttle, turn, torso and arm requests, and for a mech also plays gestures, jumps and hit reactions.
/// </summary>
/// <remarks>Original source: <c>object\plyrctrl.cpp</c>, <c>object\plyrctrl.h</c>; 0x0c bytes.</remarks>
class PlayerControl : public Control
{
public:
    /// <summary>Control::init.</summary>
    /// <remarks>MCX.EXE @ 0x00690db0</remarks>
    int32_t init(GameObject* object, int32_t unused) override;
    /// <summary>Does nothing.</summary>
    /// <remarks>MCX.EXE @ 0x00690da0</remarks>
    void destroy() override;
    /// <summary>
    /// Resets the control data, then applies <c>keySetting</c> by the object's class (mech, ground vehicle,
    /// elemental) and clears it. Most keys need <c>turn</c> of 2 or more.
    /// </summary>
    /// <returns>1.</returns>
    /// <remarks>MCX.EXE @ 0x00690dd0</remarks>
    int32_t update() override;
    /// <remarks>MCX.EXE @ 0x0065c090 (inline in <c>object\plyrctrl.h</c>)</remarks>
    uint32_t getControlClass() override { return 1; }
};
