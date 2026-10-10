#include "stdafx.h"
#include "vfx/MCVfxClip.h"

// VFX's picture decoders (IFF ILBM/PBM, PCX, GIF), its palette fade and its colour scan (vfxa.asm in MCX.EXE). The
// game draws its GIF art (aPort, TacticalMap) and takes its palette from data\palette\palettex.gif through these.
//
// The asm shared a few static scratch buffers between these routines (a 3328-byte line buffer at 0x007a8645, a
// 768-byte buffer at 0x007a9345, ...); the port gives each routine its own, sized to the data.

MCVfxRgb VfxDacPalette[256];
void (*VfxDacWriteHook)(int32_t index, const MCVfxRgb* rgb) = nullptr;
void (*VfxWaitRetraceHook)() = nullptr;

namespace
{
    uint16_t ReadBE16(const uint8_t* p)
    {
        return static_cast<uint16_t>((p[0] << 8) | p[1]);
    }

    uint16_t ReadLE16(const uint8_t* p)
    {
        return static_cast<uint16_t>(p[0] | (p[1] << 8));
    }

    /// <summary>
    /// Finds the IFF chunk tagged <paramref name="tag"/> after the FORM header (12 bytes): zero padding bytes
    /// between chunks are skipped, and only the low 16 bits of each chunk's length are used.
    /// </summary>
    /// <returns>The chunk's data (past its tag and length).</returns>
    /// <remarks>
    /// Original behaviour: there is no end check, so a missing chunk runs past the file.
    /// </remarks>
    uint8_t* FindIffChunk(const char* tag, uint8_t* iff)
    {
        uint8_t* chunk = iff + 0xc;

        for (;;)
        {
            while (*chunk == 0)
            {
                ++chunk;
            }

            if (std::memcmp(chunk, tag, 4) == 0)
            {
                return chunk + 8;
            }

            chunk += 8 + ReadBE16(chunk + 6);
        }
    }
}

void VfxLineToPane(MCPane* pane, int32_t y, uint8_t* line, int32_t width)
{
    MCVfxClip clip;

    if (MCVfxClipPane(pane, clip) != 0)
    {
        return;
    }

    int32_t x = clip.PaneX;
    y += clip.PaneY;
    int32_t count = width;
    int32_t over = clip.X1 - x + 1 - count;

    if (over < 0)
    {
        count += over;

        if (count <= 0)
        {
            return;
        }
    }

    over = x - clip.X0;

    if (over < 0)
    {
        count += over;

        if (count <= 0)
        {
            return;
        }

        line -= over;
        x -= over;
    }

    if (y > clip.Y1 || y < clip.Y0)
    {
        return;
    }

    MCRenderer::For(pane->Window).Write(pane->Window, x, y, line, count);
}

int32_t VfxIlbmDraw(MCPane* pane, uint8_t* ilbm)
{
    const int32_t paneWidth = pane->X1 - pane->X0 + 1;
    const int32_t paneHeight = pane->Y1 - pane->Y0 + 1;
    // Anything but FORM ILBM is taken for a chunky PBM.
    const bool planar = std::memcmp(ilbm + 8, "ILBM", 4) == 0;

    const uint8_t* header = FindIffChunk("BMHD", ilbm);
    int32_t width = ReadBE16(header);
    int32_t rows = std::min<int32_t>(ReadBE16(header + 2), paneHeight);

    if (header[9] == 1)
    {
        return 0; // Original behaviour: masked pictures aren't drawn (the asm returned garbage).
    }

    const int32_t compression = header[10];
    const int32_t transparent = header[13];

    // Bytes per plane row (word aligned) and the chunky row length (even).
    int32_t planeBytes = (width >> 3) + ((width & 7) != 0 ? 1 : 0);
    planeBytes += planeBytes & 1;
    const int32_t rowLength = width + (width & 1);
    width = std::min(width, paneWidth);

    uint8_t* body = FindIffChunk("BODY", ilbm);
    std::vector<uint8_t> unpacked(static_cast<size_t>(rowLength) + 256);
    std::vector<uint8_t> line(static_cast<size_t>(std::max(width, 1)));

    // Port fix: a picture (or pane) without rows draws nothing; the asm's count-down loop ran 2^32 times.
    for (int32_t y = 0; y < rows; ++y)
    {
        uint8_t* row = body;

        if (compression == 1)
        {
            // ByteRun1 into the row buffer, until at least a chunky row's length is out. Original behaviour: an
            // ILBM row (8 planes of planeBytes) is only that long when the width is a multiple of 16.
            uint8_t* out = unpacked.data();
            uint8_t* const end = out + rowLength;

            while (out < end)
            {
                const uint32_t n = *body++;

                if (n == 0x80)
                {
                    continue;
                }

                if (n < 0x80)
                {
                    std::memcpy(out, body, n + 1);
                    out += n + 1;
                    body += n + 1;
                }
                else
                {
                    const uint32_t count = 257 - n;
                    std::memset(out, *body++, count);
                    out += count;
                }
            }

            row = unpacked.data();
        }
        else
        {
            body += rowLength;
        }

        if (planar)
        {
            // Eight bitplanes, planeBytes apart; bit 7 of each plane byte is the leftmost pixel.
            uint8_t* out = line.data();
            int32_t pixels = width;
            const uint8_t* column = row;

            for (int32_t byteColumn = planeBytes; byteColumn > 0 && pixels > 0; --byteColumn, ++column)
            {
                for (uint32_t bit = 0x80; bit != 0 && pixels > 0; bit >>= 1, --pixels)
                {
                    uint8_t value = 0;

                    for (int32_t plane = 0; plane < 8; ++plane)
                    {
                        if (column[plane * planeBytes] & bit)
                        {
                            value |= static_cast<uint8_t>(1 << plane);
                        }
                    }

                    *out++ = value;
                }
            }

            VfxLineToPane(pane, y, line.data(), width);
        }
        else
        {
            VfxLineToPane(pane, y, row, width);
        }
    }

    return transparent;
}

