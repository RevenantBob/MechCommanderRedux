#include "stdafx.h"
#include "vfx/vfxint.h"

// The game's ellipses (mcx\vfx\vfx_ellipse.cpp, inline assembly): VFX's midpoint ellipse with translucent colours.
// The asm kept the stepping state in globals (0x008023c4..0x008023f4) and called the plotter through a pointer
// (0x008023d4) with the caller's frame; the port passes the frame as a struct.

namespace
{
    /// <summary>The caller's frame the plotters read: the clip rectangle, the window and the centre.</summary>
    struct EllipseFrame
    {
        uint8_t* Buffer;
        int32_t Stride; // 0x008023cc
        int32_t ClipX0;
        int32_t ClipY0;
        int32_t ClipX1;
        int32_t ClipY1;
        int32_t CenterX;
        int32_t CenterY;
        uint8_t Color;
        bool Alpha;
    };

    /// <summary>Resolves the pane as the ellipse routines do (the centre is offset by the clipped origin).</summary>
    EllipseFrame MakeFrame(PANE* pane, int32_t xc, int32_t yc, int32_t color)
    {
        const WINDOW* window = pane->window;
        EllipseFrame frame;
        frame.ClipX0 = std::max(pane->x0, 0);
        frame.ClipY0 = std::max(pane->y0, 0);
        frame.Stride = window->x_max + 1;
        frame.ClipX1 = pane->x1 < window->x_max + 1 ? pane->x1 : window->x_max;
        frame.ClipY1 = pane->y1 < window->y_max + 1 ? pane->y1 : window->y_max;
        frame.Buffer = window->buffer;
        // Original behaviour: offset by the pane's origin clipped to the window, not the pane's own.
        frame.CenterX = xc + frame.ClipX0;
        frame.CenterY = yc + frame.ClipY0;
        frame.Color = static_cast<uint8_t>(color);
        // SpecialColor takes the whole colour; the blend row only its low byte. Port fix: a colour outside the table
        // (which the original read past) counts as solid.
        frame.Alpha = color >= 0 && color < ALPHA_COLORS && SpecialColor[color] == 1;
        return frame;
    }

    void PlotPoint(const EllipseFrame& f, int32_t x, int32_t y)
    {
        if (x < f.ClipX0 || x > f.ClipX1 || y < f.ClipY0 || y > f.ClipY1)
        {
            return;
        }

        uint8_t* p = f.Buffer + static_cast<intptr_t>(y) * f.Stride + x;
        *p = f.Alpha ? static_cast<uint8_t>(AlphaTable[(static_cast<uint32_t>(f.Color) << 8) | *p]) : f.Color;
    }

    /// <summary>
    /// FUN_006b6768 (solid) and 0x006b683a (translucent): the four symmetric points. Original behaviour: points on
    /// the axes are plotted twice, blending translucent colours twice.
    /// </summary>
    void PlotFour(const EllipseFrame& f, int32_t x, int32_t y)
    {
        PlotPoint(f, f.CenterX + x, f.CenterY + y);
        PlotPoint(f, f.CenterX + x, f.CenterY - y);
        PlotPoint(f, f.CenterX - x, f.CenterY + y);
        PlotPoint(f, f.CenterX - x, f.CenterY - y);
    }

    void FillSpan(const EllipseFrame& f, uint8_t* p, int32_t count)
    {
        for (; count != 0; --count, ++p)
        {
            *p = f.Alpha ? static_cast<uint8_t>(AlphaTable[(static_cast<uint32_t>(f.Color) << 8) | *p]) : f.Color;
        }
    }

    /// <summary>
    /// FUN_006b6b5a (solid) and 0x006b6c07 (translucent): the two spans at +y and -y. Original behaviour: a row is
    /// filled again at every step that stays on it, blending translucent colours repeatedly.
    /// </summary>
    void FillTwo(const EllipseFrame& f, int32_t x, int32_t y)
    {
        int32_t right = f.CenterX + x;

        if (right < f.ClipX0)
        {
            return;
        }

        if (right >= f.ClipX1)
        {
            right = f.ClipX1;
        }

        int32_t left = f.CenterX - x;

        if (left > f.ClipX1)
        {
            return;
        }

        if (left <= f.ClipX0)
        {
            left = f.ClipX0;
        }

        const int32_t below = f.CenterY + y;

        if (below < f.ClipY0)
        {
            return;
        }

        if (below <= f.ClipY1)
        {
            FillSpan(f, f.Buffer + static_cast<intptr_t>(below) * f.Stride + left, right - left + 1);
        }

        const int32_t above = f.CenterY - y;

        if (above < f.ClipY0 || above > f.ClipY1)
        {
            return;
        }

        FillSpan(f, f.Buffer + static_cast<intptr_t>(above) * f.Stride + left, right - left + 1);
    }

    /// <summary>The midpoint stepping both routines share, calling <paramref name="plot"/> with (x, y) at each step.</summary>
    template <typename Plot> void StepEllipse(int32_t width, int32_t height, Plot plot)
    {
        // 32-bit wrapping arithmetic, as the asm's MUL/SUB.
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

        // Region 1: steps in x while the slope is shallow.
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

        // Region 2: steps in y down to the axis.
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

void AG_ellipse_draw(PANE* pane, int32_t xc, int32_t yc, int32_t width, int32_t height, int32_t color)
{
    if (width == 0 || height == 0)
    {
        VFX_line_draw(pane, xc - width, yc - height, xc + width, yc + height, LD_DRAW, color);
        return;
    }

    const EllipseFrame frame = MakeFrame(pane, xc, yc, color);
    StepEllipse(width, height, [&](int32_t x, int32_t y) { PlotFour(frame, x, y); });
}

void AG_ellipse_fill(PANE* pane, int32_t xc, int32_t yc, int32_t width, int32_t height, int32_t color)
{
    if (width == 0 || height == 0)
    {
        VFX_line_draw(pane, xc - width, yc - height, xc + width, yc + height, LD_DRAW, color);
        return;
    }

    const EllipseFrame frame = MakeFrame(pane, xc, yc, color);
    StepEllipse(width, height, [&](int32_t x, int32_t y) { FillTwo(frame, x, y); });
}
