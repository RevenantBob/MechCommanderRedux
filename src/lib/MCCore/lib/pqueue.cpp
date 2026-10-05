#include "stdafx.h"
#include "lib/pqueue.h"
#include "lib/aerror.h"

int32_t PriorityQueue::init(int32_t maxQueueItems, int32_t keyMinimum)
{
    // Port fix: one slot more than the original's maxItems + 2, since insert can fill slot maxItems + 2 (see insert).
    pqList.assign(static_cast<size_t>(maxQueueItems + 3), PQNode{});
    maxItems = maxQueueItems + 2;
    keyMin = keyMinimum;
    return 0;
}

void PriorityQueue::upHeap(int32_t curIndex)
{
    const PQNode startNode = pqList[curIndex];
    // The sentinel in slot 0 stops the climb.
    pqList[0].key = keyMin;
    pqList[0].id = -1;
    int32_t parentIndex = curIndex / 2;

    while (startNode.key <= pqList[parentIndex].key)
    {
        pqList[curIndex] = pqList[parentIndex];
        curIndex = parentIndex;
        parentIndex = curIndex / 2;
    }

    pqList[curIndex] = startNode;
}

int32_t PriorityQueue::insert(PQNode& item)
{
    // Original behaviour: the capacity check compares with the slot count (items + 2), so the last insert lands one
    // slot past the original's allocation.
    if (numItems == maxItems)
    {
        return 1;
    }

    pqList[++numItems] = item;
    upHeap(numItems);
    return 0;
}

void PriorityQueue::downHeap(int32_t curIndex)
{
    const PQNode startNode = pqList[curIndex];

    while (curIndex <= numItems / 2)
    {
        int32_t childIndex = curIndex * 2;

        if (childIndex < numItems && pqList[childIndex + 1].key < pqList[childIndex].key)
        {
            ++childIndex;
        }

        if (startNode.key <= pqList[childIndex].key)
        {
            break;
        }

        pqList[curIndex] = pqList[childIndex];
        curIndex = childIndex;
    }

    pqList[curIndex] = startNode;
}

void PriorityQueue::remove(PQNode& item)
{
    item = pqList[1];
    pqList[1] = pqList[numItems--];
    downHeap(1);
}

void PriorityQueue::change(int32_t itemIndex, int32_t newValue)
{
    if (pqList[itemIndex].key < newValue)
    {
        pqList[itemIndex].key = newValue;
        downHeap(itemIndex);
    }
    else if (newValue < pqList[itemIndex].key)
    {
        pqList[itemIndex].key = newValue;
        upHeap(itemIndex);
    }
}

int32_t PriorityQueue::find(int32_t id)
{
    for (int32_t index = 0; index <= numItems; ++index)
    {
        if (pqList[index].id == id)
        {
            return index;
        }
    }

    return 0;
}

void PriorityQueue::destroy()
{
    pqList.clear();
    maxItems = 0;
    numItems = 0;
}
