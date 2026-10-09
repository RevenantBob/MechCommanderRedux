#include "stdafx.h"
#include "iface/parser.h"
#include "ai/MCTacticalOrder.h"
#include "iface/iface.h"
#include "lib/MCFatal.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "network/multplyr.h"
#include "object/MCBigGameObject.h"
#include "object/MCMoverGroup.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectQueue.h"
#include "sound/soundsys.h"

namespace
{
    /// <summary>A mover and its distance to the goal, as SortMoverList sorts them.</summary>
    struct MCMoverDistance
    {
        MCMover* Mover = nullptr;
        float Distance = 0;
    };

    /// <summary>Most jump goals SendTacOrder can hold (its stack array).</summary>
    constexpr int32_t MaxJumpGoals = 72;
}

MCParser::MCParser()
{
    MovePath = nullptr;
    Init();
}

auto MCParser::Init() -> void
{
    ClearSubjects();
    ClearMovePath();
}

auto MCParser::AddSubject(int32_t partId, int) -> int32_t
{
    uint16_t count = NumSubjects;

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

    Subjects[count] = partId;
    NumSubjects++;
    return 0;
}

auto MCParser::AddSubject(MCMoverGroup* group, int) -> int32_t
{
    uint32_t count = NumGroupSubjects;

    // Original behaviour (OB-068): full at three lances, though there are four slots.
    if (count + 1 > 3)
    {
        return TOO_MANY_SUBJECTS;
    }

    for (int16_t i = 0; i < static_cast<int32_t>(count); i++)
    {
        if (GroupSubjects[i] == group)
        {
            return 0;
        }
    }

    // The lance's movers stop being single subjects.
    for (int32_t i = 0; i < group->NumMovers(); i++)
    {
        for (int16_t j = 0; j < NumSubjects; j++)
        {
            if (GetSubject(j) == group->Movers[i]->PartId)
            {
                RemoveSubject(group->Movers[i]->PartId);
                break;
            }
        }
    }

    GroupSubjects[NumGroupSubjects] = group;
    NumGroupSubjects++;
    return 0;
}

auto MCParser::GetSubject(int16_t index) -> int32_t
{
    if (index > 11)
    {
        return 0;
    }

    return Subjects[index];
}

auto MCParser::IsSubject(int32_t partId) -> int
{
    for (int16_t i = 0; i < NumSubjects; i++)
    {
        if (GetSubject(i) == partId)
        {
            return -1;
        }
    }

    return 0;
}

auto MCParser::IsSubject(MCMoverGroup* group) -> int
{
    for (int16_t i = 0; i < NumGroupSubjects; i++)
    {
        if (GroupSubjects[i] == group)
        {
            return -1;
        }
    }

    return 0;
}

auto MCParser::RemoveSubject(int32_t partId) -> void
{
    uint32_t count = NumSubjects;
    int16_t index = 0;

    while (index < static_cast<int32_t>(count) && Subjects[index] != partId)
    {
        index++;
    }

    if (index < static_cast<int32_t>(count))
    {
        NumSubjects--;

        while (index < NumSubjects)
        {
            Subjects[index] = Subjects[index + 1];
            index++;
        }

        if (index < 12)
        {
            Subjects[index] = 0;
        }

        return;
    }

    // Not a single subject: if it is one through its lance, the lance gives way to its other movers.
    MCBaseObject* object = ObjectList()->FindObjectFromPart(partId);

    if (object == nullptr)
    {
        return;
    }

    if (object->ObjectClass != MCObjectClass::BattleMech && object->ObjectClass != MCObjectClass::GroundVehicle &&
        object->ObjectClass != MCObjectClass::Elemental && object->ObjectClass != MCObjectClass::Mover)
    {
        return;
    }

    MCMoverGroup* group = static_cast<MCMover*>(object)->Group;

    if (IsSubject(group) == 0)
    {
        return;
    }

    RemoveSubject(group);

    for (int16_t i = 0; i < group->NumMovers(); i++)
    {
        MCMover* member = group->Movers[i];

        if (member == nullptr || member->PartId == partId || NumSubjects >= 12)
        {
            continue;
        }

        // AddSubject, inlined.
        if (IsSubject(member->PartId) != 0)
        {
            continue;
        }

        Subjects[NumSubjects] = member->PartId;
        NumSubjects++;
    }
}

