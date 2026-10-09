#include "stdafx.h"
#include "mission/mission.h"
#include "camera/MCCamera.h"
#include "camera/MCMainWindow.h"
#include "color/MCPalette.h"
#include "gui/abutton.h"
#include "gui/afont.h"
#include "gui/atextbox.h"
#include "gui/aport.h"
#include "gui/updisp.h"
#include "iface/icallbk.h"
#include "iface/iface.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "linkup/dpplayer.h"
#include "linkup/sessionmanager.h"
#include "logistics/invblock.h"
#include "logistics/logbri.h"
#include "logistics/logdlg.h"
#include "logistics/loggen.h"
#include "logistics/logmain.h"
#include "logistics/misslog.h"
#include "logistics/purchase.h"
#include "main/honorb.h"
#include "main/logistics.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/MCMasterComponent.h"
#include "object/MCContactSystem.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectQueue.h"
#include "object/MCObjectType.h"
#include "object/MCForces.h"
#include "object/MCMechWarrior.h"
#include "sound/soundsys.h"
#include "terrain/MCTerrain.h"
#include "terrain/MCTacticalMap.h"
#include "vfx/MCVfxFunctions.h"
#include "platform/MCFileSystem.h"
#include "platform/MCInput.h"
#include "platform/MCSmacker.h"
#include "platform/MCWin32Defs.h"
#include "object/MCObjectTypeManager.h"

uint32_t ResultsStepTicks = 20;
int32_t StevesOrderLut[4] = {SkillGunnery, SkillPiloting, SkillJumping, SkillSensors};
int32_t GlobalGameSegment = 0;
int32_t StartingResourcePoints = 0;
float MinPilotSkill = 0.0f;
SYSTEMTIME LogisticsStart{};
SYSTEMTIME LogisticsEnd{};
float MaxPilotSkill = 0.0f;
MCGuiCallback* ScenarioCallback = nullptr;
MCGuiCallback* InterfaceUpdateCallback = nullptr;
int32_t NextGameState = 0;
int GameOver = 0;
int FeatureMusicPlaying = 0;
uint32_t LastScenario = 0;
MCMission* Mission = nullptr;
int32_t PlayingLogisticsMusic = 0;
MCGuiCallback* SetupCallback = nullptr;
uint32_t ScenarioResult = 0;
int SomethingOnFire = 0;
int EventsToMissionResultsScreen = 0;
char MissionPath[80] = "data\\missions\\";
char CDmoviePath[80] = "data\\movies\\";
char MoviePath[80] = "data\\movies\\";

namespace
{
    /// <summary>
    /// An empty movie or scenario name list (the port's name): the mission heap's <c>malloc(0)</c> returned null.
    /// </summary>
    constexpr int32_t NO_RAM_FOR_MISSION_LISTS = static_cast<int32_t>(0xFACC0003);

    /// <summary>
    /// Reads the names <c>&lt;idFormat&gt;0</c>.. <c>&lt;idFormat&gt;(count-1)</c> of the current block into
    /// <paramref name="names"/> (the binary repeats this loop for the movies and scenarios of each init).
    /// </summary>
    /// <returns>0, a FIT read error, or NO_RAM_FOR_MISSION_LISTS for a count of 0.</returns>
    int32_t ReadNameList(MCFitIniFile* file, const char* idFormat, uint32_t count, std::vector<std::string>& names)
    {
        names.assign(count, std::string());

        if (count == 0)
        {
            return NO_RAM_FOR_MISSION_LISTS;
        }

        for (int32_t i = 0; i < static_cast<int32_t>(count); i++)
        {
            char id[50];
            char name[100];
            std::snprintf(id, sizeof(id), idFormat, i);
            const int32_t result = file->ReadIdString(id, name, 99);

            if (result != 0)
            {
                return result;
            }

            names[i] = name;
        }

        return 0;
    }

    /// <summary>
    /// The file name part of <paramref name="path"/> without folder or extension (the <c>fname</c> of
    /// <c>_splitpath</c>).
    /// </summary>
    void SplitFileName(const char* path, char* fileName, size_t size)
    {
        const char* start = path;

        for (const char* scan = path; *scan != 0; scan++)
        {
            if (*scan == '\\' || *scan == '/' || *scan == ':')
            {
                start = scan + 1;
            }
        }

        const char* end = std::strrchr(start, '.');

        if (end == nullptr)
        {
            end = start + std::strlen(start);
        }

        MCPort::StrCopy(fileName, std::min(size, static_cast<size_t>(end - start) + 1), start);
    }

    /// <summary>Switches a full-screen display back to 8 bits after a movie (inlined in several states of run).</summary>
    void ResetFullScreenDisplay()
    {
        if (GFullScreen != 0)
        {
            Application->ResetDirectDraw(Application->Width(), Application->Height(), 8);
        }
    }

    /// <summary>
    /// Starts a new logistics phase after a scenario: restarts the logistics clock and makes and inits
    /// <c>globalLogPtr</c> (EndScenario inlines this three times).
    /// </summary>
    MCLogistics* StartLogistics(MCMission* owner)
    {
        MCPort::GetSystemTime(LogisticsStart);
        auto* logistics = new MCLogistics;
        owner->Logistics = logistics;
        GlobalLogPtr = logistics;
        Assert(logistics != nullptr, 0, " Could not start logistics phase ");
        logistics->Init();
        return logistics;
    }

    /// <summary>Frees a logistics phase (<c>Logistics::destroy</c>, then the inlined destructor).</summary>
    void DeleteLogistics(MCLogistics* logistics)
    {
        logistics->Destroy();
        delete logistics;
    }
}

auto PlayScenario() -> void
{
    // Port: a running scenario draws on the whole window, whatever its size now.
    if (Mission != nullptr && Mission->MissionState == 7)
    {
        MCFollowWindowSize();
    }

    GlobalPane = ScreenPort->Frame();
    GlobalWindow = ScreenPort->Frame()->Window;

    if (Scenario != nullptr && ScenarioResult == 0)
    {
        ScenarioResult = static_cast<uint32_t>(Scenario->Run());
    }

    if (SoundSystem != nullptr && UseSound != 0)
    {
        SoundSystem->Update();
    }
}

auto RunMission() -> void
{
    Mission->Run();

    if (Scenario == nullptr)
    {
        FrameLength =
            static_cast<float>(static_cast<uint32_t>(PerfStopTime - PrevStart)) / static_cast<float>(CountsPerSecond);
    }
}

auto MCMission::Init(char* missionName) -> int32_t
{
    int32_t result = 0;

    if (GlobalGameSegment != 0)
    {
        //-----------------------------------------------------------------------------------------------------------
        // A game segment build: the mission FIT is the segment file itself.
        CheatsOn = 1;
        Init();
        MissionFile = new MCFitIniFile;

        if (MissionFile == nullptr)
        {
            return 3;
        }

        std::string fileName;
        fileName = GamePath(MissionPath, missionName, ".fit");
        result = MissionFile->Open(fileName);

        if (result != 0)
        {
            return result;
        }

        result = MissionFile->SeekBlock("Movies");

        if (result != 0)
        {
            return result;
        }

        result = MissionFile->ReadIdULong("NumMovies", NumMovies);

        if (result != 0)
        {
            return result;
        }

        result = ReadNameList(MissionFile, "Movie%d", NumMovies, Movies);

        if (result != 0)
        {
            return result;
        }

        if (MissionFile->ReadIdFloat("WaitTime", WaitTime) != 0)
        {
            WaitTime = 120.0f;
        }

        result = MissionFile->SeekBlock("Scenarios");

        if (result != 0)
        {
            return result;
        }

        result = MissionFile->ReadIdULong("NumScenarios", NumScenarios);

        if (result != 0)
        {
            return result;
        }

        result = ReadNameList(MissionFile, "Scenario%d", NumScenarios, Scenarios);

        if (result != 0)
        {
            return result;
        }

        return SetupNextSegment(GlobalGameSegment);
    }

    //---------------------------------------------------------------------------------------------------------------
    // The campaign: the control file names the campaign file, whose FIT holds the movies and scenarios.
    Init();
    MCPort::GetSystemTime(LogisticsStart);
    MissionFile = new MCFitIniFile;

    if (MissionFile == nullptr)
    {
        return 3;
    }

    std::string controlName;
    controlName = GamePath(MissionPath, missionName, ".fit");
    result = MissionFile->Open(controlName);

    if (result != 0)
    {
        return result;
    }

    result = MissionFile->SeekBlock("Control");

    if (result != 0)
    {
        return result;
    }

    uint32_t numCampaigns = 0;
    result = MissionFile->ReadIdULong("NumCampaigns", numCampaigns);

    if (result != 0)
    {
        return result;
    }

    // Port fix: MCX.EXE leaves the name uninitialised (and splits stack garbage) when there are 2+ campaigns.
    char campaignFile[80] = {};

    if (numCampaigns < 2)
    {
        result = MissionFile->SeekBlock("Campaign0");

        if (result != 0)
        {
            return result;
        }

        result = MissionFile->ReadIdString("CampaignFile", campaignFile, 79);

        if (result != 0)
        {
            return result;
        }
    }

    MissionFile->Close();
    delete MissionFile;
    MissionFile = nullptr;

    char campaignName[256];
    SplitFileName(campaignFile, campaignName, sizeof(campaignName));
    std::string campaignFileName;
    campaignFileName = GamePath(MissionPath, campaignName, ".fit");
    MissionFile = new MCFitIniFile;

    if (MissionFile == nullptr)
    {
        return 3;
    }

    result = MissionFile->Open(campaignFileName);

    if (result != 0)
    {
        return result;
    }

    result = MissionFile->SeekBlock("Movies");

    if (result != 0)
    {
        return result;
    }

    result = MissionFile->ReadIdULong("NumMovies", NumMovies);

    if (result != 0)
    {
        return result;
    }

    if (MissionFile->ReadIdBoolean("InDemo", InDemo) != 0)
    {
        InDemo = 0;
    }

    if (FileExists("ixtlriimceourl"))
    {
        CheatsOn = 1;
    }

    if (NumMovies == 0)
    {
        Movies.clear();
    }
    else
    {
        result = ReadNameList(MissionFile, "Movie%d", NumMovies, Movies);

        if (result != 0)
        {
            return result;
        }
    }

    if (MissionFile->ReadIdFloat("WaitTime", WaitTime) != 0)
    {
        WaitTime = 120.0f;
    }

    result = MissionFile->SeekBlock("Scenarios");

    if (result != 0)
    {
        return result;
    }

    result = MissionFile->ReadIdULong("NumScenarios", NumScenarios);

    if (result != 0)
    {
        return result;
    }

    result = MissionFile->ReadIdULong("LastScenario", LastScenario);

    if (result != 0)
    {
        return result;
    }

    result = ReadNameList(MissionFile, "Scenario%d", NumScenarios, Scenarios);

    if (result != 0)
    {
        return result;
    }

    //---------------------------------------------------------------------------------------------------------------
    // The component list, which logistics needs before any scenario has loaded it.
    if (MasterComponentList.empty())
    {
        std::string gameSystemName;
        gameSystemName = GamePath(MissionPath, "gamesys", ".fit");
        // MCX.EXE allocates this FIT (Fatal " Game System File " when out of memory) and never frees it.
        MCFitIniFile gameSystemFile;
        uint32_t readResult = static_cast<uint32_t>(gameSystemFile.Open(gameSystemName));
        Assert(readResult == 0, readResult, " Could not open GameSys.Fit file ");
        readResult = static_cast<uint32_t>(gameSystemFile.SeekBlock("General"));
        Assert(readResult == 0, readResult, " Could not find General Block in GameSys ");
        float maxVisualRange = 0.0f;
        float maxWeaponRange = 0.0f;
        float baseSensorRange = 0.0f;
        readResult = static_cast<uint32_t>(gameSystemFile.ReadIdFloat("MaxVisualRange", maxVisualRange));
        Assert(readResult == 0, readResult, " Could not find MaxVisualRange in GameSys ");
        readResult = static_cast<uint32_t>(gameSystemFile.ReadIdFloat("MaxWeaponRange", maxWeaponRange));
        Assert(readResult == 0, readResult, " Could not find MaxWeaponRange in GameSys ");
        readResult = static_cast<uint32_t>(gameSystemFile.ReadIdFloat("BaseSensorRange", baseSensorRange));
        Assert(readResult == 0, readResult, " Could not find BaseSensorRange in GameSys ");
        gameSystemFile.Close();

        std::string componentName;
        componentName = GamePath(ObjectPath, "compbas", ".csv");
        const int32_t loadResult =
            InitMasterComponentListExcel(componentName.data(), 0xff, maxVisualRange / maxWeaponRange, baseSensorRange);
        // Faithful: the assert reports the previous read's code.
        Assert(loadResult == 0, readResult, " Could not load compBas.csv ");
    }

    //---------------------------------------------------------------------------------------------------------------
    // Logistics.
    GlobalLogPtr = new MCLogistics;
    Logistics = GlobalLogPtr;
    GlobalLogPtr->Init();

    if (LaunchedFromLobby == 0)
    {
        SetupNextSegment(GlobalGameSegment);
    }
    else
    {
        MissionState = 3;
        Assert(MPlayer != nullptr, 0);

        if (MPlayer->SetupLobbyGame() != 0)
        {
            char text[256];
            CLoadString(ThisInstance, 0x370, text, 0xfe);
            MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
            dialog->SetText(text);
            dialog->SetTwoButton(0);
            dialog->OkButton->Callback()->SetExec(CancelToMPlayer);
            dialog->OkButton->SetUpPicture(const_cast<char*>("bh_okay.tga"));
            dialog->OkButton->SetDownPicture(const_cast<char*>("bg_okay.tga"));
            dialog->OkButton->Disabled = 0;
            dialog->OkButton->Draw();
            dialog->Activate();
            GlobalLogPtr->CurrentScreen = GlobalLogPtr->MainScreen;
            GlobalLogPtr->LogisticsState = 1;
            GlobalLogPtr->ShowLogScreen(0, 0);
        }
    }

    return 0;
}

