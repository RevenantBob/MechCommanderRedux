#include "stdafx.h"
#include "object/MCMover.h"
#include "lib/MCFatal.h"
#include "main/MCMissionGlobals.h"
#include "network/MCMultiPlayer.h"
#include "object/MCMechWarrior.h"
#include "object/MCMoverGroup.h"
#include "object/MCElemental.h"
#include "object/MCElementalType.h"
#include "object/MCElementalGameSystem.h"
#include "object/MCBattleMech.h"
#include "object/MCBattleMechType.h"
#include "object/MCMechGameSystem.h"
#include "terrain/MCTerrain.h"

// The mover's movement: its move goals, the paths it plans, and the path locks.

namespace
{
    /// <summary>The diagonal steps (row, column) that walk calcMoveGoal's diamond around the goal.</summary>
    constexpr int32_t GoalRingStep[4][2] = {{1, 1}, {1, -1}, {-1, -1}, {-1, 1}};

    /// <summary>Cells along a side of calcMoveGoal's map of the cells around a move goal: 13 tiles around the
    /// goal's.</summary>
    constexpr int32_t GoalMapCellDim = 39;

    /// <summary>
    /// The jumping the path finders allow, as calcMovePath and calcEscapePath each repeat it: 8 offsets at no cost,
    /// or the mover's jump range when an AI mover in single player. An elemental away from its last target
    /// (or without one) sets JumpOnBlocked.
    /// </summary>
    void SetUpPathJumps(MCMover* mover, int32_t& numOffsets, int32_t& jumpCost)
    {
        jumpCost = 0;
        numOffsets = 8;

        if (mover->Pilot->OnHomeTeam() == 0 && MultiPlayer() == nullptr)
        {
            mover->GetJumpRange(&numOffsets, &jumpCost);
        }

        if (mover->ObjectClass != MCObjectClass::Elemental)
        {
            return;
        }

        MCGameObject* lastTarget = mover->Pilot->GetLastTarget();

        if (lastTarget != nullptr)
        {
            MCVector3D targetPosition = lastTarget->GetPosition();

            if (mover->DistanceFrom(targetPosition) < ElementalTargetNoJumpDistance)
            {
                jumpCost = 0;
                numOffsets = 8;
                return;
            }
        }

        PathFindMap()->JumpOnBlocked = true;
    }

    /// <summary>The move level the local path finders use: the seconds a cell takes at <paramref name="speed"/>
    /// times 50, floored, as a 16-bit value.</summary>
    int32_t LocalPathMoveLevel(float speed)
    {
        const double cellMeters = static_cast<double>(MetersPerWorldUnit) * MCTerrain::MetersPerVertexDivMapcellDim;
        return static_cast<int16_t>(static_cast<int32_t>(std::floor(cellMeters / speed * 50.0)));
    }

    /// <summary>
    /// Calls <paramref name="visit"/>(tile, cellR, cellC) for the 3x3 cells around a cell, crossing into the
    /// neighbouring tiles, until it returns true. The original unrolls this per cell; it addresses a tile as
    /// width * tileR + tileC, so a column off either side wraps to the next or previous row, as here.
    /// </summary>
    /// <returns>Whether <paramref name="visit"/> stopped the walk.</returns>
    template <typename Visit>
    bool VisitCellsAround(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, Visit visit)
    {
        for (int32_t rowStep = -1; rowStep <= 1; rowStep++)
        {
            for (int32_t colStep = -1; colStep <= 1; colStep++)
            {
                int32_t row = cellR + rowStep;
                int32_t rowTile = tileR;

                if (row < 0)
                {
                    row += MapCellDim;
                    rowTile--;
                }
                else if (row >= MapCellDim)
                {
                    row -= MapCellDim;
                    rowTile++;
                }

                int32_t col = cellC + colStep;
                int32_t colTile = tileC;

                if (col < 0)
                {
                    col += MapCellDim;
                    colTile--;
                }
                else if (col >= MapCellDim)
                {
                    col -= MapCellDim;
                    colTile++;
                }

                const int32_t index = GameMap()->Width * rowTile + colTile;

                // Port fix: the original reads and writes before or past the map for a cell on its first or last row.
                if (index < 0 || index >= GameMap()->Width * GameMap()->Height)
                {
                    continue;
                }

                if (visit(GameMap()->Map[index], row, col))
                {
                    return true;
                }
            }
        }

        return false;
    }
}

