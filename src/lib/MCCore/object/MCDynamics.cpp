#include "stdafx.h"
#include "object/MCDynamics.h"

auto MCDynamicsType::CreateInstance(MCGameObject& object) -> std::unique_ptr<MCDynamics>
{
    return std::make_unique<MCDynamics>(*this, object);
}
