#include "stdafx.h"
#include "mission/scenario.h"
#include "platform/MCInput.h"
#include "platform/MCDisplay.h"
#include "abl/ablenv.h"
#include "abl/ablrtn.h"
#include "abl/ablxstd.h"
#include "ai/move.h"
#include "ai/tacordr.h"
#include "appear/MCAppearanceType.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "color/MCPalette.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCVfxElement.h"
#include "engine/MCCraterManager.h"
#include "gui/asystem.h"
#include "gui/atextbox.h"
#include "gui/updisp.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "lib/MCPacketFile.h"
#include "lib/MCPriorityQueue.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "main/honorb.h"
#include "main/main.h"
#include "mission/mission.h"
#include "network/multplyr.h"
#include "object/artlry.h"
#include "object/bldng.h"
#include "object/cmponent.h"
#include "object/collsn.h"
#include "object/comndr.h"
#include "object/contact.h"
#include "object/elemntl.h"
#include "object/fire.h"
#include "object/group.h"
#include "object/gvehicl.h"
#include "object/mech.h"
#include "object/mover.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/objtype.h"
#include "object/smoke.h"
#include "object/smokmgr.h"
#include "object/team.h"
#include "object/train.h"
#include "object/warrior.h"
#include "sound/soundsys.h"
#include "sprite/MCVfxBuildingAppearance.h"
#include "sprite/MCGVAppearance.h"
#include "sprite/MCElementalActor.h"
#include "sprite/MCMechActor.h"
#include "sprite/MCSpriteManager.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/MCVfxFunctions.h"
#include "platform/MCRenderer.h"

MCScenario* Scenario = nullptr;
float ActualTime = 0.0f;
int NextStep = 0;
int PrevStep = 0;
int32_t ScenarioEndTurn = -1;
float MinFrameLength = 0.25f;
float PartCreateTime = -1.0f;
int CollisionSwitch = 1;
int32_t TonnageDivisor = 5;
int32_t ResourcesPerTonDivided = 200;
uint32_t AblSymbolTableHeapSize = 102400;
uint32_t AblStackHeapSize = 40960;
uint32_t AblCodeHeapSize = 102400;
uint32_t AblRunTimeStackSize = 20480;
uint32_t AblMaxCodeBlockSize = 10240;
uint32_t AblMaxRegisteredModules = 200;
uint32_t AblMaxStaticVariables = 100;
MCCollisionSystem* CollisionSystem = nullptr;
MCBaseObject* MoverRoster[0xe00] = {};
int32_t MineLayThrottle = 0;
int32_t MineSweepThrottle = 0;
float MineWaitTime = 0.0f;
MCTeam* TeamTable[3] = {};
MCTrainManager* TrainManager = nullptr;
int32_t VisualRangeTable[256] = {};
int32_t GlobalPlayerWeapons[2] = {};
int32_t GlobalEnemySkills[2] = {};
MCCreatedPartRoster CreatedPartRoster[100] = {};
int32_t GlobalEnemyWeapons[2] = {};
int32_t GlobalPlayerSkills[2] = {};
int32_t GlobalSalvageModifier[2] = {};
uint32_t MissionStartTime = 0;
float RunningTime = 0.0f;
int32_t StartMusic = 0;
float InfluenceTime = 0.0f;
int DrawRevealedTacMap = 0;
int32_t CurrentCreatorPart = 0;
uint8_t* WaypointMarkers = nullptr;
int EndingScenario = 0;
uint8_t ForceAlways = 0;
char SaveTempPath[80] = "data\\save\\temp\\";

namespace
{
    /// <summary>The GUI timer id of objective 0's timer (the rest follow it).</summary>
    constexpr int16_t OBJECTIVE_TIMER_ID = 0x49f1;
    /// <summary>The event the objective timers send (the ABL scripts see it expire).</summary>
    constexpr int32_t OBJECTIVE_TIMER_EVENT = 0x1405;
    /// <summary>The objectives array always has this many entries.</summary>
    constexpr int32_t MAX_OBJECTIVES = 9;
    /// <summary>The type and status of an unused objective.</summary>
    constexpr uint32_t UNUSED_OBJECTIVE = static_cast<uint32_t>(-9999);
    /// <summary>What the objective functions return for a bad objective number.</summary>
    constexpr int32_t BAD_OBJECTIVE = -0x550fff4;
    /// <summary>The objective queries' answer for a bad objective number.</summary>
    constexpr uint32_t NO_OBJECTIVE = 9999;
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;

    /// <summary>Asserts that a FIT read or system start returned 0.</summary>
    void RequireOk(int32_t result, const char* message)
    {
        Assert(result == 0, static_cast<uint32_t>(result), message);
    }

    /// <summary>Turns a frame about its k axis (MC2's inline frame_of_ref::rotate_about_k).</summary>
    void RotateAboutK(MCFrameOfRef& frame, float s, float c)
    {
        const MCVector3D oldI = frame.I;
        frame.I = frame.I * c + frame.J * s;
        frame.J = frame.J * c - oldI * s;
    }

    /// <summary>A team with its (inlined) constructor: <c>Team::init()</c>.</summary>
    MCTeam* NewTeam()
    {
        auto* team = new MCTeam;

        if (team != nullptr)
        {
            team->MCTeam::Init();
        }

        return team;
    }

    /// <summary>A team's (inlined) destructor: <c>Team::destroy</c>, then free it.</summary>
    void DeleteTeam(MCTeam* team)
    {
        if (team == nullptr)
        {
            return;
        }

        team->MCTeam::Destroy();
        delete team;
    }

    /// <summary>A warrior with its (inlined) constructor: every tactical order cleared, then <c>init()</c>.</summary>
    MCMechWarrior* NewWarrior()
    {
        auto* warrior = new MCMechWarrior;

        if (warrior != nullptr)
        {
            for (MCTacticalOrder& order : warrior->TacOrder)
            {
                order.Init();
            }

            warrior->LastTacOrder.Init();
            warrior->CurTacOrder.Init();
            warrior->Init();
        }

        return warrior;
    }

    /// <summary>A warrior's (inlined) destructor: <c>destroy()</c>, its tactical orders, then free it.</summary>
    void DeleteWarrior(MCMechWarrior* warrior)
    {
        warrior->Destroy();
        warrior->CurTacOrder.Destroy();
        warrior->LastTacOrder.Destroy();

        for (int32_t i = NUM_ORDERSTATES - 1; i >= 0; --i)
        {
            warrior->TacOrder[i].Destroy();
        }

        delete warrior;
    }

    /// <summary>Reads a whole file into a new block (the scenario's connect and waypoint shapes).</summary>
    /// <returns>The block, or null when the file can't be opened (the old one is kept).</returns>
    uint8_t* LoadShapeFile(const char* name, uint8_t* oldShapes, int freeOld)
    {
        std::string fileName;
        fileName = GamePath(ShapesPath, name, ".shp");
        MCFile shapeFile;

        if (shapeFile.Open(fileName) != 0)
        {
            return oldShapes;
        }

        // Port fix: the original freed the malloc'd waypoint shapes with operator delete.
        if (freeOld && oldShapes != nullptr)
        {
            MCRenderer::UnregisterData(oldShapes);
            std::free(oldShapes);
        }

        auto* shapes = static_cast<uint8_t*>(std::malloc(shapeFile.FileSize()));

        if (shapes != nullptr)
        {
            shapeFile.Read(shapes, static_cast<int32_t>(shapeFile.FileSize()));
            MCRenderer::RegisterData(shapes, shapeFile.FileSize(), MCDataKind::Shapes);
        }

        shapeFile.Close();
        return shapes;
    }

    /// <summary>
    /// Opens <paramref name="name"/><c>.fit</c> in <paramref name="path"/> into <paramref name="file"/>, falling back
    /// on the temporary save folder (a saved game's copy).
    /// </summary>
    /// <returns>The result of the last open.</returns>
    int32_t OpenWithSaveFallback(MCFitIniFile& file, const char* path, const char* name)
    {
        std::string fileName;
        fileName = GamePath(path, name, ".fit");
        int32_t result = file.Open(fileName);

        if (result != 0)
        {
            fileName = GamePath(SaveTempPath, name, ".fit");
            result = file.Open(fileName);
        }

        return result;
    }
}

auto MCScenario::Update() -> int32_t
{
    if (FrameLength <= 0.0f)
    {
        FrameLength = 0.05f;
    }

    if (MinFrameLength < FrameLength)
    {
        FrameLength = MinFrameLength;
    }

    ScenarioTime = ScenarioTime + FrameLength;

    if (MissionStartTime == 0)
    {
        if (MPlayer != nullptr && 10.0f < ScenarioTime)
        {
            Fatal(0, " runningTime is not working...why? ");
        }
    }
    else
    {
        RunningTime = static_cast<float>(static_cast<double>(MCPort::Milliseconds() - MissionStartTime) * 0.001);
    }

    ActualTime = (MPlayer != nullptr) ? RunningTime : ScenarioTime;

    if (0 < Scenario->TimeLimit)
    {
        if (TwoMinuteWarningPlayed == 0 && static_cast<float>(Scenario->TimeLimit) - ActualTime < 120.0f)
        {
            SoundSystem->PlayBettySample(8);
            TwoMinuteWarningPlayed = 1;
        }

        if (ThirtySecondWarningPlayed == 0 && static_cast<float>(Scenario->TimeLimit) - ActualTime < 30.0f)
        {
            SoundSystem->PlayBettySample(7);
            ThirtySecondWarningPlayed = 1;
        }
    }

    const int32_t musicPending = StartMusic;
    Turn++;
    NextStep = 0;
    PrevStep = 0;

    if (Turn < StartUpTurns)
    {
        StartingUp = 1;
        StartUpCountdown = (StartUpTurns - Turn) * 10;
        return 0;
    }

    StartingUp = 0;
    StartUpCountdown = 0;

    if (musicPending != 0)
    {
        if (SoundSystem != nullptr)
        {
            SoundSystem->StopStaticNoise();

            if (SoundSystem != nullptr)
            {
                SoundSystem->PlayDigitalMusic(Scenario->ScenarioTuneNum, false);
            }
        }

        Application->SetCursorVisible(1);
        StartMusic = 0;
        MissionStartTime = MCPort::Milliseconds();
    }

    return 0;
}

auto MCScenario::Render(MCGuiObject* window) -> int32_t
{
    if (1 < Turn)
    {
        CameraList->RenderView(window);
    }

    return 0;
}

auto ApplyDifficultySkill(float skill, int player) -> float
{
    const int32_t* percentages = (player == 0) ? GlobalEnemySkills : GlobalPlayerSkills;

    if (GameDifficulty == 0)
    {
        skill = static_cast<float>(static_cast<double>(percentages[0]) / 100.0 * skill);
    }
    else if (GameDifficulty == 2)
    {
        skill = static_cast<float>(static_cast<double>(percentages[1]) / 100.0 * skill);
    }

    return skill;
}

auto ApplyDifficultyWeapon(float value, int player) -> float
{
    const int32_t* percentages = (player == 0) ? GlobalEnemyWeapons : GlobalPlayerWeapons;

    if (GameDifficulty == 0)
    {
        value = static_cast<float>(static_cast<double>(percentages[0]) / 100.0 * value);
    }
    else if (GameDifficulty == 2)
    {
        value = static_cast<float>(static_cast<double>(percentages[1]) / 100.0 * value);
    }

    // Round down to a quarter.
    const auto quarters = static_cast<int32_t>(value / 0.25);

    if (value != quarters * 0.25)
    {
        value = static_cast<float>(static_cast<int32_t>(value / 0.25) * 0.25);
    }

    if (value < 0.0)
    {
        value = 0.0f;
    }

    if (255.0 < value)
    {
        value = 255.0f;
    }

    return value;
}

