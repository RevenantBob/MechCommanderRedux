#include "stdafx.h"
#include "main/MCMissionGlobals.h"
#include "gui/MCGuiChatWindow.h"
#include "gui/MCGuiButton.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiSystem.h"
#include "iface/MCTacticalInterface.h"
#include "lib/MCFatal.h"
#include "linkup/MCFidpMessage.h"
#include "linkup/MCFidpPlayer.h"
#include "linkup/MCSessionManager.h"
#include "mission/MCMission.h"
#include "mission/MCScenario.h"
#include "network/MCMultiPlayer.h"
#include "terrain/MCTacticalMap.h"
#include "terrain/MCTerrain.h"
#include "vfx/MCVfxFunctions.h"

bool FirstReturn = false;

namespace
{
    /// <summary>The caret blink timer's id.</summary>
    constexpr int32_t BlinkTimer = 0;
    /// <summary>The <see cref="MCGuiEventType::Focus"/> data for a gained focus, and a lost one.</summary>
    constexpr int32_t FocusGained = 7;
    constexpr int32_t FocusLost = 8;
    /// <summary>The scan code of Alt.</summary>
    constexpr int16_t AltScanCode = 0x38;
    /// <summary>Where the text starts, past the team button.</summary>
    constexpr int32_t TextX = 0x14;

    /// <summary>Whether <paramref name="key"/> can be typed into a chat line ('%' is the chat formatter's code character).</summary>
    bool IsChatCharacter(uint8_t key)
    {
        return ((key > 0x1f && key < 0x7f) || (key > 0xbe && key < 0xfe)) && key != '%';
    }
}

auto ScenarioChatCallback(MCFidpMessage& message) -> void
{
    if (TacticalMap() != nullptr)
    {
        TacticalMap()->HandleChatMessage(message.FromID, message.MessageBuffer());
    }
}

// MCGuiChatInput

MCGuiChatInput::~MCGuiChatInput() = default;

auto MCGuiChatInput::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* newText) -> int32_t
{
    int32_t result = MCGuiObject::Init(xPos, yPos, width, height, nullptr);
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't init chatsend window");

    TeamButton = MCMakeGui<MCGuiToolButton>();
    result = TeamButton->Init(1, 1, 0xc, 0x1a, nullptr);
    TeamButton->Framed = false;
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't init team button for chatsend window");
    TeamButton->SetDownPicture("mfdsbg00.tga");
    TeamButton->SetUpPicture("mfdsbh00.tga");
    TeamButton->SetGrayPicture("mfdsbn00.tga");
    TeamButton->Pushed = false;
    AddChild(TeamButton.get());

    CursorX = 0x15;

    if (newText != nullptr)
    {
        Text = std::string_view(newText).substr(0, MaxLength);
    }

    InputFont = WhiteFont;
    BackgroundColor = 0x10;
    // Port: the original drew the line here, which also restarted the caret's blink (see Draw).
    CursorVisible = true;
    return 0;
}

auto MCGuiChatInput::Destroy() -> void
{
    TeamButton.reset();
    MCGuiObject::Destroy();
}

auto MCGuiChatInput::Draw() -> void
{
    DrawAndCheck(0);

    // Port: the caret, which the original drew into the picture in Display (a vertical line a text line high; in
    // the background colour while CursorVisible, so it blinks).
    const int32_t bottom = InputFont->Height() + 3 + CursorY;
    VfxLineDraw(DisplayPort->Frame(), CursorX, CursorY, CursorX, bottom, CursorVisible ? 0x10 : 0x1f);
    MCGuiObject::Draw();
}

