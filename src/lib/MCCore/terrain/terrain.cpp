#include "stdafx.h"
#include "terrain/terrain.h"
#include "ai/move.h"
#include "camera/camera.h"
#include "engine/bitflag.h"
#include "engine/ceglist.h"
#include "engine/celine.h"
#include "gui/asystem.h"
#include "iface/iface.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/cvmath.h"
#include "lib/inifile.h"
#include "logistics/logmain.h"
#include "mission/scenario.h"
#include "object/elemntl.h"
#include "object/objblck.h"
#include "platform/MCDisplay.h"
#include "platform/MCInput.h"
#include "terrain/terrmap.h"
#include "terrain/terrtxm.h"
#include "terrain/vertex.h"
#include "vfx/vfx.h"
#include "vfx/vfxfuncs.h"

MapBlockManager* Terrain::mapBlockManager = nullptr;
VertexManager* Terrain::vertexManager = nullptr;
TerrainTileManager* Terrain::terrainTileManager = nullptr;
TacticalMap* Terrain::terrainTacticalMap = nullptr;
ByteFlag* Terrain::terrainVisibleBits = nullptr;
BitFlag* Terrain::ISSeenBits = nullptr;
ByteFlag* Terrain::ClanVisibleBits = nullptr;
BitFlag* Terrain::ClanSeenBits = nullptr;
int32_t Terrain::currentPass = 0;
int32_t Terrain::verticesBlockSide = 0;
int32_t Terrain::blocksMapSide = 0;
int32_t Terrain::blocksToCache = 0;
int32_t Terrain::totalBlocks = 0;
int32_t Terrain::visibleVerticesPerSide = 0;
double MCTerrainGridReach = 0.0;
int32_t Terrain::visibleBlocksPerSide = 0;
float Terrain::metersPerElevLevel = 0.0f;
float Terrain::metersPerVertex = 0.0f;
float Terrain::OneOvermetersPerVertex = 0.0f;
float Terrain::OneOververticesBlockSide = 0.0f;
int32_t Terrain::verticesMapSide = 0;
float Terrain::metersPerVertexDivMAPCELL_DIM = 0.0f;
float Terrain::metersBlockSide = 0.0f;
_pane* Terrain::terrainPane = nullptr;
char* Terrain::terrainName = nullptr;
std::vector<int32_t> Terrain::screenPosX;
std::vector<int32_t> Terrain::screenPosY;
std::vector<int32_t> Terrain::blockOffsets;
int Terrain::forceRedraw = 0;
vector_2d Terrain::mapTopLeft2d100;
vector_3d Terrain::mapTopLeft3d100;
vector_2d Terrain::mapTopLeft2d50;
vector_3d Terrain::mapTopLeft3d50;

Terrain* land = nullptr;
int drawTerrainTiles = 1;
int drawTerrainOverlays = 1;
int useNonIntegerAdditive = 0;
vector_2d prevPosition;
int32_t usedBlockList[MAX_BLOCK_LIST] = {};
int32_t moverBlockList[MAX_BLOCK_LIST] = {};
uint32_t blockMemSize = 0;
int projectAll = 0;
ObjectBlockManager* objBlockManager = nullptr;
TerrainTiles* terrainTiles = nullptr;
int32_t verticesPerBlock = 0;
float worldUnitsMapSide = 0.0f;

namespace
{
    /// <summary>Terrain::init could not build one of its parts.</summary>
    constexpr int32_t TERRAIN_INIT_FAILED = static_cast<int32_t>(0xbaaa0010);
    /// <summary>TerrainWindow::init: the window has no vertex grid.</summary>
    constexpr int32_t NO_VERTEX_LIST = static_cast<int32_t>(0xbaaa000e);
    /// <summary>TerrainWindow::init: the window has no block list.</summary>
    constexpr int32_t NO_BLOCK_LIST = static_cast<int32_t>(0xbaaa000f);
    /// <summary>The TerrainWindow::update result that makes init rebuild the window (never returned).</summary>
    constexpr int32_t REBUILD_WINDOW = static_cast<int32_t>(0xfabfaded);

    /// <summary>The isometric view angle (30 degrees, as MCX.EXE stores it: 0.523598775597).</summary>
    constexpr double VIEW_ANGLE = 0x1.0c152382d45b2p-1;

    /// <summary>Depth of the debug grid's lines.</summary>
    constexpr int32_t GRID_LINE_DEPTH = -78000000;

    /// <summary>The main camera's zoom factor: 0.5 zoomed out (CameraScale 1), else 1.</summary>
    float eyeZoom()
    {
        return eye->cameraScale != 1 ? 1.0f : 0.5f;
    }

    /// <summary>Projects a world point to the main camera's screen (the binary inlines this).</summary>
    vector_2d eyeProject(const vector_3d& point)
    {
        vector_3d relative = point - eye->position;
        relative *= eyeZoom();
        return vector_2d(relative.x * eye->cosAngle + relative.y * eye->cosAngle + eye->halfWidth,
                         ((relative.x * eye->sinAngle + eye->halfHeight) - relative.y * eye->sinAngle) - relative.z);
    }

    /// <summary>
    /// Adds the four sides of a quad to the element list: from <paramref name="p1"/> to <paramref name="p2"/> to
    /// <paramref name="p3"/> to <paramref name="p0"/>, and <paramref name="p1"/> to <paramref name="p0"/>.
    /// </summary>
    void addGridQuad(vector_2d& p0, vector_2d& p1, vector_2d& p2, vector_2d& p3, int32_t color)
    {
        ElementList->add(ElementPool::Make<LineElement>(p1, p2, color, nullptr, GRID_LINE_DEPTH, -1));
        ElementList->add(ElementPool::Make<LineElement>(p2, p3, color, nullptr, GRID_LINE_DEPTH, -1));
        ElementList->add(ElementPool::Make<LineElement>(p3, p0, color, nullptr, GRID_LINE_DEPTH, -1));
        ElementList->add(ElementPool::Make<LineElement>(p1, p0, color, nullptr, GRID_LINE_DEPTH, -1));
    }

