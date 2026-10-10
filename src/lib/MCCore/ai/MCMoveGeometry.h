#pragma once

#include "lib/MCFrameOfRef.h"
#include "main/MCMissionGlobals.h"
#include "terrain/MCTerrain.h"

class MCGameObject;

// The movement maps' geometry: a terrain tile ("map tile") is split into MapCellDim x MapCellDim movement cells.

/// <summary>Movement cells a map tile side holds.</summary>
inline constexpr int32_t MapCellDim = 3;
/// <summary>Neighbour offsets the cell path finder can step to (<see cref="CellShift"/>): 8 neighbours, then jumps.</summary>
inline constexpr int32_t NumCellOffsets = 104;
/// <summary>Terrain overlay types (the low 7 bits of a tile's overlay word).</summary>
inline constexpr int32_t NumOverlayTypes = 75;
/// <summary>Movement levels of <see cref="OverlayWeightTable"/> (gamesys.fit's OverlayCellCosts).</summary>
inline constexpr int32_t NumMoveLevels = 5;
/// <summary>Entries per movement level of <see cref="OverlayWeightTable"/>: a 3x3 cell cost block per overlay.</summary>
inline constexpr int32_t OverlayWeightLevelSize = NumOverlayTypes * MapCellDim * MapCellDim;

