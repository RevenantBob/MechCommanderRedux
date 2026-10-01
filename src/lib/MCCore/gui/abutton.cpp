#include "stdafx.h"
#include "gui/abutton.h"
#include "gui/aport.h"
#include "lib/aerror.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>Destroys and deletes a picture (destroy through the vtable first: aPort has no virtual destructor).</summary>
    void freePicture(aPort*& picture)
    {
        if (picture != nullptr)
        {
            picture->destroy();
            delete picture;
            picture = nullptr;
        }
    }

    /// <summary>Destroys and deletes a callback.</summary>
    void freeCallback(aCallback*& callback)
    {
        if (callback != nullptr)
        {
            callback->destroy();
            delete callback;
            callback = nullptr;
        }
    }

    /// <summary>
    /// Replaces <paramref name="picture"/> with one loaded from <paramref name="source"/> (a file name or an art
    /// packet). With <paramref name="sizeButton"/> the button takes the picture's size and loses its back colour
    /// (0xff); a picture that fails to load is dropped.
    /// </summary>
    template <typename Source> void loadPicture(aButton* button, aPort*& picture, Source source, bool sizeButton)
    {
        freePicture(picture);
        aPort* port = new aPort;
        picture = port;

        if (port->init(source) == 0)
        {
            if (sizeButton)
            {
                button->backgroundColor = 0xff;
                button->resize(port->width(), port->height());
            }

            return;
        }

        delete port;
        picture = nullptr;
    }

    /// <summary>Whether a mouse event lies on <paramref name="button"/> (its rectangle in its parent's coordinates).</summary>
    bool eventOnButton(aButton* button, aEvent* event, bool redraw)
    {
        RECT rect;
        rect.left = button->x();
        rect.top = button->y();
        rect.right = button->x() + button->width();
        rect.bottom = button->y() + button->height();
        aObject* parent = button->parent;
        const int32_t parentX = parent->globalX();
        const int32_t eventX = event->x;
        const int32_t parentY = parent->globalY();
        const int32_t eventY = event->y;

        if (redraw)
        {
            button->draw();
        }

        POINT point;
        point.x = eventX - parentX;
        point.y = eventY - parentY;
        return PtInRect(&rect, point) != 0;
    }
}

// aButton

auto aButton::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    int32_t result = aObject::init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    upPicture = nullptr;
    downPicture = nullptr;
    grayPicture = nullptr;
    leftCallback = new aCallback;
    rightButtonCallback = new aCallback;
    disabled = 0;
    framed = -1;
    backgroundColor = 0;
    VFX_pane_wipe(displayPort->frame(), 0);
    return 0;
}

auto aButton::destroy() -> void
{
    freePicture(upPicture);
    freePicture(downPicture);
    freePicture(grayPicture);
    freeCallback(leftCallback);
    freeCallback(rightButtonCallback);
    aObject::destroy();
}

auto aButton::setUpPicture(char* fileName) -> void
{
    loadPicture(this, upPicture, fileName, true);
}

auto aButton::setGrayPicture(char* fileName) -> void
{
    loadPicture(this, grayPicture, fileName, true);
}

auto aButton::setDownPicture(char* fileName) -> void
{
    loadPicture(this, downPicture, fileName, false);
}

auto aButton::setUpPicture(int32_t artPacket) -> void
{
    loadPicture(this, upPicture, artPacket, true);
}

auto aButton::setGrayPicture(int32_t artPacket) -> void
{
    loadPicture(this, grayPicture, artPacket, true);
}

auto aButton::setDownPicture(int32_t artPacket) -> void
{
    loadPicture(this, downPicture, artPacket, false);
}

auto aButton::handleEvent(aEvent* event) -> void
{
    if (disabled != 0)
    {
        return;
    }

    switch (event->type)
    {
        case 1:
        {
            application->grab(this);
            draw();
            break;
        }
        case 3:
            application->grab(this);
            break;
        case 4:
        {
            if (application->grabbedObject() == this)
            {
                application->release();

                if (eventOnButton(this, event, true))
                {
                    leftCallback->execute();
                }
            }
            break;
        }
        case 6:
        {
            if (application->grabbedObject() == this)
            {
                application->release();

                if (eventOnButton(this, event, false))
                {
                    rightButtonCallback->execute();
                }
            }
            break;
        }
    }

    if (eventRoutine != nullptr)
    {
        eventRoutine(this, event);
    }
}

auto aButton::draw() -> void
{
    aPort* picture;

    if (disabled != 0)
    {
        if (framed != 0)
        {
            drawFramed(0, -1);
        }

        picture = grayPicture;
    }
    else if (application->grabbedObject() == this)
    {
        if (framed != 0)
        {
            drawFramed(-1, -1);
        }

        picture = downPicture;
    }
    else
    {
        if (framed != 0)
        {
            drawFramed(0, -1);
        }

        picture = upPicture;
    }

    // The frame is drawn first, so an opaque picture covers it.
    if (picture != nullptr)
    {
        picture->copyTo(displayPort->frame(), 0, 0, 0);
        aObject::draw();
        return;
    }

    VFX_pane_wipe(displayPort->frame(), backgroundColor);
    aObject::draw();
}

// aCloseButton

