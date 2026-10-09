#include "stdafx.h"
#include "object/MCElemental.h"
#include "object/MCElementalType.h"
#include "ai/MCTacticalOrder.h"
#include "appear/MCAppearanceType.h"
#include "iface/MCTacticalInterface.h"
#include "lib/MCDice.h"
#include "main/main.h"
#include "mission/MCScenario.h"
#include "network/multplyr.h"
#include "object/MCAIControl.h"
#include "object/MCArtillery.h"
#include "object/MCArtilleryType.h"
#include "object/MCArtilleryChunk.h"
#include "object/MCCameraDrone.h"
#include "object/MCCameraDroneType.h"
#include "object/MCBullet.h"
#include "object/MCBulletType.h"
#include "object/MCElementalControlData.h"
#include "object/MCElementalDynamics.h"
#include "object/MCMoverGroup.h"
#include "object/MCLaser.h"
#include "object/MCLaserType.h"
#include "object/MCMechGameSystem.h"
#include "object/MCProjectileLaser.h"
#include "object/MCProjectileLaserType.h"
#include "object/MCMechWarrior.h"
#include "sound/MCSoundSystem.h"
#include "sprite/MCElementalActor.h"
#include "terrain/MCTerrain.h"
#include "object/MCObjectType.h"
#include "object/MCWeaponShotInfo.h"

namespace
{
    /// <summary>A turn of <paramref name="turn"/> degrees as a rotate request, no faster than
    /// <paramref name="maxTurn"/>.</summary>
    int8_t RotateRequest(float turn, float maxTurn)
    {
        return static_cast<int8_t>(static_cast<int32_t>(turn / maxTurn * 64.0f));
    }
}

auto MCElemental::GetThrottle() -> int32_t
{
    return static_cast<MCElementalControlData*>(Control->ControlData.get())->Throttle;
}

auto MCElemental::IsJumping(MCVector3D* jumpGoalOut) -> int
{
    if (jumpGoalOut != nullptr)
    {
        *jumpGoalOut = JumpGoal;
    }

    return InJump;
}

auto MCElemental::GetJumpRange(int32_t* numOffsets, int32_t* jumpCost) -> float
{
    if (ElementalCanJump == 0)
    {
        return 0.0f;
    }

    if (numOffsets != nullptr)
    {
        *numOffsets = 32;
    }

    if (jumpCost != nullptr)
    {
        *jumpCost = 20;
    }

    return MetersPerWorldUnit * MCTerrain::MetersPerVertex;
}

auto MCElemental::UpdateJump() -> int
{
    if (IsJumping(nullptr) == 0)
    {
        return 0;
    }

    auto* controlData = static_cast<MCElementalControlData*>(Control->ControlData.get());
    auto* actor = static_cast<MCElementalActor*>(Appearance.get());

    if (actor->Jumping == 0)
    {
        if (actor->JumpSetup == 0)
        {
            // Landed: on to the step after the jump.
            InJump = 0;
            MCMovePath* path = Pilot->GetMovePath();
            path->NumSteps = path->NumStepsWhenNotPaused;
            path->CurStep++;
            LastValidPosition = Position;
            return 1;
        }

        controlData->Jump = 1;
        controlData->JumpDistance = static_cast<float>(DistanceFrom(JumpGoal));
        controlData->Throttle = 0;
        return 1;
    }

    // In the air: turns toward the landing point.
    auto* dynType = static_cast<MCElementalDynamicsType*>(static_cast<MCElementalType*>(ObjType)->DynamicsType.get());
    const float facing = RelFacingTo(JumpGoal, -1);
    double maxTurn = static_cast<double>(dynType->MaxElementalYawRate);

    if (maxTurn > 180.0)
    {
        maxTurn = 180.0f;
    }

    if (facing >= -5.0 && facing <= 5.0)
    {
        return 1;
    }

    double turn = -(static_cast<double>(facing) / FrameLength);

    if (turn > maxTurn)
    {
        turn = maxTurn;
    }
    else
    {
        const auto minTurn = static_cast<float>(-maxTurn);

        if (turn < minTurn)
        {
            turn = minTurn;
        }
    }

    controlData->Rotate = static_cast<int8_t>(static_cast<int32_t>(turn / maxTurn * 64.0f));
    return 1;
}

