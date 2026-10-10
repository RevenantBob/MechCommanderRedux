#pragma once

// Original source: mcx\logistics.cpp (LogPart, the part LogMech and LogVehicle share).

#include "main/MCInventoryList.h"

class MCBriefingBox;

/// <summary>
/// What mechs and vehicles have in common in logistics: identity, tonnage, cost, status flags, inventory and the
/// briefing box. <see cref="PartType"/> tells which one a part is.
/// </summary>
/// <remarks>Original source: <c>logistics.cpp</c> (no methods of its own).</remarks>
class MCLogPart
{
public:
    MCLogPart();
    virtual ~MCLogPart();
    MCLogPart(const MCLogPart&) = delete;
    MCLogPart& operator=(const MCLogPart&) = delete;

    /// <summary>1 for a <see cref="MCLogMech"/>, 2 for a <see cref="MCLogVehicle"/> (a profile's ControlDataType).</summary>
    int32_t PartType = 0;
    /// <summary>
    /// The name of the profile the part was read from (at most 9 characters), or last written for it ("tpak<i>n</i>",
    /// by the starting fit and save writers).
    /// </summary>
    std::string ProfileName;
    /// <summary>The weight class name (string table, from the tonnage).</summary>
    std::string WeightClassName;
    /// <summary>The chassis class name (string table, from the armor tonnage).</summary>
    std::string ChassisClassName;
    /// <summary>
    /// The display name: string <c>DescIndex</c> + 300 (mechs) or + 700 (vehicles) of the string table. The original
    /// calls it the file name (it is saved as a profile's MechType).
    /// </summary>
    std::string FileName;
    /// <summary>The string table index of the name.</summary>
    int32_t NameIndex = 0;
    float CurTonnage = 0;
    /// <summary>The icon file name.</summary>
    std::string IconName;
    char Status = 0;
    /// <summary>The packet of the chassis in the object packet file.</summary>
    uint32_t Chassis = 0;
    /// <summary>The current value in resource points (a mech's; vehicles keep theirs in VehicleResourcePoints).</summary>
    int32_t ResourcePoints = 0;
    /// <summary>The value of the bare chassis.</summary>
    int32_t BaseResourcePoints = 0;
    /// <summary>The part's number in the scenario file the force is written to (its Mates entry).</summary>
    int32_t PartNumber = 0;
    /// <summary>The description's index in the object description file (-1 = none).</summary>
    int32_t DescIndex = 0;
    /// <summary>The description text (empty when there is none).</summary>
    std::string Description;
    /// <summary>The engine's tonnage.</summary>
    float EngineTonnage = 0;
    /// <summary>The engine rating.</summary>
    uint32_t EngineRating = 0;
    /// <summary>The armor type (a profile's Armor Type).</summary>
    uint8_t ArmorType = 0;
    /// <summary>The armor's tonnage (a profile's Armor Tonnage).</summary>
    float ArmorTonnage = 0;
    uint8_t NumOther = 0;
    uint8_t NumWeapons = 0;
    uint8_t NumAmmo = 0;
    /// <summary>The battle rating (<see cref="MCLogMech::CalcBR"/>).</summary>
    int32_t BattleRating = 0;
    /// <summary>Assigned to the force.</summary>
    bool Assigned = false;
    /// <summary>In a drop slot.</summary>
    bool Deployed = false;
    /// <summary>The mission requires this part.</summary>
    bool Required = false;
    /// <summary>Not really the player's yet (salvage not yet taken).</summary>
    bool NotMineYet = false;
    /// <summary>The local player's part (false for one received over the network).</summary>
    bool LocalPart = false;
    /// <summary>The commander (player) id of a multiplayer part.</summary>
    int32_t CommanderID = 0;
    /// <summary>The components.</summary>
    std::unique_ptr<MCInventoryList> Inventory;
    /// <summary>The box the briefing screen shows for the part (null when it was read without widgets).</summary>
    std::unique_ptr<MCBriefingBox> BriefingBox;
    /// <summary>The multiplayer drop slot's lance.</summary>
    uint32_t DropLance = 0;
    /// <summary>The multiplayer drop slot within its lance.</summary>
    uint32_t DropSlot = 0;
};
