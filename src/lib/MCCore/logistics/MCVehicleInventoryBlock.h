#pragma once

#include "logistics/MCInventoryBlock.h"

class MCLogVehicle;

/// <summary>
/// An owned vehicle's row in the inventory list (a row of <see cref="MCListView"/>): name, tonnage and class, and a
/// small damage diagram.
/// </summary>
/// <remarks>Original source: <c>logistics\invblock.cpp</c> (<c>VehicleInventoryBlock</c>).</remarks>
class MCVehicleInventoryBlock : public MCInventoryBlock
{
public:
    /// <summary>Shows <paramref name="logVehicle"/>.</summary>
    void Init(std::shared_ptr<MCLogVehicle> logVehicle);

    /// <summary>Frees the picture.</summary>
    void OnDestroy() override;

    /// <summary>Empty (a bare <c>ret</c> the export has no function for).</summary>
    void Draw() override;

    /// <summary>Selection, drag to a drop slot or the store (a sale), and the info display.</summary>
    void HandleEvent(MCGuiEvent& event) override;

    /// <summary>
    /// Makes the damage diagram when it is missing (the original's <c>drawBackground</c>, which also painted the row).
    /// </summary>
    void OnRefresh() override;

    /// <summary>Port: the vehicle's name, tonnage/class line and diagram.</summary>
    void DrawRow(MCLogPort& port, int32_t top) override;

    /// <summary>
    /// Port: <paramref name="vehicle"/>'s details in the info box: its <paramref name="diagram"/>, tonnage, classes,
    /// speed and description.
    /// </summary>
    static void DrawVehicleInfo(const MCLogVehicle& vehicle, MCOptionalRef<const MCLogPort> diagram, MCLogPort& port);

    /// <summary>
    /// The sale dialog's answer for <paramref name="sold"/>: a confirmed sale removes the vehicle; otherwise it goes
    /// back to the inventory.
    /// </summary>
    static void OnSellConfirmed(MCLogVehicle& sold, int32_t result);

    /// <summary>The cached damage diagram (0x1c x 0x1e).</summary>
    std::shared_ptr<MCLogPort> PicturePort;
    /// <summary>The vehicle (null once it is gone); the caller keeps it as its pin.</summary>
    std::shared_ptr<MCLogVehicle> Vehicle() const { return _Vehicle.lock(); }
    /// <summary>The vehicle's <see cref="MCLogPart::ListIndex"/>.</summary>
    int32_t ListIndex() const override;

private:
    /// <summary>The vehicle the block refers to (<see cref="Vehicle"/>).</summary>
    std::weak_ptr<MCLogVehicle> _Vehicle;

    /// <summary>Offers the vehicle for sale unless the mission requires it; true when the dialog opened.</summary>
    bool OfferSale();
};
