#include "stdafx.h"
#include "object/MCGroundVehicle.h"
#include "object/MCGroundVehicleType.h"
#include "object/MCGroundVehicleGameSystem.h"
#include "ai/MCMoveSystem.h"
#include "ai/MCTacticalOrder.h"
#include "camera/MCCamera.h"
#include "iface/MCTacticalInterface.h"
#include "lib/MCFatal.h"
#include "main/MCMissionGlobals.h"
#include "mission/MCScenario.h"
#include "network/MCMultiPlayer.h"
#include "object/MCAIControl.h"
#include "object/MCArtillery.h"
#include "object/MCArtilleryType.h"
#include "object/MCArtilleryChunk.h"
#include "object/MCCameraDrone.h"
#include "object/MCCameraDroneType.h"
#include "object/MCBuilding.h"
#include "object/MCBuildingType.h"
#include "object/MCBuildingMarines.h"
#include "object/MCObjectDrawing.h"
#include "object/MCMiscTerrainObject.h"
#include "object/MCMiscTerrainObjectType.h"
#include "object/MCBullet.h"
#include "object/MCBulletType.h"
#include "object/MCCollisionSystem.h"
#include "object/MCExplosion.h"
#include "object/MCExplosionType.h"
#include "object/MCMoverGroup.h"
#include "object/MCGroundVehicleControlData.h"
#include "object/MCGroundVehicleDynamics.h"
#include "object/MCJet.h"
#include "object/MCJetType.h"
#include "object/MCLaser.h"
#include "object/MCLaserType.h"
#include "object/MCMechGameSystem.h"
#include "object/MCProjectileLaser.h"
#include "object/MCProjectileLaserType.h"
#include "object/MCNetControl.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectQueue.h"
#include "object/MCSmoke.h"
#include "object/MCSmokeType.h"
#include "object/MCEffectSystem.h"
#include "object/MCMechWarrior.h"
#include "sound/MCSoundSystem.h"
#include "terrain/MCTerrain.h"
#include "object/MCObjectType.h"
#include "object/MCWeaponChunkDebug.h"
#include "object/MCWeaponShotInfo.h"
#include "object/MCMoverMath.h"

auto MCGroundVehicle::CanMove() -> int
{
    return MovementEnabled;
}

auto MCGroundVehicle::GetThrottle() -> int32_t
{
    return static_cast<MCGroundVehicleControlData*>(Control->ControlData.get())->Throttle;
}

auto MCGroundVehicle::HandleStaticCollision() -> void
{
    if (CollisionsOn == 0 || Dynamics->GetVelocity() <= 0.0f)
    {
        return;
    }

    int32_t blockNumber = 0;
    int32_t vertexNumber = 0;
    GetBlockAndVertexNumber(blockNumber, vertexNumber);
    MCObjectList* list = ObjectList()->FindList(std::format("TBlk{}", blockNumber));
    Assert(list != nullptr, blockNumber, "Could not find objlist for block");

    // Port fix: the original reads the objects of a missing list through null.
    if (list == nullptr)
    {
        return;
    }

    for (MCBaseObject* object : *list)
    {
        auto* other = static_cast<MCGameObject*>(object);

        if (other->GetObjectType() == nullptr)
        {
            continue;
        }

        int collides = 0;
        int32_t otherBlock = -1;
        int32_t otherVertex = -1;

        switch (other->ObjectClass)
        {
            case MCObjectClass::Building:
            case MCObjectClass::Tree:
            case MCObjectClass::TerrainObject:
            case MCObjectClass::TreeBuilding:
            {
                other->GetBlockAndVertexNumber(otherBlock, otherVertex);
                collides = other->CollisionsOn;
                break;
            }
            case MCObjectClass::MiscTerrainObject:
            {
                GetBlockAndVertexNumber(otherBlock, otherVertex);

                // The walls (and, compared unsigned, an object of no kind).
                if (static_cast<uint32_t>(std::to_underlying(static_cast<MCMiscTerrainObject*>(other)->Kind)) > 6)
                {
                    collides = 1;
                }
                break;
            }
            default:
                break;
        }

        if (vertexNumber == otherVertex && collides != 0)
        {
            MCCollisionSystem::DetectStaticCollision(this, other);
        }
    }
}

