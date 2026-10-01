#include "stdafx.h"
#include "vfx/vfxint.h"
#include "vfx/mcagshape.h"

// The game's own shape drawers (mcx\vfx\vfx_translatedraw.cpp): VFX_shape_draw and VFX_shape_translate_draw
// rewritten by the MechCommander team in inline asm, adding translucent shapes blended through AlphaTable. Unlike
// VFX's routines they take window coordinates: the pane only clips.
//
// A shape is translucent when its data begins with the token pair 03 00 (a one-pixel literal of colour 0, which
// blends to the screen pixel unchanged); its pixels then become AlphaTable[shape << 8 | screen].

namespace
{
    /// <summary>A blend through the game's alpha table: <paramref name="shape"/> over <paramref name="screen"/>.</summary>
    inline uint8_t Blend(uint8_t shape, uint8_t screen)
    {
        return static_cast<uint8_t>(AlphaTable[(static_cast<uint32_t>(shape) << 8) | screen]);
    }

    /// <summary>Steps over one encoded row.</summary>
    const uint8_t* SkipRow(const uint8_t* data)
    {
        for (;;)
        {
            const uint8_t token = *data++;
            const uint32_t count = token >> 1;

            if (count != 0)
            {
                data += (token & 1) ? count : 1;
            }
            else if (token & 1)
            {
                ++data;
            }
            else
            {
                return data;
            }
        }
    }

    /// <summary>
    /// Draws one encoded row from window column <paramref name="x"/>, writing columns lo..hi of <paramref name="row"/>.
    /// <paramref name="clipped"/> is whether the asm took its clipped loop, which translated runs differently.
    /// </summary>
    template <MCAgPixelOp Op>
    const uint8_t* DrawRow(const uint8_t* data, uint8_t* row, int32_t x, int32_t lo, int32_t hi, bool clipped,
                           const uint8_t* xlat)
    {
        constexpr bool fill = Op == MCAgPixelOp::Fill || Op == MCAgPixelOp::XlatFill;
        constexpr bool translate = Op == MCAgPixelOp::Xlat || Op == MCAgPixelOp::XlatFill;

        for (;;)
        {
            const uint8_t token = *data++;
            const int32_t count = token >> 1;

            if (count == 0)
            {
                if ((token & 1) == 0)
                {
                    return data;
                }

                const int32_t skip = *data++;

                if constexpr (fill)
                {
                    // The fills write skipped pixels as colour 0.
                    for (int32_t i = 0; i < skip; ++i)
                    {
                        const int32_t column = x + i;

                        if (column < lo)
                        {
                            continue;
                        }

                        if (column > hi)
                        {
                            break;
                        }

                        row[column] = 0;
                    }
                }

                x += skip;
                continue;
            }

            if (token & 1)
            {
                for (int32_t i = 0; i < count; ++i)
                {
                    const int32_t column = x + i;

                    if (column < lo)
                    {
                        continue;
                    }

                    if (column > hi)
                    {
                        break;
                    }

                    const uint8_t pixel = data[i];

                    if constexpr (Op == MCAgPixelOp::Alpha)
                    {
                        row[column] = Blend(pixel, row[column]);
                    }
                    else if constexpr (Op == MCAgPixelOp::XlatAlpha)
                    {
                        row[column] = xlat[Blend(pixel, row[column])];
                    }
                    else if constexpr (translate)
                    {
                        row[column] = xlat[pixel];
                    }
                    else
                    {
                        row[column] = pixel;
                    }
                }

                data += count;
            }
            else
            {
                uint8_t color = *data++;

                if constexpr (translate)
                {
                    if (!clipped)
                    {
                        color = xlat[color];
                    }
                }

                uint8_t blendColor = color;

                for (int32_t i = 0; i < count; ++i)
                {
                    const int32_t column = x + i;

                    if (column < lo)
                    {
                        continue;
                    }

                    if (column > hi)
                    {
                        break;
                    }

                    if constexpr (Op == MCAgPixelOp::Alpha)
                    {
                        row[column] = Blend(color, row[column]);
                    }
                    else if constexpr (Op == MCAgPixelOp::XlatAlpha)
                    {
                        // Original behaviour: the asm cleared the colour's register after the first pixel, so only
                        // the first pixel of a translucent run blends with the run's colour, the rest with colour 0.
                        row[column] = xlat[Blend(blendColor, row[column])];
                        blendColor = 0;
                    }
                    else if constexpr (translate)
                    {
                        // Original behaviour: the clipped loop translates the colour in place before every pixel it
                        // writes, so a clipped run's pixels get the table applied once, twice, three times...
                        if (clipped)
                        {
                            color = xlat[color];
                        }

                        row[column] = color;
                    }
                    else
                    {
                        row[column] = color;
                    }
                }
            }

            x += count;
        }
    }

