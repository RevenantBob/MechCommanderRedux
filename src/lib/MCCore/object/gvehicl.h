#pragma once

#include "gui/awindow.h"
#include "object/mover.h"
#include "object/MCObjectType.h"
#include "object/MCWeaponShotInfo.h"

class MCDynamicsType;
class MCFile;
class MCFitIniFile;
class MCSmoke;

/// <summary>A ground vehicle's armor locations, in the profile's order.</summary>
enum MCGroundVehicleLocation : int32_t
{
    GROUNDVEHICLE_LOCATION_FRONT = 0,
    GROUNDVEHICLE_LOCATION_LEFT = 1,
    GROUNDVEHICLE_LOCATION_RIGHT = 2,
    GROUNDVEHICLE_LOCATION_REAR = 3,
    GROUNDVEHICLE_LOCATION_TURRET = 4,
    NUM_GROUNDVEHICLE_LOCATIONS = 5,
};

/// <summary>Most seats a vehicle type may have ("Seats", asserted).</summary>
constexpr int32_t MAX_GROUNDVEHICLE_SEATS = 4;
/// <summary>Terrain tile types per chassis row of <see cref="TileThrottleMultiplier"/>.</summary>
constexpr int32_t NUM_THROTTLE_TILE_TYPES = 59;
/// <summary>Overlay types per chassis row of <see cref="OverlayThrottleMultiplier"/>.</summary>
constexpr int32_t NUM_THROTTLE_OVERLAY_TYPES = 75;

/// <summary>The effect object type of each weapon effect (MasterComponent::weaponEffect).</summary>
extern uint32_t WeaponFXTable[32];
/// <summary>"GroundVehicle.FireWeapon" "AttackerMoveModifier".</summary>
extern int32_t GroundVehicleAttackerMoveModifier[4];
/// <summary>"GroundVehicle.Damage" "CriticalHitTable".</summary>
extern int32_t GroundVehicleCriticalHitTable[11];
/// <summary>Throttle factor per chassis and terrain tile type (calcThrottleLimits).</summary>
extern float TileThrottleMultiplier[3][NUM_THROTTLE_TILE_TYPES];
/// <summary>Throttle factor per chassis and overlay type (calcThrottleLimits).</summary>
extern float OverlayThrottleMultiplier[3][NUM_THROTTLE_OVERLAY_TYPES];
/// <summary>"GroundVehicle.Movement" "CrashAvoidSelf": the default for types that don't set it.</summary>
extern int32_t DefaultGroundVehicleCrashAvoidSelf;
/// <summary>"GroundVehicle.Movement" "CrashAvoidPath".</summary>
extern int32_t DefaultGroundVehicleCrashAvoidPath;
/// <summary>"GroundVehicle.Movement" "CrashBlockSelf".</summary>
extern int32_t DefaultGroundVehicleCrashBlockSelf;
/// <summary>"GroundVehicle.Movement" "CrashBlockPath".</summary>
extern int32_t DefaultGroundVehicleCrashBlockPath;
/// <summary>"GroundVehicle.Movement" "CrashYieldTime".</summary>
extern float DefaultGroundVehicleCrashYieldTime;
/// <summary>"GroundVehicle.Collision" "collisionThreshold".</summary>
extern float GvCollisionThreshold;
/// <summary>"GroundVehicle.Collision" "objectThreshold".</summary>
extern float GvObjectCollisionThreshold;
/// <summary>"GroundVehicle.Collision" "tonnageThreshold".</summary>
extern float GvTonnageCollisionThreshold;
/// <summary>"GroundVehicle.Collision" "treeDeflection". At 0x007de514 (file-static, no symbol; the name is the
/// port's).</summary>
extern float GvTreeDeflection;
/// <summary>"GroundVehicle.Movement" "SweeperSlowTime": how long a mine sweeper crawls after clearing a mine.</summary>
extern float GvSweepTime;
/// <summary>"GroundVehicle.Movement" "HillSpeedFactor".</summary>
extern float GvHillSpeedFactor;
/// <summary>Set by updateMovePath: 100 m in world units, or the distance left on reaching the path's end.
/// Nothing reads it.</summary>
extern float MaxVelocityMag;