auto MCParser::RemoveSubject(MCMoverGroup* group) -> void
{
    uint32_t count = NumGroupSubjects;
    int16_t index = 0;

    while (index < static_cast<int32_t>(count) && GroupSubjects[index] != group)
    {
        index++;
    }

    if (index < static_cast<int32_t>(count))
    {
        NumGroupSubjects--;
    }
    while (index < NumGroupSubjects)
    {
        GroupSubjects[index] = GroupSubjects[index + 1];
        index++;
    }

    if (index < 4)
    {
        GroupSubjects[index] = nullptr;
    }
}

auto MCParser::ClearSubjects() -> void
{
    for (int32_t i = 0; i < 12; i++)
    {
        Subjects[i] = 0;
    }

    NumSubjects = 0;

    for (int32_t i = 0; i < 4; i++)
    {
        GroupSubjects[i] = nullptr;
    }

    NumGroupSubjects = 0;
}

auto MCParser::AddObjectLoc(MCVector3D location, int run) -> int
{
    auto* node = new (std::nothrow) MCLocationNode;

    if (node == nullptr)
    {
        return 0;
    }

    node->Location = location;
    node->Run = run;
    node->Next = nullptr;

    if (MovePath == nullptr)
    {
        MovePath = node;
        return -1;
    }

    MCLocationNode* last = MovePath;

    while (last->Next != nullptr)
    {
        last = last->Next;
    }

    last->Next = node;
    return -1;
}

auto CompareDistance(const void* a, const void* b) -> int
{
    float distanceA = static_cast<const MCMoverDistance*>(a)->Distance;
    float distanceB = static_cast<const MCMoverDistance*>(b)->Distance;

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

auto SortMoverList(int32_t numMovers, MCMover** movers, MCVector3D goal) -> void
{
    std::vector<MCMoverDistance> distances(static_cast<size_t>(numMovers));

    for (int32_t i = 0; i < numMovers; i++)
    {
        MCMover* mover = movers[i];

        if (mover == nullptr)
        {
            distances[i].Mover = nullptr;
            distances[i].Distance = std::bit_cast<float>(0x7f7fc99eu);
            continue;
        }

        distances[i].Mover = mover;
        MCVector3D position = mover->GetPosition();
        float dx = position.X - goal.X;
        float dy = position.Y - goal.Y;
        float dz = position.Z - goal.Z;
        distances[i].Distance = static_cast<float>(
            std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dz) * dz + static_cast<double>(dy) * dy));
    }

    qsort(distances.data(), numMovers, sizeof(MCMoverDistance), CompareDistance);

    for (int32_t i = 0; i < numMovers; i++)
    {
        if (distances[i].Mover != nullptr)
        {
            distances[i].Mover->SelectionIndex = i;
        }
    }
}

