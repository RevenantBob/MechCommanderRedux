#include "stdafx.h"
#include "engine/bitflag.h"
#include "lib/routines.h"
#include "platform/MCRenderer.h"
#include "vfx/vfx.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>A byte mask of the low <paramref name="count"/> bits, built as the original does (count &lt; 1 gives 1).</summary>
    uint8_t lowBits(int32_t count)
    {
        uint8_t mask = 1;

        for (int32_t i = count - 1; i > 0; i--)
        {
            mask = static_cast<uint8_t>(mask * 2 + 1);
        }

        return mask;
    }
}

auto BitFlag::init(uint32_t numRows, uint32_t numColumns, uint32_t initialValue) -> int32_t
{
    rows = numRows;
    columns = numColumns;
    const uint32_t size = (numRows * numColumns) >> 3;
    divValue = 8;
    totalFlags = numRows * numColumns;
    totalRAM = size;
    colWidth = numColumns >> 3;
    // One byte more than the grid: setFlag and getFlag take column == columns, which reaches one byte past the last
    // row (the original's heap had page slack there).
    flagData.assign(static_cast<size_t>(size) + 1, 0);
    resetAll(initialValue);
    return 0;
}

auto BitFlag::resetAll(uint32_t value) -> void
{
    if (value == 0)
    {
        memclear(flagData.data(), static_cast<int>(totalRAM));
        maskValue = 1;
        return;
    }

    maskValue = 1;
    std::memset(flagData.data(), 0xff, totalRAM);
}

auto BitFlag::destroy() -> void
{
    flagData = {};
    columns = 0;
    rows = 0;
    maskValue = 0;
    divValue = 1;
    colWidth = 1;
}

auto BitFlag::setFlag(uint32_t r, uint32_t c) -> void
{
    if (r < rows && c <= columns)
    {
        uint8_t* flags = flagData.data();
        flags[(c >> 3) + colWidth * r] |= static_cast<uint8_t>(1 << ((c % divValue) & 0x1f));
    }
}

auto BitFlag::setGroup(uint32_t r, uint32_t c, uint32_t length) -> void
{
    if (length == 0 || r >= rows || c > columns || r * c + length >= columns * rows)
    {
        return;
    }

    const uint8_t bit = static_cast<uint8_t>(c % divValue);
    uint8_t* flags = flagData.data() + colWidth * r + (c >> 3);
    const int32_t firstBits = 8 - bit;

    if (static_cast<int32_t>(length) <= firstBits)
    {
        *flags |= static_cast<uint8_t>(lowBits(static_cast<int32_t>(length)) << bit);
        return;
    }

    if (bit != 0)
    {
        *flags |= static_cast<uint8_t>(lowBits(firstBits) << bit);
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
        flags[1] |= lowBits(static_cast<int32_t>(length));
    }
}

auto BitFlag::getFlag(uint32_t r, uint32_t c) -> uint8_t
{
    if (r < rows && c <= columns)
    {
        const uint8_t bit = static_cast<uint8_t>(c % divValue);
        uint8_t* flags = flagData.data();
        return static_cast<uint8_t>(
            (flags[(c >> 3) + colWidth * r] & static_cast<uint8_t>(maskValue << (bit & 0x1f))) >> (bit & 0x1f));
    }

    return 0;
}

auto ByteFlag::init(uint32_t numRows, uint32_t numColumns, uint32_t initialValue) -> int32_t
{
    rows = numRows;
    const uint32_t size = numRows * numColumns;
    columns = numColumns;
    totalFlags = size;
    totalRAM = size;
    // One byte more than the grid: setFlag and getFlag take column == columns, which reaches one byte past the last
    // row (the original's heap had page slack there).
    flagData.assign(static_cast<size_t>(size) + 1, 0);
    resetAll(initialValue);
    flagPane = new _pane();
    flagWindow = new _window();
    flagPane->x0 = 0;
    flagPane->y0 = 0;
    flagPane->x1 = static_cast<int32_t>(numColumns);
    flagPane->y1 = static_cast<int32_t>(numRows);
    flagPane->window = flagWindow;
    flagWindow->buffer = flagData.data();
    flagWindow->x_max = static_cast<int32_t>(numColumns - 1);
    flagWindow->y_max = static_cast<int32_t>(numRows - 1);
    return 0;
}

auto ByteFlag::setCircle(uint32_t x, uint32_t y, uint32_t radius) -> void
{
    VFX_ellipse_fill(flagPane, static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(radius),
                     static_cast<int32_t>(radius), 0xff);
}

auto ByteFlag::resetAll(uint32_t value) -> void
{
    // Port: once the window is made, through the renderer (the tactical map shows the visible bits from a copy of
    // them on the GPU, see TacticalMap::init).
    if (flagWindow != nullptr)
    {
        MCRenderer::For(flagWindow)
            .Clear(flagWindow, MCRect{0, 0, flagWindow->x_max, flagWindow->y_max}, value == 0 ? 0 : 0xff);
        return;
    }

    if (value == 0)
    {
        memclear(flagData.data(), static_cast<int>(totalRAM));
        return;
    }

    std::memset(flagData.data(), 0xff, totalRAM);
}

auto ByteFlag::destroy() -> void
{
    flagData = {};
    delete flagPane;
    flagPane = nullptr;
    delete flagWindow;
    columns = 0;
    rows = 0;
    flagPane = nullptr;
    flagWindow = nullptr;
}

auto ByteFlag::setFlag(uint32_t r, uint32_t c) -> void
{
    if (r < rows && c <= columns)
    {
        setBytes(columns * r + c, 1);
    }
}

auto ByteFlag::setGroup(uint32_t r, uint32_t c, uint32_t length) -> void
{
    if (length != 0 && r < rows && c <= columns && r * c + length < columns * rows)
    {
        setBytes(columns * r + c, length);
    }
}

auto ByteFlag::setBytes(uint32_t first, uint32_t count) -> void
{
    // Each row's part goes through the renderer; what runs past the grid (column == columns on the last row, or a
    // group longer than the rest of the grid) went into the original's heap slack, which nothing reads but the extra
    // byte getFlag reaches; the rest is dropped.
    uint32_t done = 0;

    while (done < count && first + done < totalRAM)
    {
        const uint32_t at = first + done;
        const uint32_t row = at / columns;
        const uint32_t column = at % columns;
        const uint32_t length = std::min(count - done, columns - column);
        const auto x = static_cast<int32_t>(column);
        const auto y = static_cast<int32_t>(row);
        MCRenderer::For(flagWindow).Clear(flagWindow, MCRect{x, y, x + static_cast<int32_t>(length) - 1, y}, 0xff);
        done += length;
    }

    if (done < count && first + done < flagData.size())
    {
        std::memset(flagData.data() + first + done, 0xff,
                    std::min<size_t>(count - done, flagData.size() - first - done));
    }
}

auto ByteFlag::getFlag(uint32_t r, uint32_t c) -> uint8_t
{
    if (r < rows && c <= columns)
    {
        return flagData.data()[columns * r + c] == 0xff;
    }

    return 0;
}
