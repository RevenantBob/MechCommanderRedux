#include "stdafx.h"
#include "vfx/vfxint.h"

// The terrain tile drawer (mcx\vfx\vfxtile.cpp). A tile is a diamond of opaque pixels stored as one span per row:
//
//   +0  uint8   hot spot x (subtracted from the draw position)
//   +1  uint8   hot spot y
//   +2  uint8   height (rows)
//   +3  uint8   width (columns of the bounding box)
//   +4  uint32  offset[height + 1]: where each row starts, from the tile's start; the last is the end of the data
//   row i, at offset[i]: uint8 x (the span's first column), then offset[i + 1] - offset[i] - 1 pixels
//
// A row of length 0 is empty.

int32_t VFX_nTile_draw(PANE* pane, uint8_t* tile, int32_t x, int32_t y, uint8_t* xlat)
{
    const WINDOW* window = pane->window;
    const int32_t stride = window->x_max + 1;
    int32_t cx0 = pane->x0 < 0 ? 0 : pane->x0;
    int32_t cy0 = pane->y0 < 0 ? 0 : pane->y0;
    int32_t cx1 = pane->x1 < stride ? pane->x1 : window->x_max;
    int32_t cy1 = pane->y1 < window->y_max + 1 ? pane->y1 : window->y_max;
    MCClipToView(window, cx0, cy0, cx1, cy1);

    const int32_t height = tile[2];
    const int32_t width = tile[3];
    // OB-115: (x, y) are relative to the pane's origin; the asm offset by its corner clipped to the window. OB-116:
    // and counted a tile starting on the last column or row as outside.
    const int32_t left = pane->x0 - tile[0] + x;
    const int32_t top = pane->y0 - tile[1] + y;

    if (cx1 < cx0 || cy1 < cy0 || left > cx1 || top > cy1 || left <= cx0 - width || top <= cy0 - height)
    {
        return static_cast<int32_t>(0xcdcf0001);
    }

    const int32_t skip = top < cy0 ? cy0 - top : 0;
    const int32_t rows = height + top > cy1 ? cy1 - top + 1 : height;

    if (skip >= rows)
    {
        return 0;
    }

    MCTileCommand command;
    command.Tile = tile;
    command.Left = left;
    command.Top = top > cy0 ? top : cy0;
    command.FirstRow = skip;
    command.EndRow = rows;
    command.Lo = cx0;
    command.Hi = cx1;
    // Wholly inside horizontally: spans are written without clipping.
    command.Unclipped = left > cx0 && left + width < cx1;
    command.Table = xlat;
    MCRenderer::For(pane->window).Tile(pane->window, command);
    return 0;
}
