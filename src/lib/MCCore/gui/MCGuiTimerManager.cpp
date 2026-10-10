#include "stdafx.h"
#include "gui/MCGuiTimerManager.h"
#include "gui/MCGuiInput.h"
#include "gui/MCGuiSystem.h"
#include "lib/MCFatal.h"
#include "main/MCMissionGlobals.h"

MCGuiTimerManager::MCGuiTimerManager()
{
    RunTimersCallback.SetExec([this] { RunTimers(); });
}

MCGuiTimerManager::~MCGuiTimerManager()
{
    _Timers.clear();

    if (MCGuiSystem* gui = GuiSystem(); gui != nullptr)
    {
        gui->RemoveCallback(&RunTimersCallback);
    }
}

auto MCGuiTimerManager::AddUniqueTimer(MCGuiObject* target, int16_t id, uint32_t interval, int32_t eventType,
                                       int32_t eventData, bool useScenarioTime) -> int32_t
{
    for (const std::unique_ptr<MCGuiTimer>& timer : _Timers)
    {
        if (timer->Target == target && timer->Id == id && timer->Interval == interval &&
            timer->EventType == eventType && timer->EventData == eventData)
        {
            return -1;
        }
    }

    return AddTimer(target, id, interval, eventType, eventData, useScenarioTime);
}

auto MCGuiTimerManager::AddTimer(MCGuiObject* target, int16_t id, uint32_t interval, int32_t eventType,
                                 int32_t eventData, bool useScenarioTime) -> int32_t
{
    if (_Timers.empty())
    {
        GuiSystem()->AddCallback(&RunTimersCallback);
    }

    auto timer = std::make_unique<MCGuiTimer>();
    timer->Target = target;
    timer->Id = id;
    timer->Interval = interval;
    timer->LastTime = useScenarioTime
                          ? static_cast<uint32_t>(static_cast<int32_t>(static_cast<double>(ScenarioTime) * 1000.0))
                          : MCPort::Milliseconds();
    timer->EventType = eventType;
    timer->EventData = eventData;
    timer->UseScenarioTime = useScenarioTime;
    _Timers.push_back(std::move(timer));
    return 0;
}

auto MCGuiTimerManager::Erase(size_t index) -> void
{
    _Timers.erase(_Timers.begin() + static_cast<ptrdiff_t>(index));

    if (_Timers.empty())
    {
        GuiSystem()->RemoveCallback(&RunTimersCallback);
    }
}

auto MCGuiTimerManager::RemoveTimers(MCGuiObject* target) -> void
{
    for (size_t i = 0; i < _Timers.size();)
    {
        MCGuiTimer* timer = _Timers[i].get();

        if (timer->Target != target)
        {
            i++;
            continue;
        }

        if (PutOff(timer))
        {
            // Put off once per matching timer.
            _TimersToWhack.push_back({target, -1});
            i++;
            continue;
        }

        Erase(i);
    }
}

auto MCGuiTimerManager::RemoveTimer(MCGuiObject* target, int16_t id) -> void
{
    const auto found = std::ranges::find_if(_Timers, [&](const std::unique_ptr<MCGuiTimer>& timer)
                                            { return timer->Id == id && timer->Target == target; });

    if (found == _Timers.end())
    {
        return;
    }

    if (PutOff(found->get()))
    {
        _TimersToWhack.push_back({target, id});
        return;
    }

    Erase(static_cast<size_t>(found - _Timers.begin()));
}

auto MCGuiTimerManager::RemoveTimer(int32_t index) -> void
{
    if (index < 0 || index >= NumTimers())
    {
        return;
    }

    if (PutOff(_Timers[static_cast<size_t>(index)].get()))
    {
        _TimersToWhack.push_back({nullptr, index});
        return;
    }

    Erase(static_cast<size_t>(index));
}

auto MCGuiTimerManager::GetTimer(int32_t index) -> MCGuiTimer*
{
    return index >= 0 && index < NumTimers() ? _Timers[static_cast<size_t>(index)].get() : nullptr;
}

auto MCGuiTimerManager::GetTimer(MCGuiObject* target, int16_t id) -> MCGuiTimer*
{
    const auto found = std::ranges::find_if(_Timers, [&](const std::unique_ptr<MCGuiTimer>& timer)
                                            { return timer->Id == id && timer->Target == target; });
    return found != _Timers.end() ? found->get() : nullptr;
}

auto MCGuiTimerManager::LockTimersExcept(MCGuiTimer* running) -> void
{
    _Locked = true;
    _LockedExcept = running;
}

auto MCGuiTimerManager::UnlockTimers() -> void
{
    _Locked = false;

    while (!_TimersToWhack.empty())
    {
        const TimerToWhack whack = _TimersToWhack.back();
        _TimersToWhack.pop_back();
        MCMouseThreadLock lock;

        if (whack.Target != nullptr && whack.Id != -1)
        {
            RemoveTimer(whack.Target, static_cast<int16_t>(whack.Id));
        }
        else if (whack.Target != nullptr)
        {
            RemoveTimers(whack.Target);
        }
        else if (whack.Id != -1)
        {
            RemoveTimer(whack.Id);
        }
        else
        {
            Fatal(static_cast<int32_t>(_TimersToWhack.size()) + 1, " Illegal timersToWhack structure!");
        }
    }
}

auto MCGuiTimerManager::RunTimers() -> void
{
    const uint32_t now = MCPort::Milliseconds();
    int32_t count = NumTimers();

    // The index runs on past a timer removed by its own handler, so the timer after it waits for the next frame.
    for (int32_t i = 0; i < NumTimers(); i++)
    {
        MCGuiTimer* timer = GetTimer(i);
        const uint32_t due = timer->LastTime + timer->Interval;

        if (timer->UseScenarioTime)
        {
            if (!(static_cast<double>(due) < static_cast<double>(ScenarioTime) * 1000.0))
            {
                continue;
            }
        }
        else if (now <= due)
        {
            continue;
        }

        const tagPOINT cursor = GetMessageCursorLoc();
        MCGuiEvent event;
        event.X = cursor.x;
        event.Y = cursor.y;
        const bool oneShot = timer->EventType != 0;
        event.Data = oneShot ? timer->EventData : timer->Id;
        event.Type = oneShot ? timer->EventType : MCGuiEventType::Timer;
        LockTimersExcept(timer);
        timer->Target->HandleEvent(&event);
        UnlockTimers();
        const int32_t numTimers = NumTimers();

        if (oneShot)
        {
            // A one-shot event: sent, then the timer goes, unless the handler changed the timers.
            if (count == numTimers)
            {
                MCMouseThreadLock lock;
                RemoveTimer(i);
            }
            else
            {
                count = numTimers;
            }

            continue;
        }

        // The time is set unless exactly one timer went (taken to be this one). Faithful: when the handler removed a
        // timer other than this one, this one's time is set too. The original also wrote the time into this timer
        // when the handler had removed it along with others; that write into the freed timer is skipped.
        const bool alive = std::ranges::any_of(_Timers, [timer](const auto& kept) { return kept.get() == timer; });

        if (count - 1 != numTimers && alive)
        {
            timer->LastTime = now;
        }

        count = numTimers;
    }
}
