#include "stdafx.h"
#include "MCTinyMap.h"
#include "main/MCGameContext.h"
#include "terrain/MCTerrain.h"

namespace
{
    /// <summary>A tile's cells word with all nine movement cells passable (bit 14 of each cell's 2-bit pair).</summary>
    constexpr uint32_t AllPassable = []
    {
        uint32_t cells = 0;

        for (uint32_t cell = 0; cell < MapCellDim * MapCellDim; cell++)
        {
            cells |= 0x4000u << (cell * 2);
        }

        return cells;
    }();
}

MCTinyMap::MCTinyMap(int32_t tiles)
    : _Tiles(tiles)
    , _PreviousWorldUnitsMapSide(WorldUnitsMapSide)
    , _PreviousVerticesBlockSide(MCTerrain::VerticesBlockSide)
    , _PreviousBlocksMapSide(MCTerrain::BlocksMapSide)
    , _PreviousMetersPerVertex(MCTerrain::MetersPerVertex)
    , _PreviousOneOverMetersPerVertex(MCTerrain::OneOvermetersPerVertex)
    , _PreviousMetersBlockSide(MCTerrain::MetersBlockSide)
    , _PreviousMetersPerVertexDivMapCell(MCTerrain::MetersPerVertexDivMapcellDim)
{
    // As a terrain FIT's [TerrainData] sets it: one block of tiles x tiles vertices.
    MCTerrain::VerticesBlockSide = tiles;
    MCTerrain::BlocksMapSide = 1;
    MCTerrain::MetersPerVertex = MetersPerTile;
    MCTerrain::OneOvermetersPerVertex = 1.0f / MCTerrain::MetersPerVertex;
    MCTerrain::MetersBlockSide = static_cast<float>(MCTerrain::VerticesBlockSide) * MCTerrain::MetersPerVertex;
    MCTerrain::MetersPerVertexDivMapcellDim = MCTerrain::MetersPerVertex * (1.0f / 3.0f);
    WorldUnitsMapSide = static_cast<float>(MCTerrain::BlocksMapSide) * MCTerrain::MetersBlockSide;

    auto system = std::make_unique<MCMoveSystem>();
    system->Map = std::make_unique<MCScenarioMap>(tiles, tiles);

    for (MCMapTile& tile : system->Map->Map)
    {
        tile.Cells = AllPassable;
    }

    _System = system.get();
    _PreviousSystem = MCGameContext::Current().SetMoveSystem(std::move(system));
}

MCTinyMap::~MCTinyMap()
{
    MCGameContext::Current().SetMoveSystem(std::move(_PreviousSystem));
    WorldUnitsMapSide = _PreviousWorldUnitsMapSide;
    MCTerrain::VerticesBlockSide = _PreviousVerticesBlockSide;
    MCTerrain::BlocksMapSide = _PreviousBlocksMapSide;
    MCTerrain::MetersPerVertex = _PreviousMetersPerVertex;
    MCTerrain::OneOvermetersPerVertex = _PreviousOneOverMetersPerVertex;
    MCTerrain::MetersBlockSide = _PreviousMetersBlockSide;
    MCTerrain::MetersPerVertexDivMapcellDim = _PreviousMetersPerVertexDivMapCell;
}

void MCTinyMap::Block(int32_t row, int32_t col)
{
    MCMapTile& tile = _System->Map->TileAt(row / MapCellDim, col / MapCellDim);
    const uint32_t shift = static_cast<uint32_t>(((row % MapCellDim) * MapCellDim + col % MapCellDim) * 2);
    tile.Cells &= ~(0x4000u << shift);
}

bool MCTinyMap::Passable(int32_t row, int32_t col) const
{
    if (row < 0 || col < 0 || row >= Cells() || col >= Cells())
    {
        return false;
    }

    return _System->Map->TileAt(row / MapCellDim, col / MapCellDim)
               .GetCellPassable(row % MapCellDim, col % MapCellDim) != 0;
}

MCVector3D MCTinyMap::CellCentre(int32_t row, int32_t col) const
{
    const float cellSide = MCTerrain::MetersPerVertexDivMapcellDim;
    const float x = (static_cast<float>(col) + 0.5f) * cellSide - WorldUnitsMapSide * 0.5f;
    const float y = WorldUnitsMapSide * 0.5f - (static_cast<float>(row) + 0.5f) * cellSide;
    return MCVector3D(x, y, 0.0f);
}
