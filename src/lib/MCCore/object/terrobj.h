#pragma once

#include "object/MCBigGameObject.h"
#include "object/MCObjectType.h"
#include "object/MCWeaponShotInfo.h"

class MCAppearance;
class MCCamera;
class MCFile;
class MCFire;
class MCGameObject;
class MCObjectEvent;

/// <summary>
/// The type of a <see cref="MCTerrainObject"/>: damage level, placement offsets, impassable area and explosion.
/// </summary>
/// <remarks>
/// Original source: <c>object\terrobj.cpp</c>, <c>object\terrobj.h</c>; 0x58 bytes. Read from the
/// "TerrainObjectData" block of its FIT.
/// </remarks>
class MCTerrainObjectType : public MCObjectType
{
public:
    /// <remarks>Inline in ObjectTypeManager::load.</remarks>
    MCTerrainObjectType() { Init(); }
    ~MCTerrainObjectType() override { Destroy(); }

    /// <summary>Sets the common type fields and the terrain object fields to their defaults.</summary>
    void Init();

    /// <summary>Makes a <see cref="MCTerrainObject"/> of this type and gives it the next object id.</summary>
    std::unique_ptr<MCBaseObject> CreateInstance() override;
    void Destroy() override;
    /// <summary>Reads the "TerrainObjectData" block, then the common type data (the FIT extent radius wins).</summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>A mover (object class below 8, not 7) running into it deals it 10 points of damage.</summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>The damage that destroys the object; 0 means it starts destroyed (FIT "DmgLevel").</summary>
    uint32_t DmgLevel = 0;
    /// <summary>Replaces the placement's pixel offset X when nonzero (FIT "BasePixelOffsetX").</summary>
    int32_t BasePixelOffsetX = 0;
    /// <summary>Replaces the placement's pixel offset Y when nonzero (FIT "BasePixelOffsetY").</summary>
    int32_t BasePixelOffsetY = 0;
    /// <summary>Replaces the pixel offset X when placing the object in the world (FIT "CollisionOffsetX").</summary>
    int32_t CollisionOffsetX = 0;
    /// <summary>Replaces the pixel offset Y when placing the object in the world (FIT "CollisionOffsetY").</summary>
    int32_t CollisionOffsetY = 0;
    /// <summary>FIT "SetImpassable".</summary>
    int32_t SetImpassable = 0;
    /// <summary>FIT "XImpasse".</summary>
    int32_t XImpasse = 0;
    /// <summary>FIT "YImpasse".</summary>
    int32_t YImpasse = 0;
    /// <summary>FIT "ExplosionDamage".</summary>
    float ExplDmg = 0;
    /// <summary>FIT "ExplosionRadius".</summary>
    float ExplRad = 0;
};

/// <summary>
/// A static object on a terrain vertex (rocks, wrecks, props): it can be damaged and set on fire, and is drawn hazed
/// by how much of it the home team can see.
/// </summary>
/// <remarks>Original source: <c>object\terrobj.cpp</c>, <c>object\terrobj.h</c>; 0xd0 bytes.</remarks>
class MCTerrainObject : public MCBigGameObject
{
public:
    /// <summary>Sets the object's fields to their defaults.</summary>
    /// <remarks>Inline in TerrainObjectType::createInstance (0x00698400).</remarks>
    MCTerrainObject();
    ~MCTerrainObject() override { Destroy(); }

    /// <summary>Does nothing.</summary>
    void Init() override;
    /// <summary>Makes the VFX appearance; a type with damage level 0 starts destroyed.</summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Frees the appearance.</summary>
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>
    /// On the first update, places the object in the world from its block, vertex and offsets, and measures its
    /// extent radius from the appearance when the type has none.
    /// </summary>
    int32_t Update() override;
    /// <summary>Draws the object hazed by how much of it is revealed, with its fire.</summary>
    void Render() override;
    MCAppearance* GetAppearance() override { return Appearance; }
    /// <summary>Handles the ABL events that set (0x1c/0x1e) and clear (0x1d/0x1f) the flags at +0x28 and +0x2c.</summary>
    int32_t HandleEvent(MCObjectEvent* event) override;
    /// <summary>Returns the block and vertex the object stands on.</summary>
    void GetBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) override;
    /// <summary>Applies a weapon hit (as whole damage through <see cref="setDamage(int32_t)"/>) and sets it burning.</summary>
    int32_t HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    void KillFireObject() override { FireObject = nullptr; }
    /// <summary>
    /// Sets the pixel offsets from <paramref name="offset"/> (the type's base pixel offsets win when nonzero) and the
    /// vertex and block numbers from <paramref name="numbers"/>.
    /// </summary>
    void SetTerrainPosition(MCVector2D& offset, MCVector2D& numbers) override;

    /// <summary>Sets the damage from a whole number.</summary>
    virtual void SetDamage(int32_t newDamage);
    using MCBigGameObject::SetDamage;

    /// <summary>Projects the object for <paramref name="cam"/>; true when on screen (stamps the render turn).</summary>
    int IsVisible(MCCamera* cam);
    /// <summary>Sets the object on fire (making the type's explosion object as its fire) and adds burn time.</summary>
    void LightOnFire(float timeToBurn);

    /// <summary>Set until the first update has placed the object in the world.</summary>
    int32_t JustCreated = 0;
    /// <summary>The object's VFX appearance.</summary>
    MCAppearance* Appearance = nullptr;
    /// <summary>The pixel offset X of the object from its vertex.</summary>
    int32_t PixelOffsetX = 0;
    /// <summary>The pixel offset Y of the object from its vertex.</summary>
    int32_t PixelOffsetY = 0;
    /// <summary>The terrain vertex within its block.</summary>
    int32_t VertexNumber = 0;
    /// <summary>The terrain block.</summary>
    int32_t BlockNumber = 0;
    /// <summary>The map cell column of its vertex.</summary>
    int32_t CellColumn = 0;
    /// <summary>The map cell row of its vertex.</summary>
    int32_t CellRow = 0;
    /// <summary>The world X of its vertex.</summary>
    float VertexWorldX = 0;
    /// <summary>The world Y of its vertex.</summary>
    float VertexWorldY = 0;
    /// <summary>The elevation of its map cell, in meters.</summary>
    float CellElevation = 0;
    /// <summary>Set while the object is burning.</summary>
    int32_t Burning = 0;
    /// <summary>The fire burning on the object.</summary>
    MCFire* FireObject = nullptr;
};
