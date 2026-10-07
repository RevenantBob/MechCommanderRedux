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

    T* const base = items.data();
    const auto shortSort = [&compare](T* lo, T* hi)
    {
        while (hi > lo)
        {
            T* largest = lo;

            for (T* p = lo + 1; p <= hi; ++p)
            {
                if (compare(*p, *largest) > 0)
                {
                    largest = p;
                }
            }

            if (largest != hi)
            {
                std::swap(*largest, *hi);
            }

            --hi;
        }
    };

    std::vector<std::pair<T*, T*>> pending;
    T* lo = base;
    T* hi = base + (items.size() - 1);

    for (;;)
    {
        const ptrdiff_t size = hi - lo + 1;

        if (size <= ShortSortSize)
        {
            shortSort(lo, hi);
        }
        else
        {
            T* const middle = lo + size / 2;

            if (middle != lo)
            {
                std::swap(*middle, *lo);
            }

            T* loGuy = lo;
            T* hiGuy = hi + 1;

            for (;;)
            {
                do
                {
                    ++loGuy;
                } while (loGuy <= hi && compare(*loGuy, *lo) <= 0);

                do
                {
                    --hiGuy;
                } while (hiGuy > lo && compare(*hiGuy, *lo) >= 0);

                if (hiGuy < loGuy)
                {
                    break;
                }

                if (loGuy != hiGuy)
                {
                    std::swap(*loGuy, *hiGuy);
                }
            }

            if (lo != hiGuy)
            {
                std::swap(*lo, *hiGuy);
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
