#include "stdafx.h"
#include "object/objevnt.h"
#include "object/gameobj.h"

auto ObjectEvent::init(int32_t newId, aEvent* newEvent) -> void
{
    type = 0;
    id = newId;

    if (newEvent == nullptr)
    {
        event.type = 0x2401;
        window = nullptr;
    }
    else
    {
        aObject* target = newEvent->target;
        event = *newEvent;
        window = target;
    }

    selectionIndex = -1;
}

auto ObjectEvent::initCombat(int32_t newId, GameObject* attacker, GameObject* target) -> void
{
    type = 2;

    if (attacker != nullptr)
    {
        attackerPartId = attacker->partId;
    }

    if (target != nullptr)
    {
        targetPartId = target->partId;
    }

    id = newId;
}