auto MCElemental::PivotTo() -> int
{
    MCMechWarrior* warrior = Pilot;
    MCMovePath* path = warrior->GetMovePath();
    const MCMoveState moveState = warrior->MoveOrders.MoveState;
    const MCMoveState moveStateGoal = warrior->MoveOrders.MoveStateGoal;
    auto* controlData = static_cast<MCElementalControlData*>(Control->ControlData.get());
    auto* dynType = static_cast<MCElementalDynamicsType*>(static_cast<MCElementalType*>(ObjType)->DynamicsType.get());

    // Starts a turn of <turn> degrees, no faster than the yaw rate allows this frame.
    const auto pivot = [&](float turn) -> int
    {
        float maxTurn = static_cast<float>(dynType->MaxElementalYawRate) * FrameLength;

        if (maxTurn > 180.0)
        {
            maxTurn = 180.0f;
        }

        if (static_cast<float>(std::abs(static_cast<int32_t>(turn))) > maxTurn)
        {
            turn = turn > 0.0f ? maxTurn : -maxTurn;
        }

        controlData->Rotate = RotateRequest(turn, maxTurn);
        return 1;
    };

    const auto hasNextStep = [&]()
    { return path->NumStepsWhenNotPaused > 0 && path->CurStep < path->NumStepsWhenNotPaused; };

    if (moveState == MCMoveState::PivotForward)
    {
        if (moveStateGoal != MCMoveState::PivotForward && moveStateGoal != MCMoveState::Forward)
        {
            warrior->MoveOrders.MoveState = MCMoveState::Forward;
        }
        else if (!hasNextStep())
        {
            warrior->MoveOrders.MoveStateGoal = MCMoveState::Forward;
        }
        else
        {
            const MCVector3D destination = path->StepList[path->CurStep].Destination;
            Appearance->SetGestureGoal(0);
            controlData->Throttle = 0;
            const float facing = RelFacingTo(destination, -1);

            if (facing < -15.0 || facing > 15.0)
            {
                return pivot(-facing);
            }

            Pilot->MoveOrders.MoveState = MCMoveState::Forward;
        }
    }
    else if (moveState == MCMoveState::PivotReverse)
    {
        if (moveStateGoal != MCMoveState::PivotReverse && moveStateGoal != MCMoveState::Reverse)
        {
            warrior->MoveOrders.MoveState = MCMoveState::Forward;
        }
        else if (!hasNextStep())
        {
            warrior->MoveOrders.MoveStateGoal = MCMoveState::Forward;
        }
        else
        {
            const MCVector3D destination = path->StepList[path->CurStep].Destination;
            Appearance->SetGestureGoal(0);
            // Original behaviour (OB-004): it clears the second byte of the jump request, not the throttle.
            controlData->Jump &= ~0xff00;
            const float facing = RelFacingTo(destination, -1);

            if (facing > -165.0 && facing < 165.0)
            {
                return pivot(-(facing < 0.0f ? facing + 180.0f : facing - 180.0f));
            }

            if (moveStateGoal == MCMoveState::Reverse)
            {
                Pilot->MoveOrders.MoveState = MCMoveState::Reverse;
            }
            else
            {
                Pilot->MoveOrders.MoveStateGoal = MCMoveState::Forward;
            }
        }
    }
    else if (moveState == MCMoveState::PivotTarget)
    {
        if (moveStateGoal != MCMoveState::PivotTarget)
        {
            warrior->MoveOrders.MoveState = MCMoveState::Forward;
        }
        else
        {
            MCVector3D targetPosition;
            MCGameObject* target = warrior->GetLastTarget();

            if (target != nullptr)
            {
                targetPosition = target->GetPosition();
            }
            else if (warrior->CurTacOrder.Code == MCTacticalOrderCode::AttackPoint)
            {
                targetPosition = warrior->AttackOrders.TargetPoint;
            }
            else
            {
                warrior->MoveOrders.MoveStateGoal = MCMoveState::Forward;
                warrior->GetMovePath()->NumSteps = warrior->GetMovePath()->NumStepsWhenNotPaused;
                return 0;
            }

            Appearance->SetGestureGoal(0);
            controlData->Throttle = 0;
            const float facing = RelFacingTo(targetPosition, -1);
            const float fireArc = GetFireArc();

            if (facing < -fireArc || fireArc < facing)
            {
                return pivot(-facing);
            }

            Pilot->MoveOrders.MoveStateGoal = MCMoveState::Forward;
        }
    }
    else if (moveStateGoal == MCMoveState::PivotTarget || moveStateGoal == MCMoveState::PivotForward ||
             moveStateGoal == MCMoveState::PivotReverse)
    {
        warrior->MoveOrders.MoveState = moveStateGoal;
    }

    warrior = Pilot;

    if (warrior->MoveOrders.YieldTime <= -1.0)
    {
        warrior->GetMovePath()->NumSteps = warrior->GetMovePath()->NumStepsWhenNotPaused;
    }

    return 0;
}

