#pragma once

#include "lib/cvmath.h"

class PriorityQueue;
class EditorMap;

// Map geometry. A terrain tile ("map tile") is split into MAPCELL_DIM x MAPCELL_DIM movement cells.
inline constexpr int32_t MAPCELL_DIM = 3;
/// <summary>Largest map side in tiles (sizes the tile coordinate tables).</summary>
inline constexpr int32_t MAX_MAP_TILE_WIDTH = 240;
/// <summary>Number of neighbour offsets the cell path finder can step to (cellShift, reverseShift, ...).</summary>
inline constexpr int32_t NUM_CELL_OFFSETS = 104;
/// <summary>Number of terrain overlay types (the low 7 bits of a tile's second word).</summary>
inline constexpr int32_t NUM_OVERLAY_TYPES = 75;
/// <summary>Number of movement levels in <see cref="OverlayWeightTable"/>.</summary>
inline constexpr int32_t NUM_MOVE_LEVELS = 5;
/// <summary>Entries per movement level of <see cref="OverlayWeightTable"/>: a 3x3 cell cost block per overlay type.</summary>
inline constexpr int32_t OVERLAY_WEIGHT_LEVEL_SIZE = NUM_OVERLAY_TYPES * MAPCELL_DIM * MAPCELL_DIM;
/// <summary>Tiles whose original state the game's ScenarioMap::spreadState can preserve (sizes the map's table).</summary>
inline constexpr int32_t MAX_PRESERVED_TILES = 300;
/// <summary>Steps of a <see cref="MovePath"/>.</summary>
inline constexpr int32_t MAX_STEPS_PER_MOVEPATH = 200;
/// <summary>Steps of a long-range (door to door) path: the size of GlobalMap::calcPathCostTable's path buffer.</summary>
inline constexpr int32_t MAX_GLOBAL_PATH = 80;
/// <summary>Doors the door build list holds while a <see cref="GlobalMap"/> is computed (0x48058 bytes / 0x3b).</summary>
inline constexpr int32_t MAX_BUILD_DOORS = 5000;
/// <summary>Version stamp at the start of a global map file.</summary>
inline constexpr int32_t GLOBALMAP_VERSION = 0x22569;
/// <summary>The two header words after the version, which neither the editor nor the game reads.</summary>
inline constexpr int32_t GLOBALMAP_HEADER_SPARE = 0;
/// <summary>The "no position" value the path finder stores (0xc97423f0).</summary>
inline constexpr float NO_POSITION = -999999.0f;

/// <summary>
/// One terrain tile of the <see cref="ScenarioMap"/>: two bit-packed words, stored raw in the map file.
/// </summary>
/// <remarks>
/// Original header: <c>ai\move.h</c>. 8 bytes (written raw, so the layout is fixed).
/// <para><c>cells</c>: bits 0-6 terrain tile type, bits 7-12 elevation level (added to
/// <see cref="ScenarioMap::baseElevation"/>), bit 13 "preserved", then two bits per cell c = cellR * 3 + cellC:
/// bit 14 + 2c passable, bit 15 + 2c line of sight.</para>
/// <para><c>overlay</c>: bits 0-6 overlay type, bits 11-12 Inner Sphere mine layout, bits 13-14 Clan mine layout,
/// bit 15 + c path locked.</para>
/// </remarks>
typedef struct _MapTile
{
    uint32_t cells;
    uint32_t overlay;
} MapTile;
static_assert(sizeof(MapTile) == 8, "MapTile is written raw to the map file");

/// <summary>A tile whose word the game's ScenarioMap::spreadState saved. Unused by the editor.</summary>
/// <remarks>8 bytes. The name is the port's.</remarks>
struct PreservedTile
{
    int16_t row;
    int16_t col;
    uint32_t cells;
};

/// <summary>
/// The movement map of the whole scenario: one <see cref="MapTile"/> per terrain tile, plus a per-tile count of the
/// paths crossing it. The editor builds it from the terrain when it saves a map and writes it as the <c>.dat</c> file.
/// </summary>
/// <remarks>
/// Original source: <c>ai\move.cpp</c> (inline methods in <c>ai\move.h</c>), the editor's revision. 0x978 bytes.
/// </remarks>
class ScenarioMap
{
public:
    /// <summary>An empty map (the constructor the save code inlines).</summary>
    ScenarioMap() = default;
    ScenarioMap(const ScenarioMap&) = delete;
    ScenarioMap& operator=(const ScenarioMap&) = delete;

