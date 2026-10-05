#pragma once

#include "appear/apprtype.h"

class Shape;

#pragma pack(push, 1)
/// <summary>An elemental's gesture (FIT "Gestures%d"), 0x12 bytes, unaligned in the original's table.</summary>
struct ElementalGestureData
{
    /// <summary>FIT "NumFrames" (0: no such gesture).</summary>
    uint32_t numFrames; // +0x00
    /// <summary>FIT "BasePacketNumber".</summary>
    uint32_t basePacketNumber; // +0x04
    /// <summary>FIT "FrameRate".</summary>
    float frameRate; // +0x08
    /// <summary>FIT "Velocity".</summary>
    float velocity; // +0x0c
    /// <summary>FIT "State".</summary>
    uint8_t state; // +0x10
    /// <summary>FIT "NumRotations".</summary>
    uint8_t numRotations; // +0x11
};
#pragma pack(pop)
static_assert(sizeof(ElementalGestureData) == 0x12);

/// <summary>The type of an <see cref="ElementalActor"/>: an elemental (armoured infantry) squad's gestures.</summary>
/// <remarks>
/// Original source: <c>sprite\elmtree.cpp</c>, 0x40 bytes (class 8 of the sprite PAK). FIT: "SpecialInfo"
/// (jumpMaxDistance), "Gestures" (NumGestures) and "Gestures%d".
/// </remarks>
class ElementalTree : public AppearanceType
{
public:
    ElementalTree() = default;
    /// <remarks>MCX.EXE @ 0x006aca40 (vector deleting destructor); slot 2</remarks>
    ~ElementalTree() override { ElementalTree::destroy(); }

    /// <remarks>MCX.EXE @ 0x0063ac00; slot 0</remarks>
    int32_t init(File* apprFile, uint32_t fileSize, uint32_t loadFlags) override;

    /// <remarks>MCX.EXE @ 0x0063b1a0; slot 1</remarks>
    void destroy() override;

    /// <summary>Forgets <paramref name="shape"/> in the list and in every user.</summary>
    /// <remarks>MCX.EXE @ 0x0063ac80 (the vtable's removeShape slot, no symbol); slot 3</remarks>
    void removeShape(Shape* shape) override;

    /// <summary>Nothing (elementals load their shapes as they're drawn).</summary>
    /// <remarks>MCX.EXE @ 0x0063ace0</remarks>
    void preloadGestures(int32_t gesture, float rotation);

    /// <summary>Reads the type's FIT.</summary>
    /// <remarks>MCX.EXE @ 0x0063acf0</remarks>
    int32_t loadIniFile(File* apprFile, uint32_t fileSize);

    /// <summary>The frame rate of gesture <paramref name="gesture"/> (<paramref name="frameRate"/>).</summary>
    /// <remarks>MCX.EXE @ 0x0063af50</remarks>
    void setGesture(int32_t gesture, float rotation, float& frameRate);

    /// <summary>
    /// The shape of gesture <paramref name="gesture"/> at <paramref name="rotation"/> degrees, frame
    /// <paramref name="frame"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0063b010</remarks>
    Shape* getGesture(int32_t gesture, float rotation, float& frameRate, int frame);

    /// <summary>The gestures.</summary>
    ElementalGestureData* gestures = nullptr; // +0x2c
    /// <summary>FIT "NumGestures".</summary>
    uint32_t numGestures = 0; // +0x30
    /// <summary>The loaded shape of each packet.</summary>
    Shape** shapeList = nullptr; // +0x34
    /// <summary>FIT "jumpMaxDistance" (one float).</summary>
    float* jumpMaxDistance = nullptr; // +0x38
    /// <summary>The number of packets.</summary>
    uint32_t numPackets = 0; // +0x3c
};
