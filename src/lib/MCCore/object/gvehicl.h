#pragma once

#include "gui/awindow.h"
#include "object/mover.h"
#include "object/objtype.h"

class DynamicsType;
class File;
class FitIniFile;
class Smoke;

/// <summary>A ground vehicle's armor locations, in the profile's order.</summary>
enum GroundVehicleLocation : int32_t
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
/// <remarks>MCX.EXE @ 0x00793dd0</remarks>
extern uint32_t weaponFXTable[32];
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
extern float gvCollisionThreshold;
/// <summary>"GroundVehicle.Collision" "objectThreshold".</summary>
extern float gvObjectCollisionThreshold;
/// <summary>"GroundVehicle.Collision" "tonnageThreshold".</summary>
extern float gvTonnageCollisionThreshold;
/// <summary>"GroundVehicle.Collision" "treeDeflection". DAT_007de514: unnamed in the binary (file-static), the name is
/// the port's.</summary>
extern float gvTreeDeflection;
/// <summary>"GroundVehicle.Movement" "SweeperSlowTime": how long a mine sweeper crawls after clearing a mine.</summary>
extern float gvSweepTime;
/// <summary>"GroundVehicle.Movement" "HillSpeedFactor".</summary>
extern float gvHillSpeedFactor;
/// <summary>Set by updateMovePath: 100 m in world units, or the distance left on reaching the path's end.
/// Nothing reads it.</summary>
/// <remarks>MCX.EXE @ 0x007de664</remarks>
extern float MaxVelocityMag;

/// <summary>
/// Reads the ground vehicle blocks of the game system file: attacker move modifiers, the critical hit table, the
/// collision thresholds and the movement defaults (crash avoidance, sweeper time, walk speed, hill factor).
/// </summary>
/// <returns>0, or the first FitIniFile error.</returns>
/// <remarks>MCX.EXE @ 0x00669110</remarks>
int32_t loadGroundVehicleGameSystem(FitIniFile* sysFile);

/// <summary>A ground vehicle type: the vehicle file's general data, internal structure, dynamics and movement.</summary>
/// <remarks>Original source: <c>object\gvehicl.cpp</c>; 0x98 bytes.</remarks>
class GroundVehicleType : public ObjectType
{
public:
    /// <remarks>MCX.EXE @ 0x00690700 (vector deleting destructor)</remarks>
    ~GroundVehicleType() override { destroy(); }

    /// <summary>Clears the fields; crash avoidance from the DefaultGroundVehicle values.</summary>
    /// <remarks>MCX.EXE @ 0x006692e0</remarks>
    void init();
    /// <summary>
    /// Reads the vehicle file ("GroundVehicleType"): "General" (id, alignment, name, chassis, tonnage, ammo truck,
    /// refit points, mine sweeper/layer, elemental carrier, seats, explosion), "InternalStructure" per location,
    /// "Dynamics" (type 2), "MovementSystem", then the common type data.
    /// </summary>
    /// <returns>0, -1 for the wrong file type, -0x5fffd for the wrong dynamics type, -0x5fffe out of memory, or the
    /// FitIniFile error.</returns>
    /// <remarks>MCX.EXE @ 0x006693b0</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <summary>Frees the name and the dynamics type.</summary>
    /// <remarks>MCX.EXE @ 0x00669360</remarks>
    void destroy() override;
    /// <summary>Makes a <see cref="GroundVehicle"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x0066a220</remarks>
    BaseObject* createInstance() override;
    /// <summary>Ramming, trees, buildings, mines and weapons against a vehicle of this type.</summary>
    /// <remarks>MCX.EXE @ 0x00669860</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <summary>Kills the vehicle: disables its sensor, alarms its pilot, sets the destroyed flags and takes it off
    /// the interface.</summary>
    /// <remarks>MCX.EXE @ 0x0066a120</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;
    /// <summary>Does nothing (returns 0).</summary>
    /// <remarks>MCX.EXE @ 0x0066a210</remarks>
    int32_t loadHotSpots(FitIniFile* vehicleFile);

