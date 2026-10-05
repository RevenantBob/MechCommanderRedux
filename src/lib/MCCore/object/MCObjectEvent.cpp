#include "stdafx.h"
#include "object/MCObjectEvent.h"
#include "object/MCGameObject.h"

auto MCObjectEvent::Init(int32_t newId) -> void
{
    Type = 0;
    Id = newId;
    Event.Type = 0x2401;
    SelectionIndex = -1;
}

auto MCObjectEvent::Init(int32_t newId, const MCGuiEvent& newEvent) -> void
{
    Type = 0;
    Id = newId;
    Event = newEvent;
    SelectionIndex = -1;
}
