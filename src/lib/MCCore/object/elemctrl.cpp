#include "stdafx.h"
#include "object/elemctrl.h"

auto MCElementalControlData::Init(int32_t) -> int32_t
{
    return 0;
}

auto MCElementalControlData::Destroy() -> void
{
}

auto MCElementalControlData::Reset() -> void
{
    Rotate = 0;
    Jump = 0;
}
