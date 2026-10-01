#include "stdafx.h"
#include "gui/awindow.h"
#include "camera/camera.h"
#include "engine/font.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "lib/aerror.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "lib/packet.h"
#include "logistics/logmain.h"
#include "network/multplyr.h"
#include "platform/MCInput.h"
#include "platform/MCSmacker.h"
#include "sound/soundsys.h"
#include "terrain/terrain.h"
#include "vfx/vfxfuncs.h"

int32_t startupRects[24] = {};
int32_t noiseSample = 0;
int movieOver = 0;

namespace
{
    /// <summary>Destroys and frees a frame part, and clears the pointer.</summary>
    template <typename T> void destroyPart(T*& part)
    {
        if (part != nullptr)
        {
            part->destroy();
            delete part;
            part = nullptr;
        }
    }

    /// <summary>Makes a plain frame part: a new aObject, fatal when out of memory.</summary>
    aObject* newPart(const char* outOfMemory)
    {
        aObject* part = new aObject;

        if (part == nullptr)
        {
            Fatal(0, outOfMemory);
        }

        return part;
    }

    /// <summary>Snaps a size to the 40-pixel grid of a grid-aligned window, rounding to the nearest step.</summary>
    void snapToGrid(int32_t& newWidth, int32_t& newHeight)
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
    void placeBarButtons(aObject* bar, TButton* const* buttons, int32_t numButtons, int16_t maxLength, int horizontal,
                         int32_t buttonWidth, int32_t buttonHeight)
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

        bar->resize(columns * buttonWidth, barHeight);
        int16_t column = 0;
        int16_t row = 0;

        for (int16_t i = 0; i < numButtons; i++)
        {
            buttons[i]->moveTo(column * buttonWidth, row * buttonHeight, 0);

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
    void keepTitleBarInside(aTitleBar* bar, const tagRECT& area)
    {
        aObject* window = bar->parent;

        if (bar->rectIntersect(area) != 0)
        {
            return;
        }

        if (area.right < bar->frame()->x0)
        {
            window->moveTo(area.right, window->frame()->y0, 0);
        }

        if (bar->frame()->x0 + bar->width() < area.left)
        {
            window->moveTo(area.left - bar->width(), window->frame()->y0, 0);
        }

        if (area.bottom < bar->frame()->y0)
        {
            window->moveTo(bar->frame()->x0, area.bottom + 0xd, 0);
        }

        if (bar->frame()->y0 + bar->height() < area.top)
        {
            window->moveTo(bar->frame()->x0, area.top, 0);
        }
    }

    /// <summary>Clears the whole screen buffer (a full-screen movie's background).</summary>
    void clearScreen()
    {
        // Port: the original cleared 640x480 bytes (0x96000 in 16-bit mode); the port clears the buffer at its
        // real size.
        _window* screen = screenPort->frame()->window;
        std::memset(screen->buffer, 0, static_cast<size_t>(screen->x_max + 1) * static_cast<size_t>(screen->y_max + 1));
    }

    /// <summary>Remaps a windowed movie onto the game's current palette (<c>SmackColorRemap</c>).</summary>
    void remapToGamePalette(SmackTag* movie)
    {
        VFX_RGB palette[256];
        std::memcpy(palette, application->currentPalette, sizeof(palette));
        movie->Player->ColorRemap(reinterpret_cast<const uint8_t*>(palette), 0x100);
    }
}

// Frame paint routines.

auto titleBarPaint(aObject* object) -> void
{
    VFX_pane_wipe(object->port()->frame(), 0x1b);
    VFX_line_draw(object->port()->frame(), 0, 6, object->width() - 1, 6, LD_DRAW, 0x10);
    VFX_line_draw(object->port()->frame(), 0, 7, object->width() - 1, 7, LD_DRAW, 0x10);
}

auto cameraBottomBarPaint(aObject* object) -> void
{
    VFX_pane_wipe(object->port()->frame(), 0x10);
    VFX_line_draw(object->port()->frame(), 0, 0, 0, 1, LD_DRAW, 0x15);
    VFX_line_draw(object->port()->frame(), 1, 0, 1, 1, LD_DRAW, 0x10);
}

auto cameraLeftBarPaint(aObject* object) -> void
{
    VFX_line_draw(object->port()->frame(), 0, 0, 0, object->height(), LD_DRAW, 0x15);
    VFX_line_draw(object->port()->frame(), 1, 0, 1, object->height(), LD_DRAW, 0x10);
}

auto cameraRightBarPaint(aObject* object) -> void
{
    VFX_pane_wipe(object->port()->frame(), 0x10);
}

auto bottomBarPaint(aObject* object) -> void
{
    object->SetBit(0, 0, 0xe);
    object->SetBit(object->width(), 0, 0xe);
    VFX_line_draw(object->port()->frame(), 1, 0, object->width(), 0, LD_DRAW, 3);
    VFX_line_draw(object->port()->frame(), 0, 1, object->width(), 1, LD_DRAW, 0xe);
}

auto leftBarPaint(aObject* object) -> void
{
    object->SetBit(object->width(), object->height(), 0xe);
    VFX_line_draw(object->port()->frame(), 0, 0, 0, object->height(), LD_DRAW, 0xe);
    VFX_line_draw(object->port()->frame(), 1, 0, 1, object->height() - 1, LD_DRAW, 3);
}

auto rightBarPaint(aObject* object) -> void
{
    object->SetBit(0, object->height(), 0xe);
    VFX_pane_wipe(object->port()->frame(), 8);
    VFX_line_draw(object->port()->frame(), 0, 0, 0, object->height() - 2, LD_DRAW, 3);
    VFX_line_draw(object->port()->frame(), 1, 0, 1, object->height() - 1, LD_DRAW, 0xe);
}

auto handleResizeButtonEvent(aObject* object, aEvent* event) -> void
{
    if (object->parent == nullptr)
    {
        return;
    }

    // The window is the handle's parent's parent (the handle sits in the right frame bar).
    aObject* window = object->parent->parent;

    if (window == nullptr)
    {
        return;
    }

    switch (event->type)
    {
        case 1: // left button down
        {
            MCInput::SetCapture();
            application->grab(object);
            break;
        }
        case 4: // left button up
        {
            application->release();
            MCInput::ReleaseCapture();
            window->draw();
            break;
        }
        case 7: // mouse move
        {
            if (application->grabbedObject() == object)
            {
                window->resize(event->x - window->globalX(), event->y - window->globalY());
                window->draw();
            }
            break;
        }
    }
}

auto handleSwoopyButtonEvent(aObject* object, aEvent* event) -> void
{
    if (event->type == 1)
    {
        application->grab(object);
        object->draw();
    }
    else if (event->type == 4 && application->grabbedObject() == object && object->parent != nullptr)
    {
        aObject* window = object->parent->parent;

        if (window != nullptr && window->GetCamera() != nullptr)
        {
            window->GetCamera()->swoopy ^= 1;
            application->release();
            object->draw();
        }
    }
}

// aTitleButton

auto aTitleButton::handleEvent(aEvent* event) -> void
{
    aObject::handleEvent(event);
}

// aTitleWindow

aTitleWindow::aTitleWindow()
{
}

aTitleWindow::~aTitleWindow()
{
    aTitleWindow::destroy();
}

auto aTitleWindow::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    int32_t result = aObject::init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    SetTransparent(0);

