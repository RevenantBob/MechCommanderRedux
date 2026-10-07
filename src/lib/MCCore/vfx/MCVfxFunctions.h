#pragma once

// Every VFX routine MechCommander uses: Miles Design's VFX (hand-written assembly in MCX.EXE, vfxa.asm and
// vfx3d.asm; C names without the cdecl underscore of the asm symbols, _VFX_shape_draw -> VFX_shape_draw) and the
// game's own additions in mcx\vfx\*.cpp (AG_*, fastShapeDraw, the alpha tables, the terrain tile drawer).
// The types are in vfx/MCVfx.h.
//
// Conventions shared by the routines that draw through a pane:
// - coordinates are relative to the pane's (x0, y0); drawing is clipped to the pane intersected with the window;
// - they return VfxErrBadWindow (-1) when the window has no pixels, VfxErrEmptyPane (-2) when the clipped pane
//   is empty, VfxErrClipped (-3) when nothing of the object is inside it, and 0 (or a result) otherwise.
//
// The sections below follow the implementation files; each is owned by its file.

// ---------------------------------------------------------------------------------------------------------------
// Common return codes and modes
// ---------------------------------------------------------------------------------------------------------------

/// <summary>The pane's window has no pixels (x_max or y_max below 0).</summary>
inline constexpr int32_t VfxErrBadWindow = -1;
/// <summary>The pane, clipped to its window, is empty.</summary>
inline constexpr int32_t VfxErrEmptyPane = -2;
/// <summary>The object lies wholly outside the clipped pane.</summary>
inline constexpr int32_t VfxErrClipped = -3;
/// <summary>A shape's bounds are empty (x_max &lt; x_min or y_max &lt; y_min).</summary>
inline constexpr int32_t VfxErrBadShape = -4;

/// <summary>The "no fill" colour of <c>VFX_pane_copy</c> and <c>VFX_pane_scroll</c>.</summary>
inline constexpr int32_t VfxNoColor = -1;

/// <summary>
/// The per-pixel callback of <see cref="VfxLineExecute"/>, given the pixel's pane coordinates. (The asm called it with
/// the coordinates in EDI/ESI and all registers saved; the port passes them as arguments.)
/// </summary>
using MCVfxLineCallback = std::function<void(int32_t x, int32_t y)>;

// ---------------------------------------------------------------------------------------------------------------
// vfxa.asm: pixels, lines, rectangles, panes, ellipses (vfx/MCVfxPrimitives.cpp)
// ---------------------------------------------------------------------------------------------------------------

/// <summary>Sets one pixel.</summary>
/// <returns>The pixel's previous colour, or VFX_ERR_* (-3 when the point is outside the pane).</returns>
int32_t VfxPixelWrite(MCPane* pane, int32_t x, int32_t y, uint8_t color);

/// <summary>Reads one pixel.</summary>
/// <returns>Its colour, or VFX_ERR_* (-3 when the point is outside the pane).</returns>
int32_t VfxPixelRead(MCPane* pane, int32_t x, int32_t y);

/// <summary>
/// Draws the line from (x0, y0) to (x1, y1), both ends included, clipped to the pane, in <paramref name="color"/>
/// (its low byte): <c>VFX_line_draw</c>'s LD_DRAW mode.
/// </summary>
/// <returns>0 when the line was drawn whole, 1 when it was clipped, 2 when nothing of it was inside the pane.</returns>
int32_t VfxLineDraw(MCPane* pane, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t color);

/// <summary>
/// The line of <see cref="VfxLineDraw"/>, each pixel replaced by its entry in the 256-byte <paramref name="table"/>
/// (LD_TRANSLATE).
/// </summary>
int32_t VfxLineTranslate(MCPane* pane, int32_t x0, int32_t y0, int32_t x1, int32_t y1, const uint8_t* table);

/// <summary>The line of <see cref="VfxLineDraw"/>, <paramref name="callback"/> called for each pixel (LD_EXECUTE).</summary>
int32_t VfxLineExecute(MCPane* pane, int32_t x0, int32_t y0, int32_t x1, int32_t y1, const MCVfxLineCallback& callback);

/// <summary>
/// Fills a rectangle (clamped to the pane) with a checkerboard of <paramref name="color"/>: every other pixel,
/// the rows an even distance above the bottom starting at the left edge.
/// </summary>
/// <returns>0, VFX_ERR_* for the pane, or VfxErrBadShape (-4) when nothing is left after clamping.</returns>
int32_t VfxRectangleHash(MCPane* pane, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t color);

