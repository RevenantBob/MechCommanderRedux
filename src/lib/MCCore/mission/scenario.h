#pragma once

// The scenario: one battle (mission\scenario.cpp). Scenario::init reads the scenario FIT and starts every game system
// (palette, cameras, objects, sprites, terrain, ABL, teams, warriors, parts, objectives); Scenario::run is the
// per-frame update; Scenario::destroy shuts it all down again.

class aObject;
class FitIniFile;
class Palette;
class ABLModule;
struct ABLParam;
class MechWarrior;
class BaseObject;
class GameObject;
class ObjectQueue;
class Team;
class CollisionSystem;
class TrainManager;
class AppearanceTypeList;
class CraterManager;
class ObjectMap;
struct _SymTableNode;

/// <summary>
/// One part of the scenario FIT (<c>Part%d</c>): an object placed at the scenario's start, with its team, pilot and
/// control. <c>Scenario::parts</c> is 1-based.
/// </summary>
/// <remarks>
/// 0x58 bytes in the original (the object pointer is 8 bytes in the port). The struct name follows MechCommander 2's
/// source, which kept this layout; the field names are the FIT keys.
/// </remarks>
struct Part
{
    /// <summary>The object created for the part (null until created, and after it is destroyed).</summary>
    BaseObject* object; // +0x0
    /// <summary><c>ObjectNumber</c>: the object type.</summary>
    uint32_t objNumber; // +0x4
    /// <summary><c>PaintScheme</c>, -1 when missing.</summary>
    int32_t paintScheme; // +0x8
    /// <summary><c>Active</c>.</summary>
    int32_t active; // +0xc
    /// <summary><c>Exists</c>.</summary>
    int32_t exists; // +0x10
    /// <summary>Set when the part's object was destroyed (<c>Scenario::destroyPartObject</c>).</summary>
    int32_t destroyed; // +0x14
    /// <summary><c>PositionX</c>, <c>PositionY</c>, <c>PositionZ</c>.</summary>
    float position[3]; // +0x18
    /// <summary><c>Velocity</c>.</summary>
    float velocity; // +0x24
    /// <summary><c>Rotation</c>.</summary>
    float rotation; // +0x28
    /// <summary><c>Gesture</c>.</summary>
    uint32_t gestureId; // +0x2c
    /// <summary>1 for team 0 or 2, -1 for team 1 (derived from <see cref="teamId"/>).</summary>
    int8_t alignment; // +0x30
    /// <summary><c>TeamId</c>: 0, 1 or 2.</summary>
    int8_t teamId; // +0x31
    /// <summary><c>CommanderId</c> (read as a char, or failing that a long).</summary>
    int32_t commanderId; // +0x34
    /// <summary><c>MyIcon</c>.</summary>
    char myIcon; // +0x38
    /// <summary><c>ControlType</c>.</summary>
    uint32_t controlType; // +0x3c
    /// <summary><c>ControlDataType</c>.</summary>
    uint32_t controlDataType; // +0x40
    /// <summary><c>ObjectProfile</c> (8 characters).</summary>
    char profileName[9]; // +0x44
    /// <summary><c>Pilot</c>: the warrior's index in <c>Scenario::warriors</c>.</summary>
    uint32_t pilot; // +0x50
    /// <summary><c>Captureable</c>.</summary>
    int32_t captureable; // +0x54
};

