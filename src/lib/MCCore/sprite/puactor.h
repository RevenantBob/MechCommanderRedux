#pragma once

#include "appear/appear.h"
#include "appear/apprtype.h"

class Shape;

/// <summary>
/// The states of a pop-up turret's appearance (the type must have exactly 6). The names are the port's; the values
/// follow <see cref="PUAppearance::setCombatMode"/> and <see cref="PUAppearance::setDestroyed"/>.
/// </summary>
enum PUActorState : int32_t
{
    PU_ACTOR_STATE_CLOSED = 0,
    PU_ACTOR_STATE_OPENING = 1,
    PU_ACTOR_STATE_OPEN = 2,
    PU_ACTOR_STATE_CLOSING = 3,
    /// <summary>Destroyed while open (or opening/closing).</summary>
    PU_ACTOR_STATE_DESTROYED_OPEN = 4,
    /// <summary>Destroyed while closed.</summary>
    PU_ACTOR_STATE_DESTROYED_CLOSED = 5,
    MAX_PU_ACTOR_STATES = 6
};

/// <summary>A state of a pop-up turret type (FIT "State%d"), 0x14 bytes.</summary>
struct PUActorData
{
    /// <summary>FIT "State".</summary>
    PUActorState state; // +0x00
    /// <summary>FIT "NumRotations".</summary>
    uint8_t numRotations; // +0x04
    /// <summary>FIT "NumFrames".</summary>
    uint32_t numFrames; // +0x08
    /// <summary>FIT "BasePacketNumber".</summary>
    uint32_t basePacketNumber; // +0x0c
    /// <summary>FIT "FrameRate".</summary>
    float frameRate; // +0x10
};

/// <summary>The type of a <see cref="PUAppearance"/>: a pop-up turret's shapes per state and facing.</summary>
/// <remarks>
/// Original source: <c>sprite\puactor.cpp</c>, 0x3c bytes (class 9 of the sprite PAK). FIT: "Main Info", "States"
/// (NumStates = 6, Scaled), "State%d".
/// </remarks>
class PUAppearanceType : public AppearanceType
{
public:
    PUAppearanceType() = default;
    /// <remarks>MCX.EXE @ 0x006ac980 (vector deleting destructor); slot 2</remarks>
    ~PUAppearanceType() override { PUAppearanceType::destroy(); }

    /// <remarks>MCX.EXE @ 0x006405c0; slot 0</remarks>
    int32_t init(File* apprFile, uint32_t fileSize, uint32_t loadFlags) override;

    /// <remarks>MCX.EXE @ 0x00640a00; slot 1</remarks>
    void destroy() override;

    /// <remarks>MCX.EXE @ 0x00640640; slot 3</remarks>
    void removeShape(Shape* shape) override;

    /// <summary>Reads the type's FIT.</summary>
    /// <remarks>MCX.EXE @ 0x00640690</remarks>
    int32_t loadIniFile(File* apprFile, uint32_t fileSize);

    /// <summary>
    /// The shape of part <paramref name="part"/> in <paramref name="state"/> facing <paramref name="rotation"/>
    /// degrees (the zoomed-out one when scaled); <paramref name="frameRate"/> gets the state's frame rate.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006408b0 (no symbol; named after the other types' getShape).</remarks>
    Shape* getShape(PUActorState state, int32_t rotation, int32_t part, float& frameRate);

    /// <summary>The 6 states.</summary>
    PUActorData* actorStateData = nullptr; // +0x2c
    /// <summary>The loaded shape of each packet.</summary>
    Shape** shapeList = nullptr; // +0x30
    /// <summary>FIT "Scaled": each rotation run is followed by its zoomed-out run.</summary>
    uint8_t scaled = 0; // +0x34
    /// <summary>The number of packets.</summary>
    int32_t numPackets = 0; // +0x38
};

