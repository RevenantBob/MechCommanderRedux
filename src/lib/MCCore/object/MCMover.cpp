#include "stdafx.h"
#include "object/MCMover.h"
#include "engine/MCByteFlag.h"
#include "lib/MCFatal.h"
#include "main/main.h"
#include "mission/MCMission.h"
#include "mission/MCScenario.h"
#include "network/multplyr.h"
#include "object/MCContactSystem.h"
#include "object/MCForces.h"
#include "object/MCMasterComponent.h"
#include "object/MCMechWarrior.h"
#include "object/MCMoverGameSystem.h"
#include "object/MCMoverGroup.h"
#include "object/MCObjectEvent.h"
#include "object/MCObjectQueue.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectType.h"
#include "object/MCSensorSystem.h"
#include "object/MCTeam.h"
#include "object/MCWeaponChunkDebug.h"
#include "object/MCBattleMech.h"
#include "object/MCBattleMechType.h"
#include "object/MCMechGameSystem.h"
#include "sound/MCSoundSystem.h"
#include "terrain/MCTacticalMap.h"
#include "terrain/MCTerrain.h"

int32_t TargetRolo = -1;
const std::array<float, 4> EntryAngleTable = {0.0f, 180.0f, -90.0f, 90.0f};
// Cleared at startup as WeaponFireChunk::init clears: no hit location.
MCWeaponFireChunk CurMoverWeaponFireChunk = {0, 0, 0, 0, 0, {0, 0}, 0, 0, 0, 0, 0, 0, -1, 0};

auto GetMoverFromPartId(int32_t partId) -> MCMover*
{
    // The original indexed MoverRoster from part id 0, reading the memory before it for ids under 0x200.
    if (partId >= MCMover::FirstPartId && partId < MCMover::EndPartId)
    {
        return static_cast<MCMover*>(MoverRoster[partId - MCMover::FirstPartId]);
    }

    return nullptr;
}

auto MCMover::LineOfSight(MCGameObject* target) -> int
{
    return Team->LineOfSight(target->GetPosition());
}

auto MCMover::LineOfSight(MCVector3D point) -> int
{
    return Team->LineOfSight(point);
}

auto MCMover::ForcePilotingCheck() -> void
{
    if (PilotCheckModifier < 0)
    {
        PilotCheckModifier = 0;
    }
}

auto MCMover::SetAlignment(int32_t newAlignment) -> void
{
    MCBigGameObject::SetAlignment(newAlignment);

    if (Pilot != nullptr)
    {
        Pilot->Alignment = static_cast<int8_t>(newAlignment);
    }
}

auto MCMover::RelViewFacingTo(MCVector3D goal) -> float
{
    return MCGameObject::RelFacingTo(goal, -1);
}

auto MCMover::GetJumpRange(int32_t* numOffsets, int32_t* jumpCost) -> float
{
    if (numOffsets != nullptr)
    {
        *numOffsets = 8;
    }

    if (jumpCost != nullptr)
    {
        *jumpCost = 0;
    }

    return 0.0f;
}

auto MCMover::CalcSpriteSpeed(float, uint32_t, int32_t& state, int32_t& throttle) -> int32_t
{
    state = 0;
    throttle = 100;
    return -1;
}

auto MCMover::GetPositionFromHS(uint32_t) -> MCVector3D
{
    MCVector3D position;
    position.X = 0.0f;
    position.Y = 0.0f;
    position.Z = 0.0f;
    return position;
}

MCMover::MCMover()
{
    // The base objects' fields (their inits are inline and not called).
    IdNumber = 0;
    Position = MCVector3D(0.0f, 0.0f, 0.0f);
    PartId = -1;
    ObjType = nullptr;
    Selected = 0;
    Alignment = 0;
    Status = 0;
    ObjectClass = MCObjectClass::Mover;
    Team = nullptr;
    CurCV = 0;
    MaxCV = 0;
    CollisionsOn = 1;
    Frame.ResetToWorldFrame();
    MoveChunk.Reset();

    if (MPlayer != nullptr)
    {
        std::array<char, 0x100> name{};
        CLoadString(ThisInstance, 0xb9, name.data(), 0xfe);
        NetName = name.data();
    }
}

auto MCMover::SetPartId(int32_t newPartId) -> void
{
    PartId = newPartId;

    // The original stored ids under 0x200 before MoverRoster (see getMoverFromPartId).
    if (newPartId >= FirstPartId && newPartId < EndPartId)
    {
        MoverRoster[newPartId - FirstPartId] = this;
    }
}

auto MCMover::SetPartId(int32_t commanderId, int32_t groupId, int32_t index) -> void
{
    SetPartId(index + 0x200 + (commanderId * 32 + groupId) * 12);
}