    /// <summary>
    /// Allocates an empty map of newHeight x newWidth tiles and fills the tile and cell coordinate tables and the map
    /// scale globals (<see cref="worldUnitsMapSide"/>, <see cref="MetersPerCell"/> and friends).
    /// </summary>
    void init(int32_t newWidth, int32_t newHeight);
    /// <summary>
    /// Builds the map the save writes: the tiles of the terrain file <paramref name="elvName"/> (their types,
    /// elevations, and the cells template.fit, the overlays and the planet block), the corners outside the map's diamond
    /// and its four tips cleared, then the footprints of <paramref name="editorMap"/>'s objects.
    /// </summary>
    /// <param name="dataPath">The game folder, with a trailing backslash (template.fit is in its teditor folder).</param>
    /// <param name="planet">Sets <see cref="CurPlanet"/>: dirt roads block passage.</param>
    /// <returns>1; 0 when a file can't be read (with a message box for template.fit).</returns>
    /// <remarks>The editor's own <c>ScenarioMap::init</c> overload; the name is the port's.</remarks>
    int32_t buildFromTerrain(const char* elvName, EditorMap* editorMap, const char* dataPath, bool planet);
    /// <summary>
    /// Stamps an object's 8x8-cell footprint, centred on its cell, by its game object type: blocks the cells (and
    /// their sight, when <paramref name="blocksLineOfSight"/>), marks its tile as holding an object, gives a gate's tile
    /// its overlay type by <paramref name="fitId"/>, or stores a mine's layout.
    /// </summary>
    /// <remarks>The name is the port's.</remarks>
    void placeTerrainObject(int32_t objectTypeNum, uint32_t footprintLow, uint32_t footprintHigh, int32_t cellCol,
                            int32_t cellRow, bool blocksLineOfSight, int32_t fitId, int32_t mineType);
    /// <summary>Writes the map: height, width, base elevation, then the tiles.</summary>
    /// <returns>1.</returns>
    int32_t write(CFile* mapFile);
    /// <summary>Frees the tiles and the path map.</summary>
    void destroy();

    /// <summary>The tile and cell of a world position.</summary>
    /// <remarks>
    /// Inline in move.h; the editor has an out-of-line copy. It reads only the coordinate globals, so the port makes
    /// it static: the editor calls it on whatever <c>this</c> is at hand, even without a map.
    /// </remarks>
    static void worldToMapPos(vector_3d pos, int32_t& tileR, int32_t& tileC, int32_t& cellR, int32_t& cellC);
    /// <summary>Whether tile (tileR, tileC) is on the map.</summary>
    /// <remarks>Inline in move.h; the editor has an out-of-line copy.</remarks>
    int inBounds(int32_t tileR, int32_t tileC);
    /// <summary>A copy of tile (tileR, tileC).</summary>
    /// <remarks>Inline in move.h; the editor has an out-of-line copy.</remarks>
    MapTile getTile(int32_t tileR, int32_t tileC);

    MapTile* map = nullptr;
    int32_t height = 0;
    int32_t width = 0;
    /// <summary>Elevation level added to every tile's 6-bit elevation.</summary>
    int32_t baseElevation = 0;
    int32_t numPreservedTiles = 0;
    PreservedTile preservedTiles[MAX_PRESERVED_TILES] = {};
    /// <summary>Per tile, how many marked paths cross it.</summary>
    uint8_t* pathMap = nullptr;
};

/// <summary>One step of a <see cref="MovePath"/>: a cell and its world position.</summary>
/// <remarks>0x1c bytes. The original name isn't known; MechCommander 2 calls it PathStep.</remarks>
typedef struct _PathStep
{
    int16_t tileR;
    int16_t tileC;
    int16_t cellR;
    int16_t cellC;
    /// <summary>Distance from this step to the end of the path, in meters.</summary>
    float distanceToGoal;
    vector_3d destination;
    /// <summary>The offset (index of <see cref="cellShift"/>) stepped from the previous step: 0-7 a neighbour, above 7 a
    /// jump. Read as a signed char.</summary>
    uint8_t direction;
} PathStep;

/// <summary>A cell path: up to 200 steps.</summary>
/// <remarks>Original source: <c>ai\move.cpp</c>, the editor's revision. 0x1610 bytes.</remarks>
class MovePath
{
public:
    /// <summary>An empty path (the constructor GlobalMap::calcLinkCost inlines).</summary>
    MovePath()
    {
        goal.zero();
        numSteps = 0;
        numStepsWhenNotPaused = 0;
        curStep = 0;
        cost = 0;
    }

