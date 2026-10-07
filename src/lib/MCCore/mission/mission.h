#pragma once

// The mission: the game's top-level state machine between logistics, briefings, Smacker movies and scenarios
// (mission\mission.cpp). It reads the campaign's mission FIT (movies, scenarios, game segments), starts and ends each
// Scenario, and shows the mission results screen afterwards.

#include "gui/asystem.h"
#include "gui/awindow.h"

class MCGuiPort;
class MCGuiButton;
class MCGuiScrollTextObject;
class MCFitIniFile;
class MCLogistics;
class MCMechWarrior;
class MCScenario;

/// <summary>
/// The game's top-level state machine. <see cref="Run"/> is called every frame (through the <c>RunMission</c>
/// callback) and switches on <see cref="MissionState"/>: logistics, movies, the scenario, the results screen, the
/// feature screen (demo) and quitting.
/// </summary>
/// <remarks>
/// Original source: <c>mission\mission.cpp</c>, 0x48 bytes, no vtable. The one instance is <c>mission</c>, created in
/// WinMain (the constructor was inlined there: it zeroes +0x0, +0x4, +0x8, +0xc, +0x2c, +0x30, +0x44 and sets +0x3c to
/// 1).
/// Values of <see cref="MissionState"/> seen in <see cref="Run"/>: 0 start (go to logistics), 3 logistics, 6 results
/// screen up, 7 scenario running, 8 movie playing (from logistics), 10 play movie <see cref="CurrentMovie"/>,
/// 11 start scenario <see cref="CurrentScenario"/>, 12/13 movie states of the game-segment path, 16 quit (posts
/// WM_CLOSE), 20 demo feature screen. They are also read from the FIT (<c>GameState</c>, <c>NextGameState</c>).
/// </remarks>
class MCMission
{
public:
    /// <summary>Loads the mission FIT <paramref name="missionName"/> (campaign control file, or a game segment).</summary>
    /// <returns>0, or an error code.</returns>
    int32_t Init(char* missionName);

    /// <summary>Reloads the movie and scenario lists from <paramref name="missionName"/> (a new campaign).</summary>
    int32_t InitAgain(char* missionName);

    /// <summary>
    /// Reads the <c>GameSegment%d</c> block <paramref name="segment"/>: game state, movie, scenario and next state.
    /// </summary>
    int32_t SetupNextSegment(int32_t segment);

    /// <summary>Clears the mission and registers the <c>RunMission</c> callback (once).</summary>
    int32_t Init();

    /// <summary>Frees the heap, FIT, callbacks, scenario and logistics.</summary>
    void Destroy();

    /// <summary>One frame of the state machine.</summary>
    int32_t Run();

    /// <summary>
    /// Leaves logistics and starts scenario <paramref name="scenarioName"/>: creates the results screen and the
    /// <c>scenario</c>, and hooks <c>playScenario</c> and the interface callback.
    /// </summary>
    void StartScenario(char* scenarioName);

    /// <summary>
    /// Ends the scenario: writes the results for logistics, frees the scenario and goes back to logistics (or on to
    /// the next movie / game over).
    /// </summary>
    void EndScenario();

    /// <summary>Writes the interface window states (zoom, tac map, palette) to <c>windows.fit</c>.</summary>
    void SaveWindowStatus();

    /// <summary>Restores the interface window states from <c>windows.fit</c>.</summary>
    void LoadWindowStatus();

    /// <summary>The state machine's current state (see the class remarks).</summary>
    int32_t MissionState = 0;
    /// <summary>The results screen shown after a scenario (created in <see cref="StartScenario"/>).</summary>
    class MCMissionResultsScreen* ResultsScreen = nullptr;
    /// <summary>The callback that runs <c>RunMission</c> every frame.</summary>
    MCGuiCallback* MissionCallback = nullptr;
    /// <summary>Number of entries in <see cref="Movies"/> (<c>NumMovies</c>).</summary>
    uint32_t NumMovies = 0;
    /// <summary>Index into <see cref="Movies"/> of the movie to play next (<c>SmackerMovieId</c>); -1 = none.</summary>
    int32_t CurrentMovie = 0;
    /// <summary>The Smacker movie names (<c>Movie%d</c>).</summary>
    std::vector<std::string> Movies;
    /// <summary>Number of entries in <see cref="Scenarios"/> (<c>NumScenarios</c>).</summary>
    uint32_t NumScenarios = 0;
    /// <summary>Index into <see cref="Scenarios"/> of the current scenario (<c>ScenarioId</c>); -1 = none.</summary>
    int32_t CurrentScenario = 0;
    /// <summary>The scenario names (<c>Scenario%d</c>).</summary>
    std::vector<std::string> Scenarios;
    /// <summary>The open mission FIT.</summary>
    MCFitIniFile* MissionFile = nullptr;
    /// <summary>Copy of <see cref="WaitTime"/> made at scenario start and end.</summary>
    float WaitTimer = 0.0f;
    /// <summary><c>WaitTime</c> from the FIT (120 when missing).</summary>
    float WaitTime = 0.0f;
    /// <summary>The logistics phase (also <c>globalLogPtr</c>), while it exists.</summary>
    MCLogistics* Logistics = nullptr;
    /// <summary>
    /// Nonzero when the scenario should end now, whatever the script says: 1 from the network, -1 when the player
    /// quits from the pause menu. <see cref="Run"/> then opens the results screen, and the objectives' points all
    /// count.
    /// </summary>
    int32_t EndScenarioRequested = 0;
};

