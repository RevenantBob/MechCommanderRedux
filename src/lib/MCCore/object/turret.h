#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class Appearance;
class Camera;
class File;
class Fire;
class GameObject;
class ObjectEvent;
class Smoke;
class WeaponFireChunk;
struct _WeaponShotInfo;

/// <summary>
/// The type of a <see cref="Turret"/>: its damage levels and effects, its single weapon, attack radius, yaw rate and
/// pilot skill, and the pixel offsets of its base, muzzle and centre.
/// </summary>
/// <remarks>Original source: <c>object\turret.cpp</c>, <c>object\turret.h</c>; 0x80 bytes. Read from the "TurretData" block of its FIT.</remarks>
class TurretType : public ObjectType
{
public:
    TurretType() { init(); }
    /// <remarks>MCX.EXE @ 0x00690ad0 (vector deleting destructor)</remarks>
    ~TurretType() override { destroy(); }

    /// <summary>Clears the base type fields and the turret's; the effect ids and weapon type start at -1.</summary>
    /// <remarks>MCX.EXE @ 0x00690a80 (inline in <c>object\turret.h</c>)</remarks>
    void init();

    /// <summary>Makes a <see cref="Turret"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x0069d960</remarks>
    BaseObject* createInstance() override;
    /// <remarks>MCX.EXE @ 0x0069db70</remarks>
    void destroy() override;
    /// <summary>Reads the "TurretData" block (with defaults for the optional keys), then the common type data.</summary>
    /// <remarks>MCX.EXE @ 0x0069db80</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <summary>
    /// An object entering the turret's range: an enemy mover (or object class 0x1c) that is alive and closer than the
    /// current target becomes the target. Always 1 (the turret isn't blocked).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0069de70</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <remarks>MCX.EXE @ 0x0069e000</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>Damage that destroys the turret (FIT "DmgLevel").</summary>
    uint32_t dmgLevel = 0; // +0x30
    /// <summary>Damage level while closed (FIT "DmgLevelClosed"; defaults to dmgLevel).</summary>
    uint32_t dmgLevelClosed = 0; // +0x34
    /// <summary>Object type made when the turret is destroyed, e.g. a fire (FIT "BlownEffectId"; -1 = none).</summary>
    uint32_t blownEffectId = 0; // +0x38
    /// <summary>FIT "NormalEffectId" (-1 = none).</summary>
    uint32_t normalEffectId = 0; // +0x3c
    /// <summary>FIT "DamageEffectId" (-1 = none).</summary>
    uint32_t damageEffectId = 0; // +0x40
    /// <summary>FIT "Tonnage" (default 20).</summary>
    float tonnage = 0; // +0x44
    /// <summary>FIT "BasePixelOffsetX".</summary>
    int32_t basePixelOffsetX = 0; // +0x48
    /// <summary>FIT "BasePixelOffsetY".</summary>
    int32_t basePixelOffsetY = 0; // +0x4c
    /// <summary>FIT "ExplosionDamage".</summary>
    float explosionDamage = 0; // +0x50
    /// <summary>FIT "ExplosionRadius".</summary>
    float explosionRadius = 0; // +0x54
    /// <summary>FIT "LittleExtent" (default 20).</summary>
    float littleExtent = 0; // +0x58
    /// <summary>FIT "AttackRadius"; when nonzero it becomes the type's extent radius.</summary>
    float attackRadius = 0; // +0x5c
    /// <summary>Degrees per second the turret turns (FIT "MaxTurretYawRate").</summary>
    float maxTurretYawRate = 0; // +0x60
    /// <summary>The weapon's master component id (FIT "WeaponType").</summary>
    int32_t weaponType = 0; // +0x64
    /// <summary>Added to the attack chance (FIT "PilotSkill").</summary>
    int32_t pilotSkill = 0; // +0x68
    /// <summary>String resource id of the turret's name (FIT "BuildingName", default 0xa4).</summary>
    int32_t buildingName = 0; // +0x6c
    /// <summary>FIT "FireOffsetX".</summary>
    int32_t fireOffsetX = 0; // +0x70
    /// <summary>FIT "FireOffsetY".</summary>
    int32_t fireOffsetY = 0; // +0x74
    /// <summary>FIT "CenterOffsetX".</summary>
    int32_t centerOffsetX = 0; // +0x78
    /// <summary>FIT "CenterOffsetY".</summary>
    int32_t centerOffsetY = 0; // +0x7c
};

