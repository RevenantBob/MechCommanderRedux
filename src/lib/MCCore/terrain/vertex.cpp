#include "stdafx.h"
#include "terrain/vertex.h"
#include "ai/move.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/bitflag.h"
#include "engine/ceglist.h"
#include "engine/celine.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/cvmath.h"
#include "lib/packet.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "object/elemntl.h"
#include "object/team.h"
#include "platform/MCRenderer.h"
#include "terrain/terrain.h"
#include "terrain/terrtxm.h"
#include "vfx/vfx.h"
#include "vfx/vfxfuncs.h"

int32_t TileCacheReqs = 0;
int32_t TileCacheHits = 0;
int32_t TileCacheMiss = 0;
int32_t NumTerrainFaces = 0;

namespace
{
    /// <summary>The block file could not be created.</summary>
    constexpr int32_t NO_BLOCK_FILE = static_cast<int32_t>(0xbaaa000e);

    /// <summary>Radians to degrees (the double at 0x0077c278).</summary>
    constexpr double RADS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;

    /// <summary>The tag at the start of a fast-shape table ("DNAH" read as a little-endian int).</summary>
    constexpr int32_t FAST_SHAPE_TAG = 'D' | ('N' << 8) | ('A' << 16) | ('H' << 24);

    /// <summary>Tile number of the first mine tile (by the corners' relative elevations).</summary>
    constexpr int32_t MINE_TILE_BASE = 0xed1;
    /// <summary>Tile number of the exploded-mine crater.</summary>
    constexpr int32_t MINE_EXPLODED_TILE = 0xf21;

    /// <summary>Tile types that are drawn flat at their highest corner (bridge decks).</summary>
    bool IsFlatTileType(uint32_t tileType)
    {
        return tileType == 0x25 || tileType == 0x26 || tileType == 0x27 || tileType == 0x28;
    }

    /// <summary>
    /// The cached terrain tile <paramref name="tileNum"/> of the current view's tile set, read in when it isn't
    /// loaded; counts the request, hit or miss. The binary inlines this in every tile draw.
    /// </summary>
    MCTerrainTile* LookupTile(int32_t tileNum)
    {
        TileCacheReqs++;

        if (tileNum < 0)
        {
            return nullptr;
        }

        uint32_t tileSet = Eye->CameraScale == 1 ? 1 : 0;

        if (tileSet > 1)
        {
            tileSet = 0;
        }

        tileNum += TerrainTiles->TileSetOffset[tileSet];

        if (tileNum >= TerrainTiles->NumTiles)
        {
            return nullptr;
        }

        MCTerrainTile* tile = &TerrainTiles->Tiles[tileNum];

        if (tile->TileData == MCTerrainTile::TILE_MISSING)
        {
            return nullptr;
        }

        if (tile->TileData == nullptr)
        {
            TileCacheMiss++;
            return TerrainTiles->ReadTile(tileNum);
        }

        tile->LastTurnUsed = Turn;
        TileCacheHits++;
        return tile;
    }

    /// <summary>The home team's visible-this-frame bits (the Clans' when the home team's alignment is -1).</summary>
    MCByteFlag* HomeVisibleBits()
    {
        return HomeTeam->Alignment != -1 ? MCTerrain::TerrainVisibleBits : MCTerrain::ClanVisibleBits;
    }

    /// <summary>How many of the block's corners the home team sees this frame.</summary>
    uint32_t CountVisibleCorners(MCTerrainBlock* block)
    {
        uint32_t visibleCount = 0;

        for (MCVertex* vertex : block->Vertices)
        {
            const uint32_t row = static_cast<uint32_t>(static_cast<int32_t>(vertex->PosTile) >> 16);
            const uint32_t col = vertex->PosTile & 0xffff;

            if (HomeVisibleBits()->GetFlag(row, col) != 0)
            {
                visibleCount++;
            }

            // The original also reads the home team's seen bits here, into a table nothing reads.
        }

        return visibleCount;
    }

    /// <summary>The haze palette for a block with <paramref name="visibleCount"/> visible corners.</summary>
    uint8_t* HazePaletteFor(int32_t hazeFactor, uint32_t visibleCount)
    {
        const int32_t hazed = Eye->HazeInc * static_cast<int32_t>(visibleCount) + hazeFactor;
        const int32_t hazeLevel = (hazeFactor < 0 && hazed > 0) ? 0 : hazed;
        return GamePalette->GetHazePalette(hazeLevel);
    }

    /// <summary>
    /// Draws a fast-shape overlay tile at the block's top-left vertex.
    /// </summary>
    /// <returns>False when the tile's data isn't a fast-shape table (the caller then stops drawing).</returns>
    bool DrawOverlayShape(MCTerrainTile* tile, MCVertex* topLeft, uint8_t* hazePalette)
    {
        if (tile == nullptr || tile->TileData == nullptr)
        {
            return true;
        }

        int32_t tag = 0;
        std::memcpy(&tag, tile->TileData, sizeof(tag));

        if (tag != FAST_SHAPE_TAG)
        {
            return false;
        }

        FastShapeDraw(GlobalPane, tile->TileData, 0, topLeft->Px, topLeft->Py, hazePalette, 0);
        return true;
    }

    /// <summary>Draws the mine tile matching the block's corner elevations.</summary>
    /// <returns>False when a tile's data isn't a fast-shape table.</returns>
    bool DrawMineTile(MCTerrainBlock* block, uint8_t* hazePalette)
    {
        const uint32_t e0 = block->Vertices[0]->PVertex->Elevation;
        const uint32_t e1 = block->Vertices[1]->PVertex->Elevation;
        const uint32_t e2 = block->Vertices[2]->PVertex->Elevation;
        const uint32_t e3 = block->Vertices[3]->PVertex->Elevation;
        const uint32_t lowest = std::min({e0, e1, e2, e3});
        // The corners' elevations above the lowest, as base-3 digits.
        const int32_t tileNum = static_cast<int32_t>(((e0 * 3 + e1) * 3 + e2) * 3 + e3) -
                                static_cast<int32_t>(lowest) * 0x28 + MINE_TILE_BASE;
        return DrawOverlayShape(LookupTile(tileNum), block->Vertices[0], hazePalette);
    }

