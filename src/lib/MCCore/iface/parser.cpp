#include "stdafx.h"
#include "iface/parser.h"
#include "ai/tacordr.h"
#include "iface/iface.h"
#include "lib/aerror.h"
#include "lib/cvmath.h"
#include "network/multplyr.h"
#include "object/gameobj.h"
#include "object/group.h"
#include "object/mover.h"
#include "object/object.h"
#include "object/objque.h"
#include "sound/soundsys.h"

namespace
{
    /// <summary>A mover and its distance to the goal, as SortMoverList sorts them.</summary>
    struct MoverDistance
    {
        Mover* mover;   // +0x0
        float distance; // +0x4
    };

    /// <summary>Most jump goals SendTacOrder can hold (its stack array).</summary>
    constexpr int32_t MaxJumpGoals = 72;
}

Parser::Parser()
{
    movePath = nullptr;
    init();
}

auto Parser::init() -> void
{
    ClearSubjects();
    ClearMovePath();
}

auto Parser::AddSubject(int32_t partId, int) -> int32_t
{
    uint16_t count = numSubjects;

    if (count > 11)
    {
        return TOO_MANY_SUBJECTS;
    }

    for (int16_t i = 0; i < count; i++)
    {
        if (GetSubject(i) == partId)
        {
            return 0;
        }
    }

    subjects[count] = partId;
    numSubjects++;
    return 0;
}

auto Parser::AddSubject(MoverGroup* group, int) -> int32_t
{
    uint32_t count = numGroupSubjects;

    // Original behaviour (OB-068): full at three lances, though there are four slots.
    if (count + 1 > 3)
    {
        return TOO_MANY_SUBJECTS;
    }

    for (int16_t i = 0; i < static_cast<int32_t>(count); i++)
    {
        if (groupSubjects[i] == group)
        {
            return 0;
        }
    }

    // The lance's movers stop being single subjects.
    for (int32_t i = 0; i < group->numMovers; i++)
    {
        for (int16_t j = 0; j < numSubjects; j++)
        {
            if (GetSubject(j) == group->movers[i]->partId)
            {
                RemoveSubject(group->movers[i]->partId);
                break;
            }
        }
    }

    groupSubjects[numGroupSubjects] = group;
    numGroupSubjects++;
    return 0;
}

auto Parser::GetSubject(int16_t index) -> int32_t
{
    if (index > 11)
    {
        return 0;
    }

    return subjects[index];
}

auto Parser::IsSubject(int32_t partId) -> int
{
    for (int16_t i = 0; i < numSubjects; i++)
    {
        if (GetSubject(i) == partId)
        {
            return -1;
        }
    }

    return 0;
}

auto Parser::IsSubject(MoverGroup* group) -> int
{
    for (int16_t i = 0; i < numGroupSubjects; i++)
    {
        if (groupSubjects[i] == group)
        {
            return -1;
        }
    }

    return 0;
}

auto Parser::RemoveSubject(int32_t partId) -> void
{
    uint32_t count = numSubjects;
    int16_t index = 0;

    while (index < static_cast<int32_t>(count) && subjects[index] != partId)
    {
        index++;
    }

    if (index < static_cast<int32_t>(count))
    {
        numSubjects--;

        while (index < numSubjects)
        {
            subjects[index] = subjects[index + 1];
            index++;
        }

        if (index < 12)
        {
            subjects[index] = 0;
        }

        return;
    }

    // Not a single subject: if it is one through its lance, the lance gives way to its other movers.
    BaseObject* object = objectList->findObjectFromPart(partId);

    if (object == nullptr)
    {
        return;
    }

    if (object->objectClass != BATTLEMECH && object->objectClass != GROUNDVEHICLE && object->objectClass != ELEMENTAL &&
        object->objectClass != MOVER)
    {
        return;
    }

    MoverGroup* group = static_cast<Mover*>(object)->group;

    if (IsSubject(group) == 0)
    {
        return;
    }

    RemoveSubject(group);

    for (int16_t i = 0; i < group->numMovers; i++)
    {
        Mover* member = group->movers[i];

        if (member == nullptr || member->partId == partId || numSubjects >= 12)
        {
            continue;
        }

        // AddSubject, inlined.
        if (IsSubject(member->partId) != 0)
        {
            continue;
        }

        subjects[numSubjects] = member->partId;
        numSubjects++;
    }
}

