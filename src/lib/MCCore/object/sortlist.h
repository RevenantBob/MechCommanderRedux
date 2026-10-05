#pragma once

/// <summary>One entry of a <see cref="SortList"/>: a value to sort by and the id it belongs to.</summary>
/// <remarks>8 bytes.</remarks>
struct SortListNode
{
    /// <summary>The value sorted on.</summary>
    float value = 0; // +0x00
    /// <summary>The caller's id (clear numbers the entries 0..n-1).</summary>
    int32_t id = 0; // +0x04
};

/// <summary>
/// A fixed-size list of (value, id) pairs sorted with qsort: movers rank weapons, contacts and ranges with it.
/// </summary>
/// <remarks>Original source: <c>object\sortlist.cpp</c>; 8 bytes.</remarks>
class SortList
{
public:
    /// <summary>Allocates <paramref name="numItems"/> entries. Returns 0.</summary>
    /// <remarks>MCX.EXE @ 0x006b4d90</remarks>
    int32_t init(int32_t numItems);
    /// <summary>
    /// Numbers the entries 0..n-1 and sets every value to +FLT_MAX-ish (3.4e38), or its negative when
    /// <paramref name="setToMin"/>, so unused entries sort last.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b4dd0</remarks>
    void clear(int setToMin);
    /// <summary>Sorts by value, descending when <paramref name="descending"/>, else ascending.</summary>
    /// <remarks>MCX.EXE @ 0x006b4ea0</remarks>
    void sort(int descending);
    /// <summary>Frees the entries.</summary>
    /// <remarks>MCX.EXE @ 0x006b4ee0</remarks>
    void destroy();

    /// <summary>The entries.</summary>
    std::unique_ptr<SortListNode[]> list; // +0x00
    /// <summary>How many.</summary>
    int32_t numItems = 0; // +0x04
};

/// <summary>qsort comparer: larger values first.</summary>
/// <remarks>MCX.EXE @ 0x006b4e20</remarks>
int descendingCompare(const void* elem1, const void* elem2);
/// <summary>qsort comparer: smaller values first.</summary>
/// <remarks>MCX.EXE @ 0x006b4e60</remarks>
int ascendingCompare(const void* elem1, const void* elem2);
