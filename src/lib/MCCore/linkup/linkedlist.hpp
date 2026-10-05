#pragma once

// Original source: mcx\linkup\linkedlist.hpp, the singly linked list templates the linkup layer (and a few game
// classes: Logistics, MultiPlayer) keep its players, groups, sessions, protocols and file transfers in. The bodies
// are the template's own, inlined wherever the original used them; the out-of-line copies MSVC emitted are listed
// on each method.

/// <summary>
/// One link of an <see cref="FLinkedList{T}"/>: the next link and the item. The list owns its links, not the items.
/// </summary>
/// <remarks>
/// Original source: <c>linkup\linkedlist.hpp</c>, 0xc bytes (vtable, next, data). The vtable holds only the
/// destructor; the list deletes links through it.
/// </remarks>
template <class T> class FLink
{
public:
    /// <summary>A link holding <paramref name="item"/>, not yet chained.</summary>
    explicit FLink(T* item) : next(nullptr), data(item) {}

    /// <summary>Unchains the link (the item is not deleted).</summary>
    /// <remarks>
    /// MCX.EXE @ 0x0074bc90 (FIDPMessage), 0x0074bcd0 (unsigned long), 0x007573d0 (FIDPNetworkProtocol),
    /// 0x00757410 (FIDPSession), 0x00757450 (FIDPPlayer), 0x00757490 (FIDPGroup), 0x007574d0 (FileTransferInfo),
    /// 0x00700790 (char): the deleting destructors.
    /// </remarks>
    virtual ~FLink() { next = nullptr; }

    FLink(const FLink&) = delete;
    FLink& operator=(const FLink&) = delete;

    FLink<T>* next = nullptr; // +0x4
    T* data = nullptr;        // +0x8
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
template <class T> class FLinkedList
{
public:
    /// <summary>An empty list.</summary>
    /// <remarks>Inlined everywhere; the original left <see cref="current"/> unset (the port clears it).</remarks>
    FLinkedList() : tail(nullptr), current(nullptr), count(0), head(nullptr) {}

    /// <summary>Deletes every link (not the items), as the owners' destructors did with <c>while (head) Del(...)</c>.</summary>
    ~FLinkedList()
    {
        while (head != nullptr)
        {
            Del(head->data);
        }
    }

    FLinkedList(const FLinkedList&) = delete;
    FLinkedList& operator=(const FLinkedList&) = delete;

    /// <summary>Appends <paramref name="item"/> at the tail.</summary>
    /// <remarks>Inlined everywhere (FIDPPlayer::JoinGroup, SessionManager::AddSession, ...).</remarks>
    void Add(T* item)
    {
        FLink<T>* link = new FLink<T>(item);

        if (head == nullptr)
        {
            head = link;
        }
        else
        {
            tail->next = link;
        }

        tail = link;
        link->next = nullptr;
        count++;
    }

    /// <summary>
    /// Removes the first link holding <paramref name="item"/> (the item itself is not deleted). The cursor moves
    /// past a removed link; removing the tail rewinds it to the head.
    /// </summary>
    /// <remarks>
    /// MCX.EXE @ 0x00756e50 (FIDPPlayer), 0x00756fb0 (FIDPGroup), 0x00757110 (FileTransferInfo),
    /// 0x00757270 (FIDPNetworkProtocol), 0x007006f0 (char).
    /// </remarks>
    void Del(T* item)
    {
        FLink<T>* link = head;

        // Port fix: the original read head->data without checking for an empty list.
        if (link == nullptr)
        {
            return;
        }

        if (link->data == item)
        {
            count--;
            head = link->next;

            if (tail == link)
            {
                tail = head;
                current = head;
            }
            else if (current == link)
            {
                current = link->next;
            }

            delete link;
            return;
        }

        for (; link->next != nullptr; link = link->next)
        {
            if (link->next->data == item)
            {
                count--;
                FLink<T>* victim = link->next;

                if (tail == victim)
                {
                    tail = link;
                    current = head;
                }
                else if (current == victim)
                {
                    current = victim->next;
                }

                link->next = victim->next;
                delete victim;
                return;
            }
        }
    }

    /// <summary>Rewinds the cursor to the head.</summary>
    /// <returns>The first item, or null when the list is empty.</returns>
    /// <remarks>MCX.EXE @ 0x0074a420 (FIDPNetworkProtocol)</remarks>
    T* Head()
    {
        current = head;
        return head != nullptr ? head->data : nullptr;
    }

    /// <summary>The number of items.</summary>
    /// <remarks>MCX.EXE @ 0x0074a450 (FIDPNetworkProtocol)</remarks>
    int Size() { return count; }

    /// <summary>The item under the cursor, moving the cursor to the next one.</summary>
    /// <returns>The item, or null past the end.</returns>
    /// <remarks>MCX.EXE @ 0x0074a470 (FIDPNetworkProtocol)</remarks>
    T* ReadAndNext()
    {
        FLink<T>* link = current;

        if (link == nullptr)
        {
            return nullptr;
        }

        current = current->next;
        return link->data;
    }

    /// <summary>The last link (appends go after it).</summary>
    FLink<T>* tail = nullptr; // +0x0
    /// <summary>The cursor of <see cref="Head"/> / <see cref="ReadAndNext"/>.</summary>
    FLink<T>* current = nullptr; // +0x4
    int32_t count = 0;           // +0x8
    FLink<T>* head = nullptr;    // +0xc
};

/// <summary>
/// A second cursor over an <see cref="FLinkedList{T}"/>, so a list can be walked without moving its own cursor.
/// </summary>
/// <remarks>
/// 8 bytes (the list, the current link). Its name is the port's: the original's only instance is
/// SessionManager's player iterator (+0x8c), built inline over the player list.
/// </remarks>
template <class T> class FLinkedListIterator
{
public:
    /// <summary>An iterator at the head of <paramref name="aList"/>.</summary>
    explicit FLinkedListIterator(FLinkedList<T>* aList) : list(aList), current(aList->head) {}

    FLinkedList<T>* list = nullptr; // +0x0
    FLink<T>* current = nullptr;    // +0x4
};
