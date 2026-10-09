#include "stdafx.h"
#include "logistics/MCLogDialogButton.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCUpdateDisplay.h"
#include "logistics/MCReusableDialog.h"

auto MCLogDialogButton::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) -> int32_t
{
    PressedDown = false;
    Result = 0;
    return MCLogButton::Init(xPos, yPos, width, height, name);
}

auto MCLogDialogButton::Draw() -> void
{
    DrawFace(Disabled ? GrayPicture.get() : (PressedDown ? DownPicture.get() : UpPicture.get()), true);
}

auto MCLogDialogButton::HandleEvent(MCGuiEvent* event) -> void
{
    if (Disabled)
    {
        return;
    }

    if (event->Type == MCGuiEventType::LeftButtonUp)
    {
        PressedDown = false;
    }

    if (event->Type == MCGuiEventType::LeftButtonDown)
    {
        // Show the press, then close the dialog with this button's result.
        PressedDown = true;
        UpdateDisplay(false, false, 0, false, 0);
        PlayLogSound(PressSample);
        Callback()->Execute();
        static_cast<MCReusableDialog*>(Parent)->Deactivate(Result);
    }

    if (EventRoutine != nullptr)
    {
        EventRoutine(this, event);
    }
}
