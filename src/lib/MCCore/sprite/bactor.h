#pragma once

#include "sprite/actor.h"

/// <summary>An animation state of a building type (FIT "AnimState%d"), 8 bytes.</summary>
struct BuildingAnimState
{
    /// <summary>FIT "numFrames".</summary>
    uint32_t numFrames; // +0x00
    /// <summary>FIT "frameRate".</summary>
    float frameRate; // +0x04
};

/// <summary>
/// The type of a <see cref="VFXBuildingAppearance"/>: a building's damage frames (packets 12 and up of its PAK)
/// and the tiles under it (packets 0..9).
/// </summary>
/// <remarks>
/// Original source: <c>sprite\bactor.cpp</c>, 0x40 bytes (class 7 of the sprite PAK). FIT: "Main Info"
/// (NumFrames), optional "AnimationInfo" (NumAnimStates) and "AnimState%d".
/// </remarks>
class VFXBuildingAppearanceType : public AppearanceType
{
public:
    VFXBuildingAppearanceType() = default;
    /// <remarks>MCX.EXE @ 0x006ac900 (vector deleting destructor); slot 2</remarks>
    ~VFXBuildingAppearanceType() override { VFXBuildingAppearanceType::destroy(); }

    /// <summary>Loads the FIT and makes the shape list (one entry per packet).</summary>
    /// <remarks>MCX.EXE @ 0x00639d50; slot 0</remarks>
    int32_t init(File* apprFile, uint32_t fileSize, uint32_t loadFlags) override;

    /// <summary>Releases the shapes and frees the shape list.</summary>
    /// <remarks>MCX.EXE @ 0x0063a0b0; slot 1</remarks>
    void destroy() override;

    /// <summary>Forgets <paramref name="shape"/> in the list and in every user (building and tile shapes).</summary>
    /// <remarks>MCX.EXE @ 0x00639dd0; slot 3</remarks>
    void removeShape(Shape* shape) override;

    /// <summary>Reads the type's FIT.</summary>
    /// <remarks>MCX.EXE @ 0x00639e20</remarks>
    int32_t loadIniFile(File* apprFile, uint32_t fileSize);

    /// <summary>The building's shape for damage frame <paramref name="frame"/> (packet frame + 12).</summary>
    /// <remarks>MCX.EXE @ 0x00639fc0</remarks>
    Shape* getShape(uint32_t frame);

    /// <summary>Tile shape <paramref name="tileNum"/> (0..9; packet tileNum).</summary>
    /// <remarks>MCX.EXE @ 0x0063a030</remarks>
    Shape* getTileShape(uint32_t tileNum);

    /// <summary>The loaded shape of each packet.</summary>
    Shape** shapeList = nullptr; // +0x2c
    /// <summary>FIT "NumFrames": the damage frames.</summary>
    uint32_t numFrames = 0; // +0x30
    /// <summary>The animation states, or null.</summary>
    BuildingAnimState* animStates = nullptr; // +0x34
    /// <summary>FIT "NumAnimStates".</summary>
    uint32_t numAnimStates = 0; // +0x38
    /// <summary>The number of packets in the building's PAK.</summary>
    uint32_t numPackets = 0; // +0x3c
};

/// <summary>A building (or other terrain object) drawn as a damage frame over its ground tile.</summary>
/// <remarks>Original source: <c>sprite\bactor.cpp</c>, <c>sprite\bactor.h</c>; 0x98 bytes.</remarks>
class VFXBuildingAppearance : public VFXAppearance
{
public:
    VFXBuildingAppearance() = default;
    /// <remarks>MCX.EXE @ 0x006531c0 (vector deleting destructor); slot 2</remarks>
    ~VFXBuildingAppearance() override { VFXBuildingAppearance::destroy(); }

    /// <remarks>MCX.EXE @ 0x0063a0f0; slot 0</remarks>
    int32_t init(AppearanceType* tree = nullptr, GameObject* obj = nullptr) override;

    /// <summary>Unregisters from the type.</summary>
    /// <remarks>MCX.EXE @ 0x0063a950; slot 1</remarks>
    void destroy() override;

    /// <summary>Starts the highlight when the building is first targeted.</summary>
    /// <remarks>MCX.EXE @ 0x0063a130; slot 3</remarks>
    int32_t update() override;

    /// <summary>Adds the tile, the building shape, bars and selection marks to the element list.</summary>
    /// <remarks>MCX.EXE @ 0x0063a530; slot 4</remarks>
    int32_t render(int32_t depthFixup = 0) override;

    /// <remarks>MCX.EXE @ 0x006531b0 (bactor.h); slot 5</remarks>
    AppearanceType* getAppearanceType() override { return buildType; }

    /// <summary>Draws the building's damage bar.</summary>
    /// <remarks>MCX.EXE @ 0x0063a960; slot 6</remarks>
    void drawBars() override;

    /// <remarks>MCX.EXE @ 0x0063a1c0; slot 7</remarks>
    int recalcBounds(Camera* cam) override;

    /// <summary>Picks damage frame <paramref name="damageLevel"/> (low 4 bits, clamped to the type's frames).</summary>
    /// <remarks>MCX.EXE @ 0x0063a170; slot 14</remarks>
    void setDamageLvl(uint32_t damageLevel) override;

    /// <summary>Sets the bounds from the current shape's frame, offset by the owner's footprint.</summary>
    /// <remarks>MCX.EXE @ 0x0063a460</remarks>
    void calcCollideBounds();

    /// <summary>The damage frame drawn.</summary>
    uint32_t damageLevel = 0; // +0x84
    /// <summary>The ground tile drawn under the building (0..9).</summary>
    uint32_t tileNum = 0; // +0x88
    /// <summary>The type.</summary>
    VFXBuildingAppearanceType* buildType = nullptr; // +0x8c
    /// <summary>The tile's shape.</summary>
    Shape* tileShape = nullptr; // +0x90
    /// <summary>The animation state ABL setanimation picked, -1 for none (init sets -1; render tests it).</summary>
    int32_t animState = -1; // +0x94
};

/// <summary>When nonzero, animation timing follows the frame rate; cleared whenever a shape must be loaded.</summary>
extern int dynamicFrameTiming;
