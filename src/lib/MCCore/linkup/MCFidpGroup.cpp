#include "stdafx.h"
#include "linkup/MCFidpGroup.h"

MCFidpGroup::MCFidpGroup(uint32_t id, const DPNAME& name, uint32_t flags)
    : Id(id), Name(LinkupName(name.lpszShortNameA, 0x40)), LongName(LinkupName(name.lpszLongNameA, 0xff)), Flags(flags)
{
}

bool MCFidpGroup::RemovePlayer(uint32_t playerID)
{
    const auto found = std::ranges::find(Players, playerID);

    if (found == Players.end())
    {
        return false;
    }

    Players.erase(found);
    return true;
}

bool MCFidpGroup::AddPlayer(uint32_t playerID)
{
    if (std::ranges::contains(Players, playerID))
    {
        return false;
    }

    Players.push_back(playerID);
    return true;
}

void MCFidpGroup::SetGroupData(std::span<const uint8_t> data)
{
    GroupData.assign(data.begin(), data.end());
}
