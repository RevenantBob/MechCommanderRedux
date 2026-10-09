#include "stdafx.h"
#include "logistics/MCPreferencesMenu.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiSystem.h"
#include "lib/MCFitIniFile.h"
#include "logistics/MCGenericScreen.h"
#include "logistics/MCLogComboBox.h"
#include "logistics/MCLogMenus.h"
#include "logistics/MCLogSlider.h"
#include "logistics/MCLogToolButton.h"
#include "logistics/MCMainMenu.h"
#include "logistics/MCRegistrySettings.h"
#include "main/logistics.h"
#include "platform/MCPresenter.h"
#include "platform/MCRenderer.h"
#include "sound/MCSoundSystem.h"
#include "sprite/MCSpriteManager.h"
#include "vfx/MCVfxFunctions.h"
#include "logistics/MCSplashScreen.h"

int32_t GameDifficulty = 1;
bool Only45Pixel = false;
bool Force32MB = false;
bool Force16MB = false;

namespace
{
    // Port: the preferences screen's DIFFICULTY and RENDERER choices are drop-downs, each in a box drawn as the panel
    // art's DIFFICULTY box (prefs_00.tga): an outline of colour 0x13 on the panel's 0x10, the title in 0xe3. The
    // DIFFICULTY box is drawn over the art's, which holds the original's three checks and labels; the checks
    // (elements 8..10) are hidden.

    /// <summary>The difficulty checks: elements 8 (easy), 9 (regular) and 10 (hard).</summary>
    constexpr int32_t FirstDifficultyToggle = 8;

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
        explicit MCPrefsBox(std::string_view title) : _Title(title) {}

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
            VfxStringDraw(Lport()->Frame(), 32, 9, WhiteFont->FontData.get(), _Title.c_str(),
                          MCLogComboBox::LabelColors());
        }

        bool DrawsLive() override { return true; }

        /// <summary>
        /// The box is part of the panel, as the art's boxes are: the mouse goes through it. (As an object it took
        /// clicks, and a click brought it in front of the controls on it, which then took no more.)
        /// </summary>
        MCGuiObject* FindObject(int32_t, int32_t) override { return nullptr; }

    private:
        /// <summary>The title.</summary>
        std::string _Title;
    };

    /// <summary>The choice when the screen opened (for CancelPrefs).</summary>
    int32_t SavedRendererPreference = 0;

    /// <summary>The RENDERER drop-down chose <paramref name="renderer"/>: another one than this run's needs a restart.</summary>
    void RendererChanged(int32_t renderer)
    {
        if (renderer != GRenderer)
        {
            ShowMenuMessage("The new renderer takes effect when you restart MechCommander.");
        }
    }

    /// <summary>Adds a box titled <paramref name="title"/> at <paramref name="place"/> to <paramref name="screen"/>.</summary>
    void AddPrefsBox(MCGenericScreen* screen, const MCPrefsBoxPlace& place, std::string_view title)
    {
        auto box = MCMakeGui<MCPrefsBox>(title);
        box->Init(place.Left, place.Top, place.Width, place.Height);
        box->SetTransparent(true);
        box->ShowGuiWindow(true);
        screen->AdoptChild(std::move(box));
    }

    /// <summary>Adds a drop-down editing <paramref name="setting"/> in the box at <paramref name="place"/>.</summary>
    void AddDropDown(MCGenericScreen* screen, const MCPrefsBoxPlace& place, int32_t* setting,
                     std::vector<MCLogComboBox::Item> items, void (*changed)(int32_t value))
    {
        auto dropDown = MCMakeGui<MCLogComboBox>();
        dropDown->Init(place.Left + DropDownLeft, place.Top + DropDownTop, DropDownWidth, setting, std::move(items),
                       changed);
        dropDown->ShowGuiWindow(true);
        screen->AdoptChild(std::move(dropDown));
    }

    /// <summary>Difficulty toggle <paramref name="index"/> (0 easy, 1 regular, 2 hard) of the preferences screen.</summary>
    MCLogToolButton* DifficultyToggle(int32_t index)
    {
        return GlobalLogPtr->PrefScreen->Element<MCLogToolButton>(FirstDifficultyToggle + index);
    }

    /// <summary>Lights difficulty toggle <paramref name="difficulty"/> (easy, regular, hard) and sets the difficulty.</summary>
    void SetDifficulty(int32_t difficulty)
    {
        for (int32_t i = 0; i < 3; i++)
        {
            DifficultyToggle(i)->Toggled = i == difficulty;
        }

        GameDifficulty = difficulty;
    }

    /// <summary>A volume slider's value as a sound level: its low byte, kept when under 0x80.</summary>
    std::optional<uint8_t> SliderVolume(MCGuiObject* slider)
    {
        const auto volume = static_cast<uint8_t>(static_cast<MCLogSlider*>(slider)->CurrentValue);
        return volume < 0x80 ? std::optional<uint8_t>(volume) : std::nullopt;
    }

    /// <summary>A saved volume back: only the saved byte is compared (a volume is 0..127).</summary>
    void RestoreVolume(uint8_t& level, int32_t saved)
    {
        if (static_cast<uint8_t>(saved) < 0x80)
        {
            level = static_cast<uint8_t>(saved);
        }
    }
}

