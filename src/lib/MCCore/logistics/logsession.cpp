#include "stdafx.h"
#include "logistics/logsession.h"
#include "gui/afont.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "lib/MCPacketFile.h"
#include "linkup/dpplayer.h"
#include "linkup/fidpgroup.h"
#include "linkup/sessionmanager.h"
#include "logistics/logdlg.h"
#include "logistics/logmain.h"
#include "logistics/logscrn.h"
#include "logistics/ticker.h"
#include "main/logistics.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "platform/MCInput.h"
#include "sound/soundsys.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>Until when (<c>timeGetTime</c>) the session screen keeps pinging after it opens (0x00808684).</summary>
    uint32_t PingUntil = 0;

    /// <summary>Set while the session screen still pings (0x00808688).</summary>
    int32_t Pinging = 0;

    /// <summary>The number of team slots.</summary>
    constexpr int32_t TeamSlots = 3;

    /// <summary>The top of the unassigned list and the height of its rows.</summary>
    constexpr int32_t UnassignedTop = 0x162;
    constexpr int32_t UnassignedRow = 0x14;

    /// <summary>A literal for the functions that take a non-const name.</summary>
    char* Art(const char* name)
    {
        return const_cast<char*>(name);
    }

    /// <summary>The slot of <paramref name="playerId"/> on <paramref name="team"/>, or -1.</summary>
    int32_t FindSlot(const uint32_t* team, uint32_t playerId)
    {
        for (int32_t slot = 0; slot < TeamSlots; ++slot)
        {
            if (team[slot] == playerId)
            {
                return slot;
            }
        }

        return -1;
    }

    /// <summary>A copy of <paramref name="text"/> in a logistics block.</summary>
    char* HeapCopy(const char* text)
    {
        size_t size = std::strlen(text) + 1;
        auto* copy = static_cast<char*>(GlobalLogPtr->LogisticsBlocks->Allocate(static_cast<uint32_t>(size)));
        std::memcpy(copy, text, size);
        return copy;
    }

    /// <summary>
    /// Sends a guaranteed message carrying a file name (<paramref name="type"/> 0x1027 load mission, 0x1028 start) to
    /// every player.
    /// </summary>
    void SendFileName(uint16_t type, const char* fileName)
    {
        size_t length = std::strlen(fileName) + 1;
        auto* message = static_cast<MCMPFileNameMessage*>(
            GlobalLogPtr->LogisticsBlocks->Allocate(static_cast<uint32_t>(length + 0xc)));
        message->Tagger.Clear();
        message->Header = type;
        std::memcpy(message->FileName, fileName, length);
        MPlayer->SessionManager->SendMessageToGroup(0, message,
                                                    static_cast<uint32_t>(std::strlen(message->FileName) + 1 + 0xc));
        GlobalLogPtr->LogisticsBlocks->Free(message);
    }

    /// <summary>Sends a two-long guaranteed message to every player.</summary>
    void SendTwoLongs(uint16_t type, int32_t value1, int32_t value2)
    {
        MCMPTwoLongMessage message{};
        message.Header = type;
        message.Value1 = value1;
        message.Value2 = value2;
        MPlayer->SessionManager->SendMessageToGroup(0, &message, sizeof(message));
    }

    /// <summary>The file name part of <paramref name="path"/> without folder or extension (<c>_splitpath</c>'s fname).</summary>
    void SplitFileName(const char* path, char* fileName, size_t size)
    {
        const char* start = path;

        for (const char* scan = path; *scan != 0; ++scan)
        {
            if (*scan == '\\' || *scan == '/' || *scan == ':')
            {
                start = scan + 1;
            }
        }

        const char* end = std::strrchr(start, '.');
        size_t length = end != nullptr ? static_cast<size_t>(end - start) : std::strlen(start);

        if (length >= size)
        {
            length = size - 1;
        }

        std::memcpy(fileName, start, length);
        fileName[length] = 0;
    }

    /// <summary>
    /// Sets up the message dialog's single button: <paramref name="callback"/> on it, the given pictures, enabled; no
    /// result callback.
    /// </summary>
    void SetDialogButton(void (*callback)(), const char* upArt, const char* downArt)
    {
        MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
        dialog->OkButton->Callback()->SetExec(callback);
        dialog->OkButton->SetUpPicture(Art(upArt));
        dialog->OkButton->SetDownPicture(Art(downArt));
        dialog->OkButton->Disabled = 0;
    }
}

// Event routines

void LToolButtonEventHandler(MCGuiObject* object, MCGuiEvent* event)
{
    auto* button = static_cast<MCLogToolButton*>(object);

    if (event->Type == 1)
    {
        if (button->Disabled == 0)
        {
            button->Toggled = button->Toggled == 0 ? 1 : 0;
            button->Callback()->Execute();
        }
    }
    else if (event->Type == 4)
    {
        if (Application->GrabbedObject() != nullptr)
        {
            Application->Release();
        }
    }
}

void ChatTeamButtonEventHandler(MCGuiObject* object, MCGuiEvent* event)
{
    LToolButtonEventHandler(object, event);
    object->Parent->HandleEvent(event);
}

void LScreenSwitchEventHandler(MCGuiObject* object, MCGuiEvent* event)
{
    auto* button = static_cast<MCLogToolButton*>(object);

    if (event->Type == 1 && button->Disabled == 0 && button->Toggled == 0)
    {
        button->Toggled = -1;
        button->Callback()->Execute();
    }
}

// lToolButton

auto MCLogToolButton::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    Toggled = 0;
    int32_t result = MCLogButton::Init(xPos, yPos, width, height, name);

    if (result == 0)
    {
        SetEventRoutine(LToolButtonEventHandler);
    }

    return result;
}

auto MCLogToolButton::HandleEvent(MCGuiEvent* event) -> void
{
    if (event->Type == 1)
    {
        // Original behaviour: the press sound is the fixed 0x33 unless an event routine is set.
        uint32_t sample = EventRoutine == nullptr ? 0x33 : PressSound;
        SoundSystem->PlayDigitalSample(sample, 1, nullptr, 0, 0);
    }
    else if (event->Type != 4)
    {
        MCLogButton::HandleEvent(event);
        return;
    }

    MCGuiObject::HandleEvent(event);
}

auto MCLogToolButton::Draw() -> void
{
    MCLogPort* picture;

    if (Disabled != 0)
    {
        picture = GrayPicture;
    }
    else if (Toggled != 0)
    {
        picture = DownPicture;
    }
    else if (OverState != 0)
    {
        picture = OverPicture;
    }
    else
    {
        // The mouse can be over the button without overState (it was disabled when the mouse came).
        MCPoint cursor = MCInput::GetCursorPos();
        int32_t cursorX = cursor.x - GlobalX();
        int32_t cursorY = cursor.y - GlobalY();
        // OB-129 (fixed): the original tested only the right and bottom edges, so the cursor anywhere above or left
        // of the button counted as over it.
        const bool over = cursorX >= 0 && cursorY >= 0 && cursorX <= Width() && cursorY <= Height();
        picture = over ? OverPicture : UpPicture;
    }

    DrawFace(picture, false);
}

// lSpinnerButton

auto MCLogSpinnerButton::HandleEvent(MCGuiEvent* event) -> void
{
    switch (event->Type)
    {
        case 1:
        {
            uint32_t sample = 0x33;

            if (Disabled == 0)
            {
                // Run the callback now, then repeat it after half a second held.
                Toggled = -1;
                Application->Grab(this);
                Application->AddTimer(this, 1, 500, 0, 0, 0);
                ButtonCallback->Execute();
                sample = PressSound;
            }

            SoundSystem->PlayDigitalSample(sample, 1, nullptr, 0, 0);
            break;
        }

        case 4:
        {
            Toggled = 0;
            Application->Release();
            Application->RemoveTimer(this, 1);
            Application->RemoveTimer(this, 2);
            break;
        }
        case 0x13:
        {
            if (event->Data == 1)
            {
                Application->RemoveTimer(this, 1);
                Application->AddTimer(this, 2, 100, 0, 0, 0);
            }

            if (event->Data == 2)
            {
                ButtonCallback->Execute();
            }
            break;
        }
        default:
            break;
    }

    MCGuiObject::HandleEvent(event);
}

auto MCLogSpinnerButton::Draw() -> void
{
    // Original behaviour (OB-130): the gray picture is never shown, so a disabled spinner looks enabled.
    MCLogPort* picture = Toggled != 0 ? DownPicture : UpPicture;

    if (picture != nullptr)
    {
        picture->CopyTo(_OwnPort->Frame(), 0, 0, 0);
    }

    MCLogObject::Draw();
}

// lChatInput

