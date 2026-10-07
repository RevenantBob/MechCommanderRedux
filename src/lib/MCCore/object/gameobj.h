#pragma once

#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "object/baseobj.h"

class MCBigGameObject;
class MCFile;
class MCFitIniFile;
class MCGameObject;
class MCMechWarrior;
class MCTeam;
struct MCObjectPosition;
struct MCPotentialContact;

/// <summary>
/// An item of the salvage an object leaves: ABL setsalvage appends them, getsalvage reads them back and
/// <see cref="MCBigGameObject::Destroy"/> walks and frees the list.
/// </summary>
/// <remarks>Has no out-of-line code, so the line tables don't name its file; 8 bytes (ABL allocates them with
/// operator new(8)).</remarks>
class MCSalvageItem
{
public:
    /// <summary>The item's id (setsalvage's second argument).</summary>
    uint8_t ItemId = 0;
    /// <summary>How many (setsalvage's third argument).</summary>
    uint8_t NumItems = 0;
    /// <summary>The next item of the list.</summary>
    MCSalvageItem* Next = nullptr;
};

/// <summary>A mech's weight class by tonnage (<see cref="MCGameObject::GetMechClass"/>); names are the port's.</summary>
enum MCMechClass : int32_t
{
    /// <summary>Not a mech.</summary>
    MECH_CLASS_NONE = 0,
    /// <summary>Under 35 tons.</summary>
    MECH_CLASS_LIGHT = 1,
    /// <summary>35 to under 55 tons.</summary>
    MECH_CLASS_MEDIUM = 2,
    /// <summary>55 to under 75 tons.</summary>
    MECH_CLASS_HEAVY = 3,
    /// <summary>75 tons and more.</summary>
    MECH_CLASS_ASSAULT = 4
};

/// <summary>
/// One shot's effect on its target: who fired, with what, how much damage, where it hits and from which side.
/// </summary>
/// <remarks>Original source: <c>object\gameobj.cpp</c>; 0x14 bytes.</remarks>
struct MCWeaponShotInfo
{
    /// <summary>
    /// Fills the record. In single player the damage is scaled by the difficulty (<c>applyDifficultyWeapon</c>);
    /// in a multiplayer game with packed damage it is rounded to quarter points and the angle to a quadrant.
    /// </summary>
    void Init(MCGameObject* shooter, int32_t weaponMasterId, float shotDamage, int32_t shotHitLocation,
              float shotEntryAngle);
    /// <summary>Sets the damage (rounded to quarter points when multiplayer packs it).</summary>
    void SetDamage(float shotDamage);
    /// <summary>Sets the entry angle (snapped to 0, -90, 90 or 180 when multiplayer packs it).</summary>
    void SetEntryAngle(float shotEntryAngle);

    /// <summary>Who fired.</summary>
    MCGameObject* Attacker = nullptr;
    /// <summary>The weapon's master component id (an index into <c>MasterComponentList</c>).</summary>
    int32_t MasterId = 0;
    /// <summary>Damage points (0 to 255).</summary>
    float Damage = 0;
    /// <summary>The body location hit (-1: none).</summary>
    int32_t HitLocation = 0;
    /// <summary>The angle the shot comes from, relative to the target's facing, in degrees.</summary>
    float EntryAngle = 0;
};

