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
// Coordinates in commands are window coordinates, rectangles inclusive. Tables are passed as pointers into registered
// data blocks (MCDataKind::Tables): a renderer keeps what it took from a table by its address until the block's owner
// says the bytes changed.

#include "vfx/vfx.h"

/// <summary>How a texture's pixels change (<see cref="MCRenderer::CreateTexture"/>).</summary>
enum class MCTextureUse : uint8_t
{
    /// <summary>Loaded once (art); changed only through <see cref="MCRenderer::LockTexture"/>.</summary>
    Static,
    /// <summary>Drawn into on the CPU as well (a port the game paints); every draw into it marks it changed.</summary>
    Dynamic,
    /// <summary>
    /// A new picture most frames (a movie), written through <see cref="MCRenderer::LockTexture"/>. While the GPU draws,
    /// a lock hands out the hardware renderer's upload memory, so the pixels go straight to the GPU and
    /// <c>Pixels</c> no longer holds them (<c>CpuStale</c>).
    /// </summary>
    Stream
};

/// <summary>What a registered data block holds (<see cref="MCRenderer::RegisterData"/>).</summary>
enum class MCDataKind : uint8_t
{
    /// <summary>Encoded images: shape tables, fast shapes, fonts, terrain tiles.</summary>
    Shapes,
    /// <summary>256-byte colour tables (fades, haze, remaps, translate tables), at any alignment.</summary>
    Tables
};

/// <summary>
/// A block of memory registered as holding data draws read (<see cref="MCRenderer::RegisterData"/>). A renderer that
/// keeps something made from the data (a GPU atlas image) keys it by the address read; the block's owner says when the
/// bytes change or go, so nothing ever compares or hashes them.
/// </summary>
struct MCDataBlock
{
    const uint8_t* Begin = nullptr;
    const uint8_t* End = nullptr;
    MCDataKind Kind = MCDataKind::Shapes;
};

/// <summary>
/// The lowest address a 256-byte table reaching <paramref name="at"/> may start at (255 bytes before it, or the bottom
/// of the address space): what a change at <paramref name="at"/> may reach of the tables kept by address.
/// </summary>
inline const uint8_t* MCFirstTableReaching(const void* at)
{
    const auto address = reinterpret_cast<uintptr_t>(at);
    return reinterpret_cast<const uint8_t*>(address - std::min<uintptr_t>(address, 255));
}

/// <summary>An inclusive rectangle in window coordinates.</summary>
struct MCRect
{
    int32_t X0;
    int32_t Y0;
    int32_t X1;
    int32_t Y1;
};

/// <summary>
/// A texture: a picture's pixels as the renderers hold them. The code that owns the pixels makes it
/// (<see cref="MCRenderer::CreateTexture"/>), hands it to the window over them (<c>_window::Texture</c>) and
/// destroys it before the pixels go. The software renderer reads <c>Pixels</c>; the hardware renderer keeps its own
/// copy (<c>Hardware</c>) and uploads <c>Pixels</c> again only when they changed (<c>Dirty</c>). Nothing ever compares
/// or hashes the pixels to find out.
/// </summary>
struct MCTexture
{
    /// <summary>The pixels (owned by the caller), <c>Width</c> bytes a row.</summary>
    uint8_t* Pixels = nullptr;
    int32_t Width = 0;
    int32_t Height = 0;
    MCTextureUse Use = MCTextureUse::Static;
    /// <summary>Whether <c>Pixels</c> changed since the hardware renderer last took them.</summary>
    bool Dirty = true;
    /// <summary>
    /// Whether a stream lock wrote into the hardware renderer's memory instead of <c>Pixels</c>, which then lack those
    /// pixels: the software renderer must not read or draw into them (<see cref="MCRenderer::StaleCpuReads"/>).
    /// </summary>
    bool CpuStale = false;
    /// <summary>The rectangle locked (<see cref="MCRenderer::LockTexture"/>), and whether the lock went to the
    /// hardware renderer.</summary>
    MCRect Locked{0, 0, -1, -1};
    bool LockedOnHardware = false;
    /// <summary>What the hardware renderer keeps for it (its own type), or null.</summary>
    void* Hardware = nullptr;
    /// <summary>Its place in the renderer's list of textures.</summary>
    size_t Slot = 0;
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
/// A map's ground as a hardware renderer keeps it: the tile of every cell (the quad whose top-left corner is that map
/// vertex) and the elevations of its corners, over the map and a ring of the off-map cells around it. Made once per map
/// (see terrain.cpp); a renderer uploads it again only when <c>Version</c> changes.
/// </summary>
struct MCTerrainMesh
{
    /// <summary>Changes whenever the contents do (a renderer compares it with the one it holds).</summary>
    uint64_t Version = 0;
    /// <summary>The map vertex (row, column) of the first cell, and how many cells a row and how many rows.</summary>
    int32_t FirstRow = 0;
    int32_t FirstCol = 0;
    int32_t Cols = 0;
    int32_t Rows = 0;
    /// <summary>Per cell, row by row: its tile (an index into <c>Tiles</c>), or <c>NoTile</c>.</summary>
    std::vector<uint32_t> CellTiles;
    /// <summary>Per cell: its corners' elevation levels, a byte each (top left, top right, bottom right, bottom left).</summary>
    std::vector<uint32_t> CellElevations;
    /// <summary>The tiles' data (vfx/vfxtile.cpp's format), copied.</summary>
    std::vector<std::vector<uint8_t>> Tiles;

