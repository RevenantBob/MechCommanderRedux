#include "stdafx.h"
#include "gui/awindow.h"
#include "camera/MCCamera.h"
#include "engine/MCFont.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "lib/MCFatal.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFile.h"
#include "lib/MCPacketFile.h"
#include "logistics/logmain.h"
#include "network/multplyr.h"
#include "platform/MCInput.h"
#include "platform/MCSmacker.h"
#include "sound/soundsys.h"
#include "terrain/MCTerrain.h"
#include "vfx/MCVfxFunctions.h"
#include "platform/MCRenderer.h"

int32_t StartupRects[24] = {};
int32_t NoiseSample = 0;
int MovieOver = 0;

namespace
{
    /// <summary>Destroys and frees a frame part, and clears the pointer.</summary>
    template <typename T> void DestroyPart(T*& part)
    {
        if (part != nullptr)
        {
            part->Destroy();
            delete part;
            part = nullptr;
        }
    }

    /// <summary>Makes a plain frame part: a new aObject, fatal when out of memory.</summary>
    MCGuiObject* NewPart(const char* outOfMemory)
    {
        MCGuiObject* part = new MCGuiObject;

        if (part == nullptr)
        {
            Fatal(0, outOfMemory);
        }

        // Port: a part only shows its paint routine or background, so it draws itself each frame.
        part->SetDrawsLive();
        return part;
    }

    /// <summary>Snaps a size to the 40-pixel grid of a grid-aligned window, rounding to the nearest step.</summary>
    void SnapToGrid(int32_t& newWidth, int32_t& newHeight)
    {
        if (newWidth % 0x28 > 0x13)
        {
            newWidth += 0x28;
        }

        newWidth -= newWidth % 0x28;

        if (newWidth == 0)
        {
            newWidth = 0x28;
        }

        if (newHeight % 0x28 > 0x13)
        {
            newHeight += 0x28;
        }

        newHeight -= newHeight % 0x28;
    }

    /// <summary>
    /// Lays out a button bar (aToolBar and aWindowBar share the code): the bar is sized to the buttons, then the
    /// buttons fill rows (horizontal) or columns of <paramref name="maxLength"/>.
    /// </summary>
    template <typename TButton>
    void PlaceBarButtons(MCGuiObject* bar, TButton* const* buttons, int32_t numButtons, int16_t maxLength,
                         int horizontal, int32_t buttonWidth, int32_t buttonHeight)
    {
        const int32_t length = maxLength;
        const int32_t across = numButtons < length ? numButtons : length;
        int16_t lines = static_cast<int16_t>(numButtons / length);

        if (numButtons % length != 0)
        {
            lines++;
        }

        int16_t columns = lines;
        int32_t barHeight = static_cast<int16_t>(across) * buttonHeight;

        if (horizontal != 0)
        {
            barHeight = lines * buttonHeight;
            columns = static_cast<int16_t>(across);
        }

        bar->Resize(columns * buttonWidth, barHeight);
        int16_t column = 0;
        int16_t row = 0;

        for (int16_t i = 0; i < numButtons; i++)
        {
            buttons[i]->MoveTo(column * buttonWidth, row * buttonHeight, 0);

            if (horizontal == 0)
            {
                row++;

                if (row == maxLength)
                {
                    row = 0;
                    column++;
                }
            }
            else
            {
                column++;

                if (column == maxLength)
                {
                    column = 0;
                    row++;
                }
            }
        }
    }

    /// <summary>
    /// Keeps a dragged window's title bar inside the scroll rectangle: when the bar has left it, the window is moved
    /// back against the side it crossed.
    /// </summary>
    void KeepTitleBarInside(MCGuiTitleBar* bar, const tagRECT& area)
    {
        MCGuiObject* window = bar->Parent;

        if (bar->RectIntersect(area) != 0)
        {
            return;
        }

        if (area.right < bar->Frame()->X0)
        {
            window->MoveTo(area.right, window->Frame()->Y0, 0);
        }

        if (bar->Frame()->X0 + bar->Width() < area.left)
        {
            window->MoveTo(area.left - bar->Width(), window->Frame()->Y0, 0);
        }

        if (area.bottom < bar->Frame()->Y0)
        {
            window->MoveTo(bar->Frame()->X0, area.bottom + 0xd, 0);
        }

        if (bar->Frame()->Y0 + bar->Height() < area.top)
        {
            window->MoveTo(bar->Frame()->X0, area.top, 0);
        }
    }

    /// <summary>Clears the whole screen buffer (a full-screen movie's background).</summary>
    void ClearScreen()
    {
        // Port: the original cleared 640x480 bytes (0x96000 in 16-bit mode) of the surface; the port wipes the
        // screen at its real size, through the renderer.
        MCPane screen{ScreenPort->Frame()->Window, 0, 0, ScreenPort->Frame()->Window->XMax,
                      ScreenPort->Frame()->Window->YMax};
        VfxPaneWipe(&screen, 0);
    }

    /// <summary>Remaps a windowed movie onto the game's current palette (<c>SmackColorRemap</c>).</summary>
    void RemapToGamePalette(MCSmackTag* movie)
    {
        MCVfxRgb palette[256];
        std::memcpy(palette, Application->CurrentPalette, sizeof(palette));
        movie->Player->ColorRemap(reinterpret_cast<const uint8_t*>(palette), 0x100);
    }
}

// Frame paint routines.

auto TitleBarPaint(MCGuiObject* object) -> void
{
    VfxPaneWipe(object->Port()->Frame(), 0x1b);
    VfxLineDraw(object->Port()->Frame(), 0, 6, object->Width() - 1, 6, 0x10);
    VfxLineDraw(object->Port()->Frame(), 0, 7, object->Width() - 1, 7, 0x10);
}

auto CameraBottomBarPaint(MCGuiObject* object) -> void
{
    VfxPaneWipe(object->Port()->Frame(), 0x10);
    VfxLineDraw(object->Port()->Frame(), 0, 0, 0, 1, 0x15);
    VfxLineDraw(object->Port()->Frame(), 1, 0, 1, 1, 0x10);
}

auto CameraLeftBarPaint(MCGuiObject* object) -> void
{
    VfxLineDraw(object->Port()->Frame(), 0, 0, 0, object->Height(), 0x15);
    VfxLineDraw(object->Port()->Frame(), 1, 0, 1, object->Height(), 0x10);
}

auto CameraRightBarPaint(MCGuiObject* object) -> void
{
    VfxPaneWipe(object->Port()->Frame(), 0x10);
}

auto BottomBarPaint(MCGuiObject* object) -> void
{
    object->SetBit(0, 0, 0xe);
    object->SetBit(object->Width(), 0, 0xe);
    VfxLineDraw(object->Port()->Frame(), 1, 0, object->Width(), 0, 3);
    VfxLineDraw(object->Port()->Frame(), 0, 1, object->Width(), 1, 0xe);
}

auto LeftBarPaint(MCGuiObject* object) -> void
{
    object->SetBit(object->Width(), object->Height(), 0xe);
    VfxLineDraw(object->Port()->Frame(), 0, 0, 0, object->Height(), 0xe);
    VfxLineDraw(object->Port()->Frame(), 1, 0, 1, object->Height() - 1, 3);
}

auto RightBarPaint(MCGuiObject* object) -> void
{
    object->SetBit(0, object->Height(), 0xe);
    VfxPaneWipe(object->Port()->Frame(), 8);
    VfxLineDraw(object->Port()->Frame(), 0, 0, 0, object->Height() - 2, 3);
    VfxLineDraw(object->Port()->Frame(), 1, 0, 1, object->Height() - 1, 0xe);
}

