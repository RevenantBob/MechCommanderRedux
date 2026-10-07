#include "stdafx.h"
#include "engine/MCByteFlag.h"
#include "platform/MCRenderer.h"
#include "vfx/MCVfxFunctions.h"

MCByteFlag::MCByteFlag(uint32_t numRows, uint32_t numColumns, bool initialValue) : _Rows(numRows), _Columns(numColumns)
{
    const uint32_t size = numRows * numColumns;
    _Bytes.assign(static_cast<size_t>(size) + 1, 0);
    std::fill_n(_Bytes.begin(), size, initialValue ? uint8_t{0xff} : uint8_t{0});
    _Window.Buffer = _Bytes.data();
    _Window.XMax = static_cast<int32_t>(numColumns - 1);
    _Window.YMax = static_cast<int32_t>(numRows - 1);
    _Pane.Window = &_Window;
    _Pane.X0 = 0;
    _Pane.Y0 = 0;
    _Pane.X1 = static_cast<int32_t>(numColumns);
    _Pane.Y1 = static_cast<int32_t>(numRows);
}

auto MCByteFlag::SetCircle(uint32_t x, uint32_t y, uint32_t radius) -> void
{
    VfxEllipseFill(&_Pane, static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(radius),
                   static_cast<int32_t>(radius), 0xff);
}

auto MCByteFlag::ResetAll(bool value) -> void
{
    MCRenderer::For(&_Window).Clear(&_Window, MCRect{0, 0, _Window.XMax, _Window.YMax}, value ? 0xff : 0);
}

auto MCByteFlag::SetFlag(uint32_t r, uint32_t c) -> void
{
    if (r < _Rows && c <= _Columns)
    {
        SetBytes(_Columns * r + c, 1);
    }
}

auto MCByteFlag::SetGroup(uint32_t r, uint32_t c, uint32_t length) -> void
{
    if (length != 0 && r < _Rows && c <= _Columns && r * c + length < _Columns * _Rows)
    {
        SetBytes(_Columns * r + c, length);
    }
}

auto MCByteFlag::SetBytes(uint32_t first, uint32_t count) -> void
{
    // Each row's part goes through the renderer; what runs past the grid (column == columns on the last row, or a
    // group longer than the rest of the grid) went into the original's heap slack, which nothing reads but the extra
    // byte GetFlag reaches; the rest is dropped.
    const uint32_t gridSize = _Rows * _Columns;
    uint32_t done = 0;

    while (done < count && first + done < gridSize)
    {
        const uint32_t at = first + done;
        const uint32_t length = std::min(count - done, _Columns - at % _Columns);
        const auto x = static_cast<int32_t>(at % _Columns);
        const auto y = static_cast<int32_t>(at / _Columns);
        MCRenderer::For(&_Window).Clear(&_Window, MCRect{x, y, x + static_cast<int32_t>(length) - 1, y}, 0xff);
        done += length;
    }

    if (done < count && first + done < _Bytes.size())
    {
        std::fill_n(_Bytes.begin() + first + done, std::min<size_t>(count - done, _Bytes.size() - first - done),
                    uint8_t{0xff});
    }
}

auto MCByteFlag::GetFlag(uint32_t r, uint32_t c) const -> bool
{
    if (r < _Rows && c <= _Columns)
    {
        return _Bytes[_Columns * r + c] == 0xff;
    }

    return false;
}
