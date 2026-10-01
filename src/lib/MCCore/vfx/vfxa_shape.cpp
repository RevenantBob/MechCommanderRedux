#include "stdafx.h"
#include "vfx/vfxint.h"

// VFX's shape routines (vfxa.asm in MCX.EXE): drawing, translating, measuring and encoding the run-length shapes the
// game's sprites, buttons and cursors are stored as. The format is documented in docs/formats/vfx-shape.md.

uint8_t VFXShapeLookaside[256];

namespace
{
    /// <summary>A shape's header fields, read from the table.</summary>
    struct ShapeInfo
    {
        const uint8_t* Header;
        int32_t XMin;
        int32_t YMin;
        int32_t XMax;
        int32_t YMax;
        /// <summary>The first row's first token.</summary>
        const uint8_t* Data;
    };

    ShapeInfo ReadShape(void* shapeTable, int32_t shapeNum)
    {
        ShapeInfo info;
        info.Header = MCVfxShape(shapeTable, shapeNum);
        info.XMin = MCVfxRead32(info.Header + 0x08);
        info.YMin = MCVfxRead32(info.Header + 0x0c);
        info.XMax = MCVfxRead32(info.Header + 0x10);
        info.YMax = MCVfxRead32(info.Header + 0x14);
        info.Data = info.Header + 0x18;
        return info;
    }

    /// <summary>Steps over one encoded row (up to and past its end token).</summary>
    const uint8_t* SkipRow(const uint8_t* data)
    {
        for (;;)
        {
            const uint8_t token = *data++;
            const uint32_t count = token >> 1;

            if (count == 0)
            {
                if ((token & 1) == 0)
                {
                    return data;
                }

                ++data; // skip: the count byte
            }
            else if (token & 1)
            {
                data += count; // literal pixels
            }
            else
            {
                ++data; // run: its colour
            }
        }
    }

    /// <summary>
    /// Draws one encoded row starting at window column <paramref name="x"/> into <paramref name="row"/> (the window
    /// row's first pixel), writing only columns cx0..cx1, each pixel mapped through <paramref name="xlat"/> when it
    /// isn't null.
    /// </summary>
    /// <returns>The next row's first token.</returns>
    /// <remarks>
    /// The asm has separate loops for rows needing no clipping, left clipping, right clipping and both, chosen from the
    /// shape's bounds; they write the same pixels as this single loop for every shape whose rows stay within its
    /// bounds. Port fix: a row running past its declared bounds is clipped here too, where the asm's unclipped loops
    /// would have written past the pane.
    /// </remarks>
    const uint8_t* DrawRow(const uint8_t* data, uint8_t* row, int32_t x, int32_t cx0, int32_t cx1, const uint8_t* xlat)
    {
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

                x += *data++;
                continue;
            }

            // The part of [x, x + count) inside [cx0, cx1].
            const int32_t first = std::max(x, cx0);
            const int32_t last = std::min(x + count - 1, cx1);

            if (token & 1)
            {
                for (int32_t i = first; i <= last; ++i)
                {
                    const uint8_t pixel = data[i - x];
                    row[i] = xlat != nullptr ? xlat[pixel] : pixel;
                }

                data += count;
            }
            else
            {
                uint8_t color = *data++;

                if (xlat != nullptr)
                {
                    color = xlat[color];
                }

                if (first <= last)
                {
                    std::memset(row + first, color, static_cast<size_t>(last - first + 1));
                }
            }

            x += count;
        }
    }

    /// <summary>
    /// The common body of VFX_shape_draw and VFX_shape_translate_draw (the latter with the lookaside table as
    /// <paramref name="xlat"/>).
    /// </summary>
    int32_t DrawShape(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, const uint8_t* xlat)
    {
        MCVfxClip clip;
        const int32_t status = MCVfxClipPane(pane, clip);

        if (status != 0)
        {
            return status;
        }

        hotX += clip.PaneX;
        hotY += clip.PaneY;
        const ShapeInfo shape = ReadShape(shapeTable, shapeNum);
        const int32_t x0 = shape.XMin + hotX;
        const int32_t y0 = shape.YMin + hotY;
        const int32_t x1 = shape.XMax + hotX;
        const int32_t y1 = shape.YMax + hotY;

        if (x1 < x0 || y1 < y0)
        {
            return VFX_ERR_BAD_SHAPE;
        }

        // Cohen-Sutherland outcodes of the two corners: a side both lie beyond rejects the shape.
        if ((x0 < clip.X0 && x1 < clip.X0) || (x0 > clip.X1 && x1 > clip.X1) || (y0 < clip.Y0 && y1 < clip.Y0) ||
            (y0 > clip.Y1 && y1 > clip.Y1))
        {
            return VFX_ERR_CLIPPED;
        }

        const uint8_t* data = shape.Data;
        int32_t y = y0;

        for (; y < clip.Y0; ++y)
        {
            data = SkipRow(data);
        }

        for (; y <= y1 && y <= clip.Y1; ++y)
        {
            data = DrawRow(data, clip.At(0, y), x0, clip.X0, clip.X1, xlat);
        }

        return 0;
    }
}

