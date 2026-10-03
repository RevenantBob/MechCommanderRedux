#pragma once

#include "ai/move.h"
#include "ai/tacordr.h"
#include "gui/awindow.h"
#include "lib/cvmath.h"

class ABLModule;
class File;
class FitIniFile;
class GameObject;
class MechWarrior;
class Mover;
class MoverGroup;
class MovePath;
class Radio;
class ScrollingTextWindow;
class SortList;
class Team;
struct _PathQueueRec;
struct _SymTableNode;

/// <summary>Pilot skills (<see cref="SkillsTable"/> order).</summary>
constexpr int32_t NUM_SKILLS = 4;
/// <summary>Skill indices (<see cref="SkillsTable"/>: "Piloting", "Jumping", "Sensors", "Gunnery"). The names are
/// MechCommander 2's style; the original's aren't known.</summary>
constexpr int32_t MWS_PILOTING = 0;
constexpr int32_t MWS_JUMPING = 1;
constexpr int32_t MWS_SENSORS = 2;
constexpr int32_t MWS_GUNNERY = 3;
/// <summary>Pilot alarms (<see cref="PilotAlarmType"/>, <see cref="pilotAlarmFunctionName"/>).</summary>
constexpr int32_t NUM_PILOT_ALARMS = 0x11;
/// <summary>Triggers an alarm remembers until it is handled.</summary>
constexpr int32_t MAX_ALARM_TRIGGERS = 10;
/// <summary>Attackers a pilot remembers.</summary>
constexpr int32_t MAX_ATTACKERS = 50;
/// <summary>Brain memory cells ("Warrior%d" / "Cell" in the brain parameter file).</summary>
constexpr int32_t NUM_MEMORY_CELLS = 50;
/// <summary>Weapons a pilot tracks the status of (<see cref="MechWarrior::weaponsStatus"/>).</summary>
constexpr int32_t MAX_WEAPONS_PER_WARRIOR = 0x20;
/// <summary>Queued orders each pilot gets from <see cref="TacOrderQueue"/>.</summary>
constexpr int32_t MAX_QUEUED_TACORDERS_PER_WARRIOR = 0x10;
/// <summary>Size of the shared order queue pool.</summary>
constexpr int32_t MAX_QUEUED_TACORDERS = 2000;

/// <summary>Which of the pilot's three order slots is current (<see cref="MechWarrior::orderState"/>). Names are
/// MechCommander 2's.</summary>
enum OrderStateType
{
    ORDERSTATE_GENERAL = 0,
    ORDERSTATE_PLAYER = 1,
    ORDERSTATE_ALARM = 2,
    NUM_ORDERSTATES
};

/// <summary>
/// The pilot alarms, in <see cref="pilotAlarmFunctionName"/> order (the brain's handler for each). Names follow
/// MechCommander 2's, which match the handler names.
/// </summary>
enum PilotAlarmType
{
    PILOT_ALARM_TARGET_OF_WEAPONFIRE = 0,
    PILOT_ALARM_HIT_BY_WEAPONFIRE = 1,
    PILOT_ALARM_DAMAGE_TAKEN_RATE = 2,
    PILOT_ALARM_DEATH_OF_MATE = 3,
    PILOT_ALARM_FRIENDLY_VEHICLE_CRIPPLED = 4,
    PILOT_ALARM_FRIENDLY_VEHICLE_DESTROYED = 5,
    PILOT_ALARM_VEHICLE_INCAPACITATED = 6,
    PILOT_ALARM_VEHICLE_DESTROYED = 7,
    PILOT_ALARM_VEHICLE_WITHDRAWN = 8,
    PILOT_ALARM_MORALE_BREAK = 9,
    PILOT_ALARM_COLLISION = 10,
    PILOT_ALARM_GUARD_RADIUS_BREACH = 11,
    PILOT_ALARM_KILLED_TARGET = 12,
    PILOT_ALARM_MATE_FIRED_WEAPON = 13,
    PILOT_ALARM_PLAYER_ORDER = 14,
    PILOT_ALARM_NO_MOVEPATH = 15,
    PILOT_ALARM_GATE_CLOSING = 16
};

/// <summary>An attacker a pilot remembers: who, and when it last attacked.</summary>
/// <remarks>8 bytes.</remarks>
struct _AttackerRec
{
    /// <summary>The attacker's part id.</summary>
    uint32_t attackerId; // +0x00
    /// <summary>Scenario time of the last attack.</summary>
    float lastTime; // +0x04
};

/// <summary>An alarm raised on a pilot: its triggers since it was last handled.</summary>
/// <remarks>0x2c bytes. The original name isn't known (MC2: PilotAlarm).</remarks>
struct _PilotAlarm
{
    uint8_t unknown00; // +0x00
    /// <summary>Triggers remembered (at most <see cref="MAX_ALARM_TRIGGERS"/>); 0 when not raised.</summary>
    uint8_t numTriggers; // +0x01
    /// <summary>What raised it (a part id, a cause code, a path error).</summary>
    uint32_t trigger[MAX_ALARM_TRIGGERS]; // +0x04
};

/// <summary>A queued player order as the queue keeps it: its id, first way point and packed data.</summary>
/// <remarks>0x18 bytes. The original name is <c>_QueuedTacOrder</c> (from <see cref="TacOrderQueue"/>'s type).</remarks>
struct _QueuedTacOrder
{
    int32_t id;             // +0x00
    vector_3d point;        // +0x04
    uint32_t packedData[2]; // +0x10
};

/// <summary>A brain memory cell: an integer or a real ("MemType" 0 or 1).</summary>
union _MemoryCell
{
    int32_t integer;
    float real;
};

/// <summary>The move states of <see cref="_MoveOrders"/> (names are the port's).</summary>
enum : int32_t
{
    MOVESTATE_FORWARD = 1,
    MOVESTATE_REVERSE = 2,
    MOVESTATE_PIVOT_FORWARD = 3,
    MOVESTATE_PIVOT_REVERSE = 4,
    MOVESTATE_PIVOT_TARGET = 5,
};

/// <summary>A pilot's movement orders: goal, speed, way path, global path and the two move paths.</summary>
/// <remarks>Original source: <c>object\warrior.cpp</c>; 0x1044 bytes. Names follow MechCommander 2's MoveOrders
/// where the layout matches.</remarks>
struct _MoveOrders
{
    /// <summary>Now; origin 1, speed type 3 (state 2, throttle 100); no goal; paths cleared.</summary>
    /// <remarks>MCX.EXE @ 0x006a2160</remarks>
    void init();

