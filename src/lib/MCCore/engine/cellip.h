#pragma once

#include "engine/celement.h"
#include "lib/cvmath.h"

/// <summary>An ellipse outline around a screen point (<c>AG_ellipse_draw</c>).</summary>
/// <remarks>Original source: <c>engine\cellip.cpp</c>, 0x20 bytes.</remarks>
class MCEllipseElement : public MCElement
{
public:
    /// <summary>
    /// The ellipse centred at <paramref name="center"/> with radii <paramref name="size"/> in
    /// <paramref name="color"/> at <paramref name="depth"/>.
    /// </summary>
    MCEllipseElement(MCVector2D& center, MCVector2D& size, int32_t color, int32_t depth);

    void Draw() override;

    /// <summary>The centre on screen.</summary>
    MCVector2D Center;
    /// <summary>The horizontal and vertical radii.</summary>
    MCVector2D Size;
    /// <summary>The colour (palette index).</summary>
    int32_t Color;
};
