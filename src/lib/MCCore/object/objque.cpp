#include "stdafx.h"
#include "object/objque.h"
#include "appear/appear.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "gui/asystem.h"
#include "main/main.h"
#include "object/bldng.h"
#include "object/bridge.h"
#include "object/gameobj.h"
#include "object/object.h"
#include "object/objevnt.h"
#include "object/mover.h"
#include "object/objtype.h"
#include "object/team.h"
#include "sprite/sprtmgr.h"
#include "terrain/terrain.h"

int32_t MCObjectQueue::ObjectsInList = 0;
int UpdateObjects = 1;
int UpdateTerrainObjects = 0;
int RenderObjects = 1;
int RenderTerrainObjects = 1;
int MaxObjectsDrawn = 0;

namespace
{
    /// <summary>Whether a list holds a terrain block's objects ("TBlk%d" or "RBlk%d").</summary>
    bool IsTerrainList(const MCObjectQueueNode* node)
    {
        return node->Id.contains("TBlk") || node->Id.contains("RBlk");
    }

    /// <summary>The list named <paramref name="listId"/> of <paramref name="first"/>'s chain, by IDString ==.</summary>
    MCObjectQueueNode* FindListById(MCObjectQueueNode* first, const char* listId)
    {
        for (MCObjectQueueNode* node = first; node != nullptr; node = node->Next)
        {
            if (*node == listId)
            {
                return node;
            }
        }

        return nullptr;
    }

    /// <summary>The object with part id <paramref name="partId"/> in <paramref name="node"/>, or null.</summary>
    MCBaseObject* FindPart(MCObjectQueueNode* node, int32_t partId)
    {
        for (MCBaseObject* object = node->Head; object != nullptr; object = object->Next)
        {
            if (object->PartId == partId)
            {
                return object;
            }
        }

        return nullptr;
    }
}

MCObjectQueueNode::MCObjectQueueNode(const char* newId)
{
    Init(newId, -1);
}

auto MCObjectQueueNode::Init(const char* newId, int32_t newBlockNumber) -> void
{
    MCIDString::Init(newId);
    BlockNumber = newBlockNumber;
    Next = nullptr;
    Tail = nullptr;
    Head = nullptr;
}

auto MCObjectQueueNode::AddNode(MCBaseObject* object) -> MCBaseObject*
{
    if (object != nullptr)
    {
        object->Next = nullptr;

        if (Tail == nullptr)
        {
            Tail = object;
            Head = object;
            return object;
        }

        Tail->Next = object;
        Tail = object;
    }

    return object;
}

auto MCObjectQueueNode::RemoveNode(MCBaseObject* prev, MCBaseObject* object) -> void
{
    if (Head == object)
    {
        Head = object->Next;
    }

    if (Tail == object)
    {
        Tail = prev;
    }

    if (prev != nullptr)
    {
        prev->Next = object->Next;
    }

    object->Next = nullptr;
}

auto MCObjectQueueNode::Traverse(MCBaseObject*& current) -> MCBaseObject*
{
    current = current == nullptr ? Head : current->Next;
    return current;
}

auto MCObjectQueueNode::Destroy() -> void
{
    MCBaseObject* object = Head;

    while (object != nullptr)
    {
        MCBaseObject* nextObject = object->Next;
        delete object;
        object = nextObject;
    }

    Tail = nullptr;
    Head = nullptr;
}

auto MCObjectQueueNode::Remove(MCBaseObject* object) -> int
{
    MCBaseObject* prev = nullptr;
    MCBaseObject* current = Head;

    while (true)
    {
        if (current == nullptr)
        {
            return 0;
        }

        if (current == object)
        {
            break;
        }

        prev = current;
        current = current->Next;
    }

    RemoveNode(prev, current);
    delete current;
    return 1;
}

auto MCObjectQueueNode::Render() -> void
{
    if (!((IsTerrainList(this) && RenderTerrainObjects != 0) || (!IsTerrainList(this) && RenderObjects != 0)))
    {
        return;
    }

    // The original breaks into the debugger (int 3) when "TBlk" is found other than at the start of the name.
    if (!BlockInList(BlockNumber))
    {
        return;
    }

    for (MCBaseObject* object = Head; object != nullptr; object = object->Next)
    {
        object->Render();

        if (GRestartRender != 0 || MaxObjectsDrawn != 0)
        {
            return;
        }
    }
}

