#pragma once

#include "object/mover.h"
#include "object/objtype.h"

class DynamicsType;
class File;
class FitIniFile;

/// <summary>"Elemental.Collision" "DamageOnImpact".</summary>
extern float elmDamageOnImpact;
/// <summary>Meters from its last target within which an elemental paths without jumping (75).</summary>
/// <remarks>MCX.EXE @ 0x0078f100</remarks>
extern float ElementalTargetNoJumpDistance;
/// <summary>Whether the old (pre-Terrain::projectTerrain) screen projection is used.</summary>
/// <remarks>MCX.EXE @ 0x0080941c</remarks>
extern int useOldProject;

/// <summary>Reads the "Elemental.Collision" and "Elemental.Combat" blocks of the game system file.</summary>
/// <returns>0, or the FitIniFile error of the collision block.</returns>
/// <remarks>MCX.EXE @ 0x0065a8b0</remarks>
int32_t loadElementalGameSystem(FitIniFile* sysFile);

/// <summary>An elemental type: armoured infantry (jumping elementals) or marines (who can't jump).</summary>
/// <remarks>Original source: <c>object\elemntl.cpp</c>, <c>object\elemntl.h</c>; 0x50 bytes.</remarks>
class ElementalType : public ObjectType
{
public:
    /// <summary>Runs <see cref="init()"/>.</summary>
    ElementalType() { init(); }
    /// <remarks>MCX.EXE @ 0x00690a00 (vector deleting destructor)</remarks>
    ~ElementalType() override { destroy(); }

    /// <summary>Clears the fields; canJump 1.</summary>
    /// <remarks>MCX.EXE @ 0x006909e0 (inline in <c>object\elemntl.h</c>)</remarks>
    void init();
    /// <summary>
    /// Reads the elemental file ("ElementalType"): "General" (id, can jump, alignment, name, max health),
    /// "Dynamics" (type 3), then the common type data.
    /// </summary>
    /// <returns>0, -1 for the wrong file type, -0x5fffd for the wrong dynamics type, -0x5fffe out of memory, or the
    /// FitIniFile error.</returns>
    /// <remarks>MCX.EXE @ 0x0065a980</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <summary>Frees the name and the dynamics type.</summary>
    /// <remarks>MCX.EXE @ 0x0065a920</remarks>
    void destroy() override;
    /// <summary>Makes an <see cref="Elemental"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x0065b4b0</remarks>
    BaseObject* createInstance() override;
    /// <summary>
    /// Knocks the elemental aside: an enemy mech or vehicle ramming it (a marine: any), a building (with damage by its
    /// tonnage), a tree, a train car.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0065ac00</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <summary>Kills the elemental: disables its sensor, alarms its pilot, takes it off the interface and sets the
    /// friendly/enemy destroyed flag.</summary>
    /// <remarks>MCX.EXE @ 0x0065b3b0</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>"ID".</summary>
    uint32_t elementalId = 0; // +0x30
    /// <summary>"Name".</summary>
    std::string name; // +0x34
    /// <summary>"Type", mapped 0 -> 1, 1 -> 0xff: the alignment (compared with the home team's id).</summary>
    uint8_t alignment = 0; // +0x38
    /// <summary>"MaxHealth".</summary>
    uint8_t maxHealth = 0; // +0x39
    /// <summary>Zeroed by init; never read.</summary>
    int32_t unknown3C = 0; // +0x3c
    /// <summary>Not accessed.</summary>
    int32_t unknown40 = 0; // +0x40
    /// <summary>The dynamics type (an ElementalDynamicsType).</summary>
    DynamicsType* dynamicsType = nullptr; // +0x44
    /// <summary>"CanJump" (1 when missing): elementals jump; marines don't.</summary>
    int32_t canJump = 1; // +0x48
    /// <summary>Zeroed by init; never read.</summary>
    int32_t unknown4C = 0; // +0x4c
};

/// <summary>
/// An elemental squad or a marine: infantry that walks, jumps (elementals only), rides in carriers, and is removed
/// from the interface when its health runs out.
/// </summary>
/// <remarks>Original source: <c>object\elemntl.cpp</c>, <c>object\elemntl.h</c>; 0x8d8 bytes.</remarks>
class Elemental : public Mover
{
public:
    /// <summary>Runs <see cref="init()"/>.</summary>
    Elemental() { init(); }
    /// <remarks>MCX.EXE @ 0x0065bc40 (vector deleting destructor)</remarks>
    ~Elemental() override { destroy(); }

