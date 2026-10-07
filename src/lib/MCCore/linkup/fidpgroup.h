#pragma once

// Original source: mcx\linkup\fidpgroup.cpp.

#include "linkup/ficommonnetwork.h"
#include "linkup/linkedlist.h"

/// <summary>
/// A DirectPlay group of the session (MultiPlayer makes "AllPlayerGroup", "InnerSphereGroup" and "ClanGroup"): its
/// id, names, the data attached to it and the ids of its players.
/// </summary>
/// <remarks>Original source: <c>linkup\fidpgroup.cpp</c>, 0x168 bytes.</remarks>
class MCFidpGroup
{
public:
    /// <summary>An empty group.</summary>
    MCFidpGroup();
    /// <summary>
    /// The group <paramref name="id"/> named by <paramref name="name"/> (the short name up to 64 characters, the long
    /// one up to 255), with DirectPlay's group <paramref name="flags"/>. <paramref name="parentID"/> is not kept.
    /// </summary>
    MCFidpGroup(uint32_t id, uint32_t parentID, const DPNAME* name, uint32_t flags);
    /// <summary>Frees the player ids and empties the list.</summary>
    virtual ~MCFidpGroup();

    MCFidpGroup(const MCFidpGroup&) = delete;
    MCFidpGroup& operator=(const MCFidpGroup&) = delete;

    /// <summary>Removes <paramref name="playerID"/> from the group.</summary>
    /// <returns>1 if it was a member, else 0.</returns>
    int RemovePlayer(uint32_t& playerID);

    /// <summary>Adds <paramref name="playerID"/> to the group.</summary>
    /// <returns>1 if added, 0 if it was already a member.</returns>
    int AddPlayer(uint32_t& playerID);

    /// <summary>Replaces the group's data with a copy of <paramref name="size"/> bytes (a linkUpBlocks block).</summary>
    void SetGroupData(void* data, uint32_t size);

    /// <summary>Deletes every group of <paramref name="list"/> and empties it.</summary>
    static void ClearList(MCFLinkedList<MCFidpGroup>& list);

    /// <summary>The group's DPID.</summary>
    uint32_t Id = 0;
    /// <summary>The short name (strncpy of 64 characters: not always terminated, as in the original).</summary>
    char Name[64]{};
    /// <summary>The long name.</summary>
    char LongName[256]{};
    /// <summary>DirectPlay's group flags.</summary>
    uint32_t Flags = 0;
    /// <summary>The data attached with <see cref="SetGroupData"/> (a linkUpBlocks block).</summary>
    void* GroupData = nullptr;
    uint32_t GroupDataSize = 0;
    /// <summary>The ids of the group's players (each a linkUpBlocks block).</summary>
    MCFLinkedList<uint32_t> Players;
};
