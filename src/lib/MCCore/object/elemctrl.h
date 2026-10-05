#pragma once

#include "object/control.h"

/// <summary>An elemental's control requests for one frame.</summary>
/// <remarks>Original source: <c>object\elemctrl.cpp</c>, <c>object\elemctrl.h</c>; 0x14 bytes.</remarks>
class ElementalControlData : public ControlData
{
public:
    /// <remarks>MCX.EXE @ 0x0065a4e0</remarks>
    int32_t init(int32_t unused) override;
    /// <remarks>MCX.EXE @ 0x0065a4f0</remarks>
    void destroy() override;
    /// <summary>+0x04 back to -1, the jump and rotate requests cleared.</summary>
    /// <remarks>MCX.EXE @ 0x0065a500</remarks>
    void reset() override;
    /// <remarks>MCX.EXE @ 0x0065c080 (inline in <c>object\elemctrl.h</c>)</remarks>
    uint32_t getControlDataClass() override { return 3; }

    /// <summary>Set by Elemental::updateJump when a jump is due; Elemental::update then sets the actor's jump up.
    /// Cleared by reset.</summary>
    int32_t jump = 0; // +0x08
    /// <summary>The distance of the jump requested by <see cref="jump"/>.</summary>
    float jumpDistance = 0.0f; // +0x0c
    /// <summary>Throttle, 0 or 100 (percent); ElementalDynamics treats anything else as 0.</summary>
    int8_t throttle = 0; // +0x10
    /// <summary>Yaw rate request (x maxElementalYawRate / 64). Cleared by reset.</summary>
    int8_t rotate = 0; // +0x11
};