int32_t VFX_shape_draw(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY)
{
    return DrawShape(pane, shapeTable, shapeNum, hotX, hotY, nullptr);
}

void VFX_shape_lookaside(uint8_t* table)
{
    std::memcpy(VFXShapeLookaside, table, sizeof(VFXShapeLookaside));
}

int32_t VFX_shape_translate_draw(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY)
{
    return DrawShape(pane, shapeTable, shapeNum, hotX, hotY, VFXShapeLookaside);
}

int32_t VFX_shape_remap_colors(void* shapeTable, int32_t shapeNum)
{
    const ShapeInfo shape = ReadShape(shapeTable, shapeNum);
    uint8_t* data = const_cast<uint8_t*>(shape.Data);

    for (int32_t rows = shape.YMax + 1 - shape.YMin; rows > 0; --rows)
    {
        for (;;)
        {
            const uint8_t token = *data++;
            const uint32_t count = token >> 1;

            if (count == 0)
            {
                if ((token & 1) == 0)
                {
                    break;
                }

                ++data;
            }
            else if (token & 1)
            {
                for (uint32_t i = 0; i < count; ++i, ++data)
                {
                    *data = VFXShapeLookaside[*data];
                }
            }
            else
            {
                *data = VFXShapeLookaside[*data];
                ++data;
            }
        }
    }

    return 0;
}

