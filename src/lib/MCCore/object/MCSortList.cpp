#include "stdafx.h"
#include "object/MCSortList.h"
#include "lib/MCMsvcSort.h"

namespace
{
    /// <summary>The bit pattern clear fills values with (0x7f7fc99e, about 3.3999e38).</summary>
    float ClearValue(bool negative)
    {
        return std::bit_cast<float>(negative ? 0xff7fc99eu : 0x7f7fc99eu);
    }

    /// <summary>The original's qsort comparer: -1, 0 or 1 by value, larger values first when descending.</summary>
    int CompareValues(const MCSortListNode& node1, const MCSortListNode& node2, bool descending)
    {
        const float value1 = descending ? node2.Value : node1.Value;
        const float value2 = descending ? node1.Value : node2.Value;

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
}

auto MCSortList::Clear(bool setToMin) -> void
{
    const float value = ClearValue(setToMin);

    for (size_t i = 0; i < List.size(); i++)
    {
        List[i].Id = static_cast<int32_t>(i);
        List[i].Value = value;
    }
}

auto MCSortList::Sort(bool descending) -> void
{
    // Rule R5: equal values keep the order MCX.EXE's qsort gave them.
    MCMsvcSort(std::span(List), [descending](const MCSortListNode& node1, const MCSortListNode& node2)
               { return CompareValues(node1, node2, descending); });
}
