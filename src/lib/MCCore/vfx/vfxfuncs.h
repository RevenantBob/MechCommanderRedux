#pragma once

// Every VFX routine MechCommander uses: Miles Design's VFX (hand-written assembly in MCX.EXE, vfxa.asm and
// vfx3d.asm; C names without the cdecl underscore of the asm symbols, _VFX_shape_draw -> VFX_shape_draw) and the
// game's own additions in mcx\vfx\*.cpp (AG_*, fastShapeDraw, the alpha tables, the terrain tile drawer).
// The types are in vfx/vfx.h.
//
// Conventions shared by the routines that draw through a pane:
// - coordinates are relative to the pane's (x0, y0); drawing is clipped to the pane intersected with the window;
// - they return VFX_ERR_BAD_WINDOW (-1) when the window has no pixels, VFX_ERR_EMPTY_PANE (-2) when the clipped pane
//   is empty, VFX_ERR_CLIPPED (-3) when nothing of the object is inside it, and 0 (or a result) otherwise.
//
// The sections below follow the implementation files; each is owned by its file.

// ---------------------------------------------------------------------------------------------------------------
// Common return codes and modes
// ---------------------------------------------------------------------------------------------------------------

/// <summary>The pane's window has no pixels (x_max or y_max below 0).</summary>
inline constexpr int32_t VFX_ERR_BAD_WINDOW = -1;
/// <summary>The pane, clipped to its window, is empty.</summary>
inline constexpr int32_t VFX_ERR_EMPTY_PANE = -2;
/// <summary>The object lies wholly outside the clipped pane.</summary>
inline constexpr int32_t VFX_ERR_CLIPPED = -3;
/// <summary>A shape's bounds are empty (x_max &lt; x_min or y_max &lt; y_min).</summary>
inline constexpr int32_t VFX_ERR_BAD_SHAPE = -4;

/// <summary><c>VFX_line_draw</c> mode: plot <c>parm</c> as the colour.</summary>
inline constexpr int32_t LD_DRAW = 0;
/// <summary><c>VFX_line_draw</c> mode: replace each pixel by its entry in the table <c>parm</c> points to.</summary>
inline constexpr int32_t LD_TRANSLATE = 1;
/// <summary><c>VFX_line_draw</c> mode: call the <see cref="VFX_LINE_CALLBACK"/> <c>parm</c> points to per pixel.</summary>
inline constexpr int32_t LD_EXECUTE = 2;

/// <summary>The "no fill" colour of <c>VFX_pane_copy</c> and <c>VFX_pane_scroll</c>.</summary>
inline constexpr int32_t NO_COLOR = -1;

/// <summary>
/// A per-pixel callback of <c>VFX_line_draw</c>'s LD_EXECUTE mode, given the pixel's pane coordinates. (The asm
/// called it with the coordinates in EDI/ESI and all registers saved; the port passes them as arguments.)
/// </summary>
using VFX_LINE_CALLBACK = void (*)(int32_t x, int32_t y);

// ---------------------------------------------------------------------------------------------------------------
// vfxa.asm: driver, pixels, lines, rectangles, panes, ellipses (vfx/vfxa.cpp)
// ---------------------------------------------------------------------------------------------------------------

/// <summary>The display driver's name. The port has no VFX drivers: returns "SDL".</summary>
/// <remarks>MCX.EXE @ 0x0076cc04</remarks>
char* VFX_driver_name(void* driver);

/// <summary>Registers a display driver's description table. The port has no VFX drivers: ignored.</summary>
/// <remarks>MCX.EXE @ 0x0076cc2d</remarks>
void VFX_register_driver(void* describe);

/// <summary>Sets one pixel.</summary>
/// <returns>The pixel's previous colour, or VFX_ERR_* (-3 when the point is outside the pane).</returns>
/// <remarks>MCX.EXE @ 0x0076cc4c</remarks>
int32_t VFX_pixel_write(PANE* pane, int32_t x, int32_t y, uint8_t color);

/// <summary>Reads one pixel.</summary>
/// <returns>Its colour, or VFX_ERR_* (-3 when the point is outside the pane).</returns>
/// <remarks>MCX.EXE @ 0x0076cd27</remarks>
int32_t VFX_pixel_read(PANE* pane, int32_t x, int32_t y);

/// <summary>
/// Draws the line from (x0, y0) to (x1, y1), both ends included, clipped to the pane. <paramref name="mode"/> is
/// LD_DRAW (<paramref name="parm"/> is the colour), LD_TRANSLATE (a pointer to a 256-byte table) or LD_EXECUTE (a
/// <see cref="VFX_LINE_CALLBACK"/>).
/// </summary>
/// <returns>0 when the line was drawn whole, 1 when it was clipped, 2 when nothing of it was inside the pane.</returns>
/// <remarks>MCX.EXE @ 0x0076cdfd (unnamed in the symbols: VFX's line routine, its only caller-visible entry).</remarks>
int32_t VFX_line_draw(PANE* pane, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t mode, intptr_t parm);

