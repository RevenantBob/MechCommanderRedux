#pragma once

#include "abl/MCAblModule.h"

#include "ai/MCMoveSystem.h"
#include "ai/MCTacticalOrder.h"
#include "gui/awindow.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "object/MCSortList.h"

class MCFile;
class MCFitIniFile;
class MCGameObject;
class MCMechWarrior;
class MCMover;
class MCMoverGroup;
class MCMovePath;
class MCRadio;
class MCScrollingTextWindow;
class MCSortList;
class MCTeam;
struct MCPathQueueRec;
struct MCAblSymbol;

/// <summary>Pilot skills (<see cref="SkillsTable"/> order).</summary>
constexpr int32_t NUM_SKILLS = 4;
/// <summary>Skill indices (<see cref="SkillsTable"/>: "Piloting", "Jumping", "Sensors", "Gunnery"). The names are
/// MechCommander 2's style; the original's aren't known.</summary>
constexpr int32_t MWS_PILOTING = 0;
constexpr int32_t MWS_JUMPING = 1;
constexpr int32_t MWS_SENSORS = 2;
constexpr int32_t MWS_GUNNERY = 3;
/// <summary>Pilot alarms (<see cref="MCPilotAlarmType"/>, <see cref="PilotAlarmFunctionName"/>).</summary>
constexpr int32_t NUM_PILOT_ALARMS = 0x11;
/// <summary>Triggers an alarm remembers until it is handled.</summary>
constexpr int32_t MAX_ALARM_TRIGGERS = 10;
/// <summary>Attackers a pilot remembers.</summary>
constexpr int32_t MAX_ATTACKERS = 50;
/// <summary>Brain memory cells ("Warrior%d" / "Cell" in the brain parameter file).</summary>
constexpr int32_t NUM_MEMORY_CELLS = 50;
/// <summary>Weapons a pilot tracks the status of (<see cref="MCMechWarrior::WeaponsStatus"/>).</summary>
constexpr int32_t MAX_WEAPONS_PER_WARRIOR = 0x20;
/// <summary>Queued orders each pilot gets from <see cref="TacOrderQueue"/>.</summary>
constexpr int32_t MAX_QUEUED_TACORDERS_PER_WARRIOR = 0x10;
/// <summary>Size of the shared order queue pool.</summary>
constexpr int32_t MAX_QUEUED_TACORDERS = 2000;

/// <summary>Which of the pilot's three order slots is current (<see cref="MCMechWarrior::OrderState"/>). Names are
/// MechCommander 2's.</summary>
enum MCOrderStateType
{
    ORDERSTATE_GENERAL = 0,
    ORDERSTATE_PLAYER = 1,
    ORDERSTATE_ALARM = 2,
    NUM_ORDERSTATES
};

/// <summary>
/// The pilot alarms, in <see cref="PilotAlarmFunctionName"/> order (the brain's handler for each). Names follow
/// MechCommander 2's, which match the handler names.
/// </summary>
enum MCPilotAlarmType
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
struct MCAttackerRec
{
    /// <summary>The attacker's part id.</summary>
    uint32_t AttackerId = 0;
    /// <summary>Scenario time of the last attack.</summary>
    float LastTime = 0;
};

/// <summary>An alarm raised on a pilot: its triggers since it was last handled.</summary>
/// <remarks>0x2c bytes. The original name isn't known (MC2: PilotAlarm).</remarks>
struct MCPilotAlarm
{
    /// <summary>Triggers remembered (at most <see cref="MAX_ALARM_TRIGGERS"/>); 0 when not raised.</summary>
    uint8_t NumTriggers = 0;
    /// <summary>What raised it (a part id, a cause code, a path error).</summary>
    uint32_t Trigger[MAX_ALARM_TRIGGERS]{};
};

/// <summary>A queued player order as the queue keeps it: its id, first way point and packed data.</summary>
/// <remarks>0x18 bytes. The original name is <c>_QueuedTacOrder</c> (from <see cref="TacOrderQueue"/>'s type).</remarks>
struct MCQueuedTacOrder
{
    int32_t Id = 0;
    MCVector3D Point;
    uint32_t PackedData[2]{};
};

/// <summary>A brain memory cell: an integer or a real ("MemType" 0 or 1).</summary>
union MCMemoryCell
{
    int32_t Integer;
    float Real;
};

/// <summary>The move states of <see cref="MCMoveOrders"/> (names are the port's).</summary>
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
struct MCMoveOrders
{
    /// <summary>Now; origin 1, speed type 3 (state 2, throttle 100); no goal; paths cleared.</summary>
    void Init();

