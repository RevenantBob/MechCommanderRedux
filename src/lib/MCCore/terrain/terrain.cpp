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

MCMapBlockManager* MCTerrain::MapBlockManager = nullptr;
MCVertexManager* MCTerrain::VertexManager = nullptr;
MCTerrainTileManager* MCTerrain::TerrainTileManager = nullptr;
MCTacticalMap* MCTerrain::TerrainTacticalMap = nullptr;
MCByteFlag* MCTerrain::TerrainVisibleBits = nullptr;
MCBitFlag* MCTerrain::ISSeenBits = nullptr;
MCByteFlag* MCTerrain::ClanVisibleBits = nullptr;
MCBitFlag* MCTerrain::ClanSeenBits = nullptr;
int32_t MCTerrain::CurrentPass = 0;
int32_t MCTerrain::VerticesBlockSide = 0;
int32_t MCTerrain::BlocksMapSide = 0;
int32_t MCTerrain::BlocksToCache = 0;
int32_t MCTerrain::TotalBlocks = 0;
int32_t MCTerrain::VisibleVerticesPerSide = 0;
double MCTerrainGridReach = 0.0;
int32_t MCTerrain::VisibleBlocksPerSide = 0;
float MCTerrain::MetersPerElevLevel = 0.0f;
float MCTerrain::MetersPerVertex = 0.0f;
float MCTerrain::OneOvermetersPerVertex = 0.0f;
float MCTerrain::OneOververticesBlockSide = 0.0f;
int32_t MCTerrain::VerticesMapSide = 0;
float MCTerrain::MetersPerVertexDivMapcellDim = 0.0f;
float MCTerrain::MetersBlockSide = 0.0f;
MCPane* MCTerrain::TerrainPane = nullptr;
char* MCTerrain::TerrainName = nullptr;
std::vector<int32_t> MCTerrain::ScreenPosX;
std::vector<int32_t> MCTerrain::ScreenPosY;
std::vector<int32_t> MCTerrain::BlockOffsets;
int MCTerrain::ForceRedraw = 0;
MCVector2D MCTerrain::MapTopLeft2d100;
MCVector3D MCTerrain::MapTopLeft3d100;
MCVector2D MCTerrain::MapTopLeft2d50;
MCVector3D MCTerrain::MapTopLeft3d50;

