#pragma once

#include "ai/MCMoveSystem.h"
#include "ai/MCTacticalOrder.h"
#include "object/MCBigGameObject.h"
#include "object/MCSortList.h"
#include "object/MCWeaponFireChunk.h"
#include "object/MCWeaponShotInfo.h"

class MCAppearance;
class MCControl;
class MCDynamics;
class MCFitIniFile;
class MCMechWarrior;
class MCMover;
class MCMoverGroup;
class MCObjectEvent;
class MCSensorSystem;
class MCSortList;
class MCTeam;
class MCWeaponFireChunk;
struct MCSystemTracker;
enum MCRadioMessageType : int32_t;

/// <summary>Entries of <c>MoverRoster</c>: movers have part ids 0x200..0xfff.</summary>
constexpr int32_t MAX_MOVER_PART_ID = 0x1000;
/// <summary>Weapon fire, critical hit chunks a mover queues per direction.</summary>
constexpr int32_t MAX_WEAPONFIRE_CHUNKS = 0x80;
/// <summary>Radio chunks a mover queues per direction.</summary>
constexpr int32_t MAX_RADIO_CHUNKS = 7;
/// <summary>Path cells a mover's range lock can hold (the original doesn't check it against the range).</summary>
constexpr int32_t MAX_PATH_RANGE_LOCKS = 10;

/// <summary>
/// A mover's state as a network status chunk: body state, target and orders. Packed into <see cref="Data"/> to
/// send.
/// </summary>
/// <remarks>Original source: <c>object\mover.cpp</c> (init inline in <c>object\mover.h</c>); 0x2c bytes. Field names
/// follow MechCommander 2's StatusChunk, which kept this layout.</remarks>
class MCStatusChunk
{
public:
    /// <summary>Clears everything; no target cell.</summary>
    void Init();
    /// <summary>Takes the mover's state.</summary>
    virtual void Build(MCMover* mover);
    /// <summary>Packs the fields into <see cref="Data"/>.</summary>
    virtual void Pack(MCMover* mover);
    /// <summary>Unpacks <see cref="Data"/> into the fields.</summary>
    virtual void Unpack(MCMover* mover);
    /// <summary>Whether two chunks carry the same state.</summary>
    int EqualTo(MCStatusChunk* chunk);

    /// <summary>The mover's body state.</summary>
    uint32_t BodyState = 0;
    /// <summary>What the target is (mover, terrain object, train car, location...).</summary>
    uint8_t TargetType = 0;
    /// <summary>The target's part id.</summary>
    int32_t TargetId = 0;
    /// <summary>A terrain target's block, or a train target's train.</summary>
    int32_t TargetBlockOrTrainNumber = 0;
    /// <summary>A terrain target's vertex, or a train target's car.</summary>
    int32_t TargetVertexOrCarNumber = 0;
    /// <summary>A terrain target's item on the vertex.</summary>
    uint8_t TargetItemNumber = 0;
    /// <summary>A location target's map cell (row, column); -1 for none.</summary>
    int16_t TargetCellRC[2] = {-1, -1};
    /// <summary>Whether the pilot was ordered to eject.</summary>
    int32_t EjectOrderGiven = 0;
    /// <summary>A jump order.</summary>
    int32_t JumpOrder = 0;
    /// <summary>The packed chunk.</summary>
    uint32_t Data = 0;
};

/// <summary>A body location's critical space: the inventory item in it ("Component%d").</summary>
struct MCCriticalSpace
{
    /// <summary>The inventory index, 0xff for an empty space.</summary>
    uint8_t InventoryID = 0;
    /// <summary>The second byte of the "Component%d" entry.</summary>
    int32_t Hit = 0;
};

/// <summary>One of a mover's body locations (a mech's head, torso, arms, legs; a vehicle's sides).</summary>
/// <remarks>0x14 bytes. Field meanings are
/// settled with mech.cpp.</remarks>
struct MCBodyLocation
{
    /// <summary>"CASE".</summary>
    int32_t HasCase = 0;
    /// <summary>The critical spaces the location's components need (their criticalSpacesReq, summed per space).</summary>
    int32_t TotalSpaces = 0;
    /// <summary>The location's critical spaces.</summary>
    MCCriticalSpace* CriticalSpaces = nullptr;
    float CurInternalStructure = 0;
    /// <summary>"HotSpotNumber".</summary>
    uint8_t HotSpotNumber = 0;
    /// <summary>The type's internal structure for the location.</summary>
    uint8_t MaxInternalStructure = 0;
    /// <summary>2 when destroyed (BattleMech::calcLegStatus).</summary>
    uint8_t DamageState = 0;
};

/// <summary>One of a mover's inventory items (a component, weapon or ammo bin).</summary>
/// <remarks>0x1c bytes. Field meanings beyond
/// these are settled with mover.cpp.</remarks>
struct MCInventoryItem
{
    /// <summary>The item's master component id.</summary>
    uint8_t MasterID = 0;
    /// <summary>Hits it has taken (MasterComponent::health minus this is what getInventoryDamage gives).</summary>
    uint8_t Health = 0;
    /// <summary>Nonzero when destroyed or disabled.</summary>
    int32_t Disabled = 0;
    /// <summary>"FacesForward" (weapons).</summary>
    uint8_t FacesForward = 0;
    /// <summary>An ammo bin's starting rounds.</summary>
    int16_t StartAmount = 0;
    /// <summary>An ammo bin's rounds (calcAmmoTotals sums them per type).</summary>
    int16_t Amount = 0;
    /// <summary>A weapon's (or anti-missile system's) ammo type: its index in Mover::ammoTypeTotal.</summary>
    int16_t AmmoIndex = 0;
    /// <summary>Scenario time a weapon is ready again (startWeaponRecycle).</summary>
    float ReadyTime = 0;
    /// <summary>The body location the item sits in (an ammo explosion hits it).</summary>
    uint8_t BodyLocation = 0;
    /// <summary>A weapon's effectiveness (calcWeaponEffectiveness sums it, scaled by gunnery).</summary>
    int16_t Effectiveness = 0;
    /// <summary>A weapon's ratings per range step (NumRangeRatings pairs: rating, then damage rate; objectCache,
    /// freed with the inventory).</summary>
    float* RangeRatings = nullptr;
};

