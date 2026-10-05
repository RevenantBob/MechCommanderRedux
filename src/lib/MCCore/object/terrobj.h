#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class Appearance;
class Camera;
class File;
class Fire;
class GameObject;
class ObjectEvent;

/// <summary>
/// The type of a <see cref="TerrainObject"/>: damage level, placement offsets, impassable area and explosion.
/// </summary>
/// <remarks>
/// Original source: <c>object\terrobj.cpp</c>, <c>object\terrobj.h</c>; 0x58 bytes. Read from the
/// "TerrainObjectData" block of its FIT.
/// </remarks>
class TerrainObjectType : public ObjectType
{
public:
    /// <remarks>Inline in ObjectTypeManager::load.</remarks>
    TerrainObjectType() { init(); }
    /// <remarks>MCX.EXE @ 0x00690920 (vector deleting destructor)</remarks>
    ~TerrainObjectType() override { destroy(); }

    /// <summary>Sets the common type fields and the terrain object fields to their defaults.</summary>
    /// <remarks>MCX.EXE @ 0x006908e0 (inline in <c>object\terrobj.h</c>)</remarks>
    void init();

    /// <summary>Makes a <see cref="TerrainObject"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x00698400</remarks>
    BaseObject* createInstance() override;
    /// <remarks>MCX.EXE @ 0x006985c0</remarks>
    void destroy() override;
    /// <summary>Reads the "TerrainObjectData" block, then the common type data (the FIT extent radius wins).</summary>
    /// <remarks>MCX.EXE @ 0x006985d0</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <summary>A mover (object class below 8, not 7) running into it deals it 10 points of damage.</summary>
    /// <remarks>MCX.EXE @ 0x00698770</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <remarks>MCX.EXE @ 0x006987e0</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>The damage that destroys the object; 0 means it starts destroyed (FIT "DmgLevel").</summary>
    uint32_t dmgLevel = 0; // +0x30
    /// <summary>Replaces the placement's pixel offset X when nonzero (FIT "BasePixelOffsetX").</summary>
    int32_t basePixelOffsetX = 0; // +0x34
    /// <summary>Replaces the placement's pixel offset Y when nonzero (FIT "BasePixelOffsetY").</summary>
    int32_t basePixelOffsetY = 0; // +0x38
    /// <summary>Replaces the pixel offset X when placing the object in the world (FIT "CollisionOffsetX").</summary>
    int32_t collisionOffsetX = 0; // +0x3c
    /// <summary>Replaces the pixel offset Y when placing the object in the world (FIT "CollisionOffsetY").</summary>
    int32_t collisionOffsetY = 0; // +0x40
    /// <summary>FIT "SetImpassable".</summary>
    int32_t setImpassable = 0; // +0x44
    /// <summary>FIT "XImpasse".</summary>
    int32_t xImpasse = 0; // +0x48
    /// <summary>FIT "YImpasse".</summary>
    int32_t yImpasse = 0; // +0x4c
    /// <summary>FIT "ExplosionDamage".</summary>
    float explDmg = 0; // +0x50
    /// <summary>FIT "ExplosionRadius".</summary>
    float explRad = 0; // +0x54
};

/// <summary>
/// A static object on a terrain vertex (rocks, wrecks, props): it can be damaged and set on fire, and is drawn hazed
/// by how much of it the home team can see.
/// </summary>
/// <remarks>Original source: <c>object\terrobj.cpp</c>, <c>object\terrobj.h</c>; 0xd0 bytes.</remarks>
class TerrainObject : public BigGameObject
{
public:
    /// <summary>Sets the object's fields to their defaults.</summary>
    /// <remarks>Inline in TerrainObjectType::createInstance (0x00698400).</remarks>
    TerrainObject();
    /// <remarks>MCX.EXE @ 0x00698570 (vector deleting destructor)</remarks>
    ~TerrainObject() override { destroy(); }

