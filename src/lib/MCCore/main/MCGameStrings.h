#pragma once

// The game's interface text: MCX.EXE's string table, read by MCStringTable. Original source: main.cpp (cLoadString).

/// <summary>The prefs' "Language": the offset of the game's strings in the string table.</summary>
extern int32_t LanguageOffset;

/// <summary>
/// String resource <paramref name="id"/> (offset by the language), cut as the original's
/// <paramref name="bufferSize"/>-byte buffer cut it; empty when there is none.
/// </summary>
std::string LoadGameString(uint32_t id, int bufferSize);
