#pragma once

#include "engine/MCElement.h"
#include "lib/MCVector2D.h"

/// <summary>An ellipse outline around a screen point (<c>AGEllipseDraw</c>).</summary>
/// <remarks>Original source: <c>engine\cellip.cpp</c>.</remarks>
class MCEllipseElement : public MCElement
{
public:
    /// <summary>
    /// The ellipse centred at <paramref name="center"/> with radii <paramref name="size"/> in
    /// <paramref name="color"/> at <paramref name="depth"/>.
    /// </summary>
    MCEllipseElement(const MCVector2D& center, const MCVector2D& size, int32_t color, int32_t depth);

    void Draw() override;

    /// <summary>The centre on screen.</summary>
    MCVector2D Center;
    /// <summary>The horizontal and vertical radii.</summary>
    MCVector2D Size;
    /// <summary>The colour (palette index).</summary>
    int32_t Color = 0;
};
