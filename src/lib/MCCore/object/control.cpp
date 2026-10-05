#include "stdafx.h"
#include "object/control.h"
#include "object/objtype.h"

auto ControlData::destroy() -> void
{
}

auto ControlData::init(int32_t) -> int32_t
{
    return 0;
}

auto ControlData::reset() -> void
{
}

auto Control::destroy() -> void
{
}

auto Control::init(GameObject* object, int32_t) -> int32_t
{
    me = object;
    return 0;
}

auto Control::update() -> int32_t
{
    return 0;
}