MCTerrain* Land = nullptr;
int DrawTerrainTiles = 1;
int DrawTerrainOverlays = 1;
int UseNonIntegerAdditive = 0;
MCVector2D PrevPosition;
int32_t UsedBlockList[MAX_BLOCK_LIST] = {};
int32_t MoverBlockList[MAX_BLOCK_LIST] = {};
uint32_t BlockMemSize = 0;
int ProjectAll = 0;
MCObjectBlockManager* ObjBlockManager = nullptr;
MCTerrainTiles* TerrainTiles = nullptr;
int32_t VerticesPerBlock = 0;
float WorldUnitsMapSide = 0.0f;

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
    float EyeZoom()
    {
        return Eye->CameraScale != 1 ? 1.0f : 0.5f;
    }

    /// <summary>Projects a world point to the main camera's screen (the binary inlines this).</summary>
    MCVector2D EyeProject(const MCVector3D& point)
    {
        MCVector3D relative = point - Eye->Position;
        relative *= EyeZoom();
        return MCVector2D(relative.X * Eye->CosAngle + relative.Y * Eye->CosAngle + Eye->HalfWidth,
                          ((relative.X * Eye->SinAngle + Eye->HalfHeight) - relative.Y * Eye->SinAngle) - relative.Z);
    }

    /// <summary>
    /// Adds the four sides of a quad to the element list: from <paramref name="p1"/> to <paramref name="p2"/> to
    /// <paramref name="p3"/> to <paramref name="p0"/>, and <paramref name="p1"/> to <paramref name="p0"/>.
    /// </summary>
    void AddGridQuad(MCVector2D& p0, MCVector2D& p1, MCVector2D& p2, MCVector2D& p3, int32_t color)
    {
        ElementList->Add(MCElementPool::Make<MCLineElement>(p1, p2, color, nullptr, GRID_LINE_DEPTH, -1));
        ElementList->Add(MCElementPool::Make<MCLineElement>(p2, p3, color, nullptr, GRID_LINE_DEPTH, -1));
        ElementList->Add(MCElementPool::Make<MCLineElement>(p3, p0, color, nullptr, GRID_LINE_DEPTH, -1));
        ElementList->Add(MCElementPool::Make<MCLineElement>(p1, p0, color, nullptr, GRID_LINE_DEPTH, -1));
    }

    /// <summary>
    /// Projects the corners of a <paramref name="width"/> x <paramref name="height"/> rectangle whose top-left is
    /// <paramref name="origin"/>, each dropped onto the terrain, and adds its outline.
    /// </summary>
    void AddTerrainQuad(MCTerrainWindow* window, MCVector3D origin, float width, float height, int32_t color)
    {
        MCVector3D point = origin;
        point.Z = window->GetTerrainElevation(point);
        MCVector2D p0 = EyeProject(point);
        point.X += width;
        point.Z = window->GetTerrainElevation(point);
        MCVector2D p1 = EyeProject(point);
        point.Y -= height;
        point.Z = window->GetTerrainElevation(point);
        MCVector2D p2 = EyeProject(point);
        point.X -= width;
        point.Z = window->GetTerrainElevation(point);
        MCVector2D p3 = EyeProject(point);
        AddGridQuad(p0, p1, p2, p3, color);
    }

    /// <summary>Whether (tileRow, tileCol) is on the map.</summary>
    int TileOnMap(int32_t tileRow, int32_t tileCol)
    {
        return (tileRow >= 0 && tileRow < GameMap->Height && tileCol >= 0 && tileCol < GameMap->Width) ? 1 : 0;
    }

    /// <summary>
    /// The debug grid for one map tile: each of its 3x3 cells outlined when impassable (0xfd), when its second
    /// passability bit is clear (0xfc) and when path-locked (0xfe).
    /// </summary>
    void DrawCellGrid(MCTerrainWindow* window, int32_t tileRow, int32_t tileCol, int32_t col, int32_t row,
                      float vertexStep)
    {
        const float cellSize = static_cast<float>(MCTerrain::MetersPerVertexDivMapcellDim - 5.0);
        Assert(TileOnMap(tileRow, tileCol), 0, " terrwindow:render MapTile Out of Bounds ");
        Assert(TileOnMap(tileRow, tileCol), 0, " Map Tile out of bounds ");
        const MCMapTile& tile = GameMap->Map[GameMap->Width * tileRow + tileCol];
        const uint32_t cells = tile.Cells;
        const uint32_t overlay = tile.Overlay;

        uint32_t cellShift = 0;
        uint32_t lockShift = 0;

        for (int32_t cellR = 0; lockShift < 9; cellR++)
        {
            for (int32_t cellC = 0; cellC < 3; cellC++)
            {
                const MCVector3D origin(
                    static_cast<float>(cellC) * cellSize + static_cast<float>(col) * vertexStep + window->TopLeftX,
                    (window->TopLeftY - static_cast<float>(row) * vertexStep) - static_cast<float>(cellR) * cellSize,
                    0.0f);

                if (((0x4000u << cellShift) & cells) >> (cellShift + 14) == 0)
                {
                    AddTerrainQuad(window, origin, cellSize, cellSize, 0xfd);
                }

                if (((0x8000u << cellShift) & cells) >> (cellShift + 15) == 0)
                {
                    AddTerrainQuad(window, origin, cellSize, cellSize, 0xfc);
                }

                if (((0x8000u << lockShift) & overlay) >> (lockShift + 15) != 0)
                {
                    AddTerrainQuad(window, origin, cellSize, cellSize, 0xfe);
                }

                cellShift += 2;
                lockShift++;
            }
        }
    }

    /// <summary>The debug grid's global-map doors on one map tile, outlined in 0xea along their length.</summary>
    void DrawDoors(MCTerrainWindow* window, int32_t tileRow, int32_t tileCol, int32_t col, int32_t row,
                   float vertexStep)
    {
        const float cellSize = static_cast<float>(MCTerrain::MetersPerVertexDivMapcellDim - 5.0);
        Assert(TileOnMap(tileRow, tileCol), 0, " terrwindow:render MapTile Out of Bounds ");

        for (int32_t i = 0; i < GlobalMoveMap->NumDoors; i++)
        {
            const MCGlobalMapDoor& door = GlobalMoveMap->Doors[i];

            if (door.Row != tileRow || door.Col != tileCol)
            {
                continue;
            }

            int32_t lengthX = 1;
            int32_t lengthY = 1;

            if (door.Direction[0] == 1)
            {
                lengthY = door.Length;
            }

            if (door.Direction[0] == 2)
            {
                lengthX = door.Length;
            }

            const MCVector3D origin(
                static_cast<float>(door.CellC) * cellSize + static_cast<float>(col) * vertexStep + window->TopLeftX,
                (window->TopLeftY - static_cast<float>(row) * vertexStep) - static_cast<float>(door.CellR) * cellSize,
                0.0f);
            AddTerrainQuad(window, origin, static_cast<float>(lengthX) * cellSize,
                           static_cast<float>(lengthY) * cellSize, 0xea);
        }
    }

    /// <summary>
    /// The camera's screen position of the terrain point <paramref name="point"/> and how far the view scrolled
    /// since the last frame (from the camera's projected corners at its zoom).
    /// </summary>
    void ScreenFromCamera(MCVector3D& point, MCVector2D& screen, int32_t& scrollX, int32_t& scrollY)
    {
        MCVector2D screen100;
        MCVector2D screen50;
        Land->ProjectTerrain(point, screen100, screen50);

        if (Eye->CameraScale == 1)
        {
            screen.X = (screen50.X - Eye->ScreenUL50.X) + Eye->HalfWidth;
            screen.Y = (screen50.Y - Eye->ScreenUL50.Y) + Eye->HalfHeight;
            scrollX = static_cast<int32_t>(std::floor(Eye->LastScreenUL50.X - Eye->ScreenUL50.X));
            scrollY = static_cast<int32_t>(std::floor(Eye->LastScreenUL50.Y - Eye->ScreenUL50.Y));
        }
        else
        {
            screen.X = (screen100.X - Eye->ScreenUL.X) + Eye->HalfWidth;
            screen.Y = (screen100.Y - Eye->ScreenUL.Y) + Eye->HalfHeight;
            scrollX = static_cast<int32_t>(std::floor(Eye->LastScreenUL.X - Eye->ScreenUL.X));
            scrollY = static_cast<int32_t>(std::floor(Eye->LastScreenUL.Y - Eye->ScreenUL.Y));
        }
    }
}

auto RevealAll() -> void
{
    if (Land == nullptr)
    {
        return;
    }

    MCVector3D origin(0.0f, 0.0f, 0.0f);
    MCVector3D lookVector;
    Land->MarkRadiusSeen(origin, lookVector, 360.0f, 10000.0f, 1);
}

auto AddBlockToList(int32_t blockNum) -> void
{
    for (int32_t i = 0; i < MAX_BLOCK_LIST; i++)
    {
        if (UsedBlockList[i] == blockNum)
        {
            return;
        }

        if (UsedBlockList[i] == -1)
        {
            UsedBlockList[i] = blockNum;
            return;
        }
    }
}

auto AddMoverToList(int32_t blockNum) -> void
{
    for (int32_t i = 0; i < MAX_BLOCK_LIST; i++)
    {
        if (MoverBlockList[i] == blockNum)
        {
            return;
        }

        if (MoverBlockList[i] == -1)
        {
            MoverBlockList[i] = blockNum;
            return;
        }
    }
}

auto ClearBlockList() -> void
{
    if (BlockMemSize == 0)
    {
        BlockMemSize = sizeof(UsedBlockList);
    }

    std::memset(UsedBlockList, 0xff, BlockMemSize);
}

auto ClearMoverList() -> void
{
    if (BlockMemSize == 0)
    {
        BlockMemSize = sizeof(MoverBlockList);
    }

    std::memset(MoverBlockList, 0xff, BlockMemSize);
}

auto MCTerrain::GetTerrainWindow(int32_t windowNum) -> MCTerrainWindow*
{
    return &Windows[windowNum];
}

auto MCTerrain::Init() -> void
{
    PartId = -1;
    IdNumber = 0;
    Next = nullptr;
    Windows = nullptr;
    NumWindows = 0;
    ObjectClass = static_cast<MCObjectClass>(1);
}

