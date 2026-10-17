#include "stdafx.h"
#include "EditorFiles.h"
#include "InstallShieldCabinet.h"

#include <filesystem>
#include <format>
#include <fstream>

namespace fs = std::filesystem;

namespace
{
    /// <summary>The file a game folder has.</summary>
    inline constexpr const char* SYSTEM_CONFIG = "SYSTEM.CFG";
    /// <summary>The editor's folder in the game folder.</summary>
    inline constexpr const char* TEDITOR_FOLDER = "Teditor";
    /// <summary>Where the setup put the editor's sounds, in the game folder.</summary>
    inline constexpr const char* SOUND_FOLDER = "Data\\Sound";
    /// <summary>The editor setup's folder on the Gold CD, and its cabinet.</summary>
    inline constexpr const char* SETUP_FOLDER = "MCEDSETP\\DISK1";
    inline constexpr const char* SETUP_DISK_FOLDER = "DISK1";
    inline constexpr const char* CABINET_HEADER = "DATA1.HDR";
    inline constexpr const char* CABINET_DATA = "DATA1.CAB";

    /// <summary>
    /// The files the setup puts in Teditor\, as the cabinet spells them: the mission database, the tile and object
    /// tables, the templates the saves fill in, the starting inventories, the brain templates, the palette and cursor.
    /// The editor needs all of them.
    /// </summary>
    inline constexpr const char* TEDITOR_FILES[] = {
        "fitabl.mdb",   "Tiles.csv",    "TedIds.csv",   "template.txt",  "template.fit", "campaign.txt",
        "sol.txt",      "MPK.txt",      "logistic.pal", "bmech.ANI",     "issolo.pur",   "clansolo.pur",
        "ambush01.txt", "attack01.txt", "Circle01.txt", "destroy01.txt", "escort01.txt", "goto01.txt",
        "guard01.txt",  "kill01.txt",   "null01.txt",   "patrol01.txt",  "sentry01.txt"};

    /// <summary>The sounds the setup puts in Data\Sound\. Without them the editor is only quieter.</summary>
    inline constexpr const char* SOUND_FILES[] = {"button7.wav",  "cmpwin.wav", "plt24tnt.WAV",
                                                  "sfx_thor.wav", "steam.wav",  "wrongbt.WAV"};

    /// <summary>The folder <see cref="SetEditorDataPath"/> set.</summary>
    CString dataPathOverride;

    /// <summary>
    /// The folder <paramref name="text"/> names, without a trailing backslash but for a drive's root ("E:" becomes
    /// "E:\", which "E:" joined with a name wouldn't be).
    /// </summary>
    fs::path toPath(const CString& text)
    {
        CString folder = text;
        folder.Trim();

        if (folder.GetLength() == 2 && folder[1] == ':')
        {
            folder += '\\';
        }
        else if (folder.GetLength() > 3)
        {
            folder.TrimRight('\\');
        }

        return fs::path(static_cast<LPCTSTR>(folder));
    }

    /// <summary>Whether <paramref name="path"/> is a file.</summary>
    bool isFile(const fs::path& path)
    {
        std::error_code error;
        return fs::is_regular_file(path, error);
    }

    /// <summary>Whether <paramref name="folder"/> has every Teditor file.</summary>
    bool hasEditorFiles(const fs::path& folder)
    {
        return std::ranges::all_of(TEDITOR_FILES, [&](const char* name) { return isFile(folder / name); });
    }

    /// <summary>Makes <paramref name="path"/> writable: files copied from a CD are read-only.</summary>
    void makeWritable(const fs::path& path)
    {
        std::error_code error;
        fs::permissions(path, fs::perms::owner_write, fs::perm_options::add, error);
    }

    /// <summary>Copies <paramref name="name"/> from <paramref name="from"/> to <paramref name="to"/> unless it's there.</summary>
    std::expected<void, std::string> copyFile(const fs::path& from, const fs::path& to, const char* name)
    {
        if (isFile(to / name))
        {
            return {};
        }

        std::error_code error;
        fs::copy_file(from / name, to / name, error);

        if (error)
        {
            return std::unexpected(
                std::format("Couldn't copy {} to {}: {}", (from / name).string(), to.string(), error.message()));
        }

        makeWritable(to / name);
        return {};
    }

    /// <summary>Extracts <paramref name="name"/> from <paramref name="cabinet"/> into <paramref name="to"/> unless it's there.</summary>
    std::expected<void, std::string> extractFile(const InstallShieldCabinet& cabinet, const fs::path& to,
                                                 const char* name)
    {
        if (isFile(to / name))
        {
            return {};
        }

        const auto file = cabinet.Find(name);

        if (!file)
        {
            return std::unexpected(std::format("The editor's setup has no {}.", name));
        }

        const auto contents = cabinet.Extract(*file);

        if (!contents)
        {
            return std::unexpected(contents.error());
        }

        // Written beside the file and renamed when complete, so a failed write leaves no partial file to be used.
        const fs::path partial = to / (std::string(name) + ".partial");

        {
            std::ofstream stream(partial, std::ios::binary | std::ios::trunc);

            if (!stream || !stream.write(reinterpret_cast<const char*>(contents->data()),
                                         static_cast<std::streamsize>(contents->size())))
            {
                std::error_code ignored;
                fs::remove(partial, ignored);
                return std::unexpected(std::format("Couldn't write {}.", (to / name).string()));
            }
        }

        std::error_code error;
        fs::rename(partial, to / name, error);

        if (error)
        {
            fs::remove(partial, error);
            return std::unexpected(std::format("Couldn't write {}.", (to / name).string()));
        }

        return {};
    }

