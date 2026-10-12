#include "stdafx.h"
#include "object/MCCommander.h"
#include "lib/MCFatal.h"
#include "object/MCForces.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"
#include "terrain/MCTacticalMap.h"
#include "terrain/MCTerrain.h"

namespace
{
    /// <summary>The home commander's support changed: redraw the tactical map.</summary>
    void RedrawTacticalMap(const MCCommander& commander)
    {
        if (TacticalMap() != nullptr && &commander == HomeCommander().get())
        {
            TacticalMap()->RefreshPage();
        }
    }
}

MCCommander::MCCommander(int32_t id) : Id(id)
{
    for (int32_t i = 0; i < MaxGroups; i++)
    {
        Groups[i] = std::make_shared<MCMoverGroup>(i);
    }
}

auto MCCommander::SetGroup(int32_t groupId, std::span<const std::shared_ptr<MCMover>> movers, int32_t point) -> int32_t
{
    Assert(groupId >= 0 && groupId < MaxGroups, 0, " Commander::bad id in setGroup ");
    MCMoverGroup& group = *Groups[groupId];
    group.Disband();

    for (const std::shared_ptr<MCMover>& mover : movers)
    {
        if (std::shared_ptr<MCMoverGroup> oldGroup = mover->Group.lock())
        {
            oldGroup->Remove(*mover);
        }

        group.Add(mover);
    }

    if (point < std::ssize(movers) && point > -1)
    {
        group.SetPoint(movers[point]);
    }

    return 0;
}

auto MCCommander::SetNetPlayerId(int32_t playerId) -> void
{
    for (const std::shared_ptr<MCMoverGroup>& group : Groups)
    {
        for (const std::shared_ptr<MCMover>& mover : group->GetMovers())
        {
            mover->NetPlayerId = playerId;
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
    RedrawTacticalMap(*this);
}

auto MCCommander::SetNumLargeStrikes(int32_t strikes) -> void
{
    NumLargeStrikes = strikes;
    RedrawTacticalMap(*this);
}

auto MCCommander::SetNumSensorStrikes(int32_t strikes) -> void
{
    NumSensorStrikes = strikes;
    RedrawTacticalMap(*this);
}

auto MCCommander::SetNumCameraDrones(int32_t drones) -> void
{
    NumCameraDrones = drones;
    RedrawTacticalMap(*this);
}