int32_t VfxIlbmResolution(uint8_t* ilbm)
{
    const uint8_t* header = FindIffChunk("BMHD", ilbm);
    return static_cast<int32_t>((static_cast<uint32_t>(ReadBE16(header)) << 16) | ReadBE16(header + 2));
}

int32_t VfxPcxDraw(MCPane* pane, uint8_t* pcx)
{
    // Rows 0..(yMax - yMin), each bytesPerLine bytes of RLE: a byte with its top two bits set repeats the next byte
    // (its low six bits) times. Original behaviour: a run may spill past the row's end into the line buffer's slack
    // and is then dropped, as in the asm.
    const int32_t lastRow = static_cast<uint16_t>(ReadLE16(pcx + 0xa) - ReadLE16(pcx + 6));
    const int32_t bytesPerLine = ReadLE16(pcx + 0x42);
    std::vector<uint8_t> line(static_cast<size_t>(bytesPerLine) + 64);
    const uint8_t* in = pcx + 0x80;

    for (int32_t y = 0; y <= lastRow; ++y)
    {
        uint8_t* out = line.data();
        uint8_t* const end = out + bytesPerLine;

        do
        {
            const uint8_t value = *in++;

            if ((value & 0xc0) == 0xc0)
            {
                const uint32_t count = value & 0x3f;
                std::memset(out, *in++, count);
                out += count;
            }
            else
            {
                *out++ = value;
            }
        } while (out < end);

        VfxLineToPane(pane, y, line.data(), bytesPerLine);
    }

    return 0;
}

void VfxPcxPalette(const uint8_t* pcx, int32_t fileSize, MCVfxRgb* palette)
{
    const uint8_t* colors = pcx + fileSize - 0x300;
    uint8_t* out = &palette[0].R;

    for (int32_t i = 0; i < 0x300; ++i)
    {
        out[i] = static_cast<uint8_t>(colors[i] >> 2);
    }
}

int32_t VfxPcxResolution(uint8_t* pcx)
{
    const uint16_t width = static_cast<uint16_t>(ReadLE16(pcx + 8) - ReadLE16(pcx + 4) + 1);
    const uint16_t height = static_cast<uint16_t>(ReadLE16(pcx + 0xa) - ReadLE16(pcx + 6) + 1);
    return static_cast<int32_t>((static_cast<uint32_t>(width) << 16) | height);
}

namespace
{
#pragma pack(push, 1)
    /// <summary>The caller's GIF work buffer, laid out as the asm used it.</summary>
    struct MCGifState
    {
        int32_t NextCode;
        int32_t CodeLimit; // the next code that widens the code size
        int32_t LinePos;
        int32_t Row;
        int32_t BlockLeft; // bytes left in the current data sub-block
        uint32_t BitBuffer;
        int32_t BitCount;
        int32_t CodeSize;
        int32_t PixelsLeft; // pixels left in the current row
        int32_t Width;
        int32_t Height;
        uint8_t Interlaced;
        uint8_t Pass;
        uint8_t Stack[0x1000];   // the string being output, last pixel first
        uint8_t First[0x1000];   // each code's first pixel
        uint8_t Suffix[0x1000];  // each code's last pixel
        uint16_t Prefix[0x1000]; // each code's prefix code (0xffff roots, 0xfffe unused)
    };
#pragma pack(pop)
    static_assert(sizeof(MCGifState) == VfxGifBufferSize);

