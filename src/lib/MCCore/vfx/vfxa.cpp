#include "stdafx.h"
#include "vfx/vfxint.h"

// VFX's primitives (vfxa.asm in MCX.EXE): the display-driver hooks, pixels, lines, hashed rectangles, pane wipes,
// copies and scrolls, and ellipses.

namespace
{
    /// <summary>The driver name buffer VFX_driver_name returns (0x007a8100 in MCX.EXE).</summary>
    char driverName[16] = "SDL";
}

char* VFX_driver_name(void* driver)
{
    // The asm called the driver's first entry to get its name and copied it here; the port has one "driver".
    (void)driver;
    return driverName;
}

void VFX_register_driver(void* describe)
{
    // The asm copied the driver's 13-dword description table into VFX_describe_driver (0x007a80cc); the port has no
    // VFX drivers.
    (void)describe;
}

int32_t VFX_pixel_write(PANE* pane, int32_t x, int32_t y, uint8_t color)
{
    MCVfxClip clip;
    const int32_t status = MCVfxClipPane(pane, clip);

    if (status != 0)
    {
        return status;
    }

    x += clip.PaneX;
    y += clip.PaneY;

    if (x < clip.X0 || x > clip.X1 || y < clip.Y0 || y > clip.Y1)
    {
        return VFX_ERR_CLIPPED;
    }

    uint8_t* pixel = clip.At(x, y);
    const int32_t previous = *pixel;
    *pixel = color;
    return previous;
}

int32_t VFX_pixel_read(PANE* pane, int32_t x, int32_t y)
{
    MCVfxClip clip;
    const int32_t status = MCVfxClipPane(pane, clip);

    if (status != 0)
    {
        return status;
    }

    x += clip.PaneX;
    y += clip.PaneY;

    if (x < clip.X0 || x > clip.X1 || y < clip.Y0 || y > clip.Y1)
    {
        return VFX_ERR_CLIPPED;
    }

    return *clip.At(x, y);
}

namespace
{
    /// <summary>
    /// <c>round(a * slope / 2^32)</c>: the asm's <c>mul</c> then <c>add eax, 0x80000000 / adc edx, 0</c>, the minor-axis
    /// offset at major-axis distance <paramref name="a"/>.
    /// </summary>
    int32_t MinorFromMajor(uint32_t a, uint32_t slope)
    {
        return static_cast<int32_t>((static_cast<uint64_t>(a) * slope + 0x80000000u) >> 32);
    }

    /// <summary>
    /// The major-axis distance at which the line reaches minor-axis distance <paramref name="a"/>, rounded as the asm
    /// does when clipping the line's start: <c>ceil((a - 1/2) * 2^32 / slope)</c>.
    /// </summary>
    int32_t MajorFromMinorStart(uint32_t a, uint32_t slope)
    {
        const uint64_t dividend = (static_cast<uint64_t>(a - 1) << 32) | 0x80000000u;
        // Port fix: the asm's DIV faults when the quotient exceeds 32 bits (never for a line the clip loop keeps).
        const uint64_t quotient = dividend / slope;
        const uint64_t remainder = dividend % slope;
        return static_cast<int32_t>(static_cast<uint32_t>(quotient + (remainder != 0 ? 1 : 0)));
    }

    /// <summary>
    /// The same for the line's end: <c>ceil((a + 1/2) * 2^32 / slope) - 1</c>, the last major-axis step still at or
    /// before minor-axis distance <paramref name="a"/>.
    /// </summary>
    int32_t MajorFromMinorEnd(uint32_t a, uint32_t slope)
    {
        const uint64_t dividend = (static_cast<uint64_t>(a) << 32) | 0x80000000u;
        const uint64_t quotient = dividend / slope;
        const uint64_t remainder = dividend % slope;
        return static_cast<int32_t>(static_cast<uint32_t>(quotient - (remainder == 0 ? 1 : 0)));
    }

    /// <summary><paramref name="value"/> negated when <paramref name="mask"/> is -1, unchanged when it is 0.</summary>
    int32_t Negate(int32_t value, int32_t mask)
    {
        return static_cast<int32_t>((static_cast<uint32_t>(value) ^ static_cast<uint32_t>(mask)) -
                                    static_cast<uint32_t>(mask));
    }

