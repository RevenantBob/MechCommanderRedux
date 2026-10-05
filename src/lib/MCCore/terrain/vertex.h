#pragma once

class vector_2d;
class vector_3d;
class PacketFile;
struct TerrainTile;
struct MCTerrainFrame;

/// <summary>
/// One vertex of a map block as stored in the terrain's <c>.elv</c> packet file: an elevation level and the terrain
/// tile (texture) and overlay drawn on the block whose top-left corner it is. Each map block is one packet of
/// <c>verticesBlockSide * verticesBlockSide</c> of these, row by row.
/// </summary>
/// <remarks>
/// On-disk and in-memory layout, 8 bytes. <see cref="MapBlockManager::generateRandomBlock"/> fills a missing block
/// with the map's base elevation, flags 0 and tile 0x29 in both tile slots.
/// </remarks>
#pragma pack(push, 1)
struct PrecompVertex
{
    /// <summary>Elevation level: the height is <c>elevation * Terrain::metersPerElevLevel</c>.</summary>
    uint8_t elevation; // +0x00
    /// <summary>
    /// The map editor's group of the vertex's terrain tile: in the retail maps it follows <see cref="textureData"/>
    /// almost one to one (0 for tiles 0-36, 1 for 79-194, 42 for 3204-3207...). The game never reads it;
    /// generateRandomBlock writes 0.
    /// </summary>
    uint8_t tileGroup; // +0x01 // Fixed layout: .elv vertex
    /// <summary>
    /// What the map editor's memory held there: 0, 0x0101 or fragments of text ("xt", "ck") in the retail maps.
    /// Never read or written by the game.
    /// </summary>
    int16_t editorLeftover; // +0x02 // Fixed layout: .elv vertex
    /// <summary>Terrain tile index into <c>TerrainTiles</c> (negative: none).</summary>
    int16_t textureData; // +0x04
    /// <summary>Overlay tile index (roads, craters... drawn over the terrain tile; negative: none).</summary>
    int16_t overlayData; // +0x06
};
#pragma pack(pop)
static_assert(sizeof(PrecompVertex) == 8);

/// <summary>
/// A vertex of a terrain window's visible grid: which map vertex it is, where it projects on screen this frame and
/// whether its blocks need redrawing.
/// </summary>
/// <remarks>
/// Original source: <c>terrain\vertex.cpp</c>, 0x18 bytes. Built by <see cref="MapBlockManager::buildWindow"/>; the
/// screen position and flags are refreshed by <c>TerrainWindow::render</c>.
/// </remarks>
class Vertex
{
public:
    /// <summary>The map vertex (in the block cache of <see cref="MapBlockManager"/>).</summary>
    PrecompVertex* pVertex; // +0x00
    /// <summary>Projected screen x.</summary>
    int32_t px; // +0x04
    /// <summary>Projected screen y.</summary>
    int32_t py; // +0x08
    /// <summary>The map block the vertex belongs to (<c>row * blocksMapSide + column</c>).</summary>
    int16_t blockNum; // +0x0c
    /// <summary>The vertex within its block (<c>row * verticesBlockSide + column</c>).</summary>
    int16_t vertexNum; // +0x0e
    /// <summary>The vertex's map position as vertex row (high 16 bits) and column (low 16 bits).</summary>
    uint32_t posTile; // +0x10
    /// <summary>Nonzero when the vertex projects outside the visible pane.</summary>
    uint8_t clipped; // +0x14
    /// <summary>Nonzero when the blocks around the vertex must be redrawn (newly exposed screen area).</summary>
    uint8_t redraw; // +0x15
    /// <summary>Nonzero when the vertex lies near the pane edge the view is scrolling towards.</summary>
    uint8_t edgeRedraw; // +0x16
};

/// <summary>
/// One quad of a terrain window: the four vertices at its corners and the tiles last drawn on it.
/// </summary>
/// <remarks>Original source: <c>terrain\vertex.cpp</c>, 0x1c bytes.</remarks>
class TerrainBlock
{
public:
    /// <summary>Stores the corner vertices.</summary>
    /// <returns>0.</returns>
    /// <remarks>MCX.EXE @ 0x007495c0</remarks>
    int32_t init(Vertex* v0, Vertex* v1, Vertex* v2, Vertex* v3);

    /// <summary>
    /// Draws the block's terrain tile through <c>VFX_nTile_draw</c>, hazed by <paramref name="hazeFactor"/> and the
    /// fog of war (visible/seen bits of the home team).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x007495e0</remarks>
    void draw(int32_t hazeFactor, uint8_t flags);

    /// <summary>Draws the block's overlay tile (roads, craters...).</summary>
    /// <remarks>MCX.EXE @ 0x00749860</remarks>
    void drawOverlay(int32_t hazeFactor, uint8_t flags);

    /// <summary>Draws the block's outline (the debug terrain grid).</summary>
    /// <remarks>MCX.EXE @ 0x00749ee0</remarks>
    void drawLine(int32_t color, int onlyTop);

