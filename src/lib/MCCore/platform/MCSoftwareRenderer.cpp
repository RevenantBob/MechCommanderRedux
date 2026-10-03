#include "stdafx.h"
#include "platform/MCSoftwareRenderer.h"
#include "vfx/vfxint.h"

// The software renderer's fills, copies, pixels, lines, ellipses, status bars, glyphs and sprites: the pixel loops of
// vfxa.asm, vfx_transform.cpp, vfx_ellipse.cpp, vfx_map_polygon.cpp and the font and image routines.

namespace
{
    /// <summary>The address of window pixel (x, y).</summary>
    uint8_t* At(_window* window, int32_t x, int32_t y)
    {
        return window->buffer + static_cast<intptr_t>(y) * (window->x_max + 1) + x;
    }

    /// <summary>A blend through the game's alpha table: <paramref name="color"/> over <paramref name="screen"/>.</summary>
    uint8_t Blend(uint8_t color, uint8_t screen)
    {
        return static_cast<uint8_t>(AlphaTable[(static_cast<uint32_t>(color) << 8) | screen]);
    }

    /// <summary>
    /// The midpoint ellipse VFX steps through (all 32-bit wrapping arithmetic, as in the asm): calls
    /// <paramref name="plot"/>(x, y) for each step of the first quadrant, from (0, b) to (a, 0).
    /// </summary>
    template <typename Plot> void WalkEllipse(int32_t width, int32_t height, Plot plot)
    {
        const uint32_t b2 = static_cast<uint32_t>(height) * static_cast<uint32_t>(height);
        const uint32_t twoB2 = b2 << 1;
        const uint32_t a2 = static_cast<uint32_t>(width) * static_cast<uint32_t>(width);
        const uint32_t twoA2 = a2 << 1;
        uint32_t px = 0;
        uint32_t py = twoA2 * static_cast<uint32_t>(height);
        uint32_t d = (a2 >> 2) + b2 - a2 * static_cast<uint32_t>(height);
        int32_t x = 0;
        int32_t y = height;
        int32_t rows = height;

        // Region 1: slope above -1, x advances every step.
        while (static_cast<int32_t>(px - py) < 0)
        {
            plot(x, y);

            if (static_cast<int32_t>(d) >= 0)
            {
                --y;
                --rows;
                py -= twoA2;
                d -= py;
            }

            ++x;
            px += twoB2;
            d += px + b2;
        }

        const uint32_t diff = a2 - b2;
        d += static_cast<uint32_t>(
            static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(diff) >> 1) + diff - px - py) >> 1);

        // Region 2: y descends every step.
        for (;;)
        {
            plot(x, y);

            if (static_cast<int32_t>(d) < 0)
            {
                ++x;
                px += twoB2;
                d += px;
            }

            --y;
            py -= twoA2;
            d -= py - a2;

            if (--rows < 0)
            {
                break;
            }
        }
    }
}

void MCSeeThrough::Map(uint8_t* p, const uint8_t* table)
{
    if (table != _LastTable)
    {
        _LastTable = table;
        _LastOp = MCRenderer::OpFor(table);
        _LastIdentity = _LastOp == 0 && std::memcmp(table, MCRenderer::OpTables(), 256) == 0;
    }

    if (_LastIdentity)
    {
        return;
    }

    uint8_t& op = _Ops[p - _Base];
    const uint8_t composed = _LastOp != 0 ? MCRenderer::ComposeOps(op, _LastOp) : 0;

    // 0 is the identity, unless every op table is taken.
    if (composed == 0 && (_LastOp == 0 || MCRenderer::OpTableCount() == 256))
    {
        // Every op table is taken: map the key itself, as an opaque pixel.
        *p = table[*p];
        return;
    }

    op = composed;
}

void MCSeeThrough::Blend(uint8_t* p, uint8_t color)
{
    Map(p, reinterpret_cast<const uint8_t*>(AlphaTable) + static_cast<intptr_t>(color) * 256);
}

