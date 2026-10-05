#pragma once

#include "engine/celement.h"
#include "lib/cvmath.h"

/// <summary>A line between two screen points, in one colour or through a fade table.</summary>
/// <remarks>Original source: <c>engine\celine.cpp</c>, 0x28 bytes.</remarks>
class LineElement : public Element
{
public:
    /// <summary>
    /// A line from <paramref name="start"/> to <paramref name="end"/> at <paramref name="_depth"/> in
    /// <paramref name="color"/>, or translated through <paramref name="fadeTable"/> when it is set;
    /// <paramref name="endColor"/> -1 makes a single-colour line (the only kind <see cref="draw"/> draws).
    /// </summary>
    /// <remarks>
    /// MCX.EXE @ 0x006b1a60 (the symbol is missing; the signature follows its callers).
    /// </remarks>
    LineElement(vector_2d& start, vector_2d& end, int32_t color, uint8_t* fadeTable, int32_t _depth, int32_t endColor);

    /// <remarks>MCX.EXE @ 0x006b1ab0; slot 0</remarks>
    void draw() override;

    /// <summary>The start on screen.</summary>
    vector_2d startPos; // +0x0c
    /// <summary>The end on screen.</summary>
    vector_2d endPos; // +0x14
    /// <summary>The colour (palette index).</summary>
    int32_t color; // +0x1c
    /// <summary>The far end's colour, -1 for one colour.</summary>
    int32_t endColor; // +0x20
    /// <summary>The fade table the line is drawn through, or null.</summary>
    uint8_t* fadeTable; // +0x24
};
