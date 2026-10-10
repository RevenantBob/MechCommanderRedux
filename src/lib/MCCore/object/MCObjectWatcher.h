#pragma once

class MCBaseObject;

/// <summary>
/// The registry of object watchers: the addresses of every <see cref="MCBaseObjectWatcher"/> pointer in use, so that a
/// destroyed object can null every pointer still aiming at it.
/// </summary>
/// <remarks>Original source: <c>object\objwtch.cpp</c>. Part of the <see cref="MCObjectSystem"/>.</remarks>
class MCObjectWatcherList
{
public:
    /// <summary>Registers the watched pointer <paramref name="watcher"/>.</summary>
    void Watch(MCBaseObject** watcher);
    /// <summary>Nulls and unregisters every watcher pointing at <paramref name="obj"/> (called as it is destroyed).</summary>
    /// <returns>The number of watchers cleared.</returns>
    int32_t RemoveObject(MCBaseObject* obj);
    /// <summary>Nulls and unregisters <paramref name="watcher"/>.</summary>
    /// <returns>Whether it was registered.</returns>
    bool RemoveWatch(MCBaseObject** watcher);
    /// <summary>Watchers registered.</summary>
    size_t Size() const { return _Watchers.size(); }

private:
    /// <summary>Removes entry <paramref name="index"/>; the last entry takes its place (the original's order).</summary>
    void RemoveAt(size_t index);

    /// <summary>The registered watchers: each the address of a pointer to an object.</summary>
    std::vector<MCBaseObject**> _Watchers;
};

/// <summary>
/// A pointer to an object that is nulled when the object is destroyed: it registers itself with the object system's
/// watchers while it points at something.
/// </summary>
/// <remarks>Original source: <c>object\objwtch.cpp</c>. The registry holds the address of <see cref="Object"/>, so a
/// watcher can't be copied; it unregisters itself when it goes.</remarks>
class MCBaseObjectWatcher
{
public:
    MCBaseObjectWatcher() = default;
    ~MCBaseObjectWatcher() { Free(); }
    MCBaseObjectWatcher(const MCBaseObjectWatcher&) = delete;
    MCBaseObjectWatcher& operator=(const MCBaseObjectWatcher&) = delete;

    /// <summary>Stops watching (unregisters and nulls the pointer).</summary>
    void Free();
    /// <summary>Watches <paramref name="obj"/> (null: watches nothing).</summary>
    void SetWatcher(MCBaseObject* obj);

    /// <summary>The watched object, or null once it is destroyed.</summary>
    MCBaseObject* Object = nullptr;
};