    /// <summary>
    /// Projects the corners of a <paramref name="width"/> x <paramref name="height"/> rectangle whose top-left is
    /// <paramref name="origin"/>, each dropped onto the terrain, and adds its outline.
    /// </summary>
    void addTerrainQuad(TerrainWindow* window, vector_3d origin, float width, float height, int32_t color)
    {
        vector_3d point = origin;
        point.z = window->getTerrainElevation(point);
        vector_2d p0 = eyeProject(point);
        point.x += width;
        point.z = window->getTerrainElevation(point);
        vector_2d p1 = eyeProject(point);
        point.y -= height;
        point.z = window->getTerrainElevation(point);
        vector_2d p2 = eyeProject(point);
        point.x -= width;
        point.z = window->getTerrainElevation(point);
        vector_2d p3 = eyeProject(point);
        addGridQuad(p0, p1, p2, p3, color);
    }

    /// <summary>Whether (tileRow, tileCol) is on the map.</summary>
    int tileOnMap(int32_t tileRow, int32_t tileCol)
    {
        return (tileRow >= 0 && tileRow < GameMap->height && tileCol >= 0 && tileCol < GameMap->width) ? 1 : 0;
    }

    /// <summary>
    /// The debug grid for one map tile: each of its 3x3 cells outlined when impassable (0xfd), when its second
    /// passability bit is clear (0xfc) and when path-locked (0xfe).
    /// </summary>
    void drawCellGrid(TerrainWindow* window, int32_t tileRow, int32_t tileCol, int32_t col, int32_t row,
                      float vertexStep)
    {
        const float cellSize = static_cast<float>(Terrain::metersPerVertexDivMAPCELL_DIM - 5.0);
        Assert(tileOnMap(tileRow, tileCol), 0, " terrwindow:render MapTile Out of Bounds ");
        Assert(tileOnMap(tileRow, tileCol), 0, " Map Tile out of bounds ");
        const MapTile& tile = GameMap->map[GameMap->width * tileRow + tileCol];
        const uint32_t cells = tile.cells;
        const uint32_t overlay = tile.overlay;

        uint32_t cellShift = 0;
        uint32_t lockShift = 0;

        for (int32_t cellR = 0; lockShift < 9; cellR++)
        {
            for (int32_t cellC = 0; cellC < 3; cellC++)
            {
                const vector_3d origin(
                    static_cast<float>(cellC) * cellSize + static_cast<float>(col) * vertexStep + window->topLeftX,
                    (window->topLeftY - static_cast<float>(row) * vertexStep) - static_cast<float>(cellR) * cellSize,
                    0.0f);

                if (((0x4000u << cellShift) & cells) >> (cellShift + 14) == 0)
                {
                    addTerrainQuad(window, origin, cellSize, cellSize, 0xfd);
                }

                if (((0x8000u << cellShift) & cells) >> (cellShift + 15) == 0)
                {
                    addTerrainQuad(window, origin, cellSize, cellSize, 0xfc);
                }

                if (((0x8000u << lockShift) & overlay) >> (lockShift + 15) != 0)
                {
                    addTerrainQuad(window, origin, cellSize, cellSize, 0xfe);
                }

                cellShift += 2;
                lockShift++;
            }
        }
    }

    /// <summary>The debug grid's global-map doors on one map tile, outlined in 0xea along their length.</summary>
    void drawDoors(TerrainWindow* window, int32_t tileRow, int32_t tileCol, int32_t col, int32_t row, float vertexStep)
    {
        const float cellSize = static_cast<float>(Terrain::metersPerVertexDivMAPCELL_DIM - 5.0);
        Assert(tileOnMap(tileRow, tileCol), 0, " terrwindow:render MapTile Out of Bounds ");

        for (int32_t i = 0; i < GlobalMoveMap->numDoors; i++)
        {
            const GlobalMapDoor& door = GlobalMoveMap->doors[i];

            if (door.row != tileRow || door.col != tileCol)
            {
                continue;
            }

            int32_t lengthX = 1;
            int32_t lengthY = 1;

            if (door.direction[0] == 1)
            {
                lengthY = door.length;
            }

            if (door.direction[0] == 2)
            {
                lengthX = door.length;
            }

            const vector_3d origin(
                static_cast<float>(door.cellC) * cellSize + static_cast<float>(col) * vertexStep + window->topLeftX,
                (window->topLeftY - static_cast<float>(row) * vertexStep) - static_cast<float>(door.cellR) * cellSize,
                0.0f);
            addTerrainQuad(window, origin, static_cast<float>(lengthX) * cellSize,
                           static_cast<float>(lengthY) * cellSize, 0xea);
        }
    }

    /// <summary>
    /// The camera's screen position of the terrain point <paramref name="point"/> and how far the view scrolled
    /// since the last frame (from the camera's projected corners at its zoom).
    /// </summary>
    void screenFromCamera(vector_3d& point, vector_2d& screen, int32_t& scrollX, int32_t& scrollY)
    {
        vector_2d screen100;
        vector_2d screen50;
        land->projectTerrain(point, screen100, screen50);

        if (eye->cameraScale == 1)
        {
            screen.x = (screen50.x - eye->screenUL50.x) + eye->halfWidth;
            screen.y = (screen50.y - eye->screenUL50.y) + eye->halfHeight;
            scrollX = static_cast<int32_t>(std::floor(eye->lastScreenUL50.x - eye->screenUL50.x));
            scrollY = static_cast<int32_t>(std::floor(eye->lastScreenUL50.y - eye->screenUL50.y));
        }
        else
        {
            screen.x = (screen100.x - eye->screenUL.x) + eye->halfWidth;
            screen.y = (screen100.y - eye->screenUL.y) + eye->halfHeight;
            scrollX = static_cast<int32_t>(std::floor(eye->lastScreenUL.x - eye->screenUL.x));
            scrollY = static_cast<int32_t>(std::floor(eye->lastScreenUL.y - eye->screenUL.y));
        }
    }
}

auto RevealAll() -> void
{
    if (land == nullptr)
    {
        return;
    }

    vector_3d origin(0.0f, 0.0f, 0.0f);
    vector_3d lookVector;
    land->markRadiusSeen(origin, lookVector, 360.0f, 10000.0f, 1);
}

auto addBlockToList(int32_t blockNum) -> void
{
    for (int32_t i = 0; i < MAX_BLOCK_LIST; i++)
    {
        if (usedBlockList[i] == blockNum)
        {
            return;
        }

        if (usedBlockList[i] == -1)
        {
            usedBlockList[i] = blockNum;
            return;
        }
    }
}