    /// <summary>Scenario time the orders were given.</summary>
    float Time = 0;
    /// <summary>Who gave them.</summary>
    int8_t Origin = 0;
    /// <summary>How fast to move (setMoveSpeedType).</summary>
    int32_t SpeedType = 0;
    /// <summary>A set speed (setMoveSpeedVelocity).</summary>
    float SpeedVelocity = 0;
    /// <summary>Speed state for the set speed.</summary>
    int8_t SpeedState = 0;
    /// <summary>Throttle for the set speed.</summary>
    int8_t SpeedThrottle = 0;
    /// <summary>The goal: -1 none, 0 a location, else the goal object's part id.</summary>
    int32_t GoalType = 0;
    /// <summary>The goal object.</summary>
    MCGameObject* GoalObject = nullptr;
    /// <summary>The goal object's position when the path was requested.</summary>
    MCVector3D GoalObjectPosition;
    /// <summary>The goal location (-999999 for none).</summary>
    MCVector3D GoalLocation;
    /// <summary>When the path was last planned plus MovementUpdateFrequency.</summary>
    float NextUpdate = 0;
    /// <summary>Set by ABL setmovegoal (hasmovegoal / hasmovepath test it); only init clears it.</summary>
    int32_t ScriptGoal = 0;
    /// <summary>The way points (setMoveWayPath, addMoveWayPoint).</summary>
    MCVector3D WayPath[MaxWayPoints];
    /// <summary>Way points held.</summary>
    int8_t NumWayPts = 0;
    /// <summary>The way point being walked to.</summary>
    int8_t CurWayPt = 0;
    /// <summary>1 to patrol (walk the way path back and forth).</summary>
    int8_t CurWayDir = 0;
    /// <summary>0 none, 1 a single (local) path, 2 a global path.</summary>
    int8_t PathType = 0;
    /// <summary>The goal as first asked for (-999999 by init).</summary>
    MCVector3D OriginalGlobalGoal[2];
    /// <summary>The goal the path actually reaches (-666666 by init).</summary>
    MCVector3D GlobalGoalLocation;
    /// <summary>The global path, area by area.</summary>
    MCGlobalPathStep GlobalPath[MCGlobalMap::MaxPathSteps]{};
    /// <summary>Steps of the global path.</summary>
    int8_t NumGlobalSteps = 0;
    /// <summary>The global step being walked.</summary>
    int8_t CurGlobalStep = 0;
    /// <summary>The move paths: the one walked, and the next leg (made by MechWarrior::init).</summary>
    MCMovePath* Path[2]{};
    /// <summary>When the move was ordered; the move times out MoveTimeOut after it (-1 for none).</summary>
    float TimeOfLastStep = 0;
    /// <summary>1 by init; 3-5 are the combat moves calcMovePath resets.</summary>
    int32_t MoveState = 0;
    /// <summary>1 by init; 5 to hold position while attacking.</summary>
    int32_t MoveStateGoal = 0;
    /// <summary>Set when the vehicle switches moveStateGoal to back away from (or turn back to) a target outside its
    /// torso or turret arc; cleared once it moves forward again or gets new orders. While set the movement tree
    /// requests no new attack paths.</summary>
    int32_t MoveStateGoalChanged = 0;
    /// <summary>When yielding to a blocking mover, the time to give up and look again (-1 for none).</summary>
    float YieldTime = 0;
    int32_t YieldState = 0;
    /// <summary>When waiting for the group's point to move (-1 for none).</summary>
    float WaitForPointTime = 0;
    /// <summary>Run to the goal.</summary>
    int32_t Run = 0;
};

/// <summary>A pilot's attack orders.</summary>
/// <remarks>Original source: <c>object\warrior.cpp</c>; 0x28 bytes. Names follow MechCommander 2's AttackOrders.</remarks>
struct MCAttackOrders
{
    /// <summary>Now; origin 1; no target; aim location -1.</summary>
    void Init();

    /// <summary>Scenario time the orders were given.</summary>
    float Time = 0;
    /// <summary>Who gave them.</summary>
    int8_t Origin = 0;
    /// <summary>The attack type.</summary>
    int32_t Type = 0;
    /// <summary>The target.</summary>
    MCGameObject* Target = nullptr;
    /// <summary>The point attacked.</summary>
    MCVector3D TargetPoint;
    /// <summary>Body location aimed at; -1 for any.</summary>
    int32_t AimLocation = 0;
    /// <summary>Whether to follow the target.</summary>
    int32_t Pursue = 0;
    /// <summary>Scenario time the target was set (setAttackTarget), -1 for none.</summary>
    float TargetTime = 0;
};

