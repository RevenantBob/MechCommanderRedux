#include "stdafx.h"
#include "object/group.h"
#include "ai/move.h"
#include "iface/iface.h"
#include "lib/aerror.h"
#include "lib/heap.h"
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
    /// is read but never used. MCX.EXE @ 0x0078f9ec.
    /// </summary>
    constexpr int32_t FormationStart[5] = {32, 0, 1, 3, 6};

    /// <summary>The formation slots behind the goal: angle and distance. MCX.EXE @ 0x0078fa00.</summary>
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
    void BlockMechCells(ObjectQueueNode* list, GameObject* dfaTarget, int32_t (&cells)[81], int32_t areaCellR,
                        int32_t areaCellC)
    {
        for (BaseObject* object = list->head; object != nullptr; object = object->next)
        {
            auto* mech = static_cast<GameObject*>(object);

            if (mech->objectClass == ELEMENTAL || mech == dfaTarget || mech->isDisabled() != 0)
            {
                continue;
            }

            const _ObjectPosition* position = mech->getObjPosition();
            const int32_t row = position->mapCellR - areaCellR;
            const int32_t col = position->mapCellC - areaCellC;

            if (row >= 0 && row < 9 && col >= 0 && col < 9)
            {
                cells[row * 9 + col] = JUMP_CELL_BLOCKED;
            }
        }
    }
}

auto MoverGroup::operator new(size_t size) noexcept -> void*
{
    return systemHeap->malloc(static_cast<uint32_t>(size));
}

auto MoverGroup::operator delete(void* ptr) -> void
{
    systemHeap->free(ptr);
}

auto MoverGroup::init() -> void
{
    id = -1;
    numMovers = 0;
    point = nullptr;
    disbandOnNoPoint = 0;
}

auto MoverGroup::destroy() -> void
{
}

auto MoverGroup::add(Mover* mover) -> int
{
    if (numMovers == MAX_MOVERGROUP_COUNT)
    {
        Fatal(0, " MoverGroup.add: Group too big ");
        return 0;
    }

    movers[numMovers] = mover;
    numMovers++;
    mover->setGroup(this);
    return 1;
}

auto MoverGroup::remove(Mover* mover) -> int
{
    if (mover == point)
    {
        disband();
        return 1;
    }

    for (int32_t i = 0; i < numMovers; i++)
    {
        if (movers[i] == mover)
        {
            mover->setGroup(nullptr);
            movers[i] = movers[numMovers - 1];
            movers[numMovers - 1] = nullptr;
            numMovers--;
            return 1;
        }
    }

    return 0;
}

auto MoverGroup::isMember(Mover* mover) -> int
{
    for (int32_t i = 0; i < numMovers; i++)
    {
        if (movers[i] == mover)
        {
            return 1;
        }
    }

    return 0;
}

auto MoverGroup::disband() -> void
{
    for (int32_t i = 0; i < numMovers; i++)
    {
        movers[i]->setGroup(nullptr);
    }

    if (point != nullptr)
    {
        theInterface->setPoint(point->partId, 0);
    }

    point = nullptr;
    numMovers = 0;
}

auto MoverGroup::setPoint(Mover* mover) -> int32_t
{
    if (isMember(mover) != 0)
    {
        if (point != nullptr)
        {
            theInterface->setPoint(point->partId, 0);
        }

        point = mover;
        theInterface->setPoint(mover->partId, 1);
    }

    return 0;
}

auto MoverGroup::selectPoint(int excludePoint) -> Mover*
{
    for (int32_t i = 0; i < numMovers; i++)
    {
        if (excludePoint != 0 && movers[i] == point)
        {
            continue;
        }

        const MechWarrior* pilot = movers[i]->getPilot();

        if (pilot != nullptr && pilot->wounds < 6.0f)
        {
            setPoint(movers[i]);
            return movers[i];
        }
    }

    setPoint(nullptr);
    return nullptr;
}

auto MoverGroup::getMovers(Mover** moverList) -> int32_t
{
    for (int32_t i = 0; i < numMovers; i++)
    {
        moverList[i] = movers[i];
    }

    return numMovers;
}

auto MoverGroup::getPointPilot() -> MechWarrior*
{
    if (point != nullptr)
    {
        return point->getPilot();
    }

    return nullptr;
}

