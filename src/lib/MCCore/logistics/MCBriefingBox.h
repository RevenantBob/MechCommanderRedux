#pragma once

#include "gui/MCScrollPane.h"
#include "logistics/MCLogObject.h"

class MCGuiEvent;
class MCLogMech;
class MCLogVehicle;

/// <summary>The panel of the briefing screen showing the selected mech (its repair block) or vehicle.</summary>
/// <remarks>Original source: <c>logistics\mrblock.cpp</c> (<c>BriefingBox</c>).</remarks>
class MCBriefingBox : public MCLogObject
{
public:
    ~MCBriefingBox() override;

    /// <summary>Shows <paramref name="logMech"/> (with a weapon-list pane) or <paramref name="logVehicle"/>.</summary>
    void Init(MCLogMech* logMech, MCLogVehicle* logVehicle);

    void Destroy() override;

    /// <summary>Draws the mech's or vehicle's repair block into the box.</summary>
    void DrawBackground();

    /// <summary>
    /// Port: draws the box (the unit's repair block, its weapon list and tonnage bar, darkened) at
    /// (<paramref name="xPos"/>, <paramref name="yPos"/>) in <paramref name="target"/>: what
    /// <see cref="DrawBackground"/> painted into the briefing screen's picture.
    /// </summary>
    void PaintBox(MCPane* target, int32_t xPos, int32_t yPos);

    /// <summary>Empty.</summary>
    void DrawVehicleBackground();

    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Port-only: over the weapon list the mouse wheel scrolls it, as its arrows do (the list's pane is hidden, so the
    /// wheel finds the box).
    /// </summary>
    bool MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) override;

    void Draw() override;

    /// <summary>Empty (a bare <c>ret</c> the export has no function for).</summary>
    void Display() override;

    MCLogMech* Mech = nullptr;
    MCLogVehicle* Vehicle = nullptr;
    /// <summary>The mech's weapon lists (none for a vehicle).</summary>
    MCGuiOwned<MCScrollPane> InventoryPane;
};
