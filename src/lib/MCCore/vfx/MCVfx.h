#pragma once

// The core types of VFX, the 8-bit software graphics library MechCommander draws everything with (Miles Design's
// "VFX", hand-written assembly in MCX.EXE: vfxa.asm, vfx3d.asm, and the game's own vfx\*.cpp helpers). The game draws
// into WINDOWs (8-bit bitmaps) through PANEs (clip rectangles on a window); the platform layer presents the screen's
// window. The functions are declared in vfx/MCVfxFunctions.h.
//
// Layouts in MCX.EXE: every VFX routine reads a pane as {window, x0, y0, x1, y1} and a window as {buffer, x_max,
// y_max} with rows of x_max + 1 bytes (no separate pitch). The port's window shares its pixels, may be a view instead,
// and knows its texture; a pane refers to its window.

struct MCWindow;
struct MCTexture;

/// <summary>A 256-byte colour table (fade, haze, remap, translate), borrowed for one call.</summary>
using MCColorTable = std::span<const uint8_t, 256>;

/// <summary>An inclusive rectangle in window coordinates.</summary>
struct MCRect
{
    int32_t X0;
    int32_t Y0;
    int32_t X1;
    int32_t Y1;
};

/// <summary>
/// A view: the window that holds it has no pixels; what is drawn into it lands on <c>Target</c>, its pixel (0, 0) at
/// (<c>OriginX</c>, <c>OriginY</c>), cut to <c>Scissor</c>. The vfx front end clips each draw to the window and then
/// to the scissor (moved into the window's coordinates), and <see cref="MCRenderer::For"/> hands the view's commands
/// on to the target's renderer, moved by the origin. A UI element owns its view and opens the scissor only while it
/// draws in the frame pass, so a draw at any other time (painting on an event) does nothing.
/// </summary>
struct MCView
{
    /// <summary>The window drawn on (a picture, not another view), shared while the view draws on it.</summary>
    std::shared_ptr<MCWindow> Target;
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

/// <summary>An 8-bit bitmap: <c>(x_max + 1) * (y_max + 1)</c> palette indices, rows of <c>x_max + 1</c> bytes.</summary>
struct MCWindow
{
    /// <summary>
    /// The pixels, shared by the windows over them (the screen ports share the display's, the fog port the terrain's
    /// flags); none for a view. At least <see cref="Size"/> bytes.
    /// </summary>
    std::shared_ptr<uint8_t[]> Pixels;
    /// <summary>The last column (width - 1).</summary>
    int32_t XMax = 0;
    /// <summary>The last row (height - 1).</summary>
    int32_t YMax = 0;
    /// <summary>
    /// Port: set when the window is a view (a UI element's place on another window) rather than pixels of its own.
    /// </summary>
    std::optional<MCView> View;
    /// <summary>
    /// Port: the renderers' handle on the pixels (<see cref="MCRenderer::CreateTexture"/>), made and destroyed by
    /// whoever owns them. A draw that reads the window as a picture reads its texture; a window without one can't be
    /// drawn from on the GPU.
    /// </summary>
    std::weak_ptr<MCTexture> Texture;

    /// <summary>The number of pixels, <c>(x_max + 1) * (y_max + 1)</c> (0 for an empty window).</summary>
    size_t Size() const
    {
        return XMax < 0 || YMax < 0 ? 0 : static_cast<size_t>(XMax + 1) * static_cast<size_t>(YMax + 1);
    }

    /// <summary>The pixels as a span (none for a window without them).</summary>
    std::span<uint8_t> Bytes() const
    {
        return Pixels != nullptr ? std::span<uint8_t>(Pixels.get(), Size()) : std::span<uint8_t>();
    }

    /// <summary>A shared window of <paramref name="width"/> x <paramref name="height"/> zeroed pixels of its own.</summary>
    static std::shared_ptr<MCWindow> Make(int32_t width, int32_t height)
    {
        auto window = std::make_shared<MCWindow>();
        window->Pixels = std::make_shared<uint8_t[]>(static_cast<size_t>(std::max(width, 0)) *
                                                     static_cast<size_t>(std::max(height, 0)));
        window->XMax = width - 1;
        window->YMax = height - 1;
        return window;
    }
};

/// <summary>
/// A clip rectangle on a window, in window coordinates, inclusive of both corners. Coordinates given to a drawing
/// routine are relative to (x0, y0); drawing is clipped to the pane intersected with the window. The corners may lie
/// outside the window (only the intersection is drawn), and a pane with x1 &lt; x0 or y1 &lt; y0 draws nothing.
/// </summary>
struct MCPane
{
    /// <summary>The window drawn into; the drawing routines draw nothing once it has gone.</summary>
    std::weak_ptr<MCWindow> Window;
    int32_t X0 = 0;
    int32_t Y0 = 0;
    int32_t X1 = 0;
    int32_t Y1 = 0;

    /// <summary>A pane over all of <paramref name="window"/>.</summary>
    static MCPane Over(std::shared_ptr<MCWindow> window)
    {
        const int32_t xMax = window->XMax;
        const int32_t yMax = window->YMax;
        return MCPane{std::move(window), 0, 0, xMax, yMax};
    }
};

/// <summary>
/// A palette entry as VFX keeps it (VFX's <c>RGB</c>, renamed by MechCommander to stay clear of the Win32 macro):
/// 6-bit VGA components, 0..63; the platform layer scales them up when it sets the display palette.
/// </summary>
struct MCVfxRgb
{
    uint8_t R;
    uint8_t G;
    uint8_t B;
};

static_assert(sizeof(MCVfxRgb) == 3);

/// <summary>A palette entry tagged with the index it belongs at (VFX's <c>CRGB</c>), as shape palettes store them.</summary>
struct MCVfxCrgb
{
    uint8_t Color;
    MCVfxRgb Rgb;
};

static_assert(sizeof(MCVfxCrgb) == 4);

/// <summary>VFX's 16.16 fixed-point number.</summary>
using MCFixed16 = int32_t;

/// <summary>A point, as <c>VFX_point_transform</c> takes it (VFX's <c>POINT</c>, prefixed to stay clear of Win32's).</summary>
struct MCVfxPoint
{
    int32_t X;
    int32_t Y;
};

/// <summary>
/// A polygon vertex as the vfx3d routines take them: screen position in pane coordinates, and 16.16 fixed-point
/// colour (Gouraud shading) and texture coordinates.
/// </summary>
struct MCScreenVertex
{
    int32_t X;
    int32_t Y;
    MCFixed16 C;
    MCFixed16 U;
    MCFixed16 V;
    MCFixed16 W;
};

/// <summary>The header of a VFX bitmap font; the character offsets and glyphs follow (see vfx/MCVfxFunctions.h).</summary>
struct MCVfxFont
{
    int32_t Version;
    int32_t CharCount;
    int32_t CharHeight;
    int32_t FontBackground;
};

/// <summary>VFX's colour-translation table: 256 indices, applied by the <c>_translate_</c> draws.</summary>
using MCVfxXlat = uint8_t[256];