auto MCMover::SetPosition(MCVector3D& newPosition) -> void
{
    // Kept on the map; a mover pushed off it (or into the corners, which the map's diamond cuts off) is destroyed
    // when the mover is withdrawing.
    const float halfSide = WorldUnitsMapSide * 0.5f;
    const float negHalfSide = -halfSide;
    const float startX = newPosition.X;

    if (startX < negHalfSide)
    {
        newPosition.X = negHalfSide;
    }

    const float clampedX = newPosition.X;

    if (halfSide < clampedX)
    {
        newPosition.X = halfSide;
    }

    const float startY = newPosition.Y;

    if (negHalfSide > startY)
    {
        newPosition.Y = negHalfSide;
    }

    bool onMap = false;

    if (newPosition.Y <= halfSide)
    {
        if (negHalfSide <= startY && halfSide >= clampedX && negHalfSide <= startX)
        {
            const double limit = static_cast<double>(MCTerrain::VerticesBlockSide) * MCTerrain::BlocksMapSide *
                                     MCTerrain::MetersPerVertex * 0.5f -
                                 1300.0;
            const double diff = static_cast<double>(newPosition.Y) - newPosition.X;
            const float sum = newPosition.X + newPosition.Y;
            const float negLimit = static_cast<float>(-limit);
            onMap = !(diff > limit) && diff >= negLimit && !(sum > limit) && sum >= negLimit;
        }
    }
    else
    {
        newPosition.Y = halfSide;
    }

    if (!onMap && Withdrawing != 0)
    {
        ObjType->HandleDestruction(this, nullptr);
    }

    Position = newPosition;

    if (ObjPosition != nullptr)
    {
        GameObjectMap()->UpdateObject(this);
    }
}

auto MCMover::SetAwake(int awake) -> void
{
    Flags &= 0xfe;

    if (awake == 0)
    {
        return;
    }

    Flags |= 1;

    if (Pilot != nullptr && static_cast<uint8_t>(Status) == 5)
    {
        Pilot->OrderPowerUp(0, MCOrderOrigin::Self);
    }
}

auto MCMover::RelFacingDelta(MCVector3D goalPos, MCVector3D targetPos) -> float
{
    const float goalFacing = RelFacingTo(goalPos, -1);
    const float targetFacing = RelFacingTo(targetPos, -1);

    // The angle between the two facings, at most 180 when they're on opposite sides.
    if (goalFacing < 0.0f)
    {
        if (targetFacing >= 0.0f)
        {
            const float delta = targetFacing - goalFacing;
            return 180.0f < delta ? 180.0f : delta;
        }

        if (targetFacing < goalFacing)
        {
            return goalFacing - targetFacing;
        }
    }
    else
    {
        if (targetFacing < 0.0f)
        {
            const float delta = goalFacing - targetFacing;
            return 180.0f < delta ? 180.0f : delta;
        }

        if (targetFacing < goalFacing)
        {
            return goalFacing - targetFacing;
        }
    }

    return targetFacing - goalFacing;
}

namespace
{
    /// <summary>A quarter turn's half, as MCX.EXE stores it (a hair over pi / 4).</summary>
    constexpr double EIGHTH_TURN = 0x1.921fb5443e88cp-1;
    /// <summary>Radians to degrees.</summary>
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;
    /// <summary>Radians to degrees, the float-rounded copy.</summary>
    constexpr double RADIANS_TO_DEGREES_F = 0x1.ca5dc2p+5;

    /// <summary>The frame turned an eighth of a turn about its up axis (the facing the art is drawn at).</summary>
    MCFrameOfRef TurnedFrame(const MCFrameOfRef& frame)
    {
        const float s = static_cast<float>(std::sin(EIGHTH_TURN));
        const float c = static_cast<float>(std::cos(EIGHTH_TURN));
        MCFrameOfRef turned = frame;
        turned.I = frame.I * c + frame.J * s;
        turned.J = frame.J * c - frame.I * s;
        return turned;
    }
}

auto MCMover::RelFacingTo(MCVector3D goal, int32_t) -> float
{
    const float x = Position.X;
    const float y = Position.Y;
    const MCFrameOfRef turned = TurnedFrame(Frame);
    MCVector3D facing;
    facing.X = -turned.J.X;
    facing.Y = -turned.J.Y;
    facing.Z = -turned.J.Z;

    MCVector3D toGoal;
    toGoal.X = goal.X - x;
    toGoal.Y = goal.Y - y;
    toGoal.Z = 0.0f;
    const double length =
        std::sqrt((static_cast<double>(toGoal.X) * toGoal.X + static_cast<double>(toGoal.Y) * toGoal.Y) +
                  static_cast<double>(toGoal.Z) * toGoal.Z);

    if (length != 0.0)
    {
        toGoal.X = static_cast<float>(toGoal.X / length);
        toGoal.Y = static_cast<float>(toGoal.Y / length);
        toGoal.Z = static_cast<float>(toGoal.Z / length);
    }

    const double cosine = static_cast<double>(toGoal.Z) * facing.Z + static_cast<double>(toGoal.Y) * facing.Y +
                          static_cast<double>(toGoal.X) * facing.X;
    const float angle = static_cast<float>(AcosMatherr(cosine) * RADIANS_TO_DEGREES_F);

    // Negative to the left.
    if ((facing & toGoal).Z >= 0.0f)
    {
        return -angle;
    }

    return angle;
}

auto MCMover::GetTerrainAngle() -> float
{
    return static_cast<float>(AcosMatherr(static_cast<double>(TerrainNormal.Z)) * RADIANS_TO_DEGREES);
}

auto MCMover::GetVelocityTilt() -> float
{
    const MCFrameOfRef turned = TurnedFrame(Frame);
    const double cosine = static_cast<double>(turned.J.Z) * TerrainNormal.Z +
                          static_cast<double>(turned.J.Y) * TerrainNormal.Y +
                          static_cast<double>(turned.J.X) * TerrainNormal.X;
    return static_cast<float>(AcosMatherr(cosine) * RADIANS_TO_DEGREES);
}

auto MCMover::GetFireArc() -> float
{
    switch (ObjectClass)
    {
        case MCObjectClass::BattleMech:
            return FireArc[0];
        case MCObjectClass::GroundVehicle:
            return FireArc[1];
        case MCObjectClass::Elemental:
            return FireArc[2];
        default:
            return 60.0f;
    }
}

