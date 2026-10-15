#include "stdafx.h"
#include "EditorUtil.h"
#include "vfx/alphapalette.h"

namespace
{
    /// <summary>Colours cached by <see cref="findNearestColor"/>: 5 bits each of red, green and blue.</summary>
    inline constexpr int32_t COLOR_CACHE_SIZE = 0x8000;
    /// <summary>A free slot of the colour cache.</summary>
    inline constexpr uint16_t COLOR_NOT_CACHED = 0xffff;
    /// <summary>The first palette index <see cref="findNearestColor"/> answers with.</summary>
    inline constexpr int32_t FIRST_MATCHED_COLOR = 10;
    /// <summary>The distance the search starts from, beyond any colour's.</summary>
    inline constexpr int32_t COLOR_DISTANCE_START = 0xfd02ff;
    /// <summary>Size of <see cref="win32ErrorText"/>'s message buffers.</summary>
    inline constexpr int32_t ERROR_TEXT_SIZE = 0x400;

    /// <summary>The palette index found for each 15-bit colour.</summary>
    uint16_t ColorCache[COLOR_CACHE_SIZE];
    /// <summary>The system's message for the last error <see cref="win32ErrorText"/> formatted.</summary>
    char SystemErrorText[ERROR_TEXT_SIZE];
    /// <summary>The text <see cref="win32ErrorText"/> returns.</summary>
    char ErrorText[ERROR_TEXT_SIZE];

    /// <summary>A little-endian 32-bit value of a shape table.</summary>
    int32_t read32(const uint8_t* p)
    {
        int32_t value;
        std::memcpy(&value, p, sizeof(value));
        return value;
    }

    /// <summary>A little-endian 16-bit value of a shape table.</summary>
    uint16_t read16(const uint8_t* p)
    {
        uint16_t value;
        std::memcpy(&value, p, sizeof(value));
        return value;
    }

    /// <summary>
    /// Steps over <paramref name="rows"/> rows of shape data: a run is 2 bytes, a literal 1 plus its length, a skip 2,
    /// and the row's end 1.
    /// </summary>
    const uint8_t* skipShapeRows(const uint8_t* data, uint32_t rows)
    {
        while (rows != 0)
        {
            const uint8_t token = *data;
            data += 2;
            const uint8_t count = token >> 1;

            if ((token & 1) == 0 && count != 0)
            {
                continue;
            }

            if ((token & 1) != 0 && count != 0)
            {
                data += count - 1;
                continue;
            }

            if ((token & 1) != 0)
            {
                continue;
            }

            data -= 1;
            rows--;
        }

        return data;
    }

    /// <summary>
    /// Draws <paramref name="rows"/> rows of shape data starting at offset <paramref name="start"/> of
    /// <paramref name="buffer"/>, every pixel inside (the shape lies within the clip rectangle). With
    /// <paramref name="Remap"/>, each shape colour goes through <paramref name="remap"/> first.
    /// </summary>
    template <bool Alpha, bool Remap>
    void drawRowsInside(uint8_t* buffer, intptr_t start, int32_t pitch, const uint8_t* data, uint32_t rows,
                        const uint8_t* remap)
    {
        intptr_t rowStart = start;
        intptr_t p = rowStart;

        for (;;)
        {
            const uint8_t token = *data++;
            uint8_t count = token >> 1;

            if ((token & 1) == 0 && count != 0)
            {
                uint8_t color = *data++;

                if constexpr (Remap)
                {
                    color = remap[color];
                }

                do
                {
                    if constexpr (Alpha)
                    {
                        buffer[p] = static_cast<uint8_t>(AlphaTable[color << 8 | buffer[p]]);
                    }
                    else
                    {
                        buffer[p] = color;
                    }

                    p++;
                } while (--count != 0);

                continue;
            }

            if (count != 0)
            {
                do
                {
                    uint8_t color = *data++;

                    if constexpr (Remap)
                    {
                        color = remap[color];
                    }

                    if constexpr (Alpha)
                    {
                        buffer[p] = static_cast<uint8_t>(AlphaTable[color << 8 | buffer[p]]);
                    }
                    else
                    {
                        buffer[p] = color;
                    }

                    p++;
                } while (--count != 0);

                continue;
            }

            if ((token & 1) != 0)
            {
                p += *data++;
                continue;
            }

            rowStart += pitch;
            p = rowStart;

            if (--rows == 0)
            {
                return;
            }
        }
    }

