#pragma once

#include "gui/awindow.h"
#include "object/mover.h"
#include "object/objtype.h"

class MCDynamicsType;
class MCFile;
class MCFitIniFile;
class MCSmoke;

/// <summary>A mech's body locations, in the mech file's order.</summary>
enum MCMechBodyLocation : int32_t
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
extern int32_t NumLocationCriticalSpaces[NUM_MECH_BODY_LOCATIONS];
/// <summary>Percent chance a disabled enemy mech leaves salvage (100 by default; the scenario may set it).</summary>
extern int32_t MechSalvageChance;
/// <summary>"MoveMarginOfError" (5, 10), read by loadMoverGameSystem.</summary>
extern float MoveMarginOfError[2];
/// <summary>Attack modifiers by rank and chassis; [r][1..4] are WeaponFireModifiers[7 + 4r..] (loadMoverGameSystem).</summary>
extern float RankVersusChassisCombatModifier[4][5];
/// <summary>The body location a hit on each hit section lands on.</summary>
extern int32_t MechHitSectionTable[5];
/// <summary>Each armor location's body location (the rear torso ones map to their torso).</summary>
extern char MechArmorToBodyLocation[12];
/// <summary>The two orthogonal neighbour directions of each path direction (crashAvoidanceSystem, diagonals).</summary>
extern int32_t AdjClippedCell[8][2];
/// <summary>"AttackerMoveModifier".</summary>
extern int32_t AttackerMoveModifier[9];
/// <summary>"CriticalHitTable".</summary>
extern char CriticalHitTable[4];
/// <summary>"TargetMoveModifierTable": (speed, modifier) pairs.</summary>
extern int32_t TargetMoveModifierTable[5][2];
/// <summary>The tonnage bounds of the mech classes ("MaxLightMech", "MaxHeavyMech"; getMechClass).</summary>
extern float MechClassWeights[5];
/// <summary>"HitLocationTable".</summary>
extern char MechHitLocationTable[0x84];
/// <summary>"MechTransferHitTable": where a hit on a destroyed location goes.</summary>
extern char MechTransferHitTable[8];
/// <summary>"PilotCheckConditions".</summary>
extern int32_t MechPilotCheckConditions[2];
/// <summary>"PilotCheckTerrainEffect", by terrain type.</summary>
extern int32_t MechPilotCheckTerrainEffect[0x40];
/// <summary>"CrashAvoidSelf" of "Mech:Movement", the mech types' default.</summary>
extern int32_t DefaultMechCrashAvoidSelf;
extern int32_t DefaultMechCrashAvoidPath;
extern int32_t DefaultMechCrashBlockSelf;
extern int32_t DefaultMechCrashBlockPath;
extern float DefaultMechCrashYieldTime;
/// <summary>Jump offsets (getJumpRange) by jump jets fitted, the last for six or more (the name is the port's).</summary>
extern int32_t MechJumpOffsets[7];
/// <summary>"JumpCost".</summary>
extern int32_t DefaultMechJumpCost;
/// <summary>"collisionThreshold": a slower mech bouncing off an object stops.</summary>
extern float MechCollisionThreshold;
/// <summary>"objectThreshold".</summary>
extern float ObjectCollisionThreshold;
/// <summary>"tonnageThreshold": mechs under it are deflected by trees.</summary>
extern float TonnageCollisionThreshold;
/// <summary>"treeDeflection", in degrees at the threshold tonnage.</summary>
extern float TreeDeflection;
/// <summary>"pivotAngle".</summary>
extern float MechPivotAngle;
/// <summary>"pivotThrottle".</summary>
extern float MechPivotThrottle;
extern MCGameObject* BadGuy;
/// <summary>Speed state by gesture.</summary>
extern char MechSpeedStateArray[32];
/// <summary>Body state by gesture.</summary>
extern char MechStateByGesture[28];
/// <summary>Whether mechs leave footprints (1).</summary>
extern uint8_t FootPrints;
extern float MineSplashRange;
extern float MineSplashDamage;
extern int32_t MineExplosion;
/// <summary>The "Mine" block's "BaseDamage": a mine's hit on the mech stepping on it (the name is the port's).</summary>
extern float MineBaseDamage;

