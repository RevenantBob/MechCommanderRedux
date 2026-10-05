#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class Appearance;
class File;
class GameObject;
class Smoke;

/// <summary>
/// The type of a <see cref="ProjectileLaser"/> (a pulse of laser light that travels like a bullet): its speed and
/// shape, its colours for friendly and enemy shooters, and its sound, effects, smoke and light.
/// </summary>
/// <remarks>
/// Original source: <c>object\prjlase.cpp</c>, 0x60 bytes. Read from the "ProjectileLaserData" block; loading it also
/// loads the hit and miss object types. Its vector deleting destructor is FUN_006909a0 (unnamed in the symbols).
/// </remarks>
class ProjectileLaserType : public ObjectType
{
public:
    /// <summary>Sets the sound and effects to none (-1) and zeroes the colours and lengths.</summary>
    /// <remarks>Inline in ObjectTypeManager::load (case 0x11).</remarks>
    ProjectileLaserType();
    /// <remarks>MCX.EXE @ 0x006909a0 (vector deleting destructor, unnamed in the symbols)</remarks>
    ~ProjectileLaserType() override { destroy(); }

    /// <summary>Makes a <see cref="ProjectileLaser"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x00691490</remarks>
    BaseObject* createInstance() override;
    /// <remarks>MCX.EXE @ 0x006916f0</remarks>
    void destroy() override;
    /// <summary>
    /// Reads the "ProjectileLaserData" block (if present) and the common type data, then loads the hit and miss types.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00691700</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <remarks>MCX.EXE @ 0x00691970</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <remarks>MCX.EXE @ 0x00691980</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>Sample played when fired (FIT "SoundEffectId"); 0xFFFFFFFF for none.</summary>
    uint32_t soundEffectId = 0; // +0x30
    /// <summary>Object type created where it hits its target (FIT "ProjectileHitEffect").</summary>
    uint32_t projectileHitEffect = 0; // +0x34
    /// <summary>Object type created where a shot without a target lands (FIT "ProjectileMissEffect").</summary>
    uint32_t projectileMissEffect = 0; // +0x38
    /// <summary>Object type of the smoke trail (FIT "SmokeObjectId", default -1).</summary>
    uint32_t smokeObjectId = 0; // +0x3c
    /// <summary>Object type of the light travelling with it (FIT "LightObjectId", default -1).</summary>
    uint32_t lightObjectId = 0; // +0x40
    /// <summary>Speed in world units per second (FIT "Velocity").</summary>
    float velocity = 0; // +0x44
    /// <summary>Distance from the target at which the smoke trail stops (FIT "CloseDistance").</summary>
    float closeDistance = 0; // +0x48
    /// <summary>The four colours used when the owner is friendly (FIT "f0Color".."f3Color").</summary>
    uint8_t fColor[4]{}; // +0x4c
    /// <summary>The four colours used when the owner is an enemy (FIT "e0Color".."e3Color").</summary>
    uint8_t eColor[4]{}; // +0x50
    /// <summary>Distance from the head to the tail (FIT "ProjectileLength").</summary>
    float projectileLength = 0; // +0x54
    /// <summary>Distance from the head to the bulge (FIT "BulgeLength").</summary>
    float bulgeLength = 0; // +0x58
    /// <summary>Half width of the bulge (FIT "BulgeWidth").</summary>
    float bulgeWidth = 0; // +0x5c
};

/// <summary>
/// A pulse laser shot: a coloured diamond (head, two bulge points, tail) that flies from its owner's hot spot to its
/// target like a <see cref="Bullet"/>, with a smoke trail and a light, and applies its shot on arrival.
/// </summary>
/// <remarks>Original source: <c>object\prjlase.cpp</c>, <c>object\prjlase.h</c>; 0x130 bytes.</remarks>
class ProjectileLaser : public BigGameObject
{
public:
    /// <summary>
    /// Zeroes the pointers and smoke displacement, sets justCreated and makes the frame the world frame (inline in
    /// ProjectileLaserType::createInstance).
    /// </summary>
    ProjectileLaser();
    /// <remarks>MCX.EXE @ 0x006916a0 (vector deleting destructor)</remarks>
    ~ProjectileLaser() override { destroy(); }

