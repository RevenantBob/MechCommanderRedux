#pragma once

#include "object/MCMover.h"
#include "object/MCPilotOrders.h"

class MCFitIniFile;

/// <summary>
/// An elemental squad or a marine: infantry that walks, jumps (elementals only), rides in carriers, and is removed
/// from the interface when its health runs out.
/// </summary>
/// <remarks>Original source: <c>object\elemntl.cpp</c>, <c>object\elemntl.h</c>. Its movement is in
/// <c>MCElementalMovement.cpp</c>, its weapons and damage in <c>MCElementalCombat.cpp</c>.</remarks>
class MCElemental : public MCMover
{
public:
    /// <summary>Class ELEMENTAL; health 11 of 11; no jump; not in transport.</summary>
    MCElemental();
    ~MCElemental() override;

    /// <summary>Copies the type's data, makes the dynamics and the ElementalActor appearance.</summary>
    int32_t Init(MCObjectType* objType) override;
    using MCMover::Init;
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
    int32_t LoadProfile(MCFitIniFile& elementalFile) override;
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

    /// <summary>
    /// Drives a jump under way: sets it up, turns toward the landing, and on landing goes back on the path.
    /// </summary>
    int UpdateJump();
    /// <summary>Pivots in place (forward, reverse or toward the target) before moving; nonzero while turning.</summary>
    int PivotTo();
    /// <summary>Back to moving forward, unless pivoting toward a target (or forward) with no path.</summary>
    void UpdateMoveStateGoal();
    /// <summary>Follows the move path: the next step, a jump step, or the turn and throttle toward it.</summary>
    int UpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec, int32_t& newGestureStateGoal,
                       MCMoveState& newMoveState, int32_t& minThrottle, int32_t& maxThrottle);
    /// <summary>At the path's end: on to the next way point, or done moving.</summary>
    void SetNextMovePath(char& newThrottleSetting, int32_t& newGestureStateGoal);
    /// <summary>
    /// Applies the frame's requests to the control data, setting up jumps for jump steps and jump orders.
    /// </summary>
    void SetControlSettings(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                            int32_t& newGestureStateGoal, int32_t& minThrottle, int32_t& maxThrottle);
    /// <summary>A marine's death: it is taken off the interface at once, leaving no wreck.</summary>
    /// <param name="deathTime">The death timer to set (0.8 when it goes quietly, 0 when shot).</param>
    void RemoveMarine(float deathTime);

    /// <summary>Profile "JumpRange"; 0 for marines.</summary>
    float JumpRange = 0.0f;
    /// <summary>-100 to start with.</summary>
    float JumpTime = -100.0f;
    /// <summary>Set while jumping.</summary>
    bool InJump = false;
    /// <summary>Where the jump lands.</summary>
    MCVector3D JumpGoal;
    /// <summary>The type's "MaxHealth" (11 to start with).</summary>
    int32_t MaxHealth = 11;
    /// <summary>Profile "CurHealth" (11 to start with); the damage comes off it.</summary>
    int32_t CurHealth = 11;
    /// <summary>The type's "CanJump": an elemental rather than a marine. A marine is taken off the map when it
    /// dies.</summary>
    bool ElementalCanJump = true;
    /// <summary>Set once the dead elemental has been taken off the interface; update then does nothing more.
    /// </summary>
    bool Removed = false;
    /// <summary>The carrier it rides in (set by the scenario and the deploy orders).</summary>
    MCGameObject* Transport = nullptr;
};
