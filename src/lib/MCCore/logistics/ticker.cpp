#include "stdafx.h"
#include "logistics/ticker.h"
#include "gui/afont.h"
#include "logistics/logmain.h"
#include "main/logistics.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>Frees one of the ticker's work ports.</summary>
    void freePort(lPort*& port)
    {
        if (port != nullptr)
        {
            port->destroy();
            delete port;
            port = nullptr;
        }
    }
}

Ticker::~Ticker()
{
    Ticker::destroy();
    // The logistics screen's pointer to its ticker.
    globalLogPtr->ticker = nullptr;
}

auto Ticker::init() -> void
{
    text[0] = 0;
    backPane = nullptr;
    textPort = nullptr;
    windowPort = nullptr;
    scrollPos = 0;
    xPos = 0;
    yPos = 0;
    textWidth = 0;
    // Original behaviour (OB-072): timer id 4, but handleEvent only scrolls on timer 7.
    application->AddTimer(this, 4, 0x4b, 0, 0, 0);
}

auto Ticker::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, lPort* port) -> void
{
    lObject::init(xPos, yPos, width, height, nullptr, port);
    setPos(xPos, yPos);
    setMaxWidth(width);
}

auto Ticker::destroy() -> void
{
    // The ticker draws into its owner's port; it must not free it.
    ownPort = nullptr;
    freePort(backPane);
    freePort(textPort);
    freePort(windowPort);
    scrollPos = 0;
    textWidth = 0;
    lObject::destroy();
}

auto Ticker::handleEvent(aEvent* event) -> void
{
    const int32_t width = textWidth;

    if (width == 0)
    {
        return;
    }

    // A wide text shows only while it scrolls (each step on timer 7); every other event painted the back pane alone.
    // Original behaviour (OB-072): the scroll timer has id 4, so a wide text is never shown.
    if (maxWidth < width)
    {
        scrollShown = event->type == 0x13 && event->data == 7;

        if (scrollShown)
        {
            const int32_t pos = scrollPos;

            if (textWidth < pos)
            {
                scrollPos = pos - textWidth;
            }
            else
            {
                scrollPos = pos + 2;
            }
        }
    }
}

auto Ticker::DrawLine(_pane* target) -> void
{
    const int32_t x = xPos;
    const int32_t y = yPos;

    if (backPane != nullptr)
    {
        backPane->copyTo(target, x, y, -1);
    }

    const bool wide = maxWidth < textWidth;

    if (textWidth == 0 || (wide && !scrollShown))
    {
        return;
    }

    // The text picture: the text (after half a window of gap when it is wide) in a picture textWidth wide and a line
    // high, keyed.
    auto* bytes = reinterpret_cast<uint8_t*>(text);
    const int32_t textX = wide ? maxWidth / 2 : 0;
    const int32_t lineHeight = medWhiteFont->height();
    auto writeClipped = [&](int32_t left, int32_t right, int32_t at)
    {
        _pane clip = *target;
        clip.x0 = target->x0 + left;
        clip.y0 = target->y0 + y;
        clip.x1 = target->x0 + right;
        clip.y1 = target->y0 + y + lineHeight - 1;

        if (clip.x0 <= clip.x1)
        {
            medWhiteFont->writeString(&clip, at - left, 0, bytes, -1);
        }
    };

    if (!wide)
    {
        writeClipped(x, x + textWidth - 1, x + textX);
        return;
    }

    // Scrolled: the window shows the picture from scrollPos, and from its start again after its end.
    const int32_t windowRight = x + maxWidth - 1;
    const int32_t pictureEnd = x + textWidth - scrollPos;
    writeClipped(x, std::min(windowRight, pictureEnd - 1), x - scrollPos + textX);

    if (textWidth < scrollPos + maxWidth)
    {
        writeClipped(pictureEnd, windowRight, pictureEnd + textX);
    }
}

auto Ticker::setFont(aFont* newFont) -> void
{
    font = newFont;
}

auto Ticker::setString(char* string) -> void
{
    if (string == nullptr || *string == 0)
    {
        text[0] = 0;
        textWidth = 0;
        scrollPos = 0;
        return;
    }

    if (std::strcmp(text, string) == 0)
    {
        return;
    }

    std::strncpy(text, string, 0xfe);
    const int32_t stringWidth = font->width(reinterpret_cast<uint8_t*>(text));
    textWidth = stringWidth;

    // A scrolling text gets half a window of gap before it repeats. (The original rendered the text into textPort
    // here; DrawLine writes it.)
    if (maxWidth < stringWidth)
    {
        textWidth = maxWidth / 2 + stringWidth;
    }

    scrollPos = 0;
    scrollShown = false;
}

auto Ticker::setPos(int32_t newX, int32_t newY) -> void
{
    xPos = newX;
    yPos = newY;
}

auto Ticker::setMaxWidth(int32_t width) -> void
{
    maxWidth = width;
    freePort(windowPort);
    windowPort = new lPort;
    windowPort->init(width, medWhiteFont->height(), -1);
}

auto Ticker::setPort(lPort* port) -> void
{
    ownPort = port;
}

auto Ticker::setBackPane(lPort* port) -> void
{
    freePort(backPane);
    backPane = new lPort;
    backPane->init(port->width(), port->height(), -1);
    port->copyTo(backPane->frame(), 0, 0, -1);
}