/// <summary>Fills the pane with <paramref name="color"/>.</summary>
int32_t VfxPaneWipe(MCPane* pane, int32_t color);

/// <summary>
/// Copies the part of <paramref name="source"/> that, placed with its point (sx, sy) on (tx, ty) of
/// <paramref name="target"/>, overlaps the target (both clipped to their windows). <paramref name="fill"/> selects
/// the operation: negative (VfxNoColor) copies; 0..255 fills that area of the target with the colour instead; above
/// 255 copies all but the pixels of colour <c>fill &amp; 0xff</c> (a colour key).
/// </summary>
/// <returns>0, VFX_ERR_* for either pane, or VfxErrClipped when they don't overlap.</returns>
/// <remarks>Rows and columns are walked in the order that survives overlap.</remarks>
int32_t VfxPaneCopy(MCPane* source, int32_t sx, int32_t sy, MCPane* target, int32_t tx, int32_t ty, int32_t fill);

/// <summary>
/// Scrolls the pane's contents by (dx, dy). <paramref name="mode"/> 0: a scroll by the pane's size or more wipes it
/// with <c>parm &amp; 0xff</c>; otherwise the image moves and the uncovered strips are filled with
/// <paramref name="parm"/> (VFX_pane_copy's fill: a colour 0..255). <paramref name="mode"/> 1 (wrap): the pixels
/// scrolled out come back on the other side.
/// </summary>
/// <returns>0, VfxErrEmptyPane, or in mode 1 with parm 0 the scratch size the asm needed (width * height).</returns>
/// <remarks>
/// In mode 1 the asm took a scratch buffer of width * height bytes as parm; the port allocates
/// its own and only tests parm against 0.
/// </remarks>
int32_t VfxPaneScroll(MCPane* pane, int32_t dx, int32_t dy, int32_t mode, int32_t parm);

/// <summary>
/// Draws the outline of the ellipse centred at (xc, yc) with radii <paramref name="width"/> and
/// <paramref name="height"/> (midpoint algorithm, each point clipped). A zero radius draws the line from
/// (xc - width, yc - height) to (xc + width, yc + height) instead and returns its result.
/// </summary>
int32_t VfxEllipseDraw(MCPane* pane, int32_t xc, int32_t yc, int32_t width, int32_t height, int32_t color);

/// <summary>
/// Fills the ellipse centred at (xc, yc) with radii <paramref name="width"/> and <paramref name="height"/>, one
/// horizontal span per step of the same midpoint walk as <c>VFX_ellipse_draw</c>; a zero radius draws a line.
/// </summary>
int32_t VfxEllipseFill(MCPane* pane, int32_t xc, int32_t yc, int32_t width, int32_t height, int32_t color);

// ---------------------------------------------------------------------------------------------------------------
// vfxa.asm: shapes (vfx/MCVfxShape.cpp, vfx/MCVfxShapeTransform.cpp)
// ---------------------------------------------------------------------------------------------------------------

/// <summary>
/// Draws shape <paramref name="shapeNum"/> of <paramref name="shapeTable"/> with its origin (hot spot) at
/// (hotX, hotY). Colour runs and literal pixels are written as stored; skipped pixels are left alone.
/// </summary>
/// <returns>0, or VFX_ERR_* (-3 wholly clipped, -4 empty bounds).</returns>
int32_t VfxShapeDraw(MCPane* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY);

/// <summary>Sets the 256-byte colour table <c>VFX_shape_translate_draw</c> maps every shape pixel through.</summary>
void VfxShapeLookaside(uint8_t* table);

/// <summary>Draws a shape like <c>VFX_shape_draw</c>, mapping each pixel through the lookaside table.</summary>
int32_t VfxShapeTranslateDraw(MCPane* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY);

/// <summary>
/// Draws a shape rotated by <paramref name="rot"/> (tenths of a degree) and scaled by x_scale/y_scale (16.16) about
/// its hot spot (clockwise on screen for positive angles). The shape is drawn upright into the work
/// <paramref name="buffer"/> (width * height bytes, cleared to 255 first), which is then texture-mapped onto the
/// transformed corners with 255 transparent. <paramref name="flags"/>: VfxShapeTransformXlat maps pixels through the lookaside
/// table as they go into the buffer; VfxShapeTransformReuse skips filling the buffer (it still holds this shape from a previous
/// call). With rot 0 and both scales 1.0 it is a plain VFX_shape_draw / VFX_shape_translate_draw.
/// </summary>
/// <returns>
/// The plain draw's result in the untransformed case, VfxErrBadWindow / VfxErrEmptyPane, else 0 (the asm
/// returned whatever EAX held).
/// </returns>
int32_t VfxShapeTransform(MCPane* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, void* buffer,
                          int32_t rot, int32_t xScale, int32_t yScale, uint32_t flags);

