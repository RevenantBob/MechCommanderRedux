#pragma once

#include "logistics/loggen.h"
#include "logistics/lport.h"

class aEvent;
class aFont;
class LogChatWindow;

/// <summary>
/// A logistics toggle button: a click flips <see cref="toggled"/> (through <see cref="lToolButtonEventHandler"/>),
/// and it shows its down picture while toggled. Session screen tabs and radio groups use <see cref="group"/> and
/// <see cref="value"/>.
/// </summary>
/// <remarks>
/// Original source: <c>logistics\logsession.cpp</c>, 0x4f4 bytes (vtable 0x00781d14, missing from the vtable
/// dump).
/// </remarks>
class lToolButton : public lButton
{
public:
    /// <remarks>MCX.EXE @ 0x006e6060 (vector deleting destructor)</remarks>
    ~lToolButton() override = default;

    /// <summary>Initialises the button and installs <see cref="lToolButtonEventHandler"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0070b7d0</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;

    /// <summary>Plays the click sound (or the disabled one) and passes the event on.</summary>
    /// <remarks>MCX.EXE @ 0x0070b820</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>
    /// The original's draw: shows the gray, down (toggled), over (mouse over) or up picture (the face it chooses,
    /// see <see cref="lButton::updateFace"/>).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0070b880</remarks>
    void updateFace() override;

    /// <summary>Nonzero while toggled on.</summary>
    int32_t toggled = 0; // +0x4e4
    /// <summary>The radio group (the team, 1 or 2, for the tech base buttons).</summary>
    int32_t group = 0; // +0x4e8
    /// <summary>The value this button stands for (1 Inner Sphere, -1 Clan for the tech base buttons).</summary>
    int32_t value = 0; // +0x4ec
    /// <summary>Not seen used.</summary>
    int32_t unknown4F0 = 0; // +0x4f0
};

/// <summary>An auto-repeating button: runs its callback on the click, then every 100 ms after half a second held.</summary>
/// <remarks>Original source: <c>logistics\logsession.cpp</c>, 0x4f4 bytes.</remarks>
class lSpinnerButton : public lToolButton
{
public:
    /// <remarks>MCX.EXE @ 0x0070d930 (vector deleting destructor)</remarks>
    ~lSpinnerButton() override = default;

    /// <summary>Press/release and the repeat timers (1: start delay, 2: repeat).</summary>
    /// <remarks>MCX.EXE @ 0x0070ba00</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>
    /// The original's draw: shows the down picture while held, else the up one; without that picture the face stays
    /// as it was.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0070bb40</remarks>
    void updateFace() override;
};

/// <summary>
/// The chat input line: typed text with a blinking cursor; Enter sends it to everyone, or to the team when the
/// team button is toggled, and echoes it in the parent <see cref="LogChatWindow"/>.
/// </summary>
/// <remarks>Original source: <c>logistics\logsession.cpp</c>, 0x5d4 bytes.</remarks>
class lChatInput : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x0070b170 (vector deleting destructor)</remarks>
    ~lChatInput() override { destroy(); }

    /// <summary>Places the line, makes the team button and takes <paramref name="text"/> as the starting text.</summary>
    /// <remarks>MCX.EXE @ 0x0070bbb0</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* text) override;

    /// <remarks>MCX.EXE @ 0x0070bd20</remarks>
    void destroy() override;

    /// <summary>
    /// Clears the box and writes the text, wrapped; it also turned the cursor off. Port: in the frame pass it draws
    /// the box, the text and the cursor (which the original's display drew into the picture each frame); at any other
    /// time it is <see cref="Refresh"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0070bd50</remarks>
    void draw() override;

    /// <summary>Port: the line draws itself each frame (its port is a view).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Port: what the original's draw did besides painting: the cursor goes off.</summary>
    void Refresh() override;

    /// <summary>Draws the cursor (lit or not) and displays the line. Port: the cursor is drawn by <see cref="draw"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0070be80</remarks>
    void display() override;

    /// <summary>Takes the text focus on a click; typing, Backspace, Enter; the blink timer.</summary>
    /// <remarks>MCX.EXE @ 0x0070bee0</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Works out the cursor's pixel position for character <paramref name="position"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0070c160</remarks>
    void setCursorPos(int32_t position);

    /// <summary>Toggled: send to the team only.</summary>
    lToolButton* teamButton = nullptr; // +0x4bc
    /// <summary>The text typed.</summary>
    char text[256] = {}; // +0x4c0
    /// <summary>The length of <see cref="text"/>.</summary>
    int32_t textLength = 0; // +0x5c0
    /// <summary>The cursor's x in the box.</summary>
    int32_t cursorX = 0; // +0x5c4
    /// <summary>The cursor's y in the box.</summary>
    int32_t cursorY = 0; // +0x5c8
    /// <summary>The cursor's blink phase.</summary>
    int32_t cursorOn = 0;  // +0x5cc
    aFont* font = nullptr; // +0x5d0
};