/// <summary>
/// A weapon fired, as sent between multiplayer machines: the target (a mover by roster index, a terrain object by
/// block/vertex/item, a train car or camera drone, or a map cell), the weapon, whether it hit, and the missile
/// counts. <see cref="Pack"/> squeezes it into <see cref="Data"/>.
/// </summary>
/// <remarks>Original source: <c>object\gameobj.cpp</c>, <c>object\gameobj.h</c>; 0x28 bytes.</remarks>
class MCWeaponFireChunk
{
public:
    /// <summary>Clears the chunk (no hit location).</summary>
    void Init();
    /// <summary>
    /// Targets a mover: its multiplayer roster index, the weapon, hit or miss, the entry angle's quadrant, the
    /// missile counts and the hit location.
    /// </summary>
    void BuildMoverTarget(MCBigGameObject* target, int32_t weapon, int hitTarget, float angle, int32_t missiles,
                          int32_t missilesPastAMS, int32_t antiMissileShots, int32_t location);
    /// <summary>Targets a terrain object by its part id (split into block, vertex and item).</summary>
    void BuildTerrainTarget(MCBigGameObject* target, int32_t weapon, int hitTarget, int32_t missiles);
    /// <summary>Targets a train car by its part id (split into train and car).</summary>
    void BuildTrainTarget(MCBigGameObject* target, int32_t weapon, int hitTarget, float angle, int32_t missiles);
    /// <summary>Targets a camera drone (a "train" numbered 0x80).</summary>
    void BuildCameraDroneTarget(MCBigGameObject* target, int32_t weapon, int hitTarget, float angle, int32_t missiles);
    /// <summary>Targets a map location (its cell).</summary>
    void BuildLocationTarget(MCVector3D location, int32_t weapon, int hitTarget, int32_t missiles);
    /// <summary>Packs the fields into <see cref="Data"/>.</summary>
    void Pack();
    /// <summary>Unpacks <see cref="Data"/>; <paramref name="attacker"/> tells whether the weapon fires missiles.</summary>
    void Unpack(MCBigGameObject* attacker);
    /// <summary>Whether every field matches <paramref name="chunk"/> (logs the difference when not).</summary>
    int EqualTo(MCWeaponFireChunk* chunk);

    /// <summary>0 mover, 1 terrain object, 2 train car or camera drone, 3 map location.</summary>
    int8_t TargetType = 0;
    /// <summary>The mover's roster index, or the target's part id.</summary>
    int32_t TargetId = 0;
    /// <summary>Terrain: the block; train: the train number (0x80 for a camera drone).</summary>
    int32_t TargetBlockOrTrainNumber = 0;
    /// <summary>Terrain: the vertex; train: the car number (camera drone: its part id minus 0x802c8).</summary>
    int32_t TargetVertexOrCarNumber = 0;
    /// <summary>Terrain: the object's item number on its vertex (0 to 7).</summary>
    int8_t TargetItemNumber = 0;
    /// <summary>Location: the target cell's row and column.</summary>
    uint16_t TargetCell[2]{};
    /// <summary>The weapon's index among the attacker's weapons.</summary>
    uint8_t WeaponIndex = 0;
    /// <summary>Nonzero when the shot hit.</summary>
    int32_t Hit = 0;
    /// <summary>The entry angle's quadrant: 0 front, 1 rear, 2 left, 3 right.</summary>
    int8_t EntryAngle = 0;
    /// <summary>Missiles fired (0 for a weapon that fires none).</summary>
    int8_t NumMissiles = 0;
    /// <summary>Missiles left after the target's anti-missile system (equal to numMissiles when none).</summary>
    int8_t NumMissilesPastAms = 0;
    /// <summary>Shots the target's anti-missile system fired.</summary>
    int8_t NumAntiMissileShots = 0;
    /// <summary>The body location hit (-1 to 11; -1 after <see cref="Init"/>).</summary>
    int8_t HitLocation = 0;
    /// <summary>The packed chunk.</summary>
    uint32_t Data = 0;
};

/// <summary>
/// A weapon hit, as sent between multiplayer machines: the target (a mover by roster index, a terrain object, a
/// train car or camera drone), the damage, its cause, the hit location and entry angle.
/// </summary>
/// <remarks>Original source: <c>object\gameobj.cpp</c>; 0x24 bytes.</remarks>
class MCWeaponHitChunk
{
public:
    void BuildMoverTarget(MCBigGameObject* target, int32_t hitCause, float hitDamage, int32_t location, float angle,
                          int isRefit);
    void BuildTerrainTarget(MCBigGameObject* target, float hitDamage);
    void BuildTrainTarget(MCBigGameObject* target, float hitDamage, float angle);
    void BuildCameraDroneTarget(MCBigGameObject* target, float hitDamage, float angle);
    /// <summary>Builds the chunk for <paramref name="target"/> from a shot.</summary>
    void Build(MCGameObject* target, MCWeaponShotInfo* shotInfo, int isRefit);
    /// <summary>Packs the fields into <see cref="Data"/> (damage in quarter points).</summary>
    void Pack();
    /// <summary>Unpacks <see cref="Data"/>.</summary>
    void Unpack();
    /// <summary>Whether every field matches <paramref name="chunk"/> (logs the difference when not).</summary>
    int EqualTo(MCWeaponHitChunk* chunk);

