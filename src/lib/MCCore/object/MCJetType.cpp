#include "stdafx.h"
#include "object/MCJetType.h"
#include "lib/MCFitIniFile.h"
#include "object/MCJet.h"

auto MCJetType::CreateInstance() -> std::unique_ptr<MCBaseObject>
{
    auto newJet = std::make_unique<MCJet>();

    if (newJet->Init(this) != 0)
    {
        return nullptr;
    }

    newJet->IdNumber = NextIdNumber++;
    return newJet;
}

auto MCJetType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile jetFile;

    if (const int32_t result = jetFile.Open(objFile, fileSize); result != 0)
    {
        return result;
    }

    if (jetFile.SeekBlock("JetData") == 0)
    {
        MCFitReader read(jetFile);
        read.Value("SoundEffectId", SoundEffectId);
        read.Value("SmokeObjectId", SmokeObjectId);

        if (read.Failed())
        {
            return std::to_underlying(read.Error());
        }
    }

    return MCObjectType::Init(&jetFile);
}
