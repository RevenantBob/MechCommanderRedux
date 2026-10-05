#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"
#include "object/smokmgr.h"

class Appearance;
class BaseObject;
class File;
class GameObject;

/// <summary>
/// The type of a <see cref="Smoke"/>: how fast and how long it puffs, how its spheres move and spread, and the
/// shape they are drawn with.
/// </summary>
/// <remarks>
/// Original source: <c>object\smoke.cpp</c>, 0x6c bytes. Read from the "SmokeData" block of its FIT.
/// </remarks>
class SmokeType : public ObjectType
{
public:
    /// <summary>Zeroes the smoke data (all but maxSmokeSpheres, numRotations and frameRate).</summary>
    /// <remarks>Inline in ObjectTypeManager::load (ObjectType::init runs again first).</remarks>
    SmokeType()
    {
        ObjectType::init();
        randomPosZ = 0.0f;
        randomPosY = 0.0f;
        randomPosX = 0.0f;
        zVelocity = 0.0f;
        slowDownPercent = 0.0f;
        smokePerSecond = 0.0f;
        randomVelZ = 0.0f;
        randomVelY = 0.0f;
        randomVelX = 0.0f;
        duration = 0;
        smokeShape = nullptr;
        hasRotation = 0;
    }

    /// <remarks>MCX.EXE @ 0x006905f0 (vector deleting destructor)</remarks>
    ~SmokeType() override { destroy(); }

    /// <summary>Makes a <see cref="Smoke"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x00693630</remarks>
    BaseObject* createInstance() override;
    /// <summary>Frees the shape from the smoke manager's sphere blocks.</summary>
    /// <remarks>MCX.EXE @ 0x00693760</remarks>
    void destroy() override;
    /// <summary>Reads the "SmokeData" block and loads the smoke shape into the smoke manager's sphere blocks.</summary>
    /// <remarks>MCX.EXE @ 0x00693780</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <remarks>MCX.EXE @ 0x00693b30</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <remarks>MCX.EXE @ 0x00693b40</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>FIT "zVelocity": the spheres' rising speed, stored in world units per second.</summary>
    float zVelocity = 0; // +0x30
    /// <summary>FIT "SmokePerSecond": spheres made per second.</summary>
    float smokePerSecond = 0; // +0x34
    /// <summary>FIT "SlowDownPercent": the fraction of the owner's velocity a new sphere keeps.</summary>
    float slowDownPercent = 0; // +0x38
    /// <summary>FIT "MaxSmokeSpheres": spheres per smoke.</summary>
    uint32_t maxSmokeSpheres = 0; // +0x3c
    /// <summary>FIT "SmokeShape": the VFX shape file, in the smoke manager's sphere blocks.</summary>
    uint8_t* smokeShape = nullptr; // +0x40
    /// <summary>FIT "Duration", seconds of puffing.</summary>
    int32_t duration = 0; // +0x44
    /// <summary>FIT "randomVelX/Y/Z": random spread added to a new sphere's velocity.</summary>
    float randomVelX = 0; // +0x48
    float randomVelY = 0; // +0x4c
    float randomVelZ = 0; // +0x50
    /// <summary>FIT "randomPosX/Y/Z": random spread added to a new sphere's position.</summary>
    float randomPosX = 0; // +0x54
    float randomPosY = 0; // +0x58
    float randomPosZ = 0; // +0x5c
    /// <summary>FIT "HasRotation": the shape holds numRotations facings; spheres never settle on the ground.</summary>
    int32_t hasRotation = 0; // +0x60
    /// <summary>FIT "NumRotations".</summary>
    int32_t numRotations = 0; // +0x64
    /// <summary>FIT "FrameRate" (default 15), frames per second.</summary>
    float frameRate = 0; // +0x68
};

/// <summary>
/// A smoke plume: puffs spheres from its owner's hot spot (or a set position) for the type's duration, then lives
/// on until the last sphere has faded.
/// </summary>
/// <remarks>
/// Original source: <c>object\smoke.cpp</c>, <c>object\smoke.h</c>; 0xb4 bytes.
/// </remarks>
class Smoke : public BigGameObject
{
public:
    /// <summary>Calls <see cref="init()"/> and zeroes the smoke's fields.</summary>
    /// <remarks>Inline in SmokeType::createInstance (MCX.EXE @ 0x00693630).</remarks>
    Smoke();
    /// <remarks>MCX.EXE @ 0x00693710 (vector deleting destructor)</remarks>
    ~Smoke() override { destroy(); }

