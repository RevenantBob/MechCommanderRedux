#include "stdafx.h"
#include "object/group.h"
#include "ai/move.h"
#include "iface/iface.h"
#include "lib/aerror.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "object/mover.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/sortlist.h"
#include "object/warrior.h"

char CellSpiralIncrement[162] = {
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
    constexpr int32_t FormationStart[5] = {32, 0, 1, 3, 6};

    /// <summary>The formation slots behind the goal: angle and distance.</summary>
    constexpr float FormationOffsets[10][2] = {{180.0f, 50.0f}, {-135.0f, 50.0f}, {135.0f, 50.0f},  {-135.0f, 50.0f},
                                               {135.0f, 50.0f}, {180.0f, 50.0f},  {-135.0f, 50.0f}, {135.0f, 50.0f},
                                               {180.0f, 50.0f}, {180.0f, 75.0f}};

    /// <summary>A cell CalcJumpGoals may use.</summary>
    constexpr int32_t JUMP_CELL_OPEN = -1;
    /// <summary>A cell CalcJumpGoals may not use.</summary>
    constexpr int32_t JUMP_CELL_BLOCKED = -2;
    /// <summary>A goal CalcJumpGoals found no cell for (each coordinate).</summary>
    constexpr float NO_JUMP_GOAL = -99999.0f;

    /// <summary>Blocks the cells of <paramref name="list"/>'s live mechs (other than <paramref name="dfaTarget"/>)
    /// that fall in the 9x9 area.</summary>
    void BlockMechCells(MCObjectQueueNode* list, MCGameObject* dfaTarget, int32_t (&cells)[81], int32_t areaCellR,
                        int32_t areaCellC)
    {
        for (MCBaseObject* object = list->Head; object != nullptr; object = object->Next)
        {
            auto* mech = static_cast<MCGameObject*>(object);

            if (mech->ObjectClass == ELEMENTAL || mech == dfaTarget || mech->IsDisabled() != 0)
            {
                continue;
            }

            const MCObjectPosition* position = mech->GetObjPosition();
            const int32_t row = position->MapCellR - areaCellR;
            const int32_t col = position->MapCellC - areaCellC;

            if (row >= 0 && row < 9 && col >= 0 && col < 9)
            {
                cells[row * 9 + col] = JUMP_CELL_BLOCKED;
            }
        }
    }
}

auto MCMoverGroup::Init() -> void
{
    Id = -1;
    NumMovers = 0;
    Point = nullptr;
    DisbandOnNoPoint = 0;
}

auto MCMoverGroup::Destroy() -> void
{
}

auto MCMoverGroup::Add(MCMover* mover) -> int
{
    if (NumMovers == MAX_MOVERGROUP_COUNT)
    {
        Fatal(0, " MoverGroup.add: Group too big ");
        return 0;
    }

    Movers[NumMovers] = mover;
    NumMovers++;
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

    for (int32_t i = 0; i < NumMovers; i++)
    {
        if (Movers[i] == mover)
        {
            mover->SetGroup(nullptr);
            Movers[i] = Movers[NumMovers - 1];
            Movers[NumMovers - 1] = nullptr;
            NumMovers--;
            return 1;
        }
    }

    return 0;
}

auto MCMoverGroup::IsMember(MCMover* mover) -> int
{
    for (int32_t i = 0; i < NumMovers; i++)
    {
        if (Movers[i] == mover)
        {
            return 1;
        }
    }

    return 0;
}

auto MCMoverGroup::Disband() -> void
{
    for (int32_t i = 0; i < NumMovers; i++)
    {
        Movers[i]->SetGroup(nullptr);
    }

    if (Point != nullptr)
    {
        TheInterface->SetPoint(Point->PartId, 0);
    }

    Point = nullptr;
    NumMovers = 0;
}

auto MCMoverGroup::SetPoint(MCMover* mover) -> int32_t
{
    if (IsMember(mover) != 0)
    {
        if (Point != nullptr)
        {
            TheInterface->SetPoint(Point->PartId, 0);
        }

        Point = mover;
        TheInterface->SetPoint(mover->PartId, 1);
    }

    return 0;
}

auto MCMoverGroup::SelectPoint(int excludePoint) -> MCMover*
{
    for (int32_t i = 0; i < NumMovers; i++)
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
    for (int32_t i = 0; i < NumMovers; i++)
    {
        moverList[i] = Movers[i];
    }

    return NumMovers;
}

auto MCMoverGroup::GetPointPilot() -> MCMechWarrior*
{
    if (Point != nullptr)
    {
        return Point->GetPilot();
    }

    return nullptr;
}

