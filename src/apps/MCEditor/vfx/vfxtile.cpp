#include "stdafx.h"
#include "vfx/vfxtile.h"

namespace
{
    /// <summary>The bytes the encoders allocate for their output.</summary>
    inline constexpr size_t SCAN_BUFFER_SIZE = 0x20000;
    /// <summary>The magic at the start of an overlay shape and of an overlay shape table ("DNAH" in memory).</summary>
    inline constexpr uint32_t SHAPE_MAGIC = 0x48414e44;
    /// <summary>The longest run or literal of an overlay shape.</summary>
    inline constexpr uint8_t MAX_TOKEN = 0x7f;
    /// <summary>Added to a literal's length in its token.</summary>
    inline constexpr uint8_t LITERAL_TOKEN = 0x80;

    /// <summary>Writes <paramref name="count"/> pixels of a span: filled, copied from <paramref name="pixels"/> or mapped.</summary>
    void writeSpan(uint8_t* out, const uint8_t* pixels, int32_t count, const uint8_t* xlat)
    {
        if (xlat == VFX_TILE_FILL)
        {
            std::memset(out, VFX_TILE_FILL_COLOR, static_cast<size_t>(count));
        }
        else if (xlat == nullptr)
        {
            std::memcpy(out, pixels, static_cast<size_t>(count));
        }
        else
        {
            for (int32_t i = 0; i < count; i++)
            {
                out[i] = xlat[pixels[i]];
            }
        }
    }

    /// <summary>Reads a 32-bit word of the tile's offset table.</summary>
    int32_t readOffset(const uint8_t* tile, int32_t row)
    {
        int32_t offset;
        std::memcpy(&offset, tile + 4 + static_cast<ptrdiff_t>(row) * 4, sizeof(offset));
        return offset;
    }
}

uint32_t VFX_nTile_draw(uint8_t* buffer, int32_t width, int32_t height, const uint8_t* tile, int32_t x, int32_t y,
                        const uint8_t* xlat, int32_t outline)
{
    const int32_t lastRow = height - 1;
    const int32_t lastColumn = width - 1;
    int32_t rows = tile[2];
    const int32_t columns = tile[3];
    const int32_t left = x - tile[0];
    const int32_t top = y - tile[1];

    if (lastColumn <= left || lastRow <= top || left <= -columns || top <= -rows)
    {
        return VFX_TILE_OUTSIDE;
    }

    uint8_t* row = buffer + (top < 1 ? 0 : static_cast<ptrdiff_t>(top) * width);
    const int32_t skip = top < 0 ? -top : 0;

    if (lastRow < rows + top)
    {
        rows = lastRow - top + 1;
    }

    const auto outlineColor = static_cast<uint8_t>(outline);

    if (left > 0 && columns + left < lastColumn)
    {
        // Wholly inside horizontally: the spans are written as they are.
        row += left;
        const uint8_t* data = tile + readOffset(tile, skip);

        for (int32_t i = skip; i < rows; i++)
        {
            const int32_t length = readOffset(tile, i + 1) - readOffset(tile, i);

            if (length != 0)
            {
                uint8_t* start = row + data[0];
                const int32_t pixels = length - 1;
                writeSpan(start, data + 1, pixels, xlat);
                data += length;

                if (outline != VFX_NO_OUTLINE)
                {
                    // Original behaviour: a span of no pixels outlines the pixel before it.
                    start[pixels - 1] = outlineColor;
                    start[0] = outlineColor;
                }
            }

            row += width;
        }

        return 0;
    }

    // Clipped against the left and right edges.
    for (int32_t i = skip; i < rows; i++)
    {
        const int32_t offset = readOffset(tile, i);
        const int32_t spanX = tile[offset] + left;
        const uint8_t* pixels = tile + offset + 1;
        const int32_t count = readOffset(tile, i + 1) - offset - 1;
        const int32_t cut = spanX < 0 ? -spanX : 0;
        const int32_t end = spanX + count;
        const int32_t over = end > lastColumn ? end - lastColumn - 1 : 0;
        const int32_t visible = count - cut - over;
        uint8_t* start = row + spanX + cut;

        if (visible > 0)
        {
            writeSpan(start, pixels + cut, visible, xlat);

            if (outline != VFX_NO_OUTLINE)
            {
                start[visible - 1] = outlineColor;

                // A span cut at the left edge keeps its first pixel.
                if (start != row)
                {
                    start[0] = outlineColor;
                }
            }
        }

        row += width;
    }

    return 0;
}

int32_t VFX_nTile_scan(const PANE& pane, uint8_t hotX, uint8_t hotY, uint8_t** tile)
{
    uint8_t* out = new uint8_t[SCAN_BUFFER_SIZE];
    *tile = out;
    out[0] = hotX;
    out[1] = hotY;
    out[2] = static_cast<uint8_t>(pane.window->y_max);
    out[3] = static_cast<uint8_t>(pane.window->x_max);
    uint8_t* offsets = out + 4;
    const uint8_t* source = pane.window->buffer;
    uint8_t* data = offsets + static_cast<ptrdiff_t>(pane.y1) * 4 + 4;

    for (int32_t y = pane.y0; y < pane.y1; y++)
    {
        const auto rowOffset = static_cast<int32_t>(data - out);
        std::memcpy(offsets + static_cast<ptrdiff_t>(y) * 4, &rowOffset, sizeof(rowOffset));
        bool started = false;
        uint8_t leading = 0;

        for (int32_t x = pane.x0; x < pane.x1; x++, source++)
        {
            if (*source == VFX_TRANSPARENT)
            {
                if (!started)
                {
                    leading++;
                }

                continue;
            }

            if (!started)
            {
                *data++ = leading;
                started = true;
            }

            *data++ = *source;
        }
    }

    const auto size = static_cast<int32_t>(data - out);
    std::memcpy(offsets + static_cast<ptrdiff_t>(pane.y1) * 4, &size, sizeof(size));
    return size;
}

