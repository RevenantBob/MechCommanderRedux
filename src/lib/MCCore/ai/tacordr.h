#pragma once

#include "lib/cvmath.h"

class MCGameObject;
class MCMover;
class MCMoverGroup;
class MCMechWarrior;
struct MCLocationNode;

/// <summary>Waypoints a tactical order's way path holds.</summary>
inline constexpr int32_t MAX_WAYPTS = 15;

/// <summary>Who gave a tactical order (packed in 2 bits).</summary>
enum MCOrderOriginType
{
    ORDER_ORIGIN_PLAYER = 0,
    ORDER_ORIGIN_COMMANDER = 1,
    ORDER_ORIGIN_SELF = 2,
    NUM_ORDER_ORIGINS
};

/// <summary>
/// What a tactical order does (packed in 5 bits; 0x1f encodes a GUARD order on a point).
/// </summary>
/// <remarks>
/// Values from TacticalOrder::execute / status / is*Order; names follow MechCommander 2's enum, whose values match
/// every case MCX.EXE uses. HOLD_FIRE (0x12) executes MechWarrior::orderWait.
/// </remarks>
enum MCTacticalOrderCode
{
    TACTICAL_ORDER_NONE = 0,
    TACTICAL_ORDER_WAIT = 1,
    TACTICAL_ORDER_MOVETO_POINT = 2,
    TACTICAL_ORDER_MOVETO_OBJECT = 3,
    TACTICAL_ORDER_JUMPTO_POINT = 4,
    TACTICAL_ORDER_JUMPTO_OBJECT = 5,
    TACTICAL_ORDER_TRAVERSE_PATH = 6,
    TACTICAL_ORDER_PATROL_PATH = 7,
    TACTICAL_ORDER_ESCORT = 8,
    TACTICAL_ORDER_FOLLOW = 9,
    TACTICAL_ORDER_GUARD = 10,
    TACTICAL_ORDER_STOP = 11,
    TACTICAL_ORDER_POWERUP = 12,
    TACTICAL_ORDER_POWERDOWN = 13,
    TACTICAL_ORDER_WAYPOINTS_DONE = 14,
    TACTICAL_ORDER_EJECT = 15,
    TACTICAL_ORDER_ATTACK_OBJECT = 16,
    TACTICAL_ORDER_ATTACK_POINT = 17,
    TACTICAL_ORDER_HOLD_FIRE = 18,
    TACTICAL_ORDER_WITHDRAW = 19,
    TACTICAL_ORDER_SCRAMBLE = 20,
    TACTICAL_ORDER_CAPTURE = 21,
    TACTICAL_ORDER_REFIT = 22,
    TACTICAL_ORDER_GETFIXED = 23,
    TACTICAL_ORDER_LOAD_INTO_CARRIER = 24,
    TACTICAL_ORDER_DEPLOY_ELEMENTALS = 25,
    NUM_TACTICAL_ORDERS
};

/// <summary>A list of waypoints.</summary>
/// <remarks>Original: <c>struct _WayPath</c> (MechWarrior::setMoveWayPath). 0xcc bytes.</remarks>
typedef struct MCWayPath
{
    int32_t NumPoints = 0;
    int32_t CurPoint = 0;
    /// <summary>x, y, z of each waypoint.</summary>
    float Points[MAX_WAYPTS * 3]{};
    /// <summary>Per waypoint, 1 to run to it.</summary>
    uint8_t Mode[MAX_WAYPTS]{};
} MCWayPath;

/// <summary>The movement part of a tactical order.</summary>
/// <remarks>0xdc bytes (copied as one block by TacticalOrder's copy). The original name isn't known
/// (MC2: TacOrderMoveParams); the flag names are MC2's, their use in MCX noted.</remarks>
typedef struct MCTacOrderMoveParams
{
    MCWayPath WayPath{};
    /// <summary>MOVETO_OBJECT: passed as move flag 4 (default 1).</summary>
    int32_t FaceObject = 0;
    /// <summary>MOVETO_POINT: move flag 2.</summary>
    int32_t Wait = 0;
    /// <summary>1 adds move flag 8.</summary>
    int32_t Mode = 0;
    /// <summary>MOVETO_POINT: move flag 0x40.</summary>
    int32_t EscapeTile = 0;
} MCTacOrderMoveParams;

