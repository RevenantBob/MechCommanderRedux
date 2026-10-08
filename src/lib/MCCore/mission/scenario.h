#pragma once

#include "abl/MCAblModule.h"

#include "color/MCPalette.h"
#include "object/MCObjectQueue.h"

// The scenario: one battle (mission\scenario.cpp). Scenario::init reads the scenario FIT and starts every game system
// (palette, cameras, objects, sprites, terrain, ABL, teams, warriors, parts, objectives); Scenario::run is the
// per-frame update; Scenario::destroy shuts it all down again.

class MCGuiObject;
class MCFitIniFile;
class MCPalette;
class MCMechWarrior;
class MCBaseObject;
class MCGameObject;
class MCTeam;
class MCTrainManager;
class MCObjectMap;
struct MCAblSymbol;

/// <summary>
/// One part of the scenario FIT (<c>Part%d</c>): an object placed at the scenario's start, with its team, pilot and
/// control. <c>Scenario::parts</c> is 1-based.
/// </summary>
/// <remarks>
/// 0x58 bytes in the original (the object pointer is 8 bytes in the port). The struct name follows MechCommander 2's
/// source, which kept this layout; the field names are the FIT keys.
/// </remarks>
struct MCPart
{
    /// <summary>The object created for the part (null until created, and after it is destroyed).</summary>
    MCBaseObject* Object = nullptr;
    /// <summary><c>ObjectNumber</c>: the object type.</summary>
    uint32_t ObjNumber = 0;
    /// <summary><c>PaintScheme</c>, -1 when missing.</summary>
    int32_t PaintScheme = 0;
    /// <summary><c>Active</c>.</summary>
    int32_t Active = 0;
    /// <summary><c>Exists</c>.</summary>
    int32_t Exists = 0;
    /// <summary>Set when the part's object was destroyed (<c>Scenario::destroyPartObject</c>).</summary>
    int32_t Destroyed = 0;
    /// <summary><c>PositionX</c>, <c>PositionY</c>, <c>PositionZ</c>.</summary>
    float Position[3]{};
    /// <summary><c>Velocity</c>.</summary>
    float Velocity = 0;
    /// <summary><c>Rotation</c>.</summary>
    float Rotation = 0;
    /// <summary><c>Gesture</c>.</summary>
    uint32_t GestureId = 0;
    /// <summary>1 for team 0 or 2, -1 for team 1 (derived from <see cref="TeamId"/>).</summary>
    int8_t Alignment = 0;
    /// <summary><c>TeamId</c>: 0, 1 or 2.</summary>
    int8_t TeamId = 0;
    /// <summary><c>CommanderId</c> (read as a char, or failing that a long).</summary>
    int32_t CommanderId = 0;
    /// <summary><c>MyIcon</c>.</summary>
    char MyIcon = 0;
    /// <summary><c>ControlType</c>.</summary>
    uint32_t ControlType = 0;
    /// <summary><c>ControlDataType</c>.</summary>
    uint32_t ControlDataType = 0;
    /// <summary><c>ObjectProfile</c> (8 characters).</summary>
    char ProfileName[9]{};
    /// <summary><c>Pilot</c>: the warrior's index in <c>Scenario::warriors</c>.</summary>
    uint32_t Pilot = 0;
    /// <summary><c>Captureable</c>.</summary>
    int32_t Captureable = 0;
};

/// <summary>A scenario objective (<c>Objective%d</c> in the scenario FIT); <c>Scenario::objectives</c> has 9.</summary>
/// <remarks>
/// 0x70 bytes. The name is the port's. Unused entries have <see cref="Type"/> and <see cref="Status"/> -9999; the
/// check functions return 9999 for an index out of range.
/// </remarks>
struct MCScenarioObjective
{
    /// <summary><c>Name</c>: the text shown on the results screen.</summary>
    char Name[80]{};
    /// <summary><c>Type</c>: 0 primary, 1 secondary, 3 the tonnage bonus added by <c>setupBonus</c>.</summary>
    uint32_t Type = 0;
    /// <summary><c>TimeLeft</c> in seconds; above 0 starts the objective's timer at the scenario's start.</summary>
    float TimeLeft = 0;
    /// <summary><c>Status</c>: 0 pending, 1 succeeded, 2 failed.</summary>
    uint32_t Status = 0;
    /// <summary>Where the objective is (-99, -99, -99 until the script sets it).</summary>
    float Position[3]{};
    /// <summary><c>Points</c>: resource points earned when it succeeds.</summary>
    int32_t Points = 0;
    /// <summary><c>Radius</c>.</summary>
    float Radius = 0;
};

