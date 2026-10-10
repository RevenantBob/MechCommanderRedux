#pragma once

// Original source: mcx\logistics.cpp (LogVehicle).

#include "main/MCLogPart.h"

class MCMechBriefBlock;
class MCVehicleInventoryBlock;
class MCVehicleRepairBlock;

/// <summary>A vehicle in logistics.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0xd0 bytes.</remarks>
class MCLogVehicle : public MCLogPart
{
public:
    /// <summary>The vehicle body locations: front, left, right, rear, turret.</summary>
    static constexpr size_t NumLocations = 5;

    MCLogVehicle();
    /// <summary>Frees the inventory and the widgets in the original's order.</summary>
    ~MCLogVehicle() override;

    /// <summary>The cost: the base value plus the components'.</summary>
    void CalcVehicleCost();

    /// <summary>Loads the description of <see cref="MCLogPart::DescIndex"/> (once; none for a negative <paramref name="descIndex"/>).</summary>
    void LoadDescription(int32_t descIndex);

    /// <summary>The crew's pilot profile (the vehicle file's Crew, at most 8 characters).</summary>
    std::string Crew;
    uint8_t MaxMoveSpeed = 0;
    /// <summary>The current internal structure of the five locations.</summary>
    std::array<uint8_t, NumLocations> CurInternalStructure{};
    std::array<uint8_t, NumLocations> MaxArmorPoints{};
    std::array<uint8_t, NumLocations> CurArmorPoints{};
    /// <summary>The current value.</summary>
    int32_t VehicleResourcePoints = 0;
    /// <summary>The value of the bare vehicle.</summary>
    int32_t BaseVehicleResourcePoints = 0;
    /// <summary>The row on the repair screen (null when read without widgets).</summary>
    std::unique_ptr<MCVehicleRepairBlock> RepairBlock;
    /// <summary>The row on the inventory screens (null when read without widgets).</summary>
    std::unique_ptr<MCVehicleInventoryBlock> InventoryBlock;
    /// <summary>The block in the briefing screen's deploy pane or a drop slot (<see cref="MCMechBriefBlock::Create"/>).</summary>
    std::unique_ptr<MCMechBriefBlock> BriefBlock;
};
