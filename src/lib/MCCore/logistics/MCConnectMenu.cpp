#include "stdafx.h"
#include "logistics/MCConnectMenu.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiSystem.h"
#include "linkup/dpplayer.h"
#include "linkup/linkedlist.hpp"
#include "linkup/session.h"
#include "linkup/sessionmanager.h"
#include "logistics/MCFileScrollPane.h"
#include "logistics/MCGameList.h"
#include "logistics/MCGenericScreen.h"
#include "logistics/MCLoadSaveMenu.h"
#include "logistics/MCLogDialogButton.h"
#include "logistics/MCLogMenus.h"
#include "logistics/MCLogScrollTextObject.h"
#include "logistics/MCLogTextObject.h"
#include "logistics/MCMainMenu.h"
#include "logistics/MCRegistrySettings.h"
#include "logistics/MCReusableDialog.h"
#include "logistics/MCSessionScreen.h"
#include "logistics/MCSplashScreen.h"
#include "main/logistics.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "lib/MCFatal.h"
#include "main/honorb.h"

int32_t ReadyRoomTicks = 0;
bool WhackTimer = false;

namespace
{
    /// <summary>The fewest and the most players a hosted LAN session takes (a game rule).</summary>
    constexpr int32_t MinSessionPlayers = 2;
    constexpr int32_t MaxSessionPlayers = 6;

    /// <summary>DirectPlay's DPERR_CONNECTING: the modem is still dialling.</summary>
    constexpr int32_t DialStillConnecting = static_cast<int32_t>(0x8877015e);

    /// <summary>The player's name for the multiplayer screens: the one remembered (or the user's), if any.</summary>
    std::optional<std::string> RememberedUserName()
    {
        std::array<char, 0x40> name = {};
        uint32_t length = 0x3f;

        if (MyGetUserName(name.data(), &length) == 0)
        {
            return std::nullopt;
        }

        return std::string(name.data());
    }

    /// <summary>The name for the player entry: the one remembered, else "Player".</summary>
    std::string PlayerNameOrDefault()
    {
        return RememberedUserName().value_or("Player");
    }

    /// <summary>The players a hosted LAN session takes: the number typed in the LAN screen's entry 11, within the rule.</summary>
    int32_t TypedMaxPlayers()
    {
        return std::clamp(ElementNumber(GlobalLogPtr->LanScreen, 11), MinSessionPlayers, MaxSessionPlayers);
    }

    /// <summary>Opens the ready room (session screen) after a session was joined or created from <paramref name="from"/>.</summary>
    void EnterReadyRoom(MCLogObject* from, std::optional<bool> goDisabled)
    {
        from->ShowGuiWindow(false);
        GlobalLogPtr->ConnectScreen->ShowGuiWindow(true);
        GlobalLogPtr->CurrentScreen = GlobalLogPtr->ConnectScreen;
        GlobalLogPtr->LogisticsState = 0xe;

        if (goDisabled.has_value())
        {
            GlobalLogPtr->ConnectScreen->Element<MCLogButton>(2)->Disabled = *goDisabled;
        }
    }

    /// <summary>After a session was joined: how many are in it.</summary>
    void CountLanPlayers()
    {
        NumLanPlayers = MPlayer->SessionManager->GetPlayers(nullptr)->Count;
    }

    /// <summary>Shows the LAN session list (after connecting), the player entry focused.</summary>
    void OpenLanScreen()
    {
        GlobalLogPtr->MultiplayerScreen->ShowGuiWindow(false);
        GlobalLogPtr->LanScreen->ShowGuiWindow(true);
        GlobalLogPtr->LanScreen->ShowBlock(0);
        GlobalLogPtr->CurrentScreen = GlobalLogPtr->LanScreen;
        GlobalLogPtr->LogisticsState = 0xb;
        GuiSystem()->SetText(GlobalLogPtr->LanScreen->Elements[4]);
    }

