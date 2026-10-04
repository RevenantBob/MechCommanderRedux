#pragma once

// Original source: logistics\logmain.cpp, the callbacks behind the logistics main menu and its sub-screens (new
// campaign, load/save, preferences, multiplayer connection: LAN, TCP/IP, IPX, modem, serial, ready room), plus the
// game-wide paths and settings the menu reads and writes. All free functions: button callbacks, dialog callbacks
// and aObject event routines.

class aEvent;
class aObject;
class GenericScreen;
class Logistics;

// Campaign CD checks: each takes the dialog result and, once the right CD is found, carries on with its action.

/// <remarks>MCX.EXE @ 0x007007c0</remarks>
void NewCampaignCDTester(int32_t result);
/// <remarks>MCX.EXE @ 0x007007d0</remarks>
void MCXCampaignCDTester(int32_t result);
/// <remarks>MCX.EXE @ 0x007007e0</remarks>
void MPXCampaignCDTester(int32_t result);
/// <remarks>MCX.EXE @ 0x007007f0</remarks>
void SaveCampaignCDTester(int32_t result);
/// <remarks>MCX.EXE @ 0x00700800</remarks>
void LoadCampaignCDTester(int32_t result);
/// <remarks>MCX.EXE @ 0x00700810</remarks>
void SoloCampaignCDTester(int32_t result);
/// <remarks>MCX.EXE @ 0x00700820</remarks>
void PrefCampaignCDTester(int32_t result);
/// <remarks>MCX.EXE @ 0x00700830</remarks>
void CineCampaignCDTester(int32_t result);

/// <summary>Whether the version stored in the settings matches this build.</summary>
/// <remarks>MCX.EXE @ 0x00700840</remarks>
bool CheckRegistryVersionNumber();

/// <summary>Stores this build's version in the settings.</summary>
/// <remarks>MCX.EXE @ 0x00700950</remarks>
void WriteRegistryVersionNumber();

/// <summary>Starts a new original campaign.</summary>
/// <remarks>MCX.EXE @ 0x00700aa0</remarks>
void NewCampaign();

/// <summary>Starts a new Mech Commander Gold (MCX) campaign.</summary>
/// <remarks>MCX.EXE @ 0x00700e50</remarks>
void NewMCXCampaign();

/// <summary>Opens the save game screen.</summary>
/// <remarks>MCX.EXE @ 0x00701200</remarks>
void SaveScreen();

/// <summary>Opens the multiplayer connection screen.</summary>
/// <remarks>MCX.EXE @ 0x00701570</remarks>
void ConnectScreen();

/// <summary>Opens the load game screen.</summary>
/// <remarks>MCX.EXE @ 0x00701b10</remarks>
void LoadScreen();

/// <summary>Opens the load screen for single missions.</summary>
/// <remarks>MCX.EXE @ 0x00701e80</remarks>
void SoloLoadScreen();

/// <summary>Opens the preferences screen.</summary>
/// <remarks>MCX.EXE @ 0x007021f0</remarks>
void ShowPreferences();

/// <summary>Leaves the preferences, restoring the old settings.</summary>
/// <remarks>MCX.EXE @ 0x00702670</remarks>
void CancelPrefs();

/// <summary>Leaves the preferences, saving the settings.</summary>
/// <remarks>MCX.EXE @ 0x007026f0</remarks>
void WritePrefs();

/// <summary>
/// Port-only: turns the preferences screen <paramref name="screen"/>'s DIFFICULTY checks into a drop-down (EASY,
/// REGULAR, HARD; <c>GameDifficulty</c>) and adds a RENDERER box under it with a drop-down of its own (VULKAN,
/// SOFTWARE; PREFS "Renderer"). The renderer takes effect at the next start, which the message dialog says when the
/// drop-down picks another renderer than the running one.
/// </summary>
void AddPreferenceDropDowns(GenericScreen* screen);

/// <summary>Opens the multiplayer menu.</summary>
/// <remarks>MCX.EXE @ 0x00702830</remarks>
void ShowMultiPlayer();

/// <remarks>MCX.EXE @ 0x00702880</remarks>
void ReplayCDTester(int32_t result);

