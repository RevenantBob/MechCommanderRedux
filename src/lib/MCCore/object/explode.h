#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class MCAppearance;
class MCFile;
class MCGameObject;

/// <summary>The type of an <see cref="MCExplosion"/>: the damage it deals, over what radius, and its sound and light.</summary>
/// <remarks>Original source: <c>object\explode.cpp</c>, 0x44 bytes. Read from the "ExplosionData" block of its FIT.</remarks>
class MCExplosionType : public MCObjectType
{
public:
    /// <summary>Zeroes the damage, radius and chunk size and sets the sound to none (-1).</summary>
    /// <remarks>Inline in ObjectTypeManager::load (case 4).</remarks>
    MCExplosionType();
    ~MCExplosionType() override { Destroy(); }

    /// <summary>Makes an <see cref="MCExplosion"/> of this type and gives it the next object id.</summary>
    MCBaseObject* CreateInstance() override;
    void Destroy() override;
    /// <summary>
    /// Reads DmgLevel, SoundEffectId, ExplosionRadius (default 0), LightObjectId (default -1) and DamageChunkSize
    /// (default 5) from the "ExplosionData" block, then the common type data.
    /// </summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>
    /// Damages what the explosion hit: the explosion's damage in hits of at most damageChunkSize (movers get random
    /// hit locations); objects of class 0x1e/0x1f only when they are within their extent of the blast.
    /// Only the host applies it in multiplayer.
    /// </summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>The damage the explosion deals (FIT "DmgLevel").</summary>
    uint32_t DmgLevel = 0;
    /// <summary>Sample played when the explosion starts (FIT "SoundEffectId"); 0xFFFFFFFF for none.</summary>
    uint32_t SoundEffectId = 0;
    /// <summary>Object type of the light the explosion creates (FIT "LightObjectId"); -1 for none.</summary>
    uint32_t LightObjectId = 0;
    /// <summary>The blast radius (FIT "ExplosionRadius"); 0 means the explosion damages nothing around it.</summary>
    int32_t ExplosionRadius = 0;
    /// <summary>The largest single hit the damage is split into (FIT "DamageChunkSize", default 5).</summary>
    float DamageChunkSize = 0;
};

/// <summary>
/// An explosion: a VFX appearance, a sound, an optional light, and after half a second a check for static objects
/// in the blast (see ExplosionType::handleCollision).
/// </summary>
/// <remarks>Original source: <c>object\explode.cpp</c>, <c>object\explode.h</c>; 0x9c bytes.</remarks>
class MCExplosion : public MCBigGameObject
{
public:
    MCExplosion() { Init(); }
    ~MCExplosion() override { Destroy(); }

    /// <summary>Clears the appearance, light and timer and marks the explosion just created.</summary>
    void Init() override;
    /// <summary>
    /// Makes the VFX appearance, sets the blast radius and damage from the type, and creates the light.
    /// </summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Destroys the appearance and the light.</summary>
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>Plays the sound on the first update, turns on collision after 0.5 s, and advances the effect.</summary>
    /// <returns>The appearance's update result: 0 once the effect has finished.</returns>
    int32_t Update() override;
    void Render() override;
    /// <summary>Checks the static objects of the 3x3 terrain blocks around the explosion for collisions.</summary>
    void HandleStaticCollision() override;
    /// <summary>Projects the explosion to the screen; true when its appearance is visible to the main camera.</summary>
    int OnScreen() override;
    /// <summary>The blast radius: BigGameObject's explosion radius (+0x64, set by setExplRad).</summary>
    float GetExtentRadius() override;
    /// <summary>Sets BigGameObject's explosion radius (+0x64).</summary>
    void SetExtentRadius(float newRadius) override;

    virtual MCAppearance* GetAppearancePtr() { return Appearance; }

    /// <summary>The VFX appearance that draws the explosion.</summary>
    MCAppearance* Appearance = nullptr;
    /// <summary>Set by init; the first update clears it (and the base field at +0x24) and plays the sound.</summary>
    int32_t JustCreated = 0;
    /// <summary>Seconds since the explosion started.</summary>
    float TimeAlive = 0;
    /// <summary>A copy of the type's damageChunkSize.</summary>
    float DamageChunkSize = 0;
    /// <summary>Set once the explosion has been checked for collisions (0.5 s in); the check is not repeated.</summary>
    int32_t CollisionChecked = 0;
    /// <summary>The light of the type's lightObjectId, kept at the explosion's position.</summary>
    MCGameObject* Light = nullptr;
};

/// <summary>
/// Creates an object of type <paramref name="objectTypeId"/> (an explosion) at <paramref name="position"/> and adds
/// it to the object list; a nonzero <paramref name="radius"/> also gives it a blast radius and
/// <paramref name="damage"/>. Does nothing for id -1.
/// </summary>
void CreateExplosion(int32_t objectTypeId, MCVector3D& position, float damage, float radius);
