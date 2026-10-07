#include "stdafx.h"
#include "ai/tacordr.h"
#include "ai/move.h"
#include "iface/iface.h"
#include "iface/parser.h"
#include "lib/MCFatal.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "object/bldng.h"
#include "object/cmponent.h"
#include "object/elemntl.h"
#include "object/gameobj.h"
#include "object/group.h"
#include "object/gvehicl.h"
#include "object/mech.h"
#include "object/mover.h"
#include "object/objque.h"
#include "object/object.h"
#include "object/tbldng.h"
#include "object/warrior.h"
#include "sound/soundsys.h"
#include "sprite/MCMechActor.h"
#include "terrain/MCTerrain.h"
#include "terrain/MCTacticalMap.h"

namespace
{
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;

    /// <summary>The "no time" value of <see cref="MCTacticalOrder::DelayedTime"/> and lastTime.</summary>
    constexpr float NO_TIME = -1.0f;

    /// <summary>A mech, vehicle, elemental or plain mover.</summary>
    bool IsMoverClass(int32_t objectClass)
    {
        return objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
               objectClass == MOVER;
    }

    /// <summary>The order's first waypoint.</summary>
    MCVector3D FirstWayPoint(const MCTacticalOrder* order)
    {
        const float* point = order->MoveParams.WayPath.Points;
        return MCVector3D(point[0], point[1], point[2]);
    }

    /// <summary>
    /// The shared start of the delayed orders: 0 to wait (the delay isn't over), else clears the delay and
    /// returns 1.
    /// </summary>
    bool DelayOver(MCTacticalOrder* order)
    {
        if (order->DelayedTime != NO_TIME)
        {
            if (ScenarioTime < order->DelayedTime)
            {
                return false;
            }

            order->DelayedTime = NO_TIME;
        }

        return true;
    }

    /// <summary>As DelayOver, for the power orders (which test "later than -1" rather than "not -1").</summary>
    bool PowerDelayOver(MCTacticalOrder* order)
    {
        if (NO_TIME < order->DelayedTime)
        {
            if (ScenarioTime < order->DelayedTime)
            {
                return false;
            }

            order->DelayedTime = NO_TIME;
        }

        return true;
    }
}

auto MCTacticalOrder::operator=(MCTacticalOrder copy) -> void
{
    Id = copy.Id;
    DelayedTime = copy.DelayedTime;
    Time = copy.Time;
    UnitOrder = copy.UnitOrder;
    LastTime = copy.LastTime;
    Code = copy.Code;
    Origin = copy.Origin;
    MoveParams = copy.MoveParams;
    AttackParams = copy.AttackParams;
    Target = copy.Target;
    TargetObjectClass = copy.TargetObjectClass;
    SelectionIndex = copy.SelectionIndex;
    Stage = copy.Stage;
    PointLocalMoverId = copy.PointLocalMoverId;
    GroupFlags = copy.GroupFlags;
    Data[0] = copy.Data[0];
    Data[1] = copy.Data[1];
    copy.Destroy();
}

auto MCTacticalOrder::Init() -> void
{
    Time = NO_TIME;
    Id = 0;
    Origin = ORDER_ORIGIN_SELF;
    UnitOrder = 0;
    Code = TACTICAL_ORDER_NONE;
    Target = nullptr;
    TargetObjectClass = 0;
    Stage = 0;
}

auto MCTacticalOrder::Init(MCOrderOriginType origin, MCTacticalOrderCode code, int unitOrder) -> void
{
    Time = ScenarioTime;
    DelayedTime = NO_TIME;
    LastTime = NO_TIME;
    Id = 0;
    UnitOrder = unitOrder;
    Origin = origin;
    Code = code;
    MoveParams.WayPath.NumPoints = 0;
    MoveParams.WayPath.CurPoint = 0;
    MoveParams.WayPath.Points[0] = 0.0f;
    MoveParams.WayPath.Points[1] = 0.0f;
    MoveParams.WayPath.Points[2] = 0.0f;
    MoveParams.WayPath.Mode[0] = 0;
    MoveParams.FaceObject = 1;
    MoveParams.Wait = 0;
    MoveParams.Mode = 0;
    MoveParams.EscapeTile = 0;
    AttackParams.Type = 1;
    AttackParams.Method = 0;
    AttackParams.Range = -1;
    AttackParams.AimLocation = -1;
    AttackParams.Pursue = 1;
    AttackParams.Obliterate = 0;
    AttackParams.TargetPoint = MCVector3D(0.0f, 0.0f, 0.0f);
    Target = nullptr;
    TargetObjectClass = 0;
    SelectionIndex = -1;
    Stage = 1;
    PointLocalMoverId = 0xf;
    GroupFlags = 0;
}

auto MCTacticalOrder::InitWayPath(MCLocationNode* path) -> void
{
    int32_t numPoints = 0;

    for (; path != nullptr; path = path->Next)
    {
        if (numPoints == MAX_WAYPTS)
        {
            Fatal(0, " Way Path Too Long ");
        }
        else
        {
            MoveParams.WayPath.Points[numPoints * 3] = path->Location.X;
            MoveParams.WayPath.Points[numPoints * 3 + 1] = path->Location.Y;
            MoveParams.WayPath.Points[numPoints * 3 + 2] = path->Location.Z;
            MoveParams.WayPath.Mode[numPoints] = path->Run != 0 ? 1 : 0;
        }

        numPoints++;
    }

    MoveParams.WayPath.NumPoints = numPoints;
}

auto MCTacticalOrder::GetWayPoint(int32_t index) -> MCVector3D
{
    const float* point = &MoveParams.WayPath.Points[index * 3];
    return MCVector3D(point[0], point[1], point[2]);
}

auto MCTacticalOrder::SetWayPoint(int32_t index, MCVector3D wayPoint) -> void
{
    float* point = &MoveParams.WayPath.Points[index * 3];
    point[0] = wayPoint.X;
    point[1] = wayPoint.Y;
    point[2] = wayPoint.Z;
}

auto MCTacticalOrder::AddWayPoint(MCVector3D wayPoint, int32_t run) -> void
{
    const int32_t index = MoveParams.WayPath.NumPoints;

    if (index == MAX_WAYPTS)
    {
        Fatal(MAX_WAYPTS, " tacticalOrder.addWayPoint: too many! ");
    }

    float* point = &MoveParams.WayPath.Points[index * 3];
    point[0] = wayPoint.X;
    point[1] = wayPoint.Y;
    point[2] = wayPoint.Z;
    MoveParams.WayPath.Mode[index] = static_cast<uint8_t>(run);
    MoveParams.WayPath.NumPoints++;
}

auto MCTacticalOrder::GetRamTarget() -> MCGameObject*
{
    if (Code == TACTICAL_ORDER_ATTACK_OBJECT && AttackParams.Method == 2)
    {
        return Target;
    }

    return nullptr;
}

auto MCTacticalOrder::GetJumpTarget() -> MCGameObject*
{
    if (Code == TACTICAL_ORDER_JUMPTO_POINT)
    {
        return Target;
    }

    return nullptr;
}

auto MCTacticalOrder::IsGroupOrder() -> int
{
    return UnitOrder;
}

auto MCTacticalOrder::IsCombatOrder() -> int
{
    return Code == TACTICAL_ORDER_ATTACK_OBJECT || Code == TACTICAL_ORDER_ATTACK_POINT ? 1 : 0;
}

