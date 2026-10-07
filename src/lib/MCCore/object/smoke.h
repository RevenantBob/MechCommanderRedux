#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"
#include "object/smokmgr.h"

class MCAppearance;
class MCBaseObject;
class MCFile;
class MCGameObject;

/// <summary>
/// The type of a <see cref="MCSmoke"/>: how fast and how long it puffs, how its spheres move and spread, and the
/// shape they are drawn with.
/// </summary>
/// <remarks>
/// Original source: <c>object\smoke.cpp</c>, 0x6c bytes. Read from the "SmokeData" block of its FIT.
/// </remarks>
class MCSmokeType : public MCObjectType
{
public:
    /// <summary>Zeroes the smoke data (all but maxSmokeSpheres, numRotations and frameRate).</summary>
    /// <remarks>Inline in ObjectTypeManager::load (ObjectType::init runs again first).</remarks>
    MCSmokeType()
    {
        MCObjectType::Init();
        RandomPosZ = 0.0f;
        RandomPosY = 0.0f;
        RandomPosX = 0.0f;
        ZVelocity = 0.0f;
        SlowDownPercent = 0.0f;
        SmokePerSecond = 0.0f;
        RandomVelZ = 0.0f;
        RandomVelY = 0.0f;
        RandomVelX = 0.0f;
        Duration = 0;
        SmokeShape = nullptr;
        HasRotation = 0;
    }

    ~MCSmokeType() override { Destroy(); }

    /// <summary>Makes a <see cref="MCSmoke"/> of this type and gives it the next object id.</summary>
    MCBaseObject* CreateInstance() override;
    /// <summary>Frees the shape from the smoke manager's sphere blocks.</summary>
    void Destroy() override;
    /// <summary>Reads the "SmokeData" block and loads the smoke shape into the smoke manager's sphere blocks.</summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>FIT "zVelocity": the spheres' rising speed, stored in world units per second.</summary>
    float ZVelocity = 0;
    /// <summary>FIT "SmokePerSecond": spheres made per second.</summary>
    float SmokePerSecond = 0;
    /// <summary>FIT "SlowDownPercent": the fraction of the owner's velocity a new sphere keeps.</summary>
    float SlowDownPercent = 0;
    /// <summary>FIT "MaxSmokeSpheres": spheres per smoke.</summary>
    uint32_t MaxSmokeSpheres = 0;
    /// <summary>FIT "SmokeShape": the VFX shape file, in the smoke manager's sphere blocks.</summary>
    uint8_t* SmokeShape = nullptr;
    /// <summary>FIT "Duration", seconds of puffing.</summary>
    int32_t Duration = 0;
    /// <summary>FIT "randomVelX/Y/Z": random spread added to a new sphere's velocity.</summary>
    float RandomVelX = 0;
    float RandomVelY = 0;
    float RandomVelZ = 0;
    /// <summary>FIT "randomPosX/Y/Z": random spread added to a new sphere's position.</summary>
    float RandomPosX = 0;
    float RandomPosY = 0;
    float RandomPosZ = 0;
    /// <summary>FIT "HasRotation": the shape holds numRotations facings; spheres never settle on the ground.</summary>
    int32_t HasRotation = 0;
    /// <summary>FIT "NumRotations".</summary>
    int32_t NumRotations = 0;
    /// <summary>FIT "FrameRate" (default 15), frames per second.</summary>
    float FrameRate = 0;
};

/// <summary>
/// A smoke plume: puffs spheres from its owner's hot spot (or a set position) for the type's duration, then lives
/// on until the last sphere has faded.
/// </summary>
/// <remarks>
/// Original source: <c>object\smoke.cpp</c>, <c>object\smoke.h</c>; 0xb4 bytes.
/// </remarks>
class MCSmoke : public MCBigGameObject
{
public:
    /// <summary>Calls <see cref="init()"/> and zeroes the smoke's fields.</summary>
    /// <remarks>Inline in SmokeType::createInstance.</remarks>
    MCSmoke();
    ~MCSmoke() override { Destroy(); }

    /// <summary>Does nothing.</summary>
    void Init() override;
    /// <summary>Takes the spheres from the smoke manager and resets them.</summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Frees the owner position and velocity and gives the spheres back.</summary>
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>Puffs new spheres while the duration lasts and moves the live ones.</summary>
    /// <returns>0 once the duration is over and no sphere is left.</returns>
    int32_t Update() override;
    /// <summary>Animates and draws the live spheres.</summary>
    void Render() override;

    virtual MCAppearance* GetAppearancePtr() { return nullptr; }

    /// <summary>Ends the puffing now (the spheres already out fade on).</summary>
    void StopSmoking();
    /// <summary>Restarts the puffing for a new duration, clearing the spheres.</summary>
    void StartSmoking();
    /// <summary>Projects sphere <paramref name="sphereIndex"/> to the screen; true when it is in the camera's view.</summary>
    int IsVisible(int32_t sphereIndex);
    /// <summary>Starts the next sphere at the owner's position, with part of its velocity plus the random spreads.</summary>
    void NewSmokeSphere();
    /// <summary>The object the smoke follows.</summary>
    void SetOwner(MCBaseObject* owner);
    /// <summary>Where new spheres start.</summary>
    void SetOwnerPosition(MCVector3D position);
    /// <summary>The velocity new spheres inherit (scaled by the type's slow-down percent).</summary>
    void SetOwnerVelocity(MCVector3D velocity);

    /// <summary>Set by init and startSmoking; the next update starts the duration.</summary>
    int32_t JustStarted = 0;
    /// <summary>Tick count (ms) the puffing ends.</summary>
    uint32_t EndTime = 0;
    /// <summary>Tick count (ms) of the next sphere.</summary>
    uint32_t NextSphereTime = 0;
    /// <summary>The sphere the next puff reuses (cycles through all of them).</summary>
    int32_t NextSphere = 0;
    /// <summary>The object the smoke follows, if any.</summary>
    MCBaseObject* Owner = nullptr;
    /// <summary>The owner's hot spot the smoke comes from (set by BattleMech::update).</summary>
    uint32_t OwnerHotSpot = 0;
    /// <summary>Where new spheres start (allocated by setOwnerPosition).</summary>
    MCVector3D* OwnerPosition = nullptr;
    /// <summary>The owner's velocity (allocated by setOwnerVelocity).</summary>
    MCVector3D* OwnerVelocity = nullptr;
    /// <summary>The spheres, from the smoke manager.</summary>
    MCSmokeSphere* Spheres = nullptr;
    /// <summary>How many spheres the smoke has.</summary>
    int32_t NumSpheres = 0;
    /// <summary>
    /// Added to the render group's depth (the group is openGroup(depthBias - screen y)): -200 by init(ObjectType*),
    /// -50 for a mech's equipment smoke (BattleMech::update), the flame's draw rotation for a jet's (Jet::render).
    /// </summary>
    int32_t DepthBias = 0;
};

/// <summary>The scenario's smoke manager.</summary>
extern MCSmokeManager* SmokeManager;
