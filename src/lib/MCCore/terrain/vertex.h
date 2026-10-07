#pragma once

class MCVector2D;
class MCVector3D;
class MCPacketFile;
struct MCTerrainTile;
struct MCTerrainFrame;

/// <summary>
/// One vertex of a map block as stored in the terrain's <c>.elv</c> packet file: an elevation level and the terrain
/// tile (texture) and overlay drawn on the block whose top-left corner it is. Each map block is one packet of
/// <c>verticesBlockSide * verticesBlockSide</c> of these, row by row.
/// </summary>
/// <remarks>
/// On-disk and in-memory layout, 8 bytes. <see cref="MCMapBlockManager::GenerateRandomBlock"/> fills a missing block
/// with the map's base elevation, flags 0 and tile 0x29 in both tile slots.
/// </remarks>
#pragma pack(push, 1)
struct MCPrecompVertex
{
    /// <summary>Elevation level: the height is <c>elevation * Terrain::metersPerElevLevel</c>.</summary>
    uint8_t Elevation;
    /// <summary>
    /// The map editor's group of the vertex's terrain tile: in the retail maps it follows <see cref="TextureData"/>
    /// almost one to one (0 for tiles 0-36, 1 for 79-194, 42 for 3204-3207...). The game never reads it;
    /// generateRandomBlock writes 0.
    /// </summary>
    uint8_t TileGroup; // Fixed layout: .elv vertex
    /// <summary>
    /// What the map editor's memory held there: 0, 0x0101 or fragments of text ("xt", "ck") in the retail maps.
    /// Never read or written by the game.
    /// </summary>
    int16_t EditorLeftover; // Fixed layout: .elv vertex
    /// <summary>Terrain tile index into <c>TerrainTiles</c> (negative: none).</summary>
    int16_t TextureData;
    /// <summary>Overlay tile index (roads, craters... drawn over the terrain tile; negative: none).</summary>
    int16_t OverlayData;
};
#pragma pack(pop)
static_assert(sizeof(MCPrecompVertex) == 8);

/// <summary>
/// A vertex of a terrain window's visible grid: which map vertex it is, where it projects on screen this frame and
/// whether its blocks need redrawing.
/// </summary>
/// <remarks>
/// Original source: <c>terrain\vertex.cpp</c>, 0x18 bytes. Built by <see cref="MCMapBlockManager::BuildWindow"/>; the
/// screen position and flags are refreshed by <c>TerrainWindow::render</c>.
/// </remarks>
class MCVertex
{
public:
    /// <summary>The map vertex (in the block cache of <see cref="MCMapBlockManager"/>).</summary>
    MCPrecompVertex* PVertex;
    /// <summary>Projected screen x.</summary>
    int32_t Px;
    /// <summary>Projected screen y.</summary>
    int32_t Py;
    /// <summary>The map block the vertex belongs to (<c>row * blocksMapSide + column</c>).</summary>
    int16_t BlockNum;
    /// <summary>The vertex within its block (<c>row * verticesBlockSide + column</c>).</summary>
    int16_t VertexNum;
    /// <summary>The vertex's map position as vertex row (high 16 bits) and column (low 16 bits).</summary>
    uint32_t PosTile;
    /// <summary>Nonzero when the vertex projects outside the visible pane.</summary>
    uint8_t Clipped;
    /// <summary>Nonzero when the blocks around the vertex must be redrawn (newly exposed screen area).</summary>
    uint8_t Redraw;
    /// <summary>Nonzero when the vertex lies near the pane edge the view is scrolling towards.</summary>
    uint8_t EdgeRedraw;
};

/// <summary>
/// One quad of a terrain window: the four vertices at its corners and the tiles last drawn on it.
/// </summary>
/// <remarks>Original source: <c>terrain\vertex.cpp</c>, 0x1c bytes.</remarks>
class MCTerrainBlock
{
public:
    /// <summary>Stores the corner vertices.</summary>
    /// <returns>0.</returns>
    int32_t Init(MCVertex* v0, MCVertex* v1, MCVertex* v2, MCVertex* v3);

    /// <summary>
    /// Draws the block's terrain tile through <c>VFX_nTile_draw</c>, hazed by <paramref name="hazeFactor"/> and the
    /// fog of war (visible/seen bits of the home team).
    /// </summary>
    void Draw(int32_t hazeFactor, uint8_t flags);

    /// <summary>Draws the block's overlay tile (roads, craters...).</summary>
    void DrawOverlay(int32_t hazeFactor, uint8_t flags);