/// <summary>
/// Fills a rectangle (clamped to the pane) with a checkerboard of <paramref name="color"/>: every other pixel,
/// the rows an even distance above the bottom starting at the left edge.
/// </summary>
/// <returns>0, VFX_ERR_* for the pane, or VFX_ERR_BAD_SHAPE (-4) when nothing is left after clamping.</returns>
/// <remarks>MCX.EXE @ 0x0076d7ff</remarks>
int32_t VFX_rectangle_hash(PANE* pane, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t color);

/// <summary>Fills the pane with <paramref name="color"/>.</summary>
/// <remarks>MCX.EXE @ 0x0076f7cc</remarks>
int32_t VFX_pane_wipe(PANE* pane, int32_t color);

/// <summary>
/// Copies the part of <paramref name="source"/> that, placed with its point (sx, sy) on (tx, ty) of
/// <paramref name="target"/>, overlaps the target (both clipped to their windows). <paramref name="fill"/> selects
/// the operation: negative (NO_COLOR) copies; 0..255 fills that area of the target with the colour instead; above
/// 255 copies all but the pixels of colour <c>fill &amp; 0xff</c> (a colour key).
/// </summary>
/// <returns>0, VFX_ERR_* for either pane, or VFX_ERR_CLIPPED when they don't overlap.</returns>
/// <remarks>MCX.EXE @ 0x0076f8ab. Rows and columns are walked in the order that survives overlap.</remarks>
int32_t VFX_pane_copy(PANE* source, int32_t sx, int32_t sy, PANE* target, int32_t tx, int32_t ty, int32_t fill);

/// <summary>
/// Scrolls the pane's contents by (dx, dy). <paramref name="mode"/> 0: a scroll by the pane's size or more wipes it
/// with <c>parm &amp; 0xff</c>; otherwise the image moves and the uncovered strips are filled with
/// <paramref name="parm"/> (VFX_pane_copy's fill: a colour 0..255). <paramref name="mode"/> 1 (wrap): the pixels
/// scrolled out come back on the other side.
/// </summary>
/// <returns>0, VFX_ERR_EMPTY_PANE, or in mode 1 with parm 0 the scratch size the asm needed (width * height).</returns>
/// <remarks>
/// MCX.EXE @ 0x0076fc93. In mode 1 the asm took a scratch buffer of width * height bytes as parm; the port allocates
/// its own and only tests parm against 0.
/// </remarks>
int32_t VFX_pane_scroll(PANE* pane, int32_t dx, int32_t dy, int32_t mode, int32_t parm);

/// <summary>
/// Draws the outline of the ellipse centred at (xc, yc) with radii <paramref name="width"/> and
/// <paramref name="height"/> (midpoint algorithm, each point clipped). A zero radius draws the line from
/// (xc - width, yc - height) to (xc + width, yc + height) instead and returns its result.
/// </summary>
/// <remarks>MCX.EXE @ 0x0076fe90</remarks>
int32_t VFX_ellipse_draw(PANE* pane, int32_t xc, int32_t yc, int32_t width, int32_t height, int32_t color);

/// <summary>
/// Fills the ellipse centred at (xc, yc) with radii <paramref name="width"/> and <paramref name="height"/>, one
/// horizontal span per step of the same midpoint walk as <c>VFX_ellipse_draw</c>; a zero radius draws a line.
/// </summary>
/// <remarks>MCX.EXE @ 0x007701d1</remarks>
int32_t VFX_ellipse_fill(PANE* pane, int32_t xc, int32_t yc, int32_t width, int32_t height, int32_t color);

// ---------------------------------------------------------------------------------------------------------------
// vfxa.asm: shapes (vfx/vfxa_shape.cpp)
// ---------------------------------------------------------------------------------------------------------------

/// <summary>
/// Draws shape <paramref name="shapeNum"/> of <paramref name="shapeTable"/> with its origin (hot spot) at
/// (hotX, hotY). Colour runs and literal pixels are written as stored; skipped pixels are left alone.
/// </summary>
/// <returns>0, or VFX_ERR_* (-3 wholly clipped, -4 empty bounds).</returns>
/// <remarks>MCX.EXE @ 0x0076d938 (the unclipped case goes through DrawShapeUnclipped @ 0x0076dd3c).</remarks>
int32_t VFX_shape_draw(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY);

/// <summary>Sets the 256-byte colour table <c>VFX_shape_translate_draw</c> maps every shape pixel through.</summary>
/// <remarks>MCX.EXE @ 0x0076de03 (copies the table into VFX's own).</remarks>
void VFX_shape_lookaside(uint8_t* table);

/// <summary>Draws a shape like <c>VFX_shape_draw</c>, mapping each pixel through the lookaside table.</summary>
/// <remarks>MCX.EXE @ 0x0076de22 (the unclipped case goes through XlatShapeUnclipped @ 0x0076e2fa).</remarks>
int32_t VFX_shape_translate_draw(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY);

