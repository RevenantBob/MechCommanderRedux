#pragma once

#include "lib/cvmath.h"
#include "object/baseobj.h"

class MCBitFlag;
class MCByteFlag;
class MCCamera;
class MCMapBlockManager;
class MCObjectBlockManager;
class MCTacticalMap;
class MCTerrainBlock;
class MCTerrainTileManager;
class MCTerrainTiles;
class MCVertex;
class MCVertexManager;
struct MCPane;

/// <summary>
/// The terrain as one camera sees it: that camera's visible vertex grid and block list, and the world position of
/// the grid's top-left corner. <see cref="MCTerrain"/> keeps <c>NumberOfWindows</c> of them; a camera takes one with
/// <see cref="MCTerrain::NewWindow"/>.
/// </summary>
/// <remarks>
/// Original source: <c>terrain\terrain.cpp</c>, 0x34 bytes. Constructed inline by <c>Terrain::init</c> (an array
/// <c>new</c>): <see cref="WindowNum"/> -1, the rest 0.
/// </remarks>
class MCTerrainWindow
{
public:
    MCTerrainWindow() = default;
    /// <summary>Calls <see cref="Destroy"/>.</summary>
    virtual ~MCTerrainWindow() { Destroy(); }

    /// <summary>
    /// Binds the window to <paramref name="cam"/> as window <paramref name="windowNum"/>: takes that window's vertex
    /// grid and block list from the terrain's managers and builds them.
    /// </summary>
    int32_t Init(MCCamera* cam, int32_t windowNum);

    /// <summary>Releases the camera and lists (the window becomes free).</summary>
    void Destroy();

    /// <summary>Whether the window has a camera and that camera is active.</summary>
    int CameraIsActive();

    float GetTerrainElevation(MCVector3D& pos);

    /// <summary>
    /// The screen position of vertex <paramref name="vertexNum"/> of block <paramref name="blockNum"/> if it is in
    /// the window's grid (else -10000, -10000).
    /// </summary>
    /// <returns>Whether it was found.</returns>
    int GetVertexScreenPos(int32_t blockNum, int32_t vertexNum, MCVector2D& screenPos);

    void MarkSeen(MCVector3D& looker, MCVector3D& lookVector, float angle, float range, uint8_t who);

    /// <summary>Whether the camera is active and <paramref name="pos"/> lies within the window's visible grid.</summary>
    int CameraShowsPosition(MCVector3D& pos);

    /// <summary>
    /// Resets the visibility bits for the new frame, follows the camera and rebuilds the vertex grid.
    /// </summary>
    int Update(int force);

    /// <summary>
    /// Projects the vertex grid, marks what needs redrawing and draws the terrain and overlay tiles of every block.
    /// </summary>
    virtual void Render(int32_t hazeFactor, uint8_t flags);

    void RenderHaze(int32_t hazeFactor, uint8_t flags);

    /// <summary>Draws the outline of every block (the debug terrain grid).</summary>
    void DrawLines();

    void DrawTopView();

    /// <summary>Marks every vertex of the grid on screen (debug).</summary>
    void DrawVertices();

    /// <summary>The camera using the window (null: the window is free).</summary>
    MCCamera* Camera = nullptr;
    /// <summary>The camera's active flag when the window was taken.</summary>
    int32_t CameraActive = 0;
    /// <summary>The camera position of the last update.</summary>
    float CameraPosX = 0.0f;
    float CameraPosY = 0.0f;
    float CameraPosZ = 0.0f;
    /// <summary>The visible vertex grid (from <see cref="MCTerrain::VertexManager"/>).</summary>
    MCVertex* VertexList = nullptr;
    /// <summary>The visible blocks (from <see cref="MCTerrain::TerrainTileManager"/>).</summary>
    MCTerrainBlock* BlockList = nullptr;
    /// <summary>Vertices in <see cref="VertexList"/>.</summary>
    int32_t NumVertices = 0;
    /// <summary>Blocks in <see cref="BlockList"/>.</summary>
    int32_t NumBlocks = 0;
    /// <summary>The window's index in the terrain's window table (-1 until init).</summary>
    int32_t WindowNum = -1;
    /// <summary>World x of the grid's top-left vertex (set by <c>MapBlockManager::update</c>).</summary>
    float TopLeftX = 0.0f;
    /// <summary>World y of the grid's top-left vertex.</summary>
    float TopLeftY = 0.0f;
};

