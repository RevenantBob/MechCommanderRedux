#include "stdafx.h"
#include "vfx/vfxnshape.h"
#include "vfx/alphapalette.h"

namespace
{
    /// <summary>A literal token: 0x80 plus its length.</summary>
    inline constexpr uint8_t LITERAL_TOKEN = 0x80;
    /// <summary>The colour of a skip.</summary>
    inline constexpr uint8_t SKIP_COLOR = 0xff;
    /// <summary>The first row's start that marks a translucent shape: a run of one pixel of colour 0.</summary>
    inline constexpr uint16_t ALPHA_MARK = 1;

    uint16_t read16(const uint8_t* p)
    {
        uint16_t value;
        std::memcpy(&value, p, sizeof(value));
        return value;
    }

    /// <summary>One row's drawing: the pixel operation of the shape's kind and where the buffer ends.</summary>
    class RowWriter
    {
    public:
        RowWriter(uint8_t* buffer, size_t size, const uint8_t* xlat, bool alpha)
            : _begin(buffer), _end(buffer + size), _xlat(xlat), _alpha(alpha)
        {
        }

        /// <summary>A run of <paramref name="count"/> pixels of <paramref name="color"/> at <paramref name="out"/>.</summary>
        /// <returns>The pixel after the run.</returns>
        uint8_t* run(uint8_t* out, uint8_t color, int32_t count) const
        {
            const uint8_t mapped = _xlat != nullptr ? _xlat[color] : color;

            if (mapped == SKIP_COLOR)
            {
                return out + count;
            }

            for (int32_t i = 0; i < count; i++)
            {
                put(out + i, mapped);
            }

            return out + count;
        }

        /// <summary>A literal of <paramref name="count"/> colours from <paramref name="pixels"/> at <paramref name="out"/>.</summary>
        /// <returns>The pixel after the literal.</returns>
        uint8_t* literal(uint8_t* out, const uint8_t* pixels, int32_t count) const
        {
            for (int32_t i = 0; i < count; i++)
            {
                if (_xlat == nullptr)
                {
                    put(out + i, pixels[i]);
                }
                else if (_xlat[pixels[i]] != SKIP_COLOR)
                {
                    put(out + i, _xlat[pixels[i]]);
                }
            }

            return out + count;
        }

        /// <summary>
        /// The end of a literal cut by the left edge: copied as it is without a table; with one, every colour mapped
        /// (skips too) and written a pixel to the right.
        /// </summary>
        uint8_t* cutLiteral(uint8_t* out, const uint8_t* pixels, int32_t count) const
        {
            if (_xlat == nullptr)
            {
                return literal(out, pixels, count);
            }

            for (int32_t i = 0; i < count; i++)
            {
                put(out + i + 1, _xlat[pixels[i]]);
            }

            return out + count;
        }

    private:
        /// <summary>
        /// Writes <paramref name="color"/> at <paramref name="out"/>: blended with what is there for a translucent
        /// shape.
        /// </summary>
        void put(uint8_t* out, uint8_t color) const
        {
            // The original can write past the buffer (a shape cut on the left isn't cut on the right); the port drops
            // those writes.
            if (out < _begin || out >= _end)
            {
                return;
            }

            *out = _alpha ? static_cast<uint8_t>(AlphaTable[static_cast<size_t>(color) << 8 | *out]) : color;
        }

        uint8_t* _begin;
        uint8_t* _end;
        const uint8_t* _xlat;
        bool _alpha;
    };
}

uint32_t VFX_nShape_draw(uint8_t* buffer, int32_t width, int32_t height, const uint8_t* table, int32_t shapeNum,
                         int32_t x, int32_t y, const uint8_t* xlat)
{
    // The editor's pane is the whole buffer.
    const int32_t clipX0 = 0;
    const int32_t clipY0 = 0;
    const int32_t clipX1 = width - 1;
    const int32_t clipY1 = height - 1;
    uint32_t shapeOffset;
    std::memcpy(&shapeOffset, table + 8 + static_cast<ptrdiff_t>(shapeNum) * 4, sizeof(shapeOffset));
    const uint8_t* shape = table + shapeOffset;
    const int32_t rows = read16(shape + 8);
    const int32_t columns = read16(shape + 10);
    const int32_t left = clipX0 + x - static_cast<int16_t>(read16(shape + 4));
    const int32_t top = clipY0 + y - static_cast<int16_t>(read16(shape + 6));

    if (left >= clipX1 || top >= clipY1 || left <= clipX0 - columns || top <= clipY0 - rows)
    {
        return 0;
    }

    int32_t skipX = 0;
    int32_t visible = columns;

    if (left < clipX0)
    {
        skipX = clipX0 - left;
        visible = columns - skipX;
    }
    else if (left + columns > clipX1)
    {
        visible = columns - (left + columns - clipX1);
    }

    // Original behaviour: a row ends one pixel past the shape's width.
    const int32_t limit = visible + 1;
    uint8_t* rowStart = buffer + static_cast<ptrdiff_t>((std::max)(top, clipY0)) * width;
    const int32_t skipRows = top < clipY0 ? clipY0 - top : 0;
    const int32_t rowEnd = rows + top > clipY1 ? clipY1 - top + 1 : rows;
    const uint8_t* nextOffset = shape + 12 + static_cast<ptrdiff_t>(skipRows) * 2;
    // Original behaviour: the first row's offset is read again for the second row.
    uint32_t rowOffset = read16(nextOffset);
    const bool alpha = read16(shape + read16(shape + 12)) == ALPHA_MARK;
    const RowWriter writer(buffer, static_cast<size_t>(width) * height, xlat, alpha);

    for (int32_t r = skipRows; r < rowEnd; r++)
    {
        const uint8_t* data = shape + rowOffset;
        int32_t done = 0;
        uint8_t* out = rowStart + left;

        if (skipX > 0)
        {
            // Past the tokens left of the edge, then the rest of the one it cuts.
            int32_t position = 0;

            for (;;)
            {
                const uint8_t token = *data;

                if (token >= LITERAL_TOKEN)
                {
                    const int32_t length = token - LITERAL_TOKEN;
                    data++;
                    position += length;

                    if (position > skipX)
                    {
                        done = position - skipX;
                        out = writer.cutLiteral(rowStart + clipX0, data + length - done, done);
                        data += length;
                        break;
                    }

                    data += length;
                }
                else
                {
                    position += token;
                    data++;

                    if (position > skipX)
                    {
                        done = position - skipX;
                        out = writer.run(rowStart + clipX0, *data, done);
                        data++;
                        break;
                    }

                    data++;
                }
            }
        }

        while (done != limit)
        {
            const uint8_t token = *data++;
            const bool isLiteral = token >= LITERAL_TOKEN;
            int32_t length = isLiteral ? token - LITERAL_TOKEN : token;
            done += length;

            if (done > limit)
            {
                length -= done - limit;
                done = limit;
            }

            if (isLiteral)
            {
                out = writer.literal(out, data, length);
                data += length;
            }
            else
            {
                out = writer.run(out, *data, length);
                data++;
            }
        }

        rowStart += width;
        rowOffset = read16(nextOffset);
        nextOffset += 2;
    }

    return 0;
}
