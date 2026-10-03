#include "stdafx.h"
#include "vfx/vfxint.h"
#include "vfx/mcagshape.h"

// The game's shape "transforms" (mcx\vfx\vfx_transform.cpp): a shape is rendered opaque into a scratch buffer
// (AG_shape_fill / AG_shape_translate_fill, skipped pixels as colour 0), then blended onto the pane at full or half
// size, optionally mirrored, through AlphaTable (CopySprite). Colour 0 blends to the screen pixel unchanged, so the
// shape's skipped pixels stay transparent.

uint8_t* lookaside = nullptr;

namespace
{
    /// <summary>
    /// The body of AG_shape_transform and AG_shape_translate_transform: renders the shape into
    /// <paramref name="buffer"/> with <paramref name="fill"/>, then copies it to the pane.
    /// </summary>
    void TransformShape(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, void* buffer,
                        int32_t mirror, int32_t fullSize, void (*fill)(PANE*, void*, int32_t, int32_t, int32_t))
    {
        const uint8_t* header = MCVfxShape(shapeTable, shapeNum);
        const int32_t xMin = MCVfxRead32(header + 0x08);
        const int32_t yMin = MCVfxRead32(header + 0x0c);
        const int32_t xMax = MCVfxRead32(header + 0x10);
        const int32_t yMax = MCVfxRead32(header + 0x14);
        const int32_t width = xMax - xMin + 1;
        const int32_t height = yMax - yMin + 1;

        // The scratch window and pane the shape is rendered into (static globals at 0x00802450 and 0x00802470 in
        // MCX.EXE).
        WINDOW scratchWindow{static_cast<uint8_t*>(buffer), width - 1, height - 1};
        PANE scratchPane{&scratchWindow, 0, 0, width - 1, height - 1};

        const uint32_t size = static_cast<uint32_t>(height) * static_cast<uint32_t>(width);

        if (size >= 0x1fa40)
        {
            return; // the callers' buffers hold 360 x 360 pixels
        }

        std::memset(buffer, 0, size);
        fill(&scratchPane, shapeTable, shapeNum, -xMin, -yMin);

        uint8_t* sprite = static_cast<uint8_t*>(buffer);

        if (fullSize != 0)
        {
            if (mirror != 0)
            {
                CopySprite(pane, sprite, hotX - xMax, yMin + hotY, width, height, mirror, fullSize);
            }
            else
            {
                CopySprite(pane, sprite, xMin + hotX, yMin + hotY, width, height, 0, fullSize);
            }
        }
        else
        {
            if (mirror != 0)
            {
                CopySprite(pane, sprite, hotX - (xMax >> 1), (yMin >> 1) + hotY, width, height, mirror, 0);
            }
            else
            {
                CopySprite(pane, sprite, (xMin >> 1) + hotX, (yMin >> 1) + hotY, width, height, 0, 0);
            }
        }
    }
}

void AG_shape_transform(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, void* buffer,
                        int32_t mirror, int32_t fullSize)
{
    TransformShape(pane, shapeTable, shapeNum, hotX, hotY, buffer, mirror, fullSize, AG_shape_fill);
}

void AG_shape_translate_transform(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY,
                                  void* buffer, int32_t mirror, int32_t fullSize)
{
    TransformShape(pane, shapeTable, shapeNum, hotX, hotY, buffer, mirror, fullSize, AG_shape_translate_fill);
}

void CopySprite(PANE* pane, uint8_t* sprite, int x, int y, int width, int height, int mirror, int fullSize)
{
    const WINDOW* window = pane->window;
    const int32_t stride = window->x_max + 1;
    int32_t cx0 = pane->x0 < 0 ? 0 : pane->x0;
    int32_t cy0 = pane->y0 < 0 ? 0 : pane->y0;
    int32_t cx1 = pane->x1 < stride ? pane->x1 : window->x_max;
    int32_t cy1 = pane->y1 < window->y_max + 1 ? pane->y1 : window->y_max;
    MCClipToView(window, cx0, cy0, cx1, cy1);

    // OB-115: (x, y) are relative to the pane's origin; the asm offset by its corner clipped to the window.
    int32_t left = x + pane->x0;
    int32_t top = y + pane->y0;
    const int32_t pitch = width;
    int32_t columns = width;
    int32_t rows = height;
    intptr_t sourceOffset = 0;

    if (left < cx0)
    {
        const int32_t over = left - cx0; // negative
        if (fullSize != 0)
        {
            columns += over;
            sourceOffset += mirror != 0 ? over : -over;
        }
        else
        {
            columns += over * 2;
            sourceOffset += mirror != 0 ? over * 2 : -over * 2;
        }

        left = cx0;
    }

    if (top < cy0)
    {
        // OB-119: the asm stepped the source down by the (possibly already clipped) column count, not the sprite's
        // pitch, so a sprite clipped on the left and the top read its rows skewed.
        const int32_t over = top - cy0; // negative
        if (fullSize != 0)
        {
            rows += over;
            sourceOffset += static_cast<intptr_t>(-over) * pitch;
        }
        else
        {
            rows += over * 2;
            sourceOffset += static_cast<intptr_t>(-over) * pitch * 2;
        }

        top = cy0;
    }

    if (fullSize != 0)
    {
        if (left + columns > cx1 + 1)
        {
            columns = cx1 - left + 1;
        }

        if (rows + top > cy1 + 1)
        {
            rows = cy1 - top + 1;
        }
    }
    else
    {
        if ((columns >> 1) + left > cx1 + 1)
        {
            columns = (cx1 - left) * 2 + 2;
        }

        if ((rows >> 1) + top > cy1 + 1)
        {
            rows = (cy1 - top) * 2 + 2;
        }
    }

    // OB-116: the asm counted a sprite starting on the last column or row as outside.
    if (cx1 < cx0 || cy1 < cy0 || left > cx1 || top > cy1 || left <= cx0 - columns || top <= cy0 - rows ||
        columns <= 0 || rows <= 0)
    {
        return;
    }

    // Half size: every other pixel of every other row; nothing when that leaves none.
    if (fullSize == 0 && ((static_cast<uint32_t>(rows) >> 1) == 0 || (static_cast<uint32_t>(columns) >> 1) == 0))
    {
        return;
    }

    MCAlphaBlitCommand command;
    command.Sprite = sprite;
    command.Offset = sourceOffset;
    command.Pitch = pitch;
    command.Left = left;
    command.Top = top;
    command.Columns = columns;
    command.Rows = rows;
    command.Mirror = mirror != 0;
    command.FullSize = fullSize != 0;
    MCRenderer::For(pane->window).AlphaBlit(pane->window, command);
}

void AG_shape_fill(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY)
{
    MCAgDrawShape(pane, shapeTable, shapeNum, hotX, hotY, MCShapeOp::Fill, nullptr);
}

void AG_shape_translate_fill(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY)
{
    MCAgDrawShape(pane, shapeTable, shapeNum, hotX, hotY, MCShapeOp::XlatFill, lookaside);
}
