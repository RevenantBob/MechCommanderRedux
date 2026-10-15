#pragma once

/// <summary>A point or direction in world space, in meters (x east, y north, z up).</summary>
/// <remarks>
/// Original header: <c>lib\cvmath.h</c>. The editor's code only stores, copies and passes it; the arithmetic of the
/// game's class isn't used, so only the fields and the zeroing are reconstructed.
/// </remarks>
struct vector_3d
{
    float x;
    float y;
    float z;

    /// <summary>Sets all three components to 0.</summary>
    void zero()
    {
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
    }
};
