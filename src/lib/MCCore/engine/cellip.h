#pragma once

#include "engine/celement.h"
#include "lib/cvmath.h"

/// <summary>An ellipse outline around a screen point (<c>AG_ellipse_draw</c>).</summary>
/// <remarks>Original source: <c>engine\cellip.cpp</c>, 0x20 bytes.</remarks>
class EllipseElement : public Element
{
public:
    /// <summary>
    /// The ellipse centred at <paramref name="_center"/> with radii <paramref name="_size"/> in
    /// <paramref name="_color"/> at <paramref name="_depth"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b1b40</remarks>
    EllipseElement(vector_2d& _center, vector_2d& _size, int32_t _color, int32_t _depth);

    /// <remarks>MCX.EXE @ 0x006b1b80; slot 0</remarks>
    void draw() override;

    /// <summary>The centre on screen.</summary>
    vector_2d center; // +0x0c
    /// <summary>The horizontal and vertical radii.</summary>
    vector_2d size; // +0x14
    /// <summary>The colour (palette index).</summary>
    int32_t color; // +0x1c
};