    /// <summary>
    /// The one-button message dialog with string <paramref name="stringId"/> whose button (showing cancel) runs
    /// <paramref name="cancelExec"/>, while <paramref name="screen"/> retries in a second (its timer
    /// <paramref name="timer"/>).
    /// </summary>
    void WaitWithCancel(MCGenericScreen* screen, int32_t timer, uint32_t stringId, void (*cancelExec)(), bool enable)
    {
        GuiSystem()->AddTimer(screen, timer, 1000, 0, 0, 0);
        WhackTimer = false;
        MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
        dialog->SetText(LoadGameString(stringId, 0xfe));
        dialog->SetTwoButton(false);
        dialog->Callback = nullptr;
        dialog->OkButton->SetUpPicture("bh_cancl.tga");
        dialog->OkButton->SetDownPicture("bg_cancl.tga");

        if (enable)
        {
            dialog->OkButton->Disabled = false;
        }

        dialog->OkButton->Callback()->SetExec(cancelExec);
        dialog->Activate();
    }
}

void ConnectScreen()
{
    EnsureRegistryVersion();
    MCGenericScreen* screen = GlobalLogPtr->MultiplayerScreen;

    if (MPlayer == nullptr)
    {
        // The multiplayer object is network/'s (P3-net gives it an owner).
        MPlayer = new MCMultiPlayer;
        MPlayer->Init(0x7d000, 0x100, 100);
    }

    GlobalLogPtr->MainScreen->ShowGuiWindow(false);
    GlobalLogPtr->MultiplayerScreen->ShowGuiWindow(true);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->MultiplayerScreen;
    GlobalLogPtr->LogisticsState = 10;

    // The connection buttons: modem, serial, LAN, internet; each is enabled when the machine has it.
    auto* modemButton = screen->Element<MCLogButton>(2);
    auto* serialButton = screen->Element<MCLogButton>(3);
    auto* lanButton = screen->Element<MCLogButton>(4);
    auto* internetButton = screen->Element<MCLogButton>(5);
    modemButton->Disabled = true;
    serialButton->Disabled = true;
    lanButton->Disabled = true;
    internetButton->Disabled = true;

    if (MPlayer->SessionManager != nullptr)
    {
        MCSessionManager* manager = MPlayer->SessionManager;
        modemButton->Disabled = manager->IsModemAvailable() == 0;
        serialButton->Disabled = (manager->AvailableProtocols & 4) == 0;
        lanButton->Disabled = manager->IsIpxAvailable() == 0 && manager->IsTcpAvailable() == 0;
        internetButton->Disabled = (manager->AvailableProtocols & 0x10) == 0;
    }

    // Less than 32 MB: multiplayer may not run well.
    if (MCPort::TotalPhysicalMemory() < 32000000)
    {
        ShowMenuMessage(0x371);
    }
}

void CancelToConnect()
{
    WhackTimer = true;
    GlobalLogPtr->CurrentScreen->ShowGuiWindow(false);
    GlobalLogPtr->MultiplayerScreen->ShowGuiWindow(true);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->MultiplayerScreen;
    GlobalLogPtr->LogisticsState = 10;
    GlobalLogPtr->LanScreen->ShowBlock(0);
    GlobalLogPtr->ModemScreen->ShowBlock(0);
    auto* games = GlobalLogPtr->LanScreen->Element<MCGameList>(2);
    games->ClearSelection();
    games->Clear();
    GlobalLogPtr->LanScreen->Element<MCLogScrollTextObject>(3)->Clear();
    GlobalLogPtr->LanScreen->Element<MCLogButton>(6)->Disabled = true;
    // Original behaviour (OB-167): the same timer is removed twice (team 2's is left).
    GuiSystem()->RemoveTimer(GlobalLogPtr->SessionScreen->Team1RPText.get(), 0);
    GuiSystem()->RemoveTimer(GlobalLogPtr->SessionScreen->Team1RPText.get(), 0);
    MPlayer->LeaveSession();
    GlobalLogPtr->PlayerLights.reset();
}