auto addMoverToList(int32_t blockNum) -> void
{
    for (int32_t i = 0; i < MAX_BLOCK_LIST; i++)
    {
        if (moverBlockList[i] == blockNum)
        {
            return;
        }

        if (moverBlockList[i] == -1)
        {
            moverBlockList[i] = blockNum;
            return;
        }
    }
}

auto clearList() -> void
{
    if (blockMemSize == 0)
    {
        blockMemSize = sizeof(usedBlockList);
    }

    std::memset(usedBlockList, 0xff, blockMemSize);
}

auto clearMoverList() -> void
{
    if (blockMemSize == 0)
    {
        blockMemSize = sizeof(moverBlockList);
    }

    std::memset(moverBlockList, 0xff, blockMemSize);
}

auto Terrain::getTerrainWindow(int32_t windowNum) -> TerrainWindow*
{
    return &windows[windowNum];
}

auto Terrain::init() -> void
{
    partId = -1;
    idNumber = 0;
    next = nullptr;
    windows = nullptr;
    numWindows = 0;
    objectClass = static_cast<ObjectClass>(1);
}

auto Terrain::init(char* fileName) -> int32_t
{
    clearList();
    clearMoverList();
    const size_t nameLength = std::strlen(fileName);
    terrainName = new char[nameLength + 1];
    std::strncpy(terrainName, fileName, nameLength);
    terrainName[nameLength] = '\0';

    FullPathFileName fitName;
    fitName.init(terrainPath, fileName, ".fit");
    FitIniFile terrainFile;
    int32_t result = terrainFile.open(fitName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = terrainFile.seekBlock("TerrainData")) != 0)
    {
        return result;
    }

    if ((result = terrainFile.readIdLong("VerticesBlockSide", verticesBlockSide)) != 0)
    {
        return result;
    }

    if ((result = terrainFile.readIdLong("BlocksMapSide", blocksMapSide)) != 0)
    {
        return result;
    }

    if ((result = terrainFile.readIdFloat("MetersPerElevLevel", metersPerElevLevel)) != 0)
    {
        return result;
    }

    if ((result = terrainFile.readIdFloat("MetersPerVertex", metersPerVertex)) != 0)
    {
        return result;
    }

    if ((result = terrainFile.readIdLong("VisibleVerticesPerSide", visibleVerticesPerSide)) != 0)
    {
        return result;
    }

    {
        // Port fix: every map's grid is 30 vertices, made for 640x480. Zoomed out at 1280x1024 it no longer reaches
        // the screen's corners (black, jagged edges, and a tactical map view box that jumps when a corner lands on
        // no terrain). Grow it with the screen: the grid is a diamond, so what it has to cover grows with
        // width / cos + height / sin of the view angle. Kept even, like the data's. The screen follows the window,
        // which can grow during the scenario, so the grid covers the largest desktop.
        int32_t screenWidth = application->width();
        int32_t screenHeight = application->height();

        if (const MCDisplay* display = MCInput::Display(); display != nullptr)
        {
            int32_t largestWidth = 0;
            int32_t largestHeight = 0;
            display->LargestScreenSize(largestWidth, largestHeight);
            screenWidth = std::max(screenWidth, largestWidth);
            screenHeight = std::max(screenHeight, largestHeight);
        }

        const double sinAngle = std::sin(VIEW_ANGLE);
        const double cosAngle = std::cos(VIEW_ANGLE);
        // The world view's surface is at most 2160 tall (viewWindow::ZoomFurthest), at the widest desktop's aspect.
        const double widest = std::max(static_cast<double>(screenWidth) / screenHeight, 16.0 / 9.0);
        const double furthest = static_cast<double>(viewWindow::ZoomFurthest);
        const double needed = std::max(static_cast<double>(screenWidth) / cosAngle + screenHeight / sinAngle,
                                       furthest * widest / cosAngle + furthest / sinAngle);
        const double designed = 640.0 / cosAngle + 480.0 / sinAngle;
        const int32_t dataVertices = visibleVerticesPerSide;
        const auto grown = static_cast<int32_t>(std::ceil(visibleVerticesPerSide * needed / designed));

        if (grown > visibleVerticesPerSide)
        {
            visibleVerticesPerSide = (grown + 1) & ~1;
        }

        MCTerrainGridReach = designed * visibleVerticesPerSide / dataVertices;
    }

    if ((result = terrainFile.readIdLong("NumberOfWindows", numWindows)) != 0)
    {
        return result;
    }

    if ((result = terrainFile.readIdULong("TerrainHeapSize", terrainHeapSize)) != 0)
    {
        return result;
    }

    if ((result = terrainFile.seekBlock("TileData")) != 0)
    {
        return result;
    }

    // The tile heap's size is still read (and required), then ignored.
    uint32_t tileHeapSize = 0;

    if ((result = terrainFile.readIdULong("TerrainTileHeapSize", tileHeapSize)) != 0)
    {
        return result;
    }

    char tileFileName[80];

    if ((result = terrainFile.readIdString("TerrainTileFile", tileFileName, 79)) != 0)
    {
        return result;
    }

    totalBlocks = blocksMapSide * blocksMapSide;
    verticesPerBlock = verticesBlockSide * verticesBlockSide;
    const int32_t mapVertices = totalBlocks * verticesBlockSide * verticesBlockSide;
    screenPosX.assign(static_cast<size_t>(mapVertices), 0x11111111);
    screenPosY.assign(static_cast<size_t>(mapVertices), 0);
    blockOffsets.assign(static_cast<size_t>(totalBlocks), 0);

    for (int32_t i = 0, offset = 0; i < totalBlocks; i++, offset += verticesBlockSide * verticesBlockSide)
    {
        blockOffsets[i] = offset;
    }

    terrainTiles = new TerrainTiles;

    if ((result = terrainTiles->init(tileFileName)) != 0)
    {
        return result;
    }

    terrainTiles->preload(fileName);

    visibleBlocksPerSide = static_cast<int32_t>(
        std::floor(static_cast<double>(visibleVerticesPerSide) / static_cast<double>(verticesBlockSide) + 0.5));

    if ((visibleBlocksPerSide & 1) == 0)
    {
        visibleBlocksPerSide++;
    }

    if (visibleBlocksPerSide < 3)
    {
        visibleBlocksPerSide = 3;
    }

    const uint32_t flagSide = static_cast<uint32_t>(blocksMapSide * verticesBlockSide);
    metersBlockSide = static_cast<float>(verticesBlockSide) * metersPerVertex;
    worldUnitsMapSide = static_cast<float>(blocksMapSide) * metersBlockSide;
    OneOvermetersPerVertex = 1.0f / metersPerVertex;
    blocksToCache = numWindows * visibleBlocksPerSide * visibleBlocksPerSide;
    metersPerVertexDivMAPCELL_DIM = metersPerVertex * (1.0f / 3.0f);
    OneOververticesBlockSide = 1.0f / static_cast<float>(verticesBlockSide);
    verticesMapSide = static_cast<int32_t>(flagSide) >> 1;
    mapTopLeft3d100.x = worldUnitsMapSide * -0.5f;
    mapTopLeft3d100.y = worldUnitsMapSide * 0.5f;

    terrainVisibleBits = new ByteFlag();

    if (terrainVisibleBits == nullptr)
    {
        return TERRAIN_INIT_FAILED;
    }

    ISSeenBits = new BitFlag();

    if (ISSeenBits == nullptr)
    {
        return TERRAIN_INIT_FAILED;
    }

    ISSeenBits->divValue = 1;
    ISSeenBits->colWidth = 1;
    ClanVisibleBits = new ByteFlag();

    if (ClanVisibleBits == nullptr)
    {
        return TERRAIN_INIT_FAILED;
    }

    ClanSeenBits = new BitFlag();

    if (ClanSeenBits == nullptr)
    {
        return TERRAIN_INIT_FAILED;
    }

    ClanSeenBits->divValue = 1;
    ClanSeenBits->colWidth = 1;
    terrainVisibleBits->init(flagSide, flagSide, 0);
    ISSeenBits->init(flagSide, flagSide, 0);
    ClanVisibleBits->init(flagSide, flagSide, 0);
    ClanSeenBits->init(flagSide, flagSide, 0);

    windows = new TerrainWindow[numWindows];

    mapBlockManager = new MapBlockManager;

    if (mapBlockManager->init(fileName, totalBlocks, verticesBlockSide * verticesBlockSide * 8) != 0)
    {
        return TERRAIN_INIT_FAILED;
    }

    mapTopLeft3d100.z = mapBlockManager->getTopLeftElevation();

    // Each window's vertex grid, after a table of pointers to them.
    const int32_t vertexListSize =
        visibleVerticesPerSide * visibleVerticesPerSide * static_cast<int32_t>(sizeof(Vertex));
    vertexManager = new VertexManager;

    if (vertexManager->vertexLists.empty())
    {
        // Zeroed raw storage, as the original's committed heap: the grids are filled by buildWindow.
        vertexManager->storage.assign(static_cast<size_t>(vertexListSize) * numWindows, 0);
        vertexManager->vertexLists.resize(static_cast<size_t>(numWindows));

        for (int32_t i = 0; i < numWindows; i++)
        {
            vertexManager->vertexLists[i] =
                reinterpret_cast<Vertex*>(vertexManager->storage.data() + static_cast<size_t>(i) * vertexListSize);
        }
    }

    const int32_t blockListSize =
        visibleVerticesPerSide * visibleVerticesPerSide * static_cast<int32_t>(sizeof(TerrainBlock));
    terrainTileManager = new TerrainTileManager;

    if (terrainTileManager->blockLists.empty())
    {
        terrainTileManager->storage.assign(static_cast<size_t>(blockListSize) * numWindows, 0);
        terrainTileManager->blockLists.resize(static_cast<size_t>(numWindows));

        for (int32_t i = 0; i < numWindows; i++)
        {
            terrainTileManager->blockLists[i] = reinterpret_cast<TerrainBlock*>(terrainTileManager->storage.data() +
                                                                                static_cast<size_t>(i) * blockListSize);
        }
    }

    objBlockManager = new ObjectBlockManager;

    if (objBlockManager->init(fileName) != 0)
    {
        return TERRAIN_INIT_FAILED;
    }

    terrainTacticalMap = new TacticalMap;

    if (terrainTacticalMap->init(0, 0) != 0)
    {
        return TERRAIN_INIT_FAILED;
    }

    theInterface->tacticalMap = terrainTacticalMap;
    screenWindow->addChild(terrainTacticalMap);
    terrainTacticalMap->RefreshPage();
    terrainFile.close();

    const float sinAngle = static_cast<float>(std::sin(VIEW_ANGLE));
    prevPosition.y = 0.0f;
    prevPosition.x = 0.0f;
    const float cosAngle = static_cast<float>(std::cos(VIEW_ANGLE));
    projectionSin = sinAngle;
    projectionCos = cosAngle;
    mapTopLeft2d100.x = mapTopLeft3d100.x * cosAngle + mapTopLeft3d100.y * cosAngle;
    mapTopLeft2d100.y = mapTopLeft3d100.x * sinAngle - mapTopLeft3d100.y * sinAngle;
    mapTopLeft3d50.x = mapTopLeft3d100.x * 0.5f;
    mapTopLeft3d50.y = mapTopLeft3d100.y * 0.5f;
    mapTopLeft3d50.z = mapTopLeft3d100.z * 0.5f;
    mapTopLeft2d50.x = mapTopLeft3d50.x * cosAngle + mapTopLeft3d50.y * cosAngle;
    mapTopLeft2d50.y = mapTopLeft3d50.x * sinAngle - mapTopLeft3d50.y * sinAngle;
    return 0;
}