auto MoverGroup::statusCount(int32_t* counts) -> void
{
    for (int32_t i = 0; i < numMovers; i++)
    {
        Mover* mover = movers[i];
        const MechWarrior* pilot = mover->getPilot();

        if (mover->getExists() == 0)
        {
            counts[8]++;
        }
        else if (mover->getAwake() == 0)
        {
            counts[7]++;
        }
        else if (pilot == nullptr || pilot->status != 2)
        {
            counts[static_cast<uint8_t>(mover->status)]++;
        }
        else
        {
            counts[6]++;
        }
    }
}

auto MoverGroup::addToGUI(int visible) -> void
{
    for (int32_t i = 0; i < numMovers; i++)
    {
        theInterface->AddMech(movers[i]->partId, id, movers[i]->getAwake(), visible);
    }
}

auto CalcJumpGoals(vector_3d goal, int32_t numGoals, vector_3d* goalList, GameObject* dfaTarget) -> int32_t
{
    int32_t numPlaced = 0;
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    GameMap->worldToMapPos(goal, tileR, tileC, cellR, cellC);
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

            if (row <= -1 || row >= GameMap->height || col <= -1 || col >= GameMap->width)
            {
                for (int32_t r = 0; r < 3; r++)
                {
                    block[r * 9] = JUMP_CELL_BLOCKED;
                    block[r * 9 + 1] = JUMP_CELL_BLOCKED;
                    block[r * 9 + 2] = JUMP_CELL_BLOCKED;
                }

                continue;
            }

            Assert(row < GameMap->height && col < GameMap->width, 0, " Map Tile out of bounds ");
            MapTile tile = GameMap->map[GameMap->width * row + col];

            for (int32_t r = 0; r < 3; r++)
            {
                for (int32_t c = 0; c < 3; c++)
                {
                    block[r * 9 + c] = tile.getCellPassable(r, c) == 0 ? JUMP_CELL_BLOCKED : JUMP_CELL_OPEN;
                }
            }

            // Bridges: their rails (the sides the road doesn't cross), or the whole tile.
            const uint32_t overlay = tile.overlay & 0x7f;

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

    BlockMechCells(innerSphereMechList, dfaTarget, cells, areaCellR, areaCellC);
    BlockMechCells(clanMechList, dfaTarget, cells, areaCellR, areaCellC);

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
            goalList[i].x = NO_JUMP_GOAL;
            goalList[i].y = NO_JUMP_GOAL;
            goalList[i].z = NO_JUMP_GOAL;
            continue;
        }

        cells[row * 9 + col] = i;
        mapCellToWorldPos(row + areaCellR, col + areaCellC, goalList[i]);
        numPlaced++;
    }

    return numPlaced;
}

auto MoverGroup::calcJumpGoals(vector_3d goal, vector_3d* goalList, GameObject* dfaTarget) -> int32_t
{
    return CalcJumpGoals(goal, numMovers, goalList, dfaTarget);
}