auto MCMission::InitAgain(char* missionName) -> int32_t
{
    std::string fileName;
    fileName = GamePath(MissionPath, missionName, ".fit");
    // MCX.EXE keeps this FIT in a local (missionFile still holds the old one) and leaks it on every error return.
    auto* file = new MCFitIniFile;

    if (file == nullptr)
    {
        return 3;
    }

    int32_t result = file->Open(fileName);

    if (result != 0)
    {
        return result;
    }

    result = file->SeekBlock("Movies");

    if (result != 0)
    {
        return result;
    }

    result = file->ReadIdULong("NumMovies", NumMovies);

    if (result != 0)
    {
        return result;
    }

    if (NumMovies == 0)
    {
        Movies.clear();
    }
    else
    {
        result = ReadNameList(file, "Movie%d", NumMovies, Movies);

        if (result != 0)
        {
            return result;
        }
    }

    if (file->ReadIdFloat("WaitTime", WaitTime) != 0)
    {
        WaitTime = 120.0f;
    }

    result = file->SeekBlock("Scenarios");

    if (result != 0)
    {
        return result;
    }

    result = file->ReadIdULong("NumScenarios", NumScenarios);

    if (result != 0)
    {
        return result;
    }

    result = file->ReadIdULong("LastScenario", LastScenario);

    if (result != 0)
    {
        return result;
    }

    result = ReadNameList(file, "Scenario%d", NumScenarios, Scenarios);

    if (result != 0)
    {
        return result;
    }

    // Freed without close() (the File destructor closes it).
    delete file;
    return 0;
}

auto MCMission::SetupNextSegment(int32_t segment) -> int32_t
{
    char blockName[50];
    std::snprintf(blockName, sizeof(blockName), "GameSegment%d", segment);
    int32_t result = MissionFile->SeekBlock(blockName);

    if (result != 0)
    {
        return result;
    }

    result = MissionFile->ReadIdULong("GameState", reinterpret_cast<uint32_t&>(MissionState));

    if (result != 0)
    {
        return result;
    }

    result = MissionFile->ReadIdULong("SmackerMovieId", reinterpret_cast<uint32_t&>(CurrentMovie));

    if (result != 0)
    {
        return result;
    }

    result = MissionFile->ReadIdULong("ScenarioId", reinterpret_cast<uint32_t&>(CurrentScenario));

    if (result != 0)
    {
        return result;
    }

    result = MissionFile->ReadIdULong("NextGameState", reinterpret_cast<uint32_t&>(NextGameState));

    if (result != 0)
    {
        return result;
    }

    if (GlobalGameSegment == 0)
    {
        Logistics->GetCurrentMission();
        return 0;
    }

    MissionState = 11;
    return 0;
}

auto MCMission::Init() -> int32_t
{
    MissionState = 0;
    ResultsScreen = nullptr;
    MissionFile = nullptr;
    Logistics = nullptr;

    if (MissionCallback == nullptr)
    {
        MissionCallback = new MCGuiCallback;
        MissionCallback->SetExec(RunMission);
        Application->AddCallback(MissionCallback);
    }

    return 0;
}

auto MCMission::Destroy() -> void
{
    Movies.clear();
    Scenarios.clear();

    if (MissionFile != nullptr)
    {
        MissionFile->Close();
        delete MissionFile;
        MissionFile = nullptr;
    }

    if (ScenarioCallback != nullptr)
    {
        Application->RemoveCallback(ScenarioCallback);
        delete ScenarioCallback;
        ScenarioCallback = nullptr;
    }

    if (InterfaceUpdateCallback != nullptr)
    {
        Application->RemoveCallback(InterfaceUpdateCallback);
        delete InterfaceUpdateCallback;
        InterfaceUpdateCallback = nullptr;
    }

    if (Scenario != nullptr)
    {
        SaveWindowStatus();
        Scenario->Destroy();
        delete Scenario;
        Scenario = nullptr;
    }

    if (MissionCallback != nullptr)
    {
        Application->RemoveCallback(MissionCallback);
        delete MissionCallback;
        MissionCallback = nullptr;
    }

    if (GlobalLogPtr != nullptr)
    {
        if (Logistics != nullptr)
        {
            DeleteLogistics(Logistics);
        }

        Logistics = nullptr;
        GlobalLogPtr = nullptr;
    }

    MissionState = 0;
}

auto MCMission::Run() -> int32_t
{
    KeepScreenBlack = 0;
    // The states that end up back in logistics share the tails of the binary's switch; these flags stand in for its
    // two jump targets (reset the display first, or not).
    bool resetDisplay = false;
    bool toLogistics = false;

    switch (MissionState)
    {
        case 0:
        {
            if (CurrentMovie == -1)
            {
                break;
            }

            if (NextGameState == 10)
            {
                MissionState = 10;
                break;
            }

            if (GFullScreen != 0)
            {
                Application->ResetDirectDraw(Application->Width(), Application->Height(), 8);
                Application->PaletteCycle = 1;
                Application->ActivatePalette(GamePalette()->RgbData.data(), 0, 0x100);
            }

            toLogistics = true;
            break;
        }

        case 3:
        {
            if (Logistics != nullptr && Logistics->CurrentScreen->IsShowing() == 0)
            {
                Logistics->ShowLogScreen(1, 0);
            }

            EscapedSmackerMovie = 0;
            MovieOver = 0;
            Application->SetCursorVisible(1);

            if (SoundSystem != nullptr)
            {
                if (Logistics == nullptr)
                {
                    break;
                }

                const bool onMainScreen = Logistics->CurrentScreen == Logistics->MainScreen;

                if (onMainScreen && PlayingLogisticsMusic != 1)
                {
                    SoundSystem->PlayDigitalMusic(0x17, true);
                    PlayingLogisticsMusic = 1;
                }

                if (!onMainScreen && PlayingLogisticsMusic != 2)
                {
                    SoundSystem->PlayDigitalMusic(0x16, true);
                    PlayingLogisticsMusic = 2;
                }
            }

            if (Logistics != nullptr && MPlayer != nullptr)
            {
                MPlayer->ProcessReceiveList();
            }
            break;
        }

        case 6:
        {
            if (MPlayer != nullptr && MPlayer->SessionManager != nullptr)
            {
                MPlayer->ProcessReceiveList();
            }
            break;
        }

        case 7:
        {
            if ((ScenarioResult != 0 || EndScenarioRequested != 0) && Scenario->StartingUp == 0)
            {
                Scenario->SetupBonus();
                ResultsScreen->Activate();
                MissionState = 6;
                PlayingLogisticsMusic = 0;
            }
            break;
        }

        case 8:
        {
            if (Application->SmackerWindow != nullptr)
            {
                break;
            }

            if (NextGameState == 10)
            {
                MissionState = 10;
                CurrentMovie++;

                if (CurrentMovie == 2)
                {
                    NextGameState = 3;
                }
                else if (CurrentMovie == 5)
                {
                    NextGameState = 16;
                    MissionState = 16;
                }
                else
                {
                    NextGameState = 10;
                }
                break;
            }

            if (GameOver != 0)
            {
                MissionState = 16;
                break;
            }

            resetDisplay = true;
            break;
        }

        case 10:
        {
            const int32_t movie = CurrentMovie;

            if (movie == -1)
            {
                break;
            }

            if (EscapedSmackerMovie == 0)
            {
                if (SoundSystem != nullptr)
                {
                    SoundSystem->StopDigitalMusic();
                }

                std::string movieName;
                movieName = GamePath(CDmoviePath, Movies[movie].c_str(), ".smk");
                // MCX.EXE: when movie 1 (the opening) isn't installed, it scans drives C: to Z: for a CD-ROM holding
                // \data\movies\opening.smk. The port reads movies from the install only.
                Application->StartSmackerMovie(movieName.data(), 0xfe000, nullptr, 1);
                MissionState = 8;
                PlayingLogisticsMusic = 0;
                break;
            }

            EscapedSmackerMovie = 0;

            if (GameOver != 0)
            {
                MissionState = 16;
                break;
            }

            ResetFullScreenDisplay();
            PlayingLogisticsMusic = 0;
            toLogistics = true;
            break;
        }

        case 11:
        {
            if (CurrentScenario != -1)
            {
                if (SoundSystem != nullptr)
                {
                    SoundSystem->StopDigitalMusic();
                }

                char* scenarioName = (MPlayer == nullptr || GlobalLogPtr == nullptr) ? Scenarios[CurrentScenario].data()
                                                                                     : GlobalLogPtr->MpMissionName;
                StartScenario(scenarioName);
                Application->SetCursorVisible(1);
            }
            break;
        }

        case 13:
        {
            if (Application->SmackerWindow2 != nullptr)
            {
                break;
            }

            if (NextGameState == 10)
            {
                if (CurrentMovie++ != 0)
                {
                    MissionState = 10;
                    NextGameState = 3;
                    break;
                }

                MissionState = 12;
                NextGameState = 10;
                break;
            }

            if (InDemo != 0 && CurrentMovie == 2)
            {
                ResetFullScreenDisplay();
                MissionState = 20;
                break;
            }

            if (GameOver != 0 && CurrentMovie == 3)
            {
                MissionState = 16;
                break;
            }

            resetDisplay = true;
            break;
        }

        case 16:
            MCInput::PostMessage(WM_CLOSE, 0, 0);
            break;

        case 20:
        {
            if (FeatureScreen != nullptr)
            {
                if (FeatureMusicPlaying == 0)
                {
                    SoundSystem->PlayDigitalMusic(8, false);
                    FeatureMusicPlaying = 1;
                }
            }

            if (FeatureScreen == nullptr)
            {
                //-------------------------------------------------------------------------------------------------------
                // The demo's feature screen: features.tga with the palette from its own colour map.
                MCFile pictureFile;
                char fileName[256];
                char message[256];
                std::snprintf(fileName, sizeof(fileName), "%s%s", ArtPath, "features.tga");

                if (pictureFile.Open(fileName) != 0)
                {
                    MCStrCopy(fileName, "features.tga");

                    if (pictureFile.Open(fileName) != 0)
                    {
                        std::snprintf(message, sizeof(message), "Error reading '%s'", fileName);
                        GeneralMsg(message);
                    }
                }

                const uint32_t size = pictureFile.FileSize();

                if (size == 0)
                {
                    std::snprintf(message, sizeof(message), "Error reading '%s'", fileName);
                    GeneralMsg(message);
                }

                if (size == 0)
                {
                    return 0;
                }

                std::vector<uint8_t> picture(size);
                pictureFile.Read(picture.data(), static_cast<int32_t>(size));
                pictureFile.Close();
                // The TGA's colour map starts at +0x12, as 256 BGR triples of 8-bit components.
                std::array<uint8_t, 0x300> palette = {};

                for (int32_t i = 0; i < 0x100; i++)
                {
                    palette[i * 3 + 0] = picture[0x12 + i * 3 + 2] >> 2;
                    palette[i * 3 + 1] = picture[0x12 + i * 3 + 1] >> 2;
                    palette[i * 3 + 2] = picture[0x12 + i * 3 + 0] >> 2;
                }

                Application->SetCursorVisible(0);
                FeatureScreen = new MCGuiObject;
                FeatureScreen->Init(0, 0, 640, 480, nullptr);
                // The picture's port is never freed in MCX.EXE.
                auto* picturePort = new MCGuiPort;
                picturePort->Init(const_cast<char*>("features.tga"));
                picturePort->CopyTo(FeatureScreen->Port()->Frame(), 0, 0, 0);
                ScreenWindow->AddChild(FeatureScreen);
                FeatureScreen->ShowGuiWindow(1);
                Application->ActivatePalette(palette.data(), 0, 0x100);
            }

            if (FeatureScreenDone != 0)
            {
                ScreenWindow->RemoveChild(FeatureScreen);
                delete FeatureScreen;
                FeatureScreen = nullptr;
                MissionState = 16;
            }
            break;
        }
    }

    if (resetDisplay)
    {
        ResetFullScreenDisplay();
        toLogistics = true;
    }

    if (toLogistics)
    {
        MissionState = 3;

        if (GlobalGameSegment == 0)
        {
            Logistics->GetCurrentMission();
        }
    }

    if (SoundSystem != nullptr && (Scenario == nullptr || EventsToMissionResultsScreen != 0))
    {
        SoundSystem->Update();
    }

    return 0;
}