auto Terrain::destroy() -> void
{
    MCTerrainForgetMesh();

    if (terrainTiles != nullptr)
    {
        // Faithful: destroyed twice (the second finds nothing left).
        terrainTiles->destroy();
        terrainTiles->destroy();
        delete terrainTiles;
        terrainTiles = nullptr;
    }

    delete[] windows;
    windows = nullptr;

    if (mapBlockManager != nullptr)
    {
        mapBlockManager->destroy();
        delete mapBlockManager;
    }

    mapBlockManager = nullptr;

    if (objBlockManager != nullptr)
    {
        objBlockManager->destroy();
        delete objBlockManager;
    }

    objBlockManager = nullptr;

    if (vertexManager != nullptr)
    {
        vertexManager->storage = {};
        vertexManager->vertexLists = {};
        delete vertexManager;
    }

    vertexManager = nullptr;

    if (terrainTileManager != nullptr)
    {
        terrainTileManager->storage = {};
        terrainTileManager->blockLists = {};
        delete terrainTileManager;
    }

    terrainTileManager = nullptr;

    if (terrainVisibleBits != nullptr)
    {
        terrainVisibleBits->destroy();
        delete terrainVisibleBits;
    }

    terrainVisibleBits = nullptr;

    if (ISSeenBits != nullptr)
    {
        ISSeenBits->destroy();
        delete ISSeenBits;
    }

    ISSeenBits = nullptr;

    if (ClanVisibleBits != nullptr)
    {
        ClanVisibleBits->destroy();
        delete ClanVisibleBits;
    }

    ClanVisibleBits = nullptr;

    if (ClanSeenBits != nullptr)
    {
        ClanSeenBits->destroy();
        delete ClanSeenBits;
    }

    ClanSeenBits = nullptr;

    if (terrainTacticalMap != nullptr)
    {
        terrainTacticalMap->destroy();
        delete terrainTacticalMap;
        terrainTacticalMap = nullptr;
    }

    screenPosX = {};
    screenPosY = {};
    blockOffsets = {};

    delete[] terrainName;
    terrainName = nullptr;
}

