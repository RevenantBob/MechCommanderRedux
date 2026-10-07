#pragma once

// Original source: mcx\lib\pqueue.cpp. A binary min-heap of nodes keyed by a long, used by the path finder's open
// list. Slot 0 is a sentinel holding the smallest possible key; the items are in slots 1..numItems.

/// <summary>An item of a <see cref="MCPriorityQueue"/>. 16 bytes.</summary>
typedef struct MCPQNode
{
    /// <summary>The priority; the smallest comes out first.</summary>
    int32_t Key;
    /// <summary>The item's identifier (what <see cref="MCPriorityQueue::Find"/> looks for).</summary>
    int32_t Id;
    /// <summary>Caller data (the path finder's cell row).</summary>
    int32_t Row;
    /// <summary>Caller data (the path finder's cell column).</summary>
    int32_t Col;
} MCPQNode;

/// <summary>A fixed-capacity binary heap of <see cref="MCPQNode"/>s.</summary>
class MCPriorityQueue
{
public:
    MCPriorityQueue() = default;
    MCPriorityQueue(const MCPriorityQueue&) = delete;
    MCPriorityQueue& operator=(const MCPriorityQueue&) = delete;

    /// <summary>
    /// Allocates room for <paramref name="maxItems"/> items; keys must stay above <paramref name="keyMinimum"/>.
    /// Call <see cref="Destroy"/> to free it (the original had no destructor).
    /// </summary>
    int32_t Init(int32_t maxItems, int32_t keyMinimum);

    /// <summary>Adds an item.</summary>
    /// <returns>0, or 1 when the queue is full.</returns>
    int32_t Insert(MCPQNode& item);

    /// <summary>Takes out the item with the smallest key.</summary>
    void Remove(MCPQNode& item);

    /// <summary>Changes the key of the item in slot <paramref name="itemIndex"/> and moves it.</summary>
    void Change(int32_t itemIndex, int32_t newValue);

    /// <summary>The slot of the item with <paramref name="id"/>, or 0 when it isn't queued.</summary>
    int32_t Find(int32_t id);

    /// <summary>Frees the heap.</summary>
    void Destroy();

    /// <summary>Empties the queue.</summary>
    void Clear() { _NumItems = 0; }

    /// <summary>The number of queued items.</summary>
    int32_t Size() const { return _NumItems; }

    /// <summary>Whether nothing is queued.</summary>
    bool IsEmpty() const { return _NumItems == 0; }

    /// <summary>The item in slot <paramref name="index"/>.</summary>
    MCPQNode* GetItem(int32_t index) { return &_PqList[index]; }

protected:
    /// <summary>Moves the item in slot <paramref name="curIndex"/> up to its place.</summary>
    void UpHeap(int32_t curIndex);

    /// <summary>Moves the item in slot <paramref name="curIndex"/> down to its place.</summary>
    void DownHeap(int32_t curIndex);

    /// <summary>The slots (maxItems + 2 of them).</summary>
    std::vector<MCPQNode> _PqList;
    /// <summary>The number of slots.</summary>
    int32_t _MaxItems = 0;
    /// <summary>The number of queued items.</summary>
    int32_t _NumItems = 0;
    /// <summary>The sentinel key in slot 0, below every real key.</summary>
    int32_t _KeyMin = 0;
};