auto InitDifficultySettings(MCFitIniFile* gameSystemFile) -> void
{
    int32_t result = gameSystemFile->SeekBlock("DifficultySettings");
    Assert(result == 0, static_cast<uint32_t>(result), "No Difficulty Settings in gameSys");
    result = gameSystemFile->ReadIdLongArray("PlayerSkills", GlobalPlayerSkills, 2);
    Assert(result == 0, static_cast<uint32_t>(result), "No Difficulty Settings for Player Skills in gameSys");
    result = gameSystemFile->ReadIdLongArray("EnemySkills", GlobalEnemySkills, 2);
    Assert(result == 0, static_cast<uint32_t>(result), "No Difficulty Settings for Enemy Skills in gameSys");
    result = gameSystemFile->ReadIdLongArray("PlayerWeapons", GlobalPlayerWeapons, 2);
    Assert(result == 0, static_cast<uint32_t>(result), "No Difficulty Settings for Player Weapons in gameSys");
    result = gameSystemFile->ReadIdLongArray("EnemyWeapons", GlobalEnemyWeapons, 2);
    Assert(result == 0, static_cast<uint32_t>(result), "No Difficulty Settings for Enemy Weapons in gameSys");
    result = gameSystemFile->ReadIdLongArray("SalvageChance", GlobalSalvageModifier, 2);
    Assert(result == 0, static_cast<uint32_t>(result), "Do Difficulty Settings for Salvage Chance in GameSys");
}

