#pragma once

#include "platform/MCBlockStore.h"

#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCLinkedList.h"

class MCBaseObject;
class MCObjectType;
class MCFile;
class MCFitIniFile;
class MCGameObject;
class MCPacketFile;

/// <summary>
/// The id the next object made gets (<see cref="MCBaseObject::IdNumber"/>); every <c>createInstance</c> takes it and
/// counts up.
/// </summary>
/// <remarks>globals_by_file.md lists it under <c>object\artlry.cpp</c> (its heaviest user); it is defined with the
/// object types, in objtype.cpp.</remarks>
extern uint32_t NextIdNumber;
/// <summary>Where the object packet file is found ("data\objects\" to begin with).</summary>
extern char ObjectPath[80];

// Link (8 bytes) is declared in lib/llist.h; objtype.cpp only emitted its destructor.

/// <summary>A link of the <see cref="MCObjectTypeManager"/>'s list: one loaded object type.</summary>
/// <remarks>Original source: <c>object\objtype.cpp</c>; 0xc bytes.</remarks>
struct MCObjectTypeNode : public MCLink
{
    ~MCObjectTypeNode() override {}

    /// <summary>The type this link holds.</summary>
    MCObjectType* ObjType = nullptr;
};

/// <summary>
/// The common part of every object type: what it looks like, what it leaves behind when destroyed or when it
/// explodes, its size, icon and alignment. A type is loaded once from the object packet file (<c>OBJECT.PAK</c>) and
/// shared, reference counted, by every object made from it.
/// </summary>
/// <remarks>
/// Original source: <c>object\objtype.h</c>, <c>object\objtype.cpp</c>. The FIT block is "ObjectType".
/// </remarks>
class MCObjectType
{
public:
    /// <summary>Resets the fields (inline in the original's header, as the derived constructors show).</summary>
    MCObjectType() { Init(); }

    /// <summary>
    /// Reads the type from its packet (<paramref name="objFile"/>, <paramref name="fileSize"/> bytes); the base reads
    /// nothing.
    /// </summary>
    virtual int32_t Init(MCFile* objFile, uint32_t fileSize) { return 0; }
    virtual ~MCObjectType() { Destroy(); }
    virtual void Destroy() {}
    /// <summary>Makes a new object of this type and gives it the next id number.</summary>
    virtual MCBaseObject* CreateInstance();
    /// <summary>
    /// What happens when <paramref name="collidee"/> (of this type) and <paramref name="collider"/> touch. Nonzero
    /// lets the collision proceed.
    /// </summary>
    virtual int HandleCollision(MCGameObject* collidee, MCGameObject* collider) { return 1; }
    /// <summary>
    /// <paramref name="collidee"/> (of this type) was destroyed by <paramref name="collider"/>: makes the type's
    /// explosion at its position.
    /// </summary>
    virtual int HandleDestruction(MCGameObject* collidee, MCGameObject* collider);

    /// <summary>Sets every field to its default (no objects, appearance or icon).</summary>
    void Init()
    {
        TypeClass = -1;
        DestroyedObject = -1;
        ExplosionObject = -1;
        AppearName = 0;
        ExtentRadius = 0.0f;
        KeepMe = 0;
        IconNumber = -1;
    }

    /// <summary>
    /// Reads the "ObjectType" block: Type, Appearance, ExplosionObject, DestroyedObject, ExtentRadius, and the
    /// optional KeepMe, IconNumber and Alignment.
    /// </summary>
    int32_t Init(MCFitIniFile* typeFile);
    /// <summary>
    /// Makes the type's explosion object at <paramref name="position"/>; with a nonzero <paramref name="radius"/>
    /// it also gets that radius and <paramref name="damage"/>. The explosion goes on the end of the object list.
    /// </summary>
    void CreateExplosion(MCVector3D& position, float damage, float radius);

    /// <summary>The type's number: its packet in the object file.</summary>
    int32_t ObjTypeNum = 0;
    /// <summary>How many objects of this type exist; the type is freed when the last goes (unless kept).</summary>
    int32_t NumUsers = 0;
    /// <summary>The FIT's "Type".</summary>
    int32_t TypeClass = -1;
    /// <summary>The type number of what the object leaves when destroyed (-1: nothing).</summary>
    int32_t DestroyedObject = -1;
    /// <summary>The type number of the explosion the object makes (-1: none).</summary>
    int32_t ExplosionObject = -1;
    /// <summary>The appearance type id (FIT "Appearance").</summary>
    uint32_t AppearName = 0;
    /// <summary>The object's extent (collision) radius.</summary>
    float ExtentRadius = 0.0f;
    /// <summary>Nonzero keeps the type loaded when no object uses it.</summary>
    int32_t KeepMe = 0;
    /// <summary>The object's icon (-1: none).</summary>
    int32_t IconNumber = -1;
    /// <summary>The alignment (team side) objects of this type start with (FIT "Alignment").</summary>
    int32_t TeamId = 0;
};

/// <summary>
/// Loads and keeps the object types: a list of <see cref="MCObjectTypeNode"/>s, the object packet file, and the two
/// block stores for type and object data that has no other owner yet.
/// </summary>
/// <remarks>Original source: <c>object\objtype.cpp</c>; 0xc bytes (the LinkedList).</remarks>
class MCObjectTypeManager : public MCLinkedList
{
public:
    /// <summary>
    /// Opens the object packet file <paramref name="objectFileName"/>.pak. The two heap sizes are ignored (the
    /// original's type and object heaps are gone).
    /// </summary>
    int32_t Init(char* objectFileName, int32_t objectTypeCacheSize, int32_t objectCacheSize);
    /// <summary>Closes the packet file, deletes the types still loaded and empties both block stores.</summary>
    void Destroy();
    /// <summary>Adds a loaded type to the list.</summary>
    void Add(MCObjectType* objType);
    /// <summary>Releases the <paramref name="index"/>th type of the list.</summary>
    void Remove(int32_t index);
    /// <summary>Releases <paramref name="objType"/>.</summary>
    void Remove(MCObjectType* objType);
    /// <summary>
    /// Releases one user of the node's type; the type is deleted and unlinked when it has no users left and isn't
    /// kept.
    /// </summary>
    void Remove(MCObjectTypeNode* node);
    /// <summary>The loaded type numbered <paramref name="objTypeNum"/>, or null.</summary>
    MCObjectType* Find(int32_t objTypeNum);
    /// <summary>The <paramref name="index"/>th type of the list, or null.</summary>
    MCObjectType* Element(int32_t index);
    /// <summary>Makes a new object of type <paramref name="objTypeNum"/>, loading the type if needed.</summary>
    MCBaseObject* Get(int32_t objTypeNum);
    /// <summary>
    /// Loads type <paramref name="objTypeNum"/> from the packet file: reads its "ObjectClass" block to pick the
    /// class, then lets the new type read its packet. <paramref name="keepMe"/> marks it kept and preloads its
    /// appearance and explosion.
    /// </summary>
    MCObjectType* Load(int32_t objTypeNum, int keepMe);

    /// <summary>The object packet file every type is read from.</summary>
    static MCPacketFile* ObjectFile;
    /// <summary>
    /// Type data the types load (shapes, hot spot and stage tables): the original's type heap, which freed whatever
    /// a type left when the object system stopped.
    /// </summary>
    static MCBlockStore ObjectTypeCache;
    /// <summary>
    /// Object data with no single owner (the camera's pause and asked shapes): the original's object heap.
    /// </summary>
    static MCBlockStore ObjectCache;
};
