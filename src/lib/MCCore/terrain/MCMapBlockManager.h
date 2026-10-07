#pragma once

#include "terrain/MCVertex.h"

class MCTerrainWindow;
class MCVector3D;

/// <summary>
/// The terrain's block cache: every map block's vertices (<see cref="MCPrecompVertex"/>) read from the terrain's
/// <c>.elv</c> packet file, plus the "off the map" block that out-of-range lookups return. It places a terrain
/// window's grid over the camera and fills it.
/// </summary>
/// <remarks>
/// The packet file has <c>blocksMapSide * blocksMapSide</c> packets (one per block), and the off-map block is read
/// from the packet after them. Files of the old formats (0x23318 or 0x4f2ec bytes) are refused with "Old Map
/// format".
/// </remarks>
class MCMapBlockManager
{
public:
    /// <summary>
    /// Reads <c>&lt;terrainPath&gt;&lt;fileName&gt;.elv</c>: <paramref name="numBlocks"/> blocks and the off-map one
    /// (the map's geometry, <see cref="MCTerrain::SetGeometry"/>, gives a block's vertices).
    /// </summary>
    static std::expected<std::unique_ptr<MCMapBlockManager>, std::string> Create(std::string_view fileName,
                                                                                 int32_t numBlocks);

    /// <summary>
    /// A cache holding <paramref name="numBlocks"/> blocks and the off-map one from <paramref name="vertices"/>, block
    /// after block (missing vertices are zero).
    /// </summary>
    MCMapBlockManager(std::span<const MCPrecompVertex> vertices, int32_t numBlocks);

    /// <summary>The vertices of block <paramref name="blockNum"/> (the off-map block when out of range).</summary>
    MCPrecompVertex* BlockPtr(int32_t blockNum);

    /// <summary>The vertices of block <paramref name="blockNum"/>, which must be on the map.</summary>
    MCPrecompVertex* Block(int32_t blockNum) { return &_Vertices[static_cast<size_t>(blockNum) * _BlockSize]; }

    /// <summary>The map vertex at (<paramref name="row"/>, <paramref name="col"/>): in its block, or in the off-map
    /// block (at the same place within a block) off the map.</summary>
    MCPrecompVertex* MapVertexAt(int32_t row, int32_t col);

    /// <summary>The elevation of the map's first vertex, in meters.</summary>
    float GetTopLeftElevation();

    /// <summary>
    /// Raises the off-map block to one level above the map's base, once the map's base elevation is known (the first
    /// turn).
    /// </summary>
    void RaiseOffMapBlock();

    /// <summary>
    /// Recomputes, from the camera position <paramref name="cameraPos"/>, the block and vertex offset of
    /// <paramref name="window"/>'s top-left corner and the window's world top-left.
    /// </summary>
    void Update(const MCVector3D& cameraPos, MCTerrainWindow& window);

    /// <summary>
    /// Fills <paramref name="window"/>'s visible vertex grid from the block cache, starting at the corner
    /// <see cref="Update"/> computed.
    /// </summary>
    void BuildWindow(MCTerrainWindow& window);

    /// <summary>Adds <paramref name="value"/> to the overlay tile of vertex <paramref name="vertexNum"/> of a block.</summary>
    void SetOverlayTile(int32_t blockNum, int32_t vertexNum, int32_t value);

    int32_t GetOverlayTile(int32_t blockNum, int32_t vertexNum);

    /// <summary>Adds <paramref name="value"/> to the terrain tile of vertex <paramref name="vertexNum"/> of a block.</summary>
    void SetTile(int32_t blockNum, int32_t vertexNum, int32_t value);

    int32_t GetTile(int32_t blockNum, int32_t vertexNum);

private:
    /// <summary>Every block's vertices, block after block, the off-map block last.</summary>
    std::vector<MCPrecompVertex> _Vertices;
    /// <summary>Vertices in a block.</summary>
    size_t _BlockSize = 0;
    /// <summary>Blocks on the map (the off-map block's index).</summary>
    int32_t _NumBlocks = 0;
};

/// <summary>
/// The terrain height at <paramref name="pos"/>, interpolated across the face (triangle) of the map cell it lies in
/// from the <c>GameMap</c> cell elevations; 0 off the map.
/// </summary>
float TerrainElevationAt(const MCVector3D& pos);

/// <summary>
/// The slope at <paramref name="pos"/> in degrees, from the elevations of the four map cells around it; the face
/// normal is written to <paramref name="normal"/> when given.
/// </summary>
/// <remarks>Original behaviour: a vertical face gives the corner's height instead of an angle.</remarks>
float TerrainAngleAt(const MCVector3D& pos, MCVector3D* normal);

/// <summary>The upward normal of the terrain face under <paramref name="pos"/>.</summary>
MCVector3D TerrainNormalAt(const MCVector3D& pos);