auto MCObjectQueueNode::Update() -> void
{
    const bool terrain = IsTerrainList(this);

    if (!((terrain && UpdateTerrainObjects != 0) || (!terrain && UpdateObjects != 0)))
    {
        return;
    }

    if (!BlockInList(BlockNumber))
    {
        return;
    }

    MCBaseObject* prev = nullptr;

    while (true)
    {
        MCBaseObject* object = prev == nullptr ? Head : prev->Next;

        if (object == nullptr)
        {
            break;
        }

        MCObjectQueue::ObjectsInList++;

        if (object->Update() == 0 && object->GetObjectType() != nullptr)
        {
            RemoveNode(prev, object);
            delete object;
            continue;
        }

        prev = object;
    }
}

auto MCObjectQueueNode::FindObjectFromEvent(MCObjectEvent* event, int skipDisabled) -> MCBaseObject*
{
    if (!BlockInList(BlockNumber))
    {
        return nullptr;
    }

    for (MCBaseObject* object = Head; object != nullptr; object = object->Next)
    {
        auto* gameObject = static_cast<MCGameObject*>(object);
        MCAppearance* appearance = object->GetAppearance();

        if (appearance == nullptr || appearance->Visible == 0)
        {
            // The original also tests objectClass 0x14 against floats at +0x94..+0xa0, but no class ever sets
            // 0x14 (see ObjectClass), so that branch is dead and left out.
            if (object->ObjectClass != MISCTERRAINOBJECT)
            {
                continue;
            }

            MCCamera* cam = event->Window->GetCamera();

            if (cam == nullptr)
            {
                continue;
            }

            // Port: on the view's world surface, through the zoom.
            const MCVector2D mouse = MCWindowPoint(event->Window, event->Event.X, event->Event.Y);
            const float mouseX = mouse.X;
            const float mouseY = mouse.Y;
            auto* misc = static_cast<MCMiscTerrainObject*>(object);
            int32_t block = misc->BlockNumber;
            int32_t vertex = misc->VertexNumber;

            if (block < 0)
            {
                block = 0;
            }

            if (block >= MCTerrain::TotalBlocks)
            {
                block = MCTerrain::TotalBlocks - 1;
            }

            if (vertex < 0)
            {
                vertex = 0;
            }

            if (vertex >= VerticesPerBlock)
            {
                vertex = VerticesPerBlock - 1;
            }

            const int32_t screenX = MCTerrain::ScreenPosX[MCTerrain::BlockOffsets[block] + vertex];

            if (screenX == 0x11111111)
            {
                continue;
            }

            // The box sits 70 pixels below the vertex: 50 pixels each way for kind 5, 30 otherwise, halved when
            // zoomed out.
            const float scale = cam->CameraScale == 1 ? 0.5f : 1.0f;
            const float halfSize = misc->TerrainObjectKind == 5 ? 50.0f : 30.0f;
            const float centerX = static_cast<float>(screenX);
            const float centerY =
                scale * 70.0f + static_cast<float>(MCTerrain::ScreenPosY[MCTerrain::BlockOffsets[block] + vertex]);

            if (centerX - scale * halfSize <= mouseX && mouseX <= scale * halfSize + centerX &&
                centerY - scale * halfSize <= mouseY && mouseY <= scale * halfSize + centerY)
            {
                return object;
            }

            continue;
        }

        if (gameObject->GetWindowsVisible() <= Turn - 3)
        {
            continue;
        }

        appearance->RecalcBounds(event->Window->GetCamera());
        // Port: on the view's world surface, through the zoom.
        const MCVector2D mouse = MCWindowPoint(event->Window, event->Event.X, event->Event.Y);
        const float mouseX = mouse.X;
        const float mouseY = mouse.Y;
        MCAppearanceType* type = appearance->GetAppearanceType();

        if (type != nullptr && (type->BoundsUpperLeftX != 0 || type->BoundsUpperLeftY != 0 ||
                                type->BoundsLowerRightX != 0 || type->BoundsLowerRightY != 0))
        {
            // Zoomed out, the type's pixel bounds are halved.
            const int shift = Eye->CameraScale == 1 ? 1 : 0;
            const int32_t left = type->BoundsUpperLeftX >> shift;
            const int32_t top = type->BoundsUpperLeftY >> shift;
            const int32_t right = type->BoundsLowerRightX >> shift;
            const int32_t bottom = type->BoundsLowerRightY >> shift;

            if (!(static_cast<float>(left) + appearance->GetScreenPos(nullptr).X <= mouseX &&
                  mouseX <= static_cast<float>(right) + appearance->GetScreenPos(nullptr).X &&
                  static_cast<float>(top) + appearance->GetScreenPos(nullptr).Y <= mouseY &&
                  mouseY <= static_cast<float>(bottom) + appearance->GetScreenPos(nullptr).Y))
            {
                continue;
            }
        }
        else if (mouseX < appearance->UpperLeft.X || appearance->LowerRight.X < mouseX ||
                 mouseY < appearance->UpperLeft.Y || appearance->LowerRight.Y < mouseY)
        {
            continue;
        }

        if (gameObject->IsDisabled() == 0 && gameObject->IsDestroyed() == 0)
        {
            return object;
        }

        if (skipDisabled == 0)
        {
            return object;
        }
    }

    return nullptr;
}

