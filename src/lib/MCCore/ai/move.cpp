#include "stdafx.h"
#include "ai/move.h"
#include "lib/MCFatal.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "lib/MCPacketFile.h"
#include "lib/MCPriorityQueue.h"
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
#include "terrain/MCTerrain.h"

int32_t MCGlobalMap::MinTileR = 0;
int32_t MCGlobalMap::MaxTileR = 0;
int32_t MCGlobalMap::MinTileC = 0;
int32_t MCGlobalMap::MaxTileC = 0;

int BlockWallTiles = 1;
int32_t SimpleMovePathRange = 7;
char RowShift[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
char ColShift[8] = {0, 1, 1, 1, 0, -1, -1, -1};
int32_t CellShift[NUM_CELL_OFFSETS * 2] = {
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

char ReverseShift[NUM_CELL_OFFSETS] = {
    4,  5,  6,  7,  0,  1,  2,  3,  12, 13, 14, 15, 8,  9,  10, 11, 20, 21, 22,  23,  16,  17,  18, 19, 28, 29,
    30, 31, 24, 25, 26, 27, 36, 37, 38, 39, 32, 33, 34, 35, 44, 45, 46, 47, 40,  41,  42,  43,  52, 53, 54, 55,
    48, 49, 50, 51, 60, 61, 62, 63, 56, 57, 58, 59, 68, 69, 70, 71, 64, 65, 66,  67,  76,  77,  78, 79, 72, 73,
    74, 75, 84, 85, 86, 87, 80, 81, 82, 83, 92, 93, 94, 95, 88, 89, 90, 91, 100, 101, 102, 103, 96, 97, 98, 99,
};

int IsDiagonalStep[NUM_CELL_OFFSETS] = {0, 1, 0, 1, 0, 1, 0, 1};
int32_t StepAdjDir[9] = {-1, 0, 2, 2, 4, 4, 6, 6, 0};
int32_t AdjTile[4][2] = {{-1, 0}, {0, 1}, {1, 0}, {0, -1}};
char MineLayout[4][9] = {{}, {}, {1, 0, 1, 0, 1, 0, 1, 0, 1}, {}};
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

int32_t AdjCellTable[MAPCELL_DIM * MAPCELL_DIM][8][4] = {
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

float CellColToWorldCoord[MAX_MAP_CELL_WIDTH] = {};
float CellToWorldCoord[MAPCELL_DIM] = {};
float TileColWorldCoords[MAX_MAP_TILE_WIDTH] = {};
float CellShiftDistance[NUM_CELL_OFFSETS] = {};
float TileRowWorldCoords[MAX_MAP_TILE_WIDTH] = {};
int32_t OverlayWeightIndex[NUM_OVERLAY_TYPES] = {};
float CellRowToWorldCoord[MAX_MAP_CELL_WIDTH] = {};
int32_t OverlayWeightTable[NUM_MOVE_LEVELS * OVERLAY_WEIGHT_LEVEL_SIZE] = {};
int32_t TileMulMapcellDim[MAX_MAP_TILE_WIDTH] = {};
int32_t MoveChunkUnpackErr = 0;
int ClearBridgeTiles = 0;
MCGameObject* MovingObject = nullptr;
MCGameObject* RamObject = nullptr;
MCPriorityQueue* OpenList = nullptr;
int JumpOnBlocked = 0;
int FindingEscapePath = 0;
MCScenarioMap* GameMap = nullptr;
MCGlobalMap* GlobalMoveMap = nullptr;
MCMoveMap* PathFindMap = nullptr;
MCObjectMap* GameObjectMap = nullptr;
MCMovePathManager* PathManager = nullptr;
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
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;

    /// <summary>
    /// Per gate overlay (67..74) and team alignment + 1, the overlay the gate behaves as for that team, or -1 when
    /// it is closed to it (cost 20000).
    /// </summary>
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

    /// <summary>The largest step count <see cref="MCMovePath::Init"/> has seen.</summary>
    int32_t MaxMovePathSteps = 0;

    /// <summary>Whether a tile's cell (cellR, cellC) is passable (<see cref="MCMapTile::GetCellPassable"/> on a copy).</summary>
    uint32_t TileCellPassable(const MCMapTile& tile, int32_t cellR, int32_t cellC)
    {
        const uint32_t shift = static_cast<uint32_t>((cellR * MAPCELL_DIM + cellC) * 2);
        return (tile.Cells & (0x4000u << shift)) >> (shift + 14);
    }

    /// <summary>
    /// <see cref="TileColWorldCoords"/>[tileC]. Port fix: the original reads past the table for a column off the
    /// map; the port computes such a column's edge the way the table was filled.
    /// </summary>
    float TileColToWorldCoord(int32_t tileC, int32_t mapWidth)
    {
        if (tileC >= 0 && tileC < mapWidth)
        {
            return TileColWorldCoords[tileC];
        }

        return static_cast<float>(static_cast<double>(tileC) * MCTerrain::MetersPerVertex -
                                  static_cast<double>(WorldUnitsMapSide) * 0.5);
    }

    /// <summary>
    /// <see cref="TileRowWorldCoords"/>[tileR]. Port fix: the original reads past the table for a row off the map;
    /// the port computes such a row's edge the way the table was filled.
    /// </summary>
    float TileRowToWorldCoord(int32_t tileR, int32_t mapHeight)
    {
        if (tileR >= 0 && tileR < mapHeight)
        {
            return TileRowWorldCoords[tileR];
        }

        return static_cast<float>(static_cast<double>(WorldUnitsMapSide) * 0.5 -
                                  static_cast<double>(tileR) * MCTerrain::MetersPerVertex);
    }
}

auto WorldCoordToMapCoord(MCVector3D pos, int32_t& tileR, int32_t& tileC, int32_t& cellR, int32_t& cellC) -> void
{
    tileC =
        static_cast<int32_t>(static_cast<double>(MCTerrain::OneOvermetersPerVertex) * pos.X + VerticesMapSideDivTwo);
    tileR =
        static_cast<int32_t>((static_cast<double>(MetersMapSideDivTwo) - pos.Y) * MCTerrain::OneOvermetersPerVertex);
    cellC =
        static_cast<int32_t>((static_cast<double>(pos.X) - TileColToWorldCoord(tileC, GameMap->Width)) / MetersPerCell);
    cellR = static_cast<int32_t>((static_cast<double>(TileRowToWorldCoord(tileR, GameMap->Height)) - pos.Y) /
                                 MetersPerCell);
}

auto WorldCoordToMapTile(MCVector3D pos, int32_t& tileR, int32_t& tileC) -> void
{
    tileC =
        static_cast<int32_t>(static_cast<double>(MCTerrain::OneOvermetersPerVertex) * pos.X + VerticesMapSideDivTwo);
    tileR =
        static_cast<int32_t>((static_cast<double>(MetersMapSideDivTwo) - pos.Y) * MCTerrain::OneOvermetersPerVertex);
}

auto WorldCoordToMapCell(MCVector3D pos, int32_t& cellR, int32_t& cellC) -> void
{
    cellC = static_cast<int32_t>((static_cast<double>(MetersMapSideDivTwo) + pos.X) /
                                 MCTerrain::MetersPerVertexDivMapcellDim);
    cellR = static_cast<int32_t>((static_cast<double>(MetersMapSideDivTwo) - pos.Y) /
                                 MCTerrain::MetersPerVertexDivMapcellDim);
}

auto RelativePositionToPoint(MCVector3D pos, float angle, float distance, uint32_t flags) -> MCVector3D
{
    const int reverse = (flags & 2) != 0;
    const double radians = angle * DEGREES_TO_RADIANS;
    const float reach = -(WorldUnitsPerMeter * distance);
    const float pointX = (static_cast<float>(std::sin(radians)) + 0.0f) * reach + pos.X;
    const float pointY = static_cast<float>(std::cos(radians) * reach) + pos.Y;

    // Walk from start toward end: from the point back toward pos, or (reverse) from pos out to the point.
    MCVector2D start;
    MCVector2D end;

    if (reverse)
    {
        start = MCVector2D(pos.X, pos.Y);
        end = MCVector2D(pointX, pointY);
    }
    else
    {
        start = MCVector2D(pointX, pointY);
        end = MCVector2D(pos.X, pos.Y);
    }

    float stepX = end.X - start.X;
    float stepY = end.Y - start.Y;
    const float length = std::sqrt(stepX * stepX + stepY * stepY);

    if (length != 0.0f)
    {
        stepX = stepX / length;
        stepY = stepY / length;
    }

    const float stepLength = MCTerrain::MetersPerVertex * (1.0f / 3.0f) * 0.5f;
    stepX = stepX * stepLength;
    stepY = stepY * stepLength;

    if (std::sqrt(stepX * stepX + stepY * stepY) == 0.0f)
    {
        return MCVector3D(pos.X, pos.Y, 0.0f);
    }

    const MCVector2D span = end - start;
    const float totalDistance = std::sqrt(span.Y * span.Y + span.X * span.X);
    MCVector2D current = start;
    MCVector2D result = start;
    float travelled = 0.0f;
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    // Port fix: the point can be off the map, where the original reads outside it. Off the map is impassable.
    auto passableAt = [&]() -> uint32_t
    {
        GameMap->WorldToMapPos(MCVector3D(current.X, current.Y, 0.0f), tileR, tileC, cellR, cellC);

        if (!GameMap->OnMap(tileR, tileC))
        {
            return 0;
        }

        return GameMap->Map[GameMap->Width * tileR + tileC].GetCellPassable(cellR, cellC);
    };

    uint32_t passable = passableAt();

    // Original behaviour (OB-032): the result trails the walk by a step, the last point before the one that ended
    // it (so walking in from an impassable point, the result is still impassable).
    while ((reverse ? passable != 0 : passable == 0) && travelled < totalDistance)
    {
        result = current;
        current.X = stepX + current.X;
        current.Y = stepY + current.Y;
        travelled =
            std::sqrt((current.X - start.X) * (current.X - start.X) + (current.Y - start.Y) * (current.Y - start.Y));
        passable = passableAt();
    }

    const float limit = WorldUnitsMapSide * 0.5f - MCTerrain::MetersPerVertex;

    if (result.X < -limit)
    {
        result.X = -limit;
    }

    if (result.X > limit)
    {
        result.X = limit;
    }

    if (result.Y < -limit)
    {
        result.Y = -limit;
    }

    if (result.Y > limit)
    {
        result.Y = limit;
    }

    const float elevation = GameMap->GetTerrainElevation(MCVector3D(result.X, result.Y, 0.0f));
    return MCVector3D(result.X, result.Y, elevation);
}

auto MapTileCellToWorldPos(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, MCVector3D& worldPos) -> void
{
    worldPos.Z = 0.0f;
    worldPos.X =
        static_cast<float>(static_cast<double>(TileColWorldCoords[tileC]) + CellToWorldCoord[cellC] + HalfMapCell);
    worldPos.Y =
        static_cast<float>(static_cast<double>(TileRowWorldCoords[tileR]) - CellToWorldCoord[cellR] - HalfMapCell);
}

auto MapCellToWorldPos(int32_t cellR, int32_t cellC, MCVector3D& worldPos) -> void
{
    worldPos.Z = 0.0f;
    worldPos.X = HalfMapCell + CellColToWorldCoord[cellC];
    worldPos.Y = CellRowToWorldCoord[cellR] - HalfMapCell;
}

auto DebugOpenList(char* msg) -> void
{
    MCFile* debugFile = new MCFile;
    debugFile->Create("openlist.dbg");
    debugFile->WriteString(msg);
    char line[512];

    if (MovingObject != nullptr)
    {
        std::snprintf(line, sizeof(line), "MovingObject = %s [%d]\n",
                      static_cast<MCMover*>(MovingObject)->DebugStatus.c_str(), MovingObject->PartId);
        debugFile->WriteString(line);

        if (MovingObject->ObjectClass == ELEMENTAL)
        {
            debugFile->WriteString("Is an elemental!\n");
        }
    }

    debugFile->WriteString("\nOPENLIST INFO\n");
    std::snprintf(line, sizeof(line), "NumItems = %d\n", OpenList->Size());
    debugFile->WriteString(line);

    for (int32_t i = 0; i < OpenList->Size(); i++)
    {
        // As the original: items are read from pqList[0] (the sentinel) up, so the last item is left out.
        const MCPQNode& item = OpenList->GetItem(i);
        std::snprintf(line, sizeof(line), "Item: %04d\n", i);
        debugFile->WriteString(line);
        std::snprintf(line, sizeof(line), "     key: %d\n", item.Key);
        debugFile->WriteString(line);
        std::snprintf(line, sizeof(line), "      id: %d\n", item.Id);
        debugFile->WriteString(line);
        std::snprintf(line, sizeof(line), "     row: %d\n", item.Row);
        debugFile->WriteString(line);
        std::snprintf(line, sizeof(line), "     col: %d\n", item.Col);
        debugFile->WriteString(line);
    }

    debugFile->Close();
    delete debugFile;
}

auto CalcTileTypeFromIndex(int32_t tileIndex) -> int32_t
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
        int32_t Limit = 0;
        int32_t Type = 0;
    } bands[] = {
        {0xc6e, 0x29}, {0xc84, 0x2a}, {0xc88, 0x2b}, {0xc96, 0x2c}, {0xca4, 0x2d}, {0xcb0, 0x2e},
        {0xcbc, 0x2f}, {0xcc8, 0x30}, {0xcd4, 0x31}, {0xce0, 0x32}, {0xcec, 0x33}, {0xcf4, 0x34},
        {0xcfc, 0x35}, {0xd15, 0x3a}, {0xd22, 0x36}, {0xd30, 0x38}, {0xd3e, 0x37},
    };

    for (const auto& band : bands)
    {
        if (tileIndex < band.Limit)
        {
            return band.Type;
        }
    }

    return tileIndex > 0xd65 ? 0 : 2;
}

auto CalcOverlayTypeFromIndex(int32_t overlayIndex) -> int32_t
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

auto MCScenarioMap::Init(int32_t newWidth, int32_t newHeight) -> void
{
    MetersPerCell = MCTerrain::MetersPerVertexDivMapcellDim;

    for (int32_t i = 0; i < MAX_MAP_TILE_WIDTH; i++)
    {
        TileMulMapcellDim[i] = i * MAPCELL_DIM;
    }

    const double mapHalf = static_cast<double>(WorldUnitsMapSide) * 0.5;

    for (int32_t row = 0; row < newHeight; row++)
    {
        TileRowWorldCoords[row] = static_cast<float>(mapHalf - static_cast<double>(row) * MCTerrain::MetersPerVertex);
    }

    for (int32_t col = 0; col < newWidth; col++)
    {
        TileColWorldCoords[col] = static_cast<float>(static_cast<double>(col) * MCTerrain::MetersPerVertex - mapHalf);
    }

    const double cellSide = static_cast<double>(MCTerrain::MetersPerVertex) * (1.0f / 3.0f);

    for (int32_t cell = 0; cell < MAPCELL_DIM; cell++)
    {
        CellToWorldCoord[cell] = static_cast<float>(static_cast<double>(cell) * cellSide);
    }

    Width = newWidth;
    Height = newHeight;
    VerticesMapSideDivTwo = static_cast<float>(MCTerrain::VerticesBlockSide * MCTerrain::BlocksMapSide) * 0.5f;
    MetersMapSideDivTwo = WorldUnitsMapSide * 0.5f;
    MapCellDiagonal = static_cast<float>(cellSide * MetersPerWorldUnit * 1.4142);
    HalfMapCell = static_cast<float>(cellSide * 0.5);

    const size_t numTiles = static_cast<size_t>(newWidth * newHeight);
    Map = std::make_unique<MCMapTile[]>(numTiles);
    PathMap = std::make_unique<uint8_t[]>(numTiles);
}

auto MCScenarioMap::Init(MCFile* mapFile) -> int32_t
{
    MetersPerCell = MCTerrain::MetersPerVertexDivMapcellDim;
    const double cellSide = static_cast<double>(MCTerrain::MetersPerVertex) * (1.0f / 3.0f);
    MapCellDiagonal = static_cast<float>(cellSide * MetersPerWorldUnit * 1.4142);
    HalfMapCell = static_cast<float>(cellSide * 0.5);
    VerticesMapSideDivTwo = static_cast<float>((MCTerrain::VerticesBlockSide * MCTerrain::BlocksMapSide) / 2);
    MetersMapSideDivTwo = WorldUnitsMapSide * 0.5f;

    Height = mapFile->ReadLong();
    Width = mapFile->ReadLong();

    for (int32_t i = 0; i < MAX_MAP_TILE_WIDTH; i++)
    {
        TileMulMapcellDim[i] = i * MAPCELL_DIM;
    }

    const double mapHalf = static_cast<double>(WorldUnitsMapSide) * 0.5;

    for (int32_t row = 0; row < Height; row++)
    {
        TileRowWorldCoords[row] = static_cast<float>(mapHalf - static_cast<double>(row) * MCTerrain::MetersPerVertex);
    }

    for (int32_t col = 0; col < Width; col++)
    {
        TileColWorldCoords[col] = static_cast<float>(static_cast<double>(col) * MCTerrain::MetersPerVertex - mapHalf);
    }

    for (int32_t cell = 0; cell < MAPCELL_DIM; cell++)
    {
        CellToWorldCoord[cell] = static_cast<float>(static_cast<double>(cell) * cellSide);
    }

    for (int32_t row = 0; row < Height * MAPCELL_DIM; row++)
    {
        CellRowToWorldCoord[row] = static_cast<float>(mapHalf - static_cast<double>(row) * MetersPerCell);
    }

    for (int32_t col = 0; col < Width * MAPCELL_DIM; col++)
    {
        CellColToWorldCoord[col] = static_cast<float>(static_cast<double>(col) * MetersPerCell - mapHalf);
    }

    BaseElevation = mapFile->ReadLong();
    const size_t numTiles = static_cast<size_t>(Width * Height);
    Map = std::make_unique<MCMapTile[]>(numTiles);
    mapFile->Read(reinterpret_cast<uint8_t*>(Map.get()), static_cast<int32_t>(numTiles * sizeof(MCMapTile)));
    PathMap = std::make_unique<uint8_t[]>(numTiles);
    return 0;
}

auto MCScenarioMap::Init(MCScenario*) -> int32_t
{
    return 0;
}

auto MCScenarioMap::Write(MCFile* mapFile) -> int32_t
{
    mapFile->WriteLong(Height);
    mapFile->WriteLong(Width);
    mapFile->WriteLong(BaseElevation);
    mapFile->Write(reinterpret_cast<const uint8_t*>(Map.get()),
                   static_cast<int32_t>(Width * Height * sizeof(MCMapTile)));
    return 0;
}

auto MCScenarioMap::Destroy() -> void
{
    Map.reset();
    PathMap.reset();
}

auto MCScenarioMap::WorldToMapPos(MCVector3D pos, int32_t& tileR, int32_t& tileC, int32_t& cellR, int32_t& cellC)
    -> void
{
    tileC = static_cast<int16_t>(static_cast<int32_t>(
        std::floor(static_cast<double>(MCTerrain::OneOvermetersPerVertex) * pos.X + VerticesMapSideDivTwo)));
    tileR = static_cast<int16_t>(static_cast<int32_t>(
        std::floor((static_cast<double>(MetersMapSideDivTwo) - pos.Y) * MCTerrain::OneOvermetersPerVertex)));
    cellC = static_cast<int32_t>((static_cast<double>(pos.X) - TileColToWorldCoord(tileC, Width)) / MetersPerCell);
    cellR = static_cast<int32_t>((static_cast<double>(TileRowToWorldCoord(tileR, Height)) - pos.Y) / MetersPerCell);
}

auto MCScenarioMap::WorldToMapTilePos(MCVector3D pos, int32_t& tileR, int32_t& tileC) -> void
{
    tileC = static_cast<int16_t>(static_cast<int32_t>(
        std::floor(static_cast<double>(MCTerrain::OneOvermetersPerVertex) * pos.X + VerticesMapSideDivTwo)));
    tileR = static_cast<int16_t>(static_cast<int32_t>(
        std::floor((static_cast<double>(MetersMapSideDivTwo) - pos.Y) * MCTerrain::OneOvermetersPerVertex)));
}

auto MCScenarioMap::CellPassable(MCVector3D pos) -> int
{
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    WorldToMapPos(pos, tileR, tileC, cellR, cellC);

    // Port fix: the original reads outside map for a point off it (the mouse past the terrain edge). Off the map is
    // impassable.
    if (!OnMap(tileR, tileC))
    {
        return 0;
    }

    return static_cast<int>(Map[Width * tileR + tileC].GetCellPassable(cellR, cellC));
}