MCSoftwareRenderer& MCSoftwareRenderer::Instance()
{
    static MCSoftwareRenderer renderer;
    return renderer;
}

void MCSoftwareRenderer::Clear(_window* target, const MCRect& rect, uint8_t color)
{
    const size_t width = static_cast<size_t>(rect.X1 + 1 - rect.X0);
    uint8_t* ops = OpPlane(target);

    for (int32_t y = rect.Y0; y <= rect.Y1; ++y)
    {
        std::memset(At(target, rect.X0, y), color, width);

        if (ops != nullptr)
        {
            std::memset(ops + (At(target, rect.X0, y) - target->buffer), 0, width);
        }
    }
}

void MCSoftwareRenderer::Hash(_window* target, const MCRect& rect, uint8_t color)
{
    const int32_t width = rect.X1 - rect.X0 + 1;

    for (int32_t y = rect.Y0; y <= rect.Y1; ++y)
    {
        uint8_t* pixel = At(target, rect.X0, y);
        int32_t left = width;

        if ((rect.Y1 - y) & 1)
        {
            ++pixel;
            --left;
        }

        // OB-121: the asm wrote at least one pixel per row, one past x1 when an odd row was one pixel wide.
        for (; left > 0; pixel += 2, left -= 2)
        {
            *pixel = color;
        }
    }
}

void MCSoftwareRenderer::Copy(_window* target, const MCCopyCommand& command)
{
    const _window* source = command.Source;
    const MCRect& rect = command.SourceRect;
    const int32_t width = rect.X1 + 1 - rect.X0;
    const int32_t height = rect.Y1 + 1 - rect.Y0;
    const int32_t sourceStride = source->x_max + 1;
    const int32_t targetStride = target->x_max + 1;
    const uint8_t* src = source->buffer;
    uint8_t* dst = target->buffer;
    intptr_t srcRowStep;
    intptr_t dstRowStep;

    if (command.Downwards)
    {
        src += static_cast<intptr_t>(rect.Y0) * sourceStride;
        dst += static_cast<intptr_t>(command.Y) * targetStride;
        srcRowStep = sourceStride;
        dstRowStep = targetStride;
    }
    else
    {
        src += static_cast<intptr_t>(rect.Y1) * sourceStride;
        dst += static_cast<intptr_t>(command.Y + height - 1) * targetStride;
        srcRowStep = -sourceStride;
        dstRowStep = -targetStride;
    }

    intptr_t pixelStep;

    if (command.Rightwards)
    {
        src += rect.X0;
        dst += command.X;
        pixelStep = 1;
    }
    else
    {
        src += rect.X1;
        dst += command.X + width - 1;
        pixelStep = -1;
    }

    if (!command.ColorKey)
    {
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

        return;
    }

    for (int32_t row = 0; row < height; ++row, src += srcRowStep, dst += dstRowStep)
    {
        const uint8_t* s = src;
        uint8_t* d = dst;

        for (int32_t i = 0; i < width; ++i, s += pixelStep, d += pixelStep)
        {
            if (*s != command.Key)
            {
                *d = *s;
            }
        }
    }
}