auto MCObjectQueueNode::HandleEvent(MCObjectEvent* event) -> MCBaseObject*
{
    MCBaseObject* object = FindObjectFromEvent(event, 0);

    if (object != nullptr)
    {
        object->HandleEvent(event);
    }

    return object;
}

auto MCObjectQueueNode::FindObject(MCVector3D position, float& distance) -> MCBaseObject*
{
    MCBaseObject* result = nullptr;

    for (MCBaseObject* object = Head; object != nullptr; object = object->Next)
    {
        if (static_cast<int32_t>(object->ObjectClass) <= 0)
        {
            continue;
        }

        auto* gameObject = static_cast<MCGameObject*>(object);

        if (gameObject->InTransport() != 0)
        {
            continue;
        }

        const auto objectDistance = static_cast<float>(gameObject->DistanceFrom(position));
        MCObjectType* type = gameObject->GetObjectType();
        const float extent = type != nullptr ? type->ExtentRadius : 0.0f;

        if (objectDistance < extent && objectDistance < distance)
        {
            distance = objectDistance;
            result = object;
        }
    }

    return result;
}

auto MCObjectQueueNode::MakeObjDataBlock(MCObjData* data) -> int32_t
{
    std::memset(data, 0xff, 0x898);
    int32_t count = 0;

    for (MCBaseObject* object = Head; object != nullptr; object = object->Next)
    {
        count++;
        MCObjectType* type = object->GetObjectType();

        if (type == nullptr || object->ObjectClass != BUILDING)
        {
            data->ObjTypeNum = -1;
            data->BlockNumber = 0;
            data->VertexNumber = 0;
            data->PixelOffsetY = 0;
            data->PixelOffsetX = 0;
            data->Damage = 0;
        }
        else
        {
            auto* building = static_cast<MCBuilding*>(object);
            data->PixelOffsetX = static_cast<int16_t>(building->PixelOffsetX);
            data->ObjTypeNum = static_cast<int16_t>(type->ObjTypeNum);
            data->PixelOffsetY = static_cast<int16_t>(building->PixelOffsetY);
            data->BlockNumber = static_cast<int16_t>(building->BlockNumber);
            data->VertexNumber = static_cast<int16_t>(building->VertexNumber);
            data->Damage = static_cast<uint8_t>(static_cast<int32_t>(building->GetDamage()));
        }

        data++;
    }

    data->ObjTypeNum = -1;
    data->BlockNumber = 0;
    data->VertexNumber = 0;
    data->PixelOffsetY = 0;
    data->PixelOffsetX = 0;
    data->Damage = 0;
    return count;
}

auto MCObjectQueue::AddList(MCObjectQueueNode* node) -> void
{
    if (node != nullptr)
    {
        node->Next = nullptr;

        if (Tail == nullptr)
        {
            Tail = node;
            Head = node;
            return;
        }

        Tail->Next = node;
        Tail = node;
    }
}

auto MCObjectQueue::FindList(const char* listId) -> MCObjectQueueNode*
{
    // The original compares up to eight characters, so a longer listId never matches.
    for (MCObjectQueueNode* node = Head; node != nullptr; node = node->Next)
    {
        if (*node == listId)
        {
            return node;
        }
    }

    return nullptr;
}

auto MCObjectQueue::Render() -> void
{
    for (MCObjectQueueNode* node = Head; node != nullptr; node = node->Next)
    {
        if (node->Head != nullptr)
        {
            node->Render();
        }

        if (GRestartRender != 0 || MaxObjectsDrawn != 0)
        {
            return;
        }
    }
}

auto MCObjectQueue::Update() -> void
{
    ObjectsInList = 0;

    for (MCObjectQueueNode* node = Head; node != nullptr; node = node->Next)
    {
        if (node->Head != nullptr)
        {
            node->Update();
        }
    }
}

auto MCObjectQueue::FindObjectFromEvent(MCObjectEvent* event) -> MCBaseObject*
{
    MCObjectQueueNode* node = Head;
    MCBaseObject* result;

    if (HomeTeam->Id == -1)
    {
        result = InnerSphereMechList->FindObjectFromEvent(event, 1);
    }
    else
    {
        result = ClanMechList->FindObjectFromEvent(event, 1);
    }

    if (result == nullptr)
    {
        result = node->FindObjectFromEvent(event, 1);

        for (; result == nullptr && node != nullptr; node = node->Next)
        {
            if (node->Head != nullptr)
            {
                result = node->FindObjectFromEvent(event, 0);
            }
        }
    }

    return result;
}

