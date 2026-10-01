#include "stdafx.h"
#include "engine/bitflag.h"
#include "lib/heap.h"
#include "lib/routines.h"
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
    flagHeap = new HeapManager();

    if (flagHeap == nullptr)
    {
        return -0x60000;
    }

    int32_t result = flagHeap->createHeap(size);

    if (result == 0)
    {
        result = flagHeap->commitHeap(0);

        if (result == 0)
        {
            resetAll(initialValue);
            result = 0;
        }
    }

    return result;
}

auto BitFlag::resetAll(uint32_t value) -> void
{
    if (value == 0)
    {
        memclear(flagHeap->getHeapPtr(), static_cast<int>(totalRAM));
        maskValue = 1;
        return;
    }

    maskValue = 1;
    std::memset(flagHeap->getHeapPtr(), 0xff, totalRAM);
}

auto BitFlag::destroy() -> void
{
    delete flagHeap;
    flagHeap = nullptr;
    unknown04 = 0;
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
        uint8_t* flags = flagHeap->getHeapPtr();
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
    uint8_t* flags = flagHeap->getHeapPtr() + colWidth * r + (c >> 3);
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
        uint8_t* flags = flagHeap->getHeapPtr();
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
    flagHeap = new HeapManager();

    if (flagHeap == nullptr)
    {
        return -0x60000;
    }

    int32_t result = flagHeap->createHeap(size);

    if (result == 0)
    {
        result = flagHeap->commitHeap(0);

        if (result == 0)
        {
            resetAll(initialValue);
            flagPane = new _pane();
            flagWindow = new _window();
            flagPane->x0 = 0;
            flagPane->y0 = 0;
            flagPane->x1 = static_cast<int32_t>(numColumns);
            flagPane->y1 = static_cast<int32_t>(numRows);
            flagPane->window = flagWindow;
            flagWindow->buffer = flagHeap->getHeapPtr();
            flagWindow->x_max = static_cast<int32_t>(numColumns - 1);
            result = 0;
            flagWindow->y_max = static_cast<int32_t>(numRows - 1);
        }
    }

    return result;
}

auto ByteFlag::setCircle(uint32_t x, uint32_t y, uint32_t radius) -> void
{
    VFX_ellipse_fill(flagPane, static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(radius),
                     static_cast<int32_t>(radius), 0xff);
}

auto ByteFlag::resetAll(uint32_t value) -> void
{
    if (value == 0)
    {
        memclear(flagHeap->getHeapPtr(), static_cast<int>(totalRAM));
        return;
    }

    std::memset(flagHeap->getHeapPtr(), 0xff, totalRAM);
}

auto ByteFlag::destroy() -> void
{
    delete flagHeap;
    flagHeap = nullptr;
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
        flagHeap->getHeapPtr()[columns * r + c] = 0xff;
    }
}

auto ByteFlag::setGroup(uint32_t r, uint32_t c, uint32_t length) -> void
{
    if (length != 0 && r < rows && c <= columns && r * c + length < columns * rows)
    {
        std::memset(flagHeap->getHeapPtr() + columns * r + c, 0xff, length);
    }
}

auto ByteFlag::getFlag(uint32_t r, uint32_t c) -> uint8_t
{
    if (r < rows && c <= columns)
    {
        return flagHeap->getHeapPtr()[columns * r + c] == 0xff;
    }

    return 0;
}
