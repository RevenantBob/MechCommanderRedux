#include "stdafx.h"
#include "logistics/MCTicker.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCGuiFont.h"
#include "main/MCLogistics.h"

MCTicker::MCTicker()
{
    // Original behaviour (OB-072): the timer's id is 4, but HandleEvent only scrolls on timer 7.
    GuiSystem()->AddTimer(this, ScrollTimer, 0x4b, 0, 0, false);
}

MCTicker::~MCTicker()
{
    MCTicker::Destroy();

    if (GlobalLogPtr != nullptr && GlobalLogPtr->Ticker.get() == this)
    {
        GlobalLogPtr->Ticker = nullptr;
    }
}

auto MCTicker::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height) -> void
{
    InitWithoutPort(xPos, yPos, width, height);
    SetPos(xPos, yPos);
    MaxWidth = width;
}

auto MCTicker::Destroy() -> void
{
    BackPane.reset();
    ScrollPos = 0;
    TextWidth = 0;
    MCLogObject::Destroy();
}

auto MCTicker::HandleEvent(MCGuiEvent* event) -> void
{
    if (TextWidth == 0 || MaxWidth >= TextWidth)
    {
        return;
    }

    // A wide text shows only while it scrolls; every other event painted the back pane alone.
    ScrollShown = event->Type == MCGuiEventType::Timer && event->Data == ScrollStepTimer;

    if (ScrollShown)
    {
        ScrollPos = TextWidth < ScrollPos ? ScrollPos - TextWidth : ScrollPos + 2;
    }
}

auto MCTicker::DrawLine(MCPane* target) const -> void
{
    const int32_t x = XPos;
    const int32_t y = YPos;

    if (BackPane != nullptr)
    {
        BackPane->CopyTo(target, x, y, true);
    }

    const bool wide = MaxWidth < TextWidth;

    if (TextWidth == 0 || (wide && !ScrollShown))
    {
        return;
    }

    // The text picture: the text (after half a line of gap when it is wide) in a picture TextWidth wide and a line
    // high, keyed.
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
            MedWhiteFont->WriteString(&clip, at - left, 0, Text, -1);
        }
    };

    if (!wide)
    {
        writeClipped(x, x + TextWidth - 1, x + textX);
        return;
    }

    // Scrolled: the line shows the picture from ScrollPos, and from its start again after its end.
    const int32_t windowRight = x + MaxWidth - 1;
    const int32_t pictureEnd = x + TextWidth - ScrollPos;
    writeClipped(x, std::min(windowRight, pictureEnd - 1), x - ScrollPos + textX);

    if (TextWidth < ScrollPos + MaxWidth)
    {
        writeClipped(pictureEnd, windowRight, pictureEnd + textX);
    }
}

auto MCTicker::SetString(std::string_view text) -> void
{
    // A text ends at a NUL, as the C string did.
    text = text.substr(0, text.find('\0'));

    if (text.empty())
    {
        Text.clear();
        TextWidth = 0;
        ScrollPos = 0;
        return;
    }

    if (Text == text)
    {
        return;
    }

    Text = text;
    const int32_t stringWidth = Font->Width(Text);
    TextWidth = stringWidth;

    // A scrolling text gets half a line of gap before it repeats.
    if (MaxWidth < stringWidth)
    {
        TextWidth = MaxWidth / 2 + stringWidth;
    }

    ScrollPos = 0;
    ScrollShown = false;
}

auto MCTicker::SetPos(int32_t xPos, int32_t yPos) -> void
{
    XPos = xPos;
    YPos = yPos;
}

auto MCTicker::SetBackPane(MCGuiPort* port) -> void
{
    BackPane = std::make_unique<MCLogPort>();
    BackPane->Init(port->Width(), port->Height());
    port->CopyTo(BackPane->Frame(), 0, 0, true);
}
