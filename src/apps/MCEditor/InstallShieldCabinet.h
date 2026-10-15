#pragma once

// The port's own: reads an InstallShield 5 cabinet, the DATA1.HDR + DATA1.CAB pair the Gold CD's editor setup
// (MCEDSETP\DISK1) packs the Mission Editor's files in. The header lists the files; the cabinet holds each one stored
// or compressed as raw deflate in chunks, each with a 16-bit length before it. Only what that setup uses is read: one
// volume, no split or obfuscated files.

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// <summary>An InstallShield 5 cabinet's file list, and its files' contents.</summary>
class InstallShieldCabinet
{
public:
    /// <summary>One file in the cabinet.</summary>
    struct File
    {
        /// <summary>Its name, without a folder.</summary>
        std::string Name;
        /// <summary>The setup's folder for it (empty in the editor's cabinet: the setup script places each file).</summary>
        std::string Folder;
        /// <summary>Its size once extracted.</summary>
        uint32_t ExpandedSize = 0;
        /// <summary>Its size in the cabinet.</summary>
        uint32_t CompressedSize = 0;
        /// <summary>Where it starts in the cabinet.</summary>
        uint32_t DataOffset = 0;
        /// <summary>The FILE_* flags.</summary>
        uint16_t Flags = 0;
        /// <summary>The MD5 of its extracted contents.</summary>
        std::array<uint8_t, 16> Md5 = {};
    };

    /// <summary>
    /// Reads the file list of the cabinet <paramref name="headerPath"/> (DATA1.HDR) whose data is in
    /// <paramref name="cabinetPath"/> (DATA1.CAB).
    /// </summary>
    static std::expected<InstallShieldCabinet, std::string> Open(const std::filesystem::path& headerPath,
                                                                 const std::filesystem::path& cabinetPath);

    /// <summary>The files, in the header's order; files the header marks invalid are left out.</summary>
    const std::vector<File>& Files() const;

    /// <summary>The file named <paramref name="name"/> (case-insensitive), if the cabinet has it.</summary>
    std::optional<File> Find(std::string_view name) const;

    /// <summary>The contents of <paramref name="file"/>, checked against its size and MD5.</summary>
    std::expected<std::vector<std::byte>, std::string> Extract(const File& file) const;

private:
    /// <summary>The cabinet file.</summary>
    std::filesystem::path _cabinetPath;
    /// <summary>The files.</summary>
    std::vector<File> _files;
};