/// <summary>
/// A pilot: name and portrait, skills and personality, wounds, the ABL brain, its orders (general, player and alarm,
/// the current one and the queue), move and attack orders, and the radio. Drives its vehicle through the decision
/// trees.
/// </summary>
/// <remarks>Original source: <c>object\warrior.cpp</c>, <c>object\warrior.h</c>; 0x1e5c bytes.
/// Field names are the port's where the original's aren't known (MechCommander 2's where the layout
/// matches).</remarks>
class MCMechWarrior
{
public:
    /// <summary>Drops the brain and its callbacks.</summary>
    void Lobotomy();
    /// <summary>
    /// Resets every field (the constructor's work): default personality 40, update times staggered by warrior
    /// count, orders cleared, two move paths made; counts the warrior and makes the shared sort list.
    /// </summary>
    void Init();
    /// <summary>
    /// Reads the pilot file: name, picture, callsign, audio/video and radio, paint scheme, personality, skills
    /// (current, original, latest, points), wounds; rolls whether the pilot survives ejection.
    /// </summary>
    int32_t Init(MCFitIniFile* warriorFile);
    /// <summary>
    /// Plays radio message <paramref name="messageId"/> when the pilot is under the local player's command
    /// (skipping repeats too soon after the last); a server passes others' messages on as radio chunks.
    /// </summary>
    void RadioMessage(int32_t messageId, int propogateIfMultiplayer);
    /// <summary>Frees the strings, the brain and the paths; the last warrior frees the sort list.</summary>
    void Destroy();
    /// <summary>The aggressiveness; halfway to 100 while on a combat order when <paramref name="current"/>.</summary>
    int32_t GetAggressiveness(int current);
    /// <summary>Takes the pilot's 16 slots of <see cref="TacOrderQueue"/>; 0 when the pool is used up.</summary>
    int EnableTacOrderQueue();
    /// <summary>Queues a player order (0; 1 without a queue, 2 when full); starts it when it is the only one.</summary>
    int32_t AddQueuedTacOrder(MCTacticalOrder tacOrder);
    /// <summary>Takes the first queued order (0; 1 without a queue, 2 when empty).</summary>
    int32_t RemoveQueuedTacOrder(MCTacticalOrder* tacOrder);
    /// <summary>Reads the first queued order without taking it (0; 1 without a queue, 2 when empty).</summary>
    int32_t PeekQueuedTacOrder(MCTacticalOrder* tacOrder);
    void ClearTacOrderQueue();
    /// <summary>Makes the next queued order the player order.</summary>
    void ExecuteTacOrderQueue();
    void LockTacOrderQueue();
    void UnlockTacOrderQueue();
    /// <summary>
    /// Copies the player order from the queue (when one runs) and the queued orders into <paramref name="list"/>
    /// (which may be null); returns how many.
    /// </summary>
    int32_t GetTacOrderQueue(MCQueuedTacOrder* list);
    /// <summary>
    /// On a client: drops the queued orders the server has executed (those before <paramref name="tacOrderId"/>,
    /// or the last executed one when 0).
    /// </summary>
    void UpdateClientOrderQueue(int32_t tacOrderId);
    MCMoverGroup* GetGroup();
    MCMover* GetPoint();
    int OnHomeTeam();
    /// <summary>Whether the vehicle belongs to a player (its net player id is set).</summary>
    int UnderHomeCommand();
    /// <summary>Rolls a skill check (counting the try and a success); returns the margin, negative on a failure.</summary>
    int32_t CheckSkill(int32_t skillId, float factor);
    /// <summary>Adds wounds; at 6 the pilot dies (or ejects when <paramref name="checkEject"/> and the roll allowed
    /// it) and the vehicle is disabled.</summary>
    int Injure(float numWounds, int checkEject);
    /// <summary>Ejects: a wound, the radio call, status 3 (4 if that killed him), the vehicle disabled.</summary>
    void Eject();
    void SetTeam(MCTeam* newTeam);
    void SetVehicle(MCGameObject* newVehicle);
    MCGameObject* GetVehicle() { return Vehicle; }
    void SetBrainName(char* brainName);
    /// <summary>Replaces the brain with a new module of <paramref name="brainHandle"/> and finds its alarm handlers.</summary>
    int32_t SetBrain(int32_t brainHandle);
    int32_t RunBrain();
    /// <summary>The vehicle's status, -1 without one.</summary>
    int32_t GetVehicleStatus();
    void UpdateAttackerStatus(uint32_t attackerId, float time);
    MCAttackerRec* GetAttackerInfo(uint32_t attackerId);
    /// <summary>The attackers of the last <paramref name="seconds"/>.</summary>
    int32_t GetAttackers(uint32_t* attackerList, float seconds);
    int32_t SetAttackTarget(MCGameObject* object);
    /// <summary>The last target while it stays worth shooting; else clears it (and a combat order).</summary>
    MCGameObject* GetLastTarget();
    /// <summary>
    /// Sets the last target (moving the vehicle's attacker count to it when the vehicle belongs to a player), and
    /// whether a disabled target still counts (<paramref name="obliterate"/>) and to conserve ammo.
    /// </summary>
    void SetLastTarget(MCGameObject* target, int obliterate = 0, int conserveAmmo = 0);
    void SetCurrentTarget(MCGameObject* target);
    MCGameObject* GetAttackTargetPosition(MCVector3D& pos);
    void ClearAttackOrders();
    void ClearMoveOrders();
    /// <summary>Sets the move goal: <paramref name="type"/> -1 none, 0 a location, else an object's part id.</summary>
    int32_t SetMoveGoal(uint32_t type, MCVector3D* location, MCGameObject* obj);
    void PausePath();
    void ResumePath();
    /// <summary>
    /// The vehicle reached the end of its path: asks for the next leg (a queued move point or the next global step),
    /// or ends the move order and raises the no-path alarm.
    /// </summary>
    void ReachedPathEnd();
    float GetMoveDistanceLeft();
    int IsJumping(MCVector3D* jumpGoal);
    /// <summary>The path being walked; when it is done, swaps in the next leg.</summary>
    MCMovePath* GetMovePath();
    void SetMoveWayPath(MCWayPath* wayPath, int patrol);
    void AddMoveWayPoint(MCVector3D wayPt, int patrol);
    void SetMoveGlobalPath(MCGlobalPathStep* path, int32_t numSteps);
    /// <summary>Queues a path request with the <see cref="PathManager()"/>.</summary>
    void RequestMovePath(int32_t selectionIndex, uint32_t moveParams, int32_t source);
    /// <summary>
    /// Plans the move to the goal: a local path when close (or no long-range movement), else a global path
    /// area by area, each leg planned as the previous one ends; escape paths and jumps too. Sets
    /// <see cref="LastMoveCalcErr"/> and raises the no-path alarm on a failure.
    /// </summary>
    int32_t CalcMovePath(int32_t selectionIndex, uint32_t moveParams, int32_t source);
    /// <summary>The first way point of a queued move order, if the next queued order is one.</summary>
    int GetNextWayPoint(MCVector3D& nextPoint, int incWayPoint);
    /// <summary>
    /// Per weapon, whether it can fire at the target now (the attack chance, or -1 not ready, -2 no ammo, -3 out
    /// of range, -4 not locked, -5 no chance); returns how many can, or -1 can't fire, -2 no target, -3 out of range.
    /// </summary>
    int32_t CalcWeaponsStatus(MCGameObject* target, int32_t* weaponList, MCVector3D* targetPoint);
    /// <summary>Fires what can fire at the last target; out of ammo ends the order.</summary>
    int32_t CombatDecisionTree();
    /// <summary>A point away from the enemy, walking the escape vector until off the map or blocked.</summary>
    MCVector3D CalcWithdrawGoal(float withdrawRange);
    /// <summary>Whether the vehicle is on, or its path crosses, a blown bridge.</summary>
    int MovingOverBlownBridge();
    /// <summary>Times out moves, reroutes around gates and blown bridges, and keeps attack moves in range.</summary>
    int MovementDecisionTree();
    /// <summary>Ends the current order; with <paramref name="updateTacOrder"/> the next order (player, then
    /// general) starts.</summary>
    void ClearCurTacOrder(int updateTacOrder, int updateBrain);
    void SetCurTacOrder(MCTacticalOrder tacOrder);
    void SetGeneralTacOrder(MCTacticalOrder tacOrder);
    void SetPlayerTacOrder(MCTacticalOrder tacOrder, int fromQueue);
    /// <summary>Sets the alarm order unless one of higher priority is waiting.</summary>
    void SetAlarmTacOrder(MCTacticalOrder tacOrder, int32_t priority);
    /// <summary>Raises an alarm (up to <see cref="MAX_ALARM_TRIGGERS"/> triggers); -1 when full.</summary>
    int32_t TriggerAlarm(int32_t alarmCode, uint32_t triggerId);
    /// <summary>Handles an alarm at once (awake vehicles only): the built-in handler, then the brain's.</summary>
    int32_t HandleAlarm(int32_t alarmCode, uint32_t triggerId);
    int32_t GetAlarmTriggers(int32_t alarmCode, uint32_t* triggerList);
    void ClearAlarm(int32_t alarmCode);
    /// <summary>Handles each raised alarm (built-in handler, then the brain's) and clears it.</summary>
    int32_t CheckAlarms();
    /// <summary>
    /// The per-frame update the AI and network controls call: order status, weapons status, the brain, alarms,
    /// the next order, then the combat and movement trees.
    /// </summary>
    int32_t MainDecisionTree();
    /// <summary>A captured vehicle drops its orders; else the combat and movement trees run.</summary>
    void UpdateActions();
    void SetDebugFlag(uint32_t flag, int on);
    int GetDebugFlag(uint32_t flag);
    void DebugPrint(char* s, int debugMode);
    void DebugOrders();
    void SetMoveSpeedType(int32_t type);
    void SetMoveSpeedVelocity(float speed);
    int32_t OpenStatusWindow(int32_t x, int32_t y, int32_t w, int32_t h);
    int32_t CloseStatusWindow();
    int32_t OrderWait(int unitOrder, MCOrderOrigin origin, int32_t seconds, int clearLastTarget);
    int32_t OrderStop(int unitOrder, int setTacOrder);
    int32_t OrderMoveToPoint(int unitOrder, int setTacOrder, MCOrderOrigin origin, MCVector3D location,
                             int32_t selectionIndex, uint32_t params);
    int32_t OrderMoveToObject(int unitOrder, int setTacOrder, MCOrderOrigin origin, MCGameObject* target,
                              int32_t selectionIndex, uint32_t params);
    int32_t OrderJumpToPoint(int unitOrder, int setTacOrder, MCOrderOrigin origin, MCVector3D location,
                             int32_t selectionIndex);
    int32_t OrderJumpToObject(int unitOrder, int setTacOrder, MCOrderOrigin origin, MCGameObject* target,
                              int32_t selectionIndex);
    int32_t OrderTraversePath(int unitOrder, int setTacOrder, MCOrderOrigin origin, MCWayPath* wayPath,
                              uint32_t params);
    int32_t OrderPatrolPath(int unitOrder, int setTacOrder, MCOrderOrigin origin, MCWayPath* wayPath);
    int32_t OrderPowerUp(int unitOrder, MCOrderOrigin origin);
    int32_t OrderPowerDown(int unitOrder, MCOrderOrigin origin);
    int32_t OrderUseSpeed(float speed);
    /// <summary>Does nothing (returns 1).</summary>
    int32_t OrderOrbitPoint(MCVector3D location);
    int32_t OrderAttackObject(int unitOrder, MCOrderOrigin origin, MCGameObject* target, int32_t type, int32_t method,
                              int32_t range, int32_t aimLocation, uint32_t params);
    int32_t OrderAttackPoint(int unitOrder, MCOrderOrigin origin, MCVector3D location, int32_t type, int32_t method,
                             int32_t range, uint32_t params);
    void SetAttackTargetPoint(MCVector3D location) { AttackOrders.TargetPoint = location; }
    MCVector3D GetAttackTargetPoint() { return AttackOrders.TargetPoint; }
    int32_t OrderWithdraw(int unitOrder, MCOrderOrigin origin, MCVector3D location);
    int32_t OrderEject(int unitOrder, int setTacOrder, MCOrderOrigin origin);
    int32_t OrderUseFireRange(int32_t range);
    int32_t OrderUseFireOdds(int32_t odds);
    int32_t OrderRefit(MCOrderOrigin origin, MCGameObject* target, uint32_t params);
    int32_t OrderGetFixed(MCOrderOrigin origin, MCGameObject* target, uint32_t params);
    int32_t OrderLoadIntoCarrier(MCOrderOrigin origin, MCGameObject* target, uint32_t params);
    int32_t OrderDeployElementals(MCOrderOrigin origin, uint32_t params);
    int32_t OrderCapture(MCOrderOrigin origin, MCGameObject* target, uint32_t params);
    int32_t HandleTargetOfWeaponFire();
    int32_t HandleHitByWeaponFire();
    int32_t HandleCollision();
    int32_t HandleDamageTakenRate();
    int32_t HandleUnitMateDeath();
    int32_t HandleFriendlyVehicleCrippled();
    int32_t HandleFriendlyVehicleDestruction();
    int32_t HandleOwnVehicleIncapacitation(uint32_t cause);
    int32_t HandleOwnVehicleDestruction(uint32_t cause);
    int32_t HandleOwnVehicleWithdrawn();
    int32_t HandleMoraleBreak();
    int32_t HandleCollisionAlert();
    /// <summary>Counts the kill, calls it in, and scores gunnery skill points (a tenth for an ally's).</summary>
    int32_t HandleKilledTarget();
    int32_t HandleUnitMateFiredWeapon();
    int32_t HandlePlayerOrder();
    int32_t HandleNoMovePath();
    int32_t HandleGateClosing();
    int32_t MissionLog(MCFile* file, int32_t unitLevel);
    /// <summary>The rank from the weighted skill ranks (<see cref="SkillWeightings"/>, <see cref="WarriorRankScale"/>).</summary>
    void CalcRank();
    /// <summary>Reads section "Warrior<paramref name="warriorId"/>": the brain's memory cells and static variables.</summary>
    int32_t LoadBrainParameters(MCFitIniFile* brainFile, int32_t warriorId);

