#include "stdafx.h"
#include "ai/move.h"
#include "lib/aerror.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "lib/inifile.h"
#include "lib/packet.h"
#include "lib/pqueue.h"
#include "lib/routines.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "object/gameobj.h"
#include "object/gvehicl.h"
#include "object/mech.h"
#include "object/mover.h"
#include "object/objblck.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/objtype.h"
#include "object/warrior.h"
#include "terrain/terrain.h"

int32_t GlobalMap::minTileR = 0;
int32_t GlobalMap::maxTileR = 0;
int32_t GlobalMap::minTileC = 0;
int32_t GlobalMap::maxTileC = 0;

int BlockWallTiles = 1;
int32_t SimpleMovePathRange = 7;
char rowShift[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
char colShift[8] = {0, 1, 1, 1, 0, -1, -1, -1};
int32_t cellShift[NUM_CELL_OFFSETS * 2] = {
    -1,  0, -1,  1,   0, 1,   1,   1,   1,   0,  1,   -1,  0,  -1,  -1,  -1,  -2,  0, -2,  2,   0, 2,   2,   2,
    2,   0, 2,   -2,  0, -2,  -2,  -2,  -3,  0,  -3,  3,   0,  3,   3,   3,   3,   0, 3,   -3,  0, -3,  -3,  -3,
    -3,  2, -2,  3,   2, 3,   3,   2,   3,   -2, 2,   -3,  -2, -3,  -3,  -2,  -4,  0, -4,  4,   0, 4,   4,   4,
    4,   0, 4,   -4,  0, -4,  -4,  -4,  -5,  0,  -5,  5,   0,  5,   5,   5,   5,   0, 5,   -5,  0, -5,  -5,  -5,
    -6,  0, -6,  6,   0, 6,   6,   6,   6,   0,  6,   -6,  0,  -6,  -6,  -6,  -7,  0, -7,  7,   0, 7,   7,   7,
    7,   0, 7,   -7,  0, -7,  -7,  -7,  -8,  0,  -8,  8,   0,  8,   8,   8,   8,   0, 8,   -8,  0, -8,  -8,  -8,
    -9,  0, -9,  9,   0, 9,   9,   9,   9,   0,  9,   -9,  0,  -9,  -9,  -9,  -10, 0, -10, 10,  0, 10,  10,  10,
    10,  0, 10,  -10, 0, -10, -10, -10, -11, 0,  -11, 11,  0,  11,  11,  11,  11,  0, 11,  -11, 0, -11, -11, -11,
    -12, 0, -12, 12,  0, 12,  12,  12,  12,  0,  12,  -12, 0,  -12, -12, -12,
};

char reverseShift[NUM_CELL_OFFSETS] = {
    4,  5,  6,  7,  0,  1,  2,  3,  12, 13, 14, 15, 8,  9,  10, 11, 20, 21, 22,  23,  16,  17,  18, 19, 28, 29,
    30, 31, 24, 25, 26, 27, 36, 37, 38, 39, 32, 33, 34, 35, 44, 45, 46, 47, 40,  41,  42,  43,  52, 53, 54, 55,
    48, 49, 50, 51, 60, 61, 62, 63, 56, 57, 58, 59, 68, 69, 70, 71, 64, 65, 66,  67,  76,  77,  78, 79, 72, 73,
    74, 75, 84, 85, 86, 87, 80, 81, 82, 83, 92, 93, 94, 95, 88, 89, 90, 91, 100, 101, 102, 103, 96, 97, 98, 99,
};

int IsDiagonalStep[NUM_CELL_OFFSETS] = {0, 1, 0, 1, 0, 1, 0, 1};
int32_t StepAdjDir[9] = {-1, 0, 2, 2, 4, 4, 6, 6, 0};
int32_t adjTile[4][2] = {{-1, 0}, {0, 1}, {1, 0}, {0, -1}};
char mineLayout[4][9] = {{}, {}, {1, 0, 1, 0, 1, 0, 1, 0, 1}, {}};
int32_t MaxHPrime = 1000;
char OverlayIsBridge[NUM_OVERLAY_TYPES] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
};

char OverlayIsClosedGate[NUM_OVERLAY_TYPES] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1,
};

char OverlayIsDirtRoad[NUM_OVERLAY_TYPES] = {
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
};

int32_t adjCellTable[MAPCELL_DIM * MAPCELL_DIM][8][4] = {
    {{-1, 0, 2, 0},
     {-1, 0, 2, 1},
     {0, 0, 0, 1},
     {0, 0, 1, 1},
     {0, 0, 1, 0},
     {0, -1, 1, 2},
     {0, -1, 0, 2},
     {-1, -1, 2, 2}},
    {{-1, 0, 2, 1}, {-1, 0, 2, 2}, {0, 0, 0, 2}, {0, 0, 1, 2}, {0, 0, 1, 1}, {0, 0, 1, 0}, {0, 0, 0, 0}, {-1, 0, 2, 0}},
    {{-1, 0, 2, 2}, {-1, 1, 2, 0}, {0, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 1, 2}, {0, 0, 1, 1}, {0, 0, 0, 1}, {-1, 0, 2, 1}},
    {{0, 0, 0, 0}, {0, 0, 0, 1}, {0, 0, 1, 1}, {0, 0, 2, 1}, {0, 0, 2, 0}, {0, -1, 2, 2}, {0, -1, 1, 2}, {0, -1, 0, 2}},
    {{0, 0, 0, 1}, {0, 0, 0, 2}, {0, 0, 1, 2}, {0, 0, 2, 2}, {0, 0, 2, 1}, {0, 0, 2, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}},
    {{0, 0, 0, 2}, {0, 1, 0, 0}, {0, 1, 1, 0}, {0, 1, 2, 0}, {0, 0, 2, 2}, {0, 0, 2, 1}, {0, 0, 1, 1}, {0, 0, 0, 1}},
    {{0, 0, 1, 0}, {0, 0, 1, 1}, {0, 0, 2, 1}, {1, 0, 0, 1}, {1, 0, 0, 0}, {1, -1, 0, 2}, {0, -1, 2, 2}, {0, -1, 1, 2}},
    {{0, 0, 1, 1}, {0, 0, 1, 2}, {0, 0, 2, 2}, {1, 0, 0, 2}, {1, 0, 0, 1}, {1, 0, 0, 0}, {0, 0, 2, 0}, {0, 0, 1, 0}},
    {{0, 0, 1, 2}, {0, 1, 1, 0}, {0, 1, 2, 0}, {1, 1, 0, 0}, {1, 0, 0, 2}, {1, 0, 0, 1}, {0, 0, 2, 1}, {0, 0, 1, 1}},
};

float cellColToWorldCoord[MAX_MAP_CELL_WIDTH] = {};
float cellToWorldCoord[MAPCELL_DIM] = {};
float tileColToWorldCoord[MAX_MAP_TILE_WIDTH] = {};
float cellShiftDistance[NUM_CELL_OFFSETS] = {};
float tileRowToWorldCoord[MAX_MAP_TILE_WIDTH] = {};
int32_t OverlayWeightIndex[NUM_OVERLAY_TYPES] = {};
float cellRowToWorldCoord[MAX_MAP_CELL_WIDTH] = {};
int32_t OverlayWeightTable[NUM_MOVE_LEVELS * OVERLAY_WEIGHT_LEVEL_SIZE] = {};
int32_t tileMulMAPCELL_DIM[MAX_MAP_TILE_WIDTH] = {};
int32_t MoveChunkUnpackErr = 0;
int ClearBridgeTiles = 0;
GameObject* MovingObject = nullptr;
GameObject* RamObject = nullptr;
PriorityQueue* openList = nullptr;
int JumpOnBlocked = 0;
int FindingEscapePath = 0;
ScenarioMap* GameMap = nullptr;
GlobalMap* GlobalMoveMap = nullptr;
MoveMap* PathFindMap = nullptr;
ObjectMap* GameObjectMap = nullptr;
MovePathManager* PathManager = nullptr;
int32_t CurPlanet = 0;
int32_t DebugMovePathType = 0;
int32_t NumPathsInQueue = 0;
float MapCellDiagonal = 0.0f;
float HalfMapCell = 0.0f;
float VerticesMapSideDivTwo = 0.0f;
float MetersMapSideDivTwo = 0.0f;
int GoalIsDoor = 0;
int PreserveMapTiles = 0;
float MetersPerCell = 0.0f;

namespace
{
    /// <summary>Degrees to radians, as MCX.EXE stores it (MCX.EXE @ 0x0077c2a0; a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;

    /// <summary>
    /// Per gate overlay (67..74) and team alignment + 1, the overlay the gate behaves as for that team, or -1 when
    /// it is closed to it (cost 20000).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00795480 (read as 0x795394 + (overlay + alignment * 8) * 4); the name is the port's.</remarks>
    constexpr int32_t GateOverlayForAlignment[3][8] = {
        {71, 72, 73, 74, 67, 68, 69, 70},
        {-1, -1, -1, -1, -1, -1, -1, -1},
        {67, 68, 69, 70, 71, 72, 73, 74},
    };

    /// <summary>First overlay type of the gates.</summary>
    constexpr uint32_t FIRST_GATE_OVERLAY = 0x43;
    /// <summary>Last overlay type of the gates.</summary>
    constexpr uint32_t LAST_GATE_OVERLAY = 0x4a;

    /// <summary>The overlay a gate overlay behaves as for a team of <paramref name="alignment"/> (-1 closed).</summary>
    int32_t GateOverlay(uint32_t overlay, int32_t alignment)
    {
        return GateOverlayForAlignment[alignment + 1][overlay - FIRST_GATE_OVERLAY];
    }

    /// <summary>The largest step count <see cref="MovePath::init"/> has seen.</summary>
    /// <remarks>MCX.EXE @ 0x0080801c; the name is the port's.</remarks>
    int32_t maxMovePathSteps = 0;

    /// <summary>Whether a tile's cell (cellR, cellC) is passable (<see cref="MapTile::getCellPassable"/> on a copy).</summary>
    uint32_t TileCellPassable(const MapTile& tile, int32_t cellR, int32_t cellC)
    {
        const uint32_t shift = static_cast<uint32_t>((cellR * MAPCELL_DIM + cellC) * 2);
        return (tile.cells & (0x4000u << shift)) >> (shift + 14);
    }

    /// <summary>A tile's 6-bit elevation level plus the map's base elevation, in meters.</summary>
    float TileElevation(uint32_t cells, int32_t baseElevation)
    {
        return static_cast<float>(static_cast<int32_t>((cells >> 7) & 0x3f) + baseElevation) *
               Terrain::metersPerElevLevel;
    }

    /// <summary>
    /// <see cref="tileColToWorldCoord"/>[tileC]. Port fix: the original reads past the table for a column off the
    /// map; the port computes such a column's edge the way the table was filled.
    /// </summary>
    float TileColToWorldCoord(int32_t tileC, int32_t mapWidth)
    {
        if (tileC >= 0 && tileC < mapWidth)
        {
            return tileColToWorldCoord[tileC];
        }

        return static_cast<float>(tileC) * Terrain::metersPerVertex - worldUnitsMapSide * 0.5f;
    }

    /// <summary>
    /// <see cref="tileRowToWorldCoord"/>[tileR]. Port fix: the original reads past the table for a row off the map;
    /// the port computes such a row's edge the way the table was filled.
    /// </summary>
    float TileRowToWorldCoord(int32_t tileR, int32_t mapHeight)
    {
        if (tileR >= 0 && tileR < mapHeight)
        {
            return tileRowToWorldCoord[tileR];
        }

        return worldUnitsMapSide * 0.5f - static_cast<float>(tileR) * Terrain::metersPerVertex;
    }
}

auto worldCoordToMapCoord(vector_3d pos, int32_t& tileR, int32_t& tileC, int32_t& cellR, int32_t& cellC) -> void
{
    tileC = static_cast<int32_t>(Terrain::OneOvermetersPerVertex * pos.x + VerticesMapSideDivTwo);
    tileR = static_cast<int32_t>((MetersMapSideDivTwo - pos.y) * Terrain::OneOvermetersPerVertex);
    cellC = static_cast<int32_t>((pos.x - TileColToWorldCoord(tileC, GameMap->width)) / MetersPerCell);
    cellR = static_cast<int32_t>((TileRowToWorldCoord(tileR, GameMap->height) - pos.y) / MetersPerCell);
}

auto worldCoordToMapTile(vector_3d pos, int32_t& tileR, int32_t& tileC) -> void
{
    tileC = static_cast<int32_t>(Terrain::OneOvermetersPerVertex * pos.x + VerticesMapSideDivTwo);
    tileR = static_cast<int32_t>((MetersMapSideDivTwo - pos.y) * Terrain::OneOvermetersPerVertex);
}

auto worldCoordToMapCell(vector_3d pos, int32_t& cellR, int32_t& cellC) -> void
{
    cellC = static_cast<int32_t>((MetersMapSideDivTwo + pos.x) / Terrain::metersPerVertexDivMAPCELL_DIM);
    cellR = static_cast<int32_t>((MetersMapSideDivTwo - pos.y) / Terrain::metersPerVertexDivMAPCELL_DIM);
}

auto relativePositionToPoint(vector_3d pos, float angle, float distance, uint32_t flags) -> vector_3d
{
    const int reverse = (flags & 2) != 0;
    const double radians = angle * DEGREES_TO_RADIANS;
    const float reach = -(worldUnitsPerMeter * distance);
    const float pointX = (static_cast<float>(std::sin(radians)) + 0.0f) * reach + pos.x;
    const float pointY = static_cast<float>(std::cos(radians) * reach) + pos.y;

    // Walk from start toward end: from the point back toward pos, or (reverse) from pos out to the point.
    vector_2d start;
    vector_2d end;

    if (reverse)
    {
        start = vector_2d(pos.x, pos.y);
        end = vector_2d(pointX, pointY);
    }
    else
    {
        start = vector_2d(pointX, pointY);
        end = vector_2d(pos.x, pos.y);
    }

    float stepX = end.x - start.x;
    float stepY = end.y - start.y;
    const float length = std::sqrt(stepX * stepX + stepY * stepY);

    if (length != 0.0f)
    {
        stepX = stepX / length;
        stepY = stepY / length;
    }

    const float stepLength = Terrain::metersPerVertex * (1.0f / 3.0f) * 0.5f;
    stepX = stepX * stepLength;
    stepY = stepY * stepLength;

    if (std::sqrt(stepX * stepX + stepY * stepY) == 0.0f)
    {
        return vector_3d(pos.x, pos.y, 0.0f);
    }

    const vector_2d span = end - start;
    const float totalDistance = std::sqrt(span.y * span.y + span.x * span.x);
    vector_2d current = start;
    vector_2d result = start;
    float travelled = 0.0f;
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    // Port fix: the point can be off the map, where the original reads outside it. Off the map is impassable.
    auto passableAt = [&]() -> uint32_t
    {
        GameMap->worldToMapPos(vector_3d(current.x, current.y, 0.0f), tileR, tileC, cellR, cellC);

        if (!GameMap->onMap(tileR, tileC))
        {
            return 0;
        }

        return GameMap->map[GameMap->width * tileR + tileC].getCellPassable(cellR, cellC);
    };

    uint32_t passable = passableAt();

    // Original behaviour (OB-032): the result trails the walk by a step, the last point before the one that ended
    // it (so walking in from an impassable point, the result is still impassable).
    while ((reverse ? passable != 0 : passable == 0) && travelled < totalDistance)
    {
        result = current;
        current.x = stepX + current.x;
        current.y = stepY + current.y;
        travelled =
            std::sqrt((current.x - start.x) * (current.x - start.x) + (current.y - start.y) * (current.y - start.y));
        passable = passableAt();
    }

    const float limit = worldUnitsMapSide * 0.5f - Terrain::metersPerVertex;

    if (result.x < -limit)
    {
        result.x = -limit;
    }

    if (result.x > limit)
    {
        result.x = limit;
    }

    if (result.y < -limit)
    {
        result.y = -limit;
    }

    if (result.y > limit)
    {
        result.y = limit;
    }

    const float elevation = GameMap->getTerrainElevation(vector_3d(result.x, result.y, 0.0f));
    return vector_3d(result.x, result.y, elevation);
}

auto mapTileCellToWorldPos(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, vector_3d& worldPos) -> void
{
    worldPos.z = 0.0f;
    worldPos.x = tileColToWorldCoord[tileC] + cellToWorldCoord[cellC] + HalfMapCell;
    worldPos.y = (tileRowToWorldCoord[tileR] - cellToWorldCoord[cellR]) - HalfMapCell;
}

auto mapCellToWorldPos(int32_t cellR, int32_t cellC, vector_3d& worldPos) -> void
{
    worldPos.z = 0.0f;
    worldPos.x = HalfMapCell + cellColToWorldCoord[cellC];
    worldPos.y = cellRowToWorldCoord[cellR] - HalfMapCell;
}

auto DebugOpenList(char* msg) -> void
{
    File* debugFile = new File;
    debugFile->create("openlist.dbg");
    debugFile->writeString(msg);
    char line[512];

    if (MovingObject != nullptr)
    {
        std::snprintf(line, sizeof(line), "MovingObject = %s [%d]\n", static_cast<Mover*>(MovingObject)->debugStatus,
                      MovingObject->partId);
        debugFile->writeString(line);

        if (MovingObject->objectClass == ELEMENTAL)
        {
            debugFile->writeString("Is an elemental!\n");
        }
    }

    debugFile->writeString("\nOPENLIST INFO\n");
    std::snprintf(line, sizeof(line), "NumItems = %d\n", openList->size());
    debugFile->writeString(line);

    for (int32_t i = 0; i < openList->size(); i++)
    {
        // As the original: items are read from pqList[0] (the sentinel) up, so the last item is left out.
        const PQNode& item = *openList->getItem(i);
        std::snprintf(line, sizeof(line), "Item: %04d\n", i);
        debugFile->writeString(line);
        std::snprintf(line, sizeof(line), "     key: %d\n", item.key);
        debugFile->writeString(line);
        std::snprintf(line, sizeof(line), "      id: %d\n", item.id);
        debugFile->writeString(line);
        std::snprintf(line, sizeof(line), "     row: %d\n", item.row);
        debugFile->writeString(line);
        std::snprintf(line, sizeof(line), "     col: %d\n", item.col);
        debugFile->writeString(line);
    }

    debugFile->close();
    delete debugFile;
}

auto calcTileTypeFromIndex(int32_t tileIndex) -> int32_t
{
    // Forty bands of 79 texture indices (types 1..40), then the smaller special bands.
    for (int32_t band = 1; band <= 40; band++)
    {
        if (tileIndex < band * 0x4f)
        {
            return band;
        }
    }

    static constexpr struct
    {
        int32_t limit;
        int32_t type;
    } bands[] = {
        {0xc6e, 0x29}, {0xc84, 0x2a}, {0xc88, 0x2b}, {0xc96, 0x2c}, {0xca4, 0x2d}, {0xcb0, 0x2e},
        {0xcbc, 0x2f}, {0xcc8, 0x30}, {0xcd4, 0x31}, {0xce0, 0x32}, {0xcec, 0x33}, {0xcf4, 0x34},
        {0xcfc, 0x35}, {0xd15, 0x3a}, {0xd22, 0x36}, {0xd30, 0x38}, {0xd3e, 0x37},
    };

    for (const auto& band : bands)
    {
        if (tileIndex < band.limit)
        {
            return band.type;
        }
    }

    return tileIndex > 0xd65 ? 0 : 2;
}

auto calcOverlayTypeFromIndex(int32_t overlayIndex) -> int32_t
{
    if (overlayIndex == 0x29)
    {
        return 0;
    }

    if ((overlayIndex > 0xd01 && overlayIndex < 0xd0a) || (overlayIndex > 0xe7d && overlayIndex < 0xe82))
    {
        return 0x3b;
    }

    if ((overlayIndex > 0xd09 && overlayIndex < 0xd0e) || (overlayIndex > 0xd65 && overlayIndex < 0xd6e))
    {
        return 0x3e;
    }

    if ((overlayIndex > 0xd0d && overlayIndex < 0xd12) || (overlayIndex > 0xd6d && overlayIndex < 0xd76))
    {
        return 0x3f;
    }

    if (overlayIndex < 0xd51 || (overlayIndex > 0xda7 && overlayIndex < 0xdbc))
    {
        return 0x3c;
    }

    if (overlayIndex < 0xd64 || (overlayIndex > 0xdbb && overlayIndex < 0xdce))
    {
        return 0x3d;
    }

    if (overlayIndex < 0xd82)
    {
        return 0x42;
    }

    if (overlayIndex < 0xd95)
    {
        return 0x40;
    }

    if (overlayIndex < 0xda8)
    {
        return 0x41;
    }

    if (overlayIndex < 0xdcd)
    {
        return 0;
    }

    if (overlayIndex < 0xde1)
    {
        if (overlayIndex < 0xdd1)
        {
            return 1;
        }

        if (overlayIndex < 0xdd4)
        {
            return 2;
        }

        return overlayIndex - 0xdd1;
    }

    if (overlayIndex < 0xdf4)
    {
        if (overlayIndex < 0xde4)
        {
            return 0x10;
        }

        if (overlayIndex < 0xde7)
        {
            return 0x11;
        }
    }
    else if (overlayIndex > 0xdf7)
    {
        if (overlayIndex < 0xe07)
        {
            return 0x25;
        }

        if (overlayIndex < 0xe16)
        {
            return 0x26;
        }

        if (overlayIndex < 0xe25)
        {
            return 0x27;
        }

        if (overlayIndex < 0xe34)
        {
            return 0x28;
        }

        if (overlayIndex < 0xe42)
        {
            return overlayIndex - 0xe0b;
        }

        if (overlayIndex < 0xe51)
        {
            return 0x37;
        }

        if (overlayIndex < 0xe60)
        {
            return 0x38;
        }

        if (overlayIndex < 0xe6f)
        {
            return 0x39;
        }

        return overlayIndex > 0xe7d ? 0 : 0x3a;
    }

    return overlayIndex - 0xdd5;
}

auto ScenarioMap::operator new(size_t size) noexcept -> void*
{
    return systemHeap->malloc(static_cast<uint32_t>(size));
}

auto ScenarioMap::operator delete(void* ptr) -> void
{
    systemHeap->free(ptr);
}

