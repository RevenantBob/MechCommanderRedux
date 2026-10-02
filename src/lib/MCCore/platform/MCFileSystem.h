#pragma once

/// <summary>
/// Where the game's files are, and how its paths map onto the disk. The original ran from its install folder and
/// opened files by relative DOS paths (<c>data\art\ACCESS00.tga</c>, any case). The port runs from anywhere: every
/// relative path the game uses resolves against <see cref="GameRoot"/>, the retail install (<c>--data</c>), with
/// <c>\</c> as a separator and case ignored, so the same paths work on a case-sensitive file system.
///
/// The install is never written to. When a user folder is set (<see cref="SetUserRoot"/>), it overlays the install:
/// a relative path is looked up there first, and everything the game creates, renames or deletes happens there
/// (saves, PREFS.CFG, the temp FITs). Without one (the tests), both roots are the install.
/// </summary>
namespace MCFileSystem
{
    /// <summary>Sets the folder of the retail install (the one holding MCX.EXE's data: ART.FST, DATA\, ...).</summary>
    void SetGameRoot(const std::filesystem::path& root);

    /// <summary>The folder of the retail install.</summary>
    const std::filesystem::path& GameRoot();

    /// <summary>Sets the writable folder that overlays the install (created if missing).</summary>
    void SetUserRoot(const std::filesystem::path& root);

    /// <summary>The writable folder: the user folder when set, else the install.</summary>
    const std::filesystem::path& UserRoot();

    /// <summary>
    /// Maps a path as the game writes it to the file on disk, for reading. A relative path is taken from the user
    /// folder when it exists there, else from the game root; each component is matched without regard to case
    /// against what exists. Components that don't exist yet (a file about to be created) are kept as written.
    /// </summary>
    /// <param name="gamePath">The path as the original code spells it.</param>
    std::filesystem::path Resolve(std::string_view gamePath);

    /// <summary>
    /// Maps a path to where the game may write it: under the user folder for a relative path (its folders are
    /// created). When <paramref name="copyExisting"/> is set and the file exists only in the install, it is copied
    /// over first, so opening it for update changes the copy.
    /// </summary>
    std::filesystem::path ResolveWrite(std::string_view gamePath, bool copyExisting = false);

    /// <summary>Creates a folder (the CRT's <c>_mkdir</c>), under the user folder for a relative path.</summary>
    /// <returns>Whether the folder exists afterwards.</returns>
    bool MakeDirectory(std::string_view gamePath);

    /// <summary>Deletes an empty folder (the CRT's <c>_rmdir</c>). Folders in the install are left alone.</summary>
    /// <returns>Whether the folder was deleted.</returns>
    bool RemoveDirectory(std::string_view gamePath);

    /// <summary>
    /// The files in a folder whose names match a DOS wildcard (<c>*</c>, <c>?</c>; case ignored), as the CRT's
    /// <c>_findfirst</c>/<c>_findnext</c> list them: names only, user folder and install merged.
    /// </summary>
    std::vector<std::string> FindFiles(std::string_view gamePattern);

    /// <summary>Whether <paramref name="gamePath"/> names an existing loose file (not one inside a FastFile).</summary>
    bool Exists(std::string_view gamePath);

    /// <summary>Deletes a loose file (the CRT's <c>remove</c>). Files in the install are left alone.</summary>
    /// <returns>Whether a file was deleted.</returns>
    bool RemoveFile(std::string_view gamePath);

    /// <summary>
    /// Renames a loose file (the CRT's <c>rename</c>). Unlike the CRT's, an existing target is replaced; the original
    /// removed the target first anyway.
    /// </summary>
    /// <returns>Whether the file was renamed.</returns>
    bool RenameFile(std::string_view fromGamePath, std::string_view toGamePath);

    /// <summary>
    /// Copies a loose file over another (Win32 <c>CopyFile</c> with overwrite, as the original's editors used to keep
    /// a <c>.bak</c>).
    /// </summary>
    /// <returns>Whether the copy was made.</returns>
    bool CopyFile(std::string_view fromGamePath, std::string_view toGamePath);
}
