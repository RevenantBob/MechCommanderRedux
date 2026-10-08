#pragma once

#include "object/MCBigGameObject.h"
#include "object/MCObjectType.h"

class MCAppearance;
class MCFile;
class MCGameObject;

/// <summary>
/// The type of a <see cref="MCLight"/>: whether its effect plays once, and how fast it rises each frame.
/// </summary>
/// <remarks>Original source: <c>object\light.cpp</c>, 0x38 bytes. Read from the "LightData" block of its FIT.</remarks>
class MCLightType : public MCObjectType
{
public:
    ~MCLightType() override { Destroy(); }

    /// <summary>Makes a <see cref="MCLight"/> of this type and gives it the next object id.</summary>
    std::unique_ptr<MCBaseObject> CreateInstance() override;
    void Destroy() override;
    /// <summary>Reads OneShotFlag and AltitudeOffset from the "LightData" block, then the common type data.</summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>Lights ignore collisions.</summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>Nonzero when the light's effect plays once and the light is then done (FIT "OneShotFlag").</summary>
    int32_t OneShotFlag = 0; // (read as a FIT boolean)
    /// <summary>Added to the light's altitude every update (FIT "AltitudeOffset").</summary>
    float AltitudeOffset = 0;
};

/// <summary>A light effect (a VFX appearance) at a point of the map, rising by its type's altitude offset.</summary>
/// <remarks>Original source: <c>object\light.cpp</c>, <c>object\light.h</c>; 0x90 bytes.</remarks>
class MCLight : public MCBigGameObject
{
public:
    MCLight() { Init(); }
    ~MCLight() override { Destroy(); }

    /// <summary>Clears the appearance and marks the light just created.</summary>
    void Init() override;
    /// <summary>Makes the VFX appearance of the type's appearance id.</summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Destroys the appearance.</summary>
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>Rises by the altitude offset and advances the effect; a one-shot light finishes with it.</summary>
    int32_t Update() override;
    void Render() override;
    /// <summary>Projects the light to the screen; true when the appearance is visible to the main camera.</summary>
    int OnScreen() override;
    float GetExtentRadius() override { return 0.0f; }

    virtual MCAppearance* GetAppearancePtr() { return Appearance; }

    /// <summary>The VFX appearance that draws the light.</summary>
    MCAppearance* Appearance = nullptr;
    /// <summary>Set by init; the first update clears it (and the base field at +0x24).</summary>
    int32_t JustCreated = 0;
    /// <summary>Set once a one-shot light's effect has finished: it no longer updates or draws.</summary>
    int32_t Finished = 0;
};
