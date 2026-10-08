#include "stdafx.h"
#include "object/MCControl.h"

auto MCControlData::Destroy() -> void
{
}

auto MCControlData::Init(int32_t) -> int32_t
{
    return 0;
}

auto MCControlData::Reset() -> void
{
}

auto MCControl::Destroy() -> void
{
}

auto MCControl::Init(MCGameObject* object, int32_t) -> int32_t
{
    Me = object;
    return 0;
}

auto MCControl::Update() -> int32_t
{
    return 0;
}
