#pragma once

#include "gui/MCGuiCallback.h"

class MCGuiObject;

/// <summary>A running GUI timer: sends a timer event (or a given event, once) to an object every interval.</summary>
/// <remarks>Original source: <c>gui\asystem.cpp</c> (<c>aTimer</c>).</remarks>
struct MCGuiTimer
{
    /// <summary>The interval in milliseconds.</summary>
    uint32_t Interval = 0;
    /// <summary>When it last fired (ms, or scenario ms for a scenario-time timer).</summary>
    uint32_t LastTime = 0;
    MCGuiObject* Target = nullptr;
    /// <summary>The id passed back in the timer event's <see cref="MCGuiEvent::Data"/>.</summary>
    int16_t Id = 0;
    /// <summary>The event type to send instead of a timer event (then the timer fires once), or 0.</summary>
    int32_t EventType = 0;
    /// <summary>The <see cref="MCGuiEvent::Data"/> of that event.</summary>
    int32_t EventData = 0;
    /// <summary>Whether it counts in scenario time instead of real time.</summary>
    bool UseScenarioTime = false;
};

/// <summary>
/// The GUI timers, run by a frame callback (<see cref="RunTimers"/>) while any exist. Removing a timer while one's
/// event is being handled is put off until the handler returns, unless it is the running timer.
/// </summary>
/// <remarks>Original source: <c>gui\asystem.cpp</c> (<c>aTimerManager</c>).</remarks>
class MCGuiTimerManager
{
public:
    /// <summary>Makes the frame callback that runs the timers.</summary>
    MCGuiTimerManager();
    /// <summary>Frees the timers and takes the frame callback off the GUI system.</summary>
    ~MCGuiTimerManager();
    MCGuiTimerManager(const MCGuiTimerManager&) = delete;
    MCGuiTimerManager& operator=(const MCGuiTimerManager&) = delete;

    /// <summary>Adds a timer unless one with the same object, id, interval and event exists.</summary>
    /// <returns>0, or -1 when there is one.</returns>
    int32_t AddUniqueTimer(MCGuiObject* target, int16_t id, uint32_t interval, int32_t eventType, int32_t eventData,
                           bool useScenarioTime);
    /// <summary>Adds a timer; the first one adds the frame callback to the GUI system.</summary>
    /// <returns>0.</returns>
    int32_t AddTimer(MCGuiObject* target, int16_t id, uint32_t interval, int32_t eventType, int32_t eventData,
                     bool useScenarioTime);
    /// <summary>Removes every timer of <paramref name="target"/>.</summary>
    void RemoveTimers(MCGuiObject* target);
    /// <summary>Removes <paramref name="target"/>'s timer <paramref name="id"/>.</summary>
    void RemoveTimer(MCGuiObject* target, int16_t id);
    /// <summary>Removes timer number <paramref name="index"/>.</summary>
    void RemoveTimer(int32_t index);
    /// <summary>Timer number <paramref name="index"/>, or null.</summary>
    MCGuiTimer* GetTimer(int32_t index);
    /// <summary><paramref name="target"/>'s timer <paramref name="id"/>, or null.</summary>
    MCGuiTimer* GetTimer(MCGuiObject* target, int16_t id);
    /// <summary>How many timers run.</summary>
    int32_t NumTimers() const { return static_cast<int32_t>(_Timers.size()); }

    /// <summary>
    /// Fires the due timers: a timer event with the timer's id, or the timer's own event (after which the timer goes,
    /// unless the handler changed the timers).
    /// </summary>
    void RunTimers();

    /// <summary>The frame callback that runs the timers.</summary>
    MCGuiCallback RunTimersCallback;

private:
    /// <summary>A put-off removal: object and id, object and -1 (all its timers), or null and an index.</summary>
    struct TimerToWhack
    {
        MCGuiObject* Target = nullptr;
        int32_t Id = 0;
    };

    /// <summary>Puts removals off while <paramref name="running"/>'s event is handled (it may remove itself).</summary>
    void LockTimersExcept(MCGuiTimer* running);
    /// <summary>Does the put-off removals, last first.</summary>
    void UnlockTimers();
    /// <summary>Whether removing <paramref name="timer"/> is put off now.</summary>
    bool PutOff(const MCGuiTimer* timer) const { return _Locked && timer != _LockedExcept; }
    /// <summary>Removes timer number <paramref name="index"/> now; the last one takes the frame callback away.</summary>
    void Erase(size_t index);

    std::vector<std::unique_ptr<MCGuiTimer>> _Timers;
    std::vector<TimerToWhack> _TimersToWhack;
    bool _Locked = false;
    /// <summary>The timer whose event is being handled while locked.</summary>
    MCGuiTimer* _LockedExcept = nullptr;
};
