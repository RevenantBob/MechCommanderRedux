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
struct MCWeaponShotInfo;

/// <summary>
/// The type of a <see cref="MCGate"/>: the damage that destroys it, its effects and explosion, how close a friendly
/// unit must come for it to open, and its name.
/// </summary>
/// <remarks>Original source: <c>object\gate.cpp</c>, <c>object\gate.h</c>; 0x64 bytes. Read from the "GateData" block
/// of its FIT.</remarks>
class MCGateType : public MCObjectType
{
public:
    /// <remarks>Inline in <c>ObjectTypeManager::load</c>.</remarks>
    MCGateType() { Init(); }
    ~MCGateType() override { Destroy(); }

    /// <summary>Sets the common type defaults, no damage level, and every effect id to -1.</summary>
    void Init();

    /// <summary>Makes a <see cref="MCGate"/> of this type and gives it the next object id.</summary>
    std::unique_ptr<MCBaseObject> CreateInstance() override;
    void Destroy() override;
    /// <summary>Reads the "GateData" block, then the common type data.</summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>
    /// A mover (object classes 2 to 4) near the gate: a friendly or neutral one within the open radius asks it to
    /// open; one standing right in the gateway is remembered as the object to blow away if the gate closes on it.
    /// </summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>Damage that destroys the gate (FIT "DmgLevel").</summary>
    uint32_t DmgLevel = 0;
    /// <summary>Object type of the fire started when the gate burns or is destroyed; -1 for none (FIT "BlownEffectId").</summary>
    uint32_t BlownEffectId = 0;
    /// <summary>FIT "NormalEffectId".</summary>
    uint32_t NormalEffectId = 0;
    /// <summary>FIT "DamageEffectId".</summary>
    uint32_t DamageEffectId = 0;
    /// <summary>Horizontal pixel offset of the gate's base, replacing the placement's (FIT "BasePixelOffsetX").</summary>
    int32_t BasePixelOffsetX = 0;
    /// <summary>Vertical pixel offset of the gate's base (FIT "BasePixelOffsetY").</summary>
    int32_t BasePixelOffsetY = 0;
    /// <summary>FIT "ExplosionDamage".</summary>
    float ExplosionDamage = 0;
    /// <summary>FIT "ExplosionRadius".</summary>
    float ExplosionRadius = 0;
    /// <summary>How close a friendly unit must be for the gate to open (FIT "OpenRadius"); also its extent radius.</summary>
    float OpenRadius = 0;
    /// <summary>Radius, added to an object's extent, within which a closing gate crushes it (FIT "LittleExtent", default 20).</summary>
    float LittleExtent = 0;
    /// <summary>String resource id of the gate's name (FIT "BuildingName", default 0xa5).</summary>
    int32_t BuildingName = 0;
    /// <summary>Nonzero when the closed gate blocks line of fire (FIT "BlocksLineOfFire").</summary>
    int32_t BlocksLineOfFire = 0;
};

/// <summary>
/// A gate in a wall: it opens for friendly units that come close, closes again (crushing whatever stands in it), and
/// can be destroyed, which leaves it open. It keeps the move map's passability of its cell in step.
/// </summary>
/// <remarks>Original source: <c>object\gate.cpp</c>, <c>object\gate.h</c>; 0x100 bytes.</remarks>
class MCGate : public MCBigGameObject
{
public:
    /// <summary>
    /// Field defaults: just created, requesting open, closed, no appearance, fire or name, unknownA0 = 500000.
    /// </summary>
    /// <remarks>Inline in <see cref="MCGateType::CreateInstance"/>.</remarks>
    MCGate();
    ~MCGate() override { Destroy(); }