/// <summary>
/// Reads the "Mech:Class", "Mech:Movement", "Mech:FireWeapon", "Mech:Damage" and "Mech:Collision" blocks of the
/// game system file.
/// </summary>
int32_t LoadMechGameSystem(MCFitIniFile* sysFile);

/// <summary>A mech type: the mech file's header, internal structure, debris, dynamics and movement settings.</summary>
/// <remarks>Original source: <c>object\mech.cpp</c>; 0xac bytes.</remarks>
class MCBattleMechType : public MCObjectType
{
public:
    ~MCBattleMechType() override { Destroy(); }

    /// <summary>Clears the fields; debris pieces -1; crash avoidance from the defaults.</summary>
    void Init();
    /// <summary>
    /// Reads the mech file: "General" (id, type, name, chassis, tonnage, explosion, endo steel, internal structure
    /// tonnage), "InternalStructure", "Debris", "Dynamics", "MovementSystem", hot spots, then the common type data.
    /// </summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>Frees the name and the dynamics type.</summary>
    void Destroy() override;
    /// <summary>Makes a <see cref="MCBattleMech"/> of this type and gives it the next object id.</summary>
    MCBaseObject* CreateInstance() override;
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;
    /// <summary>Reads the appearance's hot spots (weapon mounts, jump jets).</summary>
    int32_t LoadHotSpots(MCFitIniFile* mechFile);
    void LayOutHotSpotPackets(const std::vector<uint32_t>& packetSizes, const std::vector<uint32_t>& outlineSizes);

    /// <summary>"ID".</summary>
    uint32_t MechId = 0;
    /// <summary>"Name".</summary>
    std::string Name;
    /// <summary>"Type", mapped.</summary>
    uint8_t MechType = 0;
    /// <summary>"Chassis".</summary>
    uint8_t Chassis = 0;
    /// <summary>"TonnageClass".</summary>
    float TonnageClass = 0.0f;
    /// <summary>"EndoSteel".</summary>
    uint32_t EndoSteel = 0;
    /// <summary>"InternalStructureTonnage".</summary>
    float InternalStructureTonnage = 0.0f;
    /// <summary>"InternalStructure" per body location.</summary>
    uint8_t InternalStructure[NUM_MECH_BODY_LOCATIONS] = {};
    /// <summary>The dynamics type ("Dynamics" block, type 1).</summary>
    MCDynamicsType* DynamicsType = nullptr;
    /// <summary>The hot spot file's last packet: 32 bytes per hot spot packet (objectTypeCache).</summary>
    uint8_t* HotSpotData = nullptr;
    /// <summary>"numHotSpotPackets" of the .inf file: one per gesture.</summary>
    uint32_t NumHotSpotPackets = 0;
    /// <summary>"numWeapons".</summary>
    uint32_t NumWeapons = 0;
    /// <summary>"numOthers".</summary>
    uint32_t NumOthers = 0;
    /// <summary>"numFramesPerHotSpot" per gesture.</summary>
    uint32_t* NumFramesPerHotSpot = nullptr;
    /// <summary>The mech file's "weapon%d" hot spot entries.</summary>
    uint32_t* WeaponHotSpots = nullptr;
    /// <summary>The hot spot file's packet per gesture.</summary>
    uint8_t** GestureHotSpots = nullptr;
    /// <summary>The .jmp file.</summary>
    uint8_t* JumpData = nullptr;
    /// <summary>The .out file's packet per gesture (null where empty).</summary>
    uint8_t** GestureOutlines = nullptr;
    /// <summary>"FootprintType".</summary>
    int32_t FootprintType = 1;
    /// <summary>"RightArmPiece": the debris type of the right arm, -1 for none.</summary>
    uint32_t RightArmDebrisId = 0xffffffff;
    /// <summary>"LeftArmPiece".</summary>
    uint32_t LeftArmDebrisId = 0xffffffff;
    /// <summary>"DestroyedPiece".</summary>
    uint32_t DestroyedPiece = 0xffffffff;
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
    /// <summary>"ExplosionDamage".</summary>
    float ExplDmg = 0.0f;
    /// <summary>"ExplosionRadius".</summary>
    float ExplRad = 0.0f;

