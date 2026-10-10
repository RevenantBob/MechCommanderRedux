#pragma once

// Original source: mcx\logistics.cpp (LogVehicleList).

#include "main/MCLogVehicle.h"

class MCFitIniFile;
class MCPacketFile;

/// <summary>A list of vehicles, by tonnage when added sorted.</summary>
/// <remarks>
/// Original source: <c>logistics.cpp</c> (<c>LogVehicleList</c>, a linked list). A negative position reads the first
/// vehicle, as the original's walks from the head did.
/// </remarks>
class MCLogVehicleList
{
public:
    MCLogVehicleList() = default;
    ~MCLogVehicleList();
    MCLogVehicleList(const MCLogVehicleList&) = delete;
    MCLogVehicleList& operator=(const MCLogVehicleList&) = delete;

    /// <summary>Removes every vehicle (with its widgets).</summary>
    void Clear();

    /// <summary>The position of <paramref name="vehicle"/>, or -1.</summary>
    int32_t GetVehicleIndex(const MCLogVehicle* vehicle) const;

    /// <summary>
    /// Adds the vehicle in profile <paramref name="fileName"/> under <c>ProfilePath</c>. Original behaviour: it always
    /// gets its widgets, whatever <paramref name="widgets"/> says.
    /// </summary>
    MCLogVehicle* AddVehicle(std::string_view fileName, bool required, bool sorted, bool widgets);

    /// <summary>Adds the vehicle in packet <paramref name="packet"/> of a save (unsorted, with widgets).</summary>
    MCLogVehicle* AddVehicle(MCPacketFile& file, int32_t packet);

    /// <summary>
    /// Reads a vehicle from its profile and adds it (by tonnage when <paramref name="sorted"/>, else first), with its
    /// inventory, repair and briefing widgets when <paramref name="widgets"/> is set.
    /// </summary>
    MCLogVehicle* AddVehicle(MCFitIniFile& file, bool required, bool sorted, bool widgets);

    /// <summary>Deletes the vehicle at <paramref name="index"/>.</summary>
    /// <returns>0, or -1 past the end.</returns>
    int32_t RemoveVehicle(int32_t index);

    /// <summary>Deletes <paramref name="vehicle"/>.</summary>
    /// <returns>0, or -1 when it isn't in the list.</returns>
    int32_t RemoveVehicle(const MCLogVehicle* vehicle);

    /// <summary>The vehicle at <paramref name="index"/> into <paramref name="vehicle"/> (null past the end).</summary>
    /// <returns>0, or -1 past the end.</returns>
    int32_t GetVehicleInfo(int32_t index, MCLogVehicle*& vehicle) const;

    int32_t GetVehicleCount() const { return static_cast<int32_t>(Vehicles.size()); }

    /// <summary>Writes the vehicle at <paramref name="index"/> as a profile text file.</summary>
    int32_t SaveVehicleText(std::string_view fileName, int32_t index);

    /// <summary>The vehicles in list order.</summary>
    std::vector<std::unique_ptr<MCLogVehicle>> Vehicles;
    /// <summary>The multiplayer player whose vehicles these are (set by <see cref="MCLogistics::InitializeMultiplayer"/>).</summary>
    uint32_t PlayerID = 0;

private:
    /// <summary>The position <paramref name="index"/> reads (a negative one the first), or the end.</summary>
    size_t Position(int32_t index) const;
};
