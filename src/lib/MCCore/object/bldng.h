#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class MCAppearance;
class MCCamera;
class MCFile;
class MCFire;
class MCGameObject;
class MCObjectEvent;
class MCSensorSystem;
class MCTeam;

/// <summary>The object type the marines a destroyed building lets out are made from (0x28d).</summary>
extern int32_t DefaultPilotId;
/// <summary>The pilot profile file (in profilePath) of those marines ("PEM00001").</summary>
extern char MarineProfileName[80];
/// <summary>Nonzero to draw every terrain object's extent radius as an ellipse (debug).</summary>
extern int DrawExtents;
/// <summary>How many building marines have been made; each takes part id 0xfff minus this count.</summary>
extern int32_t NumMarines;

/// <summary>
/// The type of a <see cref="MCBuilding"/>: damage level, effects, placement offsets, tonnage, explosion, burning,
/// sensor and team, name and marines.
/// </summary>
/// <remarks>
/// Original source: <c>object\bldng.cpp</c>, <c>object\bldng.h</c>; 0x7c bytes. Read from the "BuildingData" block
/// of its FIT.
/// </remarks>
class MCBuildingType : public MCObjectType
{
public:
    /// <remarks>Inline in ObjectTypeManager::load.</remarks>
    MCBuildingType() { Init(); }
    ~MCBuildingType() override { Destroy(); }

    /// <summary>Sets the common type fields and the building fields to their defaults.</summary>
    void Init();

    /// <summary>Makes a <see cref="MCBuilding"/> of this type and gives it the next object id.</summary>
    MCBaseObject* CreateInstance() override;
    void Destroy() override;
    /// <summary>Reads the "BuildingData" block, then the common type data (the FIT extent radius wins).</summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>
    /// A mover (object class below 8, not 7) running into the building deals it 10 points of damage, but only while
    /// the scenario time hasn't passed the mover's collision-free time (server only).
    /// </summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>The damage that destroys the building (FIT "DmgLevel").</summary>
    uint32_t DmgLevel = 0;
    /// <summary>The object type made when the building is destroyed or set burning (a fire; FIT "BlownEffectId").</summary>
    uint32_t BlownEffectId = 0;
    /// <summary>The looping sound played while the building is visible, or 0xffffffff (FIT "NormalEffectId").</summary>
    uint32_t NormalEffectId = 0;
    /// <summary>FIT "DamageEffectId".</summary>
    uint32_t DamageEffectId = 0;
    /// <summary>Replaces the placement's pixel offset X when nonzero (FIT "BasePixelOffsetX").</summary>
    int32_t BasePixelOffsetX = 0;
    /// <summary>Replaces the placement's pixel offset Y when nonzero (FIT "BasePixelOffsetY").</summary>
    int32_t BasePixelOffsetY = 0;
    /// <summary>Replaces the pixel offset X when placing the building in the world (FIT "CollisionOffsetX").</summary>
    int32_t CollisionOffsetX = 0;
    /// <summary>Replaces the pixel offset Y when placing the building in the world (FIT "CollisionOffsetY").</summary>
    int32_t CollisionOffsetY = 0;
    /// <summary>The range of the building's sensor, or -1 for none (FIT "SensorRange").</summary>
    float SensorRange = 0;
    /// <summary>The team (index into TeamTable) the building belongs to, or -1 (FIT "TeamID").</summary>
    int32_t TeamId = 0;
    /// <summary>FIT "Tonnage" (default 20).</summary>
    float BaseTonnage = 0;
    /// <summary>FIT "ExplosionDamage".</summary>
    float ExplDmg = 0;
    /// <summary>FIT "ExplosionRadius".</summary>
    float ExplRad = 0;
    /// <summary>Seconds between burn damage while on fire (FIT "TimeToBurnDamage", default 5).</summary>
    float TimeToBurnDamage = 0;
    /// <summary>The damage burning deals each time (FIT "BurnDamagePerTime", default 1).</summary>
    float BurnDamagePerTime = 0;
    /// <summary>FIT "DamageLvlForBurn" (default the damage level).</summary>
    float DamageLvlForBurn = 0;
    /// <summary>The string resource id of the building's name (FIT "BuildingName", default 0xa3).</summary>
    int32_t BuildingName = 0;
    /// <summary>The building's combat value (FIT "BattleRating", default 20).</summary>
    int32_t BattleRating = 0;
    /// <summary>How many marines come out when it is destroyed (FIT "NumMarines").</summary>
    int32_t NumMarines = 0;
};

/// <summary>
/// A building standing on a terrain vertex: it can be damaged, burn, be destroyed (letting out marines), be
/// captured, hold a sensor, and shows a blip on the tactical view when it belongs to the enemy.
/// </summary>
/// <remarks>Original source: <c>object\bldng.cpp</c>, <c>object\bldng.h</c>; 0x100 bytes.</remarks>
class MCBuilding : public MCBigGameObject
{
public:
    /// <summary>Sets the building's fields to their defaults.</summary>
    /// <remarks>Inline in BuildingType::createInstance (0x00651a20).</remarks>
    MCBuilding();
    ~MCBuilding() override { Destroy(); }

