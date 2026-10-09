#include "stdafx.h"
#include "gui/ascroll.h"
#include "gui/MCGuiPort.h"
#include "platform/MCInput.h"
#include "vfx/MCVfxFunctions.h"

// Event types the bar's children post to it (the arrow's or area's id) and the thumb sends it directly.
namespace
{
    /// <summary>The event type a timer fires (<c>aEvent::data</c> holds the timer id).</summary>
    constexpr int32_t TimerEvent = 0x13;
    /// <summary>The first repeat timer (1 s), replaced by the fast one (200 ms) once it fires.</summary>
    constexpr int16_t RepeatDelayTimer = 4;
    constexpr int16_t RepeatTimer = 5;
    /// <summary>The thumb was dragged: <c>lParam</c> holds the new position.</summary>
    constexpr int32_t ThumbDragged = 0x6a;
    /// <summary>Sets the position (<c>lParam</c>) without telling the parent.</summary>
    constexpr int32_t SetPositionQuietly = 0x6b;
    /// <summary>What the bar posts its parent when the position changed.</summary>
    constexpr int32_t ScrollChanged = 0x6c;
}

auto ScrollEventHandler(MCGuiObject* obj, MCGuiEvent* event) -> void
{
    // aScrollButton and aScrollArea both keep their id byte at +0x4ac.
    const uint8_t id = static_cast<MCGuiScrollButton*>(obj)->ButtonId;

    switch (event->Type)
    {
        case 1:
        {
            GuiSystem()->Grab(obj);
            GuiSystem()->AddTimer(obj, RepeatDelayTimer, 1000, 0, 0, 0);
            APostMessage(obj->Parent, id);
            break;
        }
        case 4:
        {
            GuiSystem()->Release();
            GuiSystem()->RemoveTimer(obj, RepeatDelayTimer);
            GuiSystem()->RemoveTimer(obj, RepeatTimer);
            break;
        }
        case TimerEvent:
        {
            if (event->Data == RepeatDelayTimer)
            {
                GuiSystem()->RemoveTimer(obj, RepeatDelayTimer);
                GuiSystem()->AddTimer(obj, RepeatTimer, 200, 0, 0, 0);
            }

            APostMessage(obj->Parent, id);
            break;
        }
        default:
            break;
    }
}

auto ScrollTabHandler(MCGuiObject* obj, MCGuiEvent* event) -> void
{
    auto* bar = static_cast<MCGuiScrollBar*>(obj->Parent);

    switch (event->Type)
    {
        case 1:
        {
            // Centre the cursor on the thumb, then drag it.
            const int32_t cursorX = obj->Width() / 2 + obj->GlobalX();
            const int32_t cursorY = obj->Height() / 2 + obj->GlobalY();
            MCInput::SetCursorPos(cursorX, cursorY);
            GuiSystem()->Grab(obj);
            break;
        }

        case 4:
            GuiSystem()->Release();
            break;
        case 7:
        {
            if (GuiSystem()->GrabbedObject() == obj)
            {
                const int32_t offset = event->Y - bar->GlobalY();
                const int32_t steps = (bar->ScrollMax * offset) / (bar->Height() - 0x12);
                const float newPos = static_cast<float>(static_cast<double>(steps) + 0.5);

                if (static_cast<int16_t>(static_cast<int32_t>(newPos)) != bar->ScrollPos)
                {
                    MCGuiEvent dragEvent;
                    dragEvent.Clear();
                    dragEvent.Type = ThumbDragged;
                    dragEvent.LParam = static_cast<int32_t>(newPos);
                    bar->HandleEvent(&dragEvent);
                }
            }
            break;
        }
        default:
            break;
    }
}

auto ScrollTabPaint(MCGuiObject* obj) -> void
{
    const int32_t width = obj->Width();
    const int32_t height = obj->Height();
    MCPane* pane = obj->Port()->Frame();
    VfxPaneWipe(pane, 0x39);

    // A black outline, a light bevel top and left, a dark one bottom and right.
    VfxLineDraw(pane, 0, height - 1, width, height - 1, 0);
    VfxLineDraw(pane, 0, 0, width, 0, 0);
    VfxLineDraw(pane, 0, 0, 0, height - 1, 0);
    VfxLineDraw(pane, width - 1, 0, width - 1, height - 1, 0);
    VfxLineDraw(pane, 1, 1, width - 2, 1, 0x3d);
    VfxLineDraw(pane, 2, 2, width - 3, 2, 0x3b);
    VfxLineDraw(pane, 1, 1, 1, height - 2, 0x3d);
    VfxLineDraw(pane, 2, 2, 2, height - 3, 0x3b);
    VfxLineDraw(pane, width - 2, 1, width - 2, height - 2, 0x35);
    VfxLineDraw(pane, width - 3, 2, width - 3, height - 3, 0x37);
    VfxLineDraw(pane, 1, height - 2, width - 2, height - 2, 0x35);
    VfxLineDraw(pane, 2, height - 3, width - 3, height - 3, 0x37);
}