    /// <summary>Scenario time the orders were given.</summary>
    float time; // +0x00
    /// <summary>Who gave them.</summary>
    int8_t origin; // +0x04
    /// <summary>How fast to move (setMoveSpeedType).</summary>
    int32_t speedType; // +0x08
    /// <summary>A set speed (setMoveSpeedVelocity).</summary>
    float speedVelocity; // +0x0c
    /// <summary>Speed state for the set speed.</summary>
    int8_t speedState; // +0x10
    /// <summary>Throttle for the set speed.</summary>
    int8_t speedThrottle; // +0x11
    /// <summary>The goal: -1 none, 0 a location, else the goal object's part id.</summary>
    int32_t goalType; // +0x14
    /// <summary>The goal object.</summary>
    GameObject* goalObject; // +0x18
    /// <summary>The goal object's position when the path was requested.</summary>
    vector_3d goalObjectPosition; // +0x1c
    /// <summary>The goal location (-999999 for none).</summary>
    vector_3d goalLocation; // +0x28
    /// <summary>When the path was last planned plus MovementUpdateFrequency.</summary>
    float nextUpdate; // +0x34
    /// <summary>Set by ABL setmovegoal (hasmovegoal / hasmovepath test it); only init clears it.</summary>
    int32_t scriptGoal; // +0x38
    /// <summary>The way points (setMoveWayPath, addMoveWayPoint).</summary>
    vector_3d wayPath[MAX_WAYPTS]; // +0x3c
    /// <summary>Way points held.</summary>
    int8_t numWayPts; // +0xf0
    /// <summary>The way point being walked to.</summary>
    int8_t curWayPt; // +0xf1
    /// <summary>1 to patrol (walk the way path back and forth).</summary>
    int8_t curWayDir; // +0xf2
    /// <summary>0 none, 1 a single (local) path, 2 a global path.</summary>
    int8_t pathType; // +0xf3
    /// <summary>The goal as first asked for (-999999 by init).</summary>
    vector_3d originalGlobalGoal[2]; // +0xf4
    /// <summary>The goal the path actually reaches (-666666 by init).</summary>
    vector_3d globalGoalLocation; // +0x10c
    /// <summary>The global path, area by area.</summary>
    GlobalPathStep globalPath[MAX_GLOBAL_PATH]; // +0x118
    /// <summary>Steps of the global path.</summary>
    int8_t numGlobalSteps; // +0x1018
    /// <summary>The global step being walked.</summary>
    int8_t curGlobalStep; // +0x1019
    /// <summary>The move paths: the one walked, and the next leg (made by MechWarrior::init).</summary>
    MovePath* path[2]; // +0x101c
    /// <summary>When the move was ordered; the move times out MoveTimeOut after it (-1 for none).</summary>
    float timeOfLastStep; // +0x1024
    /// <summary>1 by init; 3-5 are the combat moves calcMovePath resets.</summary>
    int32_t moveState; // +0x1028
    /// <summary>1 by init; 5 to hold position while attacking.</summary>
    int32_t moveStateGoal; // +0x102c
    /// <summary>Nonzero keeps the movement tree from requesting new attack paths.</summary>
    int32_t unknown1030; // +0x1030
    /// <summary>When yielding to a blocking mover, the time to give up and look again (-1 for none).</summary>
    float yieldTime;    // +0x1034
    int32_t yieldState; // +0x1038
    /// <summary>When waiting for the group's point to move (-1 for none).</summary>
    float waitForPointTime; // +0x103c
    /// <summary>Run to the goal.</summary>
    int32_t run; // +0x1040
};

/// <summary>A pilot's attack orders.</summary>
/// <remarks>Original source: <c>object\warrior.cpp</c>; 0x28 bytes. Names follow MechCommander 2's AttackOrders.</remarks>
struct _AttackOrders
{
    /// <summary>Now; origin 1; no target; aim location -1.</summary>
    /// <remarks>MCX.EXE @ 0x006a2250</remarks>
    void init();

    /// <summary>Scenario time the orders were given.</summary>
    float time; // +0x00
    /// <summary>Who gave them.</summary>
    int8_t origin; // +0x04
    /// <summary>The attack type.</summary>
    int32_t type; // +0x08
    /// <summary>The target.</summary>
    GameObject* target; // +0x0c
    /// <summary>The point attacked.</summary>
    vector_3d targetPoint; // +0x10
    /// <summary>Body location aimed at; -1 for any.</summary>
    int32_t aimLocation; // +0x1c
    /// <summary>Whether to follow the target.</summary>
    int32_t pursue; // +0x20
    /// <summary>Scenario time the target was set (setAttackTarget), -1 for none.</summary>
    float targetTime; // +0x24
};

/// <summary>
/// A pilot: name and portrait, skills and personality, wounds, the ABL brain, its orders (general, player and alarm,
/// the current one and the queue), move and attack orders, and the radio. Drives its vehicle through the decision
/// trees.
/// </summary>
/// <remarks>Original source: <c>object\warrior.cpp</c>, <c>object\warrior.h</c>; 0x1e5c bytes. Allocated from
/// systemHeap. Field names are the port's where the original's aren't known (MechCommander 2's where the layout
/// matches).</remarks>
class MechWarrior
{
public:
    /// <summary>Allocates from systemHeap.</summary>
    /// <remarks>MCX.EXE @ 0x006a17d0</remarks>
    static void* operator new(size_t size) noexcept;
    /// <summary>Frees into systemHeap.</summary>
    /// <remarks>MCX.EXE @ 0x006a17f0</remarks>
    static void operator delete(void* ptr);