/// <summary>One of a mover's armor locations.</summary>
/// <remarks>8 bytes.</remarks>
struct MCArmorLocation
{
    /// <summary>Armor left.</summary>
    float CurArmor = 0;
    /// <summary>Full armor (needsRefit).</summary>
    uint8_t MaxArmor = 0;
};

/// <summary>A mover's ammo of one type.</summary>
/// <remarks>0x0c bytes.</remarks>
struct MCAmmoTally
{
    /// <summary>The ammo's master component id.</summary>
    int32_t MasterId = 0;
    /// <summary>Rounds left.</summary>
    int32_t CurAmount = 0;
    /// <summary>Rounds at the start.</summary>
    int32_t StartAmount = 0;
};

/// <summary>
/// Anything that moves under a pilot: mechs, ground vehicles, elementals. Holds the inventory, sensors and weapons,
/// the pilot, control and dynamics, path locks, and the network chunks.
/// </summary>
/// <remarks>Original source: <c>object\mover.cpp</c>, <c>object\mover.h</c>; 0x8a0 bytes. Field names are the
/// port's where the original's aren't known; many follow MechCommander 2's Mover.</remarks>
class MCMover : public MCBigGameObject
{
public:
    /// <summary>The original's vector deleting destructor calls destroy.</summary>
    ~MCMover() override { Destroy(); }
    /// <summary>The constructor calls init (inlined in the derived types' createInstance).</summary>
    MCMover()
    {
        Frame.ResetToWorldFrame();
        Init();
    }

    using MCBigGameObject::Init;
    /// <summary>Resets every field (the constructor's work); counts the mover and makes the shared sort list.</summary>
    void Init() override;
    /// <summary>Frees the inventory, sensors, jammer/ECM trackers, potential contact, appearance, control and
    /// dynamics; the last mover frees the sort list.</summary>
    void Destroy() override;
    MCAppearance* GetAppearance() override { return Appearance; }
    int32_t HandleEvent(MCObjectEvent* event) override;
    /// <summary>Zero.</summary>
    MCVector3D GetPositionFromHS(uint32_t hotSpot) override;
    int UnderPlayerControl() override { return 1; }
    /// <summary>The group's id, -1 for none.</summary>
    int32_t GetGroupId() override;
    /// <summary>Wakes or puts to sleep; a shut-down mover's pilot is ordered to power up when woken.</summary>
    void SetAwake(int awake) override;
    /// <summary>Sets the part id and registers the mover in MoverRoster.</summary>
    void SetPartId(int32_t newPartId) override;
    void ReduceAntiMissileAmmo(int32_t numShots) override;
    int32_t FireAntiMissileSystem(int32_t numMissiles, int32_t& antiMissileShots) override;
    MCVector3D RelativePosition(float angle, float distance, uint32_t flags) override;
    void SetPosition(MCVector3D& newPosition) override;
    MCVector3D GetVelocity() override { return Velocity; }
    void SetVelocity(MCVector3D& newVelocity) override { Velocity = newVelocity; }
    MCFrameOfRef GetFrame() override { return Frame; }
    void SetFrame(MCFrameOfRef& newFrame) override { Frame = newFrame; }
    /// <summary>Sets the alignment, and the pilot's.</summary>
    void SetAlignment(int32_t newAlignment) override;
    void SetCommanderId(int32_t newCommanderId) override { CommanderId = static_cast<int8_t>(newCommanderId); }
    int32_t GetCommanderId() override { return CommanderId; }
    int LineOfSight(MCGameObject* target) override;
    int LineOfSight(MCVector3D point) override;
    int LineOfFire(MCGameObject* target) override;
    float RelFacingTo(MCVector3D goal, int32_t bodyPart) override;
    float RelViewFacingTo(MCVector3D goal) override;
    /// <summary>Deselecting keeps the selection a second longer (unless networked); selecting sets it.</summary>
    void SetSelected(int32_t newSelected) override;
    /// <summary>Whether the pilot's current order is a withdraw (0x13).</summary>
    int IsWithdrawing() override;
    MCMechWarrior* GetPilot() override { return Pilot; }
    int IsRevealed() override;
    /// <summary>Joins <paramref name="newTeam"/>: sensors, jammer and ECM follow.</summary>
    int32_t SetTeam(MCTeam* newTeam) override;
    int32_t GetVitalInfo(void* vitalInfo) override;

    // Slots 116.. are Mover's own.

