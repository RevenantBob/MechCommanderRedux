#include "stdafx.h"
#include "lib/MCFastFileSet.h"

std::expected<void, std::string> MCFastFileSet::Open(std::string_view fileName)
{
    auto fastFile = MCFastFile::Create(fileName);

    if (!fastFile.has_value())
    {
        return std::unexpected(std::move(fastFile.error()));
    }

    _Files.push_back(std::move(*fastFile));
    return {};
}

std::optional<std::vector<uint8_t>> MCFastFileSet::Read(std::string_view gamePath)
{
    for (const std::unique_ptr<MCFastFile>& fastFile : _Files)
    {
        if (const std::optional<int32_t> index = fastFile->Find(gamePath); index.has_value())
        {
            std::vector<uint8_t> data(static_cast<size_t>(std::max(fastFile->GetEntry(*index)->RealSize, 0)));
            fastFile->ReadEntry(*index, data);
            return data;
        }
    }

    return std::nullopt;
}
