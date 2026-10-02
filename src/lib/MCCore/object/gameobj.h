#pragma once

#include "lib/cvmath.h"
#include "object/baseobj.h"

class BigGameObject;
class File;
class FitIniFile;
class GameObject;
class MechWarrior;
class Team;
struct _ObjectPosition;
struct _PotentialContact;

/// <summary>
/// An item of the salvage an object leaves: ABL setsalvage appends them, getsalvage reads them back and
/// <see cref="BigGameObject::destroy"/> walks and frees the list.
/// </summary>
/// <remarks>Original source: unknown; 8 bytes (ABL allocates them with operator new(8)).</remarks>
class SalvageItem
{
public:
    /// <summary>The item's id (setsalvage's second argument).</summary>
    uint8_t itemId = 0; // +0x00
    /// <summary>How many (setsalvage's third argument).</summary>
    uint8_t numItems = 0; // +0x01
    /// <summary>The next item of the list.</summary>
    SalvageItem* next = nullptr; // +0x04
};

/// <summary>A mech's weight class by tonnage (<see cref="GameObject::getMechClass"/>); names are the port's.</summary>
enum MechClass : int32_t
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
struct _WeaponShotInfo
{
    /// <summary>
    /// Fills the record. In single player the damage is scaled by the difficulty (<c>applyDifficultyWeapon</c>);
    /// in a multiplayer game with packed damage it is rounded to quarter points and the angle to a quadrant.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00661be0</remarks>
    void init(GameObject* shooter, int32_t weaponMasterId, float shotDamage, int32_t shotHitLocation,
              float shotEntryAngle);
    /// <summary>Sets the damage (rounded to quarter points when multiplayer packs it).</summary>
    /// <remarks>MCX.EXE @ 0x00661d80</remarks>
    void setDamage(float shotDamage);
    /// <summary>Sets the entry angle (snapped to 0, -90, 90 or 180 when multiplayer packs it).</summary>
    /// <remarks>MCX.EXE @ 0x00661dd0</remarks>
    void setEntryAngle(float shotEntryAngle);

    /// <summary>Who fired.</summary>
    GameObject* attacker; // +0x00
    /// <summary>The weapon's master component id (an index into <c>MasterComponentList</c>).</summary>
    int32_t masterId; // +0x04
    /// <summary>Damage points (0 to 255).</summary>
    float damage; // +0x08
    /// <summary>The body location hit (-1: none).</summary>
    int32_t hitLocation; // +0x0c
    /// <summary>The angle the shot comes from, relative to the target's facing, in degrees.</summary>
    float entryAngle; // +0x10
};

/// <summary>
/// A weapon fired, as sent between multiplayer machines: the target (a mover by roster index, a terrain object by
/// block/vertex/item, a train car or camera drone, or a map cell), the weapon, whether it hit, and the missile
/// counts. <see cref="pack"/> squeezes it into <see cref="data"/>.
/// </summary>
/// <remarks>Original source: <c>object\gameobj.cpp</c>, <c>object\gameobj.h</c>; 0x28 bytes.</remarks>
class WeaponFireChunk
{
public:
    /// <summary>Clears the chunk (no hit location).</summary>
    /// <remarks>MCX.EXE @ 0x00671960</remarks>
    void init();
    /// <summary>
    /// Targets a mover: its multiplayer roster index, the weapon, hit or miss, the entry angle's quadrant, the
    /// missile counts and the hit location.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00662ca0</remarks>
    void buildMoverTarget(BigGameObject* target, int32_t weapon, int hitTarget, float angle, int32_t missiles,
                          int32_t missilesPastAMS, int32_t antiMissileShots, int32_t location);
    /// <summary>Targets a terrain object by its part id (split into block, vertex and item).</summary>
    /// <remarks>MCX.EXE @ 0x00662e60</remarks>
    void buildTerrainTarget(BigGameObject* target, int32_t weapon, int hitTarget, int32_t missiles);
    /// <summary>Targets a train car by its part id (split into train and car).</summary>
    /// <remarks>MCX.EXE @ 0x00662f00</remarks>
    void buildTrainTarget(BigGameObject* target, int32_t weapon, int hitTarget, float angle, int32_t missiles);
    /// <summary>Targets a camera drone (a "train" numbered 0x80).</summary>
    /// <remarks>MCX.EXE @ 0x00663000</remarks>
    void buildCameraDroneTarget(BigGameObject* target, int32_t weapon, int hitTarget, float angle, int32_t missiles);
    /// <summary>Targets a map location (its cell).</summary>
    /// <remarks>MCX.EXE @ 0x006630e0</remarks>
    void buildLocationTarget(vector_3d location, int32_t weapon, int hitTarget, int32_t missiles);
    /// <summary>Packs the fields into <see cref="data"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00663140 (unnamed in Ghidra; the counterpart of <see cref="unpack"/>)</remarks>
    void pack();
    /// <summary>Unpacks <see cref="data"/>; <paramref name="attacker"/> tells whether the weapon fires missiles.</summary>
    /// <remarks>MCX.EXE @ 0x00663230</remarks>
    void unpack(BigGameObject* attacker);
    /// <summary>Whether every field matches <paramref name="chunk"/> (logs the difference when not).</summary>
    /// <remarks>MCX.EXE @ 0x00663400</remarks>
    int equalTo(WeaponFireChunk* chunk);

    /// <summary>Allocates from <c>systemHeap</c> (or the C heap before it exists).</summary>
    /// <remarks>MCX.EXE @ 0x00661e80</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x00661eb0</remarks>
    static void operator delete(void* ptr);

