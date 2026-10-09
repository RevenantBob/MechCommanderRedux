#pragma once

#include "object/MCBigGameObject.h"

class MCVfxAppearance;

/// <summary>
/// A light effect (a VFX appearance) at a point of the map, rising by its type's altitude offset. The fire,
/// explosion or shot that makes it owns it and runs its update and render.
/// </summary>
/// <remarks>Original source: <c>object\light.cpp</c>, <c>object\light.h</c>.</remarks>
class MCLight : public MCBigGameObject
{
public:
    MCLight();
    ~MCLight() override;

    /// <summary>Makes the VFX appearance of the type's appearance id.</summary>
    int32_t Init(MCObjectType* objType) override;
    using MCBigGameObject::Init;
    int32_t Kill() override { return 0; }
    /// <summary>Rises by the altitude offset and advances the effect; a one-shot light finishes with it.</summary>
    int32_t Update() override;
    void Render() override;
    /// <summary>Projects the light to the screen; true when the appearance is visible to the main camera.</summary>
    int OnScreen() override;
    float GetExtentRadius() override { return 0.0f; }

    /// <summary>The VFX appearance that draws the light.</summary>
    std::unique_ptr<MCVfxAppearance> Appearance;
    /// <summary>Set until the first update (which turns collisions off).</summary>
    bool JustCreated = true;
    /// <summary>Set once a one-shot light's effect has finished: it no longer updates or draws.</summary>
    bool Finished = false;
};