/// <summary>Replays the campaign's cinematics.</summary>
/// <remarks>MCX.EXE @ 0x00702890</remarks>
void ReplayCinema();

/// <summary>Returns from the menu to the logistics screens.</summary>
/// <remarks>MCX.EXE @ 0x00702bd0</remarks>
void ReturnToGame();

/// <summary>Ends the campaign (the player lost).</summary>
/// <remarks>MCX.EXE @ 0x00702c20</remarks>
void GameOverMan();

/// <summary>Loads the save game chosen on the load screen.</summary>
/// <remarks>MCX.EXE @ 0x00702c50</remarks>
void LoadGame();

/// <summary>Loads the multiplayer mission chosen on the load screen.</summary>
/// <remarks>MCX.EXE @ 0x00702d20</remarks>
void LoadMPGame();

/// <summary>The dialog after a save.</summary>
/// <remarks>MCX.EXE @ 0x00702d60</remarks>
void SaveWorkedCallback(int32_t result);

/// <summary>Saves the game under the name typed.</summary>
/// <remarks>MCX.EXE @ 0x00702dd0</remarks>
void SaveGameCallback();

/// <summary>After confirming an overwrite: saves.</summary>
/// <remarks>MCX.EXE @ 0x00702f50</remarks>
void ClearForSaveGameCallback();

/// <summary>The save button: asks before overwriting, then saves.</summary>
/// <remarks>MCX.EXE @ 0x00702fc0</remarks>
void SaveGame();

/// <summary>Confirmed: deletes the chosen save game.</summary>
/// <remarks>MCX.EXE @ 0x00703310</remarks>
void DeleteCallbackTrue();

/// <summary>Not confirmed: keeps the save game.</summary>
/// <remarks>MCX.EXE @ 0x00703470</remarks>
void DeleteCallbackFalse();

/// <summary>The delete button: asks for confirmation.</summary>
/// <remarks>MCX.EXE @ 0x007034c0</remarks>
void DeleteGame();

/// <summary>Back to the main menu.</summary>
/// <remarks>MCX.EXE @ 0x00703650</remarks>
void Cancel();

/// <summary>Back to the connection screen (closing the session).</summary>
/// <remarks>MCX.EXE @ 0x00703690</remarks>
void CancelToConnect();

/// <summary>Back to the multiplayer menu.</summary>
/// <remarks>MCX.EXE @ 0x00703800</remarks>
void CancelToMPlayer();

/// <summary>Back to the LAN (session list) screen.</summary>
/// <remarks>MCX.EXE @ 0x00703810</remarks>
void CancelToLAN();

/// <summary>Back to the session (ready room) screen.</summary>
/// <remarks>MCX.EXE @ 0x007038b0</remarks>
void CancelToSession();

/// <remarks>MCX.EXE @ 0x00703950</remarks>
void ShowModemScreen();

/// <remarks>MCX.EXE @ 0x00703a80</remarks>
void ShowSerialScreen();

/// <summary>Connects over IPX and shows the session list.</summary>
/// <remarks>MCX.EXE @ 0x00703b30</remarks>
void DoTheIPXThang();

/// <summary>Connects over TCP/IP and shows the session list.</summary>
/// <remarks>MCX.EXE @ 0x00703bc0</remarks>
void DoTheTCPThang();

/// <summary>The TCP/IP-or-IPX question's answer.</summary>
/// <remarks>MCX.EXE @ 0x00703c50</remarks>
void TCPIPXDialogCallback(int32_t result);

/// <remarks>MCX.EXE @ 0x00703c70</remarks>
void CallDoTheIPXThang(int32_t result);

/// <remarks>MCX.EXE @ 0x00703c80</remarks>
void CallDoTheTCPThang(int32_t result);

/// <summary>Shows the LAN session list.</summary>
/// <remarks>MCX.EXE @ 0x00703c90</remarks>
void ShowLANScreen();

/// <summary>Leaves the game for the Zone's internet lobby (first variant; finds the launcher in the registry).</summary>
/// <remarks>MCX.EXE @ 0x00704070</remarks>
void DoExitToZone1();

/// <summary>Leaves the game for the Zone's internet lobby.</summary>
/// <remarks>MCX.EXE @ 0x007046a0</remarks>
void DoExitToZone();

