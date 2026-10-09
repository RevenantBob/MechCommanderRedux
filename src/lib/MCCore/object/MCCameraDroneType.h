#pragma once

#include "object/MCObjectType.h"

/// <summary>The type of a <see cref="MCCameraDrone"/>: speed, hit points and battle value.</summary>
/// <remarks>
/// Original source: <c>object\artlry.cpp</c>. Read from the "General" block of its FIT; the extent radius is forced to
/// -1.
/// </remarks>
class MCCameraDroneType : public MCObjectType
{
public:
    /// <summary>Makes a <see cref="MCCameraDrone"/> of this type and gives it the next object id.</summary>
    std::unique_ptr<MCBaseObject> CreateInstance() override;
    /// <summary>Reads maxVelocity, maxDamage and BRValue from the "General" block.</summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override { return 0; }
    /// <summary>Marks the drone destroyed (status 2).</summary>
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>FIT "maxVelocity", meters per second.</summary>
    float MaxVelocity = 0;
    /// <summary>FIT "maxDamage": hit points.</summary>
    int32_t MaxDamage = 0;
    /// <summary>FIT "BRValue" (default 0): the drone's max and current CV.</summary>
    int32_t BrValue = 0;
};