    /// <summary>Part id 0x200 + (commander * 32 + group) * 12 + index.</summary>
    virtual void SetPartId(int32_t commanderId, int32_t groupId, int32_t index);
    virtual int32_t GetSpeedState() { return 0; }
    /// <summary>The slope under the mover, in degrees.</summary>
    virtual float GetTerrainAngle();
    virtual float GetVelocityTilt();
    virtual int LineOfFire(MCVector3D point);
    virtual void LineOfSensor(MCGameObject* target, int32_t& sensorResult, int32_t& losResult);
    /// <summary>Gives <paramref name="tacOrder"/> to the pilot (networked: through the server).</summary>
    virtual int32_t HandleTacticalOrder(MCTacticalOrder tacOrder, int32_t priority, int queuePlayerOrder);
    virtual void PilotingCheck(uint32_t situation, float modifier);
    virtual void ForcePilotingCheck();
    virtual int CanFireWeapons() { return 1; }
    /// <summary>Every DamageRateFrequency seconds: over 10 damage since the last check alarms the pilot.</summary>
    virtual void UpdateDamageTakenRate();
    /// <summary>Returns 4 (no action). Unnamed in Ghidra; the name is MechCommander 2's.</summary>
    virtual int32_t CheckShortRangeCollision() { return 4; }
    virtual void SetOverlayWeightClass(int32_t overlayClass) { OverlayWeightClass = overlayClass; }
    virtual int32_t GetOverlayWeightClass() { return OverlayWeightClass; }
    virtual void GetStopInfo(float& stopHeading, float& stopVelocity) {}
    /// <summary>Whether a mover's path has locked the cell, or (diameter 3) any of the 3x3 cells around it;
    /// diameter 5 is never locked.</summary>
    virtual int GetPathLocked(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, int32_t diameter);
    /// <summary>Locks or unlocks the cell, or (diameter 3) the 3x3 cells around it; diameter 5 does nothing.</summary>
    virtual void SetPathLock(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, int set, int32_t diameter);
    /// <summary>Whether the cell's neighbour in <paramref name="dir"/> (adjCellTable) is path locked.</summary>
    virtual int GetAdjacentCellPathLocked(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, int32_t dir);
    /// <summary>Locks (or unlocks) the mover's cell and its path's next cells.</summary>
    virtual void UpdatePathLock(int set);
    virtual int GetPathRangeLock(int32_t range, int* reachedEnd);
    virtual int32_t SetPathRangeLock(int set, int32_t range);
    virtual int GetPathRangeBlocked(int32_t range, int* reachedEnd);
    virtual int CrashAvoidanceSystem() { return 0; }
    virtual void UpdateHustleTime();
    virtual void MineCheck() {}
    virtual void UpdateMovement() {}
    virtual int32_t BounceToAdjCell();
    virtual int32_t SetGroup(MCMoverGroup* newGroup);
    virtual void SetPilot(MCMechWarrior* newPilot);
    virtual int32_t UpdateWeaponFireChunks(int32_t which);
    virtual int32_t UpdateCriticalHitChunks(int32_t which);
    virtual int32_t UpdateRadioChunks(int32_t which);
    virtual MCStatusChunk* GetStatusChunk() { return &StatusChunk; }
    virtual int32_t BuildStatusChunk() { return 0; }
    virtual int32_t HandleStatusChunk(int32_t updateAge, uint32_t chunk) { return 0; }
    virtual MCMoveChunk* GetMoveChunk() { return &MoveChunk; }
    virtual int32_t BuildMoveChunk() { return 0; }
    virtual int32_t HandleMoveChunk(uint32_t chunk) { return 0; }
    virtual int32_t CalcCV(int calcMax) { return 0; }
    virtual void GetDamageClass(int32_t& damageClass, int& shutDown);
    virtual int32_t GetBodyState() { return -1; }
    virtual float GetTotalEffectiveness() { return 0.0f; }
    virtual int32_t CalcOffsetMoveGoal(MCVector3D target, MCVector3D offset, MCVector3D& goal);
    virtual int32_t CalcMoveGoal(MCGameObject* target, MCVector3D moveGoal, int32_t isGroup, int32_t offsetIndex,
                                 int32_t groupSize, int32_t pointIndex, MCVector3D& newGoal, uint32_t params);
    virtual int32_t CalcMovePath(MCMovePath* path, MCVector3D start, int32_t thruArea, int32_t goalDoor,
                                 MCVector3D finalGoal, MCVector3D* goal, int32_t* goalCell, uint32_t params);
    virtual int32_t CalcMovePath(MCMovePath* path, int32_t pathType, MCVector3D start, MCVector3D goal,
                                 int32_t* goalCell, uint32_t params);
    virtual int32_t CalcEscapePath(MCMovePath* path, MCVector3D start, MCVector3D goal, int32_t* goalCell,
                                   uint32_t params, MCVector3D& escapeGoal);
    virtual int32_t GetContacts(int32_t* contactList, int32_t contactCriteria, int32_t sortType);
    /// <summary>The weapon's facing to <paramref name="targetPosition"/>: relFacingTo (slot 58) of the point.
    /// Unnamed in Ghidra; the subclasses name it (their mangled names return float).</summary>
    virtual float WeaponLocked(int32_t weaponIndex, MCVector3D targetPosition);
    virtual int32_t WeaponInRange(int32_t weaponIndex, float metersToTarget);
    virtual int32_t GetWeaponsReady(int32_t* list, int32_t listSize);
    virtual int32_t GetWeaponsLocked(int32_t* list, int32_t listSize);
    virtual int32_t GetWeaponsInRange(int32_t* list, int32_t listSize, float orderFireRange);
    virtual int32_t GetWeaponShots(int32_t weaponIndex);
    virtual float GetWeaponAmmoLevel(int32_t weaponIndex);
    virtual void CalcWeaponEffectiveness(int setMax);
    virtual void CalcWeaponRangeRatings();
    virtual void CalcAmmoTotals();
    virtual int CalcOptimalRange(MCGameObject* target);
    virtual int32_t CalcLongestRangeWeapon();
    virtual float GetFireRange(int32_t which);
    /// <summary>getFireRange(-2).</summary>
    virtual float GetMaxFireRange();
    virtual int IsWeaponIndex(int32_t itemIndex);
    virtual int IsWeaponReady(int32_t weaponIndex);
    virtual int IsWeaponWorking(int32_t weaponIndex);
    virtual int IsWeaponMissile(int32_t weaponIndex);
    /// <summary>Sets the weapon's ready time: now plus its recycle time. Unnamed in Ghidra; MechCommander 2's
    /// name.</summary>
    virtual void StartWeaponRecycle(int32_t weaponIndex);
    virtual int32_t TallyAmmo(int32_t ammoMasterId);
    virtual int32_t ReduceAmmo(int32_t ammoMasterId, int32_t amount);
    virtual int32_t GetNumAmmoTypes() { return NumAmmoTypes; }
    virtual int32_t GetAmmoType(int32_t ammoTypeIndex) { return AmmoTypeTotal[ammoTypeIndex].MasterId; }
    virtual int32_t GetAmmoTypeTotal(int32_t ammoTypeIndex) { return AmmoTypeTotal[ammoTypeIndex].CurAmount; }
    virtual int32_t GetAmmoTypeStart(int32_t ammoTypeIndex) { return AmmoTypeTotal[ammoTypeIndex].StartAmount; }
    virtual void DeductWeaponShot(int32_t weaponIndex, int32_t ammoAmount);
    virtual int NeedsRefit(int armorOnly);
    virtual int32_t SortWeapons(int32_t* weaponList, int32_t* valueList, int32_t listSize, int32_t sortType,
                                int skillCheck);
    virtual float CalcAttackChance(MCGameObject* target, int32_t aimLocation, float targetTime, int32_t weaponIndex,
                                   float modifiers, int32_t* range, MCVector3D* targetPoint);
    virtual float CalcAttackModifier(MCGameObject* target, int32_t weaponIndex, int skillCheck) { return 0.0f; }
    virtual int HitInventoryItem(int32_t itemIndex, int setupOnly) { return 0; }
    /// <summary>
    /// Disables the mover: the pilot is alarmed; an enemy mech may leave salvage (MechSalvageChance) or be blown
    /// apart. Unnamed in Ghidra; GroundVehicle names its override disable.
    /// </summary>
    virtual void Disable(uint32_t cause);
    virtual void ShutDown();
    virtual void StartUp();
    virtual void DestroyBodyLocation(int32_t location) {}
    virtual void CalcCriticalHit(int32_t hitLocation) {}
    virtual int InjureBodyLocation(int32_t bodyLocation, float damage) { return 0; }
    virtual void AmmoExplosion(int32_t ammoIndex);
    virtual int32_t FireWeapon(MCGameObject* target, float targetTime, int32_t weaponIndex, int32_t attackType,
                               int32_t aimLocation, MCVector3D* targetPoint)
    {
        return 0;
    }

