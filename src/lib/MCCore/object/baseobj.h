#pragma once

#include "lib/cvmath.h"

class MCAppearance;
class MCObjectEvent;
class MCObjectType;

/// <summary>
/// What an object is (<see cref="MCBaseObject::ObjectClass"/>). The values are the ones each class's init stores; the
/// names are the port's. Values no init in MCX.EXE stores (1, 0xb-0xd, 0xf, 0x14, 0x17) have no name.
/// </summary>
enum MCObjectClass : int32_t
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
/// <see cref="MCObjectType"/>'s <c>createInstance</c>.
/// </summary>
/// <remarks>
/// Original source: <c>object\baseobj.h</c>, <c>object\baseobj.cpp</c>; 0x14 bytes. Every class's destructor calls its
/// own <see cref="Destroy"/> (the original's pattern), so <c>~X() { destroy(); }</c> tears down level by level.
/// </remarks>
class MCBaseObject
{
public:
    /// <summary>Clears the fields (inline in the original's header).</summary>
    MCBaseObject() { Init(); }

    /// <summary>Sets the object up for its type; the base only resets the fields.</summary>
    virtual int32_t Init(MCObjectType* objType)
    {
        Init();
        return 0;
    }

    /// <summary>Resets the fields: no class, id or link, part id -1.</summary>
    virtual void Init()
    {
        ObjectClass = BASEOBJECT;
        IdNumber = 0;
        PartId = -1;
        Next = nullptr;
    }

    /// <summary>Releases what the object holds (tells the object watchers it is gone).</summary>
    virtual void Destroy();
    virtual ~MCBaseObject() { Destroy(); }

    /// <summary>The object's type (none for a bare BaseObject).</summary>
    virtual MCObjectType* GetObjectType() { return nullptr; }
    virtual int32_t Kill() { return 0; }
    /// <summary>Runs one frame of the object; nonzero keeps it in the world.</summary>
    virtual int32_t Update() { return 0; }
    /// <summary>Draws the object.</summary>
    virtual void Render() {}
    virtual MCAppearance* GetAppearance() { return nullptr; }
    virtual int32_t HandleEvent(MCObjectEvent* event) { return 0; }
    /// <summary>The world position of hot spot <paramref name="hotSpot"/> of the object's appearance.</summary>
    virtual MCVector3D GetPositionFromHS(uint32_t hotSpot);
    virtual int UnderPlayerControl() { return 0; }
    virtual int32_t GetGroupId() { return -1; }
    virtual int GetUseMe() { return 0; }
    virtual void SetAwake(int awake) {}
    virtual void SetPartId(int32_t newPartId) { PartId = newPartId; }
    /// <summary>Resolves collisions with the terrain objects around the object.</summary>
    virtual void HandleStaticCollision() {}
    /// <summary>The terrain block and vertex the object stands on (-1, -1 for a bare BaseObject).</summary>
    virtual void GetBlockAndVertexNumber(int32_t& blockNumber, int32_t& vertexNumber)
    {
        blockNumber = -1;
        vertexNumber = -1;
    }

    /// <summary>What the object is.</summary>
    MCObjectClass ObjectClass{};
    /// <summary>The unique id <c>ObjectType::createInstance</c> gives it (from <c>NextIdNumber</c>).</summary>
    uint32_t IdNumber = 0;
    /// <summary>The object's part id in the scenario (-1 when it has none).</summary>
    int32_t PartId = 0;
    /// <summary>The next object in the <c>ObjectQueue</c> list that holds it.</summary>
    MCBaseObject* Next = nullptr;
};