auto HandleResizeButtonEvent(MCGuiObject* object, MCGuiEvent* event) -> void
{
    if (object->Parent == nullptr)
    {
        return;
    }

    // The window is the handle's parent's parent (the handle sits in the right frame bar).
    MCGuiObject* window = object->Parent->Parent;

    if (window == nullptr)
    {
        return;
    }

    switch (event->Type)
    {
        case 1: // left button down
        {
            MCInput::SetCapture();
            Application->Grab(object);
            break;
        }
        case 4: // left button up
        {
            Application->Release();
            MCInput::ReleaseCapture();
            window->Draw();
            break;
        }
        case 7: // mouse move
        {
            if (Application->GrabbedObject() == object)
            {
                window->Resize(event->X - window->GlobalX(), event->Y - window->GlobalY());
                window->Draw();
            }
            break;
        }
    }
}

auto HandleSwoopyButtonEvent(MCGuiObject* object, MCGuiEvent* event) -> void
{
    if (event->Type == 1)
    {
        Application->Grab(object);
        object->Draw();
    }
    else if (event->Type == 4 && Application->GrabbedObject() == object && object->Parent != nullptr)
    {
        MCGuiObject* window = object->Parent->Parent;

        if (window != nullptr && window->GetCamera() != nullptr)
        {
            window->GetCamera()->Swoopy = !window->GetCamera()->Swoopy;
            Application->Release();
            object->Draw();
        }
    }
}

// aTitleButton

auto MCGuiTitleButton::HandleEvent(MCGuiEvent* event) -> void
{
    MCGuiObject::HandleEvent(event);
}

// aTitleWindow

MCGuiTitleWindow::MCGuiTitleWindow()
{
}

MCGuiTitleWindow::~MCGuiTitleWindow()
{
    MCGuiTitleWindow::Destroy();
}

auto MCGuiTitleWindow::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    int32_t result = MCGuiObject::Init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    SetTransparent(0);

    TitleBar = new MCGuiTitleBar;

    if (TitleBar == nullptr)
    {
        Fatal(0, "Not enough memory to allocate titlebar");
    }

    result = TitleBar->Init(0, 0, width + 0xc, 0xd, name);

    if (result != 0)
    {
        return result;
    }

    AddChild(TitleBar);
    TitleBar->ShowCloseButton(1);
    TitleBar->MoveTo(-2, -0xd, 0);
    TitleBar->SetZoomCallbacks();

    LeftBar = NewPart("Not enough memory to allocate left bar");
    // The left bar's init result is not checked.
    LeftBar->Init(0, 0, 2, height, nullptr);
    LeftBar->SetPaintRoutine(LeftBarPaint);
    AddChild(LeftBar);
    LeftBar->MoveTo(-2, 0, 0);

    BottomBar = NewPart("Not enough memory to allocate bottom bar");
    result = BottomBar->Init(0, 0, width + 4, 2, nullptr);

    if (result != 0)
    {
        return result;
    }

    BottomBar->SetPaintRoutine(BottomBarPaint);
    AddChild(BottomBar);
    BottomBar->MoveTo(-2, height, 0);

    RightBar = NewPart("Not enough memory to allocate right bar");
    result = RightBar->Init(0, 0, 10, height + 2, nullptr);

    if (result != 0)
    {
        return result;
    }

    RightBar->SetPaintRoutine(RightBarPaint);
    AddChild(RightBar);
    RightBar->MoveTo(width, 0, 0);

    ResizeButton = NewPart("Not enough memory to allocate resize area");
    result = ResizeButton->Init(0, 0, 8, 8, nullptr);

    if (result != 0)
    {
        return result;
    }

    ResizeButton->SetBackground(0x29);
    ResizeButton->Draw();
    RightBar->AddChild(ResizeButton);
    ResizeButton->MoveTo(2, RightBar->Height() - ResizeButton->Height(), 0);
    ResizeButton->SetEventRoutine(HandleResizeButtonEvent);

    MoveTo(xPos, yPos + 0xd, 0);
    return 0;
}

auto MCGuiTitleWindow::Destroy() -> void
{
    DestroyPart(TitleBar);
    DestroyPart(LeftBar);
    DestroyPart(ResizeButton);
    DestroyPart(RightBar);
    DestroyPart(BottomBar);
    MCGuiObject::Destroy();
}

auto MCGuiTitleWindow::Draw() -> void
{
    VfxPaneWipe(DisplayPort->Frame(), BackColor());

    if (Dragging() == 0 || WinState == 2)
    {
        MCGuiObject::Draw();
        return;
    }

    // While dragged, only the frame is drawn (the right bar twice, as the original).
    TitleBar->Draw();
    LeftBar->Draw();
    RightBar->Draw();
    RightBar->Draw();
    BottomBar->Draw();

    if (ResizeButton != nullptr)
    {
        ResizeButton->Draw();
    }
}

auto MCGuiTitleWindow::Resize(int32_t newWidth, int32_t newHeight) -> void
{
    if (newWidth < 0 || newHeight < 0 || TitleBar->ResizeOK(newWidth) == 0)
    {
        return;
    }

    if (GridAligned != 0)
    {
        SnapToGrid(newWidth, newHeight);
    }

    MCGuiObject::Resize(newWidth, newHeight);
    TitleBar->Resize(newWidth + 0xc, TitleBar->Height());
    LeftBar->Resize(LeftBar->Width(), newHeight);
    RightBar->Resize(RightBar->Width(), newHeight);
    RightBar->MoveTo(newWidth, 0, 0);
    BottomBar->Resize(newWidth + 4, BottomBar->Height());
    BottomBar->MoveTo(-2, newHeight, 0);
    ResizeButton->MoveTo(2, RightBar->Height() - ResizeButton->Height(), 0);
}

auto MCGuiTitleWindow::SetTitle(char* newTitle) -> void
{
    if (TitleBar != nullptr)
    {
        TitleBar->SetTitle(newTitle);
    }
}

// aTitleBar

MCGuiTitleBar::MCGuiTitleBar()
{
}

auto MCGuiTitleBar::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    int32_t result = MCGuiObject::Init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    SetBackColor(8);
    Font = BlackFont;

    if (name == nullptr)
    {
        name = const_cast<char*>(" ");
    }

    // Unbounded, as the original.
    std::strcpy(Title, name);

    CloseButton = new MCGuiCloseButton;
    result = CloseButton->Init(0, 0, 4, 4, nullptr);

    if (result != 0)
    {
        return result;
    }

    CloseButton->SetUpPicture(0x12);
    CloseButton->SetDownPicture(0x13);
    AddChild(CloseButton);
    CloseButton->MoveTo(WinWidth - CloseButton->Width(), 0, 0);

    ZoomButton = new MCGuiButton;
    result = ZoomButton->Init(0, 0, 4, 4, nullptr);

    if (result != 0)
    {
        return result;
    }

    ZoomButton->SetUpPicture(0x14);
    ZoomButton->SetDownPicture(0x15);
    AddChild(ZoomButton);
    ZoomButton->MoveTo(0, 0, 0);

    ZoomOutButton = new MCGuiButton;
    result = ZoomOutButton->Init(0, 0, 4, 4, nullptr);

    if (result != 0)
    {
        return result;
    }

    ZoomOutButton->SetUpPicture(0x16);
    ZoomOutButton->SetDownPicture(0x17);
    AddChild(ZoomOutButton);
    ZoomOutButton->MoveTo(ZoomButton->X() + ZoomButton->Width(), 0, 0);
    ShowZoomButtons(0);

    SwoopyButton = new MCGuiTitleButton;
    result = SwoopyButton->Init(0, 0, 4, 4, nullptr);

    if (result != 0)
    {
        return result;
    }

    SwoopyButton->SetUpPicture(0x18);
    SwoopyButton->SetDownPicture(0x19);
    AddChild(SwoopyButton);
    SwoopyButton->MoveTo(WinWidth - CloseButton->Width() - SwoopyButton->Width(), 0, 0);
    SwoopyButton->SetEventRoutine(HandleSwoopyButtonEvent);
    ShowSwoopyButton(0);
    return 0;
}

auto MCGuiTitleBar::Destroy() -> void
{
    DestroyPart(CloseButton);
    DestroyPart(ZoomButton);
    DestroyPart(ZoomOutButton);
    DestroyPart(SwoopyButton);
    MCGuiObject::Destroy();
}