    /// <summary>Bit masks for 0..8 bits (0x007a9945).</summary>
    constexpr uint8_t GifMasks[9] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};
    /// <summary>The row step of each interlace pass (0x007a994e).</summary>
    constexpr uint8_t GifPassStep[5] = {8, 8, 4, 2, 0};
    /// <summary>The first row of each interlace pass (0x007a9953).</summary>
    constexpr uint8_t GifPassStart[5] = {0, 4, 2, 1, 0};

    struct MCGifDecoder
    {
        MCGifState* State;
        const uint8_t* In;
        MCPane* Pane;
        std::vector<uint8_t> Line;

        /// <summary>GIF_init_codetable.</summary>
        void InitCodeTable(int32_t clearCode) const
        {
            State->NextCode = clearCode + 2;
            State->CodeLimit = clearCode * 2;
            int32_t i = 0;

            for (; i < clearCode; ++i)
            {
                State->First[i] = static_cast<uint8_t>(i);
                State->Suffix[i] = static_cast<uint8_t>(i);
                State->Prefix[i] = 0xffff;
            }

            for (; i < 0x1000; ++i)
            {
                State->Prefix[i] = 0xfffe;
            }
        }

        /// <summary>GIF_getb: the next data byte, stepping over sub-block lengths.</summary>
        uint32_t GetByte()
        {
            if (State->BlockLeft == 0)
            {
                State->BlockLeft = *In++;
            }

            const uint32_t value = *In++;
            --State->BlockLeft;
            return value;
        }

        /// <summary>GIF_getbcode: the next <paramref name="bits"/> (at most 8) bits, LSB first.</summary>
        uint32_t GetBits(int32_t bits)
        {
            if (State->BitCount == 0)
            {
                State->BitBuffer = GetByte();
                State->BitCount = 8;
            }

            if (State->BitCount < bits)
            {
                State->BitBuffer |= GetByte() << State->BitCount;
                State->BitCount += 8;
            }

            const uint32_t value = State->BitBuffer & GifMasks[bits];
            State->BitCount -= bits;
            State->BitBuffer >>= bits;
            return value;
        }

        /// <summary>GIF_insertcode: adds prefix + first pixel of <paramref name="code"/>.</summary>
        void InsertCode(int32_t code, int32_t prefix) const
        {
            const int32_t next = State->NextCode;

            // Port fix: a full table (a GIF that doesn't clear at 4096 codes) is left alone; the asm wrote past it.
            if (next < 0x1000)
            {
                State->Prefix[next] = static_cast<uint16_t>(prefix);
                State->Suffix[next] = State->First[code];
                State->First[next] = State->First[prefix];
            }

            ++State->NextCode;

            if (State->NextCode == State->CodeLimit && State->CodeSize < 12)
            {
                ++State->CodeSize;
                State->CodeLimit <<= 1;
            }
        }

        /// <summary>
        /// Puts one pixel in the line; a full line goes to the pane and the row advances.
        /// </summary>
        void PutPixel(uint8_t pixel)
        {
            Line[static_cast<size_t>(State->LinePos++)] = pixel;

            if (--State->PixelsLeft != 0)
            {
                return;
            }

            VfxLineToPane(Pane, State->Row, Line.data(), State->Width);
            State->LinePos = 0;
            State->PixelsLeft = State->Width;

            if (State->Interlaced != 0)
            {
                State->Row += GifPassStep[std::min<int32_t>(State->Pass, 4)];

                if (State->Row >= State->Height)
                {
                    ++State->Pass;
                    State->Row = GifPassStart[std::min<int32_t>(State->Pass, 4)];
                }
            }
            else if (++State->Row >= State->Height)
            {
                State->Row = 0;
            }
        }
    };

    /// <summary>The image descriptor: past the header and the global colour table.</summary>
    const uint8_t* GifImageDescriptor(const uint8_t* gif)
    {
        const uint8_t flags = gif[0xa];
        const uint8_t* p = gif + 0xd;

        if (flags & 0x80)
        {
            p += 3 * (1 << ((flags & 7) + 1));
        }

        return p;
    }
}