    /// <summary>The outcode of a point: 8 left of, 4 right of, 2 above, 1 below the clip rectangle.</summary>
    uint32_t OutCode(const MCVfxClip& clip, int32_t x, int32_t y)
    {
        return (x < clip.X0 ? 8u : 0u) | (x > clip.X1 ? 4u : 0u) | (y < clip.Y0 ? 2u : 0u) | (y > clip.Y1 ? 1u : 0u);
    }

    /// <summary>Plots one pixel of a line in LD_DRAW or LD_TRANSLATE mode.</summary>
    void Plot(uint8_t* pixel, int32_t mode, intptr_t parm)
    {
        if (mode == LD_TRANSLATE)
        {
            *pixel = reinterpret_cast<const uint8_t*>(parm)[*pixel];
        }
        else
        {
            *pixel = static_cast<uint8_t>(parm);
        }
    }
}

int32_t VFX_line_draw(PANE* pane, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t mode, intptr_t parm)
{
    MCVfxClip clip;
    const int32_t status = MCVfxClipPane(pane, clip);

    if (status != 0)
    {
        return status;
    }

    // Window coordinates: (P, Q) is the start as given, the reference every clipped coordinate is measured from.
    x0 += clip.PaneX;
    x1 += clip.PaneX;
    y0 += clip.PaneY;
    y1 += clip.PaneY;
    const int32_t P = x0;
    const int32_t Q = y0;

    const int32_t dx = x1 - x0;
    const int32_t signX = dx < 0 ? -1 : 0;
    const int32_t adx = dx < 0 ? -dx : dx;
    const int32_t dy = y1 - y0;
    const int32_t signY = dy < 0 ? -1 : 0;
    const int32_t ady = dy < 0 ? -dy : dy;

    int32_t startX = x0;
    int32_t startY = y0;
    int32_t endX = x1;
    int32_t endY = y1;
    int32_t count;
    int32_t step;

    if (dx != 0 && dy != 0)
    {
        const int32_t sameSign = signX ^ signY;
        const int32_t notSame = ~sameSign;
        // The minor axis's advance per major-axis pixel as a 0.32 fraction (all ones for a 45-degree line).
        uint32_t slope = 0xffffffffu;

        if (adx < ady)
        {
            slope = static_cast<uint32_t>((static_cast<uint64_t>(adx) << 32) / static_cast<uint32_t>(ady));
        }
        else if (adx > ady)
        {
            slope = static_cast<uint32_t>((static_cast<uint64_t>(ady) << 32) / static_cast<uint32_t>(adx));
        }

        const bool xMajor = adx >= ady;

        // Clip one end against one edge at a time until both ends are inside or both lie beyond one edge.
        uint32_t everClipped = 0;
        int guard = 0;

        for (;;)
        {
            const uint32_t codeStart = OutCode(clip, startX, startY);
            const uint32_t codeEnd = OutCode(clip, endX, endY);
            everClipped |= codeStart | codeEnd;

            if ((codeStart | codeEnd) == 0)
            {
                break;
            }

            // Port fix: the asm loops for as long as rounding keeps an end outside; give up as clipped instead.
            if ((codeStart & codeEnd) != 0 || ++guard > 16)
            {
                return 2;
            }

            if (xMajor)
            {
                if (codeStart & 8)
                {
                    startX = clip.X0;
                    startY = Q + Negate(MinorFromMajor(static_cast<uint32_t>(clip.X0 - P), slope), sameSign);
                }
                else if (codeStart & 4)
                {
                    startX = clip.X1;
                    startY = Q + Negate(MinorFromMajor(static_cast<uint32_t>(P - clip.X1), slope), notSame);
                }
                else if (codeStart & 2)
                {
                    startY = clip.Y0;
                    startX = P + Negate(MajorFromMinorStart(static_cast<uint32_t>(clip.Y0 - Q), slope), sameSign);
                }
                else if (codeStart & 1)
                {
                    startY = clip.Y1;
                    startX = P + Negate(MajorFromMinorStart(static_cast<uint32_t>(Q - clip.Y1), slope), notSame);
                }
                else if (codeEnd & 8)
                {
                    endX = clip.X0;
                    endY = Q + Negate(MinorFromMajor(static_cast<uint32_t>(P - clip.X0), slope), notSame);
                }
                else if (codeEnd & 4)
                {
                    endX = clip.X1;
                    endY = Q + Negate(MinorFromMajor(static_cast<uint32_t>(clip.X1 - P), slope), sameSign);
                }
                else if (codeEnd & 2)
                {
                    endY = clip.Y0;
                    endX = P + Negate(MajorFromMinorEnd(static_cast<uint32_t>(Q - clip.Y0), slope), notSame);
                }
                else
                {
                    endY = clip.Y1;
                    endX = P + Negate(MajorFromMinorEnd(static_cast<uint32_t>(clip.Y1 - Q), slope), sameSign);
                }
            }
            else
            {
                if (codeStart & 8)
                {
                    startX = clip.X0;
                    startY = Q + Negate(MajorFromMinorStart(static_cast<uint32_t>(clip.X0 - P), slope), sameSign);
                }
                else if (codeStart & 4)
                {
                    startX = clip.X1;
                    startY = Q + Negate(MajorFromMinorStart(static_cast<uint32_t>(P - clip.X1), slope), notSame);
                }
                else if (codeStart & 2)
                {
                    startY = clip.Y0;
                    startX = P + Negate(MinorFromMajor(static_cast<uint32_t>(clip.Y0 - Q), slope), sameSign);
                }
                else if (codeStart & 1)
                {
                    startY = clip.Y1;
                    startX = P + Negate(MinorFromMajor(static_cast<uint32_t>(Q - clip.Y1), slope), notSame);
                }
                else if (codeEnd & 8)
                {
                    endX = clip.X0;
                    endY = Q + Negate(MajorFromMinorEnd(static_cast<uint32_t>(P - clip.X0), slope), notSame);
                }
                else if (codeEnd & 4)
                {
                    endX = clip.X1;
                    endY = Q + Negate(MajorFromMinorEnd(static_cast<uint32_t>(clip.X1 - P), slope), sameSign);
                }
                else if (codeEnd & 2)
                {
                    endY = clip.Y0;
                    endX = P + Negate(MinorFromMajor(static_cast<uint32_t>(Q - clip.Y0), slope), notSame);
                }
                else
                {
                    endY = clip.Y1;
                    endX = P + Negate(MinorFromMajor(static_cast<uint32_t>(clip.Y1 - Q), slope), sameSign);
                }
            }
        }

        const int32_t result = everClipped != 0 ? 1 : 0;

        const int32_t rowStep = signY != 0 ? -clip.Stride : clip.Stride;

        if (adx != ady)
        {
            if (!xMajor)
            {
                // y-major: one row per pixel, the column advancing when the fraction carries.
                count = std::abs(endY - startY) + 1;
                uint32_t fraction = static_cast<uint32_t>(std::abs(startY - Q)) * slope + 0x80000000u;

                if (mode == LD_EXECUTE)
                {
                    const auto callback = reinterpret_cast<VFX_LINE_CALLBACK>(parm);
                    int32_t x = startX - pane->x0;
                    int32_t y = startY - pane->y0;
                    const int32_t yStep = signY * 2 + 1;
                    const int32_t xStep = signX != 0 ? -1 : 1;

                    for (; count != 0; --count)
                    {
                        callback(x, y);
                        const uint32_t before = fraction;
                        fraction += slope;

                        if (fraction < before)
                        {
                            x += xStep;
                        }

                        y += yStep;
                    }

                    return result;
                }

                uint8_t* pixel = clip.At(startX, startY);
                const int32_t xStep = signX != 0 ? -1 : 1;

                for (; count != 0; --count)
                {
                    Plot(pixel, mode, parm);
                    const uint32_t before = fraction;
                    fraction += slope;
                    pixel += rowStep + (fraction < before ? xStep : 0);
                }

                return result;
            }

            // x-major: one column per pixel, the row advancing when the fraction carries.
            count = std::abs(endX - startX) + 1;
            uint32_t fraction = static_cast<uint32_t>(std::abs(startX - P)) * slope + 0x80000000u;

            if (mode == LD_EXECUTE)
            {
                const auto callback = reinterpret_cast<VFX_LINE_CALLBACK>(parm);
                int32_t x = startX - pane->x0;
                int32_t y = startY - pane->y0;
                // Original behaviour: the asm steps x by the sign of dy and y by the sign of dx here (the y-major
                // case's registers), so LD_EXECUTE walks x-major lines wrongly unless both run right and down.
                const int32_t xStep = signY * 2 + 1;
                const int32_t yStep = signX != 0 ? -1 : 1;

                for (; count != 0; --count)
                {
                    callback(x, y);
                    const uint32_t before = fraction;
                    fraction += slope;

                    if (fraction < before)
                    {
                        y += yStep;
                    }

                    x += xStep;
                }

                return result;
            }

            uint8_t* pixel = clip.At(startX, startY);
            const int32_t xStep = signX != 0 ? -1 : 1;

            for (; count != 0; --count)
            {
                Plot(pixel, mode, parm);
                const uint32_t before = fraction;
                fraction += slope;
                pixel += xStep + (fraction < before ? rowStep : 0);
            }

            return result;
        }

        // 45 degrees: one row and one column per pixel.
        step = rowStep + (signX != 0 ? -1 : 1);
        count = std::abs(endX - startX) + 1;

        if (mode == LD_EXECUTE)
        {
            const auto callback = reinterpret_cast<VFX_LINE_CALLBACK>(parm);
            int32_t x = startX - pane->x0;
            int32_t y = startY - pane->y0;
            const int32_t yStep = 1 | signY;
            const int32_t xStep = 1 | signX;

            for (; count != 0; --count, x += xStep, y += yStep)
            {
                callback(x, y);
            }

            return result;
        }

        uint8_t* pixel = clip.At(startX, startY);

        for (; count != 0; --count, pixel += step)
        {
            Plot(pixel, mode, parm);
        }

        return result;
    }

    // Vertical and horizontal lines (and single points) are clamped rather than clipped, and report 0 even when
    // clamped (original behaviour).
    if (dx == 0)
    {
        if (x0 < clip.X0 || x0 > clip.X1)
        {
            return 2;
        }

        if (std::max(y0, y1) < clip.Y0)
        {
            return 2;
        }

        if (std::min(y0, y1) > clip.Y1)
        {
            return 2;
        }

        startY = std::clamp(y0, clip.Y0, clip.Y1);
        endY = std::clamp(y1, clip.Y0, clip.Y1);
        startX = x0;
        step = signY != 0 ? -clip.Stride : clip.Stride;
        count = std::abs(endY - startY) + 1;
    }
    else
    {
        if (y0 < clip.Y0 || y0 > clip.Y1)
        {
            return 2;
        }

        if (std::max(x0, x1) < clip.X0)
        {
            return 2;
        }

        if (std::min(x0, x1) > clip.X1)
        {
            return 2;
        }

        startX = std::clamp(x0, clip.X0, clip.X1);
        endX = std::clamp(x1, clip.X0, clip.X1);
        startY = y0;
        step = signX != 0 ? -1 : 1;
        count = std::abs(endX - startX) + 1;
    }

    if (mode == LD_EXECUTE)
    {
        const auto callback = reinterpret_cast<VFX_LINE_CALLBACK>(parm);
        int32_t x = startX - pane->x0;
        int32_t y = startY - pane->y0;
        const int32_t yStep = (dy != 0 ? 1 : 0) | signY;
        const int32_t xStep = (dx != 0 ? 1 : 0) | signX;

        for (; count != 0; --count, x += xStep, y += yStep)
        {
            callback(x, y);
        }

        return 0;
    }

    uint8_t* pixel = clip.At(startX, startY);

    for (; count != 0; --count, pixel += step)
    {
        Plot(pixel, mode, parm);
    }

    return 0;
}

