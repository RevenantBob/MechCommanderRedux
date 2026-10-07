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
char InterfacePath[80] = {};
char SavePath[80] = {};
char CDspritePath[80] = {};
char TerrainPath[80] = {};
char WarriorPath[80] = {};
char SpritePath[80] = {};
char ProfilePath[80] = {};
char FontPath[80] = {};
char DirectXPath[80] = {};
char SoundPath[80] = {};
char ShapesPath[80] = {};
void* ThisInstance = nullptr;
int Only45Pixel = 0;
MCLogistics* GlobalLogPtr = nullptr;
int32_t LastLogisticsMissionState = 0;
int LoadingSolo = 0;
int WhackTimer = 0;
int Force32MB = 0;
int Force16MB = 0;
int32_t ReadyRoomTicks = 0;

namespace
{
    /// <summary>The registry key of the game's version and language.</summary>
    constexpr const char* VersionKey = "Software\\Fasa Interactive\\MechCommander Expansion";

    /// <summary>Screen element <paramref name="index"/> of <paramref name="screen"/> as a <typeparamref name="T"/>.</summary>
    template <typename T> T* Element(MCGenericScreen* screen, int32_t index)
    {
        return static_cast<T*>(screen->Elements[index]);
    }

    /// <summary>The text typed in text element <paramref name="index"/> of <paramref name="screen"/>.</summary>
    char* ElementText(MCGenericScreen* screen, int32_t index)
    {
        return Element<MCLogTextObject>(screen, index)->Buffer;
    }

    /// <summary>Enables or disables <paramref name="button"/> (it shows the change on the next frame).</summary>
    void SetDisabled(MCLogButton* button, int disabled)
    {
        button->Disabled = disabled;
    }

    /// <summary>Hides the screen shown and shows <paramref name="screen"/> as logistics state <paramref name="state"/>.</summary>
    void SwitchScreen(MCLogObject* from, MCLogObject* screen, int32_t state)
    {
        from->ShowGuiWindow(0);
        screen->ShowGuiWindow(1);
        GlobalLogPtr->CurrentScreen = screen;
        GlobalLogPtr->LogisticsState = state;
    }

    /// <summary>
    /// The one-button message dialog with string <paramref name="id"/>: an OK button (<paramref name="upArt"/> /
    /// <paramref name="downArt"/>) and <paramref name="callback"/> for the answer.
    /// </summary>
    void ShowMessage(uint32_t id, void (*callback)(int32_t), const char* upArt = "bh_okay.tga",
                     const char* downArt = "bg_okay.tga")
    {
        char text[256];
        CLoadString(ThisInstance, id, text, 0xfe);
        MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
        dialog->SetText(text);
        dialog->SetTwoButton(0);
        dialog->Callback = callback;
        dialog->OkButton->SetUpPicture(const_cast<char*>(upArt));
        dialog->OkButton->SetDownPicture(const_cast<char*>(downArt));
        SetDisabled(dialog->OkButton, 0);
        dialog->Activate();
    }

    /// <summary>
    /// The two-button question dialog <paramref name="dialog"/> with string <paramref name="id"/>: OK runs
    /// <paramref name="okExec"/>, cancel <paramref name="cancelExec"/>; the cancel button shows
    /// <paramref name="cancelDownArt"/> when pressed. With <paramref name="enableButtons"/> both buttons are
    /// enabled and redrawn.
    /// </summary>
    void AskQuestion(MCReusableDialog* dialog, uint32_t id, void (*okExec)(), void (*cancelExec)(),
                     const char* cancelDownArt, bool enableButtons)
    {
        char text[256];
        CLoadString(ThisInstance, id, text, 0xfe);
        dialog->SetText(text);
        dialog->SetTwoButton(1);
        dialog->Callback = nullptr;
        dialog->OkButton->SetUpPicture(const_cast<char*>("bh_okay.tga"));
        dialog->OkButton->SetDownPicture(const_cast<char*>("bg_okay.tga"));

        if (enableButtons)
        {
            SetDisabled(dialog->OkButton, 0);
        }

        dialog->OkButton->Callback()->SetExec(okExec);
        dialog->CancelButton->SetUpPicture(const_cast<char*>("bh_cancl.tga"));
        dialog->CancelButton->SetDownPicture(const_cast<char*>(cancelDownArt));

        if (enableButtons)
        {
            SetDisabled(dialog->CancelButton, 0);
        }

        dialog->CancelButton->Callback()->SetExec(cancelExec);
        dialog->Activate();
    }

    /// <summary>The player's name for the multiplayer screens: the one remembered, else "Player".</summary>
    bool UserName(char* name, uint32_t size)
    {
        uint32_t length = size;
        return MyGetUserName(name, &length) != 0;
    }

    /// <summary>The file pane of the screen being shown (the load or save screen), or null.</summary>
    MCFileScrollPane* ShownFilePane()
    {
        MCFileScrollPane* pane = nullptr;

        if (GlobalLogPtr->CurrentScreen == GlobalLogPtr->LoadScreen)
        {
            pane = GlobalLogPtr->LoadScreen->FilePane;
        }

        if (GlobalLogPtr->CurrentScreen == GlobalLogPtr->SaveScreen)
        {
            pane = GlobalLogPtr->SaveScreen->FilePane;
        }

        return pane;
    }

    /// <summary>Whether <paramref name="pane"/> has a file selected.</summary>
    bool HasSelection(const MCFileScrollPane* pane)
    {
        return pane != nullptr && pane->SelectedFile > -1 && pane->SelectedFile < pane->NumFiles;
    }

    /// <summary>Takes down the file pane's name entry, if it has one (its <c>destroy</c>).</summary>
    void DestroyNameEntry(MCFileScrollPane* pane)
    {
        if (pane->NameEntry != nullptr)
        {
            pane->NameEntry->Destroy();
        }
    }

    /// <summary>A copy of <paramref name="text"/> in a logistics block.</summary>
    char* HeapCopy(const char* text)
    {
        auto* copy =
            static_cast<char*>(GlobalLogPtr->LogisticsBlocks->Allocate(static_cast<uint32_t>(std::strlen(text) + 1)));
        std::strcpy(copy, text);
        return copy;
    }

    /// <summary>Opens the ready room (session screen) after a session was joined or created.</summary>
    void EnterReadyRoom(MCLogObject* from, int disableGo)
    {
        from->ShowGuiWindow(0);
        GlobalLogPtr->ConnectScreen->ShowGuiWindow(1);
        GlobalLogPtr->CurrentScreen = GlobalLogPtr->ConnectScreen;
        GlobalLogPtr->LogisticsState = 0xe;

        if (disableGo >= 0)
        {
            SetDisabled(Element<MCLogButton>(GlobalLogPtr->ConnectScreen, 2), disableGo);
        }
    }