auto MCGroundVehicle::MineCheck() -> void
{
    if (MultiPlayer() != nullptr && MultiPlayer()->IsServer == 0)
    {
        return;
    }

    MCScenarioMap* map = GameMap();

    // The mine state bits of a tile's overlay: Inner Sphere 11..12, Clan 13..14; the spread counts 25..26, 27..28.
    if (MineCellHandled != 0)
    {
        const MCMapTile& tile = map->Map[ObjPosition->TileR * map->Width + ObjPosition->TileC];
        const uint32_t state = Alignment == -1 ? tile.Overlay >> 11 : tile.Overlay >> 13;

        if ((state & 3) == 0)
        {
            MineCellHandled = false;
            const int32_t tileR = ObjPosition->TileR;
            const int32_t tileC = ObjPosition->TileC;
            MCMapTile& here = map->Map[map->Width * tileR + tileC];

            if (GetAlignment() == -1)
            {
                here.Overlay = (here.Overlay & 0xffffefff) | 0x800;
            }
            else
            {
                here.Overlay = (here.Overlay & 0xffffbfff) | 0x2000;
            }

            if (MultiPlayer() != nullptr)
            {
                MultiPlayer()->AddMineChunk(tileR * 3, tileC * 3, Alignment != -1 ? 1 : 0, 1, 0);
                map = GameMap();
            }
        }
    }

    const uint32_t mine =
        Alignment == -1
            ? map->GetInnerSphereMine(ObjPosition->TileR, ObjPosition->TileC, ObjPosition->CellR, ObjPosition->CellC)
            : map->GetClanMine(ObjPosition->TileR, ObjPosition->TileC, ObjPosition->CellR, ObjPosition->CellC);

    if (mine == 0)
    {
        return;
    }

    int32_t firstRow = ObjPosition->TileR - 1;
    int32_t firstCol = ObjPosition->TileC - 1;

    if (firstRow < 0)
    {
        firstRow = 0;
    }

    if (firstCol < 0)
    {
        firstCol = 0;
    }

    const int32_t mapSide = MCTerrain::VerticesBlockSide * MCTerrain::BlocksMapSide;

    if (mapSide <= firstCol + 3)
    {
        firstCol = mapSide - 1;
    }

    if (mapSide <= firstRow + 3)
    {
        firstRow = mapSide - 1;
    }

    for (int32_t row = firstRow; row < firstRow + 3; row++)
    {
        for (int32_t col = firstCol; col < firstCol + 3; col++)
        {
            const bool inMap = row >= 0 && row < GameMap()->Height && col >= 0 && col < GameMap()->Width;
            Assert(inMap, 0, " Map Tile out of bounds ");

            // Port fix: the original goes on to touch the tile past the map's edge.
            if (!inMap)
            {
                continue;
            }

            MCMapTile& tile = GameMap()->Map[GameMap()->Width * row + col];
            const bool innerSphere = GetAlignment() == -1;
            uint32_t count = ((innerSphere ? tile.Overlay >> 25 : tile.Overlay >> 27) & 3) + 1;

            if (count > 3)
            {
                count = 3;
            }

            if (GetAlignment() == -1)
            {
                tile.Overlay = (tile.Overlay & 0xf9ffffff) | (count << 25);
            }
            else
            {
                tile.Overlay = (tile.Overlay & 0xe7ffffff) | (count << 27);
            }
        }
    }

    int32_t chunkResult = 0;

    if (MineSweeper != 0)
    {
        // A sweeper sets the mine off harmlessly, at the cost of a point of front armor.
        SweepTime = 0.0f;
        MCVector3D position = GetPosition();
        CreateExplosion(MineExplosion, position, 0.0f, 0.0f);
        Armor[GroundVehicleFront].CurArmor -= 1.0f;

        if (MultiPlayer() != nullptr)
        {
            MCWeaponShotInfo shotInfo;
            shotInfo.Init(nullptr, -2, 1.0f, 0, 0.0f);
            MultiPlayer()->AddWeaponHitChunk(this, &shotInfo, 0);
        }

        if (Armor[GroundVehicleFront].CurArmor == 0.0f)
        {
            MineSweeper = false;
            SweepTime = -1.0f;
            Pilot->ClearCurTacOrder(1, 0);
        }

        chunkResult = 1;
    }
    else
    {
        if (MineLayer != 0)
        {
            MineCellHandled = true;
            return;
        }

        MCVector3D position = GetPosition();
        CreateExplosion(MineExplosion, position, MineSplashDamage, WorldUnitsPerMeter * MineSplashRange);
        const int32_t hitLocation = CalcHitLocation(nullptr, -1, 3, 0);
        MCWeaponShotInfo shotInfo;
        shotInfo.Init(nullptr, -2, MineBaseDamage, hitLocation, 0.0f);
        HandleWeaponHit(&shotInfo, MultiPlayer() != nullptr);

        if (GetPilot() != nullptr)
        {
            GetPilot()->RadioMessage(MCRadioMessageType::HittingMines, 1);
        }

        Pilot->PausePath();
        chunkResult = 2;
    }

    const int32_t tileR = ObjPosition->TileR;
    const int32_t tileC = ObjPosition->TileC;
    MCMapTile& here = GameMap()->Map[GameMap()->Width * tileR + tileC];

    if (GetAlignment() == -1)
    {
        here.Overlay |= 0x1800;
    }
    else
    {
        here.Overlay |= 0x6000;
    }

    if (MultiPlayer() != nullptr)
    {
        MultiPlayer()->AddMineChunk(tileR * 3 + ObjPosition->CellR, tileC * 3 + ObjPosition->CellC,
                                    Alignment != -1 ? 1 : 0, 3, chunkResult);
    }

    MineCellHandled = true;
}

