#include "stdafx.h"
#include "linkup/linkedlist.h"
#include "lib/aerror.h"

FIDPMsgList::FIDPMsgList()
{
    head = nullptr;
    tail = nullptr;
    count = 0;
}

FIDPMsgList::~FIDPMsgList()
{
    while (head != nullptr)
    {
        TossHead();
    }
}

void FIDPMsgList::Add(FIDPMessage* msg)
{
    Assert(msg != nullptr, 0, " Tried to add a NULL Message to the list ");
    FIDPMsgLink* link = new FIDPMsgLink(msg);
    Assert(link != nullptr, 0, " Tried to add a NULL link to the list ");

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
    Size();
}

void FIDPMsgList::TossHead()
{
    FIDPMsgLink* link = head;

    if (link != nullptr)
    {
        head = link->next;
        delete link;
        count--;
        Size();
    }
}

FIDPMessage* FIDPMsgList::Head()
{
    if (head == nullptr)
    {
        return nullptr;
    }

    return head->message;
}

int FIDPMsgList::Size()
{
    int links = 0;

    for (FIDPMsgLink* link = head; link != nullptr; link = link->next)
    {
        links++;
    }

    if (links != count)
    {
        char message[1024];
        std::snprintf(message, sizeof(message), "Msg List Trashed:  Cnt: %d  Size: %d", links, count);
        Fatal(links, message);
    }

    return count;
}
