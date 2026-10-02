#pragma once

#include "lib/cvmath.h"

class File;
class FitIniFile;
class PriorityQueue;
class GameObject;
class Mover;
class MechWarrior;
class Scenario;
class ObjectBlockManager;
class MovePath;
struct ObjectQueueNode;

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
/// <summary>Tiles whose original state <see cref="ScenarioMap::spreadState"/> can preserve while movers are placed.</summary>
inline constexpr int32_t MAX_PRESERVED_TILES = 300;
/// <summary>Steps of a <see cref="MovePath"/>.</summary>
inline constexpr int32_t MAX_STEPS_PER_MOVEPATH = 200;
/// <summary>Steps of a long-range (door to door) path (the "Too Many Long Range Move Steps" assert).</summary>
inline constexpr int32_t MAX_GLOBAL_PATH = 80;
/// <summary>Path requests <see cref="MovePathManager"/> can queue.</summary>
inline constexpr int32_t MAX_PATH_QUEUE_RECS = 300;
/// <summary>Steps a <see cref="MoveChunk"/> carries (a net update of a mover's path).</summary>
inline constexpr int32_t MOVECHUNK_NUM_STEPS = 4;
/// <summary>Doors the door build list holds while a <see cref="GlobalMap"/> is computed (0x48058 bytes / 0x3b).</summary>
inline constexpr int32_t MAX_BUILD_DOORS = 5000;
/// <summary>Version stamp at the start of a global map file.</summary>
inline constexpr int32_t GLOBALMAP_VERSION = 0x22569;

/// <summary>
/// One terrain tile of the <see cref="ScenarioMap"/>: two bit-packed words, stored raw in the map file.
/// </summary>
/// <remarks>
/// Original header: <c>ai\move.h</c>. 8 bytes (read and written raw, so the layout is fixed).
/// <para><c>cells</c>: bits 0-6 terrain tile type, bits 7-12 elevation level (added to
/// <see cref="ScenarioMap::baseElevation"/>), bit 13 "preserved" (see <see cref="ScenarioMap::spreadState"/>),
/// then two bits per cell c = cellR * 3 + cellC: bit 14 + 2c passable, bit 15 + 2c line of sight.</para>
/// <para><c>overlay</c>: bits 0-6 overlay type (index of <see cref="OverlayIsBridge"/> and friends), bits 7-8 set
/// from the terrain object blocks, bits 11-12 Inner Sphere mine layout, bits 13-14 Clan mine layout (rows of
/// <see cref="mineLayout"/>), bit 15 + c path locked.</para>
/// </remarks>
typedef struct _MapTile
{
    uint32_t cells;   // +0x0
    uint32_t overlay; // +0x4

    /// <summary>Whether cell (cellR, cellC) can be entered.</summary>
    /// <remarks>MCX.EXE @ 0x00686910</remarks>
    uint32_t getCellPassable(int32_t cellR, int32_t cellC)
    {
        const uint32_t shift = static_cast<uint32_t>((cellR * MAPCELL_DIM + cellC) * 2);
        return (cells & (0x4000u << shift)) >> (shift + 14);
    }

    /// <summary>Whether a mover's path has locked cell (cellR, cellC).</summary>
    /// <remarks>MCX.EXE @ 0x0068a100</remarks>
    uint32_t getCellPathLocked(int32_t cellR, int32_t cellC)
    {
        const uint32_t shift = static_cast<uint32_t>(cellR * MAPCELL_DIM + cellC);
        return (overlay & (0x8000u << shift)) >> (shift + 15);
    }

    /// <summary>Sets or clears the path lock of cell (cellR, cellC).</summary>
    /// <remarks>MCX.EXE @ 0x0068ab30</remarks>
    void setCellPathLocked(int32_t cellR, int32_t cellC, uint32_t locked)
    {
        const uint32_t shift = static_cast<uint32_t>(cellR * MAPCELL_DIM + cellC);
        overlay = (locked << (shift + 15)) | (~(0x8000u << shift) & overlay);
    }
} MapTile;
static_assert(sizeof(MapTile) == 8, "MapTile is read raw from the map file");

/// <summary>A tile whose word <see cref="ScenarioMap::spreadState"/> saved, to be put back by restorePreservedMap.</summary>
/// <remarks>8 bytes. The name is the port's (the original's isn't known).</remarks>
struct PreservedTile
{
    int16_t row;    // +0x0
    int16_t col;    // +0x2
    uint32_t cells; // +0x4
};

/// <summary>
/// The movement map of the whole scenario: one <see cref="MapTile"/> per terrain tile, plus a per-tile count of the
/// paths crossing it.
/// </summary>
/// <remarks>Original source: <c>ai\move.cpp</c> (inline methods in <c>ai\move.h</c>). 0x978 bytes.</remarks>
class ScenarioMap
{
public:
    /// <remarks>MCX.EXE @ 0x006b9660</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x006b9680</remarks>
    static void operator delete(void* ptr);

    /// <summary>
    /// Allocates an empty map of newHeight x newWidth tiles and fills the tile coordinate tables (not the cell ones).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b96a0</remarks>
    void init(int32_t newWidth, int32_t newHeight);
    /// <summary>Reads the map (height, width, base elevation, then the tiles) and fills the coordinate tables.</summary>
    /// <remarks>MCX.EXE @ 0x006b9840</remarks>
    int32_t init(File* mapFile);
    /// <summary>Does nothing (returns 0).</summary>
    /// <remarks>MCX.EXE @ 0x006ba060</remarks>
    int32_t init(Scenario* scenario);
    /// <summary>Writes the map in the format <see cref="init(File*)"/> reads.</summary>
    /// <remarks>MCX.EXE @ 0x006b9a80</remarks>
    int32_t write(File* mapFile);
    /// <summary>Frees the tiles and the path map.</summary>
    /// <remarks>MCX.EXE @ 0x006bb3f0</remarks>
    void destroy();

    /// <summary>The tile and cell of a world position.</summary>
    /// <remarks>MCX.EXE @ 0x0064fd80 (inline in the original's move.h)</remarks>
    void worldToMapPos(vector_3d pos, int32_t& tileR, int32_t& tileC, int32_t& cellR, int32_t& cellC);
    /// <summary>The tile of a world position.</summary>
    /// <remarks>MCX.EXE @ 0x006515e0 (inline in the original's move.h)</remarks>
    void worldToMapTilePos(vector_3d pos, int32_t& tileR, int32_t& tileC);
    /// <summary>
    /// Whether tile (tileR, tileC) is on the map (port helper). The original indexes <see cref="map"/> with the tile of
    /// any world point; the port checks the ones that can fall off the map (a scattered shot, a point walked out from
    /// a unit, a player's waypoint).
    /// </summary>
    bool onMap(int32_t tileR, int32_t tileC) const
    {
        return tileR >= 0 && tileR < height && tileC >= 0 && tileC < width;
    }

    /// <remarks>MCX.EXE @ 0x0068a130 (inline in the original's move.h)</remarks>
    int getCellPathLocked(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC)
    {
        return static_cast<int>(map[width * tileR + tileC].getCellPathLocked(cellR, cellC));
    }

