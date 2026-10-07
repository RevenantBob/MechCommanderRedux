#pragma once

#include "appear/apprtype.h"

class MCShape;

#pragma pack(push, 1)
/// <summary>An elemental's gesture (FIT "Gestures%d"), 0x12 bytes, unaligned in the original's table.</summary>
struct MCElementalGestureData
{
    /// <summary>FIT "NumFrames" (0: no such gesture).</summary>
    uint32_t NumFrames;
    /// <summary>FIT "BasePacketNumber".</summary>
    uint32_t BasePacketNumber;
    /// <summary>FIT "FrameRate".</summary>
    float FrameRate;
    /// <summary>FIT "Velocity".</summary>
    float Velocity;
    /// <summary>FIT "State".</summary>
    uint8_t State;
    /// <summary>FIT "NumRotations".</summary>
    uint8_t NumRotations;
};
#pragma pack(pop)
static_assert(sizeof(MCElementalGestureData) == 0x12);

/// <summary>The type of an <see cref="MCElementalActor"/>: an elemental (armoured infantry) squad's gestures.</summary>
/// <remarks>
/// Original source: <c>sprite\elmtree.cpp</c>, 0x40 bytes (class 8 of the sprite PAK). FIT: "SpecialInfo"
/// (jumpMaxDistance), "Gestures" (NumGestures) and "Gestures%d".
/// </remarks>
class MCElementalTree : public MCAppearanceType
{
public:
    MCElementalTree() = default;
    ~MCElementalTree() override { MCElementalTree::Destroy(); }

    int32_t Init(MCFile* apprFile, uint32_t fileSize, uint32_t loadFlags) override;

    void Destroy() override;

    /// <summary>Forgets <paramref name="shape"/> in the list and in every user.</summary>
    void RemoveShape(MCShape* shape) override;

    /// <summary>Nothing (elementals load their shapes as they're drawn).</summary>
    void PreloadGestures(int32_t gesture, float rotation);

    /// <summary>Reads the type's FIT.</summary>
    int32_t LoadIniFile(MCFile* apprFile, uint32_t fileSize);

    /// <summary>The frame rate of gesture <paramref name="gesture"/> (<paramref name="frameRate"/>).</summary>
    void SetGesture(int32_t gesture, float rotation, float& frameRate);

    /// <summary>
    /// The shape of gesture <paramref name="gesture"/> at <paramref name="rotation"/> degrees, frame
    /// <paramref name="frame"/>.
    /// </summary>
    MCShape* GetGesture(int32_t gesture, float rotation, float& frameRate, int frame);

    /// <summary>The gestures.</summary>
    MCElementalGestureData* Gestures = nullptr;
    /// <summary>FIT "NumGestures".</summary>
    uint32_t NumGestures = 0;
    /// <summary>The loaded shape of each packet.</summary>
    MCShape** ShapeList = nullptr;
    /// <summary>FIT "jumpMaxDistance" (one float).</summary>
    float* JumpMaxDistance = nullptr;
    /// <summary>The number of packets.</summary>
    uint32_t NumPackets = 0;
};
