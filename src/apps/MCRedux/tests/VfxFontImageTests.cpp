#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "lib/file.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>An 8-bit window with a pane over all of it.</summary>
    struct TestSurface
    {
        std::vector<uint8_t> Pixels;
        MCWindow Window{};
        MCPane Pane{};

        TestSurface(int32_t width, int32_t height, uint8_t fill = 0) : Pixels(static_cast<size_t>(width) * height, fill)
        {
            Window = MCWindow{Pixels.data(), width - 1, height - 1};
            Pane = MCPane{&Window, 0, 0, width - 1, height - 1};
        }

        uint8_t At(int32_t x, int32_t y) const { return Pixels[static_cast<size_t>(y) * (Window.XMax + 1) + x]; }
    };

    void Put32(std::vector<uint8_t>& data, uint32_t value)
    {
        for (int i = 0; i < 4; ++i)
        {
            data.push_back(static_cast<uint8_t>(value >> (i * 8)));
        }
    }

    /// <summary>
    /// A font of 256 characters, 3 rows high: 'A' is 2 wide with pixels 1..6, 'B' 3 wide with 7..15, the rest empty
    /// (width 0).
    /// </summary>
    std::vector<uint8_t> MakeFont()
    {
        std::vector<uint8_t> font;
        Put32(font, 1);   // version
        Put32(font, 256); // char_count
        Put32(font, 3);   // char_height
        Put32(font, 0);   // font_background
        const size_t offsets = font.size();
        font.resize(offsets + 256 * 4);
        const auto setOffset = [&](int c, uint32_t offset) { std::memcpy(font.data() + offsets + c * 4, &offset, 4); };
        const uint32_t empty = static_cast<uint32_t>(font.size());
        Put32(font, 0);

        for (int c = 0; c < 256; ++c)
        {
            setOffset(c, empty);
        }

        setOffset('A', static_cast<uint32_t>(font.size()));
        Put32(font, 2);

        for (uint8_t p = 1; p <= 6; ++p)
        {
            font.push_back(p);
        }

        setOffset('B', static_cast<uint32_t>(font.size()));
        Put32(font, 3);

        for (uint8_t p = 7; p <= 15; ++p)
        {
            font.push_back(p);
        }

        return font;
    }

    /// <summary>LSB-first bit writer for GIF LZW data.</summary>
    struct BitWriter
    {
        std::vector<uint8_t> Bytes;
        uint32_t Buffer = 0;
        int Count = 0;

        void Write(uint32_t code, int bits)
        {
            Buffer |= code << Count;
            Count += bits;

            while (Count >= 8)
            {
                Bytes.push_back(static_cast<uint8_t>(Buffer));
                Buffer >>= 8;
                Count -= 8;
            }
        }

        void Flush()
        {
            if (Count > 0)
            {
                Bytes.push_back(static_cast<uint8_t>(Buffer));
            }

            Buffer = 0;
            Count = 0;
        }
    };

    /// <summary>
    /// A GIF87a of 4-colour pixels (minimum code size 2): a clear code before every two literal codes keeps the code
    /// size at 3 bits, which any LZW decoder reads back.
    /// </summary>
    std::vector<uint8_t> MakeGif(int width, int height, const std::vector<uint8_t>& pixels, bool interlaced)
    {
        std::vector<uint8_t> gif = {'G', 'I', 'F', '8', '7', 'a'};
        gif.push_back(static_cast<uint8_t>(width));
        gif.push_back(static_cast<uint8_t>(width >> 8));
        gif.push_back(static_cast<uint8_t>(height));
        gif.push_back(static_cast<uint8_t>(height >> 8));
        gif.push_back(0x81); // global colour table of 4 entries
        gif.push_back(3);    // background
        gif.push_back(0);
        const uint8_t colors[12] = {0, 0, 0, 252, 0, 0, 0, 128, 0, 4, 8, 255};
        gif.insert(gif.end(), colors, colors + 12);
        gif.push_back(',');

        for (int i = 0; i < 4; ++i)
        {
            gif.push_back(0);
        }

        gif.push_back(static_cast<uint8_t>(width));
        gif.push_back(static_cast<uint8_t>(width >> 8));
        gif.push_back(static_cast<uint8_t>(height));
        gif.push_back(static_cast<uint8_t>(height >> 8));
        gif.push_back(interlaced ? 0x40 : 0);
        gif.push_back(2); // minimum code size

        BitWriter bits;

        for (size_t i = 0; i < pixels.size(); ++i)
        {
            if (i % 2 == 0)
            {
                bits.Write(4, 3);
            }

            bits.Write(pixels[i], 3);
        }

        bits.Write(5, 3);
        bits.Flush();

        for (size_t i = 0; i < bits.Bytes.size(); i += 255)
        {
            const size_t n = std::min<size_t>(255, bits.Bytes.size() - i);
            gif.push_back(static_cast<uint8_t>(n));
            gif.insert(gif.end(), bits.Bytes.begin() + static_cast<ptrdiff_t>(i),
                       bits.Bytes.begin() + static_cast<ptrdiff_t>(i + n));
        }

        gif.push_back(0);
        gif.push_back(';');
        return gif;
    }

    std::vector<uint8_t> ReadGameFile(const char* name)
    {
        MCFile file;

        if (file.Open(name) != 0)
        {
            return {};
        }

        std::vector<uint8_t> data(file.FileSize());
        file.Read(data.data(), static_cast<int32_t>(data.size()));
        return data;
    }
}

