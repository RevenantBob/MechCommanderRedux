#pragma once

#include "gui/awindow.h"
#include "object/mover.h"
#include "object/objtype.h"

class DynamicsType;
class File;
class FitIniFile;
class Smoke;

/// <summary>A mech's body locations, in the mech file's order.</summary>
enum MechBodyLocation : int32_t
{
    MECH_BODY_LOCATION_HEAD = 0,
    MECH_BODY_LOCATION_CTORSO = 1,
    MECH_BODY_LOCATION_LTORSO = 2,
    MECH_BODY_LOCATION_RTORSO = 3,
    MECH_BODY_LOCATION_LARM = 4,
    MECH_BODY_LOCATION_RARM = 5,
    MECH_BODY_LOCATION_LLEG = 6,
    MECH_BODY_LOCATION_RLEG = 7,
    NUM_MECH_BODY_LOCATIONS = 8,
};

/// <summary>Armor locations of a mech (the body locations plus the three rear torso ones).</summary>
constexpr int32_t NUM_MECH_ARMOR_LOCATIONS = 11;

/// <summary>Critical spaces of each body location.</summary>
/// <remarks>MCX.EXE @ 0x00790bdc</remarks>
extern int32_t NumLocationCriticalSpaces[NUM_MECH_BODY_LOCATIONS];
/// <summary>Percent chance a disabled enemy mech leaves salvage (100 by default; the scenario may set it).</summary>
/// <remarks>MCX.EXE @ 0x00790e3c</remarks>
extern int32_t MechSalvageChance;
/// <summary>"MoveMarginOfError" (5, 10), read by loadMoverGameSystem.</summary>
/// <remarks>MCX.EXE @ 0x00790e34</remarks>
extern float MoveMarginOfError[2];
/// <summary>Attack modifiers by rank and chassis; [r][1..4] are WeaponFireModifiers[7 + 4r..] (loadMoverGameSystem).</summary>
/// <remarks>MCX.EXE @ 0x00790c64</remarks>
extern float RankVersusChassisCombatModifier[4][5];
/// <summary>The body location a hit on each hit section lands on.</summary>
/// <remarks>MCX.EXE @ 0x00790bfc</remarks>
extern int32_t MechHitSectionTable[5];
/// <summary>Each armor location's body location (the rear torso ones map to their torso).</summary>
/// <remarks>MCX.EXE @ 0x00790e20</remarks>
extern char MechArmorToBodyLocation[12];
/// <summary>The two orthogonal neighbour directions of each path direction (crashAvoidanceSystem, diagonals).</summary>
/// <remarks>MCX.EXE @ 0x00790c10</remarks>
extern int32_t adjClippedCell[8][2];
/// <summary>"AttackerMoveModifier".</summary>
/// <remarks>MCX.EXE @ 0x00790d30</remarks>
extern int32_t AttackerMoveModifier[9];
/// <summary>"CriticalHitTable".</summary>
/// <remarks>MCX.EXE @ 0x00790d54</remarks>
extern char CriticalHitTable[4];
/// <summary>"TargetMoveModifierTable": (speed, modifier) pairs.</summary>
/// <remarks>MCX.EXE @ 0x00790d58</remarks>
extern int32_t TargetMoveModifierTable[5][2];
/// <summary>The tonnage bounds of the mech classes ("MaxLightMech", "MaxHeavyMech"; getMechClass).</summary>
/// <remarks>MCX.EXE @ 0x00790d80</remarks>
extern float MechClassWeights[5];
/// <summary>"HitLocationTable".</summary>
/// <remarks>MCX.EXE @ 0x00790d94</remarks>
extern char MechHitLocationTable[0x84];
/// <summary>"MechTransferHitTable": where a hit on a destroyed location goes.</summary>
/// <remarks>MCX.EXE @ 0x00790e18</remarks>
extern char MechTransferHitTable[8];
/// <summary>"PilotCheckConditions".</summary>
/// <remarks>MCX.EXE @ 0x00790e2c</remarks>
extern int32_t MechPilotCheckConditions[2];
/// <summary>"PilotCheckTerrainEffect", by terrain type.</summary>
/// <remarks>MCX.EXE @ 0x007de54c</remarks>
extern int32_t MechPilotCheckTerrainEffect[0x40];
/// <summary>"CrashAvoidSelf" of "Mech:Movement", the mech types' default.</summary>
/// <remarks>MCX.EXE @ 0x00790e40</remarks>
extern int32_t DefaultMechCrashAvoidSelf;
/// <remarks>MCX.EXE @ 0x00790e44</remarks>
extern int32_t DefaultMechCrashAvoidPath;
/// <remarks>MCX.EXE @ 0x00790e48</remarks>
extern int32_t DefaultMechCrashBlockSelf;
/// <remarks>MCX.EXE @ 0x00790e4c</remarks>
extern int32_t DefaultMechCrashBlockPath;
/// <remarks>MCX.EXE @ 0x00790e50</remarks>
extern float DefaultMechCrashYieldTime;
/// <summary>Jump offsets (getJumpRange) by jump jets fitted, the last for six or more (the name is the port's).</summary>
/// <remarks>MCX.EXE @ 0x00790e58</remarks>
extern int32_t MechJumpOffsets[7];
/// <summary>"JumpCost".</summary>
/// <remarks>MCX.EXE @ 0x00790e54</remarks>
extern int32_t DefaultMechJumpCost;
/// <summary>"collisionThreshold": a slower mech bouncing off an object stops.</summary>
/// <remarks>MCX.EXE @ 0x007de64c</remarks>
extern float mechCollisionThreshold;
/// <summary>"objectThreshold".</summary>
/// <remarks>MCX.EXE @ 0x007de650</remarks>
extern float objectCollisionThreshold;
/// <summary>"tonnageThreshold": mechs under it are deflected by trees.</summary>
/// <remarks>MCX.EXE @ 0x007de654</remarks>
extern float tonnageCollisionThreshold;
/// <summary>"treeDeflection", in degrees at the threshold tonnage.</summary>
/// <remarks>MCX.EXE @ 0x007de658</remarks>
extern float treeDeflection;
/// <summary>"pivotAngle".</summary>
/// <remarks>MCX.EXE @ 0x007de65c</remarks>
extern float mechPivotAngle;
/// <summary>"pivotThrottle".</summary>
/// <remarks>MCX.EXE @ 0x007de660</remarks>
extern float mechPivotThrottle;
/// <remarks>MCX.EXE @ 0x007de668</remarks>
extern GameObject* BadGuy;
/// <summary>Speed state by gesture.</summary>
/// <remarks>MCX.EXE @ 0x0078d760</remarks>
extern char mechSpeedStateArray[32];
/// <summary>Body state by gesture.</summary>
/// <remarks>MCX.EXE @ 0x00790bc0</remarks>
extern char MechStateByGesture[28];
/// <summary>Whether mechs leave footprints (1).</summary>
/// <remarks>MCX.EXE @ 0x007a1c44</remarks>
extern uint8_t footPrints;
/// <remarks>MCX.EXE @ 0x00808bc0</remarks>
extern float MineSplashRange;
/// <remarks>MCX.EXE @ 0x00808c8c</remarks>
extern float MineSplashDamage;
/// <remarks>MCX.EXE @ 0x00808c98</remarks>
extern int32_t MineExplosion;
/// <summary>The "Mine" block's "BaseDamage": a mine's hit on the mech stepping on it (the name is the port's).</summary>
/// <remarks>MCX.EXE @ 0x00808fd8</remarks>
extern float MineBaseDamage;

