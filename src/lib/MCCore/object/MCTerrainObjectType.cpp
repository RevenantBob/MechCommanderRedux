#include "stdafx.h"
#include "object/MCTerrainObjectType.h"
#include "lib/MCFitIniFile.h"
#include "network/MCMultiPlayer.h"
#include "object/MCTerrainObject.h"
#include "object/MCWeaponShotInfo.h"

auto MCTerrainObjectType::CreateInstance() -> std::unique_ptr<MCBaseObject>
{
    auto newObject = std::make_unique<MCTerrainObject>();

    if (newObject->Init(this) != 0)
    {
        return nullptr;
    }

    newObject->IdNumber = NextIdNumber++;
    return newObject;
}

auto MCTerrainObjectType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile objectFile;

    if (const int32_t result = objectFile.Open(objFile, fileSize); result != 0)
    {
        return result;
    }

    if (const int32_t result = objectFile.SeekBlock("TerrainObjectData"); result != 0)
    {
        return result;
    }

    MCFitReader read(objectFile);
    read.Value("DmgLevel", DmgLevel);

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    BasePixelOffsetX = objectFile.Read<int32_t>("BasePixelOffsetX").value_or(0);
    BasePixelOffsetY = objectFile.Read<int32_t>("BasePixelOffsetY").value_or(0);
    CollisionOffsetX = objectFile.Read<int32_t>("CollisionOffsetX").value_or(0);
    CollisionOffsetY = objectFile.Read<int32_t>("CollisionOffsetY").value_or(0);
    SetImpassable = objectFile.Read<int32_t>("SetImpassable").value_or(0);
    XImpasse = objectFile.Read<int32_t>("XImpasse").value_or(0);
    YImpasse = objectFile.Read<int32_t>("YImpasse").value_or(0);
    // No ExtentRadius: -1, measured from the appearance by the first object's update.
    const float radius = objectFile.Read<float>("ExtentRadius").value_or(-1.0f);
    ExplRad = objectFile.Read<float>("ExplosionRadius").value_or(0.0f);
    ExplDmg = objectFile.Read<float>("ExplosionDamage").value_or(0.0f);
    const int32_t result = MCObjectType::Init(&objectFile);
    ExtentRadius = radius;
    return result;
}

auto MCTerrainObjectType::HandleCollision(MCGameObject* collidee, MCGameObject* collider) -> int
{
    // A mover (not artillery) running into it deals it 10 points; the server's job in multiplayer.
    if ((MultiPlayer() == nullptr || MultiPlayer()->IsServer != 0) && collider->ObjectClass < MCObjectClass::Mover &&
        collider->ObjectClass != MCObjectClass::Artillery)
    {
        MCWeaponShotInfo shot;
        shot.Init(nullptr, -1, 10.0f, 0, 0.0f);
        collidee->HandleWeaponHit(&shot, MultiPlayer() != nullptr ? 1 : 0);
    }

    return 1;
}
