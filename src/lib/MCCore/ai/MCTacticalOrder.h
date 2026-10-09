#pragma once

#include "lib/MCVector3D.h"

class MCGameObject;
class MCMover;
class MCMechWarrior;

/// <summary>Waypoints a tactical order's way path holds (P3-obj-2 decides it with the pilot's move orders).</summary>
inline constexpr int32_t MaxWayPoints = 15;

/// <summary>One point of a way path, as <see cref="MCTacticalOrder::InitWayPath"/> takes it.</summary>
/// <remarks>Original: <c>LocationNode</c>, a singly linked list node.</remarks>
struct MCWayPathPoint
{
    MCVector3D Location;
    /// <summary>Whether to run to this point.</summary>
    bool Run = false;
};

/// <summary>Who gave a tactical order (packed in 2 bits).</summary>
enum class MCOrderOrigin : int32_t
{
    Player = 0,
    Commander = 1,
    Self = 2,
};

/// <summary>What a tactical order does (packed in 5 bits; 0x1f encodes a Guard order on a point).</summary>
/// <remarks>
/// Values from TacticalOrder::execute / status / is*Order; names follow MechCommander 2's enum, whose values match
/// every case MCX.EXE uses. HoldFire executes MechWarrior::orderWait.
/// </remarks>
enum class MCTacticalOrderCode : int32_t
{
    None = 0,
    Wait = 1,
    MoveToPoint = 2,
    MoveToObject = 3,
    JumpToPoint = 4,
    JumpToObject = 5,
    TraversePath = 6,
    PatrolPath = 7,
    Escort = 8,
    Follow = 9,
    Guard = 10,
    Stop = 11,
    PowerUp = 12,
    PowerDown = 13,
    WayPointsDone = 14,
    Eject = 15,
    AttackObject = 16,
    AttackPoint = 17,
    HoldFire = 18,
    Withdraw = 19,
    Scramble = 20,
    Capture = 21,
    Refit = 22,
    GetFixed = 23,
    LoadIntoCarrier = 24,
    DeployElementals = 25,
    Count
};

/// <summary>A list of waypoints.</summary>
/// <remarks>Original: <c>struct _WayPath</c> (MechWarrior::setMoveWayPath).</remarks>
struct MCWayPath
{
    int32_t NumPoints = 0;
    int32_t CurPoint = 0;
    /// <summary>x, y, z of each waypoint.</summary>
    float Points[MaxWayPoints * 3]{};
    /// <summary>Per waypoint, 1 to run to it.</summary>
    uint8_t Mode[MaxWayPoints]{};
};

/// <summary>The movement part of a tactical order.</summary>
/// <remarks>The original name isn't known (MC2: TacOrderMoveParams); the flag names are MC2's, their use in MCX
/// noted.</remarks>
struct MCTacOrderMoveParams
{
    MCWayPath WayPath{};
    /// <summary>MoveToObject: passed as move flag 4 (default 1).</summary>
    int32_t FaceObject = 0;
    /// <summary>MoveToPoint: move flag 2.</summary>
    int32_t Wait = 0;
    /// <summary>1 adds move flag 8.</summary>
    int32_t Mode = 0;
    /// <summary>MoveToPoint: move flag 0x40.</summary>
    int32_t EscapeTile = 0;
};

/// <summary>The attack part of a tactical order.</summary>
/// <remarks>Original: <c>struct _TacOrderAttackParams</c>.</remarks>
struct MCTacOrderAttackParams
{
    /// <summary>Attack type (default 1, packed in 2 bits).</summary>
    int32_t Type = 0;
    /// <summary>Attack method (2 = ramming, see <see cref="MCTacticalOrder::GetRamTarget"/>).</summary>
    int32_t Method = 0;
    /// <summary>Fire range: a Mover::getFireRange selector, -4..2 (packed + 4 in 3 bits).</summary>
    int32_t Range = 0;
    /// <summary>Aimed location, -1 for none (packed + 2 in 4 bits; Mover::sortWeapons aims with it).</summary>
    int32_t AimLocation = 0;
    int32_t Pursue = 0;
    int32_t Obliterate = 0;
    MCVector3D TargetPoint;
};

/// <summary>
/// An order to one pilot or a group: its code, parameters, target and progress. Orders are queued on pilots and
/// packed into two 32-bit words to be sent over the network.
/// </summary>
/// <remarks>Original source: <c>ai\tacordr.cpp</c>. A new order is all zero; <see cref="Reset()"/> and
/// <see cref="Reset(MCOrderOrigin, MCTacticalOrderCode, int)"/> set one up (init in the original).</remarks>
class MCTacticalOrder
{
public:
    /// <summary>The "no time" value of <see cref="DelayedTime"/> and <see cref="LastTime"/>.</summary>
    static constexpr float NoTime = -1.0f;