    static constexpr uint32_t NoTile = 0xffffffffu;
};

/// <summary>
/// One frame's terrain pass (TerrainWindow::render's tiles), drawn from a <see cref="MCTerrainMesh"/>: the numbers that
/// place it, the cells of the terrain window's grid, the clipping and the haze. Points are in pane pixels, as the
/// terrain's vertices are projected.
/// </summary>
struct MCTerrainFrame
{
    const MCTerrainMesh* Mesh = nullptr;
    /// <summary>The screen point of map vertex (0, 0): vertex (row, col) at elevation e lies at OriginX + (col - row) *
    /// StepX, OriginY + (row + col) * StepY - e * ElevStep.</summary>
    int32_t OriginX = 0;
    int32_t OriginY = 0;
    int32_t StepX = 0;
    int32_t StepY = 0;
    int32_t ElevStep = 0;
    /// <summary>The cells drawn: their top-left vertices' rows and columns, inclusive.</summary>
    int32_t FirstRow = 0;
    int32_t FirstCol = 0;
    int32_t LastRow = -1;
    int32_t LastCol = -1;
    /// <summary>A cell is left out when all its corners lie outside these (Vertex::clipped).</summary>
    int32_t MinX = 0;
    int32_t MaxX = 0;
    int32_t MinY = 0;
    int32_t MaxY = 0;
    /// <summary>The pane's corner in the window, and the window pixels a tile may write (VFX_nTile_draw's clip).</summary>
    int32_t PaneX = 0;
    int32_t PaneY = 0;
    MCRect Clip{0, 0, -1, -1};
    /// <summary>The fog of war's flags (a ByteFlag's window, a byte per map vertex), as the home side sees.</summary>
    const _window* Fog = nullptr;
    /// <summary>Every tile filled with colour 0x10 (the haze factor's "all black").</summary>
    bool AllFilled = false;
    /// <summary>The haze tables for one, two and three corners seen.</summary>
    const uint8_t* Haze[3] = {};
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
    /// <summary>The bitmap's texture (its pixels are <c>Sprite</c>, <c>Pitch</c> wide), or null when it has none.</summary>
    MCTexture* Texture = nullptr;
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
/// A shape transform (AG_shape_transform, AG_shape_translate_transform): shape <c>ShapeNum</c> filled into
/// <c>Buffer</c>, a picture of its bounds (<c>Width</c> x <c>Height</c>: skipped pixels and the rest colour 0, drawn
/// pixels mapped through <c>Table</c> when there is one), then blended onto the target as <c>Blit</c> says
/// (<c>Blit.Sprite</c> is <c>Buffer</c>). A renderer that keeps the shape's picture itself may leave the buffer alone.
/// </summary>
struct MCShapeBlitCommand
{
    const void* ShapeTable;
    int32_t ShapeNum;
    /// <summary>The lookaside table of the translating transform, else null.</summary>
    const uint8_t* Table;
    uint8_t* Buffer;
    int32_t Width;
    int32_t Height;
    MCAlphaBlitCommand Blit;
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

/// <summary>Who draws the frame surfaces (<see cref="MCRenderer::AddFrameSurface"/>).</summary>
enum class MCGpuDrawing
{
    /// <summary>The software renderer, into the windows' memory (the presenter uploads them each frame).</summary>
    Off,
    /// <summary>The hardware renderer, on the GPU.</summary>
    On,
    /// <summary>Both, the hardware renderer's surfaces compared with the software's each frame (development).</summary>
    Mirror
};

/// <summary>The drawing a name stands for ("off", "on", "mirror", any case); empty for another name.</summary>
std::optional<MCGpuDrawing> MCGpuDrawingFromName(std::string_view name);

/// <summary>
/// The renderer. Every call is made from the game's thread. The vfx front end asks <see cref="For"/> which renderer
/// draws into a window, and issues its commands there.
/// </summary>
class MCRenderer
{
public:
    virtual ~MCRenderer() = default;

