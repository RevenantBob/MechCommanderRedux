#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class MCAppearance;
class MCFile;
class MCGameObject;
class MCSmoke;

/// <summary>The type of a <see cref="MCBullet"/>: its speed, sound, and the effects it makes on hit and miss.</summary>
/// <remarks>
/// Original source: <c>object\bullet.cpp</c>, 0x4c bytes. Read from the "BulletData" block of its FIT; loading it
/// also loads the hit, miss and smoke object types.
/// </remarks>
class MCBulletType : public MCObjectType
{
public:
    /// <summary>Sets the sound and the hit, miss and smoke objects to none (-1).</summary>
    /// <remarks>Inline in ObjectTypeManager::load (case 8).</remarks>
    MCBulletType();
    ~MCBulletType() override { Destroy(); }

    /// <summary>Makes a <see cref="MCBullet"/> of this type and gives it the next object id.</summary>
    MCBaseObject* CreateInstance() override;
    void Destroy() override;
    /// <summary>
    /// Reads the "BulletData" block (if present) and the common type data, then loads the hit, miss and smoke types.
    /// </summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>Sample played when the bullet is fired (FIT "SoundEffectId"); 0xFFFFFFFF for none.</summary>
    uint32_t SoundEffectId = 0;
    /// <summary>Object type created where the bullet hits its target (FIT "BulletHitEffect").</summary>
    uint32_t BulletHitEffect = 0;
    /// <summary>Object type created where a bullet without a target lands (FIT "BulletMissEffect").</summary>
    uint32_t BulletMissEffect = 0;
    /// <summary>Object type of the smoke trail (FIT "SmokeObjectId"); -1 for none.</summary>
    uint32_t SmokeObjectId = 0;
    /// <summary>Object type of the light that travels with the bullet (FIT "LightObjectId"); -1 for none.</summary>
    uint32_t LightObjectId = 0;
    /// <summary>Speed in world units per second (FIT "Velocity").</summary>
    float Velocity = 0;
    /// <summary>FIT "CloseDistance"; not used by bullet.cpp.</summary>
    float CloseDistance = 0;
};

/// <summary>
/// A projectile that flies from its owner's hot spot to a target (or a target position), then applies its shots'
/// damage and creates a hit or miss effect; a miss on a mined cell sets the mine off.
/// </summary>
/// <remarks>Original source: <c>object\bullet.cpp</c>, <c>object\bullet.h</c>; 0x124 bytes.</remarks>
class MCBullet : public MCBigGameObject
{
public:
    /// <summary>Zeroes the pointers and counts and sets justCreated (inline in BulletType::createInstance).</summary>
    MCBullet();
    ~MCBullet() override { Destroy(); }

    /// <summary>Empty in the original.</summary>
    void Init() override;
    /// <summary>Makes the arm appearance and the smoke and light objects of the type.</summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Frees the target position and destroys the appearance, smoke and light.</summary>
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>
    /// Moves the bullet toward its target; once it stops getting closer applies the shots to the target and creates
    /// the hit (or miss) effect.
    /// </summary>
    /// <returns>1 while flying (or while the smoke trail lasts), 0 when done.</returns>
    int32_t Update() override;
    void Render() override;

    virtual MCAppearance* GetAppearancePtr() { return nullptr; }

    /// <summary>Projects the bullet to the screen; true unless its appearance is off screen.</summary>
    int IsVisible();
    void SetOwner(MCBaseObject* newOwner);
    void SetTarget(MCBaseObject* newTarget);
    /// <summary>Sets (allocating it the first time) the position the bullet flies to.</summary>
    void SetTargetPosition(MCVector3D position);
    /// <summary>Sets the owner and the hot spot it fires from, and the target position.</summary>
    void Connect(MCGameObject* source, MCVector3D targetPos, int32_t sourceHotSpot);

    /// <summary>Set by the constructor and init; the first update clears it, plays the sound and places the bullet.</summary>
    int32_t JustCreated = 0;
    /// <summary>The object that fired the bullet (stored as a BaseObject by <see cref="SetOwner"/>).</summary>
    MCGameObject* Owner = nullptr;
    /// <summary>The owner's hot spot the bullet leaves from.</summary>
    int32_t OwnerHotSpot = 0;
    /// <summary>The object the bullet flies to and damages; null for a shot at a position.</summary>
    MCGameObject* Target = nullptr;
    /// <summary>The target's hot spot where the hit effect is placed (not used for class 0x1e targets).</summary>
    int32_t TargetHotSpot = 0;
    /// <summary>Where the bullet flies to, allocated by <see cref="SetTargetPosition"/>.</summary>
    MCVector3D* TargetPosition = nullptr;
    /// <summary>The smallest squared ground distance to the target so far (starts at 1e8); growing again means arrival.</summary>
    float ClosestDistanceSq = 0;
    /// <summary>The arm appearance of the bullet.</summary>
    MCAppearance* Appearance = nullptr;
    /// <summary>How many entries of shotInfo are applied on arrival.</summary>
    int32_t NumShots = 0;
    /// <summary>The shots applied to the target on arrival (0x14 bytes each).</summary>
    MCWeaponShotInfo ShotInfo[5]{};
    /// <summary>The smoke trail.</summary>
    MCSmoke* Smoke = nullptr;
    /// <summary>The bullet's own flight position (the object position follows the owner's hot spot).</summary>
    MCVector3D BulletPosition;
    /// <summary>The light that travels with the bullet.</summary>
    MCGameObject* Light = nullptr;
    /// <summary>The draw rotation of the appearance: -150, or 150 when a mech owner faces the other way.</summary>
    int32_t DrawRotation = 0;
};
