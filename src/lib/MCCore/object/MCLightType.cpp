#include "stdafx.h"
#include "object/MCLightType.h"
#include "lib/MCFitIniFile.h"
#include "object/MCLight.h"

auto MCLightType::CreateInstance() -> std::unique_ptr<MCBaseObject>
{
    auto newLight = std::make_unique<MCLight>();

    if (newLight->Init(this) != 0)
    {
        return nullptr;
    }

    newLight->IdNumber = NextIdNumber++;
    return newLight;
}

auto MCLightType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile lightFile;

    if (const int32_t result = lightFile.Open(objFile, fileSize); result != 0)
    {
        return result;
    }

    if (const int32_t result = lightFile.SeekBlock("LightData"); result != 0)
    {
        return result;
    }

    MCFitReader read(lightFile);
    read.Value("OneShotFlag", OneShotFlag);
    read.Value("AltitudeOffset", AltitudeOffset);

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    return MCObjectType::Init(&lightFile);
}
