#pragma once

#include "appear/appear.h"
#include "appear/apprtype.h"

class Shape;

/// <summary>
/// The states of a ground vehicle's appearance. The type has 3 (or 4) "State%d" blocks; state 2 draws no turret.
/// The names are the port's.
/// </summary>
enum GVActorState : int32_t
{
    GV_ACTOR_STATE_NORMAL = 0,
    GV_ACTOR_STATE_DAMAGED = 1,
    GV_ACTOR_STATE_DESTROYED = 2,
    /// <summary>Only when the type's NumStates is 4.</summary>
    GV_ACTOR_STATE_EXTRA = 3,
    MAX_GV_ACTOR_STATES = 4
};

/// <summary>A state of a ground vehicle type (FIT "State%d"), 0x14 bytes.</summary>
struct GVActorData
{
    /// <summary>FIT "State".</summary>
    GVActorState state; // +0x00
    /// <summary>FIT "NumRotations".</summary>
    uint8_t numRotations; // +0x04
    /// <summary>FIT "NumFrames".</summary>
    uint32_t numFrames; // +0x08
    /// <summary>FIT "BasePacketNumber": the body's first packet; the turret's follow each part's rotations.</summary>
    uint32_t basePacketNumber; // +0x0c
    /// <summary>FIT "FrameRate".</summary>
    float frameRate; // +0x10
};

/// <summary>
/// The type of a <see cref="GVAppearance"/>: a ground vehicle's body and turret shapes per state and facing.
/// </summary>
/// <remarks>
/// Original source: <c>sprite\gvactor.cpp</c>, 0x44 bytes (class 5 of the sprite PAK). FIT: "Main Info" (NumParts,
/// TurretOffset), "States" (NumStates, 3 or 4), "State%d".
/// </remarks>
class GVAppearanceType : public AppearanceType
{
public:
    GVAppearanceType() = default;
    /// <remarks>MCX.EXE @ 0x006ac940 (vector deleting destructor); slot 2</remarks>
    ~GVAppearanceType() override { GVAppearanceType::destroy(); }

    /// <remarks>MCX.EXE @ 0x0063b200; slot 0</remarks>
    int32_t init(File* apprFile, uint32_t fileSize, uint32_t loadFlags) override;

    /// <remarks>MCX.EXE @ 0x0063b690; slot 1</remarks>
    void destroy() override;

    /// <summary>Forgets <paramref name="shape"/> in the list and in every user's body and turret.</summary>
    /// <remarks>MCX.EXE @ 0x0063b280; slot 3</remarks>
    void removeShape(Shape* shape) override;

    /// <summary>Reads the type's FIT.</summary>
    /// <remarks>MCX.EXE @ 0x0063b2d0</remarks>
    int32_t loadIniFile(File* apprFile, uint32_t fileSize);

    /// <summary>
    /// The shape of part <paramref name="part"/> (0 body, 1 turret) in <paramref name="state"/> facing
    /// <paramref name="rotation"/> degrees; <paramref name="frameRate"/> gets the state's frame rate.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0063b540</remarks>
    Shape* getShape(GVActorState state, int32_t rotation, int32_t part, float& frameRate);

    /// <summary>The states (4 entries).</summary>
    GVActorData* actorStateData = nullptr; // +0x2c
    /// <summary>The loaded shape of each packet.</summary>
    Shape** shapeList = nullptr; // +0x30
    /// <summary>FIT "NumParts": 1, or 2 with a turret.</summary>
    uint32_t numParts = 0; // +0x34
    /// <summary>FIT "TurretOffset": how far the turret sits from the body's centre, in metres.</summary>
    float turretOffset = 0.0f; // +0x38
    /// <summary>The number of packets.</summary>
    int32_t numPackets = 0; // +0x3c
    /// <summary>Nonzero when the type has the fourth state.</summary>
    int32_t hasExtraState = 0; // +0x40
};

/// <summary>A ground vehicle: a body and an optional turret, each facing its own way.</summary>
/// <remarks>Original source: <c>sprite\gvactor.cpp</c>, <c>sprite\gvactor.h</c>; 0x98 bytes.</remarks>
class GVAppearance : public Appearance
{
public:
    GVAppearance() = default;
    /// <remarks>MCX.EXE @ 0x006511e0 (vector deleting destructor); slot 2</remarks>
    ~GVAppearance() override { GVAppearance::destroy(); }