    /// <summary>"ID".</summary>
    uint32_t vehicleId = 0; // +0x30
    /// <summary>"Name" (systemHeap).</summary>
    char* name = nullptr; // +0x34
    /// <summary>"Alignment", mapped 0 -> 1, 1 -> 0xff; copied to GameObject::alignment.</summary>
    uint8_t alignment = 0; // +0x38
    /// <summary>"Chassis".</summary>
    uint8_t chassis = 0; // +0x39
    /// <summary>"TonnageClass".</summary>
    float tonnageClass = 0.0f; // +0x3c
    /// <summary>Zeroed by init; never read (the mech type's EndoSteel slot).</summary>
    int32_t unknown40 = 0; // +0x40
    /// <summary>Zeroed by init; never read.</summary>
    int32_t unknown44 = 0; // +0x44
    /// <summary>Zeroed by init, never read from the file; copied to Mover::internalStructureTonnage.</summary>
    float internalStructureTonnage = 0.0f; // +0x48
    /// <summary>"InternalStructure": "Front", "Left", "Right", "Rear", "Turret".</summary>
    uint8_t internalStructure[NUM_GROUNDVEHICLE_LOCATIONS] = {}; // +0x4c
    /// <summary>Zeroed by init; never read.</summary>
    int32_t unknown54 = 0; // +0x54
    /// <summary>Not accessed.</summary>
    int32_t unknown58 = 0; // +0x58
    /// <summary>The dynamics type (a GroundVehicleDynamicsType).</summary>
    DynamicsType* dynamicsType = nullptr; // +0x5c
    /// <summary>Zeroed by init; never read.</summary>
    int32_t unknown60 = 0; // +0x60
    /// <summary>"CrashAvoidSelf".</summary>
    int32_t crashAvoidSelf = 0; // +0x64
    /// <summary>"CrashAvoidPath".</summary>
    int32_t crashAvoidPath = 0; // +0x68
    /// <summary>"CrashBlockSelf".</summary>
    int32_t crashBlockSelf = 0; // +0x6c
    /// <summary>"CrashBlockPath".</summary>
    int32_t crashBlockPath = 0; // +0x70
    /// <summary>"CrashYieldTime".</summary>
    float crashYieldTime = 0.0f; // +0x74
    /// <summary>"ExplosionDamage" (0 when missing).</summary>
    float explDmg = 0.0f; // +0x78
    /// <summary>"ExplosionRadius" (0 when missing).</summary>
    float explRad = 0.0f; // +0x7c
    /// <summary>"RefitPoints"; nonzero makes the vehicle a refitter.</summary>
    int32_t refitPoints = 0; // +0x80
    /// <summary>"AmmoTruck".</summary>
    int32_t ammoTruck = 0; // +0x84
    /// <summary>"MineSweeper".</summary>
    int32_t mineSweeper = 0; // +0x88
    /// <summary>"MinesToLay"; above 0 makes the vehicle a mine layer.</summary>
    int32_t minesToLay = 0; // +0x8c
    /// <summary>"ElementalCarrier".</summary>
    int32_t elementalCarrier = 0; // +0x90
    /// <summary>"Seats", at most <see cref="MAX_GROUNDVEHICLE_SEATS"/>.</summary>
    uint8_t seats = 0; // +0x94
};

/// <summary>
/// A ground vehicle (tanks, APCs, trucks) or pop-up turret: tracked or wheeled movement over the terrain, an optional
/// turret, mine sweeping and laying, refitting, and a marine who bails out when it dies.
/// </summary>
/// <remarks>Original source: <c>object\gvehicl.cpp</c>, <c>object\gvehicl.h</c>; 0x948 bytes.</remarks>
class GroundVehicle : public Mover
{
public:
    /// <summary>Runs <see cref="init()"/> after the bases' (inlined into <c>GroundVehicleType::createInstance</c>).</summary>
    GroundVehicle() { init(); }
    /// <remarks>MCX.EXE @ 0x0066a3f0 (vector deleting destructor)</remarks>
    ~GroundVehicle() override { destroy(); }

