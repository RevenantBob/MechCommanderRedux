#include "stdafx.h"
#include "vfx/MCVfxClip.h"

// VFX's picture decoders (IFF ILBM/PBM, PCX, GIF), its palette fade and its colour scan (vfxa.asm in MCX.EXE). The
// game draws its GIF art (aPort, TacticalMap) and takes its palette from data\palette\palettex.gif through these.
//
// The asm shared a few static scratch buffers between these routines (a 3328-byte line buffer at 0x007a8645, a
// 768-byte buffer at 0x007a9345, ...); the port gives each routine its own, sized to the data.

std::array<MCVfxRgb, 256> VfxDacPalette{};
std::function<void(int32_t index, const MCVfxRgb& rgb)> VfxDacWriteHook;
std::function<void()> VfxWaitRetraceHook;

namespace
{
    uint16_t ReadBE16(std::span<const uint8_t> data, size_t offset = 0)
    {
        return static_cast<uint16_t>((data[offset] << 8) | data[offset + 1]);
    }

    uint16_t ReadLE16(std::span<const uint8_t> data, size_t offset = 0)
    {
        return static_cast<uint16_t>(data[offset] | (data[offset + 1] << 8));
    }

    /// <summary>
    /// Finds the IFF chunk tagged <paramref name="tag"/> after the FORM header (12 bytes): zero padding bytes
    /// between chunks are skipped, and only the low 16 bits of each chunk's length are used.
    /// </summary>
    /// <returns>The chunk's data (past its tag and length) to the end of the file.</returns>
    /// <remarks>
    /// Original behaviour: there is no end check, so a missing chunk runs past the file (the span stops it).
    /// </remarks>
    std::span<const uint8_t> FindIffChunk(std::string_view tag, std::span<const uint8_t> iff)
    {
        size_t chunk = 0xc;

        for (;;)
        {
            while (iff[chunk] == 0)
            {
                ++chunk;
            }

            if (std::memcmp(iff.subspan(chunk, 4).data(), tag.data(), 4) == 0)
            {
                return iff.subspan(chunk + 8);
            }

            chunk += 8 + ReadBE16(iff, chunk + 6);
        }
    }
}

void VfxLineToPane(const MCPane& pane, int32_t y, std::span<const uint8_t> line)
{
    MCVfxClip clip;

    if (MCVfxClipPane(pane, clip) != 0)
    {
        return;
    }

    int32_t x = clip.PaneX;
    y += clip.PaneY;
    auto count = static_cast<int32_t>(line.size());
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

        line = line.subspan(static_cast<size_t>(-over));
        x -= over;
    }

    if (y > clip.Y1 || y < clip.Y0)
    {
        return;
    }

    MCRenderer::For(clip.Target()).Write(clip.Target(), x, y, line.first(static_cast<size_t>(count)));
}

