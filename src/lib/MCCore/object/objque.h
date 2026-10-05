#pragma once

#include "lib/cident.h"
#include "object/baseobj.h"

class ObjectEvent;
/// <summary>One record of ObjectQueueNode::makeObjDataBlock: a building's type, place and damage.</summary>
/// <remarks>11 bytes, packed.</remarks>
#pragma pack(push, 1)
struct ObjData
{
    /// <summary>The building's type number; -1 for a non-building, and for the end record.</summary>
    int16_t objTypeNum = 0; // +0x00
    /// <summary>Building::pixelOffsetX.</summary>
    int16_t pixelOffsetX = 0; // +0x02
    /// <summary>Building::pixelOffsetY.</summary>
    int16_t pixelOffsetY = 0; // +0x04
    /// <summary>Building::vertexNumber.</summary>
    int16_t vertexNumber = 0; // +0x06
    /// <summary>Building::blockNumber.</summary>
    int16_t blockNumber = 0; // +0x08
    /// <summary>The building's damage, truncated.</summary>
    uint8_t damage = 0; // +0x0a
};
#pragma pack(pop)

/// <summary>
/// A named list of objects (a terrain block's "TBlk%d"/"RBlk%d" objects, the clan and Inner Sphere mechs, the
/// weapons...): a singly linked list through <see cref="BaseObject::next"/>, and itself a link of the
/// <see cref="ObjectQueue"/>'s list of lists.
/// </summary>
/// <remarks>Original source: <c>object\objque.cpp</c>, <c>object\objque.h</c>; 0x18 bytes. The name is the
/// <see cref="IDString"/> base.</remarks>
struct ObjectQueueNode : public IDString
{
    /// <summary>A list named <paramref name="newId"/> (first seven characters), for no terrain block.</summary>
    /// <remarks>MCX.EXE @ 0x0068e220</remarks>
    explicit ObjectQueueNode(const char* newId);
    /// <summary>An unnamed, empty list (the original's <c>operator new</c> + zeroed id before <see cref="init"/>).</summary>
    ObjectQueueNode() = default;

    /// <summary>Names the list, empties it and sets the terrain block its objects belong to (-1 for none).</summary>
    /// <remarks>MCX.EXE @ 0x0068e1f0</remarks>
    void init(const char* newId, int32_t newBlockNumber);
    /// <summary>Deletes every object of the list.</summary>
    /// <remarks>MCX.EXE @ 0x0068ea90</remarks>
    void destroy();
    /// <summary>Appends <paramref name="object"/>; returns it.</summary>
    /// <remarks>MCX.EXE @ 0x00674550 (inline in <c>object\objque.h</c>)</remarks>
    BaseObject* addNode(BaseObject* object);
    /// <summary>Unlinks <paramref name="object"/>, whose predecessor is <paramref name="prev"/> (null for the head).</summary>
    /// <remarks>MCX.EXE @ 0x00737b50 (inline in <c>object\objque.h</c>)</remarks>
    void removeNode(BaseObject* prev, BaseObject* object);
    /// <summary>Unlinks and deletes <paramref name="object"/>; 0 when it isn't in the list.</summary>
    /// <remarks>MCX.EXE @ 0x0068ead0</remarks>
    int remove(BaseObject* object);
    /// <summary>
    /// Steps <paramref name="current"/> to the next object (the first when null) and returns it; null at the end.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006c2ae0 (inline in <c>object\objque.h</c>)</remarks>
    BaseObject* Traverse(BaseObject*& current);
    /// <summary>
    /// Renders the list's objects when its terrain block is in use, and terrain (T/RBlk) or other objects are being
    /// rendered; stops when a render restart or the drawn-object limit is hit.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0068eb40</remarks>
    void render();
    /// <summary>
    /// Updates the list's objects likewise; an object whose update returns 0 and that has a type is removed and
    /// deleted. Counts the objects in <see cref="ObjectQueue::objectsInList"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0068ec10</remarks>
    void update();
    /// <summary>
    /// The object under a mouse event's cursor: its appearance's screen box, or for an invisible
    /// MiscTerrainObject a box below its vertex. A disabled or destroyed object is passed over when <paramref name="skipDisabled"/>, else
    /// returned. Null when the list's terrain block isn't in use.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0068ed00</remarks>
    BaseObject* findObjectFromEvent(ObjectEvent* event, int skipDisabled);
    /// <summary>Sends <paramref name="event"/> to the object under its cursor; returns that object.</summary>
    /// <remarks>MCX.EXE @ 0x0068f2b0</remarks>
    BaseObject* handleEvent(ObjectEvent* event);
    /// <summary>
    /// The live object nearest <paramref name="position"/> within its type's extent and closer than
    /// <paramref name="distance"/>, which is lowered to its distance.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0068f2e0</remarks>
    BaseObject* findObject(vector_3d position, float& distance);
    /// <summary>
    /// Fills <paramref name="data"/> (0x898 bytes, first set to 0xff) with an 11-byte record per object and an
    /// end record; returns the number of objects.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0068f380</remarks>
    int32_t makeObjDataBlock(ObjData* data);