    /// <summary>Drops the brain and its callbacks.</summary>
    /// <remarks>MCX.EXE @ 0x006a1810</remarks>
    void lobotomy();
    /// <summary>
    /// Resets every field (the constructor's work): default personality 40, update times staggered by warrior
    /// count, orders cleared, two move paths made; counts the warrior and makes the shared sort list.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006a1850</remarks>
    void init();
    /// <summary>
    /// Reads the pilot file: name, picture, callsign, audio/video and radio, paint scheme, personality, skills
    /// (current, original, latest, points), wounds; rolls whether the pilot survives ejection.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006a1b60</remarks>
    int32_t init(FitIniFile* warriorFile);
    /// <summary>
    /// Plays radio message <paramref name="messageId"/> when the pilot is under the local player's command
    /// (skipping repeats too soon after the last); a server passes others' messages on as radio chunks.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006a2280</remarks>
    void radioMessage(int32_t messageId, int propogateIfMultiplayer);
    /// <summary>Frees the strings, the brain and the paths; the last warrior frees the sort list.</summary>
    /// <remarks>MCX.EXE @ 0x006a2450</remarks>
    void destroy();
    /// <summary>The aggressiveness; halfway to 100 while on a combat order when <paramref name="current"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006a2560</remarks>
    int32_t getAggressiveness(int current);
    /// <summary>Takes the pilot's 16 slots of <see cref="TacOrderQueue"/>; 0 when the pool is used up.</summary>
    /// <remarks>MCX.EXE @ 0x006a25a0</remarks>
    int enableTacOrderQueue();
    /// <summary>Queues a player order (0; 1 without a queue, 2 when full); starts it when it is the only one.</summary>
    /// <remarks>MCX.EXE @ 0x006a25e0</remarks>
    int32_t addQueuedTacOrder(TacticalOrder tacOrder);
    /// <summary>Takes the first queued order (0; 1 without a queue, 2 when empty).</summary>
    /// <remarks>MCX.EXE @ 0x006a26f0</remarks>
    int32_t removeQueuedTacOrder(TacticalOrder* tacOrder);
    /// <summary>Reads the first queued order without taking it (0; 1 without a queue, 2 when empty).</summary>
    /// <remarks>MCX.EXE @ 0x006a27f0</remarks>
    int32_t peekQueuedTacOrder(TacticalOrder* tacOrder);
    /// <remarks>MCX.EXE @ 0x006a2890</remarks>
    void clearTacOrderQueue();
    /// <summary>Makes the next queued order the player order.</summary>
    /// <remarks>MCX.EXE @ 0x006a28a0</remarks>
    void executeTacOrderQueue();
    /// <remarks>MCX.EXE @ 0x006a2920</remarks>
    void lockTacOrderQueue();
    /// <remarks>MCX.EXE @ 0x006a2930</remarks>
    void unlockTacOrderQueue();
    /// <summary>
    /// Copies the player order from the queue (when one runs) and the queued orders into <paramref name="list"/>
    /// (which may be null); returns how many.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006a2940 (unnamed in Ghidra; MechCommander 2's name).</remarks>
    int32_t getTacOrderQueue(_QueuedTacOrder* list);
    /// <summary>
    /// On a client: drops the queued orders the server has executed (those before <paramref name="tacOrderId"/>,
    /// or the last executed one when 0).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006a2a50</remarks>
    void updateClientOrderQueue(int32_t tacOrderId);
    /// <remarks>MCX.EXE @ 0x006a2b10</remarks>
    MoverGroup* getGroup();
    /// <remarks>MCX.EXE @ 0x006a2b30</remarks>
    Mover* getPoint();
    /// <remarks>MCX.EXE @ 0x006a2b50</remarks>
    int onHomeTeam();
    /// <summary>Whether the vehicle belongs to a player (its net player id is set).</summary>
    /// <remarks>MCX.EXE @ 0x006a2b70</remarks>
    int underHomeCommand();
    /// <summary>Rolls a skill check (counting the try and a success); returns the margin, negative on a failure.</summary>
    /// <remarks>MCX.EXE @ 0x006a2b90</remarks>
    int32_t checkSkill(int32_t skillId, float factor);
    /// <summary>Adds wounds; at 6 the pilot dies (or ejects when <paramref name="checkEject"/> and the roll allowed
    /// it) and the vehicle is disabled.</summary>
    /// <remarks>MCX.EXE @ 0x006a2c10</remarks>
    int injure(float numWounds, int checkEject);
    /// <summary>Ejects: a wound, the radio call, status 3 (4 if that killed him), the vehicle disabled.</summary>
    /// <remarks>MCX.EXE @ 0x006a2d50 (unnamed in Ghidra; MechCommander 2's name).</remarks>
    void eject();
    /// <remarks>MCX.EXE @ 0x006a2e00</remarks>
    void setTeam(Team* newTeam);
    /// <remarks>MCX.EXE @ 0x006a2e20</remarks>
    void setVehicle(GameObject* newVehicle);
    /// <remarks>MCX.EXE @ 0x006a36d0 (inline in <c>object\warrior.h</c>; unnamed in Ghidra)</remarks>
    GameObject* getVehicle() { return vehicle; }
    /// <remarks>MCX.EXE @ 0x006a2e70</remarks>
    void setBrainName(char* brainName);
    /// <summary>Replaces the brain with a new module of <paramref name="brainHandle"/> and finds its alarm handlers.</summary>
    /// <remarks>MCX.EXE @ 0x006a2ec0</remarks>
    int32_t setBrain(int32_t brainHandle);
    /// <remarks>MCX.EXE @ 0x006a2fd0</remarks>
    int32_t runBrain();
    /// <summary>The vehicle's status, -1 without one.</summary>
    /// <remarks>MCX.EXE @ 0x006a3050</remarks>
    int32_t getVehicleStatus();
    /// <remarks>MCX.EXE @ 0x006a3060</remarks>
    void updateAttackerStatus(uint32_t attackerId, float time);
    /// <remarks>MCX.EXE @ 0x006a30c0</remarks>
    _AttackerRec* getAttackerInfo(uint32_t attackerId);
    /// <summary>The attackers of the last <paramref name="seconds"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006a3100</remarks>
    int32_t getAttackers(uint32_t* attackerList, float seconds);
    /// <remarks>MCX.EXE @ 0x006a3160</remarks>
    int32_t setAttackTarget(GameObject* object);
    /// <summary>The last target while it stays worth shooting; else clears it (and a combat order).</summary>
    /// <remarks>MCX.EXE @ 0x006a3180</remarks>
    GameObject* getLastTarget();
    /// <summary>
    /// Sets the last target (moving the vehicle's attacker count to it when the vehicle belongs to a player), and
    /// whether a disabled target still counts (<paramref name="obliterate"/>) and to conserve ammo.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006a3240 (unnamed in Ghidra; MechCommander 2's name).</remarks>
    void setLastTarget(GameObject* target, int obliterate = 0, int conserveAmmo = 0);
    /// <remarks>MCX.EXE @ 0x006a32e0</remarks>
    void setCurrentTarget(GameObject* target);
    /// <remarks>MCX.EXE @ 0x006a3300</remarks>
    GameObject* getAttackTargetPosition(vector_3d& pos);
    /// <remarks>MCX.EXE @ 0x006a3350</remarks>
    void clearAttackOrders();
    /// <remarks>MCX.EXE @ 0x006a3380</remarks>
    void clearMoveOrders();
    /// <summary>Sets the move goal: <paramref name="type"/> -1 none, 0 a location, else an object's part id.</summary>
    /// <remarks>MCX.EXE @ 0x006a3410</remarks>
    int32_t setMoveGoal(uint32_t type, vector_3d* location, GameObject* obj);
    /// <remarks>MCX.EXE @ 0x006a3520</remarks>
    void pausePath();
    /// <remarks>MCX.EXE @ 0x006a3540</remarks>
    void resumePath();
    /// <summary>
    /// The vehicle reached the end of its path: asks for the next leg (a queued move point or the next global step),
    /// or ends the move order and raises the no-path alarm.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006a3560 (unnamed in Ghidra; the name is the port's).</remarks>
    void reachedPathEnd();
    /// <remarks>MCX.EXE @ 0x006a3660</remarks>
    float getMoveDistanceLeft();
    /// <remarks>MCX.EXE @ 0x006a36e0</remarks>
    int isJumping(vector_3d* jumpGoal);
    /// <summary>The path being walked; when it is done, swaps in the next leg.</summary>
    /// <remarks>MCX.EXE @ 0x006a3700</remarks>
    MovePath* getMovePath();
    /// <remarks>MCX.EXE @ 0x006a38c0</remarks>
    void setMoveWayPath(_WayPath* wayPath, int patrol);
    /// <remarks>MCX.EXE @ 0x006a3940</remarks>
    void addMoveWayPoint(vector_3d wayPt, int patrol);
    /// <remarks>MCX.EXE @ 0x006a39a0</remarks>
    void setMoveGlobalPath(_GlobalPathStep* path, int32_t numSteps);
    /// <summary>Queues a path request with the <see cref="PathManager"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006a3a00</remarks>
    void requestMovePath(int32_t selectionIndex, uint32_t moveParams, int32_t source);
    /// <summary>
    /// Plans the move to the goal: a local path when close (or no long-range movement), else a global path
    /// area by area, each leg planned as the previous one ends; escape paths and jumps too. Sets
    /// <see cref="LastMoveCalcErr"/> and raises the no-path alarm on a failure.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006a3a30</remarks>
    int32_t calcMovePath(int32_t selectionIndex, uint32_t moveParams, int32_t source);
    /// <summary>The first way point of a queued move order, if the next queued order is one.</summary>
    /// <remarks>MCX.EXE @ 0x006a4ce0</remarks>
    int getNextWayPoint(vector_3d& nextPoint, int incWayPoint);
    /// <summary>
    /// Per weapon, whether it can fire at the target now (the attack chance, or -1 not ready, -2 no ammo, -3 out
    /// of range, -4 not locked, -5 no chance); returns how many can, or -1 can't fire, -2 no target, -3 out of range.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006a4d60</remarks>
    int32_t calcWeaponsStatus(GameObject* target, int32_t* weaponList, vector_3d* targetPoint);
    /// <summary>Fires what can fire at the last target; out of ammo ends the order.</summary>
    /// <remarks>MCX.EXE @ 0x006a5000</remarks>
    int32_t combatDecisionTree();
    /// <summary>A point away from the enemy, walking the escape vector until off the map or blocked.</summary>
    /// <remarks>MCX.EXE @ 0x006a5840</remarks>
    vector_3d calcWithdrawGoal(float withdrawRange);
    /// <summary>Whether the vehicle is on, or its path crosses, a blown bridge.</summary>
    /// <remarks>MCX.EXE @ 0x006a5b40</remarks>
    int movingOverBlownBridge();
    /// <summary>Times out moves, reroutes around gates and blown bridges, and keeps attack moves in range.</summary>
    /// <remarks>MCX.EXE @ 0x006a5c70</remarks>
    int movementDecisionTree();
    /// <summary>Ends the current order; with <paramref name="updateTacOrder"/> the next order (player, then
    /// general) starts.</summary>
    /// <remarks>MCX.EXE @ 0x006a66e0</remarks>
    void clearCurTacOrder(int updateTacOrder, int updateBrain);
    /// <remarks>MCX.EXE @ 0x006a6c70</remarks>
    void setCurTacOrder(TacticalOrder tacOrder);
    /// <remarks>MCX.EXE @ 0x006a6ed0</remarks>
    void setGeneralTacOrder(TacticalOrder tacOrder);
    /// <remarks>MCX.EXE @ 0x006a7010</remarks>
    void setPlayerTacOrder(TacticalOrder tacOrder, int fromQueue);
    /// <summary>Sets the alarm order unless one of higher priority is waiting.</summary>
    /// <remarks>MCX.EXE @ 0x006a7160</remarks>
    void setAlarmTacOrder(TacticalOrder tacOrder, int32_t priority);
    /// <summary>Raises an alarm (up to <see cref="MAX_ALARM_TRIGGERS"/> triggers); -1 when full.</summary>
    /// <remarks>MCX.EXE @ 0x006a72c0</remarks>
    int32_t triggerAlarm(int32_t alarmCode, uint32_t triggerId);
    /// <summary>Handles an alarm at once (awake vehicles only): the built-in handler, then the brain's.</summary>
    /// <remarks>MCX.EXE @ 0x006a7310</remarks>
    int32_t handleAlarm(int32_t alarmCode, uint32_t triggerId);
    /// <remarks>MCX.EXE @ 0x006a7410</remarks>
    int32_t getAlarmTriggers(int32_t alarmCode, uint32_t* triggerList);
    /// <remarks>MCX.EXE @ 0x006a7440</remarks>
    void clearAlarm(int32_t alarmCode);
    /// <summary>Handles each raised alarm (built-in handler, then the brain's) and clears it.</summary>
    /// <remarks>MCX.EXE @ 0x006a7460</remarks>
    int32_t checkAlarms();
    /// <summary>
    /// The per-frame update the AI and network controls call: order status, weapons status, the brain, alarms,
    /// the next order, then the combat and movement trees.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006a7690 (unnamed in Ghidra; named by its own assert, "MechWarrior.mainDecisionTree").</remarks>
    int32_t mainDecisionTree();
    /// <summary>A captured vehicle drops its orders; else the combat and movement trees run.</summary>
    /// <remarks>MCX.EXE @ 0x006a7640</remarks>
    void updateActions();
    /// <remarks>MCX.EXE @ 0x006a8170</remarks>
    void setDebugFlag(uint32_t flag, int on);
    /// <remarks>MCX.EXE @ 0x006a81b0</remarks>
    int getDebugFlag(uint32_t flag);
    /// <remarks>MCX.EXE @ 0x006a81d0</remarks>
    void debugPrint(char* s, int debugMode);
    /// <remarks>MCX.EXE @ 0x006a8200</remarks>
    void debugOrders();
    /// <remarks>MCX.EXE @ 0x006a84f0</remarks>
    void setMoveSpeedType(int32_t type);
    /// <remarks>MCX.EXE @ 0x006a8500</remarks>
    void setMoveSpeedVelocity(float speed);
    /// <remarks>MCX.EXE @ 0x006a8550</remarks>
    int32_t openStatusWindow(int32_t x, int32_t y, int32_t w, int32_t h);
    /// <remarks>MCX.EXE @ 0x006a8610</remarks>
    int32_t closeStatusWindow();
    /// <remarks>MCX.EXE @ 0x006a8630</remarks>
    int32_t orderWait(int unitOrder, int32_t origin, int32_t seconds, int clearLastTarget);
    /// <remarks>MCX.EXE @ 0x006a87c0</remarks>
    int32_t orderStop(int unitOrder, int setTacOrder);
    /// <remarks>MCX.EXE @ 0x006a8830</remarks>
    int32_t orderMoveToPoint(int unitOrder, int setTacOrder, int32_t origin, vector_3d location, int32_t selectionIndex,
                             uint32_t params);
    /// <remarks>MCX.EXE @ 0x006a8ab0</remarks>
    int32_t orderMoveToObject(int unitOrder, int setTacOrder, int32_t origin, GameObject* target,
                              int32_t selectionIndex, uint32_t params);
    /// <remarks>MCX.EXE @ 0x006a8d40</remarks>
    int32_t orderJumpToPoint(int unitOrder, int setTacOrder, int32_t origin, vector_3d location,
                             int32_t selectionIndex);
    /// <remarks>MCX.EXE @ 0x006a8f80</remarks>
    int32_t orderJumpToObject(int unitOrder, int setTacOrder, int32_t origin, GameObject* target,
                              int32_t selectionIndex);
    /// <remarks>MCX.EXE @ 0x006a9220</remarks>
    int32_t orderTraversePath(int unitOrder, int setTacOrder, int32_t origin, _WayPath* wayPath, uint32_t params);
    /// <remarks>MCX.EXE @ 0x006a9420</remarks>
    int32_t orderPatrolPath(int unitOrder, int setTacOrder, int32_t origin, _WayPath* wayPath);
    /// <remarks>MCX.EXE @ 0x006a9610</remarks>
    int32_t orderPowerUp(int unitOrder, int32_t origin);
    /// <remarks>MCX.EXE @ 0x006a97f0</remarks>
    int32_t orderPowerDown(int unitOrder, int32_t origin);
    /// <remarks>MCX.EXE @ 0x006a9990</remarks>
    int32_t orderUseSpeed(float speed);
    /// <summary>Does nothing (returns 1).</summary>
    /// <remarks>MCX.EXE @ 0x006a99e0</remarks>
    int32_t orderOrbitPoint(vector_3d location);
    /// <remarks>MCX.EXE @ 0x006a9a10</remarks>
    int32_t orderAttackObject(int unitOrder, int32_t origin, GameObject* target, int32_t type, int32_t method,
                              int32_t range, int32_t aimLocation, uint32_t params);
    /// <remarks>MCX.EXE @ 0x006a9d90</remarks>
    int32_t orderAttackPoint(int unitOrder, int32_t origin, vector_3d location, int32_t type, int32_t method,
                             int32_t range, uint32_t params);
    /// <remarks>MCX.EXE @ 0x006aa0d0</remarks>
    void setAttackTargetPoint(vector_3d location) { attackOrders.targetPoint = location; }
    /// <remarks>MCX.EXE @ 0x0066cee0 (inline in <c>object\warrior.h</c>)</remarks>
    vector_3d getAttackTargetPoint() { return attackOrders.targetPoint; }
    /// <remarks>MCX.EXE @ 0x006aa100</remarks>
    int32_t orderWithdraw(int unitOrder, int32_t origin, vector_3d location);
    /// <remarks>MCX.EXE @ 0x006aa2e0</remarks>
    int32_t orderEject(int unitOrder, int setTacOrder, int32_t origin);
    /// <remarks>MCX.EXE @ 0x006aa450</remarks>
    int32_t orderUseFireRange(int32_t range);
    /// <remarks>MCX.EXE @ 0x006aa480</remarks>
    int32_t orderUseFireOdds(int32_t odds);
    /// <remarks>MCX.EXE @ 0x006aa4a0</remarks>
    int32_t orderRefit(int32_t origin, GameObject* target, uint32_t params);
    /// <remarks>MCX.EXE @ 0x006aa630</remarks>
    int32_t orderGetFixed(int32_t origin, GameObject* target, uint32_t params);
    /// <remarks>MCX.EXE @ 0x006aa800</remarks>
    int32_t orderLoadIntoCarrier(int32_t origin, GameObject* target, uint32_t params);
    /// <remarks>MCX.EXE @ 0x006aa9a0</remarks>
    int32_t orderDeployElementals(int32_t origin, uint32_t params);
    /// <remarks>MCX.EXE @ 0x006aab20</remarks>
    int32_t orderCapture(int32_t origin, GameObject* target, uint32_t params);
    /// <remarks>MCX.EXE @ 0x006aace0</remarks>
    int32_t handleTargetOfWeaponFire();
    /// <remarks>MCX.EXE @ 0x006aad00</remarks>
    int32_t handleHitByWeaponFire();
    /// <remarks>MCX.EXE @ 0x006aad20</remarks>
    int32_t handleCollision();
    /// <remarks>MCX.EXE @ 0x006aad40</remarks>
    int32_t handleDamageTakenRate();
    /// <remarks>MCX.EXE @ 0x006aad50</remarks>
    int32_t handleUnitMateDeath();
    /// <remarks>MCX.EXE @ 0x006aad80</remarks>
    int32_t handleFriendlyVehicleCrippled();
    /// <remarks>MCX.EXE @ 0x006aad90</remarks>
    int32_t handleFriendlyVehicleDestruction();
    /// <remarks>MCX.EXE @ 0x006aada0</remarks>
    int32_t handleOwnVehicleIncapacitation(uint32_t cause);
    /// <remarks>MCX.EXE @ 0x006aae60</remarks>
    int32_t handleOwnVehicleDestruction(uint32_t cause);
    /// <remarks>MCX.EXE @ 0x006aae90</remarks>
    int32_t handleOwnVehicleWithdrawn();
    /// <remarks>MCX.EXE @ 0x006aaec0</remarks>
    int32_t handleMoraleBreak();
    /// <remarks>MCX.EXE @ 0x006aaf20</remarks>
    int32_t handleCollisionAlert();
    /// <summary>Counts the kill, calls it in, and scores gunnery skill points (a tenth for an ally's).</summary>
    /// <remarks>MCX.EXE @ 0x006aaf30</remarks>
    int32_t handleKilledTarget();
    /// <remarks>MCX.EXE @ 0x006ab080</remarks>
    int32_t handleUnitMateFiredWeapon();
    /// <remarks>MCX.EXE @ 0x006ab090</remarks>
    int32_t handlePlayerOrder();
    /// <remarks>MCX.EXE @ 0x006ab0c0</remarks>
    int32_t handleNoMovePath();
    /// <remarks>MCX.EXE @ 0x006ab0f0</remarks>
    int32_t handleGateClosing();
    /// <remarks>MCX.EXE @ 0x006ab100</remarks>
    int32_t missionLog(File* file, int32_t unitLevel);
    /// <summary>The rank from the weighted skill ranks (<see cref="SkillWeightings"/>, <see cref="WarriorRankScale"/>).</summary>
    /// <remarks>MCX.EXE @ 0x006ab190</remarks>
    void calcRank();
    /// <summary>Reads section "Warrior<paramref name="warriorId"/>": the brain's memory cells and static variables.</summary>
    /// <remarks>MCX.EXE @ 0x006ab200</remarks>
    int32_t loadBrainParameters(FitIniFile* brainFile, int32_t warriorId);

