#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class Appearance;
class Camera;
class File;
class Fire;
class GameObject;
class ObjectEvent;
struct _WeaponShotInfo;

/// <summary>
/// The type of a <see cref="Gate"/>: the damage that destroys it, its effects and explosion, how close a friendly
/// unit must come for it to open, and its name.
/// </summary>
/// <remarks>Original source: <c>object\gate.cpp</c>, <c>object\gate.h</c>; 0x64 bytes. Read from the "GateData" block
/// of its FIT.</remarks>
class GateType : public ObjectType
{
public:
    /// <remarks>Inline in <c>ObjectTypeManager::load</c>.</remarks>
    GateType() { init(); }
    /// <remarks>MCX.EXE @ 0x00690b50 (vector deleting destructor)</remarks>
    ~GateType() override { destroy(); }

    /// <summary>Sets the common type defaults, no damage level, and every effect id to -1.</summary>
    /// <remarks>MCX.EXE @ 0x00690b10 (inline in <c>object\gate.h</c>)</remarks>
    void init();

    /// <summary>Makes a <see cref="Gate"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x006657f0</remarks>
    BaseObject* createInstance() override;
    /// <remarks>MCX.EXE @ 0x00665a00</remarks>
    void destroy() override;
    /// <summary>Reads the "GateData" block, then the common type data.</summary>
    /// <remarks>MCX.EXE @ 0x00665a10</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <summary>
    /// A mover (object classes 2 to 4) near the gate: a friendly or neutral one within the open radius asks it to
    /// open; one standing right in the gateway is remembered as the object to blow away if the gate closes on it.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00665be0</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <remarks>MCX.EXE @ 0x00665cf0</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>Damage that destroys the gate (FIT "DmgLevel").</summary>
    uint32_t dmgLevel = 0; // +0x30
    /// <summary>Object type of the fire started when the gate burns or is destroyed; -1 for none (FIT "BlownEffectId").</summary>
    uint32_t blownEffectId = 0; // +0x34
    /// <summary>FIT "NormalEffectId".</summary>
    uint32_t normalEffectId = 0; // +0x38
    /// <summary>FIT "DamageEffectId".</summary>
    uint32_t damageEffectId = 0; // +0x3c
    /// <summary>Horizontal pixel offset of the gate's base, replacing the placement's (FIT "BasePixelOffsetX").</summary>
    int32_t basePixelOffsetX = 0; // +0x44
    /// <summary>Vertical pixel offset of the gate's base (FIT "BasePixelOffsetY").</summary>
    int32_t basePixelOffsetY = 0; // +0x48
    /// <summary>FIT "ExplosionDamage".</summary>
    float explosionDamage = 0; // +0x4c
    /// <summary>FIT "ExplosionRadius".</summary>
    float explosionRadius = 0; // +0x50
    /// <summary>How close a friendly unit must be for the gate to open (FIT "OpenRadius"); also its extent radius.</summary>
    float openRadius = 0; // +0x54
    /// <summary>Radius, added to an object's extent, within which a closing gate crushes it (FIT "LittleExtent", default 20).</summary>
    float littleExtent = 0; // +0x58
    /// <summary>String resource id of the gate's name (FIT "BuildingName", default 0xa5).</summary>
    int32_t buildingName = 0; // +0x5c
    /// <summary>Nonzero when the closed gate blocks line of fire (FIT "BlocksLineOfFire").</summary>
    int32_t blocksLineOfFire = 0; // +0x60
};

/// <summary>
/// A gate in a wall: it opens for friendly units that come close, closes again (crushing whatever stands in it), and
/// can be destroyed, which leaves it open. It keeps the move map's passability of its cell in step.
/// </summary>
/// <remarks>Original source: <c>object\gate.cpp</c>, <c>object\gate.h</c>; 0x100 bytes.</remarks>
class Gate : public BigGameObject
{
public:
    /// <summary>
    /// Field defaults: just created, requesting open, closed, no appearance, fire or name, unknownA0 = 500000.
    /// </summary>
    /// <remarks>Inline in <see cref="GateType::createInstance"/> (MCX.EXE @ 0x006657f0).</remarks>
    Gate();
    /// <remarks>MCX.EXE @ 0x006659b0 (vector deleting destructor)</remarks>
    ~Gate() override { destroy(); }