/// <summary>A scenario objective (<c>Objective%d</c> in the scenario FIT); <c>Scenario::objectives</c> has 9.</summary>
/// <remarks>
/// 0x70 bytes. The name is the port's. Unused entries have <see cref="type"/> and <see cref="status"/> -9999; the
/// check functions return 9999 for an index out of range.
/// </remarks>
struct ScenarioObjective
{
    /// <summary><c>Name</c>: the text shown on the results screen.</summary>
    char name[80]; // +0x0
    /// <summary><c>Type</c>: 0 primary, 1 secondary, 3 the tonnage bonus added by <c>setupBonus</c>.</summary>
    uint32_t type; // +0x50
    /// <summary><c>TimeLeft</c> in seconds; above 0 starts the objective's timer at the scenario's start.</summary>
    float timeLeft; // +0x54
    /// <summary><c>Status</c>: 0 pending, 1 succeeded, 2 failed.</summary>
    uint32_t status; // +0x58
    /// <summary>Where the objective is (-99, -99, -99 until the script sets it).</summary>
    float position[3]; // +0x5c
    /// <summary><c>Points</c>: resource points earned when it succeeds.</summary>
    int32_t points; // +0x68
    /// <summary><c>Radius</c>.</summary>
    float radius; // +0x6c
};

/// <summary>An area <c>Scenario::objectInArea</c> tests against (0x10 bytes; the name is the port's).</summary>
struct ScenarioArea
{
    /// <summary>Circle: centre x, y, z. Rectangle: left, top, then the third value (see <see cref="areaType"/>).</summary>
    float coords[3]; // +0x0
    /// <summary>0 = circle, 1 = rectangle.</summary>
    uint8_t areaType; // +0xc
};

/// <summary>A part created by an ABL script (<c>createdPartRoster</c>), and whether its object was created yet.</summary>
/// <remarks>8 bytes.</remarks>
struct CreatedPartRoster
{
    /// <summary>The part's id (compared with the object's part id, BaseObject +0xc).</summary>
    int32_t partId; // +0x0
    /// <summary>Set by <c>Scenario::createScenarioObject</c> once the object is created.</summary>
    int32_t created; // +0x4
};

/// <summary>The current battle: its settings, warriors, parts, objectives and ABL brain.</summary>
/// <remarks>
/// Original source: <c>mission\scenario.cpp</c>, 0x2ac bytes, no vtable. The one instance is <c>scenario</c>, made in
/// <c>Mission::StartScenario</c> (the constructor was inlined there; the member initializers below are its stores:
/// everything 0 except <see cref="scenarioScriptHandle"/> and <see cref="timeLimit"/> -1 and
/// <see cref="captureChance"/> 2).
/// </remarks>
class Scenario
{
public:
    /// <summary>
    /// Advances the clocks (<c>scenarioTime</c>, <c>turn</c>), plays the time-limit warnings, counts the start-up turns
    /// down and starts the music when they are over.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0072fa10</remarks>
    int32_t update();

    /// <summary>Draws the camera views into <paramref name="window"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0072fc30</remarks>
    int32_t render(aObject* window);

    /// <summary>
    /// Loads scenario <paramref name="scenarioName"/> and starts every game system. <paramref name="terrainName"/>,
    /// when given, loads that terrain instead of the FIT's (the editor path).
    /// </summary>
    /// <returns>0, or an error code.</returns>
    /// <remarks>MCX.EXE @ 0x0072ff50</remarks>
    int32_t init(char* scenarioName, char* terrainName);

    /// <summary>One frame: updates the world, runs the scenario's ABL brain and takes its result.</summary>
    /// <remarks>MCX.EXE @ 0x007368b0</remarks>
    int32_t run();

    /// <summary>Shuts every game system down and frees the scenario's data.</summary>
    /// <remarks>MCX.EXE @ 0x00736a50</remarks>
    void destroy();

    /// <summary>Frees the warriors.</summary>
    /// <remarks>MCX.EXE @ 0x007370f0</remarks>
    void destroyWarriors();

    /// <summary>Creates the object of part <paramref name="partNumber"/> and gives it its pilot, team and commander.</summary>
    /// <remarks>MCX.EXE @ 0x007371b0</remarks>
    void createPartObject(int32_t partNumber);

    /// <summary>
    /// Moves the not-yet-created scenario object with part id <paramref name="partId"/> from the scenario object list
    /// into play.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x007379d0</remarks>
    void createScenarioObject(int32_t partId);

    /// <summary>Takes part <paramref name="partNumber"/>'s object out of play and marks the part destroyed.</summary>
    /// <remarks>MCX.EXE @ 0x00737bf0</remarks>
    void destroyPartObject(int32_t partNumber);