    titleBar = new aTitleBar;

    if (titleBar == nullptr)
    {
        Fatal(0, "Not enough memory to allocate titlebar");
    }

    result = titleBar->init(0, 0, width + 0xc, 0xd, name);

    if (result != 0)
    {
        return result;
    }

    addChild(titleBar);
    titleBar->showCloseButton(1);
    titleBar->moveTo(-2, -0xd, 0);
    titleBar->SetZoomCallbacks();

    leftBar = newPart("Not enough memory to allocate left bar");
    // The left bar's init result is not checked.
    leftBar->init(0, 0, 2, height, nullptr);
    leftBar->setPaintRoutine(leftBarPaint);
    addChild(leftBar);
    leftBar->moveTo(-2, 0, 0);

    bottomBar = newPart("Not enough memory to allocate bottom bar");
    result = bottomBar->init(0, 0, width + 4, 2, nullptr);

    if (result != 0)
    {
        return result;
    }

    bottomBar->setPaintRoutine(bottomBarPaint);
    addChild(bottomBar);
    bottomBar->moveTo(-2, height, 0);

    rightBar = newPart("Not enough memory to allocate right bar");
    result = rightBar->init(0, 0, 10, height + 2, nullptr);

    if (result != 0)
    {
        return result;
    }

    rightBar->setPaintRoutine(rightBarPaint);
    addChild(rightBar);
    rightBar->moveTo(width, 0, 0);

    resizeButton = newPart("Not enough memory to allocate resize area");
    result = resizeButton->init(0, 0, 8, 8, nullptr);

    if (result != 0)
    {
        return result;
    }

    resizeButton->setBackground(0x29);
    resizeButton->draw();
    rightBar->addChild(resizeButton);
    resizeButton->moveTo(2, rightBar->height() - resizeButton->height(), 0);
    resizeButton->setEventRoutine(handleResizeButtonEvent);

    moveTo(xPos, yPos + 0xd, 0);
    return 0;
}

auto aTitleWindow::destroy() -> void
{
    destroyPart(titleBar);
    destroyPart(leftBar);
    destroyPart(resizeButton);
    destroyPart(rightBar);
    destroyPart(bottomBar);
    aObject::destroy();
}

auto aTitleWindow::draw() -> void
{
    VFX_pane_wipe(displayPort->frame(), backColor());

    if (dragging() == 0 || winState == 2)
    {
        aObject::draw();
        return;
    }

    // While dragged, only the frame is drawn (the right bar twice, as the original).
    titleBar->draw();
    leftBar->draw();
    rightBar->draw();
    rightBar->draw();
    bottomBar->draw();

    if (resizeButton != nullptr)
    {
        resizeButton->draw();
    }
}

auto aTitleWindow::resize(int32_t newWidth, int32_t newHeight) -> void
{
    if (newWidth < 0 || newHeight < 0 || titleBar->ResizeOK(newWidth) == 0)
    {
        return;
    }

    if (gridAligned != 0)
    {
        snapToGrid(newWidth, newHeight);
    }

    aObject::resize(newWidth, newHeight);
    titleBar->resize(newWidth + 0xc, titleBar->height());
    leftBar->resize(leftBar->width(), newHeight);
    rightBar->resize(rightBar->width(), newHeight);
    rightBar->moveTo(newWidth, 0, 0);
    bottomBar->resize(newWidth + 4, bottomBar->height());
    bottomBar->moveTo(-2, newHeight, 0);
    resizeButton->moveTo(2, rightBar->height() - resizeButton->height(), 0);
}

auto aTitleWindow::setTitle(char* newTitle) -> void
{
    if (titleBar != nullptr)
    {
        titleBar->setTitle(newTitle);
    }
}

// aTitleBar

aTitleBar::aTitleBar()
{
}

auto aTitleBar::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    int32_t result = aObject::init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    setBackColor(8);
    font = blackFont;

    if (name == nullptr)
    {
        name = const_cast<char*>(" ");
    }

    // Unbounded, as the original.
    std::strcpy(title, name);

    closeButton = new aCloseButton;
    result = closeButton->init(0, 0, 4, 4, nullptr);

    if (result != 0)
    {
        return result;
    }

    closeButton->setUpPicture(0x12);
    closeButton->setDownPicture(0x13);
    addChild(closeButton);
    closeButton->moveTo(winWidth - closeButton->width(), 0, 0);

    zoomButton = new aButton;
    result = zoomButton->init(0, 0, 4, 4, nullptr);

    if (result != 0)
    {
        return result;
    }

    zoomButton->setUpPicture(0x14);
    zoomButton->setDownPicture(0x15);
    addChild(zoomButton);
    zoomButton->moveTo(0, 0, 0);

    zoomOutButton = new aButton;
    result = zoomOutButton->init(0, 0, 4, 4, nullptr);

    if (result != 0)
    {
        return result;
    }

    zoomOutButton->setUpPicture(0x16);
    zoomOutButton->setDownPicture(0x17);
    addChild(zoomOutButton);
    zoomOutButton->moveTo(zoomButton->x() + zoomButton->width(), 0, 0);
    showZoomButtons(0);

    swoopyButton = new aTitleButton;
    result = swoopyButton->init(0, 0, 4, 4, nullptr);

    if (result != 0)
    {
        return result;
    }

    swoopyButton->setUpPicture(0x18);
    swoopyButton->setDownPicture(0x19);
    addChild(swoopyButton);
    swoopyButton->moveTo(winWidth - closeButton->width() - swoopyButton->width(), 0, 0);
    swoopyButton->setEventRoutine(handleSwoopyButtonEvent);
    showSwoopyButton(0);
    return 0;
}