auto MCMover::CalcOffsetMoveGoal(MCVector3D target, MCVector3D offset, MCVector3D& goal) -> int32_t
{
    // Half a map cell per step, from the offset point toward the target.
    float directionX = target.X - offset.X;
    float directionY = target.Y - offset.Y;
    const float length = static_cast<float>(
        std::sqrt(static_cast<double>(directionX) * directionX + static_cast<double>(directionY) * directionY));

    if (length != 0.0f)
    {
        directionX = directionX / length;
        directionY = directionY / length;
    }

    const float stepX =
        static_cast<float>(static_cast<double>(directionX) * MCTerrain::MetersPerVertexDivMapcellDim * 0.5);
    const float stepY =
        static_cast<float>(static_cast<double>(directionY) * MCTerrain::MetersPerVertexDivMapcellDim * 0.5);

    if (std::sqrt(static_cast<double>(stepX) * stepX + static_cast<double>(stepY) * stepY) == 0.0)
    {
        goal = target;
        return 0;
    }

    MCVector3D away = offset - target;
    const float maxDistance = static_cast<float>(away.Magnitude());
    float x = offset.X;
    float y = offset.Y;

    // Whether the cell under (x, y) is passable.
    auto cellPassable = [&]()
    {
        MCVector3D point;
        point.X = x;
        point.Y = y;
        point.Z = 0.0f;
        int32_t tileR;
        int32_t tileC;
        int32_t cellR;
        int32_t cellC;
        GameMap()->WorldToMapPos(point, tileR, tileC, cellR, cellC);

        // Port fix: the walk can leave the map, where the original reads outside it. Off the map is impassable.
        if (!GameMap()->OnMap(tileR, tileC))
        {
            return 0u;
        }

        return GameMap()->Map[GameMap()->Width * tileR + tileC].GetCellPassable(cellR, cellC);
    };

    // Off a blocked cell: step on until the point before was open (so one step past the first open cell).
    if (cellPassable() == 0)
    {
        float traveled = 0.0f;
        uint32_t lastPassable;

        do
        {
            if (maxDistance <= traveled)
            {
                break;
            }

            lastPassable = cellPassable();
            x = stepX + x;
            y = stepY + y;
            const double dx = static_cast<double>(x) - target.X;
            const double dy = static_cast<double>(y) - target.Y;
            traveled = static_cast<float>(std::sqrt(dx * dx + dy * dy));
        } while (lastPassable == 0);
    }

    MCVector3D ground;
    ground.X = x;
    ground.Y = y;
    ground.Z = 0.0f;
    goal.X = x;
    goal.Y = y;
    goal.Z = GameMap()->GetTerrainElevation(ground);
    return 0;
}