auto MCTacticalOrder::IsMoveOrder() -> int
{
    return Code == TACTICAL_ORDER_MOVETO_POINT || Code == TACTICAL_ORDER_MOVETO_OBJECT ? 1 : 0;
}

auto MCTacticalOrder::IsWayPathOrder() -> int
{
    return Code == TACTICAL_ORDER_TRAVERSE_PATH || Code == TACTICAL_ORDER_PATROL_PATH ? 1 : 0;
}

auto MCTacticalOrder::IsJumpOrder() -> int
{
    return Code == TACTICAL_ORDER_JUMPTO_POINT || Code == TACTICAL_ORDER_JUMPTO_OBJECT ? 1 : 0;
}

auto MCTacticalOrder::SetId(MCMechWarrior* pilot) -> void
{
    Assert(Id == 0 ? 1 : 0, static_cast<uint32_t>(Id), " TacticalOrder.setId: id != 0 ");
    Id = pilot->NextTacOrderId;

    if (Id == 0xff)
    {
        pilot->NextTacOrderId = 1;
    }
    else
    {
        pilot->NextTacOrderId = Id + 1;
    }
}

auto MCTacticalOrder::GetParamData(float* timeStamp, int32_t* paramList) -> int32_t
{
    if (timeStamp != nullptr)
    {
        *timeStamp = Time;
    }

    paramList[0] = Code;
    paramList[1] = Origin;
    paramList[2] = UnitOrder != 0 ? 1 : 0;
    paramList[3] = Target == nullptr ? 0 : Target->PartId;
    paramList[4] = TargetObjectClass;
    paramList[5] = 0;
    paramList[6] = SelectionIndex;

    switch (Code)
    {
        case TACTICAL_ORDER_MOVETO_POINT:
        case TACTICAL_ORDER_GUARD:
        {
            paramList[7] = MoveParams.WayPath.Mode[0] == 1 ? 1 : 0;
            paramList[8] = MoveParams.Wait;
            paramList[9] = static_cast<int32_t>(MoveParams.WayPath.Points[0]);
            paramList[10] = static_cast<int32_t>(MoveParams.WayPath.Points[1]);
            paramList[11] = static_cast<int32_t>(MoveParams.WayPath.Points[2]);
            break;
        }
        case TACTICAL_ORDER_MOVETO_OBJECT:
        {
            paramList[8] = MoveParams.Wait;
            paramList[7] = MoveParams.WayPath.Mode[0] == 1 ? 1 : 0;
            paramList[9] = MoveParams.FaceObject;
            break;
        }
        case TACTICAL_ORDER_ATTACK_OBJECT:
        {
            paramList[7] = MoveParams.WayPath.Mode[0] == 1 ? 1 : 0;
            paramList[10] = AttackParams.Range;
            paramList[9] = AttackParams.Method;
            paramList[8] = AttackParams.Type;
            paramList[12] = AttackParams.Pursue != 0 ? 1 : 0;
            paramList[11] = AttackParams.AimLocation;
            paramList[13] = AttackParams.Obliterate != 0 ? 1 : 0;
            break;
        }
        default:
            break;
    }

    return Code;
}

auto MCTacticalOrder::Pack(MCMoverGroup*, MCMover*) -> int32_t
{
    // First word: code (5 bits, 0x1f for a GUARD without target), origin, unit order, group flags, point mover,
    // attack type and method, aim location.
    uint32_t header = static_cast<uint32_t>(AttackParams.Method) << 2 | static_cast<uint32_t>(AttackParams.Type);
    header = header << 4 | static_cast<uint32_t>(static_cast<int32_t>(PointLocalMoverId));
    header = header << 12 | GroupFlags;
    header = header << 1 | (UnitOrder != 0 ? 1u : 0u);
    header = header << 2 | static_cast<uint32_t>(Origin);
    header = header << 5;
    uint32_t word0 = header | static_cast<uint32_t>(AttackParams.AimLocation + 2) << 28;

    if (Code == TACTICAL_ORDER_GUARD)
    {
        word0 |= Target == nullptr ? 0x1f : TACTICAL_ORDER_GUARD;
    }
    else
    {
        word0 |= static_cast<uint32_t>(Code);
    }

    Data[0] = word0;

    // The target (low 2 bits: 0 a mover's net roster index, 1 a terrain object part, 2 a train car part) or cell.
    auto encodeObject = [](MCGameObject* object) -> uint32_t
    {
        const int32_t objectClass = object->ObjectClass;

        if (IsMoverClass(objectClass))
        {
            return static_cast<uint32_t>(static_cast<MCMover*>(object)->NetRosterIndex) << 2;
        }

        if (objectClass == TRAINCAR)
        {
            const int32_t part = object->PartId - 0x7d000;
            const int32_t high = part / 100;
            return static_cast<uint32_t>(part - high * 100) << 2 | static_cast<uint32_t>(high) << 10 | 2;
        }

        const int32_t part = object->PartId - 0x1000;
        const int32_t block = part / 3200;
        const int32_t rest = part - block * 3200;
        return (static_cast<uint32_t>(block) << 9 | static_cast<uint32_t>(rest / 8)) << 5 |
               static_cast<uint32_t>(rest % 8) << 2 | 1;
    };

    auto encodeCell = [](MCVector3D position) -> uint32_t
    {
        int32_t cellR = 0;
        int32_t cellC = 0;
        WorldCoordToMapCell(position, cellR, cellC);
        return static_cast<uint32_t>(cellR) << 10 | static_cast<uint32_t>(cellC);
    };

    uint32_t targetBits = 0;

    switch (Code)
    {
        case TACTICAL_ORDER_MOVETO_POINT:
        case TACTICAL_ORDER_JUMPTO_POINT:
            targetBits = encodeCell(GetWayPoint(0));
            break;
        case TACTICAL_ORDER_MOVETO_OBJECT:
        case TACTICAL_ORDER_JUMPTO_OBJECT:
        case TACTICAL_ORDER_ATTACK_OBJECT:
        case TACTICAL_ORDER_CAPTURE:
        case TACTICAL_ORDER_REFIT:
        case TACTICAL_ORDER_GETFIXED:
            targetBits = encodeObject(Target);
            break;
        case TACTICAL_ORDER_GUARD:
            targetBits = Target != nullptr ? encodeObject(Target) : encodeCell(GetWayPoint(0));
            break;
        case TACTICAL_ORDER_ATTACK_POINT:
            targetBits = encodeCell(AttackParams.TargetPoint);
            break;
        default:
            break;
    }

    // Second word: the target, then the move and attack flags and the fire range.
    uint32_t word1 = targetBits << 1 | (MoveParams.WayPath.Mode[0] == 1 ? 1u : 0u);
    word1 = word1 << 1 | static_cast<uint32_t>(MoveParams.Mode);
    word1 = word1 << 1 | (MoveParams.Wait != 0 ? 1u : 0u);
    word1 = word1 << 1 | (MoveParams.FaceObject != 0 ? 1u : 0u);
    word1 = word1 << 1 | (AttackParams.Obliterate != 0 ? 1u : 0u);
    word1 = word1 << 1 | (AttackParams.Pursue != 0 ? 1u : 0u);
    Data[1] = word1 << 3 | static_cast<uint32_t>(AttackParams.Range + 4);
    return 0;
}

