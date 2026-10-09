#pragma once

class MCGuiObject;

/// <summary>
/// Something to do when a button is pressed or a frame passes: a function, and/or a message posted to an object
/// (<see cref="APostMessage"/>).
/// </summary>
/// <remarks>Original source: <c>gui\asystem.cpp</c> (<c>aCallback</c>).</remarks>
class MCGuiCallback
{
public:
    MCGuiCallback() = default;
    virtual ~MCGuiCallback();
    MCGuiCallback(const MCGuiCallback&) = delete;
    MCGuiCallback& operator=(const MCGuiCallback&) = delete;

    /// <summary>
    /// Runs the callback: calls <see cref="Exec"/>, then posts <see cref="Message"/> to <see cref="Object"/> when
    /// both are set. The function may delete this callback; nothing is posted then (OB-109).
    /// </summary>
    void Execute();

    /// <summary>Makes the callback post <paramref name="msg"/> to <paramref name="obj"/>.</summary>
    void SetMessage(MCGuiObject* obj, int32_t msg);

    /// <summary>Clears the function and the message.</summary>
    void Clear();

    /// <summary>Sets the function to call.</summary>
    void SetExec(std::function<void()> func);

    /// <summary>Whether the function to call is <paramref name="func"/>.</summary>
    bool Runs(void (*func)()) const;

    /// <summary>The function to call, or empty.</summary>
    std::function<void()> Exec;
    /// <summary>The message to post, or 0.</summary>
    int32_t Message = 0;
    /// <summary>The object to post it to, or null.</summary>
    MCGuiObject* Object = nullptr;
};
