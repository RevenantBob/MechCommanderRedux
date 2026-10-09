#include "stdafx.h"
#include "gui/MCGuiCallback.h"
#include "gui/MCGuiSystem.h"

namespace
{
    /// <summary>The callback whose function is running (<see cref="MCGuiCallback::Execute"/>), cleared when it is deleted.</summary>
    MCGuiCallback* ExecutingCallback = nullptr;
}

MCGuiCallback::~MCGuiCallback()
{
    if (this == ExecutingCallback)
    {
        ExecutingCallback = nullptr;
    }
}

auto MCGuiCallback::Clear() -> void
{
    Exec = nullptr;
    Message = 0;
    Object = nullptr;
}

auto MCGuiCallback::Execute() -> void
{
    if (Exec)
    {
        // A function can delete its own callback (the mech bar's dance deletes its callback). The original then read
        // the message and object from the freed block, which the destructor had zeroed, so it posted nothing (OB-109).
        // The function runs from a copy, so deleting the callback doesn't take the function away while it runs.
        const std::function<void()> exec = Exec;
        MCGuiCallback* outerCallback = ExecutingCallback;
        ExecutingCallback = this;
        exec();
        const bool deleted = ExecutingCallback != this;
        ExecutingCallback = outerCallback;

        if (deleted)
        {
            return;
        }
    }

    if (Message != 0 && Object != nullptr)
    {
        APostMessage(Object, Message);
    }
}

auto MCGuiCallback::SetExec(std::function<void()> func) -> void
{
    Exec = std::move(func);
}

auto MCGuiCallback::Runs(void (*func)()) const -> bool
{
    const auto* target = Exec.target<void (*)()>();
    return target != nullptr && *target == func;
}

auto MCGuiCallback::SetMessage(MCGuiObject* obj, int32_t msg) -> void
{
    Message = msg;
    Object = obj;
}