    /// <summary>Draws the block's outline (the debug terrain grid).</summary>
    void DrawLine(int32_t color, int onlyTop);

    /// <summary>Does nothing in MCX.EXE.</summary>
    void DrawHaze(int32_t hazeFactor, uint8_t flags);

    /// <summary>The corners: top-left, top-right, bottom-right, bottom-left.</summary>
    MCVertex* Vertices[4];
    /// <summary>The terrain tile drawn last frame.</summary>
    MCTerrainTile* Tile;
    /// <summary>The overlay tile drawn last frame.</summary>
    MCTerrainTile* OverlayTile;
    /// <summary>How many of the four corners were visible last frame (a change forces a redraw).</summary>
    uint8_t LastVisibleCount;
};

/// <summary>
/// The terrain's block cache: every map block's vertices (<see cref="MCPrecompVertex"/>) read from the terrain's
/// <c>.elv</c> packet file into one buffer, plus the per-window bookkeeping of which block each terrain
/// window's top-left corner lies in.
/// </summary>
/// <remarks>
/// Original source: <c>terrain\vertex.cpp</c>, 0x3c bytes (a HeapManager in the original). The packet
/// file has <c>blocksMapSide * blocksMapSide</c> packets (one per block); the extra block pointer at index
/// <c>blocksMapSide * blocksMapSide</c> is the "off the map" block that out-of-range lookups return. Files of the
/// old formats (0x23318 or 0x4f2ec bytes) are refused with "Old Map format".
/// </remarks>
class MCMapBlockManager
{
public:
    /// <summary>Closes the block file, frees the block table and the blocks.</summary>
    void Destroy();

    /// <summary>
    /// Opens <c>&lt;terrainPath&gt;&lt;fileName&gt;.elv</c>, reserves <c>(numBlocks + 1) * blockSize</c> bytes and
    /// reads every block packet into it, and allocates the per-window tables for <paramref name="numBlocks"/>.
    /// </summary>
    int32_t Init(char* fileName, int32_t numBlocks, int32_t blockSize);

    /// <summary>The vertices of block <paramref name="blockNum"/> (the off-map block when out of range).</summary>
    MCPrecompVertex* BlockPtr(int32_t blockNum);

    /// <summary>The elevation of the map's first vertex, in meters (0 without a block file).</summary>
    float GetTopLeftElevation();

    /// <summary>
    /// Recomputes, from the camera position <paramref name="cameraPos"/>, the block and vertex offset of window
    /// <paramref name="windowNum"/>'s top-left corner and the window's world top-left.
    /// </summary>
    /// <returns>0.</returns>
    int32_t Update(MCVector3D& cameraPos, int32_t windowNum);

    /// <summary>
    /// Fills a window's visible vertex grid and block list from the block cache, starting at the corner
    /// <see cref="Update"/> computed.
    /// </summary>
    /// <remarks>
    /// Called by <c>MCTerrainWindow::Init</c> and <c>MCTerrainWindow::Update</c>.
    /// </remarks>
    void BuildWindow(MCVertex* vertexList, int32_t* numVertices, MCTerrainBlock* blockList, int32_t* numBlocks,
                     int32_t windowNum);

    /// <summary>Adds <paramref name="value"/> to the overlay tile of vertex <paramref name="vertexNum"/> of a block.</summary>
    void SetOverlayTile(int32_t blockNum, int32_t vertexNum, int32_t value);

    int32_t GetOverlayTile(int32_t blockNum, int32_t vertexNum);

    /// <summary>Adds <paramref name="value"/> to the terrain tile of vertex <paramref name="vertexNum"/> of a block.</summary>
    void SetTile(int32_t blockNum, int32_t vertexNum, int32_t value);

    int32_t GetTile(int32_t blockNum, int32_t vertexNum);

    /// <summary>Does nothing in MCX.EXE.</summary>
    void MarkSeen(MCVector2D& topLeft, MCVertex* vertexList, MCVector3D& looker, MCVector3D& lookVector, float angle,
                  float range, uint8_t who);

    /// <summary>
    /// The slope at <paramref name="pos"/> from the elevations of the four map cells around it; the face normal is
    /// written to <paramref name="normal"/> when given.
    /// </summary>
    float TerrainAngle(MCVector3D& pos, MCVector3D* normal);

    /// <summary>The normal of the terrain face under <paramref name="pos"/>.</summary>
    MCVector3D TerrainNormal(MCVector3D& pos);

    /// <summary>The terrain height at <paramref name="pos"/>, interpolated across its face.</summary>
    float TerrainElevation(MCVector3D& pos);

