#include "stdafx.h"
#include "lib/lzcomp.h"

namespace
{
    constexpr uint32_t ClearCode = 0x100;
    constexpr uint32_t EndCode = 0x101;
    constexpr uint32_t FirstFree = 0x102;
    constexpr uint32_t TableSize = 0x1000;

    /// <summary>Writes codes LSB-first, and tracks the code width the decoder will read each one at.</summary>
    class CodeWriter
    {
    public:
        explicit CodeWriter(uint8_t* out) : _Out(out) {}

        /// <summary>Emits a code at the decoder's current width, then advances the decoder's state.</summary>
        void Emit(uint32_t code)
        {
            Put(code, _Bits);

            if (code == ClearCode)
            {
                _Bits = 9;
                _Free = FirstFree;
                _Max = 0x200;
                _FirstAfterClear = true;
                return;
            }

            if (code == EndCode)
            {
                return;
            }

            if (_FirstAfterClear)
            {
                _FirstAfterClear = false;
                return;
            }

            // The decoder adds one entry per code after the first, and widens when it reaches its limit.
            ++_Free;

            if (_Free >= _Max && _Bits != 12)
            {
                ++_Bits;
                _Max *= 2;
            }
        }

        /// <summary>Bytes written, counting a partly filled last byte.</summary>
        int32_t Length() const { return static_cast<int32_t>(_Pos + (_BitOffset != 0 ? 1 : 0)); }

    private:
        void Put(uint32_t code, uint32_t width)
        {
            for (uint32_t i = 0; i < width; ++i)
            {
                if (_BitOffset == 0)
                {
                    _Out[_Pos] = 0;
                }

                if (code & (1u << i))
                {
                    _Out[_Pos] |= static_cast<uint8_t>(1u << _BitOffset);
                }

                if (++_BitOffset == 8)
                {
                    _BitOffset = 0;
                    ++_Pos;
                }
            }
        }

        uint8_t* _Out;
        size_t _Pos = 0;
        uint32_t _BitOffset = 0;
        uint32_t _Bits = 9;
        uint32_t _Free = FirstFree;
        uint32_t _Max = 0x200;
        bool _FirstAfterClear = true;
    };
}

int32_t LZCompress(uint8_t* dest, const uint8_t* src, uint32_t srcLen)
{
    // Dictionary: (prefix code << 8 | byte) -> code.
    std::unordered_map<uint32_t, uint16_t> dictionary;
    dictionary.reserve(TableSize * 2);
    uint32_t freeCode = FirstFree;

    CodeWriter writer(dest);
    writer.Emit(ClearCode);

    if (srcLen == 0)
    {
        writer.Emit(EndCode);
        return writer.Length();
    }

    uint32_t prefix = src[0];

    for (uint32_t i = 1; i < srcLen; ++i)
    {
        const uint8_t k = src[i];
        const uint32_t key = prefix << 8 | k;
        const auto found = dictionary.find(key);

        if (found != dictionary.end())
        {
            prefix = found->second;
            continue;
        }

        writer.Emit(prefix);
        dictionary.emplace(key, static_cast<uint16_t>(freeCode));
        ++freeCode;
        prefix = k;

        if (freeCode == TableSize)
        {
            // Full: start over. The next code is a single byte, which the decoder reads as the literal after a clear.
            writer.Emit(ClearCode);
            dictionary.clear();
            freeCode = FirstFree;
        }
    }

    writer.Emit(prefix);
    writer.Emit(EndCode);
    // Padding so the decoder's end test (it stops three bytes before the end) always reaches the last code.
    int32_t length = writer.Length();

    for (int i = 0; i < 3; ++i)
    {
        dest[length++] = 0;
    }

    return length;
}
