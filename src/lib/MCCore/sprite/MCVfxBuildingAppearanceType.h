#pragma once

#include "appear/MCAppearanceType.h"

/// <summary>An animation state of a building type (FIT "AnimState%d").</summary>
struct MCBuildingAnimState
{
    /// <summary>FIT "numFrames".</summary>
    uint32_t NumFrames = 0;
    /// <summary>FIT "frameRate".</summary>
    float FrameRate = 0.0f;
};

/// <summary>
/// The type of a <see cref="MCVfxBuildingAppearance"/>: a building's damage frames (packets 12 and up of its PAK)
/// and the tiles under it (packets 0..9).
/// </summary>
/// <remarks>
/// Original source: <c>sprite\bactor.cpp</c> (class 7 of the sprite PAK). FIT: "Main Info" (NumFrames), optional
/// "AnimationInfo" (NumAnimStates) and "AnimState%d".
/// </remarks>
class MCVfxBuildingAppearanceType : public MCAppearanceType
{
public:
    MCVfxBuildingAppearanceType() = default;
    /// <summary>Leaves the type's shapes in the cache without an owner.</summary>
    ~MCVfxBuildingAppearanceType() override;

    /// <summary>Reads the FIT and makes the shape list (one entry per packet).</summary>
    MCAppearanceLoad Load(MCFile& apprFile, uint32_t fileSize) override;

    /// <summary>Forgets <paramref name="shape"/> in the list and in every user (building and tile shapes).</summary>
    void RemoveShape(MCShape* shape) override;

    /// <summary>The building's shape for damage frame <paramref name="frame"/> (packet frame + 12).</summary>
    MCShape* GetShape(uint32_t frame);

    /// <summary>Tile shape <paramref name="tileNum"/> (0..9; packet tileNum).</summary>
    MCShape* GetTileShape(uint32_t tileNum);

    /// <summary>The loaded shape of each packet.</summary>
    std::vector<MCShape*> ShapeList;
    /// <summary>FIT "NumFrames": the damage frames.</summary>
    uint32_t NumFrames = 0;
    /// <summary>The animation states (FIT "AnimationInfo"); empty when the type has none.</summary>
    std::vector<MCBuildingAnimState> AnimStates;

private:
    /// <summary>The shape of packet <paramref name="packet"/>, loaded if needed.</summary>
    MCShape* PacketShape(uint32_t packet);
};
