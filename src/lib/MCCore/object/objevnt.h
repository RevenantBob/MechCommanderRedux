#pragma once

#include "gui/asystem.h"

class GameObject;

/// <summary>
/// An event sent to game objects: a mouse event over the map (type 0, with the GUI event and the window it came
/// from) or a combat event between two objects (type 2, by their part ids).
/// </summary>
/// <remarks>Original source: <c>object\objevnt.cpp</c>; 0x58 bytes.</remarks>
class ObjectEvent
{
public:
    /// <summary>
    /// A mouse event: copies the GUI event (or makes an empty one of type 0x2401 when null), remembers its target
    /// window, and clears the rest.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0068e420</remarks>
    void init(int32_t newId, aEvent* event);
    /// <summary>A combat event from <paramref name="attacker"/> to <paramref name="target"/> (their part ids).</summary>
    /// <remarks>MCX.EXE @ 0x0068e480</remarks>
    void initCombat(int32_t newId, GameObject* attacker, GameObject* target);

    /// <summary>0 for a mouse event, 2 for a combat event.</summary>
    int32_t type = 0; // +0x00
    /// <summary>The attacker's part id (combat events).</summary>
    int32_t attackerPartId = 0; // +0x04
    /// <summary>The target's part id (combat events).</summary>
    int32_t targetPartId = 0; // +0x08
    /// <summary>What happened.</summary>
    int32_t id = 0; // +0x0c
    /// <summary>The GUI event (mouse events); its x, y (+0x24, +0x28 here) are screen coordinates.</summary>
    aEvent event; // +0x10
    /// <summary>The window the GUI event came from (its target).</summary>
    aObject* window = nullptr; // +0x38
    /// <summary>A selection event's (0x1c) selection index, which Mover::handleEvent takes (-1 by init).</summary>
    int32_t selectionIndex = -1; // +0x54
};