    /// <summary>The next list of the queue.</summary>
    ObjectQueueNode* next = nullptr; // +0x08
    /// <summary>The first object.</summary>
    BaseObject* head = nullptr; // +0x0c
    /// <summary>The last object.</summary>
    BaseObject* tail = nullptr; // +0x10
    /// <summary>The terrain block the objects belong to (checked against usedBlockList); -1 for none.</summary>
    int32_t blockNumber = -1; // +0x14
};

/// <summary>Every list of objects in the game: <see cref="objectList"/>.</summary>
/// <remarks>Original source: <c>object\objque.cpp</c>, <c>object\objque.h</c>; 8 bytes.</remarks>
class ObjectQueue
{
public:
    /// <summary>Appends the list <paramref name="node"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0068e260 (inline in <c>object\objque.h</c>)</remarks>
    void addList(ObjectQueueNode* node);
    /// <summary>The list named <paramref name="listId"/>, or null.</summary>
    /// <remarks>MCX.EXE @ 0x0068e290</remarks>
    ObjectQueueNode* findList(const char* listId);
    /// <summary>Renders every non-empty list until a render restart or the drawn-object limit.</summary>
    /// <remarks>MCX.EXE @ 0x0068e520</remarks>
    void render();
    /// <summary>Updates every non-empty list.</summary>
    /// <remarks>MCX.EXE @ 0x0068e550</remarks>
    void update();
    /// <summary>The object under a mouse event's cursor: the player's mechs first, then every list.</summary>
    /// <remarks>MCX.EXE @ 0x0068e580</remarks>
    BaseObject* findObjectFromEvent(ObjectEvent* event);
    /// <summary>Sends <paramref name="event"/> to the first object found under its cursor.</summary>
    /// <remarks>MCX.EXE @ 0x0068e5f0</remarks>
    BaseObject* handleEvent(ObjectEvent* event);
    /// <summary>Sends <paramref name="event"/> to the object with part id <paramref name="partId"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0068e620</remarks>
    BaseObject* handleEvent(uint32_t partId, ObjectEvent* event);
    /// <summary>
    /// The object nearest <paramref name="position"/> (within its extent and 100000) over every list.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0068e650</remarks>
    BaseObject* findObject(vector_3d position);
    /// <summary>The first object whose type's id is <paramref name="typeId"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0068e6c0</remarks>
    BaseObject* findObjectId(int32_t typeId);
    /// <summary>
    /// The object with part id <paramref name="partId"/>: a mover from the part table (below 0x1000), else a terrain
    /// object from its block's TBlk or RBlk list, else from the first list.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0068e720</remarks>
    BaseObject* findObjectFromPart(int32_t partId);
    /// <summary>
    /// The object after <paramref name="current"/> (the first when null) in an ABL group: 500 the Inner Sphere's
    /// awake mechs, 501 the clan's, 502 the Inner Sphere's asleep, 1..32 / 329..360 / 165..196 by commander.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0068e890</remarks>
    BaseObject* findObjectInGroup(BaseObject* current, int32_t groupId);
    /// <summary>
    /// Steps <paramref name="current"/> through every object of every list (the first when null); null at the end.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00737b90 (inline in <c>object\objque.h</c>)</remarks>
    BaseObject* traverse(BaseObject*& current);

    /// <summary>The first list.</summary>
    ObjectQueueNode* head = nullptr; // +0x00
    /// <summary>The last list.</summary>
    ObjectQueueNode* tail = nullptr; // +0x04

    /// <summary>Objects updated this frame (counted by ObjectQueueNode::update).</summary>
    static int32_t objectsInList;
};

/// <summary>Whether terrain block <paramref name="blockNumber"/> is in usedBlockList (in use this frame).</summary>
/// <remarks>MCX.EXE @ 0x0068e4c0</remarks>
int blockInList(int32_t blockNumber);

/// <summary>Whether ObjectQueueNode::update updates the non-terrain lists (1 by default).</summary>
extern int updateObjects;
/// <summary>Whether ObjectQueueNode::update updates the TBlk/RBlk lists.</summary>
extern int updateTerrainObjects;
/// <summary>Whether ObjectQueueNode::render renders the non-terrain lists (1 by default).</summary>
extern int renderObjects;
/// <summary>Whether ObjectQueueNode::render renders the TBlk/RBlk lists (1 by default).</summary>
extern int renderTerrainObjects;
/// <summary>Set when the drawn-object limit is reached: rendering stops.</summary>
extern int MaxObjectsDrawn;
