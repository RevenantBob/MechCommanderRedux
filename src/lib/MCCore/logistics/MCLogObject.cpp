#include "stdafx.h"
#include "logistics/MCLogObject.h"
#include "gui/MCGuiAnimation.h"
#include "platform/MCInput.h"
#include "sound/MCSoundSystem.h"
#include "vfx/MCVfxFunctions.h"

auto PlayLogSound(uint32_t sampleId) -> void
{
    // A test may run the controls without a sound system.
    if (MCSoundSystem* sound = SoundSystem(); sound != nullptr)
    {
        sound->PlayDigitalSample(sampleId, 1, nullptr, false, false);
    }
}

MCLogObject::~MCLogObject()
{
    MCLogObject::Destroy();
}

auto MCLogObject::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height) -> int32_t
{
    // The base's init, inlined with a logistics port for the port.
    const bool live = DrawsLive();
    InitWithoutPort(xPos, yPos, width, height);
    auto port = std::make_unique<MCLogPort>();
    const int32_t result = live ? port->InitView(width, height) : port->Init(width, height);
    SetOwnPort(std::move(port));
    return result;
}

auto MCLogObject::InitWithoutPort(int32_t xPos, int32_t yPos, int32_t width, int32_t height) -> void
{
    _Port = nullptr;
    _OwnedPort.reset();
    Transparent = false;
    Place(xPos, yPos, width, height);
    MCGuiObject::BackgroundPort.reset();
    _BackgroundPort.reset();
}

auto MCLogObject::SetOwnPort(std::unique_ptr<MCLogPort> port) -> void
{
    _OwnedPort = std::move(port);
    _Port = _OwnedPort.get();
}

auto MCLogObject::Destroy() -> void
{
    GuiSystem()->RemoveTimers(this);
    _Port = nullptr;
    _OwnedPort.reset();
    FramePane.reset();
    MCGuiObject::BackgroundPort.reset();
    _BackgroundPort.reset();
    IconAnimation.reset();
    WindowAnimation.reset();

    if (!ChildList.empty())
    {
        // Original behaviour (OB-071): removes the first child, then deletes whichever child is first after that (its
        // destroy removes it). With more children the first is therefore only unlinked; a lone first child is deleted
        // too, as the original's array still held it in its first slot.
        MCGuiObject* first = ChildList.front();
        const bool alone = ChildList.size() == 1;
        RemoveChild(first);

        if (alone)
        {
            MCGuiDestroy{}(first);
        }

        while (!ChildList.empty())
        {
            MCGuiObject* child = ChildList.front();
            MCGuiDestroy{}(child);

            // A child that didn't take itself off (not ours after all) is only unlinked.
            if (!ChildList.empty() && ChildList.front() == child)
            {
                ChildList.erase(ChildList.begin());
            }
        }
    }

    if (Parent != nullptr)
    {
        Parent->RemoveChild(this);
    }

    Parent = nullptr;
    Animating = 0;

    if (GuiSystem()->GrabbedObject() == this)
    {
        GuiSystem()->Release();
    }

    if (GuiSystem()->TextObject() == this)
    {
        GuiSystem()->ReleaseText();
    }

    if (GuiSystem()->ModalObject() == this)
    {
        GuiSystem()->ClearModal();
    }

    if (GuiSystem()->CurrentObject() == this)
    {
        const MCPoint cursor = MCInput::GetCursorPos();
        GuiSystem()->SetCurrentObject(ScreenWindow()->FindObject(cursor.x, cursor.y));
    }
}

auto MCLogObject::Draw() -> void
{
    const MCGuiWindowState state = WinState;

    if (state == MCGuiWindowState::Iconized)
    {
        IconAnimation->Draw(_Port->Frame(), 0, 0);
    }
    else
    {
        if (_BackgroundPort != nullptr)
        {
            _BackgroundPort->CopyTo(_Port->Frame(), 0, 0, true);
        }

        if (WindowAnimation != nullptr && Animating)
        {
            WindowAnimation->Draw(_Port->Frame(), 0, 0);
        }
    }

    if (state != MCGuiWindowState::Iconized)
    {
        Paint();

        for (size_t i = 0; i < ChildList.size(); i++)
        {
            DrawChild(ChildList[i]);
        }
    }
}

auto MCLogObject::Display() -> void
{
    if (!ShowWindow)
    {
        return;
    }

    if (IsHidden() != 0 && HideOffset == 0)
    {
        return;
    }

    // An object that draws itself does so after the slide has moved it.
    if (!DrawsLive())
    {
        if (WinState == MCGuiWindowState::Iconized)
        {
            if (IconAnimation != nullptr)
            {
                Draw();
            }
        }
        else if (WindowAnimation != nullptr)
        {
            WindowAnimation->Draw(_Port->Frame(), 0, 0);
            Draw();
        }
    }

    if (HideOffset != 0)
    {
        // A slide (HideMe) moves the whole offset each frame until the object is off the screen, or back home.
        if (HideDirection == MCDirection::Left || HideDirection == MCDirection::Right)
        {
            MoveTo(X() + HideOffset, Y(), true);
        }
        else
        {
            MoveTo(X(), Y() + HideOffset, true);
        }

        if (Hidden)
        {
            if (RectIntersect(0, 0, GuiSystem()->Width(), GuiSystem()->Height()) == 0)
            {
                HideOffset = 0;
            }
        }
        else
        {
            bool home = false;

            if (HideOffset < 0)
            {
                home = HomeX >= GlobalX() && HomeY >= GlobalY();
            }
            else if (HideOffset > 0)
            {
                home = HomeX <= GlobalX() && HomeY <= GlobalY();
            }

            if (home)
            {
                const int32_t homeYOffset = HomeY - Parent->GlobalY();
                MoveTo(HomeX - Parent->GlobalX(), homeYOffset, true);
                HideOffset = 0;
            }
        }
    }

    if (DrawsLive() && _Port != nullptr)
    {
        DrawInFramePass(_Port);
        return;
    }

    if (_Port != nullptr)
    {
        _Port->CopyTo(FramePane.get(), 0, 0, Transparent);
    }

    if (WinState != MCGuiWindowState::Iconized)
    {
        for (size_t i = 0; i < ChildList.size(); i++)
        {
            ChildList[i]->Display();
        }
    }
}

auto MCLogObject::Resize(int32_t width, int32_t height) -> void
{
    if (width > 0 && height > 0 && (width != WinWidth || height != WinHeight))
    {
        if (_Port != nullptr)
        {
            _Port->Resize(width, height);
        }

        WinWidth = width;
        WinHeight = height;
        FramePane->X1 = FramePane->X0 - 1 + width;
        FramePane->Y1 = FramePane->Y0 - 1 + height;
    }
}

auto MCLogObject::FillBox(int16_t left, int16_t top, int16_t right, int16_t bottom, uint8_t color) -> void
{
    // The rectangle is in the port's bitmap coordinates (the pane's own origin is not added).
    MCPane box = *_Port->Frame();
    box.X0 = left;
    box.Y0 = top;
    box.X1 = right;
    box.Y1 = bottom;
    VfxPaneWipe(&box, color);
}

auto MCLogObject::SetBackground(std::string_view fileName) -> int32_t
{
    _BackgroundPort = std::make_unique<MCLogPort>();
    _BackgroundPort->Load(fileName);
    return 0;
}