int32_t VfxGifDraw(MCPane* pane, uint8_t* gif, void* buffer)
{
    MCGifState* state = static_cast<MCGifState*>(buffer);
    std::memset(state, 0, 0x2e);

    MCGifDecoder decoder;
    decoder.State = state;
    decoder.Pane = pane;
    const int32_t background = gif[0xb];

    const uint8_t* descriptor = GifImageDescriptor(gif);
    state->Width = ReadLE16(descriptor + 5);
    state->Height = ReadLE16(descriptor + 7);
    const uint8_t imageFlags = descriptor[9];
    state->Interlaced = imageFlags & 0x40;
    const uint8_t* in = descriptor + 0xa;

    if (imageFlags & 0x80)
    {
        in += 3 * (1 << ((imageFlags & 7) + 1));
    }

    decoder.Line.assign(static_cast<size_t>(std::max(state->Width, 1)), 0);

    state->BlockLeft = 0;
    const int32_t minCodeSize = *in++;
    decoder.In = in;
    const int32_t clearCode = 1 << minCodeSize;
    const int32_t endCode = clearCode + 1;
    state->CodeSize = minCodeSize + 1;
    decoder.InitCodeTable(clearCode);
    int32_t previous = 0xffff;
    bool done = false;
    state->Pass = 0;
    state->PixelsLeft = state->Width;
    state->LinePos = 0;
    state->Row = 0;

    while (!done)
    {
        int32_t code;

        if (state->CodeSize <= 8)
        {
            code = static_cast<int32_t>(decoder.GetBits(state->CodeSize));
        }
        else
        {
            const uint32_t low = decoder.GetBits(8);
            code = static_cast<int32_t>((decoder.GetBits(state->CodeSize - 8) << 8) | low);
        }

        if (code == clearCode)
        {
            decoder.InitCodeTable(clearCode);
            state->CodeSize = minCodeSize + 1;
            previous = 0xffff;
            continue;
        }

        if (code == endCode)
        {
            // Step over the rest of the image data, up to its terminating empty sub-block.
            do
            {
                decoder.In += state->BlockLeft;
                state->BlockLeft = *decoder.In++;
            } while (state->BlockLeft != 0);

            done = true;
            continue;
        }

        if (state->Prefix[code] != 0xfffe)
        {
            if (previous != 0xffff)
            {
                decoder.InsertCode(code, previous);
            }
        }
        else
        {
            // The code being defined now (KwKwK): previous + previous's first pixel.
            // Port fix: without a previous code the asm indexed its tables with 0xffff; the code is dropped instead.
            if (previous == 0xffff)
            {
                continue;
            }

            decoder.InsertCode(previous, previous);
        }

        // Unwind the string onto the stack and output it first pixel first. (The asm also had a path for 1-bit
        // output that split each byte into two pixels; its pixel size was fixed at 8, so it never ran.)
        int32_t length = 0;
        int32_t c = code;

        while (c < 0x1000 && length < 0x1000) // Port fix: bounds for corrupt data; valid chains end at 0xffff.
        {
            state->Stack[length++] = state->Suffix[c];
            c = state->Prefix[c];
        }
        while (length > 0)
        {
            decoder.PutPixel(state->Stack[--length]);
        }

        previous = code;
    }

    return background;
}

void VfxGifPalette(const uint8_t* gif, MCVfxRgb* palette)
{
    const uint8_t* p = gif + 0xd;
    uint8_t* out = &palette[0].R;
    const uint8_t flags = gif[0xa];

    if (flags & 0x80)
    {
        const int32_t count = 3 * (1 << ((flags & 7) + 1));

        for (int32_t i = 0; i < count; ++i)
        {
            out[i] = static_cast<uint8_t>(*p++ >> 2);
        }
    }

    const uint8_t imageFlags = p[9];

    if (imageFlags & 0x80)
    {
        const int32_t count = 3 * (1 << ((imageFlags & 7) + 1));
        p += 0xa;

        for (int32_t i = 0; i < count; ++i)
        {
            out[i] = static_cast<uint8_t>(*p++ >> 2);
        }
    }
}

int32_t VfxGifResolution(uint8_t* gif)
{
    const uint8_t* descriptor = GifImageDescriptor(gif);
    return static_cast<int32_t>((static_cast<uint32_t>(ReadLE16(descriptor + 5)) << 16) | ReadLE16(descriptor + 7));
}

