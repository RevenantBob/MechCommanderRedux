#pragma once

#include "appear/appear.h"
#include "sprite/elmtree.h"

class Shape;

/// <summary>An elemental (armoured infantry) squad's appearance: gestures (walk, jump...) from its tree.</summary>
/// <remarks>Original source: <c>sprite\lactor.cpp</c>, <c>sprite\lactor.h</c>; 0xb8 bytes.</remarks>
class ElementalActor : public Appearance
{
public:
    ElementalActor() = default;
    /// <remarks>MCX.EXE @ 0x0065bec0 (vector deleting destructor); slot 2</remarks>
    ~ElementalActor() override { ElementalActor::destroy(); }

    /// <remarks>MCX.EXE @ 0x0063c570; slot 0</remarks>
    int32_t init(AppearanceType* tree = nullptr, GameObject* obj = nullptr) override;

    /// <remarks>MCX.EXE @ 0x0063ce30; slot 1</remarks>
    void destroy() override;

    /// <summary>Advances the gesture, switching to the goal gesture when a jump ends.</summary>
    /// <remarks>MCX.EXE @ 0x0063cbc0; slot 3</remarks>
    int32_t update() override;

    /// <remarks>MCX.EXE @ 0x0063c950; slot 4</remarks>
    int32_t render(int32_t depthFixup = 0) override;

    /// <remarks>MCX.EXE @ 0x0065be70 (lactor.h); slot 5</remarks>
    AppearanceType* getAppearanceType() override { return appearType; }

    /// <summary>Nothing.</summary>
    /// <remarks>MCX.EXE @ 0x0063ce50; slot 6</remarks>
    void drawBars() override;

    /// <remarks>MCX.EXE @ 0x0063c6f0; slot 7</remarks>
    int recalcBounds(Camera* cam) override;

    /// <summary>Jumps straight to gesture <paramref name="gesture"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0065be80 (lactor.h); slot 8</remarks>
    int32_t setGesture(uint32_t gesture) override
    {
        currentGesture = static_cast<int32_t>(gesture);
        oldGesture = static_cast<int32_t>(gesture);
        gestureGoal = static_cast<int32_t>(gesture);
        return 0;
    }

    /// <summary>
    /// Sets the gesture to change to (2, the jump, only after <see cref="setJumpParameters"/>); fails while a goal
    /// is pending.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0063c510; slot 9</remarks>
    int32_t setGestureGoal(int32_t goal) override;

    /// <summary>Scales the animation speed, keeping the animation's position.</summary>
    /// <remarks>MCX.EXE @ 0x0063c6c0; slot 10</remarks>
    void setVelocityPercentage(float percent) override;

    /// <remarks>MCX.EXE @ 0x0065beb0 (lactor.h); slot 11</remarks>
    int32_t getFrameNumber() override { return currentFrame; }

    /// <remarks>MCX.EXE @ 0x0065bea0 (lactor.h); slot 13</remarks>
    virtual int32_t getGesture() { return currentGesture; }

    /// <summary>The frames of gesture <paramref name="gesture"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0063c480</remarks>
    float getNumFramesInGesture(uint32_t gesture);

    /// <summary>The velocity of gesture <paramref name="gesture"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0063c4c0</remarks>
    float getVelocityOfGesture(uint32_t gesture);

    /// <summary>Loads the tree's shapes for a gesture (the tree does nothing).</summary>
    /// <remarks>MCX.EXE @ 0x0063c4f0</remarks>
    void preloadGestures(int32_t gesture, float rotation);

    /// <summary>Prepares a jump of <paramref name="jumpDistance"/>: the jump's velocity to cover it in the gesture's time.</summary>
    /// <remarks>MCX.EXE @ 0x0063c640</remarks>
    int32_t setJumpParameters(float jumpDistance);

    /// <summary>The current gesture's velocity (the jump's while jumping).</summary>
    /// <remarks>MCX.EXE @ 0x0063c690</remarks>
    float getVelocityMagnitude();

    /// <summary>The type.</summary>
    ElementalTree* appearType = nullptr; // +0x38
    /// <summary>The shape drawn.</summary>
    Shape* currentShape = nullptr; // +0x3c
    /// <summary>The frame drawn (-1: not started).</summary>
    int32_t currentFrame = -1; // +0x40
    /// <summary>Cleared by init; never otherwise used.</summary>
    int32_t unknown44 = 0; // +0x44
    /// <summary>The gesture's frame rate (15 after init).</summary>
    float frameRate = 15.0f; // +0x48
    /// <summary>Seconds into the gesture.</summary>
    float currentTime = 0.0f; // +0x4c
    /// <summary>Frames played so far.</summary>
    int32_t lastFrame = 0; // +0x50
    /// <summary>Cleared by init; never otherwise used.</summary>
    int32_t unknown54 = 0; // +0x54
    /// <summary>The gesture playing.</summary>
    int32_t currentGesture = 0; // +0x58
    /// <summary>The gesture to change to.</summary>
    int32_t gestureGoal = 0; // +0x5c
    /// <summary>The previous gesture.</summary>
    int32_t oldGesture = 0; // +0x60
    /// <summary>The velocity (the gesture's, or the jump's).</summary>
    float velocity = 0.0f; // +0x64
    int32_t unknown68 = 0; // +0x68 (never accessed)
    /// <summary>The animation speed factor (1 after init).</summary>
    float velocityPercentage = 1.0f; // +0x6c
    /// <summary>Nonzero while jumping.</summary>
    int32_t jumping = 0; // +0x70
    /// <summary>Cleared by init; never otherwise used.</summary>
    int32_t unknown74[4] = {}; // +0x74
    /// <summary>Nonzero once <see cref="setJumpParameters"/> has set a jump up.</summary>
    int32_t jumpSetup = 0; // +0x84
    int32_t unknown88 = 0; // +0x88 (never accessed)
    /// <summary>1 after init; when 0 the jump gesture doesn't advance.</summary>
    int32_t unknown8C = 1; // +0x8c
    /// <summary>Cleared by init; never otherwise used.</summary>
    int32_t unknown90[4] = {}; // +0x90
    /// <summary>Nonzero while a gesture goal is pending.</summary>
    int32_t goalPending = 0; // +0xa0
    /// <summary>The fade table (haze table index) to draw through, -1 for none.</summary>
    int32_t fadeTableIndex = -1; // +0xa4
    /// <summary>The shape's top-left offset from its hotspot (-15 before a shape).</summary>
    float shapeMinX = -15.0f; // +0xa8
    float shapeMinY = -15.0f; // +0xac
    /// <summary>The shape's size (30 before a shape).</summary>
    float shapeMaxX = 30.0f; // +0xb0
    float shapeMaxY = 30.0f; // +0xb4
};