void MCSoftwareRenderer::AlphaBlit(_window* target, const MCAlphaBlitCommand& command)
{
    const int32_t stride = target->x_max + 1;
    const int32_t pitch = command.Pitch;
    const uint8_t* source = command.Sprite + command.Offset;
    uint8_t* destination = At(target, command.Left, command.Top);
    MCSeeThrough seeThrough(target);
    const auto blend = [&](uint8_t* p, uint8_t color)
    {
        if (seeThrough.At(p))
        {
            seeThrough.Blend(p, color);
        }
        else
        {
            *p = Blend(color, *p);
        }
    };

    if (command.FullSize)
    {
        const uint8_t* s = command.Mirror ? source + pitch - 1 : source;
        const int32_t step = command.Mirror ? -1 : 1;

        for (int32_t row = 0; row < command.Rows; ++row)
        {
            for (int32_t column = 0; column < command.Columns; ++column)
            {
                blend(destination + column, *s);
                s += step;
            }

            s += pitch - step * command.Columns;
            destination += stride;
        }

        return;
    }

    // Half size: every other pixel of every other row.
    const int32_t halfRows = static_cast<int32_t>(static_cast<uint32_t>(command.Rows) >> 1);
    const int32_t halfColumns = static_cast<int32_t>(static_cast<uint32_t>(command.Columns) >> 1);
    const uint8_t* s = command.Mirror ? source + pitch * 2 - 1 : source;
    const int32_t step = command.Mirror ? -2 : 2;

    for (int32_t row = 0; row < halfRows; ++row)
    {
        for (int32_t column = 0; column < halfColumns; ++column)
        {
            blend(destination + column, *s);
            s += step;
        }

        s += pitch * 2 - step * halfColumns;
        destination += stride;
    }
}

void MCSoftwareRenderer::Write(_window* target, int32_t x, int32_t y, const uint8_t* pixels, int32_t count)
{
    std::memcpy(At(target, x, y), pixels, static_cast<size_t>(count));
}

void MCSoftwareRenderer::Pixel(_window* target, int32_t x, int32_t y, uint8_t color)
{
    *At(target, x, y) = color;
}

void MCSoftwareRenderer::Line(_window* target, const MCLineCommand& command)
{
    int32_t x = command.X;
    int32_t y = command.Y;
    uint32_t fraction = command.Fraction;

    MCSeeThrough seeThrough(target);

    for (int32_t count = command.Count; count != 0; --count)
    {
        uint8_t* pixel = At(target, x, y);

        if (command.Table != nullptr && seeThrough.At(pixel))
        {
            seeThrough.Map(pixel, command.Table);
        }
        else
        {
            *pixel = command.Table != nullptr ? command.Table[*pixel] : command.Color;
        }

        const uint32_t before = fraction;
        fraction += command.Slope;

        if (fraction < before)
        {
            x += command.MinorX;
            y += command.MinorY;
        }

        x += command.MajorX;
        y += command.MajorY;
    }
}

void MCSoftwareRenderer::Ellipse(_window* target, const MCEllipseCommand& command)
{
    const MCRect& clip = command.Clip;
    const int32_t cx = command.CenterX;
    const int32_t cy = command.CenterY;
    MCSeeThrough seeThrough(target);
    const auto paint = [&](uint8_t* p)
    {
        if (!command.Alpha)
        {
            *p = command.Color;
        }
        else if (seeThrough.At(p))
        {
            seeThrough.Blend(p, command.Color);
        }
        else
        {
            *p = Blend(command.Color, *p);
        }
    };

    if (!command.Fill)
    {
        // OB-122: the asm plotted the points on the axes twice (blending translucent colours twice).
        const auto put = [&](int32_t x, int32_t y)
        {
            if (x >= clip.X0 && x <= clip.X1 && y >= clip.Y0 && y <= clip.Y1)
            {
                paint(At(target, x, y));
            }
        };

        WalkEllipse(command.Width, command.Height,
                    [&](int32_t x, int32_t y)
                    {
                        put(cx + x, cy + y);

                        if (y != 0)
                        {
                            put(cx + x, cy - y);
                        }

                        if (x != 0)
                        {
                            put(cx - x, cy + y);

                            if (y != 0)
                            {
                                put(cx - x, cy - y);
                            }
                        }
                    });
        return;
    }

    // OB-122: the asm filled a row again at every step that stayed on it (blending translucent colours repeatedly),
    // and the middle row twice. Each row is filled once, at its widest: the walk's x never shrinks and its y never
    // grows, so that is the last step on the row.
    const auto span = [&](int32_t row, int32_t left, int32_t right)
    {
        uint8_t* p = At(target, left, row);

        for (int32_t count = right - left + 1; count != 0; --count, ++p)
        {
            paint(p);
        }
    };

    const auto rows = [&](int32_t x, int32_t y)
    {
        // The span cx - x .. cx + x, clamped; skipped when wholly outside.
        const int32_t right = std::min(cx + x, clip.X1);
        const int32_t left = std::max(cx - x, clip.X0);

        if (right < left)
        {
            return;
        }

        const int32_t below = cy + y;

        if (below >= clip.Y0 && below <= clip.Y1)
        {
            span(below, left, right);
        }

        const int32_t above = cy - y;

        if (y != 0 && above >= clip.Y0 && above <= clip.Y1)
        {
            span(above, left, right);
        }
    };

    bool pending = false;
    int32_t pendingX = 0;
    int32_t pendingY = 0;

    WalkEllipse(command.Width, command.Height,
                [&](int32_t x, int32_t y)
                {
                    if (pending && y != pendingY)
                    {
                        rows(pendingX, pendingY);
                    }

                    pending = true;
                    pendingX = x;
                    pendingY = y;
                });

    if (pending)
    {
        rows(pendingX, pendingY);
    }
}

