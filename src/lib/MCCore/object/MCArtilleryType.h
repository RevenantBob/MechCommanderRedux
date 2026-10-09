#pragma once

#include "object/MCObjectType.h"
#include "platform/MCRegisteredBlock.h"

/// <summary>One entry of an artillery strike's explosion pattern: where it goes off from the strike point, and when.</summary>
struct MCArtilleryBlast
{
    /// <summary>World units from the strike point (FIT "ExplosionOffsetX%d").</summary>
    float OffsetX = 0;
    /// <summary>FIT "ExplosionOffsetY%d".</summary>
    float OffsetY = 0;
    /// <summary>Seconds after impact (FIT "ExplosionDelay%d").</summary>
    float Delay = 0;
};

/// <summary>
/// The type of an <see cref="MCArtillery"/> strike: its countdown sprite, timing, damage, the ranges and hit counts of
/// its major and minor blast, its sensor probe and the pattern of explosions it sets off.
/// </summary>
/// <remarks>Original source: <c>object\artlry.cpp</c>. Read from the "Artillery" block of its FIT.</remarks>
class MCArtilleryType : public MCObjectType
{
public:
    /// <summary>Makes an <see cref="MCArtillery"/> of this type and gives it the next object id.</summary>
    std::unique_ptr<MCBaseObject> CreateInstance() override;
    /// <summary>Reads the "Artillery" block and the explosion pattern, and loads the countdown sprite.</summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>
    /// Damages <paramref name="collider"/> once the strike (<paramref name="collidee"/>) has hit: the major hit count
    /// inside the major range, else the minor one.
    /// </summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override { return 0; }

    /// <summary>The countdown sprite (a VFX shape file).</summary>
    MCRegisteredBlock ShapeData;
    /// <summary>FIT "FrameCount".</summary>
    uint32_t FrameCount = 0;
    /// <summary>FIT "StartFrame".</summary>
    uint32_t StartFrame = 0;
    /// <summary>FIT "FrameRate", frames per second.</summary>
    float FrameRate = 0;
    /// <summary>FIT "NominalTimeToImpact", seconds.</summary>
    float NominalTimeToImpact = 0;
    /// <summary>FIT "NominalTimeToLaunch", seconds (default: time to impact - 10).</summary>
    float NominalTimeToLaunch = 0;
    /// <summary>FIT "NominalDamage": damage per hit; 0 for a sensor probe.</summary>
    float NominalDamage = 0;
    /// <summary>FIT "NominalMajorRange", meters.</summary>
    float NominalMajorRange = 0;
    /// <summary>FIT "NominalMajorHits".</summary>
    float NominalMajorHits = 0;
    /// <summary>FIT "NominalMinorRange", meters.</summary>
    float NominalMinorRange = 0;
    /// <summary>FIT "NominalMinorHits".</summary>
    float NominalMinorHits = 0;
    /// <summary>FIT "NominalSensorTime", seconds the sensor probe lasts.</summary>
    float NominalSensorTime = 0;
    /// <summary>FIT "NominalSensorRange", meters.</summary>
    float NominalSensorRange = 0;
    /// <summary>FIT "fontScale".</summary>
    float FontScale = 0;
    /// <summary>FIT "fontXOffset": where the countdown text goes, relative to the sprite.</summary>
    float FontXOffset = 0;
    /// <summary>FIT "fontYOffset".</summary>
    float FontYOffset = 0;
    /// <summary>FIT "fontColor".</summary>
    uint32_t FontColor = 0;
    /// <summary>The explosion pattern (FIT "NumExplosions" entries; only read when there is damage).</summary>
    std::vector<MCArtilleryBlast> Blasts;
    /// <summary>FIT "ExplosionsPerExplosion": explosions made at each entry.</summary>
    int32_t ExplosionsPerExplosion = 0;
    /// <summary>FIT "ExplosionRandomOffsetX": random spread of each explosion.</summary>
    int32_t ExplosionRandomOffsetX = 0;
    /// <summary>FIT "ExplosionRandomOffsetY".</summary>
    int32_t ExplosionRandomOffsetY = 0;
    /// <summary>
    /// FIT "MinArtilleryHeadRange" (default 5), meters: beyond it a hit lands on hit-location table 4, else 2.
    /// </summary>
    int32_t MinArtilleryHeadRange = 0;
};
