#pragma once

// Original source: mcx\linkup\fidpgroup.cpp.

#include "linkup/ficommonnetwork.h"
#include "linkup/linkedlist.h"

/// <summary>
/// A DirectPlay group of the session (MultiPlayer makes "AllPlayerGroup", "InnerSphereGroup" and "ClanGroup"): its
/// id, names, the data attached to it and the ids of its players.
/// </summary>
/// <remarks>Original source: <c>linkup\fidpgroup.cpp</c>, 0x168 bytes, allocated from linkUpHeap.</remarks>
class FIDPGroup
{
public:
    /// <summary>Allocates from linkUpHeap.</summary>
    /// <remarks>MCX.EXE @ 0x0074bd10</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x0074bd30</remarks>
    static void operator delete(void* ptr);

    /// <summary>An empty group.</summary>
    /// <remarks>MCX.EXE @ 0x0074bd50</remarks>
    FIDPGroup();
    /// <summary>
    /// The group <paramref name="id"/> named by <paramref name="name"/> (the short name up to 64 characters, the long
    /// one up to 255), with DirectPlay's group <paramref name="flags"/>. <paramref name="parentID"/> is not kept.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0074bdd0</remarks>
    FIDPGroup(uint32_t id, uint32_t parentID, const DPNAME* name, uint32_t flags);
    /// <summary>Frees the player ids and empties the list.</summary>
    /// <remarks>MCX.EXE @ 0x0074bea0 (deleting destructor 0x0074bda0)</remarks>
    virtual ~FIDPGroup();

    FIDPGroup(const FIDPGroup&) = delete;
    FIDPGroup& operator=(const FIDPGroup&) = delete;

    /// <summary>Removes <paramref name="playerID"/> from the group.</summary>
    /// <returns>1 if it was a member, else 0.</returns>
    /// <remarks>MCX.EXE @ 0x0074c0c0</remarks>
    int RemovePlayer(uint32_t& playerID);

    /// <summary>Adds <paramref name="playerID"/> to the group.</summary>
    /// <returns>1 if added, 0 if it was already a member.</returns>
    /// <remarks>MCX.EXE @ 0x0074c2e0</remarks>
    int AddPlayer(uint32_t& playerID);

    /// <summary>Replaces the group's data with a copy of <paramref name="size"/> bytes (from linkUpHeap).</summary>
    /// <remarks>MCX.EXE @ 0x0074c440</remarks>
    void SetGroupData(void* data, uint32_t size);

    /// <summary>Deletes every group of <paramref name="list"/> and empties it.</summary>
    /// <remarks>MCX.EXE @ 0x0074c4b0</remarks>
    static void ClearList(FLinkedList<FIDPGroup>& list);

    /// <summary>The group's DPID.</summary>
    uint32_t id; // +0x4
    /// <summary>Never written by the original (the constructor drops its parent-id argument).</summary>
    uint32_t unknown8; // +0x8
    /// <summary>The short name (strncpy of 64 characters: not always terminated, as in the original).</summary>
    char name[64]; // +0xc
    /// <summary>The long name.</summary>
    char longName[256]; // +0x4c
    /// <summary>DirectPlay's group flags.</summary>
    uint32_t flags; // +0x14c
    /// <summary>The data attached with <see cref="SetGroupData"/> (from linkUpHeap).</summary>
    void* groupData;        // +0x150
    uint32_t groupDataSize; // +0x154
    /// <summary>The ids of the group's players (each allocated from linkUpHeap).</summary>
    FLinkedList<uint32_t> players; // +0x158
};