MCMover::~MCMover()
{
    if (SensorSystem != nullptr)
    {
        SensorSystemManager()->FreeSensor(SensorSystem);
        SensorSystem = nullptr;
    }

    if (EcmTracker != nullptr)
    {
        Team->RemoveEcm(EcmTracker);
        EcmTracker = nullptr;
    }

    if (JammerTracker != nullptr)
    {
        Team->RemoveJammer(JammerTracker);
        JammerTracker = nullptr;
    }

    if (PotentialContact != nullptr)
    {
        PotentialContactManager()->Remove(PotentialContact);
        PotentialContact = nullptr;
    }
}

auto MCMover::RelativePosition(float angle, float distance, uint32_t flags) -> MCVector3D
{
    // The point distance meters away at angle: flag 1, an absolute angle in radians; else degrees from the
    // mover's facing. The x87 keeps some of the sums below at extended precision, done here in double.
    const float reach = -(WorldUnitsPerMeter * distance);
    const float x = Position.X;
    const float y = Position.Y;
    double offsetX;
    float offsetY;

    if ((flags & 1) != 0)
    {
        const double sine = std::sin(static_cast<double>(angle));
        const float cosine = static_cast<float>(std::cos(static_cast<double>(angle)));
        offsetX = (sine + 0.0) * reach;
        offsetY = cosine * reach;
    }
    else
    {
        MCFrameOfRef turned = Frame;
        const double radians = (static_cast<double>(angle) + 45.0) * 0x1.1df46a2526c7ap-6;
        const float s = static_cast<float>(std::sin(radians));
        const float c = static_cast<float>(std::cos(radians));
        const MCVector3D oldI = turned.I;
        turned.I = turned.I * c + turned.J * s;
        turned.J = turned.J * c - oldI * s;
        const MCVector3D offset = turned.J * reach;
        offsetX = offset.X;
        offsetY = offset.Y;
    }

    const double targetX = offsetX + x;
    const float targetY = static_cast<float>(static_cast<double>(offsetY) + y);

    // Flag 2 walks from the mover out to the point; otherwise from the point back to the mover.
    MCVector2D start;
    MCVector2D end;

    if ((flags & 2) != 0)
    {
        end.X = static_cast<float>(targetX);
        start.X = x;
        start.Y = y;
        end.Y = targetY;
    }
    else
    {
        start.Y = targetY;
        start.X = static_cast<float>(targetX);
        end.X = x;
        end.Y = y;
    }

    // Half a map cell per step.
    const double deltaX = static_cast<double>(end.X) - start.X;
    const float deltaXf = static_cast<float>(deltaX);
    const float deltaY = end.Y - start.Y;
    const float length =
        static_cast<float>(std::sqrt(static_cast<double>(deltaY) * deltaY + static_cast<double>(deltaXf) * deltaXf));
    double directionX = deltaX;
    float directionY = deltaY;

    if (length != 0.0)
    {
        directionX = static_cast<double>(deltaXf) / length;
        directionY = static_cast<float>(static_cast<double>(deltaY) / length);
    }

    const float stepLength = static_cast<float>(static_cast<double>(MCTerrain::MetersPerVertex) * 0.33333334f * 0.5);
    const float stepX = static_cast<float>(directionX * stepLength);
    const double stepYExact = static_cast<double>(directionY) * stepLength;
    const float stepY = static_cast<float>(stepYExact);

    if (std::sqrt(stepYExact * stepY + static_cast<double>(stepX) * stepX) == 0.0)
    {
        MCVector3D result;
        result.X = x;
        result.Y = y;
        result.Z = 0.0f;
        return result;
    }

    const MCVector2D span = start - end;
    const float maxDistance =
        static_cast<float>(std::sqrt(static_cast<double>(span.X) * span.X + static_cast<double>(span.Y) * span.Y));
    float traveled = 0.0f;
    MCVector2D current = start;

    // Whether the cell under current is passable.
    auto cellPassable = [&]()
    {
        MCVector3D point;
        point.X = current.X;
        point.Y = current.Y;
        point.Z = 0.0f;
        int32_t tileR;
        int32_t tileC;
        int32_t cellR;
        int32_t cellC;
        GameMap()->WorldToMapPos(point, tileR, tileC, cellR, cellC);

        // Port fix: the walk can leave the map, where the original reads outside it. Off the map is impassable.
        if (!GameMap()->OnMap(tileR, tileC))
        {
            return 0u;
        }

        return GameMap()->Map[GameMap()->Width * tileR + tileC].GetCellPassable(cellR, cellC);
    };

    uint32_t passable = cellPassable();
    MCVector2D previous = start;
    // Walk until the cell changes kind (or the distance runs out); the answer is the step before.
    const uint32_t keepGoingWhile = (flags & 2) != 0 ? 1u : 0u;

    if ((passable != 0) == (keepGoingWhile != 0))
    {
        while (traveled < maxDistance)
        {
            previous = current;
            current.X = stepX + current.X;
            current.Y = stepY + current.Y;
            const double dx = static_cast<double>(current.X) - start.X;
            const double dy = static_cast<double>(current.Y) - start.Y;
            traveled = static_cast<float>(std::sqrt(dx * dx + dy * dy));
            passable = cellPassable();

            if ((passable != 0) != (keepGoingWhile != 0))
            {
                break;
            }
        }
    }

    MCVector3D ground;
    ground.X = previous.X;
    ground.Y = previous.Y;
    ground.Z = 0.0f;
    MCVector3D result;
    result.X = previous.X;
    result.Y = previous.Y;
    result.Z = GameMap()->GetTerrainElevation(ground);
    return result;
}