auto MCScenario::Init(char* scenarioName, char* terrainName) -> int32_t
{
    int32_t result = 0;
    TacOrderQueuePos = 0;
    NumCameraDrones = 0;

    ConnectShape = LoadShapeFile("connect", ConnectShape, 0);
    WaypointMarkers = LoadShapeFile("waypoints", WaypointMarkers, 1);

    AblInit(AblSymbolTableHeapSize, AblStackHeapSize, AblCodeHeapSize, AblRunTimeStackSize, AblMaxCodeBlockSize,
            AblMaxRegisteredModules, AblMaxStaticVariables, AblDebuggerPrintCallback, 0, 0, 0);
    Turn = 0;

    // The objects placed now but brought into play later by the script.
    auto* objectQueue = new MCObjectQueue;

    if (objectQueue != nullptr)
    {
        MCObjectQueueNode* node = objectQueue->FindList(DefaultListId);

        if (node == nullptr)
        {
            node = new MCObjectQueueNode(DefaultListId);

            if (node != nullptr)
            {
                objectQueue->AddList(node);
            }
        }

        objectQueue->Tail = node;
        objectQueue->Head = node;
    }

    ScenarioObjectList = objectQueue;
    std::memset(CreatedPartRoster, 0, sizeof(CreatedPartRoster));
    CurrentCreatorPart = 0;

    UpdateDisplay(0, 1, 100, 1, 0);
    SoundSystem->PlayStaticNoise();

    //---------------------------------------------------------------------------------------------------------------
    // The game system file.
    std::string gameSystemName;
    gameSystemName = GamePath(MissionPath, "gamesys", ".fit");
    auto* gameSystemFile = new MCFitIniFile;

    if (gameSystemFile == nullptr)
    {
        Fatal(static_cast<int32_t>(0xfaaf0001), " Game System File ");
    }

    result = gameSystemFile->Open(gameSystemName);
    RequireOk(result, " Could not open GameSys.Fit file ");

    result = gameSystemFile->SeekBlock("General");
    RequireOk(result, " Could not find General Block in GameSys ");
    result = gameSystemFile->ReadIdFloat("MaxVisualRange", MaxVisualRange);
    RequireOk(result, " Could not find MaxVisualRange in GameSys ");
    MaxVisualRadius = MaxVisualRange * 1.4142f;
    result = gameSystemFile->ReadIdFloat("FireVisualRange", FireVisualRange);
    RequireOk(result, " Could not find FireVisualRange in GameSys ");
    result = gameSystemFile->ReadIdFloat("MaxWeaponRange", MaxWeaponRange);
    RequireOk(result, " Could not find MaxWeaponRange in GameSys ");
    result = gameSystemFile->ReadIdFloatArray("WeaponRange", WeaponRange, 3);
    RequireOk(result, " Could not find WeaponRange in GameSys ");
    result = gameSystemFile->ReadIdFloat("DefaultAttackRange", DefaultAttackRange);

    if (result != 0)
    {
        DefaultAttackRange = 75.0f;
    }

    result = gameSystemFile->ReadIdFloat("BaseSensorRange", BaseSensorRange);
    RequireOk(result, " Could not find BaseSensorRange in GameSys ");
    result = gameSystemFile->ReadIdLongArray("VisualRangeTable", VisualRangeTable, 256);
    RequireOk(result, " Could not find Visual Range Table ");
    UpdateDisplay(0, 1, 30, 1, 2);

    if (MasterComponentList == nullptr)
    {
        std::string componentName;
        componentName = GamePath(ObjectPath, "compbas", ".csv");
        const int32_t loadResult =
            InitMasterComponentListExcel(componentName.data(), 0xff, MaxVisualRange / MaxWeaponRange, BaseSensorRange);
        // Faithful: the assert reports the previous read's code.
        Assert(loadResult == 0, static_cast<uint32_t>(result), " Could not load compBas.csv ");
    }

    result = gameSystemFile->ReadIdUChar("AlwaysRevealed", AlwaysRevealed);
    RequireOk(result, " Could not find AlwaysRevealed in GameSys ");

    if (gameSystemFile->ReadIdUChar("GodMode", GodMode) != 0)
    {
        GodMode = 0;
    }

    if (gameSystemFile->ReadIdUChar("AlwaysDraw", ForceAlways) != 0)
    {
        ForceAlways = 0;
    }

    uint8_t revealTacMap = 0;

    if (gameSystemFile->ReadIdUChar("RevealTacMap", revealTacMap) != 0)
    {
        revealTacMap = 0;
    }

    DrawRevealedTacMap = revealTacMap;

    if (gameSystemFile->ReadIdUChar("FootPrints", FootPrints) != 0)
    {
        FootPrints = 1;
    }

    result = gameSystemFile->ReadIdLong("BonusTonnageDivisor", TonnageDivisor);
    RequireOk(result, " No Tonnage divisor in GameSys ");
    result = gameSystemFile->ReadIdLong("BonusPointsPerTon", ResourcesPerTonDivided);
    RequireOk(result, " No Bonus points per Ton in GameSys ");

    InitDifficultySettings(gameSystemFile);
    result = LoadMoverGameSystem(gameSystemFile, MaxVisualRange);
    RequireOk(result, " could not load Mover System in GameSys ");
    result = LoadMultiplayerGameSystem(gameSystemFile);
    RequireOk(result, " could not load Multiplayer System in GameSys ");
    result = LoadMechGameSystem(gameSystemFile);
    RequireOk(result, " could not load Mech System in GameSys ");

    if (GameDifficulty == 0)
    {
        MechSalvageChance = GlobalSalvageModifier[0];
    }
    else if (GameDifficulty == 2)
    {
        MechSalvageChance = GlobalSalvageModifier[1];
    }

    result = LoadGroundVehicleGameSystem(gameSystemFile);
    RequireOk(result, " could not load Ground Vehicle System in GameSys ");
    result = LoadElementalGameSystem(gameSystemFile);
    RequireOk(result, " could not load Elemental System in GameSys ");

    result = gameSystemFile->SeekBlock("Mine");
    RequireOk(result, " Could not find Mine Block in GameSys ");
    result = gameSystemFile->ReadIdFloat("BaseDamage", MineBaseDamage);
    RequireOk(result, " Could not find Damage variable in Mine Block in GameSys ");
    result = gameSystemFile->ReadIdFloat("SplashDamage", MineSplashDamage);
    RequireOk(result, " Could not find Splash Damage variable in Mine Block in GameSys ");
    result = gameSystemFile->ReadIdFloat("SplashRange", MineSplashRange);
    RequireOk(result, " Could not find Splash Range variable in Mine Block in GameSys ");
    result = gameSystemFile->ReadIdLong("Explosion", MineExplosion);
    RequireOk(result, " Could not find Explosion variable in Mine Block in GameSys ");

    if (gameSystemFile->ReadIdLong("MineLayThrottle", MineLayThrottle) != 0)
    {
        MineLayThrottle = 50;
    }

    if (gameSystemFile->ReadIdLong("MineSweepThrottle", MineSweepThrottle) != 0)
    {
        MineSweepThrottle = 50;
    }

    result = gameSystemFile->ReadIdFloat("MineWaitTime", MineWaitTime);
    RequireOk(result, " Could not find mine Wait time in Mine Block ");

    result = gameSystemFile->SeekBlock("Smoke");
    RequireOk(result, " Could not find Smoke Block in GameSys ");
    result = gameSystemFile->ReadIdLong("MaxSmokeSpheres", TotalSmokeSpheres);
    RequireOk(result, " Could not find total Smoke Count in GameSys ");
    result = gameSystemFile->ReadIdLong("TotalSmokeShapeSize", TotalSmokeShapeSize);
    RequireOk(result, " Could not find total Smoke Shape Size in GameSys ");

    result = gameSystemFile->SeekBlock("Fire");
    RequireOk(result, " Could not find Fire Block in GameSys ");
    result = gameSystemFile->ReadIdLong("MaxFiresBurning", MaxFiresBurning);
    RequireOk(result, " COuld not find max fires burning in gameSys ");
    result = gameSystemFile->ReadIdFloat("MaxFireBurnTime", MaxFireBurnTime);
    RequireOk(result, " COuld not find max fire burn time in gameSys ");
    UpdateDisplay(0, 1, 30, 1, 5);

    //---------------------------------------------------------------------------------------------------------------
    // The scenario file (from the missions folder, or a saved game's copy).
    std::string scenarioFileName;
    scenarioFileName = GamePath(MissionPath, scenarioName, ".fit");
    ScenarioFile = new MCFitIniFile;
    Assert(ScenarioFile != nullptr, 0, " no RAM for scenario file ");
    result = ScenarioFile->Open(scenarioFileName);

    if (result != 0)
    {
        scenarioFileName = GamePath(SaveTempPath, scenarioName, ".fit");
        result = ScenarioFile->Open(scenarioFileName);
        RequireOk(result, " could not open scenario file ");
    }

    result = ScenarioFile->SeekBlock("Planet");

    if (result == 0)
    {
        ScenarioFile->ReadIdLong("Setting", CurPlanet);
        RequireOk(result, " could not find Setting in Planet Block ");
    }

    if (CurPlanet == 1)
    {
        // On this planet overlay types 1-15 cost nothing to cross, at every move level.
        for (int32_t level = 0; level < NUM_MOVE_LEVELS; level++)
        {
            for (int32_t i = 0; i < 15 * MAPCELL_DIM * MAPCELL_DIM; i++)
            {
                OverlayWeightTable[level * OVERLAY_WEIGHT_LEVEL_SIZE + MAPCELL_DIM * MAPCELL_DIM + i] = 0;
            }
        }
    }

    result = ScenarioFile->SeekBlock("PaletteSystem");
    RequireOk(result, " could not find PaletteSystem Block ");
    result = ScenarioFile->ReadIdString("PaletteSystem", PaletteSystem, 79);
    RequireOk(result, " could not find PaletteSystem in PaletteSystem Block ");
    {
        // The mission's palette is shown until the scenario goes; the interface's comes back then.
        std::expected<std::unique_ptr<MCPalette>, std::string> palette = MCPalette::Create(PaletteSystem);

        if (!palette)
        {
            Fatal(0, std::format(" could not start gamePalette: {} ", palette.error()));
        }

        OldPalette = MCGameContext::Current().SetPalette(std::move(*palette));
    }

    InitAlphaLookup(GamePalette()->Colors());
    Application->ActivatePalette(GamePalette()->RgbData.data(), 10, 0xf6);
    UpdateDisplay(0, 1, 20, 1, 7);

    //---------------------------------------------------------------------------------------------------------------
    // Teams and commanders.
    result = ScenarioFile->SeekBlock("Teams");
    RequireOk(result, "Could not find Teams Block");
    int haveAlliedTeam = 0;
    result = ScenarioFile->ReadIdBoolean("AlliedTeam", haveAlliedTeam);
    RequireOk(result, " Could not find AlliedTeam in Teams Block ");

    if (ClanTeam != nullptr)
    {
        DeleteTeam(ClanTeam);
    }

    ClanTeam = NewTeam();
    ClanTeam->Alignment = -1;
    ClanTeam->Init(1, 0x80);
    TeamTable[1] = ClanTeam;

    if (AlliedTeam != nullptr)
    {
        DeleteTeam(AlliedTeam);
        AlliedTeam = nullptr;
    }

    if (haveAlliedTeam != 0)
    {
        AlliedTeam = NewTeam();
        AlliedTeam->Alignment = 1;
        AlliedTeam->Init(2, 0x80);
        TeamTable[2] = AlliedTeam;
    }

    if (InnerSphereTeam != nullptr)
    {
        DeleteTeam(InnerSphereTeam);
    }

    InnerSphereTeam = NewTeam();
    InnerSphereTeam->Alignment = 1;
    InnerSphereTeam->Init(0, 0x80);
    TeamTable[0] = InnerSphereTeam;
    UpdateDisplay(0, 1, 30, 1, 10);

    result = ScenarioFile->SeekBlock("Artillery");
    RequireOk(result, " could not find Artillery block in Scenario File ");
    NumCameraStrikes = 0;
    NumSensorStrikes = 0;
    NumSmallStrikes = 0;
    NumLargeStrikes = 0;
    ScenarioFile->ReadIdLong("NumLargeStrikes", NumLargeStrikes);
    ScenarioFile->ReadIdLong("NumSmallStrikes", NumSmallStrikes);
    ScenarioFile->ReadIdLong("NumSensorStrikes", NumSensorStrikes);
    ScenarioFile->ReadIdLong("NumCameraStrikes", NumCameraStrikes);

    if (MPlayer == nullptr)
    {
        HomeTeam = InnerSphereTeam;
        NumCommanders = (AlliedTeam == nullptr) ? 2 : 3;

        for (int32_t i = 0; i < NumCommanders; i++)
        {
            auto* commander = new MCCommander;

            if (commander != nullptr)
            {
                commander->Init();
            }

            CommanderTable[i] = commander;
            CommanderTable[i]->SetId(i);
        }

        HomeCommander = CommanderTable[0];
        CommanderTable[0]->SetNumSmallStrikes(NumSmallStrikes);
        HomeCommander->SetNumLargeStrikes(NumLargeStrikes);
        HomeCommander->SetNumSensorStrikes(NumSensorStrikes);
        HomeCommander->SetNumCameraDrones(NumCameraStrikes);
        CommanderTable[1]->SetNumSmallStrikes(999);
        CommanderTable[1]->SetNumLargeStrikes(999);
        CommanderTable[1]->SetNumSensorStrikes(999);
        CommanderTable[1]->SetNumCameraDrones(999);
    }
    else
    {
        NumCommanders = MAX_COMMANDERS;

        for (int32_t i = 0; i < NumCommanders; i++)
        {
            auto* commander = new MCCommander;

            if (commander != nullptr)
            {
                commander->Init();
            }

            CommanderTable[i] = commander;
            CommanderTable[i]->SetId(i);
        }

        if (MPlayer->HomeTeam == 0)
        {
            HomeTeam = InnerSphereTeam;
        }
        else if (MPlayer->HomeTeam == 1)
        {
            HomeTeam = ClanTeam;
        }
        else
        {
            Fatal(0, " Must Be Clan or InnerSphere in Multiplayer! ");
        }

        HomeCommander = CommanderTable[MPlayer->CheckInId];
    }

    UpdateDisplay(0, 1, 30, 1, 13);

    //---------------------------------------------------------------------------------------------------------------
    // Music, scale and the element (draw list) system.
    result = ScenarioFile->SeekBlock("Music");
    RequireOk(result, " could not find Music block in Scenario File ");
    result = ScenarioFile->ReadIdUChar("scenarioTuneNum", ScenarioTuneNum);
    RequireOk(result, " could not find ScenarioTuneNum in Music block in Scenario File ");

    result = ScenarioFile->SeekBlock("GameScale");
    RequireOk(result, " could not find GameScale block in Scenario File ");
    result = ScenarioFile->ReadIdFloat("WorldUnitsPerMeter", WorldUnitsPerMeter);
    RequireOk(result, " could not find worldUnitsperMeter in GameScale block in Scenario File ");
    result = ScenarioFile->ReadIdFloat("MetersPerWorldUnit", MetersPerWorldUnit);
    RequireOk(result, " could not find MetersperWorldUnit in GameScale block in Scenario File ");
    result = ScenarioFile->ReadIdULong("Duration", Duration);
    RequireOk(result, " could not find Duration in GameScale block in Scenario File ");
    result = ScenarioFile->ReadIdFloat("CycleLength", CycleLength);
    RequireOk(result, " could not find CycleLength in GameScale block in Scenario File ");
    uint32_t singleStep = 0;
    result = ScenarioFile->ReadIdULong("SingleStep", singleStep);
    SingleStepMode = static_cast<int>(singleStep);
    int32_t scenarioVisualRanges[256];
    result = ScenarioFile->ReadIdLongArray("VisualRangeTable", scenarioVisualRanges, 256);

    if (result == 0)
    {
        std::memcpy(VisualRangeTable, scenarioVisualRanges, sizeof(VisualRangeTable));
    }

    // The frame's draw list. The ElementSystem block's ElementHeapSize, MaxElements and MaxGroups sized the original's;
    // the port's grows.
    MCGameContext::Current().SetElementList(std::make_unique<MCElementBuffer>());
    UpdateDisplay(0, 1, 30, 1, 15);

    //---------------------------------------------------------------------------------------------------------------
    // The sensor contact blips.
    result = ScenarioFile->SeekBlock("SensorContactShape");
    RequireOk(result, " could not find SensorContactShape block in Scenario File ");
    char sensorShapeName[80];
    result = ScenarioFile->ReadIdString("shapeName", sensorShapeName, 79);
    RequireOk(result, " could not find ShapeName in SensorContactShape block in Scenario File ");
    std::string sensorShapeFileName;
    sensorShapeFileName = GamePath(SpritePath, sensorShapeName, ".pak");
    MCPacketFile sensorShapeFile;
    result = sensorShapeFile.Open(sensorShapeFileName);

    if (result != 0)
    {
        std::string cdFileName;
        cdFileName = GamePath(CDspritePath, sensorShapeName, ".pak");
        result = sensorShapeFile.Open(cdFileName);
        RequireOk(result, " could not open sensor shape file ");
    }

    for (int32_t i = 0; i < 6; i++)
    {
        sensorShapeFile.SeekPacket(i);
        // An empty packet still fails, as it did when systemHeap's malloc(0) returned null.
        Assert(sensorShapeFile.GetPacketSize() > 0, static_cast<uint32_t>(result), " no RAM for Large Sensor Shape ");
        SensorContactShapes[i] = new uint8_t[static_cast<size_t>(sensorShapeFile.GetPacketSize())]{};
        sensorShapeFile.ReadPacket(i, SensorContactShapes[i]);
        MCRenderer::RegisterData(SensorContactShapes[i], static_cast<size_t>(sensorShapeFile.GetPacketSize()),
                                 MCDataKind::Shapes);
    }

    sensorShapeFile.Close();

    //---------------------------------------------------------------------------------------------------------------
    // Craters, cameras, objects, sprites, appearances, sensors and contacts.
    // (The block's CraterShapeSize sized the original's shape heap.)
    int32_t numCraters = 0;
    result = ScenarioFile->SeekBlock("CraterSystem");
    RequireOk(result, " could not find CraterSystem Block in Scenario File ");
    result = ScenarioFile->ReadIdLong("NumCraters", numCraters);
    RequireOk(result, " could not find NumCraters in CraterSystem Block in Scenario File ");
    char craterFileName[16];
    result = ScenarioFile->ReadIdString("CraterFile", craterFileName, 15);
    RequireOk(result, " could not find CraterFile in CraterSystem Block in Scenario File ");

    {
        std::expected<std::unique_ptr<MCCraterManager>, std::string> craters =
            MCCraterManager::Create(numCraters, craterFileName);

        if (!craters)
        {
            Fatal(0, std::format(" could not Start CraterManager: {} ", craters.error()));
        }

        MCGameContext::Current().SetCraterManager(std::move(*craters));
    }

    result = ScenarioFile->SeekBlock("CameraSystem");
    RequireOk(result, " could not Find CameraSystem Block ");
    result = ScenarioFile->ReadIdULong("CameraHeapSize", CameraHeapSize);
    RequireOk(result, " could not Find CameraHeapSize in CameraSystem Block ");
    result = ScenarioFile->ReadIdString("CameraFileName", CameraFileName, 79);
    RequireOk(result, " could not Find CameraFileName in CameraSystem Block ");
    CameraList = new MCCameraList;
    Assert(CameraList != nullptr, static_cast<uint32_t>(result), " no RAM for CameraList ");
    result = CameraList->Init(CameraFileName);
    RequireOk(result, " could start CameraSystem ");
    UpdateDisplay(0, 1, 30, 1, 20);

    result = ScenarioFile->SeekBlock("ObjectSystem");
    RequireOk(result, " could not Find ObjectSystem Block ");
    result = ScenarioFile->ReadIdULong("ObjectHeapSize", ObjectHeapSize);
    RequireOk(result, " could not Find objectHeapSize in ObjectSystem Block ");
    result = ScenarioFile->ReadIdULong("ObjectTypeHeapSize", ObjectTypeHeapSize);
    RequireOk(result, " could not Find ObjectTypeHeapSzize in ObjectSystem Block ");
    result = ScenarioFile->ReadIdULong("NumObjects", NumObjects);
    RequireOk(result, " could not Find NumObjects in ObjectSystem Block ");
    result = ScenarioFile->ReadIdString("ObjectFileName", ObjectFileName, 79);
    RequireOk(result, " could not Find ObjectFileName in ObjectSystem Block ");
    result = StartObjects(ObjectFileName, static_cast<int32_t>(ObjectTypeHeapSize),
                          static_cast<int32_t>(ObjectHeapSize), static_cast<int32_t>(NumObjects));
    RequireOk(result, " could not Start ObjectSystem ");

    result = ScenarioFile->SeekBlock("SpriteSystem");
    RequireOk(result, " could not Find SpriteSystem Block ");
    result = ScenarioFile->ReadIdULong("SpriteHeapSize", SpriteHeapSize);
    RequireOk(result, " could not Find SpriteHeapSize in SpriteSystem Block ");
    uint32_t spriteManagerHeapSize = 0;
    result = ScenarioFile->ReadIdULong("SpriteManagerHeapSize", spriteManagerHeapSize);
    RequireOk(result, " could not Find SpriteManagerHeapSize in SpriteSystem Block ");
    uint32_t spriteDataHeapSize = 0;
    result = ScenarioFile->ReadIdULong("SpriteDataHeapSize", spriteDataHeapSize);
    RequireOk(result, " could not Find SpriteDataHeapSize in SpriteSystem Block ");
    result = ScenarioFile->ReadIdString("SpriteFileName", SpriteFileName, 79);
    RequireOk(result, " could not Find SpriteFileName in SpriteSystem Block ");
    char shapeFileName[80];
    result = ScenarioFile->ReadIdString("ShapeFileName", shapeFileName, 79);
    RequireOk(result, " could not Find ShapeFileName in SpriteSystem Block ");

    uint32_t legHeapSize = 0;
    uint32_t torsoHeapSize = 0;
    uint32_t rightArmHeapSize = 0;
    uint32_t leftArmHeapSize = 0;
    uint32_t totalMechs = 0;
    result = ScenarioFile->SeekBlock("SpriteManager");
    RequireOk(result, " could not Find SpriteManager Block ");
    result = ScenarioFile->ReadIdULong("LegHeapSize", legHeapSize);
    RequireOk(result, " could not Find LegHeapSize in SpriteManager Block ");
    result = ScenarioFile->ReadIdULong("TorsoHeapSize", torsoHeapSize);
    RequireOk(result, " could not Find TorsoHeapSize in SpriteManager Block ");
    result = ScenarioFile->ReadIdULong("RightArmHeapSize", rightArmHeapSize);
    RequireOk(result, " could not Find RightArmHeapSize in SpriteManager Block ");
    result = ScenarioFile->ReadIdULong("LeftArmHeapSize", leftArmHeapSize);
    RequireOk(result, " could not Find LeftArmHeapSize in SpriteManager Block ");
    result = ScenarioFile->ReadIdULong("TotalMechs", totalMechs);
    RequireOk(result, " could not Find TotalMechs in SpriteManager Block ");

    // The original sized the sprite manager's heaps from the sizes above (still read, then ignored).
    std::expected<std::unique_ptr<MCSpriteManager>, std::string> spriteManager =
        MCSpriteManager::Create(shapeFileName, Use90PixelSprite != 0);

    if (!spriteManager.has_value())
    {
        Fatal(0, std::format(" could not Start SpriteManager: {} ", spriteManager.error()));
    }

    MCGameContext::Current().SetSpriteManager(std::move(*spriteManager));

    // The original went on without the sprite PAK (it tested the list, not the result); the port stops.
    std::expected<std::unique_ptr<MCAppearanceTypeList>, std::string> typeList =
        MCAppearanceTypeList::Create(SpriteFileName);

    if (!typeList.has_value())
    {
        Fatal(0, std::format(" could not start AppearanceList: {} ", typeList.error()));
    }

    MCGameContext::Current().SetAppearanceTypeList(std::move(*typeList));

    SensorSystemManager = new MCSensorSystemManager;
    Assert(SensorSystemManager != nullptr, 0, " Unable to init sensor system manager ");
    result = SensorSystemManager->Init(gameSystemFile);
    RequireOk(result, " could not start Sensor System Manager ");

    PotentialContactManager = new MCPotentialContactManager;
    result = PotentialContactManager->Init(ScenarioFile);
    RequireOk(result, " could not start PotentialContactManager ");
    UpdateDisplay(0, 1, 20, 1, 25);

    SmokeManager = new MCSmokeManager;
    result = SmokeManager->Init(ScenarioFile);

    if (result != 0)
    {
        return result;
    }

    UpdateDisplay(0, 1, 30, 1, 35);

    CollisionSystem = new MCCollisionSystem;

    if (CollisionSystem == nullptr)
    {
        Assert(0, static_cast<uint32_t>(result), " no RAM for Collision System ");
    }

    result = CollisionSystem->Init(ScenarioFile);
    RequireOk(result, " could not start Collision System ");
    UpdateDisplay(0, 1, 30, 1, 37);

    //---------------------------------------------------------------------------------------------------------------
    // The terrain and its move maps (the editor passes a terrain of its own and gets no maps).
    if (terrainName == nullptr)
    {
        result = ScenarioFile->SeekBlock("TerrainSystem");
        RequireOk(result, " could not find TerrainSystem block ");
        result = ScenarioFile->ReadIdString("TerrainFileName", TerrainFileName, 79);
        RequireOk(result, " could not find TerrainFileName in TerrainSystem block ");
        Land = new MCTerrain;

        if (Land != nullptr)
        {
            Land->MCTerrain::Init();
        }

        Assert(Land != nullptr, static_cast<uint32_t>(result), " no RAM for Terrain ");
        result = Land->Init(TerrainFileName);
        RequireOk(result, " could not start Terrain System ");
        UpdateDisplay(0, 1, 30, 1, 50);

        GameMap = new MCScenarioMap;

        if (GameMap == nullptr)
        {
            Assert(0, static_cast<uint32_t>(result), " no RAM for Game Map ");
        }

        std::string mapFileName;
        mapFileName = GamePath(TerrainPath, TerrainFileName, ".dat");
        auto* mapFile = new MCFile;
        Assert(mapFile != nullptr, static_cast<uint32_t>(result), " no RAM for Map File");
        result = mapFile->Open(mapFileName);
        RequireOk(result, " could not start Game Map ");
        GameMap->Init(mapFile);
        mapFile->Close();
        delete mapFile;
        UpdateDisplay(0, 1, 30, 1, 60);

        GameObjectMap = new MCObjectMap;
        Assert(GameObjectMap != nullptr, static_cast<uint32_t>(result), " no RAM for Game Object Map ");
        GameObjectMap->Init(GameMap);
        PathManager = new MCMovePathManager;

        if (PathManager != nullptr)
        {
            PathManager->Init();
        }

        PathFindMap = new MCMoveMap;
        Assert(PathFindMap != nullptr, static_cast<uint32_t>(result), " no RAM for Path Find Map ");
        PathFindMap->Init(SimpleMovePathRange * 2 + 1, SimpleMovePathRange * 2 + 1);
        GlobalMoveMap = new MCGlobalMap;

        auto* globalMapFile = new MCFile;
        std::string globalMapFileName;
        globalMapFileName = GamePath(TerrainPath, TerrainFileName, ".gmm");
        result = globalMapFile->Open(globalMapFileName);
        RequireOk(result, " Could not open global Map ");
        GlobalMoveMap->Init(globalMapFile);
        delete globalMapFile;
        UpdateDisplay(0, 1, 30, 1, 65);
        Land->UpdateAllObjects();
        UpdateDisplay(0, 1, 30, 1, 70);
    }
    else
    {
        Land = new MCTerrain;

        if (Land != nullptr)
        {
            Land->MCTerrain::Init();
        }

        Assert(Land != nullptr, static_cast<uint32_t>(result), " no RAM for Terrain ");
        MCStrCopy(TerrainFileName, std::filesystem::path(terrainName).stem().string().c_str());
        result = Land->Init(TerrainFileName);
        RequireOk(result, " could not start Terrain System ");
    }

    char tacMapGifName[80];
    result = ScenarioFile->ReadIdString("TacMapGifName", tacMapGifName, 79);

    if (result == 0)
    {
        MCTerrain::TerrainTacticalMap->SetRevealedBitmap(tacMapGifName);
    }

    //---------------------------------------------------------------------------------------------------------------
    // ABL: the libraries, then the scenario's own brain.
    result = ScenarioFile->SeekBlock("ABLibraries");

    if (result == 0)
    {
        int32_t libraryNumber = 0;

        while (result == 0)
        {
            char libraryId[32];
            char libraryName[512];
            std::snprintf(libraryId, sizeof(libraryId), "Library%d", libraryNumber++);
            result = ScenarioFile->ReadIdString(libraryId, libraryName, 511);

            if (result == 0)
            {
                std::string libraryFileName;
                libraryFileName = GamePath(MissionPath, libraryName, ".abx");
                int32_t numErrors = 0;
                int32_t numLines = 0;

                if (AblLoadLibrary(libraryFileName.data(), &numErrors, &numLines, nullptr, 0) != 0)
                {
                    char message[512];
                    std::snprintf(message, sizeof(message), " Cannot load ABL Library %s ", libraryName);
                    Fatal(0, message);
                }
            }
        }
    }

    UpdateDisplay(0, 1, 30, 1, 73);

    result = ScenarioFile->SeekBlock("Script");
    RequireOk(result, " could not find Script Block ");
    result = ScenarioFile->ReadIdString("ScenarioScript", ScenarioScript, 79);
    RequireOk(result, " could not find ScenarioScript in Script Block ");
    static char windowTitle[256];
    std::snprintf(windowTitle, sizeof(windowTitle), "%s - %s", AppName, ScenarioScript);

    // Port: SetWindowTextA -> the SDL window's title.
    if (MCDisplay* display = MCInput::Display())
    {
        display->SetTitle(windowTitle);
    }

    std::strcpy(WindowTitle, windowTitle);
    UpdateDisplay(0, 1, 30, 1, 75);

    std::string scriptFileName;
    scriptFileName = GamePath(MissionPath, ScenarioScript, ".abl");
    int32_t numErrors = 0;
    int32_t numLines = 0;
    ScenarioScriptHandle = AblPreProcess(scriptFileName.data(), &numErrors, &numLines, nullptr, 0);
    Assert(-1 < ScenarioScriptHandle, static_cast<uint32_t>(ScenarioScriptHandle), " Bad Scenario Script ");
    ScenarioBrain = new MCAblModule;

    if (ScenarioBrain == nullptr)
    {
        return static_cast<int32_t>(0xfaaf000b);
    }

    const int32_t brainResult = ScenarioBrain->Init(ScenarioScriptHandle);
    Assert(brainResult == 0, static_cast<uint32_t>(result), " Error Starting Scenario Brain ");
    ScenarioBrain->SetName(const_cast<char*>("Scenario"));
    ScenarioBrain->Step = 1;
    ScenarioBrainParams = new MCAblParam;
    Assert(ScenarioBrainParams != nullptr, 0, " No RAM for Scenario Brain Parameters ");
    ScenarioBrainHandleMessage = ScenarioBrain->FindFunction(const_cast<char*>("handlemessage"), 1);

    //---------------------------------------------------------------------------------------------------------------
    // The warriors.
    result = ScenarioFile->SeekBlock("Warriors");
    Assert(result == 0, 0, " Could not find Warriors Block ");
    result = ScenarioFile->ReadIdUChar("CaptureChance", CaptureChance);

    if (result != 0 || 4 < CaptureChance)
    {
        CaptureChance = 2;
    }

    result = ScenarioFile->ReadIdULong("NumWarriors", NumWarriors);
    RequireOk(result, " Could not find NumWarriors in Warriors Block ");
    char brainParameterFileName[1024];
    result = ScenarioFile->ReadIdString("BrainParameterFile", brainParameterFileName, 1023);
    const bool haveBrainParameters = (result == 0);
    result = ScenarioFile->ReadIdUChar("CaptureChance", CaptureChance);

    if (result != 0 || 4 < CaptureChance)
    {
        CaptureChance = 2;
    }

    UpdateDisplay(0, 1, 30, 1, 77);

    NumMarines = 0;

    if (NumWarriors != 0)
    {
        Warriors = std::make_unique<MCMechWarrior*[]>(NumWarriors + 1);

        for (uint32_t i = 1; i < NumWarriors + 1; i++)
        {
            char blockName[32];
            std::snprintf(blockName, sizeof(blockName), "Warrior%d", i);
            result = ScenarioFile->SeekBlock(blockName);
            Assert(result == 0, i, " Could not find Warrior Number Block ");
            char profileName[100];
            result = ScenarioFile->ReadIdString("Profile", profileName, 99);
            Assert(result == 0, 0, " Could not find Warrior Profile in Warrior Number Block ");
            Warriors[i] = NewWarrior();
            Assert(Warriors[i] != nullptr, 0, " No RAM for Warrior ");

            std::string profileFileName;
            profileFileName = GamePath(WarriorPath, profileName, ".fit");
            auto* profileFile = new MCFitIniFile;
            Assert(profileFile != nullptr, 0, " No RAM for Warrior Profile File ");
            int32_t profileResult = profileFile->Open(profileFileName);

            if (profileResult == 0)
            {
                profileResult = Warriors[i]->Init(profileFile);
                Assert(profileResult == 0, static_cast<uint32_t>(profileResult), " Could not load Warrior Profile ");
            }
            else
            {
                // A saved game keeps its warriors' profiles in the temporary save folder.
                MCFitIniFile savedProfileFile;
                std::string savedProfileName;
                savedProfileName = GamePath(SaveTempPath, profileName, ".fit");
                profileResult = savedProfileFile.Open(savedProfileName);
                Assert(profileResult == 0, static_cast<uint32_t>(profileResult),
                       " Could not open Warrior Profile File ");
                profileResult = Warriors[i]->Init(&savedProfileFile);
                Assert(profileResult == 0, static_cast<uint32_t>(profileResult), " Could not load Warrior Profile ");
            }

            profileFile->Close();
            delete profileFile;

            Warriors[i]->Index = static_cast<int32_t>(i);
            char brainName[128];
            profileResult = ScenarioFile->ReadIdString("Brain", brainName, 127);
            Assert(profileResult == 0, static_cast<uint32_t>(profileResult),
                   " Could not find Warrior Brain in Warrior Number Block ");
            Warriors[i]->SetBrainName(brainName);
            std::string brainFileName;
            brainFileName = GamePath(WarriorPath, brainName, ".abl");
            int32_t brainErrors = 0;
            int32_t brainLines = 0;
            const int32_t brainHandle = AblPreProcess(brainFileName.data(), &brainErrors, &brainLines, nullptr, 0);
            Assert(-1 < brainHandle, static_cast<uint32_t>(brainHandle), " Could not start Warrior Brain ");
            const int32_t setBrainResult = Warriors[i]->SetBrain(brainHandle);
            Assert(setBrainResult == 0, static_cast<uint32_t>(setBrainResult), " Could Not Set Brain ");
            int notMineYet = 0;

            if (ScenarioFile->ReadIdBoolean("NotMineYet", notMineYet) != 0)
            {
                notMineYet = 0;
            }

            Warriors[i]->NotMineYet = notMineYet;
        }
    }

    if (haveBrainParameters)
    {
        std::string parameterFileName;
        parameterFileName = GamePath(WarriorPath, brainParameterFileName, ".fit");
        auto* parameterFile = new MCFitIniFile;
        Assert(parameterFile != nullptr, 0, " No RAM for Brain Parameter File ");
        const int32_t openResult = parameterFile->Open(parameterFileName);
        Assert(openResult == 0, static_cast<uint32_t>(openResult), " Could not open Brain Parameter File ");

        for (uint32_t i = 1; i <= NumWarriors; i++)
        {
            Warriors[i]->LoadBrainParameters(parameterFile, static_cast<int32_t>(i));
        }

        parameterFile->Close();
        delete parameterFile;
    }

    UpdateDisplay(0, 1, 30, 1, 80);

    //---------------------------------------------------------------------------------------------------------------
    // The parts.
    result = ScenarioFile->SeekBlock("Parts");
    RequireOk(result, " Could not find Parts Block ");
    result = ScenarioFile->ReadIdULong("NumParts", NumParts);
    RequireOk(result, " Could not find NumParts in Parts Block ");

    for (MCBaseObject*& mover : MoverRoster)
    {
        mover = nullptr;
    }

    if (NumParts != 0)
    {
        Parts = std::make_unique<MCPart[]>(NumParts + 1);

        for (int32_t i = 1; i < static_cast<int32_t>(NumParts) + 1; i++)
        {
            UpdateDisplay(0, 1, 30, 1,
                          static_cast<int32_t>(static_cast<double>(i) / static_cast<int32_t>(NumParts) * 10.0 + 80.0));
            MCPart& part = Parts[i];
            char blockName[32];
            std::snprintf(blockName, sizeof(blockName), "Part%d", i);
            result = ScenarioFile->SeekBlock(blockName);
            RequireOk(result, " Could not find PartNumber Block ");
            result = ScenarioFile->ReadIdULong("ObjectNumber", part.ObjNumber);
            RequireOk(result, " Could not find ObjectNumber in PartNumber Block ");
            result = ScenarioFile->ReadIdULong("ControlType", part.ControlType);
            RequireOk(result, " Could not find ControlType in PartNumber Block ");
            result = ScenarioFile->ReadIdULong("ControlDataType", part.ControlDataType);
            RequireOk(result, " Could not find ControlDataType in PartNumber Block ");
            result = ScenarioFile->ReadIdString("ObjectProfile", part.ProfileName, 9);
            RequireOk(result, " Could not find ObjectProfile in PartNumber Block ");
            result = ScenarioFile->ReadIdULong("Pilot", part.Pilot);
            RequireOk(result, " Could not find Pilot in PartNumber Block ");
            result = ScenarioFile->ReadIdFloat("PositionX", part.Position[0]);
            RequireOk(result, " Could not find PositionX in PartNumber Block ");
            result = ScenarioFile->ReadIdFloat("PositionY", part.Position[1]);
            RequireOk(result, " Could not find PositionY in PartNumber Block ");
            result = ScenarioFile->ReadIdFloat("PositionZ", part.Position[2]);
            RequireOk(result, " Could not find PositionZ in PartNumber Block ");
            result = ScenarioFile->ReadIdFloat("Rotation", part.Rotation);
            RequireOk(result, " Could not find Rotation in PartNumber Block ");
            char teamId = 0;
            result = ScenarioFile->ReadIdChar("TeamId", teamId);
            part.TeamId = static_cast<int8_t>(teamId);
            RequireOk(result, " Could not find TeamId in PartNumber Block ");

            if (part.TeamId == 0 || part.TeamId == 2)
            {
                part.Alignment = 1;
            }
            else if (part.TeamId == 1)
            {
                part.Alignment = -1;
            }
            else
            {
                Fatal(0, " Bad TeamId for Part ");
            }

            char commanderId = 0;
            result = ScenarioFile->ReadIdChar("CommanderId", commanderId);

            if (result == 0)
            {
                part.CommanderId = commanderId;
                result = 0;
            }
            else
            {
                result = ScenarioFile->ReadIdLong("CommanderId", part.CommanderId);
                RequireOk(result, " Could not find CommanderId in PartNumber Block ");
            }

            result = ScenarioFile->ReadIdULong("Gesture", part.GestureId);
            RequireOk(result, " Could not find Gesture in PartNumber Block ");

            if (ScenarioFile->ReadIdLong("PaintScheme", part.PaintScheme) != 0)
            {
                part.PaintScheme = -1;
            }

            result = ScenarioFile->ReadIdFloat("Velocity", part.Velocity);
            RequireOk(result, " Could not find Velocity in PartNumber Block ");
            result = ScenarioFile->ReadIdLong("Active", part.Active);
            RequireOk(result, " Could not find Active Flag in PartNumber Block ");
            result = ScenarioFile->ReadIdLong("Exists", part.Exists);
            RequireOk(result, " Could not find Exists Flag in PartNumber Block ");
            // Read twice in the original.
            result = ScenarioFile->ReadIdChar("MyIcon", part.MyIcon);
            RequireOk(result, " Could not find MyIcon in PartNumber Block ");
            result = ScenarioFile->ReadIdChar("MyIcon", part.MyIcon);
            RequireOk(result, " Could not find MyIcon in PartNumber Block ");
            int captureable = 0;
            result = ScenarioFile->ReadIdBoolean("Captureable", captureable);
            part.Captureable = (result != 0) ? 0 : captureable;

            PartCreateTime = -1.0f;
            InfluenceTime = 0.0f;
            CreatePartObject(i);
        }
    }

    UpdateDisplay(0, 1, 20, 1, 90);

    //---------------------------------------------------------------------------------------------------------------
    // Trains, elemental carriers and buses: parts that carry other parts.
    TrainManager = nullptr;
    result = ScenarioFile->SeekBlock("Trains");

    if (result == 0)
    {
        int32_t numTrains = 0;
        TrainManager = new MCTrainManager;

        if (TrainManager != nullptr)
        {
            TrainManager->Init();
        }

        Assert(TrainManager != nullptr, 0, "Couldn't create manager");
        result = ScenarioFile->ReadIdLong("NumTrains", numTrains);
        RequireOk(result, " Could not find number of trains");

        for (int32_t trainNumber = 0; trainNumber < numTrains; trainNumber++)
        {
            char blockName[32];
            std::snprintf(blockName, sizeof(blockName), "Train%d", trainNumber);
            result = ScenarioFile->SeekBlock(blockName);
            RequireOk(result, " Could not find train block");
            int32_t numCars = 0;
            result = ScenarioFile->ReadIdLong("NumCars", numCars);
            RequireOk(result, " Could not find number of cars in train block");
            Assert(0 < numCars, static_cast<uint32_t>(result), " Need at least one car in train...");
            MCTrain* train = TrainManager->CreateTrain();

            for (int32_t carNumber = 0; carNumber < numCars; carNumber++)
            {
                std::snprintf(blockName, sizeof(blockName), "Car%d", carNumber);
                int32_t carPart = 0;
                result = ScenarioFile->ReadIdLong(blockName, carPart);
                RequireOk(result, " Could not find a car in train block");
                Assert(carPart <= static_cast<int32_t>(NumParts), 0, "Illegal part number for train car");
                auto* car = static_cast<MCTrainCar*>(Parts[carPart].Object);
                Assert(car->ObjectClass == TRAINCAR, 0, "Car in train block isn't a traincar!");
                train->AddCar(car);
                car->SetPartId(trainNumber, carNumber);

                if (carNumber == 0)
                {
                    // The lead car's part sets the train's speed (clamped to its top speed) and direction.
                    const float velocity = Parts[carPart].Velocity;

                    if (std::fabs(velocity) <= train->MaxSpeed)
                    {
                        train->DesiredSpeed = velocity;
                    }
                    else if (velocity <= 0.0f)
                    {
                        train->DesiredSpeed = -train->MaxSpeed;
                    }
                    else
                    {
                        train->DesiredSpeed = train->MaxSpeed;
                    }

                    const float rotation = Parts[carPart].Rotation;

                    if (rotation != 45.0f && rotation != -45.0f && rotation != 135.0f && rotation != -135.0f)
                    {
                        Fatal(static_cast<int32_t>(rotation), " Train Rotation Invalid.  (must be 45,-45,135,-135) ");
                    }

                    train->TrackDirection = static_cast<int32_t>(rotation);
                }
            }
        }
    }

    UpdateDisplay(0, 1, 20, 1, 92);

    result = ScenarioFile->SeekBlock("Elemental Carriers");

    if (result == 0)
    {
        int32_t numCarriers = 0;
        result = ScenarioFile->ReadIdLong("Carriers", numCarriers);
        RequireOk(result, " Could not find number of carriers");

        for (int32_t carrierNumber = 0; carrierNumber < numCarriers; carrierNumber++)
        {
            char blockName[32];
            std::snprintf(blockName, sizeof(blockName), "ECarrier%d", carrierNumber);
            result = ScenarioFile->SeekBlock(blockName);
            RequireOk(result, " Could not find carrier block");
            int32_t partNumber = 0;
            result = ScenarioFile->ReadIdLong("Carrier", partNumber);
            RequireOk(result, " Could not read carrier in carrier block");
            Assert(partNumber < static_cast<int32_t>(NumParts), static_cast<uint32_t>(partNumber),
                   "Illegal part number for elemental carrier");
            auto* carrier = static_cast<MCGroundVehicle*>(Parts[partNumber].Object);
            Assert(carrier != nullptr && carrier->ObjectClass == GROUNDVEHICLE && carrier->ElementalCarrier != 0, 0,
                   "Illegal carrier object");

            for (int32_t i = 0; i < 10; i++)
            {
                std::snprintf(blockName, sizeof(blockName), "Elemental%d", i);
                result = ScenarioFile->ReadIdLong(blockName, partNumber);

                if (result != 0)
                {
                    break;
                }

                auto* elemental = static_cast<MCElemental*>(Parts[partNumber].Object);
                Assert(elemental != nullptr && elemental->ObjectClass == ELEMENTAL, 0, "Illegal elemental object");
                carrier->Elementals[i] = elemental;
                elemental->Transport = carrier;
            }
        }
    }

    UpdateDisplay(0, 1, 20, 1, 93);

    result = ScenarioFile->SeekBlock("BusBlock");

    if (result == 0)
    {
        int32_t numBuses = 0;
        result = ScenarioFile->ReadIdLong("Buses", numBuses);
        RequireOk(result, " Could not find number of buses");

        for (int32_t busNumber = 0; busNumber < numBuses; busNumber++)
        {
            char blockName[32];
            std::snprintf(blockName, sizeof(blockName), "Bus%d", busNumber);
            result = ScenarioFile->SeekBlock(blockName);
            RequireOk(result, " Could not find bus block");
            int32_t number = 0;
            result = ScenarioFile->ReadIdLong("Bus", number);
            RequireOk(result, " Could not read carrier in carrier block");
            Assert(number <= static_cast<int32_t>(NumParts), static_cast<uint32_t>(number),
                   "Illegal part number for elemental carrier");
            auto* bus = static_cast<MCGroundVehicle*>(Parts[number].Object);
            Assert(bus != nullptr && bus->ObjectClass == GROUNDVEHICLE, 0, "Illegal bus object");

            for (int32_t seat = 0; seat < 4 && seat < static_cast<int32_t>(bus->Seats); seat++)
            {
                std::snprintf(blockName, sizeof(blockName), "Passenger%d", seat);
                result = ScenarioFile->ReadIdLong(blockName, number);

                if (result != 0)
                {
                    break;
                }

                Assert(number <= static_cast<int32_t>(NumWarriors), 0, "Illegal passenger");
                bus->Passengers[seat] = Warriors[number];
            }
        }
    }

    //---------------------------------------------------------------------------------------------------------------
    // The objectives.
    result = ScenarioFile->SeekBlock("Objectives");
    RequireOk(result, " Could not find Objective Block ");

    if (ScenarioFile->ReadIdLong("TimeLeft", TimeLimit) != 0)
    {
        TimeLimit = -1;
    }

    TwoMinuteWarningPlayed = 0;
    ThirtySecondWarningPlayed = 0;
    result = ScenarioFile->ReadIdULong("NumObjectives", NumObjectives);
    RequireOk(result, " Could not find numObjectives in Objective Block ");
    Assert(NumObjectives < 10, static_cast<uint32_t>(result), " Too Many Objectives ");

    if (MPlayer == nullptr)
    {
        InnerSphereTeam->FirstObjective = 0;
        InnerSphereTeam->NumObjectives = NumObjectives;
    }
    else
    {
        uint32_t numInnerSphereObjectives = 0;
        uint32_t numClanObjectives = 0;
        result = ScenarioFile->ReadIdULong("NumInnerSphereObjectives", numInnerSphereObjectives);
        RequireOk(result, " Could not find NumInnerSphereObjectives in Objective Block ");
        result = ScenarioFile->ReadIdULong("NumClanObjectives", numClanObjectives);
        RequireOk(result, " Could not find NumClanObjectives in Objective Block ");
        Assert(numInnerSphereObjectives + numClanObjectives == NumObjectives, static_cast<uint32_t>(result),
               " Incorrect # of objectives ");
        InnerSphereTeam->FirstObjective = 0;
        InnerSphereTeam->NumObjectives = numInnerSphereObjectives;
        ClanTeam->FirstObjective = static_cast<int32_t>(numInnerSphereObjectives);
        ClanTeam->NumObjectives = numClanObjectives;
    }

    if (NumObjectives != 0)
    {
        Objectives = std::make_unique<MCScenarioObjective[]>(MAX_OBJECTIVES);

        for (int32_t i = 0; i < static_cast<int32_t>(NumObjectives); i++)
        {
            UpdateDisplay(
                0, 1, 20, 1,
                static_cast<int32_t>(static_cast<double>(i) / static_cast<int32_t>(NumObjectives) * 5.0 + 93.0));
            MCScenarioObjective& objective = Objectives[i];
            char blockName[32];
            std::snprintf(blockName, sizeof(blockName), "Objective%d", i);
            result = ScenarioFile->SeekBlock(blockName);
            Assert(result == 0, static_cast<uint32_t>(i), " Could not find ObjectiveNumber Block ");
            result = ScenarioFile->ReadIdString("Name", objective.Name, 79);
            RequireOk(result, " Could not find Name in Objective Block ");
            result = ScenarioFile->ReadIdULong("Type", objective.Type);
            RequireOk(result, " Could not find Type in Objective Block ");
            result = ScenarioFile->ReadIdFloat("TimeLeft", objective.TimeLeft);
            RequireOk(result, " Could not find TimeLeft in Objective Block ");
            result = ScenarioFile->ReadIdULong("Status", objective.Status);
            RequireOk(result, " Could not find Status in Objective Block");

            if (ScenarioFile->ReadIdLong("Points", objective.Points) != 0)
            {
                objective.Points = 0;
            }

            if (ScenarioFile->ReadIdFloat("Radius", objective.Radius) != 0)
            {
                objective.Radius = 0.0f;
            }

            objective.Position[0] = -99.0f;
            objective.Position[1] = -99.0f;
            objective.Position[2] = -99.0f;
        }

        for (int32_t i = static_cast<int32_t>(NumObjectives); i < MAX_OBJECTIVES; i++)
        {
            Objectives[i].Type = UNUSED_OBJECTIVE;
            Objectives[i].Status = UNUSED_OBJECTIVE;
        }
    }

    //---------------------------------------------------------------------------------------------------------------
    // Each commander's support strikes and groups ("Commander%dGroup:%d": the part numbers of its mates).
    for (int32_t commanderId = 0; commanderId < NumCommanders; commanderId++)
    {
        MCCommander* commander = CommanderTable[commanderId];
        int32_t groupId = 0;
        UpdateDisplay(0, 1, 30, 1, 98);
        char blockName[64];
        std::snprintf(blockName, sizeof(blockName), "Commander:%d", commanderId);

        if (ScenarioFile->SeekBlock(blockName) == 0)
        {
            int32_t strikes = 0;
            commander->SetNumSmallStrikes(ScenarioFile->ReadIdLong("NumSmallStrikes", strikes) == 0 ? strikes : 0);
            commander->SetNumLargeStrikes(ScenarioFile->ReadIdLong("NumLargeStrikes", strikes) == 0 ? strikes : 0);
            commander->SetNumSensorStrikes(ScenarioFile->ReadIdLong("NumSensorStrikes", strikes) == 0 ? strikes : 0);
            commander->SetNumCameraDrones(ScenarioFile->ReadIdLong("NumCameraDrones", strikes) == 0 ? strikes : 0);
        }

        std::snprintf(blockName, sizeof(blockName), "Commander%dGroup:%d", commanderId, groupId);
        int32_t groupResult = ScenarioFile->SeekBlock(blockName);

        while (groupResult == 0)
        {
            bool pointChosen = false;
            int32_t mates[MAX_MOVERGROUP_COUNT];
            groupResult = ScenarioFile->ReadIdLongArray("Mates", mates, MAX_MOVERGROUP_COUNT);
            Assert(groupResult == 0, static_cast<uint32_t>(groupResult),
                   " could not find Mates in Group in Scenario File ");

            for (int32_t i = 0; i < MAX_MOVERGROUP_COUNT; i++)
            {
                if (mates[i] <= 0)
                {
                    continue;
                }

                MCPart& mate = Parts[mates[i]];
                const int32_t partId = 0x200 + commanderId * 0x180 + groupId * MAX_MOVERGROUP_COUNT + i;
                mate.Object->SetPartId(partId);

                if (mate.Exists == 0)
                {
                    CreatedPartRoster[CurrentCreatorPart].PartId = partId;
                    CreatedPartRoster[CurrentCreatorPart].Created = 0;
                    CurrentCreatorPart++;
                }

                commander->GetGroup(groupId)->Add(static_cast<MCMover*>(mate.Object));

                if (!pointChosen)
                {
                    commander->GetGroup(groupId)->SelectPoint(1);
                    pointChosen = true;
                    commander->SetTeam(commander->GetGroup(groupId)->GetPoint()->GetTeam());
                }
            }

            if (MPlayer == nullptr && commanderId == 1)
            {
                CommanderTable[1]->GetGroup(groupId)->SetDisbandOnNoPoint(0);
            }

            groupId++;
            std::snprintf(blockName, sizeof(blockName), "Commander%dGroup:%d", commanderId, groupId);
            groupResult = ScenarioFile->SeekBlock(blockName);
        }
    }

    ClanTeam->BuildRoster(this);
    InnerSphereTeam->BuildRoster(this);

    if (AlliedTeam != nullptr)
    {
        AlliedTeam->BuildRoster(this);
    }

    if (MPlayer == nullptr)
    {
        HomeCommander->SetNetPlayerId(0);
    }

    HomeCommander->AddToGui(1);

    if (MPlayer != nullptr)
    {
        for (int32_t i = 0; i < NumCommanders; i++)
        {
            if (CommanderTable[i] != HomeCommander)
            {
                CommanderTable[i]->AddToGui(0);
            }
        }
    }

    ScenarioTime = 0.0f;
    MissionStartTime = 0;
    RunningTime = 0.0f;
    ActualTime = 0.0f;
    gameSystemFile->Close();
    delete gameSystemFile;
    UpdateDisplay(0, 1, 30, 1, 100);

    Eye = CameraList->ActivateAllReady();

    if (MPlayer != nullptr)
    {
        Eye->ChangeTarget(MPlayer->LocalMovers[0], 1);
    }

    // The 'Mechs start with the damage their loadouts carried over.
    for (MCBaseObject* object = InnerSphereMechList->Head; object != nullptr; object = object->Next)
    {
        if (object->ObjectClass == BATTLEMECH)
        {
            static_cast<MCBattleMech*>(object)->DamageLoadedComponents();
        }
    }

    for (MCBaseObject* object = ClanMechList->Head; object != nullptr; object = object->Next)
    {
        if (object->ObjectClass == BATTLEMECH)
        {
            static_cast<MCBattleMech*>(object)->DamageLoadedComponents();
        }
    }

    if (ScenarioFile->SeekBlock("Output") == 0)
    {
        HasOutputBlock = 1;
    }

    ScenarioFile->Close();
    delete ScenarioFile;
    ScenarioFile = nullptr;

    ScenarioEndTurn = -1;
    StartUpTurns = 10;
    StartMusic = 1;

    if (MPlayer != nullptr)
    {
        MPlayer->ChatCallback = ScenarioChatCallback;

        if (MPlayer->IsServer != 0)
        {
            for (int32_t& checkedIn : MPlayer->PlayerCheckedIn)
            {
                checkedIn = 0;
            }
        }

        MPlayer->SendPlayerCheckIn();
    }

    StartingUp = 1;
    StartUpCountdown = 100;
    std::free(ConnectShape);
    ConnectShape = nullptr;
    return 0;
}

