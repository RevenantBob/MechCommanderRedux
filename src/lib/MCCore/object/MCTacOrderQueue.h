#pragma once

#include "lib/MCVector3D.h"

/// <summary>A queued player order as the queue keeps it: its id, first way point and packed data.</summary>
/// <remarks>The original name is <c>_QueuedTacOrder</c>.</remarks>
struct MCQueuedTacOrder
{
    int32_t Id = 0;
    MCVector3D Point;
    std::array<uint32_t, 2> PackedData{};
};

/// <summary>
/// A pilot's queue of player orders (shift-click orders), first in first out.
/// </summary>
/// <remarks>
/// The original cut each pilot's 16 slots from one pool of 2000 (TacOrderQueue), so pilots past the 125th had no queue;
/// each pilot owns its slots now.
/// </remarks>
class MCTacOrderQueue
{
public:
    /// <summary>Orders a pilot can have queued: a game rule the player sees (more are refused).</summary>
    static constexpr int32_t MaxOrders = 0x10;

    /// <summary>Queues <paramref name="order"/> at the back; false when the queue is full.</summary>
    bool Push(const MCQueuedTacOrder& order);
    /// <summary>Takes the front order away.</summary>
    void PopFront();
    /// <summary>The front order (the queue must not be empty).</summary>
    const MCQueuedTacOrder& Front() const { return _Orders.front(); }
    /// <summary>Empties the queue.</summary>
    void Clear() { _Orders.clear(); }
    /// <summary>How many orders are queued.</summary>
    int32_t Size() const { return static_cast<int32_t>(_Orders.size()); }
    /// <summary>Whether no order is queued.</summary>
    bool Empty() const { return _Orders.empty(); }
    /// <summary>The queued orders, front first.</summary>
    std::span<const MCQueuedTacOrder> Orders() const { return _Orders; }

private:
    /// <summary>The orders, front first.</summary>
    std::vector<MCQueuedTacOrder> _Orders;
};

/// <summary>Order ids wrap at 255: compares two, treating 241-255 as before 1-15.</summary>
/// <returns>Negative when <paramref name="id1"/> comes first, 0 when equal, else positive.</returns>
int32_t CompareTacOrderId(int32_t id1, int32_t id2);
