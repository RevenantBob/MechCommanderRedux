#include "stdafx.h"
#include "vfx/vfxint.h"

// mcx\vfx\vfx_map_polygon.cpp: despite the name, the game's status bars, its pixel write and a transparent window
// blit. (Inline assembly in the original.)

void AG_StatusBar(PANE* pane, int x0, int y0, int x1, int y1, int alphaColor, int barLength)
{
    const WINDOW* window = pane->window;
    int32_t clipX0 = std::max(pane->x0, 0);
    int32_t clipY0 = std::max(pane->y0, 0);
    int32_t clipX1 = pane->x1 < window->x_max + 1 ? pane->x1 : window->x_max;
    int32_t clipY1 = pane->y1 < window->y_max + 1 ? pane->y1 : window->y_max;
    MCClipToView(window, clipX0, clipY0, clipX1, clipY1);

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

    // The frame (the box's top and bottom rows between the corners, its left and right columns) is darkened through
    // AlphaTable row 0x108 (0x008011d0 in MCX.EXE), the bar through the alpha colour's row.
    MCStatusBarCommand command;
    command.Box = MCRect{x0, y0, x1, y1};
    command.FrameTop = topRow;
    command.FrameBottom = bottomRow;
    command.BarLength = barLength;
    command.AlphaColor = alphaColor;
    MCRenderer::For(pane->window).StatusBar(pane->window, command);
}

void AG_pixel_write(PANE* pane, int32_t x, int32_t y, uint32_t color)
{
    MCVfxClip clip;

    if (MCVfxClipPane(pane, clip) != 0)
    {
        return;
    }

    x += pane->x0;
    y += pane->y0;

    // OB-120: the asm wrote only strictly inside the pane's rectangle, and didn't check the window's size.
    if (x < clip.X0 || x > clip.X1 || y < clip.Y0 || y > clip.Y1)
    {
        return;
    }

    MCRenderer::For(pane->window).Pixel(pane->window, x, y, static_cast<uint8_t>(color));
}

int32_t DrawTransparent(PANE* pane, WINDOW* texture, int x, int y, int width, int height)
{
    const WINDOW* window = pane->window;
    const int32_t stride = window->x_max + 1; // 0x00802408
    int32_t clipX0 = std::max(pane->x0, 0);
    int32_t clipY0 = std::max(pane->y0, 0);
    int32_t clipX1 = stride <= pane->x1 ? window->x_max : pane->x1;
    int32_t clipY1 = pane->y1 < window->y_max + 1 ? pane->y1 : window->y_max;
    MCClipToView(window, clipX0, clipY0, clipX1, clipY1);

    // OB-115: the asm offset by the pane's origin clipped to the window. OB-116: and counted a picture starting on
    // the last column or row as outside.
    int32_t dx = x + pane->x0;
    int32_t dy = y + pane->y0;

    if (clipX1 < clipX0 || clipY1 < clipY0 || dx > clipX1 || dy > clipY1 || clipX0 - width >= dx ||
        clipY0 - height >= dy)
    {
        return 1;
    }

    // Where in the texture the copy starts.
    int32_t sourceX = 0;
    int32_t sourceY = 0;

    if (dx < clipX0)
    {
        width += dx - clipX0;
        sourceX = clipX0 - dx;
        dx = clipX0;
    }

    if (dy < clipY0)
    {
        height += dy - clipY0;
        sourceY = clipY0 - dy;
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

    // Colour 255 is transparent. (The original copied 8 (MMX) or 4 bytes at a time with byte masks; the pixels
    // written are the same.)
    MCCopyCommand command;
    command.Source = texture;
    command.SourceRect = MCRect{sourceX, sourceY, sourceX + width - 1, sourceY + height - 1};
    command.X = dx;
    command.Y = dy;
    command.ColorKey = true;
    command.Key = 0xff;
    command.Downwards = true;
    command.Rightwards = true;
    MCRenderer::For(pane->window).Copy(pane->window, command);
    return 0;
}