    /// <remarks>MCX.EXE @ 0x0068ab70 (inline in the original's move.h)</remarks>
    void setCellPathLocked(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, uint32_t locked)
    {
        map[width * tileR + tileC].setCellPathLocked(cellR, cellC, locked);
    }

    /// <summary>Whether the cell under a world position is passable.</summary>
    /// <remarks>MCX.EXE @ 0x006b9ad0</remarks>
    int cellPassable(vector_3d pos);

    /// <summary>
    /// Marks cell (cellRow, cellCol) impassable and spreads outward <paramref name="depth"/> cells, saving each tile
    /// first when <see cref="PreserveMapTiles"/> is set.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006ba070</remarks>
    void spreadState(int32_t cellRow, int32_t cellCol, int32_t depth);
    /// <summary>Blocks the cells within <paramref name="radius"/> of a position.</summary>
    /// <remarks>MCX.EXE @ 0x006ba190</remarks>
    int32_t placeObject(vector_3d position, float radius);
    /// <summary>Places every existing object of a list (see <see cref="placeObject"/>).</summary>
    /// <remarks>MCX.EXE @ 0x006ba210</remarks>
    int32_t placeObjects(ObjectQueueNode* objectList);
    /// <summary>Does nothing in MCX.</summary>
    /// <remarks>MCX.EXE @ 0x006ba290</remarks>
    void placeTerrainObject(GameObject* object);
    /// <summary>Creates the terrain objects of every block and records their footprint bits in the tiles.</summary>
    /// <remarks>MCX.EXE @ 0x006ba2a0</remarks>
    void placeTerrainObjects(ObjectBlockManager* blockManager);
    /// <summary>Places both mech lists with tile preservation on.</summary>
    /// <remarks>MCX.EXE @ 0x006ba4c0</remarks>
    void updateMovingObjects();
    /// <summary>Puts back the tiles saved by <see cref="spreadState"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006ba500</remarks>
    void restorePreservedMap();

    /// <summary>The ground height under a world position, interpolated over the tile's triangle.</summary>
    /// <remarks>MCX.EXE @ 0x006ba540</remarks>
    float getTerrainElevation(vector_3d position);
    double getTerrainElevationUnrounded(vector_3d position);
    /// <summary>Whether the cell under a position lets line of sight through.</summary>
    /// <remarks>MCX.EXE @ 0x006baa80</remarks>
    int32_t getLOS(vector_3d position);
    /// <remarks>MCX.EXE @ 0x006baaf0</remarks>
    uint32_t getInnerSphereMine(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC);
    /// <remarks>MCX.EXE @ 0x006bab30</remarks>
    uint32_t getClanMine(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC);
    /// <summary>Whether a position is above the ground and its cell passable (line of fire).</summary>
    /// <remarks>MCX.EXE @ 0x006bab70</remarks>
    int32_t getLOF(vector_3d position);
    /// <remarks>MCX.EXE @ 0x006bac10</remarks>
    int lineOfSight(vector_3d start, vector_3d target);
    /// <remarks>MCX.EXE @ 0x006bae00</remarks>
    int lineOfFire(vector_3d start, vector_3d target);
    /// <summary>Counts what blocks the sensor line between two positions.</summary>
    /// <remarks>MCX.EXE @ 0x006baf30</remarks>
    void lineOfSensor(vector_3d start, vector_3d target, int32_t& numBlockingTiles, int32_t& numBlockingObjects);

    /// <summary>Dumps a rectangle of the map to a text file.</summary>
    /// <remarks>MCX.EXE @ 0x006bb290</remarks>
    void print(char* fileName, int32_t ULr, int32_t ULc, int32_t height, int32_t width);

    /// <remarks>MCX.EXE @ 0x006c3920</remarks>
    int inBounds(int32_t tileR, int32_t tileC);
    /// <remarks>MCX.EXE @ 0x006c3950</remarks>
    MapTile getTile(int32_t tileR, int32_t tileC);

    /// <summary>
    /// The movement cost of cell (cellR, cellC) of a tile for <paramref name="mover"/>: 0 without an overlay, else
    /// the <see cref="OverlayWeightTable"/> entry for its move level (gates by team; 20000 when closed).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006bb1d0. Its name wasn't kept; this one is the port's.</remarks>
    int32_t getOverlayWeight(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, Mover* mover);

    MapTile* map;   // +0x0
    int32_t height; // +0x4
    int32_t width;  // +0x8
    /// <summary>Elevation level added to every tile's 6-bit elevation.</summary>
    int32_t baseElevation;                             // +0xc
    int32_t numPreservedTiles;                         // +0x10
    PreservedTile preservedTiles[MAX_PRESERVED_TILES]; // +0x14
    /// <summary>Per tile, how many marked paths cross it (<see cref="MovePath::mark"/>).</summary>
    uint8_t* pathMap; // +0x974
};

/// <summary>Where an object sits in the <see cref="ObjectMap"/>: a node of its tile row's list, sorted by column.</summary>
/// <remarks>Original: <c>struct _ObjectPosition</c> (GameObject::getObjPosition). 0x24 bytes.</remarks>
typedef struct _ObjectPosition
{
    GameObject* object; // +0x0
    int32_t tileR;      // +0x4
    int32_t tileC;      // +0x8
    int32_t cellR;      // +0xc
    int32_t cellC;      // +0x10
    /// <summary>Row in map cells (tileMulMAPCELL_DIM[tileR] + cellR).</summary>
    int32_t mapCellR;      // +0x14
    int32_t mapCellC;      // +0x18
    _ObjectPosition* prev; // +0x1c
    _ObjectPosition* next; // +0x20
} ObjectPosition;

/// <summary>Which objects stand on which tile: per tile row, a list of <see cref="ObjectPosition"/> by column.</summary>
/// <remarks>Original source: <c>ai\move.cpp</c>. 0x10 bytes.</remarks>
class ObjectMap
{
public:
    /// <remarks>MCX.EXE @ 0x006bb430</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x006bb450</remarks>
    static void operator delete(void* ptr);

    /// <remarks>MCX.EXE @ 0x006bb470</remarks>
    void init(ScenarioMap* map);
    /// <summary>Adds an object at its current position.</summary>
    /// <remarks>MCX.EXE @ 0x006bb4d0</remarks>
    void addObject(GameObject* object);
    /// <summary>Moves an object's node when it changed tile; removes it when it left the map.</summary>
    /// <returns>0 when the object was removed.</returns>
    /// <remarks>MCX.EXE @ 0x006bb5f0</remarks>
    int updateObject(GameObject* object, int force);
    /// <remarks>MCX.EXE @ 0x006bb900</remarks>
    void removeObject(GameObject* object);
    /// <summary>How many objects stand on a tile (reads <see cref="GameObjectMap"/>, not this).</summary>
    /// <remarks>MCX.EXE @ 0x006bb960</remarks>
    int32_t getNumObjects(int32_t tileR, int32_t tileC);
    /// <summary>As <see cref="getNumObjects"/>, without trees (object class 0x15).</summary>
    /// <remarks>MCX.EXE @ 0x006bb9b0</remarks>
    int32_t getNumSensorBlockingObjects(int32_t tileR, int32_t tileC);
    /// <remarks>MCX.EXE @ 0x006bba50</remarks>
    void destroy();