    /// <summary>
    /// The renderer that draws into <paramref name="window"/>: for a frame surface, the hardware renderer when there is
    /// one and it draws (or both, mirrored); for any other picture, the software renderer; for a view, one that moves
    /// each command by the view's origin and hands it to its target's renderer (valid until the next call).
    /// </summary>
    static MCRenderer& For(const _window* window);

    // Textures ----------------------------------------------------------------------------------------------------

    /// <summary>Makes a texture over <paramref name="pixels"/> (<paramref name="width"/> bytes a row).</summary>
    static MCTexture* CreateTexture(uint8_t* pixels, int32_t width, int32_t height, MCTextureUse use);

    /// <summary>Makes a texture over <paramref name="window"/>'s pixels and hands it to the window.</summary>
    static MCTexture* CreateTexture(_window* window, MCTextureUse use);

    /// <summary>Points <paramref name="texture"/> at new pixels or a new size (they count as changed).</summary>
    static void ResizeTexture(MCTexture* texture, uint8_t* pixels, int32_t width, int32_t height);

    /// <summary>Makes <paramref name="window"/>'s texture follow its pixels and size again.</summary>
    static void ResizeTexture(_window* window);

    /// <summary>Destroys <paramref name="texture"/> (null is ignored) and clears the pointer.</summary>
    static void DestroyTexture(MCTexture*& texture);

    /// <summary>Destroys <paramref name="window"/>'s texture, if it has one.</summary>
    static void DestroyTexture(_window* window);

    /// <summary>Where to write a texture's new pixels (its <c>Pixels</c>); <see cref="UnlockTexture"/> when done.</summary>
    static uint8_t* LockTexture(MCTexture* texture);

    /// <summary>
    /// Where to write new pixels for <paramref name="rect"/> of <paramref name="texture"/>: its top left pixel, rows
    /// <c>Width</c> bytes apart (whole rows are writable). Every pixel of the rectangle must be written before
    /// <see cref="UnlockTexture"/>: for a <see cref="MCTextureUse::Stream"/> texture while the GPU draws, the memory is
    /// the hardware renderer's upload memory, whose old contents are undefined.
    /// </summary>
    static uint8_t* LockTexture(MCTexture* texture, const MCRect& rect);

    /// <summary>The pixels written since <see cref="LockTexture"/> are complete: renderers take them at the next use.</summary>
    static void UnlockTexture(MCTexture* texture);

    /// <summary>Every texture made and not yet destroyed.</summary>
    static std::span<MCTexture* const> Textures();

    // Data blocks -------------------------------------------------------------------------------------------------

    /// <summary>
    /// Registers [begin, begin + size) as holding <paramref name="kind"/> data (where its owner loads it). A block it
    /// overlaps is taken to be gone (the memory was reused). An <c>MCBlockStore</c> block, once freed, is unregistered
    /// by the store; other memory must be unregistered by its owner before it goes.
    /// </summary>
    static void RegisterData(const void* begin, size_t size, MCDataKind kind);

    /// <summary>The bytes in [begin, begin + size) of a registered block changed: renderers drop what they made of them.</summary>
    static void DataChanged(const void* begin, size_t size);

