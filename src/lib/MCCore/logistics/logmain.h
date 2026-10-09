#pragma once

// Original source: logistics\logmain.cpp, the callbacks behind the logistics main menu and its sub-screens (new
// campaign, load/save, preferences, multiplayer connection: LAN, TCP/IP, IPX, modem, serial, ready room), plus the
// game-wide paths and settings the menu reads and writes. All free functions: button callbacks, dialog callbacks
// and aObject event routines.

class MCGuiEvent;
class MCGuiObject;
class MCGenericScreen;
class MCLogistics;

// Campaign CD checks: each takes the dialog result and, once the right CD is found, carries on with its action.

void NewCampaignCDTester(int32_t result);
void McxCampaignCDTester(int32_t result);
void MpxCampaignCDTester(int32_t result);
void SaveCampaignCDTester(int32_t result);
void LoadCampaignCDTester(int32_t result);
void SoloCampaignCDTester(int32_t result);
void PrefCampaignCDTester(int32_t result);
void CineCampaignCDTester(int32_t result);

/// <summary>Whether the version stored in the settings matches this build.</summary>
bool CheckRegistryVersionNumber();

/// <summary>Stores this build's version in the settings.</summary>
void WriteRegistryVersionNumber();

/// <summary>Starts a new original campaign.</summary>
void NewCampaign();

/// <summary>Starts a new Mech Commander Gold (MCX) campaign.</summary>
void NewMcxCampaign();

/// <summary>Opens the save game screen.</summary>
void SaveScreen();

/// <summary>Opens the multiplayer connection screen.</summary>
void ConnectScreen();

/// <summary>Opens the load game screen.</summary>
void LoadScreen();

/// <summary>Opens the load screen for single missions.</summary>
void SoloLoadScreen();

/// <summary>Opens the preferences screen.</summary>
void ShowPreferences();

/// <summary>Leaves the preferences, restoring the old settings.</summary>
void CancelPrefs();

/// <summary>Leaves the preferences, saving the settings.</summary>
void WritePrefs();

/// <summary>
/// Port-only: turns the preferences screen <paramref name="screen"/>'s DIFFICULTY checks into a drop-down (EASY,
/// REGULAR, HARD; <c>GameDifficulty</c>) and adds a RENDERER box under it with a drop-down of its own (VULKAN,
/// SOFTWARE; PREFS "Renderer"). The renderer takes effect at the next start, which the message dialog says when the
/// drop-down picks another renderer than the running one.
/// </summary>
void AddPreferenceDropDowns(MCGenericScreen* screen);

/// <summary>Opens the multiplayer menu.</summary>
void ShowMultiPlayer();

void ReplayCDTester(int32_t result);

/// <summary>Replays the campaign's cinematics.</summary>
void ReplayCinema();

/// <summary>Returns from the menu to the logistics screens.</summary>
void ReturnToGame();

/// <summary>Ends the campaign (the player lost).</summary>
void GameOverMan();

/// <summary>Loads the save game chosen on the load screen.</summary>
void LoadGame();

/// <summary>Loads the multiplayer mission chosen on the load screen.</summary>
void LoadMPGame();

/// <summary>The dialog after a save.</summary>
void SaveWorkedCallback(int32_t result);

/// <summary>Saves the game under the name typed.</summary>
void SaveGameCallback();

/// <summary>After confirming an overwrite: saves.</summary>
void ClearForSaveGameCallback();

/// <summary>The save button: asks before overwriting, then saves.</summary>
void SaveGame();

/// <summary>Confirmed: deletes the chosen save game.</summary>
void DeleteCallbackTrue();

/// <summary>Not confirmed: keeps the save game.</summary>
void DeleteCallbackFalse();

/// <summary>The delete button: asks for confirmation.</summary>
void DeleteGame();

/// <summary>Back to the main menu.</summary>
void Cancel();

/// <summary>Back to the connection screen (closing the session).</summary>
void CancelToConnect();

/// <summary>Back to the multiplayer menu.</summary>
void CancelToMPlayer();

/// <summary>Back to the LAN (session list) screen.</summary>
void CancelToLan();

/// <summary>Back to the session (ready room) screen.</summary>
void CancelToSession();

void ShowModemScreen();

void ShowSerialScreen();

/// <summary>Connects over IPX and shows the session list.</summary>
void DoTheIpxThang();

/// <summary>Connects over TCP/IP and shows the session list.</summary>
void DoTheTcpThang();

/// <summary>The TCP/IP-or-IPX question's answer.</summary>
void TcpipxDialogCallback(int32_t result);

void CallDoTheIpxThang(int32_t result);

void CallDoTheTcpThang(int32_t result);

/// <summary>Shows the LAN session list.</summary>
void ShowLanScreen();

/// <summary>Leaves the game for the Zone's internet lobby (first variant; finds the launcher in the registry).</summary>
void DoExitToZone1();

/// <summary>Leaves the game for the Zone's internet lobby.</summary>
void DoExitToZone();

/// <summary>Starts <c>mplaynow.exe</c> (the Mplayer lobby) and quits, or says it failed.</summary>
void DoExitToMplayer();

/// <summary>Asks whether to leave for the internet lobby (<see cref="DoExitToMplayer"/>).</summary>
void ShowInternet();

/// <summary>Hosts a new session.</summary>
void HostGame();

