#include "stdafx.h"
#include "logistics/logmain.h"
#include "gui/afont.h"
#include "gui/asystem.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/file.h"
#include "lib/inifile.h"
#include "linkup/dpplayer.h"
#include "linkup/linkedlist.hpp"
#include "linkup/session.h"
#include "linkup/sessionmanager.h"
#include "logistics/invblock.h"
#include "logistics/logbri.h"
#include "logistics/logdlg.h"
#include "logistics/loggen.h"
#include "logistics/logsession.h"
#include "main/honorb.h"
#include "main/logistics.h"
#include "main/main.h"
#include "mission/mission.h"
#include "network/multplyr.h"
#include "platform/MCFileSystem.h"
#include "platform/MCInput.h"
#include "platform/MCPresenter.h"
#include "platform/MCRegistry.h"
#include "platform/MCRenderer.h"
#include "sound/soundsys.h"
#include "sprite/sprtmgr.h"
#include "vfx/vfxfuncs.h"

int32_t GameDifficulty = 1;
// The data paths are 80 bytes, as the game's other paths (gui\asystem.cpp's RealWinMain fills several of them).
char CDsoundPath[80] = {};
char interfacePath[80] = {};
char savePath[80] = {};
char CDspritePath[80] = {};
char terrainPath[80] = {};
char warriorPath[80] = {};
char spritePath[80] = {};
char profilePath[80] = {};
char fontPath[80] = {};
char directXPath[80] = {};
char soundPath[80] = {};
char shapesPath[80] = {};
void* thisInstance = nullptr;
int only45Pixel = 0;
Logistics* globalLogPtr = nullptr;
int32_t LastLogisticsMissionState = 0;
int LoadingSolo = 0;
int whackTimer = 0;
int force32MB = 0;
int force16MB = 0;
int32_t readyRoomTicks = 0;

namespace
{
    /// <summary>The registry key of the game's version and language.</summary>
    constexpr const char* VersionKey = "Software\\Fasa Interactive\\MechCommander Expansion";

    /// <summary>Screen element <paramref name="index"/> of <paramref name="screen"/> as a <typeparamref name="T"/>.</summary>
    template <typename T> T* element(GenericScreen* screen, int32_t index)
    {
        return static_cast<T*>(screen->elements[index]);
    }

    /// <summary>The text typed in text element <paramref name="index"/> of <paramref name="screen"/>.</summary>
    char* elementText(GenericScreen* screen, int32_t index)
    {
        return element<lTextObject>(screen, index)->buffer;
    }

    /// <summary>Enables or disables <paramref name="button"/> (it shows the change on the next frame).</summary>
    void setDisabled(lButton* button, int disabled)
    {
        button->disabled = disabled;
    }

    /// <summary>Hides the screen shown and shows <paramref name="screen"/> as logistics state <paramref name="state"/>.</summary>
    void switchScreen(lObject* from, lObject* screen, int32_t state)
    {
        from->ShowGUIWindow(0);
        screen->ShowGUIWindow(1);
        globalLogPtr->currentScreen = screen;
        globalLogPtr->logisticsState = state;
    }

    /// <summary>
    /// The one-button message dialog with string <paramref name="id"/>: an OK button (<paramref name="upArt"/> /
    /// <paramref name="downArt"/>) and <paramref name="callback"/> for the answer.
    /// </summary>
    void showMessage(uint32_t id, void (*callback)(int32_t), const char* upArt = "bh_okay.tga",
                     const char* downArt = "bg_okay.tga")
    {
        char text[256];
        cLoadString(thisInstance, id, text, 0xfe);
        ReusableDialog* dialog = globalLogPtr->messageDialog;
        dialog->setText(text);
        dialog->setTwoButton(0);
        dialog->callback = callback;
        dialog->okButton->setUpPicture(const_cast<char*>(upArt));
        dialog->okButton->setDownPicture(const_cast<char*>(downArt));
        setDisabled(dialog->okButton, 0);
        dialog->activate();
    }

    /// <summary>
    /// The two-button question dialog <paramref name="dialog"/> with string <paramref name="id"/>: OK runs
    /// <paramref name="okExec"/>, cancel <paramref name="cancelExec"/>; the cancel button shows
    /// <paramref name="cancelDownArt"/> when pressed. With <paramref name="enableButtons"/> both buttons are
    /// enabled and redrawn.
    /// </summary>
    void askQuestion(ReusableDialog* dialog, uint32_t id, void (*okExec)(), void (*cancelExec)(),
                     const char* cancelDownArt, bool enableButtons)
    {
        char text[256];
        cLoadString(thisInstance, id, text, 0xfe);
        dialog->setText(text);
        dialog->setTwoButton(1);
        dialog->callback = nullptr;
        dialog->okButton->setUpPicture(const_cast<char*>("bh_okay.tga"));
        dialog->okButton->setDownPicture(const_cast<char*>("bg_okay.tga"));

        if (enableButtons)
        {
            setDisabled(dialog->okButton, 0);
        }

        dialog->okButton->callback()->setExec(okExec);
        dialog->cancelButton->setUpPicture(const_cast<char*>("bh_cancl.tga"));
        dialog->cancelButton->setDownPicture(const_cast<char*>(cancelDownArt));

        if (enableButtons)
        {
            setDisabled(dialog->cancelButton, 0);
        }

        dialog->cancelButton->callback()->setExec(cancelExec);
        dialog->activate();
    }

    /// <summary>The player's name for the multiplayer screens: the one remembered, else "Player".</summary>
    bool userName(char* name, uint32_t size)
    {
        uint32_t length = size;
        return MyGetUserName(name, &length) != 0;
    }

    /// <summary>The file pane of the screen being shown (the load or save screen), or null.</summary>
    FileScrollPane* shownFilePane()
    {
        FileScrollPane* pane = nullptr;

        if (globalLogPtr->currentScreen == globalLogPtr->loadScreen)
        {
            pane = globalLogPtr->loadScreen->filePane;
        }

        if (globalLogPtr->currentScreen == globalLogPtr->saveScreen)
        {
            pane = globalLogPtr->saveScreen->filePane;
        }

        return pane;
    }

    /// <summary>Whether <paramref name="pane"/> has a file selected.</summary>
    bool hasSelection(const FileScrollPane* pane)
    {
        return pane != nullptr && pane->selectedFile > -1 && pane->selectedFile < pane->numFiles;
    }

    /// <summary>Takes down the file pane's name entry, if it has one (its <c>destroy</c>).</summary>
    void destroyNameEntry(FileScrollPane* pane)
    {
        if (pane->nameEntry != nullptr)
        {
            pane->nameEntry->destroy();
        }
    }

    /// <summary>A copy of <paramref name="text"/> in a logistics block.</summary>
    char* heapCopy(const char* text)
    {
        auto* copy =
            static_cast<char*>(globalLogPtr->logisticsBlocks->Allocate(static_cast<uint32_t>(std::strlen(text) + 1)));
        std::strcpy(copy, text);
        return copy;
    }

    /// <summary>Opens the ready room (session screen) after a session was joined or created.</summary>
    void enterReadyRoom(lObject* from, int disableGo)
    {
        from->ShowGUIWindow(0);
        globalLogPtr->connectScreen->ShowGUIWindow(1);
        globalLogPtr->currentScreen = globalLogPtr->connectScreen;
        globalLogPtr->logisticsState = 0xe;

        if (disableGo >= 0)
        {
            setDisabled(element<lButton>(globalLogPtr->connectScreen, 2), disableGo);
        }
    }

    /// <summary>After a session was joined: how many are in it.</summary>
    void countLANPlayers()
    {
        NumLANPlayers = MPlayer->sessionManager->GetPlayers(nullptr)->count;
    }
}

// The campaign CD checks: the dialog's OK retries the action.

void NewCampaignCDTester(int32_t result)
{
    if (result == 1)
    {
        NewCampaign();
    }
}

void MCXCampaignCDTester(int32_t result)
{
    if (result == 1)
    {
        NewMCXCampaign();
    }
}

void MPXCampaignCDTester(int32_t result)
{
    if (result == 1)
    {
        ConnectScreen();
    }
}

void SaveCampaignCDTester(int32_t result)
{
    if (result == 1)
    {
        SaveScreen();
    }
}

