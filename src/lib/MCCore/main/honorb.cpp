#include "stdafx.h"
#include "main/honorb.h"
#include "gameos/soundrenderer.h"
#include "gui/asystem.h"
#include "gui/updisp.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "abl/abldbug.h"
#include "abl/ablenv.h"
#include "abl/ablrtn.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "color/color.h"
#include "gui/aport.h"
#include "gui/atextbox.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/fastfile.h"
#include "lib/inifile.h"
#include "linkup/session.h"
#include "linkup/sessionmanager.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "main/logistics.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "object/objtype.h"
#include "object/warrior.h"
#include "platform/MCFileSystem.h"
#include "platform/MCInput.h"
#include "platform/MCPresenter.h"
#include "sound/soundsys.h"
#include "sprite/sprtmgr.h"
#include "terrain/terrtxm.h"

char campaignFile[20] = "campaign";
char missionName[80] = "MechCmdr1.fit";
uint32_t AblIncludeDebugInfo = 0;
uint32_t AblDebuggerEnabled = 0;
uint32_t AblDebuggerX = 0;
uint32_t AblDebuggerY = 400;
uint32_t AblDebuggerWidth = 260;
uint32_t AblDebuggerHeight = 125;
int32_t displayMode = 0;
DebuggerWindow* ABLDebuggerWindow = nullptr;
aCallback* colorCallback = nullptr;
int DebugGameSystem = 0;
int gNoSound = 0;
int ScreenSaverActive = 0;
int LowPowerActive = 0;
int PowerOffActive = 0;
int32_t languageOffset = 0;

void killTheGame()
{
    if (MPlayer != nullptr)
    {
        MultiPlayer* player = MPlayer;
        player->destroy();
        delete player;
        MPlayer = nullptr;
    }

    MouseTimerKill();
    SoundRendererUninstall();
    application->shutdownDirectDraw();
    FatalShutDown();
    std::exit(1);
}

bool checkForCDInDrive(int32_t checkDisk, bool retry)
{
    // The original scanned drives C: to Z: for a CD holding hidden.txt (and data\tiles\gtiles90.pak when
    // checkDisk is 1), pointed every path that starts with a drive letter at it, and otherwise asked the player to
    // insert the disc (or quit). The port reads everything from the install folder, so the disc is always there.
    return true;
}

namespace
{
    /// <summary>Reads a SYSTEM.CFG path (79 characters at most); a missing one is fatal.</summary>
    void readPath(FitIniFile* file, const char* varName, char* path, const char* errMessage)
    {
        const int32_t result = file->readIdString(varName, path, 0x4f);

        if (result != 0)
        {
            Fatal(result, errMessage);
        }
    }

    /// <summary>Reads a SYSTEM.CFG number; a missing one is fatal.</summary>
    void readULong(FitIniFile* file, const char* varName, uint32_t& value, const char* errMessage)
    {
        const int32_t result = file->readIdULong(varName, value);

        if (result != 0)
        {
            Fatal(result, errMessage);
        }
    }

    void readLong(FitIniFile* file, const char* varName, int32_t& value, const char* errMessage)
    {
        const int32_t result = file->readIdLong(varName, value);

        if (result != 0)
        {
            Fatal(result, errMessage);
        }
    }

    /// <summary>The "can't read system.cfg" exit.</summary>
    [[noreturn]] void closingMessage()
    {
        MCInput::ShowCursor(true);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "MechCommander Closing",
                                 "MechCommander Expansion or Editor already running.", nullptr);
        killTheGame();
    }
}

