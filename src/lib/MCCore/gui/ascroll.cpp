#include "stdafx.h"
#include "gui/ascroll.h"
#include "gui/aport.h"
#include "platform/MCInput.h"
#include "vfx/vfxfuncs.h"

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

/// <remarks>MCX.EXE @ 0x0060cda0</remarks>
auto ScrollEventHandler(aObject* obj, aEvent* event) -> void
{
    // aScrollButton and aScrollArea both keep their id byte at +0x4ac.
    const uint8_t id = static_cast<aScrollButton*>(obj)->buttonId;

    switch (event->type)
    {
        case 1:
        {
            application->grab(obj);
            application->AddTimer(obj, RepeatDelayTimer, 1000, 0, 0, 0);
            aPostMessage(obj->parent, id);
            break;
        }
        case 4:
        {
            application->release();
            application->RemoveTimer(obj, RepeatDelayTimer);
            application->RemoveTimer(obj, RepeatTimer);
            break;
        }
        case TimerEvent:
        {
            if (event->data == RepeatDelayTimer)
            {
                application->RemoveTimer(obj, RepeatDelayTimer);
                application->AddTimer(obj, RepeatTimer, 200, 0, 0, 0);
            }

            aPostMessage(obj->parent, id);
            break;
        }
        default:
            break;
    }
}

/// <remarks>MCX.EXE @ 0x0060ce80</remarks>
auto ScrollTabHandler(aObject* obj, aEvent* event) -> void
{
    auto* bar = static_cast<aScrollBar*>(obj->parent);

    switch (event->type)
    {
        case 1:
        {
            // Centre the cursor on the thumb, then drag it.
            const int32_t cursorX = obj->width() / 2 + obj->globalX();
            const int32_t cursorY = obj->height() / 2 + obj->globalY();
            MCInput::SetCursorPos(cursorX, cursorY);
            application->grab(obj);
            break;
        }

        case 4:
            application->release();
            break;
        case 7:
        {
            if (application->grabbedObject() == obj)
            {
                const int32_t offset = event->y - bar->globalY();
                const int32_t steps = (bar->scrollMax * offset) / (bar->height() - 0x12);
                const float newPos = static_cast<float>(static_cast<double>(steps) + 0.5);

                if (static_cast<int16_t>(static_cast<int32_t>(newPos)) != bar->scrollPos)
                {
                    aEvent dragEvent;
                    dragEvent.clear();
                    dragEvent.type = ThumbDragged;
                    dragEvent.lParam = static_cast<int32_t>(newPos);
                    bar->handleEvent(&dragEvent);
                }
            }
            break;
        }
        default:
            break;
    }
}

/// <remarks>MCX.EXE @ 0x0060cfc0</remarks>
auto ScrollTabPaint(aObject* obj) -> void
{
    const int32_t width = obj->width();
    const int32_t height = obj->height();
    PANE* pane = obj->port()->frame();
    VFX_pane_wipe(pane, 0x39);

    // A black outline, a light bevel top and left, a dark one bottom and right.
    VFX_line_draw(pane, 0, height - 1, width, height - 1, LD_DRAW, 0);
    VFX_line_draw(pane, 0, 0, width, 0, LD_DRAW, 0);
    VFX_line_draw(pane, 0, 0, 0, height - 1, LD_DRAW, 0);
    VFX_line_draw(pane, width - 1, 0, width - 1, height - 1, LD_DRAW, 0);
    VFX_line_draw(pane, 1, 1, width - 2, 1, LD_DRAW, 0x3d);
    VFX_line_draw(pane, 2, 2, width - 3, 2, LD_DRAW, 0x3b);
    VFX_line_draw(pane, 1, 1, 1, height - 2, LD_DRAW, 0x3d);
    VFX_line_draw(pane, 2, 2, 2, height - 3, LD_DRAW, 0x3b);
    VFX_line_draw(pane, width - 2, 1, width - 2, height - 2, LD_DRAW, 0x35);
    VFX_line_draw(pane, width - 3, 2, width - 3, height - 3, LD_DRAW, 0x37);
    VFX_line_draw(pane, 1, height - 2, width - 2, height - 2, LD_DRAW, 0x35);
    VFX_line_draw(pane, 2, height - 3, width - 3, height - 3, LD_DRAW, 0x37);
}

