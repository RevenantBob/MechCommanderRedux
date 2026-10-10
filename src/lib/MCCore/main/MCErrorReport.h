#pragma once

// What is left of the original's crash reporter (main.cpp). MCX.EXE walked the stack with imagehlp, gathered the
// machine, DLL and game details, the logs and a screen grab, and showed a dialog that could mail the report to FASA;
// the port's crash handling is MCCrashTrace (a symbolized stack and a minidump).

/// <summary>
/// Reports error <paramref name="errorCode"/> with <paramref name="text"/> (the in-game "User Break"): logs it and asks
/// whether to continue, break into the debugger or quit (which ends the game). A second report while one is open
/// quits. An unattended run (MCNoMessageBoxes) quits at once.
/// </summary>
/// <returns>Whether the caller should break into the debugger.</returns>
bool AssertTest(int32_t errorCode, std::string_view text);