    /// <remarks>MCX.EXE @ 0x006658d0 (inline in <c>object\gate.h</c>; empty)</remarks>
    void init() override;
    /// <summary>
    /// Makes the gate's PU appearance, sets object class 0x1f, the explosion from the type and the name from its
    /// string resource.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00667020</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Destroys the appearance and frees the name.</summary>
    /// <remarks>MCX.EXE @ 0x00666fe0</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x00665970</remarks>
    int32_t kill() override { return 0; }
    /// <summary>
    /// On the first update, places the gate at its vertex and map tile; then, while it stands, opens or closes it.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00665d70</remarks>
    int32_t update() override;
    /// <remarks>MCX.EXE @ 0x00666cf0</remarks>
    void render() override;
    /// <remarks>MCX.EXE @ 0x00665960</remarks>
    Appearance* getAppearance() override { return appearance; }
    /// <summary>Selection (0x1c/0x1d) and target (0x1e/0x1f) events.</summary>
    /// <remarks>MCX.EXE @ 0x00666ac0</remarks>
    int32_t handleEvent(ObjectEvent* event) override;
    /// <summary>Its terrain block and vertex.</summary>
    /// <remarks>MCX.EXE @ 0x00665990 (inline in <c>object\gate.h</c>)</remarks>
    void getBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) override;
    /// <summary>Adds the shot's damage; at the type's damage level the gate is destroyed.</summary>
    /// <remarks>MCX.EXE @ 0x00667240</remarks>
    int32_t handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <summary>Forgets its fire (the fire is going away).</summary>
    /// <remarks>MCX.EXE @ 0x006658e0 (inline in <c>object\gate.h</c>)</remarks>
    void killFireObject() override;
    /// <summary>
    /// Takes its pixel offset from <paramref name="pixelOffset"/> (or the type's base offset) and its block and vertex
    /// from <paramref name="blockVertex"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006658f0 (inline in <c>object\gate.h</c>)</remarks>
    void setTerrainPosition(vector_2d& pixelOffset, vector_2d& blockVertex) override;
    /// <remarks>MCX.EXE @ 0x00666ab0</remarks>
    void setAlignment(int32_t newAlignment) override;
    /// <remarks>MCX.EXE @ 0x00665980</remarks>
    int isBuilding() override { return 1; }
    /// <summary>Whether any of the four vertices around it is visible to the home team.</summary>
    /// <remarks>MCX.EXE @ 0x00666c10</remarks>
    int isRevealed() override;

    /// <summary>Projects its vertex through <paramref name="cam"/>; true (and remembers the turn) when it is on screen.</summary>
    /// <remarks>MCX.EXE @ 0x00665d00</remarks>
    int isVisible(Camera* cam);
    /// <summary>
    /// Drives the gate animation towards open (an open request or no alignment) or closed (locked, or
    /// forceGatesClosed), with its sounds, and sets the map cell's passability for the current state.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00666180</remarks>
    void openGate();
    /// <summary>Crushes the object standing in the gateway (ten 250-point hits) and damages the gate past its level.</summary>
    /// <remarks>MCX.EXE @ 0x00666060</remarks>
    void blowAnyOffendingObject();
    /// <summary>
    /// A 1-point hit when the type has no fire effect; otherwise starts (or feeds) the gate's fire.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00666b20</remarks>
    void lightOnFire(float timeToBurn);
    /// <summary>
    /// Leaves the gate destroyed and open: destroyed appearance, passable cell, a fire and the type's explosion
    /// (unless <paramref name="fromNetwork"/> or a fire was already started).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006672b0</remarks>
    void destroyGate(int fromNetwork);

    /// <summary>Set by init; the first update places the gate and clears it.</summary>
    int32_t justCreated = 0; // +0x84
    /// <summary>The gate's PU appearance (a <c>PUAppearance</c>).</summary>
    Appearance* appearance = nullptr; // +0x88
    /// <summary>Horizontal pixel offset of its base on the tile.</summary>
    int32_t pixelOffsetX = 0; // +0x8c
    /// <summary>Vertical pixel offset of its base on the tile.</summary>
    int32_t pixelOffsetY = 0; // +0x90
    /// <summary>The terrain vertex it stands on, within its block.</summary>
    int32_t vertexNumber = 0; // +0x94
    /// <summary>The terrain block it stands in.</summary>
    int32_t blockNumber = 0; // +0x98
    /// <summary>The move-map column of its tile.</summary>
    int32_t tileCol = 0; // +0xb4
    /// <summary>The move-map row of its tile.</summary>
    int32_t tileRow = 0; // +0xb8
    /// <summary>World X of its tile's corner.</summary>
    float tileWorldX = 0; // +0xbc
    /// <summary>World Y of its tile's corner.</summary>
    float tileWorldY = 0; // +0xc0
    /// <summary>Elevation of its map tile, in meters.</summary>
    float tileElevation = 0; // +0xc4
    /// <summary>Set once a fire has been started on the gate.</summary>
    int32_t fireStarted = 0; // +0xc8
    /// <summary>The fire burning on it, if any.</summary>
    Fire* fireObject = nullptr; // +0xcc
    /// <summary>Set by destroyGate; the gate then no longer opens or closes.</summary>
    int32_t destroyed = 0; // +0xd4
    /// <summary>Set by destroyGate: the gate is blown open.</summary>
    int32_t blownOpen = 0; // +0xd8
    /// <summary>When set the gate stays shut (like forceGatesClosed); never set in gate.cpp.</summary>
    int32_t lockedClosed = 0; // +0xdc
    /// <summary>Set when a friendly unit comes within the open radius; cleared after openGate.</summary>
    int32_t openRequested = 0; // +0xe0
    /// <summary>The gate animation is fully open.</summary>
    int32_t isOpen = 0; // +0xe4
    /// <summary>The gate animation is opening.</summary>
    int32_t isOpening = 0; // +0xe8
    /// <summary>The gate animation is fully closed.</summary>
    int32_t isClosed = 0; // +0xec
    /// <summary>The gate animation is closing.</summary>
    int32_t isClosing = 0; // +0xf0
    /// <summary>Set to 1 at the start of destroyGate and back to 0 at its end.</summary>
    int32_t destroying = 0; // +0xf4
    /// <summary>The gate's name, loaded from the type's string resource.</summary>
    std::string name; // +0xf8
    /// <summary>A unit standing in the gateway, to be crushed if the gate closes on it.</summary>
    GameObject* offendingObject = nullptr; // +0xfc
};
