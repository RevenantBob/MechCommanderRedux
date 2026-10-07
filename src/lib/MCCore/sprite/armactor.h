#pragma once

#include "appear/appear.h"
#include "appear/apprtype.h"

class MCShape;

/// <summary>The single animation state of an arm (weapon effect) appearance type, 0x14 bytes.</summary>
struct MCArmActorData
{
    /// <summary>FIT "NumRotations".</summary>
    uint8_t NumRotations;
    /// <summary>FIT "NumFrames".</summary>
    uint32_t NumFrames;
    /// <summary>FIT "BasePacketNumber".</summary>
    uint32_t BasePacketNumber;
    /// <summary>FIT "FrameRate".</summary>
    float FrameRate;
    /// <summary>FIT "Symmetrical".</summary>
    uint32_t Symmetrical;
};

/// <summary>
/// The type of an <see cref="MCArmAppearance"/>: a weapon effect (missile, bullet, jet...) drawn from one shape per
/// facing.
/// </summary>
/// <remarks>
/// Original source: <c>sprite\armactor.cpp</c>, 0x3c bytes (class 6 of the sprite PAK). FIT: "State" (NumFrames,
/// FrameRate, BasePacketNumber, NumRotations, Symmetrical, CheckForHeader).
/// </remarks>
class MCArmAppearanceType : public MCAppearanceType
{
public:
    MCArmAppearanceType() = default;
    ~MCArmAppearanceType() override { MCArmAppearanceType::Destroy(); }

    /// <summary>Loads the FIT and makes the shape list; with <paramref name="loadFlags"/> loads every shape now.</summary>
    int32_t Init(MCFile* apprFile, uint32_t fileSize, uint32_t loadFlags) override;

    void Destroy() override;

    void RemoveShape(MCShape* shape) override;

    /// <summary>Loads every packet's shape.</summary>
    void PreloadGestures();

    /// <summary>Reads the type's FIT.</summary>
    int32_t LoadIniFile(MCFile* apprFile, uint32_t fileSize);

    /// <summary>
    /// The shape facing <paramref name="rotation"/> degrees (clamped to +-180); <paramref name="frameRate"/> gets the
    /// frame rate, <paramref name="reverse"/> whether to mirror it. <paramref name="frame"/> is unused.
    /// </summary>
    MCShape* GetShape(int32_t rotation, int32_t frame, float& frameRate, int& reverse);

    /// <summary>The state.</summary>
    MCArmActorData* ActorData = nullptr;
    /// <summary>The loaded shape of each packet.</summary>
    MCShape** ShapeList = nullptr;
    /// <summary>FIT "CheckForHeader" (1 when absent).</summary>
    uint8_t CheckForHeader = 1;
    /// <summary>The number of packets.</summary>
    int32_t NumPackets = 0;
};

/// <summary>A weapon effect's appearance, turned to its owner's direction of travel.</summary>
/// <remarks>Original source: <c>sprite\armactor.cpp</c>, 0x78 bytes.</remarks>
class MCArmAppearance : public MCAppearance
{
public:
    MCArmAppearance() = default;
    /// <remarks>Vector deleting destructor; slot 2</remarks>
    ~MCArmAppearance() override { MCArmAppearance::Destroy(); }

    int32_t Init(MCAppearanceType* tree = nullptr, MCGameObject* obj = nullptr) override;

    void Destroy() override;

    /// <summary>Advances the animation (looping).</summary>
    int32_t Update() override;

    /// <summary>Faces the owner's velocity and adds the shape to the element list.</summary>
    int32_t Render(int32_t depthFixup = 0) override;

    int RecalcBounds(MCCamera* cam) override;

    int32_t GetFrameNumber() override { return CurrentFrame; }

    /// <summary>The object the effect belongs to (its type name labels the VFX element); set by the owner.</summary>
    MCGameObject* OwnerObject = nullptr;
    /// <summary>The type.</summary>
    MCArmAppearanceType* AppearType = nullptr;
    /// <summary>The shape drawn.</summary>
    MCShape* CurrentShape = nullptr;
    /// <summary>The frame drawn (-1: not started).</summary>
    int32_t CurrentFrame = -1;
    /// <summary>Nonzero when the shape is drawn mirrored.</summary>
    int Reverse = 0;
    /// <summary>Seconds into the animation.</summary>
    float CurrentTime = 0.0f;
    /// <summary>The state's frame rate.</summary>
    float FrameRate = 0.0f;
    /// <summary>Frames played so far.</summary>
    int32_t LastFrame = 0;
    /// <summary>The facing, in degrees.</summary>
    float Rotation = 0.0f;
    /// <summary>The fade table (index into the palette's haze tables) to draw through, -1 for none.</summary>
    int32_t FadeTableIndex = -1;
    /// <summary>The shape's top-left offset from its hotspot (-15 before a shape).</summary>
    float ShapeMinX = -15.0f;
    float ShapeMinY = -15.0f;
    /// <summary>The shape's size (15 before a shape).</summary>
    float ShapeMaxX = 15.0f;
    float ShapeMaxY = 15.0f;
};