auto MCScenarioMap::SpreadState(int32_t cellRow, int32_t cellCol, int32_t depth) -> void
{
    if (cellRow < 0 || cellRow >= Height * MAPCELL_DIM || cellCol < 0 || cellCol >= Width * MAPCELL_DIM || depth <= 0)
    {
        return;
    }

    const int32_t tileR = cellRow / MAPCELL_DIM;
    const int32_t tileC = cellCol / MAPCELL_DIM;
    MCMapTile& tile = Map[Width * tileR + tileC];

    if (PreserveMapTiles != 0 && (tile.Cells & 0x2000) == 0)
    {
        // Port fix: the original writes past preservedTiles[] once MAX_PRESERVED_TILES tiles are saved.
        if (NumPreservedTiles < MAX_PRESERVED_TILES)
        {
            PreservedTiles[NumPreservedTiles].Cells = tile.Cells;
            PreservedTiles[NumPreservedTiles].Row = static_cast<int16_t>(tileR);
            PreservedTiles[NumPreservedTiles].Col = static_cast<int16_t>(tileC);
            tile.Cells |= 0x2000;
            NumPreservedTiles++;
        }
    }

    const uint32_t shift =
        static_cast<uint32_t>(((cellRow - tileR * MAPCELL_DIM) * MAPCELL_DIM + (cellCol - tileC * MAPCELL_DIM)) * 2);
    tile.Cells &= ~(0x4000u << shift);

    for (int32_t dir = 0; dir < 8; dir++)
    {
        SpreadState(cellRow + RowShift[dir], cellCol + ColShift[dir], depth - 1);
    }
}

auto MCScenarioMap::PlaceObject(MCVector3D position, float radius) -> int32_t
{
    int32_t cellR = 0;
    int32_t cellC = 0;
    WorldCoordToMapCell(position, cellR, cellC);
    double depth = static_cast<double>(radius) /
                   (static_cast<double>(MetersPerWorldUnit) * MCTerrain::MetersPerVertexDivMapcellDim);

    if (depth > 0.5 && depth < 1.0)
    {
        depth = 1.0;
    }

    SpreadState(cellR, cellC, static_cast<int32_t>(depth));
    return 0;
}

auto MCScenarioMap::PlaceObjects(MCObjectQueueNode* objectList) -> int32_t
{
    if (objectList->Head == nullptr)
    {
        return 0;
    }

    MCBaseObject* current = nullptr;

    while (objectList->Traverse(current) != nullptr)
    {
        MCGameObject* object = static_cast<MCGameObject*>(current);

        if (object->GetUseMe() != 0 && object->GetObjectType() != nullptr)
        {
            PlaceObject(object->GetPosition(), object->GetObjectType()->ExtentRadius);
        }
    }

    return 0;
}

auto MCScenarioMap::PlaceTerrainObject(MCGameObject*) -> void
{
}

auto MCScenarioMap::PlaceTerrainObjects(MCObjectBlockManager* blockManager) -> void
{
    MCPacketFile* objectFile = blockManager->ObjectFile;
    const int32_t numTiles = Width * Height;
    // Original behaviour (OB-033): never written (placeTerrainObject does nothing), so the tiles' overlay bits 7-8
    // all end up cleared.
    std::vector<uint8_t> footprint(static_cast<size_t>(numTiles));

    for (int32_t block = 0; block < MCTerrain::BlocksMapSide * MCTerrain::BlocksMapSide; block++)
    {
        if (objectFile == nullptr || objectFile->IsOpen() == 0)
        {
            continue;
        }

        objectFile->SeekPacket(block);
        const uint32_t packetSize = static_cast<uint32_t>(objectFile->GetPacketSize());

        if (packetSize == 0)
        {
            continue;
        }

        std::vector<uint8_t> data(packetSize, 0xff);
        objectFile->ReadPacket(block, data.data());
        const uint32_t numRecords = packetSize / sizeof(MCObjData);

        for (uint32_t i = 0; i < numRecords; i++)
        {
            MCObjData record;
            std::memcpy(&record, data.data() + i * sizeof(MCObjData), sizeof(MCObjData));

            if (record.ObjTypeNum == -1)
            {
                continue;
            }

            MCGameObject* object = CreateObject(record.ObjTypeNum);
            MCVector2D position(static_cast<float>(record.PixelOffsetX), static_cast<float>(record.PixelOffsetY));
            MCVector2D numbers(static_cast<float>(static_cast<uint16_t>(record.VertexNumber)),
                               static_cast<float>(static_cast<uint16_t>(record.BlockNumber)));
            object->SetTerrainPosition(position, numbers);
            object->Update();
            PlaceTerrainObject(object);
            delete object;
        }
    }

    for (int32_t row = 0; row < Height; row++)
    {
        for (int32_t col = 0; col < Width; col++)
        {
            MCMapTile& tile = Map[row * Width + col];
            tile.Overlay =
                (static_cast<uint32_t>(footprint[row * Width + col] & 0xfe) << 6) | (tile.Overlay & 0xfffffe7f);
        }
    }
}

auto MCScenarioMap::UpdateMovingObjects() -> void
{
    PreserveMapTiles = 1;
    PlaceObjects(ClanMechList);
    PlaceObjects(InnerSphereMechList);
    PreserveMapTiles = 0;
}

auto MCScenarioMap::RestorePreservedMap() -> void
{
    for (int32_t i = 0; i < NumPreservedTiles; i++)
    {
        Map[PreservedTiles[i].Row * Width + PreservedTiles[i].Col].Cells = PreservedTiles[i].Cells;
    }

    NumPreservedTiles = 0;
}

auto MCScenarioMap::GetTerrainElevation(MCVector3D position) -> float
{
    return static_cast<float>(GetTerrainElevationUnrounded(position));
}

auto MCScenarioMap::GetTerrainElevationUnrounded(MCVector3D position) -> double
{
    const float mpv = MCTerrain::MetersPerVertex;
    const float oneOver = MCTerrain::OneOvermetersPerVertex;
    const float vertexX = static_cast<float>(mpv * std::floor(static_cast<double>(oneOver) * position.X));
    const float vertexY = static_cast<float>(mpv * (std::floor(static_cast<double>(oneOver) * position.Y) + 1.0));
    const double vertexCol = static_cast<double>(oneOver) * vertexX;
    const float vertexRow = oneOver * vertexY;
    const int32_t halfSide = (MCTerrain::BlocksMapSide * MCTerrain::VerticesBlockSide) >> 1;
    int32_t tileC = static_cast<int32_t>(std::floor(vertexCol)) + halfSide;
    int32_t tileR = halfSide - static_cast<int32_t>(std::floor(static_cast<double>(vertexRow)));
    const int32_t maxTile = MCTerrain::BlocksMapSide * MCTerrain::VerticesBlockSide - 2;

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
    { return r >= 0 && r < GameMap->Height && c >= 0 && c < GameMap->Width ? 1 : 0; };
    Assert(inMap(tileR, tileC), 0, " move:terrelev MapTile Out of Bounds ");
    Assert(inMap(tileR + 1, tileC + 1), 0, " move:terrelev2 MapTile Out of Bounds ");
    Assert(inMap(tileR, tileC), 0, " Map Tile out of bounds ");
    const uint32_t cells00 = GameMap->Map[GameMap->Width * tileR + tileC].Cells;
    Assert(inMap(tileR, tileC + 1), 0, " Map Tile out of bounds ");
    const uint32_t cells01 = GameMap->Map[GameMap->Width * tileR + tileC + 1].Cells;
    Assert(inMap(tileR + 1, tileC + 1), 0, " Map Tile out of bounds ");
    const uint32_t cells11 = GameMap->Map[GameMap->Width * (tileR + 1) + tileC + 1].Cells;
    Assert(inMap(tileR + 1, tileC), 0, " Map Tile out of bounds ");
    const uint32_t cells10 = GameMap->Map[GameMap->Width * (tileR + 1) + tileC].Cells;

    const int32_t base = GameMap->BaseElevation;
    const float mpe = MCTerrain::MetersPerElevLevel;
    const auto levelOf = [base](uint32_t cells) -> double
    {
        return static_cast<double>(
            static_cast<int64_t>(static_cast<uint32_t>(static_cast<int32_t>((cells >> 7) & 0x3f) + base)));
    };

    const float cornerX = static_cast<float>(std::floor(vertexCol) * mpv);
    const float cornerY = static_cast<float>(std::floor(static_cast<double>(vertexRow)) * mpv);
    const float elevation00 = static_cast<float>(levelOf(cells00) * mpe);
    const double offsetX = std::fabs(static_cast<double>(position.X) - vertexX);
    const float dx = static_cast<float>(offsetX);
    const float dy = static_cast<float>(std::fabs(static_cast<double>(vertexY) - position.Y));
    const double cornerXPlus = static_cast<double>(cornerX) + mpv;

    // The two edges of the tile's triangle holding the point, from its upper-left corner.
    double edge1X;
    double edge1Y;
    float edge1Z;
    double edge2X;
    double edge2Y;
    double edge2Z;

    if (offsetX > dy)
    {
        const float elevationB = static_cast<float>(levelOf(cells01) * mpe);
        const float cornerYMinus = cornerY - mpv;
        const float elevationC = static_cast<float>(levelOf(cells11) * mpe);
        const double spanX = cornerXPlus - cornerX;
        edge1X = spanX;
        edge1Y = 0.0;
        edge1Z = elevationB - elevation00;
        edge2X = static_cast<float>(spanX);
        edge2Y = cornerYMinus - cornerY;
        edge2Z = elevationC - elevation00;
    }
    else
    {
        const float cornerXPlusF = static_cast<float>(cornerXPlus);
        const float cornerYMinus = cornerY - mpv;
        const float elevationC = static_cast<float>(levelOf(cells11) * mpe);
        const double elevationD = levelOf(cells10) * mpe;
        const float spanY = cornerYMinus - cornerY;
        edge1X = 0.0;
        edge1Y = spanY;
        edge1Z = static_cast<float>(elevationD - elevation00);
        edge2X = static_cast<double>(cornerXPlusF) - cornerX;
        edge2Y = spanY;
        edge2Z = elevationC - elevation00;
    }

    const float length1 =
        static_cast<float>(std::sqrt((edge1Y * edge1Y + static_cast<double>(edge1Z) * edge1Z) + edge1X * edge1X));

    if (length1 > 0.0f)
    {
        edge1X = edge1X / length1;
        edge1Y = edge1Y / length1;
        edge1Z = static_cast<float>(edge1Z / static_cast<double>(length1));
    }

    const float length2 = static_cast<float>(std::sqrt((edge2Y * edge2Y + edge2X * edge2X) + edge2Z * edge2Z));

    if (length2 > 0.0f)
    {
        edge2X = edge2X / length2;
        edge2Y = edge2Y / length2;
        edge2Z = edge2Z / length2;
    }

    float normalX = static_cast<float>(edge2Z * edge1Y - edge2Y * edge1Z);
    float normalY = static_cast<float>(edge1Z * edge2X - edge2Z * edge1X);
    float normalZ = static_cast<float>(edge2Y * edge1X - edge1Y * edge2X);

    if (normalZ == 0.0f || std::isnan(normalZ))
    {
        Fatal(0, " Vertical Terrain ");
        return 0.0;
    }

    if (normalZ < 0.0f)
    {
        normalX = -normalX;
        normalY = -normalY;
        normalZ = -normalZ;
    }

    return -((static_cast<double>(normalY) / normalZ) * -dy + (static_cast<double>(normalX) / normalZ) * dx) +
           elevation00;
}

auto MCScenarioMap::GetLos(MCVector3D position) -> int32_t
{
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    WorldToMapPos(position, tileR, tileC, cellR, cellC);

    // Port fix: a line walked toward a point off the map reads outside it in the original. Off the map blocks.
    if (!OnMap(tileR, tileC))
    {
        return 0;
    }

    const uint32_t shift = static_cast<uint32_t>((cellR * MAPCELL_DIM + cellC) * 2);
    return ((Map[Width * tileR + tileC].Cells & (0x8000u << shift)) >> (shift + 15)) != 0 ? 1 : 0;
}

auto MCScenarioMap::GetInnerSphereMine(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC) -> uint32_t
{
    const uint32_t layout = (Map[Width * tileR + tileC].Overlay >> 11) & 3;
    return static_cast<uint32_t>(static_cast<int32_t>(MineLayout[layout][cellR * MAPCELL_DIM + cellC]));
}

auto MCScenarioMap::GetClanMine(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC) -> uint32_t
{
    const uint32_t layout = (Map[Width * tileR + tileC].Overlay >> 13) & 3;
    return static_cast<uint32_t>(static_cast<int32_t>(MineLayout[layout][cellR * MAPCELL_DIM + cellC]));
}

auto MCScenarioMap::GetLof(MCVector3D position) -> int32_t
{
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    WorldToMapPos(position, tileR, tileC, cellR, cellC);

    if (position.Z < GetTerrainElevationUnrounded(position))
    {
        return 0;
    }

    // Port fix: the original reads outside the map for a point off it. Off the map blocks.
    if (!OnMap(tileR, tileC))
    {
        return 0;
    }

    return Map[Width * tileR + tileC].GetCellPassable(cellR, cellC) != 0 ? 1 : 0;
}

auto MCScenarioMap::LineOfSight(MCVector3D start, MCVector3D target) -> int
{
    MCVector3D step = target - start;
    step.Normalize();
    const float stepLength = MetersPerWorldUnit * MCTerrain::MetersPerVertexDivMapcellDim * 0.33f;
    step.X = step.X * stepLength;
    step.Y = step.Y * stepLength;
    step.Z = step.Z * stepLength;
    const auto totalDistance = static_cast<float>((start - target).Magnitude() * MetersPerWorldUnit);
    UpdateMovingObjects();

    MCVector3D current = start + step;
    auto distance = static_cast<float>((current - start).Magnitude() * MetersPerWorldUnit);
    int result = 1;

    while (distance < totalDistance)
    {
        if (GetLos(current) == 0)
        {
            result = 0;
        }

        current.X = current.X + step.X;
        current.Y = current.Y + step.Y;
        current.Z = current.Z + step.Z;
        distance = static_cast<float>((current - start).Magnitude() * MetersPerWorldUnit);

        if (result == 0)
        {
            break;
        }
    }

    RestorePreservedMap();
    return result;
}

auto MCScenarioMap::LineOfFire(MCVector3D start, MCVector3D target) -> int
{
    double directionX = static_cast<double>(target.X) - start.X;
    const double deltaY = static_cast<double>(target.Y) - start.Y;
    float directionY = static_cast<float>(deltaY);
    const double length = std::sqrt(deltaY * directionY + directionX * directionX);

    if (length > 0.0)
    {
        directionX = directionX / length;
        directionY = static_cast<float>(directionY / length);
    }

    const float stepLength = MCTerrain::MetersPerVertexDivMapcellDim * 0.33f;
    const float stepX = static_cast<float>(directionX * stepLength);
    const float stepY = directionY * stepLength;
    const double spanX = static_cast<double>(start.X) - target.X;
    const double spanY = static_cast<double>(start.Y) - target.Y;
    const float totalDistance = static_cast<float>(std::sqrt(spanX * spanX + spanY * spanY));
    float currentX = stepX + start.X;
    float currentY = stepY + start.Y;
    int result = 1;

    do
    {
        const double travelledX = static_cast<double>(currentX) - start.X;
        const double travelledY = static_cast<double>(currentY) - start.Y;

        if (totalDistance <= std::sqrt(travelledY * travelledY + travelledX * travelledX))
        {
            return result;
        }

        if (GetLos(MCVector3D(currentX, currentY, 0.0f)) == 0)
        {
            result = 0;
        }

        currentX = currentX + stepX;
        currentY = currentY + stepY;
    } while (result != 0);

    return result;
}

auto MCScenarioMap::LineOfSensor(MCVector3D start, MCVector3D target, int32_t& numBlockingTiles,
                                 int32_t& numBlockingObjects) -> void
{
    MCVector3D step = target - start;
    const double length = std::sqrt((static_cast<double>(step.X) * step.X + static_cast<double>(step.Y) * step.Y) +
                                    static_cast<double>(step.Z) * step.Z);
    double directionZ = step.Z;

    if (length > 0.0)
    {
        step.X = static_cast<float>(step.X / length);
        step.Y = static_cast<float>(step.Y / length);
        directionZ = step.Z / length;
    }

    const double stepLength = static_cast<double>(MetersPerWorldUnit) * MCTerrain::MetersPerVertexDivMapcellDim * 2.0f;
    step.X = static_cast<float>(step.X * stepLength);
    step.Y = static_cast<float>(step.Y * stepLength);
    step.Z = static_cast<float>(directionZ * stepLength);
    const MCVector3D span = start - target;
    const float totalDistance =
        static_cast<float>(std::sqrt((static_cast<double>(span.X) * span.X + static_cast<double>(span.Y) * span.Y) +
                                     static_cast<double>(span.Z) * span.Z) *
                           MetersPerWorldUnit);
    UpdateMovingObjects();

    auto travelledDistance = [&](const MCVector3D& travelled) -> double
    {
        return std::sqrt(
                   (static_cast<double>(travelled.Z) * travelled.Z + static_cast<double>(travelled.Y) * travelled.Y) +
                   static_cast<double>(travelled.X) * travelled.X) *
               MetersPerWorldUnit;
    };

    MCVector3D current = start + step;
    MCVector3D travelled = current - start;
    int32_t prevTileR = 0;
    int32_t prevTileC = 0;
    WorldToMapTilePos(start, prevTileR, prevTileC);
    numBlockingTiles = 0;
    numBlockingObjects = 0;

    if (!(static_cast<float>(travelledDistance(travelled)) >= totalDistance))
    {
        do
        {
            int32_t tileR = 0;
            int32_t tileC = 0;
            WorldToMapTilePos(current, tileR, tileC);

            // Port fix: the original counts blockers on tiles off the map too, reading outside it.
            if ((tileR != prevTileR || tileC != prevTileC) && OnMap(tileR, tileC))
            {
                if (GetTerrainElevationUnrounded(current) > current.Z)
                {
                    numBlockingTiles++;
                }

                numBlockingObjects += GameObjectMap->GetNumSensorBlockingObjects(tileR, tileC);
                numBlockingObjects += (Map[Width * tileR + tileC].Overlay & 0x1000000) == 0x1000000 ? 1 : 0;
                prevTileR = tileR;
                prevTileC = tileC;
            }

            current.X = current.X + step.X;
            current.Y = current.Y + step.Y;
            current.Z = step.Z + current.Z;
            travelled = current - start;
        } while (!(travelledDistance(travelled) >= totalDistance));
    }

    RestorePreservedMap();
}

auto MCScenarioMap::Print(char* fileName, int32_t uLr, int32_t uLc, int32_t printHeight, int32_t printWidth) -> void
{
    MCFile* debugFile = new MCFile;
    debugFile->Create(fileName);

    for (int32_t row = uLr; row < uLr + printHeight; row++)
    {
        char line[512];
        line[0] = '\0';

        for (int32_t col = uLc; col < uLc + printWidth; col++)
        {
            const char* cell = (Map[Width * row + col].Cells & 0x55554000) != 0 ? "." : "X";
            std::strcat(line, cell);
        }

        std::strcat(line, "\n");
        debugFile->WriteString(line);
    }

    debugFile->WriteString("\n");
    debugFile->Close();
    delete debugFile;
}

auto MCScenarioMap::InBounds(int32_t tileR, int32_t tileC) -> int
{
    return tileR >= 0 && tileR < Height && tileC >= 0 && tileC < Width ? 1 : 0;
}

auto MCScenarioMap::GetTile(int32_t tileR, int32_t tileC) -> MCMapTile
{
    Assert(tileR >= 0 && tileR < Height && tileC >= 0 && tileC < Width ? 1 : 0, 0, " Map Tile out of bounds ");
    return Map[Width * tileR + tileC];
}

