#pragma once

#include "appear/appear.h"
#include "appear/apprtype.h"

class Shape;

/// <summary>The single animation state of an arm (weapon effect) appearance type, 0x14 bytes.</summary>
struct ArmActorData
{
    /// <summary>FIT "NumRotations".</summary>
    uint8_t numRotations; // +0x00
    /// <summary>FIT "NumFrames".</summary>
    uint32_t numFrames; // +0x04
    /// <summary>FIT "BasePacketNumber".</summary>
    uint32_t basePacketNumber; // +0x08
    /// <summary>FIT "FrameRate".</summary>
    float frameRate; // +0x0c
    /// <summary>FIT "Symmetrical".</summary>
    uint32_t symmetrical; // +0x10
};

/// <summary>
/// The type of an <see cref="ArmAppearance"/>: a weapon effect (missile, bullet, jet...) drawn from one shape per
/// facing.
/// </summary>
/// <remarks>
/// Original source: <c>sprite\armactor.cpp</c>, 0x3c bytes (class 6 of the sprite PAK). FIT: "State" (NumFrames,
/// FrameRate, BasePacketNumber, NumRotations, Symmetrical, CheckForHeader).
/// </remarks>
class ArmAppearanceType : public AppearanceType
{
public:
    ArmAppearanceType() = default;
    /// <remarks>MCX.EXE @ 0x006ac9c0 (vector deleting destructor); slot 2</remarks>
    ~ArmAppearanceType() override { ArmAppearanceType::destroy(); }

    /// <summary>Loads the FIT and makes the shape list; with <paramref name="loadFlags"/> loads every shape now.</summary>
    /// <remarks>MCX.EXE @ 0x006392e0; slot 0</remarks>
    int32_t init(File* apprFile, uint32_t fileSize, uint32_t loadFlags) override;

    /// <remarks>MCX.EXE @ 0x00639760; slot 1</remarks>
    void destroy() override;

    /// <remarks>MCX.EXE @ 0x00639370; slot 3</remarks>
    void removeShape(Shape* shape) override;

    /// <summary>Loads every packet's shape.</summary>
    /// <remarks>MCX.EXE @ 0x006393c0</remarks>
    void preloadGestures();

    /// <summary>Reads the type's FIT.</summary>
    /// <remarks>MCX.EXE @ 0x00639400</remarks>
    int32_t loadIniFile(File* apprFile, uint32_t fileSize);

    /// <summary>
    /// The shape facing <paramref name="rotation"/> degrees (clamped to +-180); <paramref name="frameRate"/> gets the
    /// frame rate, <paramref name="reverse"/> whether to mirror it. <paramref name="frame"/> is unused.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006395a0</remarks>
    Shape* getShape(int32_t rotation, int32_t frame, float& frameRate, int& reverse);

    /// <summary>The state.</summary>
    ArmActorData* actorData = nullptr; // +0x2c
    /// <summary>The loaded shape of each packet.</summary>
    Shape** shapeList = nullptr; // +0x30
    /// <summary>FIT "CheckForHeader" (1 when absent).</summary>
    uint8_t checkForHeader = 1; // +0x34
    /// <summary>The number of packets.</summary>
    int32_t numPackets = 0; // +0x38
};

/// <summary>A weapon effect's appearance, turned to its owner's direction of travel.</summary>
/// <remarks>Original source: <c>sprite\armactor.cpp</c>, 0x78 bytes.</remarks>
class ArmAppearance : public Appearance
{
public:
    ArmAppearance() = default;
    /// <remarks>Vector deleting destructor; slot 2</remarks>
    ~ArmAppearance() override { ArmAppearance::destroy(); }

    /// <remarks>MCX.EXE @ 0x006397b0; slot 0</remarks>
    int32_t init(AppearanceType* tree = nullptr, GameObject* obj = nullptr) override;

    /// <remarks>MCX.EXE @ 0x00639d30; slot 1</remarks>
    void destroy() override;

    /// <summary>Advances the animation (looping).</summary>
    /// <remarks>MCX.EXE @ 0x00639c80; slot 3</remarks>
    int32_t update() override;

    /// <summary>Faces the owner's velocity and adds the shape to the element list.</summary>
    /// <remarks>MCX.EXE @ 0x00639a50; slot 4</remarks>
    int32_t render(int32_t depthFixup = 0) override;

    /// <remarks>MCX.EXE @ 0x00639820; slot 7</remarks>
    int recalcBounds(Camera* cam) override;

    /// <remarks>MCX.EXE @ 0x00655cd0 (no symbol: <c>mov eax, [ecx+0x44]; ret</c>); slot 11</remarks>
    int32_t getFrameNumber() override { return currentFrame; }

    /// <summary>The object the effect belongs to (its type name labels the VFX element); set by the owner.</summary>
    GameObject* ownerObject = nullptr; // +0x38
    /// <summary>The type.</summary>
    ArmAppearanceType* appearType = nullptr; // +0x3c
    /// <summary>The shape drawn.</summary>
    Shape* currentShape = nullptr; // +0x40
    /// <summary>The frame drawn (-1: not started).</summary>
    int32_t currentFrame = -1; // +0x44
    /// <summary>Cleared by init; never otherwise used.</summary>
    int32_t unknown48 = 0; // +0x48
    /// <summary>Nonzero when the shape is drawn mirrored.</summary>
    int reverse = 0; // +0x4c
    /// <summary>Cleared by init; never otherwise used.</summary>
    int32_t unknown50 = 0; // +0x50
    /// <summary>Seconds into the animation.</summary>
    float currentTime = 0.0f; // +0x54
    /// <summary>The state's frame rate.</summary>
    float frameRate = 0.0f; // +0x58
    /// <summary>Frames played so far.</summary>
    int32_t lastFrame = 0; // +0x5c
    /// <summary>The facing, in degrees.</summary>
    float rotation = 0.0f; // +0x60
    /// <summary>The fade table (index into the palette's haze tables) to draw through, -1 for none.</summary>
    int32_t fadeTableIndex = -1; // +0x64
    /// <summary>The shape's top-left offset from its hotspot (-15 before a shape).</summary>
    float shapeMinX = -15.0f; // +0x68
    float shapeMinY = -15.0f; // +0x6c
    /// <summary>The shape's size (15 before a shape).</summary>
    float shapeMaxX = 15.0f; // +0x70
    float shapeMaxY = 15.0f; // +0x74
};