    /// <summary>Makes the order an empty one of its pilot's own (code None, no time, no target).</summary>
    void Reset();
    /// <summary>Sets up a new order: given now, with every parameter at its default.</summary>
    void Reset(MCOrderOrigin origin, MCTacticalOrderCode code, int unitOrder = 0);
    /// <summary>Copies the points into the way path (Fatal past <see cref="MaxWayPoints"/>).</summary>
    void InitWayPath(std::span<const MCWayPathPoint> path);
    MCVector3D GetWayPoint(int32_t index) const;
    void SetWayPoint(int32_t index, MCVector3D wayPoint);
    void AddWayPoint(MCVector3D wayPoint, int32_t run);
    /// <summary>The target of a ramming attack, else null.</summary>
    MCGameObject* GetRamTarget() const;
    /// <summary>The target of a JumpToPoint order, else null.</summary>
    MCGameObject* GetJumpTarget() const;
    bool IsGroupOrder() const { return UnitOrder != 0; }
    bool IsCombatOrder() const;
    bool IsMoveOrder() const;
    bool IsWayPathOrder() const;
    bool IsJumpOrder() const;
    /// <summary>Takes the pilot's next order id (1-255).</summary>
    void SetId(MCMechWarrior* pilot);
    /// <summary>The order's parameters as ABL values: its time, and a list of longs by code.</summary>
    /// <returns>The order code.</returns>
    int32_t GetParamData(float* timeStamp, int32_t* paramList) const;
    /// <summary>Packs the order into <see cref="Data"/>.</summary>
    void Pack();
    /// <summary>Rebuilds the order from <see cref="Data"/>.</summary>
    void Unpack();
    void SetGroupFlag(int32_t localMoverId, bool set);
    /// <summary>
    /// The movers of a multiplayer commander flagged in <see cref="GroupFlags"/> (into <paramref name="moverList"/>),
    /// and the point mover.
    /// </summary>
    /// <returns>The number of movers.</returns>
    int32_t GetGroup(int32_t commanderId, MCMover** moverList, MCMover** point) const;
    /// <summary>Starts the order on <paramref name="pilot"/>; <paramref name="message"/> receives the radio message.</summary>
    int32_t Execute(MCMechWarrior* pilot, int32_t& message);
    /// <summary>Advances the order's stage.</summary>
    /// <returns>Whether the order is done.</returns>
    bool Status(MCMechWarrior* pilot);

    int32_t Id = 0;
    /// <summary>Scenario time the order was given (and when it is next due).</summary>
    float Time = 0;
    /// <summary>Scenario time to wait for before executing, <see cref="NoTime"/> for none.</summary>
    float DelayedTime = 0;
    float LastTime = 0;
    /// <summary>Nonzero for an order given to a group.</summary>
    int UnitOrder = 0;
    MCOrderOrigin Origin{};
    MCTacticalOrderCode Code{};
    MCTacOrderMoveParams MoveParams{};
    MCTacOrderAttackParams AttackParams;
    MCGameObject* Target = nullptr;
    /// <summary>The target's object class.</summary>
    int32_t TargetObjectClass = 0;
    int32_t SelectionIndex = 0;
    /// <summary>Progress of the order (1 at start, <see cref="StageDone"/> when done).</summary>
    uint8_t Stage = 0;
    /// <summary>Local id of the point mover in a group order, 0xf for none.</summary>
    int8_t PointLocalMoverId = 0;
    /// <summary>Bit per local mover id of the commander's movers in the order.</summary>
    uint32_t GroupFlags = 0;
    /// <summary>The packed order (<see cref="Pack"/>).</summary>
    uint32_t Data[2]{};

    /// <summary>The <see cref="Stage"/> of an order that is over.</summary>
    static constexpr uint8_t StageDone = 0xff;

private:
    /// <summary>
    /// The shared start of the delayed orders: false to wait (the delay isn't over), else clears the delay. The power
    /// orders test "later than -1" (<paramref name="laterThanNone"/>) where the moves test "not -1".
    /// </summary>
    bool DelayOver(bool laterThanNone);
    /// <summary>The order's first waypoint.</summary>
    MCVector3D FirstWayPoint() const { return GetWayPoint(0); }
    /// <summary>Status of a Capture order: the prize taken once the vehicle is within 30 m.</summary>
    bool CaptureStatus(MCMechWarrior* pilot);
    /// <summary>Status of a Refit order: drive up, power the mech down, repair in rounds, power it up.</summary>
    bool RefitStatus(MCMechWarrior* pilot);
    /// <summary>Status of a GetFixed order: drive into the bay, power down, repair in rounds, drive out.</summary>
    bool GetFixedStatus(MCMechWarrior* pilot);
    /// <summary>Status of a LoadIntoCarrier order: stop near the carrier, walk to it, board it.</summary>
    bool LoadIntoCarrierStatus(MCMechWarrior* pilot);
    /// <summary>Status of a DeployElementals order: up to five elementals set down every five seconds.</summary>
    bool DeployElementalsStatus(MCMechWarrior* pilot);
};
