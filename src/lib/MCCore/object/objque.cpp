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

int32_t ObjectQueue::objectsInList = 0;
int updateObjects = 1;
int updateTerrainObjects = 0;
int renderObjects = 1;
int renderTerrainObjects = 1;
int MaxObjectsDrawn = 0;

namespace
{
    /// <summary>Whether a list holds a terrain block's objects ("TBlk%d" or "RBlk%d").</summary>
    bool IsTerrainList(const ObjectQueueNode* node)
    {
        return std::strstr(node->id, "TBlk") != nullptr || std::strstr(node->id, "RBlk") != nullptr;
    }

    /// <summary>The list named <paramref name="listId"/> of <paramref name="first"/>'s chain, by IDString ==.</summary>
    ObjectQueueNode* FindListById(ObjectQueueNode* first, const char* listId)
    {
        for (ObjectQueueNode* node = first; node != nullptr; node = node->next)
        {
            if (*node == listId)
            {
                return node;
            }
        }

        return nullptr;
    }

    /// <summary>The object with part id <paramref name="partId"/> in <paramref name="node"/>, or null.</summary>
    BaseObject* FindPart(ObjectQueueNode* node, int32_t partId)
    {
        for (BaseObject* object = node->head; object != nullptr; object = object->next)
        {
            if (object->partId == partId)
            {
                return object;
            }
        }

        return nullptr;
    }
}

ObjectQueueNode::ObjectQueueNode(const char* newId)
{
    init(newId, -1);
}

auto ObjectQueueNode::init(const char* newId, int32_t newBlockNumber) -> void
{
    std::strncpy(id, newId, 7);
    blockNumber = newBlockNumber;
    id[7] = 0;
    next = nullptr;
    tail = nullptr;
    head = nullptr;
}

auto ObjectQueueNode::addNode(BaseObject* object) -> BaseObject*
{
    if (object != nullptr)
    {
        object->next = nullptr;

        if (tail == nullptr)
        {
            tail = object;
            head = object;
            return object;
        }

        tail->next = object;
        tail = object;
    }

    return object;
}

auto ObjectQueueNode::removeNode(BaseObject* prev, BaseObject* object) -> void
{
    if (head == object)
    {
        head = object->next;
    }

    if (tail == object)
    {
        tail = prev;
    }

    if (prev != nullptr)
    {
        prev->next = object->next;
    }

    object->next = nullptr;
}

auto ObjectQueueNode::Traverse(BaseObject*& current) -> BaseObject*
{
    current = current == nullptr ? head : current->next;
    return current;
}

auto ObjectQueueNode::destroy() -> void
{
    BaseObject* object = head;

    while (object != nullptr)
    {
        BaseObject* nextObject = object->next;
        delete object;
        object = nextObject;
    }

    tail = nullptr;
    head = nullptr;
}

auto ObjectQueueNode::remove(BaseObject* object) -> int
{
    BaseObject* prev = nullptr;
    BaseObject* current = head;

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
        current = current->next;
    }

    removeNode(prev, current);
    delete current;
    return 1;
}

auto ObjectQueueNode::render() -> void
{
    if (!((IsTerrainList(this) && renderTerrainObjects != 0) || (!IsTerrainList(this) && renderObjects != 0)))
    {
        return;
    }

    // The original breaks into the debugger (int 3) when "TBlk" is found other than at the start of the name.
    if (!blockInList(blockNumber))
    {
        return;
    }

    for (BaseObject* object = head; object != nullptr; object = object->next)
    {
        object->render();

        if (gRestartRender != 0 || MaxObjectsDrawn != 0)
        {
            return;
        }
    }
}

auto ObjectQueueNode::update() -> void
{
    const bool terrain = IsTerrainList(this);

    if (!((terrain && updateTerrainObjects != 0) || (!terrain && updateObjects != 0)))
    {
        return;
    }

    if (!blockInList(blockNumber))
    {
        return;
    }

    BaseObject* prev = nullptr;

    while (true)
    {
        BaseObject* object = prev == nullptr ? head : prev->next;

        if (object == nullptr)
        {
            break;
        }

        ObjectQueue::objectsInList++;

        if (object->update() == 0 && object->getObjectType() != nullptr)
        {
            removeNode(prev, object);
            delete object;
            continue;
        }

        prev = object;
    }
}

