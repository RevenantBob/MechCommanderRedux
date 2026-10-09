#pragma once

#include "object/MCObjectType.h"

/// <summary>
/// The type of a <see cref="MCTrainCar"/>: its name, speed limits, hit points and what happens when it blows up.
/// </summary>
/// <remarks>Original source: <c>object\train.cpp</c>. Read from the "Train" block of its FIT.</remarks>
class MCTrainCarType : public MCObjectType
{
public:
    /// <summary>Makes a <see cref="MCTrainCar"/> of this type and gives it the next object id.</summary>
    std::unique_ptr<MCBaseObject> CreateInstance() override;
    /// <summary>
    /// Reads Name, Explosion Chance, Explosion Damage, Velocity Multiplier, Acceleration, Deceleration, TopSpeed,
    /// Damage and TonnageClass from the "Train" block, then the common type data.
    /// </summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>
    /// A car running into something: a derailed car is hurt by movers it lands on; a running one stops the train on a
    /// heavy obstacle and trades damage (scaled by the train's tonnage) with movers, buildings and tree buildings.
    /// </summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    /// <summary>Makes this type's explosion at the car, sized by <see cref="ExplosionDamage"/>.</summary>
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>String resource id of the car's name (FIT "Name").</summary>
    int32_t NameId = 0;
    /// <summary>FIT "Explosion Chance".</summary>
    uint8_t ExplosionChance = 0;
    /// <summary>FIT "Explosion Damage"; also the size of the explosion made by handleDestruction.</summary>
    uint8_t ExplosionDamage = 0;
    /// <summary>FIT "Velocity Multiplier".</summary>
    uint8_t VelocityMultiplier = 0;
    /// <summary>Top speed (FIT "TopSpeed").</summary>
    float TopSpeed = 0;
    /// <summary>FIT "Acceleration".</summary>
    float Acceleration = 0;
    /// <summary>FIT "Deceleration".</summary>
    float Deceleration = 0;
    /// <summary>Hit points of a car (FIT "Damage"); a car that has taken half of them may derail.</summary>
    int32_t Damage = 0;
    /// <summary>FIT "TonnageClass", given to the car's tonnage (-1 until read).</summary>
    float TonnageClass = -1.0f;
};
