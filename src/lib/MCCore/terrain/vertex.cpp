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

int32_t tileCacheReqs = 0;
int32_t tileCacheHits = 0;
int32_t tileCacheMiss = 0;
int32_t numTerrainFaces = 0;

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
    bool isFlatTileType(uint32_t tileType)
    {
        return tileType == 0x25 || tileType == 0x26 || tileType == 0x27 || tileType == 0x28;
    }

    /// <summary>
    /// The cached terrain tile <paramref name="tileNum"/> of the current view's tile set, read in when it isn't
    /// loaded; counts the request, hit or miss. The binary inlines this in every tile draw.
    /// </summary>
    TerrainTile* lookupTile(int32_t tileNum)
    {
        tileCacheReqs++;

        if (tileNum < 0)
        {
            return nullptr;
        }

        uint32_t tileSet = eye->cameraScale == 1 ? 1 : 0;

        if (tileSet > 1)
        {
            tileSet = 0;
        }

        tileNum += terrainTiles->tileSetOffset[tileSet];

        if (tileNum >= terrainTiles->numTiles)
        {
            return nullptr;
        }

        TerrainTile* tile = &terrainTiles->tiles[tileNum];

        if (tile->tileData == TerrainTile::TILE_MISSING)
        {
            return nullptr;
        }

        if (tile->tileData == nullptr)
        {
            tileCacheMiss++;
            return terrainTiles->readTile(tileNum);
        }

        tile->lastTurnUsed = turn;
        tileCacheHits++;
        return tile;
    }

    /// <summary>The home team's visible-this-frame bits (the Clans' when the home team's alignment is -1).</summary>
    ByteFlag* homeVisibleBits()
    {
        return homeTeam->alignment != -1 ? Terrain::terrainVisibleBits : Terrain::ClanVisibleBits;
    }

    /// <summary>How many of the block's corners the home team sees this frame.</summary>
    uint32_t countVisibleCorners(TerrainBlock* block)
    {
        uint32_t visibleCount = 0;

        for (Vertex* vertex : block->vertices)
        {
            const uint32_t row = static_cast<uint32_t>(static_cast<int32_t>(vertex->posTile) >> 16);
            const uint32_t col = vertex->posTile & 0xffff;

            if (homeVisibleBits()->getFlag(row, col) != 0)
            {
                visibleCount++;
            }

            // The original also reads the home team's seen bits here, into a table nothing reads.
        }

        return visibleCount;
    }

    /// <summary>The haze palette for a block with <paramref name="visibleCount"/> visible corners.</summary>
    uint8_t* hazePaletteFor(int32_t hazeFactor, uint32_t visibleCount)
    {
        const int32_t hazed = eye->hazeInc * static_cast<int32_t>(visibleCount) + hazeFactor;
        const int32_t hazeLevel = (hazeFactor < 0 && hazed > 0) ? 0 : hazed;
        return gamePalette->getHazePalette(hazeLevel);
    }

    /// <summary>
    /// Draws a fast-shape overlay tile at the block's top-left vertex.
    /// </summary>
    /// <returns>False when the tile's data isn't a fast-shape table (the caller then stops drawing).</returns>
    bool drawOverlayShape(TerrainTile* tile, Vertex* topLeft, uint8_t* hazePalette)
    {
        if (tile == nullptr || tile->tileData == nullptr)
        {
            return true;
        }

        int32_t tag = 0;
        std::memcpy(&tag, tile->tileData, sizeof(tag));

        if (tag != FAST_SHAPE_TAG)
        {
            return false;
        }

        fastShapeDraw(globalPane, tile->tileData, 0, topLeft->px, topLeft->py, hazePalette, 0);
        return true;
    }

    /// <summary>Draws the mine tile matching the block's corner elevations.</summary>
    /// <returns>False when a tile's data isn't a fast-shape table.</returns>
    bool drawMineTile(TerrainBlock* block, uint8_t* hazePalette)
    {
        const uint32_t e0 = block->vertices[0]->pVertex->elevation;
        const uint32_t e1 = block->vertices[1]->pVertex->elevation;
        const uint32_t e2 = block->vertices[2]->pVertex->elevation;
        const uint32_t e3 = block->vertices[3]->pVertex->elevation;
        const uint32_t lowest = std::min({e0, e1, e2, e3});
        // The corners' elevations above the lowest, as base-3 digits.
        const int32_t tileNum = static_cast<int32_t>(((e0 * 3 + e1) * 3 + e2) * 3 + e3) -
                                static_cast<int32_t>(lowest) * 0x28 + MINE_TILE_BASE;
        return drawOverlayShape(lookupTile(tileNum), block->vertices[0], hazePalette);
    }

    /// <summary>The map tile at (<paramref name="row"/>, <paramref name="col"/>), asserting it is on the map.</summary>
    MapTile& mapTileAt(int32_t row, int32_t col)
    {
        const int ok = (row >= 0 && row < GameMap->height && col >= 0 && col < GameMap->width) ? 1 : 0;
        Assert(ok, 0, " Map Tile out of bounds ");
        return GameMap->map[GameMap->width * row + col];
    }

    /// <summary>The elevation level of a tile (bits 7-12 of its cells word).</summary>
    uint32_t tileElevation(uint32_t cells)
    {
        return (cells >> 7) & 0x3f;
    }

    /// <summary>
    void normalizeX87(vector_3d& v)
    {
        const double length = std::sqrt((static_cast<double>(v.x) * v.x + static_cast<double>(v.y) * v.y) +
                                        static_cast<double>(v.z) * v.z);

        if (length > 0.0)
        {
            v.x = static_cast<float>(v.x / length);
            v.y = static_cast<float>(v.y / length);
            v.z = static_cast<float>(v.z / length);
        }
    }

    vector_3d crossX87(const vector_3d& a, const vector_3d& b)
    {
        return vector_3d(static_cast<float>(static_cast<double>(a.y) * b.z - static_cast<double>(a.z) * b.y),
                         static_cast<float>(static_cast<double>(a.z) * b.x - static_cast<double>(a.x) * b.z),
                         static_cast<float>(static_cast<double>(a.x) * b.y - static_cast<double>(a.y) * b.x));
    }

    /// The two edge vectors of the face (triangle) of the tile under <paramref name="pos"/>, from its top-left
    /// corner, and that corner's height. Shared by terrainAngle and terrainNormal (the binary repeats it).
    /// </summary>
    void faceVectors(const vector_3d& pos, vector_3d& edge1, vector_3d& edge2, float& cornerZ)
    {
        const float mpv = Terrain::metersPerVertex;
        const float oneOver = Terrain::OneOvermetersPerVertex;
        const float cornerX = static_cast<float>(mpv * std::floor(static_cast<double>(oneOver) * pos.x));
        const float cornerY = static_cast<float>(mpv * (std::floor(static_cast<double>(oneOver) * pos.y) + 1.0));
        const double gridX = static_cast<double>(oneOver) * cornerX;
        const float gridY = oneOver * cornerY;
        const int32_t half = (Terrain::blocksMapSide * Terrain::verticesBlockSide) >> 1;
        const int32_t col = static_cast<int32_t>(std::floor(gridX)) + half;
        const int32_t row = half - static_cast<int32_t>(std::floor(static_cast<double>(gridY)));

        const uint32_t tileA = mapTileAt(row, col).cells;
        const uint32_t tileB = mapTileAt(row, col + 1).cells;
        const uint32_t tileC = mapTileAt(row + 1, col + 1).cells;
        const uint32_t tileD = mapTileAt(row + 1, col).cells;
        const int32_t base = GameMap->baseElevation;
        const float mpe = Terrain::metersPerElevLevel;
        const auto levelOf = [base](uint32_t cells) -> double
        { return static_cast<double>(static_cast<int64_t>(tileElevation(cells) + base)); };

        const float x0 = static_cast<float>(std::floor(gridX) * mpv);
        const float y0 = static_cast<float>(std::floor(static_cast<double>(gridY)) * mpv);
        cornerZ = static_cast<float>(levelOf(tileA) * mpe);
        const double xPlus = static_cast<double>(x0) + mpv;
        const double offsetX = std::fabs(static_cast<double>(pos.x) - cornerX);
        const double offsetY = std::fabs(static_cast<double>(cornerY) - pos.y);

        // The face is the triangle (A, D, C) nearer the x edge, else (A, B, C).
        if (!(offsetY >= offsetX))
        {
            const float zB = static_cast<float>(levelOf(tileB) * mpe);
            const float y1 = y0 - mpv;
            const float zC = static_cast<float>(levelOf(tileC) * mpe);
            const float dx = static_cast<float>(xPlus - x0);
            edge1 = vector_3d(dx, 0.0f, zB - cornerZ);
            edge2 = vector_3d(dx, y1 - y0, zC - cornerZ);
        }
        else
        {
            const float y1 = y0 - mpv;
            const float zC = static_cast<float>(levelOf(tileC) * mpe);
            const double zD = levelOf(tileD) * mpe;
            const float dy = y1 - y0;
            edge1 = vector_3d(0.0f, dy, static_cast<float>(zD - cornerZ));
            edge2 = vector_3d(static_cast<float>(xPlus - x0), dy, zC - cornerZ);
        }

        normalizeX87(edge1);
        normalizeX87(edge2);
    }
}