    /// <summary>Calls <see cref="destroy"/>.</summary>
    ~MovePath();
    MovePath(const MovePath&) = delete;
    MovePath& operator=(const MovePath&) = delete;

    /// <summary>Sets the step counts; returns the count when it is the most seen so far, else -1.</summary>
    int32_t init(int32_t newNumSteps);
    /// <summary>Forgets the steps.</summary>
    void destroy();

    /// <summary>World position of the last step.</summary>
    vector_3d goal;
    /// <summary>Copied from <see cref="MoveMap::target"/> when the path is calculated.</summary>
    vector_3d target = {};
    int32_t numSteps;
    int32_t numStepsWhenNotPaused;
    int32_t curStep;
    int32_t cost;
    PathStep stepList[MAX_STEPS_PER_MOVEPATH] = {};
    int32_t marked = 0;
    /// <summary>The long-range path step this path walks, -1 for none.</summary>
    int32_t globalStep = 0;
};

#pragma pack(push, 1)
/// <summary>One door of an area: the door and which of its two sides the area is on.</summary>
/// <remarks>Original: <c>struct _DoorInfo</c>. 3 bytes, written packed in the global map file.</remarks>
typedef struct _DoorInfo
{
    int16_t doorIndex;
    char doorSide;
} DoorInfo;
static_assert(sizeof(DoorInfo) == 3);

/// <summary>A link from a door side to another door of the same area, with its cost.</summary>
/// <remarks>7 bytes, written packed in the global map file. The original name isn't known (MC2: DoorLink).</remarks>
typedef struct _DoorLink
{
    int16_t doorIndex;
    char doorSide;
    int32_t cost;
} DoorLink;
static_assert(sizeof(DoorLink) == 7);
#pragma pack(pop)

/// <summary>An area of the <see cref="GlobalMap"/>: a connected region of tiles within one sector.</summary>
/// <remarks>
/// The original name isn't known (MC2: GlobalMapArea). In the original a 0x29-byte packed record, written raw with the
/// <c>doors</c> pointer inside; the port keeps natural layout and writes the record field by field. Offsets in the
/// comments are the file record's. The words at +0x11, +0x15 and +0x1d-+0x25 are read by neither the editor nor
/// the game; the editor writes -1, nothing (EB-017), 0, 0 and 0 there (<see cref="GLOBALMAP_AREA_SPARE"/>).
/// </remarks>
typedef struct _GlobalMapArea
{
    int16_t sectorR; // +0x0
    int16_t sectorC; // +0x2
    /// <summary>The area's doors (its own allocation).</summary>
    DoorInfo* doors; // +0x4
    /// <summary>0 normal, 1 a north-south bridge, 2 an east-west bridge (road or railroad).</summary>
    int32_t type;   // +0x8
    char numDoors;  // +0xc
    int32_t open;   // +0xd
    int32_t closed; // +0x19
} GlobalMapArea;
/// <summary>Size of a <see cref="GlobalMapArea"/> record in the global map file.</summary>
inline constexpr int32_t GLOBALMAP_AREA_RECORD_SIZE = 0x29;
/// <summary>
/// The values of an area record's spare words, which neither the editor nor the game reads: two after <c>open</c>
/// (+0x11, +0x15), three after <c>closed</c> (+0x1d-+0x25).
/// </summary>
inline constexpr int32_t GLOBALMAP_AREA_SPARE[5] = {-1, 0, 0, 0, 0};