    /// <summary>
    /// Class GROUND_VEHICLE; five armor locations; movement and turret working; no status window, smoke, pilot;
    /// the mine cell -1.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0066a5b0</remarks>
    void init() override;
    /// <summary>
    /// Copies the type's data (internal structure, chassis, crash avoidance, refit/sweeper/layer/carrier flags,
    /// seats), makes the dynamics, and the appearance: a GVAppearance (turret) or a PUAppearance (pop-up turret).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0066a6b0</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Frees the crew name; closes the status window.</summary>
    /// <remarks>MCX.EXE @ 0x0066b8b0</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x0066df10</remarks>
    int32_t update() override;
    /// <remarks>MCX.EXE @ 0x0066ea70</remarks>
    void render() override;
    /// <summary>The vehicle's position.</summary>
    /// <remarks>MCX.EXE @ 0x0066d650</remarks>
    vector_3d getPositionFromHS(uint32_t hotSpot) override;
    /// <summary>Collides with the gates, buildings and walls of its tile block while moving.</summary>
    /// <remarks>MCX.EXE @ 0x0066a460</remarks>
    void handleStaticCollision() override;
    /// <summary>
    /// Reads the vehicle profile ("GroundVehicleProfile"): crew, description, name, tonnage, status, icon, battle
    /// rating, engine, movement system, armor, inventory (other, weapons, ammo) and the armor per location.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0066ab80</remarks>
    int32_t init(FitIniFile* vehicleFile) override;
    /// <remarks>MCX.EXE @ 0x0066d670</remarks>
    int onScreen() override;
    /// <remarks>MCX.EXE @ 0x0066f7e0</remarks>
    int32_t calcHitLocation(GameObject* attacker, int32_t weaponIndex, int32_t attackSource,
                            int32_t attackType) override;
    /// <remarks>MCX.EXE @ 0x006700d0</remarks>
    int32_t handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <summary>Replaces the control (1 player, 2 AI, 3 network) and gives it GroundVehicleControlData.</summary>
    /// <remarks>MCX.EXE @ 0x0066a970</remarks>
    int32_t setControl(uint32_t controlType, uint32_t controlData, int32_t controlParam) override;
    /// <remarks>MCX.EXE @ 0x0066f730</remarks>
    float relFacingTo(vector_3d goal, int32_t bodyPart) override;
    /// <summary>relFacingTo the goal, from the turret.</summary>
    /// <remarks>MCX.EXE @ 0x0066a2e0 (inline in <c>object\gvehicl.h</c>)</remarks>
    float relViewFacingTo(vector_3d goal) override;
    /// <remarks>MCX.EXE @ 0x006723d0</remarks>
    int32_t openStatusWindow(int32_t left, int32_t top, int32_t right, int32_t bottom) override;
    /// <remarks>MCX.EXE @ 0x006724b0</remarks>
    int32_t closeStatusWindow() override;
    /// <summary>Flagged captureable (or unknown6C set), and neither disabled nor destroyed.</summary>
    /// <remarks>MCX.EXE @ 0x0066a340 (inline in <c>object\gvehicl.h</c>)</remarks>
    int isCaptureable() override;
    /// <summary>The refit points left, for a refitter; 0 otherwise.</summary>
    /// <remarks>MCX.EXE @ 0x0066a390 (inline in <c>object\gvehicl.h</c>)</remarks>
    float getRefitPoints() override;
    /// <summary>Takes the points from a refitter that has enough.</summary>
    /// <remarks>MCX.EXE @ 0x0066a3b0 (inline in <c>object\gvehicl.h</c>)</remarks>
    int burnRefitPoints(float pointsToBurn) override;
    /// <remarks>MCX.EXE @ 0x006724e0</remarks>
    int32_t getVitalInfo(void* vitalInfo) override;
    /// <summary>From the dynamics.</summary>
    /// <remarks>MCX.EXE @ 0x0066c4e0</remarks>
    int32_t getSpeedState() override;
    /// <remarks>MCX.EXE @ 0x0066d890</remarks>
    int crashAvoidanceSystem() override;
    /// <summary>Sets off (or, for a sweeper, clears) the mines of the vehicle's cell.</summary>
    /// <remarks>MCX.EXE @ 0x0066b8f0</remarks>
    void mineCheck() override;
    /// <remarks>MCX.EXE @ 0x0066cf10</remarks>
    void updateMovement() override;
    /// <remarks>MCX.EXE @ 0x0066fb60</remarks>
    int32_t buildStatusChunk() override;
    /// <remarks>MCX.EXE @ 0x0066fd90</remarks>
    int32_t handleStatusChunk(int32_t updateAge, uint32_t chunk) override;
    /// <remarks>MCX.EXE @ 0x0066fea0</remarks>
    int32_t buildMoveChunk() override;
    /// <remarks>MCX.EXE @ 0x0066ff80</remarks>
    int32_t handleMoveChunk(uint32_t chunk) override;
    /// <summary>The profile's battle rating, or one computed from the loadout: the weapons' ratings scaled by top
    /// speed, plus structure, armor, tonnage class, the speed class and the other equipment.</summary>
    /// <remarks>MCX.EXE @ 0x0066b710</remarks>
    int32_t calcCV(int calcMax) override;
    /// <summary>The appearance's gesture (+0x74).</summary>
    /// <remarks>MCX.EXE @ 0x0066a310 (inline in <c>object\gvehicl.h</c>)</remarks>
    int32_t getBodyState() override;
    /// <summary>Product of the armor left per location (scaled 0.4..1.0), the pilot's wound factor and the weapon
    /// effectiveness ratio; 0 when disabled or destroyed.</summary>
    /// <remarks>MCX.EXE @ 0x00672520 (unnamed in Ghidra)</remarks>
    float getTotalEffectiveness() override;
    /// <summary>relFacingTo the target position, from the turret.</summary>
    /// <remarks>MCX.EXE @ 0x006700a0</remarks>
    float weaponLocked(int32_t weaponIndex, vector_3d targetPosition) override;
    /// <remarks>MCX.EXE @ 0x0066f790</remarks>
    float calcAttackChance(GameObject* target, int32_t aimLocation, float targetTime, int32_t weaponIndex,
                           float modifiers, int32_t* range, vector_3d* targetPoint) override;
    /// <summary>Fatal: vehicles have no inventory hits.</summary>
    /// <remarks>MCX.EXE @ 0x0066f8e0</remarks>
    int hitInventoryItem(int32_t itemIndex, int setupOnly) override;
    /// <summary>Disables the vehicle and starts its smoke.</summary>
    /// <remarks>MCX.EXE @ 0x0066d830</remarks>
    void disable(uint32_t cause) override;
    /// <summary>Does nothing.</summary>
    /// <remarks>MCX.EXE @ 0x0066f900</remarks>
    void destroyBodyLocation(int32_t location) override;
    /// <remarks>MCX.EXE @ 0x0066fad0</remarks>
    int injureBodyLocation(int32_t bodyLocation, float damage) override;
    /// <remarks>MCX.EXE @ 0x006703a0</remarks>
    int32_t fireWeapon(GameObject* target, float targetTime, int32_t weaponIndex, int32_t attackType,
                       int32_t aimLocation, vector_3d* targetPoint) override;
    /// <remarks>MCX.EXE @ 0x006719a0</remarks>
    int32_t handleWeaponFire(int32_t weaponIndex, GameObject* target, vector_3d* targetPoint, int hit, float entryAngle,
                             int32_t numMissiles, int32_t missilesPastAMS, int32_t antiMissileShots,
                             int32_t hitLocation) override;
    /// <summary>Whether the vehicle can move (movementEnabled).</summary>
    /// <remarks>MCX.EXE @ 0x0066a320 (inline in <c>object\gvehicl.h</c>)</remarks>
    int canMove() override;
    /// <summary>The long name (Mover::debugStatus).</summary>
    /// <remarks>MCX.EXE @ 0x0066a380 (inline in <c>object\gvehicl.h</c>)</remarks>
    const char* getIfaceName() override { return debugStatus; }
    // Slots 219.. are GroundVehicle's own.
    /// <summary>Does nothing: vehicles make no piloting checks.</summary>
    /// <remarks>MCX.EXE @ 0x0066b8a0</remarks>
    virtual void pilotingCheck() {}
    /// <remarks>MCX.EXE @ 0x0066d470</remarks>
    virtual void netUpdateMovement();
    /// <summary>Rolls a critical hit on the vehicle table; may disable movement or the turret.</summary>
    /// <remarks>MCX.EXE @ 0x0066f910</remarks>
    virtual int calcCriticalHitV(int32_t& hitLocation);
    /// <summary>The control data's throttle.</summary>
    /// <remarks>MCX.EXE @ 0x0066a330 (inline in <c>object\gvehicl.h</c>)</remarks>
    virtual int32_t getThrottle();
    using Mover::init;
    using Mover::pilotingCheck;

