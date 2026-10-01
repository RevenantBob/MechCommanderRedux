#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class Appearance;
class BaseObject;
class Camera;
class File;
class Fire;
class GameObject;
class ObjectEvent;
class SensorSystem;
class Team;

/// <summary>
/// The type of a <see cref="TreeBuilding"/>: a building drawn as a sprite with shadows, which can be a refit point
/// or mech bay.
/// </summary>
/// <remarks>
/// Original source: <c>object\tbldng.cpp</c>, <c>object\tbldng.h</c>; 0x7c bytes. Read from the "TreeData" block of
/// its FIT.
/// </remarks>
class TreeBuildingType : public ObjectType
{
public:
    /// <remarks>Inline in ObjectTypeManager::load.</remarks>
    TreeBuildingType() { init(); }
    /// <remarks>MCX.EXE @ 0x006905b0 (vector deleting destructor)</remarks>
    ~TreeBuildingType() override { destroy(); }

    /// <summary>Sets the common type fields and the building fields to their defaults.</summary>
    /// <remarks>MCX.EXE @ 0x00690550 (inline in <c>object\tbldng.h</c>)</remarks>
    void init();

    /// <summary>Makes a <see cref="TreeBuilding"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x00694f50</remarks>
    BaseObject* createInstance() override;
    /// <summary>Frees the two shadow shapes.</summary>
    /// <remarks>MCX.EXE @ 0x00695320</remarks>
    void destroy() override;
    /// <summary>
    /// Reads the "TreeData" block (loading the NormalShadow and DestroyedShadow shapes into the object type cache),
    /// then the common type data.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00695360</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <summary>Always reports a collision and does nothing else.</summary>
    /// <remarks>MCX.EXE @ 0x00695700</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <remarks>MCX.EXE @ 0x00695710</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>The damage that destroys the building; 0 means it starts destroyed (FIT "DmgLevel").</summary>
    uint32_t dmgLevel; // +0x30
    /// <summary>
    /// The object type made when the building is set burning or destroyed (a fire), or -1 (FIT "BlownEffectId").
    /// </summary>
    uint32_t blownEffectId; // +0x34
    /// <summary>The looping sound played while the building is visible, or 0xffffffff (FIT "NormalEffectId").</summary>
    uint32_t normalEffectId; // +0x38
    /// <summary>The sound played when the building is destroyed, or 0xffffffff (FIT "DamageEffectId").</summary>
    uint32_t damageEffectId; // +0x3c
    /// <summary>The shadow shape of the standing building (FIT "NormalShadow", objectTypeCache).</summary>
    uint8_t* normalShadow; // +0x40
    /// <summary>The shadow shape of the destroyed building (FIT "DestroyedShadow", objectTypeCache).</summary>
    uint8_t* destroyedShadow; // +0x44
    /// <summary>The range of the building's sensor, or -1 for none (FIT "SensorRange").</summary>
    float sensorRange; // +0x48
    /// <summary>The team (index into TeamTable) the building belongs to, or -1 (FIT "TeamID").</summary>
    int32_t teamId; // +0x4c
    /// <summary>FIT "Tonnage" (default 20).</summary>
    float baseTonnage; // +0x50
    /// <summary>FIT "ExplosionDamage".</summary>
    float explDmg; // +0x54
    /// <summary>FIT "ExplosionRadius".</summary>
    float explRad; // +0x58
    /// <summary>Seconds between burn damage while on fire (FIT "TimeToBurnDamage", default 5).</summary>
    float timeToBurnDamage; // +0x5c
    /// <summary>The damage burning deals each time (FIT "BurnDamagePerTime", default 1).</summary>
    float burnDamagePerTime; // +0x60
    /// <summary>FIT "DamageLvlForBurn" (default the damage level).</summary>
    float damageLvlForBurn; // +0x64
    /// <summary>The string resource id of the building's name (FIT "BuildingName", default 0xa3).</summary>
    int32_t buildingName; // +0x68
    /// <summary>The building's combat value (FIT "BattleRating", default 20).</summary>
    int32_t battleRating; // +0x6c
    /// <summary>How many marines come out when it is destroyed (FIT "NumMarines").</summary>
    int32_t numMarines; // +0x70
    /// <summary>Whether the building repairs units (FIT "CanRefit").</summary>
    int32_t canRefit; // +0x74
    /// <summary>Whether a refit building is a mech bay (FIT "MechBay", read only when CanRefit).</summary>
    int32_t mechBay; // +0x78
};

