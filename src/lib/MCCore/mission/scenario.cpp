#include "stdafx.h"
#include "mission/scenario.h"
#include "abl/ablenv.h"
#include "abl/ablrtn.h"
#include "abl/ablxstd.h"
#include "ai/move.h"
#include "ai/tacordr.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "color/color.h"
#include "engine/ceglist.h"
#include "engine/celement.h"
#include "engine/cevfx.h"
#include "engine/crater.h"
#include "gui/asystem.h"
#include "gui/atextbox.h"
#include "gui/updisp.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "lib/inifile.h"
#include "lib/packet.h"
#include "lib/pqueue.h"
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
#include "sprite/bactor.h"
#include "sprite/gvactor.h"
#include "sprite/lactor.h"
#include "sprite/mactor.h"
#include "sprite/sprtmgr.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/vfxfuncs.h"

Scenario* scenario = nullptr;
float actualTime = 0.0f;
int nextStep = 0;
int prevStep = 0;
int32_t scenarioEndTurn = -1;
float minFrameLength = 0.25f;
float partCreateTime = -1.0f;
int collisionSwitch = 1;
int32_t tonnageDivisor = 5;
int32_t resourcesPerTonDivided = 200;
uint32_t AblSymbolTableHeapSize = 102400;
uint32_t AblStackHeapSize = 40960;
uint32_t AblCodeHeapSize = 102400;
uint32_t AblRunTimeStackSize = 20480;
uint32_t AblMaxCodeBlockSize = 10240;
uint32_t AblMaxRegisteredModules = 200;
uint32_t AblMaxStaticVariables = 100;
CollisionSystem* collisionSystem = nullptr;
BaseObject* MoverRoster[0xe00] = {};
int32_t MineLayThrottle = 0;
int32_t MineSweepThrottle = 0;
float MineWaitTime = 0.0f;
Team* TeamTable[3] = {};
TrainManager* trainManager = nullptr;
int32_t visualRangeTable[256] = {};
int32_t globalPlayerWeapons[2] = {};
int32_t globalEnemySkills[2] = {};
CreatedPartRoster createdPartRoster[100] = {};
int32_t globalEnemyWeapons[2] = {};
int32_t globalPlayerSkills[2] = {};
int32_t globalSalvageModifier[2] = {};
uint32_t MissionStartTime = 0;
float runningTime = 0.0f;
int32_t startMusic = 0;
float InfluenceTime = 0.0f;
int drawRevealedTacMap = 0;
int32_t currentCreatorPart = 0;
uint8_t* waypointMarkers = nullptr;
int endingScenario = 0;
uint8_t forceAlways = 0;
char saveTempPath[80] = "data\\save\\temp\\";

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
    /// <summary>Degrees to radians, as MCX.EXE stores it (MCX.EXE @ 0x0077c2a0; a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;

    /// <summary>Asserts that a FIT read or system start returned 0.</summary>
    void requireOk(int32_t result, const char* message)
    {
        Assert(result == 0, static_cast<uint32_t>(result), message);
    }

    /// <summary>Turns a frame about its k axis (MC2's inline frame_of_ref::rotate_about_k).</summary>
    void rotateAboutK(frame_of_ref& frame, float s, float c)
    {
        const vector_3d oldI = frame.i;
        frame.i = frame.i * c + frame.j * s;
        frame.j = frame.j * c - oldI * s;
    }

    /// <summary>A team with its (inlined) constructor: <c>Team::init()</c>.</summary>
    Team* newTeam()
    {
        auto* team = new Team;

        if (team != nullptr)
        {
            team->Team::init();
        }

        return team;
    }

    /// <summary>A team's (inlined) destructor: <c>Team::destroy</c>, then free it.</summary>
    void deleteTeam(Team* team)
    {
        if (team == nullptr)
        {
            return;
        }

        team->Team::destroy();
        delete team;
    }

    /// <summary>A warrior with its (inlined) constructor: every tactical order cleared, then <c>init()</c>.</summary>
    MechWarrior* newWarrior()
    {
        auto* warrior = new MechWarrior;

        if (warrior != nullptr)
        {
            for (TacticalOrder& order : warrior->tacOrder)
            {
                order.init();
            }

            warrior->lastTacOrder.init();
            warrior->curTacOrder.init();
            warrior->init();
        }

        return warrior;
    }

    /// <summary>A warrior's (inlined) destructor: <c>destroy()</c>, its tactical orders, then free it.</summary>
    void deleteWarrior(MechWarrior* warrior)
    {
        warrior->destroy();
        warrior->curTacOrder.destroy();
        warrior->lastTacOrder.destroy();

        for (int32_t i = NUM_ORDERSTATES - 1; i >= 0; --i)
        {
            warrior->tacOrder[i].destroy();
        }

        delete warrior;
    }

    /// <summary>Reads a whole file into a new block (the scenario's connect and waypoint shapes).</summary>
    /// <returns>The block, or null when the file can't be opened (the old one is kept).</returns>
    uint8_t* loadShapeFile(const char* name, uint8_t* oldShapes, int freeOld)
    {
        FullPathFileName fileName;
        fileName.init(shapesPath, name, ".shp");
        File shapeFile;

        if (shapeFile.open(fileName) != 0)
        {
            return oldShapes;
        }

        // Port fix: the original freed the malloc'd waypoint shapes with operator delete.
        if (freeOld && oldShapes != nullptr)
        {
            std::free(oldShapes);
        }

        auto* shapes = static_cast<uint8_t*>(std::malloc(shapeFile.fileSize()));

        if (shapes != nullptr)
        {
            shapeFile.read(shapes, static_cast<int32_t>(shapeFile.fileSize()));
        }

        shapeFile.close();
        return shapes;
    }

    /// <summary>
    /// Opens <paramref name="name"/><c>.fit</c> in <paramref name="path"/> into <paramref name="file"/>, falling back
    /// on the temporary save folder (a saved game's copy).
    /// </summary>
    /// <returns>The result of the last open.</returns>
    int32_t openWithSaveFallback(FitIniFile& file, const char* path, const char* name)
    {
        FullPathFileName fileName;
        fileName.init(path, name, ".fit");
        int32_t result = file.open(fileName);

        if (result != 0)
        {
            fileName.init(saveTempPath, name, ".fit");
            result = file.open(fileName);
        }

        return result;
    }
}

auto Scenario::update() -> int32_t
{
    if (frameLength <= 0.0f)
    {
        frameLength = 0.05f;
    }

    if (minFrameLength < frameLength)
    {
        frameLength = minFrameLength;
    }

    if (dynamicFrameTiming == 0)
    {
        dynamicFrameTiming = 1;
    }

    scenarioTime = scenarioTime + frameLength;

    if (MissionStartTime == 0)
    {
        if (MPlayer != nullptr && 10.0f < scenarioTime)
        {
            Fatal(0, " runningTime is not working...why? ");
        }
    }
    else
    {
        runningTime = static_cast<float>(static_cast<double>(MCPort::Milliseconds() - MissionStartTime) * 0.001);
    }

    actualTime = (MPlayer != nullptr) ? runningTime : scenarioTime;

    if (0 < scenario->timeLimit)
    {
        if (twoMinuteWarningPlayed == 0 && static_cast<float>(scenario->timeLimit) - actualTime < 120.0f)
        {
            soundSystem->playBettySample(8);
            twoMinuteWarningPlayed = 1;
        }

        if (thirtySecondWarningPlayed == 0 && static_cast<float>(scenario->timeLimit) - actualTime < 30.0f)
        {
            soundSystem->playBettySample(7);
            thirtySecondWarningPlayed = 1;
        }
    }

    const int32_t musicPending = startMusic;
    turn++;
    nextStep = 0;
    prevStep = 0;

    if (turn < unknown2A4 + startUpTurns)
    {
        startingUp = 1;
        startUpCountdown = (unknown2A4 + startUpTurns - turn) * 10;
        return 0;
    }

    startingUp = 0;
    startUpCountdown = 0;

    if (musicPending != 0)
    {
        if (soundSystem != nullptr)
        {
            soundSystem->stopStaticNoise();

            if (soundSystem != nullptr)
            {
                soundSystem->playDigitalMusic(scenario->scenarioTuneNum, false);
            }
        }

        application->showCursor(1);
        startMusic = 0;
        MissionStartTime = MCPort::Milliseconds();
    }

    return 0;
}

auto Scenario::render(aObject* window) -> int32_t
{
    if (1 < turn)
    {
        cameraList->renderView(window);
    }

    return 0;
}

