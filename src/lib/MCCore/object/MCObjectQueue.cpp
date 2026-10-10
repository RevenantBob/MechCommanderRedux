#include "stdafx.h"
#include "object/MCObjectQueue.h"
#include "object/MCBaseObject.h"
#include "object/MCForces.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectType.h"
#include "object/MCTeam.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"

int32_t MCObjectQueue::ObjectsInList = 0;

MCObjectQueue::MCObjectQueue()
{
    AddList(std::make_unique<MCObjectList>(DefaultListName));
}

MCObjectQueue::~MCObjectQueue()
{
    // First to last, as the original emptied it.
    while (!_Lists.empty())
    {
        std::unique_ptr<MCObjectList> list = std::move(_Lists.front());
        _Lists.erase(_Lists.begin());
    }
}

auto MCObjectQueue::AddList(std::unique_ptr<MCObjectList> list) -> MCObjectList*
{
    return _Lists.emplace_back(std::move(list)).get();
}

auto MCObjectQueue::FindList(std::string_view name) const -> MCObjectList*
{
    for (const std::unique_ptr<MCObjectList>& list : _Lists)
    {
        if (list->Name() == name)
        {
            return list.get();
        }
    }

    return nullptr;
}

auto MCObjectQueue::FindOrAddList(std::string_view name) -> MCObjectList*
{
    if (MCObjectList* list = FindList(name))
    {
        return list;
    }

    return AddList(std::make_unique<MCObjectList>(name));
}

auto MCObjectQueue::DeleteList(MCObjectList* list) -> void
{
    const auto position = std::ranges::find(_Lists, list, &std::unique_ptr<MCObjectList>::get);

    if (position != _Lists.end())
    {
        // Out of the queue before its objects go.
        std::unique_ptr<MCObjectList> owned = std::move(*position);
        _Lists.erase(position);
    }
}

auto MCObjectQueue::Remove(MCBaseObject* object) -> bool
{
    return std::ranges::any_of(_Lists,
                               [object](const std::unique_ptr<MCObjectList>& list) { return list->Remove(object); });
}

auto MCObjectQueue::Render() -> void
{
    for (size_t i = 0; i < _Lists.size(); i++)
    {
        if (!_Lists[i]->Empty())
        {
            _Lists[i]->Render();
        }
    }
}

auto MCObjectQueue::Update() -> void
{
    ObjectsInList = 0;

    // Nothing walks the lists at the top of the frame: the slots of last frame's removed objects go.
    for (const std::unique_ptr<MCObjectList>& list : _Lists)
    {
        list->Compact();
    }

    for (size_t i = 0; i < _Lists.size(); i++)
    {
        if (!_Lists[i]->Empty())
        {
            _Lists[i]->Update();
        }
    }
}

auto MCObjectQueue::FindObjectFromEvent(MCObjectEvent* event) -> MCBaseObject*
{
    // Original behaviour: a team's id is 0, 1 or 2, never -1, so the clan's mechs are always looked at first.
    MCBaseObject* result = HomeTeam()->Id == -1 ? InnerSphereMechList()->FindObjectFromEvent(event, 1)
                                                : ClanMechList()->FindObjectFromEvent(event, 1);

    if (result == nullptr)
    {
        result = _Lists.front()->FindObjectFromEvent(event, 1);

        for (size_t i = 0; result == nullptr && i < _Lists.size(); i++)
        {
            if (!_Lists[i]->Empty())
            {
                result = _Lists[i]->FindObjectFromEvent(event, 0);
            }
        }
    }

    return result;
}

auto MCObjectQueue::HandleEvent(MCObjectEvent* event) -> MCBaseObject*
{
    MCBaseObject* result = nullptr;

    for (size_t i = 0; i < _Lists.size() && result == nullptr; i++)
    {
        if (!_Lists[i]->Empty())
        {
            result = _Lists[i]->HandleEvent(event);
        }
    }

    return result;
}