/// <remarks>MCX.EXE @ 0x0060d100</remarks>
aScrollBar::aScrollBar() = default;

/// <remarks>MCX.EXE @ 0x0060d160</remarks>
auto aScrollBar::init(int32_t xPos, int32_t yPos, int32_t, int32_t height, char* name) -> int32_t
{
    constexpr int32_t OutOfMemory = -0x1111fffe;

    int32_t result = aObject::init(xPos, yPos, 0x12, height, name);

    if (result != 0)
    {
        return result;
    }

    upButton = new aScrollButton;

    if (upButton == nullptr)
    {
        return OutOfMemory;
    }

    result = upButton->init(0, 0, 0x10, 8, nullptr);

    if (result != 0)
    {
        return result;
    }

    result = upButton->setBackground(0x27);

    if (result != 0)
    {
        return result;
    }

    addChild(upButton);
    upButton->moveTo(1, 1, 0);
    upButton->buttonId = 0x65;
    upButton->setEventRoutine(ScrollEventHandler);

    downButton = new aScrollButton;

    if (downButton == nullptr)
    {
        return OutOfMemory;
    }

    result = downButton->init(0, 0, 0x10, 8, nullptr);

    if (result != 0)
    {
        return result;
    }

    result = downButton->setBackground(0x28);

    if (result != 0)
    {
        return result;
    }

    addChild(downButton);
    downButton->moveTo(1, height - 9, 0);
    downButton->buttonId = 0x66;
    downButton->setEventRoutine(ScrollEventHandler);

    upArea = new aScrollArea;

    if (upArea == nullptr)
    {
        return OutOfMemory;
    }

    result = upArea->init(0, 0, 0x10, 1, nullptr);

    if (result != 0)
    {
        return result;
    }

    addChild(upArea);
    upArea->moveTo(1, 9, 0);
    upArea->areaId = 0x67;
    upArea->setEventRoutine(ScrollEventHandler);

    downArea = new aScrollArea;

    if (downArea == nullptr)
    {
        return OutOfMemory;
    }

    result = downArea->init(0, 0, 0x10, 1, nullptr);

    if (result != 0)
    {
        return result;
    }

    downArea->moveTo(1, this->height() - 1, 0);
    addChild(downArea);
    downArea->areaId = 0x68;
    downArea->setEventRoutine(ScrollEventHandler);

    scrollTab = new aObject;

    if (scrollTab == nullptr)
    {
        return OutOfMemory;
    }

    result = scrollTab->init(0, 0, 0x10, 0x10, nullptr);

    if (result != 0)
    {
        return result;
    }

    addChild(scrollTab);
    scrollTab->moveTo(1, 8, 0);
    scrollTab->setBackColor(6);
    scrollTab->setEventRoutine(ScrollTabHandler);
    scrollTab->setPaintRoutine(ScrollTabPaint);

    setBackColor(0xb);
    moveTo(xPos, yPos, 0);
    setDepth(10);
    upArea->setDepth(0xb);
    downArea->setDepth(0xb);
    upButton->setDepth(0xb);
    downButton->setDepth(0xb);
    scrollTab->setDepth(-0xc);
    SetScrollMax(0);
    SetScrollPos(0);
    return 0;
}

/// <remarks>MCX.EXE @ 0x0060d560</remarks>
auto aScrollBar::destroy() -> void
{
    auto release = [](auto*& child)
    {
        if (child != nullptr)
        {
            child->destroy();
            delete child;
            child = nullptr;
        }
    };

    release(upArea);
    release(downArea);
    release(upButton);
    release(downButton);
    release(scrollTab);
    aObject::destroy();
}

/// <remarks>MCX.EXE @ 0x0060d630</remarks>
auto aScrollBar::handleEvent(aEvent* event) -> void
{
    int16_t newPos;

    switch (event->type)
    {
        case 0x65:
            newPos = static_cast<int16_t>(scrollPos - 1);
            break;
        case 0x66:
            newPos = static_cast<int16_t>(scrollPos + 1);
            break;
        case 0x67:
            newPos = static_cast<int16_t>(scrollPos - 10);
            break;
        case 0x68:
            newPos = static_cast<int16_t>(scrollPos + 10);
            break;
        case ThumbDragged:
            newPos = static_cast<int16_t>(event->lParam);
            break;
        case SetPositionQuietly:
        {
            SetScrollPos(static_cast<int16_t>(event->lParam));
            return;
        }
        default:
            return;
    }

    SetScrollPos(newPos);

    if (parent != nullptr)
    {
        aPostMessage(parent, ScrollChanged);
    }
}