void systemInit()
{
    // Port: the original registered the "QueryCancelAutoPlay" window message (uMessage) to keep the CD's autorun from
    // starting while the game ran, and refused to run when system.cfg could not be opened exclusively (another copy
    // of the game or the editor had it). The port has no autorun and reads the file shared.
    auto* systemFile = new FitIniFile;

    if (systemFile->open("system.cfg") != 0)
    {
        closingMessage();
    }

    //---------------------------------------------------------------------------------------------------------------
    // Heaps.
    int32_t result = systemFile->seekBlock("systemHeap");

    if (result != 0)
    {
        Fatal(result, "Could not find systemHeap.  Using Defaults.");
    }

    readULong(systemFile, "systemHeapSize", systemHeapSize, "Could not find systemHeapSize.  Using Default.");
    readULong(systemFile, "guiHeapSize", guiHeapSize, "Could not find guiHeapSize.  Using Default.");
    readULong(systemFile, "logisticsHeapSize", LogisticsHeapSize, "Could not find logisticsHeapSize.  Using Default.");

    // Empty blocks switch sound and music on.
    useSound = systemFile->seekBlock("UseSound") == 0 ? 1 : 0;

    if (systemFile->seekBlock("UseMusic") == 0)
    {
        useMusic = 1;

        if (useSound == 0)
        {
            useMusic = 0;
        }
    }
    else
    {
        useSound = 0;
        useMusic = 0;
    }

    if (gNoSound != 0)
    {
        useSound = 0;
        useMusic = 0;
    }

    if (systemFile->seekBlock("DebugGameSystem") == 0)
    {
        DebugGameSystem = 1;
    }

    //---------------------------------------------------------------------------------------------------------------
    // ABL.
    if (systemFile->seekBlock("ABL") != 0)
    {
        Fatal(0, " Unable to find ABL settings. ");
    }

    readULong(systemFile, "SymbolTableHeapSize", AblSymbolTableHeapSize, "Could not find ABL SymbolTableHeapSize. ");
    readULong(systemFile, "StackHeapSize", AblStackHeapSize, "Could not find ABL StackHeapSize. ");
    readULong(systemFile, "CodeHeapSize", AblCodeHeapSize, "Could not find ABL CodeHeapSize. ");
    readULong(systemFile, "RunTimeStackSize", AblRunTimeStackSize, "Could not find ABL RunTimeStackSize. ");
    readULong(systemFile, "MaxCodeBlockSize", AblMaxCodeBlockSize, "Could not find ABL MaxCodeBlockSize. ");
    readULong(systemFile, "MaxRegisteredModules", AblMaxRegisteredModules, "Could not find ABL MaxRegisteredModules. ");
    readULong(systemFile, "MaxStaticVariables", AblMaxStaticVariables, "Could not find ABL MaxStaticVariables. ");
    readULong(systemFile, "IncludeDebugInfo", AblIncludeDebugInfo, "Could not find ABL IncludeDebugInfo. ");
    readULong(systemFile, "DebuggerEnabled", AblDebuggerEnabled, "Could not find ABL DebuggerEnabled. ");
    readLong(systemFile, "MaxWatchesPerModule", MaxWatchesPerModule, "Could not find ABL MaxWatchesPerModule. ");
    readLong(systemFile, "MaxBreakPointsPerModule", MaxBreakPointsPerModule,
             "Could not find ABL MaxBreakPointsPerModule. ");

    //---------------------------------------------------------------------------------------------------------------
    // Paths.
    result = systemFile->seekBlock("systemPaths");

    if (result != 0)
    {
        Fatal(result, "Could not find systemPaths.  Using Defaults.");
    }

    readPath(systemFile, "savePath", savePath, " Could not find save path ");
    // Copies of the game on one machine share the user folder: each keeps its temp FITs under its process ID, or a
    // multiplayer client loads the host's generated scenario (bridge.fit) or the reverse.
    std::snprintf(saveTempPath, sizeof(saveTempPath), "%stemp\\%u\\", savePath, MCPort::ProcessId());
    MCFileSystem::MakeDirectory(saveTempPath);
    readPath(systemFile, "terrainPath", terrainPath, " Could not find terrain path ");
    readPath(systemFile, "palettePath", palettePath, " Could not find palette path ");
    readPath(systemFile, "artPath", artPath, " Could not find art path ");
    readPath(systemFile, "fontPath", fontPath, " Could not find font path ");
    readPath(systemFile, "soundPath", soundPath, " Could not find sound path ");
    readPath(systemFile, "spritePath", spritePath, " Could not find sprite path ");
    readPath(systemFile, "shapesPath", shapesPath, " Could not find shapes path ");
    readPath(systemFile, "objectPath", objectPath, " Could not find object path ");
    readPath(systemFile, "missionPath", missionPath, " Could not find mission path ");
    readPath(systemFile, "warriorPath", warriorPath, " Could not find warrior path ");
    readPath(systemFile, "profilePath", profilePath, " Could not find profile path ");
    readPath(systemFile, "cameraPath", cameraPath, " Could not find camera path ");
    readPath(systemFile, "tilePath", tilePath, " Could not find tile path ");
    readPath(systemFile, "tile90Path", tile90Path, " Could not find tile90 path ");
    readPath(systemFile, "interfacePath", interfacePath, " Could not find interface path ");
    readPath(systemFile, "moviePath", moviePath, " Could not find movie path ");
    readPath(systemFile, "missionName", missionName, " Could not find Mission File Name ");
    readPath(systemFile, "CDsoundPath", CDsoundPath, " Could not find CD sound path ");
    readPath(systemFile, "CDspritePath", CDspritePath, " Could not find CD sprite path ");
    readPath(systemFile, "CDmoviePath", CDmoviePath, " Could not find CD movie path ");

    //---------------------------------------------------------------------------------------------------------------
    // Every *.fst in the game folder is a FastFile (SYSTEM.CFG's [FastFiles] list isn't read).
    const std::vector<std::string> fastFileNames = MCFileSystem::FindFiles("*.fst");
    maxFastFiles += static_cast<int32_t>(fastFileNames.size());

    if (maxFastFiles != 0)
    {
        fastFiles = static_cast<FastFile**>(std::malloc(sizeof(FastFile*) * static_cast<size_t>(maxFastFiles)));

        for (int32_t i = 0; i < maxFastFiles; i++)
        {
            fastFiles[i] = nullptr;
        }

        for (const std::string& name : fastFileNames)
        {
            FastFileInit(name.c_str());
        }
    }

    systemFile->close();
    delete systemFile;

    //---------------------------------------------------------------------------------------------------------------
    // The prefs.
    auto* prefsFile = new FitIniFile;
    result = prefsFile->open("prefs.cfg");

    if (result != 0)
    {
        Fatal(result, "Could not open prefs.cfg.");
    }

    result = prefsFile->seekBlock("MechCommander");

    if (result != 0)
    {
        Fatal(result, "Could not find MechCommander Prefs.");
    }

    if (prefsFile->readIdBoolean("PaletteCycle", application->paletteCycle) != 0)
    {
        application->paletteCycle = 0;
    }

    if (prefsFile->readIdLong("Gamma", application->gammaLevel) != 0)
    {
        application->gammaLevel = 0;
    }

    if (prefsFile->readIdBoolean("Use90Pixel", use90PixelSprite) != 0)
    {
        use90PixelSprite = 0;
    }

    if (prefsFile->readIdBoolean("Force45Pixel", only45Pixel) != 0)
    {
        only45Pixel = 0;
    }

    // One sprite size wins: 90-pixel sprites unless 45 is forced; without 90, 45 only.
    if (use90PixelSprite != 0 && only45Pixel != 0)
    {
        use90PixelSprite = 0;
    }

    if (use90PixelSprite == 0 && only45Pixel == 0)
    {
        only45Pixel = 1;
    }

    // Port: the full-size (90-pixel) mech art is always loaded and used: the camera stays at scale 100 and the zoom
    // scales the world view instead (the prefs only mattered for machines short of memory).
    use90PixelSprite = 1;
    only45Pixel = 0;

    if (prefsFile->readIdBoolean("Force32Mb", force32MB) != 0)
    {
        force32MB = 0;
    }

    if (prefsFile->readIdBoolean("Force16Mb", force16MB) != 0)
    {
        force16MB = 0;
    }
    else if (force16MB != 0 && force32MB != 0)
    {
        force32MB = 0;
    }

    int directDraw = 0;

    if (prefsFile->readIdBoolean("DirectDraw", directDraw) != 0)
    {
        directDraw = 0;
    }

    gFullScreen = directDraw != 0 ? 1 : 0;
    // Port: a port-only key; the picture keeps 4:3 with bars unless it is set.
    int stretchToFit = 0;

    if (prefsFile->readIdBoolean("StretchToFit", stretchToFit) != 0)
    {
        stretchToFit = 0;
    }

    gStretchToFit = stretchToFit != 0 ? 1 : 0;
    // Port: a port-only key; the cursor is the system's unless it is set.
    int softwareCursor = 0;

    if (prefsFile->readIdBoolean("SoftwareCursor", softwareCursor) != 0)
    {
        softwareCursor = 0;
    }

    gSoftwareCursor = softwareCursor != 0 ? 1 : 0;
    // Port: a port-only key; "vulkan" (the default) or "software". The command line's -renderer wins.
    char rendererName[32] = {};

    if (prefsFile->readIdString("Renderer", rendererName, sizeof(rendererName) - 1) == 0)
    {
        if (const std::optional<MCRendererKind> kind = MCRendererKindFromName(rendererName))
        {
            gRenderer = static_cast<int>(*kind);
        }
    }

    int32_t resolution = 0;

    // Port: the mode is still read, but the screen is the window's size (aSystem::startupDirectDraw).
    if (prefsFile->readIdLong("Resolution", resolution) == 0)
    {
        if (resolution == 1)
        {
            displayMode = 1;
            displayWidth = 800;
            displayHeight = 600;
        }
        else if (resolution == 2)
        {
            displayMode = 2;
            displayWidth = 1024;
            displayHeight = 768;
        }
        else if (resolution == 3)
        {
            displayMode = 3;
            displayWidth = 1280;
            displayHeight = 1024;
        }
    }

    if (prefsFile->readIdLong("Language", languageOffset) != 0)
    {
        languageOffset = 0;
    }

    if (prefsFile->readIdLong("Difficulty", GameDifficulty) != 0)
    {
        GameDifficulty = 1;
    }

    // Faithful: Brightness goes to the same field as Gamma, so it wins.
    if (prefsFile->readIdLong("Brightness", application->gammaLevel) != 0)
    {
        application->gammaLevel = 0;
    }

    if (prefsFile->readIdLong("MusicVolume", MusicVolume) != 0)
    {
        MusicVolume = 0x40;
    }

    if (prefsFile->readIdLong("RadioVolume", RadioVolume) != 0)
    {
        RadioVolume = 0x40;
    }

    if (prefsFile->readIdLong("SFXVolume", SFXVolume) != 0)
    {
        SFXVolume = 0x60;
    }

    prefsFile->close();
    delete prefsFile;

    checkForCDInDrive(1, false);
}

