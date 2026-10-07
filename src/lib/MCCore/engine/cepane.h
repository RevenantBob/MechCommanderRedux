#pragma once

#include "engine/celement.h"

/// <summary>A copy of part of a pane's window, drawn transparently (colour 0 skipped) into the global pane.</summary>
/// <remarks>Original source: <c>engine\cepane.cpp</c>, 0x28 bytes.</remarks>
class MCPaneElement : public MCElement
{
public:
    /// <summary>
    /// Copies the <paramref name="width"/> x <paramref name="height"/> window of <paramref name="pane"/> to
    /// (<paramref name="x"/> - <paramref name="offsetX"/>, <paramref name="y"/> - <paramref name="offsetY"/>); the
    /// depth is -<paramref name="y"/>.
    /// </summary>
    MCPaneElement(MCPane* pane, int32_t x, int32_t y, int32_t offsetX, int32_t offsetY, int32_t width, int32_t height);

    void Draw() override;

    /// <summary>The pane whose window is copied.</summary>
    MCPane* ShapePane;
    int32_t X;
    int32_t Y;
    int32_t OffsetX;
    int32_t OffsetY;
    int32_t Width;
    int32_t Height;
};

/// <summary>
/// A shape drawn as two frames of one shape table: frame 0 (the base) then frame <see cref="FrameNum"/> + 1 (the
/// delta over it). Used by the actors for shapes stored as base plus changes.
/// </summary>
/// <remarks>
/// Original source: <c>engine\cepane.cpp</c> (its line tables end in that file), 0x2c bytes. The constructor takes
/// the same arguments as <see cref="MCVfxElement"/>'s; only the shape, frame, position and fade table are used.
/// </remarks>
class MCDeltaElement : public MCElement
{
public:
    MCDeltaElement(uint8_t* shape, int32_t x, int32_t y, int32_t frame, int reverse, uint8_t* fadeTbl, int noScaleDraw,
                   int scaleUp);

    void Draw() override;

    /// <summary>The shape table.</summary>
    uint8_t* ShapeTable;
    /// <summary>The delta frame, less one.</summary>
    int32_t FrameNum;
    int32_t X;
    int32_t Y;
    /// <summary>Stored, unused by <see cref="Draw"/>.</summary>
    int Reverse;
    /// <summary>The fade table the shape is translated through, or null.</summary>
    uint8_t* FadeTable;
    /// <summary>Stored, unused by <see cref="Draw"/>.</summary>
    int NoScaleDraw;
    /// <summary>Stored, unused by <see cref="Draw"/>.</summary>
    int ScaleUp;
};