auto MCScenario::Run() -> int32_t
{
    if (MPlayer != nullptr && MPlayer->InMission == 0)
    {
        MPlayer->ProcessReceiveList();
        return 0;
    }

    if (GamePaused != 0)
    {
        CameraList->Update();
        return static_cast<int32_t>(ScenarioResult);
    }

    Update();
    CameraList->Update();
    Land->Update();
    PathManager->Update();

    if (TrainManager != nullptr)
    {
        TrainManager->UpdateTrains();
    }

    ObjectList->Update();
    ClanTeam->UpdateSensors();

    if (AlliedTeam != nullptr)
    {
        AlliedTeam->UpdateSensors();
    }

    InnerSphereTeam->UpdateSensors();
    PotentialContactManager->UpdateStatus();

    if (CollisionSwitch != 0)
    {
        CollisionSystem->CheckObjects();
    }

    if (Turn < 2)
    {
        StartObjectiveTimers();
    }

    if (MPlayer == nullptr)
    {
        ScenarioBrain->Execute(ScenarioBrainParams);
        ScenarioResult = static_cast<uint32_t>(ScenarioBrain->ReturnVal);
    }
    else
    {
        CurMultiplayCode = 0;
        CurMultiplayParam = 0;
        ScenarioBrain->Execute(ScenarioBrainParams);
        CurMultiplayCode = 0;
        CurMultiplayParam = 0;

        if (MPlayer->IsServer == 0)
        {
            ScenarioResult = static_cast<uint32_t>(MPlayer->ScenarioResult);
        }
        else
        {
            ScenarioResult = static_cast<uint32_t>(ScenarioBrain->ReturnVal);

            if (ScenarioResult != 0)
            {
                MPlayer->SendEndScenario(0, static_cast<int32_t>(ScenarioResult));
            }
        }
    }

    if (MPlayer != nullptr)
    {
        if (MPlayer->IsServer != 0)
        {
            MPlayer->UpdateClients();
        }

        MPlayer->ProcessReceiveList();
    }

    return static_cast<int32_t>(ScenarioResult);
}

