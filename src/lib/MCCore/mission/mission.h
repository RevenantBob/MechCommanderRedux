#pragma once

// The mission: the game's top-level state machine between logistics, briefings, Smacker movies and scenarios
// (mission\mission.cpp). It reads the campaign's mission FIT (movies, scenarios, game segments), starts and ends each
// Scenario, and shows the mission results screen afterwards.

#include "gui/asystem.h"
#include "gui/awindow.h"

class aPort;
class aButton;
class aScrollTextObject;
class FitIniFile;
class Logistics;
class MechWarrior;
class Scenario;

/// <summary>
/// The game's top-level state machine. <see cref="run"/> is called every frame (through the <c>RunMission</c>
/// callback) and switches on <see cref="missionState"/>: logistics, movies, the scenario, the results screen, the
/// feature screen (demo) and quitting.
/// </summary>
/// <remarks>
/// Original source: <c>mission\mission.cpp</c>, 0x48 bytes, no vtable. The one instance is <c>mission</c>, created in
/// WinMain (the constructor was inlined there: it zeroes +0x0, +0x4, +0x8, +0xc, +0x2c, +0x30, +0x44 and sets +0x3c to
/// 1).
/// Values of <see cref="missionState"/> seen in <see cref="run"/>: 0 start (go to logistics), 3 logistics, 6 results
/// screen up, 7 scenario running, 8 movie playing (from logistics), 10 play movie <see cref="currentMovie"/>,
/// 11 start scenario <see cref="currentScenario"/>, 12/13 movie states of the game-segment path, 16 quit (posts
/// WM_CLOSE), 20 demo feature screen. They are also read from the FIT (<c>GameState</c>, <c>NextGameState</c>).
/// </remarks>
class Mission
{
public:
    /// <summary>Loads the mission FIT <paramref name="missionName"/> (campaign control file, or a game segment).</summary>
    /// <returns>0, or an error code.</returns>
    /// <remarks>MCX.EXE @ 0x00729b90</remarks>
    int32_t init(char* missionName);

    /// <summary>Reloads the movie and scenario lists from <paramref name="missionName"/> (a new campaign).</summary>
    /// <remarks>MCX.EXE @ 0x0072a7a0</remarks>
    int32_t initAgain(char* missionName);

    /// <summary>
    /// Reads the <c>GameSegment%d</c> block <paramref name="segment"/>: game state, movie, scenario and next state.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0072ab70</remarks>
    int32_t SetupNextSegment(int32_t segment);

    /// <summary>Clears the mission and registers the <c>RunMission</c> callback (once).</summary>
    /// <remarks>MCX.EXE @ 0x0072ac20</remarks>
    int32_t init();

    /// <summary>Frees the heap, FIT, callbacks, scenario and logistics.</summary>
    /// <remarks>MCX.EXE @ 0x0072ac80</remarks>
    void destroy();

    /// <summary>One frame of the state machine.</summary>
    /// <remarks>MCX.EXE @ 0x0072af00</remarks>
    int32_t run();

    /// <summary>
    /// Leaves logistics and starts scenario <paramref name="scenarioName"/>: creates the results screen and the
    /// <c>scenario</c>, and hooks <c>playScenario</c> and the interface callback.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0072b720</remarks>
    void StartScenario(char* scenarioName);

    /// <summary>
    /// Ends the scenario: writes the results for logistics, frees the scenario and goes back to logistics (or on to
    /// the next movie / game over).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0072bc20</remarks>
    void EndScenario();

    /// <summary>Writes the interface window states (zoom, tac map, palette) to <c>windows.fit</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0072c170</remarks>
    void saveWindowStatus();

    /// <summary>Restores the interface window states from <c>windows.fit</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0072c290</remarks>
    void loadWindowStatus();

