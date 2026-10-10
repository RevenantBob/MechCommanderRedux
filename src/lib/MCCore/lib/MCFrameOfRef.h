#pragma once

#include "lib/MCVector3D.h"

/// <summary>acos of <paramref name="cosine"/>, or 0 above 1 and pi below -1 (the original's _matherr handler).</summary>
inline double AcosMatherr(double cosine)
{
    if (cosine >= -1.0 && cosine <= 1.0)
    {
        return std::acos(cosine);
    }

    return cosine >= 1.0 ? 0.0 : 3.14159265359;
}

/// <summary>An orientation: three orthonormal axes.</summary>
class MCFrameOfRef
{
public:
    MCFrameOfRef() = default;
    constexpr MCFrameOfRef(const MCVector3D& newI, const MCVector3D& newJ, const MCVector3D& newK)
        : I(newI), J(newJ), K(newK)
    {
    }

    /// <summary>Makes the axes the world's (UnitX, UnitY, UnitZ).</summary>
    MCFrameOfRef& ResetToWorldFrame()
    {
        I = UnitX;
        J = UnitY;
        K = UnitZ;
        return *this;
    }

    /// <summary>acos of <paramref name="cosine"/> clamped to [-1, 1].</summary>
    static double MyAcos(float cosine)
    {
        if (cosine < -1.0)
        {
            cosine = -1.0f;
        }

        if (cosine > 1.0)
        {
            cosine = 1.0f;
        }

        return AcosMatherr(static_cast<double>(cosine));
    }

    MCFrameOfRef& operator=(const MCFrameOfRef&) = default;
    MCFrameOfRef(const MCFrameOfRef&) = default;

    MCVector3D I;
    MCVector3D J;
    MCVector3D K;
};

/// <summary>The world frame (UnitX, UnitY, UnitZ).</summary>
inline constexpr MCFrameOfRef NullFrameOfRef(UnitX, UnitY, UnitZ);
