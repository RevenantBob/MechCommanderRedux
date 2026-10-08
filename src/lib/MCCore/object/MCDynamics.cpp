#include "stdafx.h"
#include "object/MCDynamics.h"

auto MCDynamicsType::Destroy() -> void
{
}

auto MCDynamicsType::CreateInstance() -> MCDynamics*
{
    return new MCDynamics;
}

auto MCDynamics::Destroy() -> void
{
}

auto MCDynamics::Init(MCDynamicsType* dynType, MCGameObject* object) -> int32_t
{
    Type = dynType;
    Me = object;
    return 0;
}

auto MCDynamics::Update() -> int32_t
{
    return 0;
}