auto MCMover::LineOfFire(MCGameObject* target) -> int
{
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    GameMap()->WorldToMapPos(target->GetPosition(), tileR, tileC, cellR, cellC);
    target->ClearLineOfFire();
    const int result = GameMap()->LineOfFire(Position, target->GetPosition());
    target->RestoreLineOfFire();
    return result;
}

auto MCMover::LineOfFire(MCVector3D point) -> int
{
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    GameMap()->WorldToMapPos(point, tileR, tileC, cellR, cellC);
    return GameMap()->LineOfFire(Position, point);
}

auto MCMover::LineOfSensor(MCGameObject* target, int32_t& sensorResult, int32_t& losResult) -> void
{
    // Eye to eye, ten meters up; neither blocks itself.
    MCVector3D start;
    start.X = Position.X;
    start.Y = Position.Y;
    start.Z = static_cast<float>(static_cast<double>(WorldUnitsPerMeter) * 10.0 + Position.Z);
    const MCVector3D targetPosition = target->GetPosition();
    MCVector3D end;
    end.X = targetPosition.X;
    end.Y = targetPosition.Y;
    end.Z = static_cast<float>(static_cast<double>(WorldUnitsPerMeter) * 10.0 + targetPosition.Z);
    SetUseMe(0);
    target->SetUseMe(0);
    GameMap()->LineOfSensor(start, end, sensorResult, losResult);
    SetUseMe(1);
    target->SetUseMe(1);
}

auto MCMover::HandleEvent(MCObjectEvent* event) -> int32_t
{
    switch (event->Type)
    {
        case 0:
        {
            // Interface events.
            switch (event->Id)
            {
                case 0x1c:
                {
                    Selected = 1;
                    SelectionIndex = event->SelectionIndex;
                    return 0;
                }
                case 0x1d:
                {
                    SetSelected(0);
                    SelectionIndex = -1;
                    return 0;
                }
                case 0x1e:
                case 0x1f:
                {
                    return 0;
                }
                default:
                {
                    if (event->Id < 0 || event->Id > 0x1b)
                    {
                        Fatal(2, " Bad ObjectEvent GUI Code ");
                    }

                    return 0;
                }
            }
        }
        case 1:
        {
            if (event->Id != 6 && event->Id != 7)
            {
                Fatal(0, " Bad ObjectEvent Message Code ");
            }

            return 0;
        }
        case 2:
        {
            if (event->Id < 0 || event->Id > 8)
            {
                Fatal(3, " Bad ObjectEvent Combat Code ");
            }

            return 0;
        }
        default:
        {
            Fatal(1, std::format("Mover::handleEvent->Bad ObjectEvent Type ({})", event->Type));
        }
    }
}

