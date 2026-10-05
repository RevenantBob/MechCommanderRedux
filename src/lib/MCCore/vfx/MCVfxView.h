#pragma once

#include "vfx/MCVfx.h"

// MCView, the window that draws on another, is declared with MCWindow in vfx/MCVfx.h.

/// <summary>
/// Narrows a clip rectangle in <paramref name="window"/>'s coordinates to its view's scissor (nothing happens for a
/// window with pixels of its own). The vfx front end calls it wherever it clips a draw to its pane and window.
/// </summary>
inline void MCClipToView(const MCWindow& window, int32_t& x0, int32_t& y0, int32_t& x1, int32_t& y1)
{
    if (!window.View.has_value())
    {
        return;
    }

    const MCView& view = *window.View;
    x0 = std::max(x0, view.Scissor.X0 - view.OriginX);
    y0 = std::max(y0, view.Scissor.Y0 - view.OriginY);
    x1 = std::min(x1, view.Scissor.X1 - view.OriginX);
    y1 = std::min(y1, view.Scissor.Y1 - view.OriginY);
}