    ScenarioMap* map; // +0x0
    int32_t height;   // +0x4
    int32_t width;    // +0x8
    /// <summary>Per tile row, the first node of its list.</summary>
    ObjectPosition** rows; // +0xc
};

/// <summary>
/// A mover's move path in the compact form sent over the network: up to four steps, each a tile and cell, with the
/// direction from each step to the next, and all of it packed into one 32-bit word.
/// </summary>
/// <remarks>Original source: <c>ai\move.cpp</c> (init inline in <c>ai\move.h</c>). 0x58 bytes.</remarks>
class MoveChunk
{
public:
    /// <remarks>MCX.EXE @ 0x006bc060</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x006bc090</remarks>
    static void operator delete(void* ptr);

    /// <remarks>MCX.EXE @ 0x0065b5b0 (inline in the original's move.h)</remarks>
    void init()
    {
        stepPos[0][0] = -1;
        stepPos[0][1] = -1;
        numSteps = 0;
        run = 0;
        data = 0;
    }

    /// <summary>Builds the chunk from the mover's current path (and the one after it).</summary>
    /// <remarks>MCX.EXE @ 0x006bc0c0</remarks>
    void build(Mover* mover, MovePath* path1, MovePath* path2);
    /// <summary>Builds a one-step chunk to a jump destination.</summary>
    /// <remarks>MCX.EXE @ 0x006bc5f0</remarks>
    void build(Mover* mover, vector_3d jumpGoal);
    /// <summary>Packs the steps into <see cref="data"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006bc660</remarks>
    void pack(Mover* mover);
    /// <summary>Unpacks <see cref="data"/> (sets <see cref="MoveChunkUnpackErr"/> on a bad chunk).</summary>
    /// <remarks>MCX.EXE @ 0x006bc710</remarks>
    void unpack(Mover* mover);
    /// <remarks>MCX.EXE @ 0x006bc8d0</remarks>
    int equalTo(Mover* mover, MoveChunk* chunk);

    /// <summary>Per step: tileR, tileC, cellR, cellC.</summary>
    int32_t stepPos[MOVECHUNK_NUM_STEPS][4]; // +0x0
    /// <summary>Direction (0-7) from each step to the next.</summary>
    int32_t stepRelPos[MOVECHUNK_NUM_STEPS - 1]; // +0x40
    int32_t numSteps;                            // +0x4c
    int32_t run;                                 // +0x50
    /// <summary>The packed chunk: start cell row/col, numSteps - 1, run, then the three step directions.</summary>
    uint32_t data; // +0x54
};

/// <summary>One step of a <see cref="MovePath"/>: a cell and its world position.</summary>
/// <remarks>0x1c bytes. The original name isn't known; MechCommander 2 calls it PathStep.</remarks>
typedef struct _PathStep
{
    int16_t tileR; // +0x0
    int16_t tileC; // +0x2
    int16_t cellR; // +0x4
    int16_t cellC; // +0x6
    /// <summary>Distance from this step to the end of the path, in meters.</summary>
    float distanceToGoal;  // +0x8
    vector_3d destination; // +0xc
    /// <summary>
    /// The offset (index of <see cref="cellShift"/>) stepped from the previous step: 0-7 a neighbour, above 7 a jump.
    /// Read as a signed char; MoveChunk::build sends it as the chunk's step direction.
    /// </summary>
    uint8_t direction; // +0x18
} PathStep;

/// <summary>A mover's cell path: up to 200 steps.</summary>
/// <remarks>Original source: <c>ai\move.cpp</c>. 0x1610 bytes.</remarks>
class MovePath
{
public:
    /// <remarks>MCX.EXE @ 0x006bc9b0</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x006bc9e0</remarks>
    static void operator delete(void* ptr);

    /// <summary>Sets the step counts; returns the count when it is the most seen so far, else -1.</summary>
    /// <remarks>MCX.EXE @ 0x006bca10</remarks>
    int32_t init(int32_t numSteps);
    /// <remarks>MCX.EXE @ 0x006bca40</remarks>
    void clear();
    /// <remarks>MCX.EXE @ 0x006bca80</remarks>
    void destroy();
    /// <summary>Meters from a position through step <paramref name="fromStep"/> (-1: the current) to the goal.</summary>
    /// <remarks>MCX.EXE @ 0x006bca90</remarks>
    float getDistanceLeft(vector_3d position, int32_t fromStep = -1);
    /// <summary>Counts the path's tiles in the scenario map's path map.</summary>
    /// <remarks>MCX.EXE @ 0x006bcaf0</remarks>
    void mark();
    /// <remarks>MCX.EXE @ 0x006bcb70</remarks>
    void unmark();
    /// <summary>Sets the path lock of <paramref name="range"/> steps from <paramref name="start"/> (-1: current).</summary>
    /// <remarks>MCX.EXE @ 0x006bcbf0</remarks>
    void lock(int32_t start, int32_t range, uint32_t setting);
    /// <remarks>MCX.EXE @ 0x006bcc90</remarks>
    int isLocked(int32_t start, int32_t range, int* reachedEnd = nullptr);
    /// <remarks>MCX.EXE @ 0x006bcd40</remarks>
    int isBlocked(int32_t start, int32_t range, int* reachedEnd = nullptr);
    /// <summary>The area of the first bridge tile among the steps, or -1.</summary>
    /// <remarks>MCX.EXE @ 0x006bce00</remarks>
    int32_t crossesBridge(int32_t start, int32_t range);
    /// <summary>The first step on tile (tileR, tileC), or -1.</summary>
    /// <remarks>MCX.EXE @ 0x006bcee0</remarks>
    int32_t crossesTile(int32_t start, int32_t range, int32_t tileR, int32_t tileC);
    /// <summary>Always -1 in MCX.</summary>
    /// <remarks>MCX.EXE @ 0x006bcf40</remarks>
    int32_t crossesClosedClanGate(int32_t start, int32_t range);
    /// <summary>Always -1 in MCX.</summary>
    /// <remarks>MCX.EXE @ 0x006bcf50</remarks>
    int32_t crossesClosedISGate(int32_t start, int32_t range);
    /// <summary>The first step on a closed gate overlay, or -1.</summary>
    /// <remarks>MCX.EXE @ 0x006bcf60. Its name wasn't kept; this one is the port's.</remarks>
    int32_t crossesClosedGate(int32_t start, int32_t range);
    /// <summary>Rebuilds the path from a received chunk.</summary>
    /// <remarks>MCX.EXE @ 0x006bcfd0</remarks>
    void setMoveChunk(MoveChunk* chunk);
    /// <summary>Does nothing in MCX.</summary>
    /// <remarks>MCX.EXE @ 0x006bd0d0</remarks>
    void getMoveChunk(MoveChunk* chunk, int32_t start, int32_t numSteps, int run);
    /// <remarks>MCX.EXE @ 0x006c39b0</remarks>
    void setDestination(int32_t stepNumber, vector_3d position);

