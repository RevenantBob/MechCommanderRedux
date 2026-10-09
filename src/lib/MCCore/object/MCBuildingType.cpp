#include "stdafx.h"
#include "object/MCBuildingType.h"
#include "lib/MCFitIniFile.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/MCBuilding.h"
#include "object/MCWeaponShotInfo.h"

auto MCBuildingType::CreateInstance() -> std::unique_ptr<MCBaseObject>
{
    auto newBuilding = std::make_unique<MCBuilding>();

    if (newBuilding->Init(this) != 0)
    {
        return nullptr;
    }

    newBuilding->IdNumber = NextIdNumber++;
    return newBuilding;
}

auto MCBuildingType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile bldgFile;

    if (const int32_t result = bldgFile.Open(objFile, fileSize); result != 0)
    {
        return result;
    }

    if (const int32_t result = bldgFile.SeekBlock("BuildingData"); result != 0)
    {
        return result;
    }

    MCFitReader read(bldgFile);
    read.Value("DmgLevel", DmgLevel);
    read.Value("BlownEffectId", BlownEffectId);
    read.Value("DamageEffectId", DamageEffectId);

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    NormalEffectId = bldgFile.Read<uint32_t>("NormalEffectId").value_or(0xffffffff);
    BasePixelOffsetX = bldgFile.Read<int32_t>("BasePixelOffsetX").value_or(0);
    BasePixelOffsetY = bldgFile.Read<int32_t>("BasePixelOffsetY").value_or(0);
    CollisionOffsetX = bldgFile.Read<int32_t>("CollisionOffsetX").value_or(0);
    CollisionOffsetY = bldgFile.Read<int32_t>("CollisionOffsetY").value_or(0);
    // -1 has the first building's update measure the extent from its appearance.
    const float fitExtentRadius = bldgFile.Read<float>("ExtentRadius").value_or(-1.0f);
    BaseTonnage = bldgFile.Read<float>("Tonnage").value_or(20.0f);
    BattleRating = bldgFile.Read<int32_t>("BattleRating").value_or(20);
    NumMarines = bldgFile.Read<int32_t>("NumMarines").value_or(0);
    ExplRad = bldgFile.Read<float>("ExplosionRadius").value_or(0.0f);
    ExplDmg = bldgFile.Read<float>("ExplosionDamage").value_or(0.0f);
    TimeToBurnDamage = bldgFile.Read<float>("TimeToBurnDamage").value_or(5.0f);
    BurnDamagePerTime = bldgFile.Read<float>("BurnDamagePerTime").value_or(1.0f);
    DamageLvlForBurn = bldgFile.Read<float>("DamageLvlForBurn").value_or(static_cast<float>(DmgLevel));
    // "PotentialContact" is in the files, and the original read it into nothing.
    TeamId = bldgFile.Read<int32_t>("TeamID").value_or(-1);
    SensorRange = bldgFile.Read<float>("SensorRange").value_or(-1.0f);
    BuildingName = bldgFile.Read<int32_t>("BuildingName").value_or(0xa3);
    const int32_t result = MCObjectType::Init(&bldgFile);
    ExtentRadius = fitExtentRadius;
    return result;
}

auto MCBuildingType::HandleCollision(MCGameObject* collidee, MCGameObject* collider) -> int
{
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        return 1;
    }

    // Movers (not artillery) that run into it do 10 points of damage.
    if (collider->ObjectClass < MCObjectClass::Mover && collider->ObjectClass != MCObjectClass::Artillery)
    {
        MCWeaponShotInfo shot;
        shot.Init(nullptr, -1, 10.0f, 0, 0.0f);

        if (ScenarioTime <= collider->GetCollisionFreeTime())
        {
            collidee->HandleWeaponHit(&shot, MPlayer != nullptr ? 1 : 0);
        }
    }

    return 1;
}