    /// <summary>The state machine's current state (see the class remarks).</summary>
    int32_t missionState = 0; // +0x0
    /// <summary>The results screen shown after a scenario (created in <see cref="StartScenario"/>).</summary>
    class MissionResultsScreen* resultsScreen = nullptr; // +0x8
    /// <summary>The callback that runs <c>RunMission</c> every frame.</summary>
    aCallback* missionCallback = nullptr; // +0xc
    /// <summary>Number of entries in <see cref="movies"/> (<c>NumMovies</c>).</summary>
    uint32_t numMovies = 0; // +0x10
    /// <summary>Index into <see cref="movies"/> of the movie to play next (<c>SmackerMovieId</c>); -1 = none.</summary>
    int32_t currentMovie = 0; // +0x14
    /// <summary>The Smacker movie names (<c>Movie%d</c>).</summary>
    std::vector<std::string> movies; // +0x18
    /// <summary>Number of entries in <see cref="scenarios"/> (<c>NumScenarios</c>).</summary>
    uint32_t numScenarios = 0; // +0x1c
    /// <summary>Index into <see cref="scenarios"/> of the current scenario (<c>ScenarioId</c>); -1 = none.</summary>
    int32_t currentScenario = 0; // +0x20
    /// <summary>The scenario names (<c>Scenario%d</c>).</summary>
    std::vector<std::string> scenarios; // +0x24
    /// <summary>The open mission FIT.</summary>
    FitIniFile* missionFile = nullptr; // +0x30
    /// <summary>Copy of <see cref="waitTime"/> made at scenario start and end.</summary>
    float waitTimer = 0.0f; // +0x34
    /// <summary><c>WaitTime</c> from the FIT (120 when missing).</summary>
    float waitTime = 0.0f; // +0x38
    /// <summary>The logistics phase (also <c>globalLogPtr</c>), while it exists.</summary>
    Logistics* logistics = nullptr; // +0x40
    /// <summary>
    /// Nonzero when the scenario should end now, whatever the script says: 1 from the network, -1 when the player
    /// quits from the pause menu. <see cref="run"/> then opens the results screen, and the objectives' points all
    /// count.
    /// </summary>
    int32_t endScenarioRequested = 0; // +0x44
};

/// <summary>
/// The opening movie's window: plays like an <c>aSmackerWindow</c>, then wipes the last frame off the screen 20 lines
/// per frame and destroys itself.
/// </summary>
/// <remarks>Original source: <c>mission\mission.cpp</c>, 0x4c4 bytes (aSmackerWindow is 0x4c0).</remarks>
class aOpeningSmackerWindow : public aSmackerWindow
{
public:
    /// <summary>Registers the window with the application (<c>aSystem</c> +0xaf4) and opens it at <paramref name="frame"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0072c360</remarks>
    int32_t init(tagRECT* frame, tagPOINT* position);

    /// <summary>Plays the movie; after it ends, the wipe (see <see cref="wipeLine"/>).</summary>
    /// <remarks>MCX.EXE @ 0x0072c390 (vtable slot 50)</remarks>
    void display() override;

    /// <summary>Port: the movie frame; during the wipe, the last frame <see cref="wipeLine"/> lines down.</summary>
    void draw() override;

    /// <summary>Closes the movie and starts the wipe.</summary>
    /// <remarks>MCX.EXE @ 0x0072c420 (vtable slot 77)</remarks>
    void endSmackerMovie() override;

    /// <summary>The player skipped the movie: finishes the wipe at once and destroys the application's movie window.</summary>
    /// <remarks>MCX.EXE @ 0x0072c460</remarks>
    void escapeSmackerMovie();

    /// <summary>
    /// 0 while the movie plays; then the line the wipe has reached (grows by 20 per frame). The window destroys itself
    /// once it passes the window's height.
    /// </summary>
    int32_t wipeLine = 0; // +0x4c0
};

/// <summary>
/// One pilot's line on the results screen (0x1c bytes in the original; an array of them sorted with
/// <c>ComparePilots</c>).
/// </summary>
/// <remarks>
/// The name is the port's. Single player fills it as below. Multiplayer leaves <see cref="skills"/> at 0, keeps the
/// pilot's kills in <see cref="oldRank"/>, and sorts on kills * -10000 plus the armor and (twice) the internal
/// structure the pilot's 'Mech lost (see <c>MissionResultsScreen::activate</c>).
/// </remarks>
struct MissionPilotResult
{
    /// <summary>The pilot.</summary>
    MechWarrior* warrior = nullptr; // +0x0
    /// <summary>The pilot's four skills at the end of the scenario (truncated), before any skill-ups are applied.</summary>
    int32_t skills[4]{}; // +0x4
    /// <summary>The pilot's rank before this scenario (multiplayer: the pilot's kills).</summary>
    int32_t oldRank = 0; // +0x14
    /// <summary>
    /// Sort key: (3 - rank) * 10000 plus, for each of the first three letters of the callsign, (letter * 10) XOR
    /// (2, 1, 0) (OB-058).
    /// </summary>
    int32_t sortKey = 0; // +0x18
};