auto aTitleBar::destroy() -> void
{
    destroyPart(closeButton);
    destroyPart(zoomButton);
    destroyPart(zoomOutButton);
    destroyPart(swoopyButton);
    aObject::destroy();
}

auto aTitleBar::SetZoomCallbacks() -> void
{
    if (parent == nullptr)
    {
        return;
    }

    zoomButton->callback()->setMessage(parent, 0x1a);
    zoomOutButton->callback()->setMessage(parent, 0x1b);
}

auto aTitleBar::setTitle(char* newTitle) -> void
{
    std::strcpy(title, newTitle);
}

auto aTitleBar::handleEvent(aEvent* event) -> void
{
    switch (event->type)
    {
        case 1: // left button down: start dragging the window
        {
            aObject* window = parent;
            lastX = event->x - window->x();
            lastY = event->y - window->y();

            if (window != nullptr && window->parent != nullptr &&
                window->parent->foremostChild(window->depth()) != window)
            {
                window->bringToFront(0);
                aRedrawScreen();
            }

            application->grab(this);
            parent->startDrag(parent->x(), parent->y());
            parent->draw();
            return;
        }

        case 4: // left button up: drop the window
        {
            if (application->grabbedObject() != this)
            {
                return;
            }

            aObject* window = parent;
            const tagRECT area = application->scrollRect;
            window->stopDrag();

            for (int16_t i = 9; i < window->numberOfChildren(); i++)
            {
                window->child(i)->ShowGUIWindow(1);
            }

            const int32_t mouseX = event->x;
            const int32_t mouseY = event->y;
            window->moveTo(mouseX - lastX, mouseY - lastY, 0);
            keepTitleBarInside(this, area);
            parent->draw();
            application->release();
            aObject* under = screenWindow->findObject(mouseX, mouseY);

            if (under != nullptr)
            {
                under->enter();
            }

            return;
        }

        case 7: // mouse move: drag, unless over a depth-100 (modal) object
        {
            if (application->grabbedObject() != this)
            {
                return;
            }

            const int32_t mouseX = event->x;
            const int32_t mouseY = event->y;
            const tagRECT area = application->scrollRect;
            aObject* under = screenWindow->findObject(mouseX, mouseY);

            if (under == nullptr || under->depth() != 100)
            {
                parent->moveTo(mouseX - lastX, mouseY - lastY, 0);
                keepTitleBarInside(this, area);
            }

            return;
        }

        case 0xc:
        {
            draw();
            return;
        }
    }
}

auto aTitleBar::draw() -> void
{
    VFX_pane_wipe(displayPort->frame(), backColor());
    aPort* barPort = displayPort;
    VFX_line_draw(barPort->frame(), 0, 0, width(), 0, LD_DRAW, 0xe);
    VFX_line_draw(barPort->frame(), 0, 0, 0, 0xd, LD_DRAW, 0xe);

    if (parent != nullptr)
    {
        barPort = displayPort;
        VFX_line_draw(barPort->frame(), 1, height() - 1, parent->width() + 2, height() - 1, LD_DRAW, 3);
    }

    int32_t textX = 3;

    if (zoomButton != nullptr && zoomButton->IsShowing() != 0)
    {
        textX = zoomButton->x() + zoomButton->width() + zoomOutButton->width() + 3;
    }

    font->writeString(barPort->frame(), textX, 3, reinterpret_cast<uint8_t*>(title), -1);
    aObject::draw();
}

auto aTitleBar::showCloseButton(int show) -> void
{
    if (closeButton == nullptr)
    {
        return;
    }

    closeButton->ShowGUIWindow(show);

    if (show != 0)
    {
        closeButton->callback()->setMessage(parent, 0xd);
    }
}

auto aTitleBar::showZoomButton(int show) -> void
{
    if (zoomButton != nullptr)
    {
        zoomButton->ShowGUIWindow(show);
    }
}

auto aTitleBar::showZoomButtons(int show) -> void
{
    if (zoomButton != nullptr)
    {
        zoomButton->ShowGUIWindow(show);
    }

    if (zoomOutButton != nullptr)
    {
        zoomOutButton->ShowGUIWindow(show);
    }
}

auto aTitleBar::showSwoopyButton(int show) -> void
{
    if (swoopyButton != nullptr)
    {
        swoopyButton->ShowGUIWindow(show);
    }
}

auto aTitleBar::resize(int32_t newWidth, int32_t newHeight) -> void
{
    aObject::resize(newWidth, newHeight);
    const int32_t barWidth = winWidth;
    closeButton->moveTo(barWidth - closeButton->width(), 0, 0);
    swoopyButton->moveTo(barWidth - closeButton->width() - swoopyButton->width(), 0, 0);
}

