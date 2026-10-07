#pragma once

/// <summary>One entry of a <see cref="MCSortList"/>: a value to sort by and the id it belongs to.</summary>
/// <remarks>8 bytes.</remarks>
struct MCSortListNode
{
    /// <summary>The value sorted on.</summary>
    float Value = 0;
    /// <summary>The caller's id (clear numbers the entries 0..n-1).</summary>
    int32_t Id = 0;
};

/// <summary>
/// A fixed-size list of (value, id) pairs sorted with qsort: movers rank weapons, contacts and ranges with it.
/// </summary>
/// <remarks>Original source: <c>object\sortlist.cpp</c>; 8 bytes.</remarks>
class MCSortList
{
public:
    /// <summary>Allocates <paramref name="numItems"/> entries. Returns 0.</summary>
    int32_t Init(int32_t numItems);
    /// <summary>
    /// Numbers the entries 0..n-1 and sets every value to +FLT_MAX-ish (3.4e38), or its negative when
    /// <paramref name="setToMin"/>, so unused entries sort last.
    /// </summary>
    void Clear(int setToMin);
    /// <summary>Sorts by value, descending when <paramref name="descending"/>, else ascending.</summary>
    void Sort(int descending);
    /// <summary>Frees the entries.</summary>
    void Destroy();

    /// <summary>The entries.</summary>
    std::unique_ptr<MCSortListNode[]> List;
    /// <summary>How many.</summary>
    int32_t NumItems = 0;
};

/// <summary>qsort comparer: larger values first.</summary>
int DescendingCompare(const void* elem1, const void* elem2);
/// <summary>qsort comparer: smaller values first.</summary>
int AscendingCompare(const void* elem1, const void* elem2);