    /// <remarks>MCX.EXE @ 0x0063b6e0; slot 0</remarks>
    int32_t init(AppearanceType* tree = nullptr, GameObject* obj = nullptr) override;

    /// <remarks>MCX.EXE @ 0x0063c110; slot 1</remarks>
    void destroy() override;

    /// <summary>Advances the animation and starts the target highlight.</summary>
    /// <remarks>MCX.EXE @ 0x0063c010; slot 3</remarks>
    int32_t update() override;

    /// <summary>Turns body and turret to the owner's facings and adds them to the element list.</summary>
    /// <remarks>MCX.EXE @ 0x0063b9e0; slot 4</remarks>
    int32_t render(int32_t depthFixup = 0) override;

    /// <remarks>MCX.EXE @ 0x006511b0 (gvactor.h); slot 5</remarks>
    AppearanceType* getAppearanceType() override { return appearType; }

    /// <summary>Draws the vehicle's damage bar.</summary>
    /// <remarks>MCX.EXE @ 0x0063c170; slot 6</remarks>
    void drawBars() override;

    /// <remarks>MCX.EXE @ 0x0063b780; slot 7</remarks>
    int recalcBounds(Camera* cam) override;

    /// <summary>Switches to <paramref name="state"/> when the type has it.</summary>
    /// <remarks>MCX.EXE @ 0x006511c0 (gvactor.h); slot 13</remarks>
    virtual void setTypeId(GVActorState state)
    {
        if (stateExists(state))
        {
            currentState = state;
        }
    }

    /// <summary>The number of frames of <paramref name="state"/> (0 when absent).</summary>
    /// <remarks>MCX.EXE @ 0x0063c130</remarks>
    int32_t stateExists(GVActorState state);

    /// <summary>The type.</summary>
    GVAppearanceType* appearType = nullptr; // +0x38
    /// <summary>The body's and the turret's shapes.</summary>
    Shape* currentShape[2] = {}; // +0x3c
    /// <summary>The body's and the turret's frames (-1: not started).</summary>
    int32_t currentFrame[2] = {-1, -1}; // +0x44
    /// <summary>Cleared by init per part; never otherwise used.</summary>
    int32_t unknown4C[2] = {}; // +0x4c
    /// <summary>
    /// The parts render draws, in order: set to {0, 1} there and walked as its loop index (a local array the
    /// compiler kept in the object).
    /// </summary>
    int32_t partOrder[2] = {}; // +0x54
    /// <summary>The type's part count.</summary>
    int32_t numParts = 0; // +0x5c
    /// <summary>The type's turret offset.</summary>
    float turretOffset = 0.0f; // +0x60
    /// <summary>Cleared by init; never otherwise used.</summary>
    int32_t unknown64 = 0; // +0x64
    /// <summary>Seconds into the animation.</summary>
    float currentTime = 0.0f; // +0x68
    /// <summary>The frame rate (15 after init).</summary>
    float frameRate = 15.0f; // +0x6c
    /// <summary>Frames played so far.</summary>
    int32_t lastFrame = 0; // +0x70
    /// <summary>The current state.</summary>
    GVActorState currentState = GV_ACTOR_STATE_NORMAL; // +0x74
    /// <summary>The body's facing, in degrees.</summary>
    float bodyRotation = 0.0f; // +0x78
    /// <summary>The turret's facing, in degrees (-180..180 then 0..360).</summary>
    float turretRotation = 0.0f; // +0x7c
    /// <summary>The fade table (haze table index) to draw through, -1 for none.</summary>
    int32_t fadeTableIndex = -1; // +0x80
    /// <summary>The haze palette Turret::render hands over; the appearance never reads it.</summary>
    uint8_t* hazePalette = nullptr; // +0x84
    /// <summary>The shape's top-left offset from its hotspot (-25 before a shape).</summary>
    float shapeMinX = -25.0f; // +0x88
    float shapeMinY = -25.0f; // +0x8c
    /// <summary>The shape's size (50 before a shape).</summary>
    float shapeMaxX = 50.0f; // +0x90
    float shapeMaxY = 50.0f; // +0x94
};