    virtual int32_t HandleWeaponFire(int32_t weaponIndex, MCGameObject* target, MCVector3D* targetPoint, int hit,
                                     float entryAngle, int32_t numMissiles, int32_t missilesPastAMS,
                                     int32_t antiMissileShots, int32_t hitLocation)
    {
        return 0;
    }

    virtual float RelFacingDelta(MCVector3D goalPos, MCVector3D targetPos);
    virtual int CanPowerUp() { return 1; }
    virtual int CanMove() { return 1; }
    virtual int CanJump() { return 0; }
    virtual float GetJumpRange(int32_t* numOffsets, int32_t* jumpCost);
    virtual int IsJumping(MCVector3D* jumpGoal) { return 0; }
    /// <summary>The firing arc of the mover's class (mech FireArc, vehicle, elemental; else 60).</summary>
    virtual float GetFireArc();
    virtual float CalcMaxSpeed() { return 0.0f; }
    virtual float CalcSlowSpeed() { return 0.0f; }
    /// <summary>Unnamed in Ghidra; BattleMech names its override calcModerateSpeed.</summary>
    virtual float CalcModerateSpeed() { return 0.0f; }
    virtual int32_t CalcSpriteSpeed(float speed, uint32_t flags, int32_t& state, int32_t& throttle);
    virtual float GetGestureStopDistance() { return 0.0f; }
    virtual int HandleEjection() { return 0; }
    virtual float CalcExpectedTargetDamage() { return 0.0f; }
    virtual const char* GetIfaceName() { return "No Name"; }

