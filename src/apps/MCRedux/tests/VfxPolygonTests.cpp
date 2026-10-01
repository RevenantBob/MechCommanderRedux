#include "stdafx.h"
#include "MCTest.h"
#include "vfx/vfxfuncs.h"

// The vfx3d polygon fillers on synthetic windows. The expected pixels follow from the asm's rules (edges at x + 0.5
// rounded down, rows and spans inclusive of both ends); the port was also checked against MCX.EXE's own routines run
// in an emulator on thousands of random polygons.

namespace
{
    /// <summary>A small window and a pane over it, filled with one colour.</summary>
    struct TestSurface
    {
        std::vector<uint8_t> Pixels;
        WINDOW Window{};
        PANE Pane{};

        TestSurface(int32_t width, int32_t height, uint8_t fill) : Pixels(static_cast<size_t>(width) * height, fill)
        {
            Window = {Pixels.data(), width - 1, height - 1};
            Pane = {&Window, 0, 0, width - 1, height - 1};
        }

        uint8_t At(int32_t x, int32_t y) const { return Pixels[static_cast<size_t>(y) * (Window.x_max + 1) + x]; }
    };

    SCRNVERTEX Vertex(int32_t x, int32_t y, int32_t c = 0, int32_t u = 0, int32_t v = 0)
    {
        return SCRNVERTEX{x, y, c, u, v, 0};
    }
}

TEST_CASE("vfx3d: a flat square fills its corners inclusive")
{
    TestSurface surface(16, 16, 0);
    SCRNVERTEX quad[4] = {Vertex(2, 2, 7 << 16), Vertex(6, 2), Vertex(6, 6), Vertex(2, 6)};
    VFX_flat_polygon(&surface.Pane, 4, quad);
    int covered = 0;

    for (int32_t y = 0; y < 16; ++y)
    {
        for (int32_t x = 0; x < 16; ++x)
        {
            const bool inside = x >= 2 && x <= 6 && y >= 2 && y <= 6;
            CHECK_EQ(surface.At(x, y), inside ? 7 : 0);
            covered += surface.At(x, y) != 0;
        }
    }

    CHECK_EQ(covered, 25);
}

TEST_CASE("vfx3d: flat triangle rows and the colour rounding")
{
    TestSurface surface(16, 16, 0);
    // Colour 4.5 rounds to 5. A right triangle: row y spans x = 0..y.
    SCRNVERTEX tri[3] = {Vertex(0, 0, (4 << 16) | 0x8000), Vertex(8, 8), Vertex(0, 8)};
    VFX_flat_polygon(&surface.Pane, 3, tri);

    for (int32_t y = 0; y < 16; ++y)
    {
        for (int32_t x = 0; x < 16; ++x)
        {
            CHECK_EQ(surface.At(x, y), (y <= 8 && x <= y) ? 5 : 0);
        }
    }
}

TEST_CASE("vfx3d: polygons are clipped to the pane, relative to its corner")
{
    TestSurface surface(16, 16, 1);
    surface.Pane = {&surface.Window, 4, 4, 9, 9};
    // Much larger than the pane: exactly the pane's pixels change.
    SCRNVERTEX quad[4] = {Vertex(-7, -7, 9 << 16), Vertex(12, -7), Vertex(12, 12), Vertex(-7, 12)};
    VFX_flat_polygon(&surface.Pane, 4, quad);

    for (int32_t y = 0; y < 16; ++y)
    {
        for (int32_t x = 0; x < 16; ++x)
        {
            const bool inside = x >= 4 && x <= 9 && y >= 4 && y <= 9;
            CHECK_EQ(surface.At(x, y), inside ? 9 : 1);
        }
    }

    // Wholly outside: nothing is drawn.
    TestSurface other(16, 16, 1);
    SCRNVERTEX away[3] = {Vertex(20, 0, 9 << 16), Vertex(30, 0), Vertex(25, 5)};
    VFX_flat_polygon(&other.Pane, 3, away);
    CHECK(std::all_of(other.Pixels.begin(), other.Pixels.end(), [](uint8_t p) { return p == 1; }));
}

TEST_CASE("vfx3d: Gouraud interpolates the colour along a span")
{
    TestSurface surface(16, 8, 0);
    SCRNVERTEX quad[4] = {Vertex(0, 0, 0), Vertex(10, 0, 10 << 16), Vertex(10, 3, 10 << 16), Vertex(0, 3, 0)};
    VFX_Gouraud_polygon(&surface.Pane, 4, quad);

    for (int32_t y = 0; y <= 3; ++y)
    {
        for (int32_t x = 0; x <= 10; ++x)
        {
            CHECK_EQ(surface.At(x, y), x);
        }
    }

    CHECK_EQ(surface.At(11, 0), 0);
}

TEST_CASE("vfx3d: dithered Gouraud adds the dither in a checkerboard")
{
    TestSurface surface(16, 8, 0);
    // Colour 5.0 (+0.5 at the vertex): with a dither of 0.5 alternate pixels round to 6, the others to 5.
    SCRNVERTEX quad[4] = {Vertex(0, 0, 5 << 16), Vertex(7, 0, 5 << 16), Vertex(7, 3, 5 << 16), Vertex(0, 3, 5 << 16)};
    VFX_dithered_Gouraud_polygon(&surface.Pane, 0x8000, 4, quad);

    for (int32_t y = 0; y <= 3; ++y)
    {
        for (int32_t x = 0; x <= 7; ++x)
        {
            CHECK_EQ(surface.At(x, y), ((x + y) & 1) == 0 ? 6 : 5);
        }
    }
}

