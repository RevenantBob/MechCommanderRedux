#pragma once

#include "object/MCBigGameObject.h"
#include "object/MCWeaponShotInfo.h"

class MCArmAppearance;
class MCSmoke;

/// <summary>
/// A pulse laser shot: a coloured diamond (head, two bulge points, tail) that flies from its owner's hot spot to its
/// target like a <see cref="MCBullet"/>, with a smoke trail and a light, and applies its shot on arrival.
/// </summary>
/// <remarks>Original source: <c>object\prjlase.cpp</c>, <c>object\prjlase.h</c>.</remarks>
class MCProjectileLaser : public MCBigGameObject
{
public:
    MCProjectileLaser();
    ~MCProjectileLaser() override;

    /// <summary>Makes the arm appearance and the smoke and light objects of the type; object class ProjectileLaser.</summary>
    int32_t Init(MCObjectType* objType) override;
    using MCBigGameObject::Init;
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

    /// <summary>Projects the shot to the screen; true when its appearance is visible to the main camera.</summary>
    bool IsVisible();
    void SetOwner(MCBaseObject* newOwner) { Owner = static_cast<MCGameObject*>(newOwner); }
    /// <summary>Sets the position the shot flies to.</summary>
    void SetTargetPosition(MCVector3D position) { TargetPosition = position; }
    /// <summary>
    /// Sets the owner and the hot spot it fires from, the target position, and copies <paramref name="shotInfo"/>
    /// (if any) as the shot to apply.
    /// </summary>
    void Connect(MCGameObject* source, MCVector3D targetPos, MCWeaponShotInfo* shotInfo, int32_t sourceHotSpot);

    /// <summary>Set until the first update, which places the shot and plays the sound.</summary>
    bool JustCreated = true;
    /// <summary>The object that fired.</summary>
    MCGameObject* Owner = nullptr;
    /// <summary>The owner's hot spot the shot leaves from.</summary>
    int32_t OwnerHotSpot = 0;
    /// <summary>The object the shot flies to and damages; null for a shot at a position.</summary>
    MCGameObject* Target = nullptr;
    /// <summary>The target's hot spot where the hit effect is placed (not used for turret targets).</summary>
    int32_t TargetHotSpot = 0;
    /// <summary>Where the shot flies to, once set.</summary>
    std::optional<MCVector3D> TargetPosition;
    /// <summary>The smallest squared ground distance to the target so far (starts at 1e8); growing again means arrival.</summary>
    float ClosestDistanceSq = 0;
    /// <summary>The arm appearance.</summary>
    std::unique_ptr<MCArmAppearance> Appearance;
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
    std::unique_ptr<MCSmoke> Smoke;
    /// <summary>The total the smoke has been moved by (accumulated each update; nothing reads it).</summary>
    MCVector3D SmokeDisplacement;
    /// <summary>Orientation, returned by <see cref="GetFrame"/>; the owner's frame on the first update.</summary>
    MCFrameOfRef Frame;
    /// <summary>The light travelling with the shot.</summary>
    std::unique_ptr<MCGameObject> Light;
    /// <summary>The draw rotation of the appearance: -150, or 150 when a mech owner faces the other way.</summary>
    int32_t DrawRotation = 0;
};
