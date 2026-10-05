#pragma once

#include "lib/cvmath.h"

class Appearance;
class ObjectEvent;
class ObjectType;

/// <summary>
/// What an object is (<see cref="BaseObject::objectClass"/>). The values are the ones each class's init stores; the
/// names are the port's. Values no init in MCX.EXE stores (1, 0xb-0xd, 0xf, 0x14, 0x17) have no name.
/// </summary>
enum ObjectClass : int32_t
{
    BASEOBJECT = 0,
    BATTLEMECH = 2,
    GROUNDVEHICLE = 3,
    ELEMENTAL = 4,
    EXPLOSION = 5,
    FIRE = 6,
    ARTILLERY = 7,
    /// <summary>A <c>Mover</c> that is not yet a mech, vehicle or elemental (set by <c>Mover::init</c>).</summary>
    MOVER = 8,
    GAMEOBJECT = 9,
    BIGGAMEOBJECT = 10,
    LASER = 0xe,
    BUILDING = 0x10,
    SMOKE = 0x11,
    BULLET = 0x12,
    DEBRIS = 0x13,
    TREE = 0x15,
    TERRAINOBJECT = 0x16,
    MISCTERRAINOBJECT = 0x18,
    JET = 0x19,
    PROJECTILELASER = 0x1a,
    TREEBUILDING = 0x1b,
    CAMERADRONE = 0x1c,
    TRAINCAR = 0x1d,
    TURRET = 0x1e,
    GATE = 0x1f,
    LIGHT = 0x20
};

/// <summary>
/// The root of every object in the world: its class, id, part id, and the link that chains it into an
/// <c>ObjectQueue</c>. Objects are made by their
/// <see cref="ObjectType"/>'s <c>createInstance</c>.
/// </summary>
/// <remarks>
/// Original source: <c>object\baseobj.h</c>, <c>object\baseobj.cpp</c>; 0x14 bytes. Every class's destructor calls its
/// own <see cref="destroy"/> (the original's pattern), so <c>~X() { destroy(); }</c> tears down level by level.
/// </remarks>
class BaseObject
{
public:
    /// <summary>Clears the fields (inline in the original's header).</summary>
    BaseObject() { init(); }

    /// <summary>Sets the object up for its type; the base only resets the fields.</summary>
    /// <remarks>MCX.EXE @ 0x0064e220</remarks>
    virtual int32_t init(ObjectType* objType)
    {
        init();
        return 0;
    }

    /// <summary>Resets the fields: no class, id or link, part id -1.</summary>
    /// <remarks>MCX.EXE @ 0x0064e200</remarks>
    virtual void init()
    {
        objectClass = BASEOBJECT;
        idNumber = 0;
        partId = -1;
        next = nullptr;
    }

    /// <summary>Releases what the object holds (tells the object watchers it is gone).</summary>
    /// <remarks>MCX.EXE @ 0x00651a10</remarks>
    virtual void destroy();
    /// <remarks>MCX.EXE @ 0x0064e330 (vector deleting destructor)</remarks>
    virtual ~BaseObject() { destroy(); }

    /// <summary>The object's type (none for a bare BaseObject).</summary>
    /// <remarks>MCX.EXE @ 0x0064e230</remarks>
    virtual ObjectType* getObjectType() { return nullptr; }
    /// <remarks>MCX.EXE @ 0x0064e240</remarks>
    virtual int32_t kill() { return 0; }
    /// <summary>Runs one frame of the object; nonzero keeps it in the world.</summary>
    /// <remarks>MCX.EXE @ 0x0064e250</remarks>
    virtual int32_t update() { return 0; }
    /// <summary>Draws the object.</summary>
    /// <remarks>MCX.EXE @ 0x0064e260 (an empty function Ghidra left unnamed)</remarks>
    virtual void render() {}
    /// <remarks>MCX.EXE @ 0x0064e270</remarks>
    virtual Appearance* getAppearance() { return nullptr; }
    /// <remarks>MCX.EXE @ 0x0064e280</remarks>
    virtual int32_t handleEvent(ObjectEvent* event) { return 0; }
    /// <summary>The world position of hot spot <paramref name="hotSpot"/> of the object's appearance.</summary>
    /// <remarks>MCX.EXE @ 0x0064e290</remarks>
    virtual vector_3d getPositionFromHS(uint32_t hotSpot);
    /// <remarks>MCX.EXE @ 0x0064e2b0</remarks>
    virtual int underPlayerControl() { return 0; }
    /// <remarks>MCX.EXE @ 0x0064e2c0</remarks>
    virtual int32_t getGroupId() { return -1; }
    /// <remarks>MCX.EXE @ 0x0064e2d0</remarks>
    virtual int getUseMe() { return 0; }
    /// <remarks>MCX.EXE @ 0x0064e2e0</remarks>
    virtual void setAwake(int awake) {}
    /// <remarks>MCX.EXE @ 0x0064e2f0</remarks>
    virtual void setPartId(int32_t newPartId) { partId = newPartId; }
    /// <summary>Resolves collisions with the terrain objects around the object.</summary>
    /// <remarks>MCX.EXE @ 0x0064e300</remarks>
    virtual void handleStaticCollision() {}
    /// <summary>The terrain block and vertex the object stands on (-1, -1 for a bare BaseObject).</summary>
    /// <remarks>MCX.EXE @ 0x0064e310</remarks>
    virtual void getBlockAndVertexNumber(int32_t& blockNumber, int32_t& vertexNumber)
    {
        blockNumber = -1;
        vertexNumber = -1;
    }

    /// <summary>What the object is.</summary>
    ObjectClass objectClass{}; // +0x04
    /// <summary>The unique id <c>ObjectType::createInstance</c> gives it (from <c>NextIdNumber</c>).</summary>
    uint32_t idNumber = 0; // +0x08
    /// <summary>The object's part id in the scenario (-1 when it has none).</summary>
    int32_t partId = 0; // +0x0c
    /// <summary>The next object in the <c>ObjectQueue</c> list that holds it.</summary>
    BaseObject* next = nullptr; // +0x10
};
