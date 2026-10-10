#include "stdafx.h"
#include "MCTest.h"
#include "fakes/MCMemoryFileSource.h"
#include "fakes/MCNullAudioDevice.h"
#include "lib/MCFitIniFile.h"
#include "main/MCGameContext.h"
#include "main/MCMissionGlobals.h"
#include "mission/MCDifficultySettings.h"
#include "mission/MCMissionResults.h"
#include "mission/MCScenario.h"
#include "sound/MCSoundSystem.h"

namespace
{
    /// <summary>A test context whose files are <paramref name="text"/> at <c>data\test.fit</c>, opened.</summary>
    struct MCFitFixture
    {
        explicit MCFitFixture(std::string_view text)
        {
            scope.Context().SetFiles(std::make_unique<MCMemoryFileSource>()).AddFile("data\\test.fit", text);
            result = file.Open("data\\test.fit");
        }

        MCTestContextScope scope;
        MCFitIniFile file;
        int32_t result = -1;
    };

    /// <summary>
    /// A scenario installed on its own (no world), with a sound system on the null device (sound off); the clocks it
    /// moves are put back afterwards.
    /// </summary>
    struct MCScenarioFixture
    {
        MCScenarioFixture()
        {
            scope.Context().SetAudio(std::make_unique<MCNullAudioDevice>());
            scope.Context().SetSoundSystem(std::make_unique<MCSoundSystem>());
            scope.Context().SetScenario(std::make_unique<MCScenario>());
            scenario = scope.Context().Scenario();
            Turn = 0;
            ScenarioTime = 0.0f;
            // Betty's samples need the retail sound file: the warnings are checked by their flags.
            UseSound = 0;
        }

        ~MCScenarioFixture()
        {
            Turn = savedTurn;
            ScenarioTime = savedTime;
            FrameLength = savedFrameLength;
            ActualTime = savedActualTime;
            UseSound = savedUseSound;
        }

        int32_t savedTurn = Turn;
        float savedTime = ScenarioTime;
        float savedFrameLength = FrameLength;
        float savedActualTime = ActualTime;
        int32_t savedUseSound = UseSound;
        MCTestContextScope scope;
        MCScenario* scenario = nullptr;
    };

    /// <summary>Two objectives: a primary worth 5000 with a 300-second timer, and a secondary with no points entry.</summary>
    constexpr std::string_view TwoObjectives = "FITini\n"
                                               "[Objective0]\n"
                                               "st Name = \"Destroy the base\"\n"
                                               "ul Type = 0\n"
                                               "f TimeLeft = 300.0\n"
                                               "ul Status = 0\n"
                                               "l Points = 5000\n"
                                               "f Radius = 128.0\n"
                                               "[Objective1]\n"
                                               "st Name = \"Save the convoy\"\n"
                                               "ul Type = 1\n"
                                               "f TimeLeft = 0.0\n"
                                               "ul Status = 0\n"
                                               "FITend\n";
}

/// <summary>
/// The objectives read from the scenario FIT: the named entries, Points and Radius 0 when missing, and the position
/// unknown (-99) until the script sets it; the slots after them are unused.
/// </summary>
TEST_CASE("scenario: objectives load from the scenario FIT")
{
    MCFitFixture fit(TwoObjectives);
    REQUIRE_EQ(fit.result, 0);
    MCObjectiveList objectives;
    objectives.Load(fit.file, 2);
    CHECK_EQ(objectives.Count(), 2);
    CHECK_EQ(objectives[0].Name, std::string("Destroy the base"));
    CHECK_EQ(objectives[0].Type, MCScenarioObjective::Primary);
    CHECK_EQ(objectives[0].TimeLeft, 300.0f);
    CHECK_EQ(objectives[0].Points, 5000);
    CHECK_EQ(objectives[0].Radius, 128.0f);
    CHECK_EQ(objectives[0].Position[0], -99.0f);
    CHECK_EQ(objectives[1].Type, MCScenarioObjective::Secondary);
    CHECK_EQ(objectives[1].Points, 0);
    CHECK_EQ(objectives[1].Radius, 0.0f);

    for (int32_t i = 2; i < MCObjectiveList::MaxObjectives; i++)
    {
        MCTest::Scope slot(std::format("slot {}", i));
        CHECK_EQ(objectives[i].Type, MCScenarioObjective::Unused);
        CHECK_EQ(objectives[i].Status, MCScenarioObjective::Unused);
    }
}

