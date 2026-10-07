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
        delete object;
    }
};

/// <summary>An owning pointer to a GUI object or port (destroyed by <see cref="MCGuiDestroy"/>).</summary>
template <typename T> using MCGuiOwned = std::unique_ptr<T, MCGuiDestroy>;

/// <summary>Makes a GUI object or port owned by an <see cref="MCGuiOwned"/>.</summary>
template <typename T, typename... Args> MCGuiOwned<T> MCMakeGui(Args&&... args)
{
    return MCGuiOwned<T>(new T(std::forward<Args>(args)...));
}