auto aTitleBar::ResizeOK(int32_t newWidth) -> int
{
    int32_t needed = 4;

    if (closeButton != nullptr && closeButton->IsShowing() != 0)
    {
        needed = closeButton->width() + 5;
    }

    if (zoomButton != nullptr && zoomButton->IsShowing() != 0)
    {
        needed += zoomButton->width() + 1;
    }

    if (zoomOutButton != nullptr && zoomOutButton->IsShowing() != 0)
    {
        needed += zoomOutButton->width() + 1;
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

aMenu::aMenu()
{
}

auto aMenu::ShowGUIWindow(int show) -> void
{
    showWindow = show;

    if (show != 0)
    {
        shown = 1;
    }
}

auto aMenu::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    font = nullptr;
    numItems = 0;
    selectedItem = -1;
    itemText = nullptr;
    int32_t result = aObject::init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    font = whiteFont;
    itemText = static_cast<char*>(guiHeap->malloc(1000));

    if (itemText == nullptr)
    {
        return static_cast<int32_t>(0xbadd0001);
    }

    for (int32_t i = 0; i < MaxMenuItems; i++)
    {
        itemCallbacks[i] = nullptr;
        itemData[i] = -1;
        itemLetters[i] = 0;
    }

    std::memset(itemText, 0, 1000);
    setBackColor(0);
    moveTo(xPos, yPos, 0);
    itemHeight = font->height() + 8;
    hasLetters = 0;
    rightAligned = 0;
    return 0;
}

auto aMenu::destroy() -> void
{
    if (itemText != nullptr)
    {
        guiHeap->free(itemText);
        itemText = nullptr;
    }

    for (int16_t i = 0; i < numItems; i++)
    {
        if (itemCallbacks[i] != nullptr)
        {
            delete itemCallbacks[i];
            itemCallbacks[i] = nullptr;
        }
    }

    numItems = 0;
    selectedItem = -1;
    aObject::destroy();
}

auto aMenu::handleEvent(aEvent* event) -> void
{
    if (event->type == 4)
    {
        // A release runs the item under the cursor, then hides the menu wherever it happened.
        const int32_t mouseY = event->y;
        selectedItem = (mouseY - globalY()) / itemHeight;

        if (pointInside(event->x, mouseY) != 0 && selectedItem > -1 && selectedItem < numItems &&
            itemCallbacks[selectedItem] != nullptr)
        {
            itemCallbacks[selectedItem]->execute();
        }

        ShowGUIWindow(0);
        application->release();
    }
    else if (event->type == 7)
    {
        const int32_t item = (event->y - globalY()) / itemHeight;

        if (item != selectedItem)
        {
            selectedItem = item;
            draw();
        }
    }

    aObject::handleEvent(event);
}

auto aMenu::draw() -> void
{
    const int32_t halfItem = itemHeight / 2;
    int32_t itemY = 0;
    char* text = itemText;
    VFX_pane_wipe(displayPort->frame(), backgroundColor);
    aObject::draw();

    if (dragging() != 0)
    {
        return;
    }

    VFX_line_draw(displayPort->frame(), 0, 0, width() - 1, 0, LD_DRAW, 0xf);
    VFX_line_draw(displayPort->frame(), width() - 1, 0, width() - 1, height() - 1, LD_DRAW, 0xf);
    VFX_line_draw(displayPort->frame(), 0, height() - 1, width() - 1, height() - 1, LD_DRAW, 0xf);
    VFX_line_draw(displayPort->frame(), 0, 0, 0, height() - 1, LD_DRAW, 0xf);

    for (int16_t i = 0; i < numItems; i++)
    {
        if (std::strcmp(text, MenuSeparator) == 0)
        {
            const int32_t lineY = halfItem + itemY;
            VFX_line_draw(displayPort->frame(), 4, lineY, width() - 8, lineY, LD_DRAW, 9);
        }
        else
        {
            if (i == selectedItem)
            {
                FillBox(1, static_cast<int16_t>(itemY + 1), static_cast<int16_t>(width() - 1),
                        static_cast<int16_t>(itemY + itemHeight), 0xb);
            }

            int32_t textX = 2;

            if (rightAligned != 0)
            {
                textX = width() + (-6 - font->width(reinterpret_cast<uint8_t*>(text)));
            }

            font->writeString(displayPort->frame(), textX, itemY + 4, reinterpret_cast<uint8_t*>(text), -1);
            const char letter = itemLetters[i];

            if (letter != 0)
            {
                const int32_t letterX = width() + (-3 - font->width(static_cast<uint8_t>(letter)));
                font->writeChar(displayPort->frame(), letterX, itemY + 4, letter);
            }
        }

        VFX_line_draw(displayPort->frame(), 1, itemY, width() - 2, itemY, LD_DRAW, 0xf);
        text += MenuItemLength;
        itemY += itemHeight;
    }
}

auto aMenu::ResizeMenu() -> void
{
    uint8_t* text = reinterpret_cast<uint8_t*>(itemText);
    const int32_t menuHeight = (font->height() + 8) * numItems;
    int32_t menuWidth = 0;

    for (int16_t i = 0; i < numItems; i++)
    {
        if (static_cast<double>(menuWidth) < static_cast<double>(font->width(text) + 4) * 1.25)
        {
            menuWidth = static_cast<int32_t>(static_cast<double>(font->width(text) + 4) * 1.25);
        }

        text += MenuItemLength;
    }

    if (hasLetters != 0)
    {
        menuWidth += 6 + font->width(reinterpret_cast<uint8_t*>(const_cast<char*>("W")));
    }

    if (width() == menuWidth && height() == menuHeight)
    {
        return;
    }

    resize(menuWidth, menuHeight);
}

auto aMenu::AddItem(char* text) -> int32_t
{
    const int32_t index = numItems;

    if (index > MaxMenuItems - 1)
    {
        return static_cast<int32_t>(0xeeee0001);
    }

    aCallback* callback = new aCallback;

    if (callback == nullptr)
    {
        return static_cast<int32_t>(0xeeee0002);
    }

    itemCallbacks[index] = callback;
    itemData[numItems] = 0;

    if (std::strlen(text) > MenuItemLength - 1)
    {
        text[MenuItemLength - 1] = 0;
    }

    std::strcpy(itemText + numItems * MenuItemLength, text);
    numItems++;
    ResizeMenu();
    return numItems - 1;
}

auto aMenu::RemoveItem(char* text) -> int
{
    int16_t i = 0;

    while (i < numItems && MCPort::StrICmp(itemText + i * MenuItemLength, text) != 0)
    {
        i++;
    }

    return RemoveItem(i);
}

auto aMenu::RemoveItem(int16_t index) -> int
{
    if (index >= numItems)
    {
        return 0;
    }

    if (itemCallbacks[index] != nullptr)
    {
        delete itemCallbacks[index];
    }

    int16_t i = index;

    while (i < numItems - 1)
    {
        itemCallbacks[i] = itemCallbacks[i + 1];
        itemData[i] = itemData[i + 1];
        i++;
    }

    itemCallbacks[i] = nullptr;
    itemData[i] = 0;
    // The letters are not shifted with the items.
    char* slot = itemText + index * MenuItemLength;
    std::memmove(slot, slot + MenuItemLength,
                 static_cast<size_t>(static_cast<int16_t>((0x18 - index) * MenuItemLength)));
    numItems--;
    ResizeMenu();
    return 1;
}

auto aMenu::AddSeparator() -> int32_t
{
    const int32_t index = numItems;

    if (index > MaxMenuItems - 1)
    {
        return static_cast<int32_t>(0xeeee0001);
    }

    std::strcpy(itemText + index * MenuItemLength, MenuSeparator);
    numItems = index + 1;
    return index + 1;
}

auto aMenu::SetCallback(int16_t index, void (*exec)()) -> void
{
    if (index < numItems)
    {
        itemCallbacks[index]->setExec(exec);
    }
}

auto aMenu::SetMessage(int16_t index, aObject* target, int32_t message) -> void
{
    if (index < numItems)
    {
        itemCallbacks[index]->setMessage(target, message);
    }
}

auto aMenu::ChangeItemString(int16_t index, char* text) -> int32_t
{
    if (index >= numItems)
    {
        return static_cast<int32_t>(0xeeee0003);
    }

    if (std::strlen(text) > MenuItemLength - 1)
    {
        text[MenuItemLength - 1] = 0;
    }

    std::strcpy(itemText + index * MenuItemLength, text);
    ResizeMenu();
    return 0;
}

auto aMenu::KeepOnScreen() -> void
{
    const tagRECT area = application->scrollRect;

    if (x() < area.left)
    {
        moveTo(area.left, y(), 0);
    }

    if (y() < area.top)
    {
        moveTo(x(), area.top, 0);
    }

    if (area.right < x() + width())
    {
        moveTo(area.right - width() - 10, y(), 0);
    }

    if (area.bottom < y() + height())
    {
        moveTo(x(), area.bottom - height() - 10, 0);
    }
}

auto aMenu::SetItemData(int16_t index, int32_t data) -> void
{
    if (index < numItems)
    {
        itemData[index] = data;
    }
}

auto aMenu::GetItemData(int16_t index) -> int32_t
{
    if (index >= numItems)
    {
        return static_cast<int32_t>(0xeeee0003);
    }

    return itemData[index];
}

auto aMenu::SetItemLetter(int16_t index, char letter) -> void
{
    if (index < numItems)
    {
        itemLetters[index] = letter;
        hasLetters = 1;
        ResizeMenu();
    }
}

auto aMenu::GetItemLetter(int16_t index) -> char
{
    if (index >= numItems)
    {
        return 3;
    }

    return itemLetters[index];
}

// aToolBar

aToolBar::aToolBar()
{
}

auto aToolBar::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    const int32_t result = aTitleWindow::init(xPos, yPos, width, height, name);
    setBackColor(0);

    if (titleBar != nullptr)
    {
        titleBar->showCloseButton(0);
    }

    maxLength = 8;
    return result;
}