int32_t VFX_rectangle_hash(PANE* pane, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t color)
{
    MCVfxClip clip;
    const int32_t status = MCVfxClipPane(pane, clip);

    if (status != 0)
    {
        return status;
    }

    x0 = std::max(x0 + clip.PaneX, clip.X0);
    y0 = std::max(y0 + clip.PaneY, clip.Y0);
    x1 = std::min(x1 + clip.PaneX, clip.X1);
    y1 = std::min(y1 + clip.PaneY, clip.Y1);
    const int32_t width = x1 - x0 + 1;

    if (width <= 0 || y1 < y0)
    {
        return VFX_ERR_BAD_SHAPE;
    }

    // The pattern's phase follows the number of rows below the current one: rows an even distance above y1 start
    // at x0, the others one pixel in.
    const uint8_t* end = clip.Buffer + static_cast<intptr_t>(clip.Stride) * clip.Height;

    for (int32_t y = y0; y <= y1; ++y)
    {
        uint8_t* pixel = clip.At(x0, y);
        int32_t left = width;

        if ((y1 - y) & 1)
        {
            ++pixel;
            --left;
        }

        // Original behaviour: at least one pixel is written per row, even one past x1 when an odd row is one pixel
        // wide. Port fix: never past the window's last pixel.
        do
        {
            if (pixel < end)
            {
                *pixel = color;
            }

            pixel += 2;
            left -= 2;
        } while (left > 0);
    }

    return 0;
}

