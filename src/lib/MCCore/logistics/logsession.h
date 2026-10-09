#pragma once

#include "logistics/loggen.h"
#include "logistics/lport.h"

class MCGuiEvent;
class MCGuiFont;
class MCLogChatWindow;

/// <summary>
/// A logistics toggle button: a click flips <see cref="Toggled"/> (through <see cref="LToolButtonEventHandler"/>),
/// and it shows its down picture while toggled. Session screen tabs and radio groups use <see cref="Group"/> and
/// <see cref="Value"/>.
/// </summary>
/// <remarks>
/// Original source: <c>logistics\logsession.cpp</c>, 0x4f4 bytes (vtable 0x00781d14, missing from the vtable
/// dump).
/// </remarks>
class MCLogToolButton : public MCLogButton
{
public:
    ~MCLogToolButton() override = default;

    /// <summary>Initialises the button and installs <see cref="LToolButtonEventHandler"/>.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;

    /// <summary>Plays the click sound (or the disabled one) and passes the event on.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Shows the gray (disabled), down (toggled), over (mouse over) or up picture; the back colour when that picture
    /// is missing. Port: drawn each frame from the state; outside the frame pass a call is the original's paint.
    /// </summary>
    void Draw() override;

    /// <summary>Nonzero while toggled on.</summary>
    int32_t Toggled = 0;
    /// <summary>The radio group (the team, 1 or 2, for the tech base buttons).</summary>
    int32_t Group = 0;
    /// <summary>The value this button stands for (1 Inner Sphere, -1 Clan for the tech base buttons).</summary>
    int32_t Value = 0;
};

/// <summary>An auto-repeating button: runs its callback on the click, then every 100 ms after half a second held.</summary>
/// <remarks>Original source: <c>logistics\logsession.cpp</c>, 0x4f4 bytes.</remarks>
class MCLogSpinnerButton : public MCLogToolButton
{
public:
    ~MCLogSpinnerButton() override = default;

    /// <summary>Press/release and the repeat timers (1: start delay, 2: repeat).</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Shows the down picture while held, else the up one (nothing without that picture). Port: drawn each frame
    /// from the state; outside the frame pass a call is the original's paint.
    /// </summary>
    void Draw() override;
};

/// <summary>
/// The chat input line: typed text with a blinking cursor; Enter sends it to everyone, or to the team when the
/// team button is toggled, and echoes it in the parent <see cref="MCLogChatWindow"/>.
/// </summary>
/// <remarks>Original source: <c>logistics\logsession.cpp</c>, 0x5d4 bytes.</remarks>
class MCLogChatInput : public MCLogObject
{
public:
    ~MCLogChatInput() override { Destroy(); }

    /// <summary>Places the line, makes the team button and takes <paramref name="text"/> as the starting text.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* text) override;

    void Destroy() override;

    /// <summary>
    /// Clears the box and writes the text, wrapped; it also turned the cursor off. Port: draws the box, the text and
    /// the cursor (which the original's display drew into the picture each frame).
    /// </summary>
    void Draw() override;

    /// <summary>Port: the line draws itself each frame (its port is a view).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>
    /// Port: an edit turns the cursor off, starting its blink over (the original's paint after an edit did).
    /// </summary>
    void RestartBlink();

    /// <summary>Draws the cursor (lit or not) and displays the line. Port: the cursor is drawn by <see cref="Draw"/>.</summary>
    void Display() override;

    /// <summary>Takes the text focus on a click; typing, Backspace, Enter; the blink timer.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Works out the cursor's pixel position for character <paramref name="position"/>.</summary>
    void SetCursorPos(int32_t position);

    /// <summary>Toggled: send to the team only.</summary>
    MCLogToolButton* TeamButton = nullptr;
    /// <summary>The text typed.</summary>
    char Text[256] = {};
    /// <summary>The length of <see cref="Text"/>.</summary>
    int32_t TextLength = 0;
    /// <summary>The cursor's x in the box.</summary>
    int32_t CursorX = 0;
    /// <summary>The cursor's y in the box.</summary>
    int32_t CursorY = 0;
    /// <summary>The cursor's blink phase.</summary>
    int32_t CursorOn = 0;
    MCGuiFont* Font = nullptr;
};

