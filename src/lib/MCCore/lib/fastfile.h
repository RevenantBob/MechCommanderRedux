#pragma once

class MCFastFile;

/// <summary>Opens a FastFile and adds it to the ones <c>File::open</c> searches.</summary>
/// <returns>Nonzero on success; on failure <see cref="FfLastError"/> says why.</returns>
int FastFileInit(const char* fname);

/// <summary>Closes every FastFile and frees the table.</summary>
void FastFileFini();

/// <summary>The FastFile that holds <paramref name="fname"/>, or null.</summary>
/// <remarks>
/// Original behaviour: finding the entry opens it (<c>MCFastFile::OpenFast</c>); the caller opens it again.
/// </remarks>
MCFastFile* FastFileFind(const char* fname);

/// <summary>The open FastFiles (<see cref="MaxFastFiles"/> slots).</summary>
extern MCFastFile** FastFiles;
/// <summary>How many FastFiles are open.</summary>
extern int32_t NumFastFiles;
/// <summary>The size of the <see cref="FastFiles"/> table, from SYSTEM.CFG's NumFastFiles.</summary>
extern int32_t MaxFastFiles;
/// <summary>Why the last <see cref="FastFileInit"/> failed.</summary>
extern int32_t FfLastError;
