#include "stdafx.h"
#include "terrain/terrtxm.h"
#include "lib/cident.h"
#include "lib/packet.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "terrain/vertex.h"
#include "platform/MCRenderer.h"

char TilePath[80] = "data\\tiles\\";
char Tile90Path[80] = "data\\tiles\\";

namespace
{
    /// <summary>A tile file could not be created.</summary>
    constexpr int32_t TILE_NO_FILE = static_cast<int32_t>(0xbaaa0002);
}

auto MCTerrainTiles::Init(char* tileFileName) -> int32_t
{
    TileCacheReqs = 0;
    TileCacheHits = 0;
    TileCacheMiss = 0;
    MCFullPathFileName tileName;
    MCFullPathFileName tile90Name;

    TileFile = new MCPacketFile;

    if (TileFile == nullptr)
    {
        return TILE_NO_FILE;
    }

    tileName.Init(TilePath, tileFileName, ".pak");
    const int32_t result = TileFile->Open(tileName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    const int32_t packets = TileFile->GetNumPackets();
    NumTiles = packets;

    Tile90File = new MCPacketFile;

    if (Tile90File == nullptr)
    {
        return TILE_NO_FILE;
    }

    tile90Name.Init(Tile90Path, tileFileName, "90.pak");
    // The rotated set is optional: the result isn't checked.
    Tile90File->Open(tile90Name, READ, 50);
    TileSetOffset[0] = 0;
    TileSetOffset[1] = packets / 2;

    // One slot more than the packets: readTile takes tileNum == numTiles (the original wrote past the table).
    Tiles.clear();
    Tiles.resize(static_cast<size_t>(std::max(packets, 0)) + 1);
    CustomTileSet = MCPort::StrICmp("tiles", tileFileName) != 0 ? 1 : 0;
    return 0;
}

auto MCTerrainTiles::Preload(char* terrainName) -> int32_t
{
    MCFullPathFileName preloadName;
    preloadName.Init(TerrainPath, terrainName, ".pre");
    MCFile preloadFile;
    int32_t result = preloadFile.Open(preloadName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    const uint32_t size = preloadFile.FileSize();
    // Preloaded tiles are stamped turn -1, which dumpLRU never flushes.
    const int32_t savedTurn = Turn;
    Turn = -1;

    for (uint32_t count = size >> 2; count != 0; count--)
    {
        const int32_t tileNum = preloadFile.ReadLong();
        ReadTile((NumTiles >> 1) + tileNum);
    }

    Turn = savedTurn;
    return 0;
}

auto MCTerrainTiles::Destroy() -> void
{
    if (TileFile != nullptr)
    {
        TileFile->Close();
        delete TileFile;
    }

    TileFile = nullptr;

    if (Tile90File != nullptr)
    {
        Tile90File->Close();
        delete Tile90File;
    }

    Tile90File = nullptr;

    for (MCTerrainTile& tile : Tiles)
    {
        tile.Free();
    }

    Tiles.clear();
}

auto MCTerrainTile::Free() -> void
{
    if (Storage != nullptr)
    {
        MCRenderer::UnregisterData(Storage.get());
        Storage.reset();
    }

    TileData = nullptr;
}

auto MCTerrainTiles::DumpLru(int32_t /*bytesNeeded*/) -> void
{
    MCTerrainTile* tile = Tiles.data();

    for (int32_t count = NumTiles; count > 0; count--, tile++)
    {
        if (tile->TileData == MCTerrainTile::TILE_MISSING || tile->TileData == nullptr)
        {
            continue;
        }

        const int32_t lastUsed = tile->LastTurnUsed;

        if (lastUsed >= 0 && Turn != lastUsed && Turn - lastUsed >= 0)
        {
            tile->Free();
        }
    }
}

auto MCTerrainTiles::ReadTile(int32_t tileNum) -> MCTerrainTile*
{
    // Faithful: tileNum == numTiles passes this test (one past the table).
    if (tileNum > NumTiles)
    {
        return nullptr;
    }

    // The lower half of the table is the rotated set.
    MCPacketFile* file = tileNum < (NumTiles >> 1) ? Tile90File : TileFile;
    MCTerrainTile* tile = &Tiles[tileNum];

    if (file->SeekPacket(tileNum) != 0)
    {
        tile->TileData = MCTerrainTile::TILE_MISSING;
    }

    const int32_t size = file->GetPacketSize();

    if (size == 0)
    {
        tile->TileData = MCTerrainTile::TILE_MISSING;
        return nullptr;
    }

    // The original flushed the cache (dumpLRU) when its heap was full; the port's never is.
    tile->Free();
    tile->Storage = std::make_unique<uint8_t[]>(static_cast<size_t>(size));
    file->ReadPacket(tileNum, tile->Storage.get());
    MCRenderer::RegisterData(tile->Storage.get(), static_cast<size_t>(size), MCDataKind::Shapes);
    tile->TileData = tile->Storage.get();
    tile->LastTurnUsed = Turn;
    return tile->TileData == MCTerrainTile::TILE_MISSING ? nullptr : tile;
}
