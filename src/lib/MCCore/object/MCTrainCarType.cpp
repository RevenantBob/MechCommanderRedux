#include "stdafx.h"
#include "object/MCTrainCarType.h"
#include "lib/MCFitIniFile.h"
#include "network/multplyr.h"
#include "object/MCTrain.h"
#include "object/MCTrainCar.h"
#include "object/MCWeaponShotInfo.h"

auto MCTrainCarType::CreateInstance() -> std::unique_ptr<MCBaseObject>
{
    auto newCar = std::make_unique<MCTrainCar>();

    if (newCar->Init(this) != 0)
    {
        return nullptr;
    }

    newCar->IdNumber = NextIdNumber++;
    return newCar;
}

auto MCTrainCarType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile trainFile;

    if (const int32_t result = trainFile.Open(objFile, fileSize); result != 0)
    {
        return result;
    }

    if (const int32_t result = trainFile.SeekBlock("Train"); result != 0)
    {
        return result;
    }

    MCFitReader read(trainFile);
    read.Value("Name", NameId);
    read.Value("Explosion Chance", ExplosionChance);
    read.Value("Explosion Damage", ExplosionDamage);
    read.Value("Velocity Multiplier", VelocityMultiplier);
    read.Value("Acceleration", Acceleration);
    read.Value("Deceleration", Deceleration);
    read.Value("TopSpeed", TopSpeed);
    read.Value("Damage", Damage);
    read.Value("TonnageClass", TonnageClass);

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    return MCObjectType::Init(&trainFile);
}

auto MCTrainCarType::HandleCollision(MCGameObject* collidee, MCGameObject* collider) -> int
{
    // The server's job in multiplayer.
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        return 0;
    }

    auto* car = static_cast<MCTrainCar*>(collidee);
    MCTrain* train = car->Train;
    const int multiplayer = MPlayer != nullptr ? 1 : 0;
    // The car takes (collider tonnage + 1) / 2, from the collider's side.
    const auto hitCar = [&](int32_t hitLocation)
    {
        const auto angle = static_cast<float>(car->RelFacingTo(collider->GetPosition(), -1));
        MCWeaponShotInfo shot;
        shot.Init(collider, -1, static_cast<float>((collider->GetTonnage() + 1.0) * 0.5), hitLocation, angle);
        car->HandleWeaponHit(&shot, multiplayer);
    };

    if (car->Derailed)
    {
        // A derailed car is only hurt by mechs, vehicles and elementals.
        if (collider->ObjectClass < MCObjectClass::BattleMech || MCObjectClass::Elemental < collider->ObjectClass)
        {
            return 0;
        }

        hitCar(-1);
        return 0;
    }

    // Something heavy stops the train; movers and buildings in the way take the train's weight.
    if (20.0f <= collider->GetTonnage())
    {
        train->Speed = 0.0f;
    }

    switch (collider->ObjectClass)
    {
        case MCObjectClass::BattleMech:
        case MCObjectClass::GroundVehicle:
        case MCObjectClass::Elemental:
        {
            const int32_t hitLocation = collider->CalcHitLocation(car, -1, 1, 0);
            const auto angle = static_cast<float>(collider->RelFacingTo(car->GetPosition(), -1));
            MCWeaponShotInfo shot;
            shot.Init(car, -1, train->GetTotalTonnage() * 0.2f + 0.5f, hitLocation, angle);
            collider->HandleWeaponHit(&shot, multiplayer);
            hitCar(hitLocation);
            return 0;
        }

        case MCObjectClass::Building:
        case MCObjectClass::TreeBuilding:
        {
            train->Speed = 0.0f;
            MCWeaponShotInfo shot;
            shot.Init(car, -1, train->GetTotalTonnage() * 0.2f + 0.5f, -1, -1.0f);
            collider->HandleWeaponHit(&shot, multiplayer);
            hitCar(-1);
            return 0;
        }

        default:
            return 0;
    }
}

auto MCTrainCarType::HandleDestruction(MCGameObject* collidee, MCGameObject*) -> int
{
    const auto blast = static_cast<float>(ExplosionDamage);
    MCVector3D where = collidee->GetPosition();
    CreateExplosion(where, blast, blast);
    return 0;
}