    /// <summary>Does nothing in MCX.EXE.</summary>
    /// <remarks>MCX.EXE @ 0x0074a0a0</remarks>
    void drawHaze(int32_t hazeFactor, uint8_t flags);

    /// <summary>The corners: top-left, top-right, bottom-right, bottom-left.</summary>
    Vertex* vertices[4]; // +0x00
    /// <summary>The terrain tile drawn last frame.</summary>
    TerrainTile* tile; // +0x10
    /// <summary>The overlay tile drawn last frame.</summary>
    TerrainTile* overlayTile; // +0x14
    /// <summary>How many of the four corners were visible last frame (a change forces a redraw).</summary>
    uint8_t lastVisibleCount; // +0x1b
};

/// <summary>
/// The terrain's block cache: every map block's vertices (<see cref="PrecompVertex"/>) read from the terrain's
/// <c>.elv</c> packet file into one buffer, plus the per-window bookkeeping of which block each terrain
/// window's top-left corner lies in.
/// </summary>
/// <remarks>
/// Original source: <c>terrain\vertex.cpp</c>, 0x3c bytes (a HeapManager in the original). The packet
/// file has <c>blocksMapSide * blocksMapSide</c> packets (one per block); the extra block pointer at index
/// <c>blocksMapSide * blocksMapSide</c> is the "off the map" block that out-of-range lookups return. Files of the
/// old formats (0x23318 or 0x4f2ec bytes) are refused with "Old Map format".
/// </remarks>
class MapBlockManager
{
public:
    /// <summary>Closes the block file, frees the block table and the blocks.</summary>
    /// <remarks>MCX.EXE @ 0x00747d30</remarks>
    void destroy();

    /// <summary>
    /// Opens <c>&lt;terrainPath&gt;&lt;fileName&gt;.elv</c>, reserves <c>(numBlocks + 1) * blockSize</c> bytes and
    /// reads every block packet into it, and allocates the per-window tables for <paramref name="numBlocks"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00747d80</remarks>
    int32_t init(char* fileName, int32_t numBlocks, int32_t blockSize);

    /// <summary>The vertices of block <paramref name="blockNum"/> (the off-map block when out of range).</summary>
    /// <remarks>MCX.EXE @ 0x00747f70</remarks>
    PrecompVertex* blockPtr(int32_t blockNum);

    /// <summary>The elevation of the map's first vertex, in meters (0 without a block file).</summary>
    /// <remarks>MCX.EXE @ 0x00747fa0</remarks>
    float getTopLeftElevation();

    /// <summary>
    /// Recomputes, from the camera position <paramref name="cameraPos"/>, the block and vertex offset of window
    /// <paramref name="windowNum"/>'s top-left corner and the window's world top-left.
    /// </summary>
    /// <returns>0.</returns>
    /// <remarks>MCX.EXE @ 0x00747fd0</remarks>
    int32_t update(vector_3d& cameraPos, int32_t windowNum);

    /// <summary>
    /// Fills a window's visible vertex grid and block list from the block cache, starting at the corner
    /// <see cref="update"/> computed.
    /// </summary>
    /// <remarks>
    /// MCX.EXE @ 0x00748200. Its name wasn't kept (the linker left no symbol); this is the port's name. Called by
    /// <c>TerrainWindow::init</c> and <c>TerrainWindow::update</c>.
    /// </remarks>
    void buildWindow(Vertex* vertexList, int32_t* numVertices, TerrainBlock* blockList, int32_t* numBlocks,
                     int32_t windowNum);

    /// <summary>Adds <paramref name="value"/> to the overlay tile of vertex <paramref name="vertexNum"/> of a block.</summary>
    /// <remarks>MCX.EXE @ 0x00748590</remarks>
    void setOverlayTile(int32_t blockNum, int32_t vertexNum, int32_t value);

    /// <remarks>MCX.EXE @ 0x007485b0</remarks>
    int32_t getOverlayTile(int32_t blockNum, int32_t vertexNum);

    /// <summary>Adds <paramref name="value"/> to the terrain tile of vertex <paramref name="vertexNum"/> of a block.</summary>
    /// <remarks>MCX.EXE @ 0x007485d0</remarks>
    void setTile(int32_t blockNum, int32_t vertexNum, int32_t value);

    /// <remarks>MCX.EXE @ 0x007485f0</remarks>
    int32_t getTile(int32_t blockNum, int32_t vertexNum);

    /// <summary>Does nothing in MCX.EXE.</summary>
    /// <remarks>MCX.EXE @ 0x00748610</remarks>
    void markSeen(vector_2d& topLeft, Vertex* vertexList, vector_3d& looker, vector_3d& lookVector, float angle,
                  float range, uint8_t who);

    /// <summary>
    /// The slope at <paramref name="pos"/> from the elevations of the four map cells around it; the face normal is
    /// written to <paramref name="normal"/> when given.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00748620</remarks>
    float terrainAngle(vector_3d& pos, vector_3d* normal);

    /// <summary>The normal of the terrain face under <paramref name="pos"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00748ae0</remarks>
    vector_3d terrainNormal(vector_3d& pos);