auto MCGuiChatInput::DrawAndCheck(int32_t maxLines) -> bool
{
    // Port: drawing the whole line (maxLines 0) also set CursorVisible, restarting the caret's blink. The line is
    // drawn every frame now, so the places that redrew it after an edit set it themselves.
    const bool draw = maxLines == 0;

    if (draw)
    {
        VfxPaneWipe(DisplayPort->Frame(), BackgroundColor);
    }

    int32_t extraLines = 0;

    if (!Text.empty())
    {
        // The first line leaves room for the team button; the next ones break at spaces.
        std::string_view line = Text;
        int32_t lineY = 1;
        int32_t fits = InputFont->CharactersToWidth(line, Width() - 0x18, false);

        while (fits > 0 && std::cmp_less(fits, line.size()) && (draw || extraLines < maxLines))
        {
            if (draw)
            {
                InputFont->WriteString(DisplayPort->Frame(), TextX, lineY, line.substr(0, static_cast<size_t>(fits)));
            }

            line.remove_prefix(static_cast<size_t>(fits));
            fits = InputFont->CharactersToWidth(line, Width() - TextX, true);
            lineY += 3 + InputFont->Height();
            extraLines++;
        }

        if (draw)
        {
            InputFont->WriteString(DisplayPort->Frame(), TextX, lineY, line);
        }
    }

    return extraLines < maxLines;
}

auto MCGuiChatInput::HandleEvent(MCGuiEvent* event) -> void
{
    switch (event->Type)
    {
        case MCGuiEventType::LeftButtonDown:
        {
            if (Scenario() != nullptr && EventsToMissionResultsScreen == 0 && GameAsked == 0)
            {
                GuiSystem()->SetText(this);
            }
            break;
        }
        case MCGuiEventType::KeyUp:
        case MCGuiEventType::KeyDown:
        {
            // Alt combinations go to the game interface.
            if (event->AltKey != 0 || event->ScanCode == AltScanCode)
            {
                TacticalInterface()->HandleEvent(event);
            }
            break;
        }
        case MCGuiEventType::Character:
        {
            if (GuiSystem()->TextObject() != this || EventsToMissionResultsScreen != 0)
            {
                break;
            }

            const uint8_t key = event->Key;

            if (key == '\b')
            {
                if (!Text.empty())
                {
                    Text.pop_back();
                    SetCursorPos(static_cast<int32_t>(Text.size()));
                    // The original redrew the line here, which restarted the caret's blink.
                    CursorVisible = true;
                }
            }
            else if (key == '\r')
            {
                if (FirstReturn)
                {
                    FirstReturn = false;
                    break;
                }

                if (MultiPlayer() != nullptr && !Text.empty())
                {
                    auto* chatWindow = static_cast<MCGuiChatWindow*>(Parent);
                    int32_t color;

                    if (!TeamButton->Pushed)
                    {
                        MultiPlayer()->SendChat(0, Text.data());
                        color = 6;
                    }
                    else
                    {
                        MultiPlayer()->SendChat(MultiPlayer()->HomeTeamGroupID, Text.data());
                        color = 4;
                    }

                    chatWindow->ProcessChatString(MultiPlayer()->SessionManager->MyPlayer->Id, Text, color);
                }

                Text.clear();
                SetCursorPos(0);
                GuiSystem()->ReleaseText();
                CursorVisible = true;
            }
            else if (Text.size() < MaxLength && IsChatCharacter(key))
            {
                Text.push_back(static_cast<char>(key));

                if (DrawAndCheck(2))
                {
                    SetCursorPos(static_cast<int32_t>(Text.size()));
                    CursorVisible = true;
                }
                else
                {
                    // A third line: take it back.
                    Text.pop_back();
                }
            }
            break;
        }

        case MCGuiEventType::Timer:
        {
            if (event->Data == BlinkTimer)
            {
                CursorVisible = !CursorVisible;
            }
            break;
        }
        case MCGuiEventType::Focus:
        {
            if (event->Data == FocusGained)
            {
                GuiSystem()->AddTimer(this, BlinkTimer, static_cast<int32_t>(MCPort::CaretBlinkTime()), 0, 0, false);
            }
            else if (event->Data == FocusLost)
            {
                GuiSystem()->RemoveTimer(this, BlinkTimer);
                CursorVisible = true;
            }
            break;
        }
        default:
            break;
    }

    MCGuiObject::HandleEvent(event);
}

