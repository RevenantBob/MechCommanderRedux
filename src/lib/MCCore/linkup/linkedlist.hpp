#pragma once

// Original source: mcx\linkup\linkedlist.hpp, the singly linked list templates the linkup layer (and a few game
// classes: Logistics, MultiPlayer) keep its players, groups, sessions, protocols and file transfers in. The bodies
// are the template's own, inlined wherever the original used them; the out-of-line copies MSVC emitted are listed
// on each method.

/// <summary>
/// One link of an <see cref="MCFLinkedList{T}"/>: the next link and the item. The list owns its links, not the items.
/// </summary>
/// <remarks>
/// Original source: <c>linkup\linkedlist.hpp</c>, 0xc bytes (vtable, next, data). The vtable holds only the
/// destructor; the list deletes links through it.
/// </remarks>
template <class T> class MCFLink
{
public:
    /// <summary>A link holding <paramref name="item"/>, not yet chained.</summary>
    explicit MCFLink(T* item) : Next(nullptr), Data(item) {}

    /// <summary>Unchains the link (the item is not deleted).</summary>
    virtual ~MCFLink() { Next = nullptr; }

    MCFLink(const MCFLink&) = delete;
    MCFLink& operator=(const MCFLink&) = delete;

    MCFLink<T>* Next = nullptr;
    T* Data = nullptr;
};

/// <summary>
/// A singly linked list of item pointers with a built-in cursor: <see cref="Head"/> rewinds it and
/// <see cref="ReadAndNext"/> walks it. Items are appended at the tail; the list owns its links, not the items (the
/// owners' <c>ClearList</c> helpers delete those).
/// </summary>
/// <remarks>
/// Original source: <c>linkup\linkedlist.hpp</c>, 0x10 bytes, no vtable. The field order (tail first, head last) is
/// the original's.
/// </remarks>
template <class T> class MCFLinkedList
{
public:
    /// <summary>An empty list.</summary>
    /// <remarks>Inlined everywhere; the original left <see cref="Current"/> unset (the port clears it).</remarks>
    MCFLinkedList() : Tail(nullptr), Current(nullptr), Count(0), HeadLink(nullptr) {}

    /// <summary>Deletes every link (not the items), as the owners' destructors did with <c>while (head) Del(...)</c>.</summary>
    ~MCFLinkedList()
    {
        while (HeadLink != nullptr)
        {
            Del(HeadLink->Data);
        }
    }

    MCFLinkedList(const MCFLinkedList&) = delete;
    MCFLinkedList& operator=(const MCFLinkedList&) = delete;

    /// <summary>Appends <paramref name="item"/> at the tail.</summary>
    /// <remarks>Inlined everywhere (FIDPPlayer::JoinGroup, SessionManager::AddSession, ...).</remarks>
    void Add(T* item)
    {
        MCFLink<T>* link = new MCFLink<T>(item);

        if (HeadLink == nullptr)
        {
            HeadLink = link;
        }
        else
        {
            Tail->Next = link;
        }

        Tail = link;
        link->Next = nullptr;
        Count++;
    }

    /// <summary>
    /// Removes the first link holding <paramref name="item"/> (the item itself is not deleted). The cursor moves
    /// past a removed link; removing the tail rewinds it to the head.
    /// </summary>
    void Del(T* item)
    {
        MCFLink<T>* link = HeadLink;

        // Port fix: the original read head->Data without checking for an empty list.
        if (link == nullptr)
        {
            return;
        }

        if (link->Data == item)
        {
            Count--;
            HeadLink = link->Next;

            if (Tail == link)
            {
                Tail = HeadLink;
                Current = HeadLink;
            }
            else if (Current == link)
            {
                Current = link->Next;
            }

            delete link;
            return;
        }

        for (; link->Next != nullptr; link = link->Next)
        {
            if (link->Next->Data == item)
            {
                Count--;
                MCFLink<T>* victim = link->Next;

                if (Tail == victim)
                {
                    Tail = link;
                    Current = HeadLink;
                }
                else if (Current == victim)
                {
                    Current = victim->Next;
                }

                link->Next = victim->Next;
                delete victim;
                return;
            }
        }
    }

    /// <summary>Rewinds the cursor to the head.</summary>
    /// <returns>The first item, or null when the list is empty.</returns>
    T* Head()
    {
        Current = HeadLink;
        return HeadLink != nullptr ? HeadLink->Data : nullptr;
    }

    /// <summary>The number of items.</summary>
    int Size() { return Count; }

    /// <summary>The item under the cursor, moving the cursor to the next one.</summary>
    /// <returns>The item, or null past the end.</returns>
    T* ReadAndNext()
    {
        MCFLink<T>* link = Current;

        if (link == nullptr)
        {
            return nullptr;
        }

        Current = Current->Next;
        return link->Data;
    }

    /// <summary>The last link (appends go after it).</summary>
    MCFLink<T>* Tail = nullptr;
    /// <summary>The cursor of <see cref="Head"/> / <see cref="ReadAndNext"/>.</summary>
    MCFLink<T>* Current = nullptr;
    int32_t Count = 0;
    MCFLink<T>* HeadLink = nullptr;
};

/// <summary>
/// A second cursor over an <see cref="MCFLinkedList{T}"/>, so a list can be walked without moving its own cursor.
/// </summary>
/// <remarks>
/// 8 bytes (the list, the current link). Its name is the port's: the original's only instance is
/// SessionManager's player iterator (+0x8c), built inline over the player list.
/// </remarks>
template <class T> class MCFLinkedListIterator
{
public:
    /// <summary>An iterator at the head of <paramref name="aList"/>.</summary>
    explicit MCFLinkedListIterator(MCFLinkedList<T>* aList) : List(aList), Current(aList->HeadLink) {}

    MCFLinkedList<T>* List = nullptr;
    MCFLink<T>* Current = nullptr;
};
