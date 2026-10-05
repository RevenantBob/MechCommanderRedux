#pragma once

// Original source: mcx\lib\aerror.cpp. The game's error reports.

/// <summary>
/// Stops the game with an error: <paramref name="errCode"/> and the message(s), shown to the player. The original
/// raised its crash reporter (AssertTest and a breakpoint); the port logs, shows a message box and exits.
/// </summary>
[[noreturn]] void Fatal(int32_t errCode, std::string_view message = {}, std::string_view message2 = {});

/// <summary>A fatal error during a mission: the message with the scenario clock.</summary>
[[noreturn]] void FatalMsg(std::string_view message);

/// <summary>A general error message; in MCX.EXE it is the same as <see cref="FatalMsg"/>.</summary>
[[noreturn]] void GeneralMsg(std::string_view message);

/// <summary>
/// When <paramref name="expression"/> is false: reports "<paramref name="errCode"/> : message" (the original's crash
/// reporter could then break into the debugger); the game goes on.
/// </summary>
void Assert(bool expression, uint32_t errCode, std::string_view message = {}, std::string_view message2 = {});

/// <summary>
/// Port-only: when set (by mc_tests), <see cref="Fatal"/> only logs, with no message box to block an unattended run.
/// </summary>
extern bool MCNoMessageBoxes;
