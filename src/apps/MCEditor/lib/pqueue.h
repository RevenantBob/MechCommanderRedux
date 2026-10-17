#pragma once

/// <summary>An item of a <see cref="PriorityQueue"/>. 16 bytes.</summary>
typedef struct _PQNode
{
    /// <summary>The priority; the smallest comes out first.</summary>
    int32_t key;
    /// <summary>The item's identifier (what <see cref="PriorityQueue::find"/> looks for).</summary>
    int32_t id;
    /// <summary>Caller data (the path finder's cell row).</summary>
    int32_t row;
    /// <summary>Caller data (the path finder's cell column).</summary>
    int32_t col;
} PQNode;

/// <summary>
/// A fixed-capacity binary min-heap of <see cref="PQNode"/>s: the path finders' open list. Slot 0 is a sentinel holding
/// the smallest possible key; the items are in slots 1..numItems.
/// </summary>
/// <remarks>
/// Original source: <c>lib\pqueue.cpp</c>, the editor's revision: the heap comes from <c>malloc</c>, a full queue isn't
/// fatal, and <see cref="insert"/> and <see cref="change"/> carry their own copy of <see cref="upHeap"/>.
/// </remarks>
class PriorityQueue
{
public:
    /// <summary>An empty queue with no slots; <see cref="init"/> allocates them.</summary>
    PriorityQueue() = default;
    PriorityQueue(const PriorityQueue&) = delete;
    PriorityQueue& operator=(const PriorityQueue&) = delete;

    /// <summary>
    /// Allocates room for <paramref name="maxQueueItems"/> items; keys must stay above <paramref name="keyMinimum"/>.
    /// </summary>
    /// <returns>0.</returns>
    int32_t init(int32_t maxQueueItems, int32_t keyMinimum);
    /// <summary>Adds an item.</summary>
    /// <returns>0, or 1 when the queue is full.</returns>
    int32_t insert(PQNode& item);
    /// <summary>Takes out the item with the smallest key.</summary>
    void remove(PQNode& item);
    /// <summary>Changes the key of the item in slot <paramref name="itemIndex"/> and moves it.</summary>
    void change(int32_t itemIndex, int32_t newValue);
    /// <summary>The slot of the item with <paramref name="id"/>, or 0 when it isn't queued.</summary>
    int32_t find(int32_t id);
    /// <summary>Frees the slots.</summary>
    void destroy();

    /// <summary>Empties the queue.</summary>
    void clear() { numItems = 0; }

    /// <summary>The number of queued items.</summary>
    int32_t size() const { return numItems; }

protected:
    /// <summary>Moves the item in slot <paramref name="curIndex"/> up to its place.</summary>
    void upHeap(int32_t curIndex);
    /// <summary>Moves the item in slot <paramref name="curIndex"/> down to its place.</summary>
    void downHeap(int32_t curIndex);

    /// <summary>The slots (maxItems of them).</summary>
    PQNode* pqList = nullptr;
    /// <summary>The number of slots: the capacity asked for plus 2.</summary>
    int32_t maxItems = 0;
    /// <summary>The number of queued items.</summary>
    int32_t numItems = 0;
    /// <summary>The sentinel key in slot 0, below every real key.</summary>
    int32_t keyMin = 0;
};
