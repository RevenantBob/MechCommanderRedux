#include "stdafx.h"
#include "gui/MCGuiCallback.h"
#include "gui/MCGuiSystem.h"

auto MCGuiCallback::Clear() -> void
{
    Exec = nullptr;
    Message = 0;
    Object.reset();
}

auto MCGuiCallback::Run(std::weak_ptr<MCGuiCallback> callback) -> void
{
    std::shared_ptr<MCGuiCallback> pinned = callback.lock();

    if (pinned == nullptr)
    {
        return;
    }

    if (pinned->Exec)
    {
        // A function can delete its own callback (the mech bar's dance deletes its callback). The original then read
        // the message and object from the freed block, which the destructor had zeroed, so it posted nothing (OB-109).
        // The function runs from a copy with the callback unpinned, so its holder's letting go ends it here.
        const std::function<void()> exec = pinned->Exec;
        pinned.reset();
        exec();
        pinned = callback.lock();

        if (pinned == nullptr)
        {
            return;
        }
    }

    if (const std::shared_ptr<MCGuiObject> object = pinned->Object.lock(); pinned->Message != 0 && object != nullptr)
    {
        APostMessage(*object, pinned->Message);
    }
}

auto MCGuiCallback::SetExec(std::function<void()> func) -> void
{
    Exec = std::move(func);
}

auto MCGuiCallback::Runs(void (&func)()) const -> bool
{
    return Exec.target_type() == typeid(void (*)()) && *Exec.target<void (*)()>() == &func;
}

auto MCGuiCallback::SetMessage(std::weak_ptr<MCGuiObject> obj, int32_t msg) -> void
{
    Message = msg;
    Object = std::move(obj);
}
