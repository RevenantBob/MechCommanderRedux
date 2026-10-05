#include "stdafx.h"
#include "logistics/MCLogToolButton.h"
#include "gui/MCGuiEvent.h"
#include "platform/MCInput.h"

void LToolButtonEventHandler(MCGuiObject& object, MCGuiEvent& event)
{
    auto& button = static_cast<MCLogToolButton&>(object);

    if (event.Type == 1)
    {
        if (!button.Disabled)
        {
            button.Toggled = !button.Toggled;
            button.RunCallback();
        }
    }
    else if (event.Type == 4)
    {
        if (GuiSystem().GrabbedObject() != nullptr)
        {
            GuiSystem().Release();
        }
    }
}

void ChatTeamButtonEventHandler(MCGuiObject& object, MCGuiEvent& event)
{
    LToolButtonEventHandler(object, event);
    object.ParentObject()->HandleEvent(event);
}

void LScreenSwitchEventHandler(MCGuiObject& object, MCGuiEvent& event)
{
    auto& button = static_cast<MCLogToolButton&>(object);

    if (event.Type == 1 && !button.Disabled && !button.Toggled)
    {
        button.Toggled = true;
        button.RunCallback();
    }
}

auto MCLogToolButton::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, std::string_view name) -> int32_t
{
    Toggled = false;
    int32_t result = MCLogButton::Init(xPos, yPos, width, height, name);

    if (result == 0)
    {
        SetEventRoutine(LToolButtonEventHandler);
    }

    return result;
}

auto MCLogToolButton::HandleEvent(MCGuiEvent& event) -> void
{
    if (event.Type == 1)
    {
        // Original behaviour: the press sound is the fixed 0x33 unless an event routine is set.
        PlayLogSound(EventRoutine == nullptr ? DisabledSound : PressSound);
    }
    else if (event.Type != 4)
    {
        MCLogButton::HandleEvent(event);
        return;
    }

    MCGuiObject::HandleEvent(event);
}

auto MCLogToolButton::Draw() -> void
{
    std::shared_ptr<MCLogPort> picture;

    if (Disabled)
    {
        picture = GrayPicture;
    }
    else if (Toggled)
    {
        picture = DownPicture;
    }
    else if (OverState)
    {
        picture = OverPicture;
    }
    else
    {
        // The mouse can be over the button without overState (it was disabled when the mouse came).
        MCPoint cursor = MCInput::GetCursorPos();
        int32_t cursorX = cursor.x - GlobalX();
        int32_t cursorY = cursor.y - GlobalY();
        // OB-129 (fixed): the original tested only the right and bottom edges, so the cursor anywhere above or left
        // of the button counted as over it.
        const bool over = cursorX >= 0 && cursorY >= 0 && cursorX <= Width() && cursorY <= Height();
        picture = over ? OverPicture : UpPicture;
    }

    DrawFace(MCOptionalOf(picture), false);
}

auto MCLogSpinnerButton::HandleEvent(MCGuiEvent& event) -> void
{
    switch (event.Type)
    {
        case 1:
        {
            uint32_t sample = DisabledSound;

            if (!Disabled)
            {
                // Run the callback now, then repeat it after half a second held.
                Toggled = true;
                GuiSystem().Grab(weak_from_this());
                GuiSystem().AddTimer(weak_from_this(), DelayTimer, 500, 0, 0, false);
                MCGuiCallback::Run(ButtonCallback);
                sample = PressSound;
            }

            PlayLogSound(sample);
            break;
        }

        case 4:
        {
            Toggled = false;
            GuiSystem().Release();
            GuiSystem().RemoveTimer(*this, DelayTimer);
            GuiSystem().RemoveTimer(*this, RepeatTimer);
            break;
        }
        case 0x13:
        {
            if (event.Data == DelayTimer)
            {
                GuiSystem().RemoveTimer(*this, DelayTimer);
                GuiSystem().AddTimer(weak_from_this(), RepeatTimer, 100, 0, 0, false);
            }

            if (event.Data == RepeatTimer)
            {
                MCGuiCallback::Run(ButtonCallback);
            }
            break;
        }
        default:
            break;
    }

    MCGuiObject::HandleEvent(event);
}

auto MCLogSpinnerButton::Draw() -> void
{
    // Original behaviour (OB-130): the gray picture is never shown, so a disabled spinner looks enabled.
    const std::shared_ptr<MCLogPort> picture = Toggled ? DownPicture : UpPicture;

    if (picture != nullptr)
    {
        picture->CopyTo(_Port->Frame(), 0, 0, false);
    }

    MCLogObject::Draw();
}
