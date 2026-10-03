#pragma once

// The core types of VFX, the 8-bit software graphics library MechCommander draws everything with (Miles Design's
// "VFX", hand-written assembly in MCX.EXE: vfxa.asm, vfx3d.asm, and the game's own vfx\*.cpp helpers). The game draws
// into WINDOWs (8-bit bitmaps) through PANEs (clip rectangles on a window); the platform layer presents the screen's
// window. The functions are declared in vfx/vfxfuncs.h.
//
// Layouts checked against MCX.EXE: every VFX routine reads a pane as {window, x0, y0, x1, y1} and a window as
// {buffer, x_max, y_max} with rows of x_max + 1 bytes (no separate pitch); the game's globals tempWINDOW
// (0x007bbb48) and textureWindow (0x007f0970) are 12 bytes, and tempPANE (0x007bbb68) 20. The port's window adds a
// view pointer.

struct MCView;

/// <summary>An 8-bit bitmap: <c>(x_max + 1) * (y_max + 1)</c> palette indices, rows of <c>x_max + 1</c> bytes.</summary>
struct _window
{
    /// <summary>The pixels.</summary>
    uint8_t* buffer; // +0x00
    /// <summary>The last column (width - 1).</summary>
    int32_t x_max; // +0x04
    /// <summary>The last row (height - 1).</summary>
    int32_t y_max; // +0x08
    /// <summary>
    /// Port: set when the window is a view (a UI element's place on another window, see <see cref="MCView"/>)
    /// rather than pixels of its own; <c>buffer</c> is then null.
    /// </summary>
    MCView* View = nullptr;
};

using WINDOW = _window;

/// <summary>
/// A clip rectangle on a window, in window coordinates, inclusive of both corners. Coordinates given to a drawing
/// routine are relative to (x0, y0); drawing is clipped to the pane intersected with the window. The corners may lie
/// outside the window (only the intersection is drawn), and a pane with x1 &lt; x0 or y1 &lt; y0 draws nothing.
/// </summary>
struct _pane
{
    /// <summary>The window drawn into.</summary>
    _window* window; // +0x00
    int32_t x0;      // +0x04
    int32_t y0;      // +0x08
    int32_t x1;      // +0x0c
    int32_t y1;      // +0x10
};

using PANE = _pane;

/// <summary>
/// A palette entry as VFX keeps it (VFX's <c>RGB</c>, renamed by MechCommander to stay clear of the Win32 macro):
/// 6-bit VGA components, 0..63; the platform layer scales them up when it sets the display palette.
/// </summary>
struct VFX_RGB
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

static_assert(sizeof(VFX_RGB) == 3);

/// <summary>A palette entry tagged with the index it belongs at (VFX's <c>CRGB</c>), as shape palettes store them.</summary>
struct VFX_CRGB
{
    uint8_t color;
    VFX_RGB rgb;
};

static_assert(sizeof(VFX_CRGB) == 4);

/// <summary>VFX's 16.16 fixed-point number.</summary>
using FIXED16 = int32_t;

/// <summary>A point, as <c>VFX_point_transform</c> takes it (VFX's <c>POINT</c>, prefixed to stay clear of Win32's).</summary>
struct VFX_POINT
{
    int32_t x;
    int32_t y;
};

/// <summary>
/// A polygon vertex as the vfx3d routines take them: screen position in pane coordinates, and 16.16 fixed-point
/// colour (Gouraud shading) and texture coordinates.
/// </summary>
struct SCRNVERTEX
{
    int32_t x; // +0x00
    int32_t y; // +0x04
    FIXED16 c; // +0x08
    FIXED16 u; // +0x0c
    FIXED16 v; // +0x10
    FIXED16 w; // +0x14
};

/// <summary>The header of a VFX bitmap font; the character offsets and glyphs follow (see vfx/vfxfuncs.h).</summary>
struct VFX_FONT
{
    int32_t version;         // +0x00
    int32_t char_count;      // +0x04
    int32_t char_height;     // +0x08
    int32_t font_background; // +0x0c
};

/// <summary>VFX's colour-translation table: 256 indices, applied by the <c>_translate_</c> draws.</summary>
using VFX_XLAT = uint8_t[256];