auto MCScenarioMap::GetOverlayWeight(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, MCMover* mover)
    -> int32_t
{
    const uint32_t overlay = Map[Width * tileR + tileC].Overlay & 0x7f;

    if (overlay == 0)
    {
        return 0;
    }

    const int32_t level = mover->GetOverlayWeightClass();

    if (overlay >= FIRST_GATE_OVERLAY && overlay <= LAST_GATE_OVERLAY)
    {
        const int32_t gate = GateOverlay(overlay, mover->GetAlignment());

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

auto MCObjectMap::Init(MCScenarioMap* newMap) -> void
{
    Map = newMap;
    Width = newMap->Width;
    Height = newMap->Height;
    Rows = std::make_unique<MCObjectPosition*[]>(static_cast<size_t>(Height));
}

namespace
{
    /// <summary>Links <paramref name="node"/> into its row's list, before the first node at or past its column.</summary>
    /// <remarks>The list insertion shared by ObjectMap::addObject and updateObject (inlined in both).</remarks>
    void InsertObjectPosition(MCObjectPosition** rows, MCObjectPosition* node)
    {
        MCObjectPosition*& head = rows[node->TileR];
        MCObjectPosition* current = head;

        if (current == nullptr)
        {
            node->Prev = nullptr;
            node->Next = nullptr;
            head = node;
            return;
        }

        if (current->TileC < node->TileC)
        {
            while (current->Next != nullptr)
            {
                Assert(current != current->Next ? 1 : 0, 0, " Bad ObjPosition Next ");
                current = current->Next;

                if (current->TileC >= node->TileC)
                {
                    break;
                }
            }
        }

        if (current->TileC < node->TileC)
        {
            node->Prev = current;
            node->Next = nullptr;
            current->Next = node;
            return;
        }

        node->Next = current;
        node->Prev = current->Prev;
        current->Prev = node;

        if (node->Prev == nullptr)
        {
            head = node;
        }
        else
        {
            node->Prev->Next = node;
        }
    }

    /// <summary>Unlinks <paramref name="node"/> from its row's list.</summary>
    void UnlinkObjectPosition(MCObjectPosition** rows, MCObjectPosition* node)
    {
        if (node->Prev == nullptr)
        {
            rows[node->TileR] = node->Next;
        }
        else
        {
            node->Prev->Next = node->Next;
        }

        if (node->Next != nullptr)
        {
            node->Next->Prev = node->Prev;
        }
    }

    /// <summary>Frees a row's nodes from <paramref name="node"/> on, clearing their objects' positions.</summary>
    void FreeObjectPositions(MCObjectPosition* node)
    {
        if (node == nullptr)
        {
            return;
        }

        FreeObjectPositions(node->Next);

        if (node->Object != nullptr)
        {
            node->Object->SetObjPosition(nullptr);
        }

        delete node;
    }
}

auto MCObjectMap::AddObject(MCGameObject* object) -> void
{
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    GameMap->WorldToMapPos(object->GetPosition(), tileR, tileC, cellR, cellC);
    auto* node = new MCObjectPosition{};
    node->Object = object;
    node->TileR = tileR;
    node->TileC = tileC;
    node->CellR = cellR;
    node->CellC = cellC;
    node->Prev = nullptr;
    node->Next = nullptr;
    object->SetObjPosition(node);
    InsertObjectPosition(Rows.get(), node);
}

auto MCObjectMap::UpdateObject(MCGameObject* object, int) -> int
{
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    GameMap->WorldToMapPos(object->GetPosition(), tileR, tileC, cellR, cellC);

    char message[1024];
    std::snprintf(message, sizeof(message),
                  "Bad Cell - Object: %d   Positionx: %f   Positiony: %f\t CRow: %d   CCol: %d",
                  object->GetObjectType()->ObjTypeNum, static_cast<double>(object->GetPosition().X),
                  static_cast<double>(object->GetPosition().Y), cellR, cellC);
    Assert(cellR >= 0 && cellR <= 2 ? 1 : 0, static_cast<uint32_t>(cellR), message);
    Assert(cellC >= 0 && cellC <= 2 ? 1 : 0, static_cast<uint32_t>(cellC), message);

    if (tileR < 0 || tileR >= GameMap->Height || tileC < 0 || tileC >= GameMap->Width)
    {
        char offMap[1024];
        std::snprintf(offMap, sizeof(offMap), "Object: %d   Positionx: %f   Positiony: %f\t TRow: %d   TCol: %d",
                      object->GetObjectType()->ObjTypeNum, static_cast<double>(object->GetPosition().X),
                      static_cast<double>(object->GetPosition().Y), tileR, tileC);
        const MCObjectClass objectClass = object->ObjectClass;
        const bool isMover = objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
                             objectClass == MOVER;
        Assert(isMover ? 0 : 1, 0, offMap);
        RemoveObject(object);
        return 0;
    }

    Assert(tileR >= 0 && tileR < Height ? 1 : 0, 0, " Object moved off map ");
    Assert(tileC >= 0 && tileC < Width ? 1 : 0, 0, " Object moved off map ");

    MCObjectPosition* node = object->GetObjPosition();

    if (tileR != node->TileR || tileC != node->TileC)
    {
        UnlinkObjectPosition(Rows.get(), node);
        node->TileR = tileR;
        node->TileC = tileC;
        InsertObjectPosition(Rows.get(), node);
    }

    node->CellR = cellR;
    node->CellC = cellC;
    node->MapCellR = TileMulMapcellDim[tileR] + cellR;
    node->MapCellC = TileMulMapcellDim[tileC] + cellC;
    return 1;
}

auto MCObjectMap::RemoveObject(MCGameObject* object) -> void
{
    MCObjectPosition* node = object->GetObjPosition();

    if (node != nullptr)
    {
        UnlinkObjectPosition(Rows.get(), node);
    }

    delete node;
    object->SetObjPosition(nullptr);
}

auto MCObjectMap::GetNumObjects(int32_t tileR, int32_t tileC) -> int32_t
{
    int32_t count = 0;
    MCObjectPosition* node = GameObjectMap->Rows[tileR];

    if (node == nullptr)
    {
        return 0;
    }
    while (node->TileC < tileC)
    {
        node = node->Next;

        if (node == nullptr)
        {
            return count;
        }
    }

    for (; node != nullptr; node = node->Next)
    {
        if (tileC + 1 <= node->TileC)
        {
            return count;
        }

        if (node->Object != nullptr)
        {
            count++;
        }
    }

    return count;
}

auto MCObjectMap::GetNumSensorBlockingObjects(int32_t tileR, int32_t tileC) -> int32_t
{
    int32_t count = 0;
    MCObjectPosition* node = GameObjectMap->Rows[tileR];

    if (node == nullptr)
    {
        return 0;
    }
    while (node->TileC < tileC)
    {
        node = node->Next;

        if (node == nullptr)
        {
            return count;
        }
    }

    for (; node != nullptr; node = node->Next)
    {
        if (tileC + 1 <= node->TileC)
        {
            return count;
        }

        if (node->Object != nullptr && node->Object->ObjectClass != TREE)
        {
            count++;
        }
    }

    return count;
}

auto MCObjectMap::Destroy() -> void
{
    for (int32_t row = 0; row < Height; row++)
    {
        if (Rows[row] != nullptr)
        {
            FreeObjectPositions(Rows[row]);
            Rows[row] = nullptr;
        }
    }

    Rows.reset();
}

auto CellDirToCell(int32_t fromTileR, int32_t fromTileC, int32_t fromCellR, int32_t fromCellC, int32_t toTileR,
                   int32_t toTileC, int32_t toCellR, int32_t toCellC) -> int32_t
{
    static const int32_t deltaDir[3][3] = {{7, 0, 1}, {6, -1, 2}, {5, 4, 3}};
    const int32_t rowDelta = TileMulMapcellDim[toTileR] + toCellR - TileMulMapcellDim[fromTileR] - fromCellR + 1;
    const int32_t colDelta = TileMulMapcellDim[toTileC] + toCellC - TileMulMapcellDim[fromTileC] - fromCellC + 1;

    if (rowDelta < 0 || rowDelta > 2 || colDelta < 0 || colDelta > 2)
    {
        return -2;
    }

    const int32_t dir = deltaDir[rowDelta][colDelta];
    return dir == -1 ? -2 : dir;
}

auto DebugMoveChunk(MCMover* mover, MCMoveChunk* chunk1, MCMoveChunk* chunk2) -> void
{
    char line[512];
    ChunkDebugMsg[0] = '\0';

    if (mover != nullptr)
    {
        std::snprintf(line, sizeof(line), "Mover = %s (%d)\n", mover->DebugStatus.c_str(), mover->PartId);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "Mover World Pos = (%.4f, %.4f, %.4f)\n",
                      static_cast<double>(mover->GetPosition().X), static_cast<double>(mover->GetPosition().Y),
                      static_cast<double>(mover->GetPosition().Z));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "Mover Obj Pos = [%d, %d, %d, %d]\n", mover->GetObjPosition()->TileR,
                      mover->GetObjPosition()->TileC, mover->GetObjPosition()->CellR, mover->GetObjPosition()->CellC);
        std::strcat(ChunkDebugMsg, line);

        if (mover->GetPilot() == nullptr)
        {
            std::strcat(ChunkDebugMsg, "NULL pilot!\n");
        }

        if (mover->ObjectClass == BATTLEMECH && static_cast<MCBattleMech*>(mover)->InJump != 0)
        {
            int32_t tileR = 0;
            int32_t tileC = 0;
            int32_t cellR = 0;
            int32_t cellC = 0;
            WorldCoordToMapCoord(static_cast<MCBattleMech*>(mover)->JumpGoal, tileR, tileC, cellR, cellC);
            std::snprintf(line, sizeof(line), "Jumping to [%d, %d, %d, %d]\n", tileR, tileC, cellR, cellC);
            std::strcat(ChunkDebugMsg, line);
        }

        std::strcat(ChunkDebugMsg, "\n");
    }

    auto writeChunk = [&line](const char* title, const MCMoveChunk* chunk)
    {
        std::strcat(ChunkDebugMsg, title);

        for (int32_t i = 0; i < MOVECHUNK_NUM_STEPS; i++)
        {
            std::snprintf(line, sizeof(line), "stepPos[%d] = (%d, %d, %d, %d)\n", i, chunk->StepPos[i][0],
                          chunk->StepPos[i][1], chunk->StepPos[i][2], chunk->StepPos[i][3]);
            std::strcat(ChunkDebugMsg, line);
        }

        std::snprintf(line, sizeof(line), "stepRelPos = %d, %d, %d\n", chunk->StepRelPos[0], chunk->StepRelPos[1],
                      chunk->StepRelPos[2]);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "numSteps = %d\n", chunk->NumSteps);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "run = %c\n", chunk->Run != 0 ? 'T' : 'F');
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

    MCFile* debugFile = new MCFile;
    debugFile->Create("mvchunk.dbg");
    debugFile->WriteString(ChunkDebugMsg);
    debugFile->Close();
    delete debugFile;
    ExceptionGameMsg = ChunkDebugMsg;
}

namespace
{
    /// <summary>
    /// Copies path step <paramref name="step"/> into chunk step <paramref name="index"/>, with its direction: from
    /// the start cell for the first step, else the path step's own.
    /// </summary>
    /// <remarks>Inlined three times in MoveChunk::build.</remarks>
    void CopyChunkStep(MCMoveChunk* chunk, int32_t index, const MCPathStep& step)
    {
        int32_t* pos = chunk->StepPos[index];
        pos[0] = step.TileR;
        pos[1] = step.TileC;
        pos[2] = step.CellR;
        pos[3] = step.CellC;

        if (index == 1)
        {
            const int32_t* start = chunk->StepPos[0];
            chunk->StepRelPos[0] =
                CellDirToCell(start[0], start[1], start[2], start[3], pos[0], pos[1], pos[2], pos[3]);
        }
        else
        {
            chunk->StepRelPos[index - 1] = static_cast<int8_t>(step.Direction);
        }
    }

    /// <summary>Makes the chunk a single step at the mover's cell.</summary>
    void SetChunkAtMover(MCMoveChunk* chunk, MCMover* mover)
    {
        const MCObjectPosition* objPosition = mover->GetObjPosition();
        chunk->StepPos[0][0] = objPosition->TileR;
        chunk->StepPos[0][1] = objPosition->TileC;
        chunk->StepPos[0][2] = objPosition->CellR;
        chunk->StepPos[0][3] = objPosition->CellC;
        chunk->StepRelPos[0] = 0;
        chunk->StepRelPos[1] = 0;
        chunk->StepRelPos[2] = 0;
        chunk->NumSteps = 1;
    }
}

auto MCMoveChunk::Build(MCMover* mover, MCMovePath* path1, MCMovePath* path2) -> void
{
    SetChunkAtMover(this, mover);

    if (path1 != nullptr && path1->NumSteps > 0)
    {
        const int32_t pathSteps = path1->NumSteps;
        const int32_t curStep = path1->CurStep;
        int32_t index = 1;
        int roomLeft = 1;

        if (curStep < pathSteps)
        {
            const MCPathStep& current = path1->StepList[curStep];
            int onCurrentStep = StepPos[0][0] == current.TileR && StepPos[0][1] == current.TileC &&
                                StepPos[0][2] == current.CellR && StepPos[0][3] == current.CellC;

            if (!onCurrentStep && CellDirToCell(StepPos[0][0], StepPos[0][1], StepPos[0][2], StepPos[0][3],
                                                current.TileR, current.TileC, current.CellR, current.CellC) == -2)
            {
                onCurrentStep = 1; // not next to the current step: start the chunk from it
            }

            int32_t firstStep;
            int32_t stepsLeft;

            if (onCurrentStep)
            {
                StepPos[0][0] = current.TileR;
                StepPos[0][1] = current.TileC;
                StepPos[0][2] = current.CellR;
                StepPos[0][3] = current.CellC;
                firstStep = curStep + 1;
                stepsLeft = pathSteps - firstStep;
            }
            else
            {
                firstStep = curStep;
                stepsLeft = pathSteps - curStep;
            }

            NumSteps = MOVECHUNK_NUM_STEPS - 1;
            roomLeft = stepsLeft < MOVECHUNK_NUM_STEPS - 1;

            if (roomLeft)
            {
                NumSteps = stepsLeft;
            }

            for (int32_t i = 0; i < NumSteps; i++)
            {
                CopyChunkStep(this, index++, path1->StepList[firstStep + i]);
            }

            NumSteps++;
        }

        const int nextLeg = path2 != nullptr && path1->GlobalStep >= 0 && path2->GlobalStep == path1->GlobalStep + 1;

        if (roomLeft && nextLeg && path2->NumStepsWhenNotPaused > 0 && NumSteps < MOVECHUNK_NUM_STEPS)
        {
            int32_t extraSteps = MOVECHUNK_NUM_STEPS - NumSteps;

            if (path2->NumSteps < extraSteps)
            {
                extraSteps = path2->NumSteps;
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

                CopyChunkStep(this, index++, path2->StepList[i]);
            }

            NumSteps = extraSteps + NumSteps;
            Assert(NumSteps < MOVECHUNK_NUM_STEPS + 1 ? 1 : 0, static_cast<uint32_t>(index),
                   " MoveChunk.build: path2 and bad curStep > MOVECHUNK_NUM_STEPS ");
        }
    }

    if (NumSteps < 1 || NumSteps > MOVECHUNK_NUM_STEPS)
    {
        SetChunkAtMover(this, mover);
    }

    Run = mover->GetPilot()->MoveOrders.Run;

    if (Run != 0 && mover->ObjectClass == BATTLEMECH)
    {
        Run = static_cast<MCBattleMech*>(mover)->LegStatus == 0 ? 1 : 0;
    }

    Data = 0;
}

auto MCMoveChunk::Build(MCMover*, MCVector3D jumpGoal) -> void
{
    WorldCoordToMapCoord(jumpGoal, StepPos[0][0], StepPos[0][1], StepPos[0][2], StepPos[0][3]);
    StepRelPos[0] = 0;
    StepRelPos[1] = 0;
    StepRelPos[2] = 0;
    NumSteps = 1;
    Run = 0;
}

auto MCMoveChunk::Pack(MCMover* mover) -> void
{
    const int32_t stepCount = NumSteps;
    uint32_t packed = static_cast<uint32_t>(TileMulMapcellDim[StepPos[0][1]] + StepPos[0][3]) << 3 |
                      static_cast<uint32_t>(TileMulMapcellDim[StepPos[0][0]] + StepPos[0][2]) << 13 |
                      static_cast<uint32_t>(stepCount * 2 - 2);

    if (Run != 0)
    {
        packed |= 1;
    }

    // Original behaviour (OB-029): a direction of -2 (MoveChunk::build found no neighbour) spills into the upper
    // bits.
    packed = ((packed << 3 | static_cast<uint32_t>(StepRelPos[0])) << 3 | static_cast<uint32_t>(StepRelPos[1])) << 3 |
             static_cast<uint32_t>(StepRelPos[2]);
    Data = packed;

    if (stepCount < 1 || stepCount > MOVECHUNK_NUM_STEPS)
    {
        DebugMoveChunk(mover, this, nullptr);
        char message[1024];
        std::snprintf(message, sizeof(message), " MoveChunk.pack: bad numSteps %d (save mvchunk.dbg file) ", NumSteps);
        Assert(0, static_cast<uint32_t>(NumSteps), message);
    }
}

auto MCMoveChunk::Unpack(MCMover* mover) -> void
{
    MoveChunkUnpackErr = 0;
    const uint32_t packed = Data;
    StepRelPos[2] = static_cast<int32_t>(packed & 7);
    StepRelPos[1] = static_cast<int32_t>((packed >> 3) & 7);
    StepRelPos[0] = static_cast<int32_t>((packed >> 6) & 7);
    NumSteps = static_cast<int32_t>((packed >> 10) & 3) + 1;
    Run = static_cast<int32_t>((packed >> 9) & 1);
    const uint32_t cellCol = (packed >> 12) & 0x3ff;
    StepPos[0][1] = static_cast<int32_t>(cellCol / 3);
    StepPos[0][3] = static_cast<int32_t>(cellCol) - TileMulMapcellDim[cellCol / 3];
    const uint32_t cellRow = packed >> 22;
    Data = cellRow; // as the original: the packed word is left holding the start cell row
    StepPos[0][0] = static_cast<int32_t>(cellRow / 3);
    StepPos[0][2] = static_cast<int32_t>(cellRow) - TileMulMapcellDim[cellRow / 3];

    if (NumSteps < 1 || NumSteps > MOVECHUNK_NUM_STEPS)
    {
        MoveChunkUnpackErr = 1;
        return;
    }

    for (int32_t i = 0; i < NumSteps - 1; i++)
    {
        if (i < 0 || i > 2)
        {
            MoveChunkUnpackErr = 2;
            return;
        }

        const int32_t dir = StepRelPos[i];

        if (dir < 0 || dir > 7)
        {
            MoveChunkUnpackErr = 3;
            return;
        }

        const int32_t* adj = AdjCellTable[StepPos[i][2] * MAPCELL_DIM + StepPos[i][3]][dir];
        StepPos[i + 1][0] = adj[0] + StepPos[i][0];
        StepPos[i + 1][1] = adj[1] + StepPos[i][1];
        StepPos[i + 1][2] = adj[2];
        StepPos[i + 1][3] = adj[3];
    }

    if (NumSteps < 1 || NumSteps > MOVECHUNK_NUM_STEPS)
    {
        DebugMoveChunk(mover, this, nullptr);
        char message[1024];
        std::snprintf(message, sizeof(message), " MoveChunk.unpack: bad numSteps %d (save mvchunk.dbg file) ",
                      NumSteps);
        Assert(0, static_cast<uint32_t>(NumSteps), message);
    }
}

auto MCMoveChunk::EqualTo(MCMover* mover, MCMoveChunk* chunk) -> int
{
    if (NumSteps != chunk->NumSteps || Run != chunk->Run)
    {
        DebugMoveChunk(mover, this, chunk);
        return 0;
    }

    for (int32_t i = 0; i < NumSteps; i++)
    {
        for (int32_t j = 0; j < 4; j++)
        {
            if (StepPos[i][j] != chunk->StepPos[i][j])
            {
                DebugMoveChunk(mover, this, chunk);
                return 0;
            }
        }
    }

    for (int32_t i = 0; i < NumSteps - 1; i++)
    {
        if (StepRelPos[i] != chunk->StepRelPos[i])
        {
            DebugMoveChunk(mover, this, chunk);
            return 0;
        }
    }

    return 1;
}

auto MCMovePath::Init(int32_t newNumSteps) -> int32_t
{
    NumStepsWhenNotPaused = newNumSteps;
    NumSteps = newNumSteps;

    if (MaxMovePathSteps < newNumSteps)
    {
        MaxMovePathSteps = newNumSteps;
        return newNumSteps;
    }

    return -1;
}

auto MCMovePath::Clear() -> void
{
    if (NumSteps > 0)
    {
        Unmark();
    }

    Goal.Zero();
    NumSteps = 0;
    NumStepsWhenNotPaused = 0;
    CurStep = 0;
    Cost = 0;
    Marked = 0;
    GlobalStep = -1;
}

auto MCMovePath::Destroy() -> void
{
    NumSteps = 0;
}

auto MCMovePath::GetDistanceLeft(MCVector3D position, int32_t fromStep) -> float
{
    if (fromStep == -1)
    {
        fromStep = CurStep;
    }

    const MCPathStep& step = StepList[fromStep];
    return std::sqrt((position.X - step.Destination.X) * (position.X - step.Destination.X) +
                     (position.Z - step.Destination.Z) * (position.Z - step.Destination.Z) +
                     (position.Y - step.Destination.Y) * (position.Y - step.Destination.Y)) *
               MetersPerWorldUnit +
           step.DistanceToGoal;
}

auto MCMovePath::Mark() -> void
{
    if (Marked != 0)
    {
        return;
    }

    for (int32_t i = 0; i < NumSteps; i++)
    {
        GameMap->PathMap[GameMap->Width * StepList[i].TileR + StepList[i].TileC]++;
        Assert(1, 0, " Negative pathMap Count ");
    }

    Marked = 1;
}

auto MCMovePath::Unmark() -> void
{
    if (Marked == 0)
    {
        return;
    }

    for (int32_t i = 0; i < NumSteps; i++)
    {
        GameMap->PathMap[GameMap->Width * StepList[i].TileR + StepList[i].TileC]--;
        Assert(1, 0, " Negative pathMap Count ");
    }

    Marked = 0;
}

auto MCMovePath::Lock(int32_t start, int32_t range, uint32_t setting) -> void
{
    if (start == -1)
    {
        start = CurStep;
    }

    int32_t end = start + range;

    if (NumStepsWhenNotPaused <= end)
    {
        end = NumStepsWhenNotPaused;
    }

    for (int32_t i = start; i < end; i++)
    {
        GameMap->SetCellPathLocked(StepList[i].TileR, StepList[i].TileC, StepList[i].CellR, StepList[i].CellC, setting);
    }
}