    /// <summary>
    /// Draws <paramref name="rows"/> rows of shape data starting at offset <paramref name="start"/>, writing only the
    /// pixels from <paramref name="low"/> to <paramref name="high"/> of each row (offsets in the first row).
    /// </summary>
    /// <remarks>
    /// A run or literal that crosses <paramref name="high"/> is abandoned there and the row carries on with the next
    /// packet, whose pixels all land past it.
    /// </remarks>
    template <bool Alpha, bool Remap>
    void drawRowsClipped(uint8_t* buffer, intptr_t start, intptr_t low, intptr_t high, int32_t pitch,
                         const uint8_t* data, uint32_t rows, const uint8_t* remap)
    {
        intptr_t rowStart = start;
        intptr_t p = rowStart;

        for (;;)
        {
            const uint8_t token = *data++;
            uint8_t count = token >> 1;

            if ((token & 1) == 0 && count != 0)
            {
                uint8_t color = *data++;

                if constexpr (Remap && Alpha)
                {
                    // EB-058 fixed: the original indexes the remap row with a stale high byte here and in the
                    // translucent literals below; the port remaps the shape colour.
                    color = remap[color];
                }

                do
                {
                    if (p >= low)
                    {
                        if (p > high)
                        {
                            break;
                        }

                        if constexpr (Alpha)
                        {
                            buffer[p] = static_cast<uint8_t>(AlphaTable[color << 8 | buffer[p]]);
                        }
                        else
                        {
                            if constexpr (Remap)
                            {
                                // Original behaviour (EB-057): the colour is remapped again for every pixel drawn.
                                color = remap[color];
                            }

                            buffer[p] = color;
                        }
                    }

                    p++;
                } while (--count != 0);

                continue;
            }

            if (count != 0)
            {
                do
                {
                    uint8_t color = *data++;

                    if (p >= low)
                    {
                        if (p > high)
                        {
                            data += count - 1;
                            break;
                        }

                        if constexpr (Remap)
                        {
                            color = remap[color];
                        }

                        if constexpr (Alpha)
                        {
                            buffer[p] = static_cast<uint8_t>(AlphaTable[color << 8 | buffer[p]]);
                        }
                        else
                        {
                            buffer[p] = color;
                        }
                    }

                    p++;
                } while (--count != 0);

                continue;
            }

            if ((token & 1) != 0)
            {
                p += *data++;
                continue;
            }

            high += pitch;
            rowStart += pitch;
            p = rowStart;
            low += pitch;

            if (--rows == 0)
            {
                return;
            }
        }
    }

    /// <summary>
    /// The horizontal clipping and drawing of <see cref="drawShapeInPane"/>: <paramref name="rows"/> rows of
    /// <paramref name="data"/> from row <paramref name="y"/>.
    /// </summary>
    template <bool Alpha, bool Remap>
    void drawShapeColumns(uint8_t* buffer, int32_t pitch, int32_t clipLeft, int32_t clipRight, const uint8_t* shape,
                          int32_t hotX, int32_t y, uint32_t rows, const uint8_t* data, const uint8_t* remap)
    {
        const int32_t x0 = read32(shape + 0x08);
        const int32_t x1 = read32(shape + 0x10);
        const uint32_t span = static_cast<uint32_t>(x1 - x0);
        // Original behaviour (EB-018): the translucent copy draws one pixel to the right (the remapping one doesn't).
        const int32_t x = x0 + hotX + (Alpha && !Remap ? 1 : 0);
        const intptr_t rowBase = static_cast<intptr_t>(y) * pitch;
        // The remapping copy tests the shape's last column against the right edge; the other its end.
        const uint32_t end = span + static_cast<uint32_t>(x) - (Remap ? 1 : 0);

        if (x < clipLeft)
        {
            if (span <= static_cast<uint32_t>(clipLeft - x))
            {
                return;
            }
        }
        else if (end > static_cast<uint32_t>(clipRight))
        {
            // Original behaviour (EB-018): meant to drop a shape lying wholly to the right; it drops every shape at
            // least as wide as the clip rectangle that crosses the right edge.
            if (static_cast<uint32_t>(x) <= end - static_cast<uint32_t>(clipRight))
            {
                return;
            }
        }
        else
        {
            drawRowsInside<Alpha, Remap>(buffer, rowBase + x, pitch, data, rows, remap);
            return;
        }

        drawRowsClipped<Alpha, Remap>(buffer, rowBase + x, rowBase + clipLeft, rowBase + clipRight, pitch, data, rows,
                                      remap);
    }