auto ScenarioMap::init(int32_t newWidth, int32_t newHeight) -> void
{
    MetersPerCell = Terrain::metersPerVertexDivMAPCELL_DIM;

    for (int32_t i = 0; i < MAX_MAP_TILE_WIDTH; i++)
    {
        tileMulMAPCELL_DIM[i] = i * MAPCELL_DIM;
    }

    for (int32_t row = 0; row < newHeight; row++)
    {
        tileRowToWorldCoord[row] = worldUnitsMapSide * 0.5f - static_cast<float>(row) * Terrain::metersPerVertex;
    }

    for (int32_t col = 0; col < newWidth; col++)
    {
        tileColToWorldCoord[col] = static_cast<float>(col) * Terrain::metersPerVertex - worldUnitsMapSide * 0.5f;
    }

    const float cellSide = Terrain::metersPerVertex * (1.0f / 3.0f);

    for (int32_t cell = 0; cell < MAPCELL_DIM; cell++)
    {
        cellToWorldCoord[cell] = static_cast<float>(cell) * cellSide;
    }

    width = newWidth;
    height = newHeight;
    VerticesMapSideDivTwo = static_cast<float>(Terrain::verticesBlockSide * Terrain::blocksMapSide) * 0.5f;
    MetersMapSideDivTwo = worldUnitsMapSide * 0.5f;
    MapCellDiagonal = cellSide * metersPerWorldUnit * 1.4142f;
    HalfMapCell = cellSide * 0.5f;

    const uint32_t numTiles = static_cast<uint32_t>(newWidth * newHeight);
    map = static_cast<MapTile*>(systemHeap->malloc(numTiles * sizeof(MapTile)));

    if (map == nullptr)
    {
        Fatal(0, "Not enough Memory for ScenarioMap");
    }

    memclear(map, static_cast<int>(numTiles * sizeof(MapTile)));
    pathMap = static_cast<uint8_t*>(systemHeap->malloc(numTiles));

    if (pathMap == nullptr)
    {
        Fatal(0, " No RAM for pathMap ");
    }

    memclear(pathMap, static_cast<int>(numTiles));
}

auto ScenarioMap::init(File* mapFile) -> int32_t
{
    MetersPerCell = Terrain::metersPerVertexDivMAPCELL_DIM;
    MapCellDiagonal = Terrain::metersPerVertex * (1.0f / 3.0f) * metersPerWorldUnit * 1.4142f;
    HalfMapCell = Terrain::metersPerVertex * (1.0f / 3.0f) * 0.5f;
    VerticesMapSideDivTwo = static_cast<float>((Terrain::verticesBlockSide * Terrain::blocksMapSide) / 2);
    MetersMapSideDivTwo = worldUnitsMapSide * 0.5f;

    height = mapFile->readLong();
    width = mapFile->readLong();

    for (int32_t i = 0; i < MAX_MAP_TILE_WIDTH; i++)
    {
        tileMulMAPCELL_DIM[i] = i * MAPCELL_DIM;
    }

    for (int32_t row = 0; row < height; row++)
    {
        tileRowToWorldCoord[row] = worldUnitsMapSide * 0.5f - static_cast<float>(row) * Terrain::metersPerVertex;
    }

    for (int32_t col = 0; col < width; col++)
    {
        tileColToWorldCoord[col] = static_cast<float>(col) * Terrain::metersPerVertex - worldUnitsMapSide * 0.5f;
    }

    const float cellSide = Terrain::metersPerVertex * (1.0f / 3.0f);

    for (int32_t cell = 0; cell < MAPCELL_DIM; cell++)
    {
        cellToWorldCoord[cell] = static_cast<float>(cell) * cellSide;
    }

    for (int32_t row = 0; row < height * MAPCELL_DIM; row++)
    {
        cellRowToWorldCoord[row] = worldUnitsMapSide * 0.5f - static_cast<float>(row) * MetersPerCell;
    }

    for (int32_t col = 0; col < width * MAPCELL_DIM; col++)
    {
        cellColToWorldCoord[col] = static_cast<float>(col) * MetersPerCell - worldUnitsMapSide * 0.5f;
    }

    baseElevation = mapFile->readLong();
    const uint32_t numTiles = static_cast<uint32_t>(width * height);
    map = static_cast<MapTile*>(systemHeap->malloc(numTiles * sizeof(MapTile)));

    if (map == nullptr)
    {
        Fatal(0, "Not enough Memory for ScenarioMap");
    }

    mapFile->read(reinterpret_cast<uint8_t*>(map), static_cast<int32_t>(numTiles * sizeof(MapTile)));
    pathMap = static_cast<uint8_t*>(systemHeap->malloc(numTiles));

    if (pathMap == nullptr)
    {
        Fatal(0, " No RAM for pathMap ");
    }

    memclear(pathMap, static_cast<int>(numTiles));
    return 0;
}

auto ScenarioMap::init(Scenario*) -> int32_t
{
    return 0;
}

auto ScenarioMap::write(File* mapFile) -> int32_t
{
    mapFile->writeLong(height);
    mapFile->writeLong(width);
    mapFile->writeLong(baseElevation);
    mapFile->write(reinterpret_cast<const uint8_t*>(map), static_cast<int32_t>(width * height * sizeof(MapTile)));
    return 0;
}

auto ScenarioMap::destroy() -> void
{
    if (map != nullptr)
    {
        systemHeap->free(map);
        map = nullptr;
    }

    if (pathMap != nullptr)
    {
        systemHeap->free(pathMap);
        pathMap = nullptr;
    }
}

auto ScenarioMap::worldToMapPos(vector_3d pos, int32_t& tileR, int32_t& tileC, int32_t& cellR, int32_t& cellC) -> void
{
    tileC = static_cast<int16_t>(
        static_cast<int32_t>(std::floor(Terrain::OneOvermetersPerVertex * pos.x + VerticesMapSideDivTwo)));
    tileR = static_cast<int16_t>(
        static_cast<int32_t>(std::floor((MetersMapSideDivTwo - pos.y) * Terrain::OneOvermetersPerVertex)));
    cellC = static_cast<int32_t>((pos.x - TileColToWorldCoord(tileC, width)) / MetersPerCell);
    cellR = static_cast<int32_t>((TileRowToWorldCoord(tileR, height) - pos.y) / MetersPerCell);
}

auto ScenarioMap::worldToMapTilePos(vector_3d pos, int32_t& tileR, int32_t& tileC) -> void
{
    tileC = static_cast<int16_t>(
        static_cast<int32_t>(std::floor(Terrain::OneOvermetersPerVertex * pos.x + VerticesMapSideDivTwo)));
    tileR = static_cast<int16_t>(
        static_cast<int32_t>(std::floor((MetersMapSideDivTwo - pos.y) * Terrain::OneOvermetersPerVertex)));
}

auto ScenarioMap::cellPassable(vector_3d pos) -> int
{
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    worldToMapPos(pos, tileR, tileC, cellR, cellC);

    // Port fix: the original reads outside map for a point off it (the mouse past the terrain edge). Off the map is
    // impassable.
    if (!onMap(tileR, tileC))
    {
        return 0;
    }

    return static_cast<int>(map[width * tileR + tileC].getCellPassable(cellR, cellC));
}

auto ScenarioMap::spreadState(int32_t cellRow, int32_t cellCol, int32_t depth) -> void
{
    if (cellRow < 0 || cellRow >= height * MAPCELL_DIM || cellCol < 0 || cellCol >= width * MAPCELL_DIM || depth <= 0)
    {
        return;
    }

    const int32_t tileR = cellRow / MAPCELL_DIM;
    const int32_t tileC = cellCol / MAPCELL_DIM;
    MapTile& tile = map[width * tileR + tileC];

    if (PreserveMapTiles != 0 && (tile.cells & 0x2000) == 0)
    {
        // Port fix: the original writes past preservedTiles[] once MAX_PRESERVED_TILES tiles are saved.
        if (numPreservedTiles < MAX_PRESERVED_TILES)
        {
            preservedTiles[numPreservedTiles].cells = tile.cells;
            preservedTiles[numPreservedTiles].row = static_cast<int16_t>(tileR);
            preservedTiles[numPreservedTiles].col = static_cast<int16_t>(tileC);
            tile.cells |= 0x2000;
            numPreservedTiles++;
        }
    }

    const uint32_t shift =
        static_cast<uint32_t>(((cellRow - tileR * MAPCELL_DIM) * MAPCELL_DIM + (cellCol - tileC * MAPCELL_DIM)) * 2);
    tile.cells &= ~(0x4000u << shift);

    for (int32_t dir = 0; dir < 8; dir++)
    {
        spreadState(cellRow + rowShift[dir], cellCol + colShift[dir], depth - 1);
    }
}

auto ScenarioMap::placeObject(vector_3d position, float radius) -> int32_t
{
    int32_t cellR = 0;
    int32_t cellC = 0;
    worldCoordToMapCell(position, cellR, cellC);
    float depth = radius / (metersPerWorldUnit * Terrain::metersPerVertexDivMAPCELL_DIM);

    if (depth > 0.5f && depth < 1.0f)
    {
        depth = 1.0f;
    }

    spreadState(cellR, cellC, static_cast<int32_t>(depth));
    return 0;
}

auto ScenarioMap::placeObjects(ObjectQueueNode* objectList) -> int32_t
{
    if (objectList->head == nullptr)
    {
        return 0;
    }

    BaseObject* current = nullptr;

    while (objectList->Traverse(current) != nullptr)
    {
        GameObject* object = static_cast<GameObject*>(current);

        if (object->getUseMe() != 0 && object->getObjectType() != nullptr)
        {
            placeObject(object->getPosition(), object->getObjectType()->extentRadius);
        }
    }

    return 0;
}

auto ScenarioMap::placeTerrainObject(GameObject*) -> void
{
}

auto ScenarioMap::placeTerrainObjects(ObjectBlockManager* blockManager) -> void
{
    PacketFile* objectFile = blockManager->objectFile;
    const int32_t numTiles = width * height;
    // Original behaviour (OB-033): never written (placeTerrainObject does nothing), so the tiles' overlay bits 7-8
    // all end up cleared.
    uint8_t* footprint = static_cast<uint8_t*>(systemHeap->malloc(static_cast<uint32_t>(numTiles)));
    memclear(footprint, numTiles);

    for (int32_t block = 0; block < Terrain::blocksMapSide * Terrain::blocksMapSide; block++)
    {
        if (objectFile == nullptr || objectFile->isOpen() == 0)
        {
            continue;
        }

        objectFile->seekPacket(block);
        const uint32_t packetSize = static_cast<uint32_t>(objectFile->getPacketSize());

        if (packetSize == 0)
        {
            continue;
        }

        uint8_t* data = static_cast<uint8_t*>(systemHeap->malloc(packetSize));

        if (data == nullptr)
        {
            Fatal(0, "Cannot place terrain objects");
        }

        std::memset(data, 0xff, packetSize);
        objectFile->readPacket(block, data);
        const uint32_t numRecords = packetSize / sizeof(ObjData);

        for (uint32_t i = 0; i < numRecords; i++)
        {
            ObjData record;
            std::memcpy(&record, data + i * sizeof(ObjData), sizeof(ObjData));

            if (record.objTypeNum == -1)
            {
                continue;
            }

            GameObject* object = createObject(record.objTypeNum);
            vector_2d position(static_cast<float>(record.pixelOffsetX), static_cast<float>(record.pixelOffsetY));
            vector_2d numbers(static_cast<float>(static_cast<uint16_t>(record.vertexNumber)),
                              static_cast<float>(static_cast<uint16_t>(record.blockNumber)));
            object->setTerrainPosition(position, numbers);
            object->update();
            placeTerrainObject(object);
            delete object;
        }

        systemHeap->free(data);
    }

    for (int32_t row = 0; row < height; row++)
    {
        for (int32_t col = 0; col < width; col++)
        {
            MapTile& tile = map[row * width + col];
            tile.overlay =
                (static_cast<uint32_t>(footprint[row * width + col] & 0xfe) << 6) | (tile.overlay & 0xfffffe7f);
        }
    }

    systemHeap->free(footprint);
}

auto ScenarioMap::updateMovingObjects() -> void
{
    PreserveMapTiles = 1;
    placeObjects(clanMechList);
    placeObjects(innerSphereMechList);
    PreserveMapTiles = 0;
}

auto ScenarioMap::restorePreservedMap() -> void
{
    for (int32_t i = 0; i < numPreservedTiles; i++)
    {
        map[preservedTiles[i].row * width + preservedTiles[i].col].cells = preservedTiles[i].cells;
    }

    numPreservedTiles = 0;
}

auto ScenarioMap::getTerrainElevation(vector_3d position) -> float
{
    const float vertexX =
        Terrain::metersPerVertex * static_cast<float>(std::floor(Terrain::OneOvermetersPerVertex * position.x));
    const float vertexY = Terrain::metersPerVertex *
                          (static_cast<float>(std::floor(Terrain::OneOvermetersPerVertex * position.y)) + 1.0f);
    const float vertexCol = Terrain::OneOvermetersPerVertex * vertexX;
    const float vertexRow = Terrain::OneOvermetersPerVertex * vertexY;
    const int32_t halfSide = (Terrain::blocksMapSide * Terrain::verticesBlockSide) >> 1;
    int32_t tileC = static_cast<int32_t>(std::floor(vertexCol)) + halfSide;
    int32_t tileR = halfSide - static_cast<int32_t>(std::floor(vertexRow));
    const int32_t maxTile = Terrain::blocksMapSide * Terrain::verticesBlockSide - 2;

    if (tileR < 0)
    {
        tileR = 0;
    }

    if (tileR > maxTile)
    {
        tileR = maxTile;
    }

    if (tileC < 0)
    {
        tileC = 0;
    }

    if (tileC > maxTile)
    {
        tileC = maxTile;
    }

    auto inMap = [](int32_t r, int32_t c)
    { return r >= 0 && r < GameMap->height && c >= 0 && c < GameMap->width ? 1 : 0; };
    Assert(inMap(tileR, tileC), 0, " move:terrelev MapTile Out of Bounds ");
    Assert(inMap(tileR + 1, tileC + 1), 0, " move:terrelev2 MapTile Out of Bounds ");
    Assert(inMap(tileR, tileC), 0, " Map Tile out of bounds ");
    const uint32_t cells00 = GameMap->map[GameMap->width * tileR + tileC].cells;
    Assert(inMap(tileR, tileC + 1), 0, " Map Tile out of bounds ");
    const uint32_t cells01 = GameMap->map[GameMap->width * tileR + tileC + 1].cells;
    Assert(inMap(tileR + 1, tileC + 1), 0, " Map Tile out of bounds ");
    const uint32_t cells11 = GameMap->map[GameMap->width * (tileR + 1) + tileC + 1].cells;
    Assert(inMap(tileR + 1, tileC), 0, " Map Tile out of bounds ");
    uint32_t cells10 = GameMap->map[GameMap->width * (tileR + 1) + tileC].cells;

    const int32_t base = GameMap->baseElevation;
    const float cornerX = static_cast<float>(std::floor(vertexCol)) * Terrain::metersPerVertex;
    const float cornerY = static_cast<float>(std::floor(vertexRow)) * Terrain::metersPerVertex;
    const float elevation00 = TileElevation(cells00, base);
    const float dx = std::fabs(position.x - vertexX);
    const float dy = std::fabs(vertexY - position.y);

    // The two edges of the tile's triangle holding the point, from its upper-left corner.
    float edge1X;
    float edge1Y;
    const float edge2Y = (cornerY - Terrain::metersPerVertex) - cornerY;

    if (dx <= dy)
    {
        edge1X = 0.0f;
        edge1Y = edge2Y;
    }
    else
    {
        edge1X = (cornerX + Terrain::metersPerVertex) - cornerX;
        edge1Y = 0.0f;
        cells10 = cells01;
    }

    float edge1Z = TileElevation(cells10, base) - elevation00;
    float edge2Z = TileElevation(cells11, base) - elevation00;
    float edge2X = (cornerX + Terrain::metersPerVertex) - cornerX;
    float edge2YN = edge2Y;

    float length = std::sqrt(edge1X * edge1X + edge1Z * edge1Z + edge1Y * edge1Y);

    if (length != 0.0f)
    {
        edge1X = edge1X / length;
        edge1Y = edge1Y / length;
        edge1Z = edge1Z / length;
    }

    length = std::sqrt(edge2Z * edge2Z + edge2X * edge2X + edge2YN * edge2YN);

    if (length != 0.0f)
    {
        edge2X = edge2X / length;
        edge2YN = edge2YN / length;
        edge2Z = edge2Z / length;
    }

    float normalX = edge2Z * edge1Y - edge2YN * edge1Z;
    float normalY = edge1Z * edge2X - edge2Z * edge1X;
    float normalZ = edge2YN * edge1X - edge1Y * edge2X;

    if (normalZ == 0.0f)
    {
        Fatal(0, " Vertical Terrain ");
    }

    if (normalZ < 0.0f)
    {
        normalX = -normalX;
        normalY = -normalY;
        normalZ = -normalZ;
    }

    return -((normalX / normalZ) * dx + -dy * (normalY / normalZ)) + elevation00;
}

auto ScenarioMap::getLOS(vector_3d position) -> int32_t
{
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    worldToMapPos(position, tileR, tileC, cellR, cellC);

    // Port fix: a line walked toward a point off the map reads outside it in the original. Off the map blocks.
    if (!onMap(tileR, tileC))
    {
        return 0;
    }

    const uint32_t shift = static_cast<uint32_t>((cellR * MAPCELL_DIM + cellC) * 2);
    return ((map[width * tileR + tileC].cells & (0x8000u << shift)) >> (shift + 15)) != 0 ? 1 : 0;
}

auto ScenarioMap::getInnerSphereMine(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC) -> uint32_t
{
    const uint32_t layout = (map[width * tileR + tileC].overlay >> 11) & 3;
    return static_cast<uint32_t>(static_cast<int32_t>(mineLayout[layout][cellR * MAPCELL_DIM + cellC]));
}

auto ScenarioMap::getClanMine(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC) -> uint32_t
{
    const uint32_t layout = (map[width * tileR + tileC].overlay >> 13) & 3;
    return static_cast<uint32_t>(static_cast<int32_t>(mineLayout[layout][cellR * MAPCELL_DIM + cellC]));
}

auto ScenarioMap::getLOF(vector_3d position) -> int32_t
{
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    worldToMapPos(position, tileR, tileC, cellR, cellC);

    if (position.z < getTerrainElevation(position))
    {
        return 0;
    }

    // Port fix: the original reads outside the map for a point off it. Off the map blocks.
    if (!onMap(tileR, tileC))
    {
        return 0;
    }

    return map[width * tileR + tileC].getCellPassable(cellR, cellC) != 0 ? 1 : 0;
}

auto ScenarioMap::lineOfSight(vector_3d start, vector_3d target) -> int
{
    vector_3d step = target - start;
    step.normalize();
    const float stepLength = metersPerWorldUnit * Terrain::metersPerVertexDivMAPCELL_DIM * 0.33f;
    step.x = step.x * stepLength;
    step.y = step.y * stepLength;
    step.z = step.z * stepLength;
    const float totalDistance = (start - target).magnitude() * metersPerWorldUnit;
    updateMovingObjects();

    vector_3d current = start + step;
    float distance = (current - start).magnitude() * metersPerWorldUnit;
    int result = 1;

    while (distance < totalDistance)
    {
        if (getLOS(current) == 0)
        {
            result = 0;
        }

        current.x = current.x + step.x;
        current.y = current.y + step.y;
        current.z = current.z + step.z;
        distance = (current - start).magnitude() * metersPerWorldUnit;

        if (result == 0)
        {
            break;
        }
    }

    restorePreservedMap();
    return result;
}

auto ScenarioMap::lineOfFire(vector_3d start, vector_3d target) -> int
{
    float stepX = target.x - start.x;
    float stepY = target.y - start.y;
    const float length = std::sqrt(stepX * stepX + stepY * stepY);

    if (length != 0.0f)
    {
        stepX = stepX / length;
        stepY = stepY / length;
    }

    stepX = stepX * Terrain::metersPerVertexDivMAPCELL_DIM * 0.33f;
    stepY = stepY * Terrain::metersPerVertexDivMAPCELL_DIM * 0.33f;
    const float totalDistance =
        std::sqrt((start.y - target.y) * (start.y - target.y) + (start.x - target.x) * (start.x - target.x));
    float currentX = stepX + start.x;
    float currentY = stepY + start.y;
    int result = 1;

    do
    {
        const float distanceSq =
            (currentX - start.x) * (currentX - start.x) + (currentY - start.y) * (currentY - start.y);

        if (totalDistance <= std::sqrt(distanceSq))
        {
            return result;
        }

        if (getLOS(vector_3d(currentX, currentY, 0.0f)) == 0)
        {
            result = 0;
        }

        currentX = currentX + stepX;
        currentY = currentY + stepY;
    } while (result != 0);

    return result;
}

auto ScenarioMap::lineOfSensor(vector_3d start, vector_3d target, int32_t& numBlockingTiles,
                               int32_t& numBlockingObjects) -> void
{
    vector_3d step = target - start;
    const float length = step.magnitude();

    if (length != 0.0f)
    {
        step.x = step.x / length;
        step.y = step.y / length;
        step.z = step.z / length;
    }

    const float stepLength = metersPerWorldUnit * Terrain::metersPerVertexDivMAPCELL_DIM * 2.0f;
    step.x = step.x * stepLength;
    step.y = step.y * stepLength;
    step.z = step.z * stepLength;
    const float totalDistance = (start - target).magnitude() * metersPerWorldUnit;
    updateMovingObjects();

    vector_3d current = start + step;
    vector_3d travelled = current - start;
    int32_t prevTileR = 0;
    int32_t prevTileC = 0;
    worldToMapTilePos(start, prevTileR, prevTileC);
    numBlockingTiles = 0;
    numBlockingObjects = 0;

    if (travelled.magnitude() * metersPerWorldUnit < totalDistance)
    {
        do
        {
            int32_t tileR = 0;
            int32_t tileC = 0;
            worldToMapTilePos(current, tileR, tileC);

            // Port fix: the original counts blockers on tiles off the map too, reading outside it.
            if ((tileR != prevTileR || tileC != prevTileC) && onMap(tileR, tileC))
            {
                if (getTerrainElevation(current) > current.z)
                {
                    numBlockingTiles++;
                }

                numBlockingObjects += GameObjectMap->getNumSensorBlockingObjects(tileR, tileC);
                numBlockingObjects += (map[width * tileR + tileC].overlay & 0x1000000) == 0x1000000 ? 1 : 0;
                prevTileR = tileR;
                prevTileC = tileC;
            }

            current.x = current.x + step.x;
            current.y = current.y + step.y;
            current.z = step.z + current.z;
            travelled = current - start;
        } while (travelled.magnitude() * metersPerWorldUnit < totalDistance);
    }

    restorePreservedMap();
}

auto ScenarioMap::print(char* fileName, int32_t ULr, int32_t ULc, int32_t printHeight, int32_t printWidth) -> void
{
    File* debugFile = new File;
    debugFile->create(fileName);

    for (int32_t row = ULr; row < ULr + printHeight; row++)
    {
        char line[512];
        line[0] = '\0';

        for (int32_t col = ULc; col < ULc + printWidth; col++)
        {
            const char* cell = (map[width * row + col].cells & 0x55554000) != 0 ? "." : "X";
            std::strcat(line, cell);
        }

        std::strcat(line, "\n");
        debugFile->writeString(line);
    }

    debugFile->writeString("\n");
    debugFile->close();
    delete debugFile;
}