auto Terrain::newWindow(Camera* cam) -> TerrainWindow*
{
    for (int32_t i = 0; i < numWindows; i++)
    {
        if (windows[i].camera == nullptr && windows[i].init(cam, i) == 0)
        {
            return &windows[i];
        }
    }

    return nullptr;
}

auto Terrain::killWindow(Camera* cam) -> void
{
    for (int32_t i = 0; i < numWindows; i++)
    {
        if (windows[i].camera == cam)
        {
            windows[i].destroy();
        }
    }
}

auto Terrain::update() -> int32_t
{
    windows[0].update(0);
    return 1;
}

auto Terrain::setOverlayTile(int32_t blockNum, int32_t vertexNum, int32_t value) -> void
{
    mapBlockManager->setOverlayTile(blockNum, vertexNum, value);
}

auto Terrain::getOverlayTile(int32_t blockNum, int32_t vertexNum) -> int32_t
{
    return mapBlockManager->getOverlayTile(blockNum, vertexNum);
}

auto Terrain::setTile(int32_t blockNum, int32_t vertexNum, int32_t value) -> void
{
    mapBlockManager->setTile(blockNum, vertexNum, value);
}

auto Terrain::getTile(int32_t blockNum, int32_t vertexNum) -> int32_t
{
    return mapBlockManager->getTile(blockNum, vertexNum);
}

auto Terrain::projectTerrain(vector_3d& pos, vector_2d& screen100, vector_2d& screen50) -> void
{
    const float fromTop = mapTopLeft3d100.y - pos.y;
    const float fromLeft = pos.x - mapTopLeft3d100.x;
    screen100.x = (fromLeft - fromTop) * projectionCos;
    screen100.x = static_cast<float>(static_cast<int32_t>(std::floor(screen100.x))) + mapTopLeft2d100.x;
    screen100.y = (fromLeft + fromTop) * projectionSin;
    const int32_t y100 = static_cast<int32_t>(std::floor(screen100.y));
    screen100.y = static_cast<float>(y100);
    const int32_t z100 = static_cast<int32_t>(std::floor(pos.z));
    screen100.y = (mapTopLeft2d100.y - static_cast<float>(z100)) + static_cast<float>(y100);

    const float halfZ = pos.z * 0.5f;
    const float fromTop50 = mapTopLeft3d50.y - pos.y * 0.5f;
    const float fromLeft50 = pos.x * 0.5f - mapTopLeft3d50.x;
    screen50.x = (fromLeft50 - fromTop50) * projectionCos;
    screen50.x = static_cast<float>(static_cast<int32_t>(std::floor(screen50.x))) + mapTopLeft2d50.x;
    screen50.y = (fromLeft50 + fromTop50) * projectionSin;
    const int32_t y50 = static_cast<int32_t>(std::floor(screen50.y));
    screen50.y = static_cast<float>(y50);
    const int32_t z50 = static_cast<int32_t>(std::floor(halfZ));
    screen50.y = (mapTopLeft2d50.y - static_cast<float>(z50)) + static_cast<float>(y50);
}

auto Terrain::render(int32_t hazeFactor, uint8_t flags, Camera* cam) -> void
{
    for (int32_t i = 0; i < numWindows; i++)
    {
        if (windows[i].camera == cam)
        {
            windows[i].render(hazeFactor, flags);
        }
    }
}

auto Terrain::renderHaze(int32_t hazeFactor, uint8_t flags) -> void
{
    for (int32_t i = 0; i < numWindows; i++)
    {
        if (windows[i].cameraIsActive() != 0)
        {
            windows[i].renderHaze(hazeFactor, flags);
        }
    }
}

auto Terrain::drawTopView() -> void
{
    for (int32_t i = 0; i < numWindows; i++)
    {
        if (windows[i].cameraIsActive() != 0)
        {
            windows[i].drawTopView();
        }
    }
}

auto Terrain::drawVertices() -> void
{
    for (int32_t i = 0; i < numWindows; i++)
    {
        if (windows[i].cameraIsActive() != 0)
        {
            windows[i].drawVertices();
        }
    }
}

auto Terrain::drawLines() -> void
{
    for (int32_t i = 0; i < numWindows; i++)
    {
        if (windows[i].cameraIsActive() != 0)
        {
            windows[i].drawLines();
        }
    }
}

auto Terrain::getTerrainElevation(vector_3d& pos) -> float
{
    return mapBlockManager->terrainElevation(pos);
}

auto Terrain::getTerrainAngle(vector_3d& pos, vector_3d* normal) -> float
{
    return mapBlockManager->terrainAngle(pos, normal);
}

auto Terrain::getTerrainNormal(vector_3d& pos) -> vector_3d
{
    return mapBlockManager->terrainNormal(pos);
}

auto Terrain::updateAllObjects() -> void
{
    objBlockManager->updateAllObjects();
}