/// <summary>
/// A player's name on the multiplayer session screen, which the host can drag between the unassigned list and
/// the two teams.
/// </summary>
/// <remarks>Original source: <c>logistics\logsession.cpp</c>, 0x4d0 bytes.</remarks>
class PlayerNameObject : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x0070d8f0 (vector deleting destructor)</remarks>
    ~PlayerNameObject() override { destroy(); }

    /// <remarks>MCX.EXE @ 0x0070c270</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;

    /// <remarks>MCX.EXE @ 0x0070c2e0</remarks>
    void destroy() override;

    /// <summary>
    /// Fills the box and writes the name (not while it is being dragged). Port: drawn each frame, over the player
    /// number the session screen put on the left (<see cref="numberArt"/>).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0070c320</remarks>
    void draw() override;

    /// <summary>Port: the name draws itself each frame (its port is a view).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Dragging: grabbed on a press (when <see cref="draggable"/>), follows the mouse, dropped on release.</summary>
    /// <remarks>MCX.EXE @ 0x0070c390</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Copies <paramref name="name"/> (to the logistics heap) and redraws.</summary>
    /// <remarks>MCX.EXE @ 0x0070c4f0</remarks>
    void setPlayerName(char* name);

    /// <summary>Sets the player and takes the name from the session.</summary>
    /// <remarks>MCX.EXE @ 0x0070c570</remarks>
    void setPlayerId(uint32_t playerId);

    /// <remarks>MCX.EXE @ 0x0070c5c0 (unnamed in the symbols: the name is inferred).</remarks>
    void setFont(aFont* newFont);

    /// <summary>
    /// Whether the player has the loaded mission's file: -1 no inquiry out, 0 waiting for the answer, 1 missing, 2 has
    /// it (<c>SessionScreen::loadMission</c>, <c>fileReport</c>).
    /// </summary>
    int8_t fileStatus = -1; // +0x4bc
    /// <summary>The player's name (logistics heap).</summary>
    char* playerName = nullptr; // +0x4c0
    aFont* font = nullptr;      // +0x4c4
    /// <summary>The player's network id; 0xffffffff = none.</summary>
    uint32_t playerId = 0xffffffff; // +0x4c8
    /// <summary>Nonzero when the name can be dragged (the host's screen).</summary>
    int32_t draggable = 0; // +0x4cc

    /// <summary>
    /// Port: what <c>SessionScreen::init</c> painted into the name's picture: a wipe to <see cref="numberBack"/> and
    /// the player number picture (<c>ses_p&lt;n&gt;</c>) at (1, 1); null before.
    /// </summary>
    lPort* numberArt = nullptr;
    int32_t numberBack = 0xff;
};

