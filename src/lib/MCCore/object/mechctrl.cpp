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
    unknown18 = 0;
    unknown14 = 0;
    unknown1C = 0;
    unknown20 = 0;
    pivot = 0;
}