auto MCElemental::UpdateMoveStateGoal() -> void
{
    MCMechWarrior* warrior = Pilot;
    MCMovePath* path = warrior->GetMovePath();

    if (path->NumSteps > 0 || (warrior->MoveOrders.MoveStateGoal != MCMoveState::PivotTarget &&
                               warrior->MoveOrders.MoveStateGoal != MCMoveState::PivotForward))
    {
        warrior->MoveOrders.MoveStateGoal = MCMoveState::Forward;
    }
}

auto MCElemental::UpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                 int32_t& newGestureStateGoal, MCMoveState& newMoveState, int32_t& minThrottle,
                                 int32_t& maxThrottle) -> int
{
    MCMechWarrior* warrior = Pilot;
    auto* controlData = static_cast<MCElementalControlData*>(Control->ControlData.get());
    auto* dynType = static_cast<MCElementalDynamicsType*>(static_cast<MCElementalType*>(ObjType)->DynamicsType.get());
    MCMovePath* path = warrior->GetMovePath();
    newThrottleSetting = controlData->Throttle;
    newRotatePerSec = 0.0f;

    if (path->NumSteps < 1)
    {
        newGestureStateGoal = 0;
        return 0;
    }

    // The end of the path: done, unless more global steps are to come.
    const auto reachedEnd = [&]() -> int
    {
        int finished = 1;

        if (warrior->MoveOrders.PathType == 2 &&
            warrior->MoveOrders.Path[0]->GlobalStep < warrior->MoveOrders.NumGlobalSteps - 1)
        {
            finished = 0;
        }

        if (warrior->MoveOrders.Path[0] != nullptr)
        {
            warrior->MoveOrders.Path[0]->Clear();
        }

        return finished;
    };

    // A step whose direction is past 7 is a jump.
    const auto isJumpStep = [&](int32_t step) { return static_cast<int8_t>(path->StepList[step].Direction) > 7; };

    int32_t step = path->CurStep;

    if (step == path->NumSteps)
    {
        return reachedEnd();
    }

    newGestureStateGoal = 1;
    MCVector3D destination = path->StepList[step].Destination;
    LastValidPosition = destination;
    const auto distance = static_cast<float>(DistanceFrom(destination));
    // (The original also measures the distance to the path object itself, and drops it.)
    const float margin = step == path->NumSteps - 1 ? MoveMarginOfError[1] : MoveMarginOfError[0];

    if (distance < margin)
    {
        // Reached the step: on to the next.
        step++;
        Pilot->MoveOrders.TimeOfLastStep = ScenarioTime;
        path->CurStep = step;

        if (path->NumSteps <= step)
        {
            return reachedEnd();
        }

        if (isJumpStep(step))
        {
            newGestureStateGoal = 2;
            return 0;
        }

        destination = path->StepList[step].Destination;
    }
    else if (isJumpStep(step))
    {
        newGestureStateGoal = 2;
        return 0;
    }

    const float facing = RelFacingTo(destination, -1);
    warrior = Pilot;
    const MCMoveState moveState = warrior->MoveOrders.MoveState;
    const MCMoveState moveStateGoal = warrior->MoveOrders.MoveStateGoal;
    // Stops on the path to change the move state.
    const auto changeState = [&](MCMoveState state)
    {
        warrior->GetMovePath()->NumSteps = 0;
        newMoveState = state;
        return 0;
    };

    // Turns toward the step, no faster than the yaw rate allows this frame.
    const auto steer = [&]()
    {
        double maxTurn = static_cast<double>(dynType->MaxElementalYawRate) * FrameLength;

        if (maxTurn > 180.0)
        {
            maxTurn = 180.0f;
        }

        if (std::fabs(newRotatePerSec) > maxTurn)
        {
            newRotatePerSec = static_cast<float>(newRotatePerSec <= 0.0 ? -maxTurn : maxTurn);
        }

        newRotate = static_cast<char>(static_cast<int8_t>(static_cast<int32_t>(newRotatePerSec / maxTurn * 64.0f)));
        return 0;
    };

    if (moveState == MCMoveState::Forward)
    {
        switch (moveStateGoal)
        {
            case MCMoveState::Forward:
            {
                newGestureStateGoal = 1;
                newThrottleSetting = 100;

                if (facing >= -5.0 && facing <= 5.0)
                {
                    return 0;
                }

                newRotatePerSec = -facing;
                return steer();
            }
            case MCMoveState::PivotForward:
                return changeState(MCMoveState::PivotForward);
            case MCMoveState::Reverse:
            case MCMoveState::PivotReverse:
                return changeState(MCMoveState::PivotReverse);
            default:
                return changeState(MCMoveState::Forward);
        }
    }

    if (moveState == MCMoveState::Reverse)
    {
        switch (moveStateGoal)
        {
            case MCMoveState::Reverse:
            {
                newGestureStateGoal = 1;
                newThrottleSetting = -100;
                newRotatePerSec = facing >= 0.0f ? -(facing - 180.0f) : -(facing + 180.0f);
                return steer();
            }
            case MCMoveState::Forward:
            case MCMoveState::PivotForward:
                return changeState(MCMoveState::PivotForward);
            case MCMoveState::PivotReverse:
                return changeState(MCMoveState::PivotReverse);
            default:
                return changeState(MCMoveState::Forward);
        }
    }

    switch (moveStateGoal)
    {
        case MCMoveState::Forward:
        case MCMoveState::PivotForward:
            return changeState(MCMoveState::PivotForward);
        case MCMoveState::Reverse:
        case MCMoveState::PivotReverse:
            return changeState(MCMoveState::PivotReverse);
        default:
            return 0;
    }
}

