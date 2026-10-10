#pragma once

#include "appear/MCAppearance.h"
#include "sprite/MCGVAppearanceType.h"

/// <summary>A ground vehicle: a body and an optional turret, each facing its own way.</summary>
/// <remarks>Original source: <c>sprite\gvactor.cpp</c>, <c>sprite\gvactor.h</c>.</remarks>
class MCGVAppearance : public MCAppearance
{
public:
    MCGVAppearance() = default;
    /// <summary>Leaves the type's users and drops the type.</summary>
    ~MCGVAppearance() override;

    int32_t Init(MCAppearanceType* tree = nullptr, MCGameObject* obj = nullptr) override;

    /// <summary>Advances the animation and starts the target highlight.</summary>
    int32_t Update() override;

    /// <summary>Turns body and turret to the owner's facings and adds them to the draw list.</summary>
    int32_t Render(int32_t depthFixup = 0) override;

    MCAppearanceType* GetAppearanceType() override { return AppearType; }

    /// <summary>Draws the vehicle's damage bar.</summary>
    void DrawBars() override;

    int RecalcBounds(MCCamera* cam) override;

    /// <summary>Switches to <paramref name="state"/> when the type has it.</summary>
    void SetTypeId(MCGVActorState state)
    {
        if (StateExists(state) != 0)
        {
            CurrentState = state;
        }
    }

    /// <summary>The number of frames of <paramref name="state"/> (0 when the type hasn't it).</summary>
    int32_t StateExists(MCGVActorState state) const;

    /// <summary>The type.</summary>
    MCGVAppearanceType* AppearType = nullptr;
    /// <summary>The body's and the turret's shapes.</summary>
    std::array<MCShape*, 2> CurrentShape{};
    /// <summary>The body's and the turret's frames (-1: not started).</summary>
    std::array<int32_t, 2> CurrentFrame{-1, -1};
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
    MCGVActorState CurrentState = MCGVActorState::Normal;
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
