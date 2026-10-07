#include "stdafx.h"
#include "lib/MCLinkedList.h"

void MCLinkedList::AddToHead(MCLink* link)
{
    if (link == nullptr)
    {
        return;
    }

    MCLink* oldHead = _Head;
    _Head = link;
    link->Next = oldHead;

    if (_Tail == nullptr)
    {
        _Tail = link;
    }
}

void MCLinkedList::AddToTail(MCLink* link)
{
    if (link == nullptr)
    {
        return;
    }

    if (_Tail != nullptr)
    {
        _Tail->Next = link;
        _Tail = link;
        return;
    }

    _Head = link;
    _Tail = link;
}

void MCLinkedList::Destroy(MCLink* link, MCLink* previous)
{
    Remove(link, previous);
    delete link;
}

void MCLinkedList::InsertAfter(MCLink* after, MCLink* newLink)
{
    if (after == nullptr || newLink == nullptr)
    {
        return;
    }

    if (after != _Tail)
    {
        newLink->Next = after->Next;
        after->Next = newLink;
        return;
    }

    _Tail = newLink;
    newLink->Next = nullptr;
    after->Next = newLink;
}

void MCLinkedList::Remove(MCLink* link, MCLink* previous)
{
    if (previous == nullptr)
    {
        if (_Head == nullptr)
        {
            return;
        }

        MCLink* current = _Head;

        while (current != link)
        {
            previous = current;

            if (current->Next == nullptr)
            {
                return;
            }

            current = current->Next;
        }

        if (previous == nullptr)
        {
            // Removing the head.
            _Head = _Head->Next;

            if (_Head == nullptr)
            {
                _Tail = nullptr;
            }

            return;
        }
    }

    if (_Tail == link)
    {
        _Tail = previous;
        previous->Next = nullptr;
    }
    else
    {
        previous->Next = link->Next;

        // As in the original: the head can only be the link here when the caller passed a wrong previous.
        if (_Head == link)
        {
            _Head = link->Next;
        }
    }
}

void MCLinkedList::Kill()
{
    while (_Head != nullptr)
    {
        MCLink* current = _Head;
        MCLink* following = current->Next;
        delete current;
        _Head = following;
    }

    _Tail = nullptr;
    _Head = nullptr;
}

bool MCLinkedList::Traverse(MCLink*& link)
{
    if (link == nullptr)
    {
        link = _Head;
        return link != nullptr;
    }

    link = link->Next;
    return link != nullptr;
}

uint32_t MCLinkedList::Count()
{
    uint32_t count = 0;

    for (MCLink* current = _Head; current != nullptr; current = current->Next)
    {
        ++count;
    }

    return count;
}
