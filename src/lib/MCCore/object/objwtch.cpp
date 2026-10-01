#include "stdafx.h"
#include "object/objwtch.h"
#include "lib/aerror.h"
#include "lib/heap.h"

ObjectWatcherList* objectWatchers = nullptr;

//---------------------------------------------------------------------------
// ObjectWatcherList
//---------------------------------------------------------------------------

auto ObjectWatcherList::init(int32_t) -> void
{
    // Faithful: the size asked for is ignored; the list always holds 200.
    maxWatchers = 200;
    watchers = static_cast<BaseObject***>(systemHeap->malloc(200 * sizeof(BaseObject**)));
}

auto ObjectWatcherList::free() -> void
{
    systemHeap->free(watchers);
    watchers = nullptr;
}

auto ObjectWatcherList::watch(BaseObject** watcher) -> void
{
    if (numWatchers < maxWatchers)
    {
        watchers[numWatchers] = watcher;
        numWatchers++;
        return;
    }

    Fatal(static_cast<int32_t>(0xfeef0002), " Out of Object Watchers ", nullptr);
}

auto ObjectWatcherList::removeObject(BaseObject* obj) -> int32_t
{
    if (obj == nullptr)
    {
        return 0;
    }

    int32_t numRemoved = 0;
    int32_t i = 0;

    while (i < numWatchers)
    {
        if (*watchers[i] == obj)
        {
            // Clear the watcher and move the last one into its slot, which is then checked again.
            *watchers[i] = nullptr;
            numWatchers--;
            watchers[i] = watchers[numWatchers];
            numRemoved++;
        }
        else
        {
            i++;
        }
    }

    return numRemoved;
}

auto ObjectWatcherList::removeWatch(BaseObject** watcher) -> int32_t
{
    for (int32_t i = 0; i < numWatchers; i++)
    {
        if (watchers[i] == watcher)
        {
            numWatchers--;
            *watchers[i] = nullptr;
            watchers[i] = watchers[numWatchers];
            return 1;
        }
    }

    return 0;
}

auto ObjectWatcherList::restart() -> void
{
    for (int32_t i = 0; i < numWatchers; i++)
    {
        if (watchers[i] != nullptr)
        {
            *watchers[i] = nullptr;
        }
    }

    numWatchers = 0;
}

//---------------------------------------------------------------------------
// BaseObjectWatcher
//---------------------------------------------------------------------------

auto BaseObjectWatcher::free() -> void
{
    if (object != nullptr)
    {
        // Port fix: watchers can be freed at shutdown after objectWatchers is gone.
        if (objectWatchers != nullptr)
        {
            objectWatchers->removeWatch(&object);
        }

        object = nullptr;
    }
}

auto BaseObjectWatcher::setWatcher(BaseObject* obj) -> void
{
    free();
    object = obj;

    if (obj != nullptr)
    {
        objectWatchers->watch(&object);
    }
}
