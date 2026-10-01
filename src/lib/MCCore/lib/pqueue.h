#pragma once

// Original source: mcx\lib\pqueue.cpp. A binary min-heap of nodes keyed by a long, used by the path finder's open
// list. Slot 0 is a sentinel holding the smallest possible key; the items are in slots 1..numItems.

/// <summary>An item of a <see cref="PriorityQueue"/>. 16 bytes.</summary>
typedef struct _PQNode
{
    /// <summary>The priority; the smallest comes out first.</summary>
    int32_t key; // +0x00
    /// <summary>The item's identifier (what <see cref="PriorityQueue::find"/> looks for).</summary>
    int32_t id; // +0x04
    /// <summary>Caller data (the path finder's cell row).</summary>
    int32_t row; // +0x08
    /// <summary>Caller data (the path finder's cell column).</summary>
    int32_t col; // +0x0c
} PQNode;

/// <summary>A fixed-capacity binary heap of <see cref="PQNode"/>s, allocated from systemHeap.</summary>
class PriorityQueue
{
public:
    PriorityQueue() = default;
    PriorityQueue(const PriorityQueue&) = delete;
    PriorityQueue& operator=(const PriorityQueue&) = delete;

    /// <summary>
    /// Allocates room for <paramref name="maxItems"/> items; keys must stay above <paramref name="keyMinimum"/>.
    /// Call <see cref="destroy"/> to free it (the original had no destructor).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006c51b0</remarks>
    int32_t init(int32_t maxItems, int32_t keyMinimum);

    /// <summary>Adds an item.</summary>
    /// <returns>0, or 1 when the queue is full.</returns>
    /// <remarks>MCX.EXE @ 0x006c52a0</remarks>
    int32_t insert(PQNode& item);

    /// <summary>Takes out the item with the smallest key.</summary>
    /// <remarks>MCX.EXE @ 0x006c53b0</remarks>
    void remove(PQNode& item);

    /// <summary>Changes the key of the item in slot <paramref name="itemIndex"/> and moves it.</summary>
    /// <remarks>MCX.EXE @ 0x006c5410</remarks>
    void change(int32_t itemIndex, int32_t newValue);

    /// <summary>The slot of the item with <paramref name="id"/>, or 0 when it isn't queued.</summary>
    /// <remarks>MCX.EXE @ 0x006c5450</remarks>
    int32_t find(int32_t id);

    /// <summary>Frees the heap.</summary>
    /// <remarks>MCX.EXE @ 0x006c5480</remarks>
    void destroy();

    /// <summary>Empties the queue.</summary>
    void clear() { numItems = 0; }

    /// <summary>The number of queued items.</summary>
    int32_t size() const { return numItems; }

    /// <summary>Whether nothing is queued.</summary>
    bool isEmpty() const { return numItems == 0; }

    /// <summary>The item in slot <paramref name="index"/>.</summary>
    PQNode* getItem(int32_t index) { return &pqList[index]; }

protected:
    /// <summary>Moves the item in slot <paramref name="curIndex"/> up to its place.</summary>
    /// <remarks>MCX.EXE @ 0x006c5200</remarks>
    void upHeap(int32_t curIndex);

    /// <summary>Moves the item in slot <paramref name="curIndex"/> down to its place.</summary>
    /// <remarks>MCX.EXE @ 0x006c52f0</remarks>
    void downHeap(int32_t curIndex);

    /// <summary>The slots (maxItems + 2 of them).</summary>
    PQNode* pqList = nullptr; // +0x00
    /// <summary>The number of slots.</summary>
    int32_t maxItems = 0; // +0x04
    /// <summary>The number of queued items.</summary>
    int32_t numItems = 0; // +0x08
    /// <summary>The sentinel key in slot 0, below every real key.</summary>
    int32_t keyMin = 0; // +0x0c
};