/// <summary>Starts <c>mplaynow.exe</c> (the Mplayer lobby) and quits, or says it failed.</summary>
/// <remarks>MCX.EXE @ 0x00704810</remarks>
void DoExitToMplayer();

/// <summary>Asks whether to leave for the internet lobby (<see cref="DoExitToMplayer"/>).</summary>
/// <remarks>MCX.EXE @ 0x00704920</remarks>
void ShowInternet();

/// <summary>Hosts a new session.</summary>
/// <remarks>MCX.EXE @ 0x00704a90</remarks>
void HostGame();

/// <summary>Clears the ready room for a new session.</summary>
/// <remarks>MCX.EXE @ 0x00704b50</remarks>
void ResetReadyRoom();

/// <summary>Joins the session chosen in the list.</summary>
/// <remarks>MCX.EXE @ 0x00704bd0</remarks>
void JoinGame();

/// <summary>Creates the hosted session.</summary>
/// <remarks>MCX.EXE @ 0x00704dd0</remarks>
void CreateSession();

/// <remarks>MCX.EXE @ 0x00704eb0</remarks>
void CreateSerialSession();

/// <remarks>MCX.EXE @ 0x00704fb0</remarks>
void SerialJoinButtonPressed();

/// <remarks>MCX.EXE @ 0x00705040</remarks>
void JoinSerialSession();

/// <remarks>MCX.EXE @ 0x00705210</remarks>
void JoinModemSession();

/// <summary>Dials the number typed.</summary>
/// <returns>Nonzero on a connection.</returns>
/// <remarks>MCX.EXE @ 0x00705310</remarks>
int32_t DialModemSession();

/// <summary>Everyone left: back out of the session.</summary>
/// <remarks>MCX.EXE @ 0x007053a0</remarks>
void AllGoneCallback(int32_t result);

/// <summary>Moves a connected player on to the session (ready room) screen and locks the session.</summary>
/// <remarks>MCX.EXE @ 0x00705470</remarks>
void GOCallback();

/// <summary>The ready room's go button: launches into logistics.</summary>
/// <remarks>MCX.EXE @ 0x007054f0</remarks>
void GO();

/// <remarks>MCX.EXE @ 0x00705590</remarks>
void Leave();

/// <summary>Waits for an incoming modem call.</summary>
/// <remarks>MCX.EXE @ 0x007055a0</remarks>
void WaitForCall();

/// <remarks>MCX.EXE @ 0x00705720</remarks>
void GetNumber();

/// <remarks>MCX.EXE @ 0x00705760</remarks>
void CancelDial();

/// <remarks>MCX.EXE @ 0x00705780</remarks>
void Dial();

// aObject event routines of the menu screens' widgets.

/// <remarks>MCX.EXE @ 0x00705a50</remarks>
void ImageHandleEvent(aObject* object, aEvent* event);
/// <remarks>MCX.EXE @ 0x00705a70</remarks>
void ModemListHandleEvent(aObject* object, aEvent* event);
/// <remarks>MCX.EXE @ 0x00705ae0</remarks>
void PlayerListHandleEvent(aObject* object, aEvent* event);
/// <remarks>MCX.EXE @ 0x00705be0</remarks>
void ReadyRoomPlayerListHandleEvent(aObject* object, aEvent* event);
/// <remarks>MCX.EXE @ 0x00705d30</remarks>
void LanScreenHandleEvent(aObject* object, aEvent* event);
/// <remarks>MCX.EXE @ 0x00705dd0</remarks>
void LoadSaveScreenHandleEvent(aObject* object, aEvent* event);
/// <remarks>MCX.EXE @ 0x00705f00</remarks>
void ComPortTextHandleEvent(aObject* object, aEvent* event);
/// <remarks>MCX.EXE @ 0x00705fb0</remarks>
void SerialScreenHandleEvent(aObject* object, aEvent* event);
/// <remarks>MCX.EXE @ 0x00706020</remarks>
void ModemScreenHandleEvent(aObject* object, aEvent* event);
/// <remarks>MCX.EXE @ 0x00706080</remarks>
void PrefScreenHandleEvent(aObject* object, aEvent* event);