/// <summary>
/// Reads the "Mech:Class", "Mech:Movement", "Mech:FireWeapon", "Mech:Damage" and "Mech:Collision" blocks of the
/// game system file.
/// </summary>
/// <remarks>MCX.EXE @ 0x00674ac0</remarks>
int32_t loadMechGameSystem(FitIniFile* sysFile);

/// <summary>A mech type: the mech file's header, internal structure, debris, dynamics and movement settings.</summary>
/// <remarks>Original source: <c>object\mech.cpp</c>; 0xac bytes.</remarks>
class BattleMechType : public ObjectType
{
public:
    /// <remarks>MCX.EXE @ 0x006903a0 (vector deleting destructor)</remarks>
    ~BattleMechType() override { destroy(); }

    /// <summary>Clears the fields; debris pieces -1; crash avoidance from the defaults.</summary>
    /// <remarks>MCX.EXE @ 0x00674db0</remarks>
    void init();
    /// <summary>
    /// Reads the mech file: "General" (id, type, name, chassis, tonnage, explosion, endo steel, internal structure
    /// tonnage), "InternalStructure", "Debris", "Dynamics", "MovementSystem", hot spots, then the common type data.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00674e50</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <summary>Frees the name and the dynamics type.</summary>
    /// <remarks>MCX.EXE @ 0x00675330</remarks>
    void destroy() override;
    /// <summary>Makes a <see cref="BattleMech"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x00676820</remarks>
    BaseObject* createInstance() override;
    /// <remarks>MCX.EXE @ 0x00675380</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <remarks>MCX.EXE @ 0x00675f80</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;
    /// <summary>Reads the appearance's hot spots (weapon mounts, jump jets).</summary>
    /// <remarks>MCX.EXE @ 0x006760a0</remarks>
    int32_t loadHotSpots(FitIniFile* mechFile);
    void layOutHotSpotPackets(const std::vector<uint32_t>& packetSizes, const std::vector<uint32_t>& outlineSizes);