void CancelToMPlayer()
{
    KillTheGame();
}

void CancelToLan()
{
    GlobalLogPtr->CurrentScreen->ShowGuiWindow(false);
    GlobalLogPtr->LanScreen->ShowGuiWindow(true);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->LanScreen;
    GlobalLogPtr->LogisticsState = 0xb;
    GlobalLogPtr->LanScreen->ShowBlock(0);
    GlobalLogPtr->LanScreen->Element<MCGameList>(2)->ClearSelection();
}

void CancelToSession()
{
    MCSplashScreen* loadScreen = GlobalLogPtr->LoadScreen;
    loadScreen->CancelButton->Callback()->SetExec(Cancel);
    loadScreen->LoadSaveButton->Callback()->SetExec(LoadGame);
    loadScreen->FilePane->SetMultiplayer(false);
    GlobalLogPtr->CurrentScreen->ShowGuiWindow(false);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->SessionScreen;
    GlobalLogPtr->LogisticsState = 8;
    GlobalLogPtr->ShowLogScreen(1, 1);
}

void ShowModemScreen()
{
    MCSplashScreen* screen = GlobalLogPtr->ModemScreen;

    if (MPlayer != nullptr)
    {
        auto* modems = screen->Element<MCLogScrollTextObject>(10);
        screen->Element<MCLogTextObject>(4)->SetStringBuffer(PlayerNameOrDefault());
        MPlayer->SessionManager->FindModems();
        // The list is refilled with the modems found, keeping its selection.
        const int32_t selected = modems->HighlightLine[0];
        modems->Clear();
        modems->HighlightLine[0] = selected;

        for (int32_t i = 0;; i++)
        {
            const char* modem = MPlayer->SessionManager->GetModemName(i);

            if (modem == nullptr)
            {
                break;
            }

            modems->Print(modem, 0x1f);
        }
    }

    GlobalLogPtr->MultiplayerScreen->ShowGuiWindow(false);
    GlobalLogPtr->CurrentScreen = screen;
    screen->ShowGuiWindow(true);
    GlobalLogPtr->LogisticsState = 0xc;
    screen->ShowBlock(0);
}

void ShowSerialScreen()
{
    MCGenericScreen* screen = GlobalLogPtr->SerialScreen;
    screen->Element<MCLogTextObject>(4)->SetStringBuffer(PlayerNameOrDefault());
    GlobalLogPtr->MultiplayerScreen->ShowGuiWindow(false);
    GlobalLogPtr->CurrentScreen = screen;
    screen->ShowGuiWindow(true);
    GlobalLogPtr->LogisticsState = 0xd;
    GuiSystem()->SetText(screen->Elements[4]);
}

void DoTheIpxThang()
{
    if (MPlayer != nullptr && MPlayer->SessionManager != nullptr)
    {
        MPlayer->SessionManager->ConnectIpx();
    }

    OpenLanScreen();
}

void DoTheTcpThang()
{
    if (MPlayer != nullptr && MPlayer->SessionManager != nullptr)
    {
        MPlayer->SessionManager->ConnectTcp(const_cast<char*>(""));
    }

    OpenLanScreen();
}

void TcpipxDialogCallback(int32_t result)
{
    if (result == 1)
    {
        DoTheIpxThang();
        return;
    }

    if (result == 2)
    {
        DoTheTcpThang();
    }
}

