#pragma once

class MCGameObject;
class MCWeaponFireChunk;
class MCWeaponHitChunk;

/// <summary>Prints the fields of one or two weapon fire chunks (and the attacker) to the chunk debug message.</summary>
void DebugWeaponFireChunk(MCWeaponFireChunk* chunk1, MCWeaponFireChunk* chunk2, MCGameObject* attacker);
/// <summary>Prints the fields of one or two weapon hit chunks to the chunk debug message.</summary>
void DebugWeaponHitChunk(MCWeaponHitChunk* chunk1, MCWeaponHitChunk* chunk2);
/// <summary>
/// Writes <see cref="ChunkDebugMsg"/> to <paramref name="fileName"/> and hands it to the crash report
/// (<c>ExceptionGameMsg</c>).
/// </summary>
void SaveChunkDebugMsg(std::string_view fileName);

/// <summary>
/// The text the chunk debug routines build (weapon, move and status chunks, ABL's debug dump) and the crash report
/// shows.
/// </summary>
extern std::string ChunkDebugMsg;