    /// <summary>"ID".</summary>
    uint32_t mechId = 0; // +0x30
    /// <summary>"Name" (systemHeap).</summary>
    char* name = nullptr; // +0x34
    /// <summary>"Type", mapped.</summary>
    uint8_t mechType = 0; // +0x38
    /// <summary>"Chassis".</summary>
    uint8_t chassis = 0; // +0x39
    /// <summary>"TonnageClass".</summary>
    float tonnageClass = 0.0f; // +0x3c
    /// <summary>"EndoSteel".</summary>
    uint32_t endoSteel = 0; // +0x40
    /// <summary>"InternalStructureTonnage".</summary>
    float internalStructureTonnage = 0.0f; // +0x44
    /// <summary>"InternalStructure" per body location.</summary>
    uint8_t internalStructure[NUM_MECH_BODY_LOCATIONS] = {}; // +0x48
    int32_t unknown50 = 0;                                   // +0x50
    int32_t unknown54 = 0;                                   // +0x54
    /// <summary>The dynamics type ("Dynamics" block, type 1).</summary>
    DynamicsType* dynamicsType = nullptr; // +0x58
    /// <summary>The hot spot file's last packet: 32 bytes per hot spot packet (objectTypeCache).</summary>
    uint8_t* hotSpotData = nullptr; // +0x5c
    /// <summary>"numHotSpotPackets" of the .inf file: one per gesture.</summary>
    uint32_t numHotSpotPackets = 0; // +0x60
    /// <summary>"numWeapons".</summary>
    uint32_t numWeapons = 0; // +0x64
    /// <summary>"numOthers".</summary>
    uint32_t numOthers = 0; // +0x68
    /// <summary>"numFramesPerHotSpot" per gesture.</summary>
    uint32_t* numFramesPerHotSpot = nullptr; // +0x6c
    /// <summary>The mech file's "weapon%d" hot spot entries.</summary>
    uint32_t* weaponHotSpots = nullptr; // +0x70
    /// <summary>The hot spot file's packet per gesture.</summary>
    uint8_t** gestureHotSpots = nullptr; // +0x74
    /// <summary>The .jmp file.</summary>
    uint8_t* jumpData = nullptr; // +0x78
    /// <summary>The .out file's packet per gesture (null where empty).</summary>
    uint8_t** gestureOutlines = nullptr; // +0x7c
    /// <summary>"FootprintType".</summary>
    int32_t footprintType = 1; // +0x80
    /// <summary>"RightArmPiece": the debris type of the right arm, -1 for none.</summary>
    uint32_t rightArmDebrisId = 0xffffffff; // +0x84
    /// <summary>"LeftArmPiece".</summary>
    uint32_t leftArmDebrisId = 0xffffffff; // +0x88
    /// <summary>"DestroyedPiece".</summary>
    uint32_t destroyedPiece = 0xffffffff; // +0x8c
    /// <summary>"CrashAvoidSelf".</summary>
    int32_t crashAvoidSelf = 0; // +0x90
    /// <summary>"CrashAvoidPath".</summary>
    int32_t crashAvoidPath = 0; // +0x94
    /// <summary>"CrashBlockSelf".</summary>
    int32_t crashBlockSelf = 0; // +0x98
    /// <summary>"CrashBlockPath".</summary>
    int32_t crashBlockPath = 0; // +0x9c
    /// <summary>"CrashYieldTime".</summary>
    float crashYieldTime = 0.0f; // +0xa0
    /// <summary>"ExplosionDamage".</summary>
    float explDmg = 0.0f; // +0xa4
    /// <summary>"ExplosionRadius".</summary>
    float explRad = 0.0f; // +0xa8

