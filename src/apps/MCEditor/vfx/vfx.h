#pragma once

// The editor's copy of the VFX library (0x0047e9f0-0x00484400, hand-written assembly), as far as it is translated.

/// <summary>An 8-bit bitmap: <c>(x_max + 1) * (y_max + 1)</c> palette indices, rows of <c>x_max + 1</c> bytes.</summary>
struct WINDOW
{
    /// <summary>The pixels.</summary>
    uint8_t* buffer;
    /// <summary>The last column (width - 1).</summary>
    int32_t x_max;
    /// <summary>The last row (height - 1).</summary>
    int32_t y_max;
    /// <summary>Unused by the editor's callers (0).</summary>
    void* stencil;
    /// <summary>Unused by the editor's callers (0).</summary>
    void* shadow;
};

/// <summary>A rectangle of a <see cref="WINDOW"/>, in window coordinates; the drawing routines clip to it.</summary>
struct PANE
{
    WINDOW* window;
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;
};

/// <summary>The pane lies outside its window (<see cref="VFX_shape_scan"/>).</summary>
inline constexpr int32_t VFX_ERR_BAD_WINDOW = -1;
/// <summary>The pane has no pixels inside its window (<see cref="VFX_shape_scan"/>).</summary>
inline constexpr int32_t VFX_ERR_EMPTY_PANE = -2;

/// <summary>
/// Encodes the pixels of <paramref name="pane"/> as one VFX shape with its hot spot at (<paramref name="hotX"/>,
/// <paramref name="hotY"/>) in pane coordinates: pixels of <paramref name="transparentColor"/> become skips, the rest
/// runs and literals, cropped to the bounding box of the other pixels.
/// </summary>
/// <param name="buffer">Where the shape goes; null only measures it.</param>
/// <returns>The shape's size in bytes, or <see cref="VFX_ERR_BAD_WINDOW"/> / <see cref="VFX_ERR_EMPTY_PANE"/>.</returns>
/// <remarks>The shape starts with its bounds word, origin word and box (x0, y0, x1, y1 from the hot spot).</remarks>
int32_t VFX_shape_scan(PANE* pane, uint32_t transparentColor, int32_t hotX, int32_t hotY, void* buffer);