auto MCGroundVehicle::PivotTo() -> int
{
    MCMechWarrior* warrior = Pilot;
    MCMovePath* path = warrior->GetMovePath();
    const MCMoveState moveStateGoal = warrior->MoveOrders.MoveStateGoal;
    const MCMoveState moveState = warrior->MoveOrders.MoveState;
    const int32_t run =
        MultiPlayer() == nullptr || MultiPlayer()->IsServer != 0 ? warrior->MoveOrders.Run : MoveChunk.Run;
    int hasTarget = 0;
    MCGameObject* target = warrior->GetLastTarget();
    float targetFacing = 0.0f;
    MCVector3D targetPosition;
    auto* dynType =
        static_cast<MCGroundVehicleDynamicsType*>(static_cast<MCGroundVehicleType*>(ObjType)->DynamicsType.get());
    const float maxPivot = static_cast<float>(dynType->MaxVehiclePivotRate) * FrameLength;

    if (target == nullptr)
    {
        if (warrior->CurTacOrder.Code == MCTacticalOrderCode::AttackPoint)
        {
            targetPosition = warrior->AttackOrders.TargetPoint;
            targetFacing = RelFacingTo(targetPosition, -1);
            hasTarget = 1;
        }
    }
    else
    {
        targetPosition = target->GetPosition();
        targetFacing = RelFacingTo(targetPosition, -1);
        hasTarget = 1;
    }

    // Starts the pivot: a turn of <paramref name="turn"/> degrees, no faster than the pivot rate.
    const auto pivot = [&](float turn) -> int
    {
        if (maxPivot < std::fabs(turn))
        {
            turn = turn <= 0.0f ? -maxPivot : maxPivot;
        }

        auto* controlData = static_cast<MCGroundVehicleControlData*>(Control->ControlData.get());
        controlData->Rotate = static_cast<int8_t>(static_cast<int32_t>(turn / maxPivot * 64.0f));
        controlData->Pivot = 1;
        UpdateTurret(turn);
        return 1;
    };

    const auto choosePivotDirection = [&]()
    {
        if (PivotDirection == 0xff)
        {
            PivotDirection = targetFacing >= 0.0f ? 1 : 0;
        }
    };

    const auto hasNextStep = [&]()
    { return path->NumStepsWhenNotPaused >= 1 && path->CurStep < path->NumStepsWhenNotPaused; };

    if (moveState == MCMoveState::PivotForward)
    {
        if (moveStateGoal == MCMoveState::PivotForward || moveStateGoal == MCMoveState::Forward)
        {
            if (!hasNextStep())
            {
                Pilot->MoveOrders.MoveStateGoal = MCMoveState::Forward;
            }
            else
            {
                const MCVector3D destination = path->StepList[path->CurStep].Destination;
                static_cast<MCGroundVehicleControlData*>(Control->ControlData.get())->Throttle = 0;
                const float stepFacing = RelFacingTo(destination, -1);

                if (stepFacing < -45.0f || stepFacing > 45.0f)
                {
                    float turn = -stepFacing;

                    if (hasTarget != 0 && run == 0)
                    {
                        choosePivotDirection();

                        if (PivotDirection == 0)
                        {
                            if (stepFacing >= 0.0f)
                            {
                                turn = 360.0f - stepFacing;
                            }
                        }
                        else if (stepFacing < 0.0f)
                        {
                            turn = -360.0f - stepFacing;
                        }
                    }

                    return pivot(turn);
                }

                Pilot->MoveOrders.MoveState = MCMoveState::Forward;

                if (Pilot->MoveOrders.MoveStateGoalChanged != 0)
                {
                    Pilot->MoveOrders.MoveStateGoalChanged = 0;
                }
            }
        }
        else
        {
            Pilot->MoveOrders.MoveState = MCMoveState::Forward;
        }
    }
    else if (moveState == MCMoveState::PivotReverse)
    {
        if (moveStateGoal == MCMoveState::PivotReverse || moveStateGoal == MCMoveState::Reverse)
        {
            if (!hasNextStep())
            {
                Pilot->MoveOrders.MoveStateGoal = MCMoveState::Forward;
            }
            else
            {
                const MCVector3D destination = path->StepList[path->CurStep].Destination;
                static_cast<MCGroundVehicleControlData*>(Control->ControlData.get())->Throttle = 0;
                const float stepFacing = RelFacingTo(destination, -1);

                if (stepFacing > -135.0f && stepFacing < 135.0f)
                {
                    bool turnLeft;

                    if (hasTarget == 0 || run != 0)
                    {
                        turnLeft = stepFacing < 0.0f;
                    }
                    else
                    {
                        choosePivotDirection();
                        turnLeft = PivotDirection != 0;
                    }

                    return pivot(turnLeft ? -180.0f - stepFacing : 180.0f - stepFacing);
                }

                MCMechWarrior* orders = Pilot;

                if (orders->MoveOrders.MoveStateGoalChanged != 0)
                {
                    orders->MoveOrders.MoveStateGoalChanged = 0;
                }

                if (moveStateGoal == MCMoveState::Reverse)
                {
                    orders->MoveOrders.MoveState = MCMoveState::Reverse;
                }
                else
                {
                    orders->MoveOrders.MoveStateGoal = MCMoveState::Forward;
                }
            }
        }
        else
        {
            Pilot->MoveOrders.MoveState = MCMoveState::Forward;
        }
    }
    else if (moveState != MCMoveState::PivotTarget)
    {
        if (moveStateGoal == MCMoveState::PivotTarget || moveStateGoal == MCMoveState::PivotForward ||
            moveStateGoal == MCMoveState::PivotReverse)
        {
            Pilot->MoveOrders.MoveState = moveStateGoal;
        }
    }
    else if (moveStateGoal != MCMoveState::PivotTarget)
    {
        Pilot->MoveOrders.MoveState = MCMoveState::Forward;
    }
    else if (run == 0 && hasTarget != 0)
    {
        static_cast<MCGroundVehicleControlData*>(Control->ControlData.get())->Throttle = 0;
        const float facing = RelFacingTo(targetPosition, -1);
        const float fireArc = GetFireArc();

        if (facing < -fireArc || fireArc < facing)
        {
            return pivot(-facing);
        }

        Pilot->MoveOrders.MoveStateGoal = MCMoveState::Forward;
    }
    else
    {
        Pilot->MoveOrders.MoveStateGoal = MCMoveState::Forward;
    }

    MCMechWarrior* orders = Pilot;

    if (!(orders->MoveOrders.YieldTime > -1.0f || orders->MoveOrders.WaitForPointTime > -1.0f))
    {
        orders->ResumePath();
    }

    PivotDirection = 0xff;
    return 0;
}