/// <summary>
/// A building drawn like a tree (a VFX sprite with a frame of reference): it can burn, collapse, be captured, hold a
/// sensor, and serve as a refit point or mech bay.
/// </summary>
/// <remarks>Original source: <c>object\tbldng.cpp</c>, <c>object\tbldng.h</c>; 0x138 bytes.</remarks>
class TreeBuilding : public BigGameObject
{
public:
    /// <summary>Sets the frame to the identity and the building's fields to their defaults.</summary>
    /// <remarks>Inline in TreeBuildingType::createInstance (0x00694f50).</remarks>
    TreeBuilding();
    /// <remarks>MCX.EXE @ 0x006952d0 (vector deleting destructor)</remarks>
    ~TreeBuilding() override { destroy(); }

    /// <summary>Clears the four prison slots.</summary>
    /// <remarks>MCX.EXE @ 0x00695090 (inline in <c>object\tbldng.h</c>)</remarks>
    void init() override;
    /// <summary>
    /// Makes the VFX appearance, copies tonnage, explosion, combat value and refit flags from the type, loads its
    /// name, and sets up its team and sensor. A type with damage level 0 starts destroyed.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00696730</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Frees the appearance, the sensor and the name.</summary>
    /// <remarks>MCX.EXE @ 0x00696650</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x006951f0</remarks>
    int32_t kill() override { return 0; }
    /// <summary>On the first update, places the building in the world from its block, vertex and pixel offsets.</summary>
    /// <remarks>MCX.EXE @ 0x00695960</remarks>
    int32_t update() override;
    /// <summary>
    /// Burns the building, plays its collapse, draws it hazed by how much of it is revealed, plays its looping sound
    /// and draws the enemy blip.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00695ef0</remarks>
    void render() override;
    /// <remarks>MCX.EXE @ 0x006951e0</remarks>
    Appearance* getAppearance() override { return appearance; }
    /// <summary>Handles the ABL events that set (0x1c/0x1e) and clear (0x1d/0x1f) the flags at +0x28 and +0x2c.</summary>
    /// <remarks>MCX.EXE @ 0x00695cc0</remarks>
    int32_t handleEvent(ObjectEvent* event) override;
    /// <summary>Returns the block and vertex the building stands on.</summary>
    /// <remarks>MCX.EXE @ 0x00695220 (inline in <c>object\tbldng.h</c>)</remarks>
    void getBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) override;
    /// <summary>
    /// Applies a weapon hit: past the damage level the building collapses (alarm raised for the shooter's side,
    /// marines let out, sound, sensor off, fire, explosion, salvage removed).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00696e00 (unnamed in the symbols; vtable slot 23, in <c>object\tbldng.cpp</c>)</remarks>
    int32_t handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <remarks>MCX.EXE @ 0x006950b0 (unnamed in the symbols; vtable slot 25)</remarks>
    void killFireObject() override { fireObject = nullptr; }
    /// <summary>
    /// Sets the pixel offsets from <paramref name="offset"/> and the vertex and block numbers from
    /// <paramref name="numbers"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006950d0 (inline in <c>object\tbldng.h</c>)</remarks>
    void setTerrainPosition(vector_2d& offset, vector_2d& numbers) override;
    /// <summary>The building's frame of reference.</summary>
    /// <remarks>MCX.EXE @ 0x00695120 (inline in <c>object\tbldng.h</c>)</remarks>
    frame_of_ref getFrame() override;
    /// <remarks>MCX.EXE @ 0x00695180 (inline in <c>object\tbldng.h</c>)</remarks>
    void setFrame(frame_of_ref& newFrame) override;
    /// <summary>Sets the alignment and, if not destroyed, moves the sensor to the matching team.</summary>
    /// <remarks>MCX.EXE @ 0x00695c40</remarks>
    void setAlignment(int32_t align) override;
    /// <remarks>MCX.EXE @ 0x00695cb0</remarks>
    void setCommanderId(int32_t id) override;
    /// <summary>The commander id, sign-extended from its byte.</summary>
    /// <remarks>MCX.EXE @ 0x006950c0</remarks>
    int32_t getCommanderId() override { return commanderId; }
    /// <summary>Whether it can be captured: flagged captureable and not destroyed (nor, single-player, already captured).</summary>
    /// <remarks>MCX.EXE @ 0x00695900</remarks>
    int isCaptureable() override;
    /// <summary>Whether any of the four prison slots is set.</summary>
    /// <remarks>MCX.EXE @ 0x00695200 (inline in <c>object\tbldng.h</c>)</remarks>
    int isPrison() override;
    /// <summary>Whether any corner of its vertex is visible to the home team.</summary>
    /// <remarks>MCX.EXE @ 0x00695e10</remarks>
    int isRevealed() override;
    /// <summary>For a refit building, the repair points left (damage level minus damage); otherwise 0.</summary>
    /// <remarks>MCX.EXE @ 0x00695240 (inline in <c>object\tbldng.h</c>)</remarks>
    float getRefitPoints() override;
    /// <summary>For a refit building, spends up to <paramref name="points"/> repair points (as damage).</summary>
    /// <remarks>MCX.EXE @ 0x00695260 (inline in <c>object\tbldng.h</c>)</remarks>
    int burnRefitPoints(float points) override;

    /// <summary>Gives the building a sensor of <paramref name="range"/> (if above -1), optionally on a team.</summary>
    /// <remarks>MCX.EXE @ 0x006966b0 (unnamed in the symbols; vtable slot 116, in <c>object\tbldng.cpp</c>)</remarks>
    virtual void setSensorData(Team* team, float range, int setTeam);

    /// <summary>Projects the building (and its shadow) for <paramref name="cam"/>; true when on screen.</summary>
    /// <remarks>MCX.EXE @ 0x00695720</remarks>
    int isVisible(Camera* cam);
    /// <summary>
    /// Sets the building on fire (making the fire object if needed) and adds burn time; without a fire type it takes
    /// 1 point of damage instead.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00695d20</remarks>
    void lightOnFire(float timeToBurn);
    /// <summary>Lets the type's number of marines out of the destroyed building, piloted by idle enemy warriors.</summary>
    /// <remarks>MCX.EXE @ 0x006969c0</remarks>
    void createBuildingMarines();

    /// <summary>Set until the first update has placed the building in the world.</summary>
    int32_t justCreated; // +0x84
    /// <summary>The building's VFX appearance.</summary>
    Appearance* appearance; // +0x88
    /// <summary>The pixel offset X of the building from its vertex.</summary>
    int32_t pixelOffsetX; // +0x8c
    /// <summary>The pixel offset Y of the building from its vertex.</summary>
    int32_t pixelOffsetY; // +0x90
    /// <summary>The terrain vertex within its block.</summary>
    int32_t vertexNumber; // +0x94
    /// <summary>The terrain block.</summary>
    int32_t blockNumber; // +0x98
    /// <summary>Unknown: only set to 0 by the constructor.</summary>
    int32_t unknown9C; // +0x9c
    /// <summary>Unknown: only set to 500000 by the constructor.</summary>
    int32_t unknownA0; // +0xa0
    /// <summary>Unknown: never accessed by these classes.</summary>
    int32_t unknownA4[4]; // +0xa4
    /// <summary>The map cell column of its vertex.</summary>
    int32_t cellColumn; // +0xb4
    /// <summary>The map cell row of its vertex.</summary>
    int32_t cellRow; // +0xb8
    /// <summary>The world X of its vertex.</summary>
    float vertexWorldX; // +0xbc
    /// <summary>The world Y of its vertex.</summary>
    float vertexWorldY; // +0xc0
    /// <summary>The elevation of its map cell, in meters.</summary>
    float cellElevation; // +0xc4
    /// <summary>Set while the building is burning.</summary>
    int32_t burning; // +0xc8
    /// <summary>The fire burning on the building.</summary>
    Fire* fireObject; // +0xcc
    /// <summary>Seconds since the last burn damage.</summary>
    float burnTime; // +0xd0
    /// <summary>The type's team (TeamTable[teamId]), set by init.</summary>
    Team* typeTeam; // +0xd4
    /// <summary>Set while the collapse animation plays.</summary>
    int32_t collapsing; // +0xd8
    /// <summary>Set once the collapse animation has finished.</summary>
    int32_t collapsed; // +0xdc
    /// <summary>
    /// Set once the building has taken a hit (or starts destroyed); chooses the collapse animation (1 when clear, 4
    /// when set).
    /// </summary>
    int32_t hitOnce; // +0xe0
    /// <summary>The building's frame of reference (identity at construction).</summary>
    frame_of_ref frame; // +0xe4
    /// <summary>The handle of the looping sound playing while it is visible, or 0xffffffff.</summary>
    uint32_t soundHandle; // +0x108
    /// <summary>The building's sensor, if it has one.</summary>
    SensorSystem* sensorSystem; // +0x10c
    /// <summary>Nonzero when it can be captured (set by the ABL SetCaptureable).</summary>
    int32_t captureable; // +0x110
    /// <summary>The commander id (0xff = none).</summary>
    char commanderId; // +0x114
    /// <summary>The building's name, loaded from its string resource (systemHeap).</summary>
    char* name; // +0x118
    /// <summary>The prison slots: the pilots held here (<see cref="isPrison"/> is true when any is set). Nothing in
    /// tbldng.cpp sets them; capturing the building moves them into the capturing vehicle's passenger seats.</summary>
    MechWarrior* prisonSlots[4]; // +0x11c
    /// <summary>Whether the building repairs units (from the type).</summary>
    int32_t canRefit; // +0x12c
    /// <summary>Whether the refit building is a mech bay (from the type).</summary>
    int32_t mechBay; // +0x130
    /// <summary>The unit being refitted here, if any (checked by the ABL refit order; cleared by init).</summary>
    GameObject* refitBuddy; // +0x134
};
