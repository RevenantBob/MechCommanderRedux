#include "stdafx.h"
#include "object/sortlist.h"
#include "lib/MCFatal.h"
#include "object/objtype.h"

namespace
{
    /// <summary>The bit pattern clear fills values with (0x7f7fc99e, about 3.3999e38).</summary>
    float ClearValue(bool negative)
    {
        return std::bit_cast<float>(negative ? 0xff7fc99eu : 0x7f7fc99eu);
    }
}

auto MCSortList::Init(int32_t numItems) -> int32_t
{
    this->NumItems = numItems;
    List = std::make_unique<MCSortListNode[]>(static_cast<size_t>(numItems));
    return 0;
}

auto MCSortList::Clear(int setToMin) -> void
{
    for (int32_t i = 0; i < NumItems; i++)
    {
        List[i].Id = i;
    }

    const float value = ClearValue(setToMin != 0);

    for (int32_t i = 0; i < NumItems; i++)
    {
        List[i].Value = value;
    }
}

auto DescendingCompare(const void* elem1, const void* elem2) -> int
{
    const float value1 = static_cast<const MCSortListNode*>(elem1)->Value;
    const float value2 = static_cast<const MCSortListNode*>(elem2)->Value;

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

auto AscendingCompare(const void* elem1, const void* elem2) -> int
{
    const float value1 = static_cast<const MCSortListNode*>(elem1)->Value;
    const float value2 = static_cast<const MCSortListNode*>(elem2)->Value;

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

auto MCSortList::Sort(int descending) -> void
{
    if (descending != 0)
    {
        std::qsort(List.get(), NumItems, sizeof(MCSortListNode), DescendingCompare);
        return;
    }

    std::qsort(List.get(), NumItems, sizeof(MCSortListNode), AscendingCompare);
}

auto MCSortList::Destroy() -> void
{
    // The original freed into systemHeap although init allocated from objectCache, so the block stayed allocated
    // until objectCache went; nothing reads it after destroy.
    List.reset();
}