TEST_CASE("vfx3d: illuminate adds, carrying within pixel pairs")
{
    TestSurface surface(16, 4, 0xf0);
    SCRNVERTEX quad[4] = {Vertex(0, 0, 0x20 << 16), Vertex(3, 0, 0x20 << 16), Vertex(3, 1, 0x20 << 16),
                          Vertex(0, 1, 0x20 << 16)};
    VFX_illuminate_polygon(&surface.Pane, 0, 4, quad);

    // 0xf0f0 + 0x2020 = 0x1110 per pair: the low pixel's overflow carries into the high one (as the asm's word adds).
    for (int32_t y = 0; y <= 1; ++y)
    {
        CHECK_EQ(surface.At(0, y), 0x10);
        CHECK_EQ(surface.At(1, y), 0x11);
        CHECK_EQ(surface.At(2, y), 0x10);
        CHECK_EQ(surface.At(3, y), 0x11);
        CHECK_EQ(surface.At(4, y), 0xf0);
    }
}

TEST_CASE("vfx3d: translate polygon maps the pixels under it")
{
    TestSurface surface(16, 16, 0);

    for (size_t i = 0; i < surface.Pixels.size(); ++i)
    {
        surface.Pixels[i] = static_cast<uint8_t>(i);
    }

    uint8_t table[256];

    for (int i = 0; i < 256; ++i)
    {
        table[i] = static_cast<uint8_t>(255 - i);
    }

    SCRNVERTEX quad[4] = {Vertex(3, 3), Vertex(8, 3), Vertex(8, 5), Vertex(3, 5)};
    VFX_translate_polygon(&surface.Pane, 4, quad, table);

    for (int32_t y = 0; y < 16; ++y)
    {
        for (int32_t x = 0; x < 16; ++x)
        {
            const uint8_t original = static_cast<uint8_t>(y * 16 + x);
            const bool inside = x >= 3 && x <= 8 && y >= 3 && y <= 5;
            CHECK_EQ(surface.At(x, y), inside ? static_cast<uint8_t>(255 - original) : original);
        }
    }
}

TEST_CASE("vfx3d: map polygon copies an axis-aligned texture 1:1, with lookaside and transparency")
{
    std::vector<uint8_t> texels(16 * 16);

    for (size_t i = 0; i < texels.size(); ++i)
    {
        texels[i] = static_cast<uint8_t>(i * 7 + 1);
    }

    texels[2 * 16 + 3] = 0xff; // transparent texel at (3, 2)
    WINDOW texture{texels.data(), 15, 15};
    SCRNVERTEX quad[4] = {Vertex(0, 0, 0, 0, 0), Vertex(7, 0, 0, 7 << 16, 0), Vertex(7, 7, 0, 7 << 16, 7 << 16),
                          Vertex(0, 7, 0, 0, 7 << 16)};

    TestSurface plain(16, 16, 0);
    VFX_map_polygon(&plain.Pane, 4, quad, &texture, 0);

    for (int32_t y = 0; y < 16; ++y)
    {
        for (int32_t x = 0; x < 16; ++x)
        {
            const bool inside = x <= 7 && y <= 7;
            CHECK_EQ(plain.At(x, y), inside ? texels[y * 16 + x] : 0);
        }
    }

    TestSurface transparent(16, 16, 0);
    VFX_map_polygon(&transparent.Pane, 4, quad, &texture, MP_XP);
    CHECK_EQ(transparent.At(3, 2), 0);
    CHECK_EQ(transparent.At(4, 2), texels[2 * 16 + 4]);

    uint8_t table[256];

    for (int i = 0; i < 256; ++i)
    {
        table[i] = static_cast<uint8_t>(i ^ 0x55);
    }

    table[texels[5 * 16 + 5]] = 0xff;
    VFX_map_lookaside(table);
    TestSurface xlat(16, 16, 0);
    VFX_map_polygon(&xlat.Pane, 4, quad, &texture, MP_XLAT | MP_XP);
    CHECK_EQ(xlat.At(1, 1), static_cast<uint8_t>(texels[1 * 16 + 1] ^ 0x55));
    CHECK_EQ(xlat.At(5, 5), 0);                                 // translated to 255: transparent
    CHECK_EQ(xlat.At(3, 2), static_cast<uint8_t>(0xff ^ 0x55)); // the texel is 255, but its translation isn't

    // Mirrored (u decreasing along x): the texture reads backwards.
    SCRNVERTEX mirrored[4] = {Vertex(0, 0, 0, 7 << 16, 0), Vertex(7, 0, 0, 0, 0), Vertex(7, 7, 0, 0, 7 << 16),
                              Vertex(0, 7, 0, 7 << 16, 7 << 16)};
    TestSurface flipped(16, 16, 0);
    VFX_map_polygon(&flipped.Pane, 4, mirrored, &texture, 0);
    CHECK_EQ(flipped.At(0, 0), texels[7]);
    CHECK_EQ(flipped.At(7, 4), texels[4 * 16 + 0]);
}