auto MCMovePath::IsLocked(int32_t start, int32_t range, int* reachedEnd) -> int
{
    if (start == -1)
    {
        start = CurStep;
    }

    if (reachedEnd != nullptr)
    {
        *reachedEnd = 0;
    }

    int32_t end = range + start;

    if (NumStepsWhenNotPaused <= end)
    {
        end = NumStepsWhenNotPaused;

        if (reachedEnd != nullptr)
        {
            *reachedEnd = 1;
        }
    }

    for (int32_t i = start; i < end; i++)
    {
        if (GameMap->GetCellPathLocked(StepList[i].TileR, StepList[i].TileC, StepList[i].CellR, StepList[i].CellC) != 0)
        {
            return 1;
        }
    }

    return 0;
}

auto MCMovePath::IsBlocked(int32_t start, int32_t range, int* reachedEnd) -> int
{
    if (start == -1)
    {
        start = CurStep;
    }

    if (reachedEnd != nullptr)
    {
        *reachedEnd = 0;
    }

    int32_t end = range + start;

    if (NumStepsWhenNotPaused <= end)
    {
        end = NumStepsWhenNotPaused;

        if (reachedEnd != nullptr)
        {
            *reachedEnd = 1;
        }
    }

    for (int32_t i = start; i < end; i++)
    {
        const MCMapTile& tile = GameMap->Map[StepList[i].TileR * GameMap->Width + StepList[i].TileC];

        if (TileCellPassable(tile, StepList[i].CellR, StepList[i].CellC) == 0)
        {
            return 1;
        }
    }

    return 0;
}

auto MCMovePath::CrossesBridge(int32_t start, int32_t range) -> int32_t
{
    if (start == -1)
    {
        start = CurStep;
    }

    int32_t end = start + range;

    if (NumStepsWhenNotPaused <= end)
    {
        end = NumStepsWhenNotPaused;
    }

    for (int32_t i = start; i < end; i++)
    {
        const uint32_t overlay = GameMap->Map[StepList[i].TileR * GameMap->Width + StepList[i].TileC].Overlay & 0x7f;

        if (OverlayIsBridge[overlay] != 0)
        {
            return GlobalMoveMap->CalcArea(StepList[i].TileR, StepList[i].TileC);
        }
    }

    return -1;
}

auto MCMovePath::CrossesTile(int32_t start, int32_t range, int32_t tileR, int32_t tileC) -> int32_t
{
    if (start == -1)
    {
        start = CurStep;
    }

    int32_t end = range + start;

    if (NumStepsWhenNotPaused <= end)
    {
        end = NumStepsWhenNotPaused;
    }

    for (int32_t i = start; i < end; i++)
    {
        if (tileR == StepList[i].TileR && tileC == StepList[i].TileC)
        {
            return i;
        }
    }

    return -1;
}

auto MCMovePath::CrossesClosedClanGate(int32_t, int32_t) -> int32_t
{
    return -1;
}

auto MCMovePath::CrossesClosedISGate(int32_t, int32_t) -> int32_t
{
    return -1;
}

auto MCMovePath::CrossesClosedGate(int32_t start, int32_t range) -> int32_t
{
    if (start == -1)
    {
        start = CurStep;
    }

    int32_t end = range + start;

    if (NumStepsWhenNotPaused <= end)
    {
        end = NumStepsWhenNotPaused;
    }

    for (int32_t i = start; i < end; i++)
    {
        const uint32_t overlay = GameMap->Map[StepList[i].TileR * GameMap->Width + StepList[i].TileC].Overlay & 0x7f;

        if (OverlayIsClosedGate[overlay] != 0)
        {
            return i;
        }
    }

    return -1;
}

auto MCMovePath::SetMoveChunk(MCMoveChunk* chunk) -> void
{
    const int32_t stepCount = chunk->NumSteps;

    for (int32_t i = 0; i < stepCount; i++)
    {
        MCPathStep& step = StepList[i];
        step.TileR = static_cast<int16_t>(chunk->StepPos[i][0]);
        step.TileC = static_cast<int16_t>(chunk->StepPos[i][1]);
        step.CellR = static_cast<int16_t>(chunk->StepPos[i][2]);
        step.CellC = static_cast<int16_t>(chunk->StepPos[i][3]);
        step.Destination.X = static_cast<float>(static_cast<double>(CellToWorldCoord[step.CellC]) +
                                                TileColWorldCoords[step.TileC] + HalfMapCell);
        step.DistanceToGoal = 0.0f;
        step.Destination.Z = 0.0f;
        step.Direction = 0;
        step.Destination.Y = static_cast<float>(static_cast<double>(TileRowWorldCoords[step.TileR]) -
                                                CellToWorldCoord[step.CellR] - HalfMapCell);
    }

    NumStepsWhenNotPaused = stepCount;
    NumSteps = stepCount;
    CurStep = 0;
    // With no steps the original reads numSteps, numStepsWhenNotPaused and curStep (all 0 by then) as the goal.
    Goal = stepCount > 0 ? StepList[stepCount - 1].Destination : MCVector3D(0.0f, 0.0f, 0.0f);
    Target.Zero();
    Marked = 0;
    Cost = -1;
    GlobalStep = -1;
}

auto MCMovePath::GetMoveChunk(MCMoveChunk*, int32_t, int32_t, int) -> void
{
}

auto MCMovePath::SetDestination(int32_t stepNumber, MCVector3D position) -> void
{
    StepList[stepNumber].Destination = position;
}

auto MCMovePathManager::Init() -> int32_t
{
    for (int32_t i = 0; i < MAX_PATH_QUEUE_RECS; i++)
    {
        Pool[i].Pilot = nullptr;
        Pool[i].SelectionIndex = 0;
        Pool[i].MoveParams = 0;
        Pool[i].Prev = i > 0 ? &Pool[i - 1] : nullptr;
        Pool[i].Next = i < MAX_PATH_QUEUE_RECS - 1 ? &Pool[i + 1] : nullptr;
    }

    QueueFront = nullptr;
    QueueEnd = nullptr;
    NumPathsInQueue = 0;
    FreeList = &Pool[0];
    return 0;
}

auto MCMovePathManager::Destroy() -> void
{
}

auto MCMovePathManager::Remove(MCPathQueueRec* rec) -> void
{
    if (rec->Prev == nullptr)
    {
        QueueFront = rec->Next;
    }
    else
    {
        rec->Prev->Next = rec->Next;
    }

    if (rec->Next == nullptr)
    {
        QueueEnd = rec->Prev;
    }
    else
    {
        rec->Next->Prev = rec->Prev;
    }

    rec->Prev = nullptr;
    rec->Next = FreeList;
    FreeList = rec;
    NumPathsInQueue--;
}

auto MCMovePathManager::Remove(MCMechWarrior* pilot) -> MCPathQueueRec*
{
    MCPathQueueRec* rec = pilot->MovePathRequest;

    if (rec == nullptr)
    {
        return nullptr;
    }

    Remove(rec);
    pilot->MovePathRequest = nullptr;
    return rec;
}

auto MCMovePathManager::Request(MCMechWarrior* pilot, int32_t selectionIndex, uint32_t moveParams, float priority,
                                int32_t initPath) -> void
{
    Remove(pilot);
    MCPathQueueRec* rec = FreeList;

    if (rec == nullptr)
    {
        Fatal(0, " Too many pilots calcing paths ");
    }

    FreeList = rec->Next;

    if (FreeList != nullptr)
    {
        FreeList->Prev = nullptr;
    }

    rec->SelectionIndex = selectionIndex;
    rec->InitPath = initPath;
    rec->Priority = priority;
    rec->Pilot = pilot;
    rec->MoveParams = moveParams;

    if (QueueEnd == nullptr)
    {
        rec->Prev = nullptr;
        rec->Next = nullptr;
        QueueEnd = rec;
        QueueFront = rec;
    }
    else
    {
        // Walk back from the end to the last request of higher priority value, and queue behind it.
        MCPathQueueRec* after = nullptr;
        MCPathQueueRec* current = QueueEnd;

        for (; current != nullptr; current = current->Prev)
        {
            if (priority < current->Priority)
            {
                break;
            }

            after = current;
        }

        if (current != nullptr)
        {
            rec->Prev = current;
            rec->Next = current->Next;
            current->Next = rec;

            if (rec->Next == nullptr)
            {
                QueueEnd = rec;
            }
            else
            {
                rec->Next->Prev = rec;
            }
        }
        else
        {
            rec->Prev = nullptr;
            rec->Next = after;
            after->Prev = rec;
            QueueFront = rec;
        }
    }

    pilot->MovePathRequest = rec;
    NumPathsInQueue++;
}

auto MCMovePathManager::CalcPath() -> void
{
    MCPathQueueRec* rec = QueueFront;

    if (rec == nullptr)
    {
        return;
    }

    Remove(rec);
    MCMechWarrior* pilot = rec->Pilot;
    pilot->MovePathRequest = nullptr;

    if (pilot->Vehicle != nullptr)
    {
        pilot->CalcMovePath(rec->SelectionIndex, rec->MoveParams, rec->InitPath);
    }
}

auto MCMovePathManager::Update() -> void
{
    for (int32_t i = 0; i < 5; i++)
    {
        if (QueueFront == nullptr)
        {
            return;
        }

        CalcPath();
    }
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
    void DecodeArea(const uint8_t* record, MCGlobalMapArea& area)
    {
        area.SectorR = ReadRecordField<int16_t>(record);
        area.SectorC = ReadRecordField<int16_t>(record);
        ReadRecordField<uint32_t>(record);
        area.Doors = nullptr;
        area.Type = ReadRecordField<int32_t>(record);
        area.NumDoors = ReadRecordField<char>(record);
        area.Open = ReadRecordField<int32_t>(record);
        // Two editor-only words nothing reads.
        ReadRecordField<int32_t>(record);
        ReadRecordField<int32_t>(record);
        area.Closed = ReadRecordField<int32_t>(record);
        // The record ends with three editor-only words nothing reads.
    }

    /// <summary>Encodes an area as its 0x29-byte file record (the doors pointer written as 0).</summary>
    void EncodeArea(const MCGlobalMapArea& area, uint8_t* record)
    {
        WriteRecordField<int16_t>(record, area.SectorR);
        WriteRecordField<int16_t>(record, area.SectorC);
        WriteRecordField<uint32_t>(record, 0);
        WriteRecordField<int32_t>(record, area.Type);
        WriteRecordField<char>(record, area.NumDoors);
        WriteRecordField<int32_t>(record, area.Open);
        // The editor-only words, with the values the editor's calcAreas gives them.
        WriteRecordField<int32_t>(record, -1);
        WriteRecordField<int32_t>(record, 0);
        WriteRecordField<int32_t>(record, area.Closed);
        WriteRecordField<int32_t>(record, 0);
        WriteRecordField<int32_t>(record, 0);
        WriteRecordField<int32_t>(record, 0);
    }

    /// <summary>Decodes a 0x3b-byte door record (the link pointers are left for the caller).</summary>
    void DecodeDoor(const uint8_t* record, MCGlobalMapDoor& door)
    {
        door.Row = ReadRecordField<int16_t>(record);
        door.Col = ReadRecordField<int16_t>(record);
        door.CellR = ReadRecordField<uint8_t>(record);
        door.CellC = ReadRecordField<uint8_t>(record);
        door.Length = ReadRecordField<char>(record);
        door.Open = ReadRecordField<int32_t>(record);
        door.Area[0] = ReadRecordField<int16_t>(record);
        door.Area[1] = ReadRecordField<int16_t>(record);
        door.AreaCost[0] = ReadRecordField<int16_t>(record);
        door.AreaCost[1] = ReadRecordField<int16_t>(record);
        door.Direction[0] = ReadRecordField<char>(record);
        door.Direction[1] = ReadRecordField<char>(record);
        door.NumLinks[0] = ReadRecordField<char>(record);
        door.NumLinks[1] = ReadRecordField<char>(record);
        ReadRecordField<uint32_t>(record);
        ReadRecordField<uint32_t>(record);
        door.Links[0] = nullptr;
        door.Links[1] = nullptr;
        door.Cost = ReadRecordField<int32_t>(record);
        door.Parent = ReadRecordField<int32_t>(record);
        door.FromAreaIndex = ReadRecordField<int32_t>(record);
        door.Flags = ReadRecordField<uint32_t>(record);
        door.G = ReadRecordField<int32_t>(record);
        door.HPrime = ReadRecordField<int32_t>(record);
        door.FPrime = ReadRecordField<int32_t>(record);
    }

    /// <summary>Encodes a door as its 0x3b-byte file record (the link pointers written as 0).</summary>
    void EncodeDoor(const MCGlobalMapDoor& door, uint8_t* record)
    {
        WriteRecordField<int16_t>(record, door.Row);
        WriteRecordField<int16_t>(record, door.Col);
        WriteRecordField<uint8_t>(record, door.CellR);
        WriteRecordField<uint8_t>(record, door.CellC);
        WriteRecordField<char>(record, door.Length);
        WriteRecordField<int32_t>(record, door.Open);
        WriteRecordField<int16_t>(record, door.Area[0]);
        WriteRecordField<int16_t>(record, door.Area[1]);
        WriteRecordField<int16_t>(record, door.AreaCost[0]);
        WriteRecordField<int16_t>(record, door.AreaCost[1]);
        WriteRecordField<char>(record, door.Direction[0]);
        WriteRecordField<char>(record, door.Direction[1]);
        WriteRecordField<char>(record, door.NumLinks[0]);
        WriteRecordField<char>(record, door.NumLinks[1]);
        WriteRecordField<uint32_t>(record, 0);
        WriteRecordField<uint32_t>(record, 0);
        WriteRecordField<int32_t>(record, door.Cost);
        WriteRecordField<int32_t>(record, door.Parent);
        WriteRecordField<int32_t>(record, door.FromAreaIndex);
        WriteRecordField<uint32_t>(record, door.Flags);
        WriteRecordField<int32_t>(record, door.G);
        WriteRecordField<int32_t>(record, door.HPrime);
        WriteRecordField<int32_t>(record, door.FPrime);
    }

    /// <summary>Whether (row, col) lies in the sector being filled (GlobalMap::minTileR and friends).</summary>
    bool InFillSector(int32_t row, int32_t col)
    {
        return row >= MCGlobalMap::MinTileR && row < MCGlobalMap::MaxTileR && col >= MCGlobalMap::MinTileC &&
               col < MCGlobalMap::MaxTileC;
    }

    /// <summary>A tile's overlay type.</summary>
    uint32_t TileOverlay(MCScenarioMap* map, int32_t row, int32_t col)
    {
        return map->Map[map->Width * row + col].Overlay & 0x7f;
    }

    /// <summary>Cells per row of the door finder's cell map (MCX: a 120 x 120 short array on the stack).</summary>
    constexpr int32_t DOOR_CELL_MAP_SIDE = 120;
}

auto MCGlobalMap::Init(int32_t newWidth, int32_t newHeight) -> void
{
    Width = newWidth;
    Height = newHeight;
    AreaMap = static_cast<int16_t*>(Blocks.Allocate(static_cast<uint32_t>(newWidth * newHeight * sizeof(int16_t))));

    if (AreaMap == nullptr)
    {
        Fatal(0, "Not enough Memory for LargeAreaMap");
    }

    for (int32_t row = 0; row < Height; row++)
    {
        for (int32_t col = 0; col < Width; col++)
        {
            AreaMap[Width * row + col] = -1;
        }
    }

    SectorDim = 10;

    if (Width % 10 != 0 || Height % 10 != 0)
    {
        Fatal(0, "Scenario Map Dimensions must be multiples of SectorDim");
    }

    NumAreas = 0;
    Areas = nullptr;
    NumDoors = 0;
    SectorWidth = newWidth / 10;
    SectorHeight = newWidth / 10;
    Doors = nullptr;
    DoorBuildList = nullptr;
    PathCostTable = nullptr;
}

auto MCGlobalMap::Init(MCFile* mapFile) -> int32_t
{
    const int32_t version = mapFile->ReadLong();

    if (version != GLOBALMAP_VERSION)
    {
        Fatal(version, " Bad version number in Global Map ");
    }

    // Header words 1 and 2: written by the editor (always 0), read by nothing.
    mapFile->ReadLong();
    mapFile->ReadLong();
    Height = mapFile->ReadLong();
    Width = mapFile->ReadLong();
    SectorDim = mapFile->ReadLong();
    SectorHeight = mapFile->ReadLong();
    SectorWidth = mapFile->ReadLong();
    NumAreas = mapFile->ReadLong();
    NumDoors = mapFile->ReadLong();
    NumDoorInfos = mapFile->ReadLong();
    NumDoorLinks = mapFile->ReadLong();
    SmallAreaMap = nullptr;
    AreaMap = nullptr;

    if (NumAreas < 256)
    {
        const int32_t size = Width * Height;
        SmallAreaMap = static_cast<uint8_t*>(Blocks.Allocate(static_cast<uint32_t>(size)));

        if (SmallAreaMap == nullptr)
        {
            Fatal(0, " Not Enough Memory for GlobalMap:smallAreaMap ");
        }

        mapFile->Read(SmallAreaMap, size);
    }
    else
    {
        const int32_t size = Width * Height * 2;
        AreaMap = static_cast<int16_t*>(Blocks.Allocate(static_cast<uint32_t>(size)));

        if (AreaMap == nullptr)
        {
            Fatal(0, " Not Enough Memory for GlobalMap:largeAreaMap ");
        }

        mapFile->Read(reinterpret_cast<uint8_t*>(AreaMap), size);
    }

    DoorInfos = static_cast<MCDoorInfo*>(Blocks.Allocate(static_cast<uint32_t>(NumDoorInfos * sizeof(MCDoorInfo))));

    if (DoorInfos == nullptr)
    {
        Fatal(0, " Not Enough Memory for GlobalMap:doorInfos ");
    }

    mapFile->Read(reinterpret_cast<uint8_t*>(DoorInfos), NumDoorInfos * static_cast<int32_t>(sizeof(MCDoorInfo)));

    // Port fix: room for the spare area setTempArea writes (the original allocated exactly numAreas here).
    Areas =
        static_cast<MCGlobalMapArea*>(Blocks.Allocate(static_cast<uint32_t>((NumAreas + 1) * sizeof(MCGlobalMapArea))));

    if (Areas == nullptr)
    {
        Fatal(0, " Not Enough Memory for GlobalMap:areas ");
    }

    std::vector<uint8_t> records(static_cast<size_t>(NumAreas) * GLOBALMAP_AREA_RECORD_SIZE);
    mapFile->Read(records.data(), NumAreas * GLOBALMAP_AREA_RECORD_SIZE);
    int32_t infoIndex = 0;

    for (int32_t i = 0; i < NumAreas; i++)
    {
        DecodeArea(records.data() + static_cast<size_t>(i) * GLOBALMAP_AREA_RECORD_SIZE, Areas[i]);
        Areas[i].Doors = DoorInfos + infoIndex;
        infoIndex += Areas[i].NumDoors;
    }

    std::memset(&Areas[NumAreas], 0, sizeof(MCGlobalMapArea));

    DoorLinks = static_cast<MCDoorLink*>(Blocks.Allocate(static_cast<uint32_t>(NumDoorLinks * sizeof(MCDoorLink))));

    if (DoorLinks == nullptr)
    {
        Fatal(0, " Not Enough Memory for GlobalMap:doorlinks ");
    }

    mapFile->Read(reinterpret_cast<uint8_t*>(DoorLinks), NumDoorLinks * static_cast<int32_t>(sizeof(MCDoorLink)));

    const int32_t totalDoors = NumDoors + 2;
    Doors = static_cast<MCGlobalMapDoor*>(Blocks.Allocate(static_cast<uint32_t>(totalDoors * sizeof(MCGlobalMapDoor))));

    if (Doors == nullptr)
    {
        Fatal(0, " Not Enough Memory for GlobalMap:doors ");
    }

    records.assign(static_cast<size_t>(totalDoors) * GLOBALMAP_DOOR_RECORD_SIZE, 0);
    mapFile->Read(records.data(), totalDoors * GLOBALMAP_DOOR_RECORD_SIZE);
    int32_t linkIndex = 0;

    for (int32_t i = 0; i < totalDoors; i++)
    {
        MCGlobalMapDoor& door = Doors[i];
        DecodeDoor(records.data() + static_cast<size_t>(i) * GLOBALMAP_DOOR_RECORD_SIZE, door);

        for (int32_t side = 0; side < 2; side++)
        {
            door.Links[side] = DoorLinks + linkIndex;
            Assert(door.NumLinks[side] + 2 > 1 ? 1 : 0, 0, " Bad Door Links Count ");
            linkIndex += door.NumLinks[side] + 2;
        }
    }

    PathCostTable = static_cast<uint8_t*>(Blocks.Allocate(static_cast<uint32_t>(NumAreas * NumAreas)));

    if (PathCostTable == nullptr)
    {
        Fatal(0, " Not Enough Memory for GlobalMap.pathCostTable ");
    }

    mapFile->Read(PathCostTable, NumAreas * NumAreas);
    return 0;
}

