#pragma once

class FastFile;

/// <summary>Opens a FastFile and adds it to the ones <c>File::open</c> searches.</summary>
/// <returns>Nonzero on success; on failure <see cref="ffLastError"/> says why.</returns>
/// <remarks>MCX.EXE @ 0x00644bd0</remarks>
int FastFileInit(const char* fname);

/// <summary>Closes every FastFile and frees the table.</summary>
/// <remarks>MCX.EXE @ 0x00644c50</remarks>
void FastFileFini();

/// <summary>The FastFile that holds <paramref name="fname"/>, or null.</summary>
/// <remarks>
/// MCX.EXE @ 0x00644cc0. Original behaviour: finding the entry opens it (<c>FastFile::openFast</c>); the caller opens
/// it again.
/// </remarks>
FastFile* FastFileFind(const char* fname);

/// <summary>The open FastFiles (<see cref="maxFastFiles"/> slots).</summary>
extern FastFile** fastFiles;
/// <summary>How many FastFiles are open.</summary>
extern int32_t numFastFiles;
/// <summary>The size of the <see cref="fastFiles"/> table, from SYSTEM.CFG's NumFastFiles.</summary>
extern int32_t maxFastFiles;
/// <summary>Why the last <see cref="FastFileInit"/> failed.</summary>
extern int32_t ffLastError;
