#pragma once

#include "engine/MCElement.h"

/// <summary>
/// One frame of a VFX shape table at a screen point: drawn as is, through a fade table, mirrored, or scaled to the
/// camera's zoom (<see cref="ScaleDraw"/>).
/// </summary>
/// <remarks>Original source: <c>engine\cevfx.cpp</c>. The depth is -y (lower on screen draws later).</remarks>
class MCVfxElement : public MCElement
{
public:
    /// <summary>
    /// Frame <paramref name="frame"/> (clamped to the table) of <paramref name="shape"/> at
    /// (<paramref name="x"/>, <paramref name="y"/>), mirrored by <paramref name="reverse"/>, translated through
    /// <paramref name="fadeTable"/> when it is set, at 1:1 when <paramref name="noScaleDraw"/>.
    /// </summary>
    MCVfxElement(uint8_t* shape, int32_t x, int32_t y, int32_t frame, int32_t reverse, uint8_t* fadeTable,
                 bool noScaleDraw);

    /// <summary>The same at a float position (rounded down).</summary>
    MCVfxElement(uint8_t* shape, float x, float y, int32_t frame, int32_t reverse, uint8_t* fadeTable,
                 bool noScaleDraw);

    /// <summary>
    /// Draws through <see cref="ScaleDraw"/> unless <see cref="NoScaleDraw"/>, else directly (mirrored when
    /// <see cref="Reverse"/>).
    /// </summary>
    void Draw() override;

    /// <summary>The shape table.</summary>
    uint8_t* ShapeTable = nullptr;
    /// <summary>The frame drawn.</summary>
    int32_t FrameNum = 0;
    /// <summary>The position on screen.</summary>
    int32_t X = 0;
    int32_t Y = 0;
    /// <summary>Nonzero to draw mirrored (passed to the transform draw).</summary>
    int32_t Reverse = 0;
    /// <summary>The fade table the shape is translated through, or null.</summary>
    uint8_t* FadeTable = nullptr;
    /// <summary>Whether to draw at 1:1 instead of through <see cref="ScaleDraw"/>.</summary>
    bool NoScaleDraw = false;

private:
    /// <summary>Clamps <see cref="FrameNum"/> to the table's last frame.</summary>
    void ClampFrame();
};
