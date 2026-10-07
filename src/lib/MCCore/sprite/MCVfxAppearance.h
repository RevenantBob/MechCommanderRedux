#pragma once

#include "appear/MCAppearance.h"
#include "sprite/MCVfxAppearanceType.h"

/// <summary>An object drawn from its type's shapes, animated through its states and facings.</summary>
/// <remarks>Original source: <c>sprite\actor.cpp</c>, <c>sprite\actor.h</c>.</remarks>
class MCVfxAppearance : public MCAppearance
{
public:
    MCVfxAppearance() = default;
    /// <summary>Leaves the type's users and drops the type.</summary>
    ~MCVfxAppearance() override;

    /// <summary>Binds to <paramref name="tree"/> (registering as a user) and resets the animation.</summary>
    int32_t Init(MCAppearanceType* tree = nullptr, MCGameObject* obj = nullptr) override;

    /// <summary>Advances the animation by the frame time.</summary>
    int32_t Update() override;

    /// <summary>Adds the current shape (and the selection box, bars) to the draw list.</summary>
    int32_t Render(int32_t depthFixup = 0) override;

    MCAppearanceType* GetAppearanceType() override { return AppearType; }

    /// <summary>Draws the damage bar over a tree building.</summary>
    void DrawBars() override;

    /// <summary>Takes the shape's bounds and projects the owner; nonzero when on screen.</summary>
    int RecalcBounds(MCCamera* cam) override;

    /// <summary>
    /// Switches to <paramref name="state"/> (restarting it) and, in the normal state, to sub-state
    /// <paramref name="subState"/> (<see cref="NoSubState"/> for none).
    /// </summary>
    void SetTypeId(MCActorState state, uint8_t subState);

    /// <summary>Shows damage level <paramref name="damageLevel"/>: normal, blowing up 1 or 2, or destroyed.</summary>
    virtual void SetDamageLvl(uint32_t damageLevel);

    /// <summary>The number of frames of <paramref name="state"/> (0 when the type hasn't it).</summary>
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
    MCActorState CurrentState = MCActorState::Normal;
    /// <summary>The current sub-state of the normal state, <see cref="NoSubState"/> for none.</summary>
    uint8_t CurrentSubState = NoSubState;
    /// <summary>The fade table the shape is drawn through, or null.</summary>
    uint8_t* FadeTable = nullptr;
    /// <summary>Set by <see cref="SetDamageLvl"/>.</summary>
    bool DamageSet = false;
    /// <summary>Set by <see cref="SetTypeId"/> and init.</summary>
    bool TypeChanged = true;
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
/// <remarks>The body of <c>VFXAppearance::drawBars</c> and <c>VFXBuildingAppearance::drawBars</c>, which the original
/// repeats.</remarks>
void MCDrawDamageBar(MCAppearance* appearance, MCAppearanceType* type, MCGameObject* obj);
