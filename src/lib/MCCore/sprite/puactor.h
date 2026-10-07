#pragma once

#include "appear/appear.h"
#include "appear/apprtype.h"

class MCShape;

/// <summary>
/// The states of a pop-up turret's appearance (the type must have exactly 6). The names are the port's; the values
/// follow <see cref="MCPUAppearance::SetCombatMode"/> and <see cref="MCPUAppearance::SetDestroyed"/>.
/// </summary>
enum MCPUActorState : int32_t
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
struct MCPUActorData
{
    /// <summary>FIT "State".</summary>
    MCPUActorState State;
    /// <summary>FIT "NumRotations".</summary>
    uint8_t NumRotations;
    /// <summary>FIT "NumFrames".</summary>
    uint32_t NumFrames;
    /// <summary>FIT "BasePacketNumber".</summary>
    uint32_t BasePacketNumber;
    /// <summary>FIT "FrameRate".</summary>
    float FrameRate;
};

/// <summary>The type of a <see cref="MCPUAppearance"/>: a pop-up turret's shapes per state and facing.</summary>
/// <remarks>
/// Original source: <c>sprite\puactor.cpp</c>, 0x3c bytes (class 9 of the sprite PAK). FIT: "Main Info", "States"
/// (NumStates = 6, Scaled), "State%d".
/// </remarks>
class MCPUAppearanceType : public MCAppearanceType
{
public:
    MCPUAppearanceType() = default;
    ~MCPUAppearanceType() override { MCPUAppearanceType::Destroy(); }

    int32_t Init(MCFile* apprFile, uint32_t fileSize, uint32_t loadFlags) override;

    void Destroy() override;

    void RemoveShape(MCShape* shape) override;

    /// <summary>Reads the type's FIT.</summary>
    int32_t LoadIniFile(MCFile* apprFile, uint32_t fileSize);

    /// <summary>
    /// The shape of part <paramref name="part"/> in <paramref name="state"/> facing <paramref name="rotation"/>
    /// degrees (the zoomed-out one when scaled); <paramref name="frameRate"/> gets the state's frame rate.
    /// </summary>
    MCShape* GetShape(MCPUActorState state, int32_t rotation, int32_t part, float& frameRate);

    /// <summary>The 6 states.</summary>
    MCPUActorData* ActorStateData = nullptr;
    /// <summary>The loaded shape of each packet.</summary>
    MCShape** ShapeList = nullptr;
    /// <summary>FIT "Scaled": each rotation run is followed by its zoomed-out run.</summary>
    uint8_t Scaled = 0;
    /// <summary>The number of packets.</summary>
    int32_t NumPackets = 0;
};

/// <summary>A pop-up turret: opens to fire, closes to hide.</summary>
/// <remarks>Original source: <c>sprite\puactor.cpp</c>, <c>sprite\puactor.h</c>; 0x78 bytes.</remarks>
class MCPUAppearance : public MCAppearance
{
public:
    MCPUAppearance() = default;
    ~MCPUAppearance() override { MCPUAppearance::Destroy(); }

    int32_t Init(MCAppearanceType* tree = nullptr, MCGameObject* obj = nullptr) override;

    void Destroy() override;

    /// <summary>Advances the animation (stopping on the state's last frame) and starts the target highlight.</summary>
    int32_t Update() override;

    int32_t Render(int32_t depthFixup = 0) override;

    MCAppearanceType* GetAppearanceType() override { return AppearType; }

    /// <summary>Draws the turret's damage bar.</summary>
    void DrawBars() override;

    int RecalcBounds(MCCamera* cam) override;

    /// <summary>Switches to <paramref name="state"/> when the type has it.</summary>
    virtual void SetTypeId(MCPUActorState state)
    {
        if (StateExists(state))
        {
            CurrentState = state;
        }
    }

    /// <summary>Switches to the destroyed state matching the current one.</summary>
    void SetDestroyed();

    /// <summary>
    /// Opens (<paramref name="combatMode"/> nonzero) or closes the turret, stepping through the opening/closing
    /// states as their animations finish. Returns the state.
    /// </summary>
    int32_t SetCombatMode(int combatMode);

    /// <summary>The number of frames of <paramref name="state"/> (0 when absent).</summary>
    int32_t StateExists(MCPUActorState state);

    /// <summary>The type.</summary>
    MCPUAppearanceType* AppearType = nullptr;
    /// <summary>The shape drawn.</summary>
    MCShape* CurrentShape = nullptr;
    /// <summary>The frame drawn (-1: not started).</summary>
    int32_t CurrentFrame = -1;
    /// <summary>Seconds into the animation.</summary>
    float CurrentTime = 0.0f;
    /// <summary>The frame rate (15 after init).</summary>
    float FrameRate = 15.0f;
    /// <summary>Frames played so far.</summary>
    int32_t LastFrame = 0;
    /// <summary>The current state.</summary>
    MCPUActorState CurrentState = PU_ACTOR_STATE_CLOSED;
    /// <summary>The facing, in degrees.</summary>
    float Rotation = 0.0f;
    /// <summary>The fade table (haze table index) to draw through, -1 for none; set by the owner.</summary>
    int32_t FadeTableIndex = -1;
    /// <summary>The haze palette Turret::render hands over; the appearance never reads it.</summary>
    uint8_t* HazePalette = nullptr;
    /// <summary>The shape's top-left offset from its hotspot (-25 before a shape).</summary>
    float ShapeMinX = -25.0f;
    float ShapeMinY = -25.0f;
    /// <summary>The shape's size (50 before a shape).</summary>
    float ShapeMaxX = 50.0f;
    float ShapeMaxY = 50.0f;
};
