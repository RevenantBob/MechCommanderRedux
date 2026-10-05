#pragma once

#include "ai/move.h"
#include "ai/tacordr.h"
#include "object/gameobj.h"

class Appearance;
class Control;
class Dynamics;
class FitIniFile;
class MechWarrior;
class Mover;
class MoverGroup;
class ObjectEvent;
class SensorSystem;
class SortList;
class Team;
class WeaponFireChunk;
struct _SystemTracker;
enum RadioMessageType : int32_t;

/// <summary>Entries of <c>MoverRoster</c>: movers have part ids 0x200..0xfff.</summary>
constexpr int32_t MAX_MOVER_PART_ID = 0x1000;
/// <summary>Weapon fire, critical hit chunks a mover queues per direction.</summary>
constexpr int32_t MAX_WEAPONFIRE_CHUNKS = 0x80;
/// <summary>Radio chunks a mover queues per direction.</summary>
constexpr int32_t MAX_RADIO_CHUNKS = 7;
/// <summary>Path cells a mover's range lock can hold (the original doesn't check it against the range).</summary>
constexpr int32_t MAX_PATH_RANGE_LOCKS = 10;

/// <summary>
/// A mover's state as a network status chunk: body state, target and orders. Packed into <see cref="data"/> to
/// send.
/// </summary>
/// <remarks>Original source: <c>object\mover.cpp</c> (init inline in <c>object\mover.h</c>); 0x2c bytes. Field names
/// follow MechCommander 2's StatusChunk, which kept this layout.</remarks>
class StatusChunk
{
public:
    /// <summary>Clears everything; no target cell.</summary>
    /// <remarks>MCX.EXE @ 0x0065b5d0 (inline in <c>object\mover.h</c>)</remarks>
    void init();
    /// <summary>Takes the mover's state.</summary>
    /// <remarks>MCX.EXE @ 0x006854c0</remarks>
    virtual void build(Mover* mover);
    /// <summary>Packs the fields into <see cref="data"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006854d0</remarks>
    virtual void pack(Mover* mover);
    /// <summary>Unpacks <see cref="data"/> into the fields.</summary>
    /// <remarks>MCX.EXE @ 0x00685630</remarks>
    virtual void unpack(Mover* mover);
    /// <summary>Whether two chunks carry the same state.</summary>
    /// <remarks>MCX.EXE @ 0x006857e0</remarks>
    int equalTo(StatusChunk* chunk);

    /// <summary>The mover's body state.</summary>
    uint32_t bodyState = 0; // +0x04
    /// <summary>What the target is (mover, terrain object, train car, location...).</summary>
    uint8_t targetType = 0; // +0x08
    /// <summary>The target's part id.</summary>
    int32_t targetId = 0; // +0x0c
    /// <summary>A terrain target's block, or a train target's train.</summary>
    int32_t targetBlockOrTrainNumber = 0; // +0x10
    /// <summary>A terrain target's vertex, or a train target's car.</summary>
    int32_t targetVertexOrCarNumber = 0; // +0x14
    /// <summary>A terrain target's item on the vertex.</summary>
    uint8_t targetItemNumber = 0; // +0x18
    /// <summary>A location target's map cell (row, column); -1 for none.</summary>
    int16_t targetCellRC[2] = {-1, -1}; // +0x1a
    /// <summary>Whether the pilot was ordered to eject.</summary>
    int32_t ejectOrderGiven = 0; // +0x20
    /// <summary>A jump order.</summary>
    int32_t jumpOrder = 0; // +0x24
    /// <summary>The packed chunk.</summary>
    uint32_t data = 0; // +0x28
};

/// <summary>A body location's critical space: the inventory item in it ("Component%d").</summary>
struct CriticalSpace
{
    /// <summary>The inventory index, 0xff for an empty space.</summary>
    uint8_t inventoryID = 0; // +0x00
    /// <summary>The second byte of the "Component%d" entry.</summary>
    int32_t hit = 0; // +0x04
};

/// <summary>One of a mover's body locations (a mech's head, torso, arms, legs; a vehicle's sides).</summary>
/// <remarks>0x14 bytes. Field meanings are
/// settled with mech.cpp.</remarks>
struct BodyLocation
{
    /// <summary>"CASE".</summary>
    int32_t hasCASE = 0; // +0x00
    /// <summary>The critical spaces the location's components need (their criticalSpacesReq, summed per space).</summary>
    int32_t totalSpaces = 0; // +0x04
    /// <summary>The location's critical spaces.</summary>
    CriticalSpace* criticalSpaces = nullptr; // +0x08
    float curInternalStructure = 0;          // +0x0c
    /// <summary>"HotSpotNumber".</summary>
    uint8_t hotSpotNumber = 0; // +0x10
    /// <summary>The type's internal structure for the location.</summary>
    uint8_t maxInternalStructure = 0; // +0x11
    /// <summary>2 when destroyed (BattleMech::calcLegStatus).</summary>
    uint8_t damageState = 0; // +0x12
};

/// <summary>One of a mover's inventory items (a component, weapon or ammo bin).</summary>
/// <remarks>0x1c bytes. Field meanings beyond
/// these are settled with mover.cpp.</remarks>
struct InventoryItem
{
    /// <summary>The item's master component id.</summary>
    uint8_t masterID = 0; // +0x00
    /// <summary>Hits it has taken (MasterComponent::health minus this is what getInventoryDamage gives).</summary>
    uint8_t health = 0; // +0x01
    /// <summary>Nonzero when destroyed or disabled.</summary>
    int32_t disabled = 0; // +0x04
    /// <summary>"FacesForward" (weapons).</summary>
    uint8_t facesForward = 0; // +0x08
    /// <summary>An ammo bin's starting rounds.</summary>
    int16_t startAmount = 0; // +0x0a
    /// <summary>An ammo bin's rounds (calcAmmoTotals sums them per type).</summary>
    int16_t amount = 0; // +0x0c
    /// <summary>A weapon's (or anti-missile system's) ammo type: its index in Mover::ammoTypeTotal.</summary>
    int16_t ammoIndex = 0; // +0x0e
    /// <summary>Scenario time a weapon is ready again (startWeaponRecycle).</summary>
    float readyTime = 0; // +0x10
    /// <summary>The body location the item sits in (an ammo explosion hits it).</summary>
    uint8_t bodyLocation = 0; // +0x14
    /// <summary>A weapon's effectiveness (calcWeaponEffectiveness sums it, scaled by gunnery).</summary>
    int16_t effectiveness = 0; // +0x16
    /// <summary>A weapon's ratings per range step (NumRangeRatings pairs: rating, then damage rate; objectCache,
    /// freed with the inventory).</summary>
    float* rangeRatings = nullptr; // +0x18
};