auto MCGuiTitleBar::SetZoomCallbacks() -> void
{
    if (Parent == nullptr)
    {
        return;
    }

    ZoomButton->Callback()->SetMessage(Parent, 0x1a);
    ZoomOutButton->Callback()->SetMessage(Parent, 0x1b);
}

auto MCGuiTitleBar::SetTitle(char* newTitle) -> void
{
    std::strcpy(Title, newTitle);
}

auto MCGuiTitleBar::HandleEvent(MCGuiEvent* event) -> void
{
    switch (event->Type)
    {
        case 1: // left button down: start dragging the window
        {
            MCGuiObject* window = Parent;
            LastX = event->X - window->X();
            LastY = event->Y - window->Y();

            if (window != nullptr && window->Parent != nullptr &&
                window->Parent->ForemostChild(window->Depth()) != window)
            {
                window->BringToFront(0);
                ARedrawScreen();
            }

            Application->Grab(this);
            Parent->StartDrag(Parent->X(), Parent->Y());
            Parent->Draw();
            return;
        }

        case 4: // left button up: drop the window
        {
            if (Application->GrabbedObject() != this)
            {
                return;
            }

            MCGuiObject* window = Parent;
            const tagRECT area = Application->ScrollRect;
            window->StopDrag();

            for (int16_t i = 9; i < window->NumberOfChildren(); i++)
            {
                window->Child(i)->ShowGuiWindow(1);
            }

            const int32_t mouseX = event->X;
            const int32_t mouseY = event->Y;
            window->MoveTo(mouseX - LastX, mouseY - LastY, 0);
            KeepTitleBarInside(this, area);
            Parent->Draw();
            Application->Release();
            MCGuiObject* under = ScreenWindow->FindObject(mouseX, mouseY);

            if (under != nullptr)
            {
                under->Enter();
            }

            return;
        }

        case 7: // mouse move: drag, unless over a depth-100 (modal) object
        {
            if (Application->GrabbedObject() != this)
            {
                return;
            }

            const int32_t mouseX = event->X;
            const int32_t mouseY = event->Y;
            const tagRECT area = Application->ScrollRect;
            MCGuiObject* under = ScreenWindow->FindObject(mouseX, mouseY);

            if (under == nullptr || under->Depth() != 100)
            {
                Parent->MoveTo(mouseX - LastX, mouseY - LastY, 0);
                KeepTitleBarInside(this, area);
            }

            return;
        }

        case 0xc:
        {
            Draw();
            return;
        }
    }
}

auto MCGuiTitleBar::Draw() -> void
{
    VfxPaneWipe(DisplayPort->Frame(), BackColor());
    MCGuiPort* barPort = DisplayPort;
    VfxLineDraw(barPort->Frame(), 0, 0, Width(), 0, 0xe);
    VfxLineDraw(barPort->Frame(), 0, 0, 0, 0xd, 0xe);

    if (Parent != nullptr)
    {
        barPort = DisplayPort;
        VfxLineDraw(barPort->Frame(), 1, Height() - 1, Parent->Width() + 2, Height() - 1, 3);
    }

    int32_t textX = 3;

    if (ZoomButton != nullptr && ZoomButton->IsShowing() != 0)
    {
        textX = ZoomButton->X() + ZoomButton->Width() + ZoomOutButton->Width() + 3;
    }

    Font->WriteString(barPort->Frame(), textX, 3, reinterpret_cast<uint8_t*>(Title), -1);
    MCGuiObject::Draw();
}

auto MCGuiTitleBar::ShowCloseButton(int show) -> void
{
    if (CloseButton == nullptr)
    {
        return;
    }

    CloseButton->ShowGuiWindow(show);

    if (show != 0)
    {
        CloseButton->Callback()->SetMessage(Parent, 0xd);
    }
}

auto MCGuiTitleBar::ShowZoomButton(int show) -> void
{
    if (ZoomButton != nullptr)
    {
        ZoomButton->ShowGuiWindow(show);
    }
}

auto MCGuiTitleBar::ShowZoomButtons(int show) -> void
{
    if (ZoomButton != nullptr)
    {
        ZoomButton->ShowGuiWindow(show);
    }

    if (ZoomOutButton != nullptr)
    {
        ZoomOutButton->ShowGuiWindow(show);
    }
}

auto MCGuiTitleBar::ShowSwoopyButton(int show) -> void
{
    if (SwoopyButton != nullptr)
    {
        SwoopyButton->ShowGuiWindow(show);
    }
}

auto MCGuiTitleBar::Resize(int32_t newWidth, int32_t newHeight) -> void
{
    MCGuiObject::Resize(newWidth, newHeight);
    const int32_t barWidth = WinWidth;
    CloseButton->MoveTo(barWidth - CloseButton->Width(), 0, 0);
    SwoopyButton->MoveTo(barWidth - CloseButton->Width() - SwoopyButton->Width(), 0, 0);
}

auto MCGuiTitleBar::ResizeOK(int32_t newWidth) -> int
{
    int32_t needed = 4;

    if (CloseButton != nullptr && CloseButton->IsShowing() != 0)
    {
        needed = CloseButton->Width() + 5;
    }

    if (ZoomButton != nullptr && ZoomButton->IsShowing() != 0)
    {
        needed += ZoomButton->Width() + 1;
    }

    if (ZoomOutButton != nullptr && ZoomOutButton->IsShowing() != 0)
    {
        needed += ZoomOutButton->Width() + 1;
    }

    return needed <= newWidth ? 1 : 0;
}

// aMenu

namespace
{
    /// <summary>The text of a separator item.</summary>
    constexpr const char* MenuSeparator = "::::";
    /// <summary>The length of an item's text slot.</summary>
    constexpr int32_t MenuItemLength = 0x28;
    /// <summary>The most items a menu holds.</summary>
    constexpr int32_t MaxMenuItems = 25;
}

MCGuiMenu::MCGuiMenu()
{
}

auto MCGuiMenu::ShowGuiWindow(int show) -> void
{
    ShowWindow = show;

    if (show != 0)
    {
        Shown = 1;
    }
}

auto MCGuiMenu::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    Font = nullptr;
    NumItems = 0;
    SelectedItem = -1;
    ItemText = nullptr;
    int32_t result = MCGuiObject::Init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    Font = WhiteFont;
    ItemText = std::make_unique<char[]>(1000);

    for (int32_t i = 0; i < MaxMenuItems; i++)
    {
        ItemCallbacks[i] = nullptr;
        ItemData[i] = -1;
        ItemLetters[i] = 0;
    }

    SetBackColor(0);
    MoveTo(xPos, yPos, 0);
    ItemHeight = Font->Height() + 8;
    HasLetters = 0;
    RightAligned = 0;
    return 0;
}

auto MCGuiMenu::Destroy() -> void
{
    ItemText.reset();

    for (int16_t i = 0; i < NumItems; i++)
    {
        if (ItemCallbacks[i] != nullptr)
        {
            delete ItemCallbacks[i];
            ItemCallbacks[i] = nullptr;
        }
    }

    NumItems = 0;
    SelectedItem = -1;
    MCGuiObject::Destroy();
}

auto MCGuiMenu::HandleEvent(MCGuiEvent* event) -> void
{
    if (event->Type == 4)
    {
        // A release runs the item under the cursor, then hides the menu wherever it happened.
        const int32_t mouseY = event->Y;
        SelectedItem = (mouseY - GlobalY()) / ItemHeight;

        if (PointInside(event->X, mouseY) != 0 && SelectedItem > -1 && SelectedItem < NumItems &&
            ItemCallbacks[SelectedItem] != nullptr)
        {
            ItemCallbacks[SelectedItem]->Execute();
        }

        ShowGuiWindow(0);
        Application->Release();
    }
    else if (event->Type == 7)
    {
        const int32_t item = (event->Y - GlobalY()) / ItemHeight;

        if (item != SelectedItem)
        {
            SelectedItem = item;
            Draw();
        }
    }

    MCGuiObject::HandleEvent(event);
}