TEST_CASE("vfx font: glyphs copy whole without a table, and clip to the pane")
{
    std::vector<uint8_t> font = MakeFont();
    CHECK_EQ(VfxFontHeight(font.data()), 3);
    CHECK_EQ(VfxCharacterWidth(font.data(), 'A'), 2);
    CHECK_EQ(VfxCharacterWidth(font.data(), 'B'), 3);

    TestSurface surface(8, 6, 99);
    CHECK_EQ(VfxCharacterDraw(&surface.Pane, 1, 2, font.data(), 'B', nullptr), 3);

    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            CHECK_EQ(surface.At(1 + x, 2 + y), static_cast<uint8_t>(7 + y * 3 + x));
        }
    }

    CHECK_EQ(surface.At(0, 2), 99);
    CHECK_EQ(surface.At(4, 2), 99);
    CHECK_EQ(surface.At(1, 1), 99);

    // Clipped at the top-left corner of a pane that starts at (2, 1): only the glyph's bottom-right pixel lands.
    TestSurface clipped(8, 6, 99);
    clipped.Pane = MCPane{&clipped.Window, 2, 1, 5, 4};
    CHECK_EQ(VfxCharacterDraw(&clipped.Pane, -2, -2, font.data(), 'B', nullptr), 3);
    CHECK_EQ(clipped.At(2, 1), 15);
    CHECK_EQ(clipped.At(1, 1), 99);
    CHECK_EQ(clipped.At(2, 0), 99);
    // Wholly outside: nothing drawn, the width still returned.
    CHECK_EQ(VfxCharacterDraw(&clipped.Pane, 10, 0, font.data(), 'A', nullptr), 2);
    // Clipped at the right and bottom.
    CHECK_EQ(VfxCharacterDraw(&clipped.Pane, 2, 2, font.data(), 'B', nullptr), 3);
    CHECK_EQ(clipped.At(4, 3), 7);
    CHECK_EQ(clipped.At(5, 3), 8);
    CHECK_EQ(clipped.At(5, 4), 11);
    CHECK_EQ(clipped.At(6, 3), 99);
    CHECK_EQ(clipped.At(4, 5), 99);
}