/// <summary>One of a mover's armor locations.</summary>
/// <remarks>8 bytes.</remarks>
struct ArmorLocation
{
    /// <summary>Armor left.</summary>
    float curArmor = 0; // +0x00
    /// <summary>Full armor (needsRefit).</summary>
    uint8_t maxArmor = 0; // +0x04
};

/// <summary>A mover's ammo of one type.</summary>
/// <remarks>0x0c bytes.</remarks>
struct AmmoTally
{
    /// <summary>The ammo's master component id.</summary>
    int32_t masterId = 0; // +0x00
    /// <summary>Rounds left.</summary>
    int32_t curAmount = 0; // +0x04
    /// <summary>Rounds at the start.</summary>
    int32_t startAmount = 0; // +0x08
};

/// <summary>
/// Anything that moves under a pilot: mechs, ground vehicles, elementals. Holds the inventory, sensors and weapons,
/// the pilot, control and dynamics, path locks, and the network chunks.
/// </summary>
/// <remarks>Original source: <c>object\mover.cpp</c>, <c>object\mover.h</c>; 0x8a0 bytes. Field names are the
/// port's where the original's aren't known; many follow MechCommander 2's Mover.</remarks>
class Mover : public BigGameObject
{
public:
    /// <summary>The original's vector deleting destructor calls destroy.</summary>
    /// <remarks>MCX.EXE @ 0x0065bb70</remarks>
    ~Mover() override { destroy(); }
    /// <summary>The constructor calls init (inlined in the derived types' createInstance).</summary>
    Mover()
    {
        frame.reset_to_world_frame();
        init();
    }

    using BigGameObject::init;
    /// <summary>Resets every field (the constructor's work); counts the mover and makes the shared sort list.</summary>
    /// <remarks>MCX.EXE @ 0x006858c0</remarks>
    void init() override;
    /// <summary>Frees the inventory, sensors, jammer/ECM trackers, potential contact, appearance, control and
    /// dynamics; the last mover frees the sort list.</summary>
    /// <remarks>MCX.EXE @ 0x006861e0</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x0065b7a0</remarks>
    Appearance* getAppearance() override { return appearance; }
    /// <remarks>MCX.EXE @ 0x00686b10</remarks>
    int32_t handleEvent(ObjectEvent* event) override;
    /// <summary>Zero.</summary>
    /// <remarks>MCX.EXE @ 0x0065bb00</remarks>
    vector_3d getPositionFromHS(uint32_t hotSpot) override;
    /// <remarks>MCX.EXE @ 0x0065bb40</remarks>
    int underPlayerControl() override { return 1; }
    /// <summary>The group's id, -1 for none.</summary>
    /// <remarks>MCX.EXE @ 0x0068ce00</remarks>
    int32_t getGroupId() override;
    /// <summary>Wakes or puts to sleep; a shut-down mover's pilot is ordered to power up when woken.</summary>
    /// <remarks>MCX.EXE @ 0x00685d80</remarks>
    void setAwake(int awake) override;
    /// <summary>Sets the part id and registers the mover in MoverRoster.</summary>
    /// <remarks>MCX.EXE @ 0x00685be0</remarks>
    void setPartId(int32_t newPartId) override;
    /// <remarks>MCX.EXE @ 0x00687290</remarks>
    void reduceAntiMissileAmmo(int32_t numShots) override;
    /// <remarks>MCX.EXE @ 0x006872e0</remarks>
    int32_t fireAntiMissileSystem(int32_t numMissiles, int32_t& antiMissileShots) override;
    /// <remarks>MCX.EXE @ 0x00686430</remarks>
    vector_3d relativePosition(float angle, float distance, uint32_t flags) override;
    /// <remarks>MCX.EXE @ 0x00685c30</remarks>
    void setPosition(vector_3d& newPosition) override;
    /// <remarks>MCX.EXE @ 0x0065b600</remarks>
    vector_3d getVelocity() override { return velocity; }
    /// <remarks>MCX.EXE @ 0x0065b630</remarks>
    void setVelocity(vector_3d& newVelocity) override { velocity = newVelocity; }
    /// <remarks>MCX.EXE @ 0x0065b660</remarks>
    frame_of_ref getFrame() override { return frame; }
    /// <remarks>MCX.EXE @ 0x0065b6d0</remarks>
    void setFrame(frame_of_ref& newFrame) override { frame = newFrame; }
    /// <summary>Sets the alignment, and the pilot's.</summary>
    /// <remarks>MCX.EXE @ 0x0065b850</remarks>
    void setAlignment(int32_t newAlignment) override;
    /// <remarks>MCX.EXE @ 0x00687570</remarks>
    void setCommanderId(int32_t newCommanderId) override { commanderId = static_cast<int8_t>(newCommanderId); }
    /// <remarks>MCX.EXE @ 0x0065b880</remarks>
    int32_t getCommanderId() override { return commanderId; }
    /// <remarks>MCX.EXE @ 0x0065b730</remarks>
    int lineOfSight(GameObject* target) override;
    /// <remarks>MCX.EXE @ 0x0065b770</remarks>
    int lineOfSight(vector_3d point) override;
    /// <remarks>MCX.EXE @ 0x00686940</remarks>
    int lineOfFire(GameObject* target) override;
    /// <remarks>MCX.EXE @ 0x00685e80</remarks>
    float relFacingTo(vector_3d goal, int32_t bodyPart) override;
    /// <remarks>MCX.EXE @ 0x0065ba10</remarks>
    float relViewFacingTo(vector_3d goal) override;
    /// <summary>Deselecting keeps the selection a second longer (unless networked); selecting sets it.</summary>
    /// <remarks>MCX.EXE @ 0x0068ceb0</remarks>
    void setSelected(int32_t newSelected) override;
    /// <summary>Whether the pilot's current order is a withdraw (0x13).</summary>
    /// <remarks>MCX.EXE @ 0x0068cde0</remarks>
    int isWithdrawing() override;
    /// <remarks>MCX.EXE @ 0x0065b890</remarks>
    MechWarrior* getPilot() override { return pilot; }
    /// <remarks>MCX.EXE @ 0x00687d40</remarks>
    int isRevealed() override;
    /// <summary>Joins <paramref name="newTeam"/>: sensors, jammer and ECM follow.</summary>
    /// <remarks>MCX.EXE @ 0x00687430</remarks>
    int32_t setTeam(Team* newTeam) override;
    /// <remarks>MCX.EXE @ 0x0068ce20</remarks>
    int32_t getVitalInfo(void* vitalInfo) override;