    /// <summary>0 mover, 1 terrain object, 2 train car or camera drone, 3 map location.</summary>
    int8_t targetType; // +0x00
    /// <summary>The mover's roster index, or the target's part id.</summary>
    int32_t targetId; // +0x04
    /// <summary>Terrain: the block; train: the train number (0x80 for a camera drone).</summary>
    int32_t targetBlockOrTrainNumber; // +0x08
    /// <summary>Terrain: the vertex; train: the car number (camera drone: its part id minus 0x802c8).</summary>
    int32_t targetVertexOrCarNumber; // +0x0c
    /// <summary>Terrain: the object's item number on its vertex (0 to 7).</summary>
    int8_t targetItemNumber; // +0x10
    /// <summary>Location: the target cell's row and column.</summary>
    uint16_t targetCell[2]; // +0x12
    /// <summary>The weapon's index among the attacker's weapons.</summary>
    uint8_t weaponIndex; // +0x16
    /// <summary>Nonzero when the shot hit.</summary>
    int32_t hit; // +0x18
    /// <summary>The entry angle's quadrant: 0 front, 1 rear, 2 left, 3 right.</summary>
    int8_t entryAngle; // +0x1c
    /// <summary>Missiles fired (0 for a weapon that fires none).</summary>
    int8_t numMissiles; // +0x1d
    /// <summary>Missiles left after the target's anti-missile system (equal to numMissiles when none).</summary>
    int8_t numMissilesPastAMS; // +0x1e
    /// <summary>Shots the target's anti-missile system fired.</summary>
    int8_t numAntiMissileShots; // +0x1f
    /// <summary>The body location hit (-1 to 11; -1 after <see cref="init"/>).</summary>
    int8_t hitLocation; // +0x20
    /// <summary>The packed chunk.</summary>
    uint32_t data; // +0x24
};

/// <summary>
/// A weapon hit, as sent between multiplayer machines: the target (a mover by roster index, a terrain object, a
/// train car or camera drone), the damage, its cause, the hit location and entry angle.
/// </summary>
/// <remarks>Original source: <c>object\gameobj.cpp</c>; 0x24 bytes.</remarks>
class WeaponHitChunk
{
public:
    /// <remarks>MCX.EXE @ 0x00663db0</remarks>
    void buildMoverTarget(BigGameObject* target, int32_t hitCause, float hitDamage, int32_t location, float angle,
                          int isRefit);
    /// <remarks>MCX.EXE @ 0x00663e80</remarks>
    void buildTerrainTarget(BigGameObject* target, float hitDamage);
    /// <remarks>MCX.EXE @ 0x00663ef0</remarks>
    void buildTrainTarget(BigGameObject* target, float hitDamage, float angle);
    /// <remarks>MCX.EXE @ 0x00663fd0</remarks>
    void buildCameraDroneTarget(BigGameObject* target, float hitDamage, float angle);
    /// <summary>Builds the chunk for <paramref name="target"/> from a shot.</summary>
    /// <remarks>MCX.EXE @ 0x00664090</remarks>
    void build(GameObject* target, _WeaponShotInfo* shotInfo, int isRefit);
    /// <summary>Packs the fields into <see cref="data"/> (damage in quarter points).</summary>
    /// <remarks>MCX.EXE @ 0x006641e0</remarks>
    void pack();
    /// <summary>Unpacks <see cref="data"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006642a0</remarks>
    void unpack();
    /// <summary>Whether every field matches <paramref name="chunk"/> (logs the difference when not).</summary>
    /// <remarks>MCX.EXE @ 0x00664490 (unnamed in Ghidra)</remarks>
    int equalTo(WeaponHitChunk* chunk);

    /// <summary>Allocates from <c>systemHeap</c> (or the C heap before it exists).</summary>
    /// <remarks>MCX.EXE @ 0x00663d50</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x00663d80</remarks>
    static void operator delete(void* ptr);

    /// <summary>0 mover, 1 terrain object, 2 train car or camera drone.</summary>
    int8_t targetType; // +0x00
    /// <summary>The mover's roster index, or the target's part id.</summary>
    int32_t targetId; // +0x04
    /// <summary>Terrain: the block; train: the train number (0x80 for a camera drone).</summary>
    int32_t targetBlockOrTrainNumber; // +0x08
    /// <summary>Terrain: the vertex; train: the car number.</summary>
    int32_t targetVertexOrCarNumber; // +0x0c
    /// <summary>Terrain: the object's item number on its vertex.</summary>
    int8_t targetItemNumber; // +0x10
    /// <summary>
    /// What caused the hit (-7 to 0): 0 or the shot's master id adjusted by <see cref="build"/> (-4 for a
    /// component whose form is 10).
    /// </summary>
    int8_t cause; // +0x11
    /// <summary>Damage points.</summary>
    float damage; // +0x14
    /// <summary>The body location hit (-1 to 11).</summary>
    int8_t hitLocation; // +0x18
    /// <summary>The entry angle's quadrant: 0 front, 1 rear, 2 left, 3 right.</summary>
    int8_t entryAngle; // +0x19
    /// <summary>Nonzero when the "hit" is a refit (repairs rather than damages).</summary>
    int32_t refit; // +0x1c
    /// <summary>The packed chunk.</summary>
    uint32_t data; // +0x20
};