void ShowLanScreen()
{
    MCSplashScreen* screen = GlobalLogPtr->LanScreen;
    auto* gameEntry = screen->Element<MCLogTextObject>(10);

    if (const std::optional<std::string> name = RememberedUserName())
    {
        screen->Element<MCLogTextObject>(4)->SetStringBuffer(*name);
        gameEntry->InitBuffer(0x18, MCLogInputType::Text);
        gameEntry->SetStringBuffer(MCFormatPrintf(LoadGameString(0x377, 0xfe).c_str(), name->c_str()));
    }
    else
    {
        screen->Element<MCLogTextObject>(4)->SetStringBuffer("Player");
        gameEntry->SetStringBuffer("Game");
    }

    // Both protocols: ask which; else say which one is used.
    MCSessionManager* manager = MPlayer->SessionManager;

    if (manager->IsIpxAvailable() != 0 && manager->IsTcpAvailable() != 0)
    {
        MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
        dialog->SetText(LoadGameString(0xa6, 0xfe));
        dialog->SetTwoButton(true);
        dialog->Callback = TcpipxDialogCallback;
        dialog->OkButton->SetUpPicture("bh_ipx.tga");
        dialog->OkButton->SetDownPicture("bg_ipx.tga");
        dialog->OkButton->Disabled = false;
        dialog->OkButton->Result = 1;
        dialog->CancelButton->SetUpPicture("bh_tcp.tga");
        dialog->CancelButton->SetDownPicture("bg_tcp.tga");
        dialog->CancelButton->Disabled = false;
        dialog->CancelButton->Result = 2;
        dialog->Activate();
        return;
    }

    if (manager->IsIpxAvailable() != 0)
    {
        ShowMenuMessage(0x36f, [](int32_t) { DoTheIpxThang(); });
        return;
    }

    if (manager->IsTcpAvailable() != 0)
    {
        ShowMenuMessage(0x36e, [](int32_t) { DoTheTcpThang(); });
    }
}

void DoExitToZone1()
{
    // Port: the original started the Internet Gaming Zone's launcher (zonea502.exe, from the registry) or the web
    // browser on the Zone's MechCommander page; the Zone is gone, so nothing is started. It then quit, as here.
    KillTheGame();
}

void DoExitToZone()
{
    AskMenuQuestion(GlobalLogPtr->QuestionDialog, 0x4e9, DoExitToZone1, nullptr, "bh_cancl.tga", true);
}

void DoExitToMplayer()
{
    // Port: the original started mplaynow.exe (the Mplayer.com lobby, long gone) and quit; the port takes the path
    // of a failed start: the error message.
    ShowMenuMessage(0x353);
}

void ShowInternet()
{
    AskMenuQuestion(GlobalLogPtr->MessageDialog, 0x352, DoExitToMplayer, DoExitToZone, "bh_cancl.tga", true);
}

void HostGame()
{
    MCSplashScreen* screen = GlobalLogPtr->LanScreen;
    const std::string playerName = ElementText(screen, 4);
    SaveUserName(playerName);
    screen->ShowBlock(1);
    GuiSystem()->SetText(screen->Elements[11]);
    screen->Element<MCLogTextObject>(10)->SetStringBuffer(
        MCFormatPrintf(LoadGameString(0x377, 0xfe).c_str(), playerName.c_str()));
}

void ResetReadyRoom()
{
    MCGuiEvent event;
    event.Clear();
    event.Type = 0x13;
    event.Data = 0;
    ReadyRoomPlayerListHandleEvent(GlobalLogPtr->ConnectScreen->Elements[3], &event);
    event.Clear();
    event.Type = 0x1e;
    event.Data = 4;
    PlayerListHandleEvent(GlobalLogPtr->LanScreen->Elements[3], &event);
}

void JoinGame()
{
    if (MPlayer == nullptr || MPlayer->SessionManager == nullptr)
    {
        return;
    }

    MCSessionManager* manager = MPlayer->SessionManager;

    if (_GUID* game = GlobalLogPtr->LanScreen->Element<MCGameList>(2)->GetSelectedGame(); game != nullptr)
    {
        if (MCFidpSession* session = manager->FindMatchingSession(game); session != nullptr)
        {
            std::string playerName = ElementText(GlobalLogPtr->LanScreen, 4);
            SaveUserName(playerName);

            if (session->SessionDesc.dwCurrentPlayers < session->SessionDesc.dwMaxPlayers)
            {
                const int32_t result = manager->JoinSession(&session->SessionDesc.guidInstance, playerName.data());
                CountLanPlayers();

                if (result == 0)
                {
                    EnterReadyRoom(GlobalLogPtr->LanScreen, true);
                    ReadyRoomTicks = 0;
                    ResetReadyRoom();
                    return;
                }
            }
        }
    }

    // The game can't be joined.
    ShowMenuMessage(0xa7);
}

