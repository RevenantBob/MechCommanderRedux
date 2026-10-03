#pragma once

// The port's renderer interface. The game's VFX/AG routines (vfx/*.cpp) are its front end: they keep their
// signatures, clip each draw to its pane within the window, and hand the renderer commands that are already
// resolved. The renderer walks shape, tile, glyph and polygon data and writes palette
// indices. MCSoftwareRenderer is the original pixel code; a hardware renderer implements the same commands.
//
// Targets: a WINDOW is a picture in memory (window->buffer), as in the original, or a view (window->View): a UI
// element's place on another window, with an origin and a scissor, drawn into only while the element draws in the
// frame pass. The world view has a surface of its own. Nothing is locked, read back or re-uploaded each frame.
//
// Coordinates in commands are window coordinates, rectangles inclusive. Tables are passed as pointers and read at
// the time of the call.

#include "vfx/vfx.h"

/// <summary>An inclusive rectangle in window coordinates.</summary>
struct MCRect
{
    int32_t X0;
    int32_t Y0;
    int32_t X1;
    int32_t Y1;
};

/// <summary>
/// A view: the window that points at it has no pixels; what is drawn into it lands on <c>Target</c>, its pixel
/// (0, 0) at (<c>OriginX</c>, <c>OriginY</c>), cut to <c>Scissor</c>. The vfx front end clips each draw to the window
/// and then to the scissor (moved into the window's coordinates), and <see cref="MCRenderer::For"/> hands the view's
/// commands on to the target's renderer, moved by the origin. A UI element owns its view and opens the scissor only
/// while it draws in the frame pass, so a draw at any other time (painting on an event) does nothing.
/// </summary>
struct MCView
{
    /// <summary>The window drawn on (a picture, not another view).</summary>
    _window* Target = nullptr;
    /// <summary>Where the view's pixel (0, 0) lies on the target.</summary>
    int32_t OriginX = 0;
    int32_t OriginY = 0;
    /// <summary>The part of the target the view may draw on, inclusive; empty (X1 &lt; X0) draws nothing.</summary>
    MCRect Scissor{0, 0, -1, -1};
    /// <summary>
    /// Whether writes of colour 0xff draw nothing: the element was a picture copied to the screen with 0xff as a
    /// colour key (<c>aObject::transparent</c>), so what it painted in 0xff never reached the screen.
    /// </summary>
    bool KeyTransparent = false;

    /// <summary>Whether the scissor is open.</summary>
    bool Open() const { return Scissor.X0 <= Scissor.X1 && Scissor.Y0 <= Scissor.Y1; }
};

/// <summary>
/// Narrows a clip rectangle in <paramref name="window"/>'s coordinates to its view's scissor (nothing happens for a
/// window with pixels of its own). The vfx front end calls it wherever it clips a draw to its pane and window.
/// </summary>
inline void MCClipToView(const _window* window, int32_t& x0, int32_t& y0, int32_t& x1, int32_t& y1)
{
    const MCView* view = window->View;

    if (view == nullptr)
    {
        return;
    }

    x0 = std::max(x0, view->Scissor.X0 - view->OriginX);
    y0 = std::max(y0, view->Scissor.Y0 - view->OriginY);
    x1 = std::min(x1, view->Scissor.X1 - view->OriginX);
    y1 = std::min(y1, view->Scissor.Y1 - view->OriginY);
}

/// <summary>What a run-length shape draw does with each pixel it writes.</summary>
enum class MCShapeOp
{
    /// <summary>Runs and literals are written as stored.</summary>
    Draw,
    /// <summary>Each pixel becomes <c>AlphaTable[shape &lt;&lt; 8 | screen]</c>.</summary>
    Alpha,
    /// <summary>Each pixel is mapped through the table.</summary>
    Xlat,
    /// <summary>Blended as Alpha, then mapped through the table.</summary>
    XlatAlpha,
    /// <summary>As Draw, and skipped pixels are written as colour 0.</summary>
    Fill,
    /// <summary>As Xlat, and skipped pixels are written as colour 0.</summary>
    XlatFill
};