auto aCloseButton::handleEvent(aEvent* event) -> void
{
    if (disabled != 0)
    {
        return;
    }

    if (event->type == 4 && application->grabbedObject() == this)
    {
        application->release();

        if (eventOnButton(this, event, true))
        {
            // The window is closing: the event routine isn't run.
            leftCallback->execute();
            return;
        }
    }

    const int32_t type = event->type;

    if (type == 1)
    {
        application->grab(this);
        draw();
    }
    else if (type == 3)
    {
        application->grab(this);
    }
    else if (type == 6 && application->grabbedObject() == this)
    {
        application->release();

        if (eventOnButton(this, event, false))
        {
            rightButtonCallback->execute();
        }
    }

    if (eventRoutine != nullptr)
    {
        eventRoutine(this, event);
    }
}

// aToolButton

auto aToolButton::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    pushed = 0;
    return aButton::init(xPos, yPos, width, height, name);
}

auto aToolButton::handleEvent(aEvent* event) -> void
{
    if (event->type == 1 && disabled == 0)
    {
        pushed = pushed == 0 ? 1 : 0;
        draw();
        leftCallback->execute();
        aObject::handleEvent(event);
        return;
    }

    aButton::handleEvent(event);
}

auto aToolButton::draw() -> void
{
    if (disabled != 0)
    {
        if (framed != 0)
        {
            drawFramed(0, -1);
        }

        if (grayPicture != nullptr)
        {
            grayPicture->copyTo(displayPort->frame(), 0, 0, -1);
            aObject::draw();
            return;
        }

        VFX_pane_wipe(displayPort->frame(), backgroundColor);
        aObject::draw();
        return;
    }

    // Pictures are drawn transparently, then the frame over them (unfilled).
    if (pushed != 0)
    {
        if (downPicture != nullptr)
        {
            downPicture->copyTo(displayPort->frame(), 0, 0, -1);

            if (framed != 0)
            {
                drawFramed(-1, 0);
            }
        }
        else if (framed != 0)
        {
            drawFramed(-1, -1);
        }
    }
    else
    {
        if (upPicture != nullptr)
        {
            upPicture->copyTo(displayPort->frame(), 0, 0, -1);

            if (framed != 0)
            {
                drawFramed(0, 0);
            }
        }
        else if (framed != 0)
        {
            drawFramed(0, -1);
        }
    }

    aObject::draw();
}

// aSpinnerButton

auto aSpinnerButton::handleEvent(aEvent* event) -> void
{
    switch (event->type)
    {
        case 1:
        {
            pushed = -1;
            draw();
            application->grab(this);
            application->AddTimer(this, 1, 1000, 0, 0, 0);
            leftCallback->execute();
            break;
        }
        case 4:
        {
            pushed = 0;
            draw();
            application->release();
            application->RemoveTimer(this, 1);
            application->RemoveTimer(this, 2);
            break;
        }
        case 0x13:
        {
            // Timer 1 (the first second held) hands over to the repeating timer 2.
            const int32_t timer = event->data;

            if (timer == 1)
            {
                application->RemoveTimer(this, 1);
                application->AddTimer(this, 2, 0xfa, 0, 0, 0);
            }

            if (timer == 2)
            {
                leftCallback->execute();
            }
            break;
        }
    }

    aObject::handleEvent(event);
}

auto aSpinnerButton::draw() -> void
{
    if (pushed != 0)
    {
        if (downPicture != nullptr)
        {
            downPicture->copyTo(displayPort->frame(), 0, 0, -1);
        }
    }
    else if (upPicture != nullptr)
    {
        upPicture->copyTo(displayPort->frame(), 0, 0, -1);
    }

    aObject::draw();
}

// aSpinner

auto aSpinner::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    (void)name;
    aObject* owner = parent;

    if (owner == nullptr)
    {
        Fatal(0, "Hey Scott! You have to set the parent before the init! Remember?");
    }

    int32_t result = aObject::init(xPos, yPos, width, height, nullptr);

    if (result != 0)
    {
        return result;
    }

    // aObject::init clears the parent; put it back.
    setParent(owner);

    auto* up = new aSpinnerButton;
    upButton = up;

    if (up == nullptr)
    {
        return 3;
    }

    result = up->init(1, 1, 10, 10, nullptr);

    if (result != 0)
    {
        return result;
    }

    auto* down = new aSpinnerButton;
    downButton = down;

    if (down == nullptr)
    {
        return 3;
    }

    result = down->init(1, 1, 10, 10, nullptr);

    if (result != 0)
    {
        return result;
    }

    up->setUpPicture(0xc);
    up->setDownPicture(0xd);
    down->setUpPicture(0x25);
    down->setDownPicture(0x26);
    up->draw();
    down->draw();
    const int32_t newWidth = down->width() < up->width() ? up->width() : down->width();
    resize(newWidth, up->height() + down->height());
    addChild(upButton);
    addChild(downButton);
    upButton->moveTo(0, 0, 0);
    downButton->moveTo(0, upButton->height(), 0);
    upButton->callback()->setMessage(parent, 0x15);
    downButton->callback()->setMessage(parent, 0x16);
    return -1;
}

auto aSpinner::destroy() -> void
{
    if (upButton != nullptr)
    {
        upButton->destroy();
        delete upButton;
        upButton = nullptr;
    }

    if (downButton != nullptr)
    {
        downButton->destroy();
        delete downButton;
        downButton = nullptr;
    }

    aObject::destroy();
}