    // Slots 116.. are Mover's own.

    /// <summary>Part id 0x200 + (commander * 32 + group) * 12 + index.</summary>
    /// <remarks>MCX.EXE @ 0x00685c00</remarks>
    virtual void setPartId(int32_t commanderId, int32_t groupId, int32_t index);
    /// <remarks>MCX.EXE @ 0x0065b6c0</remarks>
    virtual int32_t getSpeedState() { return 0; }
    /// <summary>The slope under the mover, in degrees.</summary>
    /// <remarks>MCX.EXE @ 0x00686040</remarks>
    virtual float getTerrainAngle();
    /// <remarks>MCX.EXE @ 0x00686060</remarks>
    virtual float getVelocityTilt();
    /// <remarks>MCX.EXE @ 0x006869e0</remarks>
    virtual int lineOfFire(vector_3d point);
    /// <remarks>MCX.EXE @ 0x00686a50</remarks>
    virtual void lineOfSensor(GameObject* target, int32_t& sensorResult, int32_t& losResult);
    /// <summary>Gives <paramref name="tacOrder"/> to the pilot (networked: through the server).</summary>
    /// <remarks>MCX.EXE @ 0x00686ca0 (unnamed in Ghidra; MechCommander 2's name)</remarks>
    virtual int32_t handleTacticalOrder(TacticalOrder tacOrder, int32_t priority, int queuePlayerOrder);
    /// <remarks>MCX.EXE @ 0x006873c0</remarks>
    virtual void pilotingCheck(uint32_t situation, float modifier);
    /// <remarks>MCX.EXE @ 0x0065b7b0</remarks>
    virtual void forcePilotingCheck();
    /// <remarks>MCX.EXE @ 0x0065b7d0</remarks>
    virtual int canFireWeapons() { return 1; }
    /// <summary>Every DamageRateFrequency seconds: over 10 damage since the last check alarms the pilot.</summary>
    /// <remarks>MCX.EXE @ 0x006873d0</remarks>
    virtual void updateDamageTakenRate();
    /// <summary>Returns 4 (no action). Unnamed in Ghidra; the name is MechCommander 2's.</summary>
    /// <remarks>MCX.EXE @ 0x0065b7e0</remarks>
    virtual int32_t checkShortRangeCollision() { return 4; }
    /// <remarks>MCX.EXE @ 0x0065b7f0</remarks>
    virtual void setOverlayWeightClass(int32_t overlayClass) { overlayWeightClass = overlayClass; }
    /// <remarks>MCX.EXE @ 0x0065b800</remarks>
    virtual int32_t getOverlayWeightClass() { return overlayWeightClass; }
    /// <remarks>MCX.EXE @ 0x0065b810</remarks>
    virtual void getStopInfo(float& stopHeading, float& stopVelocity) {}
    /// <summary>Whether a mover's path has locked the cell, or (diameter 3) any of the 3x3 cells around it;
    /// diameter 5 is never locked.</summary>
    /// <remarks>MCX.EXE @ 0x00689650</remarks>
    virtual int getPathLocked(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, int32_t diameter);
    /// <summary>Locks or unlocks the cell, or (diameter 3) the 3x3 cells around it; diameter 5 does nothing.</summary>
    /// <remarks>MCX.EXE @ 0x0068a170</remarks>
    virtual void setPathLock(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, int set, int32_t diameter);
    /// <summary>Whether the cell's neighbour in <paramref name="dir"/> (adjCellTable) is path locked.</summary>
    /// <remarks>MCX.EXE @ 0x006895e0</remarks>
    virtual int getAdjacentCellPathLocked(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, int32_t dir);
    /// <summary>Locks (or unlocks) the mover's cell and its path's next cells.</summary>
    /// <remarks>MCX.EXE @ 0x0068ae10</remarks>
    virtual void updatePathLock(int set);
    /// <remarks>MCX.EXE @ 0x0068abc0</remarks>
    virtual int getPathRangeLock(int32_t range, int* reachedEnd);
    /// <remarks>MCX.EXE @ 0x0068abf0</remarks>
    virtual int32_t setPathRangeLock(int set, int32_t range);
    /// <remarks>MCX.EXE @ 0x0068aea0</remarks>
    virtual int getPathRangeBlocked(int32_t range, int* reachedEnd);
    /// <remarks>MCX.EXE @ 0x0065b820</remarks>
    virtual int crashAvoidanceSystem() { return 0; }
    /// <remarks>MCX.EXE @ 0x0068aed0</remarks>
    virtual void updateHustleTime();
    /// <remarks>MCX.EXE @ 0x0065b830</remarks>
    virtual void mineCheck() {}
    /// <remarks>MCX.EXE @ 0x0065b840</remarks>
    virtual void updateMovement() {}
    /// <remarks>MCX.EXE @ 0x0068af40</remarks>
    virtual int32_t bounceToAdjCell();
    /// <remarks>MCX.EXE @ 0x006874f0</remarks>
    virtual int32_t setGroup(MoverGroup* newGroup);
    /// <remarks>MCX.EXE @ 0x00687530</remarks>
    virtual void setPilot(MechWarrior* newPilot);
    /// <remarks>MCX.EXE @ 0x00687760</remarks>
    virtual int32_t updateWeaponFireChunks(int32_t which);
    /// <remarks>MCX.EXE @ 0x00687b70</remarks>
    virtual int32_t updateCriticalHitChunks(int32_t which);
    /// <remarks>MCX.EXE @ 0x00687c90</remarks>
    virtual int32_t updateRadioChunks(int32_t which);
    /// <remarks>MCX.EXE @ 0x0065b8a0</remarks>
    virtual StatusChunk* getStatusChunk() { return &statusChunk; }
    /// <remarks>MCX.EXE @ 0x0065b8b0</remarks>
    virtual int32_t buildStatusChunk() { return 0; }
    /// <remarks>MCX.EXE @ 0x0065b8c0</remarks>
    virtual int32_t handleStatusChunk(int32_t updateAge, uint32_t chunk) { return 0; }
    /// <remarks>MCX.EXE @ 0x0065b8d0</remarks>
    virtual MoveChunk* getMoveChunk() { return &moveChunk; }
    /// <remarks>MCX.EXE @ 0x0065b8e0</remarks>
    virtual int32_t buildMoveChunk() { return 0; }
    /// <remarks>MCX.EXE @ 0x0065b8f0</remarks>
    virtual int32_t handleMoveChunk(uint32_t chunk) { return 0; }
    /// <remarks>MCX.EXE @ 0x0065b900</remarks>
    virtual int32_t calcCV(int calcMax) { return 0; }
    /// <remarks>MCX.EXE @ 0x00687ea0</remarks>
    virtual void getDamageClass(int32_t& damageClass, int& shutDown);
    /// <remarks>MCX.EXE @ 0x0065b910</remarks>
    virtual int32_t getBodyState() { return -1; }
    /// <remarks>MCX.EXE @ 0x0065b920</remarks>
    virtual float getTotalEffectiveness() { return 0.0f; }
    /// <remarks>MCX.EXE @ 0x00688090</remarks>
    virtual int32_t calcOffsetMoveGoal(vector_3d target, vector_3d offset, vector_3d& goal);
    /// <remarks>MCX.EXE @ 0x00688310</remarks>
    virtual int32_t calcMoveGoal(GameObject* target, vector_3d moveGoal, int32_t isGroup, int32_t offsetIndex,
                                 int32_t groupSize, int32_t pointIndex, vector_3d& newGoal, uint32_t params);
    /// <remarks>MCX.EXE @ 0x0068b110</remarks>
    virtual int32_t calcMovePath(MovePath* path, vector_3d start, int32_t thruArea, int32_t goalDoor,
                                 vector_3d finalGoal, vector_3d* goal, int32_t* goalCell, uint32_t params);
    /// <remarks>MCX.EXE @ 0x00688f60</remarks>
    virtual int32_t calcMovePath(MovePath* path, int32_t pathType, vector_3d start, vector_3d goal, int32_t* goalCell,
                                 uint32_t params);
    /// <remarks>MCX.EXE @ 0x00689360</remarks>
    virtual int32_t calcEscapePath(MovePath* path, vector_3d start, vector_3d goal, int32_t* goalCell, uint32_t params,
                                   vector_3d& escapeGoal);
    /// <remarks>MCX.EXE @ 0x0068b340</remarks>
    virtual int32_t getContacts(int32_t* contactList, int32_t contactCriteria, int32_t sortType);
    /// <summary>The weapon's facing to <paramref name="targetPosition"/>: relFacingTo (slot 58) of the point.
    /// Unnamed in Ghidra; the subclasses name it (their mangled names return float).</summary>
    /// <remarks>MCX.EXE @ 0x0068b360</remarks>
    virtual float weaponLocked(int32_t weaponIndex, vector_3d targetPosition);
    /// <remarks>MCX.EXE @ 0x0068b390</remarks>
    virtual int32_t weaponInRange(int32_t weaponIndex, float metersToTarget);
    /// <remarks>MCX.EXE @ 0x0068b420</remarks>
    virtual int32_t getWeaponsReady(int32_t* list, int32_t listSize);
    /// <remarks>MCX.EXE @ 0x0068b4f0</remarks>
    virtual int32_t getWeaponsLocked(int32_t* list, int32_t listSize);
    /// <remarks>MCX.EXE @ 0x0068b660</remarks>
    virtual int32_t getWeaponsInRange(int32_t* list, int32_t listSize, float orderFireRange);
    /// <remarks>MCX.EXE @ 0x0068b760</remarks>
    virtual int32_t getWeaponShots(int32_t weaponIndex);
    /// <remarks>MCX.EXE @ 0x0068b7d0</remarks>
    virtual float getWeaponAmmoLevel(int32_t weaponIndex);
    /// <remarks>MCX.EXE @ 0x0068b820</remarks>
    virtual void calcWeaponEffectiveness(int setMax);
    /// <remarks>MCX.EXE @ 0x0068b950</remarks>
    virtual void calcWeaponRangeRatings();
    /// <remarks>MCX.EXE @ 0x0068ba50</remarks>
    virtual void calcAmmoTotals();
    /// <remarks>MCX.EXE @ 0x0068bc40</remarks>
    virtual int calcOptimalRange(GameObject* target);
    /// <remarks>MCX.EXE @ 0x0068bf70</remarks>
    virtual int32_t calcLongestRangeWeapon();
    /// <remarks>MCX.EXE @ 0x0068c090</remarks>
    virtual float getFireRange(int32_t which);
    /// <summary>getFireRange(-2).</summary>
    /// <remarks>MCX.EXE @ 0x0068c150</remarks>
    virtual float getMaxFireRange();
    /// <remarks>MCX.EXE @ 0x0068c160</remarks>
    virtual int isWeaponIndex(int32_t itemIndex);
    /// <remarks>MCX.EXE @ 0x0068c1e0</remarks>
    virtual int isWeaponReady(int32_t weaponIndex);
    /// <remarks>MCX.EXE @ 0x0068c230</remarks>
    virtual int isWeaponWorking(int32_t weaponIndex);
    /// <remarks>MCX.EXE @ 0x0068c1a0</remarks>
    virtual int isWeaponMissile(int32_t weaponIndex);
    /// <summary>Sets the weapon's ready time: now plus its recycle time. Unnamed in Ghidra; MechCommander 2's
    /// name.</summary>
    /// <remarks>MCX.EXE @ 0x0068c270</remarks>
    virtual void startWeaponRecycle(int32_t weaponIndex);
    /// <remarks>MCX.EXE @ 0x0068c2b0</remarks>
    virtual int32_t tallyAmmo(int32_t ammoMasterId);
    /// <remarks>MCX.EXE @ 0x0068c3f0</remarks>
    virtual int32_t reduceAmmo(int32_t ammoMasterId, int32_t amount);
    /// <remarks>MCX.EXE @ 0x0065b930</remarks>
    virtual int32_t getNumAmmoTypes() { return numAmmoTypes; }
    /// <remarks>MCX.EXE @ 0x0065b940</remarks>
    virtual int32_t getAmmoType(int32_t ammoTypeIndex) { return ammoTypeTotal[ammoTypeIndex].masterId; }
    /// <remarks>MCX.EXE @ 0x0065b960</remarks>
    virtual int32_t getAmmoTypeTotal(int32_t ammoTypeIndex) { return ammoTypeTotal[ammoTypeIndex].curAmount; }
    /// <remarks>MCX.EXE @ 0x0065b980</remarks>
    virtual int32_t getAmmoTypeStart(int32_t ammoTypeIndex) { return ammoTypeTotal[ammoTypeIndex].startAmount; }
    /// <remarks>MCX.EXE @ 0x0068c4f0</remarks>
    virtual void deductWeaponShot(int32_t weaponIndex, int32_t ammoAmount);
    /// <remarks>MCX.EXE @ 0x0068c310</remarks>
    virtual int needsRefit(int armorOnly);
    /// <remarks>MCX.EXE @ 0x0068c540</remarks>
    virtual int32_t sortWeapons(int32_t* weaponList, int32_t* valueList, int32_t listSize, int32_t sortType,
                                int skillCheck);
    /// <remarks>MCX.EXE @ 0x0068c770</remarks>
    virtual float calcAttackChance(GameObject* target, int32_t aimLocation, float targetTime, int32_t weaponIndex,
                                   float modifiers, int32_t* range, vector_3d* targetPoint);
    /// <remarks>MCX.EXE @ 0x0065b9a0</remarks>
    virtual float calcAttackModifier(GameObject* target, int32_t weaponIndex, int skillCheck) { return 0.0f; }
    /// <remarks>MCX.EXE @ 0x0065b9b0</remarks>
    virtual int hitInventoryItem(int32_t itemIndex, int setupOnly) { return 0; }
    /// <summary>
    /// Disables the mover: the pilot is alarmed; an enemy mech may leave salvage (MechSalvageChance) or be blown
    /// apart. Unnamed in Ghidra; GroundVehicle names its override disable.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0068cc30</remarks>
    virtual void disable(uint32_t cause);
    /// <remarks>MCX.EXE @ 0x0068cd80</remarks>
    virtual void shutDown();
    /// <remarks>MCX.EXE @ 0x0068cdb0</remarks>
    virtual void startUp();
    /// <remarks>MCX.EXE @ 0x0065b9c0</remarks>
    virtual void destroyBodyLocation(int32_t location) {}
    /// <remarks>MCX.EXE @ 0x0065b9d0</remarks>
    virtual void calcCriticalHit(int32_t hitLocation) {}
    /// <remarks>MCX.EXE @ 0x0065b9e0</remarks>
    virtual int injureBodyLocation(int32_t bodyLocation, float damage) { return 0; }
    /// <remarks>MCX.EXE @ 0x0068cab0</remarks>
    virtual void ammoExplosion(int32_t ammoIndex);
    /// <remarks>MCX.EXE @ 0x0065b9f0</remarks>
    virtual int32_t fireWeapon(GameObject* target, float targetTime, int32_t weaponIndex, int32_t attackType,
                               int32_t aimLocation, vector_3d* targetPoint)
    {
        return 0;
    }