auto ScenarioMap::inBounds(int32_t tileR, int32_t tileC) -> int
{
    return tileR >= 0 && tileR < height && tileC >= 0 && tileC < width ? 1 : 0;
}

auto ScenarioMap::getTile(int32_t tileR, int32_t tileC) -> MapTile
{
    Assert(tileR >= 0 && tileR < height && tileC >= 0 && tileC < width ? 1 : 0, 0, " Map Tile out of bounds ");
    return map[width * tileR + tileC];
}

auto ScenarioMap::getOverlayWeight(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, Mover* mover) -> int32_t
{
    const uint32_t overlay = map[width * tileR + tileC].overlay & 0x7f;

    if (overlay == 0)
    {
        return 0;
    }

    const int32_t level = mover->getOverlayWeightClass();

    if (overlay >= FIRST_GATE_OVERLAY && overlay <= LAST_GATE_OVERLAY)
    {
        const int32_t gate = GateOverlay(overlay, mover->getAlignment());

        if (gate == -1)
        {
            return 20000;
        }

        return OverlayWeightTable[OverlayWeightIndex[gate] + level * OVERLAY_WEIGHT_LEVEL_SIZE + cellC +
                                  cellR * MAPCELL_DIM];
    }

    return OverlayWeightTable[OverlayWeightIndex[overlay] + level * OVERLAY_WEIGHT_LEVEL_SIZE + cellC +
                              cellR * MAPCELL_DIM];
}

auto ObjectMap::operator new(size_t size) noexcept -> void*
{
    return systemHeap->malloc(static_cast<uint32_t>(size));
}

auto ObjectMap::operator delete(void* ptr) -> void
{
    systemHeap->free(ptr);
}

auto ObjectMap::init(ScenarioMap* newMap) -> void
{
    map = newMap;
    width = newMap->width;
    height = newMap->height;
    rows = static_cast<ObjectPosition**>(systemHeap->malloc(static_cast<uint32_t>(height * sizeof(ObjectPosition*))));

    if (rows == nullptr)
    {
        Fatal(0, "Not enough Memory for ObjectMap");
    }

    memclear(rows, static_cast<int>(height * sizeof(ObjectPosition*)));
}

namespace
{
    /// <summary>Links <paramref name="node"/> into its row's list, before the first node at or past its column.</summary>
    /// <remarks>The list insertion shared by ObjectMap::addObject and updateObject (inlined in both).</remarks>
    void InsertObjectPosition(ObjectPosition** rows, ObjectPosition* node)
    {
        ObjectPosition*& head = rows[node->tileR];
        ObjectPosition* current = head;

        if (current == nullptr)
        {
            node->prev = nullptr;
            node->next = nullptr;
            head = node;
            return;
        }

        if (current->tileC < node->tileC)
        {
            while (current->next != nullptr)
            {
                Assert(current != current->next ? 1 : 0, 0, " Bad ObjPosition Next ");
                current = current->next;

                if (current->tileC >= node->tileC)
                {
                    break;
                }
            }
        }

        if (current->tileC < node->tileC)
        {
            node->prev = current;
            node->next = nullptr;
            current->next = node;
            return;
        }

        node->next = current;
        node->prev = current->prev;
        current->prev = node;

        if (node->prev == nullptr)
        {
            head = node;
        }
        else
        {
            node->prev->next = node;
        }
    }

    /// <summary>Unlinks <paramref name="node"/> from its row's list.</summary>
    void UnlinkObjectPosition(ObjectPosition** rows, ObjectPosition* node)
    {
        if (node->prev == nullptr)
        {
            rows[node->tileR] = node->next;
        }
        else
        {
            node->prev->next = node->next;
        }

        if (node->next != nullptr)
        {
            node->next->prev = node->prev;
        }
    }

    /// <summary>Frees a row's nodes from <paramref name="node"/> on, clearing their objects' positions.</summary>
    /// <remarks>MCX.EXE @ 0x006bba10 (unnamed; recursive).</remarks>
    void FreeObjectPositions(ObjectPosition* node)
    {
        if (node == nullptr)
        {
            return;
        }

        FreeObjectPositions(node->next);

        if (node->object != nullptr)
        {
            node->object->setObjPosition(nullptr);
        }

        systemHeap->free(node);
    }
}

auto ObjectMap::addObject(GameObject* object) -> void
{
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    GameMap->worldToMapPos(object->getPosition(), tileR, tileC, cellR, cellC);
    ObjectPosition* node = static_cast<ObjectPosition*>(systemHeap->malloc(sizeof(ObjectPosition)));

    if (node == nullptr)
    {
        Fatal(0, " No RAM for ObjectPosition ");
    }

    node->object = object;
    node->tileR = tileR;
    node->tileC = tileC;
    node->cellR = cellR;
    node->cellC = cellC;
    node->prev = nullptr;
    node->next = nullptr;
    object->setObjPosition(node);
    InsertObjectPosition(rows, node);
}

auto ObjectMap::updateObject(GameObject* object, int) -> int
{
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    GameMap->worldToMapPos(object->getPosition(), tileR, tileC, cellR, cellC);

    char message[1024];
    std::snprintf(message, sizeof(message),
                  "Bad Cell - Object: %d   Positionx: %f   Positiony: %f\t CRow: %d   CCol: %d",
                  object->getObjectType()->objTypeNum, static_cast<double>(object->getPosition().x),
                  static_cast<double>(object->getPosition().y), cellR, cellC);
    Assert(cellR >= 0 && cellR <= 2 ? 1 : 0, static_cast<uint32_t>(cellR), message);
    Assert(cellC >= 0 && cellC <= 2 ? 1 : 0, static_cast<uint32_t>(cellC), message);

    if (tileR < 0 || tileR >= GameMap->height || tileC < 0 || tileC >= GameMap->width)
    {
        char offMap[1024];
        std::snprintf(offMap, sizeof(offMap), "Object: %d   Positionx: %f   Positiony: %f\t TRow: %d   TCol: %d",
                      object->getObjectType()->objTypeNum, static_cast<double>(object->getPosition().x),
                      static_cast<double>(object->getPosition().y), tileR, tileC);
        const ObjectClass objectClass = object->objectClass;
        const bool isMover = objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
                             objectClass == MOVER;
        Assert(isMover ? 0 : 1, 0, offMap);
        removeObject(object);
        return 0;
    }

    Assert(tileR >= 0 && tileR < height ? 1 : 0, 0, " Object moved off map ");
    Assert(tileC >= 0 && tileC < width ? 1 : 0, 0, " Object moved off map ");

    ObjectPosition* node = object->getObjPosition();

    if (tileR != node->tileR || tileC != node->tileC)
    {
        UnlinkObjectPosition(rows, node);
        node->tileR = tileR;
        node->tileC = tileC;
        InsertObjectPosition(rows, node);
    }

    node->cellR = cellR;
    node->cellC = cellC;
    node->mapCellR = tileMulMAPCELL_DIM[tileR] + cellR;
    node->mapCellC = tileMulMAPCELL_DIM[tileC] + cellC;
    return 1;
}

auto ObjectMap::removeObject(GameObject* object) -> void
{
    ObjectPosition* node = object->getObjPosition();

    if (node != nullptr)
    {
        UnlinkObjectPosition(rows, node);
    }

    systemHeap->free(node);
    object->setObjPosition(nullptr);
}

auto ObjectMap::getNumObjects(int32_t tileR, int32_t tileC) -> int32_t
{
    int32_t count = 0;
    ObjectPosition* node = GameObjectMap->rows[tileR];

    if (node == nullptr)
    {
        return 0;
    }
    while (node->tileC < tileC)
    {
        node = node->next;

        if (node == nullptr)
        {
            return count;
        }
    }

    for (; node != nullptr; node = node->next)
    {
        if (tileC + 1 <= node->tileC)
        {
            return count;
        }

        if (node->object != nullptr)
        {
            count++;
        }
    }

    return count;
}

auto ObjectMap::getNumSensorBlockingObjects(int32_t tileR, int32_t tileC) -> int32_t
{
    int32_t count = 0;
    ObjectPosition* node = GameObjectMap->rows[tileR];

    if (node == nullptr)
    {
        return 0;
    }
    while (node->tileC < tileC)
    {
        node = node->next;

        if (node == nullptr)
        {
            return count;
        }
    }

    for (; node != nullptr; node = node->next)
    {
        if (tileC + 1 <= node->tileC)
        {
            return count;
        }

        if (node->object != nullptr && node->object->objectClass != TREE)
        {
            count++;
        }
    }

    return count;
}

auto ObjectMap::destroy() -> void
{
    for (int32_t row = 0; row < height; row++)
    {
        if (rows[row] != nullptr)
        {
            FreeObjectPositions(rows[row]);
            rows[row] = nullptr;
        }
    }

    systemHeap->free(rows);
    rows = nullptr;
}

auto cellDirToCell(int32_t fromTileR, int32_t fromTileC, int32_t fromCellR, int32_t fromCellC, int32_t toTileR,
                   int32_t toTileC, int32_t toCellR, int32_t toCellC) -> int32_t
{
    /// <remarks>MCX.EXE @ 0x00795880 (static deltaDir of cellDirToCell).</remarks>
    static const int32_t deltaDir[3][3] = {{7, 0, 1}, {6, -1, 2}, {5, 4, 3}};
    const int32_t rowDelta = tileMulMAPCELL_DIM[toTileR] + toCellR - tileMulMAPCELL_DIM[fromTileR] - fromCellR + 1;
    const int32_t colDelta = tileMulMAPCELL_DIM[toTileC] + toCellC - tileMulMAPCELL_DIM[fromTileC] - fromCellC + 1;

    if (rowDelta < 0 || rowDelta > 2 || colDelta < 0 || colDelta > 2)
    {
        return -2;
    }

    const int32_t dir = deltaDir[rowDelta][colDelta];
    return dir == -1 ? -2 : dir;
}

auto DebugMoveChunk(Mover* mover, MoveChunk* chunk1, MoveChunk* chunk2) -> void
{
    char line[512];
    ChunkDebugMsg[0] = '\0';

    if (mover != nullptr)
    {
        std::snprintf(line, sizeof(line), "Mover = %s (%d)\n", mover->debugStatus, mover->partId);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "Mover World Pos = (%.4f, %.4f, %.4f)\n",
                      static_cast<double>(mover->getPosition().x), static_cast<double>(mover->getPosition().y),
                      static_cast<double>(mover->getPosition().z));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "Mover Obj Pos = [%d, %d, %d, %d]\n", mover->getObjPosition()->tileR,
                      mover->getObjPosition()->tileC, mover->getObjPosition()->cellR, mover->getObjPosition()->cellC);
        std::strcat(ChunkDebugMsg, line);

        if (mover->getPilot() == nullptr)
        {
            std::strcat(ChunkDebugMsg, "NULL pilot!\n");
        }

        if (mover->objectClass == BATTLEMECH && static_cast<BattleMech*>(mover)->inJump != 0)
        {
            int32_t tileR = 0;
            int32_t tileC = 0;
            int32_t cellR = 0;
            int32_t cellC = 0;
            worldCoordToMapCoord(static_cast<BattleMech*>(mover)->jumpGoal, tileR, tileC, cellR, cellC);
            std::snprintf(line, sizeof(line), "Jumping to [%d, %d, %d, %d]\n", tileR, tileC, cellR, cellC);
            std::strcat(ChunkDebugMsg, line);
        }

        std::strcat(ChunkDebugMsg, "\n");
    }

    auto writeChunk = [&line](const char* title, const MoveChunk* chunk)
    {
        std::strcat(ChunkDebugMsg, title);

        for (int32_t i = 0; i < MOVECHUNK_NUM_STEPS; i++)
        {
            std::snprintf(line, sizeof(line), "stepPos[%d] = (%d, %d, %d, %d)\n", i, chunk->stepPos[i][0],
                          chunk->stepPos[i][1], chunk->stepPos[i][2], chunk->stepPos[i][3]);
            std::strcat(ChunkDebugMsg, line);
        }

        std::snprintf(line, sizeof(line), "stepRelPos = %d, %d, %d\n", chunk->stepRelPos[0], chunk->stepRelPos[1],
                      chunk->stepRelPos[2]);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "numSteps = %d\n", chunk->numSteps);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "run = %c\n", chunk->run != 0 ? 'T' : 'F');
        std::strcat(ChunkDebugMsg, line);
    };

    if (chunk1 != nullptr)
    {
        writeChunk("CHUNK1\n", chunk1);
    }

    if (chunk2 != nullptr)
    {
        writeChunk("\nCHUNK2\n", chunk2);
    }

    File* debugFile = new File;
    debugFile->create("mvchunk.dbg");
    debugFile->writeString(ChunkDebugMsg);
    debugFile->close();
    delete debugFile;
    ExceptionGameMsg = ChunkDebugMsg;
}

auto MoveChunk::operator new(size_t size) noexcept -> void*
{
    if (systemHeap != nullptr)
    {
        return systemHeap->malloc(static_cast<uint32_t>(size));
    }

    return std::malloc(size);
}

auto MoveChunk::operator delete(void* ptr) -> void
{
    if (systemHeap != nullptr)
    {
        systemHeap->free(ptr);
        return;
    }

    std::free(ptr);
}

namespace
{
    /// <summary>
    /// Copies path step <paramref name="step"/> into chunk step <paramref name="index"/>, with its direction: from
    /// the start cell for the first step, else the path step's own.
    /// </summary>
    /// <remarks>Inlined three times in MoveChunk::build.</remarks>
    void CopyChunkStep(MoveChunk* chunk, int32_t index, const PathStep& step)
    {
        int32_t* pos = chunk->stepPos[index];
        pos[0] = step.tileR;
        pos[1] = step.tileC;
        pos[2] = step.cellR;
        pos[3] = step.cellC;

        if (index == 1)
        {
            const int32_t* start = chunk->stepPos[0];
            chunk->stepRelPos[0] =
                cellDirToCell(start[0], start[1], start[2], start[3], pos[0], pos[1], pos[2], pos[3]);
        }
        else
        {
            chunk->stepRelPos[index - 1] = static_cast<int8_t>(step.direction);
        }
    }

    /// <summary>Makes the chunk a single step at the mover's cell.</summary>
    void SetChunkAtMover(MoveChunk* chunk, Mover* mover)
    {
        const ObjectPosition* objPosition = mover->getObjPosition();
        chunk->stepPos[0][0] = objPosition->tileR;
        chunk->stepPos[0][1] = objPosition->tileC;
        chunk->stepPos[0][2] = objPosition->cellR;
        chunk->stepPos[0][3] = objPosition->cellC;
        chunk->stepRelPos[0] = 0;
        chunk->stepRelPos[1] = 0;
        chunk->stepRelPos[2] = 0;
        chunk->numSteps = 1;
    }
}

auto MoveChunk::build(Mover* mover, MovePath* path1, MovePath* path2) -> void
{
    SetChunkAtMover(this, mover);

    if (path1 != nullptr && path1->numSteps > 0)
    {
        const int32_t pathSteps = path1->numSteps;
        const int32_t curStep = path1->curStep;
        int32_t index = 1;
        int roomLeft = 1;

        if (curStep < pathSteps)
        {
            const PathStep& current = path1->stepList[curStep];
            int onCurrentStep = stepPos[0][0] == current.tileR && stepPos[0][1] == current.tileC &&
                                stepPos[0][2] == current.cellR && stepPos[0][3] == current.cellC;

            if (!onCurrentStep && cellDirToCell(stepPos[0][0], stepPos[0][1], stepPos[0][2], stepPos[0][3],
                                                current.tileR, current.tileC, current.cellR, current.cellC) == -2)
            {
                onCurrentStep = 1; // not next to the current step: start the chunk from it
            }

            int32_t firstStep;
            int32_t stepsLeft;

            if (onCurrentStep)
            {
                stepPos[0][0] = current.tileR;
                stepPos[0][1] = current.tileC;
                stepPos[0][2] = current.cellR;
                stepPos[0][3] = current.cellC;
                firstStep = curStep + 1;
                stepsLeft = pathSteps - firstStep;
            }
            else
            {
                firstStep = curStep;
                stepsLeft = pathSteps - curStep;
            }

            numSteps = MOVECHUNK_NUM_STEPS - 1;
            roomLeft = stepsLeft < MOVECHUNK_NUM_STEPS - 1;

            if (roomLeft)
            {
                numSteps = stepsLeft;
            }

            for (int32_t i = 0; i < numSteps; i++)
            {
                CopyChunkStep(this, index++, path1->stepList[firstStep + i]);
            }

            numSteps++;
        }

        const int nextLeg = path2 != nullptr && path1->globalStep >= 0 && path2->globalStep == path1->globalStep + 1;

        if (roomLeft && nextLeg && path2->numStepsWhenNotPaused > 0 && numSteps < MOVECHUNK_NUM_STEPS)
        {
            int32_t extraSteps = MOVECHUNK_NUM_STEPS - numSteps;

            if (path2->numSteps < extraSteps)
            {
                extraSteps = path2->numSteps;
            }

            for (int32_t i = 0; i < extraSteps; i++)
            {
                Assert(index < MOVECHUNK_NUM_STEPS ? 1 : 0, static_cast<uint32_t>(index),
                       " MoveChunk.build: path2 and bad curStep > MOVECHUNK_NUM_STEPS ");

                // Port fix: the original writes past stepPos[] when the assert above fails.
                if (index >= MOVECHUNK_NUM_STEPS)
                {
                    break;
                }

                CopyChunkStep(this, index++, path2->stepList[i]);
            }

            numSteps = extraSteps + numSteps;
            Assert(numSteps < MOVECHUNK_NUM_STEPS + 1 ? 1 : 0, static_cast<uint32_t>(index),
                   " MoveChunk.build: path2 and bad curStep > MOVECHUNK_NUM_STEPS ");
        }
    }

    if (numSteps < 1 || numSteps > MOVECHUNK_NUM_STEPS)
    {
        SetChunkAtMover(this, mover);
    }

    run = mover->getPilot()->moveOrders.run;

    if (run != 0 && mover->objectClass == BATTLEMECH)
    {
        run = static_cast<BattleMech*>(mover)->legStatus == 0 ? 1 : 0;
    }

    data = 0;
}

auto MoveChunk::build(Mover*, vector_3d jumpGoal) -> void
{
    worldCoordToMapCoord(jumpGoal, stepPos[0][0], stepPos[0][1], stepPos[0][2], stepPos[0][3]);
    stepRelPos[0] = 0;
    stepRelPos[1] = 0;
    stepRelPos[2] = 0;
    numSteps = 1;
    run = 0;
}

auto MoveChunk::pack(Mover* mover) -> void
{
    const int32_t stepCount = numSteps;
    uint32_t packed = static_cast<uint32_t>(tileMulMAPCELL_DIM[stepPos[0][1]] + stepPos[0][3]) << 3 |
                      static_cast<uint32_t>(tileMulMAPCELL_DIM[stepPos[0][0]] + stepPos[0][2]) << 13 |
                      static_cast<uint32_t>(stepCount * 2 - 2);

    if (run != 0)
    {
        packed |= 1;
    }

    // Original behaviour (OB-029): a direction of -2 (MoveChunk::build found no neighbour) spills into the upper
    // bits.
    packed = ((packed << 3 | static_cast<uint32_t>(stepRelPos[0])) << 3 | static_cast<uint32_t>(stepRelPos[1])) << 3 |
             static_cast<uint32_t>(stepRelPos[2]);
    data = packed;

    if (stepCount < 1 || stepCount > MOVECHUNK_NUM_STEPS)
    {
        DebugMoveChunk(mover, this, nullptr);
        char message[1024];
        std::snprintf(message, sizeof(message), " MoveChunk.pack: bad numSteps %d (save mvchunk.dbg file) ", numSteps);
        Assert(0, static_cast<uint32_t>(numSteps), message);
    }
}

auto MoveChunk::unpack(Mover* mover) -> void
{
    MoveChunkUnpackErr = 0;
    const uint32_t packed = data;
    stepRelPos[2] = static_cast<int32_t>(packed & 7);
    stepRelPos[1] = static_cast<int32_t>((packed >> 3) & 7);
    stepRelPos[0] = static_cast<int32_t>((packed >> 6) & 7);
    numSteps = static_cast<int32_t>((packed >> 10) & 3) + 1;
    run = static_cast<int32_t>((packed >> 9) & 1);
    const uint32_t cellCol = (packed >> 12) & 0x3ff;
    stepPos[0][1] = static_cast<int32_t>(cellCol / 3);
    stepPos[0][3] = static_cast<int32_t>(cellCol) - tileMulMAPCELL_DIM[cellCol / 3];
    const uint32_t cellRow = packed >> 22;
    data = cellRow; // as the original: the packed word is left holding the start cell row
    stepPos[0][0] = static_cast<int32_t>(cellRow / 3);
    stepPos[0][2] = static_cast<int32_t>(cellRow) - tileMulMAPCELL_DIM[cellRow / 3];

    if (numSteps < 1 || numSteps > MOVECHUNK_NUM_STEPS)
    {
        MoveChunkUnpackErr = 1;
        return;
    }

    for (int32_t i = 0; i < numSteps - 1; i++)
    {
        if (i < 0 || i > 2)
        {
            MoveChunkUnpackErr = 2;
            return;
        }

        const int32_t dir = stepRelPos[i];

        if (dir < 0 || dir > 7)
        {
            MoveChunkUnpackErr = 3;
            return;
        }

        const int32_t* adj = adjCellTable[stepPos[i][2] * MAPCELL_DIM + stepPos[i][3]][dir];
        stepPos[i + 1][0] = adj[0] + stepPos[i][0];
        stepPos[i + 1][1] = adj[1] + stepPos[i][1];
        stepPos[i + 1][2] = adj[2];
        stepPos[i + 1][3] = adj[3];
    }

    if (numSteps < 1 || numSteps > MOVECHUNK_NUM_STEPS)
    {
        DebugMoveChunk(mover, this, nullptr);
        char message[1024];
        std::snprintf(message, sizeof(message), " MoveChunk.unpack: bad numSteps %d (save mvchunk.dbg file) ",
                      numSteps);
        Assert(0, static_cast<uint32_t>(numSteps), message);
    }
}

auto MoveChunk::equalTo(Mover* mover, MoveChunk* chunk) -> int
{
    if (numSteps != chunk->numSteps || run != chunk->run)
    {
        DebugMoveChunk(mover, this, chunk);
        return 0;
    }

    for (int32_t i = 0; i < numSteps; i++)
    {
        for (int32_t j = 0; j < 4; j++)
        {
            if (stepPos[i][j] != chunk->stepPos[i][j])
            {
                DebugMoveChunk(mover, this, chunk);
                return 0;
            }
        }
    }

    for (int32_t i = 0; i < numSteps - 1; i++)
    {
        if (stepRelPos[i] != chunk->stepRelPos[i])
        {
            DebugMoveChunk(mover, this, chunk);
            return 0;
        }
    }

    return 1;
}

