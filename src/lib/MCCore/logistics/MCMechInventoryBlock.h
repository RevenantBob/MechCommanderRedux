#pragma once

#include "logistics/MCInventoryBlock.h"

class MCLogMech;

/// <summary>
/// An owned mech's row in the inventory list (a row of <see cref="MCListView"/>): name, tonnage and a small damage
/// diagram.
/// </summary>
/// <remarks>Original source: <c>logistics\invblock.cpp</c> (<c>MechInventoryBlock</c>).</remarks>
class MCMechInventoryBlock : public MCInventoryBlock
{
public:
    /// <summary>Shows <paramref name="logMech"/>.</summary>
    void Init(std::shared_ptr<MCLogMech> logMech);

    /// <summary>Frees the diagram.</summary>
    void OnDestroy() override;

    /// <summary>Only draws the disabled state (the row itself is drawn by <see cref="DrawRow"/>).</summary>
    void Draw() override;

    /// <summary>
    /// Makes the damage diagram when it is missing (the original's <c>drawBackground</c>, which also painted the row).
    /// </summary>
    void OnRefresh() override;

    /// <summary>Port: the mech's name and tonnage/class line, and the diagram.</summary>
    void DrawRow(MCLogPort& port, int32_t top) override;

    /// <summary>
    /// Port: <paramref name="mech"/>'s details in the info box: its <paramref name="diagram"/>, tonnage, classes,
    /// speed and description.
    /// </summary>
    static void DrawMechInfo(const MCLogMech& mech, MCOptionalRef<const MCLogPort> diagram, MCLogPort& port);

    /// <summary>Selection, drag to a drop slot or the store (a sale), and the info display.</summary>
    void HandleEvent(MCGuiEvent& event) override;

    /// <summary>
    /// The sale dialog's answer for <paramref name="sold"/>: a confirmed sale moves the mech's undamaged weapons and
    /// equipment to the spare components and removes the mech; otherwise it goes back to the inventory.
    /// </summary>
    static void OnSellConfirmed(MCLogMech& sold, int32_t result);

    /// <summary>The cached small damage diagram (0x1c x 0x1e).</summary>
    std::shared_ptr<MCLogPort> DiagramPort;
    /// <summary>The mech (null once it is gone); the caller keeps it as its pin.</summary>
    std::shared_ptr<MCLogMech> Mech() const { return _Mech.lock(); }
    /// <summary>The mech's <see cref="MCLogPart::ListIndex"/>.</summary>
    int32_t ListIndex() const override;

private:
    /// <summary>The mech the block refers to (<see cref="Mech"/>).</summary>
    std::weak_ptr<MCLogMech> _Mech;

    /// <summary>
    /// Offers <paramref name="mech"/> for sale (unless the mission requires it), its <paramref name="diagram"/> in the
    /// dialog: both as the event that offers it took them, since its refresh can drop this row first.
    /// </summary>
    void OfferSale(const std::shared_ptr<MCLogMech>& mech, const std::shared_ptr<MCLogPort>& diagram);
};
