#include "stdafx.h"
#include "object/mechctrl.h"

auto MechControlData::init(int32_t) -> int32_t
{
    return 0;
}

auto MechControlData::destroy() -> void
{
}

auto MechControlData::reset() -> void
{
    gestureGoal = -1;
    buttonState &= 0xfffffff8;
    torsoRotate = 0;
    rotate = 0;
    leftArmRotate = 0;
    rightArmRotate = 0;
    blowRightArm = 0;
    blowLeftArm = 0;
    pivot = 0;
}
