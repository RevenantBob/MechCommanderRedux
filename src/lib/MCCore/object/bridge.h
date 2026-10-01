#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class Appearance;
class File;
class Fire;
class GameObject;
class ObjectEvent;
struct _WeaponShotInfo;

/// <summary>
/// The type shared by the terrain features that can be damaged in place: bridges, forest tiles and walls
/// (heavy, medium and light). Holds each kind's damage threshold and effect ids, and the forest-edge shapes.
/// </summary>
/// <remarks>Original source: <c>object\bridge.cpp</c>, <c>object\bridge.h</c>; 0x60 bytes. Read from the "BridgeData"
/// block of its FIT.</remarks>
class MiscTerrainObjectType : public ObjectType
{
public:
    /// <remarks>Inline in <c>ObjectTypeManager::load</c>.</remarks>
    MiscTerrainObjectType() { init(); }
    /// <remarks>MCX.EXE @ 0x00690680 (vector deleting destructor)</remarks>
    ~MiscTerrainObjectType() override { destroy(); }

    /// <summary>Sets the common type defaults, every threshold to 0 and every effect id to -1.</summary>
    /// <remarks>MCX.EXE @ 0x00690630 (inline in <c>object\bridge.h</c>)</remarks>
    void init();

    /// <summary>Makes a <see cref="MiscTerrainObject"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x00653740</remarks>
    BaseObject* createInstance() override;
    /// <remarks>MCX.EXE @ 0x006538a0</remarks>
    void destroy() override;
    /// <summary>
    /// Reads the damage levels and effect ids from the "BridgeData" block, loads the ForestEdges shape file into the
    /// object type cache, then the common type data.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006538b0</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <summary>
    /// A mover (object class 2 or 3) running into a light wall crushes it: a 250-point hit on the wall.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00653b60 (unnamed in the export; vtable slot 4 of MiscTerrainObjectType)</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <remarks>MCX.EXE @ 0x00653be0</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>Damage that destroys a heavy wall (kind 7; FIT "WallDmgLevel").</summary>
    uint32_t wallDmgLevel; // +0x30
    /// <summary>Damage that destroys a medium wall (kind 8; FIT "MediumWallDmgLevel", default half the wall's).</summary>
    uint32_t mediumWallDmgLevel; // +0x34
    /// <summary>Damage that destroys a light wall (kind 9; FIT "LightWallDmgLevel").</summary>
    uint32_t lightWallDmgLevel; // +0x38
    /// <summary>Damage that destroys a bridge (kind 5; FIT "BridgeDmgLevel").</summary>
    uint32_t bridgeDmgLevel; // +0x3c
    /// <summary>Damage that burns down a forest tile (kind 6; FIT "ForestDmgLevel").</summary>
    uint32_t forestDmgLevel; // +0x40
    /// <summary>FIT "BlownEffectId".</summary>
    uint32_t blownEffectId; // +0x44
    /// <summary>FIT "NormalEffectId".</summary>
    uint32_t normalEffectId; // +0x48
    /// <summary>FIT "DamageEffectId".</summary>
    uint32_t damageEffectId; // +0x4c
    /// <summary>Object type of a wall's fire (FIT "WallFireFX").</summary>
    uint32_t wallFireFX; // +0x50
    /// <summary>Object type of a bridge's fire (FIT "BridgeFireFX").</summary>
    uint32_t bridgeFireFX; // +0x54
    /// <summary>Object type of the fire started on a forest tile (FIT "ForestFireFX").</summary>
    uint32_t forestFireFX; // +0x58
    /// <summary>The ForestEdges shape file, loaded whole into the object type cache; drawn over burnt forest edges.</summary>
    uint8_t* forestEdgeShapes; // +0x5c
};

/// <summary>
/// A damageable terrain feature at one terrain vertex: a bridge, a forest tile or a wall. It has no appearance of its
/// own; when destroyed it changes the terrain overlay tile and the move map's passability and line-of-fire bits.
/// </summary>
/// <remarks>
/// Original source: <c>object\bridge.cpp</c>, <c>object\bridge.h</c>; 0x58 bytes. Derives <see cref="GameObject"/>
/// directly (not BigGameObject): its vtable has GameObject's 113 slots and its constructor, inlined in
/// <see cref="MiscTerrainObjectType::createInstance"/>, sets only GameObject's fields before its own.
/// </remarks>
class MiscTerrainObject : public GameObject
{
public:
    /// <summary>
    /// Field defaults: just created, no fire, not destroyed, vertex and block 0, kind -1.
    /// </summary>
    /// <remarks>Inline in <see cref="MiscTerrainObjectType::createInstance"/> (MCX.EXE @ 0x00653740).</remarks>
    MiscTerrainObject();
    /// <remarks>MCX.EXE @ 0x00653850 (vector deleting destructor, unnamed in the export)</remarks>
    ~MiscTerrainObject() override { destroy(); }

