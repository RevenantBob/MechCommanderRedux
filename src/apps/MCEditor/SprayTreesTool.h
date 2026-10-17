#pragma once

#include "PlaceObjectTool.h"

// The Spray Trees tool (virtual tables 0x004fad54 and 0x004fad28). Its source file name wasn't kept; the names are
// the port's.

/// <summary>
/// The Spray Trees tool: scatters the library's spray-can objects (the trees) at random on the cells around the one
/// under the cursor, thicker in the middle; recorded as placed objects.
/// </summary>
/// <remarks>
/// 0x260c4 bytes: a place tool (virtual table 0x004fad54) with a cursor tool after it (0x004fad28), which the view
/// holds and whose slots pass on to the place tool's but for the cursor images. The port keeps the cursor as a member.
/// </remarks>
class SprayTreesTool : public PlaceObjectTool
{
public:
    /// <summary>A tool placing the first spray-can object, with the spray cursor.</summary>
    /// <remarks>0x00410f30.</remarks>
    SprayTreesTool();

    /// <summary>Begins as a place tool, its action named "Spray Trees".</summary>
    /// <returns>true.</returns>
    /// <remarks>0x00411090.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>
    /// For each cell within 3 of the one under (<paramref name="x"/>, <paramref name="y"/>), places a random tree
    /// there with a chance of 6 in 10 next to it, else 4 in 10, where one can stand.
    /// </summary>
    /// <returns>false off the grid, else true.</returns>
    /// <remarks>0x00411190.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>false.</returns>
    /// <remarks>0x00411330.</remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>true.</returns>
    /// <remarks>The cursor tool's slot (0x00401d40).</remarks>
    bool canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>true.</returns>
    /// <remarks>0x00410fc0.</remarks>
    bool hasTarget(EditorMap& map) override;
    /// <summary>The spray cursor.</summary>
    /// <remarks>0x00410fd0.</remarks>
    const uint8_t* cursorImage(EditorMap& map, const POINT& position, POINT* topLeft, SIZE* size,
                               int32_t zoom) override;
    /// <summary>The red spray cursor.</summary>
    /// <remarks>The cursor tool's slot (0x00408460).</remarks>
    const uint8_t* notAllowedImage(EditorMap& map, const POINT& position, POINT* topLeft, SIZE* size,
                                   int32_t zoom) override;

    /// <summary>
    /// For each cell of each selected tile, places a random tree there with a chance of 5 in 10 where one can stand.
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>0x00411360.</remarks>
    bool spraySelection(EditorMap& map);

private:
    /// <summary>
    /// Places a random tree (<paramref name="draw"/> picks it) on cell (<paramref name="col"/>, <paramref name="row"/>)
    /// where one can stand, recording it.
    /// </summary>
    void plantTree(EditorMap& map, uint32_t draw, int32_t col, int32_t row);

    /// <summary>The cursor (the second base, +0x258b4).</summary>
    CursorTool cursor;
};