auto MCMover::HandleTacticalOrder(MCTacticalOrder tacOrder, int32_t priority, int queuePlayerOrder) -> int32_t
{
    if (queuePlayerOrder != 0)
    {
        tacOrder.Pack();
    }

    // A client checks the order survives packing (the result isn't used).
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        tacOrder.Pack();
        MCTacticalOrder check;
        check.Reset();
        check.Data[0] = tacOrder.Data[0];
        check.Data[1] = tacOrder.Data[1];
        check.Unpack();
    }

    int32_t radioMessageId = -1;
    int giveOrder = 1;
    bool checkCanMove = false;

    switch (tacOrder.Code)
    {
        case MCTacticalOrderCode::Wait:
        case MCTacticalOrderCode::Escort:
        case MCTacticalOrderCode::Follow:
        case MCTacticalOrderCode::Guard:
        case MCTacticalOrderCode::Stop:
        case MCTacticalOrderCode::PowerUp:
        case MCTacticalOrderCode::PowerDown:
        case MCTacticalOrderCode::WayPointsDone:
        case MCTacticalOrderCode::Eject:
        case MCTacticalOrderCode::AttackPoint:
        case MCTacticalOrderCode::HoldFire:
        case MCTacticalOrderCode::Withdraw:
        case MCTacticalOrderCode::Capture:
        case MCTacticalOrderCode::Refit:
        case MCTacticalOrderCode::GetFixed:
        case MCTacticalOrderCode::LoadIntoCarrier:
        case MCTacticalOrderCode::DeployElementals:
            break;
        case MCTacticalOrderCode::MoveToPoint:
        {
            // A group member's delayed start.
            const int32_t delay = SelectionIndex;

            if (delay != -1)
            {
                tacOrder.DelayedTime = static_cast<float>(delay) * DelayedOrderTime + ScenarioTime;
            }

            if (IsDisabled() != 0 && CanMove() == 0)
            {
                radioMessageId = 0x1f;
                giveOrder = 0;
            }
            break;
        }

        case MCTacticalOrderCode::JumpToPoint:
        case MCTacticalOrderCode::JumpToObject:
        {
            // Only mechs jump, not onto their own side, within range, onto an open cell.
            int canJumpThere = ObjectClass == MCObjectClass::BattleMech ? 1 : 0;
            MCGameObject* target = tacOrder.Target;

            if (target != nullptr && IsMoverClass(target->ObjectClass) && target->GetTeam() == GetTeam())
            {
                canJumpThere = 0;
            }

            const float jumpRange = GetJumpRange(nullptr, nullptr);
            MCVector3D jumpGoal = tacOrder.GetWayPoint(0);

            if (jumpRange < DistanceFrom(jumpGoal))
            {
                canJumpThere = 0;
            }

            bool cellOpen = true;

            if (ObjectClass == MCObjectClass::BattleMech)
            {
                int32_t tileR;
                int32_t tileC;
                int32_t cellR;
                int32_t cellC;
                GameMap()->WorldToMapPos(tacOrder.GetWayPoint(0), tileR, tileC, cellR, cellC);
                // Port fix: the player's jump point can be off the map, where the original reads outside it.
                cellOpen = GameMap()->OnMap(tileR, tileC) &&
                           GameMap()->Map[GameMap()->Width * tileR + tileC].GetCellPassable(cellR, cellC) != 0;
            }

            if (!cellOpen || canJumpThere == 0)
            {
                radioMessageId = 0x1b;
                giveOrder = 0;
            }

            checkCanMove = true;
            break;
        }

        case MCTacticalOrderCode::MoveToObject:
        case MCTacticalOrderCode::TraversePath:
        case MCTacticalOrderCode::PatrolPath:
            checkCanMove = true;
            break;
        case MCTacticalOrderCode::AttackObject:
        {
            // An attack by jumping (method 1) becomes a jump onto the target.
            if (tacOrder.AttackParams.Method == 1)
            {
                tacOrder.Code = MCTacticalOrderCode::JumpToObject;
                tacOrder.MoveParams.Wait = 0;
                tacOrder.MoveParams.WayPath.Mode[0] = 0;

                if (tacOrder.Target != nullptr)
                {
                    tacOrder.SetWayPoint(0, tacOrder.Target->GetPosition());
                }
            }
            break;
        }
        default:
        {
            Assert(false, 1,
                   std::format("Mover::handleTacticalOrder->Bad TacOrder Code ({})", static_cast<int>(tacOrder.Code)));
            return 1;
        }
    }

    if (checkCanMove && IsDisabled() != 0 && CanMove() == 0)
    {
        radioMessageId = 0x1f;
        giveOrder = 0;
    }

    MCMechWarrior* vehiclePilot = Pilot;

    if (vehiclePilot != nullptr)
    {
        vehiclePilot->RadioMessage(radioMessageId, 1);
    }

    if (MPlayer != nullptr)
    {
        tacOrder.SetId(vehiclePilot);
    }

    if (giveOrder != 0)
    {
        switch (tacOrder.Origin)
        {
            case MCOrderOrigin::Player:
            {
                if (queuePlayerOrder != 0)
                {
                    vehiclePilot->AddQueuedTacOrder(tacOrder);
                    vehiclePilot->TacOrderQueueExecuting = 1;
                    return 0;
                }

                vehiclePilot->SetPlayerTacOrder(tacOrder, 0);
                break;
            }
            case MCOrderOrigin::Commander:
            {
                vehiclePilot->SetGeneralTacOrder(tacOrder);
                return 0;
            }
            case MCOrderOrigin::Self:
            {
                vehiclePilot->SetAlarmTacOrder(tacOrder, priority);
                return 0;
            }
            default:
                break;
        }
    }

    return 0;
}

auto MCMover::ReduceAntiMissileAmmo(int32_t numShots) -> void
{
    if (numShots > 0)
    {
        ReduceAmmo(MasterComponentList[Inventory[AntiMissileSystem[0]].MasterID].AmmoMasterId, numShots);
    }
}

auto MCMover::FireAntiMissileSystem(int32_t numMissiles, int32_t& antiMissileShots) -> int32_t
{
    for (int32_t i = 0; i < NumAntiMissileSystems; i++)
    {
        const MCInventoryItem& system = Inventory[AntiMissileSystem[i]];

        if (numMissiles <= 0 || AmmoTypeTotal[system.AmmoIndex].CurAmount <= 0)
        {
            continue;
        }

        // Each volley stops one to six missiles.
        const int32_t clan = system.MasterID == MasterClanAntiMissileSystemID ? 1 : 0;

        for (int32_t volley = 0; volley < AntiMissileSystemStats[clan][0]; volley++)
        {
            numMissiles += -1 - RandomNumber(6);
        }

        antiMissileShots = (RandomNumber(6) + 1) * AntiMissileSystemStats[clan][1];
    }

    if (numMissiles < 0)
    {
        numMissiles = 0;
    }

    return numMissiles;
}

auto MCMover::PilotingCheck(uint32_t, float) -> void
{
    PilotingCheckPending = 0;
}

auto MCMover::UpdateDamageTakenRate() -> void
{
    if (!(DamageRateCheckTime < ScenarioTime))
    {
        return;
    }

    const int32_t damageRate = static_cast<int32_t>(static_cast<double>(DamageRateTally) / DamageRateFrequency);

    if (damageRate > 10)
    {
        Pilot->TriggerAlarm(MCPilotAlarmType::DamageTakenRate, static_cast<uint32_t>(damageRate));
    }

    DamageRateTally = 0.0f;
    DamageRateCheckTime = DamageRateFrequency + DamageRateCheckTime;
}