    /// <summary>World position of the last step.</summary>
    vector_3d goal; // +0x0
    /// <summary>Copied from <see cref="MoveMap::target"/> when the path is calculated.</summary>
    vector_3d target;                          // +0xc
    int32_t numSteps;                          // +0x18
    int32_t numStepsWhenNotPaused;             // +0x1c
    int32_t curStep;                           // +0x20
    int32_t cost;                              // +0x24
    PathStep stepList[MAX_STEPS_PER_MOVEPATH]; // +0x28
    int32_t marked;                            // +0x1608
    /// <summary>The <see cref="GlobalPathStep"/> this path walks, -1 for none.</summary>
    int32_t globalStep; // +0x160c
};

/// <summary>A queued path request of <see cref="MovePathManager"/>.</summary>
/// <remarks>Original: <c>struct _PathQueueRec</c>. 0x24 bytes.</remarks>
typedef struct _PathQueueRec
{
    /// <summary>Sort key; higher is served first (ties in order of request).</summary>
    float priority;         // +0x0
    MechWarrior* pilot;     // +0x4
    int32_t selectionIndex; // +0x8
    uint32_t moveParams;    // +0xc
    int32_t unknown10;      // +0x10
    int32_t unknown14;      // +0x14
    /// <summary>Passed as the last argument of MechWarrior::calcMovePath.</summary>
    int32_t initPath;    // +0x18
    _PathQueueRec* prev; // +0x1c
    _PathQueueRec* next; // +0x20
} PathQueueRec;

/// <summary>Spreads path calculation over frames: pilots queue requests, a few are served per update.</summary>
/// <remarks>Original source: <c>ai\move.cpp</c>. 0x2a3c bytes.</remarks>
class MovePathManager
{
public:
    /// <remarks>MCX.EXE @ 0x006bd0e0</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x006bd100</remarks>
    static void operator delete(void* ptr);

    /// <remarks>MCX.EXE @ 0x006bd120</remarks>
    int32_t init();
    /// <remarks>MCX.EXE @ 0x006bd180</remarks>
    void destroy();
    /// <remarks>MCX.EXE @ 0x006bd190</remarks>
    void remove(PathQueueRec* rec);
    /// <summary>Removes the pilot's pending request, if any.</summary>
    /// <remarks>MCX.EXE @ 0x006bd1f0</remarks>
    PathQueueRec* remove(MechWarrior* pilot);
    /// <summary>Queues (or requeues) a path request for <paramref name="pilot"/>, sorted by priority.</summary>
    /// <remarks>MCX.EXE @ 0x006bd230</remarks>
    void request(MechWarrior* pilot, int32_t selectionIndex, uint32_t moveParams, float priority, int32_t initPath);
    /// <summary>Serves the first request.</summary>
    /// <remarks>MCX.EXE @ 0x006bd310</remarks>
    void calcPath();
    /// <summary>Serves up to five requests.</summary>
    /// <remarks>MCX.EXE @ 0x006bd350</remarks>
    void update();

    PathQueueRec pool[MAX_PATH_QUEUE_RECS]; // +0x0
    PathQueueRec* queueFront;               // +0x2a30
    PathQueueRec* queueEnd;                 // +0x2a34
    PathQueueRec* freeList;                 // +0x2a38
};

#pragma pack(push, 1)
/// <summary>One door of an area: the door and which of its two sides the area is on.</summary>
/// <remarks>Original: <c>struct _DoorInfo</c>. 3 bytes, stored packed in the global map file.</remarks>
typedef struct _DoorInfo
{
    int16_t doorIndex; // +0x0
    char doorSide;     // +0x2
} DoorInfo;
static_assert(sizeof(DoorInfo) == 3);

/// <summary>A link from a door side to another door of the same area, with its cost.</summary>
/// <remarks>7 bytes, stored packed in the global map file. The original name isn't known (MC2: DoorLink).</remarks>
typedef struct _DoorLink
{
    int16_t doorIndex; // +0x0
    char doorSide;     // +0x2
    int32_t cost;      // +0x3
} DoorLink;
static_assert(sizeof(DoorLink) == 7);
#pragma pack(pop)

/// <summary>An area of the <see cref="GlobalMap"/>: a connected region of tiles within one sector.</summary>
/// <remarks>
/// The original name isn't known (MC2: GlobalMapArea). In the original a 0x29-byte packed record, read and written
/// raw with the <c>doors</c> pointer inside; the port keeps natural layout and reads the record field by field
/// (sectorR s16, sectorC s16, doors u32 (ignored, rebuilt from doorInfos), type s32, numDoors s8, then the rest).
/// Offsets below are the file record's.
/// </remarks>
typedef struct _GlobalMapArea
{
    int16_t sectorR; // +0x0
    int16_t sectorC; // +0x2
    /// <summary>The area's entries in GlobalMap::doorInfos.</summary>
    DoorInfo* doors; // +0x4
    /// <summary>0 normal, 1 a north-south bridge, 2 an east-west bridge (road or railroad; see
    /// GlobalMap::calcBridges).</summary>
    int32_t type;      // +0x8
    char numDoors;     // +0xc
    int32_t open;      // +0xd
    int32_t unknown11; // +0x11 (initialised -1)
    int32_t unknown15; // +0x15
    /// <summary>Set by GlobalMap::closeArea.</summary>
    int32_t closed;    // +0x19
    int32_t unknown1D; // +0x1d
    int32_t unknown21; // +0x21
    int32_t unknown25; // +0x25
} GlobalMapArea;
/// <summary>Size of a <see cref="GlobalMapArea"/> record in the global map file.</summary>
inline constexpr int32_t GLOBALMAP_AREA_RECORD_SIZE = 0x29;

/// <summary>
/// A door of the <see cref="GlobalMap"/>: a run of cells joining two areas, with the A* bookkeeping of
/// <see cref="GlobalMap::calcPath"/>.
/// </summary>
/// <remarks>
/// Original: <c>struct _GlobalMapDoor</c>. In the original a 0x3b-byte packed record, read and written raw with the
/// two <c>links</c> pointers inside; the port keeps natural layout and reads the record field by field (the link
/// pointers are rebuilt from doorLinks). Offsets below are the file record's.
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
/// <remarks>Original: <c>struct _GlobalPathStep</c>. 0x30 bytes.</remarks>
typedef struct _GlobalPathStep
{
    int32_t unknown00; // +0x0
    int32_t thruArea;  // +0x4
    int32_t goalDoor;  // +0x8
    /// <summary>Not written by GlobalMap::calcPath.</summary>
    int32_t unknown0C[6]; // +0xc
    /// <summary>The cell (row, column) the leg's path ended in (Mover::calcMovePath fills it); the next leg starts
    /// from it (MechWarrior::calcMovePath).</summary>
    int32_t goalCell[2]; // +0x24
    int32_t costToGoal;  // +0x2c
} GlobalPathStep;