int32_t VFX_pane_wipe(PANE* pane, int32_t color)
{
    MCVfxClip clip;
    const int32_t status = MCVfxClipPane(pane, clip);

    if (status != 0)
    {
        return status;
    }

    const size_t width = static_cast<size_t>(clip.X1 + 1 - clip.X0);

    for (int32_t y = clip.Y0; y <= clip.Y1; ++y)
    {
        std::memset(clip.At(clip.X0, y), static_cast<uint8_t>(color), width);
    }

    return 0;
}

int32_t VFX_pane_copy(PANE* source, int32_t sx, int32_t sy, PANE* target, int32_t tx, int32_t ty, int32_t fill)
{
    MCVfxClip from;
    int32_t status = MCVfxClipPane(source, from);

    if (status != 0)
    {
        return status;
    }

    MCVfxClip to;
    status = MCVfxClipPane(target, to);

    if (status != 0)
    {
        return status;
    }

    // Both clip rectangles relative to their pane's origin.
    const int32_t sLeft = from.X0 - from.PaneX;
    const int32_t sTop = from.Y0 - from.PaneY;
    const int32_t sRight = from.X1 - from.PaneX;
    const int32_t sBottom = from.Y1 - from.PaneY;
    const int32_t tLeft = to.X0 - to.PaneX;
    const int32_t tTop = to.Y0 - to.PaneY;
    const int32_t tRight = to.X1 - to.PaneX;
    const int32_t tBottom = to.Y1 - to.PaneY;

    // The area both cover, in source coordinates, then in target coordinates.
    const int32_t offsetX = sx - tx;
    const int32_t offsetY = sy - ty;
    const int32_t left = std::max(sLeft, tLeft + offsetX);
    const int32_t top = std::max(sTop, tTop + offsetY);
    const int32_t right = std::min(sRight, tRight + offsetX);
    const int32_t bottom = std::min(sBottom, tBottom + offsetY);

    if (right < left || bottom < top)
    {
        return VFX_ERR_CLIPPED;
    }

    const int32_t toLeft = std::max(tLeft, sLeft - offsetX);
    const int32_t toTop = std::max(tTop, sTop - offsetY);
    const int32_t toRight = std::min(tRight, sRight - offsetX);
    const int32_t toBottom = std::min(tBottom, sBottom - offsetY);
    const int32_t width = right + 1 - left;
    const int32_t height = bottom + 1 - top;

    // Copy in the order that survives overlap: rows downwards when the source row is below the target row (in pane
    // coordinates), else upwards; columns likewise.
    const uint8_t* src = from.Buffer + static_cast<intptr_t>(from.PaneY) * from.Stride + from.PaneX;
    uint8_t* dst = to.Buffer + static_cast<intptr_t>(to.PaneY) * to.Stride + to.PaneX;
    intptr_t srcRowStep;
    intptr_t dstRowStep;

    if (top > toTop)
    {
        src += static_cast<intptr_t>(top) * from.Stride;
        dst += static_cast<intptr_t>(toTop) * to.Stride;
        srcRowStep = from.Stride;
        dstRowStep = to.Stride;
    }
    else
    {
        src += static_cast<intptr_t>(bottom) * from.Stride;
        dst += static_cast<intptr_t>(toBottom) * to.Stride;
        srcRowStep = -from.Stride;
        dstRowStep = -to.Stride;
    }

    intptr_t pixelStep;

    if (left > toLeft)
    {
        src += left;
        dst += toLeft;
        pixelStep = 1;
    }
    else
    {
        src += right;
        dst += toRight;
        pixelStep = -1;
    }

    if (fill < 0)
    {
        // Plain copy (NO_COLOR and every negative value).
        for (int32_t row = 0; row < height; ++row, src += srcRowStep, dst += dstRowStep)
        {
            if (pixelStep > 0)
            {
                std::memmove(dst, src, static_cast<size_t>(width));
            }
            else
            {
                std::memmove(dst - (width - 1), src - (width - 1), static_cast<size_t>(width));
            }
        }
    }
    else if ((fill & 0xffffff00) == 0)
    {
        // A colour: the target area is filled with it instead of copied.
        for (int32_t row = 0; row < height; ++row, dst += dstRowStep)
        {
            std::memset(pixelStep > 0 ? dst : dst - (width - 1), fill, static_cast<size_t>(width));
        }
    }
    else
    {
        // Above 255: a copy that leaves the pixels of colour (fill & 0xff) alone.
        const uint8_t key = static_cast<uint8_t>(fill);

        for (int32_t row = 0; row < height; ++row, src += srcRowStep, dst += dstRowStep)
        {
            const uint8_t* s = src;
            uint8_t* d = dst;

            for (int32_t i = 0; i < width; ++i, s += pixelStep, d += pixelStep)
            {
                if (*s != key)
                {
                    *d = *s;
                }
            }
        }
    }

    return 0;
}