auto Parser::RemoveSubject(MoverGroup* group) -> void
{
    uint32_t count = numGroupSubjects;
    int16_t index = 0;

    while (index < static_cast<int32_t>(count) && groupSubjects[index] != group)
    {
        index++;
    }

    if (index < static_cast<int32_t>(count))
    {
        numGroupSubjects--;
    }
    while (index < numGroupSubjects)
    {
        groupSubjects[index] = groupSubjects[index + 1];
        index++;
    }

    if (index < 4)
    {
        groupSubjects[index] = nullptr;
    }
}

auto Parser::ClearSubjects() -> void
{
    for (int32_t i = 0; i < 12; i++)
    {
        subjects[i] = 0;
    }

    numSubjects = 0;

    for (int32_t i = 0; i < 4; i++)
    {
        groupSubjects[i] = nullptr;
    }

    numGroupSubjects = 0;
}

auto Parser::AddObjectLoc(vector_3d location, int run) -> int
{
    auto* node = new (std::nothrow) LocationNode;

    if (node == nullptr)
    {
        return 0;
    }

    node->location = location;
    node->run = run;
    node->next = nullptr;

    if (movePath == nullptr)
    {
        movePath = node;
        return -1;
    }

    LocationNode* last = movePath;

    while (last->next != nullptr)
    {
        last = last->next;
    }

    last->next = node;
    return -1;
}

auto CompareDistance(const void* a, const void* b) -> int
{
    float distanceA = static_cast<const MoverDistance*>(a)->distance;
    float distanceB = static_cast<const MoverDistance*>(b)->distance;

    if (distanceA == distanceB)
    {
        return 0;
    }

    if (distanceA > distanceB)
    {
        return 1;
    }

    return -1;
}

auto SortMoverList(int32_t numMovers, Mover** movers, vector_3d goal) -> void
{
    auto* distances = static_cast<MoverDistance*>(operator new(numMovers * sizeof(MoverDistance)));
    Assert(distances != nullptr, 0, " Parser.SendTacOrder: NULL distanceList ", nullptr);

    for (int32_t i = 0; i < numMovers; i++)
    {
        Mover* mover = movers[i];

        if (mover == nullptr)
        {
            distances[i].mover = nullptr;
            distances[i].distance = std::bit_cast<float>(0x7f7fc99eu);
            continue;
        }

        distances[i].mover = mover;
        vector_3d position = mover->getPosition();
        float dx = position.x - goal.x;
        float dy = position.y - goal.y;
        float dz = position.z - goal.z;
        distances[i].distance = static_cast<float>(
            std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dz) * dz + static_cast<double>(dy) * dy));
    }

    qsort(distances, numMovers, sizeof(MoverDistance), CompareDistance);

    for (int32_t i = 0; i < numMovers; i++)
    {
        if (distances[i].mover != nullptr)
        {
            distances[i].mover->selectionIndex = i;
        }
    }

    operator delete(distances);
}