auto MCTerrain::Init(char* fileName) -> int32_t
{
    ClearBlockList();
    ClearMoverList();
    const size_t nameLength = std::strlen(fileName);
    TerrainName = new char[nameLength + 1];
    std::strncpy(TerrainName, fileName, nameLength);
    TerrainName[nameLength] = '\0';

    MCFullPathFileName fitName;
    fitName.Init(TerrainPath, fileName, ".fit");
    MCFitIniFile terrainFile;
    int32_t result = terrainFile.Open(fitName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = terrainFile.SeekBlock("TerrainData")) != 0)
    {
        return result;
    }

    if ((result = terrainFile.ReadIdLong("VerticesBlockSide", VerticesBlockSide)) != 0)
    {
        return result;
    }

    if ((result = terrainFile.ReadIdLong("BlocksMapSide", BlocksMapSide)) != 0)
    {
        return result;
    }

    if ((result = terrainFile.ReadIdFloat("MetersPerElevLevel", MetersPerElevLevel)) != 0)
    {
        return result;
    }

    if ((result = terrainFile.ReadIdFloat("MetersPerVertex", MetersPerVertex)) != 0)
    {
        return result;
    }

    if ((result = terrainFile.ReadIdLong("VisibleVerticesPerSide", VisibleVerticesPerSide)) != 0)
    {
        return result;
    }

    {
        // Port fix: every map's grid is 30 vertices, made for 640x480. Zoomed out at 1280x1024 it no longer reaches
        // the screen's corners (black, jagged edges, and a tactical map view box that jumps when a corner lands on
        // no terrain). Grow it with the screen: the grid is a diamond, so what it has to cover grows with
        // width / cos + height / sin of the view angle. Kept even, like the data's. The screen follows the window,
        // which can grow during the scenario, so the grid covers the largest desktop.
        int32_t screenWidth = Application->Width();
        int32_t screenHeight = Application->Height();

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
        const double furthest = static_cast<double>(MCViewWindow::ZoomFurthest);
        const double needed = std::max(static_cast<double>(screenWidth) / cosAngle + screenHeight / sinAngle,
                                       furthest * widest / cosAngle + furthest / sinAngle);
        const double designed = 640.0 / cosAngle + 480.0 / sinAngle;
        const int32_t dataVertices = VisibleVerticesPerSide;
        const auto grown = static_cast<int32_t>(std::ceil(VisibleVerticesPerSide * needed / designed));

        if (grown > VisibleVerticesPerSide)
        {
            VisibleVerticesPerSide = (grown + 1) & ~1;
        }

        MCTerrainGridReach = designed * VisibleVerticesPerSide / dataVertices;
    }

    if ((result = terrainFile.ReadIdLong("NumberOfWindows", NumWindows)) != 0)
    {
        return result;
    }

    if ((result = terrainFile.ReadIdULong("TerrainHeapSize", TerrainHeapSize)) != 0)
    {
        return result;
    }

    if ((result = terrainFile.SeekBlock("TileData")) != 0)
    {
        return result;
    }

    // The tile heap's size is still read (and required), then ignored.
    uint32_t tileHeapSize = 0;

    if ((result = terrainFile.ReadIdULong("TerrainTileHeapSize", tileHeapSize)) != 0)
    {
        return result;
    }

    char tileFileName[80];

    if ((result = terrainFile.ReadIdString("TerrainTileFile", tileFileName, 79)) != 0)
    {
        return result;
    }

    TotalBlocks = BlocksMapSide * BlocksMapSide;
    VerticesPerBlock = VerticesBlockSide * VerticesBlockSide;
    const int32_t mapVertices = TotalBlocks * VerticesBlockSide * VerticesBlockSide;
    ScreenPosX.assign(static_cast<size_t>(mapVertices), 0x11111111);
    ScreenPosY.assign(static_cast<size_t>(mapVertices), 0);
    BlockOffsets.assign(static_cast<size_t>(TotalBlocks), 0);

    for (int32_t i = 0, offset = 0; i < TotalBlocks; i++, offset += VerticesBlockSide * VerticesBlockSide)
    {
        BlockOffsets[i] = offset;
    }

    TerrainTiles = new MCTerrainTiles;

    if ((result = TerrainTiles->Init(tileFileName)) != 0)
    {
        return result;
    }

    TerrainTiles->Preload(fileName);

    VisibleBlocksPerSide = static_cast<int32_t>(
        std::floor(static_cast<double>(VisibleVerticesPerSide) / static_cast<double>(VerticesBlockSide) + 0.5));

    if ((VisibleBlocksPerSide & 1) == 0)
    {
        VisibleBlocksPerSide++;
    }

    if (VisibleBlocksPerSide < 3)
    {
        VisibleBlocksPerSide = 3;
    }

    const uint32_t flagSide = static_cast<uint32_t>(BlocksMapSide * VerticesBlockSide);
    MetersBlockSide = static_cast<float>(VerticesBlockSide) * MetersPerVertex;
    WorldUnitsMapSide = static_cast<float>(BlocksMapSide) * MetersBlockSide;
    OneOvermetersPerVertex = 1.0f / MetersPerVertex;
    BlocksToCache = NumWindows * VisibleBlocksPerSide * VisibleBlocksPerSide;
    MetersPerVertexDivMapcellDim = MetersPerVertex * (1.0f / 3.0f);
    OneOververticesBlockSide = 1.0f / static_cast<float>(VerticesBlockSide);
    VerticesMapSide = static_cast<int32_t>(flagSide) >> 1;
    MapTopLeft3d100.X = WorldUnitsMapSide * -0.5f;
    MapTopLeft3d100.Y = WorldUnitsMapSide * 0.5f;

    TerrainVisibleBits = new MCByteFlag();

    if (TerrainVisibleBits == nullptr)
    {
        return TERRAIN_INIT_FAILED;
    }

    ISSeenBits = new MCBitFlag();

    if (ISSeenBits == nullptr)
    {
        return TERRAIN_INIT_FAILED;
    }

    ISSeenBits->DivValue = 1;
    ISSeenBits->ColWidth = 1;
    ClanVisibleBits = new MCByteFlag();

    if (ClanVisibleBits == nullptr)
    {
        return TERRAIN_INIT_FAILED;
    }

    ClanSeenBits = new MCBitFlag();

    if (ClanSeenBits == nullptr)
    {
        return TERRAIN_INIT_FAILED;
    }

    ClanSeenBits->DivValue = 1;
    ClanSeenBits->ColWidth = 1;
    TerrainVisibleBits->Init(flagSide, flagSide, 0);
    ISSeenBits->Init(flagSide, flagSide, 0);
    ClanVisibleBits->Init(flagSide, flagSide, 0);
    ClanSeenBits->Init(flagSide, flagSide, 0);

    Windows = new MCTerrainWindow[NumWindows];

    MapBlockManager = new MCMapBlockManager;

    if (MapBlockManager->Init(fileName, TotalBlocks, VerticesBlockSide * VerticesBlockSide * 8) != 0)
    {
        return TERRAIN_INIT_FAILED;
    }

    MapTopLeft3d100.Z = MapBlockManager->GetTopLeftElevation();

    // Each window's vertex grid, after a table of pointers to them.
    const int32_t vertexListSize =
        VisibleVerticesPerSide * VisibleVerticesPerSide * static_cast<int32_t>(sizeof(MCVertex));
    VertexManager = new MCVertexManager;

    if (VertexManager->VertexLists.empty())
    {
        // Zeroed raw storage, as the original's committed heap: the grids are filled by buildWindow.
        VertexManager->Storage.assign(static_cast<size_t>(vertexListSize) * NumWindows, 0);
        VertexManager->VertexLists.resize(static_cast<size_t>(NumWindows));

        for (int32_t i = 0; i < NumWindows; i++)
        {
            VertexManager->VertexLists[i] =
                reinterpret_cast<MCVertex*>(VertexManager->Storage.data() + static_cast<size_t>(i) * vertexListSize);
        }
    }

    const int32_t blockListSize =
        VisibleVerticesPerSide * VisibleVerticesPerSide * static_cast<int32_t>(sizeof(MCTerrainBlock));
    TerrainTileManager = new MCTerrainTileManager;

    if (TerrainTileManager->BlockLists.empty())
    {
        TerrainTileManager->Storage.assign(static_cast<size_t>(blockListSize) * NumWindows, 0);
        TerrainTileManager->BlockLists.resize(static_cast<size_t>(NumWindows));

        for (int32_t i = 0; i < NumWindows; i++)
        {
            TerrainTileManager->BlockLists[i] = reinterpret_cast<MCTerrainBlock*>(
                TerrainTileManager->Storage.data() + static_cast<size_t>(i) * blockListSize);
        }
    }

    ObjBlockManager = new MCObjectBlockManager;

    if (ObjBlockManager->Init(fileName) != 0)
    {
        return TERRAIN_INIT_FAILED;
    }

    TerrainTacticalMap = new MCTacticalMap;

    if (TerrainTacticalMap->Init(0, 0) != 0)
    {
        return TERRAIN_INIT_FAILED;
    }

    TheInterface->TacticalMap = TerrainTacticalMap;
    ScreenWindow->AddChild(TerrainTacticalMap);
    TerrainTacticalMap->RefreshPage();
    terrainFile.Close();

    const float sinAngle = static_cast<float>(std::sin(VIEW_ANGLE));
    PrevPosition.Y = 0.0f;
    PrevPosition.X = 0.0f;
    const float cosAngle = static_cast<float>(std::cos(VIEW_ANGLE));
    ProjectionSin = sinAngle;
    ProjectionCos = cosAngle;
    MapTopLeft2d100.X = MapTopLeft3d100.X * cosAngle + MapTopLeft3d100.Y * cosAngle;
    MapTopLeft2d100.Y = MapTopLeft3d100.X * sinAngle - MapTopLeft3d100.Y * sinAngle;
    MapTopLeft3d50.X = MapTopLeft3d100.X * 0.5f;
    MapTopLeft3d50.Y = MapTopLeft3d100.Y * 0.5f;
    MapTopLeft3d50.Z = MapTopLeft3d100.Z * 0.5f;
    MapTopLeft2d50.X = MapTopLeft3d50.X * cosAngle + MapTopLeft3d50.Y * cosAngle;
    MapTopLeft2d50.Y = MapTopLeft3d50.X * sinAngle - MapTopLeft3d50.Y * sinAngle;
    return 0;
}

