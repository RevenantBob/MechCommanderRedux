#include "stdafx.h"
#include "mission/mission.h"
#include "camera/camera.h"
#include "color/color.h"
#include "gui/abutton.h"
#include "gui/afont.h"
#include "gui/atextbox.h"
#include "gui/aport.h"
#include "gui/updisp.h"
#include "iface/icallbk.h"
#include "iface/iface.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "lib/inifile.h"
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
#include "object/cmponent.h"
#include "object/contact.h"
#include "object/mover.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/objtype.h"
#include "object/team.h"
#include "object/warrior.h"
#include "sound/soundsys.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/vfxfuncs.h"
#include "platform/MCFileSystem.h"
#include "platform/MCInput.h"
#include "platform/MCSmacker.h"
#include "platform/MCWin32Defs.h"

uint32_t resultsStepTicks = 20;
int32_t StevesOrderLUT[4] = {MWS_GUNNERY, MWS_PILOTING, MWS_JUMPING, MWS_SENSORS};
int32_t globalGameSegment = 0;
int32_t StartingResourcePoints = 0;
float MinPilotSkill = 0.0f;
SYSTEMTIME logisticsStart{};
SYSTEMTIME logisticsEnd{};
float MaxPilotSkill = 0.0f;
aCallback* scenarioCallback = nullptr;
aCallback* interfaceUpdateCallback = nullptr;
int32_t nextGameState = 0;
int gameOver = 0;
int featureMusicPlaying = 0;
uint32_t lastScenario = 0;
Mission* mission = nullptr;
int32_t playingLogisticsMusic = 0;
aCallback* setupCallback = nullptr;
uint32_t scenarioResult = 0;
int somethingOnFire = 0;
int EventsToMissionResultsScreen = 0;
char missionPath[80] = "data\\missions\\";
char CDmoviePath[80] = "data\\movies\\";
char moviePath[80] = "data\\movies\\";

namespace
{
    /// <summary>The mission heap could not be allocated (the port's name; MCX.EXE returns the bare value).</summary>
    constexpr int32_t NO_RAM_FOR_MISSION_HEAP = static_cast<int32_t>(0xFACC0002);
    /// <summary>A movie or scenario name list could not be allocated from the mission heap (the port's name).</summary>
    constexpr int32_t NO_RAM_FOR_MISSION_LISTS = static_cast<int32_t>(0xFACC0003);

    /// <summary>
    /// Reads the names <c>&lt;idFormat&gt;0</c>.. <c>&lt;idFormat&gt;(count-1)</c> of the current block into a list
    /// allocated from <paramref name="heap"/> (the binary repeats this loop for the movies and scenarios of each init).
    /// </summary>
    /// <returns>0, a FIT read error, or NO_RAM_FOR_MISSION_LISTS.</returns>
    int32_t readNameList(FitIniFile* file, UserHeap* heap, const char* idFormat, uint32_t count, char**& names)
    {
        names = static_cast<char**>(heap->malloc(count * static_cast<uint32_t>(sizeof(char*))));

        if (names == nullptr)
        {
            return NO_RAM_FOR_MISSION_LISTS;
        }

        std::memset(names, 0, count * sizeof(char*));

        for (int32_t i = 0; i < static_cast<int32_t>(count); i++)
        {
            char id[50];
            char name[100];
            std::snprintf(id, sizeof(id), idFormat, i);
            const int32_t result = file->readIdString(id, name, 99);

            if (result != 0)
            {
                return result;
            }

            const size_t length = std::strlen(name) + 1;
            names[i] = static_cast<char*>(heap->malloc(static_cast<uint32_t>(length)));
            std::memcpy(names[i], name, length);
        }

        return 0;
    }

    /// <summary>
    /// The file name part of <paramref name="path"/> without folder or extension (the <c>fname</c> of
    /// <c>_splitpath</c>).
    /// </summary>
    void splitFileName(const char* path, char* fileName, size_t size)
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
    void resetFullScreenDisplay()
    {
        if (gFullScreen != 0)
        {
            application->resetDirectDraw(application->width(), application->height(), 8);
        }
    }

    /// <summary>
    /// Starts a new logistics phase after a scenario: restarts the logistics clock and makes and inits
    /// <c>globalLogPtr</c> (EndScenario inlines this three times).
    /// </summary>
    Logistics* startLogistics(Mission* owner)
    {
        MCPort::GetSystemTime(logisticsStart);
        auto* logistics = new Logistics;
        owner->logistics = logistics;
        globalLogPtr = logistics;
        Assert(logistics != nullptr, 0, " Could not start logistics phase ");
        logistics->init();
        return logistics;
    }

    /// <summary>Frees a logistics phase (<c>Logistics::destroy</c>, then the inlined destructor).</summary>
    void deleteLogistics(Logistics* logistics)
    {
        logistics->destroy();
        delete logistics;
    }
}

auto playScenario() -> void
{
    // Port: a running scenario draws on the whole window, whatever its size now.
    if (mission != nullptr && mission->missionState == 7)
    {
        MCFollowWindowSize();
    }

    globalPane = screenPort->frame();
    globalWindow = screenPort->frame()->window;

    if (scenario != nullptr && scenarioResult == 0)
    {
        scenarioResult = static_cast<uint32_t>(scenario->run());
    }

    if (soundSystem != nullptr && useSound != 0)
    {
        soundSystem->update();
    }
}

auto RunMission() -> void
{
    mission->run();

    if (scenario == nullptr)
    {
        frameLength =
            static_cast<float>(static_cast<uint32_t>(stopTime - prevStart)) / static_cast<float>(countsPerSecond);
    }
}