/// <summary>
/// An object that lives in the world: it has a type, a position, a status (normal, disabled, destroyed) and an
/// alignment, and the virtual interface every weapon, sensor, AI and interface routine talks to. Most of the
/// interface does nothing here and is filled in by <see cref="BigGameObject"/> and the movers.
/// </summary>
/// <remarks>Original source: <c>object\gameobj.h</c>, <c>object\gameobj.cpp</c>; 0x38 bytes.</remarks>
class GameObject : public BaseObject
{
public:
    /// <summary>Sets the fields as <see cref="init()"/> does.</summary>
    /// <remarks>MCX.EXE @ 0x00651b20</remarks>
    GameObject();

    /// <summary>Takes the type and its alignment.</summary>
    /// <remarks>MCX.EXE @ 0x00664560</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Resets the fields: class GAMEOBJECT, no type, position or status.</summary>
    /// <remarks>MCX.EXE @ 0x0064e360</remarks>
    void init() override;
    /// <summary>Releases the object's type.</summary>
    /// <remarks>MCX.EXE @ 0x00665090</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x0064ea60 (vector deleting destructor)</remarks>
    ~GameObject() override { destroy(); }
    /// <remarks>MCX.EXE @ 0x0064e3d0</remarks>
    ObjectType* getObjectType() override { return objType; }
    /// <remarks>MCX.EXE @ 0x0064e460</remarks>
    int32_t kill() override { return 0; }
    /// <remarks>MCX.EXE @ 0x0064e3a0</remarks>
    int32_t update() override { return 0; }
    /// <remarks>MCX.EXE @ 0x0064e3b0 (an empty function Ghidra left unnamed)</remarks>
    void render() override {}
    /// <summary>The object's own position (a plain object has no hot spots).</summary>
    /// <remarks>MCX.EXE @ 0x0064e4a0</remarks>
    vector_3d getPositionFromHS(uint32_t hotSpot) override;
    /// <remarks>MCX.EXE @ 0x0064e840</remarks>
    int getUseMe() override { return 0; }
    /// <summary>The terrain block and vertex under the object's position.</summary>
    /// <remarks>MCX.EXE @ 0x00664580</remarks>
    void getBlockAndVertexNumber(int32_t& blockNumber, int32_t& vertexNumber) override;

    /// <summary>Reads the object's own data from a FIT file (nothing here).</summary>
    /// <remarks>MCX.EXE @ 0x0064e3e0</remarks>
    virtual int32_t init(FitIniFile* objFile) { return 0; }
    /// <summary>Whether the object is on screen (updates its screen position).</summary>
    /// <remarks>MCX.EXE @ 0x0064e3c0</remarks>
    virtual int onScreen() { return 0; }
    /// <summary>
    /// Picks the body location a shot from <paramref name="attacker"/>'s weapon <paramref name="weaponIndex"/>
    /// hits (-1: no locations).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0064e3f0</remarks>
    virtual int32_t calcHitLocation(GameObject* attacker, int32_t weaponIndex, int32_t attackSource, int32_t attackType)
    {
        return -1;
    }

    /// <summary>Spends <paramref name="numShots"/> of the anti-missile system's ammo.</summary>
    /// <remarks>MCX.EXE @ 0x0064e400</remarks>
    virtual void reduceAntiMissileAmmo(int32_t numShots) {}
    /// <summary>
    /// Fires the anti-missile system at <paramref name="numMissiles"/> incoming missiles.
    /// </summary>
    /// <returns>The missiles that get through; <paramref name="antiMissileShots"/> gets the shots fired.</returns>
    /// <remarks>MCX.EXE @ 0x0064e410</remarks>
    virtual int32_t fireAntiMissileSystem(int32_t numMissiles, int32_t& antiMissileShots)
    {
        antiMissileShots = 0;
        return numMissiles;
    }

