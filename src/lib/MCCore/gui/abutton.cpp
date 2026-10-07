#include "stdafx.h"
#include "gui/abutton.h"
#include "gui/aport.h"
#include "lib/MCFatal.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>Destroys and deletes a picture (destroy through the vtable first: aPort has no virtual destructor).</summary>
    void FreePicture(MCGuiPort*& picture)
    {
        if (picture != nullptr)
        {
            picture->Destroy();
            delete picture;
            picture = nullptr;
        }
    }

    /// <summary>Destroys and deletes a callback.</summary>
    void FreeCallback(MCGuiCallback*& callback)
    {
        if (callback != nullptr)
        {
            callback->Destroy();
            delete callback;
            callback = nullptr;
        }
    }

    /// <summary>
    /// Replaces <paramref name="picture"/> with one loaded from <paramref name="source"/> (a file name or an art
    /// packet). With <paramref name="sizeButton"/> the button takes the picture's size and loses its back colour
    /// (0xff); a picture that fails to load is dropped.
    /// </summary>
    template <typename Source>
    void LoadPicture(MCGuiButton* button, MCGuiPort*& picture, Source source, bool sizeButton)
    {
        FreePicture(picture);
        MCGuiPort* port = new MCGuiPort;
        picture = port;

        if (port->Init(source) == 0)
        {
            if (sizeButton)
            {
                button->BackgroundColor = 0xff;
                button->Resize(port->Width(), port->Height());
            }

            return;
        }

        delete port;
        picture = nullptr;
    }

    /// <summary>
    /// Whether a mouse event lies on <paramref name="button"/> (its rectangle in its parent's coordinates). (The
    /// original also redrew the button for a left-button release; the button draws itself each frame.)
    /// </summary>
    bool EventOnButton(MCGuiButton* button, MCGuiEvent* event)
    {
        RECT rect;
        rect.left = button->X();
        rect.top = button->Y();
        rect.right = button->X() + button->Width();
        rect.bottom = button->Y() + button->Height();
        MCGuiObject* parent = button->Parent;
        const int32_t parentX = parent->GlobalX();
        const int32_t eventX = event->X;
        const int32_t parentY = parent->GlobalY();
        const int32_t eventY = event->Y;
        POINT point;
        point.x = eventX - parentX;
        point.y = eventY - parentY;
        return PtInRect(&rect, point) != 0;
    }
}

// aButton

auto MCGuiButton::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    int32_t result = MCGuiObject::Init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    UpPicture = nullptr;
    DownPicture = nullptr;
    GrayPicture = nullptr;
    LeftCallback = new MCGuiCallback;
    RightButtonCallback = new MCGuiCallback;
    Disabled = 0;
    Framed = -1;
    BackgroundColor = 0;
    VfxPaneWipe(DisplayPort->Frame(), 0);
    return 0;
}

auto MCGuiButton::Destroy() -> void
{
    FreePicture(UpPicture);
    FreePicture(DownPicture);
    FreePicture(GrayPicture);
    FreeCallback(LeftCallback);
    FreeCallback(RightButtonCallback);
    MCGuiObject::Destroy();
}

auto MCGuiButton::SetUpPicture(char* fileName) -> void
{
    LoadPicture(this, UpPicture, fileName, true);
}

auto MCGuiButton::SetGrayPicture(char* fileName) -> void
{
    LoadPicture(this, GrayPicture, fileName, true);
}

auto MCGuiButton::SetDownPicture(char* fileName) -> void
{
    LoadPicture(this, DownPicture, fileName, false);
}

auto MCGuiButton::SetUpPicture(int32_t artPacket) -> void
{
    LoadPicture(this, UpPicture, artPacket, true);
}

auto MCGuiButton::SetGrayPicture(int32_t artPacket) -> void
{
    LoadPicture(this, GrayPicture, artPacket, true);
}

auto MCGuiButton::SetDownPicture(int32_t artPacket) -> void
{
    LoadPicture(this, DownPicture, artPacket, false);
}

auto MCGuiButton::HandleEvent(MCGuiEvent* event) -> void
{
    if (Disabled != 0)
    {
        return;
    }

    switch (event->Type)
    {
        case 1:
        {
            Application->Grab(this);
            break;
        }
        case 3:
            Application->Grab(this);
            break;
        case 4:
        {
            if (Application->GrabbedObject() == this)
            {
                Application->Release();

                if (EventOnButton(this, event))
                {
                    LeftCallback->Execute();
                }
            }
            break;
        }
        case 6:
        {
            if (Application->GrabbedObject() == this)
            {
                Application->Release();

                if (EventOnButton(this, event))
                {
                    RightButtonCallback->Execute();
                }
            }
            break;
        }
    }

    if (EventRoutine != nullptr)
    {
        EventRoutine(this, event);
    }
}

auto MCGuiButton::Draw() -> void
{
    MCGuiPort* picture;

    if (Disabled != 0)
    {
        if (Framed != 0)
        {
            DrawFramed(0, -1);
        }

        picture = GrayPicture;
    }
    else if (Application->GrabbedObject() == this)
    {
        if (Framed != 0)
        {
            DrawFramed(-1, -1);
        }

        picture = DownPicture;
    }
    else
    {
        if (Framed != 0)
        {
            DrawFramed(0, -1);
        }

        picture = UpPicture;
    }

    // The frame is drawn first, so an opaque picture covers it.
    if (picture != nullptr)
    {
        picture->CopyTo(DisplayPort->Frame(), 0, 0, 0);
        MCGuiObject::Draw();
        return;
    }

    VfxPaneWipe(DisplayPort->Frame(), BackgroundColor);
    MCGuiObject::Draw();
}

