#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class Appearance;
class File;
class GameObject;
class Smoke;

/// <summary>The type of a <see cref="Bullet"/>: its speed, sound, and the effects it makes on hit and miss.</summary>
/// <remarks>
/// Original source: <c>object\bullet.cpp</c>, 0x4c bytes. Read from the "BulletData" block of its FIT; loading it
/// also loads the hit, miss and smoke object types.
/// </remarks>
class BulletType : public ObjectType
{
public:
    /// <summary>Sets the sound and the hit, miss and smoke objects to none (-1).</summary>
    /// <remarks>Inline in ObjectTypeManager::load (case 8).</remarks>
    BulletType();
    /// <remarks>MCX.EXE @ 0x006906c0 (vector deleting destructor)</remarks>
    ~BulletType() override { destroy(); }

    /// <summary>Makes a <see cref="Bullet"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x006551a0</remarks>
    BaseObject* createInstance() override;
    /// <remarks>MCX.EXE @ 0x006552e0</remarks>
    void destroy() override;
    /// <summary>
    /// Reads the "BulletData" block (if present) and the common type data, then loads the hit, miss and smoke types.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006552f0</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <remarks>MCX.EXE @ 0x00655440</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <remarks>MCX.EXE @ 0x00655450</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>Sample played when the bullet is fired (FIT "SoundEffectId"); 0xFFFFFFFF for none.</summary>
    uint32_t soundEffectId = 0; // +0x30
    /// <summary>Object type created where the bullet hits its target (FIT "BulletHitEffect").</summary>
    uint32_t bulletHitEffect = 0; // +0x34
    /// <summary>Object type created where a bullet without a target lands (FIT "BulletMissEffect").</summary>
    uint32_t bulletMissEffect = 0; // +0x38
    /// <summary>Object type of the smoke trail (FIT "SmokeObjectId"); -1 for none.</summary>
    uint32_t smokeObjectId = 0; // +0x3c
    /// <summary>Object type of the light that travels with the bullet (FIT "LightObjectId"); -1 for none.</summary>
    uint32_t lightObjectId = 0; // +0x40
    /// <summary>Speed in world units per second (FIT "Velocity").</summary>
    float velocity = 0; // +0x44
    /// <summary>FIT "CloseDistance"; not used by bullet.cpp.</summary>
    float closeDistance = 0; // +0x48
};

/// <summary>
/// A projectile that flies from its owner's hot spot to a target (or a target position), then applies its shots'
/// damage and creates a hit or miss effect; a miss on a mined cell sets the mine off.
/// </summary>
/// <remarks>Original source: <c>object\bullet.cpp</c>, <c>object\bullet.h</c>; 0x124 bytes.</remarks>
class Bullet : public BigGameObject
{
public:
    /// <summary>Zeroes the pointers and counts and sets justCreated (inline in BulletType::createInstance).</summary>
    Bullet();
    /// <remarks>MCX.EXE @ 0x00655290 (vector deleting destructor)</remarks>
    ~Bullet() override { destroy(); }

    /// <summary>Empty in the original.</summary>
    /// <remarks>MCX.EXE @ 0x00655260 (inline in <c>object\bullet.h</c>)</remarks>
    void init() override;
    /// <summary>Makes the arm appearance and the smoke and light objects of the type.</summary>
    /// <remarks>MCX.EXE @ 0x00655ba0</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Frees the target position and destroys the appearance, smoke and light.</summary>
    /// <remarks>MCX.EXE @ 0x00655b30</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x00655270</remarks>
    int32_t kill() override { return 0; }
    /// <summary>
    /// Moves the bullet toward its target; once it stops getting closer applies the shots to the target and creates
    /// the hit (or miss) effect.
    /// </summary>
    /// <returns>1 while flying (or while the smoke trail lasts), 0 when done.</returns>
    /// <remarks>MCX.EXE @ 0x00655510</remarks>
    int32_t update() override;
    /// <remarks>MCX.EXE @ 0x00655ae0</remarks>
    void render() override;

    /// <remarks>MCX.EXE @ 0x00655280</remarks>
    virtual Appearance* getAppearancePtr() { return nullptr; }

    /// <summary>Projects the bullet to the screen; true unless its appearance is off screen.</summary>
    /// <remarks>MCX.EXE @ 0x00655460</remarks>
    int isVisible();
    /// <remarks>MCX.EXE @ 0x00655d20</remarks>
    void setOwner(BaseObject* newOwner);
    /// <remarks>MCX.EXE @ 0x00655d30</remarks>
    void setTarget(BaseObject* newTarget);
    /// <summary>Sets (allocating it the first time) the position the bullet flies to.</summary>
    /// <remarks>MCX.EXE @ 0x00655d40</remarks>
    void setTargetPosition(vector_3d position);
    /// <summary>Sets the owner and the hot spot it fires from, and the target position.</summary>
    /// <remarks>MCX.EXE @ 0x0065f6e0 (inline in <c>object\bullet.h</c>)</remarks>
    void connect(GameObject* source, vector_3d targetPos, int32_t sourceHotSpot);

    /// <summary>Set by the constructor and init; the first update clears it, plays the sound and places the bullet.</summary>
    int32_t justCreated = 0; // +0x84
    /// <summary>The object that fired the bullet (stored as a BaseObject by <see cref="setOwner"/>).</summary>
    GameObject* owner = nullptr; // +0x88
    /// <summary>The owner's hot spot the bullet leaves from.</summary>
    int32_t ownerHotSpot = 0; // +0x8c
    /// <summary>The object the bullet flies to and damages; null for a shot at a position.</summary>
    GameObject* target = nullptr; // +0x90
    /// <summary>The target's hot spot where the hit effect is placed (not used for class 0x1e targets).</summary>
    int32_t targetHotSpot = 0; // +0x94
    /// <summary>Where the bullet flies to, allocated by <see cref="setTargetPosition"/>.</summary>
    vector_3d* targetPosition = nullptr; // +0x98
    /// <summary>The smallest squared ground distance to the target so far (starts at 1e8); growing again means arrival.</summary>
    float closestDistanceSq = 0; // +0x9c
    /// <summary>The arm appearance of the bullet.</summary>
    Appearance* appearance = nullptr; // +0xa0
    /// <summary>How many entries of shotInfo are applied on arrival.</summary>
    int32_t numShots = 0; // +0xa4
    /// <summary>The shots applied to the target on arrival (0x14 bytes each).</summary>
    _WeaponShotInfo shotInfo[5]{}; // +0xa8
    /// <summary>The smoke trail.</summary>
    Smoke* smoke = nullptr; // +0x10c
    /// <summary>The bullet's own flight position (the object position follows the owner's hot spot).</summary>
    vector_3d bulletPosition; // +0x110
    /// <summary>The light that travels with the bullet.</summary>
    GameObject* light = nullptr; // +0x11c
    /// <summary>The draw rotation of the appearance: -150, or 150 when a mech owner faces the other way.</summary>
    int32_t drawRotation = 0; // +0x120
};
