#pragma once

#include "ai/MCMoveSystem.h"
#include "ai/MCTacticalOrder.h"

class MCGameObject;

/// <summary>How a mover moves along its path (<see cref="MCMoveOrders"/>; the names are the port's).</summary>
enum class MCMoveState : int32_t
{
    /// <summary>A path follower's answer: keep the move state as it is.</summary>
    NoChange = -1,
    /// <summary>Before the move orders are first reset.</summary>
    None = 0,
    /// <summary>Driving forward along the path.</summary>
    Forward = 1,
    /// <summary>Backing along the path.</summary>
    Reverse = 2,
    /// <summary>Turning in place to face along the path.</summary>
    PivotForward = 3,
    /// <summary>Turning in place to back along the path.</summary>
    PivotReverse = 4,
    /// <summary>Turning in place toward the target (holding position while attacking).</summary>
    PivotTarget = 5,
};

/// <summary>A pilot's movement orders: goal, speed, way path, global path and the two move paths.</summary>
/// <remarks>Original source: <c>object\warrior.cpp</c>. Names follow MechCommander 2's MoveOrders where the layout
/// matches.</remarks>
struct MCMoveOrders
{
    /// <summary>Two move paths, empty.</summary>
    MCMoveOrders();

    /// <summary>
    /// Orders given now by the commander: speed type 3 (state 2, throttle 100), no goal, no way points or global
    /// path, moving forward, not yielding. The move paths are kept, as they are.
    /// </summary>
    void Reset();

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
    /// <summary>Set by ABL setmovegoal (hasmovegoal / hasmovepath test it); only a reset clears it.</summary>
    int32_t ScriptGoal = 0;
    /// <summary>The way points (setMoveWayPath).</summary>
    std::vector<MCVector3D> WayPath;
    /// <summary>The way point being walked to.</summary>
    int8_t CurWayPt = 0;
    /// <summary>1 to patrol (walk the way path back and forth).</summary>
    int8_t CurWayDir = 0;
    /// <summary>0 none, 1 a single (local) path, 2 a global path.</summary>
    int8_t PathType = 0;
    /// <summary>The goal as first asked for (-999999 to start with).</summary>
    MCVector3D OriginalGlobalGoal[2];
    /// <summary>The goal the path actually reaches (-666666 to start with).</summary>
    MCVector3D GlobalGoalLocation;
    /// <summary>The global path, area by area.</summary>
    std::array<MCGlobalPathStep, MCGlobalMap::MaxPathSteps> GlobalPath{};
    /// <summary>Steps of the global path.</summary>
    int8_t NumGlobalSteps = 0;
    /// <summary>The global step being walked.</summary>
    int8_t CurGlobalStep = 0;
    /// <summary>The move paths: the one walked, and the next leg.</summary>
    std::array<std::unique_ptr<MCMovePath>, 2> Path;
    /// <summary>When the move was ordered; the move times out MoveTimeOut after it (-1 for none).</summary>
    float TimeOfLastStep = 0;
    /// <summary>Forward to start with; the pivots are the combat moves calcMovePath resets.</summary>
    MCMoveState MoveState = MCMoveState::None;
    /// <summary>Forward to start with; PivotTarget to hold position while attacking.</summary>
    MCMoveState MoveStateGoal = MCMoveState::None;
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
/// <remarks>Original source: <c>object\warrior.cpp</c>. Names follow MechCommander 2's AttackOrders.</remarks>
struct MCAttackOrders
{
    /// <summary>Orders given now by the commander: no target, aim location -1.</summary>
    void Reset();

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
