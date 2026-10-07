#include "stdafx.h"
#include "MCTest.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>A window with its own pixels and a pane over all of it.</summary>
    struct TestSurface
    {
        std::vector<uint8_t> Pixels;
        MCWindow Window{};
        MCPane Pane{};

        TestSurface(int32_t width, int32_t height, uint8_t fill) : Pixels(static_cast<size_t>(width) * height, fill)
        {
            Window.Buffer = Pixels.data();
            Window.XMax = width - 1;
            Window.YMax = height - 1;
            Pane = {&Window, 0, 0, width - 1, height - 1};
        }

        uint8_t At(int32_t x, int32_t y) const { return Pixels[static_cast<size_t>(y) * (Window.XMax + 1) + x]; }
    };

    void Put32(std::vector<uint8_t>& data, int32_t value)
    {
        for (int i = 0; i < 4; ++i)
        {
            data.push_back(static_cast<uint8_t>(static_cast<uint32_t>(value) >> (8 * i)));
        }
    }

    /// <summary>
    /// A one-shape table: 3 x 2 pixels, rows {1, 2, 3} and {4, 5, 6} as literals, the hot spot at its top left
    /// plus (xMin, yMin).
    /// </summary>
    std::vector<uint8_t> MakeShapeTable(int32_t xMin, int32_t yMin)
    {
        std::vector<uint8_t> table;
        table.insert(table.end(), {'1', '.', '1', '0'});
        Put32(table, 1);             // count
        Put32(table, 16);            // shape 0's offset
        Put32(table, 0);             // no palette
        Put32(table, (3 << 16) | 2); // bounds
        Put32(table, 0);             // origin
        Put32(table, xMin);
        Put32(table, yMin);
        Put32(table, xMin + 2);
        Put32(table, yMin + 1);
        table.insert(table.end(), {7, 1, 2, 3, 0, 7, 4, 5, 6, 0});
        return table;
    }
}

TEST_CASE("vfx: VFX_Cos_Sin reads the quarter-wave table")
{
    MCFixed16 c = 0;
    MCFixed16 s = 0;
    VfxCosSin(0, &c, &s);
    CHECK_EQ(c, 0x10000);
    CHECK_EQ(s, 0);
    VfxCosSin(900, &c, &s);
    CHECK_EQ(c, 0);
    CHECK_EQ(s, 0x10000);
    VfxCosSin(1800, &c, &s);
    CHECK_EQ(c, -0x10000);
    CHECK_EQ(s, 0);
    VfxCosSin(2700, &c, &s);
    CHECK_EQ(c, 0);
    CHECK_EQ(s, -0x10000);
    VfxCosSin(450, &c, &s);
    CHECK_EQ(c, 46341); // round(cos 45 * 65536)
    CHECK_EQ(s, 46341);
    VfxCosSin(300, &c, &s);
    CHECK_EQ(c, 56756);
    CHECK_EQ(s, 32768);
    // Angles wrap either way.
    VfxCosSin(-900, &c, &s);
    CHECK_EQ(c, 0);
    CHECK_EQ(s, -0x10000);
    VfxCosSin(3600 + 1350, &c, &s);
    CHECK_EQ(c, -46341);
    CHECK_EQ(s, 46341);

    // Every angle agrees with the maths to the table's rounding.
    for (int32_t angle = -3600; angle <= 7200; angle += 7)
    {
        VfxCosSin(angle, &c, &s);
        const double radians = angle / 10.0 * std::numbers::pi / 180.0;
        CHECK(std::abs(c - std::cos(radians) * 65536.0) <= 1.0);
        CHECK(std::abs(s - std::sin(radians) * 65536.0) <= 1.0);
    }
}

TEST_CASE("vfx: VFX_fixed_mul rounds the 16.16 product")
{
    MCFixed16 result = 0;
    CHECK_EQ(VfxFixedMul(0x20000, 0x30000, &result), 0x60000);
    CHECK_EQ(result, 0x60000);
    CHECK_EQ(VfxFixedMul(0x18000, 0x18000, &result), 0x24000); // 1.5 * 1.5
    CHECK_EQ(VfxFixedMul(-0x20000, 0x8000, &result), -0x10000);
    CHECK_EQ(VfxFixedMul(1, 0x8000, &result), 1); // 0.5 ulp rounds up
    CHECK_EQ(VfxFixedMul(1, 0x7fff, &result), 0);
    CHECK_EQ(VfxFixedMul(-1, 0x8000, &result), 0); // -0.5 ulp rounds towards +infinity
}