    /// <summary>Clears the sensor and the four prison slots.</summary>
    void Init() override;
    /// <summary>
    /// Makes the building's VFX building appearance, copies tonnage, explosion and combat value from the type, loads
    /// its name, and sets up its team and sensor.
    /// </summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Frees the appearance, the sensor and the name.</summary>
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>On the first update, places the building in the world from its block, vertex and pixel offsets.</summary>
    int32_t Update() override;
    /// <summary>
    /// Draws the building (hazed by how much of it is revealed), burns it, plays its looping sound and draws the
    /// enemy blip.
    /// </summary>
    void Render() override;
    MCAppearance* GetAppearance() override { return Appearance; }
    /// <summary>Handles the ABL events that set (0x1c/0x1e) and clear (0x1d/0x1f) the flags at +0x28 and +0x2c.</summary>
    int32_t HandleEvent(MCObjectEvent* event) override;
    /// <summary>Returns the block and vertex the building stands on.</summary>
    void GetBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) override;
    /// <summary>
    /// Applies a weapon hit: past the damage level the building is destroyed (sensor off, fire, explosion, salvage
    /// removed, marines let out).
    /// </summary>
    int32_t HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    void KillFireObject() override { FireObject = nullptr; }
    /// <summary>
    /// Sets the pixel offsets from <paramref name="offset"/> (the type's base pixel offsets win unless the flag at
    /// <see cref="TileNum"/> is 10) and the vertex and block numbers from <paramref name="numbers"/>.
    /// </summary>
    void SetTerrainPosition(MCVector2D& offset, MCVector2D& numbers) override;
    /// <summary>Sets the damage and shows the matching damage frame (at most 15).</summary>
    void SetDamage(float newDamage) override;
    /// <summary>Sets the alignment and moves the sensor to the matching team.</summary>
    void SetAlignment(int32_t align) override;
    void SetCommanderId(int32_t id) override;
    /// <summary>The commander id, sign-extended from its byte.</summary>
    int32_t GetCommanderId() override { return CommanderId; }
    /// <summary>Whether it can be captured: flagged captureable and not destroyed (nor, single-player, already captured).</summary>
    int IsCaptureable() override;
    /// <summary>Whether any of the four prison slots is set.</summary>
    int IsPrison() override;
    /// <summary>Whether any corner of its vertex is visible to the home team.</summary>
    int IsRevealed() override;

    /// <summary>Projects the building for <paramref name="cam"/>; true when on screen (stamps the render turn).</summary>
    int IsVisible(MCCamera* cam);
    /// <summary>Sets the building on fire (making the fire object if needed) and adds burn time.</summary>
    void LightOnFire(float timeToBurn);
    /// <summary>Gives the building a sensor of <paramref name="range"/> (if above -1), optionally on a team.</summary>
    void SetSensorData(MCTeam* team, float range, int setTeam);
    /// <summary>Lets the type's number of marines out of the destroyed building, piloted by idle enemy warriors.</summary>
    void CreateBuildingMarines();

    /// <summary>Set until the first update has placed the building in the world.</summary>
    int32_t JustCreated = 0;
    /// <summary>The building's VFX building appearance.</summary>
    MCAppearance* Appearance = nullptr;
    /// <summary>The pixel offset X of the building from its vertex.</summary>
    int32_t PixelOffsetX = 0;
    /// <summary>The pixel offset Y of the building from its vertex.</summary>
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
    /// <summary>Set while the building is burning.</summary>
    int32_t Burning = 0;
    /// <summary>The fire burning on the building.</summary>
    MCFire* FireObject = nullptr;
    /// <summary>Seconds since the last burn damage.</summary>
    float BurnTime = 0;
    /// <summary>
    /// The ground tile drawn under it (copied to VFXBuildingAppearance::tileNum each frame); at 10 the type's base
    /// pixel offsets are not used.
    /// </summary>
    uint8_t TileNum = 0;
    /// <summary>The handle of the looping sound playing while it is visible, or 0xffffffff.</summary>
    uint32_t SoundHandle = 0;
    /// <summary>The building's sensor, if it has one.</summary>
    MCSensorSystem* SensorSystem = nullptr;
    /// <summary>Nonzero when it can be captured (set by the ABL SetCaptureable).</summary>
    int32_t Captureable = 0;
    /// <summary>The commander id (0xff = none).</summary>
    char CommanderId = 0;
    /// <summary>The building's name, loaded from its string resource.</summary>
    std::string Name;
    /// <summary>The prison slots: the pilots held here (<see cref="IsPrison"/> is true when any is set). Nothing in
    /// bldng.cpp sets them; capturing the building moves them into the capturing vehicle's passenger seats.</summary>
    MCMechWarrior* PrisonSlots[4]{};
};