    /// <summary>Applies a shot; <paramref name="addMultiplayChunk"/> also sends it to the other machines.</summary>
    /// <remarks>MCX.EXE @ 0x0064e430</remarks>
    virtual int32_t handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) { return 0; }
    /// <summary>Sets the movement control inputs.</summary>
    /// <remarks>MCX.EXE @ 0x0064e440</remarks>
    virtual int32_t setControl(uint32_t controlType, uint32_t controlData, int32_t controlParam) { return 0; }
    /// <summary>Puts out the fire burning on the object.</summary>
    /// <remarks>MCX.EXE @ 0x0064e450</remarks>
    virtual void killFireObject() {}
    /// <remarks>MCX.EXE @ 0x0064e470</remarks>
    virtual Team* getTeam() { return nullptr; }
    /// <remarks>MCX.EXE @ 0x0064e480</remarks>
    virtual vector_3d getPosition();
    /// <summary>
    /// The point <paramref name="distance"/> meters from the object at <paramref name="angle"/>, pulled back along
    /// the way to the first cell that is passable (or, with bit 2 of <paramref name="flags"/>, impassable), on the
    /// terrain.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00664a60</remarks>
    virtual vector_3d relativePosition(float angle, float distance, uint32_t flags);
    /// <remarks>MCX.EXE @ 0x0064e4c0</remarks>
    virtual void setPosition(vector_3d& newPosition);
    /// <summary>Places a terrain object from its tile offset and block/vertex numbers.</summary>
    /// <remarks>MCX.EXE @ 0x0064e4e0</remarks>
    virtual void setTerrainPosition(vector_2d& position, vector_2d& numbers) {}
    /// <remarks>MCX.EXE @ 0x0064e4f0</remarks>
    virtual int isPotentialContact() { return 0; }
    /// <summary>Registers (nonzero <paramref name="contactType"/>) or removes the object as a sensor contact.</summary>
    /// <remarks>MCX.EXE @ 0x0064e500</remarks>
    virtual void setPotentialContact(int32_t contactType) {}
    /// <remarks>MCX.EXE @ 0x0064e510</remarks>
    virtual _PotentialContact* getPotentialContact() { return nullptr; }
    /// <remarks>MCX.EXE @ 0x0064e520</remarks>
    virtual void updateContactStatus(Team* team) {}
    /// <remarks>MCX.EXE @ 0x0064e530</remarks>
    virtual int incContactCount(int32_t teamId) { return 0; }
    /// <remarks>MCX.EXE @ 0x0064e540</remarks>
    virtual void setContactTagged(int32_t teamId, int tagged) {}
    /// <remarks>MCX.EXE @ 0x0064e550</remarks>
    virtual int getContactTagged(int32_t teamId) { return 0; }
    /// <summary>How team <paramref name="teamId"/> sees the object (0: not at all).</summary>
    /// <remarks>MCX.EXE @ 0x0064e570</remarks>
    virtual int32_t getContactType(int32_t teamId) { return 0; }
    /// <summary>As <see cref="getContactType(int32_t)"/>, also giving whether the team has it tagged.</summary>
    /// <remarks>MCX.EXE @ 0x0064e560</remarks>
    virtual int32_t getContactType(int32_t teamId, int& tagged) { return 0; }
    /// <remarks>MCX.EXE @ 0x0064e580</remarks>
    virtual vector_3d getVelocity();
    /// <summary>Where the object is on screen (computed by <see cref="onScreen"/>).</summary>
    /// <remarks>MCX.EXE @ 0x0064e5a0</remarks>
    virtual vector_2d getScreenPos(int32_t whichOne);
    /// <remarks>MCX.EXE @ 0x0064e5d0</remarks>
    virtual void setVelocity(vector_3d& newVelocity) {}
    /// <summary>The object's frame of reference (the world axes here).</summary>
    /// <remarks>MCX.EXE @ 0x0064e5e0</remarks>
    virtual frame_of_ref getFrame();
    /// <remarks>MCX.EXE @ 0x0064e670</remarks>
    virtual void setFrame(frame_of_ref& newFrame) {}
    /// <summary>Whether a mech can no longer fight (legs or pilot gone).</summary>
    /// <remarks>MCX.EXE @ 0x0064e680 (unnamed in Ghidra; <c>BattleMech::isCrippled</c> overrides the slot)</remarks>
    virtual int isCrippled() { return 0; }
    /// <remarks>MCX.EXE @ 0x0064e690</remarks>
    virtual int isDisabled() { return status == 1 || status == 2; }
    /// <remarks>MCX.EXE @ 0x0064e6b0</remarks>
    virtual int isDestroyed() { return status == 2; }
    /// <remarks>MCX.EXE @ 0x0064e6c0</remarks>
    virtual float getDamage() { return 0.0f; }
    /// <remarks>MCX.EXE @ 0x0064e6d0</remarks>
    virtual void setDamage(float newDamage) {}
    /// <remarks>MCX.EXE @ 0x0064e6e0 (unnamed in Ghidra; the overrides are named setAlignment)</remarks>
    virtual void setAlignment(int32_t newAlignment) { alignment = newAlignment; }
    /// <remarks>MCX.EXE @ 0x0064e6f0</remarks>
    virtual void setCommanderId(int32_t commanderId) {}
    /// <remarks>MCX.EXE @ 0x0064e700</remarks>
    virtual int32_t getCommanderId() { return -1; }
    /// <summary>Writes the object's state to a save file.</summary>
    /// <remarks>MCX.EXE @ 0x0064e710</remarks>
    virtual int32_t write(File* objFile) { return 0; }
    /// <summary>The distance on the ground from the object to <paramref name="goal"/>, in meters.</summary>
    /// <remarks>MCX.EXE @ 0x00664e70</remarks>
    virtual double distanceFrom(vector_3d& goal);
    /// <summary>Whether the map gives the object sight of <paramref name="target"/> (both ignore themselves).</summary>
    /// <remarks>MCX.EXE @ 0x00664f10</remarks>
    virtual int lineOfSight(GameObject* target);
    /// <summary>Whether the map gives the object sight of <paramref name="point"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00664ea0</remarks>
    virtual int lineOfSight(vector_3d point);
    /// <summary>Whether the map lets the object fire at <paramref name="target"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00664fd0</remarks>
    virtual int lineOfFire(GameObject* target);
    /// <summary>The angle from the object's facing to <paramref name="goal"/>, in degrees (-180 to 180).</summary>
    /// <remarks>MCX.EXE @ 0x006647d0</remarks>
    virtual float relFacingTo(vector_3d goal, int32_t bodyPart);
    /// <summary>The angle from the object's view (torso or turret) facing to <paramref name="goal"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0064e720</remarks>
    virtual float relViewFacingTo(vector_3d goal);
    /// <summary>Opens the object's status window at the given screen rectangle.</summary>
    /// <remarks>MCX.EXE @ 0x0064e750</remarks>
    virtual int32_t openStatusWindow(int32_t left, int32_t top, int32_t right, int32_t bottom) { return 0; }
    /// <remarks>MCX.EXE @ 0x0064e760</remarks>
    virtual int32_t closeStatusWindow() { return 0; }
    /// <remarks>MCX.EXE @ 0x0064e770</remarks>
    virtual int32_t getMoveState() { return 0; }
    /// <remarks>MCX.EXE @ 0x0064e780</remarks>
    virtual void setSelected(int32_t newSelected) { selected = newSelected; }
    /// <remarks>MCX.EXE @ 0x0064e790</remarks>
    virtual void orderWithdraw() {}
    /// <remarks>MCX.EXE @ 0x0064e7a0</remarks>
    virtual int isWithdrawing() { return 0; }
    /// <summary>The type's extent radius.</summary>
    /// <remarks>MCX.EXE @ 0x006654c0</remarks>
    virtual float getExtentRadius();
    /// <summary>Sets the type's extent radius (for every object of the type).</summary>
    /// <remarks>MCX.EXE @ 0x006654d0</remarks>
    virtual void setExtentRadius(float newRadius);
    /// <remarks>MCX.EXE @ 0x0064e7b0</remarks>
    virtual MechWarrior* getPilot() { return nullptr; }
    /// <remarks>MCX.EXE @ 0x0064e7c0</remarks>
    virtual int isBuilding() { return 0; }
    /// <remarks>MCX.EXE @ 0x0064e7d0</remarks>
    virtual MechClass getMechClass() { return MECH_CLASS_NONE; }
    /// <summary>Whether the object is riding in a transport.</summary>
    /// <remarks>MCX.EXE @ 0x0064e7e0</remarks>
    virtual int inTransport() { return 0; }
    /// <remarks>MCX.EXE @ 0x0064e7f0</remarks>
    virtual int isCaptureable() { return 0; }
    /// <remarks>MCX.EXE @ 0x0064e800</remarks>
    virtual int isPrison() { return 0; }
    /// <remarks>MCX.EXE @ 0x0064e810</remarks>
    virtual int getAwake() { return 1; }
    /// <remarks>MCX.EXE @ 0x0064e820</remarks>
    virtual int getExists() { return 1; }
    /// <remarks>MCX.EXE @ 0x0064e830</remarks>
    virtual int getExistsAndAwake() { return 1; }
    /// <remarks>MCX.EXE @ 0x0064e850</remarks>
    virtual void setUseMe(int useMe) {}
    /// <remarks>MCX.EXE @ 0x0064e860</remarks>
    virtual void setExists(int exists) {}
    /// <remarks>MCX.EXE @ 0x0064e870</remarks>
    virtual void setCaptured() {}
    /// <remarks>MCX.EXE @ 0x0064e880</remarks>
    virtual void clearCaptured() {}
    /// <remarks>MCX.EXE @ 0x0064e890</remarks>
    virtual int isCaptured() { return 0; }
    /// <remarks>MCX.EXE @ 0x0064e8a0</remarks>
    virtual void setTonnage(float newTonnage) {}
    /// <remarks>MCX.EXE @ 0x0064e8b0</remarks>
    virtual float getTonnage() { return 0.0f; }
    /// <remarks>MCX.EXE @ 0x0064e8c0</remarks>
    virtual void setHeat(float newHeat) {}
    /// <remarks>MCX.EXE @ 0x0064e8d0</remarks>
    virtual float getHeat() { return 0.0f; }
    /// <summary>Sets the object that may overlap this one without a collision (while the free time runs).</summary>
    /// <remarks>MCX.EXE @ 0x0064e8e0</remarks>
    virtual void setCollisionFreeFrom(GameObject* other) {}
    /// <remarks>MCX.EXE @ 0x0064e8f0</remarks>
    virtual GameObject* getCollisionFreeFrom() { return nullptr; }
    /// <remarks>MCX.EXE @ 0x0064e900</remarks>
    virtual void setCollisionFreeTime(float time) {}
    /// <remarks>MCX.EXE @ 0x0064e910</remarks>
    virtual float getCollisionFreeTime() { return 0.0f; }
    /// <summary>Sets the object's record in the object map.</summary>
    /// <remarks>MCX.EXE @ 0x0064e920</remarks>
    virtual void setObjPosition(_ObjectPosition* newObjPosition) {}
    /// <remarks>MCX.EXE @ 0x0064e930</remarks>
    virtual _ObjectPosition* getObjPosition() { return nullptr; }
    /// <summary>Adds <paramref name="damageAmount"/> to the object's damage.</summary>
    /// <remarks>MCX.EXE @ 0x0064e940</remarks>
    virtual void damageObject(float damageAmount) {}
    /// <remarks>MCX.EXE @ 0x0064e950</remarks>
    virtual int32_t getAlignment() { return alignment; }
    /// <summary>Sets the damage the object's explosion does.</summary>
    /// <remarks>MCX.EXE @ 0x0064e960</remarks>
    virtual void setExplDmg(float newDamage) {}
    /// <summary>Sets the radius of the object's explosion.</summary>
    /// <remarks>MCX.EXE @ 0x0064e970</remarks>
    virtual void setExplRad(float newRadius) {}
    /// <remarks>MCX.EXE @ 0x0064e980</remarks>
    virtual float getExplDmg() { return 0.0f; }
    /// <summary>Sets the salvage the object leaves.</summary>
    /// <remarks>MCX.EXE @ 0x0064e990</remarks>
    virtual void setSalvage(SalvageItem* newSalvage) {}
    /// <remarks>MCX.EXE @ 0x0064e9a0</remarks>
    virtual SalvageItem* getSalvage() { return nullptr; }
    /// <summary>The turn the object was last on screen.</summary>
    /// <remarks>MCX.EXE @ 0x0064e9b0</remarks>
    virtual int32_t getWindowsVisible() { return 0; }
    /// <summary>Whether the player's side can see the object.</summary>
    /// <remarks>MCX.EXE @ 0x0064e9c0</remarks>
    virtual int isRevealed() { return 0; }
    /// <summary>
    /// The first mover of the list <paramref name="side"/> picks (1: the clan list, else the Inner Sphere list)
    /// close enough to block the object's capture.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00665550</remarks>
    virtual GameObject* getCaptureBlocker(int32_t side);
    /// <summary>Marks the object's 3x3 map cells as not blocking fire, remembering them in objCellArray.</summary>
    /// <remarks>MCX.EXE @ 0x006656c0</remarks>
    virtual void clearLineOfFire();
    /// <summary>Restores the cells <see cref="clearLineOfFire"/> changed.</summary>
    /// <remarks>MCX.EXE @ 0x00665760</remarks>
    virtual void restoreLineOfFire();
    /// <summary>The object's current combat value.</summary>
    /// <remarks>MCX.EXE @ 0x0064e9d0</remarks>
    virtual int32_t getCurCV() { return 0; }
    /// <remarks>MCX.EXE @ 0x0064e9e0</remarks>
    virtual int32_t getMaxCV() { return 0; }
    /// <remarks>MCX.EXE @ 0x0064e9f0</remarks>
    virtual void setCurCV(int32_t newCV) {}
    /// <remarks>MCX.EXE @ 0x0064ea00</remarks>
    virtual int isMarine() { return 0; }
    /// <summary>The refit points a refit vehicle carries.</summary>
    /// <remarks>MCX.EXE @ 0x0064ea10</remarks>
    virtual float getRefitPoints() { return 0.0f; }
    /// <summary>Spends refit points; zero when there aren't enough.</summary>
    /// <remarks>MCX.EXE @ 0x0064ea20</remarks>
    virtual int burnRefitPoints(float pointsToBurn) { return 0; }
    /// <remarks>MCX.EXE @ 0x0064ea30</remarks>
    virtual void incrementAttackers() {}
    /// <remarks>MCX.EXE @ 0x0064ea40</remarks>
    virtual void decrementAttackers() {}
    /// <summary>How many movers are attacking the object.</summary>
    /// <remarks>MCX.EXE @ 0x0064ea50</remarks>
    virtual int32_t getNumAttackers() { return 0; }

    /// <summary>The object's type.</summary>
    ObjectType* objType; // +0x14
    /// <summary>The object's world position.</summary>
    vector_3d position; // +0x18
    /// <summary>
    /// Nonzero when the object takes part in collisions (<c>CollisionGrid::add</c> skips it otherwise). Movers
    /// start with 1; a destroyed tree building sets 0.
    /// </summary>
    int32_t collisionsOn; // +0x24
    /// <summary>Nonzero while the player has the object selected.</summary>
    int32_t selected; // +0x28
    /// <summary>
    /// Cleared by <see cref="init()"/>; set and cleared by object events 0x1e / 0x1f. A bullet skips drawing and
    /// drops its hit effect when nonzero.
    /// </summary>
    int32_t unknown2C; // +0x2c
    /// <summary>0 normal, 1 disabled, 2 destroyed.</summary>
    int32_t status; // +0x30
    /// <summary>The side the object is on (the type's alignment to begin with).</summary>
    int32_t alignment; // +0x34
};