/// <summary>An area <c>Scenario::objectInArea</c> tests against (0x10 bytes; the name is the port's).</summary>
struct MCScenarioArea
{
    /// <summary>Circle: centre x, y, z. Rectangle: left, top, then the third value (see <see cref="AreaType"/>).</summary>
    float Coords[3]{};
    /// <summary>0 = circle, 1 = rectangle.</summary>
    uint8_t AreaType = 0;
};

/// <summary>A part created by an ABL script (<c>createdPartRoster</c>), and whether its object was created yet.</summary>
/// <remarks>8 bytes.</remarks>
struct MCCreatedPartRoster
{
    /// <summary>The part's id (compared with the object's part id, BaseObject +0xc).</summary>
    int32_t PartId = 0;
    /// <summary>Set by <c>Scenario::createScenarioObject</c> once the object is created.</summary>
    int32_t Created = 0;
};

/// <summary>The current battle: its settings, warriors, parts, objectives and ABL brain.</summary>
/// <remarks>
/// Original source: <c>mission\scenario.cpp</c>, 0x2ac bytes, no vtable. The one instance is <c>scenario</c>, made in
/// <c>Mission::StartScenario</c> (the constructor was inlined there; the member initializers below are its stores:
/// everything 0 except <see cref="ScenarioScriptHandle"/> and <see cref="TimeLimit"/> -1 and
/// <see cref="CaptureChance"/> 2).
/// </remarks>
class MCScenario
{
public:
    /// <summary>
    /// Advances the clocks (<c>scenarioTime</c>, <c>turn</c>), plays the time-limit warnings, counts the start-up turns
    /// down and starts the music when they are over.
    /// </summary>
    int32_t Update();

    /// <summary>Draws the camera views into <paramref name="window"/>.</summary>
    int32_t Render(MCGuiObject* window);

    /// <summary>
    /// Loads scenario <paramref name="scenarioName"/> and starts every game system. <paramref name="terrainName"/>,
    /// when given, loads that terrain instead of the FIT's (the editor path).
    /// </summary>
    /// <returns>0, or an error code.</returns>
    int32_t Init(char* scenarioName, char* terrainName);

    /// <summary>One frame: updates the world, runs the scenario's ABL brain and takes its result.</summary>
    int32_t Run();

    /// <summary>Shuts every game system down and frees the scenario's data.</summary>
    void Destroy();

    /// <summary>Frees the warriors.</summary>
    void DestroyWarriors();

    /// <summary>Creates the object of part <paramref name="partNumber"/> and gives it its pilot, team and commander.</summary>
    void CreatePartObject(int32_t partNumber);

    /// <summary>
    /// Moves the not-yet-created scenario object with part id <paramref name="partId"/> from the scenario object list
    /// into play.
    /// </summary>
    void CreateScenarioObject(int32_t partId);

    /// <summary>Takes part <paramref name="partNumber"/>'s object out of play and marks the part destroyed.</summary>
    void DestroyPartObject(int32_t partNumber);

    /// <summary>Whether <paramref name="object"/> is inside area <paramref name="areaNumber"/>.</summary>
    int ObjectInArea(MCGameObject* object, int32_t areaNumber);

    /// <summary>Starts the timers of the objectives that have a time limit.</summary>
    void StartObjectiveTimers();

    /// <summary>(Re)starts objective <paramref name="objectiveNumber"/>'s timer at <paramref name="time"/> milliseconds.</summary>
    /// <returns>0, or an error code for a bad index.</returns>
    int32_t SetObjectiveTimer(int32_t objectiveNumber, float time);

    /// <summary>Seconds left on objective <paramref name="objectiveNumber"/>'s timer (0 when none).</summary>
    float CheckObjectiveTimer(int32_t objectiveNumber);

