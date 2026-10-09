#include "stdafx.h"
#include "MCTest.h"
#include "ScreenInput.h"
#include "TestGame.h"
#include "fakes/MCMemoryFileSource.h"
#include "gui/MCGuiSmackerWindow.h"
#include "gui/MCScrollPane.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "logistics/MCBriefingScreen.h"
#include "logistics/MCConnectMenu.h"
#include "logistics/MCFileScrollPane.h"
#include "logistics/MCGenericScreen.h"
#include "logistics/MCLoadSaveMenu.h"
#include "logistics/MCMainMenu.h"
#include "logistics/MCMechBriefBlock.h"
#include "logistics/MCMissionLogisticsBridge.h"
#include "logistics/MCPreferencesMenu.h"
#include "logistics/MCRegistrySettings.h"
#include "logistics/MCReusableDialog.h"
#include "logistics/MCSplashScreen.h"
#include "main/MCGameContext.h"
#include "main/logistics.h"
#include "mission/MCScenario.h"
#include "platform/MCFileSystem.h"
#include "sound/MCSoundSystem.h"

// The menus, the briefing screen and the mission/logistics bridge (P3-log-3): the registry version, the starting
// fit's components, a pilot's profile, the temp folder's clean-up, the menus' screens and states, the drop slots'
// tonnage limit and the operation movie's start delay.

using namespace MCScreenInput;

TEST_CASE("logistics: the stored version matches the build on its first 13 characters and a dot")
{
    constexpr std::string_view build = "MCX v1.8 1234";
    REQUIRE_EQ(build.size(), size_t{13});
    CHECK(RegistryVersionMatches("MCX v1.8 1234.", build));
    // What follows the dot doesn't matter; the build string past 13 characters neither.
    CHECK(RegistryVersionMatches("MCX v1.8 1234.5", build));
    CHECK(RegistryVersionMatches("MCX v1.8 1234.", "MCX v1.8 1234 extra"));
    // No dot, another character there, another build.
    CHECK(!RegistryVersionMatches("MCX v1.8 1234", build));
    CHECK(!RegistryVersionMatches("MCX v1.8 1234-", build));
    CHECK(!RegistryVersionMatches("MCX v1.8 1235.", build));
    CHECK(!RegistryVersionMatches("", build));
    // A build string shorter than 13 characters never matches: the stored one has the dot where the build ended.
    CHECK(!RegistryVersionMatches("MCX v1.8 1.", "MCX v1.8 1"));
    CHECK(!RegistryVersionMatches("MCX v1.8 1", "MCX v1.8 1"));
}

TEST_CASE("logistics: a starting fit counts 50 components, each once, each with its comment")
{
    CHECK_EQ(FitComponents.size(), size_t{50});
    std::set<uint8_t> ids;

    for (const MCFitComponent& component : FitComponents)
    {
        MCTest::Scope scope(std::string(component.Comment));
        CHECK(component.Comment.starts_with("// "));
        CHECK(ids.insert(component.Id).second);
    }

    // The first and the last of the file format's blocks.
    CHECK_EQ(FitComponents.front().Id, uint8_t{0x90});
    CHECK_EQ(FitComponents.back().Id, uint8_t{0xa1});
}

namespace
{
    /// <summary>A test context whose files are in memory (writes land in its scratch folder).</summary>
    struct MemoryFiles
    {
        MemoryFiles() : Files(Scope.Context().SetFiles(std::make_unique<MCMemoryFileSource>())) {}

        MCTestContextScope Scope;
        MCMemoryFileSource& Files;
    };

    /// <summary>Entry <paramref name="name"/> of block <paramref name="block"/> of <paramref name="file"/>.</summary>
    template <MCFitValue T> T Entry(MCFitIniFile& file, std::string_view block, std::string_view name)
    {
        REQUIRE_EQ(file.SeekBlock(block), 0);
        const MCFitResult<T> value = file.Read<T>(name);
        REQUIRE(value.has_value());
        return *value;
    }
}