/// <summary>
/// The opening movie's window: plays like an <c>aSmackerWindow</c>, then wipes the last frame off the screen 20 lines
/// per frame and destroys itself.
/// </summary>
/// <remarks>Original source: <c>mission\mission.cpp</c>, 0x4c4 bytes (aSmackerWindow is 0x4c0).</remarks>
class MCGuiOpeningSmackerWindow : public MCGuiSmackerWindow
{
public:
    /// <summary>Registers the window with the application (<c>aSystem</c> +0xaf4) and opens it at <paramref name="frame"/>.</summary>
    int32_t Init(tagRECT* frame, tagPOINT* position);

    /// <summary>Plays the movie; after it ends, the wipe (see <see cref="WipeLine"/>).</summary>
    void Display() override;

    /// <summary>Port: the movie frame; during the wipe, the last frame <see cref="WipeLine"/> lines down.</summary>
    void Draw() override;

    /// <summary>Closes the movie and starts the wipe.</summary>
    void EndSmackerMovie() override;

    /// <summary>The player skipped the movie: finishes the wipe at once and destroys the application's movie window.</summary>
    void EscapeSmackerMovie();

    /// <summary>
    /// 0 while the movie plays; then the line the wipe has reached (grows by 20 per frame). The window destroys itself
    /// once it passes the window's height.
    /// </summary>
    int32_t WipeLine = 0;
};

/// <summary>
/// One pilot's line on the results screen (0x1c bytes in the original; an array of them sorted with
/// <c>ComparePilots</c>).
/// </summary>
/// <remarks>
/// The name is the port's. Single player fills it as below. Multiplayer leaves <see cref="Skills"/> at 0, keeps the
/// pilot's kills in <see cref="OldRank"/>, and sorts on kills * -10000 plus the armor and (twice) the internal
/// structure the pilot's 'Mech lost (see <c>MissionResultsScreen::activate</c>).
/// </remarks>
struct MCMissionPilotResult
{
    /// <summary>The pilot.</summary>
    MCMechWarrior* Warrior = nullptr;
    /// <summary>The pilot's four skills at the end of the scenario (truncated), before any skill-ups are applied.</summary>
    int32_t Skills[4]{};
    /// <summary>The pilot's rank before this scenario (multiplayer: the pilot's kills).</summary>
    int32_t OldRank = 0;
    /// <summary>
    /// Sort key: (3 - rank) * 10000 plus, for each of the first three letters of the callsign, (letter * 10) XOR
    /// (2, 1, 0) (OB-058).
    /// </summary>
    int32_t SortKey = 0;
};

/// <summary>A commander's kill total on the multiplayer results screen (8 bytes; the name is the port's).</summary>
struct MCMissionCommanderScore
{
    /// <summary>The commander (the session's player number).</summary>
    int32_t CommanderId = 0;
    /// <summary>The kills of the commander's pilots; -1 when the commander has no pilots.</summary>
    int32_t Score = 0;
};

/// <summary>
/// The mission results screen: resource points, kill statistics, objectives and pilots, each drawn a step at a time
/// (timed with <see cref="_NextDrawTime"/>; Escape skips the animation), then the "move on" button.
/// </summary>
/// <remarks>
/// Original source: <c>mission\mission.cpp</c>, 0x55c bytes (aObject is 0x4ac). vtable 0x00784058, 77 slots, the
/// aObject vtable with slots 0 (destructor), 2 (destroy), 21 (handleEvent) and 50 (display) overridden.
/// Values of <see cref="_DrawState"/>: 0 resource points, 1 statistics, 2 primary / 3 secondary objectives, 4 pilots,
/// 5 finish (shows the debriefing text), 6 done.
/// </remarks>
class MCMissionResultsScreen : public MCGuiObject
{
public:
    ~MCMissionResultsScreen() override;

