#include "stdafx.h"
#include "terrain/terrtxm.h"
#include "lib/cident.h"
#include "lib/packet.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "terrain/vertex.h"
#include "platform/MCRenderer.h"

char tilePath[80] = "data\\tiles\\";
char tile90Path[80] = "data\\tiles\\";

namespace
{
    /// <summary>A tile file could not be created.</summary>
    constexpr int32_t TILE_NO_FILE = static_cast<int32_t>(0xbaaa0002);
}

auto TerrainTiles::init(char* tileFileName) -> int32_t
{
    tileCacheReqs = 0;
    tileCacheHits = 0;
    tileCacheMiss = 0;
    FullPathFileName tileName;
    FullPathFileName tile90Name;

    tileFile = new PacketFile;

    if (tileFile == nullptr)
    {
        return TILE_NO_FILE;
    }

    tileName.init(tilePath, tileFileName, ".pak");
    const int32_t result = tileFile->open(tileName, READ, 50);

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

    // One slot more than the packets: readTile takes tileNum == numTiles (the original wrote past the table).
    tiles.clear();
    tiles.resize(static_cast<size_t>(std::max(packets, 0)) + 1);
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

    for (TerrainTile& tile : tiles)
    {
        tile.free();
    }

    tiles.clear();
}

auto TerrainTile::free() -> void
{
    if (storage != nullptr)
    {
        MCRenderer::UnregisterData(storage.get());
        storage.reset();
    }

    tileData = nullptr;
}

auto TerrainTiles::dumpLRU(int32_t /*bytesNeeded*/) -> void
{
    TerrainTile* tile = tiles.data();

    for (int32_t count = numTiles; count > 0; count--, tile++)
    {
        if (tile->tileData == TerrainTile::TILE_MISSING || tile->tileData == nullptr)
        {
            continue;
        }

        const int32_t lastUsed = tile->lastTurnUsed;

        if (lastUsed >= 0 && turn != lastUsed && turn - lastUsed >= 0)
        {
            tile->free();
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

    // The original flushed the cache (dumpLRU) when its heap was full; the port's never is.
    tile->free();
    tile->storage = std::make_unique<uint8_t[]>(static_cast<size_t>(size));
    file->readPacket(tileNum, tile->storage.get());
    MCRenderer::RegisterData(tile->storage.get(), static_cast<size_t>(size), MCDataKind::Shapes);
    tile->tileData = tile->storage.get();
    tile->lastTurnUsed = turn;
    return tile->tileData == TerrainTile::TILE_MISSING ? nullptr : tile;
}
