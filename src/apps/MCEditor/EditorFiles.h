#pragma once

// The port's own: the folder the editor works in, and installing the Mission Editor's own files there. The original
// read the folder from the registry (HKCU\Software\FIT\Dreditor\Paths\DataPath), which its setup wrote along with the
// files. The port works in its own folder (the game folder, or the one -d/--data names) and, when the files are
// missing, copies them from an installed editor or extracts them from the Gold CD's editor setup.

#include <expected>
#include <string>
#include <vector>

/// <summary>
/// The game folder the editor works in, without a trailing backslash: the one <see cref="SetEditorDataPath"/> set, else
/// the exe's own folder.
/// </summary>
CString EditorDataPath();

/// <summary>Makes <paramref name="folder"/> (-d/--data on the command line) the folder the editor works in.</summary>
void SetEditorDataPath(const CString& folder);

/// <summary>Whether <paramref name="folder"/> is a MechCommander Gold folder: one with SYSTEM.CFG.</summary>
bool IsGameFolder(const CString& folder);

/// <summary>The editor's files missing from <paramref name="folder"/>\Teditor.</summary>
std::vector<CString> MissingEditorFiles(const CString& folder);

/// <summary>
/// Puts the editor's files into <paramref name="folder"/> (Teditor\ and the sounds in Data\Sound\), as the original
/// setup did, from <paramref name="source"/>: an installed editor's folder (or its Teditor folder) is copied, and the
/// Gold CD (or its MCEDSETP\DISK1 folder) is extracted. Files already there are kept.
/// </summary>
std::expected<void, std::string> InstallEditorFiles(const CString& source, const CString& folder);

/// <summary>The root of the first CD-ROM drive that holds the editor's setup, or empty.</summary>
CString FindEditorSetupDisc();