void ABLDebuggerPrintCallback(char* s)
{
}

namespace
{
    /// <summary>Set until the first event: that one points the debugger at the scenario brain.</summary>
    /// <remarks>MCX.EXE @ 0x007a5514 (starts at 1)</remarks>
    int32_t ablDebuggerFirstEvent = 1;

    /// <summary>The warrior whose index is the debugger module's id (how the "f" and "po" commands pick one).</summary>
    MechWarrior* debugModuleWarrior(Debugger* debugger)
    {
        uint32_t index = static_cast<uint32_t>(debugger->debugModule->id);

        if ((static_cast<int32_t>(index) < 1) || (scenario->numWarriors < index))
        {
            return nullptr;
        }

        return scenario->warriors[index];
    }
} // namespace

void ABLDebuggerEventRoutine(aObject* object, aEvent* event)
{
    if (ABLi_getDebugger() == nullptr)
    {
        return;
    }

    if (ablDebuggerFirstEvent != 0)
    {
        ABLi_getDebugger()->processCommand(0, nullptr, 0, scenario->scenarioBrain);
        ablDebuggerFirstEvent = 0;
    }

    // Only Enter (a key event, 10) runs the typed line.
    if ((event->type != 10) || (event->key != '\r'))
    {
        return;
    }

    aTextObject* input = static_cast<aTextObject*>(object);
    char* text = input->text;
    int32_t commandId = 0;
    char* strParam = nullptr;
    int32_t numParam = 0;

    switch (text[0])
    {
        case '?':
        {
            if (text[1] == '\0')
            {
                ABLi_getDebugger()->processCommand(9, nullptr, 0, nullptr);
                input->setText(nullptr);
                return;
            }

            if (text[1] == '?')
            {
                ABLi_getDebugger()->processCommand(10, nullptr, 0, nullptr);
                input->setText(nullptr);
                return;
            }

            break;
        }

        case 'b':
        {
            // "b+ n" / "b- n": add or remove a break point at line n.
            if (text[1] == '+')
            {
                ABLi_getDebugger()->processCommand(3, nullptr, std::atoi(text + 3), nullptr);
                input->setText(nullptr);
                return;
            }

            if (text[1] == '-')
            {
                ABLi_getDebugger()->processCommand(4, nullptr, std::atoi(text + 3), nullptr);
                input->setText(nullptr);
                return;
            }

            break;
        }

        case 'c':
        {
            ABLi_getDebugger()->processCommand(8, nullptr, 0, nullptr);
            input->setText(nullptr);
            return;
        }

        case 'f':
        {
            // "fn": the warrior's debug flags.
            MechWarrior* warrior = debugModuleWarrior(ABLi_getDebugger());

            // Port fix: the original writes through a null warrior when the module's id isn't a warrior index
            // (OB-110).
            if (warrior != nullptr)
            {
                warrior->debugFlags = static_cast<uint32_t>(std::atoi(text + 1));
            }

            input->setText(nullptr);
            return;
        }

        case 'm':
        {
            // "m n": debug module n; "m" alone: the module being executed.
            ABLModule* module = nullptr;

            if (text[1] != '\0')
            {
                module = ABLi_getModule(std::atoi(text + 2));
            }

            ABLi_getDebugger()->processCommand(0, nullptr, 0, module);
            input->setText(nullptr);
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
                        MultiPlayer* player = MPlayer;
                        delete player;
                        MPlayer = nullptr;
                        input->setText(nullptr);
                        return;
                    }

                    break;
                }

                case 'g':
                {
                    SessionManager* sessionManager = SessionManager::GetGlobalPointer(nullptr);

                    if ((sessionManager != nullptr) && (sessionManager->currentConnection != 0) &&
                        (sessionManager->isHost != 0))
                    {
                        sessionManager->StartGame();
                        input->setText(nullptr);
                        return;
                    }

                    break;
                }

                case 'h':
                {
                    SessionManager* sessionManager = SessionManager::GetGlobalPointer(nullptr);

                    if ((sessionManager != nullptr) && (sessionManager->currentConnection != 0))
                    {
                        FIDPSession session;
                        char sessionName[] = "Trooper";
                        char playerName[] = "Host";
                        char message[] = "Successfully hosted session.";
                        session.SetName(sessionName);
                        session.sessionDesc.dwMaxPlayers = 6;
                        sessionManager->HostSession(session, playerName);
                        ABLi_getDebugger()->print(message);
                        input->setText(nullptr);
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
                        MPlayer = new MultiPlayer;
                        MPlayer->init(0x7d000, 0x100, 100);
                    }

                    if (MPlayer->connectIPX() != 0)
                    {
                        ABLi_getDebugger()->print(failed);
                    }
                    else
                    {
                        ABLi_getDebugger()->print(established);
                    }

                    input->setText(nullptr);
                    return;
                }

                case 'j':
                {
                    SessionManager* sessionManager = SessionManager::GetGlobalPointer(nullptr);

                    if ((sessionManager == nullptr) || (sessionManager->currentConnection == 0))
                    {
                        break;
                    }

                    FLinkedList<FIDPSession>* sessions = sessionManager->GetSessions();

                    if (sessions->Size() == 0)
                    {
                        break;
                    }

                    // Joins the first session listed.
                    sessions->current = sessions->head;
                    FIDPSession* session = sessions->head != nullptr ? sessions->head->data : nullptr;
                    char playerName[] = "Client";
                    char message[] = "Successfully joined.";
                    sessionManager->JoinSession(&session->sessionDesc.guidInstance, playerName);
                    ABLi_getDebugger()->print(message);
                    input->setText(nullptr);
                    return;
                }

                case 'o':
                {
                    SessionManager* sessionManager = SessionManager::GetGlobalPointer(nullptr);

                    if (sessionManager != nullptr)
                    {
                        char address[] = "";
                        char message[] = "Successfully connected.";
                        sessionManager->ConnectTCP(address);
                        ABLi_getDebugger()->print(message);
                        input->setText(nullptr);
                        return;
                    }

                    break;
                }

                case 'p':
                {
                    SessionManager* sessionManager = SessionManager::GetGlobalPointer(nullptr);

                    if ((sessionManager != nullptr) && (sessionManager->currentConnection != 0))
                    {
                        sessionManager->ProcessSystemMessages();
                    }

                    break;
                }

                case 's':
                {
                    if (SessionManager::GetGlobalPointer(nullptr) == nullptr)
                    {
                        char message[] = "Created SessionManager.";
                        InitLinkUpHeap();
                        new SessionManager(MultiPlayerAppGUID);
                        ABLi_getDebugger()->print(message);
                        input->setText(nullptr);
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
                        ABLi_getDebugger()->print(message);
                        input->setText(nullptr);
                        return;
                    }

                    MPlayer->sendChat(0, text + 3);
                    input->setText(nullptr);
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
                ABLi_getDebugger()->processCommand(7, text + 2, 0, nullptr);
                input->setText(nullptr);
                return;
            }

            MechWarrior* warrior = debugModuleWarrior(ABLi_getDebugger());

            if (warrior != nullptr)
            {
                warrior->debugOrders();
                input->setText(nullptr);
                return;
            }

            break;
        }

        case 's':
        {
            // "s+" / "s-": step on or off.
            if (text[1] == '+')
            {
                ABLi_getDebugger()->processCommand(2, nullptr, 1, nullptr);
                input->setText(nullptr);
                return;
            }

            if (text[1] == '-')
            {
                ABLi_getDebugger()->processCommand(2, nullptr, 0, nullptr);
                input->setText(nullptr);
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

            ABLi_getDebugger()->processCommand(1, nullptr, numParam, nullptr);
            input->setText(nullptr);
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
                        input->setText(nullptr);
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
                        input->setText(nullptr);
                        return;
                    }

                    break;
                }

                default:
                {
                    input->setText(nullptr);
                    return;
                }
            }

            ABLi_getDebugger()->processCommand(commandId, strParam, numParam, nullptr);
            input->setText(nullptr);
            return;
        }

        case 'z':
        {
            ABLi_getDebugger()->debugMode();
            input->setText(nullptr);
            return;
        }

        default:
        {
            break;
        }
    }

    input->setText(nullptr);
}

