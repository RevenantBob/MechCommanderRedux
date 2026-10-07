#pragma once

#include "object/control.h"

/// <summary>An elemental's control requests for one frame.</summary>
/// <remarks>Original source: <c>object\elemctrl.cpp</c>, <c>object\elemctrl.h</c>; 0x14 bytes.</remarks>
class MCElementalControlData : public MCControlData
{
public:
    int32_t Init(int32_t unused) override;
    void Destroy() override;
    /// <summary>+0x04 back to -1, the jump and rotate requests cleared.</summary>
    void Reset() override;
    uint32_t GetControlDataClass() override { return 3; }

    /// <summary>Set by Elemental::updateJump when a jump is due; Elemental::update then sets the actor's jump up.
    /// Cleared by reset.</summary>
    int32_t Jump = 0;
    /// <summary>The distance of the jump requested by <see cref="Jump"/>.</summary>
    float JumpDistance = 0.0f;
    /// <summary>Throttle, 0 or 100 (percent); ElementalDynamics treats anything else as 0.</summary>
    int8_t Throttle = 0;
    /// <summary>Yaw rate request (x maxElementalYawRate / 64). Cleared by reset.</summary>
    int8_t Rotate = 0;
};