    /// <remarks>MCX.EXE @ 0x0065ba00</remarks>
    virtual int32_t handleWeaponFire(int32_t weaponIndex, GameObject* target, vector_3d* targetPoint, int hit,
                                     float entryAngle, int32_t numMissiles, int32_t missilesPastAMS,
                                     int32_t antiMissileShots, int32_t hitLocation)
    {
        return 0;
    }

    /// <remarks>MCX.EXE @ 0x00685dc0</remarks>
    virtual float relFacingDelta(vector_3d goalPos, vector_3d targetPos);
    /// <remarks>MCX.EXE @ 0x0065ba40</remarks>
    virtual int canPowerUp() { return 1; }
    /// <remarks>MCX.EXE @ 0x0065ba50</remarks>
    virtual int canMove() { return 1; }
    /// <remarks>MCX.EXE @ 0x0065ba60</remarks>
    virtual int canJump() { return 0; }
    /// <remarks>MCX.EXE @ 0x0065ba70</remarks>
    virtual float getJumpRange(int32_t* numOffsets, int32_t* jumpCost);
    /// <remarks>MCX.EXE @ 0x0065baa0</remarks>
    virtual int isJumping(vector_3d* jumpGoal) { return 0; }
    /// <summary>The firing arc of the mover's class (mech FireArc, vehicle, elemental; else 60).</summary>
    /// <remarks>MCX.EXE @ 0x006861b0</remarks>
    virtual float getFireArc();
    /// <remarks>MCX.EXE @ 0x0065bab0</remarks>
    virtual float calcMaxSpeed() { return 0.0f; }
    /// <remarks>MCX.EXE @ 0x0065bac0</remarks>
    virtual float calcSlowSpeed() { return 0.0f; }
    /// <summary>Unnamed in Ghidra; BattleMech names its override calcModerateSpeed.</summary>
    /// <remarks>MCX.EXE @ 0x0065bad0</remarks>
    virtual float calcModerateSpeed() { return 0.0f; }
    /// <remarks>MCX.EXE @ 0x0065bae0</remarks>
    virtual int32_t calcSpriteSpeed(float speed, uint32_t flags, int32_t& state, int32_t& throttle);
    /// <remarks>MCX.EXE @ 0x0065bb20</remarks>
    virtual float getGestureStopDistance() { return 0.0f; }
    /// <remarks>MCX.EXE @ 0x0065bb30</remarks>
    virtual int handleEjection() { return 0; }
    /// <remarks>MCX.EXE @ 0x0065bb50</remarks>
    virtual float calcExpectedTargetDamage() { return 0.0f; }
    /// <remarks>MCX.EXE @ 0x0065bb60</remarks>
    virtual const char* getIfaceName() { return "No Name"; }