/// <summary>
/// The long-range movement map: the scenario split into sectors of 10x10 tiles, each into areas joined by doors,
/// searched with A* over doors before a <see cref="MoveMap"/> plans the cells of each leg.
/// </summary>
/// <remarks>Original source: <c>ai\move.cpp</c>. 0x58 bytes.</remarks>
class GlobalMap
{
public:
    /// <remarks>MCX.EXE @ 0x006bd370</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x006bd390</remarks>
    static void operator delete(void* ptr);

    /// <summary>
    /// Allocates an empty area map of newHeight x newWidth tiles (both multiples of 10); the sector grid is sized
    /// from the width alone.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006bd3b0</remarks>
    void init(int32_t newWidth, int32_t newHeight);
    /// <summary>Reads a global map file (see <see cref="write"/>).</summary>
    /// <remarks>MCX.EXE @ 0x006bd480</remarks>
    int32_t init(File* mapFile);
    /// <summary>Computes the areas, doors and links of a scenario map (height/width -1: the map's).</summary>
    /// <remarks>MCX.EXE @ 0x006bf8b0</remarks>
    int32_t init(ScenarioMap* map, int32_t unknownA, int32_t unknownB, int32_t height = -1, int32_t width = -1);
    /// <remarks>MCX.EXE @ 0x006bd860</remarks>
    int32_t write(File* mapFile);
    /// <remarks>MCX.EXE @ 0x006c0b20</remarks>
    void destroy();

    /// <summary>The area of a tile, -1 for none.</summary>
    /// <remarks>MCX.EXE @ 0x00654ca0 (inline in the original's move.h)</remarks>
    int32_t calcArea(int32_t tileR, int32_t tileC);
    /// <summary>Makes the spare last area a one-tile area at (tileR, tileC).</summary>
    /// <remarks>MCX.EXE @ 0x006bda60</remarks>
    int32_t setTempArea(int32_t tileR, int32_t tileC, int32_t cost);

    /// <remarks>MCX.EXE @ 0x006bdac0</remarks>
    int fillNorthSouthBridgeArea(ScenarioMap* map, int32_t row, int32_t col, int32_t area);
    /// <remarks>MCX.EXE @ 0x006bdbf0</remarks>
    int fillEastWestBridgeArea(ScenarioMap* map, int32_t row, int32_t col, int32_t area);
    /// <remarks>MCX.EXE @ 0x006bdd20</remarks>
    int fillNorthSouthRailroadBridgeArea(ScenarioMap* map, int32_t row, int32_t col, int32_t area);
    /// <remarks>MCX.EXE @ 0x006bde50</remarks>
    int fillEastWestRailroadBridgeArea(ScenarioMap* map, int32_t row, int32_t col, int32_t area);
    /// <summary>Flood-fills an area from (row, col) within the current sector bounds.</summary>
    /// <remarks>MCX.EXE @ 0x006be0a0</remarks>
    int fillArea(ScenarioMap* map, int32_t row, int32_t col, int32_t area);
    /// <remarks>MCX.EXE @ 0x006be2b0</remarks>
    void calcSectorAreas(ScenarioMap* map, int32_t sectorR, int32_t sectorC);
    /// <remarks>MCX.EXE @ 0x006be3c0</remarks>
    void calcAreas(ScenarioMap* map);
    /// <remarks>MCX.EXE @ 0x006be560</remarks>
    void calcBridges(ScenarioMap* map);

    /// <remarks>MCX.EXE @ 0x006be600</remarks>
    void beginDoorProcessing();
    /// <remarks>MCX.EXE @ 0x006be630</remarks>
    void addDoor(int32_t area1, int32_t area2, int32_t row, int32_t col, int32_t cellR, int32_t cellC, int32_t length,
                 int32_t direction);
    /// <remarks>MCX.EXE @ 0x006be820</remarks>
    void endDoorProcessing();
    /// <remarks>MCX.EXE @ 0x006be880</remarks>
    int32_t numAreaDoors(int32_t area);
    /// <remarks>MCX.EXE @ 0x006be8c0</remarks>
    void getAreaDoors(int32_t area, DoorInfo* doorList);
    /// <remarks>MCX.EXE @ 0x006be910</remarks>
    void calcGlobalDoors(ScenarioMap* map);
    /// <remarks>MCX.EXE @ 0x006befe0</remarks>
    void calcAreaDoors();
    /// <summary>The cell path cost from one door of an area to another (runs PathFindMap).</summary>
    /// <remarks>MCX.EXE @ 0x006bf060</remarks>
    int32_t calcLinkCost(int32_t startDoor, int32_t thruArea, int32_t goalDoor);
    /// <remarks>MCX.EXE @ 0x006bf3b0</remarks>
    void calcDoorLinks();
    /// <summary>Does nothing in MCX.</summary>
    /// <remarks>MCX.EXE @ 0x006bf6d0</remarks>
    void calcSectorPaths(ScenarioMap* map, int32_t sectorR, int32_t sectorC);
    /// <remarks>MCX.EXE @ 0x006bf6e0</remarks>
    void calcPathCostTable();
    /// <summary>The direction a door leaves <paramref name="fromArea"/> by, -1 if it doesn't touch it.</summary>
    /// <remarks>MCX.EXE @ 0x006bf790</remarks>
    int32_t exitDirection(int32_t doorIndex, int32_t fromArea);
    /// <summary>Copies door <paramref name="doorIndex"/> of an area.</summary>
    /// <remarks>MCX.EXE @ 0x006bf7e0</remarks>
    void getDoorTiles(int32_t area, int32_t doorIndex, GlobalMapDoor* door);
    /// <remarks>MCX.EXE @ 0x006bf830</remarks>
    vector_3d getDoorWorldPos(int32_t area, int32_t door, int32_t* prevGoalCell);