/// <summary>
/// A gun turret fixed to a terrain vertex: it picks the nearest enemy that enters its attack radius, turns toward it,
/// pops up (for pop-up turrets) and fires its single weapon; destroyed, it burns and smokes.
/// </summary>
/// <remarks>Original source: <c>object\turret.cpp</c>, <c>object\turret.h</c>; 0x168 bytes. Object class 0x1e.</remarks>
class Turret : public BigGameObject
{
public:
    /// <summary>The weapon fire chunk lists: those to send, and those received.</summary>
    static constexpr int32_t NUM_CHUNK_LISTS = 2;
    /// <summary>The room of each weapon fire chunk list.</summary>
    static constexpr int32_t MAX_WEAPONFIRE_CHUNKS = 8;

    /// <summary>Sets the fields as TurretType::createInstance's inlined constructor does, then init().</summary>
    Turret();
    /// <remarks>MCX.EXE @ 0x0069db20 (vector deleting destructor)</remarks>
    ~Turret() override { destroy(); }

    /// <summary>Empty: the constructor sets the fields.</summary>
    /// <remarks>MCX.EXE @ 0x0069da60 (inline in <c>object\turret.h</c>)</remarks>
    void init() override;
    /// <summary>Makes the GV (fixed) or PU (pop-up) appearance and takes the type's weapon, tonnage and name.</summary>
    /// <remarks>MCX.EXE @ 0x006a1300</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Destroys the appearance, lets go of the fire, and frees the name.</summary>
    /// <remarks>MCX.EXE @ 0x006a1290</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x0069dae0</remarks>
    int32_t kill() override { return 0; }
    /// <summary>
    /// Places the turret on its first update; then drops a target out of range, reveals the map around a player
    /// turret, turns toward the target, opens or closes, fires when ready, and plays received weapon fire.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0069e080</remarks>
    int32_t update() override;
    /// <remarks>MCX.EXE @ 0x006a0cd0</remarks>
    void render() override;
    /// <remarks>MCX.EXE @ 0x0069dad0</remarks>
    Appearance* getAppearance() override { return appearance; }
    /// <summary>Sets the flags +0x28 and +0x2c on events 0x1c..0x1f.</summary>
    /// <remarks>MCX.EXE @ 0x006a09c0</remarks>
    int32_t handleEvent(ObjectEvent* event) override;
    /// <summary>
    /// The world position of a hot spot: the type's centre offset, plus (for any node but -1) the muzzle turned with
    /// the turret.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0069edc0</remarks>
    vector_3d getPositionFromHS(uint32_t nodeId) override;
    /// <remarks>MCX.EXE @ 0x0069db00 (inline in <c>object\turret.h</c>)</remarks>
    void getBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) override
    {
        blockNum = blockNumber;
        vertexNum = vertexNumber;
    }

    /// <summary>Takes the damage; past the type's damage level the turret is destroyed, catches fire and explodes.</summary>
    /// <remarks>MCX.EXE @ 0x006a15c0</remarks>
    int32_t handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <remarks>MCX.EXE @ 0x0069da70</remarks>
    void killFireObject() override { fireObject = nullptr; }
    /// <summary>Stores the tile offset and the vertex and block numbers the turret stands on.</summary>
    /// <remarks>MCX.EXE @ 0x0069da80 (inline in <c>object\turret.h</c>)</remarks>
    void setTerrainPosition(vector_2d& position, vector_2d& numbers) override
    {
        tileOffsetX = static_cast<int32_t>(position.x);
        tileOffsetY = static_cast<int32_t>(position.y);
        vertexNumber = static_cast<int32_t>(numbers.x);
        blockNumber = static_cast<int32_t>(numbers.y);
    }

    /// <summary>Changes sides and drops the target.</summary>
    /// <remarks>MCX.EXE @ 0x006a09a0</remarks>
    void setAlignment(int32_t align) override;
    /// <summary>Whether the target's vertex is seen by the turret's side and a clear line of fire reaches it in range.</summary>
    /// <remarks>MCX.EXE @ 0x0069e850</remarks>
    int lineOfFire(GameObject* target) override;
    /// <remarks>MCX.EXE @ 0x0069daf0</remarks>
    int isBuilding() override { return 1; }
    /// <summary>Whether the turret's vertex is seen by the home team.</summary>
    /// <remarks>MCX.EXE @ 0x006a0b10</remarks>
    int isRevealed() override;

    /// <summary>Plays the weapon fire chunks of list <paramref name="which"/> and empties it.</summary>
    /// <remarks>MCX.EXE @ 0x0069eb50</remarks>
    virtual int32_t updateWeaponFireChunks(int32_t which);

    /// <summary>Projects the turret's vertex; true when the appearance is visible to <paramref name="cam"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0069e010</remarks>
    int isVisible(Camera* cam);
    /// <summary>Recycled, open and enabled.</summary>
    /// <remarks>MCX.EXE @ 0x0069e680</remarks>
    int isWeaponReady();
    /// <summary>Whether the weapon's master component is a missile launcher (weapon type 9).</summary>
    /// <remarks>MCX.EXE @ 0x0069e6b0</remarks>
    int isWeaponMissile();
    /// <summary>Whether the weapon is a Streak launcher.</summary>
    /// <remarks>MCX.EXE @ 0x0069e6e0</remarks>
    int isWeaponStreak();
    /// <summary>
    /// The chance to hit <paramref name="target"/>: the range band's modifier plus the pilot skill, better against a
    /// still target; the band (short, medium, long) in <paramref name="range"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0069e700</remarks>
    float calcAttackChance(GameObject* target, int32_t* range);
    /// <remarks>MCX.EXE @ 0x0069e990</remarks>
    void recordWeaponFireTime();
    /// <summary>The weapon is ready again after its master component's recycle time.</summary>
    /// <remarks>MCX.EXE @ 0x0069e9a0</remarks>
    void startWeaponRecycle();
    /// <summary>Empties chunk list <paramref name="which"/>.</summary>
    /// <returns>How many chunks it held.</returns>
    /// <remarks>MCX.EXE @ 0x0069e9d0</remarks>
    int32_t clearWeaponFireChunks(int32_t which);
    /// <summary>Packs a chunk and appends it to list <paramref name="which"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0069e9f0</remarks>
    int32_t addWeaponFireChunk(int32_t which, WeaponFireChunk* chunk);
    /// <summary>Appends <paramref name="numChunks"/> packed chunks to list <paramref name="which"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0069ea50</remarks>
    int32_t addWeaponFireChunks(int32_t which, uint32_t* packedChunkBuffer, int32_t numChunks);
    /// <summary>Copies the packed chunks of list <paramref name="which"/> out.</summary>
    /// <remarks>MCX.EXE @ 0x0069eb20</remarks>
    int32_t grabWeaponFireChunks(int32_t which, uint32_t* packedChunkBuffer);
    /// <summary>Rolls the shot at <paramref name="target"/> and fires (recording a chunk in multiplayer).</summary>
    /// <remarks>MCX.EXE @ 0x0069ee80</remarks>
    void fireWeapon(GameObject* target);
    /// <summary>Launches the weapon's shot (bullet, laser, missiles) at a target or a point, with its result.</summary>
    /// <remarks>MCX.EXE @ 0x0069ff30</remarks>
    int32_t handleWeaponFire(int32_t weaponIndex, GameObject* target, vector_3d* targetPoint, int hit, float entryAngle,
                             int32_t numMissiles, int32_t numHits, int32_t numAntiMissiles, int32_t hitLocation);
    /// <summary>
    /// Sets the turret burning for <paramref name="timeToBurn"/> more seconds (making its blown effect fire if need
    /// be); without a blown effect it takes a point of damage instead.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006a0a20</remarks>
    void lightOnFire(float timeToBurn);
    /// <summary>Whether the turret's vertex is seen by the home team's enemy.</summary>
    /// <remarks>MCX.EXE @ 0x006a0bf0</remarks>
    int enemyRevealed();

    /// <summary>Set until the first update places the turret on the terrain.</summary>
    int32_t justCreated = 0; // +0x84
    /// <summary>A GVAppearance (fixed turret) or PUAppearance (pop-up turret).</summary>
    Appearance* appearance = nullptr; // +0x88
    /// <summary>Offset of the turret within its tile (from setTerrainPosition), turned into the world position.</summary>
    int32_t tileOffsetX = 0; // +0x8c
    int32_t tileOffsetY = 0; // +0x90
    /// <summary>The terrain vertex the turret stands on, within its block.</summary>
    int32_t vertexNumber = 0; // +0x94
    /// <summary>The terrain block the turret stands on.</summary>
    int32_t blockNumber = 0; // +0x98
    /// <summary>The map tile column of the turret's vertex (vertices and tiles share a grid).</summary>
    int32_t tileCol = 0; // +0xb4
    /// <summary>The map tile row of the turret's vertex.</summary>
    int32_t tileRow = 0; // +0xb8
    /// <summary>World x of the tile.</summary>
    float tilePositionX = 0; // +0xbc
    /// <summary>World y of the tile.</summary>
    float tilePositionY = 0; // +0xc0
    /// <summary>The tile's elevation in metres (from the scenario map).</summary>
    float tileElevation = 0; // +0xc4
    /// <summary>Set once the turret has been set on fire.</summary>
    int32_t onFire = 0; // +0xc8
    /// <summary>The fire burning on the turret (its owner points back at the turret).</summary>
    Fire* fireObject = nullptr; // +0xcc
    /// <summary>Set when the turret is destroyed: it no longer acts.</summary>
    int32_t destroyed = 0; // +0xd4
    /// <summary>The turret's yaw in degrees, turned toward the target at the type's yaw rate.</summary>
    float turretRotation = 0; // +0xd8
    /// <summary>The result of getAwake, taken every update.</summary>
    int32_t awake = 0; // +0xe0
    /// <summary>Set when the turret is open (a pop-up turret has finished rising; always for a fixed one).</summary>
    int32_t weaponDeployed = 0; // +0xe4
    /// <summary>1 for a fixed (GV appearance) turret, 0 for a pop-up (PU appearance) one.</summary>
    int32_t fixedTurret = 0; // +0xe8
    /// <summary>
    /// Must be nonzero for the weapon to be ready. Original behaviour (OB-014): nothing in MCX.EXE writes it, so the
    /// original read whatever the object heap held; the port's constructor sets it (see there).
    /// </summary>
    int32_t weaponEnabled = 0; // +0xf0
    /// <summary>The turret's name, loaded from the type's string resource .</summary>
    std::string name; // +0xf4
    /// <summary>Set once a player-side turret has revealed the map around it (visibility flag 1).</summary>
    int32_t markedSeenInnerSphere = 0; // +0xf8
    /// <summary>Set once a clan-side turret has revealed the map around it (visibility flag 2).</summary>
    int32_t markedSeenClan = 0; // +0xfc
    /// <summary>The object the turret is shooting at.</summary>
    GameObject* target = nullptr; // +0x104
    /// <summary>The scenario time the weapon is recycled at.</summary>
    float readyTime = 0; // +0x108
    /// <summary>Counts down by the frame length while the destroyed turret's smoke plays; the smoke goes at its end.</summary>
    float smokeTime = 0; // +0x10c
    /// <summary>The scenario time of the last shot.</summary>
    float lastFireTime = 0; // +0x110
    /// <summary>Smoke rising from the destroyed turret.</summary>
    Smoke* smoke = nullptr; // +0x114
    /// <summary>The turret's index in the multiplayer turret roster, or -1.</summary>
    int32_t netRosterIndex = 0; // +0x11c
    /// <summary>How many chunks each weapon fire chunk list holds.</summary>
    int32_t numWeaponFireChunks[NUM_CHUNK_LISTS]{}; // +0x120
    /// <summary>
    /// The packed weapon fire chunks. The original's add functions check a list against 0x80 (Mover's limit), not
    /// the 8 entries here; the port checks against 8.
    /// </summary>
    uint32_t weaponFireChunks[NUM_CHUNK_LISTS][MAX_WEAPONFIRE_CHUNKS]{}; // +0x128
};
