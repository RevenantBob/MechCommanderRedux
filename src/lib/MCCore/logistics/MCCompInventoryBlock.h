#pragma once

#include "logistics/MCInventoryBlock.h"

class MCLogMech;
struct MCLogInventoryItem;

/// <summary>
/// The master components a mech can only mount when its name index is 5, 0xe or 0x10 (a game rule:
/// <see cref="MCCompInventoryBlock::CanMount"/>).
/// </summary>
inline constexpr std::array<uint8_t, 5> RestrictedComps = {15, 37, 38, 42, 43};

/// <summary>A stack of an owned component (weapon, ammo, equipment) in the inventory list.</summary>
/// <remarks>Original source: <c>logistics\invblock.cpp</c> (<c>CompInventoryBlock</c>).</remarks>
class MCCompInventoryBlock : public MCInventoryBlock
{
public:
    ~MCCompInventoryBlock() override;

    /// <summary>Shows <paramref name="item"/>; builds its stat texts and icon.</summary>
    void Init(MCLogInventoryItem* item);

    /// <summary>Frees the icon (and the row's own port).</summary>
    void Destroy() override;

    /// <summary>Selection, drag onto a mech or the store (a sale), and the info display.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Shows or hides the block and sets <see cref="CantMount"/>. Port: the row is drawn by <see cref="DrawRow"/> (the
    /// original painted it here, darkened when the item can't go on the selected mech).
    /// </summary>
    void DrawBackground();

    /// <summary>Port: the icon and the count, darkened when <see cref="CantMount"/>.</summary>
    void DrawRow(MCLogPort* port, int32_t top) override;

    /// <summary>
    /// Whether component <paramref name="masterID"/>, weighing <paramref name="tonnage"/> (with a weapon's ammo), can
    /// go on <paramref name="mech"/>: the mech has the free tonnage, carries no other ECM, sensor or probe when it is
    /// one, and is one of the mechs a restricted component fits.
    /// </summary>
    static bool CanMount(uint8_t masterID, float tonnage, const MCLogMech* mech);

    /// <summary>
    /// Adds a spare component of <paramref name="masterID"/> with no copies counted yet (one copy record) to the
    /// component inventory, with its inventory row.
    /// </summary>
    /// <returns>The new item.</returns>
    static MCLogInventoryItem* AddSpare(uint8_t masterID);

    /// <summary>
    /// The sale dialog's answer: a confirmed sale takes <paramref name="quantity"/> off the stack and pays for them.
    /// </summary>
    void OnSellConfirmed(int32_t result, int32_t quantity);

    /// <summary>The range: "%.1f m", a short/medium/long word for weapons, "N/A" for probes.</summary>
    std::string RangeText;
    /// <summary>The weight ("%.1f tons").</summary>
    std::string WeightText;
    /// <summary>The recycle time ("%.2f s"), or "N/A".</summary>
    std::string RecycleText;
    /// <summary>The damage ("%.2f", tripled for one weapon kind), or "N/A".</summary>
    std::string DamageText;
    /// <summary>
    /// The item can't go on the mech selected on the repair screen (see <see cref="CanMount"/>); the row is drawn
    /// darkened.
    /// </summary>
    bool CantMount = false;
    MCLogInventoryItem* Item = nullptr;
    /// <summary>The component's icon.</summary>
    std::unique_ptr<MCLogPort> IconPort;
    /// <summary>Tonnage (plus its ammo's for ballistic and missile weapons).</summary>
    float Tonnage = 0.0f;
    /// <summary>The item's index in the owner's inventory list (set by whoever makes the block).</summary>
    int32_t InventoryIndex = 0;

private:
    /// <summary>Offers the whole stack for sale.</summary>
    void OfferSale();
};
