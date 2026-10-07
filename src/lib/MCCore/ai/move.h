#pragma once

#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "platform/MCBlockStore.h"

class MCFile;
class MCFitIniFile;
class MCPriorityQueue;
class MCGameObject;
class MCMover;
class MCMechWarrior;
class MCScenario;
class MCObjectBlockManager;
class MCMovePath;
struct MCObjectQueueNode;

// Map geometry. A terrain tile ("map tile") is split into MAPCELL_DIM x MAPCELL_DIM movement cells.
inline constexpr int32_t MAPCELL_DIM = 3;
/// <summary>Largest map side in tiles (sizes the tile coordinate tables).</summary>
inline constexpr int32_t MAX_MAP_TILE_WIDTH = 240;
/// <summary>Largest map side in cells.</summary>
inline constexpr int32_t MAX_MAP_CELL_WIDTH = MAX_MAP_TILE_WIDTH * MAPCELL_DIM;
/// <summary>Number of neighbour offsets the cell path finder can step to (cellShift, reverseShift, ...).</summary>
inline constexpr int32_t NUM_CELL_OFFSETS = 104;
/// <summary>Number of terrain overlay types (the low 7 bits of a tile's second word).</summary>
inline constexpr int32_t NUM_OVERLAY_TYPES = 75;
/// <summary>Number of movement levels in <see cref="OverlayWeightTable"/> (read as 0xd2f longs by the scenario).</summary>
inline constexpr int32_t NUM_MOVE_LEVELS = 5;
/// <summary>Entries per movement level of <see cref="OverlayWeightTable"/>: a 3x3 cell cost block per overlay type.</summary>
inline constexpr int32_t OVERLAY_WEIGHT_LEVEL_SIZE = NUM_OVERLAY_TYPES * MAPCELL_DIM * MAPCELL_DIM;
/// <summary>Tiles whose original state <see cref="MCScenarioMap::SpreadState"/> can preserve while movers are placed.</summary>
inline constexpr int32_t MAX_PRESERVED_TILES = 300;
/// <summary>Steps of a <see cref="MCMovePath"/>.</summary>
inline constexpr int32_t MAX_STEPS_PER_MOVEPATH = 200;
/// <summary>Steps of a long-range (door to door) path (the "Too Many Long Range Move Steps" assert).</summary>
inline constexpr int32_t MAX_GLOBAL_PATH = 80;
/// <summary>Path requests <see cref="MCMovePathManager"/> can queue.</summary>
inline constexpr int32_t MAX_PATH_QUEUE_RECS = 300;
/// <summary>Steps a <see cref="MCMoveChunk"/> carries (a net update of a mover's path).</summary>
inline constexpr int32_t MOVECHUNK_NUM_STEPS = 4;
/// <summary>Doors the door build list holds while a <see cref="MCGlobalMap"/> is computed (0x48058 bytes / 0x3b).</summary>
inline constexpr int32_t MAX_BUILD_DOORS = 5000;
/// <summary>Version stamp at the start of a global map file.</summary>
inline constexpr int32_t GLOBALMAP_VERSION = 0x22569;

/// <summary>
/// One terrain tile of the <see cref="MCScenarioMap"/>: two bit-packed words, stored raw in the map file.
/// </summary>
/// <remarks>
/// Original header: <c>ai\move.h</c>. 8 bytes (read and written raw, so the layout is fixed).
/// <para><c>cells</c>: bits 0-6 terrain tile type, bits 7-12 elevation level (added to
/// <see cref="MCScenarioMap::BaseElevation"/>), bit 13 "preserved" (see <see cref="MCScenarioMap::SpreadState"/>),
/// then two bits per cell c = cellR * 3 + cellC: bit 14 + 2c passable, bit 15 + 2c line of sight.</para>
/// <para><c>overlay</c>: bits 0-6 overlay type (index of <see cref="OverlayIsBridge"/> and friends), bits 7-8 set
/// from the terrain object blocks, bits 11-12 Inner Sphere mine layout, bits 13-14 Clan mine layout (rows of
/// <see cref="MineLayout"/>), bit 15 + c path locked.</para>
/// </remarks>
typedef struct MCMapTile
{
    uint32_t Cells = 0;
    uint32_t Overlay = 0;

    /// <summary>Whether cell (cellR, cellC) can be entered.</summary>
    uint32_t GetCellPassable(int32_t cellR, int32_t cellC)
    {
        const uint32_t shift = static_cast<uint32_t>((cellR * MAPCELL_DIM + cellC) * 2);
        return (Cells & (0x4000u << shift)) >> (shift + 14);
    }

    /// <summary>Whether a mover's path has locked cell (cellR, cellC).</summary>
    uint32_t GetCellPathLocked(int32_t cellR, int32_t cellC)
    {
        const uint32_t shift = static_cast<uint32_t>(cellR * MAPCELL_DIM + cellC);
        return (Overlay & (0x8000u << shift)) >> (shift + 15);
    }

    /// <summary>Sets or clears the path lock of cell (cellR, cellC).</summary>
    void SetCellPathLocked(int32_t cellR, int32_t cellC, uint32_t locked)
    {
        const uint32_t shift = static_cast<uint32_t>(cellR * MAPCELL_DIM + cellC);
        Overlay = (locked << (shift + 15)) | (~(0x8000u << shift) & Overlay);
    }
} MCMapTile;
static_assert(sizeof(MCMapTile) == 8, "MapTile is read raw from the map file");

