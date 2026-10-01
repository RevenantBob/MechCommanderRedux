#include "stdafx.h"
#include "object/object.h"
#include "object/gameobj.h"
#include "object/objque.h"
#include "object/objtype.h"
#include "object/objwtch.h"

char DEFAULT_LIST_ID[] = "DEFAULT";
char CLANMECH_LIST_ID[] = "CLANMEC";
char ISMECH_LIST_ID[] = "ISMECH";
char ICON_LIST_ID[] = "ICONS";
char WEAPON_LIST_ID[] = "WEAPON";

ObjectQueue* objectList = nullptr;
ObjectQueueNode* clanMechList = nullptr;
ObjectQueueNode* innerSphereMechList = nullptr;
ObjectQueueNode* iconList = nullptr;
ObjectQueueNode* weaponList = nullptr;
ObjectTypeManager* objectTypeManager = nullptr;

namespace
{
    /// <summary>The list named <paramref name="listId"/>, made and appended (for no block) when missing.</summary>
    ObjectQueueNode* FindOrAddList(const char* listId)
    {
        ObjectQueueNode* node = objectList->findList(listId);

        if (node == nullptr)
        {
            node = new ObjectQueueNode;
            node->init(listId, -1);
            objectList->addList(node);
        }

        return node;
    }

    /// <summary>Deletes every list of <paramref name="queue"/> and empties it.</summary>
    void DeleteLists(ObjectQueue* queue)
    {
        while (queue->head != nullptr)
        {
            ObjectQueueNode* node = queue->head;
            ObjectQueueNode* next = node->next;
            node->destroy();
            delete node;
            queue->head = next;
        }

        queue->tail = nullptr;
        queue->head = nullptr;
    }
}

auto createObject(int32_t typeId) -> GameObject*
{
    GameObject* result = nullptr;

    if (typeId > -1)
    {
        result = static_cast<GameObject*>(objectTypeManager->get(typeId));
    }

    return result;
}

auto destroyObject(GameObject* object) -> void
{
    if (static_cast<uint32_t>(object->kill()) == 0xbeaddead)
    {
        ObjectQueueNode* node = objectList->head;

        while (node != nullptr && node->remove(object) == 0)
        {
            node = node->next;
        }
    }
}

auto startObjects(char* objectFileName, int32_t typeCacheSize, int32_t objectCacheSize, int32_t maxWatchers) -> int32_t
{
    int32_t result = 0;

    if (typeCacheSize < 0x180000)
    {
        typeCacheSize = 0x17ffff;
    }

    if (objectTypeManager == nullptr)
    {
        objectTypeManager = new ObjectTypeManager;

        if (objectTypeManager == nullptr)
        {
            return static_cast<int32_t>(0xbeef0007);
        }

        result = objectTypeManager->init(objectFileName, typeCacheSize, objectCacheSize);

        if (result != 0)
        {
            return result;
        }
    }

    if (objectList == nullptr)
    {
        ObjectQueue* queue = new ObjectQueue;
        ObjectQueueNode* node = queue->findList(DEFAULT_LIST_ID);

        if (node == nullptr)
        {
            node = new ObjectQueueNode(DEFAULT_LIST_ID);
            queue->addList(node);
        }

        queue->tail = node;
        queue->head = node;
        objectList = queue;

        clanMechList = FindOrAddList(CLANMECH_LIST_ID);
        innerSphereMechList = FindOrAddList(ISMECH_LIST_ID);
        iconList = FindOrAddList(ICON_LIST_ID);
        weaponList = FindOrAddList(WEAPON_LIST_ID);
    }

    if (objectWatchers == nullptr)
    {
        objectWatchers = new ObjectWatcherList;
        objectWatchers->init(maxWatchers);
    }

    return result;
}

auto stopObjects() -> void
{
    if (objectList != nullptr)
    {
        // The original empties the queue twice (the second time an inlined ~ObjectQueue) before deleting it.
        DeleteLists(objectList);
        DeleteLists(objectList);
        delete objectList;
        objectList = nullptr;
    }

    if (objectWatchers != nullptr)
    {
        objectWatchers->free();
        delete objectWatchers;
        objectWatchers = nullptr;
    }

    clanMechList = nullptr;
    innerSphereMechList = nullptr;
    iconList = nullptr;
}
