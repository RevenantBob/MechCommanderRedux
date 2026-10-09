#include "stdafx.h"
#include "object/MCPilotOrders.h"
#include "main/main.h"

MCMoveOrders::MCMoveOrders()
{
    for (std::unique_ptr<MCMovePath>& path : Path)
    {
        path = std::make_unique<MCMovePath>();
        path->GlobalStep = -1;
    }

    Reset();
}

auto MCMoveOrders::Reset() -> void
{
    Time = ScenarioTime;
    Origin = 1;
    SpeedType = 3;
    GoalObjectPosition = MCVector3D(0.0f, 0.0f, 0.0f);
    GoalLocation = MCVector3D(-999999.0f, -999999.0f, -999999.0f);
    OriginalGlobalGoal[0] = MCVector3D(-999999.0f, -999999.0f, -999999.0f);
    OriginalGlobalGoal[1] = MCVector3D(-999999.0f, -999999.0f, -999999.0f);
    SpeedVelocity = 0.0f;
    GlobalGoalLocation = MCVector3D(-666666.0f, -666666.0f, -666666.0f);
    SpeedState = 2;
    SpeedThrottle = 100;
    GoalType = -1;
    GoalObject = nullptr;
    NextUpdate = 0.0f;
    ScriptGoal = 0;
    WayPath.clear();
    CurWayPt = 0;
    CurWayDir = 0;
    PathType = 0;
    NumGlobalSteps = 0;
    CurGlobalStep = 0;
    TimeOfLastStep = -1.0f;
    MoveState = MCMoveState::Forward;
    MoveStateGoal = MCMoveState::Forward;
    MoveStateGoalChanged = 0;
    YieldTime = -1.0f;
    YieldState = 0;
    WaitForPointTime = -1.0f;
    Run = 0;
}

auto MCAttackOrders::Reset() -> void
{
    Time = ScenarioTime;
    Origin = 1;
    Type = 0;
    Target = nullptr;
    TargetPoint = MCVector3D(0.0f, 0.0f, 0.0f);
    AimLocation = -1;
    Pursue = 0;
    TargetTime = 0.0f;
}
