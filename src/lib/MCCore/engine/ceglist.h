#pragma once

class MCElement;
class MCElementBuffer;

/// <summary>
/// A run of consecutive elements of <see cref="MCElementBuffer"/> drawn together: an object's pieces, the terrain,
/// the interface. Groups are sorted by <see cref="Depth"/>; the elements inside a group by their own depth when
/// <see cref="SortElements"/> is set.
/// </summary>
/// <remarks>Original source: <c>engine\ceglist.cpp</c>, 0x2c bytes.</remarks>
class MCElementGroup
{
public:
    /// <summary>Draws the group's elements in order.</summary>
    void Draw();

    /// <summary>Sorts the group's elements by depth (when asked to) and records their depth range.</summary>
    void Sort();

    /// <summary>Empties the group and starts it at <paramref name="buffer"/>'s next element.</summary>
    void Reset(MCElementBuffer* buffer);

    /// <summary>The number of elements in the group.</summary>
    int32_t NumElements = 0;
    /// <summary>The smallest element depth.</summary>
    float MinDepth = 0.0f;
    /// <summary>The largest element depth.</summary>
    float MaxDepth = 0.0f;
    /// <summary>The group's sort key.</summary>
    float Depth = 0.0f;
    /// <summary>The index of the group's first element in the buffer.</summary>
    int32_t FirstElement = 0;
    /// <summary>Nonzero when the elements are sorted by depth.</summary>
    int32_t SortElements = 0;
    /// <summary>The buffer the group belongs to.</summary>
    MCElementBuffer* Buffer = nullptr;
};

/// <summary>
/// The frame's draw list: element pointers and groups. Opening a group closes (sorts) the last one;
/// <see cref="Sort"/> orders the groups and <see cref="Draw"/> draws them.
/// </summary>
/// <remarks>
/// Original source: <c>engine\ceglist.cpp</c>, 0x24 bytes. The original's heap held <see cref="MaxElements"/>
/// element pointers, <see cref="MaxGroups"/> groups, then <see cref="MaxGroups"/> group pointers (the sort list).
/// </remarks>
class MCElementBuffer
{
public:
    /// <summary>Adds <paramref name="element"/> to the open group (ignored when null or when the buffer is full).</summary>
    void Add(MCElement* element);

    /// <summary>Draws every group in order.</summary>
    void Draw();

    /// <summary>
    /// Makes room for <paramref name="numElements"/> elements and <paramref name="numGroups"/> groups
    /// (<paramref name="unused"/> is ignored).
    /// </summary>
    int32_t Init(int32_t numElements, int32_t unused, int32_t numGroups);

    /// <summary>Sorts the groups by depth (a bubble sort).</summary>
    void Sort();

    /// <summary>Frees the lists.</summary>
    void Free();

    /// <summary>Empties the buffer and opens its first group.</summary>
    void Reset();

    /// <summary>Closes the current group and opens the next (sets <c>MaxObjectsDrawn</c> when none is left).</summary>
    void OpenGroup();

    /// <summary>
    /// Closes the current group and opens the next at depth <paramref name="depth"/>, its elements sorted when
    /// <paramref name="sortElements"/> is nonzero.
    /// </summary>
    void OpenGroup(int32_t depth, int sortElements);

    /// <summary>The element pointers (<see cref="MaxElements"/> of them).</summary>
    std::vector<MCElement*> Elements;
    /// <summary>Room in <see cref="Elements"/>.</summary>
    int32_t MaxElements = 0;
    /// <summary>Elements added.</summary>
    int32_t NumElements = 0;
    /// <summary>Room in <see cref="Groups"/>.</summary>
    int32_t MaxGroups = 0;
    /// <summary>Groups open.</summary>
    int32_t NumGroups = 0;
    /// <summary>The groups.</summary>
    std::vector<MCElementGroup> Groups;
    /// <summary>The group pointers, sorted by depth (after the groups in the original's heap).</summary>
    std::vector<MCElementGroup*> GroupList;
    /// <summary>The group elements are added to.</summary>
    MCElementGroup* CurrentGroup = nullptr;
    /// <summary>The last entry used in <see cref="GroupList"/>.</summary>
    MCElementGroup** LastGroupPtr = nullptr;
};

/// <summary>The frame's draw list.</summary>
/// <remarks>Defined in ceglist.cpp (globals_by_file.md lists it under sprite\mactor.cpp, its heaviest user).</remarks>
extern MCElementBuffer* ElementList;

/// <summary>Elements drawn this frame (counted by <see cref="MCElementGroup::Draw"/>).</summary>
extern int32_t NumElements;
