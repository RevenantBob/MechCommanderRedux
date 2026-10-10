#pragma once

/// <summary>
/// A <typeparamref name="T"/> made in place and never destroyed, for a function-local static that code may still reach
/// while the process ends (static destructors, threads).
/// </summary>
template <typename T> class MCNeverDestroyed
{
public:
    /// <summary>Makes the value from <paramref name="args"/>.</summary>
    template <typename... Args> explicit MCNeverDestroyed(Args&&... args)
    {
        std::construct_at(&_Value, std::forward<Args>(args)...);
    }

    /// <summary>Leaves the value alive.</summary>
    ~MCNeverDestroyed() {}

    MCNeverDestroyed(const MCNeverDestroyed&) = delete;
    MCNeverDestroyed& operator=(const MCNeverDestroyed&) = delete;

    /// <summary>The value.</summary>
    T& operator*() { return _Value; }

    /// <summary>The value's members.</summary>
    T* operator->() { return &_Value; }

private:
    /// <summary>The value (a union member, so no destructor runs for it).</summary>
    union
    {
        T _Value;
    };
};