int32_t VFX_shape_visible_rectangle(void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, int32_t mirror,
                                    int32_t* rectangle)
{
    int32_t left = 0;
    int32_t top = 0;
    int32_t right = 0;
    int32_t bottom = 0;

    const ShapeInfo shape = ReadShape(shapeTable, shapeNum);
    int32_t rows = shape.YMax + 1 - shape.YMin;

    if (rows > 0)
    {
        left = INT32_MAX;
        top = INT32_MAX;
        right = INT32_MIN;
        bottom = INT32_MIN;
        const uint8_t* data = shape.Data;

        for (int32_t y = shape.YMin + hotY; rows > 0; --rows, ++y)
        {
            int32_t x = shape.XMin + hotX;

            for (;;)
            {
                const uint8_t token = *data++;
                const int32_t count = token >> 1;

                if (count == 0)
                {
                    if ((token & 1) == 0)
                    {
                        break;
                    }

                    x += *data++;
                    continue;
                }

                data += (token & 1) ? count : 1;
                // x1 ends one past the last pixel of the rightmost packet, as the asm leaves it.
                left = std::min(left, x);
                x += count;
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
        }
    }

    if (mirror & 1)
    {
        const int32_t mirroredLeft = hotX + hotX - right;
        right = hotX + hotX - left;
        left = mirroredLeft;
    }

    if (mirror & 2)
    {
        const int32_t mirroredTop = hotY + hotY - bottom;
        bottom = hotY + hotY - top;
        top = mirroredTop;
    }

    rectangle[0] = left;
    rectangle[1] = top;
    rectangle[2] = right;
    rectangle[3] = bottom;
    return 0;
}

namespace
{
    /// <summary>
    /// The state VFX's shape encoder kept in globals between ScanLine and FlushPacket (0x007a810d..0x007a8141 in
    /// MCX.EXE).
    /// </summary>
    struct ShapeScanner
    {
        /// <summary>Where packets go; null only measures (the asm's flag at 0x007a810d).</summary>
        uint8_t* Buffer = nullptr;
        /// <summary>The next output byte, as an offset from <see cref="Buffer"/> (0x007a811d).</summary>
        intptr_t Out = 0;
        /// <summary>The first pixel of the row being encoded (0x007a8115).</summary>
        const uint8_t* Row = nullptr;
        /// <summary>The first pixel not yet encoded (0x007a8121).</summary>
        const uint8_t* PacketStart = nullptr;
        /// <summary>Where the scan stood when the flush was asked for (0x007a8119).</summary>
        const uint8_t* ScanPos = nullptr;
        /// <summary>Transparent pixels waiting to be written as skips (0x007a8111).</summary>
        int32_t PendingSkip = 0;

        void Put(uint8_t value)
        {
            if (Buffer != nullptr)
            {
                Buffer[Out] = value;
            }

            ++Out;
        }

        void EmitSkips()
        {
            while (PendingSkip != 0)
            {
                const int32_t count = std::min(PendingSkip, 0xff);
                PendingSkip -= count;
                Put(1);
                Put(static_cast<uint8_t>(count));
                PacketStart += count;
            }
        }

        /// <summary>
        /// FlushPacket (MCX.EXE @ 0x0076f5ef): 0 starts a row, 1 writes the pixels from PacketStart to
        /// ScanPos - <paramref name="holdBack"/> as literals, 2 as runs of their first colour, 3 records them as
        /// pending skips, 4 ends the row.
        /// </summary>
        void Flush(int32_t kind, int32_t holdBack)
        {
            switch (kind)
            {
                case 0:
                {
                    PendingSkip = 0;
                    PacketStart = ScanPos;
                    break;
                }
                case 1:
                {
                    EmitSkips();
                    int32_t length = static_cast<int32_t>(ScanPos - PacketStart) - holdBack;

                    while (length != 0)
                    {
                        const int32_t count = std::min(length, 0x7f);
                        Put(static_cast<uint8_t>(count * 2 + 1));

                        for (int32_t i = 0; i < count; ++i)
                        {
                            Put(PacketStart[i]);
                        }

                        PacketStart += count;
                        length -= count;
                    }
                    break;
                }

                case 2:
                {
                    EmitSkips();
                    int32_t length = static_cast<int32_t>(ScanPos - PacketStart) - holdBack;

                    while (length != 0)
                    {
                        const int32_t count = std::min(length, 0x7f);
                        Put(static_cast<uint8_t>(count * 2));
                        Put(*PacketStart);
                        PacketStart += count;
                        length -= count;
                    }
                    break;
                }

                case 3:
                    PendingSkip = static_cast<int32_t>(ScanPos - PacketStart) - holdBack;
                    break;
                case 4:
                    Put(0);
                    break;
                default:
                    break;
            }
        }

        /// <summary>
        /// ScanLine (MCX.EXE @ 0x0076f466): encodes <paramref name="width"/> pixels from <see cref="Row"/>. Pixels
        /// of <paramref name="transparent"/> become skips (trailing ones are dropped); three or more equal pixels, or
        /// two starting a packet, become a run; anything else literals.
        /// </summary>
        void ScanLine(int32_t width, uint8_t transparent)
        {
            enum State
            {
                Literal = 1,
                Run = 2,
                Transparent = 3,
                Start = 5
            };

            const uint8_t* p = Row;
            ScanPos = p;
            Flush(0, 0);
            int32_t state = Start;
            int32_t left = width;

            if (left != 0)
            {
                uint8_t current = *p++;
                --left;
                // Which state to enter next; each state ends by setting it or by running out of pixels.
                int32_t next = current == transparent ? Transparent : Literal;

                while (left >= 0)
                {
                    if (next == Literal)
                    {
                        state = Literal;

                        if (left == 0)
                        {
                            break;
                        }

                        uint8_t pixel = *p++;
                        --left;
                        bool same = pixel == current;
                        current = pixel;

                        if (same)
                        {
                            next = Run; // two equal pixels start the packet: a run
                            continue;
                        }

                        if (current == transparent)
                        {
                            ScanPos = p;
                            Flush(Literal, 1);
                            next = Transparent;
                            continue;
                        }

                        // Collect literals until a transparent pixel or three equal ones.
                        next = 0;

                        while (next == 0)
                        {
                            if (left == 0)
                            {
                                break;
                            }

                            pixel = *p++;
                            --left;
                            same = pixel == current;
                            current = pixel;

                            if (current == transparent)
                            {
                                ScanPos = p;
                                Flush(Literal, 1);
                                next = Transparent;
                                break;
                            }

                            if (!same)
                            {
                                continue;
                            }

                            if (left == 0)
                            {
                                break;
                            }

                            pixel = *p++;
                            --left;
                            same = pixel == current;
                            current = pixel;

                            if (current == transparent)
                            {
                                ScanPos = p;
                                Flush(Literal, 1);
                                next = Transparent;
                                break;
                            }

                            if (!same)
                            {
                                continue;
                            }

                            ScanPos = p;
                            Flush(Literal, 3);
                            next = Run;
                        }

                        if (next == 0)
                        {
                            break; // out of pixels inside a literal
                        }
                    }
                    else if (next == Run)
                    {
                        state = Run;
                        bool ended = true;

                        while (left != 0)
                        {
                            const uint8_t pixel = *p++;
                            --left;

                            if (pixel == current)
                            {
                                continue;
                            }

                            current = pixel;
                            ScanPos = p;
                            Flush(Run, 1);
                            next = current == transparent ? Transparent : Literal;
                            ended = false;
                            break;
                        }

                        if (ended)
                        {
                            break;
                        }
                    }
                    else
                    {
                        state = Transparent;
                        bool ended = true;

                        while (left != 0)
                        {
                            const uint8_t pixel = *p++;
                            --left;

                            if (pixel == current)
                            {
                                continue;
                            }

                            current = pixel;
                            ScanPos = p;
                            Flush(Transparent, 1);
                            next = Literal;
                            ended = false;
                            break;
                        }

                        if (ended)
                        {
                            break;
                        }
                    }
                }
            }

            ScanPos = p;
            Flush(state, 0);
            Flush(4, 0);
        }
    };

    void Write32(uint8_t* p, int32_t value)
    {
        std::memcpy(p, &value, 4);
    }
}

int32_t VFX_shape_scan_asm(PANE* pane, uint8_t transparentColor, int32_t hotX, int32_t hotY, void* buffer)
{
    MCVfxClip clip;
    const int32_t status = MCVfxClipPane(pane, clip);

    if (status != 0)
    {
        return status;
    }

    uint8_t* out = static_cast<uint8_t*>(buffer);

    if (out != nullptr)
    {
        // Bounds and origin words; the extent is the pane's (unclipped) size less one.
        const uint32_t bounds =
            (static_cast<uint32_t>(pane->x1 - pane->x0) << 16) | static_cast<uint16_t>(pane->y1 - pane->y0);
        const uint32_t origin = (static_cast<uint32_t>(hotX) << 16) | static_cast<uint16_t>(hotY);
        Write32(out + 0x00, static_cast<int32_t>(bounds));
        Write32(out + 0x04, static_cast<int32_t>(origin));
        Write32(out + 0x08, 0);
        Write32(out + 0x0c, 0);
        Write32(out + 0x10, -1);
        Write32(out + 0x14, -1);
    }

    hotX += clip.PaneX;
    hotY += clip.PaneY;
    const int32_t width = clip.X1 + 1 - clip.X0;

    // Pass 1: the bounding box of the non-transparent pixels.
    int32_t minX = INT32_MAX;
    int32_t minY = INT32_MAX;
    int32_t maxX = -INT32_MAX;
    int32_t maxY = -INT32_MAX;

    for (int32_t y = clip.Y0; y <= clip.Y1; ++y)
    {
        const uint8_t* row = clip.At(clip.X0, y);
        int32_t first = 0;

        while (first < width && row[first] == transparentColor)
        {
            ++first;
        }

        if (first == width)
        {
            continue;
        }

        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
        minX = std::min(minX, clip.X0 + first);
        int32_t last = width - 1;

        while (row[last] == transparentColor)
        {
            --last;
        }

        maxX = std::max(maxX, clip.X0 + last);
    }

    // (An all-transparent pane leaves the sentinels, and the asm's 32-bit arithmetic wraps: hence the unsigned sums.)
    const auto wrap = [](int32_t a, int32_t b)
    { return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); };

    if (out != nullptr)
    {
        Write32(out + 0x08, wrap(minX, hotX));
        Write32(out + 0x0c, wrap(minY, hotY));
        Write32(out + 0x10, wrap(maxX, hotX));
        Write32(out + 0x14, wrap(maxY, hotY));
    }

    // Pass 2: encode the rows of the bounding box.
    ShapeScanner scanner;
    scanner.Buffer = out;
    scanner.Out = 0x18;
    const int32_t rowWidth = wrap(maxX, minX) + 1;

    for (int32_t y = minY; y <= maxY; ++y)
    {
        scanner.Row = clip.At(minX, y);
        scanner.ScanLine(rowWidth, transparentColor);
    }

    return static_cast<int32_t>(scanner.Out);
}