auto aToolBar::destroy() -> void
{
    for (int16_t i = 0; i < numButtons; i++)
    {
        destroyPart(buttons[i]);
    }

    aTitleWindow::destroy();
}

auto aToolBar::AddButton(aToolButton* button) -> int32_t
{
    if (numButtons > MaxMenuItems - 1)
    {
        return static_cast<int32_t>(0xeeee0001);
    }

    buttons[numButtons] = button;
    numButtons++;
    addChild(button);
    PlaceButtons();
    return 0;
}

auto aToolBar::RemoveButton(int32_t index) -> int32_t
{
    if (index >= numButtons)
    {
        return static_cast<int32_t>(0xeeee0003);
    }

    removeChild(GetButton(index));

    while (index < numButtons - 1)
    {
        buttons[index] = buttons[index + 1];
        index++;
    }

    // Original bug (OB-064): clears the slot past the last button, not the last one; with a full bar that slot is
    // `horizontal` (+0x52c), which is zeroed.
    if (numButtons < MaxMenuItems)
    {
        buttons[numButtons] = nullptr;
    }
    else
    {
        horizontal = 0;
    }

    numButtons--;
    PlaceButtons();
    return 0;
}

auto aToolBar::GetButton(int32_t index) -> aToolButton*
{
    if (index > MaxMenuItems - 1)
    {
        return nullptr;
    }

    return buttons[index];
}

auto aToolBar::IsPushed(int32_t index) -> int
{
    if (GetButton(index) == nullptr)
    {
        return 0;
    }

    return GetButton(index)->pushed;
}

auto aToolBar::SetHorizontal(int on) -> void
{
    horizontal = on;
    PlaceButtons();
}

auto aToolBar::SetMaxLength(int16_t length) -> void
{
    maxLength = length;
    PlaceButtons();
}

auto aToolBar::PlaceButtons() -> void
{
    placeBarButtons(this, buttons, numButtons, maxLength, horizontal, buttonWidth, buttonHeight);
}

auto aToolBar::SetButtonSize(int32_t width, int32_t height) -> void
{
    buttonWidth = width;
    buttonHeight = height;
}

// aWindowBar

aWindowBar::aWindowBar()
{
}

auto aWindowBar::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    const int32_t result = aObject::init(xPos, yPos, width, height, name);
    setBackColor(0);
    maxLength = 8;
    return result;
}

auto aWindowBar::destroy() -> void
{
    for (int16_t i = 0; i < numButtons; i++)
    {
        destroyPart(buttons[i]);
    }

    aObject::destroy();
}

auto aWindowBar::InsertButton(aButton* button, int32_t index) -> int32_t
{
    if (numButtons >= MaxMenuItems || index > numButtons)
    {
        return static_cast<int32_t>(0xeeee0001);
    }

    for (int32_t i = numButtons; i > index; i--)
    {
        buttons[i] = buttons[i - 1];
    }

    buttons[index] = button;
    numButtons++;
    addChild(button);
    PlaceButtons();
    return 0;
}

auto aWindowBar::AddButton(aButton* button) -> int32_t
{
    if (numButtons > MaxMenuItems - 1)
    {
        return static_cast<int32_t>(0xeeee0001);
    }

    buttons[numButtons] = button;
    numButtons++;
    addChild(button);
    PlaceButtons();
    return 0;
}

auto aWindowBar::RemoveButton(int32_t index) -> int32_t
{
    if (index >= numButtons)
    {
        return static_cast<int32_t>(0xeeee0003);
    }

    removeChild(GetButton(index));

    while (index < numButtons - 1)
    {
        buttons[index] = buttons[index + 1];
        index++;
    }

    // Original bug (OB-064): as aToolBar::RemoveButton; past a full bar the slot is `horizontal` (+0x518).
    if (numButtons < MaxMenuItems)
    {
        buttons[numButtons] = nullptr;
    }
    else
    {
        horizontal = 0;
    }

    numButtons--;
    PlaceButtons();
    return 0;
}

auto aWindowBar::GetButton(int32_t index) -> aButton*
{
    if (index > MaxMenuItems - 1)
    {
        return nullptr;
    }

    return buttons[index];
}

