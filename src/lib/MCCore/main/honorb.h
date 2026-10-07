#pragma once

// The game's start-up and shut-down glue (honorb.cpp, the "HonorBound" codename): reading SYSTEM.CFG and the
// prefs, the CD check, creating the game's systems before the first frame, and tearing them down.

class MCGuiCallback;
class MCGuiEvent;
class MCGuiObject;
class MCAblDebuggerWindow;

/// <summary>The campaign control FIT userInit starts ("campaign", in the mission path).</summary>
extern char CampaignFile[20];
/// <summary>SYSTEM.CFG "missionName": the mission FIT a game segment (-mission N) starts. Declared in logistics.h.</summary>
extern char MissionName[80];
/// <summary>SYSTEM.CFG "DebuggerEnabled"/"IncludeDebugInfo" and the ABL debugger window's place.</summary>
extern uint32_t AblIncludeDebugInfo;
extern uint32_t AblDebuggerEnabled;
extern uint32_t AblDebuggerX;
extern uint32_t AblDebuggerY;
extern uint32_t AblDebuggerWidth;
extern uint32_t AblDebuggerHeight;
/// <summary>The display mode chosen from the prefs' "Resolution".</summary>
extern int32_t DisplayMode;
/// <summary>The ABL debugger's window, when the debugger is enabled.</summary>
extern MCAblDebuggerWindow* AblDebuggerWindow;
/// <summary>The palette cycling callback.</summary>
extern MCGuiCallback* ColorCallback;
/// <summary>Debug switch for the game system.</summary>
extern int DebugGameSystem;
/// <summary>
/// Port-only: switch sound and music off whatever SYSTEM.CFG says (the tests that run a mission: playback runs on real
/// time, and the radio and music roll the game's dice when a sound ends, so with sound two runs part).
/// </summary>
extern int GNoSound;
/// <summary>The screen saver, low-power and power-off settings found at start-up (restored on exit).</summary>
extern int ScreenSaverActive;
extern int LowPowerActive;
extern int PowerOffActive;
/// <summary>The prefs' "Language": the offset of the game's strings in the string table.</summary>
extern int32_t LanguageOffset;

/// <summary>Shuts down multiplayer, the mouse timer, sound and the display, then exits with 1.</summary>
[[noreturn]] void KillTheGame();
/// <summary>
/// Looks for the game CD in every drive, asking the player to insert it (or quit). The port always finds it.
/// </summary>
/// <returns>true when found, or when <paramref name="checkDisk"/> is 0.</returns>
bool CheckForCDInDrive(int32_t checkDisk, bool retry);
/// <summary>
/// Reads SYSTEM.CFG (heap sizes, ABL settings, paths, fast files) and the prefs (display, gamma, language,
/// difficulty, volumes), and sets the display mode.
/// </summary>
void SystemInit();
/// <summary>The ABL debugger's print callback: writes <paramref name="s"/> to its output window.</summary>
void AblDebuggerPrintCallback(std::string_view s);
/// <summary>The ABL debugger window's event routine: runs the typed commands (the ABL debugger's, and test
/// commands that host or join a multiplayer session).</summary>
void AblDebuggerEventRoutine(MCGuiObject* object, MCGuiEvent* event);
/// <summary>Creates the game's systems before the first frame: turns off the screen saver and power saving, makes
/// the ABL debugger window and the palette callback, the sound system, multiplayer (when lobby-launched), and the
/// mission.</summary>
/// <returns>0, or -1 when it can't.</returns>
int32_t UserInit();
/// <summary>Destroys what userInit made, closes the fast files and restores the screen saver settings.</summary>
void UserDestroy();