auto Parser::SendTacOrder(TacticalOrder order, int sortMovers) -> int
{
    uint16_t count = numSubjects;

    if (count == 0 && numGroupSubjects == 0)
    {
        order.destroy();
        return 0;
    }

    soundSystem->playDigitalSample(0xf, 1, nullptr, 0, 0);

    if (movePath != nullptr)
    {
        order.initWayPath(movePath);
    }

    vector_3d goal;
    goal.x = order.moveParams.wayPath.points[0];
    goal.y = order.moveParams.wayPath.points[1];
    goal.z = order.moveParams.wayPath.points[2];
    ClearMovePath();

    if (MPlayer != nullptr && MPlayer->isServer == 0)
    {
        MPlayer->sendPlayerOrder(0, &order, sortMovers, count, subjects, numGroupSubjects, groupSubjects, 0);
    }
    else
    {
        if (sortMovers != 0)
        {
            Mover* movers[12];

            for (int32_t i = 0; i < count; i++)
            {
                movers[i] = static_cast<Mover*>(objectList->findObjectFromPart(subjects[i]));
            }

            SortMoverList(count, movers, goal);
        }

        goal = order.getWayPoint(0);

        // A jump-attack (method 1) becomes a jump to the target's position.
        bool jumpToObject = false;

        if (order.code == TACTICAL_ORDER_ATTACK_OBJECT)
        {
            if (order.attackParams.method == 1)
            {
                order.code = TACTICAL_ORDER_JUMPTO_OBJECT;
                order.moveParams.wait = 0;
                order.moveParams.wayPath.mode[0] = 0;

                if (order.target != nullptr)
                {
                    order.setWayPoint(0, order.target->getPosition());
                }

                jumpToObject = true;
            }
        }
        else if (order.code == TACTICAL_ORDER_JUMPTO_OBJECT)
        {
            jumpToObject = true;
        }

        if (jumpToObject)
        {
            order.code = TACTICAL_ORDER_JUMPTO_POINT;
            Assert(order.target != nullptr, 0, " JumpToObject is NULL ", nullptr);
            order.setWayPoint(0, order.target->getPosition());
        }

        // A jump gives every mover its own landing spot around the goal.
        vector_3d jumpGoals[MaxJumpGoals];

        if (order.code == TACTICAL_ORDER_JUMPTO_POINT)
        {
            int32_t numGoals = count;

            for (int32_t i = 0; i < numGroupSubjects; i++)
            {
                numGoals += groupSubjects[i]->numMovers;
            }

            GameObject* jumpTarget = order.getJumpTarget();
            CalcJumpGoals(order.getWayPoint(0), numGoals, jumpGoals, jumpTarget);
        }

        for (int32_t i = 0; i < count; i++)
        {
            auto* mover = static_cast<Mover*>(objectList->findObjectFromPart(subjects[i]));

            if (mover == nullptr || mover == order.target)
            {
                continue;
            }

            if (sortMovers != 0)
            {
                order.selectionIndex = mover->selectionIndex;
            }

            if (order.code == TACTICAL_ORDER_JUMPTO_POINT)
            {
                order.setWayPoint(0, jumpGoals[i]);
            }

            mover->handleTacticalOrder(order, 1, 0);
        }

        order.setWayPoint(0, goal);

        int32_t goalIndex = count;

        for (int32_t i = 0; i < numGroupSubjects; i++)
        {
            MoverGroup* group = groupSubjects[i];

            if (group->numMovers == 1)
            {
                if (order.code == TACTICAL_ORDER_JUMPTO_POINT)
                {
                    order.setWayPoint(0, jumpGoals[goalIndex]);
                }

                group->getMover(0)->handleTacticalOrder(order, 1, 0);
            }
            else
            {
                vector_3d* destinations = order.code == TACTICAL_ORDER_JUMPTO_POINT ? &jumpGoals[goalIndex] : nullptr;
                group->handleTacticalOrder(order, 1, destinations, 0);
            }

            goalIndex += group->numMovers;
        }
    }

    soundSystem->playDigitalSample(0x11, 1, nullptr, 0, 0);

    if (theInterface->commandOneShot != 0)
    {
        theInterface->currentCommand = 0;
    }

    order.destroy();
    return -1;
}

auto Parser::PatrolUp() -> void
{
    if (movePath == nullptr)
    {
        return;
    }

    TacticalOrder order;
    order.init();
    order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_PATROL_PATH, 0);
    order.initWayPath(movePath);
    order.moveParams.wait = 0;
    SendTacOrder(order, -1);
    ClearMovePath();
    order.destroy();
}

auto Parser::TraverseUp() -> void
{
    if (movePath == nullptr)
    {
        return;
    }

    TacticalOrder order;
    order.init();
    order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_TRAVERSE_PATH, 0);
    order.initWayPath(movePath);
    order.moveParams.wait = -1;
    SendTacOrder(order, -1);
    ClearMovePath();
    order.destroy();
}

auto Parser::ClearMovePath() -> void
{
    while (movePath != nullptr)
    {
        LocationNode* next = movePath->next;
        delete movePath;
        movePath = next;
    }
}
