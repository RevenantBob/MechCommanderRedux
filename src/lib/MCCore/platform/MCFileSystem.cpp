#include "stdafx.h"
#include "platform/MCFileSystem.h"
#include "main/MCGameContext.h"

namespace
{
    std::vector<std::string> SplitGamePath(std::string_view path)
    {
        std::vector<std::string> parts;
        std::string current;

        for (char c : path)
        {
            if (c == '\\' || c == '/')
            {
                if (!current.empty())
                {
                    parts.push_back(std::move(current));
                }

                current.clear();
            }
            else
            {
                current.push_back(c);
            }
        }

        if (!current.empty())
        {
            parts.push_back(std::move(current));
        }

        return parts;
    }

    bool IsAbsoluteGamePath(std::string_view path)
    {
        if (path.size() >= 2 && path[1] == ':')
        {
            return true;
        }

        return !path.empty() && (path[0] == '\\' || path[0] == '/');
    }

    /// <summary>Resolves <paramref name="gamePath"/> against <paramref name="root"/> (for a relative path).</summary>
    std::filesystem::path ResolveFrom(const std::filesystem::path& root, std::string_view gamePath)
    {
        std::filesystem::path result;
        std::vector<std::string> parts = SplitGamePath(gamePath);
        size_t first = 0;

        if (IsAbsoluteGamePath(gamePath))
        {
            if (gamePath.size() >= 2 && gamePath[1] == ':')
            {
                result = std::string(gamePath.substr(0, 2)) + "/";
                first = 1;
            }
            else
            {
                result = "/";
            }
        }
        else
        {
            result = root;
        }

        for (size_t i = first; i < parts.size(); ++i)
        {
            const std::string& part = parts[i];

            if (part == ".")
            {
                continue;
            }

            if (part == "..")
            {
                result = result.parent_path();
                continue;
            }

            std::filesystem::path exact = result / part;
            std::error_code error;

            if (std::filesystem::exists(exact, error))
            {
                result = std::move(exact);
                continue;
            }

            // Not there as spelled: look for the entry that matches without regard to case.
            bool found = false;

            if (std::filesystem::is_directory(result, error))
            {
                for (const auto& entry : std::filesystem::directory_iterator(result, error))
                {
                    const std::string name = entry.path().filename().string();

                    if (MCPort::StrICmp(name.c_str(), part.c_str()) == 0)
                    {
                        result = entry.path();
                        found = true;
                        break;
                    }
                }
            }

            if (!found)
            {
                result = std::move(exact);
            }
        }

        return result;
    }

    void AddMatches(const std::filesystem::path& folder, const std::string& pattern, std::vector<std::string>& names)
    {
        std::error_code error;

        if (!std::filesystem::is_directory(folder, error))
        {
            return;
        }

        for (const auto& entry : std::filesystem::directory_iterator(folder, error))
        {
            if (!entry.is_regular_file(error))
            {
                continue;
            }

            const std::string name = entry.path().filename().string();

            if (!MCFileSystem::WildcardMatch(pattern.c_str(), name.c_str()))
            {
                continue;
            }

            const bool listed = std::ranges::any_of(names, [&](const std::string& other)
                                                    { return MCPort::StrICmp(other.c_str(), name.c_str()) == 0; });

            if (!listed)
            {
                names.push_back(name);
            }
        }
    }
}

MCDiskFileSource::MCDiskFileSource() : _Root(std::filesystem::current_path())
{
}

bool MCDiskFileSource::HasOverlay(std::string_view gamePath) const
{
    return !_UserRoot.empty() && !IsAbsoluteGamePath(gamePath);
}

void MCDiskFileSource::SetGameRoot(const std::filesystem::path& root)
{
    _Root = root;
}

const std::filesystem::path& MCDiskFileSource::GameRoot() const
{
    return _Root;
}

void MCDiskFileSource::SetUserRoot(const std::filesystem::path& root)
{
    _UserRoot = root;

    if (!root.empty())
    {
        std::error_code error;
        std::filesystem::create_directories(root, error);
    }
}

const std::filesystem::path& MCDiskFileSource::UserRoot() const
{
    return _UserRoot.empty() ? _Root : _UserRoot;
}

std::filesystem::path MCDiskFileSource::Resolve(std::string_view gamePath)
{
    if (HasOverlay(gamePath))
    {
        std::filesystem::path user = ResolveFrom(_UserRoot, gamePath);
        std::error_code error;

        if (std::filesystem::exists(user, error))
        {
            return user;
        }
    }

    return ResolveFrom(_Root, gamePath);
}

std::filesystem::path MCDiskFileSource::ResolveWrite(std::string_view gamePath, bool copyExisting)
{
    if (!HasOverlay(gamePath))
    {
        return ResolveFrom(_Root, gamePath);
    }

    std::filesystem::path user = ResolveFrom(_UserRoot, gamePath);
    std::error_code error;
    std::filesystem::create_directories(user.parent_path(), error);

    if (copyExisting && !std::filesystem::exists(user, error))
    {
        const std::filesystem::path installed = ResolveFrom(_Root, gamePath);

        if (std::filesystem::is_regular_file(installed, error))
        {
            std::filesystem::copy_file(installed, user, error);
        }
    }

    return user;
}