/// <summary>A commander's kill total on the multiplayer results screen (8 bytes; the name is the port's).</summary>
struct MissionCommanderScore
{
    /// <summary>The commander (the session's player number).</summary>
    int32_t commanderId = 0; // +0x0
    /// <summary>The kills of the commander's pilots; -1 when the commander has no pilots.</summary>
    int32_t score = 0; // +0x4
};

/// <summary>
/// The mission results screen: resource points, kill statistics, objectives and pilots, each drawn a step at a time
/// (timed with <see cref="nextDrawTime"/>; Escape skips the animation), then the "move on" button.
/// </summary>
/// <remarks>
/// Original source: <c>mission\mission.cpp</c>, 0x55c bytes (aObject is 0x4ac). vtable 0x00784058, 77 slots, the
/// aObject vtable with slots 0 (destructor), 2 (destroy), 21 (handleEvent) and 50 (display) overridden.
/// Values of <see cref="drawState"/>: 0 resource points, 1 statistics, 2 primary / 3 secondary objectives, 4 pilots,
/// 5 finish (shows the debriefing text), 6 done.
/// </remarks>
class MissionResultsScreen : public aObject
{
public:
    /// <remarks>MCX.EXE @ 0x0072bbf0 (vtable slot 0, the deleting destructor)</remarks>
    ~MissionResultsScreen() override;

    /// <summary>Opens the window: background, the move-on button, the objective marks and the scroll buttons.</summary>
    /// <remarks>MCX.EXE @ 0x0072c5b0</remarks>
    int32_t init();
    using aObject::init;

    /// <summary>
    /// Frees the screen's ports and buttons and, the first time, ends the scenario (<c>Mission::EndScenario</c>) and
    /// picks the mission's next state.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0072ca00 (vtable slot 2)</remarks>
    void destroy() override;

    /// <summary>Scroll-button clicks and auto-repeat timers, Escape (skip the animation), and the multiplayer timeout.</summary>
    /// <remarks>MCX.EXE @ 0x0072cc10 (vtable slot 21)</remarks>
    void handleEvent(aEvent* event) override;
    /// <summary>
    /// Port-only: the mouse wheel scrolls the text anywhere over the screen, as the scroll buttons do (single player
    /// only, as they are).
    /// </summary>
    bool MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) override;

    /// <summary>Dims the screen, advances the drawing steps that are due, and draws the window and its children.</summary>
    /// <remarks>MCX.EXE @ 0x0072cf00 (vtable slot 50)</remarks>
    void display() override;

    /// <summary>Port: the screen draws itself each frame (see <see cref="draw"/>).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>
    /// Port: the background, the move-on label, then what the steps have reached so far (single player) or the whole
    /// summary (multiplayer). The original painted each step into the window's picture as it came.
    /// </summary>
    void draw() override;

    /// <summary>
    /// Shows the multiplayer pilot list of the home side when <paramref name="showHomeSide"/> is nonzero, of the other
    /// side otherwise (from the pilot switch button's state). The original redrew the list here; it is drawn each frame
    /// from <see cref="mpShowHomeSide"/> now.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0072e190</remarks>
    void drawMPPilots(int showHomeSide);

    /// <summary>
    /// Fills the screen at the scenario's end: counts kills and losses, builds <see cref="pilotResults"/> (applying
    /// skill-ups), totals the resource points, and shows the window.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0072e820</remarks>
    int32_t activate();

    /// <summary>Port-only: whether every step is drawn (the debriefing text is up).</summary>
    bool Finished() const { return drawState == 6; }