auto MoverGroup::handleTacticalOrder(TacticalOrder tacOrder, int32_t priority, vector_3d* destinations,
                                     int queueGroupOrder) -> int32_t
{
    if (numMovers == 0)
    {
        tacOrder.destroy();
        return 0;
    }

    if (queueGroupOrder != 0)
    {
        tacOrder.pack(nullptr, nullptr);
    }

    int jumping = 0;
    int formation = 0;
    const vector_3d goal = tacOrder.getWayPoint(0);
    getPoint();

    // An attack by jumping (method 1) becomes a jump onto the target.
    if (tacOrder.code == TACTICAL_ORDER_ATTACK_OBJECT)
    {
        if (tacOrder.attackParams.method == 1)
        {
            tacOrder.code = TACTICAL_ORDER_JUMPTO_OBJECT;
            tacOrder.moveParams.wait = 0;
            tacOrder.moveParams.wayPath.mode[0] = 0;

            if (tacOrder.target != nullptr)
            {
                tacOrder.setWayPoint(0, tacOrder.target->getPosition());
            }
        }
    }

    if (tacOrder.code == TACTICAL_ORDER_JUMPTO_OBJECT)
    {
        GameObject* target = tacOrder.target;
        tacOrder.code = TACTICAL_ORDER_JUMPTO_POINT;
        Assert(target != nullptr, 0, " JumpToObject is NULL ");
        tacOrder.setWayPoint(0, target->getPosition());
    }

    vector_3d jumpGoals[MAX_MOVERGROUP_COUNT];

    switch (tacOrder.code)
    {
        case TACTICAL_ORDER_MOVETO_POINT:
        case TACTICAL_ORDER_MOVETO_OBJECT:
        {
            // Moving: the members set off in order of distance from the goal, the point first.
            formation = 1;
            SortList* sortList = Mover::sortList;

            if (sortList == nullptr)
            {
                break;
            }

            sortList->clear(0);
            int32_t numSorted = 0;

            for (int32_t i = 0; i < numMovers; i++)
            {
                Mover* mover = movers[i];

                if (mover == nullptr || mover->isDisabled() != 0)
                {
                    continue;
                }

                if (numSorted >= 0 && numSorted < sortList->numItems)
                {
                    sortList->list[numSorted].id = i;
                }

                vector_3d goalPosition = goal;
                const auto distance = static_cast<float>(mover->distanceFrom(goalPosition));

                if (numSorted >= 0 && numSorted < sortList->numItems)
                {
                    sortList->list[numSorted].value = distance;
                }

                numSorted++;
            }

            sortList->sort(0);

            // The followers' formation slots. The original works them out but orders every member to the goal itself.
            int32_t numFollowers = numSorted - 1;

            if (numFollowers > 4)
            {
                numFollowers = 4;
            }

            if (numFollowers > 0)
            {
                vector_3d formationGoals[4];
                const int32_t first = FormationStart[numFollowers];

                for (int32_t i = 0; i < numFollowers; i++)
                {
                    formationGoals[i] = relativePositionToPoint(goal, FormationOffsets[first + i][0],
                                                                FormationOffsets[first + i][1], 2);
                }
            }

            int32_t rank = 1;

            for (int32_t i = 0; i < numSorted; i++)
            {
                Mover* mover = movers[sortList->list[i].id];

                if (mover == point)
                {
                    mover->selectionIndex = 0;
                }
                else
                {
                    mover->selectionIndex = rank++;
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
                calcJumpGoals(tacOrder.getWayPoint(0), jumpGoals, tacOrder.target);
            }
            else
            {
                for (int32_t i = 0; i < numMovers; i++)
                {
                    jumpGoals[i] = destinations[i];
                }
            }

            // A member with no goal stays put.
            for (int32_t i = 0; i < numMovers; i++)
            {
                movers[i]->selectionIndex = jumpGoals[i].x <= -99000.0f ? -2 : 0;
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
                          static_cast<int>(tacOrder.code));
            Assert(0, 1, message);
            tacOrder.destroy();
            return 1;
        }
    }

    tacOrder.unitOrder = 1;

    for (int32_t i = 0; i < numMovers; i++)
    {
        Mover* mover = movers[i];

        if (mover == nullptr || mover->isDisabled() != 0)
        {
            continue;
        }

        const int32_t delay = mover->selectionIndex;

        if (delay != -2)
        {
            tacOrder.selectionIndex = delay;

            if (delay != -1)
            {
                if (formation != 0)
                {
                    tacOrder.setWayPoint(0, goal);
                }
                else if (jumping != 0)
                {
                    tacOrder.setWayPoint(0, jumpGoals[i]);
                }

                tacOrder.delayedTime = static_cast<float>(mover->selectionIndex) * DelayedOrderTime + scenarioTime;
            }

            if (MPlayer != nullptr)
            {
                tacOrder.id = 0;
                tacOrder.setId(mover->getPilot());
            }

            switch (tacOrder.origin)
            {
                case 0:
                {
                    if (queueGroupOrder != 0)
                    {
                        mover->getPilot()->addQueuedTacOrder(tacOrder);
                        mover->getPilot()->tacOrderQueueExecuting = 1;
                    }
                    else
                    {
                        mover->getPilot()->setPlayerTacOrder(tacOrder, 0);
                    }
                    break;
                }
                case 1:
                    mover->getPilot()->setGeneralTacOrder(tacOrder);
                    break;
                case 2:
                    mover->getPilot()->setAlarmTacOrder(tacOrder, priority);
                    break;
                default:
                    break;
            }
        }

        mover->selectionIndex = -1;
    }

    tacOrder.destroy();
    return 0;
}

auto MoverGroup::orderMoveToPoint(int setTacOrder, int32_t origin, vector_3d location, uint32_t params) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < numMovers; i++)
    {
        Mover* mover = movers[i];
        Assert(mover != nullptr, 0, " MoverGroup.orderMoveToPoint: NULL mover ");
        MechWarrior* pilot = mover->getPilot();

        if (pilot != nullptr)
        {
            result = pilot->orderMoveToPoint(1, setTacOrder, origin, location, -1, params);
        }
    }

    return result;
}

auto MoverGroup::orderMoveToObject(int setTacOrder, int32_t origin, GameObject* target, uint32_t params) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < numMovers; i++)
    {
        Mover* mover = movers[i];
        Assert(mover != nullptr, 0, " MoverGroup.orderMoveToObject: NULL mover ");
        MechWarrior* pilot = mover->getPilot();

        if (pilot != nullptr)
        {
            result = pilot->orderMoveToObject(1, setTacOrder, origin, target, -1, params);
        }
    }

    return result;
}

