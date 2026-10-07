#pragma once

#include "object/mover.h"
#include "object/objtype.h"

class MCDynamicsType;
class MCFile;
class MCFitIniFile;

/// <summary>"Elemental.Collision" "DamageOnImpact".</summary>
extern float ElmDamageOnImpact;
/// <summary>Meters from its last target within which an elemental paths without jumping (75).</summary>
extern float ElementalTargetNoJumpDistance;
/// <summary>Whether the old (pre-Terrain::projectTerrain) screen projection is used.</summary>
extern int UseOldProject;

/// <summary>Reads the "Elemental.Collision" and "Elemental.Combat" blocks of the game system file.</summary>
/// <returns>0, or the FitIniFile error of the collision block.</returns>
int32_t LoadElementalGameSystem(MCFitIniFile* sysFile);

/// <summary>An elemental type: armoured infantry (jumping elementals) or marines (who can't jump).</summary>
/// <remarks>Original source: <c>object\elemntl.cpp</c>, <c>object\elemntl.h</c>; 0x50 bytes.</remarks>
class MCElementalType : public MCObjectType
{
public:
    /// <summary>Runs <see cref="init()"/>.</summary>
    MCElementalType() { Init(); }
    ~MCElementalType() override { Destroy(); }

    /// <summary>Clears the fields; canJump 1.</summary>
    void Init();
    /// <summary>
    /// Reads the elemental file ("ElementalType"): "General" (id, can jump, alignment, name, max health),
    /// "Dynamics" (type 3), then the common type data.
    /// </summary>
    /// <returns>0, -1 for the wrong file type, -0x5fffd for the wrong dynamics type, -0x5fffe out of memory, or the
    /// FitIniFile error.</returns>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>Frees the name and the dynamics type.</summary>
    void Destroy() override;
    /// <summary>Makes an <see cref="MCElemental"/> of this type and gives it the next object id.</summary>
    MCBaseObject* CreateInstance() override;
    /// <summary>
    /// Knocks the elemental aside: an enemy mech or vehicle ramming it (a marine: any), a building (with damage by its
    /// tonnage), a tree, a train car.
    /// </summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    /// <summary>Kills the elemental: disables its sensor, alarms its pilot, takes it off the interface and sets the
    /// friendly/enemy destroyed flag.</summary>
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>"ID".</summary>
    uint32_t ElementalId = 0;
    /// <summary>"Name".</summary>
    std::string Name;
    /// <summary>"Type", mapped 0 -> 1, 1 -> 0xff: the alignment (compared with the home team's id).</summary>
    uint8_t Alignment = 0;
    /// <summary>"MaxHealth".</summary>
    uint8_t MaxHealth = 0;
    /// <summary>The dynamics type (an ElementalDynamicsType).</summary>
    MCDynamicsType* DynamicsType = nullptr;
    /// <summary>"CanJump" (1 when missing): elementals jump; marines don't.</summary>
    int32_t CanJump = 1;
};

/// <summary>
/// An elemental squad or a marine: infantry that walks, jumps (elementals only), rides in carriers, and is removed
/// from the interface when its health runs out.
/// </summary>
/// <remarks>Original source: <c>object\elemntl.cpp</c>, <c>object\elemntl.h</c>; 0x8d8 bytes.</remarks>
class MCElemental : public MCMover
{
public:
    /// <summary>Runs <see cref="init()"/>.</summary>
    MCElemental() { Init(); }
    ~MCElemental() override { Destroy(); }

