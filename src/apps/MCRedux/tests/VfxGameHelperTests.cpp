#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "lib/file.h"
#include "vfx/vfxfuncs.h"

// Tests of the game's own VFX helpers: alphapalette.cpp, encode_vfx.cpp, fastshp.cpp, vfx_ellipse.cpp and
// vfx_map_polygon.cpp.

namespace
{
    /// <summary>A window with its pixels, and a pane over all of it.</summary>
    struct TestWindow
    {
        std::vector<uint8_t> Pixels;
        WINDOW Window{};
        PANE Pane{};

        TestWindow(int32_t width, int32_t height, uint8_t fill = 0) : Pixels(static_cast<size_t>(width * height), fill)
        {
            Window.buffer = Pixels.data();
            Window.x_max = width - 1;
            Window.y_max = height - 1;
            Pane = PANE{&Window, 0, 0, width - 1, height - 1};
        }

        uint8_t& At(int32_t x, int32_t y) { return Pixels[static_cast<size_t>(y * (Window.x_max + 1) + x)]; }
    };

    void Put32(std::vector<uint8_t>& data, size_t at, int32_t value)
    {
        std::memcpy(data.data() + at, &value, 4);
    }

    void Put16(std::vector<uint8_t>& data, size_t at, uint16_t value)
    {
        std::memcpy(data.data() + at, &value, 2);
    }

    /// <summary>Saves and restores the alpha tables, so tests that fill them don't leak into others.</summary>
    struct AlphaGuard
    {
        std::vector<char> Table{AlphaTable, AlphaTable + sizeof(AlphaTable)};
        std::vector<char> Special{SpecialColor, SpecialColor + sizeof(SpecialColor)};
        ~AlphaGuard()
        {
            std::memcpy(AlphaTable, Table.data(), Table.size());
            std::memcpy(SpecialColor, Special.data(), Special.size());
        }
    };
}

TEST_CASE("vfx: VFX_shape_scan encodes a pane that VFX_shape_draw reproduces")
{
    TestWindow source(12, 7);
    // Transparent border and holes, a run of 5, literals, and a row of transparent only.
    const char* rows[] = {
        "............", "..AAAAA.....", "..ABCDB..C..", "............", "...ZZ.ZZZ...", "..Q.........", "............",
    };

    for (int32_t y = 0; y < 7; ++y)
    {
        for (int32_t x = 0; x < 12; ++x)
        {
            source.At(x, y) = rows[y][x] == '.' ? 0 : static_cast<uint8_t>(rows[y][x]);
        }
    }

    const int32_t size = VFX_shape_scan(&source.Pane, 0, 4, 3, nullptr);
    REQUIRE(size > 0x18);
    std::vector<uint8_t> shape(static_cast<size_t>(size) + 16, 0xcc);
    CHECK_EQ(VFX_shape_scan(&source.Pane, 0, 4, 3, shape.data()), size);

    // The game's copy matches the asm encoder byte for byte.
    std::vector<uint8_t> shapeAsm(shape.size(), 0xcc);
    CHECK_EQ(VFX_shape_scan_asm(&source.Pane, 0, 4, 3, shapeAsm.data()), size);
    CHECK(shape == shapeAsm);

    // Header: bounds, origin, and the box of the opaque pixels relative to the hot spot.
    int32_t header[6];
    std::memcpy(header, shape.data(), sizeof(header));
    CHECK_EQ(header[0], (11 << 16) | 6);
    CHECK_EQ(header[1], (4 << 16) | 3);
    CHECK_EQ(header[2], 2 - 4);
    CHECK_EQ(header[3], 1 - 3);
    CHECK_EQ(header[4], 9 - 4);
    CHECK_EQ(header[5], 5 - 3);

    // Wrap it in a one-shape table and draw it back.
    std::vector<uint8_t> table(16 + static_cast<size_t>(size));
    std::memcpy(table.data(), "1.10", 4);
    Put32(table, 4, 1);
    Put32(table, 8, 16);
    Put32(table, 12, 0);
    std::memcpy(table.data() + 16, shape.data(), static_cast<size_t>(size));

    TestWindow target(12, 7, 0);
    CHECK_EQ(VFX_shape_draw(&target.Pane, table.data(), 0, 4, 3), 0);
    CHECK(target.Pixels == source.Pixels);
}

