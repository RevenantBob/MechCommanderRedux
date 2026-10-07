#pragma once

/// <summary>
/// A periodic multimedia timer (<c>timeSetEvent</c>) that calls a callback with a user value, for servicing
/// streaming channels. In the port it runs on a <c>std::thread</c>.
/// </summary>
/// <remarks>Original source: <c>game os\sound renderer\sound timer.cpp</c>; 0x14 bytes.</remarks>
class MCSoundTimer
{
public:
    /// <summary>The callback: gets the user value; the original ignores the result.</summary>
    using Callback = int (*)(uintptr_t user);

    /// <summary>No timer yet.</summary>
    MCSoundTimer();
    /// <summary>Kills the timer.</summary>
    ~MCSoundTimer();

    /// <summary>Starts a periodic timer of <paramref name="delay"/> ms (resolution <paramref name="resolution"/>)
    /// calling <paramref name="callback"/> with <paramref name="user"/>. Fatal when it can't.</summary>
    void Create(uint32_t delay, uint32_t resolution, uintptr_t user, Callback callback);
    /// <summary>The timer procedure: calls the callback with the user value.</summary>
    static void TimeProc(uint32_t timerId, uint32_t msg, uintptr_t user, uintptr_t dw1, uintptr_t dw2);

    /// <summary>The callback.</summary>
    Callback Handler = nullptr;
    /// <summary>The value passed to it (the channel). Port fix: a pointer went through this 32-bit value; the port
    /// keeps a pointer-sized value.</summary>
    uintptr_t User = 0;
    /// <summary>The period in ms.</summary>
    uint32_t Delay = 0;
    /// <summary>The resolution in ms.</summary>
    uint32_t Resolution = 0;
    /// <summary>The timer's id; 0 for none.</summary>
    uint32_t TimerId = 0;
};