    /// <summary>Class ELEMENTAL; health 11 of 11; no jump; not in transport.</summary>
    void Init() override;
    /// <summary>Copies the type's data, makes the dynamics and the ElementalActor appearance.</summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Does nothing.</summary>
    void Destroy() override;
    /// <summary>
    /// Moves it (the actor in a jump, the dynamics otherwise), marks what it sees; a dead one blows up after its timer,
    /// a marine off the screen is removed.
    /// </summary>
    int32_t Update() override;
    /// <summary>
    /// Draws it when seen (a sensor blip when only sensed), and the move path in the terrain-grid debug view.
    /// </summary>
    void Render() override;
    /// <summary>The elemental's position.</summary>
    MCVector3D GetPositionFromHS(uint32_t hotSpot) override;
    /// <summary>
    /// Reads the profile ("ElementalProfile"): name, tonnage, health, icon, engine, jump range and the inventory.
    /// </summary>
    int32_t Init(MCFitIniFile* elementalFile) override;
    /// <summary>Projects it to the screen; whether its appearance is on it.</summary>
    int OnScreen() override;
    /// <summary>Always 0 (a squad has one location).</summary>
    int32_t CalcHitLocation(MCGameObject* attacker, int32_t weaponIndex, int32_t attackSource,
                            int32_t attackType) override;
    /// <summary>Takes the damage off the health; at 0 the elemental is incapacitated (a marine is removed).</summary>
    int32_t HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <summary>Replaces the control (1 player, 2 AI, 3 keeps it) and gives it ElementalControlData.</summary>
    int32_t SetControl(uint32_t controlType, uint32_t controlData, int32_t controlParam) override;
    /// <summary>Whether it rides in a carrier.</summary>
    int InTransport() override { return Transport != nullptr; }
    /// <summary>A marine is an elemental that can't jump.</summary>
    int IsMarine() override { return CanJump() == 0; }
    /// <summary>Mover's, plus jump range, a zero word, max and current health.</summary>
    int32_t GetVitalInfo(void* vitalInfo) override;
    /// <summary>Not while jumping.</summary>
    int CanFireWeapons() override { return IsJumping(nullptr) == 0; }
    /// <summary>Does nothing: infantry doesn't lock path cells.</summary>
    void UpdatePathLock(int set) override {}
    /// <summary>The frame's movement: jumps, pivots, then follows the move path (an idle marine wanders).</summary>
    void UpdateMovement() override;
    /// <summary>
    /// The combat value: weapons scaled by speed, plus health, tonnage, speed class and the other equipment.
    /// </summary>
    int32_t CalcCV(int calcMax) override;
    /// <summary>Always 0.</summary>
    int32_t GetBodyState() override;
    /// <summary>Mover's, for weapons; -1000 for any other item.</summary>
    float CalcAttackChance(MCGameObject* target, int32_t aimLocation, float targetTime, int32_t weaponIndex,
                           float modifiers, int32_t* range, MCVector3D* targetPoint) override;
    /// <summary>Always 0.</summary>
    int HitInventoryItem(int32_t itemIndex, int setupOnly) override;
    /// <summary>
    /// Fires a weapon at the target: the hit roll, missile volleys through anti-missile fire, the effect; a miss lands
    /// nearby.
    /// </summary>
    int32_t FireWeapon(MCGameObject* target, float targetTime, int32_t weaponIndex, int32_t attackType,
                       int32_t aimLocation, MCVector3D* targetPoint) override;
    /// <summary>Whether it has a jump range.</summary>
    int CanJump() override { return JumpRange > 0.0; }
    /// <summary>The jump range (one vertex), 32 offsets at cost 20; 0 for a marine.</summary>
    float GetJumpRange(int32_t* numOffsets, int32_t* jumpCost) override;
    /// <summary>Whether a jump is under way; copies its goal to <paramref name="jumpGoal"/> when given.</summary>
    int IsJumping(MCVector3D* jumpGoal) override;
    // Slots 219.. are Elemental's own.
    /// <summary>
    /// Drives a jump under way: sets it up, turns toward the landing, and on landing goes back on the path.
    /// </summary>
    virtual int UpdateJump();
    /// <summary>Pivots in place (forward, reverse or toward the target) before moving; nonzero while turning.</summary>
    virtual int PivotTo();
    /// <summary>Back to moving forward, unless pivoting toward a target (or forward) with no path.</summary>
    virtual void UpdateMoveStateGoal();
    /// <summary>Follows the move path: the next step, a jump step, or the turn and throttle toward it.</summary>
    virtual int UpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                               int32_t& newGestureStateGoal, int32_t& newMoveState, int32_t& minThrottle,
                               int32_t& maxThrottle);
    /// <summary>At the path's end: on to the next way point, or done moving.</summary>
    virtual void SetNextMovePath(char& newThrottleSetting, int32_t& newGestureStateGoal);
    /// <summary>
    /// Applies the frame's requests to the control data, setting up jumps for jump steps and jump orders.
    /// </summary>
    virtual void SetControlSettings(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                    int32_t& newGestureStateGoal, int32_t& minThrottle, int32_t& maxThrottle);
    /// <summary>The control data's throttle.</summary>
    virtual int32_t GetThrottle();
    using MCMover::Init;

    /// <summary>Profile "JumpRange"; 0 for marines.</summary>
    float JumpRange = 0.0f;
    /// <summary>-100 by init.</summary>
    float JumpTime = -100.0f;
    /// <summary>Set while jumping.</summary>
    int32_t InJump = 0;
    /// <summary>Where the jump lands.</summary>
    MCVector3D JumpGoal;
    /// <summary>The type's "MaxHealth" (11 by init).</summary>
    int32_t MaxHealth = 11;
    /// <summary>Profile "CurHealth" (11 by init); the damage comes off it.</summary>
    int32_t CurHealth = 11;
    /// <summary>The type's "CanJump": an elemental rather than a marine. A marine is taken off the map when it
    /// dies.</summary>
    int32_t ElementalCanJump = 1;
    /// <summary>Set once the dead elemental has been taken off the interface; update then does nothing more.
    /// </summary>
    int32_t Removed = 0;
    /// <summary>The carrier it rides in (set by the scenario and the deploy orders).</summary>
    MCGameObject* Transport = nullptr;
};