    /// <remarks>MCX.EXE @ 0x0066bd90</remarks>
    int pivotTo();
    /// <summary>Scales the throttle limits by the chassis' factors for the cell's terrain and overlay.</summary>
    /// <remarks>MCX.EXE @ 0x0066c3f0</remarks>
    void calcThrottleLimits(int32_t& minThrottle, int32_t& maxThrottle);
    /// <remarks>MCX.EXE @ 0x0066c500</remarks>
    void updateMoveStateGoal();
    /// <remarks>MCX.EXE @ 0x0066c6f0</remarks>
    int updateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec, int32_t& newMoveState,
                       int32_t& minThrottle, int32_t& maxThrottle);
    /// <remarks>MCX.EXE @ 0x0066cca0</remarks>
    void setNextMovePath(char& newThrottleSetting);
    /// <remarks>MCX.EXE @ 0x0066ccf0</remarks>
    void setControlSettings(char& newRotate, char& newThrottleSetting, float& newRotatePerSec, int32_t& minThrottle,
                            int32_t& maxThrottle);
    /// <remarks>MCX.EXE @ 0x0066cdb0</remarks>
    void updateTurret(float newRotatePerSec);
    /// <remarks>MCX.EXE @ 0x0066d080</remarks>
    int netUpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec, int32_t& newMoveState,
                          int32_t& minThrottle, int32_t& maxThrottle);
    /// <summary>
    /// Makes the marine who bails out of the dead vehicle (DefaultPilotId, the marine profile), hands him the
    /// vehicle's warrior and sends him off.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0066dc40</remarks>
    void createVehiclePilot();

    /// <summary>Not accessed.</summary>
    int32_t unknown8A0 = 0; // +0x8a0
    /// <summary>1 by init; a critical hit clears it. canMove returns it.</summary>
    int32_t movementEnabled = 1; // +0x8a4
    /// <summary>1 by init; a critical hit clears it.</summary>
    int32_t turretEnabled = 1; // +0x8a8
    /// <summary>The turret's yaw relative to the body (updateTurret; relFacingTo adds it).</summary>
    float turretRotation = 0.0f; // +0x8ac
    /// <summary>A pending request flag: zeroed by init, cleared by the AI and network controls' update; never
    /// set in this file.</summary>
    int32_t unknown8B0 = 0; // +0x8b0
    /// <summary>Whether the weapons can fire: always for a turret vehicle; a pop-up turret once it is up.</summary>
    int32_t weaponsDeployed = 1; // +0x8b4
    /// <summary>1 with a GVAppearance (a vehicle), 0 with a PUAppearance (a pop-up turret).</summary>
    int32_t gvAppearance = 0; // +0x8b8
    /// <summary>The status window.</summary>
    aTitleWindow* statusWindow = nullptr; // +0x8bc
    /// <summary>The smoke of a disabled or destroyed vehicle.</summary>
    Smoke* smoke = nullptr; // +0x8c0
    /// <summary>Whether the vehicle can be captured.</summary>
    int32_t captureable = 0; // +0x8c4
    /// <summary>Set when the type has refit points (the armor slot's refit pool at +0x20 of the armor block).</summary>
    int32_t refitter = 0; // +0x8c8
    /// <summary>While stopped, update plays gesture 3 instead of 0 when set. Zeroed by init.</summary>
    int32_t unknown8CC = 0; // +0x8cc
    /// <summary>The type's "MineSweeper".</summary>
    int32_t mineSweeper = 0; // +0x8d0
    /// <summary>Seconds since the sweeper last cleared a mine; -1 when it hasn't.</summary>
    float sweepTime = -1.0f; // +0x8d4
    /// <summary>Set when the type lays mines.</summary>
    int32_t mineLayer = 0; // +0x8d8
    /// <summary>The type's "MinesToLay".</summary>
    int32_t minesToLay = 0; // +0x8dc
    /// <summary>The tile column the layer last mined; -1 for none.</summary>
    int32_t cellColToMine = -1; // +0x8e0
    /// <summary>The tile row the layer last mined; -1 for none.</summary>
    int32_t cellRowToMine = -1; // +0x8e4
    /// <summary>The type's "ElementalCarrier".</summary>
    int32_t elementalCarrier = 0; // +0x8e8
    /// <summary>The elementals an elemental carrier holds (TacticalOrder LOAD_INTO_CARRIER fills it, DEPLOY_ELEMENTALS
    /// empties it).</summary>
    Mover* elementals[10] = {}; // +0x8ec
    /// <summary>The pilots riding in the seats (<see cref="seats"/>): taken from captured prisons, and put into
    /// captured mechs (TacticalOrder::status, CAPTURE).</summary>
    MechWarrior* passengers[4] = {}; // +0x914
    /// <summary>The type's "Seats".</summary>
    uint8_t seats = 0; // +0x924
    /// <summary>Profile "Crew" (systemHeap).</summary>
    char* crewName = nullptr; // +0x928
    /// <summary>Profile "NotMineYet" (1 when missing).</summary>
    int32_t notMineYet = 0; // +0x92c
    /// <summary>The marine who bailed out (createVehiclePilot).</summary>
    Mover* vehiclePilot = nullptr; // +0x930
    /// <summary>Set once mineCheck has handled the current cell's mine; cleared when the vehicle is on a cell
    /// without one.</summary>
    int32_t mineCellHandled = 0; // +0x934
    /// <summary>Profile "BattleRating"; -1 when missing (calcCV computes one).</summary>
    int32_t battleRating = -1; // +0x938
    /// <summary>Profile "DescIndex" (-1 when missing): the long name is string 700 + this.</summary>
    int32_t descIndex = -1; // +0x93c
    /// <summary>Profile "NameIndex".</summary>
    int32_t nameIndex = 0; // +0x940
    /// <summary>Seconds the mine layer has spent on the current cell (lays when over MineWaitTime).</summary>
    float mineLayTime = 0.0f; // +0x944
};