    /// <summary>"Name".</summary>
    char* Name = nullptr;
    /// <summary>"Callsign".</summary>
    char* Callsign = nullptr;
    /// <summary>"Picture", default "pilotx.gif".</summary>
    char* Picture = nullptr;
    /// <summary>"pilotVideo".</summary>
    char* VideoStr = nullptr;
    /// <summary>"pilotAudio".</summary>
    char* AudioStr = nullptr;
    /// <summary>The brain's name.</summary>
    char* BrainStr = nullptr;
    /// <summary>The pilot's index in Scenario::warriors (selectwarrior / getwarriorstatus take it).</summary>
    int32_t Index = 0;
    /// <summary>"PaintScheme", -1 for none.</summary>
    int32_t PaintScheme = -1;
    /// <summary>Rank (calcRank).</summary>
    uint8_t Rank = 0;
    /// <summary>Current skills (<see cref="SkillsTable"/>).</summary>
    int8_t Skills[NUM_SKILLS] = {};
    /// <summary>"Professionalism" (40 by default).</summary>
    int8_t Professionalism = 40;
    /// <summary>"Decorum".</summary>
    int8_t Decorum = 40;
    /// <summary>"Aggressiveness".</summary>
    int8_t Aggressiveness = 40;
    /// <summary>"Courage".</summary>
    int8_t Courage = 40;
    /// <summary>The courage read ("Courage"), before morale changes it.</summary>
    int8_t BaseCourage = 0;
    /// <summary>"Wounds"; 6 is death.</summary>
    float Wounds = 0.0f;
    /// <summary>The pilot's status: 0 normal, 2 withdrawn, 3 ejected, 4 dead.</summary>
    int32_t Status = 0;
    /// <summary>Rolled at load: survives ejection.</summary>
    int32_t EscapesThruEjection = 0;
    /// <summary>When "under attack" was last called in (at most every 20 seconds).</summary>
    float LastUnderAttackTime = -1000.0f;
    /// <summary>"Weapons at 50%" was called in.</summary>
    int32_t Weapons50Sent = 0;
    /// <summary>"Weapons out" was called in.</summary>
    int32_t WeaponsOutSent = 0;
    /// <summary>When a sensor contact was last called in (at most every 15 seconds).</summary>
    float LastContactTime = -1000.0f;
    /// <summary>The last message type played.</summary>
    int32_t LastMessageType = 0;
    /// <summary>What Radio::playMessage returned for it.</summary>
    int32_t LastMessage = -1;
    /// <summary>When it played (repeats within 10 seconds are skipped).</summary>
    float LastMessageTime = 0.0f;
    /// <summary>"NotMineYet".</summary>
    int32_t NotMineYet = 0;
    /// <summary>The team.</summary>
    MCTeam* Team = nullptr;
    /// <summary>The vehicle piloted.</summary>
    MCGameObject* Vehicle = nullptr;
    /// <summary>The team's alignment.</summary>
    int8_t Alignment = 0;
    /// <summary>Per skill: tries (two counters each; checkSkill counts the second). Mission statistics.</summary>
    int32_t NumSkillUses[NUM_SKILLS][2] = {};
    /// <summary>Per skill: successes.</summary>
    int32_t NumSkillSuccesses[NUM_SKILLS][2] = {};
    /// <summary>Kills by kind (mech class 1-4, 5 vehicles and turrets, 6 elementals; the second counter counts).</summary>
    int32_t NumKilled[7][2] = {};
    /// <summary>Enemy mechs rammed on an attack order (BattleMechType::handleCollision).</summary>
    int32_t NumRams = 0;
    /// <summary>Jumps landed on the jump order's target (BattleMechType::handleCollision).</summary>
    int32_t NumJumpAttacks = 0;
    /// <summary>Per skill: the skill as a float.</summary>
    float SkillRank[NUM_SKILLS] = {};
    /// <summary>"SkillPoints" per skill.</summary>
    float SkillPoints[NUM_SKILLS] = {};
    /// <summary>"OriginalSkills".</summary>
    int8_t OriginalSkills[NUM_SKILLS] = {};
    /// <summary>"LatestSkills".</summary>
    int8_t LatestSkills[NUM_SKILLS] = {};
    /// <summary>"DescIndex", -1 for none.</summary>
    int32_t DescIndex = -1;
    /// <summary>"NameIndex", -1 for none.</summary>
    int32_t NameIndex = -1;
    /// <summary>When a home-team pilot was last left without orders (-1 while ordered).</summary>
    float TimeOfLastOrders = -1.0f;
    /// <summary>The attackers remembered.</summary>
    MCAttackerRec Attackers[MAX_ATTACKERS] = {};
    /// <summary>How many.</summary>
    int32_t NumAttackers = 0;
    /// <summary>DefaultAttackRadius.</summary>
    float AttackRadius = 0.0f;
    /// <summary>The brain's memory cells (loadBrainParameters).</summary>
    MCMemoryCell Memory[NUM_MEMORY_CELLS] = {};
    /// <summary>The ABL brain.</summary>
    std::unique_ptr<MCAblModule> Brain;
    /// <summary>The brain's alarm handlers (<see cref="PilotAlarmFunctionName"/>).</summary>
    MCAblSymbol* BrainAlarmCallback[NUM_PILOT_ALARMS] = {};
    /// <summary>Next brain run (staggered by warrior count, then every BrainUpdateFrequency).</summary>
    float BrainUpdateTime = 0.0f;
    /// <summary>Next combat update time.</summary>
    float CombatUpdateTime = 0.0f;
    /// <summary>Next movement update time.</summary>
    float MovementUpdateTime = 0.0f;
    /// <summary>Per weapon: its status against the target (<see cref="CalcWeaponsStatus"/>).</summary>
    int32_t WeaponsStatus[MAX_WEAPONS_PER_WARRIOR] = {};
    /// <summary>What <see cref="CalcWeaponsStatus"/> returned (-2 by init).</summary>
    int32_t WeaponsStatusResult = -2;
    /// <summary>Per order state: a new order is waiting in <see cref="TacOrder"/>.</summary>
    int32_t NewTacOrderReceived[NUM_ORDERSTATES] = {};
    /// <summary>The waiting orders: general, player, alarm (0x138 bytes each).</summary>
    MCTacticalOrder TacOrder[NUM_ORDERSTATES];
    /// <summary>The last order started.</summary>
    MCTacticalOrder LastTacOrder;
    /// <summary>The current tactical order.</summary>
    MCTacticalOrder CurTacOrder;
    /// <summary>The alarms (<see cref="MCPilotAlarmType"/>).</summary>
    MCPilotAlarm Alarm[NUM_PILOT_ALARMS] = {};
    /// <summary>The priority of the waiting alarm order.</summary>
    int32_t AlarmPriority = 0;
    /// <summary>The player order came from the queue.</summary>
    int32_t PlayerOrderFromQueue = 0;
    /// <summary>Set while the queue is locked.</summary>
    int32_t TacOrderQueueLocked = 0;
    /// <summary>Set while the queue is executing.</summary>
    int32_t TacOrderQueueExecuting = 0;
    /// <summary>Orders queued.</summary>
    int8_t NumTacOrdersQueued = 0;
    /// <summary>The pilot's slots of <see cref="TacOrderQueue"/>.</summary>
    MCQueuedTacOrder* QueuedOrders = nullptr;
    /// <summary>The next tactical order id (1 by init).</summary>
    int32_t NextTacOrderId = 1;
    /// <summary>The id of the last order the server executed.</summary>
    int32_t LastTacOrderId = 0;
    /// <summary>Movement orders.</summary>
    MCMoveOrders MoveOrders;
    /// <summary>Attack orders.</summary>
    MCAttackOrders AttackOrders;
    /// <summary>The fire range ordered (orderUseFireRange), -1 for none.</summary>
    float OrderFireRange = -1.0f;
    /// <summary>The fire odds ordered (orderUseFireOdds), -1 for none.</summary>
    float OrderFireOdds = -1.0f;
    /// <summary>The last target (getLastTarget).</summary>
    MCGameObject* LastTarget = nullptr;
    /// <summary>When it was set, -1 for none.</summary>
    float LastTargetTime = -1.0f;
    /// <summary>Keep shooting it when disabled.</summary>
    int32_t LastTargetObliterate = 0;
    /// <summary>It is on our side.</summary>
    int32_t LastTargetFriendly = 0;
    /// <summary>Conserve ammo on it (the attack becomes type 3).</summary>
    int32_t LastTargetConserveAmmo = 0;
    /// <summary>Which of <see cref="TacOrder"/> is current (<see cref="MCOrderStateType"/>).</summary>
    int8_t OrderState = ORDERSTATE_GENERAL;
    /// <summary>The pending path request.</summary>
    MCPathQueueRec* MovePathRequest = nullptr;
    /// <summary>Debug flags.</summary>
    uint32_t DebugFlags = 0;
    /// <summary>The status window.</summary>
    MCGuiTitleWindow* StatusWindow = nullptr;
    /// <summary>The radio.</summary>
    MCRadio* Radio = nullptr;
    /// <summary>"OldPilot".</summary>
    uint8_t OldPilot = 0;
    /// <summary>"Ammo out" was called in.</summary>
    int32_t AmmoOutSent = 0;