    int32_t SetObjectiveStatus(int32_t objectiveNumber, uint32_t status);

    /// <summary>Objective <paramref name="objectiveNumber"/>'s status; 9999 for a bad index.</summary>
    uint32_t CheckObjectiveStatus(int32_t objectiveNumber);

    int32_t SetObjectiveType(int32_t objectiveNumber, uint32_t type);

    /// <summary>Objective <paramref name="objectiveNumber"/>'s type; 9999 for a bad index.</summary>
    uint32_t CheckObjectiveType(int32_t objectiveNumber);

    void SetObjectivePos(int32_t objectiveNumber, float x, float y, float z);

    /// <summary>
    /// The resource points the scenario earned: the points of the objectives that succeeded (all of them when the
    /// mission ended the scenario early), or 0 when the scenario was lost.
    /// </summary>
    /// <remarks>An inline member in the original, called from <c>MCMission::EndScenario</c>.</remarks>
    int32_t CalcResourcePointsEarned();

    /// <summary>Adds the unused-tonnage bonus as an extra, succeeded objective.</summary>
    void SetupBonus();

    /// <summary>
    /// Runs the brain's <c>handlemessage</c> function with a multiplayer message (the runtime's
    /// <c>MissionMessageCode</c> and <c>MissionMessageParam</c>).
    /// </summary>
    void HandleMultiplayMessage(int32_t code, int32_t param);

    /// <summary>Checks whether any of the player's 'Mechs is in combat (for the music).</summary>
    void CheckAnyoneInCombat();

