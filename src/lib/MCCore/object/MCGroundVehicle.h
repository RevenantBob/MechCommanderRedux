#pragma once

#include "object/MCGroundVehicleGameSystem.h"
#include "object/MCMover.h"
#include "object/MCPilotOrders.h"

class MCFitIniFile;
class MCSmoke;

/// <summary>
/// A ground vehicle (tanks, APCs, trucks) or pop-up turret: tracked or wheeled movement over the terrain, an optional
/// turret, mine sweeping and laying, refitting, and a marine who bails out when it dies.
/// </summary>
/// <remarks>Original source: <c>object\gvehicl.cpp</c>, <c>object\gvehicl.h</c>. Its movement is in
/// <c>MCGroundVehicleMovement.cpp</c>, its weapons and damage in <c>MCGroundVehicleCombat.cpp</c>, its network chunks
/// in <c>MCGroundVehicleNetwork.cpp</c>.</remarks>
class MCGroundVehicle : public MCMover
{
public:
    /// <summary>Elementals a carrier holds: the scenario's "Elemental0".."Elemental9" entries (data format).</summary>
    static constexpr int32_t MaxElementals = 10;

    /// <summary>
    /// Class GROUND_VEHICLE; five armor locations; movement and turret working; no smoke or pilot; the mine cell -1.
    /// </summary>
    MCGroundVehicle();
    ~MCGroundVehicle() override;

    /// <summary>
    /// Copies the type's data (internal structure, chassis, crash avoidance, refit/sweeper/layer/carrier flags,
    /// seats), makes the dynamics, and the appearance: a GVAppearance (turret) or a PUAppearance (pop-up turret).
    /// </summary>
    int32_t Init(MCObjectType* objType) override;
    using MCMover::Init;
    int32_t Update() override;
    void Render() override;
    /// <summary>The vehicle's position.</summary>
    MCVector3D GetPositionFromHS(uint32_t hotSpot) override;
    /// <summary>Collides with the gates, buildings and walls of its tile block while moving.</summary>
    void HandleStaticCollision() override;
    /// <summary>
    /// Reads the vehicle profile ("GroundVehicleProfile"): crew, description, name, tonnage, status, icon, battle
    /// rating, engine, movement system, armor, inventory (other, weapons, ammo) and the armor per location.
    /// </summary>
    int32_t LoadProfile(MCFitIniFile& vehicleFile) override;
    int OnScreen() override;
    int32_t CalcHitLocation(MCGameObject* attacker, int32_t weaponIndex, int32_t attackSource,
                            int32_t attackType) override;
    int32_t HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <summary>Replaces the control (1 player, 2 AI, 3 network) and gives it GroundVehicleControlData.</summary>
    int32_t SetControl(uint32_t controlType, uint32_t controlData, int32_t controlParam) override;
    float RelFacingTo(MCVector3D goal, int32_t bodyPart) override;
    /// <summary>relFacingTo the goal, from the turret.</summary>
    float RelViewFacingTo(MCVector3D goal) override;
    /// <summary>Flagged captureable, and neither disabled nor destroyed.</summary>
    int IsCaptureable() override;
    /// <summary>The refit points left, for a refitter; 0 otherwise.</summary>
    float GetRefitPoints() override;
    /// <summary>Takes the points from a refitter that has enough.</summary>
    int BurnRefitPoints(float pointsToBurn) override;
    int32_t GetVitalInfo(void* vitalInfo) override;
    /// <summary>From the dynamics.</summary>
    int32_t GetSpeedState() override;
    int CrashAvoidanceSystem() override;
    /// <summary>Sets off (or, for a sweeper, clears) the mines of the vehicle's cell.</summary>
    void MineCheck() override;
    void UpdateMovement() override;
    int32_t BuildStatusChunk() override;
    int32_t HandleStatusChunk(int32_t updateAge, uint32_t chunk) override;
    int32_t BuildMoveChunk() override;
    int32_t HandleMoveChunk(uint32_t chunk) override;
    /// <summary>The profile's battle rating, or one computed from the loadout: the weapons' ratings scaled by top
    /// speed, plus structure, armor, tonnage class, the speed class and the other equipment.</summary>
    int32_t CalcCV(int calcMax) override;
    /// <summary>The appearance's gesture.</summary>
    int32_t GetBodyState() override;
    /// <summary>Product of the armor left per location (scaled 0.4..1.0), the pilot's wound factor and the weapon
    /// effectiveness ratio; 0 when disabled or destroyed.</summary>
    float GetTotalEffectiveness() override;
    /// <summary>relFacingTo the target position, from the turret.</summary>
    float WeaponLocked(int32_t weaponIndex, MCVector3D targetPosition) override;
    float CalcAttackChance(MCGameObject* target, int32_t aimLocation, float targetTime, int32_t weaponIndex,
                           float modifiers, int32_t* range, MCVector3D* targetPoint) override;
    /// <summary>Fatal: vehicles have no inventory hits.</summary>
    int HitInventoryItem(int32_t itemIndex, int setupOnly) override;
    /// <summary>Disables the vehicle and starts its smoke.</summary>
    void Disable(uint32_t cause) override;
    /// <summary>Does nothing.</summary>
    void DestroyBodyLocation(int32_t location) override;
    int InjureBodyLocation(int32_t bodyLocation, float damage) override;
    int32_t FireWeapon(MCGameObject* target, float targetTime, int32_t weaponIndex, int32_t attackType,
                       int32_t aimLocation, MCVector3D* targetPoint) override;
    int32_t HandleWeaponFire(int32_t weaponIndex, MCGameObject* target, MCVector3D* targetPoint, int hit,
                             float entryAngle, int32_t numMissiles, int32_t missilesPastAMS, int32_t antiMissileShots,
                             int32_t hitLocation) override;
    /// <summary>Whether the vehicle can move (<see cref="MovementEnabled"/>).</summary>
    int CanMove() override;
    /// <summary>The long name (Mover::debugStatus).</summary>
    const char* GetIfaceName() override { return DebugStatus.c_str(); }