    /// <summary>Fills a block with flat ground at the map's base elevation and tile 0x29.</summary>
    void GenerateRandomBlock(MCPrecompVertex* block);

    /// <summary>Every block's vertices, as read from the block file.</summary>
    std::vector<uint8_t> BlockData;
    /// <summary>Each block's vertices in <see cref="BlockData"/> (<c>blocksMapSide * blocksMapSide + 1</c> entries).</summary>
    std::vector<MCPrecompVertex*> Blocks;
    /// <summary>Per window: the block its corner was in last update (-1 initially).</summary>
    std::vector<int32_t> LastBlock;
    /// <summary>Per window: the block its top-left corner is in.</summary>
    std::vector<int32_t> CurrentBlock;
    /// <summary>Block column of the corner being built.</summary>
    int32_t TopLeftBlockX = 0;
    /// <summary>Block row of the corner being built.</summary>
    int32_t TopLeftBlockY = 0;
    /// <summary>The <c>.elv</c> block file (closed after init; kept for getTopLeftElevation).</summary>
    MCPacketFile* BlockFile = nullptr;
    /// <summary>Per window: the vertex offset (x, y) of the top-left corner within its block, 8 bytes each.</summary>
    std::vector<float> VertexOffsets;
    /// <summary>Per window: block-step counter bumped by buildWindow as the corner crosses blocks.</summary>
    std::vector<int32_t> BlockSteps;
};

/// <summary>
/// The storage of every terrain window's <see cref="MCVertex"/> grid (<c>visibleVerticesPerSide^2</c> vertices each).
/// </summary>
/// <remarks>Original source: <c>terrain\vertex.cpp</c>, 0x24 bytes (a HeapManager in the original).</remarks>
class MCVertexManager
{
public:
    /// <summary>The grids' storage (zeroed, never constructed: buildWindow fills it).</summary>
    std::vector<uint8_t> Storage;
    /// <summary>Per window: its vertex grid in <see cref="Storage"/>.</summary>
    std::vector<MCVertex*> VertexLists;
};

/// <summary>
/// The storage of every terrain window's <see cref="MCTerrainBlock"/> list (<c>visibleVerticesPerSide^2</c> blocks each).
/// </summary>
/// <remarks>Original source: <c>terrain\vertex.cpp</c>, 0x24 bytes (a HeapManager in the original).</remarks>
class MCTerrainTileManager
{
public:
    /// <summary>The lists' storage (zeroed, never constructed: buildWindow fills it).</summary>
    std::vector<uint8_t> Storage;
    /// <summary>Per window: its block list in <see cref="Storage"/>.</summary>
    std::vector<MCTerrainBlock*> BlockLists;
};

/// <summary>
/// The terrain height at <paramref name="pos"/>, interpolated across the face (triangle) of the map cell it lies in
/// from the <c>GameMap</c> cell elevations. The body of <see cref="MCMapBlockManager::TerrainElevation"/>.
/// </summary>
/// <remarks>A file-local function in the original; it doesn't use the manager.</remarks>
float TerrainElevationAt(MCVector3D& pos);

/// <summary>
/// Port: the frame's terrain pass as a hardware renderer draws it from the map's ground mesh (built here, and again
/// when the map's data changed since). Checks that every vertex of <paramref name="vertexList"/>'s grid (projected
/// this frame) is the map's vertex at its place and lies where the mesh puts it; an error, saying which and how, when
/// one doesn't (the mesh would draw another picture than the tiles).
/// </summary>
/// <param name="minX">The corner range of the projection (a vertex outside it is clipped); also maxX, minY, maxY.</param>
std::expected<void, std::string> MCTerrainGroundFrame(const MCVertex* vertexList, int32_t numVertices,
                                                      int32_t numBlocks, int32_t hazeFactor, int32_t stepX,
                                                      int32_t stepY, int32_t elevStep, int32_t minX, int32_t maxX,
                                                      int32_t minY, int32_t maxY, MCTerrainFrame& frame);

/// <summary>Port: lets go of the ground mesh (the terrain is destroyed).</summary>
void MCTerrainForgetMesh();

/// <summary>Terrain tile cache requests this mission (statistics).</summary>
extern int32_t TileCacheReqs;
/// <summary>Terrain tile cache hits.</summary>
extern int32_t TileCacheHits;
/// <summary>Terrain tile cache misses (tiles read from the packet files).</summary>
extern int32_t TileCacheMiss;
/// <summary>Terrain faces drawn (statistics).</summary>
extern int32_t NumTerrainFaces;
