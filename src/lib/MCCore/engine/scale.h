#pragma once

/// <summary>
/// Draws frame <paramref name="frameNum"/> of <paramref name="shape"/> at (<paramref name="x"/>,
/// <paramref name="y"/>) into <c>globalPane</c> at the camera's zoom: directly at full size, else scaled (and
/// mirrored when <paramref name="reverse"/>) through <c>tempBuffer</c>, translated through
/// <paramref name="fadeTable"/> when set. <paramref name="scaleUp"/> is recomputed from the camera.
/// </summary>
/// <returns>The frame's bounds (width &lt;&lt; 16 | height), or -1 when the shape isn't drawable (too big, empty).</returns>
/// <remarks>Fatal "Sprite too damned big" past 0x1fa40 pixels.</remarks>
int32_t ScaleDraw(uint8_t* shape, uint32_t frameNum, int32_t x, int32_t y, int reverse, uint8_t* fadeTable,
                  int scaleUp);