    /// <summary>Adds the temporary start door (index numDoors) joining area <paramref name="startArea"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006bf9f0</remarks>
    void setStartDoor(int32_t startArea);
    /// <remarks>MCX.EXE @ 0x006bfb40</remarks>
    void resetStartDoor(int32_t startArea);
    /// <summary>Adds the temporary goal door (index numDoors + 1) and records the goal sector.</summary>
    /// <remarks>MCX.EXE @ 0x006bfbd0</remarks>
    void setGoalDoor(int32_t goalArea);
    /// <remarks>MCX.EXE @ 0x006bfda0</remarks>
    void resetGoalDoor(int32_t goalArea);
    /// <summary>A* estimate: sector distance from a door to the goal sector.</summary>
    /// <remarks>MCX.EXE @ 0x006bfe30</remarks>
    int32_t calcHPrime(int32_t door);
    /// <summary>Finds the door path from one area to another.</summary>
    /// <returns>The number of steps written to <paramref name="path"/>, 0 when there is none.</returns>
    /// <remarks>MCX.EXE @ 0x006bfef0</remarks>
    int32_t calcPath(int32_t startArea, int32_t goalArea, GlobalPathStep* path);
    /// <remarks>MCX.EXE @ 0x006c0540</remarks>
    void propogateCost(int32_t door, int32_t cost, int32_t fromSide, int32_t g);
    /// <summary>As <see cref="calcPath(int32_t, int32_t, GlobalPathStep*)"/>, between the areas of two positions.</summary>
    /// <remarks>MCX.EXE @ 0x006c0740</remarks>
    int32_t calcPath(vector_3d start, vector_3d goal, GlobalPathStep* path);
    /// <summary>The number of steps between two areas, from the path cost table.</summary>
    /// <remarks>MCX.EXE @ 0x006c0820</remarks>
    int32_t getPathCost(int32_t startArea, int32_t goalArea);
    /// <remarks>MCX.EXE @ 0x006c0860</remarks>
    void openDoor(int32_t door);
    /// <remarks>MCX.EXE @ 0x006c0890</remarks>
    void closeDoor(int32_t door);
    /// <summary>Closes an area and its doors and clears its path cost entries.</summary>
    /// <remarks>MCX.EXE @ 0x006c08c0</remarks>
    void closeArea(int32_t area);
    /// <remarks>MCX.EXE @ 0x006c0950</remarks>
    void print(char* fileName, int32_t ULr, int32_t ULc, int32_t height, int32_t width);

    /// <summary>Tile bounds of the sector being filled.</summary>
    static int32_t minTileR;
    static int32_t maxTileR;
    static int32_t minTileC;
    static int32_t maxTileC;

    /// <summary>Stored in the file header; passed to init(ScenarioMap*, ...) by the scenario.</summary>
    int32_t unknown00; // +0x0
    int32_t unknown04; // +0x4
    int32_t height;    // +0x8
    int32_t width;     // +0xc
    /// <summary>Sector side in tiles (10).</summary>
    int32_t sectorDim;    // +0x10
    int32_t sectorHeight; // +0x14
    int32_t sectorWidth;  // +0x18
    int32_t numAreas;     // +0x1c
    int32_t numDoors;     // +0x20
    int32_t numDoorInfos; // +0x24
    int32_t numDoorLinks; // +0x28
    /// <summary>Area per tile when there are fewer than 256 areas (0xff = none); else <see cref="areaMap"/>.</summary>
    uint8_t* smallAreaMap; // +0x2c
    /// <summary>Area per tile (-1 = none).</summary>
    int16_t* areaMap;     // +0x30
    GlobalMapArea* areas; // +0x34
    /// <summary>numDoors + 2 doors (the last two are the temporary start and goal doors).</summary>
    GlobalMapDoor* doors; // +0x38
    DoorInfo* doorInfos;  // +0x3c
    DoorLink* doorLinks;  // +0x40
    /// <summary>Doors collected while computing the map (MAX_BUILD_DOORS).</summary>
    GlobalMapDoor* doorBuildList; // +0x44
    /// <summary>numAreas x numAreas steps between areas (0 = no path, 0xff = 255 or more).</summary>
    uint8_t* pathCostTable; // +0x48
    /// <summary>Only counted up to numAreas by init(File*) (a leftover loop).</summary>
    int32_t unknown4C;   // +0x4c
    int32_t goalSectorR; // +0x50
    int32_t goalSectorC; // +0x54
};

/// <summary>One cell of a <see cref="MoveMap"/>.</summary>
/// <remarks>0x18 bytes. The original name isn't known (MC2: MoveMapNode).</remarks>
typedef struct _MoveMapNode
{
    int32_t cost;   // +0x0
    int32_t parent; // +0x4
    /// <summary>MOVEFLAG bits: 1 on the open list, 2 closed, 4 on the found path, 8 goal, 0x10 a mover stands
    /// here.</summary>
    uint32_t flags; // +0x8
    int32_t g;      // +0xc
    int32_t hPrime; // +0x10
    int32_t fPrime; // +0x14
} MoveMapNode;

/// <summary>The short-range cell path finder: an A* over a window of the scenario map's cells.</summary>
/// <remarks>Original source: <c>ai\move.cpp</c>. 0x88 bytes.</remarks>
class MoveMap
{
public:
    /// <remarks>MCX.EXE @ 0x006c0c60</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x006c0c80</remarks>
    static void operator delete(void* ptr);

    /// <summary>Allocates the cells for a window of up to newMaxHeight x newMaxWidth tiles.</summary>
    /// <remarks>MCX.EXE @ 0x006c0ca0</remarks>
    void init(int32_t newMaxWidth, int32_t newMaxHeight);
    /// <summary>Reads a MoveMap FIT file (Height, Width, VertexCost).</summary>
    /// <remarks>MCX.EXE @ 0x006c0ff0</remarks>
    int32_t init(FitIniFile* mapFile);
    /// <remarks>MCX.EXE @ 0x006c0d10</remarks>
    void clear();
    /// <summary>Adds the cost of the standing mechs (other than the mover) to their cells.</summary>
    /// <remarks>MCX.EXE @ 0x006c0d60</remarks>
    void placeMovers(int markGoal);
    /// <remarks>MCX.EXE @ 0x006c11b0</remarks>
    void setTarget(vector_3d targetPos);
    /// <summary>Sets the start: a world position (startR -1: computed from it) or a cell.</summary>
    /// <remarks>MCX.EXE @ 0x006c11d0</remarks>
    void setStart(vector_3d* startPos, int32_t startR = -1, int32_t startC = -1);
    /// <summary>Sets the goal: a world position (goalR -1: computed from it) or a cell.</summary>
    /// <remarks>MCX.EXE @ 0x006c1270</remarks>
    void setGoal(vector_3d goalPos, int32_t goalR = -1, int32_t goalC = -1);
    /// <summary>Sets the goal to a door of <see cref="GlobalMoveMap"/>, from area <paramref name="thruArea"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006c1310</remarks>
    void setGoal(int32_t thruArea, int32_t goalDoor);

