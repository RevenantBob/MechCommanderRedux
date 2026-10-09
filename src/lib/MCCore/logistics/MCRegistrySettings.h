#pragma once

// The registry values the menus keep (MCRegistry: the port's registry.cfg): the game's version and language, and the
// multiplayer player name. Original source: logistics\logmain.cpp.

/// <summary>The registry key of the game's version, language and player name.</summary>
inline constexpr std::string_view GameRegistryKey = "Software\\Fasa Interactive\\MechCommander Expansion";

/// <summary>
/// Whether version <paramref name="stored"/> (as the settings hold it) is <paramref name="build"/>'s: the same first 13
/// characters, then a dot.
/// </summary>
bool RegistryVersionMatches(std::string_view stored, std::string_view build);

/// <summary>Whether the version stored in the settings matches this build.</summary>
bool CheckRegistryVersionNumber();

/// <summary>Stores this build's version ("<i>build</i>.") and language in the settings.</summary>
void WriteRegistryVersionNumber();

/// <summary>Stores this build's version unless the settings already hold it (each menu action's first step).</summary>
void EnsureRegistryVersion();

/// <summary>
/// Remembers <paramref name="name"/> as the multiplayer player name (registry value "Player Name", read back by
/// <c>MyGetUserName</c>).
/// </summary>
void SaveUserName(std::string_view name);
