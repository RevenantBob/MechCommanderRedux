#include "stdafx.h"
#include "main/honorb.h"
#include "gameos/soundrenderer.h"
#include "gui/asystem.h"
#include "gui/updisp.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "abl/MCAblDebugger.h"
#include "abl/MCAblDebuggerWindow.h"
#include "abl/MCAblRuntime.h"
#include "abl/MCScrollingTextWindow.h"
#include "camera/MCCamera.h"
#include "camera/MCCameraList.h"
#include "color/MCPalette.h"
#include "color/MCWaterCycle.h"
#include "gui/aport.h"
#include "gui/atextbox.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFastFileSet.h"
#include "lib/MCFitIniFile.h"
#include "linkup/session.h"
#include "linkup/sessionmanager.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "main/MCGameContext.h"
#include "main/logistics.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "object/MCObjectType.h"
#include "object/MCMechWarrior.h"
#include "object/MCMoverGameSystem.h"
#include "platform/MCFileSystem.h"
#include "platform/MCInput.h"
#include "platform/MCPresenter.h"
#include "sound/soundsys.h"
#include "sprite/MCSpriteManager.h"
#include "terrain/MCTerrainTiles.h"
#include "object/MCObjectTypeManager.h"

char CampaignFile[20] = "campaign";
char MissionName[80] = "MechCmdr1.fit";
uint32_t AblIncludeDebugInfo = 0;
uint32_t AblDebuggerEnabled = 0;
uint32_t AblDebuggerX = 0;
uint32_t AblDebuggerY = 400;
uint32_t AblDebuggerWidth = 260;
uint32_t AblDebuggerHeight = 125;
int32_t DisplayMode = 0;
MCAblDebuggerWindow* AblDebuggerWindow = nullptr;
MCGuiCallback* ColorCallback = nullptr;
int DebugGameSystem = 0;
int GNoSound = 0;
int ScreenSaverActive = 0;
int LowPowerActive = 0;
int PowerOffActive = 0;
int32_t LanguageOffset = 0;

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
    SoundRendererUninstall();
    Application->ShutdownDirectDraw();
    FatalShutDown();
    std::exit(1);
}

bool CheckForCDInDrive(int32_t checkDisk, bool retry)
{
    // The original scanned drives C: to Z: for a CD holding hidden.txt (and data\tiles\gtiles90.pak when
    // checkDisk is 1), pointed every path that starts with a drive letter at it, and otherwise asked the player to
    // insert the disc (or quit). The port reads everything from the install folder, so the disc is always there.
    return true;
}

namespace
{
    /// <summary>Reads a SYSTEM.CFG path (79 characters at most); a missing one is fatal.</summary>
    void ReadPath(MCFitIniFile* file, const char* varName, char* path, const char* errMessage)
    {
        const int32_t result = file->ReadIdString(varName, path, 0x4f);

        if (result != 0)
        {
            Fatal(result, errMessage);
        }
    }

    /// <summary>Reads a SYSTEM.CFG path; a missing one is fatal.</summary>
    void ReadPath(MCFitIniFile* file, const char* varName, std::string& path, const char* errMessage)
    {
        MCFitResult<std::string> value = file->Read<std::string>(varName);

        if (!value)
        {
            Fatal(std::to_underlying(value.error()), errMessage);
        }

        path = std::move(*value);
    }

    /// <summary>Reads a SYSTEM.CFG number; a missing one is fatal.</summary>
    void ReadULong(MCFitIniFile* file, const char* varName, uint32_t& value, const char* errMessage)
    {
        const int32_t result = file->ReadIdULong(varName, value);

        if (result != 0)
        {
            Fatal(result, errMessage);
        }
    }

    void ReadLong(MCFitIniFile* file, const char* varName, int32_t& value, const char* errMessage)
    {
        const int32_t result = file->ReadIdLong(varName, value);

        if (result != 0)
        {
            Fatal(result, errMessage);
        }
    }

