#include "stdafx.h"
#include "linkup/fidpgroup.h"
#include "linkup/sessionmanager.h"
#include "lib/aerror.h"

FIDPGroup::FIDPGroup()
{
    groupData = nullptr;
}

FIDPGroup::FIDPGroup(uint32_t id, uint32_t, const DPNAME* name, uint32_t flags)
{
    this->id = id;

    // Port fix: the buffers start cleared, so a name strncpy cuts short is still terminated.
    std::memset(this->name, 0, sizeof(this->name));
    std::memset(longName, 0, sizeof(longName));

    if (name->lpszShortNameA == nullptr)
    {
        this->name[0] = '\0';
    }
    else
    {
        // Original behaviour: 64 characters into a 64-byte buffer (not terminated when the name is that long).
        std::strncpy(this->name, name->lpszShortNameA, 0x40);
    }

    if (name->lpszLongNameA == nullptr)
    {
        longName[0] = '\0';
    }
    else
    {
        std::strncpy(longName, name->lpszLongNameA, 0xff);
    }

    this->flags = flags;
    // Port fix: the original left the data pointer unset here; SetGroupData frees whatever it holds.
    groupData = nullptr;
    groupDataSize = 0;
}

FIDPGroup::~FIDPGroup()
{
    const int numPlayers = players.count;
    players.current = players.head;

    for (int i = 0; i < numPlayers; i++)
    {
        linkUpBlocks->Free(players.ReadAndNext());
    }

    while (players.head != nullptr)
    {
        players.Del(players.head->data);
    }
}

int FIDPGroup::RemovePlayer(uint32_t& playerID)
{
    players.current = players.head;
    uint32_t* found = nullptr;

    for (int i = 0; i < players.count; i++)
    {
        uint32_t* member = players.ReadAndNext();

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

    players.Del(found);
    linkUpBlocks->Free(found);
    return 1;
}

int FIDPGroup::AddPlayer(uint32_t& playerID)
{
    players.current = players.head;

    for (int i = 0; i < players.count; i++)
    {
        if (playerID == *players.ReadAndNext())
        {
            return 0;
        }
    }

    uint32_t* member = static_cast<uint32_t*>(linkUpBlocks->Allocate(sizeof(uint32_t)));
    *member = playerID;
    players.Add(member);
    return 1;
}

void FIDPGroup::SetGroupData(void* data, uint32_t size)
{
    if (groupData != nullptr)
    {
        linkUpBlocks->Free(groupData);
    }

    groupData = linkUpBlocks->Allocate(size);
    std::memcpy(groupData, data, size);
    groupDataSize = size;
}

void FIDPGroup::ClearList(FLinkedList<FIDPGroup>& list)
{
    const int numGroups = list.count;
    list.current = list.head;

    for (int i = 0; i < numGroups; i++)
    {
        FIDPGroup* group = list.current->data;
        list.Del(group);
        delete group;
    }

    Assert(list.count == 0, 0, nullptr);
}
