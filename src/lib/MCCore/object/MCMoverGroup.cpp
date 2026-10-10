#include "stdafx.h"
#include "object/MCMoverGroup.h"
#include "ai/MCMoveSystem.h"
#include "iface/MCTacticalInterface.h"
#include "lib/MCFatal.h"
#include "main/MCMissionGlobals.h"
#include "network/MCMultiPlayer.h"
#include "object/MCObjectSystem.h"
#include "object/MCSortList.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"
#include "object/MCMechWarrior.h"

const std::array<int8_t, 162> CellSpiralIncrement = {
    -1, 0, 0,  1,  1,  0,  1,  0,  0,  -1, 0,  -1, -1, 0,  -1, 0,  -1, 0,  0, 1,  0, 1,  0, 1,  1, 0,  1,
    0,  1, 0,  1,  0,  0,  -1, 0,  -1, 0,  -1, 0,  -1, -1, 0,  -1, 0,  -1, 0, -1, 0, -1, 0, 0,  1, 0,  1,
    0,  1, 0,  1,  0,  1,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  0, -1, 0, -1, 0, -1, 0, -1, 0,
    -1, 0, -1, -1, 0,  -1, 0,  -1, 0,  -1, 0,  -1, 0,  -1, 0,  -1, 0,  0,  1, 0,  1, 0,  1, 0,  1, 0,  1,
    0,  1, 0,  1,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1, 0,  0, -1, 0, -1, 0, -1, 0,
    -1, 0, -1, 0,  -1, 0,  -1, 0,  -1, -1, 0,  -1, 0,  -1, 0,  -1, 0,  -1, 0, -1, 0, -1, 0, -1, 0, 0,  0};

namespace
{
    /// <summary>
    /// Per number of followers (1-4), the first of their formation slots in <see cref="FormationOffsets"/>. Entry 0
    /// is read but never used.
    /// </summary>
    constexpr std::array<int32_t, 5> FormationStart = {32, 0, 1, 3, 6};

    /// <summary>The formation slots behind the goal: angle and distance.</summary>
    constexpr float FormationOffsets[10][2] = {{180.0f, 50.0f}, {-135.0f, 50.0f}, {135.0f, 50.0f},  {-135.0f, 50.0f},
                                               {135.0f, 50.0f}, {180.0f, 50.0f},  {-135.0f, 50.0f}, {135.0f, 50.0f},
                                               {180.0f, 50.0f}, {180.0f, 75.0f}};

    /// <summary>A cell CalcJumpGoals may use.</summary>
    constexpr int32_t JumpCellOpen = -1;
    /// <summary>A cell CalcJumpGoals may not use.</summary>
    constexpr int32_t JumpCellBlocked = -2;
    /// <summary>A goal CalcJumpGoals found no cell for (each coordinate).</summary>
    constexpr float NoJumpGoal = -99999.0f;

    /// <summary>Blocks the cells of <paramref name="list"/>'s live mechs (other than <paramref name="dfaTarget"/>)
    /// that fall in the 9x9 area.</summary>
    void BlockMechCells(const MCObjectList* list, MCGameObject* dfaTarget, std::array<int32_t, 81>& cells,
                        int32_t areaCellR, int32_t areaCellC)
    {
        for (MCBaseObject* object : *list)
        {
            auto* mech = static_cast<MCGameObject*>(object);

            if (mech->ObjectClass == MCObjectClass::Elemental || mech == dfaTarget || mech->IsDisabled() != 0)
            {
                continue;
            }

            const MCObjectPosition* position = mech->GetObjPosition();
            const int32_t row = position->MapCellR - areaCellR;
            const int32_t col = position->MapCellC - areaCellC;

            if (row >= 0 && row < 9 && col >= 0 && col < 9)
            {
                cells[row * 9 + col] = JumpCellBlocked;
            }
        }
    }
}

auto MCMoverGroup::Add(MCMover* mover) -> int
{
    if (NumMovers() == MaxMovers)
    {
        Fatal(0, " MoverGroup.add: Group too big ");
    }

    Movers.push_back(mover);
    mover->SetGroup(this);
    return 1;
}

auto MCMoverGroup::Remove(MCMover* mover) -> int
{
    if (mover == Point)
    {
        Disband();
        return 1;
    }

    const auto position = std::ranges::find(Movers, mover);

    if (position == Movers.end())
    {
        return 0;
    }

    mover->SetGroup(nullptr);
    *position = Movers.back();
    Movers.pop_back();
    return 1;
}

auto MCMoverGroup::IsMember(MCMover* mover) -> int
{
    return std::ranges::contains(Movers, mover) ? 1 : 0;
}

