#include "stdafx.h"
#include "engine/MCBitFlag.h"

namespace
{
    /// <summary>A byte mask of the low <paramref name="count"/> bits, built as the original does (count &lt; 1 gives 1).</summary>
    uint8_t LowBits(int32_t count)
    {
        uint8_t mask = 1;

        for (int32_t i = count - 1; i > 0; i--)
        {
            mask = static_cast<uint8_t>(mask * 2 + 1);
        }

        return mask;
    }
}

MCBitFlag::MCBitFlag(uint32_t numRows, uint32_t numColumns, bool initialValue)
    : _Rows(numRows), _Columns(numColumns), _BytesPerRow(numColumns >> 3), _ByteCount((numRows * numColumns) >> 3)
{
    _Bits.assign(static_cast<size_t>(_ByteCount) + 1, 0);
    ResetAll(initialValue);
}

auto MCBitFlag::ResetAll(bool value) -> void
{
    std::fill_n(_Bits.begin(), _ByteCount, value ? uint8_t{0xff} : uint8_t{0});
}

auto MCBitFlag::SetFlag(uint32_t r, uint32_t c) -> void
{
    if (r < _Rows && c <= _Columns)
    {
        _Bits[(c >> 3) + _BytesPerRow * r] |= static_cast<uint8_t>(1 << (c % 8));
    }
}

auto MCBitFlag::SetGroup(uint32_t r, uint32_t c, uint32_t length) -> void
{
    if (length == 0 || r >= _Rows || c > _Columns || r * c + length >= _Columns * _Rows)
    {
        return;
    }

    const auto bit = static_cast<uint8_t>(c % 8);
    uint8_t* flags = _Bits.data() + _BytesPerRow * r + (c >> 3);
    const int32_t firstBits = 8 - bit;

    if (static_cast<int32_t>(length) <= firstBits)
    {
        *flags |= static_cast<uint8_t>(LowBits(static_cast<int32_t>(length)) << bit);
        return;
    }

    if (bit != 0)
    {
        *flags |= static_cast<uint8_t>(LowBits(firstBits) << bit);
        length -= static_cast<uint32_t>(firstBits);
    }

    // Original bug (OB-069): the whole bytes start one past `flags` even when the run is byte-aligned, so an aligned
    // run skips its first byte and sets one past its end. Nothing calls this function.
    if (length >= 8)
    {
        for (uint32_t count = length >> 3; count != 0; count--)
        {
            flags++;
            length -= 8;
            *flags = 0xff;
        }
    }

    if (length != 0)
    {
        flags[1] |= LowBits(static_cast<int32_t>(length));
    }
}

auto MCBitFlag::GetFlag(uint32_t r, uint32_t c) const -> bool
{
    if (r < _Rows && c <= _Columns)
    {
        return ((_Bits[(c >> 3) + _BytesPerRow * r] >> (c % 8)) & 1) != 0;
    }

    return false;
}