    /// <summary>
    /// The clipping and drawing of <see cref="drawShape"/> and <see cref="drawShapeRemapped"/>, for the opaque or the
    /// translucent case (the original repeats the code with small differences).
    /// </summary>
    template <bool Alpha, bool Remap>
    void drawShapeInPane(uint8_t* buffer, int32_t windowXMax, int32_t windowYMax, int32_t paneX0, int32_t paneY0,
                         int32_t paneX1, int32_t paneY1, const uint8_t* shape, int32_t hotX, int32_t hotY,
                         const uint8_t* remap)
    {
        const int32_t clipLeft = (std::max)(paneX0, 0);
        const int32_t clipTop = (std::max)(paneY0, 0);
        const int32_t pitch = windowXMax + 1;
        // Original behaviour (EB-018): the right edge is clipped to the window's width, not its last column, and the
        // bottom edge to its height.
        const int32_t clipRight = paneX1 >= pitch ? pitch : paneX1;
        const int32_t clipBottom = paneY1 >= windowYMax + 1 ? windowYMax + 1 : paneY1;
        const int32_t y0 = read32(shape + 0x0c);
        const int32_t y1 = read32(shape + 0x14);
        const uint8_t* data = shape + 0x18;
        uint32_t rows = static_cast<uint32_t>(y1 - y0 + 1);
        int32_t y = y0 + hotY;

        if (y < clipTop)
        {
            const uint32_t skip = static_cast<uint32_t>(clipTop - y);

            if (rows <= skip)
            {
                return;
            }

            rows -= skip;
            data = skipShapeRows(data, skip);
            y = clipTop;
        }

        // Original behaviour (EB-018): the translucent copy takes the shape for one row taller here (the remapping one
        // doesn't).
        const uint32_t last = static_cast<uint32_t>(y) + rows - (Alpha && !Remap ? 0 : 1);

        if (last > static_cast<uint32_t>(clipBottom))
        {
            const uint32_t over = last - static_cast<uint32_t>(clipBottom);

            if (rows <= over)
            {
                return;
            }

            rows -= over;

            if constexpr (Alpha && Remap)
            {
                // Original behaviour (EB-059): the remapping copy carries on in its opaque code here.
                drawShapeColumns<false, true>(buffer, pitch, clipLeft, clipRight, shape, hotX, y, rows, data, remap);
                return;
            }
        }

        drawShapeColumns<Alpha, Remap>(buffer, pitch, clipLeft, clipRight, shape, hotX, y, rows, data, remap);
    }

    /// <summary>The shape <paramref name="shapeNum"/> of <paramref name="shapeTable"/>.</summary>
    const uint8_t* shapeAt(const uint8_t* shapeTable, int32_t shapeNum)
    {
        return shapeTable + read32(shapeTable + 8 + shapeNum * 8);
    }

    /// <summary>Whether <paramref name="shape"/>'s data starts with the translucent tokens 03 00.</summary>
    bool isTranslucent(const uint8_t* shape)
    {
        return read16(shape + 0x18) == 3;
    }
}

bool ColorCacheReady = false;

