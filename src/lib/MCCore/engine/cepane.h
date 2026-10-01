#pragma once

#include "engine/celement.h"

/// <summary>A copy of part of a pane's window, drawn transparently (colour 0 skipped) into the global pane.</summary>
/// <remarks>Original source: <c>engine\cepane.cpp</c>, 0x28 bytes.</remarks>
class PaneElement : public Element
{
public:
    /// <summary>
    /// Copies the <paramref name="width"/> x <paramref name="height"/> window of <paramref name="pane"/> to
    /// (<paramref name="x"/> - <paramref name="offsetX"/>, <paramref name="y"/> - <paramref name="offsetY"/>); the
    /// depth is -<paramref name="y"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b1bc0</remarks>
    PaneElement(_pane* pane, int32_t x, int32_t y, int32_t offsetX, int32_t offsetY, int32_t width, int32_t height);

    /// <remarks>MCX.EXE @ 0x006b1c10; slot 0</remarks>
    void draw() override;

    /// <summary>The pane whose window is copied.</summary>
    _pane* shapePane; // +0x0c
    int32_t x;        // +0x10
    int32_t y;        // +0x14
    int32_t offsetX;  // +0x18
    int32_t offsetY;  // +0x1c
    int32_t width;    // +0x20
    int32_t height;   // +0x24
};

/// <summary>
/// A shape drawn as two frames of one shape table: frame 0 (the base) then frame <see cref="frameNum"/> + 1 (the
/// delta over it). Used by the actors for shapes stored as base plus changes.
/// </summary>
/// <remarks>
/// Original source: <c>engine\cepane.cpp</c> (its line tables end in that file), 0x2c bytes. The constructor takes
/// the same arguments as <see cref="VFXElement"/>'s; only the shape, frame, position and fade table are used.
/// </remarks>
class DeltaElement : public Element
{
public:
    /// <remarks>MCX.EXE @ 0x006b1c40</remarks>
    DeltaElement(uint8_t* _shape, int32_t _x, int32_t _y, int32_t frame, int _reverse, uint8_t* fadeTbl,
                 int _noScaleDraw, int _scaleUp);

    /// <remarks>MCX.EXE @ 0x006b1c90; slot 0</remarks>
    void draw() override;

    /// <summary>The shape table.</summary>
    uint8_t* shapeTable; // +0x0c
    /// <summary>The delta frame, less one.</summary>
    int32_t frameNum; // +0x10
    int32_t x;        // +0x14
    int32_t y;        // +0x18
    /// <summary>Stored, unused by <see cref="draw"/>.</summary>
    int reverse; // +0x1c
    /// <summary>The fade table the shape is translated through, or null.</summary>
    uint8_t* fadeTable; // +0x20
    /// <summary>Stored, unused by <see cref="draw"/>.</summary>
    int noScaleDraw; // +0x24
    /// <summary>Stored, unused by <see cref="draw"/>.</summary>
    int scaleUp; // +0x28
};