void LoadCampaignCDTester(int32_t result)
{
    if (result == 1)
    {
        LoadScreen();
    }
}

void SoloCampaignCDTester(int32_t result)
{
    if (result == 1)
    {
        SoloLoadScreen();
    }
}

void PrefCampaignCDTester(int32_t result)
{
    if (result == 1)
    {
        ShowPreferences();
    }
}

void CineCampaignCDTester(int32_t result)
{
    if (result == 1)
    {
        ReplayCinema();
    }
}

bool CheckRegistryVersionNumber()
{
    // The version is "<string 0x282>." (the build string and a dot at character 13).
    const std::optional<std::string> stored = MCRegistry::Read(VersionKey, "Version");

    if (!stored.has_value())
    {
        return false;
    }

    char version[100] = {};
    std::strncpy(version, stored->c_str(), sizeof(version) - 1);
    char expected[100];
    cLoadString(thisInstance, 0x282, expected, 99);
    return std::strncmp(version, expected, 0xd) == 0 && version[13] == '.';
}

void WriteRegistryVersionNumber()
{
    char text[100];
    char version[100];
    cLoadString(thisInstance, 0x282, text, 99);
    std::snprintf(version, sizeof(version), "%s.", text);
    MCRegistry::Write(VersionKey, "Version", version);
    cLoadString(thisInstance, 900, text, 99);
    MCRegistry::Write(VersionKey, "Language", text);
}

// Each menu action first made sure a campaign CD was in a drive (scanning C: to Z: for data\tiles\gtiles90.pak,
// pointing every drive-letter path at it, and asking for the disc when none had it). The port reads everything from
// the install folder, so the disc is always there (as checkForCDInDrive says); only the version check is kept.

void NewCampaign()
{
    if (!CheckRegistryVersionNumber())
    {
        WriteRegistryVersionNumber();
    }

    soundSystem->stopDigitalMusic();
    soundSystem->playBettySample(0x19);
    std::strcpy(missionName, "mechcmdr1");
    mission->initAgain(missionName);
    CurPlanet = 0;
    Solo = 0;
    LastLogisticsMissionState = 0;
    globalLogPtr->loadCampaign(const_cast<char*>("start0"), const_cast<char*>(".pkk"), 0, 0);
    globalLogPtr->briefingScreen->briefingBox = nullptr;
    globalLogPtr->setUpBriefingScreen(0);
    globalLogPtr->mainScreen->ShowGUIWindow(0);
}

void NewMCXCampaign()
{
    if (!CheckRegistryVersionNumber())
    {
        WriteRegistryVersionNumber();
    }

    soundSystem->stopDigitalMusic();
    soundSystem->playBettySample(0x19);
    std::strcpy(missionName, "xmechcmdr1");
    mission->initAgain(missionName);
    Solo = 0;
    LastLogisticsMissionState = 0;
    globalLogPtr->loadCampaign(const_cast<char*>("xstart0"), const_cast<char*>(".pkk"), 0, 0);
    globalLogPtr->briefingScreen->briefingBox = nullptr;
    globalLogPtr->setUpBriefingScreen(0);
    globalLogPtr->mainScreen->ShowGUIWindow(0);
}

void SaveScreen()
{
    if (!CheckRegistryVersionNumber())
    {
        WriteRegistryVersionNumber();
    }

    if (globalLogPtr->currentMission == -1)
    {
        return;
    }

    globalLogPtr->mainScreen->ShowGUIWindow(0);
    globalLogPtr->currentScreen = globalLogPtr->saveScreen;
    globalLogPtr->saveScreen->ShowGUIWindow(1);
    globalLogPtr->logisticsState = 6;
    globalLogPtr->saveScreen->filePane->setSelectedFile(-1);
}

void ConnectScreen()
{
    if (!CheckRegistryVersionNumber())
    {
        WriteRegistryVersionNumber();
    }

    GenericScreen* screen = globalLogPtr->multiplayerScreen;

    if (MPlayer == nullptr)
    {
        auto* player = new MultiPlayer;
        MPlayer = player;
        Assert(player != nullptr, 0, " Unable to create MultiPlayer object ", nullptr);
        MPlayer->init(0x7d000, 0x100, 100);
    }

    globalLogPtr->mainScreen->ShowGUIWindow(0);
    globalLogPtr->multiplayerScreen->ShowGUIWindow(1);
    globalLogPtr->currentScreen = globalLogPtr->multiplayerScreen;
    globalLogPtr->logisticsState = 10;

    // The connection buttons: modem, serial, LAN, internet; each is enabled when the machine has it.
    auto* modemButton = element<lButton>(screen, 2);
    auto* serialButton = element<lButton>(screen, 3);
    auto* lanButton = element<lButton>(screen, 4);
    auto* internetButton = element<lButton>(screen, 5);
    setDisabled(modemButton, 1);
    setDisabled(serialButton, 1);
    setDisabled(lanButton, 1);
    setDisabled(internetButton, 1);

    if (MPlayer != nullptr && MPlayer->sessionManager != nullptr)
    {
        SessionManager* manager = MPlayer->sessionManager;

        if (manager->isModemAvailable() != 0)
        {
            setDisabled(modemButton, 0);
        }

        if ((manager->availableProtocols & 4) != 0)
        {
            setDisabled(serialButton, 0);
        }

        if (manager->isIPXAvailable() != 0 || manager->isTCPAvailable() != 0)
        {
            setDisabled(lanButton, 0);
        }

        if ((manager->availableProtocols & 0x10) != 0)
        {
            setDisabled(internetButton, 0);
        }
    }

    // Less than 32 MB: multiplayer may not run well.
    if (MCPort::TotalPhysicalMemory() < 32000000)
    {
        showMessage(0x371, nullptr);
    }
}

void LoadScreen()
{
    if (!CheckRegistryVersionNumber())
    {
        WriteRegistryVersionNumber();
    }

    globalLogPtr->mainScreen->ShowGUIWindow(0);
    globalLogPtr->currentScreen = globalLogPtr->loadScreen;
    globalLogPtr->loadScreen->ShowGUIWindow(1);
    globalLogPtr->logisticsState = 5;
    globalLogPtr->loadScreen->filePane->setSelectedFile(-1);
}

void SoloLoadScreen()
{
    if (!CheckRegistryVersionNumber())
    {
        WriteRegistryVersionNumber();
    }

    globalLogPtr->mainScreen->ShowGUIWindow(0);
    LoadingSolo = 1;
    globalLogPtr->currentScreen = globalLogPtr->loadScreen;
    globalLogPtr->loadScreen->ShowGUIWindow(1);
    globalLogPtr->logisticsState = 5;
    globalLogPtr->loadScreen->filePane->setSelectedFile(-1);
}

namespace
{
    // Port: the preferences screen's DIFFICULTY and RENDERER choices are drop-downs, each in a box drawn as the panel
    // art's DIFFICULTY box (prefs_00.tga): an outline of colour 0x13 on the panel's 0x10, the title in 0xe3. The
    // DIFFICULTY box is drawn over the art's, which holds the original's three checks and labels; the checks
    // (elements 8..10) are hidden.

    /// <summary>A box's place on the screen and its size.</summary>
    struct PrefsBoxPlace
    {
        /// <summary>The top left corner on the screen.</summary>
        int32_t Left = 0;
        int32_t Top = 0;
        /// <summary>The size, outline included.</summary>
        int32_t Width = 0;
        int32_t Height = 0;
    };

    /// <summary>DIFFICULTY's box is the art's; RENDERER's is under it, over ACCEPT.</summary>
    constexpr PrefsBoxPlace DifficultyBoxPlace{480, 155, 120, 63};
    constexpr PrefsBoxPlace RendererBoxPlace{480, 222, 120, 50};

    /// <summary>Where a drop-down lies in its box (under the title, the art's checks' column and first row), and its width.</summary>
    constexpr int32_t DropDownLeft = 32;
    constexpr int32_t DropDownTop = 21;
    constexpr int32_t DropDownWidth = 76;

    /// <summary>A preferences box: its back, outline and title, drawn each frame.</summary>
    class PrefsBox : public lObject
    {
    public:
        /// <summary>A box titled <paramref name="title"/>.</summary>
        explicit PrefsBox(const char* title) : title(title) {}