auto MCMission::StartScenario(char* scenarioName) -> void
{
    if (Solo == 0 && MPlayer == nullptr)
    {
        CheckForCDInDrive(CurPlanet, false);
    }

    SoundSystem->PlayBettySample(0x13);
    EndScenarioRequested = 0;
    ResultsScreen = new MCMissionResultsScreen;
    ResultsScreen->Init();

    if (GlobalGameSegment == 0)
    {
        GlobalLogPtr->PrepareScenario(scenarioName, const_cast<char*>("bridge"));
    }

    if (Logistics != nullptr)
    {
        DeleteLogistics(Logistics);
        GlobalLogPtr = nullptr;
        Logistics = nullptr;
    }

    //---------------------------------------------------------------------------------------------------------------
    // How long logistics took: the time of day of (end - start) as a FILETIME, so whole days are dropped.
    MCPort::GetSystemTime(LogisticsEnd);
    const uint64_t startTicks = MCPort::SystemTimeToFileTime(LogisticsStart);
    const uint64_t endTicks = MCPort::SystemTimeToFileTime(LogisticsEnd);
    SYSTEMTIME elapsed{};
    MCPort::FileTimeToSystemTime(endTicks - startTicks, elapsed);
    TotalLogisticsTime = static_cast<float>(elapsed.wMinute) * 60.0f + static_cast<float>(elapsed.wHour) * 3600.0f +
                         static_cast<float>(elapsed.wSecond) + static_cast<float>(elapsed.wMilliseconds / 1000);

    GamePalette()->Activate();
    InitAlphaLookup(GamePalette()->Colors());
    Application->PaletteCycle = 1;
    Application->SetCursorVisible(0);

    ScenarioCallback = new MCGuiCallback;

    if (ScenarioCallback == nullptr)
    {
        Fatal(0, " No RAM for scenario Callback");
    }

    Scenario = new MCScenario;

    if (Scenario == nullptr)
    {
        Fatal(0, " No RAM for scenario");
    }

    if (GlobalGameSegment == 0)
    {
        scenarioName = const_cast<char*>("bridge");
    }

    // Port: the scenario's windows are made at the window's size.
    MCFollowWindowSize();
    const int32_t result = Scenario->Init(scenarioName, nullptr);

    if (result != 0)
    {
        Fatal(result, " Couldnt init scenario ");
    }

    TheInterface->StartScenario();
    LoadWindowStatus();
    StartingResourcePoints = ResourcePoints;
    ScenarioResult = 0;
    MissionState = 7;
    WaitTimer = WaitTime;
    ARedrawScreen();
    ScenarioCallback->SetExec(PlayScenario);
    Application->AddCallback(ScenarioCallback);

    if (InterfaceUpdateCallback == nullptr)
    {
        InterfaceUpdateCallback = new MCGuiCallback;

        if (InterfaceUpdateCallback == nullptr)
        {
            Fatal(0, " No RAM for interface Callback");
        }
    }

    InterfaceUpdateCallback->SetExec(UpdateMouseStateCallback);
    Application->AddCallback(InterfaceUpdateCallback);
}

auto MCMission::EndScenario() -> void
{
    TotalScenarioTime = ScenarioTime;

    // MCX.EXE also copies the scenario's script name to an unused local when playing solo (after the CD check).
    if (Solo == 0 && MPlayer == nullptr)
    {
        CheckForCDInDrive(CurPlanet, false);
    }

    if (GlobalGameSegment == 0 && MPlayer == nullptr)
    {
        char bridgeName[256];
        std::snprintf(bridgeName, sizeof(bridgeName), "%s", Scenario->ScenarioScript);
        MCMissionLogisticsBridge bridge;
        const int32_t result = bridge.MissionResultsStartingFitWriter(bridgeName);

        if (result != 0)
        {
            Fatal(result, " Unable to write Mission to Logistics Bridge File ");
        }
    }

    EndScenarioRequested = 0;

    if (SetupCallback != nullptr)
    {
        delete SetupCallback;
        SetupCallback = nullptr;
    }

    Application->RemoveCallback(ScenarioCallback);
    delete ScenarioCallback;
    ScenarioCallback = nullptr;
    TheInterface->EndScenario();
    SomethingOnFire = 0;

    if (SoundSystem != nullptr)
    {
        SoundSystem->PurgeSoundSystem();
    }

    // The result is thrown away (the results screen already counted the points).
    Scenario->CalcResourcePointsEarned();

    if (Scenario != nullptr)
    {
        Scenario->Destroy();
        delete Scenario;
        Scenario = nullptr;
    }

    if (ScenarioResult > 3)
    {
        if (Solo != 0)
        {
            // A won solo (quick-start) mission: back to the main screen.
            MCLogistics* newLogistics = StartLogistics(this);
            LastLogisticsMissionState = 0;
            MissionState = 3;
            newLogistics->CurrentScreen->ShowGuiWindow(0);
            newLogistics->CurrentScreen = newLogistics->MainScreen;
            newLogistics->LogisticsState = 1;
            newLogistics->ShowLogScreen(1, 1);
            Solo = 0;
            return;
        }

        if (static_cast<uint32_t>(CurrentScenario) == LastScenario)
        {
            GameOver = 1;
        }
    }

    if (ScenarioResult < 3 && Solo != 0)
    {
        // A lost solo mission: restart the campaign at its first briefing.
        MCLogistics* newLogistics = StartLogistics(this);
        LastLogisticsMissionState = 0;
        MissionState = 3;
        Solo = 1;
        newLogistics->CurrentMission = 0;
        CurrentScenario = 0;
        CurrentMovie = 1;
        char startName[] = "start1";
        char extension[] = ".pkk";
        newLogistics->LoadCampaign(startName, extension, 1, 0);
        newLogistics->SetUpBriefingScreen(0);
        newLogistics->ShowLogScreen(1, 0);
        return;
    }

    if (GlobalGameSegment == 0 && GameOver == 0)
    {
        MCLogistics* newLogistics = StartLogistics(this);

        if (MPlayer == nullptr)
        {
            // The campaign: a win moves on to the next mission, anything else replays this one.
            int32_t missionId = CurrentScenario;

            if (ScenarioResult < 4)
            {
                newLogistics->CurrentMission = missionId;
            }
            else
            {
                missionId++;
                newLogistics->CurrentMission = missionId;
                CurrentScenario = missionId;
            }

            CurrentMovie = missionId + 1;
            newLogistics->GetCurrentMission();
            const bool replay = ScenarioResult < 4;
            char startName[32];
            std::snprintf(startName, sizeof(startName), "start%d",
                          replay ? newLogistics->CurrentMission + 1 : newLogistics->CurrentMission);
            char extension[] = ".pkk";
            newLogistics->LoadCampaign(startName, extension, replay ? 1 : 0, 0);
            newLogistics->SetUpBriefingScreen(0);
            newLogistics->ShowLogScreen(1, 0);
            return;
        }

        LastLogisticsMissionState = 0;
        MPlayer->ChatCallback = LogisticsChatCallback;
        MissionState = 3;
        newLogistics->CurrentScreen->ShowGuiWindow(0);
        newLogistics->CurrentScreen = newLogistics->MainScreen;
        newLogistics->LogisticsState = 1;
        newLogistics->ShowLogScreen(1, 1);

        if (MPlayer->InMission != 0)
        {
            if (IsMPlayerGame != 0)
            {
                KillTheGame();
            }

            delete MPlayer;
            MPlayer = nullptr;
        }
    }
    else
    {
        MissionState = 16;
        WaitTimer = WaitTime;
        CurrentScenario = -1;
    }
}