    void SetLastValidPosition(MCVector3D pos) { LastValidPosition = pos; }
    /// <summary>The group's point, or null without a group. Unnamed in Ghidra; MechCommander 2's name.</summary>
    MCMover* GetPoint();
    int32_t ClearWeaponFireChunks(int32_t which);
    int32_t AddWeaponFireChunk(int32_t which, MCWeaponFireChunk* chunk);
    int32_t AddWeaponFireChunks(int32_t which, uint32_t* packedChunkBuffer, int32_t numChunks);
    int32_t GrabWeaponFireChunks(int32_t which, uint32_t* packedChunkBuffer, int32_t maxChunks);
    int32_t ClearCriticalHitChunks(int32_t which);
    int32_t AddCriticalHitChunk(int32_t which, int32_t bodyLocation, int32_t criticalSpace);
    int32_t AddCriticalHitChunks(int32_t which, uint8_t* packedChunkBuffer, int32_t numChunks);
    int32_t GrabCriticalHitChunks(int32_t which, uint8_t* packedChunkBuffer);
    int32_t ClearRadioChunks(int32_t which);
    int32_t AddRadioChunk(int32_t which, uint8_t msg);
    int32_t AddRadioChunks(int32_t which, uint8_t* packedChunkBuffer, int32_t numChunks);
    int32_t GrabRadioChunks(int32_t which, uint8_t* packedChunkBuffer);
    void PlayMessage(MCRadioMessageType messageId, int propogateIfMultiplayer);
    int EnemyRevealed();
    int32_t GetInventoryDamage(int32_t itemIndex);
    /// <summary>The ECM's effect, 0 without a working ECM.</summary>
    float GetEcmEffect();
    float GetProbeEffect();
    /// <summary>MaxVisualRadius plus the probe's effect.</summary>
    float GetVisualRange();
    /// <summary>The mover challenging this one; dropped once disabled.</summary>
    MCGameObject* GetChallenger();
    /// <summary>Unnamed in Ghidra; MechCommander 2's name.</summary>
    void SetChallenger(MCGameObject* newChallenger);

    /// <summary>The ground's normal under the mover (getTerrainAngle, getVelocityTilt).</summary>
    MCVector3D TerrainNormal;
    /// <summary>Velocity in meters per second.</summary>
    MCVector3D Velocity;
    /// <summary>Orientation.</summary>
    MCFrameOfRef Frame;
    /// <summary>The mover's long name (the string table entry of its profile; GroundVehicle's
    /// getIfaceName returns it). Also printed by the debug status chunks.</summary>
    std::string DebugStatus;
    /// <summary>Profile "icon" (up to 19 characters).</summary>
    char IconName[20] = {};
    /// <summary>The type's "Chassis"; indexes TileThrottleMultiplier / OverlayThrottleMultiplier.</summary>
    uint8_t Chassis = 0;
    /// <summary>The type's "EndoSteel" (mechs).</summary>
    int32_t EndoSteel = 0;
    /// <summary>The type's "TonnageClass".</summary>
    float TonnageClass = 0.0f;
    /// <summary>The type's internal structure tonnage.</summary>
    float InternalStructureTonnage = 0.0f;
    /// <summary>Damage taken since the last check (updateDamageTakenRate).</summary>
    float DamageRateTally = 0.0f;
    /// <summary>Scenario time of the next damage rate check.</summary>
    float DamageRateCheckTime = 1.0f;
    /// <summary>All the damage taken (BattleMech::handleWeaponHit).</summary>
    float TotalDamageTaken = 0.0f;
    /// <summary>
    /// Body location <paramref name="location"/> of <see cref="Body"/>. Port-only: debug builds check the index is
    /// one of the <see cref="NumBodyLocations"/>, since an armor location (a mech's rear torsos, 8..10) used as a
    /// body location reads past the array. Map one through <c>MechArmorToBodyLocation</c> first.
    /// </summary>
    auto BodyAt(int32_t location) -> MCBodyLocation&
    {
        SDL_assert(Body != nullptr && location >= 0 && location < NumBodyLocations);
        return Body[location];
    }

    /// <summary>Const <see cref="BodyAt"/>.</summary>
    auto BodyAt(int32_t location) const -> const MCBodyLocation&
    {
        SDL_assert(Body != nullptr && location >= 0 && location < NumBodyLocations);
        return Body[location];
    }

