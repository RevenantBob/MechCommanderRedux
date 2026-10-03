#include "stdafx.h"
#include "platform/MCSoftwareRenderer.h"
#include "vfx/vfxint.h"

// The software renderer's run-length shapes (the row loops of vfxa.asm's shape routines and the game's
// vfx_translatedraw.cpp), fast shapes (fastshp.cpp) and terrain tiles (vfxtile.cpp).

namespace
{
    /// <summary>A blend through the game's alpha table: <paramref name="color"/> over <paramref name="screen"/>.</summary>
    uint8_t Blend(uint8_t color, uint8_t screen)
    {
        return static_cast<uint8_t>(AlphaTable[(static_cast<uint32_t>(color) << 8) | screen]);
    }

    /// <summary>Steps over one encoded row (up to and past its end token).</summary>
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
    /// </summary>
    /// <remarks>
    /// VFX's own routines had separate loops for rows needing no clipping, left clipping, right clipping and both;
    /// they write the same pixels as this loop (as the Draw and Xlat ops, unclipped) for every shape whose rows stay
    /// within its bounds. Port fix: a row running past its declared bounds is clipped here too.
    /// </remarks>
    template <MCShapeOp Op>
    const uint8_t* DrawRow(const uint8_t* data, uint8_t* row, int32_t x, int32_t lo, int32_t hi, const uint8_t* xlat)
    {
        constexpr bool fill = Op == MCShapeOp::Fill || Op == MCShapeOp::XlatFill;
        constexpr bool translate = Op == MCShapeOp::Xlat || Op == MCShapeOp::XlatFill;

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

                    if constexpr (Op == MCShapeOp::Alpha)
                    {
                        row[column] = Blend(pixel, row[column]);
                    }
                    else if constexpr (Op == MCShapeOp::XlatAlpha)
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
                    color = xlat[color];
                }

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