void CreateSession()
{
    if (MPlayer != nullptr && MPlayer->SessionManager != nullptr)
    {
        std::string sessionName = ElementText(GlobalLogPtr->LanScreen, 10);
        const int32_t maxPlayers = TypedMaxPlayers();
        std::string playerName = ElementText(GlobalLogPtr->LanScreen, 4);
        MPlayer->CreateSession(sessionName.data(), playerName.data(), maxPlayers);
    }

    GlobalLogPtr->LanScreen->ShowGuiWindow(false);
    GlobalLogPtr->ConnectScreen->ShowGuiWindow(true);
    ReadyRoomTicks = 0;
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->ConnectScreen;
    GlobalLogPtr->LogisticsState = 0xe;
    ResetReadyRoom();
}

void CreateSerialSession()
{
    if (MPlayer == nullptr || MPlayer->SessionManager == nullptr)
    {
        return;
    }

    const int32_t port = ElementNumber(GlobalLogPtr->SerialScreen, 5);

    if (port < 1 || port > 4)
    {
        return;
    }

    MPlayer->SessionManager->ConnectComPort(static_cast<uint32_t>(port), 0xe100, 0, 0, 4);
    std::string playerName = ElementText(GlobalLogPtr->SerialScreen, 4);
    SaveUserName(playerName);

    if (MPlayer->CreateSession(const_cast<char*>("SerialGame"), playerName.data(), 2) == 0)
    {
        EnterReadyRoom(GlobalLogPtr->SerialScreen, std::nullopt);
    }
}

void SerialJoinButtonPressed()
{
    if (MPlayer == nullptr)
    {
        return;
    }

    SaveUserName(ElementText(GlobalLogPtr->SerialScreen, 4));
    WhackTimer = true;
    const int32_t port = ElementNumber(GlobalLogPtr->SerialScreen, 5);

    if (port > 0 && port < 5 &&
        MPlayer->SessionManager->ConnectComPort(static_cast<uint32_t>(port), 0xe100, 0, 0, 4) == 0)
    {
        JoinSerialSession();
    }
}

void JoinSerialSession()
{
    if (MPlayer == nullptr)
    {
        return;
    }

    std::string playerName = ElementText(GlobalLogPtr->SerialScreen, 4);

    if (MPlayer->JoinSession(const_cast<char*>("SerialGame"), playerName.data()) != 0)
    {
        // No game yet: try again in a second, with a way out.
        WaitWithCancel(GlobalLogPtr->SerialScreen, 0, 0xb1, CancelToConnect, false);
        return;
    }

    CountLanPlayers();
    EnterReadyRoom(GlobalLogPtr->SerialScreen, true);
    GlobalLogPtr->MessageDialog->Deactivate(0);
}

void JoinModemSession()
{
    std::string playerName = ElementText(GlobalLogPtr->ModemScreen, 4);

    if (MPlayer->JoinSession(const_cast<char*>("MC Modem Game"), playerName.data()) != 0)
    {
        GuiSystem()->AddTimer(GlobalLogPtr->ModemScreen, 1, 1000, 0, 0, 0);
        WhackTimer = false;
        return;
    }

    CountLanPlayers();
    EnterReadyRoom(GlobalLogPtr->ModemScreen, true);
    GlobalLogPtr->MessageDialog->Deactivate(0);
    WhackTimer = true;
}