/// <summary>
/// Draws a shape rotated by <paramref name="rot"/> (tenths of a degree) and scaled by x_scale/y_scale (16.16) about
/// its hot spot (clockwise on screen for positive angles). The shape is drawn upright into the work
/// <paramref name="buffer"/> (width * height bytes, cleared to 255 first), which is then texture-mapped onto the
/// transformed corners with 255 transparent. <paramref name="flags"/>: ST_XLAT maps pixels through the lookaside
/// table as they go into the buffer; ST_REUSE skips filling the buffer (it still holds this shape from a previous
/// call). With rot 0 and both scales 1.0 it is a plain VFX_shape_draw / VFX_shape_translate_draw.
/// </summary>
/// <returns>
/// The plain draw's result in the untransformed case, VFX_ERR_BAD_WINDOW / VFX_ERR_EMPTY_PANE, else 0 (the asm
/// returned whatever EAX held).
/// </returns>
/// <remarks>MCX.EXE @ 0x0076e42d</remarks>
int32_t VFX_shape_transform(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, void* buffer,
                            int32_t rot, int32_t x_scale, int32_t y_scale, uint32_t flags);

/// <summary><c>VFX_shape_transform</c> flag: map pixels through the shape lookaside table.</summary>
inline constexpr uint32_t ST_XLAT = 0x01;
/// <summary><c>VFX_shape_transform</c> flag: the work buffer already holds the shape; don't redraw it.</summary>
inline constexpr uint32_t ST_REUSE = 0x02;

/// <summary>
/// The window-space rectangle a shape covers when drawn at (hotX, hotY), mirrored per <paramref name="mirror"/>,
/// written to <paramref name="rectangle"/> as x0, y0, x1, y1.
/// </summary>
/// <remarks>
/// MCX.EXE @ 0x0076efd9. The right and bottom edges are exclusive on x (one past the last pixel) and inclusive on y;
/// a shape without rows gives (0, 0, 0, 0), one without pixels (INT32_MAX, INT32_MAX, INT32_MIN, INT32_MIN).
/// Bit 0 of <paramref name="mirror"/> mirrors about hotX, bit 1 about hotY.
/// </remarks>
int32_t VFX_shape_visible_rectangle(void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, int32_t mirror,
                                    int32_t* rectangle);

/// <summary>Maps every pixel of a shape through the lookaside table, in place.</summary>
/// <remarks>MCX.EXE @ 0x0076f3d4 (unnamed in the symbols; VFX's VFX_shape_remap_colors).</remarks>
int32_t VFX_shape_remap_colors(void* shapeTable, int32_t shapeNum);

/// <summary>
/// VFX's shape encoder: encodes the pane's pixels as one shape (its 0x18-byte header and rows, no table header) into
/// <paramref name="buffer"/>, pixels of <paramref name="transparentColor"/> becoming skips, with its origin at
/// (hotX, hotY). With a null buffer only the size is computed.
/// </summary>
/// <returns>The number of bytes written.</returns>
/// <remarks>
/// MCX.EXE @ 0x0076f151 (helpers ScanLine @ 0x0076f466, FlushPacket @ 0x0076f5ef). Named _asm in the port: the
/// game's own C++ VFX_shape_scan (encode_vfx.cpp) has the same name and parameters.
/// </remarks>
int32_t VFX_shape_scan_asm(PANE* pane, uint8_t transparentColor, int32_t hotX, int32_t hotY, void* buffer);

/// <summary>A shape's bounds word: its width in the high 16 bits and height in the low ones.</summary>
/// <remarks>MCX.EXE @ 0x00771eeb (the shape header's first dword).</remarks>
int32_t VFX_shape_bounds(void* shapeTable, int32_t shapeNum);

/// <summary>A shape's origin word: the hot spot's x in the high 16 bits and y in the low ones.</summary>
/// <remarks>MCX.EXE @ 0x00771f0d (the shape header's second dword).</remarks>
int32_t VFX_shape_origin(void* shapeTable, int32_t shapeNum);

/// <summary>A shape's size: <c>(x_max - x_min + 1) &lt;&lt; 16 | (y_max - y_min + 1)</c> (low word only).</summary>
/// <remarks>MCX.EXE @ 0x00771f30</remarks>
int32_t VFX_shape_resolution(void* shapeTable, int32_t shapeNum);

/// <summary>A shape's top-left offset from its hot spot: <c>x_min &lt;&lt; 16 | (uint16_t)y_min</c>.</summary>
/// <remarks>MCX.EXE @ 0x00771f64</remarks>
int32_t VFX_shape_minxy(void* shapeTable, int32_t shapeNum);

/// <summary>Writes a shape's palette entries (if it has any) into <paramref name="palette"/> at their indices.</summary>
/// <remarks>MCX.EXE @ 0x00771f8e</remarks>
void VFX_shape_palette(void* shapeTable, int32_t shapeNum, VFX_RGB* palette);

/// <summary>Copies a shape's palette entries to <paramref name="colors"/> (when not null).</summary>
/// <returns>The number of entries (0 without a palette).</returns>
/// <remarks>MCX.EXE @ 0x00771fd9</remarks>
int32_t VFX_shape_colors(void* shapeTable, int32_t shapeNum, VFX_CRGB* colors);

/// <summary>Replaces a shape's palette entries with <paramref name="colors"/> (when not null).</summary>
/// <returns>The number of entries (0 without a palette).</returns>
/// <remarks>MCX.EXE @ 0x00772021</remarks>
int32_t VFX_shape_set_colors(void* shapeTable, int32_t shapeNum, VFX_CRGB* colors);

/// <summary>The number of shapes in a table.</summary>
/// <remarks>MCX.EXE @ 0x0077206b</remarks>
int32_t VFX_shape_count(void* shapeTable);