auto MCMoverGroup::Disband() -> void
{
    for (MCMover* member : Movers)
    {
        member->SetGroup(nullptr);
    }

    if (Point != nullptr)
    {
        TacticalInterface()->SetPoint(Point->PartId, false);
    }

    Point = nullptr;
    Movers.clear();
}

auto MCMoverGroup::SetPoint(MCMover* mover) -> int32_t
{
    if (IsMember(mover) != 0)
    {
        if (Point != nullptr)
        {
            TacticalInterface()->SetPoint(Point->PartId, false);
        }

        Point = mover;
        TacticalInterface()->SetPoint(mover->PartId, true);
    }

    return 0;
}

auto MCMoverGroup::SelectPoint(int excludePoint) -> MCMover*
{
    for (int32_t i = 0; i < NumMovers(); i++)
    {
        if (excludePoint != 0 && Movers[i] == Point)
        {
            continue;
        }

        const MCMechWarrior* pilot = Movers[i]->GetPilot();

        if (pilot != nullptr && pilot->Wounds < 6.0f)
        {
            SetPoint(Movers[i]);
            return Movers[i];
        }
    }

    SetPoint(nullptr);
    return nullptr;
}

auto MCMoverGroup::GetMovers(MCMover** moverList) -> int32_t
{
    for (int32_t i = 0; i < NumMovers(); i++)
    {
        moverList[i] = Movers[i];
    }

    return NumMovers();
}

auto MCMoverGroup::GetPointPilot() const -> MCMechWarrior*
{
    if (Point != nullptr)
    {
        return Point->GetPilot();
    }

    return nullptr;
}

auto MCMoverGroup::StatusCount(int32_t* counts) -> void
{
    for (int32_t i = 0; i < NumMovers(); i++)
    {
        MCMover* mover = Movers[i];
        const MCMechWarrior* pilot = mover->GetPilot();

        if (mover->GetExists() == 0)
        {
            counts[8]++;
        }
        else if (mover->GetAwake() == 0)
        {
            counts[7]++;
        }
        else if (pilot == nullptr || pilot->Status != 2)
        {
            counts[static_cast<uint8_t>(mover->Status)]++;
        }
        else
        {
            counts[6]++;
        }
    }
}

auto MCMoverGroup::AddToGui(int visible) -> void
{
    for (int32_t i = 0; i < NumMovers(); i++)
    {
        TacticalInterface()->AddMech(Movers[i]->PartId, Id, Movers[i]->GetAwake() != 0, visible != 0);
    }
}