    /// <summary>After a session was joined: how many are in it.</summary>
    void CountLanPlayers()
    {
        NumLanPlayers = MPlayer->SessionManager->GetPlayers(nullptr)->Count;
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

void McxCampaignCDTester(int32_t result)
{
    if (result == 1)
    {
        NewMcxCampaign();
    }
}

void MpxCampaignCDTester(int32_t result)
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
    CLoadString(ThisInstance, 0x282, expected, 99);
    return std::strncmp(version, expected, 0xd) == 0 && version[13] == '.';
}

void WriteRegistryVersionNumber()
{
    char text[100];
    char version[100];
    CLoadString(ThisInstance, 0x282, text, 99);
    std::snprintf(version, sizeof(version), "%s.", text);
    MCRegistry::Write(VersionKey, "Version", version);
    CLoadString(ThisInstance, 900, text, 99);
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

    SoundSystem->StopDigitalMusic();
    SoundSystem->PlayBettySample(0x19);
    std::strcpy(MissionName, "mechcmdr1");
    Mission->InitAgain(MissionName);
    CurPlanet = 0;
    Solo = 0;
    LastLogisticsMissionState = 0;
    GlobalLogPtr->LoadCampaign(const_cast<char*>("start0"), const_cast<char*>(".pkk"), 0, 0);
    GlobalLogPtr->BriefingScreen->BriefingBox = nullptr;
    GlobalLogPtr->SetUpBriefingScreen(0);
    GlobalLogPtr->MainScreen->ShowGuiWindow(0);
}

void NewMcxCampaign()
{
    if (!CheckRegistryVersionNumber())
    {
        WriteRegistryVersionNumber();
    }

    SoundSystem->StopDigitalMusic();
    SoundSystem->PlayBettySample(0x19);
    std::strcpy(MissionName, "xmechcmdr1");
    Mission->InitAgain(MissionName);
    Solo = 0;
    LastLogisticsMissionState = 0;
    GlobalLogPtr->LoadCampaign(const_cast<char*>("xstart0"), const_cast<char*>(".pkk"), 0, 0);
    GlobalLogPtr->BriefingScreen->BriefingBox = nullptr;
    GlobalLogPtr->SetUpBriefingScreen(0);
    GlobalLogPtr->MainScreen->ShowGuiWindow(0);
}

void SaveScreen()
{
    if (!CheckRegistryVersionNumber())
    {
        WriteRegistryVersionNumber();
    }

    if (GlobalLogPtr->CurrentMission == -1)
    {
        return;
    }

    GlobalLogPtr->MainScreen->ShowGuiWindow(0);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->SaveScreen;
    GlobalLogPtr->SaveScreen->ShowGuiWindow(1);
    GlobalLogPtr->LogisticsState = 6;
    GlobalLogPtr->SaveScreen->FilePane->SetSelectedFile(-1);
}

void ConnectScreen()
{
    if (!CheckRegistryVersionNumber())
    {
        WriteRegistryVersionNumber();
    }

    MCGenericScreen* screen = GlobalLogPtr->MultiplayerScreen;

    if (MPlayer == nullptr)
    {
        auto* player = new MCMultiPlayer;
        MPlayer = player;
        Assert(player != nullptr, 0, " Unable to create MultiPlayer object ", nullptr);
        MPlayer->Init(0x7d000, 0x100, 100);
    }

    GlobalLogPtr->MainScreen->ShowGuiWindow(0);
    GlobalLogPtr->MultiplayerScreen->ShowGuiWindow(1);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->MultiplayerScreen;
    GlobalLogPtr->LogisticsState = 10;

    // The connection buttons: modem, serial, LAN, internet; each is enabled when the machine has it.
    auto* modemButton = Element<MCLogButton>(screen, 2);
    auto* serialButton = Element<MCLogButton>(screen, 3);
    auto* lanButton = Element<MCLogButton>(screen, 4);
    auto* internetButton = Element<MCLogButton>(screen, 5);
    SetDisabled(modemButton, 1);
    SetDisabled(serialButton, 1);
    SetDisabled(lanButton, 1);
    SetDisabled(internetButton, 1);

    if (MPlayer != nullptr && MPlayer->SessionManager != nullptr)
    {
        MCSessionManager* manager = MPlayer->SessionManager;

        if (manager->IsModemAvailable() != 0)
        {
            SetDisabled(modemButton, 0);
        }

        if ((manager->AvailableProtocols & 4) != 0)
        {
            SetDisabled(serialButton, 0);
        }

        if (manager->IsIpxAvailable() != 0 || manager->IsTcpAvailable() != 0)
        {
            SetDisabled(lanButton, 0);
        }

        if ((manager->AvailableProtocols & 0x10) != 0)
        {
            SetDisabled(internetButton, 0);
        }
    }

    // Less than 32 MB: multiplayer may not run well.
    if (MCPort::TotalPhysicalMemory() < 32000000)
    {
        ShowMessage(0x371, nullptr);
    }
}

void LoadScreen()
{
    if (!CheckRegistryVersionNumber())
    {
        WriteRegistryVersionNumber();
    }

    GlobalLogPtr->MainScreen->ShowGuiWindow(0);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->LoadScreen;
    GlobalLogPtr->LoadScreen->ShowGuiWindow(1);
    GlobalLogPtr->LogisticsState = 5;
    GlobalLogPtr->LoadScreen->FilePane->SetSelectedFile(-1);
}

void SoloLoadScreen()
{
    if (!CheckRegistryVersionNumber())
    {
        WriteRegistryVersionNumber();
    }

    GlobalLogPtr->MainScreen->ShowGuiWindow(0);
    LoadingSolo = 1;
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->LoadScreen;
    GlobalLogPtr->LoadScreen->ShowGuiWindow(1);
    GlobalLogPtr->LogisticsState = 5;
    GlobalLogPtr->LoadScreen->FilePane->SetSelectedFile(-1);
}

namespace
{
    // Port: the preferences screen's DIFFICULTY and RENDERER choices are drop-downs, each in a box drawn as the panel
    // art's DIFFICULTY box (prefs_00.tga): an outline of colour 0x13 on the panel's 0x10, the title in 0xe3. The
    // DIFFICULTY box is drawn over the art's, which holds the original's three checks and labels; the checks
    // (elements 8..10) are hidden.

    /// <summary>A box's place on the screen and its size.</summary>
    struct MCPrefsBoxPlace
    {
        /// <summary>The top left corner on the screen.</summary>
        int32_t Left = 0;
        int32_t Top = 0;
        /// <summary>The size, outline included.</summary>
        int32_t Width = 0;
        int32_t Height = 0;
    };

    /// <summary>DIFFICULTY's box is the art's; RENDERER's is under it, over ACCEPT.</summary>
    constexpr MCPrefsBoxPlace DifficultyBoxPlace{480, 155, 120, 63};
    constexpr MCPrefsBoxPlace RendererBoxPlace{480, 222, 120, 50};

    /// <summary>Where a drop-down lies in its box (under the title, the art's checks' column and first row), and its width.</summary>
    constexpr int32_t DropDownLeft = 32;
    constexpr int32_t DropDownTop = 21;
    constexpr int32_t DropDownWidth = 76;

    /// <summary>A preferences box: its back, outline and title, drawn each frame.</summary>
    class MCPrefsBox : public MCLogObject
    {
    public:
        /// <summary>A box titled <paramref name="title"/>.</summary>
        explicit MCPrefsBox(const char* title) : _Title(title) {}

        void Draw() override
        {
            const auto right = static_cast<int16_t>(Width() - 1);
            const auto bottom = static_cast<int16_t>(Height() - 1);
            FillBox(0, 0, right, bottom, 0x10);
            FillBox(0, 0, right, 0, 0x13);
            FillBox(0, bottom, right, bottom, 0x13);
            FillBox(0, 0, 0, bottom, 0x13);
            FillBox(right, 0, right, bottom, 0x13);
            // (The font's glyphs start a column in, so the text is written a pixel left of the art's.)
            VfxStringDraw(Lport()->Frame(), 32, 9, WhiteFont->FontData.get(), _Title, MCLogComboBox::LabelColors());
        }

        bool DrawsLive() override { return true; }

        /// <summary>
        /// The box is part of the panel, as the art's boxes are: the mouse goes through it. (As an object it took
        /// clicks, and a click brought it in front of the controls on it, which then took no more.)
        /// </summary>
        MCGuiObject* FindObject(int32_t xPos, int32_t yPos) override
        {
            (void)xPos;
            (void)yPos;
            return nullptr;
        }