/// <summary>
/// The script changes an objective by number: success and failure stick, and a number outside the scenario's own
/// objectives is refused with the original's codes (sets give -0x550fff4, queries 9999).
/// </summary>
TEST_CASE("scenario: objectives succeed and fail by number; a bad number is refused")
{
    MCFitFixture fit(TwoObjectives);
    REQUIRE_EQ(fit.result, 0);
    MCObjectiveList objectives;
    objectives.Load(fit.file, 2);

    CHECK_EQ(objectives.Status(0), MCScenarioObjective::Pending);
    CHECK_EQ(objectives.SetStatus(0, MCScenarioObjective::Succeeded), 0);
    CHECK_EQ(objectives.SetStatus(1, MCScenarioObjective::Failed), 0);
    CHECK_EQ(objectives.Status(0), MCScenarioObjective::Succeeded);
    CHECK_EQ(objectives.Status(1), MCScenarioObjective::Failed);
    CHECK_EQ(objectives.SetType(1, MCScenarioObjective::Primary), 0);
    CHECK_EQ(objectives.Type(1), MCScenarioObjective::Primary);

    // The unused slots and negative numbers aren't the script's to change.
    CHECK_EQ(objectives.SetStatus(2, MCScenarioObjective::Succeeded), MCObjectiveList::BadObjective);
    CHECK_EQ(objectives.SetStatus(-1, MCScenarioObjective::Succeeded), MCObjectiveList::BadObjective);
    CHECK_EQ(objectives.SetType(2, MCScenarioObjective::Primary), MCObjectiveList::BadObjective);
    CHECK_EQ(objectives.Status(2), MCObjectiveList::NoObjective);
    CHECK_EQ(objectives.Type(-1), MCObjectiveList::NoObjective);
    CHECK_EQ(objectives[2].Status, MCScenarioObjective::Unused);

    objectives.SetPosition(0, 1.0f, 2.0f, 3.0f);
    objectives.SetPosition(5, 9.0f, 9.0f, 9.0f);
    CHECK_EQ(objectives[0].Position[1], 2.0f);
    CHECK_EQ(objectives[5].Position[0], 0.0f);
}

/// <summary>
/// A lost scenario earns nothing; a won one earns the points of the objectives that succeeded; when the mission ended
/// the scenario early (the network or the pause menu), every objective's points count.
/// </summary>
TEST_CASE("scenario: resource points earned depend on the result and the objectives met")
{
    MCObjectiveList objectives;
    objectives[0].Points = 5000;
    objectives[0].Status = MCScenarioObjective::Succeeded;
    objectives[1].Points = 2000;
    objectives[1].Status = MCScenarioObjective::Failed;

    CHECK_EQ(objectives.ResourcePointsEarned(2, false), 0);
    CHECK_EQ(objectives.ResourcePointsEarned(3, false), 0);
    CHECK_EQ(objectives.ResourcePointsEarned(4, false), 5000);
    CHECK_EQ(objectives.ResourcePointsEarned(4, true), 7000);
    CHECK_EQ(objectives.ResourcePointsEarned(2, true), 0);
    CHECK_EQ(objectives.SucceededPoints(), 5000);
}

/// <summary>
/// The unused drop weight becomes a succeeded objective in the first unused slot: BonusPointsPerTon points per
/// BonusTonnageDivisor whole tons, named from the string resource's format.
/// </summary>
TEST_CASE("scenario: the tonnage bonus takes the first unused slot")
{
    MCFitFixture fit(TwoObjectives);
    REQUIRE_EQ(fit.result, 0);
    MCObjectiveList objectives;
    objectives.Load(fit.file, 2);
    objectives.SetStatus(0, MCScenarioObjective::Succeeded);

    // 23 tons unused, 5 tons per unit, 200 points per unit: 4 whole units.
    objectives.AddTonnageBonus(23, 5, 200, "%d tons unused");
    CHECK_EQ(objectives[2].Type, MCScenarioObjective::TonnageBonus);
    CHECK_EQ(objectives[2].Status, MCScenarioObjective::Succeeded);
    CHECK_EQ(objectives[2].Points, 800);
    CHECK_EQ(objectives[2].Name, std::string("23 tons unused"));
    CHECK_EQ(objectives.Count(), 2);
    CHECK_EQ(objectives.ResourcePointsEarned(4, false), 5800);
}

