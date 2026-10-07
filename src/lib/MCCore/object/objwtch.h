#pragma once

class MCBaseObject;

/// <summary>
/// The registry of object watchers: the addresses of every <see cref="MCBaseObjectWatcher"/> pointer in use, so that a
/// destroyed object can null every pointer still aiming at it.
/// </summary>
/// <remarks>Original source: <c>object\objwtch.cpp</c>; 0xc bytes.</remarks>
class MCObjectWatcherList
{
public:
    /// <summary>Makes room for 200 watchers ; <paramref name="maxWatchers"/> is ignored.</summary>
    void Init(int32_t maxWatchers);
    /// <summary>Frees the watcher table.</summary>
    void Free();
    /// <summary>Registers the watched pointer <paramref name="watcher"/>; fatal when the table is full.</summary>
    void Watch(MCBaseObject** watcher);
    /// <summary>Nulls and unregisters every watcher pointing at <paramref name="obj"/> (called as it is destroyed).</summary>
    /// <returns>The number of watchers cleared.</returns>
    int32_t RemoveObject(MCBaseObject* obj);
    /// <summary>Nulls and unregisters <paramref name="watcher"/>.</summary>
    /// <returns>1 when it was registered.</returns>
    int32_t RemoveWatch(MCBaseObject** watcher);
    /// <summary>Nulls every registered watcher and empties the table.</summary>
    void Restart();

    /// <summary>Watchers registered.</summary>
    int32_t NumWatchers = 0;
    /// <summary>The table's size (200).</summary>
    int32_t MaxWatchers = 0;
    /// <summary>The registered watchers: each the address of a pointer to an object.</summary>
    std::unique_ptr<MCBaseObject**[]> Watchers;
};

/// <summary>
/// A pointer to an object that is nulled when the object is destroyed: it registers itself with
/// <c>objectWatchers</c> while it points at something.
/// </summary>
/// <remarks>Original source: <c>object\objwtch.cpp</c>; 4 bytes.</remarks>
class MCBaseObjectWatcher
{
public:
    /// <summary>Stops watching (unregisters and nulls the pointer).</summary>
    void Free();
    /// <summary>Watches <paramref name="obj"/> (null: watches nothing).</summary>
    void SetWatcher(MCBaseObject* obj);

    /// <summary>The watched object, or null once it is destroyed.</summary>
    MCBaseObject* Object = nullptr;
};

/// <summary>The one watcher registry.</summary>
extern MCObjectWatcherList* ObjectWatchers;