int32_t VFX_shape_bounds(void* shapeTable, int32_t shapeNum)
{
    return MCVfxRead32(MCVfxShape(shapeTable, shapeNum));
}

int32_t VFX_shape_origin(void* shapeTable, int32_t shapeNum)
{
    return MCVfxRead32(MCVfxShape(shapeTable, shapeNum) + 4);
}

int32_t VFX_shape_resolution(void* shapeTable, int32_t shapeNum)
{
    const ShapeInfo shape = ReadShape(shapeTable, shapeNum);
    const uint32_t width = static_cast<uint32_t>(shape.XMax - shape.XMin + 1);
    const uint32_t height = static_cast<uint32_t>(shape.YMax - shape.YMin + 1);
    return static_cast<int32_t>((width << 16) | (height & 0xffff));
}

int32_t VFX_shape_minxy(void* shapeTable, int32_t shapeNum)
{
    const ShapeInfo shape = ReadShape(shapeTable, shapeNum);
    return static_cast<int32_t>((static_cast<uint32_t>(shape.XMin) << 16) | static_cast<uint16_t>(shape.YMin));
}

namespace
{
    /// <summary>The palette block of shape <paramref name="shapeNum"/> (the directory's second dword), or null.</summary>
    uint8_t* ShapePalette(void* shapeTable, int32_t shapeNum)
    {
        uint8_t* table = static_cast<uint8_t*>(shapeTable);
        const int32_t offset = MCVfxRead32(table + 8 + static_cast<intptr_t>(shapeNum) * 8 + 4);
        return offset == 0 ? nullptr : table + offset;
    }
}