    /// <summary>Class ELEMENTAL; health 11 of 11; no jump; not in transport.</summary>
    /// <remarks>MCX.EXE @ 0x0065bcb0</remarks>
    void init() override;
    /// <summary>Copies the type's data, makes the dynamics and the ElementalActor appearance.</summary>
    /// <remarks>MCX.EXE @ 0x0065bd10</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Does nothing.</summary>
    /// <remarks>MCX.EXE @ 0x0065c9b0</remarks>
    void destroy() override;
    /// <summary>
    /// Moves it (the actor in a jump, the dynamics otherwise), marks what it sees; a dead one blows up after its timer,
    /// a marine off the screen is removed.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0065dbb0</remarks>
    int32_t update() override;
    /// <summary>
    /// Draws it when seen (a sensor blip when only sensed), and the move path in the terrain-grid debug view.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0065e070</remarks>
    void render() override;
    /// <summary>The elemental's position.</summary>
    /// <remarks>MCX.EXE @ 0x0065da20</remarks>
    vector_3d getPositionFromHS(uint32_t hotSpot) override;
    /// <summary>
    /// Reads the profile ("ElementalProfile"): name, tonnage, health, icon, engine, jump range and the inventory.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0065c0b0</remarks>
    int32_t init(FitIniFile* elementalFile) override;
    /// <summary>Projects it to the screen; whether its appearance is on it.</summary>
    /// <remarks>MCX.EXE @ 0x0065da40</remarks>
    int onScreen() override;
    /// <summary>Always 0 (a squad has one location).</summary>
    /// <remarks>MCX.EXE @ 0x0065e5e0</remarks>
    int32_t calcHitLocation(GameObject* attacker, int32_t weaponIndex, int32_t attackSource,
                            int32_t attackType) override;
    /// <summary>Takes the damage off the health; at 0 the elemental is incapacitated (a marine is removed).</summary>
    /// <remarks>MCX.EXE @ 0x0065e600</remarks>
    int32_t handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <summary>Replaces the control (1 player, 2 AI, 3 keeps it) and gives it ElementalControlData.</summary>
    /// <remarks>MCX.EXE @ 0x0065bf00</remarks>
    int32_t setControl(uint32_t controlType, uint32_t controlData, int32_t controlParam) override;
    /// <summary>Whether it rides in a carrier.</summary>
    /// <remarks>MCX.EXE @ 0x0065bc00 (inline in <c>object\elemntl.h</c>)</remarks>
    int inTransport() override { return transport != nullptr; }
    /// <summary>A marine is an elemental that can't jump.</summary>
    /// <remarks>MCX.EXE @ 0x0065bc30 (inline in <c>object\elemntl.h</c>)</remarks>
    int isMarine() override { return canJump() == 0; }
    /// <summary>Mover's, plus jump range, unknown8BC, max and current health.</summary>
    /// <remarks>MCX.EXE @ 0x0065f720</remarks>
    int32_t getVitalInfo(void* vitalInfo) override;
    /// <summary>Not while jumping.</summary>
    /// <remarks>MCX.EXE @ 0x0065bbf0 (inline in <c>object\elemntl.h</c>)</remarks>
    int canFireWeapons() override { return isJumping(nullptr) == 0; }
    /// <summary>Does nothing: infantry doesn't lock path cells.</summary>
    /// <remarks>MCX.EXE @ 0x0065bbd0 (inline in <c>object\elemntl.h</c>)</remarks>
    void updatePathLock(int set) override {}
    /// <summary>The frame's movement: jumps, pivots, then follows the move path (an idle marine wanders).</summary>
    /// <remarks>MCX.EXE @ 0x0065d7f0</remarks>
    void updateMovement() override;
    /// <summary>
    /// The combat value: weapons scaled by speed, plus health, tonnage, speed class and the other equipment.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0065c9c0</remarks>
    int32_t calcCV(int calcMax) override;
    /// <summary>Always 0.</summary>
    /// <remarks>MCX.EXE @ 0x0065e580</remarks>
    int32_t getBodyState() override;
    /// <summary>Mover's, for weapons; -1000 for any other item.</summary>
    /// <remarks>MCX.EXE @ 0x0065e590</remarks>
    float calcAttackChance(GameObject* target, int32_t aimLocation, float targetTime, int32_t weaponIndex,
                           float modifiers, int32_t* range, vector_3d* targetPoint) override;
    /// <summary>Always 0.</summary>
    /// <remarks>MCX.EXE @ 0x0065e5f0</remarks>
    int hitInventoryItem(int32_t itemIndex, int setupOnly) override;
    /// <summary>
    /// Fires a weapon at the target: the hit roll, missile volleys through anti-missile fire, the effect; a miss lands
    /// nearby.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0065e730</remarks>
    int32_t fireWeapon(GameObject* target, float targetTime, int32_t weaponIndex, int32_t attackType,
                       int32_t aimLocation, vector_3d* targetPoint) override;
    /// <summary>Whether it has a jump range.</summary>
    /// <remarks>MCX.EXE @ 0x0065bc10 (inline in <c>object\elemntl.h</c>)</remarks>
    int canJump() override { return jumpRange > 0.0; }
    /// <summary>The jump range (one vertex), 32 offsets at cost 20; 0 for a marine.</summary>
    /// <remarks>MCX.EXE @ 0x0065cb20</remarks>
    float getJumpRange(int32_t* numOffsets, int32_t* jumpCost) override;
    /// <summary>Whether a jump is under way; copies its goal to <paramref name="jumpGoal"/> when given.</summary>
    /// <remarks>MCX.EXE @ 0x0065caf0</remarks>
    int isJumping(vector_3d* jumpGoal) override;
    // Slots 219.. are Elemental's own.
    /// <summary>
    /// Drives a jump under way: sets it up, turns toward the landing, and on landing goes back on the path.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0065cb70</remarks>
    virtual int updateJump();
    /// <summary>Pivots in place (forward, reverse or toward the target) before moving; nonzero while turning.</summary>
    /// <remarks>MCX.EXE @ 0x0065ccf0</remarks>
    virtual int pivotTo();
    /// <summary>Back to moving forward, unless pivoting toward a target (or forward) with no path.</summary>
    /// <remarks>MCX.EXE @ 0x0065d190</remarks>
    virtual void updateMoveStateGoal();
    /// <summary>Follows the move path: the next step, a jump step, or the turn and throttle toward it.</summary>
    /// <remarks>MCX.EXE @ 0x0065d1d0</remarks>
    virtual int updateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                               int32_t& newGestureStateGoal, int32_t& newMoveState, int32_t& minThrottle,
                               int32_t& maxThrottle);
    /// <summary>At the path's end: on to the next way point, or done moving.</summary>
    /// <remarks>MCX.EXE @ 0x0065d640</remarks>
    virtual void setNextMovePath(char& newThrottleSetting, int32_t& newGestureStateGoal);
    /// <summary>
    /// Applies the frame's requests to the control data, setting up jumps for jump steps and jump orders.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0065d6a0</remarks>
    virtual void setControlSettings(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                    int32_t& newGestureStateGoal, int32_t& minThrottle, int32_t& maxThrottle);
    /// <summary>The control data's throttle.</summary>
    /// <remarks>MCX.EXE @ 0x0065bbe0 (inline in <c>object\elemntl.h</c>)</remarks>
    virtual int32_t getThrottle();
    using Mover::init;

    /// <summary>Profile "JumpRange"; 0 for marines.</summary>
    float jumpRange = 0.0f; // +0x8a0
    /// <summary>-100 by init.</summary>
    float jumpTime = -100.0f; // +0x8a4
    /// <summary>Set while jumping.</summary>
    int32_t inJump = 0; // +0x8a8
    /// <summary>Where the jump lands.</summary>
    vector_3d jumpGoal; // +0x8ac
    /// <summary>Zeroed by init and when the elemental dies (update, handleWeaponHit, handleDestruction); read by
    /// render and update.</summary>
    int32_t unknown8B8 = 0; // +0x8b8
    /// <summary>Only read (getVitalInfo).</summary>
    int32_t unknown8BC = 0; // +0x8bc
    /// <summary>The type's "MaxHealth" (11 by init).</summary>
    int32_t maxHealth = 11; // +0x8c0
    /// <summary>Profile "CurHealth" (11 by init); the damage comes off it.</summary>
    int32_t curHealth = 11; // +0x8c4
    /// <summary>Zeroed by init; never read.</summary>
    int32_t unknown8C8 = 0; // +0x8c8
    /// <summary>The type's "CanJump": an elemental rather than a marine. A marine is taken off the map when it
    /// dies.</summary>
    int32_t elementalCanJump = 1; // +0x8cc
    /// <summary>Set once the dead elemental has been taken off the interface; update then does nothing more.
    /// </summary>
    int32_t removed = 0; // +0x8d0
    /// <summary>The carrier it rides in (set by the scenario and the deploy orders).</summary>
    GameObject* transport = nullptr; // +0x8d4
};