auto MCTacticalOrder::Unpack() -> int32_t
{
    const uint32_t word0 = Data[0];
    const uint32_t packedCode = word0 & 0x1f;
    Code = static_cast<MCTacticalOrderCode>(packedCode);

    if (packedCode == 0x1f)
    {
        Code = TACTICAL_ORDER_GUARD;
    }

    const int packedUnitOrder = static_cast<int>((static_cast<int32_t>(word0) >> 7) & 1);
    Origin = static_cast<MCOrderOriginType>((static_cast<int32_t>(word0) >> 5) & 3);
    UnitOrder = packedUnitOrder;
    Init(Origin, Code, packedUnitOrder);
    GroupFlags = (static_cast<int32_t>(word0) >> 8) & 0xfff;
    PointLocalMoverId = static_cast<char>((static_cast<int32_t>(word0) >> 20) & 0xf);
    AttackParams.Method = (static_cast<int32_t>(word0) >> 26) & 3;
    const uint32_t word1 = Data[1];
    AttackParams.Type = (static_cast<int32_t>(word0) >> 24) & 3;
    AttackParams.AimLocation = ((static_cast<int32_t>(word0) >> 28) & 0xf) - 2;
    AttackParams.Range = static_cast<int32_t>(word1 & 7) - 4;
    AttackParams.Pursue = (static_cast<int32_t>(word1) >> 3) & 1;
    AttackParams.Obliterate = (static_cast<int32_t>(word1) >> 4) & 1;
    MoveParams.FaceObject = (static_cast<int32_t>(word1) >> 5) & 1;
    MoveParams.Wait = (static_cast<int32_t>(word1) >> 6) & 1;
    MoveParams.WayPath.Mode[0] = static_cast<uint8_t>((word1 >> 8) & 1);
    MoveParams.Mode = (static_cast<int32_t>(word1) >> 7) & 1;

    const uint32_t targetBits = static_cast<uint32_t>(static_cast<int32_t>(word1) >> 9);
    const uint32_t cellRow = static_cast<uint32_t>(static_cast<int32_t>(word1) >> 0x13) & 0x3ff;
    const uint32_t objectBits = static_cast<uint32_t>(static_cast<int32_t>(word1) >> 0xb);
    auto decodeObject = [&]()
    {
        switch (targetBits & 3)
        {
            case 0:
            {
                if (MPlayer != nullptr)
                {
                    Target = MPlayer->MoverRoster[objectBits & 0x1f];
                }

                return;
            }
            case 1:
            {
                Target = static_cast<MCGameObject*>(ObjectList->FindObjectFromPart(
                    static_cast<int32_t>(objectBits & 7) + 0x1000 +
                    (static_cast<int32_t>((static_cast<int32_t>(word1) >> 0x17) & 0xff) * 400 +
                     static_cast<int32_t>((static_cast<int32_t>(word1) >> 0xe) & 0x1ff)) *
                        8));
                return;
            }
            case 2:
            {
                Target = static_cast<MCGameObject*>(
                    ObjectList->FindObjectFromPart(static_cast<int32_t>(objectBits & 0xff) +
                                                   (static_cast<int32_t>((objectBits >> 8) & 0xff) + 0x1400) * 100));
                return;
            }
            default:
                Fatal(static_cast<int32_t>(targetBits & 3), " Bad targetType ");
        }
    };

    auto decodeWayPoint = [&]()
    {
        MCVector3D position;
        MapCellToWorldPos(static_cast<int32_t>(cellRow), static_cast<int32_t>(targetBits & 0x3ff), position);
        SetWayPoint(0, position);
        MoveParams.WayPath.NumPoints = 1;
    };

    switch (Code)
    {
        case TACTICAL_ORDER_MOVETO_POINT:
        case TACTICAL_ORDER_JUMPTO_POINT:
            decodeWayPoint();
            break;
        case TACTICAL_ORDER_MOVETO_OBJECT:
        case TACTICAL_ORDER_JUMPTO_OBJECT:
        case TACTICAL_ORDER_ATTACK_OBJECT:
        case TACTICAL_ORDER_CAPTURE:
        case TACTICAL_ORDER_REFIT:
        case TACTICAL_ORDER_GETFIXED:
            decodeObject();
            break;
        case TACTICAL_ORDER_GUARD:
        {
            if (packedCode == 0x1f)
            {
                decodeWayPoint();
            }
            else
            {
                decodeObject();
            }
            break;
        }
        case TACTICAL_ORDER_ATTACK_POINT:
        {
            MapCellToWorldPos(static_cast<int32_t>(cellRow), static_cast<int32_t>(targetBits & 0x3ff),
                              AttackParams.TargetPoint);
            AttackParams.TargetPoint.Z = Terrain()->GetTerrainElevation(AttackParams.TargetPoint);
            break;
        }
        default:
            break;
    }

    if (Target != nullptr)
    {
        TargetObjectClass = Target->ObjectClass;
    }

    return 0;
}

auto MCTacticalOrder::SetGroupFlag(int32_t localMoverId, int set) -> void
{
    const uint32_t bit = 1u << (localMoverId & 0x1f);

    if (set != 0)
    {
        GroupFlags |= bit;
    }
    else
    {
        GroupFlags &= ~bit;
    }
}

auto MCTacticalOrder::GetGroup(int32_t commanderId, MCMover** moverList, MCMover** point, int32_t) -> int32_t
{
    if (MPlayer == nullptr)
    {
        return 0;
    }

    int32_t numMovers = 0;
    uint32_t flags = GroupFlags;

    for (int32_t i = 0; i < 12; i++)
    {
        if ((flags & 1) != 0)
        {
            *moverList++ = MPlayer->PlayerMoverRoster[commanderId][i];
            numMovers++;
        }

        flags = static_cast<uint32_t>(static_cast<int32_t>(flags) >> 1);
    }

    *point = nullptr;
    if (PointLocalMoverId != 0xf)
    {
        *point = MPlayer->PlayerMoverRoster[commanderId][static_cast<int32_t>(PointLocalMoverId)];
    }

    return numMovers;
}