    /// <summary>Makes <paramref name="folder"/> and the folders above it.</summary>
    std::expected<void, std::string> makeFolder(const fs::path& folder)
    {
        std::error_code error;
        fs::create_directories(folder, error);

        if (error)
        {
            return std::unexpected(std::format("Couldn't make {}: {}", folder.string(), error.message()));
        }

        return {};
    }

    /// <summary>Copies the files from the installed editor whose Teditor folder is <paramref name="teditor"/>.</summary>
    std::expected<void, std::string> copyInstalled(const fs::path& teditor, const fs::path& source,
                                                   const fs::path& folder)
    {
        const fs::path teditorTarget = folder / TEDITOR_FOLDER;
        const fs::path soundTarget = folder / SOUND_FOLDER;

        if (auto made = makeFolder(teditorTarget); !made)
        {
            return made;
        }

        for (const char* name : TEDITOR_FILES)
        {
            if (auto copied = copyFile(teditor, teditorTarget, name); !copied)
            {
                return copied;
            }
        }

        // The sounds are in the install's Data\Sound, beside its Teditor folder; an install without them still works.
        const fs::path soundSource = (teditor == source ? source.parent_path() : source) / SOUND_FOLDER;

        for (const char* name : SOUND_FILES)
        {
            if (isFile(soundSource / name))
            {
                if (auto made = makeFolder(soundTarget); !made)
                {
                    return made;
                }

                if (auto copied = copyFile(soundSource, soundTarget, name); !copied)
                {
                    return copied;
                }
            }
        }

        return {};
    }

    /// <summary>Extracts the files from the editor setup in <paramref name="setup"/> (DATA1.HDR and DATA1.CAB).</summary>
    std::expected<void, std::string> extractSetup(const fs::path& setup, const fs::path& folder)
    {
        const auto cabinet = InstallShieldCabinet::Open(setup / CABINET_HEADER, setup / CABINET_DATA);

        if (!cabinet)
        {
            return std::unexpected(cabinet.error());
        }

        const fs::path teditorTarget = folder / TEDITOR_FOLDER;
        const fs::path soundTarget = folder / SOUND_FOLDER;

        for (const auto& target : {teditorTarget, soundTarget})
        {
            if (auto made = makeFolder(target); !made)
            {
                return made;
            }
        }

        for (const char* name : TEDITOR_FILES)
        {
            if (auto extracted = extractFile(*cabinet, teditorTarget, name); !extracted)
            {
                return extracted;
            }
        }

        for (const char* name : SOUND_FILES)
        {
            if (auto extracted = extractFile(*cabinet, soundTarget, name); !extracted)
            {
                return extracted;
            }
        }

        return {};
    }

    /// <summary>The folder of <paramref name="source"/> that holds the editor setup's cabinet, or empty.</summary>
    fs::path findSetup(const fs::path& source)
    {
        for (const fs::path& candidate : {source / SETUP_FOLDER, source / SETUP_DISK_FOLDER, source})
        {
            if (isFile(candidate / CABINET_HEADER) && isFile(candidate / CABINET_DATA))
            {
                return candidate;
            }
        }

        return {};
    }
}

CString EditorDataPath()
{
    if (!dataPathOverride.IsEmpty())
    {
        return dataPathOverride;
    }

    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    return CString(fs::path(exePath).parent_path().string().c_str());
}

void SetEditorDataPath(const CString& folder)
{
    dataPathOverride = CString(toPath(folder).string().c_str());
    dataPathOverride.TrimRight('\\');
}

bool IsGameFolder(const CString& folder)
{
    return isFile(toPath(folder) / SYSTEM_CONFIG);
}

std::vector<CString> MissingEditorFiles(const CString& folder)
{
    const fs::path teditor = toPath(folder) / TEDITOR_FOLDER;
    std::vector<CString> missing;

    for (const char* name : TEDITOR_FILES)
    {
        if (!isFile(teditor / name))
        {
            missing.emplace_back(name);
        }
    }

    return missing;
}

std::expected<void, std::string> InstallEditorFiles(const CString& source, const CString& folder)
{
    const fs::path sourcePath = toPath(source);
    const fs::path folderPath = toPath(folder);

    if (sourcePath.empty())
    {
        return std::unexpected("Choose the MechCommander Gold CD or a folder where the Mission Editor is installed.");
    }

    // An installed editor: its Teditor folder, or that folder itself, has every file.
    for (const fs::path& teditor : {sourcePath / TEDITOR_FOLDER, sourcePath})
    {
        if (hasEditorFiles(teditor))
        {
            return copyInstalled(teditor, sourcePath, folderPath);
        }
    }

    // The CD: the editor's setup cabinet.
    if (const fs::path setup = findSetup(sourcePath); !setup.empty())
    {
        return extractSetup(setup, folderPath);
    }

    return std::unexpected(std::format(
        "{} has neither the Mission Editor's files (a Teditor folder) nor its setup (MCEDSETP\\DISK1\\DATA1.CAB). "
        "Choose the MechCommander Gold CD, or the folder the Mission Editor is installed in.",
        sourcePath.string()));
}

CString FindEditorSetupDisc()
{
    // An empty drive fails quietly instead of asking for a disc.
    DWORD previousMode = 0;
    SetThreadErrorMode(SEM_FAILCRITICALERRORS, &previousMode);
    const DWORD drives = GetLogicalDrives();
    CString found;

    for (char letter = 'A'; letter <= 'Z' && found.IsEmpty(); letter++)
    {
        if ((drives & (1u << (letter - 'A'))) == 0)
        {
            continue;
        }

        const char root[] = {letter, ':', '\\', '\0'};

        if (GetDriveTypeA(root) == DRIVE_CDROM && !findSetup(fs::path(root)).empty())
        {
            found = root;
        }
    }

    SetThreadErrorMode(previousMode, nullptr);
    return found;
}
