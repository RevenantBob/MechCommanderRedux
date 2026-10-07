#pragma once

#include "appear/appear.h"
#include "appear/apprtype.h"

class MCShape;

/// <summary>
/// The animation states of a VFX appearance (the FIT's "State%d" blocks). The names are those of MechCommander 2's
/// <c>actor.h</c>, which descends from this code; the values match MCX.EXE's use.
/// </summary>
enum MCActorState : int32_t
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
struct MCActorData
{
    /// <summary>FIT "State".</summary>
    MCActorState State;
    /// <summary>FIT "Symmetrical": the second half of the rotations are the first half mirrored.</summary>
    uint8_t Symmetrical;
    /// <summary>FIT "NumRotations".</summary>
    uint8_t NumRotations;
    /// <summary>FIT "Sub" (sub-states only).</summary>
    uint8_t SubState;
    /// <summary>FIT "Loop" (sub-states only; states always loop).</summary>
    uint8_t Loop;
    /// <summary>FIT "NumFrames" (0: the state doesn't exist).</summary>
    uint32_t NumFrames;
    /// <summary>FIT "BasePacketNumber": the state's first packet in the appearance's PAK.</summary>
    uint32_t BasePacketNumber;
    /// <summary>FIT "FrameRate", frames per second.</summary>
    float FrameRate;
};

/// <summary>
/// The type of a <see cref="MCVfxAppearance"/>: an object drawn from one VFX shape per state and facing (effects,
/// terrain objects, trees...).
/// </summary>
/// <remarks>
/// Original source: <c>sprite\actor.cpp</c>, 0x40 bytes (class 2 of the sprite PAK). Its FIT: "Main Info"
/// (delta), "States" (NumStates, Scaled), "State%d" and "Sub%dState%d" blocks.
/// </remarks>
class MCVfxAppearanceType : public MCAppearanceType
{
public:
    MCVfxAppearanceType() = default;
    ~MCVfxAppearanceType() override { MCVfxAppearanceType::Destroy(); }

    /// <summary>
    /// Loads the FIT and makes the shape list; with <paramref name="loadFlags"/> the first shape is loaded at once
    /// and the type is kept loaded.
    /// </summary>
    int32_t Init(MCFile* apprFile, uint32_t fileSize, uint32_t loadFlags) override;

    /// <summary>Frees the shapes' ownership, the user list, the shape list and the state table.</summary>
    void Destroy() override;

    /// <summary>Forgets <paramref name="shape"/> in the shape list and in every user.</summary>
    void RemoveShape(MCShape* shape) override;

    /// <summary>Reads the type's FIT.</summary>
    int32_t LoadIniFile(MCFile* apprFile, uint32_t fileSize);

    /// <summary>
    /// The shape for <paramref name="state"/> (sub-state <paramref name="subState"/>, 0xFF for none) facing
    /// <paramref name="rotation"/> degrees, loading it if needed. <paramref name="frameRate"/> gets the state's
    /// frame rate, <paramref name="reverse"/> whether the shape must be mirrored. <paramref name="frame"/> is unused.
    /// </summary>
    MCShape* GetShape(MCActorState state, uint8_t subState, int32_t rotation, int32_t frame, float& frameRate,
                      int& reverse);

    /// <summary>The states (MAX_ACTOR_STATES entries).</summary>
    MCActorData* ActorStateData = nullptr;
    /// <summary>The sub-states of state 0, or null.</summary>
    MCActorData* ActorSubStateData = nullptr;
    /// <summary>The loaded shape of each packet of the appearance's PAK.</summary>
    MCShape** ShapeList = nullptr;
    /// <summary>FIT "NumStates".</summary>
    uint8_t NumStates = 0;
    /// <summary>FIT "Scaled": packets alternate full size and zoomed out.</summary>
    uint8_t Scaled = 0;
    /// <summary>The number of packets (shapes) in the appearance's PAK.</summary>
    int16_t NumPackets = 0;
    /// <summary>FIT "delta": the appearance starts at frame 0 rather than a random one.</summary>
    int32_t Delta = 0;
};