int32_t VFX_pane_scroll(PANE* pane, int32_t dx, int32_t dy, int32_t mode, int32_t parm)
{
    const int32_t width = pane->x1 + 1 - pane->x0;
    const int32_t height = pane->y1 + 1 - pane->y0;

    if (width <= 0 || height <= 0)
    {
        return VFX_ERR_EMPTY_PANE;
    }

    PANE* source = pane;
    int32_t fill = parm;
    WINDOW wrapWindow;
    PANE wrapPane;

    if (mode != 1)
    {
        if (std::abs(dx) >= width || std::abs(dy) >= height)
        {
            return VFX_pane_wipe(pane, parm & 0xff);
        }
    }
    else
    {
        // PS_WRAP: the pane is copied to a scratch window first. parm was that window's buffer (a pointer passed as
        // a long; a call with 0 returned the size it needs); the port allocates it.
        if (parm == 0)
        {
            return width * height;
        }

        std::vector<uint8_t> scratch(static_cast<size_t>(width) * height);
        wrapWindow = WINDOW{scratch.data(), width - 1, height - 1};
        wrapPane = PANE{&wrapWindow, 0, 0, width - 1, height - 1};
        VFX_pane_copy(pane, 0, 0, &wrapPane, 0, 0, NO_COLOR);
        dx %= width;
        dy %= height;
        fill = NO_COLOR;
        source = &wrapPane;

        if ((dx | dy) != 0)
        {
            VFX_pane_copy(source, 0, 0, pane, dx, dy, NO_COLOR);
            const int32_t offsets[8][2] = {{width, height}, {width, 0},       {width, -height}, {0, height},
                                           {0, -height},    {-width, height}, {-width, 0},      {-width, -height}};

            for (const auto& offset : offsets)
            {
                VFX_pane_copy(source, offset[0], offset[1], pane, dx, dy, fill);
            }
        }

        return 0;
    }

    if ((dx | dy) == 0)
    {
        return 0;
    }

    VFX_pane_copy(source, 0, 0, pane, dx, dy, NO_COLOR);
    // The eight neighbours of the moved image: with a colour they fill the uncovered strips (VFX_pane_copy fills
    // with a colour of 0..255); with NO_COLOR they copy the pane's already-moved pixels (original behaviour).
    const int32_t offsets[8][2] = {{width, height}, {width, 0},       {width, -height}, {0, height},
                                   {0, -height},    {-width, height}, {-width, 0},      {-width, -height}};

    for (const auto& offset : offsets)
    {
        VFX_pane_copy(source, offset[0], offset[1], pane, dx, dy, fill);
    }

    return 0;
}