    /// <summary>
    /// Port: the hot spots each gesture's packet actually holds (packet size / (numFramesPerHotSpot * 12)). The
    /// Commando's (cm.hsp) gestures 0-14 hold 3, not numWeapons + numOthers = 6.
    /// </summary>
    std::vector<std::vector<float>> hotSpotPackets;
    std::vector<uint32_t> hotSpotPacketShippedFloats;
};

/// <summary>A BattleMech: legs and torso, arms, jump jets, heat, and the mech's movement and combat.</summary>
/// <remarks>Original source: <c>object\mech.cpp</c>, <c>object\mech.h</c>; 0x958 bytes.</remarks>
class BattleMech : public Mover
{
public:
    /// <summary>The constructor calls init (inlined in BattleMechType::createInstance).</summary>
    BattleMech() { init(); }
    /// <remarks>MCX.EXE @ 0x00676970 (vector deleting destructor)</remarks>
    ~BattleMech() override { destroy(); }

    /// <summary>
    /// Class BATTLEMECH; eight body locations and eleven armor locations; legs and torso intact; no jump; torso and
    /// arms straight.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00676b60 (unnamed in Ghidra)</remarks>
    void init() override;
    /// <remarks>MCX.EXE @ 0x00676c70</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Frees the interface name; closes the status window.</summary>
    /// <remarks>MCX.EXE @ 0x00678830</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x0067b780</remarks>
    int32_t update() override;
    /// <remarks>MCX.EXE @ 0x0067d6b0</remarks>
    void render() override;
    /// <remarks>MCX.EXE @ 0x0067ad50</remarks>
    vector_3d getPositionFromHS(uint32_t hotSpot) override;
    /// <remarks>MCX.EXE @ 0x006769e0</remarks>
    void handleStaticCollision() override;
    /// <remarks>MCX.EXE @ 0x006770c0</remarks>
    int32_t init(FitIniFile* mechFile) override;
    /// <remarks>MCX.EXE @ 0x0067afd0</remarks>
    int onScreen() override;
    /// <remarks>MCX.EXE @ 0x0067e450</remarks>
    int32_t calcHitLocation(GameObject* attacker, int32_t weaponIndex, int32_t attackSource,
                            int32_t attackType) override;
    /// <remarks>MCX.EXE @ 0x0067f940</remarks>
    int32_t handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <remarks>MCX.EXE @ 0x00676eb0</remarks>
    int32_t setControl(uint32_t controlType, uint32_t controlData, int32_t controlParam) override;
    /// <summary>Whether a leg is gone (leg status 2 or 3).</summary>
    /// <remarks>MCX.EXE @ 0x006768c0</remarks>
    int isCrippled() override;
    /// <remarks>MCX.EXE @ 0x00678230</remarks>
    int32_t write(File* objFile) override;
    /// <remarks>MCX.EXE @ 0x0067e2b0</remarks>
    float relFacingTo(vector_3d goal, int32_t bodyPart) override;
    /// <remarks>MCX.EXE @ 0x00676910</remarks>
    float relViewFacingTo(vector_3d goal) override;
    /// <remarks>MCX.EXE @ 0x00682940</remarks>
    int32_t openStatusWindow(int32_t left, int32_t top, int32_t right, int32_t bottom) override;
    /// <remarks>MCX.EXE @ 0x00682a20</remarks>
    int32_t closeStatusWindow() override;
    /// <summary>A home team mech flagged captureable and not destroyed.</summary>
    /// <remarks>MCX.EXE @ 0x00682a80</remarks>
    int isCaptureable() override;
    /// <remarks>MCX.EXE @ 0x00682a50</remarks>
    int32_t getVitalInfo(void* vitalInfo) override;
    /// <summary>From the appearance's gesture (mechSpeedStateArray).</summary>
    /// <remarks>MCX.EXE @ 0x006794d0</remarks>
    int32_t getSpeedState() override;
    /// <remarks>MCX.EXE @ 0x00678660</remarks>
    void pilotingCheck(uint32_t situation, float modifier) override;
    /// <remarks>MCX.EXE @ 0x0067b3e0</remarks>
    int crashAvoidanceSystem() override;
    /// <remarks>MCX.EXE @ 0x00678870</remarks>
    void mineCheck() override;
    /// <remarks>MCX.EXE @ 0x0067a190</remarks>
    void updateMovement() override;
    /// <remarks>MCX.EXE @ 0x0067f0d0</remarks>
    int32_t updateCriticalHitChunks(int32_t which) override;
    /// <remarks>MCX.EXE @ 0x0067f150</remarks>
    int32_t buildStatusChunk() override;
    /// <remarks>MCX.EXE @ 0x0067f430</remarks>
    int32_t handleStatusChunk(int32_t updateAge, uint32_t chunk) override;
    /// <remarks>MCX.EXE @ 0x0067f540 (unnamed in Ghidra)</remarks>
    int32_t buildMoveChunk() override;
    /// <remarks>MCX.EXE @ 0x0067f6f0</remarks>
    int32_t handleMoveChunk(uint32_t chunk) override;
    /// <remarks>MCX.EXE @ 0x00678530</remarks>
    int32_t calcCV(int calcMax) override;
    /// <summary>From the appearance's gesture (MechStateByGesture).</summary>
    /// <remarks>MCX.EXE @ 0x0067e360</remarks>
    int32_t getBodyState() override;
    /// <remarks>MCX.EXE @ 0x00682e50</remarks>
    float getTotalEffectiveness() override;
    /// <remarks>MCX.EXE @ 0x0067f900</remarks>
    float weaponLocked(int32_t weaponIndex, vector_3d targetPosition) override;
    /// <remarks>MCX.EXE @ 0x0067e380</remarks>
    int isWeaponReady(int32_t weaponIndex) override;
    /// <remarks>MCX.EXE @ 0x00682e10</remarks>
    int isWeaponWorking(int32_t weaponIndex) override;
    /// <remarks>MCX.EXE @ 0x0067e3d0</remarks>
    float calcAttackChance(GameObject* target, int32_t aimLocation, float targetTime, int32_t weaponIndex,
                           float modifiers, int32_t* range, vector_3d* targetPoint) override;
    /// <remarks>MCX.EXE @ 0x0067e800</remarks>
    int hitInventoryItem(int32_t itemIndex, int setupOnly) override;
    /// <remarks>MCX.EXE @ 0x0067ed10</remarks>
    void destroyBodyLocation(int32_t location) override;
    /// <remarks>MCX.EXE @ 0x0067ee80</remarks>
    void calcCriticalHit(int32_t hitLocation) override;
    /// <remarks>MCX.EXE @ 0x0067f810</remarks>
    int injureBodyLocation(int32_t bodyLocation, float damage) override;
    /// <remarks>MCX.EXE @ 0x0067ff00</remarks>
    int32_t fireWeapon(GameObject* target, float targetTime, int32_t weaponIndex, int32_t attackType,
                       int32_t aimLocation, vector_3d* targetPoint) override;
    /// <remarks>MCX.EXE @ 0x00681970</remarks>
    int32_t handleWeaponFire(int32_t weaponIndex, GameObject* target, vector_3d* targetPoint, int hit, float entryAngle,
                             int32_t numMissiles, int32_t missilesPastAMS, int32_t antiMissileShots,
                             int32_t hitLocation) override;
    /// <remarks>MCX.EXE @ 0x00678820</remarks>
    int canPowerUp() override;
    /// <summary>Unless both legs are gone (leg status 3).</summary>
    /// <remarks>MCX.EXE @ 0x00676940</remarks>
    int canMove() override;
    /// <summary>Whether the mech has jump jets.</summary>
    /// <remarks>MCX.EXE @ 0x00676950</remarks>
    int canJump() override;
    /// <remarks>MCX.EXE @ 0x0067e620</remarks>
    float getJumpRange(int32_t* numOffsets, int32_t* jumpCost) override;
    /// <remarks>MCX.EXE @ 0x0067e5f0</remarks>
    int isJumping(vector_3d* jumpGoal) override;
    /// <remarks>MCX.EXE @ 0x00682770</remarks>
    float calcMaxSpeed() override;
    /// <remarks>MCX.EXE @ 0x006827c0</remarks>
    float calcSlowSpeed() override;
    /// <remarks>MCX.EXE @ 0x006827f0</remarks>
    float calcModerateSpeed() override;
    /// <remarks>MCX.EXE @ 0x00682820</remarks>
    int32_t calcSpriteSpeed(float speed, uint32_t flags, int32_t& state, int32_t& throttle) override;
    /// <remarks>MCX.EXE @ 0x0067e6a0</remarks>
    int handleEjection() override;
    /// <remarks>MCX.EXE @ 0x00676960</remarks>
    const char* getIfaceName() override { return ifaceName; }

