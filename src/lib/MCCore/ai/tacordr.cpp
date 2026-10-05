#include "stdafx.h"
#include "ai/tacordr.h"
#include "ai/move.h"
#include "iface/iface.h"
#include "iface/parser.h"
#include "lib/aerror.h"
#include "lib/cvmath.h"
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
#include "sprite/mactor.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"

namespace
{
    /// <summary>Degrees to radians, as MCX.EXE stores it (MCX.EXE @ 0x0077c2a0; a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;

    /// <summary>The "no time" value of <see cref="TacticalOrder::delayedTime"/> and lastTime.</summary>
    constexpr float NO_TIME = -1.0f;

    /// <summary>A mech, vehicle, elemental or plain mover.</summary>
    bool IsMoverClass(int32_t objectClass)
    {
        return objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
               objectClass == MOVER;
    }

    /// <summary>The order's first waypoint.</summary>
    vector_3d FirstWayPoint(const TacticalOrder* order)
    {
        const float* point = order->moveParams.wayPath.points;
        return vector_3d(point[0], point[1], point[2]);
    }

    /// <summary>
    /// The shared start of the delayed orders: 0 to wait (the delay isn't over), else clears the delay and
    /// returns 1.
    /// </summary>
    bool DelayOver(TacticalOrder* order)
    {
        if (order->delayedTime != NO_TIME)
        {
            if (scenarioTime < order->delayedTime)
            {
                return false;
            }

            order->delayedTime = NO_TIME;
        }

        return true;
    }

    /// <summary>As DelayOver, for the power orders (which test "later than -1" rather than "not -1").</summary>
    bool PowerDelayOver(TacticalOrder* order)
    {
        if (NO_TIME < order->delayedTime)
        {
            if (scenarioTime < order->delayedTime)
            {
                return false;
            }

            order->delayedTime = NO_TIME;
        }

        return true;
    }
}

auto TacticalOrder::operator=(TacticalOrder copy) -> void
{
    id = copy.id;
    delayedTime = copy.delayedTime;
    time = copy.time;
    unitOrder = copy.unitOrder;
    lastTime = copy.lastTime;
    code = copy.code;
    origin = copy.origin;
    moveParams = copy.moveParams;
    attackParams = copy.attackParams;
    target = copy.target;
    targetObjectClass = copy.targetObjectClass;
    selectionIndex = copy.selectionIndex;
    stage = copy.stage;
    pointLocalMoverId = copy.pointLocalMoverId;
    groupFlags = copy.groupFlags;
    data[0] = copy.data[0];
    data[1] = copy.data[1];
    copy.destroy();
}

auto TacticalOrder::init() -> void
{
    time = NO_TIME;
    id = 0;
    origin = ORDER_ORIGIN_SELF;
    unitOrder = 0;
    code = TACTICAL_ORDER_NONE;
    target = nullptr;
    targetObjectClass = 0;
    stage = 0;
}

auto TacticalOrder::init(OrderOriginType _origin, TacticalOrderCode _code, int _unitOrder) -> void
{
    time = scenarioTime;
    delayedTime = NO_TIME;
    lastTime = NO_TIME;
    id = 0;
    unitOrder = _unitOrder;
    origin = _origin;
    code = _code;
    moveParams.wayPath.numPoints = 0;
    moveParams.wayPath.curPoint = 0;
    moveParams.wayPath.points[0] = 0.0f;
    moveParams.wayPath.points[1] = 0.0f;
    moveParams.wayPath.points[2] = 0.0f;
    moveParams.wayPath.mode[0] = 0;
    moveParams.faceObject = 1;
    moveParams.wait = 0;
    moveParams.mode = 0;
    moveParams.escapeTile = 0;
    attackParams.type = 1;
    attackParams.method = 0;
    attackParams.range = -1;
    attackParams.aimLocation = -1;
    attackParams.pursue = 1;
    attackParams.obliterate = 0;
    attackParams.targetPoint = vector_3d(0.0f, 0.0f, 0.0f);
    target = nullptr;
    targetObjectClass = 0;
    selectionIndex = -1;
    stage = 1;
    pointLocalMoverId = 0xf;
    groupFlags = 0;
}

auto TacticalOrder::initWayPath(LocationNode* path) -> void
{
    int32_t numPoints = 0;

    for (; path != nullptr; path = path->next)
    {
        if (numPoints == MAX_WAYPTS)
        {
            Fatal(0, " Way Path Too Long ");
        }
        else
        {
            moveParams.wayPath.points[numPoints * 3] = path->location.x;
            moveParams.wayPath.points[numPoints * 3 + 1] = path->location.y;
            moveParams.wayPath.points[numPoints * 3 + 2] = path->location.z;
            moveParams.wayPath.mode[numPoints] = path->run != 0 ? 1 : 0;
        }

        numPoints++;
    }

    moveParams.wayPath.numPoints = numPoints;
}

auto TacticalOrder::getWayPoint(int32_t index) -> vector_3d
{
    const float* point = &moveParams.wayPath.points[index * 3];
    return vector_3d(point[0], point[1], point[2]);
}

auto TacticalOrder::setWayPoint(int32_t index, vector_3d wayPoint) -> void
{
    float* point = &moveParams.wayPath.points[index * 3];
    point[0] = wayPoint.x;
    point[1] = wayPoint.y;
    point[2] = wayPoint.z;
}

auto TacticalOrder::addWayPoint(vector_3d wayPoint, int32_t run) -> void
{
    const int32_t index = moveParams.wayPath.numPoints;

    if (index == MAX_WAYPTS)
    {
        Fatal(MAX_WAYPTS, " tacticalOrder.addWayPoint: too many! ");
    }

    float* point = &moveParams.wayPath.points[index * 3];
    point[0] = wayPoint.x;
    point[1] = wayPoint.y;
    point[2] = wayPoint.z;
    moveParams.wayPath.mode[index] = static_cast<uint8_t>(run);
    moveParams.wayPath.numPoints++;
}

auto TacticalOrder::getRamTarget() -> GameObject*
{
    if (code == TACTICAL_ORDER_ATTACK_OBJECT && attackParams.method == 2)
    {
        return target;
    }

    return nullptr;
}

auto TacticalOrder::getJumpTarget() -> GameObject*
{
    if (code == TACTICAL_ORDER_JUMPTO_POINT)
    {
        return target;
    }

    return nullptr;
}

auto TacticalOrder::isGroupOrder() -> int
{
    return unitOrder;
}

auto TacticalOrder::isCombatOrder() -> int
{
    return code == TACTICAL_ORDER_ATTACK_OBJECT || code == TACTICAL_ORDER_ATTACK_POINT ? 1 : 0;
}