    /// <summary>Empty in the original.</summary>
    /// <remarks>MCX.EXE @ 0x006915b0 (inline in <c>object\prjlase.h</c>)</remarks>
    void init() override;
    /// <summary>Makes the arm appearance and the smoke and light objects of the type; class 0x1a.</summary>
    /// <remarks>MCX.EXE @ 0x00692b30</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Frees the target position and destroys the appearance, smoke and light.</summary>
    /// <remarks>MCX.EXE @ 0x00692ac0</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x00691680</remarks>
    int32_t kill() override { return 0; }
    /// <summary>
    /// Moves the head toward the target and rebuilds the bulge and tail points; once it stops getting closer applies
    /// the shot and creates the hit (or miss) effect, setting off a mine under a miss.
    /// </summary>
    /// <returns>1 while flying, 0 when done.</returns>
    /// <remarks>MCX.EXE @ 0x00691a40</remarks>
    int32_t update() override;
    /// <summary>Draws the shot as polygons in the type's friendly or enemy colours, then the smoke and light.</summary>
    /// <remarks>MCX.EXE @ 0x006923d0</remarks>
    void render() override;
    /// <remarks>MCX.EXE @ 0x006915c0 (inline in <c>object\prjlase.h</c>)</remarks>
    frame_of_ref getFrame() override { return frame; }
    /// <remarks>MCX.EXE @ 0x00691620 (inline in <c>object\prjlase.h</c>)</remarks>
    void setFrame(frame_of_ref& newFrame) override { frame = newFrame; }

    /// <summary>Always null (the vtable's slot 116; unnamed in the symbols: xor eax,eax / ret).</summary>
    /// <remarks>MCX.EXE @ 0x00691690</remarks>
    virtual Appearance* getAppearancePtr() { return nullptr; }

    /// <summary>Projects the shot to the screen; true when its appearance is visible to the main camera.</summary>
    /// <remarks>MCX.EXE @ 0x00691990</remarks>
    int isVisible();
    /// <remarks>MCX.EXE @ 0x00692c60</remarks>
    void setOwner(BaseObject* newOwner);
    /// <summary>Sets (allocating it the first time) the position the shot flies to.</summary>
    /// <remarks>MCX.EXE @ 0x00692c80</remarks>
    void setTargetPosition(vector_3d position);
    /// <summary>
    /// Sets the owner and the hot spot it fires from, the target position, and copies <paramref name="shotInfo"/>
    /// (if any) as the shot to apply.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0065f660 (inline in <c>object\prjlase.h</c>)</remarks>
    void connect(GameObject* source, vector_3d targetPos, _WeaponShotInfo* shotInfo, int32_t sourceHotSpot);

    /// <summary>Set by the constructor and init; the first update clears it, places the shot and plays the sound.</summary>
    int32_t justCreated = 0; // +0x84
    /// <summary>The object that fired (stored as a BaseObject by <see cref="setOwner"/>).</summary>
    GameObject* owner = nullptr; // +0x88
    /// <summary>The owner's hot spot the shot leaves from.</summary>
    int32_t ownerHotSpot = 0; // +0x8c
    /// <summary>The object the shot flies to and damages; null for a shot at a position.</summary>
    GameObject* target = nullptr; // +0x90
    /// <summary>The target's hot spot where the hit effect is placed (not used for class 0x1e targets).</summary>
    int32_t targetHotSpot = 0; // +0x94
    /// <summary>Where the shot flies to, allocated by <see cref="setTargetPosition"/>.</summary>
    vector_3d* targetPosition = nullptr; // +0x98
    /// <summary>The smallest squared ground distance to the target so far (starts at 1e8); growing again means arrival.</summary>
    float closestDistanceSq = 0; // +0x9c
    /// <summary>The arm appearance.</summary>
    Appearance* appearance = nullptr; // +0xa0
    /// <summary>The shot applied to the target on arrival.</summary>
    _WeaponShotInfo shotInfo{}; // +0xa4
    /// <summary>The head of the shot: its own flight position.</summary>
    vector_3d headPosition; // +0xb8
    /// <summary>One side point of the bulge (bulgeLength behind the head, bulgeWidth to one side).</summary>
    vector_3d bulgeSide1; // +0xc4
    /// <summary>The other side point of the bulge.</summary>
    vector_3d bulgeSide2; // +0xd0
    /// <summary>The tail (projectileLength behind the head); the smoke and light follow it.</summary>
    vector_3d tailPosition; // +0xdc
    /// <summary>The centre of the bulge (bulgeLength behind the head).</summary>
    vector_3d bulgeCenter; // +0xe8
    /// <summary>The smoke trail.</summary>
    Smoke* smoke = nullptr; // +0xf4
    /// <summary>The total the smoke has been moved by (accumulated each update; not read in prjlase.cpp).</summary>
    vector_3d smokeDisplacement; // +0xf8
    /// <summary>Orientation, returned by <see cref="getFrame"/>; the owner's frame on the first update.</summary>
    frame_of_ref frame; // +0x104
    /// <summary>The light travelling with the shot.</summary>
    GameObject* light = nullptr; // +0x128
    /// <summary>The draw rotation of the appearance: -150, or 150 when a mech owner faces the other way.</summary>
    int32_t drawRotation = 0; // +0x12c
};