/// <summary>A tile whose word <see cref="MCScenarioMap::SpreadState"/> saved, to be put back by restorePreservedMap.</summary>
/// <remarks>8 bytes. The name is the port's (the original's isn't known).</remarks>
struct MCPreservedTile
{
    int16_t Row = 0;
    int16_t Col = 0;
    uint32_t Cells = 0;
};

/// <summary>
/// The movement map of the whole scenario: one <see cref="MCMapTile"/> per terrain tile, plus a per-tile count of the
/// paths crossing it.
/// </summary>
/// <remarks>Original source: <c>ai\move.cpp</c> (inline methods in <c>ai\move.h</c>). 0x978 bytes.</remarks>
class MCScenarioMap
{
public:
    /// <summary>
    /// Allocates an empty map of newHeight x newWidth tiles and fills the tile coordinate tables (not the cell ones).
    /// </summary>
    void Init(int32_t newWidth, int32_t newHeight);
    /// <summary>Reads the map (height, width, base elevation, then the tiles) and fills the coordinate tables.</summary>
    int32_t Init(MCFile* mapFile);
    /// <summary>Does nothing (returns 0).</summary>
    int32_t Init(MCScenario* scenario);
    /// <summary>Writes the map in the format <see cref="init(File*)"/> reads.</summary>
    int32_t Write(MCFile* mapFile);
    /// <summary>Frees the tiles and the path map.</summary>
    void Destroy();

    /// <summary>The tile and cell of a world position.</summary>
    void WorldToMapPos(MCVector3D pos, int32_t& tileR, int32_t& tileC, int32_t& cellR, int32_t& cellC);
    /// <summary>The tile of a world position.</summary>
    void WorldToMapTilePos(MCVector3D pos, int32_t& tileR, int32_t& tileC);
    /// <summary>
    /// Whether tile (tileR, tileC) is on the map (port helper). The original indexes <see cref="Map"/> with the tile of
    /// any world point; the port checks the ones that can fall off the map (a scattered shot, a point walked out from
    /// a unit, a player's waypoint).
    /// </summary>
    bool OnMap(int32_t tileR, int32_t tileC) const
    {
        return tileR >= 0 && tileR < Height && tileC >= 0 && tileC < Width;
    }

    int GetCellPathLocked(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC)
    {
        return static_cast<int>(Map[Width * tileR + tileC].GetCellPathLocked(cellR, cellC));
    }

    void SetCellPathLocked(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, uint32_t locked)
    {
        Map[Width * tileR + tileC].SetCellPathLocked(cellR, cellC, locked);
    }

    /// <summary>Whether the cell under a world position is passable.</summary>
    int CellPassable(MCVector3D pos);

    /// <summary>
    /// Marks cell (cellRow, cellCol) impassable and spreads outward <paramref name="depth"/> cells, saving each tile
    /// first when <see cref="PreserveMapTiles"/> is set.
    /// </summary>
    void SpreadState(int32_t cellRow, int32_t cellCol, int32_t depth);
    /// <summary>Blocks the cells within <paramref name="radius"/> of a position.</summary>
    int32_t PlaceObject(MCVector3D position, float radius);
    /// <summary>Places every existing object of a list (see <see cref="PlaceObject"/>).</summary>
    int32_t PlaceObjects(MCObjectQueueNode* objectList);
    /// <summary>Does nothing in MCX.</summary>
    void PlaceTerrainObject(MCGameObject* object);
    /// <summary>Creates the terrain objects of every block and records their footprint bits in the tiles.</summary>
    void PlaceTerrainObjects(MCObjectBlockManager* blockManager);
    /// <summary>Places both mech lists with tile preservation on.</summary>
    void UpdateMovingObjects();
    /// <summary>Puts back the tiles saved by <see cref="SpreadState"/>.</summary>
    void RestorePreservedMap();

    /// <summary>The ground height under a world position, interpolated over the tile's triangle.</summary>
    float GetTerrainElevation(MCVector3D position);
    double GetTerrainElevationUnrounded(MCVector3D position);
    /// <summary>Whether the cell under a position lets line of sight through.</summary>
    int32_t GetLos(MCVector3D position);
    uint32_t GetInnerSphereMine(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC);
    uint32_t GetClanMine(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC);
    /// <summary>Whether a position is above the ground and its cell passable (line of fire).</summary>
    int32_t GetLof(MCVector3D position);
    int LineOfSight(MCVector3D start, MCVector3D target);
    int LineOfFire(MCVector3D start, MCVector3D target);
    /// <summary>Counts what blocks the sensor line between two positions.</summary>
    void LineOfSensor(MCVector3D start, MCVector3D target, int32_t& numBlockingTiles, int32_t& numBlockingObjects);

    /// <summary>Dumps a rectangle of the map to a text file.</summary>
    void Print(char* fileName, int32_t uLr, int32_t uLc, int32_t height, int32_t width);

    int InBounds(int32_t tileR, int32_t tileC);
    MCMapTile GetTile(int32_t tileR, int32_t tileC);

    /// <summary>
    /// The movement cost of cell (cellR, cellC) of a tile for <paramref name="mover"/>: 0 without an overlay, else
    /// the <see cref="OverlayWeightTable"/> entry for its move level (gates by team; 20000 when closed).
    /// </summary>
    int32_t GetOverlayWeight(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, MCMover* mover);

    std::unique_ptr<MCMapTile[]> Map;
    int32_t Height = 0;
    int32_t Width = 0;
    /// <summary>Elevation level added to every tile's 6-bit elevation.</summary>
    int32_t BaseElevation = 0;
    int32_t NumPreservedTiles = 0;
    MCPreservedTile PreservedTiles[MAX_PRESERVED_TILES]{};
    /// <summary>Per tile, how many marked paths cross it (<see cref="MCMovePath::Mark"/>).</summary>
    std::unique_ptr<uint8_t[]> PathMap;
};

