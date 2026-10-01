#include "stdafx.h"
#include "lib/lzdecomp.h"

namespace
{
    constexpr uint32_t ClearCode = 0x100;
    constexpr uint32_t EndCode = 0x101;
    constexpr uint32_t FirstFree = 0x102;
    constexpr uint32_t MinBits = 9;
    constexpr uint32_t MaxBits = 12;

    /// <summary>One dictionary entry: the code of the string's prefix and its last byte (the original's HashStruct).</summary>
    struct HashStruct
    {
        uint16_t chain;
        uint8_t suffix;
    };
}

int32_t LZDecomp(uint8_t* dest, const uint8_t* src, uint32_t srcLen, uint32_t destLen)
{
    // The original reads each code as an unaligned 32-bit load, so it stops 3 bytes before the end of the source.
    if (srcLen < 3)
    {
        return 0;
    }

    const uint8_t* srcEnd = src + (srcLen - 3);
    const uint8_t* srcLimit = src + srcLen;

    static HashStruct hashBuffer[1 << MaxBits];
    std::array<uint8_t, 1 << MaxBits> stack;

    uint32_t codeMask = (1u << MinBits) - 1;
    uint32_t maxIndex = 1u << MinBits;
    uint32_t freeIndex = FirstFree;
    uint32_t bits = MinBits;
    uint32_t bitOffset = 0;
    uint32_t oldChain = 0;
    uint8_t oldSuffix = 0;
    uint32_t written = 0;

    auto readCode = [&](uint32_t width) -> uint32_t
    {
        // Port fix: the original's 32-bit load can read a byte past the end of the source.
        uint32_t word = 0;
        std::memcpy(&word, src, static_cast<size_t>(std::min<ptrdiff_t>(4, srcLimit - src)));
        const uint32_t code = (word >> bitOffset) & ((1u << width) - 1);
        bitOffset += width;
        src += bitOffset >> 3;
        bitOffset &= 7;
        return code;
    };

    auto put = [&](uint8_t value)
    {
        if (written < destLen)
        {
            dest[written] = value;
        }

        ++written;
    };

    while (src <= srcEnd)
    {
        const uint32_t code = readCode(bits) & codeMask;

        if (code == EndCode)
        {
            break;
        }

        if (code == ClearCode)
        {
            codeMask = (1u << MinBits) - 1;
            maxIndex = 1u << MinBits;
            freeIndex = FirstFree;
            bits = MinBits;

            if (src > srcEnd)
            {
                break;
            }

            oldChain = readCode(MinBits);
            oldSuffix = static_cast<uint8_t>(oldChain);
            put(oldSuffix);
            continue;
        }

        uint32_t walk = code;
        size_t depth = 0;

        if (code >= freeIndex)
        {
            // The code being defined right now (the KwKwK case): the previous string plus its own first byte.
            hashBuffer[code].suffix = oldSuffix;
            hashBuffer[code].chain = static_cast<uint16_t>(oldChain);
        }
        while (walk > 0xff && depth < stack.size())
        {
            stack[depth++] = hashBuffer[walk].suffix;
            walk = hashBuffer[walk].chain;
        }

        oldSuffix = static_cast<uint8_t>(walk);
        put(oldSuffix);

        while (depth > 0)
        {
            put(stack[--depth]);
        }

        if (freeIndex < (1u << MaxBits))
        {
            hashBuffer[freeIndex].suffix = oldSuffix;
            hashBuffer[freeIndex].chain = static_cast<uint16_t>(oldChain);
            ++freeIndex;
        }

        oldChain = code;

        if (freeIndex >= maxIndex && bits != MaxBits)
        {
            ++bits;
            maxIndex *= 2;
            codeMask = codeMask * 2 | 1;
        }
    }

    return static_cast<int32_t>(std::min(written, destLen));
}
