#include "stdafx.h"
#include "gameos/soundtimer.h"
#include "lib/aerror.h"

namespace
{
    /// <summary>The port's multimedia timers: one thread each, by timer id.</summary>
    struct TimerThread
    {
        std::jthread Thread;
    };

    std::mutex timerLock;
    std::unordered_map<uint32_t, std::unique_ptr<TimerThread>> timers;
    uint32_t nextTimerId = 1;

    /// <summary>timeSetEvent(delay, resolution, TimeProc, user, TIME_PERIODIC).</summary>
    uint32_t startTimer(uint32_t delay, uintptr_t user)
    {
        std::lock_guard<std::mutex> lock(timerLock);
        uint32_t id = nextTimerId++;
        auto timer = std::make_unique<TimerThread>();
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

                    SoundTimer::TimeProc(id, 0, user, 0, 0);
                }
            });
        timers[id] = std::move(timer);
        return id;
    }

    /// <summary>timeKillEvent: stops the timer and waits for its thread.</summary>
    void killTimer(uint32_t id)
    {
        std::unique_ptr<TimerThread> timer;
        {
            std::lock_guard<std::mutex> lock(timerLock);
            auto found = timers.find(id);

            if (found == timers.end())
            {
                return;
            }

            timer = std::move(found->second);
            timers.erase(found);
        }

        // Port fix: a timer killed from its own callback can't join itself; it is left to finish.
        if (timer->Thread.get_id() == std::this_thread::get_id())
        {
            timer->Thread.request_stop();
            timer->Thread.detach();
        }
    }
}

SoundTimer::SoundTimer()
{
    timerId = 0;
}

SoundTimer::~SoundTimer()
{
    if (timerId != 0)
    {
        killTimer(timerId);
    }
}

void SoundTimer::Create(uint32_t delay, uint32_t resolution, uintptr_t user, Callback callback)
{
    this->user = user;
    this->delay = delay;
    this->resolution = resolution;
    this->callback = callback;
    // The timer's user value is the SoundTimer itself.
    timerId = startTimer(delay, reinterpret_cast<uintptr_t>(this));

    if (timerId == 0)
    {
        Fatal(-1, "SoundTimer: Couldn't create timer");
    }
}

void SoundTimer::TimeProc(uint32_t timerId, uint32_t msg, uintptr_t user, uintptr_t dw1, uintptr_t dw2)
{
    SoundTimer* timer = reinterpret_cast<SoundTimer*>(user);
    timer->callback(timer->user);
}
