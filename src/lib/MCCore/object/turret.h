#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class MCAppearance;
class MCCamera;
class MCFile;
class MCFire;
class MCGameObject;
class MCObjectEvent;
class MCSmoke;
class MCWeaponFireChunk;
struct MCWeaponShotInfo;

/// <summary>
/// The type of a <see cref="MCTurret"/>: its damage levels and effects, its single weapon, attack radius, yaw rate and
/// pilot skill, and the pixel offsets of its base, muzzle and centre.
/// </summary>
/// <remarks>Original source: <c>object\turret.cpp</c>, <c>object\turret.h</c>; 0x80 bytes. Read from the "TurretData" block of its FIT.</remarks>
class MCTurretType : public MCObjectType
{
public:
    MCTurretType() { Init(); }
    ~MCTurretType() override { Destroy(); }

    /// <summary>Clears the base type fields and the turret's; the effect ids and weapon type start at -1.</summary>
    void Init();

    /// <summary>Makes a <see cref="MCTurret"/> of this type and gives it the next object id.</summary>
    MCBaseObject* CreateInstance() override;
    void Destroy() override;
    /// <summary>Reads the "TurretData" block (with defaults for the optional keys), then the common type data.</summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>
    /// An object entering the turret's range: an enemy mover (or object class 0x1c) that is alive and closer than the
    /// current target becomes the target. Always 1 (the turret isn't blocked).
    /// </summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>Damage that destroys the turret (FIT "DmgLevel").</summary>
    uint32_t DmgLevel = 0;
    /// <summary>Damage level while closed (FIT "DmgLevelClosed"; defaults to dmgLevel).</summary>
    uint32_t DmgLevelClosed = 0;
    /// <summary>Object type made when the turret is destroyed, e.g. a fire (FIT "BlownEffectId"; -1 = none).</summary>
    uint32_t BlownEffectId = 0;
    /// <summary>FIT "NormalEffectId" (-1 = none).</summary>
    uint32_t NormalEffectId = 0;
    /// <summary>FIT "DamageEffectId" (-1 = none).</summary>
    uint32_t DamageEffectId = 0;
    /// <summary>FIT "Tonnage" (default 20).</summary>
    float Tonnage = 0;
    /// <summary>FIT "BasePixelOffsetX".</summary>
    int32_t BasePixelOffsetX = 0;
    /// <summary>FIT "BasePixelOffsetY".</summary>
    int32_t BasePixelOffsetY = 0;
    /// <summary>FIT "ExplosionDamage".</summary>
    float ExplosionDamage = 0;
    /// <summary>FIT "ExplosionRadius".</summary>
    float ExplosionRadius = 0;
    /// <summary>FIT "LittleExtent" (default 20).</summary>
    float LittleExtent = 0;
    /// <summary>FIT "AttackRadius"; when nonzero it becomes the type's extent radius.</summary>
    float AttackRadius = 0;
    /// <summary>Degrees per second the turret turns (FIT "MaxTurretYawRate").</summary>
    float MaxTurretYawRate = 0;
    /// <summary>The weapon's master component id (FIT "WeaponType").</summary>
    int32_t WeaponType = 0;
    /// <summary>Added to the attack chance (FIT "PilotSkill").</summary>
    int32_t PilotSkill = 0;
    /// <summary>String resource id of the turret's name (FIT "BuildingName", default 0xa4).</summary>
    int32_t BuildingName = 0;
    /// <summary>FIT "FireOffsetX".</summary>
    int32_t FireOffsetX = 0;
    /// <summary>FIT "FireOffsetY".</summary>
    int32_t FireOffsetY = 0;
    /// <summary>FIT "CenterOffsetX".</summary>
    int32_t CenterOffsetX = 0;
    /// <summary>FIT "CenterOffsetY".</summary>
    int32_t CenterOffsetY = 0;
};

/// <summary>
/// A gun turret fixed to a terrain vertex: it picks the nearest enemy that enters its attack radius, turns toward it,
/// pops up (for pop-up turrets) and fires its single weapon; destroyed, it burns and smokes.
/// </summary>
/// <remarks>Original source: <c>object\turret.cpp</c>, <c>object\turret.h</c>; 0x168 bytes. Object class 0x1e.</remarks>
class MCTurret : public MCBigGameObject
{
public:
    /// <summary>The weapon fire chunk lists: those to send, and those received.</summary>
    static constexpr int32_t NUM_CHUNK_LISTS = 2;
    /// <summary>The room of each weapon fire chunk list.</summary>
    static constexpr int32_t MAX_WEAPONFIRE_CHUNKS = 8;

