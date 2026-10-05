#pragma once

// Original source: mcx\linkup\fidpgroup.cpp.

#include "linkup/MCLinkupMessages.h"

/// <summary>
/// A DirectPlay group of the session (MultiPlayer makes "AllPlayerGroup", "InnerSphereGroup" and "ClanGroup"): its id,
/// names, the data attached to it and the ids of its players.
/// </summary>
class MCFidpGroup
{
public:
    /// <summary>
    /// The group <paramref name="id"/> named by <paramref name="name"/> (the short name cut to 64 characters, the long
    /// one to 255), with DirectPlay's group <paramref name="flags"/>.
    /// </summary>
    MCFidpGroup(uint32_t id, const DPNAME& name, uint32_t flags);

    MCFidpGroup(const MCFidpGroup&) = delete;
    MCFidpGroup& operator=(const MCFidpGroup&) = delete;

    /// <summary>Removes <paramref name="playerID"/> from the group.</summary>
    /// <returns>Whether it was a member.</returns>
    bool RemovePlayer(uint32_t playerID);

    /// <summary>Adds <paramref name="playerID"/> to the group.</summary>
    /// <returns>Whether it was added (false: it was already a member).</returns>
    bool AddPlayer(uint32_t playerID);

    /// <summary>Replaces the group's data with a copy of <paramref name="data"/>.</summary>
    void SetGroupData(std::span<const uint8_t> data);

    /// <summary>The group's DPID.</summary>
    uint32_t Id = 0;
    /// <summary>The short name.</summary>
    std::string Name;
    /// <summary>The long name.</summary>
    std::string LongName;
    /// <summary>DirectPlay's group flags.</summary>
    uint32_t Flags = 0;
    /// <summary>The data attached with <see cref="SetGroupData"/>.</summary>
    std::vector<uint8_t> GroupData;
    /// <summary>The ids of the group's players, in the order they joined.</summary>
    std::vector<uint32_t> Players;
};