    const uint8_t* DrawRowOp(MCAgPixelOp op, const uint8_t* data, uint8_t* row, int32_t x, int32_t lo, int32_t hi,
                             bool clipped, const uint8_t* xlat)
    {
        switch (op)
        {
            case MCAgPixelOp::Draw:
                return DrawRow<MCAgPixelOp::Draw>(data, row, x, lo, hi, clipped, xlat);
            case MCAgPixelOp::Alpha:
                return DrawRow<MCAgPixelOp::Alpha>(data, row, x, lo, hi, clipped, xlat);
            case MCAgPixelOp::Xlat:
                return DrawRow<MCAgPixelOp::Xlat>(data, row, x, lo, hi, clipped, xlat);
            case MCAgPixelOp::XlatAlpha:
                return DrawRow<MCAgPixelOp::XlatAlpha>(data, row, x, lo, hi, clipped, xlat);
            case MCAgPixelOp::Fill:
                return DrawRow<MCAgPixelOp::Fill>(data, row, x, lo, hi, clipped, xlat);
            case MCAgPixelOp::XlatFill:
                return DrawRow<MCAgPixelOp::XlatFill>(data, row, x, lo, hi, clipped, xlat);
        }

        return data;
    }

    /// <summary>AG_shape_translate_draw's quirks for an opaque shape.</summary>
    constexpr MCAgShapeQuirks TranslateDrawQuirks{0, true, 1, true, false};
}