    /// <summary>Does nothing.</summary>
    /// <remarks>MCX.EXE @ 0x006936e0 (inline in <c>object\smoke.h</c>)</remarks>
    void init() override;
    /// <summary>Takes the spheres from the smoke manager and resets them.</summary>
    /// <remarks>MCX.EXE @ 0x00694940</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Frees the owner position and velocity and gives the spheres back.</summary>
    /// <remarks>MCX.EXE @ 0x00694610 (unnamed in the symbols; vtable slot 2 of Smoke)</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x006936f0</remarks>
    int32_t kill() override { return 0; }
    /// <summary>Puffs new spheres while the duration lasts and moves the live ones.</summary>
    /// <returns>0 once the duration is over and no sphere is left.</returns>
    /// <remarks>MCX.EXE @ 0x00693e80</remarks>
    int32_t update() override;
    /// <summary>Animates and draws the live spheres.</summary>
    /// <remarks>MCX.EXE @ 0x00694110</remarks>
    void render() override;

    /// <remarks>MCX.EXE @ 0x00693700</remarks>
    virtual Appearance* getAppearancePtr() { return nullptr; }

    /// <summary>Ends the puffing now (the spheres already out fade on).</summary>
    /// <remarks>MCX.EXE @ 0x00693bb0</remarks>
    void stopSmoking();
    /// <summary>Restarts the puffing for a new duration, clearing the spheres.</summary>
    /// <remarks>MCX.EXE @ 0x00693bd0</remarks>
    void startSmoking();
    /// <summary>Projects sphere <paramref name="sphereIndex"/> to the screen; true when it is in the camera's view.</summary>
    /// <remarks>MCX.EXE @ 0x00693c00</remarks>
    int isVisible(int32_t sphereIndex);
    /// <summary>Starts the next sphere at the owner's position, with part of its velocity plus the random spreads.</summary>
    /// <remarks>MCX.EXE @ 0x00694680</remarks>
    void newSmokeSphere();
    /// <summary>The object the smoke follows.</summary>
    /// <remarks>MCX.EXE @ 0x006949f0</remarks>
    void setOwner(BaseObject* owner);
    /// <summary>Where new spheres start.</summary>
    /// <remarks>MCX.EXE @ 0x00694a00</remarks>
    void setOwnerPosition(vector_3d position);
    /// <summary>The velocity new spheres inherit (scaled by the type's slow-down percent).</summary>
    /// <remarks>MCX.EXE @ 0x00694a40</remarks>
    void setOwnerVelocity(vector_3d velocity);

    /// <summary>Set by init and startSmoking; the next update starts the duration.</summary>
    int32_t justStarted = 0; // +0x84
    /// <summary>Tick count (ms) the puffing ends.</summary>
    uint32_t endTime = 0; // +0x88
    /// <summary>Tick count (ms) of the next sphere.</summary>
    uint32_t nextSphereTime = 0; // +0x8c
    /// <summary>The sphere the next puff reuses (cycles through all of them).</summary>
    int32_t nextSphere = 0; // +0x90
    /// <summary>The object the smoke follows, if any.</summary>
    BaseObject* owner = nullptr; // +0x94
    /// <summary>The owner's hot spot the smoke comes from (set by BattleMech::update).</summary>
    uint32_t ownerHotSpot = 0; // +0x98
    /// <summary>Where new spheres start (allocated by setOwnerPosition).</summary>
    vector_3d* ownerPosition = nullptr; // +0x9c
    /// <summary>The owner's velocity (allocated by setOwnerVelocity).</summary>
    vector_3d* ownerVelocity = nullptr; // +0xa0
    /// <summary>The spheres, from the smoke manager.</summary>
    SmokeSphere* spheres = nullptr; // +0xa4
    /// <summary>How many spheres the smoke has.</summary>
    int32_t numSpheres = 0; // +0xac
    /// <summary>
    /// Added to the render group's depth (the group is openGroup(depthBias - screen y)): -200 by init(ObjectType*),
    /// -50 for a mech's equipment smoke (BattleMech::update), the flame's draw rotation for a jet's (Jet::render).
    /// </summary>
    int32_t depthBias = 0; // +0xb0
};

/// <summary>The scenario's smoke manager.</summary>
extern SmokeManager* smokeManager;
