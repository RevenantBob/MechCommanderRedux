#pragma once

// Port-only helpers shared by the vfx/*.cpp files. The asm expanded the same prologue macro at the top of every
// routine that draws through a pane; the port has it once here.
//
// The vfx routines are the renderer's front end: they keep the original's clip arithmetic and hand each draw, resolved,
// to the renderer of the window (MCRenderer::For), which writes the pixels.

#include "platform/MCRenderer.h"
#include "vfx/MCVfxFunctions.h"

/// <summary>A pane resolved against its window, as the VFX prologue computes it.</summary>
struct MCVfxClip
{
    /// <summary>The window's pixels.</summary>
    uint8_t* Buffer;
    /// <summary>The window's row length in bytes (x_max + 1).</summary>
    int32_t Stride;
    /// <summary>The window's height (y_max + 1).</summary>
    int32_t Height;
    /// <summary>The pane's origin, unclipped: pane coordinates are relative to it.</summary>
    int32_t PaneX;
    int32_t PaneY;
    /// <summary>The pane clipped to the window, inclusive, in window coordinates.</summary>
    int32_t X0;
    int32_t Y0;
    int32_t X1;
    int32_t Y1;

    /// <summary>The address of window pixel (x, y).</summary>
    uint8_t* At(int32_t x, int32_t y) const { return Buffer + static_cast<intptr_t>(y) * Stride + x; }
};

/// <summary>
/// Resolves <paramref name="pane"/>: the pane's rectangle with x0/y0 raised to 0 and x1/y1 lowered to the window's
/// last column/row.
/// </summary>
/// <returns>0, VfxErrBadWindow when the window has no pixels, or VfxErrEmptyPane when the result is empty.</returns>
inline int32_t MCVfxClipPane(const MCPane* pane, MCVfxClip& clip)
{
    const MCWindow* window = pane->Window;
    clip.Stride = window->XMax + 1;

    if (clip.Stride <= 0)
    {
        return VfxErrBadWindow;
    }

    clip.Height = window->YMax + 1;

    if (clip.Height <= 0)
    {
        return VfxErrBadWindow;
    }

    clip.PaneX = pane->X0;
    clip.PaneY = pane->Y0;
    clip.X0 = pane->X0 > 0 ? pane->X0 : 0;
    clip.Y0 = pane->Y0 > 0 ? pane->Y0 : 0;
    clip.X1 = pane->X1 < clip.Stride - 1 ? pane->X1 : clip.Stride - 1;
    clip.Y1 = pane->Y1 < clip.Height - 1 ? pane->Y1 : clip.Height - 1;
    MCClipToView(window, clip.X0, clip.Y0, clip.X1, clip.Y1);

    if (clip.X1 < clip.X0 || clip.Y1 < clip.Y0)
    {
        return VfxErrEmptyPane;
    }

    clip.Buffer = window->Buffer;
    return 0;
}

/// <summary>
/// The table <c>VFX_shape_lookaside</c> fills and the translating shape draws map pixels through (VFX's own copy,
/// at 0x007a995c in MCX.EXE). Defined in vfx/vfxa_shape.cpp.
/// </summary>
extern uint8_t VfxShapeLookasideTable[256];

/// <summary>A shape's header in a VFX shape table.</summary>
struct MCVfxShapeHeader
{
    int32_t Bounds;
    int32_t Origin;
    int32_t XMin;
    int32_t YMin;
    int32_t XMax;
    int32_t YMax;
};

static_assert(sizeof(MCVfxShapeHeader) == 0x18);

/// <summary>Shape <paramref name="shapeNum"/>'s header: at the offset in the table's directory (8 + 8n).</summary>
inline uint8_t* MCVfxShape(void* shapeTable, int32_t shapeNum)
{
    uint8_t* table = static_cast<uint8_t*>(shapeTable);
    int32_t offset;
    std::memcpy(&offset, table + 8 + static_cast<intptr_t>(shapeNum) * 8, 4);
    return table + offset;
}

/// <summary>Reads a little-endian 32-bit value at <paramref name="p"/> (shape data isn't always aligned).</summary>
inline int32_t MCVfxRead32(const void* p)
{
    int32_t value;
    std::memcpy(&value, p, 4);
    return value;
}
