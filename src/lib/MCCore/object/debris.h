#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class MCAppearance;
class MCFile;
class MCGameObject;

/// <summary>The type of a piece of <see cref="MCDebris"/> (a mech's falling arm): how it is thrown and slows down.</summary>
/// <remarks>Original source: <c>object\debris.cpp</c>, 0x44 bytes. Read from the "ArmFall" block of its FIT.</remarks>
class MCDebrisType : public MCObjectType
{
public:
    /// <summary>Zeroes the five ArmFall values.</summary>
    /// <remarks>Inline in ObjectTypeManager::load (case 9).</remarks>
    MCDebrisType();
    ~MCDebrisType() override { Destroy(); }

    /// <summary>Makes a <see cref="MCDebris"/> of this type and gives it the next object id.</summary>
    MCBaseObject* CreateInstance() override;
    void Destroy() override;
    /// <summary>Reads the "ArmFall" block, then the common type data.</summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>Base yaw added by <see cref="MCDebris::RandomAngle"/> (FIT "ArmFallYaw").</summary>
    float ArmFallYaw = 0;
    /// <summary>Random spread of the yaw (FIT "ArmFallYawRange").</summary>
    float ArmFallYawRange = 0;
    /// <summary>Base speed the debris is thrown with (FIT "ArmFallVelMag").</summary>
    float ArmFallVelMag = 0;
    /// <summary>Random spread of the speed (FIT "ArmFallVelRange").</summary>
    float ArmFallVelRange = 0;
    /// <summary>Deceleration while it slides after the fall animation (FIT "ArmFallDecelRate").</summary>
    float ArmFallDecelRate = 0;
};

/// <summary>
/// A piece of debris (a mech's arm) thrown off when it is destroyed: an arm appearance that flies along its velocity,
/// then slows to a stop once its fall animation has finished.
/// </summary>
/// <remarks>Original source: <c>object\debris.cpp</c>, <c>object\debris.h</c>; 0xd0 bytes.</remarks>
class MCDebris : public MCBigGameObject
{
public:
    /// <summary>
    /// Sets justCreated and visible, zeroes the rest, and makes the frame the identity (inline in
    /// DebrisType::createInstance).
    /// </summary>
    MCDebris();
    ~MCDebris() override { Destroy(); }

    /// <summary>Empty in the original.</summary>
    void Init() override;
    /// <summary>Makes the arm appearance.</summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Destroys the appearance.</summary>
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>
    /// On the first update scales the velocity by the type's speed (plus a random part); then moves, and once the
    /// animation is done slows down at the type's deceleration until stopped.
    /// </summary>
    int32_t Update() override;
    void Render() override;
    MCAppearance* GetAppearance() override { return Appearance; }
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
    /// <summary>Sets the arm appearance's paint scheme (its field at +0x64).</summary>
    void SetPaintScheme(int32_t paintScheme);

    /// <summary>Set by the constructor and init; the first update clears it after scaling the velocity.</summary>
    int32_t JustCreated = 0;
    /// <summary>The arm appearance.</summary>
    MCAppearance* Appearance = nullptr;
    /// <summary>The last onScreen result, passed to the appearance's update.</summary>
    int32_t Visible = 0;
    /// <summary>The type's armFallDecelRate, copied on the first update.</summary>
    float DecelRate = 0;
    /// <summary>Set once the appearance's animation has finished: the debris then slows down.</summary>
    int32_t FallDone = 0;
    /// <summary>Set once the debris has slowed to a stop: it no longer moves.</summary>
    int32_t Stopped = 0;
    /// <summary>Velocity in meters per second (converted with worldUnitsPerMeter when moving).</summary>
    MCVector3D Velocity;
    /// <summary>Orientation, returned by <see cref="GetFrame"/>.</summary>
    MCFrameOfRef Frame;
};
