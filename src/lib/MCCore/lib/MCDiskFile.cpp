#include "stdafx.h"
#include "lib/MCDiskFile.h"

std::optional<MCDiskFile> MCDiskFile::Open(const std::filesystem::path& path, MCDiskFileMode mode)
{
    MCDiskFile file;
    file._Handle.reset(std::fopen(path.string().c_str(), mode == MCDiskFileMode::Create ? "w+b" : "rb"));

    if (file._Handle == nullptr)
    {
        return std::nullopt;
    }

    return file;
}

size_t MCDiskFile::Read(std::span<uint8_t> buffer)
{
    return std::fread(buffer.data(), 1, buffer.size(), _Handle.get());
}

size_t MCDiskFile::ReadAt(uint32_t offset, std::span<uint8_t> buffer)
{
    std::fseek(_Handle.get(), static_cast<long>(offset), SEEK_SET);
    return Read(buffer);
}

size_t MCDiskFile::Write(std::span<const uint8_t> data)
{
    return std::fwrite(data.data(), 1, data.size(), _Handle.get());
}

size_t MCDiskFile::WriteAt(uint32_t offset, std::span<const uint8_t> data)
{
    std::fseek(_Handle.get(), static_cast<long>(offset), SEEK_SET);
    return Write(data);
}

uint32_t MCDiskFile::Length()
{
    const long here = std::ftell(_Handle.get());
    std::fseek(_Handle.get(), 0, SEEK_END);
    const auto length = static_cast<uint32_t>(std::ftell(_Handle.get()));
    std::fseek(_Handle.get(), here, SEEK_SET);
    return length;
}