/// <summary>
/// A part block of the scenario FIT: team 1 is the Clan (alignment -1), a long CommanderId is read when there is no
/// char one, and a missing PaintScheme means the pilot's (-1).
/// </summary>
TEST_CASE("scenario: a part reads from the scenario FIT")
{
    MCFitFixture fit("FITini\n"
                     "[Part3]\n"
                     "ul ObjectNumber = 52\n"
                     "ul ControlType = 2\n"
                     "ul ControlDataType = 1\n"
                     "st ObjectProfile = \"NONE\"\n"
                     "ul Pilot = 4\n"
                     "f PositionX = 100.0\n"
                     "f PositionY = -50.5\n"
                     "f PositionZ = 0.0\n"
                     "f Rotation = 45.0\n"
                     "c TeamId = 1\n"
                     "l CommanderId = 1\n"
                     "ul Gesture = 2\n"
                     "f Velocity = 0.0\n"
                     "l Active = 1\n"
                     "l Exists = 0\n"
                     "c MyIcon = 0\n"
                     "b Captureable = TRUE\n"
                     "FITend\n");
    REQUIRE_EQ(fit.result, 0);
    const MCPart part = ReadScenarioPart(fit.file, 3);
    CHECK_EQ(part.ObjNumber, 52u);
    CHECK_EQ(part.ProfileName, std::string("NONE"));
    CHECK_EQ(part.Pilot, 4u);
    CHECK_EQ(part.Position[1], -50.5f);
    CHECK_EQ(part.Rotation, 45.0f);
    CHECK_EQ(part.TeamId, int8_t{1});
    CHECK_EQ(part.Alignment, int8_t{-1});
    CHECK_EQ(part.CommanderId, 1);
    CHECK_EQ(part.PaintScheme, -1);
    CHECK_EQ(part.Exists, 0);
    CHECK(part.Captureable);
    CHECK(part.Object == nullptr);
}

/// <summary>
/// The difficulty percentages scale skills and weapon damage on easy and hard only; weapon damage then rounds down to
/// a quarter and stays within 0-255; easy and hard set the salvage chance, normal keeps the game system's.
/// </summary>
TEST_CASE("scenario: difficulty settings scale skills, weapons and salvage")
{
    MCFitFixture fit("FITini\n"
                     "[DifficultySettings]\n"
                     "l[2] PlayerSkills = 120, 80\n"
                     "l[2] EnemySkills = 80, 120\n"
                     "l[2] PlayerWeapons = 150, 50\n"
                     "l[2] EnemyWeapons = 50, 150\n"
                     "l[2] SalvageChance = 40, 10\n"
                     "FITend\n");
    REQUIRE_EQ(fit.result, 0);
    MCDifficultySettings settings;
    settings.Load(fit.file);

    CHECK_EQ(settings.ApplySkill(50.0f, true, 0), 60.0f);
    CHECK_EQ(settings.ApplySkill(50.0f, false, 0), 40.0f);
    CHECK_EQ(settings.ApplySkill(50.0f, true, 1), 50.0f);
    CHECK_EQ(settings.ApplySkill(50.0f, true, 2), 40.0f);

    // 3.1 * 1.5 = 4.65, down to 4.5; 3.1 * 0.5 = 1.55, down to 1.5; normal 3.1 rounds down to 3.0.
    CHECK_EQ(settings.ApplyWeapon(3.1f, true, 0), 4.5f);
    CHECK_EQ(settings.ApplyWeapon(3.1f, true, 2), 1.5f);
    CHECK_EQ(settings.ApplyWeapon(3.1f, false, 1), 3.0f);
    CHECK_EQ(settings.ApplyWeapon(200.0f, true, 0), 255.0f);
    CHECK_EQ(settings.ApplyWeapon(-4.0f, true, 1), 0.0f);

    CHECK_EQ(settings.Salvage(25, 0), 40);
    CHECK_EQ(settings.Salvage(25, 1), 25);
    CHECK_EQ(settings.Salvage(25, 2), 10);
}

