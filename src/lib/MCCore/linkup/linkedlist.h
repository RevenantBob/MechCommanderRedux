#pragma once

// Original source: mcx\linkup\linkedlist.cpp (the message queues) and linkedlist.hpp (the FLinkedList templates).

#include "linkup/linkedlist.hpp"

class FIDPMessage;

/// <summary>One link of an <see cref="FIDPMsgList"/>.</summary>
/// <remarks>Original source: <c>linkup\linkedlist.cpp</c>, 0xc bytes (vtable, next, message).</remarks>
class FIDPMsgLink
{
public:
    /// <summary>A link holding <paramref name="msg"/>, not yet chained.</summary>
    /// <remarks>Inlined in FIDPMsgList::Add.</remarks>
    explicit FIDPMsgLink(FIDPMessage* msg) : next(nullptr), message(msg) {}

    /// <summary>Unchains the link (the message is not deleted).</summary>
    /// <remarks>MCX.EXE @ 0x0074ce90 (vector deleting destructor)</remarks>
    virtual ~FIDPMsgLink() { next = nullptr; }

    FIDPMsgLink(const FIDPMsgLink&) = delete;
    FIDPMsgLink& operator=(const FIDPMsgLink&) = delete;

    FIDPMsgLink* next;    // +0x4
    FIDPMessage* message; // +0x8
};

/// <summary>
/// A FIFO queue of <see cref="FIDPMessage"/> pointers: the SessionManager's free, system, application, and outgoing
/// message queues. It owns its links, not the messages.
/// </summary>
/// <remarks>Original source: <c>linkup\linkedlist.cpp</c>, 0xc bytes, no vtable (allocated with the global new).</remarks>
class FIDPMsgList
{
public:
    /// <remarks>MCX.EXE @ 0x0074cdd0</remarks>
    FIDPMsgList();
    /// <summary>Drops every link (<see cref="TossHead"/> until empty).</summary>
    /// <remarks>MCX.EXE @ 0x0074cde0</remarks>
    ~FIDPMsgList();

    FIDPMsgList(const FIDPMsgList&) = delete;
    FIDPMsgList& operator=(const FIDPMsgList&) = delete;

    /// <summary>Appends <paramref name="msg"/> (asserts it isn't null).</summary>
    /// <remarks>MCX.EXE @ 0x0074ce00</remarks>
    void Add(FIDPMessage* msg);

    /// <summary>Removes the first link (the message is not deleted).</summary>
    /// <remarks>MCX.EXE @ 0x0074cec0</remarks>
    void TossHead();

    /// <summary>The first message, or null.</summary>
    /// <remarks>MCX.EXE @ 0x0074cef0</remarks>
    FIDPMessage* Head();

    /// <summary>The number of messages; fatal ("Msg List Trashed") when the links don't add up to the count.</summary>
    /// <remarks>MCX.EXE @ 0x0074cf00</remarks>
    int Size();

    FIDPMsgLink* tail; // +0x0
    int32_t count;     // +0x4
    FIDPMsgLink* head; // +0x8
};
