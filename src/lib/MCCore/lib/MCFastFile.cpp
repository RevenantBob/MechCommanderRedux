#include "stdafx.h"
#include "lib/MCFastFile.h"
#include "lib/MCLz.h"
#include "platform/MCFileSystem.h"

namespace
{
    /// <summary>The name in the case the index keys use.</summary>
    std::string IndexKey(std::string_view name)
    {
        std::string key(name);

        for (char& c : key)
        {
            if (c >= 'a' && c <= 'z')
            {
                c = static_cast<char>(c - 'a' + 'A');
            }
        }

        return key;
    }
}

std::expected<std::unique_ptr<MCFastFile>, std::string> MCFastFile::Create(std::string_view fileName)
{
    auto fastFile = std::make_unique<MCFastFile>(Key());
    fastFile->_Handle = MCDiskFile::Open(MCFileSystem::Resolve(fileName), MCDiskFileMode::Read);

    if (!fastFile->_Handle.has_value())
    {
        return std::unexpected(std::format("{}: not found", fileName));
    }

    MCDiskFile& file = *fastFile->_Handle;
    std::array<uint8_t, sizeof(int32_t)> countBytes{};

    if (file.Read(countBytes) != countBytes.size())
    {
        return std::unexpected(std::format("{}: no directory", fileName));
    }

    fastFile->_Entries.assign(static_cast<size_t>(std::max(std::bit_cast<int32_t>(countBytes), 0)), MCFileEntry{});

    for (size_t i = 0; i < fastFile->_Entries.size(); ++i)
    {
        // The original ignores the byte count; a short directory leaves the rest of the entry zeroed.
        std::array<uint8_t, sizeof(MCFileEntry)> entryBytes{};
        file.Read(entryBytes);
        MCFileEntry& entry = fastFile->_Entries[i];
        entry = std::bit_cast<MCFileEntry>(entryBytes);
        entry.Name.back() = 0;
        fastFile->_Index.try_emplace(IndexKey(entry.GetName()), static_cast<int32_t>(i));
    }

    return fastFile;
}

std::optional<int32_t> MCFastFile::Find(std::string_view gamePath) const
{
    if (const auto found = _Index.find(IndexKey(gamePath)); found != _Index.end())
    {
        return found->second;
    }

    return std::nullopt;
}

int32_t MCFastFile::ReadEntry(int32_t index, std::span<uint8_t> data)
{
    const MCFileEntry& entry = _Entries.at(static_cast<size_t>(index));
    data = data.first(std::min(data.size(), static_cast<size_t>(std::max(entry.RealSize, 0))));
    const auto offset = static_cast<uint32_t>(entry.Offset);

    if (entry.Size == entry.RealSize)
    {
        return static_cast<int32_t>(_Handle->ReadAt(offset, data));
    }

    // Packed: read the whole stored entry and unpack it.
    std::vector<uint8_t> packed(static_cast<size_t>(std::max(entry.Size, 0)));
    packed.resize(_Handle->ReadAt(offset, packed));
    const int32_t unpacked = LZDecomp(data, packed);
    return unpacked == entry.RealSize ? unpacked : 0;
}
