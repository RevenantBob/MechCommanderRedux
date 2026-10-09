#pragma once

#include "object/MCObjectType.h"

/// <summary>The type of a piece of <see cref="MCDebris"/> (a mech's falling arm): how it is thrown and slows down.</summary>
/// <remarks>Original source: <c>object\debris.cpp</c>. Read from the "ArmFall" block of its FIT.</remarks>
class MCDebrisType : public MCObjectType
{
public:
    /// <summary>Makes a <see cref="MCDebris"/> of this type and gives it the next object id.</summary>
    std::unique_ptr<MCBaseObject> CreateInstance() override;
    /// <summary>Reads the "ArmFall" block, then the common type data.</summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override { return 0; }
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override { return 0; }

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