    /// <summary>The "can't read system.cfg" exit.</summary>
    [[noreturn]] void ClosingMessage()
    {
        MCInput::ShowCursor(true);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "MechCommander Expansion or Editor already running.");

        if (!MCNoMessageBoxes)
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "MechCommander Closing",
                                     "MechCommander Expansion or Editor already running.", nullptr);
        }

        KillTheGame();
    }
}

void SystemInit()
{
    // Port: the original registered the "QueryCancelAutoPlay" window message (uMessage) to keep the CD's autorun from
    // starting while the game ran, and refused to run when system.cfg could not be opened exclusively (another copy
    // of the game or the editor had it). The port has no autorun and reads the file shared.
    auto* systemFile = new MCFitIniFile;

    if (systemFile->Open("system.cfg") != 0)
    {
        ClosingMessage();
    }

    // The "systemHeap" block (systemHeapSize, guiHeapSize, logisticsHeapSize) sized the original's heaps; the port has
    // none and doesn't read it.

    // Empty blocks switch sound and music on.
    UseSound = systemFile->SeekBlock("UseSound") == 0 ? 1 : 0;

    if (systemFile->SeekBlock("UseMusic") == 0)
    {
        UseMusic = 1;

        if (UseSound == 0)
        {
            UseMusic = 0;
        }
    }
    else
    {
        UseSound = 0;
        UseMusic = 0;
    }

    if (GNoSound != 0)
    {
        UseSound = 0;
        UseMusic = 0;
    }

    if (systemFile->SeekBlock("DebugGameSystem") == 0)
    {
        DebugGameSystem = 1;
    }

    //---------------------------------------------------------------------------------------------------------------
    // ABL.
    if (systemFile->SeekBlock("ABL") != 0)
    {
        Fatal(0, " Unable to find ABL settings. ");
    }

    // ABL's heap, stack, code block, module and static variable sizes and its debugger limits are gone (the port's
    // ABL grows); the keys are still read, so a SYSTEM.CFG without them still fails as before.
    uint32_t ignoredSize = 0;
    int32_t ignoredLimit = 0;
    ReadULong(systemFile, "SymbolTableHeapSize", ignoredSize, "Could not find ABL SymbolTableHeapSize. ");
    ReadULong(systemFile, "StackHeapSize", ignoredSize, "Could not find ABL StackHeapSize. ");
    ReadULong(systemFile, "CodeHeapSize", ignoredSize, "Could not find ABL CodeHeapSize. ");
    ReadULong(systemFile, "RunTimeStackSize", ignoredSize, "Could not find ABL RunTimeStackSize. ");
    ReadULong(systemFile, "MaxCodeBlockSize", ignoredSize, "Could not find ABL MaxCodeBlockSize. ");
    ReadULong(systemFile, "MaxRegisteredModules", ignoredSize, "Could not find ABL MaxRegisteredModules. ");
    ReadULong(systemFile, "MaxStaticVariables", ignoredSize, "Could not find ABL MaxStaticVariables. ");
    ReadULong(systemFile, "IncludeDebugInfo", AblIncludeDebugInfo, "Could not find ABL IncludeDebugInfo. ");
    ReadULong(systemFile, "DebuggerEnabled", AblDebuggerEnabled, "Could not find ABL DebuggerEnabled. ");
    ReadLong(systemFile, "MaxWatchesPerModule", ignoredLimit, "Could not find ABL MaxWatchesPerModule. ");
    ReadLong(systemFile, "MaxBreakPointsPerModule", ignoredLimit, "Could not find ABL MaxBreakPointsPerModule. ");

    //---------------------------------------------------------------------------------------------------------------
    // Paths.
    int32_t result = systemFile->SeekBlock("systemPaths");

    if (result != 0)
    {
        Fatal(result, "Could not find systemPaths.  Using Defaults.");
    }

    ReadPath(systemFile, "savePath", SavePath, " Could not find save path ");
    // Copies of the game on one machine share the user folder: each keeps its temp FITs under its process ID, or a
    // multiplayer client loads the host's generated scenario (bridge.fit) or the reverse.
    std::snprintf(SaveTempPath, sizeof(SaveTempPath), "%stemp\\%u\\", SavePath, MCPort::ProcessId());
    MCFileSystem::MakeDirectory(SaveTempPath);
    ReadPath(systemFile, "terrainPath", TerrainPath, " Could not find terrain path ");
    ReadPath(systemFile, "palettePath", PalettePath, " Could not find palette path ");
    ReadPath(systemFile, "artPath", ArtPath, " Could not find art path ");
    ReadPath(systemFile, "fontPath", FontPath, " Could not find font path ");
    ReadPath(systemFile, "soundPath", SoundPath, " Could not find sound path ");
    ReadPath(systemFile, "spritePath", SpritePath, " Could not find sprite path ");
    ReadPath(systemFile, "shapesPath", ShapesPath, " Could not find shapes path ");
    ReadPath(systemFile, "objectPath", ObjectPath, " Could not find object path ");
    ReadPath(systemFile, "missionPath", MissionPath, " Could not find mission path ");
    ReadPath(systemFile, "warriorPath", WarriorPath, " Could not find warrior path ");
    ReadPath(systemFile, "profilePath", ProfilePath, " Could not find profile path ");
    ReadPath(systemFile, "cameraPath", CameraPath, " Could not find camera path ");
    ReadPath(systemFile, "tilePath", TilePath, " Could not find tile path ");
    ReadPath(systemFile, "tile90Path", Tile90Path, " Could not find tile90 path ");
    ReadPath(systemFile, "interfacePath", InterfacePath, " Could not find interface path ");
    ReadPath(systemFile, "moviePath", MoviePath, " Could not find movie path ");
    ReadPath(systemFile, "missionName", MissionName, " Could not find Mission File Name ");
    ReadPath(systemFile, "CDsoundPath", CDsoundPath, " Could not find CD sound path ");
    ReadPath(systemFile, "CDspritePath", CDspritePath, " Could not find CD sprite path ");
    ReadPath(systemFile, "CDmoviePath", CDmoviePath, " Could not find CD movie path ");

    //---------------------------------------------------------------------------------------------------------------
    // Every *.fst in the game folder is a FastFile (SYSTEM.CFG's [FastFiles] list isn't read).
    MCFastFileSet& fastFiles = MCGameContext::Current().FastFiles();

    for (const std::string& name : MCFileSystem::FindFiles("*.fst"))
    {
        // A FastFile that doesn't open is left out, as in the original.
        (void)fastFiles.Open(name);
    }

    systemFile->Close();
    delete systemFile;

    //---------------------------------------------------------------------------------------------------------------
    // The prefs.
    auto* prefsFile = new MCFitIniFile;
    result = prefsFile->Open("prefs.cfg");

    if (result != 0)
    {
        Fatal(result, "Could not open prefs.cfg.");
    }

    result = prefsFile->SeekBlock("MechCommander");

    if (result != 0)
    {
        Fatal(result, "Could not find MechCommander Prefs.");
    }

    if (prefsFile->ReadIdBoolean("PaletteCycle", Application->PaletteCycle) != 0)
    {
        Application->PaletteCycle = 0;
    }

    if (prefsFile->ReadIdLong("Gamma", Application->GammaLevel) != 0)
    {
        Application->GammaLevel = 0;
    }

    if (prefsFile->ReadIdBoolean("Use90Pixel", Use90PixelSprite) != 0)
    {
        Use90PixelSprite = 0;
    }

    if (prefsFile->ReadIdBoolean("Force45Pixel", Only45Pixel) != 0)
    {
        Only45Pixel = 0;
    }

    // One sprite size wins: 90-pixel sprites unless 45 is forced; without 90, 45 only.
    if (Use90PixelSprite != 0 && Only45Pixel != 0)
    {
        Use90PixelSprite = 0;
    }

    if (Use90PixelSprite == 0 && Only45Pixel == 0)
    {
        Only45Pixel = 1;
    }

    // Port: the full-size (90-pixel) mech art is always loaded and used: the camera stays at scale 100 and the zoom
    // scales the world view instead (the prefs only mattered for machines short of memory).
    Use90PixelSprite = 1;
    Only45Pixel = 0;

    if (prefsFile->ReadIdBoolean("Force32Mb", Force32MB) != 0)
    {
        Force32MB = 0;
    }

    if (prefsFile->ReadIdBoolean("Force16Mb", Force16MB) != 0)
    {
        Force16MB = 0;
    }
    else if (Force16MB != 0 && Force32MB != 0)
    {
        Force32MB = 0;
    }

    int directDraw = 0;

    if (prefsFile->ReadIdBoolean("DirectDraw", directDraw) != 0)
    {
        directDraw = 0;
    }

    GFullScreen = directDraw != 0 ? 1 : 0;
    // Port: a port-only key; the picture keeps 4:3 with bars unless it is set.
    int stretchToFit = 0;

    if (prefsFile->ReadIdBoolean("StretchToFit", stretchToFit) != 0)
    {
        stretchToFit = 0;
    }

    GStretchToFit = stretchToFit != 0 ? 1 : 0;
    // Port: a port-only key; the cursor is the system's unless it is set.
    int softwareCursor = 0;

    if (prefsFile->ReadIdBoolean("SoftwareCursor", softwareCursor) != 0)
    {
        softwareCursor = 0;
    }

    GSoftwareCursor = softwareCursor != 0 ? 1 : 0;
    // Port: a port-only key; the frame counter in the top-right corner. The command line's -fps sets it too.
    int showFps = 0;

    if (prefsFile->ReadIdBoolean("ShowFps", showFps) != 0)
    {
        showFps = 0;
    }

    GShowFpsPreference = showFps != 0 ? 1 : 0;
    GShowFps = showFps != 0 ? 1 : GShowFps;
    // Port: a port-only key; "vulkan" (the default) or "software". The command line's -renderer wins.
    char rendererName[32] = {};

    if (prefsFile->ReadIdString("Renderer", rendererName, sizeof(rendererName) - 1) == 0)
    {
        if (const std::optional<MCRendererKind> kind = MCRendererKindFromName(rendererName))
        {
            GRenderer = static_cast<int>(*kind);
        }
    }

    GRendererPreference = GRenderer;

    int32_t resolution = 0;

    // Port: the mode is still read, but the screen is the window's size (aSystem::startupDirectDraw).
    if (prefsFile->ReadIdLong("Resolution", resolution) == 0)
    {
        if (resolution == 1)
        {
            DisplayMode = 1;
            DisplayWidth = 800;
            DisplayHeight = 600;
        }
        else if (resolution == 2)
        {
            DisplayMode = 2;
            DisplayWidth = 1024;
            DisplayHeight = 768;
        }
        else if (resolution == 3)
        {
            DisplayMode = 3;
            DisplayWidth = 1280;
            DisplayHeight = 1024;
        }
    }

    if (prefsFile->ReadIdLong("Language", LanguageOffset) != 0)
    {
        LanguageOffset = 0;
    }

    if (prefsFile->ReadIdLong("Difficulty", GameDifficulty) != 0)
    {
        GameDifficulty = 1;
    }

    // Faithful: Brightness goes to the same field as Gamma, so it wins.
    if (prefsFile->ReadIdLong("Brightness", Application->GammaLevel) != 0)
    {
        Application->GammaLevel = 0;
    }

    if (prefsFile->ReadIdLong("MusicVolume", MusicVolume) != 0)
    {
        MusicVolume = 0x40;
    }

    if (prefsFile->ReadIdLong("RadioVolume", RadioVolume) != 0)
    {
        RadioVolume = 0x40;
    }

    if (prefsFile->ReadIdLong("SFXVolume", SfxVolume) != 0)
    {
        SfxVolume = 0x60;
    }

    prefsFile->Close();
    delete prefsFile;

    CheckForCDInDrive(1, false);
}