/// <summary>
/// Lists the shapes with distinct data: writes to <paramref name="indexList"/> (when not null) the number of every
/// shape whose data offset no earlier shape shares.
/// </summary>
/// <returns>The number of distinct shapes.</returns>
/// <remarks>MCX.EXE @ 0x0077207e</remarks>
int32_t VFX_shape_list(void* shapeTable, uint32_t* indexList);

/// <summary>Lists the shapes with distinct palettes, as <c>VFX_shape_list</c> does for data.</summary>
/// <remarks>MCX.EXE @ 0x007720e0</remarks>
int32_t VFX_shape_palette_list(void* shapeTable, uint32_t* indexList);

// ---------------------------------------------------------------------------------------------------------------
// vfxa.asm: fixed-point maths (vfx/vfxa_math.cpp)
// ---------------------------------------------------------------------------------------------------------------

/// <summary>The cosine and sine of <paramref name="angle"/> (tenths of a degree), as 16.16 fixed point.</summary>
/// <remarks>MCX.EXE @ 0x007712e3 (a quarter-wave table at 0x007704cf).</remarks>
void VFX_Cos_Sin(int32_t angle, FIXED16* cosine, FIXED16* sine);

/// <summary>The 16.16 product of <paramref name="m1"/> and <paramref name="m2"/>, also stored to <paramref name="result"/>.</summary>
/// <remarks>MCX.EXE @ 0x0077139b</remarks>
FIXED16 VFX_fixed_mul(FIXED16 m1, FIXED16 m2, FIXED16* result);

/// <summary>
/// Rotates <paramref name="in"/> by <paramref name="rot"/> (tenths of a degree) about <paramref name="origin"/> and
/// scales it by x_scale/y_scale (16.16), into <paramref name="out"/>.
/// </summary>
/// <remarks>MCX.EXE @ 0x007713c1</remarks>
void VFX_point_transform(VFX_POINT* in, VFX_POINT* out, VFX_POINT* origin, int32_t rot, int32_t x_scale,
                         int32_t y_scale);

// ---------------------------------------------------------------------------------------------------------------
// vfxa.asm: fonts (vfx/vfxa_font.cpp)
// ---------------------------------------------------------------------------------------------------------------

// A VFX font (the game's data\fonts\*.fnt) is a VFX_FONT header, then at +0x10 one dword per character: the offset
// of its glyph from the start of the font. A glyph is a dword width followed by width * char_height pixel bytes, row
// by row, top row first.

/// <summary>The height of a font's characters in pixels (the header's char_height).</summary>
/// <remarks>MCX.EXE @ 0x00771488</remarks>
int32_t VFX_font_height(void* font);

/// <summary>The width of <paramref name="character"/> in pixels (its glyph's first dword).</summary>
/// <remarks>MCX.EXE @ 0x0077149b</remarks>
int32_t VFX_character_width(void* font, int32_t character);

/// <summary>
/// Draws <paramref name="character"/> with its top-left corner at (x, y), clipped to the pane. Without a
/// <paramref name="colorTranslate"/> table every glyph byte is copied (the background too); with one, each byte is
/// mapped through the table and the pixels it maps to 255 are left alone (transparent).
/// </summary>
/// <returns>The character's width, whether or not any of it was drawn; VFX_ERR_* for a bad or empty pane.</returns>
/// <remarks>MCX.EXE @ 0x007714bb</remarks>
int32_t VFX_character_draw(PANE* pane, int32_t x, int32_t y, void* font, int32_t character, uint8_t* colorTranslate);

/// <summary>
/// Draws a zero-terminated string from (x, y), each character advancing x by <c>VFX_character_draw</c>'s result.
/// The first character is drawn before the terminator is checked (an empty string draws character 0).
/// </summary>
/// <remarks>MCX.EXE @ 0x0077164e</remarks>
void VFX_string_draw(PANE* pane, int32_t x, int32_t y, void* font, const char* string, uint8_t* colorTranslate);

// ---------------------------------------------------------------------------------------------------------------
// vfxa.asm: image files, fades, colour scans (vfx/vfxa_image.cpp)
// ---------------------------------------------------------------------------------------------------------------

/// <summary>
/// Writes <paramref name="width"/> pixels of <paramref name="line"/> to pane row <paramref name="y"/> from the pane's
/// left edge, clipped to the pane (rows outside it are dropped).
/// </summary>
/// <remarks>MCX.EXE @ 0x00771685 (the image decoders' output routine).</remarks>
void VFX_line_to_pane(PANE* pane, int32_t y, uint8_t* line, int32_t width);

/// <summary>
/// Draws an IFF picture into the pane from its top-left corner: FORM ILBM (8 bitplanes) or, for any other form
/// type, PBM (chunky); BODY uncompressed or ByteRun1. Pictures with a mask plane (masking 1) aren't drawn.
/// </summary>
/// <returns>The low byte of BMHD's transparent colour (0 for a masked picture, which the asm left undefined).</returns>
/// <remarks>MCX.EXE @ 0x007717d6</remarks>
int32_t VFX_ILBM_draw(PANE* pane, uint8_t* ilbm);

