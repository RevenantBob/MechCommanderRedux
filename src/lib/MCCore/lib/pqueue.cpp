#include "stdafx.h"
#include "lib/pqueue.h"
#include "lib/aerror.h"

int32_t MCPriorityQueue::Init(int32_t maxQueueItems, int32_t keyMinimum)
{
    // Port fix: one slot more than the original's maxItems + 2, since insert can fill slot maxItems + 2 (see insert).
    _PqList.assign(static_cast<size_t>(maxQueueItems + 3), MCPQNode{});
    _MaxItems = maxQueueItems + 2;
    _KeyMin = keyMinimum;
    return 0;
}

void MCPriorityQueue::UpHeap(int32_t curIndex)
{
    const MCPQNode startNode = _PqList[curIndex];
    // The sentinel in slot 0 stops the climb.
    _PqList[0].Key = _KeyMin;
    _PqList[0].Id = -1;
    int32_t parentIndex = curIndex / 2;

    while (startNode.Key <= _PqList[parentIndex].Key)
    {
        _PqList[curIndex] = _PqList[parentIndex];
        curIndex = parentIndex;
        parentIndex = curIndex / 2;
    }

    _PqList[curIndex] = startNode;
}

int32_t MCPriorityQueue::Insert(MCPQNode& item)
{
    // Original behaviour: the capacity check compares with the slot count (items + 2), so the last insert lands one
    // slot past the original's allocation.
    if (_NumItems == _MaxItems)
    {
        return 1;
    }

    _PqList[++_NumItems] = item;
    UpHeap(_NumItems);
    return 0;
}

void MCPriorityQueue::DownHeap(int32_t curIndex)
{
    const MCPQNode startNode = _PqList[curIndex];

    while (curIndex <= _NumItems / 2)
    {
        int32_t childIndex = curIndex * 2;

        if (childIndex < _NumItems && _PqList[childIndex + 1].Key < _PqList[childIndex].Key)
        {
            ++childIndex;
        }

        if (startNode.Key <= _PqList[childIndex].Key)
        {
            break;
        }

        _PqList[curIndex] = _PqList[childIndex];
        curIndex = childIndex;
    }

    _PqList[curIndex] = startNode;
}

void MCPriorityQueue::Remove(MCPQNode& item)
{
    item = _PqList[1];
    _PqList[1] = _PqList[_NumItems--];
    DownHeap(1);
}

void MCPriorityQueue::Change(int32_t itemIndex, int32_t newValue)
{
    if (_PqList[itemIndex].Key < newValue)
    {
        _PqList[itemIndex].Key = newValue;
        DownHeap(itemIndex);
    }
    else if (newValue < _PqList[itemIndex].Key)
    {
        _PqList[itemIndex].Key = newValue;
        UpHeap(itemIndex);
    }
}

int32_t MCPriorityQueue::Find(int32_t id)
{
    for (int32_t index = 0; index <= _NumItems; ++index)
    {
        if (_PqList[index].Id == id)
        {
            return index;
        }
    }

    return 0;
}

void MCPriorityQueue::Destroy()
{
    _PqList.clear();
    _MaxItems = 0;
    _NumItems = 0;
}
