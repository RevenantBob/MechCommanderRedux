#pragma once

// The shapes the editing tools show on the map while they work (an object under the cursor, a brush outline), kept in
// the map's mark list (EditorMap::marks) and drawn by its marks pass. The name is the port's.

/// <summary>A shape a tool shows at a view position, drawn over the map.</summary>
/// <remarks>Virtual table slot 0 draws, slot 1 is the deleting destructor.</remarks>
class MapMarker
{
public:
    /// <summary>
    /// Draws the shape at (<paramref name="x"/>, <paramref name="y"/>) on <paramref name="dc"/> (a
    /// <paramref name="width"/> x <paramref name="height"/> bitmap) at zoom level <paramref name="zoom"/>.
    /// </summary>
    /// <returns>Whether it drew.</returns>
    virtual bool draw(HDC dc, int32_t width, int32_t height, int32_t x, int32_t y, int32_t zoom) = 0;
    virtual ~MapMarker() = default;
};