auto MCScenario::Destroy() -> void
{
    // Faithful: the id wraps to a short when the timer is removed (0x249f1 -> 0x49f1).
    for (uint32_t id = 0x249f1; id < NumObjectives + 0x249f1u; id++)
    {
        Application->RemoveTimer(Application, static_cast<int16_t>(id));
    }

    EndingScenario = 1;

    Assert(CollisionSystem != nullptr, 0, " collisionSystem already NULL ");

    if (CollisionSystem != nullptr)
    {
        CollisionSystem->Destroy();
        delete CollisionSystem;
    }

    CollisionSystem = nullptr;

    Assert(ElementList() != nullptr, 0, " ElementList already NULL ");
    MCGameContext::Current().SetElementList(nullptr);
    MCGameContext::Current().SetCraterManager(nullptr);

    Assert(Land != nullptr, 0, " land already NULL ");
    delete Land;
    Land = nullptr;

    Assert(ScenarioObjectList != nullptr, 0, " scenarioObjectList already NULL ");

    if (ScenarioObjectList != nullptr)
    {
        while (MCObjectQueueNode* node = ScenarioObjectList->Head)
        {
            MCObjectQueueNode* next = node->Next;
            node->Destroy();
            delete node;
            ScenarioObjectList->Head = next;
        }

        ScenarioObjectList->Tail = nullptr;
        ScenarioObjectList->Head = nullptr;
        delete ScenarioObjectList;
    }

    ScenarioObjectList = nullptr;
    StopObjects();

    if (SensorSystemManager != nullptr)
    {
        SensorSystemManager->Destroy();
        delete SensorSystemManager;
        SensorSystemManager = nullptr;
        MCSensorSystem::SortList = nullptr;
        ContactSortList = nullptr;
    }

    if (PotentialContactManager != nullptr)
    {
        PotentialContactManager->Destroy();
        delete PotentialContactManager;
        PotentialContactManager = nullptr;
    }

    if (ObjectTypeManager != nullptr)
    {
        ObjectTypeManager->Destroy();
        delete ObjectTypeManager;
        ObjectTypeManager = nullptr;
    }

    if (TrainManager != nullptr)
    {
        // Faithful: destroyed twice (once here, once by the inlined destructor).
        TrainManager->Destroy();
        TrainManager->Destroy();
        delete TrainManager;
        TrainManager = nullptr;
    }

    Assert(Parts != nullptr, 0, " parts already NULL ");
    Parts.reset();
    Assert(Objectives != nullptr, 0, " parts already NULL ");
    Objectives.reset();

    for (int32_t i = 0; i < NumCommanders; i++)
    {
        if (CommanderTable[i] != nullptr)
        {
            CommanderTable[i]->MCCommander::Destroy();
            delete CommanderTable[i];
        }

        CommanderTable[i] = nullptr;
    }

    DeleteTeam(ClanTeam);
    ClanTeam = nullptr;
    DeleteTeam(AlliedTeam);
    AlliedTeam = nullptr;
    DeleteTeam(InnerSphereTeam);
    InnerSphereTeam = nullptr;
    TeamTable[0] = nullptr;
    TeamTable[1] = nullptr;
    TeamTable[2] = nullptr;

    for (int32_t i = 0; i < 6; i++)
    {
        if (SensorContactShapes[i] != nullptr)
        {
            MCRenderer::UnregisterData(SensorContactShapes[i]);
            delete[] SensorContactShapes[i];
            SensorContactShapes[i] = nullptr;
        }
    }

    if (SmokeManager != nullptr)
    {
        SmokeManager->Destroy();
        delete SmokeManager;
        SmokeManager = nullptr;
    }

    Assert(AppearanceTypeList() != nullptr, 0, " appearanceTypeList already NULL ");
    MCGameContext::Current().SetAppearanceTypeList(nullptr);
    Assert(SpriteManager() != nullptr, 0, " spriteManager already NULL ");
    MCGameContext::Current().SetSpriteManager(nullptr);
    Assert(CameraList != nullptr, 0, " cameraList already NULL ");
    delete CameraList;
    CameraList = nullptr;
    Eye = nullptr;

    Assert(ScenarioBrainParams != nullptr, 0, " scenarioParams already NULL ");
    delete ScenarioBrainParams;
    ScenarioBrainParams = nullptr;

    if (OldPalette != nullptr)
    {
        MCGameContext::Current().SetPalette(std::move(OldPalette));
    }

    if (GameMap != nullptr)
    {
        GameMap->Destroy();
        delete GameMap;
        GameMap = nullptr;
    }

    if (GameObjectMap != nullptr)
    {
        GameObjectMap->Destroy();
        delete GameObjectMap;
        GameObjectMap = nullptr;
    }

    if (GlobalMoveMap != nullptr)
    {
        GlobalMoveMap->Destroy();
        delete GlobalMoveMap;
        GlobalMoveMap = nullptr;
    }

    if (PathFindMap != nullptr)
    {
        PathFindMap->Destroy();
        delete PathFindMap;
        PathFindMap = nullptr;
    }

    if (PathManager != nullptr)
    {
        PathManager->Destroy();
        delete PathManager;
        PathManager = nullptr;
    }

    MCFire::MaxFiresList.reset();

    DestroyWarriors();

    if (ScenarioBrain != nullptr)
    {
        ScenarioBrain->Destroy();
        delete ScenarioBrain;
        ScenarioBrain = nullptr;
    }

    if (OpenList != nullptr)
    {
        delete OpenList;
    }

    OpenList = nullptr;
    AblClose();
    MCRenderer::UnregisterData(WaypointMarkers);
    std::free(WaypointMarkers);
    WaypointMarkers = nullptr;
}

