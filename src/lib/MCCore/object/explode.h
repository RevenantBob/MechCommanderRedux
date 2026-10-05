#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class Appearance;
class File;
class GameObject;

/// <summary>The type of an <see cref="Explosion"/>: the damage it deals, over what radius, and its sound and light.</summary>
/// <remarks>Original source: <c>object\explode.cpp</c>, 0x44 bytes. Read from the "ExplosionData" block of its FIT.</remarks>
class ExplosionType : public ObjectType
{
public:
    /// <summary>Zeroes the damage, radius and chunk size and sets the sound to none (-1).</summary>
    /// <remarks>Inline in ObjectTypeManager::load (case 4).</remarks>
    ExplosionType();
    /// <remarks>MCX.EXE @ 0x00690480 (vector deleting destructor)</remarks>
    ~ExplosionType() override { destroy(); }

    /// <summary>Makes an <see cref="Explosion"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x0065f780</remarks>
    BaseObject* createInstance() override;
    /// <remarks>MCX.EXE @ 0x0065f8e0</remarks>
    void destroy() override;
    /// <summary>
    /// Reads DmgLevel, SoundEffectId, ExplosionRadius (default 0), LightObjectId (default -1) and DamageChunkSize
    /// (default 5) from the "ExplosionData" block, then the common type data.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0065f8f0</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <summary>
    /// Damages what the explosion hit: the explosion's damage in hits of at most damageChunkSize (movers get random
    /// hit locations); objects of class 0x1e/0x1f only when they are within their extent of the blast.
    /// Only the host applies it in multiplayer.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0065fa20 (unnamed in the symbols: ExplosionType's vtable slot 4)</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <remarks>MCX.EXE @ 0x0065fd50</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>The damage the explosion deals (FIT "DmgLevel").</summary>
    uint32_t dmgLevel = 0; // +0x30
    /// <summary>Sample played when the explosion starts (FIT "SoundEffectId"); 0xFFFFFFFF for none.</summary>
    uint32_t soundEffectId = 0; // +0x34
    /// <summary>Object type of the light the explosion creates (FIT "LightObjectId"); -1 for none.</summary>
    uint32_t lightObjectId = 0; // +0x38
    /// <summary>The blast radius (FIT "ExplosionRadius"); 0 means the explosion damages nothing around it.</summary>
    int32_t explosionRadius = 0; // +0x3c
    /// <summary>The largest single hit the damage is split into (FIT "DamageChunkSize", default 5).</summary>
    float damageChunkSize = 0; // +0x40
};

/// <summary>
/// An explosion: a VFX appearance, a sound, an optional light, and after half a second a check for static objects
/// in the blast (see ExplosionType::handleCollision).
/// </summary>
/// <remarks>Original source: <c>object\explode.cpp</c>, <c>object\explode.h</c>; 0x9c bytes.</remarks>
class Explosion : public BigGameObject
{
public:
    Explosion() { init(); }
    /// <remarks>MCX.EXE @ 0x0065f890 (vector deleting destructor)</remarks>
    ~Explosion() override { destroy(); }

    /// <summary>Clears the appearance, light and timer and marks the explosion just created.</summary>
    /// <remarks>MCX.EXE @ 0x0065f820 (inline in <c>object\explode.h</c>)</remarks>
    void init() override;
    /// <summary>
    /// Makes the VFX appearance, sets the blast radius and damage from the type, and creates the light.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006602c0</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Destroys the appearance and the light.</summary>
    /// <remarks>MCX.EXE @ 0x00660280</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x0065f850</remarks>
    int32_t kill() override { return 0; }
    /// <summary>Plays the sound on the first update, turns on collision after 0.5 s, and advances the effect.</summary>
    /// <returns>The appearance's update result: 0 once the effect has finished.</returns>
    /// <remarks>MCX.EXE @ 0x00660150</remarks>
    int32_t update() override;
    /// <remarks>MCX.EXE @ 0x00660230</remarks>
    void render() override;
    /// <summary>Checks the static objects of the 3x3 terrain blocks around the explosion for collisions.</summary>
    /// <remarks>MCX.EXE @ 0x0065fe10</remarks>
    void handleStaticCollision() override;
    /// <summary>Projects the explosion to the screen; true when its appearance is visible to the main camera.</summary>
    /// <remarks>MCX.EXE @ 0x0065fd60</remarks>
    int onScreen() override;
    /// <summary>The blast radius: BigGameObject's explosion radius (+0x64, set by setExplRad).</summary>
    /// <remarks>MCX.EXE @ 0x0065f860 (inline in <c>object\explode.h</c>)</remarks>
    float getExtentRadius() override;
    /// <summary>Sets BigGameObject's explosion radius (+0x64).</summary>
    /// <remarks>MCX.EXE @ 0x0065f870 (inline in <c>object\explode.h</c>)</remarks>
    void setExtentRadius(float newRadius) override;

    /// <remarks>MCX.EXE @ 0x0065f880</remarks>
    virtual Appearance* getAppearancePtr() { return appearance; }

    /// <summary>The VFX appearance that draws the explosion.</summary>
    Appearance* appearance = nullptr; // +0x84
    /// <summary>Set by init; the first update clears it (and the base field at +0x24) and plays the sound.</summary>
    int32_t justCreated = 0; // +0x88
    /// <summary>Seconds since the explosion started.</summary>
    float timeAlive = 0; // +0x8c
    /// <summary>A copy of the type's damageChunkSize.</summary>
    float damageChunkSize = 0; // +0x90
    /// <summary>Set once the explosion has been checked for collisions (0.5 s in); the check is not repeated.</summary>
    int32_t collisionChecked = 0; // +0x94
    /// <summary>The light of the type's lightObjectId, kept at the explosion's position.</summary>
    GameObject* light = nullptr; // +0x98
};

/// <summary>
/// Creates an object of type <paramref name="objectTypeId"/> (an explosion) at <paramref name="position"/> and adds
/// it to the object list; a nonzero <paramref name="radius"/> also gives it a blast radius and
/// <paramref name="damage"/>. Does nothing for id -1.
/// </summary>
/// <remarks>MCX.EXE @ 0x00660430</remarks>
void CreateExplosion(int32_t objectTypeId, vector_3d& position, float damage, float radius);
