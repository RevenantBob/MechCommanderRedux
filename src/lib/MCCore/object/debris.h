#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class Appearance;
class File;
class GameObject;

/// <summary>The type of a piece of <see cref="Debris"/> (a mech's falling arm): how it is thrown and slows down.</summary>
/// <remarks>Original source: <c>object\debris.cpp</c>, 0x44 bytes. Read from the "ArmFall" block of its FIT.</remarks>
class DebrisType : public ObjectType
{
public:
    /// <summary>Zeroes the five ArmFall values.</summary>
    /// <remarks>Inline in ObjectTypeManager::load (case 9).</remarks>
    DebrisType();
    /// <remarks>MCX.EXE @ 0x00690740 (vector deleting destructor)</remarks>
    ~DebrisType() override { destroy(); }

    /// <summary>Makes a <see cref="Debris"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x00659ae0</remarks>
    BaseObject* createInstance() override;
    /// <remarks>MCX.EXE @ 0x00659d70</remarks>
    void destroy() override;
    /// <summary>Reads the "ArmFall" block, then the common type data.</summary>
    /// <remarks>MCX.EXE @ 0x00659d80</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <remarks>MCX.EXE @ 0x00659ed0</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <remarks>MCX.EXE @ 0x00659ee0</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>Base yaw added by <see cref="Debris::randomAngle"/> (FIT "ArmFallYaw").</summary>
    float armFallYaw = 0; // +0x30
    /// <summary>Random spread of the yaw (FIT "ArmFallYawRange").</summary>
    float armFallYawRange = 0; // +0x34
    /// <summary>Base speed the debris is thrown with (FIT "ArmFallVelMag").</summary>
    float armFallVelMag = 0; // +0x38
    /// <summary>Random spread of the speed (FIT "ArmFallVelRange").</summary>
    float armFallVelRange = 0; // +0x3c
    /// <summary>Deceleration while it slides after the fall animation (FIT "ArmFallDecelRate").</summary>
    float armFallDecelRate = 0; // +0x40
};

/// <summary>
/// A piece of debris (a mech's arm) thrown off when it is destroyed: an arm appearance that flies along its velocity,
/// then slows to a stop once its fall animation has finished.
/// </summary>
/// <remarks>Original source: <c>object\debris.cpp</c>, <c>object\debris.h</c>; 0xd0 bytes.</remarks>
class Debris : public BigGameObject
{
public:
    /// <summary>
    /// Sets justCreated and visible, zeroes the rest, and makes the frame the identity (inline in
    /// DebrisType::createInstance).
    /// </summary>
    Debris();
    /// <remarks>MCX.EXE @ 0x00659d20 (vector deleting destructor)</remarks>
    ~Debris() override { destroy(); }

    /// <summary>Empty in the original.</summary>
    /// <remarks>MCX.EXE @ 0x00659bc0 (inline in <c>object\debris.h</c>)</remarks>
    void init() override;
    /// <summary>Makes the arm appearance.</summary>
    /// <remarks>MCX.EXE @ 0x0065a280</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Destroys the appearance.</summary>
    /// <remarks>MCX.EXE @ 0x0065a260</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x00659d10</remarks>
    int32_t kill() override { return 0; }
    /// <summary>
    /// On the first update scales the velocity by the type's speed (plus a random part); then moves, and once the
    /// animation is done slows down at the type's deceleration until stopped.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00659fa0</remarks>
    int32_t update() override;
    /// <remarks>MCX.EXE @ 0x0065a220</remarks>
    void render() override;
    /// <remarks>MCX.EXE @ 0x00659d00</remarks>
    Appearance* getAppearance() override { return appearance; }
    /// <summary>Projects the debris to the screen; true when its appearance is visible to the main camera.</summary>
    /// <remarks>MCX.EXE @ 0x00659ef0</remarks>
    int onScreen() override;
    /// <remarks>MCX.EXE @ 0x00659bd0 (inline in <c>object\debris.h</c>)</remarks>
    vector_3d getVelocity() override { return velocity; }
    /// <remarks>MCX.EXE @ 0x00659c10 (inline in <c>object\debris.h</c>)</remarks>
    void setVelocity(vector_3d& newVelocity) override { velocity = newVelocity; }
    /// <remarks>MCX.EXE @ 0x00659c40 (inline in <c>object\debris.h</c>)</remarks>
    frame_of_ref getFrame() override { return frame; }
    /// <remarks>MCX.EXE @ 0x00659ca0 (inline in <c>object\debris.h</c>)</remarks>
    void setFrame(frame_of_ref& newFrame) override { frame = newFrame; }

    /// <summary>
    /// Adds the type's yaw to <paramref name="angle"/>, then a random amount (of the yaw range) in a random
    /// direction.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0065a1b0</remarks>
    void randomAngle(float& angle);
    /// <summary>Sets the arm appearance's paint scheme (its field at +0x64).</summary>
    /// <remarks>MCX.EXE @ 0x0065a370</remarks>
    void setPaintScheme(int32_t paintScheme);

    /// <summary>Set by the constructor and init; the first update clears it after scaling the velocity.</summary>
    int32_t justCreated = 0; // +0x84
    /// <summary>The arm appearance.</summary>
    Appearance* appearance = nullptr; // +0x88
    /// <summary>The last onScreen result, passed to the appearance's update.</summary>
    int32_t visible = 0; // +0x8c
    /// <summary>The type's armFallDecelRate, copied on the first update.</summary>
    float decelRate = 0; // +0x94
    /// <summary>Set once the appearance's animation has finished: the debris then slows down.</summary>
    int32_t fallDone = 0; // +0x98
    /// <summary>Set once the debris has slowed to a stop: it no longer moves.</summary>
    int32_t stopped = 0; // +0x9c
    /// <summary>Velocity in meters per second (converted with worldUnitsPerMeter when moving).</summary>
    vector_3d velocity; // +0xa0
    /// <summary>Orientation, returned by <see cref="getFrame"/>.</summary>
    frame_of_ref frame; // +0xac
};
