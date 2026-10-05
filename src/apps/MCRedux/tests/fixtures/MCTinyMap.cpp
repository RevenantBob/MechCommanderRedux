#include "stdafx.h"
#include "MCTinyMap.h"
#include "lib/heap.h"
#include "terrain/terrain.h"

namespace
{
    /// <summary>A tile's cells word with all nine movement cells passable (bit 14 of each cell's 2-bit pair).</summary>
    constexpr uint32_t AllPassable = []
    {
        uint32_t cells = 0;

        for (uint32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++)
        {
            cells |= 0x4000u << (cell * 2);
        }

        return cells;
    }();
}

MCTinyMap::MCTinyMap(int32_t tiles)
    : _Tiles(tiles)
    , _PreviousMap(GameMap)
    , _PreviousWorldUnitsMapSide(worldUnitsMapSide)
    , _PreviousVerticesBlockSide(Terrain::verticesBlockSide)
    , _PreviousBlocksMapSide(Terrain::blocksMapSide)
    , _PreviousMetersPerVertex(Terrain::metersPerVertex)
    , _PreviousOneOverMetersPerVertex(Terrain::OneOvermetersPerVertex)
    , _PreviousMetersBlockSide(Terrain::metersBlockSide)
    , _PreviousMetersPerVertexDivMapCell(Terrain::metersPerVertexDivMAPCELL_DIM)
{
    if (systemHeap == nullptr)
    {
        systemHeap = new UserHeap;
        systemHeap->init(16383999, "SystemHeap");
    }

    // As a terrain FIT's [TerrainData] sets it: one block of tiles x tiles vertices.
    Terrain::verticesBlockSide = tiles;
    Terrain::blocksMapSide = 1;
    Terrain::metersPerVertex = MetersPerTile;
    Terrain::OneOvermetersPerVertex = 1.0f / Terrain::metersPerVertex;
    Terrain::metersBlockSide = static_cast<float>(Terrain::verticesBlockSide) * Terrain::metersPerVertex;
    Terrain::metersPerVertexDivMAPCELL_DIM = Terrain::metersPerVertex * (1.0f / 3.0f);
    worldUnitsMapSide = static_cast<float>(Terrain::blocksMapSide) * Terrain::metersBlockSide;

    GameMap = new ScenarioMap;
    GameMap->init(tiles, tiles);

    for (int32_t i = 0; i < tiles * tiles; i++)
    {
        GameMap->map[i].cells = AllPassable;
    }
}

MCTinyMap::~MCTinyMap()
{
    GameMap->destroy();
    delete GameMap;
    GameMap = _PreviousMap;
    worldUnitsMapSide = _PreviousWorldUnitsMapSide;
    Terrain::verticesBlockSide = _PreviousVerticesBlockSide;
    Terrain::blocksMapSide = _PreviousBlocksMapSide;
    Terrain::metersPerVertex = _PreviousMetersPerVertex;
    Terrain::OneOvermetersPerVertex = _PreviousOneOverMetersPerVertex;
    Terrain::metersBlockSide = _PreviousMetersBlockSide;
    Terrain::metersPerVertexDivMAPCELL_DIM = _PreviousMetersPerVertexDivMapCell;
}

void MCTinyMap::Block(int32_t row, int32_t col)
{
    MapTile& tile = GameMap->map[(row / MAPCELL_DIM) * _Tiles + col / MAPCELL_DIM];
    const uint32_t shift = static_cast<uint32_t>(((row % MAPCELL_DIM) * MAPCELL_DIM + col % MAPCELL_DIM) * 2);
    tile.cells &= ~(0x4000u << shift);
}

bool MCTinyMap::Passable(int32_t row, int32_t col) const
{
    if (row < 0 || col < 0 || row >= Cells() || col >= Cells())
    {
        return false;
    }

    return GameMap->map[(row / MAPCELL_DIM) * _Tiles + col / MAPCELL_DIM].getCellPassable(row % MAPCELL_DIM,
                                                                                          col % MAPCELL_DIM) != 0;
}

vector_3d MCTinyMap::CellCentre(int32_t row, int32_t col) const
{
    const float cellSide = Terrain::metersPerVertexDivMAPCELL_DIM;
    const float x = (static_cast<float>(col) + 0.5f) * cellSide - worldUnitsMapSide * 0.5f;
    const float y = worldUnitsMapSide * 0.5f - (static_cast<float>(row) + 0.5f) * cellSide;
    return vector_3d(x, y, 0.0f);
}
