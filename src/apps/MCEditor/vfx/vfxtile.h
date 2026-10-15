#pragma once

#include "vfx/vfx.h"

// The editor's revision of the game's vfx\vfxtile.cpp (0x00479da0-0x0047a630): the terrain tile drawer, which here
// draws into a bare 8-bit buffer and can outline each row, and the two encoders that turn a drawn 80 x 60 tile or
// overlay into art for the smallest zoom. The encoders' names are the port's.
//
// A terrain tile is a diamond stored as one span per row:
//
//   +0  uint8   hot spot x (subtracted from the draw position)
//   +1  uint8   hot spot y
//   +2  uint8   rows drawn
//   +3  uint8   width (columns of the bounding box)
//   +4  uint32  offset[rows + 1]: where each row starts, from the tile's start; the last is the end of the data
//   row i, at offset[i]: uint8 x (the span's first column), then offset[i + 1] - offset[i] - 1 pixels

/// <summary>The <c>xlat</c> of <see cref="VFX_nTile_draw"/> that fills the tile with colour 0x10 (the original's -1).</summary>
inline const uint8_t* const VFX_TILE_FILL = reinterpret_cast<const uint8_t*>(static_cast<intptr_t>(-1));
/// <summary>The colour <see cref="VFX_TILE_FILL"/> fills with.</summary>
inline constexpr uint8_t VFX_TILE_FILL_COLOR = 0x10;
/// <summary>The <c>outline</c> of <see cref="VFX_nTile_draw"/> that draws no outline.</summary>
inline constexpr int32_t VFX_NO_OUTLINE = -1;
/// <summary>What <see cref="VFX_nTile_draw"/> returns for a tile wholly outside the buffer.</summary>
inline constexpr uint32_t VFX_TILE_OUTSIDE = 0xcdcf0001;
/// <summary>The colour the encoders take for no pixel.</summary>
inline constexpr uint8_t VFX_TRANSPARENT = 0xff;

/// <summary>
/// Draws terrain tile <paramref name="tile"/> into <paramref name="buffer"/> (<paramref name="width"/> x
/// <paramref name="height"/>) with its hot spot at (<paramref name="x"/>, <paramref name="y"/>): copied when
/// <paramref name="xlat"/> is null, filled with colour 0x10 when it is <see cref="VFX_TILE_FILL"/>, else mapped
/// through the 256-byte table it points to; each row's first and last pixel become <paramref name="outline"/> unless
/// it is <see cref="VFX_NO_OUTLINE"/>.
/// </summary>
/// <returns>0, or <see cref="VFX_TILE_OUTSIDE"/> when the tile is wholly outside (a tile on the last column or row
/// counts as outside).</returns>
/// <remarks>0x0047a290; MCX.EXE's revision (0x006b89b0) draws into a pane instead and has no outline.</remarks>
uint32_t VFX_nTile_draw(uint8_t* buffer, int32_t width, int32_t height, const uint8_t* tile, int32_t x, int32_t y,
                        const uint8_t* xlat, int32_t outline);

/// <summary>
/// Encodes rows <c>y0</c> to <c>y1 - 1</c> of <paramref name="pane"/>'s window as a terrain tile with hot spot
/// (<paramref name="hotX"/>, <paramref name="hotY"/>): each row's pixels from <c>x0</c> to <c>x1 - 1</c> but
/// <see cref="VFX_TRANSPARENT"/> ones, starting at the first such. The tile's rows and width bytes are the window's
/// <c>y_max</c> and <c>x_max</c>.
/// </summary>
/// <param name="tile">Gets the tile, a new[] block of 0x20000 bytes.</param>
/// <returns>The tile's size in bytes.</returns>
/// <remarks>
/// 0x00479da0. The pane's window must be as wide as the pane: the pixels are read one after another. A transparent
/// pixel after the first opaque one is dropped, not kept, so only tiles without holes survive.
/// </remarks>
int32_t VFX_nTile_scan(const PANE& pane, uint8_t hotX, uint8_t hotY, uint8_t** tile);

/// <summary>
/// Encodes <paramref name="pane"/>'s pixels as an overlay shape (format in vfx/vfxnshape.h) with hot spot
/// (<paramref name="hotX"/>, <paramref name="hotY"/>) in pane coordinates: cropped to the box of the
/// non-<see cref="VFX_TRANSPARENT"/> pixels, one empty row and column added above and left.
/// </summary>
/// <param name="shape">Gets the shape, a new[] block of 0x20000 bytes; null when it can't be allocated.</param>
/// <returns>The shape's size in bytes, or -1.</returns>
/// <remarks>0x00479fe0.</remarks>
int32_t VFX_nShape_scan(const PANE& pane, uint8_t hotX, uint8_t hotY, uint8_t** shape);