TEST_CASE("vfx font: a colour table maps pixels, 255 is transparent, strings advance")
{
    std::vector<uint8_t> font = MakeFont();
    uint8_t table[256];

    for (int i = 0; i < 256; ++i)
    {
        table[i] = static_cast<uint8_t>(i + 100);
    }

    table[1] = 255;
    table[4] = 255;

    TestSurface surface(8, 4, 50);
    VfxStringDraw(&surface.Pane, 0, 0, font.data(), "AB", table);
    CHECK_EQ(surface.At(0, 0), 50); // pixel 1 transparent
    CHECK_EQ(surface.At(1, 0), 102);
    CHECK_EQ(surface.At(0, 1), 103);
    CHECK_EQ(surface.At(1, 1), 50);  // pixel 4 transparent
    CHECK_EQ(surface.At(2, 0), 107); // 'B' starts after 'A''s 2 columns
    CHECK_EQ(surface.At(4, 2), 115);
    CHECK_EQ(surface.At(5, 0), 50);
}

TEST_CASE("vfx pcx: an 8-bit RLE picture decodes row by row")
{
    std::vector<uint8_t> pcx(0x80, 0);
    pcx[0] = 10;
    pcx[1] = 5;
    pcx[2] = 1;
    pcx[3] = 8;
    pcx[8] = 3;   // xMax
    pcx[0xa] = 1; // yMax
    pcx[0x41] = 1;
    pcx[0x42] = 4; // bytes per line
    const uint8_t data[] = {0xc3, 7, 9, /* row 1 */ 0xc1, 0xc5, 1, 2, 0xc2, 3};
    pcx.insert(pcx.end(), data, data + sizeof(data));
    std::vector<uint8_t> palette(0x300);

    for (int i = 0; i < 0x300; ++i)
    {
        palette[i] = static_cast<uint8_t>(i);
    }

    pcx.push_back(12);
    pcx.insert(pcx.end(), palette.begin(), palette.end());

    CHECK_EQ(VfxPcxResolution(pcx.data()), (4 << 16) | 2);
    TestSurface surface(6, 3, 0xee);
    CHECK_EQ(VfxPcxDraw(&surface.Pane, pcx.data()), 0);
    const uint8_t expected[2][4] = {{7, 7, 7, 9}, {0xc5, 1, 2, 3}};

    for (int y = 0; y < 2; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            CHECK_EQ(surface.At(x, y), expected[y][x]);
        }
    }

    CHECK_EQ(surface.At(4, 0), 0xee);
    CHECK_EQ(surface.At(0, 2), 0xee);

    MCVfxRgb out[256];
    VfxPcxPalette(pcx.data(), static_cast<int32_t>(pcx.size()), out);
    CHECK_EQ(out[1].R, 3 >> 2);
    CHECK_EQ(out[255].B, 0x2ff % 256 >> 2);
}

TEST_CASE("vfx gif: plain and interlaced pictures decode to their pixels")
{
    const int width = 3;
    const int height = 8;
    std::vector<uint8_t> pixels(width * height);

    for (size_t i = 0; i < pixels.size(); ++i)
    {
        pixels[i] = static_cast<uint8_t>((i * 7 + i / 5) % 4);
    }

    std::vector<uint8_t> gif = MakeGif(width, height, pixels, false);
    CHECK_EQ(VfxGifResolution(gif.data()), (width << 16) | height);
    std::vector<uint8_t> work(VFX_GIF_BUFFER_SIZE);
    TestSurface surface(5, 10, 0xee);
    CHECK_EQ(VfxGifDraw(&surface.Pane, gif.data(), work.data()), 3);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            CHECK_EQ(surface.At(x, y), pixels[static_cast<size_t>(y * width + x)]);
        }
    }

    CHECK_EQ(surface.At(3, 0), 0xee);
    CHECK_EQ(surface.At(0, 8), 0xee);

    // Interlaced: stored rows 0, 4 | 2, 6 | 1, 3, 5, 7.
    const int order[8] = {0, 4, 2, 6, 1, 3, 5, 7};
    std::vector<uint8_t> stored;

    for (int row : order)
    {
        stored.insert(stored.end(), pixels.begin() + row * width, pixels.begin() + (row + 1) * width);
    }

    gif = MakeGif(width, height, stored, true);
    TestSurface interlaced(5, 10, 0xee);
    VfxGifDraw(&interlaced.Pane, gif.data(), work.data());

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            CHECK_EQ(interlaced.At(x, y), pixels[static_cast<size_t>(y * width + x)]);
        }
    }

    MCVfxRgb palette[256] = {};
    VfxGifPalette(gif.data(), palette);
    CHECK_EQ(palette[1].R, 63);
    CHECK_EQ(palette[2].G, 32);
    CHECK_EQ(palette[3].B, 63);
    CHECK_EQ(palette[4].R, 0);
}