auto aWindowBar::SetHorizontal(int on) -> void
{
    horizontal = on;
    PlaceButtons();
}

auto aWindowBar::SetMaxLength(int16_t length) -> void
{
    maxLength = length;
    PlaceButtons();
}

auto aWindowBar::PlaceButtons() -> void
{
    placeBarButtons(this, buttons, numButtons, maxLength, horizontal, buttonWidth, buttonHeight);
}

auto aWindowBar::SetButtonSize(int32_t width, int32_t height) -> void
{
    buttonWidth = width;
    buttonHeight = height;
}

// aSmackerWindow
//
// Port: a full-screen movie (fullScreen set while the game is full screen) switched the original's display to
// 16-bit and let Smacker draw to the DirectDraw surface in its own colours. The port's display stays 8-bit, so a
// full-screen movie decodes straight into the window's frame and puts up its own palette (checkSmackerPalette), as
// the original did in a windowed game. A windowed movie decodes into its own pane, remapped to the game palette.

aSmackerWindow::aSmackerWindow()
{
}

auto aSmackerWindow::init(tagRECT* area, tagPOINT* position) -> int32_t
{
    int32_t left = area->left;
    int32_t top = area->top;

    if (position != nullptr)
    {
        left += position->x;
        top += position->y;
    }

    // The rectangle's right and bottom are the width and height.
    return aObject::init(left, top, area->right, area->bottom, nullptr);
}

auto aSmackerWindow::startSmackerMovie(SmackTag* newMovie, int fullScreenPlay) -> int32_t
{
    fullScreen = fullScreenPlay;
    movie = newMovie;
    movieOver = 0;

    if (fullScreen != 0)
    {
        return 0;
    }

    moviePane = static_cast<_pane*>(guiHeap->malloc(sizeof(_pane)));

    if (moviePane == nullptr)
    {
        return static_cast<int32_t>(0xd4d40000);
    }

    *moviePane = *frame();
    _window* movieWindow = static_cast<_window*>(guiHeap->malloc(sizeof(_window)));
    moviePane->window = movieWindow;

    if (movieWindow == nullptr)
    {
        return static_cast<int32_t>(0xd4d40000);
    }

    if (moviePane->x1 < 0 || moviePane->y1 < 0)
    {
        movieWindow->buffer = nullptr;
        return static_cast<int32_t>(0xd4d40000);
    }

    movieWindow->x_max = moviePane->x1;
    movieWindow->y_max = moviePane->y1;
    movieWindow->buffer = static_cast<uint8_t*>(
        guiHeap->malloc(static_cast<uint32_t>((movieWindow->y_max + 1) * (movieWindow->x_max + 1))));

    if (movieWindow->buffer == nullptr)
    {
        return static_cast<int32_t>(0xd4d40000);
    }

    remapToGamePalette(movie);
    return 0;
}

auto aSmackerWindow::destroy() -> void
{
    SmackClose(movie);
    movie = nullptr;

    if (fullScreen == 0)
    {
        if (moviePane != nullptr && moviePane->window != nullptr)
        {
            guiHeap->free(moviePane->window->buffer);
            guiHeap->free(moviePane->window);
        }

        guiHeap->free(moviePane);
        moviePane = nullptr;
    }

    aObject::destroy();
    screenWindow->removeChild(this);
}

auto aSmackerWindow::endSmackerMovie() -> void
{
    SmackClose(movie);
    movie = nullptr;
    movieOver = 1;
    destroy();
}

auto aSmackerWindow::checkSmackerPalette() -> void
{
    // Port fix: the original reads the movie unguarded.
    if (movie == nullptr || !movie->Player->NewPalette())
    {
        return;
    }

    if (fullScreen == 0)
    {
        remapToGamePalette(movie);
        return;
    }

    application->activateSmackerPalette(const_cast<uint8_t*>(movie->Player->Palette().data()));
}

auto aSmackerWindow::display() -> void
{
    if (showWindow == 0)
    {
        return;
    }

    if (IsHidden() != 0 && hideOffset == 0)
    {
        return;
    }

    if (movie == nullptr)
    {
        if (fullScreen != 0)
        {
            clearScreen();
        }
        else
        {
            VFX_pane_wipe(moviePane, 0);
        }

        return;
    }

    MCSmackerPlayer* player = movie->Player.get();

    if (static_cast<uint32_t>(width()) < static_cast<uint32_t>(player->Width()) ||
        static_cast<uint32_t>(height()) < static_cast<uint32_t>(player->Height()))
    {
        Fatal(0, "Movie is too big for its window");
    }

    _pane* target = fullScreen == 0 ? moviePane : frame();
    player->ToBuffer(target->x0, target->y0, target->window->x_max + 1, player->Height(), target->window->buffer);

    if (firstFrame != 0)
    {
        if (fullScreen == 0)
        {
            VFX_pane_wipe(moviePane, 0);
        }
        else
        {
            clearScreen();
        }

        firstFrame = 0;
    }

    if (!player->Wait() && nextFrame() == 0)
    {
        movieOver = 1;
    }

    if (fullScreen == 0 && moviePane != nullptr)
    {
        VFX_pane_copy(moviePane, 0, 0, frame(), 0, 0, -1);
    }
}

auto aSmackerWindow::draw() -> void
{
    if (movie != nullptr)
    {
        aObject::draw();
    }
}

auto aSmackerWindow::nextFrame() -> int
{
    MCSmackerPlayer* player = movie->Player.get();
    // Port: SmackToBufferRect (collecting the changed rectangle) has no use here; the whole frame is copied.
    (void)player->DoFrame();

    if (player->FrameNum() == player->Frames() - 1)
    {
        return 0;
    }

    player->NextFrame();
    return 1;
}

auto aSmackerWindow::findObject(int32_t, int32_t) -> aObject*
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

aStartupWindow::aStartupWindow()
{
    std::memcpy(startupRects, StartupPoints, sizeof(startupRects));
}

auto aStartupWindow::destroy() -> void
{
    for (uint8_t*& image : staticImages)
    {
        std::free(image);
        image = nullptr;
    }

    aObject::destroy();
    screenWindow->removeChild(this);
}