        void draw() override
        {
            const auto right = static_cast<int16_t>(width() - 1);
            const auto bottom = static_cast<int16_t>(height() - 1);
            FillBox(0, 0, right, bottom, 0x10);
            FillBox(0, 0, right, 0, 0x13);
            FillBox(0, bottom, right, bottom, 0x13);
            FillBox(0, 0, 0, bottom, 0x13);
            FillBox(right, 0, right, bottom, 0x13);
            // (The font's glyphs start a column in, so the text is written a pixel left of the art's.)
            VFX_string_draw(lport()->frame(), 32, 9, whiteFont->fontData.get(), title, lComboBox::LabelColors());
        }

        bool DrawsLive() override { return true; }

        /// <summary>
        /// The box is part of the panel, as the art's boxes are: the mouse goes through it. (As an object it took
        /// clicks, and a click brought it in front of the controls on it, which then took no more.)
        /// </summary>
        aObject* findObject(int32_t xPos, int32_t yPos) override
        {
            (void)xPos;
            (void)yPos;
            return nullptr;
        }

    private:
        /// <summary>The title.</summary>
        const char* title = nullptr;
    };

    /// <summary>The choice when the screen opened (for CancelPrefs).</summary>
    int32_t savedRendererPreference = 0;

    /// <summary>Says, in the message dialog, that the renderer chosen takes effect at the next start.</summary>
    void showRestartNotice()
    {
        char text[] = "The new renderer takes effect when you restart MechCommander.";
        ReusableDialog* dialog = globalLogPtr->messageDialog;
        dialog->setText(text);
        dialog->setTwoButton(0);
        dialog->callback = nullptr;
        dialog->okButton->setUpPicture(const_cast<char*>("bh_okay.tga"));
        dialog->okButton->setDownPicture(const_cast<char*>("bg_okay.tga"));
        dialog->okButton->disabled = 0;
        dialog->activate();
    }

    /// <summary>The RENDERER drop-down chose <paramref name="renderer"/>: another one than this run's needs a restart.</summary>
    void rendererChanged(int32_t renderer)
    {
        if (renderer != gRenderer)
        {
            showRestartNotice();
        }
    }

    /// <summary>Adds a box titled <paramref name="title"/> at <paramref name="place"/> to <paramref name="screen"/>.</summary>
    void addPrefsBox(GenericScreen* screen, const PrefsBoxPlace& place, const char* title)
    {
        auto* box = new PrefsBox(title);
        box->init(place.Left, place.Top, place.Width, place.Height, nullptr, nullptr);
        box->SetTransparent(-1);
        box->ShowGUIWindow(-1);
        screen->addChild(box);
    }

    /// <summary>Adds a drop-down editing <paramref name="setting"/> in the box at <paramref name="place"/>.</summary>
    lComboBox* addDropDown(GenericScreen* screen, const PrefsBoxPlace& place, int32_t* setting,
                           std::vector<lComboBox::Item> items, void (*changed)(int32_t value))
    {
        auto* dropDown = new lComboBox;
        dropDown->init(place.Left + DropDownLeft, place.Top + DropDownTop, DropDownWidth, setting, std::move(items),
                       changed);
        dropDown->ShowGUIWindow(-1);
        screen->addChild(dropDown);
        return dropDown;
    }
}

void AddPreferenceDropDowns(GenericScreen* screen)
{
    // The original's DIFFICULTY checks (easy, regular, hard) give way to the drop-down.
    for (int32_t i = 0; i < 3; i++)
    {
        element<lToolButton>(screen, 8 + i)->ShowGUIWindow(0);
    }

    addPrefsBox(screen, DifficultyBoxPlace, "DIFFICULTY");
    addPrefsBox(screen, RendererBoxPlace, "RENDERER");
    addDropDown(screen, DifficultyBoxPlace, &GameDifficulty, {{"EASY", 0}, {"REGULAR", 1}, {"HARD", 2}}, nullptr);
    addDropDown(screen, RendererBoxPlace, &gRendererPreference,
                {{"VULKAN", static_cast<int32_t>(MCRendererKind::Vulkan)},
                 {"SOFTWARE", static_cast<int32_t>(MCRendererKind::Software)}},
                rendererChanged);
}

void ShowPreferences()
{
    if (!CheckRegistryVersionNumber())
    {
        WriteRegistryVersionNumber();
    }

    // The settings as they are, for CancelPrefs.
    Logistics* logistics = globalLogPtr;
    const int32_t brightness = application->gammaLevel;
    logistics->savedPrefs0 = application->paletteCycle;
    logistics->savedPrefs1 = only45Pixel;
    logistics->savedPrefs2 = brightness;
    logistics->savedPrefs3 = soundSystem->musicVolume;
    logistics->savedPrefs4 = soundSystem->radioVolume;
    logistics->savedPrefs5 = soundSystem->digitalMasterVolume;
    logistics->savedPrefs6 = GameDifficulty;
    savedRendererPreference = gRendererPreference;
    GenericScreen* screen = logistics->prefScreen;
    element<lSlider>(screen, 3)->setCurrentValue(brightness);
    element<lSlider>(screen, 4)->setCurrentValue(static_cast<int32_t>(globalLogPtr->savedPrefs3));
    element<lSlider>(screen, 5)->setCurrentValue(static_cast<int32_t>(globalLogPtr->savedPrefs4));
    element<lSlider>(screen, 6)->setCurrentValue(static_cast<int32_t>(globalLogPtr->savedPrefs5));
    element<lToolButton>(screen, 8)->toggled = GameDifficulty == 0 ? 1 : 0;
    element<lToolButton>(screen, 9)->toggled = GameDifficulty == 1 ? 1 : 0;
    element<lToolButton>(screen, 10)->toggled = GameDifficulty != 0 && GameDifficulty != 1 ? 1 : 0;
    globalLogPtr->mainScreen->ShowGUIWindow(0);
    globalLogPtr->currentScreen = globalLogPtr->prefScreen;
    globalLogPtr->prefScreen->ShowGUIWindow(1);
    globalLogPtr->logisticsState = 9;
}

void CancelPrefs()
{
    Logistics* logistics = globalLogPtr;
    application->paletteCycle = logistics->savedPrefs0;
    only45Pixel = logistics->savedPrefs1;
    application->gammaCorrectCurrentPalette(logistics->savedPrefs2);

    // Only the saved byte is compared (a volume is 0..127).
    if (static_cast<uint8_t>(logistics->savedPrefs3) < 0x80)
    {
        soundSystem->musicVolume = static_cast<uint8_t>(logistics->savedPrefs3);
    }

    if (static_cast<uint8_t>(logistics->savedPrefs4) < 0x80)
    {
        soundSystem->radioVolume = static_cast<uint8_t>(logistics->savedPrefs4);
    }

    if (static_cast<uint8_t>(logistics->savedPrefs5) < 0x80)
    {
        soundSystem->digitalMasterVolume = static_cast<uint8_t>(logistics->savedPrefs5);
    }

    GameDifficulty = logistics->savedPrefs6;
    gRendererPreference = savedRendererPreference;
    Cancel();
}

void WritePrefs()
{
    FitIniFile prefs;
    prefs.create("prefs.cfg");
    prefs.writeBlock("MechCommander");
    prefs.writeIdBoolean("PaletteCycle", application->paletteCycle);
    prefs.writeIdBoolean("DirectDraw", gFullScreen != 0);
    prefs.writeIdBoolean("Use90Pixel", use90PixelSprite);
    prefs.writeIdBoolean("Force45Pixel", only45Pixel);
    prefs.writeIdBoolean("Force16Mb", force16MB);
    prefs.writeIdBoolean("Force32Mb", force32MB);
    prefs.writeIdLong("Difficulty", GameDifficulty);
    prefs.writeIdLong("Brightness", application->gammaLevel);
    prefs.writeIdLong("MusicVolume", soundSystem->musicVolume);
    prefs.writeIdLong("RadioVolume", soundSystem->radioVolume);
    prefs.writeIdLong("SFXVolume", soundSystem->digitalMasterVolume);
    // Port: keep the port-only key. Original behaviour (OB-101): the hidden "Resolution" key is not written back.
    prefs.writeIdBoolean("StretchToFit", gStretchToFit != 0);
    prefs.writeIdBoolean("SoftwareCursor", gSoftwareCursor != 0);
    prefs.writeIdString("Renderer", MCRendererKindName(static_cast<MCRendererKind>(gRendererPreference)));
    prefs.writeIdBoolean("ShowFps", gShowFpsPreference != 0);
    Cancel();
}