    /// <remarks>MCX.EXE @ 0x0066dee0 (inline in <c>object\mover.h</c>)</remarks>
    void setLastValidPosition(vector_3d pos) { lastValidPosition = pos; }
    /// <summary>The group's point, or null without a group. Unnamed in Ghidra; MechCommander 2's name.</summary>
    /// <remarks>MCX.EXE @ 0x00687580</remarks>
    Mover* getPoint();
    /// <remarks>MCX.EXE @ 0x006875a0</remarks>
    int32_t clearWeaponFireChunks(int32_t which);
    /// <remarks>MCX.EXE @ 0x006875c0</remarks>
    int32_t addWeaponFireChunk(int32_t which, WeaponFireChunk* chunk);
    /// <remarks>MCX.EXE @ 0x00687630</remarks>
    int32_t addWeaponFireChunks(int32_t which, uint32_t* packedChunkBuffer, int32_t numChunks);
    /// <remarks>MCX.EXE @ 0x00687710</remarks>
    int32_t grabWeaponFireChunks(int32_t which, uint32_t* packedChunkBuffer, int32_t maxChunks);
    /// <remarks>MCX.EXE @ 0x00687a30</remarks>
    int32_t clearCriticalHitChunks(int32_t which);
    /// <remarks>MCX.EXE @ 0x00687a50</remarks>
    int32_t addCriticalHitChunk(int32_t which, int32_t bodyLocation, int32_t criticalSpace);
    /// <remarks>MCX.EXE @ 0x00687ab0</remarks>
    int32_t addCriticalHitChunks(int32_t which, uint8_t* packedChunkBuffer, int32_t numChunks);
    /// <remarks>MCX.EXE @ 0x00687b30</remarks>
    int32_t grabCriticalHitChunks(int32_t which, uint8_t* packedChunkBuffer);
    /// <remarks>MCX.EXE @ 0x00687b90</remarks>
    int32_t clearRadioChunks(int32_t which);
    /// <remarks>MCX.EXE @ 0x00687bb0</remarks>
    int32_t addRadioChunk(int32_t which, uint8_t msg);
    /// <remarks>MCX.EXE @ 0x00687c00</remarks>
    int32_t addRadioChunks(int32_t which, uint8_t* packedChunkBuffer, int32_t numChunks);
    /// <remarks>MCX.EXE @ 0x00687c50</remarks>
    int32_t grabRadioChunks(int32_t which, uint8_t* packedChunkBuffer);
    /// <remarks>MCX.EXE @ 0x00687d20</remarks>
    void playMessage(RadioMessageType messageId, int propogateIfMultiplayer);
    /// <remarks>MCX.EXE @ 0x00687df0</remarks>
    int enemyRevealed();
    /// <remarks>MCX.EXE @ 0x00687f40</remarks>
    int32_t getInventoryDamage(int32_t itemIndex);
    /// <summary>The ECM's effect, 0 without a working ECM.</summary>
    /// <remarks>MCX.EXE @ 0x00687fa0</remarks>
    float getEcmEffect();
    /// <remarks>MCX.EXE @ 0x00687ff0</remarks>
    float getProbeEffect();
    /// <summary>MaxVisualRadius plus the probe's effect.</summary>
    /// <remarks>MCX.EXE @ 0x00688040</remarks>
    float getVisualRange();
    /// <summary>The mover challenging this one; dropped once disabled.</summary>
    /// <remarks>MCX.EXE @ 0x00688060</remarks>
    GameObject* getChallenger();
    /// <summary>Unnamed in Ghidra; MechCommander 2's name.</summary>
    /// <remarks>MCX.EXE @ 0x00688050</remarks>
    void setChallenger(GameObject* newChallenger);

