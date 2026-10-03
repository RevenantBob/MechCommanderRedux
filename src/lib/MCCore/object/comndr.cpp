#include "stdafx.h"
#include "object/comndr.h"
#include "lib/aerror.h"
#include "lib/heap.h"
#include "object/group.h"
#include "object/mover.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"

int32_t NumCommanders = 0;
Commander* CommanderTable[MAX_COMMANDERS] = {};
Commander* HomeCommander = nullptr;

namespace
{
    /// <summary>The home commander's support changed: redraw the tactical map.</summary>
    void RedrawTacticalMap(const Commander* commander)
    {
        if (Terrain::terrainTacticalMap != nullptr && commander == HomeCommander)
        {
            Terrain::terrainTacticalMap->RefreshPage();
        }
    }
}

auto Commander::operator new(size_t size) noexcept -> void*
{
    return systemHeap->malloc(static_cast<uint32_t>(size));
}

auto Commander::operator delete(void* ptr) -> void
{
    systemHeap->free(ptr);
}

auto Commander::init() -> void
{
    id = -1;
    team = nullptr;

    for (int32_t i = 0; i < MAX_GROUPS_PER_COMMANDER; i++)
    {
        MoverGroup* group = new MoverGroup;
        groups[i] = group;
        group->setId(i);
    }

    numSmallStrikes = 0;
    numLargeStrikes = 0;
    numSensorStrikes = 0;
    numCameraDrones = 0;
}

auto Commander::destroy() -> void
{
    for (MoverGroup*& group : groups)
    {
        if (group != nullptr)
        {
            group->destroy();
            delete group;
        }

        group = nullptr;
    }
}

auto Commander::setGroup(int32_t groupId, int32_t numMovers, Mover** moverList, int32_t point) -> int32_t
{
    Assert(groupId >= 0 && groupId < MAX_GROUPS_PER_COMMANDER, 0, " Commander::bad id in setGroup ");
    MoverGroup* group = groups[groupId];
    Assert(group != nullptr, 0, " Commander::setGroup has null group ");
    group->disband();

    for (int32_t i = 0; i < numMovers; i++)
    {
        Mover* mover = moverList[i];

        if (mover->group != nullptr)
        {
            mover->group->remove(mover);
        }

        group->add(mover);
    }

    if (point < numMovers && point > -1)
    {
        group->setPoint(moverList[point]);
    }

    return 0;
}

auto Commander::setNetPlayerId(int32_t playerId) -> void
{
    for (MoverGroup* group : groups)
    {
        for (int32_t i = 0; i < group->numMovers; i++)
        {
            group->movers[i]->netPlayerId = playerId;
        }
    }
}

auto Commander::addToGUI(int visible) -> void
{
    for (int32_t i = 0; i < 4; i++)
    {
        groups[i]->addToGUI(visible);
    }
}

auto Commander::setNumSmallStrikes(int32_t strikes) -> void
{
    numSmallStrikes = strikes;
    RedrawTacticalMap(this);
}

auto Commander::setNumLargeStrikes(int32_t strikes) -> void
{
    numLargeStrikes = strikes;
    RedrawTacticalMap(this);
}

auto Commander::setNumSensorStrikes(int32_t strikes) -> void
{
    numSensorStrikes = strikes;
    RedrawTacticalMap(this);
}

auto Commander::setNumCameraDrones(int32_t drones) -> void
{
    numCameraDrones = drones;
    RedrawTacticalMap(this);
}
