#pragma once

#include "appear/appear.h"
#include "appear/apprtype.h"

class Shape;

/// <summary>
/// The animation states of a VFX appearance (the FIT's "State%d" blocks). The names are those of MechCommander 2's
/// <c>actor.h</c>, which descends from this code; the values match MCX.EXE's use.
/// </summary>
enum ActorState : int32_t
{
    ACTOR_STATE_INVALID = -1,
    ACTOR_STATE_NORMAL = 0,
    /// <summary>From normal to damaged (the frame is the damage level).</summary>
    ACTOR_STATE_BLOWING_UP1 = 1,
    ACTOR_STATE_DAMAGED = 2,
    /// <summary>From damaged to destroyed (the frame is the damage level past state 1's frames).</summary>
    ACTOR_STATE_BLOWING_UP2 = 3,
    ACTOR_STATE_DESTROYED = 4,
    ACTOR_STATE_FALLEN_DMG = 5,
    MAX_ACTOR_STATES = 6
};

/// <summary>One animation state (or sub-state) of a VFX appearance type, from its FIT (0x14 bytes).</summary>
struct ActorData
{
    /// <summary>FIT "State".</summary>
    ActorState state; // +0x00
    /// <summary>FIT "Symmetrical": the second half of the rotations are the first half mirrored.</summary>
    uint8_t symmetrical; // +0x04
    /// <summary>FIT "NumRotations".</summary>
    uint8_t numRotations; // +0x05
    /// <summary>FIT "Sub" (sub-states only).</summary>
    uint8_t subState; // +0x06
    /// <summary>FIT "Loop" (sub-states only; states always loop).</summary>
    uint8_t loop; // +0x07
    /// <summary>FIT "NumFrames" (0: the state doesn't exist).</summary>
    uint32_t numFrames; // +0x08
    /// <summary>FIT "BasePacketNumber": the state's first packet in the appearance's PAK.</summary>
    uint32_t basePacketNumber; // +0x0c
    /// <summary>FIT "FrameRate", frames per second.</summary>
    float frameRate; // +0x10
};

/// <summary>
/// The type of a <see cref="VFXAppearance"/>: an object drawn from one VFX shape per state and facing (effects,
/// terrain objects, trees...).
/// </summary>
/// <remarks>
/// Original source: <c>sprite\actor.cpp</c>, 0x40 bytes (class 2 of the sprite PAK). Its FIT: "Main Info"
/// (delta), "States" (NumStates, Scaled), "State%d" and "Sub%dState%d" blocks.
/// </remarks>
class VFXAppearanceType : public AppearanceType
{
public:
    VFXAppearanceType() = default;
    /// <remarks>MCX.EXE @ 0x006ac8c0 (vector deleting destructor); slot 2</remarks>
    ~VFXAppearanceType() override { VFXAppearanceType::destroy(); }

    /// <summary>
    /// Loads the FIT and makes the shape list; with <paramref name="loadFlags"/> the first shape is loaded at once
    /// and the type is kept loaded.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00637fa0; slot 0</remarks>
    int32_t init(File* apprFile, uint32_t fileSize, uint32_t loadFlags) override;

    /// <summary>Frees the shapes' ownership, the user list, the shape list and the state table.</summary>
    /// <remarks>MCX.EXE @ 0x00638670; slot 1</remarks>
    void destroy() override;

    /// <summary>Forgets <paramref name="shape"/> in the shape list and in every user.</summary>
    /// <remarks>MCX.EXE @ 0x00638040; slot 3</remarks>
    void removeShape(Shape* shape) override;

    /// <summary>Reads the type's FIT.</summary>
    /// <remarks>MCX.EXE @ 0x00638090</remarks>
    int32_t loadIniFile(File* apprFile, uint32_t fileSize);

    /// <summary>
    /// The shape for <paramref name="state"/> (sub-state <paramref name="subState"/>, 0xFF for none) facing
    /// <paramref name="rotation"/> degrees, loading it if needed. <paramref name="frameRate"/> gets the state's
    /// frame rate, <paramref name="reverse"/> whether the shape must be mirrored. <paramref name="frame"/> is unused.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006384e0</remarks>
    Shape* getShape(ActorState state, uint8_t subState, int32_t rotation, int32_t frame, float& frameRate,
                    int& reverse);

    /// <summary>The states (MAX_ACTOR_STATES entries).</summary>
    ActorData* actorStateData = nullptr; // +0x2c
    /// <summary>The sub-states of state 0, or null.</summary>
    ActorData* actorSubStateData = nullptr; // +0x30
    /// <summary>The loaded shape of each packet of the appearance's PAK.</summary>
    Shape** shapeList = nullptr; // +0x34
    /// <summary>FIT "NumStates".</summary>
    uint8_t numStates = 0; // +0x38
    /// <summary>FIT "Scaled": packets alternate full size and zoomed out.</summary>
    uint8_t scaled = 0; // +0x39
    /// <summary>The number of packets (shapes) in the appearance's PAK.</summary>
    int16_t numPackets = 0; // +0x3a
    /// <summary>FIT "delta": the appearance starts at frame 0 rather than a random one.</summary>
    int32_t delta = 0; // +0x3c
};

