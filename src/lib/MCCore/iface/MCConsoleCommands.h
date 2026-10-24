#pragma once

/// <summary>
/// Port-only: runs a debug console line (<see cref="MCGuiConsole"/>): its first word names the command, the rest are
/// its arguments, case blind. The commands:
/// <list type="bullet">
/// <item><c>destroyarm right|left</c>: blows that arm off the selected mechs, as a destroyed arm does.</item>
/// </list>
/// </summary>
/// <returns>Nothing, or why the line did nothing (an unknown command, a wrong argument, nothing to act on).</returns>
std::expected<void, std::string> RunConsoleCommand(std::string_view line);