    /// <summary>The terrain height at <paramref name="pos"/>, interpolated across its face.</summary>
    /// <remarks>MCX.EXE @ 0x00748f80</remarks>
    float terrainElevation(vector_3d& pos);

    /// <summary>Fills a block with flat ground at the map's base elevation and tile 0x29.</summary>
    /// <remarks>MCX.EXE @ 0x00749500</remarks>
    void generateRandomBlock(PrecompVertex* block);

    /// <summary>Every block's vertices, as read from the block file.</summary>
    std::vector<uint8_t> blockData;
    /// <summary>Each block's vertices in <see cref="blockData"/> (<c>blocksMapSide * blocksMapSide + 1</c> entries).</summary>
    std::vector<PrecompVertex*> blocks; // +0x1c
    /// <summary>Per window: the block its corner was in last update (-1 initially).</summary>
    std::vector<int32_t> lastBlock; // +0x20
    /// <summary>Per window: the block its top-left corner is in.</summary>
    std::vector<int32_t> currentBlock; // +0x24
    /// <summary>Block column of the corner being built.</summary>
    int32_t topLeftBlockX = 0; // +0x28
    /// <summary>Block row of the corner being built.</summary>
    int32_t topLeftBlockY = 0; // +0x2c
    /// <summary>The <c>.elv</c> block file (closed after init; kept for getTopLeftElevation).</summary>
    PacketFile* blockFile = nullptr; // +0x30
    /// <summary>Per window: the vertex offset (x, y) of the top-left corner within its block, 8 bytes each.</summary>
    std::vector<float> vertexOffsets; // +0x34
    /// <summary>Per window: block-step counter bumped by buildWindow as the corner crosses blocks.</summary>
    std::vector<int32_t> blockSteps; // +0x38
};

/// <summary>
/// The storage of every terrain window's <see cref="Vertex"/> grid (<c>visibleVerticesPerSide^2</c> vertices each).
/// </summary>
/// <remarks>Original source: <c>terrain\vertex.cpp</c>, 0x24 bytes (a HeapManager in the original).</remarks>
class VertexManager
{
public:
    /// <summary>The grids' storage (zeroed, never constructed: buildWindow fills it).</summary>
    std::vector<uint8_t> storage;
    /// <summary>Per window: its vertex grid in <see cref="storage"/>.</summary>
    std::vector<Vertex*> vertexLists; // +0x20
};

/// <summary>
/// The storage of every terrain window's <see cref="TerrainBlock"/> list (<c>visibleVerticesPerSide^2</c> blocks each).
/// </summary>
/// <remarks>Original source: <c>terrain\vertex.cpp</c>, 0x24 bytes (a HeapManager in the original).</remarks>
class TerrainTileManager
{
public:
    /// <summary>The lists' storage (zeroed, never constructed: buildWindow fills it).</summary>
    std::vector<uint8_t> storage;
    /// <summary>Per window: its block list in <see cref="storage"/>.</summary>
    std::vector<TerrainBlock*> blockLists; // +0x20
};

/// <summary>
/// The terrain height at <paramref name="pos"/>, interpolated across the face (triangle) of the map cell it lies in
/// from the <c>GameMap</c> cell elevations. The body of <see cref="MapBlockManager::terrainElevation"/>.
/// </summary>
/// <remarks>
/// MCX.EXE @ 0x00748fa0. A file-local function whose name wasn't kept (no symbol); this is the port's name. It
/// doesn't use the manager.
/// </remarks>
float terrainElevationAt(vector_3d& pos);

/// <summary>
/// Port: the frame's terrain pass as a hardware renderer draws it from the map's ground mesh (built here, and again
/// when the map's data changed since). Checks that every vertex of <paramref name="vertexList"/>'s grid (projected
/// this frame) is the map's vertex at its place and lies where the mesh puts it; an error, saying which and how, when
/// one doesn't (the mesh would draw another picture than the tiles).
/// </summary>
/// <param name="minX">The corner range of the projection (a vertex outside it is clipped); also maxX, minY, maxY.</param>
std::expected<void, std::string> MCTerrainGroundFrame(const Vertex* vertexList, int32_t numVertices, int32_t numBlocks,
                                                      int32_t hazeFactor, int32_t stepX, int32_t stepY,
                                                      int32_t elevStep, int32_t minX, int32_t maxX, int32_t minY,
                                                      int32_t maxY, MCTerrainFrame& frame);

/// <summary>Port: lets go of the ground mesh (the terrain is destroyed).</summary>
void MCTerrainForgetMesh();

/// <summary>Terrain tile cache requests this mission (statistics).</summary>
extern int32_t tileCacheReqs;
/// <summary>Terrain tile cache hits.</summary>
extern int32_t tileCacheHits;
/// <summary>Terrain tile cache misses (tiles read from the packet files).</summary>
extern int32_t tileCacheMiss;
/// <summary>Terrain faces drawn (statistics).</summary>
extern int32_t numTerrainFaces;