/// <summary>Copies an ILBM's CMAP (256 entries) into <paramref name="palette"/>, scaled down to 6 bits.</summary>
/// <remarks>MCX.EXE @ 0x0077199d</remarks>
void VFX_ILBM_palette(uint8_t* ilbm, VFX_RGB* palette);

/// <summary>An ILBM's size from BMHD: width in the high 16 bits, height in the low ones.</summary>
/// <remarks>MCX.EXE @ 0x007719ce</remarks>
int32_t VFX_ILBM_resolution(uint8_t* ilbm);

/// <summary>Draws an 8-bit, single-plane, RLE PCX picture into the pane from its top-left corner.</summary>
/// <returns>0.</returns>
/// <remarks>MCX.EXE @ 0x007719fb</remarks>
int32_t VFX_PCX_draw(PANE* pane, uint8_t* pcx);

/// <summary>Copies a PCX file's 256-colour palette (its last 768 bytes) into <paramref name="palette"/>, scaled to 6 bits.</summary>
/// <remarks>MCX.EXE @ 0x00771a7d</remarks>
void VFX_PCX_palette(uint8_t* pcx, int32_t fileSize, VFX_RGB* palette);

/// <summary>A PCX picture's size: width in the high 16 bits, height in the low ones.</summary>
/// <remarks>MCX.EXE @ 0x00771aa8 (unnamed in the symbols; VFX's VFX_PCX_resolution).</remarks>
int32_t VFX_PCX_resolution(uint8_t* pcx);

/// <summary>The size of the work buffer <c>VFX_GIF_draw</c> needs (the game allocates 0x502e bytes).</summary>
inline constexpr int32_t VFX_GIF_BUFFER_SIZE = 0x502e;

/// <summary>
/// Draws the first image of a GIF into the pane from its top-left corner, using <paramref name="buffer"/>
/// (<see cref="VFX_GIF_BUFFER_SIZE"/> bytes) for the LZW state. Interlaced images are drawn in their pass order.
/// Extension blocks before the image aren't skipped (the game's GIFs have none).
/// </summary>
/// <returns>The GIF's background colour index.</returns>
/// <remarks>MCX.EXE @ 0x00771c39</remarks>
int32_t VFX_GIF_draw(PANE* pane, uint8_t* gif, void* buffer);

/// <summary>
/// Copies a GIF's global colour table and then its first image's local colour table (each if present) into
/// <paramref name="palette"/>, scaled down to 6 bits.
/// </summary>
/// <remarks>MCX.EXE @ 0x00771e52</remarks>
void VFX_GIF_palette(uint8_t* gif, VFX_RGB* palette);

/// <summary>A GIF's first image's size: width in the high 16 bits, height in the low ones.</summary>
/// <remarks>MCX.EXE @ 0x00771eb3</remarks>
int32_t VFX_GIF_resolution(uint8_t* gif);

/// <summary>
/// The display palette ("DAC") VFX_window_fade reads and writes, 6-bit entries. Port: the asm went through the
/// registered driver's DAC read/write entries (0x007a80ec/0x007a80f0); the port keeps the palette here and tells
/// the platform layer through <see cref="VFXDacWriteHook"/>.
/// </summary>
extern VFX_RGB VFXDacPalette[256];
/// <summary>Called after each <see cref="VFXDacPalette"/> entry changes (null: nothing is told).</summary>
extern void (*VFXDacWriteHook)(int32_t index, const VFX_RGB* rgb);
/// <summary>Called once per vertical retrace wait of a fade (the driver's entry at 0x007a80e0; null: no wait).</summary>
extern void (*VFXWaitRetraceHook)();

/// <summary>
/// Fades the display palette entries of the colours the window uses towards <paramref name="palette"/>, one DAC
/// step per channel at a time, over about <paramref name="intervals"/> vertical retraces.
/// </summary>
/// <remarks>MCX.EXE @ 0x00772435</remarks>
void VFX_window_fade(WINDOW* window, VFX_RGB* palette, int32_t intervals);

/// <summary>
/// Lists in <paramref name="colors"/> (when not null) every distinct colour index in the pane, in the order first met
/// scanning rows top to bottom and each row right to left. The pane isn't clipped to its window.
/// </summary>
/// <returns>The number of distinct colours.</returns>
/// <remarks>MCX.EXE @ 0x007725b9</remarks>
int32_t VFX_color_scan(PANE* pane, uint32_t* colors);

// ---------------------------------------------------------------------------------------------------------------
// vfx3d.asm: polygons (vfx/vfx3d.cpp)
// ---------------------------------------------------------------------------------------------------------------

// The polygon routines take vertices relative to the pane's corner clipped to the window (max(0, x0), max(0, y0)),
// which is the pane's own corner for any pane inside its window. Polygons must be convex; rows are filled from the
// top vertex to the bottom one inclusive, each span from its left to its right edge inclusive (edges at x + 0.5
// rounded down).

/// <summary>Fills a convex polygon with the colour of its first vertex (<c>c</c> rounded, 16.16).</summary>
/// <remarks>MCX.EXE @ 0x00772648</remarks>
void VFX_flat_polygon(PANE* pane, int32_t vcnt, SCRNVERTEX* vlist);

