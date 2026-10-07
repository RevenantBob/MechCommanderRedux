#pragma once

#include "appear/MCAppearanceType.h"

/// <summary>An elemental's gesture (FIT "Gestures%d").</summary>
struct MCElementalGestureData
{
    /// <summary>FIT "NumFrames" (0: no such gesture).</summary>
    uint32_t NumFrames = 0;
    /// <summary>FIT "BasePacketNumber".</summary>
    uint32_t BasePacketNumber = 0;
    /// <summary>FIT "FrameRate".</summary>
    float FrameRate = 0.0f;
    /// <summary>FIT "Velocity".</summary>
    float Velocity = 0.0f;
    /// <summary>FIT "State".</summary>
    uint8_t State = 0;
    /// <summary>FIT "NumRotations".</summary>
    uint8_t NumRotations = 0;
};

/// <summary>The type of an <see cref="MCElementalActor"/>: an elemental (armoured infantry) squad's gestures.</summary>
/// <remarks>
/// Original source: <c>sprite\elmtree.cpp</c> (class 8 of the sprite PAK). FIT: "SpecialInfo" (jumpMaxDistance),
/// "Gestures" (NumGestures) and "Gestures%d".
/// </remarks>
class MCElementalTree : public MCAppearanceType
{
public:
    MCElementalTree() = default;
    /// <summary>Leaves the type's shapes in the cache without an owner.</summary>
    ~MCElementalTree() override;

    /// <summary>Reads the FIT and makes the shape list (one entry per packet).</summary>
    MCAppearanceLoad Load(MCFile& apprFile, uint32_t fileSize) override;

    /// <summary>Forgets <paramref name="shape"/> in the list and in every user.</summary>
    void RemoveShape(MCShape* shape) override;

    /// <summary>The frame rate of gesture <paramref name="gesture"/>, made positive (0 when it has no frames).</summary>
    float GestureFrameRate(int32_t gesture) const;

    /// <summary>The shape of gesture <paramref name="gesture"/> at <paramref name="rotation"/> degrees.</summary>
    MCShape* GetGesture(int32_t gesture, float rotation);

    /// <summary>The gestures.</summary>
    std::vector<MCElementalGestureData> Gestures;
    /// <summary>The loaded shape of each packet.</summary>
    std::vector<MCShape*> ShapeList;
    /// <summary>FIT "jumpMaxDistance".</summary>
    float JumpMaxDistance = 0.0f;
};