/// <summary>
/// A player's name on the multiplayer session screen, which the host can drag between the unassigned list and
/// the two teams.
/// </summary>
/// <remarks>Original source: <c>logistics\logsession.cpp</c>, 0x4d0 bytes.</remarks>
class MCPlayerNameObject : public MCLogObject
{
public:
    ~MCPlayerNameObject() override { Destroy(); }

    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;

    void Destroy() override;

    /// <summary>
    /// Fills the box and writes the name (not while it is being dragged). Port: drawn each frame, over the player
    /// number the session screen put on the left (<see cref="NumberArt"/>).
    /// </summary>
    void Draw() override;

    /// <summary>Port: the name draws itself each frame (its port is a view).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Dragging: grabbed on a press (when <see cref="Draggable"/>), follows the mouse, dropped on release.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Copies <paramref name="name"/> (to a logistics block) and redraws.</summary>
    void SetPlayerName(char* name);

    /// <summary>Sets the player and takes the name from the session.</summary>
    void SetPlayerId(uint32_t playerId);

    void SetFont(MCGuiFont* newFont);

    /// <summary>
    /// Whether the player has the loaded mission's file: -1 no inquiry out, 0 waiting for the answer, 1 missing, 2 has
    /// it (<c>SessionScreen::loadMission</c>, <c>fileReport</c>).
    /// </summary>
    int8_t FileStatus = -1;
    /// <summary>The player's name (a logistics block).</summary>
    char* PlayerName = nullptr;
    MCGuiFont* Font = nullptr;
    /// <summary>The player's network id; 0xffffffff = none.</summary>
    uint32_t PlayerId = 0xffffffff;
    /// <summary>Nonzero when the name can be dragged (the host's screen).</summary>
    int32_t Draggable = 0;

    /// <summary>
    /// Port: what <c>SessionScreen::init</c> painted into the name's picture: a wipe to <see cref="NumberBack"/> and
    /// the player number picture (<c>ses_p&lt;n&gt;</c>) at (1, 1); null before.
    /// </summary>
    MCLogPort* NumberArt = nullptr;
    int32_t NumberBack = 0xff;
};

/// <summary>
/// The multiplayer session (ready room) screen: the players and the two teams, each team's resource points and
/// tech base, the mission chosen by the host, and the load/start buttons.
/// </summary>
/// <remarks>Original source: <c>logistics\logsession.cpp</c>, 0x554 bytes (<c>Logistics</c> +0x4b8).</remarks>
class MCSessionScreen : public MCLogObject
{
public:
    ~MCSessionScreen() override { Destroy(); }

    /// <summary>Makes the buttons, the RP spinners and texts, the tech base toggles and the six name slots.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;

    void Destroy() override;

    /// <summary>
    /// Draws the background, the unassigned list, the mission and map names, the file name and each team's resource
    /// points per player. Port: draws them from the state, then the shared places (<see cref="MCLogScreenChrome"/>:
    /// the ticker, the clock, the lights' backing).
    /// </summary>
    void Draw() override;

    /// <summary>Port: the screen draws itself each frame (its port is a view).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Port: see <see cref="MCLogScreenChrome"/>.</summary>
    MCLogScreenChrome* Chrome() override { return &ScreenChrome; }

    /// <summary>Port: what the screen shows of the shared places.</summary>
    MCLogScreenChrome ScreenChrome;

    /// <summary>Name drops, chat keys and the RP text edits.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Fills the screen from the session: the players, the teams and the controls for host or guest;
    /// <paramref name="refresh"/> keeps the mission already loaded.
    /// </summary>
    void Activate(int refresh);

