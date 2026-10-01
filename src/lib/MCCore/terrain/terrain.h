#pragma once

#include "lib/cvmath.h"
#include "object/baseobj.h"

class BitFlag;
class ByteFlag;
class Camera;
class MapBlockManager;
class ObjectBlockManager;
class TacticalMap;
class TerrainBlock;
class TerrainTileManager;
class TerrainTiles;
class UserHeap;
class Vertex;
class VertexManager;
struct _pane;

/// <summary>
/// The terrain as one camera sees it: that camera's visible vertex grid and block list, and the world position of
/// the grid's top-left corner. <see cref="Terrain"/> keeps <c>NumberOfWindows</c> of them; a camera takes one with
/// <see cref="Terrain::newWindow"/>.
/// </summary>
/// <remarks>
/// Original source: <c>terrain\terrain.cpp</c>, 0x34 bytes. Constructed inline by <c>Terrain::init</c> (an array
/// <c>new</c>): <see cref="windowNum"/> -1, the rest 0.
/// </remarks>
class TerrainWindow
{
public:
    TerrainWindow() = default;
    /// <summary>Calls <see cref="destroy"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0073bd30 (vector deleting destructor); slot 0</remarks>
    virtual ~TerrainWindow() { destroy(); }

    /// <summary>Allocates from <c>Terrain::terrainHeap</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0073c700</remarks>
    static void* operator new(size_t size) noexcept;
    /// <summary>Frees into <c>Terrain::terrainHeap</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0073c720 (no symbol kept)</remarks>
    static void operator delete(void* ptr);

    /// <summary>
    /// Binds the window to <paramref name="cam"/> as window <paramref name="windowNum"/>: takes that window's vertex
    /// grid and block list from the terrain's managers and builds them.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0073c740</remarks>
    int32_t init(Camera* cam, int32_t windowNum);

    /// <summary>Releases the camera and lists (the window becomes free).</summary>
    /// <remarks>MCX.EXE @ 0x0073c7d0</remarks>
    void destroy();

    /// <summary>Whether the window has a camera and that camera is active.</summary>
    /// <remarks>MCX.EXE @ 0x0073c7e0</remarks>
    int cameraIsActive();

    /// <remarks>MCX.EXE @ 0x0073c800</remarks>
    float getTerrainElevation(vector_3d& pos);

    /// <summary>
    /// The screen position of vertex <paramref name="vertexNum"/> of block <paramref name="blockNum"/> if it is in
    /// the window's grid (else -10000, -10000).
    /// </summary>
    /// <returns>Whether it was found.</returns>
    /// <remarks>MCX.EXE @ 0x0073c820</remarks>
    int getVertexScreenPos(int32_t blockNum, int32_t vertexNum, vector_2d& screenPos);

    /// <remarks>MCX.EXE @ 0x0073c890</remarks>
    void markSeen(vector_3d& looker, vector_3d& lookVector, float angle, float range, uint8_t who);

    /// <summary>Whether the camera is active and <paramref name="pos"/> lies within the window's visible grid.</summary>
    /// <remarks>MCX.EXE @ 0x0073c8c0</remarks>
    int cameraShowsPosition(vector_3d& pos);

    /// <summary>
    /// Resets the visibility bits for the new frame, follows the camera and rebuilds the vertex grid.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0073c940</remarks>
    int update(int force);

    /// <summary>
    /// Projects the vertex grid, marks what needs redrawing and draws the terrain and overlay tiles of every block.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0073ca10; slot 1</remarks>
    virtual void render(int32_t hazeFactor, uint8_t flags);

    /// <remarks>MCX.EXE @ 0x0073eb00</remarks>
    void renderHaze(int32_t hazeFactor, uint8_t flags);

    /// <summary>Draws the outline of every block (the debug terrain grid).</summary>
    /// <remarks>MCX.EXE @ 0x0073eb30</remarks>
    void drawLines();

    /// <remarks>MCX.EXE @ 0x0073eb70</remarks>
    void drawTopView();

    /// <summary>Marks every vertex of the grid on screen (debug).</summary>
    /// <remarks>MCX.EXE @ 0x0073ec00</remarks>
    void drawVertices();