    /// <summary>The map tile at (<paramref name="row"/>, <paramref name="col"/>), asserting it is on the map.</summary>
    MCMapTile& MapTileAt(int32_t row, int32_t col)
    {
        const int ok = (row >= 0 && row < GameMap->Height && col >= 0 && col < GameMap->Width) ? 1 : 0;
        Assert(ok, 0, " Map Tile out of bounds ");
        return GameMap->Map[GameMap->Width * row + col];
    }

    /// <summary>The elevation level of a tile (bits 7-12 of its cells word).</summary>
    uint32_t TileElevation(uint32_t cells)
    {
        return (cells >> 7) & 0x3f;
    }

    /// <summary>
    void NormalizeX87(MCVector3D& v)
    {
        const double length = std::sqrt((static_cast<double>(v.X) * v.X + static_cast<double>(v.Y) * v.Y) +
                                        static_cast<double>(v.Z) * v.Z);

        if (length > 0.0)
        {
            v.X = static_cast<float>(v.X / length);
            v.Y = static_cast<float>(v.Y / length);
            v.Z = static_cast<float>(v.Z / length);
        }
    }

    MCVector3D CrossX87(const MCVector3D& a, const MCVector3D& b)
    {
        return MCVector3D(static_cast<float>(static_cast<double>(a.Y) * b.Z - static_cast<double>(a.Z) * b.Y),
                          static_cast<float>(static_cast<double>(a.Z) * b.X - static_cast<double>(a.X) * b.Z),
                          static_cast<float>(static_cast<double>(a.X) * b.Y - static_cast<double>(a.Y) * b.X));
    }

    /// The two edge vectors of the face (triangle) of the tile under <paramref name="pos"/>, from its top-left
    /// corner, and that corner's height. Shared by terrainAngle and terrainNormal (the binary repeats it).
    /// </summary>
    void FaceVectors(const MCVector3D& pos, MCVector3D& edge1, MCVector3D& edge2, float& cornerZ)
    {
        const float mpv = MCTerrain::MetersPerVertex;
        const float oneOver = MCTerrain::OneOvermetersPerVertex;
        const float cornerX = static_cast<float>(mpv * std::floor(static_cast<double>(oneOver) * pos.X));
        const float cornerY = static_cast<float>(mpv * (std::floor(static_cast<double>(oneOver) * pos.Y) + 1.0));
        const double gridX = static_cast<double>(oneOver) * cornerX;
        const float gridY = oneOver * cornerY;
        const int32_t half = (MCTerrain::BlocksMapSide * MCTerrain::VerticesBlockSide) >> 1;
        const int32_t col = static_cast<int32_t>(std::floor(gridX)) + half;
        const int32_t row = half - static_cast<int32_t>(std::floor(static_cast<double>(gridY)));

        const uint32_t tileA = MapTileAt(row, col).Cells;
        const uint32_t tileB = MapTileAt(row, col + 1).Cells;
        const uint32_t tileC = MapTileAt(row + 1, col + 1).Cells;
        const uint32_t tileD = MapTileAt(row + 1, col).Cells;
        const int32_t base = GameMap->BaseElevation;
        const float mpe = MCTerrain::MetersPerElevLevel;
        const auto levelOf = [base](uint32_t cells) -> double
        { return static_cast<double>(static_cast<int64_t>(TileElevation(cells) + base)); };

        const float x0 = static_cast<float>(std::floor(gridX) * mpv);
        const float y0 = static_cast<float>(std::floor(static_cast<double>(gridY)) * mpv);
        cornerZ = static_cast<float>(levelOf(tileA) * mpe);
        const double xPlus = static_cast<double>(x0) + mpv;
        const double offsetX = std::fabs(static_cast<double>(pos.X) - cornerX);
        const double offsetY = std::fabs(static_cast<double>(cornerY) - pos.Y);

        // The face is the triangle (A, D, C) nearer the x edge, else (A, B, C).
        if (!(offsetY >= offsetX))
        {
            const float zB = static_cast<float>(levelOf(tileB) * mpe);
            const float y1 = y0 - mpv;
            const float zC = static_cast<float>(levelOf(tileC) * mpe);
            const float dx = static_cast<float>(xPlus - x0);
            edge1 = MCVector3D(dx, 0.0f, zB - cornerZ);
            edge2 = MCVector3D(dx, y1 - y0, zC - cornerZ);
        }
        else
        {
            const float y1 = y0 - mpv;
            const float zC = static_cast<float>(levelOf(tileC) * mpe);
            const double zD = levelOf(tileD) * mpe;
            const float dy = y1 - y0;
            edge1 = MCVector3D(0.0f, dy, static_cast<float>(zD - cornerZ));
            edge2 = MCVector3D(static_cast<float>(xPlus - x0), dy, zC - cornerZ);
        }

        NormalizeX87(edge1);
        NormalizeX87(edge2);
    }
}

auto MCMapBlockManager::Destroy() -> void
{
    if (BlockFile != nullptr)
    {
        BlockFile->Close();
        delete BlockFile;
    }

    BlockFile = nullptr;
    Blocks = {};
    BlockData = {};
}

