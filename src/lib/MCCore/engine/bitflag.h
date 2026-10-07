#pragma once

/// <summary>A grid of 1-bit flags (rows x columns), in its own buffer.</summary>
/// <remarks>Original source: <c>engine\bitflag.cpp</c>, 0x24 bytes.</remarks>
class MCBitFlag
{
public:
    /// <summary>
    /// Creates a <paramref name="numRows"/> x <paramref name="numColumns"/> grid, every flag set when
    /// <paramref name="initialValue"/> is nonzero.
    /// </summary>
    int32_t Init(uint32_t numRows, uint32_t numColumns, uint32_t initialValue);

    /// <summary>Sets every flag (<paramref name="value"/> nonzero) or clears them.</summary>
    void ResetAll(uint32_t value);

    /// <summary>Frees the grid.</summary>
    void Destroy();

    /// <summary>Sets the flag at (<paramref name="r"/>, <paramref name="c"/>).</summary>
    void SetFlag(uint32_t r, uint32_t c);

    /// <summary>Sets <paramref name="length"/> flags of row <paramref name="r"/> from column <paramref name="c"/>.</summary>
    /// <remarks>The twin of <see cref="MCByteFlag::SetGroup"/>.</remarks>
    void SetGroup(uint32_t r, uint32_t c, uint32_t length);

    /// <summary>The flag at (<paramref name="r"/>, <paramref name="c"/>) (0 outside the grid).</summary>
    uint8_t GetFlag(uint32_t r, uint32_t c);

    /// <summary>The bits (one byte more than <see cref="TotalRam"/>).</summary>
    std::vector<uint8_t> FlagData;
    /// <summary>The number of rows.</summary>
    uint32_t Rows;
    /// <summary>The number of columns.</summary>
    uint32_t Columns;
    /// <summary>The mask of one flag (1).</summary>
    uint8_t MaskValue;
    /// <summary>Flags per byte (8).</summary>
    uint32_t DivValue;
    /// <summary>Bytes per row.</summary>
    uint32_t ColWidth;
    /// <summary>The number of flags.</summary>
    uint32_t TotalFlags;
    /// <summary>The grid's size in bytes.</summary>
    uint32_t TotalRam;
};

/// <summary>
/// A grid of byte flags (0 or 0xFF), in its own buffer, with a VFX window and pane over it so shapes (circles) can be
/// drawn into it. The terrain keeps its visibility this way.
/// </summary>
/// <remarks>Original source: <c>engine\bitflag.cpp</c>, 0x1c bytes.</remarks>
class MCByteFlag
{
public:
    /// <summary>
    /// Creates a <paramref name="numRows"/> x <paramref name="numColumns"/> grid (all 0xFF when
    /// <paramref name="initialValue"/> is nonzero) and its window and pane.
    /// </summary>
    int32_t Init(uint32_t numRows, uint32_t numColumns, uint32_t initialValue);

    /// <summary>Sets the flags in a filled circle of <paramref name="radius"/> at (<paramref name="x"/>, <paramref name="y"/>).</summary>
    void SetCircle(uint32_t x, uint32_t y, uint32_t radius);

    /// <summary>Sets every flag (<paramref name="value"/> nonzero) or clears them.</summary>
    void ResetAll(uint32_t value);

    /// <summary>Frees the grid, its window and pane.</summary>
    void Destroy();

    /// <summary>Sets the flag at (<paramref name="r"/>, <paramref name="c"/>).</summary>
    void SetFlag(uint32_t r, uint32_t c);

    /// <summary>Sets <paramref name="length"/> flags from (<paramref name="r"/>, <paramref name="c"/>).</summary>
    void SetGroup(uint32_t r, uint32_t c, uint32_t length);

    /// <summary>Whether the flag at (<paramref name="r"/>, <paramref name="c"/>) is set (0 outside the grid).</summary>
    uint8_t GetFlag(uint32_t r, uint32_t c);

    /// <summary>
    /// Port-only: sets <paramref name="count"/> bytes of the grid from byte <paramref name="first"/> (rows run on into
    /// the next) to 0xFF, through the renderer.
    /// </summary>
    void SetBytes(uint32_t first, uint32_t count);

    /// <summary>The bytes (one more than <see cref="TotalRam"/>).</summary>
    std::vector<uint8_t> FlagData;
    /// <summary>The number of rows.</summary>
    uint32_t Rows;
    /// <summary>The number of columns.</summary>
    uint32_t Columns;
    /// <summary>The number of flags.</summary>
    uint32_t TotalFlags;
    /// <summary>The grid's size in bytes.</summary>
    uint32_t TotalRam;
    /// <summary>A pane over the whole grid (0x14 bytes in the original).</summary>
    MCPane* FlagPane;
    /// <summary>The window over the grid's bytes (0x14 bytes allocated in the original).</summary>
    MCWindow* FlagWindow;
};