auto MCGlobalMap::Init(MCScenarioMap* map, int32_t newHeight, int32_t newWidth) -> int32_t
{
    if (newHeight == -1)
    {
        newHeight = map->Height;
    }

    if (newWidth == -1)
    {
        newWidth = map->Width;
    }

    Init(newHeight, newWidth); // as the original: the map's height goes to init's width slot (maps are square)
    CalcAreas(map);
    CalcBridges(map);
    CalcGlobalDoors(map);
    CalcAreaDoors();
    CalcDoorLinks();

    if (NumAreas < 256)
    {
        SmallAreaMap = static_cast<uint8_t*>(Blocks.Allocate(static_cast<uint32_t>(Height * Width)));

        if (SmallAreaMap == nullptr)
        {
            Fatal(0, " Not Enough Memory for SmallAreaMap ");
        }

        for (int32_t row = 0; row < Height; row++)
        {
            for (int32_t col = 0; col < Width; col++)
            {
                const int16_t area = AreaMap[Width * row + col];
                SmallAreaMap[Width * row + col] = area < 0 ? 0xff : static_cast<uint8_t>(area);
            }
        }

        Blocks.Free(AreaMap);
        AreaMap = nullptr;
    }

    return 0;
}

auto MCGlobalMap::Write(MCFile* mapFile) -> int32_t
{
    mapFile->WriteLong(GLOBALMAP_VERSION);
    mapFile->WriteLong(0);
    mapFile->WriteLong(0);
    mapFile->WriteLong(Height);
    mapFile->WriteLong(Width);
    mapFile->WriteLong(SectorDim);
    mapFile->WriteLong(SectorHeight);
    mapFile->WriteLong(SectorWidth);
    mapFile->WriteLong(NumAreas);
    mapFile->WriteLong(NumDoors);
    mapFile->WriteLong(NumDoorInfos);
    mapFile->WriteLong(NumDoorLinks);

    if (SmallAreaMap != nullptr)
    {
        mapFile->Write(SmallAreaMap, Width * Height);
    }
    else
    {
        mapFile->Write(reinterpret_cast<const uint8_t*>(AreaMap), Width * Height * 2);
    }

    for (int32_t i = 0; i < NumAreas; i++)
    {
        mapFile->Write(reinterpret_cast<const uint8_t*>(Areas[i].Doors), Areas[i].NumDoors * 3);
    }

    std::vector<uint8_t> records(static_cast<size_t>(NumAreas) * GLOBALMAP_AREA_RECORD_SIZE);

    for (int32_t i = 0; i < NumAreas; i++)
    {
        EncodeArea(Areas[i], records.data() + static_cast<size_t>(i) * GLOBALMAP_AREA_RECORD_SIZE);
    }

    mapFile->Write(records.data(), NumAreas * GLOBALMAP_AREA_RECORD_SIZE);

    const int32_t totalDoors = NumDoors + 2;

    for (int32_t i = 0; i < totalDoors; i++)
    {
        for (int32_t side = 0; side < 2; side++)
        {
            const int32_t count = Doors[i].NumLinks[side] + 2;
            Assert(count > 1 ? 1 : 0, 0, " Bad Door Links Count ");
            mapFile->Write(reinterpret_cast<const uint8_t*>(Doors[i].Links[side]), count * 7);
        }
    }

    records.assign(static_cast<size_t>(totalDoors) * GLOBALMAP_DOOR_RECORD_SIZE, 0);

    for (int32_t i = 0; i < totalDoors; i++)
    {
        EncodeDoor(Doors[i], records.data() + static_cast<size_t>(i) * GLOBALMAP_DOOR_RECORD_SIZE);
    }

    mapFile->Write(records.data(), totalDoors * GLOBALMAP_DOOR_RECORD_SIZE);

    CalcPathCostTable();
    mapFile->Write(PathCostTable, NumAreas * NumAreas);
    return 0;
}

auto MCGlobalMap::Destroy() -> void
{
    if (SmallAreaMap != nullptr)
    {
        Blocks.Free(SmallAreaMap);
        SmallAreaMap = nullptr;
    }

    if (AreaMap != nullptr)
    {
        Blocks.Free(AreaMap);
        AreaMap = nullptr;
    }

    if (Areas != nullptr)
    {
        // Computed maps give each area its own door list; loaded ones point into doorInfos.
        if (DoorInfos == nullptr)
        {
            for (int32_t i = 0; i < NumAreas + 1; i++)
            {
                if (Areas[i].Doors != nullptr)
                {
                    Blocks.Free(Areas[i].Doors);
                    Areas[i].Doors = nullptr;
                }
            }
        }

        Blocks.Free(Areas);
        Areas = nullptr;
    }

    if (Doors != nullptr)
    {
        if (DoorLinks == nullptr)
        {
            for (int32_t i = 0; i < NumDoors + 2; i++)
            {
                for (int32_t side = 0; side < 2; side++)
                {
                    if (Doors[i].Links[side] != nullptr)
                    {
                        Blocks.Free(Doors[i].Links[side]);
                        Doors[i].Links[side] = nullptr;
                    }
                }
            }
        }

        Blocks.Free(Doors);
        Doors = nullptr;
    }

    if (DoorInfos != nullptr)
    {
        Blocks.Free(DoorInfos);
        DoorInfos = nullptr;
    }

    if (DoorLinks != nullptr)
    {
        Blocks.Free(DoorLinks);
        DoorLinks = nullptr;
    }

    if (PathCostTable != nullptr)
    {
        Blocks.Free(PathCostTable);
        PathCostTable = nullptr;
    }

    // Blocks nothing points to any more (a path cost table recomputed over an older one, an unused build list).
    Blocks.Clear();
}

auto MCGlobalMap::SetTempArea(int32_t tileR, int32_t tileC, int32_t) -> int32_t
{
    MCGlobalMapArea& area = Areas[NumAreas];
    area.NumDoors = 0;
    area.SectorR = static_cast<int16_t>(tileR / SectorDim);
    area.SectorC = static_cast<int16_t>(tileC / SectorDim);
    area.Open = 1;
    return NumAreas;
}

auto MCGlobalMap::FillNorthSouthBridgeArea(MCScenarioMap* map, int32_t row, int32_t col, int32_t area) -> int
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    AreaMap[row * Width + col] = static_cast<int16_t>(area);

    for (const int32_t next : {row - 1, row + 1})
    {
        if (InFillSector(next, col) && TileOverlay(map, next, col) == 0x25 && AreaMap[next * Width + col] == -1)
        {
            FillNorthSouthBridgeArea(map, next, col, area);
        }
    }

    return 1;
}

auto MCGlobalMap::FillEastWestBridgeArea(MCScenarioMap* map, int32_t row, int32_t col, int32_t area) -> int
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    AreaMap[row * Width + col] = static_cast<int16_t>(area);

    for (const int32_t next : {col + 1, col - 1})
    {
        if (InFillSector(row, next) && TileOverlay(map, row, next) == 0x27 && AreaMap[row * Width + next] == -1)
        {
            FillEastWestBridgeArea(map, row, next, area);
        }
    }

    return 1;
}

auto MCGlobalMap::FillNorthSouthRailroadBridgeArea(MCScenarioMap* map, int32_t row, int32_t col, int32_t area) -> int
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    AreaMap[row * Width + col] = static_cast<int16_t>(area);

    for (const int32_t next : {row - 1, row + 1})
    {
        if (InFillSector(next, col) && TileOverlay(map, next, col) == 0x37 && AreaMap[next * Width + col] == -1)
        {
            FillNorthSouthRailroadBridgeArea(map, next, col, area);
        }
    }

    return 1;
}

auto MCGlobalMap::FillEastWestRailroadBridgeArea(MCScenarioMap* map, int32_t row, int32_t col, int32_t area) -> int
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    AreaMap[row * Width + col] = static_cast<int16_t>(area);

    for (const int32_t next : {col + 1, col - 1})
    {
        if (InFillSector(row, next) && TileOverlay(map, row, next) == 0x39 && AreaMap[row * Width + next] == -1)
        {
            FillEastWestRailroadBridgeArea(map, row, next, area);
        }
    }

    return 1;
}

auto IsLRBlocked(MCMapTile* tile) -> int
{
    // Per cell (row-major), whether it is impassable.
    uint32_t blocked[9];

    for (int32_t cell = 0; cell < 9; cell++)
    {
        blocked[cell] = (~tile->Cells >> (14 + cell * 2)) & 1;
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

auto MCGlobalMap::FillArea(MCScenarioMap* map, int32_t row, int32_t col, int32_t area) -> int
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    const MCMapTile* mapTile = &map->Map[map->Width * row + col];
    const uint32_t overlay = mapTile->Overlay & 0x7f;

    if (overlay == 0x27 || overlay == 0x25 || overlay == 0x39 || overlay == 0x37)
    {
        return 0;
    }

    if (CurPlanet == 1 && OverlayIsDirtRoad[overlay] != 0)
    {
        return 0;
    }

    Assert(row >= 0 && row < map->Height && col >= 0 && col < map->Width ? 1 : 0, 0, " Map Tile out of bounds ");
    MCMapTile tile = *mapTile;
    const uint32_t terrain = tile.Cells & 0x7f;
    bool open = false;

    if ((tile.Cells & 0x55554000) != 0 && IsLRBlocked(&tile) == 0 && overlay != 0x3e && terrain != 0x2a &&
        terrain != 0x29)
    {
        open = BlockWallTiles == 0 || overlay != 0x3c;
    }

    if (!open)
    {
        AreaMap[Width * row + col] = -2;
        return 0;
    }

    AreaMap[Width * row + col] = static_cast<int16_t>(area);

    for (int32_t dir = 0; dir < 4; dir++)
    {
        const int32_t nextRow = AdjTile[dir][0] + row;
        const int32_t nextCol = AdjTile[dir][1] + col;

        if (InFillSector(nextRow, nextCol) && AreaMap[Width * nextRow + nextCol] == -1)
        {
            FillArea(map, nextRow, nextCol, area);
        }
    }

    return 1;
}

auto MCGlobalMap::CalcSectorAreas(MCScenarioMap* map, int32_t sectorR, int32_t sectorC) -> void
{
    MinTileR = SectorDim * sectorR;
    MaxTileR = SectorDim + MinTileR;
    MinTileC = SectorDim * sectorC;
    MaxTileC = SectorDim + MinTileC;

    for (int32_t row = MinTileR; row < MaxTileR; row++)
    {
        for (int32_t col = MinTileC; col < MaxTileC; col++)
        {
            if (AreaMap[Width * row + col] != -1)
            {
                continue;
            }

            int filled;

            switch (TileOverlay(map, row, col))
            {
                case 0x25:
                    filled = FillNorthSouthBridgeArea(map, row, col, NumAreas);
                    break;
                case 0x27:
                    filled = FillEastWestBridgeArea(map, row, col, NumAreas);
                    break;
                case 0x37:
                    filled = FillNorthSouthRailroadBridgeArea(map, row, col, NumAreas);
                    break;
                case 0x39:
                    filled = FillEastWestRailroadBridgeArea(map, row, col, NumAreas);
                    break;
                default:
                    filled = FillArea(map, row, col, NumAreas);
                    break;
            }

            if (filled != 0)
            {
                NumAreas++;
            }
        }
    }
}

auto MCGlobalMap::CalcAreas(MCScenarioMap* map) -> void
{
    for (int32_t sectorR = 0; sectorR < SectorHeight; sectorR++)
    {
        for (int32_t sectorC = 0; sectorC < SectorWidth; sectorC++)
        {
            CalcSectorAreas(map, sectorR, sectorC);
        }
    }

    if (NumAreas > 10000)
    {
        Fatal(0, " Too many GlobalMapAreas ");
    }

    // One spare area past the last, for setTempArea.
    const int32_t count = NumAreas + 1;
    Areas = static_cast<MCGlobalMapArea*>(Blocks.Allocate(static_cast<uint32_t>(count * sizeof(MCGlobalMapArea))));

    for (int32_t i = 0; i < count; i++)
    {
        MCGlobalMapArea& area = Areas[i];
        area.Type = 0;
        area.NumDoors = 0;
        area.Doors = nullptr;
        area.Open = 1;
        area.Closed = 0;
    }

    for (int32_t sectorR = 0; sectorR < SectorHeight; sectorR++)
    {
        MinTileR = SectorDim * sectorR;
        MaxTileR = SectorDim + MinTileR;

        for (int32_t sectorC = 0; sectorC < SectorWidth; sectorC++)
        {
            MinTileC = SectorDim * sectorC;
            MaxTileC = SectorDim + MinTileC;

            for (int32_t row = MinTileR; row < MaxTileR; row++)
            {
                for (int32_t col = MinTileC; col < MaxTileC; col++)
                {
                    const int16_t area = AreaMap[Width * row + col];

                    if (area >= 0)
                    {
                        Areas[area].SectorR = static_cast<int16_t>(sectorR);
                        Areas[area].SectorC = static_cast<int16_t>(sectorC);
                    }
                }
            }
        }
    }
}

auto MCGlobalMap::CalcBridges(MCScenarioMap* map) -> void
{
    for (int32_t row = 0; row < Height; row++)
    {
        for (int32_t col = 0; col < Width; col++)
        {
            const uint32_t overlay = map->Map[row * map->Width + col].Overlay & 0x7f;

            if (overlay == 0x25 || overlay == 0x37)
            {
                Areas[AreaMap[Width * row + col]].Type = 1;
            }
            else if (overlay == 0x27 || overlay == 0x39)
            {
                Areas[AreaMap[Width * row + col]].Type = 2;
            }
        }
    }
}

auto MCGlobalMap::BeginDoorProcessing() -> void
{
    DoorBuildList = static_cast<MCGlobalMapDoor*>(Blocks.Allocate(MAX_BUILD_DOORS * sizeof(MCGlobalMapDoor)));

    if (DoorBuildList == nullptr)
    {
        Fatal(0, " No RAM for Door Build List ");
    }
}

auto MCGlobalMap::AddDoor(int32_t area1, int32_t area2, int32_t row, int32_t col, int32_t cellR, int32_t cellC,
                          int32_t length, int32_t direction) -> void
{
    for (int32_t i = 0; i < NumDoors; i++)
    {
        const MCGlobalMapDoor& door = DoorBuildList[i];

        if (door.Row == row && door.Col == col && door.CellR == cellR && door.CellC == cellC && door.Length == length &&
            door.Direction[0] == direction)
        {
            return;
        }
    }

    // Port fix: the original writes past the build list once it holds MAX_BUILD_DOORS doors.
    if (NumDoors >= MAX_BUILD_DOORS - 2)
    {
        return;
    }

    MCGlobalMapDoor& door = DoorBuildList[NumDoors];
    door.Row = static_cast<int16_t>(row);
    door.Col = static_cast<int16_t>(col);
    door.CellR = static_cast<uint8_t>(cellR);
    door.CellC = static_cast<uint8_t>(cellC);
    door.Length = static_cast<char>(length);
    door.Open = 1;
    door.Area[0] = static_cast<int16_t>(area1);
    door.AreaCost[0] = 1;
    door.Direction[0] = static_cast<char>(direction);
    door.Area[1] = static_cast<int16_t>(area2);
    door.AreaCost[1] = 1;
    door.Direction[1] = static_cast<char>((direction + 2) % 4);
    NumDoors++;
}

auto MCGlobalMap::EndDoorProcessing() -> void
{
    if (DoorBuildList == nullptr)
    {
        return;
    }

    const uint32_t size = static_cast<uint32_t>((NumDoors + 2) * sizeof(MCGlobalMapDoor));
    Doors = static_cast<MCGlobalMapDoor*>(Blocks.Allocate(size));
    std::memcpy(Doors, DoorBuildList, size);
    Blocks.Free(DoorBuildList);
    DoorBuildList = nullptr;
}

auto MCGlobalMap::NumAreaDoors(int32_t area) -> int32_t
{
    int32_t count = 0;

    for (int32_t i = 0; i < NumDoors; i++)
    {
        if (Doors[i].Area[0] == area || Doors[i].Area[1] == area)
        {
            count++;
        }
    }

    return count;
}

auto MCGlobalMap::GetAreaDoors(int32_t area, MCDoorInfo* doorList) -> void
{
    for (int32_t i = 0; i < NumDoors; i++)
    {
        if (Doors[i].Area[0] == area || Doors[i].Area[1] == area)
        {
            doorList->DoorIndex = static_cast<int16_t>(i);
            doorList->DoorSide = Doors[i].Area[1] == area ? 1 : 0;
            doorList++;
        }
    }
}

auto MCGlobalMap::CalcGlobalDoors(MCScenarioMap* map) -> void
{
    BeginDoorProcessing();
    std::vector<int16_t> cellMap(DOOR_CELL_MAP_SIDE * DOOR_CELL_MAP_SIDE);

    for (int32_t sectorR = 0; sectorR < SectorHeight; sectorR++)
    {
        for (int32_t sectorC = 0; sectorC < SectorWidth; sectorC++)
        {
            // Direction 1 looks east, 2 south (adjTile), for the cells an area can cross into its neighbour by.
            for (int32_t dir = 1; dir < 3; dir++)
            {
                std::fill(cellMap.begin(), cellMap.end(), static_cast<int16_t>(-1));
                MinTileR = SectorDim * sectorR;
                MaxTileR = SectorDim + MinTileR;
                MinTileC = SectorDim * sectorC;
                MaxTileC = SectorDim + MinTileC;
                const int32_t minCellR = MinTileR * MAPCELL_DIM;
                const int32_t maxCellR = MaxTileR * MAPCELL_DIM;
                const int32_t minCellC = MinTileC * MAPCELL_DIM;
                const int32_t maxCellC = MaxTileC * MAPCELL_DIM;
                auto cell = [&](int32_t cellRow, int32_t cellCol) -> int16_t&
                { return cellMap[(cellRow - minCellR) * DOOR_CELL_MAP_SIDE + (cellCol - minCellC)]; };

                for (int32_t row = MinTileR; row < MaxTileR; row++)
                {
                    for (int32_t col = MinTileC; col < MaxTileC; col++)
                    {
                        const int32_t area = AreaMap[Width * row + col];

                        if (area < 0)
                        {
                            continue;
                        }

                        const int32_t nextRow = AdjTile[dir][0] + row;
                        const int32_t nextCol = AdjTile[dir][1] + col;

                        if (nextRow < 0 || nextRow >= Height || nextCol < 0 || nextCol >= Width)
                        {
                            continue;
                        }

                        const int16_t nextArea = AreaMap[Width * nextRow + nextCol];

                        if (nextArea < 0 || area == nextArea)
                        {
                            continue;
                        }

                        // Bridges only join areas along their own direction.
                        const int32_t type = Areas[area].Type;
                        const int32_t nextType = Areas[nextArea].Type;
                        const int32_t crossType = dir == 1 ? 2 : 1;

                        if ((type != 0 && type != crossType) || (nextType != 0 && nextType != crossType))
                        {
                            continue;
                        }

                        Assert(row >= 0 && row < map->Height && col >= 0 && col < map->Width ? 1 : 0, 0,
                               " Map Tile out of bounds ");
                        const MCMapTile tile = map->Map[map->Width * row + col];
                        Assert(nextRow >= 0 && nextRow < map->Height && nextCol >= 0 && nextCol < map->Width ? 1 : 0, 0,
                               " Map Tile out of bounds ");
                        const MCMapTile nextTile = map->Map[map->Width * nextRow + nextCol];
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
                            const int32_t area = AreaMap[(cellRow / 3) * Width + tileC];
                            int32_t length = 0;

                            while (cellRow < maxCellR && AreaMap[(cellRow / 3) * Width + tileC] == area &&
                                   cell(cellRow, cellCol) == nextArea)
                            {
                                length++;
                                cellRow++;
                            }

                            AddDoor(area, nextArea, (cellRow - length) / 3, tileC, (cellRow - length) % 3, cellCol % 3,
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
                            const int32_t area = AreaMap[cellCol / 3 + Width * tileR];
                            int32_t length = 0;

                            while (cellCol < maxCellC && AreaMap[cellCol / 3 + Width * tileR] == area &&
                                   cell(cellRow, cellCol) == nextArea)
                            {
                                length++;
                                cellCol++;
                            }

                            AddDoor(area, nextArea, tileR, (cellCol - length) / 3, cellRow % 3, (cellCol - length) % 3,
                                    length, dir);
                        }
                    }
                }
            }
        }
    }

    EndDoorProcessing();
}

auto MCGlobalMap::CalcAreaDoors() -> void
{
    NumDoorInfos = 0;

    for (int32_t i = 0; i < NumAreas; i++)
    {
        MCGlobalMapArea& area = Areas[i];
        area.NumDoors = static_cast<char>(NumAreaDoors(i));
        NumDoorInfos += area.NumDoors;

        if (area.NumDoors == 0)
        {
            area.Doors = nullptr;
            continue;
        }

        area.Doors = static_cast<MCDoorInfo*>(Blocks.Allocate(static_cast<uint32_t>(area.NumDoors * 3)));
        GetAreaDoors(i, area.Doors);
    }
}

auto MCGlobalMap::CalcLinkCost(int32_t startDoor, int32_t thruArea, int32_t goalDoor) -> int32_t
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
        const MCGlobalMapDoor& door = Doors[doorIndex];

        if (door.Area[0] != thruArea && door.Area[1] != thruArea)
        {
            return false;
        }

        const int32_t side = door.Area[1] == thruArea ? 1 : 0;
        int32_t rowOffset;
        int32_t colOffset;

        if (door.Direction[0] == 1)
        {
            colOffset = side;
            rowOffset = door.Length / 2;
        }
        else
        {
            colOffset = door.Length / 2;
            rowOffset = side;
        }

        cellCol = door.Col * 3 + door.CellC + colOffset;
        cellRow = door.CellR + door.Row * 3 + rowOffset;
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

    MCVector3D goalPos;
    const double mapHalf = static_cast<double>(WorldUnitsMapSide) * 0.5f;
    goalPos.X = static_cast<float>((static_cast<double>(goalCol) + 0.5) * MetersPerCell - mapHalf);
    goalPos.Y = static_cast<float>((mapHalf - static_cast<double>(goalRow) * MetersPerCell) -
                                   static_cast<double>(MetersPerCell) * 0.5);
    goalPos.Z = 0.0f;

    if (PathFindMap == nullptr)
    {
        Fatal(0, " No PathFindMap ");
    }

    MCMovePath path;
    path.Goal.Zero();
    path.NumSteps = 0;
    path.NumStepsWhenNotPaused = 0;
    path.CurStep = 0;
    path.Cost = 0;
    path.Marked = 0;
    path.GlobalStep = -1;
    const int32_t uLr = Areas[thruArea].SectorR * SectorDim;
    const int32_t uLc = Areas[thruArea].SectorC * SectorDim;
    ClearBridgeTiles = 1;
    PathFindMap->SetUp(GameMap, uLr, uLc, SectorDim, SectorDim, nullptr, (startRow / 3 - uLr) * 3 + startRow % 3,
                       (startCol / 3 - uLc) * 3 + startCol % 3, goalPos, (goalRow / 3 - uLr) * 3 + goalRow % 3,
                       (goalCol / 3 - uLc) * 3 + goalCol % 3, nullptr, 10, 0, 8, 0);
    int32_t goalCell[2] = {};
    PathFindMap->CalcPath(&path, nullptr, goalCell);
    ClearBridgeTiles = 0;

    if (path.NumSteps == 0)
    {
        path.Destroy();
        return 9999;
    }

    const int32_t cost = path.Cost;
    path.Destroy();
    return cost;
}