    /// <summary>"Name" (systemHeap).</summary>
    char* name = nullptr; // +0x00
    /// <summary>"Callsign" (systemHeap).</summary>
    char* callsign = nullptr; // +0x04
    /// <summary>"Picture", default "pilotx.gif" (systemHeap).</summary>
    char* picture = nullptr; // +0x08
    /// <summary>"pilotVideo" (systemHeap).</summary>
    char* videoStr = nullptr; // +0x0c
    /// <summary>"pilotAudio" (systemHeap).</summary>
    char* audioStr = nullptr; // +0x10
    /// <summary>The brain's name (systemHeap).</summary>
    char* brainStr = nullptr; // +0x14
    /// <summary>The pilot's index in Scenario::warriors (selectwarrior / getwarriorstatus take it).</summary>
    int32_t index = 0; // +0x18
    /// <summary>"PaintScheme", -1 for none.</summary>
    int32_t paintScheme = -1; // +0x1c
    /// <summary>Rank (calcRank).</summary>
    uint8_t rank = 0; // +0x20
    /// <summary>Current skills (<see cref="SkillsTable"/>).</summary>
    int8_t skills[NUM_SKILLS] = {}; // +0x21
    /// <summary>"Professionalism" (40 by default).</summary>
    int8_t professionalism = 40; // +0x25
    int8_t unknown26 = 0;        // +0x26
    /// <summary>"Decorum".</summary>
    int8_t decorum = 40;  // +0x27
    int8_t unknown28 = 0; // +0x28
    /// <summary>"Aggressiveness".</summary>
    int8_t aggressiveness = 40; // +0x29
    /// <summary>"Courage".</summary>
    int8_t courage = 40; // +0x2a
    /// <summary>The courage read ("Courage"), before morale changes it.</summary>
    int8_t baseCourage = 0; // +0x2b
    /// <summary>"Wounds"; 6 is death.</summary>
    float wounds = 0.0f;   // +0x2c
    int32_t unknown30 = 0; // +0x30
    /// <summary>The pilot's status: 0 normal, 2 withdrawn, 3 ejected, 4 dead.</summary>
    int32_t status = 0; // +0x34
    /// <summary>Rolled at load: survives ejection.</summary>
    int32_t escapesThruEjection = 0; // +0x38
    int32_t unknown3C = 0;           // +0x3c
    /// <summary>When "under attack" was last called in (at most every 20 seconds).</summary>
    float lastUnderAttackTime = -1000.0f; // +0x40
    /// <summary>"Weapons at 50%" was called in.</summary>
    int32_t weapons50Sent = 0; // +0x44
    /// <summary>"Weapons out" was called in.</summary>
    int32_t weaponsOutSent = 0; // +0x48
    /// <summary>When a sensor contact was last called in (at most every 15 seconds).</summary>
    float lastContactTime = -1000.0f; // +0x4c
    /// <summary>The last message type played.</summary>
    int32_t lastMessageType = 0; // +0x50
    /// <summary>What Radio::playMessage returned for it.</summary>
    int32_t lastMessage = -1; // +0x54
    /// <summary>When it played (repeats within 10 seconds are skipped).</summary>
    float lastMessageTime = 0.0f; // +0x58
    /// <summary>"NotMineYet".</summary>
    int32_t notMineYet = 0; // +0x5c
    /// <summary>The team.</summary>
    Team* team = nullptr; // +0x60
    /// <summary>The vehicle piloted.</summary>
    GameObject* vehicle = nullptr; // +0x64
    /// <summary>The team's alignment.</summary>
    int8_t alignment = 0; // +0x68
    /// <summary>Per skill: tries (two counters each; checkSkill counts the second). Mission statistics.</summary>
    int32_t numSkillUses[NUM_SKILLS][2] = {}; // +0x6c
    /// <summary>Per skill: successes.</summary>
    int32_t numSkillSuccesses[NUM_SKILLS][2] = {}; // +0x8c
    /// <summary>Kills by kind (mech class 1-4, 5 vehicles and turrets, 6 elementals; the second counter counts).</summary>
    int32_t numKilled[7][2] = {}; // +0xac
    int32_t unknownE4 = 0;        // +0xe4
    /// <summary>Enemy mechs rammed on an attack order (BattleMechType::handleCollision).</summary>
    int32_t numRams = 0;   // +0xe8
    int32_t unknownEC = 0; // +0xec
    /// <summary>Jumps landed on the jump order's target (BattleMechType::handleCollision).</summary>
    int32_t numJumpAttacks = 0; // +0xf0
    /// <summary>Per skill: the skill as a float.</summary>
    float skillRank[NUM_SKILLS] = {}; // +0xf4
    /// <summary>"SkillPoints" per skill.</summary>
    float skillPoints[NUM_SKILLS] = {}; // +0x104
    /// <summary>"OriginalSkills".</summary>
    int8_t originalSkills[NUM_SKILLS] = {}; // +0x114
    /// <summary>"LatestSkills".</summary>
    int8_t latestSkills[NUM_SKILLS] = {}; // +0x118
    /// <summary>"DescIndex", -1 for none.</summary>
    int32_t descIndex = -1; // +0x11c
    /// <summary>"NameIndex", -1 for none.</summary>
    int32_t nameIndex = -1; // +0x120
    /// <summary>When a home-team pilot was last left without orders (-1 while ordered).</summary>
    float timeOfLastOrders = -1.0f; // +0x124
    /// <summary>The attackers remembered.</summary>
    _AttackerRec attackers[MAX_ATTACKERS] = {}; // +0x128
    /// <summary>How many.</summary>
    int32_t numAttackers = 0; // +0x2b8
    /// <summary>DefaultAttackRadius.</summary>
    float attackRadius = 0.0f; // +0x2bc
    /// <summary>The brain's memory cells (loadBrainParameters).</summary>
    _MemoryCell memory[NUM_MEMORY_CELLS] = {}; // +0x2c0
    /// <summary>The ABL brain.</summary>
    ABLModule* brain = nullptr; // +0x388
    /// <summary>The brain's alarm handlers (<see cref="pilotAlarmFunctionName"/>).</summary>
    _SymTableNode* brainAlarmCallback[NUM_PILOT_ALARMS] = {}; // +0x38c
    /// <summary>Next brain run (staggered by warrior count, then every BrainUpdateFrequency).</summary>
    float brainUpdateTime = 0.0f; // +0x3d0
    /// <summary>Next combat update time.</summary>
    float combatUpdateTime = 0.0f; // +0x3d4
    /// <summary>Next movement update time.</summary>
    float movementUpdateTime = 0.0f; // +0x3d8
    /// <summary>Per weapon: its status against the target (<see cref="calcWeaponsStatus"/>).</summary>
    int32_t weaponsStatus[MAX_WEAPONS_PER_WARRIOR] = {}; // +0x3dc
    /// <summary>What <see cref="calcWeaponsStatus"/> returned (-2 by init).</summary>
    int32_t weaponsStatusResult = -2; // +0x45c
    /// <summary>Per order state: a new order is waiting in <see cref="tacOrder"/>.</summary>
    int32_t newTacOrderReceived[NUM_ORDERSTATES] = {}; // +0x460
    /// <summary>The waiting orders: general, player, alarm (0x138 bytes each).</summary>
    TacticalOrder tacOrder[NUM_ORDERSTATES]; // +0x46c
    /// <summary>The last order started.</summary>
    TacticalOrder lastTacOrder; // +0x814
    /// <summary>The current tactical order.</summary>
    TacticalOrder curTacOrder; // +0x94c
    /// <summary>The alarms (<see cref="PilotAlarmType"/>).</summary>
    _PilotAlarm alarm[NUM_PILOT_ALARMS] = {}; // +0xa84
    /// <summary>The priority of the waiting alarm order.</summary>
    int32_t alarmPriority = 0; // +0xd70
    /// <summary>The player order came from the queue.</summary>
    int32_t playerOrderFromQueue = 0; // +0xd74
    /// <summary>Set while the queue is locked.</summary>
    int32_t tacOrderQueueLocked = 0; // +0xd78
    /// <summary>Set while the queue is executing.</summary>
    int32_t tacOrderQueueExecuting = 0; // +0xd7c
    /// <summary>Orders queued.</summary>
    int8_t numTacOrdersQueued = 0; // +0xd80
    /// <summary>The pilot's slots of <see cref="TacOrderQueue"/>.</summary>
    _QueuedTacOrder* tacOrderQueue = nullptr; // +0xd84
    /// <summary>The next tactical order id (1 by init).</summary>
    int32_t nextTacOrderId = 1; // +0xd88
    /// <summary>The id of the last order the server executed.</summary>
    int32_t lastTacOrderId = 0; // +0xd8c
    /// <summary>Movement orders.</summary>
    _MoveOrders moveOrders; // +0xd90
    /// <summary>Attack orders.</summary>
    _AttackOrders attackOrders; // +0x1dd4
    int32_t unknown1DFC = 0;    // +0x1dfc
    int32_t unknown1E00 = 0;    // +0x1e00
    float unknown1E04 = 10.0f;  // +0x1e04
    float unknown1E08 = 10.0f;  // +0x1e08
    /// <summary>Set by the attack orders.</summary>
    int32_t unknown1E0C = 0; // +0x1e0c
    int32_t unknown1E10 = 0; // +0x1e10
    /// <summary>The fire range ordered (orderUseFireRange), -1 for none.</summary>
    float orderFireRange = -1.0f; // +0x1e14
    /// <summary>The fire odds ordered (orderUseFireOdds), -1 for none.</summary>
    float orderFireOdds = -1.0f; // +0x1e18
    int32_t unknown1E1C = 0;     // +0x1e1c
    int32_t unknown1E20 = 0;     // +0x1e20
    int32_t unknown1E24 = 1;     // +0x1e24
    /// <summary>The last target (getLastTarget).</summary>
    GameObject* lastTarget = nullptr; // +0x1e28
    /// <summary>When it was set, -1 for none.</summary>
    float lastTargetTime = -1.0f; // +0x1e2c
    /// <summary>Keep shooting it when disabled.</summary>
    int32_t lastTargetObliterate = 0; // +0x1e30
    /// <summary>It is on our side.</summary>
    int32_t lastTargetFriendly = 0; // +0x1e34
    /// <summary>Conserve ammo on it (the attack becomes type 3).</summary>
    int32_t lastTargetConserveAmmo = 0; // +0x1e38
    /// <summary>Which of <see cref="tacOrder"/> is current (<see cref="OrderStateType"/>).</summary>
    int8_t orderState = ORDERSTATE_GENERAL; // +0x1e3c
    /// <summary>The pending path request.</summary>
    _PathQueueRec* movePathRequest = nullptr; // +0x1e40
    /// <summary>Debug flags.</summary>
    uint32_t debugFlags = 0; // +0x1e44
    /// <summary>The status window.</summary>
    aTitleWindow* statusWindow = nullptr; // +0x1e48
    /// <summary>The radio.</summary>
    Radio* radio = nullptr; // +0x1e4c
    /// <summary>"OldPilot".</summary>
    uint8_t oldPilot = 0;    // +0x1e50
    int32_t unknown1E54 = 0; // +0x1e54
    /// <summary>"Ammo out" was called in.</summary>
    int32_t ammoOutSent = 0; // +0x1e58