int32_t DialModemSession()
{
    if (MPlayer == nullptr)
    {
        return -1;
    }

    SaveUserName(ElementText(GlobalLogPtr->ModemScreen, 4));
    const int32_t result = MPlayer->SessionManager->Dial();

    if (result == 0)
    {
        JoinModemSession();
        return 0;
    }

    GuiSystem()->AddTimer(GlobalLogPtr->ModemScreen, 0, 1000, 0, 0, 0);
    WhackTimer = false;
    return result == DialStillConnecting ? 2 : 1;
}

void AllGoneCallback(int32_t)
{
    MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
    dialog->SetText(LoadGameString(0xbb, 0xfe));
    dialog->SetTwoButton(false);
    dialog->Callback = nullptr;
    dialog->OkButton->SetUpPicture("bh_okay.tga");
    dialog->OkButton->SetDownPicture("bg_okay.tga");
    dialog->OkButton->Callback()->SetExec(nullptr);
    dialog->Activate();
}

void GOCallback()
{
    if (MPlayer == nullptr)
    {
        return;
    }

    GlobalLogPtr->CurrentScreen->ShowGuiWindow(false);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->SessionScreen;
    GlobalLogPtr->ShowLogScreen(1, 1);
    GlobalLogPtr->LogisticsState = 8;
    GlobalLogPtr->SessionScreen->Activate(false);

    if (MPlayer != nullptr)
    {
        MPlayer->SessionManager->LockSession();
        return;
    }

    GlobalLogPtr->MessageDialog->Callback = AllGoneCallback;
}

void Go()
{
    if (MPlayer == nullptr || MPlayer->SessionManager == nullptr)
    {
        return;
    }

    MCSessionManager* manager = MPlayer->SessionManager;
    MCFidpSession* session = manager->CurrentSession;

    if (session == nullptr)
    {
        return;
    }

    // A LAN session takes the number typed; a lobby (0x10) game six, a modem or serial game two.
    const int32_t connection = manager->CurrentConnection;
    int32_t maxPlayers;

    if (connection == 2 || connection == 1)
    {
        maxPlayers = TypedMaxPlayers();
    }
    else
    {
        maxPlayers = connection == 0x10 ? MaxSessionPlayers : 2;
    }

    const uint32_t players = session->SessionDesc.dwCurrentPlayers;
    Assert(static_cast<int32_t>(players) <= maxPlayers, players, " How'd we get too many players? ");
    GOCallback();
}

void Leave()
{
}

void WaitForCall()
{
    if (MPlayer != nullptr && MPlayer->SessionManager != nullptr)
    {
        auto* modems = GlobalLogPtr->ModemScreen->Element<MCLogScrollTextObject>(10);

        if (std::optional<std::string> modem = modems->GetTextLine(modems->HighlightLine[0] + 1))
        {
            MPlayer->SessionManager->ConnectModem(const_cast<char*>(""), modem->data());
            std::string playerName = ElementText(GlobalLogPtr->ModemScreen, 4);
            SaveUserName(playerName);
            MPlayer->CreateSession(const_cast<char*>("MC Modem Game"), playerName.data(), 2);
        }
    }

    GlobalLogPtr->LanScreen->Element<MCLogTextObject>(11)->InitBuffer(2, MCLogInputType::Digits);
    GlobalLogPtr->ModemScreen->ShowGuiWindow(false);
    GlobalLogPtr->ConnectScreen->Element<MCLogButton>(2)->Disabled = false;
    GlobalLogPtr->ConnectScreen->ShowGuiWindow(true);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->ConnectScreen;
    GlobalLogPtr->LogisticsState = 0xe;
    GuiSystem()->SetText(GlobalLogPtr->ModemScreen->Elements[4]);
    GuiSystem()->AddTimer(GlobalLogPtr->ModemScreen->Elements[4], 0, MCPort::CaretBlinkTime(), 0, 0, 0);
}

void GetNumber()
{
    GuiSystem()->SetText(GlobalLogPtr->ModemScreen->Elements[5]);
    GlobalLogPtr->ModemScreen->ShowBlock(1);
}