auto MCMission::SaveWindowStatus() -> void
{
    MCFitIniFile windowFile;
    // windows.tmp in the original: under the process ID, as copies of the game on one machine share the user folder
    // and leave a multiplayer mission together.
    char tempName[32];
    std::snprintf(tempName, sizeof(tempName), "windows.%u.tmp", MCPort::ProcessId());

    if (windowFile.Open(tempName, MCFileMode::Create) != 0)
    {
        return;
    }

    windowFile.WriteBlock("Info");

    if (MainHolder()->GetPane(0) != nullptr && MainHolder()->GetPane(0)->GetCamera() != nullptr)
    {
        windowFile.WriteIdBoolean("MainZoomed", MainHolder()->GetPane(0)->GetCamera()->CameraScale != 100);
    }

    windowFile.WriteIdBoolean("TacHidden", TheInterface->TacticalMap->IsHidden());
    windowFile.WriteIdBoolean("ShowPalette", TheInterface->TacticalMap->PaletteFrame->IsShowing());
    windowFile.Close();
    MCFileSystem::RemoveFile("windows.fit");
    MCFileSystem::RenameFile(tempName, "windows.fit");
}

auto MCMission::LoadWindowStatus() -> void
{
    MCFitIniFile windowFile;
    MainHolder()->SetTiled(0);
    TheInterface->TacticalMap->ShowGuiWindow(1);
    MainHolder()->ZoomActivePane();
    TheInterface->TacticalMap->HideMe(0);

    if (windowFile.Open("windows.fit") != 0)
    {
        return;
    }

    if (windowFile.SeekBlock("Info") == 0)
    {
        int showPalette = 0;

        if (windowFile.ReadIdBoolean("ShowPalette", showPalette) != 0)
        {
            showPalette = 1;
        }

        TheInterface->TacticalMap->PaletteFrame->ShowGuiWindow(showPalette);
    }

    windowFile.Close();
}

auto MCGuiOpeningSmackerWindow::Init(tagRECT* frame, tagPOINT* position) -> int32_t
{
    WipeLine = 0;
    Application->OpeningSmackerWindow = this;
    return MCGuiSmackerWindow::Init(frame, position);
}

auto MCGuiOpeningSmackerWindow::Display() -> void
{
    if (ShowWindow == 0 || (IsHidden() != 0 && HideOffset == 0))
    {
        return;
    }

    if (WipeLine < 1)
    {
        MCGuiSmackerWindow::Display();
    }
    else
    {
        // (The original copied the frame onto the screen wipeLine lines down; draw does it.)
        DrawInFramePass(DisplayPort);
        WipeLine += 20;
    }

    if (Height() + 20 <= WipeLine)
    {
        Destroy();
        delete this;
    }
}

auto MCGuiOpeningSmackerWindow::Draw() -> void
{
    if (WipeLine < 1)
    {
        MCGuiSmackerWindow::Draw();
        return;
    }

    if (MoviePane != nullptr && MoviePane->Window != nullptr && MoviePane->Window->Buffer != nullptr)
    {
        VfxPaneCopy(MoviePane, 0, 0, Port()->Frame(), 0, WipeLine, -1);
    }
}

auto MCGuiOpeningSmackerWindow::EndSmackerMovie() -> void
{
    SmackClose(Movie);
    Movie = nullptr;
    WipeLine++;
    Application->OpeningSmackerWindow = nullptr;
    // Port: the lines above the sliding frame show what is under the window (the original didn't touch them); movie
    // pixels of the key colour 0xff show through too.
    Transparent = 1;
}

auto MCGuiOpeningSmackerWindow::EscapeSmackerMovie() -> void
{
    WipeLine += Height();
    EndSmackerMovie();
    Application->SmackerWindow->Destroy();

    if (Application->SmackerWindow != nullptr)
    {
        delete Application->SmackerWindow;
    }

    Application->SmackerWindow = nullptr;
}

auto ComparePilots(const void* a, const void* b) -> int
{
    const int32_t keyA = static_cast<const MCMissionPilotResult*>(a)->SortKey;
    const int32_t keyB = static_cast<const MCMissionPilotResult*>(b)->SortKey;

    if (keyA == keyB)
    {
        return 0;
    }

    if (keyB < keyA)
    {
        return 1;
    }

    return -1;
}

auto CompareCommanders(const void* a, const void* b) -> int
{
    const int32_t valueA = static_cast<const MCMissionCommanderScore*>(a)->Score;
    const int32_t valueB = static_cast<const MCMissionCommanderScore*>(b)->Score;

    if (valueA == valueB)
    {
        return 0;
    }

    if (valueA < valueB)
    {
        return 1;
    }

    return -1;
}

auto MoveOnButtonHandleEvent(MCGuiObject*, MCGuiEvent* event) -> void
{
    if (event->Type == 4)
    {
        MCMissionResultsScreen* screen = Mission->ResultsScreen;

        if (screen != nullptr)
        {
            screen->Destroy();
            delete screen;
            Mission->ResultsScreen = nullptr;
        }
    }
}

auto PilotSwitchHandleEvent(MCGuiObject* object, MCGuiEvent* event) -> void
{
    if (event->Type == 1)
    {
        Mission->ResultsScreen->DrawMPPilots(static_cast<MCGuiToolButton*>(object)->Pushed == 0);
    }
}

MCMissionResultsScreen::~MCMissionResultsScreen()
{
    Destroy();
}

namespace
{
    /// <summary>Removes <paramref name="child"/> from <paramref name="parent"/>, then destroys and deletes it.</summary>
    template <typename T> auto DeleteChild(MCGuiObject* parent, T*& child) -> void
    {
        if (child == nullptr)
        {
            return;
        }

        parent->RemoveChild(child);
        child->Destroy();
        delete child;
        child = nullptr;
    }

    /// <summary>Destroys and deletes <paramref name="port"/>.</summary>
    auto DeletePort(MCGuiPort*& port) -> void
    {
        if (port == nullptr)
        {
            return;
        }

        port->Destroy();
        delete port;
        port = nullptr;
    }

    /// <summary>
    /// A picture of <paramref name="warrior"/>'s icon without its 2-pixel frame: the pilot's own portrait for a pilot
    /// with status 3, whose icon shows the dead image. (The original redrew the icon's own picture with the portrait
    /// swapped in, moved its pane's edges in by 2 and copied it onto the window; the icon draws itself through a view
    /// now, so it is drawn into a picture, which the screen copies each frame.)
    /// </summary>
    /// <returns>The picture, or null when the pilot's mover has no icon.</returns>
    auto TakeIconPicture(MCMechWarrior* warrior, int32_t partId) -> MCGuiPort*
    {
        MCFriendlyMechIcon* icon = TheInterface->GetMechIconFromID(partId);

        if (icon == nullptr)
        {
            return nullptr;
        }

        const int32_t status = warrior->Status;

        if (status == 3)
        {
            icon->PilotImage->Init(warrior->Picture.data());
        }

        auto* picture = new MCGuiPort;
        picture->Init(icon->Port()->Width(), icon->Port()->Height());
        icon->UpdateModel();
        icon->DrawIcon(picture);
        MCPane* pane = picture->Frame();
        pane->X0 += 2;
        pane->Y0 += 2;
        pane->X1 -= 2;
        pane->Y1 -= 2;

        if (status == 3)
        {
            icon->PilotImage->Init(4);
        }

        return picture;
    }

    /// <summary>Copies an icon picture from <see cref="TakeIconPicture"/> to (<paramref name="x"/>, <paramref name="y"/>) of <paramref name="target"/>.</summary>
    auto DrawIconPicture(MCGuiPort* picture, MCPane* target, int32_t x, int32_t y) -> void
    {
        if (picture != nullptr)
        {
            picture->CopyTo(target, x, y, 0);
        }
    }

    /// <summary>The left and top of single-player pilot line <paramref name="index"/>'s box.</summary>
    auto PilotBox(int32_t index, int32_t& left, int32_t& top) -> void
    {
        if (index < 6)
        {
            left = 0xe8;
            top = index * 0x42 + 0x2c;
        }
        else
        {
            left = 0x18a;
            top = index * 0x42 - 0x160;
        }
    }

    /// <summary>The length in pixels of a skill bar for <paramref name="skill"/> (55 across the skill range).</summary>
    auto SkillBarLength(int32_t skill) -> int32_t
    {
        const int32_t aboveMinimum = static_cast<int32_t>(skill - static_cast<double>(MinPilotSkill));
        return static_cast<int32_t>(static_cast<double>(aboveMinimum * 55) /
                                    (static_cast<double>(MaxPilotSkill) - MinPilotSkill));
    }

    /// <summary>Whether the home side lost a multiplayer game with result <paramref name="result"/>.</summary>
    auto HomeSideLost(uint32_t result) -> bool
    {
        return result == 3 || (result == 1 && HomeTeam()->Alignment == -1) ||
               (result == 2 && HomeTeam()->Alignment == 1);
    }

    /// <summary>The kills of <paramref name="warrior"/>, all kinds together.</summary>
    auto TotalKills(const MCMechWarrior* warrior) -> int32_t
    {
        int32_t kills = 0;

        for (int32_t kind = 0; kind < 7; kind++)
        {
            kills += warrior->NumKilled[kind][1];
        }

        return kills;
    }
}