auto MCGroundVehicle::CalcThrottleLimits(int32_t& minThrottle, int32_t& maxThrottle) -> void
{
    const MCMapTile& tile = GameMap()->Map[ObjPosition->TileR * GameMap()->Width + ObjPosition->TileC];
    const float tileFactor = TileThrottleMultiplier[Chassis][tile.Cells & 0x7f];
    const float overlayFactor = OverlayThrottleMultiplier[Chassis][tile.Overlay & 0x7f];
    // Each limit goes through a short, as in the original.
    maxThrottle = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<float>(maxThrottle) * tileFactor)));
    minThrottle = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<float>(minThrottle) * tileFactor)));
    maxThrottle =
        static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<float>(maxThrottle) * overlayFactor)));
    minThrottle =
        static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<float>(minThrottle) * overlayFactor)));
}

auto MCGroundVehicle::GetSpeedState() -> int32_t
{
    return GetBodyState() == 1 ? 2 : 0;
}

auto MCGroundVehicle::UpdateMoveStateGoal() -> void
{
    MCMechWarrior* warrior = Pilot;
    MCMovePath* path = warrior->GetMovePath();
    const MCMoveState moveStateGoal = warrior->MoveOrders.MoveStateGoal;

    if (path->NumSteps < 1)
    {
        if (moveStateGoal != MCMoveState::PivotTarget && moveStateGoal != MCMoveState::PivotForward &&
            moveStateGoal != MCMoveState::PivotReverse)
        {
            warrior->MoveOrders.MoveStateGoal = MCMoveState::Forward;
        }

        return;
    }

    const int32_t run =
        MultiPlayer() == nullptr || MultiPlayer()->IsServer != 0 ? warrior->MoveOrders.Run : MoveChunk.Run;

    if (run != 0)
    {
        warrior->MoveOrders.MoveStateGoal = MCMoveState::Forward;
        return;
    }

    MCVector3D targetPosition;
    MCGameObject* target = warrior->GetLastTarget();

    if (target == nullptr)
    {
        if (warrior->CurTacOrder.Code != MCTacticalOrderCode::AttackPoint)
        {
            warrior->MoveOrders.MoveStateGoal = MCMoveState::Forward;
            return;
        }

        targetPosition = warrior->AttackOrders.TargetPoint;
    }
    else
    {
        targetPosition = target->GetPosition();
    }

    if (path->NumStepsWhenNotPaused <= 0 || path->CurStep >= path->NumStepsWhenNotPaused)
    {
        return;
    }

    const double delta = RelFacingDelta(path->StepList[path->CurStep].Destination, targetPosition);
    MCMechWarrior* orders = Pilot;
    const double turretArc =
        static_cast<MCGroundVehicleDynamicsType*>(static_cast<MCGroundVehicleType*>(ObjType)->DynamicsType.get())
            ->MaxTurretYaw;

    if (orders->MoveOrders.MoveStateGoal == MCMoveState::Forward)
    {
        // The target is behind: drive backward.
        if (turretArc < delta && 180.0 - delta <= turretArc && orders->MoveOrders.MoveStateGoalChanged == 0)
        {
            orders->MoveOrders.MoveStateGoalChanged = 1;
            orders->MoveOrders.MoveStateGoal = MCMoveState::Reverse;
        }
    }
    else if (turretArc < 180.0 - delta && delta <= turretArc && orders->MoveOrders.MoveStateGoalChanged == 0)
    {
        orders->MoveOrders.MoveStateGoalChanged = 1;
        orders->MoveOrders.MoveStateGoal = MCMoveState::Forward;
    }
}