void CancelDial()
{
    WhackTimer = true;

    if (MPlayer != nullptr)
    {
        MPlayer->SessionManager->CancelDialing();
    }
}

void Dial()
{
    MCSplashScreen* screen = GlobalLogPtr->ModemScreen;
    auto* modems = screen->Element<MCLogScrollTextObject>(10);

    if (MPlayer == nullptr || MPlayer->SessionManager == nullptr)
    {
        return;
    }

    std::optional<std::string> modem = modems->GetTextLine(modems->HighlightLine[0] + 1);

    if (!modem.has_value())
    {
        return;
    }

    std::string number = ElementText(screen, 5);
    MPlayer->SessionManager->ConnectModem(number.data(), modem->data());
    const int32_t result = DialModemSession();
    Assert(result != 1, 0, "Not currently connected to a modem");

    switch (result)
    {
        case 0:
        {
            EnterReadyRoom(screen, true);
            return;
        }
        case 1:
        case 3:
        {
            ShowMenuMessage(0xb3);
            return;
        }
        case 2:
        {
            // Still dialling: check again in a second; the button cancels.
            WaitWithCancel(screen, 0, 0xb2, CancelDial, true);
            return;
        }

        default:
            return;
    }
}

void ModemListHandleEvent(MCGuiObject* object, MCGuiEvent* event)
{
    if (event->Type != 1)
    {
        return;
    }

    // A click picks the modem on the line under the mouse.
    auto* list = static_cast<MCLogScrollTextObject*>(object);
    const int32_t lineHeight = Fonts[0][list->FontIndex]->Height();
    const int32_t line = (list->FirstPixel + event->Y - list->GlobalY()) / (lineHeight + 4);

    if (list->GetTextLine(line + 1).has_value())
    {
        list->HighlightLine[0] = line;
    }
}

void PlayerListHandleEvent(MCGuiObject* object, MCGuiEvent* event)
{
    if (event->Type != 0x1e)
    {
        return;
    }

    auto* list = static_cast<MCLogScrollTextObject*>(object);

    if (event->Data == 3)
    {
        // A game was picked: list its players.
        if (object->Parent == nullptr || MPlayer == nullptr || MPlayer->SessionManager == nullptr)
        {
            return;
        }

        auto* games = static_cast<MCGenericScreen*>(object->Parent)->Element<MCGameList>(2);
        list->Clear();

        if (games == nullptr || games->SelectedSession <= -1)
        {
            return;
        }

        MCFidpSession* session = MPlayer->SessionManager->FindMatchingSession(games->GetSelectedGame());
        MCFLinkedList<MCFidpPlayer>* players =
            session != nullptr ? MPlayer->SessionManager->GetPlayers(session) : nullptr;

        if (players != nullptr)
        {
            const int32_t count = players->Count;
            players->Current = players->HeadLink;

            for (int32_t i = count; i > 0; i--)
            {
                list->Print(players->ReadAndNext()->Name, 0x1f);
            }
        }
    }
    else if (event->Data == 4)
    {
        list->Clear();
    }
}

void ReadyRoomPlayerListHandleEvent(MCGuiObject* object, MCGuiEvent* event)
{
    if (event->Type != 0x13 || MPlayer == nullptr)
    {
        return;
    }

    MCSessionManager* manager = MPlayer->SessionManager;

    if (manager == nullptr)
    {
        return;
    }

    auto* list = static_cast<MCLogScrollTextObject*>(object);
    MCFidpSession* session = manager->CurrentSession;
    ++ReadyRoomTicks;

    if (session == nullptr)
    {
        return;
    }

    if (manager->IsHost == 0 && LaunchedFromLobby == 0)
    {
        manager->SendPing();
    }

    list->Clear();
    MCFLinkedList<MCFidpPlayer>* players = manager->GetPlayers(session);

    if (players == nullptr)
    {
        return;
    }

    // Each player, with the ping outside a lobby launch.
    for (MCFLink<MCFidpPlayer>* link = players->HeadLink; link != nullptr && link->Data != nullptr; link = link->Next)
    {
        MCFidpPlayer* player = link->Data;

        if (LaunchedFromLobby == 0)
        {
            list->Print(std::format("{} - {:04} ms", player->Name, player->LastLatency), 0x1f);
        }
        else
        {
            list->Print(player->Name, 0x1f);
        }
    }

    // The host can go once someone else is in.
    if (manager->IsHost != 0)
    {
        GlobalLogPtr->ConnectScreen->Element<MCLogButton>(2)->Disabled = players->Count < 2;
    }
}

