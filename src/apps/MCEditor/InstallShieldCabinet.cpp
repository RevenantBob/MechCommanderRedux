#include "stdafx.h"
#include "InstallShieldCabinet.h"

#include <bcrypt.h>
#include <format>
#include <fstream>
#include <span>

#include "miniz.h"

#pragma comment(lib, "bcrypt.lib")

namespace
{
    /// <summary>"ISc(", the header's signature.</summary>
    inline constexpr uint32_t SIGNATURE = 0x28635349;
    /// <summary>The only format version read.</summary>
    inline constexpr uint32_t SUPPORTED_MAJOR_VERSION = 5;

    /// <summary>The common header's fields: the version, and where the cabinet descriptor is.</summary>
    inline constexpr size_t VERSION_OFFSET = 0x04;
    inline constexpr size_t DESCRIPTOR_OFFSET = 0x0c;

    /// <summary>The cabinet descriptor's fields: the file table's offset (from the descriptor) and its counts.</summary>
    inline constexpr size_t FILE_TABLE_OFFSET = 0x0c;
    inline constexpr size_t DIRECTORY_COUNT = 0x1c;
    inline constexpr size_t FILE_COUNT = 0x28;

    /// <summary>A version 5 file descriptor's fields.</summary>
    inline constexpr size_t FILE_NAME_OFFSET = 0x00;
    inline constexpr size_t FILE_DIRECTORY = 0x04;
    inline constexpr size_t FILE_FLAGS = 0x08;
    inline constexpr size_t FILE_EXPANDED_SIZE = 0x0a;
    inline constexpr size_t FILE_COMPRESSED_SIZE = 0x0e;
    inline constexpr size_t FILE_DATA_OFFSET = 0x26;
    inline constexpr size_t FILE_MD5 = 0x2a;
    inline constexpr size_t FILE_DESCRIPTOR_SIZE = 0x3a;

    /// <summary>The descriptor's flags.</summary>
    inline constexpr uint16_t FILE_SPLIT = 0x1;
    inline constexpr uint16_t FILE_OBFUSCATED = 0x2;
    inline constexpr uint16_t FILE_COMPRESSED = 0x4;
    inline constexpr uint16_t FILE_INVALID = 0x8;

    /// <summary>The header's bytes, read with bounds checks.</summary>
    class HeaderReader
    {
    public:
        explicit HeaderReader(std::span<const std::byte> bytes) : _bytes(bytes) {}

        /// <summary>The little-endian 16-bit value at <paramref name="offset"/>.</summary>
        std::expected<uint16_t, std::string> U16(size_t offset) const
        {
            if (offset + 2 > _bytes.size())
            {
                return std::unexpected(outside(offset));
            }

            return static_cast<uint16_t>(std::to_integer<uint16_t>(_bytes[offset]) |
                                         std::to_integer<uint16_t>(_bytes[offset + 1]) << 8);
        }

        /// <summary>The little-endian 32-bit value at <paramref name="offset"/>.</summary>
        std::expected<uint32_t, std::string> U32(size_t offset) const
        {
            if (offset + 4 > _bytes.size())
            {
                return std::unexpected(outside(offset));
            }

            uint32_t value = 0;

            for (size_t i = 0; i < 4; i++)
            {
                value |= std::to_integer<uint32_t>(_bytes[offset + i]) << (8 * i);
            }

            return value;
        }

        /// <summary>The zero-ended string at <paramref name="offset"/>.</summary>
        std::expected<std::string, std::string> String(size_t offset) const
        {
            std::string text;

            for (size_t i = offset; i < _bytes.size(); i++)
            {
                if (_bytes[i] == std::byte{0})
                {
                    return text;
                }

                text += static_cast<char>(_bytes[i]);
            }

            return std::unexpected(outside(offset));
        }

        /// <summary>The <paramref name="count"/> bytes at <paramref name="offset"/>.</summary>
        std::expected<std::span<const std::byte>, std::string> Bytes(size_t offset, size_t count) const
        {
            if (offset + count > _bytes.size())
            {
                return std::unexpected(outside(offset));
            }

            return _bytes.subspan(offset, count);
        }

    private:
        static std::string outside(size_t offset) { return std::format("the header ends before offset {:#x}", offset); }

