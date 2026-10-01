#include "stdafx.h"
#include "vfx/vfxint.h"

// The game's "fast shape" drawer (mcx\vfx\fastshp.cpp, inline assembly), used for the terrain overlay tiles. The
// format is its own, not VFX's:
//
//   table:  +0 "DNAH", then at +8 + 4n the offset of shape n from the table
//   shape:  +4 int16 hot spot x, +6 int16 hot spot y,
//           +8 uint16 height (rows), +10 uint16 width (pixels per row, less one),
//           +0xc uint16 row offsets from the shape
//   row:    packets until width + 1 pixels are covered: a byte c < 0x80 is a run of c pixels of the colour in the
//           next byte (255: transparent); c >= 0x80 is c - 0x80 literal pixels following.
//
// A shape whose first row starts with the 16-bit word 1 is translucent: every pixel is blended through AlphaTable.

namespace
{
    uint16_t Read16(const uint8_t* p)
    {
        uint16_t value;
        std::memcpy(&value, p, 2);
        return value;
    }

    /// <summary>The four drawing variants, each an inline-asm loop of its own in the original.</summary>
    enum class FastMode
    {
        Plain,
        Translate,
        Alpha,
        AlphaTranslate
    };

    uint8_t Blend(uint8_t color, uint8_t background)
    {
        return static_cast<uint8_t>(AlphaTable[(static_cast<uint32_t>(color) << 8) | background]);
    }

    /// <summary>Draws one row: skips <paramref name="leftSkip"/> pixels, then draws until <paramref name="limit"/>.</summary>
    void DrawFastRow(const uint8_t* data, uint8_t* row, int32_t startX, int32_t clipX0, int32_t leftSkip, int32_t limit,
                     const uint8_t* xlat, FastMode mode)
    {
        int32_t count = 0;
        uint8_t* dst;

        if (leftSkip > 0)
        {
            // Step over the clipped-off pixels; the packet that crosses the edge is drawn from the pane's left edge.
            for (;;)
            {
                const uint32_t c = *data;

                if (c >= 0x80)
                {
                    const int32_t n = static_cast<int32_t>(c) - 0x80;
                    ++data;
                    count += n;

                    if (count > leftSkip)
                    {
                        const int32_t rem = count - leftSkip;
                        count = rem;
                        data += n - rem;
                        dst = row + clipX0;

                        switch (mode)
                        {
                            case FastMode::Plain:
                            {
                                std::memcpy(dst, data, static_cast<size_t>(rem));
                                data += rem;
                                dst += rem;
                                break;
                            }
                            case FastMode::Translate:
                            {
                                // Original behaviour: the pointer is advanced before the store, so these pixels land one
                                // to the right, and colour 255 isn't skipped.
                                for (uint8_t left = static_cast<uint8_t>(rem); left != 0; --left)
                                {
                                    const uint8_t texel = *data++;
                                    ++dst;
                                    *dst = xlat[texel];
                                }
                                break;
                            }
                            case FastMode::Alpha:
                            {
                                for (int32_t i = 0; i < rem; ++i, ++dst)
                                {
                                    *dst = Blend(*data++, *dst);
                                }
                                break;
                            }
                            case FastMode::AlphaTranslate:
                            {
                                // Original behaviour: shifted one to the right, as in the translated variant.
                                for (uint8_t left = static_cast<uint8_t>(rem); left != 0; --left)
                                {
                                    const uint8_t texel = *data++;
                                    ++dst;
                                    *dst = Blend(xlat[texel], *dst);
                                }
                                break;
                            }
                        }
                        break;
                    }

                    data += n;
                }
                else
                {
                    count += static_cast<int32_t>(c);
                    ++data;

                    if (count > leftSkip)
                    {
                        const int32_t rem = count - leftSkip;
                        count = rem;
                        uint8_t color = *data++;

                        if (mode == FastMode::Translate || mode == FastMode::AlphaTranslate)
                        {
                            color = xlat[color];
                        }

                        dst = row + clipX0;

                        if (color == 0xff)
                        {
                            dst += rem;
                        }
                        else if (mode == FastMode::Plain || mode == FastMode::Translate)
                        {
                            std::memset(dst, color, static_cast<size_t>(rem));
                            dst += rem;
                        }
                        else
                        {
                            for (int32_t i = 0; i < rem; ++i, ++dst)
                            {
                                *dst = Blend(color, *dst);
                            }
                        }
                        break;
                    }

                    ++data;
                }
            }
        }
        else
        {
            dst = row + startX;
        }

        while (count != limit)
        {
            const uint32_t c = *data++;

            if (c >= 0x80)
            {
                int32_t n = static_cast<int32_t>(c) - 0x80;
                count += n;

                if (count > limit)
                {
                    n -= count - limit;
                    count = limit;
                }

                // Port fix: the plain and alpha loops were do-whiles that ran away on an empty literal (0x80).
                switch (mode)
                {
                    case FastMode::Plain:
                    {
                        for (int32_t i = 0; i < n; ++i)
                        {
                            *dst++ = *data++;
                        }
                        break;
                    }
                    case FastMode::Translate:
                    {
                        for (int32_t i = 0; i < n; ++i)
                        {
                            const uint8_t color = xlat[*data++];
                            ++dst;

                            if (color != 0xff)
                            {
                                dst[-1] = color;
                            }
                        }
                        break;
                    }
                    case FastMode::Alpha:
                    {
                        for (int32_t i = 0; i < n; ++i, ++dst)
                        {
                            *dst = Blend(*data++, *dst);
                        }
                        break;
                    }
                    case FastMode::AlphaTranslate:
                    {
                        for (int32_t i = 0; i < n; ++i)
                        {
                            const uint8_t color = xlat[*data++];
                            ++dst;

                            if (color != 0xff)
                            {
                                dst[-1] = Blend(color, dst[-1]);
                            }
                        }
                        break;
                    }
                }
            }
            else
            {
                int32_t n = static_cast<int32_t>(c);
                count += n;

                if (count > limit)
                {
                    n -= count - limit;
                    count = limit;
                }

                uint8_t color = *data++;

                if (mode == FastMode::Translate || mode == FastMode::AlphaTranslate)
                {
                    color = xlat[color];
                }

                if (color == 0xff)
                {
                    dst += n;
                }
                else if (mode == FastMode::Plain || mode == FastMode::Translate)
                {
                    std::memset(dst, color, static_cast<size_t>(n));
                    dst += n;
                }
                else
                {
                    for (int32_t i = 0; i < n; ++i, ++dst)
                    {
                        *dst = Blend(color, *dst);
                    }
                }
            }
        }
    }
}

