#include "stdafx.h"
#include "object/elemctrl.h"

auto ElementalControlData::init(int32_t) -> int32_t
{
    return 0;
}

auto ElementalControlData::destroy() -> void
{
}

auto ElementalControlData::reset() -> void
{
    rotate = 0;
    jump = 0;
}