/// <summary>
/// A game object with the full state the interface uses: tonnage, team, damage, sensor contact, screen position,
/// awake/exists/use-me/captured flags, explosion, salvage, combat value and attackers. Buildings, turrets, gates and
/// movers derive from it.
/// </summary>
/// <remarks>Original source: <c>object\gameobj.h</c>, <c>object\gameobj.cpp</c>; 0x84 bytes.</remarks>
class BigGameObject : public GameObject
{
public:
    /// <summary>Resets the fields (inline in the original).</summary>
    BigGameObject() { init(); }

    /// <remarks>MCX.EXE @ 0x00664670</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Resets the fields: class BIGGAMEOBJECT, awake and use-me set.</summary>
    /// <remarks>MCX.EXE @ 0x0064eaa0</remarks>
    void init() override;
    /// <summary>Frees the salvage list and removes the object from the contact manager and the object map.</summary>
    /// <remarks>MCX.EXE @ 0x006650a0</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x0064ee70 (vector deleting destructor)</remarks>
    ~BigGameObject() override { destroy(); }
    /// <summary>Makes the type's explosion and its destroyed object at the object's position.</summary>
    /// <returns>0xBEADDEAD.</returns>
    /// <remarks>MCX.EXE @ 0x00664690 (unnamed in Ghidra)</remarks>
    int32_t kill() override;
    /// <remarks>MCX.EXE @ 0x0064eb10</remarks>
    int32_t update() override { return 0; }
    /// <remarks>MCX.EXE @ 0x0064eb20</remarks>
    void render() override {}
    /// <remarks>MCX.EXE @ 0x0064edc0</remarks>
    int getUseMe() override { return (flags & 4) >> 2; }
    /// <remarks>MCX.EXE @ 0x0064ed50</remarks>
    void setAwake(int awake) override
    {
        flags &= ~1;

        if (awake)
        {
            flags |= 1;
        }
    }