auto CalcJumpGoals(MCVector3D goal, int32_t numGoals, MCVector3D* goalList, MCGameObject* dfaTarget) -> int32_t
{
    int32_t numPlaced = 0;
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    MCScenarioMap::WorldToMapPos(goal, tileR, tileC, cellR, cellC);
    // Port fix: the original reads these from tileMulMAPCELL_DIM (tile * 3), past its ends for a goal on the map's
    // first row or column (tile - 1) or off the map. The port multiplies.
    const int32_t goalCellR = cellR + tileR * MapCellDim;
    const int32_t goalCellC = cellC + tileC * MapCellDim;
    // The 9x9 cells of the 3x3 tiles around the goal's tile.
    const int32_t areaCellR = (tileR - 1) * MapCellDim;
    const int32_t areaCellC = (tileC - 1) * MapCellDim;

    std::array<int32_t, 81> cells;

    for (int32_t tileRow = 0; tileRow < 3; tileRow++)
    {
        const int32_t row = tileR - 1 + tileRow;

        for (int32_t tileCol = 0; tileCol < 3; tileCol++)
        {
            const int32_t col = tileC - 1 + tileCol;
            int32_t* block = &cells[tileRow * 27 + tileCol * 3];

            if (row <= -1 || row >= GameMap()->Height || col <= -1 || col >= GameMap()->Width)
            {
                for (int32_t r = 0; r < 3; r++)
                {
                    block[r * 9] = JumpCellBlocked;
                    block[r * 9 + 1] = JumpCellBlocked;
                    block[r * 9 + 2] = JumpCellBlocked;
                }

                continue;
            }

            Assert(row < GameMap()->Height && col < GameMap()->Width, 0, " Map Tile out of bounds ");
            MCMapTile tile = GameMap()->Map[GameMap()->Width * row + col];

            for (int32_t r = 0; r < 3; r++)
            {
                for (int32_t c = 0; c < 3; c++)
                {
                    block[r * 9 + c] = tile.GetCellPassable(r, c) == 0 ? JumpCellBlocked : JumpCellOpen;
                }
            }

            // Bridges: their rails (the sides the road doesn't cross), or the whole tile.
            const uint32_t overlay = tile.Overlay & 0x7f;

            if (OverlayIsBridge[overlay] == 0)
            {
                continue;
            }

            switch (overlay)
            {
                case 0x25:
                case 0x37:
                {
                    for (int32_t r = 0; r < 3; r++)
                    {
                        block[r * 9] = JumpCellBlocked;
                        block[r * 9 + 2] = JumpCellBlocked;
                    }
                    break;
                }
                case 0x26:
                case 0x28:
                case 0x38:
                case 0x3a:
                {
                    for (int32_t r = 0; r < 3; r++)
                    {
                        block[r * 9] = JumpCellBlocked;
                        block[r * 9 + 1] = JumpCellBlocked;
                        block[r * 9 + 2] = JumpCellBlocked;
                    }
                    break;
                }
                case 0x27:
                case 0x39:
                {
                    for (int32_t c = 0; c < 3; c++)
                    {
                        block[c] = JumpCellBlocked;
                        block[18 + c] = JumpCellBlocked;
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    BlockMechCells(InnerSphereMechList(), dfaTarget, cells, areaCellR, areaCellC);
    BlockMechCells(ClanMechList(), dfaTarget, cells, areaCellR, areaCellC);

    // Each goal takes the first open cell on the spiral out from the goal's cell.
    for (int32_t i = 0; i < numGoals; i++)
    {
        int32_t row = goalCellR - areaCellR;
        int32_t col = goalCellC - areaCellC;
        int32_t step = 0;
        bool found = true;

        while (cells[row * 9 + col] != JumpCellOpen)
        {
            do
            {
                if (step == 162)
                {
                    found = false;
                    break;
                }

                row += CellSpiralIncrement[step];
                col += CellSpiralIncrement[step + 1];
                step += 2;
            } while (row < 0 || row > 8 || col < 0 || col > 8);

            if (!found)
            {
                break;
            }
        }

        if (!found)
        {
            goalList[i].X = NoJumpGoal;
            goalList[i].Y = NoJumpGoal;
            goalList[i].Z = NoJumpGoal;
            continue;
        }

        cells[row * 9 + col] = i;
        goalList[i] = MapCellToWorldPos(row + areaCellR, col + areaCellC);
        numPlaced++;
    }

    return numPlaced;
}

auto MCMoverGroup::CalcMemberJumpGoals(MCVector3D goal, MCVector3D* goalList, MCGameObject* dfaTarget) const -> int32_t
{
    return CalcJumpGoals(goal, NumMovers(), goalList, dfaTarget);
}

auto MCMoverGroup::HandleTacticalOrder(MCTacticalOrder tacOrder, int32_t priority, MCVector3D* destinations,
                                       int queueGroupOrder) -> int32_t
{
    if (Movers.empty())
    {
        return 0;
    }

    if (queueGroupOrder != 0)
    {
        tacOrder.Pack();
    }

    int jumping = 0;
    int formation = 0;
    const MCVector3D goal = tacOrder.GetWayPoint(0);
    GetPoint();

    // An attack by jumping (method 1) becomes a jump onto the target.
    if (tacOrder.Code == MCTacticalOrderCode::AttackObject)
    {
        if (tacOrder.AttackParams.Method == 1)
        {
            tacOrder.Code = MCTacticalOrderCode::JumpToObject;
            tacOrder.MoveParams.Wait = 0;
            tacOrder.MoveParams.WayPath.Mode[0] = 0;

            if (tacOrder.Target != nullptr)
            {
                tacOrder.SetWayPoint(0, tacOrder.Target->GetPosition());
            }
        }
    }

    if (tacOrder.Code == MCTacticalOrderCode::JumpToObject)
    {
        MCGameObject* target = tacOrder.Target;
        tacOrder.Code = MCTacticalOrderCode::JumpToPoint;
        Assert(target != nullptr, 0, " JumpToObject is NULL ");
        tacOrder.SetWayPoint(0, target->GetPosition());
    }

    std::array<MCVector3D, MaxMovers> jumpGoals;

    switch (tacOrder.Code)
    {
        case MCTacticalOrderCode::MoveToPoint:
        case MCTacticalOrderCode::MoveToObject:
        {
            // Moving: the members set off in order of distance from the goal, the point first.
            formation = 1;
            // Sorted in a list as long as the 100-entry one the movers shared in MCX.EXE.
            MCSortList sortListStorage(100);
            MCSortList* sortList = &sortListStorage;
            sortList->Clear(false);
            int32_t numSorted = 0;

            for (int32_t i = 0; i < NumMovers(); i++)
            {
                MCMover* mover = Movers[i];

                if (mover == nullptr || mover->IsDisabled() != 0)
                {
                    continue;
                }

                if (numSorted >= 0 && numSorted < sortList->NumItems())
                {
                    sortList->List[numSorted].Id = i;
                }

                MCVector3D goalPosition = goal;
                const auto distance = static_cast<float>(mover->DistanceFrom(goalPosition));

                if (numSorted >= 0 && numSorted < sortList->NumItems())
                {
                    sortList->List[numSorted].Value = distance;
                }

                numSorted++;
            }

            sortList->Sort(false);

            // The followers' formation slots. The original works them out but orders every member to the goal itself.
            int32_t numFollowers = numSorted - 1;

            if (numFollowers > 4)
            {
                numFollowers = 4;
            }

            if (numFollowers > 0)
            {
                MCVector3D formationGoals[4];
                const int32_t first = FormationStart[numFollowers];

                for (int32_t i = 0; i < numFollowers; i++)
                {
                    formationGoals[i] = RelativePositionToPoint(goal, FormationOffsets[first + i][0],
                                                                FormationOffsets[first + i][1], 2);
                }
            }

            int32_t rank = 1;

            for (int32_t i = 0; i < numSorted; i++)
            {
                MCMover* mover = Movers[sortList->List[i].Id];

                if (mover == Point)
                {
                    mover->SelectionIndex = 0;
                }
                else
                {
                    mover->SelectionIndex = rank++;
                }
            }
            break;
        }

        case MCTacticalOrderCode::JumpToPoint:
        case MCTacticalOrderCode::JumpToObject:
        {
            jumping = 1;

            if (destinations == nullptr)
            {
                CalcMemberJumpGoals(tacOrder.GetWayPoint(0), jumpGoals.data(), tacOrder.Target);
            }
            else
            {
                for (int32_t i = 0; i < NumMovers(); i++)
                {
                    jumpGoals[i] = destinations[i];
                }
            }

            // A member with no goal stays put.
            for (int32_t i = 0; i < NumMovers(); i++)
            {
                Movers[i]->SelectionIndex = jumpGoals[i].X <= -99000.0f ? -2 : 0;
            }
            break;
        }
        case MCTacticalOrderCode::Wait:
        case MCTacticalOrderCode::TraversePath:
        case MCTacticalOrderCode::PatrolPath:
        case MCTacticalOrderCode::Escort:
        case MCTacticalOrderCode::Follow:
        case MCTacticalOrderCode::Guard:
        case MCTacticalOrderCode::Stop:
        case MCTacticalOrderCode::PowerUp:
        case MCTacticalOrderCode::PowerDown:
        case MCTacticalOrderCode::WayPointsDone:
        case MCTacticalOrderCode::Eject:
        case MCTacticalOrderCode::AttackObject:
        case MCTacticalOrderCode::AttackPoint:
        case MCTacticalOrderCode::HoldFire:
        case MCTacticalOrderCode::Withdraw:
        case MCTacticalOrderCode::Capture:
        case MCTacticalOrderCode::Refit:
        case MCTacticalOrderCode::GetFixed:
        case MCTacticalOrderCode::LoadIntoCarrier:
            break;
        default:
        {
            Assert(false, 1,
                   std::format("Unit::handleTacticalOrder->Bad TacOrder Code ({})", static_cast<int>(tacOrder.Code)));
            return 1;
        }
    }

    tacOrder.UnitOrder = 1;

    for (int32_t i = 0; i < NumMovers(); i++)
    {
        MCMover* mover = Movers[i];

        if (mover == nullptr || mover->IsDisabled() != 0)
        {
            continue;
        }

        const int32_t delay = mover->SelectionIndex;

        if (delay != -2)
        {
            tacOrder.SelectionIndex = delay;

            if (delay != -1)
            {
                if (formation != 0)
                {
                    tacOrder.SetWayPoint(0, goal);
                }
                else if (jumping != 0)
                {
                    tacOrder.SetWayPoint(0, jumpGoals[i]);
                }

                tacOrder.DelayedTime = static_cast<float>(mover->SelectionIndex) * DelayedOrderTime + ScenarioTime;
            }

            if (MultiPlayer() != nullptr)
            {
                tacOrder.Id = 0;
                tacOrder.SetId(mover->GetPilot());
            }

            switch (tacOrder.Origin)
            {
                case MCOrderOrigin::Player:
                {
                    if (queueGroupOrder != 0)
                    {
                        mover->GetPilot()->AddQueuedTacOrder(tacOrder);
                        mover->GetPilot()->TacOrderQueueExecuting = true;
                    }
                    else
                    {
                        mover->GetPilot()->SetPlayerTacOrder(tacOrder, 0);
                    }
                    break;
                }
                case MCOrderOrigin::Commander:
                    mover->GetPilot()->SetGeneralTacOrder(tacOrder);
                    break;
                case MCOrderOrigin::Self:
                    mover->GetPilot()->SetAlarmTacOrder(tacOrder, priority);
                    break;
                default:
                    break;
            }
        }

        mover->SelectionIndex = -1;
    }

    return 0;
}

auto MCMoverGroup::OrderMoveToPoint(int setTacOrder, MCOrderOrigin origin, MCVector3D location, uint32_t params)
    -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < NumMovers(); i++)
    {
        MCMover* mover = Movers[i];
        Assert(mover != nullptr, 0, " MoverGroup.orderMoveToPoint: NULL mover ");
        MCMechWarrior* pilot = mover->GetPilot();

        if (pilot != nullptr)
        {
            result = pilot->OrderMoveToPoint(1, setTacOrder, origin, location, -1, params);
        }
    }

    return result;
}

auto MCMoverGroup::OrderMoveToObject(int setTacOrder, MCOrderOrigin origin, MCGameObject* target, uint32_t params)
    -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < NumMovers(); i++)
    {
        MCMover* mover = Movers[i];
        Assert(mover != nullptr, 0, " MoverGroup.orderMoveToObject: NULL mover ");
        MCMechWarrior* pilot = mover->GetPilot();

        if (pilot != nullptr)
        {
            result = pilot->OrderMoveToObject(1, setTacOrder, origin, target, -1, params);
        }
    }

    return result;
}

auto MCMoverGroup::OrderPowerDown(MCOrderOrigin origin) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < NumMovers(); i++)
    {
        MCMover* mover = Movers[i];
        Assert(mover != nullptr, 0, " MoverGroup.orderPowerDown: NULL mover ");
        MCMechWarrior* pilot = mover->GetPilot();

        if (pilot != nullptr)
        {
            result = pilot->OrderPowerDown(1, origin);
        }
    }

    return result;
}

