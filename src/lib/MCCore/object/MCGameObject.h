#pragma once

#include "object/MCBaseObject.h"
#include "object/MCSalvageItem.h"

class MCFile;
class MCFitIniFile;
class MCMechWarrior;
class MCTeam;
struct MCObjectPosition;
struct MCPotentialContact;
struct MCWeaponShotInfo;

/// <summary>A mech's weight class by tonnage (<see cref="MCGameObject::GetMechClass"/>); names are the port's.</summary>
enum class MCMechClass : int32_t
{
    /// <summary>Not a mech.</summary>
    None = 0,
    /// <summary>Under 35 tons.</summary>
    Light = 1,
    /// <summary>35 to under 55 tons.</summary>
    Medium = 2,
    /// <summary>55 to under 75 tons.</summary>
    Heavy = 3,
    /// <summary>75 tons and more.</summary>
    Assault = 4
};

/// <summary>
/// An object that lives in the world: it has a type, a position, a status (normal, disabled, destroyed) and an
/// alignment, and the virtual interface every weapon, sensor, AI and interface routine talks to. Most of the
/// interface does nothing here and is filled in by <see cref="MCBigGameObject"/> and the movers.
/// </summary>
/// <remarks>Original source: <c>object\gameobj.h</c>, <c>object\gameobj.cpp</c>.</remarks>
class MCGameObject : public MCBaseObject
{
public:
    MCGameObject() { ObjectClass = MCObjectClass::GameObject; }
    /// <summary>Releases the object's type.</summary>
    ~MCGameObject() override;

    /// <summary>Takes the type and its alignment.</summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Resets the fields: class GAMEOBJECT, no type, position or status.</summary>
    /// <remarks>The laser re-runs it on a live object (its own Init()).</remarks>
    void Init() override;
    MCObjectType* GetObjectType() override { return ObjType; }
    int32_t Kill() override { return 0; }
    int32_t Update() override { return 0; }
    void Render() override {}
    /// <summary>The object's own position (a plain object has no hot spots).</summary>
    MCVector3D GetPositionFromHS(uint32_t hotSpot) override;
    int GetUseMe() override { return 0; }
    /// <summary>The terrain block and vertex under the object's position.</summary>
    void GetBlockAndVertexNumber(int32_t& blockNumber, int32_t& vertexNumber) override;

    /// <summary>Reads the object's own data from a FIT file (nothing here).</summary>
    virtual int32_t Init(MCFitIniFile* objFile) { return 0; }
    /// <summary>Whether the object is on screen (updates its screen position).</summary>
    virtual int OnScreen() { return 0; }
    /// <summary>
    /// Picks the body location a shot from <paramref name="attacker"/>'s weapon <paramref name="weaponIndex"/>
    /// hits (-1: no locations).
    /// </summary>
    virtual int32_t CalcHitLocation(MCGameObject* attacker, int32_t weaponIndex, int32_t attackSource,
                                    int32_t attackType)
    {
        return -1;
    }

    /// <summary>Spends <paramref name="numShots"/> of the anti-missile system's ammo.</summary>
    virtual void ReduceAntiMissileAmmo(int32_t numShots) {}
    /// <summary>
    /// Fires the anti-missile system at <paramref name="numMissiles"/> incoming missiles.
    /// </summary>
    /// <returns>The missiles that get through; <paramref name="antiMissileShots"/> gets the shots fired.</returns>
    virtual int32_t FireAntiMissileSystem(int32_t numMissiles, int32_t& antiMissileShots)
    {
        antiMissileShots = 0;
        return numMissiles;
    }