    /// <remarks>MCX.EXE @ 0x0064eb50</remarks>
    int32_t init(FitIniFile* objFile) override { return 0; }
    /// <summary>Projects the object through the main camera; on screen, remembers the turn.</summary>
    /// <remarks>MCX.EXE @ 0x00665380</remarks>
    int onScreen() override;
    /// <remarks>MCX.EXE @ 0x0064ec10</remarks>
    Team* getTeam() override { return team; }
    /// <remarks>MCX.EXE @ 0x0064ebf0</remarks>
    int isPotentialContact() override { return potentialContact != nullptr; }
    /// <summary>
    /// Adds the object to the potential contact manager with <paramref name="contactType"/> (or moves it there),
    /// or removes it for 0.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00665190</remarks>
    void setPotentialContact(int32_t contactType) override;
    /// <remarks>MCX.EXE @ 0x0064ec00</remarks>
    _PotentialContact* getPotentialContact() override { return potentialContact; }
    /// <remarks>MCX.EXE @ 0x00665200</remarks>
    void updateContactStatus(Team* contactTeam) override;
    /// <remarks>MCX.EXE @ 0x00665260</remarks>
    void setContactTagged(int32_t teamId, int tagged) override;
    /// <remarks>MCX.EXE @ 0x006652b0</remarks>
    int getContactTagged(int32_t teamId) override;
    /// <remarks>MCX.EXE @ 0x00665340</remarks>
    int32_t getContactType(int32_t teamId) override;
    /// <remarks>MCX.EXE @ 0x006652f0</remarks>
    int32_t getContactType(int32_t teamId, int& tagged) override;
    /// <remarks>MCX.EXE @ 0x0064eb30</remarks>
    vector_2d getScreenPos(int32_t whichOne) override;
    /// <remarks>MCX.EXE @ 0x0064ec50</remarks>
    float getDamage() override { return damage; }
    /// <remarks>MCX.EXE @ 0x0064ec60</remarks>
    void setDamage(float newDamage) override { damage = newDamage; }
    /// <summary>Sets the alignment and re-registers the sensor contact under it.</summary>
    /// <remarks>MCX.EXE @ 0x00665160</remarks>
    void setAlignment(int32_t newAlignment) override;
    /// <remarks>MCX.EXE @ 0x0064ed20</remarks>
    void setCommanderId(int32_t commanderId) override {}
    /// <remarks>MCX.EXE @ 0x0064ed30</remarks>
    int32_t getCommanderId() override { return -1; }
    /// <summary>Writes tonnage, status, damage, captured, explosion radius and damage.</summary>
    /// <remarks>MCX.EXE @ 0x006650f0</remarks>
    int32_t write(File* objFile) override;
    /// <summary>A building or a tree building.</summary>
    /// <remarks>MCX.EXE @ 0x0064ece0 (unnamed in Ghidra)</remarks>
    int isBuilding() override { return objectClass == BUILDING || objectClass == TREEBUILDING; }
    /// <summary>A mech's weight class from its tonnage.</summary>
    /// <remarks>MCX.EXE @ 0x006654e0</remarks>
    MechClass getMechClass() override;
    /// <remarks>MCX.EXE @ 0x0064ed00</remarks>
    int inTransport() override { return 0; }
    /// <remarks>MCX.EXE @ 0x0064ed10</remarks>
    int isCaptureable() override { return 0; }
    /// <remarks>MCX.EXE @ 0x0064ed40</remarks>
    int isPrison() override { return 0; }
    /// <remarks>MCX.EXE @ 0x0064ed70</remarks>
    int getAwake() override { return flags & 1; }
    /// <remarks>MCX.EXE @ 0x0064eda0</remarks>
    int getExists() override { return (flags >> 1) & 1; }
    /// <remarks>MCX.EXE @ 0x0064edb0</remarks>
    int getExistsAndAwake() override { return (flags & 3) == 3; }
    /// <remarks>MCX.EXE @ 0x0064edd0</remarks>
    void setUseMe(int useMe) override
    {
        flags &= ~4;

        if (useMe)
        {
            flags |= 4;
        }
    }

