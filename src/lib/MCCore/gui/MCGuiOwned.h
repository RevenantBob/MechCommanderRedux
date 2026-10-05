#pragma once

/// <summary>
/// Deletes a GUI object or port the way the GUI expects: its (virtual) <c>Destroy</c> first, which takes it off its
/// parent and frees its ports, then the object.
/// </summary>
struct MCGuiDestroy
{
    template <typename T> void operator()(T* object) const
    {
        object->Destroy();
        std::default_delete<T>()(object);
    }
};

/// <summary>
/// The owner's share of a GUI object or port (destroyed by <see cref="MCGuiDestroy"/> when the owner and every pin let
/// go). Parents, the GUI system's grab, focus and modal slots, timers and callbacks refer to it by <c>weak_ptr</c>.
/// </summary>
template <typename T> using MCGuiOwned = std::shared_ptr<T>;

/// <summary>Makes a GUI object or port owned by an <see cref="MCGuiOwned"/> (its deleter <see cref="MCGuiDestroy"/>).</summary>
template <typename T, typename... Args> MCGuiOwned<T> MCMakeGui(Args&&... args)
{
    std::unique_ptr<T, MCGuiDestroy> made(std::make_unique<T>(std::forward<Args>(args)...).release());
    return MCGuiOwned<T>(std::move(made));
}

/// <summary>
/// Whether <paramref name="first"/> and <paramref name="second"/> refer to the same object: the same control block, so
/// it also holds once the object's last share is gone. Two references to nothing are not the same.
/// </summary>
template <typename T, typename U> bool MCSameObject(const std::weak_ptr<T>& first, const std::weak_ptr<U>& second)
{
    const std::weak_ptr<T> none;
    const bool refers = first.owner_before(none) || none.owner_before(first);
    return refers && !first.owner_before(second) && !second.owner_before(first);
}

/// <summary>
/// Whether <paramref name="reference"/> refers to <paramref name="object"/> (by its control block, so it also holds
/// while the object is being destroyed). False for an object no <c>shared_ptr</c> owns.
/// </summary>
template <typename T, typename U> bool MCRefersTo(const std::weak_ptr<T>& reference, const U& object)
{
    return MCSameObject(object.weak_from_this(), reference);
}