auto MCMissionResultsScreen::Init() -> int32_t
{
    int32_t result = MCGuiObject::Init(0x28, 0xf, 0x230, 0x1bc, nullptr);
    Assert(result == 0, static_cast<uint32_t>(result), " error initializing mission results screen display elements ");
    char* backgroundName = MPlayer == nullptr ? const_cast<char*>("mr_bkgd.tga") : const_cast<char*>("mrm_bkgd.tga");
    // (The original loaded the art into the window's own picture.)
    result = SetBackground(backgroundName);
    Assert(result == 0, static_cast<uint32_t>(result), " error initializing mission results screen display elements ");

    _MoveOnPort = new MCGuiPort();
    Assert(_MoveOnPort != nullptr, static_cast<uint32_t>(result), " not enough memory to init mission results screen ");
    // The original drops this init's result and asserts the previous one again.
    _MoveOnPort->Init(const_cast<char*>("mr_ms00.tga"));
    Assert(result == 0, static_cast<uint32_t>(result), " error initializing mission results screen display elements ");

    _MoveOnButton = new MCGuiButton();
    Assert(_MoveOnButton != nullptr, static_cast<uint32_t>(result),
           " not enough memory to init mission results screen ");
    _MoveOnButton->Init(0x1bc, 6, 0x6b, 0x12, nullptr);
    _MoveOnButton->SetUpPicture(const_cast<char*>("mr_ms01.tga"));
    _MoveOnButton->SetDownPicture(const_cast<char*>("mr_ms02.tga"));
    AddChild(_MoveOnButton);
    _MoveOnButton->SetEventRoutine(MoveOnButtonHandleEvent);
    _MoveOnButton->SetTransparent(1);

    if (MPlayer != nullptr)
    {
        auto* switchButton = new MCGuiToolButton();
        _PilotSwitchButton = switchButton;
        Assert(switchButton != nullptr, static_cast<uint32_t>(result),
               " not enough memory to init mission results screen ");
        switchButton->Init(0xe4, 0x1e, 0x148, 0xb, nullptr);
        switchButton->SetUpPicture(const_cast<char*>("mrm_bkgd00.tga"));
        switchButton->SetDownPicture(const_cast<char*>("mrm_bkgd01.tga"));
        switchButton->Framed = 0;
        switchButton->Draw();
        switchButton->Draw();
        AddChild(switchButton);
        _PilotSwitchButton->SetEventRoutine(PilotSwitchHandleEvent);
    }

    _SuccessPort = new MCGuiPort();
    Assert(_SuccessPort != nullptr, static_cast<uint32_t>(result),
           " not enough memory to init mission results screen ");
    _FailurePort = new MCGuiPort();
    Assert(_FailurePort != nullptr, static_cast<uint32_t>(result),
           " not enough memory to init mission results screen ");
    result = _SuccessPort->Init(const_cast<char*>("guimr08.tga"));
    Assert(result == 0, static_cast<uint32_t>(result), " error initializing mission results screen display elements ");
    result = _FailurePort->Init(const_cast<char*>("guimr07.tga"));
    Assert(result == 0, static_cast<uint32_t>(result), " error initializing mission results screen display elements ");

    _NextDrawTime = 0;

    if (MPlayer == nullptr)
    {
        // (The original loaded the pictures into the objects' own ports.)
        _ScrollUpButton = new MCGuiObject();
        _ScrollUpButton->SetDrawsLive();
        _ScrollUpButton->Init(0, 0, 0xb, 0xb, nullptr);
        _ScrollUpButton->SetBackground(const_cast<char*>("mfddbg04.tga"));
        _ScrollUpButton->ShowGuiWindow(0);
        AddChild(_ScrollUpButton);

        _ScrollDownButton = new MCGuiObject();
        _ScrollDownButton->SetDrawsLive();
        _ScrollDownButton->Init(0, 0, 0xb, 0xb, nullptr);
        _ScrollDownButton->SetBackground(const_cast<char*>("mfddbg05.tga"));
        _ScrollDownButton->ShowGuiWindow(0);
        AddChild(_ScrollDownButton);

        _ScrollUpRect = {0xcf, 0x15b, 0xda, 0x166};
        _ScrollUpButton->MoveTo(0xcf, 0x15b, 0);
        _ScrollDownRect = {0xcf, 0x1a7, 0xda, 0x1b2};
        _ScrollDownButton->MoveTo(0xcf, 0x1a7, 0);
        _ScrollBarRect = {0xcf, 0x169, 0xda, 0x1a4};
    }

    return result;
}

auto MCMissionResultsScreen::Destroy() -> void
{
    EventsToMissionResultsScreen = 0;
    _PilotResults.reset();
    DeletePort(_SuccessPort);
    DeletePort(_FailurePort);
    DeletePort(_MoveOnPort);
    DeletePort(_BestPilotIcon);

    for (MCGuiPort*& picture : _PilotIcons)
    {
        DeletePort(picture);
    }

    _PilotIcons.clear();
    _CommanderLines.clear();
    DeleteChild(this, _MoveOnButton);
    DeleteChild(this, _ScrollUpButton);
    DeleteChild(this, _ScrollDownButton);
    DeleteChild(this, _PilotSwitchButton);
    // The debriefing text box belongs to the tactical map; it is only taken off the screen.
    RemoveChild(_TextObject);
    MCGuiObject::Destroy();

    Application->CursorHidden = 0;

    if (_ScenarioEnded == 0 && Scenario != nullptr)
    {
        _ScenarioEnded = 1;
        Mission->EndScenario();

        if (GlobalGameSegment == 0)
        {
            if (GameOver == 0)
            {
                GamePaused = 0;
                Mission->MissionState = 3;
                return;
            }

            if (InDemo != 0)
            {
                GamePaused = 0;
                Mission->CurrentMovie = 2;
                Mission->MissionState = 0x14;
                NextGameState = 0x14;
                return;
            }

            Mission->CurrentMovie = 3;
            Mission->MissionState = 10;
            NextGameState = 10;
        }

        GamePaused = 0;
    }
}

auto MCMissionResultsScreen::MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) -> bool
{
    if (MPlayer != nullptr || _TextObject == nullptr)
    {
        return false;
    }

    return _TextObject->MouseWheel(steps, xPos, yPos);
}

auto MCMissionResultsScreen::HandleEvent(MCGuiEvent* event) -> void
{
    MCGuiObject::HandleEvent(event);

    switch (event->Type)
    {
        case 1:
        {
            if (MPlayer == nullptr)
            {
                const POINT point{event->X - GlobalX(), event->Y - GlobalY()};

                if (PtInRect(&_ScrollUpRect, point))
                {
                    Application->Grab(this);
                    _ScrollUpButton->ShowGuiWindow(1);
                    Application->AddTimer(this, 4, TheInterface->ScrollStart, 0, 0, 0);
                    _TextObject->ReceiveClick(-1, 0);
                    return;
                }

                if (PtInRect(&_ScrollDownRect, point))
                {
                    Application->Grab(this);
                    _ScrollDownButton->ShowGuiWindow(1);
                    Application->AddTimer(this, 4, TheInterface->ScrollStart, 0, 0, 0);
                    _TextObject->ReceiveClick(1, 0);
                    return;
                }

                if (PtInRect(&_ScrollBarRect, point))
                {
                    // Original behaviour (OB-057): the line is picked from the cursor's screen y, not its window y, so
                    // the click lands 15 pixels (the window's y) further down the text.
                    _TextObject->ReceiveClick(0, event->Y - _ScrollBarRect.top);
                    return;
                }
            }
            break;
        }
        case 4:
        {
            Application->RemoveTimer(this, 4);
            Application->RemoveTimer(this, 5);
            Application->Release();

            if (_ScrollUpButton != nullptr)
            {
                _ScrollUpButton->ShowGuiWindow(0);
            }

            if (_ScrollDownButton != nullptr)
            {
                _ScrollDownButton->ShowGuiWindow(0);
            }
            break;
        }
        case 10:
        {
            if (event->Key == 0x1b)
            {
                _SkipAnimation = 1;
                return;
            }
            break;
        }
        case 0x13:
        {
            const int32_t timerId = event->Data;

            if (timerId == 4)
            {
                // The first repeat delay has passed: repeat five times as fast.
                Application->RemoveTimer(this, 4);
                Application->AddTimer(this, 5, TheInterface->ScrollStart / 5, 0, 0, 0);
            }
            else if (timerId != 5)
            {
                if (timerId != 10)
                {
                    return;
                }

                // The multiplayer timeout: close the screen (this object is deleted here).
                MCMissionResultsScreen* screen = Mission->ResultsScreen;

                if (screen == nullptr)
                {
                    return;
                }

                screen->Destroy();
                delete screen;
                Mission->ResultsScreen = nullptr;
                return;
            }

            const POINT point{event->X - GlobalX(), event->Y - GlobalY()};

            if (PtInRect(&_ScrollUpRect, point))
            {
                _TextObject->ReceiveClick(-1, 0);
                return;
            }

            if (PtInRect(&_ScrollDownRect, point))
            {
                _TextObject->ReceiveClick(1, 0);
                return;
            }
            break;
        }

        default:
            break;
    }
}

auto MCMissionResultsScreen::Display() -> void
{
    uint8_t* hazePalette = GamePalette()->GetHazePalette(-7);
    MCScreenVertex vertices[4] = {};
    vertices[1].X = Application->Width() - 1;
    vertices[2].X = Application->Width() - 1;
    vertices[2].Y = Application->Height() - 1;
    vertices[3].Y = Application->Height() - 1;
    VfxTranslatePolygon(ScreenPort->Frame(), std::span(vertices, 4), hazePalette);

    if (MPlayer == nullptr)
    {
        while (_NextDrawTime != 0 && (_NextDrawTime <= MouseTicks || _SkipAnimation != 0))
        {
            switch (_DrawState)
            {
                case 0:
                {
                    if (ScenarioResult < 4 || Solo != 0)
                    {
                        _DrawState = 1;
                    }
                    else
                    {
                        DrawRPs();
                    }
                    break;
                }
                case 1:
                    DrawStats();
                    break;
                case 2:
                case 3:
                    DrawObjectives();
                    break;
                case 4:
                    DrawPilots();
                    break;
                case 5:
                {
                    MCGuiScrollTextObject* text = _TextObject;
                    text->ShowGuiWindow(1);

                    if (text->TextBuffer == nullptr || text->TextBuffer[0] == '\0')
                    {
                        char line[256];
                        CLoadString(ThisInstance, 0x361, line, 0xfe);
                        text->FontIndex = 1;
                        text->Print(line, 0x1f);
                    }

                    text->Draw();
                    _NextDrawTime = 0;
                    _DrawState = 6;

                    if (_SkipAnimation == 0)
                    {
                        SoundSystem->PlayDigitalSample(0x32, 1, nullptr, 0, 0);
                    }
                    break;
                }

                default:
                    break;
            }
        }
    }

    // (The original copied the window's picture to the screen and displayed the children.)
    if (DisplayPort != nullptr)
    {
        DrawInFramePass(DisplayPort);
    }
}

auto MCMissionResultsScreen::Draw() -> void
{
    MCGuiObject::Draw();

    if (_MoveOnPort != nullptr)
    {
        _MoveOnPort->CopyTo(Port()->Frame(), 4, 4, 1);
    }

    if (MPlayer == nullptr)
    {
        DrawResourcePoints();
        DrawStatistics();
        DrawObjectiveList();
        // The pilots the steps have reached.
        const int32_t pilots = _DrawState == 4 ? _DrawIndex : (_DrawState > 4 ? _NumPilotResults : 0);

        for (int32_t i = 0; i < pilots; i++)
        {
            DrawPilot(i);
        }
    }
    else
    {
        DrawMPSummary();
        DrawMPPilotList();
        DrawMPObjectives();
    }
}

auto MCMissionResultsScreen::DrawRPs() -> void
{
    const int32_t shown = _DrawIndex * 1000;

    if (_ResourcePointsEarned < shown)
    {
        _ShownResourcePoints = _ResourcePointsEarned;
        _DrawIndex = 0;
        _DrawState++;
        _NextDrawTime = ResultsStepTicks + MouseTicks;

        if (_SkipAnimation == 0)
        {
            SoundSystem->PlayDigitalSample(0x43, 1, nullptr, 0, 0);
        }
    }
    else
    {
        _ShownResourcePoints = shown;
        _NextDrawTime = ResultsStepTicks / 20 + MouseTicks;

        if (_SkipAnimation == 0)
        {
            SoundSystem->PlayDigitalSample(0x42, 1, nullptr, 0, 0);
        }

        _DrawIndex++;
    }
}

auto MCMissionResultsScreen::DrawResourcePoints() -> void
{
    if (_ShownResourcePoints < 0)
    {
        return;
    }

    char text[256];
    FillBox(0x149, 9, 0x185, 0x14, 0x10);
    std::snprintf(text, sizeof(text), "%i", _ShownResourcePoints);
    LgWhiteFont->WriteString(Port()->Frame(), 0x14a, 10, reinterpret_cast<uint8_t*>(text), -1);
}