void AblDebuggerPrintCallback(std::string_view s)
{
}

namespace
{
    /// <summary>Set until the first event: that one points the debugger at the scenario brain.</summary>
    int32_t AblDebuggerFirstEvent = 1;

    /// <summary>The warrior whose index is the debugger module's id (how the "f" and "po" commands pick one).</summary>
    MCMechWarrior* DebugModuleWarrior(MCAblDebugger* debugger)
    {
        uint32_t index = static_cast<uint32_t>(debugger->DebugModule()->Id());

        if ((static_cast<int32_t>(index) < 1) || (Scenario->NumWarriors < index))
        {
            return nullptr;
        }

        return Scenario->Warriors[index];
    }
} // namespace

void AblDebuggerEventRoutine(MCGuiObject* object, MCGuiEvent* event)
{
    if (AblGetDebugger() == nullptr)
    {
        return;
    }

    if (AblDebuggerFirstEvent != 0)
    {
        AblGetDebugger()->ProcessCommand(MCAblDebugCommand::SelectModule, {}, 0, Scenario->ScenarioBrain.get());
        AblDebuggerFirstEvent = 0;
    }

    // Only Enter (a key event, 10) runs the typed line.
    if ((event->Type != 10) || (event->Key != '\r'))
    {
        return;
    }

    MCGuiTextObject* input = static_cast<MCGuiTextObject*>(object);
    char* text = input->Text;
    int32_t commandId = 0;
    char* strParam = nullptr;
    int32_t numParam = 0;

    switch (text[0])
    {
        case '?':
        {
            if (text[1] == '\0')
            {
                AblGetDebugger()->ProcessCommand(MCAblDebugCommand::Help, {}, 0, nullptr);
                input->SetText(nullptr);
                return;
            }

            if (text[1] == '?')
            {
                AblGetDebugger()->ProcessCommand(MCAblDebugCommand::ModuleInfo, {}, 0, nullptr);
                input->SetText(nullptr);
                return;
            }

            break;
        }

        case 'b':
        {
            // "b+ n" / "b- n": add or remove a break point at line n.
            if (text[1] == '+')
            {
                AblGetDebugger()->ProcessCommand(MCAblDebugCommand::AddBreakPoint, {}, std::atoi(text + 3), nullptr);
                input->SetText(nullptr);
                return;
            }

            if (text[1] == '-')
            {
                AblGetDebugger()->ProcessCommand(MCAblDebugCommand::RemoveBreakPoint, {}, std::atoi(text + 3), nullptr);
                input->SetText(nullptr);
                return;
            }

            break;
        }

        case 'c':
        {
            AblGetDebugger()->ProcessCommand(MCAblDebugCommand::Resume, {}, 0, nullptr);
            input->SetText(nullptr);
            return;
        }

        case 'f':
        {
            // "fn": the warrior's debug flags.
            MCMechWarrior* warrior = DebugModuleWarrior(AblGetDebugger());

            // Port fix: the original writes through a null warrior when the module's id isn't a warrior index
            // (OB-110).
            if (warrior != nullptr)
            {
                warrior->DebugFlags = static_cast<uint32_t>(std::atoi(text + 1));
            }

            input->SetText(nullptr);
            return;
        }

        case 'm':
        {
            // "m n": debug module n; "m" alone: the module being executed.
            MCAblModule* module = nullptr;

            if (text[1] != '\0')
            {
                module = AblRuntime()->InstanceAt(std::atoi(text + 2));
            }

            AblGetDebugger()->ProcessCommand(MCAblDebugCommand::SelectModule, {}, 0, module);
            input->SetText(nullptr);
            return;
        }

        case 'n':
        {
            // Network test commands.
            switch (text[1])
            {
                case 'd':
                {
                    if (MPlayer != nullptr)
                    {
                        MCMultiPlayer* player = MPlayer;
                        delete player;
                        MPlayer = nullptr;
                        input->SetText(nullptr);
                        return;
                    }

                    break;
                }

                case 'g':
                {
                    MCSessionManager* sessionManager = MCSessionManager::GetGlobalPointer(nullptr);

                    if ((sessionManager != nullptr) && (sessionManager->CurrentConnection != 0) &&
                        (sessionManager->IsHost != 0))
                    {
                        sessionManager->StartGame();
                        input->SetText(nullptr);
                        return;
                    }

                    break;
                }

                case 'h':
                {
                    MCSessionManager* sessionManager = MCSessionManager::GetGlobalPointer(nullptr);

                    if ((sessionManager != nullptr) && (sessionManager->CurrentConnection != 0))
                    {
                        MCFidpSession session;
                        char sessionName[] = "Trooper";
                        char playerName[] = "Host";
                        char message[] = "Successfully hosted session.";
                        session.SetName(sessionName);
                        session.SessionDesc.dwMaxPlayers = 6;
                        sessionManager->HostSession(session, playerName);
                        AblGetDebugger()->Print(message);
                        input->SetText(nullptr);
                        return;
                    }

                    break;
                }

                case 'i':
                {
                    char failed[] = "Connection Failed";
                    char established[] = "Connection Established";

                    if (MPlayer == nullptr)
                    {
                        MPlayer = new MCMultiPlayer;
                        MPlayer->Init(0x7d000, 0x100, 100);
                    }

                    if (MPlayer->ConnectIpx() != 0)
                    {
                        AblGetDebugger()->Print(failed);
                    }
                    else
                    {
                        AblGetDebugger()->Print(established);
                    }

                    input->SetText(nullptr);
                    return;
                }

                case 'j':
                {
                    MCSessionManager* sessionManager = MCSessionManager::GetGlobalPointer(nullptr);

                    if ((sessionManager == nullptr) || (sessionManager->CurrentConnection == 0))
                    {
                        break;
                    }

                    MCFLinkedList<MCFidpSession>* sessions = sessionManager->GetSessions();

                    if (sessions->Size() == 0)
                    {
                        break;
                    }

                    // Joins the first session listed.
                    sessions->Current = sessions->HeadLink;
                    MCFidpSession* session = sessions->HeadLink != nullptr ? sessions->HeadLink->Data : nullptr;
                    char playerName[] = "Client";
                    char message[] = "Successfully joined.";
                    sessionManager->JoinSession(&session->SessionDesc.guidInstance, playerName);
                    AblGetDebugger()->Print(message);
                    input->SetText(nullptr);
                    return;
                }

                case 'o':
                {
                    MCSessionManager* sessionManager = MCSessionManager::GetGlobalPointer(nullptr);

                    if (sessionManager != nullptr)
                    {
                        char address[] = "";
                        char message[] = "Successfully connected.";
                        sessionManager->ConnectTcp(address);
                        AblGetDebugger()->Print(message);
                        input->SetText(nullptr);
                        return;
                    }

                    break;
                }

                case 'p':
                {
                    MCSessionManager* sessionManager = MCSessionManager::GetGlobalPointer(nullptr);

                    if ((sessionManager != nullptr) && (sessionManager->CurrentConnection != 0))
                    {
                        sessionManager->ProcessSystemMessages();
                    }

                    break;
                }

                case 's':
                {
                    if (MCSessionManager::GetGlobalPointer(nullptr) == nullptr)
                    {
                        char message[] = "Created SessionManager.";
                        InitLinkUpBlocks();
                        new MCSessionManager(MultiPlayerAppGuid);
                        AblGetDebugger()->Print(message);
                        input->SetText(nullptr);
                        return;
                    }

                    break;
                }

                case 't':
                {
                    // "nt text": chat to everyone.
                    if (MPlayer == nullptr)
                    {
                        char message[] = "Not Connected";
                        AblGetDebugger()->Print(message);
                        input->SetText(nullptr);
                        return;
                    }

                    MPlayer->SendChat(0, text + 3);
                    input->SetText(nullptr);
                    return;
                }

                default:
                {
                    break;
                }
            }

            break;
        }

        case 'p':
        {
            // "po": the warrior's orders; "p expr": print a value.
            if (text[1] != 'o')
            {
                AblGetDebugger()->ProcessCommand(MCAblDebugCommand::PrintValue, text + 2, 0, nullptr);
                input->SetText(nullptr);
                return;
            }

            MCMechWarrior* warrior = DebugModuleWarrior(AblGetDebugger());

            if (warrior != nullptr)
            {
                warrior->DebugOrders();
                input->SetText(nullptr);
                return;
            }

            break;
        }

        case 's':
        {
            // "s+" / "s-": step on or off.
            if (text[1] == '+')
            {
                AblGetDebugger()->ProcessCommand(MCAblDebugCommand::Step, {}, 1, nullptr);
                input->SetText(nullptr);
                return;
            }

            if (text[1] == '-')
            {
                AblGetDebugger()->ProcessCommand(MCAblDebugCommand::Step, {}, 0, nullptr);
                input->SetText(nullptr);
                return;
            }

            break;
        }

        case 't':
        {
            // "t+" / "t-": trace on or off.
            if (text[1] == '+')
            {
                numParam = 1;
            }
            else if (text[1] == '-')
            {
                numParam = 0;
            }
            else
            {
                break;
            }

            AblGetDebugger()->ProcessCommand(MCAblDebugCommand::Trace, {}, numParam, nullptr);
            input->SetText(nullptr);
            return;
        }

        case 'w':
        {
            // Watches: "w+ name" (".name": the numeric form), "w- name", "w-" (clear all), "wf+"/"wf-" fetches,
            // "ws+"/"ws-" stores. The number is the watch type passed to the watch manager.
            commandId = 5;

            switch (text[1])
            {
                case '+':
                {
                    if (text[2] != '.')
                    {
                        strParam = text + 3;
                        numParam = 10;
                    }
                    else
                    {
                        strParam = text + 4;
                        numParam = 0x1a;
                    }

                    break;
                }

                case '-':
                {
                    if (text[2] != '\0')
                    {
                        strParam = text + 3;
                        numParam = 5;
                    }
                    else
                    {
                        commandId = 6;
                    }

                    break;
                }

                case 'f':
                {
                    if (text[2] == '+')
                    {
                        if (text[3] == '.')
                        {
                            strParam = text + 5;
                            numParam = 0x18;
                        }
                        else
                        {
                            strParam = text + 4;
                            numParam = 8;
                        }
                    }
                    else if (text[2] == '-')
                    {
                        strParam = text + 4;
                        numParam = 4;
                    }
                    else
                    {
                        input->SetText(nullptr);
                        return;
                    }

                    break;
                }

                case 's':
                {
                    if (text[2] == '+')
                    {
                        if (text[3] == '.')
                        {
                            strParam = text + 5;
                            numParam = 0x12;
                        }
                        else
                        {
                            strParam = text + 4;
                            numParam = 2;
                        }
                    }
                    else if (text[2] == '-')
                    {
                        strParam = text + 4;
                        numParam = 1;
                    }
                    else
                    {
                        input->SetText(nullptr);
                        return;
                    }

                    break;
                }

                default:
                {
                    input->SetText(nullptr);
                    return;
                }
            }

            AblGetDebugger()->ProcessCommand(static_cast<MCAblDebugCommand>(commandId),
                                             strParam != nullptr ? std::string_view(strParam) : std::string_view{},
                                             numParam, nullptr);
            input->SetText(nullptr);
            return;
        }

        case 'z':
        {
            AblGetDebugger()->DebugMode();
            input->SetText(nullptr);
            return;
        }

        default:
        {
            break;
        }
    }

    input->SetText(nullptr);
}