auto MCTacticalOrder::Execute(MCMechWarrior* pilot, int32_t& message) -> int32_t
{
    int32_t result = 0;
    MCMover* vehicle = static_cast<MCMover*>(pilot->Vehicle);
    message = -1;

    // A new order ends any refit the vehicle was part of.
    if (vehicle->RefitBuddy != nullptr)
    {
        if (vehicle->ObjectClass == GROUNDVEHICLE)
        {
            static_cast<MCGroundVehicle*>(vehicle)->Refitting = 0;
        }

        MCGameObject* buddy = vehicle->RefitBuddy;

        if (buddy->ObjectClass == GROUNDVEHICLE)
        {
            static_cast<MCGroundVehicle*>(buddy)->Refitting = 0;
        }

        if (IsMoverClass(buddy->ObjectClass))
        {
            static_cast<MCMover*>(buddy)->RefitBuddy = nullptr;
        }
        else if (buddy->ObjectClass == TREEBUILDING)
        {
            static_cast<MCTreeBuilding*>(buddy)->RefitBuddy = nullptr;
        }

        vehicle->RefitBuddy = nullptr;
    }

    switch (Code)
    {
        case TACTICAL_ORDER_MOVETO_POINT:
        {
            if (!DelayOver(this))
            {
                return 0;
            }

            uint32_t params = MoveParams.WayPath.Mode[0] == 1 ? 1 : 0;

            if (MoveParams.Wait != 0)
            {
                params |= 2;
            }

            if (MoveParams.Mode == 1)
            {
                params |= 8;
            }

            if (MoveParams.EscapeTile != 0)
            {
                params |= 0x40;
            }

            result = pilot->OrderMoveToPoint(UnitOrder, 1, Origin, FirstWayPoint(this), SelectionIndex, params);
            message = -1;
            break;
        }

        case TACTICAL_ORDER_MOVETO_OBJECT:
        {
            uint32_t params = MoveParams.WayPath.Mode[0] == 1 ? 1 : 0;

            if (MoveParams.FaceObject != 0)
            {
                params |= 4;
            }

            if (MoveParams.Mode == 1)
            {
                params |= 8;
            }

            pilot->OrderMoveToObject(UnitOrder, 1, Origin, Target, SelectionIndex, params);
            message = -1;
            break;
        }

        case TACTICAL_ORDER_JUMPTO_POINT:
        {
            if (!DelayOver(this))
            {
                return 0;
            }

            result = pilot->OrderJumpToPoint(UnitOrder, 1, Origin, FirstWayPoint(this), -1);

            if (result == 0)
            {
                message = 2;
            }
            break;
        }
        case TACTICAL_ORDER_JUMPTO_OBJECT:
        {
            if (!DelayOver(this))
            {
                return 0;
            }

            if (Target == nullptr)
            {
                return 1;
            }

            result = pilot->OrderJumpToObject(UnitOrder, 1, Origin, Target, -1);

            if (result == 0)
            {
                message = 2;
            }
            break;
        }
        case TACTICAL_ORDER_TRAVERSE_PATH:
        {
            pilot->OrderTraversePath(UnitOrder, 1, Origin, &MoveParams.WayPath, MoveParams.Mode == 1 ? 8 : 0);
            message = 0;
            break;
        }
        case TACTICAL_ORDER_PATROL_PATH:
        {
            pilot->OrderPatrolPath(UnitOrder, 1, Origin, &MoveParams.WayPath);
            message = 0;
            break;
        }
        case TACTICAL_ORDER_GUARD:
        {
            message = 0;
            AttackParams.Type = 1;
            AttackParams.Method = 0;
            AttackParams.Pursue = 1;
            AttackParams.Range = -1;
            break;
        }
        case TACTICAL_ORDER_STOP:
        {
            message = 3;
            pilot->OrderStop(UnitOrder, 1);
            break;
        }
        case TACTICAL_ORDER_POWERUP:
        {
            message = 0x10;

            if (!PowerDelayOver(this))
            {
                return 0;
            }

            pilot->OrderPowerUp(UnitOrder, Origin);
            break;
        }
        case TACTICAL_ORDER_POWERDOWN:
        {
            message = 0x10;

            if (!PowerDelayOver(this))
            {
                return 0;
            }

            pilot->OrderPowerDown(UnitOrder, Origin);
            break;
        }
        case TACTICAL_ORDER_EJECT:
            pilot->OrderEject(UnitOrder, 1, Origin);
            break;
        case TACTICAL_ORDER_ATTACK_OBJECT:
        {
            result = -1;
            message = 0x1b;

            if (Target == nullptr)
            {
                pilot->SetLastTarget(nullptr, 0, 0);
                static_cast<MCMover*>(pilot->Vehicle)->CalcOptimalRange(nullptr);
                uint32_t params = AttackParams.Obliterate != 0 ? 0x20 : 0;

                if (AttackParams.Pursue != 0)
                {
                    params |= 0x10;
                }

                result = pilot->OrderAttackPoint(UnitOrder, Origin, AttackParams.TargetPoint, AttackParams.Type,
                                                 AttackParams.Method, AttackParams.Range, params);

                if (result != 0)
                {
                    break;
                }

                if (AttackParams.Range != -1 && AttackParams.Range != -4)
                {
                    message = 5;
                    break;
                }
            }
            else
            {
                MCGameObject* attackTarget = Target;

                if (attackTarget->InTransport() != 0)
                {
                    attackTarget = static_cast<MCElemental*>(attackTarget)->Transport;
                    Target = attackTarget;
                }

                static_cast<MCMover*>(pilot->Vehicle)->CalcOptimalRange(attackTarget);
                TargetObjectClass = attackTarget->ObjectClass;
                const int disabled = attackTarget->IsDisabled();
                AttackParams.Obliterate = disabled;

                if (attackTarget->IsDestroyed() != 0)
                {
                    break;
                }

                uint32_t params = disabled != 0 ? 0x20 : 0;

                if (AttackParams.Pursue != 0)
                {
                    params |= 0x10;
                }

                result =
                    pilot->OrderAttackObject(UnitOrder, Origin, attackTarget, AttackParams.Type, AttackParams.Method,
                                             AttackParams.Range, AttackParams.AimLocation, params);

                if (result != 0)
                {
                    break;
                }

                message = 4;

                if (AttackParams.Method == 2)
                {
                    message = 7;
                    break;
                }

                if (AttackParams.Method == 1)
                {
                    message = 8;
                    break;
                }

                if (AttackParams.Range != -1 && AttackParams.Range != -4)
                {
                    message = 5;
                    break;
                }

                if (AttackParams.AimLocation != -1)
                {
                    message = 9;
                    break;
                }
            }

            if (AttackParams.Pursue == 0)
            {
                message = 6;
            }
            break;
        }

        case TACTICAL_ORDER_ATTACK_POINT:
        {
            message = 0x1b;
            pilot->SetLastTarget(nullptr, 0, 0);
            static_cast<MCMover*>(pilot->Vehicle)->CalcOptimalRange(nullptr);
            uint32_t params = AttackParams.Obliterate != 0 ? 0x20 : 0;

            if (AttackParams.Pursue != 0)
            {
                params |= 0x10;
            }

            result = pilot->OrderAttackPoint(UnitOrder, Origin, AttackParams.TargetPoint, AttackParams.Type,
                                             AttackParams.Method, AttackParams.Range, params);

            if (result == 0)
            {
                if (AttackParams.Range == -1 || AttackParams.Range == -4)
                {
                    message = AttackParams.Pursue != 0 ? -1 : 6;
                }
                else
                {
                    message = 5;
                }
            }
            break;
        }

        case TACTICAL_ORDER_HOLD_FIRE:
        {
            message = 3;
            pilot->OrderWait(UnitOrder, Origin, 0, 1);
            break;
        }
        case TACTICAL_ORDER_WITHDRAW:
            pilot->OrderWithdraw(UnitOrder, Origin, FirstWayPoint(this));
            break;
        case TACTICAL_ORDER_CAPTURE:
        {
            result = 1;
            uint32_t params = MoveParams.WayPath.Mode[0] == 1 ? 1 : 0;

            if (MoveParams.FaceObject != 0)
            {
                params |= 4;
            }

            bool refuse = Target == nullptr || Target->IsCaptureable() == 0 ||
                          Target->GetAlignment() == pilot->Alignment ||
                          Target->GetCaptureBlocker(pilot->Alignment) != nullptr;

            if (!refuse && Target->IsBuilding() != 0)
            {
                // Only a vehicle with seats can empty a prison.
                int isPrison = Target->ObjectClass == BUILDING && Target->IsPrison() != 0;

                if (((Target->ObjectClass == TREEBUILDING && Target->IsPrison() != 0) || isPrison) &&
                    (vehicle->ObjectClass != GROUNDVEHICLE || static_cast<MCGroundVehicle*>(vehicle)->Seats == 0))
                {
                    refuse = true;
                }
            }

            if (refuse)
            {
                message = 0xb;
            }
            else
            {
                result = pilot->OrderMoveToObject(0, 0, Origin, Target, 0, params);

                if (result == 0)
                {
                    message = 10;
                }

                if (result != 1)
                {
                    break;
                }
            }

            Stage = 0xff;
            break;
        }

        case TACTICAL_ORDER_REFIT:
        {
            // Port fix: the original calls target->getPilot() (and drops the result) before the null check.
            if (Target != nullptr)
            {
                Target->GetPilot();
            }

            result = 1;

            if (Target == nullptr || Target->ObjectClass != BATTLEMECH ||
                static_cast<MCMover*>(Target)->NeedsRefit(0) == 0)
            {
                break;
            }

            if (vehicle->ObjectClass == GROUNDVEHICLE && vehicle->GetRefitPoints() > 0.0f &&
                vehicle->RefitBuddy == nullptr)
            {
                result = pilot->OrderMoveToObject(UnitOrder, 0, Origin, Target, SelectionIndex,
                                                  MoveParams.WayPath.Mode[0] == 1 ? 5 : 4);
                Time = 0.0f;

                if (result == 0)
                {
                    static_cast<MCMover*>(Target)->RefitBuddy = vehicle;
                    vehicle->RefitBuddy = Target;
                    message = 0xe;
                    break;
                }
            }

            Stage = 0xff;
            break;
        }

        case TACTICAL_ORDER_GETFIXED:
        {
            result = 1;

            if (Target == nullptr || Target->ObjectClass != TREEBUILDING)
            {
                break;
            }

            MCTreeBuilding* bay = static_cast<MCTreeBuilding*>(Target);

            if (bay->GetRefitPoints() > 0.0f && bay->RefitBuddy == nullptr &&
                ((vehicle->ObjectClass == BATTLEMECH && bay->MechBay == 1) ||
                 (vehicle->ObjectClass == GROUNDVEHICLE && bay->MechBay == 0)))
            {
                int32_t tileR = 0;
                int32_t tileC = 0;
                WorldCoordToMapTile(bay->GetPosition(), tileR, tileC);
                MCVector3D dest;
                MapTileCellToWorldPos(tileR, tileC, 2, 1, dest);
                result = pilot->OrderMoveToPoint(UnitOrder, 0, Origin, dest, SelectionIndex,
                                                 MoveParams.WayPath.Mode[0] == 1 ? 5 : 4);
                Time = 0.0f;

                if (result == 0)
                {
                    message = 0xe;
                    bay->RefitBuddy = vehicle;
                    vehicle->RefitBuddy = Target;
                    break;
                }
            }

            Stage = 0xff;
            break;
        }

        case TACTICAL_ORDER_LOAD_INTO_CARRIER:
        {
            result = 1;

            if (vehicle->ObjectClass == ELEMENTAL && Target != nullptr && Target->ObjectClass == GROUNDVEHICLE &&
                static_cast<MCGroundVehicle*>(Target)->ElementalCarrier != 0)
            {
                result = Target->GetPilot()->OrderMoveToObject(0, 0, Origin, vehicle, -1, 4);

                if (result == 0)
                {
                    message = 0x27;
                    break;
                }
            }

            Stage = 0xff;
            break;
        }
        case TACTICAL_ORDER_DEPLOY_ELEMENTALS:
        {
            result = 1;

            if (vehicle->ObjectClass == GROUNDVEHICLE &&
                static_cast<MCGroundVehicle*>(vehicle)->Elementals[0] != nullptr)
            {
                result = 0;
                static_cast<MCGroundVehicle*>(vehicle)->Elementals[0]->PlayMessage(
                    static_cast<MCRadioMessageType>(0x26), 0);
                Time = ScenarioTime + 5.0f;
                break;
            }

            Stage = 0xff;
            break;
        }
        default:
            break;
    }

    if (Origin == ORDER_ORIGIN_PLAYER)
    {
        pilot->TriggerAlarm(0xe, static_cast<uint32_t>(Code));
    }
    else
    {
        message = -1;
    }

    if (Code != TACTICAL_ORDER_WITHDRAW && static_cast<char>(pilot->Vehicle->Status) != 2)
    {
        static_cast<MCMover*>(pilot->Vehicle)->Withdrawing = 0;
    }

    return result;
}