auto MCScenario::DestroyWarriors() -> void
{
    if (Warriors == nullptr)
    {
        return;
    }

    for (uint32_t i = 0; i < NumWarriors + 1; i++)
    {
        if (Warriors[i] != nullptr)
        {
            DeleteWarrior(Warriors[i]);
            Warriors[i] = nullptr;
        }
    }

    Warriors.reset();
    NumWarriors = 0;
}

auto MCScenario::CreatePartObject(int32_t partNumber) -> void
{
    MCPart& part = Parts[partNumber];

    if (part.Destroyed != 0 || part.Object != nullptr)
    {
        return;
    }

    MCGameObject* object = CreateObject(static_cast<int32_t>(part.ObjNumber));
    part.Object = object;

    if (object == nullptr)
    {
        char message[256];
        std::snprintf(message, sizeof(message), " Couldnt create object number %d  which is part Number %d",
                      part.ObjNumber, partNumber);
        Fatal(static_cast<int32_t>(part.ObjNumber), message);
    }

    object->SetAwake(part.Active);

    if (std::strcmp(part.ProfileName, "NONE") != 0)
    {
        std::string profileFileName;
        profileFileName = GamePath(ProfilePath, part.ProfileName, ".fit");
        auto* profileFile = new MCFitIniFile;

        if (profileFile == nullptr)
        {
            Fatal(static_cast<int32_t>(0xfaaf0001), " Profile File ");
        }

        if (profileFile->Open(profileFileName) == 0)
        {
            if (object->Init(profileFile) != 0)
            {
                Fatal(static_cast<int32_t>(0xfaaf0007), " Bad Profile File ");
            }
        }
        else
        {
            MCFitIniFile savedProfileFile;
            std::string savedProfileName;
            savedProfileName = GamePath(SaveTempPath, part.ProfileName, ".fit");
            const int32_t openResult = savedProfileFile.Open(savedProfileName);

            if (openResult != 0)
            {
                Fatal(openResult);
            }

            if (object->Init(&savedProfileFile) != 0)
            {
                Fatal(static_cast<int32_t>(0xfaaf0007), " Bad Profile File ");
            }
        }

        profileFile->Close();
        delete profileFile;
    }

    MCTeam* team = nullptr;

    if (part.TeamId == 0)
    {
        team = InnerSphereTeam;
    }
    else if (part.TeamId == 1)
    {
        team = ClanTeam;
    }
    else if (part.TeamId == 2)
    {
        team = AlliedTeam;
    }

    const MCObjectClass objectClass = object->ObjectClass;

    if (objectClass == BATTLEMECH)
    {
        auto* mech = static_cast<MCBattleMech*>(object);
        mech->SetPilot(Warriors[part.Pilot]);
        mech->SetTeam(team);
        mech->CalcWeaponEffectiveness(1);
        mech->CalcWeaponEffectiveness(0);
        mech->CalcWeaponRangeRatings();
        mech->Captureable = part.Captureable;
        const int32_t paintScheme = (part.PaintScheme == -1) ? Warriors[part.Pilot]->PaintScheme : part.PaintScheme;
        static_cast<MCMechActor*>(mech->Appearance)->FadeTableIndex = paintScheme;
    }
    else if (objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL)
    {
        auto* mover = static_cast<MCMover*>(object);
        mover->SetPilot(Warriors[part.Pilot]);
        mover->SetTeam(team);
        mover->CalcWeaponRangeRatings();

        // The original tests +0x8b8 of both: a vehicle's gvAppearance flag, and an elemental's field there, which
        // MCX.EXE only ever sets to 0.
        if (objectClass == GROUNDVEHICLE && static_cast<MCGroundVehicle*>(mover)->GvAppearance != 0)
        {
            static_cast<MCGVAppearance*>(mover->Appearance)->FadeTableIndex =
                (part.PaintScheme == -1) ? Warriors[part.Pilot]->PaintScheme : part.PaintScheme;
        }
    }

    object->SetControl(part.ControlType, part.ControlDataType, -1);
    MCVector3D position(part.Position[0], part.Position[1], part.Position[2]);
    object->SetPosition(position);

    if (objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL || objectClass == MOVER)
    {
        static_cast<MCMover*>(object)->SetLastValidPosition(position);
    }

    MCFrameOfRef frame;
    frame.ResetToWorldFrame();
    const double radians = part.Rotation * DEGREES_TO_RADIANS;
    RotateAboutK(frame, static_cast<float>(std::sin(radians)), static_cast<float>(std::cos(radians)));
    object->SetFrame(frame);

    if (objectClass == BATTLEMECH)
    {
        auto* actor = static_cast<MCMechActor*>(object->GetAppearance());

        if (actor != nullptr)
        {
            actor->SetGesture(part.GestureId);
        }

        if (part.Alignment == HomeTeam->Alignment)
        {
            actor->PreloadGestures();
        }
    }
    else if (objectClass == ELEMENTAL)
    {
        auto* actor = static_cast<MCElementalActor*>(object->GetAppearance());

        if (actor != nullptr)
        {
            actor->SetGesture(part.GestureId);
        }
    }

    // The part number is kept in the object's id.
    object->IdNumber = static_cast<uint32_t>(partNumber);

    if (MPlayer != nullptr)
    {
        MPlayer->AddToMoverRoster(static_cast<MCMover*>(object));
        MPlayer->AddToPlayerMoverRoster(part.CommanderId, static_cast<MCMover*>(object));

        if (part.CommanderId == MPlayer->CheckInId)
        {
            MPlayer->AddToLocalMovers(static_cast<MCMover*>(object));
        }
    }

    if (part.Exists == 0)
    {
        // Not in play yet: the script brings it in with createScenarioObject.
        object->SetCommanderId(part.CommanderId);
        object->SetAlignment(part.Alignment);

        if (MCObjectQueueNode* node = ScenarioObjectList->Head)
        {
            node->AddNode(object);
        }

        return;
    }

    if (objectClass < BATTLEMECH || ELEMENTAL < objectClass)
    {
        object->SetExists(1);

        if (MCObjectQueueNode* node = ObjectList->Head)
        {
            node->AddNode(object);
        }
    }
    else
    {
        object->SetCommanderId(part.CommanderId);
        object->SetAlignment(part.Alignment);
        MCObjectQueueNode* list = (part.Alignment == -1) ? ClanMechList : InnerSphereMechList;

        if (list != nullptr)
        {
            list->AddNode(object);
        }

        object->SetPotentialContact(objectClass == ELEMENTAL ? 2 : 1);
        object->SetExists(1);
    }

    GameObjectMap->AddObject(object);
}

