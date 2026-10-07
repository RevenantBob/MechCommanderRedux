#include "stdafx.h"
#include "MCTest.h"
#include "lib/MCMsvcSort.h"

namespace
{
    /// <summary>An item with a sort key and a label telling equal keys apart.</summary>
    struct Item
    {
        int32_t Key = 0;
        char Label = 0;
    };

    /// <summary>The <c>qsort</c> comparison of two items by key, ascending.</summary>
    int CompareKeys(const Item& a, const Item& b)
    {
        return a.Key < b.Key ? -1 : (a.Key > b.Key ? 1 : 0);
    }

    /// <summary>The labels of <paramref name="items"/>, in order.</summary>
    std::string Labels(std::span<const Item> items)
    {
        std::string labels;

        for (const Item& item : items)
        {
            labels += item.Label;
        }

        return labels;
    }
}

TEST_CASE("sort: up to 8 items, the largest goes to the end, the first of equal ones last")
{
    // MCX.EXE's qsort sorts up to 8 items by swapping the largest with the last: of equal largest ones the first
    // found goes, so equal items don't keep their order.
    std::vector<Item> items = {{1, 'a'}, {5, 'B'}, {1, 'b'}};
    MCMsvcSort(std::span(items), CompareKeys);
    CHECK_EQ(Labels(items), std::string("baB"));

    // abcd -> dbca (a to the end) -> cbda (d to third) -> bcda (c to second).
    items = {{2, 'a'}, {2, 'b'}, {2, 'c'}, {2, 'd'}};
    MCMsvcSort(std::span(items), CompareKeys);
    CHECK_EQ(Labels(items), std::string("bcda"));
}

TEST_CASE("sort: past 8 items, the middle one is the pivot and equal items keep the partition's order")
{
    // Nine equal items: the middle (e) is swapped to the front as the pivot; the scans then meet without a swap and
    // nothing is left to sort.
    std::vector<Item> items;

    for (char label = 'a'; label <= 'i'; ++label)
    {
        items.push_back({7, label});
    }

    MCMsvcSort(std::span(items), CompareKeys);
    CHECK_EQ(Labels(items), std::string("ebcdafghi"));
}

TEST_CASE("sort: the keys come out in order for any input")
{
    std::vector<Item> items;
    uint32_t seed = 12345;

    for (int32_t i = 0; i < 1000; ++i)
    {
        seed = seed * 1103515245 + 12345;
        items.push_back({static_cast<int32_t>((seed >> 16) % 50), static_cast<char>('a' + i % 26)});
    }

    MCMsvcSort(std::span(items), CompareKeys);
    CHECK(std::ranges::is_sorted(items, {}, &Item::Key));
    CHECK_EQ(items.size(), size_t{1000});

    std::vector<Item> none;
    MCMsvcSort(std::span(none), CompareKeys);
    std::vector<Item> one = {{3, 'x'}};
    MCMsvcSort(std::span(one), CompareKeys);
    CHECK_EQ(one[0].Label, 'x');
}