    /// <summary>The camera using the window (null: the window is free).</summary>
    Camera* camera = nullptr; // +0x04
    /// <summary>The camera's active flag when the window was taken.</summary>
    int32_t cameraActive = 0; // +0x08
    /// <summary>The camera position of the last update.</summary>
    float cameraPosX = 0.0f; // +0x0c
    float cameraPosY = 0.0f; // +0x10
    float cameraPosZ = 0.0f; // +0x14
    /// <summary>The visible vertex grid (from <see cref="Terrain::vertexManager"/>).</summary>
    Vertex* vertexList = nullptr; // +0x18
    /// <summary>The visible blocks (from <see cref="Terrain::terrainTileManager"/>).</summary>
    TerrainBlock* blockList = nullptr; // +0x1c
    /// <summary>Vertices in <see cref="vertexList"/>.</summary>
    int32_t numVertices = 0; // +0x20
    /// <summary>Blocks in <see cref="blockList"/>.</summary>
    int32_t numBlocks = 0; // +0x24
    /// <summary>The window's index in the terrain's window table (-1 until init).</summary>
    int32_t windowNum = -1; // +0x28
    /// <summary>World x of the grid's top-left vertex (set by <c>MapBlockManager::update</c>).</summary>
    float topLeftX = 0.0f; // +0x2c
    /// <summary>World y of the grid's top-left vertex.</summary>
    float topLeftY = 0.0f; // +0x30
};

/// <summary>
/// The map: the terrain's configuration (<c>&lt;name&gt;.fit</c>), its block, vertex and tile caches, the fog-of-war
/// bit maps and the camera windows that draw it. One instance, <see cref="land"/>, created by the scenario.
/// </summary>
/// <remarks>
/// Original source: <c>terrain\terrain.cpp</c>, 0x30 bytes. Its caches are static members shared by the whole
/// game. The <c>.fit</c> file gives VerticesBlockSide, BlocksMapSide, MetersPerElevLevel, MetersPerVertex,
/// VisibleVerticesPerSide, NumberOfWindows, TerrainHeapSize, TerrainTileHeapSize and TerrainTileFile.
/// </remarks>
class Terrain : public BaseObject
{
public:
    /// <summary>Calls <see cref="destroy"/>.</summary>
    /// <remarks>MCX.EXE @ 0x007367e0 (vector deleting destructor); slot 3</remarks>
    ~Terrain() override { destroy(); }

    /// <summary>Clears the fields (object class 1, no windows).</summary>
    /// <remarks>MCX.EXE @ 0x0073b340; slot 1</remarks>
    void init() override;

    /// <summary>
    /// Reads <c>&lt;terrainPath&gt;&lt;fileName&gt;.fit</c> and builds everything: the terrain heap, the screen
    /// position tables, the tile cache, the fog-of-war bits, the windows, the block/vertex/tile managers, the object
    /// blocks and the tactical map.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0073b360</remarks>
    int32_t init(char* fileName);

    /// <summary>Frees everything <see cref="init(char*)"/> built.</summary>
    /// <remarks>MCX.EXE @ 0x0073bdb0; slot 2</remarks>
    void destroy() override;

    /// <summary>Updates the first window (the main camera's).</summary>
    /// <returns>1.</returns>
    /// <remarks>MCX.EXE @ 0x0073c030; slot 6</remarks>
    int32_t update() override;

    /// <summary>Window <paramref name="windowNum"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0073b320</remarks>
    TerrainWindow* getTerrainWindow(int32_t windowNum);

    /// <summary>Gives <paramref name="cam"/> the first free window that initialises.</summary>
    /// <returns>The window, or null.</returns>
    /// <remarks>MCX.EXE @ 0x0073bfa0</remarks>
    TerrainWindow* newWindow(Camera* cam);

    /// <summary>Frees the windows of <paramref name="cam"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0073c000</remarks>
    void killWindow(Camera* cam);

    /// <remarks>MCX.EXE @ 0x0073c040</remarks>
    void setOverlayTile(int32_t blockNum, int32_t vertexNum, int32_t value);
    /// <remarks>MCX.EXE @ 0x0073c060</remarks>
    int32_t getOverlayTile(int32_t blockNum, int32_t vertexNum);
    /// <remarks>MCX.EXE @ 0x0073c080</remarks>
    void setTile(int32_t blockNum, int32_t vertexNum, int32_t value);
    /// <remarks>MCX.EXE @ 0x0073c0a0</remarks>
    int32_t getTile(int32_t blockNum, int32_t vertexNum);

    /// <summary>
    /// Projects a world position to the isometric screen at full (<paramref name="screen100"/>) and half
    /// (<paramref name="screen50"/>) zoom, using the 30-degree view angle.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0073c0c0</remarks>
    void projectTerrain(vector_3d& pos, vector_2d& screen100, vector_2d& screen50);

    /// <summary>Renders the windows of <paramref name="cam"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0073c230</remarks>
    void render(int32_t hazeFactor, uint8_t flags, Camera* cam);

