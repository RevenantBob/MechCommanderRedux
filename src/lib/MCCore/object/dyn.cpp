#include "stdafx.h"
#include "object/dyn.h"
#include "object/objtype.h"

auto DynamicsType::destroy() -> void
{
}

auto DynamicsType::createInstance() -> Dynamics*
{
    return new Dynamics;
}

auto Dynamics::destroy() -> void
{
}

auto Dynamics::init(DynamicsType* dynType, GameObject* object) -> int32_t
{
    type = dynType;
    me = object;
    return 0;
}

auto Dynamics::update() -> int32_t
{
    return 0;
}