/// <summary>The row offset of each search offset.</summary>
inline constexpr std::array<int32_t, NumCellOffsets> CellShiftRow = []
{
    std::array<int32_t, NumCellOffsets> rows{};
    // Offset i: ring i / 8, direction i % 8 clockwise from north. Rings 0..2 step 1..3 cells, ring 3 holds the
    // knight moves, rings 4..12 step 4..12 cells.
    constexpr int32_t unitRow[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    constexpr int32_t knightRow[8] = {-3, -2, 2, 3, 3, 2, -2, -3};

    for (int32_t i = 0; i < NumCellOffsets; i++)
    {
        const int32_t ring = i / 8;
        const int32_t dir = i % 8;

        if (ring == 3)
        {
            rows[i] = knightRow[dir];
        }
        else
        {
            rows[i] = unitRow[dir] * (ring < 3 ? ring + 1 : ring);
        }
    }

    return rows;
}();

/// <summary>The column offset of each search offset.</summary>
inline constexpr std::array<int32_t, NumCellOffsets> CellShiftCol = []
{
    std::array<int32_t, NumCellOffsets> cols{};
    constexpr int32_t unitCol[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    constexpr int32_t knightCol[8] = {2, 3, 3, 2, -2, -3, -3, -2};

    for (int32_t i = 0; i < NumCellOffsets; i++)
    {
        const int32_t ring = i / 8;
        const int32_t dir = i % 8;

        if (ring == 3)
        {
            cols[i] = knightCol[dir];
        }
        else
        {
            cols[i] = unitCol[dir] * (ring < 3 ? ring + 1 : ring);
        }
    }

    return cols;
}();

/// <summary>The offset that steps back: the same ring, the opposite direction.</summary>
inline constexpr std::array<int32_t, NumCellOffsets> ReverseShift = []
{
    std::array<int32_t, NumCellOffsets> reverse{};

    for (int32_t i = 0; i < NumCellOffsets; i++)
    {
        reverse[i] = (i / 8) * 8 + (i % 8 + 4) % 8;
    }

    return reverse;
}();

/// <summary>Whether an offset is a diagonal step (only the eight neighbours count; jumps are never diagonal).</summary>
constexpr bool IsDiagonalStep(int32_t offset)
{
    return offset < 8 && offset % 2 == 1;
}

/// <summary>
/// Per diagonal neighbour d (1, 3, 5, 7), the two side neighbours [d] and [d + 1] whose openness lets the step cut the
/// corner.
/// </summary>
inline constexpr std::array<int32_t, 9> StepAdjDir = {-1, 0, 2, 2, 4, 4, 6, 6, 0};

/// <summary>The tile row/column offset of the four sides (north, east, south, west).</summary>
inline constexpr std::array<std::array<int32_t, 2>, 4> AdjTile = {{{-1, 0}, {0, 1}, {1, 0}, {0, -1}}};

/// <summary>The cell row/column offset of the eight neighbours, clockwise from north (for SpreadState).</summary>
inline constexpr std::array<int32_t, 8> RowShift = {-1, -1, 0, 1, 1, 1, 0, -1};
inline constexpr std::array<int32_t, 8> ColShift = {0, 1, 1, 1, 0, -1, -1, -1};

/// <summary>
/// Per cell (cellR * 3 + cellC) and direction: the neighbour's tile row and column delta, then its cell row and
/// column.
/// </summary>
inline constexpr std::array<std::array<std::array<int32_t, 4>, 8>, MapCellDim * MapCellDim> AdjCellTable = {{
    {{{-1, 0, 2, 0},
      {-1, 0, 2, 1},
      {0, 0, 0, 1},
      {0, 0, 1, 1},
      {0, 0, 1, 0},
      {0, -1, 1, 2},
      {0, -1, 0, 2},
      {-1, -1, 2, 2}}},
    {{{-1, 0, 2, 1},
      {-1, 0, 2, 2},
      {0, 0, 0, 2},
      {0, 0, 1, 2},
      {0, 0, 1, 1},
      {0, 0, 1, 0},
      {0, 0, 0, 0},
      {-1, 0, 2, 0}}},
    {{{-1, 0, 2, 2},
      {-1, 1, 2, 0},
      {0, 1, 0, 0},
      {0, 1, 1, 0},
      {0, 0, 1, 2},
      {0, 0, 1, 1},
      {0, 0, 0, 1},
      {-1, 0, 2, 1}}},
    {{{0, 0, 0, 0},
      {0, 0, 0, 1},
      {0, 0, 1, 1},
      {0, 0, 2, 1},
      {0, 0, 2, 0},
      {0, -1, 2, 2},
      {0, -1, 1, 2},
      {0, -1, 0, 2}}},
    {{{0, 0, 0, 1}, {0, 0, 0, 2}, {0, 0, 1, 2}, {0, 0, 2, 2}, {0, 0, 2, 1}, {0, 0, 2, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}}},
    {{{0, 0, 0, 2}, {0, 1, 0, 0}, {0, 1, 1, 0}, {0, 1, 2, 0}, {0, 0, 2, 2}, {0, 0, 2, 1}, {0, 0, 1, 1}, {0, 0, 0, 1}}},
    {{{0, 0, 1, 0},
      {0, 0, 1, 1},
      {0, 0, 2, 1},
      {1, 0, 0, 1},
      {1, 0, 0, 0},
      {1, -1, 0, 2},
      {0, -1, 2, 2},
      {0, -1, 1, 2}}},
    {{{0, 0, 1, 1}, {0, 0, 1, 2}, {0, 0, 2, 2}, {1, 0, 0, 2}, {1, 0, 0, 1}, {1, 0, 0, 0}, {0, 0, 2, 0}, {0, 0, 1, 0}}},
    {{{0, 0, 1, 2}, {0, 1, 1, 0}, {0, 1, 2, 0}, {1, 1, 0, 0}, {1, 0, 0, 2}, {1, 0, 0, 1}, {0, 0, 2, 1}, {0, 0, 1, 1}}},
}};

/// <summary>Per mine layout (a tile's 2-bit mine field), the mine of each of its nine cells.</summary>
inline constexpr std::array<std::array<uint8_t, 9>, 4> MineLayout = {{{}, {}, {1, 0, 1, 0, 1, 0, 1, 0, 1}, {}}};

/// <summary>The overlays that are bridges (road 37..40, railroad 55..58).</summary>
inline constexpr std::array<bool, NumOverlayTypes> OverlayIsBridge = []
{
    std::array<bool, NumOverlayTypes> bridge{};

    for (int32_t overlay : {37, 38, 39, 40, 55, 56, 57, 58})
    {
        bridge[overlay] = true;
    }

    return bridge;
}();

/// <summary>The gate overlays that are closed (68, 70, 72, 74).</summary>
inline constexpr std::array<bool, NumOverlayTypes> OverlayIsClosedGate = []
{
    std::array<bool, NumOverlayTypes> closed{};

    for (int32_t overlay : {68, 70, 72, 74})
    {
        closed[overlay] = true;
    }

    return closed;
}();

/// <summary>The overlays that are dirt roads (1..15 and 31..36).</summary>
inline constexpr std::array<bool, NumOverlayTypes> OverlayIsDirtRoad = []
{
    std::array<bool, NumOverlayTypes> road{};

    for (int32_t overlay = 1; overlay <= 15; overlay++)
    {
        road[overlay] = true;
    }

    for (int32_t overlay = 31; overlay <= 36; overlay++)
    {
        road[overlay] = true;
    }

    return road;
}();

/// <summary>Whether an overlay is a gate (67..74), which opens or closes by team.</summary>
constexpr bool IsGateOverlay(uint32_t overlay)
{
    return overlay >= 67 && overlay <= 74;
}

/// <summary>
/// The overlay a tile's <paramref name="overlay"/> behaves as for a team of <paramref name="alignment"/>: a gate
/// (67..74) opens or closes by team (-1 when it is closed to it, a cost of 20000); any other overlay is itself.
/// </summary>
int32_t GateOverlay(uint32_t overlay, int32_t alignment);

/// <summary>Cell costs per move level and overlay (gamesys.fit's OverlayCellCosts, read by the mover game system).</summary>
extern std::array<int32_t, NumMoveLevels * OverlayWeightLevelSize> OverlayWeightTable;

/// <summary>Where an overlay's 3x3 block starts within a level of <see cref="OverlayWeightTable"/>.</summary>
constexpr int32_t OverlayWeightIndex(int32_t overlay)
{
    return overlay * MapCellDim * MapCellDim;
}

/// <summary>The map's current planet (1 makes dirt roads free to cross; set by the scenario).</summary>
extern int32_t CurPlanet;
/// <summary>The tiles around a mover a simple (one window) path may reach (gamesys.fit's SimplePathTileRange).</summary>
extern int32_t SimpleMovePathRange;

// The size of a movement cell and the map's coordinates, from the terrain's geometry. The original kept them in
// tables and globals the scenario map filled when it loaded; each function gives the same float the table held.

/// <summary>The metres a movement cell side spans.</summary>
inline float MetersPerCell()
{
    return MCTerrain::MetersPerVertexDivMapcellDim;
}

/// <summary>A movement cell side as the coordinate tables computed it (the vertex side times the float 1/3).</summary>
inline double CellSide()
{
    return static_cast<double>(MCTerrain::MetersPerVertex) * (1.0f / 3.0f);
}

/// <summary>Half a movement cell side.</summary>
inline float HalfMapCell()
{
    return static_cast<float>(CellSide() * 0.5);
}

/// <summary>A movement cell's diagonal, in world units.</summary>
inline float MapCellDiagonal()
{
    return static_cast<float>(CellSide() * MetersPerWorldUnit * 1.4142);
}

/// <summary>Half the map side, in vertices.</summary>
inline float VerticesMapSideDivTwo()
{
    return static_cast<float>((MCTerrain::VerticesBlockSide * MCTerrain::BlocksMapSide) / 2);
}

/// <summary>Half the map side, in metres.</summary>
inline float MetersMapSideDivTwo()
{
    return WorldUnitsMapSide * 0.5f;
}

/// <summary>The x of a tile column's west edge.</summary>
inline float TileColToWorld(int32_t tileC)
{
    return static_cast<float>(static_cast<double>(tileC) * MCTerrain::MetersPerVertex -
                              static_cast<double>(WorldUnitsMapSide) * 0.5);
}

/// <summary>The y of a tile row's north edge.</summary>
inline float TileRowToWorld(int32_t tileR)
{
    return static_cast<float>(static_cast<double>(WorldUnitsMapSide) * 0.5 -
                              static_cast<double>(tileR) * MCTerrain::MetersPerVertex);
}

/// <summary>The offset of cell <paramref name="cell"/> (0..2) from its tile's edge.</summary>
inline float CellOffsetToWorld(int32_t cell)
{
    return static_cast<float>(static_cast<double>(cell) * CellSide());
}

/// <summary>The x of a map cell column's west edge.</summary>
inline float CellColToWorld(int32_t cellC)
{
    return static_cast<float>(static_cast<double>(cellC) * MetersPerCell() -
                              static_cast<double>(WorldUnitsMapSide) * 0.5);
}

/// <summary>The y of a map cell row's north edge.</summary>
inline float CellRowToWorld(int32_t cellR)
{
    return static_cast<float>(static_cast<double>(WorldUnitsMapSide) * 0.5 -
                              static_cast<double>(cellR) * MetersPerCell());
}

/// <summary>The centre of map cell (cellR, cellC), as the path finders compute it (z = 0).</summary>
inline MCVector3D MapCellCentre(int32_t cellR, int32_t cellC)
{
    const double mapHalf = static_cast<double>(WorldUnitsMapSide) * 0.5f;
    const float x = static_cast<float>((static_cast<double>(cellC) + 0.5) * MetersPerCell() - mapHalf);
    const float y = static_cast<float>((mapHalf - static_cast<double>(cellR) * MetersPerCell()) -
                                       static_cast<double>(MetersPerCell()) * 0.5);
    return MCVector3D(x, y, 0.0f);
}

/// <summary>The metres an offset steps (the distance from a cell to the one <paramref name="offset"/> away).</summary>
inline float CellShiftDistance(int32_t offset)
{
    const double rows = static_cast<double>(CellShiftRow[offset]);
    const double cols = static_cast<double>(CellShiftCol[offset]);
    const double scale = static_cast<double>(MetersPerWorldUnit) * MCTerrain::MetersPerVertexDivMapcellDim;
    return static_cast<float>(std::sqrt(rows * rows + cols * cols) * scale);
}

/// <summary>The tile and cell of a world position (truncating, not flooring: see MCScenarioMap::WorldToMapPos).</summary>
void WorldCoordToMapCoord(MCVector3D pos, int32_t& tileR, int32_t& tileC, int32_t& cellR, int32_t& cellC);
/// <summary>The tile of a world position (truncating).</summary>
void WorldCoordToMapTile(MCVector3D pos, int32_t& tileR, int32_t& tileC);
/// <summary>The map cell of a world position (truncating).</summary>
void WorldCoordToMapCell(MCVector3D pos, int32_t& cellR, int32_t& cellC);
/// <summary>The centre of cell (cellR, cellC) of tile (tileR, tileC), at z 0.</summary>
MCVector3D MapTileCellToWorldPos(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC);
/// <summary>The centre of map cell (cellR, cellC), at z 0.</summary>
MCVector3D MapCellToWorldPos(int32_t cellR, int32_t cellC);
/// <summary>
/// The point <paramref name="distance"/> meters from <paramref name="pos"/> at <paramref name="angle"/> degrees,
/// pulled back along the line to the last (or first, per <paramref name="flags"/> bit 2) passable cell.
/// </summary>
MCVector3D RelativePositionToPoint(MCVector3D pos, float angle, float distance, uint32_t flags);
/// <summary>The terrain type of a tile texture index (bands of 79).</summary>
int32_t CalcTileTypeFromIndex(int32_t tileIndex);
/// <summary>The overlay type of an overlay texture index.</summary>
int32_t CalcOverlayTypeFromIndex(int32_t overlayIndex);
/// <summary>The cell direction (0-7) an object faces.</summary>
int32_t CellFacing(MCGameObject* object);
/// <summary>The direction (0-7) from one tile/cell to an adjacent one, -2 when they aren't neighbours.</summary>
/// <remarks>Only inlined copies exist in MCX.EXE (in MoveChunk::build).</remarks>
int32_t CellDirToCell(int32_t fromTileR, int32_t fromTileC, int32_t fromCellR, int32_t fromCellC, int32_t toTileR,
                      int32_t toTileC, int32_t toCellR, int32_t toCellC);
