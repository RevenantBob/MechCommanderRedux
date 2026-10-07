#pragma once

class MCPacketFile;

/// <summary>
/// A cache slot for one terrain tile: the tile's bitmap (a VFX tile shape, one packet of the tile file) while it is
/// loaded and the turn it was last drawn, for the LRU flush.
/// </summary>
/// <remarks>
/// 8 bytes in the original. <see cref="TileData"/> is null while the tile isn't loaded and the sentinel
/// <see cref="TILE_MISSING"/> (-1 in the original) when its packet doesn't exist.
/// </remarks>
struct MCTerrainTile
{
    /// <summary>The value of <see cref="TileData"/> for a tile whose packet is missing.</summary>
    static inline uint8_t* const TILE_MISSING = reinterpret_cast<uint8_t*>(static_cast<intptr_t>(-1));

    /// <summary>Unregisters and frees the tile's data; the slot reads as not loaded.</summary>
    void Free();

    /// <summary>The tile's VFX shape data (<see cref="Storage"/>, or <see cref="TILE_MISSING"/>).</summary>
    uint8_t* TileData = nullptr;
    /// <summary>The <c>turn</c> the tile was last used (-1: never flush).</summary>
    int32_t LastTurnUsed = 0;
    /// <summary>The loaded tile's data.</summary>
    std::unique_ptr<uint8_t[]> Storage;
};

/// <summary>
/// The terrain tile cache: the tile bitmaps of the terrain's tile set, read on demand from two packet files
/// (<c>&lt;tilePath&gt;&lt;name&gt;.pak</c> and the rotated set <c>&lt;tile90Path&gt;&lt;name&gt;90.pak</c>). The
/// original's cache was a private heap, flushed least-recently-used when it filled; the port's never fills.
/// </summary>
/// <remarks>
/// Original source: <c>terrain\terrtxm.cpp</c>, 0x20 bytes. The slot table has one entry per packet of the first
/// file; <see cref="TileSetOffset"/>[1] (half the tiles) selects the second half of the table for the camera's
/// rotated view. Tiles below that half come from the rotated file, the rest from the normal one. A <c>.pre</c>
/// file in the terrain folder lists (as int32 indices) the tiles to preload.
/// </remarks>
class MCTerrainTiles
{
public:
    /// <summary>Opens both tile files and allocates the slot table.</summary>
    int32_t Init(char* tileFileName);

    /// <summary>Loads the tiles listed in <c>&lt;terrainPath&gt;&lt;terrainName&gt;.pre</c> (never flushed).</summary>
    int32_t Preload(char* terrainName);

    /// <summary>Closes the tile files and frees the tiles.</summary>
    void Destroy();

    /// <summary>Frees every tile not used this turn.</summary>
    void DumpLru(int32_t bytesNeeded);

    /// <summary>Reads tile <paramref name="tileNum"/> into the cache.</summary>
    /// <returns>Its slot, or null when it doesn't exist or doesn't fit.</returns>
    MCTerrainTile* ReadTile(int32_t tileNum);

    /// <summary>Number of tile slots (packets in the tile file).</summary>
    int32_t NumTiles = 0;
    /// <summary>Added to a vertex's tile index: [0] = 0 for the normal view, [1] = numTiles / 2 for the rotated one.</summary>
    int32_t TileSetOffset[2] = {};
    /// <summary>The slots (one more than <see cref="NumTiles"/>).</summary>
    std::vector<MCTerrainTile> Tiles;
    /// <summary><c>&lt;name&gt;.pak</c>.</summary>
    MCPacketFile* TileFile = nullptr;
    /// <summary><c>&lt;name&gt;90.pak</c>, the tiles for the rotated view.</summary>
    MCPacketFile* Tile90File = nullptr;
    /// <summary>1 unless the tile set is the default one ("tiles").</summary>
    int32_t CustomTileSet = 0;
};

/// <summary>The folder of the tile files ("data\tiles\").</summary>
extern char TilePath[80];
/// <summary>The folder of the rotated tile files ("data\tiles\").</summary>
extern char Tile90Path[80];
