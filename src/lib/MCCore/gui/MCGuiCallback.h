#pragma once

class MCGuiObject;

/// <summary>
/// Something to do when a button is pressed or a frame passes: a function, and/or a message posted to an object
/// (<see cref="APostMessage"/>).
/// </summary>
/// <remarks>
/// Original source: <c>gui\asystem.cpp</c> (<c>aCallback</c>). Its holder keeps it by <c>shared_ptr</c>; the GUI
/// system's frame list refers to it by <c>weak_ptr</c>.
/// </remarks>
class MCGuiCallback
{
public:
    MCGuiCallback() = default;
    virtual ~MCGuiCallback() = default;
    MCGuiCallback(const MCGuiCallback&) = delete;
    MCGuiCallback& operator=(const MCGuiCallback&) = delete;

    /// <summary>
    /// Runs <paramref name="callback"/> (nothing when it is gone): calls its <see cref="Exec"/>, then posts its
    /// <see cref="Message"/> to its <see cref="Object"/> when both are set. The function may delete its own callback
    /// (its holder lets go); nothing is posted then (OB-109). The function runs from a copy, unpinned.
    /// </summary>
    static void Run(std::weak_ptr<MCGuiCallback> callback);

    /// <summary>Makes the callback post <paramref name="msg"/> to <paramref name="obj"/>.</summary>
    void SetMessage(std::weak_ptr<MCGuiObject> obj, int32_t msg);

    /// <summary>Clears the function and the message.</summary>
    void Clear();

    /// <summary>Sets the function to call.</summary>
    void SetExec(std::function<void()> func);

    /// <summary>Whether the function to call is <paramref name="func"/>.</summary>
    bool Runs(void (&func)()) const;

    /// <summary>The function to call, or empty.</summary>
    std::function<void()> Exec;
    /// <summary>The message to post, or 0.</summary>
    int32_t Message = 0;
    /// <summary>The object to post it to, or none.</summary>
    std::weak_ptr<MCGuiObject> Object;
};
