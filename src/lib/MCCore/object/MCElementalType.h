#pragma once

#include "object/MCObjectType.h"

class MCDynamicsType;
class MCFile;

/// <summary>An elemental type: armoured infantry (jumping elementals) or marines (who can't jump).</summary>
/// <remarks>Original source: <c>object\elemntl.cpp</c>, <c>object\elemntl.h</c>.</remarks>
class MCElementalType : public MCObjectType
{
public:
    MCElementalType();
    ~MCElementalType() override;

    /// <summary>
    /// Reads the elemental file ("ElementalType"): "General" (id, can jump, alignment, name, max health),
    /// "Dynamics" (type 3), then the common type data.
    /// </summary>
    /// <returns>0, -1 for the wrong file type, -0x5fffd for the wrong dynamics type, or the FIT error.</returns>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>Makes an <see cref="MCElemental"/> of this type and gives it the next object id.</summary>
    std::unique_ptr<MCBaseObject> CreateInstance() override;
    /// <summary>
    /// Knocks the elemental aside: an enemy mech or vehicle ramming it (a marine: any), a building (with damage by its
    /// tonnage), a tree, a train car.
    /// </summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    /// <summary>Kills the elemental: disables its sensor, alarms its pilot, takes it off the interface and sets the
    /// friendly/enemy destroyed flag.</summary>
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>"ID".</summary>
    uint32_t ElementalId = 0;
    /// <summary>"Name".</summary>
    std::string Name;
    /// <summary>"Type", mapped 0 -> 1, 1 -> 0xff: the alignment (compared with the home team's id).</summary>
    uint8_t Alignment = 0;
    /// <summary>"MaxHealth".</summary>
    uint8_t MaxHealth = 0;
    /// <summary>The dynamics type (an ElementalDynamicsType).</summary>
    std::unique_ptr<MCDynamicsType> DynamicsType;
    /// <summary>"CanJump" (true when missing): elementals jump; marines don't.</summary>
    bool CanJump = true;
};