    /// <summary>0 mover, 1 terrain object, 2 train car or camera drone.</summary>
    int8_t TargetType = 0;
    /// <summary>The mover's roster index, or the target's part id.</summary>
    int32_t TargetId = 0;
    /// <summary>Terrain: the block; train: the train number (0x80 for a camera drone).</summary>
    int32_t TargetBlockOrTrainNumber = 0;
    /// <summary>Terrain: the vertex; train: the car number.</summary>
    int32_t TargetVertexOrCarNumber = 0;
    /// <summary>Terrain: the object's item number on its vertex.</summary>
    int8_t TargetItemNumber = 0;
    /// <summary>
    /// What caused the hit (-7 to 0): 0 or the shot's master id adjusted by <see cref="Build"/> (-4 for a
    /// component whose form is 10).
    /// </summary>
    int8_t Cause = 0;
    /// <summary>Damage points.</summary>
    float Damage = 0;
    /// <summary>The body location hit (-1 to 11).</summary>
    int8_t HitLocation = 0;
    /// <summary>The entry angle's quadrant: 0 front, 1 rear, 2 left, 3 right.</summary>
    int8_t EntryAngle = 0;
    /// <summary>Nonzero when the "hit" is a refit (repairs rather than damages).</summary>
    int32_t Refit = 0;
    /// <summary>The packed chunk.</summary>
    uint32_t Data = 0;
};

/// <summary>
/// An object that lives in the world: it has a type, a position, a status (normal, disabled, destroyed) and an
/// alignment, and the virtual interface every weapon, sensor, AI and interface routine talks to. Most of the
/// interface does nothing here and is filled in by <see cref="MCBigGameObject"/> and the movers.
/// </summary>
/// <remarks>Original source: <c>object\gameobj.h</c>, <c>object\gameobj.cpp</c>; 0x38 bytes.</remarks>
class MCGameObject : public MCBaseObject
{
public:
    /// <summary>Sets the fields as <see cref="init()"/> does.</summary>
    MCGameObject();

    /// <summary>Takes the type and its alignment.</summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Resets the fields: class GAMEOBJECT, no type, position or status.</summary>
    void Init() override;
    /// <summary>Releases the object's type.</summary>
    void Destroy() override;
    ~MCGameObject() override { Destroy(); }
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
    /// <summary>As <see cref="getContactType(int32_t)"/>, also giving whether the team has it tagged.</summary>
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
    virtual MCMechClass GetMechClass() { return MECH_CLASS_NONE; }
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
    /// <summary>Sets the salvage the object leaves.</summary>
    virtual void SetSalvage(MCSalvageItem* newSalvage) {}
    virtual MCSalvageItem* GetSalvage() { return nullptr; }
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

/// <summary>
/// A game object with the full state the interface uses: tonnage, team, damage, sensor contact, screen position,
/// awake/exists/use-me/captured flags, explosion, salvage, combat value and attackers. Buildings, turrets, gates and
/// movers derive from it.
/// </summary>
/// <remarks>Original source: <c>object\gameobj.h</c>, <c>object\gameobj.cpp</c>; 0x84 bytes.</remarks>
class MCBigGameObject : public MCGameObject
{
public:
    /// <summary>Resets the fields (inline in the original).</summary>
    MCBigGameObject() { Init(); }

    int32_t Init(MCObjectType* objType) override;
    /// <summary>Resets the fields: class BIGGAMEOBJECT, awake and use-me set.</summary>
    void Init() override;
    /// <summary>Frees the salvage list and removes the object from the contact manager and the object map.</summary>
    void Destroy() override;
    ~MCBigGameObject() override { Destroy(); }
    /// <summary>Makes the type's explosion and its destroyed object at the object's position.</summary>
    /// <returns>0xBEADDEAD.</returns>
    int32_t Kill() override;
    int32_t Update() override { return 0; }
    void Render() override {}
    int GetUseMe() override { return (Flags & 4) >> 2; }
    void SetAwake(int awake) override
    {
        Flags &= ~1;

        if (awake)
        {
            Flags |= 1;
        }
    }