/// <summary>
/// A door of the <see cref="GlobalMap"/>: a run of cells joining two areas, with the A* bookkeeping of
/// <see cref="GlobalMap::calcPath"/>.
/// </summary>
/// <remarks>
/// Original: <c>struct _GlobalMapDoor</c>. In the original a 0x3b-byte packed record, written raw with the two
/// <c>links</c> pointers inside; the port keeps natural layout and writes the record field by field. Offsets in the
/// comments are the file record's.
/// </remarks>
typedef struct _GlobalMapDoor
{
    int16_t row;   // +0x0
    int16_t col;   // +0x2
    uint8_t cellR; // +0x4
    uint8_t cellC; // +0x5
    /// <summary>Length in cells.</summary>
    char length;         // +0x6
    int32_t open;        // +0x7
    int16_t area[2];     // +0xb
    int16_t areaCost[2]; // +0xf
    /// <summary>Exit direction from each side's area.</summary>
    char direction[2]; // +0x13
    /// <summary>Per side, the link count; links[side] has numLinks + 2 entries (room for the start and goal doors).</summary>
    char numLinks[2];   // +0x15
    DoorLink* links[2]; // +0x17
    /// <summary>Cost of the link the search reached this door by.</summary>
    int32_t cost; // +0x1f
    /// <summary>A* parent door.</summary>
    int32_t parent; // +0x23
    /// <summary>Which of <see cref="area"/> (0 or 1) the search reached the door from.</summary>
    int32_t fromAreaIndex; // +0x27
    /// <summary>A* list flags: 1 open, 2 closed.</summary>
    uint32_t flags; // +0x2b
    int32_t g;      // +0x2f
    int32_t hPrime; // +0x33
    int32_t fPrime; // +0x37
} GlobalMapDoor;
/// <summary>Size of a <see cref="GlobalMapDoor"/> record in the global map file.</summary>
inline constexpr int32_t GLOBALMAP_DOOR_RECORD_SIZE = 0x3b;

/// <summary>One step of a long-range path: an area to cross and the door to leave it by.</summary>
/// <remarks>
/// Original: <c>struct _GlobalPathStep</c>, 0x30 bytes; its words at +0x0 and +0xc-+0x20 were never accessed and are
/// gone.
/// </remarks>
typedef struct _GlobalPathStep
{
    int32_t thruArea;
    int32_t goalDoor;
    int32_t goalCell[2];
    int32_t costToGoal;
} GlobalPathStep;

/// <summary>
/// The long-range movement map: the scenario split into sectors of 10x10 tiles, each into areas joined by doors,
/// with the cell path cost of every link and the step count between every two areas. The editor computes it from the
/// <see cref="ScenarioMap"/> when it saves a map and writes it as the <c>.gmm</c> file.
/// </summary>
/// <remarks>Original source: <c>ai\move.cpp</c>, the editor's revision. 0x58 bytes.</remarks>
class GlobalMap
{
public:
    /// <summary>
    /// An empty map with a sector side of 12 (init makes it 10). The constructor the save code inlines; it also resets
    /// <see cref="PathFindMap"/>, without freeing it.
    /// </summary>
    GlobalMap();
    GlobalMap(const GlobalMap&) = delete;
    GlobalMap& operator=(const GlobalMap&) = delete;

    /// <summary>
    /// Allocates an empty area map of newHeight x newWidth tiles; the sector grid is sized from the width alone.
    /// </summary>
    void init(int32_t newWidth, int32_t newHeight);
    /// <summary>
    /// Computes the areas, doors and links of a scenario map (height/width -1: the map's) and makes it
    /// <see cref="GameMap"/>.
    /// </summary>
    /// <returns>0.</returns>
    /// <remarks>
    /// The original also took two values for the file header's words after the version, always 0 from the editor and
    /// read by neither exe; <see cref="write"/> writes <see cref="GLOBALMAP_HEADER_SPARE"/> there.
    /// </remarks>
    int32_t init(ScenarioMap* scenarioMap, int32_t newHeight = -1, int32_t newWidth = -1);
    /// <summary>Writes the global map file, computing the path cost table on the way.</summary>
    /// <returns>0.</returns>
    int32_t write(CFile* mapFile);
    /// <summary>Frees everything, and the shared path finder and open list.</summary>
    void destroy();

    /// <summary>The area of a tile, -1 for none.</summary>
    /// <remarks>Inline in move.h; the editor has an out-of-line copy.</remarks>
    int32_t calcArea(int32_t tileR, int32_t tileC);

    int fillNorthSouthBridgeArea(ScenarioMap* scenarioMap, int32_t row, int32_t col, int32_t area);
    int fillEastWestBridgeArea(ScenarioMap* scenarioMap, int32_t row, int32_t col, int32_t area);
    int fillNorthSouthRailroadBridgeArea(ScenarioMap* scenarioMap, int32_t row, int32_t col, int32_t area);
    int fillEastWestRailroadBridgeArea(ScenarioMap* scenarioMap, int32_t row, int32_t col, int32_t area);
    /// <summary>Flood-fills an area from (row, col) within the current sector bounds.</summary>
    int fillArea(ScenarioMap* scenarioMap, int32_t row, int32_t col, int32_t area);
    void calcSectorAreas(ScenarioMap* scenarioMap, int32_t sectorR, int32_t sectorC);
    void calcAreas(ScenarioMap* scenarioMap);
    void calcBridges(ScenarioMap* scenarioMap);