auto MCParser::SendTacOrder(MCTacticalOrder order, int sortMovers) -> int
{
    uint16_t count = NumSubjects;

    if (count == 0 && NumGroupSubjects == 0)
    {
        return 0;
    }

    SoundSystem->PlayDigitalSample(0xf, 1, nullptr, 0, 0);

    if (MovePath != nullptr)
    {
        order.InitWayPath(MovePath);
    }

    MCVector3D goal;
    goal.X = order.MoveParams.WayPath.Points[0];
    goal.Y = order.MoveParams.WayPath.Points[1];
    goal.Z = order.MoveParams.WayPath.Points[2];
    ClearMovePath();

    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        MPlayer->SendPlayerOrder(0, &order, sortMovers, count, Subjects, NumGroupSubjects, GroupSubjects, 0);
    }
    else
    {
        if (sortMovers != 0)
        {
            MCMover* movers[12];

            for (int32_t i = 0; i < count; i++)
            {
                movers[i] = static_cast<MCMover*>(ObjectList()->FindObjectFromPart(Subjects[i]));
            }

            SortMoverList(count, movers, goal);
        }

        goal = order.GetWayPoint(0);

        // A jump-attack (method 1) becomes a jump to the target's position.
        bool jumpToObject = false;

        if (order.Code == MCTacticalOrderCode::AttackObject)
        {
            if (order.AttackParams.Method == 1)
            {
                order.Code = MCTacticalOrderCode::JumpToObject;
                order.MoveParams.Wait = 0;
                order.MoveParams.WayPath.Mode[0] = 0;

                if (order.Target != nullptr)
                {
                    order.SetWayPoint(0, order.Target->GetPosition());
                }

                jumpToObject = true;
            }
        }
        else if (order.Code == MCTacticalOrderCode::JumpToObject)
        {
            jumpToObject = true;
        }

        if (jumpToObject)
        {
            order.Code = MCTacticalOrderCode::JumpToPoint;
            Assert(order.Target != nullptr, 0, " JumpToObject is NULL ");
            order.SetWayPoint(0, order.Target->GetPosition());
        }

        // A jump gives every mover its own landing spot around the goal.
        MCVector3D jumpGoals[MaxJumpGoals];

        if (order.Code == MCTacticalOrderCode::JumpToPoint)
        {
            int32_t numGoals = count;

            for (int32_t i = 0; i < NumGroupSubjects; i++)
            {
                numGoals += GroupSubjects[i]->NumMovers();
            }

            MCGameObject* jumpTarget = order.GetJumpTarget();
            CalcJumpGoals(order.GetWayPoint(0), numGoals, jumpGoals, jumpTarget);
        }

        for (int32_t i = 0; i < count; i++)
        {
            auto* mover = static_cast<MCMover*>(ObjectList()->FindObjectFromPart(Subjects[i]));

            if (mover == nullptr || mover == order.Target)
            {
                continue;
            }

            if (sortMovers != 0)
            {
                order.SelectionIndex = mover->SelectionIndex;
            }

            if (order.Code == MCTacticalOrderCode::JumpToPoint)
            {
                order.SetWayPoint(0, jumpGoals[i]);
            }

            mover->HandleTacticalOrder(order, 1, 0);
        }

        order.SetWayPoint(0, goal);

        int32_t goalIndex = count;

        for (int32_t i = 0; i < NumGroupSubjects; i++)
        {
            MCMoverGroup* group = GroupSubjects[i];

            if (group->NumMovers() == 1)
            {
                if (order.Code == MCTacticalOrderCode::JumpToPoint)
                {
                    order.SetWayPoint(0, jumpGoals[goalIndex]);
                }

                group->GetMover(0)->HandleTacticalOrder(order, 1, 0);
            }
            else
            {
                MCVector3D* destinations =
                    order.Code == MCTacticalOrderCode::JumpToPoint ? &jumpGoals[goalIndex] : nullptr;
                group->HandleTacticalOrder(order, 1, destinations, 0);
            }

            goalIndex += group->NumMovers();
        }
    }

    SoundSystem->PlayDigitalSample(0x11, 1, nullptr, 0, 0);

    if (TheInterface->CommandOneShot != 0)
    {
        TheInterface->CurrentCommand = 0;
    }

    return -1;
}

auto MCParser::PatrolUp() -> void
{
    if (MovePath == nullptr)
    {
        return;
    }

    MCTacticalOrder order;
    order.Reset();
    order.Reset(MCOrderOrigin::Player, MCTacticalOrderCode::PatrolPath, 0);
    order.InitWayPath(MovePath);
    order.MoveParams.Wait = 0;
    SendTacOrder(order, -1);
    ClearMovePath();
}

auto MCParser::TraverseUp() -> void
{
    if (MovePath == nullptr)
    {
        return;
    }

    MCTacticalOrder order;
    order.Reset();
    order.Reset(MCOrderOrigin::Player, MCTacticalOrderCode::TraversePath, 0);
    order.InitWayPath(MovePath);
    order.MoveParams.Wait = -1;
    SendTacOrder(order, -1);
    ClearMovePath();
}

auto MCParser::ClearMovePath() -> void
{
    while (MovePath != nullptr)
    {
        MCLocationNode* next = MovePath->Next;
        delete MovePath;
        MovePath = next;
    }
}