TEST_CASE("vfx ilbm: PBM pictures decode, raw and ByteRun1")
{
    const auto makeIff = [](const char* type, uint8_t compression, const std::vector<uint8_t>& body)
    {
        std::vector<uint8_t> iff = {'F', 'O', 'R', 'M', 0, 0, 0, 0};
        iff.insert(iff.end(), type, type + 4);
        const uint8_t bmhd[] = {'B', 'M', 'H', 'D', 0,           0, 0, 20, 0, 4, 0, 2, 0, 0,
                                0,   0,   8,   0,   compression, 0, 0, 9,  1, 1, 0, 4, 0, 2};
        iff.insert(iff.end(), bmhd, bmhd + sizeof(bmhd));
        const uint8_t header[] = {'B', 'O', 'D', 'Y', 0, 0, 0, static_cast<uint8_t>(body.size())};
        iff.insert(iff.end(), header, header + sizeof(header));
        iff.insert(iff.end(), body.begin(), body.end());
        return iff;
    };

    std::vector<uint8_t> raw = makeIff("PBM ", 0, {1, 2, 3, 4, 5, 6, 7, 8});
    CHECK_EQ(VfxIlbmResolution(raw.data()), (4 << 16) | 2);
    TestSurface surface(5, 3, 0xee);
    CHECK_EQ(VfxIlbmDraw(&surface.Pane, raw.data()), 9);

    for (int i = 0; i < 8; ++i)
    {
        CHECK_EQ(surface.At(i % 4, i / 4), static_cast<uint8_t>(i + 1));
    }

    CHECK_EQ(surface.At(4, 0), 0xee);

    std::vector<uint8_t> packed = makeIff("PBM ", 1, {0xfd, 5, /* row 1 */ 1, 6, 7, 0xff, 8});
    TestSurface unpacked(5, 3, 0xee);
    VfxIlbmDraw(&unpacked.Pane, packed.data());
    const uint8_t expected[8] = {5, 5, 5, 5, 6, 7, 8, 8};

    for (int i = 0; i < 8; ++i)
    {
        CHECK_EQ(unpacked.At(i % 4, i / 4), expected[i]);
    }
}

TEST_CASE("vfx color_scan: distinct colours in scan order")
{
    TestSurface surface(4, 3, 0);
    surface.Pixels = {1, 2, 3, 4, /**/ 5, 6, 7, 8, /**/ 9, 9, 9, 9};
    MCPane pane{&surface.Window, 1, 0, 2, 1};
    uint32_t colors[256] = {};
    CHECK_EQ(VfxColorScan(&pane, colors), 4);
    // Rows top to bottom, each right to left.
    CHECK_EQ(colors[0], 3u);
    CHECK_EQ(colors[1], 2u);
    CHECK_EQ(colors[2], 7u);
    CHECK_EQ(colors[3], 6u);
    CHECK_EQ(VfxColorScan(&surface.Pane, nullptr), 9);
}

TEST_CASE("vfx window_fade: the used colours reach the target palette")
{
    TestSurface surface(4, 2, 1);
    surface.Pixels[3] = 2;
    MCVfxRgb target[256] = {};

    for (int i = 0; i < 256; ++i)
    {
        VfxDacPalette[i] = MCVfxRgb{10, 20, 30};
        target[i] = MCVfxRgb{0, 0, 0};
    }

    target[1] = MCVfxRgb{63, 0, 30};
    target[2] = MCVfxRgb{0, 40, 5};
    static int waits;
    waits = 0;
    VfxWaitRetraceHook = [] { ++waits; };
    VfxWindowFade(&surface.Window, target, 20);
    VfxWaitRetraceHook = nullptr;
    CHECK_EQ(VfxDacPalette[1].R, 63);
    CHECK_EQ(VfxDacPalette[1].G, 0);
    CHECK_EQ(VfxDacPalette[1].B, 30);
    CHECK_EQ(VfxDacPalette[2].R, 0);
    CHECK_EQ(VfxDacPalette[2].G, 40);
    CHECK_EQ(VfxDacPalette[2].B, 5);
    CHECK_EQ(VfxDacPalette[3].R, 10); // not in the window: untouched
    CHECK(waits >= 19 && waits <= 21);
}

