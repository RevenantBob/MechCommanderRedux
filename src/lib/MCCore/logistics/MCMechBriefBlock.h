#pragma once

#include "logistics/MCLogObject.h"

class MCGuiEvent;
class MCLogMech;
class MCLogVehicle;

/// <summary>
/// One mech or vehicle in the briefing screen's deploy pane or a drop slot: its picture, name and status; it can be
/// dragged into a drop slot (left button: the slot under the mouse; right button: the first free slot, of the lance
/// whose number key is held).
/// </summary>
/// <remarks>
/// Original source: <c>logistics\logbri.cpp</c> (<c>MechBriefBlock</c>). Its unit owns it (<c>BriefBlock</c>).
/// </remarks>
class MCMechBriefBlock : public MCLogObject
{
public:
    ~MCMechBriefBlock() override { Destroy(); }

    /// <summary>
    /// A block for <paramref name="mech"/> (its <c>BriefBlock</c>) at (<paramref name="xPos"/>, <paramref name="yPos"/>)
    /// in <paramref name="parent"/>.
    /// </summary>
    static MCMechBriefBlock* Create(MCLogMech* mech, MCLogObject* parent, int32_t xPos, int32_t yPos);

    /// <summary>As the other overload, for <paramref name="vehicle"/>.</summary>
    static MCMechBriefBlock* Create(MCLogVehicle* vehicle, MCLogObject* parent, int32_t xPos, int32_t yPos);

    /// <summary>Frees a unit's block <paramref name="block"/> (if any) and clears it.</summary>
    static void Discard(std::unique_ptr<MCMechBriefBlock>& block);

    void Destroy() override;

    /// <summary>Drags the block and drops it into a slot (or back), updating the tonnage.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Port: the block is drawn each frame by its parent; in a drop slot, the screen draws it there from now on.</summary>
    void DrawBackground();

    /// <summary>
    /// Port: draws the block's picture (the unit's picture and name, darkened when another player's, with a bevelled
    /// frame when <paramref name="framed"/>) at (<paramref name="xPos"/>, <paramref name="yPos"/>) in
    /// <paramref name="target"/>: what <see cref="DrawBackground"/> painted into its parent's picture.
    /// </summary>
    void PaintBlock(MCPane* target, int32_t xPos, int32_t yPos, bool framed);

    /// <summary>
    /// Port: draws the drag icon's picture into <paramref name="surface"/> (0x34 x 0x2e): the block framed over the
    /// empty slot when it is in a drop slot, unframed over colour 0x10 in the deploy pane.
    /// </summary>
    void OnBeginDrag(MCLogPort* surface);

    /// <summary>The mech shown, or null for a vehicle.</summary>
    MCLogMech* Mech = nullptr;
    /// <summary>The vehicle shown, or null for a mech.</summary>
    MCLogVehicle* Vehicle = nullptr;

private:
    /// <summary>Places the block at (<paramref name="xPos"/>, <paramref name="yPos"/>) in <paramref name="parent"/>.</summary>
    void Attach(MCLogObject* parent, int32_t xPos, int32_t yPos);

    /// <summary>A button went down on the block: its briefing shows, and (unless another player's) it is picked up.</summary>
    void PickUp(MCGuiEvent* event);

    /// <summary>The block was let go at the event's place: into a slot, or back to the deploy pane.</summary>
    void Drop(MCGuiEvent* event);

    /// <summary>
    /// Puts the unit into empty slot <paramref name="slot"/> of <paramref name="lance"/>: false when it is too heavy
    /// (without the hammer down).
    /// </summary>
    bool PlaceInEmptySlot(int32_t lance, int32_t slot) const;

    /// <summary>Puts the block into the slot on the screen and recounts the tonnage.</summary>
    void Settle(int32_t lance, int32_t slot);

    /// <summary>The right-button drop: the first free slot (of the lance whose key 1..3 is held); whether it was placed.</summary>
    bool DropInFreeSlot();

    /// <summary>The left-button drop at <paramref name="point"/>: the slot under it; whether it was placed.</summary>
    bool DropInSlotAt(POINT point, int32_t firstLance);
};