    /// <summary>
    /// Moves <paramref name="playerId"/> to team <paramref name="team"/> (0 = unassigned) at <paramref name="slot"/>;
    /// unless <paramref name="remote"/>, tells the other players.
    /// </summary>
    void AssignPlayer(uint32_t playerId, char team, char slot, int remote);

    /// <summary>Shows the map picture for mission file <paramref name="fileName"/>.</summary>
    void SetMap(char* fileName);

    void SetMissionName(char* name);

    void SetMapName(char* name);

    /// <summary>A player has the mission: when all have, the start button lights.</summary>
    void SomeoneCheckedIn();

    /// <summary>Player <paramref name="playerId"/> reports whether it has the mission file (<paramref name="haveFile"/>).</summary>
    void FileReport(uint32_t playerId, int haveFile);

    /// <summary>Loads mission <paramref name="fileName"/> chosen by the host and tells the others.</summary>
    void LoadMission(char* fileName);

    /// <summary>Forgets the loaded mission.</summary>
    void CancelMission();

    /// <summary>
    /// Lists the player ids of one team into <paramref name="ids"/> (count in <paramref name="count"/>):
    /// <paramref name="myTeam"/> picks the local player's team, else the other.
    /// </summary>
    void FillDpidArray(uint32_t* ids, int32_t* count, int myTeam);

    /// <summary>Lights the start button when every player is on a team and a mission is loaded.</summary>
    void CheckGoodToGo();

    /// <summary>Takes <paramref name="playerId"/> off the screen.</summary>
    void RemovePlayer(uint32_t playerId);

    /// <summary>Sets team <paramref name="team"/>'s tech base (1 Inner Sphere, -1 Clan) and its toggles.</summary>
    void SetTeamTechBase(char team, char techBase);

    /// <summary>Enables the host's controls.</summary>
    void ControlsOn();

    /// <summary>Disables the host's controls (a guest's screen).</summary>
    void ControlsOff();

    /// <summary>Greys the RP spinners while <paramref name="lock"/>.</summary>
    void LockControls(int lock);

    /// <summary>Sets team 1's resource points text.</summary>
    void SetTeam1RP(int32_t resourcePoints);

    /// <summary>Sets team 2's resource points text.</summary>
    void SetTeam2RP(int32_t resourcePoints);

    /// <summary>The tab back to the session list.</summary>
    MCLogToolButton* SessionButton = nullptr;
    MCLogToolButton* ExitButton = nullptr;
    /// <summary>Opens the load dialog (host).</summary>
    MCLogButton* LoadMissionButton = nullptr;
    /// <summary>Starts the mission (host, when <see cref="CheckGoodToGo"/> allows).</summary>
    MCLogButton* StartButton = nullptr;
    /// <summary>Team 1's resource points (a number in text).</summary>
    MCLogTextObject* Team1RPText = nullptr;
    /// <summary>Team 2's resource points (a number in text).</summary>
    MCLogTextObject* Team2RPText = nullptr;
    /// <summary>The mission description.</summary>
    MCLogScrollTextObject* MissionText = nullptr;
    MCLogSpinnerButton* Team1RPUp = nullptr;
    MCLogSpinnerButton* Team1RPDown = nullptr;
    MCLogSpinnerButton* Team2RPUp = nullptr;
    MCLogSpinnerButton* Team2RPDown = nullptr;
    /// <summary>A short name of the loaded mission, drawn beside the mission name (cleared as 4 + 4 + 2 bytes).</summary>
    char MissionLabel[10] = {};
    /// <summary>Team 1's Inner Sphere tech base toggle.</summary>
    MCLogToolButton* Team1ISButton = nullptr;
    /// <summary>Team 1's Clan tech base toggle.</summary>
    MCLogToolButton* Team1ClanButton = nullptr;
    /// <summary>Team 2's Inner Sphere tech base toggle.</summary>
    MCLogToolButton* Team2ISButton = nullptr;
    /// <summary>Team 2's Clan tech base toggle.</summary>
    MCLogToolButton* Team2ClanButton = nullptr;
    /// <summary>Team 1's tech base: 1 Inner Sphere, -1 Clan.</summary>
    int8_t Team1TechBase = 1;
    /// <summary>Team 2's tech base.</summary>
    int8_t Team2TechBase = -1;
    /// <summary>The name slots, one per player (top to bottom in the unassigned list).</summary>
    MCPlayerNameObject* PlayerNames[6] = {};
    /// <summary>The number of players in the session.</summary>
    int32_t NumPlayers = 0;
    /// <summary>The number of players on no team.</summary>
    int32_t NumUnassigned = 0;
    /// <summary>Team 1's players by slot; 0xffffffff = empty.</summary>
    uint32_t Team1Players[3] = {};
    /// <summary>Team 2's players by slot; 0xffffffff = empty.</summary>
    uint32_t Team2Players[3] = {};
    /// <summary>Team 1's resource points last sent.</summary>
    int32_t Team1RP = 0;
    /// <summary>Team 2's resource points last sent.</summary>
    int32_t Team2RP = 0;
    /// <summary>The loaded mission's name (a logistics block).</summary>
    char* MissionName = nullptr;
    /// <summary>The loaded mission's file (a logistics block); null = none.</summary>
    char* MissionFile = nullptr;
    /// <summary>The loaded mission's map name (a logistics block).</summary>
    char* MapName = nullptr;