auto MCScenario::CreateScenarioObject(int32_t partId) -> void
{
    MCObjectQueue* queue = ScenarioObjectList;
    MCBaseObject* object = nullptr;
    object = queue->Traverse(object);

    while (object != nullptr && object->PartId != partId)
    {
        object = queue->Traverse(object);
    }

    if (object == nullptr)
    {
        return;
    }

    if (partId < 0x200 || 0xfff < partId)
    {
        Fatal(0, " Unknown Object in createScenarioObject ");
    }

    // Take it out of the scenario list.
    for (MCObjectQueueNode* node = queue->Head; node != nullptr; node = node->Next)
    {
        MCBaseObject* prev = nullptr;
        MCBaseObject* current = node->Head;

        while (current != nullptr && current != object)
        {
            prev = current;
            current = current->Next;
        }

        if (current != nullptr)
        {
            node->RemoveNode(prev, current);
            break;
        }
    }

    auto* gameObject = static_cast<MCGameObject*>(object);
    MCObjectQueueNode* list = (gameObject->GetAlignment() != -1) ? InnerSphereMechList : ClanMechList;

    if (list != nullptr)
    {
        list->AddNode(object);
    }

    gameObject->SetPotentialContact(object->ObjectClass == ELEMENTAL ? 2 : 1);
    GameObjectMap->AddObject(gameObject);
    gameObject->SetExists(1);

    for (int32_t i = 0; i < CurrentCreatorPart; i++)
    {
        if (CreatedPartRoster[i].PartId == object->PartId)
        {
            CreatedPartRoster[i].Created = 1;
            return;
        }
    }
}

