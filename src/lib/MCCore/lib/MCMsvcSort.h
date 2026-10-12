#pragma once

/// <summary>
/// Sorts <paramref name="items"/> exactly as MCX.EXE's <c>qsort</c> (the Visual C++ 6 runtime's) did, so elements that
/// compare equal end up in the same order as in the original. <paramref name="compare"/> returns a negative number,
/// zero or a positive number, as a <c>qsort</c> comparison does.
/// </summary>
/// <remarks>
/// The VC6 algorithm: a quicksort that moves the middle element to the front as the pivot, sorts partitions of 8 or
/// fewer elements by repeatedly moving the largest to the end, and pushes the larger partition. Today's runtimes (the
/// UCRT's median-of-three, glibc's merge sort) order equal elements differently, so <c>std::qsort</c> and
/// <c>std::sort</c> can't stand in for it (rule R5).
/// </remarks>
template <typename T, typename Compare> void MCMsvcSort(std::span<T> items, Compare compare)
{
    constexpr ptrdiff_t ShortSortSize = 8;

    if (items.size() < 2)
    {
        return;
    }

    // Positions are indexes into items; a partition is [lo, hi], both included.
    const auto at = [&items](ptrdiff_t index) -> T& { return items[static_cast<size_t>(index)]; };
    const auto shortSort = [&compare, &at](ptrdiff_t lo, ptrdiff_t hi)
    {
        while (hi > lo)
        {
            ptrdiff_t largest = lo;

            for (ptrdiff_t p = lo + 1; p <= hi; ++p)
            {
                if (compare(at(p), at(largest)) > 0)
                {
                    largest = p;
                }
            }

            if (largest != hi)
            {
                std::swap(at(largest), at(hi));
            }

            --hi;
        }
    };

    std::vector<std::pair<ptrdiff_t, ptrdiff_t>> pending;
    ptrdiff_t lo = 0;
    auto hi = static_cast<ptrdiff_t>(items.size() - 1);

    for (;;)
    {
        const ptrdiff_t size = hi - lo + 1;

        if (size <= ShortSortSize)
        {
            shortSort(lo, hi);
        }
        else
        {
            const ptrdiff_t middle = lo + size / 2;

            if (middle != lo)
            {
                std::swap(at(middle), at(lo));
            }

            ptrdiff_t loGuy = lo;
            ptrdiff_t hiGuy = hi + 1;

            for (;;)
            {
                do
                {
                    ++loGuy;
                } while (loGuy <= hi && compare(at(loGuy), at(lo)) <= 0);

                do
                {
                    --hiGuy;
                } while (hiGuy > lo && compare(at(hiGuy), at(lo)) >= 0);

                if (hiGuy < loGuy)
                {
                    break;
                }

                if (loGuy != hiGuy)
                {
                    std::swap(at(loGuy), at(hiGuy));
                }
            }

            if (lo != hiGuy)
            {
                std::swap(at(lo), at(hiGuy));
            }

            // Sort the smaller side next, the larger one later.
            if (hiGuy - 1 - lo >= hi - loGuy)
            {
                if (lo + 1 < hiGuy)
                {
                    pending.emplace_back(lo, hiGuy - 1);
                }

                if (loGuy < hi)
                {
                    lo = loGuy;
                    continue;
                }
            }
            else
            {
                if (loGuy < hi)
                {
                    pending.emplace_back(loGuy, hi);
                }

                if (lo + 1 < hiGuy)
                {
                    hi = hiGuy - 1;
                    continue;
                }
            }
        }

        if (pending.empty())
        {
            return;
        }

        std::tie(lo, hi) = pending.back();
        pending.pop_back();
    }
}