auto MovePath::operator new(size_t size) noexcept -> void*
{
    if (systemHeap != nullptr)
    {
        return systemHeap->malloc(static_cast<uint32_t>(size));
    }

    return std::malloc(size);
}

auto MovePath::operator delete(void* ptr) -> void
{
    if (systemHeap != nullptr)
    {
        systemHeap->free(ptr);
        return;
    }

    std::free(ptr);
}

auto MovePath::init(int32_t newNumSteps) -> int32_t
{
    numStepsWhenNotPaused = newNumSteps;
    numSteps = newNumSteps;

    if (maxMovePathSteps < newNumSteps)
    {
        maxMovePathSteps = newNumSteps;
        return newNumSteps;
    }

    return -1;
}

auto MovePath::clear() -> void
{
    if (numSteps > 0)
    {
        unmark();
    }

    goal.zero();
    numSteps = 0;
    numStepsWhenNotPaused = 0;
    curStep = 0;
    cost = 0;
    marked = 0;
    globalStep = -1;
}

auto MovePath::destroy() -> void
{
    numSteps = 0;
}

auto MovePath::getDistanceLeft(vector_3d position, int32_t fromStep) -> float
{
    if (fromStep == -1)
    {
        fromStep = curStep;
    }

    const PathStep& step = stepList[fromStep];
    return std::sqrt((position.x - step.destination.x) * (position.x - step.destination.x) +
                     (position.z - step.destination.z) * (position.z - step.destination.z) +
                     (position.y - step.destination.y) * (position.y - step.destination.y)) *
               metersPerWorldUnit +
           step.distanceToGoal;
}

auto MovePath::mark() -> void
{
    if (marked != 0)
    {
        return;
    }

    for (int32_t i = 0; i < numSteps; i++)
    {
        GameMap->pathMap[GameMap->width * stepList[i].tileR + stepList[i].tileC]++;
        Assert(1, 0, " Negative pathMap Count ");
    }

    marked = 1;
}

auto MovePath::unmark() -> void
{
    if (marked == 0)
    {
        return;
    }

    for (int32_t i = 0; i < numSteps; i++)
    {
        GameMap->pathMap[GameMap->width * stepList[i].tileR + stepList[i].tileC]--;
        Assert(1, 0, " Negative pathMap Count ");
    }

    marked = 0;
}

auto MovePath::lock(int32_t start, int32_t range, uint32_t setting) -> void
{
    if (start == -1)
    {
        start = curStep;
    }

    int32_t end = start + range;

    if (numStepsWhenNotPaused <= end)
    {
        end = numStepsWhenNotPaused;
    }

    for (int32_t i = start; i < end; i++)
    {
        GameMap->setCellPathLocked(stepList[i].tileR, stepList[i].tileC, stepList[i].cellR, stepList[i].cellC, setting);
    }
}

auto MovePath::isLocked(int32_t start, int32_t range, int* reachedEnd) -> int
{
    if (start == -1)
    {
        start = curStep;
    }

    if (reachedEnd != nullptr)
    {
        *reachedEnd = 0;
    }

    int32_t end = range + start;

    if (numStepsWhenNotPaused <= end)
    {
        end = numStepsWhenNotPaused;

        if (reachedEnd != nullptr)
        {
            *reachedEnd = 1;
        }
    }

    for (int32_t i = start; i < end; i++)
    {
        if (GameMap->getCellPathLocked(stepList[i].tileR, stepList[i].tileC, stepList[i].cellR, stepList[i].cellC) != 0)
        {
            return 1;
        }
    }

    return 0;
}

auto MovePath::isBlocked(int32_t start, int32_t range, int* reachedEnd) -> int
{
    if (start == -1)
    {
        start = curStep;
    }

    if (reachedEnd != nullptr)
    {
        *reachedEnd = 0;
    }

    int32_t end = range + start;

    if (numStepsWhenNotPaused <= end)
    {
        end = numStepsWhenNotPaused;

        if (reachedEnd != nullptr)
        {
            *reachedEnd = 1;
        }
    }

    for (int32_t i = start; i < end; i++)
    {
        const MapTile& tile = GameMap->map[stepList[i].tileR * GameMap->width + stepList[i].tileC];

        if (TileCellPassable(tile, stepList[i].cellR, stepList[i].cellC) == 0)
        {
            return 1;
        }
    }

    return 0;
}

auto MovePath::crossesBridge(int32_t start, int32_t range) -> int32_t
{
    if (start == -1)
    {
        start = curStep;
    }

    int32_t end = start + range;

    if (numStepsWhenNotPaused <= end)
    {
        end = numStepsWhenNotPaused;
    }

    for (int32_t i = start; i < end; i++)
    {
        const uint32_t overlay = GameMap->map[stepList[i].tileR * GameMap->width + stepList[i].tileC].overlay & 0x7f;

        if (OverlayIsBridge[overlay] != 0)
        {
            return GlobalMoveMap->calcArea(stepList[i].tileR, stepList[i].tileC);
        }
    }

    return -1;
}

auto MovePath::crossesTile(int32_t start, int32_t range, int32_t tileR, int32_t tileC) -> int32_t
{
    if (start == -1)
    {
        start = curStep;
    }

    int32_t end = range + start;

    if (numStepsWhenNotPaused <= end)
    {
        end = numStepsWhenNotPaused;
    }

    for (int32_t i = start; i < end; i++)
    {
        if (tileR == stepList[i].tileR && tileC == stepList[i].tileC)
        {
            return i;
        }
    }

    return -1;
}

auto MovePath::crossesClosedClanGate(int32_t, int32_t) -> int32_t
{
    return -1;
}

auto MovePath::crossesClosedISGate(int32_t, int32_t) -> int32_t
{
    return -1;
}

auto MovePath::crossesClosedGate(int32_t start, int32_t range) -> int32_t
{
    if (start == -1)
    {
        start = curStep;
    }

    int32_t end = range + start;

    if (numStepsWhenNotPaused <= end)
    {
        end = numStepsWhenNotPaused;
    }

    for (int32_t i = start; i < end; i++)
    {
        const uint32_t overlay = GameMap->map[stepList[i].tileR * GameMap->width + stepList[i].tileC].overlay & 0x7f;

        if (OverlayIsClosedGate[overlay] != 0)
        {
            return i;
        }
    }

    return -1;
}

auto MovePath::setMoveChunk(MoveChunk* chunk) -> void
{
    const int32_t stepCount = chunk->numSteps;

    for (int32_t i = 0; i < stepCount; i++)
    {
        PathStep& step = stepList[i];
        step.tileR = static_cast<int16_t>(chunk->stepPos[i][0]);
        step.tileC = static_cast<int16_t>(chunk->stepPos[i][1]);
        step.cellR = static_cast<int16_t>(chunk->stepPos[i][2]);
        step.cellC = static_cast<int16_t>(chunk->stepPos[i][3]);
        step.destination.x = cellToWorldCoord[step.cellC] + tileColToWorldCoord[step.tileC] + HalfMapCell;
        step.distanceToGoal = 0.0f;
        step.destination.z = 0.0f;
        step.direction = 0;
        step.destination.y = (tileRowToWorldCoord[step.tileR] - cellToWorldCoord[step.cellR]) - HalfMapCell;
    }

    numStepsWhenNotPaused = stepCount;
    numSteps = stepCount;
    curStep = 0;
    // With no steps the original reads numSteps, numStepsWhenNotPaused and curStep (all 0 by then) as the goal.
    goal = stepCount > 0 ? stepList[stepCount - 1].destination : vector_3d(0.0f, 0.0f, 0.0f);
    target.zero();
    marked = 0;
    cost = -1;
    globalStep = -1;
}

auto MovePath::getMoveChunk(MoveChunk*, int32_t, int32_t, int) -> void
{
}

auto MovePath::setDestination(int32_t stepNumber, vector_3d position) -> void
{
    stepList[stepNumber].destination = position;
}

auto MovePathManager::operator new(size_t size) noexcept -> void*
{
    return systemHeap->malloc(static_cast<uint32_t>(size));
}

auto MovePathManager::operator delete(void* ptr) -> void
{
    systemHeap->free(ptr);
}

auto MovePathManager::init() -> int32_t
{
    for (int32_t i = 0; i < MAX_PATH_QUEUE_RECS; i++)
    {
        pool[i].pilot = nullptr;
        pool[i].selectionIndex = 0;
        pool[i].moveParams = 0;
        pool[i].prev = i > 0 ? &pool[i - 1] : nullptr;
        pool[i].next = i < MAX_PATH_QUEUE_RECS - 1 ? &pool[i + 1] : nullptr;
    }

    queueFront = nullptr;
    queueEnd = nullptr;
    NumPathsInQueue = 0;
    freeList = &pool[0];
    return 0;
}

auto MovePathManager::destroy() -> void
{
}

auto MovePathManager::remove(PathQueueRec* rec) -> void
{
    if (rec->prev == nullptr)
    {
        queueFront = rec->next;
    }
    else
    {
        rec->prev->next = rec->next;
    }

    if (rec->next == nullptr)
    {
        queueEnd = rec->prev;
    }
    else
    {
        rec->next->prev = rec->prev;
    }

    rec->prev = nullptr;
    rec->next = freeList;
    freeList = rec;
    NumPathsInQueue--;
}

auto MovePathManager::remove(MechWarrior* pilot) -> PathQueueRec*
{
    PathQueueRec* rec = pilot->movePathRequest;

    if (rec == nullptr)
    {
        return nullptr;
    }

    remove(rec);
    pilot->movePathRequest = nullptr;
    return rec;
}

auto MovePathManager::request(MechWarrior* pilot, int32_t selectionIndex, uint32_t moveParams, float priority,
                              int32_t initPath) -> void
{
    remove(pilot);
    PathQueueRec* rec = freeList;

    if (rec == nullptr)
    {
        Fatal(0, " Too many pilots calcing paths ");
    }

    freeList = rec->next;

    if (freeList != nullptr)
    {
        freeList->prev = nullptr;
    }

    rec->selectionIndex = selectionIndex;
    rec->initPath = initPath;
    rec->priority = priority;
    rec->pilot = pilot;
    rec->moveParams = moveParams;

    if (queueEnd == nullptr)
    {
        rec->prev = nullptr;
        rec->next = nullptr;
        queueEnd = rec;
        queueFront = rec;
    }
    else
    {
        // Walk back from the end to the last request of higher priority value, and queue behind it.
        PathQueueRec* after = nullptr;
        PathQueueRec* current = queueEnd;

        for (; current != nullptr; current = current->prev)
        {
            if (priority < current->priority)
            {
                break;
            }

            after = current;
        }

        if (current != nullptr)
        {
            rec->prev = current;
            rec->next = current->next;
            current->next = rec;

            if (rec->next == nullptr)
            {
                queueEnd = rec;
            }
            else
            {
                rec->next->prev = rec;
            }
        }
        else
        {
            rec->prev = nullptr;
            rec->next = after;
            after->prev = rec;
            queueFront = rec;
        }
    }

    pilot->movePathRequest = rec;
    NumPathsInQueue++;
}

auto MovePathManager::calcPath() -> void
{
    PathQueueRec* rec = queueFront;

    if (rec == nullptr)
    {
        return;
    }

    remove(rec);
    MechWarrior* pilot = rec->pilot;
    pilot->movePathRequest = nullptr;

    if (pilot->vehicle != nullptr)
    {
        pilot->calcMovePath(rec->selectionIndex, rec->moveParams, rec->initPath);
    }
}

auto MovePathManager::update() -> void
{
    for (int32_t i = 0; i < 5; i++)
    {
        if (queueFront == nullptr)
        {
            return;
        }

        calcPath();
    }
}

auto GlobalMap::operator new(size_t size) noexcept -> void*
{
    return systemHeap->malloc(static_cast<uint32_t>(size));
}

auto GlobalMap::operator delete(void* ptr) -> void
{
    systemHeap->free(ptr);
}

namespace
{
    /// <summary>Reads a little-endian field from a file record and steps past it.</summary>
    template <typename T> T ReadRecordField(const uint8_t*& cursor)
    {
        T value;
        std::memcpy(&value, cursor, sizeof(T));
        cursor += sizeof(T);
        return value;
    }

    /// <summary>Writes a field into a file record and steps past it.</summary>
    template <typename T> void WriteRecordField(uint8_t*& cursor, T value)
    {
        std::memcpy(cursor, &value, sizeof(T));
        cursor += sizeof(T);
    }

    /// <summary>Decodes a 0x29-byte area record (the doors pointer is left for the caller).</summary>
    void DecodeArea(const uint8_t* record, GlobalMapArea& area)
    {
        area.sectorR = ReadRecordField<int16_t>(record);
        area.sectorC = ReadRecordField<int16_t>(record);
        ReadRecordField<uint32_t>(record);
        area.doors = nullptr;
        area.type = ReadRecordField<int32_t>(record);
        area.numDoors = ReadRecordField<char>(record);
        area.open = ReadRecordField<int32_t>(record);
        area.unknown11 = ReadRecordField<int32_t>(record);
        area.unknown15 = ReadRecordField<int32_t>(record);
        area.closed = ReadRecordField<int32_t>(record);
        area.unknown1D = ReadRecordField<int32_t>(record);
        area.unknown21 = ReadRecordField<int32_t>(record);
        area.unknown25 = ReadRecordField<int32_t>(record);
    }

    /// <summary>Encodes an area as its 0x29-byte file record (the doors pointer written as 0).</summary>
    void EncodeArea(const GlobalMapArea& area, uint8_t* record)
    {
        WriteRecordField<int16_t>(record, area.sectorR);
        WriteRecordField<int16_t>(record, area.sectorC);
        WriteRecordField<uint32_t>(record, 0);
        WriteRecordField<int32_t>(record, area.type);
        WriteRecordField<char>(record, area.numDoors);
        WriteRecordField<int32_t>(record, area.open);
        WriteRecordField<int32_t>(record, area.unknown11);
        WriteRecordField<int32_t>(record, area.unknown15);
        WriteRecordField<int32_t>(record, area.closed);
        WriteRecordField<int32_t>(record, area.unknown1D);
        WriteRecordField<int32_t>(record, area.unknown21);
        WriteRecordField<int32_t>(record, area.unknown25);
    }

    /// <summary>Decodes a 0x3b-byte door record (the link pointers are left for the caller).</summary>
    void DecodeDoor(const uint8_t* record, GlobalMapDoor& door)
    {
        door.row = ReadRecordField<int16_t>(record);
        door.col = ReadRecordField<int16_t>(record);
        door.cellR = ReadRecordField<uint8_t>(record);
        door.cellC = ReadRecordField<uint8_t>(record);
        door.length = ReadRecordField<char>(record);
        door.open = ReadRecordField<int32_t>(record);
        door.area[0] = ReadRecordField<int16_t>(record);
        door.area[1] = ReadRecordField<int16_t>(record);
        door.areaCost[0] = ReadRecordField<int16_t>(record);
        door.areaCost[1] = ReadRecordField<int16_t>(record);
        door.direction[0] = ReadRecordField<char>(record);
        door.direction[1] = ReadRecordField<char>(record);
        door.numLinks[0] = ReadRecordField<char>(record);
        door.numLinks[1] = ReadRecordField<char>(record);
        ReadRecordField<uint32_t>(record);
        ReadRecordField<uint32_t>(record);
        door.links[0] = nullptr;
        door.links[1] = nullptr;
        door.cost = ReadRecordField<int32_t>(record);
        door.parent = ReadRecordField<int32_t>(record);
        door.fromAreaIndex = ReadRecordField<int32_t>(record);
        door.flags = ReadRecordField<uint32_t>(record);
        door.g = ReadRecordField<int32_t>(record);
        door.hPrime = ReadRecordField<int32_t>(record);
        door.fPrime = ReadRecordField<int32_t>(record);
    }

    /// <summary>Encodes a door as its 0x3b-byte file record (the link pointers written as 0).</summary>
    void EncodeDoor(const GlobalMapDoor& door, uint8_t* record)
    {
        WriteRecordField<int16_t>(record, door.row);
        WriteRecordField<int16_t>(record, door.col);
        WriteRecordField<uint8_t>(record, door.cellR);
        WriteRecordField<uint8_t>(record, door.cellC);
        WriteRecordField<char>(record, door.length);
        WriteRecordField<int32_t>(record, door.open);
        WriteRecordField<int16_t>(record, door.area[0]);
        WriteRecordField<int16_t>(record, door.area[1]);
        WriteRecordField<int16_t>(record, door.areaCost[0]);
        WriteRecordField<int16_t>(record, door.areaCost[1]);
        WriteRecordField<char>(record, door.direction[0]);
        WriteRecordField<char>(record, door.direction[1]);
        WriteRecordField<char>(record, door.numLinks[0]);
        WriteRecordField<char>(record, door.numLinks[1]);
        WriteRecordField<uint32_t>(record, 0);
        WriteRecordField<uint32_t>(record, 0);
        WriteRecordField<int32_t>(record, door.cost);
        WriteRecordField<int32_t>(record, door.parent);
        WriteRecordField<int32_t>(record, door.fromAreaIndex);
        WriteRecordField<uint32_t>(record, door.flags);
        WriteRecordField<int32_t>(record, door.g);
        WriteRecordField<int32_t>(record, door.hPrime);
        WriteRecordField<int32_t>(record, door.fPrime);
    }

    /// <summary>Whether (row, col) lies in the sector being filled (GlobalMap::minTileR and friends).</summary>
    bool InFillSector(int32_t row, int32_t col)
    {
        return row >= GlobalMap::minTileR && row < GlobalMap::maxTileR && col >= GlobalMap::minTileC &&
               col < GlobalMap::maxTileC;
    }

    /// <summary>A tile's overlay type.</summary>
    uint32_t TileOverlay(ScenarioMap* map, int32_t row, int32_t col)
    {
        return map->map[map->width * row + col].overlay & 0x7f;
    }

    /// <summary>Cells per row of the door finder's cell map (MCX: a 120 x 120 short array on the stack).</summary>
    constexpr int32_t DOOR_CELL_MAP_SIDE = 120;
}

auto GlobalMap::init(int32_t newWidth, int32_t newHeight) -> void
{
    width = newWidth;
    height = newHeight;
    areaMap = static_cast<int16_t*>(systemHeap->malloc(static_cast<uint32_t>(newWidth * newHeight * sizeof(int16_t))));

    if (areaMap == nullptr)
    {
        Fatal(0, "Not enough Memory for LargeAreaMap");
    }

    for (int32_t row = 0; row < height; row++)
    {
        for (int32_t col = 0; col < width; col++)
        {
            areaMap[width * row + col] = -1;
        }
    }

    sectorDim = 10;

    if (width % 10 != 0 || height % 10 != 0)
    {
        Fatal(0, "Scenario Map Dimensions must be multiples of SectorDim");
    }

    numAreas = 0;
    areas = nullptr;
    numDoors = 0;
    sectorWidth = newWidth / 10;
    sectorHeight = newWidth / 10;
    doors = nullptr;
    doorBuildList = nullptr;
    pathCostTable = nullptr;
}

auto GlobalMap::init(File* mapFile) -> int32_t
{
    const int32_t version = mapFile->readLong();

    if (version != GLOBALMAP_VERSION)
    {
        Fatal(version, " Bad version number in Global Map ");
    }

    unknown00 = mapFile->readLong();
    unknown04 = mapFile->readLong();
    height = mapFile->readLong();
    width = mapFile->readLong();
    sectorDim = mapFile->readLong();
    sectorHeight = mapFile->readLong();
    sectorWidth = mapFile->readLong();
    numAreas = mapFile->readLong();
    numDoors = mapFile->readLong();
    numDoorInfos = mapFile->readLong();
    numDoorLinks = mapFile->readLong();
    smallAreaMap = nullptr;
    areaMap = nullptr;

    if (numAreas < 256)
    {
        const int32_t size = width * height;
        smallAreaMap = static_cast<uint8_t*>(systemHeap->malloc(static_cast<uint32_t>(size)));

        if (smallAreaMap == nullptr)
        {
            Fatal(0, " Not Enough Memory for GlobalMap:smallAreaMap ");
        }

        mapFile->read(smallAreaMap, size);
    }
    else
    {
        const int32_t size = width * height * 2;
        areaMap = static_cast<int16_t*>(systemHeap->malloc(static_cast<uint32_t>(size)));

        if (areaMap == nullptr)
        {
            Fatal(0, " Not Enough Memory for GlobalMap:largeAreaMap ");
        }

        mapFile->read(reinterpret_cast<uint8_t*>(areaMap), size);
    }

    doorInfos = static_cast<DoorInfo*>(systemHeap->malloc(static_cast<uint32_t>(numDoorInfos * sizeof(DoorInfo))));

    if (doorInfos == nullptr)
    {
        Fatal(0, " Not Enough Memory for GlobalMap:doorInfos ");
    }

    mapFile->read(reinterpret_cast<uint8_t*>(doorInfos), numDoorInfos * static_cast<int32_t>(sizeof(DoorInfo)));

    // Port fix: room for the spare area setTempArea writes (the original allocated exactly numAreas here).
    areas =
        static_cast<GlobalMapArea*>(systemHeap->malloc(static_cast<uint32_t>((numAreas + 1) * sizeof(GlobalMapArea))));

    if (areas == nullptr)
    {
        Fatal(0, " Not Enough Memory for GlobalMap:areas ");
    }

    std::vector<uint8_t> records(static_cast<size_t>(numAreas) * GLOBALMAP_AREA_RECORD_SIZE);
    mapFile->read(records.data(), numAreas * GLOBALMAP_AREA_RECORD_SIZE);
    int32_t infoIndex = 0;

    for (int32_t i = 0; i < numAreas; i++)
    {
        DecodeArea(records.data() + static_cast<size_t>(i) * GLOBALMAP_AREA_RECORD_SIZE, areas[i]);
        areas[i].doors = doorInfos + infoIndex;
        infoIndex += areas[i].numDoors;
    }

    std::memset(&areas[numAreas], 0, sizeof(GlobalMapArea));

    doorLinks = static_cast<DoorLink*>(systemHeap->malloc(static_cast<uint32_t>(numDoorLinks * sizeof(DoorLink))));

    if (doorLinks == nullptr)
    {
        Fatal(0, " Not Enough Memory for GlobalMap:doorlinks ");
    }

    mapFile->read(reinterpret_cast<uint8_t*>(doorLinks), numDoorLinks * static_cast<int32_t>(sizeof(DoorLink)));

    const int32_t totalDoors = numDoors + 2;
    doors = static_cast<GlobalMapDoor*>(systemHeap->malloc(static_cast<uint32_t>(totalDoors * sizeof(GlobalMapDoor))));

    if (doors == nullptr)
    {
        Fatal(0, " Not Enough Memory for GlobalMap:doors ");
    }

    records.assign(static_cast<size_t>(totalDoors) * GLOBALMAP_DOOR_RECORD_SIZE, 0);
    mapFile->read(records.data(), totalDoors * GLOBALMAP_DOOR_RECORD_SIZE);
    int32_t linkIndex = 0;

    for (int32_t i = 0; i < totalDoors; i++)
    {
        GlobalMapDoor& door = doors[i];
        DecodeDoor(records.data() + static_cast<size_t>(i) * GLOBALMAP_DOOR_RECORD_SIZE, door);

        for (int32_t side = 0; side < 2; side++)
        {
            door.links[side] = doorLinks + linkIndex;
            Assert(door.numLinks[side] + 2 > 1 ? 1 : 0, 0, " Bad Door Links Count ");
            linkIndex += door.numLinks[side] + 2;
        }
    }

    pathCostTable = static_cast<uint8_t*>(systemHeap->malloc(static_cast<uint32_t>(numAreas * numAreas)));

    if (pathCostTable == nullptr)
    {
        Fatal(0, " Not Enough Memory for GlobalMap.pathCostTable ");
    }

    mapFile->read(pathCostTable, numAreas * numAreas);

    // A leftover loop of the original: it only leaves unknown4C counted up to numAreas.
    for (int32_t i = 0; i < numAreas; i++)
    {
        unknown4C = 0;

        do
        {
            unknown4C++;
        } while (unknown4C < numAreas);
    }

    return 0;
}