    /// <summary>Whether <paramref name="object"/> is inside area <paramref name="areaNumber"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00737c60</remarks>
    int objectInArea(GameObject* object, int32_t areaNumber);

    /// <summary>Starts the timers of the objectives that have a time limit.</summary>
    /// <remarks>MCX.EXE @ 0x00737d60</remarks>
    void startObjectiveTimers();

    /// <summary>(Re)starts objective <paramref name="objectiveNumber"/>'s timer at <paramref name="time"/> milliseconds.</summary>
    /// <returns>0, or an error code for a bad index.</returns>
    /// <remarks>MCX.EXE @ 0x00737dc0</remarks>
    int32_t setObjectiveTimer(int32_t objectiveNumber, float time);

    /// <summary>Seconds left on objective <paramref name="objectiveNumber"/>'s timer (0 when none).</summary>
    /// <remarks>MCX.EXE @ 0x00737e20</remarks>
    float checkObjectiveTimer(int32_t objectiveNumber);

    /// <remarks>MCX.EXE @ 0x00737eb0</remarks>
    int32_t setObjectiveStatus(int32_t objectiveNumber, uint32_t status);

    /// <summary>Objective <paramref name="objectiveNumber"/>'s status; 9999 for a bad index.</summary>
    /// <remarks>MCX.EXE @ 0x00737ef0</remarks>
    uint32_t checkObjectiveStatus(int32_t objectiveNumber);

    /// <remarks>MCX.EXE @ 0x00737f30</remarks>
    int32_t setObjectiveType(int32_t objectiveNumber, uint32_t type);

    /// <summary>Objective <paramref name="objectiveNumber"/>'s type; 9999 for a bad index.</summary>
    /// <remarks>MCX.EXE @ 0x00737f70</remarks>
    uint32_t checkObjectiveType(int32_t objectiveNumber);

    /// <remarks>MCX.EXE @ 0x00737fb0</remarks>
    void setObjectivePos(int32_t objectiveNumber, float x, float y, float z);

    /// <summary>
    /// The resource points the scenario earned: the points of the objectives that succeeded (all of them when the
    /// mission ended the scenario early), or 0 when the scenario was lost.
    /// </summary>
    /// <remarks>
    /// MCX.EXE @ 0x00737ff0. Unnamed in the binary (an inline member, called from <c>Mission::EndScenario</c>); the
    /// name is the port's.
    /// </remarks>
    int32_t calcResourcePointsEarned();

    /// <summary>Adds the unused-tonnage bonus as an extra, succeeded objective.</summary>
    /// <remarks>MCX.EXE @ 0x00738030</remarks>
    void setupBonus();

    /// <summary>
    /// Runs the brain's <c>handlemessage</c> function with a multiplayer message (<c>CurMultiplayCode</c>,
    /// <c>CurMultiplayParam</c>).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00738100</remarks>
    void handleMultiplayMessage(int32_t code, int32_t param);

    /// <summary>Checks whether any of the player's 'Mechs is in combat (for the music).</summary>
    /// <remarks>MCX.EXE @ 0x00738150</remarks>
    void checkAnyoneInCombat();

