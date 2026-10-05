#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class Appearance;
class File;
class GameObject;

/// <summary>
/// The type of a <see cref="Light"/>: whether its effect plays once, and how fast it rises each frame.
/// </summary>
/// <remarks>Original source: <c>object\light.cpp</c>, 0x38 bytes. Read from the "LightData" block of its FIT.</remarks>
class LightType : public ObjectType
{
public:
    /// <remarks>MCX.EXE @ 0x00690b90 (vector deleting destructor)</remarks>
    ~LightType() override { destroy(); }

    /// <summary>Makes a <see cref="Light"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x00674600</remarks>
    BaseObject* createInstance() override;
    /// <remarks>MCX.EXE @ 0x00674720</remarks>
    void destroy() override;
    /// <summary>Reads OneShotFlag and AltitudeOffset from the "LightData" block, then the common type data.</summary>
    /// <remarks>MCX.EXE @ 0x00674730</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <summary>Lights ignore collisions.</summary>
    /// <remarks>MCX.EXE @ 0x00674810</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <remarks>MCX.EXE @ 0x00674820</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>Nonzero when the light's effect plays once and the light is then done (FIT "OneShotFlag").</summary>
    int32_t oneShotFlag = 0; // +0x30 (read as a FIT boolean)
    /// <summary>Added to the light's altitude every update (FIT "AltitudeOffset").</summary>
    float altitudeOffset = 0; // +0x34
};

/// <summary>A light effect (a VFX appearance) at a point of the map, rising by its type's altitude offset.</summary>
/// <remarks>Original source: <c>object\light.cpp</c>, <c>object\light.h</c>; 0x90 bytes.</remarks>
class Light : public BigGameObject
{
public:
    Light() { init(); }
    /// <remarks>MCX.EXE @ 0x006746d0 (vector deleting destructor)</remarks>
    ~Light() override { destroy(); }

    /// <summary>Clears the appearance and marks the light just created.</summary>
    /// <remarks>MCX.EXE @ 0x00674680 (inline in <c>object\light.h</c>)</remarks>
    void init() override;
    /// <summary>Makes the VFX appearance of the type's appearance id.</summary>
    /// <remarks>MCX.EXE @ 0x006749c0</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Destroys the appearance.</summary>
    /// <remarks>MCX.EXE @ 0x006749a0</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x006746a0</remarks>
    int32_t kill() override { return 0; }
    /// <summary>Rises by the altitude offset and advances the effect; a one-shot light finishes with it.</summary>
    /// <remarks>MCX.EXE @ 0x006748e0</remarks>
    int32_t update() override;
    /// <remarks>MCX.EXE @ 0x00674950</remarks>
    void render() override;
    /// <summary>Projects the light to the screen; true when the appearance is visible to the main camera.</summary>
    /// <remarks>MCX.EXE @ 0x00674830</remarks>
    int onScreen() override;
    /// <remarks>MCX.EXE @ 0x006746b0</remarks>
    float getExtentRadius() override { return 0.0f; }

    /// <remarks>MCX.EXE @ 0x006746c0</remarks>
    virtual Appearance* getAppearancePtr() { return appearance; }

    /// <summary>The VFX appearance that draws the light.</summary>
    Appearance* appearance = nullptr; // +0x84
    /// <summary>Set by init; the first update clears it (and the base field at +0x24).</summary>
    int32_t justCreated = 0; // +0x88
    /// <summary>Set once a one-shot light's effect has finished: it no longer updates or draws.</summary>
    int32_t finished = 0; // +0x8c
};