auto GlobalMap::init(ScenarioMap* map, int32_t unknownA, int32_t unknownB, int32_t newHeight, int32_t newWidth)
    -> int32_t
{
    if (newHeight == -1)
    {
        newHeight = map->height;
    }

    if (newWidth == -1)
    {
        newWidth = map->width;
    }

    init(newHeight, newWidth); // as the original: the map's height goes to init's width slot (maps are square)
    unknown04 = unknownB;
    unknown00 = unknownA;
    calcAreas(map);
    calcBridges(map);
    calcGlobalDoors(map);
    calcAreaDoors();
    calcDoorLinks();

    if (numAreas < 256)
    {
        smallAreaMap = static_cast<uint8_t*>(systemHeap->malloc(static_cast<uint32_t>(height * width)));

        if (smallAreaMap == nullptr)
        {
            Fatal(0, " Not Enough Memory for SmallAreaMap ");
        }

        for (int32_t row = 0; row < height; row++)
        {
            for (int32_t col = 0; col < width; col++)
            {
                const int16_t area = areaMap[width * row + col];
                smallAreaMap[width * row + col] = area < 0 ? 0xff : static_cast<uint8_t>(area);
            }
        }

        systemHeap->free(areaMap);
        areaMap = nullptr;
    }

    return 0;
}

auto GlobalMap::write(File* mapFile) -> int32_t
{
    mapFile->writeLong(GLOBALMAP_VERSION);
    mapFile->writeLong(unknown00);
    mapFile->writeLong(unknown04);
    mapFile->writeLong(height);
    mapFile->writeLong(width);
    mapFile->writeLong(sectorDim);
    mapFile->writeLong(sectorHeight);
    mapFile->writeLong(sectorWidth);
    mapFile->writeLong(numAreas);
    mapFile->writeLong(numDoors);
    mapFile->writeLong(numDoorInfos);
    mapFile->writeLong(numDoorLinks);

    if (smallAreaMap != nullptr)
    {
        mapFile->write(smallAreaMap, width * height);
    }
    else
    {
        mapFile->write(reinterpret_cast<const uint8_t*>(areaMap), width * height * 2);
    }

    for (int32_t i = 0; i < numAreas; i++)
    {
        mapFile->write(reinterpret_cast<const uint8_t*>(areas[i].doors), areas[i].numDoors * 3);
    }

    std::vector<uint8_t> records(static_cast<size_t>(numAreas) * GLOBALMAP_AREA_RECORD_SIZE);

    for (int32_t i = 0; i < numAreas; i++)
    {
        EncodeArea(areas[i], records.data() + static_cast<size_t>(i) * GLOBALMAP_AREA_RECORD_SIZE);
    }

    mapFile->write(records.data(), numAreas * GLOBALMAP_AREA_RECORD_SIZE);

    const int32_t totalDoors = numDoors + 2;

    for (int32_t i = 0; i < totalDoors; i++)
    {
        for (int32_t side = 0; side < 2; side++)
        {
            const int32_t count = doors[i].numLinks[side] + 2;
            Assert(count > 1 ? 1 : 0, 0, " Bad Door Links Count ");
            mapFile->write(reinterpret_cast<const uint8_t*>(doors[i].links[side]), count * 7);
        }
    }

    records.assign(static_cast<size_t>(totalDoors) * GLOBALMAP_DOOR_RECORD_SIZE, 0);

    for (int32_t i = 0; i < totalDoors; i++)
    {
        EncodeDoor(doors[i], records.data() + static_cast<size_t>(i) * GLOBALMAP_DOOR_RECORD_SIZE);
    }

    mapFile->write(records.data(), totalDoors * GLOBALMAP_DOOR_RECORD_SIZE);

    calcPathCostTable();
    mapFile->write(pathCostTable, numAreas * numAreas);
    return 0;
}

auto GlobalMap::destroy() -> void
{
    if (smallAreaMap != nullptr)
    {
        systemHeap->free(smallAreaMap);
        smallAreaMap = nullptr;
    }

    if (areaMap != nullptr)
    {
        systemHeap->free(areaMap);
        areaMap = nullptr;
    }

    if (areas != nullptr)
    {
        // Computed maps give each area its own door list; loaded ones point into doorInfos.
        if (doorInfos == nullptr)
        {
            for (int32_t i = 0; i < numAreas + 1; i++)
            {
                if (areas[i].doors != nullptr)
                {
                    systemHeap->free(areas[i].doors);
                    areas[i].doors = nullptr;
                }
            }
        }

        systemHeap->free(areas);
        areas = nullptr;
    }

    if (doors != nullptr)
    {
        if (doorLinks == nullptr)
        {
            for (int32_t i = 0; i < numDoors + 2; i++)
            {
                for (int32_t side = 0; side < 2; side++)
                {
                    if (doors[i].links[side] != nullptr)
                    {
                        systemHeap->free(doors[i].links[side]);
                        doors[i].links[side] = nullptr;
                    }
                }
            }
        }

        systemHeap->free(doors);
        doors = nullptr;
    }

    if (doorInfos != nullptr)
    {
        systemHeap->free(doorInfos);
        doorInfos = nullptr;
    }

    if (doorLinks != nullptr)
    {
        systemHeap->free(doorLinks);
        doorLinks = nullptr;
    }

    if (pathCostTable != nullptr)
    {
        systemHeap->free(pathCostTable);
        pathCostTable = nullptr;
    }
}

auto GlobalMap::setTempArea(int32_t tileR, int32_t tileC, int32_t) -> int32_t
{
    GlobalMapArea& area = areas[numAreas];
    area.numDoors = 0;
    area.sectorR = static_cast<int16_t>(tileR / sectorDim);
    area.sectorC = static_cast<int16_t>(tileC / sectorDim);
    area.open = 1;
    return numAreas;
}

auto GlobalMap::fillNorthSouthBridgeArea(ScenarioMap* map, int32_t row, int32_t col, int32_t area) -> int
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    areaMap[row * width + col] = static_cast<int16_t>(area);

    for (const int32_t next : {row - 1, row + 1})
    {
        if (InFillSector(next, col) && TileOverlay(map, next, col) == 0x25 && areaMap[next * width + col] == -1)
        {
            fillNorthSouthBridgeArea(map, next, col, area);
        }
    }

    return 1;
}

auto GlobalMap::fillEastWestBridgeArea(ScenarioMap* map, int32_t row, int32_t col, int32_t area) -> int
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    areaMap[row * width + col] = static_cast<int16_t>(area);

    for (const int32_t next : {col + 1, col - 1})
    {
        if (InFillSector(row, next) && TileOverlay(map, row, next) == 0x27 && areaMap[row * width + next] == -1)
        {
            fillEastWestBridgeArea(map, row, next, area);
        }
    }

    return 1;
}

auto GlobalMap::fillNorthSouthRailroadBridgeArea(ScenarioMap* map, int32_t row, int32_t col, int32_t area) -> int
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    areaMap[row * width + col] = static_cast<int16_t>(area);

    for (const int32_t next : {row - 1, row + 1})
    {
        if (InFillSector(next, col) && TileOverlay(map, next, col) == 0x37 && areaMap[next * width + col] == -1)
        {
            fillNorthSouthRailroadBridgeArea(map, next, col, area);
        }
    }

    return 1;
}

auto GlobalMap::fillEastWestRailroadBridgeArea(ScenarioMap* map, int32_t row, int32_t col, int32_t area) -> int
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    areaMap[row * width + col] = static_cast<int16_t>(area);

    for (const int32_t next : {col + 1, col - 1})
    {
        if (InFillSector(row, next) && TileOverlay(map, row, next) == 0x39 && areaMap[row * width + next] == -1)
        {
            fillEastWestRailroadBridgeArea(map, row, next, area);
        }
    }

    return 1;
}

auto isLRBlocked(MapTile* tile) -> int
{
    // Per cell (row-major), whether it is impassable.
    uint32_t blocked[9];

    for (int32_t cell = 0; cell < 9; cell++)
    {
        blocked[cell] = (~tile->cells >> (14 + cell * 2)) & 1;
    }

    // A full row or a full column of blocked cells.
    for (int32_t i = 0; i < 3; i++)
    {
        if (blocked[i * 3] != 0 && blocked[i * 3 + 1] != 0 && blocked[i * 3 + 2] != 0)
        {
            return 1;
        }
    }

    for (int32_t i = 0; i < 3; i++)
    {
        if (blocked[i] != 0 && blocked[i + 3] != 0 && blocked[i + 6] != 0)
        {
            return 1;
        }
    }

    return 0;
}

auto GlobalMap::fillArea(ScenarioMap* map, int32_t row, int32_t col, int32_t area) -> int
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    const MapTile* mapTile = &map->map[map->width * row + col];
    const uint32_t overlay = mapTile->overlay & 0x7f;

    if (overlay == 0x27 || overlay == 0x25 || overlay == 0x39 || overlay == 0x37)
    {
        return 0;
    }

    if (CurPlanet == 1 && OverlayIsDirtRoad[overlay] != 0)
    {
        return 0;
    }

    Assert(row >= 0 && row < map->height && col >= 0 && col < map->width ? 1 : 0, 0, " Map Tile out of bounds ");
    MapTile tile = *mapTile;
    const uint32_t terrain = tile.cells & 0x7f;
    bool open = false;

    if ((tile.cells & 0x55554000) != 0 && isLRBlocked(&tile) == 0 && overlay != 0x3e && terrain != 0x2a &&
        terrain != 0x29)
    {
        open = BlockWallTiles == 0 || overlay != 0x3c;
    }

    if (!open)
    {
        areaMap[width * row + col] = -2;
        return 0;
    }

    areaMap[width * row + col] = static_cast<int16_t>(area);

    for (int32_t dir = 0; dir < 4; dir++)
    {
        const int32_t nextRow = adjTile[dir][0] + row;
        const int32_t nextCol = adjTile[dir][1] + col;

        if (InFillSector(nextRow, nextCol) && areaMap[width * nextRow + nextCol] == -1)
        {
            fillArea(map, nextRow, nextCol, area);
        }
    }

    return 1;
}

auto GlobalMap::calcSectorAreas(ScenarioMap* map, int32_t sectorR, int32_t sectorC) -> void
{
    minTileR = sectorDim * sectorR;
    maxTileR = sectorDim + minTileR;
    minTileC = sectorDim * sectorC;
    maxTileC = sectorDim + minTileC;

    for (int32_t row = minTileR; row < maxTileR; row++)
    {
        for (int32_t col = minTileC; col < maxTileC; col++)
        {
            if (areaMap[width * row + col] != -1)
            {
                continue;
            }

            int filled;

            switch (TileOverlay(map, row, col))
            {
                case 0x25:
                    filled = fillNorthSouthBridgeArea(map, row, col, numAreas);
                    break;
                case 0x27:
                    filled = fillEastWestBridgeArea(map, row, col, numAreas);
                    break;
                case 0x37:
                    filled = fillNorthSouthRailroadBridgeArea(map, row, col, numAreas);
                    break;
                case 0x39:
                    filled = fillEastWestRailroadBridgeArea(map, row, col, numAreas);
                    break;
                default:
                    filled = fillArea(map, row, col, numAreas);
                    break;
            }

            if (filled != 0)
            {
                numAreas++;
            }
        }
    }
}

auto GlobalMap::calcAreas(ScenarioMap* map) -> void
{
    for (int32_t sectorR = 0; sectorR < sectorHeight; sectorR++)
    {
        for (int32_t sectorC = 0; sectorC < sectorWidth; sectorC++)
        {
            calcSectorAreas(map, sectorR, sectorC);
        }
    }

    if (numAreas > 10000)
    {
        Fatal(0, " Too many GlobalMapAreas ");
    }

    // One spare area past the last, for setTempArea.
    const int32_t count = numAreas + 1;
    areas = static_cast<GlobalMapArea*>(systemHeap->malloc(static_cast<uint32_t>(count * sizeof(GlobalMapArea))));

    for (int32_t i = 0; i < count; i++)
    {
        GlobalMapArea& area = areas[i];
        area.type = 0;
        area.numDoors = 0;
        area.doors = nullptr;
        area.open = 1;
        area.unknown11 = -1;
        area.closed = 0;
        area.unknown1D = 0;
        area.unknown21 = 0;
        area.unknown25 = 0;
    }

    for (int32_t sectorR = 0; sectorR < sectorHeight; sectorR++)
    {
        minTileR = sectorDim * sectorR;
        maxTileR = sectorDim + minTileR;

        for (int32_t sectorC = 0; sectorC < sectorWidth; sectorC++)
        {
            minTileC = sectorDim * sectorC;
            maxTileC = sectorDim + minTileC;

            for (int32_t row = minTileR; row < maxTileR; row++)
            {
                for (int32_t col = minTileC; col < maxTileC; col++)
                {
                    const int16_t area = areaMap[width * row + col];

                    if (area >= 0)
                    {
                        areas[area].sectorR = static_cast<int16_t>(sectorR);
                        areas[area].sectorC = static_cast<int16_t>(sectorC);
                    }
                }
            }
        }
    }
}

auto GlobalMap::calcBridges(ScenarioMap* map) -> void
{
    for (int32_t row = 0; row < height; row++)
    {
        for (int32_t col = 0; col < width; col++)
        {
            const uint32_t overlay = map->map[row * map->width + col].overlay & 0x7f;

            if (overlay == 0x25 || overlay == 0x37)
            {
                areas[areaMap[width * row + col]].type = 1;
            }
            else if (overlay == 0x27 || overlay == 0x39)
            {
                areas[areaMap[width * row + col]].type = 2;
            }
        }
    }
}

auto GlobalMap::beginDoorProcessing() -> void
{
    doorBuildList = static_cast<GlobalMapDoor*>(systemHeap->malloc(MAX_BUILD_DOORS * sizeof(GlobalMapDoor)));

    if (doorBuildList == nullptr)
    {
        Fatal(0, " No RAM for Door Build List ");
    }
}

auto GlobalMap::addDoor(int32_t area1, int32_t area2, int32_t row, int32_t col, int32_t cellR, int32_t cellC,
                        int32_t length, int32_t direction) -> void
{
    for (int32_t i = 0; i < numDoors; i++)
    {
        const GlobalMapDoor& door = doorBuildList[i];

        if (door.row == row && door.col == col && door.cellR == cellR && door.cellC == cellC && door.length == length &&
            door.direction[0] == direction)
        {
            return;
        }
    }

    // Port fix: the original writes past the build list once it holds MAX_BUILD_DOORS doors.
    if (numDoors >= MAX_BUILD_DOORS - 2)
    {
        return;
    }

    GlobalMapDoor& door = doorBuildList[numDoors];
    door.row = static_cast<int16_t>(row);
    door.col = static_cast<int16_t>(col);
    door.cellR = static_cast<uint8_t>(cellR);
    door.cellC = static_cast<uint8_t>(cellC);
    door.length = static_cast<char>(length);
    door.open = 1;
    door.area[0] = static_cast<int16_t>(area1);
    door.areaCost[0] = 1;
    door.direction[0] = static_cast<char>(direction);
    door.area[1] = static_cast<int16_t>(area2);
    door.areaCost[1] = 1;
    door.direction[1] = static_cast<char>((direction + 2) % 4);
    numDoors++;
}

auto GlobalMap::endDoorProcessing() -> void
{
    if (doorBuildList == nullptr)
    {
        return;
    }

    const uint32_t size = static_cast<uint32_t>((numDoors + 2) * sizeof(GlobalMapDoor));
    doors = static_cast<GlobalMapDoor*>(systemHeap->malloc(size));
    std::memcpy(doors, doorBuildList, size);
    systemHeap->free(doorBuildList);
    doorBuildList = nullptr;
}

auto GlobalMap::numAreaDoors(int32_t area) -> int32_t
{
    int32_t count = 0;

    for (int32_t i = 0; i < numDoors; i++)
    {
        if (doors[i].area[0] == area || doors[i].area[1] == area)
        {
            count++;
        }
    }

    return count;
}

auto GlobalMap::getAreaDoors(int32_t area, DoorInfo* doorList) -> void
{
    for (int32_t i = 0; i < numDoors; i++)
    {
        if (doors[i].area[0] == area || doors[i].area[1] == area)
        {
            doorList->doorIndex = static_cast<int16_t>(i);
            doorList->doorSide = doors[i].area[1] == area ? 1 : 0;
            doorList++;
        }
    }
}

auto GlobalMap::calcGlobalDoors(ScenarioMap* map) -> void
{
    beginDoorProcessing();
    std::vector<int16_t> cellMap(DOOR_CELL_MAP_SIDE * DOOR_CELL_MAP_SIDE);

    for (int32_t sectorR = 0; sectorR < sectorHeight; sectorR++)
    {
        for (int32_t sectorC = 0; sectorC < sectorWidth; sectorC++)
        {
            // Direction 1 looks east, 2 south (adjTile), for the cells an area can cross into its neighbour by.
            for (int32_t dir = 1; dir < 3; dir++)
            {
                std::fill(cellMap.begin(), cellMap.end(), static_cast<int16_t>(-1));
                minTileR = sectorDim * sectorR;
                maxTileR = sectorDim + minTileR;
                minTileC = sectorDim * sectorC;
                maxTileC = sectorDim + minTileC;
                const int32_t minCellR = minTileR * MAPCELL_DIM;
                const int32_t maxCellR = maxTileR * MAPCELL_DIM;
                const int32_t minCellC = minTileC * MAPCELL_DIM;
                const int32_t maxCellC = maxTileC * MAPCELL_DIM;
                auto cell = [&](int32_t cellRow, int32_t cellCol) -> int16_t&
                { return cellMap[(cellRow - minCellR) * DOOR_CELL_MAP_SIDE + (cellCol - minCellC)]; };

                for (int32_t row = minTileR; row < maxTileR; row++)
                {
                    for (int32_t col = minTileC; col < maxTileC; col++)
                    {
                        const int32_t area = areaMap[width * row + col];

                        if (area < 0)
                        {
                            continue;
                        }

                        const int32_t nextRow = adjTile[dir][0] + row;
                        const int32_t nextCol = adjTile[dir][1] + col;

                        if (nextRow < 0 || nextRow >= height || nextCol < 0 || nextCol >= width)
                        {
                            continue;
                        }

                        const int16_t nextArea = areaMap[width * nextRow + nextCol];

                        if (nextArea < 0 || area == nextArea)
                        {
                            continue;
                        }

                        // Bridges only join areas along their own direction.
                        const int32_t type = areas[area].type;
                        const int32_t nextType = areas[nextArea].type;
                        const int32_t crossType = dir == 1 ? 2 : 1;

                        if ((type != 0 && type != crossType) || (nextType != 0 && nextType != crossType))
                        {
                            continue;
                        }

                        Assert(row >= 0 && row < map->height && col >= 0 && col < map->width ? 1 : 0, 0,
                               " Map Tile out of bounds ");
                        const MapTile tile = map->map[map->width * row + col];
                        Assert(nextRow >= 0 && nextRow < map->height && nextCol >= 0 && nextCol < map->width ? 1 : 0, 0,
                               " Map Tile out of bounds ");
                        const MapTile nextTile = map->map[map->width * nextRow + nextCol];
                        const int32_t baseRow = row * MAPCELL_DIM;
                        const int32_t baseCol = col * MAPCELL_DIM;

                        for (int32_t i = 0; i < MAPCELL_DIM; i++)
                        {
                            if (dir == 1)
                            {
                                if (TileCellPassable(tile, i, 2) != 0 && TileCellPassable(nextTile, i, 0) != 0)
                                {
                                    cell(baseRow + i, baseCol + 2) = nextArea;
                                }
                            }
                            else
                            {
                                if (TileCellPassable(tile, 2, i) != 0 && TileCellPassable(nextTile, 0, i) != 0)
                                {
                                    cell(baseRow + 2, baseCol + i) = nextArea;
                                }
                            }
                        }
                    }
                }

                if (dir == 1)
                {
                    // Runs down each cell column, from the east edge.
                    for (int32_t cellCol = maxCellC - 1; cellCol >= minCellC; cellCol--)
                    {
                        int32_t cellRow = minCellR;

                        while (cellRow < maxCellR)
                        {
                            const int16_t nextArea = cell(cellRow, cellCol);

                            if (nextArea < 0)
                            {
                                cellRow++;
                                continue;
                            }

                            const int32_t tileC = cellCol / 3;
                            const int32_t area = areaMap[(cellRow / 3) * width + tileC];
                            int32_t length = 0;

                            while (cellRow < maxCellR && areaMap[(cellRow / 3) * width + tileC] == area &&
                                   cell(cellRow, cellCol) == nextArea)
                            {
                                length++;
                                cellRow++;
                            }

                            addDoor(area, nextArea, (cellRow - length) / 3, tileC, (cellRow - length) % 3, cellCol % 3,
                                    length, 1);
                        }
                    }
                }
                else
                {
                    // Runs along each cell row, from the south edge.
                    for (int32_t cellRow = maxCellR - 1; cellRow >= minCellR; cellRow--)
                    {
                        int32_t cellCol = minCellC;

                        while (cellCol < maxCellC)
                        {
                            const int16_t nextArea = cell(cellRow, cellCol);

                            if (nextArea < 0)
                            {
                                cellCol++;
                                continue;
                            }

                            const int32_t tileR = cellRow / 3;
                            const int32_t area = areaMap[cellCol / 3 + width * tileR];
                            int32_t length = 0;

                            while (cellCol < maxCellC && areaMap[cellCol / 3 + width * tileR] == area &&
                                   cell(cellRow, cellCol) == nextArea)
                            {
                                length++;
                                cellCol++;
                            }

                            addDoor(area, nextArea, tileR, (cellCol - length) / 3, cellRow % 3, (cellCol - length) % 3,
                                    length, dir);
                        }
                    }
                }
            }
        }
    }

    endDoorProcessing();
}

