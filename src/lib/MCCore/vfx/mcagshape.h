#pragma once

// Port-only: the shape-walking engine behind the game's AG_shape_* routines (vfx\vfx_translatedraw.cpp and
// vfx\vfx_transform.cpp in MCX.EXE). The original repeated the same inline-asm body in every routine with small
// differences in its clipping arithmetic; the port has it once, with those differences as parameters, so each
// routine keeps the exact pixels it wrote. Defined in vfx/vfx_translatedraw.cpp.

#include "vfx/vfxint.h"

/// <summary>What an AG shape routine does with each pixel it writes.</summary>
enum class MCAgPixelOp
{
    /// <summary>Runs and literals are written as stored (AG_shape_draw).</summary>
    Draw,
    /// <summary>Each pixel becomes <c>AlphaTable[shape &lt;&lt; 8 | screen]</c> (AG_shape_draw of a marked shape).</summary>
    Alpha,
    /// <summary>Each pixel is mapped through <c>lookaside</c> (AG_shape_translate_draw).</summary>
    Xlat,
    /// <summary>Blended as Alpha, then mapped through <c>lookaside</c> (AG_shape_translate_draw of a marked shape).</summary>
    XlatAlpha,
    /// <summary>As Draw, and skipped pixels are written as colour 0 (AG_shape_fill).</summary>
    Fill,
    /// <summary>As Xlat, and skipped pixels are written as colour 0 (AG_shape_translate_fill).</summary>
    XlatFill
};

/// <summary>
/// The clipping arithmetic one AG routine used. Each routine computed the same quantities with slightly different
/// off-by-ones; these reproduce them.
/// </summary>
struct MCAgShapeQuirks
{
    /// <summary>Added to the shape's left column (1 for AG_shape_draw of a marked shape, else 0).</summary>
    int32_t XShift;
    /// <summary>Whether the bottom clip keeps row cy1 (true) or stops one row above it.</summary>
    bool BottomInclusive;
    /// <summary>Subtracted from the shape's right column before comparing it with cx1 (0 or 1).</summary>
    int32_t RightBias;
    /// <summary>Whether the biased right column equal to cx1 already takes the clipped path (the asm's JAE vs JA).</summary>
    bool ClipOnEqual;
    /// <summary>Whether the clipped path stops before column cx1 (AG_shape_translate_fill) instead of after it.</summary>
    bool ClipRightExclusive;
};

/// <summary>
/// Draws shape <paramref name="shapeNum"/> of <paramref name="shapeTable"/> with its hot spot at window coordinates
/// (hotX, hotY) (the AG routines don't offset by the pane's origin), clipped to the pane, applying
/// <paramref name="op"/> to each pixel with the table <paramref name="xlat"/> where the op uses one.
/// </summary>
void MCAgDrawShape(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, MCAgPixelOp op,
                   const MCAgShapeQuirks& quirks, const uint8_t* xlat);

/// <summary>Whether the game's shape routines treat a shape as translucent: its data starts with the token pair 03 00.</summary>
inline bool MCAgShapeIsAlpha(void* shapeTable, int32_t shapeNum)
{
    const uint8_t* data = MCVfxShape(shapeTable, shapeNum) + 0x18;
    return data[0] == 3 && data[1] == 0;
}
