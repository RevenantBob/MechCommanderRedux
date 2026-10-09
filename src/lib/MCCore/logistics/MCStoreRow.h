#pragma once

#include "logistics/MCDragIcon.h"

class MCGuiEvent;
class MCGuiObject;

/// <summary>The store rows' common parts (the four kinds of <c>...PurchaseBlock</c>).</summary>
namespace MCStoreRow
{
    /// <summary>
    /// Makes the drag icon: a 0x20 square of the row (<paramref name="render"/>, the row's <c>OnBeginDrag</c>), framed,
    /// on the purchase screen at (<paramref name="drag"/>.X, .Y), above everything.
    /// </summary>
    void MakeDragIcon(const MCDragState& drag, const std::function<void(MCLogPort* surface)>& render);

    /// <summary>
    /// Whether the event is inside the purchase screen's inventory pane (its size taken from the repair screen's, as
    /// the original did).
    /// </summary>
    bool OverInventory(MCGuiEvent* event);

    /// <summary>Whether the event is inside the store (the purchase screen's unit pane; the repair screen's size).</summary>
    bool OverStore(MCGuiEvent* event);

    /// <summary>
    /// What every store row does first with an event: nothing on the repair screen, and keys go to the parent while
    /// nothing is dragged.
    /// </summary>
    /// <returns>False when the row should do nothing more.</returns>
    bool PreHandleEvent(MCGuiObject* row, const MCDragState& drag, MCGuiEvent* event);
}