void ShowMultiPlayer()
{
    globalLogPtr->mainScreen->ShowGUIWindow(0);
    globalLogPtr->multiplayerScreen->ShowGUIWindow(1);
    Solo = 0;
    globalLogPtr->currentScreen = globalLogPtr->multiplayerScreen;
    globalLogPtr->logisticsState = 10;
}

void ReplayCDTester(int32_t result)
{
    if (result == 1)
    {
        ReplayCinema();
    }
}

void ReplayCinema()
{
    if (!CheckRegistryVersionNumber())
    {
        WriteRegistryVersionNumber();
    }

    nextGameState = 10;
    mission->missionState = 10;
    mission->currentMovie = 0;
    globalLogPtr->mainScreen->ShowGUIWindow(0);
    soundSystem->stopDigitalMusic();
}

void ReturnToGame()
{
    if (globalLogPtr->currentMission == -1)
    {
        return;
    }

    if (globalLogPtr->previousState == 2)
    {
        globalLogPtr->setUpPurchaseScreen(0);
    }
    else if (globalLogPtr->previousState == 4)
    {
        globalLogPtr->setUpRepairScreen(0);
    }
    else
    {
        globalLogPtr->setUpBriefingScreen(0);
    }

    globalLogPtr->mainScreen->ShowGUIWindow(0);
}

void GameOverMan()
{
    MCInput::PostMessage(WM_DESTROY, 0, 0);
    soundSystem->stopDigitalMusic();
}

void LoadGame()
{
    FileScrollPane* pane = globalLogPtr->loadScreen->filePane;

    if (!hasSelection(pane))
    {
        return;
    }

    char* fileName = pane->fileNames[pane->selectedFile];
    soundSystem->stopDigitalMusic();
    globalLogPtr->briefingScreen->briefingBox = nullptr;
    const bool campaign = LoadingSolo == 0;
    const char* extension = ".sav";

    if (!campaign)
    {
        LoadingSolo = 0;
        extension = ".sol";
    }

    Solo = campaign ? 0 : 1;
    LastLogisticsMissionState = 0;

    if (globalLogPtr->loadCampaign(fileName, const_cast<char*>(extension), 0, 1) == 0)
    {
        // Original behaviour: the save screen is the one hidden, not the load screen shown.
        globalLogPtr->saveScreen->ShowGUIWindow(0);
        globalLogPtr->setUpBriefingScreen(0);
        soundSystem->playDigitalMusic(0x16, true);
    }
}

void LoadMPGame()
{
    FileScrollPane* pane = globalLogPtr->loadScreen->filePane;

    if (hasSelection(pane))
    {
        globalLogPtr->sessionScreen->loadMission(pane->fileNames[pane->selectedFile]);
    }
}

void SaveWorkedCallback(int32_t)
{
    globalLogPtr->saveScreen->ShowGUIWindow(0);

    if (globalLogPtr->previousState == 2)
    {
        globalLogPtr->setUpPurchaseScreen(0);
    }
    else if (globalLogPtr->previousState != 4)
    {
        globalLogPtr->setUpBriefingScreen(0);
    }
    else
    {
        globalLogPtr->setUpRepairScreen(0);
    }

    soundSystem->playDigitalMusic(0x16, true);
}

void SaveGameCallback()
{
    int32_t result = -1;
    FileScrollPane* pane = globalLogPtr->saveScreen->filePane;
    bool saved = false;

    if (hasSelection(pane))
    {
        char* fileName = pane->fileNames[pane->selectedFile];

        if (fileName != nullptr)
        {
            soundSystem->stopDigitalMusic();
            result = globalLogPtr->saveCampaign(fileName);
        }

        saved = result == 0;
    }

    if (!saved)
    {
        pane->setSelectedFile(-1);
    }

    pane->getAllFiles(const_cast<char*>(".sav"), true);

    if (result == 0)
    {
        // "Game saved", closing by itself after three seconds.
        char text[256];
        cLoadString(thisInstance, 0x76, text, 0xfe);
        ReusableDialog* dialog = globalLogPtr->messageDialog;
        dialog->setText(text);
        dialog->setTwoButton(0);
        dialog->callback = SaveWorkedCallback;
        dialog->okButton->setUpPicture(const_cast<char*>("bh_okay.tga"));
        dialog->okButton->setDownPicture(const_cast<char*>("bg_okay.tga"));
        setDisabled(dialog->okButton, 0);
        dialog->timeout = 3000;
        dialog->activate();
        destroyNameEntry(pane);
    }

    soundSystem->playDigitalMusic(0x16, true);
}

void ClearForSaveGameCallback()
{
    FileScrollPane* pane = globalLogPtr->saveScreen->filePane;
    FullPathFileName fileName;
    fileName.init(pane->startDirectory, pane->fileNames[pane->selectedFile], ".sav");

    // The original cleared the file's read-only attribute first.
    if (MCFileSystem::RemoveFile(static_cast<char*>(fileName)))
    {
        SaveGameCallback();
    }
}

void SaveGame()
{
    FileScrollPane* pane = globalLogPtr->saveScreen->filePane;

    if (pane == nullptr)
    {
        return;
    }

    const int32_t selected = pane->selectedFile;

    if (selected < 0 || selected >= pane->numFiles)
    {
        return;
    }

    // The name typed replaces the selected entry's.
    lTextObject* entry = pane->nameEntry;

    if (entry != nullptr && entry->parent == pane)
    {
        const char* typed = entry->buffer;

        if (typed[0] == '\0')
        {
            typed = EmptyFile;
        }

        globalLogPtr->logisticsBlocks->Free(pane->fileNames[selected]);
        pane->fileNames[selected] = heapCopy(typed);
    }

    char** slot = &pane->fileNames[pane->selectedFile];

    if (*slot == nullptr)
    {
        return;
    }

    // The empty entry gets the first free default name.
    if (std::strcmp(EmptyFile, *slot) == 0)
    {
        const char* directory = pane->startDirectory;

        for (int32_t i = 0; i < 1000; i++)
        {
            char format[0x95];
            cLoadString(thisInstance, 0x37c, format, 0x95);
            char candidate[0x68];
            std::snprintf(candidate, sizeof(candidate), format, i);
            FullPathFileName candidatePath;
            candidatePath.init(directory, candidate, ".sav");

            if (fileExists(candidatePath) == 0)
            {
                globalLogPtr->logisticsBlocks->Free(*slot);
                *slot = heapCopy(candidate);
                break;
            }
        }
    }

    // Port fix (OB-084): the original tested the old, freed name here (the empty entry's text); the name now in
    // the slot is tested instead.
    FullPathFileName savePathName;
    savePathName.init(pane->startDirectory, *slot, ".sav");

    if (fileExists(savePathName) == 0)
    {
        SaveGameCallback();
        return;
    }

    // Overwrite?
    char text[256];
    cLoadString(thisInstance, 0x75, text, 0xfe);
    ReusableDialog* dialog = globalLogPtr->messageDialog;
    dialog->setText(text);
    dialog->setTwoButton(1);
    dialog->callback = nullptr;
    dialog->okButton->setUpPicture(const_cast<char*>("bh_okay.tga"));
    dialog->okButton->setDownPicture(const_cast<char*>("bg_okay.tga"));
    dialog->okButton->callback()->setExec(ClearForSaveGameCallback);
    dialog->cancelButton->setUpPicture(const_cast<char*>("bh_cancl.tga"));
    dialog->cancelButton->setDownPicture(const_cast<char*>("bg_cancl.tga"));
    dialog->activate();
}

