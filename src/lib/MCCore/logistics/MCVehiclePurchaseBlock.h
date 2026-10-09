#pragma once

#include "logistics/MCLogObject.h"

class MCGuiEvent;
class MCPurVehicle;

/// <summary>A row of the vehicle store: picture, diagram, class texts, weapons, stock and price.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c> (<c>VehiclePurchaseBlock</c>).</remarks>
class MCVehiclePurchaseBlock : public MCLogObject
{
public:
    ~MCVehiclePurchaseBlock() override;

    /// <summary>Shows <paramref name="purVehicle"/>; reads its class texts.</summary>
    void Init(MCPurVehicle* purVehicle);

    /// <summary>Frees the diagram and the texts.</summary>
    void Destroy() override;

    /// <summary>Buying (a drag to the inventory) and the help line.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Readies row <paramref name="row"/>: remakes the diagram the purchase dialog shows (while in stock) and readies
    /// the description. Port: the row is drawn by <see cref="DrawRow"/> (the original painted it here).
    /// </summary>
    void DrawBackground(int32_t row);

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

    /// <summary>Empty.</summary>
    void SetBar();

    /// <summary>
    /// The purchase dialog's answer: a confirmed purchase of <paramref name="quantity"/> adds them to the inventory and
    /// takes them off the stock.
    /// </summary>
    void OnBuyConfirmed(int32_t result, int32_t quantity);

    /// <summary>The list position the row is drawn at.</summary>
    int32_t Row = 0;
    MCPurVehicle* PurVehicle = nullptr;
    int32_t NameIndex = 0;
    /// <summary>The vehicle's diagram (0x1e x 0x1e) for the purchase dialog.</summary>
    std::unique_ptr<MCLogPort> PicturePort;
    /// <summary>The weight class by tonnage (cut at 14 characters).</summary>
    std::string WeightClassText;
    /// <summary>The armor rating by armor tonnage (cut at 14 characters).</summary>
    std::string ArmorText;
};
