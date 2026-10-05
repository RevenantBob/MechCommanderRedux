#pragma once

#include "object/MCWeaponFireChunk.h"
#include "object/MCWeaponHitChunk.h"

class MCGameObject;

/// <summary>Prints the fields of one or two weapon fire chunks (and the attacker) to the chunk debug message.</summary>
void DebugWeaponFireChunk(const MCWeaponFireChunk& chunk1, std::optional<MCWeaponFireChunk> chunk2,
                          std::shared_ptr<MCGameObject> attacker);
/// <summary>Prints the fields of one or two weapon hit chunks to the chunk debug message.</summary>
void DebugWeaponHitChunk(const MCWeaponHitChunk& chunk1, std::optional<MCWeaponHitChunk> chunk2);
/// <summary>
/// Writes <see cref="ChunkDebugMsg"/> to <paramref name="fileName"/>. (The original also handed it to its crash report,
/// which the port doesn't have.)
/// </summary>
void SaveChunkDebugMsg(std::string_view fileName);

/// <summary>
/// The text the chunk debug routines build (weapon, move and status chunks, ABL's debug dump) and the crash report
/// shows.
/// </summary>
extern std::string ChunkDebugMsg;