/// <summary>
/// Reads the ground vehicle blocks of the game system file: attacker move modifiers, the critical hit table, the
/// collision thresholds and the movement defaults (crash avoidance, sweeper time, walk speed, hill factor).
/// </summary>
/// <returns>0, or the first FitIniFile error.</returns>
int32_t LoadGroundVehicleGameSystem(MCFitIniFile* sysFile);

/// <summary>A ground vehicle type: the vehicle file's general data, internal structure, dynamics and movement.</summary>
/// <remarks>Original source: <c>object\gvehicl.cpp</c>; 0x98 bytes.</remarks>
class MCGroundVehicleType : public MCObjectType
{
public:
    ~MCGroundVehicleType() override { Destroy(); }

    /// <summary>Clears the fields; crash avoidance from the DefaultGroundVehicle values.</summary>
    void Init();
    /// <summary>
    /// Reads the vehicle file ("GroundVehicleType"): "General" (id, alignment, name, chassis, tonnage, ammo truck,
    /// refit points, mine sweeper/layer, elemental carrier, seats, explosion), "InternalStructure" per location,
    /// "Dynamics" (type 2), "MovementSystem", then the common type data.
    /// </summary>
    /// <returns>0, -1 for the wrong file type, -0x5fffd for the wrong dynamics type, -0x5fffe out of memory, or the
    /// FitIniFile error.</returns>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>Frees the name and the dynamics type.</summary>
    void Destroy() override;
    /// <summary>Makes a <see cref="MCGroundVehicle"/> of this type and gives it the next object id.</summary>
    std::unique_ptr<MCBaseObject> CreateInstance() override;
    /// <summary>Ramming, trees, buildings, mines and weapons against a vehicle of this type.</summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    /// <summary>Kills the vehicle: disables its sensor, alarms its pilot, sets the destroyed flags and takes it off
    /// the interface.</summary>
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;
    /// <summary>Does nothing (returns 0).</summary>
    int32_t LoadHotSpots(MCFitIniFile* vehicleFile);

    /// <summary>"ID".</summary>
    uint32_t VehicleId = 0;
    /// <summary>"Name".</summary>
    std::string Name;
    /// <summary>"Alignment", mapped 0 -> 1, 1 -> 0xff; copied to GameObject::alignment.</summary>
    uint8_t Alignment = 0;
    /// <summary>"Chassis".</summary>
    uint8_t Chassis = 0;
    /// <summary>"TonnageClass".</summary>
    float TonnageClass = 0.0f;
    /// <summary>Zeroed by init, never read from the file; copied to Mover::internalStructureTonnage.</summary>
    float InternalStructureTonnage = 0.0f;
    /// <summary>"InternalStructure": "Front", "Left", "Right", "Rear", "Turret".</summary>
    uint8_t InternalStructure[NUM_GROUNDVEHICLE_LOCATIONS] = {};
    /// <summary>The dynamics type (a GroundVehicleDynamicsType).</summary>
    MCDynamicsType* DynamicsType = nullptr;
    /// <summary>"CrashAvoidSelf".</summary>
    int32_t CrashAvoidSelf = 0;
    /// <summary>"CrashAvoidPath".</summary>
    int32_t CrashAvoidPath = 0;
    /// <summary>"CrashBlockSelf".</summary>
    int32_t CrashBlockSelf = 0;
    /// <summary>"CrashBlockPath".</summary>
    int32_t CrashBlockPath = 0;
    /// <summary>"CrashYieldTime".</summary>
    float CrashYieldTime = 0.0f;
    /// <summary>"ExplosionDamage" (0 when missing).</summary>
    float ExplDmg = 0.0f;
    /// <summary>"ExplosionRadius" (0 when missing).</summary>
    float ExplRad = 0.0f;
    /// <summary>"RefitPoints"; nonzero makes the vehicle a refitter.</summary>
    int32_t RefitPoints = 0;
    /// <summary>"AmmoTruck".</summary>
    int32_t AmmoTruck = 0;
    /// <summary>"MineSweeper".</summary>
    int32_t MineSweeper = 0;
    /// <summary>"MinesToLay"; above 0 makes the vehicle a mine layer.</summary>
    int32_t MinesToLay = 0;
    /// <summary>"ElementalCarrier".</summary>
    int32_t ElementalCarrier = 0;
    /// <summary>"Seats", at most <see cref="MAX_GROUNDVEHICLE_SEATS"/>.</summary>
    uint8_t Seats = 0;
};

