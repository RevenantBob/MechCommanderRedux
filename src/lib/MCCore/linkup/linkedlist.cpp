#include "stdafx.h"
#include "linkup/linkedlist.h"
#include "lib/aerror.h"

MCFidpMsgList::MCFidpMsgList()
{
    HeadLink = nullptr;
    Tail = nullptr;
    Count = 0;
}

MCFidpMsgList::~MCFidpMsgList()
{
    while (HeadLink != nullptr)
    {
        TossHead();
    }
}

void MCFidpMsgList::Add(MCFidpMessage* msg)
{
    Assert(msg != nullptr, 0, " Tried to add a NULL Message to the list ");
    MCFidpMsgLink* link = new MCFidpMsgLink(msg);
    Assert(link != nullptr, 0, " Tried to add a NULL link to the list ");

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
    Size();
}

void MCFidpMsgList::TossHead()
{
    MCFidpMsgLink* link = HeadLink;

    if (link != nullptr)
    {
        HeadLink = link->Next;
        delete link;
        Count--;
        Size();
    }
}

MCFidpMessage* MCFidpMsgList::Head()
{
    if (HeadLink == nullptr)
    {
        return nullptr;
    }

    return HeadLink->Message;
}

int MCFidpMsgList::Size()
{
    int links = 0;

    for (MCFidpMsgLink* link = HeadLink; link != nullptr; link = link->Next)
    {
        links++;
    }

    if (links != Count)
    {
        char message[1024];
        std::snprintf(message, sizeof(message), "Msg List Trashed:  Cnt: %d  Size: %d", links, Count);
        Fatal(links, message);
    }

    return Count;
}