auto MCMover::SetTeam(MCTeam* newTeam) -> int32_t
{
    Team = newTeam;
    SetAlignment(newTeam->Alignment);

    if (SensorSystem != nullptr)
    {
        SensorSystem->SetTeam(Team);
        SensorSystem->ScanFrequency = ContactUpdateFrequency;
    }

    if (Team != nullptr)
    {
        if (Ecm != 0xff)
        {
            EcmTracker = Team->AddEcm(this, Inventory[Ecm].MasterID);
        }

        if (Jammer != 0xff)
        {
            JammerTracker = Team->AddJammer(this, Inventory[Jammer].MasterID);
        }
    }

    if (Pilot != nullptr)
    {
        Pilot->SetTeam(newTeam);
    }

    return 0;
}

auto MCMover::SetGroup(MCMoverGroup* newGroup) -> int32_t
{
    Group = newGroup;

    if (newGroup != nullptr && Pilot != nullptr)
    {
        Pilot->ClearCurTacOrder(0, 0);
        Pilot->OrderState = MCOrderState::General;
    }

    return 0;
}

auto MCMover::SetPilot(MCMechWarrior* newPilot) -> void
{
    Pilot = newPilot;

    if (SensorSystem != nullptr)
    {
        SensorSystem->SetRange(SensorSystem->Range);
    }

    newPilot->Alignment = static_cast<int8_t>(Alignment);
    newPilot->SetVehicle(this);
}

auto MCMover::GetPoint() -> MCMover*
{
    if (Group != nullptr)
    {
        return Group->GetPoint();
    }

    return nullptr;
}

auto MCMover::ClearWeaponFireChunks(int32_t which) -> int32_t
{
    const int32_t numChunks = NumWeaponFireChunks[which];
    NumWeaponFireChunks[which] = 0;
    return numChunks;
}

auto MCMover::AddWeaponFireChunk(int32_t which, MCWeaponFireChunk* chunk) -> int32_t
{
    if (NumWeaponFireChunks[which] == MaxWeaponFireChunks)
    {
        Fatal(0, " Mover::addWeaponFireChunk--Too many weaponfire chunks ");
    }

    chunk->Pack();
    WeaponFireChunks[which][NumWeaponFireChunks[which]] = chunk->Data;
    NumWeaponFireChunks[which]++;
    return NumWeaponFireChunks[which];
}

auto MCMover::AddWeaponFireChunks(int32_t which, std::span<const uint32_t> packedChunks) -> int32_t
{
    if (NumWeaponFireChunks[which] + std::ssize(packedChunks) > MaxWeaponFireChunks - 1)
    {
        Fatal(0, " Mover::addWeaponFireChunks--Too many weaponfire chunks ");
    }

    for (const uint32_t packed : packedChunks)
    {
        WeaponFireChunks[which][NumWeaponFireChunks[which]] = packed;
        NumWeaponFireChunks[which]++;
        // Unpacked into a scratch chunk (the result isn't kept).
        MCWeaponFireChunk chunk;
        chunk.Init();
        chunk.Data = packed;
        chunk.Unpack(this);
    }

    return NumWeaponFireChunks[which];
}

auto MCMover::GrabWeaponFireChunks(int32_t which, std::span<uint32_t> packedChunks) -> int32_t
{
    // The count drops by those taken, but the ones left are the first ones again (as the original left them).
    const int32_t numChunks = NumWeaponFireChunks[which];
    const int32_t numGrabbed = std::min(static_cast<int32_t>(packedChunks.size()), numChunks);
    std::copy_n(WeaponFireChunks[which].begin(), numGrabbed, packedChunks.begin());
    NumWeaponFireChunks[which] = numChunks - numGrabbed;
    return numGrabbed;
}

auto MCMover::UpdateWeaponFireChunks(int32_t which) -> int32_t
{
    // Replays the weapon fire the network sent: each chunk's shot, on its target.
    for (int32_t i = 0; i < NumWeaponFireChunks[which]; i++)
    {
        MCWeaponFireChunk chunk = {0, 0, 0, 0, 0, {0, 0}, 0, 0, 0, 0, 0, 0, -1, 0};
        chunk.Data = WeaponFireChunks[which][i];
        chunk.Unpack(this);
        CurMoverWeaponFireChunk = chunk;

        const int32_t weaponIndex = NumOther + chunk.WeaponIndex;

        if (IsWeaponIndex(weaponIndex) == 0)
        {
            continue;
        }

        TargetRolo = chunk.TargetType;

        switch (chunk.TargetType)
        {
            case 0:
            case 1:
            case 2:
            {
                MCBaseObject* target = nullptr;
                const char* missing = nullptr;

                if (chunk.TargetType == 0)
                {
                    target = MPlayer->MoverRoster[chunk.TargetId];
                    missing = " Mover.updateWeaponFireChunks: NULL Mover Target (save wfchunk.dbg file) ";
                }
                else
                {
                    target = ObjectList()->FindObjectFromPart(chunk.TargetId);
                    missing = chunk.TargetType == 1
                                  ? " Mover.updateWeaponFireChunks: NULL Terrain Target (save wfchunk.dbg file) "
                                  : " Mover.updateWeaponFireChunks: NULL Special Target (save wfchunk.dbg file) ";
                }

                if (target == nullptr)
                {
                    DebugWeaponFireChunk(&chunk, nullptr, this);
                    Assert(0, 0, missing);
                }

                HandleWeaponFire(weaponIndex, static_cast<MCGameObject*>(target), nullptr, chunk.Hit,
                                 EntryAngleTable[chunk.EntryAngle], chunk.NumMissiles, chunk.NumMissilesPastAms,
                                 chunk.NumAntiMissileShots, chunk.HitLocation);
                break;
            }

            case 3:
            {
                // A point on the ground: the middle of the target cell.
                const float halfSide = WorldUnitsMapSide * 0.5f;
                MCVector3D point;
                point.X =
                    static_cast<float>((chunk.TargetCell[1] + 0.5f) * static_cast<double>(MetersPerCell()) - halfSide);
                point.Y = static_cast<float>(
                    (static_cast<double>(halfSide) - chunk.TargetCell[0] * static_cast<double>(MetersPerCell())) -
                    static_cast<double>(MetersPerCell()) * 0.5f);
                point.Z = 0.0f;
                point.Z = GameMap()->GetTerrainElevation(point);
                HandleWeaponFire(weaponIndex, nullptr, &point, chunk.Hit, 0.0f, chunk.NumMissiles,
                                 chunk.NumMissilesPastAms, 0, 0);
                break;
            }

            default:
                Fatal(0, " Mover.updateWeaponFireChunks: bad targetType ");
        }
    }

    NumWeaponFireChunks[which] = 0;
    return 0;
}