/// <summary>
/// A ground vehicle (tanks, APCs, trucks) or pop-up turret: tracked or wheeled movement over the terrain, an optional
/// turret, mine sweeping and laying, refitting, and a marine who bails out when it dies.
/// </summary>
/// <remarks>Original source: <c>object\gvehicl.cpp</c>, <c>object\gvehicl.h</c>; 0x948 bytes.</remarks>
class MCGroundVehicle : public MCMover
{
public:
    /// <summary>Runs <see cref="init()"/> after the bases' (inlined into <c>GroundVehicleType::createInstance</c>).</summary>
    MCGroundVehicle() { Init(); }
    ~MCGroundVehicle() override { Destroy(); }

    /// <summary>
    /// Class GROUND_VEHICLE; five armor locations; movement and turret working; no status window, smoke, pilot;
    /// the mine cell -1.
    /// </summary>
    void Init() override;
    /// <summary>
    /// Copies the type's data (internal structure, chassis, crash avoidance, refit/sweeper/layer/carrier flags,
    /// seats), makes the dynamics, and the appearance: a GVAppearance (turret) or a PUAppearance (pop-up turret).
    /// </summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Frees the crew name; closes the status window.</summary>
    void Destroy() override;
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
    int32_t Init(MCFitIniFile* vehicleFile) override;
    int OnScreen() override;
    int32_t CalcHitLocation(MCGameObject* attacker, int32_t weaponIndex, int32_t attackSource,
                            int32_t attackType) override;
    int32_t HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <summary>Replaces the control (1 player, 2 AI, 3 network) and gives it GroundVehicleControlData.</summary>
    int32_t SetControl(uint32_t controlType, uint32_t controlData, int32_t controlParam) override;
    float RelFacingTo(MCVector3D goal, int32_t bodyPart) override;
    /// <summary>relFacingTo the goal, from the turret.</summary>
    float RelViewFacingTo(MCVector3D goal) override;
    int32_t OpenStatusWindow(int32_t left, int32_t top, int32_t right, int32_t bottom) override;
    int32_t CloseStatusWindow() override;
    /// <summary>Flagged captureable (or unknown6C set), and neither disabled nor destroyed.</summary>
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
    /// <summary>The appearance's gesture (+0x74).</summary>
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
    /// <summary>Whether the vehicle can move (movementEnabled).</summary>
    int CanMove() override;
    /// <summary>The long name (Mover::debugStatus).</summary>
    const char* GetIfaceName() override { return DebugStatus.c_str(); }
    // Slots 219.. are GroundVehicle's own.
    /// <summary>Does nothing: vehicles make no piloting checks.</summary>
    virtual void PilotingCheck() {}
    virtual void NetUpdateMovement();
    /// <summary>Rolls a critical hit on the vehicle table; may disable movement or the turret.</summary>
    virtual int CalcCriticalHitV(int32_t& hitLocation);
    /// <summary>The control data's throttle.</summary>
    virtual int32_t GetThrottle();
    using MCMover::Init;
    using MCMover::PilotingCheck;

