#pragma once

#include "logistics/MCInventoryBlock.h"

class MCLogMech;
class MCLogVehicle;
class MCLogWarrior;

/// <summary>An owned MechWarrior in the inventory list: portrait, callsign, rank and skills.</summary>
/// <remarks>Original source: <c>logistics\invblock.cpp</c> (<c>PilotInventoryBlock</c>).</remarks>
class MCPilotInventoryBlock : public MCInventoryBlock
{
public:
    ~MCPilotInventoryBlock() override;

    /// <summary>Shows <paramref name="logWarrior"/>; takes the next free row and loads the portrait.</summary>
    void Init(MCLogWarrior* logWarrior);

    /// <summary>Frees the portrait.</summary>
    void Destroy() override;

    /// <summary>Only draws the disabled state.</summary>
    void Draw() override;

    /// <summary>
    /// Sets <see cref="GreyedOut"/> and moves the block to its row. Port: the row is drawn by <see cref="DrawRow"/>
    /// (the original painted the portrait, callsign, rank and skills here).
    /// </summary>
    void DrawBackground();

    /// <summary>Port: the portrait, callsign and rank, darkened when <see cref="GreyedOut"/>.</summary>
    void DrawRow(MCLogPort* port, int32_t top) override;

    /// <summary>Port: the skills, portrait, rank, health and description.</summary>
    void DrawInfo(MCLogPort* port) override;

    /// <summary>Selection, drag onto a mech or the store (a sale), and the info display.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// The sale dialog's answer: a confirmed sale lets the pilot go (marked sold back in the store); otherwise the
    /// pilot goes back to the inventory.
    /// </summary>
    void OnSellConfirmed(int32_t result) const;

    MCLogWarrior* Warrior = nullptr;
    /// <summary>
    /// Set by <see cref="DrawBackground"/> on the repair screen when the selected mech can't take a pilot; blocks
    /// dragging.
    /// </summary>
    bool GreyedOut = false;
    /// <summary>The pilot's portrait (<c>logart\&lt;picture&gt;</c>).</summary>
    std::unique_ptr<MCLogPort> PortraitPort;
    /// <summary>The mech the pilot was last dropped on (repair screen); cleared by <see cref="Init"/>, never read.</summary>
    MCLogMech* Mech = nullptr;
    /// <summary>
    /// The vehicle the pilot was last dropped on (<c>MCVehicleRepairBlock::HandleEvent</c>); cleared by
    /// <see cref="Init"/>, never read.
    /// </summary>
    MCLogVehicle* Vehicle = nullptr;

private:
    /// <summary>Opens the sale dialog for the pilot.</summary>
    void OfferSale();

    /// <summary>Gives the pilot to <paramref name="mech"/> (force row <paramref name="row"/>) and says so.</summary>
    void BoardMech(MCLogMech* mech, int32_t row) const;

    /// <summary>Puts the pilot back in the inventory with the drop sound (fine over the inventory, refused elsewhere).</summary>
    void BackToInventory(MCGuiEvent* event) const;
};