auto MCGlobalMap::CalcDoorLinks() -> void
{
    int32_t maxAreaDoors = 0;
    NumDoorLinks = 0;

    for (int32_t doorIndex = 0; doorIndex < NumDoors; doorIndex++)
    {
        MCGlobalMapDoor& door = Doors[doorIndex];

        for (int32_t side = 0; side < 2; side++)
        {
            door.NumLinks[side] = 0;
            door.Links[side] = nullptr;
            const int32_t area = door.Area[side];
            const int32_t areaDoors = Areas[area].NumDoors;
            door.NumLinks[side] = static_cast<char>(areaDoors - 1);
            door.Links[side] =
                static_cast<MCDoorLink*>(Blocks.Allocate(static_cast<uint32_t>((door.NumLinks[side] + 2) * 7)));
            NumDoorLinks += door.NumLinks[side] + 2;

            if (door.Links[side] == nullptr)
            {
                Fatal(0, " Coud not malloc systemHeap door link ");
            }

            MCDoorLink* link = door.Links[side];

            for (int32_t i = 0; i < areaDoors; i++)
            {
                const int16_t otherIndex = Areas[area].Doors[i].DoorIndex;

                if (otherIndex == doorIndex)
                {
                    continue;
                }

                link->DoorIndex = otherIndex;
                link->DoorSide = Doors[otherIndex].Area[1] == area ? 1 : 0;
                link->Cost = CalcLinkCost(doorIndex, area, otherIndex);
                link++;
            }

            if (maxAreaDoors < areaDoors)
            {
                maxAreaDoors = areaDoors;
            }
        }
    }

    // The temporary start and goal doors link out of any area, so they get room for the most doors an area has.
    for (int32_t doorIndex = NumDoors; doorIndex < NumDoors + 2; doorIndex++)
    {
        MCGlobalMapDoor& door = Doors[doorIndex];
        door.NumLinks[0] = static_cast<char>(maxAreaDoors);
        NumDoorLinks += door.NumLinks[0] + 2;
        door.Links[0] = static_cast<MCDoorLink*>(Blocks.Allocate(static_cast<uint32_t>((door.NumLinks[0] + 2) * 7)));
        door.NumLinks[1] = 0;
        NumDoorLinks += door.NumLinks[1] + 2;
        door.Links[1] = static_cast<MCDoorLink*>(Blocks.Allocate(static_cast<uint32_t>((door.NumLinks[1] + 2) * 7)));
    }
}

auto MCGlobalMap::CalcSectorPaths(MCScenarioMap*, int32_t, int32_t) -> void
{
}

auto MCGlobalMap::CalcPathCostTable() -> void
{
    PathCostTable = static_cast<uint8_t*>(Blocks.Allocate(static_cast<uint32_t>(NumAreas * NumAreas)));
    Assert(PathCostTable != nullptr ? 1 : 0, 0, " GlobalMap.calcPathCostTable: unable to malloc pathCostTable ");
    MCGlobalPathStep path[MAX_GLOBAL_PATH];

    for (int32_t startArea = 0; startArea < NumAreas; startArea++)
    {
        for (int32_t goalArea = 0; goalArea < NumAreas; goalArea++)
        {
            if (startArea == goalArea)
            {
                PathCostTable[NumAreas * startArea + goalArea] = 0;
            }
            else
            {
                PathCostTable[startArea * NumAreas + goalArea] =
                    static_cast<uint8_t>(CalcPath(startArea, goalArea, path));
            }
        }
    }
}

auto MCGlobalMap::ExitDirection(int32_t doorIndex, int32_t fromArea) -> int32_t
{
    const MCGlobalMapDoor& door = Doors[doorIndex];

    if (door.Area[0] == fromArea)
    {
        return door.Direction[0];
    }

    if (door.Area[1] == fromArea)
    {
        return door.Direction[1];
    }

    return -1;
}

auto MCGlobalMap::GetDoorTiles(int32_t area, int32_t doorIndex, MCGlobalMapDoor* door) -> void
{
    *door = Doors[Areas[area].Doors[doorIndex].DoorIndex];
}

auto MCGlobalMap::GetDoorWorldPos(int32_t, int32_t, int32_t* prevGoalCell) -> MCVector3D
{
    const double mapHalf = static_cast<double>(WorldUnitsMapSide) * 0.5f;
    const float x = static_cast<float>((static_cast<double>(prevGoalCell[1]) + 0.5) * MetersPerCell - mapHalf);
    const float y = static_cast<float>((mapHalf - static_cast<double>(prevGoalCell[0]) * MetersPerCell) -
                                       static_cast<double>(MetersPerCell) * 0.5);
    const float z = GameMap->GetTerrainElevation(MCVector3D(x, y, 0.0f));
    return MCVector3D(x, y, z);
}

namespace
{
    /// <summary>Makes a temporary door joining <paramref name="areaIndex"/> to itself (the start or goal door).</summary>
    /// <remarks>The field setup shared by GlobalMap::setStartDoor and setGoalDoor (inlined in both).</remarks>
    void InitTempDoor(MCGlobalMapDoor& door, int32_t areaIndex, char numLinks)
    {
        door.Direction[0] = -1;
        door.Direction[1] = -1;
        door.Area[0] = static_cast<int16_t>(areaIndex);
        door.Area[1] = static_cast<int16_t>(areaIndex);
        door.Row = 0;
        door.Col = 0;
        door.CellR = 0;
        door.CellC = 0;
        door.Length = 0;
        door.Open = 1;
        door.AreaCost[0] = 1;
        door.AreaCost[1] = 1;
        door.NumLinks[0] = numLinks;
        door.NumLinks[1] = 0;
    }
}

auto MCGlobalMap::SetStartDoor(int32_t startArea) -> void
{
    MCGlobalMapDoor& startDoor = Doors[NumDoors];
    const MCGlobalMapArea& area = Areas[startArea];
    InitTempDoor(startDoor, startArea, area.NumDoors);
    startDoor.FromAreaIndex = 1;

    for (int32_t i = 0; i < startDoor.NumLinks[0]; i++)
    {
        const MCDoorInfo& info = area.Doors[i];
        MCDoorLink& link = startDoor.Links[0][i];
        link.DoorIndex = info.DoorIndex;
        link.DoorSide = info.DoorSide;
        link.Cost = 1;
        // Links the area's door back to the start door, in the spare room past its links.
        MCGlobalMapDoor& areaDoor = Doors[info.DoorIndex];
        const int32_t side = info.DoorSide;
        MCDoorLink& backLink = areaDoor.Links[side][areaDoor.NumLinks[side]];
        backLink.DoorIndex = static_cast<int16_t>(NumDoors);
        backLink.DoorSide = 0;
        backLink.Cost = 1;
        areaDoor.NumLinks[side]++;
    }
}

auto MCGlobalMap::ResetStartDoor(int32_t startArea) -> void
{
    const MCGlobalMapDoor& startDoor = Doors[NumDoors];

    for (int32_t i = 0; i < startDoor.NumLinks[0]; i++)
    {
        const MCDoorInfo& info = Areas[startArea].Doors[i];
        Doors[info.DoorIndex].NumLinks[static_cast<int32_t>(info.DoorSide)]--;
    }
}

auto MCGlobalMap::SetGoalDoor(int32_t goalArea) -> void
{
    if (goalArea < 0 || goalArea >= NumAreas)
    {
        char message[256];
        std::snprintf(message, sizeof(message), " GlobalMap.setGoalDoor: bad goalArea (%d of %d) ", goalArea, NumAreas);
        Fatal(0, message);
    }

    MCGlobalMapDoor& goalDoor = Doors[NumDoors + 1];
    const MCGlobalMapArea& area = Areas[goalArea];
    goalDoor.Area[0] = static_cast<int16_t>(goalArea);
    goalDoor.Area[1] = static_cast<int16_t>(goalArea);
    GoalSectorR = area.SectorR;
    GoalSectorC = area.SectorC;
    InitTempDoor(goalDoor, goalArea, area.NumDoors);

    for (int32_t i = 0; i < goalDoor.NumLinks[0]; i++)
    {
        const MCDoorInfo& info = area.Doors[i];
        MCDoorLink& link = goalDoor.Links[0][i];
        link.DoorIndex = info.DoorIndex;
        link.DoorSide = info.DoorSide;
        link.Cost = 1;
        Assert(info.DoorIndex >= 0 && info.DoorIndex < NumDoors + 2 ? 1 : 0, static_cast<uint32_t>(info.DoorIndex),
               " GlobalMap.setGoalDoor: bad doorIndex ");
        MCGlobalMapDoor& areaDoor = Doors[info.DoorIndex];
        const int32_t side = info.DoorSide;
        MCDoorLink& backLink = areaDoor.Links[side][areaDoor.NumLinks[side]];
        backLink.DoorIndex = static_cast<int16_t>(NumDoors + 1);
        backLink.DoorSide = 0;
        backLink.Cost = 1;
        areaDoor.NumLinks[side]++;
    }
}

auto MCGlobalMap::ResetGoalDoor(int32_t goalArea) -> void
{
    const MCGlobalMapDoor& goalDoor = Doors[NumDoors + 1];

    for (int32_t i = 0; i < goalDoor.NumLinks[0]; i++)
    {
        const MCDoorInfo& info = Areas[goalArea].Doors[i];
        Doors[info.DoorIndex].NumLinks[static_cast<int32_t>(info.DoorSide)]--;
    }
}

auto MCGlobalMap::CalcHPrime(int32_t door) -> int32_t
{
    Assert(door >= 0 && door < NumDoors + 2 ? 1 : 0, 0xffffffff, " CalcHPrime: Bad Door ");
    const MCGlobalMapArea& area0 = Areas[Doors[door].Area[0]];
    const MCGlobalMapArea& area1 = Areas[Doors[door].Area[1]];
    const int32_t sectorR = (area1.SectorR + area0.SectorR) / 2;
    const int32_t sectorC = (area0.SectorC + area1.SectorC) / 2;
    const int32_t rowDistance = GoalSectorR < sectorR ? sectorR - GoalSectorR : GoalSectorR - sectorR;
    const int32_t colDistance = GoalSectorC < sectorC ? sectorC - GoalSectorC : GoalSectorC - sectorC;
    return rowDistance + colDistance;
}

auto MCGlobalMap::CalcPath(int32_t startArea, int32_t goalArea, MCGlobalPathStep* path) -> int32_t
{
    if (startArea == -1 || goalArea == -1)
    {
        return -1;
    }

    if (OpenList == nullptr)
    {
        OpenList = new MCPriorityQueue(5000, -2000000);
    }

    const int32_t startDoor = NumDoors;
    const int32_t goalDoor = NumDoors + 1;

    for (int32_t i = 0; i < NumDoors + 2; i++)
    {
        MCGlobalMapDoor& door = Doors[i];
        door.Cost = 1;
        door.Parent = -1;
        door.FromAreaIndex = -1;
        door.Flags = 0;
        door.G = 0;
        door.HPrime = -1;
        door.FPrime = 0;
    }

    SetStartDoor(startArea);
    SetGoalDoor(goalArea);

    OpenList->Clear();
    MCPQNode startNode = {};
    startNode.Key = 0;
    startNode.Id = startDoor;

    if (!OpenList->Insert(startNode))
    {
        Fatal(0, "PathFind OPEN overflow");
    }

    Doors[startDoor].Flags |= 1;

    int goalFound = 0;

    while (OpenList->Size() != 0)
    {
        const MCPQNode best = OpenList->Pop();
        const int32_t curIndex = best.Id;
        MCGlobalMapDoor& current = Doors[curIndex];
        const int32_t g = current.G;
        current.Flags = (current.Flags & ~1u) | 2;

        if (curIndex == goalDoor)
        {
            goalFound = 1;
            break;
        }

        const int32_t side = 1 - current.FromAreaIndex;
        const int32_t thruArea = current.Area[side];
        const int32_t numLinks = current.NumLinks[side];

        for (int32_t i = 0; i < numLinks; i++)
        {
            const MCDoorLink& link = current.Links[side][i];
            const int32_t succIndex = link.DoorIndex;
            Assert(succIndex >= 0 && succIndex < NumDoors + 2 ? 1 : 0, 0, " Bad Door Index ");
            const int32_t linkCost = link.Cost;
            MCGlobalMapDoor& successor = Doors[succIndex];

            if (successor.Open == 0 || linkCost >= 10000)
            {
                continue;
            }

            if (successor.HPrime == -1)
            {
                successor.HPrime = CalcHPrime(succIndex);
            }

            const int32_t newG = g + linkCost;
            const int32_t succSide = successor.Area[1] == thruArea ? 1 : 0;

            if ((successor.Flags & 1) == 0)
            {
                if ((successor.Flags & 2) == 0)
                {
                    successor.FromAreaIndex = succSide;
                    successor.Parent = curIndex;
                    successor.G = newG;
                    successor.FPrime = newG + successor.HPrime;
                    successor.Cost = linkCost;
                    MCPQNode node = {};
                    node.Key = successor.FPrime;
                    node.Id = succIndex;

                    if (!OpenList->Insert(node))
                    {
                        Fatal(0, "PathFind OPEN overflow");
                    }

                    successor.Flags |= 1;
                }
                else if (newG < successor.G)
                {
                    // A cheaper way to a closed door: reparent it and push the saving on.
                    successor.Cost = linkCost;
                    successor.Parent = curIndex;
                    successor.FromAreaIndex = succSide;
                    PropogateCost(succIndex, linkCost, succSide, g);
                }
            }
            else if (newG < successor.G)
            {
                successor.FromAreaIndex = succSide;
                successor.Cost = linkCost;
                successor.FPrime = successor.HPrime + newG;
                successor.Parent = curIndex;
                successor.G = newG;
                const int32_t itemIndex = OpenList->Find(succIndex);

                if (itemIndex == 0)
                {
                    char message[256];
                    std::snprintf(message, sizeof(message),
                                  "GlobalMap.calcPath: Cannot find globalmap door [%d, %d, %d, %d] for change\n",
                                  succIndex, i, succSide, linkCost);
                    DebugOpenList(message);
                    Fatal(0, "GlobalMap.calcPath: Save OPENLIST.DBG file for Glenn!");
                }

                OpenList->Change(itemIndex, successor.FPrime);
            }
        }
    }

    ResetStartDoor(startArea);
    ResetGoalDoor(goalArea);

    if (goalFound == 0)
    {
        return 0;
    }

    int32_t count = 1;

    for (int32_t door = goalDoor; door != startDoor; door = Doors[door].Parent)
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
            path[i].ThruArea = Doors[door].Area[Doors[door].FromAreaIndex];
            path[i].GoalDoor = door;
            path[i].CostToGoal = costToGoal;
        }

        costToGoal += Doors[door].Cost;
        door = Doors[door].Parent;
    }

    if (PathCostTable != nullptr)
    {
        uint8_t& entry = PathCostTable[NumAreas * startArea + goalArea];

        if (entry != numSteps)
        {
            entry = count > 0xff ? 0xff : static_cast<uint8_t>(numSteps);
        }
    }

    return numSteps;
}

auto MCGlobalMap::PropogateCost(int32_t door, int32_t cost, int32_t fromSide, int32_t g) -> void
{
    Assert(door >= 0 && door < NumDoors + 2 && (fromSide == 0 || fromSide == 1) && g >= 0 ? 1 : 0, 0xffffffff,
           " Bad Door Propogate ");
    const int32_t newG = cost + g;
    MCGlobalMapDoor& current = Doors[door];

    if (newG >= current.G)
    {
        return;
    }

    current.G = newG;
    current.FPrime = current.HPrime + newG;

    if ((current.Flags & 1) != 0)
    {
        if (OpenList->Find(door) == 0)
        {
            char message[256];
            std::snprintf(message, sizeof(message),
                          "GlobalMap.propogateCost: Cannot find globalmap door [%d, %d, %d, %d] for change\n", door,
                          cost, fromSide, g);
            DebugOpenList(message);
            Fatal(0, "GlobalMap.propogateCost: Save OPENLIST.DBG file for Glenn!");
        }

        // Original behaviour (OB-025): passes the door number where PriorityQueue::change wants the heap index.
        OpenList->Change(door, current.FPrime);
        return;
    }

    const int32_t side = 1 - fromSide;
    const int32_t numLinks = current.NumLinks[side];

    for (int32_t i = 0; i < numLinks; i++)
    {
        const MCDoorLink& link = current.Links[side][i];
        const int32_t nextIndex = link.DoorIndex;
        Assert(nextIndex >= 0 && nextIndex < NumDoors + 2 ? 1 : 0, 0, " Bad Door Index ");
        const int32_t linkCost = link.Cost;
        MCGlobalMapDoor& next = Doors[nextIndex];
        const int32_t nextSide = next.Area[1] == current.Area[side] ? 1 : 0;

        if (next.Open == 0 || linkCost >= 10000 || next.HPrime == -1)
        {
            continue;
        }

        if (door == next.Parent)
        {
            // Original behaviour (OB-026): passes this door's exit side, not the next door's entry side.
            PropogateCost(nextIndex, linkCost, side, current.G);
        }
        else if (current.G + linkCost < next.G)
        {
            next.Cost = linkCost;
            next.Parent = door;
            next.FromAreaIndex = nextSide;
            PropogateCost(nextIndex, linkCost, nextSide, current.G);
        }
    }
}

auto MCGlobalMap::CalcPath(MCVector3D start, MCVector3D goal, MCGlobalPathStep* path) -> int32_t
{
    int32_t startR = 0;
    int32_t startC = 0;
    GameMap->WorldToMapTilePos(start, startR, startC);
    int32_t goalR = 0;
    int32_t goalC = 0;
    GameMap->WorldToMapTilePos(goal, goalR, goalC);
    const int32_t goalArea = CalcArea(goalR, goalC);
    const int32_t startArea = CalcArea(startR, startC);
    return CalcPath(startArea, goalArea, path);
}

auto MCGlobalMap::GetPathCost(int32_t startArea, int32_t goalArea) -> int32_t
{
    if (startArea < 0 || goalArea < 0)
    {
        return 0;
    }

    return PathCostTable[NumAreas * startArea + goalArea];
}

auto MCGlobalMap::OpenDoor(int32_t door) -> void
{
    Doors[door].Open = 1;
}

auto MCGlobalMap::CloseDoor(int32_t door) -> void
{
    Doors[door].Open = 0;
}

auto MCGlobalMap::CloseArea(int32_t area) -> void
{
    MCGlobalMapArea& closing = Areas[area];
    closing.Closed = 1;

    for (int32_t i = 0; i < closing.NumDoors; i++)
    {
        CloseDoor(closing.Doors[i].DoorIndex);
    }

    for (int32_t i = 0; i < NumAreas; i++)
    {
        PathCostTable[NumAreas * i + area] = 0;
        PathCostTable[NumAreas * area + i] = 0;
    }
}