auto Terrain::markSeen(vector_3d& looker, vector_3d& /*lookVector*/, float angle, float /*range*/, uint8_t who) -> void
{
    if (angle != 360.0f)
    {
        return;
    }

    const float fromTop = mapTopLeft3d100.y - looker.y;
    const double gridX = std::floor(OneOvermetersPerVertex * (looker.x - mapTopLeft3d100.x));
    const double gridY = std::floor(OneOvermetersPerVertex * fromTop);
    const int32_t col = static_cast<int32_t>(std::floor(static_cast<float>(gridX)));
    const int32_t row = static_cast<int32_t>(std::floor(static_cast<float>(gridY)));

    Assert(tileOnMap(row, col), 0, " Map Tile out of bounds ");
    const uint32_t cellsA = GameMap->map[GameMap->width * row + col].cells;
    Assert(tileOnMap(row, col + 1), 0, " Map Tile out of bounds ");
    const uint32_t cellsB = GameMap->map[GameMap->width * row + col + 1].cells;
    Assert(tileOnMap(row + 1, col + 1), 0, " Map Tile out of bounds ");
    const uint32_t cellsC = GameMap->map[GameMap->width * (row + 1) + col + 1].cells;
    Assert(tileOnMap(row + 1, col), 0, " Map Tile out of bounds ");
    const uint32_t cellsD = GameMap->map[GameMap->width * (row + 1) + col].cells;

    // The highest of the four corners sets the sight radius.
    const uint32_t level =
        std::max({(cellsA >> 7) & 0x3f, (cellsB >> 7) & 0x3f, (cellsC >> 7) & 0x3f, (cellsD >> 7) & 0x3f});
    ByteFlag* visibleBits = who == 1 ? terrainVisibleBits : ClanVisibleBits;
    visibleBits->setCircle(static_cast<uint32_t>(col), static_cast<uint32_t>(row),
                           static_cast<uint32_t>(visualRangeTable[level]));
}

auto Terrain::markRadiusSeen(vector_3d& looker, vector_3d& /*lookVector*/, float angle, float range, uint8_t who)
    -> void
{
    if (angle != 360.0f)
    {
        return;
    }

    const float radius = static_cast<float>(static_cast<double>(3.34f) * range * OneOvermetersPerVertex);
    const float fromTop = mapTopLeft3d100.y - looker.y;
    const double gridX = std::floor(OneOvermetersPerVertex * (static_cast<double>(looker.x) - mapTopLeft3d100.x));
    const double gridY = std::floor(static_cast<double>(OneOvermetersPerVertex) * fromTop);
    const int32_t col = static_cast<int32_t>(std::floor(static_cast<float>(gridX)));
    const int32_t row = static_cast<int32_t>(std::floor(static_cast<float>(gridY)));
    ByteFlag* visibleBits = who == 1 ? terrainVisibleBits : ClanVisibleBits;
    visibleBits->setCircle(static_cast<uint32_t>(col), static_cast<uint32_t>(row),
                           static_cast<uint32_t>(static_cast<int32_t>(radius)));
}

auto Terrain::flipBuffers() -> void
{
}

auto Terrain::copyBuffers(int32_t /*from*/, int32_t /*to*/) -> void
{
}

auto TerrainWindow::init(Camera* cam, int32_t newWindowNum) -> int32_t
{
    camera = cam;
    cameraActive = cam->active;
    vertexList = Terrain::vertexManager->vertexLists[newWindowNum];

    if (vertexList == nullptr)
    {
        return NO_VERTEX_LIST;
    }

    blockList = Terrain::terrainTileManager->blockLists[newWindowNum];

    if (blockList == nullptr)
    {
        return NO_BLOCK_LIST;
    }

    windowNum = newWindowNum;
    int32_t result = update(1);

    if (result == REBUILD_WINDOW)
    {
        Terrain::mapBlockManager->buildWindow(vertexList, &numVertices, blockList, &numBlocks, windowNum);
        result = 0;
    }

    return result;
}

auto TerrainWindow::destroy() -> void
{
    camera = nullptr;
    cameraActive = 0;
    vertexList = nullptr;
    blockList = nullptr;
}

auto TerrainWindow::cameraIsActive() -> int
{
    if (camera != nullptr)
    {
        return camera->active;
    }

    return 0;
}

auto TerrainWindow::getTerrainElevation(vector_3d& pos) -> float
{
    return Terrain::mapBlockManager->terrainElevation(pos);
}

auto TerrainWindow::getVertexScreenPos(int32_t blockNum, int32_t vertexNum, vector_2d& screenPos) -> int
{
    if (vertexList != nullptr && numVertices != 0)
    {
        for (int32_t i = 0; i < numVertices; i++)
        {
            const Vertex& vertex = vertexList[i];

            if (vertex.blockNum == blockNum && vertex.vertexNum == vertexNum)
            {
                screenPos.x = static_cast<float>(vertex.px);
                screenPos.y = static_cast<float>(vertex.py);
                return 1;
            }
        }
    }

    screenPos.y = -10000.0f;
    screenPos.x = -10000.0f;
    return 0;
}

auto TerrainWindow::markSeen(vector_3d& looker, vector_3d& lookVector, float angle, float range, uint8_t who) -> void
{
    vector_2d topLeft(topLeftX, topLeftY);
    Terrain::mapBlockManager->markSeen(topLeft, vertexList, looker, lookVector, angle, range, who);
}

auto TerrainWindow::cameraShowsPosition(vector_3d& pos) -> int
{
    if (camera == nullptr || camera->active == 0)
    {
        return 0;
    }

    const float span = static_cast<float>(Terrain::visibleVerticesPerSide) * Terrain::metersPerVertex;

    if (topLeftX <= pos.x && pos.x <= span + topLeftX && pos.y <= topLeftY && topLeftY - span <= pos.y)
    {
        return 1;
    }

    return 0;
}

auto TerrainWindow::update(int /*force*/) -> int
{
    if (scenario->alwaysRevealed == 0)
    {
        Terrain::terrainVisibleBits->resetAll(0);
    }

    if (scenario->godMode != 0)
    {
        Terrain::terrainVisibleBits->resetAll(1);
        Terrain::ISSeenBits->resetAll(1);
    }

    cameraPosX = camera->position.x;
    cameraPosY = camera->position.y;
    cameraPosZ = camera->position.z;
    vector_3d cameraPos = camera->position;
    const int32_t result = Terrain::mapBlockManager->update(cameraPos, windowNum);

    if (result != 0)
    {
        return result;
    }

    Terrain::mapBlockManager->buildWindow(vertexList, &numVertices, blockList, &numBlocks, windowNum);
    return 0;
}