auto TacticalOrder::isMoveOrder() -> int
{
    return code == TACTICAL_ORDER_MOVETO_POINT || code == TACTICAL_ORDER_MOVETO_OBJECT ? 1 : 0;
}

auto TacticalOrder::isWayPathOrder() -> int
{
    return code == TACTICAL_ORDER_TRAVERSE_PATH || code == TACTICAL_ORDER_PATROL_PATH ? 1 : 0;
}

auto TacticalOrder::isJumpOrder() -> int
{
    return code == TACTICAL_ORDER_JUMPTO_POINT || code == TACTICAL_ORDER_JUMPTO_OBJECT ? 1 : 0;
}

auto TacticalOrder::setId(MechWarrior* pilot) -> void
{
    Assert(id == 0 ? 1 : 0, static_cast<uint32_t>(id), " TacticalOrder.setId: id != 0 ");
    id = pilot->nextTacOrderId;

    if (id == 0xff)
    {
        pilot->nextTacOrderId = 1;
    }
    else
    {
        pilot->nextTacOrderId = id + 1;
    }
}

auto TacticalOrder::getParamData(float* timeStamp, int32_t* paramList) -> int32_t
{
    if (timeStamp != nullptr)
    {
        *timeStamp = time;
    }

    paramList[0] = code;
    paramList[1] = origin;
    paramList[2] = unitOrder != 0 ? 1 : 0;
    paramList[3] = target == nullptr ? 0 : target->partId;
    paramList[4] = targetObjectClass;
    paramList[5] = 0;
    paramList[6] = selectionIndex;

    switch (code)
    {
        case TACTICAL_ORDER_MOVETO_POINT:
        case TACTICAL_ORDER_GUARD:
        {
            paramList[7] = moveParams.wayPath.mode[0] == 1 ? 1 : 0;
            paramList[8] = moveParams.wait;
            paramList[9] = static_cast<int32_t>(moveParams.wayPath.points[0]);
            paramList[10] = static_cast<int32_t>(moveParams.wayPath.points[1]);
            paramList[11] = static_cast<int32_t>(moveParams.wayPath.points[2]);
            break;
        }
        case TACTICAL_ORDER_MOVETO_OBJECT:
        {
            paramList[8] = moveParams.wait;
            paramList[7] = moveParams.wayPath.mode[0] == 1 ? 1 : 0;
            paramList[9] = moveParams.faceObject;
            break;
        }
        case TACTICAL_ORDER_ATTACK_OBJECT:
        {
            paramList[7] = moveParams.wayPath.mode[0] == 1 ? 1 : 0;
            paramList[10] = attackParams.range;
            paramList[9] = attackParams.method;
            paramList[8] = attackParams.type;
            paramList[12] = attackParams.pursue != 0 ? 1 : 0;
            paramList[11] = attackParams.aimLocation;
            paramList[13] = attackParams.obliterate != 0 ? 1 : 0;
            break;
        }
        default:
            break;
    }

    return code;
}

auto TacticalOrder::pack(MoverGroup*, Mover*) -> int32_t
{
    // First word: code (5 bits, 0x1f for a GUARD without target), origin, unit order, group flags, point mover,
    // attack type and method, aim location.
    uint32_t header = static_cast<uint32_t>(attackParams.method) << 2 | static_cast<uint32_t>(attackParams.type);
    header = header << 4 | static_cast<uint32_t>(static_cast<int32_t>(pointLocalMoverId));
    header = header << 12 | groupFlags;
    header = header << 1 | (unitOrder != 0 ? 1u : 0u);
    header = header << 2 | static_cast<uint32_t>(origin);
    header = header << 5;
    uint32_t word0 = header | static_cast<uint32_t>(attackParams.aimLocation + 2) << 28;

    if (code == TACTICAL_ORDER_GUARD)
    {
        word0 |= target == nullptr ? 0x1f : TACTICAL_ORDER_GUARD;
    }
    else
    {
        word0 |= static_cast<uint32_t>(code);
    }

    data[0] = word0;

    // The target (low 2 bits: 0 a mover's net roster index, 1 a terrain object part, 2 a train car part) or cell.
    auto encodeObject = [](GameObject* object) -> uint32_t
    {
        const int32_t objectClass = object->objectClass;

        if (IsMoverClass(objectClass))
        {
            return static_cast<uint32_t>(static_cast<Mover*>(object)->netRosterIndex) << 2;
        }

        if (objectClass == TRAINCAR)
        {
            const int32_t part = object->partId - 0x7d000;
            const int32_t high = part / 100;
            return static_cast<uint32_t>(part - high * 100) << 2 | static_cast<uint32_t>(high) << 10 | 2;
        }

        const int32_t part = object->partId - 0x1000;
        const int32_t block = part / 3200;
        const int32_t rest = part - block * 3200;
        return (static_cast<uint32_t>(block) << 9 | static_cast<uint32_t>(rest / 8)) << 5 |
               static_cast<uint32_t>(rest % 8) << 2 | 1;
    };

    auto encodeCell = [](vector_3d position) -> uint32_t
    {
        int32_t cellR = 0;
        int32_t cellC = 0;
        worldCoordToMapCell(position, cellR, cellC);
        return static_cast<uint32_t>(cellR) << 10 | static_cast<uint32_t>(cellC);
    };

    uint32_t targetBits = 0;

    switch (code)
    {
        case TACTICAL_ORDER_MOVETO_POINT:
        case TACTICAL_ORDER_JUMPTO_POINT:
            targetBits = encodeCell(getWayPoint(0));
            break;
        case TACTICAL_ORDER_MOVETO_OBJECT:
        case TACTICAL_ORDER_JUMPTO_OBJECT:
        case TACTICAL_ORDER_ATTACK_OBJECT:
        case TACTICAL_ORDER_CAPTURE:
        case TACTICAL_ORDER_REFIT:
        case TACTICAL_ORDER_GETFIXED:
            targetBits = encodeObject(target);
            break;
        case TACTICAL_ORDER_GUARD:
            targetBits = target != nullptr ? encodeObject(target) : encodeCell(getWayPoint(0));
            break;
        case TACTICAL_ORDER_ATTACK_POINT:
            targetBits = encodeCell(attackParams.targetPoint);
            break;
        default:
            break;
    }

    // Second word: the target, then the move and attack flags and the fire range.
    uint32_t word1 = targetBits << 1 | (moveParams.wayPath.mode[0] == 1 ? 1u : 0u);
    word1 = word1 << 1 | static_cast<uint32_t>(moveParams.mode);
    word1 = word1 << 1 | (moveParams.wait != 0 ? 1u : 0u);
    word1 = word1 << 1 | (moveParams.faceObject != 0 ? 1u : 0u);
    word1 = word1 << 1 | (attackParams.obliterate != 0 ? 1u : 0u);
    word1 = word1 << 1 | (attackParams.pursue != 0 ? 1u : 0u);
    data[1] = word1 << 3 | static_cast<uint32_t>(attackParams.range + 4);
    return 0;
}

