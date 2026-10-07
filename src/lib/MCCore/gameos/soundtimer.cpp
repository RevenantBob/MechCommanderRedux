#include "stdafx.h"
#include "gameos/soundtimer.h"
#include "lib/MCFatal.h"

namespace
{
    /// <summary>The port's multimedia timers: one thread each, by timer id.</summary>
    struct MCTimerThread
    {
        std::jthread Thread;
    };

    std::mutex TimerLock;
    std::unordered_map<uint32_t, std::unique_ptr<MCTimerThread>> Timers;
    uint32_t NextTimerId = 1;

    /// <summary>timeSetEvent(delay, resolution, TimeProc, user, TIME_PERIODIC).</summary>
    uint32_t StartTimer(uint32_t delay, uintptr_t user)
    {
        std::lock_guard<std::mutex> lock(TimerLock);
        uint32_t id = NextTimerId++;
        auto timer = std::make_unique<MCTimerThread>();
        timer->Thread = std::jthread(
            [id, delay, user](std::stop_token stop)
            {
                auto next = std::chrono::steady_clock::now();

                while (!stop.stop_requested())
                {
                    next += std::chrono::milliseconds(delay);
                    std::this_thread::sleep_until(next);

                    if (stop.stop_requested())
                    {
                        break;
                    }

                    MCSoundTimer::TimeProc(id, 0, user, 0, 0);
                }
            });
        Timers[id] = std::move(timer);
        return id;
    }

    /// <summary>timeKillEvent: stops the timer and waits for its thread.</summary>
    void KillTimer(uint32_t id)
    {
        std::unique_ptr<MCTimerThread> timer;
        {
            std::lock_guard<std::mutex> lock(TimerLock);
            auto found = Timers.find(id);

            if (found == Timers.end())
            {
                return;
            }

            timer = std::move(found->second);
            Timers.erase(found);
        }

        // Port fix: a timer killed from its own callback can't join itself; it is left to finish.
        if (timer->Thread.get_id() == std::this_thread::get_id())
        {
            timer->Thread.request_stop();
            timer->Thread.detach();
        }
    }
}

MCSoundTimer::MCSoundTimer()
{
    TimerId = 0;
}

MCSoundTimer::~MCSoundTimer()
{
    if (TimerId != 0)
    {
        KillTimer(TimerId);
    }
}

void MCSoundTimer::Create(uint32_t delay, uint32_t resolution, uintptr_t user, Callback callback)
{
    this->User = user;
    this->Delay = delay;
    this->Resolution = resolution;
    this->Handler = callback;
    // The timer's user value is the SoundTimer itself.
    TimerId = StartTimer(delay, reinterpret_cast<uintptr_t>(this));

    if (TimerId == 0)
    {
        Fatal(-1, "SoundTimer: Couldn't create timer");
    }
}

void MCSoundTimer::TimeProc(uint32_t timerId, uint32_t msg, uintptr_t user, uintptr_t dw1, uintptr_t dw2)
{
    MCSoundTimer* timer = reinterpret_cast<MCSoundTimer*>(user);
    timer->Handler(timer->User);
}