void LanScreenHandleEvent(MCGuiObject* object, MCGuiEvent* event)
{
    auto* screen = static_cast<MCGenericScreen*>(object);

    if (event->Type == 0x13)
    {
        screen->Elements[2]->HandleEvent(event);
        screen->Elements[3]->HandleEvent(event);
        return;
    }

    if (event->Type != 0x1e)
    {
        return;
    }

    auto* joinButton = screen->Element<MCLogButton>(6);

    if (event->Data == 3)
    {
        // A game was picked: its players are listed, and it can be joined unless full.
        MCFidpSession* session =
            MPlayer->SessionManager->FindMatchingSession(screen->Element<MCGameList>(2)->GetSelectedGame());
        screen->Elements[3]->HandleEvent(event);
        joinButton->Disabled = session->SessionDesc.dwMaxPlayers <= session->SessionDesc.dwCurrentPlayers;
        return;
    }

    if (event->Data == 4)
    {
        screen->Elements[3]->HandleEvent(event);
        joinButton->Disabled = true;
    }
}

void ComPortTextHandleEvent(MCGuiObject* object, MCGuiEvent* event)
{
    if (event->Type != 10)
    {
        return;
    }

    // The port number typed is checked (1 to 4); a bad one is backspaced out. The screen hears whether it is good.
    auto* entry = static_cast<MCLogTextObject*>(object);
    bool valid = false;
    const std::string text(entry->Text());

    if (!text.empty() && event->LParam != 999)
    {
        const int32_t port = std::atoi(text.c_str());

        if (port < 5 && port != 0)
        {
            valid = true;
        }
        else
        {
            MCGuiEvent backspace;
            backspace.Type = 10;
            backspace.Key = 8;
            backspace.LParam = 999;
            object->HandleEvent(&backspace);
        }
    }

    MCGuiEvent notify;
    notify.Type = 0x1e;
    notify.Data = 6;
    notify.LParam = valid ? 1 : 0;
    object->Parent->HandleEvent(&notify);
}

void SerialScreenHandleEvent(MCGuiObject* object, MCGuiEvent* event)
{
    if (event->Type == 0x13)
    {
        GuiSystem()->RemoveTimer(object, 0);

        if (!WhackTimer)
        {
            JoinSerialSession();
        }

        return;
    }

    if (event->Type == 0x1e && event->Data == 6)
    {
        // The host and join buttons need a good port number.
        const bool disabled = event->LParam == 0;
        auto* screen = static_cast<MCGenericScreen*>(object);
        screen->Element<MCLogButton>(2)->Disabled = disabled;
        screen->Element<MCLogButton>(3)->Disabled = disabled;
    }
}

void ModemScreenHandleEvent(MCGuiObject* object, MCGuiEvent* event)
{
    if (event->Type != 0x13)
    {
        return;
    }

    if (event->Data == 0)
    {
        GuiSystem()->RemoveTimer(object, 0);

        if (!WhackTimer)
        {
            DialModemSession();
        }
    }
    else if (event->Data == 1)
    {
        GuiSystem()->RemoveTimer(object, 1);

        if (!WhackTimer)
        {
            JoinModemSession();
        }
    }
}