    int32_t Init(MCFitIniFile* objFile) override { return 0; }
    /// <summary>Projects the object through the main camera; on screen, remembers the turn.</summary>
    int OnScreen() override;
    MCTeam* GetTeam() override { return Team; }
    int IsPotentialContact() override { return PotentialContact != nullptr; }
    /// <summary>
    /// Adds the object to the potential contact manager with <paramref name="contactType"/> (or moves it there),
    /// or removes it for 0.
    /// </summary>
    void SetPotentialContact(int32_t contactType) override;
    MCPotentialContact* GetPotentialContact() override { return PotentialContact; }
    void UpdateContactStatus(MCTeam* contactTeam) override;
    void SetContactTagged(int32_t teamId, int tagged) override;
    int GetContactTagged(int32_t teamId) override;
    int32_t GetContactType(int32_t teamId) override;
    int32_t GetContactType(int32_t teamId, int& tagged) override;
    MCVector2D GetScreenPos(int32_t whichOne) override;
    float GetDamage() override { return Damage; }
    void SetDamage(float newDamage) override { Damage = newDamage; }
    /// <summary>Sets the alignment and re-registers the sensor contact under it.</summary>
    void SetAlignment(int32_t newAlignment) override;
    void SetCommanderId(int32_t commanderId) override {}
    int32_t GetCommanderId() override { return -1; }
    /// <summary>Writes tonnage, status, damage, captured, explosion radius and damage.</summary>
    int32_t Write(MCFile* objFile) override;
    /// <summary>A building or a tree building.</summary>
    int IsBuilding() override { return ObjectClass == BUILDING || ObjectClass == TREEBUILDING; }
    /// <summary>A mech's weight class from its tonnage.</summary>
    MCMechClass GetMechClass() override;
    int InTransport() override { return 0; }
    int IsCaptureable() override { return 0; }
    int IsPrison() override { return 0; }
    int GetAwake() override { return Flags & 1; }
    int GetExists() override { return (Flags >> 1) & 1; }
    int GetExistsAndAwake() override { return (Flags & 3) == 3; }
    void SetUseMe(int useMe) override
    {
        Flags &= ~4;

        if (useMe)
        {
            Flags |= 4;
        }
    }

    void SetExists(int exists) override
    {
        Flags &= ~2;

        if (exists)
        {
            Flags |= 2;
        }
    }

    void SetCaptured() override { Flags |= 8; }
    void ClearCaptured() override { Flags &= ~8; }
    int IsCaptured() override { return (Flags & 8) >> 3; }
    void SetTonnage(float newTonnage) override { Tonnage = newTonnage; }
    float GetTonnage() override { return Tonnage; }
    void SetCollisionFreeFrom(MCGameObject* other) override { CollisionFreeFrom = other; }
    MCGameObject* GetCollisionFreeFrom() override { return CollisionFreeFrom; }
    void SetCollisionFreeTime(float time) override { CollisionFreeTime = time; }
    float GetCollisionFreeTime() override { return CollisionFreeTime; }
    void SetObjPosition(MCObjectPosition* newObjPosition) override { ObjPosition = newObjPosition; }
    MCObjectPosition* GetObjPosition() override { return ObjPosition; }
    void DamageObject(float damageAmount) override { Damage += damageAmount; }
    void SetExplDmg(float newDamage) override { ExplDamage = newDamage; }
    void SetExplRad(float newRadius) override { ExplRadius = newRadius; }
    float GetExplDmg() override { return ExplDamage; }
    void SetSalvage(MCSalvageItem* newSalvage) override { Salvage = newSalvage; }
    MCSalvageItem* GetSalvage() override { return Salvage; }
    int32_t GetWindowsVisible() override { return WindowsVisible; }
    int32_t GetCurCV() override { return CurCV; }
    int32_t GetMaxCV() override { return MaxCV; }
    void SetCurCV(int32_t newCV) override { CurCV = newCV; }
    void IncrementAttackers() override { NumAttackers++; }
    /// <summary>One attacker fewer (asserts there was one).</summary>
    void DecrementAttackers() override;
    int32_t GetNumAttackers() override { return NumAttackers; }