/// <summary>Where an object sits in the <see cref="MCObjectMap"/>: a node of its tile row's list, sorted by column.</summary>
/// <remarks>Original: <c>struct _ObjectPosition</c> (GameObject::getObjPosition). 0x24 bytes.</remarks>
typedef struct MCObjectPosition
{
    MCGameObject* Object = nullptr;
    int32_t TileR = 0;
    int32_t TileC = 0;
    int32_t CellR = 0;
    int32_t CellC = 0;
    /// <summary>Row in map cells (tileMulMAPCELL_DIM[tileR] + cellR).</summary>
    int32_t MapCellR = 0;
    int32_t MapCellC = 0;
    MCObjectPosition* Prev = nullptr;
    MCObjectPosition* Next = nullptr;
} MCObjectPosition;

/// <summary>Which objects stand on which tile: per tile row, a list of <see cref="MCObjectPosition"/> by column.</summary>
/// <remarks>Original source: <c>ai\move.cpp</c>. 0x10 bytes.</remarks>
class MCObjectMap
{
public:
    void Init(MCScenarioMap* map);
    /// <summary>Adds an object at its current position.</summary>
    void AddObject(MCGameObject* object);
    /// <summary>Moves an object's node when it changed tile; removes it when it left the map.</summary>
    /// <returns>0 when the object was removed.</returns>
    int UpdateObject(MCGameObject* object, int force);
    void RemoveObject(MCGameObject* object);
    /// <summary>How many objects stand on a tile (reads <see cref="GameObjectMap"/>, not this).</summary>
    int32_t GetNumObjects(int32_t tileR, int32_t tileC);
    /// <summary>As <see cref="GetNumObjects"/>, without trees (object class 0x15).</summary>
    int32_t GetNumSensorBlockingObjects(int32_t tileR, int32_t tileC);
    void Destroy();

    MCScenarioMap* Map = nullptr;
    int32_t Height = 0;
    int32_t Width = 0;
    /// <summary>Per tile row, the first node of its list.</summary>
    std::unique_ptr<MCObjectPosition*[]> Rows;
};

/// <summary>
/// A mover's move path in the compact form sent over the network: up to four steps, each a tile and cell, with the
/// direction from each step to the next, and all of it packed into one 32-bit word.
/// </summary>
/// <remarks>Original source: <c>ai\move.cpp</c> (init inline in <c>ai\move.h</c>). 0x58 bytes.</remarks>
class MCMoveChunk
{
public:
    void Init()
    {
        StepPos[0][0] = -1;
        StepPos[0][1] = -1;
        NumSteps = 0;
        Run = 0;
        Data = 0;
    }

    /// <summary>Builds the chunk from the mover's current path (and the one after it).</summary>
    void Build(MCMover* mover, MCMovePath* path1, MCMovePath* path2);
    /// <summary>Builds a one-step chunk to a jump destination.</summary>
    void Build(MCMover* mover, MCVector3D jumpGoal);
    /// <summary>Packs the steps into <see cref="Data"/>.</summary>
    void Pack(MCMover* mover);
    /// <summary>Unpacks <see cref="Data"/> (sets <see cref="MoveChunkUnpackErr"/> on a bad chunk).</summary>
    void Unpack(MCMover* mover);
    int EqualTo(MCMover* mover, MCMoveChunk* chunk);

    /// <summary>Per step: tileR, tileC, cellR, cellC.</summary>
    int32_t StepPos[MOVECHUNK_NUM_STEPS][4]{};
    /// <summary>Direction (0-7) from each step to the next.</summary>
    int32_t StepRelPos[MOVECHUNK_NUM_STEPS - 1]{};
    int32_t NumSteps = 0;
    int32_t Run = 0;
    /// <summary>The packed chunk: start cell row/col, numSteps - 1, run, then the three step directions.</summary>
    uint32_t Data = 0;
};

/// <summary>One step of a <see cref="MCMovePath"/>: a cell and its world position.</summary>
/// <remarks>0x1c bytes. The original name isn't known; MechCommander 2 calls it PathStep.</remarks>
typedef struct MCPathStep
{
    int16_t TileR = 0;
    int16_t TileC = 0;
    int16_t CellR = 0;
    int16_t CellC = 0;
    /// <summary>Distance from this step to the end of the path, in meters.</summary>
    float DistanceToGoal = 0;
    MCVector3D Destination;
    /// <summary>
    /// The offset (index of <see cref="CellShift"/>) stepped from the previous step: 0-7 a neighbour, above 7 a jump.
    /// Read as a signed char; MoveChunk::build sends it as the chunk's step direction.
    /// </summary>
    uint8_t Direction = 0;
} MCPathStep;

