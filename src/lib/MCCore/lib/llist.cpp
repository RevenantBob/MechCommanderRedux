#include "stdafx.h"
#include "lib/llist.h"

void LinkedList::AddToHead(Link* link)
{
    if (link == nullptr)
    {
        return;
    }

    Link* oldHead = head;
    head = link;
    link->next = oldHead;

    if (tail == nullptr)
    {
        tail = link;
    }
}

void LinkedList::AddToTail(Link* link)
{
    if (link == nullptr)
    {
        return;
    }

    if (tail != nullptr)
    {
        tail->next = link;
        tail = link;
        return;
    }

    head = link;
    tail = link;
}

void LinkedList::Destroy(Link* link, Link* previous)
{
    Remove(link, previous);
    delete link;
}

void LinkedList::InsertAfter(Link* after, Link* newLink)
{
    if (after == nullptr || newLink == nullptr)
    {
        return;
    }

    if (after != tail)
    {
        newLink->next = after->next;
        after->next = newLink;
        return;
    }

    tail = newLink;
    newLink->next = nullptr;
    after->next = newLink;
}

void LinkedList::Remove(Link* link, Link* previous)
{
    if (previous == nullptr)
    {
        if (head == nullptr)
        {
            return;
        }

        Link* current = head;

        while (current != link)
        {
            previous = current;

            if (current->next == nullptr)
            {
                return;
            }

            current = current->next;
        }

        if (previous == nullptr)
        {
            // Removing the head.
            head = head->next;

            if (head == nullptr)
            {
                tail = nullptr;
            }

            return;
        }
    }

    if (tail == link)
    {
        tail = previous;
        previous->next = nullptr;
    }
    else
    {
        previous->next = link->next;

        // Original behaviour: head can only equal link here when a wrong previous was passed.
        if (head == link)
        {
            head = link->next;
        }
    }
}

void LinkedList::Kill()
{
    while (head != nullptr)
    {
        Link* current = head;
        Link* following = current->next;
        delete current;
        head = following;
    }

    tail = nullptr;
    head = nullptr;
}

int LinkedList::Traverse(Link*& link)
{
    if (link == nullptr)
    {
        link = head;
        return link != nullptr;
    }

    link = link->next;
    return link != nullptr;
}

uint32_t LinkedList::Count()
{
    uint32_t count = 0;

    for (Link* current = head; current != nullptr; current = current->next)
    {
        ++count;
    }

    return count;
}
