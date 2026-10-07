#include "stdafx.h"
#include "linkup/fidpgroup.h"
#include "linkup/sessionmanager.h"
#include "lib/aerror.h"

MCFidpGroup::MCFidpGroup()
{
    GroupData = nullptr;
}

MCFidpGroup::MCFidpGroup(uint32_t id, uint32_t, const DPNAME* name, uint32_t flags)
{
    this->Id = id;

    // Port fix: the buffers start cleared, so a name strncpy cuts short is still terminated.
    std::memset(this->Name, 0, sizeof(this->Name));
    std::memset(LongName, 0, sizeof(LongName));

    if (name->lpszShortNameA == nullptr)
    {
        this->Name[0] = '\0';
    }
    else
    {
        // Original behaviour: 64 characters into a 64-byte buffer (not terminated when the name is that long).
        std::strncpy(this->Name, name->lpszShortNameA, 0x40);
    }

    if (name->lpszLongNameA == nullptr)
    {
        LongName[0] = '\0';
    }
    else
    {
        std::strncpy(LongName, name->lpszLongNameA, 0xff);
    }

    this->Flags = flags;
    // Port fix: the original left the data pointer unset here; SetGroupData frees whatever it holds.
    GroupData = nullptr;
    GroupDataSize = 0;
}

MCFidpGroup::~MCFidpGroup()
{
    const int numPlayers = Players.Count;
    Players.Current = Players.HeadLink;

    for (int i = 0; i < numPlayers; i++)
    {
        LinkUpBlocks->Free(Players.ReadAndNext());
    }

    while (Players.HeadLink != nullptr)
    {
        Players.Del(Players.HeadLink->Data);
    }
}

int MCFidpGroup::RemovePlayer(uint32_t& playerID)
{
    Players.Current = Players.HeadLink;
    uint32_t* found = nullptr;

    for (int i = 0; i < Players.Count; i++)
    {
        uint32_t* member = Players.ReadAndNext();

        if (*member == playerID)
        {
            found = member;
            break;
        }
    }

    if (found == nullptr)
    {
        return 0;
    }

    Players.Del(found);
    LinkUpBlocks->Free(found);
    return 1;
}

int MCFidpGroup::AddPlayer(uint32_t& playerID)
{
    Players.Current = Players.HeadLink;

    for (int i = 0; i < Players.Count; i++)
    {
        if (playerID == *Players.ReadAndNext())
        {
            return 0;
        }
    }

    uint32_t* member = static_cast<uint32_t*>(LinkUpBlocks->Allocate(sizeof(uint32_t)));
    *member = playerID;
    Players.Add(member);
    return 1;
}

void MCFidpGroup::SetGroupData(void* data, uint32_t size)
{
    if (GroupData != nullptr)
    {
        LinkUpBlocks->Free(GroupData);
    }

    GroupData = LinkUpBlocks->Allocate(size);
    std::memcpy(GroupData, data, size);
    GroupDataSize = size;
}

void MCFidpGroup::ClearList(MCFLinkedList<MCFidpGroup>& list)
{
    const int numGroups = list.Count;
    list.Current = list.HeadLink;

    for (int i = 0; i < numGroups; i++)
    {
        MCFidpGroup* group = list.Current->Data;
        list.Del(group);
        delete group;
    }

    Assert(list.Count == 0, 0, nullptr);
}
