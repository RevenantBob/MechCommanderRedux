#include "stdafx.h"
#include "lib/lzcomp.h"

namespace
{
    constexpr uint32_t CLEAR_CODE = 0x100;
    constexpr uint32_t END_CODE = 0x101;
    constexpr uint32_t FIRST_FREE = 0x102;
    constexpr uint32_t MAX_CODES = 0x1000;
    constexpr uint32_t MIN_BITS = 9;
    constexpr uint32_t MAX_BITS = 12;
    constexpr uint32_t NO_LINK = 0xffffffff;

    /// <summary>
    /// One dictionary entry: the first string that extends this one, the next string with the same prefix, and the
    /// string's last byte.
    /// </summary>
#pragma pack(push, 1)
    struct LZHashEntry
    {
        /// <summary>The first longer string with this prefix, or NO_LINK.</summary>
        uint32_t child;
        /// <summary>The next string with the same prefix, or NO_LINK.</summary>
        uint32_t sibling;
        /// <summary>The string's last byte.</summary>
        uint8_t suffix;
    };
#pragma pack(pop)
    static_assert(sizeof(LZHashEntry) == 9);

    /// <summary>The dictionary's allocation, in bytes.</summary>
    constexpr size_t TABLE_BYTES = 0xc400;

    /// <summary>The bytes a clear resets to NO_LINK: every single-byte string's links.</summary>
    constexpr size_t RESET_BYTES = 0xc00;

    /// <summary>Writes codes LSB first, three bytes at a time.</summary>
    class CodeWriter
    {
    public:
        /// <summary>Writes to <paramref name="out"/>.</summary>
        explicit CodeWriter(uint8_t* out) : _out(out) {}

        /// <summary>The current code width.</summary>
        uint32_t bits = MIN_BITS;

        /// <summary>
        /// Stores <paramref name="code"/> at the current bit: ORed into the partly filled byte, the next two bytes
        /// overwritten.
        /// </summary>
        void put(uint32_t code)
        {
            const uint32_t shifted = code << _bitOffset;
            _out[0] = static_cast<uint8_t>((_bitOffset != 0 ? _out[0] : 0) | static_cast<uint8_t>(shifted));
            _out[1] = static_cast<uint8_t>(shifted >> 8);
            _out[2] = static_cast<uint8_t>(shifted >> 16);
            const uint32_t end = _bitOffset + bits;
            _out += end >> 3;
            _bitOffset = end & 7;
        }

        /// <summary>Counts a partly filled last byte in.</summary>
        void flush()
        {
            if (_bitOffset != 0)
            {
                ++_out;
            }
        }

        /// <summary>Where the next whole byte goes.</summary>
        uint8_t* position() const { return _out; }

    private:
        uint8_t* _out;
        uint32_t _bitOffset = 0;
    };
}

int32_t LZCompress(uint8_t* dest, const uint8_t* src, int32_t srcLen)
{
    // The original kept the dictionary in a global and freed it at the end of every call.
    auto table = static_cast<LZHashEntry*>(std::malloc(TABLE_BYTES));

    if (table == nullptr)
    {
        return LZ_NO_RAM_FOR_TABLE;
    }

    const uint8_t* in = src;
    const uint8_t* end = src + srcLen;
    CodeWriter writer(dest);
    uint32_t maxCode = 1u << MIN_BITS;
    std::memset(table, 0xff, RESET_BYTES);
    uint32_t freeCode = FIRST_FREE;
    writer.put(CLEAR_CODE);

    // Original behaviour (EB-004): an empty source still gets one code, 1, left over in a register.
    uint32_t prefix = 1;

    if (in < end)
    {
        prefix = *in++;
    }

    for (;;)
    {
        const uint32_t freeBefore = freeCode;
        LZHashEntry* last = nullptr;
        bool viaSibling = false;
        uint8_t k = 0;

        // Extend the string while the dictionary has it.
        for (;;)
        {
            if (in >= end)
            {
                writer.put(prefix);
                writer.put(END_CODE);
                writer.flush();
                std::free(table);
                return static_cast<int32_t>(writer.position() - dest);
            }

            k = *in++;
            last = &table[prefix];
            viaSibling = false;

            if (last->child == NO_LINK)
            {
                break;
            }

            viaSibling = true;
            uint32_t code = last->child;
            bool found = false;

            for (;;)
            {
                last = &table[code];

                if (last->suffix == k)
                {
                    found = true;
                    break;
                }

                if (last->sibling == NO_LINK)
                {
                    break;
                }

                code = last->sibling;
            }

            if (!found)
            {
                break;
            }

            prefix = code;
        }

        // Original behaviour: the link is made even when the table is full; the clear below drops it.
        if (viaSibling)
        {
            last->sibling = freeCode;
        }
        else
        {
            last->child = freeCode;
        }

        if (freeCode != MAX_CODES)
        {
            table[freeCode].child = NO_LINK;
            table[freeCode].sibling = NO_LINK;
            table[freeCode].suffix = k;
            ++freeCode;
        }

        writer.put(prefix);
        prefix = k;

        if (freeBefore >= maxCode)
        {
            if (writer.bits < MAX_BITS)
            {
                ++writer.bits;
                maxCode <<= 1;
            }
            else
            {
                writer.put(CLEAR_CODE);
                writer.bits = MIN_BITS;
                maxCode = 1u << MIN_BITS;
                std::memset(table, 0xff, RESET_BYTES);
                freeCode = FIRST_FREE;
            }
        }
    }
}

size_t LZCompressBound(int32_t srcLen)
{
    // Codes: the first clear, at most one per input byte (one for an empty source), a clear per filled table, and
    // the end; at most 12 bits each, plus the two bytes the last store runs over.
    const size_t length = static_cast<size_t>((std::max)(srcLen, 1));
    const size_t codes = 1 + length + length / (MAX_CODES - FIRST_FREE) + 1 + 1;
    return (codes * MAX_BITS + 7) / 8 + 2;
}