    virtual int32_t SetTeam(MCTeam* newTeam)
    {
        Team = newTeam;
        return 0;
    }

    /// <summary>How many of team <paramref name="teamId"/>'s sensors see the object.</summary>
    virtual int32_t GetContactCount(int32_t teamId);
    /// <summary>
    /// Fills <paramref name="vitalInfo"/> with what the status displays need (if not null).
    /// </summary>
    /// <returns>The size of the record (0x19 here).</returns>
    virtual int32_t GetVitalInfo(void* vitalInfo);

    /// <summary>Tonnage (weight class of a mech).</summary>
    float Tonnage = 0;
    /// <summary>The object's record in the object map (<c>GameObjectMap</c>), or null.</summary>
    MCObjectPosition* ObjPosition = nullptr;
    /// <summary>The team the object belongs to.</summary>
    MCTeam* Team = nullptr;
    /// <summary>The object's entry in the potential contact manager, or null when it is no sensor contact.</summary>
    MCPotentialContact* PotentialContact = nullptr;
    /// <summary>Damage taken.</summary>
    float Damage = 0;
    /// <summary>The object this one may overlap without colliding, while <see cref="CollisionFreeTime"/> runs.</summary>
    MCGameObject* CollisionFreeFrom = nullptr;
    float CollisionFreeTime = 0;
    /// <summary>The object's screen position, computed by <see cref="OnScreen"/>.</summary>
    MCVector2D ScreenPos;
    /// <summary>Bit 0 awake, bit 1 exists, bit 2 use me, bit 3 captured (5 after init).</summary>
    uint8_t Flags = 0;
    /// <summary>The turn the object was last on screen.</summary>
    int32_t WindowsVisible = 0;
    /// <summary>The radius of the object's explosion.</summary>
    float ExplRadius = 0;
    /// <summary>The damage the object's explosion does.</summary>
    float ExplDamage = 0;
    /// <summary>The salvage the object leaves: a list chained through each item's next pointer.</summary>
    MCSalvageItem* Salvage = nullptr;
    /// <summary>Maximum combat value.</summary>
    int32_t MaxCV = 0;
    /// <summary>Current combat value.</summary>
    int32_t CurCV = 0;
    /// <summary>The sensor blip's animation frame (BattleMech::render).</summary>
    int32_t BlipFrame = 0;
    /// <summary>Seconds since the sensor blip's last frame.</summary>
    float BlipTime = 0;
    /// <summary>How many movers are attacking the object.</summary>
    int32_t NumAttackers = 0;
};

/// <summary>Prints the fields of one or two weapon fire chunks (and the attacker) to the chunk debug message.</summary>
void DebugWeaponFireChunk(MCWeaponFireChunk* chunk1, MCWeaponFireChunk* chunk2, MCGameObject* attacker);
/// <summary>Opens the weapon fire log (does nothing in MCX.EXE).</summary>
void OpenWeaponFireLog();
/// <summary>Logs a weapon fire chunk (does nothing in MCX.EXE).</summary>
void LogWeaponFireChunk(MCWeaponFireChunk* chunk, MCGameObject* attacker, MCGameObject* target);
/// <summary>Prints the fields of one or two weapon hit chunks to the chunk debug message.</summary>
void DebugWeaponHitChunk(MCWeaponHitChunk* chunk1, MCWeaponHitChunk* chunk2);

/// <summary>The 3x3 map cells' "blocks fire" bits <see cref="MCGameObject::ClearLineOfFire"/> saved.</summary>
extern int32_t ObjCellArray[9];
/// <summary>The text the chunk debug routines build (and the crash handler reports).</summary>
extern char ChunkDebugMsg[];
/// <summary>How close an enemy mover must be to block a capture.</summary>
extern float BlockCaptureRange;