    /// <summary>The body locations.</summary>
    std::unique_ptr<MCBodyLocation[]> Body;
    /// <summary>How many.</summary>
    int8_t NumBodyLocations = 0;
    /// <summary>"Armor" "Type".</summary>
    uint8_t ArmorType = 0;
    /// <summary>"Armor" "Tonnage".</summary>
    float ArmorTonnage = 0.0f;
    /// <summary>The armor locations.</summary>
    std::unique_ptr<MCArmorLocation[]> Armor;
    /// <summary>How many.</summary>
    int8_t NumArmorLocations = 0;
    /// <summary>The inventory: other items, then weapons, then ammo.</summary>
    std::unique_ptr<MCInventoryItem[]> Inventory;
    /// <summary>Items before the weapons.</summary>
    uint8_t NumOther = 0;
    /// <summary>Weapons.</summary>
    uint8_t NumWeapons = 0;
    /// <summary>Ammo bins.</summary>
    uint8_t NumAmmos = 0;
    /// <summary>Ammo by type.</summary>
    std::unique_ptr<MCAmmoTally[]> AmmoTypeTotal;
    /// <summary>How many.</summary>
    int8_t NumAmmoTypes = 0;
    /// <summary>The pilot.</summary>
    MCMechWarrior* Pilot = nullptr;
    /// <summary>The sensors (from sensorSystemManager).</summary>
    MCSensorSystem* SensorSystem = nullptr;
    /// <summary>The team's tracker of the mover's jammer.</summary>
    MCSystemTracker* JammerTracker = nullptr;
    /// <summary>The team's tracker of the mover's ECM.</summary>
    MCSystemTracker* EcmTracker = nullptr;
    /// <summary>Inventory index of the cockpit, 0xff for none.</summary>
    uint8_t Cockpit = 0xff;
    /// <summary>Inventory index of the engine.</summary>
    uint8_t Engine = 0xff;
    /// <summary>Inventory index of the life support.</summary>
    uint8_t LifeSupport = 0xff;
    /// <summary>Inventory index of the sensors.</summary>
    uint8_t Sensor = 0xff;
    /// <summary>Inventory index of the ECM.</summary>
    uint8_t Ecm = 0xff;
    /// <summary>Inventory index of the probe.</summary>
    uint8_t Probe = 0xff;
    /// <summary>Inventory index of the jammer (setTeam adds it to the team's jammers).</summary>
    uint8_t Jammer = 0xff;
    /// <summary>The largest minimum range of the working weapons (calcLongestRangeWeapon).</summary>
    float MaxMinRange = 0.0f;
    /// <summary>The working weapon with the longest long range, 0xff for none.</summary>
    uint8_t LongestRangeWeapon = 0;
    /// <summary>The working weapon with the shortest short range, 0xff for none.</summary>
    uint8_t ShortestRangeWeapon = 0;
    /// <summary>calcWeaponEffectiveness(max).</summary>
    float MaxWeaponEffectiveness = 0.0f;
    /// <summary>calcWeaponEffectiveness.</summary>
    float WeaponEffectiveness = 0.0f;
    /// <summary>calcOptimalRange; -1 until known.</summary>
    float OptimalRange = -1.0f;
    /// <summary>Anti-missile systems.</summary>
    int8_t NumAntiMissileSystems = 0;
    /// <summary>Their inventory indices.</summary>
    uint8_t AntiMissileSystem[23] = {};
    /// <summary>"Engine" "Tonnage".</summary>
    float EngineTonnage = 0.0f;
    /// <summary>"Engine" "Rating".</summary>
    uint32_t EngineRating = 0;
    /// <summary>When a disabled engine blows (5 s after the critical hit; -1 for never). A mover with one set no
    /// longer moves; destroyBodyLocation checks it too.</summary>
    float EngineBlowTime = -1.0f;
    /// <summary>Used by calcMovePath and calcEscapePath.</summary>
    /// <summary>"Engine" "MaxRunSpeed".</summary>
    float MaxRunSpeed = 0.0f;
    /// <summary>Set by shutDown.</summary>
    int32_t ShutDownThisFrame = 0;
    /// <summary>Set by startUp.</summary>
    int32_t StartUpThisFrame = 0;
    /// <summary>Set by disable.</summary>
    int32_t DisableThisFrame = 0;
    /// <summary>The group (lance).</summary>
    MCMoverGroup* Group = nullptr;
    /// <summary>
    /// The mover's rank in its order (SortMoverList, the group's order spread; copied into
    /// TacticalOrder::selectionIndex); -1 by init.
    /// </summary>
    int32_t SelectionIndex = -1;
    /// <summary>The commander's id.</summary>
    int8_t CommanderId = 0;
    /// <summary>-1 by init; forcePilotingCheck raises it to 0.</summary>
    int32_t PilotCheckModifier = -1;
    /// <summary>Cleared by pilotingCheck.</summary>
    int32_t PilotingCheckPending = 0;
    /// <summary>When calcWeaponEffectiveness last ran (the AI recomputes its optimal range against a target whose
    /// effectiveness is newer).</summary>
    float LastWeaponEffectivenessCalc = 0.0f;
    /// <summary>When calcOptimalRange last ran.</summary>
    float LastOptimalRangeCalc = 0.0f;
    /// <summary>The mover challenging this one (getChallenger).</summary>
    MCGameObject* Challenger = nullptr;
    /// <summary>The appearance.</summary>
    MCAppearance* Appearance = nullptr;
    /// <summary>The control.</summary>
    MCControl* Control = nullptr;
    /// <summary>The dynamics.</summary>
    MCDynamics* Dynamics = nullptr;
    /// <summary>The DPID of the player whose mover this is (multiplayer; set when the scenario starts).</summary>
    uint32_t NetOwnerID = 0;
    /// <summary>Networked: a 256-byte name buffer (string 0xb9); null in single player.</summary>
    std::unique_ptr<char[]> NetName;
    /// <summary>-1 by init; networked: the owning player.</summary>
    int32_t NetPlayerId = -1;
    /// <summary>-1 by init; networked: the roster slot.</summary>
    int32_t NetRosterIndex = -1;
    /// <summary>The network status chunk.</summary>
    MCStatusChunk StatusChunk;
    /// <summary>Set when a network move chunk arrives; the next update warps to its first step if too far off.</summary>
    int32_t NewMoveChunk = 0;
    /// <summary>The network move chunk.</summary>
    MCMoveChunk MoveChunk{};
    /// <summary>Weapon fire chunks queued, out (0) and in (1).</summary>
    int32_t NumWeaponFireChunks[2] = {};
    uint32_t WeaponFireChunks[2][MAX_WEAPONFIRE_CHUNKS] = {};
    /// <summary>Critical hit chunks queued.</summary>
    int32_t NumCriticalHitChunks[2] = {};
    uint8_t CriticalHitChunks[2][MAX_WEAPONFIRE_CHUNKS] = {};
    /// <summary>Radio chunks queued.</summary>
    int32_t NumRadioChunks[2] = {};
    uint8_t RadioChunks[2][MAX_RADIO_CHUNKS] = {};
    /// <summary>Whether the pilot was ordered to eject (sent in the status chunk).</summary>
    int32_t EjectOrderGiven = 0;
    /// <summary>
    /// Seconds left of the death sequence, counted down once the mover lies dead (0.8 for a mech or elemental): the
    /// death explosion goes off under 0.4, the wreck is left under 0.
    /// </summary>
    float DeathTimer = 1.0f;
    /// <summary>Set once the death explosion has gone off.</summary>
    int32_t DeathExplosionDone = 0;
    /// <summary>Set by the withdraw order: the mover leaves the scenario once off the map or off screen.</summary>
    int32_t Withdrawing = 0;
    /// <summary>The last position the mover could stand on.</summary>
    MCVector3D LastValidPosition;
    /// <summary>The way a mech pivots toward a target: 0 or 1, 0xff to choose again (BattleMech::pivotTo).</summary>
    uint8_t PivotDirection = 0xff;
    /// <summary>-999 by init (updateHustleTime).</summary>
    float LastHustleTime = -999.0f;
    /// <summary>A ground vehicle type's "AmmoTruck"; 0 for the others.</summary>
    int32_t AmmoTruck = 0;
    /// <summary>Distance moved since the mover last marked what it sees (it marks again past Terrain::metersPerVertex; 1000 by
    /// init, so the first update marks).</summary>
    float DistanceSinceMarkSeen = 0.0f;
    /// <summary>Used by needsRefit and TacticalOrder::status.</summary>
    MCGameObject* RefitBuddy = nullptr;
    /// <summary>The type's "CrashAvoidSelf".</summary>
    int32_t CrashAvoidSelf = 1;
    int32_t CrashAvoidPath = 1;
    /// <summary>Path lock level of the mover's own cell (updatePathLock); the type's "CrashBlockSelf".</summary>
    int32_t PathLockLevel = 1;
    /// <summary>Path lock range (updatePathLock); the type's "CrashBlockPath".</summary>
    int32_t PathLockRange = 1;
    float CrashYieldTime = 1.5f;
    /// <summary>Cells setPathRangeLock has locked along the path.</summary>
    int32_t NumPathRangeLocks = 0;
    /// <summary>Those cells: tileR, tileC, cellR, cellC.</summary>
    int32_t PathRangeLocks[MAX_PATH_RANGE_LOCKS][4] = {};
    /// <summary>setOverlayWeightClass.</summary>
    int32_t OverlayWeightClass = 0;
    /// <summary>Scenario time a deselection takes effect (setSelected).</summary>
    float DeselectTime = 0.0f;
    /// <summary>Seconds <see cref="StationaryTarget"/> has held still (calcAttackChance).</summary>
    float StationaryTime = 0.0f;
    /// <summary>The target calcAttackChance last timed.</summary>
    MCGameObject* StationaryTarget = nullptr;
    /// <summary>The salvage roll (MechSalvageChance), -999 until rolled.</summary>
    int32_t SalvageRoll = -999;
    /// <summary>
    /// Set by the interface while the player holds a forced-order key (ctrl, F9-F12) with this mover selected;
    /// cleared on release. The mover's queued waypoints are then joined by lines.
    /// </summary>
    int32_t DrawOrderLines = 0;