    void beginDoorProcessing();
    void addDoor(int32_t area1, int32_t area2, int32_t row, int32_t col, int32_t cellR, int32_t cellC, int32_t length,
                 int32_t direction);
    void endDoorProcessing();
    int32_t numAreaDoors(int32_t area);
    void getAreaDoors(int32_t area, DoorInfo* doorList);
    void calcGlobalDoors(ScenarioMap* scenarioMap);
    void calcAreaDoors();
    /// <summary>The cell path cost from one door of an area to another (runs <see cref="PathFindMap"/>).</summary>
    /// <returns>The cost, 9999 when there is no path, -1 or -2 when a door isn't on the area.</returns>
    int32_t calcLinkCost(int32_t startDoor, int32_t thruArea, int32_t goalDoor);
    void calcDoorLinks();
    /// <summary>Does nothing.</summary>
    void calcSectorPaths(ScenarioMap* scenarioMap, int32_t sectorR, int32_t sectorC);
    /// <summary>Fills the area-to-area step table (allocating a new one every call).</summary>
    void calcPathCostTable();

    /// <summary>Adds the temporary start door (index numDoors) joining area <paramref name="startArea"/>.</summary>
    void setStartDoor(int32_t startArea);
    void resetStartDoor(int32_t startArea);
    /// <summary>Adds the temporary goal door (index numDoors + 1) and records the goal sector.</summary>
    void setGoalDoor(int32_t goalArea);
    void resetGoalDoor(int32_t goalArea);
    /// <summary>A* estimate: sector distance from a door to the goal sector.</summary>
    int32_t calcHPrime(int32_t door);
    /// <summary>Finds the door path from one area to another.</summary>
    /// <returns>The number of steps written to <paramref name="path"/>, 0 when there is none, -1 for a bad area.</returns>
    int32_t calcPath(int32_t startArea, int32_t goalArea, GlobalPathStep* path);
    void propogateCost(int32_t door, int32_t cost, int32_t fromSide, int32_t g);
    /// <summary>The number of steps between two areas, from the path cost table (0 for a bad area).</summary>
    int32_t getPathCost(int32_t startArea, int32_t goalArea);

    int32_t height = 0;
    int32_t width = 0;
    /// <summary>Sector side in tiles (10 once initialised).</summary>
    int32_t sectorDim = 0;
    int32_t sectorHeight = 0;
    int32_t sectorWidth = 0;
    int32_t numAreas = 0;
    int32_t numDoors = 0;
    int32_t numDoorInfos = 0;
    int32_t numDoorLinks = 0;
    /// <summary>Area per tile when there are fewer than 256 areas (0xff = none); else <see cref="areaMap"/>.</summary>
    uint8_t* smallAreaMap = nullptr;
    /// <summary>Area per tile (-1 none, -2 blocked).</summary>
    int16_t* areaMap = nullptr;
    /// <summary>numAreas + 1 areas (the last is spare).</summary>
    GlobalMapArea* areas = nullptr;
    /// <summary>numDoors + 2 doors (the last two are the temporary start and goal doors).</summary>
    GlobalMapDoor* doors = nullptr;
    /// <summary>Only set when a map is read (the game's reader); the editor leaves it null.</summary>
    DoorInfo* doorInfos = nullptr;
    /// <summary>Only set when a map is read (the game's reader); the editor leaves it null.</summary>
    DoorLink* doorLinks = nullptr;
    /// <summary>Doors collected while computing the map (MAX_BUILD_DOORS).</summary>
    GlobalMapDoor* doorBuildList = nullptr;
    /// <summary>numAreas x numAreas steps between areas (0 = no path, 0xff = 255 or more).</summary>
    uint8_t* pathCostTable = nullptr;
    int32_t goalSectorR = 0;
    int32_t goalSectorC = 0;
};

/// <summary>One cell of a <see cref="MoveMap"/>.</summary>
/// <remarks>0x18 bytes. The original name isn't known (MC2: MoveMapNode).</remarks>
typedef struct _MoveMapNode
{
    int32_t cost;
    int32_t parent;
    /// <summary>1 on the open list, 2 closed, 4 on the found path, 8 goal, 0x10 a mover stands here.</summary>
    uint32_t flags;
    int32_t g;
    int32_t hPrime;
    int32_t fPrime;
} MoveMapNode;

