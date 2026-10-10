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