protected:
    /// <summary>
    /// Counts the resource points up, one step per call (the step's timing and sound; <see cref="draw"/> shows
    /// <see cref="shownResourcePoints"/>).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0072d140</remarks>
    void drawRPs();
    /// <summary>Steps to the next kill/loss statistic.</summary>
    /// <remarks>MCX.EXE @ 0x0072d280</remarks>
    void drawStats();
    /// <summary>Steps to the next objective (and the tonnage bonus after the secondary ones).</summary>
    /// <remarks>MCX.EXE @ 0x0072d450</remarks>
    void drawObjectives();
    /// <summary>Steps to the next pilot, taking the picture of the pilot's icon.</summary>
    /// <remarks>MCX.EXE @ 0x0072d900</remarks>
    void drawPilots();
    /// <summary>
    /// Draws the objectives of a multiplayer game. (The original drew them once into the picture; <see cref="draw"/>
    /// calls this each frame.)
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0072f800</remarks>
    void drawMPObjectives();

    /// <summary>Port: the resource point box, once the count has started.</summary>
    void DrawResourcePoints();
    /// <summary>Port: the statistics the steps have reached.</summary>
    void DrawStatistics();
    /// <summary>Port: the objectives the steps have reached, laid out from the top of the list.</summary>
    void DrawObjectiveList();
    /// <summary>Port: pilot <paramref name="index"/>'s line, at its place in the single-player list.</summary>
    void DrawPilot(int32_t index);
    /// <summary>Port: the multiplayer pilot boxes, with the pilots of the side <see cref="mpShowHomeSide"/> picks.</summary>
    void DrawMPPilotList();
    /// <summary>Port: the multiplayer summary: the best pilot, the statistics, the time and the commanders.</summary>
    void DrawMPSummary();

    /// <summary>Which part of the screen is being drawn (see the class remarks).</summary>
    int32_t drawState = 0; // +0x4ac
    /// <summary>The item within the current part (objective, statistic, pilot).</summary>
    int32_t drawIndex = 0; // +0x4b0
    /// <summary>The y of the next line drawn in the objectives list.</summary>
    int32_t drawY = 0; // +0x4b4
    /// <summary>The debriefing text box (the tactical map's text object, borrowed while the screen is up).</summary>
    aScrollTextObject* textObject = nullptr; // +0x4b8
    /// <summary>The "objective succeeded" mark (<c>guimr08.tga</c>).</summary>
    aPort* successPort = nullptr; // +0x4bc
    /// <summary>The "objective failed" mark (<c>guimr07.tga</c>).</summary>
    aPort* failurePort = nullptr; // +0x4c0
    /// <summary>
    /// The move-on button's label image, picked in <see cref="activate"/>. (The original copied it onto the window there
    /// and freed it; the window draws it each frame.)
    /// </summary>
    aPort* moveOnPort = nullptr; // +0x4c4
    /// <summary>The "move on" button (closes the screen).</summary>
    aButton* moveOnButton = nullptr; // +0x4c8
    /// <summary>Multiplayer only: the button switching the pilot list's page (an aButton subclass, 0x4d8 bytes).</summary>
    aButton* pilotSwitchButton = nullptr; // +0x4cc
    /// <summary>The pilot lines (allocated in <see cref="activate"/>, <see cref="numPilotResults"/> of them).</summary>
    std::unique_ptr<MissionPilotResult[]> pilotResults; // +0x4d0
    int32_t numPilotResults = 0;                        // +0x4d4
    /// <summary>
    /// Statistic: enemy units destroyed or disabled (single player: the clan list, marines left out; multiplayer:
    /// the disabled units of the other teams' pilots).
    /// </summary>
    int32_t enemyMechsHit = 0; // +0x4d8
    /// <summary>Statistic: of those, the BattleMechs.</summary>
    int32_t enemyMechsDestroyed = 0; // +0x4dc
    /// <summary>Statistic: enemy pilots killed (warrior status 4).</summary>
    int32_t enemyPilotsKilled = 0; // +0x4e0
    /// <summary>Statistic: player units destroyed or disabled.</summary>
    int32_t playerMechsHit = 0; // +0x4e4
    /// <summary>Statistic: of those, the BattleMechs (single player: only ones with a network player id).</summary>
    int32_t playerMechsDestroyed = 0; // +0x4e8
    /// <summary>Resource points earned from the objectives that succeeded.</summary>
    int32_t resourcePointsEarned = 0; // +0x4ec
    /// <summary>The scroll-up button's hot spot, in window coordinates.</summary>
    tagRECT scrollUpRect{}; // +0x4f0
    /// <summary>The scroll-down button's hot spot.</summary>
    tagRECT scrollDownRect{}; // +0x500
    /// <summary>The scroll bar's hot spot (a click there scrolls to the clicked line).</summary>
    tagRECT scrollBarRect{}; // +0x510
    /// <summary>The scroll-up button's pressed image (shown while held).</summary>
    aObject* scrollUpButton = nullptr; // +0x520
    /// <summary>The scroll-down button's pressed image.</summary>
    aObject* scrollDownButton = nullptr; // +0x524
    /// <summary><c>MouseTicks</c> at which the next drawing step is due; 0 when drawing is finished.</summary>
    uint32_t nextDrawTime = 0; // +0x54c
    /// <summary>Set once the secondary objectives' header is drawn.</summary>
    int32_t objectivesHeaderDrawn = 0; // +0x550
    /// <summary>Set by Escape: the remaining steps are drawn at once, without sounds.</summary>
    int32_t skipAnimation = 0; // +0x554
    /// <summary>Set once <see cref="destroy"/> has ended the scenario.</summary>
    int32_t scenarioEnded = 0; // +0x558

    /// <summary>A commander's line on the multiplayer screen, as <see cref="activate"/> found it.</summary>
    struct CommanderLine
    {
        /// <summary>The place (1-6) by kills.</summary>
        int32_t place = 0;
        /// <summary>The player's name (kept: the session may be left before the screen closes).</summary>
        std::string name;
        int32_t score = 0;
    };

    /// <summary>Port: the resource points the count shows; -1 before the count starts.</summary>
    int32_t shownResourcePoints = -1;
    /// <summary>
    /// Port: each pilot line's icon picture (frame trimmed), taken when the line comes up (multiplayer: in
    /// <see cref="activate"/>); null when the pilot has no icon.
    /// </summary>
    std::vector<aPort*> pilotIcons;
    /// <summary>Port: the multiplayer best pilot's icon picture (taken before the list's, as the original drew it).</summary>
    aPort* bestPilotIcon = nullptr;
    /// <summary>Port: which side the multiplayer pilot list shows (see <see cref="drawMPPilots"/>).</summary>
    int32_t mpShowHomeSide = 1;
    /// <summary>Port: the multiplayer commanders by kills.</summary>
    std::vector<CommanderLine> commanderLines;
    /// <summary>Port: the multiplayer game's length, "mm:ss".</summary>
    char timeText[32] = {};
};