    /// <summary>
    /// Port: the hot spots each gesture's packet actually holds (packet size / (numFramesPerHotSpot * 12)). The
    /// Commando's (cm.hsp) gestures 0-14 hold 3, not numWeapons + numOthers = 6.
    /// </summary>
    std::vector<std::vector<float>> HotSpotPackets;
    std::vector<uint32_t> HotSpotPacketShippedFloats;
};

/// <summary>A BattleMech: legs and torso, arms, jump jets, heat, and the mech's movement and combat.</summary>
/// <remarks>Original source: <c>object\mech.cpp</c>, <c>object\mech.h</c>; 0x958 bytes.</remarks>
class MCBattleMech : public MCMover
{
public:
    /// <summary>The constructor calls init (inlined in BattleMechType::createInstance).</summary>
    MCBattleMech() { Init(); }
    ~MCBattleMech() override { Destroy(); }

    /// <summary>
    /// Class BATTLEMECH; eight body locations and eleven armor locations; legs and torso intact; no jump; torso and
    /// arms straight.
    /// </summary>
    void Init() override;
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Frees the interface name; closes the status window.</summary>
    void Destroy() override;
    int32_t Update() override;
    void Render() override;
    MCVector3D GetPositionFromHS(uint32_t hotSpot) override;
    void HandleStaticCollision() override;
    int32_t Init(MCFitIniFile* mechFile) override;
    int OnScreen() override;
    int32_t CalcHitLocation(MCGameObject* attacker, int32_t weaponIndex, int32_t attackSource,
                            int32_t attackType) override;
    int32_t HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    int32_t SetControl(uint32_t controlType, uint32_t controlData, int32_t controlParam) override;
    /// <summary>Whether a leg is gone (leg status 2 or 3).</summary>
    int IsCrippled() override;
    int32_t Write(MCFile* objFile) override;
    float RelFacingTo(MCVector3D goal, int32_t bodyPart) override;
    float RelViewFacingTo(MCVector3D goal) override;
    int32_t OpenStatusWindow(int32_t left, int32_t top, int32_t right, int32_t bottom) override;
    int32_t CloseStatusWindow() override;
    /// <summary>A home team mech flagged captureable and not destroyed.</summary>
    int IsCaptureable() override;
    int32_t GetVitalInfo(void* vitalInfo) override;
    /// <summary>From the appearance's gesture (mechSpeedStateArray).</summary>
    int32_t GetSpeedState() override;
    void PilotingCheck(uint32_t situation, float modifier) override;
    int CrashAvoidanceSystem() override;
    void MineCheck() override;
    void UpdateMovement() override;
    int32_t UpdateCriticalHitChunks(int32_t which) override;
    int32_t BuildStatusChunk() override;
    int32_t HandleStatusChunk(int32_t updateAge, uint32_t chunk) override;
    int32_t BuildMoveChunk() override;
    int32_t HandleMoveChunk(uint32_t chunk) override;
    int32_t CalcCV(int calcMax) override;
    /// <summary>From the appearance's gesture (MechStateByGesture).</summary>
    int32_t GetBodyState() override;
    float GetTotalEffectiveness() override;
    float WeaponLocked(int32_t weaponIndex, MCVector3D targetPosition) override;
    int IsWeaponReady(int32_t weaponIndex) override;
    int IsWeaponWorking(int32_t weaponIndex) override;
    float CalcAttackChance(MCGameObject* target, int32_t aimLocation, float targetTime, int32_t weaponIndex,
                           float modifiers, int32_t* range, MCVector3D* targetPoint) override;
    int HitInventoryItem(int32_t itemIndex, int setupOnly) override;
    void DestroyBodyLocation(int32_t location) override;
    void CalcCriticalHit(int32_t hitLocation) override;
    int InjureBodyLocation(int32_t bodyLocation, float damage) override;
    int32_t FireWeapon(MCGameObject* target, float targetTime, int32_t weaponIndex, int32_t attackType,
                       int32_t aimLocation, MCVector3D* targetPoint) override;
    int32_t HandleWeaponFire(int32_t weaponIndex, MCGameObject* target, MCVector3D* targetPoint, int hit,
                             float entryAngle, int32_t numMissiles, int32_t missilesPastAMS, int32_t antiMissileShots,
                             int32_t hitLocation) override;
    int CanPowerUp() override;
    /// <summary>Unless both legs are gone (leg status 3).</summary>
    int CanMove() override;
    /// <summary>Whether the mech has jump jets.</summary>
    int CanJump() override;
    float GetJumpRange(int32_t* numOffsets, int32_t* jumpCost) override;
    int IsJumping(MCVector3D* jumpGoal) override;
    float CalcMaxSpeed() override;
    float CalcSlowSpeed() override;
    float CalcModerateSpeed() override;
    int32_t CalcSpriteSpeed(float speed, uint32_t flags, int32_t& state, int32_t& throttle) override;
    int HandleEjection() override;
    const char* GetIfaceName() override { return IfaceName.c_str(); }