/// <summary>
/// A run-length shape (VFX's format: 0x18-byte header, then rows of tokens) with its clipping resolved: rows
/// <c>SkipRows</c>.. of the shape are drawn from window row <c>Top</c>, each starting at window column <c>Left</c>,
/// writing only columns <c>Lo</c>..<c>Hi</c>.
/// </summary>
struct MCShapeCommand
{
    /// <summary>The shape table and the shape's number in it.</summary>
    const void* ShapeTable;
    int32_t ShapeNum;
    /// <summary>The encoded rows stepped over before the first drawn, and how many are drawn.</summary>
    int32_t SkipRows;
    int32_t Rows;
    /// <summary>The window row of the first drawn row, and the window column every row starts at.</summary>
    int32_t Top;
    int32_t Left;
    /// <summary>The columns written, inclusive.</summary>
    int32_t Lo;
    int32_t Hi;
    MCShapeOp Op;
    /// <summary>The 256-byte table of the Xlat ops (null otherwise).</summary>
    const uint8_t* Table;
};

/// <summary>
/// A "fast shape" (fastShapeDraw's DNAH format, the terrain overlay tiles) with its clipping resolved: shape rows
/// <c>FirstRow</c>..<c>EndRow</c> - 1 from window row <c>Top</c>, each starting at column <c>StartX</c>, or, when
/// <c>LeftSkip</c> is set, with that many pixels stepped over and the rest drawn from <c>ClipX0</c>; <c>Limit</c>
/// pixels of each row are covered.
/// </summary>
struct MCFastShapeCommand
{
    /// <summary>The shape (its header, at the offset the table gives).</summary>
    const uint8_t* Shape;
    int32_t Top;
    int32_t FirstRow;
    int32_t EndRow;
    int32_t StartX;
    int32_t ClipX0;
    int32_t LeftSkip;
    int32_t Limit;
    /// <summary>Whether the shape is translucent (its first row starts with the word 1).</summary>
    bool Alpha;
    /// <summary>The colour table, or null.</summary>
    const uint8_t* Table;
};

/// <summary>A terrain tile (VFX_nTile_draw) with its clipping resolved.</summary>
struct MCTileCommand
{
    /// <summary>The tile's data (format in vfx/vfxtile.cpp).</summary>
    const uint8_t* Tile;
    /// <summary>The window column of the tile's bounding box, and the window row of the first row drawn.</summary>
    int32_t Left;
    int32_t Top;
    /// <summary>The first tile row drawn and one past the last.</summary>
    int32_t FirstRow;
    int32_t EndRow;
    /// <summary>The columns written, inclusive; ignored when <c>Unclipped</c>.</summary>
    int32_t Lo;
    int32_t Hi;
    /// <summary>Whether the tile lies wholly inside horizontally (its spans are written without clipping).</summary>
    bool Unclipped;
    /// <summary>Null: copied; VFX_TILE_FILL: colour 0x10; else a 256-byte table.</summary>
    const uint8_t* Table;
};

/// <summary>Which vfx3d filler a <see cref="MCPolygonCommand"/> reproduces.</summary>
enum class MCPolygonKind
{
    Flat,
    Gouraud,
    DitheredGouraud,
    Translate,
    Illuminate,
    Map
};

/// <summary>
/// A convex polygon (vfx3d.cpp). Its vertices are relative to the clipped pane's corner (<c>OriginX</c>,
/// <c>OriginY</c>), and it is clipped to 0..<c>XMax</c>, 0..<c>YMax</c> from there.
/// </summary>
struct MCPolygonCommand
{
    MCPolygonKind Kind;
    int32_t OriginX;
    int32_t OriginY;
    int32_t XMax;
    int32_t YMax;
    int32_t VertexCount;
    const SCRNVERTEX* Vertices;
    /// <summary>The dithered kinds' amount (16.16).</summary>
    int32_t DitherAmount;
    /// <summary>Translate: the destination table. Map: the lookaside table MP_XLAT maps texels through.</summary>
    const uint8_t* Table;
    /// <summary>Map: the texture and the MP_* flags.</summary>
    const _window* Texture;
    uint32_t MapFlags;
};

