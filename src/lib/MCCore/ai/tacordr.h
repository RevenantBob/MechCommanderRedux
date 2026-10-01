#pragma once

#include "lib/cvmath.h"

class GameObject;
class Mover;
class MoverGroup;
class MechWarrior;
struct LocationNode;

/// <summary>Waypoints a tactical order's way path holds.</summary>
inline constexpr int32_t MAX_WAYPTS = 15;

/// <summary>Who gave a tactical order (packed in 2 bits).</summary>
enum OrderOriginType
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
enum TacticalOrderCode
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
typedef struct _WayPath
{
    int32_t numPoints; // +0x0
    int32_t curPoint;  // +0x4
    /// <summary>x, y, z of each waypoint.</summary>
    float points[MAX_WAYPTS * 3]; // +0x8
    /// <summary>Per waypoint, 1 to run to it.</summary>
    uint8_t mode[MAX_WAYPTS]; // +0xbc
} WayPath;

/// <summary>The movement part of a tactical order.</summary>
/// <remarks>0xdc bytes (copied as one block by TacticalOrder's copy). The original name isn't known
/// (MC2: TacOrderMoveParams); the flag names are MC2's, their use in MCX noted.</remarks>
typedef struct _TacOrderMoveParams
{
    WayPath wayPath; // +0x0
    /// <summary>MOVETO_OBJECT: passed as move flag 4 (default 1).</summary>
    int32_t faceObject; // +0xcc
    /// <summary>MOVETO_POINT: move flag 2.</summary>
    int32_t wait; // +0xd0
    /// <summary>1 adds move flag 8.</summary>
    int32_t mode; // +0xd4
    /// <summary>MOVETO_POINT: move flag 0x40.</summary>
    int32_t escapeTile; // +0xd8
} TacOrderMoveParams;

/// <summary>The attack part of a tactical order.</summary>
/// <remarks>Original: <c>struct _TacOrderAttackParams</c> (copy constructor at 0x006a8130). 0x24 bytes.</remarks>
typedef struct _TacOrderAttackParams
{
    /// <summary>Attack type (default 1, packed in 2 bits).</summary>
    int32_t type; // +0x0
    /// <summary>Attack method (2 = ramming, see TacticalOrder::getRamTarget).</summary>
    int32_t method; // +0x4
    /// <summary>Fire range: a Mover::getFireRange selector, -4..2 (packed + 4 in 3 bits).</summary>
    int32_t range; // +0x8
    /// <summary>Aimed location, -1 for none (packed + 2 in 4 bits; Mover::sortWeapons aims with it).</summary>
    int32_t aimLocation;   // +0xc
    int32_t pursue;        // +0x10
    int32_t obliterate;    // +0x14
    vector_3d targetPoint; // +0x18
} TacOrderAttackParams;

/// <summary>
/// An order to one pilot or a group: its code, parameters, target and progress. Orders are queued on pilots and
/// packed into two 32-bit words to be sent over the network.
/// </summary>
/// <remarks>Original source: <c>ai\tacordr.cpp</c> (operator= inline in <c>ai\tacordr.h</c>). 0x138 bytes. No
/// constructor: new orders are set up with <see cref="init()"/>. The compiler-generated copy constructor is emitted
/// at 0x00602ea0 (network\multplyr.cpp).</remarks>
class TacticalOrder
{
public:
    /// <remarks>MCX.EXE @ 0x006c54a0</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x006c54c0</remarks>
    static void operator delete(void* ptr);

    /// <summary>Copies an order and destroys the (by-value) source.</summary>
    /// <remarks>MCX.EXE @ 0x006a6b50 (inline in the original's tacordr.h)</remarks>
    void operator=(TacticalOrder copy);

