#pragma once

/// <summary>
/// Stops the game with an error: <paramref name="errCode"/> and the message(s), shown to the player. The original
/// raised its crash reporter (AssertTest and a breakpoint); the port shows a message box and exits.
/// </summary>
/// <remarks>MCX.EXE @ 0x00644180</remarks>
[[noreturn]] void Fatal(int32_t errCode, const char* errMessage, const char* errMessage2 = nullptr);

/// <summary>Fatal error with the scenario clock appended (for errors during a mission).</summary>
/// <remarks>MCX.EXE @ 0x006440d0</remarks>
[[noreturn]] void FatalMsg(const char* message);

/// <summary>A general error message; in MCX.EXE it is the same as <see cref="FatalMsg"/>.</summary>
/// <remarks>MCX.EXE @ 0x006440b0</remarks>
[[noreturn]] void GeneralMsg(const char* message);

/// <summary>The last fatal message, with context, as the crash reporter showed it.</summary>
extern char McMsg1[1024];

/// <summary>Set while the display is in exclusive (DirectDraw) mode, when the original changed how it reported.</summary>
extern int inDirectDrawOnFatal;

/// <summary>
/// Game state added to fatal messages (the name of the mission's application, for the crash report). An empty
/// string outside a mission.
/// </summary>
extern char MissionAppName[256];

/// <summary>
/// When <paramref name="expression"/> is 0: reports "<paramref name="errCode"/> : message" (the original's crash
/// reporter could then break into the debugger); the game goes on.
/// </summary>
/// <remarks>MCX.EXE @ 0x00644250</remarks>
void Assert(int expression, uint32_t errCode, const char* errMessage, const char* errMessage2 = nullptr);