/// <summary>One corner of a <see cref="MCMapQuadCommand"/>: window position and texel.</summary>
struct MCMapQuadVertex
{
    int32_t X;
    int32_t Y;
    int32_t U;
    int32_t V;
};

/// <summary>
/// VFX_shape_transform's textured quadrilateral: the corners clockwise from the shape's top left, clipped to
/// <c>Clip</c>; texels of colour 255 are transparent.
/// </summary>
struct MCMapQuadCommand
{
    MCMapQuadVertex Corners[4];
    MCRect Clip;
    const _window* Texture;
};

/// <summary>
/// A line with VFX_line_draw's clipping resolved into a walk: <c>Count</c> pixels from (X, Y). After each pixel the
/// 0.32 fraction gains <c>Slope</c>; on a carry the position also moves by (MinorX, MinorY); then it moves by
/// (MajorX, MajorY).
/// </summary>
struct MCLineCommand
{
    int32_t X;
    int32_t Y;
    int32_t Count;
    int32_t MajorX;
    int32_t MajorY;
    int32_t MinorX;
    int32_t MinorY;
    uint32_t Slope;
    uint32_t Fraction;
    /// <summary>Null: every pixel becomes <c>Color</c>; else each is mapped through the table (LD_TRANSLATE).</summary>
    const uint8_t* Table;
    uint8_t Color;
};

/// <summary>
/// An ellipse (VFX's midpoint walk, which the game's routines share) centred at window (<c>CenterX</c>,
/// <c>CenterY</c>) with radii <c>Width</c> and <c>Height</c> (both non-zero), each point or span clipped to
/// <c>Clip</c>. Points on the axes are plotted twice and filled rows filled again at every step that stays on them,
/// which matters when the colour blends.
/// </summary>
struct MCEllipseCommand
{
    int32_t CenterX;
    int32_t CenterY;
    int32_t Width;
    int32_t Height;
    MCRect Clip;
    /// <summary>Whether the ellipse is filled (two spans per step) or outlined (four points per step).</summary>
    bool Fill;
    uint8_t Color;
    /// <summary>Whether the colour blends through its AlphaTable row (the game's routines, for special colours).</summary>
    bool Alpha;
};

/// <summary>
/// A copy of <c>SourceRect</c> of <c>Source</c> to (X, Y) of the target. Rows are copied downwards or upwards and
/// columns rightwards or leftwards as the routine chose, so an overlapping copy within one window comes out as it did.
/// </summary>
struct MCCopyCommand
{
    const _window* Source;
    MCRect SourceRect;
    int32_t X;
    int32_t Y;
    /// <summary>Whether source pixels of colour <c>Key</c> are left out.</summary>
    bool ColorKey;
    uint8_t Key;
    bool Downwards;
    bool Rightwards;
};

/// <summary>AG_StatusBar with its clamping done.</summary>
struct MCStatusBarCommand
{
    /// <summary>The clamped box.</summary>
    MCRect Box;
    /// <summary>The unclamped box's top and bottom rows: the frame rows (which may have been clipped away).</summary>
    int32_t FrameTop;
    int32_t FrameBottom;
    /// <summary>The bar's length inside each row (0: none), and its AlphaTable row.</summary>
    int32_t BarLength;
    int32_t AlphaColor;
};

/// <summary>
/// A font character (VFX_character_draw) with its clipping resolved: <c>Columns</c> x <c>Rows</c> of the glyph from
/// (SourceX, SourceY), drawn at window (X, Y).
/// </summary>
struct MCGlyphCommand
{
    const void* Font;
    int32_t Character;
    int32_t X;
    int32_t Y;
    int32_t SourceX;
    int32_t SourceY;
    int32_t Columns;
    int32_t Rows;
    /// <summary>Null: every byte copied; else mapped through the table, 255 transparent.</summary>
    const uint8_t* Table;
};