TEST_CASE("logistics: a pilot's profile from logistics holds its traits, skills and wounds")
{
    MemoryFiles memory;
    std::string name = "Jane Doe";
    std::string callsign = "Ghost";
    std::string picture = "pilot05.tga";
    std::string video = "video5";
    std::string audio = "audio5";
    std::string brain = "pbrain";
    MCLogWarrior warrior;
    warrior.Name = name.data();
    warrior.Callsign = callsign.data();
    warrior.Picture = picture.data();
    warrior.PilotVideo = video.data();
    warrior.PilotAudio = audio.data();
    warrior.Brain = brain.data();
    warrior.PaintScheme = 3;
    warrior.NameIndex = 7;
    warrior.DescIndex = 9;
    warrior.Assigned = 1;
    std::ranges::copy(std::string_view("\x0a\x14\x1e\x28"), warrior.Personality);
    std::ranges::copy(std::string_view("\x32\x33\x34\x35"), warrior.Skills);
    std::ranges::copy(std::string_view("\x28\x29\x2a\x2b"), warrior.OriginalSkills);
    std::ranges::copy(std::string_view("\x2d\x2e\x2f\x30"), warrior.StartingSkills);
    warrior.SkillPoints[2] = 1.5f;
    warrior.Wounds = 2.0f;
    // A longer name was there: the profile's name takes the first 9 bytes, as strncpy wrote them.
    std::ranges::copy(std::string_view("oldprofile1"), warrior.FileName);

    REQUIRE_EQ(MCMissionLogisticsBridge::LogisticsWarriorProfileWriter("tpak3", &warrior), 0);
    CHECK_EQ(std::string_view(warrior.FileName), std::string_view("tpak3"));
    CHECK_EQ(warrior.FileName[9], 'e');
    CHECK_EQ(warrior.FileName[10], '1');

    MCFitIniFile file;
    REQUIRE_EQ(file.Open(GamePath(SaveTempPath, "tpak3", ".fit")), 0);
    CHECK_EQ(Entry<std::string>(file, "General", "Name"), name);
    CHECK_EQ(Entry<std::string>(file, "General", "Callsign"), callsign);
    CHECK_EQ(Entry<std::string>(file, "General", "Picture"), picture);
    CHECK_EQ(Entry<std::string>(file, "General", "Brain"), brain);
    CHECK_EQ(Entry<int32_t>(file, "General", "paintScheme"), 3);
    CHECK_EQ(Entry<int32_t>(file, "General", "NameIndex"), 7);
    CHECK_EQ(Entry<int32_t>(file, "General", "DescIndex"), 9);
    CHECK(Entry<bool>(file, "General", "Assigned"));
    CHECK(!Entry<bool>(file, "General", "NotMineYet"));
    CHECK_EQ(Entry<char>(file, "PersonalityTraits", "Professionalism"), char{0x0a});
    CHECK_EQ(Entry<char>(file, "PersonalityTraits", "Courage"), char{0x28});
    CHECK_EQ(Entry<char>(file, "Skills", "Piloting"), char{0x32});
    CHECK_EQ(Entry<char>(file, "Skills", "Gunnery"), char{0x35});
    CHECK_EQ(Entry<char>(file, "OriginalSkills", "Jumping"), char{0x29});
    // "LatestSkills" are the skills the pilot started the mission with.
    CHECK_EQ(Entry<char>(file, "LatestSkills", "Sensors"), char{0x2f});
    CHECK_EQ(Entry<float>(file, "SkillPoints", "Sensors"), 1.5f);
    CHECK_EQ(Entry<char>(file, "Status", "Wounds"), char{2});
}

TEST_CASE("logistics: the temp folder's FIT files are deleted, nothing else")
{
    MemoryFiles memory;
    const std::string folder = SaveTempPath;
    memory.Files.AddUserFile(folder + "tpak0.fit", std::vector<uint8_t>{'a'});
    memory.Files.AddUserFile(folder + "bridge.fit", std::vector<uint8_t>{'b'});
    memory.Files.AddUserFile(folder + "keep.pak", std::vector<uint8_t>{'c'});
    memory.Files.AddUserFile("data\\save\\other.fit", std::vector<uint8_t>{'d'});
    DestroyAllFitFiles(folder);
    CHECK(!MCFileSystem::Exists(folder + "tpak0.fit"));
    CHECK(!MCFileSystem::Exists(folder + "bridge.fit"));
    CHECK(MCFileSystem::Exists(folder + "keep.pak"));
    CHECK(MCFileSystem::Exists("data\\save\\other.fit"));
}

