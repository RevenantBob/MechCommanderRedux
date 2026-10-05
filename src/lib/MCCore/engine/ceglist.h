#pragma once

class Element;
class ElementBuffer;

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
    int32_t numElements = 0; // +0x00
    /// <summary>The smallest element depth.</summary>
    float minDepth = 0.0f; // +0x14
    /// <summary>The largest element depth.</summary>
    float maxDepth = 0.0f; // +0x18
    /// <summary>The group's sort key.</summary>
    float depth = 0.0f; // +0x1c
    /// <summary>The index of the group's first element in the buffer.</summary>
    int32_t firstElement = 0; // +0x20
    /// <summary>Nonzero when the elements are sorted by depth.</summary>
    int32_t sortElements = 0; // +0x24
    /// <summary>The buffer the group belongs to.</summary>
    ElementBuffer* buffer = nullptr; // +0x28
};

/// <summary>
/// The frame's draw list: element pointers and groups. Opening a group closes (sorts) the last one;
/// <see cref="sort"/> orders the groups and <see cref="draw"/> draws them.
/// </summary>
/// <remarks>
/// Original source: <c>engine\ceglist.cpp</c>, 0x24 bytes. The original's heap held <see cref="maxElements"/>
/// element pointers, <see cref="maxGroups"/> groups, then <see cref="maxGroups"/> group pointers (the sort list).
/// </remarks>
class ElementBuffer
{
public:
    /// <summary>Adds <paramref name="element"/> to the open group (ignored when null or when the buffer is full).</summary>
    /// <remarks>MCX.EXE @ 0x006b15b0</remarks>
    void add(Element* element);

    /// <summary>Draws every group in order.</summary>
    /// <remarks>MCX.EXE @ 0x006b1610</remarks>
    void draw();

    /// <summary>
    /// Makes room for <paramref name="numElements"/> elements and <paramref name="numGroups"/> groups
    /// (<paramref name="unused"/> is ignored).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b1640</remarks>
    int32_t init(int32_t numElements, int32_t unused, int32_t numGroups);

    /// <summary>Sorts the groups by depth (a bubble sort).</summary>
    /// <remarks>MCX.EXE @ 0x006b16e0</remarks>
    void sort();

    /// <summary>Frees the lists.</summary>
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

    /// <summary>The element pointers (<see cref="maxElements"/> of them).</summary>
    std::vector<Element*> elements; // +0x04
    /// <summary>Room in <see cref="elements"/>.</summary>
    int32_t maxElements = 0; // +0x08
    /// <summary>Elements added.</summary>
    int32_t numElements = 0; // +0x0c
    /// <summary>Room in <see cref="groups"/>.</summary>
    int32_t maxGroups = 0; // +0x10
    /// <summary>Groups open.</summary>
    int32_t numGroups = 0; // +0x14
    /// <summary>The groups.</summary>
    std::vector<ElementGroup> groups; // +0x18
    /// <summary>The group pointers, sorted by depth (after the groups in the original's heap).</summary>
    std::vector<ElementGroup*> groupList;
    /// <summary>The group elements are added to.</summary>
    ElementGroup* currentGroup = nullptr; // +0x1c
    /// <summary>The last entry used in <see cref="groupList"/>.</summary>
    ElementGroup** lastGroupPtr = nullptr; // +0x20
};

/// <summary>The frame's draw list.</summary>
/// <remarks>Defined in ceglist.cpp (globals_by_file.md lists it under sprite\mactor.cpp, its heaviest user).</remarks>
extern ElementBuffer* ElementList;

/// <summary>Elements drawn this frame (counted by <see cref="ElementGroup::draw"/>).</summary>
extern int32_t numElements;