auto GlobalMap::calcAreaDoors() -> void
{
    numDoorInfos = 0;

    for (int32_t i = 0; i < numAreas; i++)
    {
        GlobalMapArea& area = areas[i];
        area.numDoors = static_cast<char>(numAreaDoors(i));
        numDoorInfos += area.numDoors;

        if (area.numDoors == 0)
        {
            area.doors = nullptr;
            continue;
        }

        area.doors = static_cast<DoorInfo*>(systemHeap->malloc(static_cast<uint32_t>(area.numDoors * 3)));
        getAreaDoors(i, area.doors);
    }
}

auto GlobalMap::calcLinkCost(int32_t startDoor, int32_t thruArea, int32_t goalDoor) -> int32_t
{
    if (CurPlanet == 1)
    {
        // Dirt roads (overlays 1..15) cost nothing to cross, at every move level.
        for (int32_t level = 0; level < NUM_MOVE_LEVELS; level++)
        {
            int32_t* weights = OverlayWeightTable + level * OVERLAY_WEIGHT_LEVEL_SIZE + MAPCELL_DIM * MAPCELL_DIM;
            std::fill(weights, weights + 15 * MAPCELL_DIM * MAPCELL_DIM, 0);
        }
    }

    // The middle cell of a door, on the side facing thruArea.
    auto doorCell = [this, thruArea](int32_t doorIndex, int32_t& cellRow, int32_t& cellCol) -> bool
    {
        const GlobalMapDoor& door = doors[doorIndex];

        if (door.area[0] != thruArea && door.area[1] != thruArea)
        {
            return false;
        }

        const int32_t side = door.area[1] == thruArea ? 1 : 0;
        int32_t rowOffset;
        int32_t colOffset;

        if (door.direction[0] == 1)
        {
            colOffset = side;
            rowOffset = door.length / 2;
        }
        else
        {
            colOffset = door.length / 2;
            rowOffset = side;
        }

        cellCol = door.col * 3 + door.cellC + colOffset;
        cellRow = door.cellR + door.row * 3 + rowOffset;
        return true;
    };

    int32_t startRow = 0;
    int32_t startCol = 0;

    if (!doorCell(startDoor, startRow, startCol))
    {
        return -1;
    }

    int32_t goalRow = 0;
    int32_t goalCol = 0;

    if (!doorCell(goalDoor, goalRow, goalCol))
    {
        return -2;
    }

    vector_3d goalPos;
    goalPos.x = (static_cast<float>(goalCol) + 0.5f) * MetersPerCell - worldUnitsMapSide * 0.5f;
    goalPos.y = (worldUnitsMapSide * 0.5f - static_cast<float>(goalRow) * MetersPerCell) - MetersPerCell * 0.5f;
    goalPos.z = 0.0f;

    if (PathFindMap == nullptr)
    {
        Fatal(0, " No PathFindMap ");
    }

    MovePath path;
    path.goal.zero();
    path.numSteps = 0;
    path.numStepsWhenNotPaused = 0;
    path.curStep = 0;
    path.cost = 0;
    path.marked = 0;
    path.globalStep = -1;
    const int32_t ULr = areas[thruArea].sectorR * sectorDim;
    const int32_t ULc = areas[thruArea].sectorC * sectorDim;
    ClearBridgeTiles = 1;
    PathFindMap->setUp(GameMap, ULr, ULc, sectorDim, sectorDim, nullptr, (startRow / 3 - ULr) * 3 + startRow % 3,
                       (startCol / 3 - ULc) * 3 + startCol % 3, goalPos, (goalRow / 3 - ULr) * 3 + goalRow % 3,
                       (goalCol / 3 - ULc) * 3 + goalCol % 3, nullptr, 10, 0, 8, 0);
    int32_t goalCell[2] = {};
    PathFindMap->calcPath(&path, nullptr, goalCell);
    ClearBridgeTiles = 0;

    if (path.numSteps == 0)
    {
        path.destroy();
        return 9999;
    }

    const int32_t cost = path.cost;
    path.destroy();
    return cost;
}

auto GlobalMap::calcDoorLinks() -> void
{
    int32_t maxAreaDoors = 0;
    numDoorLinks = 0;

    for (int32_t doorIndex = 0; doorIndex < numDoors; doorIndex++)
    {
        GlobalMapDoor& door = doors[doorIndex];

        for (int32_t side = 0; side < 2; side++)
        {
            door.numLinks[side] = 0;
            door.links[side] = nullptr;
            const int32_t area = door.area[side];
            const int32_t areaDoors = areas[area].numDoors;
            door.numLinks[side] = static_cast<char>(areaDoors - 1);
            door.links[side] =
                static_cast<DoorLink*>(systemHeap->malloc(static_cast<uint32_t>((door.numLinks[side] + 2) * 7)));
            numDoorLinks += door.numLinks[side] + 2;

            if (door.links[side] == nullptr)
            {
                Fatal(0, " Coud not malloc systemHeap door link ");
            }

            DoorLink* link = door.links[side];

            for (int32_t i = 0; i < areaDoors; i++)
            {
                const int16_t otherIndex = areas[area].doors[i].doorIndex;

                if (otherIndex == doorIndex)
                {
                    continue;
                }

                link->doorIndex = otherIndex;
                link->doorSide = doors[otherIndex].area[1] == area ? 1 : 0;
                link->cost = calcLinkCost(doorIndex, area, otherIndex);
                link++;
            }

            if (maxAreaDoors < areaDoors)
            {
                maxAreaDoors = areaDoors;
            }
        }
    }

    // The temporary start and goal doors link out of any area, so they get room for the most doors an area has.
    for (int32_t doorIndex = numDoors; doorIndex < numDoors + 2; doorIndex++)
    {
        GlobalMapDoor& door = doors[doorIndex];
        door.numLinks[0] = static_cast<char>(maxAreaDoors);
        numDoorLinks += door.numLinks[0] + 2;
        door.links[0] = static_cast<DoorLink*>(systemHeap->malloc(static_cast<uint32_t>((door.numLinks[0] + 2) * 7)));
        door.numLinks[1] = 0;
        numDoorLinks += door.numLinks[1] + 2;
        door.links[1] = static_cast<DoorLink*>(systemHeap->malloc(static_cast<uint32_t>((door.numLinks[1] + 2) * 7)));
    }
}

auto GlobalMap::calcSectorPaths(ScenarioMap*, int32_t, int32_t) -> void
{
}

auto GlobalMap::calcPathCostTable() -> void
{
    pathCostTable = static_cast<uint8_t*>(systemHeap->malloc(static_cast<uint32_t>(numAreas * numAreas)));
    Assert(pathCostTable != nullptr ? 1 : 0, 0, " GlobalMap.calcPathCostTable: unable to malloc pathCostTable ");
    GlobalPathStep path[MAX_GLOBAL_PATH];

    for (int32_t startArea = 0; startArea < numAreas; startArea++)
    {
        for (unknown4C = 0; unknown4C < numAreas; unknown4C++)
        {
            if (startArea == unknown4C)
            {
                pathCostTable[numAreas * startArea + unknown4C] = 0;
            }
            else
            {
                pathCostTable[startArea * numAreas + unknown4C] =
                    static_cast<uint8_t>(calcPath(startArea, unknown4C, path));
            }
        }
    }
}

auto GlobalMap::exitDirection(int32_t doorIndex, int32_t fromArea) -> int32_t
{
    const GlobalMapDoor& door = doors[doorIndex];

    if (door.area[0] == fromArea)
    {
        return door.direction[0];
    }

    if (door.area[1] == fromArea)
    {
        return door.direction[1];
    }

    return -1;
}

auto GlobalMap::getDoorTiles(int32_t area, int32_t doorIndex, GlobalMapDoor* door) -> void
{
    *door = doors[areas[area].doors[doorIndex].doorIndex];
}

auto GlobalMap::getDoorWorldPos(int32_t, int32_t, int32_t* prevGoalCell) -> vector_3d
{
    const float x = (static_cast<float>(prevGoalCell[1]) + 0.5f) * MetersPerCell - worldUnitsMapSide * 0.5f;
    const float y =
        (worldUnitsMapSide * 0.5f - static_cast<float>(prevGoalCell[0]) * MetersPerCell) - MetersPerCell * 0.5f;
    const float z = GameMap->getTerrainElevation(vector_3d(x, y, 0.0f));
    return vector_3d(x, y, z);
}

namespace
{
    /// <summary>Makes a temporary door joining <paramref name="areaIndex"/> to itself (the start or goal door).</summary>
    /// <remarks>The field setup shared by GlobalMap::setStartDoor and setGoalDoor (inlined in both).</remarks>
    void InitTempDoor(GlobalMapDoor& door, int32_t areaIndex, char numLinks)
    {
        door.direction[0] = -1;
        door.direction[1] = -1;
        door.area[0] = static_cast<int16_t>(areaIndex);
        door.area[1] = static_cast<int16_t>(areaIndex);
        door.row = 0;
        door.col = 0;
        door.cellR = 0;
        door.cellC = 0;
        door.length = 0;
        door.open = 1;
        door.areaCost[0] = 1;
        door.areaCost[1] = 1;
        door.numLinks[0] = numLinks;
        door.numLinks[1] = 0;
    }
}

auto GlobalMap::setStartDoor(int32_t startArea) -> void
{
    GlobalMapDoor& startDoor = doors[numDoors];
    const GlobalMapArea& area = areas[startArea];
    InitTempDoor(startDoor, startArea, area.numDoors);
    startDoor.fromAreaIndex = 1;

    for (int32_t i = 0; i < startDoor.numLinks[0]; i++)
    {
        const DoorInfo& info = area.doors[i];
        DoorLink& link = startDoor.links[0][i];
        link.doorIndex = info.doorIndex;
        link.doorSide = info.doorSide;
        link.cost = 1;
        // Links the area's door back to the start door, in the spare room past its links.
        GlobalMapDoor& areaDoor = doors[info.doorIndex];
        const int32_t side = info.doorSide;
        DoorLink& backLink = areaDoor.links[side][areaDoor.numLinks[side]];
        backLink.doorIndex = static_cast<int16_t>(numDoors);
        backLink.doorSide = 0;
        backLink.cost = 1;
        areaDoor.numLinks[side]++;
    }
}

auto GlobalMap::resetStartDoor(int32_t startArea) -> void
{
    const GlobalMapDoor& startDoor = doors[numDoors];

    for (int32_t i = 0; i < startDoor.numLinks[0]; i++)
    {
        const DoorInfo& info = areas[startArea].doors[i];
        doors[info.doorIndex].numLinks[static_cast<int32_t>(info.doorSide)]--;
    }
}

auto GlobalMap::setGoalDoor(int32_t goalArea) -> void
{
    if (goalArea < 0 || goalArea >= numAreas)
    {
        char message[256];
        std::snprintf(message, sizeof(message), " GlobalMap.setGoalDoor: bad goalArea (%d of %d) ", goalArea, numAreas);
        Fatal(0, message);
    }

    GlobalMapDoor& goalDoor = doors[numDoors + 1];
    const GlobalMapArea& area = areas[goalArea];
    goalDoor.area[0] = static_cast<int16_t>(goalArea);
    goalDoor.area[1] = static_cast<int16_t>(goalArea);
    goalSectorR = area.sectorR;
    goalSectorC = area.sectorC;
    InitTempDoor(goalDoor, goalArea, area.numDoors);

    for (int32_t i = 0; i < goalDoor.numLinks[0]; i++)
    {
        const DoorInfo& info = area.doors[i];
        DoorLink& link = goalDoor.links[0][i];
        link.doorIndex = info.doorIndex;
        link.doorSide = info.doorSide;
        link.cost = 1;
        Assert(info.doorIndex >= 0 && info.doorIndex < numDoors + 2 ? 1 : 0, static_cast<uint32_t>(info.doorIndex),
               " GlobalMap.setGoalDoor: bad doorIndex ");
        GlobalMapDoor& areaDoor = doors[info.doorIndex];
        const int32_t side = info.doorSide;
        DoorLink& backLink = areaDoor.links[side][areaDoor.numLinks[side]];
        backLink.doorIndex = static_cast<int16_t>(numDoors + 1);
        backLink.doorSide = 0;
        backLink.cost = 1;
        areaDoor.numLinks[side]++;
    }
}

auto GlobalMap::resetGoalDoor(int32_t goalArea) -> void
{
    const GlobalMapDoor& goalDoor = doors[numDoors + 1];

    for (int32_t i = 0; i < goalDoor.numLinks[0]; i++)
    {
        const DoorInfo& info = areas[goalArea].doors[i];
        doors[info.doorIndex].numLinks[static_cast<int32_t>(info.doorSide)]--;
    }
}

auto GlobalMap::calcHPrime(int32_t door) -> int32_t
{
    Assert(door >= 0 && door < numDoors + 2 ? 1 : 0, 0xffffffff, " CalcHPrime: Bad Door ");
    const GlobalMapArea& area0 = areas[doors[door].area[0]];
    const GlobalMapArea& area1 = areas[doors[door].area[1]];
    const int32_t sectorR = (area1.sectorR + area0.sectorR) / 2;
    const int32_t sectorC = (area0.sectorC + area1.sectorC) / 2;
    const int32_t rowDistance = goalSectorR < sectorR ? sectorR - goalSectorR : goalSectorR - sectorR;
    const int32_t colDistance = goalSectorC < sectorC ? sectorC - goalSectorC : goalSectorC - sectorC;
    return rowDistance + colDistance;
}

auto GlobalMap::calcPath(int32_t startArea, int32_t goalArea, GlobalPathStep* path) -> int32_t
{
    if (startArea == -1 || goalArea == -1)
    {
        return -1;
    }

    if (openList == nullptr)
    {
        openList = new PriorityQueue;

        if (openList == nullptr)
        {
            Fatal(0, " Unable to create MoveMap::openList ");
        }

        openList->init(5000, -2000000);
    }

    const int32_t startDoor = numDoors;
    const int32_t goalDoor = numDoors + 1;

    for (int32_t i = 0; i < numDoors + 2; i++)
    {
        GlobalMapDoor& door = doors[i];
        door.cost = 1;
        door.parent = -1;
        door.fromAreaIndex = -1;
        door.flags = 0;
        door.g = 0;
        door.hPrime = -1;
        door.fPrime = 0;
    }

    setStartDoor(startArea);
    setGoalDoor(goalArea);

    openList->clear();
    PQNode startNode = {};
    startNode.key = 0;
    startNode.id = startDoor;

    if (openList->insert(startNode) != 0)
    {
        Fatal(0, "PathFind OPEN overflow");
    }

    doors[startDoor].flags |= 1;

    int goalFound = 0;

    while (openList->size() != 0)
    {
        PQNode best;
        openList->remove(best);
        const int32_t curIndex = best.id;
        GlobalMapDoor& current = doors[curIndex];
        const int32_t g = current.g;
        current.flags = (current.flags & ~1u) | 2;

        if (curIndex == goalDoor)
        {
            goalFound = 1;
            break;
        }

        const int32_t side = 1 - current.fromAreaIndex;
        const int32_t thruArea = current.area[side];
        const int32_t numLinks = current.numLinks[side];

        for (int32_t i = 0; i < numLinks; i++)
        {
            const DoorLink& link = current.links[side][i];
            const int32_t succIndex = link.doorIndex;
            Assert(succIndex >= 0 && succIndex < numDoors + 2 ? 1 : 0, 0, " Bad Door Index ");
            const int32_t linkCost = link.cost;
            GlobalMapDoor& successor = doors[succIndex];

            if (successor.open == 0 || linkCost >= 10000)
            {
                continue;
            }

            if (successor.hPrime == -1)
            {
                successor.hPrime = calcHPrime(succIndex);
            }

            const int32_t newG = g + linkCost;
            const int32_t succSide = successor.area[1] == thruArea ? 1 : 0;

            if ((successor.flags & 1) == 0)
            {
                if ((successor.flags & 2) == 0)
                {
                    successor.fromAreaIndex = succSide;
                    successor.parent = curIndex;
                    successor.g = newG;
                    successor.fPrime = newG + successor.hPrime;
                    successor.cost = linkCost;
                    PQNode node = {};
                    node.key = successor.fPrime;
                    node.id = succIndex;

                    if (openList->insert(node) != 0)
                    {
                        Fatal(0, "PathFind OPEN overflow");
                    }

                    successor.flags |= 1;
                }
                else if (newG < successor.g)
                {
                    // A cheaper way to a closed door: reparent it and push the saving on.
                    successor.cost = linkCost;
                    successor.parent = curIndex;
                    successor.fromAreaIndex = succSide;
                    propogateCost(succIndex, linkCost, succSide, g);
                }
            }
            else if (newG < successor.g)
            {
                successor.fromAreaIndex = succSide;
                successor.cost = linkCost;
                successor.fPrime = successor.hPrime + newG;
                successor.parent = curIndex;
                successor.g = newG;
                const int32_t itemIndex = openList->find(succIndex);

                if (itemIndex == 0)
                {
                    char message[256];
                    std::snprintf(message, sizeof(message),
                                  "GlobalMap.calcPath: Cannot find globalmap door [%d, %d, %d, %d] for change\n",
                                  succIndex, i, succSide, linkCost);
                    DebugOpenList(message);
                    Fatal(0, "GlobalMap.calcPath: Save OPENLIST.DBG file for Glenn!");
                }

                openList->change(itemIndex, successor.fPrime);
            }
        }
    }

    resetStartDoor(startArea);
    resetGoalDoor(goalArea);

    if (goalFound == 0)
    {
        return 0;
    }

    int32_t count = 1;

    for (int32_t door = goalDoor; door != startDoor; door = doors[door].parent)
    {
        count++;
    }

    const int32_t numSteps = count - 1;
    Assert(numSteps < MAX_GLOBAL_PATH ? 1 : 0, static_cast<uint32_t>(numSteps), " Too Many Long Range Move Steps ");
    int32_t costToGoal = 0;
    int32_t door = goalDoor;

    for (int32_t i = numSteps - 1; i >= 0; i--)
    {
        // Port fix: the original writes past the caller's MAX_GLOBAL_PATH steps when the assert above fails.
        if (i < MAX_GLOBAL_PATH)
        {
            path[i].thruArea = doors[door].area[doors[door].fromAreaIndex];
            path[i].goalDoor = door;
            path[i].costToGoal = costToGoal;
        }

        costToGoal += doors[door].cost;
        door = doors[door].parent;
    }

    if (pathCostTable != nullptr)
    {
        uint8_t& entry = pathCostTable[numAreas * startArea + goalArea];

        if (entry != numSteps)
        {
            entry = count > 0xff ? 0xff : static_cast<uint8_t>(numSteps);
        }
    }

    return numSteps;
}

auto GlobalMap::propogateCost(int32_t door, int32_t cost, int32_t fromSide, int32_t g) -> void
{
    Assert(door >= 0 && door < numDoors + 2 && (fromSide == 0 || fromSide == 1) && g >= 0 ? 1 : 0, 0xffffffff,
           " Bad Door Propogate ");
    const int32_t newG = cost + g;
    GlobalMapDoor& current = doors[door];

    if (newG >= current.g)
    {
        return;
    }

    current.g = newG;
    current.fPrime = current.hPrime + newG;

    if ((current.flags & 1) != 0)
    {
        if (openList->find(door) == 0)
        {
            char message[256];
            std::snprintf(message, sizeof(message),
                          "GlobalMap.propogateCost: Cannot find globalmap door [%d, %d, %d, %d] for change\n", door,
                          cost, fromSide, g);
            DebugOpenList(message);
            Fatal(0, "GlobalMap.propogateCost: Save OPENLIST.DBG file for Glenn!");
        }

        // Original behaviour (OB-025): passes the door number where PriorityQueue::change wants the heap index.
        openList->change(door, current.fPrime);
        return;
    }

    const int32_t side = 1 - fromSide;
    const int32_t numLinks = current.numLinks[side];

    for (int32_t i = 0; i < numLinks; i++)
    {
        const DoorLink& link = current.links[side][i];
        const int32_t nextIndex = link.doorIndex;
        Assert(nextIndex >= 0 && nextIndex < numDoors + 2 ? 1 : 0, 0, " Bad Door Index ");
        const int32_t linkCost = link.cost;
        GlobalMapDoor& next = doors[nextIndex];
        const int32_t nextSide = next.area[1] == current.area[side] ? 1 : 0;

        if (next.open == 0 || linkCost >= 10000 || next.hPrime == -1)
        {
            continue;
        }

        if (door == next.parent)
        {
            // Original behaviour (OB-026): passes this door's exit side, not the next door's entry side.
            propogateCost(nextIndex, linkCost, side, current.g);
        }
        else if (current.g + linkCost < next.g)
        {
            next.cost = linkCost;
            next.parent = door;
            next.fromAreaIndex = nextSide;
            propogateCost(nextIndex, linkCost, nextSide, current.g);
        }
    }
}

auto GlobalMap::calcPath(vector_3d start, vector_3d goal, GlobalPathStep* path) -> int32_t
{
    int32_t startR = 0;
    int32_t startC = 0;
    GameMap->worldToMapTilePos(start, startR, startC);
    int32_t goalR = 0;
    int32_t goalC = 0;
    GameMap->worldToMapTilePos(goal, goalR, goalC);
    const int32_t goalArea = calcArea(goalR, goalC);
    const int32_t startArea = calcArea(startR, startC);
    return calcPath(startArea, goalArea, path);
}

auto GlobalMap::getPathCost(int32_t startArea, int32_t goalArea) -> int32_t
{
    if (startArea < 0 || goalArea < 0)
    {
        return 0;
    }

    return pathCostTable[numAreas * startArea + goalArea];
}

auto GlobalMap::openDoor(int32_t door) -> void
{
    doors[door].open = 1;
}

auto GlobalMap::closeDoor(int32_t door) -> void
{
    doors[door].open = 0;
}

auto GlobalMap::closeArea(int32_t area) -> void
{
    GlobalMapArea& closing = areas[area];
    closing.closed = 1;

    for (int32_t i = 0; i < closing.numDoors; i++)
    {
        closeDoor(closing.doors[i].doorIndex);
    }

    for (int32_t i = 0; i < numAreas; i++)
    {
        pathCostTable[numAreas * i + area] = 0;
        pathCostTable[numAreas * area + i] = 0;
    }
}