/// <summary>
/// The map: the terrain's configuration (<c>&lt;name&gt;.fit</c>), its block, vertex and tile caches, the fog-of-war
/// bit maps and the camera windows that draw it. One instance, <see cref="Land"/>, created by the scenario.
/// </summary>
/// <remarks>
/// Original source: <c>terrain\terrain.cpp</c>, 0x30 bytes. Its caches are static members shared by the whole
/// game. The <c>.fit</c> file gives VerticesBlockSide, BlocksMapSide, MetersPerElevLevel, MetersPerVertex,
/// VisibleVerticesPerSide, NumberOfWindows, TerrainHeapSize, TerrainTileHeapSize and TerrainTileFile.
/// </remarks>
class MCTerrain : public MCBaseObject
{
public:
    /// <summary>Calls <see cref="Destroy"/>.</summary>
    ~MCTerrain() override { Destroy(); }

    /// <summary>Clears the fields (object class 1, no windows).</summary>
    void Init() override;

    /// <summary>
    /// Reads <c>&lt;terrainPath&gt;&lt;fileName&gt;.fit</c> and builds everything: the terrain heap, the screen
    /// position tables, the tile cache, the fog-of-war bits, the windows, the block/vertex/tile managers, the object
    /// blocks and the tactical map.
    /// </summary>
    int32_t Init(char* fileName);

    /// <summary>Frees everything <see cref="init(char*)"/> built.</summary>
    void Destroy() override;

    /// <summary>Updates the first window (the main camera's).</summary>
    /// <returns>1.</returns>
    int32_t Update() override;

    /// <summary>Window <paramref name="windowNum"/>.</summary>
    MCTerrainWindow* GetTerrainWindow(int32_t windowNum);

    /// <summary>Gives <paramref name="cam"/> the first free window that initialises.</summary>
    /// <returns>The window, or null.</returns>
    MCTerrainWindow* NewWindow(MCCamera* cam);

    /// <summary>Frees the windows of <paramref name="cam"/>.</summary>
    void KillWindow(MCCamera* cam);

    void SetOverlayTile(int32_t blockNum, int32_t vertexNum, int32_t value);
    int32_t GetOverlayTile(int32_t blockNum, int32_t vertexNum);
    void SetTile(int32_t blockNum, int32_t vertexNum, int32_t value);
    int32_t GetTile(int32_t blockNum, int32_t vertexNum);

    /// <summary>
    /// Projects a world position to the isometric screen at full (<paramref name="screen100"/>) and half
    /// (<paramref name="screen50"/>) zoom, using the 30-degree view angle.
    /// </summary>
    void ProjectTerrain(MCVector3D& pos, MCVector2D& screen100, MCVector2D& screen50);

    /// <summary>Renders the windows of <paramref name="cam"/>.</summary>
    void Render(int32_t hazeFactor, uint8_t flags, MCCamera* cam);

    void RenderHaze(int32_t hazeFactor, uint8_t flags);
    void DrawTopView();
    void DrawVertices();
    void DrawLines();

    /// <summary>The terrain height at <paramref name="pos"/>.</summary>
    float GetTerrainElevation(MCVector3D& pos);

    /// <summary>The slope at <paramref name="pos"/> (and the face normal in <paramref name="normal"/>).</summary>
    float GetTerrainAngle(MCVector3D& pos, MCVector3D* normal);

    MCVector3D GetTerrainNormal(MCVector3D& pos);

    /// <summary>Updates the objects of every object block.</summary>
    void UpdateAllObjects();

