#include "stdafx.h"
#include "lib/MCPriorityQueue.h"

MCPriorityQueue::MCPriorityQueue(int32_t maxItems, int32_t keyMinimum)
    : _Slots(static_cast<size_t>(maxItems) + 3), _Capacity(maxItems + 2), _KeyMin(keyMinimum)
{
}

void MCPriorityQueue::Reserve(int32_t maxItems)
{
    if (maxItems + 2 > _Capacity)
    {
        _Slots.resize(static_cast<size_t>(maxItems) + 3);
        _Capacity = maxItems + 2;
    }
}

void MCPriorityQueue::UpHeap(int32_t curIndex)
{
    const MCPQNode startNode = _Slots[curIndex];
    // The sentinel in slot 0 stops the climb.
    _Slots[0].Key = _KeyMin;
    _Slots[0].Id = -1;
    int32_t parentIndex = curIndex / 2;

    while (startNode.Key <= _Slots[parentIndex].Key)
    {
        _Slots[curIndex] = _Slots[parentIndex];
        curIndex = parentIndex;
        parentIndex = curIndex / 2;
    }

    _Slots[curIndex] = startNode;
}

bool MCPriorityQueue::Insert(const MCPQNode& item)
{
    if (_NumItems == _Capacity)
    {
        return false;
    }

    _Slots[++_NumItems] = item;
    UpHeap(_NumItems);
    return true;
}

void MCPriorityQueue::DownHeap(int32_t curIndex)
{
    const MCPQNode startNode = _Slots[curIndex];

    while (curIndex <= _NumItems / 2)
    {
        int32_t childIndex = curIndex * 2;

        if (childIndex < _NumItems && _Slots[childIndex + 1].Key < _Slots[childIndex].Key)
        {
            ++childIndex;
        }

        if (startNode.Key <= _Slots[childIndex].Key)
        {
            break;
        }

        _Slots[curIndex] = _Slots[childIndex];
        curIndex = childIndex;
    }

    _Slots[curIndex] = startNode;
}

MCPQNode MCPriorityQueue::Pop()
{
    const MCPQNode item = _Slots[1];
    _Slots[1] = _Slots[_NumItems--];
    DownHeap(1);
    return item;
}

void MCPriorityQueue::Change(int32_t itemIndex, int32_t newValue)
{
    if (_Slots[itemIndex].Key < newValue)
    {
        _Slots[itemIndex].Key = newValue;
        DownHeap(itemIndex);
    }
    else if (newValue < _Slots[itemIndex].Key)
    {
        _Slots[itemIndex].Key = newValue;
        UpHeap(itemIndex);
    }
}

int32_t MCPriorityQueue::Find(int32_t id) const
{
    // Slot 0 (the sentinel, id -1) is searched too, as in the original.
    for (int32_t index = 0; index <= _NumItems; ++index)
    {
        if (_Slots[index].Id == id)
        {
            return index;
        }
    }

    return 0;
}