/// <summary>
/// The multiplayer session (ready room) screen: the players and the two teams, each team's resource points and
/// tech base, the mission chosen by the host, and the load/start buttons.
/// </summary>
/// <remarks>Original source: <c>logistics\logsession.cpp</c>, 0x554 bytes (<c>Logistics</c> +0x4b8).</remarks>
class SessionScreen : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x006f04f0 (vector deleting destructor)</remarks>
    ~SessionScreen() override { destroy(); }

    /// <summary>Makes the buttons, the RP spinners and texts, the tech base toggles and the six name slots.</summary>
    /// <remarks>MCX.EXE @ 0x0070cb30</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;

    /// <remarks>MCX.EXE @ 0x0070d970</remarks>
    void destroy() override;

    /// <summary>
    /// Draws the background, the unassigned list, the mission and map names, the file name and each team's resource
    /// points per player. Port: in the frame pass it draws them from the state, then the shared places
    /// (<see cref="LogScreenChrome"/>: the ticker, the clock, the lights' backing); at any other time it only does
    /// what the original's paint did to them (painted over, so forgotten) and refreshes the children.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0070dbe0</remarks>
    void draw() override;

    /// <summary>Port: the screen draws itself each frame (its port is a view).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Port: see <see cref="LogScreenChrome"/>.</summary>
    LogScreenChrome* Chrome() override { return &chrome; }

    /// <summary>Port: what the screen shows of the shared places.</summary>
    LogScreenChrome chrome;

    /// <summary>Name drops, chat keys and the RP text edits.</summary>
    /// <remarks>MCX.EXE @ 0x0070ddd0</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>
    /// Fills the screen from the session: the players, the teams and the controls for host or guest;
    /// <paramref name="refresh"/> keeps the mission already loaded.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0070e120</remarks>
    void activate(int refresh);

    /// <summary>
    /// Moves <paramref name="playerId"/> to team <paramref name="team"/> (0 = unassigned) at <paramref name="slot"/>;
    /// unless <paramref name="remote"/>, tells the other players.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0070e6b0</remarks>
    void assignPlayer(uint32_t playerId, char team, char slot, int remote);

    /// <summary>Shows the map picture for mission file <paramref name="fileName"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0070ecc0</remarks>
    void setMap(char* fileName);

    /// <remarks>MCX.EXE @ 0x0070ee90</remarks>
    void setMissionName(char* name);

    /// <remarks>MCX.EXE @ 0x0070ef10</remarks>
    void setMapName(char* name);

    /// <summary>A player has the mission: when all have, the start button lights.</summary>
    /// <remarks>MCX.EXE @ 0x0070ef90</remarks>
    void someoneCheckedIn();

    /// <summary>Player <paramref name="playerId"/> reports whether it has the mission file (<paramref name="haveFile"/>).</summary>
    /// <remarks>MCX.EXE @ 0x0070f040</remarks>
    void fileReport(uint32_t playerId, int haveFile);

    /// <summary>Loads mission <paramref name="fileName"/> chosen by the host and tells the others.</summary>
    /// <remarks>MCX.EXE @ 0x0070f290</remarks>
    void loadMission(char* fileName);

    /// <summary>Forgets the loaded mission.</summary>
    /// <remarks>MCX.EXE @ 0x0070fa30</remarks>
    void cancelMission();

    /// <summary>
    /// Lists the player ids of one team into <paramref name="ids"/> (count in <paramref name="count"/>):
    /// <paramref name="myTeam"/> picks the local player's team, else the other.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0070fac0</remarks>
    void fillDPIDArray(uint32_t* ids, int32_t* count, int myTeam);

    /// <summary>Lights the start button when every player is on a team and a mission is loaded.</summary>
    /// <remarks>MCX.EXE @ 0x0070fb70</remarks>
    void checkGoodToGo();

    /// <summary>Takes <paramref name="playerId"/> off the screen.</summary>
    /// <remarks>MCX.EXE @ 0x0070fc10</remarks>
    void removePlayer(uint32_t playerId);

    /// <summary>Sets team <paramref name="team"/>'s tech base (1 Inner Sphere, -1 Clan) and its toggles.</summary>
    /// <remarks>MCX.EXE @ 0x0070fec0</remarks>
    void setTeamTechBase(char team, char techBase);

    /// <summary>Enables the host's controls.</summary>
    /// <remarks>MCX.EXE @ 0x00710030</remarks>
    void controlsOn();

    /// <summary>Disables the host's controls (a guest's screen).</summary>
    /// <remarks>MCX.EXE @ 0x00710180</remarks>
    void controlsOff();

    /// <summary>Greys the RP spinners while <paramref name="lock"/>.</summary>
    /// <remarks>MCX.EXE @ 0x007102c0</remarks>
    void lockControls(int lock);

    /// <summary>Sets team 1's resource points text.</summary>
    /// <remarks>MCX.EXE @ 0x0070ec40 (unnamed in the symbols: the name is inferred).</remarks>
    void setTeam1RP(int32_t resourcePoints);

    /// <summary>Sets team 2's resource points text.</summary>
    /// <remarks>MCX.EXE @ 0x0070ec80 (unnamed in the symbols: the name is inferred).</remarks>
    void setTeam2RP(int32_t resourcePoints);

    /// <summary>The tab back to the session list.</summary>
    lToolButton* sessionButton = nullptr; // +0x4bc
    lToolButton* exitButton = nullptr;    // +0x4c0
    /// <summary>Opens the load dialog (host).</summary>
    lButton* loadMissionButton = nullptr; // +0x4c4
    /// <summary>Starts the mission (host, when <see cref="checkGoodToGo"/> allows).</summary>
    lButton* startButton = nullptr; // +0x4c8
    /// <summary>Team 1's resource points (a number in text).</summary>
    lTextObject* team1RPText = nullptr; // +0x4cc
    /// <summary>Team 2's resource points (a number in text).</summary>
    lTextObject* team2RPText = nullptr; // +0x4d0
    /// <summary>The mission description.</summary>
    lScrollTextObject* missionText = nullptr; // +0x4d4
    lSpinnerButton* team1RPUp = nullptr;      // +0x4d8
    lSpinnerButton* team1RPDown = nullptr;    // +0x4dc
    lSpinnerButton* team2RPUp = nullptr;      // +0x4e0
    lSpinnerButton* team2RPDown = nullptr;    // +0x4e4
    /// <summary>A short name of the loaded mission, drawn beside the mission name (cleared as 4 + 4 + 2 bytes).</summary>
    char missionLabel[10] = {}; // +0x4e8
    /// <summary>Team 1's Inner Sphere tech base toggle.</summary>
    lToolButton* team1ISButton = nullptr; // +0x4f4
    /// <summary>Team 1's Clan tech base toggle.</summary>
    lToolButton* team1ClanButton = nullptr; // +0x4f8
    /// <summary>Team 2's Inner Sphere tech base toggle.</summary>
    lToolButton* team2ISButton = nullptr; // +0x4fc
    /// <summary>Team 2's Clan tech base toggle.</summary>
    lToolButton* team2ClanButton = nullptr; // +0x500
    /// <summary>Team 1's tech base: 1 Inner Sphere, -1 Clan.</summary>
    int8_t team1TechBase = 1; // +0x504
    /// <summary>Team 2's tech base.</summary>
    int8_t team2TechBase = -1; // +0x505
    /// <summary>The name slots, one per player (top to bottom in the unassigned list).</summary>
    PlayerNameObject* playerNames[6] = {}; // +0x508
    /// <summary>The number of players in the session.</summary>
    int32_t numPlayers = 0; // +0x520
    /// <summary>The number of players on no team.</summary>
    int32_t numUnassigned = 0; // +0x524
    /// <summary>Team 1's players by slot; 0xffffffff = empty.</summary>
    uint32_t team1Players[3] = {}; // +0x528
    /// <summary>Team 2's players by slot; 0xffffffff = empty.</summary>
    uint32_t team2Players[3] = {}; // +0x534
    /// <summary>Team 1's resource points last sent.</summary>
    int32_t team1RP = 0; // +0x540
    /// <summary>Team 2's resource points last sent.</summary>
    int32_t team2RP = 0; // +0x544
    /// <summary>The loaded mission's name (logistics heap).</summary>
    char* missionName = nullptr; // +0x548
    /// <summary>The loaded mission's file (logistics heap); null = none.</summary>
    char* missionFile = nullptr; // +0x54c
    /// <summary>The loaded mission's map name (logistics heap).</summary>
    char* mapName = nullptr; // +0x550

    /// <summary>
    /// Port: what <see cref="setMap"/> drew into the background picture's map box, in order since the box was last
    /// cleared: each map's picture (owned), stretched over the box. The screen draws them over its background.
    /// </summary>
    std::vector<lPort*> mapLayers;
    /// <summary>Port: whether the map box was wiped to 0x10 (under <see cref="mapLayers"/>).</summary>
    bool mapBoxWiped = false;

    /// <summary>Port: draws <see cref="mapLayers"/> into <paramref name="target"/>.</summary>
    void DrawMapLayers(_pane* target);

    /// <summary>Port: frees <see cref="mapLayers"/> (the box was cleared).</summary>
    void ClearMapLayers();
};

