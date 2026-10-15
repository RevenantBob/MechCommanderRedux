#pragma once

// The overlay on a map tile (0x0046e050-0x0046e1b0): a road, wall, forest or the like, as overlay kinds on the tile's
// corners and the overlay tile that shows them. Its source file name wasn't kept; the names are the port's.

/// <summary>The overlay of a <see cref="EditorTile"/>.</summary>
/// <remarks>0x28 bytes, no virtual table.</remarks>
struct MapOverlay
{
    /// <summary>No tile, no kinds.</summary>
    MapOverlay();
    /// <summary>A copy of <paramref name="other"/> (the tile reference copied without counting).</summary>
    MapOverlay(const MapOverlay& other);
    /// <summary>The overlay <paramref name="tileId"/>, with <paramref name="kinds"/> on its corners.</summary>
    MapOverlay(const int32_t* kinds, int32_t tileId, bool damaged);
    MapOverlay& operator=(const MapOverlay&) = delete;

    /// <summary>
    /// Takes <paramref name="kinds"/> and the overlay tile for them on a tile of <paramref name="heights"/>.
    /// </summary>
    /// <returns>false, changing nothing, when the library has no such overlay.</returns>
    bool set(const int32_t* kinds, const uint8_t* heights, bool damaged);
    /// <summary>
    /// Puts <paramref name="kind"/> on corner <paramref name="corner"/> and takes the overlay tile for the kinds then.
    /// </summary>
    /// <returns>false when the library has no such overlay (the corner keeps its new kind).</returns>
    bool setCorner(int32_t corner, int32_t kind, const uint8_t* heights);
    /// <summary>
    /// Draws the overlay tile's art at zoom level <paramref name="zoom"/> (fetched on first use) into
    /// <paramref name="buffer"/> at (<paramref name="x"/>, <paramref name="y"/>), mapped through
    /// <paramref name="xlat"/> only when <see cref="tinted"/>.
    /// </summary>
    /// <remarks>0x00452060.</remarks>
    void draw(uint8_t* buffer, int32_t width, int32_t height, int32_t x, int32_t y, const uint8_t* xlat, int32_t zoom);

    /// <summary>Whether the map's tint applies to the overlay; never set but by copying.</summary>
    uint8_t tinted = 0;
    /// <summary>The overlay tile.</summary>
    int32_t tileId = -1;
    /// <summary>The overlay tile's art at each zoom level (the library's, not owned), fetched on first draw.</summary>
    uint8_t* zoomArt[3] = {};
    /// <summary>The overlay kind on each corner, <c>OVERLAY_NONE</c> where it has none.</summary>
    int32_t kinds[4];
    /// <summary>The damaged look.</summary>
    bool damaged = false;
};