    /// <summary>The ground's normal under the mover (getTerrainAngle, getVelocityTilt).</summary>
    vector_3d terrainNormal; // +0x88
    /// <summary>Velocity in meters per second.</summary>
    vector_3d velocity; // +0x94
    /// <summary>Orientation.</summary>
    frame_of_ref frame; // +0xa0
    /// <summary>The mover's long name (the string table entry of its profile; GroundVehicle's
    /// getIfaceName returns it). Also printed by the debug status chunks.</summary>
    std::string debugStatus; // +0xc4
    /// <summary>Profile "icon" (up to 19 characters).</summary>
    char iconName[20] = {}; // +0xc8
    /// <summary>The type's "Chassis"; indexes TileThrottleMultiplier / OverlayThrottleMultiplier.</summary>
    uint8_t chassis = 0; // +0xdc
    /// <summary>The type's "EndoSteel" (mechs).</summary>
    int32_t endoSteel = 0; // +0xe0
    /// <summary>The type's "TonnageClass".</summary>
    float tonnageClass = 0.0f; // +0xe4
    /// <summary>The type's internal structure tonnage.</summary>
    float internalStructureTonnage = 0.0f; // +0xe8
    /// <summary>Damage taken since the last check (updateDamageTakenRate).</summary>
    float damageRateTally = 0.0f; // +0xec
    /// <summary>Scenario time of the next damage rate check.</summary>
    float damageRateCheckTime = 1.0f; // +0xf0
    /// <summary>All the damage taken (BattleMech::handleWeaponHit).</summary>
    float totalDamageTaken = 0.0f; // +0xf4
    /// <summary>
    /// Body location <paramref name="location"/> of <see cref="body"/>. Port-only: debug builds check the index is
    /// one of the <see cref="numBodyLocations"/>, since an armor location (a mech's rear torsos, 8..10) used as a
    /// body location reads past the array. Map one through <c>MechArmorToBodyLocation</c> first.
    /// </summary>
    auto bodyAt(int32_t location) -> BodyLocation&
    {
        SDL_assert(body != nullptr && location >= 0 && location < numBodyLocations);
        return body[location];
    }

    /// <summary>Const <see cref="bodyAt"/>.</summary>
    auto bodyAt(int32_t location) const -> const BodyLocation&
    {
        SDL_assert(body != nullptr && location >= 0 && location < numBodyLocations);
        return body[location];
    }