/// <summary>Fills a convex polygon interpolating the vertices' colours (<c>c</c>, 16.16).</summary>
/// <remarks>MCX.EXE @ 0x00772b0e (unnamed in the symbols; the game's PolygonElement calls it for shaded polygons).</remarks>
void VFX_Gouraud_polygon(PANE* pane, int32_t vcnt, SCRNVERTEX* vlist);

/// <summary>
/// A Gouraud polygon with <paramref name="ditherAmount"/> (16.16) added to the colour of every other pixel, in a
/// checkerboard, before it is rounded down.
/// </summary>
/// <remarks>MCX.EXE @ 0x00773372</remarks>
void VFX_dithered_Gouraud_polygon(PANE* pane, FIXED16 ditherAmount, int32_t vcnt, SCRNVERTEX* vlist);

/// <summary>Maps every pixel under a convex polygon through the 256-byte table <paramref name="lookaside"/>.</summary>
/// <remarks>MCX.EXE @ 0x00773c30</remarks>
void VFX_translate_polygon(PANE* pane, int32_t vcnt, SCRNVERTEX* vlist, void* lookaside);

/// <summary>
/// Adds the interpolated, dithered vertex colours (as <c>VFX_dithered_Gouraud_polygon</c> computes them) to the pixels
/// under the polygon, as byte offsets into a shading palette.
/// </summary>
/// <remarks>MCX.EXE @ 0x0077419e (unnamed in the symbols).</remarks>
void VFX_illuminate_polygon(PANE* pane, FIXED16 ditherAmount, int32_t vcnt, SCRNVERTEX* vlist);

/// <summary>Sets the 256-byte table <c>VFX_map_polygon</c> maps texels through with MP_XLAT.</summary>
/// <remarks>MCX.EXE @ 0x00774a68 (copies the table into VFX's own, at 0x007a9b8c).</remarks>
void VFX_map_lookaside(uint8_t* table);

/// <summary><c>VFX_map_polygon</c> flag: map texels through the map lookaside table.</summary>
inline constexpr uint32_t MP_XLAT = 0x01;
/// <summary>
/// <c>VFX_map_polygon</c> flag: texels of colour 255 (after the lookaside, with MP_XLAT) are transparent.
/// </summary>
inline constexpr uint32_t MP_XP = 0x02;

/// <summary>
/// Texture-maps <paramref name="texture"/> onto a convex polygon, affinely per the vertices' u, v (16.16 texel
/// coordinates). <paramref name="flags"/> is a combination of MP_XLAT and MP_XP (the asm indexes a jump table with
/// it; only 0..3 exist).
/// </summary>
/// <remarks>MCX.EXE @ 0x00774a88</remarks>
void VFX_map_polygon(PANE* pane, int32_t vcnt, SCRNVERTEX* vlist, WINDOW* texture, uint32_t flags);

// ---------------------------------------------------------------------------------------------------------------
// vfx\alphapalette.cpp
// ---------------------------------------------------------------------------------------------------------------

/// <summary>The number of alpha "colours": the 256 palette indices plus 24 extra blend-only ones (256..279).</summary>
inline constexpr int32_t ALPHA_COLORS = 0x118;

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
extern char AlphaTable[ALPHA_COLORS * 256];
/// <summary>1 for every alpha colour AlphaPal.ini defines (a translucent colour), else 0. 0x008021d0 in MCX.EXE.</summary>
extern char SpecialColor[ALPHA_COLORS];

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
extern MCAlphaColor MCAlphaColors[ALPHA_COLORS];

/// <summary>
/// Builds <see cref="AlphaTable"/> and <see cref="SpecialColor"/> for <paramref name="palette"/> from
/// <c>AlphaPal.ini</c>: lines of <c>index R G B A B2</c>; a special colour blended over background (r, g, b) (8-bit)
/// gives <c>background * B2 + RGB * A</c>, or, with A and B2 both 0, <c>background * 255 / (255 - RGB)</c> (a
/// colour dodge). Backgrounds outside 10..245 (the reserved system colours) blend to 255.
/// </summary>
/// <remarks>MCX.EXE @ 0x006b51b0</remarks>
void InitAlphaLookup(VFX_RGB* palette);

/// <summary>
/// The palette index 10..245 nearest to (r, g, b) (6-bit components), by distance weighted 39:51:10 (R:G:B); the first
/// exact match wins.
/// </summary>
/// <remarks>MCX.EXE @ 0x006b55d0</remarks>
uint8_t FindClosest(VFX_RGB* palette, int r, int g, int b);

/// <summary>
/// Writes an 8-bit image as a 24-bit top-down TGA through the game palette (<c>gamePalette</c>), a debugging aid.
/// </summary>
/// <remarks>MCX.EXE @ 0x006b50d0 (unassigned in the line tables; it sits just before InitAlphaLookup).</remarks>
void writeTGA(char* fileName, uint8_t* image, uint32_t width, uint32_t height);

// ---------------------------------------------------------------------------------------------------------------
// vfx\encode_vfx.cpp
// ---------------------------------------------------------------------------------------------------------------