void DeleteCallbackTrue()
{
    FileScrollPane* pane = shownFilePane();

    if (!hasSelection(pane))
    {
        return;
    }

    FullPathFileName fileName;
    fileName.init(pane->startDirectory, pane->fileNames[pane->selectedFile], LoadingSolo == 0 ? ".sav" : ".sol");
    // The original cleared the file's read-only attribute first.
    MCFileSystem::RemoveFile(static_cast<char*>(fileName));
    pane->setSelectedFile(-1);

    if (LoadingSolo == 0)
    {
        globalLogPtr->loadScreen->filePane->getAllFiles(const_cast<char*>(".sav"), true);
        globalLogPtr->saveScreen->filePane->getAllFiles(const_cast<char*>(".sav"), true);
    }
    else
    {
        globalLogPtr->loadScreen->filePane->getAllFiles(const_cast<char*>(".sol"), true);
    }
}

void DeleteCallbackFalse()
{
    FileScrollPane* pane = shownFilePane();

    if (pane != nullptr && pane->nameEntry != nullptr)
    {
        destroyNameEntry(pane);
    }
}

void DeleteGame()
{
    FileScrollPane* pane = shownFilePane();

    if (!hasSelection(pane))
    {
        return;
    }

    askQuestion(globalLogPtr->messageDialog, 0x74, DeleteCallbackTrue, DeleteCallbackFalse, "bg_cancl.tga", false);
    destroyNameEntry(pane);
}

void Cancel()
{
    if (globalLogPtr->currentScreen == globalLogPtr->mainScreen)
    {
        return;
    }

    whackTimer = 1;
    globalLogPtr->setUpMainScreen(1);
    LoadingSolo = 0;
}

void CancelToConnect()
{
    whackTimer = 1;
    globalLogPtr->currentScreen->ShowGUIWindow(0);
    globalLogPtr->multiplayerScreen->ShowGUIWindow(1);
    globalLogPtr->currentScreen = globalLogPtr->multiplayerScreen;
    globalLogPtr->logisticsState = 10;
    globalLogPtr->lanScreen->showBlock(0);
    globalLogPtr->modemScreen->showBlock(0);
    auto* games = element<GameList>(globalLogPtr->lanScreen, 2);

    if (games->numSessions > -1)
    {
        games->selectedSession = -1;
        // Port fix (OB-085): the original copied sessions[-1] (the four list fields before the table) as the
        // selected GUID; no session is selected, so it is cleared.
        games->selectedGuid = {};
    }

    games->Clear();
    element<lScrollTextObject>(globalLogPtr->lanScreen, 3)->Clear();
    setDisabled(element<lButton>(globalLogPtr->lanScreen, 6), 1);
    application->RemoveTimer(globalLogPtr->sessionScreen->team1RPText, 0);
    application->RemoveTimer(globalLogPtr->sessionScreen->team1RPText, 0);
    MPlayer->leaveSession();
    delete globalLogPtr->playerLights;
    globalLogPtr->playerLights = nullptr;
}

void CancelToMPlayer()
{
    killTheGame();
}

void CancelToLAN()
{
    globalLogPtr->currentScreen->ShowGUIWindow(0);
    globalLogPtr->lanScreen->ShowGUIWindow(1);
    globalLogPtr->currentScreen = globalLogPtr->lanScreen;
    globalLogPtr->logisticsState = 0xb;
    globalLogPtr->lanScreen->showBlock(0);
    auto* games = element<GameList>(globalLogPtr->lanScreen, 2);

    if (games->numSessions > -1)
    {
        games->selectedSession = -1;
        // Port fix (OB-085): see CancelToConnect.
        games->selectedGuid = {};
    }
}

void CancelToSession()
{
    MCSplashScreen* loadScreen = globalLogPtr->loadScreen;
    loadScreen->cancelButton->callback()->setExec(Cancel);
    loadScreen->loadSaveButton->callback()->setExec(LoadGame);
    loadScreen->filePane->setMultiplayer(0);
    globalLogPtr->currentScreen->ShowGUIWindow(0);
    globalLogPtr->currentScreen = globalLogPtr->sessionScreen;
    globalLogPtr->logisticsState = 8;
    globalLogPtr->showLogScreen(1, 1);
}

void ShowModemScreen()
{
    if (MPlayer != nullptr)
    {
        GenericScreen* screen = globalLogPtr->modemScreen;
        auto* modems = element<lScrollTextObject>(screen, 10);
        char name[0x40];
        const bool known = userName(name, 0x3f);
        element<lTextObject>(screen, 4)->setStringBuffer(known ? name : const_cast<char*>("Player"));
        MPlayer->sessionManager->FindModems();
        // The list is refilled with the modems found, keeping its selection.
        const int32_t selected = modems->highlightLine[0];
        modems->Clear();
        modems->highlightLine[0] = selected;

        for (int32_t i = 0;; i++)
        {
            char* modem = MPlayer->sessionManager->GetModemName(i);

            if (modem == nullptr)
            {
                break;
            }

            element<lScrollTextObject>(globalLogPtr->modemScreen, 10)->Print(modem, 0x1f);
        }
    }

    globalLogPtr->multiplayerScreen->ShowGUIWindow(0);
    globalLogPtr->currentScreen = globalLogPtr->modemScreen;
    globalLogPtr->modemScreen->ShowGUIWindow(1);
    globalLogPtr->logisticsState = 0xc;
    globalLogPtr->modemScreen->showBlock(0);
}

void ShowSerialScreen()
{
    char name[0x40];
    const bool known = userName(name, 0x3f);
    element<lTextObject>(globalLogPtr->serialScreen, 4)->setStringBuffer(known ? name : const_cast<char*>("Player"));
    globalLogPtr->multiplayerScreen->ShowGUIWindow(0);
    globalLogPtr->currentScreen = globalLogPtr->serialScreen;
    globalLogPtr->serialScreen->ShowGUIWindow(1);
    globalLogPtr->logisticsState = 0xd;
    application->setText(globalLogPtr->serialScreen->elements[4]);
}

void DoTheIPXThang()
{
    if (MPlayer != nullptr && MPlayer->sessionManager != nullptr)
    {
        MPlayer->sessionManager->ConnectIPX();
    }

    globalLogPtr->multiplayerScreen->ShowGUIWindow(0);
    globalLogPtr->lanScreen->ShowGUIWindow(1);
    globalLogPtr->lanScreen->showBlock(0);
    globalLogPtr->currentScreen = globalLogPtr->lanScreen;
    globalLogPtr->logisticsState = 0xb;
    application->setText(globalLogPtr->lanScreen->elements[4]);
}

void DoTheTCPThang()
{
    if (MPlayer != nullptr && MPlayer->sessionManager != nullptr)
    {
        MPlayer->sessionManager->ConnectTCP(const_cast<char*>(""));
    }

    globalLogPtr->multiplayerScreen->ShowGUIWindow(0);
    globalLogPtr->lanScreen->ShowGUIWindow(1);
    globalLogPtr->lanScreen->showBlock(0);
    globalLogPtr->currentScreen = globalLogPtr->lanScreen;
    globalLogPtr->logisticsState = 0xb;
    application->setText(globalLogPtr->lanScreen->elements[4]);
}

void TCPIPXDialogCallback(int32_t result)
{
    if (result == 1)
    {
        DoTheIPXThang();
        return;
    }

    if (result == 2)
    {
        DoTheTCPThang();
    }
}

void CallDoTheIPXThang(int32_t)
{
    DoTheIPXThang();
}

void CallDoTheTCPThang(int32_t)
{
    DoTheTCPThang();
}

void ShowLANScreen()
{
    GenericScreen* screen = globalLogPtr->lanScreen;
    char name[0x40];
    const char* gameName;
    char game[0x200];

    if (!userName(name, 0x3f))
    {
        element<lTextObject>(screen, 4)->setStringBuffer(const_cast<char*>("Player"));
        gameName = "Game";
    }
    else
    {
        element<lTextObject>(screen, 4)->setStringBuffer(name);
        char format[256];
        cLoadString(thisInstance, 0x377, format, 0xfe);
        std::snprintf(game, sizeof(game), format, name);
        element<lTextObject>(globalLogPtr->lanScreen, 10)->initBuffer(0x18, 0);
        gameName = game;
    }

    element<lTextObject>(globalLogPtr->lanScreen, 10)->setStringBuffer(const_cast<char*>(gameName));

    // Both protocols: ask which; else say which one is used.
    SessionManager* manager = MPlayer->sessionManager;

    if (manager->isIPXAvailable() != 0 && manager->isTCPAvailable() != 0)
    {
        char text[256];
        cLoadString(thisInstance, 0xa6, text, 0xfe);
        ReusableDialog* dialog = globalLogPtr->messageDialog;
        dialog->setText(text);
        dialog->setTwoButton(1);
        dialog->callback = TCPIPXDialogCallback;
        dialog->okButton->setUpPicture(const_cast<char*>("bh_ipx.tga"));
        dialog->okButton->setDownPicture(const_cast<char*>("bg_ipx.tga"));
        setDisabled(dialog->okButton, 0);
        dialog->okButton->result = 1;
        dialog->cancelButton->setUpPicture(const_cast<char*>("bh_tcp.tga"));
        dialog->cancelButton->setDownPicture(const_cast<char*>("bg_tcp.tga"));
        setDisabled(dialog->cancelButton, 0);
        dialog->cancelButton->result = 2;
        dialog->activate();
        return;
    }

    if (manager->isIPXAvailable() != 0)
    {
        showMessage(0x36f, CallDoTheIPXThang);
        return;
    }

    if (manager->isTCPAvailable() != 0)
    {
        showMessage(0x36e, CallDoTheTCPThang);
    }
}