auto MCTacticalOrder::Status(MCMechWarrior* pilot) -> int32_t
{
    MCTacticalOrder queued;
    queued.Init();
    const int nextIsMove = pilot->PeekQueuedTacOrder(&queued) == 0 && queued.Code == TACTICAL_ORDER_MOVETO_POINT;
    MCMover* vehicle = static_cast<MCMover*>(pilot->Vehicle);
    int32_t done = 1;

    switch (Code)
    {
        case TACTICAL_ORDER_WAIT:
        {
            if (ScenarioTime <= DelayedTime)
            {
                done = 0;
            }
            break;
        }
        case TACTICAL_ORDER_MOVETO_POINT:
        {
            if (NO_TIME < DelayedTime)
            {
                if (ScenarioTime < DelayedTime)
                {
                    queued.Destroy();
                    return 0;
                }

                DelayedTime = NO_TIME;
                int32_t message = 0;
                Execute(pilot, message);
            }

            MCVector3D point = FirstWayPoint(this);
            const double distance = vehicle->DistanceFrom(point);
            const float margin = nextIsMove ? 8.0f : MoveMarginOfError[1];

            if (distance < margin)
            {
                if (MoveParams.Wait != 0)
                {
                    Code = TACTICAL_ORDER_WAIT;
                    done = 0;
                }
                break;
            }

            done = 0;
            break;
        }

        case TACTICAL_ORDER_PATROL_PATH:
        case TACTICAL_ORDER_GUARD:
        case TACTICAL_ORDER_WAYPOINTS_DONE:
        case TACTICAL_ORDER_ATTACK_POINT:
        case TACTICAL_ORDER_HOLD_FIRE:
        case TACTICAL_ORDER_WITHDRAW:
            done = 0;
            break;
        case TACTICAL_ORDER_MOVETO_OBJECT:
        {
            if (Target == nullptr)
            {
                break;
            }

            MCVector3D targetPos = Target->GetPosition();

            if (vehicle->DistanceFrom(targetPos) > MoveMarginOfError[1])
            {
                done = 0;
                break;
            }

            if (MoveParams.FaceObject != 0)
            {
                // Done only once the target is inside the fire arc.
                const float facing = pilot->GetVehicle()->RelViewFacingTo(Target->GetPosition());
                const float fireArc = static_cast<MCMover*>(pilot->Vehicle)->GetFireArc();

                if (facing < -fireArc)
                {
                    done = 0;
                }
                else if (fireArc < facing)
                {
                    done = 0;
                }
            }
            break;
        }

        case TACTICAL_ORDER_JUMPTO_POINT:
        {
            if (Stage != 3)
            {
                done = 0;
            }
            break;
        }
        case TACTICAL_ORDER_JUMPTO_OBJECT:
        {
            if (static_cast<MCBattleMech*>(vehicle)->InJump == 0 ||
                static_cast<MCMechActor*>(vehicle->Appearance)->InJump != 0)
            {
                done = 0;
            }
            break;
        }
        case TACTICAL_ORDER_TRAVERSE_PATH:
            done = Stage == 2 ? 1 : 0;
            break;
        case TACTICAL_ORDER_STOP:
            break;
        case TACTICAL_ORDER_POWERUP:
        {
            if (NO_TIME < DelayedTime)
            {
                if (ScenarioTime < DelayedTime)
                {
                    queued.Destroy();
                    return 0;
                }

                DelayedTime = NO_TIME;
                int32_t message = 0;
                Execute(pilot, message);
            }

            if (pilot->GetVehicleStatus() != 0)
            {
                done = 0;
            }
            break;
        }
        case TACTICAL_ORDER_POWERDOWN:
        {
            if (NO_TIME < DelayedTime)
            {
                if (ScenarioTime < DelayedTime)
                {
                    queued.Destroy();
                    return 0;
                }

                DelayedTime = NO_TIME;
                int32_t message = 0;
                Execute(pilot, message);
            }

            done = 0;
            break;
        }
        case TACTICAL_ORDER_ATTACK_OBJECT:
        {
            MCGameObject* attackTarget = Target;

            if (attackTarget == nullptr)
            {
                break;
            }

            if (attackTarget->ObjectClass == ELEMENTAL && static_cast<MCElemental*>(attackTarget)->Transport != nullptr)
            {
                attackTarget = static_cast<MCElemental*>(attackTarget)->Transport;
                Target = attackTarget;
            }

            if (attackTarget->IsDestroyed() != 0)
            {
                break;
            }

            if (attackTarget->IsDisabled() != 0)
            {
                done = AttackParams.Obliterate == 0 ? 1 : 0;
            }
            else
            {
                done = 0;
            }
            break;
        }

        case TACTICAL_ORDER_SCRAMBLE:
        {
            done = 0;
            MCVector3D point = FirstWayPoint(this);
            vehicle->DistanceFrom(point);
            break;
        }

        case TACTICAL_ORDER_CAPTURE:
        {
            if (Stage == 0xff)
            {
                break;
            }

            MCGameObject* prize = Target;
            MCVector3D prizePos = prize->GetPosition();
            const auto distance = static_cast<float>(vehicle->DistanceFrom(prizePos));
            const int32_t alignment = pilot->Alignment;
            done = 0;

            if (prize->GetCaptureBlocker(alignment) != nullptr)
            {
                done = 1;
                pilot->RadioMessage(0xb, 1);
                break;
            }

            if (distance >= 30.0f || prize->IsCaptureable() == 0)
            {
                break;
            }

            switch (prize->ObjectClass)
            {
                case BATTLEMECH:
                {
                    // A seated pilot takes over the mech.
                    const MCObjectPosition* position = prize->GetObjPosition();
                    const uint32_t overlay =
                        GameMap->Map[position->TileR * GameMap->Width + position->TileC].Overlay & 0x7f;

                    if (OverlayIsBridge[overlay] != 0 || vehicle->ObjectClass != GROUNDVEHICLE)
                    {
                        break;
                    }

                    MCGroundVehicle* carrier = static_cast<MCGroundVehicle*>(vehicle);

                    for (int32_t seat = 0; seat < carrier->Seats; seat++)
                    {
                        if (carrier->Passengers[seat] == nullptr)
                        {
                            continue;
                        }

                        MCMechWarrior* newPilot = carrier->Passengers[seat];
                        carrier->Passengers[seat] = nullptr;
                        static_cast<MCMover*>(prize)->Pilot = newPilot;
                        prize->SetAwake(1);
                        TheInterface->ActivateMech(prize->PartId);
                        done = 1;
                        break;
                    }
                    break;
                }

                case GROUNDVEHICLE:
                {
                    const MCObjectPosition* position = prize->GetObjPosition();
                    const uint32_t overlay =
                        GameMap->Map[position->TileR * GameMap->Width + position->TileC].Overlay & 0x7f;

                    if (OverlayIsBridge[overlay] != 0)
                    {
                        break;
                    }

                    prize->SetCaptured();

                    if (prize->GetSalvage() != nullptr)
                    {
                        TacticalMap()->AddSalvage(prize);
                    }

                    done = 1;
                    pilot->RadioMessage(0xd, 1);
                    break;
                }

                case BUILDING:
                case TREEBUILDING:
                {
                    prize->SetCaptured();
                    prize->SetAlignment(alignment);
                    prize->SetCommanderId(vehicle->GetCommanderId());

                    if (prize->GetSalvage() != nullptr)
                    {
                        TacticalMap()->AddSalvage(prize);
                    }

                    if (prize->IsPrison() != 0 && vehicle->ObjectClass == GROUNDVEHICLE)
                    {
                        // Original behaviour (OB-031): every seat pass moves all the prisoners into that seat, so only the
                        // last prisoner is kept, in the first seat.
                        MCGroundVehicle* carrier = static_cast<MCGroundVehicle*>(vehicle);
                        MCMechWarrior** prisoners = nullptr;

                        if (prize->ObjectClass == BUILDING)
                        {
                            prisoners = static_cast<MCBuilding*>(prize)->PrisonSlots;
                        }
                        else
                        {
                            prisoners = static_cast<MCTreeBuilding*>(prize)->PrisonSlots;
                        }

                        for (int32_t seat = 0; seat < carrier->Seats; seat++)
                        {
                            for (int32_t slot = 0; slot < 4; slot++)
                            {
                                if (prisoners[slot] != nullptr)
                                {
                                    carrier->Passengers[seat] = prisoners[slot];
                                    prisoners[slot] = nullptr;
                                }
                            }
                        }
                    }

                    done = 1;
                    pilot->RadioMessage(0xc, 1);
                    break;
                }

                default:
                    break;
            }
            break;
        }

        case TACTICAL_ORDER_REFIT:
        {
            MCMover* refitee = static_cast<MCMover*>(Target);
            MCMover* refitter = vehicle;

            if (refitter->RefitBuddy == nullptr || refitee->RefitBuddy == nullptr)
            {
                break;
            }

            Assert(refitter->RefitBuddy == refitee && refitee->RefitBuddy == refitter ? 1 : 0, 0,
                   "Refitee and refitter aren't pointing at each other.");
            const uint8_t currentStage = Stage;
            done = 0;

            switch (currentStage)
            {
                case 1:
                {
                    MCVector3D refiteePos = refitee->GetPosition();

                    if (refitter->DistanceFrom(refiteePos) < RefitRange)
                    {
                        Stage = currentStage + 1;
                    }
                    break;
                }

                case 2:
                {
                    if (Time == 0.0f)
                    {
                        refitee->GetPilot()->OrderPowerDown(UnitOrder, 2);
                        Time = ScenarioTime;
                    }
                    else if (static_cast<double>(Time) + 3.0 < ScenarioTime)
                    {
                        Stage = currentStage + 1;
                        Time = ScenarioTime;
                    }
                    break;
                }
                case 3:
                {
                    MCGroundVehicle* truck = static_cast<MCGroundVehicle*>(refitter);
                    truck->Refitting = 1;

                    if (static_cast<double>(RefitTime) + Time < ScenarioTime)
                    {
                        float pointsUsed = 0.0f;
                        const int ammoOnly = truck->AmmoTruck;
                        const int32_t finished = DoRefit(refitee, refitter->GetRefitPoints(), pointsUsed, ammoOnly);
                        refitter->BurnRefitPoints(pointsUsed);

                        if (MPlayer != nullptr)
                        {
                            const int32_t shotType = -5 - (truck->AmmoTruck != 0 ? 1 : 0);
                            MCWeaponShotInfo shot;
                            shot.Init(nullptr, shotType, pointsUsed, 4, 0.0f);
                            MPlayer->AddWeaponHitChunk(refitter, &shot, 0);
                            shot.Init(nullptr, shotType, pointsUsed, 0, 0.0f);
                            MPlayer->AddWeaponHitChunk(refitee, &shot, 1);
                        }

                        Stage = static_cast<uint8_t>(currentStage + finished);

                        if (finished == 0)
                        {
                            Time = ScenarioTime;
                        }
                    }
                    break;
                }

                case 4:
                {
                    static_cast<MCGroundVehicle*>(refitter)->Refitting = 0;
                    refitee->GetPilot()->OrderPowerUp(UnitOrder, 2);
                    done = 1;
                    refitee->RefitBuddy = nullptr;
                    refitter->RefitBuddy = nullptr;
                    break;
                }
                case 0xff:
                    done = 1;
                    break;
                default:
                    break;
            }
            break;
        }

        case TACTICAL_ORDER_GETFIXED:
        {
            MCMover* mover = vehicle;
            MCTreeBuilding* bay = static_cast<MCTreeBuilding*>(Target);

            if (mover->RefitBuddy == nullptr || bay->RefitBuddy == nullptr)
            {
                break;
            }

            Assert(mover->RefitBuddy == bay && bay->RefitBuddy == mover ? 1 : 0, 0,
                   "Refitee and refitter aren't pointing at each other.");
            const float now = ScenarioTime;
            const uint8_t currentStage = Stage;
            done = 0;

            switch (currentStage)
            {
                case 1:
                case 4:
                {
                    if (pilot->GetMovePath()->NumStepsWhenNotPaused == 0 && pilot->MovePathRequest == nullptr)
                    {
                        Stage = currentStage + 1;
                    }
                    break;
                }
                case 2:
                {
                    if (Time == 0.0f)
                    {
                        pilot->OrderPowerDown(UnitOrder, 2);
                        Time = ScenarioTime;
                    }
                    else if (static_cast<double>(Time) + 3.0 < ScenarioTime)
                    {
                        Stage = currentStage + 1;
                        Time = now;
                    }
                    break;
                }
                case 3:
                {
                    if (static_cast<double>(RefitTime) + Time < ScenarioTime)
                    {
                        float pointsUsed = 0.0f;
                        const int32_t finished = DoRefit(mover, bay->GetRefitPoints(), pointsUsed, 0);
                        bay->BurnRefitPoints(pointsUsed);

                        if (MPlayer != nullptr)
                        {
                            MCWeaponShotInfo shot;
                            shot.Init(nullptr, -5, pointsUsed, -1, 0.0f);
                            MPlayer->AddWeaponHitChunk(bay, &shot, 0);
                            shot.Init(nullptr, -5, pointsUsed, 0, 0.0f);
                            MPlayer->AddWeaponHitChunk(mover, &shot, 1);
                        }

                        Stage = static_cast<uint8_t>(currentStage + finished);

                        if (finished == 0)
                        {
                            Time = ScenarioTime;
                        }
                        else
                        {
                            // Repaired: power up and roll out of the bay.
                            pilot->OrderPowerUp(UnitOrder, 2);
                            int32_t tileR = 0;
                            int32_t tileC = 0;
                            WorldCoordToMapTile(Target->GetPosition(), tileR, tileC);
                            MCVector3D exitPos;
                            MapTileCellToWorldPos(tileR + 1, tileC, 1, 1, exitPos);
                            pilot->OrderMoveToPoint(UnitOrder, 0, ORDER_ORIGIN_SELF, exitPos, SelectionIndex, 0);
                        }
                    }
                    break;
                }
                case 5:
                {
                    done = 1;
                    bay->RefitBuddy = nullptr;
                    mover->RefitBuddy = nullptr;
                    break;
                }
                case 0xff:
                    done = 1;
                    break;
                default:
                    break;
            }
            break;
        }

        case TACTICAL_ORDER_LOAD_INTO_CARRIER:
        {
            done = 0;
            MCMoverGroup* group = pilot->GetGroup();
            MCGameObject* carrier = Target;

            if (group == nullptr)
            {
                break;
            }

            const uint8_t currentStage = Stage;

            switch (currentStage)
            {
                case 1:
                {
                    const MCVector3D carrierPos = carrier->GetPosition();
                    const MCVector3D pos = pilot->Vehicle->GetPosition();
                    const double dx = static_cast<double>(pos.X) - carrierPos.X;
                    const double dy = static_cast<double>(pos.Y) - carrierPos.Y;
                    const float dz = pos.Z - carrierPos.Z;

                    if (std::sqrt((dy * dy + static_cast<double>(dz) * dz) + dx * dx) < 200.0)
                    {
                        pilot->ClearMoveOrders();

                        for (int32_t i = 0; i < group->NumMovers; i++)
                        {
                            group->Movers[i]->GetPilot()->ClearMoveOrders();
                        }

                        carrier->GetPilot()->ClearMoveOrders();
                        Time = ScenarioTime;
                        Stage = currentStage + 1;
                    }
                    break;
                }

                case 2:
                {
                    if (static_cast<double>(Time) + 3.0 < ScenarioTime)
                    {
                        for (int32_t i = 0; i < group->NumMovers; i++)
                        {
                            group->Movers[i]->GetPilot()->OrderMoveToObject(0, 0, 2, carrier, -1, 4);
                        }

                        Stage = currentStage + 1;
                    }
                    break;
                }
                case 3:
                {
                    if (ScenarioTime <= static_cast<double>(Time) + 5.0)
                    {
                        break;
                    }

                    MCGroundVehicle* transport = static_cast<MCGroundVehicle*>(carrier);

                    for (int32_t i = 0; i < group->NumMovers; i++)
                    {
                        MCMover* passenger = group->Movers[i];
                        static_cast<MCElemental*>(passenger)->Transport = carrier;

                        for (int32_t slot = 0; slot < 10; slot++)
                        {
                            if (transport->Elementals[slot] == nullptr)
                            {
                                transport->Elementals[slot] = passenger;
                                break;
                            }
                        }
                    }

                    done = 1;
                    break;
                }

                case 0xff:
                    done = 1;
                    break;
                default:
                    break;
            }
            break;
        }

        case TACTICAL_ORDER_DEPLOY_ELEMENTALS:
        {
            if (Stage == 0xff)
            {
                break;
            }

            done = 0;

            if (ScenarioTime <= static_cast<double>(Time) + 5.0)
            {
                break;
            }

            // Up to five at a time, set down in a ring around the carrier.
            int16_t toDeploy = 5;
            MCGroundVehicle* transport = static_cast<MCGroundVehicle*>(pilot->Vehicle);
            int32_t slot = 0;

            for (; slot < 10; slot++)
            {
                MCMover* elemental = transport->Elementals[slot];

                if (elemental == nullptr)
                {
                    continue;
                }

                toDeploy--;
                const double angle = static_cast<double>(toDeploy * 72) * DEGREES_TO_RADIANS;
                const float sinPart = static_cast<float>(std::sin(angle)) * 50.0f;
                const float offsetX = static_cast<float>(std::cos(angle) * 50.0 + sinPart);
                const float offsetY = static_cast<float>(std::cos(angle) * 50.0 - sinPart);
                const MCVector3D pos = transport->GetPosition();
                MCVector3D dropPos(pos.X + offsetX, pos.Y + offsetY, pos.Z);
                elemental->SetPosition(dropPos);
                static_cast<MCElemental*>(elemental)->Transport = nullptr;
                transport->Elementals[slot] = nullptr;

                if (toDeploy == 0)
                {
                    Time = ScenarioTime;
                    break;
                }
            }

            if (slot == 10)
            {
                done = 1;
            }
            break;
        }

        default:
            break;
    }

    queued.Destroy();
    return done;
}