    /// <summary>The body locations.</summary>
    std::unique_ptr<BodyLocation[]> body; // +0xf8
    /// <summary>How many.</summary>
    int8_t numBodyLocations = 0; // +0xfc
    /// <summary>"Armor" "Type".</summary>
    uint8_t armorType = 0; // +0x104
    /// <summary>"Armor" "Tonnage".</summary>
    float armorTonnage = 0.0f; // +0x108
    /// <summary>The armor locations.</summary>
    std::unique_ptr<ArmorLocation[]> armor; // +0x10c
    /// <summary>How many.</summary>
    int8_t numArmorLocations = 0; // +0x110
    /// <summary>The inventory: other items, then weapons, then ammo.</summary>
    std::unique_ptr<InventoryItem[]> inventory; // +0x114
    /// <summary>Items before the weapons.</summary>
    uint8_t numOther = 0; // +0x118
    /// <summary>Weapons.</summary>
    uint8_t numWeapons = 0; // +0x119
    /// <summary>Ammo bins.</summary>
    uint8_t numAmmos = 0; // +0x11a
    /// <summary>Ammo by type.</summary>
    std::unique_ptr<AmmoTally[]> ammoTypeTotal; // +0x11c
    /// <summary>How many.</summary>
    int8_t numAmmoTypes = 0; // +0x120
    /// <summary>The pilot.</summary>
    MechWarrior* pilot = nullptr; // +0x124
    /// <summary>The sensors (from sensorSystemManager).</summary>
    SensorSystem* sensorSystem = nullptr; // +0x128
    /// <summary>The team's tracker of the mover's jammer.</summary>
    _SystemTracker* jammerTracker = nullptr; // +0x12c
    /// <summary>The team's tracker of the mover's ECM.</summary>
    _SystemTracker* ecmTracker = nullptr; // +0x130
    /// <summary>Inventory index of the cockpit, 0xff for none.</summary>
    uint8_t cockpit = 0xff; // +0x134
    /// <summary>Inventory index of the engine.</summary>
    uint8_t engine = 0xff; // +0x135
    /// <summary>Inventory index of the life support.</summary>
    uint8_t lifeSupport = 0xff; // +0x136
    /// <summary>Inventory index of the sensors.</summary>
    uint8_t sensor = 0xff; // +0x137
    /// <summary>Inventory index of the ECM.</summary>
    uint8_t ecm = 0xff; // +0x138
    /// <summary>Inventory index of the probe.</summary>
    uint8_t probe = 0xff; // +0x139
    /// <summary>Inventory index of the jammer (setTeam adds it to the team's jammers).</summary>
    uint8_t jammer = 0xff; // +0x13a
    /// <summary>The largest minimum range of the working weapons (calcLongestRangeWeapon).</summary>
    float maxMinRange = 0.0f; // +0x13c
    /// <summary>The working weapon with the longest long range, 0xff for none.</summary>
    uint8_t longestRangeWeapon = 0; // +0x140
    /// <summary>The working weapon with the shortest short range, 0xff for none.</summary>
    uint8_t shortestRangeWeapon = 0; // +0x141
    /// <summary>calcWeaponEffectiveness(max).</summary>
    float maxWeaponEffectiveness = 0.0f; // +0x144
    /// <summary>calcWeaponEffectiveness.</summary>
    float weaponEffectiveness = 0.0f; // +0x148
    /// <summary>calcOptimalRange; -1 until known.</summary>
    float optimalRange = -1.0f; // +0x14c
    /// <summary>Anti-missile systems.</summary>
    int8_t numAntiMissileSystems = 0; // +0x150
    /// <summary>Their inventory indices.</summary>
    uint8_t antiMissileSystem[23] = {}; // +0x151
    /// <summary>"Engine" "Tonnage".</summary>
    float engineTonnage = 0.0f; // +0x168
    /// <summary>"Engine" "Rating".</summary>
    uint32_t engineRating = 0; // +0x16c
    /// <summary>When a disabled engine blows (5 s after the critical hit; -1 for never). A mover with one set no
    /// longer moves; destroyBodyLocation checks it too.</summary>
    float engineBlowTime = -1.0f; // +0x170
    /// <summary>Used by calcMovePath and calcEscapePath.</summary>
    /// <summary>"Engine" "MaxRunSpeed".</summary>
    float maxRunSpeed = 0.0f; // +0x178
    /// <summary>Set by shutDown.</summary>
    int32_t shutDownThisFrame = 0; // +0x17c
    /// <summary>Set by startUp.</summary>
    int32_t startUpThisFrame = 0; // +0x180
    /// <summary>Set by disable.</summary>
    int32_t disableThisFrame = 0; // +0x184
    /// <summary>The group (lance).</summary>
    MoverGroup* group = nullptr; // +0x188
    /// <summary>
    /// The mover's rank in its order (SortMoverList, the group's order spread; copied into
    /// TacticalOrder::selectionIndex); -1 by init.
    /// </summary>
    int32_t selectionIndex = -1; // +0x18c
    /// <summary>The commander's id.</summary>
    int8_t commanderId = 0; // +0x194
    /// <summary>-1 by init; forcePilotingCheck raises it to 0.</summary>
    int32_t pilotCheckModifier = -1; // +0x198
    /// <summary>Cleared by pilotingCheck.</summary>
    int32_t pilotingCheckPending = 0; // +0x1a8
    /// <summary>When calcWeaponEffectiveness last ran (the AI recomputes its optimal range against a target whose
    /// effectiveness is newer).</summary>
    float lastWeaponEffectivenessCalc = 0.0f; // +0x1b4
    /// <summary>When calcOptimalRange last ran.</summary>
    float lastOptimalRangeCalc = 0.0f; // +0x1b8
    /// <summary>The mover challenging this one (getChallenger).</summary>
    GameObject* challenger = nullptr; // +0x1bc
    /// <summary>The appearance.</summary>
    Appearance* appearance = nullptr; // +0x1c4
    /// <summary>The control.</summary>
    Control* control = nullptr; // +0x1c8
    /// <summary>The dynamics.</summary>
    Dynamics* dynamics = nullptr; // +0x1cc
    /// <summary>The DPID of the player whose mover this is (multiplayer; set when the scenario starts).</summary>
    uint32_t netOwnerID = 0; // +0x1d0
    /// <summary>Networked: a 256-byte name buffer (string 0xb9); null in single player.</summary>
    std::unique_ptr<char[]> netName; // +0x1d4
    /// <summary>-1 by init; networked: the owning player.</summary>
    int32_t netPlayerId = -1; // +0x1d8
    /// <summary>-1 by init; networked: the roster slot.</summary>
    int32_t netRosterIndex = -1; // +0x1dc
    /// <summary>The network status chunk.</summary>
    StatusChunk statusChunk; // +0x1e0
    /// <summary>Set when a network move chunk arrives; the next update warps to its first step if too far off.</summary>
    int32_t newMoveChunk = 0; // +0x20c
    /// <summary>The network move chunk.</summary>
    MoveChunk moveChunk{}; // +0x210
    /// <summary>Weapon fire chunks queued, out (0) and in (1).</summary>
    int32_t numWeaponFireChunks[2] = {};                      // +0x268
    uint32_t weaponFireChunks[2][MAX_WEAPONFIRE_CHUNKS] = {}; // +0x270
    /// <summary>Critical hit chunks queued.</summary>
    int32_t numCriticalHitChunks[2] = {};                     // +0x670
    uint8_t criticalHitChunks[2][MAX_WEAPONFIRE_CHUNKS] = {}; // +0x678
    /// <summary>Radio chunks queued.</summary>
    int32_t numRadioChunks[2] = {};                // +0x778
    uint8_t radioChunks[2][MAX_RADIO_CHUNKS] = {}; // +0x780
    /// <summary>Whether the pilot was ordered to eject (sent in the status chunk).</summary>
    int32_t ejectOrderGiven = 0; // +0x790
    /// <summary>
    /// Seconds left of the death sequence, counted down once the mover lies dead (0.8 for a mech or elemental): the
    /// death explosion goes off under 0.4, the wreck is left under 0.
    /// </summary>
    float deathTimer = 1.0f; // +0x794
    /// <summary>Set once the death explosion has gone off.</summary>
    int32_t deathExplosionDone = 0; // +0x798
    /// <summary>Set by the withdraw order: the mover leaves the scenario once off the map or off screen.</summary>
    int32_t withdrawing = 0; // +0x79c
    /// <summary>The last position the mover could stand on.</summary>
    vector_3d lastValidPosition; // +0x7b0
    /// <summary>The way a mech pivots toward a target: 0 or 1, 0xff to choose again (BattleMech::pivotTo).</summary>
    uint8_t pivotDirection = 0xff; // +0x7bc
    /// <summary>-999 by init (updateHustleTime).</summary>
    float lastHustleTime = -999.0f; // +0x7c0
    /// <summary>A ground vehicle type's "AmmoTruck"; 0 for the others.</summary>
    int32_t ammoTruck = 0; // +0x7c4
    /// <summary>Distance moved since the mover last marked what it sees (it marks again past Terrain::metersPerVertex; 1000 by
    /// init, so the first update marks).</summary>
    float distanceSinceMarkSeen = 0.0f; // +0x7c8
    /// <summary>Used by needsRefit and TacticalOrder::status.</summary>
    GameObject* refitBuddy = nullptr; // +0x7cc
    /// <summary>The type's "CrashAvoidSelf".</summary>
    int32_t crashAvoidSelf = 1; // +0x7d0
    int32_t crashAvoidPath = 1; // +0x7d4
    /// <summary>Path lock level of the mover's own cell (updatePathLock); the type's "CrashBlockSelf".</summary>
    int32_t pathLockLevel = 1; // +0x7d8
    /// <summary>Path lock range (updatePathLock); the type's "CrashBlockPath".</summary>
    int32_t pathLockRange = 1;   // +0x7dc
    float crashYieldTime = 1.5f; // +0x7e0
    /// <summary>Cells setPathRangeLock has locked along the path.</summary>
    int32_t numPathRangeLocks = 0; // +0x7e4
    /// <summary>Those cells: tileR, tileC, cellR, cellC.</summary>
    int32_t pathRangeLocks[MAX_PATH_RANGE_LOCKS][4] = {}; // +0x7e8
    /// <summary>setOverlayWeightClass.</summary>
    int32_t overlayWeightClass = 0; // +0x888
    /// <summary>Scenario time a deselection takes effect (setSelected).</summary>
    float deselectTime = 0.0f; // +0x88c
    /// <summary>Seconds <see cref="stationaryTarget"/> has held still (calcAttackChance).</summary>
    float stationaryTime = 0.0f; // +0x890
    /// <summary>The target calcAttackChance last timed.</summary>
    GameObject* stationaryTarget = nullptr; // +0x894
    /// <summary>The salvage roll (MechSalvageChance), -999 until rolled.</summary>
    int32_t salvageRoll = -999; // +0x898
    /// <summary>
    /// Set by the interface while the player holds a forced-order key (ctrl, F9-F12) with this mover selected;
    /// cleared on release. The mover's queued waypoints are then joined by lines.
    /// </summary>
    int32_t drawOrderLines = 0; // +0x89c

