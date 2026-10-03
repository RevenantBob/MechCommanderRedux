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

    if (maxWidth < width)
    {
        if (event->type == 0x13 && event->data == 7)
        {
            const int32_t pos = scrollPos;
            paint(true, pos);

            if (textWidth < pos)
            {
                scrollPos = pos - textWidth;
            }
            else
            {
                scrollPos = pos + 2;
            }
        }
        else
        {
            paint(false, -1);
        }
    }
    else
    {
        paint(true, -1);
    }
}

auto Ticker::paint(bool withText, int32_t scroll) -> void
{
    LogScreenChrome* chrome = paintScreen != nullptr ? paintScreen->Chrome() : nullptr;

    if (chrome != nullptr)
    {
        if (backPane != nullptr && ownPort != nullptr)
        {
            chrome->tickerPainted = true;
            chrome->tickerTextShown = false;
        }

        chrome->tickerX = xPos;
        chrome->tickerY = yPos;

        if (withText)
        {
            chrome->tickerTextShown = true;
            std::snprintf(chrome->tickerText, sizeof(chrome->tickerText), "%s", text);
            chrome->tickerTextX = maxWidth < font->width(reinterpret_cast<uint8_t*>(text)) ? maxWidth / 2 : 0;
            chrome->tickerScroll = scroll;
            chrome->tickerTextWidth = textWidth;
            chrome->tickerMaxWidth = maxWidth;
        }

        return;
    }

    if (backPane != nullptr && ownPort != nullptr)
    {
        backPane->copyTo(ownPort->frame(), xPos, yPos, -1);
        backPane->copyTo(windowPort->frame(), 0, 0, -1);
    }

    if (!withText)
    {
        return;
    }

    if (scroll < 0)
    {
        textPort->copyTo(ownPort->frame(), xPos, yPos, -1);
        return;
    }

    VFX_pane_copy(textPort->frame(), scroll, 0, windowPort->frame(), 0, 0, -1);

    // Past the end the text starts again after it.
    if (textWidth < scroll + maxWidth)
    {
        textPort->copyTo(windowPort->frame(), textWidth - scroll, 0, -1);
    }

    windowPort->copyTo(ownPort->frame(), xPos, yPos, -1);
}

auto Ticker::drawPainted(const LogScreenChrome& chrome, _pane* target) -> void
{
    const int32_t x = chrome.tickerX;
    const int32_t y = chrome.tickerY;

    if (chrome.tickerPainted && backPane != nullptr)
    {
        backPane->copyTo(target, x, y, -1);
    }

    if (!chrome.tickerTextShown)
    {
        return;
    }

    // The text picture: the text at tickerTextX in a picture tickerTextWidth wide and a line high, keyed.
    auto* bytes = reinterpret_cast<uint8_t*>(const_cast<char*>(chrome.tickerText));
    const int32_t lineHeight = medWhiteFont->height();
    auto writeClipped = [&](int32_t left, int32_t right, int32_t textX)
    {
        _pane clip = *target;
        clip.x0 = target->x0 + left;
        clip.y0 = target->y0 + y;
        clip.x1 = target->x0 + right;
        clip.y1 = target->y0 + y + lineHeight - 1;

        if (clip.x0 <= clip.x1)
        {
            medWhiteFont->writeString(&clip, textX - left, 0, bytes, -1);
        }
    };

    if (chrome.tickerScroll < 0)
    {
        writeClipped(x, x + chrome.tickerTextWidth - 1, x + chrome.tickerTextX);
        return;
    }

    // Scrolled: the window shows the picture from tickerScroll, and from its start again after its end.
    const int32_t windowRight = x + chrome.tickerMaxWidth - 1;
    const int32_t pictureEnd = x + chrome.tickerTextWidth - chrome.tickerScroll;
    writeClipped(x, std::min(windowRight, pictureEnd - 1), x - chrome.tickerScroll + chrome.tickerTextX);

    if (chrome.tickerTextWidth < chrome.tickerScroll + chrome.tickerMaxWidth)
    {
        writeClipped(pictureEnd, windowRight, pictureEnd + chrome.tickerTextX);
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
        if (backPane != nullptr && ownPort != nullptr)
        {
            if (LogScreenChrome* chrome = paintScreen != nullptr ? paintScreen->Chrome() : nullptr; chrome != nullptr)
            {
                chrome->tickerPainted = true;
                chrome->tickerTextShown = false;
                chrome->tickerX = xPos;
                chrome->tickerY = yPos;
            }
            else
            {
                backPane->copyTo(ownPort->frame(), xPos, yPos, -1);
            }
        }

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
    freePort(textPort);
    textPort = new lPort;
    // A scrolling text gets half a window of gap before it repeats.
    int32_t textX = 0;

    if (maxWidth < stringWidth)
    {
        textX = maxWidth / 2;
        textWidth = textX + stringWidth;
    }

    textPort->init(textWidth, medWhiteFont->height(), -1);
    VFX_pane_wipe(textPort->frame(), 0xff);
    medWhiteFont->writeString(textPort->frame(), textX, 0, reinterpret_cast<uint8_t*>(string), -1);
    scrollPos = 0;
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