auto MCMoverGroup::OrderPowerUp(MCOrderOrigin origin) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < NumMovers(); i++)
    {
        MCMover* mover = Movers[i];
        Assert(mover != nullptr, 0, " MoverGroup.orderPowerUp: NULL mover ");
        MCMechWarrior* pilot = mover->GetPilot();

        if (pilot != nullptr)
        {
            result = pilot->OrderPowerUp(1, origin);
        }
    }

    return result;
}

auto MCMoverGroup::OrderAttackObject(MCOrderOrigin origin, MCGameObject* target, int32_t attackType,
                                     int32_t attackMethod, int32_t attackRange, int32_t aimLocation, uint32_t params)
    -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < NumMovers(); i++)
    {
        MCMover* mover = Movers[i];
        Assert(mover != nullptr, 0, " MoverGroup.orderAttackObject: NULL mover ");
        MCMechWarrior* pilot = mover->GetPilot();

        if (pilot != nullptr)
        {
            result =
                pilot->OrderAttackObject(1, origin, target, attackType, attackMethod, attackRange, aimLocation, params);
        }
    }

    return result;
}

auto MCMoverGroup::OrderWithdraw(MCOrderOrigin origin, MCVector3D location) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < NumMovers(); i++)
    {
        MCMover* mover = Movers[i];
        Assert(mover != nullptr, 0, " MoverGroup.orderWithdraw: NULL mover ");
        MCMechWarrior* pilot = mover->GetPilot();

        if (pilot != nullptr)
        {
            result = pilot->OrderWithdraw(1, origin, location);
        }
    }

    return result;
}

auto MCMoverGroup::TriggerAlarm(MCPilotAlarmType alarm, uint32_t triggerId) -> void
{
    for (int32_t i = 0; i < NumMovers(); i++)
    {
        MCMechWarrior* pilot = Movers[i]->GetPilot();

        if (pilot != nullptr)
        {
            pilot->TriggerAlarm(alarm, triggerId);
        }
    }
}

auto MCMoverGroup::HandleMateDestroyed(uint32_t mateId) -> int32_t
{
    TriggerAlarm(MCPilotAlarmType::DeathOfMate, mateId);
    return 0;
}

auto MCMoverGroup::HandleMateEjected(uint32_t) -> int32_t
{
    return 0;
}

auto MCMoverGroup::HandleMateFiredWeapon(uint32_t mateId) -> void
{
    TriggerAlarm(MCPilotAlarmType::MateFiredWeapon, mateId);
}