// aCloseButton

auto MCGuiCloseButton::HandleEvent(MCGuiEvent* event) -> void
{
    if (Disabled != 0)
    {
        return;
    }

    if (event->Type == 4 && Application->GrabbedObject() == this)
    {
        Application->Release();

        if (EventOnButton(this, event))
        {
            // The window is closing: the event routine isn't run.
            LeftCallback->Execute();
            return;
        }
    }

    const int32_t type = event->Type;

    if (type == 1)
    {
        Application->Grab(this);
    }
    else if (type == 3)
    {
        Application->Grab(this);
    }
    else if (type == 6 && Application->GrabbedObject() == this)
    {
        Application->Release();

        if (EventOnButton(this, event))
        {
            RightButtonCallback->Execute();
        }
    }

    if (EventRoutine != nullptr)
    {
        EventRoutine(this, event);
    }
}

// aToolButton

auto MCGuiToolButton::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    Pushed = 0;
    return MCGuiButton::Init(xPos, yPos, width, height, name);
}

auto MCGuiToolButton::HandleEvent(MCGuiEvent* event) -> void
{
    if (event->Type == 1 && Disabled == 0)
    {
        Pushed = Pushed == 0 ? 1 : 0;
        LeftCallback->Execute();
        MCGuiObject::HandleEvent(event);
        return;
    }

    MCGuiButton::HandleEvent(event);
}

auto MCGuiToolButton::Draw() -> void
{
    if (Disabled != 0)
    {
        if (Framed != 0)
        {
            DrawFramed(0, -1);
        }

        if (GrayPicture != nullptr)
        {
            GrayPicture->CopyTo(DisplayPort->Frame(), 0, 0, -1);
            MCGuiObject::Draw();
            return;
        }

        VfxPaneWipe(DisplayPort->Frame(), BackgroundColor);
        MCGuiObject::Draw();
        return;
    }

    // Pictures are drawn transparently, then the frame over them (unfilled).
    if (Pushed != 0)
    {
        if (DownPicture != nullptr)
        {
            DownPicture->CopyTo(DisplayPort->Frame(), 0, 0, -1);

            if (Framed != 0)
            {
                DrawFramed(-1, 0);
            }
        }
        else if (Framed != 0)
        {
            DrawFramed(-1, -1);
        }
    }
    else
    {
        if (UpPicture != nullptr)
        {
            UpPicture->CopyTo(DisplayPort->Frame(), 0, 0, -1);

            if (Framed != 0)
            {
                DrawFramed(0, 0);
            }
        }
        else if (Framed != 0)
        {
            DrawFramed(0, -1);
        }
    }

    MCGuiObject::Draw();
}

// aSpinnerButton

auto MCGuiSpinnerButton::HandleEvent(MCGuiEvent* event) -> void
{
    switch (event->Type)
    {
        case 1:
        {
            Pushed = -1;
            Application->Grab(this);
            Application->AddTimer(this, 1, 1000, 0, 0, 0);
            LeftCallback->Execute();
            break;
        }
        case 4:
        {
            Pushed = 0;
            Application->Release();
            Application->RemoveTimer(this, 1);
            Application->RemoveTimer(this, 2);
            break;
        }
        case 0x13:
        {
            // Timer 1 (the first second held) hands over to the repeating timer 2.
            const int32_t timer = event->Data;

            if (timer == 1)
            {
                Application->RemoveTimer(this, 1);
                Application->AddTimer(this, 2, 0xfa, 0, 0, 0);
            }

            if (timer == 2)
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
    if (Pushed != 0)
    {
        if (DownPicture != nullptr)
        {
            DownPicture->CopyTo(DisplayPort->Frame(), 0, 0, -1);
        }
    }
    else if (UpPicture != nullptr)
    {
        UpPicture->CopyTo(DisplayPort->Frame(), 0, 0, -1);
    }

    MCGuiObject::Draw();
}

// aSpinner

auto MCGuiSpinner::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    (void)name;
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

    // aObject::init clears the parent; put it back.
    SetParent(owner);

    auto* up = new MCGuiSpinnerButton;
    UpButton = up;

    if (up == nullptr)
    {
        return 3;
    }

    result = up->Init(1, 1, 10, 10, nullptr);

    if (result != 0)
    {
        return result;
    }

    auto* down = new MCGuiSpinnerButton;
    DownButton = down;

    if (down == nullptr)
    {
        return 3;
    }

    result = down->Init(1, 1, 10, 10, nullptr);

    if (result != 0)
    {
        return result;
    }

    up->SetUpPicture(0xc);
    up->SetDownPicture(0xd);
    down->SetUpPicture(0x25);
    down->SetDownPicture(0x26);
    const int32_t newWidth = down->Width() < up->Width() ? up->Width() : down->Width();
    Resize(newWidth, up->Height() + down->Height());
    AddChild(UpButton);
    AddChild(DownButton);
    UpButton->MoveTo(0, 0, 0);
    DownButton->MoveTo(0, UpButton->Height(), 0);
    UpButton->Callback()->SetMessage(Parent, 0x15);
    DownButton->Callback()->SetMessage(Parent, 0x16);
    return -1;
}

auto MCGuiSpinner::Destroy() -> void
{
    if (UpButton != nullptr)
    {
        UpButton->Destroy();
        delete UpButton;
        UpButton = nullptr;
    }

    if (DownButton != nullptr)
    {
        DownButton->Destroy();
        delete DownButton;
        DownButton = nullptr;
    }

    MCGuiObject::Destroy();
}