int32_t VfxIlbmDraw(const MCPane& pane, std::span<const uint8_t> ilbm)
{
    const int32_t paneWidth = pane.X1 - pane.X0 + 1;
    const int32_t paneHeight = pane.Y1 - pane.Y0 + 1;
    // Anything but FORM ILBM is taken for a chunky PBM.
    const bool planar = std::memcmp(ilbm.subspan(8, 4).data(), "ILBM", 4) == 0;

    const std::span<const uint8_t> header = FindIffChunk("BMHD", ilbm);
    int32_t width = ReadBE16(header);
    int32_t rows = std::min<int32_t>(ReadBE16(header, 2), paneHeight);

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

    const std::span<const uint8_t> bodyData = FindIffChunk("BODY", ilbm);
    size_t body = 0;
    std::vector<uint8_t> unpacked(static_cast<size_t>(rowLength) + 256);
    std::vector<uint8_t> line(static_cast<size_t>(std::max(width, 1)));

    // Port fix: a picture (or pane) without rows draws nothing; the asm's count-down loop ran 2^32 times.
    for (int32_t y = 0; y < rows; ++y)
    {
        std::span<const uint8_t> row;

        if (compression == 1)
        {
            // ByteRun1 into the row buffer, until at least a chunky row's length is out. Original behaviour: an
            // ILBM row (8 planes of planeBytes) is only that long when the width is a multiple of 16.
            size_t out = 0;
            const auto end = static_cast<size_t>(rowLength);

            while (out < end)
            {
                const uint32_t n = bodyData[body++];

                if (n == 0x80)
                {
                    continue;
                }

                if (n < 0x80)
                {
                    std::ranges::copy(bodyData.subspan(body, n + 1), unpacked.begin() + static_cast<ptrdiff_t>(out));
                    out += n + 1;
                    body += n + 1;
                }
                else
                {
                    const uint32_t count = 257 - n;
                    std::fill_n(unpacked.begin() + static_cast<ptrdiff_t>(out), count, bodyData[body++]);
                    out += count;
                }
            }

            row = unpacked;
        }
        else
        {
            row = bodyData.subspan(body);
            body += static_cast<size_t>(rowLength);
        }

        if (planar)
        {
            // Eight bitplanes, planeBytes apart; bit 7 of each plane byte is the leftmost pixel.
            size_t out = 0;
            int32_t pixels = width;
            size_t column = 0;

            for (int32_t byteColumn = planeBytes; byteColumn > 0 && pixels > 0; --byteColumn, ++column)
            {
                for (uint32_t bit = 0x80; bit != 0 && pixels > 0; bit >>= 1, --pixels)
                {
                    uint8_t value = 0;

                    for (int32_t plane = 0; plane < 8; ++plane)
                    {
                        if (row[column + static_cast<size_t>(plane * planeBytes)] & bit)
                        {
                            value |= static_cast<uint8_t>(1 << plane);
                        }
                    }

                    line[out++] = value;
                }
            }

            VfxLineToPane(pane, y, std::span(line).first(static_cast<size_t>(width)));
        }
        else
        {
            VfxLineToPane(pane, y, row.first(static_cast<size_t>(width)));
        }
    }

    return transparent;
}

int32_t VfxIlbmResolution(std::span<const uint8_t> ilbm)
{
    const std::span<const uint8_t> header = FindIffChunk("BMHD", ilbm);
    return static_cast<int32_t>((static_cast<uint32_t>(ReadBE16(header)) << 16) | ReadBE16(header, 2));
}

int32_t VfxPcxDraw(const MCPane& pane, std::span<const uint8_t> pcx)
{
    // Rows 0..(yMax - yMin), each bytesPerLine bytes of RLE: a byte with its top two bits set repeats the next byte
    // (its low six bits) times. Original behaviour: a run may spill past the row's end into the line buffer's slack
    // and is then dropped, as in the asm.
    const int32_t lastRow = static_cast<uint16_t>(ReadLE16(pcx, 0xa) - ReadLE16(pcx, 6));
    const int32_t bytesPerLine = ReadLE16(pcx, 0x42);
    std::vector<uint8_t> line(static_cast<size_t>(bytesPerLine) + 64);
    size_t in = 0x80;

    for (int32_t y = 0; y <= lastRow; ++y)
    {
        size_t out = 0;
        const auto end = static_cast<size_t>(bytesPerLine);

        do
        {
            const uint8_t value = pcx[in++];

            if ((value & 0xc0) == 0xc0)
            {
                const uint32_t count = value & 0x3f;
                std::fill_n(line.begin() + static_cast<ptrdiff_t>(out), count, pcx[in++]);
                out += count;
            }
            else
            {
                line[out++] = value;
            }
        } while (out < end);

        VfxLineToPane(pane, y, std::span(line).first(static_cast<size_t>(bytesPerLine)));
    }

    return 0;
}

void VfxPcxPalette(std::span<const uint8_t> pcx, std::span<MCVfxRgb, 256> palette)
{
    const std::span<const uint8_t> colors = pcx.last(0x300);
    const std::span<uint8_t> out(reinterpret_cast<uint8_t*>(palette.data()), sizeof(MCVfxRgb) * palette.size());

    for (size_t i = 0; i < out.size(); ++i)
    {
        out[i] = static_cast<uint8_t>(colors[i] >> 2);
    }
}

int32_t VfxPcxResolution(std::span<const uint8_t> pcx)
{
    const uint16_t width = static_cast<uint16_t>(ReadLE16(pcx, 8) - ReadLE16(pcx, 4) + 1);
    const uint16_t height = static_cast<uint16_t>(ReadLE16(pcx, 0xa) - ReadLE16(pcx, 6) + 1);
    return static_cast<int32_t>((static_cast<uint32_t>(width) << 16) | height);
}