    private:
        /// <summary>The title.</summary>
        const char* _Title = nullptr;
    };

    /// <summary>The choice when the screen opened (for CancelPrefs).</summary>
    int32_t SavedRendererPreference = 0;

    /// <summary>Says, in the message dialog, that the renderer chosen takes effect at the next start.</summary>
    void ShowRestartNotice()
    {
        char text[] = "The new renderer takes effect when you restart MechCommander.";
        MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
        dialog->SetText(text);
        dialog->SetTwoButton(0);
        dialog->Callback = nullptr;
        dialog->OkButton->SetUpPicture(const_cast<char*>("bh_okay.tga"));
        dialog->OkButton->SetDownPicture(const_cast<char*>("bg_okay.tga"));
        dialog->OkButton->Disabled = 0;
        dialog->Activate();
    }

    /// <summary>The RENDERER drop-down chose <paramref name="renderer"/>: another one than this run's needs a restart.</summary>
    void RendererChanged(int32_t renderer)
    {
        if (renderer != GRenderer)
        {
            ShowRestartNotice();
        }
    }

    /// <summary>Adds a box titled <paramref name="title"/> at <paramref name="place"/> to <paramref name="screen"/>.</summary>
    void AddPrefsBox(MCGenericScreen* screen, const MCPrefsBoxPlace& place, const char* title)
    {
        auto* box = new MCPrefsBox(title);
        box->Init(place.Left, place.Top, place.Width, place.Height, nullptr, nullptr);
        box->SetTransparent(-1);
        box->ShowGuiWindow(-1);
        screen->AddChild(box);
    }

