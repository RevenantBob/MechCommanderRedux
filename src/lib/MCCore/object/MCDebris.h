#pragma once

#include "object/MCBigGameObject.h"

class MCArmAppearance;

/// <summary>
/// A piece of debris (a mech's arm) thrown off when it is destroyed: an arm appearance that flies along its velocity,
/// then slows to a stop once its fall animation has finished.
/// </summary>
/// <remarks>Original source: <c>object\debris.cpp</c>, <c>object\debris.h</c>.</remarks>
class MCDebris : public MCBigGameObject
{
public:
    /// <summary>Starts in the world frame, visible.</summary>
    MCDebris();
    ~MCDebris() override;

    /// <summary>Makes the arm appearance.</summary>
    int32_t Init(MCObjectType* objType) override;
    using MCBigGameObject::Init;
    int32_t Kill() override { return 0; }
    /// <summary>
    /// On the first update scales the velocity by the type's speed (plus a random part); then moves, and once the
    /// animation is done slows down at the type's deceleration until stopped.
    /// </summary>
    int32_t Update() override;
    void Render() override;
    MCAppearance* GetAppearance() override;
    /// <summary>Projects the debris to the screen; true when its appearance is visible to the main camera.</summary>
    int OnScreen() override;
    MCVector3D GetVelocity() override { return Velocity; }
    void SetVelocity(MCVector3D& newVelocity) override { Velocity = newVelocity; }
    MCFrameOfRef GetFrame() override { return Frame; }
    void SetFrame(MCFrameOfRef& newFrame) override { Frame = newFrame; }

    /// <summary>
    /// Adds the type's yaw to <paramref name="angle"/>, then a random amount (of the yaw range) in a random
    /// direction.
    /// </summary>
    void RandomAngle(float& angle);
    /// <summary>Sets the arm appearance's paint scheme.</summary>
    void SetPaintScheme(int32_t paintScheme) const;

    /// <summary>Set until the first update, which scales the velocity.</summary>
    bool JustCreated = true;
    /// <summary>The arm appearance.</summary>
    std::unique_ptr<MCArmAppearance> Appearance;
    /// <summary>The last onScreen result, passed to the appearance's update.</summary>
    int32_t Visible = 1;
    /// <summary>The type's armFallDecelRate, copied on the first update.</summary>
    float DecelRate = 0;
    /// <summary>Set once the appearance's animation has finished: the debris then slows down.</summary>
    bool FallDone = false;
    /// <summary>Set once the debris has slowed to a stop: it no longer moves.</summary>
    bool Stopped = false;
    /// <summary>Velocity in meters per second (converted with worldUnitsPerMeter when moving).</summary>
    MCVector3D Velocity;
    /// <summary>Orientation, returned by <see cref="GetFrame"/>.</summary>
    MCFrameOfRef Frame;
};