auto TerrainWindow::render(int32_t hazeFactor, uint8_t flags) -> void
{
    float vertexStep = Terrain::metersPerVertex;
    float elevStep = Terrain::metersPerElevLevel;
    int32_t col = 0;
    int32_t row = 0;
    const int32_t mapVertices =
        Terrain::blocksMapSide * Terrain::blocksMapSide * Terrain::verticesBlockSide * Terrain::verticesBlockSide;

    for (int32_t i = 0; i < mapVertices; i++)
    {
        Terrain::screenPosX[i] = 0x11111111;
    }

    Vertex* vertex = vertexList;
    vector_3d point(0.0f + topLeftX, topLeftY - 0.0f, 0.0f);
    Terrain::forceRedraw = 1;
    point.z = static_cast<float>(static_cast<int32_t>(vertex->pVertex->elevation)) * Terrain::metersPerElevLevel;

    // The map tile under the grid's corner (for the debug grid), kept on the map.
    int32_t tileCol = 0;
    int32_t tileRow = 0;
    const int32_t mapSide = Terrain::blocksMapSide * Terrain::verticesBlockSide;

    if (drawTerrainGrid != 0)
    {
        const float gridX =
            static_cast<float>(std::floor(Terrain::OneOvermetersPerVertex * point.x)) * Terrain::metersPerVertex;
        const float gridY =
            static_cast<float>(std::floor(Terrain::OneOvermetersPerVertex * point.y)) * Terrain::metersPerVertex;
        const int32_t vertexX = static_cast<int32_t>(std::floor(gridX * Terrain::OneOvermetersPerVertex));
        const int32_t vertexY = static_cast<int32_t>(std::floor(gridY * Terrain::OneOvermetersPerVertex));
        tileCol = std::clamp(vertexX + (mapSide >> 1), 0, mapSide - 1);
        tileRow = std::clamp((mapSide >> 1) - vertexY, 0, mapSide - 1);
    }

    const float zoom = eye->cameraScale != 1 ? 1.0f : 0.5f;
    vertexStep = zoom * vertexStep;
    elevStep = zoom * elevStep;
    const int32_t firstTileCol = tileCol;

    vector_2d rowStart;
    // Port fix: the original leaves the scroll uninitialised when useOldProject is set.
    int32_t scrollX = 0;
    int32_t scrollY = 0;

    if (useOldProject != 0)
    {
        vector_3d corner(0.0f + topLeftX, topLeftY - 0.0f, static_cast<float>(vertex->pVertex->elevation) * elevStep);
        vector_3d relative = corner - camera->position;
        relative *= camera->cameraScale != 1 ? 1.0f : 0.5f;
        rowStart.x = relative.x * camera->cosAngle + relative.y * camera->cosAngle + camera->halfWidth;
        rowStart.y =
            ((relative.x * camera->sinAngle + camera->halfHeight) - relative.y * camera->sinAngle) - relative.z;
    }

    // One vertex step across the grid moves the screen point by (stepX, stepY), plus the elevation change.
    float elevScreenStep = elevStep;
    float stepX = camera->cosAngle * vertexStep;
    float stepY = camera->sinAngle * vertexStep;

    if (useNonIntegerAdditive == 0)
    {
        stepX = std::floor(stepX);
        stepY = std::floor(stepY);
        elevScreenStep = std::floor(elevStep);
    }

    vector_2d screen;

    if (useOldProject == 0)
    {
        screenFromCamera(point, screen, scrollX, scrollY);
        rowStart = screen;
    }
    else
    {
        vector_2d screen100;
        vector_2d screen50;
        land->projectTerrain(point, screen100, screen50);
    }

    float lastElevation = static_cast<float>(vertex->pVertex->elevation);
    float rowFirstElevation = lastElevation;
    const int32_t minX = static_cast<int32_t>(std::floor(-stepX));
    const int32_t minY = static_cast<int32_t>(std::floor(-stepY));
    const int32_t maxX = (globalPane->x1 - globalPane->x0) + static_cast<int32_t>(std::floor(stepX));
    const int32_t maxY = globalPane->y1 - globalPane->y0;
    vector_2d current = rowStart;

    for (int16_t index = 0; index < numVertices; index++)
    {
        if (projectAll == 0)
        {
            if (index != 0 && col != 0)
            {
                const float elevation = static_cast<float>(vertex->pVertex->elevation);
                current.x = current.x + stepX;
                current.y = (lastElevation - elevation) * elevScreenStep + current.y + stepY;
                lastElevation = elevation;
            }
        }
        else
        {
            point.x = static_cast<float>(col) * vertexStep + topLeftX;
            point.y = topLeftY - static_cast<float>(row) * vertexStep;
            point.z = static_cast<float>(vertex->pVertex->elevation) * elevStep;
            screenFromCamera(point, current, scrollX, scrollY);
        }

        const int32_t px = static_cast<int32_t>(std::floor(current.x));
        vertex->px = px;
        const int32_t py = static_cast<int32_t>(std::floor(current.y));
        vertex->py = py;
        const int16_t blockNum = vertex->blockNum;
        const int16_t vertexNum = vertex->vertexNum;

        if (blockNum >= 0 && vertexNum >= 0 && blockNum < Terrain::blocksMapSide * Terrain::blocksMapSide &&
            vertexNum < Terrain::verticesBlockSide * Terrain::verticesBlockSide)
        {
            const int32_t screenIndex = Terrain::blockOffsets[blockNum] + vertexNum;
            Terrain::screenPosX[screenIndex] = static_cast<int32_t>(std::floor(current.x));
            Terrain::screenPosY[screenIndex] = static_cast<int32_t>(std::floor(current.y));
        }

        vertex->edgeRedraw = 0;
        vertex->redraw = 0;
        vertex->clipped = 0;

        if (px < minX || px > maxX || py < minY || py > maxY)
        {
            vertex->clipped = 1;
        }

        // A scroll of a screen or more (or forceAlways) redraws everything.
        if (std::abs(scrollX) < application->width() && std::abs(scrollY) < application->height() && forceAlways == 0)
        {
            if (Terrain::forceRedraw != 0)
            {
                vertex->redraw = 1;
                vertex->edgeRedraw = 1;
            }
        }
        else
        {
            Terrain::forceRedraw = 1;
            vertex->redraw = 1;
            vertex->edgeRedraw = 1;
        }

        if (vertex->clipped == 0)
        {
            addBlockToList(blockNum);
            // Vertices over the strip the scroll uncovered are redrawn; those near it get their edges redrawn.
            const int32_t oldX = px - scrollX;

            if (oldX < 0 && scrollX > 0)
            {
                vertex->redraw = 1;
            }

            if (static_cast<float>(oldX) <= stepX + stepX && scrollX > 0)
            {
                vertex->edgeRedraw = 1;
            }

            const int32_t paneWidth = globalPane->x1 - globalPane->x0;

            if (paneWidth < oldX && scrollX < 0)
            {
                vertex->redraw = 1;
            }

            if (static_cast<float>(paneWidth) - (stepX + stepX) <= static_cast<float>(oldX) && scrollX < 0)
            {
                vertex->edgeRedraw = 1;
            }

            const int32_t oldY = py - scrollY;

            if (oldY < 0 && scrollY > 0)
            {
                vertex->redraw = 1;
            }

            if (static_cast<float>(oldY) <= stepY + stepY && scrollY > 0)
            {
                vertex->edgeRedraw = 1;
            }

            const int32_t paneHeight = globalPane->y1 - globalPane->y0;

            if (static_cast<float>(paneHeight) - stepY < static_cast<float>(oldY) && scrollY < 0)
            {
                vertex->redraw = 1;
            }

            if (static_cast<float>(paneHeight) - (stepY + stepY) <= static_cast<float>(oldY) && scrollY < 0)
            {
                vertex->edgeRedraw = 1;
            }

            if (drawTerrainGrid != 0 && eye->cameraScale != 1)
            {
                drawCellGrid(this, tileRow, tileCol, col, row, vertexStep);
            }
        }

        if (drawTerrainGrid != 0 && eye->cameraScale != 1)
        {
            drawDoors(this, tileRow, tileCol, col, row, vertexStep);
        }

        col++;
        vertex++;
        tileCol++;

        if (col == Terrain::visibleVerticesPerSide)
        {
            row++;
            tileRow++;
            col = 0;

            if (row != Terrain::visibleVerticesPerSide)
            {
                lastElevation = static_cast<float>(vertex->pVertex->elevation);
            }

            rowStart.x = rowStart.x - stepX;
            const float rise = rowFirstElevation - lastElevation;
            rowFirstElevation = lastElevation;
            rowStart.y = rise * elevScreenStep + rowStart.y + stepY;
            tileCol = firstTileCol;
            current = rowStart;
        }

        tileCol = std::clamp(tileCol, 0, mapSide - 1);
        tileRow = std::clamp(tileRow, 0, mapSide - 1);
    }

    if (drawTerrainTiles != 0)
    {
        ElementList->openGroup(50000000, 0);
        numTerrainFaces = 0;

        // Port: the GPU draws the pass from the map's ground mesh in one draw; the tiles below then only reach the
        // software renderer (when it draws too). Where the mesh can't stand for the tiles, that's an error.
        _window* target = globalPane->window;
        const bool layer = MCRenderer::Hardware() != nullptr && MCRenderer::GpuDrawing() != MCGpuDrawing::Off &&
                           MCRenderer::FrameSurfaceOf(target) != nullptr;

        if (layer)
        {
            MCTerrainFrame ground;
            auto drawn = MCTerrainGroundFrame(vertexList, numVertices, numBlocks, hazeFactor,
                                              static_cast<int32_t>(stepX), static_cast<int32_t>(stepY),
                                              static_cast<int32_t>(elevScreenStep), minX, maxX, minY, maxY, ground);

            if (drawn)
            {
                drawn = MCRenderer::For(target).TerrainLayer(target, ground);
            }

            if (!drawn)
            {
                Fatal(-1, drawn.error().substr(0, 240).c_str());
            }
        }

        for (int16_t i = 0; i < numBlocks; i++)
        {
            blockList[i].draw(hazeFactor, flags);
        }

        if (layer)
        {
            MCRenderer::For(target).EndTerrainLayer(target);
        }
    }

    if (drawTerrainOverlays != 0)
    {
        ElementList->openGroup(10000000, 0);
        numTerrainFaces = 0;

        for (int16_t i = 0; i < numBlocks; i++)
        {
            blockList[i].drawOverlay(hazeFactor, flags);
        }
    }

    Terrain::forceRedraw = 0;
}

