#pragma once

#include "object/control.h"

/// <summary>A mech's control requests for one frame: torso, turn and arm rates, gesture, and the button bits.</summary>
/// <remarks>Original source: <c>object\mechctrl.cpp</c>, <c>object\mechctrl.h</c>; 0x28 bytes. The rates are
/// signed fractions of 64 (MechDynamics::update scales them by the type's rates and frameLength / 64).</remarks>
class MechControlData : public ControlData
{
public:
    /// <remarks>MCX.EXE @ 0x006837a0</remarks>
    int32_t init(int32_t unused) override;
    /// <remarks>MCX.EXE @ 0x006837b0</remarks>
    void destroy() override;
    /// <summary>No gesture, no rates, button bits 0-2 and the flags cleared.</summary>
    /// <remarks>MCX.EXE @ 0x006837c0</remarks>
    void reset() override;
    /// <remarks>MCX.EXE @ 0x00677090 (inline in <c>object\mechctrl.h</c>)</remarks>
    uint32_t getControlDataClass() override { return 1; }

    /// <summary>Button bits; reset clears bits 0-2.</summary>
    uint32_t buttonState = 0; // +0x04
    /// <summary>Torso yaw rate request (x maxTorsoYawRate / 64).</summary>
    int8_t torsoRotate = 0; // +0x08
    /// <summary>The throttle, 0..100 (100 by the constructor; BattleMech's movement sets it).</summary>
    int8_t throttle = 100; // +0x09
    /// <summary>Body yaw rate request (x maxMechYawRate, or maxMechPivotRate when pivoting, / 64).</summary>
    int8_t rotate = 0; // +0x0a
    /// <summary>Right arm yaw rate request (x maxRightArmYawRate / 64).</summary>
    int8_t rightArmRotate = 0; // +0x0b
    /// <summary>Left arm yaw rate request (x maxLeftArmYawRate / 64).</summary>
    int8_t leftArmRotate = 0; // +0x0c
    /// <summary>The gesture to start, -1 for none (passed to the appearance by MechDynamics::update).</summary>
    int32_t gestureGoal = -1; // +0x10
    /// <summary>Set by the AI and network controls from the mech's pending flag at +0x8d0.</summary>
    int32_t unknown14 = 0; // +0x14
    /// <summary>Set by the AI and network controls from the mech's pending flag at +0x8d4.</summary>
    int32_t unknown18 = 0; // +0x18
    /// <summary>Cleared by reset.</summary>
    int32_t unknown1C = 0; // +0x1c
    /// <summary>Cleared by reset.</summary>
    int32_t unknown20 = 0; // +0x20
    /// <summary>Nonzero while pivoting in place: MechDynamics turns at maxMechPivotRate instead.</summary>
    int32_t pivot = 0; // +0x24
};
