#pragma once

#include "appear/MCAppearanceType.h"

/// <summary>
/// The states of a ground vehicle's appearance. The type has 3 (or 4) "State%d" blocks; the destroyed state draws no
/// turret. The names are the port's.
/// </summary>
enum class MCGVActorState : int32_t
{
    Normal = 0,
    Damaged = 1,
    Destroyed = 2,
    /// <summary>Only when the type's NumStates is 4.</summary>
    Extra = 3
};

/// <summary>The most states a ground vehicle type has.</summary>
inline constexpr int32_t GVActorStateCount = 4;

/// <summary>A state of a ground vehicle type (FIT "State%d").</summary>
struct MCGVActorData
{
    /// <summary>FIT "State".</summary>
    MCGVActorState State = MCGVActorState::Normal;
    /// <summary>FIT "NumRotations".</summary>
    uint8_t NumRotations = 0;
    /// <summary>FIT "NumFrames".</summary>
    uint32_t NumFrames = 0;
    /// <summary>FIT "BasePacketNumber": the body's first packet; the turret's follow each part's rotations.</summary>
    uint32_t BasePacketNumber = 0;
    /// <summary>FIT "FrameRate".</summary>
    float FrameRate = 0.0f;
};

/// <summary>
/// The type of a <see cref="MCGVAppearance"/>: a ground vehicle's body and turret shapes per state and facing.
/// </summary>
/// <remarks>
/// Original source: <c>sprite\gvactor.cpp</c> (class 5 of the sprite PAK). FIT: "Main Info" (NumParts,
/// TurretOffset), "States" (NumStates, 3 or 4), "State%d".
/// </remarks>
class MCGVAppearanceType : public MCAppearanceType
{
public:
    MCGVAppearanceType() = default;
    /// <summary>Leaves the type's shapes in the cache without an owner.</summary>
    ~MCGVAppearanceType() override;

    /// <summary>Reads the FIT and makes the shape list (one entry per packet).</summary>
    MCAppearanceLoad Load(MCFile& apprFile, uint32_t fileSize) override;

    /// <summary>Forgets <paramref name="shape"/> in the list and in every user's body and turret.</summary>
    void RemoveShape(MCShape* shape) override;

    /// <summary>The data of <paramref name="state"/>.</summary>
    const MCGVActorData& StateData(MCGVActorState state) const { return States[static_cast<size_t>(state)]; }

    /// <summary>
    /// The shape of part <paramref name="part"/> (0 body, 1 turret) in <paramref name="state"/> facing
    /// <paramref name="rotation"/> degrees; <paramref name="frameRate"/> gets the state's frame rate.
    /// </summary>
    MCShape* GetShape(MCGVActorState state, int32_t rotation, int32_t part, float& frameRate);

    /// <summary>The states.</summary>
    std::array<MCGVActorData, GVActorStateCount> States{};
    /// <summary>The loaded shape of each packet.</summary>
    std::vector<MCShape*> ShapeList;
    /// <summary>FIT "NumParts": 1, or 2 with a turret.</summary>
    uint32_t NumParts = 0;
    /// <summary>FIT "TurretOffset": how far the turret sits from the body's centre, in metres.</summary>
    float TurretOffset = 0.0f;
    /// <summary>Whether the type has the fourth state.</summary>
    bool HasExtraState = false;
};