auto MCMover::CalcMoveGoal(MCGameObject* target, MCVector3D moveGoal, int32_t isGroup, int32_t offsetIndex,
                           int32_t groupSize, int32_t pointIndex, MCVector3D& newGoal, uint32_t params) -> int32_t
{
    // 0x800: no fire range ring and no bonus around the goal itself.
    const uint32_t noRangeRing = (params >> 11) & 1;

    // 0x20: go straight to the goal.
    if ((params & 0x20) != 0)
    {
        newGoal = moveGoal;
        return 0;
    }

    // The scores of the cells around the goal, row by row.
    std::array<int32_t, GoalMapCellDim * GoalMapCellDim> goal{};
    auto goalCell = [&goal](int32_t row, int32_t col) -> int32_t& { return goal[row * GoalMapCellDim + col]; };

    // 0x400: one and a half vertices toward the goal.
    if ((params & 0x400) != 0)
    {
        const float dx = moveGoal.X - Position.X;
        const float dy = moveGoal.Y - Position.Y;
        const double length = std::sqrt(static_cast<double>(dy) * dy + static_cast<double>(dx) * dx);

        if (length <= 0.0)
        {
            return 0;
        }

        const float lengthF = static_cast<float>(length);
        const double stepLength = static_cast<double>(MCTerrain::MetersPerVertex) * 1.5;
        MCVector3D step;
        step.X = static_cast<float>(static_cast<double>(dx / lengthF) * stepLength);
        step.Y = static_cast<float>(stepLength * (dy / lengthF));
        step.Z = 0.0f;
        // The original takes the elevation of the step itself, not of the point it reaches.
        const float elevation = GameMap()->GetTerrainElevation(step);
        MCVector3D stepGoal;
        stepGoal.X = step.X + Position.X;
        stepGoal.Y = step.Y + Position.Y;
        stepGoal.Z = elevation + Position.Z;
        CalcOffsetMoveGoal(Position, stepGoal, newGoal);
        return 0;
    }

    // The map covers the 13x13 tiles around the goal's tile.
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    GameMap()->WorldToMapPos(moveGoal, tileR, tileC, cellR, cellC);
    const int32_t goalCellR = tileR * MapCellDim + cellR;
    const int32_t goalCellC = tileC * MapCellDim + cellC;
    const int32_t mapTileR0 = tileR - 6;
    const int32_t mapTileC0 = tileC - 6;
    const int32_t mapCellR0 = mapTileR0 * MapCellDim;
    const int32_t mapCellC0 = mapTileC0 * MapCellDim;

    if (target == nullptr)
    {
        CalcOffsetMoveGoal(Position, moveGoal, newGoal);
        return 0;
    }

    const MCObjectClass targetClass = target->ObjectClass;

    if (targetClass == MCObjectClass::BattleMech || targetClass == MCObjectClass::GroundVehicle ||
        targetClass == MCObjectClass::Elemental || targetClass == MCObjectClass::Mover)
    {
        // The facing is computed and dropped.
        MCVector3D targetPosition = target->GetPosition();
        targetPosition.Y = static_cast<float>(targetPosition.Y + 50.0);
        target->RelFacingTo(targetPosition, -1);
    }

    const int32_t* overlayWeights = &OverlayWeightTable[OverlayWeightClass * OverlayWeightLevelSize];

    // Adds amount to the square of cells within radius of the goal.
    auto addSquare = [&](int32_t radius, int32_t amount)
    {
        const int32_t rowEnd = (radius - mapCellR0) + 1 + goalCellR;
        const int32_t colStart = (goalCellC - radius) - mapCellC0;
        const int32_t colEnd = (radius - mapCellC0) + 1 + goalCellC;

        for (int32_t row = (goalCellR - radius) - mapCellR0; row < rowEnd; row++)
        {
            const int32_t rowIndex = row * GoalMapCellDim;

            for (int32_t col = colStart; col < colEnd; col++)
            {
                if (rowIndex > -1 && rowIndex < GoalMapCellDim * GoalMapCellDim && col > -1 && col < GoalMapCellDim)
                {
                    goal[rowIndex + col] += amount;
                }
            }
        }
    };

    const MCTacticalOrderCode orderCode = Pilot->CurTacOrder.Code;
    const bool attacking = orderCode == MCTacticalOrderCode::AttackObject || orderCode == MCTacticalOrderCode::Guard;
    int32_t attackRange = -5;

    if (attacking)
    {
        attackRange = Pilot->CurTacOrder.AttackParams.Range;
    }

    int32_t ringRange = 2;

    if (attacking)
    {
        const float cellMeters = MetersPerWorldUnit * MCTerrain::MetersPerVertexDivMapcellDim;

        // Within the longest fire range (less 3 cells) is good.
        if (noRangeRing == 0 && attackRange != 0 && attackRange != 1 && attackRange != 2)
        {
            int32_t radius = static_cast<int32_t>(static_cast<double>(GetFireRange(-2)) / cellMeters) - 3;

            if (radius < 1)
            {
                radius = 1;
            }
            else if (radius > 19)
            {
                radius = 19;
            }

            addSquare(radius, 25);
        }

        ringRange = 0;
        const float orderFireRange = Pilot->OrderFireRange;

        if (orderFireRange > 0.0f)
        {
            // An ordered fire range: the ring at that range, and nothing within the weapons' minimum range.
            ringRange = static_cast<int32_t>(static_cast<double>(orderFireRange) / cellMeters);

            if (noRangeRing == 0)
            {
                ringRange -= 3;
            }

            if (ringRange < 1)
            {
                ringRange = 1;
            }
            else if (ringRange > 19)
            {
                ringRange = 19;
            }

            int32_t radius = static_cast<int32_t>(static_cast<double>(MaxMinRange) / cellMeters + 1.0f);

            if (radius > 19)
            {
                radius = 19;
            }

            addSquare(radius, -525);
        }
        else if (orderFireRange == -1.0f)
        {
            ringRange = 2;
        }
    }

    // A diamond of cells ringRange from the goal.
    int32_t ringRow = (goalCellR - ringRange) - mapCellR0;
    int32_t ringCol = goalCellC - mapCellC0;

    for (int32_t side = 0; side < 4; side++)
    {
        for (int32_t count = 0; count < ringRange; count++)
        {
            ringRow += GoalRingStep[side][0];
            ringCol += GoalRingStep[side][1];

            if (ringRow > -1 && ringRow < GoalMapCellDim && ringCol > -1 && ringCol < GoalMapCellDim)
            {
                goalCell(ringRow, ringCol) += 500;
            }
        }
    }

    // Where the mover stands, clamped to the map.
    int32_t myTileR;
    int32_t myTileC;
    int32_t myCellR;
    int32_t myCellC;
    GameMap()->WorldToMapPos(Position, myTileR, myTileC, myCellR, myCellC);
    int32_t myRow = myCellR + (myTileR - mapTileR0) * MapCellDim;
    int32_t myCol = myCellC + (myTileC - mapTileC0) * MapCellDim;

    if (myRow < 0)
    {
        myRow = 0;
    }
    else if (myRow >= GoalMapCellDim)
    {
        myRow = GoalMapCellDim - 1;
    }

    if (myCol < 0)
    {
        myCol = 0;
    }
    else if (myCol >= GoalMapCellDim)
    {
        myCol = GoalMapCellDim - 1;
    }

    // The 3x3 cells of the goal.
    if (noRangeRing == 0)
    {
        for (int32_t row = -1; row < 2; row++)
        {
            for (int32_t col = -1; col < 2; col++)
            {
                goal[(goalCellR - mapCellR0 + row) * GoalMapCellDim + (goalCellC - mapCellC0) + col] += 100;
            }
        }
    }

    // 0x8: somewhere other than where the mover stands.
    if ((params & 0x8) != 0)
    {
        goalCell(myRow, myCol) -= 10000;
    }

    // Farther from the mover is worse.
    for (int32_t row = 0; row < GoalMapCellDim; row++)
    {
        for (int32_t col = 0; col < GoalMapCellDim; col++)
        {
            const int32_t rowDistance = row > myRow ? row - myRow : myRow - row;
            const int32_t colDistance = col > myCol ? col - myCol : myCol - col;
            goalCell(row, col) -= rowDistance + colDistance;
        }
    }

    // The cells the group mates are heading for.
    MCMover* movers[MCMoverGroup::MaxMovers];

    if (Group != nullptr)
    {
        const int32_t numMovers = Group->GetMovers(movers);

        for (int32_t i = 0; i < numMovers; i++)
        {
            if (movers[i] == this)
            {
                continue;
            }

            MCMechWarrior* matePilot = movers[i]->GetPilot();

            if (matePilot == nullptr || matePilot->MoveOrders.PathType == 0)
            {
                continue;
            }

            int32_t mateCellR;
            int32_t mateCellC;
            WorldCoordToMapCell(matePilot->MoveOrders.OriginalGlobalGoal[1], mateCellR, mateCellC);
            mateCellR -= mapCellR0;
            mateCellC -= mapCellC0;

            if (mateCellR > -1 && mateCellR < GoalMapCellDim && mateCellC > -1 && mateCellC < GoalMapCellDim)
            {
                goalCell(mateCellR, mateCellC) -= 100;
            }
        }
    }

    // Off the map and impassable cells are out; overlays cost their weight.
    for (int32_t tileRow = 0; tileRow * MapCellDim < GoalMapCellDim; tileRow++)
    {
        const int32_t mapR = tileRow + mapTileR0;
        int32_t mapC = mapTileC0;

        for (int32_t cellCol = 0; cellCol < GoalMapCellDim; cellCol += MapCellDim, mapC++)
        {
            int32_t* block = &goal[(tileRow * MapCellDim) * GoalMapCellDim + cellCol];

            if (mapR <= -1 || mapR >= GameMap()->Height || mapC <= -1 || mapC >= GameMap()->Width)
            {
                for (int32_t row = 0; row < MapCellDim; row++)
                {
                    for (int32_t col = 0; col < MapCellDim; col++)
                    {
                        block[row * GoalMapCellDim + col] -= 10000;
                    }
                }

                continue;
            }

            Assert(mapR < GameMap()->Height && mapC < GameMap()->Width, 0, " Map Tile out of bounds ");
            MCMapTile tile = GameMap()->Map[GameMap()->Width * mapR + mapC];

            for (int32_t row = 0; row < MapCellDim; row++)
            {
                for (int32_t col = 0; col < MapCellDim; col++)
                {
                    if (tile.GetCellPassable(row, col) == 0)
                    {
                        block[row * GoalMapCellDim + col] -= 10000;
                    }
                }
            }

            const uint32_t overlayType = tile.Overlay & 0x7f;

            if (overlayType != 0)
            {
                const int32_t* weight = overlayWeights + OverlayWeightIndex(overlayType);

                for (int32_t row = 0; row < MapCellDim; row++)
                {
                    for (int32_t col = 0; col < MapCellDim; col++)
                    {
                        block[row * GoalMapCellDim + col] -= *weight++;
                    }
                }
            }
        }
    }

    // The 20 best cells, best first. A cell better than only the last goes in last.
    struct GoalCandidate
    {
        int32_t Row = 0;
        int32_t Col = 0;
        int32_t Value = 0;
    };

    GoalCandidate best[20];

    for (GoalCandidate& candidate : best)
    {
        candidate = {-1, -1, -999999};
    }

    for (int32_t index = 0; index < GoalMapCellDim * GoalMapCellDim; index++)
    {
        const int32_t value = goal[index];

        if (index >= 20 && value <= best[19].Value)
        {
            continue;
        }

        int32_t slot = 18;

        while (slot > -1 && value >= best[slot].Value)
        {
            slot--;
        }

        if (slot < 18)
        {
            std::memmove(&best[slot + 2], &best[slot + 1], (18 - slot) * sizeof(GoalCandidate));
        }

        best[slot + 1] = {index / GoalMapCellDim, index % GoalMapCellDim, value};
    }

    // An elemental asks its group mates ahead of it for their last targets, and drops the answers.
    if (ObjectClass == MCObjectClass::Elemental && Group != nullptr)
    {
        const int32_t numMovers = Group->GetMovers(movers);

        for (int32_t i = 0; i < numMovers; i++)
        {
            if (movers[i] == this)
            {
                break;
            }

            MCMechWarrior* matePilot = movers[i]->GetPilot();

            if (matePilot != nullptr)
            {
                matePilot->GetLastTarget();
            }
        }
    }

    // The best cell with a line of fire to the goal (else the 20th).
    const double halfMapSide = static_cast<double>(WorldUnitsMapSide) * 0.5f;
    int32_t goalRow = mapTileR0;
    int32_t goalCol = mapTileC0;
    target->ClearLineOfFire();

    for (int32_t i = 0; i < 20; i++)
    {
        goalCol = best[i].Col;
        goalRow = best[i].Row;
        MCVector3D cellCenter;
        cellCenter.X =
            static_cast<float>((static_cast<double>(goalCol + mapCellC0) + 0.5f) * MetersPerCell() - halfMapSide);
        cellCenter.Y = static_cast<float>((halfMapSide - static_cast<double>(goalRow + mapCellR0) * MetersPerCell()) -
                                          static_cast<double>(MetersPerCell()) * 0.5f);
        cellCenter.Z = 0.0f;

        if (GameMap()->LineOfFire(cellCenter, moveGoal) != 0)
        {
            break;
        }
    }

    goalRow += mapCellR0;
    goalCol += mapCellC0;
    target->RestoreLineOfFire();

    newGoal.X = static_cast<float>((static_cast<double>(goalCol) + 0.5) * MetersPerCell() - halfMapSide);
    newGoal.Y = static_cast<float>(halfMapSide - (static_cast<double>(goalRow) + 0.5) * MetersPerCell());
    newGoal.Z = Terrain()->GetTerrainElevation(newGoal);
    CalcOffsetMoveGoal(Position, newGoal, newGoal);
    return 0;
}