    /// <summary>Warriors alive.</summary>
    static int32_t NumWarriors;
    /// <summary>Warriors in combat.</summary>
    static int32_t NumWarriorsInCombat;
    /// <summary>The warriors' shared sort list.</summary>
    static MCSortList* SortList;
};

/// <summary>The scenario's warriors by index.</summary>
/// <remarks>Original source: <c>object\warrior.cpp</c>; 8 bytes.</remarks>
class MCMechWarriorManager
{
public:
    /// <summary>Deletes every warrior.</summary>
    void Destroy();
    /// <summary>Room for <paramref name="numWarriors"/>.</summary>
    void Init(int32_t numWarriors);
    void Set(int32_t index, MCMechWarrior* warrior);
    MCMechWarrior* Get(int32_t index);

    /// <summary>How many.</summary>
    int32_t NumWarriors = 0;
    /// <summary>The warriors.</summary>
    std::unique_ptr<MCMechWarrior*[]> Warriors;
};

/// <summary>A pilot's status window.</summary>
/// <remarks>Original source: <c>object\warrior.cpp</c>, <c>object\warrior.h</c>; 0x4c4 bytes.</remarks>
class MCWarriorStatusWindow : public MCGuiTitleWindow
{
public:
    ~MCWarriorStatusWindow() override;
    void Init(int32_t x, int32_t y, int32_t w, int32_t h, MCMechWarrior* newWarrior);
    void HandleEvent(MCGuiEvent* event) override;
    void Resize(int32_t w, int32_t h) override;
    /// <summary>Titles the window with the pilot's names and shows his wounds.</summary>
    void Display() override;
    void Draw() override;
    /// <summary>Port: still paints a picture (in display), so it keeps one.</summary>
    bool DrawsLive() override { return false; }
    virtual MCMechWarrior* GetWarrior() { return Warrior; }