/// <summary>A pop-up turret: opens to fire, closes to hide.</summary>
/// <remarks>Original source: <c>sprite\puactor.cpp</c>, <c>sprite\puactor.h</c>; 0x78 bytes.</remarks>
class PUAppearance : public Appearance
{
public:
    PUAppearance() = default;
    /// <remarks>MCX.EXE @ 0x00667200 (vector deleting destructor); slot 2</remarks>
    ~PUAppearance() override { PUAppearance::destroy(); }

    /// <remarks>MCX.EXE @ 0x00640a50; slot 0</remarks>
    int32_t init(AppearanceType* tree = nullptr, GameObject* obj = nullptr) override;

    /// <remarks>MCX.EXE @ 0x006412d0; slot 1</remarks>
    void destroy() override;

    /// <summary>Advances the animation (stopping on the state's last frame) and starts the target highlight.</summary>
    /// <remarks>MCX.EXE @ 0x006411f0; slot 3</remarks>
    int32_t update() override;

    /// <remarks>MCX.EXE @ 0x00640d70; slot 4</remarks>
    int32_t render(int32_t depthFixup = 0) override;

    /// <remarks>MCX.EXE @ 0x006671d0 (puactor.h); slot 5</remarks>
    AppearanceType* getAppearanceType() override { return appearType; }

    /// <summary>Draws the turret's damage bar.</summary>
    /// <remarks>MCX.EXE @ 0x00641320 (the vtable's drawBars slot); slot 6</remarks>
    void drawBars() override;

    /// <remarks>MCX.EXE @ 0x00640ac0; slot 7</remarks>
    int recalcBounds(Camera* cam) override;

    /// <summary>Switches to <paramref name="state"/> when the type has it.</summary>
    /// <remarks>MCX.EXE @ 0x006671e0 (puactor.h: the vtable's slot after getAppearanceClass); slot 13</remarks>
    virtual void setTypeId(PUActorState state)
    {
        if (stateExists(state))
        {
            currentState = state;
        }
    }

    /// <summary>Switches to the destroyed state matching the current one.</summary>
    /// <remarks>MCX.EXE @ 0x006410d0</remarks>
    void setDestroyed();

    /// <summary>
    /// Opens (<paramref name="combatMode"/> nonzero) or closes the turret, stepping through the opening/closing
    /// states as their animations finish. Returns the state.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006410e0</remarks>
    int32_t setCombatMode(int combatMode);

    /// <summary>The number of frames of <paramref name="state"/> (0 when absent).</summary>
    /// <remarks>MCX.EXE @ 0x006412f0</remarks>
    int32_t stateExists(PUActorState state);

    /// <summary>The type.</summary>
    PUAppearanceType* appearType = nullptr; // +0x38
    /// <summary>The shape drawn.</summary>
    Shape* currentShape = nullptr; // +0x3c
    /// <summary>The frame drawn (-1: not started).</summary>
    int32_t currentFrame = -1; // +0x40
    /// <summary>Seconds into the animation.</summary>
    float currentTime = 0.0f; // +0x4c
    /// <summary>The frame rate (15 after init).</summary>
    float frameRate = 15.0f; // +0x50
    /// <summary>Frames played so far.</summary>
    int32_t lastFrame = 0; // +0x54
    /// <summary>The current state.</summary>
    PUActorState currentState = PU_ACTOR_STATE_CLOSED; // +0x58
    /// <summary>The facing, in degrees.</summary>
    float rotation = 0.0f; // +0x5c
    /// <summary>The fade table (haze table index) to draw through, -1 for none; set by the owner.</summary>
    int32_t fadeTableIndex = -1; // +0x60
    /// <summary>The haze palette Turret::render hands over; the appearance never reads it.</summary>
    uint8_t* hazePalette = nullptr; // +0x64
    /// <summary>The shape's top-left offset from its hotspot (-25 before a shape).</summary>
    float shapeMinX = -25.0f; // +0x68
    float shapeMinY = -25.0f; // +0x6c
    /// <summary>The shape's size (50 before a shape).</summary>
    float shapeMaxX = 50.0f; // +0x70
    float shapeMaxY = 50.0f; // +0x74
};