    /// <summary>The open scenario FIT while <see cref="init"/> runs.</summary>
    FitIniFile* scenarioFile = nullptr; // +0x0
    /// <summary>Never accessed in MCX.EXE.</summary>
    int32_t unknown04 = 0; // +0x4
    /// <summary>Never accessed in MCX.EXE.</summary>
    int32_t unknown08 = 0; // +0x8
    /// <summary><c>CameraHeapSize</c>.</summary>
    uint32_t cameraHeapSize = 0; // +0xc
    /// <summary><c>CameraFileName</c>.</summary>
    char cameraFileName[80] = {}; // +0x10
    /// <summary><c>ObjectHeapSize</c>.</summary>
    uint32_t objectHeapSize = 0; // +0x60
    /// <summary><c>ObjectTypeHeapSize</c>.</summary>
    uint32_t objectTypeHeapSize = 0; // +0x64
    /// <summary><c>NumObjects</c>.</summary>
    uint32_t numObjects = 0; // +0x68
    /// <summary><c>ObjectFileName</c>.</summary>
    char objectFileName[80] = {}; // +0x6c
    /// <summary><c>SpriteHeapSize</c>.</summary>
    uint32_t spriteHeapSize = 0; // +0xbc
    /// <summary><c>SpriteFileName</c>.</summary>
    char spriteFileName[80] = {}; // +0xc0
    /// <summary><c>TerrainFileName</c>.</summary>
    char terrainFileName[80] = {}; // +0x110
    /// <summary><c>ScenarioScript</c>: the ABL script (and the scenario's name on the results).</summary>
    char scenarioScript[80] = {}; // +0x160
    /// <summary>The script's module handle from <c>ABLi_preProcess</c>; -1 before.</summary>
    int32_t scenarioScriptHandle = -1; // +0x1b0
    /// <summary>The scenario's ABL brain.</summary>
    ABLModule* scenarioBrain = nullptr; // +0x1b4
    /// <summary>The parameters passed to the brain each frame.</summary>
    ABLParam* scenarioBrainParams = nullptr; // +0x1b8
    /// <summary>The brain's <c>handlemessage</c> function, if it has one.</summary>
    _SymTableNode* scenarioBrainHandleMessage = nullptr; // +0x1bc
    /// <summary><c>PaletteSystem</c>: the palette file.</summary>
    char paletteSystem[80] = {}; // +0x1c0
    /// <summary>The palette before the scenario's (<c>gamePalette</c>), restored by <see cref="destroy"/>.</summary>
    Palette* oldPalette = nullptr; // +0x210
    /// <summary><c>NumWarriors</c>.</summary>
    uint32_t numWarriors = 0; // +0x214
    /// <summary>The warriors, 1-based (<see cref="numWarriors"/> + 1 entries).</summary>
    MechWarrior** warriors = nullptr; // +0x218
    /// <summary><c>CaptureChance</c> (0-4; 2 when missing or out of range).</summary>
    uint8_t captureChance = 2; // +0x21c
    /// <summary><c>NumParts</c>.</summary>
    uint32_t numParts = 0; // +0x220
    /// <summary>The parts, 1-based (<see cref="numParts"/> + 1 entries).</summary>
    Part* parts = nullptr; // +0x224
    /// <summary>Zeroed by the constructor; never accessed otherwise.</summary>
    int32_t unknown228 = 0; // +0x228
    /// <summary>Zeroed by the constructor; never accessed otherwise.</summary>
    int32_t unknown22C = 0; // +0x22c
    /// <summary>Number of <see cref="areas"/>.</summary>
    int32_t numAreas = 0; // +0x230
    /// <summary>The areas for <see cref="objectInArea"/> (never filled in MCX.EXE: numAreas stays 0).</summary>
    ScenarioArea* areas = nullptr; // +0x234
    /// <summary>Zeroed by <see cref="init"/>; never read.</summary>
    int32_t unknown238 = 0; // +0x238
    /// <summary>The objects made at the start but not yet in play (created later by the script).</summary>
    ObjectQueue* scenarioObjectList = nullptr; // +0x23c
    /// <summary><c>NumObjectives</c> (at most 9).</summary>
    uint32_t numObjectives = 0; // +0x240
    /// <summary><c>Duration</c> from the game system block.</summary>
    uint32_t duration = 0; // +0x244
    /// <summary>The objectives: always 9 entries when there are any.</summary>
    ScenarioObjective* objectives = nullptr; // +0x248
    /// <summary><c>NumLargeStrikes</c> for the home commander.</summary>
    int32_t numLargeStrikes = 0; // +0x24c
    /// <summary><c>NumSmallStrikes</c>.</summary>
    int32_t numSmallStrikes = 0; // +0x250
    /// <summary><c>NumSensorStrikes</c>.</summary>
    int32_t numSensorStrikes = 0; // +0x254
    /// <summary><c>NumCameraStrikes</c> (camera drones).</summary>
    int32_t numCameraStrikes = 0; // +0x258
    /// <summary><c>scenarioTuneNum</c>: the music track started after the start-up turns.</summary>
    uint8_t scenarioTuneNum = 0; // +0x25c
    /// <summary>
    /// The six VFX shapes of the <c>SensorContactShape</c> packet file (packets 0-5): the sensor contact blips drawn
    /// for objects seen only on sensors.
    /// </summary>
    uint8_t* sensorContactShapes[6] = {}; // +0x260
    /// <summary><c>MaxVisualRange</c>.</summary>
    float maxVisualRange = 0.0f; // +0x278
    /// <summary><c>FireVisualRange</c>: how far a firing object is revealed.</summary>
    float fireVisualRange = 0.0f; // +0x27c
    /// <summary><c>MaxWeaponRange</c>.</summary>
    float maxWeaponRange = 0.0f; // +0x280
    /// <summary><c>BaseSensorRange</c>.</summary>
    float baseSensorRange = 0.0f; // +0x284
    /// <summary><c>AlwaysRevealed</c>: the whole map is revealed.</summary>
    uint8_t alwaysRevealed = 0; // +0x288
    /// <summary><c>GodMode</c>.</summary>
    uint8_t godMode = 0; // +0x289
    /// <summary>Set when the scenario FIT has an <c>Output</c> block.</summary>
    uint8_t hasOutputBlock = 0; // +0x28a
    /// <summary><c>CycleLength</c>.</summary>
    float cycleLength = 0.0f; // +0x28c
    /// <summary><c>TimeLeft</c>: the time limit in seconds; -1 = none.</summary>
    int32_t timeLimit = -1; // +0x290
    /// <summary>Set once the two-minute warning played.</summary>
    int32_t twoMinuteWarningPlayed = 0; // +0x294
    /// <summary>Set once the thirty-second warning played.</summary>
    int32_t thirtySecondWarningPlayed = 0; // +0x298
    /// <summary>Set during the start-up turns (the "loading" countdown on the interface).</summary>
    int32_t startingUp = 0; // +0x29c
    /// <summary>Start-up turns left, times 10 (100 at the start).</summary>
    int32_t startUpCountdown = 0; // +0x2a0
    /// <summary>Added to <see cref="startUpTurns"/> in <see cref="update"/>; only ever 0 in MCX.EXE.</summary>
    int32_t unknown2A4 = 0; // +0x2a4
    /// <summary>The turns the scenario runs before the player gets control (10).</summary>
    int32_t startUpTurns = 0; // +0x2a8
};

