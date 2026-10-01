#include "stdafx.h"
#include "object/sortlist.h"
#include "lib/aerror.h"
#include "lib/heap.h"
#include "object/objtype.h"

namespace
{
    /// <summary>The bit pattern clear fills values with (0x7f7fc99e, about 3.3999e38).</summary>
    float ClearValue(bool negative)
    {
        return std::bit_cast<float>(negative ? 0xff7fc99eu : 0x7f7fc99eu);
    }
}

auto SortList::init(int32_t numItems) -> int32_t
{
    this->numItems = numItems;
    list = static_cast<SortListNode*>(ObjectTypeManager::objectCache->malloc(numItems * sizeof(SortListNode)));

    if (list == nullptr)
    {
        Fatal(0, " Unable to init sortList ");
    }

    return list == nullptr ? 1 : 0;
}

auto SortList::clear(int setToMin) -> void
{
    for (int32_t i = 0; i < numItems; i++)
    {
        list[i].id = i;
    }

    const float value = ClearValue(setToMin != 0);

    for (int32_t i = 0; i < numItems; i++)
    {
        list[i].value = value;
    }
}

auto descendingCompare(const void* elem1, const void* elem2) -> int
{
    const float value1 = static_cast<const SortListNode*>(elem1)->value;
    const float value2 = static_cast<const SortListNode*>(elem2)->value;

    if (value2 < value1)
    {
        return -1;
    }

    if (value1 < value2)
    {
        return 1;
    }

    return 0;
}

auto ascendingCompare(const void* elem1, const void* elem2) -> int
{
    const float value1 = static_cast<const SortListNode*>(elem1)->value;
    const float value2 = static_cast<const SortListNode*>(elem2)->value;

    if (value2 < value1)
    {
        return 1;
    }

    if (value1 < value2)
    {
        return -1;
    }

    return 0;
}

auto SortList::sort(int descending) -> void
{
    if (descending != 0)
    {
        std::qsort(list, numItems, sizeof(SortListNode), descendingCompare);
        return;
    }

    std::qsort(list, numItems, sizeof(SortListNode), ascendingCompare);
}

auto SortList::destroy() -> void
{
    // The original frees into systemHeap although init allocated from objectCache; systemHeap ignores the foreign
    // block, so it stays allocated until objectCache goes.
    systemHeap->free(list);
    list = nullptr;
}
