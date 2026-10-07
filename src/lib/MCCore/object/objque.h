#pragma once

#include "lib/MCIDString.h"
#include "object/baseobj.h"

class MCObjectEvent;
/// <summary>One record of ObjectQueueNode::makeObjDataBlock: a building's type, place and damage.</summary>
/// <remarks>11 bytes, packed.</remarks>
#pragma pack(push, 1)
struct MCObjData
{
    /// <summary>The building's type number; -1 for a non-building, and for the end record.</summary>
    int16_t ObjTypeNum = 0;
    /// <summary>Building::pixelOffsetX.</summary>
    int16_t PixelOffsetX = 0;
    /// <summary>Building::pixelOffsetY.</summary>
    int16_t PixelOffsetY = 0;
    /// <summary>Building::vertexNumber.</summary>
    int16_t VertexNumber = 0;
    /// <summary>Building::blockNumber.</summary>
    int16_t BlockNumber = 0;
    /// <summary>The building's damage, truncated.</summary>
    uint8_t Damage = 0;
};
#pragma pack(pop)

/// <summary>
/// A named list of objects (a terrain block's "TBlk%d"/"RBlk%d" objects, the clan and Inner Sphere mechs, the
/// weapons...): a singly linked list through <see cref="MCBaseObject::Next"/>, and itself a link of the
/// <see cref="MCObjectQueue"/>'s list of lists.
/// </summary>
/// <remarks>Original source: <c>object\objque.cpp</c>, <c>object\objque.h</c>; 0x18 bytes. The name is the
/// <see cref="MCIDString"/> base.</remarks>
struct MCObjectQueueNode : public MCIDString
{
    /// <summary>A list named <paramref name="newId"/> (first seven characters), for no terrain block.</summary>
    explicit MCObjectQueueNode(const char* newId);
    /// <summary>An unnamed, empty list (the original's <c>operator new</c> + zeroed id before <see cref="Init"/>).</summary>
    MCObjectQueueNode() = default;

    /// <summary>Names the list, empties it and sets the terrain block its objects belong to (-1 for none).</summary>
    void Init(const char* newId, int32_t newBlockNumber);
    /// <summary>Deletes every object of the list.</summary>
    void Destroy();
    /// <summary>Appends <paramref name="object"/>; returns it.</summary>
    MCBaseObject* AddNode(MCBaseObject* object);
    /// <summary>Unlinks <paramref name="object"/>, whose predecessor is <paramref name="prev"/> (null for the head).</summary>
    void RemoveNode(MCBaseObject* prev, MCBaseObject* object);
    /// <summary>Unlinks and deletes <paramref name="object"/>; 0 when it isn't in the list.</summary>
    int Remove(MCBaseObject* object);
    /// <summary>
    /// Steps <paramref name="current"/> to the next object (the first when null) and returns it; null at the end.
    /// </summary>
    MCBaseObject* Traverse(MCBaseObject*& current);
    /// <summary>
    /// Renders the list's objects when its terrain block is in use, and terrain (T/RBlk) or other objects are being
    /// rendered; stops when a render restart or the drawn-object limit is hit.
    /// </summary>
    void Render();
    /// <summary>
    /// Updates the list's objects likewise; an object whose update returns 0 and that has a type is removed and
    /// deleted. Counts the objects in <see cref="MCObjectQueue::ObjectsInList"/>.
    /// </summary>
    void Update();
    /// <summary>
    /// The object under a mouse event's cursor: its appearance's screen box, or for an invisible
    /// MiscTerrainObject a box below its vertex. A disabled or destroyed object is passed over when <paramref name="skipDisabled"/>, else
    /// returned. Null when the list's terrain block isn't in use.
    /// </summary>
    MCBaseObject* FindObjectFromEvent(MCObjectEvent* event, int skipDisabled);
    /// <summary>Sends <paramref name="event"/> to the object under its cursor; returns that object.</summary>
    MCBaseObject* HandleEvent(MCObjectEvent* event);
    /// <summary>
    /// The live object nearest <paramref name="position"/> within its type's extent and closer than
    /// <paramref name="distance"/>, which is lowered to its distance.
    /// </summary>
    MCBaseObject* FindObject(MCVector3D position, float& distance);
    /// <summary>
    /// Fills <paramref name="data"/> (0x898 bytes, first set to 0xff) with an 11-byte record per object and an
    /// end record; returns the number of objects.
    /// </summary>
    int32_t MakeObjDataBlock(MCObjData* data);

    /// <summary>The next list of the queue.</summary>
    MCObjectQueueNode* Next = nullptr;
    /// <summary>The first object.</summary>
    MCBaseObject* Head = nullptr;
    /// <summary>The last object.</summary>
    MCBaseObject* Tail = nullptr;
    /// <summary>The terrain block the objects belong to (checked against usedBlockList); -1 for none.</summary>
    int32_t BlockNumber = -1;
};

/// <summary>Every list of objects in the game: <see cref="ObjectList"/>.</summary>
/// <remarks>Original source: <c>object\objque.cpp</c>, <c>object\objque.h</c>; 8 bytes.</remarks>
class MCObjectQueue
{
public:
    /// <summary>Appends the list <paramref name="node"/>.</summary>
    void AddList(MCObjectQueueNode* node);
    /// <summary>The list named <paramref name="listId"/>, or null.</summary>
    MCObjectQueueNode* FindList(const char* listId);
    /// <summary>Renders every non-empty list until a render restart or the drawn-object limit.</summary>
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
    MCBaseObject* FindObjectId(int32_t typeId);
    /// <summary>
    /// The object with part id <paramref name="partId"/>: a mover from the part table (below 0x1000), else a terrain
    /// object from its block's TBlk or RBlk list, else from the first list.
    /// </summary>
    MCBaseObject* FindObjectFromPart(int32_t partId);
    /// <summary>
    /// The object after <paramref name="current"/> (the first when null) in an ABL group: 500 the Inner Sphere's
    /// awake mechs, 501 the clan's, 502 the Inner Sphere's asleep, 1..32 / 329..360 / 165..196 by commander.
    /// </summary>
    MCBaseObject* FindObjectInGroup(MCBaseObject* current, int32_t groupId);
    /// <summary>
    /// Steps <paramref name="current"/> through every object of every list (the first when null); null at the end.
    /// </summary>
    MCBaseObject* Traverse(MCBaseObject*& current);

    /// <summary>The first list.</summary>
    MCObjectQueueNode* Head = nullptr;
    /// <summary>The last list.</summary>
    MCObjectQueueNode* Tail = nullptr;

    /// <summary>Objects updated this frame (counted by ObjectQueueNode::update).</summary>
    static int32_t ObjectsInList;
};

/// <summary>Whether terrain block <paramref name="blockNumber"/> is in usedBlockList (in use this frame).</summary>
int BlockInList(int32_t blockNumber);

/// <summary>Whether ObjectQueueNode::update updates the non-terrain lists (1 by default).</summary>
extern int UpdateObjects;
/// <summary>Whether ObjectQueueNode::update updates the TBlk/RBlk lists.</summary>
extern int UpdateTerrainObjects;
/// <summary>Whether ObjectQueueNode::render renders the non-terrain lists (1 by default).</summary>
extern int RenderObjects;
/// <summary>Whether ObjectQueueNode::render renders the TBlk/RBlk lists (1 by default).</summary>
extern int RenderTerrainObjects;
