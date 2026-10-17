#include "stdafx.h"
#include "vfx/vfx.h"

// The editor's copy of VFX's shape encoder (VFX_shape_scan with its ScanLine and FlushPacket helpers, 0x00480f3d,
// 0x00481252 and 0x004813db). Hand-written assembly in the original; the same code as the game's copy, so this follows
// the game's translation.

namespace
{
    /// <summary>The state the encoder kept in globals between ScanLine and FlushPacket.</summary>
    struct ShapeScanner
    {
        /// <summary>Where packets go; null only measures.</summary>
        uint8_t* Buffer = nullptr;
        /// <summary>The next output byte, as an offset from <see cref="Buffer"/>.</summary>
        intptr_t Out = 0;
        /// <summary>The first pixel of the row being encoded.</summary>
        const uint8_t* Row = nullptr;
        /// <summary>The first pixel not yet encoded.</summary>
        const uint8_t* PacketStart = nullptr;
        /// <summary>Where the scan stood when the flush was asked for.</summary>
        const uint8_t* ScanPos = nullptr;
        /// <summary>Transparent pixels waiting to be written as skips.</summary>
        int32_t PendingSkip = 0;

        /// <summary>Writes one byte (or only counts it).</summary>
        void Put(uint8_t value)
        {
            if (Buffer != nullptr)
            {
                Buffer[Out] = value;
            }

            ++Out;
        }

        /// <summary>Writes the pending skips, at most 255 pixels each.</summary>
        void EmitSkips()
        {
            while (PendingSkip != 0)
            {
                const int32_t count = (std::min)(PendingSkip, 0xff);
                PendingSkip -= count;
                Put(1);
                Put(static_cast<uint8_t>(count));
                PacketStart += count;
            }
        }

        /// <summary>
        /// FlushPacket: 0 starts a row, 1 writes the pixels from PacketStart to ScanPos - <paramref name="holdBack"/>
        /// as literals, 2 as runs of their first colour, 3 records them as pending skips, 4 ends the row.
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
                        const int32_t count = (std::min)(length, 0x7f);
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
                        const int32_t count = (std::min)(length, 0x7f);
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
        /// ScanLine: encodes <paramref name="width"/> pixels from <see cref="Row"/>. Pixels of
        /// <paramref name="transparent"/> become skips (trailing ones are dropped); three or more equal pixels, or two
        /// starting a packet, become a run; anything else literals.
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

    /// <summary>Stores a little-endian 32-bit value.</summary>
    void write32(uint8_t* p, int32_t value)
    {
        std::memcpy(p, &value, sizeof(value));
    }

    /// <summary>a - b with the assembly's 32-bit wrap-around (an all-transparent pane leaves the sentinels).</summary>
    int32_t wrap(int32_t a, int32_t b)
    {
        return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
    }
}

int32_t VFX_shape_scan(PANE* pane, uint32_t transparentColor, int32_t hotX, int32_t hotY, void* buffer)
{
    const WINDOW& window = *pane->window;
    const int32_t pitch = window.x_max + 1;

    if (pitch == 0 || window.x_max < -1 || window.y_max < 0)
    {
        return VFX_ERR_BAD_WINDOW;
    }

    const int32_t clipX0 = (std::max)(pane->x0, 0);
    const int32_t clipY0 = (std::max)(pane->y0, 0);
    const int32_t clipX1 = (std::min)(pane->x1, window.x_max);
    const int32_t clipY1 = (std::min)(pane->y1, window.y_max);

    if (clipX1 < clipX0 || clipY1 < clipY0)
    {
        return VFX_ERR_EMPTY_PANE;
    }

    const uint8_t transparent = static_cast<uint8_t>(transparentColor);
    uint8_t* out = static_cast<uint8_t*>(buffer);

    if (out != nullptr)
    {
        // Bounds and origin words; the extent is the pane's (unclipped) size less one.
        const uint32_t bounds =
            (static_cast<uint32_t>(pane->x1 - pane->x0) << 16) | static_cast<uint16_t>(pane->y1 - pane->y0);
        const uint32_t origin = (static_cast<uint32_t>(hotX) << 16) | static_cast<uint16_t>(hotY);
        write32(out + 0x00, static_cast<int32_t>(bounds));
        write32(out + 0x04, static_cast<int32_t>(origin));
        write32(out + 0x08, 0);
        write32(out + 0x0c, 0);
        write32(out + 0x10, -1);
        write32(out + 0x14, -1);
    }

    const int32_t width = clipX1 + 1 - clipX0;
    int32_t minX = INT32_MAX;
    int32_t minY = INT32_MAX;
    int32_t maxX = -INT32_MAX;
    int32_t maxY = -INT32_MAX;

    // Pass 1: the bounding box of the non-transparent pixels.
    for (int32_t y = clipY0; y <= clipY1; ++y)
    {
        const uint8_t* row = window.buffer + static_cast<intptr_t>(y) * pitch + clipX0;
        int32_t first = 0;

        while (first < width && row[first] == transparent)
        {
            ++first;
        }

        if (first == width)
        {
            continue;
        }

        minY = (std::min)(minY, y);
        maxY = (std::max)(maxY, y);
        minX = (std::min)(minX, clipX0 + first);
        int32_t last = width - 1;

        while (row[last] == transparent)
        {
            --last;
        }

        maxX = (std::max)(maxX, clipX0 + last);
    }

    hotX += pane->x0;
    hotY += pane->y0;

    if (out != nullptr)
    {
        write32(out + 0x08, wrap(minX, hotX));
        write32(out + 0x0c, wrap(minY, hotY));
        write32(out + 0x10, wrap(maxX, hotX));
        write32(out + 0x14, wrap(maxY, hotY));
    }

    // Pass 2: encode the rows of the bounding box.
    ShapeScanner scanner;
    scanner.Buffer = out;
    scanner.Out = 0x18;
    const int32_t rowWidth = wrap(maxX, minX) + 1;

    for (int32_t y = minY; y <= maxY; ++y)
    {
        scanner.Row = window.buffer + static_cast<intptr_t>(y) * pitch + minX;
        scanner.ScanLine(rowWidth, transparent);
    }

    return static_cast<int32_t>(scanner.Out);
}
