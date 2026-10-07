#pragma once

#include "sprite/actor.h"

/// <summary>An animation state of a building type (FIT "AnimState%d"), 8 bytes.</summary>
struct MCBuildingAnimState
{
    /// <summary>FIT "numFrames".</summary>
    uint32_t NumFrames;
    /// <summary>FIT "frameRate".</summary>
    float FrameRate;
};

/// <summary>
/// The type of a <see cref="MCVfxBuildingAppearance"/>: a building's damage frames (packets 12 and up of its PAK)
/// and the tiles under it (packets 0..9).
/// </summary>
/// <remarks>
/// Original source: <c>sprite\bactor.cpp</c>, 0x40 bytes (class 7 of the sprite PAK). FIT: "Main Info"
/// (NumFrames), optional "AnimationInfo" (NumAnimStates) and "AnimState%d".
/// </remarks>
class MCVfxBuildingAppearanceType : public MCAppearanceType
{
public:
    MCVfxBuildingAppearanceType() = default;
    ~MCVfxBuildingAppearanceType() override { MCVfxBuildingAppearanceType::Destroy(); }

    /// <summary>Loads the FIT and makes the shape list (one entry per packet).</summary>
    int32_t Init(MCFile* apprFile, uint32_t fileSize, uint32_t loadFlags) override;

    /// <summary>Releases the shapes and frees the shape list.</summary>
    void Destroy() override;

    /// <summary>Forgets <paramref name="shape"/> in the list and in every user (building and tile shapes).</summary>
    void RemoveShape(MCShape* shape) override;

    /// <summary>Reads the type's FIT.</summary>
    int32_t LoadIniFile(MCFile* apprFile, uint32_t fileSize);

    /// <summary>The building's shape for damage frame <paramref name="frame"/> (packet frame + 12).</summary>
    MCShape* GetShape(uint32_t frame);

    /// <summary>Tile shape <paramref name="tileNum"/> (0..9; packet tileNum).</summary>
    MCShape* GetTileShape(uint32_t tileNum);

    /// <summary>The loaded shape of each packet.</summary>
    MCShape** ShapeList = nullptr;
    /// <summary>FIT "NumFrames": the damage frames.</summary>
    uint32_t NumFrames = 0;
    /// <summary>The animation states, or null.</summary>
    MCBuildingAnimState* AnimStates = nullptr;
    /// <summary>FIT "NumAnimStates".</summary>
    uint32_t NumAnimStates = 0;
    /// <summary>The number of packets in the building's PAK.</summary>
    uint32_t NumPackets = 0;
};

/// <summary>A building (or other terrain object) drawn as a damage frame over its ground tile.</summary>
/// <remarks>Original source: <c>sprite\bactor.cpp</c>, <c>sprite\bactor.h</c>; 0x98 bytes.</remarks>
class MCVfxBuildingAppearance : public MCVfxAppearance
{
public:
    MCVfxBuildingAppearance() = default;
    ~MCVfxBuildingAppearance() override { MCVfxBuildingAppearance::Destroy(); }

    int32_t Init(MCAppearanceType* tree = nullptr, MCGameObject* obj = nullptr) override;

    /// <summary>Unregisters from the type.</summary>
    void Destroy() override;

    /// <summary>Starts the highlight when the building is first targeted.</summary>
    int32_t Update() override;

    /// <summary>Adds the tile, the building shape, bars and selection marks to the element list.</summary>
    int32_t Render(int32_t depthFixup = 0) override;

    MCAppearanceType* GetAppearanceType() override { return BuildType; }

    /// <summary>Draws the building's damage bar.</summary>
    void DrawBars() override;

    int RecalcBounds(MCCamera* cam) override;

    /// <summary>Picks damage frame <paramref name="damageLevel"/> (low 4 bits, clamped to the type's frames).</summary>
    void SetDamageLvl(uint32_t damageLevel) override;

    /// <summary>Sets the bounds from the current shape's frame, offset by the owner's footprint.</summary>
    void CalcCollideBounds();

    /// <summary>The damage frame drawn.</summary>
    uint32_t DamageLevel = 0;
    /// <summary>The ground tile drawn under the building (0..9).</summary>
    uint32_t TileNum = 0;
    /// <summary>The type.</summary>
    MCVfxBuildingAppearanceType* BuildType = nullptr;
    /// <summary>The tile's shape.</summary>
    MCShape* TileShape = nullptr;
    /// <summary>The animation state ABL setanimation picked, -1 for none (init sets -1; render tests it).</summary>
    int32_t AnimState = -1;
};

/// <summary>When nonzero, animation timing follows the frame rate; cleared whenever a shape must be loaded.</summary>
extern int DynamicFrameTiming;
