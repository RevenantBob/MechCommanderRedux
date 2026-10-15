#include "stdafx.h"
#include "lib/lzdecomp.h"

namespace
{
    constexpr uint32_t CLEAR_CODE = 0x100;
    constexpr uint32_t END_CODE = 0x101;
    constexpr uint32_t FIRST_FREE = 0x102;
    constexpr uint32_t MIN_BITS = 9;
    constexpr uint32_t MAX_BITS = 12;

    /// <summary>One dictionary entry: the code of the string's prefix and its last byte.</summary>
    struct HashStruct
    {
        /// <summary>The prefix's code.</summary>
        uint16_t chain;
        /// <summary>The string's last byte.</summary>
        uint8_t suffix;
    };

    /// <summary>The dictionary, kept between calls as in the original.</summary>
    HashStruct hashBuffer[1 << MAX_BITS];
}

int32_t LZDecomp(uint8_t* dest, const uint8_t* src, uint32_t srcLen)
{
    // The original reads each code as an unaligned 32-bit load, so it stops 3 bytes before the end of the source.
    if (srcLen < 3)
    {
        return 0;
    }

    const uint8_t* srcEnd = src + (srcLen - 3);
    const uint8_t* srcLimit = src + srcLen;
    uint8_t stack[1 << MAX_BITS];
    uint32_t codeMask = (1u << MIN_BITS) - 1;
    uint32_t maxIndex = 1u << MIN_BITS;
    uint32_t freeIndex = FIRST_FREE;
    uint32_t bits = MIN_BITS;
    uint32_t bitOffset = 0;
    uint32_t oldChain = 0;
    uint8_t oldSuffix = 0;
    uint8_t* out = dest;

    auto readCode = [&](uint32_t mask) -> uint32_t
    {
        // The original's 32-bit load can take one byte past the end of the source; read only what is there.
        uint32_t word = 0;
        std::memcpy(&word, src, static_cast<size_t>((std::min<ptrdiff_t>)(4, srcLimit - src)));
        const uint32_t code = (word >> bitOffset) & mask;
        bitOffset += bits;
        src += bitOffset >> 3;
        bitOffset &= 7;
        return code;
    };

    while (src <= srcEnd)
    {
        const uint32_t code = readCode(codeMask);

        if (code == END_CODE)
        {
            break;
        }

        if (code == CLEAR_CODE)
        {
            codeMask = (1u << MIN_BITS) - 1;
            maxIndex = 1u << MIN_BITS;
            freeIndex = FIRST_FREE;
            bits = MIN_BITS;

            if (src > srcEnd)
            {
                break;
            }

            oldChain = readCode(codeMask);
            oldSuffix = static_cast<uint8_t>(oldChain);
            *out++ = oldSuffix;
            continue;
        }

        if (code >= freeIndex)
        {
            // The code being defined right now (the KwKwK case): the previous string plus its own first byte.
            hashBuffer[code].suffix = oldSuffix;
            hashBuffer[code].chain = static_cast<uint16_t>(oldChain);
        }

        uint32_t walk = code;
        size_t depth = 0;

        while (walk > 0xff && depth < std::size(stack))
        {
            stack[depth++] = hashBuffer[walk].suffix;
            walk = hashBuffer[walk].chain;
        }

        oldSuffix = static_cast<uint8_t>(walk);
        *out++ = oldSuffix;

        while (depth > 0)
        {
            *out++ = stack[--depth];
        }

        // LZCompress clears before the table overflows; the check keeps damaged data inside it.
        if (freeIndex < std::size(hashBuffer))
        {
            hashBuffer[freeIndex].suffix = oldSuffix;
            hashBuffer[freeIndex].chain = static_cast<uint16_t>(oldChain);
            ++freeIndex;
        }

        oldChain = code;

        if (freeIndex >= maxIndex && bits != MAX_BITS)
        {
            ++bits;
            maxIndex *= 2;
            codeMask = codeMask * 2 | 1;
        }
    }

    return static_cast<int32_t>(out - dest);
}