auto MCMapBlockManager::Init(char* fileName, int32_t numBlocks, int32_t blockSize) -> int32_t
{
    MCFullPathFileName blockName;
    blockName.Init(TerrainPath, fileName, ".elv");
    BlockFile = new MCPacketFile;

    if (BlockFile == nullptr)
    {
        return NO_BLOCK_FILE;
    }

    int32_t result = BlockFile->Open(blockName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    const uint32_t size = BlockFile->FileSize();

    if (size == 0x23318 || size == 0x4f2ec)
    {
        Fatal(-1, " Old Map format.  Resave in Teditor! ");
    }

    if (Blocks.empty())
    {
        // Every block plus the off-map one.
        const int32_t count = numBlocks + 1;
        BlockData.assign(static_cast<size_t>(count) * blockSize, 0);
        Blocks.resize(static_cast<size_t>(count));

        for (int32_t i = 0; i < count; i++)
        {
            const int32_t perBlock = MCTerrain::VerticesBlockSide * MCTerrain::VerticesBlockSide;
            auto* block = reinterpret_cast<MCPrecompVertex*>(BlockData.data() + static_cast<size_t>(perBlock) * i *
                                                                                    sizeof(MCPrecompVertex));
            Blocks[i] = block;
            BlockFile->ReadPacket(i, reinterpret_cast<uint8_t*>(block));
        }
    }

    // Per-window tables, sized by the block count as the original's.
    LastBlock.assign(static_cast<size_t>(numBlocks), -1);
    CurrentBlock.assign(static_cast<size_t>(numBlocks), 0);
    BlockSteps.assign(static_cast<size_t>(numBlocks), 0);
    VertexOffsets.assign(static_cast<size_t>(numBlocks) * 2, 0.0f);

    BlockFile->Close();
    return 0;
}

auto MCMapBlockManager::BlockPtr(int32_t blockNum) -> MCPrecompVertex*
{
    const int32_t total = MCTerrain::BlocksMapSide * MCTerrain::BlocksMapSide;

    if (blockNum < 0 || blockNum >= total)
    {
        blockNum = total;
    }

    return Blocks[blockNum];
}

auto MCMapBlockManager::GetTopLeftElevation() -> float
{
    if (BlockFile == nullptr)
    {
        return 0.0f;
    }

    return static_cast<float>(Blocks[0]->Elevation) * MCTerrain::MetersPerElevLevel;
}

auto MCMapBlockManager::Update(MCVector3D& cameraPos, int32_t windowNum) -> int32_t
{
    if (Turn == 1)
    {
        // The off-map block sits one level above the map's base.
        MCPrecompVertex* offMap = Blocks[MCTerrain::BlocksMapSide * MCTerrain::BlocksMapSide];
        const int32_t count = MCTerrain::VerticesBlockSide * MCTerrain::VerticesBlockSide;

        for (int32_t i = 0; i < count; i++)
        {
            offMap[i].Elevation = static_cast<uint8_t>(static_cast<uint8_t>(GameMap->BaseElevation) + 1);
        }
    }

    const MCVector3D& topLeft = MCTerrain::MapTopLeft3d100;
    const float fromTop = topLeft.Y - cameraPos.Y;
    const int32_t blockX =
        static_cast<int32_t>(std::floor(std::fabs((topLeft.X - cameraPos.X) / MCTerrain::MetersBlockSide)));
    TopLeftBlockX = blockX;
    const int32_t blockY = static_cast<int32_t>(std::floor(std::fabs(fromTop / MCTerrain::MetersBlockSide)));
    TopLeftBlockY = blockY;
    const int32_t block = blockY * MCTerrain::BlocksMapSide + blockX;
    CurrentBlock[windowNum] = block;

    if (block != LastBlock[windowNum])
    {
        LastBlock[windowNum] = block;
    }

    float* offset = &VertexOffsets[windowNum * 2];
    offset[0] = (cameraPos.X - topLeft.X) * MCTerrain::OneOvermetersPerVertex;
    offset[1] = (topLeft.Y - cameraPos.Y) * MCTerrain::OneOvermetersPerVertex;
    const int32_t vertexX = static_cast<int32_t>(std::floor(offset[0]));
    const int32_t vertexY = static_cast<int32_t>(std::floor(offset[1]));
    const int32_t half = MCTerrain::VisibleVerticesPerSide >> 1;
    const int32_t inBlockX = vertexX % MCTerrain::VerticesBlockSide;
    const int32_t inBlockY = vertexY % MCTerrain::VerticesBlockSide;
    const int32_t cornerX = static_cast<int32_t>(std::floor(static_cast<float>(vertexX) - static_cast<float>(half)));
    const int32_t cornerY = static_cast<int32_t>(std::floor(static_cast<float>(vertexY) - static_cast<float>(half)));
    Land->GetTerrainWindow(windowNum)->TopLeftX = static_cast<float>(cornerX) * MCTerrain::MetersPerVertex + topLeft.X;
    Land->GetTerrainWindow(windowNum)->TopLeftY = topLeft.Y - static_cast<float>(cornerY) * MCTerrain::MetersPerVertex;

    // The corner's vertex within its block, as seen from the camera's block. The literal 20 and 15 are the
    // original's (a 20-vertex block side).
    int32_t cornerInBlockX = cornerX < 0 ? cornerX + 20 : cornerX % MCTerrain::VerticesBlockSide;
    int32_t cornerInBlockY = cornerY < 0 ? cornerY + 20 : cornerY % MCTerrain::VerticesBlockSide;

    if (inBlockX >= 15)
    {
        cornerInBlockX += 20;
    }

    if (inBlockY >= 15)
    {
        cornerInBlockY += 20;
    }

    if (half > 15)
    {
        // Port fix: the literals only hold for the data's 30-vertex grid. A grid grown for a larger screen
        // (Terrain::init) measures the corner from the block before the camera's, as the literals do for 30; it can
        // then lie more blocks back, which buildWindow steps to.
        cornerInBlockX = cornerX - (blockX - 1) * MCTerrain::VerticesBlockSide;
        cornerInBlockY = cornerY - (blockY - 1) * MCTerrain::VerticesBlockSide;
    }

    offset[0] = static_cast<float>(cornerInBlockX);
    offset[1] = static_cast<float>(cornerInBlockY);
    return 0;
}

auto MCMapBlockManager::BuildWindow(MCVertex* vertexList, int32_t* numVertices, MCTerrainBlock* blockList,
                                    int32_t* numBlocks, int32_t windowNum) -> void
{
    const int32_t side = MCTerrain::VerticesBlockSide;
    float* offset = &VertexOffsets[windowNum * 2];
    int32_t startX = static_cast<int32_t>(std::floor(offset[0]));
    int32_t vertexY = static_cast<int32_t>(std::floor(offset[1]));

    // An offset past the block's side moves the corner into the next block.
    // Original behaviour (OB-102): in the map's last block column that wraps to the next block row.
    if (startX >= side)
    {
        BlockSteps[windowNum] += 1;
        offset[0] = offset[0] - static_cast<float>(side);
        CurrentBlock[windowNum] += 1;
        startX = static_cast<int32_t>(std::floor(offset[0]));
    }

    if (vertexY >= side)
    {
        const int32_t blocksSide = MCTerrain::BlocksMapSide;
        BlockSteps[windowNum] += blocksSide;
        offset[1] = offset[1] - static_cast<float>(side);
        CurrentBlock[windowNum] += blocksSide;
        vertexY = static_cast<int32_t>(std::floor(offset[1]));
    }

    // Port fix: a grid grown past 41 vertices (Terrain::init) starts more than one block before the camera's.
    int32_t blocksBackX = 0;
    int32_t blocksBackY = 0;

    while (startX < 0)
    {
        startX += side;
        blocksBackX++;
    }
    while (vertexY < 0)
    {
        vertexY += side;
        blocksBackY++;
    }

    *numVertices = 0;
    const int32_t blocksSide = MCTerrain::BlocksMapSide;
    const int32_t corner = CurrentBlock[windowNum];
    TopLeftBlockY = corner / blocksSide - 1 - blocksBackY;
    const int32_t firstBlockX = corner % blocksSide - 1 - blocksBackX;
    TopLeftBlockX = firstBlockX;
    const int32_t offMap = blocksSide * blocksSide;

    int32_t blockNum = offMap;

    if (TopLeftBlockY >= 0 && firstBlockX >= 0)
    {
        blockNum = TopLeftBlockY * blocksSide + firstBlockX;
    }

    MCPrecompVertex* source = BlockPtr(blockNum);

    if (source == nullptr)
    {
        Fatal(0, " Terrain System has trashed Memory! ");
    }

    source += MCTerrain::VerticesBlockSide * vertexY + startX;

    MCVertex* vertex = vertexList;

    for (int32_t row = 0; row < MCTerrain::VisibleVerticesPerSide; row++)
    {
        int32_t vertexX = startX;

        for (int32_t col = 0; col < MCTerrain::VisibleVerticesPerSide; col++)
        {
            const int32_t blockY = TopLeftBlockY;
            vertex->PVertex = source;
            vertex->BlockNum = static_cast<int16_t>(static_cast<int16_t>(TopLeftBlockY) *
                                                        static_cast<int16_t>(MCTerrain::BlocksMapSide) +
                                                    static_cast<int16_t>(TopLeftBlockX));
            vertex->VertexNum = static_cast<int16_t>(static_cast<int16_t>(vertexY) * static_cast<int16_t>(side) +
                                                     static_cast<int16_t>(vertexX));
            vertex->PosTile = static_cast<uint32_t>((blockY * side + vertexY) * 0x10000) +
                              (static_cast<uint32_t>(TopLeftBlockX * side + vertexX) & 0xffff);
            vertex++;
            (*numVertices)++;
            vertexX++;
            source++;

            if (vertexX == side)
            {
                vertexX = 0;
                TopLeftBlockX++;
                blockNum = offMap;

                // Port fix: the grown grid can step across blocks left of the map, which aren't the row before's.
                if (TopLeftBlockX >= 0 && TopLeftBlockX < MCTerrain::BlocksMapSide)
                {
                    blockNum = blockY * MCTerrain::BlocksMapSide + TopLeftBlockX;
                }

                source = BlockPtr(blockNum);

                if (source == nullptr)
                {
                    Fatal(0, " Terrain System has trashed Memory! ");
                }

                source += MCTerrain::VerticesBlockSide * vertexY;
            }
        }

        vertexY++;
        TopLeftBlockX = firstBlockX;

        if (vertexY == side)
        {
            vertexY = 0;
            TopLeftBlockY++;
        }

        blockNum = offMap;

        if (firstBlockX >= 0 && TopLeftBlockY >= 0 && firstBlockX <= MCTerrain::BlocksMapSide - 1 &&
            TopLeftBlockY <= MCTerrain::BlocksMapSide - 1)
        {
            blockNum = TopLeftBlockY * MCTerrain::BlocksMapSide + firstBlockX;
        }

        source = BlockPtr(blockNum);

        if (source == nullptr)
        {
            Fatal(0, " MapBlock Not Cached ");
        }

        source += MCTerrain::VerticesBlockSide * vertexY + startX;
    }

    // One block per grid square: corners top-left, top-right, bottom-right, bottom-left.
    const int32_t perSide = MCTerrain::VisibleVerticesPerSide;

    *numBlocks = 0;
    for (int32_t row = 0; row < perSide - 1; row++)
    {
        for (int32_t col = 0; col < perSide - 1; col++)
        {
            MCVertex* topLeft = vertexList + row * perSide + col;
            blockList->Init(topLeft, topLeft + 1, topLeft + perSide + 1, topLeft + perSide);
            blockList++;
            (*numBlocks)++;
        }
    }
}

auto MCMapBlockManager::SetOverlayTile(int32_t blockNum, int32_t vertexNum, int32_t value) -> void
{
    Blocks[blockNum][vertexNum].OverlayData = static_cast<int16_t>(Blocks[blockNum][vertexNum].OverlayData + value);
}

auto MCMapBlockManager::GetOverlayTile(int32_t blockNum, int32_t vertexNum) -> int32_t
{
    return Blocks[blockNum][vertexNum].OverlayData;
}

auto MCMapBlockManager::SetTile(int32_t blockNum, int32_t vertexNum, int32_t value) -> void
{
    Blocks[blockNum][vertexNum].TextureData = static_cast<int16_t>(Blocks[blockNum][vertexNum].TextureData + value);
}

auto MCMapBlockManager::GetTile(int32_t blockNum, int32_t vertexNum) -> int32_t
{
    return Blocks[blockNum][vertexNum].TextureData;
}

auto MCMapBlockManager::MarkSeen(MCVector2D& /*topLeft*/, MCVertex* /*vertexList*/, MCVector3D& /*looker*/,
                                 MCVector3D& /*lookVector*/, float /*angle*/, float /*range*/, uint8_t /*who*/) -> void
{
}

auto MCMapBlockManager::TerrainAngle(MCVector3D& pos, MCVector3D* normal) -> float
{
    MCVector3D edge1;
    MCVector3D edge2;
    float cornerZ = 0.0f;
    FaceVectors(pos, edge1, edge2, cornerZ);
    MCVector3D faceNormal = CrossX87(edge1, edge2);

    float angle = 0.0f;

    if (faceNormal.Z == 0.0f || std::isnan(faceNormal.Z))
    {
        // Faithful: a vertical face returns the corner's height, not an angle.
        angle = cornerZ;
    }
    else
    {
        NormalizeX87(faceNormal);
        angle = static_cast<float>(AcosMatherr(static_cast<double>(faceNormal.Z)) * RADS_TO_DEGREES);
    }

    if (normal != nullptr)
    {
        *normal = faceNormal;
    }

    return angle;
}

auto MCMapBlockManager::TerrainNormal(MCVector3D& pos) -> MCVector3D
{
    MCVector3D edge1;
    MCVector3D edge2;
    float cornerZ = 0.0f;
    FaceVectors(pos, edge1, edge2, cornerZ);
    MCVector3D faceNormal = CrossX87(edge2, edge1);

    if (!(faceNormal.Z >= 0.0f))
    {
        faceNormal = CrossX87(edge1, edge2);
    }

    NormalizeX87(faceNormal);
    return faceNormal;
}

auto MCMapBlockManager::TerrainElevation(MCVector3D& pos) -> float
{
    return TerrainElevationAt(pos);
}

auto TerrainElevationAt(MCVector3D& pos) -> float
{
    using Ext = double;
    const float mpv = MCTerrain::MetersPerVertex;
    const float oneOver = MCTerrain::OneOvermetersPerVertex;
    const float cornerX = static_cast<float>(mpv * std::floor(static_cast<double>(oneOver) * pos.X));
    const float cornerY = static_cast<float>(mpv * (std::floor(static_cast<double>(oneOver) * pos.Y) + 1.0));
    const double gridX = static_cast<double>(oneOver) * cornerX;
    const double gridY = static_cast<double>(static_cast<float>(oneOver * cornerY));
    const int32_t half = (MCTerrain::BlocksMapSide * MCTerrain::VerticesBlockSide) >> 1;
    const int32_t col = static_cast<int32_t>(std::floor(gridX)) + half;
    const int32_t row = half - static_cast<int32_t>(std::floor(gridY));

    if (row < 0 || row >= GameMap->Height || col < 0 || col >= GameMap->Width)
    {
        return 0.0f;
    }

    if (row + 1 < 0 || row + 1 >= GameMap->Height || col + 1 < 0 || col + 1 >= GameMap->Width)
    {
        return 0.0f;
    }

    const MCMapTile& tileA = MapTileAt(row, col);
    const uint32_t cellsA = tileA.Cells;
    const uint32_t overlayA = tileA.Overlay;
    const uint32_t cellsB = MapTileAt(row, col + 1).Cells;
    const uint32_t cellsC = MapTileAt(row + 1, col + 1).Cells;
    const uint32_t cellsD = MapTileAt(row + 1, col).Cells;

    if (IsFlatTileType(overlayA & 0x7f))
    {
        uint32_t level = TileElevation(cellsA);
        const uint32_t levelB = TileElevation(cellsB);
        const uint32_t levelC = TileElevation(cellsC);
        const uint32_t levelD = TileElevation(cellsD);

        if (levelB == level && levelB == levelC && levelC == levelD)
        {
            level++;
        }
        else
        {
            level = std::max({level, levelB, levelC, levelD});
        }

        return static_cast<float>(static_cast<Ext>(static_cast<int32_t>(GameMap->BaseElevation + level)) *
                                  MCTerrain::MetersPerElevLevel);
    }

    const int32_t base = GameMap->BaseElevation;
    const float mpe = MCTerrain::MetersPerElevLevel;
    const auto levelOf = [base](uint32_t cells)
    { return static_cast<int64_t>(static_cast<uint32_t>(static_cast<int32_t>(TileElevation(cells)) + base)); };
    const float x0 = static_cast<float>(std::floor(gridX) * mpv);
    const float y0 = static_cast<float>(std::floor(gridY) * mpv);
    const Ext elevA = static_cast<Ext>(levelOf(cellsA)) * mpe;
    const float offsetX = std::fabs(pos.X - cornerX);
    const float offsetY = std::fabs(cornerY - pos.Y);
    const Ext offsetXExt = std::fabs(static_cast<Ext>(pos.X) - cornerX);

    Ext uX;
    Ext uY;
    Ext vX;
    Ext vY;
    Ext vZ;
    float uZ;

    if (offsetXExt > static_cast<Ext>(offsetY))
    {
        const float elevB = static_cast<float>(static_cast<Ext>(levelOf(cellsB)) * mpe);
        const float y1 = static_cast<float>(y0 - mpv);
        const float elevC = static_cast<float>(static_cast<Ext>(levelOf(cellsC)) * mpe);
        const Ext spanX = (static_cast<Ext>(x0) + mpv) - x0;
        uZ = static_cast<float>(static_cast<Ext>(elevB) - elevA);
        uX = spanX;
        uY = 0.0;
        vX = static_cast<float>(spanX);
        vY = static_cast<Ext>(y1) - y0;
        vZ = static_cast<Ext>(elevC) - elevA;
    }
    else
    {
        const float x1 = static_cast<float>(static_cast<Ext>(x0) + mpv);
        const float y1 = static_cast<float>(y0 - mpv);
        const float elevC = static_cast<float>(static_cast<Ext>(levelOf(cellsC)) * mpe);
        const Ext elevD = static_cast<Ext>(levelOf(cellsD)) * mpe;
        const float spanY = static_cast<float>(static_cast<Ext>(y1) - y0);
        uZ = static_cast<float>(elevD - elevA);
        uX = 0.0;
        uY = spanY;
        vX = static_cast<Ext>(x1) - x0;
        vY = spanY;
        vZ = static_cast<Ext>(elevC) - elevA;
    }

    const float uYf = static_cast<float>(uY);
    const Ext lengthU = std::sqrt((static_cast<Ext>(uYf) * uYf + static_cast<Ext>(uZ) * uZ) + uX * uX);

    if (lengthU != 0.0)
    {
        uX = uX / lengthU;
        uY = uY / lengthU;
        uZ = static_cast<float>(static_cast<Ext>(uZ) / lengthU);
    }

    const float vZf = static_cast<float>(vZ);
    const float vYf = static_cast<float>(vY);
    const Ext lengthV = std::sqrt((vX * vX + static_cast<Ext>(vZf) * vZf) + static_cast<Ext>(vYf) * vYf);

    if (lengthV != 0.0)
    {
        vX = vX / lengthV;
        vY = vY / lengthV;
        vZ = vZ / lengthV;
    }

    // The plane through the corner: z = z0 - (nx/nz) * dx + (ny/nz) * dy.
    const float normalX = static_cast<float>(vZ * uY - vY * static_cast<Ext>(uZ));
    const float normalY = static_cast<float>(static_cast<Ext>(uZ) * vX - vZ * uX);
    const float normalZ = static_cast<float>(vY * uX - uY * vX);

    if (normalZ == 0.0f)
    {
        return static_cast<float>(elevA);
    }

    Ext nx = normalX;
    Ext ny = normalY;
    Ext nz = normalZ;

    if (normalZ < 0.0f)
    {
        nx = -nx;
        ny = -ny;
        nz = -nz;
    }

    return static_cast<float>(elevA +
                              -((ny / nz) * static_cast<Ext>(-offsetY) + (nx / nz) * static_cast<Ext>(offsetX)));
}

auto MCMapBlockManager::GenerateRandomBlock(MCPrecompVertex* block) -> void
{
    const int32_t count = MCTerrain::VerticesBlockSide * MCTerrain::VerticesBlockSide;

    for (int32_t i = 0; i < count; i++)
    {
        block[i].Elevation = static_cast<uint8_t>(GameMap->BaseElevation);
        block[i].TileGroup = 0;
        block[i].TextureData = 0x29;
        block[i].OverlayData = 0x29;
    }
}

auto MCTerrainBlock::Init(MCVertex* v0, MCVertex* v1, MCVertex* v2, MCVertex* v3) -> int32_t
{
    Vertices[0] = v0;
    Vertices[1] = v1;
    Vertices[2] = v2;
    Vertices[3] = v3;
    return 0;
}

auto MCTerrainBlock::Draw(int32_t hazeFactor, uint8_t /*flags*/) -> void
{
    MCVertex* topLeft = Vertices[0];
    uint32_t redrawCount = 0;
    uint32_t clippedCount = 0;

    for (MCVertex* vertex : Vertices)
    {
        redrawCount += vertex->Redraw;
        clippedCount += vertex->Clipped;
    }

    if (clippedCount == 4)
    {
        return;
    }

    uint8_t* hazePalette = nullptr;
    uint32_t visibleCount = 0;

    if (hazeFactor != 0x7fff)
    {
        visibleCount = CountVisibleCorners(this);

        if (visibleCount != LastVisibleCount)
        {
            redrawCount++;

            for (MCVertex* vertex : Vertices)
            {
                vertex->EdgeRedraw = 1;
            }
        }

        if (visibleCount != 4)
        {
            hazePalette = HazePaletteFor(hazeFactor, visibleCount);
        }
    }

    // Unseen blocks (and the 0x7fff "all black" factor) are filled black.
    if (hazeFactor == 0x7fff || visibleCount == 0)
    {
        hazePalette = VFX_TILE_FILL;
    }

    const int32_t textureData = topLeft->PVertex->TextureData;
    MCTerrainTile* terrainTile = nullptr;

    if (textureData < 0)
    {
        TileCacheReqs++;
    }
    else
    {
        terrainTile = LookupTile(textureData);
    }

    Tile = terrainTile;
    NumTerrainFaces++;

    if (terrainTile != nullptr && terrainTile->TileData != nullptr && redrawCount != 0)
    {
        VfxNTileDraw(GlobalPane, terrainTile->TileData, topLeft->Px, topLeft->Py, hazePalette);
    }
}

auto MCTerrainBlock::DrawOverlay(int32_t hazeFactor, uint8_t /*flags*/) -> void
{
    MCVertex* topLeft = Vertices[0];
    uint32_t clippedCount = 0;

    for (MCVertex* vertex : Vertices)
    {
        clippedCount += vertex->Clipped;
    }

    if (clippedCount == 4)
    {
        return;
    }

    if (hazeFactor == 0x7fff)
    {
        return;
    }

    const uint32_t visibleCount = CountVisibleCorners(this);

    if (visibleCount != LastVisibleCount)
    {
        LastVisibleCount = static_cast<uint8_t>(visibleCount);
    }

    uint8_t* hazePalette = nullptr;

    if (visibleCount != 4)
    {
        hazePalette = HazePaletteFor(hazeFactor, visibleCount);
    }

    if (visibleCount == 0)
    {
        return;
    }

    NumTerrainFaces++;
    const int32_t overlayData = topLeft->PVertex->OverlayData;
    MCTerrainTile* overlay = nullptr;

    if (overlayData < 0)
    {
        TileCacheReqs++;
    }
    else
    {
        overlay = LookupTile(overlayData);
    }

    OverlayTile = overlay;

    if (!DrawOverlayShape(overlay, topLeft, hazePalette))
    {
        return;
    }

    // Mines: the map tile's overlay word holds the Inner Sphere's (bits 11-12) and the Clans' (bits 13-14) mine
    // state; 2 = a known mine, 3 = an exploded one.
    const int32_t col = (topLeft->BlockNum % MCTerrain::BlocksMapSide) * MCTerrain::VerticesBlockSide +
                        topLeft->VertexNum % MCTerrain::VerticesBlockSide;
    const int32_t row = (topLeft->BlockNum / MCTerrain::BlocksMapSide) * MCTerrain::VerticesBlockSide +
                        topLeft->VertexNum / MCTerrain::VerticesBlockSide;
    const int ok = (row >= 0 && row < GameMap->Height && col >= 0 && col < GameMap->Width) ? 1 : 0;
    Assert(ok, 0, " bldng MapTile Out of Bounds ");
    const uint32_t mineWord = GameMap->Map[GameMap->Width * row + col].Overlay;
    const uint32_t clanMine = (mineWord >> 13) & 3;
    const uint32_t innerSphereMine = (mineWord >> 11) & 3;

    const bool playingInnerSphere = HomeTeam == InnerSphereTeam;

    if (playingInnerSphere && innerSphereMine == 2)
    {
        if (!DrawMineTile(this, hazePalette))
        {
            return;
        }
    }

    // The Clans' mines show when playing Clan, or in god mode.
    const bool showClanMines = !playingInnerSphere || Scenario->GodMode != 0;

    if (showClanMines && clanMine == 2)
    {
        if (!DrawMineTile(this, hazePalette))
        {
            return;
        }

        if (innerSphereMine != 3)
        {
            return;
        }
    }
    else if (clanMine != 3 && innerSphereMine != 3)
    {
        return;
    }

    MCTerrainTile* crater = LookupTile(MINE_EXPLODED_TILE);
    DrawOverlayShape(crater, topLeft, hazePalette);
}

auto MCTerrainBlock::DrawLine(int32_t color, int /*onlyTop*/) -> void
{
    uint32_t clippedCount = 0;

    for (MCVertex* vertex : Vertices)
    {
        clippedCount += vertex->Clipped;
    }

    if (clippedCount == 4)
    {
        return;
    }

    // Every edge is drawn at the top-left vertex's depth.
    const int32_t depth = Vertices[0]->Py;

    for (int32_t edge = 0; edge < 4; edge++)
    {
        MCVertex* from = Vertices[edge];
        MCVertex* to = Vertices[(edge + 1) & 3];
        MCVector2D start(static_cast<float>(from->Px), static_cast<float>(from->Py));
        MCVector2D end(static_cast<float>(to->Px), static_cast<float>(to->Py));
        ElementList->Add(MCElementPool::Make<MCLineElement>(start, end, color, nullptr, depth, -1));
    }
}

auto MCTerrainBlock::DrawHaze(int32_t /*hazeFactor*/, uint8_t /*flags*/) -> void
{
}

// Port: the ground mesh -------------------------------------------------------------------------------------------

namespace
{
    /// <summary>
    /// The map's ground mesh (MCTerrainMesh) and what it was made with: the steps it places vertices by, the tile set,
    /// and per mesh vertex the elevation and tile it holds (to check each frame's grid against).
    /// </summary>
    struct MCGroundMesh
    {
        MCTerrainMesh Mesh;
        int32_t StepX = 0;
        int32_t StepY = 0;
        int32_t ElevStep = 0;
        uint32_t TileSet = 0;
        /// <summary>The mesh's vertices a side ((Cols + 1) = (Rows + 1)), from (Mesh.FirstRow, Mesh.FirstCol).</summary>
        int32_t Side = 0;
        std::vector<uint8_t> Elevations;
        std::vector<int16_t> Textures;
    };

    std::unique_ptr<MCGroundMesh> GroundMesh;
    uint64_t GroundMeshVersions = 0;

    /// <summary>The map vertex at (<paramref name="row"/>, <paramref name="col"/>) as buildWindow finds it: in its
    /// block, or in the off-map block.</summary>
    const MCPrecompVertex* MapVertexAt(int32_t row, int32_t col)
    {
        const int32_t side = MCTerrain::VerticesBlockSide;
        const int32_t mapSide = MCTerrain::BlocksMapSide * side;
        int32_t blockNum = MCTerrain::BlocksMapSide * MCTerrain::BlocksMapSide;

        if (row >= 0 && col >= 0 && row < mapSide && col < mapSide)
        {
            blockNum = (row / side) * MCTerrain::BlocksMapSide + col / side;
        }

        const int32_t vertexY = ((row % side) + side) % side;
        const int32_t vertexX = ((col % side) + side) % side;
        return MCTerrain::MapBlockManager->BlockPtr(blockNum) + vertexY * side + vertexX;
    }

    /// <summary>
    /// Builds the ground mesh over the map and a ring of <paramref name="ring"/> off-map vertices around it, reading
    /// every tile it uses.
    /// </summary>
    void BuildGroundMesh(int32_t ring, int32_t stepX, int32_t stepY, int32_t elevStep, uint32_t tileSet)
    {
        auto built = std::make_unique<MCGroundMesh>();
        built->StepX = stepX;
        built->StepY = stepY;
        built->ElevStep = elevStep;
        built->TileSet = tileSet;
        const int32_t mapSide = MCTerrain::BlocksMapSide * MCTerrain::VerticesBlockSide;
        const int32_t side = mapSide + 2 * ring;
        built->Side = side;
        built->Elevations.resize(static_cast<size_t>(side) * side);
        built->Textures.resize(static_cast<size_t>(side) * side);

        for (int32_t r = 0; r < side; ++r)
        {
            for (int32_t c = 0; c < side; ++c)
            {
                const MCPrecompVertex* vertex = MapVertexAt(r - ring, c - ring);
                built->Elevations[static_cast<size_t>(r) * side + c] = vertex->Elevation;
                built->Textures[static_cast<size_t>(r) * side + c] = vertex->TextureData;
            }
        }

        MCTerrainMesh& mesh = built->Mesh;
        mesh.Version = ++GroundMeshVersions;
        mesh.FirstRow = -ring;
        mesh.FirstCol = -ring;
        mesh.Cols = side - 1;
        mesh.Rows = side - 1;
        mesh.CellTiles.resize(static_cast<size_t>(mesh.Cols) * mesh.Rows);
        mesh.CellElevations.resize(mesh.CellTiles.size());
        std::unordered_map<int32_t, uint32_t> tiles;

        for (int32_t r = 0; r < mesh.Rows; ++r)
        {
            for (int32_t c = 0; c < mesh.Cols; ++c)
            {
                const size_t at = static_cast<size_t>(r) * side + c;
                const size_t cell = static_cast<size_t>(r) * mesh.Cols + c;
                mesh.CellElevations[cell] = static_cast<uint32_t>(built->Elevations[at]) |
                                            static_cast<uint32_t>(built->Elevations[at + 1]) << 8 |
                                            static_cast<uint32_t>(built->Elevations[at + side + 1]) << 16 |
                                            static_cast<uint32_t>(built->Elevations[at + side]) << 24;

                // The tile TerrainBlock::draw would draw: none for a negative number or a tile that can't be read.
                const int32_t textureData = built->Textures[at];
                uint32_t index = MCTerrainMesh::NoTile;

                if (textureData >= 0)
                {
                    if (const auto found = tiles.find(textureData); found != tiles.end())
                    {
                        index = found->second;
                    }
                    else
                    {
                        const MCTerrainTile* tile = LookupTile(textureData);

                        if (tile != nullptr && tile->TileData != nullptr)
                        {
                            const uint8_t* data = tile->TileData;
                            uint32_t size = 0;
                            std::memcpy(&size, data + 4 + static_cast<size_t>(data[2]) * 4, sizeof(size));
                            index = static_cast<uint32_t>(mesh.Tiles.size());
                            mesh.Tiles.emplace_back(data, data + size);
                        }

                        tiles.emplace(textureData, index);
                    }
                }

                mesh.CellTiles[cell] = index;
            }
        }

        GroundMesh = std::move(built);
    }

    /// <summary>Whether the mesh's vertices take in the grid's, from (<paramref name="row"/>, <paramref name="col"/>).</summary>
    bool MeshCovers(int32_t row, int32_t col, int32_t perSide)
    {
        const int32_t first = GroundMesh->Mesh.FirstRow;
        return row >= first && col >= first && row + perSide <= first + GroundMesh->Side &&
               col + perSide <= first + GroundMesh->Side;
    }
}

std::expected<void, std::string> MCTerrainGroundFrame(const MCVertex* vertexList, int32_t numVertices,
                                                      int32_t numBlocks, int32_t hazeFactor, int32_t stepX,
                                                      int32_t stepY, int32_t elevStep, int32_t minX, int32_t maxX,
                                                      int32_t minY, int32_t maxY, MCTerrainFrame& frame)
{
    const int32_t perSide = MCTerrain::VisibleVerticesPerSide;

    if (UseOldProject != 0 || ProjectAll != 0 || UseNonIntegerAdditive != 0)
    {
        return std::unexpected("the ground mesh needs whole-pixel vertex steps (useOldProject, projectAll and "
                               "useNonIntegerAdditive off)");
    }

    if (vertexList == nullptr || perSide < 2 || numVertices != perSide * perSide ||
        numBlocks != (perSide - 1) * (perSide - 1))
    {
        return std::unexpected(std::format("the terrain grid has {} vertices and {} blocks for {} vertices a side",
                                           numVertices, numBlocks, perSide));
    }

    MCByteFlag* fog = HomeTeam != nullptr ? HomeVisibleBits() : nullptr;

    if (fog == nullptr || fog->FlagWindow == nullptr)
    {
        return std::unexpected("the ground mesh has no fog of war flags to read");
    }

    if (MCTerrain::MapBlockManager == nullptr || TerrainTiles == nullptr || GlobalPane == nullptr)
    {
        return std::unexpected("the ground mesh is asked for without a terrain");
    }

    const uint32_t tileSet = Eye->CameraScale == 1 ? 1 : 0;

    // The grid's corner, from its first vertex's map position.
    const uint32_t firstTile = vertexList[0].PosTile;
    const int32_t firstRow = static_cast<int32_t>(firstTile) >> 16;
    const int32_t firstCol = static_cast<int16_t>(firstTile & 0xffff);

    if (GroundMesh == nullptr || GroundMesh->StepX != stepX || GroundMesh->StepY != stepY ||
        GroundMesh->ElevStep != elevStep || GroundMesh->TileSet != tileSet || !MeshCovers(firstRow, firstCol, perSide))
    {
        BuildGroundMesh(perSide + 2, stepX, stepY, elevStep, tileSet);

        if (!MeshCovers(firstRow, firstCol, perSide))
        {
            return std::unexpected(std::format(
                "the ground mesh doesn't cover the terrain grid from map vertex ({}, {})", firstRow, firstCol));
        }
    }

    // Each grid vertex must be the map's vertex at its place, projected where the mesh puts it. The mesh is built again
    // when the map's own data changed since (setTile).
    const int32_t originX = vertexList[0].Px - (firstCol - firstRow) * stepX;
    const int32_t originY = vertexList[0].Py - (firstRow + firstCol) * stepY +
                            static_cast<int32_t>(vertexList[0].PVertex->Elevation) * elevStep;

    for (bool rebuilt = false;;)
    {
        bool current = true;

        for (int32_t i = 0; i < numVertices; ++i)
        {
            const MCVertex& vertex = vertexList[i];
            const int32_t row = firstRow + i / perSide;
            const int32_t col = firstCol + i % perSide;
            const uint32_t elevation = vertex.PVertex->Elevation;

            if (vertex.PosTile != static_cast<uint32_t>(row * 0x10000) + (static_cast<uint32_t>(col) & 0xffff))
            {
                return std::unexpected(std::format("terrain grid vertex {} isn't map vertex ({}, {}) (posTile {:08x})",
                                                   i, row, col, vertex.PosTile));
            }

            // OB-102's wrapped grid lands here.
            if (vertex.PVertex != MapVertexAt(row, col))
            {
                return std::unexpected(std::format(
                    "terrain grid vertex {} at map vertex ({}, {}) holds another vertex's data", i, row, col));
            }

            if (vertex.Redraw == 0)
            {
                return std::unexpected(std::format("terrain grid vertex {} isn't redrawn this frame", i));
            }

            const int32_t x = originX + (col - row) * stepX;
            const int32_t y = originY + (row + col) * stepY - static_cast<int32_t>(elevation) * elevStep;

            if (vertex.Px != x || vertex.Py != y)
            {
                return std::unexpected(std::format("terrain grid vertex {} projects to ({}, {}); the mesh puts it at "
                                                   "({}, {})",
                                                   i, vertex.Px, vertex.Py, x, y));
            }

            const size_t at = static_cast<size_t>(row - GroundMesh->Mesh.FirstRow) * GroundMesh->Side +
                              static_cast<size_t>(col - GroundMesh->Mesh.FirstCol);
            current = current && GroundMesh->Elevations[at] == elevation &&
                      GroundMesh->Textures[at] == vertex.PVertex->TextureData;
        }

        if (current)
        {
            break;
        }

        if (rebuilt)
        {
            return std::unexpected("the ground mesh doesn't hold the map's data even when built again");
        }

        BuildGroundMesh(perSide + 2, stepX, stepY, elevStep, tileSet);
        rebuilt = true;
    }

    frame.Mesh = &GroundMesh->Mesh;
    frame.OriginX = originX;
    frame.OriginY = originY;
    frame.StepX = stepX;
    frame.StepY = stepY;
    frame.ElevStep = elevStep;
    frame.FirstRow = firstRow;
    frame.FirstCol = firstCol;
    frame.LastRow = firstRow + perSide - 2;
    frame.LastCol = firstCol + perSide - 2;
    frame.MinX = minX;
    frame.MaxX = maxX;
    frame.MinY = minY;
    frame.MaxY = maxY;

    // VFX_nTile_draw's clip: the pane within its window and view.
    const MCWindow* window = GlobalPane->Window;
    int32_t x0 = std::max(GlobalPane->X0, 0);
    int32_t y0 = std::max(GlobalPane->Y0, 0);
    int32_t x1 = GlobalPane->X1 < window->XMax + 1 ? GlobalPane->X1 : window->XMax;
    int32_t y1 = GlobalPane->Y1 < window->YMax + 1 ? GlobalPane->Y1 : window->YMax;
    MCClipToView(window, x0, y0, x1, y1);
    frame.PaneX = GlobalPane->X0;
    frame.PaneY = GlobalPane->Y0;
    frame.Clip = MCRect{x0, y0, x1, y1};
    frame.Fog = fog->FlagWindow;
    frame.AllFilled = hazeFactor == 0x7fff;

    if (!frame.AllFilled)
    {
        for (uint32_t seen = 1; seen <= 3; ++seen)
        {
            frame.Haze[seen - 1] = HazePaletteFor(hazeFactor, seen);
        }
    }

    return {};
}

void MCTerrainForgetMesh()
{
    GroundMesh.reset();
}