    /// <summary>
    /// Port: the loaded mission's map picture (owned), drawn stretched over the map box, or null. (The original
    /// stretched each map into the background picture over the ones before.)
    /// </summary>
    MCLogPort* MapPicture = nullptr;
    /// <summary>Port: the mission was taken away: the map box shows colour 0x10 rather than the background's art.</summary>
    bool MapBoxWiped = false;

    /// <summary>Port: draws the map box (<see cref="MapPicture"/>) into <paramref name="target"/>.</summary>
    void DrawMap(MCPane* target);

    /// <summary>Port: frees <see cref="MapPicture"/>.</summary>
    void ClearMap();
};

/// <summary>The default <see cref="MCLogToolButton"/> event routine: a click flips the toggle and runs the callback.</summary>
void LToolButtonEventHandler(MCGuiObject* object, MCGuiEvent* event);

/// <summary>The chat team button: toggles, then passes the event to the chat input.</summary>
void ChatTeamButtonEventHandler(MCGuiObject* object, MCGuiEvent* event);

/// <summary>A screen tab: a click on an untoggled tab toggles it and runs the callback.</summary>
void LScreenSwitchEventHandler(MCGuiObject* object, MCGuiEvent* event);

/// <summary>The dialog's Cancel: drops the loaded mission.</summary>
void MPCancelCallback();

/// <summary>After a multiplayer save: returns to the session screen and tells the others which file to load.</summary>
void MPLoadWorkedCallback(int32_t result);

/// <summary>The load-mission button: opens the load screen for a multiplayer mission.</summary>
void LoadMissionCallback();

/// <summary>The start button: tells every player to start the loaded mission.</summary>
void StartMissionCallback();

/// <summary>A tech base toggle: sets its team's tech base and tells the others.</summary>
void TechTabRoutine(MCGuiObject* object, MCGuiEvent* event);

/// <summary>Team 1's resource points up 1000.</summary>
void IncrementTeam1RP();

/// <summary>Team 2's resource points up 1000.</summary>
void IncrementTeam2RP();

/// <summary>Team 1's resource points down 1000 (not below 0).</summary>
void DecrementTeam1RP();

/// <summary>Team 2's resource points down 1000 (not below 0).</summary>
void DecrementTeam2RP();

/// <summary>The session screen's paint routine: the unassigned list's background and a bar per unassigned player.</summary>
void SessionScreenDrawRoutine(MCGuiObject* object);

/// <summary>A <c>qsort</c> comparison of two unsigned 32-bit values (ascending).</summary>
int CompareLong(const void* first, const void* second);
