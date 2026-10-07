#pragma once

#include "engine/MCElement.h"

/// <summary>
/// A shape drawn as two frames of one shape table: frame 0 (the base) then frame <see cref="FrameNum"/> + 1 (the
/// delta over it). The actors use it for animations stored as a base plus changes.
/// </summary>
/// <remarks>Original source: <c>engine\cepane.cpp</c> (its line tables end in that file). The depth is -y.</remarks>
class MCDeltaElement : public MCElement
{
public:
    /// <summary>
    /// Delta frame <paramref name="frame"/> of <paramref name="shape"/> at (<paramref name="x"/>, <paramref name="y"/>),
    /// translated through <paramref name="fadeTable"/> when it is set.
    /// </summary>
    MCDeltaElement(uint8_t* shape, int32_t x, int32_t y, int32_t frame, uint8_t* fadeTable);

    void Draw() override;

    /// <summary>The shape table.</summary>
    uint8_t* ShapeTable = nullptr;
    /// <summary>The delta frame, less one.</summary>
    int32_t FrameNum = 0;
    /// <summary>The position on screen.</summary>
    int32_t X = 0;
    int32_t Y = 0;
    /// <summary>The fade table the shape is translated through, or null.</summary>
    uint8_t* FadeTable = nullptr;
};
