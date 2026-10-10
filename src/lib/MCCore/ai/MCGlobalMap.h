#pragma once

#include "lib/MCVector3D.h"

class MCFile;
class MCMoveMap;
class MCPriorityQueue;
class MCScenarioMap;

#pragma pack(push, 1)
/// <summary>One door of an area: the door and which of its two sides the area is on.</summary>
/// <remarks>Original: <c>struct _DoorInfo</c>. Fixed layout: 3 bytes, stored packed in the global map file.</remarks>
struct MCDoorInfo
{
    int16_t DoorIndex = 0;
    int8_t DoorSide = 0;
};

static_assert(sizeof(MCDoorInfo) == 3);

/// <summary>A link from a door side to another door of the same area, with its cost.</summary>
/// <remarks>Fixed layout: 7 bytes, stored packed in the global map file. The original name isn't known (MC2:
/// DoorLink).</remarks>
struct MCDoorLink
{
    int16_t DoorIndex = 0;
    int8_t DoorSide = 0;
    int32_t Cost = 0;
};

static_assert(sizeof(MCDoorLink) == 7);
#pragma pack(pop)

/// <summary>An area of the <see cref="MCGlobalMap"/>: a connected region of tiles within one sector.</summary>
/// <remarks>
/// The original name isn't known (MC2: GlobalMapArea). The file record is 0x29 bytes (<see cref="MCGlobalMap::AreaRecordSize"/>):
/// sectorR s16, sectorC s16, doors u32 (the editor's pointer, ignored), type s32, numDoors s8, open s32, two
/// editor-only words, closed s32, three editor-only words. The editor's calcAreas writes -1, never set, 0, 0, 0 into
/// the editor-only words, and neither the game nor the editor reads them; the port skips them on reading and writes the
/// editor's values.
/// </remarks>
struct MCGlobalMapArea
{
    int16_t SectorR = 0;
    int16_t SectorC = 0;
    /// <summary>0 normal, 1 a north-south bridge, 2 an east-west bridge (road or railroad; see CalcBridges).</summary>
    int32_t Type = 0;
    /// <summary>The area's doors: <see cref="NumDoors"/> entries of <see cref="MCGlobalMap::DoorInfos"/> from
    /// <see cref="FirstDoorInfo"/>.</summary>
    int32_t FirstDoorInfo = 0;
    int32_t NumDoors = 0;
    int32_t Open = 0;
    /// <summary>Set by <see cref="MCGlobalMap::CloseArea"/>.</summary>
    int32_t Closed = 0;
};

/// <summary>
/// A door of the <see cref="MCGlobalMap"/>: a run of cells joining two areas, with the A* bookkeeping of
/// <see cref="MCGlobalMap::CalcPath"/>.
/// </summary>
/// <remarks>
/// Original: <c>struct _GlobalMapDoor</c>. The file record is 0x3b bytes (<see cref="MCGlobalMap::DoorRecordSize"/>),
/// with the two link list pointers inside (written as 0, ignored on reading).
/// </remarks>
struct MCGlobalMapDoor
{
    int16_t Row = 0;
    int16_t Col = 0;
    uint8_t CellR = 0;
    uint8_t CellC = 0;
    /// <summary>Length in cells.</summary>
    int8_t Length = 0;
    int32_t Open = 0;
    std::array<int16_t, 2> Area{};
    std::array<int16_t, 2> AreaCost{};
    /// <summary>Exit direction from each side's area.</summary>
    std::array<int8_t, 2> Direction{};
    /// <summary>
    /// Per side, the link count. The side's list in <see cref="MCGlobalMap::DoorLinks"/> has NumLinks + 2 entries from
    /// <see cref="FirstLink"/> (room for the start and goal doors).
    /// </summary>
    std::array<int8_t, 2> NumLinks{};
    std::array<int32_t, 2> FirstLink{};
    /// <summary>Cost of the link the search reached this door by.</summary>
    int32_t Cost = 0;
    /// <summary>A* parent door.</summary>
    int32_t Parent = 0;
    /// <summary>Which of <see cref="Area"/> (0 or 1) the search reached the door from.</summary>
    int32_t FromAreaIndex = 0;
    /// <summary>A* list flags: 1 open, 2 closed.</summary>
    uint32_t Flags = 0;
    int32_t G = 0;
    int32_t HPrime = 0;
    int32_t FPrime = 0;
};

