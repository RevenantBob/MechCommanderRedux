#include "stdafx.h"
#include "lib/cvmath.h"

// Set by the original's static initialisers (MCX.EXE @ 0x00644a30..0x00644af0).
vector_3d UnitX(1.0f, 0.0f, 0.0f);
vector_3d UnitY(0.0f, 1.0f, 0.0f);
vector_3d UnitZ(0.0f, 0.0f, 1.0f);
vector_3d NULL_vector_3d(0.0f, 0.0f, 0.0f);
frame_of_ref NULL_frame_of_ref(vector_3d(1.0f, 0.0f, 0.0f), vector_3d(0.0f, 1.0f, 0.0f), vector_3d(0.0f, 0.0f, 1.0f));

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
