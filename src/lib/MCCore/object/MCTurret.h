#pragma once

#include "object/MCBigGameObject.h"
#include "object/MCWeaponShotInfo.h"

class MCAppearance;
class MCCamera;
class MCFire;
class MCGameObject;
class MCObjectEvent;
class MCSmoke;
class MCWeaponFireChunk;

/// <summary>
/// A gun turret fixed to a terrain vertex: it picks the nearest enemy that enters its attack radius, turns toward it,
/// pops up (for pop-up turrets) and fires its single weapon; destroyed, it burns and smokes.
/// </summary>
/// <remarks>Original source: <c>object\turret.cpp</c>, <c>object\turret.h</c>. Object class 0x1e.</remarks>
class MCTurret : public MCBigGameObject
{
public:
    /// <summary>The weapon fire chunk lists: those to send, and those received.</summary>
    static constexpr int32_t NumChunkLists = 2;

    MCTurret();
    /// <summary>Lets go of the fire (taking it off the contacts).</summary>
    ~MCTurret() override;

    /// <summary>Makes the GV (fixed) or PU (pop-up) appearance and takes the type's weapon, tonnage and name.</summary>
    int32_t Init(MCObjectType* objType) override;
    using MCBigGameObject::Init;
    int32_t Kill() override { return 0; }
    /// <summary>
    /// Places the turret on its first update; then drops a target out of range, reveals the map around a player
    /// turret, turns toward the target, opens or closes, fires when ready, and plays received weapon fire.
    /// </summary>
    int32_t Update() override;
    void Render() override;
    MCAppearance* GetAppearance() override { return Appearance.get(); }
    /// <summary>Wakes the turret, or puts it to sleep, on events 0x1c..0x1f.</summary>
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
    /// <summary>The fire burnt out, on its own list's update: its list owns it, so the turret lets go. (The
    /// turret never runs its fire's update, so the fire doesn't get there.)</summary>
    void KillFireObject() override;
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
    int32_t UpdateWeaponFireChunks(int32_t which);
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
    /// <summary>Records the scenario time of the shot.</summary>
    void RecordWeaponFireTime();
    /// <summary>The weapon is ready again after its master component's recycle time.</summary>
    void StartWeaponRecycle();
    /// <summary>Empties chunk list <paramref name="which"/>.</summary>
    /// <returns>How many chunks it held.</returns>
    int32_t ClearWeaponFireChunks(int32_t which);
    /// <summary>Packs a chunk and appends it to list <paramref name="which"/>.</summary>
    /// <returns>The list's chunk count.</returns>
    int32_t AddWeaponFireChunk(int32_t which, MCWeaponFireChunk* chunk);
    /// <summary>Appends packed chunks to list <paramref name="which"/>.</summary>
    /// <returns>The list's chunk count.</returns>
    int32_t AddWeaponFireChunks(int32_t which, std::span<const uint32_t> packedChunks);
    /// <summary>Copies the packed chunks of list <paramref name="which"/> out, as many as
    /// <paramref name="packedChunks"/> holds (the list keeps them).</summary>
    /// <returns>How many were copied.</returns>
    int32_t GrabWeaponFireChunks(int32_t which, std::span<uint32_t> packedChunks);
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
    bool JustCreated = true;
    /// <summary>A GVAppearance (fixed turret) or PUAppearance (pop-up turret).</summary>
    std::unique_ptr<MCAppearance> Appearance;
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
    bool OnFire = false;
    /// <summary>The fire burning on the turret (its owner points back at the turret); the turret owns it.</summary>
    std::unique_ptr<MCFire> FireObject;
    /// <summary>Set when the turret is destroyed: it no longer acts.</summary>
    bool Destroyed = false;
    /// <summary>The turret's yaw in degrees, turned toward the target at the type's yaw rate.</summary>
    float TurretRotation = 0;
    /// <summary>The result of getAwake, taken every update.</summary>
    bool Awake = true;
    /// <summary>Set when the turret is open (a pop-up turret has finished rising; always for a fixed one).</summary>
    bool WeaponDeployed = true;
    /// <summary>True for a fixed (GV appearance) turret, false for a pop-up (PU appearance) one.</summary>
    bool FixedTurret = false;
    /// <summary>
    /// Must be set for the weapon to be ready. Original behaviour (OB-014): nothing in MCX.EXE writes it, so the
    /// original read whatever the object heap held; the port's turrets are armed.
    /// </summary>
    bool WeaponEnabled = true;
    /// <summary>The turret's name, loaded from the type's string resource.</summary>
    std::string Name;
    /// <summary>Set once a player-side turret has revealed the map around it (visibility flag 1).</summary>
    bool MarkedSeenInnerSphere = false;
    /// <summary>Set once a clan-side turret has revealed the map around it (visibility flag 2).</summary>
    bool MarkedSeenClan = false;
    /// <summary>The object the turret is shooting at.</summary>
    MCGameObject* Target = nullptr;
    /// <summary>The scenario time the weapon is recycled at.</summary>
    float ReadyTime = 0;
    /// <summary>Counts down by the frame length while the destroyed turret's smoke plays; the smoke goes at its end.</summary>
    float SmokeTime = 0;
    /// <summary>The scenario time of the last shot.</summary>
    float LastFireTime = 0;
    /// <summary>Smoke rising from the destroyed turret; the turret owns it.</summary>
    std::unique_ptr<MCSmoke> Smoke;
    /// <summary>The turret's index in the multiplayer turret roster, or -1.</summary>
    int32_t NetRosterIndex = -1;
    /// <summary>The packed weapon fire chunks of each list (to send, received).</summary>
    std::array<std::vector<uint32_t>, NumChunkLists> WeaponFireChunks;
};
