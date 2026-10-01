#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class Appearance;
class Camera;
class File;
class Fire;
class GameObject;
class ObjectEvent;
class SensorSystem;
class Team;

/// <summary>The object type the marines a destroyed building lets out are made from (0x28d).</summary>
/// <remarks>MCX.EXE @ 0x0078fcfc</remarks>
extern int32_t DefaultPilotId;
/// <summary>The pilot profile file (in profilePath) of those marines ("PEM00001").</summary>
/// <remarks>MCX.EXE @ 0x0078fd00</remarks>
extern char marineProfileName[80];
/// <summary>Nonzero to draw every terrain object's extent radius as an ellipse (debug).</summary>
/// <remarks>MCX.EXE @ 0x007dd020</remarks>
extern int drawExtents;
/// <summary>How many building marines have been made; each takes part id 0xfff minus this count.</summary>
/// <remarks>MCX.EXE @ 0x007de524</remarks>
extern int32_t NumMarines;

/// <summary>
/// The type of a <see cref="Building"/>: damage level, effects, placement offsets, tonnage, explosion, burning,
/// sensor and team, name and marines.
/// </summary>
/// <remarks>
/// Original source: <c>object\bldng.cpp</c>, <c>object\bldng.h</c>; 0x7c bytes. Read from the "BuildingData" block
/// of its FIT.
/// </remarks>
class BuildingType : public ObjectType
{
public:
    /// <remarks>Inline in ObjectTypeManager::load.</remarks>
    BuildingType() { init(); }
    /// <remarks>MCX.EXE @ 0x00690510 (vector deleting destructor)</remarks>
    ~BuildingType() override { destroy(); }

    /// <summary>Sets the common type fields and the building fields to their defaults.</summary>
    /// <remarks>MCX.EXE @ 0x006904c0 (inline in <c>object\bldng.h</c>)</remarks>
    void init();

    /// <summary>Makes a <see cref="Building"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x00651a20</remarks>
    BaseObject* createInstance() override;
    /// <remarks>MCX.EXE @ 0x00651cc0</remarks>
    void destroy() override;
    /// <summary>Reads the "BuildingData" block, then the common type data (the FIT extent radius wins).</summary>
    /// <remarks>MCX.EXE @ 0x00651cd0</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <summary>
    /// A mover (object class below 8, not 7) running into the building deals it 10 points of damage, but only while
    /// the scenario time hasn't passed the mover's collision-free time (server only).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00651f90</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <remarks>MCX.EXE @ 0x00652010</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>The damage that destroys the building (FIT "DmgLevel").</summary>
    uint32_t dmgLevel; // +0x30
    /// <summary>The object type made when the building is destroyed or set burning (a fire; FIT "BlownEffectId").</summary>
    uint32_t blownEffectId; // +0x34
    /// <summary>The looping sound played while the building is visible, or 0xffffffff (FIT "NormalEffectId").</summary>
    uint32_t normalEffectId; // +0x38
    /// <summary>FIT "DamageEffectId".</summary>
    uint32_t damageEffectId; // +0x3c
    /// <summary>Replaces the placement's pixel offset X when nonzero (FIT "BasePixelOffsetX").</summary>
    int32_t basePixelOffsetX; // +0x40
    /// <summary>Replaces the placement's pixel offset Y when nonzero (FIT "BasePixelOffsetY").</summary>
    int32_t basePixelOffsetY; // +0x44
    /// <summary>Replaces the pixel offset X when placing the building in the world (FIT "CollisionOffsetX").</summary>
    int32_t collisionOffsetX; // +0x48
    /// <summary>Replaces the pixel offset Y when placing the building in the world (FIT "CollisionOffsetY").</summary>
    int32_t collisionOffsetY; // +0x4c
    /// <summary>The range of the building's sensor, or -1 for none (FIT "SensorRange").</summary>
    float sensorRange; // +0x50
    /// <summary>The team (index into TeamTable) the building belongs to, or -1 (FIT "TeamID").</summary>
    int32_t teamId; // +0x54
    /// <summary>FIT "Tonnage" (default 20).</summary>
    float baseTonnage; // +0x58
    /// <summary>FIT "ExplosionDamage".</summary>
    float explDmg; // +0x5c
    /// <summary>FIT "ExplosionRadius".</summary>
    float explRad; // +0x60
    /// <summary>Seconds between burn damage while on fire (FIT "TimeToBurnDamage", default 5).</summary>
    float timeToBurnDamage; // +0x64
    /// <summary>The damage burning deals each time (FIT "BurnDamagePerTime", default 1).</summary>
    float burnDamagePerTime; // +0x68
    /// <summary>FIT "DamageLvlForBurn" (default the damage level).</summary>
    float damageLvlForBurn; // +0x6c
    /// <summary>The string resource id of the building's name (FIT "BuildingName", default 0xa3).</summary>
    int32_t buildingName; // +0x70
    /// <summary>The building's combat value (FIT "BattleRating", default 20).</summary>
    int32_t battleRating; // +0x74
    /// <summary>How many marines come out when it is destroyed (FIT "NumMarines").</summary>
    int32_t numMarines; // +0x78
};