/// <summary><c>VFX_shape_transform</c> flag: map pixels through the shape lookaside table.</summary>
inline constexpr uint32_t VfxShapeTransformXlat = 0x01;
/// <summary><c>VFX_shape_transform</c> flag: the work buffer already holds the shape; don't redraw it.</summary>
inline constexpr uint32_t VfxShapeTransformReuse = 0x02;

/// <summary>
/// The window-space rectangle a shape covers when drawn at (hotX, hotY), mirrored per <paramref name="mirror"/>,
/// written to <paramref name="rectangle"/> as x0, y0, x1, y1.
/// </summary>
/// <remarks>
/// The right and bottom edges are exclusive on x (one past the last pixel) and inclusive on y;
/// a shape without rows gives (0, 0, 0, 0), one without pixels (INT32_MAX, INT32_MAX, INT32_MIN, INT32_MIN).
/// Bit 0 of <paramref name="mirror"/> mirrors about hotX, bit 1 about hotY.
/// </remarks>
int32_t VfxShapeVisibleRectangle(void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, int32_t mirror,
                                 int32_t* rectangle);

/// <summary>Maps every pixel of a shape through the lookaside table, in place.</summary>
int32_t VfxShapeRemapColors(void* shapeTable, int32_t shapeNum);

/// <summary>
/// VFX's shape encoder: encodes the pane's pixels as one shape (its 0x18-byte header and rows, no table header) into
/// <paramref name="buffer"/>, pixels of <paramref name="transparentColor"/> becoming skips, with its origin at
/// (hotX, hotY). With a null buffer only the size is computed.
/// </summary>
/// <returns>The number of bytes written.</returns>
/// <remarks>
/// Named Asm in the port: the
/// game's own C++ VFX_shape_scan (encode_vfx.cpp) has the same name and parameters.
/// </remarks>
int32_t VfxShapeScanAsm(MCPane* pane, uint8_t transparentColor, int32_t hotX, int32_t hotY, void* buffer);

/// <summary>A shape's bounds word: its width in the high 16 bits and height in the low ones.</summary>
int32_t VfxShapeBounds(void* shapeTable, int32_t shapeNum);

/// <summary>A shape's origin word: the hot spot's x in the high 16 bits and y in the low ones.</summary>
int32_t VfxShapeOrigin(void* shapeTable, int32_t shapeNum);

/// <summary>A shape's size: <c>(x_max - x_min + 1) &lt;&lt; 16 | (y_max - y_min + 1)</c> (low word only).</summary>
int32_t VfxShapeResolution(void* shapeTable, int32_t shapeNum);

/// <summary>A shape's top-left offset from its hot spot: <c>x_min &lt;&lt; 16 | (uint16_t)y_min</c>.</summary>
int32_t VfxShapeMinxy(void* shapeTable, int32_t shapeNum);

/// <summary>Writes a shape's palette entries (if it has any) into <paramref name="palette"/> at their indices.</summary>
void VfxShapePalette(void* shapeTable, int32_t shapeNum, MCVfxRgb* palette);

/// <summary>Copies a shape's palette entries to <paramref name="colors"/> (when not null).</summary>
/// <returns>The number of entries (0 without a palette).</returns>
int32_t VfxShapeColors(void* shapeTable, int32_t shapeNum, MCVfxCrgb* colors);

/// <summary>Replaces a shape's palette entries with <paramref name="colors"/> (when not null).</summary>
/// <returns>The number of entries (0 without a palette).</returns>
int32_t VfxShapeSetColors(void* shapeTable, int32_t shapeNum, MCVfxCrgb* colors);

/// <summary>The number of shapes in a table.</summary>
int32_t VfxShapeCount(void* shapeTable);

/// <summary>Whether <paramref name="shapeTable"/> starts with the shape table version "1.10".</summary>
inline bool VfxIsShapeTable(const void* shapeTable)
{
    return std::memcmp(shapeTable, "1.10", 4) == 0;
}

/// <summary>
/// Lists the shapes with distinct data: writes to <paramref name="indexList"/> (when not null) the number of every
/// shape whose data offset no earlier shape shares.
/// </summary>
/// <returns>The number of distinct shapes.</returns>
int32_t VfxShapeList(void* shapeTable, uint32_t* indexList);

/// <summary>Lists the shapes with distinct palettes, as <c>VFX_shape_list</c> does for data.</summary>
int32_t VfxShapePaletteList(void* shapeTable, uint32_t* indexList);

