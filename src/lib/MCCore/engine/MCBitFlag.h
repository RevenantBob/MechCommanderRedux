#pragma once

/// <summary>A grid of 1-bit flags (rows x columns), each row a whole number of bytes.</summary>
/// <remarks>Original source: <c>engine\bitflag.cpp</c>. The terrain keeps the tiles each side has seen this way.</remarks>
class MCBitFlag
{
public:
    /// <summary>
    /// A <paramref name="numRows"/> x <paramref name="numColumns"/> grid, every flag set when
    /// <paramref name="initialValue"/>.
    /// </summary>
    MCBitFlag(uint32_t numRows, uint32_t numColumns, bool initialValue);

    /// <summary>Sets every flag (<paramref name="value"/>) or clears them.</summary>
    void ResetAll(bool value);

    /// <summary>Sets the flag at (<paramref name="r"/>, <paramref name="c"/>).</summary>
    void SetFlag(uint32_t r, uint32_t c);

    /// <summary>Sets <paramref name="length"/> flags of row <paramref name="r"/> from column <paramref name="c"/>.</summary>
    void SetGroup(uint32_t r, uint32_t c, uint32_t length);

    /// <summary>The flag at (<paramref name="r"/>, <paramref name="c"/>) (false outside the grid).</summary>
    bool GetFlag(uint32_t r, uint32_t c) const;

    /// <summary>The number of rows.</summary>
    uint32_t Rows() const { return _Rows; }

    /// <summary>The number of columns.</summary>
    uint32_t Columns() const { return _Columns; }

    /// <summary>The grid's size in bytes.</summary>
    uint32_t ByteCount() const { return _ByteCount; }

    /// <summary>The bits (one byte more than <see cref="ByteCount"/>).</summary>
    std::span<const uint8_t> Data() const { return _Bits; }

private:
    /// <summary>The bits.</summary>
    /// <remarks>
    /// One byte more than the grid: <see cref="SetFlag"/> and <see cref="GetFlag"/> take column == columns, which
    /// reaches one byte past the last row (the original's heap had slack there).
    /// </remarks>
    std::vector<uint8_t> _Bits;
    /// <summary>The number of rows.</summary>
    uint32_t _Rows = 0;
    /// <summary>The number of columns.</summary>
    uint32_t _Columns = 0;
    /// <summary>Bytes per row (columns / 8).</summary>
    uint32_t _BytesPerRow = 0;
    /// <summary>The grid's size in bytes (rows * columns / 8).</summary>
    uint32_t _ByteCount = 0;
};
