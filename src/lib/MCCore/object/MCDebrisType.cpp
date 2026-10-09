#include "stdafx.h"
#include "object/MCDebrisType.h"
#include "lib/MCFitIniFile.h"
#include "object/MCDebris.h"

auto MCDebrisType::CreateInstance() -> std::unique_ptr<MCBaseObject>
{
    auto newDebris = std::make_unique<MCDebris>();

    if (newDebris->Init(this) != 0)
    {
        return nullptr;
    }

    newDebris->IdNumber = NextIdNumber++;
    return newDebris;
}

auto MCDebrisType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile debrisFile;

    if (const int32_t result = debrisFile.Open(objFile, fileSize); result != 0)
    {
        return result;
    }

    if (const int32_t result = debrisFile.SeekBlock("ArmFall"); result != 0)
    {
        return result;
    }

    MCFitReader read(debrisFile);
    read.Value("ArmFallYaw", ArmFallYaw);
    read.Value("ArmFallYawRange", ArmFallYawRange);
    read.Value("ArmFallVelMag", ArmFallVelMag);
    read.Value("ArmFallVelRange", ArmFallVelRange);
    read.Value("ArmFallDecelRate", ArmFallDecelRate);

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    return MCObjectType::Init(&debrisFile);
}