    /// <summary>Applies a shot; <paramref name="addMultiplayChunk"/> also sends it to the other machines.</summary>
    virtual int32_t HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) { return 0; }
    /// <summary>Sets the movement control inputs.</summary>
    virtual int32_t SetControl(uint32_t controlType, uint32_t controlData, int32_t controlParam) { return 0; }
    /// <summary>Puts out the fire burning on the object.</summary>
    virtual void KillFireObject() {}
    virtual MCTeam* GetTeam() { return nullptr; }
    virtual MCVector3D GetPosition();
    /// <summary>
    /// The point <paramref name="distance"/> meters from the object at <paramref name="angle"/>, pulled back along
    /// the way to the first cell that is passable (or, with bit 2 of <paramref name="flags"/>, impassable), on the
    /// terrain.
    /// </summary>
    virtual MCVector3D RelativePosition(float angle, float distance, uint32_t flags);
    virtual void SetPosition(MCVector3D& newPosition);
    /// <summary>Places a terrain object from its tile offset and block/vertex numbers.</summary>
    virtual void SetTerrainPosition(MCVector2D& position, MCVector2D& numbers) {}
    virtual int IsPotentialContact() { return 0; }
    /// <summary>Registers (nonzero <paramref name="contactType"/>) or removes the object as a sensor contact.</summary>
    virtual void SetPotentialContact(int32_t contactType) {}
    virtual MCPotentialContact* GetPotentialContact() { return nullptr; }
    virtual void UpdateContactStatus(MCTeam* team) {}
    virtual int IncContactCount(int32_t teamId) { return 0; }
    virtual void SetContactTagged(int32_t teamId, int tagged) {}
    virtual int GetContactTagged(int32_t teamId) { return 0; }
    /// <summary>How team <paramref name="teamId"/> sees the object (0: not at all).</summary>
    virtual int32_t GetContactType(int32_t teamId) { return 0; }
    /// <summary>As <see cref="GetContactType(int32_t)"/>, also giving whether the team has it tagged.</summary>
    virtual int32_t GetContactType(int32_t teamId, int& tagged) { return 0; }
    virtual MCVector3D GetVelocity();
    /// <summary>Where the object is on screen (computed by <see cref="OnScreen"/>).</summary>
    virtual MCVector2D GetScreenPos(int32_t whichOne);
    virtual void SetVelocity(MCVector3D& newVelocity) {}
    /// <summary>The object's frame of reference (the world axes here).</summary>
    virtual MCFrameOfRef GetFrame();
    virtual void SetFrame(MCFrameOfRef& newFrame) {}
    /// <summary>Whether a mech can no longer fight (legs or pilot gone).</summary>
    virtual int IsCrippled() { return 0; }
    virtual int IsDisabled() { return Status == 1 || Status == 2; }
    virtual int IsDestroyed() { return Status == 2; }
    virtual float GetDamage() { return 0.0f; }
    virtual void SetDamage(float newDamage) {}
    virtual void SetAlignment(int32_t newAlignment) { Alignment = newAlignment; }
    virtual void SetCommanderId(int32_t commanderId) {}
    virtual int32_t GetCommanderId() { return -1; }
    /// <summary>Writes the object's state to a save file.</summary>
    virtual int32_t Write(MCFile* objFile) { return 0; }
    /// <summary>The distance on the ground from the object to <paramref name="goal"/>, in meters.</summary>
    virtual double DistanceFrom(MCVector3D& goal);
    /// <summary>Whether the map gives the object sight of <paramref name="target"/> (both ignore themselves).</summary>
    virtual int LineOfSight(MCGameObject* target);
    /// <summary>Whether the map gives the object sight of <paramref name="point"/>.</summary>
    virtual int LineOfSight(MCVector3D point);
    /// <summary>Whether the map lets the object fire at <paramref name="target"/>.</summary>
    virtual int LineOfFire(MCGameObject* target);
    /// <summary>The angle from the object's facing to <paramref name="goal"/>, in degrees (-180 to 180).</summary>
    virtual float RelFacingTo(MCVector3D goal, int32_t bodyPart);
    /// <summary>The angle from the object's view (torso or turret) facing to <paramref name="goal"/>.</summary>
    virtual float RelViewFacingTo(MCVector3D goal);
    /// <summary>Opens the object's status window at the given screen rectangle.</summary>
    virtual int32_t OpenStatusWindow(int32_t left, int32_t top, int32_t right, int32_t bottom) { return 0; }
    virtual int32_t CloseStatusWindow() { return 0; }
    virtual int32_t GetMoveState() { return 0; }
    virtual void SetSelected(int32_t newSelected) { Selected = newSelected; }
    virtual void OrderWithdraw() {}
    virtual int IsWithdrawing() { return 0; }
    /// <summary>The type's extent radius.</summary>
    virtual float GetExtentRadius();
    /// <summary>Sets the type's extent radius (for every object of the type).</summary>
    virtual void SetExtentRadius(float newRadius);
    virtual MCMechWarrior* GetPilot() { return nullptr; }
    virtual int IsBuilding() { return 0; }
    virtual MCMechClass GetMechClass() { return MCMechClass::None; }
    /// <summary>Whether the object is riding in a transport.</summary>
    virtual int InTransport() { return 0; }
    virtual int IsCaptureable() { return 0; }
    virtual int IsPrison() { return 0; }
    virtual int GetAwake() { return 1; }
    virtual int GetExists() { return 1; }
    virtual int GetExistsAndAwake() { return 1; }
    virtual void SetUseMe(int useMe) {}
    virtual void SetExists(int exists) {}
    virtual void SetCaptured() {}
    virtual void ClearCaptured() {}
    virtual int IsCaptured() { return 0; }
    virtual void SetTonnage(float newTonnage) {}
    virtual float GetTonnage() { return 0.0f; }
    virtual void SetHeat(float newHeat) {}
    virtual float GetHeat() { return 0.0f; }
    /// <summary>Sets the object that may overlap this one without a collision (while the free time runs).</summary>
    virtual void SetCollisionFreeFrom(MCGameObject* other) {}
    virtual MCGameObject* GetCollisionFreeFrom() { return nullptr; }
    virtual void SetCollisionFreeTime(float time) {}
    virtual float GetCollisionFreeTime() { return 0.0f; }
    /// <summary>Sets the object's record in the object map.</summary>
    virtual void SetObjPosition(MCObjectPosition* newObjPosition) {}
    virtual MCObjectPosition* GetObjPosition() { return nullptr; }
    /// <summary>Adds <paramref name="damageAmount"/> to the object's damage.</summary>
    virtual void DamageObject(float damageAmount) {}
    virtual int32_t GetAlignment() { return Alignment; }
    /// <summary>Sets the damage the object's explosion does.</summary>
    virtual void SetExplDmg(float newDamage) {}
    /// <summary>Sets the radius of the object's explosion.</summary>
    virtual void SetExplRad(float newRadius) {}
    virtual float GetExplDmg() { return 0.0f; }
    /// <summary>Adds <paramref name="numItems"/> of item <paramref name="itemId"/> to the salvage the object leaves.</summary>
    virtual void AddSalvage(uint8_t itemId, uint8_t numItems) {}
    /// <summary>The salvage the object leaves, in the order it was added (none here).</summary>
    virtual std::span<const MCSalvageItem> GetSalvage() { return {}; }
    /// <summary>The turn the object was last on screen.</summary>
    virtual int32_t GetWindowsVisible() { return 0; }
    /// <summary>Whether the player's side can see the object.</summary>
    virtual int IsRevealed() { return 0; }
    /// <summary>
    /// The first mover of the list <paramref name="side"/> picks (1: the clan list, else the Inner Sphere list)
    /// close enough to block the object's capture.
    /// </summary>
    virtual MCGameObject* GetCaptureBlocker(int32_t side);
    /// <summary>Marks the object's 3x3 map cells as not blocking fire, remembering them in objCellArray.</summary>
    virtual void ClearLineOfFire();
    /// <summary>Restores the cells <see cref="ClearLineOfFire"/> changed.</summary>
    virtual void RestoreLineOfFire();
    /// <summary>The object's current combat value.</summary>
    virtual int32_t GetCurCV() { return 0; }
    virtual int32_t GetMaxCV() { return 0; }
    virtual void SetCurCV(int32_t newCV) {}
    virtual int IsMarine() { return 0; }
    /// <summary>The refit points a refit vehicle carries.</summary>
    virtual float GetRefitPoints() { return 0.0f; }
    /// <summary>Spends refit points; zero when there aren't enough.</summary>
    virtual int BurnRefitPoints(float pointsToBurn) { return 0; }
    virtual void IncrementAttackers() {}
    virtual void DecrementAttackers() {}
    /// <summary>How many movers are attacking the object.</summary>
    virtual int32_t GetNumAttackers() { return 0; }

    /// <summary>The object's type.</summary>
    MCObjectType* ObjType = nullptr;
    /// <summary>The object's world position.</summary>
    MCVector3D Position;
    /// <summary>
    /// Nonzero when the object takes part in collisions (<c>CollisionGrid::add</c> skips it otherwise). Movers
    /// start with 1; a destroyed tree building sets 0.
    /// </summary>
    int32_t CollisionsOn = 0;
    /// <summary>Nonzero while the player has the object selected.</summary>
    int32_t Selected = 0;
    /// <summary>0 normal, 1 disabled, 2 destroyed.</summary>
    int32_t Status = 0;
    /// <summary>The side the object is on (the type's alignment to begin with).</summary>
    int32_t Alignment = 0;
};

/// <summary>The 3x3 map cells' "blocks fire" bits <see cref="MCGameObject::ClearLineOfFire"/> saved.</summary>
extern std::array<int32_t, 9> ObjCellArray;
/// <summary>How close an enemy mover must be to block a capture.</summary>
extern float BlockCaptureRange;