    /// <summary>Unregisters every block that starts in [begin, begin + size) (renderers drop what they made of them).</summary>
    static void UnregisterData(const void* begin, size_t size = 1);

    /// <summary>The registered block holding <paramref name="at"/>, or null.</summary>
    static const MCDataBlock* DataBlockOf(const void* at);

    /// <summary>How many blocks are registered.</summary>
    static size_t DataBlockCount();

    /// <summary>
    /// Notes a draw that read a resource nobody registered (a window without a texture, shape data outside every
    /// registered block): a bug in the caller, asserted in Debug, logged once per <paramref name="what"/> and counted.
    /// </summary>
    static void NoteUnregistered(const char* what);

    /// <summary>How many draws <see cref="NoteUnregistered"/> counted.</summary>
    static int64_t UnregisteredDraws();

    /// <summary>
    /// While one is alive, <see cref="NoteUnregistered"/> logs and counts but doesn't assert: for tests that draw
    /// unregistered data on purpose to check that it is refused. Scopes nest.
    /// </summary>
    class ExpectUnregistered
    {
    public:
        /// <summary>Starts expecting unregistered draws.</summary>
        ExpectUnregistered();

        /// <summary>Stops expecting them (once the outermost scope ends).</summary>
        ~ExpectUnregistered();

        ExpectUnregistered(const ExpectUnregistered&) = delete;
        ExpectUnregistered& operator=(const ExpectUnregistered&) = delete;
    };

    // Frame surfaces ----------------------------------------------------------------------------------------------

    /// <summary>
    /// Makes <paramref name="window"/> a frame surface: a picture drawn every frame (the screen, the world view's
    /// surface), which a hardware renderer keeps on the GPU. Windows over the same pixels count as the same surface.
    /// A <paramref name="kept"/> surface is one the game also reads (the fog of war's flags): while the GPU draws, the
    /// software renderer draws its memory too, and the GPU's copy starts from the pixels it has when added.
    /// </summary>
    static void AddFrameSurface(const _window* window, bool kept = false);

    /// <summary>Whether <paramref name="window"/> is (or lies over) a kept frame surface.</summary>
    static bool KeptSurface(const _window* window);

    /// <summary>Makes <paramref name="window"/> an ordinary picture again (before its pixels go).</summary>
    static void RemoveFrameSurface(const _window* window);

    /// <summary>The frame surface <paramref name="window"/> is (or lies over the pixels of), or null.</summary>
    static const _window* FrameSurfaceOf(const _window* window);

    /// <summary>
    /// Sets the hardware renderer (the Vulkan presenter's, or null) and who draws the frame surfaces; the drawing is
    /// <see cref="MCGpuDrawing::Off"/> without one.
    /// </summary>
    static void SetHardware(MCRenderer* hardware, MCGpuDrawing drawing);

    /// <summary>The hardware renderer, or null.</summary>
    static MCRenderer* Hardware();

    /// <summary>Who draws the frame surfaces now.</summary>
    static MCGpuDrawing GpuDrawing();

    /// <summary>
    /// Who should draw the frame surfaces when a hardware renderer starts: what <see cref="RequestGpuDrawing"/> set
    /// (<c>-gpudraw off|on|mirror</c>, tests); on by default.
    /// </summary>
    static MCGpuDrawing RequestedGpuDrawing();

    /// <summary>Sets what <see cref="RequestedGpuDrawing"/> returns (before the display is made).</summary>
    static void RequestGpuDrawing(MCGpuDrawing drawing);

    /// <summary>Where mirror mode saves the first frame that differs (<c>-gpudump</c>); empty saves nothing.</summary>
    static const std::filesystem::path& MirrorDumpFolder();

    /// <summary>Sets <see cref="MirrorDumpFolder"/>.</summary>
    static void SetMirrorDumpFolder(const std::filesystem::path& folder);

    /// <summary>
    /// Called where the software renderer reads <paramref name="source"/>'s pixels (for <paramref name="command"/>).
    /// While the GPU draws the frame surfaces alone, a frame surface's memory is never drawn, so such a read sees stale
    /// pixels: it is logged (once per command) and counted.
    /// </summary>
    static void NoteCpuRead(const _window* source, const char* command);