auto MCMover::ClearCriticalHitChunks(int32_t which) -> int32_t
{
    const int32_t numChunks = NumCriticalHitChunks[which];
    NumCriticalHitChunks[which] = 0;
    return numChunks;
}

auto MCMover::AddCriticalHitChunk(int32_t which, int32_t bodyLocation, int32_t criticalSpace) -> int32_t
{
    if (NumCriticalHitChunks[which] == MaxWeaponFireChunks)
    {
        Fatal(0, " Mover::addCriticalHitChunk--Too many criticalhit chunks ");
    }

    CriticalHitChunks[which][NumCriticalHitChunks[which]] = static_cast<uint8_t>(bodyLocation * 16 + criticalSpace);
    NumCriticalHitChunks[which]++;
    return NumCriticalHitChunks[which];
}

auto MCMover::AddCriticalHitChunks(int32_t which, std::span<const uint8_t> packedChunks) -> int32_t
{
    if (NumCriticalHitChunks[which] + std::ssize(packedChunks) > MaxWeaponFireChunks - 1)
    {
        Fatal(0, " Mover::addCriticalHitChunks--Too many criticalhit chunks ");
    }

    std::ranges::copy(packedChunks, CriticalHitChunks[which].begin() + NumCriticalHitChunks[which]);
    NumCriticalHitChunks[which] += static_cast<int32_t>(packedChunks.size());
    return NumCriticalHitChunks[which];
}

auto MCMover::GrabCriticalHitChunks(int32_t which, uint8_t* packedChunks) -> int32_t
{
    const int32_t numChunks = NumCriticalHitChunks[which];
    std::copy_n(CriticalHitChunks[which].begin(), numChunks, packedChunks);
    return numChunks;
}

auto MCMover::UpdateCriticalHitChunks(int32_t which) -> int32_t
{
    NumCriticalHitChunks[which] = 0;
    return 0;
}

auto MCMover::ClearRadioChunks(int32_t which) -> int32_t
{
    const int32_t numChunks = NumRadioChunks[which];
    NumRadioChunks[which] = 0;
    return numChunks;
}

auto MCMover::AddRadioChunk(int32_t which, uint8_t msg) -> int32_t
{
    if (NumRadioChunks[which] == MaxRadioChunks)
    {
        return MaxRadioChunks;
    }

    RadioChunks[which][NumRadioChunks[which]] = msg;
    NumRadioChunks[which]++;
    return NumRadioChunks[which];
}

auto MCMover::AddRadioChunks(int32_t which, std::span<const uint8_t> packedChunks) -> int32_t
{
    for (const uint8_t msg : packedChunks)
    {
        AddRadioChunk(which, msg);
    }

    return NumRadioChunks[which];
}

auto MCMover::GrabRadioChunks(int32_t which, uint8_t* packedChunks) -> int32_t
{
    const int32_t numChunks = NumRadioChunks[which];
    std::copy_n(RadioChunks[which].begin(), numChunks, packedChunks);
    return numChunks;
}

auto MCMover::UpdateRadioChunks(int32_t which) -> int32_t
{
    if (NetPlayerId >= 0)
    {
        for (int32_t i = 0; i < NumRadioChunks[which]; i++)
        {
            PlayMessage(static_cast<MCRadioMessageType>(RadioChunks[which][i]), 0);
        }
    }

    NumRadioChunks[which] = 0;
    return 0;
}

auto MCMover::PlayMessage(MCRadioMessageType messageId, int propogateIfMultiplayer) -> void
{
    if (Pilot != nullptr)
    {
        Pilot->RadioMessage(messageId, propogateIfMultiplayer);
    }
}

namespace
{
    /// <summary>Whether <paramref name="bits"/> shows any corner of the tile the object stands on.</summary>
    int TileVisible(MCByteFlag* bits, const MCObjectPosition* objPosition)
    {
        const uint32_t row = static_cast<uint32_t>(objPosition->TileR);
        const uint32_t col = static_cast<uint32_t>(objPosition->TileC);

        if (bits->GetFlag(row, col) != 0)
        {
            return 1;
        }

        if (bits->GetFlag(row + 1, col) != 0)
        {
            return 1;
        }

        if (bits->GetFlag(row + 1, col + 1) != 0)
        {
            return 1;
        }

        return bits->GetFlag(row, col + 1) != 0 ? 1 : 0;
    }
}

auto MCMover::IsRevealed() -> int
{
    // The home side's visibility bits (the names are the original's, swapped).
    MCByteFlag* bits = Terrain()->HomeVisibleBits();
    return TileVisible(bits, ObjPosition);
}

