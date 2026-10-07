#include "stdafx.h"
#include "engine/bitflag.h"
#include "platform/MCRenderer.h"
#include "vfx/vfx.h"
#include "vfx/vfxfuncs.h"

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

auto MCBitFlag::Init(uint32_t numRows, uint32_t numColumns, uint32_t initialValue) -> int32_t
{
    Rows = numRows;
    Columns = numColumns;
    const uint32_t size = (numRows * numColumns) >> 3;
    DivValue = 8;
    TotalFlags = numRows * numColumns;
    TotalRam = size;
    ColWidth = numColumns >> 3;
    // One byte more than the grid: setFlag and getFlag take column == columns, which reaches one byte past the last
    // row (the original's heap had page slack there).
    FlagData.assign(static_cast<size_t>(size) + 1, 0);
    ResetAll(initialValue);
    return 0;
}

auto MCBitFlag::ResetAll(uint32_t value) -> void
{
    if (value == 0)
    {
        std::memset(FlagData.data(), 0, TotalRam);
        MaskValue = 1;
        return;
    }

    MaskValue = 1;
    std::memset(FlagData.data(), 0xff, TotalRam);
}

auto MCBitFlag::Destroy() -> void
{
    FlagData = {};
    Columns = 0;
    Rows = 0;
    MaskValue = 0;
    DivValue = 1;
    ColWidth = 1;
}

auto MCBitFlag::SetFlag(uint32_t r, uint32_t c) -> void
{
    if (r < Rows && c <= Columns)
    {
        uint8_t* flags = FlagData.data();
        flags[(c >> 3) + ColWidth * r] |= static_cast<uint8_t>(1 << ((c % DivValue) & 0x1f));
    }
}

auto MCBitFlag::SetGroup(uint32_t r, uint32_t c, uint32_t length) -> void
{
    if (length == 0 || r >= Rows || c > Columns || r * c + length >= Columns * Rows)
    {
        return;
    }

    const uint8_t bit = static_cast<uint8_t>(c % DivValue);
    uint8_t* flags = FlagData.data() + ColWidth * r + (c >> 3);
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

auto MCBitFlag::GetFlag(uint32_t r, uint32_t c) -> uint8_t
{
    if (r < Rows && c <= Columns)
    {
        const uint8_t bit = static_cast<uint8_t>(c % DivValue);
        uint8_t* flags = FlagData.data();
        return static_cast<uint8_t>(
            (flags[(c >> 3) + ColWidth * r] & static_cast<uint8_t>(MaskValue << (bit & 0x1f))) >> (bit & 0x1f));
    }

    return 0;
}

auto MCByteFlag::Init(uint32_t numRows, uint32_t numColumns, uint32_t initialValue) -> int32_t
{
    Rows = numRows;
    const uint32_t size = numRows * numColumns;
    Columns = numColumns;
    TotalFlags = size;
    TotalRam = size;
    // One byte more than the grid: setFlag and getFlag take column == columns, which reaches one byte past the last
    // row (the original's heap had page slack there).
    FlagData.assign(static_cast<size_t>(size) + 1, 0);
    ResetAll(initialValue);
    FlagPane = new MCPane();
    FlagWindow = new MCWindow();
    FlagPane->X0 = 0;
    FlagPane->Y0 = 0;
    FlagPane->X1 = static_cast<int32_t>(numColumns);
    FlagPane->Y1 = static_cast<int32_t>(numRows);
    FlagPane->Window = FlagWindow;
    FlagWindow->Buffer = FlagData.data();
    FlagWindow->XMax = static_cast<int32_t>(numColumns - 1);
    FlagWindow->YMax = static_cast<int32_t>(numRows - 1);
    return 0;
}

auto MCByteFlag::SetCircle(uint32_t x, uint32_t y, uint32_t radius) -> void
{
    VfxEllipseFill(FlagPane, static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(radius),
                   static_cast<int32_t>(radius), 0xff);
}

auto MCByteFlag::ResetAll(uint32_t value) -> void
{
    // Port: once the window is made, through the renderer (the tactical map shows the visible bits from a copy of
    // them on the GPU, see TacticalMap::init).
    if (FlagWindow != nullptr)
    {
        MCRenderer::For(FlagWindow)
            .Clear(FlagWindow, MCRect{0, 0, FlagWindow->XMax, FlagWindow->YMax}, value == 0 ? 0 : 0xff);
        return;
    }

    if (value == 0)
    {
        std::memset(FlagData.data(), 0, TotalRam);
        return;
    }

    std::memset(FlagData.data(), 0xff, TotalRam);
}

auto MCByteFlag::Destroy() -> void
{
    FlagData = {};
    delete FlagPane;
    FlagPane = nullptr;
    delete FlagWindow;
    Columns = 0;
    Rows = 0;
    FlagPane = nullptr;
    FlagWindow = nullptr;
}

auto MCByteFlag::SetFlag(uint32_t r, uint32_t c) -> void
{
    if (r < Rows && c <= Columns)
    {
        SetBytes(Columns * r + c, 1);
    }
}

auto MCByteFlag::SetGroup(uint32_t r, uint32_t c, uint32_t length) -> void
{
    if (length != 0 && r < Rows && c <= Columns && r * c + length < Columns * Rows)
    {
        SetBytes(Columns * r + c, length);
    }
}

auto MCByteFlag::SetBytes(uint32_t first, uint32_t count) -> void
{
    // Each row's part goes through the renderer; what runs past the grid (column == columns on the last row, or a
    // group longer than the rest of the grid) went into the original's heap slack, which nothing reads but the extra
    // byte getFlag reaches; the rest is dropped.
    uint32_t done = 0;

    while (done < count && first + done < TotalRam)
    {
        const uint32_t at = first + done;
        const uint32_t row = at / Columns;
        const uint32_t column = at % Columns;
        const uint32_t length = std::min(count - done, Columns - column);
        const auto x = static_cast<int32_t>(column);
        const auto y = static_cast<int32_t>(row);
        MCRenderer::For(FlagWindow).Clear(FlagWindow, MCRect{x, y, x + static_cast<int32_t>(length) - 1, y}, 0xff);
        done += length;
    }

    if (done < count && first + done < FlagData.size())
    {
        std::memset(FlagData.data() + first + done, 0xff,
                    std::min<size_t>(count - done, FlagData.size() - first - done));
    }
}

auto MCByteFlag::GetFlag(uint32_t r, uint32_t c) -> uint8_t
{
    if (r < Rows && c <= Columns)
    {
        return FlagData.data()[Columns * r + c] == 0xff;
    }

    return 0;
}