uint8_t findNearestColor(const uint8_t* palette, const uint8_t* bgr)
{
    uint8_t best = 0;
    int32_t bestDistance = COLOR_DISTANCE_START;

    if (!ColorCacheReady)
    {
        ColorCacheReady = true;
        std::fill(std::begin(ColorCache), std::end(ColorCache), COLOR_NOT_CACHED);
    }

    const uint8_t blue = bgr[0];
    const uint8_t green = bgr[1];
    const uint8_t red = bgr[2];
    const uint32_t key = ((static_cast<uint32_t>(red & 0xf8) << 5 | (green & 0xf8)) << 2) | (blue >> 3);

    if (ColorCache[key] != COLOR_NOT_CACHED)
    {
        return static_cast<uint8_t>(ColorCache[key]);
    }

    const uint8_t* entry = palette + 0x28;

    for (int32_t index = FIRST_MATCHED_COLOR; index < 0x100; index++, entry += 4)
    {
        const int32_t dBlue = blue - entry[0];
        const int32_t dGreen = green - entry[1];
        const int32_t dRed = red - entry[2];
        const int32_t distance = dGreen * dGreen * 0x33 + dBlue * dBlue * 10 + dRed * dRed * 0x27;

        if (distance < bestDistance)
        {
            best = static_cast<uint8_t>(index);
            bestDistance = distance;

            if (distance == 0)
            {
                break;
            }
        }
    }

    ColorCache[key] = best;
    return best;
}

const char* win32ErrorText(const char* function, DWORD error)
{
    if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error, 0, SystemErrorText, ERROR_TEXT_SIZE, nullptr) == 0)
    {
        lstrcpyA(SystemErrorText, "Unknown Error");
    }

    wsprintfA(ErrorText, "%s failed with error code (0x%x) because %s\n", function, error, SystemErrorText);
    return ErrorText;
}

int32_t getCsvField(const char* source, char delimiter, char* dest, int32_t* length)
{
    int32_t count = 0;

    do
    {
        const char c = source[count];

        if (c == '\n' || c == delimiter || c == '\0')
        {
            break;
        }

        count++;
    } while (count < MAX_CSV_FIELD);

    if (count == MAX_CSV_FIELD)
    {
        return MAX_CSV_FIELD;
    }

    std::memcpy(dest, source, static_cast<size_t>(count));
    dest[count] = '\0';
    *length = count + 1;
    return 1;
}

void drawShape(uint8_t* buffer, int32_t width, int32_t height, const uint8_t* shapeTable, int32_t shapeNum,
               int32_t hotX, int32_t hotY)
{
    // The original builds a VFX window and pane over the bitmap in globals and draws through them.
    const int32_t windowXMax = width - 1;
    const int32_t windowYMax = height - 1;
    const uint8_t* shape = shapeAt(shapeTable, shapeNum);

    if (isTranslucent(shape))
    {
        drawShapeInPane<true, false>(buffer, windowXMax, windowYMax, 0, 0, windowXMax, windowYMax, shape, hotX, hotY,
                                     nullptr);
    }
    else
    {
        drawShapeInPane<false, false>(buffer, windowXMax, windowYMax, 0, 0, windowXMax, windowYMax, shape, hotX, hotY,
                                      nullptr);
    }
}

void drawShapeRemapped(uint8_t* buffer, int32_t width, int32_t height, const uint8_t* shapeTable, int32_t shapeNum,
                       int32_t hotX, int32_t hotY, const uint8_t* remap)
{
    // The original takes the remap table from a global its callers set first (0x00479790).
    const int32_t windowXMax = width - 1;
    const int32_t windowYMax = height - 1;
    const uint8_t* shape = shapeAt(shapeTable, shapeNum);

    if (isTranslucent(shape))
    {
        drawShapeInPane<true, true>(buffer, windowXMax, windowYMax, 0, 0, windowXMax, windowYMax, shape, hotX, hotY,
                                    remap);
    }
    else
    {
        drawShapeInPane<false, true>(buffer, windowXMax, windowYMax, 0, 0, windowXMax, windowYMax, shape, hotX, hotY,
                                     remap);
    }
}