    /// <summary>A networked vehicle's movement: follows the move chunks.</summary>
    virtual void NetUpdateMovement();
    /// <summary>Rolls a critical hit on the vehicle table; may disable movement or the turret.</summary>
    virtual int CalcCriticalHitV(int32_t& hitLocation);
    /// <summary>The control data's throttle.</summary>
    virtual int32_t GetThrottle();

    /// <summary>Pivots in place (forward, reverse or toward the target) before moving; nonzero while turning.</summary>
    int PivotTo();
    /// <summary>Scales the throttle limits by the chassis' factors for the cell's terrain and overlay.</summary>
    void CalcThrottleLimits(int32_t& minThrottle, int32_t& maxThrottle);
    /// <summary>Back to moving forward, unless pivoting with no path.</summary>
    void UpdateMoveStateGoal();
    /// <summary>Follows the move path: the next step, or the turn and throttle toward it. Nonzero once the path is
    /// done.</summary>
    int UpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec, MCMoveState& newMoveState,
                       int32_t& minThrottle, int32_t& maxThrottle);
    /// <summary>At the path's end: on to the next way point, or done moving.</summary>
    void SetNextMovePath(char& newThrottleSetting);
    /// <summary>Applies the frame's requests to the control data.</summary>
    void SetControlSettings(char& newRotate, char& newThrottleSetting, float& newRotatePerSec, int32_t& minThrottle,
                            int32_t& maxThrottle);
    /// <summary>Turns the turret toward the target (or back to straight) at the dynamics' rate.</summary>
    void UpdateTurret(float newRotatePerSec);
    /// <summary>A networked vehicle's <see cref="UpdateMovePath"/> (nothing: the move chunks steer it).</summary>
    int NetUpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec, MCMoveState& newMoveState,
                          int32_t& minThrottle, int32_t& maxThrottle);
    /// <summary>
    /// Makes the marine who bails out of the dead vehicle (DefaultPilotId, the marine profile), hands him the
    /// vehicle's warrior and sends him off.
    /// </summary>
    void CreateVehiclePilot();

    /// <summary>True to start with; a critical hit clears it. canMove returns it.</summary>
    bool MovementEnabled = true;
    /// <summary>True to start with; a critical hit clears it.</summary>
    bool TurretEnabled = true;
    /// <summary>The turret's yaw relative to the body (updateTurret; relFacingTo adds it).</summary>
    float TurretRotation = 0.0f;
    /// <summary>Whether the weapons can fire: always for a turret vehicle; a pop-up turret once it is up.</summary>
    bool WeaponsDeployed = true;
    /// <summary>True with a GVAppearance (a vehicle), false with a PUAppearance (a pop-up turret).</summary>
    bool GvAppearance = false;
    /// <summary>The smoke of a disabled or destroyed vehicle; the vehicle owns it.</summary>
    std::unique_ptr<MCSmoke> Smoke;
    /// <summary>Whether the vehicle can be captured.</summary>
    bool Captureable = false;
    /// <summary>Set when the type has refit points (the armor slot's refit pool).</summary>
    bool Refitter = false;
    /// <summary>Set while a refit truck is refitting (TacticalOrder's refit, stage 3 until done); a stopped vehicle
    /// then shows its extra (refit) state instead of the normal one.</summary>
    bool Refitting = false;
    /// <summary>The type's "MineSweeper".</summary>
    bool MineSweeper = false;
    /// <summary>Seconds since the sweeper last cleared a mine; -1 when it hasn't.</summary>
    float SweepTime = -1.0f;
    /// <summary>Set when the type lays mines.</summary>
    bool MineLayer = false;
    /// <summary>The type's "MinesToLay".</summary>
    int32_t MinesToLay = 0;
    /// <summary>The tile column the layer last mined; -1 for none.</summary>
    int32_t CellColToMine = -1;
    /// <summary>The tile row the layer last mined; -1 for none.</summary>
    int32_t CellRowToMine = -1;
    /// <summary>The type's "ElementalCarrier".</summary>
    bool ElementalCarrier = false;
    /// <summary>The elementals an elemental carrier holds (the scenario and TacticalOrder LOAD_INTO_CARRIER fill it,
    /// DEPLOY_ELEMENTALS empties it); empty slots are null.</summary>
    std::array<MCMover*, MaxElementals> Elementals{};
    /// <summary>The pilots riding in the seats (<see cref="Seats"/>): taken from captured prisons, and put into
    /// captured mechs (TacticalOrder::status, CAPTURE); empty seats are null.</summary>
    std::array<MCMechWarrior*, MaxGroundVehicleSeats> Passengers{};
    /// <summary>The type's "Seats".</summary>
    uint8_t Seats = 0;
    /// <summary>Profile "Crew".</summary>
    std::string CrewName;
    /// <summary>Profile "NotMineYet" (true when missing).</summary>
    bool NotMineYet = false;
    /// <summary>The marine who bailed out (createVehiclePilot); the mech list owns him.</summary>
    MCMover* VehiclePilot = nullptr;
    /// <summary>Set once mineCheck has handled the current cell's mine; cleared when the vehicle is on a cell
    /// without one.</summary>
    bool MineCellHandled = false;
    /// <summary>Profile "BattleRating"; -1 when missing (calcCV computes one).</summary>
    int32_t BattleRating = -1;
    /// <summary>Profile "DescIndex" (-1 when missing): the long name is string 700 + this.</summary>
    int32_t DescIndex = -1;
    /// <summary>Profile "NameIndex".</summary>
    int32_t NameIndex = 0;
    /// <summary>Seconds the mine layer has spent on the current cell (lays when over MineWaitTime).</summary>
    float MineLayTime = 0.0f;
};
