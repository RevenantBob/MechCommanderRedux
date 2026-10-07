#pragma once

#include "lib/MCVector2D.h"
#include "lib/MCVector3D.h"

/// <summary>
/// Sets the terrain's geometry statics (<see cref="MCTerrain::SetGeometry"/>, the top-left corner at elevation 0) for
/// its lifetime, then puts the previous values back.
/// </summary>
class MCTerrainGeometryScope
{
public:
    MCTerrainGeometryScope(int32_t verticesBlockSide, int32_t blocksMapSide, float metersPerVertex,
                           float metersPerElevLevel);
    ~MCTerrainGeometryScope();
    MCTerrainGeometryScope(const MCTerrainGeometryScope&) = delete;
    MCTerrainGeometryScope& operator=(const MCTerrainGeometryScope&) = delete;

private:
    int32_t _VerticesBlockSide;
    int32_t _BlocksMapSide;
    int32_t _TotalBlocks;
    int32_t _VisibleVerticesPerSide;
    float _MetersPerElevLevel;
    float _MetersPerVertex;
    float _OneOverMetersPerVertex;
    float _OneOverVerticesBlockSide;
    int32_t _VerticesMapSide;
    float _MetersPerVertexDivMapcellDim;
    float _MetersBlockSide;
    float _ProjectionSin;
    float _ProjectionCos;
    MCVector2D _MapTopLeft2d100;
    MCVector3D _MapTopLeft3d100;
    MCVector2D _MapTopLeft2d50;
    MCVector3D _MapTopLeft3d50;
    int32_t _VerticesPerBlock;
    float _WorldUnitsMapSide;
};