/// <summary>A mover's cell path: up to 200 steps.</summary>
/// <remarks>Original source: <c>ai\move.cpp</c>. 0x1610 bytes.</remarks>
class MCMovePath
{
public:
    /// <summary>Sets the step counts; returns the count when it is the most seen so far, else -1.</summary>
    int32_t Init(int32_t numSteps);
    void Clear();
    void Destroy();
    /// <summary>Meters from a position through step <paramref name="fromStep"/> (-1: the current) to the goal.</summary>
    float GetDistanceLeft(MCVector3D position, int32_t fromStep = -1);
    /// <summary>Counts the path's tiles in the scenario map's path map.</summary>
    void Mark();
    void Unmark();
    /// <summary>Sets the path lock of <paramref name="range"/> steps from <paramref name="start"/> (-1: current).</summary>
    void Lock(int32_t start, int32_t range, uint32_t setting);
    int IsLocked(int32_t start, int32_t range, int* reachedEnd = nullptr);
    int IsBlocked(int32_t start, int32_t range, int* reachedEnd = nullptr);
    /// <summary>The area of the first bridge tile among the steps, or -1.</summary>
    int32_t CrossesBridge(int32_t start, int32_t range);
    /// <summary>The first step on tile (tileR, tileC), or -1.</summary>
    int32_t CrossesTile(int32_t start, int32_t range, int32_t tileR, int32_t tileC);
    /// <summary>Always -1 in MCX.</summary>
    int32_t CrossesClosedClanGate(int32_t start, int32_t range);
    /// <summary>Always -1 in MCX.</summary>
    int32_t CrossesClosedISGate(int32_t start, int32_t range);
    /// <summary>The first step on a closed gate overlay, or -1.</summary>
    int32_t CrossesClosedGate(int32_t start, int32_t range);
    /// <summary>Rebuilds the path from a received chunk.</summary>
    void SetMoveChunk(MCMoveChunk* chunk);
    /// <summary>Does nothing in MCX.</summary>
    void GetMoveChunk(MCMoveChunk* chunk, int32_t start, int32_t numSteps, int run);
    void SetDestination(int32_t stepNumber, MCVector3D position);

    /// <summary>World position of the last step.</summary>
    MCVector3D Goal;
    /// <summary>Copied from <see cref="MCMoveMap::Target"/> when the path is calculated.</summary>
    MCVector3D Target;
    int32_t NumSteps = 0;
    int32_t NumStepsWhenNotPaused = 0;
    int32_t CurStep = 0;
    int32_t Cost = 0;
    MCPathStep StepList[MAX_STEPS_PER_MOVEPATH];
    int32_t Marked = 0;
    /// <summary>The <see cref="MCGlobalPathStep"/> this path walks, -1 for none.</summary>
    int32_t GlobalStep = 0;
};

/// <summary>A queued path request of <see cref="MCMovePathManager"/>.</summary>
/// <remarks>Original: <c>struct _PathQueueRec</c>. 0x24 bytes.</remarks>
typedef struct MCPathQueueRec
{
    /// <summary>Sort key; higher is served first (ties in order of request).</summary>
    float Priority = 0;
    MCMechWarrior* Pilot = nullptr;
    int32_t SelectionIndex = 0;
    uint32_t MoveParams = 0;
    /// <summary>Passed as the last argument of MechWarrior::calcMovePath.</summary>
    int32_t InitPath = 0;
    MCPathQueueRec* Prev = nullptr;
    MCPathQueueRec* Next = nullptr;
} MCPathQueueRec;

/// <summary>Spreads path calculation over frames: pilots queue requests, a few are served per update.</summary>
/// <remarks>Original source: <c>ai\move.cpp</c>. 0x2a3c bytes.</remarks>
class MCMovePathManager
{
public:
    int32_t Init();
    void Destroy();
    void Remove(MCPathQueueRec* rec);
    /// <summary>Removes the pilot's pending request, if any.</summary>
    MCPathQueueRec* Remove(MCMechWarrior* pilot);
    /// <summary>Queues (or requeues) a path request for <paramref name="pilot"/>, sorted by priority.</summary>
    void Request(MCMechWarrior* pilot, int32_t selectionIndex, uint32_t moveParams, float priority, int32_t initPath);
    /// <summary>Serves the first request.</summary>
    void CalcPath();
    /// <summary>Serves up to five requests.</summary>
    void Update();

    MCPathQueueRec Pool[MAX_PATH_QUEUE_RECS]{};
    MCPathQueueRec* QueueFront = nullptr;
    MCPathQueueRec* QueueEnd = nullptr;
    MCPathQueueRec* FreeList = nullptr;
};

#pragma pack(push, 1)
/// <summary>One door of an area: the door and which of its two sides the area is on.</summary>
/// <remarks>Original: <c>struct _DoorInfo</c>. 3 bytes, stored packed in the global map file.</remarks>
typedef struct MCDoorInfo
{
    int16_t DoorIndex = 0;
    char DoorSide = 0;
} MCDoorInfo;
static_assert(sizeof(MCDoorInfo) == 3);

/// <summary>A link from a door side to another door of the same area, with its cost.</summary>
/// <remarks>7 bytes, stored packed in the global map file. The original name isn't known (MC2: DoorLink).</remarks>
typedef struct MCDoorLink
{
    int16_t DoorIndex = 0;
    char DoorSide = 0;
    int32_t Cost = 0;
} MCDoorLink;
static_assert(sizeof(MCDoorLink) == 7);
#pragma pack(pop)