namespace
{
    /// <summary>The state of the midpoint ellipse both ellipse routines step through (all 32-bit, as in the asm).</summary>
    struct EllipseWalk
    {
        uint32_t A2;
        uint32_t B2;
        uint32_t TwoA2;
        uint32_t TwoB2;
        uint32_t Px;
        uint32_t Py;
        uint32_t P;
        int32_t X;
        int32_t Y;

        EllipseWalk(int32_t a, int32_t b)
        {
            B2 = static_cast<uint32_t>(b) * static_cast<uint32_t>(b);
            TwoB2 = B2 << 1;
            A2 = static_cast<uint32_t>(a) * static_cast<uint32_t>(a);
            TwoA2 = A2 << 1;
            Px = 0;
            Py = TwoA2 * static_cast<uint32_t>(b);
            P = (A2 >> 2) + B2 - A2 * static_cast<uint32_t>(b);
            X = 0;
            Y = b;
        }

        /// <summary>Calls <paramref name="plot"/>(x, y) for each step of the first quadrant, from (0, b) to (a, 0).</summary>
        template <typename Plot> void Walk(Plot plot)
        {
            int32_t rows = Y;

            // Region 1: slope above -1, x advances every step.
            while (static_cast<int32_t>(Px - Py) < 0)
            {
                plot(X, Y);

                if (static_cast<int32_t>(P) >= 0)
                {
                    --Y;
                    --rows;
                    Py -= TwoA2;
                    P -= Py;
                }

                ++X;
                Px += TwoB2;
                P += Px + B2;
            }

            // Region 2: y descends every step.
            const int32_t diff = static_cast<int32_t>(A2 - B2);
            P += static_cast<uint32_t>(((diff >> 1) + diff - static_cast<int32_t>(Px) - static_cast<int32_t>(Py)) >> 1);

            for (;;)
            {
                plot(X, Y);

                if (static_cast<int32_t>(P) < 0)
                {
                    ++X;
                    Px += TwoB2;
                    P += Px;
                }

                --Y;
                Py -= TwoA2;
                P -= Py - A2;

                if (--rows < 0)
                {
                    break;
                }
            }
        }
    };
}