    /// <summary>Opens the window: background, the move-on button, the objective marks and the scroll buttons.</summary>
    int32_t Init();
    using MCGuiObject::Init;

    /// <summary>
    /// Frees the screen's ports and buttons and, the first time, ends the scenario (<c>Mission::EndScenario</c>) and
    /// picks the mission's next state.
    /// </summary>
    void Destroy() override;

    /// <summary>Scroll-button clicks and auto-repeat timers, Escape (skip the animation), and the multiplayer timeout.</summary>
    void HandleEvent(MCGuiEvent* event) override;
    /// <summary>
    /// Port-only: the mouse wheel scrolls the text anywhere over the screen, as the scroll buttons do (single player
    /// only, as they are).
    /// </summary>
    bool MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) override;

    /// <summary>Dims the screen, advances the drawing steps that are due, and draws the window and its children.</summary>
    void Display() override;

    /// <summary>Port: the screen draws itself each frame (see <see cref="Draw"/>).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>
    /// Port: the background, the move-on label, then what the steps have reached so far (single player) or the whole
    /// summary (multiplayer). The original painted each step into the window's picture as it came.
    /// </summary>
    void Draw() override;

    /// <summary>
    /// Shows the multiplayer pilot list of the home side when <paramref name="showHomeSide"/> is nonzero, of the other
    /// side otherwise (from the pilot switch button's state). The original redrew the list here; it is drawn each frame
    /// from <see cref="_MpShowHomeSide"/> now.
    /// </summary>
    void DrawMPPilots(int showHomeSide);

    /// <summary>
    /// Fills the screen at the scenario's end: counts kills and losses, builds <see cref="_PilotResults"/> (applying
    /// skill-ups), totals the resource points, and shows the window.
    /// </summary>
    int32_t Activate();

    /// <summary>Port-only: whether every step is drawn (the debriefing text is up).</summary>
    bool Finished() const { return _DrawState == 6; }