TEST_CASE("vfx: FindClosest searches only indices 10..245 and stops at an exact match")
{
    VFX_RGB palette[256] = {};

    for (int i = 0; i < 256; ++i)
    {
        palette[i] = VFX_RGB{63, 63, 63};
    }

    palette[3] = VFX_RGB{10, 20, 30}; // a system colour: never chosen
    palette[40] = VFX_RGB{10, 20, 30};
    palette[41] = VFX_RGB{10, 20, 30}; // same colour later: the first wins
    palette[100] = VFX_RGB{12, 20, 30};
    CHECK_EQ(FindClosest(palette, 10, 20, 30), static_cast<uint8_t>(40));
    CHECK_EQ(FindClosest(palette, 13, 20, 30), static_cast<uint8_t>(100));
    CHECK_EQ(FindClosest(palette, 63, 63, 63), static_cast<uint8_t>(10));
}

TEST_CASE("vfx: AG_ellipse_draw is symmetric and touches its extremes")
{
    TestWindow w(41, 31);
    AG_ellipse_draw(&w.Pane, 20, 15, 12, 8, 7);
    CHECK_EQ(w.At(32, 15), 7);
    CHECK_EQ(w.At(8, 15), 7);
    CHECK_EQ(w.At(20, 7), 7);
    CHECK_EQ(w.At(20, 23), 7);
    CHECK_EQ(w.At(20, 15), 0);
    int drawn = 0;

    for (int32_t y = 0; y < 31; ++y)
    {
        for (int32_t x = 0; x < 41; ++x)
        {
            const uint8_t p = w.At(x, y);

            if (p == 0)
            {
                continue;
            }

            ++drawn;
            CHECK(x >= 8 && x <= 32 && y >= 7 && y <= 23);
            CHECK_EQ(w.At(40 - x, y), p);
            CHECK_EQ(w.At(x, 30 - y), p);
        }
    }

    CHECK(drawn > 40);
}

TEST_CASE("vfx: AG_ellipse_fill fills rows symmetrically, clipped to the pane")
{
    TestWindow w(41, 31);
    AG_ellipse_fill(&w.Pane, 20, 15, 12, 8, 9);

    for (int32_t x = 8; x <= 32; ++x)
    {
        CHECK_EQ(w.At(x, 15), 9);
    }

    CHECK_EQ(w.At(7, 15), 0);
    CHECK_EQ(w.At(33, 15), 0);

    for (int32_t y = 0; y < 31; ++y)
    {
        for (int32_t x = 0; x < 41; ++x)
        {
            CHECK_EQ(w.At(x, y), w.At(40 - x, 30 - y));
        }
    }

    // A pane on part of the window: nothing outside it (the centre is relative to its origin).
    TestWindow clipped(41, 31);
    clipped.Pane = PANE{&clipped.Window, 5, 5, 20, 20};
    AG_ellipse_fill(&clipped.Pane, 10, 10, 12, 8, 9);

    for (int32_t y = 0; y < 31; ++y)
    {
        for (int32_t x = 0; x < 41; ++x)
        {
            if (x < 5 || x > 20 || y < 5 || y > 20)
            {
                CHECK_EQ(clipped.At(x, y), 0);
            }
        }
    }

    CHECK_EQ(clipped.At(15, 15), 9);

    // A zero radius draws a line.
    TestWindow line(20, 20);
    AG_ellipse_draw(&line.Pane, 10, 10, 0, 3, 5);

    for (int32_t y = 7; y <= 13; ++y)
    {
        CHECK_EQ(line.At(10, y), 5);
    }
}

TEST_CASE("vfx: AG_ellipse_draw blends a special colour through AlphaTable")
{
    AlphaGuard guard;
    SpecialColor[0x40] = 1;

    for (int b = 0; b < 256; ++b)
    {
        AlphaTable[0x40 * 256 + b] = static_cast<char>(b + 1);
    }

    TestWindow w(41, 31, 50);
    AG_ellipse_draw(&w.Pane, 20, 15, 12, 8, 0x40);
    // Points on the axes are plotted twice (as in the original), so they blend twice.
    CHECK_EQ(w.At(32, 15), 52);
    CHECK_EQ(w.At(20, 23), 52);
    CHECK_EQ(w.At(20, 15), 50);
}