void DoExitToZone1()
{
    // Port: the original started the Internet Gaming Zone's launcher (zonea502.exe, from the registry) or the web
    // browser on the Zone's MechCommander page; the Zone is gone, so nothing is started. It then quit, as here.
    killTheGame();
}

void DoExitToZone()
{
    askQuestion(globalLogPtr->questionDialog, 0x4e9, DoExitToZone1, nullptr, "bh_cancl.tga", true);
}

void DoExitToMplayer()
{
    // Port: the original started mplaynow.exe (the Mplayer.com lobby, long gone) and quit; the port takes the path
    // of a failed start: the error message.
    showMessage(0x353, nullptr);
}

void ShowInternet()
{
    askQuestion(globalLogPtr->messageDialog, 0x352, DoExitToMplayer, DoExitToZone, "bh_cancl.tga", true);
}

void HostGame()
{
    GenericScreen* screen = globalLogPtr->lanScreen;
    char* playerName = elementText(screen, 4);
    SaveUserName(playerName);
    globalLogPtr->lanScreen->showBlock(1);
    application->setText(globalLogPtr->lanScreen->elements[11]);
    char format[256];
    cLoadString(thisInstance, 0x377, format, 0xfe);
    char game[0x200];
    std::snprintf(game, sizeof(game), format, playerName);
    element<lTextObject>(globalLogPtr->lanScreen, 10)->setStringBuffer(game);
}

void ResetReadyRoom()
{
    aEvent event;
    event.clear();
    event.type = 0x13;
    event.data = 0;
    ReadyRoomPlayerListHandleEvent(globalLogPtr->connectScreen->elements[3], &event);
    event.clear();
    event.type = 0x1e;
    event.data = 4;
    PlayerListHandleEvent(globalLogPtr->lanScreen->elements[3], &event);
}

void JoinGame()
{
    if (MPlayer == nullptr || MPlayer->sessionManager == nullptr)
    {
        return;
    }

    SessionManager* manager = MPlayer->sessionManager;
    _GUID* game = element<GameList>(globalLogPtr->lanScreen, 2)->getSelectedGame();

    if (game != nullptr)
    {
        FIDPSession* session = manager->FindMatchingSession(game);

        if (session != nullptr)
        {
            char* playerName = elementText(globalLogPtr->lanScreen, 4);
            SaveUserName(playerName);

            if (session->sessionDesc.dwCurrentPlayers < session->sessionDesc.dwMaxPlayers)
            {
                const int32_t result = manager->JoinSession(&session->sessionDesc.guidInstance, playerName);
                countLANPlayers();

                if (result == 0)
                {
                    enterReadyRoom(globalLogPtr->lanScreen, 1);
                    readyRoomTicks = 0;
                    ResetReadyRoom();
                    return;
                }
            }
        }
    }

    // The game can't be joined.
    showMessage(0xa7, nullptr);
}

void CreateSession()
{
    if (MPlayer != nullptr && MPlayer->sessionManager != nullptr)
    {
        char* sessionName = elementText(globalLogPtr->lanScreen, 10);
        int32_t maxPlayers = std::atoi(elementText(globalLogPtr->lanScreen, 11));

        if (maxPlayers < 2)
        {
            maxPlayers = 2;
        }
        else if (maxPlayers > 6)
        {
            maxPlayers = 6;
        }

        MPlayer->createSession(sessionName, elementText(globalLogPtr->lanScreen, 4), maxPlayers);
    }

    globalLogPtr->lanScreen->ShowGUIWindow(0);
    globalLogPtr->connectScreen->ShowGUIWindow(1);
    readyRoomTicks = 0;
    globalLogPtr->currentScreen = globalLogPtr->connectScreen;
    globalLogPtr->logisticsState = 0xe;
    ResetReadyRoom();
}

void CreateSerialSession()
{
    if (MPlayer == nullptr || MPlayer->sessionManager == nullptr)
    {
        return;
    }

    char* portText = elementText(globalLogPtr->serialScreen, 5);

    if (portText == nullptr)
    {
        return;
    }

    const auto port = static_cast<uint32_t>(std::atoi(portText));

    if (static_cast<int32_t>(port) < 1 || static_cast<int32_t>(port) > 4)
    {
        return;
    }

    MPlayer->sessionManager->ConnectComPort(port, 0xe100, 0, 0, 4);
    char* playerName = elementText(globalLogPtr->serialScreen, 4);
    SaveUserName(playerName);

    if (MPlayer->createSession(const_cast<char*>("SerialGame"), playerName, 2) == 0)
    {
        enterReadyRoom(globalLogPtr->serialScreen, -1);
    }
}

void SerialJoinButtonPressed()
{
    if (MPlayer == nullptr)
    {
        return;
    }

    SaveUserName(elementText(globalLogPtr->serialScreen, 4));
    whackTimer = 1;
    char* portText = elementText(globalLogPtr->serialScreen, 5);

    if (portText == nullptr)
    {
        return;
    }

    const auto port = static_cast<uint32_t>(std::atoi(portText));

    if (static_cast<int32_t>(port) > 0 && static_cast<int32_t>(port) < 5 &&
        MPlayer->sessionManager->ConnectComPort(port, 0xe100, 0, 0, 4) == 0)
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

    if (MPlayer->joinSession(const_cast<char*>("SerialGame"), elementText(globalLogPtr->serialScreen, 4)) != 0)
    {
        // No game yet: try again in a second, with a way out.
        application->AddTimer(globalLogPtr->serialScreen, 0, 1000, 0, 0, 0);
        whackTimer = 0;
        char text[256];
        cLoadString(thisInstance, 0xb1, text, 0xfe);
        ReusableDialog* dialog = globalLogPtr->messageDialog;
        dialog->setText(text);
        dialog->setTwoButton(0);
        dialog->callback = nullptr;
        dialog->okButton->setUpPicture(const_cast<char*>("bh_cancl.tga"));
        dialog->okButton->setDownPicture(const_cast<char*>("bg_cancl.tga"));
        dialog->okButton->callback()->setExec(CancelToConnect);
        dialog->activate();
        return;
    }

    countLANPlayers();
    enterReadyRoom(globalLogPtr->serialScreen, 1);
    globalLogPtr->messageDialog->deactivate(0);
}

void JoinModemSession()
{
    if (MPlayer->joinSession(const_cast<char*>("MC Modem Game"), elementText(globalLogPtr->modemScreen, 4)) != 0)
    {
        application->AddTimer(globalLogPtr->modemScreen, 1, 1000, 0, 0, 0);
        whackTimer = 0;
        return;
    }

    countLANPlayers();
    enterReadyRoom(globalLogPtr->modemScreen, 1);
    globalLogPtr->messageDialog->deactivate(0);
    whackTimer = 1;
}

int32_t DialModemSession()
{
    if (MPlayer == nullptr)
    {
        return -1;
    }

    SaveUserName(elementText(globalLogPtr->modemScreen, 4));
    const int32_t result = MPlayer->sessionManager->Dial();

    if (result == 0)
    {
        JoinModemSession();
        return 0;
    }

    application->AddTimer(globalLogPtr->modemScreen, 0, 1000, 0, 0, 0);
    whackTimer = 0;
    // 2 while the line is still dialling (DirectPlay's DPERR_CONNECTING, 0x8877015e), else 1.
    return (result == static_cast<int32_t>(0x8877015e)) ? 2 : 1;
}

