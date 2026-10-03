#pragma once

// Port-only: the body behind the game's AG_shape_* routines (vfx\vfx_translatedraw.cpp and vfx\vfx_transform.cpp in
// MCX.EXE). The original repeated the same inline-asm body in every routine; the port has it once. Defined in
// vfx/vfx_translatedraw.cpp. Each routine's op (MCShapeOp): AG_shape_draw Draw, or Alpha for a marked shape;
// AG_shape_translate_draw Xlat, or XlatAlpha for a marked shape; AG_shape_fill Fill; AG_shape_translate_fill XlatFill.
// The translating ops map through <c>lookaside</c>.

#include "vfx/vfxint.h"

/// <summary>
/// Draws shape <paramref name="shapeNum"/> of <paramref name="shapeTable"/> with its hot spot at window coordinates
/// (hotX, hotY) (the AG routines don't offset by the pane's origin), clipped to the pane, applying
/// <paramref name="op"/> to each pixel with the table <paramref name="xlat"/> where the op uses one.
/// </summary>
void MCAgDrawShape(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, MCShapeOp op,
                   const uint8_t* xlat);

/// <summary>Whether the game's shape routines treat a shape as translucent: its data starts with the token pair 03 00.</summary>
inline bool MCAgShapeIsAlpha(void* shapeTable, int32_t shapeNum)
{
    const uint8_t* data = MCVfxShape(shapeTable, shapeNum) + 0x18;
    return data[0] == 3 && data[1] == 0;
}