auto MapBlockManager::operator new(size_t size) noexcept -> void*
{
    return Terrain::terrainHeap->malloc(static_cast<uint32_t>(size));
}

auto MapBlockManager::operator delete(void* ptr) -> void
{
    Terrain::terrainHeap->free(ptr);
}

auto MapBlockManager::destroy() -> void
{
    if (blockFile != nullptr)
    {
        blockFile->close();
        delete blockFile;
    }

    blockFile = nullptr;
    systemHeap->free(blocks);
    blocks = nullptr;
    HeapManager::destroy();
}

auto MapBlockManager::init(char* fileName, int32_t numBlocks, int32_t blockSize) -> int32_t
{
    FullPathFileName blockName;
    blockName.init(terrainPath, fileName, ".elv");
    blockFile = new PacketFile;

    if (blockFile == nullptr)
    {
        return NO_BLOCK_FILE;
    }

    int32_t result = blockFile->open(blockName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    const uint32_t size = blockFile->fileSize();

    if (size == 0x23318 || size == 0x4f2ec)
    {
        Fatal(-1, " Old Map format.  Resave in Teditor! ");
    }

    if (blocks == nullptr)
    {
        // Every block plus the off-map one.
        const int32_t count = numBlocks + 1;

        if ((result = createHeap(static_cast<uint32_t>(count * blockSize))) != 0)
        {
            return result;
        }

        if ((result = commitHeap(0)) != 0)
        {
            return result;
        }

        uint8_t* heap = getHeapPtr();
        blocks =
            static_cast<PrecompVertex**>(systemHeap->malloc(static_cast<uint32_t>(count) * sizeof(PrecompVertex*)));

        for (int32_t i = 0; i < count; i++)
        {
            const int32_t perBlock = Terrain::verticesBlockSide * Terrain::verticesBlockSide;
            auto* block =
                reinterpret_cast<PrecompVertex*>(heap + static_cast<size_t>(perBlock) * i * sizeof(PrecompVertex));
            blocks[i] = block;
            blockFile->readPacket(i, reinterpret_cast<uint8_t*>(block));
        }
    }

    const uint32_t tableSize = static_cast<uint32_t>(numBlocks) * sizeof(int32_t);
    lastBlock = static_cast<int32_t*>(Terrain::terrainHeap->malloc(tableSize));
    currentBlock = static_cast<int32_t*>(Terrain::terrainHeap->malloc(tableSize));
    blockSteps = static_cast<int32_t*>(Terrain::terrainHeap->malloc(tableSize));
    vertexOffsets =
        static_cast<float*>(Terrain::terrainHeap->malloc(static_cast<uint32_t>(numBlocks * 2 * sizeof(float))));

    for (int32_t i = 0; i < numBlocks; i++)
    {
        lastBlock[i] = -1;
    }

    blockFile->close();
    return 0;
}

auto MapBlockManager::blockPtr(int32_t blockNum) -> PrecompVertex*
{
    const int32_t total = Terrain::blocksMapSide * Terrain::blocksMapSide;

    if (blockNum < 0 || blockNum >= total)
    {
        blockNum = total;
    }

    return blocks[blockNum];
}

auto MapBlockManager::getTopLeftElevation() -> float
{
    if (blockFile == nullptr)
    {
        return 0.0f;
    }

    return static_cast<float>(blocks[0]->elevation) * Terrain::metersPerElevLevel;
}

auto MapBlockManager::update(vector_3d& cameraPos, int32_t windowNum) -> int32_t
{
    if (turn == 1)
    {
        // The off-map block sits one level above the map's base.
        PrecompVertex* offMap = blocks[Terrain::blocksMapSide * Terrain::blocksMapSide];
        const int32_t count = Terrain::verticesBlockSide * Terrain::verticesBlockSide;

        for (int32_t i = 0; i < count; i++)
        {
            offMap[i].elevation = static_cast<uint8_t>(static_cast<uint8_t>(GameMap->baseElevation) + 1);
        }
    }

    const vector_3d& topLeft = Terrain::mapTopLeft3d100;
    const float fromTop = topLeft.y - cameraPos.y;
    const int32_t blockX =
        static_cast<int32_t>(std::floor(std::fabs((topLeft.x - cameraPos.x) / Terrain::metersBlockSide)));
    topLeftBlockX = blockX;
    const int32_t blockY = static_cast<int32_t>(std::floor(std::fabs(fromTop / Terrain::metersBlockSide)));
    topLeftBlockY = blockY;
    const int32_t block = blockY * Terrain::blocksMapSide + blockX;
    currentBlock[windowNum] = block;

    if (block != lastBlock[windowNum])
    {
        lastBlock[windowNum] = block;
    }

    float* offset = &vertexOffsets[windowNum * 2];
    offset[0] = (cameraPos.x - topLeft.x) * Terrain::OneOvermetersPerVertex;
    offset[1] = (topLeft.y - cameraPos.y) * Terrain::OneOvermetersPerVertex;
    const int32_t vertexX = static_cast<int32_t>(std::floor(offset[0]));
    const int32_t vertexY = static_cast<int32_t>(std::floor(offset[1]));
    const int32_t half = Terrain::visibleVerticesPerSide >> 1;
    const int32_t inBlockX = vertexX % Terrain::verticesBlockSide;
    const int32_t inBlockY = vertexY % Terrain::verticesBlockSide;
    const int32_t cornerX = static_cast<int32_t>(std::floor(static_cast<float>(vertexX) - static_cast<float>(half)));
    const int32_t cornerY = static_cast<int32_t>(std::floor(static_cast<float>(vertexY) - static_cast<float>(half)));
    land->getTerrainWindow(windowNum)->topLeftX = static_cast<float>(cornerX) * Terrain::metersPerVertex + topLeft.x;
    land->getTerrainWindow(windowNum)->topLeftY = topLeft.y - static_cast<float>(cornerY) * Terrain::metersPerVertex;

    // The corner's vertex within its block, as seen from the camera's block. The literal 20 and 15 are the
    // original's (a 20-vertex block side).
    int32_t cornerInBlockX = cornerX < 0 ? cornerX + 20 : cornerX % Terrain::verticesBlockSide;
    int32_t cornerInBlockY = cornerY < 0 ? cornerY + 20 : cornerY % Terrain::verticesBlockSide;

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
        cornerInBlockX = cornerX - (blockX - 1) * Terrain::verticesBlockSide;
        cornerInBlockY = cornerY - (blockY - 1) * Terrain::verticesBlockSide;
    }

    offset[0] = static_cast<float>(cornerInBlockX);
    offset[1] = static_cast<float>(cornerInBlockY);
    return 0;
}

