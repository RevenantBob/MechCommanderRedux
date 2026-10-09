#pragma once

#include "object/MCObjectType.h"

/// <summary>
/// The type of a <see cref="MCLight"/>: whether its effect plays once, and how fast it rises each frame.
/// </summary>
/// <remarks>Original source: <c>object\light.cpp</c>. Read from the "LightData" block of its FIT.</remarks>
class MCLightType : public MCObjectType
{
public:
    /// <summary>Makes a <see cref="MCLight"/> of this type and gives it the next object id.</summary>
    std::unique_ptr<MCBaseObject> CreateInstance() override;
    /// <summary>Reads OneShotFlag and AltitudeOffset from the "LightData" block, then the common type data.</summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>Lights ignore collisions.</summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override { return 0; }
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override { return 0; }

    /// <summary>Set when the light's effect plays once and the light is then done (FIT "OneShotFlag").</summary>
    bool OneShotFlag = false;
    /// <summary>Added to the light's altitude every update (FIT "AltitudeOffset").</summary>
    float AltitudeOffset = 0;
};