auto TerrainWindow::renderHaze(int32_t hazeFactor, uint8_t flags) -> void
{
    for (int32_t i = 0; i < numBlocks; i++)
    {
        blockList[i].drawHaze(hazeFactor, flags);
    }
}

auto TerrainWindow::drawLines() -> void
{
    ElementList->openGroup(49990000, 0);

    for (int32_t i = 0; i < numBlocks; i++)
    {
        blockList[i].drawLine(0xff, 1);
    }
}

auto TerrainWindow::drawTopView() -> void
{
    if (Terrain::currentPass != 0)
    {
        return;
    }

    // Lays the grid out flat in the top-left square of the window.
    const int32_t side = std::min(globalWindow->x_max, globalWindow->y_max);
    const int32_t step = side / Terrain::visibleVerticesPerSide;
    int32_t col = 0;
    int32_t rowY = 0;

    for (int32_t i = 0; i < numVertices; i++)
    {
        vertexList[i].px = step * col;
        vertexList[i].py = rowY;
        col++;

        if (col == Terrain::visibleVerticesPerSide)
        {
            col = 0;
            rowY += step;
        }
    }
}

auto TerrainWindow::drawVertices() -> void
{
    // One pixel per vertex, in the top-right corner of the window.
    const int32_t left = globalWindow->x_max - Terrain::visibleVerticesPerSide;
    int32_t col = 0;
    int32_t row = 0;

    for (int32_t i = 0; i < numVertices; i++)
    {
        AG_pixel_write(globalPane, left + col, row, 0x10);
        col++;

        if (col == Terrain::visibleVerticesPerSide)
        {
            col = 0;
            row++;
        }
    }
}