namespace
{
    /// <summary>Runs the logistics screens for two seconds: a screen change or a dialog settles.</summary>
    void Settle()
    {
        for (int32_t frame = 0; frame < 30; frame++)
        {
            MCTestGame::RunFrame(1.0f / 15.0f);
        }
    }

    /// <summary>Runs the game for <paramref name="seconds"/> in frames of a fifteenth of a second.</summary>
    void RunFor(float seconds)
    {
        for (float run = 0.0f; run < seconds; run += 1.0f / 15.0f)
        {
            MCTestGame::RunFrame(1.0f / 15.0f);
        }
    }
}

TEST_CASE_ISOLATED("game: the main menu opens its screens in their states, and cancel comes back")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartLogistics());
    Settle();
    MCLogistics* logistics = GlobalLogPtr;
    REQUIRE(logistics->CurrentScreen == logistics->MainScreen);

    // The load screen: campaign saves, then single missions.
    LoadScreen();
    Settle();
    CHECK(logistics->CurrentScreen == logistics->LoadScreen);
    CHECK_EQ(logistics->LogisticsState, 5);
    CHECK(!LoadingSolo);
    CHECK(logistics->LoadScreen->FilePane->SelectedFile == -1);
    Cancel();
    Settle();
    CHECK(logistics->CurrentScreen == logistics->MainScreen);
    CHECK(WhackTimer);
    SoloLoadScreen();
    Settle();
    CHECK(logistics->CurrentScreen == logistics->LoadScreen);
    CHECK(LoadingSolo);
    Cancel();
    Settle();
    CHECK(logistics->CurrentScreen == logistics->MainScreen);
    CHECK(!LoadingSolo);

    // No campaign yet: nothing to save.
    if (logistics->CurrentMission == -1)
    {
        SaveScreen();
        Settle();
        CHECK(logistics->CurrentScreen == logistics->MainScreen);
    }

    // The preferences: cancel brings back the settings they opened with.
    const int32_t difficulty = GameDifficulty;
    const uint8_t music = SoundSystem()->MusicLevel;
    ShowPreferences();
    Settle();
    CHECK(logistics->CurrentScreen == logistics->PrefScreen);
    CHECK_EQ(logistics->LogisticsState, 9);
    GameDifficulty = difficulty == 2 ? 0 : 2;
    SoundSystem()->MusicLevel = static_cast<uint8_t>(music == 10 ? 20 : 10);
    HardToggle();
    CHECK_EQ(GameDifficulty, 2);
    EasyToggle();
    CHECK_EQ(GameDifficulty, 0);
    CancelPrefs();
    Settle();
    CHECK(logistics->CurrentScreen == logistics->MainScreen);
    CHECK_EQ(GameDifficulty, difficulty);
    CHECK_EQ(SoundSystem()->MusicLevel, music);

    // The multiplayer menu.
    ShowMultiPlayer();
    Settle();
    CHECK(logistics->CurrentScreen == logistics->MultiplayerScreen);
    CHECK_EQ(logistics->LogisticsState, 10);
    CHECK(!Solo);
    Cancel();
    Settle();
    CHECK(logistics->CurrentScreen == logistics->MainScreen);

    // A new campaign opens on the briefing screen, and the menu's return goes back there.
    NewCampaign();
    Settle();
    CHECK(logistics->CurrentScreen == logistics->BriefingScreen);
    CHECK(logistics->CurrentMission != -1);
}

namespace
{
    /// <summary>The middle of drop slot <paramref name="slot"/>.</summary>
    POINT SlotMiddle(size_t slot)
    {
        const RECT& area = GlobalLogPtr->BriefingScreen->SlotRects[slot];
        return {(area.left + area.right) / 2, (area.top + area.bottom) / 2};
    }

    /// <summary>Drags <paramref name="mech"/>'s block (wherever it is) to (<paramref name="x"/>, <paramref name="y"/>).</summary>
    void DragMech(MCLogMech* mech, int32_t x, int32_t y)
    {
        MCMechBriefBlock* block = mech->BriefBlock;
        REQUIRE(block != nullptr);
        Drag(block->GlobalX() + block->Width() / 2, block->GlobalY() + block->Height() / 2, x, y);
        Settle();
    }
}