void AllGoneCallback(int32_t)
{
    char text[256];
    cLoadString(thisInstance, 0xbb, text, 0xfe);
    ReusableDialog* dialog = globalLogPtr->messageDialog;
    dialog->setText(text);
    dialog->setTwoButton(0);
    dialog->callback = nullptr;
    dialog->okButton->setUpPicture(const_cast<char*>("bh_okay.tga"));
    dialog->okButton->setDownPicture(const_cast<char*>("bg_okay.tga"));
    dialog->okButton->callback()->setExec(nullptr);
    dialog->activate();
}

void GOCallback()
{
    if (MPlayer == nullptr)
    {
        return;
    }

    globalLogPtr->currentScreen->ShowGUIWindow(0);
    globalLogPtr->currentScreen = globalLogPtr->sessionScreen;
    globalLogPtr->showLogScreen(1, 1);
    globalLogPtr->logisticsState = 8;
    globalLogPtr->sessionScreen->activate(0);

    if (MPlayer != nullptr)
    {
        MPlayer->sessionManager->LockSession();
        return;
    }

    globalLogPtr->messageDialog->callback = AllGoneCallback;
}

void GO()
{
    if (MPlayer == nullptr || MPlayer->sessionManager == nullptr)
    {
        return;
    }

    SessionManager* manager = MPlayer->sessionManager;
    FIDPSession* session = manager->currentSession;

    if (session == nullptr)
    {
        return;
    }

    int32_t maxPlayers;
    const int32_t connection = manager->currentConnection;

    if (connection == 2 || connection == 1)
    {
        maxPlayers = std::atoi(elementText(globalLogPtr->lanScreen, 11));

        if (maxPlayers < 2)
        {
            maxPlayers = 2;
        }
        else if (maxPlayers > 6)
        {
            maxPlayers = 6;
        }
    }
    else
    {
        maxPlayers = connection == 0x10 ? 6 : 2;
    }

    const uint32_t players = session->sessionDesc.dwCurrentPlayers;
    Assert(static_cast<int32_t>(players) <= maxPlayers, players, " How'd we get too many players? ", nullptr);
    GOCallback();
}

void Leave()
{
}

void WaitForCall()
{
    if (MPlayer != nullptr && MPlayer->sessionManager != nullptr)
    {
        auto* modems = element<lScrollTextObject>(globalLogPtr->modemScreen, 10);
        char modem[256];

        if (modems->getTextLine(modems->highlightLine[0] + 1, modem, 0xff) != 0)
        {
            MPlayer->sessionManager->ConnectModem(const_cast<char*>(""), modem);
            char* playerName = elementText(globalLogPtr->modemScreen, 4);
            SaveUserName(playerName);
            MPlayer->createSession(const_cast<char*>("MC Modem Game"), playerName, 2);
        }
    }

    element<lTextObject>(globalLogPtr->lanScreen, 11)->initBuffer(2, 1);
    globalLogPtr->modemScreen->ShowGUIWindow(0);
    setDisabled(element<lButton>(globalLogPtr->connectScreen, 2), 0);
    globalLogPtr->connectScreen->ShowGUIWindow(1);
    globalLogPtr->currentScreen = globalLogPtr->connectScreen;
    globalLogPtr->logisticsState = 0xe;
    application->setText(globalLogPtr->modemScreen->elements[4]);
    application->AddTimer(globalLogPtr->modemScreen->elements[4], 0, MCPort::CaretBlinkTime(), 0, 0, 0);
}

void GetNumber()
{
    application->setText(globalLogPtr->modemScreen->elements[5]);
    globalLogPtr->modemScreen->showBlock(1);
}

void CancelDial()
{
    whackTimer = 1;

    if (MPlayer != nullptr)
    {
        MPlayer->sessionManager->CancelDialing();
    }
}

void Dial()
{
    GenericScreen* screen = globalLogPtr->modemScreen;
    auto* modems = element<lScrollTextObject>(screen, 10);

    if (MPlayer == nullptr || MPlayer->sessionManager == nullptr)
    {
        return;
    }

    char modem[512];

    if (modems->getTextLine(modems->highlightLine[0] + 1, modem, 0xff) == 0)
    {
        return;
    }

    MPlayer->sessionManager->ConnectModem(elementText(screen, 5), modem);
    const int32_t result = DialModemSession();
    Assert(result != 1, 0, "Not currently connected to a modem", nullptr);

    switch (result)
    {
        case 0:
        {
            enterReadyRoom(globalLogPtr->modemScreen, 1);
            return;
        }
        case 1:
        case 3:
        {
            showMessage(0xb3, nullptr);
            return;
        }
        case 2:
        {
            // Still dialling: check again in a second; the button cancels.
            application->AddTimer(globalLogPtr->modemScreen, 0, 1000, 0, 0, 0);
            whackTimer = 0;
            char text[256];
            cLoadString(thisInstance, 0xb2, text, 0xfe);
            ReusableDialog* dialog = globalLogPtr->messageDialog;
            dialog->setText(text);
            dialog->setTwoButton(0);
            dialog->callback = nullptr;
            dialog->okButton->setUpPicture(const_cast<char*>("bh_cancl.tga"));
            dialog->okButton->setDownPicture(const_cast<char*>("bg_cancl.tga"));
            setDisabled(dialog->okButton, 0);
            dialog->okButton->callback()->setExec(CancelDial);
            dialog->activate();
            return;
        }

        default:
            return;
    }
}

void ImageHandleEvent(aObject* object, aEvent* event)
{
    if (object->parent != nullptr)
    {
        object->parent->handleEvent(event);
    }
}

void ModemListHandleEvent(aObject* object, aEvent* event)
{
    if (event->type != 1)
    {
        return;
    }

    // A click picks the modem on the line under the mouse.
    auto* list = static_cast<lScrollTextObject*>(object);
    const int32_t lineHeight = fonts[0][list->fontIndex]->height();
    const int32_t line = (list->firstPixel + event->y - list->globalY()) / (lineHeight + 4);

    if (list->getTextLine(line + 1, nullptr, 0) != 0)
    {
        list->highlightLine[0] = line;
    }
}

void PlayerListHandleEvent(aObject* object, aEvent* event)
{
    if (event->type != 0x1e)
    {
        return;
    }

    auto* list = static_cast<lScrollTextObject*>(object);

    if (event->data == 3)
    {
        // A game was picked: list its players.
        if (object->parent == nullptr || MPlayer == nullptr)
        {
            return;
        }

        if (MPlayer->sessionManager != nullptr)
        {
            auto* games = element<GameList>(static_cast<GenericScreen*>(object->parent), 2);
            list->Clear();

            if (games != nullptr && games->selectedSession > -1)
            {
                FIDPSession* session = MPlayer->sessionManager->FindMatchingSession(games->getSelectedGame());
                FLinkedList<FIDPPlayer>* players =
                    session != nullptr ? MPlayer->sessionManager->GetPlayers(session) : nullptr;

                if (players != nullptr)
                {
                    const int32_t count = players->count;
                    players->current = players->head;

                    for (int32_t i = count; i > 0; i--)
                    {
                        FIDPPlayer* player = players->ReadAndNext();
                        list->Print(player->name, 0x1f);
                    }
                }
            }
        }
    }
    else if (event->data == 4)
    {
        list->Clear();
    }
}

void ReadyRoomPlayerListHandleEvent(aObject* object, aEvent* event)
{
    if (event->type != 0x13 || MPlayer == nullptr)
    {
        return;
    }

    SessionManager* manager = MPlayer->sessionManager;

    if (manager == nullptr)
    {
        return;
    }

    auto* list = static_cast<lScrollTextObject*>(object);
    FIDPSession* session = manager->currentSession;
    ++readyRoomTicks;

    if (session != nullptr)
    {
        if (manager->isHost == 0 && launchedFromLobby == 0)
        {
            manager->SendPing();
        }

        list->Clear();
        FLinkedList<FIDPPlayer>* players = MPlayer->sessionManager->GetPlayers(session);

        if (players != nullptr)
        {
            // Each player, with the ping outside a lobby launch.
            for (FLink<FIDPPlayer>* link = players->head; link != nullptr && link->data != nullptr; link = link->next)
            {
                FIDPPlayer* player = link->data;

                if (launchedFromLobby == 0)
                {
                    char line[256];
                    std::snprintf(line, sizeof(line), "%s - %04d ms", player->name, player->lastLatency);
                    list->Print(line, 0x1f);
                }
                else
                {
                    list->Print(player->name, 0x1f);
                }
            }

            // The host can go once someone else is in.
            if (MPlayer->sessionManager->isHost != 0)
            {
                setDisabled(element<lButton>(globalLogPtr->connectScreen, 2), players->count < 2 ? 1 : 0);
            }
        }
    }
}