auto MCLogChatInput::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* newText) -> int32_t
{
    int32_t result = MCLogObject::Init(xPos, yPos, width, height, nullptr, nullptr);
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't init chatsend window");
    auto* button = new MCLogToolButton;
    TeamButton = button;
    result = button->Init(1, 1, 0xc, 0x1a, nullptr);
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't init team button for chatsend window");
    result = button->SetOverPicture(Art("lsbdw06.tga"));
    // The later picture loads' results are dropped: every check repeats the first one's.
    const int loadOk = result == 0;
    Assert(loadOk, static_cast<uint32_t>(result), " Couldn't load upstate team button for chatsend window");
    button->SetDownPicture(Art("lsbdw07.tga"));
    Assert(loadOk, static_cast<uint32_t>(result), " Couldn't load downstate team button for chatsend window");
    button->SetUpPicture(Art("lsbdw05.tga"));
    Assert(loadOk, static_cast<uint32_t>(result), " Couldn't load graystate team button for chatsend window");
    button->Toggled = 0;
    button->SetEventRoutine(ChatTeamButtonEventHandler);
    AddChild(button);
    CursorX = 0x15;

    if (newText != nullptr)
    {
        std::strncpy(Text, newText, 0xff);
    }

    Font = WhiteFont;
    BackgroundColor = 0x10;
    RestartBlink();
    return 0;
}

auto MCLogChatInput::Destroy() -> void
{
    if (TeamButton != nullptr)
    {
        delete TeamButton;
        TeamButton = nullptr;
    }

    MCLogObject::Destroy();
}

auto MCLogChatInput::Draw() -> void
{
    VfxPaneWipe(_OwnPort->Frame(), BackgroundColor);
    auto* line = reinterpret_cast<uint8_t*>(Text);
    int32_t lineY = 1;

    if (Text[0] != 0)
    {
        // The first line leaves room for the team button.
        int32_t lineLength = static_cast<int32_t>(std::strlen(Text));
        int32_t fits = Font->CharactersToWidth(line, Width() - 0x18, 0);

        while (fits > 0 && fits < lineLength)
        {
            uint8_t* next = line + fits;
            const uint8_t saved = *next;
            *next = 0;
            Font->WriteString(_OwnPort->Frame(), 0x14, lineY, line, -1);
            *next = saved;
            lineLength = static_cast<int32_t>(std::strlen(reinterpret_cast<char*>(next)));
            fits = Font->CharactersToWidth(next, Width() - 0x14, -1);
            lineY += 3 + Font->Height();
            line = next;
        }

        Font->WriteString(_OwnPort->Frame(), 0x14, lineY, line, -1);
    }

    // The caret, which the original's display drew into the picture each frame: a vertical line a text line high.
    const int32_t bottom = Font->Height() + 3 + CursorY;
    const int32_t color = CursorOn != 0 ? 0x10 : 0x1f;
    VfxLineDraw(_OwnPort->Frame(), CursorX, CursorY, CursorX, bottom, LD_DRAW, color);
    MCLogObject::Draw();
}