auto MCMissionResultsScreen::DrawStats() -> void
{
    _NextDrawTime = ResultsStepTicks / 2 + MouseTicks;

    switch (_DrawIndex)
    {
        case 0:
        case 1:
        case 2:
        case 3:
            _DrawIndex++;
            break;
        case 4:
        {
            _DrawIndex = 0;
            _DrawState++;
            _NextDrawTime = ResultsStepTicks + MouseTicks;
            break;
        }
        default:
            break;
    }

    if (_SkipAnimation == 0)
    {
        SoundSystem->PlayDigitalSample(0x47, 1, nullptr, 0, 0);
    }
}

auto MCMissionResultsScreen::DrawStatistics() -> void
{
    static constexpr int32_t statY[5] = {0x33, 0x40, 0x4d, 0x5a, 0x67};
    const int32_t values[5] = {_EnemyMechsHit, _EnemyMechsDestroyed, _EnemyPilotsKilled, _PlayerMechsHit,
                               _PlayerMechsDestroyed};
    // The statistics the steps have reached: each step shows one.
    const int32_t shown = _DrawState == 1 ? _DrawIndex : (_DrawState > 1 ? 5 : 0);

    for (int32_t i = 0; i < shown; i++)
    {
        char text[8];
        std::snprintf(text, sizeof(text), "%i", values[i]);
        LgWhiteFont->WriteString(Port()->Frame(), 0xcc, statY[i], reinterpret_cast<uint8_t*>(text), -1);
    }
}

auto MCMissionResultsScreen::DrawObjectives() -> void
{
    // drawState 2 lists the primary objectives (type 0), 3 the secondary ones (type 1).
    const uint32_t wantedType = _DrawState != 2 ? 1 : 0;
    int drew = 0;

    MCScenarioObjective& objective = Scenario->Objectives[_DrawIndex];

    if (objective.Type == wantedType)
    {
        drew = 1;

        if (wantedType == 1)
        {
            _ObjectivesHeaderDrawn = 1;
        }
    }

    if (_DrawIndex == static_cast<int32_t>(Scenario->NumObjectives))
    {
        // After the secondary objectives: the tonnage bonus, when it was earned.
        MCScenarioObjective& bonus = Scenario->Objectives[Scenario->NumObjectives];

        if (MPlayer == nullptr && _DrawState == 3 && bonus.Type == 3 && bonus.Points > 0 && ScenarioResult > 3 &&
            Solo == 0)
        {
            SoundSystem->PlayBettySample(10);
            drew = 1;
        }

        _DrawIndex = 0;
        _DrawState++;
    }
    else
    {
        _DrawIndex++;
    }

    if (drew != 0 && MPlayer == nullptr)
    {
        if (_SkipAnimation == 0)
        {
            SoundSystem->PlayDigitalSample(0x47, 1, nullptr, 0, 0);
        }

        _NextDrawTime = ResultsStepTicks + MouseTicks;
    }
}

auto MCMissionResultsScreen::DrawObjectiveList() -> void
{
    char text[256];
    char pointsName[256];
    char bonusHeader[256];
    char secondaryHeader[256];
    CLoadString(ThisInstance, 0x363, pointsName, 0xfe);
    CLoadString(ThisInstance, 0x360, bonusHeader, 0xfe);
    CLoadString(ThisInstance, 0x362, secondaryHeader, 0xfe);
    int32_t y = 0x92;
    bool headerDrawn = false;

    // Each step (state 2 or 3, objective 0..numObjectives) the steps have passed, laid out as the original drew them.
    for (int32_t state = 2; state <= 3; state++)
    {
        const uint32_t wantedType = state != 2 ? 1 : 0;

        for (int32_t index = 0; index <= static_cast<int32_t>(Scenario->NumObjectives); index++)
        {
            if (state > _DrawState || (state == _DrawState && index >= _DrawIndex))
            {
                return;
            }

            MCScenarioObjective& objective = Scenario->Objectives[index];

            if (objective.Type == wantedType)
            {
                if (wantedType == 1 && !headerDrawn)
                {
                    MedBlueFont->WriteString(Port()->Frame(), 0xf, y, reinterpret_cast<uint8_t*>(secondaryHeader), -1);
                    headerDrawn = true;
                    y += 0xe;
                }

                MCGuiFont* font = GreyFont;
                bool listed = true;

                if (objective.Status == 1)
                {
                    _SuccessPort->CopyTo(Port()->Frame(), 0xb, y - 1, 0);
                    font = GreenFont;
                }
                else if (objective.Status == 2)
                {
                    _FailurePort->CopyTo(Port()->Frame(), 0xb, y - 1, 0);
                    font = RedFont;
                }
                else if (objective.Status != 0)
                {
                    listed = false;
                }

                if (listed && font != nullptr)
                {
                    font->WriteString(Port()->Frame(), 0x17, y, reinterpret_cast<uint8_t*>(objective.Name), -1);
                    const int32_t nameY = y;
                    y = nameY + 10;

                    if (MPlayer == nullptr && objective.Points != 0)
                    {
                        std::snprintf(text, sizeof(text), "%i %s", objective.Points, pointsName);
                        font->WriteString(Port()->Frame(), 0x1d, nameY + 10, reinterpret_cast<uint8_t*>(text), -1);
                        y += 10;
                    }
                    else
                    {
                        y = nameY + 0xd;
                    }
                }
            }

            if (index == static_cast<int32_t>(Scenario->NumObjectives))
            {
                MCScenarioObjective& bonus = Scenario->Objectives[Scenario->NumObjectives];

                if (MPlayer == nullptr && state == 3 && bonus.Type == 3 && bonus.Points > 0 && ScenarioResult > 3 &&
                    Solo == 0)
                {
                    BlueFont->WriteString(Port()->Frame(), 0xf, y, reinterpret_cast<uint8_t*>(bonusHeader), -1);
                    const int32_t markY = y + 10;
                    y += 0xb;
                    _SuccessPort->CopyTo(Port()->Frame(), 0xb, markY, 0);
                    GreenFont->WriteString(Port()->Frame(), 0x17, y, reinterpret_cast<uint8_t*>(bonus.Name), -1);
                    const int32_t nameY = y;
                    y = nameY + 10;
                    std::snprintf(text, sizeof(text), "%i %s", bonus.Points, pointsName);
                    GreenFont->WriteString(Port()->Frame(), 0x1d, nameY + 10, reinterpret_cast<uint8_t*>(text), -1);
                    y += 0xb;
                }
            }
        }
    }
}

auto MCMissionResultsScreen::DrawPilots() -> void
{
    const int32_t index = _DrawIndex;
    _NextDrawTime = ResultsStepTicks + MouseTicks;

    if (index == _NumPilotResults)
    {
        _DrawIndex = 0;
        _DrawState++;
        return;
    }

    MCMechWarrior* warrior = _PilotResults[index].Warrior;

    if (warrior != nullptr)
    {
        _PilotIcons[static_cast<size_t>(index)] = TakeIconPicture(warrior, warrior->Vehicle->PartId);
        _DrawIndex++;
    }

    if (_SkipAnimation == 0)
    {
        SoundSystem->PlayDigitalSample(0x10, 1, nullptr, 0, 0);
    }
}

auto MCMissionResultsScreen::DrawPilot(int32_t index) -> void
{
    MCMechWarrior* warrior = _PilotResults[index].Warrior;

    if (warrior == nullptr)
    {
        return;
    }

    const auto line = [this](int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t color)
    { VfxLineDraw(Port()->Frame(), x0, y0, x1, y1, color); };
    const auto pixel = [this](int32_t x, int32_t y) { AGPixelWrite(Port()->Frame(), x, y, 0x10); };

    int32_t left;
    int32_t top;
    PilotBox(index, left, top);
    char text[256];
    uint32_t stringId;

    if (warrior->Status == 4)
    {
        stringId = 0x356;
    }
    else if (warrior->Wounds <= 4.0f)
    {
        stringId = 0x358;
    }
    else
    {
        stringId = 0x357;
    }

    CLoadString(ThisInstance, stringId, text, 0xfe);
    const int32_t textY = top + 3;
    WhiteFont->WriteString(Port()->Frame(), left + 3, textY, reinterpret_cast<uint8_t*>(text), -1);

    // The rank; a rank outside 0..3 leaves the status text in the buffer.
    const int32_t rank = static_cast<int8_t>(warrior->Rank);

    if (rank >= 0 && rank <= 3)
    {
        CLoadString(ThisInstance, 0x70 + static_cast<uint32_t>(rank), text, 0xfe);
    }

    MCGuiFont* rankFont = _PilotResults[index].OldRank < rank ? YellowFont : WhiteFont;
    rankFont->WriteString(Port()->Frame(), left + 0x39, textY, reinterpret_cast<uint8_t*>(text), -1);

    std::snprintf(text, sizeof(text), "%i", TotalKills(warrior));
    WhiteFont->WriteString(Port()->Frame(), left + 0x8c, textY, reinterpret_cast<uint8_t*>(text), -1);

    // A bar per skill: the old value, and the gain in another colour.
    int32_t barY = top + 0x18;

    for (const int32_t skill : StevesOrderLut)
    {
        const int32_t oldLength = SkillBarLength(_PilotResults[index].Skills[skill]);
        const int32_t newLength = SkillBarLength(static_cast<int32_t>(warrior->SkillRank[skill]));
        const int32_t barLeft = left + 0x62;
        line(barLeft, barY - 1, barLeft, barY, 0xe3);

        if (oldLength == newLength)
        {
            const int32_t end = oldLength + barLeft;
            const int32_t fillEnd = end - 2;
            line(left + 99, barY - 2, fillEnd, barY - 2, 0xe3);
            pixel(end - 1, barY - 2);
            line(end, barY - 2, end, barY + 1, 0x10);
            pixel(end - 1, barY + 1);
            line(end - 1, barY - 1, end - 1, barY, 0xe3);
            line(left + 99, barY + 1, fillEnd, barY + 1, 0xe5);
            line(left + 99, barY - 1, fillEnd, barY - 1, 0xe4);
            line(left + 99, barY, fillEnd, barY, 0xe4);
        }
        else
        {
            const int32_t oldEnd = oldLength + barLeft;
            line(left + 99, barY - 2, oldEnd, barY - 2, 0xe3);
            line(left + 99, barY - 1, oldEnd, barY - 1, 0xe4);
            line(left + 99, barY, oldEnd, barY, 0xe4);
            line(left + 99, barY + 1, oldEnd, barY + 1, 0xe5);
            const int32_t end = barLeft + newLength;
            const int32_t gainStart = oldEnd + 1;
            line(gainStart, barY - 2, end - 2, barY - 2, 0xa3);
            line(gainStart, barY - 1, end - 2, barY - 1, 0xf2);
            line(gainStart, barY, end - 2, barY, 0xf2);
            line(gainStart, barY + 1, end - 2, barY + 1, 0xf1);
            pixel(end - 1, barY - 2);
            line(end, barY - 2, end, barY + 1, 0x10);
            pixel(end - 1, barY + 1);
            line(end - 1, barY - 2, end - 1, barY + 1, 0xf1);
        }

        barY += 9;
    }

    DrawIconPicture(_PilotIcons[static_cast<size_t>(index)], Port()->Frame(), left + 4, top + 0x10);
}