/// <summary>
/// The game's copy of VFX's shape encoder (the asm transcribed into inline assembly with its state in globals):
/// encodes the pane's pixels as one shape (header and rows, no table header) into <paramref name="buffer"/>,
/// <paramref name="transparentColor"/> becoming skips, with its origin at (hotX, hotY). Its output is byte for byte
/// that of <see cref="VFX_shape_scan_asm"/>.
/// </summary>
/// <returns>The number of bytes written (or VFX_ERR_BAD_WINDOW / VFX_ERR_EMPTY_PANE).</returns>
/// <remarks>MCX.EXE @ 0x006b5670 (ScanLine @ 0x006b5973, FlushPacket @ 0x006b5bdb, the shared epilogue @ 0x006b5db8).</remarks>
int VFX_shape_scan(PANE* pane, uint8_t transparentColor, int hotX, int hotY, void* buffer);

// ---------------------------------------------------------------------------------------------------------------
// vfx\fastshp.cpp
// ---------------------------------------------------------------------------------------------------------------

/// <summary>
/// Draws shape <paramref name="shapeNum"/> of a "fast shape" table (the terrain overlay tiles, tables tagged "DNAH";
/// not the VFX format), its hot spot at (hotX, hotY), each colour mapped through
/// <paramref name="xlat"/> when it isn't null. Run colour 255 is transparent; a shape whose first row begins with the
/// word 1 is drawn translucent through <see cref="AlphaTable"/>.
/// </summary>
/// <param name="unused">Always 0 from the game; not read.</param>
/// <returns>0.</returns>
/// <remarks>MCX.EXE @ 0x006b5dd0 (inline assembly).</remarks>
int32_t fastShapeDraw(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, uint8_t* xlat,
                      int unused);

// ---------------------------------------------------------------------------------------------------------------
// vfx\vfx_ellipse.cpp
// ---------------------------------------------------------------------------------------------------------------

/// <summary>
/// The game's ellipse outline (midpoint algorithm) centred at (xc, yc) with radii <paramref name="width"/> and
/// <paramref name="height"/>, clipped to the pane. A special (translucent) <paramref name="color"/> blends through
/// <see cref="AlphaTable"/>. A zero radius draws the line from (xc - width, yc - height) to (xc + width, yc + height).
/// </summary>
/// <remarks>
/// MCX.EXE @ 0x006b6580 (plotters at 0x006b6768 and 0x006b683a). Unlike VFX, the centre is offset by the pane's
/// origin clipped to the window.
/// </remarks>
void AG_ellipse_draw(PANE* pane, int32_t xc, int32_t yc, int32_t width, int32_t height, int32_t color);

/// <summary>The game's filled ellipse, as <see cref="AG_ellipse_draw"/> with horizontal spans.</summary>
/// <remarks>MCX.EXE @ 0x006b6970 (span fillers at 0x006b6b5a and 0x006b6c07).</remarks>
void AG_ellipse_fill(PANE* pane, int32_t xc, int32_t yc, int32_t width, int32_t height, int32_t color);

// ---------------------------------------------------------------------------------------------------------------
// vfx\vfx_map_polygon.cpp
// ---------------------------------------------------------------------------------------------------------------

/// <summary>
/// Draws a translucent status bar in the box (x0, y0)-(x1, y1), in window coordinates (the pane only clips): the
/// top and bottom rows and the left and right columns are darkened through AlphaTable row 0x108, and the first
/// <paramref name="barLength"/> + 1 pixels inside each other row are blended with alpha colour
/// <paramref name="alphaColor"/>.
/// </summary>
/// <remarks>MCX.EXE @ 0x006b6d00</remarks>
void AG_StatusBar(PANE* pane, int x0, int y0, int x1, int y1, int alphaColor, int barLength);

/// <summary>
/// Writes a pixel (the low byte of <paramref name="color"/>) at pane coordinates (x, y), only strictly inside the
/// pane's rectangle (its border rows and columns are never written; the window isn't checked).
/// </summary>
/// <remarks>MCX.EXE @ 0x006b6ed0</remarks>
void AG_pixel_write(PANE* pane, int32_t x, int32_t y, uint32_t color);

/// <summary>
/// Copies the top-left <paramref name="width"/> x <paramref name="height"/> pixels of <paramref name="texture"/> to
/// (x, y) in the pane, skipping texels of colour 255, clipped to the pane.
/// </summary>
/// <returns>0 when anything was drawn, 1 when it was wholly clipped.</returns>
/// <remarks>MCX.EXE @ 0x006b6f20 (MMX and 32-bit paths chosen by <c>Processor</c>; both copy the same pixels).</remarks>
int32_t DrawTransparent(PANE* pane, WINDOW* texture, int x, int y, int width, int height);

// ---------------------------------------------------------------------------------------------------------------
// vfx\vfx_transform.cpp
// ---------------------------------------------------------------------------------------------------------------

/// <summary>
/// The table AG_shape_translate_draw and AG_shape_translate_fill map pixels through, set by
/// <see cref="AG_shape_lookaside"/> (the caller's table is used in place, not copied).
/// </summary>
/// <remarks>MCX.EXE @ 0x008024dc (declared <c>unsigned int</c> in the original; a pointer in the port).</remarks>
extern uint8_t* lookaside;

