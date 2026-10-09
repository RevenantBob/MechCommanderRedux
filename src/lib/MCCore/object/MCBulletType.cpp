#include "stdafx.h"
#include "object/MCBulletType.h"
#include "lib/MCFitIniFile.h"
#include "object/MCBullet.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectTypeManager.h"

auto MCBulletType::CreateInstance() -> std::unique_ptr<MCBaseObject>
{
    auto newBullet = std::make_unique<MCBullet>();

    if (newBullet->Init(this) != 0)
    {
        return nullptr;
    }

    newBullet->IdNumber = NextIdNumber++;
    return newBullet;
}

auto MCBulletType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile bulletFile;

    if (const int32_t result = bulletFile.Open(objFile, fileSize); result != 0)
    {
        return result;
    }

    if (bulletFile.SeekBlock("BulletData") == 0)
    {
        MCFitReader read(bulletFile);
        read.Value("SoundEffectId", SoundEffectId);
        read.Value("BulletHitEffect", BulletHitEffect);
        read.Value("BulletMissEffect", BulletMissEffect);
        read.Value("Velocity", Velocity);
        read.Value("CloseDistance", CloseDistance);
        read.Value("SmokeObjectId", SmokeObjectId);

        if (read.Failed())
        {
            return std::to_underlying(read.Error());
        }

        LightObjectId = bulletFile.Read<uint32_t>("LightObjectId").value_or(0xffffffff);
    }

    const int32_t result = MCObjectType::Init(&bulletFile);
    ObjectTypeManager()->Load(static_cast<int32_t>(BulletHitEffect), 1);
    ObjectTypeManager()->Load(static_cast<int32_t>(BulletMissEffect), 1);
    ObjectTypeManager()->Load(static_cast<int32_t>(SmokeObjectId), 1);
    return result;
}