bool MCDiskFileSource::MakeDirectory(std::string_view gamePath)
{
    const std::filesystem::path folder = ResolveWrite(gamePath, false);
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    return std::filesystem::is_directory(folder, error);
}

bool MCDiskFileSource::RemoveDirectory(std::string_view gamePath)
{
    const std::filesystem::path folder = ResolveWrite(gamePath, false);
    std::error_code error;

    if (!std::filesystem::is_directory(folder, error) || !std::filesystem::is_empty(folder, error))
    {
        return false;
    }

    return std::filesystem::remove(folder, error);
}

std::vector<std::string> MCDiskFileSource::FindFiles(std::string_view gamePattern)
{
    std::string folder(gamePattern);
    std::string pattern = folder;
    const size_t slash = folder.find_last_of("\\/");

    if (slash == std::string::npos)
    {
        folder.clear();
    }
    else
    {
        pattern = folder.substr(slash + 1);
        folder.resize(slash);
    }

    std::vector<std::string> names;

    if (HasOverlay(gamePattern))
    {
        AddMatches(ResolveFrom(_UserRoot, folder), pattern, names);
    }

    AddMatches(ResolveFrom(_Root, folder), pattern, names);
    return names;
}

bool MCDiskFileSource::Exists(std::string_view gamePath)
{
    std::error_code error;
    return std::filesystem::is_regular_file(Resolve(gamePath), error);
}

bool MCDiskFileSource::RemoveFile(std::string_view gamePath)
{
    std::error_code error;
    return std::filesystem::remove(ResolveWrite(gamePath, false), error);
}

bool MCDiskFileSource::RenameFile(std::string_view fromGamePath, std::string_view toGamePath)
{
    std::error_code error;
    std::filesystem::rename(ResolveWrite(fromGamePath, true), ResolveWrite(toGamePath, false), error);
    return !error;
}

bool MCDiskFileSource::CopyFile(std::string_view fromGamePath, std::string_view toGamePath)
{
    std::error_code error;
    return std::filesystem::copy_file(Resolve(fromGamePath), ResolveWrite(toGamePath, false),
                                      std::filesystem::copy_options::overwrite_existing, error);
}

namespace MCFileSystem
{
    namespace
    {
        MCFileSource& Files()
        {
            return MCGameContext::Current().Files();
        }
    }

    /// <summary>Case-insensitive DOS wildcard match (<c>*</c> and <c>?</c>).</summary>
    bool WildcardMatch(const char* pattern, const char* name)
    {
        if (*pattern == '\0')
        {
            return *name == '\0';
        }

        if (*pattern == '*')
        {
            for (const char* rest = name;; ++rest)
            {
                if (WildcardMatch(pattern + 1, rest))
                {
                    return true;
                }

                if (*rest == '\0')
                {
                    return false;
                }
            }
        }

        if (*name == '\0')
        {
            return false;
        }

        if (*pattern != '?' &&
            std::tolower(static_cast<unsigned char>(*pattern)) != std::tolower(static_cast<unsigned char>(*name)))
        {
            return false;
        }

        return WildcardMatch(pattern + 1, name + 1);
    }

    void SetGameRoot(const std::filesystem::path& root)
    {
        Files().SetGameRoot(root);
    }

    const std::filesystem::path& GameRoot()
    {
        return Files().GameRoot();
    }

    void SetUserRoot(const std::filesystem::path& root)
    {
        Files().SetUserRoot(root);
    }

    const std::filesystem::path& UserRoot()
    {
        return Files().UserRoot();
    }

    std::filesystem::path Resolve(std::string_view gamePath)
    {
        return Files().Resolve(gamePath);
    }

    std::filesystem::path ResolveWrite(std::string_view gamePath, bool copyExisting)
    {
        return Files().ResolveWrite(gamePath, copyExisting);
    }

    bool MakeDirectory(std::string_view gamePath)
    {
        return Files().MakeDirectory(gamePath);
    }

    bool RemoveDirectory(std::string_view gamePath)
    {
        return Files().RemoveDirectory(gamePath);
    }

    std::vector<std::string> FindFiles(std::string_view gamePattern)
    {
        return Files().FindFiles(gamePattern);
    }

    std::optional<std::span<const uint8_t>> FindImage(std::string_view gamePath)
    {
        return Files().FindImage(gamePath);
    }

    bool Exists(std::string_view gamePath)
    {
        return Files().Exists(gamePath);
    }

    bool RemoveFile(std::string_view gamePath)
    {
        return Files().RemoveFile(gamePath);
    }

    bool RenameFile(std::string_view fromGamePath, std::string_view toGamePath)
    {
        return Files().RenameFile(fromGamePath, toGamePath);
    }

    bool CopyFile(std::string_view fromGamePath, std::string_view toGamePath)
    {
        return Files().CopyFile(fromGamePath, toGamePath);
    }
}