/// <summary>
/// Draws a shape blended onto the pane through <see cref="AlphaTable"/>, at full size or half size (every other
/// pixel of every other row), mirrored left to right when <paramref name="mirror"/> is nonzero: the shape is first
/// rendered opaque into <paramref name="buffer"/> by <see cref="AG_shape_fill"/> (skips as colour 0), then blended
/// by <see cref="CopySprite"/>. (hotX, hotY) are relative to the pane's corner clipped to the window.
/// Shapes of 0x1fa40 (360 x 360) pixels or more are not drawn.
/// </summary>
/// <remarks>MCX.EXE @ 0x006b7220 (unnamed in the symbols; named after its translating twin).</remarks>
void AG_shape_transform(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, void* buffer,
                        int32_t mirror, int32_t fullSize);

/// <summary>As <see cref="AG_shape_transform"/>, rendering the shape through <see cref="lookaside"/> first.</summary>
/// <remarks>MCX.EXE @ 0x006b73f0</remarks>
void AG_shape_translate_transform(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY,
                                  void* buffer, int32_t mirror, int32_t fullSize);

/// <summary>
/// Blends a <paramref name="width"/> x <paramref name="height"/> 8-bit <paramref name="sprite"/> onto the pane at
/// (x, y) relative to the pane's corner clipped to the window: each pixel becomes
/// <c>AlphaTable[sprite &lt;&lt; 8 | screen]</c>. <paramref name="fullSize"/> 0 halves it (every other pixel of every
/// other row); <paramref name="mirror"/> nonzero mirrors it left to right. Clipped to the pane.
/// </summary>
/// <remarks>MCX.EXE @ 0x006b75c0</remarks>
void CopySprite(PANE* pane, uint8_t* sprite, int x, int y, int width, int height, int mirror, int fullSize);

/// <summary>
/// Port: <see cref="CopySprite(PANE*, uint8_t*, int, int, int, int, int, int)"/> of a window's pixels, which the
/// renderers read through its texture (the sprite is the whole window, <paramref name="width"/> its width).
/// </summary>
void CopySprite(PANE* pane, WINDOW* sprite, int x, int y, int width, int height, int mirror, int fullSize);

/// <summary>
/// Draws a shape opaque, its skipped pixels written as colour 0, with its hot spot at window coordinates
/// (hotX, hotY) (the pane only clips): renders a shape into a scratch bitmap.
/// </summary>
/// <remarks>MCX.EXE @ 0x006b7930</remarks>
void AG_shape_fill(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY);

/// <summary>As <see cref="AG_shape_fill"/>, each pixel mapped through <see cref="lookaside"/> (skips stay 0).</summary>
/// <remarks>MCX.EXE @ 0x006b7be0. When clipped, the pane's right column itself is not written.</remarks>
void AG_shape_translate_fill(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY);

// ---------------------------------------------------------------------------------------------------------------
// vfx\vfx_translatedraw.cpp
// ---------------------------------------------------------------------------------------------------------------

/// <summary>
/// The game's shape draw: <c>VFX_shape_draw</c>'s format and pixels, but with the hot spot at window coordinates
/// (hotX, hotY) (the pane only clips) and translucent shapes: a shape whose data begins with the token pair 03 00 is
/// blended, each pixel becoming <c>AlphaTable[shape &lt;&lt; 8 | screen]</c>, one column to the right.
/// </summary>
/// <remarks>MCX.EXE @ 0x006b7f00 (inline assembly)</remarks>
void AG_shape_draw(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY);

/// <summary>Sets <see cref="lookaside"/>, the table AG_shape_translate_draw and AG_shape_translate_fill use.</summary>
/// <remarks>MCX.EXE @ 0x006b83c0</remarks>
void AG_shape_lookaside(uint8_t* table);

/// <summary>
/// As <see cref="AG_shape_draw"/>, each pixel mapped through <see cref="lookaside"/> (translucent shapes blended
/// first, then mapped).
/// </summary>
/// <remarks>MCX.EXE @ 0x006b83d0 (inline assembly)</remarks>
void AG_shape_translate_draw(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY);

// ---------------------------------------------------------------------------------------------------------------
// vfx\vfxtile.cpp
// ---------------------------------------------------------------------------------------------------------------

/// <summary>The <c>xlat</c> of <see cref="VFX_nTile_draw"/> that fills the tile with colour 0x10 (the original's -1).</summary>
inline uint8_t* const VFX_TILE_FILL = reinterpret_cast<uint8_t*>(static_cast<intptr_t>(-1));

/// <summary>
/// Draws a terrain tile (format in vfx/vfxtile.cpp) with its hot spot at (x, y) relative to the pane's corner
/// clipped to the window, clipped to the pane: copied when <paramref name="xlat"/> is null, filled with colour 0x10
/// when it is <see cref="VFX_TILE_FILL"/>, else mapped through the 256-byte table it points to (a haze palette).
/// </summary>
/// <returns>0, or 0xcdcf0001 when the tile is wholly outside the pane.</returns>
/// <remarks>MCX.EXE @ 0x006b89b0 (<c>_VFX_nTile_draw</c>, a C function despite the asm-style name)</remarks>
int32_t VFX_nTile_draw(PANE* pane, uint8_t* tile, int32_t x, int32_t y, uint8_t* xlat);