/// <summary>An area of the <see cref="MCGlobalMap"/>: a connected region of tiles within one sector.</summary>
/// <remarks>
/// The original name isn't known (MC2: GlobalMapArea). In the original a 0x29-byte packed record, read and written
/// raw with the <c>doors</c> pointer inside; the port keeps natural layout and reads the record field by field
/// (sectorR s16, sectorC s16, doors u32 (ignored, rebuilt from doorInfos), type s32, numDoors s8, then the rest).
/// Offsets below are the file record's. The record's words at +0x11, +0x15, +0x1d, +0x21 and +0x25 are written by
/// the editor's calcAreas (-1, never set, 0, 0, 0) and read by neither the game nor the editor; the port skips them
/// on reading and writes the editor's values.
/// </remarks>
typedef struct MCGlobalMapArea
{
    int16_t SectorR = 0;
    int16_t SectorC = 0;
    /// <summary>The area's entries in GlobalMap::doorInfos.</summary>
    MCDoorInfo* Doors = nullptr;
    /// <summary>0 normal, 1 a north-south bridge, 2 an east-west bridge (road or railroad; see
    /// GlobalMap::calcBridges).</summary>
    int32_t Type = 0;
    char NumDoors = 0;
    int32_t Open = 0;
    /// <summary>Set by GlobalMap::closeArea.</summary>
    int32_t Closed = 0;
} MCGlobalMapArea;
/// <summary>Size of a <see cref="MCGlobalMapArea"/> record in the global map file.</summary>
inline constexpr int32_t GLOBALMAP_AREA_RECORD_SIZE = 0x29;

/// <summary>
/// A door of the <see cref="MCGlobalMap"/>: a run of cells joining two areas, with the A* bookkeeping of
/// <see cref="MCGlobalMap::CalcPath"/>.
/// </summary>
/// <remarks>
/// Original: <c>struct _GlobalMapDoor</c>. In the original a 0x3b-byte packed record, read and written raw with the
/// two <c>links</c> pointers inside; the port keeps natural layout and reads the record field by field (the link
/// pointers are rebuilt from doorLinks). Offsets below are the file record's.
/// </remarks>
typedef struct MCGlobalMapDoor
{
    int16_t Row = 0;
    int16_t Col = 0;
    uint8_t CellR = 0;
    uint8_t CellC = 0;
    /// <summary>Length in cells.</summary>
    char Length = 0;
    int32_t Open = 0;
    int16_t Area[2]{};
    int16_t AreaCost[2]{};
    /// <summary>Exit direction from each side's area.</summary>
    char Direction[2]{};
    /// <summary>Per side, the link count; links[side] has numLinks + 2 entries (room for the start and goal doors).</summary>
    char NumLinks[2]{};
    MCDoorLink* Links[2]{};
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
} MCGlobalMapDoor;
/// <summary>Size of a <see cref="MCGlobalMapDoor"/> record in the global map file.</summary>
inline constexpr int32_t GLOBALMAP_DOOR_RECORD_SIZE = 0x3b;

/// <summary>One step of a long-range path: an area to cross and the door to leave it by.</summary>
/// <remarks>Original: <c>struct _GlobalPathStep</c>. 0x30 bytes; the words at +0x0 and +0xc .. +0x20 were never
/// accessed and are gone.</remarks>
typedef struct MCGlobalPathStep
{
    int32_t ThruArea = 0;
    int32_t GoalDoor = 0;
    /// <summary>The cell (row, column) the leg's path ended in (Mover::calcMovePath fills it); the next leg starts
    /// from it (MechWarrior::calcMovePath).</summary>
    int32_t GoalCell[2]{};
    int32_t CostToGoal = 0;
} MCGlobalPathStep;

/// <summary>
/// The long-range movement map: the scenario split into sectors of 10x10 tiles, each into areas joined by doors,
/// searched with A* over doors before a <see cref="MCMoveMap"/> plans the cells of each leg.
/// </summary>
/// <remarks>Original source: <c>ai\move.cpp</c>. 0x58 bytes.</remarks>
class MCGlobalMap
{
public:
    /// <summary>
    /// Allocates an empty area map of newHeight x newWidth tiles (both multiples of 10); the sector grid is sized
    /// from the width alone.
    /// </summary>
    void Init(int32_t newWidth, int32_t newHeight);
    /// <summary>Reads a global map file (see <see cref="Write"/>).</summary>
    int32_t Init(MCFile* mapFile);
    /// <summary>Computes the areas, doors and links of a scenario map (height/width -1: the map's).</summary>
    /// <remarks>
    /// Editor code (MCEditor.exe, called as init(map, 0, 0, -1, -1)): the two
    /// values it took after the map only went to the file header's words 1 and 2, which nothing reads, and are gone.
    /// </remarks>
    int32_t Init(MCScenarioMap* map, int32_t height = -1, int32_t width = -1);
    /// <summary>Writes the global map file; header words 1 and 2 are written as 0, as the editor wrote them.</summary>
    int32_t Write(MCFile* mapFile);
    void Destroy();

    /// <summary>The area of a tile, -1 for none.</summary>
    int32_t CalcArea(int32_t tileR, int32_t tileC);
    /// <summary>Makes the spare last area a one-tile area at (tileR, tileC).</summary>
    int32_t SetTempArea(int32_t tileR, int32_t tileC, int32_t cost);

    int FillNorthSouthBridgeArea(MCScenarioMap* map, int32_t row, int32_t col, int32_t area);
    int FillEastWestBridgeArea(MCScenarioMap* map, int32_t row, int32_t col, int32_t area);
    int FillNorthSouthRailroadBridgeArea(MCScenarioMap* map, int32_t row, int32_t col, int32_t area);
    int FillEastWestRailroadBridgeArea(MCScenarioMap* map, int32_t row, int32_t col, int32_t area);
    /// <summary>Flood-fills an area from (row, col) within the current sector bounds.</summary>
    int FillArea(MCScenarioMap* map, int32_t row, int32_t col, int32_t area);
    void CalcSectorAreas(MCScenarioMap* map, int32_t sectorR, int32_t sectorC);
    void CalcAreas(MCScenarioMap* map);
    void CalcBridges(MCScenarioMap* map);