auto MapBlockManager::buildWindow(Vertex* vertexList, int32_t* numVertices, TerrainBlock* blockList, int32_t* numBlocks,
                                  int32_t windowNum) -> void
{
    const int32_t side = Terrain::verticesBlockSide;
    float* offset = &vertexOffsets[windowNum * 2];
    int32_t startX = static_cast<int32_t>(std::floor(offset[0]));
    int32_t vertexY = static_cast<int32_t>(std::floor(offset[1]));

    // An offset past the block's side moves the corner into the next block.
    // Original behaviour (OB-102): in the map's last block column that wraps to the next block row.
    if (startX >= side)
    {
        blockSteps[windowNum] += 1;
        offset[0] = offset[0] - static_cast<float>(side);
        currentBlock[windowNum] += 1;
        startX = static_cast<int32_t>(std::floor(offset[0]));
    }

    if (vertexY >= side)
    {
        const int32_t blocksSide = Terrain::blocksMapSide;
        blockSteps[windowNum] += blocksSide;
        offset[1] = offset[1] - static_cast<float>(side);
        currentBlock[windowNum] += blocksSide;
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
    const int32_t blocksSide = Terrain::blocksMapSide;
    const int32_t corner = currentBlock[windowNum];
    topLeftBlockY = corner / blocksSide - 1 - blocksBackY;
    const int32_t firstBlockX = corner % blocksSide - 1 - blocksBackX;
    topLeftBlockX = firstBlockX;
    const int32_t offMap = blocksSide * blocksSide;

    int32_t blockNum = offMap;

    if (topLeftBlockY >= 0 && firstBlockX >= 0)
    {
        blockNum = topLeftBlockY * blocksSide + firstBlockX;
    }

    PrecompVertex* source = blockPtr(blockNum);

    if (source == nullptr)
    {
        Fatal(0, " Terrain System has trashed Memory! ");
    }

    source += Terrain::verticesBlockSide * vertexY + startX;

    Vertex* vertex = vertexList;

    for (int32_t row = 0; row < Terrain::visibleVerticesPerSide; row++)
    {
        int32_t vertexX = startX;

        for (int32_t col = 0; col < Terrain::visibleVerticesPerSide; col++)
        {
            const int32_t blockY = topLeftBlockY;
            vertex->pVertex = source;
            vertex->blockNum = static_cast<int16_t>(static_cast<int16_t>(topLeftBlockY) *
                                                        static_cast<int16_t>(Terrain::blocksMapSide) +
                                                    static_cast<int16_t>(topLeftBlockX));
            vertex->vertexNum = static_cast<int16_t>(static_cast<int16_t>(vertexY) * static_cast<int16_t>(side) +
                                                     static_cast<int16_t>(vertexX));
            vertex->posTile = static_cast<uint32_t>((blockY * side + vertexY) * 0x10000) +
                              (static_cast<uint32_t>(topLeftBlockX * side + vertexX) & 0xffff);
            vertex++;
            (*numVertices)++;
            vertexX++;
            source++;

            if (vertexX == side)
            {
                vertexX = 0;
                topLeftBlockX++;
                blockNum = offMap;

                // Port fix: the grown grid can step across blocks left of the map, which aren't the row before's.
                if (topLeftBlockX >= 0 && topLeftBlockX < Terrain::blocksMapSide)
                {
                    blockNum = blockY * Terrain::blocksMapSide + topLeftBlockX;
                }

                source = blockPtr(blockNum);

                if (source == nullptr)
                {
                    Fatal(0, " Terrain System has trashed Memory! ");
                }

                source += Terrain::verticesBlockSide * vertexY;
            }
        }

        vertexY++;
        topLeftBlockX = firstBlockX;

        if (vertexY == side)
        {
            vertexY = 0;
            topLeftBlockY++;
        }

        blockNum = offMap;

        if (firstBlockX >= 0 && topLeftBlockY >= 0 && firstBlockX <= Terrain::blocksMapSide - 1 &&
            topLeftBlockY <= Terrain::blocksMapSide - 1)
        {
            blockNum = topLeftBlockY * Terrain::blocksMapSide + firstBlockX;
        }

        source = blockPtr(blockNum);

        if (source == nullptr)
        {
            Fatal(0, " MapBlock Not Cached ");
        }

        source += Terrain::verticesBlockSide * vertexY + startX;
    }

    // One block per grid square: corners top-left, top-right, bottom-right, bottom-left.
    const int32_t perSide = Terrain::visibleVerticesPerSide;

    *numBlocks = 0;
    for (int32_t row = 0; row < perSide - 1; row++)
    {
        for (int32_t col = 0; col < perSide - 1; col++)
        {
            Vertex* topLeft = vertexList + row * perSide + col;
            blockList->init(topLeft, topLeft + 1, topLeft + perSide + 1, topLeft + perSide);
            blockList++;
            (*numBlocks)++;
        }
    }
}

auto MapBlockManager::setOverlayTile(int32_t blockNum, int32_t vertexNum, int32_t value) -> void
{
    blocks[blockNum][vertexNum].overlayData = static_cast<int16_t>(blocks[blockNum][vertexNum].overlayData + value);
}

auto MapBlockManager::getOverlayTile(int32_t blockNum, int32_t vertexNum) -> int32_t
{
    return blocks[blockNum][vertexNum].overlayData;
}

auto MapBlockManager::setTile(int32_t blockNum, int32_t vertexNum, int32_t value) -> void
{
    blocks[blockNum][vertexNum].textureData = static_cast<int16_t>(blocks[blockNum][vertexNum].textureData + value);
}

auto MapBlockManager::getTile(int32_t blockNum, int32_t vertexNum) -> int32_t
{
    return blocks[blockNum][vertexNum].textureData;
}

auto MapBlockManager::markSeen(vector_2d& /*topLeft*/, Vertex* /*vertexList*/, vector_3d& /*looker*/,
                               vector_3d& /*lookVector*/, float /*angle*/, float /*range*/, uint8_t /*who*/) -> void
{
}

auto MapBlockManager::terrainAngle(vector_3d& pos, vector_3d* normal) -> float
{
    vector_3d edge1;
    vector_3d edge2;
    float cornerZ = 0.0f;
    faceVectors(pos, edge1, edge2, cornerZ);
    vector_3d faceNormal = crossX87(edge1, edge2);

    float angle = 0.0f;

    if (faceNormal.z == 0.0f || std::isnan(faceNormal.z))
    {
        // Faithful: a vertical face returns the corner's height, not an angle.
        angle = cornerZ;
    }
    else
    {
        normalizeX87(faceNormal);
        angle = static_cast<float>(acosMatherr(static_cast<double>(faceNormal.z)) * RADS_TO_DEGREES);
    }

    if (normal != nullptr)
    {
        *normal = faceNormal;
    }

    return angle;
}

auto MapBlockManager::terrainNormal(vector_3d& pos) -> vector_3d
{
    vector_3d edge1;
    vector_3d edge2;
    float cornerZ = 0.0f;
    faceVectors(pos, edge1, edge2, cornerZ);
    vector_3d faceNormal = crossX87(edge2, edge1);

    if (!(faceNormal.z >= 0.0f))
    {
        faceNormal = crossX87(edge1, edge2);
    }

    normalizeX87(faceNormal);
    return faceNormal;
}

auto MapBlockManager::terrainElevation(vector_3d& pos) -> float
{
    return terrainElevationAt(pos);
}

auto terrainElevationAt(vector_3d& pos) -> float
{
    using ext = double;
    const float mpv = Terrain::metersPerVertex;
    const float oneOver = Terrain::OneOvermetersPerVertex;
    const float cornerX = static_cast<float>(mpv * std::floor(static_cast<double>(oneOver) * pos.x));
    const float cornerY = static_cast<float>(mpv * (std::floor(static_cast<double>(oneOver) * pos.y) + 1.0));
    const double gridX = static_cast<double>(oneOver) * cornerX;
    const double gridY = static_cast<double>(static_cast<float>(oneOver * cornerY));
    const int32_t half = (Terrain::blocksMapSide * Terrain::verticesBlockSide) >> 1;
    const int32_t col = static_cast<int32_t>(std::floor(gridX)) + half;
    const int32_t row = half - static_cast<int32_t>(std::floor(gridY));

    if (row < 0 || row >= GameMap->height || col < 0 || col >= GameMap->width)
    {
        return 0.0f;
    }

    if (row + 1 < 0 || row + 1 >= GameMap->height || col + 1 < 0 || col + 1 >= GameMap->width)
    {
        return 0.0f;
    }

    const MapTile& tileA = mapTileAt(row, col);
    const uint32_t cellsA = tileA.cells;
    const uint32_t overlayA = tileA.overlay;
    const uint32_t cellsB = mapTileAt(row, col + 1).cells;
    const uint32_t cellsC = mapTileAt(row + 1, col + 1).cells;
    const uint32_t cellsD = mapTileAt(row + 1, col).cells;

    if (isFlatTileType(overlayA & 0x7f))
    {
        uint32_t level = tileElevation(cellsA);
        const uint32_t levelB = tileElevation(cellsB);
        const uint32_t levelC = tileElevation(cellsC);
        const uint32_t levelD = tileElevation(cellsD);

        if (levelB == level && levelB == levelC && levelC == levelD)
        {
            level++;
        }
        else
        {
            level = std::max({level, levelB, levelC, levelD});
        }

        return static_cast<float>(static_cast<ext>(static_cast<int32_t>(GameMap->baseElevation + level)) *
                                  Terrain::metersPerElevLevel);
    }

    const int32_t base = GameMap->baseElevation;
    const float mpe = Terrain::metersPerElevLevel;
    const auto levelOf = [base](uint32_t cells)
    { return static_cast<int64_t>(static_cast<uint32_t>(static_cast<int32_t>(tileElevation(cells)) + base)); };
    const float x0 = static_cast<float>(std::floor(gridX) * mpv);
    const float y0 = static_cast<float>(std::floor(gridY) * mpv);
    const ext elevA = static_cast<ext>(levelOf(cellsA)) * mpe;
    const float offsetX = std::fabs(pos.x - cornerX);
    const float offsetY = std::fabs(cornerY - pos.y);
    const ext offsetXExt = std::fabs(static_cast<ext>(pos.x) - cornerX);

    ext uX;
    ext uY;
    ext vX;
    ext vY;
    ext vZ;
    float uZ;

    if (offsetXExt > static_cast<ext>(offsetY))
    {
        const float elevB = static_cast<float>(static_cast<ext>(levelOf(cellsB)) * mpe);
        const float y1 = static_cast<float>(y0 - mpv);
        const float elevC = static_cast<float>(static_cast<ext>(levelOf(cellsC)) * mpe);
        const ext spanX = (static_cast<ext>(x0) + mpv) - x0;
        uZ = static_cast<float>(static_cast<ext>(elevB) - elevA);
        uX = spanX;
        uY = 0.0;
        vX = static_cast<float>(spanX);
        vY = static_cast<ext>(y1) - y0;
        vZ = static_cast<ext>(elevC) - elevA;
    }
    else
    {
        const float x1 = static_cast<float>(static_cast<ext>(x0) + mpv);
        const float y1 = static_cast<float>(y0 - mpv);
        const float elevC = static_cast<float>(static_cast<ext>(levelOf(cellsC)) * mpe);
        const ext elevD = static_cast<ext>(levelOf(cellsD)) * mpe;
        const float spanY = static_cast<float>(static_cast<ext>(y1) - y0);
        uZ = static_cast<float>(elevD - elevA);
        uX = 0.0;
        uY = spanY;
        vX = static_cast<ext>(x1) - x0;
        vY = spanY;
        vZ = static_cast<ext>(elevC) - elevA;
    }

    const float uYf = static_cast<float>(uY);
    const ext lengthU = std::sqrt((static_cast<ext>(uYf) * uYf + static_cast<ext>(uZ) * uZ) + uX * uX);

    if (lengthU != 0.0)
    {
        uX = uX / lengthU;
        uY = uY / lengthU;
        uZ = static_cast<float>(static_cast<ext>(uZ) / lengthU);
    }

    const float vZf = static_cast<float>(vZ);
    const float vYf = static_cast<float>(vY);
    const ext lengthV = std::sqrt((vX * vX + static_cast<ext>(vZf) * vZf) + static_cast<ext>(vYf) * vYf);

    if (lengthV != 0.0)
    {
        vX = vX / lengthV;
        vY = vY / lengthV;
        vZ = vZ / lengthV;
    }

    // The plane through the corner: z = z0 - (nx/nz) * dx + (ny/nz) * dy.
    const float normalX = static_cast<float>(vZ * uY - vY * static_cast<ext>(uZ));
    const float normalY = static_cast<float>(static_cast<ext>(uZ) * vX - vZ * uX);
    const float normalZ = static_cast<float>(vY * uX - uY * vX);

    if (normalZ == 0.0f)
    {
        return static_cast<float>(elevA);
    }

    ext nx = normalX;
    ext ny = normalY;
    ext nz = normalZ;

    if (normalZ < 0.0f)
    {
        nx = -nx;
        ny = -ny;
        nz = -nz;
    }

    return static_cast<float>(elevA +
                              -((ny / nz) * static_cast<ext>(-offsetY) + (nx / nz) * static_cast<ext>(offsetX)));
}

auto MapBlockManager::generateRandomBlock(PrecompVertex* block) -> void
{
    const int32_t count = Terrain::verticesBlockSide * Terrain::verticesBlockSide;

    for (int32_t i = 0; i < count; i++)
    {
        block[i].elevation = static_cast<uint8_t>(GameMap->baseElevation);
        block[i].unknown01 = 0;
        block[i].textureData = 0x29;
        block[i].overlayData = 0x29;
    }
}

auto VertexManager::operator new(size_t size) noexcept -> void*
{
    return Terrain::terrainHeap->malloc(static_cast<uint32_t>(size));
}

auto VertexManager::operator delete(void* ptr) -> void
{
    Terrain::terrainHeap->free(ptr);
}

auto TerrainTileManager::operator new(size_t size) noexcept -> void*
{
    return Terrain::terrainHeap->malloc(static_cast<uint32_t>(size));
}

auto TerrainTileManager::operator delete(void* ptr) -> void
{
    Terrain::terrainHeap->free(ptr);
}

auto TerrainBlock::init(Vertex* v0, Vertex* v1, Vertex* v2, Vertex* v3) -> int32_t
{
    vertices[0] = v0;
    vertices[1] = v1;
    vertices[2] = v2;
    vertices[3] = v3;
    return 0;
}

auto TerrainBlock::draw(int32_t hazeFactor, uint8_t /*flags*/) -> void
{
    Vertex* topLeft = vertices[0];
    uint32_t redrawCount = 0;
    uint32_t clippedCount = 0;

    for (Vertex* vertex : vertices)
    {
        redrawCount += vertex->redraw;
        clippedCount += vertex->clipped;
    }

    if (clippedCount == 4)
    {
        return;
    }

    uint8_t* hazePalette = nullptr;
    uint32_t visibleCount = 0;

    if (hazeFactor != 0x7fff)
    {
        visibleCount = countVisibleCorners(this);

        if (visibleCount != lastVisibleCount)
        {
            redrawCount++;

            for (Vertex* vertex : vertices)
            {
                vertex->edgeRedraw = 1;
            }
        }

        if (visibleCount != 4)
        {
            hazePalette = hazePaletteFor(hazeFactor, visibleCount);
        }
    }

    // Unseen blocks (and the 0x7fff "all black" factor) are filled black.
    if (hazeFactor == 0x7fff || visibleCount == 0)
    {
        hazePalette = VFX_TILE_FILL;
    }

    const int32_t textureData = topLeft->pVertex->textureData;
    TerrainTile* terrainTile = nullptr;

    if (textureData < 0)
    {
        tileCacheReqs++;
    }
    else
    {
        terrainTile = lookupTile(textureData);
    }

    tile = terrainTile;
    numTerrainFaces++;

    if (terrainTile != nullptr && terrainTile->tileData != nullptr && redrawCount != 0)
    {
        VFX_nTile_draw(globalPane, terrainTile->tileData, topLeft->px, topLeft->py, hazePalette);
    }
}

auto TerrainBlock::drawOverlay(int32_t hazeFactor, uint8_t /*flags*/) -> void
{
    Vertex* topLeft = vertices[0];
    uint32_t clippedCount = 0;

    for (Vertex* vertex : vertices)
    {
        clippedCount += vertex->clipped;
    }

    if (clippedCount == 4)
    {
        return;
    }

    if (hazeFactor == 0x7fff)
    {
        return;
    }

    const uint32_t visibleCount = countVisibleCorners(this);

    if (visibleCount != lastVisibleCount)
    {
        lastVisibleCount = static_cast<uint8_t>(visibleCount);
    }

    uint8_t* hazePalette = nullptr;

    if (visibleCount != 4)
    {
        hazePalette = hazePaletteFor(hazeFactor, visibleCount);
    }

    if (visibleCount == 0)
    {
        return;
    }

    numTerrainFaces++;
    const int32_t overlayData = topLeft->pVertex->overlayData;
    TerrainTile* overlay = nullptr;

    if (overlayData < 0)
    {
        tileCacheReqs++;
    }
    else
    {
        overlay = lookupTile(overlayData);
    }

    overlayTile = overlay;

    if (!drawOverlayShape(overlay, topLeft, hazePalette))
    {
        return;
    }

    // Mines: the map tile's overlay word holds the Inner Sphere's (bits 11-12) and the Clans' (bits 13-14) mine
    // state; 2 = a known mine, 3 = an exploded one.
    const int32_t col = (topLeft->blockNum % Terrain::blocksMapSide) * Terrain::verticesBlockSide +
                        topLeft->vertexNum % Terrain::verticesBlockSide;
    const int32_t row = (topLeft->blockNum / Terrain::blocksMapSide) * Terrain::verticesBlockSide +
                        topLeft->vertexNum / Terrain::verticesBlockSide;
    const int ok = (row >= 0 && row < GameMap->height && col >= 0 && col < GameMap->width) ? 1 : 0;
    Assert(ok, 0, " bldng MapTile Out of Bounds ");
    const uint32_t mineWord = GameMap->map[GameMap->width * row + col].overlay;
    const uint32_t clanMine = (mineWord >> 13) & 3;
    const uint32_t innerSphereMine = (mineWord >> 11) & 3;

    const bool playingInnerSphere = homeTeam == innerSphereTeam;

    if (playingInnerSphere && innerSphereMine == 2)
    {
        if (!drawMineTile(this, hazePalette))
        {
            return;
        }
    }

    // The Clans' mines show when playing Clan, or in god mode.
    const bool showClanMines = !playingInnerSphere || scenario->godMode != 0;

    if (showClanMines && clanMine == 2)
    {
        if (!drawMineTile(this, hazePalette))
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

    TerrainTile* crater = lookupTile(MINE_EXPLODED_TILE);
    drawOverlayShape(crater, topLeft, hazePalette);
}

auto TerrainBlock::drawLine(int32_t color, int /*onlyTop*/) -> void
{
    uint32_t clippedCount = 0;

    for (Vertex* vertex : vertices)
    {
        clippedCount += vertex->clipped;
    }

    if (clippedCount == 4)
    {
        return;
    }

    // Every edge is drawn at the top-left vertex's depth.
    const int32_t depth = vertices[0]->py;

    for (int32_t edge = 0; edge < 4; edge++)
    {
        Vertex* from = vertices[edge];
        Vertex* to = vertices[(edge + 1) & 3];
        vector_2d start(static_cast<float>(from->px), static_cast<float>(from->py));
        vector_2d end(static_cast<float>(to->px), static_cast<float>(to->py));
        ElementList->add(new LineElement(start, end, color, nullptr, depth, -1));
    }
}

auto TerrainBlock::drawHaze(int32_t /*hazeFactor*/, uint8_t /*flags*/) -> void
{
}

// Port: the ground mesh -------------------------------------------------------------------------------------------

namespace
{
    /// <summary>
    /// The map's ground mesh (MCTerrainMesh) and what it was made with: the steps it places vertices by, the tile set,
    /// and per mesh vertex the elevation and tile it holds (to check each frame's grid against).
    /// </summary>
    struct GroundMesh
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

    std::unique_ptr<GroundMesh> groundMesh;
    uint64_t groundMeshVersions = 0;

    /// <summary>The map vertex at (<paramref name="row"/>, <paramref name="col"/>) as buildWindow finds it: in its
    /// block, or in the off-map block.</summary>
    const PrecompVertex* mapVertexAt(int32_t row, int32_t col)
    {
        const int32_t side = Terrain::verticesBlockSide;
        const int32_t mapSide = Terrain::blocksMapSide * side;
        int32_t blockNum = Terrain::blocksMapSide * Terrain::blocksMapSide;

        if (row >= 0 && col >= 0 && row < mapSide && col < mapSide)
        {
            blockNum = (row / side) * Terrain::blocksMapSide + col / side;
        }

        const int32_t vertexY = ((row % side) + side) % side;
        const int32_t vertexX = ((col % side) + side) % side;
        return Terrain::mapBlockManager->blockPtr(blockNum) + vertexY * side + vertexX;
    }

    /// <summary>
    /// Builds the ground mesh over the map and a ring of <paramref name="ring"/> off-map vertices around it, reading
    /// every tile it uses.
    /// </summary>
    void buildGroundMesh(int32_t ring, int32_t stepX, int32_t stepY, int32_t elevStep, uint32_t tileSet)
    {
        auto built = std::make_unique<GroundMesh>();
        built->StepX = stepX;
        built->StepY = stepY;
        built->ElevStep = elevStep;
        built->TileSet = tileSet;
        const int32_t mapSide = Terrain::blocksMapSide * Terrain::verticesBlockSide;
        const int32_t side = mapSide + 2 * ring;
        built->Side = side;
        built->Elevations.resize(static_cast<size_t>(side) * side);
        built->Textures.resize(static_cast<size_t>(side) * side);

        for (int32_t r = 0; r < side; ++r)
        {
            for (int32_t c = 0; c < side; ++c)
            {
                const PrecompVertex* vertex = mapVertexAt(r - ring, c - ring);
                built->Elevations[static_cast<size_t>(r) * side + c] = vertex->elevation;
                built->Textures[static_cast<size_t>(r) * side + c] = vertex->textureData;
            }
        }

        MCTerrainMesh& mesh = built->Mesh;
        mesh.Version = ++groundMeshVersions;
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
                        const TerrainTile* tile = lookupTile(textureData);

                        if (tile != nullptr && tile->tileData != nullptr)
                        {
                            const uint8_t* data = tile->tileData;
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

        groundMesh = std::move(built);
    }

    /// <summary>Whether the mesh's vertices take in the grid's, from (<paramref name="row"/>, <paramref name="col"/>).</summary>
    bool meshCovers(int32_t row, int32_t col, int32_t perSide)
    {
        const int32_t first = groundMesh->Mesh.FirstRow;
        return row >= first && col >= first && row + perSide <= first + groundMesh->Side &&
               col + perSide <= first + groundMesh->Side;
    }
}

std::expected<void, std::string> MCTerrainGroundFrame(const Vertex* vertexList, int32_t numVertices, int32_t numBlocks,
                                                      int32_t hazeFactor, int32_t stepX, int32_t stepY,
                                                      int32_t elevStep, int32_t minX, int32_t maxX, int32_t minY,
                                                      int32_t maxY, MCTerrainFrame& frame)
{
    const int32_t perSide = Terrain::visibleVerticesPerSide;

    if (useOldProject != 0 || projectAll != 0 || useNonIntegerAdditive != 0)
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

    ByteFlag* fog = homeTeam != nullptr ? homeVisibleBits() : nullptr;

    if (fog == nullptr || fog->flagWindow == nullptr)
    {
        return std::unexpected("the ground mesh has no fog of war flags to read");
    }

    if (Terrain::mapBlockManager == nullptr || terrainTiles == nullptr || globalPane == nullptr)
    {
        return std::unexpected("the ground mesh is asked for without a terrain");
    }

    const uint32_t tileSet = eye->cameraScale == 1 ? 1 : 0;

    // The grid's corner, from its first vertex's map position.
    const uint32_t firstTile = vertexList[0].posTile;
    const int32_t firstRow = static_cast<int32_t>(firstTile) >> 16;
    const int32_t firstCol = static_cast<int16_t>(firstTile & 0xffff);

    if (groundMesh == nullptr || groundMesh->StepX != stepX || groundMesh->StepY != stepY ||
        groundMesh->ElevStep != elevStep || groundMesh->TileSet != tileSet || !meshCovers(firstRow, firstCol, perSide))
    {
        buildGroundMesh(perSide + 2, stepX, stepY, elevStep, tileSet);

        if (!meshCovers(firstRow, firstCol, perSide))
        {
            return std::unexpected(std::format(
                "the ground mesh doesn't cover the terrain grid from map vertex ({}, {})", firstRow, firstCol));
        }
    }

    // Each grid vertex must be the map's vertex at its place, projected where the mesh puts it. The mesh is built again
    // when the map's own data changed since (setTile).
    const int32_t originX = vertexList[0].px - (firstCol - firstRow) * stepX;
    const int32_t originY = vertexList[0].py - (firstRow + firstCol) * stepY +
                            static_cast<int32_t>(vertexList[0].pVertex->elevation) * elevStep;

    for (bool rebuilt = false;;)
    {
        bool current = true;

        for (int32_t i = 0; i < numVertices; ++i)
        {
            const Vertex& vertex = vertexList[i];
            const int32_t row = firstRow + i / perSide;
            const int32_t col = firstCol + i % perSide;
            const uint32_t elevation = vertex.pVertex->elevation;

            if (vertex.posTile != static_cast<uint32_t>(row * 0x10000) + (static_cast<uint32_t>(col) & 0xffff))
            {
                return std::unexpected(std::format("terrain grid vertex {} isn't map vertex ({}, {}) (posTile {:08x})",
                                                   i, row, col, vertex.posTile));
            }

            // OB-102's wrapped grid lands here.
            if (vertex.pVertex != mapVertexAt(row, col))
            {
                return std::unexpected(std::format(
                    "terrain grid vertex {} at map vertex ({}, {}) holds another vertex's data", i, row, col));
            }

            if (vertex.redraw == 0)
            {
                return std::unexpected(std::format("terrain grid vertex {} isn't redrawn this frame", i));
            }

            const int32_t x = originX + (col - row) * stepX;
            const int32_t y = originY + (row + col) * stepY - static_cast<int32_t>(elevation) * elevStep;

            if (vertex.px != x || vertex.py != y)
            {
                return std::unexpected(std::format("terrain grid vertex {} projects to ({}, {}); the mesh puts it at "
                                                   "({}, {})",
                                                   i, vertex.px, vertex.py, x, y));
            }

            const size_t at = static_cast<size_t>(row - groundMesh->Mesh.FirstRow) * groundMesh->Side +
                              static_cast<size_t>(col - groundMesh->Mesh.FirstCol);
            current = current && groundMesh->Elevations[at] == elevation &&
                      groundMesh->Textures[at] == vertex.pVertex->textureData;
        }

        if (current)
        {
            break;
        }

        if (rebuilt)
        {
            return std::unexpected("the ground mesh doesn't hold the map's data even when built again");
        }

        buildGroundMesh(perSide + 2, stepX, stepY, elevStep, tileSet);
        rebuilt = true;
    }

    frame.Mesh = &groundMesh->Mesh;
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
    const _window* window = globalPane->window;
    int32_t x0 = std::max(globalPane->x0, 0);
    int32_t y0 = std::max(globalPane->y0, 0);
    int32_t x1 = globalPane->x1 < window->x_max + 1 ? globalPane->x1 : window->x_max;
    int32_t y1 = globalPane->y1 < window->y_max + 1 ? globalPane->y1 : window->y_max;
    MCClipToView(window, x0, y0, x1, y1);
    frame.PaneX = globalPane->x0;
    frame.PaneY = globalPane->y0;
    frame.Clip = MCRect{x0, y0, x1, y1};
    frame.Fog = fog->flagWindow;
    frame.AllFilled = hazeFactor == 0x7fff;

    if (!frame.AllFilled)
    {
        for (uint32_t seen = 1; seen <= 3; ++seen)
        {
            frame.Haze[seen - 1] = hazePaletteFor(hazeFactor, seen);
        }
    }

    return {};
}

void MCTerrainForgetMesh()
{
    groundMesh.reset();
}