TEST_CASE("vfx: AG_StatusBar darkens the frame and blends the bar")
{
    AlphaGuard guard;

    for (int b = 0; b < 256; ++b)
    {
        AlphaTable[0x108 * 256 + b] = static_cast<char>(b + 1);
        AlphaTable[0x110 * 256 + b] = static_cast<char>(b + 100);
    }

    TestWindow w(30, 12, 10);
    AG_StatusBar(&w.Pane, 5, 2, 20, 8, 0x110, 6);
    // Top and bottom rows: between the corners only.
    CHECK_EQ(w.At(5, 2), 10);

    for (int32_t x = 6; x < 20; ++x)
    {
        CHECK_EQ(w.At(x, 2), 11);
        CHECK_EQ(w.At(x, 8), 11);
    }

    CHECK_EQ(w.At(20, 2), 10);

    for (int32_t y = 3; y < 8; ++y)
    {
        CHECK_EQ(w.At(5, y), 11);
        CHECK_EQ(w.At(20, y), 11);

        for (int32_t x = 6; x <= 12; ++x)
        {
            CHECK_EQ(w.At(x, y), 110);
        }

        CHECK_EQ(w.At(13, y), 10);
    }

    CHECK_EQ(w.At(5, 9), 10);
}

TEST_CASE("vfx: AG_pixel_write writes only strictly inside the pane")
{
    TestWindow w(10, 10);
    w.Pane = PANE{&w.Window, 2, 2, 7, 7};
    AG_pixel_write(&w.Pane, 0, 0, 5);
    AG_pixel_write(&w.Pane, 1, 1, 6);
    AG_pixel_write(&w.Pane, 5, 3, 0x107);
    AG_pixel_write(&w.Pane, 4, 4, 8);
    CHECK_EQ(w.At(2, 2), 0);
    CHECK_EQ(w.At(3, 3), 6);
    CHECK_EQ(w.At(7, 5), 0);
    CHECK_EQ(w.At(6, 6), 8);
}

TEST_CASE("vfx: DrawTransparent copies all but colour 255, clipped")
{
    TestWindow texture(4, 3);

    for (int32_t y = 0; y < 3; ++y)
    {
        for (int32_t x = 0; x < 4; ++x)
        {
            texture.At(x, y) = static_cast<uint8_t>(x == 1 ? 0xff : 10 * y + x + 1);
        }
    }

    TestWindow w(10, 8, 7);
    CHECK_EQ(DrawTransparent(&w.Pane, &texture.Window, 2, 3, 4, 3), 0);
    CHECK_EQ(w.At(2, 3), 1);
    CHECK_EQ(w.At(3, 3), 7);
    CHECK_EQ(w.At(5, 5), 24);
    CHECK_EQ(w.At(6, 5), 7);
    // Hanging off the left and top.
    TestWindow edge(10, 8, 7);
    CHECK_EQ(DrawTransparent(&edge.Pane, &texture.Window, -2, -1, 4, 3), 0);
    CHECK_EQ(edge.At(0, 0), 13);
    CHECK_EQ(edge.At(1, 1), 24);
    CHECK_EQ(DrawTransparent(&edge.Pane, &texture.Window, 20, 0, 4, 3), 1);
}

namespace
{
    /// <summary>A fast-shape table of one shape: hot spot (0, 0), rows given as packet bytes.</summary>
    std::vector<uint8_t> MakeFastShape(uint16_t widthField, const std::vector<std::vector<uint8_t>>& rows)
    {
        const size_t shapeAt = 12;
        const size_t rowTable = shapeAt + 0xc;
        size_t dataAt = rowTable + rows.size() * 2;
        std::vector<uint8_t> t(dataAt);
        std::memcpy(t.data(), "DNAH", 4);
        Put32(t, 8, static_cast<int32_t>(shapeAt));
        Put16(t, shapeAt + 4, 0);
        Put16(t, shapeAt + 6, 0);
        Put16(t, shapeAt + 8, static_cast<uint16_t>(rows.size()));
        Put16(t, shapeAt + 10, widthField);

        for (size_t r = 0; r < rows.size(); ++r)
        {
            Put16(t, rowTable + r * 2, static_cast<uint16_t>(t.size() - shapeAt));
            t.insert(t.end(), rows[r].begin(), rows[r].end());
        }

        return t;
    }
}