auto MCMover::CalcMovePath(MCMovePath* path, int32_t pathType, MCVector3D start, MCVector3D goal, int32_t* goalCell,
                           uint32_t params) -> int32_t
{
    if (PathFindMap() == nullptr)
    {
        Fatal(0, " No PathFindMap() in Mover::calcMovePath ");
    }

    int32_t startTileR;
    int32_t startTileC;
    int32_t startCellR;
    int32_t startCellC;
    GameMap()->WorldToMapPos(start, startTileR, startTileC, startCellR, startCellC);
    int32_t goalTileR;
    int32_t goalTileC;
    int32_t goalCellR;
    int32_t goalCellC;
    GameMap()->WorldToMapPos(goal, goalTileR, goalTileC, goalCellR, goalCellC);
    path->Clear();

    int32_t numOffsets;
    int32_t jumpCost;
    int32_t* overlayWeights = &OverlayWeightTable[OverlayWeightClass * OverlayWeightLevelSize];

    if (pathType == 1)
    {
        // A simple path: the window of SimpleMovePathRange tiles around the start.
        int32_t uLr = startTileR - SimpleMovePathRange;

        if (uLr < 0)
        {
            uLr = 0;
        }

        int32_t uLc = startTileC - SimpleMovePathRange;

        if (uLc < 0)
        {
            uLc = 0;
        }

        if (MaxRunSpeed == 0.0f)
        {
            return 0;
        }

        const int32_t moveLevel = LocalPathMoveLevel(MaxRunSpeed);

        if (moveLevel <= 0)
        {
            return 0;
        }

        SetUpPathJumps(this, numOffsets, jumpCost);
        const int32_t dim = SimpleMovePathRange * 2 + 1;
        PathFindMap()->SetUp(*GameMap(), uLr, uLc, dim, dim, &start, (startTileR - uLr) * MapCellDim + startCellR,
                             (startTileC - uLc) * MapCellDim + startCellC, goal,
                             (goalTileR - uLr) * MapCellDim + goalCellR, (goalTileC - uLc) * MapCellDim + goalCellC,
                             overlayWeights, moveLevel, jumpCost, numOffsets, params);
        PathFindMap()->DebugMovePathType = 1;
        // The caller's goalCell is left alone.
        int32_t simpleGoalCell[2];
        const int32_t result = PathFindMap()->CalcPath(path, nullptr, simpleGoalCell);
        PathFindMap()->JumpOnBlocked = false;
        return result;
    }

    // Within the start's sector of the global map.
    if (MaxRunSpeed == 0.0f)
    {
        return 0;
    }

    const int32_t moveLevel = static_cast<int32_t>(static_cast<double>(MetersPerWorldUnit) *
                                                   MCTerrain::MetersPerVertexDivMapcellDim / MaxRunSpeed * 50.0);

    if (moveLevel <= 0)
    {
        return 0;
    }

    const int32_t sectorDim = GlobalMoveMap()->SectorDim;
    const int32_t uLr = (startTileR / sectorDim) * sectorDim;
    const int32_t uLc = (startTileC / sectorDim) * sectorDim;
    SetUpPathJumps(this, numOffsets, jumpCost);
    PathFindMap()->SetUp(*GameMap(), uLr, uLc, GlobalMoveMap()->SectorDim, GlobalMoveMap()->SectorDim, &start,
                         (startTileR - uLr) * MapCellDim + startCellR, (startTileC - uLc) * MapCellDim + startCellC,
                         goal, (goalTileR - uLr) * MapCellDim + goalCellR, (goalTileC - uLc) * MapCellDim + goalCellC,
                         overlayWeights, moveLevel, jumpCost, numOffsets, params);
    PathFindMap()->DebugMovePathType = pathType;
    const int32_t result = PathFindMap()->CalcPath(path, nullptr, goalCell);
    PathFindMap()->JumpOnBlocked = false;
    return result;
}