    /// <summary>
    /// Marks the circle a looker at <paramref name="looker"/> sees as visible for <paramref name="who"/> (1: the
    /// Inner Sphere bits, else the Clan bits). Only full-circle looks (<paramref name="angle"/> 360) are handled;
    /// the radius comes from <c>visualRangeTable</c> for the highest cell elevation around the looker.
    /// </summary>
    void MarkSeen(MCVector3D& looker, MCVector3D& lookVector, float angle, float range, uint8_t who);

    /// <summary>
    /// Marks a circle of <paramref name="range"/> around <paramref name="looker"/> as visible (only for a
    /// 360-degree <paramref name="angle"/>).
    /// </summary>
    void MarkRadiusSeen(MCVector3D& looker, MCVector3D& lookVector, float angle, float range, uint8_t who);

    /// <summary>Does nothing in MCX.EXE.</summary>
    void FlipBuffers();

    /// <summary>Does nothing in MCX.EXE.</summary>
    void CopyBuffers(int32_t from, int32_t to);

    /// <summary>The camera windows (an array new of <see cref="NumWindows"/>).</summary>
    MCTerrainWindow* Windows = nullptr;
    /// <summary>NumberOfWindows from the .fit file.</summary>
    int32_t NumWindows = 0;
    /// <summary>TerrainHeapSize from the .fit file, then the size actually reserved.</summary>
    uint32_t TerrainHeapSize = 0;
    /// <summary>sin of the isometric view angle (30 degrees).</summary>
    float ProjectionSin = 0.0f;
    /// <summary>cos of the isometric view angle.</summary>
    float ProjectionCos = 0.0f;

    /// <summary>The block cache.</summary>
    static MCMapBlockManager* MapBlockManager;
    /// <summary>The windows' vertex grids.</summary>
    static MCVertexManager* VertexManager;
    /// <summary>The windows' block lists.</summary>
    static MCTerrainTileManager* TerrainTileManager;
    /// <summary>The tactical map (the MFD) of the in-mission interface.</summary>
    static MCTacticalMap* TerrainTacticalMap;
    /// <summary>What the Inner Sphere (the player, unless playing Clan) sees this frame, per map vertex.</summary>
    static MCByteFlag* TerrainVisibleBits;
    /// <summary>What the Inner Sphere has ever seen, per map vertex.</summary>
    static MCBitFlag* ISSeenBits;
    /// <summary>What the Clans see this frame.</summary>
    static MCByteFlag* ClanVisibleBits;
    /// <summary>What the Clans have ever seen.</summary>
    static MCBitFlag* ClanSeenBits;
    static int32_t CurrentPass;
    /// <summary>Vertices along a block's side.</summary>
    static int32_t VerticesBlockSide;
    /// <summary>Blocks along the map's side.</summary>
    static int32_t BlocksMapSide;
    /// <summary>NumberOfWindows * visibleBlocksPerSide^2.</summary>
    static int32_t BlocksToCache;
    /// <summary>blocksMapSide^2.</summary>
    static int32_t TotalBlocks;
    /// <summary>Vertices along a window's side.</summary>
    static int32_t VisibleVerticesPerSide;
    /// <summary>Blocks along a window's side (odd, at least 3).</summary>
    static int32_t VisibleBlocksPerSide;
    /// <summary>Meters per elevation level.</summary>
    static float MetersPerElevLevel;
    /// <summary>Meters between two vertices.</summary>
    static float MetersPerVertex;
    static float OneOvermetersPerVertex;
    static float OneOververticesBlockSide;
    /// <summary>Half the vertices along the map's side.</summary>
    static int32_t VerticesMapSide;
    /// <summary>metersPerVertex / 3 (map cells per vertex).</summary>
    static float MetersPerVertexDivMapcellDim;
    /// <summary>Meters along a block's side.</summary>
    static float MetersBlockSide;
    static MCPane* TerrainPane;
    /// <summary>The terrain's name (the .fit file's base name).</summary>
    static char* TerrainName;
    /// <summary>Screen x of every map vertex (by blockOffsets[block] + vertex), 0x11111111 until it is projected.</summary>
    static std::vector<int32_t> ScreenPosX;
    /// <summary>Screen y of every map vertex.</summary>
    static std::vector<int32_t> ScreenPosY;
    /// <summary>Index of each block's first vertex in screenPosX/screenPosY.</summary>
    static std::vector<int32_t> BlockOffsets;
    /// <summary>Set when the whole view must be redrawn.</summary>
    static int ForceRedraw;
    /// <summary>The map's top-left corner at full zoom, in screen space.</summary>
    static MCVector2D MapTopLeft2d100;
    /// <summary>The map's top-left corner (x = -side/2, y = +side/2, z = the first vertex's elevation).</summary>
    static MCVector3D MapTopLeft3d100;
    /// <summary>The map's top-left corner at half zoom, in screen space.</summary>
    static MCVector2D MapTopLeft2d50;
    /// <summary>Half of <see cref="MapTopLeft3d100"/>.</summary>
    static MCVector3D MapTopLeft3d50;
};

