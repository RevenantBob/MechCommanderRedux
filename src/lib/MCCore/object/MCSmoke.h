#pragma once

#include "object/MCBigGameObject.h"

/// <summary>One puff of a <see cref="MCSmoke"/>: the smoke's shape drifting with its own velocity and animation.</summary>
/// <remarks>Original source: <c>object\smokmgr.cpp</c>, <c>object\smoke.cpp</c>.</remarks>
struct MCSmokeSphere
{
    /// <summary>World position.</summary>
    MCVector3D Position;
    /// <summary>World units per second.</summary>
    MCVector3D Velocity;
    /// <summary>Set while the puff is alive and on screen.</summary>
    bool Active = false;
    /// <summary>Screen position (from the last visibility check).</summary>
    float ScreenX = 0;
    float ScreenY = 0;
    /// <summary>The shape frame drawn.</summary>
    int32_t Frame = 0;
    /// <summary>floor(frameTime * frameRate) at the last frame advance.</summary>
    int32_t FrameCount = 0;
    /// <summary>Seconds the puff has been animating.</summary>
    float FrameTime = 0;
    /// <summary>Set once the puff has touched the ground (it then spreads sideways).</summary>
    bool OnGround = false;
};

/// <summary>
/// A smoke plume: puffs spheres from its owner's hot spot (or a set position) for the type's duration, then lives
/// on until the last sphere has faded. The object that makes it owns it and runs its update and render.
/// </summary>
/// <remarks>Original source: <c>object\smoke.cpp</c>, <c>object\smoke.h</c>.</remarks>
class MCSmoke : public MCBigGameObject
{
public:
    MCSmoke() = default;
    /// <summary>Gives the spheres back (to the mission's count, while there is one).</summary>
    ~MCSmoke() override;

    /// <summary>Takes the type's spheres; a type with none can't make a smoke.</summary>
    int32_t Init(MCObjectType* objType) override;
    using MCBigGameObject::Init;
    int32_t Kill() override { return 0; }
    /// <summary>Puffs new spheres while the duration lasts and moves the live ones.</summary>
    /// <returns>0 once the duration is over and no sphere is left.</returns>
    int32_t Update() override;
    /// <summary>Animates and draws the live spheres.</summary>
    void Render() override;

    /// <summary>Ends the puffing now (the spheres already out fade on).</summary>
    void StopSmoking();
    /// <summary>Projects sphere <paramref name="sphereIndex"/> to the screen; true when it is in the camera's view.</summary>
    bool IsVisible(size_t sphereIndex);
    /// <summary>Starts the next sphere at the owner's position, with part of its velocity plus the random spreads.</summary>
    void NewSmokeSphere();
    /// <summary>The object the smoke follows.</summary>
    void SetOwner(MCBaseObject* owner) { Owner = owner; }
    /// <summary>Where new spheres start.</summary>
    void SetOwnerPosition(MCVector3D position) { OwnerPosition = position; }
    /// <summary>The velocity new spheres inherit (scaled by the type's slow-down percent).</summary>
    void SetOwnerVelocity(MCVector3D velocity) { OwnerVelocity = velocity; }

    /// <summary>Set by init and startSmoking; the next update starts the duration.</summary>
    bool JustStarted = false;
    /// <summary>Tick count (ms) the puffing ends.</summary>
    uint32_t EndTime = 0;
    /// <summary>Tick count (ms) of the next sphere.</summary>
    uint32_t NextSphereTime = 0;
    /// <summary>The sphere the next puff reuses (cycles through all of them).</summary>
    size_t NextSphere = 0;
    /// <summary>The object the smoke follows, if any.</summary>
    MCBaseObject* Owner = nullptr;
    /// <summary>The owner's hot spot the smoke comes from (set by BattleMech::update).</summary>
    uint32_t OwnerHotSpot = 0;
    /// <summary>Where new spheres start, once set.</summary>
    std::optional<MCVector3D> OwnerPosition;
    /// <summary>The owner's velocity, once set.</summary>
    std::optional<MCVector3D> OwnerVelocity;
    /// <summary>The spheres (the type's MaxSmokeSpheres).</summary>
    std::vector<MCSmokeSphere> Spheres;
    /// <summary>
    /// Added to the render group's depth (the group is openGroup(depthBias - screen y)): -200 by init, -50 for a mech's
    /// equipment smoke (BattleMech::update), the flame's draw rotation for a jet's (Jet::render).
    /// </summary>
    int32_t DepthBias = 0;
};