void VfxWindowFade(MCWindow* window, MCVfxRgb* palette, int32_t intervals)
{
    // The asm's scratch tables: current values (0x007a8645), steps left per channel (0x007a8a45), step directions
    // (0x007a9345, which doubles as the seen-colour flags) and error accumulators (0x007a9645, which persist between
    // fades: see below).
    static uint8_t accumulators[0x300];
    uint8_t current[0x300];
    uint8_t deltas[0x300];
    uint8_t directions[0x300];
    uint8_t seen[0x100] = {};
    uint8_t used[0x100];

    // The colours the window uses, in the order met, with their current DAC values.
    const uint32_t pixels = static_cast<uint32_t>(window->XMax + 1) * static_cast<uint32_t>(window->YMax + 1);
    int32_t count = 0;

    for (uint32_t i = 0; i < pixels; ++i)
    {
        const uint8_t color = window->Buffer[i];

        if (seen[color] != 0)
        {
            continue;
        }

        seen[color] = 1;
        used[count++] = color;
        std::memcpy(current + color * 3, &VfxDacPalette[color], 3);
    }

    if (count == 0)
    {
        return; // Port fix: the asm's count-down loops needed at least one pixel.
    }

    uint8_t largest = 0;

    for (int32_t i = count - 1; i >= 0; --i)
    {
        const int32_t base = used[i] * 3;

        for (int32_t channel = 0; channel < 3; ++channel)
        {
            const uint8_t from = current[base + channel];
            const uint8_t to = (&palette[0].R)[base + channel];
            uint8_t difference = static_cast<uint8_t>(from - to);
            uint8_t direction = 0xff;

            if (static_cast<int8_t>(from) < static_cast<int8_t>(to))
            {
                direction = 1;
                difference = static_cast<uint8_t>(-difference);
            }

            directions[base + channel] = direction;
            deltas[base + channel] = difference;

            if (static_cast<int8_t>(difference) > static_cast<int8_t>(largest))
            {
                largest = difference;
            }
        }
    }

    // Each faded colour's accumulators start at half a step. OB-128: the asm reset only the first count - 1 bytes
    // (they are indexed by colour * 3 + channel), so the others kept what earlier fades left.
    for (int32_t i = 0; i < count; ++i)
    {
        std::memset(accumulators + used[i] * 3, largest >> 1, 3);
    }

    if (largest == 0)
    {
        return;
    }

    // Spread the steps over the intervals: 16.16 retraces per step, rounded by starting at one half.
    const uint32_t retracesPerStep =
        static_cast<uint32_t>((static_cast<uint64_t>(static_cast<uint32_t>(intervals)) << 16) / largest);
    uint32_t retraces = 0x8000;

    for (uint32_t step = largest; step != 0; --step)
    {
        for (int32_t i = count - 1; i >= 0; --i)
        {
            const uint8_t color = used[i];
            const int32_t base = color * 3;

            for (int32_t channel = 2; channel >= 0; --channel)
            {
                uint8_t sum = static_cast<uint8_t>(accumulators[base + channel] + deltas[base + channel]);

                if (static_cast<int8_t>(sum) >= static_cast<int8_t>(largest))
                {
                    sum = static_cast<uint8_t>(sum - largest);
                    current[base + channel] =
                        static_cast<uint8_t>(current[base + channel] + directions[base + channel]);
                }

                accumulators[base + channel] = sum;
            }

            std::memcpy(&VfxDacPalette[color], current + base, 3);

            if (VfxDacWriteHook != nullptr)
            {
                VfxDacWriteHook(color, &VfxDacPalette[color]);
            }
        }

        retraces += retracesPerStep;
        int16_t wholeRetraces = static_cast<int16_t>(retraces >> 16);

        if (wholeRetraces >= 1)
        {
            for (; wholeRetraces != 0; --wholeRetraces)
            {
                if (VfxWaitRetraceHook != nullptr)
                {
                    VfxWaitRetraceHook();
                }
            }

            retraces &= 0xffff;
        }
    }
}

int32_t VfxColorScan(MCPane* pane, uint32_t* colors)
{
    uint8_t seen[0x100] = {};
    MCVfxClip clip;

    // Port: a view has no pixels to read.
    if (MCVfxClipPane(pane, clip) != 0 || clip.Buffer == nullptr)
    {
        return 0;
    }

    // OB-123: rows y0..y1 and columns x1 down to x0; the asm didn't clip them to the window.
    const int32_t lastColumn = clip.X1 - clip.X0;
    const uint8_t* row = clip.At(clip.X0, clip.Y0);
    int32_t count = 0;

    for (int32_t rowsLeft = clip.Y1 - clip.Y0; rowsLeft >= 0; --rowsLeft, row += clip.Stride)
    {
        for (int32_t x = lastColumn; x >= 0; --x)
        {
            const uint8_t color = row[x];

            if (seen[color] != 0)
            {
                continue;
            }

            seen[color] = 1;
            ++count;

            if (colors != nullptr)
            {
                *colors++ = color;
            }
        }
    }

    return count;
}
