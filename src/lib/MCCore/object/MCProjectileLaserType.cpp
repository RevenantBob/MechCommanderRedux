#include "stdafx.h"
#include "object/MCProjectileLaserType.h"
#include "lib/MCFitIniFile.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectTypeManager.h"
#include "object/MCProjectileLaser.h"

auto MCProjectileLaserType::CreateInstance() -> std::unique_ptr<MCBaseObject>
{
    auto newLaser = std::make_unique<MCProjectileLaser>();

    if (newLaser->Init(this) != 0)
    {
        return nullptr;
    }

    newLaser->IdNumber = NextIdNumber++;
    return newLaser;
}

auto MCProjectileLaserType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile laserFile;

    if (const int32_t result = laserFile.Open(objFile, fileSize); result != 0)
    {
        return result;
    }

    if (laserFile.SeekBlock("ProjectileLaserData") == 0)
    {
        MCFitReader read(laserFile);
        read.Value("SoundEffectId", SoundEffectId);
        read.Value("ProjectileHitEffect", ProjectileHitEffect);
        read.Value("ProjectileMissEffect", ProjectileMissEffect);
        read.Value("Velocity", Velocity);
        read.Value("CloseDistance", CloseDistance);
        read.Value("ProjectileLength", ProjectileLength);
        read.Value("BulgeLength", BulgeLength);
        read.Value("BulgeWidth", BulgeWidth);

        for (size_t i = 0; i < EColor.size(); i++)
        {
            read.Value(std::format("e{}Color", i), EColor[i]);
        }

        for (size_t i = 0; i < FColor.size(); i++)
        {
            read.Value(std::format("f{}Color", i), FColor[i]);
        }

        if (read.Failed())
        {
            return std::to_underlying(read.Error());
        }

        SmokeObjectId = laserFile.Read<uint32_t>("SmokeObjectId").value_or(0xffffffff);
        LightObjectId = laserFile.Read<uint32_t>("LightObjectId").value_or(0xffffffff);
    }

    const int32_t result = MCObjectType::Init(&laserFile);
    ObjectTypeManager()->Load(static_cast<int32_t>(ProjectileHitEffect), 1);
    ObjectTypeManager()->Load(static_cast<int32_t>(ProjectileMissEffect), 1);
    return result;
}