int32_t VFX_nShape_scan(const PANE& pane, uint8_t hotX, uint8_t hotY, uint8_t** shape)
{
    uint8_t* out = new (std::nothrow) uint8_t[SCAN_BUFFER_SIZE];
    *shape = out;

    if (out == nullptr)
    {
        return -1;
    }

    std::memcpy(out, &SHAPE_MAGIC, sizeof(SHAPE_MAGIC));
    const uint8_t* buffer = pane.window->buffer;
    // The box of the opaque pixels: a column to the left of the leftmost and a row above the topmost.
    int32_t left = pane.x1;
    int32_t right = pane.x0;
    int32_t top = -1;
    int32_t bottom = -1;
    const uint8_t* source = buffer;

    for (int32_t y = pane.y0; y < pane.y1; y++)
    {
        for (int32_t x = pane.x0; x < pane.x1; x++, source++)
        {
            if (*source == VFX_TRANSPARENT)
            {
                continue;
            }

            // Original behaviour: an opaque pixel on row 0 leaves the top unset, so a later row's opaque pixel sets it
            // to the row above that one, and the bottom keeps the last such row.
            if (top == -1)
            {
                top = y - 1;

                if (y != 0)
                {
                    bottom = y;
                }
            }
            else
            {
                bottom = y;
            }

            left = (std::min)(left, x - 1);
            right = (std::max)(right, x);
        }
    }

    const auto writeWord = [&](size_t offset, int32_t value)
    {
        const auto word = static_cast<uint16_t>(value);
        std::memcpy(out + offset, &word, sizeof(word));
    };

    const int32_t shapeHeight = bottom - top;
    writeWord(4, hotX - left);
    writeWord(6, hotY - pane.y0 - top);
    writeWord(8, shapeHeight);
    writeWord(10, right - left);
    constexpr size_t OFFSETS = 12;
    const int32_t stride = pane.x1 - pane.x0;
    // Original behaviour: the first row encoded is the topmost opaque one, though the offsets and hot spot count from
    // the row above it (the drawer draws the first row twice, which puts it back in place); the last is the row
    // below the bottom, which the drawer never reaches, and may lie past the pane.
    const uint8_t* row = buffer + static_cast<ptrdiff_t>(stride) * (top + 1);
    uint8_t* data = out + OFFSETS + static_cast<size_t>(shapeHeight) * 2 + 4;

    if (top <= bottom)
    {
        for (int32_t k = 0; k <= shapeHeight; k++)
        {
            writeWord(OFFSETS + static_cast<size_t>(k) * 2, static_cast<int32_t>(data - out));
            int32_t x = pane.x0 + left;
            const uint8_t* pixel = row + left;
            // The colour of the last skip (0 until the row has one), and the run length, carried from run to run.
            uint8_t skipColor = 0;
            uint8_t runLength = 0;
            uint8_t previous = 0;

            while (x < right)
            {
                uint8_t color = *pixel;

                if (color == VFX_TRANSPARENT)
                {
                    uint8_t count = 0;
                    skipColor = color;

                    while (x < right && count != MAX_TOKEN)
                    {
                        color = pixel[1];
                        pixel++;
                        x++;
                        count++;

                        if (color != VFX_TRANSPARENT)
                        {
                            break;
                        }
                    }

                    *data++ = count;
                    *data++ = VFX_TRANSPARENT;
                    previous = skipColor;
                }
                else if (previous == color)
                {
                    // Original behaviour: the count goes on from the last run of the row.
                    uint8_t count = runLength;

                    while (x < right && count != MAX_TOKEN)
                    {
                        pixel++;
                        x++;
                        count++;

                        if (*pixel != previous)
                        {
                            break;
                        }
                    }

                    *data++ = count;
                    *data++ = previous;
                    runLength = count;
                }
                else
                {
                    const uint8_t* literal = pixel;
                    uint8_t count = 0;

                    while (color != skipColor && x < right && count != MAX_TOKEN)
                    {
                        color = pixel[1];
                        pixel++;
                        x++;
                        count++;

                        if (color == VFX_TRANSPARENT)
                        {
                            break;
                        }
                    }

                    if (count > 0)
                    {
                        *data++ = static_cast<uint8_t>(count + LITERAL_TOKEN);
                        std::memcpy(data, literal, count);
                        data += count;
                    }

                    previous = skipColor;
                }
            }

            row = pixel + (pane.x1 - right);
        }
    }

    const auto size = static_cast<int16_t>(data - out);
    writeWord(OFFSETS + static_cast<size_t>(shapeHeight) * 2 + 2, size);
    return size;
}