/// <summary>The short-range cell path finder: an A* over a window of the scenario map's cells.</summary>
/// <remarks>Original source: <c>ai\move.cpp</c>, the editor's revision (no movers, mines or escape paths). 0x88 bytes.</remarks>
class MoveMap
{
public:
    /// <summary>An empty finder (the constructor GlobalMap::calcLinkCost inlines).</summary>
    MoveMap() = default;
    MoveMap(const MoveMap&) = delete;
    MoveMap& operator=(const MoveMap&) = delete;

    /// <summary>Allocates the cells for a window of up to newMaxHeight x newMaxWidth tiles.</summary>
    /// <remarks>Inline in move.h; the editor has an out-of-line copy.</remarks>
    void init(int32_t newMaxWidth, int32_t newMaxHeight);
    /// <remarks>Inline in move.h; the editor has an out-of-line copy.</remarks>
    void clear();
    /// <summary>Does nothing in the editor.</summary>
    void placeMovers(int markGoal);
    /// <summary>Sets the start: a world position (startR -1: computed from it) or a cell.</summary>
    void setStart(vector_3d* newStartPos, int32_t newStartR = -1, int32_t newStartC = -1);
    /// <summary>Sets the goal: a world position (goalR -1: computed from it) or a cell.</summary>
    void setGoal(vector_3d newGoalPos, int32_t newGoalR = -1, int32_t newGoalC = -1);
    /// <summary>Loads the cell costs of a window of the map and sets start and goal.</summary>
    /// <returns>0.</returns>
    int32_t setUp(ScenarioMap* scenarioMap, int32_t newULr, int32_t newULc, int32_t newHeight, int32_t newWidth,
                  vector_3d* newStartPos, int32_t newStartR, int32_t newStartC, vector_3d newGoalPos, int32_t newGoalR,
                  int32_t newGoalC, int32_t* newOverlayWeightTable, int32_t newMoveLevel, int32_t newJumpCost,
                  int32_t newNumOffsets, uint32_t params);
    /// <summary>
    /// Runs the search from the start cell to the nearest goal cell and fills <paramref name="path"/>; the goal cell
    /// reached goes to <paramref name="goalCell"/>.
    /// </summary>
    /// <returns>The path's step count, 0 when no path was found.</returns>
    int32_t calcPath(MovePath* path, vector_3d* goalWorldPos, int32_t* goalCell);
    /// <summary>Frees the cells and the shared open list.</summary>
    void destroy();

protected:
    /// <summary>Whether the neighbour of cell (r, c) in direction <paramref name="dir"/> can be stepped on.</summary>
    int adjacentCellOpen(int32_t r, int32_t c, int32_t dir);
    void propogateCost(int32_t r, int32_t c, int32_t cost, int32_t g);

public:
    /// <summary>Upper-left tile of the window.</summary>
    int32_t ULr = 0;
    int32_t ULc = 0;
    /// <summary>Upper-left cell of the window (ULr * 3, ULc * 3).</summary>
    int32_t minRow = 0;
    int32_t minCol = 0;
    int32_t maxWidth = 0;
    int32_t maxHeight = 0;
    /// <summary>Row stride of <see cref="map"/>, in cells.</summary>
    int32_t maxCellWidth = 0;
    int32_t maxCellHeight = 0;
    int32_t width = 0;
    int32_t height = 0;
    int32_t cellWidth = 0;
    int32_t cellHeight = 0;
    MoveMapNode* map = nullptr;
    vector_3d startPos = {};
    int32_t startR = -1;
    int32_t startC = -1;
    vector_3d goalPos = {};
    int32_t goalR = -1;
    int32_t goalC = -1;
    int32_t door = -1;
    int32_t doorSide = -1;
    /// <summary>Direction the goal door is entered from, -1 when the goal isn't a door.</summary>
    int32_t doorDirection = -1;
    vector_3d target = {NO_POSITION, NO_POSITION, NO_POSITION};
    /// <summary>The cost of a plain passable cell; overlays and locks add to it.</summary>
    int32_t moveLevel = 1;
    /// <summary>Added to the cost of the jump offsets (past the eight neighbours).</summary>
    int32_t jumpCost = 0;
    /// <summary>How many of the NUM_CELL_OFFSETS offsets the search tries.</summary>
    int32_t numOffsets = 8;
    float calcTime = 0.0f;
    int32_t* overlayWeightTable = nullptr;
};

