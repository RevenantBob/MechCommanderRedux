#pragma once

#include "lib/MCVector3D.h"

class MCBaseObject;
class MCObjectEvent;

/// <summary>
/// A named list of objects (a terrain block's "TBlk%d"/"RBlk%d" objects, the clan and Inner Sphere mechs, the
/// weapons...), in the order they were added. The list owns its objects.
/// </summary>
/// <remarks>
/// Original source: <c>object\objque.cpp</c>, <c>object\objque.h</c> (<c>ObjectQueueNode</c>, a list chained through
/// <c>BaseObject::next</c>). Objects are added and removed while the lists are walked: an update or a collision check
/// deletes the object being looked at or another one, an explosion adds one at the end. MCX.EXE's walks went on from
/// the deleted object's stale <c>next</c>; here a removed object's slot stays, empty, until <see cref="Compact"/>
/// (once a frame, before the update), so a walk goes on from it and passes the empty slots. An object added at the
/// end is reached by the walk going on, as in MCX.EXE.
/// </remarks>
class MCObjectList
{
    using Objects = std::list<std::unique_ptr<MCBaseObject>>;

public:
    /// <summary>Walks the objects as plain pointers, passing the empty slots of removed ones.</summary>
    class Iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = MCBaseObject*;
        using difference_type = std::ptrdiff_t;

        Iterator() = default;
        Iterator(Objects::const_iterator position, Objects::const_iterator end) : _Position(position), _End(end)
        {
            SkipEmpty();
        }

        MCBaseObject* operator*() const { return _Position->get(); }
        Iterator& operator++()
        {
            ++_Position;
            SkipEmpty();
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator old = *this;
            ++*this;
            return old;
        }

        bool operator==(const Iterator& other) const { return _Position == other._Position; }

    private:
        void SkipEmpty()
        {
            while (_Position != _End && *_Position == nullptr)
            {
                ++_Position;
            }
        }

        Objects::const_iterator _Position;
        Objects::const_iterator _End;
    };

    /// <summary>
    /// An empty list named <paramref name="name"/>, for terrain block <paramref name="blockNumber"/> (-1 for none).
    /// </summary>
    explicit MCObjectList(std::string_view name, int32_t blockNumber = -1);
    /// <summary>Deletes the objects, first to last.</summary>
    ~MCObjectList();
    MCObjectList(const MCObjectList&) = delete;
    MCObjectList& operator=(const MCObjectList&) = delete;

    /// <summary>The list's name.</summary>
    const std::string& Name() const { return _Name; }
    /// <summary>Whether the list holds a terrain block's objects ("TBlk%d" or "RBlk%d").</summary>
    bool IsTerrainList() const;
    Iterator begin() const { return Iterator(_Objects.begin(), _Objects.end()); }
    Iterator end() const { return Iterator(_Objects.end(), _Objects.end()); }
    /// <summary>The first object, or null.</summary>
    MCBaseObject* First() const { return begin() == end() ? nullptr : *begin(); }
    bool Empty() const { return _Count == 0; }
    /// <summary>Objects in the list.</summary>
    size_t Size() const { return _Count; }
    /// <summary>The object after <paramref name="current"/> (the first when null); null at the end or when
    /// <paramref name="current"/> isn't in the list.</summary>
    MCBaseObject* After(MCBaseObject* current) const;

    /// <summary>Appends <paramref name="object"/>; returns it.</summary>
    MCBaseObject* Add(std::unique_ptr<MCBaseObject> object);
    /// <summary>Takes <paramref name="object"/> out of the list and hands it over; null when it isn't in it.</summary>
    std::unique_ptr<MCBaseObject> Release(MCBaseObject* object);
    /// <summary>Takes <paramref name="object"/> out and deletes it; false when it isn't in the list.</summary>
    bool Remove(MCBaseObject* object);
    /// <summary>Drops the empty slots removed objects left. Only while nothing walks the list.</summary>
    void Compact();
    /// <summary>
    /// Renders the list's objects when its terrain block is in use, and terrain (T/RBlk) or other objects are being
    /// rendered.
    /// </summary>
    void Render();
    /// <summary>
    /// Updates the list's objects likewise; an object whose update returns 0 and that has a type is deleted. Counts the
    /// objects in <see cref="MCObjectQueue::ObjectsInList"/>.
    /// </summary>
    void Update();
    /// <summary>
    /// The object under a mouse event's cursor: its appearance's screen box, or for an invisible
    /// MiscTerrainObject a box below its vertex. A disabled or destroyed object is passed over when
    /// <paramref name="skipDisabled"/>, else returned. Null when the list's terrain block isn't in use.
    /// </summary>
    MCBaseObject* FindObjectFromEvent(MCObjectEvent* event, int skipDisabled);
    /// <summary>Sends <paramref name="event"/> to the object under its cursor; returns that object.</summary>
    MCBaseObject* HandleEvent(MCObjectEvent* event);
    /// <summary>
    /// The live object nearest <paramref name="position"/> within its type's extent and closer than
    /// <paramref name="distance"/>, which is lowered to its distance.
    /// </summary>
    MCBaseObject* FindObject(MCVector3D position, float& distance);
    /// <summary>The object with part id <paramref name="partId"/>, or null.</summary>
    MCBaseObject* FindPart(int32_t partId) const;

    /// <summary>The terrain block the objects belong to (checked against the blocks in use); -1 for none.</summary>
    int32_t BlockNumber = -1;

private:
    std::string _Name;
    /// <summary>The objects, and the empty slots of those removed since the last <see cref="Compact"/>.</summary>
    Objects _Objects;
    /// <summary>Objects in the list (the slots that aren't empty).</summary>
    size_t _Count = 0;
};

/// <summary>Whether terrain block <paramref name="blockNumber"/> is in use this frame (always, for -1).</summary>
int BlockInList(int32_t blockNumber);

/// <summary>Whether MCObjectList::Update updates the non-terrain lists (1 by default).</summary>
extern int UpdateObjects;
/// <summary>Whether MCObjectList::Update updates the TBlk/RBlk lists.</summary>
extern int UpdateTerrainObjects;
/// <summary>Whether MCObjectList::Render renders the non-terrain lists (1 by default).</summary>
extern int RenderObjects;
/// <summary>Whether MCObjectList::Render renders the TBlk/RBlk lists (1 by default).</summary>
extern int RenderTerrainObjects;