auto MCMover::CalcEscapePath(MCMovePath* path, MCVector3D start, MCVector3D goal, int32_t* goalCell, uint32_t params,
                             MCVector3D& escapeGoal) -> int32_t
{
    escapeGoal.X = -999999.0f;
    escapeGoal.Y = -999999.0f;
    escapeGoal.Z = -999999.0f;

    if (PathFindMap() == nullptr)
    {
        Fatal(0, " No PathFindMap() in Mover::calcMovePath ");
    }

    int32_t startTileR;
    int32_t startTileC;
    int32_t startCellR;
    int32_t startCellC;
    GameMap()->WorldToMapPos(start, startTileR, startTileC, startCellR, startCellC);
    int32_t goalTileR;
    int32_t goalTileC;
    int32_t goalCellR;
    int32_t goalCellC;
    GameMap()->WorldToMapPos(goal, goalTileR, goalTileC, goalCellR, goalCellC);
    path->Clear();

    int32_t uLr = startTileR - SimpleMovePathRange;

    if (uLr < 0)
    {
        uLr = 0;
    }

    int32_t uLc = startTileC - SimpleMovePathRange;

    if (uLc < 0)
    {
        uLc = 0;
    }

    if (MaxRunSpeed == 0.0f)
    {
        return 0;
    }

    const int32_t moveLevel = LocalPathMoveLevel(MaxRunSpeed);

    if (moveLevel <= 0)
    {
        return 0;
    }

    int32_t numOffsets;
    int32_t jumpCost;
    SetUpPathJumps(this, numOffsets, jumpCost);
    const int32_t dim = SimpleMovePathRange * 2 + 1;
    PathFindMap()->FindingEscapePath = true;
    PathFindMap()->SetUp(*GameMap(), uLr, uLc, dim, dim, &start, (startTileR - uLr) * MapCellDim + startCellR,
                         (startTileC - uLc) * MapCellDim + startCellC, goal, (goalTileR - uLr) * MapCellDim + goalCellR,
                         (goalTileC - uLc) * MapCellDim + goalCellC,
                         &OverlayWeightTable[OverlayWeightClass * OverlayWeightLevelSize], moveLevel, jumpCost,
                         numOffsets, params);
    PathFindMap()->DebugMovePathType = 0;
    // goalCell is unused: the escape goal cell goes to a local.
    int32_t escapeGoalCell[2];
    const int32_t result = PathFindMap()->CalcEscapePath(path, &escapeGoal, escapeGoalCell);
    PathFindMap()->JumpOnBlocked = false;
    PathFindMap()->FindingEscapePath = false;
    return result;
}