// ---------------------------------------------------------------------------------------------------------------
// vfxa.asm: fixed-point maths (vfx/MCVfxMath.cpp)
// ---------------------------------------------------------------------------------------------------------------

/// <summary>The cosine and sine of <paramref name="angle"/> (tenths of a degree), as 16.16 fixed point.</summary>
void VfxCosSin(int32_t angle, MCFixed16* cosine, MCFixed16* sine);

/// <summary>The 16.16 product of <paramref name="m1"/> and <paramref name="m2"/>, also stored to <paramref name="result"/>.</summary>
MCFixed16 VfxFixedMul(MCFixed16 m1, MCFixed16 m2, MCFixed16* result);

/// <summary>
/// Rotates <paramref name="in"/> by <paramref name="rot"/> (tenths of a degree) about <paramref name="origin"/> and
/// scales it by x_scale/y_scale (16.16), into <paramref name="out"/>.
/// </summary>
void VfxPointTransform(MCVfxPoint* in, MCVfxPoint* out, MCVfxPoint* origin, int32_t rot, int32_t xScale,
                       int32_t yScale);

// ---------------------------------------------------------------------------------------------------------------
// vfxa.asm: fonts (vfx/MCVfxFont.cpp)
// ---------------------------------------------------------------------------------------------------------------

// A VFX font (the game's data\fonts\*.fnt) is a VFX_FONT header, then at +0x10 one dword per character: the offset
// of its glyph from the start of the font. A glyph is a dword width followed by width * char_height pixel bytes, row
// by row, top row first.

/// <summary>The height of a font's characters in pixels (the header's char_height).</summary>
int32_t VfxFontHeight(void* font);

/// <summary>The width of <paramref name="character"/> in pixels (its glyph's first dword).</summary>
int32_t VfxCharacterWidth(void* font, int32_t character);

/// <summary>
/// Draws <paramref name="character"/> with its top-left corner at (x, y), clipped to the pane. Without a
/// <paramref name="colorTranslate"/> table every glyph byte is copied (the background too); with one, each byte is
/// mapped through the table and the pixels it maps to 255 are left alone (transparent).
/// </summary>
/// <returns>The character's width, whether or not any of it was drawn; VFX_ERR_* for a bad or empty pane.</returns>
int32_t VfxCharacterDraw(MCPane* pane, int32_t x, int32_t y, void* font, int32_t character, uint8_t* colorTranslate);

/// <summary>
/// Draws a zero-terminated string from (x, y), each character advancing x by <c>VFX_character_draw</c>'s result.
/// The first character is drawn before the terminator is checked (an empty string draws character 0).
/// </summary>
void VfxStringDraw(MCPane* pane, int32_t x, int32_t y, void* font, const char* string, uint8_t* colorTranslate);

// ---------------------------------------------------------------------------------------------------------------
// vfxa.asm: image files, fades, colour scans (vfx/MCVfxImage.cpp)
// ---------------------------------------------------------------------------------------------------------------

/// <summary>
/// Writes <paramref name="width"/> pixels of <paramref name="line"/> to pane row <paramref name="y"/> from the pane's
/// left edge, clipped to the pane (rows outside it are dropped).
/// </summary>
void VfxLineToPane(MCPane* pane, int32_t y, uint8_t* line, int32_t width);

/// <summary>
/// Draws an IFF picture into the pane from its top-left corner: FORM ILBM (8 bitplanes) or, for any other form
/// type, PBM (chunky); BODY uncompressed or ByteRun1. Pictures with a mask plane (masking 1) aren't drawn.
/// </summary>
/// <returns>The low byte of BMHD's transparent colour (0 for a masked picture, which the asm left undefined).</returns>
int32_t VfxIlbmDraw(MCPane* pane, uint8_t* ilbm);

/// <summary>Copies an ILBM's CMAP (256 entries) into <paramref name="palette"/>, scaled down to 6 bits.</summary>
void VfxIlbmPalette(uint8_t* ilbm, MCVfxRgb* palette);

/// <summary>An ILBM's size from BMHD: width in the high 16 bits, height in the low ones.</summary>
int32_t VfxIlbmResolution(uint8_t* ilbm);

/// <summary>Draws an 8-bit, single-plane, RLE PCX picture into the pane from its top-left corner.</summary>
/// <returns>0.</returns>
int32_t VfxPcxDraw(MCPane* pane, uint8_t* pcx);