void MCAgDrawShape(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, MCAgPixelOp op,
                   const MCAgShapeQuirks& quirks, const uint8_t* xlat)
{
    const WINDOW* window = pane->window;
    const int32_t stride = window->x_max + 1;
    // The clip rectangle as the asm computed it: the right and bottom edges are capped at the window's width and
    // height, one past its last column and row.
    const int32_t cx0 = pane->x0 < 0 ? 0 : pane->x0;
    const int32_t cy0 = pane->y0 < 0 ? 0 : pane->y0;
    const int32_t cx1 = pane->x1 >= stride ? stride : pane->x1;
    const int32_t cy1 = pane->y1 >= window->y_max + 1 ? window->y_max + 1 : pane->y1;

    const uint8_t* header = MCVfxShape(shapeTable, shapeNum);
    const int32_t xMin = MCVfxRead32(header + 0x08);
    const int32_t yMin = MCVfxRead32(header + 0x0c);
    const int32_t xMax = MCVfxRead32(header + 0x10);
    const int32_t yMax = MCVfxRead32(header + 0x14);
    const uint8_t* data = header + 0x18;

    // Rows: skip those above the pane, then drop those below it. The asm compares these unsigned.
    int32_t top = yMin + hotY;
    uint32_t rows = static_cast<uint32_t>(yMax - yMin + 1);

    if (top < cy0)
    {
        const uint32_t skip = static_cast<uint32_t>(cy0 - top);

        if (rows <= skip)
        {
            return;
        }

        rows -= skip;

        for (uint32_t i = 0; i < skip; ++i)
        {
            data = SkipRow(data);
        }

        top = cy0;
    }

    const uint32_t overhang =
        static_cast<uint32_t>(top) + rows - (quirks.BottomInclusive ? 1u : 0u) - static_cast<uint32_t>(cy1);
    const bool bottomClipped =
        static_cast<uint32_t>(top) + rows - (quirks.BottomInclusive ? 1u : 0u) > static_cast<uint32_t>(cy1);
    MCAgShapeQuirks horizontal = quirks;

    if (bottomClipped)
    {
        if (rows <= overhang)
        {
            return;
        }

        rows -= overhang;

        if (op == MCAgPixelOp::XlatAlpha)
        {
            // Original behaviour: the translucent branch of AG_shape_translate_draw jumps into the opaque branch's
            // code when it clips the bottom, so such a shape is drawn translated but not blended.
            op = MCAgPixelOp::Xlat;
            horizontal = TranslateDrawQuirks;
        }
    }

    // Columns: reject, or choose the clipped or unclipped loop, with the routine's own arithmetic.
    const int32_t left = xMin + hotX + horizontal.XShift;
    const uint32_t lastOffset = static_cast<uint32_t>(xMax - xMin);
    bool clipped;

    if (left < cx0)
    {
        if (lastOffset <= static_cast<uint32_t>(cx0 - left))
        {
            return;
        }

        clipped = true;
    }
    else
    {
        const uint32_t right = lastOffset + static_cast<uint32_t>(left) - static_cast<uint32_t>(horizontal.RightBias);
        const bool over =
            horizontal.ClipOnEqual ? right >= static_cast<uint32_t>(cx1) : right > static_cast<uint32_t>(cx1);

        if (over)
        {
            // Original behaviour: the test compares the left column (not the width) with the overhang, so a shape
            // starting near column 0 that sticks out past the pane's right edge by at least its left column isn't
            // drawn at all.
            if (static_cast<uint32_t>(left) <= right - static_cast<uint32_t>(cx1))
            {
                return;
            }

            clipped = true;
        }
        else
        {
            clipped = false;
        }
    }

    int32_t lo = 0;
    int32_t hi = stride - 1;

    if (clipped)
    {
        lo = cx0;
        hi = horizontal.ClipRightExclusive ? cx1 - 1 : cx1;
    }

    // Port fix: the asm let the right edge reach column x_max + 1 (the next row's first pixel) and the bottom row
    // y_max + 1 (past the buffer); the port stops at the window's edges.
    hi = std::min(hi, stride - 1);

    uint8_t* buffer = window->buffer;

    for (int32_t y = top; rows != 0; --rows, ++y)
    {
        if (y > window->y_max)
        {
            break;
        }

        data = DrawRowOp(op, data, buffer + static_cast<intptr_t>(y) * stride, left, lo, hi, clipped, xlat);
    }
}

void AG_shape_draw(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY)
{
    if (MCAgShapeIsAlpha(shapeTable, shapeNum))
    {
        // Original behaviour: translucent shapes are drawn one column to the right, and their bottom clip stops a
        // row short of the pane's last row.
        MCAgDrawShape(pane, shapeTable, shapeNum, hotX, hotY, MCAgPixelOp::Alpha, {1, false, 0, false, false}, nullptr);
    }
    else
    {
        MCAgDrawShape(pane, shapeTable, shapeNum, hotX, hotY, MCAgPixelOp::Draw, {0, true, 0, false, false}, nullptr);
    }
}

void AG_shape_lookaside(uint8_t* table)
{
    lookaside = table;
}

void AG_shape_translate_draw(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY)
{
    // The clipped loop also checked, after every skip, that the destination pointer's top nibble was 8 (the Win9x
    // shared arena DirectDraw surfaces were mapped in) and gave up on the shape otherwise. The port always
    // continues, as on the video surfaces the check was written for.
    if (MCAgShapeIsAlpha(shapeTable, shapeNum))
    {
        MCAgDrawShape(pane, shapeTable, shapeNum, hotX, hotY, MCAgPixelOp::XlatAlpha, {0, true, 1, false, false},
                      lookaside);
    }
    else
    {
        MCAgDrawShape(pane, shapeTable, shapeNum, hotX, hotY, MCAgPixelOp::Xlat, TranslateDrawQuirks, lookaside);
    }
}
