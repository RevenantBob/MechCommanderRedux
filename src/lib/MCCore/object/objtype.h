#pragma once

#include "platform/MCBlockStore.h"

#include "lib/cvmath.h"
#include "lib/llist.h"

class BaseObject;
class ObjectType;
class File;
class FitIniFile;
class GameObject;
class PacketFile;

/// <summary>
/// The id the next object made gets (<see cref="BaseObject::idNumber"/>); every <c>createInstance</c> takes it and
/// counts up.
/// </summary>
/// <remarks>globals_by_file.md lists it under <c>object\artlry.cpp</c> (its heaviest user); it is defined with the
/// object types, in objtype.cpp.</remarks>
extern uint32_t NextIdNumber;
/// <summary>Where the object packet file is found ("data\objects\" to begin with).</summary>
extern char objectPath[80];

// Link (8 bytes) is declared in lib/llist.h; objtype.cpp only emitted its destructor (MCX.EXE @ 0x0068f950).

/// <summary>A link of the <see cref="ObjectTypeManager"/>'s list: one loaded object type.</summary>
/// <remarks>Original source: <c>object\objtype.cpp</c>; 0xc bytes.</remarks>
struct ObjectTypeNode : public Link
{
    /// <remarks>MCX.EXE @ 0x0068f920 (vector deleting destructor)</remarks>
    ~ObjectTypeNode() override {}

    /// <summary>The type this link holds.</summary>
    ObjectType* objType = nullptr; // +0x08
};

/// <summary>
/// The common part of every object type: what it looks like, what it leaves behind when destroyed or when it
/// explodes, its size, icon and alignment. A type is loaded once from the object packet file (<c>OBJECT.PAK</c>) and
/// shared, reference counted, by every object made from it.
/// </summary>
/// <remarks>
/// Original source: <c>object\objtype.h</c>, <c>object\objtype.cpp</c>; 0x30 bytes. Derived types' fields start at
/// +0x30. The FIT block is "ObjectType".
/// </remarks>
class ObjectType
{
public:
    /// <summary>Resets the fields (inline in the original's header, as the derived constructors show).</summary>
    ObjectType() { init(); }

    /// <summary>
    /// Reads the type from its packet (<paramref name="objFile"/>, <paramref name="fileSize"/> bytes); the base reads
    /// nothing.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0068f510</remarks>
    virtual int32_t init(File* objFile, uint32_t fileSize) { return 0; }
    /// <remarks>MCX.EXE @ 0x00690370 (vector deleting destructor)</remarks>
    virtual ~ObjectType() { destroy(); }
    /// <remarks>MCX.EXE @ 0x0068f500</remarks>
    virtual void destroy() {}
    /// <summary>Makes a new object of this type and gives it the next id number.</summary>
    /// <remarks>MCX.EXE @ 0x0068f4b0</remarks>
    virtual BaseObject* createInstance();
    /// <summary>
    /// What happens when <paramref name="collidee"/> (of this type) and <paramref name="collider"/> touch. Nonzero
    /// lets the collision proceed.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0068f6a0</remarks>
    virtual int handleCollision(GameObject* collidee, GameObject* collider) { return 1; }
    /// <summary>
    /// <paramref name="collidee"/> (of this type) was destroyed by <paramref name="collider"/>: makes the type's
    /// explosion at its position.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0068f6b0</remarks>
    virtual int handleDestruction(GameObject* collidee, GameObject* collider);

    /// <summary>Sets every field to its default (no objects, appearance or icon).</summary>
    /// <remarks>MCX.EXE @ 0x00690350</remarks>
    void init()
    {
        typeClass = -1;
        destroyedObject = -1;
        explosionObject = -1;
        appearName = 0;
        extentRadius = 0.0f;
        keepMe = 0;
        iconNumber = -1;
    }

    /// <summary>
    /// Reads the "ObjectType" block: Type, Appearance, ExplosionObject, DestroyedObject, ExtentRadius, and the
    /// optional KeepMe, IconNumber and Alignment.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0068f520</remarks>
    int32_t init(FitIniFile* typeFile);
    /// <summary>
    /// Makes the type's explosion object at <paramref name="position"/>; with a nonzero <paramref name="radius"/>
    /// it also gets that radius and <paramref name="damage"/>. The explosion goes on the end of the object list.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0068f610</remarks>
    void createExplosion(vector_3d& position, float damage, float radius);