/// <summary>One step of a long-range path: an area to cross and the door to leave it by.</summary>
/// <remarks>Original: <c>struct _GlobalPathStep</c>; the words at +0x0 and +0xc .. +0x20 were never accessed and are
/// gone.</remarks>
struct MCGlobalPathStep
{
    int32_t ThruArea = 0;
    int32_t GoalDoor = 0;
    /// <summary>The cell (row, column) the leg's path ended in (Mover::calcMovePath fills it); the next leg starts
    /// from it (MechWarrior::calcMovePath).</summary>
    int32_t GoalCell[2]{};
    int32_t CostToGoal = 0;
};

/// <summary>
/// The long-range movement map: the scenario split into sectors of 10x10 tiles, each into areas joined by doors,
/// searched with A* over doors before a <see cref="MCMoveMap"/> plans the cells of each leg.
/// </summary>
/// <remarks>Original source: <c>ai\move.cpp</c>.</remarks>
class MCGlobalMap
{
public:
    /// <summary>Version stamp at the start of a global map file.</summary>
    static constexpr int32_t FileVersion = 0x22569;
    /// <summary>Size of an area record in the global map file.</summary>
    static constexpr int32_t AreaRecordSize = 0x29;
    /// <summary>Size of a door record in the global map file.</summary>
    static constexpr int32_t DoorRecordSize = 0x3b;
    /// <summary>Sector side in tiles.</summary>
    static constexpr int32_t DefaultSectorDim = 10;
    /// <summary>
    /// Steps a pilot's long-range path holds (MechWarrior's move orders keep them, with an 8-bit count; the door
    /// search asserts below it).
    /// </summary>
    static constexpr int32_t MaxPathSteps = 80;

    /// <summary>Reads a global map file (see <see cref="Write"/>); the door search uses <paramref name="openList"/>.</summary>
    MCGlobalMap(MCFile& mapFile, MCPriorityQueue& openList);
    /// <summary>
    /// Computes the areas, doors and links of a scenario map (the editor's work), planning the link costs with
    /// <paramref name="pathFinder"/>.
    /// </summary>
    /// <remarks>MCEditor.exe calls it as init(map, 0, 0, -1, -1): the two values after the map only went to the file
    /// header's words 1 and 2, which nothing reads, and are gone; so are the height and width (always the map's).</remarks>
    MCGlobalMap(const MCScenarioMap& map, MCMoveMap& pathFinder, MCPriorityQueue& openList);

    /// <summary>Writes the global map file (recomputing the path cost table); header words 1 and 2 are written as 0,
    /// as the editor wrote them.</summary>
    void Write(MCFile& mapFile);

    /// <summary>The area of a tile, -1 for none (or off the map).</summary>
    int32_t CalcArea(int32_t tileR, int32_t tileC) const;

    /// <summary>The doors of an area.</summary>
    std::span<MCDoorInfo> AreaDoors(int32_t area);
    /// <summary>A door side's links (NumLinks of them, plus the spare room for the start and goal doors).</summary>
    std::span<MCDoorLink> DoorSideLinks(int32_t door, int32_t side);

    /// <summary>The centre of the cell a leg's path ended in (<paramref name="prevGoalCell"/>), on the ground.</summary>
    static MCVector3D GetDoorWorldPos(const int32_t* prevGoalCell);

    /// <summary>
    /// Finds the door path from one area to another, writing at most <paramref name="path"/>'s size steps.
    /// </summary>
    /// <returns>The number of steps, 0 when there is none, -1 when either area is -1.</returns>
    int32_t CalcPath(int32_t startArea, int32_t goalArea, std::span<MCGlobalPathStep> path);
    /// <summary>As <see cref="CalcPath(int32_t, int32_t, std::span{MCGlobalPathStep})"/>, between the areas of two
    /// positions.</summary>
    int32_t CalcPath(MCVector3D start, MCVector3D goal, std::span<MCGlobalPathStep> path);
    /// <summary>The number of steps between two areas, from the path cost table.</summary>
    int32_t GetPathCost(int32_t startArea, int32_t goalArea) const;
    void CloseDoor(int32_t door);
    /// <summary>Closes an area and its doors and clears its path cost entries.</summary>
    void CloseArea(int32_t area);
    /// <summary>Dumps a rectangle of a computed map's areas to a text file (nothing for a loaded one).</summary>
    void Print(std::string_view fileName, int32_t uLr, int32_t uLc, int32_t height, int32_t width) const;