    int PivotTo();
    /// <summary>Scales the throttle limits by the chassis' factors for the cell's terrain and overlay.</summary>
    void CalcThrottleLimits(int32_t& minThrottle, int32_t& maxThrottle);
    void UpdateMoveStateGoal();
    int UpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec, int32_t& newMoveState,
                       int32_t& minThrottle, int32_t& maxThrottle);
    void SetNextMovePath(char& newThrottleSetting);
    void SetControlSettings(char& newRotate, char& newThrottleSetting, float& newRotatePerSec, int32_t& minThrottle,
                            int32_t& maxThrottle);
    void UpdateTurret(float newRotatePerSec);
    int NetUpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec, int32_t& newMoveState,
                          int32_t& minThrottle, int32_t& maxThrottle);
    /// <summary>
    /// Makes the marine who bails out of the dead vehicle (DefaultPilotId, the marine profile), hands him the
    /// vehicle's warrior and sends him off.
    /// </summary>
    void CreateVehiclePilot();

    /// <summary>1 by init; a critical hit clears it. canMove returns it.</summary>
    int32_t MovementEnabled = 1;
    /// <summary>1 by init; a critical hit clears it.</summary>
    int32_t TurretEnabled = 1;
    /// <summary>The turret's yaw relative to the body (updateTurret; relFacingTo adds it).</summary>
    float TurretRotation = 0.0f;
    /// <summary>Whether the weapons can fire: always for a turret vehicle; a pop-up turret once it is up.</summary>
    int32_t WeaponsDeployed = 1;
    /// <summary>1 with a GVAppearance (a vehicle), 0 with a PUAppearance (a pop-up turret).</summary>
    int32_t GvAppearance = 0;
    /// <summary>The status window.</summary>
    MCGuiTitleWindow* StatusWindow = nullptr;
    /// <summary>The smoke of a disabled or destroyed vehicle.</summary>
    MCSmoke* Smoke = nullptr;
    /// <summary>Whether the vehicle can be captured.</summary>
    int32_t Captureable = 0;
    /// <summary>Set when the type has refit points (the armor slot's refit pool at +0x20 of the armor block).</summary>
    int32_t Refitter = 0;
    /// <summary>Set while a refit truck is refitting (TacticalOrder's refit, stage 3 until done); a stopped vehicle then
    /// shows its extra (refit) state instead of the normal one.</summary>
    int32_t Refitting = 0;
    /// <summary>The type's "MineSweeper".</summary>
    int32_t MineSweeper = 0;
    /// <summary>Seconds since the sweeper last cleared a mine; -1 when it hasn't.</summary>
    float SweepTime = -1.0f;
    /// <summary>Set when the type lays mines.</summary>
    int32_t MineLayer = 0;
    /// <summary>The type's "MinesToLay".</summary>
    int32_t MinesToLay = 0;
    /// <summary>The tile column the layer last mined; -1 for none.</summary>
    int32_t CellColToMine = -1;
    /// <summary>The tile row the layer last mined; -1 for none.</summary>
    int32_t CellRowToMine = -1;
    /// <summary>The type's "ElementalCarrier".</summary>
    int32_t ElementalCarrier = 0;
    /// <summary>The elementals an elemental carrier holds (TacticalOrder LOAD_INTO_CARRIER fills it, DEPLOY_ELEMENTALS
    /// empties it).</summary>
    MCMover* Elementals[10] = {};
    /// <summary>The pilots riding in the seats (<see cref="Seats"/>): taken from captured prisons, and put into
    /// captured mechs (TacticalOrder::status, CAPTURE).</summary>
    MCMechWarrior* Passengers[4] = {};
    /// <summary>The type's "Seats".</summary>
    uint8_t Seats = 0;
    /// <summary>Profile "Crew".</summary>
    std::string CrewName;
    /// <summary>Profile "NotMineYet" (1 when missing).</summary>
    int32_t NotMineYet = 0;
    /// <summary>The marine who bailed out (createVehiclePilot).</summary>
    MCMover* VehiclePilot = nullptr;
    /// <summary>Set once mineCheck has handled the current cell's mine; cleared when the vehicle is on a cell
    /// without one.</summary>
    int32_t MineCellHandled = 0;
    /// <summary>Profile "BattleRating"; -1 when missing (calcCV computes one).</summary>
    int32_t BattleRating = -1;
    /// <summary>Profile "DescIndex" (-1 when missing): the long name is string 700 + this.</summary>
    int32_t DescIndex = -1;
    /// <summary>Profile "NameIndex".</summary>
    int32_t NameIndex = 0;
    /// <summary>Seconds the mine layer has spent on the current cell (lays when over MineWaitTime).</summary>
    float MineLayTime = 0.0f;
};

/// <summary>A ground vehicle's status window.</summary>
/// <remarks>Original source: <c>object\gvehicl.cpp</c>, <c>object\gvehicl.h</c>; 0x4c4 bytes.</remarks>
class MCGroundVehicleStatusWindow : public MCGuiTitleWindow
{
public:
    ~MCGroundVehicleStatusWindow() override;
    void Init(int32_t x, int32_t y, int32_t w, int32_t h, MCGroundVehicle* newVehicle);
    void HandleEvent(MCGuiEvent* event) override;
    void Resize(int32_t w, int32_t h) override;
    void Display() override;
    void Draw() override;
    /// <summary>Port: still paints a picture (in display), so it keeps one.</summary>
    bool DrawsLive() override { return false; }
    virtual MCGroundVehicle* GetVehicle() { return Vehicle; }

    /// <summary>The vehicle shown.</summary>
    MCGroundVehicle* Vehicle = nullptr;
};
