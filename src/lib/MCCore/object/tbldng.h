#pragma once

#include "object/MCBigGameObject.h"
#include "object/MCObjectType.h"
#include "object/MCWeaponShotInfo.h"

class MCAppearance;
class MCBaseObject;
class MCCamera;
class MCFile;
class MCFire;
class MCGameObject;
class MCObjectEvent;
class MCSensorSystem;
class MCTeam;

/// <summary>
/// The type of a <see cref="MCTreeBuilding"/>: a building drawn as a sprite with shadows, which can be a refit point
/// or mech bay.
/// </summary>
/// <remarks>
/// Original source: <c>object\tbldng.cpp</c>, <c>object\tbldng.h</c>; 0x7c bytes. Read from the "TreeData" block of
/// its FIT.
/// </remarks>
class MCTreeBuildingType : public MCObjectType
{
public:
    /// <remarks>Inline in ObjectTypeManager::load.</remarks>
    MCTreeBuildingType() { Init(); }
    ~MCTreeBuildingType() override { Destroy(); }

    /// <summary>Sets the common type fields and the building fields to their defaults.</summary>
    void Init();

    /// <summary>Makes a <see cref="MCTreeBuilding"/> of this type and gives it the next object id.</summary>
    std::unique_ptr<MCBaseObject> CreateInstance() override;
    /// <summary>Frees the two shadow shapes.</summary>
    void Destroy() override;
    /// <summary>
    /// Reads the "TreeData" block (loading the NormalShadow and DestroyedShadow shapes into the object type cache),
    /// then the common type data.
    /// </summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>Always reports a collision and does nothing else.</summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>The damage that destroys the building; 0 means it starts destroyed (FIT "DmgLevel").</summary>
    uint32_t DmgLevel = 0;
    /// <summary>
    /// The object type made when the building is set burning or destroyed (a fire), or -1 (FIT "BlownEffectId").
    /// </summary>
    uint32_t BlownEffectId = 0;
    /// <summary>The looping sound played while the building is visible, or 0xffffffff (FIT "NormalEffectId").</summary>
    uint32_t NormalEffectId = 0;
    /// <summary>The sound played when the building is destroyed, or 0xffffffff (FIT "DamageEffectId").</summary>
    uint32_t DamageEffectId = 0;
    /// <summary>The shadow shape of the standing building (FIT "NormalShadow").</summary>
    uint8_t* NormalShadow = nullptr;
    /// <summary>The shadow shape of the destroyed building (FIT "DestroyedShadow").</summary>
    uint8_t* DestroyedShadow = nullptr;
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
    /// <summary>Whether the building repairs units (FIT "CanRefit").</summary>
    int32_t CanRefit = 0;
    /// <summary>Whether a refit building is a mech bay (FIT "MechBay", read only when CanRefit).</summary>
    int32_t MechBay = 0;
};

/// <summary>
/// A building drawn like a tree (a VFX sprite with a frame of reference): it can burn, collapse, be captured, hold a
/// sensor, and serve as a refit point or mech bay.
/// </summary>
/// <remarks>Original source: <c>object\tbldng.cpp</c>, <c>object\tbldng.h</c>; 0x138 bytes.</remarks>
class MCTreeBuilding : public MCBigGameObject
{
public:
    /// <summary>Sets the frame to the identity and the building's fields to their defaults.</summary>
    /// <remarks>Inline in TreeBuildingType::createInstance (0x00694f50).</remarks>
    MCTreeBuilding();
    ~MCTreeBuilding() override { Destroy(); }

    /// <summary>Clears the four prison slots.</summary>
    void Init() override;
    /// <summary>
    /// Makes the VFX appearance, copies tonnage, explosion, combat value and refit flags from the type, loads its
    /// name, and sets up its team and sensor. A type with damage level 0 starts destroyed.
    /// </summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Frees the appearance, the sensor and the name.</summary>
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>On the first update, places the building in the world from its block, vertex and pixel offsets.</summary>
    int32_t Update() override;
    /// <summary>
    /// Burns the building, plays its collapse, draws it hazed by how much of it is revealed, plays its looping sound
    /// and draws the enemy blip.
    /// </summary>
    void Render() override;
    MCAppearance* GetAppearance() override { return Appearance; }
    /// <summary>Handles the ABL events that set (0x1c/0x1e) and clear (0x1d/0x1f) the flags at +0x28 and +0x2c.</summary>
    int32_t HandleEvent(MCObjectEvent* event) override;
    /// <summary>Returns the block and vertex the building stands on.</summary>
    void GetBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) override;
    /// <summary>
    /// Applies a weapon hit: past the damage level the building collapses (alarm raised for the shooter's side,
    /// marines let out, sound, sensor off, fire, explosion, salvage removed).
    /// </summary>
    int32_t HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    void KillFireObject() override { FireObject = nullptr; }
    /// <summary>
    /// Sets the pixel offsets from <paramref name="offset"/> and the vertex and block numbers from
    /// <paramref name="numbers"/>.
    /// </summary>
    void SetTerrainPosition(MCVector2D& offset, MCVector2D& numbers) override;
    /// <summary>The building's frame of reference.</summary>
    MCFrameOfRef GetFrame() override;
    void SetFrame(MCFrameOfRef& newFrame) override;
    /// <summary>Sets the alignment and, if not destroyed, moves the sensor to the matching team.</summary>
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
    /// <summary>For a refit building, the repair points left (damage level minus damage); otherwise 0.</summary>
    float GetRefitPoints() override;
    /// <summary>For a refit building, spends up to <paramref name="points"/> repair points (as damage).</summary>
    int BurnRefitPoints(float points) override;

    /// <summary>Gives the building a sensor of <paramref name="range"/> (if above -1), optionally on a team.</summary>
    virtual void SetSensorData(MCTeam* team, float range, int setTeam);

    /// <summary>Projects the building (and its shadow) for <paramref name="cam"/>; true when on screen.</summary>
    int IsVisible(MCCamera* cam);
    /// <summary>
    /// Sets the building on fire (making the fire object if needed) and adds burn time; without a fire type it takes
    /// 1 point of damage instead.
    /// </summary>
    void LightOnFire(float timeToBurn);
    /// <summary>Lets the type's number of marines out of the destroyed building, piloted by idle enemy warriors.</summary>
    void CreateBuildingMarines();

    /// <summary>Set until the first update has placed the building in the world.</summary>
    int32_t JustCreated = 0;
    /// <summary>The building's VFX appearance.</summary>
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
    /// <summary>The type's team (TeamTable[teamId]), set by init.</summary>
    MCTeam* TypeTeam = nullptr;
    /// <summary>Set while the collapse animation plays.</summary>
    int32_t Collapsing = 0;
    /// <summary>Set once the collapse animation has finished.</summary>
    int32_t Collapsed = 0;
    /// <summary>
    /// Set once the building has taken a hit (or starts destroyed); chooses the collapse animation (1 when clear, 4
    /// when set).
    /// </summary>
    int32_t HitOnce = 0;
    /// <summary>The building's frame of reference (identity at construction).</summary>
    MCFrameOfRef Frame;
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
    /// tbldng.cpp sets them; capturing the building moves them into the capturing vehicle's passenger seats.</summary>
    MCMechWarrior* PrisonSlots[4]{};
    /// <summary>Whether the building repairs units (from the type).</summary>
    int32_t CanRefit = 0;
    /// <summary>Whether the refit building is a mech bay (from the type).</summary>
    int32_t MechBay = 0;
    /// <summary>The unit being refitted here, if any (checked by the ABL refit order; cleared by init).</summary>
    MCGameObject* RefitBuddy = nullptr;
};
