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
    AddList(std::make_shared<MCObjectList>(DefaultListName));
}

MCObjectQueue::~MCObjectQueue()
{
    // First to last, as the original emptied it. A list the object system still shares goes empty.
    while (!_Lists.empty())
    {
        std::shared_ptr<MCObjectList> list = std::move(_Lists.front());
        _Lists.erase(_Lists.begin());
        list->Clear();
    }
}

auto MCObjectQueue::AddList(std::shared_ptr<MCObjectList> list) -> std::shared_ptr<MCObjectList>
{
    return _Lists.emplace_back(std::move(list));
}

auto MCObjectQueue::FindList(std::string_view name) const -> std::shared_ptr<MCObjectList>
{
    for (const std::shared_ptr<MCObjectList>& list : _Lists)
    {
        if (list->Name() == name)
        {
            return list;
        }
    }

    return nullptr;
}

auto MCObjectQueue::FindOrAddList(std::string_view name) -> std::shared_ptr<MCObjectList>
{
    if (std::shared_ptr<MCObjectList> list = FindList(name))
    {
        return list;
    }

    return AddList(std::make_shared<MCObjectList>(name));
}

auto MCObjectQueue::DeleteList(MCObjectList& list) -> void
{
    const auto position = std::ranges::find(_Lists, &list, &std::shared_ptr<MCObjectList>::get);

    if (position != _Lists.end())
    {
        // Out of the queue before its objects go.
        std::shared_ptr<MCObjectList> owned = std::move(*position);
        _Lists.erase(position);
        owned->Clear();
    }
}

auto MCObjectQueue::Remove(MCBaseObject& object) -> bool
{
    return std::ranges::any_of(_Lists,
                               [&object](const std::shared_ptr<MCObjectList>& list) { return list->Remove(object); });
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
    for (const std::shared_ptr<MCObjectList>& list : _Lists)
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

auto MCObjectQueue::FindObjectFromEvent(MCObjectEvent& event) -> std::shared_ptr<MCBaseObject>
{
    // Original behaviour: a team's id is 0, 1 or 2, never -1, so the clan's mechs are always looked at first.
    std::shared_ptr<MCBaseObject> result = HomeTeam()->Id == -1 ? InnerSphereMechList()->FindObjectFromEvent(event, 1)
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

auto MCObjectQueue::HandleEvent(MCObjectEvent& event) -> std::shared_ptr<MCBaseObject>
{
    std::shared_ptr<MCBaseObject> result;

    for (size_t i = 0; i < _Lists.size() && result == nullptr; i++)
    {
        if (!_Lists[i]->Empty())
        {
            result = _Lists[i]->HandleEvent(event);
        }
    }

    return result;
}

auto MCObjectQueue::HandleEvent(uint32_t partId, MCObjectEvent& event) -> std::shared_ptr<MCBaseObject>
{
    std::shared_ptr<MCBaseObject> object = FindObjectFromPart(static_cast<int32_t>(partId));

    if (object != nullptr)
    {
        object->HandleEvent(event);
        return object;
    }

    return nullptr;
}

auto MCObjectQueue::FindObject(MCVector3D position) -> std::shared_ptr<MCBaseObject>
{
    std::shared_ptr<MCBaseObject> result;
    float bestDistance = 100000.0f;

    for (const std::shared_ptr<MCObjectList>& list : _Lists)
    {
        if (list->Empty())
        {
            continue;
        }

        float distance = 100000.0f;
        std::shared_ptr<MCBaseObject> object = list->FindObject(position, distance);

        if (object != nullptr && distance < bestDistance)
        {
            result = std::move(object);
            bestDistance = distance;
        }
    }

    return result;
}

auto MCObjectQueue::FindObjectId(int32_t typeId) const -> std::shared_ptr<MCBaseObject>
{
    return FindIf(
        [typeId](MCBaseObject& object)
        {
            std::shared_ptr<MCObjectType> type = object.GetObjectType();
            return type != nullptr && type->ObjTypeNum == typeId;
        });
}

auto MCObjectQueue::FindIf(const std::function<bool(MCBaseObject&)>& match) const -> std::shared_ptr<MCBaseObject>
{
    for (const std::shared_ptr<MCObjectList>& list : _Lists)
    {
        for (std::shared_ptr<MCBaseObject> object : *list)
        {
            if (match(*object))
            {
                return object;
            }
        }
    }

    return nullptr;
}

auto MCObjectQueue::FindObjectFromPart(int32_t partId) -> std::shared_ptr<MCBaseObject>
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
        if (std::shared_ptr<MCObjectList> list = FindList(name))
        {
            if (std::shared_ptr<MCBaseObject> object = list->FindPart(partId))
            {
                return object;
            }
        }
    }

    return _Lists.front()->FindPart(partId);
}

auto MCObjectQueue::FindObjectInGroup(std::shared_ptr<MCBaseObject> current, int32_t groupId)
    -> std::shared_ptr<MCBaseObject>
{
    // Steps from the object after current (or the list's first) until test passes; null at the end.
    auto step = [&current](const MCObjectList& list, auto test) -> std::shared_ptr<MCBaseObject>
    {
        std::shared_ptr<MCBaseObject> object = current != nullptr ? list.After(*current) : list.After();

        while (object != nullptr && !test(*object))
        {
            object = list.After(*object);
        }

        return object;
    };

    if (groupId == 0)
    {
        return nullptr;
    }

    if (groupId == 500)
    {
        return step(*InnerSphereMechList(), [](MCBaseObject& object) { return object.UnderPlayerControl() != 0; });
    }

    if (groupId == 501)
    {
        return step(*ClanMechList(), [](MCBaseObject&) { return true; });
    }

    if (groupId == 502)
    {
        return step(*InnerSphereMechList(), [](MCBaseObject& object) { return object.UnderPlayerControl() == 0; });
    }

    if (groupId >= 1 && groupId <= 0x20)
    {
        return step(*InnerSphereMechList(),
                    [groupId](MCBaseObject& object) { return object.GetGroupId() == groupId - 1; });
    }

    if (groupId >= 0x149 && groupId <= 0x168)
    {
        return step(*InnerSphereMechList(),
                    [groupId](MCBaseObject& object) { return object.GetGroupId() == groupId - 0x149; });
    }

    if (groupId >= 0xa5 && groupId <= 0xc4)
    {
        return step(*ClanMechList(), [groupId](MCBaseObject& object) { return object.GetGroupId() == groupId - 0xa5; });
    }

    return nullptr;
}