auto MCMoverGroup::StatusCount(int32_t* counts) -> void
{
    for (int32_t i = 0; i < NumMovers; i++)
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
    for (int32_t i = 0; i < NumMovers; i++)
    {
        TheInterface->AddMech(Movers[i]->PartId, Id, Movers[i]->GetAwake(), visible);
    }
}

auto CalcJumpGoals(MCVector3D goal, int32_t numGoals, MCVector3D* goalList, MCGameObject* dfaTarget) -> int32_t
{
    int32_t numPlaced = 0;
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    GameMap->WorldToMapPos(goal, tileR, tileC, cellR, cellC);
    // Port fix: the original reads these from tileMulMAPCELL_DIM (tile * 3), past its ends for a goal on the map's
    // first row or column (tile - 1) or off the map. The port multiplies.
    const int32_t goalCellR = cellR + tileR * MAPCELL_DIM;
    const int32_t goalCellC = cellC + tileC * MAPCELL_DIM;
    // The 9x9 cells of the 3x3 tiles around the goal's tile.
    const int32_t areaCellR = (tileR - 1) * MAPCELL_DIM;
    const int32_t areaCellC = (tileC - 1) * MAPCELL_DIM;

    int32_t cells[81];

    for (int32_t tileRow = 0; tileRow < 3; tileRow++)
    {
        const int32_t row = tileR - 1 + tileRow;

        for (int32_t tileCol = 0; tileCol < 3; tileCol++)
        {
            const int32_t col = tileC - 1 + tileCol;
            int32_t* block = &cells[tileRow * 27 + tileCol * 3];

            if (row <= -1 || row >= GameMap->Height || col <= -1 || col >= GameMap->Width)
            {
                for (int32_t r = 0; r < 3; r++)
                {
                    block[r * 9] = JUMP_CELL_BLOCKED;
                    block[r * 9 + 1] = JUMP_CELL_BLOCKED;
                    block[r * 9 + 2] = JUMP_CELL_BLOCKED;
                }

                continue;
            }

            Assert(row < GameMap->Height && col < GameMap->Width, 0, " Map Tile out of bounds ");
            MCMapTile tile = GameMap->Map[GameMap->Width * row + col];

            for (int32_t r = 0; r < 3; r++)
            {
                for (int32_t c = 0; c < 3; c++)
                {
                    block[r * 9 + c] = tile.GetCellPassable(r, c) == 0 ? JUMP_CELL_BLOCKED : JUMP_CELL_OPEN;
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
                        block[r * 9] = JUMP_CELL_BLOCKED;
                        block[r * 9 + 2] = JUMP_CELL_BLOCKED;
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
                        block[r * 9] = JUMP_CELL_BLOCKED;
                        block[r * 9 + 1] = JUMP_CELL_BLOCKED;
                        block[r * 9 + 2] = JUMP_CELL_BLOCKED;
                    }
                    break;
                }
                case 0x27:
                case 0x39:
                {
                    for (int32_t c = 0; c < 3; c++)
                    {
                        block[c] = JUMP_CELL_BLOCKED;
                        block[18 + c] = JUMP_CELL_BLOCKED;
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    BlockMechCells(InnerSphereMechList, dfaTarget, cells, areaCellR, areaCellC);
    BlockMechCells(ClanMechList, dfaTarget, cells, areaCellR, areaCellC);

    // Each goal takes the first open cell on the spiral out from the goal's cell.
    for (int32_t i = 0; i < numGoals; i++)
    {
        int32_t row = goalCellR - areaCellR;
        int32_t col = goalCellC - areaCellC;
        int32_t step = 0;
        bool found = true;

        while (cells[row * 9 + col] != JUMP_CELL_OPEN)
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
            goalList[i].X = NO_JUMP_GOAL;
            goalList[i].Y = NO_JUMP_GOAL;
            goalList[i].Z = NO_JUMP_GOAL;
            continue;
        }

        cells[row * 9 + col] = i;
        MapCellToWorldPos(row + areaCellR, col + areaCellC, goalList[i]);
        numPlaced++;
    }

    return numPlaced;
}

auto MCMoverGroup::CalcMemberJumpGoals(MCVector3D goal, MCVector3D* goalList, MCGameObject* dfaTarget) -> int32_t
{
    return CalcJumpGoals(goal, NumMovers, goalList, dfaTarget);
}

auto MCMoverGroup::HandleTacticalOrder(MCTacticalOrder tacOrder, int32_t priority, MCVector3D* destinations,
                                       int queueGroupOrder) -> int32_t
{
    if (NumMovers == 0)
    {
        tacOrder.Destroy();
        return 0;
    }

    if (queueGroupOrder != 0)
    {
        tacOrder.Pack(nullptr, nullptr);
    }

    int jumping = 0;
    int formation = 0;
    const MCVector3D goal = tacOrder.GetWayPoint(0);
    GetPoint();

    // An attack by jumping (method 1) becomes a jump onto the target.
    if (tacOrder.Code == TACTICAL_ORDER_ATTACK_OBJECT)
    {
        if (tacOrder.AttackParams.Method == 1)
        {
            tacOrder.Code = TACTICAL_ORDER_JUMPTO_OBJECT;
            tacOrder.MoveParams.Wait = 0;
            tacOrder.MoveParams.WayPath.Mode[0] = 0;

            if (tacOrder.Target != nullptr)
            {
                tacOrder.SetWayPoint(0, tacOrder.Target->GetPosition());
            }
        }
    }

    if (tacOrder.Code == TACTICAL_ORDER_JUMPTO_OBJECT)
    {
        MCGameObject* target = tacOrder.Target;
        tacOrder.Code = TACTICAL_ORDER_JUMPTO_POINT;
        Assert(target != nullptr, 0, " JumpToObject is NULL ");
        tacOrder.SetWayPoint(0, target->GetPosition());
    }

    MCVector3D jumpGoals[MAX_MOVERGROUP_COUNT];

    switch (tacOrder.Code)
    {
        case TACTICAL_ORDER_MOVETO_POINT:
        case TACTICAL_ORDER_MOVETO_OBJECT:
        {
            // Moving: the members set off in order of distance from the goal, the point first.
            formation = 1;
            MCSortList* sortList = MCMover::SortList;

            if (sortList == nullptr)
            {
                break;
            }

            sortList->Clear(0);
            int32_t numSorted = 0;

            for (int32_t i = 0; i < NumMovers; i++)
            {
                MCMover* mover = Movers[i];

                if (mover == nullptr || mover->IsDisabled() != 0)
                {
                    continue;
                }

                if (numSorted >= 0 && numSorted < sortList->NumItems)
                {
                    sortList->List[numSorted].Id = i;
                }

                MCVector3D goalPosition = goal;
                const auto distance = static_cast<float>(mover->DistanceFrom(goalPosition));

                if (numSorted >= 0 && numSorted < sortList->NumItems)
                {
                    sortList->List[numSorted].Value = distance;
                }

                numSorted++;
            }

            sortList->Sort(0);

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

        case TACTICAL_ORDER_JUMPTO_POINT:
        case TACTICAL_ORDER_JUMPTO_OBJECT:
        {
            jumping = 1;

            if (destinations == nullptr)
            {
                CalcMemberJumpGoals(tacOrder.GetWayPoint(0), jumpGoals, tacOrder.Target);
            }
            else
            {
                for (int32_t i = 0; i < NumMovers; i++)
                {
                    jumpGoals[i] = destinations[i];
                }
            }

            // A member with no goal stays put.
            for (int32_t i = 0; i < NumMovers; i++)
            {
                Movers[i]->SelectionIndex = jumpGoals[i].X <= -99000.0f ? -2 : 0;
            }
            break;
        }
        case TACTICAL_ORDER_WAIT:
        case TACTICAL_ORDER_TRAVERSE_PATH:
        case TACTICAL_ORDER_PATROL_PATH:
        case TACTICAL_ORDER_ESCORT:
        case TACTICAL_ORDER_FOLLOW:
        case TACTICAL_ORDER_GUARD:
        case TACTICAL_ORDER_STOP:
        case TACTICAL_ORDER_POWERUP:
        case TACTICAL_ORDER_POWERDOWN:
        case TACTICAL_ORDER_WAYPOINTS_DONE:
        case TACTICAL_ORDER_EJECT:
        case TACTICAL_ORDER_ATTACK_OBJECT:
        case TACTICAL_ORDER_ATTACK_POINT:
        case TACTICAL_ORDER_HOLD_FIRE:
        case TACTICAL_ORDER_WITHDRAW:
        case TACTICAL_ORDER_CAPTURE:
        case TACTICAL_ORDER_REFIT:
        case TACTICAL_ORDER_GETFIXED:
        case TACTICAL_ORDER_LOAD_INTO_CARRIER:
            break;
        default:
        {
            char message[256];
            std::snprintf(message, sizeof(message), "Unit::handleTacticalOrder->Bad TacOrder Code (%d)",
                          static_cast<int>(tacOrder.Code));
            Assert(0, 1, message);
            tacOrder.Destroy();
            return 1;
        }
    }

    tacOrder.UnitOrder = 1;

    for (int32_t i = 0; i < NumMovers; i++)
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

            if (MPlayer != nullptr)
            {
                tacOrder.Id = 0;
                tacOrder.SetId(mover->GetPilot());
            }

            switch (tacOrder.Origin)
            {
                case 0:
                {
                    if (queueGroupOrder != 0)
                    {
                        mover->GetPilot()->AddQueuedTacOrder(tacOrder);
                        mover->GetPilot()->TacOrderQueueExecuting = 1;
                    }
                    else
                    {
                        mover->GetPilot()->SetPlayerTacOrder(tacOrder, 0);
                    }
                    break;
                }
                case 1:
                    mover->GetPilot()->SetGeneralTacOrder(tacOrder);
                    break;
                case 2:
                    mover->GetPilot()->SetAlarmTacOrder(tacOrder, priority);
                    break;
                default:
                    break;
            }
        }

        mover->SelectionIndex = -1;
    }

    tacOrder.Destroy();
    return 0;
}

auto MCMoverGroup::OrderMoveToPoint(int setTacOrder, int32_t origin, MCVector3D location, uint32_t params) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < NumMovers; i++)
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

auto MCMoverGroup::OrderMoveToObject(int setTacOrder, int32_t origin, MCGameObject* target, uint32_t params) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < NumMovers; i++)
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