/// <summary>
/// CopySprite with its clipping resolved: a bitmap blended onto the target through AlphaTable, at full size or
/// halved (every other pixel of every other row), optionally mirrored.
/// </summary>
struct MCAlphaBlitCommand
{
    /// <summary>The bitmap, and where in it the first pixel read lies (it may be before the start when mirrored).</summary>
    const uint8_t* Sprite;
    intptr_t Offset;
    int32_t Pitch;
    /// <summary>The window position of the first pixel written.</summary>
    int32_t Left;
    int32_t Top;
    /// <summary>The source columns and rows covered (before halving).</summary>
    int32_t Columns;
    int32_t Rows;
    bool Mirror;
    bool FullSize;
};

/// <summary>
/// A picture shown under a window's key colour (<see cref="MCRenderer::UnderlayKey"/>): <c>Source</c> scaled into
/// <c>Rect</c> of <c>Target</c>, each target pixel showing the source pixel under its centre (nearest). The world view
/// is the screen's underlay: the camera draws the world into a surface of its own at 1x, the display's composite
/// shader scales it into the view, and the UI drawn on the screen lets it show through where it left the key.
/// </summary>
/// <remarks>
/// A draw that maps the pixels under it (a translucent shape, a status bar, a translate polygon) can't map the world
/// from the screen. Where it meets the key, it records its table in the target's op plane instead: the pixel stays the
/// key, and its op names a table (<see cref="MCRenderer::OpTables"/>) the composite maps the world pixel through. A
/// second such draw on the pixel records the two tables composed. The world under it comes out as the original's
/// palette arithmetic made it.
/// </remarks>
struct MCUnderlay
{
    /// <summary>Who set it (a view window), to replace or remove it by.</summary>
    const void* Owner;
    /// <summary>The window it lies under (the screen).</summary>
    const _window* Target;
    /// <summary>Where it is shown, in target coordinates (may reach past the target).</summary>
    MCRect Rect;
    /// <summary>The picture, shown whole.</summary>
    const _window* Source;
};

/// <summary>
/// The renderer. Every call is made from the game's thread. The vfx front end asks <see cref="For"/> which renderer
/// draws into a window, and issues its commands there.
/// </summary>
class MCRenderer
{
public:
    virtual ~MCRenderer() = default;

    /// <summary>
    /// The renderer that draws into <paramref name="window"/>: for a picture, the software renderer; for a view, one
    /// that moves each command by the view's origin and hands it to its target's renderer (valid until the next call).
    /// </summary>
    static MCRenderer& For(const _window* window);

    // Commands --------------------------------------------------------------------------------------------------

    /// <summary>Fills <paramref name="rect"/> with <paramref name="color"/>.</summary>
    virtual void Clear(_window* target, const MCRect& rect, uint8_t color) = 0;

    /// <summary>
    /// VFX_rectangle_hash's pattern over <paramref name="rect"/>: every other pixel, rows an even distance above its
    /// bottom row starting at its left column, the others one pixel in.
    /// </summary>
    virtual void Hash(_window* target, const MCRect& rect, uint8_t color) = 0;

    /// <summary>Copies a rectangle of one window into another (or the same).</summary>
    virtual void Copy(_window* target, const MCCopyCommand& command) = 0;

    /// <summary>Blends a bitmap onto the target (CopySprite).</summary>
    virtual void AlphaBlit(_window* target, const MCAlphaBlitCommand& command) = 0;

    /// <summary>Writes <paramref name="count"/> pixels as they are from (x, y) (the image decoders' rows).</summary>
    virtual void Write(_window* target, int32_t x, int32_t y, const uint8_t* pixels, int32_t count) = 0;

    /// <summary>One pixel.</summary>
    virtual void Pixel(_window* target, int32_t x, int32_t y, uint8_t color) = 0;

    /// <summary>A run-length shape.</summary>
    virtual void Shape(_window* target, const MCShapeCommand& command) = 0;

    /// <summary>A fast shape.</summary>
    virtual void FastShape(_window* target, const MCFastShapeCommand& command) = 0;

    /// <summary>A terrain tile.</summary>
    virtual void Tile(_window* target, const MCTileCommand& command) = 0;