auto MCScenario::DestroyPartObject(int32_t partNumber) -> void
{
    MCPart& part = Parts[partNumber];
    auto* object = static_cast<MCGameObject*>(part.Object);

    if (object == nullptr)
    {
        return;
    }

    object->GetObjectType()->HandleDestruction(object, nullptr);

    for (MCObjectQueueNode* node = ObjectList->Head; node != nullptr && node->Remove(object) == 0; node = node->Next)
    {
    }

    part.Destroyed = 1;
    part.Active = 0;
    part.Exists = 0;
}

auto MCScenario::ObjectInArea(MCGameObject* object, int32_t areaNumber) -> int
{
    if (Areas == nullptr || NumAreas <= areaNumber)
    {
        return 0;
    }

    const MCScenarioArea& area = Areas[areaNumber];
    // OB-056: the area's size (a circle's radius, a rectangle's extent) is an uninitialised local in MCX.EXE; the
    // port reads it as 0. Never matters: nothing fills the areas, so numAreas stays 0.
    const float size = 0.0f;
    const MCVector3D position = object->GetPosition();

    if (area.AreaType == 0)
    {
        const float dx = area.Coords[0] - position.X;
        const float dy = area.Coords[1] - position.Y;
        const float dz = area.Coords[2] - position.Z;

        if (std::sqrt(dx * dx + dy * dy + dz * dz) < size)
        {
            return 1;
        }
    }
    else if (area.AreaType == 1)
    {
        // Faithful: the rectangle test compares x with y and y with z.
        if (size + area.Coords[0] < position.X && position.X < position.Y + area.Coords[0] &&
            position.X + area.Coords[1] < position.Y && position.Y < position.Z + area.Coords[1])
        {
            return 1;
        }
    }

    return 0;
}

auto MCScenario::StartObjectiveTimers() -> void
{
    for (int32_t i = 0; i < static_cast<int32_t>(NumObjectives); i++)
    {
        if (0.0f < Objectives[i].TimeLeft)
        {
            SetObjectiveTimer(i, Objectives[i].TimeLeft * 1000.0f);
        }
    }
}

auto MCScenario::SetObjectiveTimer(int32_t objectiveNumber, float time) -> int32_t
{
    if (objectiveNumber < 0 || static_cast<int32_t>(NumObjectives) <= objectiveNumber)
    {
        return BAD_OBJECTIVE;
    }

    const auto id = static_cast<int16_t>(objectiveNumber + OBJECTIVE_TIMER_ID);
    Application->RemoveTimer(Application, id);
    Application->AddTimer(Application, id, static_cast<int32_t>(time), OBJECTIVE_TIMER_EVENT, 0, 1);
    return 0;
}

auto MCScenario::CheckObjectiveTimer(int32_t objectiveNumber) -> float
{
    if (objectiveNumber < 0 || static_cast<int32_t>(NumObjectives) <= objectiveNumber)
    {
        return 0.0f;
    }

    uint32_t remaining = 0;

    if (MCGuiTimer* timer = Application->TimerManager->GetTimer(
            Application, static_cast<int16_t>(objectiveNumber + OBJECTIVE_TIMER_ID)))
    {
        // The timer counts in scenario milliseconds.
        const uint32_t fireTime = timer->Interval + timer->LastTime;
        remaining = static_cast<uint32_t>(
            static_cast<int32_t>(static_cast<double>(fireTime) - static_cast<double>(ScenarioTime) * 1000.0));
    }

    return static_cast<float>(static_cast<double>(remaining) * 0.001);
}

auto MCScenario::SetObjectiveStatus(int32_t objectiveNumber, uint32_t status) -> int32_t
{
    if (objectiveNumber < 0 || static_cast<int32_t>(NumObjectives) <= objectiveNumber)
    {
        return BAD_OBJECTIVE;
    }

    Objectives[objectiveNumber].Status = status;
    return 0;
}

auto MCScenario::CheckObjectiveStatus(int32_t objectiveNumber) -> uint32_t
{
    if (objectiveNumber < 0 || static_cast<int32_t>(NumObjectives) <= objectiveNumber)
    {
        return NO_OBJECTIVE;
    }

    return Objectives[objectiveNumber].Status;
}

auto MCScenario::SetObjectiveType(int32_t objectiveNumber, uint32_t type) -> int32_t
{
    if (objectiveNumber < 0 || static_cast<int32_t>(NumObjectives) <= objectiveNumber)
    {
        return BAD_OBJECTIVE;
    }

    Objectives[objectiveNumber].Type = type;
    return 0;
}

auto MCScenario::CheckObjectiveType(int32_t objectiveNumber) -> uint32_t
{
    if (objectiveNumber < 0 || static_cast<int32_t>(NumObjectives) <= objectiveNumber)
    {
        return NO_OBJECTIVE;
    }

    return Objectives[objectiveNumber].Type;
}

auto MCScenario::SetObjectivePos(int32_t objectiveNumber, float x, float y, float z) -> void
{
    if (objectiveNumber < 0 || static_cast<int32_t>(NumObjectives) <= objectiveNumber)
    {
        return;
    }

    Objectives[objectiveNumber].Position[0] = x;
    Objectives[objectiveNumber].Position[1] = y;
    Objectives[objectiveNumber].Position[2] = z;
}

auto MCScenario::CalcResourcePointsEarned() -> int32_t
{
    if (ScenarioResult <= 3)
    {
        return 0;
    }

    int32_t points = 0;

    for (int32_t i = 0; i < MAX_OBJECTIVES; i++)
    {
        if (Objectives[i].Status == 1 || Mission->EndScenarioRequested != 0)
        {
            points += Objectives[i].Points;
        }
    }

    return points;
}

auto MCScenario::SetupBonus() -> void
{
    // Port fix: the original's search read one objective past the array when all nine were in use.
    int32_t slot = 0;

    while (slot < MAX_OBJECTIVES && Objectives[slot].Status != UNUSED_OBJECTIVE)
    {
        slot++;
    }

    Assert(slot < MAX_OBJECTIVES, static_cast<uint32_t>(slot), " Too Many objectives in use ");

    MCScenarioObjective& bonus = Objectives[slot];
    const int32_t unusedTonnage = MaxDeployTonnage - CurDeployTonnage;
    bonus.Status = 1;
    bonus.Type = 3;
    bonus.Points = unusedTonnage / TonnageDivisor * ResourcesPerTonDivided;
    char format[256];
    CLoadString(ThisInstance, 0x376, format, 0xfe);
    std::snprintf(bonus.Name, sizeof(bonus.Name), format, unusedTonnage);
}

auto MCScenario::HandleMultiplayMessage(int32_t code, int32_t param) -> void
{
    if (ScenarioBrainHandleMessage == nullptr)
    {
        return;
    }

    CurMultiplayCode = code;
    CurMultiplayParam = param;
    ScenarioBrain->Execute(nullptr, ScenarioBrainHandleMessage, nullptr);
    CurMultiplayCode = 0;
    CurMultiplayParam = 0;
}

auto MCScenario::CheckAnyoneInCombat() -> void
{
    for (int32_t i = 1; i <= static_cast<int32_t>(NumWarriors); i++)
    {
        MCMechWarrior* warrior = Warriors[i];

        if (warrior == nullptr || warrior->Status != 0)
        {
            continue;
        }

        MCGameObject* target = warrior->GetLastTarget();

        if (target != nullptr && target->GetAlignment() != warrior->Alignment && target->GetAlignment() != 0 &&
            target->IsDisabled() == 0)
        {
            InCombat = 1;
            return;
        }
    }

    InCombat = 0;
}