    /// <summary>Movers alive (the last one's destroy frees <see cref="SortList"/>).</summary>
    static int32_t NumMovers;
    /// <summary>The movers' shared sort list.</summary>
    static MCSortList* SortList;
};

/// <summary>Seconds between a sensor's scans (FIT "ContactUpdateFrequency", 4). Unnamed in MCX.EXE; the port
/// names it after its FIT key.</summary>
extern float ContactUpdateFrequency;
/// <summary>Seconds between the group members' delayed orders (FIT "DelayedOrderTime", 1).</summary>
extern float DelayedOrderTime;
/// <summary>Firing arcs in degrees: mechs, ground vehicles, elementals (FIT "FireArc").</summary>
extern float FireArc[3];
/// <summary>Anti-missile systems' volleys and damage per volley: [0] Inner Sphere, [1] clan.</summary>
extern int32_t AntiMissileSystemStats[2][2];
/// <summary>Seconds between damage rate checks (Mover::updateDamageTakenRate).</summary>
extern float DamageRateFrequency;
/// <summary>How far a pilot sees unaided, in meters (Mover::getVisualRange adds the probe's range).</summary>
extern float MaxVisualRadius;
/// <summary>The target type of the weapon fire chunk being replayed (Mover::updateWeaponFireChunks).</summary>
/// <remarks>globals_by_file.md places it in object\gvehicl.cpp; it sits among mover.cpp's globals.</remarks>
extern int32_t TargetRolo;
/// <summary>A weapon fire chunk's entry angle quadrants in degrees: front, rear, left, right. Unnamed in MCX.EXE
/// (0x00791958).</summary>
extern float EntryAngleTable[4];
/// <summary>The weapon fire chunk being replayed (Mover::updateWeaponFireChunks).</summary>
extern MCWeaponFireChunk CurMoverWeaponFireChunk;

