#include "stdafx.h"
#include "object/object.h"
#include "object/gameobj.h"
#include "object/objque.h"
#include "object/objtype.h"
#include "object/objwtch.h"

char DefaultListId[] = "DEFAULT";
char ClanmechListId[] = "CLANMEC";
char IsmechListId[] = "ISMECH";
char IconListId[] = "ICONS";
char WeaponListId[] = "WEAPON";

MCObjectQueue* ObjectList = nullptr;
MCObjectQueueNode* ClanMechList = nullptr;
MCObjectQueueNode* InnerSphereMechList = nullptr;
MCObjectQueueNode* IconList = nullptr;
MCObjectQueueNode* WeaponList = nullptr;
MCObjectTypeManager* ObjectTypeManager = nullptr;

namespace
{
    /// <summary>The list named <paramref name="listId"/>, made and appended (for no block) when missing.</summary>
    MCObjectQueueNode* FindOrAddList(const char* listId)
    {
        MCObjectQueueNode* node = ObjectList->FindList(listId);

        if (node == nullptr)
        {
            node = new MCObjectQueueNode;
            node->Init(listId, -1);
            ObjectList->AddList(node);
        }

        return node;
    }

    /// <summary>Deletes every list of <paramref name="queue"/> and empties it.</summary>
    void DeleteLists(MCObjectQueue* queue)
    {
        while (queue->Head != nullptr)
        {
            MCObjectQueueNode* node = queue->Head;
            MCObjectQueueNode* next = node->Next;
            node->Destroy();
            delete node;
            queue->Head = next;
        }

        queue->Tail = nullptr;
        queue->Head = nullptr;
    }
}

auto CreateObject(int32_t typeId) -> MCGameObject*
{
    MCGameObject* result = nullptr;

    if (typeId > -1)
    {
        result = static_cast<MCGameObject*>(ObjectTypeManager->Get(typeId));
    }

    return result;
}

auto DestroyObject(MCGameObject* object) -> void
{
    if (static_cast<uint32_t>(object->Kill()) == 0xbeaddead)
    {
        MCObjectQueueNode* node = ObjectList->Head;

        while (node != nullptr && node->Remove(object) == 0)
        {
            node = node->Next;
        }
    }
}

auto StartObjects(char* objectFileName, int32_t typeCacheSize, int32_t objectCacheSize, int32_t maxWatchers) -> int32_t
{
    int32_t result = 0;

    if (typeCacheSize < 0x180000)
    {
        typeCacheSize = 0x17ffff;
    }

    if (ObjectTypeManager == nullptr)
    {
        ObjectTypeManager = new MCObjectTypeManager;

        if (ObjectTypeManager == nullptr)
        {
            return static_cast<int32_t>(0xbeef0007);
        }

        result = ObjectTypeManager->Init(objectFileName, typeCacheSize, objectCacheSize);

        if (result != 0)
        {
            return result;
        }
    }

    if (ObjectList == nullptr)
    {
        MCObjectQueue* queue = new MCObjectQueue;
        MCObjectQueueNode* node = queue->FindList(DefaultListId);

        if (node == nullptr)
        {
            node = new MCObjectQueueNode(DefaultListId);
            queue->AddList(node);
        }

        queue->Tail = node;
        queue->Head = node;
        ObjectList = queue;

        ClanMechList = FindOrAddList(ClanmechListId);
        InnerSphereMechList = FindOrAddList(IsmechListId);
        IconList = FindOrAddList(IconListId);
        WeaponList = FindOrAddList(WeaponListId);
    }

    if (ObjectWatchers == nullptr)
    {
        ObjectWatchers = new MCObjectWatcherList;
        ObjectWatchers->Init(maxWatchers);
    }

    return result;
}

auto StopObjects() -> void
{
    if (ObjectList != nullptr)
    {
        // The original empties the queue twice (the second time an inlined ~ObjectQueue) before deleting it.
        DeleteLists(ObjectList);
        DeleteLists(ObjectList);
        delete ObjectList;
        ObjectList = nullptr;
    }

    if (ObjectWatchers != nullptr)
    {
        ObjectWatchers->Free();
        delete ObjectWatchers;
        ObjectWatchers = nullptr;
    }

    ClanMechList = nullptr;
    InnerSphereMechList = nullptr;
    IconList = nullptr;
}
