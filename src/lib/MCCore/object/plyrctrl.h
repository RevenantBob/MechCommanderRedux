#pragma once

#include "object/control.h"

/// <summary>
/// A mover steered straight from the keyboard: a debug control that turns the last key (<c>keySetting</c>) into
/// throttle, turn, torso and arm requests, and for a mech also plays gestures, jumps and hit reactions.
/// </summary>
/// <remarks>Original source: <c>object\plyrctrl.cpp</c>, <c>object\plyrctrl.h</c>; 0x0c bytes.</remarks>
class MCPlayerControl : public MCControl
{
public:
    /// <summary>Control::init.</summary>
    int32_t Init(MCGameObject* object, int32_t unused) override;
    /// <summary>Does nothing.</summary>
    void Destroy() override;
    /// <summary>
    /// Resets the control data, then applies <c>keySetting</c> by the object's class (mech, ground vehicle,
    /// elemental) and clears it. Most keys need <c>turn</c> of 2 or more.
    /// </summary>
    /// <returns>1.</returns>
    int32_t Update() override;
    uint32_t GetControlClass() override { return 1; }
};