auto Mission::init(char* missionName) -> int32_t
{
    int32_t result = 0;

    if (globalGameSegment != 0)
    {
        //-----------------------------------------------------------------------------------------------------------
        // A game segment build: the mission FIT is the segment file itself.
        cheatsOn = 1;
        init();
        missionFile = new FitIniFile;

        if (missionFile == nullptr)
        {
            return 3;
        }

        FullPathFileName fileName;
        fileName.init(missionPath, missionName, ".fit");
        result = missionFile->open(fileName);

        if (result != 0)
        {
            return result;
        }

        result = missionFile->seekBlock("HeapInfo");

        if (result != 0)
        {
            return result;
        }

        result = missionFile->readIdULong("HeapSize", heapSize);

        if (result != 0)
        {
            return result;
        }

        missionHeap = new UserHeap;

        if (missionHeap == nullptr)
        {
            return NO_RAM_FOR_MISSION_HEAP;
        }

        result = missionHeap->init(heapSize, nullptr);

        if (result != 0)
        {
            return result;
        }

        result = missionFile->seekBlock("Movies");

        if (result != 0)
        {
            return result;
        }

        result = missionFile->readIdULong("NumMovies", numMovies);

        if (result != 0)
        {
            return result;
        }

        result = readNameList(missionFile, missionHeap, "Movie%d", numMovies, movies);

        if (result != 0)
        {
            return result;
        }

        if (missionFile->readIdFloat("WaitTime", waitTime) != 0)
        {
            waitTime = 120.0f;
        }

        result = missionFile->seekBlock("Scenarios");

        if (result != 0)
        {
            return result;
        }

        result = missionFile->readIdULong("NumScenarios", numScenarios);

        if (result != 0)
        {
            return result;
        }

        result = readNameList(missionFile, missionHeap, "Scenario%d", numScenarios, scenarios);

        if (result != 0)
        {
            return result;
        }

        return SetupNextSegment(globalGameSegment);
    }

    //---------------------------------------------------------------------------------------------------------------
    // The campaign: the control file names the campaign file, whose FIT holds the movies and scenarios.
    init();
    MCPort::GetSystemTime(logisticsStart);
    missionFile = new FitIniFile;

    if (missionFile == nullptr)
    {
        return 3;
    }

    FullPathFileName controlName;
    controlName.init(missionPath, missionName, ".fit");
    result = missionFile->open(controlName);

    if (result != 0)
    {
        return result;
    }

    result = missionFile->seekBlock("Control");

    if (result != 0)
    {
        return result;
    }

    uint32_t numCampaigns = 0;
    result = missionFile->readIdULong("NumCampaigns", numCampaigns);

    if (result != 0)
    {
        return result;
    }

    // Port fix: MCX.EXE leaves the name uninitialised (and splits stack garbage) when there are 2+ campaigns.
    char campaignFile[80] = {};

    if (numCampaigns < 2)
    {
        result = missionFile->seekBlock("Campaign0");

        if (result != 0)
        {
            return result;
        }

        result = missionFile->readIdString("CampaignFile", campaignFile, 79);

        if (result != 0)
        {
            return result;
        }
    }

    missionFile->close();
    delete missionFile;
    missionFile = nullptr;

    char campaignName[256];
    splitFileName(campaignFile, campaignName, sizeof(campaignName));
    FullPathFileName campaignFileName;
    campaignFileName.init(missionPath, campaignName, ".fit");
    missionFile = new FitIniFile;

    if (missionFile == nullptr)
    {
        return 3;
    }

    result = missionFile->open(campaignFileName);

    if (result != 0)
    {
        return result;
    }

    result = missionFile->seekBlock("HeapInfo");

    if (result != 0)
    {
        return result;
    }

    result = missionFile->readIdULong("HeapSize", heapSize);

    if (result != 0)
    {
        return result;
    }

    missionHeap = new UserHeap;

    if (missionHeap == nullptr)
    {
        return NO_RAM_FOR_MISSION_HEAP;
    }

    result = missionHeap->init(heapSize, nullptr);

    if (result != 0)
    {
        return result;
    }

    result = missionFile->seekBlock("Movies");

    if (result != 0)
    {
        return result;
    }

    result = missionFile->readIdULong("NumMovies", numMovies);

    if (result != 0)
    {
        return result;
    }

    if (missionFile->readIdBoolean("InDemo", InDemo) != 0)
    {
        InDemo = 0;
    }

    if (fileExists("ixtlriimceourl"))
    {
        cheatsOn = 1;
    }

    if (numMovies == 0)
    {
        movies = nullptr;
    }
    else
    {
        result = readNameList(missionFile, missionHeap, "Movie%d", numMovies, movies);

        if (result != 0)
        {
            return result;
        }
    }

    if (missionFile->readIdFloat("WaitTime", waitTime) != 0)
    {
        waitTime = 120.0f;
    }

    result = missionFile->seekBlock("Scenarios");

    if (result != 0)
    {
        return result;
    }

    result = missionFile->readIdULong("NumScenarios", numScenarios);

    if (result != 0)
    {
        return result;
    }

    result = missionFile->readIdULong("LastScenario", lastScenario);

    if (result != 0)
    {
        return result;
    }

    result = readNameList(missionFile, missionHeap, "Scenario%d", numScenarios, scenarios);

    if (result != 0)
    {
        return result;
    }

    //---------------------------------------------------------------------------------------------------------------
    // The component list, which logistics needs before any scenario has loaded it.
    if (MasterComponentList == nullptr)
    {
        FullPathFileName gameSystemName;
        gameSystemName.init(missionPath, "gamesys", ".fit");
        // MCX.EXE allocates this FIT (Fatal " Game System File " when out of memory) and never frees it.
        FitIniFile gameSystemFile;
        uint32_t readResult = static_cast<uint32_t>(gameSystemFile.open(gameSystemName));
        Assert(readResult == 0, readResult, " Could not open GameSys.Fit file ");
        readResult = static_cast<uint32_t>(gameSystemFile.seekBlock("General"));
        Assert(readResult == 0, readResult, " Could not find General Block in GameSys ");
        float maxVisualRange = 0.0f;
        float maxWeaponRange = 0.0f;
        float baseSensorRange = 0.0f;
        readResult = static_cast<uint32_t>(gameSystemFile.readIdFloat("MaxVisualRange", maxVisualRange));
        Assert(readResult == 0, readResult, " Could not find MaxVisualRange in GameSys ");
        readResult = static_cast<uint32_t>(gameSystemFile.readIdFloat("MaxWeaponRange", maxWeaponRange));
        Assert(readResult == 0, readResult, " Could not find MaxWeaponRange in GameSys ");
        readResult = static_cast<uint32_t>(gameSystemFile.readIdFloat("BaseSensorRange", baseSensorRange));
        Assert(readResult == 0, readResult, " Could not find BaseSensorRange in GameSys ");
        gameSystemFile.close();

        FullPathFileName componentName;
        componentName.init(objectPath, "compbas", ".csv");
        const int32_t loadResult =
            initMasterComponentListEXCEL(componentName, 0xff, maxVisualRange / maxWeaponRange, baseSensorRange);
        // Faithful: the assert reports the previous read's code.
        Assert(loadResult == 0, readResult, " Could not load compBas.csv ");
    }

    //---------------------------------------------------------------------------------------------------------------
    // Logistics.
    globalLogPtr = new Logistics;
    logistics = globalLogPtr;
    globalLogPtr->init();

    if (launchedFromLobby == 0)
    {
        SetupNextSegment(globalGameSegment);
    }
    else
    {
        missionState = 3;
        Assert(MPlayer != nullptr, 0, nullptr);

        if (MPlayer->setupLobbyGame() != 0)
        {
            char text[256];
            cLoadString(thisInstance, 0x370, text, 0xfe);
            ReusableDialog* dialog = globalLogPtr->messageDialog;
            dialog->setText(text);
            dialog->setTwoButton(0);
            dialog->okButton->callback()->setExec(CancelToMPlayer);
            dialog->okButton->setUpPicture(const_cast<char*>("bh_okay.tga"));
            dialog->okButton->setDownPicture(const_cast<char*>("bg_okay.tga"));
            dialog->okButton->disabled = 0;
            dialog->okButton->draw();
            dialog->activate();
            globalLogPtr->currentScreen = globalLogPtr->mainScreen;
            globalLogPtr->logisticsState = 1;
            globalLogPtr->showLogScreen(0, 0);
        }
    }

    return 0;
}

auto Mission::initAgain(char* missionName) -> int32_t
{
    FullPathFileName fileName;
    fileName.init(missionPath, missionName, ".fit");
    // MCX.EXE keeps this FIT in a local (missionFile still holds the old one) and leaks it on every error return.
    auto* file = new FitIniFile;

    if (file == nullptr)
    {
        return 3;
    }

    int32_t result = file->open(fileName);

    if (result != 0)
    {
        return result;
    }

    result = file->seekBlock("HeapInfo");

    if (result != 0)
    {
        return result;
    }

    result = file->readIdULong("HeapSize", heapSize);

    if (result != 0)
    {
        return result;
    }

    // The old heap is deleted without UserHeap::destroy (as Mission::destroy does first).
    delete missionHeap;
    missionHeap = new UserHeap;

    if (missionHeap == nullptr)
    {
        return NO_RAM_FOR_MISSION_HEAP;
    }

    result = missionHeap->init(heapSize, nullptr);

    if (result != 0)
    {
        return result;
    }

    result = file->seekBlock("Movies");

    if (result != 0)
    {
        return result;
    }

    result = file->readIdULong("NumMovies", numMovies);

    if (result != 0)
    {
        return result;
    }

    if (numMovies == 0)
    {
        movies = nullptr;
    }
    else
    {
        result = readNameList(file, missionHeap, "Movie%d", numMovies, movies);

        if (result != 0)
        {
            return result;
        }
    }

    if (file->readIdFloat("WaitTime", waitTime) != 0)
    {
        waitTime = 120.0f;
    }

    result = file->seekBlock("Scenarios");

    if (result != 0)
    {
        return result;
    }

    result = file->readIdULong("NumScenarios", numScenarios);

    if (result != 0)
    {
        return result;
    }

    result = file->readIdULong("LastScenario", lastScenario);

    if (result != 0)
    {
        return result;
    }

    result = readNameList(file, missionHeap, "Scenario%d", numScenarios, scenarios);

    if (result != 0)
    {
        return result;
    }

    // Freed without close() (the File destructor closes it).
    delete file;
    return 0;
}

auto Mission::SetupNextSegment(int32_t segment) -> int32_t
{
    char blockName[50];
    std::snprintf(blockName, sizeof(blockName), "GameSegment%d", segment);
    int32_t result = missionFile->seekBlock(blockName);

    if (result != 0)
    {
        return result;
    }

    result = missionFile->readIdULong("GameState", reinterpret_cast<uint32_t&>(missionState));

    if (result != 0)
    {
        return result;
    }

    result = missionFile->readIdULong("SmackerMovieId", reinterpret_cast<uint32_t&>(currentMovie));

    if (result != 0)
    {
        return result;
    }

    result = missionFile->readIdULong("ScenarioId", reinterpret_cast<uint32_t&>(currentScenario));

    if (result != 0)
    {
        return result;
    }

    result = missionFile->readIdULong("NextGameState", reinterpret_cast<uint32_t&>(nextGameState));

    if (result != 0)
    {
        return result;
    }

    if (globalGameSegment == 0)
    {
        logistics->getCurrentMission();
        return 0;
    }

    missionState = 11;
    return 0;
}

auto Mission::init() -> int32_t
{
    missionState = 0;
    resultsScreen = nullptr;
    missionHeap = nullptr;
    unknown04 = nullptr;
    missionFile = nullptr;
    logistics = nullptr;

    if (missionCallback == nullptr)
    {
        missionCallback = new aCallback;
        missionCallback->setExec(RunMission);
        application->addCallback(missionCallback);
    }

    return 0;
}

auto Mission::destroy() -> void
{
    if (missionHeap != nullptr)
    {
        missionHeap->destroy();
        delete missionHeap;
        missionHeap = nullptr;
    }

    if (missionFile != nullptr)
    {
        missionFile->close();
        delete missionFile;
        missionFile = nullptr;
    }

    if (unknown04 != nullptr)
    {
        delete unknown04;
        unknown04 = nullptr;
    }

    if (scenarioCallback != nullptr)
    {
        application->removeCallback(scenarioCallback);
        delete scenarioCallback;
        scenarioCallback = nullptr;
    }

    if (interfaceUpdateCallback != nullptr)
    {
        application->removeCallback(interfaceUpdateCallback);
        delete interfaceUpdateCallback;
        interfaceUpdateCallback = nullptr;
    }

    if (scenario != nullptr)
    {
        saveWindowStatus();
        scenario->destroy();
        delete scenario;
        scenario = nullptr;
    }

    if (missionCallback != nullptr)
    {
        application->removeCallback(missionCallback);
        delete missionCallback;
        missionCallback = nullptr;
    }

    if (globalLogPtr != nullptr)
    {
        if (logistics != nullptr)
        {
            deleteLogistics(logistics);
        }

        logistics = nullptr;
        globalLogPtr = nullptr;
    }

    missionState = 0;
}