auto MCTacticalOrder::Destroy() -> void
{
}

auto DoRefit(MCMover* mover, float refitPoints, float& pointsUsed, int ammoOnly) -> int32_t
{
    float pointsLeft = refitPoints;
    int32_t finished = 0;
    uint32_t bettySample = 0;
    bool playBetty = false;

    // Locations 4 and 5 (the arms) aren't repaired once destroyed.
    auto skipLocation = [mover](int32_t location)
    { return (location == 4 || location == 5) && mover->BodyAt(location).DamageState == 2; };

    if (refitPoints <= 0.0f)
    {
        if (mover->NetPlayerId != -1)
        {
            bettySample = 0x15;
            playBetty = true;
        }

        finished = 1;
    }
    else
    {
        const float shareBase = static_cast<float>(RefitAmount * (1.0 / 3.0));
        char locationsToFix = 0;
        char ammoToFix = 0;

        if (ammoOnly == 0)
        {
            for (char location = 0; location < mover->NumArmorLocations; location++)
            {
                if (skipLocation(location))
                {
                    continue;
                }

                if (location < mover->NumBodyLocations &&
                    mover->BodyAt(location).CurInternalStructure <
                        static_cast<float>(mover->BodyAt(location).MaxInternalStructure))
                {
                    locationsToFix++;
                }

                if (mover->Armor[location].CurArmor < static_cast<float>(mover->Armor[location].MaxArmor))
                {
                    locationsToFix++;
                }
            }
        }

        for (char i = 0; i < mover->NumAmmoTypes; i++)
        {
            if (mover->AmmoTypeTotal[i].CurAmount < mover->AmmoTypeTotal[i].StartAmount)
            {
                ammoToFix++;
            }
        }

        for (char location = 0; location < mover->NumArmorLocations; location++)
        {
            if (locationsToFix == 0 || pointsLeft <= 0.0f || skipLocation(location))
            {
                continue;
            }

            MCArmorLocation& armor = mover->Armor[location];
            const float maxArmor = static_cast<float>(armor.MaxArmor);

            if (armor.CurArmor < maxArmor)
            {
                double share = static_cast<double>(shareBase) / static_cast<int32_t>(locationsToFix);

                if (pointsLeft < share)
                {
                    share = pointsLeft;
                }

                const double added = static_cast<double>(RefitCostArray[0][0]) * share;
                float addedStored = static_cast<float>(added);

                if (maxArmor < added + armor.CurArmor)
                {
                    const double room = static_cast<double>(maxArmor) - armor.CurArmor;
                    addedStored = static_cast<float>(room);
                    share = room / RefitCostArray[0][0];
                }

                armor.CurArmor = static_cast<float>(static_cast<double>(addedStored) + armor.CurArmor);
                pointsLeft = static_cast<float>(pointsLeft - share);
            }

            if (location < mover->NumBodyLocations)
            {
                MCBodyLocation& body = mover->BodyAt(location);
                const float maxStructure = static_cast<float>(body.MaxInternalStructure);

                if (body.CurInternalStructure < maxStructure)
                {
                    const double shareRaw = static_cast<double>(shareBase) / static_cast<int32_t>(locationsToFix);
                    float share = static_cast<float>(shareRaw);

                    if (pointsLeft < shareRaw)
                    {
                        share = pointsLeft;
                    }

                    double added = static_cast<double>(RefitCostArray[1][0]) * share;

                    if (maxStructure < added + body.CurInternalStructure)
                    {
                        added = static_cast<double>(maxStructure) - body.CurInternalStructure;
                        share = static_cast<float>(added / RefitCostArray[1][0]);
                    }

                    const double newStructure = added + body.CurInternalStructure;
                    body.CurInternalStructure = static_cast<float>(newStructure);
                    uint8_t damageState;

                    if (newStructure == 0.0)
                    {
                        damageState = 2;
                    }
                    else
                    {
                        damageState = 0.5 < newStructure / maxStructure ? 0 : 1;
                    }

                    if (mover->ObjectClass == BATTLEMECH && damageState != body.DamageState)
                    {
                        if (location == 6 || location == 7)
                        {
                            static_cast<MCBattleMech*>(mover)->CalcLegStatus();
                        }

                        if (location == 1)
                        {
                            static_cast<MCBattleMech*>(mover)->CalcTorsoStatus();
                        }
                    }

                    pointsLeft = static_cast<float>(static_cast<double>(pointsLeft) - share);
                    body.DamageState = damageState;
                }
            }
        }

        for (char i = 0; i < mover->NumAmmoTypes; i++)
        {
            if (ammoToFix <= 0 || pointsLeft <= 0.0f)
            {
                continue;
            }

            MCAmmoTally& ammo = mover->AmmoTypeTotal[i];
            const int32_t curAmount = ammo.CurAmount;
            const int32_t maxAmount = ammo.StartAmount;

            if (curAmount >= maxAmount)
            {
                continue;
            }

            const int wasEmpty = curAmount == 0;
            double share = static_cast<double>(shareBase) / static_cast<int32_t>(ammoToFix);

            if (share > pointsLeft)
            {
                share = pointsLeft;
            }

            const double costPerPoint =
                static_cast<double>(MasterComponentList[ammo.MasterId].LongValue) * RefitCostArray[2][0];
            const float costPerPointStored = static_cast<float>(costPerPoint);
            float added = static_cast<float>(costPerPoint * share);
            const float current = static_cast<float>(curAmount);

            if (static_cast<double>(maxAmount) < static_cast<double>(current) + added)
            {
                added = static_cast<float>(maxAmount - curAmount);
                share = static_cast<double>(maxAmount - curAmount) / costPerPointStored;
            }

            ammo.CurAmount = static_cast<int32_t>(static_cast<double>(current) + added);
            pointsLeft = static_cast<float>(pointsLeft - share);

            if (wasEmpty)
            {
                mover->CalcLongestRangeWeapon();
                mover->CalcWeaponEffectiveness(0);
                mover->CalcOptimalRange(nullptr);
            }
        }

        // Finished once nothing is left to fix.
        bool needsMore = false;

        if (ammoOnly == 0)
        {
            for (char location = 0; location < mover->NumArmorLocations && !needsMore; location++)
            {
                if (skipLocation(location))
                {
                    continue;
                }

                if ((location < mover->NumBodyLocations &&
                     mover->BodyAt(location).CurInternalStructure <
                         static_cast<float>(mover->BodyAt(location).MaxInternalStructure)) ||
                    mover->Armor[location].CurArmor < static_cast<float>(mover->Armor[location].MaxArmor))
                {
                    needsMore = true;
                }
            }
        }

        for (char i = 0; i < mover->NumAmmoTypes && !needsMore; i++)
        {
            if (mover->AmmoTypeTotal[i].CurAmount < mover->AmmoTypeTotal[i].StartAmount)
            {
                needsMore = true;
            }
        }

        if (!needsMore)
        {
            if (mover->NetPlayerId != -1)
            {
                mover->GetPilot()->RadioMessage(0xf, 1);
                bettySample = 0x14;
                playBetty = true;
            }

            finished = 1;
        }
    }

    if (playBetty)
    {
        SoundSystem->PlayBettySample(bettySample);
    }

    // Points used, at least a quarter and rounded to quarters.
    float used = refitPoints - pointsLeft;
    pointsUsed = used;

    if (0.0f < used && used < 0.25f)
    {
        pointsUsed = 0.25f;
    }

    pointsUsed = static_cast<float>(static_cast<int32_t>((static_cast<double>(pointsUsed) + 0.125) * 4.0)) * 0.25f;
    return finished;
}