/// <summary>Clears the ready room for a new session.</summary>
void ResetReadyRoom();

/// <summary>Joins the session chosen in the list.</summary>
void JoinGame();

/// <summary>Creates the hosted session.</summary>
void CreateSession();

void CreateSerialSession();

void SerialJoinButtonPressed();

void JoinSerialSession();

void JoinModemSession();

/// <summary>Dials the number typed.</summary>
/// <returns>Nonzero on a connection.</returns>
int32_t DialModemSession();

/// <summary>Everyone left: back out of the session.</summary>
void AllGoneCallback(int32_t result);

/// <summary>Moves a connected player on to the session (ready room) screen and locks the session.</summary>
void GOCallback();

/// <summary>The ready room's go button: launches into logistics.</summary>
void Go();

void Leave();

/// <summary>Waits for an incoming modem call.</summary>
void WaitForCall();

void GetNumber();

void CancelDial();

void Dial();

// aObject event routines of the menu screens' widgets.

void ImageHandleEvent(MCGuiObject* object, MCGuiEvent* event);
void ModemListHandleEvent(MCGuiObject* object, MCGuiEvent* event);
void PlayerListHandleEvent(MCGuiObject* object, MCGuiEvent* event);
void ReadyRoomPlayerListHandleEvent(MCGuiObject* object, MCGuiEvent* event);
void LanScreenHandleEvent(MCGuiObject* object, MCGuiEvent* event);
void LoadSaveScreenHandleEvent(MCGuiObject* object, MCGuiEvent* event);
void ComPortTextHandleEvent(MCGuiObject* object, MCGuiEvent* event);
void SerialScreenHandleEvent(MCGuiObject* object, MCGuiEvent* event);
void ModemScreenHandleEvent(MCGuiObject* object, MCGuiEvent* event);
void PrefScreenHandleEvent(MCGuiObject* object, MCGuiEvent* event);

/// <summary>The brightness slider.</summary>
void SlideScreenBrightness(MCGuiObject* object, MCGuiEvent* event);
/// <summary>The music volume slider.</summary>
void SlideMusicVolume(MCGuiObject* object, MCGuiEvent* event);
/// <summary>The radio volume slider.</summary>
void SlideRadioVolume(MCGuiObject* object, MCGuiEvent* event);
/// <summary>The sound effects volume slider.</summary>
void SlideFXVolume(MCGuiObject* object, MCGuiEvent* event);

/// <summary>Difficulty: easy.</summary>
void EasyToggle();
/// <summary>Difficulty: regular.</summary>
void RegularToggle();
/// <summary>Difficulty: hard.</summary>
void HardToggle();

/// <summary>Quits the game.</summary>
void DoExit();

/// <summary>The exit button: asks for confirmation.</summary>
void CheckExit();

/// <summary>
/// Remembers <paramref name="name"/> as the multiplayer player name (registry value "Player Name", read back by
/// <c>MyGetUserName</c>). Null is ignored.
/// </summary>
void SaveUserName(char* name);

/// <summary>
/// The ready room's player list refreshes (0x00808644): counted up by its timer, cleared when a session is joined
/// or created; <c>logistics.cpp</c> reads it too.
/// </summary>
extern int32_t ReadyRoomTicks;

/// <summary>The difficulty (0 easy, 1 regular, 2 hard; starts at 1).</summary>
extern int32_t GameDifficulty;

// The data folders, read from the settings at start-up.

/// <summary>Sounds on the CD.</summary>
extern char CDsoundPath[];
/// <summary>The in-mission interface art.</summary>
extern char InterfacePath[80];
/// <summary>The save games.</summary>
extern char SavePath[80];
/// <summary>Sprites on the CD.</summary>
extern char CDspritePath[80];
/// <summary>The terrain (mission) files.</summary>
extern char TerrainPath[80];
/// <summary>The pilot profiles.</summary>
extern char WarriorPath[];
/// <summary>The sprites.</summary>
extern char SpritePath[80];
/// <summary>The object profiles.</summary>
extern char ProfilePath[];
/// <summary>The fonts.</summary>
extern char FontPath[80];
/// <summary>The DirectX redistributable (kept for the settings; unused by the port).</summary>
extern char DirectXPath[80];
/// <summary>The sounds.</summary>
extern char SoundPath[80];
/// <summary>The shapes (hot spot files).</summary>
extern char ShapesPath[];

/// <summary>The application instance (an <c>HINSTANCE</c> in the original; string resources are loaded through it).</summary>
extern void* ThisInstance;

/// <summary>A display option read at start-up (used by the GUI, camera, interface and terrain map).</summary>
extern int Only45Pixel;

/// <summary>The logistics screens, while they exist.</summary>
extern MCLogistics* GlobalLogPtr;

/// <summary>The logistics state last left for a mission (shared with <c>logistics.cpp</c> and <c>mission.cpp</c>).</summary>
extern int32_t LastLogisticsMissionState;

/// <summary>Set while a single mission (not a campaign) is being loaded.</summary>
extern int LoadingSolo;

/// <summary>Set by <see cref="CancelDial"/> to break out of a modem wait.</summary>
extern int WhackTimer;

/// <summary>Forces the 32 MB memory settings.</summary>
extern int Force32MB;

/// <summary>Forces the 16 MB memory settings.</summary>
extern int Force16MB;