auto Mission::run() -> int32_t
{
    keepScreenBlack = 0;
    // The states that end up back in logistics share the tails of the binary's switch; these flags stand in for its
    // two jump targets (reset the display first, or not).
    bool resetDisplay = false;
    bool toLogistics = false;

    switch (missionState)
    {
        case 0:
        {
            if (currentMovie == -1)
            {
                break;
            }

            if (nextGameState == 10)
            {
                missionState = 10;
                break;
            }

            if (gFullScreen != 0)
            {
                application->resetDirectDraw(application->width(), application->height(), 8);
                application->paletteCycle = 1;
                application->activatePalette(gamePalette->rgbData, 0, 0x100);
            }

            toLogistics = true;
            break;
        }

        case 3:
        {
            if (logistics != nullptr && logistics->currentScreen->IsShowing() == 0)
            {
                logistics->showLogScreen(1, 0);
            }

            escapedSmackerMovie = 0;
            movieOver = 0;
            application->showCursor(1);

            if (soundSystem != nullptr)
            {
                if (logistics == nullptr)
                {
                    break;
                }

                const bool onMainScreen = logistics->currentScreen == logistics->mainScreen;

                if (onMainScreen && playingLogisticsMusic != 1)
                {
                    soundSystem->playDigitalMusic(0x17, true);
                    playingLogisticsMusic = 1;
                }

                if (!onMainScreen && playingLogisticsMusic != 2)
                {
                    soundSystem->playDigitalMusic(0x16, true);
                    playingLogisticsMusic = 2;
                }
            }

            if (logistics != nullptr && MPlayer != nullptr)
            {
                MPlayer->processReceiveList();
            }
            break;
        }

        case 6:
        {
            if (MPlayer != nullptr && MPlayer->sessionManager != nullptr)
            {
                MPlayer->processReceiveList();
            }
            break;
        }

        case 7:
        {
            if ((scenarioResult != 0 || endScenarioRequested != 0) && scenario->startingUp == 0)
            {
                scenario->setupBonus();
                resultsScreen->activate();
                missionState = 6;
                playingLogisticsMusic = 0;
            }
            break;
        }

        case 8:
        {
            if (application->smackerWindow != nullptr)
            {
                break;
            }

            if (nextGameState == 10)
            {
                missionState = 10;
                currentMovie++;

                if (currentMovie == 2)
                {
                    nextGameState = 3;
                }
                else if (currentMovie == 5)
                {
                    nextGameState = 16;
                    missionState = 16;
                }
                else
                {
                    nextGameState = 10;
                }
                break;
            }

            if (gameOver != 0)
            {
                missionState = 16;
                break;
            }

            resetDisplay = true;
            break;
        }

        case 10:
        {
            const int32_t movie = currentMovie;

            if (movie == -1)
            {
                break;
            }

            if (escapedSmackerMovie == 0)
            {
                if (soundSystem != nullptr)
                {
                    soundSystem->stopDigitalMusic();
                }

                FullPathFileName movieName;
                movieName.init(CDmoviePath, movies[movie], ".smk");
                // MCX.EXE: when movie 1 (the opening) isn't installed, it scans drives C: to Z: for a CD-ROM holding
                // \data\movies\opening.smk. The port reads movies from the install only.
                application->startSmackerMovie(movieName, 0xfe000, nullptr, 1);
                missionState = 8;
                playingLogisticsMusic = 0;
                break;
            }

            escapedSmackerMovie = 0;

            if (gameOver != 0)
            {
                missionState = 16;
                break;
            }

            resetFullScreenDisplay();
            playingLogisticsMusic = 0;
            toLogistics = true;
            break;
        }

        case 11:
        {
            if (currentScenario != -1)
            {
                if (soundSystem != nullptr)
                {
                    soundSystem->stopDigitalMusic();
                }

                char* scenarioName = (MPlayer == nullptr || globalLogPtr == nullptr) ? scenarios[currentScenario]
                                                                                     : globalLogPtr->mpMissionName;
                StartScenario(scenarioName);
                application->showCursor(1);
            }
            break;
        }

        case 13:
        {
            if (application->smackerWindow2 != nullptr)
            {
                break;
            }

            if (nextGameState == 10)
            {
                if (currentMovie++ != 0)
                {
                    missionState = 10;
                    nextGameState = 3;
                    break;
                }

                missionState = 12;
                nextGameState = 10;
                break;
            }

            if (InDemo != 0 && currentMovie == 2)
            {
                resetFullScreenDisplay();
                missionState = 20;
                break;
            }

            if (gameOver != 0 && currentMovie == 3)
            {
                missionState = 16;
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
            if (featureScreen != nullptr)
            {
                if (featureMusicPlaying == 0)
                {
                    soundSystem->playDigitalMusic(8, false);
                    featureMusicPlaying = 1;
                }
            }

            if (featureScreen == nullptr)
            {
                //-------------------------------------------------------------------------------------------------------
                // The demo's feature screen: features.tga with the palette from its own colour map.
                File pictureFile;
                char fileName[256];
                char message[256];
                std::snprintf(fileName, sizeof(fileName), "%s%s", artPath, "features.tga");

                if (pictureFile.open(fileName) != 0)
                {
                    MCStrCopy(fileName, "features.tga");

                    if (pictureFile.open(fileName) != 0)
                    {
                        std::snprintf(message, sizeof(message), "Error reading '%s'", fileName);
                        GeneralMsg(message);
                    }
                }

                const uint32_t size = pictureFile.fileSize();

                if (size == 0)
                {
                    std::snprintf(message, sizeof(message), "Error reading '%s'", fileName);
                    GeneralMsg(message);
                }

                auto* picture = static_cast<uint8_t*>(guiHeap->malloc(size));

                if (picture == nullptr)
                {
                    return 0;
                }

                pictureFile.read(picture, static_cast<int32_t>(size));
                pictureFile.close();
                // The TGA's colour map starts at +0x12, as 256 BGR triples of 8-bit components.
                auto* palette = static_cast<uint8_t*>(guiHeap->malloc(0x300));

                for (int32_t i = 0; i < 0x100; i++)
                {
                    palette[i * 3 + 0] = picture[0x12 + i * 3 + 2] >> 2;
                    palette[i * 3 + 1] = picture[0x12 + i * 3 + 1] >> 2;
                    palette[i * 3 + 2] = picture[0x12 + i * 3 + 0] >> 2;
                }

                guiHeap->free(picture);
                application->showCursor(0);
                featureScreen = new aObject;
                featureScreen->init(0, 0, 640, 480, nullptr);
                // The picture's port is never freed in MCX.EXE.
                auto* picturePort = new aPort;
                picturePort->init(const_cast<char*>("features.tga"));
                picturePort->copyTo(featureScreen->port()->frame(), 0, 0, 0);
                screenWindow->addChild(featureScreen);
                featureScreen->ShowGUIWindow(1);
                application->activatePalette(palette, 0, 0x100);
                guiHeap->free(palette);
            }

            if (featureScreenDone != 0)
            {
                screenWindow->removeChild(featureScreen);
                delete featureScreen;
                featureScreen = nullptr;
                missionState = 16;
            }
            break;
        }
    }

    if (resetDisplay)
    {
        resetFullScreenDisplay();
        toLogistics = true;
    }

    if (toLogistics)
    {
        missionState = 3;

        if (globalGameSegment == 0)
        {
            logistics->getCurrentMission();
        }
    }

    if (soundSystem != nullptr && (scenario == nullptr || EventsToMissionResultsScreen != 0))
    {
        soundSystem->update();
    }

    return 0;
}

auto Mission::StartScenario(char* scenarioName) -> void
{
    if (Solo == 0 && MPlayer == nullptr)
    {
        checkForCDInDrive(CurPlanet, false);
    }

    soundSystem->playBettySample(0x13);
    endScenarioRequested = 0;
    resultsScreen = new MissionResultsScreen;
    resultsScreen->init();

    if (globalGameSegment == 0)
    {
        globalLogPtr->prepareScenario(scenarioName, const_cast<char*>("bridge"));
    }

    if (logistics != nullptr)
    {
        deleteLogistics(logistics);
        globalLogPtr = nullptr;
        logistics = nullptr;
    }

    //---------------------------------------------------------------------------------------------------------------
    // How long logistics took: the time of day of (end - start) as a FILETIME, so whole days are dropped.
    MCPort::GetSystemTime(logisticsEnd);
    const uint64_t startTicks = MCPort::SystemTimeToFileTime(logisticsStart);
    const uint64_t endTicks = MCPort::SystemTimeToFileTime(logisticsEnd);
    SYSTEMTIME elapsed{};
    MCPort::FileTimeToSystemTime(endTicks - startTicks, elapsed);
    totalLogisticsTime = static_cast<float>(elapsed.wMinute) * 60.0f + static_cast<float>(elapsed.wHour) * 3600.0f +
                         static_cast<float>(elapsed.wSecond) + static_cast<float>(elapsed.wMilliseconds / 1000);

    gamePalette->activate(0, 0);
    InitAlphaLookup(reinterpret_cast<VFX_RGB*>(gamePalette->rgbData));
    application->paletteCycle = 1;
    application->showCursor(0);

    scenarioCallback = new aCallback;

    if (scenarioCallback == nullptr)
    {
        Fatal(0, " No RAM for scenario Callback");
    }

    scenario = new Scenario;

    if (scenario == nullptr)
    {
        Fatal(0, " No RAM for scenario");
    }

    if (globalGameSegment == 0)
    {
        scenarioName = const_cast<char*>("bridge");
    }

    // Port: the scenario's windows are made at the window's size.
    MCFollowWindowSize();
    const int32_t result = scenario->init(scenarioName, nullptr);

    if (result != 0)
    {
        Fatal(result, " Couldnt init scenario ");
    }

    theInterface->StartScenario();
    loadWindowStatus();
    StartingResourcePoints = ResourcePoints;
    scenarioResult = 0;
    missionState = 7;
    waitTimer = waitTime;
    aRedrawScreen();
    scenarioCallback->setExec(playScenario);
    application->addCallback(scenarioCallback);

    if (interfaceUpdateCallback == nullptr)
    {
        interfaceUpdateCallback = new aCallback;

        if (interfaceUpdateCallback == nullptr)
        {
            Fatal(0, " No RAM for interface Callback");
        }
    }

    interfaceUpdateCallback->setExec(UpdateMouseStateCallback);
    application->addCallback(interfaceUpdateCallback);
}

auto Mission::EndScenario() -> void
{
    totalScenarioTime = scenarioTime;

    // MCX.EXE also copies the scenario's script name to an unused local when playing solo (after the CD check).
    if (Solo == 0 && MPlayer == nullptr)
    {
        checkForCDInDrive(CurPlanet, false);
    }

    if (globalGameSegment == 0 && MPlayer == nullptr)
    {
        char bridgeName[256];
        std::snprintf(bridgeName, sizeof(bridgeName), "%s", scenario->scenarioScript);
        MissionLogisticsBridge bridge;
        const int32_t result = bridge.missionResultsStartingFitWriter(bridgeName);

        if (result != 0)
        {
            Fatal(result, " Unable to write Mission to Logistics Bridge File ");
        }
    }

    endScenarioRequested = 0;

    if (setupCallback != nullptr)
    {
        delete setupCallback;
        setupCallback = nullptr;
    }

    application->removeCallback(scenarioCallback);
    delete scenarioCallback;
    scenarioCallback = nullptr;
    theInterface->EndScenario();
    somethingOnFire = 0;

    if (soundSystem != nullptr)
    {
        soundSystem->purgeSoundSystem();
    }

    // The result is thrown away (the results screen already counted the points).
    scenario->calcResourcePointsEarned();

    if (scenario != nullptr)
    {
        scenario->destroy();
        delete scenario;
        scenario = nullptr;
    }

    if (scenarioResult > 3)
    {
        if (Solo != 0)
        {
            // A won solo (quick-start) mission: back to the main screen.
            Logistics* newLogistics = startLogistics(this);
            LastLogisticsMissionState = 0;
            missionState = 3;
            newLogistics->currentScreen->ShowGUIWindow(0);
            newLogistics->currentScreen = newLogistics->mainScreen;
            newLogistics->logisticsState = 1;
            newLogistics->showLogScreen(1, 1);
            Solo = 0;
            return;
        }

        if (static_cast<uint32_t>(currentScenario) == lastScenario)
        {
            gameOver = 1;
        }
    }

    if (scenarioResult < 3 && Solo != 0)
    {
        // A lost solo mission: restart the campaign at its first briefing.
        Logistics* newLogistics = startLogistics(this);
        LastLogisticsMissionState = 0;
        missionState = 3;
        Solo = 1;
        newLogistics->currentMission = 0;
        currentScenario = 0;
        currentMovie = 1;
        char startName[] = "start1";
        char extension[] = ".pkk";
        newLogistics->loadCampaign(startName, extension, 1, 0);
        newLogistics->setUpBriefingScreen(0);
        newLogistics->showLogScreen(1, 0);
        return;
    }

    if (globalGameSegment == 0 && gameOver == 0)
    {
        Logistics* newLogistics = startLogistics(this);

        if (MPlayer == nullptr)
        {
            // The campaign: a win moves on to the next mission, anything else replays this one.
            int32_t missionId = currentScenario;

            if (scenarioResult < 4)
            {
                newLogistics->currentMission = missionId;
            }
            else
            {
                missionId++;
                newLogistics->currentMission = missionId;
                currentScenario = missionId;
            }

            currentMovie = missionId + 1;
            newLogistics->getCurrentMission();
            const bool replay = scenarioResult < 4;
            char startName[32];
            std::snprintf(startName, sizeof(startName), "start%d",
                          replay ? newLogistics->currentMission + 1 : newLogistics->currentMission);
            char extension[] = ".pkk";
            newLogistics->loadCampaign(startName, extension, replay ? 1 : 0, 0);
            newLogistics->setUpBriefingScreen(0);
            newLogistics->showLogScreen(1, 0);
            return;
        }

        LastLogisticsMissionState = 0;
        MPlayer->chatCallback = LogisticsChatCallback;
        missionState = 3;
        newLogistics->currentScreen->ShowGUIWindow(0);
        newLogistics->currentScreen = newLogistics->mainScreen;
        newLogistics->logisticsState = 1;
        newLogistics->showLogScreen(1, 1);

        if (MPlayer->inMission != 0)
        {
            if (isMPlayerGame != 0)
            {
                killTheGame();
            }

            delete MPlayer;
            MPlayer = nullptr;
        }
    }
    else
    {
        missionState = 16;
        waitTimer = waitTime;
        currentScenario = -1;
    }
}

auto Mission::saveWindowStatus() -> void
{
    FitIniFile windowFile;
    // windows.tmp in the original: under the process ID, as copies of the game on one machine share the user folder
    // and leave a multiplayer mission together.
    char tempName[32];
    std::snprintf(tempName, sizeof(tempName), "windows.%u.tmp", MCPort::ProcessId());

    if (windowFile.open(tempName, CREATE) != 0)
    {
        return;
    }

    windowFile.writeBlock("Info");

    if (mainHolder->GetPane(0) != nullptr && mainHolder->GetPane(0)->GetCamera() != nullptr)
    {
        windowFile.writeIdBoolean("MainZoomed", mainHolder->GetPane(0)->GetCamera()->cameraScale != 100);
    }

    windowFile.writeIdBoolean("TacHidden", theInterface->tacticalMap->IsHidden());
    windowFile.writeIdBoolean("ShowPalette", theInterface->tacticalMap->paletteFrame->IsShowing());
    windowFile.close();
    MCFileSystem::RemoveFile("windows.fit");
    MCFileSystem::RenameFile(tempName, "windows.fit");
}

auto Mission::loadWindowStatus() -> void
{
    FitIniFile windowFile;
    mainHolder->SetTiled(0);
    theInterface->tacticalMap->ShowGUIWindow(1);
    mainHolder->ZoomActivePane();
    theInterface->tacticalMap->HideMe(0);

    if (windowFile.open("windows.fit") != 0)
    {
        return;
    }

    if (windowFile.seekBlock("Info") == 0)
    {
        int showPalette = 0;

        if (windowFile.readIdBoolean("ShowPalette", showPalette) != 0)
        {
            showPalette = 1;
        }

        theInterface->tacticalMap->paletteFrame->ShowGUIWindow(showPalette);
    }

    windowFile.close();
}

auto aOpeningSmackerWindow::init(tagRECT* frame, tagPOINT* position) -> int32_t
{
    wipeLine = 0;
    application->openingSmackerWindow = this;
    return aSmackerWindow::init(frame, position);
}

auto aOpeningSmackerWindow::display() -> void
{
    if (showWindow == 0 || (IsHidden() != 0 && hideOffset == 0))
    {
        return;
    }

    if (wipeLine < 1)
    {
        aSmackerWindow::display();
    }
    else
    {
        VFX_pane_copy(moviePane, 0, 0, frame(), 0, wipeLine, -1);
        wipeLine += 20;
    }

    if (height() + 20 <= wipeLine)
    {
        destroy();
        delete this;
    }
}

auto aOpeningSmackerWindow::endSmackerMovie() -> void
{
    SmackClose(movie);
    movie = nullptr;
    wipeLine++;
    application->openingSmackerWindow = nullptr;
}

auto aOpeningSmackerWindow::escapeSmackerMovie() -> void
{
    wipeLine += height();
    endSmackerMovie();
    application->smackerWindow->destroy();

    if (application->smackerWindow != nullptr)
    {
        delete application->smackerWindow;
    }

    application->smackerWindow = nullptr;
}

auto ComparePilots(const void* a, const void* b) -> int
{
    const int32_t keyA = static_cast<const MissionPilotResult*>(a)->sortKey;
    const int32_t keyB = static_cast<const MissionPilotResult*>(b)->sortKey;

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
    const int32_t valueA = static_cast<const MissionCommanderScore*>(a)->score;
    const int32_t valueB = static_cast<const MissionCommanderScore*>(b)->score;

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

auto moveOnButtonHandleEvent(aObject*, aEvent* event) -> void
{
    if (event->type == 4)
    {
        MissionResultsScreen* screen = mission->resultsScreen;

        if (screen != nullptr)
        {
            screen->destroy();
            delete screen;
            mission->resultsScreen = nullptr;
        }
    }
}

auto PilotSwitchHandleEvent(aObject* object, aEvent* event) -> void
{
    if (event->type == 1)
    {
        mission->resultsScreen->drawMPPilots(static_cast<aToolButton*>(object)->pushed == 0);
    }
}

MissionResultsScreen::~MissionResultsScreen()
{
    destroy();
}

namespace
{
    /// <summary>Removes <paramref name="child"/> from <paramref name="parent"/>, then destroys and deletes it.</summary>
    template <typename T> auto deleteChild(aObject* parent, T*& child) -> void
    {
        if (child == nullptr)
        {
            return;
        }

        parent->removeChild(child);
        child->destroy();
        delete child;
        child = nullptr;
    }

    /// <summary>Destroys and deletes <paramref name="port"/>.</summary>
    auto deletePort(aPort*& port) -> void
    {
        if (port == nullptr)
        {
            return;
        }

        port->destroy();
        delete port;
        port = nullptr;
    }

    /// <summary>
    /// Moves the edges of <paramref name="icon"/>'s pane in by <paramref name="inset"/> (out when negative), so the
    /// results screen copies the pilot picture without the icon's frame.
    /// </summary>
    auto insetIconPane(FriendlyMechIcon* icon, int32_t inset) -> void
    {
        _pane* pane = icon->port()->frame();
        pane->x0 += inset;
        pane->y0 += inset;
        pane->x1 -= inset;
        pane->y1 -= inset;
    }

    /// <summary>The length in pixels of a skill bar for <paramref name="skill"/> (55 across the skill range).</summary>
    auto skillBarLength(int32_t skill) -> int32_t
    {
        const int32_t aboveMinimum = static_cast<int32_t>(skill - static_cast<double>(MinPilotSkill));
        return static_cast<int32_t>(static_cast<double>(aboveMinimum * 55) /
                                    (static_cast<double>(MaxPilotSkill) - MinPilotSkill));
    }

    /// <summary>Whether the home side lost a multiplayer game with result <paramref name="result"/>.</summary>
    auto homeSideLost(uint32_t result) -> bool
    {
        return result == 3 || (result == 1 && homeTeam->alignment == -1) || (result == 2 && homeTeam->alignment == 1);
    }

    /// <summary>The kills of <paramref name="warrior"/>, all kinds together.</summary>
    auto totalKills(const MechWarrior* warrior) -> int32_t
    {
        int32_t kills = 0;

        for (int32_t kind = 0; kind < 7; kind++)
        {
            kills += warrior->numKilled[kind][1];
        }

        return kills;
    }
}

auto MissionResultsScreen::init() -> int32_t
{
    int32_t result = aObject::init(0x28, 0xf, 0x230, 0x1bc, nullptr);
    Assert(result == 0, static_cast<uint32_t>(result), " error initializing mission results screen display elements ");
    char* backgroundName = MPlayer == nullptr ? const_cast<char*>("mr_bkgd.tga") : const_cast<char*>("mrm_bkgd.tga");
    result = port()->init(backgroundName);
    Assert(result == 0, static_cast<uint32_t>(result), " error initializing mission results screen display elements ");

    moveOnPort = new aPort();
    Assert(moveOnPort != nullptr, static_cast<uint32_t>(result), " not enough memory to init mission results screen ");
    // The original drops this init's result and asserts the previous one again.
    moveOnPort->init(const_cast<char*>("mr_ms00.tga"));
    Assert(result == 0, static_cast<uint32_t>(result), " error initializing mission results screen display elements ");

    moveOnButton = new aButton();
    Assert(moveOnButton != nullptr, static_cast<uint32_t>(result),
           " not enough memory to init mission results screen ");
    moveOnButton->init(0x1bc, 6, 0x6b, 0x12, nullptr);
    moveOnButton->setUpPicture(const_cast<char*>("mr_ms01.tga"));
    moveOnButton->setDownPicture(const_cast<char*>("mr_ms02.tga"));
    addChild(moveOnButton);
    moveOnButton->setEventRoutine(moveOnButtonHandleEvent);
    moveOnButton->SetTransparent(1);

    if (MPlayer != nullptr)
    {
        auto* switchButton = new aToolButton();
        pilotSwitchButton = switchButton;
        Assert(switchButton != nullptr, static_cast<uint32_t>(result),
               " not enough memory to init mission results screen ");
        switchButton->init(0xe4, 0x1e, 0x148, 0xb, nullptr);
        switchButton->setUpPicture(const_cast<char*>("mrm_bkgd00.tga"));
        switchButton->setDownPicture(const_cast<char*>("mrm_bkgd01.tga"));
        switchButton->framed = 0;
        switchButton->draw();
        switchButton->draw();
        addChild(switchButton);
        pilotSwitchButton->setEventRoutine(PilotSwitchHandleEvent);
    }

    successPort = new aPort();
    Assert(successPort != nullptr, static_cast<uint32_t>(result), " not enough memory to init mission results screen ");
    failurePort = new aPort();
    Assert(failurePort != nullptr, static_cast<uint32_t>(result), " not enough memory to init mission results screen ");
    result = successPort->init(const_cast<char*>("guimr08.tga"));
    Assert(result == 0, static_cast<uint32_t>(result), " error initializing mission results screen display elements ");
    result = failurePort->init(const_cast<char*>("guimr07.tga"));
    Assert(result == 0, static_cast<uint32_t>(result), " error initializing mission results screen display elements ");

    nextDrawTime = 0;

    if (MPlayer == nullptr)
    {
        scrollUpButton = new aObject();
        scrollUpButton->init(0, 0, 0xb, 0xb, nullptr);
        scrollUpButton->port()->init(const_cast<char*>("mfddbg04.tga"));
        scrollUpButton->ShowGUIWindow(0);
        addChild(scrollUpButton);

        scrollDownButton = new aObject();
        scrollDownButton->init(0, 0, 0xb, 0xb, nullptr);
        scrollDownButton->port()->init(const_cast<char*>("mfddbg05.tga"));
        scrollDownButton->ShowGUIWindow(0);
        addChild(scrollDownButton);

        scrollUpRect = {0xcf, 0x15b, 0xda, 0x166};
        scrollUpButton->moveTo(0xcf, 0x15b, 0);
        scrollDownRect = {0xcf, 0x1a7, 0xda, 0x1b2};
        scrollDownButton->moveTo(0xcf, 0x1a7, 0);
        scrollBarRect = {0xcf, 0x169, 0xda, 0x1a4};
    }

    return result;
}

auto MissionResultsScreen::destroy() -> void
{
    EventsToMissionResultsScreen = 0;
    ::operator delete(pilotResults);
    pilotResults = nullptr;
    deletePort(successPort);
    deletePort(failurePort);
    deleteChild(this, moveOnButton);
    deleteChild(this, scrollUpButton);
    deleteChild(this, scrollDownButton);
    deleteChild(this, pilotSwitchButton);
    // The debriefing text box belongs to the tactical map; it is only taken off the screen.
    removeChild(textObject);
    aObject::destroy();

    application->cursorHidden = 0;

    if (scenarioEnded == 0 && scenario != nullptr)
    {
        scenarioEnded = 1;
        mission->EndScenario();

        if (globalGameSegment == 0)
        {
            if (gameOver == 0)
            {
                gamePaused = 0;
                mission->missionState = 3;
                return;
            }

            if (InDemo != 0)
            {
                gamePaused = 0;
                mission->currentMovie = 2;
                mission->missionState = 0x14;
                nextGameState = 0x14;
                return;
            }

            mission->currentMovie = 3;
            mission->missionState = 10;
            nextGameState = 10;
        }

        gamePaused = 0;
    }
}

auto MissionResultsScreen::MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) -> bool
{
    if (MPlayer != nullptr || textObject == nullptr)
    {
        return false;
    }

    return textObject->MouseWheel(steps, xPos, yPos);
}

auto MissionResultsScreen::handleEvent(aEvent* event) -> void
{
    aObject::handleEvent(event);

    switch (event->type)
    {
        case 1:
        {
            if (MPlayer == nullptr)
            {
                const POINT point{event->x - globalX(), event->y - globalY()};

                if (PtInRect(&scrollUpRect, point))
                {
                    application->grab(this);
                    scrollUpButton->ShowGUIWindow(1);
                    application->AddTimer(this, 4, theInterface->scrollStart, 0, 0, 0);
                    textObject->ReceiveClick(-1, 0);
                    return;
                }

                if (PtInRect(&scrollDownRect, point))
                {
                    application->grab(this);
                    scrollDownButton->ShowGUIWindow(1);
                    application->AddTimer(this, 4, theInterface->scrollStart, 0, 0, 0);
                    textObject->ReceiveClick(1, 0);
                    return;
                }

                if (PtInRect(&scrollBarRect, point))
                {
                    // Original behaviour (OB-057): the line is picked from the cursor's screen y, not its window y, so
                    // the click lands 15 pixels (the window's y) further down the text.
                    textObject->ReceiveClick(0, event->y - scrollBarRect.top);
                    return;
                }
            }
            break;
        }
        case 4:
        {
            application->RemoveTimer(this, 4);
            application->RemoveTimer(this, 5);
            application->release();

            if (scrollUpButton != nullptr)
            {
                scrollUpButton->ShowGUIWindow(0);
            }

            if (scrollDownButton != nullptr)
            {
                scrollDownButton->ShowGUIWindow(0);
            }
            break;
        }
        case 10:
        {
            if (event->key == 0x1b)
            {
                skipAnimation = 1;
                return;
            }
            break;
        }
        case 0x13:
        {
            const int32_t timerId = event->data;

            if (timerId == 4)
            {
                // The first repeat delay has passed: repeat five times as fast.
                application->RemoveTimer(this, 4);
                application->AddTimer(this, 5, theInterface->scrollStart / 5, 0, 0, 0);
            }
            else if (timerId != 5)
            {
                if (timerId != 10)
                {
                    return;
                }

                // The multiplayer timeout: close the screen (this object is deleted here).
                MissionResultsScreen* screen = mission->resultsScreen;

                if (screen == nullptr)
                {
                    return;
                }

                screen->destroy();
                delete screen;
                mission->resultsScreen = nullptr;
                return;
            }

            const POINT point{event->x - globalX(), event->y - globalY()};

            if (PtInRect(&scrollUpRect, point))
            {
                textObject->ReceiveClick(-1, 0);
                return;
            }

            if (PtInRect(&scrollDownRect, point))
            {
                textObject->ReceiveClick(1, 0);
                return;
            }
            break;
        }

        default:
            break;
    }
}

auto MissionResultsScreen::display() -> void
{
    uint8_t* hazePalette = gamePalette->getHazePalette(-7);
    SCRNVERTEX vertices[4] = {};
    vertices[1].x = application->width() - 1;
    vertices[2].x = application->width() - 1;
    vertices[2].y = application->height() - 1;
    vertices[3].y = application->height() - 1;
    VFX_translate_polygon(screenPort->frame(), 4, vertices, hazePalette);

    if (MPlayer == nullptr)
    {
        while (nextDrawTime != 0 && (nextDrawTime <= MouseTicks || skipAnimation != 0))
        {
            switch (drawState)
            {
                case 0:
                {
                    if (scenarioResult < 4 || Solo != 0)
                    {
                        drawState = 1;
                    }
                    else
                    {
                        drawRPs();
                    }
                    break;
                }
                case 1:
                    drawStats();
                    break;
                case 2:
                case 3:
                    drawObjectives();
                    break;
                case 4:
                    drawPilots();
                    break;
                case 5:
                {
                    aScrollTextObject* text = textObject;
                    text->ShowGUIWindow(1);

                    if (text->textBuffer == nullptr || text->textBuffer[0] == '\0')
                    {
                        char line[256];
                        cLoadString(thisInstance, 0x361, line, 0xfe);
                        text->fontIndex = 1;
                        text->Print(line, 0x1f);
                    }

                    text->draw();
                    nextDrawTime = 0;
                    drawState = 6;

                    if (skipAnimation == 0)
                    {
                        soundSystem->playDigitalSample(0x32, 1, nullptr, 0, 0);
                    }
                    break;
                }

                default:
                    break;
            }
        }
    }

    if (displayPort != nullptr)
    {
        displayPort->copyTo(framePane, 0, 0, 0);
    }

    for (int32_t i = 0; i < numChildren; i++)
    {
        childList[i]->display();
    }
}

auto MissionResultsScreen::drawRPs() -> void
{
    char text[256];
    FillBox(0x149, 9, 0x185, 0x14, 0x10);
    const int32_t shown = drawIndex * 1000;

    if (resourcePointsEarned < shown)
    {
        std::snprintf(text, sizeof(text), "%i", resourcePointsEarned);
        drawIndex = 0;
        drawState++;
        nextDrawTime = resultsStepTicks + MouseTicks;

        if (skipAnimation == 0)
        {
            soundSystem->playDigitalSample(0x43, 1, nullptr, 0, 0);
        }
    }
    else
    {
        std::snprintf(text, sizeof(text), "%i", shown);
        nextDrawTime = resultsStepTicks / 20 + MouseTicks;

        if (skipAnimation == 0)
        {
            soundSystem->playDigitalSample(0x42, 1, nullptr, 0, 0);
        }

        drawIndex++;
    }

    lgWhiteFont->writeString(port()->frame(), 0x14a, 10, reinterpret_cast<uint8_t*>(text), -1);
}

auto MissionResultsScreen::drawStats() -> void
{
    static constexpr int32_t statY[4] = {0x33, 0x40, 0x4d, 0x5a};
    char text[8];
    nextDrawTime = resultsStepTicks / 2 + MouseTicks;

    switch (drawIndex)
    {
        case 0:
        case 1:
        case 2:
        case 3:
        {
            const int32_t values[4] = {enemyMechsHit, enemyMechsDestroyed, enemyPilotsKilled, playerMechsHit};
            std::snprintf(text, sizeof(text), "%i", values[drawIndex]);
            lgWhiteFont->writeString(port()->frame(), 0xcc, statY[drawIndex], reinterpret_cast<uint8_t*>(text), -1);
            drawIndex++;
            break;
        }

        case 4:
        {
            std::snprintf(text, sizeof(text), "%i", playerMechsDestroyed);
            lgWhiteFont->writeString(port()->frame(), 0xcc, 0x67, reinterpret_cast<uint8_t*>(text), -1);
            drawIndex = 0;
            drawState++;
            nextDrawTime = resultsStepTicks + MouseTicks;
            break;
        }
        default:
            break;
    }

    if (skipAnimation == 0)
    {
        soundSystem->playDigitalSample(0x47, 1, nullptr, 0, 0);
    }
}

auto MissionResultsScreen::drawObjectives() -> void
{
    char text[256];
    char pointsName[256];
    char bonusHeader[256];
    char secondaryHeader[256];
    // drawState 2 lists the primary objectives (type 0), 3 the secondary ones (type 1).
    const uint32_t wantedType = drawState != 2 ? 1 : 0;
    int drew = 0;
    cLoadString(thisInstance, 0x363, pointsName, 0xfe);
    cLoadString(thisInstance, 0x360, bonusHeader, 0xfe);
    cLoadString(thisInstance, 0x362, secondaryHeader, 0xfe);

    ScenarioObjective& objective = scenario->objectives[drawIndex];

    if (objective.type == wantedType)
    {
        drew = 1;

        if (wantedType == 1 && objectivesHeaderDrawn == 0)
        {
            medBlueFont->writeString(port()->frame(), 0xf, drawY, reinterpret_cast<uint8_t*>(secondaryHeader), -1);
            objectivesHeaderDrawn = 1;
            drawY += 0xe;
        }

        aFont* font = greyFont;
        bool listed = true;

        if (objective.status == 1)
        {
            successPort->copyTo(port()->frame(), 0xb, drawY - 1, 0);
            font = greenFont;
        }
        else if (objective.status == 2)
        {
            failurePort->copyTo(port()->frame(), 0xb, drawY - 1, 0);
            font = redFont;
        }
        else if (objective.status != 0)
        {
            listed = false;
        }

        if (listed && font != nullptr)
        {
            font->writeString(port()->frame(), 0x17, drawY, reinterpret_cast<uint8_t*>(objective.name), -1);
            const int32_t nameY = drawY;
            drawY = nameY + 10;

            if (MPlayer == nullptr && objective.points != 0)
            {
                std::snprintf(text, sizeof(text), "%i %s", objective.points, pointsName);
                font->writeString(port()->frame(), 0x1d, nameY + 10, reinterpret_cast<uint8_t*>(text), -1);
                drawY += 10;
            }
            else
            {
                drawY = nameY + 0xd;
            }
        }
    }

    if (drawIndex == static_cast<int32_t>(scenario->numObjectives))
    {
        // After the secondary objectives: the tonnage bonus, when it was earned.
        ScenarioObjective& bonus = scenario->objectives[scenario->numObjectives];

        if (MPlayer == nullptr && drawState == 3 && bonus.type == 3 && bonus.points > 0 && scenarioResult > 3 &&
            Solo == 0)
        {
            soundSystem->playBettySample(10);
            blueFont->writeString(port()->frame(), 0xf, drawY, reinterpret_cast<uint8_t*>(bonusHeader), -1);
            const int32_t markY = drawY + 10;
            drawY += 0xb;
            successPort->copyTo(port()->frame(), 0xb, markY, 0);
            greenFont->writeString(port()->frame(), 0x17, drawY, reinterpret_cast<uint8_t*>(bonus.name), -1);
            const int32_t nameY = drawY;
            drawY = nameY + 10;
            std::snprintf(text, sizeof(text), "%i %s", bonus.points, pointsName);
            greenFont->writeString(port()->frame(), 0x1d, nameY + 10, reinterpret_cast<uint8_t*>(text), -1);
            drew = 1;
            drawY += 0xb;
        }

        drawIndex = 0;
        drawState++;
    }
    else
    {
        drawIndex++;
    }

    if (drew != 0 && MPlayer == nullptr)
    {
        if (skipAnimation == 0)
        {
            soundSystem->playDigitalSample(0x47, 1, nullptr, 0, 0);
        }

        nextDrawTime = resultsStepTicks + MouseTicks;
    }
}

auto MissionResultsScreen::drawPilots() -> void
{
    const int32_t index = drawIndex;
    nextDrawTime = resultsStepTicks + MouseTicks;

    if (index == numPilotResults)
    {
        drawIndex = 0;
        drawState++;
        return;
    }

    MechWarrior* warrior = pilotResults[index].warrior;

    if (warrior != nullptr)
    {
        const auto line = [this](int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t color)
        { VFX_line_draw(port()->frame(), x0, y0, x1, y1, LD_DRAW, color); };
        const auto pixel = [this](int32_t x, int32_t y) { AG_pixel_write(port()->frame(), x, y, 0x10); };

        int32_t left;
        int32_t top;

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

        char text[256];
        uint32_t stringId;

        if (warrior->status == 4)
        {
            stringId = 0x356;
        }
        else if (warrior->wounds <= 4.0f)
        {
            stringId = 0x358;
        }
        else
        {
            stringId = 0x357;
        }

        cLoadString(thisInstance, stringId, text, 0xfe);
        const int32_t textY = top + 3;
        whiteFont->writeString(port()->frame(), left + 3, textY, reinterpret_cast<uint8_t*>(text), -1);

        // The rank; a rank outside 0..3 leaves the status text in the buffer.
        const int32_t rank = static_cast<int8_t>(warrior->rank);

        if (rank >= 0 && rank <= 3)
        {
            cLoadString(thisInstance, 0x70 + static_cast<uint32_t>(rank), text, 0xfe);
        }

        aFont* rankFont = pilotResults[drawIndex].oldRank < rank ? yellowFont : whiteFont;
        rankFont->writeString(port()->frame(), left + 0x39, textY, reinterpret_cast<uint8_t*>(text), -1);

        std::snprintf(text, sizeof(text), "%i", totalKills(warrior));
        whiteFont->writeString(port()->frame(), left + 0x8c, textY, reinterpret_cast<uint8_t*>(text), -1);

        // A bar per skill: the old value, and the gain in another colour.
        int32_t barY = top + 0x18;

        for (const int32_t skill : StevesOrderLUT)
        {
            const int32_t oldLength = skillBarLength(pilotResults[drawIndex].skills[skill]);
            const int32_t newLength = skillBarLength(static_cast<int32_t>(warrior->skillRank[skill]));
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

        FriendlyMechIcon* icon = theInterface->GetMechIconFromID(warrior->vehicle->partId);

        if (icon != nullptr)
        {
            const int32_t status = warrior->status;

            if (status == 3)
            {
                icon->pilotImage->init(warrior->picture);
            }

            icon->draw();
            insetIconPane(icon, 2);
            icon->port()->copyTo(port()->frame(), left + 4, top + 0x10, 0);
            insetIconPane(icon, -2);

            if (status == 3)
            {
                icon->pilotImage->init(4);
                icon->draw();
            }
        }

        drawIndex++;
    }

    if (skipAnimation == 0)
    {
        soundSystem->playDigitalSample(0x10, 1, nullptr, 0, 0);
    }
}

auto MissionResultsScreen::drawMPPilots(int showHomeSide) -> void
{
    const auto line = [this](int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t color)
    { VFX_line_draw(port()->frame(), x0, y0, x1, y1, LD_DRAW, color); };
    const auto pixel = [this](int32_t x, int32_t y) { AG_pixel_write(port()->frame(), x, y, 0x10); };

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

    for (int32_t i = 0; i < numPilotResults; i++)
    {
        MechWarrior* warrior = pilotResults[i].warrior;

        if (warrior == nullptr)
        {
            continue;
        }

        const bool homeSide = warrior->alignment == homeTeam->alignment;

        if (homeSide != (showHomeSide != 0))
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
        auto* mover = static_cast<Mover*>(warrior->vehicle);
        whiteFont->writeString(port()->frame(), left + 3, top + 2, reinterpret_cast<uint8_t*>(mover->netName), -1);
        std::snprintf(text, sizeof(text), "%i", totalKills(warrior));
        whiteFont->writeString(port()->frame(), left + 0x8c, top + 3, reinterpret_cast<uint8_t*>(text), -1);

        int32_t barY = top + 0x17;

        for (const int32_t skill : StevesOrderLUT)
        {
            const int32_t length = skillBarLength(static_cast<int32_t>(warrior->skillRank[skill]));
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

        FriendlyMechIcon* icon = theInterface->GetMechIconFromID(mover->partId);

        if (icon != nullptr)
        {
            if (warrior->status == 3)
            {
                if (showHomeSide == 0)
                {
                    icon->draw();
                }

                icon->pilotImage->init(warrior->picture);
            }

            icon->draw();
            insetIconPane(icon, 2);
            icon->port()->copyTo(port()->frame(), left + 4, top + 0x10, 0);
            insetIconPane(icon, -2);

            if (warrior->status == 3)
            {
                icon->pilotImage->init(4);
                icon->draw();
            }
        }

        rowBase += 0x42;
    }
}

auto MissionResultsScreen::activate() -> int32_t
{
    EventsToMissionResultsScreen = 1;
    skipAnimation = 0;
    application->removeCallback(interfaceUpdateCallback);
    delete interfaceUpdateCallback;
    interfaceUpdateCallback = nullptr;
    theInterface->HideTags();
    application->removeCallback(scenarioCallback);
    delete scenarioCallback;
    scenarioCallback = nullptr;
    application->cursorHidden = 0;
    application->SetCurrentCursor(static_cast<CursorType>(0));
    application->cursorHidden = 1;
    application->release();

    drawY = 0x92;
    drawIndex = 0;
    drawState = 0;
    nextDrawTime = MouseTicks;
    screenWindow->addChild(this);
    setDepth(0x5f);

    // The move-on button's label: "mission failed" when the scenario (or the home side) lost.
    if (MPlayer == nullptr)
    {
        if (scenarioResult < 3)
        {
            moveOnPort->destroy();
            moveOnPort->init(const_cast<char*>("mr_mf00.tga"));
            moveOnButton->setUpPicture(const_cast<char*>("mr_mf01.tga"));
            moveOnButton->setDownPicture(const_cast<char*>("mr_mf02.tga"));
        }
    }
    else
    {
        moveOnPort->destroy();
        moveOnPort->init(homeSideLost(scenarioResult) ? const_cast<char*>("mr_mf00.tga")
                                                      : const_cast<char*>("mrm_ms00.tga"));
    }

    moveOnButton->draw();
    moveOnPort->copyTo(port()->frame(), 4, 4, 1);
    deletePort(moveOnPort);

    if (MPlayer == nullptr)
    {
        pilotResults = nullptr;
        playerMechsDestroyed = 0;
        playerMechsHit = 0;
        enemyPilotsKilled = 0;
        enemyMechsDestroyed = 0;
        enemyMechsHit = 0;

        BaseObject* current = nullptr;

        while (clanMechList->Traverse(current) != nullptr)
        {
            auto* object = static_cast<GameObject*>(current);

            if ((object->isDestroyed() || object->isDisabled()) && !object->isMarine())
            {
                enemyMechsHit++;

                if (object->objectClass == BATTLEMECH)
                {
                    enemyMechsDestroyed++;
                }
            }
        }

        current = nullptr;

        while (innerSphereMechList->Traverse(current) != nullptr)
        {
            auto* object = static_cast<GameObject*>(current);

            if ((object->isDestroyed() || object->isDisabled()) && !object->isMarine())
            {
                playerMechsHit++;

                if (object->objectClass == BATTLEMECH && static_cast<Mover*>(object)->netPlayerId != -1)
                {
                    playerMechsDestroyed++;
                }
            }
        }

        // Count the home side's pilots.
        numPilotResults = 0;

        for (uint32_t i = 1; i <= scenario->numWarriors; i++)
        {
            MechWarrior* warrior = scenario->warriors[i];

            if (warrior == nullptr)
            {
                continue;
            }

            auto* vehicle = static_cast<Mover*>(warrior->vehicle);

            if (vehicle != nullptr && vehicle->getAwake() && vehicle->objectClass == BATTLEMECH &&
                warrior->alignment == homeTeam->alignment && vehicle->netPlayerId != -1)
            {
                numPilotResults++;
            }
        }

        const size_t resultsSize = static_cast<size_t>(numPilotResults) * sizeof(MissionPilotResult);
        pilotResults = static_cast<MissionPilotResult*>(::operator new(resultsSize));
        std::memset(pilotResults, 0, resultsSize);

        // Fill the lines and apply the skill-ups.
        int32_t filled = 0;

        for (uint32_t i = 1; i <= scenario->numWarriors; i++)
        {
            MechWarrior* warrior = scenario->warriors[i];

            if (warrior == nullptr)
            {
                continue;
            }

            auto* vehicle = static_cast<Mover*>(warrior->vehicle);

            if (vehicle == nullptr || vehicle->objectClass != BATTLEMECH || !vehicle->getAwake())
            {
                continue;
            }

            if (!warrior->onHomeTeam())
            {
                if (warrior->status == 4 && warrior->alignment != homeTeam->alignment)
                {
                    enemyPilotsKilled++;
                }

                continue;
            }

            // Port fix: the fill loop doesn't test the network player id or the alignment the count did, so it can
            // find more pilots than it allocated for; the extra ones are left off the screen.
            if (filled >= numPilotResults)
            {
                continue;
            }

            MissionPilotResult& entry = pilotResults[filled];
            entry.warrior = warrior;

            if (vehicle->sensorSystem != nullptr)
            {
                warrior->skillPoints[MWS_SENSORS] =
                    static_cast<float>(vehicle->sensorSystem->totalContacts) * SensorSkill;
            }

            warrior->skillPoints[MWS_PILOTING] += warrior->skillRank[MWS_PILOTING];
            entry.oldRank = static_cast<int8_t>(warrior->rank);

            for (int32_t skill = 0; skill < 4; skill++)
            {
                float& skillRank = warrior->skillRank[skill];
                float& skillPoints = warrior->skillPoints[skill];
                int32_t gains = 0;
                entry.skills[skill] = static_cast<int32_t>(skillRank);

                if (scenarioResult > 3 && skillRank != 0.0f)
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

            warrior->calcRank();
            Assert(entry.oldRank <= static_cast<int8_t>(warrior->rank), 0, "Hey, how'd we drop in rank???");
            entry.sortKey = (3 - static_cast<int8_t>(warrior->rank)) * 10000;
            // Original behaviour (OB-058): meant as letter * 10^n, the callsign's letters are XORed with n.
            const char* callsign = warrior->callsign;

            for (int32_t letter = 0, power = 2; power >= 0; letter++, power--)
            {
                entry.sortKey += (callsign[letter] * 10) ^ power;
            }

            filled++;
        }

        numPilotResults = filled;
        std::qsort(pilotResults, static_cast<size_t>(numPilotResults), sizeof(MissionPilotResult), ComparePilots);

        // Only the first nine objectives' points count.
        resourcePointsEarned = 0;

        for (int32_t i = 0; i < 9; i++)
        {
            if (scenario->objectives[i].status == 1)
            {
                resourcePointsEarned += scenario->objectives[i].points;
            }
        }

        // Borrow the tactical map's text box for the debriefing.
        TacticalMap* tacMap = Terrain::terrainTacticalMap;
        textObject = nullptr;
        tacMap->removeChild(tacMap->salvageText);
        textObject = tacMap->salvageText;
        addChild(textObject);
        textObject->moveTo(10, 0x15b, 0);
        textObject->resize(0xc4, 0x59);
        textObject->ShowGUIWindow(0);

        soundSystem->playBettySample(scenarioResult < 4 ? 0xb : 0x12);
    }
    else
    {
        if (isMPlayerGame != 0)
        {
            application->AddTimer(this, 10, 90000, 0, 0, 0);
        }

        MissionCommanderScore scores[6];

        for (int32_t i = 0; i < 6; i++)
        {
            scores[i] = {i, -1};
        }

        playerMechsDestroyed = 0;
        playerMechsHit = 0;
        enemyPilotsKilled = 0;
        enemyMechsDestroyed = 0;
        enemyMechsHit = 0;

        numPilotResults = 0;

        for (uint32_t i = 1; i <= scenario->numWarriors; i++)
        {
            MechWarrior* warrior = scenario->warriors[i];

            if (warrior != nullptr && warrior->vehicle != nullptr && static_cast<Mover*>(warrior->vehicle)->getAwake())
            {
                numPilotResults++;
            }
        }

        const size_t resultsSize = static_cast<size_t>(numPilotResults) * sizeof(MissionPilotResult);
        pilotResults = static_cast<MissionPilotResult*>(::operator new(resultsSize));
        std::memset(pilotResults, 0, resultsSize);

        int32_t filled = 0;

        for (uint32_t i = 1; i <= scenario->numWarriors; i++)
        {
            MechWarrior* warrior = scenario->warriors[i];

            if (warrior == nullptr)
            {
                continue;
            }

            auto* vehicle = static_cast<Mover*>(warrior->vehicle);

            if (vehicle == nullptr || !vehicle->getAwake())
            {
                continue;
            }

            if (vehicle->isDisabled())
            {
                if (warrior->team == homeTeam)
                {
                    playerMechsHit++;

                    if (vehicle->objectClass == BATTLEMECH)
                    {
                        playerMechsDestroyed++;
                    }
                }
                else
                {
                    enemyMechsHit++;

                    if (vehicle->objectClass == BATTLEMECH)
                    {
                        enemyMechsDestroyed++;
                    }
                }
            }

            MissionPilotResult& entry = pilotResults[filled];
            entry.warrior = warrior;

            if (warrior->status == 4 && warrior->team != homeTeam)
            {
                enemyPilotsKilled++;
            }

            for (int32_t kind = 0; kind < 7; kind++)
            {
                MissionCommanderScore& score = scores[vehicle->getCommanderId()];

                if (score.score == -1)
                {
                    score.score = 0;
                }

                const int32_t kills = warrior->numKilled[kind][1];
                score.score += kills;
                entry.sortKey += kills * -10000;
                entry.oldRank += kills;
            }

            // Damage taken counts against the pilot: armor lost, and internal structure lost twice over.
            for (int32_t j = 0; j < vehicle->numArmorLocations; j++)
            {
                const ArmorLocation& armor = vehicle->armor[j];
                entry.sortKey =
                    static_cast<int32_t>(static_cast<double>(armor.maxArmor) - armor.curArmor + entry.sortKey);
            }

            for (int32_t j = 0; j < vehicle->numBodyLocations; j++)
            {
                const BodyLocation& body = vehicle->bodyAt(j);
                entry.sortKey = static_cast<int32_t>(
                    (static_cast<double>(body.maxInternalStructure) - body.curInternalStructure) * 2 + entry.sortKey);
            }

            filled++;
        }

        std::qsort(scores, 6, sizeof(MissionCommanderScore), CompareCommanders);
        Assert(numPilotResults == filled, static_cast<uint32_t>(numPilotResults), " warriorcount != warriorcount2! ");
        std::qsort(pilotResults, static_cast<size_t>(numPilotResults), sizeof(MissionPilotResult), ComparePilots);

        char text[256];

        // Port fix: MCX.EXE reads the best pilot without checking there is one.
        if (numPilotResults > 0)
        {
            // The best pilot.
            MechWarrior* best = pilotResults[0].warrior;
            auto* bestMover = static_cast<Mover*>(best->vehicle);
            medWhiteFont->writeString(port()->frame(), 0x70, 0x40, reinterpret_cast<uint8_t*>(bestMover->netName),
                                      0x68);
            cLoadString(thisInstance, best->alignment == homeTeam->alignment ? 0xb6 : 0xb7, text, 0xfe);
            medWhiteFont->writeString(port()->frame(), 0x70, 0x4d, reinterpret_cast<uint8_t*>(text), -1);
            std::snprintf(text, sizeof(text), "%i", pilotResults[0].oldRank);
            medWhiteFont->writeString(port()->frame(), 0xca, 0x4d, reinterpret_cast<uint8_t*>(text), -1);
            FriendlyMechIcon* icon = theInterface->GetMechIconFromID(bestMover->partId);

            if (icon != nullptr)
            {
                if (best->status == 3)
                {
                    icon->pilotImage->init(best->picture);
                }

                icon->draw();
                insetIconPane(icon, 2);
                icon->port()->copyTo(port()->frame(), 0xb, 0x2f, 0);

                if (best->status == 3)
                {
                    icon->pilotImage->init(4);
                    icon->draw();
                }

                insetIconPane(icon, -2);
            }
        }

        const int32_t statistics[5] = {enemyMechsHit, enemyMechsDestroyed, enemyPilotsKilled, playerMechsHit,
                                       playerMechsDestroyed};
        static constexpr int32_t statisticY[5] = {100, 0x70, 0x7c, 0x88, 0x94};

        for (int32_t i = 0; i < 5; i++)
        {
            std::snprintf(text, sizeof(text), "%i", statistics[i]);
            medWhiteFont->writeString(port()->frame(), 0xcc, statisticY[i], reinterpret_cast<uint8_t*>(text), -1);
        }

        const int32_t seconds = static_cast<int32_t>(std::fmod(static_cast<double>(actualTime), 60.0));
        std::snprintf(text, sizeof(text), "%02i:%02i", static_cast<int32_t>(actualTime) / 60, seconds);
        medWhiteFont->writeString(port()->frame(), 0xb8, 0xa0, reinterpret_cast<uint8_t*>(text), -1);

        // The commanders by kills.
        int32_t row = 0;

        for (int32_t i = 0; i < 6; i++)
        {
            if (scores[i].score < 0 || MPlayer->sessionManager->GetPlayerNumber(scores[i].commanderId) == nullptr)
            {
                continue;
            }

            std::snprintf(text, sizeof(text), "%i.", i + 1);
            const int32_t rowY = static_cast<int16_t>(row) * 0xc + 0xbe;
            medBlueFont->writeString(port()->frame(), 0x1a, rowY, reinterpret_cast<uint8_t*>(text), -1);
            FIDPPlayer* player = MPlayer->sessionManager->GetPlayerNumber(scores[i].commanderId);
            medWhiteFont->writeString(port()->frame(), 0x28, rowY, reinterpret_cast<uint8_t*>(player->name), 0x68);
            std::snprintf(text, sizeof(text), "%i", scores[i].score);
            medWhiteFont->writeString(port()->frame(), 0xcc, rowY, reinterpret_cast<uint8_t*>(text), -1);
            row++;
        }

        drawMPPilots(1);
        drawY = 0x11d;
        drawMPObjectives();
        soundSystem->playBettySample(homeSideLost(scenarioResult) ? 0xb : 0x12);
    }

    if (MPlayer != nullptr && MPlayer->sessionManager != nullptr && mission->endScenarioRequested != 0)
    {
        MPlayer->leaveSession();
        MPlayer->inMission = 1;
    }

    somethingOnFire = 0;
    scenarioEnded = 0;
    return 0;
}

auto MissionResultsScreen::drawMPObjectives() -> void
{
    char pointsName[256];
    char bonusHeader[256];
    char secondaryHeader[256];
    cLoadString(thisInstance, 0x363, pointsName, 0xfe);
    cLoadString(thisInstance, 0x360, bonusHeader, 0xfe);
    cLoadString(thisInstance, 0x362, secondaryHeader, 0xfe);

    // Only the primary objectives (type 0) of the home team are listed; the loop over the types stops after the
    // first, so the secondary header below is never drawn.
    for (uint32_t type = 0; type < 1; type++)
    {
        const int32_t first = homeTeam->firstObjective;
        const int32_t end = first + static_cast<int32_t>(homeTeam->numObjectives);

        for (int32_t i = first; i < end; i++)
        {
            ScenarioObjective& objective = scenario->objectives[i];

            if (objective.type != type)
            {
                continue;
            }

            if (type == 1 && objectivesHeaderDrawn == 0)
            {
                medBlueFont->writeString(port()->frame(), 0xf, drawY, reinterpret_cast<uint8_t*>(secondaryHeader), -1);
                objectivesHeaderDrawn = 1;
                drawY += 0xe;
            }

            aFont* font = greyFont;

            if (objective.status == 1)
            {
                successPort->copyTo(port()->frame(), 0xb, drawY - 1, 0);
                font = greenFont;
            }
            else if (objective.status == 2)
            {
                failurePort->copyTo(port()->frame(), 0xb, drawY - 1, 0);
                font = redFont;
            }
            else if (objective.status != 0)
            {
                continue;
            }

            if (font != nullptr)
            {
                font->writeString(port()->frame(), 0x17, drawY, reinterpret_cast<uint8_t*>(objective.name), -1);
                drawY += 0xd;
            }
        }
    }
}
