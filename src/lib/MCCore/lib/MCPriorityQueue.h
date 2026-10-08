#pragma once

/// <summary>An item of a <see cref="MCPriorityQueue"/>.</summary>
struct MCPQNode
{
    /// <summary>The priority; the smallest comes out first.</summary>
    int32_t Key = 0;
    /// <summary>The item's identifier (what <see cref="MCPriorityQueue::Find"/> looks for).</summary>
    int32_t Id = 0;
    /// <summary>Caller data (the path finder's cell row).</summary>
    int32_t Row = 0;
    /// <summary>Caller data (the path finder's cell column).</summary>
    int32_t Col = 0;
};

/// <summary>
/// A binary min-heap of <see cref="MCPQNode"/>s with a fixed capacity: the path finder's open list. Slot 0 is a
/// sentinel holding a key below every real one; the items are in slots 1..Size().
/// </summary>
/// <remarks>
/// Original source: <c>lib\pqueue.cpp</c>. It stays a hand-written heap (rule R5): the order equal keys come out in
/// decides the paths, and <c>Change</c> and <c>Find</c> work on slots, which <c>std::priority_queue</c> hides.
/// </remarks>
class MCPriorityQueue
{
public:
    /// <summary>
    /// An empty queue that holds <paramref name="maxItems"/> + 2 items (the original's capacity test counts its two
    /// spare slots), with keys above <paramref name="keyMinimum"/>.
    /// </summary>
    MCPriorityQueue(int32_t maxItems, int32_t keyMinimum);

    /// <summary>
    /// Makes room for at least <paramref name="maxItems"/> items. The slots already there keep what they hold (a
    /// <see cref="Change"/> past the queued items reads them).
    /// </summary>
    void Reserve(int32_t maxItems);

    /// <summary>Adds an item.</summary>
    /// <returns>Whether it was added (false when the queue is full).</returns>
    bool Insert(const MCPQNode& item);

    /// <summary>Takes out the item with the smallest key (the queue must not be empty).</summary>
    MCPQNode Pop();

    /// <summary>Changes the key of the item in slot <paramref name="itemIndex"/> and moves it.</summary>
    void Change(int32_t itemIndex, int32_t newValue);

    /// <summary>The slot of the item with <paramref name="id"/>, or 0 when it isn't queued.</summary>
    int32_t Find(int32_t id) const;

    /// <summary>Empties the queue.</summary>
    void Clear() { _NumItems = 0; }

    /// <summary>The number of queued items.</summary>
    int32_t Size() const { return _NumItems; }

    /// <summary>Whether nothing is queued.</summary>
    bool IsEmpty() const { return _NumItems == 0; }

    /// <summary>The item in slot <paramref name="index"/>.</summary>
    const MCPQNode& GetItem(int32_t index) const { return _Slots[static_cast<size_t>(index)]; }

private:
    /// <summary>Moves the item in slot <paramref name="curIndex"/> up to its place.</summary>
    void UpHeap(int32_t curIndex);

    /// <summary>Moves the item in slot <paramref name="curIndex"/> down to its place.</summary>
    void DownHeap(int32_t curIndex);

    /// <summary>The sentinel slot and one slot per item.</summary>
    std::vector<MCPQNode> _Slots;
    /// <summary>How many items fit.</summary>
    int32_t _Capacity = 0;
    /// <summary>The number of queued items.</summary>
    int32_t _NumItems = 0;
    /// <summary>The sentinel key in slot 0, below every real key.</summary>
    int32_t _KeyMin = 0;
};
