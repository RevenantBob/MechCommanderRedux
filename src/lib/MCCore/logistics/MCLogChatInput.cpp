#include "stdafx.h"
#include "logistics/MCLogChatInput.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiGlobals.h"
#include "lib/MCFatal.h"
#include "linkup/dpplayer.h"
#include "linkup/sessionmanager.h"
#include "logistics/MCLogChatWindow.h"
#include "logistics/MCLogToolButton.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>The caret blink timer.</summary>
    constexpr int32_t BlinkTimer = 0;
    /// <summary>Where the text starts (right of the team button).</summary>
    constexpr int32_t TextX = 0x14;
}

MCLogChatInput::~MCLogChatInput()
{
    MCLogChatInput::Destroy();
}

auto MCLogChatInput::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* newText) -> int32_t
{
    int32_t result = MCLogObject::Init(xPos, yPos, width, height);
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't init chatsend window");
    TeamButton = MCMakeGui<MCLogToolButton>();
    result = TeamButton->Init(1, 1, 0xc, 0x1a, nullptr);
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't init team button for chatsend window");
    // A picture that can't be loaded is fatal.
    TeamButton->SetOverPicture("lsbdw06.tga");
    TeamButton->SetDownPicture("lsbdw07.tga");
    TeamButton->SetUpPicture("lsbdw05.tga");
    TeamButton->Toggled = false;
    TeamButton->SetEventRoutine(ChatTeamButtonEventHandler);
    AddChild(TeamButton.get());
    CursorX = 0x15;

    if (newText != nullptr)
    {
        // Original behaviour: the starting text isn't counted in TextLength (nothing passes one).
        std::string_view start = std::string_view(newText).substr(0, MaxLength);
        std::ranges::copy(start, Text.begin());
    }

    Font = WhiteFont;
    BackgroundColor = 0x10;
    RestartBlink();
    return 0;
}

auto MCLogChatInput::Destroy() -> void
{
    TeamButton.reset();
    MCLogObject::Destroy();
}

auto MCLogChatInput::ShownText() const -> std::string_view
{
    return std::string_view(Text.c_str());
}

auto MCLogChatInput::HideText() -> void
{
    Text[0] = 0;
}

auto MCLogChatInput::Draw() -> void
{
    VfxPaneWipe(_Port->Frame(), BackgroundColor);
    std::string_view line = ShownText();
    int32_t lineY = 1;

    if (!line.empty())
    {
        // The first line leaves room for the team button; the next ones break at spaces.
        int32_t fits = Font->CharactersToWidth(line, Width() - 0x18, false);

        while (fits > 0 && std::cmp_less(fits, line.size()))
        {
            Font->WriteString(_Port->Frame(), TextX, lineY, line.substr(0, static_cast<size_t>(fits)));
            line.remove_prefix(static_cast<size_t>(fits));
            fits = Font->CharactersToWidth(line, Width() - TextX, true);
            lineY += 3 + Font->Height();
        }

        Font->WriteString(_Port->Frame(), TextX, lineY, line);
    }

    // The caret, which the original's display drew into the picture each frame: a vertical line a text line high.
    const int32_t bottom = Font->Height() + 3 + CursorY;
    VfxLineDraw(_Port->Frame(), CursorX, CursorY, CursorX, bottom, CursorOn ? 0x10 : 0x1f);
    MCLogObject::Draw();
}

auto MCLogChatInput::RestartBlink() -> void
{
    CursorOn = true;
}

auto MCLogChatInput::Display() -> void
{
    MCLogObject::Display();
}

auto MCLogChatInput::HandleEvent(MCGuiEvent* event) -> void
{
    switch (event->Type)
    {
        case 1:
            GuiSystem()->SetText(this);
            break;
        case 10:
        {
            if (GuiSystem()->TextObject() != this)
            {
                break;
            }

            const uint8_t key = event->Key;

            if (key == '\b')
            {
                if (TextLength != 0)
                {
                    Text[--TextLength] = 0;
                    SetCursorPos(TextLength);
                    RestartBlink();
                }
            }
            else if (key == '\r')
            {
                if (MPlayer != nullptr && TextLength != 0)
                {
                    auto* chatWindow = static_cast<MCLogChatWindow*>(Parent);
                    int32_t color;

                    if (!TeamButton->Toggled)
                    {
                        MPlayer->SendChat(0, Text.data());
                        color = 6;
                    }
                    else
                    {
                        MPlayer->SendChat(MPlayer->HomeTeamGroupID, Text.data());
                        color = 4;
                    }

                    chatWindow->ProcessChatString(MPlayer->SessionManager->MyPlayer->Id, Text.data(), color);
                }

                std::ranges::fill(Text, '\0');
                TextLength = 0;
                SetCursorPos(0);
                RestartBlink();
            }
            else if (TextLength < MaxLength && ((key > 0x1f && key < 0x7f) || (key > 0xbe && key < 0xfe)) && key != '%')
            {
                // '%' is the chat formatter's code character.
                Text[TextLength++] = static_cast<char>(key);
                SetCursorPos(TextLength);
                RestartBlink();
            }
            break;
        }

        case 0x13:
        {
            if (event->Data == BlinkTimer)
            {
                CursorOn = !CursorOn;
            }
            break;
        }
        case 0x1e:
        {
            if (event->Data == 7)
            {
                GuiSystem()->AddTimer(this, BlinkTimer, MCPort::CaretBlinkTime(), 0, 0, 0);
            }
            else if (event->Data == 8)
            {
                GuiSystem()->RemoveTimer(this, BlinkTimer);
            }
            break;
        }
        default:
            break;
    }

    MCGuiObject::HandleEvent(event);
}

auto MCLogChatInput::SetCursorPos(int32_t position) -> void
{
    // The text up to position, as far as it is shown.
    std::string_view line = ShownText().substr(0, static_cast<size_t>(position));
    CursorX = TextX;
    CursorY = 0;

    if (!line.empty())
    {
        int32_t fits = Font->CharactersToWidth(line, Width() - 0x18, false);

        while (fits > 0 && std::cmp_less(fits, line.size()))
        {
            line.remove_prefix(static_cast<size_t>(fits));
            fits = Font->CharactersToWidth(line, Width() - TextX, false);
            CursorY += Font->Height() + 3;
        }

        CursorX = Font->Width(line) + TextX + 1;
    }
}
