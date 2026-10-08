#pragma once

#include "object/MCBigGameObject.h"
#include "object/MCObjectType.h"
#include "object/MCWeaponShotInfo.h"

class MCAppearance;
class MCFile;
class MCGameObject;
class MCSmoke;

/// <summary>
/// The type of a <see cref="MCProjectileLaser"/> (a pulse of laser light that travels like a bullet): its speed and
/// shape, its colours for friendly and enemy shooters, and its sound, effects, smoke and light.
/// </summary>
/// <remarks>
/// Original source: <c>object\prjlase.cpp</c>, 0x60 bytes. Read from the "ProjectileLaserData" block; loading it also
/// loads the hit and miss object types. Its vector deleting destructor is at 0x006909a0 (no symbol).
/// </remarks>
class MCProjectileLaserType : public MCObjectType
{
public:
    /// <summary>Sets the sound and effects to none (-1) and zeroes the colours and lengths.</summary>
    /// <remarks>Inline in ObjectTypeManager::load (case 0x11).</remarks>
    MCProjectileLaserType();
    ~MCProjectileLaserType() override { Destroy(); }

    /// <summary>Makes a <see cref="MCProjectileLaser"/> of this type and gives it the next object id.</summary>
    std::unique_ptr<MCBaseObject> CreateInstance() override;
    void Destroy() override;
    /// <summary>
    /// Reads the "ProjectileLaserData" block (if present) and the common type data, then loads the hit and miss types.
    /// </summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>Sample played when fired (FIT "SoundEffectId"); 0xFFFFFFFF for none.</summary>
    uint32_t SoundEffectId = 0;
    /// <summary>Object type created where it hits its target (FIT "ProjectileHitEffect").</summary>
    uint32_t ProjectileHitEffect = 0;
    /// <summary>Object type created where a shot without a target lands (FIT "ProjectileMissEffect").</summary>
    uint32_t ProjectileMissEffect = 0;
    /// <summary>Object type of the smoke trail (FIT "SmokeObjectId", default -1).</summary>
    uint32_t SmokeObjectId = 0;
    /// <summary>Object type of the light travelling with it (FIT "LightObjectId", default -1).</summary>
    uint32_t LightObjectId = 0;
    /// <summary>Speed in world units per second (FIT "Velocity").</summary>
    float Velocity = 0;
    /// <summary>Distance from the target at which the smoke trail stops (FIT "CloseDistance").</summary>
    float CloseDistance = 0;
    /// <summary>The four colours used when the owner is friendly (FIT "f0Color".."f3Color").</summary>
    uint8_t FColor[4]{};
    /// <summary>The four colours used when the owner is an enemy (FIT "e0Color".."e3Color").</summary>
    uint8_t EColor[4]{};
    /// <summary>Distance from the head to the tail (FIT "ProjectileLength").</summary>
    float ProjectileLength = 0;
    /// <summary>Distance from the head to the bulge (FIT "BulgeLength").</summary>
    float BulgeLength = 0;
    /// <summary>Half width of the bulge (FIT "BulgeWidth").</summary>
    float BulgeWidth = 0;
};

/// <summary>
/// A pulse laser shot: a coloured diamond (head, two bulge points, tail) that flies from its owner's hot spot to its
/// target like a <see cref="MCBullet"/>, with a smoke trail and a light, and applies its shot on arrival.
/// </summary>
/// <remarks>Original source: <c>object\prjlase.cpp</c>, <c>object\prjlase.h</c>; 0x130 bytes.</remarks>
class MCProjectileLaser : public MCBigGameObject
{
public:
    /// <summary>
    /// Zeroes the pointers and smoke displacement, sets justCreated and makes the frame the world frame (inline in
    /// ProjectileLaserType::createInstance).
    /// </summary>
    MCProjectileLaser();
    ~MCProjectileLaser() override { Destroy(); }