namespace
{
#pragma pack(push, 1)
    /// <summary>The LZW state, laid out as the asm used the caller's work buffer.</summary>
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

    /// <summary>One GIF decode: the state, the data and where it reads, the pane drawn into.</summary>
    struct MCGifDecoder
    {
        MCGifState& State;
        std::span<const uint8_t> Data;
        /// <summary>The next byte read, as an offset into <see cref="Data"/>.</summary>
        size_t In = 0;
        const MCPane& Pane;
        std::vector<uint8_t> Line;

        /// <summary>GIF_init_codetable.</summary>
        void InitCodeTable(int32_t clearCode) const
        {
            State.NextCode = clearCode + 2;
            State.CodeLimit = clearCode * 2;
            int32_t i = 0;

            for (; i < clearCode; ++i)
            {
                State.First[i] = static_cast<uint8_t>(i);
                State.Suffix[i] = static_cast<uint8_t>(i);
                State.Prefix[i] = 0xffff;
            }

            for (; i < 0x1000; ++i)
            {
                State.Prefix[i] = 0xfffe;
            }
        }

        /// <summary>Set once a read ran past the file: the image ends there.</summary>
        bool Ended = false;

        /// <summary>The next byte of the file (0, and <see cref="Ended"/> set, past its end).</summary>
        uint8_t Next()
        {
            if (In >= Data.size())
            {
                Ended = true;
                return 0;
            }

            return Data[In++];
        }

        /// <summary>GIF_getb: the next data byte, stepping over sub-block lengths.</summary>
        uint32_t GetByte()
        {
            if (State.BlockLeft == 0)
            {
                State.BlockLeft = Next();
            }

            const uint32_t value = Next();
            --State.BlockLeft;
            return value;
        }

        /// <summary>GIF_getbcode: the next <paramref name="bits"/> (at most 8) bits, LSB first.</summary>
        uint32_t GetBits(int32_t bits)
        {
            if (State.BitCount == 0)
            {
                State.BitBuffer = GetByte();
                State.BitCount = 8;
            }

            if (State.BitCount < bits)
            {
                State.BitBuffer |= GetByte() << State.BitCount;
                State.BitCount += 8;
            }

            const uint32_t value = State.BitBuffer & GifMasks[bits];
            State.BitCount -= bits;
            State.BitBuffer >>= bits;
            return value;
        }

        /// <summary>GIF_insertcode: adds prefix + first pixel of <paramref name="code"/>.</summary>
        void InsertCode(int32_t code, int32_t prefix) const
        {
            const int32_t next = State.NextCode;

            // Port fix: a full table (a GIF that doesn't clear at 4096 codes) is left alone; the asm wrote past it.
            if (next < 0x1000)
            {
                State.Prefix[next] = static_cast<uint16_t>(prefix);
                State.Suffix[next] = State.First[code];
                State.First[next] = State.First[prefix];
            }

            ++State.NextCode;

            if (State.NextCode == State.CodeLimit && State.CodeSize < 12)
            {
                ++State.CodeSize;
                State.CodeLimit <<= 1;
            }
        }

        /// <summary>
        /// Puts one pixel in the line; a full line goes to the pane and the row advances.
        /// </summary>
        void PutPixel(uint8_t pixel)
        {
            Line[static_cast<size_t>(State.LinePos++)] = pixel;

            if (--State.PixelsLeft != 0)
            {
                return;
            }

            VfxLineToPane(Pane, State.Row, std::span(Line).first(static_cast<size_t>(State.Width)));
            State.LinePos = 0;
            State.PixelsLeft = State.Width;

            if (State.Interlaced != 0)
            {
                State.Row += GifPassStep[std::min<int32_t>(State.Pass, 4)];

                if (State.Row >= State.Height)
                {
                    ++State.Pass;
                    State.Row = GifPassStart[std::min<int32_t>(State.Pass, 4)];
                }
            }
            else if (++State.Row >= State.Height)
            {
                State.Row = 0;
            }
        }
    };

