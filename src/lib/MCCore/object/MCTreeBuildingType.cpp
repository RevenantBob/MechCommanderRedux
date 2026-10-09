#include "stdafx.h"
#include "object/MCTreeBuildingType.h"
#include "lib/MCFitIniFile.h"
#include "object/MCTreeBuilding.h"
#include "object/MCTreeType.h"

auto MCTreeBuildingType::CreateInstance() -> std::unique_ptr<MCBaseObject>
{
    auto newBuilding = std::make_unique<MCTreeBuilding>();

    if (newBuilding->Init(this) != 0)
    {
        return nullptr;
    }

    newBuilding->IdNumber = NextIdNumber++;
    return newBuilding;
}

auto MCTreeBuildingType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile treeFile;

    if (const int32_t result = treeFile.Open(objFile, fileSize); result != 0)
    {
        return result;
    }

    if (const int32_t result = treeFile.SeekBlock("TreeData"); result != 0)
    {
        return result;
    }

    MCFitReader read(treeFile);
    read.Value("DmgLevel", DmgLevel);

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    ReadEffectId(treeFile, "NormalEffectId", NormalEffectId);
    ReadEffectId(treeFile, "BlownEffectId", BlownEffectId);
    ReadEffectId(treeFile, "DamageEffectId", DamageEffectId);
    CanRefit = treeFile.Read<bool>("CanRefit").value_or(false);

    if (CanRefit)
    {
        MechBay = treeFile.Read<bool>("MechBay").value_or(false);
    }

    if (const int32_t result = LoadShadowShape(treeFile, "NormalShadow", NormalShadow); result != 0)
    {
        return result;
    }

    if (const int32_t result = LoadShadowShape(treeFile, "DestroyedShadow", DestroyedShadow); result != 0)
    {
        return result;
    }

    ExplRad = treeFile.Read<float>("ExplosionRadius").value_or(0.0f);
    ExplDmg = treeFile.Read<float>("ExplosionDamage").value_or(0.0f);
    TimeToBurnDamage = treeFile.Read<float>("TimeToBurnDamage").value_or(5.0f);
    BurnDamagePerTime = treeFile.Read<float>("BurnDamagePerTime").value_or(1.0f);
    DamageLvlForBurn = treeFile.Read<float>("DamageLvlForBurn").value_or(static_cast<float>(DmgLevel));

    // The team is only read for a building with a sensor.
    if (const MCFitResult<float> range = treeFile.Read<float>("SensorRange"); range.has_value())
    {
        SensorRange = *range;
        TeamId = treeFile.Read<int32_t>("TeamID").value_or(-1);
    }
    else
    {
        SensorRange = -1.0f;
    }

    BaseTonnage = treeFile.Read<float>("Tonnage").value_or(20.0f);
    BattleRating = treeFile.Read<int32_t>("BattleRating").value_or(20);
    NumMarines = treeFile.Read<int32_t>("NumMarines").value_or(0);
    BuildingName = treeFile.Read<int32_t>("BuildingName").value_or(0xa3);
    return MCObjectType::Init(&treeFile);
}