auto MCMover::GetAdjacentCellPathLocked(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, int32_t dir) -> int
{
    const int32_t* adjCell = AdjCellTable[cellR * MapCellDim + cellC][dir].data();
    return GameMap()->Map[(adjCell[0] + tileR) * GameMap()->Width + adjCell[1] + tileC].GetCellPathLocked(
               adjCell[2], adjCell[3]) != 0;
}

auto MCMover::GetPathLocked(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, int32_t diameter) -> int
{
    if (diameter == 1)
    {
        return GameMap()->Map[GameMap()->Width * tileR + tileC].GetCellPathLocked(cellR, cellC) != 0;
    }

    if (diameter != 3)
    {
        if (diameter == 5)
        {
            return 0;
        }

        Fatal(0, " Bad PathLock Radius ");
    }

    return VisitCellsAround(tileR, tileC, cellR, cellC, [](MCMapTile& tile, int32_t row, int32_t col)
                            { return tile.GetCellPathLocked(row, col) != 0; });
}

auto MCMover::SetPathLock(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, int set, int32_t diameter) -> void
{
    const uint32_t locked = set != 0 ? 1 : 0;

    if (diameter == 1)
    {
        GameMap()->Map[GameMap()->Width * tileR + tileC].SetCellPathLocked(cellR, cellC, locked);
        return;
    }

    if (diameter != 3)
    {
        if (diameter != 5)
        {
            Fatal(0, " Bad PathLock Diameter ");
        }

        return;
    }

    VisitCellsAround(tileR, tileC, cellR, cellC,
                     [locked](MCMapTile& tile, int32_t row, int32_t col)
                     {
                         tile.SetCellPathLocked(row, col, locked);
                         return false;
                     });
}