auto aStartupWindow::doStatic() -> void
{
    for (int32_t row = 0; row < height(); row++)
    {
        if (RollDice(0x1e) == 0)
        {
            // Now and then a whole row is copied from a random one.
            if (RollDice(0x32) != 0)
            {
                uint8_t* buffer = frame()->window->buffer;
                uint8_t* dest = buffer + width() * row;
                const int32_t sourceRow = RandomNumber(height());
                uint8_t* source = frame()->window->buffer + sourceRow * width();
                std::memmove(dest, source, static_cast<size_t>(width()));
            }
        }
        else
        {
            for (int32_t column = 0; column < width(); column++)
            {
                AG_pixel_write(frame(), column, row, static_cast<uint32_t>(std::rand()) & 0x1f);
            }
        }
    }
}

auto aStartupWindow::endStatic() -> void
{
    VFX_pane_wipe(frame(), 0);
}

auto aStartupWindow::display() -> void
{
    if (showWindow == 0)
    {
        return;
    }

    if (IsHidden() != 0 && hideOffset == 0)
    {
        return;
    }

    SoundSystem* sounds = soundSystem;
    const int32_t step = startupState;
    startupState = step + 1;
    Font* font = lineFont;

    if (sounds == nullptr)
    {
        return;
    }

    // Once the sequence is over, the screen shows noise.
    if (startupState > 0xaa)
    {
        if (RollDice(10) != 0)
        {
            doStatic();
            AG_shape_draw(frame(), staticImages[2], 0, 0x140, 0xf0);
            return;
        }

        if (RollDice(0x1e) == 0)
        {
            return;
        }

        soundSystem->update();
        doStatic();
        AG_shape_draw(frame(), staticImages[2], 1, 0x140, 0xf0);
        return;
    }

    const int32_t pointX = startupRects[randomStart * 2];
    const int32_t pointY = startupRects[randomStart * 2 + 1];
    auto setLarge = [font](int large)
    {
        font->scale = large != 0 ? 1.6f : 1.0f;
        font->scaled = large;
    };

    // The text is typed two letters a frame: the first piece restarts the line, the next ones follow the width
    // typed so far (measured by `measured`, which is the piece itself except once), the last one isn't measured.
    auto typeFirst = [&](int32_t lineX, int32_t lineY, const char* text)
    {
        setLarge(0);
        font->print(lineX, lineY, const_cast<char*>(text), 0xfd, frame());
        textX = font->printWidth(const_cast<char*>(text), 0);
    };

    auto typeNext = [&](int32_t lineX, int32_t lineY, const char* text, const char* measured)
    {
        const int32_t typed = textX;
        setLarge(0);
        font->print(typed + lineX, lineY, const_cast<char*>(text), 0xfd, frame());
        textX = font->printWidth(const_cast<char*>(measured), 0) + typed;
    };

    auto typeLast = [&](int32_t lineX, int32_t lineY, const char* text)
    {
        setLarge(0);
        font->print(textX + lineX, lineY, const_cast<char*>(text), 0xfd, frame());
    };

    // The finished picture: the map, the bunker and its uplink to the chosen point.
    auto drawUplink = [&]()
    {
        AG_shape_draw(frame(), staticImages[0], 0, 0x140, 0xf0);
        AG_ellipse_fill(frame(), 0x10a, 0xe5, 3, 3, 0xfd);
        VFX_line_draw(frame(), 0x10a, 0xe5, 0x1c1, 400, LD_DRAW, 0xfd);
        setLarge(0);
        font->print(0x1c2, 0x18b, const_cast<char*>("Forward Command Bunker"), 0xfd, frame());
        setLarge(1);
        font->print(0x1c2, 0x19f, const_cast<char*>("Uplinking..."), 0xfc, frame());
        VFX_line_draw(frame(), 0x10a, 0xe5, pointX, pointY, LD_DRAW, 0xfe);
        VFX_line_draw(frame(), pointX, pointY, 10, pointY, LD_DRAW, 0xfd);
    };

    uint32_t sample = 0x10;

    switch (step)
    {
        case 0:
        {
            AG_shape_draw(frame(), staticImages[0], 0, 0x140, 0xf0);
            sample = 0x11;
            break;
        }
        case 9:
        {
            AG_ellipse_fill(frame(), 0x10a, 0xe5, 3, 3, 0xfd);
            sample = 0xf;
            break;
        }
        case 0x13:
            VFX_line_draw(frame(), 0x10a, 0xe5, 0x1c1, 400, LD_DRAW, 0xfd);
            break;
        // "Forward Command Bunker"
        case 0x1d:
        {
            textX = 0;
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
            font->print(0x1c2, 0x19f, const_cast<char*>("Uplinking..."), 0xfc, frame());
            break;
        }
        case 0x31:
            VFX_line_draw(frame(), 0x10a, 0xe5, pointX, pointY, LD_DRAW, 0xfe);
            break;
        case 0x3b:
        {
            sounds->playDigitalSample(0x20, 1, nullptr, 0, 0);
            soundSystem->update();
            doStatic();
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
            VFX_pane_wipe(frame(), 0);
            drawUplink();
            soundSystem->update();
            doStatic();
            AG_shape_draw(frame(), staticImages[1], 0, 0x140, 0xf0);
            return;
        }
        case 0x45:
        {
            endStatic();
            drawUplink();
            soundSystem->playDigitalSample(0x10, 1, nullptr, 0, 0);
            soundSystem->update();
            AG_ellipse_fill(frame(), pointX, pointY, 2, 2, 0xfc);
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
            sounds->playDigitalSample(0x10, 1, nullptr, 0, 0);
            soundSystem->update();
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
            VFX_line_draw(frame(), pointX, pointY, 0xf3, 0x101, LD_DRAW, 0xfd);
            break;
        case 0x81:
        {
            AG_ellipse_fill(frame(), 0xf3, 0x101, 5, 5, 0xfb);
            sample = 0xf;
            break;
        }
        case 0x8b:
            VFX_line_draw(frame(), 0xf3, 0x101, 0x1b, 0x101, LD_DRAW, 0xfd);
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
            font->print(0x1b, 0x113, const_cast<char*>("GO"), 0xfd, frame());
            sample = 0x11;
            break;
        }
        case 0xa9:
        {
            noiseSample = sounds->playDigitalSample(0x21, 0, nullptr, 0, 0);
            soundSystem->update();
            doStatic();
            AG_shape_draw(frame(), staticImages[2], 0, 0x140, 0xf0);
            return;
        }
        default:
            return;
    }

    soundSystem->playDigitalSample(sample, 1, nullptr, 0, 0);
    soundSystem->update();
}