    int32_t Height = 0;
    int32_t Width = 0;
    /// <summary>Sector side in tiles (10).</summary>
    int32_t SectorDim = 0;
    int32_t SectorHeight = 0;
    int32_t SectorWidth = 0;
    int32_t NumAreas = 0;
    int32_t NumDoors = 0;
    int32_t NumDoorInfos = 0;
    int32_t NumDoorLinks = 0;
    /// <summary>Area per tile when there are fewer than 256 areas (0xff = none); else <see cref="AreaMap"/>.</summary>
    std::vector<uint8_t> SmallAreaMap;
    /// <summary>Area per tile (-1 = none, -2 = blocked while computing).</summary>
    std::vector<int16_t> AreaMap;
    /// <summary>NumAreas areas and a spare one (<see cref="SetTempArea"/>).</summary>
    std::vector<MCGlobalMapArea> Areas;
    /// <summary>NumDoors + 2 doors (the last two are the temporary start and goal doors).</summary>
    std::vector<MCGlobalMapDoor> Doors;
    /// <summary>The areas' door lists, area by area.</summary>
    std::vector<MCDoorInfo> DoorInfos;
    /// <summary>The doors' link lists, door by door and side by side.</summary>
    std::vector<MCDoorLink> DoorLinks;
    /// <summary>NumAreas x NumAreas steps between areas (0 = no path, 0xff = 255 or more).</summary>
    std::vector<uint8_t> PathCostTable;
    int32_t GoalSectorR = 0;
    int32_t GoalSectorC = 0;

private:
    /// <summary>Allocates an empty area map of the given size (multiples of 10); the sector grid is sized from the
    /// width alone.</summary>
    void InitAreaMap(int32_t newWidth, int32_t newHeight);

    int32_t FillNorthSouthBridgeArea(const MCScenarioMap& map, int32_t row, int32_t col, int32_t area);
    int32_t FillEastWestBridgeArea(const MCScenarioMap& map, int32_t row, int32_t col, int32_t area);
    int32_t FillNorthSouthRailroadBridgeArea(const MCScenarioMap& map, int32_t row, int32_t col, int32_t area);
    int32_t FillEastWestRailroadBridgeArea(const MCScenarioMap& map, int32_t row, int32_t col, int32_t area);
    /// <summary>Flood-fills an area from (row, col) within the current sector bounds.</summary>
    int32_t FillArea(const MCScenarioMap& map, int32_t row, int32_t col, int32_t area);
    /// <summary>Whether (row, col) lies in the sector being filled.</summary>
    bool InFillSector(int32_t row, int32_t col) const;
    /// <summary>Sets the sector being filled.</summary>
    void SetFillSector(int32_t sectorR, int32_t sectorC);
    void CalcSectorAreas(const MCScenarioMap& map, int32_t sectorR, int32_t sectorC);
    void CalcAreas(const MCScenarioMap& map);
    void CalcBridges(const MCScenarioMap& map);
    /// <summary>Adds a door to the build list, unless it is there already.</summary>
    static void AddDoor(std::vector<MCGlobalMapDoor>& buildList, int32_t area1, int32_t area2, int32_t row, int32_t col,
                        int32_t cellR, int32_t cellC, int32_t length, int32_t direction);
    void CalcGlobalDoors(const MCScenarioMap& map);
    void CalcAreaDoors();
    /// <summary>The cell path cost from one door of an area to another (runs the path finder).</summary>
    int32_t CalcLinkCost(int32_t startDoor, int32_t thruArea, int32_t goalDoor);
    void CalcDoorLinks();
    void CalcPathCostTable();
    /// <summary>Adds the temporary start door (index NumDoors) joining area <paramref name="startArea"/>.</summary>
    void SetStartDoor(int32_t startArea);
    void ResetStartDoor(int32_t startArea);
    /// <summary>Adds the temporary goal door (index NumDoors + 1) and records the goal sector.</summary>
    void SetGoalDoor(int32_t goalArea);
    void ResetGoalDoor(int32_t goalArea);
    /// <summary>Links the doors of an area back to a temporary door, in the spare room past their links.</summary>
    void LinkTempDoor(int32_t tempDoor, int32_t areaIndex);
    /// <summary>A* estimate: sector distance from a door to the goal sector.</summary>
    int32_t CalcHPrime(int32_t door) const;
    void PropogateCost(int32_t door, int32_t cost, int32_t fromSide, int32_t g);

    /// <summary>The A* open list (shared with the cell path finder).</summary>
    MCPriorityQueue* _OpenList = nullptr;
    /// <summary>The cell path finder the link costs are planned with (only while computing a map).</summary>
    MCMoveMap* _PathFinder = nullptr;
    /// <summary>The scenario map being computed from (only while computing a map).</summary>
    const MCScenarioMap* _ScenarioMap = nullptr;
    /// <summary>Tile bounds of the sector being filled.</summary>
    int32_t _MinTileR = 0;
    int32_t _MaxTileR = 0;
    int32_t _MinTileC = 0;
    int32_t _MaxTileC = 0;
};