    /// <remarks>MCX.EXE @ 0x0064ed80</remarks>
    void setExists(int exists) override
    {
        flags &= ~2;

        if (exists)
        {
            flags |= 2;
        }
    }

    /// <remarks>MCX.EXE @ 0x0064ecb0</remarks>
    void setCaptured() override { flags |= 8; }
    /// <remarks>MCX.EXE @ 0x0064ecc0</remarks>
    void clearCaptured() override { flags &= ~8; }
    /// <remarks>MCX.EXE @ 0x0064ecd0</remarks>
    int isCaptured() override { return (flags & 8) >> 3; }
    /// <remarks>MCX.EXE @ 0x0064eb70</remarks>
    void setTonnage(float newTonnage) override { tonnage = newTonnage; }
    /// <remarks>MCX.EXE @ 0x0064eb80</remarks>
    float getTonnage() override { return tonnage; }
    /// <remarks>MCX.EXE @ 0x0064eb90</remarks>
    void setCollisionFreeFrom(GameObject* other) override { collisionFreeFrom = other; }
    /// <remarks>MCX.EXE @ 0x0064eba0</remarks>
    GameObject* getCollisionFreeFrom() override { return collisionFreeFrom; }
    /// <remarks>MCX.EXE @ 0x0064ebb0</remarks>
    void setCollisionFreeTime(float time) override { collisionFreeTime = time; }
    /// <remarks>MCX.EXE @ 0x0064ebc0</remarks>
    float getCollisionFreeTime() override { return collisionFreeTime; }
    /// <remarks>MCX.EXE @ 0x0064ebd0</remarks>
    void setObjPosition(_ObjectPosition* newObjPosition) override { objPosition = newObjPosition; }
    /// <remarks>MCX.EXE @ 0x0064ebe0</remarks>
    _ObjectPosition* getObjPosition() override { return objPosition; }
    /// <remarks>MCX.EXE @ 0x0064ec70 (unnamed in Ghidra)</remarks>
    void damageObject(float damageAmount) override { damage += damageAmount; }
    /// <remarks>MCX.EXE @ 0x0064ec80</remarks>
    void setExplDmg(float newDamage) override { explDamage = newDamage; }
    /// <remarks>MCX.EXE @ 0x0064ec90</remarks>
    void setExplRad(float newRadius) override { explRadius = newRadius; }
    /// <remarks>MCX.EXE @ 0x0064eca0</remarks>
    float getExplDmg() override { return explDamage; }
    /// <remarks>MCX.EXE @ 0x0064ec30</remarks>
    void setSalvage(SalvageItem* newSalvage) override { salvage = newSalvage; }
    /// <remarks>MCX.EXE @ 0x0064ec40</remarks>
    SalvageItem* getSalvage() override { return salvage; }
    /// <remarks>MCX.EXE @ 0x0064eb60 (unnamed in Ghidra)</remarks>
    int32_t getWindowsVisible() override { return windowsVisible; }
    /// <remarks>MCX.EXE @ 0x0064edf0</remarks>
    int32_t getCurCV() override { return curCV; }
    /// <remarks>MCX.EXE @ 0x0064ee00</remarks>
    int32_t getMaxCV() override { return maxCV; }
    /// <remarks>MCX.EXE @ 0x0064ee10</remarks>
    void setCurCV(int32_t newCV) override { curCV = newCV; }
    /// <remarks>MCX.EXE @ 0x0064ee20</remarks>
    void incrementAttackers() override { numAttackers++; }
    /// <summary>One attacker fewer (asserts there was one).</summary>
    /// <remarks>MCX.EXE @ 0x0064ee30</remarks>
    void decrementAttackers() override;
    /// <remarks>MCX.EXE @ 0x0064ee60</remarks>
    int32_t getNumAttackers() override { return numAttackers; }