    /// <summary>Where the parts of a GIF the routines read lie, checked against the file.</summary>
    /// <remarks>
    /// As the original, the image descriptor is taken to follow the global colour table (no extension blocks).
    /// </remarks>
    struct MCGifLayout
    {
        uint8_t Background = 0;
        /// <summary>The global colour table's offset and size in bytes (0 without one).</summary>
        size_t GlobalPalette = 0;
        size_t GlobalPaletteSize = 0;
        int32_t Width = 0;
        int32_t Height = 0;
        bool Interlaced = false;
        /// <summary>The local colour table's offset and size in bytes (0 without one).</summary>
        size_t LocalPalette = 0;
        size_t LocalPaletteSize = 0;
        int32_t MinCodeSize = 0;
        /// <summary>The first sub-block of the image data.</summary>
        size_t Data = 0;
    };

    /// <summary>
    /// The layout of <paramref name="gif"/>, or why it can't be drawn: too short for its header, colour tables or
    /// image descriptor, an image with no pixels, or an LZW code size the 4096-entry tables can't hold.
    /// </summary>
    /// <remarks>
    /// Issue #25: the file comes from the data (a mission's tactical-map picture); the original read past it, wrote
    /// a zero-width image's pixels past its line, and ran its code tables over with a code size from 12 on.
    /// </remarks>
    std::expected<MCGifLayout, std::string> ReadGifLayout(std::span<const uint8_t> gif)
    {
        MCGifLayout layout;

        if (gif.size() < 0xd)
        {
            return std::unexpected("shorter than a GIF header");
        }

        const uint8_t flags = gif[0xa];
        layout.Background = gif[0xb];
        size_t p = 0xd;

        if ((flags & 0x80) != 0)
        {
            layout.GlobalPalette = p;
            layout.GlobalPaletteSize = static_cast<size_t>(3 * (1 << ((flags & 7) + 1)));
            p += layout.GlobalPaletteSize;
        }

        if (gif.size() < p + 0xa)
        {
            return std::unexpected("no image descriptor");
        }

        layout.Width = ReadLE16(gif, p + 5);
        layout.Height = ReadLE16(gif, p + 7);
        const uint8_t imageFlags = gif[p + 9];
        layout.Interlaced = (imageFlags & 0x40) != 0;
        p += 0xa;

        if (layout.Width == 0 || layout.Height == 0)
        {
            return std::unexpected(std::format("an image of {}x{} pixels", layout.Width, layout.Height));
        }

        if ((imageFlags & 0x80) != 0)
        {
            layout.LocalPalette = p;
            layout.LocalPaletteSize = static_cast<size_t>(3 * (1 << ((imageFlags & 7) + 1)));
            p += layout.LocalPaletteSize;
        }

        if (gif.size() < p + 1)
        {
            return std::unexpected("no image data");
        }

        layout.MinCodeSize = gif[p];
        layout.Data = p + 1;

        // The codes grow to 12 bits at most: the clear code and the end code must leave room in the tables.
        if (layout.MinCodeSize < 1 || layout.MinCodeSize > 11)
        {
            return std::unexpected(std::format("an LZW code size of {}", layout.MinCodeSize));
        }

        return layout;
    }
}

std::expected<int32_t, std::string> VfxGifDraw(const MCPane& pane, std::span<const uint8_t> gif)
{
    const std::expected<MCGifLayout, std::string> layout = ReadGifLayout(gif);

    if (!layout.has_value())
    {
        return std::unexpected(layout.error());
    }

    // The work buffer the game passed held this state; it is the routine's own now (zeroed, where the asm cleared
    // its first 0x2e bytes and filled the tables before reading them).
    const auto stateHolder = std::make_unique<MCGifState>();
    MCGifState& state = *stateHolder;

    MCGifDecoder decoder{.State = state, .Data = gif, .Pane = pane};
    const int32_t background = layout->Background;
    state.Width = layout->Width;
    state.Height = layout->Height;
    state.Interlaced = layout->Interlaced ? 0x40 : 0;
    decoder.Line.assign(static_cast<size_t>(state.Width), 0);

    state.BlockLeft = 0;
    const int32_t minCodeSize = layout->MinCodeSize;
    decoder.In = layout->Data;
    const int32_t clearCode = 1 << minCodeSize;
    const int32_t endCode = clearCode + 1;
    state.CodeSize = minCodeSize + 1;
    decoder.InitCodeTable(clearCode);
    int32_t previous = 0xffff;
    bool done = false;
    state.Pass = 0;
    state.PixelsLeft = state.Width;
    state.LinePos = 0;
    state.Row = 0;

    // A file that ends inside the image draws what it holds.
    while (!done && !decoder.Ended)
    {
        int32_t code;

        if (state.CodeSize <= 8)
        {
            code = static_cast<int32_t>(decoder.GetBits(state.CodeSize));
        }
        else
        {
            const uint32_t low = decoder.GetBits(8);
            code = static_cast<int32_t>((decoder.GetBits(state.CodeSize - 8) << 8) | low);
        }

        if (code == clearCode)
        {
            decoder.InitCodeTable(clearCode);
            state.CodeSize = minCodeSize + 1;
            previous = 0xffff;
            continue;
        }

        if (code == endCode)
        {
            // Step over the rest of the image data, up to its terminating empty sub-block.
            do
            {
                decoder.In += static_cast<size_t>(state.BlockLeft);
                state.BlockLeft = decoder.Next();
            } while (state.BlockLeft != 0 && !decoder.Ended);

            done = true;
            continue;
        }

        if (state.Prefix[code] != 0xfffe)
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
            state.Stack[length++] = state.Suffix[c];
            c = state.Prefix[c];
        }
        while (length > 0)
        {
            decoder.PutPixel(state.Stack[--length]);
        }

        previous = code;
    }

    return background;
}