    /// <summary>A convex polygon.</summary>
    virtual void Polygon(_window* target, const MCPolygonCommand& command) = 0;

    /// <summary>A textured quadrilateral (a rotated or scaled shape).</summary>
    virtual void MapQuad(_window* target, const MCMapQuadCommand& command) = 0;

    /// <summary>A line.</summary>
    virtual void Line(_window* target, const MCLineCommand& command) = 0;

    /// <summary>An ellipse outline or fill.</summary>
    virtual void Ellipse(_window* target, const MCEllipseCommand& command) = 0;

    /// <summary>A status bar.</summary>
    virtual void StatusBar(_window* target, const MCStatusBarCommand& command) = 0;

    /// <summary>A font character.</summary>
    virtual void Glyph(_window* target, const MCGlyphCommand& command) = 0;

    // Shared state ----------------------------------------------------------------------------------------------

    /// <summary>Tells every renderer the alpha table changed (InitAlphaLookup).</summary>
    static void AlphaTableChanged();

    /// <summary>
    /// Tells every renderer that shape data in [begin, begin + size) was freed or changed (SpriteManager::freeShapeRAM,
    /// VFX_shape_remap_colors), or with (null, SIZE_MAX) that all of it was (SpriteManager::dumpALL), so nothing cached
    /// from it outlives it.
    /// </summary>
    static void ForgetShapes(const void* begin, size_t size);

    // Underlays -------------------------------------------------------------------------------------------------

    /// <summary>The colour that shows a window's underlays.</summary>
    static constexpr uint8_t UnderlayKey = 0xff;

    /// <summary>Sets the underlay of <c>underlay.Owner</c> (replacing the one it set before).</summary>
    static void SetUnderlay(const MCUnderlay& underlay);

    /// <summary>Drops the underlay <paramref name="owner"/> set, if any.</summary>
    static void RemoveUnderlay(const void* owner);

    /// <summary>The underlays, in the order they were first set (later ones are shown over earlier ones).</summary>
    static std::span<const MCUnderlay> Underlays();

    /// <summary>
    /// Gives <paramref name="target"/> an op plane: one byte per pixel, laid out as its pixels (null removes it).
    /// Clearing the target clears its ops too.
    /// </summary>
    static void SetOpPlane(const _window* target, uint8_t* ops);

    /// <summary>The op plane of <paramref name="target"/>, or null.</summary>
    static uint8_t* OpPlane(const _window* target);

    /// <summary>The op tables: 256 rows of 256 bytes, row 0 the identity; <see cref="OpTableCount"/> rows in use.</summary>
    static const uint8_t* OpTables();

    /// <summary>How many rows of <see cref="OpTables"/> are in use (at least 1).</summary>
    static int32_t OpTableCount();

    /// <summary>
    /// The op of a 256-byte table: an existing row with the same bytes, or a new one; 0 when every row is taken
    /// (the caller then maps the key itself).
    /// </summary>
    static uint8_t OpFor(const uint8_t* table);

    /// <summary>The op that maps through <paramref name="first"/>'s table, then <paramref name="second"/>'s (0 when
    /// every row is taken).</summary>
    static uint8_t ComposeOps(uint8_t first, uint8_t second);

    /// <summary>Starts a frame's op tables (once a frame, before anything is drawn, after the last present).</summary>
    static void ResetOpTables();

    /// <summary>
    /// The picture as shown, in palette indices: in <paramref name="pixels"/>, a copy of <paramref name="target"/>'s
    /// pixels with its layout, each key pixel of <paramref name="rect"/> over an underlay becomes the underlay's pixel,
    /// mapped through its op (the composite shader's rule, for screenshots, tests and a display without shaders).
    /// </summary>
    static void ComposeUnderlays(const _window* target, uint8_t* pixels, const MCRect& rect);

protected:
    /// <summary>The alpha table changed: drop anything built from it.</summary>
    virtual void OnAlphaTableChanged() = 0;

    /// <summary>Shape data in [begin, begin + size) is going away: drop anything cached from it.</summary>
    virtual void OnShapesForgotten(const void* begin, size_t size) = 0;
};
