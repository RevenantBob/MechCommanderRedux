#pragma once

/// <summary>
/// Draws frame <paramref name="frameNum"/> of <paramref name="shape"/> at (<paramref name="x"/>,
/// <paramref name="y"/>) into <c>GlobalPane</c> at the camera's zoom: directly at full size, else at half size (and
/// mirrored when <paramref name="reverse"/>), translated through <paramref name="fadeTable"/> when it is set.
/// </summary>
/// <returns>
/// The frame's bounds (width &lt;&lt; 16 | height), or -1 when the shape isn't drawn (not a shape table, empty, or
/// 401 pixels or more on a side).
/// </returns>
/// <remarks>Original source: <c>engine\scale.cpp</c>.</remarks>
int32_t ScaleDraw(uint8_t* shape, uint32_t frameNum, int32_t x, int32_t y, int32_t reverse, uint8_t* fadeTable);