/// <summary>The brightness slider.</summary>
/// <remarks>MCX.EXE @ 0x00706090</remarks>
void SlideScreenBrightness(aObject* object, aEvent* event);
/// <summary>The music volume slider.</summary>
/// <remarks>MCX.EXE @ 0x007060b0</remarks>
void SlideMusicVolume(aObject* object, aEvent* event);
/// <summary>The radio volume slider.</summary>
/// <remarks>MCX.EXE @ 0x007060f0</remarks>
void SlideRadioVolume(aObject* object, aEvent* event);
/// <summary>The sound effects volume slider.</summary>
/// <remarks>MCX.EXE @ 0x00706130</remarks>
void SlideFXVolume(aObject* object, aEvent* event);

/// <summary>Difficulty: easy.</summary>
/// <remarks>MCX.EXE @ 0x00706170</remarks>
void EasyToggle();
/// <summary>Difficulty: regular.</summary>
/// <remarks>MCX.EXE @ 0x007061d0</remarks>
void RegularToggle();
/// <summary>Difficulty: hard.</summary>
/// <remarks>MCX.EXE @ 0x00706230</remarks>
void HardToggle();

/// <summary>Quits the game.</summary>
/// <remarks>MCX.EXE @ 0x00706290</remarks>
void DoExit();

/// <summary>The exit button: asks for confirmation.</summary>
/// <remarks>MCX.EXE @ 0x007062d0</remarks>
void CheckExit();

/// <summary>
/// Remembers <paramref name="name"/> as the multiplayer player name (registry value "Player Name", read back by
/// <c>MyGetUserName</c>). Null is ignored.
/// </summary>
/// <remarks>MCX.EXE @ 0x00706410 (after the file's last line-table entry, but it ends logmain.cpp)</remarks>
void SaveUserName(char* name);

/// <summary>
/// The ready room's player list refreshes (DAT_00808644): counted up by its timer, cleared when a session is joined
/// or created; <c>logistics.cpp</c> reads it too.
/// </summary>
extern int32_t readyRoomTicks;

/// <summary>The difficulty (0 easy, 1 regular, 2 hard; starts at 1).</summary>
extern int32_t GameDifficulty;

// The data folders, read from the settings at start-up.

/// <summary>Sounds on the CD.</summary>
extern char CDsoundPath[];
/// <summary>The in-mission interface art.</summary>
extern char interfacePath[];
/// <summary>The save games.</summary>
extern char savePath[];
/// <summary>Sprites on the CD.</summary>
extern char CDspritePath[];
/// <summary>The terrain (mission) files.</summary>
extern char terrainPath[];
/// <summary>The pilot profiles.</summary>
extern char warriorPath[];
/// <summary>The sprites.</summary>
extern char spritePath[];
/// <summary>The object profiles.</summary>
extern char profilePath[];
/// <summary>The fonts.</summary>
extern char fontPath[];
/// <summary>The DirectX redistributable (kept for the settings; unused by the port).</summary>
extern char directXPath[];
/// <summary>The sounds.</summary>
extern char soundPath[];
/// <summary>The shapes (hot spot files).</summary>
/// <remarks>MCX.EXE @ 0x007944cc</remarks>
extern char shapesPath[];

/// <summary>The application instance (an <c>HINSTANCE</c> in the original; string resources are loaded through it).</summary>
extern void* thisInstance;

/// <summary>A display option read at start-up (used by the GUI, camera, interface and terrain map).</summary>
extern int only45Pixel;

/// <summary>The logistics screens, while they exist.</summary>
extern Logistics* globalLogPtr;

/// <summary>The logistics state last left for a mission (shared with <c>logistics.cpp</c> and <c>mission.cpp</c>).</summary>
extern int32_t LastLogisticsMissionState;

/// <summary>Set while a single mission (not a campaign) is being loaded.</summary>
extern int LoadingSolo;

/// <summary>Set by <see cref="CancelDial"/> to break out of a modem wait.</summary>
extern int whackTimer;

/// <summary>Forces the 32 MB memory settings.</summary>
extern int force32MB;

/// <summary>Forces the 16 MB memory settings.</summary>
extern int force16MB;