    /// <summary>Movers alive (the last one's destroy frees <see cref="sortList"/>).</summary>
    static int32_t numMovers;
    /// <summary>The movers' shared sort list.</summary>
    static SortList* sortList;
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
extern WeaponFireChunk CurMoverWeaponFireChunk;

/// <summary>Fire range selectors 0..2 of Mover::getFireRange, in meters (short 250, medium 500, long 1000).</summary>
/// <remarks>MCX.EXE @ 0x00791888</remarks>
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
/// <remarks>MCX.EXE @ 0x00790cb8 (mech.cpp)</remarks>
extern float WeaponFireModifiers[30];
/// <summary>Seconds a target must hold still for the full stationary modifier.</summary>
/// <remarks>MCX.EXE @ 0x007e3704</remarks>
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
extern int32_t hitLevel[2];
/// <summary>"ClusterSizeSRM": read, then always 2.</summary>
extern int32_t ClusterSizeSRM;
/// <summary>"ClusterSizeLRM": read, then always 5.</summary>
extern int32_t ClusterSizeLRM;
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
/// <remarks>MCX.EXE @ 0x00683e00</remarks>
int32_t loadMoverGameSystem(FitIniFile* sysFile, float maxVisualRange);

/// <summary>Cells along a side of <see cref="goalMap"/>: 13 tiles around the goal's.</summary>
constexpr int32_t GOALMAP_CELL_DIM = 39;
/// <summary>Mover::calcMoveGoal's scores of the cells around a move goal.</summary>
extern int32_t goalMap[GOALMAP_CELL_DIM][GOALMAP_CELL_DIM];

/// <summary>Why the last StatusChunk::pack or unpack found its target bad (0 none; 1-2 mover, 3 terrain object, 4
/// train car, 5 bad type).</summary>
extern int32_t StatusChunkUnpackErr;

/// <summary>
/// Writes a mover's two status chunks, field by field, to ChunkDebugMsg and the file "stchunk.dbg", and points the
/// crash report at them (StatusChunk::equalTo calls it on a mismatch).
/// </summary>
/// <remarks>MCX.EXE @ 0x006848a0</remarks>
void DebugStatusChunk(Mover* mover, StatusChunk* chunk1, StatusChunk* chunk2);

/// <summary>The mover with part id <paramref name="partId"/> (from MoverRoster), or null.</summary>
/// <remarks>MCX.EXE @ 0x00684880</remarks>
Mover* getMoverFromPartId(int32_t partId);
