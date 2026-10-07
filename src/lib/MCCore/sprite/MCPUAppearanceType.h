#pragma once

#include "appear/MCAppearanceType.h"

/// <summary>
/// The states of a pop-up turret's appearance (the type has exactly 6). The names are the port's; the values follow
/// <see cref="MCPUAppearance::SetCombatMode"/> and <see cref="MCPUAppearance::SetDestroyed"/>.
/// </summary>
enum class MCPUActorState : int32_t
{
    Closed = 0,
    Opening = 1,
    Open = 2,
    Closing = 3,
    /// <summary>Destroyed while open (or opening/closing).</summary>
    DestroyedOpen = 4,
    /// <summary>Destroyed while closed.</summary>
    DestroyedClosed = 5
};

/// <summary>The number of states of a pop-up turret type.</summary>
inline constexpr int32_t PUActorStateCount = 6;

/// <summary>A state of a pop-up turret type (FIT "State%d").</summary>
struct MCPUActorData
{
    /// <summary>FIT "State".</summary>
    MCPUActorState State = MCPUActorState::Closed;
    /// <summary>FIT "NumRotations".</summary>
    uint8_t NumRotations = 0;
    /// <summary>FIT "NumFrames".</summary>
    uint32_t NumFrames = 0;
    /// <summary>FIT "BasePacketNumber".</summary>
    uint32_t BasePacketNumber = 0;
    /// <summary>FIT "FrameRate".</summary>
    float FrameRate = 0.0f;
};

/// <summary>The type of a <see cref="MCPUAppearance"/>: a pop-up turret's shapes per state and facing.</summary>
/// <remarks>
/// Original source: <c>sprite\puactor.cpp</c> (class 9 of the sprite PAK). FIT: "Main Info", "States" (NumStates = 6,
/// Scaled), "State%d".
/// </remarks>
class MCPUAppearanceType : public MCAppearanceType
{
public:
    MCPUAppearanceType() = default;
    /// <summary>Leaves the type's shapes in the cache without an owner.</summary>
    ~MCPUAppearanceType() override;

    /// <summary>Reads the FIT and makes the shape list (one entry per packet).</summary>
    MCAppearanceLoad Load(MCFile& apprFile, uint32_t fileSize) override;

    void RemoveShape(MCShape* shape) override;

    /// <summary>The data of <paramref name="state"/>.</summary>
    const MCPUActorData& StateData(MCPUActorState state) const { return States[static_cast<size_t>(state)]; }

    /// <summary>
    /// The shape of part <paramref name="part"/> in <paramref name="state"/> facing <paramref name="rotation"/>
    /// degrees (the zoomed-out one when scaled); <paramref name="frameRate"/> gets the state's frame rate.
    /// </summary>
    MCShape* GetShape(MCPUActorState state, int32_t rotation, int32_t part, float& frameRate);

    /// <summary>The states.</summary>
    std::array<MCPUActorData, PUActorStateCount> States{};
    /// <summary>The loaded shape of each packet.</summary>
    std::vector<MCShape*> ShapeList;
    /// <summary>FIT "Scaled": each rotation run is followed by its zoomed-out run.</summary>
    bool Scaled = false;
};