auto MCTerrain::Destroy() -> void
{
    MCTerrainForgetMesh();

    if (TerrainTiles != nullptr)
    {
        // Faithful: destroyed twice (the second finds nothing left).
        TerrainTiles->Destroy();
        TerrainTiles->Destroy();
        delete TerrainTiles;
        TerrainTiles = nullptr;
    }

    delete[] Windows;
    Windows = nullptr;

    if (MapBlockManager != nullptr)
    {
        MapBlockManager->Destroy();
        delete MapBlockManager;
    }

    MapBlockManager = nullptr;

    if (ObjBlockManager != nullptr)
    {
        ObjBlockManager->Destroy();
        delete ObjBlockManager;
    }

    ObjBlockManager = nullptr;

    if (VertexManager != nullptr)
    {
        VertexManager->Storage = {};
        VertexManager->VertexLists = {};
        delete VertexManager;
    }

    VertexManager = nullptr;

    if (TerrainTileManager != nullptr)
    {
        TerrainTileManager->Storage = {};
        TerrainTileManager->BlockLists = {};
        delete TerrainTileManager;
    }

    TerrainTileManager = nullptr;

    if (TerrainVisibleBits != nullptr)
    {
        TerrainVisibleBits->Destroy();
        delete TerrainVisibleBits;
    }

    TerrainVisibleBits = nullptr;

    if (ISSeenBits != nullptr)
    {
        ISSeenBits->Destroy();
        delete ISSeenBits;
    }

    ISSeenBits = nullptr;

    if (ClanVisibleBits != nullptr)
    {
        ClanVisibleBits->Destroy();
        delete ClanVisibleBits;
    }

    ClanVisibleBits = nullptr;

    if (ClanSeenBits != nullptr)
    {
        ClanSeenBits->Destroy();
        delete ClanSeenBits;
    }

    ClanSeenBits = nullptr;

    if (TerrainTacticalMap != nullptr)
    {
        TerrainTacticalMap->Destroy();
        delete TerrainTacticalMap;
        TerrainTacticalMap = nullptr;
    }

    ScreenPosX = {};
    ScreenPosY = {};
    BlockOffsets = {};

    delete[] TerrainName;
    TerrainName = nullptr;
}

