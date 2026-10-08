#include "stdafx.h"
#include "object/MCObjectEvent.h"
#include "object/MCGameObject.h"

auto MCObjectEvent::Init(int32_t newId, MCGuiEvent* newEvent) -> void
{
    Type = 0;
    Id = newId;

    if (newEvent == nullptr)
    {
        Event.Type = 0x2401;
        Window = nullptr;
    }
    else
    {
        MCGuiObject* target = newEvent->Target;
        Event = *newEvent;
        Window = target;
    }

    SelectionIndex = -1;
}

auto MCObjectEvent::InitCombat(int32_t newId, MCGameObject* attacker, MCGameObject* target) -> void
{
    Type = 2;

    if (attacker != nullptr)
    {
        AttackerPartId = attacker->PartId;
    }

    if (target != nullptr)
    {
        TargetPartId = target->PartId;
    }

    Id = newId;
}