/// <summary>An object drawn from its type's shapes, animated through its states and facings.</summary>
/// <remarks>Original source: <c>sprite\actor.cpp</c>, <c>sprite\actor.h</c>; 0x84 bytes.</remarks>
class MCVfxAppearance : public MCAppearance
{
public:
    MCVfxAppearance() = default;
    ~MCVfxAppearance() override { MCVfxAppearance::Destroy(); }

    /// <summary>Binds to <paramref name="tree"/> (registering as a user) and resets the animation.</summary>
    int32_t Init(MCAppearanceType* tree = nullptr, MCGameObject* obj = nullptr) override;

    /// <summary>Unregisters from the type and releases it.</summary>
    void Destroy() override;

    /// <summary>Advances the animation by the frame time.</summary>
    int32_t Update() override;

    /// <summary>Adds the current shape (and the selection box, bars) to the element list.</summary>
    int32_t Render(int32_t depthFixup = 0) override;

    MCAppearanceType* GetAppearanceType() override { return AppearType; }

    /// <summary>Draws the damage bar over the object.</summary>
    void DrawBars() override;

    /// <summary>Takes the shape's bounds and projects the owner; nonzero when on screen.</summary>
    int RecalcBounds(MCCamera* cam) override;

    /// <summary>
    /// Switches to <paramref name="state"/> (restarting it) and, in state 0, to sub-state
    /// <paramref name="subState"/> (0xFF for none).
    /// </summary>
    virtual void SetTypeId(MCActorState state, uint8_t subState);

    /// <summary>Shows damage level <paramref name="damageLevel"/>: normal, blowing up 1 or 2, or destroyed.</summary>
    virtual void SetDamageLvl(uint32_t damageLevel);

    /// <summary>The number of frames of <paramref name="state"/> (0 when it doesn't exist).</summary>
    int32_t StateExists(MCActorState state);

    /// <summary>The type.</summary>
    MCVfxAppearanceType* AppearType = nullptr;
    /// <summary>The shape drawn.</summary>
    MCShape* CurrentShape = nullptr;
    /// <summary>The frame drawn (-1: not started).</summary>
    int32_t CurrentFrame = -1;
    /// <summary>Seconds into the current state.</summary>
    float CurrentTime = 0.0f;
    /// <summary>Frames advanced by the last update.</summary>
    float FramesAdvanced = 0.0f;
    /// <summary>Frames played so far in the current state.</summary>
    int32_t LastFrame = 0;
    /// <summary>The frame the animation loops back to at <see cref="LoopEnd"/>.</summary>
    int32_t LoopStart = -1;
    /// <summary>The frame at which the animation goes back to <see cref="LoopStart"/> (-1: loop the whole state).</summary>
    int32_t LoopEnd = -1;
    /// <summary>The current state.</summary>
    MCActorState CurrentState = ACTOR_STATE_NORMAL;
    /// <summary>The current sub-state of state 0, 0xFF for none.</summary>
    uint8_t CurrentSubState = 0xff;
    /// <summary>The fade table the shape is drawn through, or null.</summary>
    uint8_t* FadeTable = nullptr;
    /// <summary>Set by <see cref="SetDamageLvl"/>.</summary>
    int32_t DamageSet = 0;
    /// <summary>Set by <see cref="SetTypeId"/> and init.</summary>
    int32_t TypeChanged = 1;
    /// <summary>The shape's top-left offset from its hotspot (from VFX_shape_minxy; -15 before a shape).</summary>
    float ShapeMinX = -15.0f;
    float ShapeMinY = -15.0f;
    /// <summary>The shape's size (VFX_shape_resolution; 15 before a shape).</summary>
    float ShapeMaxX = 15.0f;
    float ShapeMaxY = 15.0f;
};

/// <summary>
/// Draws the damage bar of <paramref name="obj"/> (a building or tree building) above
/// <paramref name="appearance"/>: centred over <paramref name="type"/>'s bounds, or the appearance's when the type
/// has none. Nothing when <paramref name="obj"/> is null.
/// </summary>
/// <remarks>
/// Port helper: the body of <c>VFXAppearance::drawBars</c> and
/// <c>VFXBuildingAppearance::drawBars</c> (0x0063a960), which the original repeats.
/// </remarks>
void MCDrawDamageBar(MCAppearance* appearance, MCAppearanceType* type, MCGameObject* obj);
