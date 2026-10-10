#pragma once

#include "gui/MCGuiSystem.h"

class MCGameObject;

/// <summary>
/// An event sent to game objects: a mouse event over the map (type 0, with the GUI event and the window it came
/// from) or a combat event between two objects (type 2, by their part ids).
/// </summary>
/// <remarks>Original source: <c>object\objevnt.cpp</c>; 0x58 bytes.</remarks>
class MCObjectEvent
{
public:
    /// <summary>
    /// A mouse event: copies the GUI event (or makes an empty one of type 0x2401 when null), remembers its target
    /// window, and clears the rest.
    /// </summary>
    void Init(int32_t newId, MCGuiEvent* event);

    /// <summary>0 for a mouse event, 2 for a combat event.</summary>
    int32_t Type = 0;
    /// <summary>The attacker's part id (combat events).</summary>
    int32_t AttackerPartId = 0;
    /// <summary>The target's part id (combat events).</summary>
    int32_t TargetPartId = 0;
    /// <summary>What happened.</summary>
    int32_t Id = 0;
    /// <summary>The GUI event (mouse events); its x, y (+0x24, +0x28 here) are screen coordinates.</summary>
    MCGuiEvent Event;
    /// <summary>The window the GUI event came from (its target).</summary>
    MCGuiObject* Window = nullptr;
    /// <summary>A selection event's (0x1c) selection index, which Mover::handleEvent takes (-1 by init).</summary>
    int32_t SelectionIndex = -1;
};