    /// <summary>Warriors alive.</summary>
    static int32_t numWarriors;
    /// <summary>Warriors in combat.</summary>
    static int32_t numWarriorsInCombat;
    /// <summary>The warriors' shared sort list.</summary>
    static SortList* sortList;
};

/// <summary>The scenario's warriors by index.</summary>
/// <remarks>Original source: <c>object\warrior.cpp</c>; 8 bytes.</remarks>
class MechWarriorManager
{
public:
    /// <summary>Deletes every warrior.</summary>
    /// <remarks>MCX.EXE @ 0x006ab560</remarks>
    void destroy();
    /// <summary>Room for <paramref name="numWarriors"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006ab610</remarks>
    void init(int32_t numWarriors);
    /// <remarks>MCX.EXE @ 0x006ab660</remarks>
    void set(int32_t index, MechWarrior* warrior);
    /// <remarks>MCX.EXE @ 0x006ab680</remarks>
    MechWarrior* get(int32_t index);

    /// <summary>How many.</summary>
    int32_t numWarriors = 0; // +0x00
    /// <summary>The warriors.</summary>
    MechWarrior** warriors = nullptr; // +0x04
};

/// <summary>A pilot's status window.</summary>
/// <remarks>Original source: <c>object\warrior.cpp</c>, <c>object\warrior.h</c>; 0x4c4 bytes.</remarks>
class WarriorStatusWindow : public aTitleWindow
{
public:
    /// <remarks>MCX.EXE @ 0x006a85e0 (vector deleting destructor)</remarks>
    ~WarriorStatusWindow() override;
    /// <remarks>MCX.EXE @ 0x006ab690</remarks>
    void init(int32_t x, int32_t y, int32_t w, int32_t h, MechWarrior* newWarrior);
    /// <remarks>MCX.EXE @ 0x006ab6c0</remarks>
    void handleEvent(aEvent* event) override;
    /// <remarks>MCX.EXE @ 0x006ab6d0</remarks>
    void resize(int32_t w, int32_t h) override;
    /// <summary>Titles the window with the pilot's names and shows his wounds.</summary>
    /// <remarks>MCX.EXE @ 0x006ab6f0</remarks>
    void display() override;
    /// <remarks>MCX.EXE @ 0x006ab7d0</remarks>
    void draw() override;
    /// <summary>Port: still paints a picture (in display), so it keeps one.</summary>
    bool DrawsLive() override { return false; }
    /// <remarks>MCX.EXE @ 0x006a85d0</remarks>
    virtual MechWarrior* getWarrior() { return warrior; }