namespace
{
    /// <summary>
    /// Steers along the move path, the part updateMovePath and netUpdateMovePath share (each has its own copy in
    /// MCX.EXE): advances past reached steps, turns toward the step and sets the throttle for the move state, or
    /// pauses the path and asks for a pivot. Then caps the throttle of a sweeper that just cleared a mine and of a
    /// layer laying mines.
    /// </summary>
    /// <returns>1 at the path's end, else 0.</returns>
    int SteerAlongPath(MCGroundVehicle* vehicle, MCMovePath* path, char& newRotate, char& newThrottleSetting,
                       float& newRotatePerSec, MCMoveState& newMoveState, int32_t& maxThrottle)
    {
        auto* dynType = static_cast<MCGroundVehicleDynamicsType*>(
            static_cast<MCGroundVehicleType*>(vehicle->ObjType)->DynamicsType.get());
        int result = 0;
        const auto steer = [&]()
        {
            if (path->NumSteps < 1)
            {
                newThrottleSetting = 0;
                return;
            }

            int32_t step = path->CurStep;

            if (step == path->NumSteps)
            {
                result = 1;
                return;
            }

            MCVector3D destination = path->StepList[step].Destination;
            vehicle->LastValidPosition = destination;
            const auto distance = static_cast<float>(vehicle->DistanceFrom(destination));
            const int32_t numSteps = path->NumSteps;
            const float margin = step == numSteps - 1 ? MoveMarginOfError[1] : MoveMarginOfError[0];
            MaxVelocityMag = WorldUnitsPerMeter * 100.0f;

            if (distance < margin)
            {
                // Reached the step: on to the next.
                step++;
                vehicle->Pilot->MoveOrders.TimeOfLastStep = ScenarioTime;
                path->CurStep = step;

                if (numSteps <= step)
                {
                    MaxVelocityMag = WorldUnitsPerMeter * distance;
                    result = 1;
                    return;
                }

                destination = path->StepList[step].Destination;
            }

            const float facing = vehicle->RelFacingTo(destination, -1);
            MCMechWarrior* orders = vehicle->Pilot;
            const MCMoveState moveState = orders->MoveOrders.MoveState;
            const MCMoveState moveStateGoal = orders->MoveOrders.MoveStateGoal;
            const float maxTurn = static_cast<float>(dynType->MaxVehicleYawRate) * FrameLength;

            if (moveState == MCMoveState::Forward)
            {
                if (moveStateGoal == MCMoveState::Forward)
                {
                    newThrottleSetting = 100;

                    if (facing < -5.0f || facing > 5.0f)
                    {
                        newRotatePerSec = -facing;

                        if (maxTurn < std::fabs(newRotatePerSec))
                        {
                            newRotatePerSec = newRotatePerSec <= 0.0f ? -maxTurn : maxTurn;
                        }

                        newRotate = static_cast<char>(static_cast<int32_t>(newRotatePerSec / maxTurn * 64.0f));
                    }

                    return;
                }

                orders->PausePath();

                if (moveStateGoal == MCMoveState::Reverse || moveStateGoal == MCMoveState::PivotReverse)
                {
                    newMoveState = MCMoveState::PivotReverse;
                }
                else if (moveStateGoal == MCMoveState::PivotForward)
                {
                    newMoveState = MCMoveState::PivotForward;
                }
                else
                {
                    newMoveState = MCMoveState::Forward;
                }

                return;
            }

            if (moveState == MCMoveState::Reverse)
            {
                if (moveStateGoal == MCMoveState::Reverse)
                {
                    newThrottleSetting = -100;
                    newRotatePerSec = facing >= 0.0f ? -(facing - 180.0f) : -(facing + 180.0f);

                    if (std::fabs(newRotatePerSec) <= maxTurn)
                    {
                        newThrottleSetting = -100;
                    }
                    else
                    {
                        // Turning hard: back up at half speed.
                        newRotatePerSec = newRotatePerSec <= 0.0f ? -maxTurn : maxTurn;
                        newThrottleSetting = -50;
                    }

                    newRotate = static_cast<char>(static_cast<int32_t>(newRotatePerSec / maxTurn * 64.0f));
                    return;
                }

                orders->PausePath();

                if (moveStateGoal == MCMoveState::Forward || moveStateGoal == MCMoveState::PivotForward)
                {
                    newMoveState = MCMoveState::PivotForward;
                }
                else if (moveStateGoal == MCMoveState::PivotReverse)
                {
                    newMoveState = MCMoveState::PivotReverse;
                }
                else
                {
                    newMoveState = MCMoveState::Forward;
                }

                return;
            }

            if (moveStateGoal == MCMoveState::Forward || moveStateGoal == MCMoveState::PivotForward)
            {
                orders->PausePath();
                newMoveState = MCMoveState::PivotForward;
            }
            else if (moveStateGoal == MCMoveState::Reverse || moveStateGoal == MCMoveState::PivotReverse)
            {
                orders->PausePath();
                newMoveState = MCMoveState::PivotReverse;
            }
        };

        steer();

        if (vehicle->MineSweeper != 0 && vehicle->SweepTime > 0.0f && vehicle->SweepTime < GvSweepTime)
        {
            maxThrottle = MineSweepThrottle;
        }

        if (vehicle->MineLayer != 0 && vehicle->Pilot->CurTacOrder.MoveParams.Mode == 1)
        {
            maxThrottle = MineLayThrottle;
        }

        return result;
    }
}

