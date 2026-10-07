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
    fastFile->_Handle.reset(std::fopen(MCFileSystem::Resolve(fileName).string().c_str(), "rb"));

    if (fastFile->_Handle == nullptr)
    {
        return std::unexpected(std::format("{}: not found", fileName));
    }

    std::FILE* file = fastFile->_Handle.get();
    int32_t count = 0;

    if (std::fread(&count, sizeof(count), 1, file) != 1)
    {
        return std::unexpected(std::format("{}: no directory", fileName));
    }

    fastFile->_Entries.assign(static_cast<size_t>(std::max(count, 0)), MCFileEntry{});

    for (size_t i = 0; i < fastFile->_Entries.size(); ++i)
    {
        MCFileEntry& entry = fastFile->_Entries[i];
        // The original ignores the byte count; a short directory leaves the rest of the entry zeroed.
        std::fread(&entry, sizeof(MCFileEntry), 1, file);
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
    std::fseek(_Handle.get(), entry.Offset, SEEK_SET);

    if (entry.Size == entry.RealSize)
    {
        return static_cast<int32_t>(std::fread(data.data(), 1, data.size(), _Handle.get()));
    }

    // Packed: read the whole stored entry and unpack it.
    std::vector<uint8_t> packed(static_cast<size_t>(std::max(entry.Size, 0)));
    packed.resize(std::fread(packed.data(), 1, packed.size(), _Handle.get()));
    const int32_t unpacked = LZDecomp(data, packed);
    return unpacked == entry.RealSize ? unpacked : 0;
}