auto MCGuiMenu::Draw() -> void
{
    const int32_t halfItem = ItemHeight / 2;
    int32_t itemY = 0;
    char* text = ItemText.get();
    VfxPaneWipe(DisplayPort->Frame(), BackgroundColor);
    MCGuiObject::Draw();

    if (Dragging() != 0)
    {
        return;
    }

    VfxLineDraw(DisplayPort->Frame(), 0, 0, Width() - 1, 0, 0xf);
    VfxLineDraw(DisplayPort->Frame(), Width() - 1, 0, Width() - 1, Height() - 1, 0xf);
    VfxLineDraw(DisplayPort->Frame(), 0, Height() - 1, Width() - 1, Height() - 1, 0xf);
    VfxLineDraw(DisplayPort->Frame(), 0, 0, 0, Height() - 1, 0xf);

    for (int16_t i = 0; i < NumItems; i++)
    {
        if (std::strcmp(text, MenuSeparator) == 0)
        {
            const int32_t lineY = halfItem + itemY;
            VfxLineDraw(DisplayPort->Frame(), 4, lineY, Width() - 8, lineY, 9);
        }
        else
        {
            if (i == SelectedItem)
            {
                FillBox(1, static_cast<int16_t>(itemY + 1), static_cast<int16_t>(Width() - 1),
                        static_cast<int16_t>(itemY + ItemHeight), 0xb);
            }

            int32_t textX = 2;

            if (RightAligned != 0)
            {
                textX = Width() + (-6 - Font->Width(reinterpret_cast<uint8_t*>(text)));
            }

            Font->WriteString(DisplayPort->Frame(), textX, itemY + 4, reinterpret_cast<uint8_t*>(text), -1);
            const char letter = ItemLetters[i];

            if (letter != 0)
            {
                const int32_t letterX = Width() + (-3 - Font->Width(static_cast<uint8_t>(letter)));
                Font->WriteChar(DisplayPort->Frame(), letterX, itemY + 4, letter);
            }
        }

        VfxLineDraw(DisplayPort->Frame(), 1, itemY, Width() - 2, itemY, 0xf);
        text += MenuItemLength;
        itemY += ItemHeight;
    }
}

auto MCGuiMenu::ResizeMenu() -> void
{
    uint8_t* text = reinterpret_cast<uint8_t*>(ItemText.get());
    const int32_t menuHeight = (Font->Height() + 8) * NumItems;
    int32_t menuWidth = 0;

    for (int16_t i = 0; i < NumItems; i++)
    {
        if (static_cast<double>(menuWidth) < static_cast<double>(Font->Width(text) + 4) * 1.25)
        {
            menuWidth = static_cast<int32_t>(static_cast<double>(Font->Width(text) + 4) * 1.25);
        }

        text += MenuItemLength;
    }

    if (HasLetters != 0)
    {
        menuWidth += 6 + Font->Width(reinterpret_cast<uint8_t*>(const_cast<char*>("W")));
    }

    if (Width() == menuWidth && Height() == menuHeight)
    {
        return;
    }

    Resize(menuWidth, menuHeight);
}

auto MCGuiMenu::AddItem(char* text) -> int32_t
{
    const int32_t index = NumItems;

    if (index > MaxMenuItems - 1)
    {
        return static_cast<int32_t>(0xeeee0001);
    }

    MCGuiCallback* callback = new MCGuiCallback;

    if (callback == nullptr)
    {
        return static_cast<int32_t>(0xeeee0002);
    }

    ItemCallbacks[index] = callback;
    ItemData[NumItems] = 0;

    if (std::strlen(text) > MenuItemLength - 1)
    {
        text[MenuItemLength - 1] = 0;
    }

    std::strcpy(ItemText.get() + NumItems * MenuItemLength, text);
    NumItems++;
    ResizeMenu();
    return NumItems - 1;
}

auto MCGuiMenu::RemoveItem(char* text) -> int
{
    int16_t i = 0;

    while (i < NumItems && MCPort::StrICmp(ItemText.get() + i * MenuItemLength, text) != 0)
    {
        i++;
    }

    return RemoveItem(i);
}

auto MCGuiMenu::RemoveItem(int16_t index) -> int
{
    if (index >= NumItems)
    {
        return 0;
    }

    if (ItemCallbacks[index] != nullptr)
    {
        delete ItemCallbacks[index];
    }

    int16_t i = index;

    while (i < NumItems - 1)
    {
        ItemCallbacks[i] = ItemCallbacks[i + 1];
        ItemData[i] = ItemData[i + 1];
        i++;
    }

    ItemCallbacks[i] = nullptr;
    ItemData[i] = 0;
    // The letters are not shifted with the items.
    char* slot = ItemText.get() + index * MenuItemLength;
    std::memmove(slot, slot + MenuItemLength,
                 static_cast<size_t>(static_cast<int16_t>((0x18 - index) * MenuItemLength)));
    NumItems--;
    ResizeMenu();
    return 1;
}

auto MCGuiMenu::AddSeparator() -> int32_t
{
    const int32_t index = NumItems;

    if (index > MaxMenuItems - 1)
    {
        return static_cast<int32_t>(0xeeee0001);
    }

    std::strcpy(ItemText.get() + index * MenuItemLength, MenuSeparator);
    NumItems = index + 1;
    return index + 1;
}

auto MCGuiMenu::SetCallback(int16_t index, void (*exec)()) -> void
{
    if (index < NumItems)
    {
        ItemCallbacks[index]->SetExec(exec);
    }
}

auto MCGuiMenu::SetMessage(int16_t index, MCGuiObject* target, int32_t message) -> void
{
    if (index < NumItems)
    {
        ItemCallbacks[index]->SetMessage(target, message);
    }
}

auto MCGuiMenu::ChangeItemString(int16_t index, char* text) -> int32_t
{
    if (index >= NumItems)
    {
        return static_cast<int32_t>(0xeeee0003);
    }

    if (std::strlen(text) > MenuItemLength - 1)
    {
        text[MenuItemLength - 1] = 0;
    }

    std::strcpy(ItemText.get() + index * MenuItemLength, text);
    ResizeMenu();
    return 0;
}

auto MCGuiMenu::KeepOnScreen() -> void
{
    const tagRECT area = Application->ScrollRect;

    if (X() < area.left)
    {
        MoveTo(area.left, Y(), 0);
    }

    if (Y() < area.top)
    {
        MoveTo(X(), area.top, 0);
    }

    if (area.right < X() + Width())
    {
        MoveTo(area.right - Width() - 10, Y(), 0);
    }

    if (area.bottom < Y() + Height())
    {
        MoveTo(X(), area.bottom - Height() - 10, 0);
    }
}

auto MCGuiMenu::SetItemData(int16_t index, int32_t data) -> void
{
    if (index < NumItems)
    {
        ItemData[index] = data;
    }
}

auto MCGuiMenu::GetItemData(int16_t index) -> int32_t
{
    if (index >= NumItems)
    {
        return static_cast<int32_t>(0xeeee0003);
    }

    return ItemData[index];
}

auto MCGuiMenu::SetItemLetter(int16_t index, char letter) -> void
{
    if (index < NumItems)
    {
        ItemLetters[index] = letter;
        HasLetters = 1;
        ResizeMenu();
    }
}

auto MCGuiMenu::GetItemLetter(int16_t index) -> char
{
    if (index >= NumItems)
    {
        return 3;
    }

    return ItemLetters[index];
}

// aToolBar

MCGuiToolBar::MCGuiToolBar()
{
}

auto MCGuiToolBar::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    const int32_t result = MCGuiTitleWindow::Init(xPos, yPos, width, height, name);
    SetBackColor(0);

    if (TitleBar != nullptr)
    {
        TitleBar->ShowCloseButton(0);
    }

    MaxLength = 8;
    return result;
}