auto MCMoverGroup::OrderTraversePath(int32_t origin, MCWayPath* wayPath, uint32_t params) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < NumMovers; i++)
    {
        MCMover* mover = Movers[i];
        Assert(mover != nullptr, 0, " MoverGroup.orderTraversePath: NULL mover ");
        MCMechWarrior* pilot = mover->GetPilot();

        if (pilot != nullptr)
        {
            result = pilot->OrderTraversePath(1, 1, origin, wayPath, params);
        }
    }

    return result;
}

auto MCMoverGroup::OrderPatrolPath(int32_t origin, MCWayPath* wayPath) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < NumMovers; i++)
    {
        MCMover* mover = Movers[i];
        Assert(mover != nullptr, 0, " MoverGroup.orderPatrolPath: NULL mover ");
        MCMechWarrior* pilot = mover->GetPilot();

        if (pilot != nullptr)
        {
            result = pilot->OrderPatrolPath(1, 1, origin, wayPath);
        }
    }

    return result;
}

auto MCMoverGroup::OrderPowerDown(int32_t origin) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < NumMovers; i++)
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

auto MCMoverGroup::OrderPowerUp(int32_t origin) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < NumMovers; i++)
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

auto MCMoverGroup::OrderAttackObject(int32_t origin, MCGameObject* target, int32_t attackType, int32_t attackMethod,
                                     int32_t attackRange, int32_t aimLocation, uint32_t params) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < NumMovers; i++)
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