void LanScreenHandleEvent(aObject* object, aEvent* event)
{
    auto* screen = static_cast<GenericScreen*>(object);

    if (event->type == 0x13)
    {
        screen->elements[2]->handleEvent(event);
        screen->elements[3]->handleEvent(event);
        return;
    }

    if (event->type != 0x1e)
    {
        return;
    }

    auto* joinButton = element<lButton>(screen, 6);

    if (event->data == 3)
    {
        // A game was picked: its players are listed, and it can be joined unless full.
        FIDPSession* session =
            MPlayer->sessionManager->FindMatchingSession(element<GameList>(screen, 2)->getSelectedGame());
        screen->elements[3]->handleEvent(event);
        setDisabled(joinButton, session->sessionDesc.dwMaxPlayers <= session->sessionDesc.dwCurrentPlayers ? 1 : 0);
        return;
    }

    if (event->data == 4)
    {
        screen->elements[3]->handleEvent(event);
        setDisabled(joinButton, 1);
    }
}

void LoadSaveScreenHandleEvent(aObject* object, aEvent* event)
{
    if (event->type != 0x1e)
    {
        return;
    }

    auto* screen = static_cast<GenericScreen*>(object);
    const int32_t message = event->data;

    if (message == 1)
    {
        // A file was picked: it can be loaded (or saved over); deleted unless it is the empty entry.
        setDisabled(screen->loadSaveButton, 0);
        lButton* deleteButton = screen->deleteButton;
        setDisabled(deleteButton, 1);
        FileScrollPane* pane = screen->filePane;

        if (hasSelection(pane) && std::strcmp(pane->fileNames[pane->selectedFile], EmptyFile) != 0)
        {
            setDisabled(deleteButton, 0);
        }
    }
    else if (message == 2)
    {
        setDisabled(screen->loadSaveButton, 1);
        setDisabled(screen->deleteButton, 1);
    }
    else if (message == 5 && object == globalLogPtr->saveScreen)
    {
        // Enter in the name entry: save.
        soundSystem->playDigitalSample(screen->loadSaveButton->pressSound, 1, nullptr, 0, 0);
        SaveGame();
    }
}

void ComPortTextHandleEvent(aObject* object, aEvent* event)
{
    if (event->type != 10)
    {
        return;
    }

    // The port number typed is checked (1 to 4); a bad one is backspaced out. The screen hears whether it is good.
    auto* entry = static_cast<lTextObject*>(object);
    int32_t valid = 0;
    char* text = entry->buffer;

    if (text != nullptr && text[0] != '\0' && event->lParam != 999)
    {
        if (std::atoi(text) < 5 && std::atoi(text) != 0)
        {
            valid = 1;
        }
        else
        {
            aEvent backspace;
            backspace.type = 10;
            backspace.key = 8;
            backspace.lParam = 999;
            object->handleEvent(&backspace);
        }
    }

    aEvent notify;
    notify.type = 0x1e;
    notify.data = 6;
    notify.lParam = valid;
    object->parent->handleEvent(&notify);
}

void SerialScreenHandleEvent(aObject* object, aEvent* event)
{
    if (event->type == 0x13)
    {
        application->RemoveTimer(object, 0);

        if (whackTimer == 0)
        {
            JoinSerialSession();
        }

        return;
    }

    if (event->type == 0x1e && event->data == 6)
    {
        // The host and join buttons need a good port number.
        const int disabled = event->lParam == 0 ? 1 : 0;
        auto* screen = static_cast<GenericScreen*>(object);
        setDisabled(element<lButton>(screen, 2), disabled);
        setDisabled(element<lButton>(screen, 3), disabled);
    }
}

void ModemScreenHandleEvent(aObject* object, aEvent* event)
{
    if (event->type != 0x13)
    {
        return;
    }

    if (event->data == 0)
    {
        application->RemoveTimer(object, 0);

        if (whackTimer == 0)
        {
            DialModemSession();
        }
    }
    else if (event->data == 1)
    {
        application->RemoveTimer(object, 1);

        if (whackTimer == 0)
        {
            JoinModemSession();
        }
    }
}

void PrefScreenHandleEvent(aObject*, aEvent*)
{
}

void SlideScreenBrightness(aObject* object, aEvent*)
{
    application->gammaCorrectCurrentPalette(static_cast<lSlider*>(object)->currentValue);
}

void SlideMusicVolume(aObject* object, aEvent* event)
{
    if (event->type != 4)
    {
        return;
    }

    const auto volume = static_cast<uint8_t>(static_cast<lSlider*>(object)->currentValue);

    if (volume < 0x80)
    {
        soundSystem->musicVolume = volume;
    }

    soundSystem->stopDigitalMusic();
    soundSystem->playDigitalMusic(0x16, true);
}

void SlideRadioVolume(aObject* object, aEvent* event)
{
    if (event->type != 4)
    {
        return;
    }

    const auto volume = static_cast<uint8_t>(static_cast<lSlider*>(object)->currentValue);

    if (volume < 0x80)
    {
        soundSystem->radioVolume = volume;
    }

    soundSystem->playPilotSpeech(const_cast<char*>("pilotd"), 0x15);
}

void SlideFXVolume(aObject* object, aEvent* event)
{
    if (event->type != 4)
    {
        return;
    }

    const auto volume = static_cast<uint8_t>(static_cast<lSlider*>(object)->currentValue);

    if (volume < 0x80)
    {
        soundSystem->digitalMasterVolume = volume;
    }

    soundSystem->playDigitalSample(0xf, 1, nullptr, 0, 0);
}

namespace
{
    /// <summary>Lights difficulty toggle <paramref name="difficulty"/> (easy, regular, hard) and sets the difficulty.</summary>
    void setDifficulty(int32_t difficulty)
    {
        GenericScreen* screen = globalLogPtr->prefScreen;

        for (int32_t i = 0; i < 3; i++)
        {
            auto* toggle = element<lToolButton>(screen, 8 + i);
            toggle->toggled = i == difficulty ? 1 : 0;
        }

        GameDifficulty = difficulty;
    }
}

void EasyToggle()
{
    setDifficulty(0);
}

void RegularToggle()
{
    setDifficulty(1);
}

void HardToggle()
{
    setDifficulty(2);
}

void DoExit()
{
    if (MPlayer != nullptr)
    {
        if (launchedFromLobby != 0)
        {
            killTheGame();
            return;
        }

        globalLogPtr->destroyMultiplayer();
        MPlayer->leaveSession();
    }

    globalLogPtr->setUpMainScreen(0);
}

void CheckExit()
{
    char text[256];
    cLoadString(thisInstance, 0xaf, text, 0xfe);
    ReusableDialog* dialog = globalLogPtr->messageDialog;
    dialog->setText(text);
    dialog->setTwoButton(1);
    // The dialog's callback and the cancel button's action are left as they were.
    dialog->okButton->setUpPicture(const_cast<char*>("bh_okay.tga"));
    dialog->okButton->setDownPicture(const_cast<char*>("bg_okay.tga"));
    setDisabled(dialog->okButton, 0);
    dialog->okButton->callback()->setExec(DoExit);
    dialog->cancelButton->setUpPicture(const_cast<char*>("bh_cancl.tga"));
    dialog->cancelButton->setDownPicture(const_cast<char*>("bh_cancl.tga"));
    setDisabled(dialog->cancelButton, 0);
    dialog->activate();
}

void SaveUserName(char* name)
{
    if (name != nullptr)
    {
        MCRegistry::Write("Software\\FASA Interactive\\MechCommander Expansion", "Player Name", name);
    }
}
