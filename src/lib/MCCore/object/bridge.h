#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class MCAppearance;
class MCFile;
class MCFire;
class MCGameObject;
class MCObjectEvent;
struct MCWeaponShotInfo;

/// <summary>
/// The type shared by the terrain features that can be damaged in place: bridges, forest tiles and walls
/// (heavy, medium and light). Holds each kind's damage threshold and effect ids, and the forest-edge shapes.
/// </summary>
/// <remarks>Original source: <c>object\bridge.cpp</c>, <c>object\bridge.h</c>; 0x60 bytes. Read from the "BridgeData"
/// block of its FIT.</remarks>
class MCMiscTerrainObjectType : public MCObjectType
{
public:
    /// <remarks>Inline in <c>ObjectTypeManager::load</c>.</remarks>
    MCMiscTerrainObjectType() { Init(); }
    ~MCMiscTerrainObjectType() override { Destroy(); }

    /// <summary>Sets the common type defaults, every threshold to 0 and every effect id to -1.</summary>
    void Init();

    /// <summary>Makes a <see cref="MCMiscTerrainObject"/> of this type and gives it the next object id.</summary>
    MCBaseObject* CreateInstance() override;
    void Destroy() override;
    /// <summary>
    /// Reads the damage levels and effect ids from the "BridgeData" block, loads the ForestEdges shape file into the
    /// object type cache, then the common type data.
    /// </summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>
    /// A mover (object class 2 or 3) running into a light wall crushes it: a 250-point hit on the wall.
    /// </summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>Damage that destroys a heavy wall (kind 7; FIT "WallDmgLevel").</summary>
    uint32_t WallDmgLevel = 0;
    /// <summary>Damage that destroys a medium wall (kind 8; FIT "MediumWallDmgLevel", default half the wall's).</summary>
    uint32_t MediumWallDmgLevel = 0;
    /// <summary>Damage that destroys a light wall (kind 9; FIT "LightWallDmgLevel").</summary>
    uint32_t LightWallDmgLevel = 0;
    /// <summary>Damage that destroys a bridge (kind 5; FIT "BridgeDmgLevel").</summary>
    uint32_t BridgeDmgLevel = 0;
    /// <summary>Damage that burns down a forest tile (kind 6; FIT "ForestDmgLevel").</summary>
    uint32_t ForestDmgLevel = 0;
    /// <summary>FIT "BlownEffectId".</summary>
    uint32_t BlownEffectId = 0;
    /// <summary>FIT "NormalEffectId".</summary>
    uint32_t NormalEffectId = 0;
    /// <summary>FIT "DamageEffectId".</summary>
    uint32_t DamageEffectId = 0;
    /// <summary>Object type of a wall's fire (FIT "WallFireFX").</summary>
    uint32_t WallFireFX = 0;
    /// <summary>Object type of a bridge's fire (FIT "BridgeFireFX").</summary>
    uint32_t BridgeFireFX = 0;
    /// <summary>Object type of the fire started on a forest tile (FIT "ForestFireFX").</summary>
    uint32_t ForestFireFX = 0;
    /// <summary>The ForestEdges shape file, loaded whole into the object type cache; drawn over burnt forest edges.</summary>
    uint8_t* ForestEdgeShapes = nullptr;
};

/// <summary>
/// A damageable terrain feature at one terrain vertex: a bridge, a forest tile or a wall. It has no appearance of its
/// own; when destroyed it changes the terrain overlay tile and the move map's passability and line-of-fire bits.
/// </summary>
/// <remarks>
/// Original source: <c>object\bridge.cpp</c>, <c>object\bridge.h</c>; 0x58 bytes. Derives <see cref="MCGameObject"/>
/// directly (not BigGameObject): its vtable has GameObject's 113 slots and its constructor, inlined in
/// <see cref="MCMiscTerrainObjectType::CreateInstance"/>, sets only GameObject's fields before its own.
/// </remarks>
class MCMiscTerrainObject : public MCGameObject
{
public:
    /// <summary>
    /// Field defaults: just created, no fire, not destroyed, vertex and block 0, kind -1.
    /// </summary>
    /// <remarks>Inline in <see cref="MCMiscTerrainObjectType::CreateInstance"/>.</remarks>
    MCMiscTerrainObject();
    ~MCMiscTerrainObject() override { Destroy(); }