/// <remarks>MCX.EXE @ 0x0060d6d0</remarks>
auto aScrollBar::draw() -> void
{
    VFX_pane_wipe(displayPort->frame(), backgroundColor);
    VFX_line_draw(displayPort->frame(), 0, 0, width() - 1, 0, LD_DRAW, 0xf);
    VFX_line_draw(displayPort->frame(), width() - 1, 0, width() - 1, height() - 1, LD_DRAW, 0xf);
    VFX_line_draw(displayPort->frame(), 0, height() - 1, width() - 1, height() - 1, LD_DRAW, 0xf);
    VFX_line_draw(displayPort->frame(), 0, 0, 0, height() - 1, LD_DRAW, 0xf);
    aObject::draw();
}

/// <remarks>MCX.EXE @ 0x0060d7b0</remarks>
auto aScrollBar::SetScrollMax(int16_t newMax) -> void
{
    scrollMax = newMax;
    scrollTab->ShowGUIWindow(newMax != 0);
    ResizeAreas();
}

/// <remarks>MCX.EXE @ 0x0060d7f0</remarks>
auto aScrollBar::SetScrollPos(int16_t newPos) -> void
{
    if (newPos < 0)
    {
        newPos = 0;
    }

    if (newPos > scrollMax)
    {
        newPos = scrollMax;
    }

    scrollPos = newPos;
    ResizeAreas();
}

/// <remarks>MCX.EXE @ 0x0060d820</remarks>
auto aScrollBar::ResizeAreas() -> void
{
    if (scrollMax == 0)
    {
        return;
    }

    // The thumb (16 high) travels between the arrows (9 each) and a 1-pixel border.
    const int32_t tabTop = (scrollPos * (height() - 0x22)) / scrollMax;
    scrollTab->moveTo(1, tabTop + 9, 0);
    upArea->resize(0x10, tabTop);
    downArea->resize(0x10, height() + (-0x11 - (tabTop + 0x10)));
    downArea->moveTo(1, tabTop + 0x19, 0);
    draw();
}

/// <remarks>MCX.EXE @ 0x0060d8d0</remarks>
auto aScrollArea::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char*) -> int32_t
{
    winWidth = width;
    winY = yPos;
    winHeight = height;
    winX = xPos;
    winState = 0;
    showWindow = 1;
    dragOn = 0;
    displayPort = nullptr;

    if (framePane != nullptr)
    {
        delete framePane;
        framePane = nullptr;
    }

    framePane = new (std::nothrow) _pane;

    if (framePane == nullptr)
    {
        return -0x1111fffe;
    }

    framePane->window = screenPort->bitmap();
    framePane->x0 = xPos;
    framePane->y0 = yPos;
    framePane->x1 = xPos + width;
    framePane->y1 = yPos + height;
    paintRoutine = nullptr;
    eventRoutine = nullptr;
    numChildren = 0;
    parent = nullptr;
    winDepth = 0;
    windowAnimation = nullptr;
    animating = 0;
    iconAnimation = nullptr;
    return 0;
}

/// <remarks>MCX.EXE @ 0x0060d9b0</remarks>
auto aScrollArea::destroy() -> void
{
    if (framePane != nullptr)
    {
        delete framePane;
        framePane = nullptr;
    }

    numChildren = 0;

    if (parent != nullptr)
    {
        parent->removeChild(this);
    }

    parent = nullptr;
    animating = 0;
}

/// <remarks>MCX.EXE @ 0x0060d9f0</remarks>
auto aScrollArea::handleEvent(aEvent* event) -> void
{
    if (eventRoutine != nullptr)
    {
        eventRoutine(this, event);
    }
}

/// <remarks>MCX.EXE @ 0x0060da10</remarks>
auto aScrollArea::resize(int32_t newWidth, int32_t newHeight) -> void
{
    winWidth = newWidth;
    framePane->x1 = framePane->x0 - 1 + newWidth;
    winHeight = newHeight;
    framePane->y1 = framePane->y0 - 1 + newHeight;
}