    /// <summary>Loads the cell costs of a window of the map and sets start and goal (a position).</summary>
    /// <remarks>MCX.EXE @ 0x006c15a0</remarks>
    int32_t setUp(ScenarioMap* map, int32_t ULr, int32_t ULc, int32_t height, int32_t width, vector_3d* startPos,
                  int32_t startR, int32_t startC, vector_3d goalPos, int32_t goalR, int32_t goalC,
                  int32_t* overlayWeightTable, int32_t moveLevel, int32_t jumpCost, int32_t numOffsets,
                  uint32_t params);
    /// <summary>As the other setUp, with the goal a door (thruArea, goalDoor) of <see cref="GlobalMoveMap"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006c1db0</remarks>
    int32_t setUp(ScenarioMap* map, int32_t ULr, int32_t ULc, int32_t height, int32_t width, vector_3d* startPos,
                  int32_t startR, int32_t startC, int32_t thruArea, int32_t goalDoor, vector_3d targetPos,
                  int32_t* overlayWeightTable, int32_t moveLevel, int32_t jumpCost, int32_t numOffsets,
                  uint32_t params);
    /// <remarks>MCX.EXE @ 0x006c1c00</remarks>
    int32_t markEscapeGoalCells(vector_3d goalPos);
    /// <remarks>MCX.EXE @ 0x006c22f0</remarks>
    int32_t markGoalCells(vector_3d goalPos);
    /// <summary>
    /// Runs the search from the start cell to the nearest goal cell and fills <paramref name="path"/>; the goal
    /// cell reached (its map cell, when the goal is a door) goes to <paramref name="goalCell"/>.
    /// </summary>
    /// <returns>The path's step count, 0 when no path was found.</returns>
    /// <remarks>MCX.EXE @ 0x006c2b10</remarks>
    int32_t calcPath(MovePath* path, vector_3d* goalWorldPos, int32_t* goalCell);
    /// <summary>As <see cref="calcPath"/>, with a flat estimate (every cell 10 from the goal cells
    /// markEscapeGoalCells set).</summary>
    /// <remarks>MCX.EXE @ 0x006c3fb0</remarks>
    int32_t calcEscapePath(MovePath* path, vector_3d* goalWorldPos, int32_t* goalCell);
    /// <summary>Writes the window's costs and the path to a debug file.</summary>
    /// <remarks>MCX.EXE @ 0x006c4c90</remarks>
    void writeDebug(File* debugFile);
    /// <remarks>MCX.EXE @ 0x006c5190</remarks>
    void destroy();

protected:
    /// <remarks>MCX.EXE @ 0x006c39e0</remarks>
    int adjacentCellOpen(int32_t r, int32_t c, int32_t dir);
    /// <remarks>MCX.EXE @ 0x006c3b30</remarks>
    void propogateCost(int32_t r, int32_t c, int32_t cost, int32_t g);
    /// <summary>
    /// The A* search and path building calcPath and calcEscapePath share (MCX has two copies that differ only in
    /// the estimate and their messages).
    /// </summary>
    /// <remarks>The port's; not in MCX.EXE.</remarks>
    int32_t searchPath(MovePath* path, vector_3d* goalWorldPos, int32_t* goalCell, bool escape);

public:
    /// <summary>Upper-left tile of the window.</summary>
    int32_t ULr; // +0x0
    int32_t ULc; // +0x4
    /// <summary>Upper-left cell of the window (ULr * 3, ULc * 3).</summary>
    int32_t minRow; // +0x8
    int32_t minCol; // +0xc
    /// <remarks>init(maxHeight, maxWidth) stores its first argument here; the maps are square, so which side is
    /// which isn't certain.</remarks>
    int32_t maxWidth;  // +0x10
    int32_t maxHeight; // +0x14
    /// <summary>Row stride of <see cref="map"/>, in cells.</summary>
    int32_t maxCellWidth;  // +0x18
    int32_t maxCellHeight; // +0x1c
    int32_t width;         // +0x20
    int32_t height;        // +0x24
    int32_t cellWidth;     // +0x28
    int32_t cellHeight;    // +0x2c
    MoveMapNode* map;      // +0x30
    vector_3d startPos;    // +0x34
    int32_t startR;        // +0x40
    int32_t startC;        // +0x44
    vector_3d goalPos;     // +0x48
    int32_t goalR;         // +0x54
    int32_t goalC;         // +0x58
    /// <summary>The goal door, when the goal is a door (<see cref="GoalIsDoor"/>).</summary>
    int32_t door;     // +0x5c
    int32_t doorSide; // +0x60
    /// <summary>Direction the goal door is entered from, -1 when the goal isn't a door.</summary>
    int32_t doorDirection; // +0x64
    vector_3d target;      // +0x68
    /// <summary>
    /// The cost of a plain passable cell (setUp's moveLevel); overlays, locks and mines add to it.
    /// </summary>
    int32_t moveLevel; // +0x74
    /// <summary>Added to the cost of the jump offsets (the ones past the eight neighbours); with
    /// <see cref="JumpOnBlocked"/> it is their whole cost.</summary>
    int32_t jumpCost; // +0x78
    /// <summary>How many of the NUM_CELL_OFFSETS offsets the search tries.</summary>
    int32_t numOffsets;          // +0x7c
    float calcTime;              // +0x80
    int32_t* overlayWeightTable; // +0x84
};

/// <remarks>MCX.EXE @ 0x006b8eb0</remarks>
void worldCoordToMapCoord(vector_3d pos, int32_t& tileR, int32_t& tileC, int32_t& cellR, int32_t& cellC);
/// <remarks>MCX.EXE @ 0x006b8f30</remarks>
void worldCoordToMapTile(vector_3d pos, int32_t& tileR, int32_t& tileC);
/// <remarks>MCX.EXE @ 0x006b8f70</remarks>
void worldCoordToMapCell(vector_3d pos, int32_t& cellR, int32_t& cellC);
/// <summary>
/// The point <paramref name="distance"/> meters from <paramref name="pos"/> at <paramref name="angle"/> degrees,
/// pulled back along the line to the last (or first, per <paramref name="flags"/> bit 2) passable cell.
/// </summary>
/// <remarks>MCX.EXE @ 0x006b8fb0</remarks>
vector_3d relativePositionToPoint(vector_3d pos, float angle, float distance, uint32_t flags);
/// <remarks>MCX.EXE @ 0x006b93f0</remarks>
void mapTileCellToWorldPos(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, vector_3d& worldPos);
/// <remarks>MCX.EXE @ 0x006b9440</remarks>
void mapCellToWorldPos(int32_t cellR, int32_t cellC, vector_3d& worldPos);
/// <summary>Dumps <see cref="openList"/> to openlist.dbg.</summary>
/// <remarks>MCX.EXE @ 0x006b9480</remarks>
void DebugOpenList(char* msg);
/// <summary>The terrain type of a tile texture index (bands of 79).</summary>
/// <remarks>MCX.EXE @ 0x006b9b40</remarks>
int32_t calcTileTypeFromIndex(int32_t tileIndex);
/// <remarks>MCX.EXE @ 0x006b9e80</remarks>
int32_t calcOverlayTypeFromIndex(int32_t overlayIndex);
/// <summary>Writes one or two chunks and the mover's state to mvchunk.dbg.</summary>
/// <remarks>MCX.EXE @ 0x006bbaa0</remarks>
void DebugMoveChunk(Mover* mover, MoveChunk* chunk1, MoveChunk* chunk2);
/// <summary>Whether a tile's overlay blocks left-right crossing.</summary>
/// <remarks>MCX.EXE @ 0x006bdf80</remarks>
int isLRBlocked(MapTile* tile);
/// <summary>The cell direction (0-7) an object faces.</summary>
/// <remarks>MCX.EXE @ 0x006c1470</remarks>
int32_t cellFacing(GameObject* object);
/// <summary>The direction from one tile/cell to another adjacent one (uses a static deltaDir[][3] table).</summary>
/// <remarks>Only inlined copies exist in MCX.EXE (in MoveChunk::build).</remarks>
int32_t cellDirToCell(int32_t fromTileR, int32_t fromTileC, int32_t fromCellR, int32_t fromCellC, int32_t toTileR,
                      int32_t toTileC, int32_t toCellR, int32_t toCellC);