auto MCMissionResultsScreen::DrawMPPilots(int showHomeSide) -> void
{
    _MpShowHomeSide = showHomeSide;
}

auto MCMissionResultsScreen::DrawMPPilotList() -> void
{
    const auto line = [this](int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t color)
    { VfxLineDraw(Port()->Frame(), x0, y0, x1, y1, color); };
    const auto pixel = [this](int32_t x, int32_t y) { AGPixelWrite(Port()->Frame(), x, y, 0x10); };

    // Clear the twelve pilot boxes (six per column) and their empty skill bars.
    for (int32_t row = -0x160; row <= 0x1b7; row += 0x42)
    {
        int32_t left;
        int32_t top;

        if (row < 0x2c)
        {
            left = 0xe8;
            top = row + 0x18c;
        }
        else
        {
            left = 0x18a;
            top = row;
        }

        const auto boxLeft = static_cast<int16_t>(left);
        const auto boxTop = static_cast<int16_t>(top);
        FillBox(boxLeft + 1, boxTop + 1, boxLeft + 0x70, boxTop + 10, 0x12);
        FillBox(boxLeft + 0x8c, boxTop + 1, boxLeft + 0x9a, boxTop + 10, 0x12);
        FillBox(boxLeft + 4, boxTop + 0x10, boxLeft + 0x33, boxTop + 0x39, 0x10);
        int32_t barY = top + 0x18;

        for (int32_t bar = 0; bar < 4; bar++)
        {
            line(left + 99, barY - 2, left + 0x97, barY - 2, 0x32);
            line(left + 99, barY - 1, left + 0x97, barY - 1, 0x12);
            line(left + 99, barY, left + 0x97, barY, 0x12);
            line(left + 99, barY + 1, left + 0x97, barY + 1, 0x14);
            line(left + 0x62, barY - 1, left + 0x62, barY, 0x32);
            line(left + 0x98, barY - 1, left + 0x98, barY, 0x14);
            barY += 9;
        }
    }

    int32_t rowBase = -0x160;

    for (int32_t i = 0; i < _NumPilotResults; i++)
    {
        MCMechWarrior* warrior = _PilotResults[i].Warrior;

        if (warrior == nullptr)
        {
            continue;
        }

        const bool homeSide = warrior->Alignment == HomeTeam()->Alignment;

        if (homeSide != (_MpShowHomeSide != 0))
        {
            continue;
        }

        int32_t top = rowBase;
        int32_t left;

        if (top < 0x2c)
        {
            left = 0xe8;
            top += 0x18c;
        }
        else
        {
            left = 0x18a;
        }

        char text[256];
        auto* mover = static_cast<MCMover*>(warrior->Vehicle);
        WhiteFont->WriteString(Port()->Frame(), left + 3, top + 2, reinterpret_cast<uint8_t*>(mover->NetName.data()),
                               -1);
        std::snprintf(text, sizeof(text), "%i", TotalKills(warrior));
        WhiteFont->WriteString(Port()->Frame(), left + 0x8c, top + 3, reinterpret_cast<uint8_t*>(text), -1);

        int32_t barY = top + 0x17;

        for (const int32_t skill : StevesOrderLut)
        {
            const int32_t length = SkillBarLength(static_cast<int32_t>(warrior->SkillRank[skill]));
            line(left + 0x62, barY, left + 0x62, barY + 1, 0xe3);
            const int32_t end = left + 0x62 + length;
            const int32_t fillEnd = end - 2;
            line(left + 99, barY - 1, fillEnd, barY - 1, 0xe3);
            pixel(end - 1, barY - 1);
            line(end, barY - 1, end, barY + 2, 0x10);
            pixel(end - 1, barY + 2);
            line(end - 1, barY, end - 1, barY + 1, 0xe3);
            line(left + 99, barY + 2, fillEnd, barY + 2, 0xe5);
            line(left + 99, barY, fillEnd, barY, 0xe4);
            line(left + 99, barY + 1, fillEnd, barY + 1, 0xe4);
            barY += 9;
        }

        DrawIconPicture(_PilotIcons[static_cast<size_t>(i)], Port()->Frame(), left + 4, top + 0x10);
        rowBase += 0x42;
    }
}

auto MCMissionResultsScreen::DrawMPSummary() -> void
{
    char text[256];

    // Port fix: MCX.EXE reads the best pilot without checking there is one.
    if (_NumPilotResults > 0)
    {
        // The best pilot.
        MCMechWarrior* best = _PilotResults[0].Warrior;
        auto* bestMover = static_cast<MCMover*>(best->Vehicle);
        MedWhiteFont->WriteString(Port()->Frame(), 0x70, 0x40, reinterpret_cast<uint8_t*>(bestMover->NetName.data()),
                                  0x68);
        CLoadString(ThisInstance, best->Alignment == HomeTeam()->Alignment ? 0xb6 : 0xb7, text, 0xfe);
        MedWhiteFont->WriteString(Port()->Frame(), 0x70, 0x4d, reinterpret_cast<uint8_t*>(text), -1);
        std::snprintf(text, sizeof(text), "%i", _PilotResults[0].OldRank);
        MedWhiteFont->WriteString(Port()->Frame(), 0xca, 0x4d, reinterpret_cast<uint8_t*>(text), -1);
        DrawIconPicture(_BestPilotIcon, Port()->Frame(), 0xb, 0x2f);
    }

    const int32_t statistics[5] = {_EnemyMechsHit, _EnemyMechsDestroyed, _EnemyPilotsKilled, _PlayerMechsHit,
                                   _PlayerMechsDestroyed};
    static constexpr int32_t statisticY[5] = {100, 0x70, 0x7c, 0x88, 0x94};

    for (int32_t i = 0; i < 5; i++)
    {
        std::snprintf(text, sizeof(text), "%i", statistics[i]);
        MedWhiteFont->WriteString(Port()->Frame(), 0xcc, statisticY[i], reinterpret_cast<uint8_t*>(text), -1);
    }

    MedWhiteFont->WriteString(Port()->Frame(), 0xb8, 0xa0, reinterpret_cast<uint8_t*>(_TimeText), -1);

    // The commanders by kills.
    int32_t row = 0;

    for (const CommanderLine& commander : _CommanderLines)
    {
        std::snprintf(text, sizeof(text), "%i.", commander.Place);
        const int32_t rowY = static_cast<int16_t>(row) * 0xc + 0xbe;
        MedBlueFont->WriteString(Port()->Frame(), 0x1a, rowY, reinterpret_cast<uint8_t*>(text), -1);
        std::snprintf(text, sizeof(text), "%s", commander.Name.c_str());
        MedWhiteFont->WriteString(Port()->Frame(), 0x28, rowY, reinterpret_cast<uint8_t*>(text), 0x68);
        std::snprintf(text, sizeof(text), "%i", commander.Score);
        MedWhiteFont->WriteString(Port()->Frame(), 0xcc, rowY, reinterpret_cast<uint8_t*>(text), -1);
        row++;
    }
}