/// <summary>An object drawn from its type's shapes, animated through its states and facings.</summary>
/// <remarks>Original source: <c>sprite\actor.cpp</c>, <c>sprite\actor.h</c>; 0x84 bytes.</remarks>
class VFXAppearance : public Appearance
{
public:
    VFXAppearance() = default;
    /// <remarks>MCX.EXE @ 0x00653170 (vector deleting destructor); slot 2</remarks>
    ~VFXAppearance() override { VFXAppearance::destroy(); }

    /// <summary>Binds to <paramref name="tree"/> (registering as a user) and resets the animation.</summary>
    /// <remarks>MCX.EXE @ 0x006386d0; slot 0</remarks>
    int32_t init(AppearanceType* tree = nullptr, GameObject* obj = nullptr) override;

    /// <summary>Unregisters from the type and releases it.</summary>
    /// <remarks>MCX.EXE @ 0x00638ff0; slot 1</remarks>
    void destroy() override;

    /// <summary>Advances the animation by the frame time.</summary>
    /// <remarks>MCX.EXE @ 0x00638e50; slot 3</remarks>
    int32_t update() override;

    /// <summary>Adds the current shape (and the selection box, bars) to the element list.</summary>
    /// <remarks>MCX.EXE @ 0x00638980; slot 4</remarks>
    int32_t render(int32_t depthFixup = 0) override;

    /// <remarks>MCX.EXE @ 0x00653100; slot 5</remarks>
    AppearanceType* getAppearanceType() override { return appearType; }

    /// <summary>Draws the damage bar over the object.</summary>
    /// <remarks>MCX.EXE @ 0x00639040; slot 6</remarks>
    void drawBars() override;

    /// <summary>Takes the shape's bounds and projects the owner; nonzero when on screen.</summary>
    /// <remarks>MCX.EXE @ 0x00638750; slot 7</remarks>
    int recalcBounds(Camera* cam) override;

    /// <summary>
    /// Switches to <paramref name="state"/> (restarting it) and, in state 0, to sub-state
    /// <paramref name="subState"/> (0xFF for none).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00653110 (actor.h); slot 13</remarks>
    virtual void setTypeId(ActorState state, uint8_t subState);

    /// <summary>Shows damage level <paramref name="damageLevel"/>: normal, blowing up 1 or 2, or destroyed.</summary>
    /// <remarks>MCX.EXE @ 0x00638dc0; slot 14</remarks>
    virtual void setDamageLvl(uint32_t damageLevel);

    /// <summary>The number of frames of <paramref name="state"/> (0 when it doesn't exist).</summary>
    /// <remarks>MCX.EXE @ 0x00639010</remarks>
    int32_t stateExists(ActorState state);

    /// <summary>The type.</summary>
    VFXAppearanceType* appearType = nullptr; // +0x38
    /// <summary>The shape drawn.</summary>
    Shape* currentShape = nullptr; // +0x3c
    /// <summary>The frame drawn (-1: not started).</summary>
    int32_t currentFrame = -1; // +0x40
    /// <summary>Cleared by init; never otherwise used.</summary>
    int32_t unknown44 = 0; // +0x44
    /// <summary>Cleared by init; never otherwise used.</summary>
    int32_t unknown48 = 0; // +0x48
    /// <summary>Seconds into the current state.</summary>
    float currentTime = 0.0f; // +0x4c
    /// <summary>Frames advanced by the last update.</summary>
    float framesAdvanced = 0.0f; // +0x50
    /// <summary>Frames played so far in the current state.</summary>
    int32_t lastFrame = 0; // +0x54
    /// <summary>The frame the animation loops back to at <see cref="loopEnd"/>.</summary>
    int32_t loopStart = -1; // +0x58
    /// <summary>The frame at which the animation goes back to <see cref="loopStart"/> (-1: loop the whole state).</summary>
    int32_t loopEnd = -1; // +0x5c
    /// <summary>The current state.</summary>
    ActorState currentState = ACTOR_STATE_NORMAL; // +0x60
    /// <summary>The current sub-state of state 0, 0xFF for none.</summary>
    uint8_t currentSubState = 0xff; // +0x64
    /// <summary>The fade table the shape is drawn through, or null.</summary>
    uint8_t* fadeTable = nullptr; // +0x68
    /// <summary>Set by <see cref="setDamageLvl"/>.</summary>
    int32_t damageSet = 0; // +0x6c
    /// <summary>Set by <see cref="setTypeId"/> and init.</summary>
    int32_t typeChanged = 1; // +0x70
    /// <summary>The shape's top-left offset from its hotspot (from VFX_shape_minxy; -15 before a shape).</summary>
    float shapeMinX = -15.0f; // +0x74
    float shapeMinY = -15.0f; // +0x78
    /// <summary>The shape's size (VFX_shape_resolution; 15 before a shape).</summary>
    float shapeMaxX = 15.0f; // +0x7c
    float shapeMaxY = 15.0f; // +0x80
};

/// <summary>
/// Draws the damage bar of <paramref name="obj"/> (a building or tree building) above
/// <paramref name="appearance"/>: centred over <paramref name="type"/>'s bounds, or the appearance's when the type
/// has none. Nothing when <paramref name="obj"/> is null.
/// </summary>
/// <remarks>
/// Port helper: the body of <c>VFXAppearance::drawBars</c> (MCX.EXE @ 0x00639040) and
/// <c>VFXBuildingAppearance::drawBars</c> (0x0063a960), which the original repeats.
/// </remarks>
void MCDrawDamageBar(Appearance* appearance, AppearanceType* type, GameObject* obj);