auto TacticalOrder::unpack() -> int32_t
{
    const uint32_t word0 = data[0];
    const uint32_t packedCode = word0 & 0x1f;
    code = static_cast<TacticalOrderCode>(packedCode);

    if (packedCode == 0x1f)
    {
        code = TACTICAL_ORDER_GUARD;
    }

    const int packedUnitOrder = static_cast<int>((static_cast<int32_t>(word0) >> 7) & 1);
    origin = static_cast<OrderOriginType>((static_cast<int32_t>(word0) >> 5) & 3);
    unitOrder = packedUnitOrder;
    init(origin, code, packedUnitOrder);
    groupFlags = (static_cast<int32_t>(word0) >> 8) & 0xfff;
    pointLocalMoverId = static_cast<char>((static_cast<int32_t>(word0) >> 20) & 0xf);
    attackParams.method = (static_cast<int32_t>(word0) >> 26) & 3;
    const uint32_t word1 = data[1];
    attackParams.type = (static_cast<int32_t>(word0) >> 24) & 3;
    attackParams.aimLocation = ((static_cast<int32_t>(word0) >> 28) & 0xf) - 2;
    attackParams.range = static_cast<int32_t>(word1 & 7) - 4;
    attackParams.pursue = (static_cast<int32_t>(word1) >> 3) & 1;
    attackParams.obliterate = (static_cast<int32_t>(word1) >> 4) & 1;
    moveParams.faceObject = (static_cast<int32_t>(word1) >> 5) & 1;
    moveParams.wait = (static_cast<int32_t>(word1) >> 6) & 1;
    moveParams.wayPath.mode[0] = static_cast<uint8_t>((word1 >> 8) & 1);
    moveParams.mode = (static_cast<int32_t>(word1) >> 7) & 1;

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
                    target = MPlayer->moverRoster[objectBits & 0x1f];
                }

                return;
            }
            case 1:
            {
                target = static_cast<GameObject*>(objectList->findObjectFromPart(
                    static_cast<int32_t>(objectBits & 7) + 0x1000 +
                    (static_cast<int32_t>((static_cast<int32_t>(word1) >> 0x17) & 0xff) * 400 +
                     static_cast<int32_t>((static_cast<int32_t>(word1) >> 0xe) & 0x1ff)) *
                        8));
                return;
            }
            case 2:
            {
                target = static_cast<GameObject*>(
                    objectList->findObjectFromPart(static_cast<int32_t>(objectBits & 0xff) +
                                                   (static_cast<int32_t>((objectBits >> 8) & 0xff) + 0x1400) * 100));
                return;
            }
            default:
                Fatal(static_cast<int32_t>(targetBits & 3), " Bad targetType ");
        }
    };

    auto decodeWayPoint = [&]()
    {
        vector_3d position;
        mapCellToWorldPos(static_cast<int32_t>(cellRow), static_cast<int32_t>(targetBits & 0x3ff), position);
        setWayPoint(0, position);
        moveParams.wayPath.numPoints = 1;
    };

    switch (code)
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
            mapCellToWorldPos(static_cast<int32_t>(cellRow), static_cast<int32_t>(targetBits & 0x3ff),
                              attackParams.targetPoint);
            attackParams.targetPoint.z = land->getTerrainElevation(attackParams.targetPoint);
            break;
        }
        default:
            break;
    }

    if (target != nullptr)
    {
        targetObjectClass = target->objectClass;
    }

    return 0;
}

auto TacticalOrder::setGroupFlag(int32_t localMoverId, int set) -> void
{
    const uint32_t bit = 1u << (localMoverId & 0x1f);

    if (set != 0)
    {
        groupFlags |= bit;
    }
    else
    {
        groupFlags &= ~bit;
    }
}

auto TacticalOrder::getGroup(int32_t commanderId, Mover** moverList, Mover** point, int32_t) -> int32_t
{
    if (MPlayer == nullptr)
    {
        return 0;
    }

    int32_t numMovers = 0;
    uint32_t flags = groupFlags;

    for (int32_t i = 0; i < 12; i++)
    {
        if ((flags & 1) != 0)
        {
            *moverList++ = MPlayer->playerMoverRoster[commanderId][i];
            numMovers++;
        }

        flags = static_cast<uint32_t>(static_cast<int32_t>(flags) >> 1);
    }

    *point = nullptr;
    if (pointLocalMoverId != 0xf)
    {
        *point = MPlayer->playerMoverRoster[commanderId][static_cast<int32_t>(pointLocalMoverId)];
    }

    return numMovers;
}

