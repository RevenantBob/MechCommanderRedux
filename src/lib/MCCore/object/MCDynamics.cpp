#include "stdafx.h"
#include "object/MCDynamics.h"
#include "object/MCGameObject.h"

auto MCDynamicsType::CreateInstance(MCGameObject& object) -> std::unique_ptr<MCDynamics>
{
    return std::make_unique<MCDynamics>(shared_from_this(), object.WeakAs<MCGameObject>());
}
