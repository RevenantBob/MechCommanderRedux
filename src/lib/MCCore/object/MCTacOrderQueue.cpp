#include "stdafx.h"
#include "object/MCTacOrderQueue.h"

auto MCTacOrderQueue::Push(const MCQueuedTacOrder& order) -> bool
{
    if (Size() == MaxOrders)
    {
        return false;
    }

    _Orders.push_back(order);
    return true;
}

auto MCTacOrderQueue::PopFront() -> void
{
    _Orders.erase(_Orders.begin());
}

auto CompareTacOrderId(int32_t id1, int32_t id2) -> int32_t
{
    if (id1 < 0xf1)
    {
        if (id1 < 0x10 && id2 > 0xf0)
        {
            return (id1 - id2) + 0xff;
        }
    }
    else if (id2 < 0x10)
    {
        return (id1 - id2) - 0xff;
    }

    return id1 - id2;
}