auto MCGuiToolBar::Destroy() -> void
{
    for (int16_t i = 0; i < NumButtons; i++)
    {
        DestroyPart(Buttons[i]);
    }

    MCGuiTitleWindow::Destroy();
}

auto MCGuiToolBar::AddButton(MCGuiToolButton* button) -> int32_t
{
    if (NumButtons > MaxMenuItems - 1)
    {
        return static_cast<int32_t>(0xeeee0001);
    }

    Buttons[NumButtons] = button;
    NumButtons++;
    AddChild(button);
    PlaceButtons();
    return 0;
}

auto MCGuiToolBar::RemoveButton(int32_t index) -> int32_t
{
    if (index >= NumButtons)
    {
        return static_cast<int32_t>(0xeeee0003);
    }

    RemoveChild(GetButton(index));

    while (index < NumButtons - 1)
    {
        Buttons[index] = Buttons[index + 1];
        index++;
    }

    // Original bug (OB-064): clears the slot past the last button, not the last one; with a full bar that slot is
    // `horizontal` (+0x52c), which is zeroed.
    if (NumButtons < MaxMenuItems)
    {
        Buttons[NumButtons] = nullptr;
    }
    else
    {
        Horizontal = 0;
    }

    NumButtons--;
    PlaceButtons();
    return 0;
}

auto MCGuiToolBar::GetButton(int32_t index) -> MCGuiToolButton*
{
    if (index > MaxMenuItems - 1)
    {
        return nullptr;
    }

    return Buttons[index];
}

auto MCGuiToolBar::IsPushed(int32_t index) -> int
{
    if (GetButton(index) == nullptr)
    {
        return 0;
    }

    return GetButton(index)->Pushed;
}

auto MCGuiToolBar::SetHorizontal(int on) -> void
{
    Horizontal = on;
    PlaceButtons();
}

auto MCGuiToolBar::SetMaxLength(int16_t length) -> void
{
    MaxLength = length;
    PlaceButtons();
}

auto MCGuiToolBar::PlaceButtons() -> void
{
    PlaceBarButtons(this, Buttons, NumButtons, MaxLength, Horizontal, ButtonWidth, ButtonHeight);
}

auto MCGuiToolBar::SetButtonSize(int32_t width, int32_t height) -> void
{
    ButtonWidth = width;
    ButtonHeight = height;
}

// aWindowBar

MCGuiWindowBar::MCGuiWindowBar()
{
}

auto MCGuiWindowBar::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    const int32_t result = MCGuiObject::Init(xPos, yPos, width, height, name);
    SetBackColor(0);
    MaxLength = 8;
    return result;
}

auto MCGuiWindowBar::Destroy() -> void
{
    for (int16_t i = 0; i < NumButtons; i++)
    {
        DestroyPart(Buttons[i]);
    }

    MCGuiObject::Destroy();
}

auto MCGuiWindowBar::InsertButton(MCGuiButton* button, int32_t index) -> int32_t
{
    if (NumButtons >= MaxMenuItems || index > NumButtons)
    {
        return static_cast<int32_t>(0xeeee0001);
    }

    for (int32_t i = NumButtons; i > index; i--)
    {
        Buttons[i] = Buttons[i - 1];
    }

    Buttons[index] = button;
    NumButtons++;
    AddChild(button);
    PlaceButtons();
    return 0;
}

auto MCGuiWindowBar::AddButton(MCGuiButton* button) -> int32_t
{
    if (NumButtons > MaxMenuItems - 1)
    {
        return static_cast<int32_t>(0xeeee0001);
    }

    Buttons[NumButtons] = button;
    NumButtons++;
    AddChild(button);
    PlaceButtons();
    return 0;
}

auto MCGuiWindowBar::RemoveButton(int32_t index) -> int32_t
{
    if (index >= NumButtons)
    {
        return static_cast<int32_t>(0xeeee0003);
    }

    RemoveChild(GetButton(index));

    while (index < NumButtons - 1)
    {
        Buttons[index] = Buttons[index + 1];
        index++;
    }

    // Original bug (OB-064): as aToolBar::RemoveButton; past a full bar the slot is `horizontal` (+0x518).
    if (NumButtons < MaxMenuItems)
    {
        Buttons[NumButtons] = nullptr;
    }
    else
    {
        Horizontal = 0;
    }

    NumButtons--;
    PlaceButtons();
    return 0;
}

auto MCGuiWindowBar::GetButton(int32_t index) -> MCGuiButton*
{
    if (index > MaxMenuItems - 1)
    {
        return nullptr;
    }

    return Buttons[index];
}

auto MCGuiWindowBar::SetHorizontal(int on) -> void
{
    Horizontal = on;
    PlaceButtons();
}

auto MCGuiWindowBar::SetMaxLength(int16_t length) -> void
{
    MaxLength = length;
    PlaceButtons();
}

auto MCGuiWindowBar::PlaceButtons() -> void
{
    PlaceBarButtons(this, Buttons, NumButtons, MaxLength, Horizontal, ButtonWidth, ButtonHeight);
}

auto MCGuiWindowBar::SetButtonSize(int32_t width, int32_t height) -> void
{
    ButtonWidth = width;
    ButtonHeight = height;
}

// aSmackerWindow
//
// Port: a full-screen movie (fullScreen set while the game is full screen) switched the original's display to
// 16-bit and let Smacker draw to the DirectDraw surface in its own colours. The port's display stays 8-bit, so a
// full-screen movie puts up its own palette (checkSmackerPalette), as the original did in a windowed game; a windowed
// movie is remapped to the game palette. Either way the movie decodes into the window's own pane (the original
// decoded a full-screen movie straight onto the screen), and the window copies the pane to the screen in the frame
// pass: movie frames are the one picture that changes under the renderer every frame.

MCGuiSmackerWindow::MCGuiSmackerWindow()
{
}

auto MCGuiSmackerWindow::Init(tagRECT* area, tagPOINT* position) -> int32_t
{
    int32_t left = area->left;
    int32_t top = area->top;

    if (position != nullptr)
    {
        left += position->x;
        top += position->y;
    }

    // The rectangle's right and bottom are the width and height.
    return MCGuiObject::Init(left, top, area->right, area->bottom, nullptr);
}

auto MCGuiSmackerWindow::StartSmackerMovie(MCSmackTag* newMovie, int fullScreenPlay) -> int32_t
{
    FullScreen = fullScreenPlay;
    Movie = newMovie;
    MovieOver = 0;
    // (The original returned here for a full-screen movie: it had no pane.)
    MoviePane = new MCPane{*Frame()};
    MCWindow* movieWindow = new MCWindow{};
    MoviePane->Window = movieWindow;
    movieWindow->View = nullptr;
    movieWindow->Texture = nullptr;

    if (MoviePane->X1 < 0 || MoviePane->Y1 < 0)
    {
        movieWindow->Buffer = nullptr;
        return static_cast<int32_t>(0xd4d40000);
    }

    movieWindow->XMax = MoviePane->X1;
    movieWindow->YMax = MoviePane->Y1;
    movieWindow->Buffer = new uint8_t[static_cast<size_t>((movieWindow->YMax + 1) * (movieWindow->XMax + 1))]{};
    MCRenderer::CreateTexture(movieWindow, MCTextureUse::Stream);

    if (FullScreen == 0)
    {
        RemapToGamePalette(Movie);
    }

    return 0;
}

auto MCGuiSmackerWindow::Destroy() -> void
{
    SmackClose(Movie);
    Movie = nullptr;

    if (MoviePane != nullptr && MoviePane->Window != nullptr)
    {
        MCRenderer::DestroyTexture(MoviePane->Window);
        delete[] MoviePane->Window->Buffer;
        delete MoviePane->Window;
    }

    delete MoviePane;
    MoviePane = nullptr;
    MCGuiObject::Destroy();
    ScreenWindow->RemoveChild(this);
}

