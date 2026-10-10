#include "stdafx.h"
#include "main/MCGameSession.h"
#include "abl/MCAblDebuggerWindow.h"
#include "abl/MCScrollingTextWindow.h"
#include "camera/MCCamera.h"
#include "color/MCWaterCycle.h"
#include "gameos/MCSoundRenderer.h"
#include "gui/MCUpdateDisplay.h"
#include "gui/MCGuiPort.h"
#include "gui/MCGuiSystem.h"
#include "gui/MCGuiTextObject.h"
#include "lib/MCFastFileSet.h"
#include "lib/MCFatal.h"
#include "lib/MCFitIniFile.h"
#include "linkup/sessionmanager.h"
#include "main/MCAblDebuggerConsole.h"
#include "main/MCGameContext.h"
#include "main/MCGamePaths.h"
#include "main/MCSystemConfig.h"
#include "mission/MCMission.h"
#include "network/multplyr.h"
#include "object/MCMechWarrior.h"
#include "sound/MCSoundSystem.h"

namespace
{
    /// <summary>The ABL debugger window's place on the screen.</summary>
    struct MCDebuggerPlacement
    {
        int32_t X = 0;
        int32_t Y = 400;
        int32_t Width = 260;
        int32_t Height = 125;
    };

    /// <summary>iface.fit's [ABL Window] (the defaults for what it lacks).</summary>
    MCDebuggerPlacement ReadDebuggerPlacement()
    {
        MCDebuggerPlacement placement;
        MCFitIniFile ifaceFile;

        if (ifaceFile.Open(GamePath(InterfacePath, "iface", ".fit")) != 0 || ifaceFile.SeekBlock("ABL Window") != 0)
        {
            return placement;
        }

        // Read as unsigned numbers, as the original did.
        const auto read = [&ifaceFile](std::string_view name, int32_t& value)
        {
            if (const MCFitResult<uint32_t> number = ifaceFile.Read<uint32_t>(name); number.has_value())
            {
                value = static_cast<int32_t>(*number);
            }
        };

        read("X", placement.X);
        read("Y", placement.Y);
        read("Width", placement.Width);
        read("Height", placement.Height);
        return placement;
    }
}

MCGameSession::MCGameSession()
{
    // Port: the original switched off the screen saver, low-power and power-off timeouts (SystemParametersInfo) and
    // put them back when it ended. SDL keeps the screen saver off while its window is up.
    GlobalPane = ScreenPort()->Frame();
    GlobalWindow = ScreenPort()->Frame()->Window;

    if (DebugGameSystem)
    {
        _GameSystemWindow = MCMakeGui<MCScrollingTextWindow>();
        _GameSystemWindow->Init(10, 20, 250, 300, "Game System");
        ScreenWindow()->AddChild(_GameSystemWindow.get());
        GameSystemWindow = _GameSystemWindow.get();
    }

    if (AblDebuggerEnabled)
    {
        const MCDebuggerPlacement placement = ReadDebuggerPlacement();
        _DebuggerWindow = MCMakeGui<MCAblDebuggerWindow>();
        _DebuggerWindow->Init(placement.X, placement.Y, placement.Width, placement.Height, "ABL Developer Studio (tm)");
        ScreenWindow()->AddChild(_DebuggerWindow.get());
        _DebuggerWindow->Input()->SetEventRoutine(AblDebuggerEventRoutine);
    }

    if (SoundSystem() == nullptr)
    {
        MCGameContext::Current().SetSoundSystem(std::make_unique<MCSoundSystem>("sound"));
    }

    _ColorCallback.SetExec(CycleColors);
    GuiSystem()->AddCallback(&_ColorCallback);

    // Multiplayer is made to ask the session manager whether a lobby launched the game, and dropped when not. The
    // port has no lobby (MCDirectPlay), so it is always dropped here.
    MPlayer = new MCMultiPlayer;
    MPlayer->Init(0x7d000, 0x100, 100);
    LaunchedFromLobby = MPlayer->SessionManager->WasLaunchedFromLobby() != 0 ? 1 : 0;

    if (LaunchedFromLobby == 0)
    {
        delete MPlayer;
        MPlayer = nullptr;
    }

    MCGameContext::Current().SetMission(std::make_unique<MCMission>());

    // A game segment (-mission N on the command line) starts SYSTEM.CFG's missionName; otherwise the campaign.
    if (const int32_t result = Mission()->Load(GlobalGameSegment == 0 ? CampaignFile : MissionName); result != 0)
    {
        Fatal(result, "Error initializing mission.");
    }
}

MCGameSession::~MCGameSession()
{
    _DebuggerWindow.reset();

    if (Mission() != nullptr)
    {
        // Taken down while still installed: what it frees reaches for it.
        Mission()->Shutdown();
        MCGameContext::Current().SetMission(nullptr);
    }

    if (MCGuiSystem* gui = GuiSystem(); gui != nullptr)
    {
        gui->RemoveCallback(&_ColorCallback);
    }

    _ColorCallback.Clear();

    if (MPlayer != nullptr)
    {
        delete MPlayer;
        MPlayer = nullptr;
    }

    MCGameContext::Current().FastFiles().Clear();
    MCGameContext::Current().SetSoundSystem(nullptr);

    if (_GameSystemWindow != nullptr)
    {
        GameSystemWindow = nullptr;
        _GameSystemWindow.reset();
    }
}

void KillTheGame()
{
    if (MPlayer != nullptr)
    {
        MCMultiPlayer* player = MPlayer;
        player->Destroy();
        delete player;
        MPlayer = nullptr;
    }

    MouseTimerKill();
    MCSoundRenderer::Uninstall();
    GuiSystem()->CloseDisplay();
    std::exit(1);
}
