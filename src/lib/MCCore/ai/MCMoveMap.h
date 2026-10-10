#pragma once

#include "lib/MCVector3D.h"

class MCFile;
class MCGameObject;
class MCMovePath;
class MCPriorityQueue;
class MCScenarioMap;
struct MCMapTile;
class MCObjectList;

/// <summary>One cell of a <see cref="MCMoveMap"/>.</summary>
/// <remarks>The original name isn't known (MC2: MoveMapNode).</remarks>
struct MCMoveMapNode
{
    int32_t Cost = 0;
    int32_t Parent = 0;
    /// <summary>Flags: 1 on the open list, 2 closed, 4 on the found path, 8 goal, 0x10 a mover stands here.</summary>
    uint32_t Flags = 0;
    int32_t G = 0;
    int32_t HPrime = 0;
    int32_t FPrime = 0;
};

/// <summary>The short-range cell path finder: an A* over a window of the scenario map's cells.</summary>
/// <remarks>
/// Original source: <c>ai\move.cpp</c>. The cells keep their costs between searches: a window reaching past the
/// map's edge leaves the cells beyond it as the last search that covered them set them, as in the original.
/// </remarks>
class MCMoveMap
{
public:
    /// <summary>Cell flag: on the open list.</summary>
    static constexpr uint32_t OpenFlag = 1;
    /// <summary>Cell flag: closed.</summary>
    static constexpr uint32_t ClosedFlag = 2;
    /// <summary>Cell flag: on the path found.</summary>
    static constexpr uint32_t PathFlag = 4;
    /// <summary>Cell flag: a goal cell.</summary>
    static constexpr uint32_t GoalFlag = 8;
    /// <summary>Cell flag: a standing mover blocks it.</summary>
    static constexpr uint32_t MoverFlag = 0x10;
    /// <summary>The cost of an impassable cell; a cell costing this much or more is never entered.</summary>
    static constexpr int32_t BlockedCost = 10000;
    /// <summary>The cost a closed gate or a standing mover adds.</summary>
    static constexpr int32_t ClosedCost = 20000;

    /// <summary>
    /// A path finder for windows of up to <paramref name="maxWidth"/> x <paramref name="maxHeight"/> tiles, searching
    /// with <paramref name="openList"/>.
    /// </summary>
    MCMoveMap(int32_t maxWidth, int32_t maxHeight, MCPriorityQueue& openList);

    /// <summary>Loads the cell costs of a window of the map and sets start and goal (a position or a cell).</summary>
    /// <param name="overlayWeightTable">The mover's level of <see cref="OverlayWeightTable"/> (null: level 0).</param>
    /// <param name="moveLevel">The cost of a plain passable cell.</param>
    /// <param name="jumpCost">What a jump offset adds (with <see cref="JumpOnBlocked"/>, its whole cost).</param>
    /// <param name="numOffsets">How many of the offsets the search tries (8: no jumps).</param>
    /// <param name="params">0x40: standing mechs block; 0x80: path locks cost.</param>
    void SetUp(const MCScenarioMap& map, int32_t uLr, int32_t uLc, int32_t height, int32_t width,
               const MCVector3D* startPos, int32_t startR, int32_t startC, MCVector3D goalPos, int32_t goalR,
               int32_t goalC, int32_t* overlayWeightTable, int32_t moveLevel, int32_t jumpCost, int32_t numOffsets,
               uint32_t params);
    /// <summary>As the other SetUp, with the goal door <paramref name="goalDoor"/> of the global map, leaving
    /// <paramref name="thruArea"/>.</summary>
    /// <returns>False when no cell of the door is free to aim at.</returns>
    bool SetUp(const MCScenarioMap& map, int32_t uLr, int32_t uLc, int32_t height, int32_t width,
               const MCVector3D* startPos, int32_t startR, int32_t startC, int32_t thruArea, int32_t goalDoor,
               MCVector3D targetPos, int32_t* overlayWeightTable, int32_t moveLevel, int32_t jumpCost,
               int32_t numOffsets, uint32_t params);
    /// <summary>
    /// Runs the search from the start cell to the nearest goal cell and fills <paramref name="path"/>; the goal
    /// cell reached (its map cell, when the goal is a door) goes to <paramref name="goalCell"/>.
    /// </summary>
    /// <returns>The path's step count, 0 when no path was found.</returns>
    int32_t CalcPath(MCMovePath* path, MCVector3D* goalWorldPos, int32_t* goalCell);
    /// <summary>As <see cref="CalcPath"/>, with a flat estimate (every cell 10 from the goal cells), to the cells of
    /// any area with a path to the goal's.</summary>
    int32_t CalcEscapePath(MCMovePath* path, MCVector3D* goalWorldPos, int32_t* goalCell);

    /// <summary>The node of window cell (r, c).</summary>
    MCMoveMapNode& NodeAt(int32_t r, int32_t c) { return _Map[static_cast<size_t>(MaxCellWidth * r + c)]; }
    const MCMoveMapNode& NodeAt(int32_t r, int32_t c) const { return _Map[static_cast<size_t>(MaxCellWidth * r + c)]; }