auto GlobalMap::print(char* fileName, int32_t ULr, int32_t ULc, int32_t printHeight, int32_t printWidth) -> void
{
    // Port fix: the original tests the other way round (it prints only when areaMap is null, and then reads through
    // the null pointer).
    if (areaMap == nullptr)
    {
        return;
    }

    File* debugFile = new File;
    debugFile->create(fileName);
    char line[512];
    std::snprintf(line, sizeof(line), "ULr: %d, ULc: %d, h: %d, w: %d\n", ULr, ULc, printHeight, printWidth);
    debugFile->writeString(line);

    for (int32_t row = ULr; row < ULr + printHeight; row++)
    {
        line[0] = '\0';

        for (int32_t col = ULc; col < ULc + printWidth; col++)
        {
            const int16_t area = areaMap[width * row + col];
            char cell[16];

            if (area == -2)
            {
                std::strcpy(cell, ">< ");
            }
            else if (area == -1)
            {
                std::strcpy(cell, "** ");
            }
            else
            {
                std::snprintf(cell, sizeof(cell), "%02x ", area);
            }

            std::strcat(line, cell);
        }

        std::strcat(line, "\n");
        debugFile->writeString(line);
    }

    debugFile->writeString("\n");
    debugFile->close();
    delete debugFile;
}

auto GlobalMap::calcArea(int32_t tileR, int32_t tileC) -> int32_t
{
    // Port fix: the original reads outside the area map for a goal off the map. Off the map is in no area.
    if (tileR < 0 || tileR >= height || tileC < 0 || tileC >= width)
    {
        return -1;
    }

    if (smallAreaMap == nullptr)
    {
        const int32_t area = areaMap[width * tileR + tileC];
        return area < 0 ? -1 : area;
    }

    const int32_t area = smallAreaMap[width * tileR + tileC];
    return area == 0xff ? -1 : area;
}

auto MoveMap::operator new(size_t size) noexcept -> void*
{
    return systemHeap->malloc(static_cast<uint32_t>(size));
}

auto MoveMap::operator delete(void* ptr) -> void
{
    systemHeap->free(ptr);
}

namespace
{
    /// <summary>The "no position" value MoveMap::clear and setStart/setGoal store (0xc97423f0).</summary>
    constexpr float NO_POSITION = -999999.0f;

    /// <summary>
    /// The weights ClearBridgeTiles lowers by 10000 in the first move level of the overlay weights (the centre
    /// column of the two bridge overlays' cells, and of the two railroad bridges').
    /// </summary>
    constexpr int32_t BRIDGE_WEIGHT_CELLS[12] = {334, 337, 340, 352, 355, 358, 496, 499, 502, 514, 517, 520};

    /// <summary>Adds <paramref name="delta"/> to the bridge cells of <paramref name="weights"/>.</summary>
    void AdjustBridgeWeights(int32_t* weights, int32_t delta)
    {
        for (const int32_t cell : BRIDGE_WEIGHT_CELLS)
        {
            weights[cell] += delta;
        }
    }

    /// <summary>A cost plus <paramref name="delta"/>, at least 1.</summary>
    int32_t AddCost(int32_t cost, int32_t delta)
    {
        const int32_t result = cost + delta;
        return result < 1 ? 1 : result;
    }

    /// <summary>The open-list id of a MoveMap cell.</summary>
    int32_t CellId(int32_t r, int32_t c)
    {
        return c + r * 1000;
    }

    /// <summary>Whether a standing mover blocks MoveMap cells (not an elemental, not the mover itself, alive).</summary>
    bool IsBlockingMover(GameObject* object)
    {
        return object->objectClass != ELEMENTAL && object != MovingObject && object != RamObject &&
               object->isDisabled() == 0;
    }

    /// <summary>MoveMap::markGoalCells's per-cell "free" state of the goal door (a function static in MCX).</summary>
    /// <remarks>MCX.EXE @ 0x0080bf00</remarks>
    char doorCellState[256];

    /// <summary>Whether cellShiftDistance has been filled (once per search function in MCX).</summary>
    /// <remarks>MCX.EXE @ 0x00808024 (calcPath) and 0x00808028 (calcEscapePath).</remarks>
    int cellShiftDistanceReady[2] = {};
}

auto MoveMap::init(int32_t newMaxWidth, int32_t newMaxHeight) -> void
{
    maxWidth = newMaxWidth;
    width = newMaxWidth;
    maxHeight = newMaxHeight;
    maxCellHeight = newMaxHeight * MAPCELL_DIM;
    cellHeight = newMaxHeight * MAPCELL_DIM;
    height = newMaxHeight;
    maxCellWidth = newMaxWidth * MAPCELL_DIM;
    cellWidth = newMaxWidth * MAPCELL_DIM;
    map = static_cast<MoveMapNode*>(
        systemHeap->malloc(static_cast<uint32_t>(maxCellHeight * maxCellWidth * sizeof(MoveMapNode))));

    if (map == nullptr)
    {
        Fatal(0, "Not enough Memory for MoveMap");
    }

    clear();
}

auto MoveMap::init(FitIniFile* mapFile) -> int32_t
{
    int32_t result = mapFile->seekBlock("Header");

    if (result != 0)
    {
        return result;
    }

    char fileType[128];
    result = mapFile->readIdString("FileType", fileType, 127);

    if (result != 0)
    {
        return result;
    }

    if (std::strcmp(fileType, "MoveMap") != 0)
    {
        return -1;
    }

    result = mapFile->seekBlock("MapData");

    if (result != 0)
    {
        return result;
    }

    result = mapFile->readIdLong("Height", height);

    if (result != 0)
    {
        return result;
    }

    result = mapFile->readIdLong("Width", width);

    if (result != 0)
    {
        return result;
    }

    init(height, width); // as the original: the height goes to init's width slot (maps are square)
    const int32_t tileRows = height;
    const int32_t tileCols = width;
    char* vertexCost = static_cast<char*>(systemHeap->malloc(static_cast<uint32_t>(tileRows * tileCols)));
    result = mapFile->readIdCharArray("VertexCost", vertexCost, static_cast<uint32_t>(tileRows * tileCols));

    if (result != 0)
    {
        return result;
    }

    for (int32_t row = 0; row < height; row++)
    {
        for (int32_t col = 0; col < width; col++)
        {
            for (int32_t cellR = 0; cellR < MAPCELL_DIM; cellR++)
            {
                for (int32_t cellC = 0; cellC < MAPCELL_DIM; cellC++)
                {
                    map[cellWidth * (row * 3 + cellR) + col * 3 + cellC].cost = vertexCost[col + width * row];
                }
            }
        }
    }

    systemHeap->free(vertexCost);
    return 0;
}

auto MoveMap::clear() -> void
{
    const int32_t numCells = cellHeight * maxCellWidth;

    for (int32_t i = 0; i < numCells; i++)
    {
        map[i].parent = -1;
        map[i].flags = 0;
        map[i].hPrime = -1;
    }

    goalPos = vector_3d(0.0f, 0.0f, 0.0f);
    target = vector_3d(NO_POSITION, NO_POSITION, NO_POSITION);
}

auto MoveMap::placeMovers(int) -> void
{
    auto placeList = [this](ObjectQueueNode* list, int bridgeCost)
    {
        BaseObject* current = nullptr;

        while (list->Traverse(current) != nullptr)
        {
            GameObject* object = static_cast<GameObject*>(current);

            if (!IsBlockingMover(object))
            {
                continue;
            }

            const ObjectPosition* position = object->getObjPosition();
            const int32_t r = (position->tileR - ULr) * 3 + position->cellR;
            const int32_t c = (position->tileC - ULc) * 3 + position->cellC;

            if (r < 0 || r >= cellHeight || c < 0 || c >= cellWidth || (r == startR && c == startC))
            {
                continue;
            }

            MoveMapNode& node = map[maxCellWidth * r + c];

            if ((node.flags & 8) != 0)
            {
                continue;
            }

            MovePath* path = object->getPilot()->getMovePath();

            if (path == nullptr || path->numSteps != 0)
            {
                continue;
            }

            node.flags |= 0x10;
            const uint32_t overlay = GameMap->map[GameMap->width * position->tileR + position->tileC].overlay & 0x7f;
            node.cost = AddCost(node.cost, OverlayIsBridge[overlay] == 0 ? 20000 : bridgeCost);
        }
    };

    // Original behaviour (OB-030): Inner Sphere mechs standing on a bridge cost 3333 to pass, Clan ones nothing extra.
    placeList(innerSphereMechList, 0xd05);
    placeList(clanMechList, 0);
}

auto MoveMap::setTarget(vector_3d targetPos) -> void
{
    target = targetPos;
}

auto MoveMap::setStart(vector_3d* newStartPos, int32_t newStartR, int32_t newStartC) -> void
{
    if (newStartPos == nullptr)
    {
        startPos = vector_3d(NO_POSITION, NO_POSITION, NO_POSITION);
    }
    else
    {
        startPos = *newStartPos;
    }

    if (newStartR == -1)
    {
        int32_t tileR = 0;
        int32_t tileC = 0;
        int32_t cellR = 0;
        int32_t cellC = 0;
        GameMap->worldToMapPos(*newStartPos, tileR, tileC, cellR, cellC);
        startR = (tileR - ULr) * 3 + cellR;
        startC = (tileC - ULc) * 3 + cellC;
        return;
    }

    startR = newStartR;
    startC = newStartC;
}

auto MoveMap::setGoal(vector_3d newGoalPos, int32_t newGoalR, int32_t newGoalC) -> void
{
    goalPos = newGoalPos;

    if (newGoalR == -1)
    {
        int32_t tileR = 0;
        int32_t tileC = 0;
        int32_t cellR = 0;
        int32_t cellC = 0;
        GameMap->worldToMapPos(goalPos, tileR, tileC, cellR, cellC);
        goalR = (tileR - ULr) * 3 + cellR;
        goalC = (tileC - ULc) * 3 + cellC;
    }
    else
    {
        goalR = newGoalR;
        goalC = newGoalC;
    }

    doorDirection = -1;
    GoalIsDoor = 0;
}

auto MoveMap::setGoal(int32_t thruArea, int32_t goalDoor) -> void
{
    goalPos = vector_3d(NO_POSITION, NO_POSITION, NO_POSITION);
    door = goalDoor;
    GoalIsDoor = 1;
    // Per door direction (1 east-west, 2 north-south) and side: the direction the door is entered from.
    static constexpr int32_t entryDirection[8] = {-1, -1, 1, 3, 2, 0, -1, -1};
    const GlobalMapDoor& goal = GlobalMoveMap->doors[goalDoor];
    doorSide = goal.area[1] == thruArea ? 1 : 0;
    const int32_t direction = goal.direction[0];
    Assert(direction == 1 || direction == 2 ? 1 : 0, 0, " MoveMap: Bad Area Door Direction in setGoal() ");
    doorDirection = entryDirection[doorSide + direction * 2];

    if (doorDirection == 0 || doorDirection == 2)
    {
        goalR = goal.row * 3 + doorSide + (goal.cellR - minRow);
        goalC = ((goal.col * 3 + goal.cellC) - minCol) + goal.length / 2;
    }
    else if (doorDirection == 1 || doorDirection == 3)
    {
        goalR = (goal.cellR - minRow) + goal.row * 3 + goal.length / 2;
        goalC = ((goal.col * 3 + goal.cellC) - minCol) + doorSide;
    }
}

auto cellFacing(GameObject* object) -> int32_t
{
    if (object == nullptr)
    {
        return 0;
    }

    vector_3d ahead = object->getPosition();
    ahead.y = static_cast<float>(static_cast<double>(ahead.y) + 50.0);
    const float facing = object->relFacingTo(ahead, -1);

    if (facing < -157.5f)
    {
        return 4;
    }

    if (facing < -112.5f)
    {
        return 3;
    }

    if (facing < -67.5f)
    {
        return 2;
    }

    if (facing < -22.5f)
    {
        return 1;
    }

    if (facing < 22.5f)
    {
        return 0;
    }

    if (facing < 67.5f)
    {
        return 7;
    }

    if (facing < 112.5f)
    {
        return 6;
    }

    if (facing < 157.5f)
    {
        return 5;
    }

    return 4;
}

auto MoveMap::setUp(ScenarioMap* scenarioMap, int32_t newULr, int32_t newULc, int32_t newHeight, int32_t newWidth,
                    vector_3d* newStartPos, int32_t newStartR, int32_t newStartC, vector_3d newGoalPos,
                    int32_t newGoalR, int32_t newGoalC, int32_t* newOverlayWeightTable, int32_t newMoveLevel,
                    int32_t newJumpCost, int32_t newNumOffsets, uint32_t params) -> int32_t
{
    if (map == nullptr)
    {
        init(newHeight, newWidth); // as the original: the height goes to init's width slot (windows are square)
    }
    else
    {
        width = newWidth;
        height = newHeight;
        cellWidth = newWidth * 3;
        cellHeight = newHeight * 3;
        clear();
    }

    ULr = newULr;
    ULc = newULc;
    minCol = newULc * 3;
    minRow = newULr * 3;
    overlayWeightTable = newOverlayWeightTable == nullptr ? OverlayWeightTable : newOverlayWeightTable;
    moveLevel = newMoveLevel;
    jumpCost = newJumpCost;
    numOffsets = newNumOffsets;
    setStart(newStartPos, newStartR, newStartC);
    setGoal(newGoalPos, newGoalR, newGoalC);

    if (ClearBridgeTiles != 0)
    {
        AdjustBridgeWeights(overlayWeightTable, -10000);
    }

    int checkMines = 1;
    const int32_t lockCost = moveLevel << 3;

    if (MovingObject != nullptr && MovingObject->objectClass == GROUNDVEHICLE &&
        static_cast<GroundVehicle*>(MovingObject)->mineSweeper != 0)
    {
        checkMines = 0;
    }

    for (int32_t row = 0; row < height; row++)
    {
        for (int32_t col = 0; col < width; col++)
        {
            const int32_t tileR = ULr + row;
            const int32_t tileC = ULc + col;

            if (tileR < 0 || tileR >= GameMap->height || tileC < 0 || tileC >= GameMap->width)
            {
                continue;
            }

            Assert(tileR >= 0 && tileR < scenarioMap->height && tileC >= 0 && tileC < scenarioMap->width ? 1 : 0, 0,
                   " Map Tile out of bounds ");
            const MapTile tile = scenarioMap->map[scenarioMap->width * tileR + tileC];
            MoveMapNode* tileNodes = &map[maxCellWidth * row * 3 + col * 3];
            auto node = [&](int32_t cellR, int32_t cellC) -> MoveMapNode&
            { return tileNodes[maxCellWidth * cellR + cellC]; };

            for (int32_t cellR = 0; cellR < MAPCELL_DIM; cellR++)
            {
                for (int32_t cellC = 0; cellC < MAPCELL_DIM; cellC++)
                {
                    node(cellR, cellC).cost = TileCellPassable(tile, cellR, cellC) != 0 ? moveLevel : 10000;
                }
            }

            const uint32_t overlay = tile.overlay & 0x7f;

            if (overlay != 0)
            {
                int32_t weightOverlay = static_cast<int32_t>(overlay);

                if (overlay >= FIRST_GATE_OVERLAY && overlay <= LAST_GATE_OVERLAY)
                {
                    weightOverlay = GateOverlay(overlay, MovingObject->getAlignment());
                }

                for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++)
                {
                    const int32_t weight =
                        weightOverlay == -1 ? 20000 : overlayWeightTable[OverlayWeightIndex[weightOverlay] + cell];
                    node(cell / 3, cell % 3).cost = AddCost(node(cell / 3, cell % 3).cost, weight);
                }
            }

            uint32_t locks = (tile.overlay >> 15) & 0x1ff;

            if (locks != 0 && (params & 0x80) != 0)
            {
                for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++, locks >>= 1)
                {
                    if ((locks & 1) != 0)
                    {
                        node(cell / 3, cell % 3).cost = AddCost(node(cell / 3, cell % 3).cost, lockCost);
                    }
                }
            }

            if (checkMines != 0)
            {
                const int32_t alignment = MovingObject->getAlignment();
                const uint32_t knownMines = (tile.overlay >> (alignment == -1 ? 0x19 : 0x1b)) & 3;

                if (knownMines != 0)
                {
                    for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++)
                    {
                        node(cell / 3, cell % 3).cost = AddCost(node(cell / 3, cell % 3).cost, moveLevel << knownMines);
                    }
                }

                const uint32_t ownMines = (tile.overlay >> (alignment == -1 ? 0xb : 0xd)) & 3;

                if (ownMines == 3 || ownMines == 1)
                {
                    for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++)
                    {
                        node(cell / 3, cell % 3).cost =
                            AddCost(node(cell / 3, cell % 3).cost, (moveLevel << ownMines) * -2);
                    }
                }
            }

            // A mine layer laying mines is drawn to the middle of each tile.
            if (MovingObject->objectClass == GROUNDVEHICLE)
            {
                GroundVehicle* vehicle = static_cast<GroundVehicle*>(MovingObject);

                if (vehicle->mineLayer != 0 && vehicle->pilot->curTacOrder.moveParams.mode == 1)
                {
                    node(1, 1).cost = AddCost(node(1, 1).cost, moveLevel * -16);
                }
            }
        }
    }

    if (FindingEscapePath == 0)
    {
        // Port fix: the original marks a goal outside the window too, writing outside map, before searchPath stops
        // on it (" Bad Move Goal "). The port leaves the mark out so that Fatal is what reports it.
        if (goalR >= 0 && goalR < cellHeight && goalC >= 0 && goalC < cellWidth)
        {
            map[goalR * maxCellWidth + goalC].flags |= 8;
        }
    }
    else
    {
        markEscapeGoalCells(newGoalPos);
    }

    if ((params & 0x40) != 0)
    {
        placeMovers(1);
    }

    return 0;
}

auto MoveMap::markEscapeGoalCells(vector_3d escapeGoal) -> int32_t
{
    int32_t goalTileR = 0;
    int32_t goalTileC = 0;
    int32_t goalCellR = 0;
    int32_t goalCellC = 0;
    GameMap->worldToMapPos(escapeGoal, goalTileR, goalTileC, goalCellR, goalCellC);
    const int32_t goalArea = GlobalMoveMap->calcArea(goalTileR, goalTileC);

    for (int32_t row = 0; row < height; row++)
    {
        for (int32_t col = 0; col < width; col++)
        {
            const int32_t tileR = ULr + row;
            const int32_t tileC = ULc + col;

            if (tileR < 0 || tileR >= GameMap->height || tileC < 0 || tileC >= GameMap->width)
            {
                continue;
            }

            const int32_t area = GlobalMoveMap->calcArea(tileR, tileC);
            const int32_t cost = GlobalMoveMap->getPathCost(area, goalArea);

            if (area != goalArea && cost <= 0)
            {
                continue;
            }

            for (int32_t cellR = 0; cellR < MAPCELL_DIM; cellR++)
            {
                for (int32_t cellC = 0; cellC < MAPCELL_DIM; cellC++)
                {
                    map[maxCellWidth * (row * 3 + cellR) + col * 3 + cellC].flags |= 8;
                }
            }
        }
    }

    return 0;
}

auto MoveMap::setUp(ScenarioMap* scenarioMap, int32_t newULr, int32_t newULc, int32_t newHeight, int32_t newWidth,
                    vector_3d* newStartPos, int32_t newStartR, int32_t newStartC, int32_t thruArea, int32_t goalDoor,
                    vector_3d targetPos, int32_t* newOverlayWeightTable, int32_t newMoveLevel, int32_t newJumpCost,
                    int32_t newNumOffsets, uint32_t params) -> int32_t
{
    if (map == nullptr)
    {
        init(newHeight, newWidth); // as the original: the height goes to init's width slot (windows are square)
    }
    else
    {
        width = newWidth;
        height = newHeight;
        cellWidth = newWidth * 3;
        cellHeight = newHeight * 3;
        clear();
    }

    ULr = newULr;
    ULc = newULc;
    minCol = newULc * 3;
    minRow = newULr * 3;
    overlayWeightTable = newOverlayWeightTable == nullptr ? OverlayWeightTable : newOverlayWeightTable;
    moveLevel = newMoveLevel;
    jumpCost = newJumpCost;
    numOffsets = newNumOffsets;
    setStart(newStartPos, newStartR, newStartC);
    setGoal(thruArea, goalDoor);

    if (ClearBridgeTiles != 0)
    {
        AdjustBridgeWeights(overlayWeightTable, -10000);
    }

    for (int32_t row = 0; row < height; row++)
    {
        for (int32_t col = 0; col < width; col++)
        {
            const int32_t tileR = ULr + row;
            const int32_t tileC = ULc + col;

            if (tileR < 0 || tileR >= GameMap->height || tileC < 0 || tileC >= GameMap->width)
            {
                continue;
            }

            Assert(tileR >= 0 && tileR < scenarioMap->height && tileC >= 0 && tileC < scenarioMap->width ? 1 : 0, 0,
                   " Map Tile out of bounds ");
            const MapTile tile = scenarioMap->map[scenarioMap->width * tileR + tileC];
            MoveMapNode* tileNodes = &map[maxCellWidth * row * 3 + col * 3];
            auto node = [&](int32_t cellR, int32_t cellC) -> MoveMapNode&
            { return tileNodes[maxCellWidth * cellR + cellC]; };

            for (int32_t cellR = 0; cellR < MAPCELL_DIM; cellR++)
            {
                for (int32_t cellC = 0; cellC < MAPCELL_DIM; cellC++)
                {
                    node(cellR, cellC).cost = TileCellPassable(tile, cellR, cellC) != 0 ? moveLevel : 10000;
                }
            }

            const uint32_t overlay = tile.overlay & 0x7f;

            if (overlay != 0)
            {
                int32_t weightOverlay = static_cast<int32_t>(overlay);

                if (overlay >= FIRST_GATE_OVERLAY && overlay <= LAST_GATE_OVERLAY)
                {
                    weightOverlay = GateOverlay(overlay, MovingObject->getAlignment());
                }

                for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++)
                {
                    const int32_t weight =
                        weightOverlay == -1 ? 20000 : overlayWeightTable[OverlayWeightIndex[weightOverlay] + cell];
                    node(cell / 3, cell % 3).cost = AddCost(node(cell / 3, cell % 3).cost, weight);
                }
            }

            // Unlike the other setUp: a lock costs moveLevel (not 8 x), and only the known mines count.
            uint32_t locks = (tile.overlay >> 15) & 0x1ff;

            if (locks != 0 && (params & 0x80) != 0)
            {
                for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++, locks >>= 1)
                {
                    if ((locks & 1) != 0)
                    {
                        node(cell / 3, cell % 3).cost = AddCost(node(cell / 3, cell % 3).cost, moveLevel);
                    }
                }
            }

            const uint32_t knownMines = (tile.overlay >> (MovingObject->getAlignment() == -1 ? 0x19 : 0x1b)) & 3;

            for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++)
            {
                if (knownMines != 0)
                {
                    node(cell / 3, cell % 3).cost = AddCost(node(cell / 3, cell % 3).cost, moveLevel << knownMines);
                }
            }
        }
    }

    if (markGoalCells(targetPos) == 0)
    {
        return -1;
    }

    if ((params & 0x40) != 0)
    {
        placeMovers(1);
    }

    return 0;
}

