#pragma once

#include "object/MCObjectType.h"

/// <summary>
/// The type of a <see cref="MCTerrainObject"/>: damage level, placement offsets, impassable area and explosion.
/// </summary>
/// <remarks>Original source: <c>object\terrobj.cpp</c>, <c>object\terrobj.h</c>. Read from the "TerrainObjectData"
/// block of its FIT.</remarks>
class MCTerrainObjectType : public MCObjectType
{
public:
    /// <summary>Makes a <see cref="MCTerrainObject"/> of this type and gives it the next object id.</summary>
    std::unique_ptr<MCBaseObject> CreateInstance() override;
    /// <summary>
    /// Reads the "TerrainObjectData" block, then the common type data; the object's own ExtentRadius (-1 when missing:
    /// measured from the appearance) wins.
    /// </summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>A mover (object class below 8, not artillery) running into it deals it 10 points of damage.</summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override { return 0; }

    /// <summary>The damage that destroys the object; 0 means it starts destroyed (FIT "DmgLevel").</summary>
    uint32_t DmgLevel = 0;
    /// <summary>Replaces the placement's pixel offset X when nonzero (FIT "BasePixelOffsetX").</summary>
    int32_t BasePixelOffsetX = 0;
    /// <summary>Replaces the placement's pixel offset Y when nonzero (FIT "BasePixelOffsetY").</summary>
    int32_t BasePixelOffsetY = 0;
    /// <summary>Replaces the pixel offset X when placing the object in the world (FIT "CollisionOffsetX").</summary>
    int32_t CollisionOffsetX = 0;
    /// <summary>Replaces the pixel offset Y when placing the object in the world (FIT "CollisionOffsetY").</summary>
    int32_t CollisionOffsetY = 0;
    /// <summary>FIT "SetImpassable".</summary>
    int32_t SetImpassable = 0;
    /// <summary>FIT "XImpasse".</summary>
    int32_t XImpasse = 0;
    /// <summary>FIT "YImpasse".</summary>
    int32_t YImpasse = 0;
    /// <summary>FIT "ExplosionDamage".</summary>
    float ExplDmg = 0;
    /// <summary>FIT "ExplosionRadius".</summary>
    float ExplRad = 0;
};
