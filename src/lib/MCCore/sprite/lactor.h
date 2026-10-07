#pragma once

#include "appear/appear.h"
#include "sprite/elmtree.h"

class MCShape;

/// <summary>An elemental (armoured infantry) squad's appearance: gestures (walk, jump...) from its tree.</summary>
/// <remarks>Original source: <c>sprite\lactor.cpp</c>, <c>sprite\lactor.h</c>; 0xb8 bytes.</remarks>
class MCElementalActor : public MCAppearance
{
public:
    MCElementalActor() = default;
    ~MCElementalActor() override { MCElementalActor::Destroy(); }

    int32_t Init(MCAppearanceType* tree = nullptr, MCGameObject* obj = nullptr) override;

    void Destroy() override;

    /// <summary>Advances the gesture, switching to the goal gesture when a jump ends.</summary>
    int32_t Update() override;

    int32_t Render(int32_t depthFixup = 0) override;

    MCAppearanceType* GetAppearanceType() override { return AppearType; }

    /// <summary>Nothing.</summary>
    void DrawBars() override;

    int RecalcBounds(MCCamera* cam) override;

    /// <summary>Jumps straight to gesture <paramref name="gesture"/>.</summary>
    int32_t SetGesture(uint32_t gesture) override
    {
        CurrentGesture = static_cast<int32_t>(gesture);
        OldGesture = static_cast<int32_t>(gesture);
        GestureGoal = static_cast<int32_t>(gesture);
        return 0;
    }

    /// <summary>
    /// Sets the gesture to change to (2, the jump, only after <see cref="SetJumpParameters"/>); fails while a goal
    /// is pending.
    /// </summary>
    int32_t SetGestureGoal(int32_t goal) override;

    /// <summary>Scales the animation speed, keeping the animation's position.</summary>
    void SetVelocityPercentage(float percent) override;

    int32_t GetFrameNumber() override { return CurrentFrame; }

    virtual int32_t GetGesture() { return CurrentGesture; }

    /// <summary>The frames of gesture <paramref name="gesture"/>.</summary>
    float GetNumFramesInGesture(uint32_t gesture);

    /// <summary>The velocity of gesture <paramref name="gesture"/>.</summary>
    float GetVelocityOfGesture(uint32_t gesture);

    /// <summary>Loads the tree's shapes for a gesture (the tree does nothing).</summary>
    void PreloadGestures(int32_t gesture, float rotation);

    /// <summary>Prepares a jump of <paramref name="jumpDistance"/>: the jump's velocity to cover it in the gesture's time.</summary>
    int32_t SetJumpParameters(float jumpDistance);

    /// <summary>The current gesture's velocity (the jump's while jumping).</summary>
    float GetVelocityMagnitude();

    /// <summary>The type.</summary>
    MCElementalTree* AppearType = nullptr;
    /// <summary>The shape drawn.</summary>
    MCShape* CurrentShape = nullptr;
    /// <summary>The frame drawn (-1: not started).</summary>
    int32_t CurrentFrame = -1;
    /// <summary>The gesture's frame rate (15 after init).</summary>
    float FrameRate = 15.0f;
    /// <summary>Seconds into the gesture.</summary>
    float CurrentTime = 0.0f;
    /// <summary>Frames played so far.</summary>
    int32_t LastFrame = 0;
    /// <summary>The gesture playing.</summary>
    int32_t CurrentGesture = 0;
    /// <summary>The gesture to change to.</summary>
    int32_t GestureGoal = 0;
    /// <summary>The previous gesture.</summary>
    int32_t OldGesture = 0;
    /// <summary>The velocity (the gesture's, or the jump's).</summary>
    float Velocity = 0.0f;
    /// <summary>The animation speed factor (1 after init).</summary>
    float VelocityPercentage = 1.0f;
    /// <summary>Nonzero while jumping.</summary>
    int32_t Jumping = 0;
    /// <summary>Nonzero once <see cref="SetJumpParameters"/> has set a jump up.</summary>
    int32_t JumpSetup = 0;
    /// <summary>Nonzero while a gesture goal is pending.</summary>
    int32_t GoalPending = 0;
    /// <summary>The fade table (haze table index) to draw through, -1 for none.</summary>
    int32_t FadeTableIndex = -1;
    /// <summary>The shape's top-left offset from its hotspot (-15 before a shape).</summary>
    float ShapeMinX = -15.0f;
    float ShapeMinY = -15.0f;
    /// <summary>The shape's size (30 before a shape).</summary>
    float ShapeMaxX = 30.0f;
    float ShapeMaxY = 30.0f;
};
