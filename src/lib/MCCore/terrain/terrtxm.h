#pragma once

class PacketFile;
class UserHeap;

/// <summary>
/// A cache slot for one terrain tile: the tile's bitmap (a VFX tile shape, one packet of the tile file) while it is
/// loaded and the turn it was last drawn, for the LRU flush.
/// </summary>
/// <remarks>
/// 8 bytes in the original. <see cref="tileData"/> is null while the tile isn't loaded and the sentinel
/// <see cref="TILE_MISSING"/> (-1 in the original) when its packet doesn't exist.
/// </remarks>
struct TerrainTile
{
    /// <summary>The value of <see cref="tileData"/> for a tile whose packet is missing.</summary>
    static inline uint8_t* const TILE_MISSING = reinterpret_cast<uint8_t*>(static_cast<intptr_t>(-1));

    /// <summary>The tile's VFX shape data (from <c>TerrainTiles::tileHeap</c>).</summary>
    uint8_t* tileData; // +0x00
    /// <summary>The <c>turn</c> the tile was last used (-1: never flush).</summary>
    int32_t lastTurnUsed; // +0x04
};

/// <summary>
/// The terrain tile cache: the tile bitmaps of the terrain's tile set, read on demand from two packet files
/// (<c>&lt;tilePath&gt;&lt;name&gt;.pak</c> and the rotated set <c>&lt;tile90Path&gt;&lt;name&gt;90.pak</c>) into a
/// private heap, flushed least-recently-used when it fills.
/// </summary>
/// <remarks>
/// Original source: <c>terrain\terrtxm.cpp</c>, 0x20 bytes. The slot table has one entry per packet of the first
/// file; <see cref="tileSetOffset"/>[1] (half the tiles) selects the second half of the table for the camera's
/// rotated view. Tiles below that half come from the rotated file, the rest from the normal one. A <c>.pre</c>
/// file in the terrain folder lists (as int32 indices) the tiles to preload.
/// </remarks>
class TerrainTiles
{
public:
    /// <summary>
    /// Creates the tile heap (<paramref name="heapSize"/> bytes), opens both tile files and allocates the slot
    /// table.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00747820</remarks>
    int32_t init(char* tileFileName, int32_t heapSize);

    /// <summary>Loads the tiles listed in <c>&lt;terrainPath&gt;&lt;terrainName&gt;.pre</c> (never flushed).</summary>
    /// <remarks>MCX.EXE @ 0x00747a30</remarks>
    int32_t preload(char* terrainName);

    /// <summary>Closes the tile files and frees the heap.</summary>
    /// <remarks>MCX.EXE @ 0x00747af0</remarks>
    void destroy();

    /// <summary>Frees every tile not used this turn.</summary>
    /// <remarks>MCX.EXE @ 0x00747b60</remarks>
    void dumpLRU(int32_t bytesNeeded);

    /// <summary>Reads tile <paramref name="tileNum"/> into the cache.</summary>
    /// <returns>Its slot, or null when it doesn't exist or doesn't fit.</returns>
    /// <remarks>MCX.EXE @ 0x00747bc0</remarks>
    TerrainTile* readTile(int32_t tileNum);

    /// <summary>Number of tile slots (packets in the tile file).</summary>
    int32_t numTiles = 0; // +0x00
    /// <summary>Added to a vertex's tile index: [0] = 0 for the normal view, [1] = numTiles / 2 for the rotated one.</summary>
    int32_t tileSetOffset[2] = {}; // +0x04
    /// <summary>The slots.</summary>
    TerrainTile* tiles = nullptr; // +0x0c
    /// <summary><c>&lt;name&gt;.pak</c>.</summary>
    PacketFile* tileFile = nullptr; // +0x10
    /// <summary><c>&lt;name&gt;90.pak</c>, the tiles for the rotated view.</summary>
    PacketFile* tile90File = nullptr; // +0x14
    /// <summary>The heap the tiles are read into.</summary>
    UserHeap* tileHeap = nullptr; // +0x18
    /// <summary>1 unless the tile set is the default one ("tiles").</summary>
    int32_t customTileSet = 0; // +0x1c
};

/// <summary>The folder of the tile files ("data\tiles\").</summary>
extern char tilePath[80];
/// <summary>The folder of the rotated tile files ("data\tiles\").</summary>
extern char tile90Path[80];