auto MCElemental::SetNextMovePath(char& newThrottleSetting, int32_t& newGestureStateGoal) -> void
{
    MCMechWarrior* warrior = Pilot;
    MCVector3D nextWayPoint;

    if (warrior->GetNextWayPoint(nextWayPoint, 1) != 0)
    {
        warrior->SetMoveGoal(0, &nextWayPoint, nullptr);
        warrior->RequestMovePath(warrior->CurTacOrder.SelectionIndex, 1, 0);
        return;
    }

    warrior->ClearMoveOrders();
    newGestureStateGoal = 0;
}

auto MCElemental::SetControlSettings(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                     int32_t& newGestureStateGoal, int32_t& minThrottle, int32_t& maxThrottle) -> void
{
    auto* actor = static_cast<MCElementalActor*>(Appearance.get());

    if (InJump != 0 && actor->Jumping == 0)
    {
        // The jump is over: back on the path.
        InJump = 0;
        MCMovePath* path = Pilot->GetMovePath();
        path->NumSteps = path->NumStepsWhenNotPaused;
    }

    if (newGestureStateGoal == 2)
    {
        // A jump step: jump to it.
        MCMovePath* path = Pilot->GetMovePath();
        path->NumSteps = 0;
        JumpGoal = path->StepList[path->CurStep].Destination;
        actor->SetJumpParameters(static_cast<float>(DistanceFrom(JumpGoal)));
    }

    MCMechWarrior* warrior = Pilot;

    if (warrior->CurTacOrder.IsJumpOrder() != 0 && InJump == 0)
    {
        // A jump order: to its first way point.
        newGestureStateGoal = 2;
        const float* point = warrior->CurTacOrder.MoveParams.WayPath.Points;
        JumpGoal.X = point[0];
        JumpGoal.Y = point[1];
        JumpGoal.Z = point[2];
        actor->SetJumpParameters(static_cast<float>(DistanceFrom(JumpGoal)));
    }

    auto* controlData = static_cast<MCElementalControlData*>(Control->ControlData.get());

    if (newGestureStateGoal != -1)
    {
        switch (newGestureStateGoal)
        {
            case 0:
                newThrottleSetting = 0;
                break;
            case 1:
                newThrottleSetting = 100;
                break;
            case 2:
            {
                InJump = 1;
                newThrottleSetting = 0;
                break;
            }
            default:
                break;
        }

        if (newThrottleSetting < minThrottle)
        {
            newThrottleSetting = static_cast<char>(minThrottle);
        }
        else if (maxThrottle < newThrottleSetting)
        {
            newThrottleSetting = static_cast<char>(maxThrottle);
        }

        controlData->Throttle = newThrottleSetting;
    }

    if (newRotate != 0)
    {
        controlData->Rotate = newRotate;
    }
}

