#include "stdafx.h"
#include "MCRetailData.h"
#include "../MCTest.h"
#include "../TestGame.h"
#include "lib/file.h"

namespace MCRetailData
{
    std::unique_ptr<MCMemoryFileSource> Load(std::initializer_list<std::string_view> gamePaths)
    {
        if (!MCTestGame::Available())
        {
            std::cout << "  no install (--game): skipped\n";
            return nullptr;
        }

        MCTestGame::OpenFastFiles();
        auto source = std::make_unique<MCMemoryFileSource>();

        for (const std::string_view gamePath : gamePaths)
        {
            const std::string name(gamePath);
            File file;

            if (file.open(name.c_str()) != NO_ERR)
            {
                FAIL_CHECK(std::format("{} isn't in the install", name));
                continue;
            }

            std::vector<uint8_t> bytes(file.fileSize());

            if (!bytes.empty())
            {
                file.read(bytes.data(), static_cast<int32_t>(bytes.size()));
            }

            file.close();
            source->AddFile(gamePath, std::move(bytes));
        }

        return source;
    }
}