        std::span<const std::byte> _bytes;
    };

    /// <summary>The whole of <paramref name="path"/>.</summary>
    std::expected<std::vector<std::byte>, std::string> readFile(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);

        if (!stream)
        {
            return std::unexpected(std::format("can't open {}", path.string()));
        }

        std::vector<std::byte> bytes(static_cast<size_t>(stream.tellg()));
        stream.seekg(0);

        if (!stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        {
            return std::unexpected(std::format("can't read {}", path.string()));
        }

        return bytes;
    }

    /// <summary>The MD5 of <paramref name="bytes"/>.</summary>
    std::expected<std::array<uint8_t, 16>, std::string> md5(std::span<const std::byte> bytes)
    {
        std::array<uint8_t, 16> digest = {};
        // BCryptHash takes a non-const input pointer but only reads it.
        const NTSTATUS status = BCryptHash(
            BCRYPT_MD5_ALG_HANDLE, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<std::byte*>(bytes.data())),
            static_cast<ULONG>(bytes.size()), digest.data(), static_cast<ULONG>(digest.size()));

        if (!BCRYPT_SUCCESS(status))
        {
            return std::unexpected(std::format("MD5 failed ({:#x})", static_cast<uint32_t>(status)));
        }

        return digest;
    }

    /// <summary>Whether <paramref name="a"/> and <paramref name="b"/> are the same, ignoring ASCII case.</summary>
    bool equalNoCase(std::string_view a, std::string_view b)
    {
        return std::ranges::equal(
            a, b, [](char x, char y)
            { return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y)); });
    }
}

std::expected<InstallShieldCabinet, std::string> InstallShieldCabinet::Open(const std::filesystem::path& headerPath,
                                                                            const std::filesystem::path& cabinetPath)
{
    const auto bytes = readFile(headerPath);

    if (!bytes)
    {
        return std::unexpected(bytes.error());
    }

    const HeaderReader header(*bytes);
    const auto signature = header.U32(0);
    const auto version = header.U32(VERSION_OFFSET);
    const auto descriptor = header.U32(DESCRIPTOR_OFFSET);

    if (!signature || *signature != SIGNATURE || !version || !descriptor)
    {
        return std::unexpected(std::format("{} isn't an InstallShield cabinet header", headerPath.string()));
    }

    // Versions written as 0x01xxxxxx keep the major version in bits 12-15; 5 has always been written that way.
    const uint32_t major = (*version >> 24) == 1 ? (*version >> 12) & 0xf : 0;

    if (major != SUPPORTED_MAJOR_VERSION)
    {
        return std::unexpected(std::format("{} is InstallShield cabinet version {:#x}; only version 5 is read",
                                           headerPath.string(), *version));
    }

    const auto tableOffset = header.U32(*descriptor + FILE_TABLE_OFFSET);
    const auto directoryCount = header.U32(*descriptor + DIRECTORY_COUNT);
    const auto fileCount = header.U32(*descriptor + FILE_COUNT);

    if (!tableOffset || !directoryCount || !fileCount)
    {
        return std::unexpected(std::format("{}: the cabinet descriptor is cut short", headerPath.string()));
    }

    // The table holds an offset (from the table) for each folder name, then for each file descriptor.
    const size_t table = static_cast<size_t>(*descriptor) + *tableOffset;
    std::vector<std::string> folders;

    for (uint32_t i = 0; i < *directoryCount; i++)
    {
        const auto name = header.U32(table + 4 * static_cast<size_t>(i))
                              .and_then([&](uint32_t entry) { return header.String(table + entry); });

        if (!name)
        {
            return std::unexpected(std::format("{}: {}", headerPath.string(), name.error()));
        }

        folders.push_back(*name);
    }

    InstallShieldCabinet cabinet;
    cabinet._cabinetPath = cabinetPath;

    for (uint32_t i = 0; i < *fileCount; i++)
    {
        const auto entry = header.U32(table + 4 * (static_cast<size_t>(*directoryCount) + i));

        if (!entry)
        {
            return std::unexpected(std::format("{}: {}", headerPath.string(), entry.error()));
        }

        const size_t at = table + *entry;
        const auto nameOffset = header.U32(at + FILE_NAME_OFFSET);
        const auto folder = header.U32(at + FILE_DIRECTORY);
        const auto flags = header.U16(at + FILE_FLAGS);
        const auto expanded = header.U32(at + FILE_EXPANDED_SIZE);
        const auto compressed = header.U32(at + FILE_COMPRESSED_SIZE);
        const auto dataOffset = header.U32(at + FILE_DATA_OFFSET);
        const auto digest = header.Bytes(at + FILE_MD5, FILE_DESCRIPTOR_SIZE - FILE_MD5);
        const auto name = nameOffset.and_then([&](uint32_t offset) { return header.String(table + offset); });

        if (!folder || !flags || !expanded || !compressed || !dataOffset || !digest || !name ||
            *folder >= folders.size())
        {
            return std::unexpected(std::format("{}: file {} has a bad descriptor", headerPath.string(), i));
        }

        if ((*flags & FILE_INVALID) != 0)
        {
            continue;
        }

        File file;
        file.Name = *name;
        file.Folder = folders[*folder];
        file.ExpandedSize = *expanded;
        file.CompressedSize = *compressed;
        file.DataOffset = *dataOffset;
        file.Flags = *flags;
        std::ranges::transform(*digest, file.Md5.begin(), [](std::byte b) { return std::to_integer<uint8_t>(b); });
        cabinet._files.push_back(std::move(file));
    }

    return cabinet;
}

