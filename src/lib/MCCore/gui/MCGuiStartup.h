#pragma once

// The program's start (gui\asystem.cpp): the data paths, the GUI system's life, and the command line.

/// <summary>
/// The program's entry: sets the data paths, makes the GUI system (an <see cref="MCGameContext"/> system), starts and
/// runs it, then stops and deletes it.
/// </summary>
/// <param name="instance">The HINSTANCE in the original (kept as <c>ThisInstance</c>; unused by the port).</param>
/// <returns>0, or -4 when the game didn't start.</returns>
int RealWinMain(void* instance, std::string_view commandLine);

/// <summary>
/// Reads the command line's switches: <c>-mission n</c> (or <c>+ n</c>), <c>-network file</c>, <c>-load file</c>, and
/// the port's <c>-renderer</c>, <c>-gpudraw</c>, <c>-gpudump</c>, <c>-framelog</c>, <c>-fps</c> and <c>-novsync</c>.
/// </summary>
void ParseCommandLine(std::string_view commandLine);