/// <summary>Reveals the whole map (the cheat): a 10000 m circle around the origin.</summary>
void RevealAll();

/// <summary>Adds block <paramref name="blockNum"/> to <see cref="UsedBlockList"/> unless it is there.</summary>
void AddBlockToList(int32_t blockNum);

/// <summary>Adds block <paramref name="blockNum"/> to <see cref="MoverBlockList"/> unless it is there.</summary>
void AddMoverToList(int32_t blockNum);

/// <summary>Fills <see cref="UsedBlockList"/> with -1.</summary>
void ClearBlockList();

/// <summary>Fills <see cref="MoverBlockList"/> with -1.</summary>
void ClearMoverList();

/// <summary>Number of entries of <see cref="UsedBlockList"/> and <see cref="MoverBlockList"/> (0x510 bytes each).</summary>
inline constexpr int32_t MAX_BLOCK_LIST = 324;

/// <summary>Port: the isometric view angle (30 degrees, as MCX.EXE stores it).</summary>
inline constexpr double MCTerrainViewAngle = 0x1.0c152382d45b2p-1;
/// <summary>
/// Port: how large a world view the visible terrain grid covers, as width / cos + height / sin of the view angle in
/// pixels at camera scale 100 (0 before Terrain::init). A view's furthest zoom stays within it.
/// </summary>
extern double MCTerrainGridReach;

/// <summary>The terrain.</summary>
extern MCTerrain* Land;
/// <summary>Draw the terrain tiles (debug switch, default on).</summary>
extern int DrawTerrainTiles;
/// <summary>Draw the overlay tiles (debug switch, default on).</summary>
extern int DrawTerrainOverlays;
extern int UseNonIntegerAdditive;
/// <summary>Draw the debug terrain grid.</summary>
extern int DrawTerrainGrid;
/// <summary>Always redraw every block.</summary>
extern uint8_t ForceAlways;
/// <summary>The camera's previous screen position (cleared by Terrain::init).</summary>
extern MCVector2D PrevPosition;
/// <summary>The blocks visible this frame (-1 terminated).</summary>
extern int32_t UsedBlockList[MAX_BLOCK_LIST];
/// <summary>The blocks movers are in this frame (-1 terminated).</summary>
extern int32_t MoverBlockList[MAX_BLOCK_LIST];
/// <summary>Bytes of the block lists (0x510 once set).</summary>
extern uint32_t BlockMemSize;
extern int ProjectAll;

/// <summary>The object blocks of the map (the binary kept no name for it).</summary>
extern MCObjectBlockManager* ObjBlockManager;
/// <summary>The terrain tile cache (the binary kept no name for it).</summary>
extern MCTerrainTiles* TerrainTiles;
/// <summary>verticesBlockSide^2 (the binary kept no name for it).</summary>
extern int32_t VerticesPerBlock;
/// <summary>Meters along the map's side (the binary kept no name for it).</summary>
extern float WorldUnitsMapSide;
