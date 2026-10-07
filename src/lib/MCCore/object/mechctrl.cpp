#include "stdafx.h"
#include "object/mechctrl.h"

auto MCMechControlData::Init(int32_t) -> int32_t
{
    return 0;
}

auto MCMechControlData::Destroy() -> void
{
}

auto MCMechControlData::Reset() -> void
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