auto MCGuiSmackerWindow::EndSmackerMovie() -> void
{
    SmackClose(Movie);
    Movie = nullptr;
    MovieOver = 1;
    Destroy();
}

auto MCGuiSmackerWindow::CheckSmackerPalette() -> void
{
    // Port fix: the original reads the movie unguarded.
    if (Movie == nullptr || !Movie->Player->NewPalette())
    {
        return;
    }

    if (FullScreen == 0)
    {
        RemapToGamePalette(Movie);
        return;
    }

    Application->ActivateSmackerPalette(const_cast<uint8_t*>(Movie->Player->Palette().data()));
}

auto MCGuiSmackerWindow::Display() -> void
{
    if (ShowWindow == 0)
    {
        return;
    }

    if (IsHidden() != 0 && HideOffset == 0)
    {
        return;
    }

    if (Movie == nullptr)
    {
        if (FullScreen != 0)
        {
            ClearScreen();
        }

        if (MoviePane != nullptr)
        {
            VfxPaneWipe(MoviePane, 0);
        }

        // (The original left the screen as it was.) An escaped movie has destroyed the window already (endSmackerMovie)
        // while application->smackerWindow still names it until its owner clears it, so there is no pane to draw.
        if (FramePane != nullptr)
        {
            DrawInFramePass(DisplayPort);
        }

        return;
    }

    MCSmackerPlayer* player = Movie->Player.get();

    if (static_cast<uint32_t>(Width()) < static_cast<uint32_t>(player->Width()) ||
        static_cast<uint32_t>(Height()) < static_cast<uint32_t>(player->Height()))
    {
        Fatal(0, "Movie is too big for its window");
    }

    if (FirstFrame != 0)
    {
        VfxPaneWipe(MoviePane, 0);

        if (FullScreen != 0)
        {
            ClearScreen();
        }

        FirstFrame = 0;
    }

    // (The original set SmackToBuffer every display; the port locks the movie's rectangle of the texture only while
    // a frame is decoded into it, so a display with no new frame sends nothing up.)
    if (!player->Wait())
    {
        MCTexture* texture = MoviePane->Window->Texture;
        const MCRect rect{MoviePane->X0, MoviePane->Y0, MoviePane->X0 + player->Width() - 1,
                          MoviePane->Y0 + player->Height() - 1};
        player->ToBuffer(0, 0, texture->Width, player->Height(), MCRenderer::LockTexture(texture, rect));
        const int more = NextFrame();
        MCRenderer::UnlockTexture(texture);
        player->ToBuffer(0, 0, 0, 0, nullptr);

        if (more == 0)
        {
            MovieOver = 1;
        }
    }

    DrawInFramePass(DisplayPort);
}

auto MCGuiSmackerWindow::Draw() -> void
{
    // (The original drew the window's own picture, and only while the movie played.)
    if (MoviePane != nullptr && MoviePane->Window != nullptr && MoviePane->Window->Buffer != nullptr)
    {
        VfxPaneCopy(MoviePane, 0, 0, Port()->Frame(), 0, 0, -1);
    }
}

auto MCGuiSmackerWindow::NextFrame() -> int
{
    MCSmackerPlayer* player = Movie->Player.get();
    // Port: SmackToBufferRect (collecting the changed rectangle) has no use here; the whole frame is copied.
    (void)player->DoFrame();

    if (player->FrameNum() == player->Frames() - 1)
    {
        return 0;
    }

    player->NextFrame();
    return 1;
}

auto MCGuiSmackerWindow::FindObject(int32_t, int32_t) -> MCGuiObject*
{
    return nullptr;
}

// aStartupWindow

namespace
{
    /// <summary>The uplink end points the startup window's constructor stores in startupRects.</summary>
    constexpr int32_t StartupPoints[24] = {
        0x12f, 0x196, 0x14e, 0x17f, 0x17e, 0x150, 0x1eb, 0x71, 0x14e, 0x17f, 0x17e, 0x150,
        0x193, 0x138, 0x1a3, 0x125, 0x1ec, 0x8a,  0x1e6, 0xa9, 0x1ec, 0x8a,  0x1eb, 0x71,
    };
}

MCGuiStartupWindow::MCGuiStartupWindow()
{
    std::memcpy(StartupRects, StartupPoints, sizeof(StartupRects));
}

auto MCGuiStartupWindow::Destroy() -> void
{
    for (uint8_t*& image : StaticImages)
    {
        MCRenderer::UnregisterData(image);
        std::free(image);
        image = nullptr;
    }

    if (StaticPort != nullptr)
    {
        StaticPort->Destroy();
        delete StaticPort;
        StaticPort = nullptr;
    }

    MCGuiObject::Destroy();
    ScreenWindow->RemoveChild(this);
}

auto MCGuiStartupWindow::DoStatic() -> void
{
    for (int32_t row = 0; row < Height(); row++)
    {
        if (!RollDice(0x1e))
        {
            // Now and then a whole row is copied from a random one.
            if (RollDice(0x32) != 0)
            {
                uint8_t* buffer = StaticPane()->Window->Buffer;
                uint8_t* dest = buffer + Width() * row;
                const int32_t sourceRow = RandomNumber(Height());
                uint8_t* source = StaticPane()->Window->Buffer + sourceRow * Width();
                std::memmove(dest, source, static_cast<size_t>(Width()));
            }
        }
        else
        {
            for (int32_t column = 0; column < Width(); column++)
            {
                AGPixelWrite(StaticPane(), column, row, static_cast<uint32_t>(MCPort::Rand()) & 0x1f);
            }
        }
    }
}

auto MCGuiStartupWindow::EndStatic() -> void
{
    VfxPaneWipe(StaticPane(), 0);
}

auto MCGuiStartupWindow::Display() -> void
{
    if (ShowWindow == 0)
    {
        return;
    }

    if (IsHidden() != 0 && HideOffset == 0)
    {
        return;
    }

    // (The original drew each step straight onto the screen.)
    if (StaticPort != nullptr)
    {
        Step();
        DrawInFramePass(DisplayPort);
    }
}