/// <summary>The terrain type of a tile texture index (bands of 79).</summary>
int32_t calcTileTypeFromIndex(int32_t tileIndex);
/// <summary>The overlay type of an overlay texture index.</summary>
int32_t calcOverlayTypeFromIndex(int32_t overlayIndex);
/// <summary>Whether a tile has a full row or column of blocked cells.</summary>
int isLRBlocked(MapTile* tile);

/// <summary>Meters per terrain tile side (a constant in the editor).</summary>
inline constexpr float metersPerVertex = 128.0f;

// Globals of ai\move.cpp.

/// <summary>When set, a wall overlay (0x3c) also keeps a tile out of every area.</summary>
extern int BlockWallTiles;
/// <summary>The window radius, in tiles, of <see cref="PathFindMap"/>.</summary>
extern int32_t SimpleMovePathRange;
/// <summary>Meters per world unit (0.2).</summary>
extern float metersPerWorldUnit;
/// <summary>Row and column offset of each search offset.</summary>
extern int32_t cellShift[NUM_CELL_OFFSETS * 2];
/// <summary>The offset back to the cell a step came from.</summary>
extern char reverseShift[NUM_CELL_OFFSETS];
extern int IsDiagonalStep[NUM_CELL_OFFSETS];
/// <summary>Per diagonal offset i, the two orthogonal offsets beside it (StepAdjDir[i], StepAdjDir[i + 1]).</summary>
extern int32_t StepAdjDir[9];
/// <summary>Row and column step of the four tile directions (north, east, south, west).</summary>
extern int32_t adjTile[4][2];
/// <summary>Per mine layout, the mine type of each of the nine cells.</summary>
extern char mineLayout[4][9];
/// <summary>The cell search's cut-off for the estimate; calcPath sets it per search.</summary>
extern int32_t MaxHPrime;
/// <summary>Cell costs per move level and overlay. GlobalMap::calcLinkCost resets part of it.</summary>
extern std::array<int32_t, NUM_MOVE_LEVELS * OVERLAY_WEIGHT_LEVEL_SIZE> OverlayWeightTable;
/// <summary>Start of each overlay type's 3x3 block in a level of <see cref="OverlayWeightTable"/>.</summary>
/// <remarks>Never filled in the editor: every overlay reads overlay 0's block, which is all zeros.</remarks>
extern int32_t OverlayWeightIndex[NUM_OVERLAY_TYPES];
extern float cellToWorldCoord[MAPCELL_DIM];
/// <summary>tile * MAPCELL_DIM.</summary>
extern int32_t tileMulMAPCELL_DIM[MAX_MAP_TILE_WIDTH];
extern float cellShiftDistance[NUM_CELL_OFFSETS];
extern float tileRowToWorldCoord[MAX_MAP_TILE_WIDTH];
extern float tileColToWorldCoord[MAX_MAP_TILE_WIDTH];
/// <summary>Map side in meters, set by ScenarioMap::init.</summary>
extern float worldUnitsMapSide;
/// <summary>When set, the bridges' middle cells are made cheap for the search (GlobalMap::calcLinkCost).</summary>
extern int ClearBridgeTiles;
/// <summary>The A* open list both path finders share.</summary>
extern PriorityQueue* openList;
/// <summary>Never set in the editor.</summary>
extern int JumpOnBlocked;
/// <summary>Never set in the editor.</summary>
extern int FindingEscapePath;
/// <summary>The scenario map GlobalMap::init last computed.</summary>
extern ScenarioMap* GameMap;
/// <summary>Never set in the editor.</summary>
extern GlobalMap* GlobalMoveMap;
/// <summary>
/// Set from the map builder's last argument. When 1, dirt roads (overlays 1-15) cost nothing in calcLinkCost; else
/// those weights are reset to the built-in table.
/// </summary>
extern int32_t CurPlanet;
extern float MapCellDiagonal;
extern float HalfMapCell;
extern float VerticesMapSideDivTwo;
extern float MetersMapSideDivTwo;
extern int GoalIsDoor;
extern float MetersPerCell;
/// <summary>The cell path finder of GlobalMap::calcLinkCost, made on first use.</summary>
extern MoveMap* PathFindMap;