    /// <summary>The pilot shown.</summary>
    MCMechWarrior* Warrior = nullptr;
};

/// <summary>Order ids wrap at 255: compares two, treating 241-255 as before 1-15.</summary>
int32_t CompareTacOrderId(int32_t id1, int32_t id2);
/// <summary>
/// Walks from <paramref name="start"/> toward <paramref name="end"/> (from the end back when <paramref
/// name="reverse"/>) in half-cell steps to the first passable cell, and returns the point on the ground there.
/// </summary>
MCVector3D VectorOffset(MCVector3D start, MCVector3D end, int32_t reverse);
/// <summary>Does nothing (returns 1).</summary>
int32_t OrderOrbitObject(MCGameObject* target);
/// <summary>Does nothing (returns 1).</summary>
int32_t OrderUseOrbitRange(int32_t type, float range);

/// <summary>The brain's alarm handler names, by <see cref="MCPilotAlarmType"/>.</summary>
extern const char* PilotAlarmFunctionName[NUM_PILOT_ALARMS];
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
extern MCScrollingTextWindow* GameSystemWindow;
/// <summary>The pool the pilots' order queues are cut from (16 each).</summary>
extern MCQueuedTacOrder TacOrderQueue[MAX_QUEUED_TACORDERS];
/// <summary>Slots of <see cref="TacOrderQueue"/> handed out (the scenario resets it). Unnamed in MCX.EXE (0x007f04f4).</summary>
extern int32_t TacOrderQueuePos;
/// <summary>The error of the last <see cref="MCMechWarrior::CalcMovePath"/>.</summary>
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