protected:
    /// <summary>
    /// Counts the resource points up, one step per call (the step's timing and sound; <see cref="Draw"/> shows
    /// <see cref="_ShownResourcePoints"/>).
    /// </summary>
    void DrawRPs();
    /// <summary>Steps to the next kill/loss statistic.</summary>
    void DrawStats();
    /// <summary>Steps to the next objective (and the tonnage bonus after the secondary ones).</summary>
    void DrawObjectives();
    /// <summary>Steps to the next pilot, taking the picture of the pilot's icon.</summary>
    void DrawPilots();
    /// <summary>
    /// Draws the objectives of a multiplayer game. (The original drew them once into the picture; <see cref="Draw"/>
    /// calls this each frame.)
    /// </summary>
    void DrawMPObjectives();

    /// <summary>Port: the resource point box, once the count has started.</summary>
    void DrawResourcePoints();
    /// <summary>Port: the statistics the steps have reached.</summary>
    void DrawStatistics();
    /// <summary>Port: the objectives the steps have reached, laid out from the top of the list.</summary>
    void DrawObjectiveList();
    /// <summary>Port: pilot <paramref name="index"/>'s line, at its place in the single-player list.</summary>
    void DrawPilot(int32_t index);
    /// <summary>Port: the multiplayer pilot boxes, with the pilots of the side <see cref="_MpShowHomeSide"/> picks.</summary>
    void DrawMPPilotList();
    /// <summary>Port: the multiplayer summary: the best pilot, the statistics, the time and the commanders.</summary>
    void DrawMPSummary();

    /// <summary>Which part of the screen is being drawn (see the class remarks).</summary>
    int32_t _DrawState = 0;
    /// <summary>The item within the current part (objective, statistic, pilot).</summary>
    int32_t _DrawIndex = 0;
    /// <summary>The y of the next line drawn in the objectives list.</summary>
    int32_t _DrawY = 0;
    /// <summary>The debriefing text box (the tactical map's text object, borrowed while the screen is up).</summary>
    MCGuiScrollTextObject* _TextObject = nullptr;
    /// <summary>The "objective succeeded" mark (<c>guimr08.tga</c>).</summary>
    MCGuiPort* _SuccessPort = nullptr;
    /// <summary>The "objective failed" mark (<c>guimr07.tga</c>).</summary>
    MCGuiPort* _FailurePort = nullptr;
    /// <summary>
    /// The move-on button's label image, picked in <see cref="Activate"/>. (The original copied it onto the window there
    /// and freed it; the window draws it each frame.)
    /// </summary>
    MCGuiPort* _MoveOnPort = nullptr;
    /// <summary>The "move on" button (closes the screen).</summary>
    MCGuiButton* _MoveOnButton = nullptr;
    /// <summary>Multiplayer only: the button switching the pilot list's page (an aButton subclass, 0x4d8 bytes).</summary>
    MCGuiButton* _PilotSwitchButton = nullptr;
    /// <summary>The pilot lines (allocated in <see cref="Activate"/>, <see cref="_NumPilotResults"/> of them).</summary>
    std::unique_ptr<MCMissionPilotResult[]> _PilotResults;
    int32_t _NumPilotResults = 0;
    /// <summary>
    /// Statistic: enemy units destroyed or disabled (single player: the clan list, marines left out; multiplayer:
    /// the disabled units of the other teams' pilots).
    /// </summary>
    int32_t _EnemyMechsHit = 0;
    /// <summary>Statistic: of those, the BattleMechs.</summary>
    int32_t _EnemyMechsDestroyed = 0;
    /// <summary>Statistic: enemy pilots killed (warrior status 4).</summary>
    int32_t _EnemyPilotsKilled = 0;
    /// <summary>Statistic: player units destroyed or disabled.</summary>
    int32_t _PlayerMechsHit = 0;
    /// <summary>Statistic: of those, the BattleMechs (single player: only ones with a network player id).</summary>
    int32_t _PlayerMechsDestroyed = 0;
    /// <summary>Resource points earned from the objectives that succeeded.</summary>
    int32_t _ResourcePointsEarned = 0;
    /// <summary>The scroll-up button's hot spot, in window coordinates.</summary>
    tagRECT _ScrollUpRect{};
    /// <summary>The scroll-down button's hot spot.</summary>
    tagRECT _ScrollDownRect{};
    /// <summary>The scroll bar's hot spot (a click there scrolls to the clicked line).</summary>
    tagRECT _ScrollBarRect{};
    /// <summary>The scroll-up button's pressed image (shown while held).</summary>
    MCGuiObject* _ScrollUpButton = nullptr;
    /// <summary>The scroll-down button's pressed image.</summary>
    MCGuiObject* _ScrollDownButton = nullptr;
    /// <summary><c>MouseTicks</c> at which the next drawing step is due; 0 when drawing is finished.</summary>
    uint32_t _NextDrawTime = 0;
    /// <summary>Set once the secondary objectives' header is drawn.</summary>
    int32_t _ObjectivesHeaderDrawn = 0;
    /// <summary>Set by Escape: the remaining steps are drawn at once, without sounds.</summary>
    int32_t _SkipAnimation = 0;
    /// <summary>Set once <see cref="Destroy"/> has ended the scenario.</summary>
    int32_t _ScenarioEnded = 0;

    /// <summary>A commander's line on the multiplayer screen, as <see cref="Activate"/> found it.</summary>
    struct CommanderLine
    {
        /// <summary>The place (1-6) by kills.</summary>
        int32_t Place = 0;
        /// <summary>The player's name (kept: the session may be left before the screen closes).</summary>
        std::string Name;
        int32_t Score = 0;
    };

    /// <summary>Port: the resource points the count shows; -1 before the count starts.</summary>
    int32_t _ShownResourcePoints = -1;
    /// <summary>
    /// Port: each pilot line's icon picture (frame trimmed), taken when the line comes up (multiplayer: in
    /// <see cref="Activate"/>); null when the pilot has no icon.
    /// </summary>
    std::vector<MCGuiPort*> _PilotIcons;
    /// <summary>Port: the multiplayer best pilot's icon picture (taken before the list's, as the original drew it).</summary>
    MCGuiPort* _BestPilotIcon = nullptr;
    /// <summary>Port: which side the multiplayer pilot list shows (see <see cref="DrawMPPilots"/>).</summary>
    int32_t _MpShowHomeSide = 1;
    /// <summary>Port: the multiplayer commanders by kills.</summary>
    std::vector<CommanderLine> _CommanderLines;
    /// <summary>Port: the multiplayer game's length, "mm:ss".</summary>
    char _TimeText[32] = {};
};

/// <summary>The scenario callback: runs the scenario one frame and updates the sound system.</summary>
void PlayScenario();

