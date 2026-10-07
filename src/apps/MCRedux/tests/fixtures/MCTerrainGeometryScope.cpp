#include "stdafx.h"
#include "MCTerrainGeometryScope.h"
#include "terrain/MCTerrain.h"

MCTerrainGeometryScope::MCTerrainGeometryScope(int32_t verticesBlockSide, int32_t blocksMapSide, float metersPerVertex,
                                               float metersPerElevLevel)
    : _VerticesBlockSide(MCTerrain::VerticesBlockSide)
    , _BlocksMapSide(MCTerrain::BlocksMapSide)
    , _TotalBlocks(MCTerrain::TotalBlocks)
    , _VisibleVerticesPerSide(MCTerrain::VisibleVerticesPerSide)
    , _MetersPerElevLevel(MCTerrain::MetersPerElevLevel)
    , _MetersPerVertex(MCTerrain::MetersPerVertex)
    , _OneOverMetersPerVertex(MCTerrain::OneOvermetersPerVertex)
    , _OneOverVerticesBlockSide(MCTerrain::OneOververticesBlockSide)
    , _VerticesMapSide(MCTerrain::VerticesMapSide)
    , _MetersPerVertexDivMapcellDim(MCTerrain::MetersPerVertexDivMapcellDim)
    , _MetersBlockSide(MCTerrain::MetersBlockSide)
    , _ProjectionSin(MCTerrain::ProjectionSin)
    , _ProjectionCos(MCTerrain::ProjectionCos)
    , _MapTopLeft2d100(MCTerrain::MapTopLeft2d100)
    , _MapTopLeft3d100(MCTerrain::MapTopLeft3d100)
    , _MapTopLeft2d50(MCTerrain::MapTopLeft2d50)
    , _MapTopLeft3d50(MCTerrain::MapTopLeft3d50)
    , _VerticesPerBlock(VerticesPerBlock)
    , _WorldUnitsMapSide(WorldUnitsMapSide)
{
    MCTerrain::SetGeometry(verticesBlockSide, blocksMapSide, metersPerVertex, metersPerElevLevel);
    MCTerrain::SetTopLeftElevation(0.0f);
}

MCTerrainGeometryScope::~MCTerrainGeometryScope()
{
    MCTerrain::VerticesBlockSide = _VerticesBlockSide;
    MCTerrain::BlocksMapSide = _BlocksMapSide;
    MCTerrain::TotalBlocks = _TotalBlocks;
    MCTerrain::VisibleVerticesPerSide = _VisibleVerticesPerSide;
    MCTerrain::MetersPerElevLevel = _MetersPerElevLevel;
    MCTerrain::MetersPerVertex = _MetersPerVertex;
    MCTerrain::OneOvermetersPerVertex = _OneOverMetersPerVertex;
    MCTerrain::OneOververticesBlockSide = _OneOverVerticesBlockSide;
    MCTerrain::VerticesMapSide = _VerticesMapSide;
    MCTerrain::MetersPerVertexDivMapcellDim = _MetersPerVertexDivMapcellDim;
    MCTerrain::MetersBlockSide = _MetersBlockSide;
    MCTerrain::ProjectionSin = _ProjectionSin;
    MCTerrain::ProjectionCos = _ProjectionCos;
    MCTerrain::MapTopLeft2d100 = _MapTopLeft2d100;
    MCTerrain::MapTopLeft3d100 = _MapTopLeft3d100;
    MCTerrain::MapTopLeft2d50 = _MapTopLeft2d50;
    MCTerrain::MapTopLeft3d50 = _MapTopLeft3d50;
    VerticesPerBlock = _VerticesPerBlock;
    WorldUnitsMapSide = _WorldUnitsMapSide;
}