TEST_CASE("vfx: fastShapeDraw decodes runs, literals and transparency")
{
    // Width field 5: rows of 6 pixels.
    auto table = MakeFastShape(5, {
                                      {0x82, 1, 2, 2, 0xff, 2, 3}, // literal 1 2, skip 2, run of 2 x 3
                                      {0x82, 4, 5, 2, 0xff, 2, 6}, // (drawn twice: see below)
                                      {0x86, 7, 8, 9, 10, 11, 12},
                                  });
    TestWindow w(12, 8, 0x55);
    CHECK_EQ(fastShapeDraw(&w.Pane, table.data(), 0, 3, 2, nullptr, 0), 0);
    const uint8_t row0[] = {1, 2, 0x55, 0x55, 3, 3};

    for (int32_t i = 0; i < 6; ++i)
    {
        CHECK_EQ(w.At(3 + i, 2), row0[i]);
    }

    // Original behaviour: the second row reuses the first row's offset, and the third draws the second's data.
    for (int32_t i = 0; i < 6; ++i)
    {
        CHECK_EQ(w.At(3 + i, 3), row0[i]);
    }

    const uint8_t row2[] = {4, 5, 0x55, 0x55, 6, 6};

    for (int32_t i = 0; i < 6; ++i)
    {
        CHECK_EQ(w.At(3 + i, 4), row2[i]);
    }

    CHECK_EQ(w.At(2, 2), 0x55);
    CHECK_EQ(w.At(9, 2), 0x55);
    CHECK_EQ(w.At(3, 5), 0x55);

    // Through a table: 255 after translation is transparent.
    uint8_t xlat[256];

    for (int i = 0; i < 256; ++i)
    {
        xlat[i] = static_cast<uint8_t>(i + 100);
    }

    xlat[2] = 0xff;
    TestWindow t(12, 8, 0x55);
    fastShapeDraw(&t.Pane, table.data(), 0, 3, 2, xlat, 0);
    CHECK_EQ(t.At(3, 2), 101);
    CHECK_EQ(t.At(4, 2), 0x55);
    CHECK_EQ(t.At(7, 2), 103);

    // Clipped on the right: the rows stop at the pane's edge.
    TestWindow c(12, 8, 0x55);
    c.Pane.x1 = 5;
    fastShapeDraw(&c.Pane, table.data(), 0, 3, 2, nullptr, 0);
    CHECK_EQ(c.At(3, 2), 1);
    CHECK_EQ(c.At(4, 2), 2);
    CHECK_EQ(c.At(6, 2), 0x55);
    CHECK_EQ(c.At(6, 4), 0x55);

    // Clipped on the left: the packet crossing the edge starts at the pane's left.
    TestWindow l(12, 8, 0x55);
    l.Pane.x0 = 2;
    fastShapeDraw(&l.Pane, table.data(), 0, -1, 0, nullptr, 0);
    // sx = 2 + -1 = 1: pixels 1..6; 1 is clipped. Rows land at y = 0..2, y = 2 drawing the second row's data.
    const uint8_t clipped0[] = {0x55, 2, 0x55, 0x55, 3, 3, 0x55};
    const uint8_t clipped2[] = {0x55, 5, 0x55, 0x55, 6, 6, 0x55};

    for (int32_t i = 0; i < 7; ++i)
    {
        CHECK_EQ(l.At(1 + i, 0), clipped0[i]);
        CHECK_EQ(l.At(1 + i, 1), clipped0[i]);
        CHECK_EQ(l.At(1 + i, 2), clipped2[i]);
    }

    CHECK_EQ(l.At(2, 3), 0x55);
}

TEST_CASE("game: InitAlphaLookup builds the tables from AlphaPal.ini")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    MCTestGame::OpenFastFiles();
    AlphaGuard guard;

    File paletteFile;
    REQUIRE_EQ(paletteFile.open("data\\palette\\HB.PAL"), 0);
    std::vector<uint8_t> pal(paletteFile.getLength());
    paletteFile.read(pal.data(), static_cast<int32_t>(pal.size()));
    REQUIRE(pal.size() >= 4 + 768);
    VFX_RGB palette[256];
    std::memcpy(palette, pal.data() + 4, 768);

    InitAlphaLookup(palette);
    int special = 0;

    for (int c = 0; c < ALPHA_COLORS; ++c)
    {
        special += SpecialColor[c] == 1;
    }

    CHECK(special > 0);
    const uint8_t* table = reinterpret_cast<const uint8_t*>(AlphaTable);

    for (int b = 0; b < 256; ++b)
    {
        CHECK_EQ(table[b], static_cast<uint8_t>(b));
        CHECK_EQ(table[0xff * 256 + b], static_cast<uint8_t>(b));
    }

    for (int c = 1; c < ALPHA_COLORS; ++c)
    {
        if (c == 0xff)
        {
            continue;
        }

        MCTest::Scope scope(std::format("alpha colour {}", c));

        for (int b = 0; b < 256; ++b)
        {
            const uint8_t v = table[c * 256 + b];

            if (!SpecialColor[c])
            {
                CHECK_EQ(v, static_cast<uint8_t>(c));
            }
            else if (b < 10 || b > 0xf5)
            {
                CHECK_EQ(v, 0xff);
            }
            else
            {
                CHECK(v >= 10 && v <= 0xf5);
            }
        }
    }
}