TEST_CASE("game: palettex.gif decodes to the reference pixels")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    MCTestGame::OpenFastFiles();
    std::vector<uint8_t> gif = ReadGameFile("data\\palette\\palettex.gif");
    REQUIRE(!gif.empty());
    CHECK_EQ(VfxGifResolution(gif.data()), (640 << 16) | 400);

    TestSurface surface(640, 400, 0);
    std::vector<uint8_t> work(VFX_GIF_BUFFER_SIZE);
    CHECK_EQ(VfxGifDraw(&surface.Pane, gif.data(), work.data()), 16);
    // FNV-1a of the pixels as an independent decoder (PIL) gives them.
    uint32_t hash = 0x811c9dc5;

    for (uint8_t p : surface.Pixels)
    {
        hash = (hash ^ p) * 0x01000193;
    }

    CHECK_EQ(hash, 0xa27515bfu);
    CHECK_EQ(VfxColorScan(&surface.Pane, nullptr), 155);

    MCVfxRgb palette[256];
    VfxGifPalette(gif.data(), palette);
    CHECK_EQ(palette[0].R, 63);
    CHECK_EQ(palette[0].G, 0);
    CHECK_EQ(palette[0].B, 56);
}

TEST_CASE("game: the fonts decode and draw")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    MCTestGame::OpenFastFiles();

    for (const char* name : {"data\\fonts\\WHITE12.FNT", "data\\fonts\\DIM10.FNT", "data\\fonts\\LBLACK.FNT"})
    {
        MCTest::Scope scope(name);
        std::vector<uint8_t> font = ReadGameFile(name);
        REQUIRE(font.size() > 0x10 + 4);
        int32_t count;
        std::memcpy(&count, font.data() + 4, 4);
        const int32_t height = VfxFontHeight(font.data());
        CHECK(height > 4 && height < 32);
        // char_count can exceed 256 (WHITE12.FNT has 280 entries, LBLACK.FNT 770); strings index only the first 256.
        REQUIRE(count > 0 && 0x10 + static_cast<size_t>(count) * 4 <= font.size());

        // Every glyph lies inside the file.
        for (int32_t c = 0; c < count; ++c)
        {
            int32_t offset;
            std::memcpy(&offset, font.data() + 0x10 + c * 4, 4);
            REQUIRE(offset >= 0x10 + count * 4 && static_cast<size_t>(offset) + 4 <= font.size());
            const int32_t width = VfxCharacterWidth(font.data(), c);
            CHECK(width >= 0 && width < 64);
            CHECK(static_cast<size_t>(offset) + 4 + static_cast<size_t>(width) * height <= font.size());
        }

        CHECK(VfxCharacterWidth(font.data(), 'M') > 0);

        // Drawn through an identity table with the background made transparent, the text stays in its box.
        int32_t background;
        std::memcpy(&background, font.data() + 0xc, 4);
        uint8_t table[256];

        for (int i = 0; i < 256; ++i)
        {
            table[i] = static_cast<uint8_t>(i);
        }

        table[static_cast<uint8_t>(background)] = 255;
        TestSurface surface(200, 40, 0xfe);
        VfxStringDraw(&surface.Pane, 5, 5, font.data(), "MechCommander", table);
        int32_t drawn = 0;

        for (int y = 0; y < 40; ++y)
        {
            for (int x = 0; x < 200; ++x)
            {
                if (surface.At(x, y) == 0xfe)
                {
                    continue;
                }

                ++drawn;
                CHECK(y >= 5 && y < 5 + height && x >= 5);
            }
        }

        CHECK(drawn > 20);
    }
}