auto MCMover::GetPathRangeLock(int32_t range, int* reachedEnd) -> int
{
    MCMovePath* path = Pilot->GetMovePath();

    if (path != nullptr)
    {
        return path->IsLocked(-1, range, reachedEnd);
    }

    return 0;
}

auto MCMover::SetPathRangeLock(int set, int32_t range) -> int32_t
{
    MCMovePath* path = Pilot->GetMovePath();

    if (set == 0)
    {
        for (const MCPathRangeLock& lock : PathRangeLocks)
        {
            GameMap()->Map[lock.TileR * GameMap()->Width + lock.TileC].SetCellPathLocked(lock.CellR, lock.CellC, 0);
        }

        PathRangeLocks.clear();
        return 0;
    }

    if (!PathRangeLocks.empty())
    {
        SetPathRangeLock(0, 0);
    }

    if (path == nullptr || path->NumSteps <= 0)
    {
        return 0;
    }

    int32_t lastStep = path->CurStep + range;

    if (path->NumStepsWhenNotPaused <= lastStep)
    {
        lastStep = path->NumStepsWhenNotPaused;
    }

    PathRangeLocks.clear();

    for (int32_t step = path->CurStep; step < lastStep; step++)
    {
        const MCPathStep& pathStep = path->StepList[step];
        MCMapTile& tile = GameMap()->Map[pathStep.TileR * GameMap()->Width + pathStep.TileC];

        // Someone else holds the cell: the cells locked so far stay locked.
        if (tile.GetCellPathLocked(pathStep.CellR, pathStep.CellC) != 0)
        {
            return -1;
        }

        tile.SetCellPathLocked(pathStep.CellR, pathStep.CellC, 1);
        PathRangeLocks.push_back({pathStep.TileR, pathStep.TileC, pathStep.CellR, pathStep.CellC});
    }

    return 0;
}

auto MCMover::UpdatePathLock(int set) -> void
{
    // Not while a mech is in the air.
    if (ObjectClass == MCObjectClass::BattleMech && static_cast<MCBattleMech*>(this)->InJump != 0)
    {
        return;
    }

    MCObjectPosition* objectPosition = ObjPosition;

    if (objectPosition != nullptr)
    {
        SetPathLock(objectPosition->TileR, objectPosition->TileC, objectPosition->CellR, objectPosition->CellC, set,
                    PathLockLevel);
    }

    Pilot->GetMovePath();

    if (set == 0 || Pilot->MoveOrders.YieldTime <= -1.0f)
    {
        SetPathRangeLock(set, PathLockRange);
    }
}

auto MCMover::GetPathRangeBlocked(int32_t range, int* reachedEnd) -> int
{
    MCMovePath* path = Pilot->GetMovePath();

    if (path != nullptr)
    {
        return path->IsBlocked(-1, range, reachedEnd);
    }

    return 0;
}

auto MCMover::UpdateHustleTime() -> void
{
    const MCObjectPosition* objectPosition = ObjPosition;

    switch (GameMap()->Map[objectPosition->TileR * GameMap()->Width + objectPosition->TileC].Overlay & 0x7f)
    {
        case 0x25:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x37:
        case 0x38:
        case 0x39:
        case 0x3a:
            LastHustleTime = ScenarioTime;
            break;
        default:
            break;
    }
}