    /// <summary>Empty in the original.</summary>
    void Init() override;
    /// <summary>Makes the arm appearance and the smoke and light objects of the type; class 0x1a.</summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Frees the target position and destroys the appearance, smoke and light.</summary>
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>
    /// Moves the head toward the target and rebuilds the bulge and tail points; once it stops getting closer applies
    /// the shot and creates the hit (or miss) effect, setting off a mine under a miss.
    /// </summary>
    /// <returns>1 while flying, 0 when done.</returns>
    int32_t Update() override;
    /// <summary>Draws the shot as polygons in the type's friendly or enemy colours, then the smoke and light.</summary>
    void Render() override;
    MCFrameOfRef GetFrame() override { return Frame; }
    void SetFrame(MCFrameOfRef& newFrame) override { Frame = newFrame; }

    /// <summary>Always null (the vtable's slot 116; unnamed in the symbols: xor eax,eax / ret).</summary>
    virtual MCAppearance* GetAppearancePtr() { return nullptr; }

    /// <summary>Projects the shot to the screen; true when its appearance is visible to the main camera.</summary>
    int IsVisible();
    void SetOwner(MCBaseObject* newOwner);
    /// <summary>Sets (allocating it the first time) the position the shot flies to.</summary>
    void SetTargetPosition(MCVector3D position);
    /// <summary>
    /// Sets the owner and the hot spot it fires from, the target position, and copies <paramref name="shotInfo"/>
    /// (if any) as the shot to apply.
    /// </summary>
    void Connect(MCGameObject* source, MCVector3D targetPos, MCWeaponShotInfo* shotInfo, int32_t sourceHotSpot);

    /// <summary>Set by the constructor and init; the first update clears it, places the shot and plays the sound.</summary>
    int32_t JustCreated = 0;
    /// <summary>The object that fired (stored as a BaseObject by <see cref="SetOwner"/>).</summary>
    MCGameObject* Owner = nullptr;
    /// <summary>The owner's hot spot the shot leaves from.</summary>
    int32_t OwnerHotSpot = 0;
    /// <summary>The object the shot flies to and damages; null for a shot at a position.</summary>
    MCGameObject* Target = nullptr;
    /// <summary>The target's hot spot where the hit effect is placed (not used for class 0x1e targets).</summary>
    int32_t TargetHotSpot = 0;
    /// <summary>Where the shot flies to, allocated by <see cref="SetTargetPosition"/>.</summary>
    MCVector3D* TargetPosition = nullptr;
    /// <summary>The smallest squared ground distance to the target so far (starts at 1e8); growing again means arrival.</summary>
    float ClosestDistanceSq = 0;
    /// <summary>The arm appearance.</summary>
    MCAppearance* Appearance = nullptr;
    /// <summary>The shot applied to the target on arrival.</summary>
    MCWeaponShotInfo ShotInfo{};
    /// <summary>The head of the shot: its own flight position.</summary>
    MCVector3D HeadPosition;
    /// <summary>One side point of the bulge (bulgeLength behind the head, bulgeWidth to one side).</summary>
    MCVector3D BulgeSide1;
    /// <summary>The other side point of the bulge.</summary>
    MCVector3D BulgeSide2;
    /// <summary>The tail (projectileLength behind the head); the smoke and light follow it.</summary>
    MCVector3D TailPosition;
    /// <summary>The centre of the bulge (bulgeLength behind the head).</summary>
    MCVector3D BulgeCenter;
    /// <summary>The smoke trail.</summary>
    MCSmoke* Smoke = nullptr;
    /// <summary>The total the smoke has been moved by (accumulated each update; not read in prjlase.cpp).</summary>
    MCVector3D SmokeDisplacement;
    /// <summary>Orientation, returned by <see cref="GetFrame"/>; the owner's frame on the first update.</summary>
    MCFrameOfRef Frame;
    /// <summary>The light travelling with the shot.</summary>
    MCGameObject* Light = nullptr;
    /// <summary>The draw rotation of the appearance: -150, or 150 when a mech owner faces the other way.</summary>
    int32_t DrawRotation = 0;
};
