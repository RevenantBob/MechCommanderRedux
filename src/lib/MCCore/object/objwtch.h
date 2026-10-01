#pragma once

class BaseObject;

/// <summary>
/// The registry of object watchers: the addresses of every <see cref="BaseObjectWatcher"/> pointer in use, so that a
/// destroyed object can null every pointer still aiming at it.
/// </summary>
/// <remarks>Original source: <c>object\objwtch.cpp</c>; 0xc bytes.</remarks>
class ObjectWatcherList
{
public:
    /// <summary>Makes room for 200 watchers (from <c>systemHeap</c>); <paramref name="maxWatchers"/> is ignored.</summary>
    /// <remarks>MCX.EXE @ 0x00690bd0</remarks>
    void init(int32_t maxWatchers);
    /// <summary>Frees the watcher table.</summary>
    /// <remarks>MCX.EXE @ 0x00690c00</remarks>
    void free();
    /// <summary>Registers the watched pointer <paramref name="watcher"/>; fatal when the table is full.</summary>
    /// <remarks>MCX.EXE @ 0x00690c20</remarks>
    void watch(BaseObject** watcher);
    /// <summary>Nulls and unregisters every watcher pointing at <paramref name="obj"/> (called as it is destroyed).</summary>
    /// <returns>The number of watchers cleared.</returns>
    /// <remarks>MCX.EXE @ 0x00690c60 (unnamed in Ghidra)</remarks>
    int32_t removeObject(BaseObject* obj);
    /// <summary>Nulls and unregisters <paramref name="watcher"/>.</summary>
    /// <returns>1 when it was registered.</returns>
    /// <remarks>MCX.EXE @ 0x00690cd0</remarks>
    int32_t removeWatch(BaseObject** watcher);
    /// <summary>Nulls every registered watcher and empties the table.</summary>
    /// <remarks>MCX.EXE @ 0x00690d20</remarks>
    void restart();

    /// <summary>Watchers registered.</summary>
    int32_t numWatchers = 0; // +0x00
    /// <summary>The table's size (200).</summary>
    int32_t maxWatchers = 0; // +0x04
    /// <summary>The registered watchers: each the address of a pointer to an object.</summary>
    BaseObject*** watchers = nullptr; // +0x08
};

/// <summary>
/// A pointer to an object that is nulled when the object is destroyed: it registers itself with
/// <c>objectWatchers</c> while it points at something.
/// </summary>
/// <remarks>Original source: <c>object\objwtch.cpp</c>; 4 bytes.</remarks>
class BaseObjectWatcher
{
public:
    /// <summary>Stops watching (unregisters and nulls the pointer).</summary>
    /// <remarks>MCX.EXE @ 0x00690d50</remarks>
    void free();
    /// <summary>Watches <paramref name="obj"/> (null: watches nothing).</summary>
    /// <remarks>MCX.EXE @ 0x00690d70</remarks>
    void setWatcher(BaseObject* obj);

    /// <summary>The watched object, or null once it is destroyed.</summary>
    BaseObject* object = nullptr; // +0x00
};

/// <summary>The one watcher registry.</summary>
extern ObjectWatcherList* objectWatchers;