auto applyDifficultySkill(float skill, int player) -> float
{
    const int32_t* percentages = (player == 0) ? globalEnemySkills : globalPlayerSkills;

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

auto applyDifficultyWeapon(float value, int player) -> float
{
    const int32_t* percentages = (player == 0) ? globalEnemyWeapons : globalPlayerWeapons;

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

auto InitDifficultySettings(FitIniFile* gameSystemFile) -> void
{
    int32_t result = gameSystemFile->seekBlock("DifficultySettings");
    Assert(result == 0, static_cast<uint32_t>(result), "No Difficulty Settings in gameSys");
    result = gameSystemFile->readIdLongArray("PlayerSkills", globalPlayerSkills, 2);
    Assert(result == 0, static_cast<uint32_t>(result), "No Difficulty Settings for Player Skills in gameSys");
    result = gameSystemFile->readIdLongArray("EnemySkills", globalEnemySkills, 2);
    Assert(result == 0, static_cast<uint32_t>(result), "No Difficulty Settings for Enemy Skills in gameSys");
    result = gameSystemFile->readIdLongArray("PlayerWeapons", globalPlayerWeapons, 2);
    Assert(result == 0, static_cast<uint32_t>(result), "No Difficulty Settings for Player Weapons in gameSys");
    result = gameSystemFile->readIdLongArray("EnemyWeapons", globalEnemyWeapons, 2);
    Assert(result == 0, static_cast<uint32_t>(result), "No Difficulty Settings for Enemy Weapons in gameSys");
    result = gameSystemFile->readIdLongArray("SalvageChance", globalSalvageModifier, 2);
    Assert(result == 0, static_cast<uint32_t>(result), "Do Difficulty Settings for Salvage Chance in GameSys");
}

auto Scenario::init(char* scenarioName, char* terrainName) -> int32_t
{
    int32_t result = 0;
    TacOrderQueuePos = 0;
    numCameraDrones = 0;

    connectShape = loadShapeFile("connect", connectShape, 0);
    waypointMarkers = loadShapeFile("waypoints", waypointMarkers, 1);

    ABLi_init(AblSymbolTableHeapSize, AblStackHeapSize, AblCodeHeapSize, AblRunTimeStackSize, AblMaxCodeBlockSize,
              AblMaxRegisteredModules, AblMaxStaticVariables, ABLDebuggerPrintCallback, 0, 0, 0);
    turn = 0;
    unknown238 = 0;

    // The objects placed now but brought into play later by the script.
    auto* objectQueue = new ObjectQueue;

    if (objectQueue != nullptr)
    {
        ObjectQueueNode* node = objectQueue->findList(DEFAULT_LIST_ID);

        if (node == nullptr)
        {
            node = new ObjectQueueNode(DEFAULT_LIST_ID);

            if (node != nullptr)
            {
                objectQueue->addList(node);
            }
        }

        objectQueue->tail = node;
        objectQueue->head = node;
    }

    scenarioObjectList = objectQueue;
    std::memset(createdPartRoster, 0, sizeof(createdPartRoster));
    currentCreatorPart = 0;

    UpdateDisplay(0, 1, 100, 1, 0);
    soundSystem->playStaticNoise();

    //---------------------------------------------------------------------------------------------------------------
    // The game system file.
    FullPathFileName gameSystemName;
    gameSystemName.init(missionPath, "gamesys", ".fit");
    auto* gameSystemFile = new FitIniFile;

    if (gameSystemFile == nullptr)
    {
        Fatal(static_cast<int32_t>(0xfaaf0001), " Game System File ");
    }

    result = gameSystemFile->open(gameSystemName);
    requireOk(result, " Could not open GameSys.Fit file ");

    result = gameSystemFile->seekBlock("General");
    requireOk(result, " Could not find General Block in GameSys ");
    result = gameSystemFile->readIdFloat("MaxVisualRange", maxVisualRange);
    requireOk(result, " Could not find MaxVisualRange in GameSys ");
    MaxVisualRadius = maxVisualRange * 1.4142f;
    result = gameSystemFile->readIdFloat("FireVisualRange", fireVisualRange);
    requireOk(result, " Could not find FireVisualRange in GameSys ");
    result = gameSystemFile->readIdFloat("MaxWeaponRange", maxWeaponRange);
    requireOk(result, " Could not find MaxWeaponRange in GameSys ");
    result = gameSystemFile->readIdFloatArray("WeaponRange", WeaponRange, 3);
    requireOk(result, " Could not find WeaponRange in GameSys ");
    result = gameSystemFile->readIdFloat("DefaultAttackRange", DefaultAttackRange);

    if (result != 0)
    {
        DefaultAttackRange = 75.0f;
    }

    result = gameSystemFile->readIdFloat("BaseSensorRange", baseSensorRange);
    requireOk(result, " Could not find BaseSensorRange in GameSys ");
    result = gameSystemFile->readIdLongArray("VisualRangeTable", visualRangeTable, 256);
    requireOk(result, " Could not find Visual Range Table ");
    UpdateDisplay(0, 1, 30, 1, 2);

    if (MasterComponentList == nullptr)
    {
        FullPathFileName componentName;
        componentName.init(objectPath, "compbas", ".csv");
        const int32_t loadResult =
            initMasterComponentListEXCEL(componentName, 0xff, maxVisualRange / maxWeaponRange, baseSensorRange);
        // Faithful: the assert reports the previous read's code.
        Assert(loadResult == 0, static_cast<uint32_t>(result), " Could not load compBas.csv ");
    }

    result = gameSystemFile->readIdUChar("AlwaysRevealed", alwaysRevealed);
    requireOk(result, " Could not find AlwaysRevealed in GameSys ");

    if (gameSystemFile->readIdUChar("GodMode", godMode) != 0)
    {
        godMode = 0;
    }

    if (gameSystemFile->readIdUChar("AlwaysDraw", forceAlways) != 0)
    {
        forceAlways = 0;
    }

    uint8_t revealTacMap = 0;

    if (gameSystemFile->readIdUChar("RevealTacMap", revealTacMap) != 0)
    {
        revealTacMap = 0;
    }

    drawRevealedTacMap = revealTacMap;

    if (gameSystemFile->readIdUChar("FootPrints", footPrints) != 0)
    {
        footPrints = 1;
    }

    result = gameSystemFile->readIdLong("BonusTonnageDivisor", tonnageDivisor);
    requireOk(result, " No Tonnage divisor in GameSys ");
    result = gameSystemFile->readIdLong("BonusPointsPerTon", resourcesPerTonDivided);
    requireOk(result, " No Bonus points per Ton in GameSys ");

    InitDifficultySettings(gameSystemFile);
    result = loadMoverGameSystem(gameSystemFile, maxVisualRange);
    requireOk(result, " could not load Mover System in GameSys ");
    result = loadMultiplayerGameSystem(gameSystemFile);
    requireOk(result, " could not load Multiplayer System in GameSys ");
    result = loadMechGameSystem(gameSystemFile);
    requireOk(result, " could not load Mech System in GameSys ");

    if (GameDifficulty == 0)
    {
        MechSalvageChance = globalSalvageModifier[0];
    }
    else if (GameDifficulty == 2)
    {
        MechSalvageChance = globalSalvageModifier[1];
    }

    result = loadGroundVehicleGameSystem(gameSystemFile);
    requireOk(result, " could not load Ground Vehicle System in GameSys ");
    result = loadElementalGameSystem(gameSystemFile);
    requireOk(result, " could not load Elemental System in GameSys ");

    result = gameSystemFile->seekBlock("Mine");
    requireOk(result, " Could not find Mine Block in GameSys ");
    result = gameSystemFile->readIdFloat("BaseDamage", MineBaseDamage);
    requireOk(result, " Could not find Damage variable in Mine Block in GameSys ");
    result = gameSystemFile->readIdFloat("SplashDamage", MineSplashDamage);
    requireOk(result, " Could not find Splash Damage variable in Mine Block in GameSys ");
    result = gameSystemFile->readIdFloat("SplashRange", MineSplashRange);
    requireOk(result, " Could not find Splash Range variable in Mine Block in GameSys ");
    result = gameSystemFile->readIdLong("Explosion", MineExplosion);
    requireOk(result, " Could not find Explosion variable in Mine Block in GameSys ");

    if (gameSystemFile->readIdLong("MineLayThrottle", MineLayThrottle) != 0)
    {
        MineLayThrottle = 50;
    }

    if (gameSystemFile->readIdLong("MineSweepThrottle", MineSweepThrottle) != 0)
    {
        MineSweepThrottle = 50;
    }

    result = gameSystemFile->readIdFloat("MineWaitTime", MineWaitTime);
    requireOk(result, " Could not find mine Wait time in Mine Block ");

    result = gameSystemFile->seekBlock("Smoke");
    requireOk(result, " Could not find Smoke Block in GameSys ");
    result = gameSystemFile->readIdLong("MaxSmokeSpheres", totalSmokeSpheres);
    requireOk(result, " Could not find total Smoke Count in GameSys ");
    result = gameSystemFile->readIdLong("TotalSmokeShapeSize", totalSmokeShapeSize);
    requireOk(result, " Could not find total Smoke Shape Size in GameSys ");

    result = gameSystemFile->seekBlock("Fire");
    requireOk(result, " Could not find Fire Block in GameSys ");
    result = gameSystemFile->readIdLong("MaxFiresBurning", maxFiresBurning);
    requireOk(result, " COuld not find max fires burning in gameSys ");
    result = gameSystemFile->readIdFloat("MaxFireBurnTime", maxFireBurnTime);
    requireOk(result, " COuld not find max fire burn time in gameSys ");
    UpdateDisplay(0, 1, 30, 1, 5);

    //---------------------------------------------------------------------------------------------------------------
    // The scenario file (from the missions folder, or a saved game's copy).
    FullPathFileName scenarioFileName;
    scenarioFileName.init(missionPath, scenarioName, ".fit");
    scenarioFile = new FitIniFile;
    Assert(scenarioFile != nullptr, 0, " no RAM for scenario file ");
    result = scenarioFile->open(scenarioFileName);

    if (result != 0)
    {
        scenarioFileName.init(saveTempPath, scenarioName, ".fit");
        result = scenarioFile->open(scenarioFileName);
        requireOk(result, " could not open scenario file ");
    }

    result = scenarioFile->seekBlock("Planet");

    if (result == 0)
    {
        scenarioFile->readIdLong("Setting", CurPlanet);
        requireOk(result, " could not find Setting in Planet Block ");
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

    result = scenarioFile->seekBlock("PaletteSystem");
    requireOk(result, " could not find PaletteSystem Block ");
    result = scenarioFile->readIdString("PaletteSystem", paletteSystem, 79);
    requireOk(result, " could not find PaletteSystem in PaletteSystem Block ");
    oldPalette = gamePalette;
    auto* palette = new Palette;

    if (palette != nullptr)
    {
        palette->numColors = 0;
        palette->rgbData = nullptr;
        palette->init();
    }

    gamePalette = palette;
    Assert(palette != nullptr, static_cast<uint32_t>(result), " no RAM for gamePalette ");
    result = gamePalette->init(paletteSystem);
    requireOk(result, " could not start gamePalette ");
    InitAlphaLookup(reinterpret_cast<VFX_RGB*>(gamePalette->rgbData));
    application->activatePalette(gamePalette->rgbData, 10, 0xf6);
    UpdateDisplay(0, 1, 20, 1, 7);

    //---------------------------------------------------------------------------------------------------------------
    // Teams and commanders.
    result = scenarioFile->seekBlock("Teams");
    requireOk(result, "Could not find Teams Block");
    int haveAlliedTeam = 0;
    result = scenarioFile->readIdBoolean("AlliedTeam", haveAlliedTeam);
    requireOk(result, " Could not find AlliedTeam in Teams Block ");

    if (clanTeam != nullptr)
    {
        deleteTeam(clanTeam);
    }

    clanTeam = newTeam();
    clanTeam->alignment = -1;
    clanTeam->init(1, 0x80);
    TeamTable[1] = clanTeam;

    if (alliedTeam != nullptr)
    {
        deleteTeam(alliedTeam);
        alliedTeam = nullptr;
    }

    if (haveAlliedTeam != 0)
    {
        alliedTeam = newTeam();
        alliedTeam->alignment = 1;
        alliedTeam->init(2, 0x80);
        TeamTable[2] = alliedTeam;
    }

    if (innerSphereTeam != nullptr)
    {
        deleteTeam(innerSphereTeam);
    }

    innerSphereTeam = newTeam();
    innerSphereTeam->alignment = 1;
    innerSphereTeam->init(0, 0x80);
    TeamTable[0] = innerSphereTeam;
    UpdateDisplay(0, 1, 30, 1, 10);

    result = scenarioFile->seekBlock("Artillery");
    requireOk(result, " could not find Artillery block in Scenario File ");
    numCameraStrikes = 0;
    numSensorStrikes = 0;
    numSmallStrikes = 0;
    numLargeStrikes = 0;
    scenarioFile->readIdLong("NumLargeStrikes", numLargeStrikes);
    scenarioFile->readIdLong("NumSmallStrikes", numSmallStrikes);
    scenarioFile->readIdLong("NumSensorStrikes", numSensorStrikes);
    scenarioFile->readIdLong("NumCameraStrikes", numCameraStrikes);

    if (MPlayer == nullptr)
    {
        homeTeam = innerSphereTeam;
        NumCommanders = (alliedTeam == nullptr) ? 2 : 3;

        for (int32_t i = 0; i < NumCommanders; i++)
        {
            auto* commander = new Commander;

            if (commander != nullptr)
            {
                commander->init();
            }

            CommanderTable[i] = commander;
            CommanderTable[i]->setId(i);
        }

        HomeCommander = CommanderTable[0];
        CommanderTable[0]->setNumSmallStrikes(numSmallStrikes);
        HomeCommander->setNumLargeStrikes(numLargeStrikes);
        HomeCommander->setNumSensorStrikes(numSensorStrikes);
        HomeCommander->setNumCameraDrones(numCameraStrikes);
        CommanderTable[1]->setNumSmallStrikes(999);
        CommanderTable[1]->setNumLargeStrikes(999);
        CommanderTable[1]->setNumSensorStrikes(999);
        CommanderTable[1]->setNumCameraDrones(999);
    }
    else
    {
        NumCommanders = MAX_COMMANDERS;

        for (int32_t i = 0; i < NumCommanders; i++)
        {
            auto* commander = new Commander;

            if (commander != nullptr)
            {
                commander->init();
            }

            CommanderTable[i] = commander;
            CommanderTable[i]->setId(i);
        }

        if (MPlayer->homeTeam == 0)
        {
            homeTeam = innerSphereTeam;
        }
        else if (MPlayer->homeTeam == 1)
        {
            homeTeam = clanTeam;
        }
        else
        {
            Fatal(0, " Must Be Clan or InnerSphere in Multiplayer! ");
        }

        HomeCommander = CommanderTable[MPlayer->checkInId];
    }

    UpdateDisplay(0, 1, 30, 1, 13);

    //---------------------------------------------------------------------------------------------------------------
    // Music, scale and the element (draw list) system.
    result = scenarioFile->seekBlock("Music");
    requireOk(result, " could not find Music block in Scenario File ");
    result = scenarioFile->readIdUChar("scenarioTuneNum", scenarioTuneNum);
    requireOk(result, " could not find ScenarioTuneNum in Music block in Scenario File ");

    result = scenarioFile->seekBlock("GameScale");
    requireOk(result, " could not find GameScale block in Scenario File ");
    result = scenarioFile->readIdFloat("WorldUnitsPerMeter", worldUnitsPerMeter);
    requireOk(result, " could not find worldUnitsperMeter in GameScale block in Scenario File ");
    result = scenarioFile->readIdFloat("MetersPerWorldUnit", metersPerWorldUnit);
    requireOk(result, " could not find MetersperWorldUnit in GameScale block in Scenario File ");
    result = scenarioFile->readIdULong("Duration", duration);
    requireOk(result, " could not find Duration in GameScale block in Scenario File ");
    result = scenarioFile->readIdFloat("CycleLength", cycleLength);
    requireOk(result, " could not find CycleLength in GameScale block in Scenario File ");
    uint32_t singleStep = 0;
    result = scenarioFile->readIdULong("SingleStep", singleStep);
    singleStepMode = static_cast<int>(singleStep);
    int32_t scenarioVisualRanges[256];
    result = scenarioFile->readIdLongArray("VisualRangeTable", scenarioVisualRanges, 256);

    if (result == 0)
    {
        std::memcpy(visualRangeTable, scenarioVisualRanges, sizeof(visualRangeTable));
    }

    result = scenarioFile->seekBlock("ElementSystem");
    requireOk(result, " could not find ElementSystem block in Scenario File ");
    uint32_t elementHeapSize = 0;
    result = scenarioFile->readIdULong("ElementHeapSize", elementHeapSize);
    requireOk(result, " could not find ElementHeapSize in ElementSystem block in Scenario File ");
    uint32_t maxElements = 0;
    result = scenarioFile->readIdULong("MaxElements", maxElements);
    requireOk(result, " could not find MaxElements in ElementSystem block in Scenario File ");
    uint32_t maxGroups = 0;
    result = scenarioFile->readIdULong("MaxGroups", maxGroups);
    requireOk(result, " could not find MaxGroups in ElementSystem block in Scenario File ");

    if (MPlayer != nullptr)
    {
        elementHeapSize <<= 1;
    }

    // Port fix: ElementHeapSize was tuned for 32-bit elements; the x64 ones (vtable and pointer fields) are up to
    // twice the size, and running the pool dry is Fatal(0xeeeb0003).
    elementHeapSize <<= 1;
    result = ElementPool::init(static_cast<int32_t>(elementHeapSize));
    requireOk(result, " could not Start ElementSystem ");
    ElementList = new ElementBuffer;
    Assert(ElementList != nullptr, static_cast<uint32_t>(result), " no RAM for ElementList ");

    if (MPlayer == nullptr)
    {
        maxGroups <<= 1;
        maxElements <<= 1;
    }
    else
    {
        maxGroups <<= 2;
        maxElements <<= 2;
    }

    result = ElementList->init(static_cast<int32_t>(maxElements), 0, static_cast<int32_t>(maxGroups));
    requireOk(result, " could not start ElementList ");
    UpdateDisplay(0, 1, 30, 1, 15);

    //---------------------------------------------------------------------------------------------------------------
    // The sensor contact blips.
    result = scenarioFile->seekBlock("SensorContactShape");
    requireOk(result, " could not find SensorContactShape block in Scenario File ");
    char sensorShapeName[80];
    result = scenarioFile->readIdString("shapeName", sensorShapeName, 79);
    requireOk(result, " could not find ShapeName in SensorContactShape block in Scenario File ");
    FullPathFileName sensorShapeFileName;
    sensorShapeFileName.init(spritePath, sensorShapeName, ".pak");
    PacketFile sensorShapeFile;
    result = sensorShapeFile.open(sensorShapeFileName);

    if (result != 0)
    {
        FullPathFileName cdFileName;
        cdFileName.init(CDspritePath, sensorShapeName, ".pak");
        result = sensorShapeFile.open(cdFileName);
        requireOk(result, " could not open sensor shape file ");
    }

    for (int32_t i = 0; i < 6; i++)
    {
        sensorShapeFile.seekPacket(i);
        sensorContactShapes[i] =
            static_cast<uint8_t*>(systemHeap->malloc(static_cast<uint32_t>(sensorShapeFile.getPacketSize())));
        Assert(sensorContactShapes[i] != nullptr, static_cast<uint32_t>(result), " no RAM for Large Sensor Shape ");
        sensorShapeFile.readPacket(i, sensorContactShapes[i]);
    }

    sensorShapeFile.close();

    //---------------------------------------------------------------------------------------------------------------
    // Craters, cameras, objects, sprites, appearances, sensors and contacts.
    int32_t numCraters = 0;
    uint32_t craterShapeSize = 0;
    result = scenarioFile->seekBlock("CraterSystem");
    requireOk(result, " could not find CraterSystem Block in Scenario File ");
    result = scenarioFile->readIdLong("NumCraters", numCraters);
    requireOk(result, " could not find NumCraters in CraterSystem Block in Scenario File ");
    result = scenarioFile->readIdULong("CraterShapeSize", craterShapeSize);
    requireOk(result, " could not find CraterShapeSize in CraterSystem Block in Scenario File ");
    char craterFileName[16];
    result = scenarioFile->readIdString("CraterFile", craterFileName, 15);
    requireOk(result, " could not find CraterFile in CraterSystem Block in Scenario File ");

    // The heap block is never constructed in the original; the port constructs it in place.
    if (void* block = systemHeap->malloc(sizeof(CraterManager)))
    {
        craterManager = ::new (block) CraterManager;
    }
    else
    {
        craterManager = nullptr;
    }

    Assert(craterManager != nullptr, static_cast<uint32_t>(result), " no RAM for Crater Manager ");
    result = craterManager->init(numCraters, craterShapeSize, craterFileName);
    requireOk(result, " could not Start CraterManager ");

    result = scenarioFile->seekBlock("CameraSystem");
    requireOk(result, " could not Find CameraSystem Block ");
    result = scenarioFile->readIdULong("CameraHeapSize", cameraHeapSize);
    requireOk(result, " could not Find CameraHeapSize in CameraSystem Block ");
    result = scenarioFile->readIdString("CameraFileName", cameraFileName, 79);
    requireOk(result, " could not Find CameraFileName in CameraSystem Block ");
    cameraList = new CameraList;
    Assert(cameraList != nullptr, static_cast<uint32_t>(result), " no RAM for CameraList ");
    result = cameraList->init(cameraFileName, static_cast<int32_t>(cameraHeapSize));
    requireOk(result, " could start CameraSystem ");
    UpdateDisplay(0, 1, 30, 1, 20);

    result = scenarioFile->seekBlock("ObjectSystem");
    requireOk(result, " could not Find ObjectSystem Block ");
    result = scenarioFile->readIdULong("ObjectHeapSize", objectHeapSize);
    requireOk(result, " could not Find objectHeapSize in ObjectSystem Block ");
    result = scenarioFile->readIdULong("ObjectTypeHeapSize", objectTypeHeapSize);
    requireOk(result, " could not Find ObjectTypeHeapSzize in ObjectSystem Block ");
    result = scenarioFile->readIdULong("NumObjects", numObjects);
    requireOk(result, " could not Find NumObjects in ObjectSystem Block ");
    result = scenarioFile->readIdString("ObjectFileName", objectFileName, 79);
    requireOk(result, " could not Find ObjectFileName in ObjectSystem Block ");
    result = startObjects(objectFileName, static_cast<int32_t>(objectTypeHeapSize),
                          static_cast<int32_t>(objectHeapSize), static_cast<int32_t>(numObjects));
    requireOk(result, " could not Start ObjectSystem ");

    result = scenarioFile->seekBlock("SpriteSystem");
    requireOk(result, " could not Find SpriteSystem Block ");
    result = scenarioFile->readIdULong("SpriteHeapSize", spriteHeapSize);
    requireOk(result, " could not Find SpriteHeapSize in SpriteSystem Block ");
    uint32_t spriteManagerHeapSize = 0;
    result = scenarioFile->readIdULong("SpriteManagerHeapSize", spriteManagerHeapSize);
    requireOk(result, " could not Find SpriteManagerHeapSize in SpriteSystem Block ");
    uint32_t spriteDataHeapSize = 0;
    result = scenarioFile->readIdULong("SpriteDataHeapSize", spriteDataHeapSize);
    requireOk(result, " could not Find SpriteDataHeapSize in SpriteSystem Block ");
    result = scenarioFile->readIdString("SpriteFileName", spriteFileName, 79);
    requireOk(result, " could not Find SpriteFileName in SpriteSystem Block ");
    char shapeFileName[80];
    result = scenarioFile->readIdString("ShapeFileName", shapeFileName, 79);
    requireOk(result, " could not Find ShapeFileName in SpriteSystem Block ");
    spriteManager = new SpriteManager;
    Assert(spriteManager != nullptr, static_cast<uint32_t>(result), " no RAM for SpriteManager ");

    uint32_t legHeapSize = 0;
    uint32_t torsoHeapSize = 0;
    uint32_t rightArmHeapSize = 0;
    uint32_t leftArmHeapSize = 0;
    uint32_t totalMechs = 0;
    result = scenarioFile->seekBlock("SpriteManager");
    requireOk(result, " could not Find SpriteManager Block ");
    result = scenarioFile->readIdULong("LegHeapSize", legHeapSize);
    requireOk(result, " could not Find LegHeapSize in SpriteManager Block ");
    result = scenarioFile->readIdULong("TorsoHeapSize", torsoHeapSize);
    requireOk(result, " could not Find TorsoHeapSize in SpriteManager Block ");
    result = scenarioFile->readIdULong("RightArmHeapSize", rightArmHeapSize);
    requireOk(result, " could not Find RightArmHeapSize in SpriteManager Block ");
    result = scenarioFile->readIdULong("LeftArmHeapSize", leftArmHeapSize);
    requireOk(result, " could not Find LeftArmHeapSize in SpriteManager Block ");
    result = scenarioFile->readIdULong("TotalMechs", totalMechs);
    requireOk(result, " could not Find TotalMechs in SpriteManager Block ");

    // The shape heap: the FIT's total, halved for the small sprites, doubled for the 90-pixel ones on a big machine.
    const auto shapeHeapTotal =
        static_cast<int32_t>(spriteManagerHeapSize + legHeapSize + torsoHeapSize + rightArmHeapSize + leftArmHeapSize);
    const uint32_t totalPhysicalMemory = MCPort::TotalPhysicalMemory();
    int32_t shapeHeapSize = 0;

    if (use90PixelSprite == 0 || totalPhysicalMemory < 60000000 || force32MB != 0 || force16MB != 0)
    {
        shapeHeapSize = shapeHeapTotal >> 1;

        if (use90PixelSprite != 0 && 31999999 < totalPhysicalMemory && force16MB == 0)
        {
            shapeHeapSize = shapeHeapTotal + shapeHeapSize;
        }
    }
    else
    {
        shapeHeapSize = shapeHeapTotal * 2;
    }

    result = spriteManager->init(static_cast<uint32_t>(shapeHeapSize), spriteDataHeapSize, shapeFileName);
    requireOk(result, " could not Start SpriteManager ");

    appearanceTypeList = new AppearanceTypeList;

    if (appearanceTypeList != nullptr)
    {
        AppearanceTypeList::appearanceHeap = nullptr;
    }

    Assert(appearanceTypeList != nullptr, static_cast<uint32_t>(result), " no RAM for AppearanceList ");
    result = appearanceTypeList->init(spriteFileName, spriteHeapSize);
    // Faithful: tests the list, not the result.
    Assert(appearanceTypeList != nullptr, static_cast<uint32_t>(result), " could not start AppearanceList ");

    sensorSystemManager = new SensorSystemManager;
    Assert(sensorSystemManager != nullptr, 0, " Unable to init sensor system manager ");
    result = sensorSystemManager->init(gameSystemFile);
    requireOk(result, " could not start Sensor System Manager ");

    if (void* block = systemHeap->malloc(sizeof(PotentialContactManager)))
    {
        potentialContactManager = ::new (block) PotentialContactManager;
    }
    else
    {
        potentialContactManager = nullptr;
    }

    Assert(potentialContactManager != nullptr, static_cast<uint32_t>(result), " no RAM for PotentialContactManager ");
    result = potentialContactManager->init(scenarioFile);
    requireOk(result, " could not start PotentialContactManager ");
    UpdateDisplay(0, 1, 20, 1, 25);

    void* smokeBlock = systemHeap->malloc(sizeof(SmokeManager));

    if (smokeBlock == nullptr)
    {
        smokeManager = nullptr;
        return static_cast<int32_t>(0xdcdc0017);
    }

    smokeManager = ::new (smokeBlock) SmokeManager;
    result = smokeManager->init(scenarioFile);

    if (result != 0)
    {
        return result;
    }

    UpdateDisplay(0, 1, 30, 1, 35);

    collisionSystem = new CollisionSystem;

    if (collisionSystem == nullptr)
    {
        Assert(0, static_cast<uint32_t>(result), " no RAM for Collision System ");
    }

    result = collisionSystem->init(scenarioFile);
    requireOk(result, " could not start Collision System ");
    UpdateDisplay(0, 1, 30, 1, 37);

    //---------------------------------------------------------------------------------------------------------------
    // The terrain and its move maps (the editor passes a terrain of its own and gets no maps).
    if (terrainName == nullptr)
    {
        result = scenarioFile->seekBlock("TerrainSystem");
        requireOk(result, " could not find TerrainSystem block ");
        result = scenarioFile->readIdString("TerrainFileName", terrainFileName, 79);
        requireOk(result, " could not find TerrainFileName in TerrainSystem block ");
        land = new Terrain;

        if (land != nullptr)
        {
            land->Terrain::init();
        }

        Assert(land != nullptr, static_cast<uint32_t>(result), " no RAM for Terrain ");
        result = land->init(terrainFileName);
        requireOk(result, " could not start Terrain System ");
        UpdateDisplay(0, 1, 30, 1, 50);

        GameMap = new ScenarioMap;

        if (GameMap == nullptr)
        {
            Assert(0, static_cast<uint32_t>(result), " no RAM for Game Map ");
        }

        FullPathFileName mapFileName;
        mapFileName.init(terrainPath, terrainFileName, ".dat");
        auto* mapFile = new File;
        Assert(mapFile != nullptr, static_cast<uint32_t>(result), " no RAM for Map File");
        result = mapFile->open(mapFileName);
        requireOk(result, " could not start Game Map ");
        GameMap->init(mapFile);
        mapFile->close();
        delete mapFile;
        UpdateDisplay(0, 1, 30, 1, 60);

        GameObjectMap = new ObjectMap;
        Assert(GameObjectMap != nullptr, static_cast<uint32_t>(result), " no RAM for Game Object Map ");
        GameObjectMap->init(GameMap);
        PathManager = new MovePathManager;

        if (PathManager != nullptr)
        {
            PathManager->init();
        }

        PathFindMap = new MoveMap;
        Assert(PathFindMap != nullptr, static_cast<uint32_t>(result), " no RAM for Path Find Map ");
        PathFindMap->init(SimpleMovePathRange * 2 + 1, SimpleMovePathRange * 2 + 1);
        GlobalMoveMap = new GlobalMap;

        auto* globalMapFile = new File;
        FullPathFileName globalMapFileName;
        globalMapFileName.init(terrainPath, terrainFileName, ".gmm");
        result = globalMapFile->open(globalMapFileName);
        requireOk(result, " Could not open global Map ");
        GlobalMoveMap->init(globalMapFile);
        delete globalMapFile;
        UpdateDisplay(0, 1, 30, 1, 65);
        land->updateAllObjects();
        UpdateDisplay(0, 1, 30, 1, 70);
    }
    else
    {
        land = new Terrain;

        if (land != nullptr)
        {
            land->Terrain::init();
        }

        Assert(land != nullptr, static_cast<uint32_t>(result), " no RAM for Terrain ");
        MCStrCopy(terrainFileName, std::filesystem::path(terrainName).stem().string().c_str());
        result = land->init(terrainFileName);
        requireOk(result, " could not start Terrain System ");
    }

    char tacMapGifName[80];
    result = scenarioFile->readIdString("TacMapGifName", tacMapGifName, 79);

    if (result == 0)
    {
        Terrain::terrainTacticalMap->setRevealedBitmap(tacMapGifName);
    }

    //---------------------------------------------------------------------------------------------------------------
    // ABL: the libraries, then the scenario's own brain.
    result = scenarioFile->seekBlock("ABLibraries");

    if (result == 0)
    {
        int32_t libraryNumber = 0;

        while (result == 0)
        {
            char libraryId[32];
            char libraryName[512];
            std::snprintf(libraryId, sizeof(libraryId), "Library%d", libraryNumber++);
            result = scenarioFile->readIdString(libraryId, libraryName, 511);

            if (result == 0)
            {
                FullPathFileName libraryFileName;
                libraryFileName.init(missionPath, libraryName, ".abx");
                int32_t numErrors = 0;
                int32_t numLines = 0;

                if (ABLi_loadLibrary(libraryFileName, &numErrors, &numLines, nullptr, 0) != 0)
                {
                    char message[512];
                    std::snprintf(message, sizeof(message), " Cannot load ABL Library %s ", libraryName);
                    Fatal(0, message);
                }
            }
        }
    }

    UpdateDisplay(0, 1, 30, 1, 73);

    result = scenarioFile->seekBlock("Script");
    requireOk(result, " could not find Script Block ");
    result = scenarioFile->readIdString("ScenarioScript", scenarioScript, 79);
    requireOk(result, " could not find ScenarioScript in Script Block ");
    static char windowTitle[256];
    std::snprintf(windowTitle, sizeof(windowTitle), "%s - %s", appName, scenarioScript);

    // Port: SetWindowTextA -> the SDL window's title.
    if (auto* window = static_cast<SDL_Window*>(application->window()))
    {
        SDL_SetWindowTitle(window, windowTitle);
    }

    std::strcpy(WindowTitle, windowTitle);
    UpdateDisplay(0, 1, 30, 1, 75);

    FullPathFileName scriptFileName;
    scriptFileName.init(missionPath, scenarioScript, ".abl");
    int32_t numErrors = 0;
    int32_t numLines = 0;
    scenarioScriptHandle = ABLi_preProcess(scriptFileName, &numErrors, &numLines, nullptr, 0);
    Assert(-1 < scenarioScriptHandle, static_cast<uint32_t>(scenarioScriptHandle), " Bad Scenario Script ");
    scenarioBrain = new ABLModule;

    if (scenarioBrain == nullptr)
    {
        return static_cast<int32_t>(0xfaaf000b);
    }

    const int32_t brainResult = scenarioBrain->init(scenarioScriptHandle);
    Assert(brainResult == 0, static_cast<uint32_t>(result), " Error Starting Scenario Brain ");
    scenarioBrain->setName(const_cast<char*>("Scenario"));
    scenarioBrain->step = 1;
    scenarioBrainParams = new ABLParam;
    Assert(scenarioBrainParams != nullptr, 0, " No RAM for Scenario Brain Parameters ");
    scenarioBrainHandleMessage = scenarioBrain->findFunction(const_cast<char*>("handlemessage"), 1);

    //---------------------------------------------------------------------------------------------------------------
    // The warriors.
    result = scenarioFile->seekBlock("Warriors");
    Assert(result == 0, 0, " Could not find Warriors Block ");
    result = scenarioFile->readIdUChar("CaptureChance", captureChance);

    if (result != 0 || 4 < captureChance)
    {
        captureChance = 2;
    }

    result = scenarioFile->readIdULong("NumWarriors", numWarriors);
    requireOk(result, " Could not find NumWarriors in Warriors Block ");
    char brainParameterFileName[1024];
    result = scenarioFile->readIdString("BrainParameterFile", brainParameterFileName, 1023);
    const bool haveBrainParameters = (result == 0);
    result = scenarioFile->readIdUChar("CaptureChance", captureChance);

    if (result != 0 || 4 < captureChance)
    {
        captureChance = 2;
    }

    UpdateDisplay(0, 1, 30, 1, 77);

    NumMarines = 0;

    if (numWarriors != 0)
    {
        warriors = static_cast<MechWarrior**>(systemHeap->malloc((numWarriors + 1) * sizeof(MechWarrior*)));
        Assert(warriors != nullptr, 0, " no RAM for Warriors ");

        for (uint32_t i = 0; i < numWarriors + 1; i++)
        {
            warriors[i] = nullptr;
        }

        for (uint32_t i = 1; i < numWarriors + 1; i++)
        {
            char blockName[32];
            std::snprintf(blockName, sizeof(blockName), "Warrior%d", i);
            result = scenarioFile->seekBlock(blockName);
            Assert(result == 0, i, " Could not find Warrior Number Block ");
            char profileName[100];
            result = scenarioFile->readIdString("Profile", profileName, 99);
            Assert(result == 0, 0, " Could not find Warrior Profile in Warrior Number Block ");
            warriors[i] = newWarrior();
            Assert(warriors[i] != nullptr, 0, " No RAM for Warrior ");

            FullPathFileName profileFileName;
            profileFileName.init(warriorPath, profileName, ".fit");
            auto* profileFile = new FitIniFile;
            Assert(profileFile != nullptr, 0, " No RAM for Warrior Profile File ");
            int32_t profileResult = profileFile->open(profileFileName);

            if (profileResult == 0)
            {
                profileResult = warriors[i]->init(profileFile);
                Assert(profileResult == 0, static_cast<uint32_t>(profileResult), " Could not load Warrior Profile ");
            }
            else
            {
                // A saved game keeps its warriors' profiles in the temporary save folder.
                FitIniFile savedProfileFile;
                FullPathFileName savedProfileName;
                savedProfileName.init(saveTempPath, profileName, ".fit");
                profileResult = savedProfileFile.open(savedProfileName);
                Assert(profileResult == 0, static_cast<uint32_t>(profileResult),
                       " Could not open Warrior Profile File ");
                profileResult = warriors[i]->init(&savedProfileFile);
                Assert(profileResult == 0, static_cast<uint32_t>(profileResult), " Could not load Warrior Profile ");
            }

            profileFile->close();
            delete profileFile;

            warriors[i]->index = static_cast<int32_t>(i);
            char brainName[128];
            profileResult = scenarioFile->readIdString("Brain", brainName, 127);
            Assert(profileResult == 0, static_cast<uint32_t>(profileResult),
                   " Could not find Warrior Brain in Warrior Number Block ");
            warriors[i]->setBrainName(brainName);
            FullPathFileName brainFileName;
            brainFileName.init(warriorPath, brainName, ".abl");
            int32_t brainErrors = 0;
            int32_t brainLines = 0;
            const int32_t brainHandle = ABLi_preProcess(brainFileName, &brainErrors, &brainLines, nullptr, 0);
            Assert(-1 < brainHandle, static_cast<uint32_t>(brainHandle), " Could not start Warrior Brain ");
            const int32_t setBrainResult = warriors[i]->setBrain(brainHandle);
            Assert(setBrainResult == 0, static_cast<uint32_t>(setBrainResult), " Could Not Set Brain ");
            int notMineYet = 0;

            if (scenarioFile->readIdBoolean("NotMineYet", notMineYet) != 0)
            {
                notMineYet = 0;
            }

            warriors[i]->notMineYet = notMineYet;
        }
    }

    if (haveBrainParameters)
    {
        FullPathFileName parameterFileName;
        parameterFileName.init(warriorPath, brainParameterFileName, ".fit");
        auto* parameterFile = new FitIniFile;
        Assert(parameterFile != nullptr, 0, " No RAM for Brain Parameter File ");
        const int32_t openResult = parameterFile->open(parameterFileName);
        Assert(openResult == 0, static_cast<uint32_t>(openResult), " Could not open Brain Parameter File ");

        for (uint32_t i = 1; i <= numWarriors; i++)
        {
            warriors[i]->loadBrainParameters(parameterFile, static_cast<int32_t>(i));
        }

        parameterFile->close();
        delete parameterFile;
    }

    UpdateDisplay(0, 1, 30, 1, 80);

    //---------------------------------------------------------------------------------------------------------------
    // The parts.
    result = scenarioFile->seekBlock("Parts");
    requireOk(result, " Could not find Parts Block ");
    result = scenarioFile->readIdULong("NumParts", numParts);
    requireOk(result, " Could not find NumParts in Parts Block ");

    for (BaseObject*& mover : MoverRoster)
    {
        mover = nullptr;
    }

    if (numParts != 0)
    {
        parts = static_cast<Part*>(systemHeap->malloc((numParts + 1) * sizeof(Part)));
        Assert(parts != nullptr, 0, " no RAM for Parts ");
        std::memset(parts, 0, (numParts + 1) * sizeof(Part));

        for (int32_t i = 1; i < static_cast<int32_t>(numParts) + 1; i++)
        {
            UpdateDisplay(0, 1, 30, 1,
                          static_cast<int32_t>(static_cast<double>(i) / static_cast<int32_t>(numParts) * 10.0 + 80.0));
            Part& part = parts[i];
            char blockName[32];
            std::snprintf(blockName, sizeof(blockName), "Part%d", i);
            result = scenarioFile->seekBlock(blockName);
            requireOk(result, " Could not find PartNumber Block ");
            result = scenarioFile->readIdULong("ObjectNumber", part.objNumber);
            requireOk(result, " Could not find ObjectNumber in PartNumber Block ");
            result = scenarioFile->readIdULong("ControlType", part.controlType);
            requireOk(result, " Could not find ControlType in PartNumber Block ");
            result = scenarioFile->readIdULong("ControlDataType", part.controlDataType);
            requireOk(result, " Could not find ControlDataType in PartNumber Block ");
            result = scenarioFile->readIdString("ObjectProfile", part.profileName, 9);
            requireOk(result, " Could not find ObjectProfile in PartNumber Block ");
            result = scenarioFile->readIdULong("Pilot", part.pilot);
            requireOk(result, " Could not find Pilot in PartNumber Block ");
            result = scenarioFile->readIdFloat("PositionX", part.position[0]);
            requireOk(result, " Could not find PositionX in PartNumber Block ");
            result = scenarioFile->readIdFloat("PositionY", part.position[1]);
            requireOk(result, " Could not find PositionY in PartNumber Block ");
            result = scenarioFile->readIdFloat("PositionZ", part.position[2]);
            requireOk(result, " Could not find PositionZ in PartNumber Block ");
            result = scenarioFile->readIdFloat("Rotation", part.rotation);
            requireOk(result, " Could not find Rotation in PartNumber Block ");
            char teamId = 0;
            result = scenarioFile->readIdChar("TeamId", teamId);
            part.teamId = static_cast<int8_t>(teamId);
            requireOk(result, " Could not find TeamId in PartNumber Block ");

            if (part.teamId == 0 || part.teamId == 2)
            {
                part.alignment = 1;
            }
            else if (part.teamId == 1)
            {
                part.alignment = -1;
            }
            else
            {
                Fatal(0, " Bad TeamId for Part ");
            }

            char commanderId = 0;
            result = scenarioFile->readIdChar("CommanderId", commanderId);

            if (result == 0)
            {
                part.commanderId = commanderId;
                result = 0;
            }
            else
            {
                result = scenarioFile->readIdLong("CommanderId", part.commanderId);
                requireOk(result, " Could not find CommanderId in PartNumber Block ");
            }

            result = scenarioFile->readIdULong("Gesture", part.gestureId);
            requireOk(result, " Could not find Gesture in PartNumber Block ");

            if (scenarioFile->readIdLong("PaintScheme", part.paintScheme) != 0)
            {
                part.paintScheme = -1;
            }

            result = scenarioFile->readIdFloat("Velocity", part.velocity);
            requireOk(result, " Could not find Velocity in PartNumber Block ");
            result = scenarioFile->readIdLong("Active", part.active);
            requireOk(result, " Could not find Active Flag in PartNumber Block ");
            result = scenarioFile->readIdLong("Exists", part.exists);
            requireOk(result, " Could not find Exists Flag in PartNumber Block ");
            // Read twice in the original.
            result = scenarioFile->readIdChar("MyIcon", part.myIcon);
            requireOk(result, " Could not find MyIcon in PartNumber Block ");
            result = scenarioFile->readIdChar("MyIcon", part.myIcon);
            requireOk(result, " Could not find MyIcon in PartNumber Block ");
            int captureable = 0;
            result = scenarioFile->readIdBoolean("Captureable", captureable);
            part.captureable = (result != 0) ? 0 : captureable;

            partCreateTime = -1.0f;
            InfluenceTime = 0.0f;
            createPartObject(i);
        }
    }

    UpdateDisplay(0, 1, 20, 1, 90);

    //---------------------------------------------------------------------------------------------------------------
    // Trains, elemental carriers and buses: parts that carry other parts.
    trainManager = nullptr;
    result = scenarioFile->seekBlock("Trains");

    if (result == 0)
    {
        int32_t numTrains = 0;
        trainManager = new TrainManager;

        if (trainManager != nullptr)
        {
            trainManager->init();
        }

        Assert(trainManager != nullptr, 0, "Couldn't create manager");
        result = scenarioFile->readIdLong("NumTrains", numTrains);
        requireOk(result, " Could not find number of trains");

        for (int32_t trainNumber = 0; trainNumber < numTrains; trainNumber++)
        {
            char blockName[32];
            std::snprintf(blockName, sizeof(blockName), "Train%d", trainNumber);
            result = scenarioFile->seekBlock(blockName);
            requireOk(result, " Could not find train block");
            int32_t numCars = 0;
            result = scenarioFile->readIdLong("NumCars", numCars);
            requireOk(result, " Could not find number of cars in train block");
            Assert(0 < numCars, static_cast<uint32_t>(result), " Need at least one car in train...");
            Train* train = trainManager->CreateTrain();

            for (int32_t carNumber = 0; carNumber < numCars; carNumber++)
            {
                std::snprintf(blockName, sizeof(blockName), "Car%d", carNumber);
                int32_t carPart = 0;
                result = scenarioFile->readIdLong(blockName, carPart);
                requireOk(result, " Could not find a car in train block");
                Assert(carPart <= static_cast<int32_t>(numParts), 0, "Illegal part number for train car");
                auto* car = static_cast<TrainCar*>(parts[carPart].object);
                Assert(car->objectClass == TRAINCAR, 0, "Car in train block isn't a traincar!");
                train->AddCar(car);
                car->setPartId(trainNumber, carNumber);

                if (carNumber == 0)
                {
                    // The lead car's part sets the train's speed (clamped to its top speed) and direction.
                    const float velocity = parts[carPart].velocity;

                    if (std::fabs(velocity) <= train->maxSpeed)
                    {
                        train->desiredSpeed = velocity;
                    }
                    else if (velocity <= 0.0f)
                    {
                        train->desiredSpeed = -train->maxSpeed;
                    }
                    else
                    {
                        train->desiredSpeed = train->maxSpeed;
                    }

                    const float rotation = parts[carPart].rotation;

                    if (rotation != 45.0f && rotation != -45.0f && rotation != 135.0f && rotation != -135.0f)
                    {
                        Fatal(static_cast<int32_t>(rotation), " Train Rotation Invalid.  (must be 45,-45,135,-135) ");
                    }

                    train->trackDirection = static_cast<int32_t>(rotation);
                }
            }
        }
    }

    UpdateDisplay(0, 1, 20, 1, 92);

    result = scenarioFile->seekBlock("Elemental Carriers");

    if (result == 0)
    {
        int32_t numCarriers = 0;
        result = scenarioFile->readIdLong("Carriers", numCarriers);
        requireOk(result, " Could not find number of carriers");

        for (int32_t carrierNumber = 0; carrierNumber < numCarriers; carrierNumber++)
        {
            char blockName[32];
            std::snprintf(blockName, sizeof(blockName), "ECarrier%d", carrierNumber);
            result = scenarioFile->seekBlock(blockName);
            requireOk(result, " Could not find carrier block");
            int32_t partNumber = 0;
            result = scenarioFile->readIdLong("Carrier", partNumber);
            requireOk(result, " Could not read carrier in carrier block");
            Assert(partNumber < static_cast<int32_t>(numParts), static_cast<uint32_t>(partNumber),
                   "Illegal part number for elemental carrier");
            auto* carrier = static_cast<GroundVehicle*>(parts[partNumber].object);
            Assert(carrier != nullptr && carrier->objectClass == GROUNDVEHICLE && carrier->elementalCarrier != 0, 0,
                   "Illegal carrier object");

            for (int32_t i = 0; i < 10; i++)
            {
                std::snprintf(blockName, sizeof(blockName), "Elemental%d", i);
                result = scenarioFile->readIdLong(blockName, partNumber);

                if (result != 0)
                {
                    break;
                }

                auto* elemental = static_cast<Elemental*>(parts[partNumber].object);
                Assert(elemental != nullptr && elemental->objectClass == ELEMENTAL, 0, "Illegal elemental object");
                carrier->elementals[i] = elemental;
                elemental->transport = carrier;
            }
        }
    }

    UpdateDisplay(0, 1, 20, 1, 93);

    result = scenarioFile->seekBlock("BusBlock");

    if (result == 0)
    {
        int32_t numBuses = 0;
        result = scenarioFile->readIdLong("Buses", numBuses);
        requireOk(result, " Could not find number of buses");

        for (int32_t busNumber = 0; busNumber < numBuses; busNumber++)
        {
            char blockName[32];
            std::snprintf(blockName, sizeof(blockName), "Bus%d", busNumber);
            result = scenarioFile->seekBlock(blockName);
            requireOk(result, " Could not find bus block");
            int32_t number = 0;
            result = scenarioFile->readIdLong("Bus", number);
            requireOk(result, " Could not read carrier in carrier block");
            Assert(number <= static_cast<int32_t>(numParts), static_cast<uint32_t>(number),
                   "Illegal part number for elemental carrier");
            auto* bus = static_cast<GroundVehicle*>(parts[number].object);
            Assert(bus != nullptr && bus->objectClass == GROUNDVEHICLE, 0, "Illegal bus object");

            for (int32_t seat = 0; seat < 4 && seat < static_cast<int32_t>(bus->seats); seat++)
            {
                std::snprintf(blockName, sizeof(blockName), "Passenger%d", seat);
                result = scenarioFile->readIdLong(blockName, number);

                if (result != 0)
                {
                    break;
                }

                Assert(number <= static_cast<int32_t>(numWarriors), 0, "Illegal passenger");
                bus->passengers[seat] = warriors[number];
            }
        }
    }

    //---------------------------------------------------------------------------------------------------------------
    // The objectives.
    result = scenarioFile->seekBlock("Objectives");
    requireOk(result, " Could not find Objective Block ");

    if (scenarioFile->readIdLong("TimeLeft", timeLimit) != 0)
    {
        timeLimit = -1;
    }

    twoMinuteWarningPlayed = 0;
    thirtySecondWarningPlayed = 0;
    result = scenarioFile->readIdULong("NumObjectives", numObjectives);
    requireOk(result, " Could not find numObjectives in Objective Block ");
    Assert(numObjectives < 10, static_cast<uint32_t>(result), " Too Many Objectives ");

    if (MPlayer == nullptr)
    {
        innerSphereTeam->firstObjective = 0;
        innerSphereTeam->numObjectives = numObjectives;
    }
    else
    {
        uint32_t numInnerSphereObjectives = 0;
        uint32_t numClanObjectives = 0;
        result = scenarioFile->readIdULong("NumInnerSphereObjectives", numInnerSphereObjectives);
        requireOk(result, " Could not find NumInnerSphereObjectives in Objective Block ");
        result = scenarioFile->readIdULong("NumClanObjectives", numClanObjectives);
        requireOk(result, " Could not find NumClanObjectives in Objective Block ");
        Assert(numInnerSphereObjectives + numClanObjectives == numObjectives, static_cast<uint32_t>(result),
               " Incorrect # of objectives ");
        innerSphereTeam->firstObjective = 0;
        innerSphereTeam->numObjectives = numInnerSphereObjectives;
        clanTeam->firstObjective = static_cast<int32_t>(numInnerSphereObjectives);
        clanTeam->numObjectives = numClanObjectives;
    }

    if (numObjectives != 0)
    {
        objectives = static_cast<ScenarioObjective*>(systemHeap->malloc(MAX_OBJECTIVES * sizeof(ScenarioObjective)));
        Assert(objectives != nullptr, 0, " no RAM for Objectives ");
        std::memset(objectives, 0, MAX_OBJECTIVES * sizeof(ScenarioObjective));

        for (int32_t i = 0; i < static_cast<int32_t>(numObjectives); i++)
        {
            UpdateDisplay(
                0, 1, 20, 1,
                static_cast<int32_t>(static_cast<double>(i) / static_cast<int32_t>(numObjectives) * 5.0 + 93.0));
            ScenarioObjective& objective = objectives[i];
            char blockName[32];
            std::snprintf(blockName, sizeof(blockName), "Objective%d", i);
            result = scenarioFile->seekBlock(blockName);
            Assert(result == 0, static_cast<uint32_t>(i), " Could not find ObjectiveNumber Block ");
            result = scenarioFile->readIdString("Name", objective.name, 79);
            requireOk(result, " Could not find Name in Objective Block ");
            result = scenarioFile->readIdULong("Type", objective.type);
            requireOk(result, " Could not find Type in Objective Block ");
            result = scenarioFile->readIdFloat("TimeLeft", objective.timeLeft);
            requireOk(result, " Could not find TimeLeft in Objective Block ");
            result = scenarioFile->readIdULong("Status", objective.status);
            requireOk(result, " Could not find Status in Objective Block");

            if (scenarioFile->readIdLong("Points", objective.points) != 0)
            {
                objective.points = 0;
            }

            if (scenarioFile->readIdFloat("Radius", objective.radius) != 0)
            {
                objective.radius = 0.0f;
            }

            objective.position[0] = -99.0f;
            objective.position[1] = -99.0f;
            objective.position[2] = -99.0f;
        }

        for (int32_t i = static_cast<int32_t>(numObjectives); i < MAX_OBJECTIVES; i++)
        {
            objectives[i].type = UNUSED_OBJECTIVE;
            objectives[i].status = UNUSED_OBJECTIVE;
        }
    }

    //---------------------------------------------------------------------------------------------------------------
    // Each commander's support strikes and groups ("Commander%dGroup:%d": the part numbers of its mates).
    for (int32_t commanderId = 0; commanderId < NumCommanders; commanderId++)
    {
        Commander* commander = CommanderTable[commanderId];
        int32_t groupId = 0;
        UpdateDisplay(0, 1, 30, 1, 98);
        char blockName[64];
        std::snprintf(blockName, sizeof(blockName), "Commander:%d", commanderId);

        if (scenarioFile->seekBlock(blockName) == 0)
        {
            int32_t strikes = 0;
            commander->setNumSmallStrikes(scenarioFile->readIdLong("NumSmallStrikes", strikes) == 0 ? strikes : 0);
            commander->setNumLargeStrikes(scenarioFile->readIdLong("NumLargeStrikes", strikes) == 0 ? strikes : 0);
            commander->setNumSensorStrikes(scenarioFile->readIdLong("NumSensorStrikes", strikes) == 0 ? strikes : 0);
            commander->setNumCameraDrones(scenarioFile->readIdLong("NumCameraDrones", strikes) == 0 ? strikes : 0);
        }

        std::snprintf(blockName, sizeof(blockName), "Commander%dGroup:%d", commanderId, groupId);
        int32_t groupResult = scenarioFile->seekBlock(blockName);

        while (groupResult == 0)
        {
            bool pointChosen = false;
            int32_t mates[MAX_MOVERGROUP_COUNT];
            groupResult = scenarioFile->readIdLongArray("Mates", mates, MAX_MOVERGROUP_COUNT);
            Assert(groupResult == 0, static_cast<uint32_t>(groupResult),
                   " could not find Mates in Group in Scenario File ");

            for (int32_t i = 0; i < MAX_MOVERGROUP_COUNT; i++)
            {
                if (mates[i] <= 0)
                {
                    continue;
                }

                Part& mate = parts[mates[i]];
                const int32_t partId = 0x200 + commanderId * 0x180 + groupId * MAX_MOVERGROUP_COUNT + i;
                mate.object->setPartId(partId);

                if (mate.exists == 0)
                {
                    createdPartRoster[currentCreatorPart].partId = partId;
                    createdPartRoster[currentCreatorPart].created = 0;
                    currentCreatorPart++;
                }

                commander->getGroup(groupId)->add(static_cast<Mover*>(mate.object));

                if (!pointChosen)
                {
                    commander->getGroup(groupId)->selectPoint(1);
                    pointChosen = true;
                    commander->setTeam(commander->getGroup(groupId)->getPoint()->getTeam());
                }
            }

            if (MPlayer == nullptr && commanderId == 1)
            {
                CommanderTable[1]->getGroup(groupId)->setDisbandOnNoPoint(0);
            }

            groupId++;
            std::snprintf(blockName, sizeof(blockName), "Commander%dGroup:%d", commanderId, groupId);
            groupResult = scenarioFile->seekBlock(blockName);
        }
    }

    clanTeam->buildRoster(this);
    innerSphereTeam->buildRoster(this);

    if (alliedTeam != nullptr)
    {
        alliedTeam->buildRoster(this);
    }

    if (MPlayer == nullptr)
    {
        HomeCommander->setNetPlayerId(0);
    }

    HomeCommander->addToGUI(1);

    if (MPlayer != nullptr)
    {
        for (int32_t i = 0; i < NumCommanders; i++)
        {
            if (CommanderTable[i] != HomeCommander)
            {
                CommanderTable[i]->addToGUI(0);
            }
        }
    }

    scenarioTime = 0.0f;
    MissionStartTime = 0;
    runningTime = 0.0f;
    actualTime = 0.0f;
    gameSystemFile->close();
    delete gameSystemFile;
    UpdateDisplay(0, 1, 30, 1, 100);

    eye = cameraList->activateAllReady();

    if (MPlayer != nullptr)
    {
        eye->changeTarget(MPlayer->localMovers[0], 1);
    }

    // The 'Mechs start with the damage their loadouts carried over.
    for (BaseObject* object = innerSphereMechList->head; object != nullptr; object = object->next)
    {
        if (object->objectClass == BATTLEMECH)
        {
            static_cast<BattleMech*>(object)->damageLoadedComponents();
        }
    }

    for (BaseObject* object = clanMechList->head; object != nullptr; object = object->next)
    {
        if (object->objectClass == BATTLEMECH)
        {
            static_cast<BattleMech*>(object)->damageLoadedComponents();
        }
    }

    if (scenarioFile->seekBlock("Output") == 0)
    {
        hasOutputBlock = 1;
    }

    scenarioFile->close();
    delete scenarioFile;
    scenarioFile = nullptr;

    scenarioEndTurn = -1;
    startUpTurns = 10;
    startMusic = 1;

    if (MPlayer != nullptr)
    {
        MPlayer->chatCallback = ScenarioChatCallback;

        if (MPlayer->isServer != 0)
        {
            for (int32_t& checkedIn : MPlayer->playerCheckedIn)
            {
                checkedIn = 0;
            }
        }

        MPlayer->sendPlayerCheckIn();
    }

    startingUp = 1;
    startUpCountdown = 100;
    std::free(connectShape);
    connectShape = nullptr;
    return 0;
}

auto Scenario::run() -> int32_t
{
    if (MPlayer != nullptr && MPlayer->inMission == 0)
    {
        MPlayer->processReceiveList();
        return 0;
    }

    if (gamePaused != 0)
    {
        cameraList->update();
        return static_cast<int32_t>(scenarioResult);
    }

    update();
    cameraList->update();
    land->update();
    craterManager->update();
    PathManager->update();

    if (trainManager != nullptr)
    {
        trainManager->UpdateTrains();
    }

    objectList->update();
    clanTeam->updateSensors();

    if (alliedTeam != nullptr)
    {
        alliedTeam->updateSensors();
    }

    innerSphereTeam->updateSensors();
    potentialContactManager->updateStatus();

    if (collisionSwitch != 0)
    {
        collisionSystem->checkObjects();
    }

    if (turn < 2)
    {
        startObjectiveTimers();
    }

    if (MPlayer == nullptr)
    {
        scenarioBrain->execute(scenarioBrainParams);
        scenarioResult = static_cast<uint32_t>(scenarioBrain->returnVal);
    }
    else
    {
        CurMultiplayCode = 0;
        CurMultiplayParam = 0;
        scenarioBrain->execute(scenarioBrainParams);
        CurMultiplayCode = 0;
        CurMultiplayParam = 0;

        if (MPlayer->isServer == 0)
        {
            scenarioResult = static_cast<uint32_t>(MPlayer->scenarioResult);
        }
        else
        {
            scenarioResult = static_cast<uint32_t>(scenarioBrain->returnVal);

            if (scenarioResult != 0)
            {
                MPlayer->sendEndScenario(0, static_cast<int32_t>(scenarioResult));
            }
        }
    }

    if (MPlayer != nullptr)
    {
        if (MPlayer->isServer != 0)
        {
            MPlayer->updateClients();
        }

        MPlayer->processReceiveList();
    }

    return static_cast<int32_t>(scenarioResult);
}

auto Scenario::destroy() -> void
{
    // Faithful: the id wraps to a short when the timer is removed (0x249f1 -> 0x49f1).
    for (uint32_t id = 0x249f1; id < numObjectives + 0x249f1u; id++)
    {
        application->RemoveTimer(application, static_cast<int16_t>(id));
    }

    endingScenario = 1;

    Assert(collisionSystem != nullptr, 0, " collisionSystem already NULL ");

    if (collisionSystem != nullptr)
    {
        collisionSystem->destroy();
        delete collisionSystem;
    }

    collisionSystem = nullptr;

    Assert(ElementList != nullptr, 0, " ElementList already NULL ");

    if (ElementList != nullptr)
    {
        ElementList->free();
        delete ElementList;
    }

    ElementList = nullptr;
    ElementPool::free();

    if (craterManager != nullptr)
    {
        craterManager->destroy();
        systemHeap->free(craterManager);
        craterManager = nullptr;
    }

    Assert(land != nullptr, 0, " land already NULL ");
    delete land;
    land = nullptr;

    Assert(scenarioObjectList != nullptr, 0, " scenarioObjectList already NULL ");

    if (scenarioObjectList != nullptr)
    {
        while (ObjectQueueNode* node = scenarioObjectList->head)
        {
            ObjectQueueNode* next = node->next;
            node->destroy();
            delete node;
            scenarioObjectList->head = next;
        }

        scenarioObjectList->tail = nullptr;
        scenarioObjectList->head = nullptr;
        delete scenarioObjectList;
    }

    scenarioObjectList = nullptr;
    stopObjects();

    if (sensorSystemManager != nullptr)
    {
        sensorSystemManager->destroy();
        delete sensorSystemManager;
        sensorSystemManager = nullptr;
        SensorSystem::sortList = nullptr;
        ContactSortList = nullptr;
    }

    if (potentialContactManager != nullptr)
    {
        potentialContactManager->destroy();
        systemHeap->free(potentialContactManager);
        potentialContactManager = nullptr;
    }

    if (objectTypeManager != nullptr)
    {
        objectTypeManager->destroy();
        delete objectTypeManager;
        objectTypeManager = nullptr;
    }

    if (trainManager != nullptr)
    {
        // Faithful: destroyed twice (once here, once by the inlined destructor).
        trainManager->destroy();
        trainManager->destroy();
        delete trainManager;
        trainManager = nullptr;
    }

    destroyMechShadows();
    systemHeap->free(tempBuffer);
    tempBuffer = nullptr;

    Assert(parts != nullptr, 0, " parts already NULL ");
    systemHeap->free(parts);
    parts = nullptr;
    Assert(objectives != nullptr, 0, " parts already NULL ");
    systemHeap->free(objectives);
    objectives = nullptr;

    for (int32_t i = 0; i < NumCommanders; i++)
    {
        if (CommanderTable[i] != nullptr)
        {
            CommanderTable[i]->Commander::destroy();
            delete CommanderTable[i];
        }

        CommanderTable[i] = nullptr;
    }

    deleteTeam(clanTeam);
    clanTeam = nullptr;
    deleteTeam(alliedTeam);
    alliedTeam = nullptr;
    deleteTeam(innerSphereTeam);
    innerSphereTeam = nullptr;
    TeamTable[0] = nullptr;
    TeamTable[1] = nullptr;
    TeamTable[2] = nullptr;

    for (int32_t i = 0; i < 6; i++)
    {
        if (sensorContactShapes[i] != nullptr)
        {
            systemHeap->free(sensorContactShapes[i]);
            sensorContactShapes[i] = nullptr;
        }
    }

    if (smokeManager != nullptr)
    {
        smokeManager->destroy();
        systemHeap->free(smokeManager);
        smokeManager = nullptr;
    }

    Assert(appearanceTypeList != nullptr, 0, " appearanceTypeList already NULL ");

    if (appearanceTypeList != nullptr)
    {
        appearanceTypeList->destroy();
        delete appearanceTypeList;
    }

    appearanceTypeList = nullptr;
    Assert(spriteManager != nullptr, 0, " spriteManager already NULL ");

    if (spriteManager != nullptr)
    {
        spriteManager->destroy();
        delete spriteManager;
    }

    spriteManager = nullptr;
    Assert(cameraList != nullptr, 0, " cameraList already NULL ");
    delete cameraList;
    cameraList = nullptr;
    eye = nullptr;

    Assert(scenarioBrainParams != nullptr, 0, " scenarioParams already NULL ");
    delete scenarioBrainParams;
    scenarioBrainParams = nullptr;

    if (oldPalette != nullptr)
    {
        if (gamePalette != nullptr)
        {
            gamePalette->destroy();
            delete gamePalette;
        }

        gamePalette = oldPalette;
        oldPalette = nullptr;
    }

    if (GameMap != nullptr)
    {
        GameMap->destroy();
        delete GameMap;
        GameMap = nullptr;
    }

    if (GameObjectMap != nullptr)
    {
        GameObjectMap->destroy();
        delete GameObjectMap;
        GameObjectMap = nullptr;
    }

    if (GlobalMoveMap != nullptr)
    {
        GlobalMoveMap->destroy();
        delete GlobalMoveMap;
        GlobalMoveMap = nullptr;
    }

    if (PathFindMap != nullptr)
    {
        PathFindMap->destroy();
        delete PathFindMap;
        PathFindMap = nullptr;
    }

    if (PathManager != nullptr)
    {
        PathManager->destroy();
        delete PathManager;
        PathManager = nullptr;
    }

    systemHeap->free(Fire::maxFiresList);
    Fire::maxFiresList = nullptr;

    destroyWarriors();

    if (scenarioBrain != nullptr)
    {
        scenarioBrain->destroy();
        delete scenarioBrain;
        scenarioBrain = nullptr;
    }

    if (openList != nullptr)
    {
        // Faithful: destroyed twice (once here, once by the inlined destructor).
        openList->destroy();
        openList->destroy();
        delete openList;
    }

    openList = nullptr;
    ABLi_close();
    std::free(waypointMarkers);
    waypointMarkers = nullptr;
}

auto Scenario::destroyWarriors() -> void
{
    if (warriors == nullptr)
    {
        return;
    }

    for (uint32_t i = 0; i < numWarriors + 1; i++)
    {
        if (warriors[i] != nullptr)
        {
            deleteWarrior(warriors[i]);
            warriors[i] = nullptr;
        }
    }

    systemHeap->free(warriors);
    warriors = nullptr;
    numWarriors = 0;
}

auto Scenario::createPartObject(int32_t partNumber) -> void
{
    Part& part = parts[partNumber];

    if (part.destroyed != 0 || part.object != nullptr)
    {
        return;
    }

    GameObject* object = createObject(static_cast<int32_t>(part.objNumber));
    part.object = object;

    if (object == nullptr)
    {
        char message[256];
        std::snprintf(message, sizeof(message), " Couldnt create object number %d  which is part Number %d",
                      part.objNumber, partNumber);
        Fatal(static_cast<int32_t>(part.objNumber), message);
    }

    object->setAwake(part.active);

    if (std::strcmp(part.profileName, "NONE") != 0)
    {
        FullPathFileName profileFileName;
        profileFileName.init(profilePath, part.profileName, ".fit");
        auto* profileFile = new FitIniFile;

        if (profileFile == nullptr)
        {
            Fatal(static_cast<int32_t>(0xfaaf0001), " Profile File ");
        }

        if (profileFile->open(profileFileName) == 0)
        {
            if (object->init(profileFile) != 0)
            {
                Fatal(static_cast<int32_t>(0xfaaf0007), " Bad Profile File ");
            }
        }
        else
        {
            FitIniFile savedProfileFile;
            FullPathFileName savedProfileName;
            savedProfileName.init(saveTempPath, part.profileName, ".fit");
            const int32_t openResult = savedProfileFile.open(savedProfileName);

            if (openResult != 0)
            {
                Fatal(openResult, nullptr);
            }

            if (object->init(&savedProfileFile) != 0)
            {
                Fatal(static_cast<int32_t>(0xfaaf0007), " Bad Profile File ");
            }
        }

        profileFile->close();
        delete profileFile;
    }

    Team* team = nullptr;

    if (part.teamId == 0)
    {
        team = innerSphereTeam;
    }
    else if (part.teamId == 1)
    {
        team = clanTeam;
    }
    else if (part.teamId == 2)
    {
        team = alliedTeam;
    }

    const ObjectClass objectClass = object->objectClass;

    if (objectClass == BATTLEMECH)
    {
        auto* mech = static_cast<BattleMech*>(object);
        mech->setPilot(warriors[part.pilot]);
        mech->setTeam(team);
        mech->calcWeaponEffectiveness(1);
        mech->calcWeaponEffectiveness(0);
        mech->calcWeaponRangeRatings();
        mech->captureable = part.captureable;
        const int32_t paintScheme = (part.paintScheme == -1) ? warriors[part.pilot]->paintScheme : part.paintScheme;
        static_cast<MechActor*>(mech->appearance)->fadeTableIndex = paintScheme;
    }
    else if (objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL)
    {
        auto* mover = static_cast<Mover*>(object);
        mover->setPilot(warriors[part.pilot]);
        mover->setTeam(team);
        mover->calcWeaponRangeRatings();

        // The original tests +0x8b8 of both: a vehicle's gvAppearance flag, and an elemental's field there, which
        // MCX.EXE only ever sets to 0.
        if (objectClass == GROUNDVEHICLE && static_cast<GroundVehicle*>(mover)->gvAppearance != 0)
        {
            static_cast<GVAppearance*>(mover->appearance)->fadeTableIndex =
                (part.paintScheme == -1) ? warriors[part.pilot]->paintScheme : part.paintScheme;
        }
    }

    object->setControl(part.controlType, part.controlDataType, -1);
    vector_3d position(part.position[0], part.position[1], part.position[2]);
    object->setPosition(position);

    if (objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL || objectClass == MOVER)
    {
        static_cast<Mover*>(object)->setLastValidPosition(position);
    }

    frame_of_ref frame;
    frame.reset_to_world_frame();
    const double radians = part.rotation * DEGREES_TO_RADIANS;
    rotateAboutK(frame, static_cast<float>(std::sin(radians)), static_cast<float>(std::cos(radians)));
    object->setFrame(frame);

    if (objectClass == BATTLEMECH)
    {
        auto* actor = static_cast<MechActor*>(object->getAppearance());

        if (actor != nullptr)
        {
            actor->setGesture(part.gestureId);
        }

        if (part.alignment == homeTeam->alignment)
        {
            actor->preloadGestures(static_cast<int32_t>(part.gestureId), part.rotation);
        }
    }
    else if (objectClass == ELEMENTAL)
    {
        auto* actor = static_cast<ElementalActor*>(object->getAppearance());

        if (actor != nullptr)
        {
            actor->setGesture(part.gestureId);
        }

        actor->preloadGestures(static_cast<int32_t>(part.gestureId), part.rotation);
    }

    // The part number is kept in the object's id.
    object->idNumber = static_cast<uint32_t>(partNumber);

    if (MPlayer != nullptr)
    {
        MPlayer->addToMoverRoster(static_cast<Mover*>(object));
        MPlayer->addToPlayerMoverRoster(part.commanderId, static_cast<Mover*>(object));

        if (part.commanderId == MPlayer->checkInId)
        {
            MPlayer->addToLocalMovers(static_cast<Mover*>(object));
        }
    }

    if (part.exists == 0)
    {
        // Not in play yet: the script brings it in with createScenarioObject.
        object->setCommanderId(part.commanderId);
        object->setAlignment(part.alignment);

        if (ObjectQueueNode* node = scenarioObjectList->head)
        {
            node->addNode(object);
        }

        return;
    }

    if (objectClass < BATTLEMECH || ELEMENTAL < objectClass)
    {
        object->setExists(1);

        if (ObjectQueueNode* node = objectList->head)
        {
            node->addNode(object);
        }
    }
    else
    {
        object->setCommanderId(part.commanderId);
        object->setAlignment(part.alignment);
        ObjectQueueNode* list = (part.alignment == -1) ? clanMechList : innerSphereMechList;

        if (list != nullptr)
        {
            list->addNode(object);
        }

        object->setPotentialContact(objectClass == ELEMENTAL ? 2 : 1);
        object->setExists(1);
    }

    GameObjectMap->addObject(object);
}

auto Scenario::createScenarioObject(int32_t partId) -> void
{
    ObjectQueue* queue = scenarioObjectList;
    BaseObject* object = nullptr;
    object = queue->traverse(object);

    while (object != nullptr && object->partId != partId)
    {
        object = queue->traverse(object);
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
    for (ObjectQueueNode* node = queue->head; node != nullptr; node = node->next)
    {
        BaseObject* prev = nullptr;
        BaseObject* current = node->head;

        while (current != nullptr && current != object)
        {
            prev = current;
            current = current->next;
        }

        if (current != nullptr)
        {
            node->removeNode(prev, current);
            break;
        }
    }

    auto* gameObject = static_cast<GameObject*>(object);
    ObjectQueueNode* list = (gameObject->getAlignment() != -1) ? innerSphereMechList : clanMechList;

    if (list != nullptr)
    {
        list->addNode(object);
    }

    gameObject->setPotentialContact(object->objectClass == ELEMENTAL ? 2 : 1);
    GameObjectMap->addObject(gameObject);
    gameObject->setExists(1);

    for (int32_t i = 0; i < currentCreatorPart; i++)
    {
        if (createdPartRoster[i].partId == object->partId)
        {
            createdPartRoster[i].created = 1;
            return;
        }
    }
}

auto Scenario::destroyPartObject(int32_t partNumber) -> void
{
    Part& part = parts[partNumber];
    auto* object = static_cast<GameObject*>(part.object);

    if (object == nullptr)
    {
        return;
    }

    object->getObjectType()->handleDestruction(object, nullptr);

    for (ObjectQueueNode* node = objectList->head; node != nullptr && node->remove(object) == 0; node = node->next)
    {
    }

    part.destroyed = 1;
    part.active = 0;
    part.exists = 0;
}

auto Scenario::objectInArea(GameObject* object, int32_t areaNumber) -> int
{
    if (areas == nullptr || numAreas <= areaNumber)
    {
        return 0;
    }

    const ScenarioArea& area = areas[areaNumber];
    // OB-056: the area's size (a circle's radius, a rectangle's extent) is an uninitialised local in MCX.EXE; the
    // port reads it as 0. Never matters: nothing fills the areas, so numAreas stays 0.
    const float size = 0.0f;
    const vector_3d position = object->getPosition();

    if (area.areaType == 0)
    {
        const float dx = area.coords[0] - position.x;
        const float dy = area.coords[1] - position.y;
        const float dz = area.coords[2] - position.z;

        if (std::sqrt(dx * dx + dy * dy + dz * dz) < size)
        {
            return 1;
        }
    }
    else if (area.areaType == 1)
    {
        // Faithful: the rectangle test compares x with y and y with z.
        if (size + area.coords[0] < position.x && position.x < position.y + area.coords[0] &&
            position.x + area.coords[1] < position.y && position.y < position.z + area.coords[1])
        {
            return 1;
        }
    }

    return 0;
}

auto Scenario::startObjectiveTimers() -> void
{
    for (int32_t i = 0; i < static_cast<int32_t>(numObjectives); i++)
    {
        if (0.0f < objectives[i].timeLeft)
        {
            setObjectiveTimer(i, objectives[i].timeLeft * 1000.0f);
        }
    }
}

auto Scenario::setObjectiveTimer(int32_t objectiveNumber, float time) -> int32_t
{
    if (objectiveNumber < 0 || static_cast<int32_t>(numObjectives) <= objectiveNumber)
    {
        return BAD_OBJECTIVE;
    }

    const auto id = static_cast<int16_t>(objectiveNumber + OBJECTIVE_TIMER_ID);
    application->RemoveTimer(application, id);
    application->AddTimer(application, id, static_cast<int32_t>(time), OBJECTIVE_TIMER_EVENT, 0, 1);
    return 0;
}

auto Scenario::checkObjectiveTimer(int32_t objectiveNumber) -> float
{
    if (objectiveNumber < 0 || static_cast<int32_t>(numObjectives) <= objectiveNumber)
    {
        return 0.0f;
    }

    uint32_t remaining = 0;

    if (aTimer* timer = application->timerManager->GetTimer(application,
                                                            static_cast<int16_t>(objectiveNumber + OBJECTIVE_TIMER_ID)))
    {
        // The timer counts in scenario milliseconds.
        const uint32_t fireTime = timer->interval + timer->lastTime;
        remaining = static_cast<uint32_t>(
            static_cast<int32_t>(static_cast<double>(fireTime) - static_cast<double>(scenarioTime) * 1000.0));
    }

    return static_cast<float>(static_cast<double>(remaining) * 0.001);
}

auto Scenario::setObjectiveStatus(int32_t objectiveNumber, uint32_t status) -> int32_t
{
    if (objectiveNumber < 0 || static_cast<int32_t>(numObjectives) <= objectiveNumber)
    {
        return BAD_OBJECTIVE;
    }

    objectives[objectiveNumber].status = status;
    return 0;
}

auto Scenario::checkObjectiveStatus(int32_t objectiveNumber) -> uint32_t
{
    if (objectiveNumber < 0 || static_cast<int32_t>(numObjectives) <= objectiveNumber)
    {
        return NO_OBJECTIVE;
    }

    return objectives[objectiveNumber].status;
}

auto Scenario::setObjectiveType(int32_t objectiveNumber, uint32_t type) -> int32_t
{
    if (objectiveNumber < 0 || static_cast<int32_t>(numObjectives) <= objectiveNumber)
    {
        return BAD_OBJECTIVE;
    }

    objectives[objectiveNumber].type = type;
    return 0;
}

auto Scenario::checkObjectiveType(int32_t objectiveNumber) -> uint32_t
{
    if (objectiveNumber < 0 || static_cast<int32_t>(numObjectives) <= objectiveNumber)
    {
        return NO_OBJECTIVE;
    }

    return objectives[objectiveNumber].type;
}

auto Scenario::setObjectivePos(int32_t objectiveNumber, float x, float y, float z) -> void
{
    if (objectiveNumber < 0 || static_cast<int32_t>(numObjectives) <= objectiveNumber)
    {
        return;
    }

    objectives[objectiveNumber].position[0] = x;
    objectives[objectiveNumber].position[1] = y;
    objectives[objectiveNumber].position[2] = z;
}

auto Scenario::calcResourcePointsEarned() -> int32_t
{
    if (scenarioResult <= 3)
    {
        return 0;
    }

    int32_t points = 0;

    for (int32_t i = 0; i < MAX_OBJECTIVES; i++)
    {
        if (objectives[i].status == 1 || mission->endScenarioRequested != 0)
        {
            points += objectives[i].points;
        }
    }

    return points;
}

auto Scenario::setupBonus() -> void
{
    // Port fix: the original's search read one objective past the array when all nine were in use.
    int32_t slot = 0;

    while (slot < MAX_OBJECTIVES && objectives[slot].status != UNUSED_OBJECTIVE)
    {
        slot++;
    }

    Assert(slot < MAX_OBJECTIVES, static_cast<uint32_t>(slot), " Too Many objectives in use ");

    ScenarioObjective& bonus = objectives[slot];
    const int32_t unusedTonnage = maxDeployTonnage - curDeployTonnage;
    bonus.status = 1;
    bonus.type = 3;
    bonus.points = unusedTonnage / tonnageDivisor * resourcesPerTonDivided;
    char format[256];
    cLoadString(thisInstance, 0x376, format, 0xfe);
    std::snprintf(bonus.name, sizeof(bonus.name), format, unusedTonnage);
}

auto Scenario::handleMultiplayMessage(int32_t code, int32_t param) -> void
{
    if (scenarioBrainHandleMessage == nullptr)
    {
        return;
    }

    CurMultiplayCode = code;
    CurMultiplayParam = param;
    scenarioBrain->execute(nullptr, scenarioBrainHandleMessage, nullptr);
    CurMultiplayCode = 0;
    CurMultiplayParam = 0;
}

auto Scenario::checkAnyoneInCombat() -> void
{
    for (int32_t i = 1; i <= static_cast<int32_t>(numWarriors); i++)
    {
        MechWarrior* warrior = warriors[i];

        if (warrior == nullptr || warrior->status != 0)
        {
            continue;
        }

        GameObject* target = warrior->getLastTarget();

        if (target != nullptr && target->getAlignment() != warrior->alignment && target->getAlignment() != 0 &&
            target->isDisabled() == 0)
        {
            inCombat = 1;
            return;
        }
    }

    inCombat = 0;
}