/// <summary>The default <see cref="lToolButton"/> event routine: a click flips the toggle and runs the callback.</summary>
/// <remarks>MCX.EXE @ 0x0070b6e0</remarks>
void lToolButtonEventHandler(aObject* object, aEvent* event);

/// <summary>The chat team button: toggles, then passes the event to the chat input.</summary>
/// <remarks>MCX.EXE @ 0x0070b750</remarks>
void ChatTeamButtonEventHandler(aObject* object, aEvent* event);

/// <summary>A screen tab: a click on an untoggled tab toggles it and runs the callback.</summary>
/// <remarks>MCX.EXE @ 0x0070b780</remarks>
void lScreenSwitchEventHandler(aObject* object, aEvent* event);

/// <summary>The dialog's Cancel: drops the loaded mission.</summary>
/// <remarks>MCX.EXE @ 0x0070c5d0</remarks>
void MPCancelCallback();

/// <summary>After a multiplayer save: returns to the session screen and tells the others which file to load.</summary>
/// <remarks>MCX.EXE @ 0x0070c5f0</remarks>
void MPLoadWorkedCallback(int32_t result);

/// <summary>The load-mission button: opens the load screen for a multiplayer mission.</summary>
/// <remarks>MCX.EXE @ 0x0070c730</remarks>
void LoadMissionCallback();

