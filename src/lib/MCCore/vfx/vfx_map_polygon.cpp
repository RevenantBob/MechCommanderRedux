#include "stdafx.h"
#include "vfx/vfxint.h"

// mcx\vfx\vfx_map_polygon.cpp: despite the name, the game's status bars, its pixel write and a transparent window
// blit. (Inline assembly in the original.)

namespace
{
    /// <summary>The AlphaTable row status-bar frames are darkened through (0x008011d0 in MCX.EXE).</summary>
    constexpr int32_t STATUS_FRAME_ALPHA = 0x108;
}

void AG_StatusBar(PANE* pane, int x0, int y0, int x1, int y1, int alphaColor, int barLength)
{
    const WINDOW* window = pane->window;
    const int32_t stride = window->x_max + 1; // 0x00802408
    const int32_t clipX0 = std::max(pane->x0, 0);
    const int32_t clipY0 = std::max(pane->y0, 0);
    const int32_t clipX1 = pane->x1 < window->x_max + 1 ? pane->x1 : window->x_max;
    const int32_t clipY1 = pane->y1 < window->y_max + 1 ? pane->y1 : window->y_max;

    if (x0 > x1)
    {
        std::swap(x0, x1);
        barLength = -barLength;
    }

    if (y0 > y1)
    {
        std::swap(y0, y1);
    }

    // The frame rows are the box's own top and bottom: once clipped away, no row gets the frame.
    const int32_t topRow = y0;    // 0x0080240c
    const int32_t bottomRow = y1; // 0x008023fc

    if (x0 >= clipX1 || y0 >= clipY1 || x1 <= clipX0 || y1 <= clipY0)
    {
        return;
    }

    if (x0 < clipX0)
    {
        barLength += x0 - clipX0;
        x0 = clipX0;
    }

    if (y0 < clipY0)
    {
        y0 = clipY0;
    }

    if (x1 > clipX1)
    {
        x1 = clipX1;
    }

    if (y1 > clipY1)
    {
        y1 = clipY1;
    }

    if (x0 + barLength >= x1)
    {
        barLength = x1 - x0 - 2;
    }

    if (barLength < 0)
    {
        barLength = 0;
    }

    const int32_t width = x1 - x0;

    if (width < 3)
    {
        return;
    }

    const uint8_t* frame = reinterpret_cast<const uint8_t*>(AlphaTable) + STATUS_FRAME_ALPHA * 256;
    const uint8_t* fill = reinterpret_cast<const uint8_t*>(AlphaTable) + static_cast<intptr_t>(alphaColor) * 256;
    uint8_t* p = window->buffer + static_cast<intptr_t>(y0) * stride + x0;

    for (int32_t y = y0; y <= y1; ++y, p += stride)
    {
        if (y == topRow || y == bottomRow)
        {
            // The frame's top or bottom: the pixels between the corners.
            for (int32_t i = 1; i < width; ++i)
            {
                p[i] = frame[p[i]];
            }

            continue;
        }

        p[0] = frame[p[0]];
        p[width] = frame[p[width]];

        if (barLength != 0)
        {
            for (int32_t i = 1; i <= barLength + 1; ++i)
            {
                p[i] = fill[p[i]];
            }
        }
    }
}

void AG_pixel_write(PANE* pane, int32_t x, int32_t y, uint32_t color)
{
    x += pane->x0;
    y += pane->y0;

    // Original behaviour: strictly inside the pane's rectangle; the window's size isn't checked.
    if (x <= pane->x0 || x >= pane->x1 || y <= pane->y0 || y >= pane->y1)
    {
        return;
    }

    const WINDOW* window = pane->window;
    window->buffer[static_cast<intptr_t>(window->x_max + 1) * y + x] = static_cast<uint8_t>(color);
}

int32_t DrawTransparent(PANE* pane, WINDOW* texture, int x, int y, int width, int height)
{
    const WINDOW* window = pane->window;
    const int32_t stride = window->x_max + 1; // 0x00802408
    const int32_t clipX0 = std::max(pane->x0, 0);
    const int32_t clipY0 = std::max(pane->y0, 0);
    const int32_t clipX1 = stride <= pane->x1 ? window->x_max : pane->x1;
    const int32_t clipY1 = pane->y1 < window->y_max + 1 ? pane->y1 : window->y_max;

    // Original behaviour: offset by the pane's origin clipped to the window, not the pane's own.
    int32_t dx = x + clipX0;
    int32_t dy = y + clipY0;

    if (dx >= clipX1 || dy >= clipY1 || clipX0 - width >= dx || clipY0 - height >= dy)
    {
        return 1;
    }

    const int32_t textureStride = texture->x_max + 1; // 0x00802410
    const uint8_t* src = texture->buffer;

    if (dx < clipX0)
    {
        width += dx - clipX0;
        src += clipX0 - dx;
        dx = clipX0;
    }

    if (dy < clipY0)
    {
        height += dy - clipY0;
        src += static_cast<intptr_t>(clipY0 - dy) * textureStride;
        dy = clipY0;
    }

    if (clipX1 + 1 < dx + width)
    {
        width = clipX1 - dx + 1;
    }

    if (clipY1 + 1 < dy + height)
    {
        height = clipY1 - dy + 1;
    }

    // The original copied 8 (MMX) or 4 bytes at a time with byte masks; the pixels written are the same.
    uint8_t* dst = window->buffer + static_cast<intptr_t>(dy) * stride + dx;

    do
    {
        for (int32_t i = 0; i < width; ++i)
        {
            if (src[i] != 0xff)
            {
                dst[i] = src[i];
            }
        }

        dst += stride;
        src += textureStride;
    } while (--height != 0);

    return 0;
}
