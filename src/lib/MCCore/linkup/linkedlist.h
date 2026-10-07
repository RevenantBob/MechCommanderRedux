#pragma once

// Original source: mcx\linkup\linkedlist.cpp (the message queues) and linkedlist.hpp (the FLinkedList templates).

#include "linkup/linkedlist.hpp"

class MCFidpMessage;

/// <summary>One link of an <see cref="MCFidpMsgList"/>.</summary>
/// <remarks>Original source: <c>linkup\linkedlist.cpp</c>, 0xc bytes (vtable, next, message).</remarks>
class MCFidpMsgLink
{
public:
    /// <summary>A link holding <paramref name="msg"/>, not yet chained.</summary>
    /// <remarks>Inlined in FIDPMsgList::Add.</remarks>
    explicit MCFidpMsgLink(MCFidpMessage* msg) : Next(nullptr), Message(msg) {}

    /// <summary>Unchains the link (the message is not deleted).</summary>
    virtual ~MCFidpMsgLink() { Next = nullptr; }

    MCFidpMsgLink(const MCFidpMsgLink&) = delete;
    MCFidpMsgLink& operator=(const MCFidpMsgLink&) = delete;

    MCFidpMsgLink* Next = nullptr;
    MCFidpMessage* Message = nullptr;
};

/// <summary>
/// A FIFO queue of <see cref="MCFidpMessage"/> pointers: the SessionManager's free, system, application, and outgoing
/// message queues. It owns its links, not the messages.
/// </summary>
/// <remarks>Original source: <c>linkup\linkedlist.cpp</c>, 0xc bytes, no vtable (allocated with the global new).</remarks>
class MCFidpMsgList
{
public:
    MCFidpMsgList();
    /// <summary>Drops every link (<see cref="TossHead"/> until empty).</summary>
    ~MCFidpMsgList();

    MCFidpMsgList(const MCFidpMsgList&) = delete;
    MCFidpMsgList& operator=(const MCFidpMsgList&) = delete;

    /// <summary>Appends <paramref name="msg"/> (asserts it isn't null).</summary>
    void Add(MCFidpMessage* msg);

    /// <summary>Removes the first link (the message is not deleted).</summary>
    void TossHead();

    /// <summary>The first message, or null.</summary>
    MCFidpMessage* Head();

    /// <summary>The number of messages; fatal ("Msg List Trashed") when the links don't add up to the count.</summary>
    int Size();

    MCFidpMsgLink* Tail = nullptr;
    int32_t Count = 0;
    MCFidpMsgLink* HeadLink = nullptr;
};