auto MCGroundVehicle::UpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                     MCMoveState& newMoveState, int32_t& minThrottle, int32_t& maxThrottle) -> int
{
    MCMechWarrior* warrior = Pilot;
    auto* controlData = static_cast<MCGroundVehicleControlData*>(Control->ControlData.get());
    MCMovePath* path = warrior->GetMovePath();
    newThrottleSetting = static_cast<char>(controlData->Throttle);
    const int32_t running = warrior->MoveOrders.Run;
    newRotatePerSec = 0.0f;
    UpdateHustleTime();
    const bool hustling = ScenarioTime < LastHustleTime + 2.0f;
    warrior = Pilot;
    MCMover* point = warrior->GetPoint();
    const bool groupMove = warrior->CurTacOrder.IsGroupOrder() != 0 && warrior->CurTacOrder.IsMoveOrder() != 0;

    if (running == 0 && !hustling && point != nullptr && point->IsDisabled() == 0 && point != this && groupMove)
    {
        // Keep pace with the group's point: wait (at most five seconds while moving) when ahead of it.
        MCMechWarrior* pointPilot = point->GetPilot();
        pointPilot->GetMovePath();
        const float pointDistanceLeft = pointPilot->GetMoveDistanceLeft();

        if (pointDistanceLeft <= warrior->GetMoveDistanceLeft())
        {
            warrior->MoveOrders.WaitForPointTime = -1.0f;

            if (warrior->MoveOrders.YieldTime <= -1.0f)
            {
                warrior->ResumePath();
            }
        }
        else
        {
            const int32_t speedState = GetSpeedState();
            warrior = Pilot;

            if (speedState == 2)
            {
                if (warrior->MoveOrders.WaitForPointTime <= -1.0f)
                {
                    warrior->MoveOrders.WaitForPointTime = ScenarioTime + 5.0f;
                }
            }
            else if (warrior->MoveOrders.WaitForPointTime < ScenarioTime)
            {
                warrior->PausePath();
                warrior->MoveOrders.WaitForPointTime = 999999.0f;
            }
        }
    }
    else
    {
        warrior->MoveOrders.WaitForPointTime = -1.0f;
    }

    int result = SteerAlongPath(this, path, newRotate, newThrottleSetting, newRotatePerSec, newMoveState, maxThrottle);
    warrior = Pilot;

    if (result != 0)
    {
        if (warrior->MoveOrders.PathType == 2 &&
            warrior->MoveOrders.Path[0]->GlobalStep < warrior->MoveOrders.NumGlobalSteps - 1)
        {
            result = 0;
        }

        if (warrior->MoveOrders.Path[0] != nullptr)
        {
            warrior->MoveOrders.Path[0]->Clear();
        }

        newThrottleSetting = 0;
    }

    return result;
}