// Globals of ai\move.cpp. (Several are listed under their heaviest users in globals_by_file.md, but sit in
// move.cpp's data between its own globals.)

/// <remarks>MCX.EXE @ 0x00794828</remarks>
extern int BlockWallTiles;
/// <remarks>MCX.EXE @ 0x0079482c</remarks>
extern int32_t SimpleMovePathRange;
/// <summary>Row / column offsets of the eight neighbours, for spreadState.</summary>
/// <remarks>MCX.EXE @ 0x00794838</remarks>
extern char rowShift[8];
/// <remarks>MCX.EXE @ 0x00794840</remarks>
extern char colShift[8];
/// <summary>Row and column offset of each search offset.</summary>
/// <remarks>MCX.EXE @ 0x00794848</remarks>
extern int32_t cellShift[NUM_CELL_OFFSETS * 2];
/// <remarks>MCX.EXE @ 0x00794b88</remarks>
extern char reverseShift[NUM_CELL_OFFSETS];
/// <remarks>MCX.EXE @ 0x00794bf0</remarks>
extern int IsDiagonalStep[NUM_CELL_OFFSETS];
/// <remarks>MCX.EXE @ 0x00794d90</remarks>
extern int32_t StepAdjDir[9];
/// <remarks>MCX.EXE @ 0x00794db4</remarks>
extern int32_t adjTile[4][2];
/// <summary>Per mine layout, the mine type of each of the nine cells.</summary>
/// <remarks>MCX.EXE @ 0x00794dd4</remarks>
extern char mineLayout[4][9];
/// <remarks>MCX.EXE @ 0x00794df8</remarks>
extern int32_t MaxHPrime;
/// <remarks>MCX.EXE @ 0x00794ee0</remarks>
extern char OverlayIsBridge[NUM_OVERLAY_TYPES];
/// <remarks>MCX.EXE @ 0x00794f2c</remarks>
extern char OverlayIsClosedGate[NUM_OVERLAY_TYPES];
/// <remarks>MCX.EXE @ 0x00794f78</remarks>
extern char OverlayIsDirtRoad[NUM_OVERLAY_TYPES];
/// <summary>Per cell (cellR * 3 + cellC) and direction: tile row/col delta and the neighbour's cell row/col.</summary>
/// <remarks>MCX.EXE @ 0x00795000. The first dimension (9) is inferred from its use, not from the image.</remarks>
extern int32_t adjCellTable[MAPCELL_DIM * MAPCELL_DIM][8][4];
/// <remarks>MCX.EXE @ 0x0080250c</remarks>
extern float cellColToWorldCoord[MAX_MAP_CELL_WIDTH];
/// <remarks>MCX.EXE @ 0x0080304c</remarks>
extern float cellToWorldCoord[MAPCELL_DIM];
/// <remarks>MCX.EXE @ 0x00803058</remarks>
extern float tileColToWorldCoord[MAX_MAP_TILE_WIDTH];
/// <remarks>MCX.EXE @ 0x00803418</remarks>
extern float cellShiftDistance[NUM_CELL_OFFSETS];
/// <remarks>MCX.EXE @ 0x008035b8</remarks>
extern float tileRowToWorldCoord[MAX_MAP_TILE_WIDTH];
/// <summary>Start of each overlay type's 3x3 block within a level of <see cref="OverlayWeightTable"/>.</summary>
/// <remarks>MCX.EXE @ 0x00803abc</remarks>
extern int32_t OverlayWeightIndex[NUM_OVERLAY_TYPES];
/// <remarks>MCX.EXE @ 0x00803be8</remarks>
extern float cellRowToWorldCoord[MAX_MAP_CELL_WIDTH];
/// <summary>Cell costs per move level and overlay (the scenario's OverlayCellCosts).</summary>
/// <remarks>MCX.EXE @ 0x00804728</remarks>
extern int32_t OverlayWeightTable[NUM_MOVE_LEVELS * OVERLAY_WEIGHT_LEVEL_SIZE];
/// <summary>tile * MAPCELL_DIM.</summary>
/// <remarks>MCX.EXE @ 0x00807be4</remarks>
extern int32_t tileMulMAPCELL_DIM[MAX_MAP_TILE_WIDTH];
/// <remarks>MCX.EXE @ 0x00807fb8</remarks>
extern int32_t MoveChunkUnpackErr;
/// <remarks>MCX.EXE @ 0x00807fc0</remarks>
extern int ClearBridgeTiles;
/// <summary>The object whose path is being calculated (it doesn't block itself).</summary>
/// <remarks>MCX.EXE @ 0x00807fc4</remarks>
extern GameObject* MovingObject;
/// <remarks>MCX.EXE @ 0x00807fc8</remarks>
extern GameObject* RamObject;
/// <summary>The A* open list shared by the path finders.</summary>
/// <remarks>MCX.EXE @ 0x00807fcc</remarks>
extern PriorityQueue* openList;
/// <remarks>MCX.EXE @ 0x00807fd0</remarks>
extern int JumpOnBlocked;
/// <remarks>MCX.EXE @ 0x00807fd4</remarks>
extern int FindingEscapePath;
/// <remarks>MCX.EXE @ 0x00807fd8</remarks>
extern ScenarioMap* GameMap;
/// <remarks>MCX.EXE @ 0x00807fdc</remarks>
extern GlobalMap* GlobalMoveMap;
/// <summary>The cell path finder the movers share. Unnamed in MCX.EXE; the name is the one its Fatal messages
/// use.</summary>
/// <remarks>MCX.EXE @ 0x007e36f0</remarks>
extern MoveMap* PathFindMap;
/// <remarks>MCX.EXE @ 0x00807fe0</remarks>
extern ObjectMap* GameObjectMap;
/// <remarks>MCX.EXE @ 0x00807fe4</remarks>
extern MovePathManager* PathManager;
/// <remarks>MCX.EXE @ 0x00807fe8</remarks>
extern int32_t CurPlanet;
/// <remarks>MCX.EXE @ 0x00807fec</remarks>
extern int32_t DebugMovePathType;
/// <remarks>MCX.EXE @ 0x00807ff0</remarks>
extern int32_t NumPathsInQueue;
/// <remarks>MCX.EXE @ 0x00807ff4</remarks>
extern float MapCellDiagonal;
/// <remarks>MCX.EXE @ 0x00807ff8</remarks>
extern float HalfMapCell;
/// <remarks>MCX.EXE @ 0x00807ffc</remarks>
extern float VerticesMapSideDivTwo;
/// <remarks>MCX.EXE @ 0x00808000</remarks>
extern float MetersMapSideDivTwo;
/// <remarks>MCX.EXE @ 0x00808004</remarks>
extern int GoalIsDoor;
/// <remarks>MCX.EXE @ 0x00808010</remarks>
extern int PreserveMapTiles;
/// <remarks>MCX.EXE @ 0x00808014</remarks>
extern float MetersPerCell;