auto TacticalOrder::execute(MechWarrior* pilot, int32_t& message) -> int32_t
{
    int32_t result = 0;
    Mover* vehicle = static_cast<Mover*>(pilot->vehicle);
    message = -1;

    // A new order ends any refit the vehicle was part of.
    if (vehicle->refitBuddy != nullptr)
    {
        if (vehicle->objectClass == GROUNDVEHICLE)
        {
            static_cast<GroundVehicle*>(vehicle)->refitting = 0;
        }

        GameObject* buddy = vehicle->refitBuddy;

        if (buddy->objectClass == GROUNDVEHICLE)
        {
            static_cast<GroundVehicle*>(buddy)->refitting = 0;
        }

        if (IsMoverClass(buddy->objectClass))
        {
            static_cast<Mover*>(buddy)->refitBuddy = nullptr;
        }
        else if (buddy->objectClass == TREEBUILDING)
        {
            static_cast<TreeBuilding*>(buddy)->refitBuddy = nullptr;
        }

        vehicle->refitBuddy = nullptr;
    }

    switch (code)
    {
        case TACTICAL_ORDER_MOVETO_POINT:
        {
            if (!DelayOver(this))
            {
                return 0;
            }

            uint32_t params = moveParams.wayPath.mode[0] == 1 ? 1 : 0;

            if (moveParams.wait != 0)
            {
                params |= 2;
            }

            if (moveParams.mode == 1)
            {
                params |= 8;
            }

            if (moveParams.escapeTile != 0)
            {
                params |= 0x40;
            }

            result = pilot->orderMoveToPoint(unitOrder, 1, origin, FirstWayPoint(this), selectionIndex, params);
            message = -1;
            break;
        }

        case TACTICAL_ORDER_MOVETO_OBJECT:
        {
            uint32_t params = moveParams.wayPath.mode[0] == 1 ? 1 : 0;

            if (moveParams.faceObject != 0)
            {
                params |= 4;
            }

            if (moveParams.mode == 1)
            {
                params |= 8;
            }

            pilot->orderMoveToObject(unitOrder, 1, origin, target, selectionIndex, params);
            message = -1;
            break;
        }

        case TACTICAL_ORDER_JUMPTO_POINT:
        {
            if (!DelayOver(this))
            {
                return 0;
            }

            result = pilot->orderJumpToPoint(unitOrder, 1, origin, FirstWayPoint(this), -1);

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

            if (target == nullptr)
            {
                return 1;
            }

            result = pilot->orderJumpToObject(unitOrder, 1, origin, target, -1);

            if (result == 0)
            {
                message = 2;
            }
            break;
        }
        case TACTICAL_ORDER_TRAVERSE_PATH:
        {
            pilot->orderTraversePath(unitOrder, 1, origin, &moveParams.wayPath, moveParams.mode == 1 ? 8 : 0);
            message = 0;
            break;
        }
        case TACTICAL_ORDER_PATROL_PATH:
        {
            pilot->orderPatrolPath(unitOrder, 1, origin, &moveParams.wayPath);
            message = 0;
            break;
        }
        case TACTICAL_ORDER_GUARD:
        {
            message = 0;
            attackParams.type = 1;
            attackParams.method = 0;
            attackParams.pursue = 1;
            attackParams.range = -1;
            break;
        }
        case TACTICAL_ORDER_STOP:
        {
            message = 3;
            pilot->orderStop(unitOrder, 1);
            break;
        }
        case TACTICAL_ORDER_POWERUP:
        {
            message = 0x10;

            if (!PowerDelayOver(this))
            {
                return 0;
            }

            pilot->orderPowerUp(unitOrder, origin);
            break;
        }
        case TACTICAL_ORDER_POWERDOWN:
        {
            message = 0x10;

            if (!PowerDelayOver(this))
            {
                return 0;
            }

            pilot->orderPowerDown(unitOrder, origin);
            break;
        }
        case TACTICAL_ORDER_EJECT:
            pilot->orderEject(unitOrder, 1, origin);
            break;
        case TACTICAL_ORDER_ATTACK_OBJECT:
        {
            result = -1;
            message = 0x1b;

            if (target == nullptr)
            {
                pilot->setLastTarget(nullptr, 0, 0);
                static_cast<Mover*>(pilot->vehicle)->calcOptimalRange(nullptr);
                uint32_t params = attackParams.obliterate != 0 ? 0x20 : 0;

                if (attackParams.pursue != 0)
                {
                    params |= 0x10;
                }

                result = pilot->orderAttackPoint(unitOrder, origin, attackParams.targetPoint, attackParams.type,
                                                 attackParams.method, attackParams.range, params);

                if (result != 0)
                {
                    break;
                }

                if (attackParams.range != -1 && attackParams.range != -4)
                {
                    message = 5;
                    break;
                }
            }
            else
            {
                GameObject* attackTarget = target;

                if (attackTarget->inTransport() != 0)
                {
                    attackTarget = static_cast<Elemental*>(attackTarget)->transport;
                    target = attackTarget;
                }

                static_cast<Mover*>(pilot->vehicle)->calcOptimalRange(attackTarget);
                targetObjectClass = attackTarget->objectClass;
                const int disabled = attackTarget->isDisabled();
                attackParams.obliterate = disabled;

                if (attackTarget->isDestroyed() != 0)
                {
                    break;
                }

                uint32_t params = disabled != 0 ? 0x20 : 0;

                if (attackParams.pursue != 0)
                {
                    params |= 0x10;
                }

                result =
                    pilot->orderAttackObject(unitOrder, origin, attackTarget, attackParams.type, attackParams.method,
                                             attackParams.range, attackParams.aimLocation, params);

                if (result != 0)
                {
                    break;
                }

                message = 4;

                if (attackParams.method == 2)
                {
                    message = 7;
                    break;
                }

                if (attackParams.method == 1)
                {
                    message = 8;
                    break;
                }

                if (attackParams.range != -1 && attackParams.range != -4)
                {
                    message = 5;
                    break;
                }

                if (attackParams.aimLocation != -1)
                {
                    message = 9;
                    break;
                }
            }

            if (attackParams.pursue == 0)
            {
                message = 6;
            }
            break;
        }

        case TACTICAL_ORDER_ATTACK_POINT:
        {
            message = 0x1b;
            pilot->setLastTarget(nullptr, 0, 0);
            static_cast<Mover*>(pilot->vehicle)->calcOptimalRange(nullptr);
            uint32_t params = attackParams.obliterate != 0 ? 0x20 : 0;

            if (attackParams.pursue != 0)
            {
                params |= 0x10;
            }

            result = pilot->orderAttackPoint(unitOrder, origin, attackParams.targetPoint, attackParams.type,
                                             attackParams.method, attackParams.range, params);

            if (result == 0)
            {
                if (attackParams.range == -1 || attackParams.range == -4)
                {
                    message = attackParams.pursue != 0 ? -1 : 6;
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
            pilot->orderWait(unitOrder, origin, 0, 1);
            break;
        }
        case TACTICAL_ORDER_WITHDRAW:
            pilot->orderWithdraw(unitOrder, origin, FirstWayPoint(this));
            break;
        case TACTICAL_ORDER_CAPTURE:
        {
            result = 1;
            uint32_t params = moveParams.wayPath.mode[0] == 1 ? 1 : 0;

            if (moveParams.faceObject != 0)
            {
                params |= 4;
            }

            bool refuse = target == nullptr || target->isCaptureable() == 0 ||
                          target->getAlignment() == pilot->alignment ||
                          target->getCaptureBlocker(pilot->alignment) != nullptr;

            if (!refuse && target->isBuilding() != 0)
            {
                // Only a vehicle with seats can empty a prison.
                int isPrison = target->objectClass == BUILDING && target->isPrison() != 0;

                if (((target->objectClass == TREEBUILDING && target->isPrison() != 0) || isPrison) &&
                    (vehicle->objectClass != GROUNDVEHICLE || static_cast<GroundVehicle*>(vehicle)->seats == 0))
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
                result = pilot->orderMoveToObject(0, 0, origin, target, 0, params);

                if (result == 0)
                {
                    message = 10;
                }

                if (result != 1)
                {
                    break;
                }
            }

            stage = 0xff;
            break;
        }

        case TACTICAL_ORDER_REFIT:
        {
            // Port fix: the original calls target->getPilot() (and drops the result) before the null check.
            if (target != nullptr)
            {
                target->getPilot();
            }

            result = 1;

            if (target == nullptr || target->objectClass != BATTLEMECH ||
                static_cast<Mover*>(target)->needsRefit(0) == 0)
            {
                break;
            }

            if (vehicle->objectClass == GROUNDVEHICLE && vehicle->getRefitPoints() > 0.0f &&
                vehicle->refitBuddy == nullptr)
            {
                result = pilot->orderMoveToObject(unitOrder, 0, origin, target, selectionIndex,
                                                  moveParams.wayPath.mode[0] == 1 ? 5 : 4);
                time = 0.0f;

                if (result == 0)
                {
                    static_cast<Mover*>(target)->refitBuddy = vehicle;
                    vehicle->refitBuddy = target;
                    message = 0xe;
                    break;
                }
            }

            stage = 0xff;
            break;
        }

        case TACTICAL_ORDER_GETFIXED:
        {
            result = 1;

            if (target == nullptr || target->objectClass != TREEBUILDING)
            {
                break;
            }

            TreeBuilding* bay = static_cast<TreeBuilding*>(target);

            if (bay->getRefitPoints() > 0.0f && bay->refitBuddy == nullptr &&
                ((vehicle->objectClass == BATTLEMECH && bay->mechBay == 1) ||
                 (vehicle->objectClass == GROUNDVEHICLE && bay->mechBay == 0)))
            {
                int32_t tileR = 0;
                int32_t tileC = 0;
                worldCoordToMapTile(bay->getPosition(), tileR, tileC);
                vector_3d dest;
                mapTileCellToWorldPos(tileR, tileC, 2, 1, dest);
                result = pilot->orderMoveToPoint(unitOrder, 0, origin, dest, selectionIndex,
                                                 moveParams.wayPath.mode[0] == 1 ? 5 : 4);
                time = 0.0f;

                if (result == 0)
                {
                    message = 0xe;
                    bay->refitBuddy = vehicle;
                    vehicle->refitBuddy = target;
                    break;
                }
            }

            stage = 0xff;
            break;
        }

        case TACTICAL_ORDER_LOAD_INTO_CARRIER:
        {
            result = 1;

            if (vehicle->objectClass == ELEMENTAL && target != nullptr && target->objectClass == GROUNDVEHICLE &&
                static_cast<GroundVehicle*>(target)->elementalCarrier != 0)
            {
                result = target->getPilot()->orderMoveToObject(0, 0, origin, vehicle, -1, 4);

                if (result == 0)
                {
                    message = 0x27;
                    break;
                }
            }

            stage = 0xff;
            break;
        }
        case TACTICAL_ORDER_DEPLOY_ELEMENTALS:
        {
            result = 1;

            if (vehicle->objectClass == GROUNDVEHICLE && static_cast<GroundVehicle*>(vehicle)->elementals[0] != nullptr)
            {
                result = 0;
                static_cast<GroundVehicle*>(vehicle)->elementals[0]->playMessage(static_cast<RadioMessageType>(0x26),
                                                                                 0);
                time = scenarioTime + 5.0f;
                break;
            }

            stage = 0xff;
            break;
        }
        default:
            break;
    }

    if (origin == ORDER_ORIGIN_PLAYER)
    {
        pilot->triggerAlarm(0xe, static_cast<uint32_t>(code));
    }
    else
    {
        message = -1;
    }

    if (code != TACTICAL_ORDER_WITHDRAW && static_cast<char>(pilot->vehicle->status) != 2)
    {
        static_cast<Mover*>(pilot->vehicle)->withdrawing = 0;
    }

    return result;
}

auto TacticalOrder::status(MechWarrior* pilot) -> int32_t
{
    TacticalOrder queued;
    queued.init();
    const int nextIsMove = pilot->peekQueuedTacOrder(&queued) == 0 && queued.code == TACTICAL_ORDER_MOVETO_POINT;
    Mover* vehicle = static_cast<Mover*>(pilot->vehicle);
    int32_t done = 1;

    switch (code)
    {
        case TACTICAL_ORDER_WAIT:
        {
            if (scenarioTime <= delayedTime)
            {
                done = 0;
            }
            break;
        }
        case TACTICAL_ORDER_MOVETO_POINT:
        {
            if (NO_TIME < delayedTime)
            {
                if (scenarioTime < delayedTime)
                {
                    queued.destroy();
                    return 0;
                }

                delayedTime = NO_TIME;
                int32_t message = 0;
                execute(pilot, message);
            }

            vector_3d point = FirstWayPoint(this);
            const double distance = vehicle->distanceFrom(point);
            const float margin = nextIsMove ? 8.0f : MoveMarginOfError[1];

            if (distance < margin)
            {
                if (moveParams.wait != 0)
                {
                    code = TACTICAL_ORDER_WAIT;
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
            if (target == nullptr)
            {
                break;
            }

            vector_3d targetPos = target->getPosition();

            if (vehicle->distanceFrom(targetPos) > MoveMarginOfError[1])
            {
                done = 0;
                break;
            }

            if (moveParams.faceObject != 0)
            {
                // Done only once the target is inside the fire arc.
                const float facing = pilot->getVehicle()->relViewFacingTo(target->getPosition());
                const float fireArc = static_cast<Mover*>(pilot->vehicle)->getFireArc();

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
            if (stage != 3)
            {
                done = 0;
            }
            break;
        }
        case TACTICAL_ORDER_JUMPTO_OBJECT:
        {
            if (static_cast<BattleMech*>(vehicle)->inJump == 0 ||
                static_cast<MechActor*>(vehicle->appearance)->inJump != 0)
            {
                done = 0;
            }
            break;
        }
        case TACTICAL_ORDER_TRAVERSE_PATH:
            done = stage == 2 ? 1 : 0;
            break;
        case TACTICAL_ORDER_STOP:
            break;
        case TACTICAL_ORDER_POWERUP:
        {
            if (NO_TIME < delayedTime)
            {
                if (scenarioTime < delayedTime)
                {
                    queued.destroy();
                    return 0;
                }

                delayedTime = NO_TIME;
                int32_t message = 0;
                execute(pilot, message);
            }

            if (pilot->getVehicleStatus() != 0)
            {
                done = 0;
            }
            break;
        }
        case TACTICAL_ORDER_POWERDOWN:
        {
            if (NO_TIME < delayedTime)
            {
                if (scenarioTime < delayedTime)
                {
                    queued.destroy();
                    return 0;
                }

                delayedTime = NO_TIME;
                int32_t message = 0;
                execute(pilot, message);
            }

            done = 0;
            break;
        }
        case TACTICAL_ORDER_ATTACK_OBJECT:
        {
            GameObject* attackTarget = target;

            if (attackTarget == nullptr)
            {
                break;
            }

            if (attackTarget->objectClass == ELEMENTAL && static_cast<Elemental*>(attackTarget)->transport != nullptr)
            {
                attackTarget = static_cast<Elemental*>(attackTarget)->transport;
                target = attackTarget;
            }

            if (attackTarget->isDestroyed() != 0)
            {
                break;
            }

            if (attackTarget->isDisabled() != 0)
            {
                done = attackParams.obliterate == 0 ? 1 : 0;
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
            vector_3d point = FirstWayPoint(this);
            vehicle->distanceFrom(point);
            break;
        }

        case TACTICAL_ORDER_CAPTURE:
        {
            if (stage == 0xff)
            {
                break;
            }

            GameObject* prize = target;
            vector_3d prizePos = prize->getPosition();
            const auto distance = static_cast<float>(vehicle->distanceFrom(prizePos));
            const int32_t alignment = pilot->alignment;
            done = 0;

            if (prize->getCaptureBlocker(alignment) != nullptr)
            {
                done = 1;
                pilot->radioMessage(0xb, 1);
                break;
            }

            if (distance >= 30.0f || prize->isCaptureable() == 0)
            {
                break;
            }

            switch (prize->objectClass)
            {
                case BATTLEMECH:
                {
                    // A seated pilot takes over the mech.
                    const ObjectPosition* position = prize->getObjPosition();
                    const uint32_t overlay =
                        GameMap->map[position->tileR * GameMap->width + position->tileC].overlay & 0x7f;

                    if (OverlayIsBridge[overlay] != 0 || vehicle->objectClass != GROUNDVEHICLE)
                    {
                        break;
                    }

                    GroundVehicle* carrier = static_cast<GroundVehicle*>(vehicle);

                    for (int32_t seat = 0; seat < carrier->seats; seat++)
                    {
                        if (carrier->passengers[seat] == nullptr)
                        {
                            continue;
                        }

                        MechWarrior* newPilot = carrier->passengers[seat];
                        carrier->passengers[seat] = nullptr;
                        static_cast<Mover*>(prize)->pilot = newPilot;
                        prize->setAwake(1);
                        theInterface->ActivateMech(prize->partId);
                        done = 1;
                        break;
                    }
                    break;
                }

                case GROUNDVEHICLE:
                {
                    const ObjectPosition* position = prize->getObjPosition();
                    const uint32_t overlay =
                        GameMap->map[position->tileR * GameMap->width + position->tileC].overlay & 0x7f;

                    if (OverlayIsBridge[overlay] != 0)
                    {
                        break;
                    }

                    prize->setCaptured();

                    if (prize->getSalvage() != nullptr)
                    {
                        Terrain::terrainTacticalMap->AddSalvage(prize);
                    }

                    done = 1;
                    pilot->radioMessage(0xd, 1);
                    break;
                }

                case BUILDING:
                case TREEBUILDING:
                {
                    prize->setCaptured();
                    prize->setAlignment(alignment);
                    prize->setCommanderId(vehicle->getCommanderId());

                    if (prize->getSalvage() != nullptr)
                    {
                        Terrain::terrainTacticalMap->AddSalvage(prize);
                    }

                    if (prize->isPrison() != 0 && vehicle->objectClass == GROUNDVEHICLE)
                    {
                        // Original behaviour (OB-031): every seat pass moves all the prisoners into that seat, so only the
                        // last prisoner is kept, in the first seat.
                        GroundVehicle* carrier = static_cast<GroundVehicle*>(vehicle);
                        MechWarrior** prisoners = nullptr;

                        if (prize->objectClass == BUILDING)
                        {
                            prisoners = static_cast<Building*>(prize)->prisonSlots;
                        }
                        else
                        {
                            prisoners = static_cast<TreeBuilding*>(prize)->prisonSlots;
                        }

                        for (int32_t seat = 0; seat < carrier->seats; seat++)
                        {
                            for (int32_t slot = 0; slot < 4; slot++)
                            {
                                if (prisoners[slot] != nullptr)
                                {
                                    carrier->passengers[seat] = prisoners[slot];
                                    prisoners[slot] = nullptr;
                                }
                            }
                        }
                    }

                    done = 1;
                    pilot->radioMessage(0xc, 1);
                    break;
                }

                default:
                    break;
            }
            break;
        }

        case TACTICAL_ORDER_REFIT:
        {
            Mover* refitee = static_cast<Mover*>(target);
            Mover* refitter = vehicle;

            if (refitter->refitBuddy == nullptr || refitee->refitBuddy == nullptr)
            {
                break;
            }

            Assert(refitter->refitBuddy == refitee && refitee->refitBuddy == refitter ? 1 : 0, 0,
                   "Refitee and refitter aren't pointing at each other.");
            const uint8_t currentStage = stage;
            done = 0;

            switch (currentStage)
            {
                case 1:
                {
                    vector_3d refiteePos = refitee->getPosition();

                    if (refitter->distanceFrom(refiteePos) < RefitRange)
                    {
                        stage = currentStage + 1;
                    }
                    break;
                }

                case 2:
                {
                    if (time == 0.0f)
                    {
                        refitee->getPilot()->orderPowerDown(unitOrder, 2);
                        time = scenarioTime;
                    }
                    else if (static_cast<double>(time) + 3.0 < scenarioTime)
                    {
                        stage = currentStage + 1;
                        time = scenarioTime;
                    }
                    break;
                }
                case 3:
                {
                    GroundVehicle* truck = static_cast<GroundVehicle*>(refitter);
                    truck->refitting = 1;

                    if (static_cast<double>(RefitTime) + time < scenarioTime)
                    {
                        float pointsUsed = 0.0f;
                        const int ammoOnly = truck->ammoTruck;
                        const int32_t finished = DoRefit(refitee, refitter->getRefitPoints(), pointsUsed, ammoOnly);
                        refitter->burnRefitPoints(pointsUsed);

                        if (MPlayer != nullptr)
                        {
                            const int32_t shotType = -5 - (truck->ammoTruck != 0 ? 1 : 0);
                            _WeaponShotInfo shot;
                            shot.init(nullptr, shotType, pointsUsed, 4, 0.0f);
                            MPlayer->addWeaponHitChunk(refitter, &shot, 0);
                            shot.init(nullptr, shotType, pointsUsed, 0, 0.0f);
                            MPlayer->addWeaponHitChunk(refitee, &shot, 1);
                        }

                        stage = static_cast<uint8_t>(currentStage + finished);

                        if (finished == 0)
                        {
                            time = scenarioTime;
                        }
                    }
                    break;
                }

                case 4:
                {
                    static_cast<GroundVehicle*>(refitter)->refitting = 0;
                    refitee->getPilot()->orderPowerUp(unitOrder, 2);
                    done = 1;
                    refitee->refitBuddy = nullptr;
                    refitter->refitBuddy = nullptr;
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
            Mover* mover = vehicle;
            TreeBuilding* bay = static_cast<TreeBuilding*>(target);

            if (mover->refitBuddy == nullptr || bay->refitBuddy == nullptr)
            {
                break;
            }

            Assert(mover->refitBuddy == bay && bay->refitBuddy == mover ? 1 : 0, 0,
                   "Refitee and refitter aren't pointing at each other.");
            const float now = scenarioTime;
            const uint8_t currentStage = stage;
            done = 0;

            switch (currentStage)
            {
                case 1:
                case 4:
                {
                    if (pilot->getMovePath()->numStepsWhenNotPaused == 0 && pilot->movePathRequest == nullptr)
                    {
                        stage = currentStage + 1;
                    }
                    break;
                }
                case 2:
                {
                    if (time == 0.0f)
                    {
                        pilot->orderPowerDown(unitOrder, 2);
                        time = scenarioTime;
                    }
                    else if (static_cast<double>(time) + 3.0 < scenarioTime)
                    {
                        stage = currentStage + 1;
                        time = now;
                    }
                    break;
                }
                case 3:
                {
                    if (static_cast<double>(RefitTime) + time < scenarioTime)
                    {
                        float pointsUsed = 0.0f;
                        const int32_t finished = DoRefit(mover, bay->getRefitPoints(), pointsUsed, 0);
                        bay->burnRefitPoints(pointsUsed);

                        if (MPlayer != nullptr)
                        {
                            _WeaponShotInfo shot;
                            shot.init(nullptr, -5, pointsUsed, -1, 0.0f);
                            MPlayer->addWeaponHitChunk(bay, &shot, 0);
                            shot.init(nullptr, -5, pointsUsed, 0, 0.0f);
                            MPlayer->addWeaponHitChunk(mover, &shot, 1);
                        }

                        stage = static_cast<uint8_t>(currentStage + finished);

                        if (finished == 0)
                        {
                            time = scenarioTime;
                        }
                        else
                        {
                            // Repaired: power up and roll out of the bay.
                            pilot->orderPowerUp(unitOrder, 2);
                            int32_t tileR = 0;
                            int32_t tileC = 0;
                            worldCoordToMapTile(target->getPosition(), tileR, tileC);
                            vector_3d exitPos;
                            mapTileCellToWorldPos(tileR + 1, tileC, 1, 1, exitPos);
                            pilot->orderMoveToPoint(unitOrder, 0, ORDER_ORIGIN_SELF, exitPos, selectionIndex, 0);
                        }
                    }
                    break;
                }
                case 5:
                {
                    done = 1;
                    bay->refitBuddy = nullptr;
                    mover->refitBuddy = nullptr;
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
            MoverGroup* group = pilot->getGroup();
            GameObject* carrier = target;

            if (group == nullptr)
            {
                break;
            }

            const uint8_t currentStage = stage;

            switch (currentStage)
            {
                case 1:
                {
                    const vector_3d carrierPos = carrier->getPosition();
                    const vector_3d pos = pilot->vehicle->getPosition();
                    const double dx = static_cast<double>(pos.x) - carrierPos.x;
                    const double dy = static_cast<double>(pos.y) - carrierPos.y;
                    const float dz = pos.z - carrierPos.z;

                    if (std::sqrt((dy * dy + static_cast<double>(dz) * dz) + dx * dx) < 200.0)
                    {
                        pilot->clearMoveOrders();

                        for (int32_t i = 0; i < group->numMovers; i++)
                        {
                            group->movers[i]->getPilot()->clearMoveOrders();
                        }

                        carrier->getPilot()->clearMoveOrders();
                        time = scenarioTime;
                        stage = currentStage + 1;
                    }
                    break;
                }

                case 2:
                {
                    if (static_cast<double>(time) + 3.0 < scenarioTime)
                    {
                        for (int32_t i = 0; i < group->numMovers; i++)
                        {
                            group->movers[i]->getPilot()->orderMoveToObject(0, 0, 2, carrier, -1, 4);
                        }

                        stage = currentStage + 1;
                    }
                    break;
                }
                case 3:
                {
                    if (scenarioTime <= static_cast<double>(time) + 5.0)
                    {
                        break;
                    }

                    GroundVehicle* transport = static_cast<GroundVehicle*>(carrier);

                    for (int32_t i = 0; i < group->numMovers; i++)
                    {
                        Mover* passenger = group->movers[i];
                        static_cast<Elemental*>(passenger)->transport = carrier;

                        for (int32_t slot = 0; slot < 10; slot++)
                        {
                            if (transport->elementals[slot] == nullptr)
                            {
                                transport->elementals[slot] = passenger;
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
            if (stage == 0xff)
            {
                break;
            }

            done = 0;

            if (scenarioTime <= static_cast<double>(time) + 5.0)
            {
                break;
            }

            // Up to five at a time, set down in a ring around the carrier.
            int16_t toDeploy = 5;
            GroundVehicle* transport = static_cast<GroundVehicle*>(pilot->vehicle);
            int32_t slot = 0;

            for (; slot < 10; slot++)
            {
                Mover* elemental = transport->elementals[slot];

                if (elemental == nullptr)
                {
                    continue;
                }

                toDeploy--;
                const double angle = static_cast<double>(toDeploy * 72) * DEGREES_TO_RADIANS;
                const float sinPart = static_cast<float>(std::sin(angle)) * 50.0f;
                const float offsetX = static_cast<float>(std::cos(angle) * 50.0 + sinPart);
                const float offsetY = static_cast<float>(std::cos(angle) * 50.0 - sinPart);
                const vector_3d pos = transport->getPosition();
                vector_3d dropPos(pos.x + offsetX, pos.y + offsetY, pos.z);
                elemental->setPosition(dropPos);
                static_cast<Elemental*>(elemental)->transport = nullptr;
                transport->elementals[slot] = nullptr;

                if (toDeploy == 0)
                {
                    time = scenarioTime;
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

    queued.destroy();
    return done;
}

auto TacticalOrder::destroy() -> void
{
}

auto DoRefit(Mover* mover, float refitPoints, float& pointsUsed, int ammoOnly) -> int32_t
{
    float pointsLeft = refitPoints;
    int32_t finished = 0;
    uint32_t bettySample = 0;
    bool playBetty = false;

    // Locations 4 and 5 (the arms) aren't repaired once destroyed.
    auto skipLocation = [mover](int32_t location)
    { return (location == 4 || location == 5) && mover->bodyAt(location).damageState == 2; };

    if (refitPoints <= 0.0f)
    {
        if (mover->netPlayerId != -1)
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
            for (char location = 0; location < mover->numArmorLocations; location++)
            {
                if (skipLocation(location))
                {
                    continue;
                }

                if (location < mover->numBodyLocations &&
                    mover->bodyAt(location).curInternalStructure <
                        static_cast<float>(mover->bodyAt(location).maxInternalStructure))
                {
                    locationsToFix++;
                }

                if (mover->armor[location].curArmor < static_cast<float>(mover->armor[location].maxArmor))
                {
                    locationsToFix++;
                }
            }
        }

        for (char i = 0; i < mover->numAmmoTypes; i++)
        {
            if (mover->ammoTypeTotal[i].curAmount < mover->ammoTypeTotal[i].startAmount)
            {
                ammoToFix++;
            }
        }

        for (char location = 0; location < mover->numArmorLocations; location++)
        {
            if (locationsToFix == 0 || pointsLeft <= 0.0f || skipLocation(location))
            {
                continue;
            }

            ArmorLocation& armor = mover->armor[location];
            const float maxArmor = static_cast<float>(armor.maxArmor);

            if (armor.curArmor < maxArmor)
            {
                double share = static_cast<double>(shareBase) / static_cast<int32_t>(locationsToFix);

                if (pointsLeft < share)
                {
                    share = pointsLeft;
                }

                const double added = static_cast<double>(RefitCostArray[0][0]) * share;
                float addedStored = static_cast<float>(added);

                if (maxArmor < added + armor.curArmor)
                {
                    const double room = static_cast<double>(maxArmor) - armor.curArmor;
                    addedStored = static_cast<float>(room);
                    share = room / RefitCostArray[0][0];
                }

                armor.curArmor = static_cast<float>(static_cast<double>(addedStored) + armor.curArmor);
                pointsLeft = static_cast<float>(pointsLeft - share);
            }

            if (location < mover->numBodyLocations)
            {
                BodyLocation& body = mover->bodyAt(location);
                const float maxStructure = static_cast<float>(body.maxInternalStructure);

                if (body.curInternalStructure < maxStructure)
                {
                    const double shareRaw = static_cast<double>(shareBase) / static_cast<int32_t>(locationsToFix);
                    float share = static_cast<float>(shareRaw);

                    if (pointsLeft < shareRaw)
                    {
                        share = pointsLeft;
                    }

                    double added = static_cast<double>(RefitCostArray[1][0]) * share;

                    if (maxStructure < added + body.curInternalStructure)
                    {
                        added = static_cast<double>(maxStructure) - body.curInternalStructure;
                        share = static_cast<float>(added / RefitCostArray[1][0]);
                    }

                    const double newStructure = added + body.curInternalStructure;
                    body.curInternalStructure = static_cast<float>(newStructure);
                    uint8_t damageState;

                    if (newStructure == 0.0)
                    {
                        damageState = 2;
                    }
                    else
                    {
                        damageState = 0.5 < newStructure / maxStructure ? 0 : 1;
                    }

                    if (mover->objectClass == BATTLEMECH && damageState != body.damageState)
                    {
                        if (location == 6 || location == 7)
                        {
                            static_cast<BattleMech*>(mover)->calcLegStatus();
                        }

                        if (location == 1)
                        {
                            static_cast<BattleMech*>(mover)->calcTorsoStatus();
                        }
                    }

                    pointsLeft = static_cast<float>(static_cast<double>(pointsLeft) - share);
                    body.damageState = damageState;
                }
            }
        }

        for (char i = 0; i < mover->numAmmoTypes; i++)
        {
            if (ammoToFix <= 0 || pointsLeft <= 0.0f)
            {
                continue;
            }

            AmmoTally& ammo = mover->ammoTypeTotal[i];
            const int32_t curAmount = ammo.curAmount;
            const int32_t maxAmount = ammo.startAmount;

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
                static_cast<double>(MasterComponentList[ammo.masterId].longValue) * RefitCostArray[2][0];
            const float costPerPointStored = static_cast<float>(costPerPoint);
            float added = static_cast<float>(costPerPoint * share);
            const float current = static_cast<float>(curAmount);

            if (static_cast<double>(maxAmount) < static_cast<double>(current) + added)
            {
                added = static_cast<float>(maxAmount - curAmount);
                share = static_cast<double>(maxAmount - curAmount) / costPerPointStored;
            }

            ammo.curAmount = static_cast<int32_t>(static_cast<double>(current) + added);
            pointsLeft = static_cast<float>(pointsLeft - share);

            if (wasEmpty)
            {
                mover->calcLongestRangeWeapon();
                mover->calcWeaponEffectiveness(0);
                mover->calcOptimalRange(nullptr);
            }
        }

        // Finished once nothing is left to fix.
        bool needsMore = false;

        if (ammoOnly == 0)
        {
            for (char location = 0; location < mover->numArmorLocations && !needsMore; location++)
            {
                if (skipLocation(location))
                {
                    continue;
                }

                if ((location < mover->numBodyLocations &&
                     mover->bodyAt(location).curInternalStructure <
                         static_cast<float>(mover->bodyAt(location).maxInternalStructure)) ||
                    mover->armor[location].curArmor < static_cast<float>(mover->armor[location].maxArmor))
                {
                    needsMore = true;
                }
            }
        }

        for (char i = 0; i < mover->numAmmoTypes && !needsMore; i++)
        {
            if (mover->ammoTypeTotal[i].curAmount < mover->ammoTypeTotal[i].startAmount)
            {
                needsMore = true;
            }
        }

        if (!needsMore)
        {
            if (mover->netPlayerId != -1)
            {
                mover->getPilot()->radioMessage(0xf, 1);
                bettySample = 0x14;
                playBetty = true;
            }

            finished = 1;
        }
    }

    if (playBetty)
    {
        soundSystem->playBettySample(bettySample);
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