    // Slots 219.. are BattleMech's own.

    /// <remarks>MCX.EXE @ 0x00678220</remarks>
    virtual int32_t init(File* objFile) { return 0; }
    /// <remarks>MCX.EXE @ 0x006768e0</remarks>
    virtual float getWeaponHeat(int32_t weaponIndex);
    /// <remarks>MCX.EXE @ 0x0067a990</remarks>
    virtual void netUpdateMovement();
    /// <remarks>MCX.EXE @ 0x0067f080</remarks>
    virtual void handleCriticalHit(int32_t bodyLocation, int32_t criticalSpace);
    /// <remarks>MCX.EXE @ 0x0067e690</remarks>
    virtual int handleFall(int forward) { return 0; }
    /// <remarks>MCX.EXE @ 0x00682ac0</remarks>
    virtual float calcMaxTargetDamage();
    /// <remarks>MCX.EXE @ 0x00682bd0</remarks>
    virtual float calcExpectedTargetDamage(GameObject* target);

    using Mover::calcExpectedTargetDamage;
    using Mover::init;

    /// <summary>Sets and returns the leg status from the legs' damage (alarms / radio on change).</summary>
    /// <remarks>MCX.EXE @ 0x006785b0</remarks>
    int32_t calcLegStatus();
    /// <remarks>MCX.EXE @ 0x00678630</remarks>
    int32_t calcTorsoStatus();
    /// <remarks>MCX.EXE @ 0x00678c60</remarks>
    int updateJump();
    /// <remarks>MCX.EXE @ 0x00678e70</remarks>
    int pivotTo();
    /// <remarks>MCX.EXE @ 0x006794f0</remarks>
    void updateMoveStateGoal();
    /// <summary>
    /// Steers along the pilot's move path: advances the step once within the margin of error, then sets the
    /// gesture, throttle and turn (or asks for a pivot through <paramref name="newMoveState"/>). Nonzero once the
    /// path is done. The last two parameters are unused.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006796f0</remarks>
    int updateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec, int32_t& newGestureStateGoal,
                       int32_t& newMoveState, int32_t& minThrottle, int32_t& maxThrottle);
    /// <remarks>MCX.EXE @ 0x00679e10</remarks>
    void setNextMovePath(char& newThrottleSetting, int32_t& newGestureStateGoal);
    /// <remarks>MCX.EXE @ 0x00679e60</remarks>
    void updateTorso(float newRotatePerSec);
    /// <remarks>MCX.EXE @ 0x00679f90</remarks>
    void setControlSettings(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                            int32_t& newGestureStateGoal, int32_t& minThrottle, int32_t& maxThrottle);
    /// <remarks>MCX.EXE @ 0x0067a440</remarks>
    int netUpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                          int32_t& newGestureStateGoal, int32_t& newMoveState, int32_t& minThrottle,
                          int32_t& maxThrottle);
    /// <remarks>MCX.EXE @ 0x0067b140</remarks>
    void createJumpFX();
    /// <remarks>MCX.EXE @ 0x0067b1b0</remarks>
    void endJumpFX();
    /// <summary>Where jump jet <paramref name="jet"/> (0 or 1) is this frame.</summary>
    /// <remarks>MCX.EXE @ 0x0067b200</remarks>
    vector_3d getJumpPosition(int32_t jet);
    /// <remarks>MCX.EXE @ 0x0067e580</remarks>
    int32_t transferHitLocation(int32_t hitLocation);
    /// <remarks>MCX.EXE @ 0x0067e5c0</remarks>
    int32_t startJump(vector_3d jumpGoal);
    /// <remarks>MCX.EXE @ 0x00683000</remarks>
    void damageLoadedComponents();

    /// <summary>The weight class (getMechClass; 1 by init), indexing RankVersusChassisCombatModifier.</summary>
    uint8_t mechClass = 1; // +0x8a0
    /// <summary>"ChassisBR" (100 when missing).</summary>
    int32_t chassisBR = 0; // +0x8a4
    /// <summary>0 intact, 2 a leg gone, 3 both legs gone (calcLegStatus).</summary>
    int8_t legStatus = 0; // +0x8a8
    /// <summary>calcTorsoStatus.</summary>
    int8_t torsoStatus = 0; // +0x8a9
    /// <summary>Inventory index of the left arm actuator.</summary>
    uint8_t leftArmActuator = 0; // +0x8aa
    /// <summary>Inventory index of the right arm actuator.</summary>
    uint8_t rightArmActuator = 0; // +0x8ab
    /// <summary>Inventory index of the left leg actuator.</summary>
    uint8_t leftLegActuator = 0; // +0x8ac
    /// <summary>Inventory index of the right leg actuator.</summary>
    uint8_t rightLegActuator = 0; // +0x8ad
    /// <summary>Inventory index of the gyro.</summary>
    uint8_t gyro = 0; // +0x8ae
    /// <summary>Jump jets fitted (canJump, getJumpRange).</summary>
    uint8_t numJumpJets = 0; // +0x8af
    /// <summary>-100 by init (updateJump).</summary>
    float jumpTime = -100.0f; // +0x8b0
    /// <summary>Set while jumping.</summary>
    int32_t inJump = 0; // +0x8b4
    /// <summary>Where the jump lands.</summary>
    vector_3d jumpGoal; // +0x8b8
    /// <summary>-1 by init (destroyBodyLocation, injureBodyLocation).</summary>
    float unknown8C4 = -1.0f; // +0x8c4
    int32_t unknown8C8 = 0;   // +0x8c8
    int32_t unknown8CC = 0;   // +0x8cc
    /// <summary>Pending flag the controls turn into MechControlData::unknown14.</summary>
    int32_t pendingControl8D0 = 0; // +0x8d0
    /// <summary>Pending flag the controls turn into MechControlData::unknown18.</summary>
    int32_t pendingControl8D4 = 0; // +0x8d4
    int32_t unknown8D8 = 0;        // +0x8d8
    int32_t unknown8DC = 0;        // +0x8dc
    /// <summary>Torso yaw in degrees, within the dynamics type's maxTorsoYaw.</summary>
    float torsoRotation = 0.0f; // +0x8e0
    /// <summary>Right arm yaw in degrees, within maxArmYaw.</summary>
    float rightArmRotation = 0.0f; // +0x8e4
    /// <summary>Left arm yaw in degrees, within maxArmYaw.</summary>
    float leftArmRotation = 0.0f; // +0x8e8
    int32_t unknown8EC = 0;       // +0x8ec
    int32_t unknown8F0 = 0;       // +0x8f0
    /// <summary>The status window.</summary>
    aTitleWindow* statusWindow = nullptr; // +0x8f4
    /// <summary>Used by updateJump.</summary>
    float unknown8F8 = 0.0f; // +0x8f8
    /// <summary>Smoke streaming from damaged equipment (hitInventoryItem).</summary>
    Smoke* smoke[4] = {}; // +0x8fc
    /// <summary>The hot spot each smoke streams from.</summary>
    int32_t smokeHotSpot[4] = {}; // +0x90c
    /// <summary>Seconds each smoke has left (15 at the start).</summary>
    float smokeTime[4] = {}; // +0x91c
    /// <summary>The jump jet effects.</summary>
    GameObject* jumpFX[2] = {}; // +0x92c
    /// <summary>calcMaxTargetDamage.</summary>
    float maxTargetDamage = 0.0f; // +0x934
    /// <summary>The name the interface shows (systemHeap).</summary>
    char* ifaceName = nullptr; // +0x938
    /// <summary>"Pilot" (-1 when missing).</summary>
    int32_t pilotId = 0; // +0x93c
    /// <summary>Whether the mech can be captured.</summary>
    int32_t captureable = 0; // +0x940
    /// <summary>"NotMineYet" (1 when missing).</summary>
    int notMineYet = 0; // +0x944
    /// <summary>Used by mineCheck.</summary>
    int32_t unknown948 = 0; // +0x948
    /// <summary>"DescIndex": the interface name is string 300 + it (-1 when missing).</summary>
    int32_t descIndex = 0; // +0x94c
    /// <summary>"NameIndex".</summary>
    int32_t nameIndex = 0; // +0x950
    /// <summary>"NameVariant".</summary>
    int32_t nameVariant = 0; // +0x954
};

