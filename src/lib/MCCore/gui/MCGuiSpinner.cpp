#include "stdafx.h"
#include "gui/MCGuiSpinner.h"
#include "gui/MCGuiSystem.h"
#include "lib/MCFatal.h"

// aSpinnerButton

auto MCGuiSpinnerButton::HandleEvent(MCGuiEvent* event) -> void
{
    MCGuiSystem* gui = GuiSystem();

    switch (event->Type)
    {
        case MCGuiEventType::LeftButtonDown:
        {
            Pushed = true;
            gui->Grab(this);
            gui->AddTimer(this, 1, 1000, 0, 0, false);
            LeftCallback->Execute();
            break;
        }
        case MCGuiEventType::LeftButtonUp:
        {
            Pushed = false;
            gui->Release();
            gui->RemoveTimer(this, 1);
            gui->RemoveTimer(this, 2);
            break;
        }
        case MCGuiEventType::Timer:
        {
            // Timer 1 (the first second held) hands over to the repeating timer 2.
            if (event->Data == 1)
            {
                gui->RemoveTimer(this, 1);
                gui->AddTimer(this, 2, 250, 0, 0, false);
            }

            if (event->Data == 2)
            {
                LeftCallback->Execute();
            }
            break;
        }
    }

    MCGuiObject::HandleEvent(event);
}

auto MCGuiSpinnerButton::Draw() -> void
{
    if (MCGuiPort* picture = Pushed ? DownPicture.get() : UpPicture.get(); picture != nullptr)
    {
        picture->CopyTo(DisplayPort->Frame(), 0, 0, true);
    }

    MCGuiObject::Draw();
}

// aSpinner

MCGuiSpinner::~MCGuiSpinner()
{
    UpButton.reset();
    DownButton.reset();
}

auto MCGuiSpinner::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, [[maybe_unused]] const char* name)
    -> int32_t
{
    MCGuiObject* owner = Parent;

    if (owner == nullptr)
    {
        Fatal(0, "Hey Scott! You have to set the parent before the init! Remember?");
    }

    int32_t result = MCGuiObject::Init(xPos, yPos, width, height, nullptr);

    if (result != 0)
    {
        return result;
    }

    // The base's init clears the parent; put it back.
    SetParent(owner);
    UpButton = MCMakeGui<MCGuiSpinnerButton>();
    result = UpButton->Init(1, 1, 10, 10, nullptr);

    if (result != 0)
    {
        return result;
    }

    DownButton = MCMakeGui<MCGuiSpinnerButton>();
    result = DownButton->Init(1, 1, 10, 10, nullptr);

    if (result != 0)
    {
        return result;
    }

    UpButton->SetUpPicture(0xc);
    UpButton->SetDownPicture(0xd);
    DownButton->SetUpPicture(0x25);
    DownButton->SetDownPicture(0x26);
    Resize(std::max(UpButton->Width(), DownButton->Width()), UpButton->Height() + DownButton->Height());
    AddChild(UpButton.get());
    AddChild(DownButton.get());
    UpButton->MoveTo(0, 0);
    DownButton->MoveTo(0, UpButton->Height());
    UpButton->Callback()->SetMessage(Parent, MCGuiEventType::SpinUp);
    DownButton->Callback()->SetMessage(Parent, MCGuiEventType::SpinDown);
    return -1;
}

auto MCGuiSpinner::Destroy() -> void
{
    UpButton.reset();
    DownButton.reset();
    MCGuiObject::Destroy();
}