    /// <summary>Does nothing.</summary>
    /// <remarks>MCX.EXE @ 0x006984a0 (unnamed in the symbols; vtable slot 1, an empty inline)</remarks>
    void init() override;
    /// <summary>Makes the VFX appearance; a type with damage level 0 starts destroyed.</summary>
    /// <remarks>MCX.EXE @ 0x00698f60</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Frees the appearance.</summary>
    /// <remarks>MCX.EXE @ 0x00698f30</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x00698540</remarks>
    int32_t kill() override { return 0; }
    /// <summary>
    /// On the first update, places the object in the world from its block, vertex and offsets, and measures its
    /// extent radius from the appearance when the type has none.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00698860</remarks>
    int32_t update() override;
    /// <summary>Draws the object hazed by how much of it is revealed, with its fire.</summary>
    /// <remarks>MCX.EXE @ 0x00698c40</remarks>
    void render() override;
    /// <remarks>MCX.EXE @ 0x00698530</remarks>
    Appearance* getAppearance() override { return appearance; }
    /// <summary>Handles the ABL events that set (0x1c/0x1e) and clear (0x1d/0x1f) the flags at +0x28 and +0x2c.</summary>
    /// <remarks>MCX.EXE @ 0x00698be0</remarks>
    int32_t handleEvent(ObjectEvent* event) override;
    /// <summary>Returns the block and vertex the object stands on.</summary>
    /// <remarks>MCX.EXE @ 0x00698550 (inline in <c>object\terrobj.h</c>)</remarks>
    void getBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) override;
    /// <summary>Applies a weapon hit (as whole damage through <see cref="setDamage(int32_t)"/>) and sets it burning.</summary>
    /// <remarks>MCX.EXE @ 0x00699110</remarks>
    int32_t handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <remarks>MCX.EXE @ 0x006984b0</remarks>
    void killFireObject() override { fireObject = nullptr; }
    /// <summary>
    /// Sets the pixel offsets from <paramref name="offset"/> (the type's base pixel offsets win when nonzero) and the
    /// vertex and block numbers from <paramref name="numbers"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006984c0 (inline in <c>object\terrobj.h</c>)</remarks>
    void setTerrainPosition(vector_2d& offset, vector_2d& numbers) override;

    /// <summary>Sets the damage from a whole number.</summary>
    /// <remarks>MCX.EXE @ 0x00698f50 (vtable slot 116)</remarks>
    virtual void setDamage(int32_t newDamage);
    using BigGameObject::setDamage;

    /// <summary>Projects the object for <paramref name="cam"/>; true when on screen (stamps the render turn).</summary>
    /// <remarks>MCX.EXE @ 0x006987f0</remarks>
    int isVisible(Camera* cam);
    /// <summary>Sets the object on fire (making the type's explosion object as its fire) and adds burn time.</summary>
    /// <remarks>MCX.EXE @ 0x00699080</remarks>
    void lightOnFire(float timeToBurn);

    /// <summary>Set until the first update has placed the object in the world.</summary>
    int32_t justCreated = 0; // +0x84
    /// <summary>The object's VFX appearance.</summary>
    Appearance* appearance = nullptr; // +0x88
    /// <summary>The pixel offset X of the object from its vertex.</summary>
    int32_t pixelOffsetX = 0; // +0x8c
    /// <summary>The pixel offset Y of the object from its vertex.</summary>
    int32_t pixelOffsetY = 0; // +0x90
    /// <summary>The terrain vertex within its block.</summary>
    int32_t vertexNumber = 0; // +0x94
    /// <summary>The terrain block.</summary>
    int32_t blockNumber = 0; // +0x98
    /// <summary>The map cell column of its vertex.</summary>
    int32_t cellColumn = 0; // +0xb4
    /// <summary>The map cell row of its vertex.</summary>
    int32_t cellRow = 0; // +0xb8
    /// <summary>The world X of its vertex.</summary>
    float vertexWorldX = 0; // +0xbc
    /// <summary>The world Y of its vertex.</summary>
    float vertexWorldY = 0; // +0xc0
    /// <summary>The elevation of its map cell, in meters.</summary>
    float cellElevation = 0; // +0xc4
    /// <summary>Set while the object is burning.</summary>
    int32_t burning = 0; // +0xc8
    /// <summary>The fire burning on the object.</summary>
    Fire* fireObject = nullptr; // +0xcc
};