int32_t fastShapeDraw(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, uint8_t* xlat,
                      int unused)
{
    (void)unused;
    uint8_t* table = static_cast<uint8_t*>(shapeTable);
    const uint8_t* shape = table + MCVfxRead32(table + 8 + static_cast<intptr_t>(shapeNum) * 4);
    const uint32_t dims = static_cast<uint32_t>(MCVfxRead32(shape + 8));
    const int32_t height = static_cast<int32_t>(dims & 0xffff);
    const int32_t width = static_cast<int32_t>(dims >> 16);

    const WINDOW* window = pane->window;
    const int32_t stride = window->x_max + 1;
    const int32_t clipX1 = std::min(pane->x1, window->x_max);
    const int32_t clipY1 = std::min(pane->y1, window->y_max);
    const int32_t clipX0 = std::max(pane->x0, 0);
    const int32_t clipY0 = std::max(pane->y0, 0);

    // Original behaviour: the position is offset by the pane's origin clipped to the window, not the pane's own.
    const int32_t sx = clipX0 + hotX - static_cast<int16_t>(Read16(shape + 4));
    const int32_t sy = clipY0 + hotY - static_cast<int16_t>(Read16(shape + 6));

    // Original behaviour: a shape starting on the last column or row counts as outside.
    if (sx >= clipX1 || sy >= clipY1 || sx <= clipX0 - width || sy <= clipY0 - height)
    {
        return 0;
    }

    int32_t leftSkip = 0;
    int32_t span = width;

    if (sx < clipX0)
    {
        leftSkip = clipX0 - sx;
        span = width - leftSkip;
        // Port fix: the original clipped only the left of a shape wider than the pane on both sides, running the
        // rows on past the right edge (and, on the window's last row, past its buffer).
        span = std::min(span, clipX1 - clipX0);
    }
    else if (sx + width > clipX1)
    {
        span = width - (sx + width - clipX1);
    }

    const int32_t limit = span + 1;

    uint8_t* row = window->buffer + static_cast<intptr_t>(std::max(sy, clipY0)) * stride;
    const int32_t firstRow = sy < clipY0 ? clipY0 - sy : 0;
    const int32_t endRow = height + sy > clipY1 ? clipY1 - sy + 1 : height;

    const bool alpha = Read16(shape + Read16(shape + 0xc)) == 1;
    const FastMode mode = alpha ? (xlat != nullptr ? FastMode::AlphaTranslate : FastMode::Alpha)
                                : (xlat != nullptr ? FastMode::Translate : FastMode::Plain);

    // Original behaviour: the row-offset pointer is only advanced after it is read for the second row, so the first
    // two rows drawn both start at the first row's offset.
    const uint8_t* nextOffset = shape + 0xc + static_cast<intptr_t>(firstRow) * 2;
    uint32_t offset = Read16(nextOffset);

    for (int32_t r = firstRow; r < endRow; ++r)
    {
        DrawFastRow(shape + offset, row, sx, clipX0, leftSkip, limit, xlat, mode);
        row += stride;
        offset = Read16(nextOffset);
        nextOffset += 2;
    }

    return 0;
}