void MCSoftwareRenderer::StatusBar(_window* target, const MCStatusBarCommand& command)
{
    // The AlphaTable row status-bar frames are darkened through (0x008011d0 in MCX.EXE).
    constexpr int32_t StatusFrameAlpha = 0x108;
    const uint8_t* frame = reinterpret_cast<const uint8_t*>(AlphaTable) + StatusFrameAlpha * 256;
    const uint8_t* fill =
        reinterpret_cast<const uint8_t*>(AlphaTable) + static_cast<intptr_t>(command.AlphaColor) * 256;
    const MCRect& box = command.Box;
    const int32_t width = box.X1 - box.X0;
    const int32_t stride = target->x_max + 1;
    uint8_t* p = At(target, box.X0, box.Y0);
    MCSeeThrough seeThrough(target);
    const auto map = [&](uint8_t* pixel, const uint8_t* table)
    {
        if (seeThrough.At(pixel))
        {
            seeThrough.Map(pixel, table);
        }
        else
        {
            *pixel = table[*pixel];
        }
    };

    for (int32_t y = box.Y0; y <= box.Y1; ++y, p += stride)
    {
        if (y == command.FrameTop || y == command.FrameBottom)
        {
            // The frame's top or bottom: the pixels between the corners.
            for (int32_t i = 1; i < width; ++i)
            {
                map(p + i, frame);
            }

            continue;
        }

        map(p, frame);
        map(p + width, frame);

        if (command.BarLength != 0)
        {
            for (int32_t i = 1; i <= command.BarLength + 1; ++i)
            {
                map(p + i, fill);
            }
        }
    }
}

void MCSoftwareRenderer::Glyph(_window* target, const MCGlyphCommand& command)
{
    // The glyph: its width dword, then its rows (layout in vfx/vfxfuncs.h).
    const uint8_t* font = static_cast<const uint8_t*>(command.Font);
    const uint8_t* glyph = font + MCVfxRead32(font + 0x10 + static_cast<intptr_t>(command.Character) * 4);
    const int32_t width = MCVfxRead32(glyph);
    const uint8_t* source = glyph + 4 + static_cast<intptr_t>(command.SourceY) * width + command.SourceX;
    const int32_t stride = target->x_max + 1;
    uint8_t* dest = At(target, command.X, command.Y);

    for (int32_t rows = command.Rows; rows > 0; --rows)
    {
        if (command.Table == nullptr)
        {
            std::memcpy(dest, source, static_cast<size_t>(command.Columns));
        }
        else
        {
            for (int32_t i = 0; i < command.Columns; ++i)
            {
                const uint8_t pixel = command.Table[source[i]];

                if (pixel != 0xff)
                {
                    dest[i] = pixel;
                }
            }
        }

        source += width;
        dest += stride;
    }
}