    /// <remarks>MCX.EXE @ 0x006c54e0</remarks>
    void init();
    /// <remarks>MCX.EXE @ 0x006c5510</remarks>
    void init(OrderOriginType _origin, TacticalOrderCode _code, int _unitOrder = 0);
    /// <summary>Copies a location list into the way path (at most MAX_WAYPTS).</summary>
    /// <remarks>MCX.EXE @ 0x006c55e0</remarks>
    void initWayPath(LocationNode* path);
    /// <remarks>MCX.EXE @ 0x006c5650</remarks>
    vector_3d getWayPoint(int32_t index);
    /// <remarks>MCX.EXE @ 0x006c5690</remarks>
    void setWayPoint(int32_t index, vector_3d wayPoint);
    /// <remarks>MCX.EXE @ 0x006c56c0</remarks>
    void addWayPoint(vector_3d wayPoint, int32_t run);
    /// <summary>The target of a ramming attack, else null.</summary>
    /// <remarks>MCX.EXE @ 0x006c5720</remarks>
    GameObject* getRamTarget();
    /// <summary>The target of a JUMPTO_POINT order, else null.</summary>
    /// <remarks>MCX.EXE @ 0x006c5740</remarks>
    GameObject* getJumpTarget();
    /// <remarks>MCX.EXE @ 0x006c5750</remarks>
    int isGroupOrder();
    /// <remarks>MCX.EXE @ 0x006c5760</remarks>
    int isCombatOrder();
    /// <remarks>MCX.EXE @ 0x006c5780</remarks>
    int isMoveOrder();
    /// <remarks>MCX.EXE @ 0x006c57a0</remarks>
    int isWayPathOrder();
    /// <remarks>MCX.EXE @ 0x006c57c0</remarks>
    int isJumpOrder();
    /// <summary>Takes the pilot's next order id (1-255).</summary>
    /// <remarks>MCX.EXE @ 0x006c57e0</remarks>
    void setId(MechWarrior* pilot);
    /// <summary>The order's parameters as ABL values: its time, and a list of longs by code.</summary>
    /// <returns>The order code.</returns>
    /// <remarks>MCX.EXE @ 0x006c5830</remarks>
    int32_t getParamData(float* timeStamp, int32_t* paramList);
    /// <summary>Packs the order into <see cref="data"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006c59a0</remarks>
    int32_t pack(MoverGroup* group, Mover* point);
    /// <summary>Rebuilds the order from <see cref="data"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006c5c50</remarks>
    int32_t unpack();
    /// <remarks>MCX.EXE @ 0x006c5f60</remarks>
    void setGroupFlag(int32_t localMoverId, int set);
    /// <summary>The movers of a multiplayer commander flagged in <see cref="groupFlags"/>, and the point mover.</summary>
    /// <remarks>MCX.EXE @ 0x006c5fa0</remarks>
    int32_t getGroup(int32_t commanderId, Mover** moverList, Mover** point, int32_t sortType);
    /// <summary>Starts the order on <paramref name="pilot"/>; <paramref name="message"/> receives the radio message.</summary>
    /// <remarks>MCX.EXE @ 0x006c6030</remarks>
    int32_t execute(MechWarrior* pilot, int32_t& message);
    /// <summary>Advances the order's stage; returns its status.</summary>
    /// <remarks>MCX.EXE @ 0x006c6b80</remarks>
    int32_t status(MechWarrior* pilot);
    /// <summary>Does nothing.</summary>
    /// <remarks>MCX.EXE @ 0x006c7a30</remarks>
    void destroy();

    int32_t id; // +0x0
    /// <summary>Scenario time the order was given (and when it is next due).</summary>
    float time; // +0x4
    /// <summary>Scenario time to wait for before executing, -1 for none.</summary>
    float delayedTime; // +0x8
    float lastTime;    // +0xc
    /// <summary>Nonzero for an order given to a group.</summary>
    int unitOrder;                     // +0x10
    OrderOriginType origin;            // +0x14
    TacticalOrderCode code;            // +0x18
    TacOrderMoveParams moveParams;     // +0x1c
    TacOrderAttackParams attackParams; // +0xf8
    GameObject* target;                // +0x11c
    /// <summary>The target's object class (copied from target + 4).</summary>
    int32_t targetObjectClass; // +0x120
    int32_t selectionIndex;    // +0x124
    /// <summary>Progress of the order (1 at start, 0xff when done).</summary>
    uint8_t stage; // +0x128
    /// <summary>Local id of the point mover in a group order, 0xf for none.</summary>
    char pointLocalMoverId; // +0x129
    /// <summary>Bit per local mover id of the commander's movers in the order.</summary>
    uint32_t groupFlags; // +0x12c
    /// <summary>The packed order (<see cref="pack"/>).</summary>
    uint32_t data[2]; // +0x130
};

/// <summary>Repairs and reloads a mover from a refit vehicle's points.</summary>
/// <remarks>MCX.EXE @ 0x006c7a40</remarks>
int32_t DoRefit(Mover* mover, float refitPoints, float& pointsUsed, int ammoOnly);