/// <summary>The start button: tells every player to start the loaded mission.</summary>
/// <remarks>MCX.EXE @ 0x0070c7d0</remarks>
void StartMissionCallback();

/// <summary>A tech base toggle: sets its team's tech base and tells the others.</summary>
/// <remarks>MCX.EXE @ 0x0070c900</remarks>
void techTabRoutine(aObject* object, aEvent* event);

/// <summary>Team 1's resource points up 1000.</summary>
/// <remarks>MCX.EXE @ 0x0070c980</remarks>
void incrementTeam1RP();

/// <summary>Team 2's resource points up 1000.</summary>
/// <remarks>MCX.EXE @ 0x0070c9d0</remarks>
void incrementTeam2RP();

/// <summary>Team 1's resource points down 1000 (not below 0).</summary>
/// <remarks>MCX.EXE @ 0x0070ca20</remarks>
void decrementTeam1RP();

/// <summary>Team 2's resource points down 1000 (not below 0).</summary>
/// <remarks>MCX.EXE @ 0x0070ca70</remarks>
void decrementTeam2RP();

/// <summary>The session screen's paint routine: the unassigned list's background and a bar per unassigned player.</summary>
/// <remarks>MCX.EXE @ 0x0070cac0</remarks>
void SessionScreenDrawRoutine(aObject* object);

/// <summary>A <c>qsort</c> comparison of two unsigned 32-bit values (ascending).</summary>
/// <remarks>MCX.EXE @ 0x0070e100</remarks>
int CompareLong(const void* first, const void* second);
