#pragma once

// The pieces of File > Write Distribution File (.dpk): a mission's files copied under a temporary name, its scripts
// rewritten to include the copies, and the FastFile packer that makes the .dpk (0x00429460, 0x00429530 and the game
// tools' makefst at 0x0047e1f0). Their source file names weren't kept; the names are the port's.

class CStringArray;

/// <summary>
/// Copies <paramref name="folder"/><paramref name="name"/><paramref name="extension"/> to
/// <paramref name="folder"/><paramref name="copyName"/><paramref name="extension"/>, over any file there.
/// </summary>
/// <returns>Whether it copied.</returns>
/// <remarks>0x00429460.</remarks>
bool copyMissionFile(const CString& name, const CString& copyName, const char* folder, const char* extension);

/// <summary>
/// Writes <paramref name="folder"/><paramref name="name"/><paramref name="extension"/> again as
/// <paramref name="folder"/><paramref name="copyName"/><paramref name="extension"/>, line by line, giving the lines
/// <paramref name="lines"/> name in turn <paramref name="copyName"/> in place of <paramref name="name"/>.
/// </summary>
/// <param name="name">
/// The mission's name; lower-cased in place (the caller's later file names use it lower case), as are
/// <paramref name="lines"/>, which are found in each line without regard to case.
/// </param>
/// <returns>false when either file doesn't open (after a message box).</returns>
/// <remarks>
/// 0x00429530. Original behaviour: after the last line the line before it is looked at again for the next of
/// <paramref name="lines"/>; if it holds that one too, <paramref name="copyName"/> is written once more on its own.
/// </remarks>
bool rewriteMissionFile(CString& name, const CString& copyName, const char* folder, const char* extension,
                        CStringArray& lines);

/// <summary>
/// Builds FastFile <paramref name="fastFileName"/> from the files <paramref name="responseName"/> lists, a line each up
/// to the last line: each entry named as its line, LZ packed when that is smaller.
/// </summary>
/// <remarks>
/// 0x0047e1f0. EB-128 fixed: when the list or a listed file doesn't open, or the FastFile can't be made, the original
/// prints a fatal error to the (missing) console and exits the editor; the port stops, leaving what it has written.
/// </remarks>
void buildFastFile(const char* responseName, const char* fastFileName);
