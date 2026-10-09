#include "stdafx.h"
#include "logistics/MCLogButton.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCUpdateDisplay.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>Loads <paramref name="fileName"/> into a new picture at <paramref name="picture"/>.</summary>
    void LoadPicture(std::unique_ptr<MCLogPort>& picture, std::string_view fileName)
    {
        picture = std::make_unique<MCLogPort>();
        picture->Load(fileName);
    }
}

MCLogButton::~MCLogButton()
{
    MCLogButton::Destroy();
}

auto MCLogButton::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, [[maybe_unused]] const char* name)
    -> int32_t
{
    const int32_t result = MCLogObject::Init(xPos, yPos, width, height);

    if (result != 0)
    {
        return result;
    }

    ButtonCallback.Clear();
    UpPicture.reset();
    DownPicture.reset();
    GrayPicture.reset();
    OverPicture.reset();
    Disabled = false;
    OverState = false;
    Pressed = false;
    BackgroundColor = 0;
    PressSound = 0xf;
    OverSound = 0xffffffff;
    return 0;
}

auto MCLogButton::Destroy() -> void
{
    UpPicture.reset();
    DownPicture.reset();
    GrayPicture.reset();
    OverPicture.reset();
    ButtonCallback.Clear();

    if (HeldButton == this)
    {
        HeldButton = nullptr;
    }

    MCLogObject::Destroy();
}

auto MCLogButton::Press() -> void
{
    LetGoPress();
    Pressed = true;
    HeldButton = this;
}

auto MCLogButton::LetGoPress() -> void
{
    if (HeldButton != nullptr)
    {
        HeldButton->Pressed = false;
        HeldButton = nullptr;
    }
}

auto MCLogButton::SetUpPicture(std::string_view fileName) -> void
{
    LoadPicture(UpPicture, fileName);
    // The button takes the picture's size.
    BackgroundColor = 0xff;
    Resize(UpPicture->Width(), UpPicture->Height());
}

auto MCLogButton::SetOverPicture(std::string_view fileName) -> void
{
    LoadPicture(OverPicture, fileName);
}

auto MCLogButton::SetGrayPicture(std::string_view fileName) -> void
{
    LoadPicture(GrayPicture, fileName);
}

auto MCLogButton::SetDownPicture(std::string_view fileName) -> void
{
    LoadPicture(DownPicture, fileName);
}

auto MCLogButton::HandleEvent(MCGuiEvent* event) -> void
{
    if (event->Type == MCGuiEventType::LeftButtonDown)
    {
        if (!Disabled)
        {
            // Shown pressed, and on screen before the callback runs.
            Press();
            PlayLogSound(PressSound);
            UpdateDisplay(false, false, 0, false, 0);
            ButtonCallback.Execute();
        }
        else
        {
            PlayLogSound(DisabledSound);
        }
    }
    else if (event->Type == MCGuiEventType::LeftButtonUp)
    {
        // The press shows until the button is let go (the original's next paint put the face back up).
        LetGoPress();
    }

    if (!Disabled && EventRoutine != nullptr)
    {
        EventRoutine(this, event);
    }
}

auto MCLogButton::Draw() -> void
{
    MCLogPort* picture = nullptr;

    if (Disabled)
    {
        picture = GrayPicture.get();
    }
    else if (!Pressed && (GuiSystem()->GrabbedObject() != this || GuiSystem()->CurrentObject() != this))
    {
        picture = OverState ? OverPicture.get() : UpPicture.get();
    }
    else
    {
        picture = DownPicture.get();
    }

    DrawFace(picture, false);
}

auto MCLogButton::DrawFace(MCLogPort* picture, bool keyed) -> void
{
    if (picture != nullptr)
    {
        picture->CopyTo(_Port->Frame(), 0, 0, keyed);
    }
    else
    {
        VfxPaneWipe(_Port->Frame(), static_cast<uint32_t>(BackgroundColor));
    }

    MCLogObject::Draw();
}

auto MCLogButton::Enter() -> void
{
    if (!Disabled)
    {
        OverState = true;

        if (HeldButton == this)
        {
            LetGoPress();
        }

        PlayLogSound(OverSound);
    }
}

auto MCLogButton::Leave() -> void
{
    if (OverState)
    {
        OverState = false;

        if (HeldButton == this)
        {
            LetGoPress();
        }
    }
}
