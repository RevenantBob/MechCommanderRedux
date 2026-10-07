#pragma once

#include "appear/appear.h"
#include "appear/apprtype.h"

class MCShape;

/// <summary>
/// The states of a ground vehicle's appearance. The type has 3 (or 4) "State%d" blocks; state 2 draws no turret.
/// The names are the port's.
/// </summary>
enum MCGVActorState : int32_t
{
    GV_ACTOR_STATE_NORMAL = 0,
    GV_ACTOR_STATE_DAMAGED = 1,
    GV_ACTOR_STATE_DESTROYED = 2,
    /// <summary>Only when the type's NumStates is 4.</summary>
    GV_ACTOR_STATE_EXTRA = 3,
    MAX_GV_ACTOR_STATES = 4
};

/// <summary>A state of a ground vehicle type (FIT "State%d"), 0x14 bytes.</summary>
struct MCGVActorData
{
    /// <summary>FIT "State".</summary>
    MCGVActorState State;
    /// <summary>FIT "NumRotations".</summary>
    uint8_t NumRotations;
    /// <summary>FIT "NumFrames".</summary>
    uint32_t NumFrames;
    /// <summary>FIT "BasePacketNumber": the body's first packet; the turret's follow each part's rotations.</summary>
    uint32_t BasePacketNumber;
    /// <summary>FIT "FrameRate".</summary>
    float FrameRate;
};

/// <summary>
/// The type of a <see cref="MCGVAppearance"/>: a ground vehicle's body and turret shapes per state and facing.
/// </summary>
/// <remarks>
/// Original source: <c>sprite\gvactor.cpp</c>, 0x44 bytes (class 5 of the sprite PAK). FIT: "Main Info" (NumParts,
/// TurretOffset), "States" (NumStates, 3 or 4), "State%d".
/// </remarks>
class MCGVAppearanceType : public MCAppearanceType
{
public:
    MCGVAppearanceType() = default;
    ~MCGVAppearanceType() override { MCGVAppearanceType::Destroy(); }

    int32_t Init(MCFile* apprFile, uint32_t fileSize, uint32_t loadFlags) override;

    void Destroy() override;

    /// <summary>Forgets <paramref name="shape"/> in the list and in every user's body and turret.</summary>
    void RemoveShape(MCShape* shape) override;

    /// <summary>Reads the type's FIT.</summary>
    int32_t LoadIniFile(MCFile* apprFile, uint32_t fileSize);

    /// <summary>
    /// The shape of part <paramref name="part"/> (0 body, 1 turret) in <paramref name="state"/> facing
    /// <paramref name="rotation"/> degrees; <paramref name="frameRate"/> gets the state's frame rate.
    /// </summary>
    MCShape* GetShape(MCGVActorState state, int32_t rotation, int32_t part, float& frameRate);

    /// <summary>The states (4 entries).</summary>
    MCGVActorData* ActorStateData = nullptr;
    /// <summary>The loaded shape of each packet.</summary>
    MCShape** ShapeList = nullptr;
    /// <summary>FIT "NumParts": 1, or 2 with a turret.</summary>
    uint32_t NumParts = 0;
    /// <summary>FIT "TurretOffset": how far the turret sits from the body's centre, in metres.</summary>
    float TurretOffset = 0.0f;
    /// <summary>The number of packets.</summary>
    int32_t NumPackets = 0;
    /// <summary>Nonzero when the type has the fourth state.</summary>
    int32_t HasExtraState = 0;
};

/// <summary>A ground vehicle: a body and an optional turret, each facing its own way.</summary>
/// <remarks>Original source: <c>sprite\gvactor.cpp</c>, <c>sprite\gvactor.h</c>; 0x98 bytes.</remarks>
class MCGVAppearance : public MCAppearance
{
public:
    MCGVAppearance() = default;
    ~MCGVAppearance() override { MCGVAppearance::Destroy(); }

    int32_t Init(MCAppearanceType* tree = nullptr, MCGameObject* obj = nullptr) override;

    void Destroy() override;

    /// <summary>Advances the animation and starts the target highlight.</summary>
    int32_t Update() override;

    /// <summary>Turns body and turret to the owner's facings and adds them to the element list.</summary>
    int32_t Render(int32_t depthFixup = 0) override;

    MCAppearanceType* GetAppearanceType() override { return AppearType; }

    /// <summary>Draws the vehicle's damage bar.</summary>
    void DrawBars() override;

    int RecalcBounds(MCCamera* cam) override;

    /// <summary>Switches to <paramref name="state"/> when the type has it.</summary>
    virtual void SetTypeId(MCGVActorState state)
    {
        if (StateExists(state))
        {
            CurrentState = state;
        }
    }

    /// <summary>The number of frames of <paramref name="state"/> (0 when absent).</summary>
    int32_t StateExists(MCGVActorState state);

    /// <summary>The type.</summary>
    MCGVAppearanceType* AppearType = nullptr;
    /// <summary>The body's and the turret's shapes.</summary>
    MCShape* CurrentShape[2] = {};
    /// <summary>The body's and the turret's frames (-1: not started).</summary>
    int32_t CurrentFrame[2] = {-1, -1};
    /// <summary>
    /// The parts render draws, in order: set to {0, 1} there and walked as its loop index (a local array the
    /// compiler kept in the object).
    /// </summary>
    int32_t PartOrder[2] = {};
    /// <summary>The type's part count.</summary>
    int32_t NumParts = 0;
    /// <summary>The type's turret offset.</summary>
    float TurretOffset = 0.0f;
    /// <summary>Seconds into the animation.</summary>
    float CurrentTime = 0.0f;
    /// <summary>The frame rate (15 after init).</summary>
    float FrameRate = 15.0f;
    /// <summary>Frames played so far.</summary>
    int32_t LastFrame = 0;
    /// <summary>The current state.</summary>
    MCGVActorState CurrentState = GV_ACTOR_STATE_NORMAL;
    /// <summary>The body's facing, in degrees.</summary>
    float BodyRotation = 0.0f;
    /// <summary>The turret's facing, in degrees (-180..180 then 0..360).</summary>
    float TurretRotation = 0.0f;
    /// <summary>The fade table (haze table index) to draw through, -1 for none.</summary>
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