    /// <summary>The type's number: its packet in the object file.</summary>
    int32_t objTypeNum = 0; // +0x04
    /// <summary>How many objects of this type exist; the type is freed when the last goes (unless kept).</summary>
    int32_t numUsers = 0; // +0x08
    /// <summary>The FIT's "Type".</summary>
    int32_t typeClass = -1; // +0x0c
    /// <summary>The type number of what the object leaves when destroyed (-1: nothing).</summary>
    int32_t destroyedObject = -1; // +0x10
    /// <summary>The type number of the explosion the object makes (-1: none).</summary>
    int32_t explosionObject = -1; // +0x14
    /// <summary>The appearance type id (FIT "Appearance").</summary>
    uint32_t appearName = 0; // +0x1c
    /// <summary>The object's extent (collision) radius.</summary>
    float extentRadius = 0.0f; // +0x20
    /// <summary>Nonzero keeps the type loaded when no object uses it.</summary>
    int32_t keepMe = 0; // +0x24
    /// <summary>The object's icon (-1: none).</summary>
    int32_t iconNumber = -1; // +0x28
    /// <summary>The alignment (team side) objects of this type start with (FIT "Alignment").</summary>
    int32_t teamId = 0; // +0x2c
};

/// <summary>
/// Loads and keeps the object types: a list of <see cref="ObjectTypeNode"/>s, the object packet file, and the two
/// block stores for type and object data that has no other owner yet.
/// </summary>
/// <remarks>Original source: <c>object\objtype.cpp</c>; 0xc bytes (the LinkedList).</remarks>
class ObjectTypeManager : public LinkedList
{
public:
    /// <summary>
    /// Opens the object packet file <paramref name="objectFileName"/>.pak. The two heap sizes are ignored (the
    /// original's type and object heaps are gone).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0068f6f0</remarks>
    int32_t init(char* objectFileName, int32_t objectTypeCacheSize, int32_t objectCacheSize);
    /// <summary>Closes the packet file, deletes the types still loaded and empties both block stores.</summary>
    /// <remarks>MCX.EXE @ 0x0068f850</remarks>
    void destroy();
    /// <summary>Adds a loaded type to the list.</summary>
    /// <remarks>MCX.EXE @ 0x0068f8d0</remarks>
    void add(ObjectType* objType);
    /// <summary>Releases the <paramref name="index"/>th type of the list.</summary>
    /// <remarks>MCX.EXE @ 0x0068f980</remarks>
    void remove(int32_t index);
    /// <summary>Releases <paramref name="objType"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0068f9d0</remarks>
    void remove(ObjectType* objType);
    /// <summary>
    /// Releases one user of the node's type; the type is deleted and unlinked when it has no users left and isn't
    /// kept.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0068fa20</remarks>
    void remove(ObjectTypeNode* node);
    /// <summary>The loaded type numbered <paramref name="objTypeNum"/>, or null.</summary>
    /// <remarks>MCX.EXE @ 0x0068fa60</remarks>
    ObjectType* find(int32_t objTypeNum);
    /// <summary>The <paramref name="index"/>th type of the list, or null.</summary>
    /// <remarks>MCX.EXE @ 0x0068fab0</remarks>
    ObjectType* element(int32_t index);
    /// <summary>Makes a new object of type <paramref name="objTypeNum"/>, loading the type if needed.</summary>
    /// <remarks>MCX.EXE @ 0x0068fb00</remarks>
    BaseObject* get(int32_t objTypeNum);
    /// <summary>
    /// Loads type <paramref name="objTypeNum"/> from the packet file: reads its "ObjectClass" block to pick the
    /// class, then lets the new type read its packet. <paramref name="keepMe"/> marks it kept and preloads its
    /// appearance and explosion.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0068fb50</remarks>
    ObjectType* load(int32_t objTypeNum, int keepMe);

    /// <summary>The object packet file every type is read from.</summary>
    static PacketFile* objectFile;
    /// <summary>
    /// Type data the types load (shapes, hot spot and stage tables): the original's type heap, which freed whatever
    /// a type left when the object system stopped.
    /// </summary>
    static MCBlockStore objectTypeCache;
    /// <summary>
    /// Object data with no single owner (the camera's pause and asked shapes): the original's object heap.
    /// </summary>
    static MCBlockStore objectCache;
};