    void Init() override;
    /// <summary>
    /// Makes the gate's PU appearance, sets object class 0x1f, the explosion from the type and the name from its
    /// string resource.
    /// </summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Destroys the appearance and frees the name.</summary>
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>
    /// On the first update, places the gate at its vertex and map tile; then, while it stands, opens or closes it.
    /// </summary>
    int32_t Update() override;
    void Render() override;
    MCAppearance* GetAppearance() override { return Appearance; }
    /// <summary>Selection (0x1c/0x1d) and target (0x1e/0x1f) events.</summary>
    int32_t HandleEvent(MCObjectEvent* event) override;
    /// <summary>Its terrain block and vertex.</summary>
    void GetBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) override;
    /// <summary>Adds the shot's damage; at the type's damage level the gate is destroyed.</summary>
    int32_t HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <summary>Forgets its fire (the fire is going away).</summary>
    void KillFireObject() override;
    /// <summary>
    /// Takes its pixel offset from <paramref name="pixelOffset"/> (or the type's base offset) and its block and vertex
    /// from <paramref name="blockVertex"/>.
    /// </summary>
    void SetTerrainPosition(MCVector2D& pixelOffset, MCVector2D& blockVertex) override;
    void SetAlignment(int32_t newAlignment) override;
    int IsBuilding() override { return 1; }
    /// <summary>Whether any of the four vertices around it is visible to the home team.</summary>
    int IsRevealed() override;

    /// <summary>Projects its vertex through <paramref name="cam"/>; true (and remembers the turn) when it is on screen.</summary>
    int IsVisible(MCCamera* cam);
    /// <summary>
    /// Drives the gate animation towards open (an open request or no alignment) or closed (locked, or
    /// forceGatesClosed), with its sounds, and sets the map cell's passability for the current state.
    /// </summary>
    void OpenGate();
    /// <summary>Crushes the object standing in the gateway (ten 250-point hits) and damages the gate past its level.</summary>
    void BlowAnyOffendingObject();
    /// <summary>
    /// A 1-point hit when the type has no fire effect; otherwise starts (or feeds) the gate's fire.
    /// </summary>
    void LightOnFire(float timeToBurn);
    /// <summary>
    /// Leaves the gate destroyed and open: destroyed appearance, passable cell, a fire and the type's explosion
    /// (unless <paramref name="fromNetwork"/> or a fire was already started).
    /// </summary>
    void DestroyGate(int fromNetwork);

    /// <summary>Set by init; the first update places the gate and clears it.</summary>
    int32_t JustCreated = 0;
    /// <summary>The gate's PU appearance (a <c>PUAppearance</c>).</summary>
    MCAppearance* Appearance = nullptr;
    /// <summary>Horizontal pixel offset of its base on the tile.</summary>
    int32_t PixelOffsetX = 0;
    /// <summary>Vertical pixel offset of its base on the tile.</summary>
    int32_t PixelOffsetY = 0;
    /// <summary>The terrain vertex it stands on, within its block.</summary>
    int32_t VertexNumber = 0;
    /// <summary>The terrain block it stands in.</summary>
    int32_t BlockNumber = 0;
    /// <summary>The move-map column of its tile.</summary>
    int32_t TileCol = 0;
    /// <summary>The move-map row of its tile.</summary>
    int32_t TileRow = 0;
    /// <summary>World X of its tile's corner.</summary>
    float TileWorldX = 0;
    /// <summary>World Y of its tile's corner.</summary>
    float TileWorldY = 0;
    /// <summary>Elevation of its map tile, in meters.</summary>
    float TileElevation = 0;
    /// <summary>Set once a fire has been started on the gate.</summary>
    int32_t FireStarted = 0;
    /// <summary>The fire burning on it, if any.</summary>
    MCFire* FireObject = nullptr;
    /// <summary>Set by destroyGate; the gate then no longer opens or closes.</summary>
    int32_t Destroyed = 0;
    /// <summary>Set by destroyGate: the gate is blown open.</summary>
    int32_t BlownOpen = 0;
    /// <summary>When set the gate stays shut (like forceGatesClosed); never set in gate.cpp.</summary>
    int32_t LockedClosed = 0;
    /// <summary>Set when a friendly unit comes within the open radius; cleared after openGate.</summary>
    int32_t OpenRequested = 0;
    /// <summary>The gate animation is fully open.</summary>
    int32_t IsOpen = 0;
    /// <summary>The gate animation is opening.</summary>
    int32_t IsOpening = 0;
    /// <summary>The gate animation is fully closed.</summary>
    int32_t IsClosed = 0;
    /// <summary>The gate animation is closing.</summary>
    int32_t IsClosing = 0;
    /// <summary>Set to 1 at the start of destroyGate and back to 0 at its end.</summary>
    int32_t Destroying = 0;
    /// <summary>The gate's name, loaded from the type's string resource.</summary>
    std::string Name;
    /// <summary>A unit standing in the gateway, to be crushed if the gate closes on it.</summary>
    MCGameObject* OffendingObject = nullptr;
};
