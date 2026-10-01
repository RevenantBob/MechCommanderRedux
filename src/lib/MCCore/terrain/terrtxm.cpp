#include "stdafx.h"
#include "terrain/terrtxm.h"
#include "lib/cident.h"
#include "lib/heap.h"
#include "lib/packet.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "terrain/vertex.h"

char tilePath[80] = "data\\tiles\\";
char tile90Path[80] = "data\\tiles\\";

namespace
{
    /// <summary>The heap could not be created or sized.</summary>
    constexpr int32_t TILE_NO_HEAP = static_cast<int32_t>(0xbaaa0003);
    /// <summary>A tile file could not be created.</summary>
    constexpr int32_t TILE_NO_FILE = static_cast<int32_t>(0xbaaa0002);
    /// <summary>The slot table didn't fit in the heap.</summary>
    constexpr int32_t TILE_NO_TABLE = static_cast<int32_t>(0xbaaa0001);
}

auto TerrainTiles::init(char* tileFileName, int32_t heapSize) -> int32_t
{
    tileCacheReqs = 0;
    tileCacheHits = 0;
    tileCacheMiss = 0;
    FullPathFileName tileName;
    FullPathFileName tile90Name;

    UserHeap* heap = new UserHeap;
    tileHeap = heap;

    if (heap == nullptr)
    {
        return TILE_NO_HEAP;
    }

    int32_t result = heap->init(static_cast<uint32_t>(heapSize), nullptr);

    if (result != 0)
    {
        return result;
    }

    tileFile = new PacketFile;

    if (tileFile == nullptr)
    {
        return TILE_NO_FILE;
    }

    tileName.init(tilePath, tileFileName, ".pak");
    result = tileFile->open(tileName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    const int32_t packets = tileFile->getNumPackets();
    numTiles = packets;

    tile90File = new PacketFile;

    if (tile90File == nullptr)
    {
        return TILE_NO_FILE;
    }

    tile90Name.init(tile90Path, tileFileName, "90.pak");
    // The rotated set is optional: the result isn't checked.
    tile90File->open(tile90Name, READ, 50);
    tileSetOffset[0] = 0;
    tileSetOffset[1] = packets / 2;

    tiles = static_cast<TerrainTile*>(heap->malloc(static_cast<uint32_t>(packets) * sizeof(TerrainTile)));

    if (tiles == nullptr)
    {
        return TILE_NO_TABLE;
    }

    std::memset(tiles, 0, static_cast<size_t>(packets) * sizeof(TerrainTile));
    tileHeap->unknown2C = 0;
    customTileSet = MCPort::StrICmp("tiles", tileFileName) != 0 ? 1 : 0;
    return 0;
}

auto TerrainTiles::preload(char* terrainName) -> int32_t
{
    FullPathFileName preloadName;
    preloadName.init(terrainPath, terrainName, ".pre");
    File preloadFile;
    int32_t result = preloadFile.open(preloadName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    const uint32_t size = preloadFile.fileSize();
    // Preloaded tiles are stamped turn -1, which dumpLRU never flushes.
    const int32_t savedTurn = turn;
    turn = -1;

    for (uint32_t count = size >> 2; count != 0; count--)
    {
        const int32_t tileNum = preloadFile.readLong();
        readTile((numTiles >> 1) + tileNum);
    }

    turn = savedTurn;
    return 0;
}

auto TerrainTiles::destroy() -> void
{
    if (tileFile != nullptr)
    {
        tileFile->close();
        delete tileFile;
    }

    tileFile = nullptr;

    if (tile90File != nullptr)
    {
        tile90File->close();
        delete tile90File;
    }

    tile90File = nullptr;
    delete tileHeap;
    tileHeap = nullptr;
}

auto TerrainTiles::dumpLRU(int32_t /*bytesNeeded*/) -> void
{
    UserHeap* heap = tileHeap;
    heap->coreLeft();
    TerrainTile* tile = tiles;

    for (int32_t count = numTiles; count > 0; count--, tile++)
    {
        if (tile->tileData == TerrainTile::TILE_MISSING || tile->tileData == nullptr)
        {
            continue;
        }

        const int32_t lastUsed = tile->lastTurnUsed;

        if (lastUsed >= 0 && turn != lastUsed && turn - lastUsed >= 0)
        {
            heap->free(tile->tileData);
            tile->tileData = nullptr;
        }
    }
}

auto TerrainTiles::readTile(int32_t tileNum) -> TerrainTile*
{
    // Faithful: tileNum == numTiles passes this test (one past the table).
    if (tileNum > numTiles)
    {
        return nullptr;
    }

    // The lower half of the table is the rotated set.
    PacketFile* file = tileNum < (numTiles >> 1) ? tile90File : tileFile;
    TerrainTile* tile = &tiles[tileNum];

    if (file->seekPacket(tileNum) != 0)
    {
        tile->tileData = TerrainTile::TILE_MISSING;
    }

    const int32_t size = file->getPacketSize();

    if (size == 0)
    {
        tile->tileData = TerrainTile::TILE_MISSING;
        return nullptr;
    }

    auto* data = static_cast<uint8_t*>(tileHeap->malloc(static_cast<uint32_t>(size)));

    if (data == nullptr)
    {
        dumpLRU(size);
        data = static_cast<uint8_t*>(tileHeap->malloc(static_cast<uint32_t>(size)));

        if (data == nullptr)
        {
            return nullptr;
        }
    }

    file->readPacket(tileNum, data);
    tile->tileData = data;
    tile->lastTurnUsed = turn;
    return tile->tileData == TerrainTile::TILE_MISSING ? nullptr : tile;
}