/// <summary>
/// The scenario clock: a frame is at most a quarter second and 0.05 s when the clock gave none; the first ten turns
/// are the start-up countdown (ten per turn left), after which the player has control.
/// </summary>
TEST_CASE("scenario: the clock caps a frame and counts the start-up turns down")
{
    MCScenarioFixture fixture;
    MCScenario& scenario = *fixture.scenario;
    scenario.StartUpTurns = MCScenario::DefaultStartUpTurns;

    FrameLength = 1.0f;
    scenario.Update();
    CHECK_EQ(FrameLength, MCScenario::MaxFrameLength);
    CHECK_EQ(ScenarioTime, 0.25f);
    CHECK_EQ(ActualTime, 0.25f);
    CHECK_EQ(Turn, 1);
    CHECK(scenario.StartingUp);
    CHECK_EQ(scenario.StartUpCountdown, 90);

    FrameLength = 0.0f;
    scenario.Update();
    CHECK_EQ(FrameLength, MCScenario::DefaultFrameLength);
    CHECK_EQ(scenario.StartUpCountdown, 80);

    for (int32_t turn = 3; turn <= 10; turn++)
    {
        scenario.Update();
    }

    CHECK_EQ(Turn, 10);
    CHECK(!scenario.StartingUp);
    CHECK_EQ(scenario.StartUpCountdown, 0);
}

/// <summary>
/// With a time limit, Betty warns once when less than two minutes are left and once under thirty seconds; a scenario
/// without one (-1) never warns.
/// </summary>
TEST_CASE("scenario: the time limit warns at two minutes and at thirty seconds")
{
    MCScenarioFixture fixture;
    MCScenario& scenario = *fixture.scenario;
    scenario.TimeLimit = 200;
    FrameLength = 0.25f;

    ScenarioTime = 79.0f;
    scenario.Update();
    CHECK(!scenario.TwoMinuteWarningPlayed);

    ScenarioTime = 80.0f;
    scenario.Update();
    CHECK(scenario.TwoMinuteWarningPlayed);
    CHECK(!scenario.ThirtySecondWarningPlayed);

    ScenarioTime = 170.0f;
    scenario.Update();
    CHECK(scenario.ThirtySecondWarningPlayed);

    MCScenarioFixture untimed;
    untimed.scenario->TimeLimit = -1;
    ScenarioTime = 1000.0f;
    untimed.scenario->Update();
    CHECK(!untimed.scenario->TwoMinuteWarningPlayed);
    CHECK(!untimed.scenario->ThirtySecondWarningPlayed);
}

/// <summary>
/// A results line's sort key: (3 - rank) * 10000, then each of the first three callsign letters times ten, XORed with
/// 2, 1, 0 (OB-058: meant as letter * 100, * 10, * 1). A short callsign counts its end as 0.
/// </summary>
TEST_CASE("results: the pilot sort key ranks first, then the callsign (OB-058)")
{
    // 'B' 66 -> 660 ^ 2 = 662, 'o' 111 -> 1110 ^ 1 = 1111, 'b' 98 -> 980 ^ 0 = 980.
    CHECK_EQ(PilotSortKey(2, "Bob"), 10000 + 662 + 1111 + 980);
    CHECK_EQ(PilotSortKey(2, "Bobcat"), PilotSortKey(2, "Bob"));
    // 'A' 65 -> 650 ^ 2 = 648, 'l' 108 -> 1080 ^ 1 = 1081, end -> 0 ^ 0 = 0.
    CHECK_EQ(PilotSortKey(0, "Al"), 30000 + 648 + 1081);
    // A higher rank sorts first, whatever the name.
    CHECK(PilotSortKey(3, "Zed") < PilotSortKey(2, "Abe"));
}

