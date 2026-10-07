#pragma once

#include "engine/MCElement.h"
#include "lib/MCVector2D.h"

/// <summary>A line between two screen points, in one colour or through a fade table.</summary>
/// <remarks>Original source: <c>engine\celine.cpp</c>.</remarks>
class MCLineElement : public MCElement
{
public:
    /// <summary>
    /// A line from <paramref name="start"/> to <paramref name="end"/> at <paramref name="depth"/> in
    /// <paramref name="color"/>, or translated through <paramref name="fadeTable"/> when it is set;
    /// <paramref name="endColor"/> -1 makes a single-colour line (the only kind <see cref="Draw"/> draws).
    /// </summary>
    MCLineElement(const MCVector2D& start, const MCVector2D& end, int32_t color, const uint8_t* fadeTable,
                  int32_t depth, int32_t endColor);

    void Draw() override;

    /// <summary>The start on screen.</summary>
    MCVector2D StartPos;
    /// <summary>The end on screen.</summary>
    MCVector2D EndPos;
    /// <summary>The colour (palette index).</summary>
    int32_t Color = 0;
    /// <summary>The far end's colour, -1 for one colour.</summary>
    int32_t EndColor = 0;
    /// <summary>The fade table the line is drawn through, or null.</summary>
    const uint8_t* FadeTable = nullptr;
};