    /// <summary>The open scenario FIT while <see cref="Init"/> runs.</summary>
    MCFitIniFile* ScenarioFile = nullptr;
    /// <summary><c>CameraHeapSize</c>.</summary>
    uint32_t CameraHeapSize = 0;
    /// <summary><c>CameraFileName</c>.</summary>
    char CameraFileName[80] = {};
    /// <summary><c>ObjectHeapSize</c>.</summary>
    uint32_t ObjectHeapSize = 0;
    /// <summary><c>ObjectTypeHeapSize</c>.</summary>
    uint32_t ObjectTypeHeapSize = 0;
    /// <summary><c>NumObjects</c>.</summary>
    uint32_t NumObjects = 0;
    /// <summary><c>ObjectFileName</c>.</summary>
    char ObjectFileName[80] = {};
    /// <summary><c>SpriteHeapSize</c>.</summary>
    uint32_t SpriteHeapSize = 0;
    /// <summary><c>SpriteFileName</c>.</summary>
    char SpriteFileName[80] = {};
    /// <summary><c>TerrainFileName</c>.</summary>
    char TerrainFileName[80] = {};
    /// <summary><c>ScenarioScript</c>: the ABL script (and the scenario's name on the results).</summary>
    char ScenarioScript[80] = {};
    /// <summary>The script's module handle from <c>ABLi_preProcess</c>; -1 before.</summary>
    int32_t ScenarioScriptHandle = -1;
    /// <summary>The scenario's ABL brain.</summary>
    std::unique_ptr<MCAblModule> ScenarioBrain;
    /// <summary>The parameters passed to the brain each frame.</summary>
    MCAblParam ScenarioBrainParams{};
    /// <summary>The brain's <c>handlemessage</c> function, if it has one.</summary>
    MCAblSymbol* ScenarioBrainHandleMessage = nullptr;
    /// <summary><c>PaletteSystem</c>: the palette file.</summary>
    char PaletteSystem[80] = {};
    /// <summary>The palette before the scenario's (the interface's), shown again by <see cref="Destroy"/>.</summary>
    std::unique_ptr<MCPalette> OldPalette;
    /// <summary><c>NumWarriors</c>.</summary>
    uint32_t NumWarriors = 0;
    /// <summary>The warriors, 1-based (<see cref="NumWarriors"/> + 1 entries).</summary>
    std::unique_ptr<MCMechWarrior*[]> Warriors;
    /// <summary><c>CaptureChance</c> (0-4; 2 when missing or out of range).</summary>
    uint8_t CaptureChance = 2;
    /// <summary><c>NumParts</c>.</summary>
    uint32_t NumParts = 0;
    /// <summary>The parts, 1-based (<see cref="NumParts"/> + 1 entries).</summary>
    std::unique_ptr<MCPart[]> Parts;
    /// <summary>Number of <see cref="Areas"/>.</summary>
    int32_t NumAreas = 0;
    /// <summary>The areas for <see cref="ObjectInArea"/> (never filled in MCX.EXE: numAreas stays 0).</summary>
    MCScenarioArea* Areas = nullptr;
    /// <summary>The objects made at the start but not yet in play (created later by the script).</summary>
    std::unique_ptr<MCObjectQueue> ScenarioObjectList;
    /// <summary><c>NumObjectives</c> (at most 9).</summary>
    uint32_t NumObjectives = 0;
    /// <summary><c>Duration</c> from the game system block.</summary>
    uint32_t Duration = 0;
    /// <summary>The objectives: always 9 entries when there are any.</summary>
    std::unique_ptr<MCScenarioObjective[]> Objectives;
    /// <summary><c>NumLargeStrikes</c> for the home commander.</summary>
    int32_t NumLargeStrikes = 0;
    /// <summary><c>NumSmallStrikes</c>.</summary>
    int32_t NumSmallStrikes = 0;
    /// <summary><c>NumSensorStrikes</c>.</summary>
    int32_t NumSensorStrikes = 0;
    /// <summary><c>NumCameraStrikes</c> (camera drones).</summary>
    int32_t NumCameraStrikes = 0;
    /// <summary><c>scenarioTuneNum</c>: the music track started after the start-up turns.</summary>
    uint8_t ScenarioTuneNum = 0;
    /// <summary>
    /// The six VFX shapes of the <c>SensorContactShape</c> packet file (packets 0-5): the sensor contact blips drawn
    /// for objects seen only on sensors.
    /// </summary>
    uint8_t* SensorContactShapes[6] = {};
    /// <summary><c>MaxVisualRange</c>.</summary>
    float MaxVisualRange = 0.0f;
    /// <summary><c>FireVisualRange</c>: how far a firing object is revealed.</summary>
    float FireVisualRange = 0.0f;
    /// <summary><c>MaxWeaponRange</c>.</summary>
    float MaxWeaponRange = 0.0f;
    /// <summary><c>BaseSensorRange</c>.</summary>
    float BaseSensorRange = 0.0f;
    /// <summary><c>AlwaysRevealed</c>: the whole map is revealed.</summary>
    uint8_t AlwaysRevealed = 0;
    /// <summary><c>GodMode</c>.</summary>
    uint8_t GodMode = 0;
    /// <summary>Set when the scenario FIT has an <c>Output</c> block.</summary>
    uint8_t HasOutputBlock = 0;
    /// <summary><c>CycleLength</c>.</summary>
    float CycleLength = 0.0f;
    /// <summary><c>TimeLeft</c>: the time limit in seconds; -1 = none.</summary>
    int32_t TimeLimit = -1;
    /// <summary>Set once the two-minute warning played.</summary>
    int32_t TwoMinuteWarningPlayed = 0;
    /// <summary>Set once the thirty-second warning played.</summary>
    int32_t ThirtySecondWarningPlayed = 0;
    /// <summary>Set during the start-up turns (the "loading" countdown on the interface).</summary>
    int32_t StartingUp = 0;
    /// <summary>Start-up turns left, times 10 (100 at the start).</summary>
    int32_t StartUpCountdown = 0;
    /// <summary>The turns the scenario runs before the player gets control (10).</summary>
    int32_t StartUpTurns = 0;
};

/// <summary>Scales <paramref name="skill"/> by the difficulty's skill percentage (for the player when <paramref name="player"/>).</summary>
float ApplyDifficultySkill(float skill, int player);

/// <summary>
/// Scales a weapon value by the difficulty's weapon percentage, rounds it to a quarter and clamps it to 0-255.
/// </summary>
float ApplyDifficultyWeapon(float value, int player);

/// <summary>Reads the <c>DifficultySettings</c> block of the game system file.</summary>
void InitDifficultySettings(MCFitIniFile* gameSystemFile);