auto MCTerrain::NewWindow(MCCamera* cam) -> MCTerrainWindow*
{
    for (int32_t i = 0; i < NumWindows; i++)
    {
        if (Windows[i].Camera == nullptr && Windows[i].Init(cam, i) == 0)
        {
            return &Windows[i];
        }
    }

    return nullptr;
}

auto MCTerrain::KillWindow(MCCamera* cam) -> void
{
    for (int32_t i = 0; i < NumWindows; i++)
    {
        if (Windows[i].Camera == cam)
        {
            Windows[i].Destroy();
        }
    }
}

auto MCTerrain::Update() -> int32_t
{
    Windows[0].Update(0);
    return 1;
}

auto MCTerrain::SetOverlayTile(int32_t blockNum, int32_t vertexNum, int32_t value) -> void
{
    MapBlockManager->SetOverlayTile(blockNum, vertexNum, value);
}

auto MCTerrain::GetOverlayTile(int32_t blockNum, int32_t vertexNum) -> int32_t
{
    return MapBlockManager->GetOverlayTile(blockNum, vertexNum);
}

auto MCTerrain::SetTile(int32_t blockNum, int32_t vertexNum, int32_t value) -> void
{
    MapBlockManager->SetTile(blockNum, vertexNum, value);
}

auto MCTerrain::GetTile(int32_t blockNum, int32_t vertexNum) -> int32_t
{
    return MapBlockManager->GetTile(blockNum, vertexNum);
}

auto MCTerrain::ProjectTerrain(MCVector3D& pos, MCVector2D& screen100, MCVector2D& screen50) -> void
{
    const float fromTop = MapTopLeft3d100.Y - pos.Y;
    const float fromLeft = pos.X - MapTopLeft3d100.X;
    screen100.X = (fromLeft - fromTop) * ProjectionCos;
    screen100.X = static_cast<float>(static_cast<int32_t>(std::floor(screen100.X))) + MapTopLeft2d100.X;
    screen100.Y = (fromLeft + fromTop) * ProjectionSin;
    const int32_t y100 = static_cast<int32_t>(std::floor(screen100.Y));
    screen100.Y = static_cast<float>(y100);
    const int32_t z100 = static_cast<int32_t>(std::floor(pos.Z));
    screen100.Y = (MapTopLeft2d100.Y - static_cast<float>(z100)) + static_cast<float>(y100);

    const float halfZ = pos.Z * 0.5f;
    const float fromTop50 = MapTopLeft3d50.Y - pos.Y * 0.5f;
    const float fromLeft50 = pos.X * 0.5f - MapTopLeft3d50.X;
    screen50.X = (fromLeft50 - fromTop50) * ProjectionCos;
    screen50.X = static_cast<float>(static_cast<int32_t>(std::floor(screen50.X))) + MapTopLeft2d50.X;
    screen50.Y = (fromLeft50 + fromTop50) * ProjectionSin;
    const int32_t y50 = static_cast<int32_t>(std::floor(screen50.Y));
    screen50.Y = static_cast<float>(y50);
    const int32_t z50 = static_cast<int32_t>(std::floor(halfZ));
    screen50.Y = (MapTopLeft2d50.Y - static_cast<float>(z50)) + static_cast<float>(y50);
}

auto MCTerrain::Render(int32_t hazeFactor, uint8_t flags, MCCamera* cam) -> void
{
    for (int32_t i = 0; i < NumWindows; i++)
    {
        if (Windows[i].Camera == cam)
        {
            Windows[i].Render(hazeFactor, flags);
        }
    }
}

auto MCTerrain::RenderHaze(int32_t hazeFactor, uint8_t flags) -> void
{
    for (int32_t i = 0; i < NumWindows; i++)
    {
        if (Windows[i].CameraIsActive() != 0)
        {
            Windows[i].RenderHaze(hazeFactor, flags);
        }
    }
}

auto MCTerrain::DrawTopView() -> void
{
    for (int32_t i = 0; i < NumWindows; i++)
    {
        if (Windows[i].CameraIsActive() != 0)
        {
            Windows[i].DrawTopView();
        }
    }
}

auto MCTerrain::DrawVertices() -> void
{
    for (int32_t i = 0; i < NumWindows; i++)
    {
        if (Windows[i].CameraIsActive() != 0)
        {
            Windows[i].DrawVertices();
        }
    }
}

auto MCTerrain::DrawLines() -> void
{
    for (int32_t i = 0; i < NumWindows; i++)
    {
        if (Windows[i].CameraIsActive() != 0)
        {
            Windows[i].DrawLines();
        }
    }
}

auto MCTerrain::GetTerrainElevation(MCVector3D& pos) -> float
{
    return MapBlockManager->TerrainElevation(pos);
}

auto MCTerrain::GetTerrainAngle(MCVector3D& pos, MCVector3D* normal) -> float
{
    return MapBlockManager->TerrainAngle(pos, normal);
}

auto MCTerrain::GetTerrainNormal(MCVector3D& pos) -> MCVector3D
{
    return MapBlockManager->TerrainNormal(pos);
}

auto MCTerrain::UpdateAllObjects() -> void
{
    ObjBlockManager->UpdateAllObjects();
}