auto MoverGroup::orderTraversePath(int32_t origin, _WayPath* wayPath, uint32_t params) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < numMovers; i++)
    {
        Mover* mover = movers[i];
        Assert(mover != nullptr, 0, " MoverGroup.orderTraversePath: NULL mover ");
        MechWarrior* pilot = mover->getPilot();

        if (pilot != nullptr)
        {
            result = pilot->orderTraversePath(1, 1, origin, wayPath, params);
        }
    }

    return result;
}

auto MoverGroup::orderPatrolPath(int32_t origin, _WayPath* wayPath) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < numMovers; i++)
    {
        Mover* mover = movers[i];
        Assert(mover != nullptr, 0, " MoverGroup.orderPatrolPath: NULL mover ");
        MechWarrior* pilot = mover->getPilot();

        if (pilot != nullptr)
        {
            result = pilot->orderPatrolPath(1, 1, origin, wayPath);
        }
    }

    return result;
}

auto MoverGroup::orderPowerDown(int32_t origin) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < numMovers; i++)
    {
        Mover* mover = movers[i];
        Assert(mover != nullptr, 0, " MoverGroup.orderPowerDown: NULL mover ");
        MechWarrior* pilot = mover->getPilot();

        if (pilot != nullptr)
        {
            result = pilot->orderPowerDown(1, origin);
        }
    }

    return result;
}

auto MoverGroup::orderPowerUp(int32_t origin) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < numMovers; i++)
    {
        Mover* mover = movers[i];
        Assert(mover != nullptr, 0, " MoverGroup.orderPowerUp: NULL mover ");
        MechWarrior* pilot = mover->getPilot();

        if (pilot != nullptr)
        {
            result = pilot->orderPowerUp(1, origin);
        }
    }

    return result;
}

auto MoverGroup::orderAttackObject(int32_t origin, GameObject* target, int32_t attackType, int32_t attackMethod,
                                   int32_t attackRange, int32_t aimLocation, uint32_t params) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < numMovers; i++)
    {
        Mover* mover = movers[i];
        Assert(mover != nullptr, 0, " MoverGroup.orderAttackObject: NULL mover ");
        MechWarrior* pilot = mover->getPilot();

        if (pilot != nullptr)
        {
            result =
                pilot->orderAttackObject(1, origin, target, attackType, attackMethod, attackRange, aimLocation, params);
        }
    }

    return result;
}

auto MoverGroup::orderWithdraw(int32_t origin, vector_3d location) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < numMovers; i++)
    {
        Mover* mover = movers[i];
        Assert(mover != nullptr, 0, " MoverGroup.orderWithdraw: NULL mover ");
        MechWarrior* pilot = mover->getPilot();

        if (pilot != nullptr)
        {
            result = pilot->orderWithdraw(1, origin, location);
        }
    }

    return result;
}

auto MoverGroup::orderEject(int32_t origin) -> int32_t
{
    int32_t result = 0;

    for (int32_t i = 0; i < numMovers; i++)
    {
        Mover* mover = movers[i];
        // The original reuses orderWithdraw's message.
        Assert(mover != nullptr, 0, " MoverGroup.orderWithdraw: NULL mover ");
        MechWarrior* pilot = mover->getPilot();

        if (pilot != nullptr)
        {
            result = pilot->orderEject(1, 1, origin);
        }
    }

    return result;
}

auto MoverGroup::triggerAlarm(int32_t alarmCode, uint32_t triggerId) -> void
{
    for (int32_t i = 0; i < numMovers; i++)
    {
        MechWarrior* pilot = movers[i]->getPilot();

        if (pilot != nullptr)
        {
            pilot->triggerAlarm(alarmCode, triggerId);
        }
    }
}

auto MoverGroup::handleMateCrippled(uint32_t mateId) -> int32_t
{
    triggerAlarm(4, mateId);
    return 0;
}

auto MoverGroup::handleMateDisabled(uint32_t) -> int32_t
{
    return 0;
}

auto MoverGroup::handleMateDestroyed(uint32_t mateId) -> int32_t
{
    triggerAlarm(3, mateId);
    return 0;
}

auto MoverGroup::handleMateEjected(uint32_t) -> int32_t
{
    return 0;
}

auto MoverGroup::handleMateFiredWeapon(uint32_t mateId) -> void
{
    triggerAlarm(0xd, mateId);
}