/// <summary>The scenario callback: runs the scenario one frame and updates the sound system.</summary>
/// <remarks>MCX.EXE @ 0x00729ae0</remarks>
void playScenario();

/// <summary>The mission callback: runs <c>mission</c> one frame (and keeps <c>frameLength</c> between scenarios).</summary>
/// <remarks>MCX.EXE @ 0x00729b40</remarks>
void RunMission();

/// <summary><c>qsort</c> order of <see cref="MissionPilotResult"/> by <c>sortKey</c>, ascending.</summary>
/// <remarks>MCX.EXE @ 0x0072c4c0</remarks>
int ComparePilots(const void* a, const void* b);

/// <summary><c>qsort</c> order of <see cref="MissionCommanderScore"/> by <c>score</c>, descending.</summary>
/// <remarks>MCX.EXE @ 0x0072c500</remarks>
int CompareCommanders(const void* a, const void* b);

/// <summary>The move-on button's event routine: on release, destroys the results screen.</summary>
/// <remarks>MCX.EXE @ 0x0072c540</remarks>
void moveOnButtonHandleEvent(aObject* object, aEvent* event);

/// <summary>The pilot switch button's event routine: redraws the multiplayer pilot list for the button's state.</summary>
/// <remarks>MCX.EXE @ 0x0072c580</remarks>
void PilotSwitchHandleEvent(aObject* object, aEvent* event);