void VFX_shape_palette(void* shapeTable, int32_t shapeNum, VFX_RGB* palette)
{
    const uint8_t* block = ShapePalette(shapeTable, shapeNum);

    if (block == nullptr)
    {
        return;
    }

    // Original behaviour: a count of 0 would loop 2^32 times; the game's shapes have no palettes.
    uint32_t count = static_cast<uint32_t>(MCVfxRead32(block));
    block += 4;

    do
    {
        palette[block[0]] = VFX_RGB{block[1], block[2], block[3]};
        block += 4;
    } while (--count != 0);
}

int32_t VFX_shape_colors(void* shapeTable, int32_t shapeNum, VFX_CRGB* colors)
{
    const uint8_t* block = ShapePalette(shapeTable, shapeNum);

    if (block == nullptr)
    {
        return 0;
    }

    const int32_t count = MCVfxRead32(block);

    if (colors != nullptr)
    {
        std::memcpy(colors, block + 4, static_cast<size_t>(count) * sizeof(VFX_CRGB));
    }

    return count;
}

int32_t VFX_shape_set_colors(void* shapeTable, int32_t shapeNum, VFX_CRGB* colors)
{
    uint8_t* block = ShapePalette(shapeTable, shapeNum);

    if (block == nullptr)
    {
        return 0;
    }

    const int32_t count = MCVfxRead32(block);

    if (colors != nullptr)
    {
        std::memcpy(block + 4, colors, static_cast<size_t>(count) * sizeof(VFX_CRGB));
    }

    return count;
}

int32_t VFX_shape_count(void* shapeTable)
{
    return MCVfxRead32(static_cast<uint8_t*>(shapeTable) + 4);
}

namespace
{
    /// <summary>
    /// VFX_shape_list and VFX_shape_palette_list: list shape 0 and every later shape whose directory dword at
    /// <paramref name="field"/> (0 data, 4 palette) differs from all earlier shapes'.
    /// </summary>
    int32_t ListDistinct(void* shapeTable, int32_t field, uint32_t* indexList)
    {
        const uint8_t* table = static_cast<uint8_t*>(shapeTable);
        const int32_t count = MCVfxRead32(table + 4);
        const uint8_t* directory = table + 8 + field;
        int32_t distinct = 1;

        if (indexList != nullptr)
        {
            *indexList++ = 0;
        }

        // Original behaviour: an empty table (count 0) would make the asm's LOOP run 2^32 times.
        for (int32_t i = 1; i < count; ++i)
        {
            const int32_t value = MCVfxRead32(directory + i * 8);
            bool seen = false;

            for (int32_t j = 0; j < i && !seen; ++j)
            {
                seen = MCVfxRead32(directory + j * 8) == value;
            }

            if (seen)
            {
                continue;
            }

            if (indexList != nullptr)
            {
                *indexList++ = static_cast<uint32_t>(i);
            }

            ++distinct;
        }

        return distinct;
    }
}

int32_t VFX_shape_list(void* shapeTable, uint32_t* indexList)
{
    return ListDistinct(shapeTable, 0, indexList);
}

int32_t VFX_shape_palette_list(void* shapeTable, uint32_t* indexList)
{
    return ListDistinct(shapeTable, 4, indexList);
}