    /// <remarks>MCX.EXE @ 0x0064ec20</remarks>
    virtual int32_t setTeam(Team* newTeam)
    {
        team = newTeam;
        return 0;
    }

    /// <summary>How many of team <paramref name="teamId"/>'s sensors see the object.</summary>
    /// <remarks>MCX.EXE @ 0x00665220</remarks>
    virtual int32_t getContactCount(int32_t teamId);
    /// <summary>
    /// Fills <paramref name="vitalInfo"/> with what the status displays need (if not null).
    /// </summary>
    /// <returns>The size of the record (0x19 here).</returns>
    /// <remarks>MCX.EXE @ 0x00665530</remarks>
    virtual int32_t getVitalInfo(void* vitalInfo);

    /// <summary>Tonnage (weight class of a mech).</summary>
    float tonnage; // +0x38
    /// <summary>The object's record in the object map (<c>GameObjectMap</c>), or null.</summary>
    _ObjectPosition* objPosition; // +0x3c
    /// <summary>The team the object belongs to.</summary>
    Team* team; // +0x40
    /// <summary>The object's entry in the potential contact manager, or null when it is no sensor contact.</summary>
    _PotentialContact* potentialContact; // +0x44
    /// <summary>Damage taken.</summary>
    float damage; // +0x48
    /// <summary>The object this one may overlap without colliding, while <see cref="collisionFreeTime"/> runs.</summary>
    GameObject* collisionFreeFrom; // +0x4c
    float collisionFreeTime;       // +0x50
    /// <summary>The object's screen position, computed by <see cref="onScreen"/>.</summary>
    vector_2d screenPos; // +0x54
    /// <summary>Bit 0 awake, bit 1 exists, bit 2 use me, bit 3 captured (5 after init).</summary>
    uint8_t flags; // +0x5c
    /// <summary>The turn the object was last on screen.</summary>
    int32_t windowsVisible; // +0x60
    /// <summary>The radius of the object's explosion.</summary>
    float explRadius; // +0x64
    /// <summary>The damage the object's explosion does.</summary>
    float explDamage; // +0x68
    /// <summary>The salvage the object leaves: a list chained through each item's next pointer.</summary>
    SalvageItem* salvage; // +0x6c
    /// <summary>Maximum combat value.</summary>
    int32_t maxCV; // +0x70
    /// <summary>Current combat value.</summary>
    int32_t curCV; // +0x74
    /// <summary>The sensor blip's animation frame (BattleMech::render).</summary>
    int32_t blipFrame; // +0x78
    /// <summary>Seconds since the sensor blip's last frame.</summary>
    float blipTime; // +0x7c
    /// <summary>How many movers are attacking the object.</summary>
    int32_t numAttackers; // +0x80
};

/// <summary>Prints the fields of one or two weapon fire chunks (and the attacker) to the chunk debug message.</summary>
/// <remarks>MCX.EXE @ 0x00661ee0</remarks>
void DebugWeaponFireChunk(WeaponFireChunk* chunk1, WeaponFireChunk* chunk2, GameObject* attacker);
/// <summary>Opens the weapon fire log (does nothing in MCX.EXE).</summary>
/// <remarks>MCX.EXE @ 0x00662c80</remarks>
void OpenWeaponFireLog();
/// <summary>Logs a weapon fire chunk (does nothing in MCX.EXE).</summary>
/// <remarks>MCX.EXE @ 0x00662c90</remarks>
void LogWeaponFireChunk(WeaponFireChunk* chunk, GameObject* attacker, GameObject* target);
/// <summary>Prints the fields of one or two weapon hit chunks to the chunk debug message.</summary>
/// <remarks>MCX.EXE @ 0x00663550</remarks>
void DebugWeaponHitChunk(WeaponHitChunk* chunk1, WeaponHitChunk* chunk2);

/// <summary>The 3x3 map cells' "blocks fire" bits <see cref="GameObject::clearLineOfFire"/> saved.</summary>
extern int32_t objCellArray[9];
/// <summary>The text the chunk debug routines build (and the crash handler reports).</summary>
extern char ChunkDebugMsg[];
/// <summary>How close an enemy mover must be to block a capture.</summary>
extern float BlockCaptureRange;