const std::vector<InstallShieldCabinet::File>& InstallShieldCabinet::Files() const
{
    return _files;
}

std::optional<InstallShieldCabinet::File> InstallShieldCabinet::Find(std::string_view name) const
{
    const auto found = std::ranges::find_if(_files, [&](const File& file) { return equalNoCase(file.Name, name); });

    if (found == _files.end())
    {
        return std::nullopt;
    }

    return *found;
}

std::expected<std::vector<std::byte>, std::string> InstallShieldCabinet::Extract(const File& file) const
{
    if ((file.Flags & (FILE_SPLIT | FILE_OBFUSCATED)) != 0)
    {
        return std::unexpected(
            std::format("{} is split across volumes or obfuscated, which isn't supported", file.Name));
    }

    std::ifstream stream(_cabinetPath, std::ios::binary);
    std::vector<std::byte> stored(file.CompressedSize);

    if (!stream || !stream.seekg(file.DataOffset) ||
        !stream.read(reinterpret_cast<char*>(stored.data()), static_cast<std::streamsize>(stored.size())))
    {
        return std::unexpected(std::format("can't read {} from {}", file.Name, _cabinetPath.string()));
    }

    std::vector<std::byte> contents;

    if ((file.Flags & FILE_COMPRESSED) == 0)
    {
        contents = std::move(stored);
    }
    else
    {
        // Each chunk is its own raw deflate stream: a 16-bit length, then that many bytes.
        contents.resize(file.ExpandedSize);
        size_t read = 0;
        size_t written = 0;

        while (read < stored.size())
        {
            if (read + 2 > stored.size())
            {
                return std::unexpected(std::format("{}: a chunk length is cut short", file.Name));
            }

            const size_t length = std::to_integer<size_t>(stored[read]) | std::to_integer<size_t>(stored[read + 1])
                                                                              << 8;
            read += 2;

            if (read + length > stored.size())
            {
                return std::unexpected(std::format("{}: a chunk runs past the file's data", file.Name));
            }

            const size_t produced = tinfl_decompress_mem_to_mem(contents.data() + written, contents.size() - written,
                                                                stored.data() + read, length, 0);

            if (produced == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED)
            {
                return std::unexpected(std::format("{}: the compressed data is damaged", file.Name));
            }

            read += length;
            written += produced;
        }

        contents.resize(written);
    }

    if (contents.size() != file.ExpandedSize)
    {
        return std::unexpected(
            std::format("{}: {} bytes extracted, {} expected", file.Name, contents.size(), file.ExpandedSize));
    }

    const auto digest = md5(contents);

    if (!digest)
    {
        return std::unexpected(std::format("{}: {}", file.Name, digest.error()));
    }

    if (*digest != file.Md5)
    {
        return std::unexpected(std::format("{}: the extracted file doesn't match its checksum", file.Name));
    }

    return contents;
}
