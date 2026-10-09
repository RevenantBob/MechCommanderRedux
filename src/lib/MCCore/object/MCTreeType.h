#pragma once

#include "object/MCObjectType.h"
#include "platform/MCRegisteredBlock.h"

/// <summary>
/// The type of a <see cref="MCTree"/>: the damage it takes, its explosion, and its normal and destroyed shadow shapes.
/// </summary>
/// <remarks>Original source: <c>object\tree.cpp</c>. Read from the "TreeData" block of its FIT.</remarks>
class MCTreeType : public MCObjectType
{
public:
    /// <summary>Makes a <see cref="MCTree"/> of this type and gives it the next object id.</summary>
    std::unique_ptr<MCBaseObject> CreateInstance() override;
    /// <summary>
    /// Reads the "TreeData" block and loads the NormalShadow and DestroyedShadow shape files, then the common type
    /// data.
    /// </summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>
    /// A mover running into a standing tree knocks it down: the tree is tilted away from the mover, plays its
    /// falling animation, and makes a crash sound.
    /// </summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override { return 0; }

    /// <summary>FIT "DmgLevel".</summary>
    uint32_t DmgLevel = 0;
    /// <summary>The NormalShadow shape file.</summary>
    MCRegisteredBlock NormalShadow;
    /// <summary>The DestroyedShadow shape file.</summary>
    MCRegisteredBlock DestroyedShadow;
    /// <summary>FIT "ExplosionDamage".</summary>
    float ExplosionDamage = 0;
    /// <summary>FIT "ExplosionRadius".</summary>
    float ExplosionRadius = 0;
};

/// <summary>
/// Loads the shadow shape named by FIT entry <paramref name="entry"/> (a .shp in the sprite path) into
/// <paramref name="shadow"/>. No entry leaves it empty and succeeds; a file that won't open returns its error.
/// </summary>
int32_t LoadShadowShape(MCFitIniFile& typeFile, std::string_view entry, MCRegisteredBlock& shadow);
