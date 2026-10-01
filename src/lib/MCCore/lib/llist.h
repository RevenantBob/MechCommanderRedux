#pragma once

// Original source: mcx\lib\llist.cpp. A singly linked list of polymorphic links; the list owns them and deletes them
// through their virtual destructor.

/// <summary>A node of a <see cref="LinkedList"/>; derive from it to carry data (CameraNode, ...).</summary>
class Link
{
public:
    Link() = default;
    /// <remarks>MCX.EXE @ 0x0068f950 (compiled in objtype.cpp)</remarks>
    virtual ~Link() = default;

    /// <summary>The next link, or null.</summary>
    Link* next = nullptr; // +0x04
};

/// <summary>A singly linked list with head and tail.</summary>
/// <remarks>Original source: <c>lib\llist.cpp</c>.</remarks>
class LinkedList
{
public:
    LinkedList() = default;
    LinkedList(const LinkedList&) = delete;
    LinkedList& operator=(const LinkedList&) = delete;

    /// <summary>Deletes every link.</summary>
    /// <remarks>MCX.EXE @ 0x0068e1c0</remarks>
    virtual ~LinkedList() { Kill(); }

    /// <summary>Puts <paramref name="link"/> first.</summary>
    /// <remarks>MCX.EXE @ 0x0064c730</remarks>
    void AddToHead(Link* link);

    /// <summary>Puts <paramref name="link"/> last.</summary>
    /// <remarks>MCX.EXE @ 0x0064c760</remarks>
    void AddToTail(Link* link);

    /// <summary>Removes <paramref name="link"/> (see <see cref="Remove"/>) and deletes it.</summary>
    /// <remarks>MCX.EXE @ 0x0064c790</remarks>
    void Destroy(Link* link, Link* previous = nullptr);

    /// <summary>Puts <paramref name="newLink"/> after <paramref name="after"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0064c7c0</remarks>
    void InsertAfter(Link* after, Link* newLink);

    /// <summary>
    /// Unlinks <paramref name="link"/>. <paramref name="previous"/> is the link before it when the caller knows it;
    /// otherwise the list is searched.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0064c800</remarks>
    void Remove(Link* link, Link* previous = nullptr);

    /// <summary>Deletes every link and empties the list.</summary>
    /// <remarks>MCX.EXE @ 0x0064c880</remarks>
    void Kill();

    /// <summary>
    /// Steps <paramref name="link"/> through the list: from null to the head, then to each next link.
    /// </summary>
    /// <returns>Nonzero while <paramref name="link"/> is a link.</returns>
    /// <remarks>MCX.EXE @ 0x0064c8c0</remarks>
    int Traverse(Link*& link);

    /// <summary>The number of links.</summary>
    /// <remarks>MCX.EXE @ 0x0064c900</remarks>
    uint32_t Count();

protected:
    /// <summary>The first link.</summary>
    Link* head = nullptr; // +0x04
    /// <summary>The last link.</summary>
    Link* tail = nullptr; // +0x08
};