auto MCMover::EnemyRevealed() -> int
{
    MCByteFlag* bits = HomeTeam()->Alignment != -1 ? Terrain()->ClanVisibleBits.get() : Terrain()->ISVisibleBits.get();
    return TileVisible(bits, ObjPosition);
}

auto MCMover::GetDamageClass(int32_t& damageClass, int& shutDown) -> void
{
    const double quotient = static_cast<double>(CurCV) / MaxCV;
    const float health = static_cast<float>(quotient);

    if (quotient > 0.9)
    {
        damageClass = 0;
    }
    else if (health > 0.75)
    {
        damageClass = 1;
    }
    else if (health > 0.5)
    {
        damageClass = 2;
    }
    else if (health > 0.1)
    {
        damageClass = 3;
    }
    else
    {
        damageClass = 4;
    }

    shutDown = Status == 5 ? 1 : 0;
}

auto MCMover::GetInventoryDamage(int32_t itemIndex) -> int32_t
{
    if (itemIndex >= NumAmmos + NumWeapons + NumOther)
    {
        return 0;
    }

    const MCInventoryItem& item = Inventory[itemIndex];
    return static_cast<int8_t>(MasterComponentList[item.MasterID].Health) - item.Health;
}

auto MCMover::GetEcmEffect() -> float
{
    if (Ecm != 0xff && Inventory[Ecm].Disabled == 0)
    {
        return MasterComponentList[Inventory[Ecm].MasterID].Damage;
    }

    return 0.0f;
}

auto MCMover::GetProbeEffect() -> float
{
    if (Probe != 0xff && Inventory[Probe].Disabled == 0)
    {
        return MasterComponentList[Inventory[Probe].MasterID].RangeOrHeat;
    }

    return 0.0f;
}

auto MCMover::GetVisualRange() -> float
{
    return GetProbeEffect() + MaxVisualRadius;
}

auto MCMover::GetChallenger() -> MCGameObject*
{
    MCGameObject* current = Challenger;

    if (current != nullptr && current->IsDisabled() != 0)
    {
        Challenger = nullptr;
        return nullptr;
    }

    return current;
}

auto MCMover::Disable(uint32_t cause) -> void
{
    if (IsDisabled() != 0)
    {
        return;
    }

    if (Pilot != nullptr)
    {
        Pilot->HandleAlarm(MCPilotAlarmType::VehicleIncapacitated, cause);
    }

    Status = 1;
    DisableThisFrame = 1;

    if (Alignment == HomeTeam()->Alignment)
    {
        FriendlyDestroyed = 1;
    }
    else
    {
        // An enemy mech is salvage, unless the roll (or the cause) blows it apart.
        if (MPlayer == nullptr && ObjectClass == MCObjectClass::BattleMech)
        {
            if (SalvageRoll == -999)
            {
                SalvageRoll = RollDice(MechSalvageChance);
            }

            if (cause == 3 || cause == 2)
            {
                if (SalvageRoll == 0 && CantBlowSalvage == 0)
                {
                    for (int32_t i = 0; i < NumBodyLocations(); i++)
                    {
                        DestroyBodyLocation(i);
                    }

                    Status = 2;
                }
                else
                {
                    TacticalMap()->AddSalvage(this);
                }
            }
            else if (CantBlowSalvage == 0 && SalvageRoll == 0)
            {
                for (int32_t i = 0; i < NumBodyLocations(); i++)
                {
                    DestroyBodyLocation(i);
                }

                Status = 2;
                TacticalMap()->RemoveSalvage(this, 1);
            }
        }

        EnemyDestroyed = 1;
    }

    if (SensorSystem != nullptr)
    {
        SensorSystem->Disable();
    }
}

auto MCMover::ShutDown() -> void
{
    if (IsDisabled() == 0 && Status != 5 && Status != 4)
    {
        Status = 4;
        ShutDownThisFrame = 1;
    }
}

auto MCMover::StartUp() -> void
{
    if (IsDisabled() == 0 && Status != 3 && Status != 0)
    {
        Status = 3;
        StartUpThisFrame = 1;
    }
}

auto MCMover::IsWithdrawing() -> int
{
    return Pilot->CurTacOrder.Code == MCTacticalOrderCode::Withdraw ? 1 : 0;
}

auto MCMover::GetGroupId() -> int32_t
{
    if (Group != nullptr)
    {
        return Group->GetId();
    }

    return -1;
}

auto MCMover::GetVitalInfo(void* vitalInfo) -> int32_t
{
    int32_t size = MCBigGameObject::GetVitalInfo(nullptr);
    size = static_cast<int32_t>(DebugStatus.size() + 1) + size + (static_cast<int32_t>(IconName.size() + 1) - 2) +
           (NumAmmos + NumWeapons + 9 + NumOther) * 0x1c;

    for (const int32_t criticalSpaces : NumLocationCriticalSpaces)
    {
        size += criticalSpaces * 8;
    }

    if (vitalInfo != nullptr)
    {
        MCBigGameObject::GetVitalInfo(vitalInfo);
    }

    return size;
}

auto MCMover::SetSelected(int32_t newSelected) -> void
{
    // Deselection takes a second (not for network players' movers).
    if (newSelected == 0 && NetPlayerId < 0)
    {
        DeselectTime = ScenarioTime + 1.0f;
        return;
    }

    Selected = newSelected;
    DeselectTime = 0.0f;
}
