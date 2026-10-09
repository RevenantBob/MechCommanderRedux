#include "stdafx.h"
#include "terrain/MCTerrainTiles.h"
#include "lib/MCPacketFile.h"
#include "main/MCGamePaths.h"
#include "main/main.h"

std::string TilePath = "data\\tiles\\";
std::string Tile90Path = "data\\tiles\\";

auto MCTerrainTiles::Create(std::string_view tileFileName)
    -> std::expected<std::unique_ptr<MCTerrainTiles>, std::string>
{
    auto tileFile = std::make_unique<MCPacketFile>();

    if (const int32_t result = tileFile->Open(GamePath(TilePath, tileFileName, ".pak")); result != 0)
    {
        return std::unexpected(
            std::format("could not open the tile file {}.pak ({:#x})", tileFileName, static_cast<uint32_t>(result)));
    }

    // The rotated set is optional: the result isn't checked.
    auto tile90File = std::make_unique<MCPacketFile>();
    tile90File->Open(GamePath(Tile90Path, tileFileName, "90.pak"));
    return std::make_unique<MCTerrainTiles>(std::move(tileFile), std::move(tile90File),
                                            !MCIEquals("tiles", tileFileName));
}

MCTerrainTiles::MCTerrainTiles(std::unique_ptr<MCPacketFile> tileFile, std::unique_ptr<MCPacketFile> tile90File,
                               bool customTileSet)
    : _TileFile(std::move(tileFile)), _Tile90File(std::move(tile90File)), _CustomTileSet(customTileSet)
{
    _NumTiles = _TileFile->GetNumPackets();
    TileSetOffset = {0, _NumTiles / 2};
    // One slot more than the packets: ReadTile takes tileNum == numTiles (the original wrote past the table).
    _Tiles.resize(static_cast<size_t>(std::max(_NumTiles, 0)) + 1);
}

MCTerrainTiles::~MCTerrainTiles() = default;

auto MCTerrainTiles::Preload(std::string_view terrainName) -> void
{
    MCFile preloadFile;

    if (preloadFile.Open(GamePath(TerrainPath, terrainName, ".pre")) != 0)
    {
        return;
    }

    for (uint32_t count = preloadFile.FileSize() >> 2; count != 0; count--)
    {
        const int32_t tileNum = preloadFile.ReadLong();
        ReadTile((_NumTiles >> 1) + tileNum);
    }
}

auto MCTerrainTiles::Lookup(int32_t tileNum, bool rotated) -> MCTerrainTile*
{
    if (tileNum < 0)
    {
        return nullptr;
    }

    tileNum += TileSetOffset[rotated ? 1 : 0];

    if (tileNum >= _NumTiles)
    {
        return nullptr;
    }

    MCTerrainTile& tile = _Tiles[static_cast<size_t>(tileNum)];

    if (tile.Missing)
    {
        return nullptr;
    }

    if (tile.TileData() == nullptr)
    {
        return ReadTile(tileNum);
    }

    return &tile;
}

auto MCTerrainTiles::ReadTile(int32_t tileNum) -> MCTerrainTile*
{
    // Faithful: tileNum == numTiles passes this test (one past the table).
    if (tileNum < 0 || tileNum > _NumTiles)
    {
        return nullptr;
    }

    // The lower half of the table is the rotated set.
    MCPacketFile& file = tileNum < (_NumTiles >> 1) ? *_Tile90File : *_TileFile;
    MCTerrainTile& tile = _Tiles[static_cast<size_t>(tileNum)];
    // Original behaviour: a failed seek marked the tile missing, which the read below overwrote unless the packet size
    // it then found was 0.
    file.SeekPacket(tileNum);
    const int32_t size = file.GetPacketSize();

    if (size == 0)
    {
        tile.Missing = true;
        return nullptr;
    }

    // The original flushed the cache when its heap was full; the port's never is.
    tile.Data = MCRegisteredBlock(static_cast<size_t>(size), MCDataKind::Shapes);
    file.ReadPacket(tileNum, tile.Data.Bytes());
    tile.Missing = false;
    return &tile;
}
