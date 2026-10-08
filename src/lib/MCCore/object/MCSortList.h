#pragma once

/// <summary>One entry of a <see cref="MCSortList"/>: a value to sort by and the id it belongs to.</summary>
struct MCSortListNode
{
    /// <summary>The value sorted on.</summary>
    float Value = 0;
    /// <summary>The caller's id (clear numbers the entries 0..n-1).</summary>
    int32_t Id = 0;
};

/// <summary>
/// A list of (value, id) pairs sorted as MCX.EXE's qsort sorted them: movers rank weapons, contacts and ranges with
/// it.
/// </summary>
/// <remarks>Original source: <c>object\sortlist.cpp</c>.</remarks>
class MCSortList
{
public:
    /// <summary>A list of <paramref name="numItems"/> entries.</summary>
    explicit MCSortList(int32_t numItems) : List(static_cast<size_t>(numItems)) {}

    /// <summary>
    /// Numbers the entries 0..n-1 and sets every value to +FLT_MAX-ish (3.4e38), or its negative when
    /// <paramref name="setToMin"/>, so unused entries sort last.
    /// </summary>
    void Clear(bool setToMin);
    /// <summary>Sorts by value, descending when <paramref name="descending"/>, else ascending.</summary>
    void Sort(bool descending);
    /// <summary>Entries.</summary>
    int32_t NumItems() const { return static_cast<int32_t>(List.size()); }

    /// <summary>The entries.</summary>
    std::vector<MCSortListNode> List;
};