    /// <summary>Sets the fields as TurretType::createInstance's inlined constructor does, then init().</summary>
    MCTurret();
    ~MCTurret() override { Destroy(); }

    /// <summary>Empty: the constructor sets the fields.</summary>
    void Init() override;
    /// <summary>Makes the GV (fixed) or PU (pop-up) appearance and takes the type's weapon, tonnage and name.</summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Destroys the appearance, lets go of the fire, and frees the name.</summary>
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>
    /// Places the turret on its first update; then drops a target out of range, reveals the map around a player
    /// turret, turns toward the target, opens or closes, fires when ready, and plays received weapon fire.
    /// </summary>
    int32_t Update() override;
    void Render() override;
    MCAppearance* GetAppearance() override { return Appearance; }
    /// <summary>Sets the flags +0x28 and +0x2c on events 0x1c..0x1f.</summary>
    int32_t HandleEvent(MCObjectEvent* event) override;
    /// <summary>
    /// The world position of a hot spot: the type's centre offset, plus (for any node but -1) the muzzle turned with
    /// the turret.
    /// </summary>
    MCVector3D GetPositionFromHS(uint32_t nodeId) override;
    void GetBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) override
    {
        blockNum = BlockNumber;
        vertexNum = VertexNumber;
    }

    /// <summary>Takes the damage; past the type's damage level the turret is destroyed, catches fire and explodes.</summary>
    int32_t HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    void KillFireObject() override { FireObject = nullptr; }
    /// <summary>Stores the tile offset and the vertex and block numbers the turret stands on.</summary>
    void SetTerrainPosition(MCVector2D& position, MCVector2D& numbers) override
    {
        TileOffsetX = static_cast<int32_t>(position.X);
        TileOffsetY = static_cast<int32_t>(position.Y);
        VertexNumber = static_cast<int32_t>(numbers.X);
        BlockNumber = static_cast<int32_t>(numbers.Y);
    }

    /// <summary>Changes sides and drops the target.</summary>
    void SetAlignment(int32_t align) override;
    /// <summary>Whether the target's vertex is seen by the turret's side and a clear line of fire reaches it in range.</summary>
    int LineOfFire(MCGameObject* target) override;
    int IsBuilding() override { return 1; }
    /// <summary>Whether the turret's vertex is seen by the home team.</summary>
    int IsRevealed() override;

    /// <summary>Plays the weapon fire chunks of list <paramref name="which"/> and empties it.</summary>
    virtual int32_t UpdateWeaponFireChunks(int32_t which);

    /// <summary>Projects the turret's vertex; true when the appearance is visible to <paramref name="cam"/>.</summary>
    int IsVisible(MCCamera* cam);
    /// <summary>Recycled, open and enabled.</summary>
    int IsWeaponReady();
    /// <summary>Whether the weapon's master component is a missile launcher (weapon type 9).</summary>
    int IsWeaponMissile();
    /// <summary>Whether the weapon is a Streak launcher.</summary>
    int IsWeaponStreak();
    /// <summary>
    /// The chance to hit <paramref name="target"/>: the range band's modifier plus the pilot skill, better against a
    /// still target; the band (short, medium, long) in <paramref name="range"/>.
    /// </summary>
    float CalcAttackChance(MCGameObject* target, int32_t* range);
    void RecordWeaponFireTime();
    /// <summary>The weapon is ready again after its master component's recycle time.</summary>
    void StartWeaponRecycle();
    /// <summary>Empties chunk list <paramref name="which"/>.</summary>
    /// <returns>How many chunks it held.</returns>
    int32_t ClearWeaponFireChunks(int32_t which);
    /// <summary>Packs a chunk and appends it to list <paramref name="which"/>.</summary>
    int32_t AddWeaponFireChunk(int32_t which, MCWeaponFireChunk* chunk);
    /// <summary>Appends <paramref name="numChunks"/> packed chunks to list <paramref name="which"/>.</summary>
    int32_t AddWeaponFireChunks(int32_t which, uint32_t* packedChunkBuffer, int32_t numChunks);
    /// <summary>Copies the packed chunks of list <paramref name="which"/> out.</summary>
    int32_t GrabWeaponFireChunks(int32_t which, uint32_t* packedChunkBuffer);
    /// <summary>Rolls the shot at <paramref name="target"/> and fires (recording a chunk in multiplayer).</summary>
    void FireWeapon(MCGameObject* target);
    /// <summary>Launches the weapon's shot (bullet, laser, missiles) at a target or a point, with its result.</summary>
    int32_t HandleWeaponFire(int32_t weaponIndex, MCGameObject* target, MCVector3D* targetPoint, int hit,
                             float entryAngle, int32_t numMissiles, int32_t numHits, int32_t numAntiMissiles,
                             int32_t hitLocation);
    /// <summary>
    /// Sets the turret burning for <paramref name="timeToBurn"/> more seconds (making its blown effect fire if need
    /// be); without a blown effect it takes a point of damage instead.
    /// </summary>
    void LightOnFire(float timeToBurn);
    /// <summary>Whether the turret's vertex is seen by the home team's enemy.</summary>
    int EnemyRevealed();

    /// <summary>Set until the first update places the turret on the terrain.</summary>
    int32_t JustCreated = 0;
    /// <summary>A GVAppearance (fixed turret) or PUAppearance (pop-up turret).</summary>
    MCAppearance* Appearance = nullptr;
    /// <summary>Offset of the turret within its tile (from setTerrainPosition), turned into the world position.</summary>
    int32_t TileOffsetX = 0;
    int32_t TileOffsetY = 0;
    /// <summary>The terrain vertex the turret stands on, within its block.</summary>
    int32_t VertexNumber = 0;
    /// <summary>The terrain block the turret stands on.</summary>
    int32_t BlockNumber = 0;
    /// <summary>The map tile column of the turret's vertex (vertices and tiles share a grid).</summary>
    int32_t TileCol = 0;
    /// <summary>The map tile row of the turret's vertex.</summary>
    int32_t TileRow = 0;
    /// <summary>World x of the tile.</summary>
    float TilePositionX = 0;
    /// <summary>World y of the tile.</summary>
    float TilePositionY = 0;
    /// <summary>The tile's elevation in metres (from the scenario map).</summary>
    float TileElevation = 0;
    /// <summary>Set once the turret has been set on fire.</summary>
    int32_t OnFire = 0;
    /// <summary>The fire burning on the turret (its owner points back at the turret).</summary>
    MCFire* FireObject = nullptr;
    /// <summary>Set when the turret is destroyed: it no longer acts.</summary>
    int32_t Destroyed = 0;
    /// <summary>The turret's yaw in degrees, turned toward the target at the type's yaw rate.</summary>
    float TurretRotation = 0;
    /// <summary>The result of getAwake, taken every update.</summary>
    int32_t Awake = 0;
    /// <summary>Set when the turret is open (a pop-up turret has finished rising; always for a fixed one).</summary>
    int32_t WeaponDeployed = 0;
    /// <summary>1 for a fixed (GV appearance) turret, 0 for a pop-up (PU appearance) one.</summary>
    int32_t FixedTurret = 0;
    /// <summary>
    /// Must be nonzero for the weapon to be ready. Original behaviour (OB-014): nothing in MCX.EXE writes it, so the
    /// original read whatever the object heap held; the port's constructor sets it (see there).
    /// </summary>
    int32_t WeaponEnabled = 0;
    /// <summary>The turret's name, loaded from the type's string resource .</summary>
    std::string Name;
    /// <summary>Set once a player-side turret has revealed the map around it (visibility flag 1).</summary>
    int32_t MarkedSeenInnerSphere = 0;
    /// <summary>Set once a clan-side turret has revealed the map around it (visibility flag 2).</summary>
    int32_t MarkedSeenClan = 0;
    /// <summary>The object the turret is shooting at.</summary>
    MCGameObject* Target = nullptr;
    /// <summary>The scenario time the weapon is recycled at.</summary>
    float ReadyTime = 0;
    /// <summary>Counts down by the frame length while the destroyed turret's smoke plays; the smoke goes at its end.</summary>
    float SmokeTime = 0;
    /// <summary>The scenario time of the last shot.</summary>
    float LastFireTime = 0;
    /// <summary>Smoke rising from the destroyed turret.</summary>
    MCSmoke* Smoke = nullptr;
    /// <summary>The turret's index in the multiplayer turret roster, or -1.</summary>
    int32_t NetRosterIndex = 0;
    /// <summary>How many chunks each weapon fire chunk list holds.</summary>
    int32_t NumWeaponFireChunks[NUM_CHUNK_LISTS]{};
    /// <summary>
    /// The packed weapon fire chunks. The original's add functions check a list against 0x80 (Mover's limit), not
    /// the 8 entries here; the port checks against 8.
    /// </summary>
    uint32_t WeaponFireChunks[NUM_CHUNK_LISTS][MAX_WEAPONFIRE_CHUNKS]{};
};
