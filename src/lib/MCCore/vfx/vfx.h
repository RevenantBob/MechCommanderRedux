#pragma once

// The core types of VFX, the 8-bit software graphics library MechCommander draws everything with (Miles Design's
// "VFX", hand-written assembly in MCX.EXE: vfxa.asm, vfx3d.asm, and the game's own vfx\*.cpp helpers). The game draws
// into WINDOWs (8-bit bitmaps) through PANEs (clip rectangles on a window); the platform layer presents the screen's
// window. The functions are declared in vfx/vfxfuncs.h.
//
// Layouts checked against MCX.EXE: every VFX routine reads a pane as {window, x0, y0, x1, y1} and a window as
// {buffer, x_max, y_max} with rows of x_max + 1 bytes (no separate pitch); the game's globals tempWINDOW
// (0x007bbb48) and textureWindow (0x007f0970) are 12 bytes, and tempPANE (0x007bbb68) 20. The port's window adds a
// view pointer and a texture handle.

struct MCView;
struct MCTexture;

/// <summary>An 8-bit bitmap: <c>(x_max + 1) * (y_max + 1)</c> palette indices, rows of <c>x_max + 1</c> bytes.</summary>
struct MCWindow
{
    /// <summary>The pixels.</summary>
    uint8_t* Buffer;
    /// <summary>The last column (width - 1).</summary>
    int32_t XMax;
    /// <summary>The last row (height - 1).</summary>
    int32_t YMax;
    /// <summary>
    /// Port: set when the window is a view (a UI element's place on another window, see <see cref="MCView"/>)
    /// rather than pixels of its own; <c>buffer</c> is then null.
    /// </summary>
    MCView* View = nullptr;
    /// <summary>
    /// Port: the renderers' handle on the pixels (<see cref="MCRenderer::CreateTexture"/>), made and destroyed by
    /// whoever owns them. A draw that reads the window as a picture reads its texture; a window without one can't be
    /// drawn from on the GPU.
    /// </summary>
    MCTexture* Texture = nullptr;
};

using MCWindow = MCWindow;

/// <summary>
/// A clip rectangle on a window, in window coordinates, inclusive of both corners. Coordinates given to a drawing
/// routine are relative to (x0, y0); drawing is clipped to the pane intersected with the window. The corners may lie
/// outside the window (only the intersection is drawn), and a pane with x1 &lt; x0 or y1 &lt; y0 draws nothing.
/// </summary>
struct MCPane
{
    /// <summary>The window drawn into.</summary>
    MCWindow* Window;
    int32_t X0;
    int32_t Y0;
    int32_t X1;
    int32_t Y1;
};

using MCPane = MCPane;

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

/// <summary>The header of a VFX bitmap font; the character offsets and glyphs follow (see vfx/vfxfuncs.h).</summary>
struct MCVfxFont
{
    int32_t Version;
    int32_t CharCount;
    int32_t CharHeight;
    int32_t FontBackground;
};

/// <summary>VFX's colour-translation table: 256 indices, applied by the <c>_translate_</c> draws.</summary>
using MCVfxXlat = uint8_t[256];