auto MCTerrain::MarkSeen(MCVector3D& looker, MCVector3D& /*lookVector*/, float angle, float /*range*/, uint8_t who)
    -> void
{
    if (angle != 360.0f)
    {
        return;
    }

    const float fromTop = MapTopLeft3d100.Y - looker.Y;
    const double gridX = std::floor(OneOvermetersPerVertex * (looker.X - MapTopLeft3d100.X));
    const double gridY = std::floor(OneOvermetersPerVertex * fromTop);
    const int32_t col = static_cast<int32_t>(std::floor(static_cast<float>(gridX)));
    const int32_t row = static_cast<int32_t>(std::floor(static_cast<float>(gridY)));

    Assert(TileOnMap(row, col), 0, " Map Tile out of bounds ");
    const uint32_t cellsA = GameMap->Map[GameMap->Width * row + col].Cells;
    Assert(TileOnMap(row, col + 1), 0, " Map Tile out of bounds ");
    const uint32_t cellsB = GameMap->Map[GameMap->Width * row + col + 1].Cells;
    Assert(TileOnMap(row + 1, col + 1), 0, " Map Tile out of bounds ");
    const uint32_t cellsC = GameMap->Map[GameMap->Width * (row + 1) + col + 1].Cells;
    Assert(TileOnMap(row + 1, col), 0, " Map Tile out of bounds ");
    const uint32_t cellsD = GameMap->Map[GameMap->Width * (row + 1) + col].Cells;

    // The highest of the four corners sets the sight radius.
    const uint32_t level =
        std::max({(cellsA >> 7) & 0x3f, (cellsB >> 7) & 0x3f, (cellsC >> 7) & 0x3f, (cellsD >> 7) & 0x3f});
    MCByteFlag* visibleBits = who == 1 ? TerrainVisibleBits : ClanVisibleBits;
    visibleBits->SetCircle(static_cast<uint32_t>(col), static_cast<uint32_t>(row),
                           static_cast<uint32_t>(VisualRangeTable[level]));
}

auto MCTerrain::MarkRadiusSeen(MCVector3D& looker, MCVector3D& /*lookVector*/, float angle, float range, uint8_t who)
    -> void
{
    if (angle != 360.0f)
    {
        return;
    }

    const float radius = static_cast<float>(static_cast<double>(3.34f) * range * OneOvermetersPerVertex);
    const float fromTop = MapTopLeft3d100.Y - looker.Y;
    const double gridX = std::floor(OneOvermetersPerVertex * (static_cast<double>(looker.X) - MapTopLeft3d100.X));
    const double gridY = std::floor(static_cast<double>(OneOvermetersPerVertex) * fromTop);
    const int32_t col = static_cast<int32_t>(std::floor(static_cast<float>(gridX)));
    const int32_t row = static_cast<int32_t>(std::floor(static_cast<float>(gridY)));
    MCByteFlag* visibleBits = who == 1 ? TerrainVisibleBits : ClanVisibleBits;
    visibleBits->SetCircle(static_cast<uint32_t>(col), static_cast<uint32_t>(row),
                           static_cast<uint32_t>(static_cast<int32_t>(radius)));
}

auto MCTerrain::FlipBuffers() -> void
{
}

auto MCTerrain::CopyBuffers(int32_t /*from*/, int32_t /*to*/) -> void
{
}

auto MCTerrainWindow::Init(MCCamera* cam, int32_t newWindowNum) -> int32_t
{
    Camera = cam;
    CameraActive = cam->Active;
    VertexList = MCTerrain::VertexManager->VertexLists[newWindowNum];

    if (VertexList == nullptr)
    {
        return NO_VERTEX_LIST;
    }

    BlockList = MCTerrain::TerrainTileManager->BlockLists[newWindowNum];

    if (BlockList == nullptr)
    {
        return NO_BLOCK_LIST;
    }

    WindowNum = newWindowNum;
    int32_t result = Update(1);

    if (result == REBUILD_WINDOW)
    {
        MCTerrain::MapBlockManager->BuildWindow(VertexList, &NumVertices, BlockList, &NumBlocks, WindowNum);
        result = 0;
    }

    return result;
}

auto MCTerrainWindow::Destroy() -> void
{
    Camera = nullptr;
    CameraActive = 0;
    VertexList = nullptr;
    BlockList = nullptr;
}

auto MCTerrainWindow::CameraIsActive() -> int
{
    if (Camera != nullptr)
    {
        return Camera->Active;
    }

    return 0;
}

auto MCTerrainWindow::GetTerrainElevation(MCVector3D& pos) -> float
{
    return MCTerrain::MapBlockManager->TerrainElevation(pos);
}

auto MCTerrainWindow::GetVertexScreenPos(int32_t blockNum, int32_t vertexNum, MCVector2D& screenPos) -> int
{
    if (VertexList != nullptr && NumVertices != 0)
    {
        for (int32_t i = 0; i < NumVertices; i++)
        {
            const MCVertex& vertex = VertexList[i];

            if (vertex.BlockNum == blockNum && vertex.VertexNum == vertexNum)
            {
                screenPos.X = static_cast<float>(vertex.Px);
                screenPos.Y = static_cast<float>(vertex.Py);
                return 1;
            }
        }
    }

    screenPos.Y = -10000.0f;
    screenPos.X = -10000.0f;
    return 0;
}

auto MCTerrainWindow::MarkSeen(MCVector3D& looker, MCVector3D& lookVector, float angle, float range, uint8_t who)
    -> void
{
    MCVector2D topLeft(TopLeftX, TopLeftY);
    MCTerrain::MapBlockManager->MarkSeen(topLeft, VertexList, looker, lookVector, angle, range, who);
}

auto MCTerrainWindow::CameraShowsPosition(MCVector3D& pos) -> int
{
    if (Camera == nullptr || Camera->Active == 0)
    {
        return 0;
    }

    const float span = static_cast<float>(MCTerrain::VisibleVerticesPerSide) * MCTerrain::MetersPerVertex;

    if (TopLeftX <= pos.X && pos.X <= span + TopLeftX && pos.Y <= TopLeftY && TopLeftY - span <= pos.Y)
    {
        return 1;
    }

    return 0;
}

auto MCTerrainWindow::Update(int /*force*/) -> int
{
    if (Scenario->AlwaysRevealed == 0)
    {
        MCTerrain::TerrainVisibleBits->ResetAll(0);
    }

    if (Scenario->GodMode != 0)
    {
        MCTerrain::TerrainVisibleBits->ResetAll(1);
        MCTerrain::ISSeenBits->ResetAll(1);
    }

    CameraPosX = Camera->Position.X;
    CameraPosY = Camera->Position.Y;
    CameraPosZ = Camera->Position.Z;
    MCVector3D cameraPos = Camera->Position;
    const int32_t result = MCTerrain::MapBlockManager->Update(cameraPos, WindowNum);

    if (result != 0)
    {
        return result;
    }

    MCTerrain::MapBlockManager->BuildWindow(VertexList, &NumVertices, BlockList, &NumBlocks, WindowNum);
    return 0;
}

