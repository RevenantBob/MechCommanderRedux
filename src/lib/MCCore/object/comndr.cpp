#include "stdafx.h"
#include "object/comndr.h"
#include "lib/aerror.h"
#include "object/group.h"
#include "object/mover.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"

int32_t NumCommanders = 0;
MCCommander* CommanderTable[MAX_COMMANDERS] = {};
MCCommander* HomeCommander = nullptr;

namespace
{
    /// <summary>The home commander's support changed: redraw the tactical map.</summary>
    void RedrawTacticalMap(const MCCommander* commander)
    {
        if (MCTerrain::TerrainTacticalMap != nullptr && commander == HomeCommander)
        {
            MCTerrain::TerrainTacticalMap->RefreshPage();
        }
    }
}

auto MCCommander::Init() -> void
{
    Id = -1;
    Team = nullptr;

    for (int32_t i = 0; i < MAX_GROUPS_PER_COMMANDER; i++)
    {
        MCMoverGroup* group = new MCMoverGroup;
        Groups[i] = group;
        group->SetId(i);
    }

    NumSmallStrikes = 0;
    NumLargeStrikes = 0;
    NumSensorStrikes = 0;
    NumCameraDrones = 0;
}

auto MCCommander::Destroy() -> void
{
    for (MCMoverGroup*& group : Groups)
    {
        if (group != nullptr)
        {
            group->Destroy();
            delete group;
        }

        group = nullptr;
    }
}

auto MCCommander::SetGroup(int32_t groupId, int32_t numMovers, MCMover** moverList, int32_t point) -> int32_t
{
    Assert(groupId >= 0 && groupId < MAX_GROUPS_PER_COMMANDER, 0, " Commander::bad id in setGroup ");
    MCMoverGroup* group = Groups[groupId];
    Assert(group != nullptr, 0, " Commander::setGroup has null group ");
    group->Disband();

    for (int32_t i = 0; i < numMovers; i++)
    {
        MCMover* mover = moverList[i];

        if (mover->Group != nullptr)
        {
            mover->Group->Remove(mover);
        }

        group->Add(mover);
    }

    if (point < numMovers && point > -1)
    {
        group->SetPoint(moverList[point]);
    }

    return 0;
}

auto MCCommander::SetNetPlayerId(int32_t playerId) -> void
{
    for (MCMoverGroup* group : Groups)
    {
        for (int32_t i = 0; i < group->NumMovers; i++)
        {
            group->Movers[i]->NetPlayerId = playerId;
        }
    }
}

auto MCCommander::AddToGui(int visible) -> void
{
    for (int32_t i = 0; i < 4; i++)
    {
        Groups[i]->AddToGui(visible);
    }
}

auto MCCommander::SetNumSmallStrikes(int32_t strikes) -> void
{
    NumSmallStrikes = strikes;
    RedrawTacticalMap(this);
}

auto MCCommander::SetNumLargeStrikes(int32_t strikes) -> void
{
    NumLargeStrikes = strikes;
    RedrawTacticalMap(this);
}

auto MCCommander::SetNumSensorStrikes(int32_t strikes) -> void
{
    NumSensorStrikes = strikes;
    RedrawTacticalMap(this);
}

auto MCCommander::SetNumCameraDrones(int32_t drones) -> void
{
    NumCameraDrones = drones;
    RedrawTacticalMap(this);
}