/// <summary>
/// After a won scenario each rank's worth of skill points buys a rank, at most three and not past the highest skill;
/// points keep being spent when no more ranks can be bought; a skill of 0 never rises.
/// </summary>
TEST_CASE("results: skill-ups spend a rank's worth of points per rank")
{
    float rank = 3.0f;
    float points = 10.0f;
    CHECK_EQ(ApplySkillUps(rank, points, 7.0f), 2);
    CHECK_EQ(rank, 5.0f);
    CHECK_EQ(points, 3.0f);

    // Three ranks at most; the rest is spent at the new rank: 100 - 1 - 2 - 3 = 94, then 94 - 23 * 4 = 2.
    rank = 1.0f;
    points = 100.0f;
    CHECK_EQ(ApplySkillUps(rank, points, 10.0f), 3);
    CHECK_EQ(rank, 4.0f);
    CHECK_EQ(points, 2.0f);

    // Capped at the highest skill: 100 - 6 = 94, then 94 - 13 * 7 = 3.
    rank = 6.0f;
    points = 100.0f;
    CHECK_EQ(ApplySkillUps(rank, points, 7.0f), 1);
    CHECK_EQ(rank, 7.0f);
    CHECK_EQ(points, 3.0f);

    rank = 0.0f;
    points = 50.0f;
    CHECK_EQ(ApplySkillUps(rank, points, 7.0f), 0);
    CHECK_EQ(points, 50.0f);
}

/// <summary>
/// The pilot lines sort by key, lowest first; the commanders by score, best first (no pilots, -1, last). Equal keys
/// keep MCX.EXE's qsort order (R5): the VC6 sort of these arrays.
/// </summary>
TEST_CASE("results: pilot lines and commander scores sort as the original did")
{
    std::array<int32_t, 4> tags{};
    std::vector<MCMissionPilotResult> pilots(4);
    const std::array<int32_t, 4> keys = {300, 100, 200, 100};

    for (size_t i = 0; i < pilots.size(); i++)
    {
        pilots[i].SortKey = keys[i];
        pilots[i].Warrior = reinterpret_cast<MCMechWarrior*>(&tags[i]);
    }

    SortPilotResults(pilots);
    CHECK_EQ(pilots[0].SortKey, 100);
    CHECK_EQ(pilots[1].SortKey, 100);
    CHECK_EQ(pilots[2].SortKey, 200);
    CHECK_EQ(pilots[3].SortKey, 300);
    // The VC6 short sort moves the first largest to the end each pass: 300 swaps with the last 100, then the first of
    // the two 100s (that one) swaps with the other.
    CHECK(pilots[0].Warrior == reinterpret_cast<MCMechWarrior*>(&tags[1]));
    CHECK(pilots[1].Warrior == reinterpret_cast<MCMechWarrior*>(&tags[3]));

    std::array<MCMissionCommanderScore, 6> scores = {{{0, -1}, {1, 5}, {2, 2}, {3, 5}, {4, -1}, {5, 0}}};
    SortCommanderScores(scores);
    const std::array<int32_t, 6> sorted = {5, 5, 2, 0, -1, -1};

    for (size_t i = 0; i < scores.size(); i++)
    {
        MCTest::Scope place(std::format("place {}", i + 1));
        CHECK_EQ(scores[i].Score, sorted[i]);
    }
}

/// <summary>
/// The home side lost a multiplayer game when the script says quit (3), or when the other side won: result 1 is the
/// Inner Sphere's win (the Clan side, alignment -1, lost), 2 the Clan's (the Inner Sphere side, alignment 1, lost).
/// </summary>
TEST_CASE("results: the home side's multiplayer loss")
{
    CHECK(HomeSideLost(3, 1));
    CHECK(HomeSideLost(3, -1));
    CHECK(HomeSideLost(1, -1));
    CHECK(!HomeSideLost(1, 1));
    CHECK(HomeSideLost(2, 1));
    CHECK(!HomeSideLost(2, -1));
    CHECK(!HomeSideLost(4, 1));
}
