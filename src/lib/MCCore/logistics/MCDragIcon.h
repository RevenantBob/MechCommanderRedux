#pragma once

#include "logistics/MCLogObject.h"

/// <summary>The drag state of one kind of logistics row (the inventory's mechs, the store's pilots, ...).</summary>
struct MCDragState
{
    /// <summary>The row is dragged with the left button held (picked up by a left press).</summary>
    bool Dragging = false;
    /// <summary>The row is carried after a right-button pick-up.</summary>
    bool Carrying = false;
    /// <summary>Where the drag icon is (window coordinates).</summary>
    int32_t X = 0;
    int32_t Y = 0;

    /// <summary>Neither dragged nor carried.</summary>
    bool Idle() const { return !Dragging && !Carrying; }
};

/// <summary>
/// The icon that follows the mouse while a mech, pilot, vehicle or component is dragged between the logistics panes.
/// There is one at a time, owned by the logistics screen (<see cref="Create"/>, <see cref="Remove"/>).
/// </summary>
/// <remarks>Original source: <c>logistics\invblock.cpp</c> (<c>DragIcon</c>, no fields of its own).</remarks>
class MCDragIcon : public MCLogObject
{
public:
    ~MCDragIcon() override = default;

    /// <summary>Copies the icon's port to the screen at its current position.</summary>
    void Display() override;

    /// <summary>
    /// Port: makes the icon at (<paramref name="xPos"/>, <paramref name="yPos"/>), <paramref name="width"/> x
    /// <paramref name="height"/>: its surface starts as colour 0 (the heap's fill), <paramref name="render"/> draws the
    /// dragged item into it once (the item's <c>OnBeginDrag</c>), and its edge is outlined in colour 0xea. The surface
    /// then stays as it is for the whole drag; <see cref="Display"/> only draws it.
    /// </summary>
    void Begin(int32_t xPos, int32_t yPos, int32_t width, int32_t height,
               const std::function<void(MCLogPort* surface)>& render);

    /// <summary>Shows the icon above everything else on its screen.</summary>
    void Raise();

    /// <summary>Adds the icon to <paramref name="screen"/>, raises it and moves it to (<paramref name="xPos"/>, <paramref name="yPos"/>).</summary>
    void ShowOn(MCGuiObject* screen, int32_t xPos, int32_t yPos);

    /// <summary>
    /// Port: calls <paramref name="draw"/> with <paramref name="surface"/> moved so that what it draws at
    /// (<paramref name="xPos"/>, <paramref name="yPos"/>) lands at the surface's (0, 0); the surface clips the rest.
    /// For an item that draws itself at its place in a list.
    /// </summary>
    static void DrawFrom(MCLogPort* surface, int32_t xPos, int32_t yPos,
                         const std::function<void(MCLogPort* port)>& draw);

    /// <summary>Makes the drag icon (replacing one still there), owned by the logistics screen.</summary>
    static MCDragIcon* Create();

    /// <summary>The drag icon, or null.</summary>
    static MCDragIcon* Current();

    /// <summary>Frees the drag icon, if any.</summary>
    static void Remove();
};