MCGuiScrollBar::MCGuiScrollBar() = default;

auto MCGuiScrollBar::Init(int32_t xPos, int32_t yPos, int32_t, int32_t height, const char* name) -> int32_t
{
    constexpr int32_t outOfMemory = -0x1111fffe;

    int32_t result = MCGuiObject::Init(xPos, yPos, 0x12, height, name);

    if (result != 0)
    {
        return result;
    }

    UpButton = new MCGuiScrollButton;

    if (UpButton == nullptr)
    {
        return outOfMemory;
    }

    result = UpButton->Init(0, 0, 0x10, 8, nullptr);

    if (result != 0)
    {
        return result;
    }

    result = UpButton->SetBackground(0x27);

    if (result != 0)
    {
        return result;
    }

    AddChild(UpButton);
    UpButton->MoveTo(1, 1, 0);
    UpButton->ButtonId = 0x65;
    UpButton->SetEventRoutine(ScrollEventHandler);

    DownButton = new MCGuiScrollButton;

    if (DownButton == nullptr)
    {
        return outOfMemory;
    }

    result = DownButton->Init(0, 0, 0x10, 8, nullptr);

    if (result != 0)
    {
        return result;
    }

    result = DownButton->SetBackground(0x28);

    if (result != 0)
    {
        return result;
    }

    AddChild(DownButton);
    DownButton->MoveTo(1, height - 9, 0);
    DownButton->ButtonId = 0x66;
    DownButton->SetEventRoutine(ScrollEventHandler);

    UpArea = new MCGuiScrollArea;

    if (UpArea == nullptr)
    {
        return outOfMemory;
    }

    result = UpArea->Init(0, 0, 0x10, 1, nullptr);

    if (result != 0)
    {
        return result;
    }

    AddChild(UpArea);
    UpArea->MoveTo(1, 9, 0);
    UpArea->AreaId = 0x67;
    UpArea->SetEventRoutine(ScrollEventHandler);

    DownArea = new MCGuiScrollArea;

    if (DownArea == nullptr)
    {
        return outOfMemory;
    }

    result = DownArea->Init(0, 0, 0x10, 1, nullptr);

    if (result != 0)
    {
        return result;
    }

    DownArea->MoveTo(1, this->Height() - 1, 0);
    AddChild(DownArea);
    DownArea->AreaId = 0x68;
    DownArea->SetEventRoutine(ScrollEventHandler);

    ScrollTab = new MCGuiObject;

    if (ScrollTab == nullptr)
    {
        return outOfMemory;
    }

    ScrollTab->SetDrawsLive();
    result = ScrollTab->Init(0, 0, 0x10, 0x10, nullptr);

    if (result != 0)
    {
        return result;
    }

    AddChild(ScrollTab);
    ScrollTab->MoveTo(1, 8, 0);
    ScrollTab->SetBackColor(6);
    ScrollTab->SetEventRoutine(ScrollTabHandler);
    ScrollTab->SetPaintRoutine(ScrollTabPaint);

    SetBackColor(0xb);
    MoveTo(xPos, yPos, 0);
    SetDepth(10);
    UpArea->SetDepth(0xb);
    DownArea->SetDepth(0xb);
    UpButton->SetDepth(0xb);
    DownButton->SetDepth(0xb);
    ScrollTab->SetDepth(-0xc);
    SetScrollMax(0);
    SetScrollPos(0);
    return 0;
}

auto MCGuiScrollBar::Destroy() -> void
{
    auto release = [](auto*& child)
    {
        if (child != nullptr)
        {
            child->Destroy();
            delete child;
            child = nullptr;
        }
    };

    release(UpArea);
    release(DownArea);
    release(UpButton);
    release(DownButton);
    release(ScrollTab);
    MCGuiObject::Destroy();
}