    void BeginDoorProcessing();
    void AddDoor(int32_t area1, int32_t area2, int32_t row, int32_t col, int32_t cellR, int32_t cellC, int32_t length,
                 int32_t direction);
    void EndDoorProcessing();
    int32_t NumAreaDoors(int32_t area);
    void GetAreaDoors(int32_t area, MCDoorInfo* doorList);
    void CalcGlobalDoors(MCScenarioMap* map);
    void CalcAreaDoors();
    /// <summary>The cell path cost from one door of an area to another (runs PathFindMap).</summary>
    int32_t CalcLinkCost(int32_t startDoor, int32_t thruArea, int32_t goalDoor);
    void CalcDoorLinks();
    /// <summary>Does nothing in MCX.</summary>
    void CalcSectorPaths(MCScenarioMap* map, int32_t sectorR, int32_t sectorC);
    void CalcPathCostTable();
    /// <summary>The direction a door leaves <paramref name="fromArea"/> by, -1 if it doesn't touch it.</summary>
    int32_t ExitDirection(int32_t doorIndex, int32_t fromArea);
    /// <summary>Copies door <paramref name="doorIndex"/> of an area.</summary>
    void GetDoorTiles(int32_t area, int32_t doorIndex, MCGlobalMapDoor* door);
    MCVector3D GetDoorWorldPos(int32_t area, int32_t door, int32_t* prevGoalCell);

    /// <summary>Adds the temporary start door (index numDoors) joining area <paramref name="startArea"/>.</summary>
    void SetStartDoor(int32_t startArea);
    void ResetStartDoor(int32_t startArea);
    /// <summary>Adds the temporary goal door (index numDoors + 1) and records the goal sector.</summary>
    void SetGoalDoor(int32_t goalArea);
    void ResetGoalDoor(int32_t goalArea);
    /// <summary>A* estimate: sector distance from a door to the goal sector.</summary>
    int32_t CalcHPrime(int32_t door);
    /// <summary>Finds the door path from one area to another.</summary>
    /// <returns>The number of steps written to <paramref name="path"/>, 0 when there is none.</returns>
    int32_t CalcPath(int32_t startArea, int32_t goalArea, MCGlobalPathStep* path);
    void PropogateCost(int32_t door, int32_t cost, int32_t fromSide, int32_t g);
    /// <summary>As <see cref="calcPath(int32_t, int32_t, GlobalPathStep*)"/>, between the areas of two positions.</summary>
    int32_t CalcPath(MCVector3D start, MCVector3D goal, MCGlobalPathStep* path);
    /// <summary>The number of steps between two areas, from the path cost table.</summary>
    int32_t GetPathCost(int32_t startArea, int32_t goalArea);
    void OpenDoor(int32_t door);
    void CloseDoor(int32_t door);
    /// <summary>Closes an area and its doors and clears its path cost entries.</summary>
    void CloseArea(int32_t area);
    void Print(char* fileName, int32_t uLr, int32_t uLc, int32_t height, int32_t width);

    /// <summary>Tile bounds of the sector being filled.</summary>
    static int32_t MinTileR;
    static int32_t MaxTileR;
    static int32_t MinTileC;
    static int32_t MaxTileC;

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
    uint8_t* SmallAreaMap = nullptr;
    /// <summary>Area per tile (-1 = none).</summary>
    int16_t* AreaMap = nullptr;
    MCGlobalMapArea* Areas = nullptr;
    /// <summary>numDoors + 2 doors (the last two are the temporary start and goal doors).</summary>
    MCGlobalMapDoor* Doors = nullptr;
    MCDoorInfo* DoorInfos = nullptr;
    MCDoorLink* DoorLinks = nullptr;
    /// <summary>Doors collected while computing the map (MAX_BUILD_DOORS).</summary>
    MCGlobalMapDoor* DoorBuildList = nullptr;
    /// <summary>numAreas x numAreas steps between areas (0 = no path, 0xff = 255 or more).</summary>
    uint8_t* PathCostTable = nullptr;
    int32_t GoalSectorR = 0;
    int32_t GoalSectorC = 0;
    /// <summary>
    /// Owns the map's arrays and door lists. A computed map's areas and doors have their own door and link lists; a
    /// loaded one's point into doorInfos and doorLinks.
    /// </summary>
    MCBlockStore Blocks;
};

/// <summary>One cell of a <see cref="MCMoveMap"/>.</summary>
/// <remarks>0x18 bytes. The original name isn't known (MC2: MoveMapNode).</remarks>
typedef struct MCMoveMapNode
{
    int32_t Cost = 0;
    int32_t Parent = 0;
    /// <summary>MOVEFLAG bits: 1 on the open list, 2 closed, 4 on the found path, 8 goal, 0x10 a mover stands
    /// here.</summary>
    uint32_t Flags = 0;
    int32_t G = 0;
    int32_t HPrime = 0;
    int32_t FPrime = 0;
} MCMoveMapNode;

