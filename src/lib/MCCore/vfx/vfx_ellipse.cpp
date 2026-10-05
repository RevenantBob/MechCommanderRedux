#include "stdafx.h"
#include "vfx/vfxint.h"

// The game's ellipses (mcx\vfx\vfx_ellipse.cpp, inline assembly): VFX's midpoint ellipse with translucent colours.
// The asm kept the stepping state in globals (0x008023c4..0x008023f4) and called the plotter through a pointer
// (0x008023d4) with the caller's frame; the renderer walks the ellipse.

namespace
{
    /// <summary>
    /// AG_ellipse_draw and AG_ellipse_fill: the centre is relative to the pane's origin; draws the ellipse, outlined (the plotters at
    /// 0x006b6768 and 0x006b683a: four points per step) or filled (the span fillers at 0x006b6b5a and 0x006b6c07: two
    /// spans per step).
    /// </summary>
    void DrawEllipse(PANE* pane, int32_t xc, int32_t yc, int32_t width, int32_t height, int32_t color, bool fill)
    {
        if (width == 0 || height == 0)
        {
            VFX_line_draw(pane, xc - width, yc - height, xc + width, yc + height, LD_DRAW, color);
            return;
        }

        const WINDOW* window = pane->window;
        MCEllipseCommand command;
        command.Clip.X0 = std::max(pane->x0, 0);
        command.Clip.Y0 = std::max(pane->y0, 0);
        command.Clip.X1 = pane->x1 < window->x_max + 1 ? pane->x1 : window->x_max;
        command.Clip.Y1 = pane->y1 < window->y_max + 1 ? pane->y1 : window->y_max;
        MCClipToView(window, command.Clip.X0, command.Clip.Y0, command.Clip.X1, command.Clip.Y1);

        if (command.Clip.X1 < command.Clip.X0 || command.Clip.Y1 < command.Clip.Y0)
        {
            return;
        }

        // OB-115: the asm offset by the pane's origin clipped to the window.
        command.CenterX = xc + pane->x0;
        command.CenterY = yc + pane->y0;
        command.Width = width;
        command.Height = height;
        command.Fill = fill;
        command.Color = static_cast<uint8_t>(color);
        // SpecialColor takes the whole colour; the blend row only its low byte. Port fix: a colour outside the table
        // (which the original read past) counts as solid.
        command.Alpha = color >= 0 && color < ALPHA_COLORS && SpecialColor[color] == 1;
        MCRenderer::For(pane->window).Ellipse(pane->window, command);
    }
}

void AG_ellipse_draw(PANE* pane, int32_t xc, int32_t yc, int32_t width, int32_t height, int32_t color)
{
    DrawEllipse(pane, xc, yc, width, height, color, false);
}

void AG_ellipse_fill(PANE* pane, int32_t xc, int32_t yc, int32_t width, int32_t height, int32_t color)
{
    DrawEllipse(pane, xc, yc, width, height, color, true);
}
