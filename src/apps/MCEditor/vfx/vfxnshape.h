#pragma once

// The drawer of the terrain overlays' shapes (0x0044f5e0, inline assembly among the editor's own code; the source file
// name wasn't kept, the name is the port's). The overlays of Tiles.pak and the ones VFX_nShape_scan makes are shape
// tables in this format:
//
//   table +0  "DNAH" (0x48414e44), +4 shape count, +8 uint32 offset of each shape from the table's start
//   shape +0  "DNAH", +4 int16 hot spot x, +6 int16 hot spot y, +8 uint16 rows, +10 uint16 width,
//         +12 uint16 offset of each row from the shape's start, then the end of the data
//   a row is a list of tokens: n < 0x80 then a colour is a run of n pixels of it (0xff: a skip), 0x80 + n then
//   n colours is a literal
//
// A shape whose first row starts with the run "01 00" is translucent: each pixel becomes
// AlphaTable[colour << 8 | screen pixel].

/// <summary>
/// Draws shape <paramref name="shapeNum"/> of <paramref name="table"/> into <paramref name="buffer"/>
/// (<paramref name="width"/> x <paramref name="height"/>) with its hot spot at (<paramref name="x"/>,
/// <paramref name="y"/>), each colour mapped through <paramref name="xlat"/> unless it is null (a colour that maps to
/// 0xff is then skipped).
/// </summary>
/// <returns>0.</returns>
/// <remarks>
/// Original behaviour, kept (EB-047): every row is drawn one pixel wider than the shape (the extra pixel comes from
/// the next row's first token), the first row is drawn twice and the last never, a shape cut by the left edge isn't
/// cut by the right one, and where the left edge cuts a literal the mapped kinds write it one pixel to the right.
/// </remarks>
uint32_t VFX_nShape_draw(uint8_t* buffer, int32_t width, int32_t height, const uint8_t* table, int32_t shapeNum,
                         int32_t x, int32_t y, const uint8_t* xlat);