auto MCMoverGroup::OrderWithdraw(int32_t origin, MCVector3D location) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < NumMovers; i++)
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

auto MCMoverGroup::OrderEject(int32_t origin) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < NumMovers; i++)
    {
        MCMover* mover = Movers[i];
        // The original reuses orderWithdraw's message.
        Assert(mover != nullptr, 0, " MoverGroup.orderWithdraw: NULL mover ");
        MCMechWarrior* pilot = mover->GetPilot();

        if (pilot != nullptr)
        {
            result = pilot->OrderEject(1, 1, origin);
        }
    }

    return result;
}

auto MCMoverGroup::TriggerAlarm(int32_t alarmCode, uint32_t triggerId) -> void
{
    for (int32_t i = 0; i < NumMovers; i++)
    {
        MCMechWarrior* pilot = Movers[i]->GetPilot();

        if (pilot != nullptr)
        {
            pilot->TriggerAlarm(alarmCode, triggerId);
        }
    }
}

auto MCMoverGroup::HandleMateCrippled(uint32_t mateId) -> int32_t
{
    TriggerAlarm(4, mateId);
    return 0;
}

auto MCMoverGroup::HandleMateDisabled(uint32_t) -> int32_t
{
    return 0;
}

auto MCMoverGroup::HandleMateDestroyed(uint32_t mateId) -> int32_t
{
    TriggerAlarm(3, mateId);
    return 0;
}

auto MCMoverGroup::HandleMateEjected(uint32_t) -> int32_t
{
    return 0;
}

auto MCMoverGroup::HandleMateFiredWeapon(uint32_t mateId) -> void
{
    TriggerAlarm(0xd, mateId);
}
