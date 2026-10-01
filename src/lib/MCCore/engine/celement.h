#pragma once

class HeapManager;

/// <summary>
/// Something to draw this frame: a line, a shape, a polygon, a string... Appearances, the terrain and the interface
/// add elements to <c>ElementList</c>; it sorts them by <see cref="depth"/> and calls <see cref="draw"/> on each.
/// </summary>
/// <remarks>
/// Original source: <c>engine\celement.cpp</c>, 0xc bytes. Elements are allocated from <see cref="ElementPool"/>,
/// a stack emptied every frame, and are never deleted one by one (they have no destructor).
/// </remarks>
class Element
{
public:
    /// <summary>An element at depth <paramref name="_depth"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006b18d0</remarks>
    Element(int32_t _depth);
    /// <summary>An element at depth <paramref name="_depth"/>, rounded down to a whole number.</summary>
    /// <remarks>MCX.EXE @ 0x006b18f0</remarks>
    Element(float _depth);

    /// <summary>Allocates from <see cref="ElementPool"/>; fatal (0xEEEB0003) when the pool is exhausted.</summary>
    /// <remarks>MCX.EXE @ 0x006b1930</remarks>
    static void* operator new(size_t size) noexcept;
    /// <summary>Elements go when the pool is reset, never one by one.</summary>
    static void operator delete(void*) {}

    /// <summary>Draws the element into <c>globalPane</c>.</summary>
    /// <remarks>Pure virtual (slot 0 is <c>_purecall</c>).</remarks>
    virtual void draw() = 0; // slot 0

    /// <summary>The sort key: elements draw from low to high.</summary>
    float depth; // +0x04
    /// <summary>Set to 1 by the constructors; never read in MCX.EXE.</summary>
    int32_t unknown08; // +0x08
};

/// <summary>The per-frame stack the elements are allocated from (top-down from the end of its heap).</summary>
/// <remarks>Original source: <c>engine\celement.cpp</c>; static members only.</remarks>
class ElementPool
{
public:
    /// <summary>Empties the pool (every element of the last frame goes).</summary>
    /// <remarks>MCX.EXE @ 0x006b19a0</remarks>
    static void reset();
    /// <summary>Creates the pool's heap of <paramref name="poolSize"/> bytes.</summary>
    /// <remarks>MCX.EXE @ 0x006b19c0</remarks>
    static int32_t init(int32_t poolSize);
    /// <summary>Frees the pool's heap.</summary>
    /// <remarks>MCX.EXE @ 0x006b1a30</remarks>
    static void free();
    /// <summary>
    /// Takes <paramref name="size"/> bytes off the top of the pool; null when it is exhausted.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b1960 (FUN_006b1960: a cdecl helper of celement.cpp without a symbol; the name is the port's)</remarks>
    static uint8_t* malloc(int32_t size);

    /// <summary>The pool's heap.</summary>
    static HeapManager* poolHeap;
    /// <summary>The pool's size in bytes.</summary>
    static int32_t size;
    /// <summary>The offset of the lowest allocation (elements are taken downward from <see cref="size"/>).</summary>
    static int32_t dataEdge;
    /// <summary>Elements allocated this frame.</summary>
    static int32_t elementCount;
};
