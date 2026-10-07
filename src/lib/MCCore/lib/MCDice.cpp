#include "stdafx.h"
#include "lib/MCDice.h"

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

bool RollDice(int32_t percent)
{
    return ScaleRand(MCPort::Rand(), 100) < percent;
}

int32_t SignedRandomNumber(int32_t range)
{
    return RandomNumber(range * 2) - range;
}