/// <summary>Copies a PCX file's 256-colour palette (its last 768 bytes) into <paramref name="palette"/>, scaled to 6 bits.</summary>
void VfxPcxPalette(uint8_t* pcx, int32_t fileSize, MCVfxRgb* palette);

/// <summary>A PCX picture's size: width in the high 16 bits, height in the low ones.</summary>
int32_t VfxPcxResolution(uint8_t* pcx);

/// <summary>The size of the work buffer <c>VFX_GIF_draw</c> needs (the game allocates 0x502e bytes).</summary>
inline constexpr int32_t VfxGifBufferSize = 0x502e;

/// <summary>
/// Draws the first image of a GIF into the pane from its top-left corner, using <paramref name="buffer"/>
/// (<see cref="VfxGifBufferSize"/> bytes) for the LZW state. Interlaced images are drawn in their pass order.
/// Extension blocks before the image aren't skipped (the game's GIFs have none).
/// </summary>
/// <returns>The GIF's background colour index.</returns>
int32_t VfxGifDraw(MCPane* pane, uint8_t* gif, void* buffer);

/// <summary>
/// Copies a GIF's global colour table and then its first image's local colour table (each if present) into
/// <paramref name="palette"/>, scaled down to 6 bits.
/// </summary>
void VfxGifPalette(uint8_t* gif, MCVfxRgb* palette);

/// <summary>A GIF's first image's size: width in the high 16 bits, height in the low ones.</summary>
int32_t VfxGifResolution(uint8_t* gif);

/// <summary>
/// The display palette ("DAC") VFX_window_fade reads and writes, 6-bit entries. Port: the asm went through the
/// registered driver's DAC read/write entries (0x007a80ec/0x007a80f0); the port keeps the palette here and tells
/// the platform layer through <see cref="VfxDacWriteHook"/>.
/// </summary>
extern MCVfxRgb VfxDacPalette[256];
/// <summary>Called after each <see cref="VfxDacPalette"/> entry changes (null: nothing is told).</summary>
extern void (*VfxDacWriteHook)(int32_t index, const MCVfxRgb* rgb);
/// <summary>Called once per vertical retrace wait of a fade (the driver's entry at 0x007a80e0; null: no wait).</summary>
extern void (*VfxWaitRetraceHook)();

/// <summary>
/// Fades the display palette entries of the colours the window uses towards <paramref name="palette"/>, one DAC
/// step per channel at a time, over about <paramref name="intervals"/> vertical retraces.
/// </summary>
void VfxWindowFade(MCWindow* window, MCVfxRgb* palette, int32_t intervals);

/// <summary>
/// Lists in <paramref name="colors"/> (when not null) every distinct colour index in the pane, in the order first met
/// scanning rows top to bottom and each row right to left. The pane isn't clipped to its window.
/// </summary>
/// <returns>The number of distinct colours.</returns>
int32_t VfxColorScan(MCPane* pane, uint32_t* colors);

// ---------------------------------------------------------------------------------------------------------------
// vfx3d.asm: polygons (vfx/MCVfxPolygon.cpp)
// ---------------------------------------------------------------------------------------------------------------

// The polygon routines take vertices relative to the pane's corner clipped to the window (max(0, x0), max(0, y0)),
// which is the pane's own corner for any pane inside its window. Polygons must be convex; rows are filled from the
// top vertex to the bottom one inclusive, each span from its left to its right edge inclusive (edges at x + 0.5
// rounded down).

/// <summary>Fills a convex polygon with the colour of its first vertex (<c>c</c> rounded, 16.16).</summary>
void VfxFlatPolygon(MCPane* pane, std::span<const MCScreenVertex> vertices);

/// <summary>Fills a convex polygon interpolating the vertices' colours (<c>c</c>, 16.16).</summary>
void VfxGouraudPolygon(MCPane* pane, std::span<const MCScreenVertex> vertices);

/// <summary>
/// A Gouraud polygon with <paramref name="ditherAmount"/> (16.16) added to the colour of every other pixel, in a
/// checkerboard, before it is rounded down.
/// </summary>
void VfxDitheredGouraudPolygon(MCPane* pane, MCFixed16 ditherAmount, std::span<const MCScreenVertex> vertices);

/// <summary>Maps every pixel under a convex polygon through the 256-byte table <paramref name="lookaside"/>.</summary>
void VfxTranslatePolygon(MCPane* pane, std::span<const MCScreenVertex> vertices, const uint8_t* lookaside);