auto MCGuiStartupWindow::Step() -> void
{
    MCSoundSystem* sounds = SoundSystem;
    const int32_t step = StartupState;
    StartupState = step + 1;
    MCFont* font = LineFont.get();

    if (sounds == nullptr)
    {
        return;
    }

    // Once the sequence is over, the screen shows noise.
    if (StartupState > 0xaa)
    {
        if (RollDice(10) != 0)
        {
            DoStatic();
            AGShapeDraw(StaticPane(), StaticImages[2], 0, 0x140, 0xf0);
            return;
        }

        if (!RollDice(0x1e))
        {
            return;
        }

        SoundSystem->Update();
        DoStatic();
        AGShapeDraw(StaticPane(), StaticImages[2], 1, 0x140, 0xf0);
        return;
    }

    const int32_t pointX = StartupRects[RandomStart * 2];
    const int32_t pointY = StartupRects[RandomStart * 2 + 1];
    auto setLarge = [font](int large)
    {
        font->Scale = large != 0 ? 1.6f : 1.0f;
        font->Scaled = large;
    };

    // The text is typed two letters a frame: the first piece restarts the line, the next ones follow the width
    // typed so far (measured by `measured`, which is the piece itself except once), the last one isn't measured.
    auto typeFirst = [&](int32_t lineX, int32_t lineY, const char* text)
    {
        setLarge(0);
        font->Print(lineX, lineY, const_cast<char*>(text), 0xfd, StaticPane());
        TextX = font->PrintWidth(const_cast<char*>(text), 0);
    };

    auto typeNext = [&](int32_t lineX, int32_t lineY, const char* text, const char* measured)
    {
        const int32_t typed = TextX;
        setLarge(0);
        font->Print(typed + lineX, lineY, const_cast<char*>(text), 0xfd, StaticPane());
        TextX = font->PrintWidth(const_cast<char*>(measured), 0) + typed;
    };

    auto typeLast = [&](int32_t lineX, int32_t lineY, const char* text)
    {
        setLarge(0);
        font->Print(TextX + lineX, lineY, const_cast<char*>(text), 0xfd, StaticPane());
    };

    // The finished picture: the map, the bunker and its uplink to the chosen point.
    auto drawUplink = [&]()
    {
        AGShapeDraw(StaticPane(), StaticImages[0], 0, 0x140, 0xf0);
        AGEllipseFill(StaticPane(), 0x10a, 0xe5, 3, 3, 0xfd);
        VfxLineDraw(StaticPane(), 0x10a, 0xe5, 0x1c1, 400, 0xfd);
        setLarge(0);
        font->Print(0x1c2, 0x18b, const_cast<char*>("Forward Command Bunker"), 0xfd, StaticPane());
        setLarge(1);
        font->Print(0x1c2, 0x19f, const_cast<char*>("Uplinking..."), 0xfc, StaticPane());
        VfxLineDraw(StaticPane(), 0x10a, 0xe5, pointX, pointY, 0xfe);
        VfxLineDraw(StaticPane(), pointX, pointY, 10, pointY, 0xfd);
    };

    uint32_t sample = 0x10;

    switch (step)
    {
        case 0:
        {
            AGShapeDraw(StaticPane(), StaticImages[0], 0, 0x140, 0xf0);
            sample = 0x11;
            break;
        }
        case 9:
        {
            AGEllipseFill(StaticPane(), 0x10a, 0xe5, 3, 3, 0xfd);
            sample = 0xf;
            break;
        }
        case 0x13:
            VfxLineDraw(StaticPane(), 0x10a, 0xe5, 0x1c1, 400, 0xfd);
            break;
        // "Forward Command Bunker"
        case 0x1d:
        {
            TextX = 0;
            typeFirst(0x1c7, 0x18b, "Fo");
            break;
        }
        case 0x1e:
            typeNext(0x1c7, 0x18b, "rw", "rw");
            break;
        case 0x1f:
            typeNext(0x1c7, 0x18b, "ar", "ar");
            break;
        case 0x20:
        case 0x24:
            typeNext(0x1c7, 0x18b, "d ", "d ");
            break;
        case 0x21:
            typeNext(0x1c7, 0x18b, "Co", "Co");
            break;
        case 0x22:
            typeNext(0x1c7, 0x18b, "mm", "mm");
            break;
        case 0x23:
            typeNext(0x1c7, 0x18b, "an", "an");
            break;
        case 0x25:
            typeNext(0x1c7, 0x18b, "Bu", "Bu");
            break;
        case 0x26:
            typeLast(0x1c7, 0x18b, "nker");
            break;
        case 0x27:
        {
            setLarge(1);
            font->Print(0x1c2, 0x19f, const_cast<char*>("Uplinking..."), 0xfc, StaticPane());
            break;
        }
        case 0x31:
            VfxLineDraw(StaticPane(), 0x10a, 0xe5, pointX, pointY, 0xfe);
            break;
        case 0x3b:
        {
            sounds->PlayDigitalSample(0x20, 1, nullptr, 0, 0);
            SoundSystem->Update();
            DoStatic();
            return;
        }
        case 0x3c:
        case 0x3d:
        case 0x3e:
        case 0x3f:
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        {
            VfxPaneWipe(StaticPane(), 0);
            drawUplink();
            SoundSystem->Update();
            DoStatic();
            AGShapeDraw(StaticPane(), StaticImages[1], 0, 0x140, 0xf0);
            return;
        }
        case 0x45:
        {
            EndStatic();
            drawUplink();
            SoundSystem->PlayDigitalSample(0x10, 1, nullptr, 0, 0);
            SoundSystem->Update();
            AGEllipseFill(StaticPane(), pointX, pointY, 2, 2, 0xfc);
            return;
        }

        // "ComSat CSM-43a"
        case 0x4f:
            typeFirst(10, pointY + 10, "Co");
            break;
        case 0x50:
            // Original bug (OB-065): "mS" is typed but "Ms" measured.
            typeNext(10, pointY + 10, "mS", "Ms");
            break;
        case 0x51:
            typeNext(10, pointY + 10, "at", "at");
            break;
        case 0x52:
            typeNext(10, pointY + 10, " C", " C");
            break;
        case 0x53:
            typeNext(10, pointY + 10, "SM", "SM");
            break;
        case 0x54:
            typeNext(10, pointY + 10, "-4", "-4");
            break;
        case 0x55:
            typeNext(10, pointY + 10, "3a", "3a");
            break;
        // "Establishing Protocols"
        case 0x59:
            typeFirst(10, pointY + 0x19, "Es");
            break;
        case 0x5a:
            typeNext(10, pointY + 0x19, "ta", "ta");
            break;
        case 0x5b:
            typeNext(10, pointY + 0x19, "bl", "bl");
            break;
        case 0x5c:
            typeNext(10, pointY + 0x19, "is", "is");
            break;
        case 0x5d:
            typeNext(10, pointY + 0x19, "hi", "hi");
            break;
        case 0x5e:
            typeNext(10, pointY + 0x19, "ng", "ng");
            break;
        case 0x5f:
            typeNext(10, pointY + 0x19, " P", " P");
            break;
        case 0x60:
            typeNext(10, pointY + 0x19, "ro", "ro");
            break;
        case 0x61:
            typeNext(10, pointY + 0x19, "to", "to");
            break;
        case 0x62:
            typeLast(10, pointY + 0x19, "cols");
            break;
        case 0x63:
        {
            sounds->PlayDigitalSample(0x10, 1, nullptr, 0, 0);
            SoundSystem->Update();
            return;
        }

        // "Establishing Downlink"
        case 0x6d:
            typeFirst(10, pointY + 0x2d, "Es");
            break;
        case 0x6e:
            typeNext(10, pointY + 0x2d, "ta", "ta");
            break;
        case 0x6f:
            typeNext(10, pointY + 0x2d, "bl", "bl");
            break;
        case 0x70:
            typeNext(10, pointY + 0x2d, "is", "is");
            break;
        case 0x71:
            typeNext(10, pointY + 0x2d, "hi", "hi");
            break;
        case 0x72:
            typeNext(10, pointY + 0x2d, "ng", "ng");
            break;
        case 0x73:
            typeNext(10, pointY + 0x2d, " D", " D");
            break;
        case 0x74:
            typeNext(10, pointY + 0x2d, "ow", "ow");
            break;
        case 0x75:
            typeNext(10, pointY + 0x2d, "nl", "nl");
            break;
        case 0x76:
            typeLast(10, pointY + 0x2d, "ink");
            break;
        // The field site
        case 0x77:
            VfxLineDraw(StaticPane(), pointX, pointY, 0xf3, 0x101, 0xfd);
            break;
        case 0x81:
        {
            AGEllipseFill(StaticPane(), 0xf3, 0x101, 5, 5, 0xfb);
            sample = 0xf;
            break;
        }
        case 0x8b:
            VfxLineDraw(StaticPane(), 0xf3, 0x101, 0x1b, 0x101, 0xfd);
            break;
        // "Field Site Linking..."
        case 0x95:
            typeFirst(0x1b, 0x104, "Fi");
            break;
        case 0x96:
            typeNext(0x1b, 0x104, "el", "el");
            break;
        case 0x97:
            typeNext(0x1b, 0x104, "d ", "d ");
            break;
        case 0x98:
            typeNext(0x1b, 0x104, "Si", "Si");
            break;
        case 0x99:
            typeNext(0x1b, 0x104, "te", "te");
            break;
        case 0x9a:
            typeNext(0x1b, 0x104, " L", " L");
            break;
        case 0x9b:
            typeNext(0x1b, 0x104, "in", "in");
            break;
        case 0x9c:
            typeNext(0x1b, 0x104, "ki", "ki");
            break;
        case 0x9d:
            typeNext(0x1b, 0x104, "ng", "ng");
            break;
        case 0x9e:
            typeLast(0x1b, 0x104, "...");
            break;
        case 0x9f:
        {
            setLarge(1);
            font->Print(0x1b, 0x113, const_cast<char*>("GO"), 0xfd, StaticPane());
            sample = 0x11;
            break;
        }
        case 0xa9:
        {
            NoiseSample = sounds->PlayDigitalSample(0x21, 0, nullptr, 0, 0);
            SoundSystem->Update();
            DoStatic();
            AGShapeDraw(StaticPane(), StaticImages[2], 0, 0x140, 0xf0);
            return;
        }
        default:
            return;
    }

    SoundSystem->PlayDigitalSample(sample, 1, nullptr, 0, 0);
    SoundSystem->Update();
}

