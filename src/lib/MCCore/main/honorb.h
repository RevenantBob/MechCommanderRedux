#pragma once

// The game's start-up and shut-down glue (honorb.cpp, the "HonorBound" codename): reading SYSTEM.CFG and the
// prefs, the CD check, creating the game's systems before the first frame, and tearing them down.

class aCallback;
class aEvent;
class aObject;
class DebuggerWindow;

/// <summary>The campaign control FIT userInit starts ("campaign", in the mission path).</summary>
extern char campaignFile[20];
/// <summary>SYSTEM.CFG "missionName": the mission FIT a game segment (-mission N) starts. Declared in logistics.h.</summary>
extern char missionName[80];
/// <summary>SYSTEM.CFG "DebuggerEnabled"/"IncludeDebugInfo" and the ABL debugger window's place.</summary>
extern uint32_t AblIncludeDebugInfo;
extern uint32_t AblDebuggerEnabled;
extern uint32_t AblDebuggerX;
extern uint32_t AblDebuggerY;
extern uint32_t AblDebuggerWidth;
extern uint32_t AblDebuggerHeight;
/// <summary>The display mode chosen from the prefs' "Resolution".</summary>
extern int32_t displayMode;
/// <summary>The ABL debugger's window, when the debugger is enabled.</summary>
extern DebuggerWindow* ABLDebuggerWindow;
/// <summary>The palette cycling callback.</summary>
extern aCallback* colorCallback;
/// <summary>Debug switch for the game system.</summary>
extern int DebugGameSystem;
/// <summary>
/// Port-only: switch sound and music off whatever SYSTEM.CFG says (the tests that run a mission: playback runs on real
/// time, and the radio and music roll the game's dice when a sound ends, so with sound two runs part).
/// </summary>
extern int gNoSound;
/// <summary>The screen saver, low-power and power-off settings found at start-up (restored on exit).</summary>
extern int ScreenSaverActive;
extern int LowPowerActive;
extern int PowerOffActive;
/// <summary>The prefs' "Language": the offset of the game's strings in the string table.</summary>
extern int32_t languageOffset;

/// <summary>Shuts down multiplayer, the mouse timer, sound and the display, then exits with 1.</summary>
/// <remarks>MCX.EXE @ 0x00758d50</remarks>
[[noreturn]] void killTheGame();
/// <summary>
/// Looks for the game CD in every drive, asking the player to insert it (or quit). The port always finds it.
/// </summary>
/// <returns>true when found, or when <paramref name="checkDisk"/> is 0.</returns>
/// <remarks>MCX.EXE @ 0x00758da0</remarks>
bool checkForCDInDrive(int32_t checkDisk, bool retry);
/// <summary>
/// Reads SYSTEM.CFG (heap sizes, ABL settings, paths, fast files) and the prefs (display, gamma, language,
/// difficulty, volumes), and sets the display mode.
/// </summary>
/// <remarks>MCX.EXE @ 0x007590d0</remarks>
void systemInit();
/// <summary>The ABL debugger's print callback: writes <paramref name="s"/> to its output window.</summary>
/// <remarks>MCX.EXE @ 0x00759b30</remarks>
void ABLDebuggerPrintCallback(char* s);
/// <summary>The ABL debugger window's event routine: runs the typed commands (the ABL debugger's, and test
/// commands that host or join a multiplayer session).</summary>
/// <remarks>MCX.EXE @ 0x00759b40</remarks>
void ABLDebuggerEventRoutine(aObject* object, aEvent* event);
/// <summary>Creates the game's systems before the first frame: turns off the screen saver and power saving, makes
/// the ABL debugger window and the palette callback, the sound system, multiplayer (when lobby-launched), and the
/// mission.</summary>
/// <returns>0, or -1 when it can't.</returns>
/// <remarks>MCX.EXE @ 0x0075a470</remarks>
int32_t userInit();
/// <summary>Destroys what userInit made, closes the fast files and restores the screen saver settings.</summary>
/// <remarks>MCX.EXE @ 0x0075a880</remarks>
void userDestroy();