auto MCGlobalMap::Print(char* fileName, int32_t uLr, int32_t uLc, int32_t printHeight, int32_t printWidth) -> void
{
    // Port fix: the original tests the other way round (it prints only when areaMap is null, and then reads through
    // the null pointer).
    if (AreaMap == nullptr)
    {
        return;
    }

    MCFile* debugFile = new MCFile;
    debugFile->Create(fileName);
    char line[512];
    std::snprintf(line, sizeof(line), "ULr: %d, ULc: %d, h: %d, w: %d\n", uLr, uLc, printHeight, printWidth);
    debugFile->WriteString(line);

    for (int32_t row = uLr; row < uLr + printHeight; row++)
    {
        line[0] = '\0';

        for (int32_t col = uLc; col < uLc + printWidth; col++)
        {
            const int16_t area = AreaMap[Width * row + col];
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
        debugFile->WriteString(line);
    }

    debugFile->WriteString("\n");
    debugFile->Close();
    delete debugFile;
}

auto MCGlobalMap::CalcArea(int32_t tileR, int32_t tileC) -> int32_t
{
    // Port fix: the original reads outside the area map for a goal off the map. Off the map is in no area.
    if (tileR < 0 || tileR >= Height || tileC < 0 || tileC >= Width)
    {
        return -1;
    }

    if (SmallAreaMap == nullptr)
    {
        const int32_t area = AreaMap[Width * tileR + tileC];
        return area < 0 ? -1 : area;
    }

    const int32_t area = SmallAreaMap[Width * tileR + tileC];
    return area == 0xff ? -1 : area;
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
    bool IsBlockingMover(MCGameObject* object)
    {
        return object->ObjectClass != ELEMENTAL && object != MovingObject && object != RamObject &&
               object->IsDisabled() == 0;
    }

    /// <summary>MoveMap::markGoalCells's per-cell "free" state of the goal door (a function static in MCX).</summary>
    char DoorCellState[256];

    /// <summary>Whether cellShiftDistance has been filled (once per search function in MCX).</summary>
    /// <remarks>and 0x00808028 (calcEscapePath).</remarks>
    int CellShiftDistanceReady[2] = {};
}

auto MCMoveMap::Init(int32_t newMaxWidth, int32_t newMaxHeight) -> void
{
    MaxWidth = newMaxWidth;
    Width = newMaxWidth;
    MaxHeight = newMaxHeight;
    MaxCellHeight = newMaxHeight * MAPCELL_DIM;
    CellHeight = newMaxHeight * MAPCELL_DIM;
    Height = newMaxHeight;
    MaxCellWidth = newMaxWidth * MAPCELL_DIM;
    CellWidth = newMaxWidth * MAPCELL_DIM;
    Map = std::make_unique<MCMoveMapNode[]>(static_cast<size_t>(MaxCellHeight * MaxCellWidth));
    Clear();
}

auto MCMoveMap::Init(MCFitIniFile* mapFile) -> int32_t
{
    int32_t result = mapFile->SeekBlock("Header");

    if (result != 0)
    {
        return result;
    }

    char fileType[128];
    result = mapFile->ReadIdString("FileType", fileType, 127);

    if (result != 0)
    {
        return result;
    }

    if (std::strcmp(fileType, "MoveMap") != 0)
    {
        return -1;
    }

    result = mapFile->SeekBlock("MapData");

    if (result != 0)
    {
        return result;
    }

    result = mapFile->ReadIdLong("Height", Height);

    if (result != 0)
    {
        return result;
    }

    result = mapFile->ReadIdLong("Width", Width);

    if (result != 0)
    {
        return result;
    }

    Init(Height, Width); // as the original: the height goes to init's width slot (maps are square)
    const int32_t tileRows = Height;
    const int32_t tileCols = Width;
    std::vector<char> vertexCost(static_cast<size_t>(tileRows * tileCols));
    result = mapFile->ReadIdCharArray("VertexCost", vertexCost.data(), static_cast<uint32_t>(tileRows * tileCols));

    if (result != 0)
    {
        return result;
    }

    for (int32_t row = 0; row < Height; row++)
    {
        for (int32_t col = 0; col < Width; col++)
        {
            for (int32_t cellR = 0; cellR < MAPCELL_DIM; cellR++)
            {
                for (int32_t cellC = 0; cellC < MAPCELL_DIM; cellC++)
                {
                    Map[CellWidth * (row * 3 + cellR) + col * 3 + cellC].Cost = vertexCost[col + Width * row];
                }
            }
        }
    }

    return 0;
}

auto MCMoveMap::Clear() -> void
{
    const int32_t numCells = CellHeight * MaxCellWidth;

    for (int32_t i = 0; i < numCells; i++)
    {
        Map[i].Parent = -1;
        Map[i].Flags = 0;
        Map[i].HPrime = -1;
    }

    GoalPos = MCVector3D(0.0f, 0.0f, 0.0f);
    Target = MCVector3D(NO_POSITION, NO_POSITION, NO_POSITION);
}

auto MCMoveMap::PlaceMovers(int) -> void
{
    auto placeList = [this](MCObjectQueueNode* list, int bridgeCost)
    {
        MCBaseObject* current = nullptr;

        while (list->Traverse(current) != nullptr)
        {
            MCGameObject* object = static_cast<MCGameObject*>(current);

            if (!IsBlockingMover(object))
            {
                continue;
            }

            const MCObjectPosition* position = object->GetObjPosition();
            const int32_t r = (position->TileR - ULr) * 3 + position->CellR;
            const int32_t c = (position->TileC - ULc) * 3 + position->CellC;

            if (r < 0 || r >= CellHeight || c < 0 || c >= CellWidth || (r == StartR && c == StartC))
            {
                continue;
            }

            MCMoveMapNode& node = Map[MaxCellWidth * r + c];

            if ((node.Flags & 8) != 0)
            {
                continue;
            }

            MCMovePath* path = object->GetPilot()->GetMovePath();

            if (path == nullptr || path->NumSteps != 0)
            {
                continue;
            }

            node.Flags |= 0x10;
            const uint32_t overlay = GameMap->Map[GameMap->Width * position->TileR + position->TileC].Overlay & 0x7f;
            node.Cost = AddCost(node.Cost, OverlayIsBridge[overlay] == 0 ? 20000 : bridgeCost);
        }
    };

    // Original behaviour (OB-030): Inner Sphere mechs standing on a bridge cost 3333 to pass, Clan ones nothing extra.
    placeList(InnerSphereMechList, 0xd05);
    placeList(ClanMechList, 0);
}

auto MCMoveMap::SetTarget(MCVector3D targetPos) -> void
{
    Target = targetPos;
}

auto MCMoveMap::SetStart(MCVector3D* newStartPos, int32_t newStartR, int32_t newStartC) -> void
{
    if (newStartPos == nullptr)
    {
        StartPos = MCVector3D(NO_POSITION, NO_POSITION, NO_POSITION);
    }
    else
    {
        StartPos = *newStartPos;
    }

    if (newStartR == -1)
    {
        int32_t tileR = 0;
        int32_t tileC = 0;
        int32_t cellR = 0;
        int32_t cellC = 0;
        GameMap->WorldToMapPos(*newStartPos, tileR, tileC, cellR, cellC);
        StartR = (tileR - ULr) * 3 + cellR;
        StartC = (tileC - ULc) * 3 + cellC;
        return;
    }

    StartR = newStartR;
    StartC = newStartC;
}

auto MCMoveMap::SetGoal(MCVector3D newGoalPos, int32_t newGoalR, int32_t newGoalC) -> void
{
    GoalPos = newGoalPos;

    if (newGoalR == -1)
    {
        int32_t tileR = 0;
        int32_t tileC = 0;
        int32_t cellR = 0;
        int32_t cellC = 0;
        GameMap->WorldToMapPos(GoalPos, tileR, tileC, cellR, cellC);
        GoalR = (tileR - ULr) * 3 + cellR;
        GoalC = (tileC - ULc) * 3 + cellC;
    }
    else
    {
        GoalR = newGoalR;
        GoalC = newGoalC;
    }

    DoorDirection = -1;
    GoalIsDoor = 0;
}

auto MCMoveMap::SetGoal(int32_t thruArea, int32_t goalDoor) -> void
{
    GoalPos = MCVector3D(NO_POSITION, NO_POSITION, NO_POSITION);
    Door = goalDoor;
    GoalIsDoor = 1;
    // Per door direction (1 east-west, 2 north-south) and side: the direction the door is entered from.
    static constexpr int32_t entryDirection[8] = {-1, -1, 1, 3, 2, 0, -1, -1};
    const MCGlobalMapDoor& goal = GlobalMoveMap->Doors[goalDoor];
    DoorSide = goal.Area[1] == thruArea ? 1 : 0;
    const int32_t direction = goal.Direction[0];
    Assert(direction == 1 || direction == 2 ? 1 : 0, 0, " MoveMap: Bad Area Door Direction in setGoal() ");
    DoorDirection = entryDirection[DoorSide + direction * 2];

    if (DoorDirection == 0 || DoorDirection == 2)
    {
        GoalR = goal.Row * 3 + DoorSide + (goal.CellR - MinRow);
        GoalC = ((goal.Col * 3 + goal.CellC) - MinCol) + goal.Length / 2;
    }
    else if (DoorDirection == 1 || DoorDirection == 3)
    {
        GoalR = (goal.CellR - MinRow) + goal.Row * 3 + goal.Length / 2;
        GoalC = ((goal.Col * 3 + goal.CellC) - MinCol) + DoorSide;
    }
}

auto CellFacing(MCGameObject* object) -> int32_t
{
    if (object == nullptr)
    {
        return 0;
    }

    MCVector3D ahead = object->GetPosition();
    ahead.Y = static_cast<float>(static_cast<double>(ahead.Y) + 50.0);
    const float facing = object->RelFacingTo(ahead, -1);

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

auto MCMoveMap::SetUp(MCScenarioMap* scenarioMap, int32_t newULr, int32_t newULc, int32_t newHeight, int32_t newWidth,
                      MCVector3D* newStartPos, int32_t newStartR, int32_t newStartC, MCVector3D newGoalPos,
                      int32_t newGoalR, int32_t newGoalC, int32_t* newOverlayWeightTable, int32_t newMoveLevel,
                      int32_t newJumpCost, int32_t newNumOffsets, uint32_t params) -> int32_t
{
    if (Map == nullptr)
    {
        Init(newHeight, newWidth); // as the original: the height goes to init's width slot (windows are square)
    }
    else
    {
        Width = newWidth;
        Height = newHeight;
        CellWidth = newWidth * 3;
        CellHeight = newHeight * 3;
        Clear();
    }

    ULr = newULr;
    ULc = newULc;
    MinCol = newULc * 3;
    MinRow = newULr * 3;
    OverlayWeights = newOverlayWeightTable == nullptr ? OverlayWeightTable : newOverlayWeightTable;
    MoveLevel = newMoveLevel;
    JumpCost = newJumpCost;
    NumOffsets = newNumOffsets;
    SetStart(newStartPos, newStartR, newStartC);
    SetGoal(newGoalPos, newGoalR, newGoalC);

    if (ClearBridgeTiles != 0)
    {
        AdjustBridgeWeights(OverlayWeights, -10000);
    }

    int checkMines = 1;
    const int32_t lockCost = MoveLevel << 3;

    if (MovingObject != nullptr && MovingObject->ObjectClass == GROUNDVEHICLE &&
        static_cast<MCGroundVehicle*>(MovingObject)->MineSweeper != 0)
    {
        checkMines = 0;
    }

    for (int32_t row = 0; row < Height; row++)
    {
        for (int32_t col = 0; col < Width; col++)
        {
            const int32_t tileR = ULr + row;
            const int32_t tileC = ULc + col;

            if (tileR < 0 || tileR >= GameMap->Height || tileC < 0 || tileC >= GameMap->Width)
            {
                continue;
            }

            Assert(tileR >= 0 && tileR < scenarioMap->Height && tileC >= 0 && tileC < scenarioMap->Width ? 1 : 0, 0,
                   " Map Tile out of bounds ");
            const MCMapTile tile = scenarioMap->Map[scenarioMap->Width * tileR + tileC];
            MCMoveMapNode* tileNodes = &Map[MaxCellWidth * row * 3 + col * 3];
            auto node = [&](int32_t cellR, int32_t cellC) -> MCMoveMapNode&
            { return tileNodes[MaxCellWidth * cellR + cellC]; };

            for (int32_t cellR = 0; cellR < MAPCELL_DIM; cellR++)
            {
                for (int32_t cellC = 0; cellC < MAPCELL_DIM; cellC++)
                {
                    node(cellR, cellC).Cost = TileCellPassable(tile, cellR, cellC) != 0 ? MoveLevel : 10000;
                }
            }

            const uint32_t overlay = tile.Overlay & 0x7f;

            if (overlay != 0)
            {
                int32_t weightOverlay = static_cast<int32_t>(overlay);

                if (overlay >= FIRST_GATE_OVERLAY && overlay <= LAST_GATE_OVERLAY)
                {
                    weightOverlay = GateOverlay(overlay, MovingObject->GetAlignment());
                }

                for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++)
                {
                    const int32_t weight =
                        weightOverlay == -1 ? 20000 : OverlayWeights[OverlayWeightIndex[weightOverlay] + cell];
                    node(cell / 3, cell % 3).Cost = AddCost(node(cell / 3, cell % 3).Cost, weight);
                }
            }

            uint32_t locks = (tile.Overlay >> 15) & 0x1ff;

            if (locks != 0 && (params & 0x80) != 0)
            {
                for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++, locks >>= 1)
                {
                    if ((locks & 1) != 0)
                    {
                        node(cell / 3, cell % 3).Cost = AddCost(node(cell / 3, cell % 3).Cost, lockCost);
                    }
                }
            }

            if (checkMines != 0)
            {
                const int32_t alignment = MovingObject->GetAlignment();
                const uint32_t knownMines = (tile.Overlay >> (alignment == -1 ? 0x19 : 0x1b)) & 3;

                if (knownMines != 0)
                {
                    for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++)
                    {
                        node(cell / 3, cell % 3).Cost = AddCost(node(cell / 3, cell % 3).Cost, MoveLevel << knownMines);
                    }
                }

                const uint32_t ownMines = (tile.Overlay >> (alignment == -1 ? 0xb : 0xd)) & 3;

                if (ownMines == 3 || ownMines == 1)
                {
                    for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++)
                    {
                        node(cell / 3, cell % 3).Cost =
                            AddCost(node(cell / 3, cell % 3).Cost, (MoveLevel << ownMines) * -2);
                    }
                }
            }

            // A mine layer laying mines is drawn to the middle of each tile.
            if (MovingObject->ObjectClass == GROUNDVEHICLE)
            {
                MCGroundVehicle* vehicle = static_cast<MCGroundVehicle*>(MovingObject);

                if (vehicle->MineLayer != 0 && vehicle->Pilot->CurTacOrder.MoveParams.Mode == 1)
                {
                    node(1, 1).Cost = AddCost(node(1, 1).Cost, MoveLevel * -16);
                }
            }
        }
    }

    if (FindingEscapePath == 0)
    {
        // Port fix: the original marks a goal outside the window too, writing outside map, before searchPath stops
        // on it (" Bad Move Goal "). The port leaves the mark out so that Fatal is what reports it.
        if (GoalR >= 0 && GoalR < CellHeight && GoalC >= 0 && GoalC < CellWidth)
        {
            Map[GoalR * MaxCellWidth + GoalC].Flags |= 8;
        }
    }
    else
    {
        MarkEscapeGoalCells(newGoalPos);
    }

    if ((params & 0x40) != 0)
    {
        PlaceMovers(1);
    }

    return 0;
}

auto MCMoveMap::MarkEscapeGoalCells(MCVector3D escapeGoal) -> int32_t
{
    int32_t goalTileR = 0;
    int32_t goalTileC = 0;
    int32_t goalCellR = 0;
    int32_t goalCellC = 0;
    GameMap->WorldToMapPos(escapeGoal, goalTileR, goalTileC, goalCellR, goalCellC);
    const int32_t goalArea = GlobalMoveMap->CalcArea(goalTileR, goalTileC);

    for (int32_t row = 0; row < Height; row++)
    {
        for (int32_t col = 0; col < Width; col++)
        {
            const int32_t tileR = ULr + row;
            const int32_t tileC = ULc + col;

            if (tileR < 0 || tileR >= GameMap->Height || tileC < 0 || tileC >= GameMap->Width)
            {
                continue;
            }

            const int32_t area = GlobalMoveMap->CalcArea(tileR, tileC);
            const int32_t cost = GlobalMoveMap->GetPathCost(area, goalArea);

            if (area != goalArea && cost <= 0)
            {
                continue;
            }

            for (int32_t cellR = 0; cellR < MAPCELL_DIM; cellR++)
            {
                for (int32_t cellC = 0; cellC < MAPCELL_DIM; cellC++)
                {
                    Map[MaxCellWidth * (row * 3 + cellR) + col * 3 + cellC].Flags |= 8;
                }
            }
        }
    }

    return 0;
}

auto MCMoveMap::SetUp(MCScenarioMap* scenarioMap, int32_t newULr, int32_t newULc, int32_t newHeight, int32_t newWidth,
                      MCVector3D* newStartPos, int32_t newStartR, int32_t newStartC, int32_t thruArea, int32_t goalDoor,
                      MCVector3D targetPos, int32_t* newOverlayWeightTable, int32_t newMoveLevel, int32_t newJumpCost,
                      int32_t newNumOffsets, uint32_t params) -> int32_t
{
    if (Map == nullptr)
    {
        Init(newHeight, newWidth); // as the original: the height goes to init's width slot (windows are square)
    }
    else
    {
        Width = newWidth;
        Height = newHeight;
        CellWidth = newWidth * 3;
        CellHeight = newHeight * 3;
        Clear();
    }

    ULr = newULr;
    ULc = newULc;
    MinCol = newULc * 3;
    MinRow = newULr * 3;
    OverlayWeights = newOverlayWeightTable == nullptr ? OverlayWeightTable : newOverlayWeightTable;
    MoveLevel = newMoveLevel;
    JumpCost = newJumpCost;
    NumOffsets = newNumOffsets;
    SetStart(newStartPos, newStartR, newStartC);
    SetGoal(thruArea, goalDoor);

    if (ClearBridgeTiles != 0)
    {
        AdjustBridgeWeights(OverlayWeights, -10000);
    }

    for (int32_t row = 0; row < Height; row++)
    {
        for (int32_t col = 0; col < Width; col++)
        {
            const int32_t tileR = ULr + row;
            const int32_t tileC = ULc + col;

            if (tileR < 0 || tileR >= GameMap->Height || tileC < 0 || tileC >= GameMap->Width)
            {
                continue;
            }

            Assert(tileR >= 0 && tileR < scenarioMap->Height && tileC >= 0 && tileC < scenarioMap->Width ? 1 : 0, 0,
                   " Map Tile out of bounds ");
            const MCMapTile tile = scenarioMap->Map[scenarioMap->Width * tileR + tileC];
            MCMoveMapNode* tileNodes = &Map[MaxCellWidth * row * 3 + col * 3];
            auto node = [&](int32_t cellR, int32_t cellC) -> MCMoveMapNode&
            { return tileNodes[MaxCellWidth * cellR + cellC]; };

            for (int32_t cellR = 0; cellR < MAPCELL_DIM; cellR++)
            {
                for (int32_t cellC = 0; cellC < MAPCELL_DIM; cellC++)
                {
                    node(cellR, cellC).Cost = TileCellPassable(tile, cellR, cellC) != 0 ? MoveLevel : 10000;
                }
            }

            const uint32_t overlay = tile.Overlay & 0x7f;

            if (overlay != 0)
            {
                int32_t weightOverlay = static_cast<int32_t>(overlay);

                if (overlay >= FIRST_GATE_OVERLAY && overlay <= LAST_GATE_OVERLAY)
                {
                    weightOverlay = GateOverlay(overlay, MovingObject->GetAlignment());
                }

                for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++)
                {
                    const int32_t weight =
                        weightOverlay == -1 ? 20000 : OverlayWeights[OverlayWeightIndex[weightOverlay] + cell];
                    node(cell / 3, cell % 3).Cost = AddCost(node(cell / 3, cell % 3).Cost, weight);
                }
            }

            // Unlike the other setUp: a lock costs moveLevel (not 8 x), and only the known mines count.
            uint32_t locks = (tile.Overlay >> 15) & 0x1ff;

            if (locks != 0 && (params & 0x80) != 0)
            {
                for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++, locks >>= 1)
                {
                    if ((locks & 1) != 0)
                    {
                        node(cell / 3, cell % 3).Cost = AddCost(node(cell / 3, cell % 3).Cost, MoveLevel);
                    }
                }
            }

            const uint32_t knownMines = (tile.Overlay >> (MovingObject->GetAlignment() == -1 ? 0x19 : 0x1b)) & 3;

            for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++)
            {
                if (knownMines != 0)
                {
                    node(cell / 3, cell % 3).Cost = AddCost(node(cell / 3, cell % 3).Cost, MoveLevel << knownMines);
                }
            }
        }
    }

    if (MarkGoalCells(targetPos) == 0)
    {
        return -1;
    }

    if ((params & 0x40) != 0)
    {
        PlaceMovers(1);
    }

    return 0;
}