auto MCGroundVehicle::SetNextMovePath(char& newThrottleSetting) -> void
{
    MCMechWarrior* warrior = Pilot;

    if (warrior->PlayerOrderFromQueue != 0 && warrior->CurTacOrder.IsMoveOrder() != 0)
    {
        if (warrior->MoveOrders.Path[0] != nullptr)
        {
            warrior->MoveOrders.Path[0]->Clear();
        }

        return;
    }

    warrior->ClearMoveOrders();
    newThrottleSetting = 0;
}

auto MCGroundVehicle::SetControlSettings(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                         int32_t& minThrottle, int32_t& maxThrottle) -> void
{
    MCMechWarrior* warrior = Pilot;
    MCMovePath* path = warrior->GetMovePath();
    const int32_t run =
        MultiPlayer() == nullptr || MultiPlayer()->IsServer != 0 ? warrior->MoveOrders.Run : MoveChunk.Run;

    if (path->NumSteps == 0)
    {
        newThrottleSetting = 0;
    }

    auto* controlData = static_cast<MCGroundVehicleControlData*>(Control->ControlData.get());

    if (newThrottleSetting != -1)
    {
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

    // Anything but a run order moves at the walk speed.
    controlData->Walk = run == 0 ? 1 : 0;
}

auto MCGroundVehicle::UpdateTurret(float newRotatePerSec) -> void
{
    MCMechWarrior* warrior = Pilot;
    MCGameObject* target = warrior->GetLastTarget();
    double facing;

    if (target != nullptr)
    {
        facing = static_cast<double>(RelFacingTo(target->GetPosition(), -1)) + TurretRotation + newRotatePerSec;
    }
    else if (warrior->CurTacOrder.Code == MCTacticalOrderCode::AttackPoint)
    {
        facing =
            static_cast<double>(RelFacingTo(warrior->GetAttackTargetPoint(), -1)) + TurretRotation + newRotatePerSec;
    }
    else
    {
        facing = TurretRotation;
    }

    if (facing < -180.0)
    {
        facing += 360.0;
    }
    else if (facing > 180.0f)
    {
        facing -= 360.0;
    }

    if (facing >= -2.0 && facing <= 2.0)
    {
        return;
    }

    double turn = -facing;
    auto* dynType =
        static_cast<MCGroundVehicleDynamicsType*>(static_cast<MCGroundVehicleType*>(ObjType)->DynamicsType.get());
    const float maxTurn = static_cast<float>(dynType->MaxTurretYawRate) * FrameLength;

    if (maxTurn < std::fabs(turn))
    {
        turn = turn < 0.0 ? -maxTurn : maxTurn;
    }

    static_cast<MCGroundVehicleControlData*>(Control->ControlData.get())->TurretRotate =
        static_cast<int8_t>(static_cast<int32_t>(turn / maxTurn * 64.0f));
}

auto MCGroundVehicle::UpdateMovement() -> void
{
    auto* controlData = static_cast<MCGroundVehicleControlData*>(Control->ControlData.get());

    if (DisableThisFrame != 0)
    {
        DisableThisFrame = false;
        ShutDownThisFrame = false;
        StartUpThisFrame = false;
        Status = 1;
        controlData->Throttle = 0;
        return;
    }

    if (ShutDownThisFrame != 0)
    {
        controlData->Throttle = 0;
        ShutDownThisFrame = false;
        StartUpThisFrame = false;
        Status = 5;
        return;
    }

    if (StartUpThisFrame != 0)
    {
        controlData->Throttle = 100;
        StartUpThisFrame = false;
        Status = 0;
        return;
    }

    if (IsCaptured() != 0 || IsDisabled() != 0)
    {
        controlData->Throttle = 0;
        return;
    }

    if (EngineBlowTime > -1.0f)
    {
        return;
    }

    float newRotatePerSec = static_cast<float>(PivotTo());

    if (newRotatePerSec != 0.0f)
    {
        return;
    }

    int32_t minThrottle = -100;
    int32_t maxThrottle = 100;
    char newRotate = 0;
    char newThrottleSetting = 0;
    MCMoveState newMoveState = MCMoveState::NoChange;
    CalcThrottleLimits(minThrottle, maxThrottle);
    UpdateMoveStateGoal();

    if (UpdateMovePath(newRotate, newThrottleSetting, newRotatePerSec, newMoveState, minThrottle, maxThrottle) != 0)
    {
        SetNextMovePath(newThrottleSetting);
    }

    if (newMoveState != MCMoveState::NoChange)
    {
        Pilot->MoveOrders.MoveState = newMoveState;
    }

    SetControlSettings(newRotate, newThrottleSetting, newRotatePerSec, minThrottle, maxThrottle);
    UpdateTurret(newRotatePerSec);
}

auto MCGroundVehicle::CrashAvoidanceSystem() -> int
{
    if (MultiPlayer() != nullptr && MultiPlayer()->IsServer == 0)
    {
        return 0;
    }

    MCMechWarrior* warrior = Pilot;
    MCMovePath* path = warrior->GetMovePath();

    if (path->NumStepsWhenNotPaused == 0)
    {
        return 0;
    }

    if (static_cast<double>(warrior->MoveOrders.WaitForPointTime) > 999990.0)
    {
        return 0;
    }

    // A look a frame ahead along the frame turned by a quarter pi (its result is unused).
    const float speed = -Dynamics->GetVelocity();
    MCFrameOfRef ahead = Frame;
    MCMoverMath::RotateAboutK(ahead, static_cast<float>(std::sin(MCMoverMath::HalfPi / 2.0)),
                              static_cast<float>(std::cos(MCMoverMath::HalfPi / 2.0)));
    MCVector3D lookAhead(ahead.J.X * speed * FrameLength * WorldUnitsPerMeter + Position.X,
                         ahead.J.Y * speed * FrameLength * WorldUnitsPerMeter + Position.Y,
                         ahead.J.Z * speed * FrameLength * WorldUnitsPerMeter + Position.Z);
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    MCScenarioMap::WorldToMapPos(lookAhead, tileR, tileC, cellR, cellC);

    int cornerBlocked = 0;
    const int32_t direction = static_cast<int8_t>(path->StepList[path->CurStep].Direction);

    if (direction == 1 || direction == 3 || direction == 5 || direction == 7)
    {
        // A diagonal step: blocked when both cells beside it are locked.
        const int first = GetAdjacentCellPathLocked(ObjPosition->TileR, ObjPosition->TileC, ObjPosition->CellR,
                                                    ObjPosition->CellC, AdjClippedCell[direction][0]);
        const int second = GetAdjacentCellPathLocked(ObjPosition->TileR, ObjPosition->TileC, ObjPosition->CellR,
                                                     ObjPosition->CellC, AdjClippedCell[direction][1]);
        cornerBlocked = first != 0 && second != 0 ? 1 : 0;
    }

    int lockReachedEnd = 0;
    int blockReachedEnd = 0;
    const int locked = GetPathRangeLock(CrashAvoidPath, &lockReachedEnd);
    const int blocked = GetPathRangeBlocked(CrashAvoidPath, &blockReachedEnd);
    const int32_t closedGates = path->CrossesClosedGate(-1, 2);
    warrior = Pilot;
    const bool clear = locked == 0 && blocked == 0 && cornerBlocked == 0 && closedGates < 2;

    if (warrior->MoveOrders.YieldTime > -1.0f)
    {
        // Yielding: go on once the way is clear.
        if (clear)
        {
            warrior->ResumePath();
            warrior->MoveOrders.YieldTime = -1.0f;
            return 0;
        }

        warrior->PausePath();
        return 1;
    }

    if (clear)
    {
        return 0;
    }

    if (lockReachedEnd == 0 && blockReachedEnd == 0)
    {
        warrior->PausePath();
        warrior->MoveOrders.YieldTime = ScenarioTime + CrashYieldTime;
        Control->ControlData->Brake();
        return 1;
    }

    warrior->ReachedPathEnd();
    Control->ControlData->Brake();
    return 1;
}

auto MCGroundVehicle::NetUpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                        MCMoveState& newMoveState, int32_t& minThrottle, int32_t& maxThrottle) -> int
{
    auto* controlData = static_cast<MCGroundVehicleControlData*>(Control->ControlData.get());
    MCMovePath* path = Pilot->GetMovePath();
    newRotatePerSec = 0.0f;
    newThrottleSetting = static_cast<char>(controlData->Throttle);
    return SteerAlongPath(this, path, newRotate, newThrottleSetting, newRotatePerSec, newMoveState, maxThrottle);
}
