#pragma once

#include "appear/MCAppearance.h"
#include "sprite/MCArmAppearanceType.h"

/// <summary>A weapon effect's appearance, turned to its owner's direction of travel.</summary>
/// <remarks>Original source: <c>sprite\armactor.cpp</c>.</remarks>
class MCArmAppearance : public MCAppearance
{
public:
    MCArmAppearance() = default;
    /// <summary>Leaves the type's users and drops the type.</summary>
    ~MCArmAppearance() override;

    int32_t Init(MCAppearanceType* tree = nullptr, MCGameObject* obj = nullptr) override;

    /// <summary>Advances the animation (stopping on the last frame).</summary>
    int32_t Update() override;

    /// <summary>Faces the owner's direction and adds the shape to the draw list.</summary>
    int32_t Render(int32_t depthFixup = 0) override;

    int RecalcBounds(MCCamera* cam) override;

    int32_t GetFrameNumber() override { return CurrentFrame; }

    /// <summary>The object the effect belongs to; set by the owner.</summary>
    MCGameObject* OwnerObject = nullptr;
    /// <summary>The type.</summary>
    MCArmAppearanceType* AppearType = nullptr;
    /// <summary>The shape drawn.</summary>
    MCShape* CurrentShape = nullptr;
    /// <summary>The frame drawn (-1: not started).</summary>
    int32_t CurrentFrame = -1;
    /// <summary>Whether the shape is drawn mirrored.</summary>
    bool Reverse = false;
    /// <summary>Seconds into the animation.</summary>
    float CurrentTime = 0.0f;
    /// <summary>The state's frame rate.</summary>
    float FrameRate = 0.0f;
    /// <summary>Frames played so far.</summary>
    int32_t LastFrame = 0;
    /// <summary>The facing, in degrees.</summary>
    float Rotation = 0.0f;
    /// <summary>The fade table (index into the palette's haze tables) to draw through, -1 for none.</summary>
    int32_t FadeTableIndex = -1;
    /// <summary>The shape's top-left offset from its hotspot (-15 before a shape).</summary>
    float ShapeMinX = -15.0f;
    float ShapeMinY = -15.0f;
    /// <summary>The shape's size (15 before a shape).</summary>
    float ShapeMaxX = 15.0f;
    float ShapeMaxY = 15.0f;
};
