#pragma once

// Original source: mcx\lib\llist.cpp. A singly linked list of polymorphic links; the list owns them and deletes them
// through their virtual destructor.

/// <summary>A node of a <see cref="MCLinkedList"/>; derive from it to carry data (CameraNode, ...).</summary>
class MCLink
{
public:
    MCLink() = default;
    virtual ~MCLink() = default;

    /// <summary>The next link, or null.</summary>
    MCLink* Next = nullptr;
};

/// <summary>A singly linked list with head and tail.</summary>
/// <remarks>Original source: <c>lib\llist.cpp</c>.</remarks>
class MCLinkedList
{
public:
    MCLinkedList() = default;
    MCLinkedList(const MCLinkedList&) = delete;
    MCLinkedList& operator=(const MCLinkedList&) = delete;

    /// <summary>Deletes every link.</summary>
    virtual ~MCLinkedList() { Kill(); }

    /// <summary>Puts <paramref name="link"/> first.</summary>
    void AddToHead(MCLink* link);

    /// <summary>Puts <paramref name="link"/> last.</summary>
    void AddToTail(MCLink* link);

    /// <summary>Removes <paramref name="link"/> (see <see cref="Remove"/>) and deletes it.</summary>
    void Destroy(MCLink* link, MCLink* previous = nullptr);

    /// <summary>Puts <paramref name="newLink"/> after <paramref name="after"/>.</summary>
    void InsertAfter(MCLink* after, MCLink* newLink);

    /// <summary>
    /// Unlinks <paramref name="link"/>. <paramref name="previous"/> is the link before it when the caller knows it;
    /// otherwise the list is searched.
    /// </summary>
    void Remove(MCLink* link, MCLink* previous = nullptr);

    /// <summary>Deletes every link and empties the list.</summary>
    void Kill();

    /// <summary>
    /// Steps <paramref name="link"/> through the list: from null to the head, then to each next link.
    /// </summary>
    /// <returns>Nonzero while <paramref name="link"/> is a link.</returns>
    int Traverse(MCLink*& link);

    /// <summary>The number of links.</summary>
    uint32_t Count();

protected:
    /// <summary>The first link.</summary>
    MCLink* _Head = nullptr;
    /// <summary>The last link.</summary>
    MCLink* _Tail = nullptr;
};