auto MCMoveMap::MarkGoalCells(MCVector3D targetPos) -> int32_t
{
    const MCGlobalMapDoor& goal = GlobalMoveMap->Doors[Door];
    const int32_t length = goal.Length;

    if (length > 0)
    {
        std::memset(DoorCellState, 1, static_cast<size_t>(length));
    }

    Assert(Door >= 0 && Door < GlobalMoveMap->NumDoors ? 1 : 0, 0, " FUDGE 1");
    Assert(goal.Direction[0] == 1 || goal.Direction[0] == 2 ? 1 : 0, 0, " FUDGE 2");
    Assert(goal.Length >= 1 && goal.Length <= 0x3ff ? 1 : 0, 0, " FUDGE 3");

    // Clears the door cells a standing mech occupies (on either side of the door).
    const bool alongRows = goal.Direction[0] == 1;
    const int32_t doorRow = TileMulMapcellDim[goal.Row] + goal.CellR;
    const int32_t doorCol = TileMulMapcellDim[goal.Col] + goal.CellC;
    auto clearOccupied = [&](MCObjectQueueNode* list)
    {
        MCBaseObject* current = nullptr;

        while (list->Traverse(current) != nullptr)
        {
            MCGameObject* object = static_cast<MCGameObject*>(current);

            if (!IsBlockingMover(object))
            {
                continue;
            }

            const MCObjectPosition* position = object->GetObjPosition();
            int32_t index;

            if (alongRows)
            {
                if (position->MapCellR < doorRow || position->MapCellR >= length + doorRow ||
                    position->MapCellC < doorCol || position->MapCellC >= doorCol + 2)
                {
                    continue;
                }

                index = position->MapCellR - doorRow;
            }
            else
            {
                if (position->MapCellR < doorRow || position->MapCellR >= doorRow + 2 || position->MapCellC < doorCol ||
                    position->MapCellC >= length + doorCol)
                {
                    continue;
                }

                index = position->MapCellC - doorCol;
            }

            Assert(index >= 0 && index < length ? 1 : 0, 0, " Bad Cell Index ");
            DoorCellState[index] = 0;
        }
    };

    clearOccupied(InnerSphereMechList);
    clearOccupied(ClanMechList);

    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    GameMap->WorldToMapPos(targetPos, tileR, tileC, cellR, cellC);
    const int32_t targetR = cellR + (tileR * 3 - MinRow);
    const int32_t targetC = cellC + (tileC * 3 - MinCol);
    int32_t numMarked = 0;
    const int32_t half = length / 2;
    const int32_t step = MoveLevel / 2;

    if (DoorDirection == 0 || DoorDirection == 2)
    {
        // The door runs along goal row goalR; the target just past it is the only goal.
        const int32_t firstCol = GoalC - half;
        const int32_t beyondRow = DoorSide == 0 ? GoalR + 1 : GoalR - 1;

        if (targetR == beyondRow && targetC >= firstCol && targetC < firstCol + length)
        {
            Map[MaxCellWidth * GoalR + targetC].Flags |= 8;
            return 1;
        }

        // Else every free door cell is a goal, costing more the further from the middle.
        for (int32_t i = 0; i < length; i++)
        {
            if (DoorCellState[i] == 0)
            {
                continue;
            }

            MCMoveMapNode& node = Map[MaxCellWidth * GoalR + i + firstCol];
            node.Flags |= 8;
            numMarked++;
            node.Cost = AddCost(node.Cost, (i < half ? half - i : i - half) * step);
        }
    }
    else if (DoorDirection == 1 || DoorDirection == 3)
    {
        const int32_t firstRow = GoalR - half;

        if (targetC == GoalC + DoorSide * -2 + 1 && targetR >= firstRow && targetR < length + firstRow)
        {
            Map[MaxCellWidth * targetR + GoalC].Flags |= 8;
            return 1;
        }

        for (int32_t i = 0; i < length; i++)
        {
            if (DoorCellState[i] == 0)
            {
                continue;
            }

            MCMoveMapNode& node = Map[MaxCellWidth * (firstRow + i) + GoalC];
            node.Flags |= 8;
            numMarked++;
            node.Cost = AddCost(node.Cost, (i < half ? half - i : i - half) * step);
        }
    }

    return numMarked;
}

auto MCMoveMap::AdjacentCellOpen(int32_t r, int32_t c, int32_t dir) -> int
{
    const int32_t nextR = CellShift[dir * 2] + r;
    const int32_t nextC = CellShift[dir * 2 + 1] + c;

    if (nextR < 0 || nextR >= CellHeight || nextC < 0 || nextC >= CellWidth)
    {
        return 0;
    }

    const MCMoveMapNode& node = Map[MaxCellWidth * nextR + nextC];

    if ((node.Flags & 0x10) != 0)
    {
        return 0;
    }

    // Original behaviour (OB-027): the known-mine bits are read from the map tile at the window cell's coordinates.
    const uint32_t mineBits = MovingObject->GetAlignment() == -1 ? 0x6000000u : 0x18000000u;
    Assert(nextR >= 0 && nextR < GameMap->Height && nextC >= 0 && nextC < GameMap->Width ? 1 : 0, 0,
           " Map Tile out of bounds ");

    if ((GameMap->Map[GameMap->Width * nextR + nextC].Overlay & mineBits) != 0)
    {
        return 0;
    }

    return node.Cost < 10000 ? 1 : 0;
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

auto MCMoveMap::PropogateCost(int32_t r, int32_t c, int32_t cost, int32_t g) -> void
{
    Assert(cost > 0 ? 1 : 0, 0, " MoveMap.propogateCost: bad cost ");

    if (g < 0)
    {
        Fatal(0, "Negative g-cost in MoveMap");
    }

    MCMoveMapNode& current = Map[MaxCellWidth * r + c];
    const int32_t newG = g + cost;

    if (newG >= current.G)
    {
        return;
    }

    // Original behaviour (OB-028): the cell's own cost is replaced by the step cost (diagonal and jump extras
    // included), so later searches through it see the inflated cost.
    current.Cost = cost;
    current.G = newG;
    current.FPrime = current.HPrime + newG;

    if ((current.Flags & 1) != 0)
    {
        const int32_t id = CellId(r, c);
        const int32_t itemIndex = OpenList->Find(id);

        if (itemIndex != 0)
        {
            OpenList->Change(itemIndex, current.FPrime);
            return;
        }

        char message[256];
        std::snprintf(message, sizeof(message),
                      "MoveMap.propogateCost: Cannot find movemap node [%d, %d, %d] for change\n", r, c, id);
        DebugOpenList(message);
        return;
    }

    for (int32_t i = 0; i < NumOffsets; i++)
    {
        if (IsDiagonalStep[i] != 0 && AdjacentCellOpen(r, c, StepAdjDir[i]) == 0 &&
            AdjacentCellOpen(r, c, StepAdjDir[i + 1]) == 0)
        {
            continue;
        }

        const int32_t nextR = r + CellShift[i * 2];
        const int32_t nextC = c + CellShift[i * 2 + 1];

        if (nextR < 0 || nextR >= CellHeight || nextC < 0 || nextC >= CellWidth)
        {
            continue;
        }

        MCMoveMapNode& next = Map[MaxCellWidth * nextR + nextC];

        if (next.Cost >= 10000 || next.HPrime == -1 || next.HPrime >= MaxHPrime)
        {
            continue;
        }

        const int32_t dir = ReverseShift[i];
        Assert(next.Cost > 0 ? 1 : 0, 0, " MoveMap.propogateCost: bad cost 1");
        const int32_t stepCost = StepCost(next.Cost, i, JumpCost);

        if (dir != next.Parent)
        {
            if (next.G <= current.G + stepCost)
            {
                continue;
            }

            next.Parent = dir;
        }

        PropogateCost(nextR, nextC, stepCost, current.G);
    }
}

auto MCMoveMap::SearchPath(MCMovePath* path, MCVector3D* goalWorldPos, int32_t* goalCell, bool escape) -> int32_t
{
    if (escape)
    {
        MaxHPrime = 500;
    }
    else
    {
        if (GoalR < 0 || GoalR >= CellHeight || GoalC < 0 || GoalC >= CellWidth)
        {
            const double mapHalf = static_cast<double>(WorldUnitsMapSide) * 0.5f;
            const float x = static_cast<float>((static_cast<double>(GoalC) + 0.5) * MetersPerCell - mapHalf);
            const float y = static_cast<float>((mapHalf - static_cast<double>(GoalR) * MetersPerCell) -
                                               static_cast<double>(MetersPerCell) * 0.5);
            char message[256];
            std::snprintf(message, sizeof(message), " Bad Move Goal: %d [%d(%d), %d(%d)], (%.2f, %.2f, %.2f)",
                          DebugMovePathType, GoalR, CellHeight, GoalC, CellWidth, static_cast<double>(x),
                          static_cast<double>(y), 0.0);
            Fatal(0, message);
        }

        const int32_t distance = std::abs(GoalR - StartR) + std::abs(GoalC - StartC);
        MaxHPrime = static_cast<int32_t>(std::floor(static_cast<double>(distance) * 2.5));

        if (MaxHPrime < 500)
        {
            MaxHPrime = 500;
        }
    }

    if (OpenList == nullptr)
    {
        OpenList = new MCPriorityQueue(5000, -2000000);
    }

    MCMoveMapNode& start = Map[MaxCellWidth * StartR + StartC];
    start.G = 0;
    const int32_t startH = escape ? 10 : std::abs(GoalR - StartR) + std::abs(GoalC - StartC);
    start.HPrime = startH;
    start.FPrime = startH;
    OpenList->Clear();
    MCPQNode startNode;
    startNode.Key = startH;
    startNode.Id = CellId(StartR, StartC);
    startNode.Row = StartR;
    startNode.Col = StartC;

    if (!OpenList->Insert(startNode))
    {
        Fatal(0, "PathFind OPEN overflow");
    }

    start.Flags |= 1;

    int32_t bestR = -1;
    int32_t bestC = -1;
    int goalFound = 0;

    while (OpenList->Size() != 0)
    {
        const MCPQNode best = OpenList->Pop();
        bestR = best.Row;
        bestC = best.Col;
        MCMoveMapNode& current = Map[MaxCellWidth * bestR + bestC];
        const int32_t g = current.G;
        const uint32_t flags = current.Flags;
        current.Flags = (flags & ~1u) | 2;

        if ((flags & 8) != 0)
        {
            goalFound = 1;
            break;
        }

        for (int32_t i = 0; i < NumOffsets; i++)
        {
            if (IsDiagonalStep[i] != 0 && AdjacentCellOpen(bestR, bestC, StepAdjDir[i]) == 0 &&
                AdjacentCellOpen(bestR, bestC, StepAdjDir[i + 1]) == 0)
            {
                continue;
            }

            const int32_t nextR = CellShift[i * 2] + bestR;
            const int32_t nextC = CellShift[i * 2 + 1] + bestC;

            if (nextR < 0 || nextR >= CellHeight || nextC < 0 || nextC >= CellWidth)
            {
                continue;
            }

            MCMoveMapNode& next = Map[MaxCellWidth * nextR + nextC];

            if (next.Cost >= 10000)
            {
                continue;
            }

            if (next.HPrime == -1)
            {
                next.HPrime = escape ? 10 : std::abs(GoalR - nextR) + std::abs(GoalC - nextC);
            }

            if (next.HPrime >= MaxHPrime)
            {
                continue;
            }

            const int32_t dir = ReverseShift[i];
            const int32_t stepCost = StepCost(next.Cost, i, JumpCost);
            Assert(stepCost > 0 ? 1 : 0, 0, " MoveMap.propogateCost: bad cost 3");
            const int32_t newG = stepCost + g;

            if ((next.Flags & 1) == 0)
            {
                if ((next.Flags & 2) == 0)
                {
                    next.Parent = dir;
                    next.G = newG;
                    next.FPrime = newG + next.HPrime;
                    MCPQNode node;
                    node.Key = next.FPrime;
                    node.Id = CellId(nextR, nextC);
                    node.Row = nextR;
                    node.Col = nextC;

                    if (!OpenList->Insert(node))
                    {
                        Fatal(0, "PathFind OPEN overflow");
                    }

                    next.Flags |= 1;
                }
                else if (newG < next.G)
                {
                    next.Parent = dir;
                    PropogateCost(nextR, nextC, stepCost, g);
                }
            }
            else if (newG < next.G)
            {
                next.Parent = dir;
                next.G = newG;
                next.FPrime = next.HPrime + newG;
                const int32_t id = CellId(nextR, nextC);
                const int32_t itemIndex = OpenList->Find(id);

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
                    OpenList->Change(itemIndex, next.FPrime);
                }
            }
        }
    }

    if (ClearBridgeTiles != 0)
    {
        AdjustBridgeWeights(OverlayWeights, 10000);
    }

    if (goalFound == 0)
    {
        return 0;
    }

    goalCell[0] = bestR;
    goalCell[1] = bestC;
    int32_t count = 0;

    for (int32_t r = bestR, c = bestC; r != StartR || c != StartC;)
    {
        count++;
        const int32_t parent = Map[MaxCellWidth * r + c].Parent;
        r += CellShift[parent * 2];
        c += CellShift[parent * 2 + 1];
    }

    if (DoorDirection != -1)
    {
        count++;
    }

    path->Goal.Zero();
    path->NumSteps = 0;
    path->NumStepsWhenNotPaused = 0;
    path->CurStep = 0;
    path->Cost = 0;
    path->Marked = 0;
    path->GlobalStep = -1;

    if (count == 0)
    {
        return path->NumSteps;
    }

    // Port fix: the original writes past stepList when the path is longer than MAX_STEPS_PER_MOVEPATH.
    if (count > MAX_STEPS_PER_MOVEPATH)
    {
        return path->NumSteps;
    }

    path->Init(count);
    path->Target = Target;
    path->Cost = Map[MaxCellWidth * bestR + bestC].G;
    int32_t stepIndex = count;

    if (DoorDirection == -1)
    {
        if (goalWorldPos == nullptr)
        {
            path->Goal = GoalPos;
        }
        else
        {
            goalWorldPos->Z = 0.0f;
            path->Goal.Z = 0.0f;
            const double mapHalf = static_cast<double>(WorldUnitsMapSide * 0.5f);
            const float x = static_cast<float>((static_cast<double>(MinCol + bestC) + 0.5) * MetersPerCell - mapHalf);
            const float y = static_cast<float>((mapHalf - static_cast<double>(MinRow + bestR) * MetersPerCell) -
                                               static_cast<double>(MetersPerCell) * 0.5);
            goalWorldPos->X = x;
            goalWorldPos->Y = y;
            path->Goal.X = x;
            path->Goal.Y = y;
        }
    }
    else
    {
        // The last step goes through the door, into the next area.
        stepIndex = count - 1;
        MCPathStep& doorStep = path->StepList[stepIndex];
        doorStep.Direction = static_cast<uint8_t>(static_cast<char>(DoorDirection) << 1);
        const int32_t doorRow = AdjTile[DoorDirection][0] + MinRow + bestR;
        const int32_t doorCol = MinCol + AdjTile[DoorDirection][1] + bestC;
        goalCell[0] = doorRow;
        goalCell[1] = doorCol;
        const double mapHalf = static_cast<double>(WorldUnitsMapSide) * 0.5f;
        const float x = static_cast<float>((static_cast<double>(doorCol) + 0.5) * MetersPerCell - mapHalf);
        const float y = static_cast<float>((mapHalf - static_cast<double>(doorRow) * MetersPerCell) -
                                           static_cast<double>(MetersPerCell) * 0.5);
        path->SetDestination(stepIndex, MCVector3D(x, y, 0.0f));
        doorStep.DistanceToGoal = 0.0f;
        doorStep.TileR = static_cast<int16_t>(doorRow / 3);
        doorStep.TileC = static_cast<int16_t>(doorCol / 3);
        doorStep.CellR = static_cast<int16_t>(doorRow - doorStep.TileR * 3);
        doorStep.CellC = static_cast<int16_t>(doorCol - doorStep.TileC * 3);
        path->Goal = MCVector3D(x, y, 0.0f);

        if (goalWorldPos != nullptr)
        {
            *goalWorldPos = MCVector3D(x, y, 0.0f);
        }
    }

    int& ready = CellShiftDistanceReady[escape ? 1 : 0];

    if (ready == 0)
    {
        const double scale = static_cast<double>(MetersPerWorldUnit) * MCTerrain::MetersPerVertexDivMapcellDim;

        for (int32_t i = 0; i < NUM_CELL_OFFSETS; i++)
        {
            const double dr = static_cast<double>(CellShift[i * 2]);
            const double dc = static_cast<double>(CellShift[i * 2 + 1]);
            CellShiftDistance[i] = static_cast<float>(std::sqrt(dr * dr + dc * dc) * scale);
        }

        ready = 1;
    }

    // Walks back from the goal cell, filling the steps from the end.
    for (int32_t r = bestR, c = bestC; r != StartR || c != StartC;)
    {
        stepIndex--;
        MCMoveMapNode& node = Map[MaxCellWidth * r + c];
        MCPathStep& step = path->StepList[stepIndex];
        step.Direction = static_cast<uint8_t>(ReverseShift[node.Parent]);
        const int32_t mapRow = MinRow + r;
        const int32_t mapCol = MinCol + c;
        const double mapHalf = static_cast<double>(WorldUnitsMapSide * 0.5f);
        const float x = static_cast<float>((static_cast<double>(mapCol) + 0.5) * MetersPerCell - mapHalf);
        const float y = static_cast<float>((mapHalf - static_cast<double>(mapRow) * MetersPerCell) -
                                           static_cast<double>(MetersPerCell) * 0.5);

        if (stepIndex == count - 1 && static_cast<int8_t>(step.Direction) < 8)
        {
            step.DistanceToGoal = 0.0f;
        }
        else
        {
            // As the original: for a final jump step this reads the (unused) step past the end.
            const MCPathStep& nextStep = path->StepList[stepIndex + 1];
            step.DistanceToGoal = nextStep.DistanceToGoal + CellShiftDistance[static_cast<int8_t>(nextStep.Direction)];
        }

        path->SetDestination(stepIndex, MCVector3D(x, y, 0.0f));
        step.TileR = static_cast<int16_t>(mapRow / 3);
        step.TileC = static_cast<int16_t>(mapCol / 3);
        step.CellR = static_cast<int16_t>(mapRow - step.TileR * 3);
        step.CellC = static_cast<int16_t>(mapCol - step.TileC * 3);
        node.Flags |= 4;
        r += CellShift[node.Parent * 2];
        c += CellShift[node.Parent * 2 + 1];
    }

    return path->NumSteps;
}

auto MCMoveMap::CalcPath(MCMovePath* path, MCVector3D* goalWorldPos, int32_t* goalCell) -> int32_t
{
    return SearchPath(path, goalWorldPos, goalCell, false);
}

auto MCMoveMap::CalcEscapePath(MCMovePath* path, MCVector3D* goalWorldPos, int32_t* goalCell) -> int32_t
{
    return SearchPath(path, goalWorldPos, goalCell, true);
}

auto MCMoveMap::WriteDebug(MCFile* debugFile) -> void
{
    char line[512];
    std::snprintf(line, sizeof(line), "Time = %.6f\n\n", static_cast<double>(CalcTime));
    debugFile->WriteString(line);
    std::snprintf(line, sizeof(line), "Start = (%d, %d)\n", StartR, StartC);
    debugFile->WriteString(line);
    std::snprintf(line, sizeof(line), "Goal = (%d, %d)\n", GoalR, GoalC);
    debugFile->WriteString(line);
    std::strcpy(line, "\n");
    debugFile->WriteString(line);

    char cell[16] = {};
    debugFile->WriteString("PARENT:\n");
    debugFile->WriteString("-------\n");

    for (int32_t r = 0; r < CellHeight; r++)
    {
        line[0] = '\0';

        for (int32_t c = 0; c < CellWidth; c++)
        {
            const MCMoveMapNode& node = Map[MaxCellWidth * r + c];

            if (StartR == r && StartC == c)
            {
                std::strcpy(cell, "S");
            }
            else if (node.Parent == -1)
            {
                std::strcpy(cell, ".");
            }
            else if ((node.Flags & 4) == 0)
            {
                std::snprintf(cell, sizeof(cell), "%d", node.Parent);
            }
            else
            {
                std::strcpy(cell, "X");
            }

            std::strcat(line, cell);
        }

        std::strcat(line, "\n");
        debugFile->WriteString(line);
    }

    debugFile->WriteString("\n");
    debugFile->WriteString("MAP:\n");
    debugFile->WriteString("-------\n");

    for (int32_t r = 0; r < CellHeight; r++)
    {
        line[0] = '\0';

        for (int32_t c = 0; c < CellWidth; c++)
        {
            const int32_t cost = Map[MaxCellWidth * r + c].Cost;

            if (GoalR == r && GoalC == c)
            {
                std::strcpy(cell, "G");
            }
            else if (StartR == r && StartC == c)
            {
                std::strcpy(cell, "S");
            }
            else if (cost == MoveLevel)
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
        debugFile->WriteString(line);
    }

    debugFile->WriteString("\n");
    debugFile->WriteString("PATH:\n");
    debugFile->WriteString("-------\n");

    for (int32_t r = 0; r < CellHeight; r++)
    {
        line[0] = '\0';

        for (int32_t c = 0; c < CellWidth; c++)
        {
            const MCMoveMapNode& node = Map[MaxCellWidth * r + c];

            if (GoalR == r && GoalC == c)
            {
                std::strcpy(cell, "G");
            }
            else if (StartR == r && StartC == c)
            {
                std::strcpy(cell, "S");
            }
            else if ((node.Flags & 4) != 0)
            {
                std::strcpy(cell, "*");
            }
            else if (node.Cost == MoveLevel)
            {
                std::strcpy(cell, ".");
            }
            else if (node.Cost >= 10000)
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
        debugFile->WriteString(line);
    }

    debugFile->WriteString("\n");
}

auto MCMoveMap::Destroy() -> void
{
    Map.reset();
}