    /// <summary>Adds a drop-down editing <paramref name="setting"/> in the box at <paramref name="place"/>.</summary>
    MCLogComboBox* AddDropDown(MCGenericScreen* screen, const MCPrefsBoxPlace& place, int32_t* setting,
                               std::vector<MCLogComboBox::Item> items, void (*changed)(int32_t value))
    {
        auto* dropDown = new MCLogComboBox;
        dropDown->Init(place.Left + DropDownLeft, place.Top + DropDownTop, DropDownWidth, setting, std::move(items),
                       changed);
        dropDown->ShowGuiWindow(-1);
        screen->AddChild(dropDown);
        return dropDown;
    }
}

void AddPreferenceDropDowns(MCGenericScreen* screen)
{
    // The original's DIFFICULTY checks (easy, regular, hard) give way to the drop-down.
    for (int32_t i = 0; i < 3; i++)
    {
        Element<MCLogToolButton>(screen, 8 + i)->ShowGuiWindow(0);
    }

    AddPrefsBox(screen, DifficultyBoxPlace, "DIFFICULTY");
    AddPrefsBox(screen, RendererBoxPlace, "RENDERER");
    AddDropDown(screen, DifficultyBoxPlace, &GameDifficulty, {{"EASY", 0}, {"REGULAR", 1}, {"HARD", 2}}, nullptr);
    AddDropDown(screen, RendererBoxPlace, &GRendererPreference,
                {{"VULKAN", static_cast<int32_t>(MCRendererKind::Vulkan)},
                 {"SOFTWARE", static_cast<int32_t>(MCRendererKind::Software)}},
                RendererChanged);
}

void ShowPreferences()
{
    if (!CheckRegistryVersionNumber())
    {
        WriteRegistryVersionNumber();
    }

    // The settings as they are, for CancelPrefs.
    MCLogistics* logistics = GlobalLogPtr;
    const int32_t brightness = Application->GammaLevel;
    logistics->SavedPrefs0 = Application->PaletteCycle;
    logistics->SavedPrefs1 = Only45Pixel;
    logistics->SavedPrefs2 = brightness;
    logistics->SavedPrefs3 = SoundSystem->MusicLevel;
    logistics->SavedPrefs4 = SoundSystem->RadioLevel;
    logistics->SavedPrefs5 = SoundSystem->DigitalMasterVolume;
    logistics->SavedPrefs6 = GameDifficulty;
    SavedRendererPreference = GRendererPreference;
    MCGenericScreen* screen = logistics->PrefScreen;
    Element<MCLogSlider>(screen, 3)->SetCurrentValue(brightness);
    Element<MCLogSlider>(screen, 4)->SetCurrentValue(static_cast<int32_t>(GlobalLogPtr->SavedPrefs3));
    Element<MCLogSlider>(screen, 5)->SetCurrentValue(static_cast<int32_t>(GlobalLogPtr->SavedPrefs4));
    Element<MCLogSlider>(screen, 6)->SetCurrentValue(static_cast<int32_t>(GlobalLogPtr->SavedPrefs5));
    Element<MCLogToolButton>(screen, 8)->Toggled = GameDifficulty == 0 ? 1 : 0;
    Element<MCLogToolButton>(screen, 9)->Toggled = GameDifficulty == 1 ? 1 : 0;
    Element<MCLogToolButton>(screen, 10)->Toggled = GameDifficulty != 0 && GameDifficulty != 1 ? 1 : 0;
    GlobalLogPtr->MainScreen->ShowGuiWindow(0);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->PrefScreen;
    GlobalLogPtr->PrefScreen->ShowGuiWindow(1);
    GlobalLogPtr->LogisticsState = 9;
}

void CancelPrefs()
{
    MCLogistics* logistics = GlobalLogPtr;
    Application->PaletteCycle = logistics->SavedPrefs0;
    Only45Pixel = logistics->SavedPrefs1;
    Application->GammaCorrectCurrentPalette(logistics->SavedPrefs2);

    // Only the saved byte is compared (a volume is 0..127).
    if (static_cast<uint8_t>(logistics->SavedPrefs3) < 0x80)
    {
        SoundSystem->MusicLevel = static_cast<uint8_t>(logistics->SavedPrefs3);
    }

    if (static_cast<uint8_t>(logistics->SavedPrefs4) < 0x80)
    {
        SoundSystem->RadioLevel = static_cast<uint8_t>(logistics->SavedPrefs4);
    }

    if (static_cast<uint8_t>(logistics->SavedPrefs5) < 0x80)
    {
        SoundSystem->DigitalMasterVolume = static_cast<uint8_t>(logistics->SavedPrefs5);
    }

    GameDifficulty = logistics->SavedPrefs6;
    GRendererPreference = SavedRendererPreference;
    Cancel();
}

void WritePrefs()
{
    MCFitIniFile prefs;
    prefs.Create("prefs.cfg");
    prefs.WriteBlock("MechCommander");
    prefs.WriteIdBoolean("PaletteCycle", Application->PaletteCycle);
    prefs.WriteIdBoolean("DirectDraw", GFullScreen != 0);
    prefs.WriteIdBoolean("Use90Pixel", Use90PixelSprite);
    prefs.WriteIdBoolean("Force45Pixel", Only45Pixel);
    prefs.WriteIdBoolean("Force16Mb", Force16MB);
    prefs.WriteIdBoolean("Force32Mb", Force32MB);
    prefs.WriteIdLong("Difficulty", GameDifficulty);
    prefs.WriteIdLong("Brightness", Application->GammaLevel);
    prefs.WriteIdLong("MusicVolume", SoundSystem->MusicLevel);
    prefs.WriteIdLong("RadioVolume", SoundSystem->RadioLevel);
    prefs.WriteIdLong("SFXVolume", SoundSystem->DigitalMasterVolume);
    // Port: keep the port-only key. Original behaviour (OB-101): the hidden "Resolution" key is not written back.
    prefs.WriteIdBoolean("StretchToFit", GStretchToFit != 0);
    prefs.WriteIdBoolean("SoftwareCursor", GSoftwareCursor != 0);
    prefs.WriteIdString("Renderer", MCRendererKindName(static_cast<MCRendererKind>(GRendererPreference)));
    prefs.WriteIdBoolean("ShowFps", GShowFpsPreference != 0);
    Cancel();
}

void ShowMultiPlayer()
{
    GlobalLogPtr->MainScreen->ShowGuiWindow(0);
    GlobalLogPtr->MultiplayerScreen->ShowGuiWindow(1);
    Solo = 0;
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->MultiplayerScreen;
    GlobalLogPtr->LogisticsState = 10;
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

    NextGameState = 10;
    Mission->MissionState = 10;
    Mission->CurrentMovie = 0;
    GlobalLogPtr->MainScreen->ShowGuiWindow(0);
    SoundSystem->StopDigitalMusic();
}

void ReturnToGame()
{
    if (GlobalLogPtr->CurrentMission == -1)
    {
        return;
    }

    if (GlobalLogPtr->PreviousState == 2)
    {
        GlobalLogPtr->SetUpPurchaseScreen(0);
    }
    else if (GlobalLogPtr->PreviousState == 4)
    {
        GlobalLogPtr->SetUpRepairScreen(0);
    }
    else
    {
        GlobalLogPtr->SetUpBriefingScreen(0);
    }

    GlobalLogPtr->MainScreen->ShowGuiWindow(0);
}

void GameOverMan()
{
    MCInput::PostMessage(WM_DESTROY, 0, 0);
    SoundSystem->StopDigitalMusic();
}

void LoadGame()
{
    MCFileScrollPane* pane = GlobalLogPtr->LoadScreen->FilePane;

    if (!HasSelection(pane))
    {
        return;
    }

    char* fileName = pane->FileNames[pane->SelectedFile];
    SoundSystem->StopDigitalMusic();
    GlobalLogPtr->BriefingScreen->BriefingBox = nullptr;
    const bool campaign = LoadingSolo == 0;
    const char* extension = ".sav";

    if (!campaign)
    {
        LoadingSolo = 0;
        extension = ".sol";
    }

    Solo = campaign ? 0 : 1;
    LastLogisticsMissionState = 0;

    if (GlobalLogPtr->LoadCampaign(fileName, const_cast<char*>(extension), 0, 1) == 0)
    {
        // Original behaviour: the save screen is the one hidden, not the load screen shown.
        GlobalLogPtr->SaveScreen->ShowGuiWindow(0);
        GlobalLogPtr->SetUpBriefingScreen(0);
        SoundSystem->PlayDigitalMusic(0x16, true);
    }
}

void LoadMPGame()
{
    MCFileScrollPane* pane = GlobalLogPtr->LoadScreen->FilePane;

    if (HasSelection(pane))
    {
        GlobalLogPtr->SessionScreen->LoadMission(pane->FileNames[pane->SelectedFile]);
    }
}

void SaveWorkedCallback(int32_t)
{
    GlobalLogPtr->SaveScreen->ShowGuiWindow(0);

    if (GlobalLogPtr->PreviousState == 2)
    {
        GlobalLogPtr->SetUpPurchaseScreen(0);
    }
    else if (GlobalLogPtr->PreviousState != 4)
    {
        GlobalLogPtr->SetUpBriefingScreen(0);
    }
    else
    {
        GlobalLogPtr->SetUpRepairScreen(0);
    }

    SoundSystem->PlayDigitalMusic(0x16, true);
}

void SaveGameCallback()
{
    int32_t result = -1;
    MCFileScrollPane* pane = GlobalLogPtr->SaveScreen->FilePane;
    bool saved = false;

    if (HasSelection(pane))
    {
        char* fileName = pane->FileNames[pane->SelectedFile];

        if (fileName != nullptr)
        {
            SoundSystem->StopDigitalMusic();
            result = GlobalLogPtr->SaveCampaign(fileName);
        }

        saved = result == 0;
    }

    if (!saved)
    {
        pane->SetSelectedFile(-1);
    }

    pane->GetAllFiles(const_cast<char*>(".sav"), true);

    if (result == 0)
    {
        // "Game saved", closing by itself after three seconds.
        char text[256];
        CLoadString(ThisInstance, 0x76, text, 0xfe);
        MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
        dialog->SetText(text);
        dialog->SetTwoButton(0);
        dialog->Callback = SaveWorkedCallback;
        dialog->OkButton->SetUpPicture(const_cast<char*>("bh_okay.tga"));
        dialog->OkButton->SetDownPicture(const_cast<char*>("bg_okay.tga"));
        SetDisabled(dialog->OkButton, 0);
        dialog->Timeout = 3000;
        dialog->Activate();
        DestroyNameEntry(pane);
    }

    SoundSystem->PlayDigitalMusic(0x16, true);
}

void ClearForSaveGameCallback()
{
    MCFileScrollPane* pane = GlobalLogPtr->SaveScreen->FilePane;
    MCFullPathFileName fileName;
    fileName.Init(pane->StartDirectory, pane->FileNames[pane->SelectedFile], ".sav");

    // The original cleared the file's read-only attribute first.
    if (MCFileSystem::RemoveFile(static_cast<char*>(fileName)))
    {
        SaveGameCallback();
    }
}

void SaveGame()
{
    MCFileScrollPane* pane = GlobalLogPtr->SaveScreen->FilePane;

    if (pane == nullptr)
    {
        return;
    }

    const int32_t selected = pane->SelectedFile;

    if (selected < 0 || selected >= pane->NumFiles)
    {
        return;
    }

    // The name typed replaces the selected entry's.
    MCLogTextObject* entry = pane->NameEntry;

    if (entry != nullptr && entry->Parent == pane)
    {
        const char* typed = entry->Buffer;

        if (typed[0] == '\0')
        {
            typed = EmptyFile;
        }

        GlobalLogPtr->LogisticsBlocks->Free(pane->FileNames[selected]);
        pane->FileNames[selected] = HeapCopy(typed);
    }

    char** slot = &pane->FileNames[pane->SelectedFile];

    if (*slot == nullptr)
    {
        return;
    }

    // The empty entry gets the first free default name.
    if (std::strcmp(EmptyFile, *slot) == 0)
    {
        const char* directory = pane->StartDirectory;

        for (int32_t i = 0; i < 1000; i++)
        {
            char format[0x95];
            CLoadString(ThisInstance, 0x37c, format, 0x95);
            char candidate[0x68];
            std::snprintf(candidate, sizeof(candidate), format, i);
            MCFullPathFileName candidatePath;
            candidatePath.Init(directory, candidate, ".sav");

            if (FileExists(candidatePath) == 0)
            {
                GlobalLogPtr->LogisticsBlocks->Free(*slot);
                *slot = HeapCopy(candidate);
                break;
            }
        }
    }

    // Port fix (OB-084): the original tested the old, freed name here (the empty entry's text); the name now in
    // the slot is tested instead.
    MCFullPathFileName savePathName;
    savePathName.Init(pane->StartDirectory, *slot, ".sav");

    if (FileExists(savePathName) == 0)
    {
        SaveGameCallback();
        return;
    }

    // Overwrite?
    char text[256];
    CLoadString(ThisInstance, 0x75, text, 0xfe);
    MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
    dialog->SetText(text);
    dialog->SetTwoButton(1);
    dialog->Callback = nullptr;
    dialog->OkButton->SetUpPicture(const_cast<char*>("bh_okay.tga"));
    dialog->OkButton->SetDownPicture(const_cast<char*>("bg_okay.tga"));
    dialog->OkButton->Callback()->SetExec(ClearForSaveGameCallback);
    dialog->CancelButton->SetUpPicture(const_cast<char*>("bh_cancl.tga"));
    dialog->CancelButton->SetDownPicture(const_cast<char*>("bg_cancl.tga"));
    dialog->Activate();
}

void DeleteCallbackTrue()
{
    MCFileScrollPane* pane = ShownFilePane();

    if (!HasSelection(pane))
    {
        return;
    }

    MCFullPathFileName fileName;
    fileName.Init(pane->StartDirectory, pane->FileNames[pane->SelectedFile], LoadingSolo == 0 ? ".sav" : ".sol");
    // The original cleared the file's read-only attribute first.
    MCFileSystem::RemoveFile(static_cast<char*>(fileName));
    pane->SetSelectedFile(-1);

    if (LoadingSolo == 0)
    {
        GlobalLogPtr->LoadScreen->FilePane->GetAllFiles(const_cast<char*>(".sav"), true);
        GlobalLogPtr->SaveScreen->FilePane->GetAllFiles(const_cast<char*>(".sav"), true);
    }
    else
    {
        GlobalLogPtr->LoadScreen->FilePane->GetAllFiles(const_cast<char*>(".sol"), true);
    }
}

void DeleteCallbackFalse()
{
    MCFileScrollPane* pane = ShownFilePane();

    if (pane != nullptr && pane->NameEntry != nullptr)
    {
        DestroyNameEntry(pane);
    }
}

void DeleteGame()
{
    MCFileScrollPane* pane = ShownFilePane();

    if (!HasSelection(pane))
    {
        return;
    }

    AskQuestion(GlobalLogPtr->MessageDialog, 0x74, DeleteCallbackTrue, DeleteCallbackFalse, "bg_cancl.tga", false);
    DestroyNameEntry(pane);
}

void Cancel()
{
    if (GlobalLogPtr->CurrentScreen == GlobalLogPtr->MainScreen)
    {
        return;
    }

    WhackTimer = 1;
    GlobalLogPtr->SetUpMainScreen(1);
    LoadingSolo = 0;
}

void CancelToConnect()
{
    WhackTimer = 1;
    GlobalLogPtr->CurrentScreen->ShowGuiWindow(0);
    GlobalLogPtr->MultiplayerScreen->ShowGuiWindow(1);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->MultiplayerScreen;
    GlobalLogPtr->LogisticsState = 10;
    GlobalLogPtr->LanScreen->ShowBlock(0);
    GlobalLogPtr->ModemScreen->ShowBlock(0);
    auto* games = Element<MCGameList>(GlobalLogPtr->LanScreen, 2);

    if (games->NumSessions > -1)
    {
        games->SelectedSession = -1;
        // Port fix (OB-085): the original copied sessions[-1] (the four list fields before the table) as the
        // selected GUID; no session is selected, so it is cleared.
        games->SelectedGuid = {};
    }

    games->Clear();
    Element<MCLogScrollTextObject>(GlobalLogPtr->LanScreen, 3)->Clear();
    SetDisabled(Element<MCLogButton>(GlobalLogPtr->LanScreen, 6), 1);
    Application->RemoveTimer(GlobalLogPtr->SessionScreen->Team1RPText, 0);
    Application->RemoveTimer(GlobalLogPtr->SessionScreen->Team1RPText, 0);
    MPlayer->LeaveSession();
    delete GlobalLogPtr->PlayerLights;
    GlobalLogPtr->PlayerLights = nullptr;
}

void CancelToMPlayer()
{
    KillTheGame();
}

void CancelToLan()
{
    GlobalLogPtr->CurrentScreen->ShowGuiWindow(0);
    GlobalLogPtr->LanScreen->ShowGuiWindow(1);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->LanScreen;
    GlobalLogPtr->LogisticsState = 0xb;
    GlobalLogPtr->LanScreen->ShowBlock(0);
    auto* games = Element<MCGameList>(GlobalLogPtr->LanScreen, 2);

    if (games->NumSessions > -1)
    {
        games->SelectedSession = -1;
        // Port fix (OB-085): see CancelToConnect.
        games->SelectedGuid = {};
    }
}

void CancelToSession()
{
    MCSplashScreen* loadScreen = GlobalLogPtr->LoadScreen;
    loadScreen->CancelButton->Callback()->SetExec(Cancel);
    loadScreen->LoadSaveButton->Callback()->SetExec(LoadGame);
    loadScreen->FilePane->SetMultiplayer(0);
    GlobalLogPtr->CurrentScreen->ShowGuiWindow(0);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->SessionScreen;
    GlobalLogPtr->LogisticsState = 8;
    GlobalLogPtr->ShowLogScreen(1, 1);
}

void ShowModemScreen()
{
    if (MPlayer != nullptr)
    {
        MCGenericScreen* screen = GlobalLogPtr->ModemScreen;
        auto* modems = Element<MCLogScrollTextObject>(screen, 10);
        char name[0x40];
        const bool known = UserName(name, 0x3f);
        Element<MCLogTextObject>(screen, 4)->SetStringBuffer(known ? name : const_cast<char*>("Player"));
        MPlayer->SessionManager->FindModems();
        // The list is refilled with the modems found, keeping its selection.
        const int32_t selected = modems->HighlightLine[0];
        modems->Clear();
        modems->HighlightLine[0] = selected;

        for (int32_t i = 0;; i++)
        {
            char* modem = MPlayer->SessionManager->GetModemName(i);

            if (modem == nullptr)
            {
                break;
            }

            Element<MCLogScrollTextObject>(GlobalLogPtr->ModemScreen, 10)->Print(modem, 0x1f);
        }
    }

    GlobalLogPtr->MultiplayerScreen->ShowGuiWindow(0);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->ModemScreen;
    GlobalLogPtr->ModemScreen->ShowGuiWindow(1);
    GlobalLogPtr->LogisticsState = 0xc;
    GlobalLogPtr->ModemScreen->ShowBlock(0);
}

void ShowSerialScreen()
{
    char name[0x40];
    const bool known = UserName(name, 0x3f);
    Element<MCLogTextObject>(GlobalLogPtr->SerialScreen, 4)
        ->SetStringBuffer(known ? name : const_cast<char*>("Player"));
    GlobalLogPtr->MultiplayerScreen->ShowGuiWindow(0);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->SerialScreen;
    GlobalLogPtr->SerialScreen->ShowGuiWindow(1);
    GlobalLogPtr->LogisticsState = 0xd;
    Application->SetText(GlobalLogPtr->SerialScreen->Elements[4]);
}

void DoTheIpxThang()
{
    if (MPlayer != nullptr && MPlayer->SessionManager != nullptr)
    {
        MPlayer->SessionManager->ConnectIpx();
    }

    GlobalLogPtr->MultiplayerScreen->ShowGuiWindow(0);
    GlobalLogPtr->LanScreen->ShowGuiWindow(1);
    GlobalLogPtr->LanScreen->ShowBlock(0);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->LanScreen;
    GlobalLogPtr->LogisticsState = 0xb;
    Application->SetText(GlobalLogPtr->LanScreen->Elements[4]);
}

void DoTheTcpThang()
{
    if (MPlayer != nullptr && MPlayer->SessionManager != nullptr)
    {
        MPlayer->SessionManager->ConnectTcp(const_cast<char*>(""));
    }

    GlobalLogPtr->MultiplayerScreen->ShowGuiWindow(0);
    GlobalLogPtr->LanScreen->ShowGuiWindow(1);
    GlobalLogPtr->LanScreen->ShowBlock(0);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->LanScreen;
    GlobalLogPtr->LogisticsState = 0xb;
    Application->SetText(GlobalLogPtr->LanScreen->Elements[4]);
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

void CallDoTheIpxThang(int32_t)
{
    DoTheIpxThang();
}

void CallDoTheTcpThang(int32_t)
{
    DoTheTcpThang();
}

void ShowLanScreen()
{
    MCGenericScreen* screen = GlobalLogPtr->LanScreen;
    char name[0x40];
    const char* gameName;
    char game[0x200];

    if (!UserName(name, 0x3f))
    {
        Element<MCLogTextObject>(screen, 4)->SetStringBuffer(const_cast<char*>("Player"));
        gameName = "Game";
    }
    else
    {
        Element<MCLogTextObject>(screen, 4)->SetStringBuffer(name);
        char format[256];
        CLoadString(ThisInstance, 0x377, format, 0xfe);
        std::snprintf(game, sizeof(game), format, name);
        Element<MCLogTextObject>(GlobalLogPtr->LanScreen, 10)->InitBuffer(0x18, 0);
        gameName = game;
    }

    Element<MCLogTextObject>(GlobalLogPtr->LanScreen, 10)->SetStringBuffer(const_cast<char*>(gameName));

    // Both protocols: ask which; else say which one is used.
    MCSessionManager* manager = MPlayer->SessionManager;

    if (manager->IsIpxAvailable() != 0 && manager->IsTcpAvailable() != 0)
    {
        char text[256];
        CLoadString(ThisInstance, 0xa6, text, 0xfe);
        MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
        dialog->SetText(text);
        dialog->SetTwoButton(1);
        dialog->Callback = TcpipxDialogCallback;
        dialog->OkButton->SetUpPicture(const_cast<char*>("bh_ipx.tga"));
        dialog->OkButton->SetDownPicture(const_cast<char*>("bg_ipx.tga"));
        SetDisabled(dialog->OkButton, 0);
        dialog->OkButton->Result = 1;
        dialog->CancelButton->SetUpPicture(const_cast<char*>("bh_tcp.tga"));
        dialog->CancelButton->SetDownPicture(const_cast<char*>("bg_tcp.tga"));
        SetDisabled(dialog->CancelButton, 0);
        dialog->CancelButton->Result = 2;
        dialog->Activate();
        return;
    }

    if (manager->IsIpxAvailable() != 0)
    {
        ShowMessage(0x36f, CallDoTheIpxThang);
        return;
    }

    if (manager->IsTcpAvailable() != 0)
    {
        ShowMessage(0x36e, CallDoTheTcpThang);
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
    AskQuestion(GlobalLogPtr->QuestionDialog, 0x4e9, DoExitToZone1, nullptr, "bh_cancl.tga", true);
}

void DoExitToMplayer()
{
    // Port: the original started mplaynow.exe (the Mplayer.com lobby, long gone) and quit; the port takes the path
    // of a failed start: the error message.
    ShowMessage(0x353, nullptr);
}

void ShowInternet()
{
    AskQuestion(GlobalLogPtr->MessageDialog, 0x352, DoExitToMplayer, DoExitToZone, "bh_cancl.tga", true);
}

void HostGame()
{
    MCGenericScreen* screen = GlobalLogPtr->LanScreen;
    char* playerName = ElementText(screen, 4);
    SaveUserName(playerName);
    GlobalLogPtr->LanScreen->ShowBlock(1);
    Application->SetText(GlobalLogPtr->LanScreen->Elements[11]);
    char format[256];
    CLoadString(ThisInstance, 0x377, format, 0xfe);
    char game[0x200];
    std::snprintf(game, sizeof(game), format, playerName);
    Element<MCLogTextObject>(GlobalLogPtr->LanScreen, 10)->SetStringBuffer(game);
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
    _GUID* game = Element<MCGameList>(GlobalLogPtr->LanScreen, 2)->GetSelectedGame();

    if (game != nullptr)
    {
        MCFidpSession* session = manager->FindMatchingSession(game);

        if (session != nullptr)
        {
            char* playerName = ElementText(GlobalLogPtr->LanScreen, 4);
            SaveUserName(playerName);

            if (session->SessionDesc.dwCurrentPlayers < session->SessionDesc.dwMaxPlayers)
            {
                const int32_t result = manager->JoinSession(&session->SessionDesc.guidInstance, playerName);
                CountLanPlayers();

                if (result == 0)
                {
                    EnterReadyRoom(GlobalLogPtr->LanScreen, 1);
                    ReadyRoomTicks = 0;
                    ResetReadyRoom();
                    return;
                }
            }
        }
    }

    // The game can't be joined.
    ShowMessage(0xa7, nullptr);
}

void CreateSession()
{
    if (MPlayer != nullptr && MPlayer->SessionManager != nullptr)
    {
        char* sessionName = ElementText(GlobalLogPtr->LanScreen, 10);
        int32_t maxPlayers = std::atoi(ElementText(GlobalLogPtr->LanScreen, 11));

        if (maxPlayers < 2)
        {
            maxPlayers = 2;
        }
        else if (maxPlayers > 6)
        {
            maxPlayers = 6;
        }

        MPlayer->CreateSession(sessionName, ElementText(GlobalLogPtr->LanScreen, 4), maxPlayers);
    }

    GlobalLogPtr->LanScreen->ShowGuiWindow(0);
    GlobalLogPtr->ConnectScreen->ShowGuiWindow(1);
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

    char* portText = ElementText(GlobalLogPtr->SerialScreen, 5);

    if (portText == nullptr)
    {
        return;
    }

    const auto port = static_cast<uint32_t>(std::atoi(portText));

    if (static_cast<int32_t>(port) < 1 || static_cast<int32_t>(port) > 4)
    {
        return;
    }

    MPlayer->SessionManager->ConnectComPort(port, 0xe100, 0, 0, 4);
    char* playerName = ElementText(GlobalLogPtr->SerialScreen, 4);
    SaveUserName(playerName);

    if (MPlayer->CreateSession(const_cast<char*>("SerialGame"), playerName, 2) == 0)
    {
        EnterReadyRoom(GlobalLogPtr->SerialScreen, -1);
    }
}

void SerialJoinButtonPressed()
{
    if (MPlayer == nullptr)
    {
        return;
    }

    SaveUserName(ElementText(GlobalLogPtr->SerialScreen, 4));
    WhackTimer = 1;
    char* portText = ElementText(GlobalLogPtr->SerialScreen, 5);

    if (portText == nullptr)
    {
        return;
    }

    const auto port = static_cast<uint32_t>(std::atoi(portText));

    if (static_cast<int32_t>(port) > 0 && static_cast<int32_t>(port) < 5 &&
        MPlayer->SessionManager->ConnectComPort(port, 0xe100, 0, 0, 4) == 0)
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

    if (MPlayer->JoinSession(const_cast<char*>("SerialGame"), ElementText(GlobalLogPtr->SerialScreen, 4)) != 0)
    {
        // No game yet: try again in a second, with a way out.
        Application->AddTimer(GlobalLogPtr->SerialScreen, 0, 1000, 0, 0, 0);
        WhackTimer = 0;
        char text[256];
        CLoadString(ThisInstance, 0xb1, text, 0xfe);
        MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
        dialog->SetText(text);
        dialog->SetTwoButton(0);
        dialog->Callback = nullptr;
        dialog->OkButton->SetUpPicture(const_cast<char*>("bh_cancl.tga"));
        dialog->OkButton->SetDownPicture(const_cast<char*>("bg_cancl.tga"));
        dialog->OkButton->Callback()->SetExec(CancelToConnect);
        dialog->Activate();
        return;
    }

    CountLanPlayers();
    EnterReadyRoom(GlobalLogPtr->SerialScreen, 1);
    GlobalLogPtr->MessageDialog->Deactivate(0);
}

void JoinModemSession()
{
    if (MPlayer->JoinSession(const_cast<char*>("MC Modem Game"), ElementText(GlobalLogPtr->ModemScreen, 4)) != 0)
    {
        Application->AddTimer(GlobalLogPtr->ModemScreen, 1, 1000, 0, 0, 0);
        WhackTimer = 0;
        return;
    }

    CountLanPlayers();
    EnterReadyRoom(GlobalLogPtr->ModemScreen, 1);
    GlobalLogPtr->MessageDialog->Deactivate(0);
    WhackTimer = 1;
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

    Application->AddTimer(GlobalLogPtr->ModemScreen, 0, 1000, 0, 0, 0);
    WhackTimer = 0;
    // 2 while the line is still dialling (DirectPlay's DPERR_CONNECTING, 0x8877015e), else 1.
    return (result == static_cast<int32_t>(0x8877015e)) ? 2 : 1;
}

void AllGoneCallback(int32_t)
{
    char text[256];
    CLoadString(ThisInstance, 0xbb, text, 0xfe);
    MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
    dialog->SetText(text);
    dialog->SetTwoButton(0);
    dialog->Callback = nullptr;
    dialog->OkButton->SetUpPicture(const_cast<char*>("bh_okay.tga"));
    dialog->OkButton->SetDownPicture(const_cast<char*>("bg_okay.tga"));
    dialog->OkButton->Callback()->SetExec(nullptr);
    dialog->Activate();
}

void GOCallback()
{
    if (MPlayer == nullptr)
    {
        return;
    }

    GlobalLogPtr->CurrentScreen->ShowGuiWindow(0);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->SessionScreen;
    GlobalLogPtr->ShowLogScreen(1, 1);
    GlobalLogPtr->LogisticsState = 8;
    GlobalLogPtr->SessionScreen->Activate(0);

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

    int32_t maxPlayers;
    const int32_t connection = manager->CurrentConnection;

    if (connection == 2 || connection == 1)
    {
        maxPlayers = std::atoi(ElementText(GlobalLogPtr->LanScreen, 11));

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

    const uint32_t players = session->SessionDesc.dwCurrentPlayers;
    Assert(static_cast<int32_t>(players) <= maxPlayers, players, " How'd we get too many players? ", nullptr);
    GOCallback();
}

void Leave()
{
}

void WaitForCall()
{
    if (MPlayer != nullptr && MPlayer->SessionManager != nullptr)
    {
        auto* modems = Element<MCLogScrollTextObject>(GlobalLogPtr->ModemScreen, 10);
        char modem[256];

        if (modems->GetTextLine(modems->HighlightLine[0] + 1, modem, 0xff) != 0)
        {
            MPlayer->SessionManager->ConnectModem(const_cast<char*>(""), modem);
            char* playerName = ElementText(GlobalLogPtr->ModemScreen, 4);
            SaveUserName(playerName);
            MPlayer->CreateSession(const_cast<char*>("MC Modem Game"), playerName, 2);
        }
    }

    Element<MCLogTextObject>(GlobalLogPtr->LanScreen, 11)->InitBuffer(2, 1);
    GlobalLogPtr->ModemScreen->ShowGuiWindow(0);
    SetDisabled(Element<MCLogButton>(GlobalLogPtr->ConnectScreen, 2), 0);
    GlobalLogPtr->ConnectScreen->ShowGuiWindow(1);
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->ConnectScreen;
    GlobalLogPtr->LogisticsState = 0xe;
    Application->SetText(GlobalLogPtr->ModemScreen->Elements[4]);
    Application->AddTimer(GlobalLogPtr->ModemScreen->Elements[4], 0, MCPort::CaretBlinkTime(), 0, 0, 0);
}

void GetNumber()
{
    Application->SetText(GlobalLogPtr->ModemScreen->Elements[5]);
    GlobalLogPtr->ModemScreen->ShowBlock(1);
}

void CancelDial()
{
    WhackTimer = 1;

    if (MPlayer != nullptr)
    {
        MPlayer->SessionManager->CancelDialing();
    }
}

void Dial()
{
    MCGenericScreen* screen = GlobalLogPtr->ModemScreen;
    auto* modems = Element<MCLogScrollTextObject>(screen, 10);

    if (MPlayer == nullptr || MPlayer->SessionManager == nullptr)
    {
        return;
    }

    char modem[512];

    if (modems->GetTextLine(modems->HighlightLine[0] + 1, modem, 0xff) == 0)
    {
        return;
    }

    MPlayer->SessionManager->ConnectModem(ElementText(screen, 5), modem);
    const int32_t result = DialModemSession();
    Assert(result != 1, 0, "Not currently connected to a modem", nullptr);

    switch (result)
    {
        case 0:
        {
            EnterReadyRoom(GlobalLogPtr->ModemScreen, 1);
            return;
        }
        case 1:
        case 3:
        {
            ShowMessage(0xb3, nullptr);
            return;
        }
        case 2:
        {
            // Still dialling: check again in a second; the button cancels.
            Application->AddTimer(GlobalLogPtr->ModemScreen, 0, 1000, 0, 0, 0);
            WhackTimer = 0;
            char text[256];
            CLoadString(ThisInstance, 0xb2, text, 0xfe);
            MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
            dialog->SetText(text);
            dialog->SetTwoButton(0);
            dialog->Callback = nullptr;
            dialog->OkButton->SetUpPicture(const_cast<char*>("bh_cancl.tga"));
            dialog->OkButton->SetDownPicture(const_cast<char*>("bg_cancl.tga"));
            SetDisabled(dialog->OkButton, 0);
            dialog->OkButton->Callback()->SetExec(CancelDial);
            dialog->Activate();
            return;
        }

        default:
            return;
    }
}

void ImageHandleEvent(MCGuiObject* object, MCGuiEvent* event)
{
    if (object->Parent != nullptr)
    {
        object->Parent->HandleEvent(event);
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

    if (list->GetTextLine(line + 1, nullptr, 0) != 0)
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
        if (object->Parent == nullptr || MPlayer == nullptr)
        {
            return;
        }

        if (MPlayer->SessionManager != nullptr)
        {
            auto* games = Element<MCGameList>(static_cast<MCGenericScreen*>(object->Parent), 2);
            list->Clear();

            if (games != nullptr && games->SelectedSession > -1)
            {
                MCFidpSession* session = MPlayer->SessionManager->FindMatchingSession(games->GetSelectedGame());
                MCFLinkedList<MCFidpPlayer>* players =
                    session != nullptr ? MPlayer->SessionManager->GetPlayers(session) : nullptr;

                if (players != nullptr)
                {
                    const int32_t count = players->Count;
                    players->Current = players->HeadLink;

                    for (int32_t i = count; i > 0; i--)
                    {
                        MCFidpPlayer* player = players->ReadAndNext();
                        list->Print(player->Name, 0x1f);
                    }
                }
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

    if (session != nullptr)
    {
        if (manager->IsHost == 0 && LaunchedFromLobby == 0)
        {
            manager->SendPing();
        }

        list->Clear();
        MCFLinkedList<MCFidpPlayer>* players = MPlayer->SessionManager->GetPlayers(session);

        if (players != nullptr)
        {
            // Each player, with the ping outside a lobby launch.
            for (MCFLink<MCFidpPlayer>* link = players->HeadLink; link != nullptr && link->Data != nullptr;
                 link = link->Next)
            {
                MCFidpPlayer* player = link->Data;

                if (LaunchedFromLobby == 0)
                {
                    char line[256];
                    std::snprintf(line, sizeof(line), "%s - %04d ms", player->Name, player->LastLatency);
                    list->Print(line, 0x1f);
                }
                else
                {
                    list->Print(player->Name, 0x1f);
                }
            }

            // The host can go once someone else is in.
            if (MPlayer->SessionManager->IsHost != 0)
            {
                SetDisabled(Element<MCLogButton>(GlobalLogPtr->ConnectScreen, 2), players->Count < 2 ? 1 : 0);
            }
        }
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

    auto* joinButton = Element<MCLogButton>(screen, 6);

    if (event->Data == 3)
    {
        // A game was picked: its players are listed, and it can be joined unless full.
        MCFidpSession* session =
            MPlayer->SessionManager->FindMatchingSession(Element<MCGameList>(screen, 2)->GetSelectedGame());
        screen->Elements[3]->HandleEvent(event);
        SetDisabled(joinButton, session->SessionDesc.dwMaxPlayers <= session->SessionDesc.dwCurrentPlayers ? 1 : 0);
        return;
    }

    if (event->Data == 4)
    {
        screen->Elements[3]->HandleEvent(event);
        SetDisabled(joinButton, 1);
    }
}

void LoadSaveScreenHandleEvent(MCGuiObject* object, MCGuiEvent* event)
{
    if (event->Type != 0x1e)
    {
        return;
    }

    auto* screen = static_cast<MCGenericScreen*>(object);
    const int32_t message = event->Data;

    if (message == 1)
    {
        // A file was picked: it can be loaded (or saved over); deleted unless it is the empty entry.
        SetDisabled(screen->LoadSaveButton, 0);
        MCLogButton* deleteButton = screen->DeleteButton;
        SetDisabled(deleteButton, 1);
        MCFileScrollPane* pane = screen->FilePane;

        if (HasSelection(pane) && std::strcmp(pane->FileNames[pane->SelectedFile], EmptyFile) != 0)
        {
            SetDisabled(deleteButton, 0);
        }
    }
    else if (message == 2)
    {
        SetDisabled(screen->LoadSaveButton, 1);
        SetDisabled(screen->DeleteButton, 1);
    }
    else if (message == 5 && object == GlobalLogPtr->SaveScreen)
    {
        // Enter in the name entry: save.
        SoundSystem->PlayDigitalSample(screen->LoadSaveButton->PressSound, 1, nullptr, 0, 0);
        SaveGame();
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
    int32_t valid = 0;
    char* text = entry->Buffer;

    if (text != nullptr && text[0] != '\0' && event->LParam != 999)
    {
        if (std::atoi(text) < 5 && std::atoi(text) != 0)
        {
            valid = 1;
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
    notify.LParam = valid;
    object->Parent->HandleEvent(&notify);
}

void SerialScreenHandleEvent(MCGuiObject* object, MCGuiEvent* event)
{
    if (event->Type == 0x13)
    {
        Application->RemoveTimer(object, 0);

        if (WhackTimer == 0)
        {
            JoinSerialSession();
        }

        return;
    }

    if (event->Type == 0x1e && event->Data == 6)
    {
        // The host and join buttons need a good port number.
        const int disabled = event->LParam == 0 ? 1 : 0;
        auto* screen = static_cast<MCGenericScreen*>(object);
        SetDisabled(Element<MCLogButton>(screen, 2), disabled);
        SetDisabled(Element<MCLogButton>(screen, 3), disabled);
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
        Application->RemoveTimer(object, 0);

        if (WhackTimer == 0)
        {
            DialModemSession();
        }
    }
    else if (event->Data == 1)
    {
        Application->RemoveTimer(object, 1);

        if (WhackTimer == 0)
        {
            JoinModemSession();
        }
    }
}

void PrefScreenHandleEvent(MCGuiObject*, MCGuiEvent*)
{
}

void SlideScreenBrightness(MCGuiObject* object, MCGuiEvent*)
{
    Application->GammaCorrectCurrentPalette(static_cast<MCLogSlider*>(object)->CurrentValue);
}

void SlideMusicVolume(MCGuiObject* object, MCGuiEvent* event)
{
    if (event->Type != 4)
    {
        return;
    }

    const auto volume = static_cast<uint8_t>(static_cast<MCLogSlider*>(object)->CurrentValue);

    if (volume < 0x80)
    {
        SoundSystem->MusicLevel = volume;
    }

    SoundSystem->StopDigitalMusic();
    SoundSystem->PlayDigitalMusic(0x16, true);
}

void SlideRadioVolume(MCGuiObject* object, MCGuiEvent* event)
{
    if (event->Type != 4)
    {
        return;
    }

    const auto volume = static_cast<uint8_t>(static_cast<MCLogSlider*>(object)->CurrentValue);

    if (volume < 0x80)
    {
        SoundSystem->RadioLevel = volume;
    }

    SoundSystem->PlayPilotSpeech(const_cast<char*>("pilotd"), 0x15);
}

void SlideFXVolume(MCGuiObject* object, MCGuiEvent* event)
{
    if (event->Type != 4)
    {
        return;
    }

    const auto volume = static_cast<uint8_t>(static_cast<MCLogSlider*>(object)->CurrentValue);

    if (volume < 0x80)
    {
        SoundSystem->DigitalMasterVolume = volume;
    }

    SoundSystem->PlayDigitalSample(0xf, 1, nullptr, 0, 0);
}

namespace
{
    /// <summary>Lights difficulty toggle <paramref name="difficulty"/> (easy, regular, hard) and sets the difficulty.</summary>
    void SetDifficulty(int32_t difficulty)
    {
        MCGenericScreen* screen = GlobalLogPtr->PrefScreen;

        for (int32_t i = 0; i < 3; i++)
        {
            auto* toggle = Element<MCLogToolButton>(screen, 8 + i);
            toggle->Toggled = i == difficulty ? 1 : 0;
        }

        GameDifficulty = difficulty;
    }
}

void EasyToggle()
{
    SetDifficulty(0);
}

void RegularToggle()
{
    SetDifficulty(1);
}

void HardToggle()
{
    SetDifficulty(2);
}

void DoExit()
{
    if (MPlayer != nullptr)
    {
        if (LaunchedFromLobby != 0)
        {
            KillTheGame();
            return;
        }

        GlobalLogPtr->DestroyMultiplayer();
        MPlayer->LeaveSession();
    }

    GlobalLogPtr->SetUpMainScreen(0);
}

void CheckExit()
{
    char text[256];
    CLoadString(ThisInstance, 0xaf, text, 0xfe);
    MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
    dialog->SetText(text);
    dialog->SetTwoButton(1);
    // The dialog's callback and the cancel button's action are left as they were.
    dialog->OkButton->SetUpPicture(const_cast<char*>("bh_okay.tga"));
    dialog->OkButton->SetDownPicture(const_cast<char*>("bg_okay.tga"));
    SetDisabled(dialog->OkButton, 0);
    dialog->OkButton->Callback()->SetExec(DoExit);
    dialog->CancelButton->SetUpPicture(const_cast<char*>("bh_cancl.tga"));
    dialog->CancelButton->SetDownPicture(const_cast<char*>("bh_cancl.tga"));
    SetDisabled(dialog->CancelButton, 0);
    dialog->Activate();
}

void SaveUserName(char* name)
{
    if (name != nullptr)
    {
        MCRegistry::Write("Software\\FASA Interactive\\MechCommander Expansion", "Player Name", name);
    }
}