/// <summary>Scales <paramref name="skill"/> by the difficulty's skill percentage (for the player when <paramref name="player"/>).</summary>
/// <remarks>MCX.EXE @ 0x0072fc60</remarks>
float applyDifficultySkill(float skill, int player);

/// <summary>
/// Scales a weapon value by the difficulty's weapon percentage, rounds it to a quarter and clamps it to 0-255.
/// </summary>
/// <remarks>MCX.EXE @ 0x0072fd00</remarks>
float applyDifficultyWeapon(float value, int player);

/// <summary>Reads the <c>DifficultySettings</c> block of the game system file.</summary>
/// <remarks>MCX.EXE @ 0x0072fe10</remarks>
void InitDifficultySettings(FitIniFile* gameSystemFile);

/// <summary>The current scenario (null outside one).</summary>
/// <remarks>MCX.EXE @ 0x00809404 (globals_by_file.md assigns it to abl\ablxstd.cpp; its home is here).</remarks>
extern Scenario* scenario;
/// <summary>The scenario clock used for timing: <c>scenarioTime</c>, or the real running time in multiplayer.</summary>
/// <remarks>MCX.EXE @ 0x008093f8 (sits among scenario.cpp's globals).</remarks>
extern float actualTime;
/// <summary>Zeroed each turn by <see cref="Scenario::update"/>.</summary>
/// <remarks>MCX.EXE @ 0x00809430 (sits among scenario.cpp's globals).</remarks>
extern int nextStep;
/// <summary>Zeroed each turn by <see cref="Scenario::update"/>.</summary>
/// <remarks>MCX.EXE @ 0x00809434 (sits among scenario.cpp's globals).</remarks>
extern int prevStep;
/// <summary>The turn the scenario ends at; -1 = not ending.</summary>
/// <remarks>MCX.EXE @ 0x007a1c2c (sits among scenario.cpp's globals).</remarks>
extern int32_t scenarioEndTurn;
/// <summary>The longest frame the simulation takes (0.25 s).</summary>
/// <remarks>MCX.EXE @ 0x007a1c28</remarks>
extern float minFrameLength;
/// <summary>Set to -1.0 before each part is created in <see cref="Scenario::init"/>; nothing reads it.</summary>
/// <remarks>MCX.EXE @ 0x007a1c30 (unnamed in the binary; the name is the port's).</remarks>
extern float partCreateTime;
/// <summary>Whether object collisions are checked (1).</summary>
/// <remarks>MCX.EXE @ 0x007a1c34</remarks>
extern int collisionSwitch;
/// <summary>Tons of unused drop weight per bonus unit (5).</summary>
/// <remarks>MCX.EXE @ 0x007a1c48</remarks>
extern int32_t tonnageDivisor;
/// <summary>Resource points per bonus unit (200).</summary>
/// <remarks>MCX.EXE @ 0x007a1c4c</remarks>
extern int32_t resourcesPerTonDivided;
/// <remarks>MCX.EXE @ 0x007a5464 (102400; bookkeeping only in the port)</remarks>
extern uint32_t AblSymbolTableHeapSize;
/// <remarks>MCX.EXE @ 0x007a5468 (40960)</remarks>
extern uint32_t AblStackHeapSize;
/// <remarks>MCX.EXE @ 0x007a546c (102400)</remarks>
extern uint32_t AblCodeHeapSize;
/// <remarks>MCX.EXE @ 0x007a5470 (20480)</remarks>
extern uint32_t AblRunTimeStackSize;
/// <remarks>MCX.EXE @ 0x007a5474 (10240)</remarks>
extern uint32_t AblMaxCodeBlockSize;
/// <remarks>MCX.EXE @ 0x007a5478 (200)</remarks>
extern uint32_t AblMaxRegisteredModules;
/// <remarks>MCX.EXE @ 0x007a547c (100)</remarks>
extern uint32_t AblMaxStaticVariables;
/// <remarks>MCX.EXE @ 0x007dd05c</remarks>
extern CollisionSystem* collisionSystem;
/// <summary>Every mover by part id (0xe00 entries).</summary>
/// <remarks>MCX.EXE @ 0x007dfec0</remarks>
extern BaseObject* MoverRoster[0xe00];
/// <summary>Mission "MineLayThrottle" (50 when missing): a mine layer's top throttle while laying.</summary>
/// <remarks>MCX.EXE @ 0x00808ca4</remarks>
extern int32_t MineLayThrottle;
/// <summary>Mission "MineSweepThrottle" (50 when missing): a sweeper's top throttle after clearing a mine.</summary>
/// <remarks>MCX.EXE @ 0x00808fdc</remarks>
extern int32_t MineSweepThrottle;
/// <summary>Mission "MineWaitTime": the seconds a mine layer waits on a cell before laying.</summary>
/// <remarks>MCX.EXE @ 0x008093e0</remarks>
extern float MineWaitTime;
/// <summary>The teams: [1] the clan team, [2] the allied team (set in <see cref="Scenario::init"/>).</summary>
/// <remarks>MCX.EXE @ 0x007e4948</remarks>
extern Team* TeamTable[3];
/// <remarks>MCX.EXE @ 0x007e4958</remarks>
extern TrainManager* trainManager;
/// <remarks>MCX.EXE @ 0x007f0500</remarks>
extern AppearanceTypeList* appearanceTypeList;
/// <remarks>MCX.EXE @ 0x007f09b4</remarks>
extern CraterManager* craterManager;
/// <summary>The scenario's frame (turn) counter.</summary>
/// <remarks>MCX.EXE @ 0x007f09c0</remarks>
extern int32_t turn;
/// <remarks>MCX.EXE @ 0x00807fe0</remarks>
extern ObjectMap* GameObjectMap;
/// <summary>The <c>VisualRangeTable</c> of the game system file (256 entries).</summary>
/// <remarks>MCX.EXE @ 0x008087c0</remarks>
extern int32_t visualRangeTable[256];
/// <summary>Difficulty weapon percentages for the player (<c>PlayerWeapons</c>: easy, hard).</summary>
/// <remarks>MCX.EXE @ 0x00808c90</remarks>
extern int32_t globalPlayerWeapons[2];
/// <summary>Difficulty skill percentages for the enemy (<c>EnemySkills</c>).</summary>
/// <remarks>MCX.EXE @ 0x00808c9c</remarks>
extern int32_t globalEnemySkills[2];
/// <summary>The parts ABL scripts created (<c>currentCreatorPart</c> of them).</summary>
/// <remarks>MCX.EXE @ 0x00808ca8 (100 entries: Scenario::init clears 800 bytes)</remarks>
extern CreatedPartRoster createdPartRoster[100];
/// <summary>Difficulty weapon percentages for the enemy (<c>EnemyWeapons</c>).</summary>
/// <remarks>MCX.EXE @ 0x00808fc8</remarks>
extern int32_t globalEnemyWeapons[2];
/// <summary>Difficulty skill percentages for the player (<c>PlayerSkills</c>).</summary>
/// <remarks>MCX.EXE @ 0x00808fd0</remarks>
extern int32_t globalPlayerSkills[2];
/// <summary>Difficulty salvage percentages (<c>SalvageChance</c>).</summary>
/// <remarks>MCX.EXE @ 0x008093e4</remarks>
extern int32_t globalSalvageModifier[2];
/// <summary>The real time the scenario started (milliseconds; 0 until the start-up turns are over).</summary>
/// <remarks>MCX.EXE @ 0x008093f0</remarks>
extern uint32_t MissionStartTime;
/// <summary>Seconds since <c>MissionStartTime</c>.</summary>
/// <remarks>MCX.EXE @ 0x008093f4</remarks>
extern float runningTime;
/// <summary>Set when the scenario music should start after the start-up turns.</summary>
/// <remarks>MCX.EXE @ 0x00809410</remarks>
extern int32_t startMusic;
/// <remarks>MCX.EXE @ 0x00809414</remarks>
extern float InfluenceTime;
/// <remarks>MCX.EXE @ 0x00809424</remarks>
extern int drawRevealedTacMap;
/// <summary>Number of entries used in <c>createdPartRoster</c>.</summary>
/// <remarks>MCX.EXE @ 0x00809438</remarks>
extern int32_t currentCreatorPart;
/// <summary>The waypoint marker shapes (loaded from a file in <see cref="Scenario::init"/>).</summary>
/// <remarks>MCX.EXE @ 0x00809440</remarks>
extern uint8_t* waypointMarkers;
/// <remarks>MCX.EXE @ 0x0080944c</remarks>
extern int endingScenario;
/// <summary><c>AlwaysDraw</c> from the game system file.</summary>
/// <remarks>MCX.EXE @ 0x00809f6c</remarks>
extern uint8_t forceAlways;
/// <summary>
/// Where a saved game's copies of the scenario, warrior and object profile FITs are unpacked
/// (<c>"data\save\temp\"</c>); the scenario falls back on it when a file isn't in its usual folder.
/// </summary>
/// <remarks>MCX.EXE @ 0x0079451c (one of the 80-byte path globals; the owner is unknown).</remarks>
extern char saveTempPath[80];