TEST_CASE_ISOLATED("game: units dragged into the drop slots count against the drop tonnage")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartLogistics());
    Settle();
    NewCampaign();
    Settle();
    MCLogistics* logistics = GlobalLogPtr;
    MCBriefingScreen* briefing = logistics->BriefingScreen;
    REQUIRE(logistics->CurrentScreen == briefing);
    REQUIRE(logistics->LocalDropSlot[0] != 0);
    REQUIRE_EQ(logistics->HammerDown, 0);
    MCScrollPane* pane = briefing->DeployPane.get();
    REQUIRE(pane->NumberOfChildren() > 0);
    MCLogMech* mech = static_cast<MCMechBriefBlock*>(pane->Child(0))->Mech;
    REQUIRE(mech != nullptr);
    REQUIRE(mech->Deployed == 0);
    const auto tons = static_cast<int32_t>(mech->CurTonnage);
    const int32_t startTons = CurDeployTonnage;
    const auto& slot0 = logistics->DeploySlots[0][0];
    REQUIRE_EQ(slot0.Unit, -1);
    const POINT middle = SlotMiddle(0);
    const POINT paneMiddle{pane->GlobalX() + pane->Width() / 2, pane->GlobalY() + pane->Height() / 2};

    // Into the first slot: the lance's tonnage grows by the mech's, the block shows in the slot.
    MaxDeployTonnage = startTons + tons;
    DragMech(mech, middle.x, middle.y);
    CHECK(mech->Deployed != 0);
    CHECK_EQ(slot0.Unit, logistics->ForceMechList->GetMechIndex(mech));
    CHECK_EQ(CurDeployTonnage, startTons + tons);
    CHECK(briefing->SlotBlocks[0] == mech->BriefBlock);
    CHECK(mech->BriefBlock->Parent == briefing);
    CHECK(std::ranges::find(briefing->UndeployedMechs, logistics->ForceMechList->GetMechIndex(mech)) ==
          briefing->UndeployedMechs.end());

    // Back to the deploy pane: the slot is empty again.
    DragMech(mech, paneMiddle.x, paneMiddle.y);
    CHECK(mech->Deployed == 0);
    CHECK_EQ(slot0.Unit, -1);
    CHECK_EQ(CurDeployTonnage, startTons);
    CHECK(briefing->SlotBlocks[0] == nullptr);
    CHECK(mech->BriefBlock->Parent == pane);

    // A ton over the limit: refused with the message, the mech stays in the pane.
    MaxDeployTonnage = startTons + tons - 1;
    DragMech(mech, middle.x, middle.y);
    CHECK(mech->Deployed == 0);
    CHECK_EQ(slot0.Unit, -1);
    CHECK_EQ(CurDeployTonnage, startTons);
    CHECK(logistics->MessageDialog->ShowWindow != 0);
    CHECK(mech->BriefBlock->Parent == pane);
}

TEST_CASE_ISOLATED("game: the operation tab starts the briefing movie half a second later")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartLogistics());
    Settle();
    NewCampaign();
    Settle();
    MCBriefingScreen* briefing = GlobalLogPtr->BriefingScreen;
    REQUIRE(GlobalLogPtr->CurrentScreen == briefing);
    briefing->SetUpMission();
    CHECK_EQ(briefing->CurrentTab, MCBriefingScreen::MissionTab);
    CHECK(briefing->SmackerWindow == nullptr);

    // The tab: the operation art at once, the movie after its timer (500 ms).
    briefing->SetUpOperation();
    CHECK_EQ(briefing->CurrentTab, MCBriefingScreen::OperationTab);
    CHECK(briefing->OperationShown);
    CHECK(!briefing->PlayMovie);
    RunFor(0.3f);
    CHECK(!briefing->PlayMovie);
    CHECK(briefing->SmackerWindow == nullptr);
    RunFor(0.4f);
    CHECK(briefing->PlayMovie);

    if (GlobalLogPtr->OperationCinema != nullptr)
    {
        CHECK(briefing->SmackerWindow != nullptr);

        // The mission tab stops it.
        briefing->SetUpMission();
        CHECK(briefing->SmackerWindow == nullptr);
        CHECK(!briefing->PlayMovie);
    }
}
