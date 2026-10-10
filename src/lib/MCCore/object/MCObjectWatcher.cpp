#include "stdafx.h"
#include "object/MCObjectWatcher.h"
#include "object/MCObjectSystem.h"

//---------------------------------------------------------------------------
// ObjectWatcherList
//---------------------------------------------------------------------------

auto MCObjectWatcherList::Watch(MCBaseObject** watcher) -> void
{
    _Watchers.push_back(watcher);
}

auto MCObjectWatcherList::RemoveAt(size_t index) -> void
{
    _Watchers[index] = _Watchers.back();
    _Watchers.pop_back();
}

auto MCObjectWatcherList::RemoveObject(MCBaseObject* obj) -> int32_t
{
    if (obj == nullptr)
    {
        return 0;
    }

    int32_t numRemoved = 0;
    size_t i = 0;

    while (i < _Watchers.size())
    {
        if (*_Watchers[i] == obj)
        {
            // Clear the watcher and move the last one into its slot, which is then checked again.
            *_Watchers[i] = nullptr;
            RemoveAt(i);
            numRemoved++;
        }
        else
        {
            i++;
        }
    }

    return numRemoved;
}

auto MCObjectWatcherList::RemoveWatch(MCBaseObject** watcher) -> bool
{
    for (size_t i = 0; i < _Watchers.size(); i++)
    {
        if (_Watchers[i] == watcher)
        {
            *_Watchers[i] = nullptr;
            RemoveAt(i);
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------------------------
// BaseObjectWatcher
//---------------------------------------------------------------------------

auto MCBaseObjectWatcher::Free() -> void
{
    if (Object != nullptr)
    {
        // Watchers can be freed at shutdown after the object system is gone.
        if (MCObjectWatcherList* watchers = ObjectWatchers())
        {
            watchers->RemoveWatch(&Object);
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
        ObjectWatchers()->Watch(&Object);
    }
}