    void Init() override;
    /// <summary>Marks the object just created, object class 0x18, no damage.</summary>
    int32_t Init(MCObjectType* objType) override;
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>
    /// On the first update, places the object at its vertex and marks it destroyed already when its map cell is
    /// impassable (the map says it was blown before).
    /// </summary>
    int32_t Update() override;
    /// <summary>
    /// Clears the passability bits of a destroyed feature once its fire is out, draws the damage bars when selected
    /// or pointed at, and the forest-edge shape over a burnt forest tile.
    /// </summary>
    void Render() override;
    MCAppearance* GetAppearance() override { return nullptr; }
    /// <summary>Selection (0x1c/0x1d) and target (0x1e/0x1f) events.</summary>
    int32_t HandleEvent(MCObjectEvent* event) override;
    /// <summary>Its terrain block and vertex.</summary>
    void GetBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) override;
    /// <summary>
    /// Adds the shot's damage; past the kind's threshold it explodes (and a light wall plays its crumble sound). A
    /// forest tile also catches fire.
    /// </summary>
    int32_t HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <summary>Forgets its fire (the fire is going away).</summary>
    void KillFireObject() override;
    float GetDamage() override { return Damage; }
    /// <summary>
    /// Sets the damage; past the kind's threshold the feature is destroyed: new overlay tile, the move map's cell
    /// made passable or blocked, and (for a bridge) the global move-map area closed.
    /// </summary>
    void SetDamage(float newDamage) override;
    int IsBuilding() override { return 1; }
    /// <summary>Whether any of the four vertices around it is visible to the home team.</summary>
    int IsRevealed() override;
    /// <summary>Saves the line-of-fire bits of its map cell in <see cref="CellArray"/> and clears them.</summary>
    void ClearLineOfFire() override;
    /// <summary>Puts back the line-of-fire bits saved by <see cref="ClearLineOfFire"/>.</summary>
    void RestoreLineOfFire() override;

    using MCGameObject::GetScreenPos;
    /// <summary>Its vertex projected through the main camera.</summary>
    MCVector2D GetScreenPos();
    /// <summary>Draws the damage bar (green, yellow, red) above <paramref name="screenPos"/>.</summary>
    void DrawBars(MCVector2D screenPos);
    /// <summary>A 1-point hit, and more burning time for its fire if it has one.</summary>
    void LightOnFire(float timeToBurn);

    /// <summary>
    /// The move-map line-of-fire bits of the 3x3 sub-cells saved by <see cref="ClearLineOfFire"/>, one per entry.
    /// </summary>
    static int32_t CellArray[9];

    /// <summary>Set by init; the first update places the object and clears it.</summary>
    int32_t JustCreated = 0;
    /// <summary>The terrain vertex it stands on, within its block.</summary>
    int32_t VertexNumber = 0;
    /// <summary>The terrain block it stands in.</summary>
    int32_t BlockNumber = 0;
    /// <summary>Set when setDamage has swapped the overlay tile for the destroyed one; the first update clears it.</summary>
    int32_t OverlayDestroyed = 0;
    float Damage = 0;
    /// <summary>
    /// Which feature it is: 5 bridge, 6 forest, 7 heavy wall, 8 medium wall, 9 light wall (set by the object block
    /// manager when it places the object).
    /// </summary>
    int32_t TerrainObjectKind = 0;
    /// <summary>The fire burning on it (made from the type's forest fire effect), if any.</summary>
    MCFire* FireObject = nullptr;
    /// <summary>Set once destroyed: further hits are ignored and render updates the map's passability.</summary>
    int32_t Destroyed = 0;
};