auto MCObjectQueue::HandleEvent(MCObjectEvent* event) -> MCBaseObject*
{
    MCBaseObject* result = nullptr;

    for (MCObjectQueueNode* node = Head; node != nullptr && result == nullptr; node = node->Next)
    {
        if (node->Head != nullptr)
        {
            result = node->HandleEvent(event);
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

    for (MCObjectQueueNode* node = Head; node != nullptr; node = node->Next)
    {
        if (node->Head == nullptr)
        {
            continue;
        }

        float distance = 100000.0f;
        MCBaseObject* object = node->FindObject(position, distance);

        if (object != nullptr && distance < bestDistance)
        {
            result = object;
            bestDistance = distance;
        }
    }

    return result;
}

auto MCObjectQueue::FindObjectId(int32_t typeId) -> MCBaseObject*
{
    for (MCObjectQueueNode* node = Head; node != nullptr; node = node->Next)
    {
        for (MCBaseObject* object = node->Head; object != nullptr; object = object->Next)
        {
            MCObjectType* type = object->GetObjectType();

            if (type != nullptr && type->ObjTypeNum == typeId)
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

    const int32_t blockNum = (partId - 0x1000) / 0xc80;
    char listId[12];
    std::snprintf(listId, sizeof(listId), "TBlk%d", blockNum);
    MCObjectQueueNode* node = FindListById(Head, listId);

    if (node != nullptr && node->Head != nullptr)
    {
        if (MCBaseObject* object = FindPart(node, partId))
        {
            return object;
        }
    }

    // Not in the block's TBlk list (movers, light walls): try its RBlk list (buildings and the rest).
    std::snprintf(listId, sizeof(listId), "RBlk%d", blockNum);
    node = FindListById(Head, listId);

    if (node != nullptr && node->Head != nullptr)
    {
        if (MCBaseObject* object = FindPart(node, partId))
        {
            return object;
        }
    }

    return Head != nullptr ? FindPart(Head, partId) : nullptr;
}

auto MCObjectQueue::FindObjectInGroup(MCBaseObject* current, int32_t groupId) -> MCBaseObject*
{
    // Steps from current (or the list's head) until test passes; null at the end.
    auto step = [current](MCObjectQueueNode* list, auto test) -> MCBaseObject*
    {
        MCBaseObject* object = current == nullptr ? list->Head : current->Next;

        while (object != nullptr && !test(object))
        {
            object = object->Next;
        }

        return object;
    };

    if (groupId == 0)
    {
        return nullptr;
    }

    if (groupId == 500)
    {
        return step(InnerSphereMechList, [](MCBaseObject* object) { return object->UnderPlayerControl() != 0; });
    }

    if (groupId == 501)
    {
        return current != nullptr ? current->Next : ClanMechList->Head;
    }

    if (groupId == 502)
    {
        return step(InnerSphereMechList, [](MCBaseObject* object) { return object->UnderPlayerControl() == 0; });
    }

    if (groupId >= 1 && groupId <= 0x20)
    {
        return step(InnerSphereMechList,
                    [groupId](MCBaseObject* object) { return object->GetGroupId() == groupId - 1; });
    }

    if (groupId >= 0x149 && groupId <= 0x168)
    {
        return step(InnerSphereMechList,
                    [groupId](MCBaseObject* object) { return object->GetGroupId() == groupId - 0x149; });
    }

    if (groupId >= 0xa5 && groupId <= 0xc4)
    {
        return step(ClanMechList, [groupId](MCBaseObject* object) { return object->GetGroupId() == groupId - 0xa5; });
    }

    return nullptr;
}

auto MCObjectQueue::Traverse(MCBaseObject*& current) -> MCBaseObject*
{
    MCBaseObject* result = nullptr;

    if (current == nullptr || (result = current->Next) == nullptr)
    {
        MCObjectQueueNode* node = Head;

        if (current != nullptr)
        {
            // Find the list that ends with current, then go on from the one after it.
            if (node == nullptr)
            {
                current = nullptr;
                return nullptr;
            }
            while (true)
            {
                MCBaseObject* last = node->Tail;
                node = node->Next;

                if (last == current)
                {
                    break;
                }

                if (node == nullptr)
                {
                    current = nullptr;
                    return nullptr;
                }
            }
        }

        for (; node != nullptr && (result = node->Head) == nullptr; node = node->Next)
        {
        }
    }

    current = result;
    return result;
}

auto BlockInList(int32_t blockNumber) -> int
{
    for (int32_t i = 0; i < MAX_BLOCK_LIST; i++)
    {
        if (UsedBlockList[i] == blockNumber)
        {
            return 1;
        }

        if (UsedBlockList[i] == -1)
        {
            break;
        }
    }

    return 0;
}
