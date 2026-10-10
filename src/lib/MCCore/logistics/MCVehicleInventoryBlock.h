#pragma once

#include "logistics/MCInventoryBlock.h"

class MCLogVehicle;

/// <summary>An owned vehicle in the inventory list: name, tonnage and class, and a small damage diagram.</summary>
/// <remarks>Original source: <c>logistics\invblock.cpp</c> (<c>VehicleInventoryBlock</c>).</remarks>
class MCVehicleInventoryBlock : public MCInventoryBlock
{
public:
    ~MCVehicleInventoryBlock() override;

    /// <summary>Shows <paramref name="logVehicle"/>; reads its weight-class and armor texts.</summary>
    void Init(MCLogVehicle* logVehicle);

    /// <summary>Frees the picture and the texts.</summary>
    void Destroy() override;

    /// <summary>Empty (a bare <c>ret</c> the export has no function for).</summary>
    void Draw() override;

    /// <summary>Selection, drag to a drop slot or the store (a sale), and the info display.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Makes the damage diagram when it is missing. Port: the row is drawn by <see cref="DrawRow"/> (the original
    /// painted it here).
    /// </summary>
    void DrawBackground();

    /// <summary>Port: the vehicle's name, tonnage/class line and diagram.</summary>
    void DrawRow(MCLogPort* port, int32_t top) override;

    /// <summary>Port: the diagram, tonnage, classes, speed and description.</summary>
    void DrawInfo(MCLogPort* port) override;

    /// <summary>
    /// The sale dialog's answer: a confirmed sale removes the vehicle; otherwise it goes back to the inventory.
    /// </summary>
    void OnSellConfirmed(int32_t result) const;

    /// <summary>The cached damage diagram (0x1c x 0x1e).</summary>
    std::unique_ptr<MCLogPort> PicturePort;
    MCLogVehicle* Vehicle = nullptr;
    /// <summary>The weight class (string table 0x4f..0x52 by tonnage).</summary>
    std::string WeightClassText;
    /// <summary>The armor rating (string table 0x4f/0x51/0x64..0x66 by armor tonnage), cut at 14 characters.</summary>
    std::string ArmorText;

private:
    /// <summary>Offers the vehicle for sale unless the mission requires it; true when the dialog opened.</summary>
    bool OfferSale();
};