int32_t UserInit()
{
    // Port: the original switched off the screen saver, low-power and power-off timeouts (SystemParametersInfo),
    // noting in ScreenSaverActive/LowPowerActive/PowerOffActive which were on so userDestroy could restore them. SDL
    // keeps the screen saver off while its window is up.
    GlobalPane = ScreenPort->Frame();
    GlobalWindow = ScreenPort->Frame()->Window;

    if (DebugGameSystem != 0)
    {
        GameSystemWindow = new MCScrollingTextWindow;
        GameSystemWindow->Init(10, 20, 250, 300, const_cast<char*>("Game System"));
        ScreenWindow->AddChild(GameSystemWindow);
    }

    if (AblDebuggerEnabled != 0)
    {
        // iface.fit's [ABL Window] places the debugger window.
        MCFitIniFile ifaceFile;
        std::string ifaceName;
        ifaceName = GamePath(InterfacePath, "iface", ".fit");

        if (ifaceFile.Open(ifaceName) == 0)
        {
            if (ifaceFile.SeekBlock("ABL Window") == 0)
            {
                uint32_t value = 0;

                if (ifaceFile.ReadIdULong("X", value) == 0)
                {
                    AblDebuggerX = value;
                }

                if (ifaceFile.ReadIdULong("Y", value) == 0)
                {
                    AblDebuggerY = value;
                }

                if (ifaceFile.ReadIdULong("Width", value) == 0)
                {
                    AblDebuggerWidth = value;
                }

                if (ifaceFile.ReadIdULong("Height", value) == 0)
                {
                    AblDebuggerHeight = value;
                }
            }

            ifaceFile.Close();
        }

        AblDebuggerWindow = new MCAblDebuggerWindow;
        AblDebuggerWindow->Init(static_cast<int32_t>(AblDebuggerX), static_cast<int32_t>(AblDebuggerY),
                                static_cast<int32_t>(AblDebuggerWidth), static_cast<int32_t>(AblDebuggerHeight),
                                const_cast<char*>("ABL Developer Studio (tm)"));
        ScreenWindow->AddChild(AblDebuggerWindow);
        AblDebuggerWindow->Input()->SetEventRoutine(AblDebuggerEventRoutine);
    }

    if (SoundSystem == nullptr)
    {
        SoundSystem = new (std::nothrow) MCSoundSystem;

        if (SoundSystem == nullptr)
        {
            return -1;
        }

        SoundSystem->Init(const_cast<char*>("sound"));
    }

    ColorCallback = new MCGuiCallback;
    ColorCallback->SetExec(CycleColors);
    Application->AddCallback(ColorCallback);

    // Multiplayer is made to ask the session manager whether a lobby launched the game, and dropped when not. The
    // port has no lobby (MCDirectPlay), so it is always dropped here.
    MPlayer = new MCMultiPlayer;
    Assert(MPlayer != nullptr, 0, " Unable to create MultiPlayer object ");
    MPlayer->Init(0x7d000, 0x100, 100);
    LaunchedFromLobby = MPlayer->SessionManager->WasLaunchedFromLobby() != 0 ? 1 : 0;

    if (LaunchedFromLobby == 0)
    {
        delete MPlayer;
        MPlayer = nullptr;
    }

    Mission = new MCMission;
    // A game segment (-mission N on the command line) starts SYSTEM.CFG's missionName; otherwise the campaign.
    const int32_t result = Mission->Init(GlobalGameSegment == 0 ? CampaignFile : MissionName);

    if (result != 0)
    {
        Fatal(result, "Error initializing mission.");
    }

    return 0;
}

void UserDestroy()
{
    if (AblDebuggerWindow != nullptr)
    {
        AblDebuggerWindow->Destroy();
        delete AblDebuggerWindow;
        AblDebuggerWindow = nullptr;
    }

    // Port: the original put back the screen saver and power-down timeouts userInit had switched off.
    if (Mission != nullptr)
    {
        // Faithful: destroy runs twice (once more before the delete).
        Mission->Destroy();
        Mission->Destroy();
        delete Mission;
        Mission = nullptr;
    }

    if (ColorCallback != nullptr)
    {
        Application->RemoveCallback(ColorCallback);
        ColorCallback->Destroy();
        delete ColorCallback;
        ColorCallback = nullptr;
    }

    if (MPlayer != nullptr)
    {
        delete MPlayer;
        MPlayer = nullptr;
    }

    MCGameContext::Current().FastFiles().Clear();
    delete SoundSystem;
    SoundSystem = nullptr;
}