std::expected<void, std::string> VfxGifPalette(std::span<const uint8_t> gif, std::span<MCVfxRgb, 256> palette)
{
    const std::expected<MCGifLayout, std::string> layout = ReadGifLayout(gif);

    if (!layout.has_value())
    {
        return std::unexpected(layout.error());
    }

    const std::span<uint8_t> out(reinterpret_cast<uint8_t*>(palette.data()), sizeof(MCVfxRgb) * palette.size());

    // The global table, then the local one over it (each at most 256 colours, which the palette holds).
    for (const auto [offset, size] : {std::pair(layout->GlobalPalette, layout->GlobalPaletteSize),
                                      std::pair(layout->LocalPalette, layout->LocalPaletteSize)})
    {
        for (size_t i = 0; i < size; ++i)
        {
            out[i] = static_cast<uint8_t>(gif[offset + i] >> 2);
        }
    }

    return {};
}

std::expected<int32_t, std::string> VfxGifResolution(std::span<const uint8_t> gif)
{
    const std::expected<MCGifLayout, std::string> layout = ReadGifLayout(gif);

    if (!layout.has_value())
    {
        return std::unexpected(layout.error());
    }

    return static_cast<int32_t>((static_cast<uint32_t>(layout->Width) << 16) | static_cast<uint32_t>(layout->Height));
}

void VfxWindowFade(const MCWindow& window, std::span<const MCVfxRgb, 256> palette, int32_t intervals)
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
    int32_t count = 0;

    for (const uint8_t color : window.Bytes())
    {
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
            const uint8_t to = reinterpret_cast<const uint8_t*>(palette.data())[base + channel];
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

            if (VfxDacWriteHook)
            {
                VfxDacWriteHook(color, VfxDacPalette[color]);
            }
        }

        retraces += retracesPerStep;
        int16_t wholeRetraces = static_cast<int16_t>(retraces >> 16);

        if (wholeRetraces >= 1)
        {
            for (; wholeRetraces != 0; --wholeRetraces)
            {
                if (VfxWaitRetraceHook)
                {
                    VfxWaitRetraceHook();
                }
            }

            retraces &= 0xffff;
        }
    }
}

int32_t VfxColorScan(const MCPane& pane, std::span<uint32_t> colors)
{
    uint8_t seen[0x100] = {};
    MCVfxClip clip;

    // Port: a view has no pixels to read.
    if (MCVfxClipPane(pane, clip) != 0 || !clip.HasPixels())
    {
        return 0;
    }

    // OB-123: rows y0..y1 and columns x1 down to x0; the asm didn't clip them to the window.
    const int32_t lastColumn = clip.X1 - clip.X0;
    int32_t count = 0;

    for (int32_t y = clip.Y0; y <= clip.Y1; ++y)
    {
        const std::span<const uint8_t> row = clip.Row(clip.X0, y);

        for (int32_t x = lastColumn; x >= 0; --x)
        {
            const uint8_t color = row[static_cast<size_t>(x)];

            if (seen[color] != 0)
            {
                continue;
            }

            seen[color] = 1;

            if (static_cast<size_t>(count) < colors.size())
            {
                colors[static_cast<size_t>(count)] = color;
            }

            ++count;
        }
    }

    return count;
}
