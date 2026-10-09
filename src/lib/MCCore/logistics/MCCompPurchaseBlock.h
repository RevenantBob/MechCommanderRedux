#pragma once

#include "logistics/MCLogObject.h"

class MCGuiEvent;
struct MCLogInventoryItem;

/// <summary>A row of the component store: picture, name, range, damage, recycle time, stock and price.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c> (<c>CompPurchaseBlock</c>).</remarks>
class MCCompPurchaseBlock : public MCLogObject
{
public:
    ~MCCompPurchaseBlock() override;

    /// <summary>Shows <paramref name="item"/>; builds its stat texts.</summary>
    void Init(MCLogInventoryItem* item);

    /// <summary>Nothing of its own (the base's).</summary>
    void Destroy() override;

    /// <summary>Buying (a drag to the inventory) and the help line.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Readies row <paramref name="row"/>: the description. Port: the row is drawn by <see cref="DrawRow"/> (the
    /// original painted it here).
    /// </summary>
    void DrawBackground(int32_t row, int32_t unused);

    /// <summary>
    /// Port: draws the row into <paramref name="port"/> (the store's view) with its top at <paramref name="top"/>:
    /// what the original's <c>drawBackground</c> painted into the store's picture.
    /// </summary>
    void DrawRow(MCLogPort* port, int32_t top);

    /// <summary>
    /// Port: draws the drag icon's picture into <paramref name="surface"/>: a 0x20 square of the row, over the store's
    /// background.
    /// </summary>
    void OnBeginDrag(MCLogPort* surface);

    /// <summary>
    /// The purchase dialog's answer: a confirmed purchase moves <paramref name="quantity"/> from the store's stock to
    /// the spare components (a new one gets its inventory row) and pays for them.
    /// </summary>
    void OnBuyConfirmed(int32_t result, int32_t quantity);

    /// <summary>The list position the row is drawn at.</summary>
    int32_t Row = 0;
    /// <summary>The item's sort order (set by the maker from the item's).</summary>
    int32_t SortOrder = 0;
    /// <summary>The range: a short/medium/long word for weapons, "%.1f m" or "N/A" for equipment.</summary>
    std::string RangeText;
    /// <summary>The weight (string 0x27f, "%.1f tons").</summary>
    std::string WeightText;
    /// <summary>The recycle time with its rating word, or "N/A".</summary>
    std::string RecycleText;
    /// <summary>The damage with its rating word, or "N/A".</summary>
    std::string DamageText;
    MCLogInventoryItem* Item = nullptr;
};