    // Slots 219.. are BattleMech's own.

    virtual int32_t Init(MCFile* objFile) { return 0; }
    virtual float GetWeaponHeat(int32_t weaponIndex);
    virtual void NetUpdateMovement();
    virtual void HandleCriticalHit(int32_t bodyLocation, int32_t criticalSpace);
    virtual int HandleFall(int forward) { return 0; }
    virtual float CalcMaxTargetDamage();
    virtual float CalcExpectedTargetDamage(MCGameObject* target);

    using MCMover::CalcExpectedTargetDamage;
    using MCMover::Init;

    /// <summary>Sets and returns the leg status from the legs' damage (alarms / radio on change).</summary>
    int32_t CalcLegStatus();
    int32_t CalcTorsoStatus();
    int UpdateJump();
    int PivotTo();
    void UpdateMoveStateGoal();
    /// <summary>
    /// Steers along the pilot's move path: advances the step once within the margin of error, then sets the
    /// gesture, throttle and turn (or asks for a pivot through <paramref name="newMoveState"/>). Nonzero once the
    /// path is done. The last two parameters are unused.
    /// </summary>
    int UpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec, int32_t& newGestureStateGoal,
                       int32_t& newMoveState, int32_t& minThrottle, int32_t& maxThrottle);
    void SetNextMovePath(char& newThrottleSetting, int32_t& newGestureStateGoal);
    void UpdateTorso(float newRotatePerSec);
    void SetControlSettings(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                            int32_t& newGestureStateGoal, int32_t& minThrottle, int32_t& maxThrottle);
    int NetUpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                          int32_t& newGestureStateGoal, int32_t& newMoveState, int32_t& minThrottle,
                          int32_t& maxThrottle);
    void CreateJumpFX();
    void EndJumpFX();
    /// <summary>Where jump jet <paramref name="jet"/> (0 or 1) is this frame.</summary>
    MCVector3D GetJumpPosition(int32_t jet);
    int32_t TransferHitLocation(int32_t hitLocation);
    int32_t StartJump(MCVector3D jumpGoal);
    void DamageLoadedComponents();

    /// <summary>The weight class (getMechClass; 1 by init), indexing RankVersusChassisCombatModifier.</summary>
    uint8_t MechClass = 1;
    /// <summary>"ChassisBR" (100 when missing).</summary>
    int32_t ChassisBR = 0;
    /// <summary>0 intact, 2 a leg gone, 3 both legs gone (calcLegStatus).</summary>
    int8_t LegStatus = 0;
    /// <summary>calcTorsoStatus.</summary>
    int8_t TorsoStatus = 0;
    /// <summary>Inventory index of the left arm actuator.</summary>
    uint8_t LeftArmActuator = 0;
    /// <summary>Inventory index of the right arm actuator.</summary>
    uint8_t RightArmActuator = 0;
    /// <summary>Inventory index of the left leg actuator.</summary>
    uint8_t LeftLegActuator = 0;
    /// <summary>Inventory index of the right leg actuator.</summary>
    uint8_t RightLegActuator = 0;
    /// <summary>Inventory index of the gyro.</summary>
    uint8_t Gyro = 0;
    /// <summary>Jump jets fitted (canJump, getJumpRange).</summary>
    uint8_t NumJumpJets = 0;
    /// <summary>-100 by init (updateJump).</summary>
    float JumpTime = -100.0f;
    /// <summary>Set while jumping.</summary>
    int32_t InJump = 0;
    /// <summary>Where the jump lands.</summary>
    MCVector3D JumpGoal;
    /// <summary>When the center torso's internal structure was first injured (-1 before); destroyBodyLocation.</summary>
    float CenterTorsoInjuredTime = -1.0f;
    /// <summary>Set by a hit from behind (outside 90 degrees of the torso); a fall then goes forward (gesture 7).</summary>
    int32_t HitFromBehindThisFrame = 0;
    /// <summary>Set by a hit from the front; a fall then goes backward (gesture 8). Both clear when the fall starts.</summary>
    int32_t HitFromFrontThisFrame = 0;
    /// <summary>Set when the left arm is destroyed; the control turns it into MechControlData::blowLeftArm.</summary>
    int32_t LeftArmBlownThisFrame = 0;
    /// <summary>Set when the right arm is destroyed; the control turns it into MechControlData::blowRightArm.</summary>
    int32_t RightArmBlownThisFrame = 0;
    /// <summary>The footprint of the gesture's second step (hot spot packet slot 4) is down; the walking gestures
    /// re-arm it once past that frame.</summary>
    int32_t SecondStepPrinted = 0;
    /// <summary>The same for the first step (packet slot 0).</summary>
    int32_t FirstStepPrinted = 0;
    /// <summary>Torso yaw in degrees, within the dynamics type's maxTorsoYaw.</summary>
    float TorsoRotation = 0.0f;
    /// <summary>Right arm yaw in degrees, within maxArmYaw.</summary>
    float RightArmRotation = 0.0f;
    /// <summary>Left arm yaw in degrees, within maxArmYaw.</summary>
    float LeftArmRotation = 0.0f;
    /// <summary>Latched once the dead mech's actor lies still; the death sequence (deathTimer) runs from then.</summary>
    int32_t LyingDead = 0;
    /// <summary>Set once the wreck and its crater are left and the mech is off the interface.</summary>
    int32_t WreckDone = 0;
    /// <summary>The status window.</summary>
    MCGuiTitleWindow* StatusWindow = nullptr;
    /// <summary>Smoke streaming from damaged equipment (hitInventoryItem).</summary>
    MCSmoke* Smoke[4] = {};
    /// <summary>The hot spot each smoke streams from.</summary>
    int32_t SmokeHotSpot[4] = {};
    /// <summary>Seconds each smoke has left (15 at the start).</summary>
    float SmokeTime[4] = {};
    /// <summary>The jump jet effects.</summary>
    MCGameObject* JumpFX[2] = {};
    /// <summary>calcMaxTargetDamage.</summary>
    float MaxTargetDamage = 0.0f;
    /// <summary>The name the interface shows.</summary>
    std::string IfaceName;
    /// <summary>"Pilot" (-1 when missing).</summary>
    int32_t PilotId = 0;
    /// <summary>Whether the mech can be captured.</summary>
    int32_t Captureable = 0;
    /// <summary>"NotMineYet" (1 when missing).</summary>
    int NotMineYet = 0;
    /// <summary>Set when a mine goes off under the mech; mineCheck then marks the next tile without its side's mine
    /// state (state 1) and clears it.</summary>
    int32_t SteppedOnMine = 0;
    /// <summary>"DescIndex": the interface name is string 300 + it (-1 when missing).</summary>
    int32_t DescIndex = 0;
    /// <summary>"NameIndex".</summary>
    int32_t NameIndex = 0;
    /// <summary>"NameVariant".</summary>
    int32_t NameVariant = 0;
};

/// <summary>A mech's status window.</summary>
/// <remarks>Original source: <c>object\mech.cpp</c>, <c>object\mech.h</c>; 0x4c4 bytes.</remarks>
class MCMechStatusWindow : public MCGuiTitleWindow
{
public:
    ~MCMechStatusWindow() override;
    void Init(int32_t x, int32_t y, int32_t w, int32_t h, MCBattleMech* newMech);
    void HandleEvent(MCGuiEvent* event) override;
    void Resize(int32_t w, int32_t h) override;
    void Display() override;
    void Draw() override;
    /// <summary>Port: still paints a picture (in display), so it keeps one.</summary>
    bool DrawsLive() override { return false; }
    virtual MCBattleMech* GetMech() { return Mech; }

    /// <summary>The mech shown.</summary>
    MCBattleMech* Mech = nullptr;
};