/// <summary>The mission callback: runs <c>mission</c> one frame (and keeps <c>frameLength</c> between scenarios).</summary>
void RunMission();

/// <summary><c>qsort</c> order of <see cref="MCMissionPilotResult"/> by <c>sortKey</c>, ascending.</summary>
int ComparePilots(const void* a, const void* b);

/// <summary><c>qsort</c> order of <see cref="MCMissionCommanderScore"/> by <c>score</c>, descending.</summary>
int CompareCommanders(const void* a, const void* b);

/// <summary>The move-on button's event routine: on release, destroys the results screen.</summary>
void MoveOnButtonHandleEvent(MCGuiObject* object, MCGuiEvent* event);

/// <summary>The pilot switch button's event routine: redraws the multiplayer pilot list for the button's state.</summary>
void PilotSwitchHandleEvent(MCGuiObject* object, MCGuiEvent* event);

/// <summary>Ticks between two drawing steps of the results screen (20).</summary>
extern uint32_t ResultsStepTicks;
/// <summary>The skill order the results screen lists (the four <c>Skill</c> values 3, 0, 1, 2).</summary>
extern int32_t StevesOrderLut[4];
/// <summary>The game segment a demo/segment build starts at; 0 for the normal campaign.</summary>
extern int32_t GlobalGameSegment;
/// <summary>The interface's millisecond clock the results screen times its steps with.</summary>
extern uint32_t MouseTicks;
/// <summary>Resource points at the scenario's start.</summary>
extern int32_t StartingResourcePoints;
extern float MinPilotSkill;
/// <summary>When the current logistics phase began.</summary>
extern SYSTEMTIME LogisticsStart;
/// <summary>When the logistics phase ended (scenario start).</summary>
extern SYSTEMTIME LogisticsEnd;
/// <summary>The highest skill a pilot can reach by skill-ups.</summary>
extern float MaxPilotSkill;
/// <summary>Runs <c>playScenario</c> every frame while a scenario is up.</summary>
extern MCGuiCallback* ScenarioCallback;
/// <summary>Runs <c>UpdateMouseStateCallback</c> every frame while a scenario is up.</summary>
extern MCGuiCallback* InterfaceUpdateCallback;
/// <summary>The state to go to after the current one (<c>NextGameState</c> in the mission FIT).</summary>
extern int32_t NextGameState;
/// <summary>Set when the last scenario of the campaign is won.</summary>
extern int GameOver;
/// <summary>Whether the demo's feature-screen music has started.</summary>
extern int FeatureMusicPlaying;
/// <summary>The campaign's last scenario (<c>LastScenario</c>).</summary>
extern uint32_t LastScenario;
/// <summary>The mission.</summary>
extern MCMission* Mission;
/// <summary>Which logistics music is playing: 0 none, 1 logistics (track 0x17), 2 briefing (track 0x16).</summary>
extern int32_t PlayingLogisticsMusic;
/// <summary>Deleted in <c>Mission::EndScenario</c> if set.</summary>
extern MCGuiCallback* SetupCallback;
/// <summary>
/// The scenario's outcome from its ABL script (0 while running; below 3 lost, 3 quit, above 3 won).
/// </summary>
extern uint32_t ScenarioResult;
/// <summary>Whether any fire is burning (for the fire sound).</summary>
extern int SomethingOnFire;
/// <summary>The mission, scenario and ABL script files (<c>"data\missions\"</c>).</summary>
extern char MissionPath[80];
/// <summary>The Smacker movies (<c>"data\movies\"</c>).</summary>
extern char CDmoviePath[80];
/// <summary>The pilots' radio videos (<c>"data\movies\"</c>).</summary>
extern char MoviePath[80];
// The next five sit among mission.cpp's globals in the image (0x0080878c-0x008087b8), though globals_by_file.md
// assigns them to heavier users (gui\asystem.cpp, logistics.cpp, logistics\misslog.cpp, network\multplyr.cpp).

/// <summary>Whether cheats are enabled.</summary>
extern int CheatsOn;
/// <summary>Whether this is the demo (<c>InDemo</c> in the mission FIT).</summary>
extern int InDemo;
/// <summary>Seconds the last scenario took.</summary>
extern float TotalScenarioTime;
/// <summary>Seconds the last logistics phase took.</summary>
extern float TotalLogisticsTime;
/// <summary>Whether input events go to the results screen (the scenario is over).</summary>
extern int EventsToMissionResultsScreen;