/// <summary>The short-range cell path finder: an A* over a window of the scenario map's cells.</summary>
/// <remarks>Original source: <c>ai\move.cpp</c>. 0x88 bytes.</remarks>
class MCMoveMap
{
public:
    /// <summary>Allocates the cells for a window of up to newMaxHeight x newMaxWidth tiles.</summary>
    void Init(int32_t newMaxWidth, int32_t newMaxHeight);
    /// <summary>Reads a MoveMap FIT file (Height, Width, VertexCost).</summary>
    int32_t Init(MCFitIniFile* mapFile);
    void Clear();
    /// <summary>Adds the cost of the standing mechs (other than the mover) to their cells.</summary>
    void PlaceMovers(int markGoal);
    void SetTarget(MCVector3D targetPos);
    /// <summary>Sets the start: a world position (startR -1: computed from it) or a cell.</summary>
    void SetStart(MCVector3D* startPos, int32_t startR = -1, int32_t startC = -1);
    /// <summary>Sets the goal: a world position (goalR -1: computed from it) or a cell.</summary>
    void SetGoal(MCVector3D goalPos, int32_t goalR = -1, int32_t goalC = -1);
    /// <summary>Sets the goal to a door of <see cref="GlobalMoveMap"/>, from area <paramref name="thruArea"/>.</summary>
    void SetGoal(int32_t thruArea, int32_t goalDoor);

    /// <summary>Loads the cell costs of a window of the map and sets start and goal (a position).</summary>
    int32_t SetUp(MCScenarioMap* map, int32_t uLr, int32_t uLc, int32_t height, int32_t width, MCVector3D* startPos,
                  int32_t startR, int32_t startC, MCVector3D goalPos, int32_t goalR, int32_t goalC,
                  int32_t* overlayWeightTable, int32_t moveLevel, int32_t jumpCost, int32_t numOffsets,
                  uint32_t params);
    /// <summary>As the other setUp, with the goal a door (thruArea, goalDoor) of <see cref="GlobalMoveMap"/>.</summary>
    int32_t SetUp(MCScenarioMap* map, int32_t uLr, int32_t uLc, int32_t height, int32_t width, MCVector3D* startPos,
                  int32_t startR, int32_t startC, int32_t thruArea, int32_t goalDoor, MCVector3D targetPos,
                  int32_t* overlayWeightTable, int32_t moveLevel, int32_t jumpCost, int32_t numOffsets,
                  uint32_t params);
    int32_t MarkEscapeGoalCells(MCVector3D goalPos);
    int32_t MarkGoalCells(MCVector3D goalPos);
    /// <summary>
    /// Runs the search from the start cell to the nearest goal cell and fills <paramref name="path"/>; the goal
    /// cell reached (its map cell, when the goal is a door) goes to <paramref name="goalCell"/>.
    /// </summary>
    /// <returns>The path's step count, 0 when no path was found.</returns>
    int32_t CalcPath(MCMovePath* path, MCVector3D* goalWorldPos, int32_t* goalCell);
    /// <summary>As <see cref="CalcPath"/>, with a flat estimate (every cell 10 from the goal cells
    /// markEscapeGoalCells set).</summary>
    int32_t CalcEscapePath(MCMovePath* path, MCVector3D* goalWorldPos, int32_t* goalCell);
    /// <summary>Writes the window's costs and the path to a debug file.</summary>
    void WriteDebug(MCFile* debugFile);
    void Destroy();

protected:
    int AdjacentCellOpen(int32_t r, int32_t c, int32_t dir);
    void PropogateCost(int32_t r, int32_t c, int32_t cost, int32_t g);
    /// <summary>
    /// The A* search and path building calcPath and calcEscapePath share (MCX has two copies that differ only in
    /// the estimate and their messages).
    /// </summary>
    /// <remarks>The port's; not in MCX.EXE.</remarks>
    int32_t SearchPath(MCMovePath* path, MCVector3D* goalWorldPos, int32_t* goalCell, bool escape);

public:
    /// <summary>Upper-left tile of the window.</summary>
    int32_t ULr = 0;
    int32_t ULc = 0;
    /// <summary>Upper-left cell of the window (ULr * 3, ULc * 3).</summary>
    int32_t MinRow = 0;
    int32_t MinCol = 0;
    /// <remarks>init(maxHeight, maxWidth) stores its first argument here; the maps are square, so which side is
    /// which isn't certain.</remarks>
    int32_t MaxWidth = 0;
    int32_t MaxHeight = 0;
    /// <summary>Row stride of <see cref="Map"/>, in cells.</summary>
    int32_t MaxCellWidth = 0;
    int32_t MaxCellHeight = 0;
    int32_t Width = 0;
    int32_t Height = 0;
    int32_t CellWidth = 0;
    int32_t CellHeight = 0;
    std::unique_ptr<MCMoveMapNode[]> Map;
    MCVector3D StartPos;
    int32_t StartR = 0;
    int32_t StartC = 0;
    MCVector3D GoalPos;
    int32_t GoalR = 0;
    int32_t GoalC = 0;
    /// <summary>The goal door, when the goal is a door (<see cref="GoalIsDoor"/>).</summary>
    int32_t Door = 0;
    int32_t DoorSide = 0;
    /// <summary>Direction the goal door is entered from, -1 when the goal isn't a door.</summary>
    int32_t DoorDirection = 0;
    MCVector3D Target;
    /// <summary>
    /// The cost of a plain passable cell (setUp's moveLevel); overlays, locks and mines add to it.
    /// </summary>
    int32_t MoveLevel = 0;
    /// <summary>Added to the cost of the jump offsets (the ones past the eight neighbours); with
    /// <see cref="JumpOnBlocked"/> it is their whole cost.</summary>
    int32_t JumpCost = 0;
    /// <summary>How many of the NUM_CELL_OFFSETS offsets the search tries.</summary>
    int32_t NumOffsets = 0;
    float CalcTime = 0;
    int32_t* OverlayWeights = nullptr;
};