    /// <remarks>MCX.EXE @ 0x0073c270</remarks>
    void renderHaze(int32_t hazeFactor, uint8_t flags);
    /// <remarks>MCX.EXE @ 0x0073c2b0</remarks>
    void drawTopView();
    /// <remarks>MCX.EXE @ 0x0073c2e0</remarks>
    void drawVertices();
    /// <remarks>MCX.EXE @ 0x0073c310</remarks>
    void drawLines();

    /// <summary>The terrain height at <paramref name="pos"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0073c340</remarks>
    float getTerrainElevation(vector_3d& pos);

    /// <summary>The slope at <paramref name="pos"/> (and the face normal in <paramref name="normal"/>).</summary>
    /// <remarks>MCX.EXE @ 0x0073c360</remarks>
    float getTerrainAngle(vector_3d& pos, vector_3d* normal);

    /// <remarks>MCX.EXE @ 0x0073c380</remarks>
    vector_3d getTerrainNormal(vector_3d& pos);

    /// <summary>Updates the objects of every object block.</summary>
    /// <remarks>MCX.EXE @ 0x0073c3c0</remarks>
    void updateAllObjects();

    /// <summary>
    /// Marks the circle a looker at <paramref name="looker"/> sees as visible for <paramref name="who"/> (1: the
    /// Inner Sphere bits, else the Clan bits). Only full-circle looks (<paramref name="angle"/> 360) are handled;
    /// the radius comes from <c>visualRangeTable</c> for the highest cell elevation around the looker.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0073c3d0</remarks>
    void markSeen(vector_3d& looker, vector_3d& lookVector, float angle, float range, uint8_t who);

    /// <summary>
    /// Marks a circle of <paramref name="range"/> around <paramref name="looker"/> as visible (only for a
    /// 360-degree <paramref name="angle"/>).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0073c600</remarks>
    void markRadiusSeen(vector_3d& looker, vector_3d& lookVector, float angle, float range, uint8_t who);

    /// <summary>Does nothing in MCX.EXE.</summary>
    /// <remarks>MCX.EXE @ 0x0073c6e0</remarks>
    void flipBuffers();

    /// <summary>Does nothing in MCX.EXE.</summary>
    /// <remarks>MCX.EXE @ 0x0073c6f0</remarks>
    void copyBuffers(int32_t from, int32_t to);

    /// <summary>The camera windows (an array new of <see cref="numWindows"/>).</summary>
    TerrainWindow* windows = nullptr; // +0x14
    /// <summary>NumberOfWindows from the .fit file.</summary>
    int32_t numWindows = 0; // +0x18
    /// <summary>TerrainHeapSize from the .fit file, then the size actually reserved.</summary>
    uint32_t terrainHeapSize = 0; // +0x1c
    /// <summary>Cleared by init; not otherwise used.</summary>
    int32_t unknown20 = 0; // +0x20
    /// <summary>sin of the isometric view angle (30 degrees).</summary>
    float projectionSin = 0.0f; // +0x24
    /// <summary>cos of the isometric view angle.</summary>
    float projectionCos = 0.0f; // +0x28
    /// <summary>Cleared by init(char*); not otherwise used.</summary>
    int32_t unknown2C = 0; // +0x2c