/// <summary>A mech's status window.</summary>
/// <remarks>Original source: <c>object\mech.cpp</c>, <c>object\mech.h</c>; 0x4c4 bytes.</remarks>
class MechStatusWindow : public aTitleWindow
{
public:
    /// <remarks>MCX.EXE @ 0x006829f0 (vector deleting destructor)</remarks>
    ~MechStatusWindow() override;
    /// <remarks>MCX.EXE @ 0x00683070</remarks>
    void init(int32_t x, int32_t y, int32_t w, int32_t h, BattleMech* newMech);
    /// <remarks>MCX.EXE @ 0x006830c0</remarks>
    void handleEvent(aEvent* event) override;
    /// <remarks>MCX.EXE @ 0x006830f0</remarks>
    void resize(int32_t w, int32_t h) override;
    /// <remarks>MCX.EXE @ 0x00683110</remarks>
    void display() override;
    /// <remarks>MCX.EXE @ 0x00683720</remarks>
    void draw() override;
    /// <summary>Port: still paints a picture (in display), so it keeps one.</summary>
    bool DrawsLive() override { return false; }
    /// <remarks>MCX.EXE @ 0x006829e0</remarks>
    virtual BattleMech* getMech() { return mech; }

    /// <summary>The mech shown.</summary>
    BattleMech* mech = nullptr; // +0x4c0
};
