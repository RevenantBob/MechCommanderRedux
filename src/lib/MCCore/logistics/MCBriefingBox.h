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
    void Init(std::shared_ptr<MCLogMech> logMech, std::shared_ptr<MCLogVehicle> logVehicle);

    void Destroy() override;

    /// <summary>Draws the mech's or vehicle's repair block into the box.</summary>
    void DrawBackground();

    /// <summary>
    /// Port: draws the box (the unit's repair block, its weapon list and tonnage bar, darkened) at
    /// (<paramref name="xPos"/>, <paramref name="yPos"/>) in <paramref name="target"/>: what
    /// <see cref="DrawBackground"/> painted into the briefing screen's picture.
    /// </summary>
    void PaintBox(const MCPane& target, int32_t xPos, int32_t yPos) const;

    void HandleEvent(MCGuiEvent& event) override;

    /// <summary>
    /// Port-only: over the weapon list the mouse wheel scrolls it, as its arrows do (the list's pane is hidden, so the
    /// wheel finds the box).
    /// </summary>
    bool MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) override;

    void Draw() override;

    /// <summary>Empty (a bare <c>ret</c> the export has no function for).</summary>
    void Display() override;

    /// <summary>The mech (null once it is gone); the caller keeps it as its pin.</summary>
    std::shared_ptr<MCLogMech> Mech() const { return _Mech.lock(); }
    /// <summary>The vehicle (null once it is gone); the caller keeps it as its pin.</summary>
    std::shared_ptr<MCLogVehicle> Vehicle() const { return _Vehicle.lock(); }
    /// <summary>The mech's weapon lists (none for a vehicle).</summary>
    MCGuiOwned<MCScrollPane> InventoryPane;

private:
    /// <summary>The mech the block refers to (<see cref="Mech"/>).</summary>
    std::weak_ptr<MCLogMech> _Mech;
    /// <summary>The vehicle the block refers to (<see cref="Vehicle"/>).</summary>
    std::weak_ptr<MCLogVehicle> _Vehicle;
};
