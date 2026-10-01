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

namespace
{
    /// <summary>Writes <paramref name="count"/> pixels of a tile span: copied, translated, or the fill colour.</summary>
    void WriteSpan(uint8_t* destination, const uint8_t* source, int32_t count, const uint8_t* xlat)
    {
        if (xlat == VFX_TILE_FILL)
        {
            std::memset(destination, 0x10, static_cast<size_t>(count));
        }
        else if (xlat == nullptr)
        {
            std::memcpy(destination, source, static_cast<size_t>(count));
        }
        else
        {
            for (int32_t i = 0; i < count; ++i)
            {
                destination[i] = xlat[source[i]];
            }
        }
    }

    uint32_t ReadOffset(const uint8_t* tile, int32_t row)
    {
        return static_cast<uint32_t>(MCVfxRead32(tile + 4 + static_cast<intptr_t>(row) * 4));
    }
}

int32_t VFX_nTile_draw(PANE* pane, uint8_t* tile, int32_t x, int32_t y, uint8_t* xlat)
{
    const WINDOW* window = pane->window;
    const int32_t stride = window->x_max + 1;
    const int32_t cx0 = pane->x0 < 0 ? 0 : pane->x0;
    const int32_t cy0 = pane->y0 < 0 ? 0 : pane->y0;
    const int32_t cx1 = pane->x1 < stride ? pane->x1 : window->x_max;
    const int32_t cy1 = pane->y1 < window->y_max + 1 ? pane->y1 : window->y_max;

    const int32_t height = tile[2];
    const int32_t width = tile[3];
    // (x, y) are relative to the clipped pane's corner.
    const int32_t left = cx0 - tile[0] + x;
    const int32_t top = cy0 - tile[1] + y;

    if (left >= cx1 || top >= cy1 || left <= cx0 - width || top <= cy0 - height)
    {
        return static_cast<int32_t>(0xcdcf0001);
    }

    uint8_t* row = window->buffer + static_cast<intptr_t>(stride) * (top > cy0 ? top : cy0);
    const int32_t skip = top < cy0 ? cy0 - top : 0;
    const int32_t rows = height + top > cy1 ? cy1 - top + 1 : height;

    if (left > cx0 && left + width < cx1)
    {
        // Wholly inside horizontally: spans are written without clipping.
        row += left;
        uint32_t offset = ReadOffset(tile, skip);

        for (int32_t i = skip; i < rows; ++i)
        {
            const uint32_t next = ReadOffset(tile, i + 1);
            const int32_t length = static_cast<int32_t>(next - offset);

            if (length != 0)
            {
                // Original behaviour: a translated span of length 1 (an x byte and no pixels) would have looped
                // 2^32 times in the asm; tiles have none.
                if (length > 1)
                {
                    WriteSpan(row + tile[offset], tile + offset + 1, length - 1, xlat);
                }
            }

            offset = next;
            row += stride;
        }

        return 0;
    }

    if (skip >= rows)
    {
        return 0;
    }

    uint32_t offset = ReadOffset(tile, skip);

    for (int32_t i = skip; i < rows; ++i)
    {
        const uint32_t next = ReadOffset(tile, i + 1);
        const uint8_t* source = tile + offset;
        const int32_t first = *source++ + left;
        int32_t count = static_cast<int32_t>(next - offset) - 1;
        const int32_t end = first + count;
        int32_t cut = 0;

        if (first < cx0)
        {
            cut = cx0 - first;
            source += cut;
        }

        const int32_t overhang = end > cx1 ? end - cx1 - 1 : 0;
        count -= cut + overhang;

        if (count > 0)
        {
            WriteSpan(row + first + cut, source, count, xlat);
        }

        offset = next;
        row += stride;
    }

    return 0;
}