auto MCElemental::UpdateMovement() -> void
{
    auto* controlData = static_cast<MCElementalControlData*>(Control->ControlData.get());

    if (DisableThisFrame != 0 || ShutDownThisFrame != 0 || StartUpThisFrame != 0)
    {
        // Elementals just stand still for these; the requests clear once the actor stops.
        if (Appearance->SetGestureGoal(0) == 0)
        {
            DisableThisFrame = 0;
            ShutDownThisFrame = 0;
            StartUpThisFrame = 0;
        }

        controlData->Throttle = 0;
        return;
    }

    if (IsCaptured() != 0 || Status == 4 || Status == 5 || Status == 1)
    {
        return;
    }

    if (UpdateJump() != 0)
    {
        return;
    }

    if (PivotTo() != 0)
    {
        return;
    }

    float newRotatePerSec = 0.0f;
    int32_t minThrottle = -100;
    int32_t maxThrottle = 100;
    char newRotate = 0;
    MCMoveState newMoveState = MCMoveState::NoChange;
    int32_t newGestureStateGoal = -1;
    char newThrottleSetting = controlData->Throttle;

    if (ElementalCanJump == 0)
    {
        MCMechWarrior* warrior = Pilot;

        if (warrior->GetMovePath()->NumSteps == 0)
        {
            // An idle marine wanders. Original behaviour (OB-001): RollDice(100) is always 1, so always toward -x, -y.
            MCVector3D wanderPoint = Position;

            if (static_cast<int>(RollDice(100)) < 51)
            {
                wanderPoint.X = wanderPoint.X - static_cast<float>(RandomNumber(200));
            }
            else
            {
                wanderPoint.X = static_cast<float>(RandomNumber(200)) + wanderPoint.X;
            }

            if (static_cast<int>(RollDice(100)) < 51)
            {
                wanderPoint.Y = wanderPoint.Y - static_cast<float>(RandomNumber(200));
            }
            else
            {
                wanderPoint.Y = static_cast<float>(RandomNumber(200)) + wanderPoint.Y;
            }

            warrior->OrderMoveToPoint(0, 1, MCOrderOrigin::Player, wanderPoint, -1, 1);
        }
    }

    UpdateMoveStateGoal();

    if (UpdateMovePath(newRotate, newThrottleSetting, newRotatePerSec, newGestureStateGoal, newMoveState, minThrottle,
                       maxThrottle) != 0)
    {
        SetNextMovePath(newThrottleSetting, newGestureStateGoal);
    }

    if (newMoveState != MCMoveState::NoChange)
    {
        Pilot->MoveOrders.MoveState = newMoveState;
    }

    SetControlSettings(newRotate, newThrottleSetting, newRotatePerSec, newGestureStateGoal, minThrottle, maxThrottle);
}