void WorldCoordToMapCoord(MCVector3D pos, int32_t& tileR, int32_t& tileC, int32_t& cellR, int32_t& cellC);
void WorldCoordToMapTile(MCVector3D pos, int32_t& tileR, int32_t& tileC);
void WorldCoordToMapCell(MCVector3D pos, int32_t& cellR, int32_t& cellC);
/// <summary>
/// The point <paramref name="distance"/> meters from <paramref name="pos"/> at <paramref name="angle"/> degrees,
/// pulled back along the line to the last (or first, per <paramref name="flags"/> bit 2) passable cell.
/// </summary>
MCVector3D RelativePositionToPoint(MCVector3D pos, float angle, float distance, uint32_t flags);
void MapTileCellToWorldPos(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, MCVector3D& worldPos);
void MapCellToWorldPos(int32_t cellR, int32_t cellC, MCVector3D& worldPos);
/// <summary>Dumps <see cref="OpenList"/> to openlist.dbg.</summary>
void DebugOpenList(char* msg);
/// <summary>The terrain type of a tile texture index (bands of 79).</summary>
int32_t CalcTileTypeFromIndex(int32_t tileIndex);
int32_t CalcOverlayTypeFromIndex(int32_t overlayIndex);
/// <summary>Writes one or two chunks and the mover's state to mvchunk.dbg.</summary>
void DebugMoveChunk(MCMover* mover, MCMoveChunk* chunk1, MCMoveChunk* chunk2);
/// <summary>Whether a tile's overlay blocks left-right crossing.</summary>
int IsLRBlocked(MCMapTile* tile);
/// <summary>The cell direction (0-7) an object faces.</summary>
int32_t CellFacing(MCGameObject* object);
/// <summary>The direction from one tile/cell to another adjacent one (uses a static deltaDir[][3] table).</summary>
/// <remarks>Only inlined copies exist in MCX.EXE (in MoveChunk::build).</remarks>
int32_t CellDirToCell(int32_t fromTileR, int32_t fromTileC, int32_t fromCellR, int32_t fromCellC, int32_t toTileR,
                      int32_t toTileC, int32_t toCellR, int32_t toCellC);

// Globals of ai\move.cpp. (Several are listed under their heaviest users in globals_by_file.md, but sit in
// move.cpp's data between its own globals.)

extern int BlockWallTiles;
extern int32_t SimpleMovePathRange;
/// <summary>Row / column offsets of the eight neighbours, for spreadState.</summary>
extern char RowShift[8];
extern char ColShift[8];
/// <summary>Row and column offset of each search offset.</summary>
extern int32_t CellShift[NUM_CELL_OFFSETS * 2];
extern char ReverseShift[NUM_CELL_OFFSETS];
extern int IsDiagonalStep[NUM_CELL_OFFSETS];
extern int32_t StepAdjDir[9];
extern int32_t AdjTile[4][2];
/// <summary>Per mine layout, the mine type of each of the nine cells.</summary>
extern char MineLayout[4][9];
extern int32_t MaxHPrime;
extern char OverlayIsBridge[NUM_OVERLAY_TYPES];
extern char OverlayIsClosedGate[NUM_OVERLAY_TYPES];
extern char OverlayIsDirtRoad[NUM_OVERLAY_TYPES];
/// <summary>Per cell (cellR * 3 + cellC) and direction: tile row/col delta and the neighbour's cell row/col.</summary>
extern int32_t AdjCellTable[MAPCELL_DIM * MAPCELL_DIM][8][4];
extern float CellColToWorldCoord[MAX_MAP_CELL_WIDTH];
extern float CellToWorldCoord[MAPCELL_DIM];
extern float TileColWorldCoords[MAX_MAP_TILE_WIDTH];
extern float CellShiftDistance[NUM_CELL_OFFSETS];
extern float TileRowWorldCoords[MAX_MAP_TILE_WIDTH];
/// <summary>Start of each overlay type's 3x3 block within a level of <see cref="OverlayWeightTable"/>.</summary>
extern int32_t OverlayWeightIndex[NUM_OVERLAY_TYPES];
extern float CellRowToWorldCoord[MAX_MAP_CELL_WIDTH];
/// <summary>Cell costs per move level and overlay (the scenario's OverlayCellCosts).</summary>
extern int32_t OverlayWeightTable[NUM_MOVE_LEVELS * OVERLAY_WEIGHT_LEVEL_SIZE];
/// <summary>tile * MAPCELL_DIM.</summary>
extern int32_t TileMulMapcellDim[MAX_MAP_TILE_WIDTH];
extern int32_t MoveChunkUnpackErr;
extern int ClearBridgeTiles;
/// <summary>The object whose path is being calculated (it doesn't block itself).</summary>
extern MCGameObject* MovingObject;
extern MCGameObject* RamObject;
/// <summary>The A* open list shared by the path finders.</summary>
extern MCPriorityQueue* OpenList;
extern int JumpOnBlocked;
extern int FindingEscapePath;
extern MCScenarioMap* GameMap;
extern MCGlobalMap* GlobalMoveMap;
/// <summary>The cell path finder the movers share. Unnamed in MCX.EXE; the name is the one its Fatal messages
/// use.</summary>
extern MCMoveMap* PathFindMap;
extern MCObjectMap* GameObjectMap;
extern MCMovePathManager* PathManager;
extern int32_t CurPlanet;
extern int32_t DebugMovePathType;
extern int32_t NumPathsInQueue;
extern float MapCellDiagonal;
extern float HalfMapCell;
extern float VerticesMapSideDivTwo;
extern float MetersMapSideDivTwo;
extern int GoalIsDoor;
extern int PreserveMapTiles;
extern float MetersPerCell;