    /// <summary>The pilot shown.</summary>
    MechWarrior* warrior = nullptr; // +0x4c0
};

/// <summary>Order ids wrap at 255: compares two, treating 241-255 as before 1-15.</summary>
/// <remarks>MCX.EXE @ 0x006a2a10</remarks>
int32_t compareTacOrderId(int32_t id1, int32_t id2);
/// <summary>
/// Walks from <paramref name="start"/> toward <paramref name="end"/> (from the end back when <paramref
/// name="reverse"/>) in half-cell steps to the first passable cell, and returns the point on the ground there.
/// </summary>
/// <remarks>MCX.EXE @ 0x006a55a0</remarks>
vector_3d vectorOffset(vector_3d start, vector_3d end, int32_t reverse);
/// <summary>Does nothing (returns 1).</summary>
/// <remarks>MCX.EXE @ 0x006a99f0</remarks>
int32_t orderOrbitObject(GameObject* target);
/// <summary>Does nothing (returns 1).</summary>
/// <remarks>MCX.EXE @ 0x006a9a00</remarks>
int32_t orderUseOrbitRange(int32_t type, float range);

/// <summary>The brain's alarm handler names, by <see cref="PilotAlarmType"/>.</summary>
extern const char* pilotAlarmFunctionName[NUM_PILOT_ALARMS];
/// <summary>The skill names as the pilot files spell them.</summary>
extern const char* SkillsTable[NUM_SKILLS];
/// <summary>Per threat rating level: three effect factors. Not read by MCX's code.</summary>
extern float ThreatRatingEffect[8][3];
/// <summary>Seconds between brain runs (2).</summary>
extern float BrainUpdateFrequency;
/// <summary>"SkillWeightings" (1, 1, 1, 1).</summary>
extern float SkillWeightings[NUM_SKILLS];
/// <summary>"WarriorRankScale" (60, 75, 85, 999): the weighted skill below which each rank is.</summary>
extern float WarriorRankScale[4];
/// <summary>1: setMoveWayPath resets the way point and direction.</summary>
extern int InitWayPath;
/// <summary>The window the attack orders describe themselves in (debug builds made it; null in MCX).</summary>
extern ScrollingTextWindow* GameSystemWindow;
/// <summary>The pool the pilots' order queues are cut from (16 each).</summary>
extern _QueuedTacOrder TacOrderQueue[MAX_QUEUED_TACORDERS];
/// <summary>Slots of <see cref="TacOrderQueue"/> handed out (the scenario resets it). Unnamed in MCX.EXE (0x007f04f4).</summary>
extern int32_t TacOrderQueuePos;
/// <summary>The error of the last <see cref="MechWarrior::calcMovePath"/>.</summary>
extern int32_t LastMoveCalcErr;