auto MCObjectQueue::HandleEvent(uint32_t partId, MCObjectEvent* event) -> MCBaseObject*
{
    MCBaseObject* object = FindObjectFromPart(static_cast<int32_t>(partId));

    if (object != nullptr)
    {
        object->HandleEvent(event);
        return object;
    }

    return nullptr;
}

auto MCObjectQueue::FindObject(MCVector3D position) -> MCBaseObject*
{
    MCBaseObject* result = nullptr;
    float bestDistance = 100000.0f;

    for (const std::unique_ptr<MCObjectList>& list : _Lists)
    {
        if (list->Empty())
        {
            continue;
        }

        float distance = 100000.0f;
        MCBaseObject* object = list->FindObject(position, distance);

        if (object != nullptr && distance < bestDistance)
        {
            result = object;
            bestDistance = distance;
        }
    }

    return result;
}

auto MCObjectQueue::FindObjectId(int32_t typeId) const -> MCBaseObject*
{
    return FindIf(
        [typeId](MCBaseObject* object)
        {
            MCObjectType* type = object->GetObjectType();
            return type != nullptr && type->ObjTypeNum == typeId;
        });
}

auto MCObjectQueue::FindIf(const std::function<bool(MCBaseObject*)>& match) const -> MCBaseObject*
{
    for (const std::unique_ptr<MCObjectList>& list : _Lists)
    {
        for (MCBaseObject* object : *list)
        {
            if (match(object))
            {
                return object;
            }
        }
    }

    return nullptr;
}

auto MCObjectQueue::FindObjectFromPart(int32_t partId) -> MCBaseObject*
{
    if (partId == 0 || partId < 0x200)
    {
        return nullptr;
    }

    if (partId < 0x1000)
    {
        return GetMoverFromPartId(partId);
    }

    // The block's TBlk list (movers, light walls), then its RBlk list (buildings and the rest), then the first.
    const int32_t blockNum = (partId - 0x1000) / 0xc80;

    for (const std::string& name : {std::format("TBlk{}", blockNum), std::format("RBlk{}", blockNum)})
    {
        if (MCObjectList* list = FindList(name))
        {
            if (MCBaseObject* object = list->FindPart(partId))
            {
                return object;
            }
        }
    }

    return _Lists.front()->FindPart(partId);
}

auto MCObjectQueue::FindObjectInGroup(MCBaseObject* current, int32_t groupId) -> MCBaseObject*
{
    // Steps from the object after current (or the list's first) until test passes; null at the end.
    auto step = [current](const MCObjectList* list, auto test) -> MCBaseObject*
    {
        MCBaseObject* object = list->After(current);

        while (object != nullptr && !test(object))
        {
            object = list->After(object);
        }

        return object;
    };

    if (groupId == 0)
    {
        return nullptr;
    }

    if (groupId == 500)
    {
        return step(InnerSphereMechList(), [](MCBaseObject* object) { return object->UnderPlayerControl() != 0; });
    }

    if (groupId == 501)
    {
        return ClanMechList()->After(current);
    }

    if (groupId == 502)
    {
        return step(InnerSphereMechList(), [](MCBaseObject* object) { return object->UnderPlayerControl() == 0; });
    }

    if (groupId >= 1 && groupId <= 0x20)
    {
        return step(InnerSphereMechList(),
                    [groupId](MCBaseObject* object) { return object->GetGroupId() == groupId - 1; });
    }

    if (groupId >= 0x149 && groupId <= 0x168)
    {
        return step(InnerSphereMechList(),
                    [groupId](MCBaseObject* object) { return object->GetGroupId() == groupId - 0x149; });
    }

    if (groupId >= 0xa5 && groupId <= 0xc4)
    {
        return step(ClanMechList(), [groupId](MCBaseObject* object) { return object->GetGroupId() == groupId - 0xa5; });
    }

    return nullptr;
}
