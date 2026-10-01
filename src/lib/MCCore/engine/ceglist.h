#pragma once

class Element;
class ElementBuffer;
class HeapManager;

/// <summary>
/// A run of consecutive elements of <see cref="ElementBuffer"/> drawn together: an object's pieces, the terrain,
/// the interface. Groups are sorted by <see cref="depth"/>; the elements inside a group by their own depth when
/// <see cref="sortElements"/> is set.
/// </summary>
/// <remarks>Original source: <c>engine\ceglist.cpp</c>, 0x2c bytes.</remarks>
class ElementGroup
{
public:
    /// <summary>Draws the group's elements in order.</summary>
    /// <remarks>MCX.EXE @ 0x006b1470</remarks>
    void draw();

    /// <summary>Sorts the group's elements by depth (when asked to) and records their depth range.</summary>
    /// <remarks>MCX.EXE @ 0x006b14b0</remarks>
    void sort();

    /// <summary>Empties the group and starts it at <paramref name="buffer"/>'s next element.</summary>
    /// <remarks>MCX.EXE @ 0x006b1540</remarks>
    void reset(ElementBuffer* buffer);

    /// <summary>The number of elements in the group.</summary>
    int32_t numElements; // +0x00
    int32_t unknown04;   // +0x04 (never accessed)
    int32_t unknown08;   // +0x08 (never accessed)
    int32_t unknown0C;   // +0x0c (never accessed)
    int32_t unknown10;   // +0x10 (never accessed)
    /// <summary>The smallest element depth.</summary>
    float minDepth; // +0x14
    /// <summary>The largest element depth.</summary>
    float maxDepth; // +0x18
    /// <summary>The group's sort key.</summary>
    float depth; // +0x1c
    /// <summary>The index of the group's first element in the buffer.</summary>
    int32_t firstElement; // +0x20
    /// <summary>Nonzero when the elements are sorted by depth.</summary>
    int32_t sortElements; // +0x24
    /// <summary>The buffer the group belongs to.</summary>
    ElementBuffer* buffer; // +0x28
};

/// <summary>
/// The frame's draw list: element pointers and groups in one heap. Opening a group closes (sorts) the last one;
/// <see cref="sort"/> orders the groups and <see cref="draw"/> draws them.
/// </summary>
/// <remarks>
/// Original source: <c>engine\ceglist.cpp</c>, 0x24 bytes. The heap holds <see cref="maxElements"/> element
/// pointers, <see cref="maxGroups"/> groups, then <see cref="maxGroups"/> group pointers (the sort list).
/// </remarks>
class ElementBuffer
{
public:
    /// <summary>Allocates from <c>systemHeap</c>.</summary>
    /// <remarks>MCX.EXE @ 0x006b1570</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x006b1590</remarks>
    static void operator delete(void* block);

    /// <summary>Adds <paramref name="element"/> to the open group (ignored when null or when the buffer is full).</summary>
    /// <remarks>MCX.EXE @ 0x006b15b0</remarks>
    void add(Element* element);

    /// <summary>Draws every group in order.</summary>
    /// <remarks>MCX.EXE @ 0x006b1610</remarks>
    void draw();

    /// <summary>
    /// Creates the heap for <paramref name="numElements"/> elements and <paramref name="numGroups"/> groups
    /// (<paramref name="unused"/> is ignored).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b1640</remarks>
    int32_t init(int32_t numElements, int32_t unused, int32_t numGroups);

    /// <summary>Sorts the groups by depth (a bubble sort).</summary>
    /// <remarks>MCX.EXE @ 0x006b16e0</remarks>
    void sort();

    /// <summary>Frees the heap and the sort queue.</summary>
    /// <remarks>MCX.EXE @ 0x006b1770</remarks>
    void free();

    /// <summary>Empties the buffer and opens its first group.</summary>
    /// <remarks>MCX.EXE @ 0x006b17b0</remarks>
    void reset();

    /// <summary>Closes the current group and opens the next (sets <c>MaxObjectsDrawn</c> when none is left).</summary>
    /// <remarks>MCX.EXE @ 0x006b17f0</remarks>
    void openGroup();

    /// <summary>
    /// Closes the current group and opens the next at depth <paramref name="depth"/>, its elements sorted when
    /// <paramref name="sortElements"/> is nonzero.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b1850</remarks>
    void openGroup(int32_t depth, int sortElements);

    /// <summary>A sort scratch list (never allocated in MCX.EXE; group sorting skips when it is set).</summary>
    static Element** sortQueue;

    /// <summary>The heap holding the lists.</summary>
    HeapManager* elementHeap; // +0x00
    /// <summary>The element pointers.</summary>
    Element** elements; // +0x04
    /// <summary>Room in <see cref="elements"/>.</summary>
    int32_t maxElements; // +0x08
    /// <summary>Elements added.</summary>
    int32_t numElements; // +0x0c
    /// <summary>Room in <see cref="groups"/>.</summary>
    int32_t maxGroups; // +0x10
    /// <summary>Groups open.</summary>
    int32_t numGroups; // +0x14
    /// <summary>The groups (after the element pointers).</summary>
    ElementGroup* groups; // +0x18
    /// <summary>The group elements are added to.</summary>
    ElementGroup* currentGroup; // +0x1c
    /// <summary>The last entry used in the group pointer list (after the groups).</summary>
    ElementGroup** lastGroupPtr; // +0x20
};

/// <summary>The frame's draw list.</summary>
/// <remarks>Defined in ceglist.cpp (globals_by_file.md lists it under sprite\mactor.cpp, its heaviest user).</remarks>
extern ElementBuffer* ElementList;

/// <summary>Elements drawn this frame (counted by <see cref="ElementGroup::draw"/>).</summary>
extern int32_t numElements;