/// <summary>
/// Adds the interpolated, dithered vertex colours (as <c>VFX_dithered_Gouraud_polygon</c> computes them) to the pixels
/// under the polygon, as byte offsets into a shading palette.
/// </summary>
void VfxIlluminatePolygon(MCPane* pane, MCFixed16 ditherAmount, std::span<const MCScreenVertex> vertices);

/// <summary>Sets the 256-byte table <c>VFX_map_polygon</c> maps texels through with VfxMapXlat.</summary>
void VfxMapLookaside(const uint8_t* table);

/// <summary><c>VFX_map_polygon</c> flag: map texels through the map lookaside table.</summary>
inline constexpr uint32_t VfxMapXlat = 0x01;
/// <summary>
/// <c>VFX_map_polygon</c> flag: texels of colour 255 (after the lookaside, with VfxMapXlat) are transparent.
/// </summary>
inline constexpr uint32_t VfxMapTransparent = 0x02;

/// <summary>
/// Texture-maps <paramref name="texture"/> onto a convex polygon, affinely per the vertices' u, v (16.16 texel
/// coordinates). <paramref name="flags"/> is a combination of VfxMapXlat and VfxMapTransparent (the asm indexes a jump table with
/// it; only 0..3 exist).
/// </summary>
void VfxMapPolygon(MCPane* pane, std::span<const MCScreenVertex> vertices, MCWindow* texture, uint32_t flags);

// ---------------------------------------------------------------------------------------------------------------
// vfx\alphapalette.cpp (vfx/MCAlphaPalette.cpp)
// ---------------------------------------------------------------------------------------------------------------

/// <summary>The number of alpha "colours": the 256 palette indices plus 24 extra blend-only ones (256..279).</summary>
inline constexpr int32_t AlphaColorCount = 0x118;

/// <summary>
/// The blend tables built by <see cref="InitAlphaLookup"/>: row <c>c</c> (256 bytes, at <c>c * 256</c>) gives, for
/// each background index, the index to draw when colour <c>c</c> is blended over it. Rows of colours that aren't
/// special are plain (every entry <c>c</c>); rows 0 and 255 leave the background unchanged. The game's translucent
/// draws index it as <c>AlphaTable[(c &lt;&lt; 8) | background]</c>.
/// </summary>
/// <remarks>
/// 0x007f09d0 in MCX.EXE, 0x11800 bytes (mangled as <c>char*</c>, as MSVC mangles arrays). Row 0x108 is the darkening
/// table <c>AG_StatusBar</c> draws bar frames with (0x008011d0).
/// </remarks>
extern std::array<uint8_t, AlphaColorCount * 256> AlphaTable;
/// <summary>1 for every alpha colour AlphaPal.ini defines (a translucent colour), else 0. 0x008021d0 in MCX.EXE.</summary>
extern std::array<uint8_t, AlphaColorCount> SpecialColor;

/// <summary>An AlphaPal.ini entry: the colour's (8-bit) components, its weight A and the background's weight B2.</summary>
struct MCAlphaColor
{
    float R = 0.0f;
    float G = 0.0f;
    float B = 0.0f;
    float Alpha = 0.0f;
    float BackgroundWeight = 0.0f;
};

/// <summary>
/// The entries <see cref="InitAlphaLookup"/> read (port: the original kept them on its stack), for a renderer that
/// blends the colours itself instead of looking the result up in <see cref="AlphaTable"/>. Zero where
/// <see cref="SpecialColor"/> is 0.
/// </summary>
extern std::array<MCAlphaColor, AlphaColorCount> MCAlphaColors;

/// <summary>
/// Builds <see cref="AlphaTable"/> and <see cref="SpecialColor"/> for <paramref name="palette"/> from
/// <c>AlphaPal.ini</c>: lines of <c>index R G B A B2</c>; a special colour blended over background (r, g, b) (8-bit)
/// gives <c>background * B2 + RGB * A</c>, or, with A and B2 both 0, <c>background * 255 / (255 - RGB)</c> (a
/// colour dodge). Backgrounds outside 10..245 (the reserved system colours) blend to 255.
/// </summary>
void InitAlphaLookup(std::span<const MCVfxRgb, 256> palette);

/// <summary>
/// The palette index 10..245 nearest to (r, g, b) (6-bit components), by distance weighted 39:51:10 (R:G:B); the first
/// exact match wins.
/// </summary>
uint8_t FindClosest(std::span<const MCVfxRgb, 256> palette, int r, int g, int b);

// ---------------------------------------------------------------------------------------------------------------
// vfx\encode_vfx.cpp (vfx/MCVfxShapeScan.cpp)
// ---------------------------------------------------------------------------------------------------------------