auto ObjectQueueNode::findObjectFromEvent(ObjectEvent* event, int skipDisabled) -> BaseObject*
{
    if (!blockInList(blockNumber))
    {
        return nullptr;
    }

    for (BaseObject* object = head; object != nullptr; object = object->next)
    {
        auto* gameObject = static_cast<GameObject*>(object);
        Appearance* appearance = object->getAppearance();

        if (appearance == nullptr || appearance->visible == 0)
        {
            // The original also tests objectClass 0x14 against floats at +0x94..+0xa0, but no class ever sets
            // 0x14 (see ObjectClass), so that branch is dead and left out.
            if (object->objectClass != MISCTERRAINOBJECT)
            {
                continue;
            }

            Camera* cam = event->window->GetCamera();

            if (cam == nullptr)
            {
                continue;
            }

            float mouseX = static_cast<float>(event->event.x);
            float mouseY = static_cast<float>(event->event.y);
            mouseX -= static_cast<float>(event->window->globalX());
            mouseY -= static_cast<float>(event->window->globalY());
            auto* misc = static_cast<MiscTerrainObject*>(object);
            int32_t block = misc->blockNumber;
            int32_t vertex = misc->vertexNumber;

            if (block < 0)
            {
                block = 0;
            }

            if (block >= Terrain::totalBlocks)
            {
                block = Terrain::totalBlocks - 1;
            }

            if (vertex < 0)
            {
                vertex = 0;
            }

            if (vertex >= verticesPerBlock)
            {
                vertex = verticesPerBlock - 1;
            }

            const int32_t screenX = Terrain::screenPosX[Terrain::blockOffsets[block] + vertex];

            if (screenX == 0x11111111)
            {
                continue;
            }

            // The box sits 70 pixels below the vertex: 50 pixels each way for kind 5, 30 otherwise, halved when
            // zoomed out.
            const float scale = cam->cameraScale == 1 ? 0.5f : 1.0f;
            const float halfSize = misc->terrainObjectKind == 5 ? 50.0f : 30.0f;
            const float centerX = static_cast<float>(screenX);
            const float centerY =
                scale * 70.0f + static_cast<float>(Terrain::screenPosY[Terrain::blockOffsets[block] + vertex]);

            if (centerX - scale * halfSize <= mouseX && mouseX <= scale * halfSize + centerX &&
                centerY - scale * halfSize <= mouseY && mouseY <= scale * halfSize + centerY)
            {
                return object;
            }

            continue;
        }

        if (gameObject->getWindowsVisible() <= turn - 3)
        {
            continue;
        }

        appearance->recalcBounds(event->window->GetCamera());
        float mouseX = static_cast<float>(event->event.x);
        float mouseY = static_cast<float>(event->event.y);
        mouseX -= static_cast<float>(event->window->globalX());
        mouseY -= static_cast<float>(event->window->globalY());
        AppearanceType* type = appearance->getAppearanceType();

        if (type != nullptr && (type->boundsUpperLeftX != 0 || type->boundsUpperLeftY != 0 ||
                                type->boundsLowerRightX != 0 || type->boundsLowerRightY != 0))
        {
            // Zoomed out, the type's pixel bounds are halved.
            const int shift = eye->cameraScale == 1 ? 1 : 0;
            const int32_t left = type->boundsUpperLeftX >> shift;
            const int32_t top = type->boundsUpperLeftY >> shift;
            const int32_t right = type->boundsLowerRightX >> shift;
            const int32_t bottom = type->boundsLowerRightY >> shift;

            if (!(static_cast<float>(left) + appearance->getScreenPos(nullptr).x <= mouseX &&
                  mouseX <= static_cast<float>(right) + appearance->getScreenPos(nullptr).x &&
                  static_cast<float>(top) + appearance->getScreenPos(nullptr).y <= mouseY &&
                  mouseY <= static_cast<float>(bottom) + appearance->getScreenPos(nullptr).y))
            {
                continue;
            }
        }
        else if (mouseX < appearance->upperLeft.x || appearance->lowerRight.x < mouseX ||
                 mouseY < appearance->upperLeft.y || appearance->lowerRight.y < mouseY)
        {
            continue;
        }

        if (gameObject->isDisabled() == 0 && gameObject->isDestroyed() == 0)
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

auto ObjectQueueNode::handleEvent(ObjectEvent* event) -> BaseObject*
{
    BaseObject* object = findObjectFromEvent(event, 0);

    if (object != nullptr)
    {
        object->handleEvent(event);
    }

    return object;
}

auto ObjectQueueNode::findObject(vector_3d position, float& distance) -> BaseObject*
{
    BaseObject* result = nullptr;

    for (BaseObject* object = head; object != nullptr; object = object->next)
    {
        if (static_cast<int32_t>(object->objectClass) <= 0)
        {
            continue;
        }

        auto* gameObject = static_cast<GameObject*>(object);

        if (gameObject->inTransport() != 0)
        {
            continue;
        }

        const float objectDistance = gameObject->distanceFrom(position);
        ObjectType* type = gameObject->getObjectType();
        const float extent = type != nullptr ? type->extentRadius : 0.0f;

        if (objectDistance < extent && objectDistance < distance)
        {
            distance = objectDistance;
            result = object;
        }
    }

    return result;
}

auto ObjectQueueNode::makeObjDataBlock(ObjData* data) -> int32_t
{
    std::memset(data, 0xff, 0x898);
    int32_t count = 0;

    for (BaseObject* object = head; object != nullptr; object = object->next)
    {
        count++;
        ObjectType* type = object->getObjectType();

        if (type == nullptr || object->objectClass != BUILDING)
        {
            data->objTypeNum = -1;
            data->blockNumber = 0;
            data->vertexNumber = 0;
            data->pixelOffsetY = 0;
            data->pixelOffsetX = 0;
            data->damage = 0;
        }
        else
        {
            auto* building = static_cast<Building*>(object);
            data->pixelOffsetX = static_cast<int16_t>(building->pixelOffsetX);
            data->objTypeNum = static_cast<int16_t>(type->objTypeNum);
            data->pixelOffsetY = static_cast<int16_t>(building->pixelOffsetY);
            data->blockNumber = static_cast<int16_t>(building->blockNumber);
            data->vertexNumber = static_cast<int16_t>(building->vertexNumber);
            data->damage = static_cast<uint8_t>(static_cast<int32_t>(building->getDamage()));
        }

        data++;
    }

    data->objTypeNum = -1;
    data->blockNumber = 0;
    data->vertexNumber = 0;
    data->pixelOffsetY = 0;
    data->pixelOffsetX = 0;
    data->damage = 0;
    return count;
}

auto ObjectQueue::addList(ObjectQueueNode* node) -> void
{
    if (node != nullptr)
    {
        node->next = nullptr;

        if (tail == nullptr)
        {
            tail = node;
            head = node;
            return;
        }

        tail->next = node;
        tail = node;
    }
}

auto ObjectQueue::findList(const char* listId) -> ObjectQueueNode*
{
    // The original compares up to eight characters, stopping at the end of listId.
    for (ObjectQueueNode* node = head; node != nullptr; node = node->next)
    {
        if (std::strncmp(listId, node->id, 8) == 0)
        {
            return node;
        }
    }

    return nullptr;
}

auto ObjectQueue::render() -> void
{
    for (ObjectQueueNode* node = head; node != nullptr; node = node->next)
    {
        if (node->head != nullptr)
        {
            node->render();
        }

        if (gRestartRender != 0 || MaxObjectsDrawn != 0)
        {
            return;
        }
    }
}

auto ObjectQueue::update() -> void
{
    objectsInList = 0;

    for (ObjectQueueNode* node = head; node != nullptr; node = node->next)
    {
        if (node->head != nullptr)
        {
            node->update();
        }
    }
}

auto ObjectQueue::findObjectFromEvent(ObjectEvent* event) -> BaseObject*
{
    ObjectQueueNode* node = head;
    BaseObject* result;

    if (homeTeam->id == -1)
    {
        result = innerSphereMechList->findObjectFromEvent(event, 1);
    }
    else
    {
        result = clanMechList->findObjectFromEvent(event, 1);
    }

    if (result == nullptr)
    {
        result = node->findObjectFromEvent(event, 1);

        for (; result == nullptr && node != nullptr; node = node->next)
        {
            if (node->head != nullptr)
            {
                result = node->findObjectFromEvent(event, 0);
            }
        }
    }

    return result;
}

auto ObjectQueue::handleEvent(ObjectEvent* event) -> BaseObject*
{
    BaseObject* result = nullptr;

    for (ObjectQueueNode* node = head; node != nullptr && result == nullptr; node = node->next)
    {
        if (node->head != nullptr)
        {
            result = node->handleEvent(event);
        }
    }

    return result;
}

auto ObjectQueue::handleEvent(uint32_t partId, ObjectEvent* event) -> BaseObject*
{
    BaseObject* object = findObjectFromPart(static_cast<int32_t>(partId));

    if (object != nullptr)
    {
        object->handleEvent(event);
        return object;
    }

    return nullptr;
}

auto ObjectQueue::findObject(vector_3d position) -> BaseObject*
{
    BaseObject* result = nullptr;
    float bestDistance = 100000.0f;

    for (ObjectQueueNode* node = head; node != nullptr; node = node->next)
    {
        if (node->head == nullptr)
        {
            continue;
        }

        float distance = 100000.0f;
        BaseObject* object = node->findObject(position, distance);

        if (object != nullptr && distance < bestDistance)
        {
            result = object;
            bestDistance = distance;
        }
    }

    return result;
}

auto ObjectQueue::findObjectId(int32_t typeId) -> BaseObject*
{
    for (ObjectQueueNode* node = head; node != nullptr; node = node->next)
    {
        for (BaseObject* object = node->head; object != nullptr; object = object->next)
        {
            ObjectType* type = object->getObjectType();

            if (type != nullptr && type->objTypeNum == typeId)
            {
                return object;
            }
        }
    }

    return nullptr;
}

auto ObjectQueue::findObjectFromPart(int32_t partId) -> BaseObject*
{
    if (partId == 0 || partId < 0x200)
    {
        return nullptr;
    }

    if (partId < 0x1000)
    {
        return getMoverFromPartId(partId);
    }

    const int32_t blockNum = (partId - 0x1000) / 0xc80;
    char listId[12];
    std::snprintf(listId, sizeof(listId), "TBlk%d", blockNum);
    ObjectQueueNode* node = FindListById(head, listId);

    if (node != nullptr && node->head != nullptr)
    {
        if (BaseObject* object = FindPart(node, partId))
        {
            return object;
        }
    }

    // Not in the block's TBlk list (movers, light walls): try its RBlk list (buildings and the rest).
    std::snprintf(listId, sizeof(listId), "RBlk%d", blockNum);
    node = FindListById(head, listId);

    if (node != nullptr && node->head != nullptr)
    {
        if (BaseObject* object = FindPart(node, partId))
        {
            return object;
        }
    }

    return head != nullptr ? FindPart(head, partId) : nullptr;
}

auto ObjectQueue::findObjectInGroup(BaseObject* current, int32_t groupId) -> BaseObject*
{
    // Steps from current (or the list's head) until test passes; null at the end.
    auto step = [current](ObjectQueueNode* list, auto test) -> BaseObject*
    {
        BaseObject* object = current == nullptr ? list->head : current->next;

        while (object != nullptr && !test(object))
        {
            object = object->next;
        }

        return object;
    };

    if (groupId == 0)
    {
        return nullptr;
    }

    if (groupId == 500)
    {
        return step(innerSphereMechList, [](BaseObject* object) { return object->underPlayerControl() != 0; });
    }

    if (groupId == 501)
    {
        return current != nullptr ? current->next : clanMechList->head;
    }

    if (groupId == 502)
    {
        return step(innerSphereMechList, [](BaseObject* object) { return object->underPlayerControl() == 0; });
    }

    if (groupId >= 1 && groupId <= 0x20)
    {
        return step(innerSphereMechList, [groupId](BaseObject* object) { return object->getGroupId() == groupId - 1; });
    }

    if (groupId >= 0x149 && groupId <= 0x168)
    {
        return step(innerSphereMechList,
                    [groupId](BaseObject* object) { return object->getGroupId() == groupId - 0x149; });
    }

    if (groupId >= 0xa5 && groupId <= 0xc4)
    {
        return step(clanMechList, [groupId](BaseObject* object) { return object->getGroupId() == groupId - 0xa5; });
    }

    return nullptr;
}

auto ObjectQueue::traverse(BaseObject*& current) -> BaseObject*
{
    BaseObject* result = nullptr;

    if (current == nullptr || (result = current->next) == nullptr)
    {
        ObjectQueueNode* node = head;

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
                BaseObject* last = node->tail;
                node = node->next;

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

        for (; node != nullptr && (result = node->head) == nullptr; node = node->next)
        {
        }
    }

    current = result;
    return result;
}

auto blockInList(int32_t blockNumber) -> int
{
    for (int32_t i = 0; i < MAX_BLOCK_LIST; i++)
    {
        if (usedBlockList[i] == blockNumber)
        {
            return 1;
        }

        if (usedBlockList[i] == -1)
        {
            break;
        }
    }

    return 0;
}
