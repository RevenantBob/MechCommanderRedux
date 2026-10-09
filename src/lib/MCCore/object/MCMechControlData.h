#pragma once

#include "object/MCControl.h"

/// <summary>A mech's control requests for one frame: torso, turn and arm rates, gesture, and the button bits.</summary>
/// <remarks>Original source: <c>object\mechctrl.cpp</c>, <c>object\mechctrl.h</c>. The rates are signed fractions of
/// 64 (MechDynamics::update scales them by the type's rates and frameLength / 64).</remarks>
class MCMechControlData : public MCControlData
{
public:
    /// <summary>No gesture, no rates, button bits 0-2 and the flags cleared.</summary>
    void Reset() override
    {
        GestureGoal = -1;
        ButtonState &= 0xfffffff8;
        TorsoRotate = 0;
        Rotate = 0;
        LeftArmRotate = 0;
        RightArmRotate = 0;
        BlowRightArm = 0;
        BlowLeftArm = 0;
        Pivot = 0;
    }

    uint32_t GetControlDataClass() override { return 1; }

    /// <summary>Button bits; reset clears bits 0-2.</summary>
    uint32_t ButtonState = 0;
    /// <summary>Torso yaw rate request (x maxTorsoYawRate / 64).</summary>
    int8_t TorsoRotate = 0;
    /// <summary>The throttle, 0..100 (BattleMech's movement sets it).</summary>
    int8_t Throttle = 100;
    /// <summary>Body yaw rate request (x maxMechYawRate, or maxMechPivotRate when pivoting, / 64).</summary>
    int8_t Rotate = 0;
    /// <summary>Right arm yaw rate request (x maxRightArmYawRate / 64).</summary>
    int8_t RightArmRotate = 0;
    /// <summary>Left arm yaw rate request (x maxLeftArmYawRate / 64).</summary>
    int8_t LeftArmRotate = 0;
    /// <summary>The gesture to start, -1 for none (passed to the appearance by MechDynamics::update).</summary>
    int32_t GestureGoal = -1;
    /// <summary>Throw the left arm off this frame (BattleMech::leftArmBlownThisFrame, or the player's debug key).</summary>
    int32_t BlowLeftArm = 0;
    /// <summary>Throw the right arm off this frame (BattleMech::rightArmBlownThisFrame, or the player's debug
    /// key).</summary>
    int32_t BlowRightArm = 0;
    /// <summary>Nonzero while pivoting in place: MechDynamics turns at maxMechPivotRate instead.</summary>
    int32_t Pivot = 0;
};