/// <summary>
/// The game's copy of VFX's shape encoder (the asm transcribed into inline assembly with its state in globals):
/// encodes the pane's pixels as one shape (header and rows, no table header) into <paramref name="buffer"/>,
/// <paramref name="transparentColor"/> becoming skips, with its origin at (hotX, hotY). Its output is byte for byte
/// that of <see cref="VfxShapeScanAsm"/>.
/// </summary>
/// <returns>The number of bytes written (or VfxErrBadWindow / VfxErrEmptyPane).</returns>
int VfxShapeScan(MCPane* pane, uint8_t transparentColor, int hotX, int hotY, void* buffer);

// ---------------------------------------------------------------------------------------------------------------
// vfx\fastshp.cpp (vfx/MCFastShape.cpp)
// ---------------------------------------------------------------------------------------------------------------

/// <summary>
/// Draws shape <paramref name="shapeNum"/> of a "fast shape" table (the terrain overlay tiles, tables tagged "DNAH";
/// not the VFX format), its hot spot at (hotX, hotY), each colour mapped through
/// <paramref name="xlat"/> when it isn't null. Run colour 255 is transparent; a shape whose first row begins with the
/// word 1 is drawn translucent through <see cref="AlphaTable"/>.
/// </summary>
/// <returns>0.</returns>
/// <remarks>The original took a last argument it never read (the game passed 0).</remarks>
int32_t FastShapeDraw(MCPane* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, uint8_t* xlat);

// ---------------------------------------------------------------------------------------------------------------
// vfx\vfx_ellipse.cpp (vfx/MCAgEllipse.cpp)
// ---------------------------------------------------------------------------------------------------------------

/// <summary>
/// The game's ellipse outline (midpoint algorithm) centred at (xc, yc) with radii <paramref name="width"/> and
/// <paramref name="height"/>, clipped to the pane. A special (translucent) <paramref name="color"/> blends through
/// <see cref="AlphaTable"/>. A zero radius draws the line from (xc - width, yc - height) to (xc + width, yc + height).
/// </summary>
/// <remarks>
/// Unlike VFX, the centre is offset by the pane's
/// origin clipped to the window.
/// </remarks>
void AGEllipseDraw(MCPane* pane, int32_t xc, int32_t yc, int32_t width, int32_t height, int32_t color);

/// <summary>The game's filled ellipse, as <see cref="AGEllipseDraw"/> with horizontal spans.</summary>
void AGEllipseFill(MCPane* pane, int32_t xc, int32_t yc, int32_t width, int32_t height, int32_t color);

// ---------------------------------------------------------------------------------------------------------------
// vfx\vfx_map_polygon.cpp (vfx/MCAgStatusBar.cpp)
// ---------------------------------------------------------------------------------------------------------------

/// <summary>
/// Draws a translucent status bar in the box (x0, y0)-(x1, y1), in window coordinates (the pane only clips): the
/// top and bottom rows and the left and right columns are darkened through AlphaTable row 0x108, and the first
/// <paramref name="barLength"/> + 1 pixels inside each other row are blended with alpha colour
/// <paramref name="alphaColor"/>.
/// </summary>
void AGStatusBar(MCPane* pane, int x0, int y0, int x1, int y1, int alphaColor, int barLength);

/// <summary>
/// Writes a pixel (the low byte of <paramref name="color"/>) at pane coordinates (x, y), only strictly inside the
/// pane's rectangle (its border rows and columns are never written; the window isn't checked).
/// </summary>
void AGPixelWrite(MCPane* pane, int32_t x, int32_t y, uint32_t color);

/// <summary>
/// Copies the top-left <paramref name="width"/> x <paramref name="height"/> pixels of <paramref name="texture"/> to
/// (x, y) in the pane, skipping texels of colour 255, clipped to the pane.
/// </summary>
/// <returns>0 when anything was drawn, 1 when it was wholly clipped.</returns>
int32_t DrawTransparent(MCPane* pane, MCWindow* texture, int x, int y, int width, int height);

// ---------------------------------------------------------------------------------------------------------------
// vfx\vfx_transform.cpp (vfx/MCAgShapeTransform.cpp)
// ---------------------------------------------------------------------------------------------------------------

/// <summary>
/// The table AG_shape_translate_draw and AG_shape_translate_fill map pixels through, set by
/// <see cref="AGShapeLookaside"/> (the caller's table is used in place, not copied).
/// </summary>
extern uint8_t* Lookaside;