auto aStartupWindow::draw() -> void
{
    aObject::draw();
}

auto aStartupWindow::setup() -> int32_t
{
    File file;
    // Reads art packet `packet` whole into a CRT block.
    auto loadPacket = [&file](int32_t packet, uint8_t*& image) -> int32_t
    {
        int32_t result = artFile->seekPacket(packet);

        if (result != 0)
        {
            return result;
        }

        result = file.open(artFile, static_cast<uint32_t>(artFile->getPacketSize()), 0x32);

        if (result != 0)
        {
            return result;
        }

        image = static_cast<uint8_t*>(std::malloc(file.fileSize()));

        if (image == nullptr)
        {
            return -1;
        }

        file.read(image, static_cast<int32_t>(file.fileSize()));
        file.close();
        return 0;
    };

    for (int32_t i = 0; i < 3; i++)
    {
        const int32_t result = loadPacket(0x2d + i, staticImages[i]);

        if (result != 0)
        {
            return result;
        }
    }

    frameCount = 0;
    randomStart = RandomNumber(0xc);
    return 0;
}

// aEmptyTitleWindow

aEmptyTitleWindow::aEmptyTitleWindow()
{
}

aEmptyTitleWindow::~aEmptyTitleWindow()
{
    aEmptyTitleWindow::destroy();
}

auto aEmptyTitleWindow::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    int32_t result = aHolderObject::init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    titleBar = new aTitleBar;

    if (titleBar == nullptr)
    {
        Fatal(0, "Not enough memory to allocate titlebar");
    }

    result = titleBar->init(0, 0, width + 4, 8, name);

    if (result != 0)
    {
        return result;
    }

    addChild(titleBar);
    titleBar->setDepth(1);
    titleBar->showCloseButton(0);
    titleBar->moveTo(-2, -8, 0);
    titleBar->SetZoomCallbacks();

    leftBar = newPart("Not enough memory to allocate left bar");
    // The left bar's init result is not checked.
    leftBar->init(0, 0, 2, height, nullptr);
    leftBar->setPaintRoutine(cameraLeftBarPaint);
    addChild(leftBar);
    leftBar->moveTo(-2, 0, 0);

    bottomBar = newPart("Not enough memory to allocate bottom bar");
    result = bottomBar->init(0, 0, width + 4, 2, nullptr);

    if (result != 0)
    {
        return result;
    }

    bottomBar->setPaintRoutine(cameraBottomBarPaint);
    addChild(bottomBar);
    bottomBar->moveTo(-2, height, 0);

    rightBar = newPart("Not enough memory to allocate right bar");
    result = rightBar->init(0, 0, 2, height, nullptr);

    if (result != 0)
    {
        return result;
    }

    rightBar->setPaintRoutine(cameraRightBarPaint);
    addChild(rightBar);
    rightBar->moveTo(width, 0, 0);

    resizeButton = newPart("Not enough memory to allocate resize area");
    result = resizeButton->init(0, 0, 8, 8, nullptr);

    if (result != 0)
    {
        return result;
    }

    resizeButton->setBackground(0x29);
    resizeButton->draw();
    // Original bug (OB-066): the handle is the window's own child here, so handleResizeButtonEvent resizes the
    // window's parent (the screen window), not the window.
    addChild(resizeButton);
    resizeButton->moveTo(2 - resizeButton->width() + this->width(), 2 - resizeButton->height() + this->height(), 0);
    resizeButton->setEventRoutine(handleResizeButtonEvent);
    resizeButton->setDepth(1);

    moveTo(xPos, yPos + 8, 0);
    return 0;
}

auto aEmptyTitleWindow::destroy() -> void
{
    destroyPart(titleBar);
    destroyPart(leftBar);
    destroyPart(rightBar);
    destroyPart(bottomBar);
    destroyPart(resizeButton);
    aHolderObject::destroy();
}

auto aEmptyTitleWindow::resize(int32_t newWidth, int32_t newHeight) -> void
{
    if (newWidth < 0 || newHeight < 0 || titleBar->ResizeOK(newWidth) == 0)
    {
        return;
    }

    if (gridAligned != 0)
    {
        snapToGrid(newWidth, newHeight);
    }

    aHolderObject::resize(newWidth, newHeight);
    titleBar->resize(newWidth + 4, titleBar->height());
    leftBar->resize(leftBar->width(), newHeight);
    rightBar->resize(rightBar->width(), newHeight);
    rightBar->moveTo(newWidth, 0, 0);
    bottomBar->resize(newWidth + 4, bottomBar->height());
    bottomBar->moveTo(-2, newHeight, 0);
    resizeButton->moveTo(2 - resizeButton->width() + width(), 2 - resizeButton->height() + height(), 0);
}

auto aEmptyTitleWindow::handleEvent(aEvent* event) -> void
{
    aObject* pane = panes[0];

    if (pane != nullptr)
    {
        if (event->type == 0xd)
        {
            // Close: the camera goes off and the window leaves the screen.
            if (pane->GetCamera() != nullptr)
            {
                pane->GetCamera()->deactivate();
            }

            screenWindow->removeChild(this);
            return;
        }

        if (event->type == 0x1a)
        {
            // Zoom: the camera flips between full scale (100) and 1; only 1 in multiplayer, while paused or asked,
            // or when only the 45-degree art is loaded.
            Camera* camera = pane->GetCamera();

            if (camera != nullptr)
            {
                if (only45Pixel == 0 && gamePaused == 0 && gameAsked == 0 && MPlayer == nullptr)
                {
                    camera->cameraScale = camera->cameraScale != 100 ? 100 : 1;
                }
                else
                {
                    camera->cameraScale = 1;
                }

                camera->forceUpdate = 1;
                Terrain::forceRedraw = 1;
            }
        }
        else if (event->type == 0x1c)
        {
            if (pane->GetCamera() != nullptr)
            {
                pane->GetCamera()->changeTarget(nullptr, 0);
            }
        }
    }

    aObject::handleEvent(event);
}

auto aEmptyTitleWindow::setTitle(char* newTitle) -> void
{
    if (titleBar != nullptr)
    {
        titleBar->setTitle(newTitle);
    }
}

auto aEmptyTitleWindow::setBackColor(int32_t color) -> void
{
    if (titleBar != nullptr)
    {
        titleBar->setBackColor(color);
    }
}