int32_t VFX_ellipse_draw(PANE* pane, int32_t xc, int32_t yc, int32_t width, int32_t height, int32_t color)
{
    if (width == 0 || height == 0)
    {
        return VFX_line_draw(pane, xc - width, yc - height, xc + width, yc + height, LD_DRAW, color);
    }

    MCVfxClip clip;
    const int32_t status = MCVfxClipPane(pane, clip);

    if (status != 0)
    {
        return status;
    }

    const int32_t cx = xc + clip.PaneX;
    const int32_t cy = yc + clip.PaneY;
    const auto put = [&](int32_t x, int32_t y)
    {
        if (x >= clip.X0 && x <= clip.X1 && y >= clip.Y0 && y <= clip.Y1)
        {
            *clip.At(x, y) = static_cast<uint8_t>(color);
        }
    };

    EllipseWalk walk(width, height);
    walk.Walk(
        [&](int32_t x, int32_t y)
        {
            put(cx + x, cy + y);
            put(cx + x, cy - y);
            put(cx - x, cy + y);
            put(cx - x, cy - y);
        });
    // Original behaviour: the asm returned whatever EAX held (a leftover of the stepping); 0 here.
    return 0;
}

int32_t VFX_ellipse_fill(PANE* pane, int32_t xc, int32_t yc, int32_t width, int32_t height, int32_t color)
{
    if (width == 0 || height == 0)
    {
        return VFX_line_draw(pane, xc - width, yc - height, xc + width, yc + height, LD_DRAW, color);
    }

    MCVfxClip clip;
    const int32_t status = MCVfxClipPane(pane, clip);

    if (status != 0)
    {
        return status;
    }

    const int32_t cx = xc + clip.PaneX;
    const int32_t cy = yc + clip.PaneY;
    EllipseWalk walk(width, height);
    walk.Walk(
        [&](int32_t x, int32_t y)
        {
            // The span cx - x .. cx + x, clamped; skipped when wholly outside.
            int32_t right = cx + x;

            if (right < clip.X0)
            {
                return;
            }

            right = std::min(right, clip.X1);
            int32_t left = cx - x;

            if (left > clip.X1)
            {
                return;
            }

            left = std::max(left, clip.X0);
            const size_t length = static_cast<size_t>(right - left + 1);

            const int32_t below = cy + y;

            if (below < clip.Y0)
            {
                return;
            }

            if (below <= clip.Y1)
            {
                std::memset(clip.At(left, below), static_cast<uint8_t>(color), length);
            }

            const int32_t above = cy - y;

            if (above < clip.Y0 || above > clip.Y1)
            {
                return;
            }

            std::memset(clip.At(left, above), static_cast<uint8_t>(color), length);
        });
    return 0;
}