/// <summary>
/// Draws a shape blended onto the pane through <see cref="AlphaTable"/>, at full size or half size (every other
/// pixel of every other row), mirrored left to right when <paramref name="mirror"/> is nonzero: the shape is first
/// rendered opaque into a scratch picture by <see cref="AGShapeFill"/> (skips as colour 0), then blended by
/// <see cref="CopySprite"/>. (hotX, hotY) are relative to the pane's corner clipped to the window.
/// </summary>
/// <remarks>
/// The original rendered into a buffer its callers passed (0x1fa40 bytes: shapes of 360 x 360 pixels or more weren't
/// drawn); the port's renderers keep their own.
/// </remarks>
void AGShapeTransform(MCPane* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, int32_t mirror,
                      int32_t fullSize);

/// <summary>As <see cref="AGShapeTransform"/>, rendering the shape through <see cref="Lookaside"/> first.</summary>
void AGShapeTranslateTransform(MCPane* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY,
                               int32_t mirror, int32_t fullSize);

/// <summary>
/// Blends a <paramref name="width"/> x <paramref name="height"/> 8-bit <paramref name="sprite"/> onto the pane at
/// (x, y) relative to the pane's corner clipped to the window: each pixel becomes
/// <c>AlphaTable[sprite &lt;&lt; 8 | screen]</c>. <paramref name="fullSize"/> 0 halves it (every other pixel of every
/// other row); <paramref name="mirror"/> nonzero mirrors it left to right. Clipped to the pane.
/// </summary>
void CopySprite(MCPane* pane, uint8_t* sprite, int x, int y, int width, int height, int mirror, int fullSize);

/// <summary>
/// Port: <see cref="CopySprite(PANE*, uint8_t*, int, int, int, int, int, int)"/> of a window's pixels, which the
/// renderers read through its texture (the sprite is the whole window, <paramref name="width"/> its width).
/// </summary>
void CopySprite(MCPane* pane, MCWindow* sprite, int x, int y, int width, int height, int mirror, int fullSize);

/// <summary>
/// Draws a shape opaque, its skipped pixels written as colour 0, with its hot spot at window coordinates
/// (hotX, hotY) (the pane only clips): renders a shape into a scratch bitmap.
/// </summary>
void AGShapeFill(MCPane* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY);

/// <summary>As <see cref="AGShapeFill"/>, each pixel mapped through <see cref="Lookaside"/> (skips stay 0).</summary>
/// <remarks>When clipped, the pane's right column itself is not written.</remarks>
void AGShapeTranslateFill(MCPane* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY);

// ---------------------------------------------------------------------------------------------------------------
// vfx\vfx_translatedraw.cpp (vfx/MCAgShapeDraw.cpp)
// ---------------------------------------------------------------------------------------------------------------

/// <summary>
/// The game's shape draw: <c>VFX_shape_draw</c>'s format and pixels, but with the hot spot at window coordinates
/// (hotX, hotY) (the pane only clips) and translucent shapes: a shape whose data begins with the token pair 03 00 is
/// blended, each pixel becoming <c>AlphaTable[shape &lt;&lt; 8 | screen]</c>, one column to the right.
/// </summary>
void AGShapeDraw(MCPane* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY);

/// <summary>Sets <see cref="Lookaside"/>, the table AG_shape_translate_draw and AG_shape_translate_fill use.</summary>
void AGShapeLookaside(uint8_t* table);

/// <summary>
/// As <see cref="AGShapeDraw"/>, each pixel mapped through <see cref="Lookaside"/> (translucent shapes blended
/// first, then mapped).
/// </summary>
void AGShapeTranslateDraw(MCPane* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY);

// ---------------------------------------------------------------------------------------------------------------
// vfx\vfxtile.cpp (vfx/MCVfxTile.cpp)
// ---------------------------------------------------------------------------------------------------------------

/// <summary>The <c>xlat</c> of <see cref="VfxNTileDraw"/> that fills the tile with colour 0x10 (the original's -1).</summary>
inline uint8_t* const VfxTileFill = reinterpret_cast<uint8_t*>(static_cast<intptr_t>(-1));

/// <summary>
/// Draws a terrain tile (format in vfx/MCVfxTile.cpp) with its hot spot at (x, y) relative to the pane's corner
/// clipped to the window, clipped to the pane: copied when <paramref name="xlat"/> is null, filled with colour 0x10
/// when it is <see cref="VfxTileFill"/>, else mapped through the 256-byte table it points to (a haze palette).
/// </summary>
/// <returns>0, or 0xcdcf0001 when the tile is wholly outside the pane.</returns>
int32_t VfxNTileDraw(MCPane* pane, uint8_t* tile, int32_t x, int32_t y, uint8_t* xlat);