auto MCGuiChatInput::SetCursorPos(int32_t pos) -> void
{
    std::string_view line = std::string_view(Text).substr(0, static_cast<size_t>(pos));
    CursorX = TextX;
    CursorY = 0;

    if (!line.empty())
    {
        // The line the caret is on (the wrapping as DrawAndCheck's, but without word breaks).
        int32_t fits = InputFont->CharactersToWidth(line, Width() - 0x18, false);

        while (fits > 0 && std::cmp_less(fits, line.size()))
        {
            line.remove_prefix(static_cast<size_t>(fits));
            fits = InputFont->CharactersToWidth(line, Width() - TextX, false);
            CursorY += InputFont->Height() + 3;
        }

        CursorX = InputFont->Width(line) + TextX + 1;
    }
}

// MCGuiChatWindow

auto MCGuiChatWindow::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) -> int32_t
{
    int32_t result = MCGuiObject::Init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    VfxPaneWipe(Port()->Frame(), 0x10);
    ChatInput = MCMakeGui<MCGuiChatInput>();
    result = ChatInput->Init(0, height + 4, Width(), 0x1c, nullptr);

    if (result == 0)
    {
        AddChild(ChatInput.get());
        ChatInput->ShowGuiWindow(true);
        // The original drew the input line here, restarting its caret's blink.
        ChatInput->CursorVisible = true;
    }

    return result;
}

auto MCGuiChatWindow::Destroy() -> void
{
    ChatInput.reset();
    MCGuiObject::Destroy();
}

auto MCGuiChatWindow::HandleNetworkMessage(uint32_t fromID, const void* data) -> void
{
    const auto* bytes = static_cast<const char*>(data);
    ProcessChatString(fromID, bytes + 9, bytes[8] != '\0' ? 6 : 4);
}

auto MCGuiChatWindow::ProcessChatString(uint32_t playerId, std::string_view text, int32_t color) -> void
{
    std::string_view name = "?";
    int32_t playerNumber = 0;

    if (playerId != 0)
    {
        const auto* player = MultiPlayer()->SessionManager->GetPlayer(playerId);
        name = player->Name;
        playerNumber = player->PlayerNumber;
    }

    if (color == -1)
    {
        color = 6;
    }

    text = text.substr(0, text.find('\0'));
    std::string line =
        std::format("%fc{}{}: %fc{}{}", PlayerColor[static_cast<size_t>(playerNumber)], name, color, text);

    // The original scrolled its picture up by the new text's height, wiped the bottom and wrote the text there; the
    // line is kept and Draw shows the lines that way. Lines scrolled wholly off the top are dropped.
    MCSmuti& formatter = GuiSystem()->TextFormatter;
    const int32_t textHeight = formatter.Process(line, nullptr, Port()->Width(), 0);
    ChatLines.push_back(ChatLine{std::move(line), textHeight});
    int32_t below = 0;

    for (size_t i = ChatLines.size(); i > 0; i--)
    {
        below += ChatLines[i - 1].Height;

        if (below > Port()->Height())
        {
            ChatLines.erase(ChatLines.begin(), ChatLines.begin() + static_cast<std::ptrdiff_t>(i - 1));
            break;
        }
    }
}

auto MCGuiChatWindow::Draw() -> void
{
    // The picture was wiped to 0x10 at init, and every scroll wiped the rows it uncovered.
    VfxPaneWipe(Port()->Frame(), 0x10);
    MCSmuti& formatter = GuiSystem()->TextFormatter;
    // Each line lies above the ones after it; the newest ends a row above the bottom.
    int32_t lineY = Port()->Height() - 1;

    for (const ChatLine& chatLine : ChatLines)
    {
        lineY -= chatLine.Height;
    }

    for (const ChatLine& chatLine : ChatLines)
    {
        formatter.Process(chatLine.Text, Port(), 0, lineY);
        lineY += chatLine.Height;
    }

    MCGuiObject::Draw();
}