/// <summary>The current scenario (null outside one).</summary>
extern MCScenario* Scenario;
/// <summary>The scenario clock used for timing: <c>scenarioTime</c>, or the real running time in multiplayer.</summary>
extern float ActualTime;
/// <summary>Zeroed each turn by <see cref="MCScenario::Update"/>.</summary>
extern int NextStep;
/// <summary>Zeroed each turn by <see cref="MCScenario::Update"/>.</summary>
extern int PrevStep;
/// <summary>The turn the scenario ends at; -1 = not ending.</summary>
extern int32_t ScenarioEndTurn;
/// <summary>The longest frame the simulation takes (0.25 s).</summary>
extern float MinFrameLength;
/// <summary>Set to -1.0 before each part is created in <see cref="MCScenario::Init"/>; nothing reads it.</summary>
extern float PartCreateTime;
/// <summary>Whether object collisions are checked (1).</summary>
extern int CollisionSwitch;
/// <summary>Tons of unused drop weight per bonus unit (5).</summary>
extern int32_t TonnageDivisor;
/// <summary>Resource points per bonus unit (200).</summary>
extern int32_t ResourcesPerTonDivided;
/// <summary>Every mover by part id (0xe00 entries).</summary>
extern MCBaseObject* MoverRoster[0xe00];
/// <summary>Mission "MineLayThrottle" (50 when missing): a mine layer's top throttle while laying.</summary>
extern int32_t MineLayThrottle;
/// <summary>Mission "MineSweepThrottle" (50 when missing): a sweeper's top throttle after clearing a mine.</summary>
extern int32_t MineSweepThrottle;
/// <summary>Mission "MineWaitTime": the seconds a mine layer waits on a cell before laying.</summary>
extern float MineWaitTime;
extern MCTrainManager* TrainManager;
/// <summary>The scenario's frame (turn) counter.</summary>
extern int32_t Turn;
/// <summary>The <c>VisualRangeTable</c> of the game system file (256 entries).</summary>
extern int32_t VisualRangeTable[256];
/// <summary>Difficulty weapon percentages for the player (<c>PlayerWeapons</c>: easy, hard).</summary>
extern int32_t GlobalPlayerWeapons[2];
/// <summary>Difficulty skill percentages for the enemy (<c>EnemySkills</c>).</summary>
extern int32_t GlobalEnemySkills[2];
/// <summary>The parts ABL scripts created (<c>currentCreatorPart</c> of them).</summary>
extern MCCreatedPartRoster CreatedPartRoster[100];
/// <summary>Difficulty weapon percentages for the enemy (<c>EnemyWeapons</c>).</summary>
extern int32_t GlobalEnemyWeapons[2];
/// <summary>Difficulty skill percentages for the player (<c>PlayerSkills</c>).</summary>
extern int32_t GlobalPlayerSkills[2];
/// <summary>Difficulty salvage percentages (<c>SalvageChance</c>).</summary>
extern int32_t GlobalSalvageModifier[2];
/// <summary>The real time the scenario started (milliseconds; 0 until the start-up turns are over).</summary>
extern uint32_t MissionStartTime;
/// <summary>Seconds since <c>MissionStartTime</c>.</summary>
extern float RunningTime;
/// <summary>Set when the scenario music should start after the start-up turns.</summary>
extern int32_t StartMusic;
extern float InfluenceTime;
extern int DrawRevealedTacMap;
/// <summary>Number of entries used in <c>createdPartRoster</c>.</summary>
extern int32_t CurrentCreatorPart;
/// <summary>The waypoint marker shapes (loaded from a file in <see cref="MCScenario::Init"/>).</summary>
extern uint8_t* WaypointMarkers;
extern int EndingScenario;
/// <summary><c>AlwaysDraw</c> from the game system file.</summary>
extern uint8_t ForceAlways;
/// <summary>
/// Where a saved game's copies of the scenario, warrior and object profile FITs are unpacked
/// (<c>"data\save\temp\"</c>); the scenario falls back on it when a file isn't in its usual folder.
/// </summary>
extern char SaveTempPath[80];