    /// <summary>The block cache.</summary>
    static MapBlockManager* mapBlockManager;
    /// <summary>The windows' vertex grids.</summary>
    static VertexManager* vertexManager;
    /// <summary>The windows' block lists.</summary>
    static TerrainTileManager* terrainTileManager;
    /// <summary>The tactical map (the MFD) of the in-mission interface.</summary>
    static TacticalMap* terrainTacticalMap;
    /// <summary>What the Inner Sphere (the player, unless playing Clan) sees this frame, per map vertex.</summary>
    static ByteFlag* terrainVisibleBits;
    /// <summary>What the Inner Sphere has ever seen, per map vertex.</summary>
    static BitFlag* ISSeenBits;
    /// <summary>What the Clans see this frame.</summary>
    static ByteFlag* ClanVisibleBits;
    /// <summary>What the Clans have ever seen.</summary>
    static BitFlag* ClanSeenBits;
    static int32_t currentPass;
    /// <summary>Vertices along a block's side.</summary>
    static int32_t verticesBlockSide;
    /// <summary>Blocks along the map's side.</summary>
    static int32_t blocksMapSide;
    /// <summary>NumberOfWindows * visibleBlocksPerSide^2.</summary>
    static int32_t blocksToCache;
    /// <summary>blocksMapSide^2.</summary>
    static int32_t totalBlocks;
    /// <summary>Vertices along a window's side.</summary>
    static int32_t visibleVerticesPerSide;
    /// <summary>Blocks along a window's side (odd, at least 3).</summary>
    static int32_t visibleBlocksPerSide;
    /// <summary>Meters per elevation level.</summary>
    static float metersPerElevLevel;
    /// <summary>Meters between two vertices.</summary>
    static float metersPerVertex;
    static float OneOvermetersPerVertex;
    static float OneOververticesBlockSide;
    /// <summary>Half the vertices along the map's side.</summary>
    static int32_t verticesMapSide;
    /// <summary>metersPerVertex / 3 (map cells per vertex).</summary>
    static float metersPerVertexDivMAPCELL_DIM;
    /// <summary>Meters along a block's side.</summary>
    static float metersBlockSide;
    /// <summary>The heap of every terrain allocation.</summary>
    static UserHeap* terrainHeap;
    static _pane* terrainPane;
    /// <summary>The terrain's name (the .fit file's base name).</summary>
    static char* terrainName;
    /// <summary>Screen x of every map vertex (by blockOffsets[block] + vertex), 0x11111111 when unknown.</summary>
    static int32_t* screenPosX;
    /// <summary>Screen y of every map vertex.</summary>
    static int32_t* screenPosY;
    /// <summary>Index of each block's first vertex in screenPosX/screenPosY.</summary>
    static int32_t* blockOffsets;
    /// <summary>Set when the whole view must be redrawn.</summary>
    static int forceRedraw;
    /// <summary>The map's top-left corner at full zoom, in screen space.</summary>
    static vector_2d mapTopLeft2d100;
    /// <summary>The map's top-left corner (x = -side/2, y = +side/2, z = the first vertex's elevation).</summary>
    static vector_3d mapTopLeft3d100;
    /// <summary>The map's top-left corner at half zoom, in screen space.</summary>
    static vector_2d mapTopLeft2d50;
    /// <summary>Half of <see cref="mapTopLeft3d100"/>.</summary>
    static vector_3d mapTopLeft3d50;
};

/// <summary>Reveals the whole map (the cheat): a 10000 m circle around the origin.</summary>
/// <remarks>MCX.EXE @ 0x0073b1f0</remarks>
void RevealAll();

/// <summary>Adds block <paramref name="blockNum"/> to <see cref="usedBlockList"/> unless it is there.</summary>
/// <remarks>MCX.EXE @ 0x0073b240</remarks>
void addBlockToList(int32_t blockNum);

/// <summary>Adds block <paramref name="blockNum"/> to <see cref="moverBlockList"/> unless it is there.</summary>
/// <remarks>MCX.EXE @ 0x0073b280</remarks>
void addMoverToList(int32_t blockNum);

/// <summary>Fills <see cref="usedBlockList"/> with -1.</summary>
/// <remarks>MCX.EXE @ 0x0073b2c0</remarks>
void clearList();

/// <summary>Fills <see cref="moverBlockList"/> with -1.</summary>
/// <remarks>MCX.EXE @ 0x0073b2f0</remarks>
void clearMoverList();

/// <summary>Number of entries of <see cref="usedBlockList"/> and <see cref="moverBlockList"/> (0x510 bytes each).</summary>
inline constexpr int32_t MAX_BLOCK_LIST = 324;

/// <summary>The terrain.</summary>
extern Terrain* land;
/// <summary>Draw the terrain tiles (debug switch, default on).</summary>
extern int drawTerrainTiles;
/// <summary>Draw the overlay tiles (debug switch, default on).</summary>
extern int drawTerrainOverlays;
extern int useNonIntegerAdditive;
/// <summary>Draw the debug terrain grid.</summary>
extern int drawTerrainGrid;
/// <summary>Always redraw every block.</summary>
extern uint8_t forceAlways;
/// <summary>The camera's previous screen position (cleared by Terrain::init).</summary>
extern vector_2d prevPosition;
/// <summary>The blocks visible this frame (-1 terminated).</summary>
extern int32_t usedBlockList[MAX_BLOCK_LIST];
/// <summary>The blocks movers are in this frame (-1 terminated).</summary>
extern int32_t moverBlockList[MAX_BLOCK_LIST];
/// <summary>Bytes of the block lists (0x510 once set).</summary>
extern uint32_t blockMemSize;
extern int projectAll;

/// <summary>The object blocks of the map (DAT_00809ee4; the original's name wasn't kept).</summary>
extern ObjectBlockManager* objBlockManager;
/// <summary>The terrain tile cache (DAT_00809ee8; the original's name wasn't kept).</summary>
extern TerrainTiles* terrainTiles;
/// <summary>verticesBlockSide^2 (DAT_00809f1c; the original's name wasn't kept).</summary>
extern int32_t verticesPerBlock;
/// <summary>Meters along the map's side (DAT_00809f40; the original's name wasn't kept).</summary>
extern float worldUnitsMapSide;