auto MCLogChatInput::RestartBlink() -> void
{
    CursorOn = -1;
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
            Application->SetText(this);
            break;
        case 10:
        {
            if (Application->TextObject() != this)
            {
                break;
            }

            const uint8_t key = event->Key;

            if (key == 8)
            {
                if (TextLength != 0)
                {
                    Text[TextLength - 1] = 0;
                    TextLength--;
                    SetCursorPos(TextLength);
                    RestartBlink();
                }
            }
            else if (key == 0xd)
            {
                if (MPlayer != nullptr && TextLength != 0)
                {
                    auto* chatWindow = static_cast<MCLogChatWindow*>(Parent);
                    int32_t color;

                    if (TeamButton->Toggled == 0)
                    {
                        MPlayer->SendChat(0, Text);
                        color = 6;
                    }
                    else
                    {
                        MPlayer->SendChat(MPlayer->HomeTeamGroupID, Text);
                        color = 4;
                    }

                    chatWindow->ProcessChatString(MPlayer->SessionManager->MyPlayer->Id, Text, color);
                }

                std::memset(Text, 0, 0xff);
                TextLength = 0;
                SetCursorPos(0);
                RestartBlink();
            }
            else if (TextLength < 0xff && ((key > 0x1f && key < 0x7f) || (key > 0xbe && key < 0xfe)) && key != '%')
            {
                // '%' is the chat formatter's code character.
                Text[TextLength] = static_cast<char>(key);
                TextLength++;
                SetCursorPos(TextLength);
                RestartBlink();
            }
            break;
        }

        case 0x13:
        {
            // The caret blink timer.
            if (event->Data == 0)
            {
                CursorOn = CursorOn == 0 ? 1 : 0;
            }
            break;
        }
        case 0x1e:
        {
            if (event->Data == 7)
            {
                Application->AddTimer(this, 0, MCPort::CaretBlinkTime(), 0, 0, 0);
            }
            else if (event->Data == 8)
            {
                Application->RemoveTimer(this, 0);
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
    // Measure the text up to position.
    char saved = 0;

    if (position < TextLength)
    {
        saved = Text[position];
        Text[position] = 0;
    }

    auto* line = reinterpret_cast<uint8_t*>(Text);
    CursorX = 0x14;
    CursorY = 0;

    if (Text[0] != 0)
    {
        int32_t lineLength = static_cast<int32_t>(std::strlen(Text));
        int32_t fits = Font->CharactersToWidth(line, Width() - 0x18, 0);

        while (fits > 0 && fits < lineLength)
        {
            line += fits;
            lineLength = static_cast<int32_t>(std::strlen(reinterpret_cast<char*>(line)));
            fits = Font->CharactersToWidth(line, Width() - 0x14, 0);
            CursorY += Font->Height() + 3;
        }

        CursorX = Font->Width(line) + 0x15;
    }

    if (saved != 0)
    {
        Text[position] = saved;
    }
}

// PlayerNameObject

auto MCPlayerNameObject::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    if (PlayerName != nullptr)
    {
        GlobalLogPtr->LogisticsBlocks->Free(PlayerName);
        PlayerName = nullptr;
    }

    int32_t result = MCLogObject::Init(xPos, yPos, width, height, name, nullptr);
    SetFont(LgWhiteFont);
    BackgroundColor = 0xff;
    return result;
}

auto MCPlayerNameObject::Destroy() -> void
{
    if (PlayerName != nullptr)
    {
        GlobalLogPtr->LogisticsBlocks->Free(PlayerName);
        PlayerName = nullptr;
    }

    MCLogObject::Destroy();
}

auto MCPlayerNameObject::Draw() -> void
{
    if (!_OwnPort->ViewOpen())
    {
        return;
    }

    if (NumberArt != nullptr)
    {
        VfxPaneWipe(_OwnPort->Frame(), static_cast<uint32_t>(NumberBack));
        NumberArt->CopyTo(_OwnPort->Frame(), 1, 1, 0);
    }

    const auto color = static_cast<uint8_t>(BackgroundColor);
    const auto bottom = static_cast<int16_t>(Height() - 1);
    FillBox(0x14, 1, static_cast<int16_t>(Width() - 1), bottom, color);

    // The name is left out while it is dragged.
    if (Font != nullptr && Application->GrabbedObject() != this)
    {
        Font->WriteString(_OwnPort->Frame(), 0x1b, 2, reinterpret_cast<uint8_t*>(PlayerName), -1);
    }
}

auto MCPlayerNameObject::HandleEvent(MCGuiEvent* event) -> void
{
    switch (event->Type)
    {
        case 1:
        {
            if (Draggable != 0)
            {
                int32_t grabX = event->X - X();

                if (grabX > 0x14)
                {
                    grabX = 0x14;
                }

                Application->Grab(this);
                StartDrag(grabX, event->Y - Y());
            }
            break;
        }
        case 4:
        {
            if (Application->GrabbedObject() == this)
            {
                Application->Release();
                ShowGuiWindow(0);
                // The original looks up the object under the drop and drops the result.
                ScreenWindow->FindObject(event->X, event->Y);
                ShowGuiWindow(-1);
                // Tell the session screen where the name was dropped.
                MCGuiEvent dropped;
                dropped.Clear();
                dropped.Type = 0x1d;
                dropped.Target = this;
                Parent->HandleEvent(&dropped);
            }
            break;
        }
        case 7:
        {
            if (Application->GrabbedObject() == this)
            {
                MoveTo(event->X - DragStartX(), event->Y - DragStartY(), 0);
            }
            break;
        }
        default:
            break;
    }

    MCGuiObject::HandleEvent(event);
}

auto MCPlayerNameObject::SetPlayerName(char* name) -> void
{
    if (PlayerName != nullptr)
    {
        GlobalLogPtr->LogisticsBlocks->Free(PlayerName);
    }

    size_t size = std::strlen(name) + 1;
    PlayerName = static_cast<char*>(GlobalLogPtr->LogisticsBlocks->Allocate(static_cast<uint32_t>(size)));

    if (PlayerName != nullptr)
    {
        std::memcpy(PlayerName, name, size);
    }
}

auto MCPlayerNameObject::SetPlayerId(uint32_t newPlayerId) -> void
{
    PlayerId = newPlayerId;

    if (MPlayer != nullptr && MPlayer->SessionManager != nullptr &&
        MPlayer->SessionManager->GetPlayer(newPlayerId) != nullptr)
    {
        SetPlayerName(MPlayer->SessionManager->GetPlayer(newPlayerId)->Name);
    }
}

auto MCPlayerNameObject::SetFont(MCGuiFont* newFont) -> void
{
    Font = newFont;
}

// Callbacks

void MPCancelCallback()
{
    GlobalLogPtr->MessageDialog->Callback = nullptr;
    GlobalLogPtr->SessionScreen->CancelMission();
}

void MPLoadWorkedCallback(int32_t)
{
    // The saved multiplayer mission: back to the session screen, and every player loads it.
    const char* fileName = GlobalLogPtr->SessionScreen->MissionFile;
    size_t length = std::strlen(fileName) + 1;
    auto* message =
        static_cast<MCMPFileNameMessage*>(GlobalLogPtr->LogisticsBlocks->Allocate(static_cast<uint32_t>(length + 0xc)));
    MCSplashScreen* loadScreen = GlobalLogPtr->LoadScreen;
    loadScreen->CancelButton->Callback()->SetExec(Cancel);
    loadScreen->LoadSaveButton->Callback()->SetExec(LoadGame);
    loadScreen->FilePane->SetMultiplayer(0);
    GlobalLogPtr->CurrentScreen->ShowGuiWindow(0);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->SessionScreen;
    GlobalLogPtr->LogisticsState = 8;
    GlobalLogPtr->ShowLogScreen(-1, -1);
    message->Tagger.Clear();
    message->Header = FIMSG_GUARANTEED | MPMSG_LOAD_MISSION;
    std::memcpy(message->FileName, fileName, length);
    MPlayer->SessionManager->SendMessageToGroup(0, message,
                                                static_cast<uint32_t>(std::strlen(message->FileName) + 1 + 0xc));
    GlobalLogPtr->LogisticsBlocks->Free(message);
}

void LoadMissionCallback()
{
    MCSplashScreen* loadScreen = GlobalLogPtr->LoadScreen;
    loadScreen->LoadSaveButton->Callback()->SetExec(LoadMPGame);
    loadScreen->CancelButton->Callback()->SetExec(CancelToSession);
    loadScreen->FilePane->SetMultiplayer(-1);
    GlobalLogPtr->CurrentScreen->ShowGuiWindow(0);
    GlobalLogPtr->CurrentScreen = loadScreen;
    GlobalLogPtr->LogisticsState = 5;
    GlobalLogPtr->ShowLogScreen(-1, 0);
}

void StartMissionCallback()
{
    char missionName[256];
    SplitFileName(GlobalLogPtr->SessionScreen->MissionFile, missionName, sizeof(missionName));
    SendFileName(FIMSG_GUARANTEED | MPMSG_START, missionName);
    Application->RemoveTimer(GlobalLogPtr->SessionScreen, 0);
    MPlayer->SessionManager->SendLatencyInfo();
    SoundSystem->PlayBettySample(0x19);
    GlobalLogPtr->InitializeMultiplayer();
    GlobalLogPtr->LoadCampaign(missionName, Art(".MPK"), 0, 0);
    GlobalLogPtr->SetUpBriefingScreen(0);
}

void TechTabRoutine(MCGuiObject* object, MCGuiEvent* event)
{
    if (event->Type != 1)
    {
        return;
    }

    auto* button = static_cast<MCLogToolButton*>(object);
    int32_t techBase = button->Value;
    int32_t team = button->Group;
    GlobalLogPtr->SessionScreen->SetTeamTechBase(static_cast<char>(team), static_cast<char>(techBase));

    if (MPlayer->IsHost != 0)
    {
        SendTwoLongs(FIMSG_GUARANTEED | MPMSG_TECHBASE_CHANGE, team, techBase);
    }
}

void IncrementTeam1RP()
{
    MCSessionScreen* screen = GlobalLogPtr->SessionScreen;
    screen->SetTeam1RP(std::atol(screen->Team1RPText->Buffer) + 1000);
}

void IncrementTeam2RP()
{
    MCSessionScreen* screen = GlobalLogPtr->SessionScreen;
    screen->SetTeam2RP(std::atol(screen->Team2RPText->Buffer) + 1000);
}

void DecrementTeam1RP()
{
    MCSessionScreen* screen = GlobalLogPtr->SessionScreen;
    int32_t points = std::atol(screen->Team1RPText->Buffer) - 1000;

    if (points < 0)
    {
        points = 0;
    }

    screen->SetTeam1RP(points);
}

void DecrementTeam2RP()
{
    MCSessionScreen* screen = GlobalLogPtr->SessionScreen;
    int32_t points = std::atol(screen->Team2RPText->Buffer) - 1000;

    if (points < 0)
    {
        points = 0;
    }

    screen->SetTeam2RP(points);
}

void SessionScreenDrawRoutine(MCGuiObject* object)
{
    auto* screen = static_cast<MCLogObject*>(object);
    screen->FillBox(0x1f, 0x162, 0xc6, 0x1d3, 0x10);
    int16_t top = 0x163;

    for (int32_t row = 0; row < GlobalLogPtr->SessionScreen->NumUnassigned; ++row)
    {
        screen->FillBox(0x1f, top, 0xc6, static_cast<int16_t>(top + 0xd), 0x12);
        top += 0x14;
    }
}

int CompareLong(const void* first, const void* second)
{
    const uint32_t a = *static_cast<const uint32_t*>(first);
    const uint32_t b = *static_cast<const uint32_t*>(second);

    if (b == a)
    {
        return 0;
    }

    return b < a ? 1 : -1;
}

// MPPlayerLights (its destructor was emitted here)

MCMPPlayerLights::~MCMPPlayerLights()
{
    Destroy();
}

// SessionScreen

auto MCSessionScreen::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    int32_t result = MCLogObject::Init(xPos, yPos, width, height, name, nullptr);
    SetPaintRoutine(SessionScreenDrawRoutine);
    Assert(result == 0, static_cast<uint32_t>(result), " Error initing session screen ");

    // The screen tabs.
    auto* toolButton = new MCLogToolButton;
    SessionButton = toolButton;

    if (toolButton != nullptr)
    {
        result = toolButton->Init(2, 0x10, 0xcf, 0x12, nullptr);
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing session button on session screen ");
        result = toolButton->SetUpPicture(Art("bn_session.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing session button art on session screen ");
        result = toolButton->SetDownPicture(Art("bg_session.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing session button art on session screen ");
        result = toolButton->SetOverPicture(Art("bh_session.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing session button art on session screen ");
        toolButton->Toggled = -1;
        toolButton->SetEventRoutine(LScreenSwitchEventHandler);
        AddChild(toolButton);
    }

    toolButton = new MCLogToolButton;
    ExitButton = toolButton;

    if (toolButton != nullptr)
    {
        result = toolButton->Init(2, 0x22, 0xcf, 0x12, nullptr);
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing session button on session screen ");
        result = toolButton->SetUpPicture(Art("bn_exit.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing session button art on session screen ");
        result = toolButton->SetDownPicture(Art("bg_exit.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing session button art on session screen ");
        result = toolButton->SetOverPicture(Art("bh_exit.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing session button art on session screen ");
        toolButton->SetEventRoutine(LScreenSwitchEventHandler);
        toolButton->Callback()->SetExec(LaunchedFromLobby == 0 ? Cancel : CancelToMPlayer);
        AddChild(toolButton);
    }

    // The host's load and start buttons.
    auto* button = new MCLogButton;
    LoadMissionButton = button;

    if (button != nullptr)
    {
        result = button->Init(0x107, 0xd1, 0x5e, 0x13, nullptr);
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button on session screen ");
        result = button->SetUpPicture(Art("ses_bh_map.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button art on session screen ");
        result = button->SetOverPicture(Art("ses_bh_map.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button art on session screen ");
        button->Callback()->SetExec(LoadMissionCallback);
        result = button->SetDownPicture(Art("ses_bg_map.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button art on session screen ");
        result = button->SetGrayPicture(Art("ses_bn_map.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button art on session screen ");
        button->SetTransparent(-1);
        AddChild(button);
    }

    button = new MCLogButton;
    StartButton = button;

    if (button != nullptr)
    {
        result = button->Init(0x1de, 0x1b4, 0x5e, 0x13, nullptr);
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button on session screen ");
        result = button->SetUpPicture(Art("ses_bh_begin.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button art on session screen ");
        result = button->SetOverPicture(Art("ses_bh_begin.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button art on session screen ");
        result = button->SetDownPicture(Art("ses_bg_begin.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button art on session screen ");
        button->Callback()->SetExec(StartMissionCallback);
        result = button->SetGrayPicture(Art("ses_bn_begin.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button art on session screen ");
        AddChild(button);
        StartButton->Disabled = -1;
        StartButton->SetTransparent(-1);
    }

    // The mission description.
    auto* description = new MCLogScrollTextObject;
    MissionText = description;

    if (description != nullptr)
    {
        result = description->Init(0x17f, 0x35, 0xdc, 0x82, nullptr);
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing mission description on session screen ");
        description->ScrollTab->ShowGuiWindow(0);
        description->Scrolling = 0;
        AddChild(description);
    }

    // Each team's resource points, a number the host can type.
    auto makeRPText = [this](int32_t textX)
    {
        auto* text = new MCLogTextObject;

        if (text != nullptr)
        {
            text->MCLogObject::Init(textX, 0x184, 0x42, 0xe, nullptr, nullptr);
            text->Font = LgBlackFont;
            text->BackgroundColor = 0x1f;
            text->CursorPos = 0;
            text->CursorPixel = 0;
            VfxPaneWipe(text->Lport()->Frame(), 0x1f);
            text->InitBuffer(8, -1);
            text->SetStringBuffer(Art("0"));
            text->SetBackColor(0x10);
            text->Font = LgWhiteFont;
            AddChild(text);
        }

        return text;
    };

    Team1RPText = makeRPText(0x10f);
    Team2RPText = makeRPText(0x1d1);

    auto makeSpinner = [this](int32_t spinX, int32_t spinY, const char* upArt, const char* downArt, const char* grayArt)
    {
        auto* spinner = new MCLogSpinnerButton;

        if (spinner != nullptr)
        {
            spinner->Init(spinX, spinY, 9, 7, nullptr);
            spinner->SetUpPicture(Art(upArt));
            spinner->SetDownPicture(Art(downArt));
            spinner->SetGrayPicture(Art(grayArt));
            spinner->SetEventRoutine(nullptr);
            spinner->PressSound = 0xf;
            AddChild(spinner);
        }

        return spinner;
    };

    Team1RPUp = makeSpinner(0x102, 0x183, "Ses_bh_rpup.tga", "Ses_bg_rpup.tga", "Ses_bn_rpup.tga");
    Team1RPDown = makeSpinner(0x102, 0x18b, "Ses_bh_rpdn.tga", "Ses_bg_rpdn.tga", "Ses_bn_rpdn.tga");
    Team2RPUp = makeSpinner(0x1c4, 0x183, "Ses_bh_rpup.tga", "Ses_bg_rpup.tga", "Ses_bn_rpup.tga");
    Team2RPDown = makeSpinner(0x1c4, 0x18b, "Ses_bh_rpdn.tga", "Ses_bg_rpdn.tga", "Ses_bn_rpdn.tga");

    // The tech base toggles (team 1: Inner Sphere, team 2: Clan).
    auto makeTechButton = [this](int32_t techX, int32_t team, int32_t techBase, int toggledOn)
    {
        auto* techButton = new MCLogToolButton;

        if (techButton != nullptr)
        {
            techButton->Init(techX, 0x10c, 0x27, 0xe, nullptr);
            const bool clan = techBase == -1;
            techButton->SetUpPicture(Art(clan ? "ses_bh_clan.tga" : "ses_bh_is.tga"));
            techButton->SetOverPicture(Art(clan ? "ses_bh_clan.tga" : "ses_bh_is.tga"));
            techButton->SetDownPicture(Art(clan ? "ses_bg_clan.tga" : "ses_bg_is.tga"));
            techButton->Group = team;
            techButton->Value = techBase;

            if (toggledOn != 0)
            {
                techButton->Toggled = -1;
            }

            techButton->SetTransparent(-1);
            techButton->PressSound = 0xf;
            AddChild(techButton);
        }

        return techButton;
    };

    Team1TechBase = 1;
    Team1ISButton = makeTechButton(0x153, 1, 1, -1);
    Team1ClanButton = makeTechButton(0x179, 1, -1, 0);
    Team2TechBase = -1;
    Team2ISButton = makeTechButton(0x215, 2, 1, 0);
    Team2ClanButton = makeTechButton(0x23b, 2, -1, -1);

    // Where names can be dropped: team 1's three slots, then team 2's.
    NumDropTargets = 6;
    DropTargets = new tagRECT[6];

    if (DropTargets != nullptr)
    {
        DropTargets[0] = {0xf7, 0x132, 0x199, 0x144};
        DropTargets[1] = {0xf7, 0x148, 0x199, 0x15a};
        DropTargets[2] = {0xf7, 0x15d, 0x199, 0x16f};
        DropTargets[3] = {0x1b9, 0x132, 0x25b, 0x144};
        DropTargets[4] = {0x1b9, 0x148, 0x25b, 0x15a};
        DropTargets[5] = {0x1b9, 0x15d, 0x25a, 0x16f};
    }

    // The six name slots, each with its player number picture.
    int32_t number = 0;
    MCPlayerNameObject** slot = PlayerNames;

    for (int32_t slotY = UnassignedTop; slotY < 0x1da; slotY += UnassignedRow, ++slot)
    {
        auto* nameObject = new MCPlayerNameObject;
        *slot = nameObject;
        nameObject->Init(0xb, UnassignedTop, 0xa0, 0x10, nullptr);
        AddChild(nameObject);
        nameObject->MoveTo(0xb, slotY, 0);
        nameObject->ShowGuiWindow(0);
        nameObject->SetTransparent(-1);
        ++number;
        // The original wiped the name's picture to the screen's background colour and pasted the number there; the
        // name draws them each frame.
        nameObject->NumberBack = BackgroundColor;
        nameObject->NumberArt = LogArtf("ses_p%i.tga", number);
    }

    SetBackground(Art("ses_bk00.tga"));
    ScreenWindow->AddChild(this);
    ShowGuiWindow(0);
    NumUnassigned = 0;
    return 0;
}

auto MCSessionScreen::Destroy() -> void
{
    auto release = [](auto*& object)
    {
        if (object != nullptr)
        {
            delete object;
            object = nullptr;
        }
    };

    auto destroyAndRelease = [](auto*& object)
    {
        if (object != nullptr)
        {
            object->Destroy();
            delete object;
            object = nullptr;
        }
    };

    release(SessionButton);
    release(ExitButton);
    release(LoadMissionButton);
    release(StartButton);
    release(MissionText);
    destroyAndRelease(Team1RPText);
    destroyAndRelease(Team2RPText);
    destroyAndRelease(Team1RPUp);
    destroyAndRelease(Team1RPDown);
    destroyAndRelease(Team2RPUp);
    destroyAndRelease(Team2RPDown);
    destroyAndRelease(Team1ISButton);
    destroyAndRelease(Team1ClanButton);
    destroyAndRelease(Team2ISButton);
    destroyAndRelease(Team2ClanButton);

    for (MCPlayerNameObject*& nameObject : PlayerNames)
    {
        release(nameObject);
    }

    if (DropTargets != nullptr)
    {
        delete[] DropTargets;
        DropTargets = nullptr;
    }

    // Original behaviour: the mission name isn't freed here (the logistics heap goes with the logistics object).
    if (MissionFile != nullptr)
    {
        GlobalLogPtr->LogisticsBlocks->Free(MissionFile);
        MissionFile = nullptr;
    }

    if (MapName != nullptr)
    {
        GlobalLogPtr->LogisticsBlocks->Free(MapName);
        MapName = nullptr;
    }

    ClearMap();
    MCLogObject::Destroy();
}

auto MCSessionScreen::Draw() -> void
{
    if (MPlayer == nullptr || MPlayer->SessionManager->CurrentSession == nullptr)
    {
        return;
    }

    MCLogObject::Draw();
    MCLogPort* port = _OwnPort;
    DrawMap(port->Frame());
    MedWhiteFont->WriteString(port->Frame(), 0x180, 199, reinterpret_cast<uint8_t*>(MissionName), -1);
    MedWhiteFont->WriteString(port->Frame(), 0x180, 0xe3, reinterpret_cast<uint8_t*>(MapName), -1);
    char noLabel[256];
    CLoadString(ThisInstance, 0x37f, noLabel, 0xfe);
    char* label = MissionLabel[0] != 0 ? MissionLabel : noLabel;
    MedWhiteFont->WriteString(port->Frame(), 0x20e, 199, reinterpret_cast<uint8_t*>(label), -1);

    // Each team's resource points per player.
    if (MPlayer->ClanGroupID == 0)
    {
        GlobalLogPtr->DrawScreenChrome(this, port->Frame());
        return;
    }

    int32_t players = 1;
    MCFidpGroup* group = MPlayer->SessionManager->GetGroup(MPlayer->InnerSphereGroupID);
    int32_t divisor = players;

    if (group != nullptr)
    {
        divisor = group->Players.Count;

        if (divisor == 0)
        {
            divisor = players = 1;
        }
    }

    char text[12];
    std::snprintf(text, sizeof(text), "%d", static_cast<int32_t>(std::atol(Team1RPText->Buffer) / divisor));
    LgWhiteFont->WriteString(port->Frame(), 0x157, 0x185, reinterpret_cast<uint8_t*>(text), -1);
    group = MPlayer->SessionManager->GetGroup(MPlayer->ClanGroupID);

    if (group != nullptr)
    {
        divisor = group->Players.Count;
        players = divisor;
    }

    if (divisor == 0)
    {
        divisor = players = 1;
    }

    std::snprintf(text, sizeof(text), "%d", static_cast<int32_t>(std::atol(Team2RPText->Buffer) / divisor));
    LgWhiteFont->WriteString(port->Frame(), 0x219, 0x185, reinterpret_cast<uint8_t*>(text), -1);
    GlobalLogPtr->DrawScreenChrome(this, port->Frame());
}

auto MCSessionScreen::HandleEvent(MCGuiEvent* event) -> void
{
    if (event->Type == 0x13)
    {
        if (event->Data == 0)
        {
            if (MPlayer == nullptr)
            {
                Application->RemoveTimer(this, 0);
            }
            else
            {
                // Ping for the first three seconds so the latencies are known.
                if (Pinging != 0)
                {
                    if (MCPort::Milliseconds() < PingUntil)
                    {
                        if (MPlayer->SessionManager->IsHost == 0)
                        {
                            MPlayer->SessionManager->SendPing();
                        }
                    }
                    else
                    {
                        Pinging = 0;
                    }
                }

                // Tell the others when the host changed a team's points.
                int32_t points = std::atol(Team1RPText->Buffer);

                if (Team1RP != points)
                {
                    Team1RP = std::atol(Team1RPText->Buffer);

                    if (MPlayer->IsHost != 0)
                    {
                        SendTwoLongs(FIMSG_GUARANTEED | MPMSG_RP_UPDATE, Team1RP, 1);
                    }
                }

                points = std::atol(Team2RPText->Buffer);

                if (Team2RP != points)
                {
                    Team2RP = std::atol(Team2RPText->Buffer);

                    if (MPlayer->IsHost != 0)
                    {
                        SendTwoLongs(FIMSG_GUARANTEED | MPMSG_RP_UPDATE, Team2RP, 2);
                    }
                }
            }
        }
    }
    else if (event->Type == 0x1d)
    {
        // A name was dropped: onto a team slot, or anywhere else (no team).
        auto* nameObject = static_cast<MCPlayerNameObject*>(event->Target);
        uint32_t id = nameObject->PlayerId;
        RECT area;
        area.left = nameObject->GlobalX();
        area.top = nameObject->GlobalY();
        area.right = area.left + 0x14;
        area.bottom = nameObject->Height() + area.top;
        RECT overlap;
        static constexpr char slotTeam[6] = {1, 1, 1, 2, 2, 2};
        static constexpr char slotIndex[6] = {0, 1, 2, 0, 1, 2};
        bool placed = false;

        for (int32_t target = 0; target < 6; ++target)
        {
            if (IntersectRect(&overlap, &area, &DropTargets[target]) != 0)
            {
                AssignPlayer(id, slotTeam[target], slotIndex[target], 0);
                placed = true;
                break;
            }
        }

        if (!placed)
        {
            AssignPlayer(id, 0, 0, 0);
        }
    }

    MCGuiObject::HandleEvent(event);
}

auto MCSessionScreen::Activate(int refresh) -> void
{
    const int32_t isHost = MPlayer->IsHost;

    if (MPlayer->SessionManager->GetPlayers(nullptr)->Count < 2)
    {
        Cancel();
        return;
    }

    PingUntil = MCPort::Milliseconds() + 3000;
    Pinging = -1;
    SessionButton->Disabled = 0;
    ExitButton->Toggled = 0;
    ExitButton->Disabled = 0;
    StartButton->Disabled = -1;

    if (LaunchedFromLobby != 0)
    {
        ExitButton->Callback()->SetExec(CancelToMPlayer);
    }

    CancelMission();
    Team2RP = 0;
    Team1RP = 0;
    int32_t slotY = UnassignedTop;

    for (MCPlayerNameObject* nameObject : PlayerNames)
    {
        nameObject->MoveTo(0xb, slotY, 0);
        nameObject->ShowGuiWindow(0);
        slotY += UnassignedRow;
    }

    for (int32_t slot = 0; slot < TeamSlots; ++slot)
    {
        Team1Players[slot] = 0xffffffff;
        Team2Players[slot] = 0xffffffff;
    }

    NumUnassigned = 0;
    NumPlayers = 0;

    if (isHost != 0 && refresh == 0)
    {
        SendTwoLongs(FIMSG_GUARANTEED | MPMSG_SWITCH_SCREEN, 1, 0);
    }

    Team1ISButton->Toggled = -1;
    Team1ClanButton->Toggled = 0;
    Team2ISButton->Toggled = 0;
    Team1TechBase = 1;
    Team2ClanButton->Toggled = -1;
    Team2TechBase = -1;

    // The players' ids in ascending order (the empty slots, 0xffffffff, last).
    MCFLinkedList<MCFidpPlayer>* players = MPlayer->SessionManager->GetPlayers(nullptr);
    NumPlayers = players->Count;
    uint32_t ids[6];
    int32_t count = 0;
    MCFLink<MCFidpPlayer>* link = players->HeadLink;
    MCFidpPlayer* player = link != nullptr ? link->Data : nullptr;

    while (player != nullptr)
    {
        ids[count++] = player->Id;
        Assert(link != nullptr, 0);
        link = link->Next;

        if (link == nullptr)
        {
            break;
        }

        player = link->Data;
    }

    for (int32_t index = count; index < 6; ++index)
    {
        ids[index] = 0xffffffff;
    }

    std::qsort(ids, 6, sizeof(uint32_t), CompareLong);

    if (GlobalLogPtr->PlayerLights == nullptr)
    {
        auto* lights = new MCMPPlayerLights;

        if (lights != nullptr)
        {
            lights->Init();
        }

        GlobalLogPtr->PlayerLights = lights;
    }

    if (GlobalLogPtr->PlayerLights->Parent != nullptr)
    {
        GlobalLogPtr->PlayerLights->Parent->RemoveChild(GlobalLogPtr->PlayerLights);
    }

    AddChild(GlobalLogPtr->PlayerLights);
    GlobalLogPtr->PlayerLights->SetNumPlayers(NumPlayers);
    int32_t index = 0;

    do
    {
        MCPlayerNameObject* nameObject = PlayerNames[index];
        nameObject->SetPlayerId(ids[index]);
        nameObject->ShowGuiWindow(-1);
        nameObject->Draggable = 0;
        AssignPlayer(ids[index], 0, 0, -1);
        GlobalLogPtr->PlayerLights->SetPlayerID(index, ids[index]);
        GlobalLogPtr->PlayerLights->SetPlayerStatus(ids[index], refresh == 0 ? 0 : 2);
        ++index;
    } while (index < 6 && ids[index] != 0xffffffff);

    if (isHost == 0 || refresh != 0)
    {
        ControlsOff();

        if (refresh != 0)
        {
            // Back from a mission: wait for everyone to check in again.
            char text[1024];
            CLoadString(ThisInstance, 0xb8, text, 0xfe);
            MissionText->Clear();
            MissionText->FontIndex = 2;
            MissionText->Print(text, 0x1f);
            MCFIGuaranteedMessageHeader checkIn{};
            checkIn.Header = FIMSG_GUARANTEED | MPMSG_SESSION_CHECK_IN;
            MPlayer->SessionManager->SendMessageToGroup(0, &checkIn, sizeof(checkIn));
            MPlayer->PlayerSessionCheckIn[MPlayer->SessionManager->MyPlayer->PlayerNumber] = -1;
            SomeoneCheckedIn();
        }
    }
    else
    {
        ControlsOn();
    }

    MissionName = nullptr;
    MapName = nullptr;
    std::memset(MissionLabel, 0, sizeof(MissionLabel));
    MCLogChatWindow* chatWindow = GlobalLogPtr->ChatWindow;

    if (chatWindow != nullptr)
    {
        chatWindow->MoveTo(2, 0x42, 0);
        chatWindow->Resize(0x10a);

        if (chatWindow->Parent != nullptr)
        {
            chatWindow->Parent->RemoveChild(chatWindow);
        }

        AddChild(chatWindow);
        chatWindow->ShowGuiWindow(-1);
    }

    MCTicker* ticker = GlobalLogPtr->Ticker;

    if (ticker != nullptr)
    {
        if (ticker->Parent != nullptr)
        {
            ticker->Parent->RemoveChild(ticker);
        }

        AddChild(ticker);
        ticker->SetPort(Lport());
        // The ticker paints into this screen now (what it shows is kept in the screen's chrome).
        ticker->SetScreen(this);
        ticker->SetPos(3, 3);
    }

    Application->AddTimer(this, 0, 500, 0, 0, 0);
    MPlayer->ChatCallback = LogisticsChatCallback;
}

auto MCSessionScreen::AssignPlayer(uint32_t playerId, char team, char slot, int remote) -> void
{
    MCPlayerNameObject* nameObject = nullptr;

    for (MCPlayerNameObject* candidate : PlayerNames)
    {
        if (candidate->PlayerId == playerId)
        {
            nameObject = candidate;
            break;
        }
    }

    // Lays the names of the players on no team down the unassigned list, skipping skipId.
    auto layOutUnassigned = [this](uint32_t skipId, int32_t names, bool skipEmpty)
    {
        NumUnassigned = 0;

        for (int32_t index = 0; index < names; ++index)
        {
            uint32_t id = PlayerNames[index]->PlayerId;

            if (skipEmpty ? id == 0xffffffff : id == skipId)
            {
                continue;
            }

            if (FindSlot(Team1Players, id) >= 0 || FindSlot(Team2Players, id) >= 0)
            {
                continue;
            }

            PlayerNames[index]->MoveTo(0xb, NumUnassigned * UnassignedRow + UnassignedTop, 0);
            NumUnassigned++;
        }
    };

    if (team == 0)
    {
        if (remote != 0)
        {
            // Filling the screen: the player starts unassigned.
            NumUnassigned++;
            CheckGoodToGo();
            return;
        }

        int32_t index = FindSlot(Team1Players, playerId);
        uint32_t groupID = MPlayer->InnerSphereGroupID;
        uint32_t* players = Team1Players;

        if (index < 0)
        {
            index = FindSlot(Team2Players, playerId);
            groupID = MPlayer->ClanGroupID;
            players = Team2Players;
        }

        if (index >= 0)
        {
            nameObject->MoveTo(0xb, NumUnassigned * UnassignedRow + UnassignedTop, 0);
            players[index] = 0xffffffff;
            MPlayer->SessionManager->RemovePlayerFromGroup(groupID, playerId);
            NumUnassigned++;
        }
        else
        {
            layOutUnassigned(0xffffffff, 6, true);
        }

        GlobalLogPtr->PlayerLights->SetPlayerStatus(playerId, 0);
    }
    else if (team == 1 || team == 2)
    {
        uint32_t* teamPlayers = team == 1 ? Team1Players : Team2Players;

        if (teamPlayers[slot] != 0xffffffff)
        {
            AssignPlayer(teamPlayers[slot], 0, 0, 0);
        }

        int32_t index = FindSlot(Team1Players, playerId);

        if (index >= 0)
        {
            Team1Players[index] = 0xffffffff;
            MPlayer->SessionManager->RemovePlayerFromGroup(MPlayer->InnerSphereGroupID, playerId);
        }
        else if ((index = FindSlot(Team2Players, playerId)) >= 0)
        {
            Team2Players[index] = 0xffffffff;
            MPlayer->SessionManager->RemovePlayerFromGroup(MPlayer->ClanGroupID, playerId);
        }
        else
        {
            layOutUnassigned(playerId, NumPlayers, false);
        }

        teamPlayers[slot] = playerId;
        uint32_t groupID = team == 1 ? MPlayer->InnerSphereGroupID : MPlayer->ClanGroupID;
        MPlayer->SessionManager->AddPlayerToGroup(groupID, playerId);

        if (MPlayer->SessionManager->MyPlayer->Id == playerId)
        {
            MPlayer->HomeTeam = team == 1 ? 0 : 1;
            MPlayer->HomeTeamGroupID = groupID;
            MPlayer->EnemyTeamGroupID = team == 1 ? MPlayer->ClanGroupID : MPlayer->InnerSphereGroupID;
        }

        nameObject->MoveTo(team == 1 ? 0xf8 : 0x1ba, slot * 0x16 + 0x133, 0);
        GlobalLogPtr->PlayerLights->SetPlayerStatus(playerId, 1);

        if (remote != 0)
        {
            CheckGoodToGo();
            return;
        }
    }
    else if (remote != 0)
    {
        CheckGoodToGo();
        return;
    }

    if (MPlayer->IsHost != 0)
    {
        MCMPJoinTeamMessage message{};
        message.Header = FIMSG_GUARANTEED | MPMSG_JOIN_TEAM;
        message.PlayerID = playerId;
        message.Team = team;
        message.Slot = slot;
        MPlayer->SessionManager->SendMessageToGroup(0, &message, sizeof(message));
    }

    CheckGoodToGo();
}

auto MCSessionScreen::SetTeam1RP(int32_t resourcePoints) -> void
{
    char text[16];
    std::snprintf(text, sizeof(text), "%d", resourcePoints);
    Team1RPText->SetStringBuffer(text);
}

auto MCSessionScreen::SetTeam2RP(int32_t resourcePoints) -> void
{
    char text[16];
    std::snprintf(text, sizeof(text), "%d", resourcePoints);
    Team2RPText->SetStringBuffer(text);
}

auto MCSessionScreen::SetMap(char* fileName) -> void
{
    if (fileName == nullptr)
    {
        // No mission: clear the map box (the original wiped it in the background picture).
        ClearMap();
        MapBoxWiped = true;
        return;
    }

    ClearMap();

    auto* picture = new MCLogPort;
    std::string path;
    path = GamePath(TerrainPath, fileName, ".log.tga");
    int32_t result = picture->Init(path.data());

    if (result != 0)
    {
        Fatal(result, " Unable to create Port for TacMap ");
    }

    // The original stretched the map picture over the box in the background picture; the screen keeps the picture
    // and draws it so each frame.
    MapPicture = picture;
}

auto MCSessionScreen::DrawMap(MCPane* target) -> void
{
    if (MapBoxWiped)
    {
        MCPane box = *target;
        box.X0 = 0xf4;
        box.Y0 = 0x33;
        box.X1 = 0x177;
        box.Y1 = 0xb6;
        VfxPaneWipe(&box, 0x10);
    }

    if (MapPicture == nullptr)
    {
        return;
    }

    // Stretch the map picture over the box.
    const int32_t maxU = MapPicture->Frame()->Window->XMax;
    const int32_t maxV = MapPicture->Frame()->Window->YMax;
    MCScreenVertex corners[4] = {
        {0xf4, 0x33, 0, 0, 0, 0},
        {0x177, 0x33, 0, maxU << 16, 0, 0},
        {0x177, 0xb6, 0, maxU << 16, maxV << 16, 0},
        {0xf4, 0xb6, 0, 0, maxV << 16, 0},
    };

    VfxMapPolygon(target, 4, corners, MapPicture->Frame()->Window, MP_XP);
}

auto MCSessionScreen::ClearMap() -> void
{
    if (MapPicture != nullptr)
    {
        MapPicture->Destroy();
        delete MapPicture;
        MapPicture = nullptr;
    }
}

auto MCSessionScreen::SetMissionName(char* name) -> void
{
    if (MissionName != nullptr)
    {
        GlobalLogPtr->LogisticsBlocks->Free(MissionName);
        MissionName = nullptr;
    }

    if (name != nullptr)
    {
        MissionName = HeapCopy(name);
    }
}

auto MCSessionScreen::SetMapName(char* name) -> void
{
    if (MapName != nullptr)
    {
        GlobalLogPtr->LogisticsBlocks->Free(MapName);
        MapName = nullptr;
    }

    if (name != nullptr)
    {
        MapName = HeapCopy(name);
    }
}

auto MCSessionScreen::SomeoneCheckedIn() -> void
{
    int32_t allIn = -1;

    for (int32_t index = 0; index < 6; ++index)
    {
        uint32_t id = PlayerNames[index]->PlayerId;

        if (id == 0xffffffff)
        {
            continue;
        }

        // Original behaviour: the check-in flags are read by name slot, not by player number.
        if (MPlayer->PlayerSessionCheckIn[index] == 0)
        {
            allIn = 0;
        }
        else
        {
            GlobalLogPtr->PlayerLights->SetPlayerStatus(id, 0);
        }
    }

    if (allIn != 0)
    {
        MissionText->Clear();
        MissionText->FontIndex = 0;

        if (MPlayer->IsHost != 0)
        {
            ControlsOn();
        }
    }
}

auto MCSessionScreen::FileReport(uint32_t playerId, int haveFile) -> void
{
    // Nothing to do unless an inquiry is out.
    int32_t index = 0;

    while (PlayerNames[index]->FileStatus == -1)
    {
        if (++index > 5)
        {
            return;
        }
    }

    for (MCPlayerNameObject* nameObject : PlayerNames)
    {
        if (nameObject->PlayerId == playerId)
        {
            nameObject->FileStatus = haveFile == 0 ? 1 : 2;
            break;
        }
    }

    int32_t allHave = -1;

    for (MCPlayerNameObject* nameObject : PlayerNames)
    {
        if (nameObject->FileStatus == 0)
        {
            return;
        }

        if (nameObject->FileStatus == 1)
        {
            allHave = 0;
        }
    }

    MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;

    if (allHave != 0)
    {
        // Everyone has it: the waiting dialog closes itself.
        Application->AddTimer(dialog, 0, 1000, 0, 0, 0);
        dialog->OkButton->Callback()->SetExec(nullptr);
        CheckGoodToGo();
        return;
    }

    // Name the players missing the file.
    char text[1024];
    CLoadString(ThisInstance, 0xab, text, 0xfe);

    for (MCPlayerNameObject* nameObject : PlayerNames)
    {
        if (nameObject->FileStatus == 1)
        {
            std::strcat(text, nameObject->PlayerName);
        }

        nameObject->FileStatus = -1;
    }

    dialog->SetText(text);
    dialog->SetTwoButton(0);
    SetDialogButton(MPCancelCallback, "bh_okay.tga", "bg_okay.tga");
    dialog = GlobalLogPtr->MessageDialog;
    dialog->Callback = nullptr;
    dialog->Activate();
}

auto MCSessionScreen::LoadMission(char* fileName) -> void
{
    MCPacketFile packetFile;
    MCFitIniFile iniFile;
    MCFile textFile;
    std::string path;

    if (MPlayer->IsHost == 0)
    {
        path = GamePath(fileName, "", "");
    }
    else
    {
        path = GamePath(SavePath, fileName, ".mpk");
    }

    char text[256];
    uint32_t value = 0;

    int32_t result = packetFile.Open(path);
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't open .MPK file ");

    if (result == 0)
    {
        result = packetFile.SeekPacket(0);
    }

    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't find packet 0 in .MPK file ");

    if (result == 0)
    {
        result = iniFile.Open(&packetFile, packetFile.GetPacketSize());
        Assert(result == 0, static_cast<uint32_t>(result), " Couldn't open packet 0 in .MPK file ");

        if (result == 0)
        {
            result = iniFile.SeekBlock("Multiplayer");
        }
    }

    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't find Multiplayer block in file ");

    if (result == 0)
    {
        result = iniFile.ReadIdString("LongMissionName", text, 0xfe);
    }

    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't read mission name in Multiplayer file ");

    if (result == 0)
    {
        SetMissionName(text);
        result = iniFile.ReadIdString("LongMapName", text, 0xfe);
    }

    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't read map name in Multiplayer file ");

    if (result == 0)
    {
        SetMapName(text);
        result = iniFile.ReadIdString("MapFileName", text, 0xfe);
    }

    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't read map file name in Multiplayer file ");

    if (result == 0)
    {
        SetMap(text);

        if (iniFile.ReadIdULong("MissionTime", value) == 0)
        {
            std::snprintf(text, sizeof(text), "%01d:%02d", value / 60, value % 60);
            // Port fix: the original strcpy'd the text into the 10-byte label unchecked.
            MCStrCopy(GlobalLogPtr->SessionScreen->MissionLabel, text);
        }

        result = iniFile.SeekBlock("ResourcePoints");
    }

    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't find ResourcePoints block in MPK file ");

    if (result == 0)
    {
        int32_t team1Missing = iniFile.ReadIdULong("Team1Points", value);

        if (team1Missing == 0)
        {
            SetTeam1RP(static_cast<int32_t>(value));
        }

        int32_t team2Missing = iniFile.ReadIdULong("Team2Points", value);

        if (team2Missing == 0)
        {
            SetTeam2RP(static_cast<int32_t>(value));
        }

        if (team1Missing != 0 || team2Missing != 0)
        {
            // An older file: one figure for both teams.
            if (iniFile.ReadIdULong("numPoints", value) != 0)
            {
                value = 0;
            }

            if (team1Missing != 0)
            {
                SetTeam1RP(static_cast<int32_t>(value));
            }

            if (team2Missing != 0)
            {
                SetTeam2RP(static_cast<int32_t>(value));
            }
        }
    }

    if (MPlayer->IsHost != 0)
    {
        LockControls(iniFile.SeekBlock("Lock") == 0);
    }

    iniFile.Close();

    // Packet 1: the mission description.
    result = packetFile.SeekPacket(1);
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't find packet 1 in .MPK file ");

    if (result == 0)
    {
        result = textFile.Open(&packetFile, packetFile.GetPacketSize());
        Assert(result == 0, static_cast<uint32_t>(result), " Couldn't open packet 1 in .MPK file ");

        if (result == 0)
        {
            MissionText->Clear();
            MissionText->FontIndex = 0;

            while (textFile.Eof() == 0)
            {
                textFile.ReadLine(reinterpret_cast<uint8_t*>(text), 0xfe);
                MissionText->Print(text, 0x1f);
            }

            Assert(textFile.Eof(), 0, "Error reading MP mission description");

            if (MPlayer->IsHost != 0)
            {
                // Ask everyone whether they have the file, and wait for the answers.
                if (MissionFile != nullptr)
                {
                    GlobalLogPtr->LogisticsBlocks->Free(MissionFile);
                }

                MissionFile = HeapCopy(path.c_str());
                MPlayer->SendFileInquiry(path.data());
                // The server has it already.
                uint32_t serverId = MPlayer->SessionManager->ServerID;

                for (MCPlayerNameObject* nameObject : PlayerNames)
                {
                    if (nameObject->PlayerId != 0xffffffff)
                    {
                        nameObject->FileStatus = 0;
                    }

                    if (nameObject->PlayerId == serverId)
                    {
                        nameObject->FileStatus = 2;
                    }
                }

                char message[256];
                CLoadString(ThisInstance, 0xac, message, 0xfe);
                MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
                dialog->SetText(message);
                dialog->SetTwoButton(0);
                dialog = GlobalLogPtr->MessageDialog;
                dialog->Callback = MPLoadWorkedCallback;
                SetDialogButton(MPCancelCallback, "bh_cancl.tga", "bg_cancl.tga");
                GlobalLogPtr->MessageDialog->Activate();
            }

            return;
        }
    }

    // The file couldn't be read.
    SetMissionName(nullptr);
    SetMapName(nullptr);
    SetMap(nullptr);
    std::memset(MissionLabel, 0, sizeof(MissionLabel));
    SetTeam1RP(0);
    SetTeam2RP(0);
    MissionText->Clear();
    char format[256];
    char message[1024];
    CLoadString(ThisInstance, 0xad, format, 0xfe);
    std::snprintf(message, sizeof(message), format, fileName);
    MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
    dialog->SetText(message);
    dialog->SetTwoButton(0);
    SetDialogButton(MPCancelCallback, "bh_okay.tga", "bg_okay.tga");
    dialog = GlobalLogPtr->MessageDialog;
    dialog->Callback = nullptr;
    dialog->Activate();
}

auto MCSessionScreen::CancelMission() -> void
{
    SetMissionName(nullptr);
    SetMapName(nullptr);
    SetMap(nullptr);

    if (MissionFile != nullptr)
    {
        GlobalLogPtr->LogisticsBlocks->Free(MissionFile);
    }

    MissionFile = nullptr;
    std::memset(MissionLabel, 0, sizeof(MissionLabel));
    SetTeam1RP(0);
    SetTeam2RP(0);
    MissionText->Clear();

    for (MCPlayerNameObject* nameObject : PlayerNames)
    {
        nameObject->FileStatus = -1;
    }
}

auto MCSessionScreen::FillDpidArray(uint32_t* ids, int32_t* count, int myTeam) -> void
{
    // Team 2 when the local player is on it and myTeam is set, or when it isn't and myTeam is clear.
    const bool onTeam2 = FindSlot(Team2Players, MPlayer->SessionManager->MyPlayer->Id) >= 0;
    *count = 0;
    const uint32_t* players = (myTeam != 0) == onTeam2 ? Team2Players : Team1Players;

    for (int32_t slot = 0; slot < TeamSlots; ++slot)
    {
        if (players[slot] != 0xffffffff)
        {
            ids[(*count)++] = players[slot];
        }
    }
}

auto MCSessionScreen::CheckGoodToGo() -> void
{
    int32_t goodToGo = 0;

    if (NumUnassigned == 0 && MissionFile != nullptr && MPlayer->PlayersOnHomeTeam()->Count != 0 &&
        MPlayer->PlayersOnEnemyTeam()->Count != 0)
    {
        int32_t team1Filled = 0;

        for (int32_t slot = 0; slot < TeamSlots; ++slot)
        {
            if (Team1Players[slot] != 0xffffffff)
            {
                team1Filled = -1;
            }

            if (Team2Players[slot] != 0xffffffff)
            {
                goodToGo = -1;
            }

            if (team1Filled != 0 && goodToGo != 0)
            {
                break;
            }
        }

        goodToGo = team1Filled != 0 && goodToGo != 0 ? 1 : 0;
    }

    StartButton->Disabled = goodToGo == 0 ? 1 : 0;
}

auto MCSessionScreen::RemovePlayer(uint32_t playerId) -> void
{
    NumPlayers--;

    if (NumPlayers < 2)
    {
        // Too few left: the session ends.
        char text[256];
        CLoadString(ThisInstance, LaunchedFromLobby == 0 ? 0xb0 : 0xba, text, 0xfe);
        MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
        dialog->SetText(text);
        dialog->SetTwoButton(0);
        GlobalLogPtr->MessageDialog->Callback = nullptr;
        SetDialogButton(LaunchedFromLobby == 0 ? Cancel : GameOverMan, "bh_okay.tga", "bg_okay.tga");
        GlobalLogPtr->MessageDialog->Activate();
        return;
    }

    for (MCPlayerNameObject* nameObject : PlayerNames)
    {
        if (nameObject->PlayerId == playerId)
        {
            nameObject->ShowGuiWindow(0);
            nameObject->FileStatus = -1;
            nameObject->SetPlayerId(0xffffffff);
            SomeoneCheckedIn();
            FileReport(0xffffffff, 0);
            GlobalLogPtr->PlayerLights->SetPlayerStatus(playerId, 0);
            break;
        }
    }

    int32_t index = FindSlot(Team1Players, playerId);

    if (index >= 0)
    {
        Team1Players[index] = 0xffffffff;
    }
    else if ((index = FindSlot(Team2Players, playerId)) >= 0)
    {
        Team2Players[index] = 0xffffffff;
    }
    else
    {
        // An unassigned player: close up the list.
        NumUnassigned = 0;

        for (MCPlayerNameObject* nameObject : PlayerNames)
        {
            uint32_t id = nameObject->PlayerId;

            if (id == 0xffffffff || FindSlot(Team1Players, id) >= 0 || FindSlot(Team2Players, id) >= 0)
            {
                continue;
            }

            nameObject->MoveTo(0xb, NumUnassigned * UnassignedRow + UnassignedTop, 0);
            NumUnassigned++;
        }
    }

    CheckGoodToGo();
}

auto MCSessionScreen::SetTeamTechBase(char team, char techBase) -> void
{
    if (techBase != 1 && techBase != -1)
    {
        return;
    }

    MCLogToolButton* isButton;
    MCLogToolButton* clanButton;

    if (team == 1)
    {
        Team1TechBase = techBase;
        isButton = Team1ISButton;
        clanButton = Team1ClanButton;
    }
    else if (team == 2)
    {
        Team2TechBase = techBase;
        isButton = Team2ISButton;
        clanButton = Team2ClanButton;
    }
    else
    {
        return;
    }

    // The chosen toggle on, the other off.
    MCLogToolButton* chosen = techBase == -1 ? clanButton : isButton;
    MCLogToolButton* other = techBase == -1 ? isButton : clanButton;
    chosen->Toggled = -1;
    other->Toggled = 0;
}

auto MCSessionScreen::ControlsOn() -> void
{
    Team1RPText->AllowedInput = MCLogTextObject::INPUT_DIGITS;
    Team2RPText->AllowedInput = MCLogTextObject::INPUT_DIGITS;
    Team1RPUp->Callback()->SetExec(IncrementTeam1RP);
    Team1RPDown->Callback()->SetExec(DecrementTeam1RP);
    Team2RPUp->Callback()->SetExec(IncrementTeam2RP);
    Team2RPDown->Callback()->SetExec(DecrementTeam2RP);

    for (MCLogSpinnerButton* spinner : {Team1RPUp, Team1RPDown, Team2RPUp, Team2RPDown})
    {
        spinner->Disabled = 0;
    }

    for (MCLogToolButton* techButton : {Team1ISButton, Team1ClanButton, Team2ISButton, Team2ClanButton})
    {
        techButton->SetEventRoutine(TechTabRoutine);
    }

    for (MCPlayerNameObject* nameObject : PlayerNames)
    {
        nameObject->Draggable = -1;
    }

    LoadMissionButton->Disabled = 0;
}

auto MCSessionScreen::ControlsOff() -> void
{
    Team1RPText->AllowedInput = MCLogTextObject::INPUT_NONE;
    Team2RPText->AllowedInput = MCLogTextObject::INPUT_NONE;

    for (MCLogSpinnerButton* spinner : {Team1RPUp, Team1RPDown, Team2RPUp, Team2RPDown})
    {
        spinner->Callback()->SetExec(nullptr);
    }

    for (MCLogSpinnerButton* spinner : {Team1RPUp, Team1RPDown, Team2RPUp, Team2RPDown})
    {
        spinner->Disabled = -1;
    }

    for (MCLogToolButton* techButton : {Team1ISButton, Team1ClanButton, Team2ISButton, Team2ClanButton})
    {
        techButton->SetEventRoutine(nullptr);
    }

    for (MCPlayerNameObject* nameObject : PlayerNames)
    {
        nameObject->Draggable = 0;
    }

    LoadMissionButton->Disabled = -1;
}

auto MCSessionScreen::LockControls(int lock) -> void
{
    if (lock != 0)
    {
        // A locked mission: fixed points and tech bases (team 1 Inner Sphere, team 2 Clan).
        Team1RPText->AllowedInput = MCLogTextObject::INPUT_NONE;
        Team2RPText->AllowedInput = MCLogTextObject::INPUT_NONE;

        for (MCLogSpinnerButton* spinner : {Team1RPUp, Team1RPDown, Team2RPUp, Team2RPDown})
        {
            spinner->Callback()->SetExec(nullptr);
        }

        for (MCLogSpinnerButton* spinner : {Team1RPUp, Team1RPDown, Team2RPUp, Team2RPDown})
        {
            spinner->Disabled = -1;
        }

        for (MCLogToolButton* techButton : {Team1ISButton, Team1ClanButton, Team2ISButton, Team2ClanButton})
        {
            techButton->SetEventRoutine(nullptr);
        }

        SetTeamTechBase(1, 1);
        SetTeamTechBase(2, -1);
        SendTwoLongs(FIMSG_GUARANTEED | MPMSG_TECHBASE_CHANGE, 1, 1);
        SendTwoLongs(FIMSG_GUARANTEED | MPMSG_TECHBASE_CHANGE, 2, -1);
        return;
    }

    Team1RPText->AllowedInput = MCLogTextObject::INPUT_DIGITS;
    Team2RPText->AllowedInput = MCLogTextObject::INPUT_DIGITS;
    Team1RPUp->Callback()->SetExec(IncrementTeam1RP);
    Team1RPDown->Callback()->SetExec(DecrementTeam1RP);
    Team2RPUp->Callback()->SetExec(IncrementTeam2RP);
    Team2RPDown->Callback()->SetExec(DecrementTeam2RP);

    for (MCLogSpinnerButton* spinner : {Team1RPUp, Team1RPDown, Team2RPUp, Team2RPDown})
    {
        spinner->Disabled = 0;
    }

    for (MCLogToolButton* techButton : {Team1ISButton, Team1ClanButton, Team2ISButton, Team2ClanButton})
    {
        techButton->SetEventRoutine(TechTabRoutine);
    }
}