/// <summary>The attack part of a tactical order.</summary>
/// <remarks>Original: <c>struct _TacOrderAttackParams</c> (copy constructor at 0x006a8130). 0x24 bytes.</remarks>
typedef struct MCTacOrderAttackParams
{
    /// <summary>Attack type (default 1, packed in 2 bits).</summary>
    int32_t Type = 0;
    /// <summary>Attack method (2 = ramming, see TacticalOrder::getRamTarget).</summary>
    int32_t Method = 0;
    /// <summary>Fire range: a Mover::getFireRange selector, -4..2 (packed + 4 in 3 bits).</summary>
    int32_t Range = 0;
    /// <summary>Aimed location, -1 for none (packed + 2 in 4 bits; Mover::sortWeapons aims with it).</summary>
    int32_t AimLocation = 0;
    int32_t Pursue = 0;
    int32_t Obliterate = 0;
    MCVector3D TargetPoint;
} MCTacOrderAttackParams;

/// <summary>
/// An order to one pilot or a group: its code, parameters, target and progress. Orders are queued on pilots and
/// packed into two 32-bit words to be sent over the network.
/// </summary>
/// <remarks>Original source: <c>ai\tacordr.cpp</c> (operator= inline in <c>ai\tacordr.h</c>). 0x138 bytes. No
/// constructor: new orders are set up with <see cref="init()"/>. The compiler-generated copy constructor is emitted
/// at 0x00602ea0 (network\multplyr.cpp).</remarks>
class MCTacticalOrder
{
public:
    /// <summary>Copies an order and destroys the (by-value) source.</summary>
    void operator=(MCTacticalOrder copy);

    void Init();
    void Init(MCOrderOriginType origin, MCTacticalOrderCode code, int unitOrder = 0);
    /// <summary>Copies a location list into the way path (at most MAX_WAYPTS).</summary>
    void InitWayPath(MCLocationNode* path);
    MCVector3D GetWayPoint(int32_t index);
    void SetWayPoint(int32_t index, MCVector3D wayPoint);
    void AddWayPoint(MCVector3D wayPoint, int32_t run);
    /// <summary>The target of a ramming attack, else null.</summary>
    MCGameObject* GetRamTarget();
    /// <summary>The target of a JUMPTO_POINT order, else null.</summary>
    MCGameObject* GetJumpTarget();
    int IsGroupOrder();
    int IsCombatOrder();
    int IsMoveOrder();
    int IsWayPathOrder();
    int IsJumpOrder();
    /// <summary>Takes the pilot's next order id (1-255).</summary>
    void SetId(MCMechWarrior* pilot);
    /// <summary>The order's parameters as ABL values: its time, and a list of longs by code.</summary>
    /// <returns>The order code.</returns>
    int32_t GetParamData(float* timeStamp, int32_t* paramList);
    /// <summary>Packs the order into <see cref="Data"/>.</summary>
    int32_t Pack(MCMoverGroup* group, MCMover* point);
    /// <summary>Rebuilds the order from <see cref="Data"/>.</summary>
    int32_t Unpack();
    void SetGroupFlag(int32_t localMoverId, int set);
    /// <summary>The movers of a multiplayer commander flagged in <see cref="GroupFlags"/>, and the point mover.</summary>
    int32_t GetGroup(int32_t commanderId, MCMover** moverList, MCMover** point, int32_t sortType);
    /// <summary>Starts the order on <paramref name="pilot"/>; <paramref name="message"/> receives the radio message.</summary>
    int32_t Execute(MCMechWarrior* pilot, int32_t& message);
    /// <summary>Advances the order's stage; returns its status.</summary>
    int32_t Status(MCMechWarrior* pilot);
    /// <summary>Does nothing.</summary>
    void Destroy();

    int32_t Id = 0;
    /// <summary>Scenario time the order was given (and when it is next due).</summary>
    float Time = 0;
    /// <summary>Scenario time to wait for before executing, -1 for none.</summary>
    float DelayedTime = 0;
    float LastTime = 0;
    /// <summary>Nonzero for an order given to a group.</summary>
    int UnitOrder = 0;
    MCOrderOriginType Origin{};
    MCTacticalOrderCode Code{};
    MCTacOrderMoveParams MoveParams{};
    MCTacOrderAttackParams AttackParams;
    MCGameObject* Target = nullptr;
    /// <summary>The target's object class (copied from target + 4).</summary>
    int32_t TargetObjectClass = 0;
    int32_t SelectionIndex = 0;
    /// <summary>Progress of the order (1 at start, 0xff when done).</summary>
    uint8_t Stage = 0;
    /// <summary>Local id of the point mover in a group order, 0xf for none.</summary>
    char PointLocalMoverId = 0;
    /// <summary>Bit per local mover id of the commander's movers in the order.</summary>
    uint32_t GroupFlags = 0;
    /// <summary>The packed order (<see cref="Pack"/>).</summary>
    uint32_t Data[2]{};
};

/// <summary>Repairs and reloads a mover from a refit vehicle's points.</summary>
int32_t DoRefit(MCMover* mover, float refitPoints, float& pointsUsed, int ammoOnly);
