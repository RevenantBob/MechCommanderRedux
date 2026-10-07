#include "stdafx.h"
#include "object/objwtch.h"
#include "lib/aerror.h"

MCObjectWatcherList* ObjectWatchers = nullptr;

//---------------------------------------------------------------------------
// ObjectWatcherList
//---------------------------------------------------------------------------

auto MCObjectWatcherList::Init(int32_t) -> void
{
    // Faithful: the size asked for is ignored; the list always holds 200.
    MaxWatchers = 200;
    Watchers = std::make_unique<MCBaseObject**[]>(200);
}

auto MCObjectWatcherList::Free() -> void
{
    Watchers.reset();
}

auto MCObjectWatcherList::Watch(MCBaseObject** watcher) -> void
{
    if (NumWatchers < MaxWatchers)
    {
        Watchers[NumWatchers] = watcher;
        NumWatchers++;
        return;
    }

    Fatal(static_cast<int32_t>(0xfeef0002), " Out of Object Watchers ", nullptr);
}

auto MCObjectWatcherList::RemoveObject(MCBaseObject* obj) -> int32_t
{
    if (obj == nullptr)
    {
        return 0;
    }

    int32_t numRemoved = 0;
    int32_t i = 0;

    while (i < NumWatchers)
    {
        if (*Watchers[i] == obj)
        {
            // Clear the watcher and move the last one into its slot, which is then checked again.
            *Watchers[i] = nullptr;
            NumWatchers--;
            Watchers[i] = Watchers[NumWatchers];
            numRemoved++;
        }
        else
        {
            i++;
        }
    }

    return numRemoved;
}

auto MCObjectWatcherList::RemoveWatch(MCBaseObject** watcher) -> int32_t
{
    for (int32_t i = 0; i < NumWatchers; i++)
    {
        if (Watchers[i] == watcher)
        {
            NumWatchers--;
            *Watchers[i] = nullptr;
            Watchers[i] = Watchers[NumWatchers];
            return 1;
        }
    }

    return 0;
}

auto MCObjectWatcherList::Restart() -> void
{
    for (int32_t i = 0; i < NumWatchers; i++)
    {
        if (Watchers[i] != nullptr)
        {
            *Watchers[i] = nullptr;
        }
    }

    NumWatchers = 0;
}

//---------------------------------------------------------------------------
// BaseObjectWatcher
//---------------------------------------------------------------------------

auto MCBaseObjectWatcher::Free() -> void
{
    if (Object != nullptr)
    {
        // Port fix: watchers can be freed at shutdown after objectWatchers is gone.
        if (ObjectWatchers != nullptr)
        {
            ObjectWatchers->RemoveWatch(&Object);
        }

        Object = nullptr;
    }
}

auto MCBaseObjectWatcher::SetWatcher(MCBaseObject* obj) -> void
{
    Free();
    Object = obj;

    if (obj != nullptr)
    {
        ObjectWatchers->Watch(&Object);
    }
}
