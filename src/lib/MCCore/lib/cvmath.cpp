#include "stdafx.h"
#include "lib/cvmath.h"

// Set by the original's static initialisers.
MCVector3D UnitX(1.0f, 0.0f, 0.0f);
MCVector3D UnitY(0.0f, 1.0f, 0.0f);
MCVector3D UnitZ(0.0f, 0.0f, 1.0f);
MCVector3D NullVector3d(0.0f, 0.0f, 0.0f);
MCFrameOfRef NullFrameOfRef(MCVector3D(1.0f, 0.0f, 0.0f), MCVector3D(0.0f, 1.0f, 0.0f), MCVector3D(0.0f, 0.0f, 1.0f));

namespace
{
    /// <summary>The 32-bit product the original's imul gave (wrapping), shifted right arithmetically.</summary>
    int32_t ScaleRand(int32_t roll, int32_t range)
    {
        const int32_t product = static_cast<int32_t>(static_cast<uint32_t>(roll) * static_cast<uint32_t>(range));
        return product >> 15;
    }
}

int32_t RandomNumber(int32_t range)
{
    return ScaleRand(MCPort::Rand(), range);
}

int RollDice(int32_t percent)
{
    return ScaleRand(MCPort::Rand(), 100) < percent ? 1 : 0;
}

int32_t SignedRandomNumber(int32_t range)
{
    return RandomNumber(range * 2) - range;
}
