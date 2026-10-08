#include "stdafx.h"
#include "object/MCCommander.h"
#include "lib/MCFatal.h"
#include "object/MCForces.h"
#include "object/mover.h"
#include "terrain/MCTacticalMap.h"
#include "terrain/MCTerrain.h"

namespace
{
    /// <summary>The home commander's support changed: redraw the tactical map.</summary>
    void RedrawTacticalMap(const MCCommander* commander)
    {
        if (TacticalMap() != nullptr && commander == HomeCommander())
        {
            TacticalMap()->RefreshPage();
        }
    }
}

MCCommander::MCCommander(int32_t id) : Id(id)
{
    for (int32_t i = 0; i < MaxGroups; i++)
    {
        Groups[i].SetId(i);
    }
}

auto MCCommander::SetGroup(int32_t groupId, int32_t numMovers, MCMover** moverList, int32_t point) -> int32_t
{
    Assert(groupId >= 0 && groupId < MaxGroups, 0, " Commander::bad id in setGroup ");
    MCMoverGroup& group = Groups[groupId];
    group.Disband();

    for (int32_t i = 0; i < numMovers; i++)
    {
        MCMover* mover = moverList[i];

        if (mover->Group != nullptr)
        {
            mover->Group->Remove(mover);
        }

        group.Add(mover);
    }

    if (point < numMovers && point > -1)
    {
        group.SetPoint(moverList[point]);
    }

    return 0;
}

auto MCCommander::SetNetPlayerId(int32_t playerId) -> void
{
    for (MCMoverGroup& group : Groups)
    {
        for (MCMover* mover : group.Movers)
        {
            mover->NetPlayerId = playerId;
        }
    }
}

auto MCCommander::AddToGui(int visible) -> void
{
    for (int32_t i = 0; i < 4; i++)
    {
        Groups[i].AddToGui(visible);
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