auto MCTerrainWindow::Render(int32_t hazeFactor, uint8_t flags) -> void
{
    float vertexStep = MCTerrain::MetersPerVertex;
    float elevStep = MCTerrain::MetersPerElevLevel;
    int32_t col = 0;
    int32_t row = 0;
    const int32_t mapVertices = MCTerrain::BlocksMapSide * MCTerrain::BlocksMapSide * MCTerrain::VerticesBlockSide *
                                MCTerrain::VerticesBlockSide;

    for (int32_t i = 0; i < mapVertices; i++)
    {
        MCTerrain::ScreenPosX[i] = 0x11111111;
    }

    MCVertex* vertex = VertexList;
    MCVector3D point(0.0f + TopLeftX, TopLeftY - 0.0f, 0.0f);
    MCTerrain::ForceRedraw = 1;
    point.Z = static_cast<float>(static_cast<int32_t>(vertex->PVertex->Elevation)) * MCTerrain::MetersPerElevLevel;

    // The map tile under the grid's corner (for the debug grid), kept on the map.
    int32_t tileCol = 0;
    int32_t tileRow = 0;
    const int32_t mapSide = MCTerrain::BlocksMapSide * MCTerrain::VerticesBlockSide;

    if (DrawTerrainGrid != 0)
    {
        const float gridX =
            static_cast<float>(std::floor(MCTerrain::OneOvermetersPerVertex * point.X)) * MCTerrain::MetersPerVertex;
        const float gridY =
            static_cast<float>(std::floor(MCTerrain::OneOvermetersPerVertex * point.Y)) * MCTerrain::MetersPerVertex;
        const int32_t vertexX = static_cast<int32_t>(std::floor(gridX * MCTerrain::OneOvermetersPerVertex));
        const int32_t vertexY = static_cast<int32_t>(std::floor(gridY * MCTerrain::OneOvermetersPerVertex));
        tileCol = std::clamp(vertexX + (mapSide >> 1), 0, mapSide - 1);
        tileRow = std::clamp((mapSide >> 1) - vertexY, 0, mapSide - 1);
    }

    const float zoom = Eye->CameraScale != 1 ? 1.0f : 0.5f;
    vertexStep = zoom * vertexStep;
    elevStep = zoom * elevStep;
    const int32_t firstTileCol = tileCol;

    MCVector2D rowStart;
    // Port fix: the original leaves the scroll uninitialised when useOldProject is set.
    int32_t scrollX = 0;
    int32_t scrollY = 0;

    if (UseOldProject != 0)
    {
        MCVector3D corner(0.0f + TopLeftX, TopLeftY - 0.0f, static_cast<float>(vertex->PVertex->Elevation) * elevStep);
        MCVector3D relative = corner - Camera->Position;
        relative *= Camera->CameraScale != 1 ? 1.0f : 0.5f;
        rowStart.X = relative.X * Camera->CosAngle + relative.Y * Camera->CosAngle + Camera->HalfWidth;
        rowStart.Y =
            ((relative.X * Camera->SinAngle + Camera->HalfHeight) - relative.Y * Camera->SinAngle) - relative.Z;
    }

    // One vertex step across the grid moves the screen point by (stepX, stepY), plus the elevation change.
    float elevScreenStep = elevStep;
    float stepX = Camera->CosAngle * vertexStep;
    float stepY = Camera->SinAngle * vertexStep;

    if (UseNonIntegerAdditive == 0)
    {
        stepX = std::floor(stepX);
        stepY = std::floor(stepY);
        elevScreenStep = std::floor(elevStep);
    }

    MCVector2D screen;

    if (UseOldProject == 0)
    {
        ScreenFromCamera(point, screen, scrollX, scrollY);
        rowStart = screen;
    }
    else
    {
        MCVector2D screen100;
        MCVector2D screen50;
        Land->ProjectTerrain(point, screen100, screen50);
    }

    float lastElevation = static_cast<float>(vertex->PVertex->Elevation);
    float rowFirstElevation = lastElevation;
    const int32_t minX = static_cast<int32_t>(std::floor(-stepX));
    const int32_t minY = static_cast<int32_t>(std::floor(-stepY));
    const int32_t maxX = (GlobalPane->X1 - GlobalPane->X0) + static_cast<int32_t>(std::floor(stepX));
    const int32_t maxY = GlobalPane->Y1 - GlobalPane->Y0;
    MCVector2D current = rowStart;

    for (int16_t index = 0; index < NumVertices; index++)
    {
        if (ProjectAll == 0)
        {
            if (index != 0 && col != 0)
            {
                const float elevation = static_cast<float>(vertex->PVertex->Elevation);
                current.X = current.X + stepX;
                current.Y = (lastElevation - elevation) * elevScreenStep + current.Y + stepY;
                lastElevation = elevation;
            }
        }
        else
        {
            point.X = static_cast<float>(col) * vertexStep + TopLeftX;
            point.Y = TopLeftY - static_cast<float>(row) * vertexStep;
            point.Z = static_cast<float>(vertex->PVertex->Elevation) * elevStep;
            ScreenFromCamera(point, current, scrollX, scrollY);
        }

        const int32_t px = static_cast<int32_t>(std::floor(current.X));
        vertex->Px = px;
        const int32_t py = static_cast<int32_t>(std::floor(current.Y));
        vertex->Py = py;
        const int16_t blockNum = vertex->BlockNum;
        const int16_t vertexNum = vertex->VertexNum;

        if (blockNum >= 0 && vertexNum >= 0 && blockNum < MCTerrain::BlocksMapSide * MCTerrain::BlocksMapSide &&
            vertexNum < MCTerrain::VerticesBlockSide * MCTerrain::VerticesBlockSide)
        {
            const int32_t screenIndex = MCTerrain::BlockOffsets[blockNum] + vertexNum;
            MCTerrain::ScreenPosX[screenIndex] = static_cast<int32_t>(std::floor(current.X));
            MCTerrain::ScreenPosY[screenIndex] = static_cast<int32_t>(std::floor(current.Y));
        }

        vertex->EdgeRedraw = 0;
        vertex->Redraw = 0;
        vertex->Clipped = 0;

        if (px < minX || px > maxX || py < minY || py > maxY)
        {
            vertex->Clipped = 1;
        }

        // A scroll of a screen or more (or forceAlways) redraws everything.
        if (std::abs(scrollX) < Application->Width() && std::abs(scrollY) < Application->Height() && ForceAlways == 0)
        {
            if (MCTerrain::ForceRedraw != 0)
            {
                vertex->Redraw = 1;
                vertex->EdgeRedraw = 1;
            }
        }
        else
        {
            MCTerrain::ForceRedraw = 1;
            vertex->Redraw = 1;
            vertex->EdgeRedraw = 1;
        }

        if (vertex->Clipped == 0)
        {
            AddBlockToList(blockNum);
            // Vertices over the strip the scroll uncovered are redrawn; those near it get their edges redrawn.
            const int32_t oldX = px - scrollX;

            if (oldX < 0 && scrollX > 0)
            {
                vertex->Redraw = 1;
            }

            if (static_cast<float>(oldX) <= stepX + stepX && scrollX > 0)
            {
                vertex->EdgeRedraw = 1;
            }

            const int32_t paneWidth = GlobalPane->X1 - GlobalPane->X0;

            if (paneWidth < oldX && scrollX < 0)
            {
                vertex->Redraw = 1;
            }

            if (static_cast<float>(paneWidth) - (stepX + stepX) <= static_cast<float>(oldX) && scrollX < 0)
            {
                vertex->EdgeRedraw = 1;
            }

            const int32_t oldY = py - scrollY;

            if (oldY < 0 && scrollY > 0)
            {
                vertex->Redraw = 1;
            }

            if (static_cast<float>(oldY) <= stepY + stepY && scrollY > 0)
            {
                vertex->EdgeRedraw = 1;
            }

            const int32_t paneHeight = GlobalPane->Y1 - GlobalPane->Y0;

            if (static_cast<float>(paneHeight) - stepY < static_cast<float>(oldY) && scrollY < 0)
            {
                vertex->Redraw = 1;
            }

            if (static_cast<float>(paneHeight) - (stepY + stepY) <= static_cast<float>(oldY) && scrollY < 0)
            {
                vertex->EdgeRedraw = 1;
            }

            if (DrawTerrainGrid != 0 && Eye->CameraScale != 1)
            {
                DrawCellGrid(this, tileRow, tileCol, col, row, vertexStep);
            }
        }

        if (DrawTerrainGrid != 0 && Eye->CameraScale != 1)
        {
            DrawDoors(this, tileRow, tileCol, col, row, vertexStep);
        }

        col++;
        vertex++;
        tileCol++;

        if (col == MCTerrain::VisibleVerticesPerSide)
        {
            row++;
            tileRow++;
            col = 0;

            if (row != MCTerrain::VisibleVerticesPerSide)
            {
                lastElevation = static_cast<float>(vertex->PVertex->Elevation);
            }

            rowStart.X = rowStart.X - stepX;
            const float rise = rowFirstElevation - lastElevation;
            rowFirstElevation = lastElevation;
            rowStart.Y = rise * elevScreenStep + rowStart.Y + stepY;
            tileCol = firstTileCol;
            current = rowStart;
        }

        tileCol = std::clamp(tileCol, 0, mapSide - 1);
        tileRow = std::clamp(tileRow, 0, mapSide - 1);
    }

    if (DrawTerrainTiles != 0)
    {
        ElementList->OpenGroup(50000000, 0);
        NumTerrainFaces = 0;

        // Port: the GPU draws the pass from the map's ground mesh in one draw; the tiles below then only reach the
        // software renderer (when it draws too). Where the mesh can't stand for the tiles, that's an error.
        MCWindow* target = GlobalPane->Window;
        const bool layer = MCRenderer::Hardware() != nullptr && MCRenderer::GpuDrawing() != MCGpuDrawing::Off &&
                           MCRenderer::FrameSurfaceOf(target) != nullptr;

        if (layer)
        {
            MCTerrainFrame ground;
            auto drawn = MCTerrainGroundFrame(VertexList, NumVertices, NumBlocks, hazeFactor,
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

        for (int16_t i = 0; i < NumBlocks; i++)
        {
            BlockList[i].Draw(hazeFactor, flags);
        }

        if (layer)
        {
            MCRenderer::For(target).EndTerrainLayer(target);
        }
    }

    if (DrawTerrainOverlays != 0)
    {
        ElementList->OpenGroup(10000000, 0);
        NumTerrainFaces = 0;

        for (int16_t i = 0; i < NumBlocks; i++)
        {
            BlockList[i].DrawOverlay(hazeFactor, flags);
        }
    }

    MCTerrain::ForceRedraw = 0;
}

auto MCTerrainWindow::RenderHaze(int32_t hazeFactor, uint8_t flags) -> void
{
    for (int32_t i = 0; i < NumBlocks; i++)
    {
        BlockList[i].DrawHaze(hazeFactor, flags);
    }
}

auto MCTerrainWindow::DrawLines() -> void
{
    ElementList->OpenGroup(49990000, 0);

    for (int32_t i = 0; i < NumBlocks; i++)
    {
        BlockList[i].DrawLine(0xff, 1);
    }
}

auto MCTerrainWindow::DrawTopView() -> void
{
    if (MCTerrain::CurrentPass != 0)
    {
        return;
    }

    // Lays the grid out flat in the top-left square of the window.
    const int32_t side = std::min(GlobalWindow->XMax, GlobalWindow->YMax);
    const int32_t step = side / MCTerrain::VisibleVerticesPerSide;
    int32_t col = 0;
    int32_t rowY = 0;

    for (int32_t i = 0; i < NumVertices; i++)
    {
        VertexList[i].Px = step * col;
        VertexList[i].Py = rowY;
        col++;

        if (col == MCTerrain::VisibleVerticesPerSide)
        {
            col = 0;
            rowY += step;
        }
    }
}

auto MCTerrainWindow::DrawVertices() -> void
{
    // One pixel per vertex, in the top-right corner of the window.
    const int32_t left = GlobalWindow->XMax - MCTerrain::VisibleVerticesPerSide;
    int32_t col = 0;
    int32_t row = 0;

    for (int32_t i = 0; i < NumVertices; i++)
    {
        AGPixelWrite(GlobalPane, left + col, row, 0x10);
        col++;

        if (col == MCTerrain::VisibleVerticesPerSide)
        {
            col = 0;
            row++;
        }
    }
}