/// <summary>Fire range selectors 0..2 of Mover::getFireRange, in meters (short 250, medium 500, long 1000).</summary>
extern float WeaponRange[3];
/// <summary>Mover::getFireRange(-4) (75).</summary>
extern float DefaultAttackRange;
/// <summary>Range steps a weapon is rated at (calcWeaponRangeRatings; 31).</summary>
extern int32_t NumRangeRatings;
/// <summary>Meters between the range steps (30).</summary>
extern float RangeRatingIncrement;
/// <summary>
/// Attack chance modifiers (percent), FIT "WeaponFireModifiers": [0..2] target at short, medium, long range; [3]
/// aimed at the head, [4] torso, [5] limbs; [6] a target that isn't a mover; [7..22] copied into
/// RankVersusChassisCombatModifier; [23] a target stationary MaxStationaryTime (scaled below it). turret.cpp reads
/// [11..14].
/// </summary>
extern float WeaponFireModifiers[30];
/// <summary>Seconds a target must hold still for the full stationary modifier.</summary>
extern float MaxStationaryTime;
/// <summary>"GroupOrderGoalOffset" (127).</summary>
extern float GroupOrderGoalOffset;
/// <summary>"MinRangeIncrement" (30).</summary>
extern float MinRangeIncrement;
/// <summary>"MinRangeModIncrement" (10).</summary>
extern float MinRangeModIncrement;
/// <summary>"MaxWeaponRangeMod" (45).</summary>
extern float MaxWeaponRangeMod;
/// <summary>"DisableAttackModifier" (10).</summary>
extern float DisableAttackModifier;
/// <summary>"DisableGunneryModifier" (5).</summary>
extern float DisableGunneryModifier;
/// <summary>"SalvageAttackModifier" (30).</summary>
extern float SalvageAttackModifier;
/// <summary>"PilotingCheckFactor" (1).</summary>
extern float PilotingCheckFactor;
/// <summary>"HitLevel" (10, 20).</summary>
extern int32_t HitLevel[2];
/// <summary>"ClusterSizeSRM": read, then always 2.</summary>
extern int32_t ClusterSizeSrm;
/// <summary>"ClusterSizeLRM": read, then always 5.</summary>
extern int32_t ClusterSizeLrm;
/// <summary>"PilotCheckHalfRate" (5).</summary>
extern float PilotCheckHalfRate;
/// <summary>"AttitudeEffect" (6x6).</summary>
extern uint8_t AttitudeEffect[6][6];
/// <summary>Sensors "BaseSensorRollTarget" (50).</summary>
extern float SensorBaseChance;
/// <summary>Sensors "SensorSkillFactor" (10).</summary>
extern float SensorSkillFactor;
/// <summary>Sensors "BlockingObjectModifier" (-5).</summary>
extern float SensorBlockingObjectModifier;
/// <summary>Sensors "ShutdownMech" (-50).</summary>
extern float SensorShutDownMechModifier;
/// <summary>Sensors "SensorRangeModifier": four (range fraction, modifier) pairs.</summary>
extern float SensorRangeModifier[4][2];
/// <summary>Sensors "SizeModifier": three (tonnage, modifier) pairs.</summary>
extern float SensorSizeModifier[3][2];
/// <summary>Sensors "BlockingTerrainModifiers".</summary>
extern float SensorBlockingTerrain[2];
/// <summary>Skills "Sensor Contact Skill".</summary>
extern float SensorSkill;
/// <summary>"RefitRange".</summary>
extern float RefitRange;
/// <summary>Skills "Skill Attempt", per skill.</summary>
extern float SkillTry[4];
/// <summary>Refit costs, [armor, internal structure, ammo][refit vehicle, refit bay] ("RefitVehicleArmorCost",
/// "RefitBayArmorCost", ...).</summary>
extern float RefitCostArray[3][2];
/// <summary>"RefitTime".</summary>
extern float RefitTime;
/// <summary>"RefitAmount".</summary>
extern float RefitAmount;
/// <summary>"LongRangeMovementEnabled", as booleans (the FIT value 1).</summary>
extern int32_t LongRangeMovementEnabled[3];
/// <summary>Skills "Skill Success", per skill.</summary>
extern float SkillSuccess[4];
/// <summary>"AimedFireHitTable".</summary>
extern int32_t AimedFireHitTable[3];
/// <summary>"AimedFireAbort".</summary>
extern int32_t AimedFireAbort;
/// <summary>Skills "KillSkillValues".</summary>
extern float KillSkill[6];
/// <summary>Skills "WeaponHit".</summary>
extern float WeaponHit;
/// <summary>Warrior "JumpSkillMod".</summary>
extern int32_t PilotJumpMod;
/// <summary>Warrior "SkillIncreaseCap".</summary>
extern int32_t IncreaseCap;

/// <summary>
/// Reads the movement, combat, damage, warrior, sensor and skill settings of the game system file ("Pathfinding",
/// "OptimumRange", "Mover:General", "Mover:FireWeapon", "Mover:Damage", "Components", "Warrior", "Sensors",
/// "Skills"). <paramref name="maxVisualRange"/> is unused.
/// </summary>
/// <returns>0, or the FitIniFile error of the first entry missing (most stop the reading).</returns>
int32_t LoadMoverGameSystem(MCFitIniFile* sysFile, float maxVisualRange);

/// <summary>Cells along a side of <see cref="GoalMap"/>: 13 tiles around the goal's.</summary>
constexpr int32_t GOALMAP_CELL_DIM = 39;
/// <summary>Mover::calcMoveGoal's scores of the cells around a move goal.</summary>
extern int32_t GoalMap[GOALMAP_CELL_DIM][GOALMAP_CELL_DIM];

/// <summary>Why the last StatusChunk::pack or unpack found its target bad (0 none; 1-2 mover, 3 terrain object, 4
/// train car, 5 bad type).</summary>
extern int32_t StatusChunkUnpackErr;

/// <summary>
/// Writes a mover's two status chunks, field by field, to ChunkDebugMsg and the file "stchunk.dbg", and points the
/// crash report at them (StatusChunk::equalTo calls it on a mismatch).
/// </summary>
void DebugStatusChunk(MCMover* mover, MCStatusChunk* chunk1, MCStatusChunk* chunk2);

/// <summary>The mover with part id <paramref name="partId"/> (from MoverRoster), or null.</summary>
MCMover* GetMoverFromPartId(int32_t partId);