/// <summary>
/// A building standing on a terrain vertex: it can be damaged, burn, be destroyed (letting out marines), be
/// captured, hold a sensor, and shows a blip on the tactical view when it belongs to the enemy.
/// </summary>
/// <remarks>Original source: <c>object\bldng.cpp</c>, <c>object\bldng.h</c>; 0x100 bytes.</remarks>
class Building : public BigGameObject
{
public:
    /// <summary>Sets the building's fields to their defaults.</summary>
    /// <remarks>Inline in BuildingType::createInstance (0x00651a20).</remarks>
    Building();
    /// <remarks>MCX.EXE @ 0x00651c70 (vector deleting destructor)</remarks>
    ~Building() override { destroy(); }

    /// <summary>Clears the sensor and the four prison slots.</summary>
    /// <remarks>MCX.EXE @ 0x00651b60 (inline in <c>object\bldng.h</c>)</remarks>
    void init() override;
    /// <summary>
    /// Makes the building's VFX building appearance, copies tonnage, explosion and combat value from the type, loads
    /// its name, and sets up its team and sensor.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00652e90</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Frees the appearance, the sensor and the name.</summary>
    /// <remarks>MCX.EXE @ 0x00652d70</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x00651c10</remarks>
    int32_t kill() override { return 0; }
    /// <summary>On the first update, places the building in the world from its block, vertex and pixel offsets.</summary>
    /// <remarks>MCX.EXE @ 0x006520a0</remarks>
    int32_t update() override;
    /// <summary>
    /// Draws the building (hazed by how much of it is revealed), burns it, plays its looping sound and draws the
    /// enemy blip.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006526f0</remarks>
    void render() override;
    /// <remarks>MCX.EXE @ 0x00651c00</remarks>
    Appearance* getAppearance() override { return appearance; }
    /// <summary>Handles the ABL events that set (0x1c/0x1e) and clear (0x1d/0x1f) the flags at +0x28 and +0x2c.</summary>
    /// <remarks>MCX.EXE @ 0x006524a0</remarks>
    int32_t handleEvent(ObjectEvent* event) override;
    /// <summary>Returns the block and vertex the building stands on.</summary>
    /// <remarks>MCX.EXE @ 0x00651c50</remarks>
    void getBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) override;
    /// <summary>
    /// Applies a weapon hit: past the damage level the building is destroyed (sensor off, fire, explosion, salvage
    /// removed, marines let out).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006535b0</remarks>
    int32_t handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <remarks>MCX.EXE @ 0x00651b80</remarks>
    void killFireObject() override { fireObject = nullptr; }
    /// <summary>
    /// Sets the pixel offsets from <paramref name="offset"/> (the type's base pixel offsets win unless the flag at
    /// <see cref="tileNum"/> is 10) and the vertex and block numbers from <paramref name="numbers"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00651b90 (inline in <c>object\bldng.h</c>)</remarks>
    void setTerrainPosition(vector_2d& offset, vector_2d& numbers) override;
    /// <summary>Sets the damage and shows the matching damage frame (at most 15).</summary>
    /// <remarks>MCX.EXE @ 0x00652dd0</remarks>
    void setDamage(float newDamage) override;
    /// <summary>Sets the alignment and moves the sensor to the matching team.</summary>
    /// <remarks>MCX.EXE @ 0x00652440</remarks>
    void setAlignment(int32_t align) override;
    /// <remarks>MCX.EXE @ 0x00652600</remarks>
    void setCommanderId(int32_t id) override;
    /// <summary>The commander id, sign-extended from its byte.</summary>
    /// <remarks>MCX.EXE @ 0x00651c20 (unnamed in the symbols; vtable slot 52)</remarks>
    int32_t getCommanderId() override { return commanderId; }
    /// <summary>Whether it can be captured: flagged captureable and not destroyed (nor, single-player, already captured).</summary>
    /// <remarks>MCX.EXE @ 0x006525a0</remarks>
    int isCaptureable() override;
    /// <summary>Whether any of the four prison slots is set.</summary>
    /// <remarks>MCX.EXE @ 0x00651c30 (inline in <c>object\bldng.h</c>)</remarks>
    int isPrison() override;
    /// <summary>Whether any corner of its vertex is visible to the home team.</summary>
    /// <remarks>MCX.EXE @ 0x00652610</remarks>
    int isRevealed() override;

    /// <summary>Projects the building for <paramref name="cam"/>; true when on screen (stamps the render turn).</summary>
    /// <remarks>MCX.EXE @ 0x00652020</remarks>
    int isVisible(Camera* cam);
    /// <summary>Sets the building on fire (making the fire object if needed) and adds burn time.</summary>
    /// <remarks>MCX.EXE @ 0x00652500</remarks>
    void lightOnFire(float timeToBurn);
    /// <summary>Gives the building a sensor of <paramref name="range"/> (if above -1), optionally on a team.</summary>
    /// <remarks>MCX.EXE @ 0x00652e10</remarks>
    void setSensorData(Team* team, float range, int setTeam);
    /// <summary>Lets the type's number of marines out of the destroyed building, piloted by idle enemy warriors.</summary>
    /// <remarks>MCX.EXE @ 0x00653210</remarks>
    void createBuildingMarines();

    /// <summary>Set until the first update has placed the building in the world.</summary>
    int32_t justCreated; // +0x84
    /// <summary>The building's VFX building appearance.</summary>
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
    /// <summary>
    /// The ground tile drawn under it (copied to VFXBuildingAppearance::tileNum each frame); at 10 the type's base
    /// pixel offsets are not used.
    /// </summary>
    uint8_t tileNum; // +0xd4
    /// <summary>The handle of the looping sound playing while it is visible, or 0xffffffff.</summary>
    uint32_t soundHandle; // +0xd8
    /// <summary>The building's sensor, if it has one.</summary>
    SensorSystem* sensorSystem; // +0xdc
    /// <summary>Nonzero when it can be captured (set by the ABL SetCaptureable).</summary>
    int32_t captureable; // +0xe0
    /// <summary>Unknown: only set to 0 by the constructor.</summary>
    int32_t unknownE4; // +0xe4
    /// <summary>The commander id (0xff = none).</summary>
    char commanderId; // +0xe8
    /// <summary>The building's name, loaded from its string resource (systemHeap).</summary>
    char* name; // +0xec
    /// <summary>The prison slots: the pilots held here (<see cref="isPrison"/> is true when any is set). Nothing in
    /// bldng.cpp sets them; capturing the building moves them into the capturing vehicle's passenger seats.</summary>
    MechWarrior* prisonSlots[4]; // +0xf0
};
