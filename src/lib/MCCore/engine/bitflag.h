#pragma once

class HeapManager;

/// <summary>A grid of 1-bit flags (rows x columns), in its own heap.</summary>
/// <remarks>Original source: <c>engine\bitflag.cpp</c>, 0x24 bytes.</remarks>
class BitFlag
{
public:
    /// <summary>
    /// Creates a <paramref name="numRows"/> x <paramref name="numColumns"/> grid, every flag set when
    /// <paramref name="initialValue"/> is nonzero.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00644310</remarks>
    int32_t init(uint32_t numRows, uint32_t numColumns, uint32_t initialValue);

    /// <summary>Sets every flag (<paramref name="value"/> nonzero) or clears them.</summary>
    /// <remarks>MCX.EXE @ 0x006443a0</remarks>
    void resetAll(uint32_t value);

    /// <summary>Frees the grid.</summary>
    /// <remarks>MCX.EXE @ 0x00644400</remarks>
    void destroy();

    /// <summary>Sets the flag at (<paramref name="r"/>, <paramref name="c"/>).</summary>
    /// <remarks>MCX.EXE @ 0x00644440</remarks>
    void setFlag(uint32_t r, uint32_t c);

    /// <summary>Sets <paramref name="length"/> flags of row <paramref name="r"/> from column <paramref name="c"/>.</summary>
    /// <remarks>
    /// MCX.EXE @ 0x00644490 (FUN_00644490: no symbol; named after <see cref="ByteFlag::setGroup"/>, its twin).
    /// </remarks>
    void setGroup(uint32_t r, uint32_t c, uint32_t length);

    /// <summary>The flag at (<paramref name="r"/>, <paramref name="c"/>) (0 outside the grid).</summary>
    /// <remarks>MCX.EXE @ 0x00644580</remarks>
    uint8_t getFlag(uint32_t r, uint32_t c);

    /// <summary>The bits.</summary>
    HeapManager* flagHeap; // +0x00
    /// <summary>Cleared by destroy; never otherwise used.</summary>
    uint8_t unknown04; // +0x04
    /// <summary>The number of rows.</summary>
    uint32_t rows; // +0x08
    /// <summary>The number of columns.</summary>
    uint32_t columns; // +0x0c
    /// <summary>The mask of one flag (1).</summary>
    uint8_t maskValue; // +0x10
    /// <summary>Flags per byte (8).</summary>
    uint32_t divValue; // +0x14
    /// <summary>Bytes per row.</summary>
    uint32_t colWidth; // +0x18
    /// <summary>The number of flags.</summary>
    uint32_t totalFlags; // +0x1c
    /// <summary>The grid's size in bytes.</summary>
    uint32_t totalRAM; // +0x20
};

/// <summary>
/// A grid of byte flags (0 or 0xFF), in its own heap, with a VFX window and pane over it so shapes (circles) can be
/// drawn into it. The terrain keeps its visibility this way.
/// </summary>
/// <remarks>Original source: <c>engine\bitflag.cpp</c>, 0x1c bytes.</remarks>
class ByteFlag
{
public:
    /// <summary>
    /// Creates a <paramref name="numRows"/> x <paramref name="numColumns"/> grid (all 0xFF when
    /// <paramref name="initialValue"/> is nonzero) and its window and pane.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006445e0</remarks>
    int32_t init(uint32_t numRows, uint32_t numColumns, uint32_t initialValue);

    /// <summary>Sets the flags in a filled circle of <paramref name="radius"/> at (<paramref name="x"/>, <paramref name="y"/>).</summary>
    /// <remarks>MCX.EXE @ 0x006446b0</remarks>
    void setCircle(uint32_t x, uint32_t y, uint32_t radius);

    /// <summary>Sets every flag (<paramref name="value"/> nonzero) or clears them.</summary>
    /// <remarks>MCX.EXE @ 0x006446e0</remarks>
    void resetAll(uint32_t value);

    /// <summary>Frees the grid, its window and pane.</summary>
    /// <remarks>MCX.EXE @ 0x00644730</remarks>
    void destroy();

    /// <summary>Sets the flag at (<paramref name="r"/>, <paramref name="c"/>).</summary>
    /// <remarks>MCX.EXE @ 0x00644780</remarks>
    void setFlag(uint32_t r, uint32_t c);

    /// <summary>Sets <paramref name="length"/> flags from (<paramref name="r"/>, <paramref name="c"/>).</summary>
    /// <remarks>MCX.EXE @ 0x006447c0</remarks>
    void setGroup(uint32_t r, uint32_t c, uint32_t length);

    /// <summary>Whether the flag at (<paramref name="r"/>, <paramref name="c"/>) is set (0 outside the grid).</summary>
    /// <remarks>MCX.EXE @ 0x00644830</remarks>
    uint8_t getFlag(uint32_t r, uint32_t c);

    /// <summary>The bytes.</summary>
    HeapManager* flagHeap; // +0x00
    /// <summary>The number of rows.</summary>
    uint32_t rows; // +0x04
    /// <summary>The number of columns.</summary>
    uint32_t columns; // +0x08
    /// <summary>The number of flags.</summary>
    uint32_t totalFlags; // +0x0c
    /// <summary>The grid's size in bytes.</summary>
    uint32_t totalRAM; // +0x10
    /// <summary>A pane over the whole grid (0x14 bytes in the original).</summary>
    _pane* flagPane; // +0x14
    /// <summary>The window over the grid's bytes (0x14 bytes allocated in the original).</summary>
    _window* flagWindow; // +0x18
};
