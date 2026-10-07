#include "stdafx.h"
#include "logistics/ticker.h"
#include "gui/afont.h"
#include "logistics/logmain.h"
#include "main/logistics.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>Frees one of the ticker's work ports.</summary>
    void FreePort(MCLogPort*& port)
    {
        if (port != nullptr)
        {
            port->Destroy();
            delete port;
            port = nullptr;
        }
    }
}

MCTicker::~MCTicker()
{
    MCTicker::Destroy();
    // The logistics screen's pointer to its ticker.
    GlobalLogPtr->Ticker = nullptr;
}

auto MCTicker::Init() -> void
{
    Text[0] = 0;
    BackPane = nullptr;
    TextPort = nullptr;
    WindowPort = nullptr;
    ScrollPos = 0;
    XPos = 0;
    YPos = 0;
    TextWidth = 0;
    // Original behaviour (OB-072): timer id 4, but handleEvent only scrolls on timer 7.
    Application->AddTimer(this, 4, 0x4b, 0, 0, 0);
}

auto MCTicker::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, MCLogPort* port) -> void
{
    MCLogObject::Init(xPos, yPos, width, height, nullptr, port);
    SetPos(xPos, yPos);
    SetMaxWidth(width);
}

auto MCTicker::Destroy() -> void
{
    // The ticker draws into its owner's port; it must not free it.
    _OwnPort = nullptr;
    FreePort(BackPane);
    FreePort(TextPort);
    FreePort(WindowPort);
    ScrollPos = 0;
    TextWidth = 0;
    MCLogObject::Destroy();
}

auto MCTicker::HandleEvent(MCGuiEvent* event) -> void
{
    const int32_t width = TextWidth;

    if (width == 0)
    {
        return;
    }

    // A wide text shows only while it scrolls (each step on timer 7); every other event painted the back pane alone.
    // Original behaviour (OB-072): the scroll timer has id 4, so a wide text is never shown.
    if (MaxWidth < width)
    {
        ScrollShown = event->Type == 0x13 && event->Data == 7;

        if (ScrollShown)
        {
            const int32_t pos = ScrollPos;

            if (TextWidth < pos)
            {
                ScrollPos = pos - TextWidth;
            }
            else
            {
                ScrollPos = pos + 2;
            }
        }
    }
}

auto MCTicker::DrawLine(MCPane* target) -> void
{
    const int32_t x = XPos;
    const int32_t y = YPos;

    if (BackPane != nullptr)
    {
        BackPane->CopyTo(target, x, y, -1);
    }

    const bool wide = MaxWidth < TextWidth;

    if (TextWidth == 0 || (wide && !ScrollShown))
    {
        return;
    }

    // The text picture: the text (after half a window of gap when it is wide) in a picture textWidth wide and a line
    // high, keyed.
    auto* bytes = reinterpret_cast<uint8_t*>(Text);
    const int32_t textX = wide ? MaxWidth / 2 : 0;
    const int32_t lineHeight = MedWhiteFont->Height();
    auto writeClipped = [&](int32_t left, int32_t right, int32_t at)
    {
        MCPane clip = *target;
        clip.X0 = target->X0 + left;
        clip.Y0 = target->Y0 + y;
        clip.X1 = target->X0 + right;
        clip.Y1 = target->Y0 + y + lineHeight - 1;

        if (clip.X0 <= clip.X1)
        {
            MedWhiteFont->WriteString(&clip, at - left, 0, bytes, -1);
        }
    };

    if (!wide)
    {
        writeClipped(x, x + TextWidth - 1, x + textX);
        return;
    }

    // Scrolled: the window shows the picture from scrollPos, and from its start again after its end.
    const int32_t windowRight = x + MaxWidth - 1;
    const int32_t pictureEnd = x + TextWidth - ScrollPos;
    writeClipped(x, std::min(windowRight, pictureEnd - 1), x - ScrollPos + textX);

    if (TextWidth < ScrollPos + MaxWidth)
    {
        writeClipped(pictureEnd, windowRight, pictureEnd + textX);
    }
}

auto MCTicker::SetFont(MCGuiFont* newFont) -> void
{
    Font = newFont;
}

auto MCTicker::SetString(char* string) -> void
{
    if (string == nullptr || *string == 0)
    {
        Text[0] = 0;
        TextWidth = 0;
        ScrollPos = 0;
        return;
    }

    if (std::strcmp(Text, string) == 0)
    {
        return;
    }

    std::strncpy(Text, string, 0xfe);
    const int32_t stringWidth = Font->Width(reinterpret_cast<uint8_t*>(Text));
    TextWidth = stringWidth;

    // A scrolling text gets half a window of gap before it repeats. (The original rendered the text into textPort
    // here; DrawLine writes it.)
    if (MaxWidth < stringWidth)
    {
        TextWidth = MaxWidth / 2 + stringWidth;
    }

    ScrollPos = 0;
    ScrollShown = false;
}

auto MCTicker::SetPos(int32_t newX, int32_t newY) -> void
{
    XPos = newX;
    YPos = newY;
}

auto MCTicker::SetMaxWidth(int32_t width) -> void
{
    MaxWidth = width;
    FreePort(WindowPort);
    WindowPort = new MCLogPort;
    WindowPort->Init(width, MedWhiteFont->Height(), -1);
}

auto MCTicker::SetPort(MCLogPort* port) -> void
{
    _OwnPort = port;
}

auto MCTicker::SetBackPane(MCLogPort* port) -> void
{
    FreePort(BackPane);
    BackPane = new MCLogPort;
    BackPane->Init(port->Width(), port->Height(), -1);
    port->CopyTo(BackPane->Frame(), 0, 0, -1);
}