// Warrior settings loadMoverGameSystem reads (MCX.EXE 0x007931d4..0x0079330c).

/// <summary>"FireOddsTable" (20, 35, 50, 65, 80).</summary>
extern float FireOddsTable[5];
/// <summary>"ProfessionalismTable".</summary>
extern int8_t ProfessionalismOffsetTable[5][2];
/// <summary>"DecorumTable".</summary>
extern int8_t DecorumOffsetTable[5][2];
/// <summary>"AmmoTable": {ammo percent, attack modifier} below which the modifier applies.</summary>
extern int8_t AmmoConservationModifiers[2][2];
/// <summary>"MovementUpdateFrequency" (5).</summary>
extern float MovementUpdateFrequency;
/// <summary>"CombatUpdateFrequency" (0.25).</summary>
extern float CombatUpdateFrequency;
/// <summary>"CommandUpdateFrequency" (6).</summary>
extern float CommandUpdateFrequency;
/// <summary>"ContactUpdateFrequency" (4).</summary>
extern float ContactUpdateFrequency;
/// <summary>"PilotCheckUpdateFrequency" (1).</summary>
extern float PilotCheckUpdateFrequency;
/// <summary>"PilotCheckModifiers" (25, 25).</summary>
extern int32_t PilotCheckModifierTable[2];
/// <summary>"GroupMoveTrailLength" ({0, 1} when missing): a group member's path is cut by
/// selectionIndex / [1] * [0] steps.</summary>
extern int32_t GroupMoveTrailLen[2];
/// <summary>"MoveTimeOut" (30 when missing).</summary>
extern float MoveTimeOut;
/// <summary>"MoveYieldTime" (1.5 when missing).</summary>
extern float MoveYieldTime;
/// <summary>"DefaultAttackRadius" (275 when missing).</summary>
extern float DefaultAttackRadius;