/// <summary>Ticks between two drawing steps of the results screen (20).</summary>
/// <remarks>MCX.EXE @ 0x0078ab0c (unnamed in the binary; the name is the port's).</remarks>
extern uint32_t resultsStepTicks;
/// <summary>The skill order the results screen lists (the four <c>Skill</c> values 3, 0, 1, 2).</summary>
/// <remarks>MCX.EXE @ 0x007a166c. Its type is <c>Skill[4]</c> (object/warrior.h, not written yet).</remarks>
extern int32_t StevesOrderLUT[4];
/// <summary>The game segment a demo/segment build starts at; 0 for the normal campaign.</summary>
/// <remarks>MCX.EXE @ 0x007ab15c</remarks>
extern int32_t globalGameSegment;
/// <summary>The interface's millisecond clock the results screen times its steps with.</summary>
/// <remarks>MCX.EXE @ 0x007bbbac</remarks>
extern uint32_t MouseTicks;
/// <summary>Resource points at the scenario's start.</summary>
/// <remarks>MCX.EXE @ 0x0080863c</remarks>
extern int32_t StartingResourcePoints;
/// <remarks>MCX.EXE @ 0x00808750</remarks>
extern float MinPilotSkill;
/// <summary>When the current logistics phase began.</summary>
/// <remarks>MCX.EXE @ 0x00808758</remarks>
extern SYSTEMTIME logisticsStart;
/// <summary>When the logistics phase ended (scenario start).</summary>
/// <remarks>MCX.EXE @ 0x00808768</remarks>
extern SYSTEMTIME logisticsEnd;
/// <summary>The highest skill a pilot can reach by skill-ups.</summary>
/// <remarks>MCX.EXE @ 0x0080877c</remarks>
extern float MaxPilotSkill;
/// <summary>Runs <c>playScenario</c> every frame while a scenario is up.</summary>
/// <remarks>MCX.EXE @ 0x00808784</remarks>
extern aCallback* scenarioCallback;
/// <summary>Runs <c>UpdateMouseStateCallback</c> every frame while a scenario is up.</summary>
/// <remarks>MCX.EXE @ 0x00808788</remarks>
extern aCallback* interfaceUpdateCallback;
/// <summary>The state to go to after the current one (<c>NextGameState</c> in the mission FIT).</summary>
/// <remarks>MCX.EXE @ 0x00808790 (unnamed in the binary; the name is the port's).</remarks>
extern int32_t nextGameState;
/// <summary>Set when the last scenario of the campaign is won.</summary>
/// <remarks>MCX.EXE @ 0x00808794</remarks>
extern int gameOver;
/// <summary>Whether the demo's feature-screen music has started.</summary>
/// <remarks>MCX.EXE @ 0x008087a0</remarks>
extern int featureMusicPlaying;
/// <summary>The campaign's last scenario (<c>LastScenario</c>).</summary>
/// <remarks>MCX.EXE @ 0x008087a4</remarks>
extern uint32_t lastScenario;
/// <summary>The mission.</summary>
/// <remarks>MCX.EXE @ 0x008087a8</remarks>
extern Mission* mission;
/// <summary>Which logistics music is playing: 0 none, 1 logistics (track 0x17), 2 briefing (track 0x16).</summary>
/// <remarks>MCX.EXE @ 0x008087ac</remarks>
extern int32_t playingLogisticsMusic;
/// <summary>Deleted in <c>Mission::EndScenario</c> if set.</summary>
/// <remarks>MCX.EXE @ 0x008087b8</remarks>
extern aCallback* setupCallback;
/// <summary>
/// The scenario's outcome from its ABL script (0 while running; below 3 lost, 3 quit, above 3 won).
/// </summary>
/// <remarks>MCX.EXE @ 0x00809408</remarks>
extern uint32_t scenarioResult;
/// <summary>Whether any fire is burning (for the fire sound).</summary>
/// <remarks>MCX.EXE @ 0x00809470</remarks>
extern int somethingOnFire;
/// <summary>The mission, scenario and ABL script files (<c>"data\missions\"</c>).</summary>
/// <remarks>MCX.EXE @ 0x0079433c (one of the 80-byte path globals).</remarks>
extern char missionPath[80];
/// <summary>The Smacker movies (<c>"data\movies\"</c>).</summary>
/// <remarks>MCX.EXE @ 0x007946ac (one of the 80-byte path globals).</remarks>
extern char CDmoviePath[80];
/// <summary>The pilots' radio videos (<c>"data\movies\"</c>).</summary>
/// <remarks>MCX.EXE @ 0x0079447c. It is defined beside <see cref="CDmoviePath"/>.</remarks>
extern char moviePath[80];
// The next five sit among mission.cpp's globals in the image (0x0080878c-0x008087b8), though globals_by_file.md
// assigns them to heavier users (gui\asystem.cpp, logistics.cpp, logistics\misslog.cpp, network\multplyr.cpp).

/// <summary>Whether cheats are enabled.</summary>
/// <remarks>MCX.EXE @ 0x0080879c</remarks>
extern int cheatsOn;
/// <summary>Whether this is the demo (<c>InDemo</c> in the mission FIT).</summary>
/// <remarks>MCX.EXE @ 0x00808798</remarks>
extern int InDemo;
/// <summary>Seconds the last scenario took.</summary>
/// <remarks>MCX.EXE @ 0x008087b0</remarks>
extern float totalScenarioTime;
/// <summary>Seconds the last logistics phase took.</summary>
/// <remarks>MCX.EXE @ 0x008087b4</remarks>
extern float totalLogisticsTime;
/// <summary>Whether input events go to the results screen (the scenario is over).</summary>
/// <remarks>MCX.EXE @ 0x0080878c</remarks>
extern int EventsToMissionResultsScreen;
