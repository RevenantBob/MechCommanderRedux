#pragma once

#include "object/MCControl.h"

/// <summary>
/// A mover steered straight from the keyboard: a debug control that turns the last key (<c>KeySetting</c>) into
/// throttle, turn, torso and arm requests, and for a mech also plays gestures, jumps and hit reactions.
/// </summary>
/// <remarks>Original source: <c>object\plyrctrl.cpp</c>, <c>object\plyrctrl.h</c>.</remarks>
class MCPlayerControl : public MCControl
{
public:
    using MCControl::MCControl;

    /// <summary>
    /// Resets the control data, then applies <c>KeySetting</c> by the object's class (mech, ground vehicle,
    /// elemental) and clears it. Every key needs turn 2 or later.
    /// </summary>
    /// <returns>1.</returns>
    int32_t Update() override;
    uint32_t GetControlClass() override { return 1; }
};