void AddPreferenceDropDowns(MCGenericScreen* screen)
{
    // The original's DIFFICULTY checks (easy, regular, hard) give way to the drop-down.
    for (int32_t i = 0; i < 3; i++)
    {
        screen->Element<MCLogToolButton>(FirstDifficultyToggle + i)->ShowGuiWindow(false);
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
    EnsureRegistryVersion();

    // The settings as they are, for CancelPrefs.
    MCLogistics* logistics = GlobalLogPtr;
    const int32_t brightness = GuiSystem()->GammaLevel;
    logistics->SavedPrefs0 = GuiSystem()->PaletteCycle;
    logistics->SavedPrefs1 = Only45Pixel ? 1 : 0;
    logistics->SavedPrefs2 = brightness;
    logistics->SavedPrefs3 = SoundSystem()->MusicLevel;
    logistics->SavedPrefs4 = SoundSystem()->RadioLevel;
    logistics->SavedPrefs5 = SoundSystem()->DigitalMasterVolume;
    logistics->SavedPrefs6 = GameDifficulty;
    SavedRendererPreference = GRendererPreference;
    MCGenericScreen* screen = logistics->PrefScreen;
    screen->Element<MCLogSlider>(3)->SetCurrentValue(brightness);
    screen->Element<MCLogSlider>(4)->SetCurrentValue(static_cast<int32_t>(logistics->SavedPrefs3));
    screen->Element<MCLogSlider>(5)->SetCurrentValue(static_cast<int32_t>(logistics->SavedPrefs4));
    screen->Element<MCLogSlider>(6)->SetCurrentValue(static_cast<int32_t>(logistics->SavedPrefs5));
    // A difficulty past regular lights hard.
    DifficultyToggle(0)->Toggled = GameDifficulty == 0;
    DifficultyToggle(1)->Toggled = GameDifficulty == 1;
    DifficultyToggle(2)->Toggled = GameDifficulty != 0 && GameDifficulty != 1;
    logistics->MainScreen->ShowGuiWindow(false);
    logistics->CurrentScreen = logistics->PrefScreen;
    logistics->PrefScreen->ShowGuiWindow(true);
    logistics->LogisticsState = 9;
}

void CancelPrefs()
{
    MCLogistics* logistics = GlobalLogPtr;
    GuiSystem()->PaletteCycle = logistics->SavedPrefs0;
    Only45Pixel = logistics->SavedPrefs1 != 0;
    GuiSystem()->GammaCorrectCurrentPalette(logistics->SavedPrefs2);
    RestoreVolume(SoundSystem()->MusicLevel, logistics->SavedPrefs3);
    RestoreVolume(SoundSystem()->RadioLevel, logistics->SavedPrefs4);
    RestoreVolume(SoundSystem()->DigitalMasterVolume, logistics->SavedPrefs5);
    GameDifficulty = logistics->SavedPrefs6;
    GRendererPreference = SavedRendererPreference;
    Cancel();
}

void WritePrefs()
{
    MCFitIniFile prefs;
    prefs.Create("prefs.cfg");
    prefs.WriteBlock("MechCommander");
    prefs.WriteIdBoolean("PaletteCycle", GuiSystem()->PaletteCycle);
    prefs.WriteIdBoolean("DirectDraw", GFullScreen != 0);
    prefs.WriteIdBoolean("Use90Pixel", Use90PixelSprite);
    prefs.WriteIdBoolean("Force45Pixel", Only45Pixel);
    prefs.WriteIdBoolean("Force16Mb", Force16MB);
    prefs.WriteIdBoolean("Force32Mb", Force32MB);
    prefs.WriteIdLong("Difficulty", GameDifficulty);
    prefs.WriteIdLong("Brightness", GuiSystem()->GammaLevel);
    prefs.WriteIdLong("MusicVolume", SoundSystem()->MusicLevel);
    prefs.WriteIdLong("RadioVolume", SoundSystem()->RadioLevel);
    prefs.WriteIdLong("SFXVolume", SoundSystem()->DigitalMasterVolume);
    // Port: keep the port-only key. Original behaviour (OB-101): the hidden "Resolution" key is not written back.
    prefs.WriteIdBoolean("StretchToFit", GStretchToFit != 0);
    prefs.WriteIdBoolean("SoftwareCursor", GSoftwareCursor != 0);
    prefs.WriteIdString("Renderer", MCRendererKindName(static_cast<MCRendererKind>(GRendererPreference)));
    prefs.WriteIdBoolean("ShowFps", GShowFpsPreference != 0);
    Cancel();
}

void PrefScreenHandleEvent(MCGuiObject*, MCGuiEvent*)
{
}

void SlideScreenBrightness(MCGuiObject* object, MCGuiEvent*)
{
    GuiSystem()->GammaCorrectCurrentPalette(static_cast<MCLogSlider*>(object)->CurrentValue);
}

void SlideMusicVolume(MCGuiObject* object, MCGuiEvent* event)
{
    if (event->Type != 4)
    {
        return;
    }

    if (const std::optional<uint8_t> volume = SliderVolume(object))
    {
        SoundSystem()->MusicLevel = *volume;
    }

    SoundSystem()->StopDigitalMusic();
    SoundSystem()->PlayDigitalMusic(0x16, true);
}

void SlideRadioVolume(MCGuiObject* object, MCGuiEvent* event)
{
    if (event->Type != 4)
    {
        return;
    }

    if (const std::optional<uint8_t> volume = SliderVolume(object))
    {
        SoundSystem()->RadioLevel = *volume;
    }

    SoundSystem()->PlayPilotSpeech(const_cast<char*>("pilotd"), 0x15);
}

void SlideFXVolume(MCGuiObject* object, MCGuiEvent* event)
{
    if (event->Type != 4)
    {
        return;
    }

    if (const std::optional<uint8_t> volume = SliderVolume(object))
    {
        SoundSystem()->DigitalMasterVolume = *volume;
    }

    SoundSystem()->PlayDigitalSample(0xf, 1, nullptr, 0, 0);
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