auto MCMover::BounceToAdjCell() -> int32_t
{
    // The first neighbour that is passable, affordable and not path locked.
    int32_t dir = 0;
    int32_t adjTileR;
    int32_t adjTileC;
    int32_t adjCellR;
    int32_t adjCellC;

    while (true)
    {
        const MCObjectPosition* objectPosition = ObjPosition;
        const int32_t* adjCell = AdjCellTable[objectPosition->CellR * MapCellDim + objectPosition->CellC][dir].data();
        adjTileR = adjCell[0] + objectPosition->TileR;
        adjTileC = adjCell[1] + objectPosition->TileC;
        adjCellR = adjCell[2];
        adjCellC = adjCell[3];
        // The tile's words are read before the overlay weight is.
        MCMapTile tile = GameMap()->Map[GameMap()->Width * adjTileR + adjTileC];
        uint32_t passable = tile.GetCellPassable(adjCellR, adjCellC);

        if (GameMap()->GetOverlayWeight(adjTileR, adjTileC, adjCellR, adjCellC, this) > 9999)
        {
            passable = 0;
        }

        if (tile.GetCellPathLocked(adjCellR, adjCellC) == 0 && passable != 0)
        {
            break;
        }

        dir++;

        if (dir > 7)
        {
            return -1;
        }
    }

    const MCObjectPosition* objectPosition = ObjPosition;
    const uint32_t wasLocked =
        GameMap()->Map[objectPosition->TileR * GameMap()->Width + objectPosition->TileC].GetCellPathLocked(
            objectPosition->CellR, objectPosition->CellC);

    if (wasLocked != 0)
    {
        UpdatePathLock(0);
    }

    const double halfMapSide = static_cast<double>(WorldUnitsMapSide) * 0.5f;
    MCVector3D cellCenter;
    cellCenter.X = static_cast<float>((static_cast<double>(adjCellC + adjTileC * MapCellDim) + 0.5f) * MetersPerCell() -
                                      halfMapSide);
    cellCenter.Y =
        static_cast<float>((halfMapSide - static_cast<double>(adjCellR + adjTileR * MapCellDim) * MetersPerCell()) -
                           static_cast<double>(MetersPerCell()) * 0.5f);
    cellCenter.Z = 0.0f;
    SetPosition(cellCenter);
    GameObjectMap()->UpdateObject(this);
    Pilot->PausePath();

    if (wasLocked != 0)
    {
        UpdatePathLock(1);
    }

    return dir;
}

auto MCMover::CalcMovePath(MCMovePath* path, MCVector3D start, int32_t thruArea, int32_t goalDoor, MCVector3D finalGoal,
                           MCVector3D* goal, int32_t* goalCell, uint32_t params) -> int32_t
{
    if (PathFindMap() == nullptr)
    {
        Fatal(0, " No PathFindMap() in Mover::calcMovePath ");
    }

    // Within the sector of the area the path goes through.
    const MCGlobalMapArea& area = GlobalMoveMap()->Areas[thruArea];
    const int32_t uLr = area.SectorR * GlobalMoveMap()->SectorDim;
    const int32_t uLc = area.SectorC * GlobalMoveMap()->SectorDim;
    path->Clear();

    if (MaxRunSpeed == 0.0f)
    {
        return 0;
    }

    const int32_t moveLevel = static_cast<int32_t>(static_cast<double>(MetersPerWorldUnit) *
                                                   MCTerrain::MetersPerVertexDivMapcellDim / MaxRunSpeed * 50.0);

    if (moveLevel <= 0)
    {
        return 0;
    }

    int32_t numOffsets;
    int32_t jumpCost;
    SetUpPathJumps(this, numOffsets, jumpCost);
    int32_t startTileR;
    int32_t startTileC;
    int32_t startCellR;
    int32_t startCellC;
    GameMap()->WorldToMapPos(start, startTileR, startTileC, startCellR, startCellC);
    const int32_t sectorDim = GlobalMoveMap()->SectorDim;

    if (!PathFindMap()->SetUp(
            *GameMap(), uLr, uLc, sectorDim, sectorDim, &start, (startTileR - uLr) * MapCellDim + startCellR,
            (startTileC - uLc) * MapCellDim + startCellC, thruArea, goalDoor, finalGoal,
            &OverlayWeightTable[OverlayWeightClass * OverlayWeightLevelSize], moveLevel, jumpCost, numOffsets, params))
    {
        PathFindMap()->JumpOnBlocked = false;
        return -999;
    }

    const int32_t result = PathFindMap()->CalcPath(path, goal, goalCell);
    PathFindMap()->JumpOnBlocked = false;
    return result;
}