int32_t userInit()
{
    // Port: the original switched off the screen saver, low-power and power-off timeouts (SystemParametersInfo),
    // noting in ScreenSaverActive/LowPowerActive/PowerOffActive which were on so userDestroy could restore them. SDL
    // keeps the screen saver off while its window is up.
    globalPane = screenPort->frame();
    globalWindow = screenPort->frame()->window;

    if (DebugGameSystem != 0)
    {
        GameSystemWindow = new ScrollingTextWindow;
        GameSystemWindow->init(10, 20, 250, 300, const_cast<char*>("Game System"));
        screenWindow->addChild(GameSystemWindow);
    }

    if (AblDebuggerEnabled != 0)
    {
        // iface.fit's [ABL Window] places the debugger window.
        FitIniFile ifaceFile;
        FullPathFileName ifaceName;
        ifaceName.init(interfacePath, "iface", ".fit");

        if (ifaceFile.open(ifaceName) == 0)
        {
            if (ifaceFile.seekBlock("ABL Window") == 0)
            {
                uint32_t value = 0;

                if (ifaceFile.readIdULong("X", value) == 0)
                {
                    AblDebuggerX = value;
                }

                if (ifaceFile.readIdULong("Y", value) == 0)
                {
                    AblDebuggerY = value;
                }

                if (ifaceFile.readIdULong("Width", value) == 0)
                {
                    AblDebuggerWidth = value;
                }

                if (ifaceFile.readIdULong("Height", value) == 0)
                {
                    AblDebuggerHeight = value;
                }
            }

            ifaceFile.close();
        }

        ABLDebuggerWindow = new DebuggerWindow;
        ABLDebuggerWindow->init(static_cast<int32_t>(AblDebuggerX), static_cast<int32_t>(AblDebuggerY),
                                static_cast<int32_t>(AblDebuggerWidth), static_cast<int32_t>(AblDebuggerHeight),
                                const_cast<char*>("ABL Developer Studio (tm)"));
        screenWindow->addChild(ABLDebuggerWindow);
        ABLDebuggerIn->setEventRoutine(ABLDebuggerEventRoutine);
    }

    if (soundSystem == nullptr)
    {
        soundSystem = new (std::nothrow) SoundSystem;

        if (soundSystem == nullptr)
        {
            return -1;
        }

        soundSystem->init(const_cast<char*>("sound"));
    }

    colorCallback = new aCallback;
    colorCallback->setExec(cycleColors);
    application->addCallback(colorCallback);

    // Multiplayer is made to ask the session manager whether a lobby launched the game, and dropped when not. The
    // port has no lobby (MCDirectPlay), so it is always dropped here.
    MPlayer = new MultiPlayer;
    Assert(MPlayer != nullptr, 0, " Unable to create MultiPlayer object ");
    MPlayer->init(0x7d000, 0x100, 100);
    launchedFromLobby = MPlayer->sessionManager->WasLaunchedFromLobby() != 0 ? 1 : 0;

    if (launchedFromLobby == 0)
    {
        delete MPlayer;
        MPlayer = nullptr;
    }

    mission = new Mission;
    // A game segment (-mission N on the command line) starts SYSTEM.CFG's missionName; otherwise the campaign.
    const int32_t result = mission->init(globalGameSegment == 0 ? campaignFile : missionName);

    if (result != 0)
    {
        Fatal(result, "Error initializing mission.");
    }

    return 0;
}

void userDestroy()
{
    if (ABLDebuggerWindow != nullptr)
    {
        ABLDebuggerWindow->destroy();
        delete ABLDebuggerWindow;
        ABLDebuggerWindow = nullptr;
    }

    // Port: the original put back the screen saver and power-down timeouts userInit had switched off.
    if (mission != nullptr)
    {
        // Faithful: destroy runs twice (once more before the delete).
        mission->destroy();
        mission->destroy();
        delete mission;
        mission = nullptr;
    }

    if (colorCallback != nullptr)
    {
        application->removeCallback(colorCallback);
        colorCallback->destroy();
        delete colorCallback;
        colorCallback = nullptr;
    }

    if (MPlayer != nullptr)
    {
        delete MPlayer;
        MPlayer = nullptr;
    }

    FastFileFini();
    delete soundSystem;
    soundSystem = nullptr;
}