/// <summary>A ground vehicle's status window.</summary>
/// <remarks>Original source: <c>object\gvehicl.cpp</c>, <c>object\gvehicl.h</c>; 0x4c4 bytes.</remarks>
class GroundVehicleStatusWindow : public aTitleWindow
{
public:
    /// <remarks>MCX.EXE @ 0x00672480 (vector deleting destructor)</remarks>
    ~GroundVehicleStatusWindow() override;
    /// <remarks>MCX.EXE @ 0x006726a0</remarks>
    void init(int32_t x, int32_t y, int32_t w, int32_t h, GroundVehicle* newVehicle);
    /// <remarks>MCX.EXE @ 0x006726f0</remarks>
    void handleEvent(aEvent* event) override;
    /// <remarks>MCX.EXE @ 0x00672720</remarks>
    void resize(int32_t w, int32_t h) override;
    /// <remarks>MCX.EXE @ 0x00672740</remarks>
    void display() override;
    /// <remarks>MCX.EXE @ 0x006727e0</remarks>
    void draw() override;
    /// <remarks>MCX.EXE @ 0x00672470 (inline in <c>object\gvehicl.h</c>)</remarks>
    virtual GroundVehicle* getVehicle() { return vehicle; }

    /// <summary>The vehicle shown.</summary>
    GroundVehicle* vehicle = nullptr; // +0x4c0
};