auto MCGuiScrollBar::HandleEvent(MCGuiEvent* event) -> void
{
    int16_t newPos;

    switch (event->Type)
    {
        case 0x65:
            newPos = static_cast<int16_t>(ScrollPos - 1);
            break;
        case 0x66:
            newPos = static_cast<int16_t>(ScrollPos + 1);
            break;
        case 0x67:
            newPos = static_cast<int16_t>(ScrollPos - 10);
            break;
        case 0x68:
            newPos = static_cast<int16_t>(ScrollPos + 10);
            break;
        case ThumbDragged:
            newPos = static_cast<int16_t>(event->LParam);
            break;
        case SetPositionQuietly:
        {
            SetScrollPos(static_cast<int16_t>(event->LParam));
            return;
        }
        default:
            return;
    }

    SetScrollPos(newPos);

    if (Parent != nullptr)
    {
        APostMessage(Parent, ScrollChanged);
    }
}

auto MCGuiScrollBar::Draw() -> void
{
    VfxPaneWipe(DisplayPort->Frame(), BackgroundColor);
    VfxLineDraw(DisplayPort->Frame(), 0, 0, Width() - 1, 0, 0xf);
    VfxLineDraw(DisplayPort->Frame(), Width() - 1, 0, Width() - 1, Height() - 1, 0xf);
    VfxLineDraw(DisplayPort->Frame(), 0, Height() - 1, Width() - 1, Height() - 1, 0xf);
    VfxLineDraw(DisplayPort->Frame(), 0, 0, 0, Height() - 1, 0xf);
    MCGuiObject::Draw();
}

auto MCGuiScrollBar::SetScrollMax(int16_t newMax) -> void
{
    ScrollMax = newMax;
    ScrollTab->ShowGuiWindow(newMax != 0);
    ResizeAreas();
}

auto MCGuiScrollBar::SetScrollPos(int16_t newPos) -> void
{
    if (newPos < 0)
    {
        newPos = 0;
    }

    if (newPos > ScrollMax)
    {
        newPos = ScrollMax;
    }

    ScrollPos = newPos;
    ResizeAreas();
}

auto MCGuiScrollBar::ResizeAreas() -> void
{
    if (ScrollMax == 0)
    {
        return;
    }

    // The thumb (16 high) travels between the arrows (9 each) and a 1-pixel border.
    const int32_t tabTop = (ScrollPos * (Height() - 0x22)) / ScrollMax;
    ScrollTab->MoveTo(1, tabTop + 9, 0);
    UpArea->Resize(0x10, tabTop);
    DownArea->Resize(0x10, Height() + (-0x11 - (tabTop + 0x10)));
    DownArea->MoveTo(1, tabTop + 0x19, 0);
}

auto MCGuiScrollArea::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char*) -> int32_t
{
    WinWidth = width;
    WinY = yPos;
    WinHeight = height;
    WinX = xPos;
    WinState = MCGuiWindowState::Normal;
    ShowWindow = true;
    DragOn = false;
    DisplayPort.reset();
    FramePane = std::make_unique<MCPane>();
    FramePane->Window = ScreenPort()->Bitmap();
    FramePane->X0 = xPos;
    FramePane->Y0 = yPos;
    FramePane->X1 = xPos + width;
    FramePane->Y1 = yPos + height;
    PaintRoutine = nullptr;
    EventRoutine = nullptr;
    ChildList.clear();
    Parent = nullptr;
    WinDepth = 0;
    WindowAnimation.reset();
    Animating = false;
    IconAnimation.reset();
    return 0;
}

auto MCGuiScrollArea::Destroy() -> void
{
    FramePane.reset();
    ChildList.clear();

    if (Parent != nullptr)
    {
        Parent->RemoveChild(this);
    }

    Parent = nullptr;
    Animating = false;
}

auto MCGuiScrollArea::HandleEvent(MCGuiEvent* event) -> void
{
    if (EventRoutine != nullptr)
    {
        EventRoutine(this, event);
    }
}

auto MCGuiScrollArea::Resize(int32_t newWidth, int32_t newHeight) -> void
{
    WinWidth = newWidth;
    FramePane->X1 = FramePane->X0 - 1 + newWidth;
    WinHeight = newHeight;
    FramePane->Y1 = FramePane->Y0 - 1 + newHeight;
}
