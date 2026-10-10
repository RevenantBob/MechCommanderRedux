#pragma once

#include "object/MCObjectList.h"

/// <summary>
/// A set of object lists: the game's (<see cref="ObjectList"/>), whose first list "DEFAULT" takes the effects and
/// other loose objects, and the scenario's objects waiting for the script.
/// </summary>
/// <remarks>Original source: <c>object\objque.cpp</c>, <c>object\objque.h</c> (a list of <c>ObjectQueueNode</c>s).</remarks>
class MCObjectQueue
{
public:
    /// <summary>The name of the first list.</summary>
    static constexpr std::string_view DefaultListName = "DEFAULT";

    /// <summary>A queue with its first list, "DEFAULT".</summary>
    MCObjectQueue();
    /// <summary>Deletes the lists, first to last, with their objects.</summary>
    ~MCObjectQueue();
    MCObjectQueue(const MCObjectQueue&) = delete;
    MCObjectQueue& operator=(const MCObjectQueue&) = delete;

    /// <summary>Appends <paramref name="list"/>; returns it.</summary>
    MCObjectList* AddList(std::unique_ptr<MCObjectList> list);
    /// <summary>The list named <paramref name="name"/>, or null.</summary>
    MCObjectList* FindList(std::string_view name) const;
    /// <summary>The list named <paramref name="name"/>, appended (for no block) when missing.</summary>
    MCObjectList* FindOrAddList(std::string_view name);
    /// <summary>Takes <paramref name="list"/> out of the queue and deletes it with its objects.</summary>
    void DeleteList(MCObjectList* list);
    /// <summary>The first list, "DEFAULT".</summary>
    MCObjectList& DefaultList() const { return *_Lists.front(); }
    /// <summary>The lists, in order.</summary>
    const std::vector<std::unique_ptr<MCObjectList>>& Lists() const { return _Lists; }
    /// <summary>Takes <paramref name="object"/> out of whichever list holds it and deletes it; false when none
    /// does.</summary>
    bool Remove(MCBaseObject* object);
    /// <summary>Renders every non-empty list.</summary>
    void Render();
    /// <summary>Updates every non-empty list.</summary>
    void Update();
    /// <summary>The object under a mouse event's cursor: the player's mechs first, then every list.</summary>
    MCBaseObject* FindObjectFromEvent(MCObjectEvent* event);
    /// <summary>Sends <paramref name="event"/> to the first object found under its cursor.</summary>
    MCBaseObject* HandleEvent(MCObjectEvent* event);
    /// <summary>Sends <paramref name="event"/> to the object with part id <paramref name="partId"/>.</summary>
    MCBaseObject* HandleEvent(uint32_t partId, MCObjectEvent* event);
    /// <summary>
    /// The object nearest <paramref name="position"/> (within its extent and 100000) over every list.
    /// </summary>
    MCBaseObject* FindObject(MCVector3D position);
    /// <summary>The first object whose type's id is <paramref name="typeId"/>.</summary>
    MCBaseObject* FindObjectId(int32_t typeId) const;
    /// <summary>
    /// The object with part id <paramref name="partId"/>: a mover from the part table (below 0x1000), else a terrain
    /// object from its block's TBlk or RBlk list, else from the first list.
    /// </summary>
    MCBaseObject* FindObjectFromPart(int32_t partId);
    /// <summary>
    /// The object after <paramref name="current"/> (the first when null) in an ABL group: 500 the Inner Sphere's
    /// awake mechs, 501 the clan's, 502 the Inner Sphere's asleep, 1..32 / 329..360 / 165..196 by commander.
    /// </summary>
    static MCBaseObject* FindObjectInGroup(MCBaseObject* current, int32_t groupId);
    /// <summary>The first object of any list that <paramref name="match"/> accepts, or null.</summary>
    MCBaseObject* FindIf(const std::function<bool(MCBaseObject*)>& match) const;

    /// <summary>Objects updated this frame (counted by MCObjectList::Update).</summary>
    static int32_t ObjectsInList;

private:
    std::vector<std::unique_ptr<MCObjectList>> _Lists;
};