                    if constexpr (Op == MCShapeOp::Alpha)
                    {
                        row[column] = Blend(color, row[column]);
                    }
                    else if constexpr (Op == MCShapeOp::XlatAlpha)
                    {
                        // OB-114: the asm blended only a run's first pixel with its colour, the rest with colour 0.
                        row[column] = xlat[Blend(color, row[column])];
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

    template <MCShapeOp Op> void DrawRows(_window* target, const MCShapeCommand& command, const uint8_t* data)
    {
        const intptr_t stride = target->x_max + 1;
        uint8_t* row = target->buffer + command.Top * stride;

        for (int32_t rows = command.Rows; rows > 0; --rows, row += stride)
        {
            data = DrawRow<Op>(data, row, command.Left, command.Lo, command.Hi, command.Table);
        }
    }

    uint16_t Read16(const uint8_t* p)
    {
        uint16_t value;
        std::memcpy(&value, p, 2);
        return value;
    }

    /// <summary>The four fast-shape variants, each an inline-asm loop of its own in the original.</summary>
    enum class FastMode
    {
        Plain,
        Translate,
        Alpha,
        AlphaTranslate
    };

    /// <summary>Draws one fast-shape row: skips <paramref name="leftSkip"/> pixels, then draws until <paramref name="limit"/>.</summary>
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
                                // OB-118: the asm advanced the pointer before the store, so these pixels landed one
                                // to the right, and didn't skip colour 255.
                                for (int32_t i = 0; i < rem; ++i, ++dst)
                                {
                                    const uint8_t color = xlat[*data++];

                                    if (color != 0xff)
                                    {
                                        *dst = color;
                                    }
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
                                for (int32_t i = 0; i < rem; ++i, ++dst)
                                {
                                    const uint8_t color = xlat[*data++];

                                    if (color != 0xff)
                                    {
                                        *dst = Blend(color, *dst);
                                    }
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

    /// <summary>Where tile row <paramref name="row"/> starts, from the tile's start.</summary>
    uint32_t ReadOffset(const uint8_t* tile, int32_t row)
    {
        return static_cast<uint32_t>(MCVfxRead32(tile + 4 + static_cast<intptr_t>(row) * 4));
    }
}

void MCSoftwareRenderer::Shape(_window* target, const MCShapeCommand& command)
{
    const uint8_t* data = MCVfxShape(const_cast<void*>(command.ShapeTable), command.ShapeNum) + 0x18;

    for (int32_t i = 0; i < command.SkipRows; ++i)
    {
        data = SkipRow(data);
    }

    switch (command.Op)
    {
        case MCShapeOp::Draw:
            DrawRows<MCShapeOp::Draw>(target, command, data);
            break;
        case MCShapeOp::Alpha:
            DrawRows<MCShapeOp::Alpha>(target, command, data);
            break;
        case MCShapeOp::Xlat:
            DrawRows<MCShapeOp::Xlat>(target, command, data);
            break;
        case MCShapeOp::XlatAlpha:
            DrawRows<MCShapeOp::XlatAlpha>(target, command, data);
            break;
        case MCShapeOp::Fill:
            DrawRows<MCShapeOp::Fill>(target, command, data);
            break;
        case MCShapeOp::XlatFill:
            DrawRows<MCShapeOp::XlatFill>(target, command, data);
            break;
    }
}

void MCSoftwareRenderer::FastShape(_window* target, const MCFastShapeCommand& command)
{
    const intptr_t stride = target->x_max + 1;
    const uint8_t* shape = command.Shape;
    const FastMode mode = command.Alpha ? (command.Table != nullptr ? FastMode::AlphaTranslate : FastMode::Alpha)
                                        : (command.Table != nullptr ? FastMode::Translate : FastMode::Plain);
    uint8_t* row = target->buffer + command.Top * stride;

    // OB-117: the asm advanced the row-offset pointer only after reading it for the second row, so the first two rows
    // drawn both started at the first row's offset.
    for (int32_t r = command.FirstRow; r < command.EndRow; ++r)
    {
        const uint32_t offset = Read16(shape + 0xc + static_cast<intptr_t>(r) * 2);
        DrawFastRow(shape + offset, row, command.StartX, command.ClipX0, command.LeftSkip, command.Limit, command.Table,
                    mode);
        row += stride;
    }
}

void MCSoftwareRenderer::Tile(_window* target, const MCTileCommand& command)
{
    const intptr_t stride = target->x_max + 1;
    const uint8_t* tile = command.Tile;
    uint8_t* row = target->buffer + command.Top * stride;
    uint32_t offset = ReadOffset(tile, command.FirstRow);

    if (command.Unclipped)
    {
        row += command.Left;

        for (int32_t i = command.FirstRow; i < command.EndRow; ++i)
        {
            const uint32_t next = ReadOffset(tile, i + 1);
            const int32_t length = static_cast<int32_t>(next - offset);

            // Original behaviour: a translated span of length 1 (an x byte and no pixels) would have looped 2^32
            // times in the asm; tiles have none.
            if (length > 1)
            {
                WriteSpan(row + tile[offset], tile + offset + 1, length - 1, command.Table);
            }

            offset = next;
            row += stride;
        }

        return;
    }

    for (int32_t i = command.FirstRow; i < command.EndRow; ++i)
    {
        const uint32_t next = ReadOffset(tile, i + 1);
        const uint8_t* source = tile + offset;
        const int32_t first = *source++ + command.Left;
        int32_t count = static_cast<int32_t>(next - offset) - 1;
        const int32_t end = first + count;
        int32_t cut = 0;

        if (first < command.Lo)
        {
            cut = command.Lo - first;
            source += cut;
        }

        const int32_t overhang = end > command.Hi ? end - command.Hi - 1 : 0;
        count -= cut + overhang;

        if (count > 0)
        {
            WriteSpan(row + first + cut, source, count, command.Table);
        }

        offset = next;
        row += stride;
    }
}
