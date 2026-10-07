#include "stdafx.h"
#include "lib/MCLz.h"

namespace
{
    constexpr uint32_t ClearCode = 0x100;
    constexpr uint32_t EndCode = 0x101;
    constexpr uint32_t FirstFree = 0x102;
    constexpr uint32_t TableSize = 0x1000;

    /// <summary>Writes codes LSB-first, and tracks the code width the decoder will read each one at.</summary>
    class MCCodeWriter
    {
    public:
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

        /// <summary>The bytes written (a partly filled last byte included), taken out of the writer.</summary>
        std::vector<uint8_t> Take() { return std::move(_Out); }

    private:
        void Put(uint32_t code, uint32_t width)
        {
            for (uint32_t i = 0; i < width; ++i)
            {
                if (_BitOffset == 0)
                {
                    _Out.push_back(0);
                }

                if (code & (1u << i))
                {
                    _Out.back() |= static_cast<uint8_t>(1u << _BitOffset);
                }

                _BitOffset = (_BitOffset + 1) & 7;
            }
        }

        std::vector<uint8_t> _Out;
        uint32_t _BitOffset = 0;
        uint32_t _Bits = 9;
        uint32_t _Free = FirstFree;
        uint32_t _Max = 0x200;
        bool _FirstAfterClear = true;
    };
}

std::vector<uint8_t> LZCompress(std::span<const uint8_t> data)
{
    // Dictionary: (prefix code << 8 | byte) -> code.
    std::unordered_map<uint32_t, uint16_t> dictionary;
    dictionary.reserve(TableSize * 2);
    uint32_t freeCode = FirstFree;

    MCCodeWriter writer;
    writer.Emit(ClearCode);

    if (data.empty())
    {
        // No padding here, as the original's encoder.
        writer.Emit(EndCode);
        return writer.Take();
    }

    uint32_t prefix = data[0];

    for (const uint8_t k : data.subspan(1))
    {
        const uint32_t key = prefix << 8 | k;

        if (const auto found = dictionary.find(key); found != dictionary.end())
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
            // Full: start over. The next code is a single byte, which the decoder reads as the literal after a
            // clear.
            writer.Emit(ClearCode);
            dictionary.clear();
            freeCode = FirstFree;
        }
    }

    writer.Emit(prefix);
    writer.Emit(EndCode);
    std::vector<uint8_t> packed = writer.Take();
    // Padding so the decoder's end test (it stops three bytes before the end) always reaches the last code.
    packed.resize(packed.size() + 3, 0);
    return packed;
}