auto MCGuiStartupWindow::Draw() -> void
{
    MCGuiObject::Draw();

    if (StaticPort != nullptr)
    {
        StaticPort->CopyTo(Port()->Frame(), 0, 0, 0);
    }
}

auto MCGuiStartupWindow::StaticPane() -> MCPane*
{
    return StaticPort->Frame();
}

auto MCGuiStartupWindow::Setup() -> int32_t
{
    MCFile file;
    // Reads art packet `packet` whole into a CRT block.
    auto loadPacket = [&file](int32_t packet, uint8_t*& image) -> int32_t
    {
        int32_t result = ArtFile->SeekPacket(packet);

        if (result != 0)
        {
            return result;
        }

        result = file.Open(ArtFile, static_cast<uint32_t>(ArtFile->GetPacketSize()));

        if (result != 0)
        {
            return result;
        }

        image = static_cast<uint8_t*>(std::malloc(file.FileSize()));

        if (image == nullptr)
        {
            return -1;
        }

        file.Read(image, static_cast<int32_t>(file.FileSize()));
        MCRenderer::RegisterData(image, file.FileSize(), MCDataKind::Shapes);
        file.Close();
        return 0;
    };

    for (int32_t i = 0; i < 3; i++)
    {
        const int32_t result = loadPacket(0x2d + i, StaticImages[i]);

        if (result != 0)
        {
            return result;
        }
    }

    StaticPort = new MCGuiPort;

    if (StaticPort->Init(Width(), Height()) != 0)
    {
        return -1;
    }

    FrameCount = 0;
    RandomStart = RandomNumber(0xc);
    return 0;
}

// aEmptyTitleWindow

MCGuiEmptyTitleWindow::MCGuiEmptyTitleWindow()
{
}

MCGuiEmptyTitleWindow::~MCGuiEmptyTitleWindow()
{
    MCGuiEmptyTitleWindow::Destroy();
}

auto MCGuiEmptyTitleWindow::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    int32_t result = MCGuiHolderObject::Init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    TitleBar = new MCGuiTitleBar;

    if (TitleBar == nullptr)
    {
        Fatal(0, "Not enough memory to allocate titlebar");
    }

    result = TitleBar->Init(0, 0, width + 4, 8, name);

    if (result != 0)
    {
        return result;
    }

    AddChild(TitleBar);
    TitleBar->SetDepth(1);
    TitleBar->ShowCloseButton(0);
    TitleBar->MoveTo(-2, -8, 0);
    TitleBar->SetZoomCallbacks();

    LeftBar = NewPart("Not enough memory to allocate left bar");
    // The left bar's init result is not checked.
    LeftBar->Init(0, 0, 2, height, nullptr);
    LeftBar->SetPaintRoutine(CameraLeftBarPaint);
    AddChild(LeftBar);
    LeftBar->MoveTo(-2, 0, 0);

    BottomBar = NewPart("Not enough memory to allocate bottom bar");
    result = BottomBar->Init(0, 0, width + 4, 2, nullptr);

    if (result != 0)
    {
        return result;
    }

    BottomBar->SetPaintRoutine(CameraBottomBarPaint);
    AddChild(BottomBar);
    BottomBar->MoveTo(-2, height, 0);

    RightBar = NewPart("Not enough memory to allocate right bar");
    result = RightBar->Init(0, 0, 2, height, nullptr);

    if (result != 0)
    {
        return result;
    }

    RightBar->SetPaintRoutine(CameraRightBarPaint);
    AddChild(RightBar);
    RightBar->MoveTo(width, 0, 0);

    ResizeButton = NewPart("Not enough memory to allocate resize area");
    result = ResizeButton->Init(0, 0, 8, 8, nullptr);

    if (result != 0)
    {
        return result;
    }

    ResizeButton->SetBackground(0x29);
    ResizeButton->Draw();
    // Original bug (OB-066): the handle is the window's own child here, so handleResizeButtonEvent resizes the
    // window's parent (the screen window), not the window.
    AddChild(ResizeButton);
    ResizeButton->MoveTo(2 - ResizeButton->Width() + this->Width(), 2 - ResizeButton->Height() + this->Height(), 0);
    ResizeButton->SetEventRoutine(HandleResizeButtonEvent);
    ResizeButton->SetDepth(1);

    MoveTo(xPos, yPos + 8, 0);
    return 0;
}

auto MCGuiEmptyTitleWindow::Destroy() -> void
{
    DestroyPart(TitleBar);
    DestroyPart(LeftBar);
    DestroyPart(RightBar);
    DestroyPart(BottomBar);
    DestroyPart(ResizeButton);
    MCGuiHolderObject::Destroy();
}

auto MCGuiEmptyTitleWindow::Resize(int32_t newWidth, int32_t newHeight) -> void
{
    if (newWidth < 0 || newHeight < 0 || TitleBar->ResizeOK(newWidth) == 0)
    {
        return;
    }

    if (GridAligned != 0)
    {
        SnapToGrid(newWidth, newHeight);
    }

    MCGuiHolderObject::Resize(newWidth, newHeight);
    TitleBar->Resize(newWidth + 4, TitleBar->Height());
    LeftBar->Resize(LeftBar->Width(), newHeight);
    RightBar->Resize(RightBar->Width(), newHeight);
    RightBar->MoveTo(newWidth, 0, 0);
    BottomBar->Resize(newWidth + 4, BottomBar->Height());
    BottomBar->MoveTo(-2, newHeight, 0);
    ResizeButton->MoveTo(2 - ResizeButton->Width() + Width(), 2 - ResizeButton->Height() + Height(), 0);
}

auto MCGuiEmptyTitleWindow::HandleEvent(MCGuiEvent* event) -> void
{
    MCGuiObject* pane = Panes[0];

    if (pane != nullptr)
    {
        if (event->Type == 0xd)
        {
            // Close: the camera goes off and the window leaves the screen.
            if (pane->GetCamera() != nullptr)
            {
                pane->GetCamera()->Deactivate();
            }

            ScreenWindow->RemoveChild(this);
            return;
        }

        if (event->Type == 0x1a)
        {
            // Zoom. Port: the view goes between the closest and the furthest zoom, not while paused or asked; the camera
            // stays at full scale (the original flipped it between 100 and 1, only 1 while paused or asked, or when
            // only the 45-pixel art is loaded, and locked multiplayer to 1).
            MCCamera* camera = pane->GetCamera();

            if (camera != nullptr)
            {
                if (GamePaused == 0 && GameAsked == 0 && camera->View() != nullptr)
                {
                    camera->View()->ToggleZoom();
                }

                camera->ForceUpdate = 1;
                MCTerrain::ForceRedraw = 1;
            }
        }
        else if (event->Type == 0x1c)
        {
            if (pane->GetCamera() != nullptr)
            {
                pane->GetCamera()->ChangeTarget(nullptr, 0);
            }
        }
    }

    MCGuiObject::HandleEvent(event);
}

auto MCGuiEmptyTitleWindow::SetTitle(char* newTitle) -> void
{
    if (TitleBar != nullptr)
    {
        TitleBar->SetTitle(newTitle);
    }
}

auto MCGuiEmptyTitleWindow::SetBackColor(int32_t color) -> void
{
    if (TitleBar != nullptr)
    {
        TitleBar->SetBackColor(color);
    }
}