    /// <summary>
    /// How many stale CPU reads <see cref="NoteCpuRead"/> found, plus software draws into a stream texture whose pixels
    /// went to the GPU alone (<c>MCTexture::CpuStale</c>).
    /// </summary>
    static int64_t StaleCpuReads();

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

    /// <summary>Fills a shape into a picture and blends that onto the target (a shape transform).</summary>
    virtual void ShapeBlit(_window* target, const MCShapeBlitCommand& command) = 0;

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

    /// <summary>
    /// Draws a frame's terrain tiles from the map's mesh. A hardware renderer always does (the terrain pass asks it
    /// whenever it draws the world, and a failure is an error, never a quiet return to tile commands); the software
    /// renderer draws none. Until <see cref="EndTerrainLayer"/>, the tiles of the pass (VFX_nTile_draw) still come as
    /// commands, for the software renderer (mirror mode, or alone); one that drew the layer leaves them.
    /// </summary>
    virtual std::expected<void, std::string> TerrainLayer(_window* target, const MCTerrainFrame& frame);

    /// <summary>Ends the terrain pass <see cref="TerrainLayer"/> began.</summary>
    virtual void EndTerrainLayer(_window* target);

    // Shared state ----------------------------------------------------------------------------------------------

    /// <summary>Tells every renderer the alpha table changed (InitAlphaLookup).</summary>
    static void AlphaTableChanged();

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
    /// The op of a registered 256-byte table: the frame's row for that address, or a new one holding its bytes; 0 when
    /// every row is taken (the caller then maps the key itself).
    /// </summary>
    static uint8_t OpFor(const uint8_t* table);

    /// <summary>The op that maps through <paramref name="first"/>'s table, then <paramref name="second"/>'s (0 when
    /// both are 0, or every row is taken).</summary>
    static uint8_t ComposeOps(uint8_t first, uint8_t second);

    /// <summary>Starts a frame's op tables (once a frame, before anything is drawn, after the last present).</summary>
    static void ResetOpTables();

    /// <summary>
    /// The picture as shown, in palette indices: in <paramref name="pixels"/>, a copy of <paramref name="target"/>'s
    /// pixels with its layout, each key pixel of <paramref name="rect"/> over an underlay becomes the underlay's pixel,
    /// mapped through its op (the composite shader's rule, for screenshots, tests and a display without shaders).
    /// </summary>
    static void ComposeUnderlays(const _window* target, uint8_t* pixels, const MCRect& rect);

private:
    /// <summary>Logs <paramref name="what"/> (once) and counts it in <see cref="StaleCpuReads"/>.</summary>
    static void NoteStale(const char* what);

protected:
    /// <summary>The alpha table changed: drop anything built from it.</summary>
    virtual void OnAlphaTableChanged() = 0;

    /// <summary>Registered data in [begin, begin + size) changed or is going: drop anything made from it.</summary>
    virtual void OnDataChanged(const void*, size_t) {}

    /// <summary>A frame surface became an ordinary picture (its pixels are going): drop what was kept for it.</summary>
    virtual void OnFrameSurfaceRemoved(const _window*) {}

    /// <summary>A texture is going (or its size changed): drop what is kept in its <c>Hardware</c>.</summary>
    virtual void OnTextureReleased(MCTexture*) {}

    /// <summary>
    /// A stream texture's lock while the GPU draws alone: memory to write <c>Locked</c> into (rows <c>Width</c> bytes
    /// apart, uploaded at <see cref="OnUnlockStream"/>), or null to have the pixels written into <c>Pixels</c>.
    /// </summary>
    virtual uint8_t* OnLockStream(MCTexture*) { return nullptr; }

    /// <summary>
    /// A stream texture's new pixels for <c>Locked</c> are complete: written into the memory
    /// <see cref="OnLockStream"/> gave (<paramref name="pixels"/> null), or at <paramref name="pixels"/> (its top left
    /// pixel in <c>Pixels</c>; mirror mode), to be copied up the same way.
    /// </summary>
    virtual void OnUnlockStream(MCTexture*, const uint8_t* /*pixels*/) {}
};