TEST_CASE("vfx: VFX_point_transform rotates and scales about the origin")
{
    MCVfxPoint origin{10, 20};
    MCVfxPoint in{15, 20};
    MCVfxPoint out{};
    VfxPointTransform(&in, &out, &origin, 0, 0x10000, 0x10000);
    CHECK_EQ(out.X, 15);
    CHECK_EQ(out.Y, 20);
    // 90 degrees turns +x into +y (clockwise with y down).
    VfxPointTransform(&in, &out, &origin, 900, 0x10000, 0x10000);
    CHECK_EQ(out.X, 10);
    CHECK_EQ(out.Y, 25);
    VfxPointTransform(&in, &out, &origin, 1800, 0x10000, 0x10000);
    CHECK_EQ(out.X, 5);
    CHECK_EQ(out.Y, 20);
    // Scaling happens before rotating.
    VfxPointTransform(&in, &out, &origin, 0, 0x20000, 0x10000);
    CHECK_EQ(out.X, 20);
    CHECK_EQ(out.Y, 20);
    MCVfxPoint below{10, 24};
    VfxPointTransform(&below, &out, &origin, 900, 0x10000, 0x8000);
    CHECK_EQ(out.X, 8);
    CHECK_EQ(out.Y, 20);
    // 45 degrees of (10, 0): 7.07 each way.
    MCVfxPoint far{20, 20};
    VfxPointTransform(&far, &out, &origin, 450, 0x10000, 0x10000);
    CHECK_EQ(out.X, 17);
    CHECK_EQ(out.Y, 27);
}

TEST_CASE("vfx: VFX_shape_transform untransformed matches VFX_shape_draw")
{
    std::vector<uint8_t> table = MakeShapeTable(-1, -1);
    std::vector<uint8_t> work(3 * 2);

    TestSurface plain(16, 12, 9);
    TestSurface transformed(16, 12, 9);
    TestSurface mapped(16, 12, 9);
    VfxShapeDraw(&plain.Pane, table.data(), 0, 6, 5);
    CHECK_EQ(VfxShapeTransform(&transformed.Pane, table.data(), 0, 6, 5, work.data(), 0, 0x10000, 0x10000, 0), 0);
    CHECK(plain.Pixels == transformed.Pixels);

    // A full turn goes through the texture mapper and must land on the same pixels.
    VfxShapeTransform(&mapped.Pane, table.data(), 0, 6, 5, work.data(), 3600, 0x10000, 0x10000, 0);
    CHECK(plain.Pixels == mapped.Pixels);
    CHECK_EQ(plain.At(5, 4), 1);
    CHECK_EQ(plain.At(7, 5), 6);

    // The work buffer holds the upright shape.
    const std::vector<uint8_t> upright{1, 2, 3, 4, 5, 6};
    CHECK(work == upright);
}

TEST_CASE("vfx: VFX_shape_transform rotates a quarter turn")
{
    std::vector<uint8_t> table = MakeShapeTable(0, 0);
    std::vector<uint8_t> work(3 * 2);
    TestSurface surface(16, 12, 9);
    VfxShapeTransform(&surface.Pane, table.data(), 0, 5, 5, work.data(), 900, 0x10000, 0x10000, 0);
    // Texel (u, v) lands at (5 - v, 5 + u).
    CHECK_EQ(surface.At(5, 5), 1);
    CHECK_EQ(surface.At(5, 6), 2);
    CHECK_EQ(surface.At(5, 7), 3);
    CHECK_EQ(surface.At(4, 5), 4);
    CHECK_EQ(surface.At(4, 6), 5);
    CHECK_EQ(surface.At(4, 7), 6);
    int32_t touched = 0;

    for (uint8_t pixel : surface.Pixels)
    {
        touched += pixel != 9;
    }

    CHECK_EQ(touched, 6);

    // Transparent (skipped) pixels of the shape leave the target alone; VfxShapeTransformReuse keeps the buffer's contents.
    std::fill(work.begin(), work.end(), uint8_t{0xff});
    work[0] = 42;
    TestSurface reuse(16, 12, 9);
    VfxShapeTransform(&reuse.Pane, table.data(), 0, 5, 5, work.data(), 900, 0x10000, 0x10000, VfxShapeTransformReuse);
    CHECK_EQ(reuse.At(5, 5), 42);
    touched = 0;

    for (uint8_t pixel : reuse.Pixels)
    {
        touched += pixel != 9;
    }

    CHECK_EQ(touched, 1);
}

TEST_CASE("vfx: VFX_shape_transform scales and clips to the pane")
{
    std::vector<uint8_t> table = MakeShapeTable(0, 0);
    std::vector<uint8_t> work(3 * 2);
    TestSurface surface(16, 12, 9);
    // Twice the size, into a pane that cuts off the right part.
    MCPane pane{&surface.Window, 2, 2, 5, 11};
    VfxShapeTransform(&pane, table.data(), 0, 0, 0, work.data(), 3600, 0x20000, 0x20000, 0);

    for (int32_t y = 0; y < 12; ++y)
    {
        for (int32_t x = 0; x < 16; ++x)
        {
            MCTest::Scope scope(std::format("({}, {})", x, y));

            if (x < 2 || x > 5 || y < 2 || y > 11)
            {
                CHECK_EQ(surface.At(x, y), 9);
            }
        }
    }

    CHECK_EQ(surface.At(2, 2), 1);
    CHECK(surface.At(5, 2) != 9);
    CHECK(surface.At(2, 4) == 4 || surface.At(2, 5) == 4);
}
