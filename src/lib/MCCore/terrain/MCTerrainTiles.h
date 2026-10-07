#pragma once

#include "platform/MCRegisteredBlock.h"

class MCPacketFile;

/// <summary>
/// A cache slot for one terrain tile: the tile's bitmap (a VFX tile shape, one packet of the tile file) once it is
/// loaded, or a note that its packet is missing.
/// </summary>
struct MCTerrainTile
{
    /// <summary>The tile's VFX shape data (null while not loaded, or when <see cref="Missing"/>).</summary>
    uint8_t* TileData() const { return Data.Data(); }

    /// <summary>The loaded tile's data, registered with the renderers.</summary>
    MCRegisteredBlock Data;
    /// <summary>The tile's packet is missing or empty: it is never read again.</summary>
    bool Missing = false;
};

/// <summary>
/// The terrain tile cache: the tile bitmaps of the terrain's tile set, read on demand from two packet files
/// (<c>&lt;tilePath&gt;&lt;name&gt;.pak</c> and the rotated set <c>&lt;tile90Path&gt;&lt;name&gt;90.pak</c>). The
/// original's cache was a private heap, flushed least-recently-used when it filled; the port's never fills.
/// </summary>
/// <remarks>
/// The slot table has one entry per packet of the first file; <see cref="TileSetOffset"/>[1] (half the tiles)
/// selects the second half of the table for the camera's rotated view. Tiles below that half come from the rotated
/// file, the rest from the normal one. A <c>.pre</c> file in the terrain folder lists (as int32 indices) the tiles to
/// preload.
/// </remarks>
class MCTerrainTiles
{
public:
    /// <summary>Opens both tile files of the tile set <paramref name="tileFileName"/>.</summary>
    static std::expected<std::unique_ptr<MCTerrainTiles>, std::string> Create(std::string_view tileFileName);

    /// <summary>A cache over <paramref name="tileFile"/> and <paramref name="tile90File"/> (which may be closed).</summary>
    MCTerrainTiles(std::unique_ptr<MCPacketFile> tileFile, std::unique_ptr<MCPacketFile> tile90File,
                   bool customTileSet);

    ~MCTerrainTiles();
    MCTerrainTiles(const MCTerrainTiles&) = delete;
    MCTerrainTiles& operator=(const MCTerrainTiles&) = delete;

    /// <summary>Loads the tiles listed in <c>&lt;terrainPath&gt;&lt;terrainName&gt;.pre</c>, if there is one.</summary>
    void Preload(std::string_view terrainName);

    /// <summary>
    /// The cached tile <paramref name="tileNum"/> of the view's tile set (<paramref name="rotated"/>: the rotated
    /// set), read in when it isn't loaded.
    /// </summary>
    /// <returns>The tile, or null when the number is negative, past the table, or its packet is missing.</returns>
    MCTerrainTile* Lookup(int32_t tileNum, bool rotated = false);

    /// <summary>Reads slot <paramref name="tileNum"/> of the table into the cache.</summary>
    /// <returns>The slot, or null when its packet is missing or empty.</returns>
    MCTerrainTile* ReadTile(int32_t tileNum);

    /// <summary>Number of tile slots (packets in the tile file).</summary>
    int32_t NumTiles() const { return _NumTiles; }

    /// <summary>The tile set isn't the default one ("tiles").</summary>
    bool CustomTileSet() const { return _CustomTileSet; }

    /// <summary>Added to a vertex's tile index: [0] = 0 for the normal view, [1] = numTiles / 2 for the rotated one.</summary>
    std::array<int32_t, 2> TileSetOffset{};

private:
    /// <summary>Number of tile slots (packets in the tile file).</summary>
    int32_t _NumTiles = 0;
    /// <summary>The slots (one more than <see cref="_NumTiles"/>).</summary>
    std::vector<MCTerrainTile> _Tiles;
    /// <summary><c>&lt;name&gt;.pak</c>.</summary>
    std::unique_ptr<MCPacketFile> _TileFile;
    /// <summary><c>&lt;name&gt;90.pak</c>, the tiles for the rotated view.</summary>
    std::unique_ptr<MCPacketFile> _Tile90File;
    /// <summary>The tile set isn't the default one ("tiles").</summary>
    bool _CustomTileSet = false;
};

/// <summary>The folder of the tile files ("data\tiles\").</summary>
extern std::string TilePath;
/// <summary>The folder of the rotated tile files ("data\tiles\").</summary>
extern std::string Tile90Path;