auto MoveMap::markGoalCells(vector_3d targetPos) -> int32_t
{
    const GlobalMapDoor& goal = GlobalMoveMap->doors[door];
    const int32_t length = goal.length;

    if (length > 0)
    {
        std::memset(doorCellState, 1, static_cast<size_t>(length));
    }

    Assert(door >= 0 && door < GlobalMoveMap->numDoors ? 1 : 0, 0, " FUDGE 1");
    Assert(goal.direction[0] == 1 || goal.direction[0] == 2 ? 1 : 0, 0, " FUDGE 2");
    Assert(goal.length >= 1 && goal.length <= 0x3ff ? 1 : 0, 0, " FUDGE 3");

    // Clears the door cells a standing mech occupies (on either side of the door).
    const bool alongRows = goal.direction[0] == 1;
    const int32_t doorRow = tileMulMAPCELL_DIM[goal.row] + goal.cellR;
    const int32_t doorCol = tileMulMAPCELL_DIM[goal.col] + goal.cellC;
    auto clearOccupied = [&](ObjectQueueNode* list)
    {
        BaseObject* current = nullptr;

        while (list->Traverse(current) != nullptr)
        {
            GameObject* object = static_cast<GameObject*>(current);

            if (!IsBlockingMover(object))
            {
                continue;
            }

            const ObjectPosition* position = object->getObjPosition();
            int32_t index;

            if (alongRows)
            {
                if (position->mapCellR < doorRow || position->mapCellR >= length + doorRow ||
                    position->mapCellC < doorCol || position->mapCellC >= doorCol + 2)
                {
                    continue;
                }

                index = position->mapCellR - doorRow;
            }
            else
            {
                if (position->mapCellR < doorRow || position->mapCellR >= doorRow + 2 || position->mapCellC < doorCol ||
                    position->mapCellC >= length + doorCol)
                {
                    continue;
                }

                index = position->mapCellC - doorCol;
            }

            Assert(index >= 0 && index < length ? 1 : 0, 0, " Bad Cell Index ");
            doorCellState[index] = 0;
        }
    };

    clearOccupied(innerSphereMechList);
    clearOccupied(clanMechList);

    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    GameMap->worldToMapPos(targetPos, tileR, tileC, cellR, cellC);
    const int32_t targetR = cellR + (tileR * 3 - minRow);
    const int32_t targetC = cellC + (tileC * 3 - minCol);
    int32_t numMarked = 0;
    const int32_t half = length / 2;
    const int32_t step = moveLevel / 2;

    if (doorDirection == 0 || doorDirection == 2)
    {
        // The door runs along goal row goalR; the target just past it is the only goal.
        const int32_t firstCol = goalC - half;
        const int32_t beyondRow = doorSide == 0 ? goalR + 1 : goalR - 1;

        if (targetR == beyondRow && targetC >= firstCol && targetC < firstCol + length)
        {
            map[maxCellWidth * goalR + targetC].flags |= 8;
            return 1;
        }

        // Else every free door cell is a goal, costing more the further from the middle.
        for (int32_t i = 0; i < length; i++)
        {
            if (doorCellState[i] == 0)
            {
                continue;
            }

            MoveMapNode& node = map[maxCellWidth * goalR + i + firstCol];
            node.flags |= 8;
            numMarked++;
            node.cost = AddCost(node.cost, (i < half ? half - i : i - half) * step);
        }
    }
    else if (doorDirection == 1 || doorDirection == 3)
    {
        const int32_t firstRow = goalR - half;

        if (targetC == goalC + doorSide * -2 + 1 && targetR >= firstRow && targetR < length + firstRow)
        {
            map[maxCellWidth * targetR + goalC].flags |= 8;
            return 1;
        }

        for (int32_t i = 0; i < length; i++)
        {
            if (doorCellState[i] == 0)
            {
                continue;
            }

            MoveMapNode& node = map[maxCellWidth * (firstRow + i) + goalC];
            node.flags |= 8;
            numMarked++;
            node.cost = AddCost(node.cost, (i < half ? half - i : i - half) * step);
        }
    }

    return numMarked;
}

auto MoveMap::adjacentCellOpen(int32_t r, int32_t c, int32_t dir) -> int
{
    const int32_t nextR = cellShift[dir * 2] + r;
    const int32_t nextC = cellShift[dir * 2 + 1] + c;

    if (nextR < 0 || nextR >= cellHeight || nextC < 0 || nextC >= cellWidth)
    {
        return 0;
    }

    const MoveMapNode& node = map[maxCellWidth * nextR + nextC];

    if ((node.flags & 0x10) != 0)
    {
        return 0;
    }

    // Original behaviour (OB-027): the known-mine bits are read from the map tile at the window cell's coordinates.
    const uint32_t mineBits = MovingObject->getAlignment() == -1 ? 0x6000000u : 0x18000000u;
    Assert(nextR >= 0 && nextR < GameMap->height && nextC >= 0 && nextC < GameMap->width ? 1 : 0, 0,
           " Map Tile out of bounds ");

    if ((GameMap->map[GameMap->width * nextR + nextC].overlay & mineBits) != 0)
    {
        return 0;
    }

    return node.cost < 10000 ? 1 : 0;
}

namespace
{
    /// <summary>
    /// The cost of stepping into a cell of <paramref name="cellCost"/> by offset <paramref name="offset"/>: half
    /// again for a diagonal, plus jumpCost (or only jumpCost, with JumpOnBlocked) for a jump.
    /// </summary>
    int32_t StepCost(int32_t cellCost, int32_t offset, int32_t jumpCost)
    {
        if (offset < 8)
        {
            return IsDiagonalStep[offset] != 0 ? cellCost + cellCost / 2 : cellCost;
        }

        if (JumpOnBlocked == 0)
        {
            return cellCost + jumpCost;
        }

        return jumpCost;
    }
}

auto MoveMap::propogateCost(int32_t r, int32_t c, int32_t cost, int32_t g) -> void
{
    Assert(cost > 0 ? 1 : 0, 0, " MoveMap.propogateCost: bad cost ");

    if (g < 0)
    {
        Fatal(0, "Negative g-cost in MoveMap");
    }

    MoveMapNode& current = map[maxCellWidth * r + c];
    const int32_t newG = g + cost;

    if (newG >= current.g)
    {
        return;
    }

    // Original behaviour (OB-028): the cell's own cost is replaced by the step cost (diagonal and jump extras
    // included), so later searches through it see the inflated cost.
    current.cost = cost;
    current.g = newG;
    current.fPrime = current.hPrime + newG;

    if ((current.flags & 1) != 0)
    {
        const int32_t id = CellId(r, c);
        const int32_t itemIndex = openList->find(id);

        if (itemIndex != 0)
        {
            openList->change(itemIndex, current.fPrime);
            return;
        }

        char message[256];
        std::snprintf(message, sizeof(message),
                      "MoveMap.propogateCost: Cannot find movemap node [%d, %d, %d] for change\n", r, c, id);
        DebugOpenList(message);
        return;
    }

    for (int32_t i = 0; i < numOffsets; i++)
    {
        if (IsDiagonalStep[i] != 0 && adjacentCellOpen(r, c, StepAdjDir[i]) == 0 &&
            adjacentCellOpen(r, c, StepAdjDir[i + 1]) == 0)
        {
            continue;
        }

        const int32_t nextR = r + cellShift[i * 2];
        const int32_t nextC = c + cellShift[i * 2 + 1];

        if (nextR < 0 || nextR >= cellHeight || nextC < 0 || nextC >= cellWidth)
        {
            continue;
        }

        MoveMapNode& next = map[maxCellWidth * nextR + nextC];

        if (next.cost >= 10000 || next.hPrime == -1 || next.hPrime >= MaxHPrime)
        {
            continue;
        }

        const int32_t dir = reverseShift[i];
        Assert(next.cost > 0 ? 1 : 0, 0, " MoveMap.propogateCost: bad cost 1");
        const int32_t stepCost = StepCost(next.cost, i, jumpCost);

        if (dir != next.parent)
        {
            if (next.g <= current.g + stepCost)
            {
                continue;
            }

            next.parent = dir;
        }

        propogateCost(nextR, nextC, stepCost, current.g);
    }
}

auto MoveMap::searchPath(MovePath* path, vector_3d* goalWorldPos, int32_t* goalCell, bool escape) -> int32_t
{
    if (escape)
    {
        MaxHPrime = 500;
    }
    else
    {
        if (goalR < 0 || goalR >= cellHeight || goalC < 0 || goalC >= cellWidth)
        {
            const float x = (static_cast<float>(goalC) + 0.5f) * MetersPerCell - worldUnitsMapSide * 0.5f;
            const float y =
                (worldUnitsMapSide * 0.5f - static_cast<float>(goalR) * MetersPerCell) - MetersPerCell * 0.5f;
            char message[256];
            std::snprintf(message, sizeof(message), " Bad Move Goal: %d [%d(%d), %d(%d)], (%.2f, %.2f, %.2f)",
                          DebugMovePathType, goalR, cellHeight, goalC, cellWidth, static_cast<double>(x),
                          static_cast<double>(y), 0.0);
            Fatal(0, message);
        }

        const int32_t distance = std::abs(goalR - startR) + std::abs(goalC - startC);
        MaxHPrime = static_cast<int32_t>(std::floor(static_cast<double>(distance) * 2.5));

        if (MaxHPrime < 500)
        {
            MaxHPrime = 500;
        }
    }

    if (openList == nullptr)
    {
        openList = new PriorityQueue;

        if (openList == nullptr)
        {
            Fatal(0, " Unable to create MoveMap::openList ");
        }

        openList->init(5000, -2000000);
    }

    MoveMapNode& start = map[maxCellWidth * startR + startC];
    start.g = 0;
    const int32_t startH = escape ? 10 : std::abs(goalR - startR) + std::abs(goalC - startC);
    start.hPrime = startH;
    start.fPrime = startH;
    openList->clear();
    PQNode startNode;
    startNode.key = startH;
    startNode.id = CellId(startR, startC);
    startNode.row = startR;
    startNode.col = startC;

    if (openList->insert(startNode) != 0)
    {
        Fatal(0, "PathFind OPEN overflow");
    }

    start.flags |= 1;

    int32_t bestR = -1;
    int32_t bestC = -1;
    int goalFound = 0;

    while (openList->size() != 0)
    {
        PQNode best;
        openList->remove(best);
        bestR = best.row;
        bestC = best.col;
        MoveMapNode& current = map[maxCellWidth * bestR + bestC];
        const int32_t g = current.g;
        const uint32_t flags = current.flags;
        current.flags = (flags & ~1u) | 2;

        if ((flags & 8) != 0)
        {
            goalFound = 1;
            break;
        }

        for (int32_t i = 0; i < numOffsets; i++)
        {
            if (IsDiagonalStep[i] != 0 && adjacentCellOpen(bestR, bestC, StepAdjDir[i]) == 0 &&
                adjacentCellOpen(bestR, bestC, StepAdjDir[i + 1]) == 0)
            {
                continue;
            }

            const int32_t nextR = cellShift[i * 2] + bestR;
            const int32_t nextC = cellShift[i * 2 + 1] + bestC;

            if (nextR < 0 || nextR >= cellHeight || nextC < 0 || nextC >= cellWidth)
            {
                continue;
            }

            MoveMapNode& next = map[maxCellWidth * nextR + nextC];

            if (next.cost >= 10000)
            {
                continue;
            }

            if (next.hPrime == -1)
            {
                next.hPrime = escape ? 10 : std::abs(goalR - nextR) + std::abs(goalC - nextC);
            }

            if (next.hPrime >= MaxHPrime)
            {
                continue;
            }

            const int32_t dir = reverseShift[i];
            const int32_t stepCost = StepCost(next.cost, i, jumpCost);
            Assert(stepCost > 0 ? 1 : 0, 0, " MoveMap.propogateCost: bad cost 3");
            const int32_t newG = stepCost + g;

            if ((next.flags & 1) == 0)
            {
                if ((next.flags & 2) == 0)
                {
                    next.parent = dir;
                    next.g = newG;
                    next.fPrime = newG + next.hPrime;
                    PQNode node;
                    node.key = next.fPrime;
                    node.id = CellId(nextR, nextC);
                    node.row = nextR;
                    node.col = nextC;

                    if (openList->insert(node) != 0)
                    {
                        Fatal(0, "PathFind OPEN overflow");
                    }

                    next.flags |= 1;
                }
                else if (newG < next.g)
                {
                    next.parent = dir;
                    propogateCost(nextR, nextC, stepCost, g);
                }
            }
            else if (newG < next.g)
            {
                next.parent = dir;
                next.g = newG;
                next.fPrime = next.hPrime + newG;
                const int32_t id = CellId(nextR, nextC);
                const int32_t itemIndex = openList->find(id);

                if (itemIndex == 0)
                {
                    // The original passes three values for the four %d (the last prints stack garbage).
                    char message[256];
                    std::snprintf(message, sizeof(message),
                                  escape
                                      ? "MoveMap.calcEscapePath: Cannot find movemap node [%d, %d, %d, %d] for change\n"
                                      : "MoveMap.calcPath: Cannot find movemap node [%d, %d, %d, %d] for change\n",
                                  nextR, nextC, id, 0);
                    DebugOpenList(message);
                }
                else
                {
                    openList->change(itemIndex, next.fPrime);
                }
            }
        }
    }

    if (ClearBridgeTiles != 0)
    {
        AdjustBridgeWeights(overlayWeightTable, 10000);
    }

    if (goalFound == 0)
    {
        return 0;
    }

    goalCell[0] = bestR;
    goalCell[1] = bestC;
    int32_t count = 0;

    for (int32_t r = bestR, c = bestC; r != startR || c != startC;)
    {
        count++;
        const int32_t parent = map[maxCellWidth * r + c].parent;
        r += cellShift[parent * 2];
        c += cellShift[parent * 2 + 1];
    }

    if (doorDirection != -1)
    {
        count++;
    }

    path->goal.zero();
    path->numSteps = 0;
    path->numStepsWhenNotPaused = 0;
    path->curStep = 0;
    path->cost = 0;
    path->marked = 0;
    path->globalStep = -1;

    if (count == 0)
    {
        return path->numSteps;
    }

    // Port fix: the original writes past stepList when the path is longer than MAX_STEPS_PER_MOVEPATH.
    if (count > MAX_STEPS_PER_MOVEPATH)
    {
        return path->numSteps;
    }

    path->init(count);
    path->target = target;
    path->cost = map[maxCellWidth * bestR + bestC].g;
    int32_t stepIndex = count;

    if (doorDirection == -1)
    {
        if (goalWorldPos == nullptr)
        {
            path->goal = goalPos;
        }
        else
        {
            goalWorldPos->z = 0.0f;
            path->goal.z = 0.0f;
            const float x = (static_cast<float>(minCol + bestC) + 0.5f) * MetersPerCell - worldUnitsMapSide * 0.5f;
            const float y =
                (worldUnitsMapSide * 0.5f - static_cast<float>(minRow + bestR) * MetersPerCell) - MetersPerCell * 0.5f;
            goalWorldPos->x = x;
            goalWorldPos->y = y;
            path->goal.x = x;
            path->goal.y = y;
        }
    }
    else
    {
        // The last step goes through the door, into the next area.
        stepIndex = count - 1;
        PathStep& doorStep = path->stepList[stepIndex];
        doorStep.direction = static_cast<uint8_t>(static_cast<char>(doorDirection) << 1);
        const int32_t doorRow = adjTile[doorDirection][0] + minRow + bestR;
        const int32_t doorCol = minCol + adjTile[doorDirection][1] + bestC;
        goalCell[0] = doorRow;
        goalCell[1] = doorCol;
        const float x = (static_cast<float>(doorCol) + 0.5f) * MetersPerCell - worldUnitsMapSide * 0.5f;
        const float y = (worldUnitsMapSide * 0.5f - static_cast<float>(doorRow) * MetersPerCell) - MetersPerCell * 0.5f;
        path->setDestination(stepIndex, vector_3d(x, y, 0.0f));
        doorStep.distanceToGoal = 0.0f;
        doorStep.tileR = static_cast<int16_t>(doorRow / 3);
        doorStep.tileC = static_cast<int16_t>(doorCol / 3);
        doorStep.cellR = static_cast<int16_t>(doorRow - doorStep.tileR * 3);
        doorStep.cellC = static_cast<int16_t>(doorCol - doorStep.tileC * 3);
        path->goal = vector_3d(x, y, 0.0f);

        if (goalWorldPos != nullptr)
        {
            *goalWorldPos = vector_3d(x, y, 0.0f);
        }
    }

    int& ready = cellShiftDistanceReady[escape ? 1 : 0];

    if (ready == 0)
    {
        const float scale = metersPerWorldUnit * Terrain::metersPerVertexDivMAPCELL_DIM;

        for (int32_t i = 0; i < NUM_CELL_OFFSETS; i++)
        {
            const float dr = static_cast<float>(cellShift[i * 2]);
            const float dc = static_cast<float>(cellShift[i * 2 + 1]);
            cellShiftDistance[i] = std::sqrt(dc * dc + dr * dr) * scale;
        }

        ready = 1;
    }

    // Walks back from the goal cell, filling the steps from the end.
    for (int32_t r = bestR, c = bestC; r != startR || c != startC;)
    {
        stepIndex--;
        MoveMapNode& node = map[maxCellWidth * r + c];
        PathStep& step = path->stepList[stepIndex];
        step.direction = static_cast<uint8_t>(reverseShift[node.parent]);
        const int32_t mapRow = minRow + r;
        const int32_t mapCol = minCol + c;
        const float x = (static_cast<float>(mapCol) + 0.5f) * MetersPerCell;
        const float y = (worldUnitsMapSide * 0.5f - static_cast<float>(mapRow) * MetersPerCell) - MetersPerCell * 0.5f;

        if (stepIndex == count - 1 && static_cast<int8_t>(step.direction) < 8)
        {
            step.distanceToGoal = 0.0f;
        }
        else
        {
            // As the original: for a final jump step this reads the (unused) step past the end.
            const PathStep& nextStep = path->stepList[stepIndex + 1];
            step.distanceToGoal = nextStep.distanceToGoal + cellShiftDistance[static_cast<int8_t>(nextStep.direction)];
        }

        path->setDestination(stepIndex, vector_3d(x - worldUnitsMapSide * 0.5f, y, 0.0f));
        step.tileR = static_cast<int16_t>(mapRow / 3);
        step.tileC = static_cast<int16_t>(mapCol / 3);
        step.cellR = static_cast<int16_t>(mapRow - step.tileR * 3);
        step.cellC = static_cast<int16_t>(mapCol - step.tileC * 3);
        node.flags |= 4;
        r += cellShift[node.parent * 2];
        c += cellShift[node.parent * 2 + 1];
    }

    return path->numSteps;
}

auto MoveMap::calcPath(MovePath* path, vector_3d* goalWorldPos, int32_t* goalCell) -> int32_t
{
    return searchPath(path, goalWorldPos, goalCell, false);
}

auto MoveMap::calcEscapePath(MovePath* path, vector_3d* goalWorldPos, int32_t* goalCell) -> int32_t
{
    return searchPath(path, goalWorldPos, goalCell, true);
}

auto MoveMap::writeDebug(File* debugFile) -> void
{
    char line[512];
    std::snprintf(line, sizeof(line), "Time = %.6f\n\n", static_cast<double>(calcTime));
    debugFile->writeString(line);
    std::snprintf(line, sizeof(line), "Start = (%d, %d)\n", startR, startC);
    debugFile->writeString(line);
    std::snprintf(line, sizeof(line), "Goal = (%d, %d)\n", goalR, goalC);
    debugFile->writeString(line);
    std::strcpy(line, "\n");
    debugFile->writeString(line);

    char cell[16] = {};
    debugFile->writeString("PARENT:\n");
    debugFile->writeString("-------\n");

    for (int32_t r = 0; r < cellHeight; r++)
    {
        line[0] = '\0';

        for (int32_t c = 0; c < cellWidth; c++)
        {
            const MoveMapNode& node = map[maxCellWidth * r + c];

            if (startR == r && startC == c)
            {
                std::strcpy(cell, "S");
            }
            else if (node.parent == -1)
            {
                std::strcpy(cell, ".");
            }
            else if ((node.flags & 4) == 0)
            {
                std::snprintf(cell, sizeof(cell), "%d", node.parent);
            }
            else
            {
                std::strcpy(cell, "X");
            }

            std::strcat(line, cell);
        }

        std::strcat(line, "\n");
        debugFile->writeString(line);
    }

    debugFile->writeString("\n");
    debugFile->writeString("MAP:\n");
    debugFile->writeString("-------\n");

    for (int32_t r = 0; r < cellHeight; r++)
    {
        line[0] = '\0';

        for (int32_t c = 0; c < cellWidth; c++)
        {
            const int32_t cost = map[maxCellWidth * r + c].cost;

            if (goalR == r && goalC == c)
            {
                std::strcpy(cell, "G");
            }
            else if (startR == r && startC == c)
            {
                std::strcpy(cell, "S");
            }
            else if (cost == moveLevel)
            {
                std::strcpy(cell, ".");
            }
            else if (cost >= 10000)
            {
                std::strcpy(cell, " ");
            }
            else if (cost < 0x100)
            {
                std::strcpy(cell, "o");
            }

            // As the original: other costs repeat the previous cell's text.
            std::strcat(line, cell);
        }

        std::strcat(line, "\n");
        debugFile->writeString(line);
    }

    debugFile->writeString("\n");
    debugFile->writeString("PATH:\n");
    debugFile->writeString("-------\n");

    for (int32_t r = 0; r < cellHeight; r++)
    {
        line[0] = '\0';

        for (int32_t c = 0; c < cellWidth; c++)
        {
            const MoveMapNode& node = map[maxCellWidth * r + c];

            if (goalR == r && goalC == c)
            {
                std::strcpy(cell, "G");
            }
            else if (startR == r && startC == c)
            {
                std::strcpy(cell, "S");
            }
            else if ((node.flags & 4) != 0)
            {
                std::strcpy(cell, "*");
            }
            else if (node.cost == moveLevel)
            {
                std::strcpy(cell, ".");
            }
            else if (node.cost >= 10000)
            {
                std::strcpy(cell, " ");
            }
            else
            {
                std::strcpy(cell, "o");
            }

            std::strcat(line, cell);
        }

        std::strcat(line, "\n");
        debugFile->writeString(line);
    }

    debugFile->writeString("\n");
}

auto MoveMap::destroy() -> void
{
    if (map != nullptr)
    {
        systemHeap->free(map);
    }
}