    /// <summary>Upper-left tile of the window.</summary>
    int32_t ULr = 0;
    int32_t ULc = 0;
    /// <summary>Upper-left cell of the window (ULr * 3, ULc * 3).</summary>
    int32_t MinRow = 0;
    int32_t MinCol = 0;
    int32_t MaxWidth = 0;
    int32_t MaxHeight = 0;
    /// <summary>Row stride of the nodes, in cells.</summary>
    int32_t MaxCellWidth = 0;
    int32_t MaxCellHeight = 0;
    int32_t Width = 0;
    int32_t Height = 0;
    int32_t CellWidth = 0;
    int32_t CellHeight = 0;
    MCVector3D StartPos;
    int32_t StartR = 0;
    int32_t StartC = 0;
    MCVector3D GoalPos;
    int32_t GoalR = 0;
    int32_t GoalC = 0;
    /// <summary>The goal door, when the goal is a door.</summary>
    int32_t Door = 0;
    int32_t DoorSide = 0;
    /// <summary>Direction the goal door is entered from, -1 when the goal isn't a door.</summary>
    int32_t DoorDirection = 0;
    MCVector3D Target;
    /// <summary>The cost of a plain passable cell (SetUp's moveLevel); overlays, locks and mines add to it.</summary>
    int32_t MoveLevel = 0;
    /// <summary>Added to the cost of the jump offsets (the ones past the eight neighbours); with
    /// <see cref="JumpOnBlocked"/> it is their whole cost.</summary>
    int32_t JumpCost = 0;
    /// <summary>How many of the offsets the search tries.</summary>
    int32_t NumOffsets = 0;
    /// <summary>The estimate past which a cell isn't searched (2.5 x the start's distance, at least 500).</summary>
    int32_t MaxHPrime = 1000;
    /// <summary>The overlay costs of the mover's level (ClearBridgeTiles lowers and restores some of them).</summary>
    int32_t* OverlayWeights = nullptr;

    // The search's settings, which the callers set around a search (globals in the original).

    /// <summary>The object whose path is being calculated: it doesn't block itself, and decides gates and mines.</summary>
    MCGameObject* MovingObject = nullptr;
    /// <summary>The object a ramming attack aims at (it doesn't block either).</summary>
    MCGameObject* RamObject = nullptr;
    /// <summary>A jump costs only the jump cost (an elemental closing in jumps over what blocks it).</summary>
    bool JumpOnBlocked = false;
    /// <summary>The next SetUp marks escape goal cells rather than one goal cell.</summary>
    bool FindingEscapePath = false;
    /// <summary>The bridge cells' overlay costs are lowered by 10000 for the search (the link costs of the global
    /// map).</summary>
    bool ClearBridgeTiles = false;
    /// <summary>The kind of path being planned (Mover::calcMovePath's type; the "Bad Move Goal" message shows it).</summary>
    int32_t DebugMovePathType = 0;

private:
    /// <summary>Resets the search state of the window's nodes (their parents, flags and estimates).</summary>
    void Clear();
    /// <summary>The setup the two SetUps share: the window, the start and the settings.</summary>
    void BeginSetUp(const MCScenarioMap& map, int32_t uLr, int32_t uLc, int32_t height, int32_t width,
                    const MCVector3D* startPos, int32_t startR, int32_t startC, int32_t* overlayWeightTable,
                    int32_t moveLevel, int32_t jumpCost, int32_t numOffsets);
    /// <summary>Gives each cell of a tile of the window its base and overlay cost.</summary>
    void SetTileCosts(const MCMapTile& tile, int32_t row, int32_t col);
    /// <summary>Adds the cost of the standing mechs (other than the mover) to their cells.</summary>
    void PlaceMovers();
    void SetStart(const MCVector3D* startPos, int32_t startR, int32_t startC);
    void SetGoal(MCVector3D goalPos, int32_t goalR, int32_t goalC);
    void SetGoal(int32_t thruArea, int32_t goalDoor);
    /// <summary>Marks every cell of the window whose area has a path to the escape goal's area.</summary>
    void MarkEscapeGoalCells(MCVector3D escapeGoal);
    /// <summary>Marks the free cells of the goal door (or the one cell past it the target is in).</summary>
    /// <returns>The number of cells marked.</returns>
    int32_t MarkGoalCells(MCVector3D targetPos);
    /// <summary>Whether a mover may cut past a neighbour (in the window, no mover, no known mine, passable).</summary>
    bool AdjacentCellOpen(int32_t r, int32_t c, int32_t dir) const;
    /// <summary>Whether a standing mover blocks cells (not an elemental, not the mover or its ram target, alive).</summary>
    bool IsBlockingMover(MCGameObject* object) const;
    /// <summary>The cost of stepping into a cell of <paramref name="cellCost"/> by <paramref name="offset"/>.</summary>
    int32_t StepCost(int32_t cellCost, int32_t offset) const;
    void PropogateCost(int32_t r, int32_t c, int32_t cost, int32_t g);
    /// <summary>
    /// The A* search and path building CalcPath and CalcEscapePath share (MCX has two copies that differ only in
    /// the estimate and their messages).
    /// </summary>
    int32_t SearchPath(MCMovePath* path, MCVector3D* goalWorldPos, int32_t* goalCell, bool escape);

    /// <summary>The window's nodes, MaxCellHeight x MaxCellWidth.</summary>
    std::vector<MCMoveMapNode> _Map;
    /// <summary>The A* open list (shared with the global map).</summary>
    MCPriorityQueue* _OpenList = nullptr;
    /// <summary>The map of the last SetUp.</summary>
    const MCScenarioMap* _ScenarioMap = nullptr;
    /// <summary>MarkGoalCells's per cell "free" state of the goal door (a function static in MCX).</summary>
    std::vector<bool> _DoorCellFree;
};

/// <summary>Dumps an open list to openlist.dbg, after <paramref name="message"/>.</summary>
void DebugOpenList(const MCPriorityQueue& openList, std::string_view message);