auto MCMissionResultsScreen::Activate() -> int32_t
{
    EventsToMissionResultsScreen = 1;
    _SkipAnimation = 0;
    Application->RemoveCallback(InterfaceUpdateCallback);
    delete InterfaceUpdateCallback;
    InterfaceUpdateCallback = nullptr;
    TheInterface->HideTags();
    Application->RemoveCallback(ScenarioCallback);
    delete ScenarioCallback;
    ScenarioCallback = nullptr;
    Application->CursorHidden = 0;
    Application->SetCurrentCursor(static_cast<MCCursorType>(0));
    Application->CursorHidden = 1;
    Application->Release();

    _DrawY = 0x92;
    _DrawIndex = 0;
    _DrawState = 0;
    _NextDrawTime = MouseTicks;
    ScreenWindow->AddChild(this);
    SetDepth(0x5f);

    // The move-on button's label: "mission failed" when the scenario (or the home side) lost.
    if (MPlayer == nullptr)
    {
        if (ScenarioResult < 3)
        {
            _MoveOnPort->Destroy();
            _MoveOnPort->Init(const_cast<char*>("mr_mf00.tga"));
            _MoveOnButton->SetUpPicture(const_cast<char*>("mr_mf01.tga"));
            _MoveOnButton->SetDownPicture(const_cast<char*>("mr_mf02.tga"));
        }
    }
    else
    {
        _MoveOnPort->Destroy();
        _MoveOnPort->Init(HomeSideLost(ScenarioResult) ? const_cast<char*>("mr_mf00.tga")
                                                       : const_cast<char*>("mrm_ms00.tga"));
    }

    // (The original copied the label onto the window here and freed it; draw shows it.)
    _MoveOnButton->Draw();

    if (MPlayer == nullptr)
    {
        _PilotResults = nullptr;
        _PlayerMechsDestroyed = 0;
        _PlayerMechsHit = 0;
        _EnemyPilotsKilled = 0;
        _EnemyMechsDestroyed = 0;
        _EnemyMechsHit = 0;

        for (MCBaseObject* current : *ClanMechList())
        {
            auto* object = static_cast<MCGameObject*>(current);

            if ((object->IsDestroyed() || object->IsDisabled()) && !object->IsMarine())
            {
                _EnemyMechsHit++;

                if (object->ObjectClass == MCObjectClass::BattleMech)
                {
                    _EnemyMechsDestroyed++;
                }
            }
        }

        for (MCBaseObject* current : *InnerSphereMechList())
        {
            auto* object = static_cast<MCGameObject*>(current);

            if ((object->IsDestroyed() || object->IsDisabled()) && !object->IsMarine())
            {
                _PlayerMechsHit++;

                if (object->ObjectClass == MCObjectClass::BattleMech &&
                    static_cast<MCMover*>(object)->NetPlayerId != -1)
                {
                    _PlayerMechsDestroyed++;
                }
            }
        }

        // Count the home side's pilots.
        _NumPilotResults = 0;

        for (uint32_t i = 1; i <= Scenario->NumWarriors; i++)
        {
            MCMechWarrior* warrior = Scenario->Warriors[i];

            if (warrior == nullptr)
            {
                continue;
            }

            auto* vehicle = static_cast<MCMover*>(warrior->Vehicle);

            if (vehicle != nullptr && vehicle->GetAwake() && vehicle->ObjectClass == MCObjectClass::BattleMech &&
                warrior->Alignment == HomeTeam()->Alignment && vehicle->NetPlayerId != -1)
            {
                _NumPilotResults++;
            }
        }

        _PilotResults = std::make_unique<MCMissionPilotResult[]>(static_cast<size_t>(_NumPilotResults));

        // Fill the lines and apply the skill-ups.
        int32_t filled = 0;

        for (uint32_t i = 1; i <= Scenario->NumWarriors; i++)
        {
            MCMechWarrior* warrior = Scenario->Warriors[i];

            if (warrior == nullptr)
            {
                continue;
            }

            auto* vehicle = static_cast<MCMover*>(warrior->Vehicle);

            if (vehicle == nullptr || vehicle->ObjectClass != MCObjectClass::BattleMech || !vehicle->GetAwake())
            {
                continue;
            }

            if (!warrior->OnHomeTeam())
            {
                if (warrior->Status == 4 && warrior->Alignment != HomeTeam()->Alignment)
                {
                    _EnemyPilotsKilled++;
                }

                continue;
            }

            // Port fix: the fill loop doesn't test the network player id or the alignment the count did, so it can
            // find more pilots than it allocated for; the extra ones are left off the screen.
            if (filled >= _NumPilotResults)
            {
                continue;
            }

            MCMissionPilotResult& entry = _PilotResults[filled];
            entry.Warrior = warrior;

            if (vehicle->SensorSystem != nullptr)
            {
                warrior->SkillPoints[SkillSensors] =
                    static_cast<float>(vehicle->SensorSystem->TotalContacts) * SensorSkill;
            }

            warrior->SkillPoints[SkillPiloting] += warrior->SkillRank[SkillPiloting];
            entry.OldRank = static_cast<int8_t>(warrior->Rank);

            for (int32_t skill = 0; skill < 4; skill++)
            {
                float& skillRank = warrior->SkillRank[skill];
                float& skillPoints = warrior->SkillPoints[skill];
                int32_t gains = 0;
                entry.Skills[skill] = static_cast<int32_t>(skillRank);

                if (ScenarioResult > 3 && skillRank != 0.0f)
                {
                    // Each rank's worth of points buys one rank, at most three per scenario.
                    do
                    {
                        if (skillPoints < skillRank)
                        {
                            break;
                        }

                        skillPoints -= skillRank;

                        if (skillRank < MaxPilotSkill && gains < 3)
                        {
                            gains++;
                            skillRank += 1.0f;
                        }
                    } while (skillRank != 0.0f);
                }
            }

            warrior->CalcRank();
            Assert(entry.OldRank <= static_cast<int8_t>(warrior->Rank), 0, "Hey, how'd we drop in rank???");
            entry.SortKey = (3 - static_cast<int8_t>(warrior->Rank)) * 10000;
            // Original behaviour (OB-058): meant as letter * 10^n, the callsign's letters are XORed with n.
            const char* callsign = warrior->Callsign.c_str();

            for (int32_t letter = 0, power = 2; power >= 0; letter++, power--)
            {
                entry.SortKey += (callsign[letter] * 10) ^ power;
            }

            filled++;
        }

        _NumPilotResults = filled;
        std::qsort(_PilotResults.get(), static_cast<size_t>(_NumPilotResults), sizeof(MCMissionPilotResult),
                   ComparePilots);
        _PilotIcons.assign(static_cast<size_t>(_NumPilotResults), nullptr);
        _ShownResourcePoints = -1;

        // Only the first nine objectives' points count.
        _ResourcePointsEarned = 0;

        for (int32_t i = 0; i < 9; i++)
        {
            if (Scenario->Objectives[i].Status == 1)
            {
                _ResourcePointsEarned += Scenario->Objectives[i].Points;
            }
        }

        // Borrow the tactical map's text box for the debriefing.
        MCTacticalMap* tacMap = TacticalMap();
        _TextObject = nullptr;
        tacMap->RemoveChild(tacMap->SalvageText.get());
        _TextObject = tacMap->SalvageText.get();
        AddChild(_TextObject);
        _TextObject->MoveTo(10, 0x15b, 0);
        _TextObject->Resize(0xc4, 0x59);
        _TextObject->ShowGuiWindow(0);

        SoundSystem->PlayBettySample(ScenarioResult < 4 ? 0xb : 0x12);
    }
    else
    {
        if (IsMPlayerGame != 0)
        {
            Application->AddTimer(this, 10, 90000, 0, 0, 0);
        }

        MCMissionCommanderScore scores[6];

        for (int32_t i = 0; i < 6; i++)
        {
            scores[i] = {i, -1};
        }

        _PlayerMechsDestroyed = 0;
        _PlayerMechsHit = 0;
        _EnemyPilotsKilled = 0;
        _EnemyMechsDestroyed = 0;
        _EnemyMechsHit = 0;

        _NumPilotResults = 0;

        for (uint32_t i = 1; i <= Scenario->NumWarriors; i++)
        {
            MCMechWarrior* warrior = Scenario->Warriors[i];

            if (warrior != nullptr && warrior->Vehicle != nullptr &&
                static_cast<MCMover*>(warrior->Vehicle)->GetAwake())
            {
                _NumPilotResults++;
            }
        }

        _PilotResults = std::make_unique<MCMissionPilotResult[]>(static_cast<size_t>(_NumPilotResults));

        int32_t filled = 0;

        for (uint32_t i = 1; i <= Scenario->NumWarriors; i++)
        {
            MCMechWarrior* warrior = Scenario->Warriors[i];

            if (warrior == nullptr)
            {
                continue;
            }

            auto* vehicle = static_cast<MCMover*>(warrior->Vehicle);

            if (vehicle == nullptr || !vehicle->GetAwake())
            {
                continue;
            }

            if (vehicle->IsDisabled())
            {
                if (warrior->Team == HomeTeam())
                {
                    _PlayerMechsHit++;

                    if (vehicle->ObjectClass == MCObjectClass::BattleMech)
                    {
                        _PlayerMechsDestroyed++;
                    }
                }
                else
                {
                    _EnemyMechsHit++;

                    if (vehicle->ObjectClass == MCObjectClass::BattleMech)
                    {
                        _EnemyMechsDestroyed++;
                    }
                }
            }

            MCMissionPilotResult& entry = _PilotResults[filled];
            entry.Warrior = warrior;

            if (warrior->Status == 4 && warrior->Team != HomeTeam())
            {
                _EnemyPilotsKilled++;
            }

            for (int32_t kind = 0; kind < 7; kind++)
            {
                MCMissionCommanderScore& score = scores[vehicle->GetCommanderId()];

                if (score.Score == -1)
                {
                    score.Score = 0;
                }

                const int32_t kills = warrior->NumKilled[kind][1];
                score.Score += kills;
                entry.SortKey += kills * -10000;
                entry.OldRank += kills;
            }

            // Damage taken counts against the pilot: armor lost, and internal structure lost twice over.
            for (int32_t j = 0; j < vehicle->NumArmorLocations(); j++)
            {
                const MCArmorLocation& armor = vehicle->Armor[j];
                entry.SortKey =
                    static_cast<int32_t>(static_cast<double>(armor.MaxArmor) - armor.CurArmor + entry.SortKey);
            }

            for (int32_t j = 0; j < vehicle->NumBodyLocations(); j++)
            {
                const MCBodyLocation& body = vehicle->BodyAt(j);
                entry.SortKey = static_cast<int32_t>(
                    (static_cast<double>(body.MaxInternalStructure) - body.CurInternalStructure) * 2 + entry.SortKey);
            }

            filled++;
        }

        std::qsort(scores, 6, sizeof(MCMissionCommanderScore), CompareCommanders);
        Assert(_NumPilotResults == filled, static_cast<uint32_t>(_NumPilotResults), " warriorcount != warriorcount2! ");
        std::qsort(_PilotResults.get(), static_cast<size_t>(_NumPilotResults), sizeof(MCMissionPilotResult),
                   ComparePilots);

        // (The original drew the summary, the home side's pilots and the objectives into the window's picture here;
        // draw shows them each frame from what is kept below.)
        // Port fix: MCX.EXE reads the best pilot without checking there is one.
        if (_NumPilotResults > 0)
        {
            MCMechWarrior* best = _PilotResults[0].Warrior;
            _BestPilotIcon = TakeIconPicture(best, static_cast<MCMover*>(best->Vehicle)->PartId);
        }

        _PilotIcons.assign(static_cast<size_t>(_NumPilotResults), nullptr);

        for (int32_t i = 0; i < _NumPilotResults; i++)
        {
            MCMechWarrior* warrior = _PilotResults[i].Warrior;

            if (warrior != nullptr)
            {
                _PilotIcons[static_cast<size_t>(i)] =
                    TakeIconPicture(warrior, static_cast<MCMover*>(warrior->Vehicle)->PartId);
            }
        }

        const int32_t seconds = static_cast<int32_t>(std::fmod(static_cast<double>(ActualTime), 60.0));
        std::snprintf(_TimeText, sizeof(_TimeText), "%02i:%02i", static_cast<int32_t>(ActualTime) / 60, seconds);

        // The commanders by kills.
        _CommanderLines.clear();

        for (int32_t i = 0; i < 6; i++)
        {
            if (scores[i].Score < 0 || MPlayer->SessionManager->GetPlayerNumber(scores[i].CommanderId) == nullptr)
            {
                continue;
            }

            MCFidpPlayer* player = MPlayer->SessionManager->GetPlayerNumber(scores[i].CommanderId);
            _CommanderLines.push_back({i + 1, player->Name, scores[i].Score});
        }

        DrawMPPilots(1);
        _DrawY = 0x11d;
        SoundSystem->PlayBettySample(HomeSideLost(ScenarioResult) ? 0xb : 0x12);
    }

    if (MPlayer != nullptr && MPlayer->SessionManager != nullptr && Mission->EndScenarioRequested != 0)
    {
        MPlayer->LeaveSession();
        MPlayer->InMission = 1;
    }

    SomethingOnFire = 0;
    _ScenarioEnded = 0;
    return 0;
}

auto MCMissionResultsScreen::DrawMPObjectives() -> void
{
    char pointsName[256];
    char bonusHeader[256];
    char secondaryHeader[256];
    CLoadString(ThisInstance, 0x363, pointsName, 0xfe);
    CLoadString(ThisInstance, 0x360, bonusHeader, 0xfe);
    CLoadString(ThisInstance, 0x362, secondaryHeader, 0xfe);

    // (The original drew once, stepping drawY and objectivesHeaderDrawn; draw calls this each frame, so it lays out
    // from locals.)
    int32_t y = _DrawY;
    int32_t headerDrawn = _ObjectivesHeaderDrawn;

    // Only the primary objectives (type 0) of the home team are listed; the loop over the types stops after the
    // first, so the secondary header below is never drawn.
    for (uint32_t type = 0; type < 1; type++)
    {
        const int32_t first = HomeTeam()->FirstObjective;
        const int32_t end = first + static_cast<int32_t>(HomeTeam()->NumObjectives);

        for (int32_t i = first; i < end; i++)
        {
            MCScenarioObjective& objective = Scenario->Objectives[i];

            if (objective.Type != type)
            {
                continue;
            }

            if (type == 1 && headerDrawn == 0)
            {
                MedBlueFont->WriteString(Port()->Frame(), 0xf, y, reinterpret_cast<uint8_t*>(secondaryHeader), -1);
                headerDrawn = 1;
                y += 0xe;
            }

            MCGuiFont* font = GreyFont;

            if (objective.Status == 1)
            {
                _SuccessPort->CopyTo(Port()->Frame(), 0xb, y - 1, 0);
                font = GreenFont;
            }
            else if (objective.Status == 2)
            {
                _FailurePort->CopyTo(Port()->Frame(), 0xb, y - 1, 0);
                font = RedFont;
            }
            else if (objective.Status != 0)
            {
                continue;
            }

            if (font != nullptr)
            {
                font->WriteString(Port()->Frame(), 0x17, y, reinterpret_cast<uint8_t*>(objective.Name), -1);
                y += 0xd;
            }
        }
    }
}
