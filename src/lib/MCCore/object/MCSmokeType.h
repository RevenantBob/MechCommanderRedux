#pragma once

#include "object/MCObjectType.h"
#include "platform/MCRegisteredBlock.h"

/// <summary>
/// The type of a <see cref="MCSmoke"/>: how fast and how long it puffs, how its spheres move and spread, and the
/// shape they are drawn with.
/// </summary>
/// <remarks>Original source: <c>object\smoke.cpp</c>. Read from the "SmokeData" block of its FIT.</remarks>
class MCSmokeType : public MCObjectType
{
public:
    /// <summary>Makes a <see cref="MCSmoke"/> of this type and gives it the next object id.</summary>
    std::unique_ptr<MCBaseObject> CreateInstance() override;
    /// <summary>Reads the "SmokeData" block and loads the smoke shape, then the common type data.</summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>Smoke ignores collisions.</summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override { return 0; }
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override { return 0; }

    /// <summary>FIT "zVelocity": the spheres' rising speed, stored in world units per second.</summary>
    float ZVelocity = 0;
    /// <summary>FIT "SmokePerSecond": spheres made per second.</summary>
    float SmokePerSecond = 0;
    /// <summary>FIT "SlowDownPercent": the fraction of the owner's velocity a new sphere keeps.</summary>
    float SlowDownPercent = 0;
    /// <summary>FIT "MaxSmokeSpheres": spheres per smoke.</summary>
    uint32_t MaxSmokeSpheres = 0;
    /// <summary>FIT "SmokeShape": the VFX shape file.</summary>
    MCRegisteredBlock SmokeShape;
    /// <summary>FIT "Duration", seconds of puffing.</summary>
    int32_t Duration = 0;
    /// <summary>FIT "randomVelX/Y/Z": random spread added to a new sphere's velocity.</summary>
    float RandomVelX = 0;
    float RandomVelY = 0;
    float RandomVelZ = 0;
    /// <summary>FIT "randomPosX/Y/Z": random spread added to a new sphere's position.</summary>
    float RandomPosX = 0;
    float RandomPosY = 0;
    float RandomPosZ = 0;
    /// <summary>FIT "HasRotation": the shape holds numRotations facings; spheres never settle on the ground.</summary>
    bool HasRotation = false;
    /// <summary>FIT "NumRotations" (read only with HasRotation).</summary>
    int32_t NumRotations = 0;
    /// <summary>FIT "FrameRate" (default 15), frames per second.</summary>
    float FrameRate = 0;
};