    /// <remarks>MCX.EXE @ 0x006537d0 (inline in <c>object\bridge.h</c>; empty)</remarks>
    void init() override;
    /// <summary>Marks the object just created, object class 0x18, no damage.</summary>
    /// <remarks>MCX.EXE @ 0x00654cf0</remarks>
    int32_t init(ObjectType* objType) override;
    /// <remarks>MCX.EXE @ 0x00654650</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x00653810</remarks>
    int32_t kill() override { return 0; }
    /// <summary>
    /// On the first update, places the object at its vertex and marks it destroyed already when its map cell is
    /// impassable (the map says it was blown before).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00653bf0</remarks>
    int32_t update() override;
    /// <summary>
    /// Clears the passability bits of a destroyed feature once its fire is out, draws the damage bars when selected
    /// or pointed at, and the forest-edge shape over a burnt forest tile.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00653f00</remarks>
    void render() override;
    /// <remarks>MCX.EXE @ 0x00653800</remarks>
    Appearance* getAppearance() override { return nullptr; }
    /// <summary>Selection (0x1c/0x1d) and target (0x1e/0x1f) events.</summary>
    /// <remarks>MCX.EXE @ 0x00653e60</remarks>
    int32_t handleEvent(ObjectEvent* event) override;
    /// <summary>Its terrain block and vertex.</summary>
    /// <remarks>MCX.EXE @ 0x00653830 (inline in <c>object\bridge.h</c>)</remarks>
    void getBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) override;
    /// <summary>
    /// Adds the shot's damage; past the kind's threshold it explodes (and a light wall plays its crumble sound). A
    /// forest tile also catches fire.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00654ed0</remarks>
    int32_t handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <summary>Forgets its fire (the fire is going away).</summary>
    /// <remarks>MCX.EXE @ 0x006537e0 (inline in <c>object\bridge.h</c>)</remarks>
    void killFireObject() override;
    /// <remarks>MCX.EXE @ 0x006537f0</remarks>
    float getDamage() override { return damage; }
    /// <summary>
    /// Sets the damage; past the kind's threshold the feature is destroyed: new overlay tile, the move map's cell
    /// made passable or blocked, and (for a bridge) the global move-map area closed.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00654660</remarks>
    void setDamage(float newDamage) override;
    /// <remarks>MCX.EXE @ 0x00653820</remarks>
    int isBuilding() override { return 1; }
    /// <summary>Whether any of the four vertices around it is visible to the home team.</summary>
    /// <remarks>MCX.EXE @ 0x00655090</remarks>
    int isRevealed() override;
    /// <summary>Saves the line-of-fire bits of its map cell in <see cref="cellArray"/> and clears them.</summary>
    /// <remarks>MCX.EXE @ 0x00654da0</remarks>
    void clearLineOfFire() override;
    /// <summary>Puts back the line-of-fire bits saved by <see cref="clearLineOfFire"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00654e40</remarks>
    void restoreLineOfFire() override;

    using GameObject::getScreenPos;
    /// <summary>Its vertex projected through the main camera.</summary>
    /// <remarks>MCX.EXE @ 0x00653ec0</remarks>
    vector_2d getScreenPos();
    /// <summary>Draws the damage bar (green, yellow, red) above <paramref name="screenPos"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00654340</remarks>
    void drawBars(vector_2d screenPos);
    /// <summary>A 1-point hit, and more burning time for its fire if it has one.</summary>
    /// <remarks>MCX.EXE @ 0x00654d30</remarks>
    void lightOnFire(float timeToBurn);

    /// <summary>
    /// The move-map line-of-fire bits of the 3x3 sub-cells saved by <see cref="clearLineOfFire"/>, one per entry.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x007dd024 (to 0x007dd048)</remarks>
    static int32_t cellArray[9];

    /// <summary>Set by init; the first update places the object and clears it.</summary>
    int32_t justCreated; // +0x38
    /// <summary>The terrain vertex it stands on, within its block.</summary>
    int32_t vertexNumber; // +0x3c
    /// <summary>The terrain block it stands in.</summary>
    int32_t blockNumber; // +0x40
    /// <summary>Set when setDamage has swapped the overlay tile for the destroyed one; the first update clears it.</summary>
    int32_t overlayDestroyed; // +0x44
    float damage;             // +0x48
    /// <summary>
    /// Which feature it is: 5 bridge, 6 forest, 7 heavy wall, 8 medium wall, 9 light wall (set by the object block
    /// manager when it places the object).
    /// </summary>
    int32_t terrainObjectKind; // +0x4c
    /// <summary>The fire burning on it (made from the type's forest fire effect), if any.</summary>
    Fire* fireObject; // +0x50
    /// <summary>Set once destroyed: further hits are ignored and render updates the map's passability.</summary>
    int32_t destroyed; // +0x54
};
