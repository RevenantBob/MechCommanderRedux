#pragma once

#include "lib/cvmath.h"

class MCAppearanceType;
class MCCamera;
class MCGameObject;

/// <summary>
/// What an object looks like on screen: the base of every actor (mechs, vehicles, buildings, effects...). The
/// object owns one; each frame the object queue calls <see cref="Update"/>, <see cref="RecalcBounds"/> against the
/// camera and <see cref="Render"/>, which adds draw elements to the global <c>ElementList</c>.
/// </summary>
/// <remarks>
/// Original source: <c>appear\appear.h</c> (inline virtuals) and <c>appear\appear.cpp</c>, 0x38 bytes. The constructor was inlined at every <c>new</c> site: it clears
/// the fields (it also set a never-read field at +0x04 to 0x70000000).
/// </remarks>
class MCAppearance
{
public:
    /// <summary>The inlined constructor of every allocation site.</summary>
    MCAppearance() = default;
    /// <summary>Calls <see cref="Destroy"/>.</summary>
    virtual ~MCAppearance() { Init(nullptr, nullptr); }

    /// <summary>Binds the appearance to its type and its object.</summary>
    virtual int32_t Init(MCAppearanceType* tree = nullptr, MCGameObject* obj = nullptr)
    {
        Owner = obj;
        Visible = 0;
        return 0;
    }

    /// <summary>Resets the appearance (the base calls <see cref="Init"/> with nothing).</summary>
    virtual void Destroy() { Init(nullptr, nullptr); }

    /// <summary>Advances animation. Returns nonzero when it wants to be drawn.</summary>
    virtual int32_t Update() { return 0; }

    /// <summary>Adds the appearance's elements to the element list at <paramref name="depthFixup"/>.</summary>
    virtual int32_t Render(int32_t depthFixup = 0) { return 0; }

    /// <summary>The appearance's type (none for the base).</summary>
    virtual MCAppearanceType* GetAppearanceType() { return nullptr; }

    /// <summary>Draws the damage/status bars over the object (nothing for the base).</summary>
    virtual void DrawBars();

    /// <summary>
    /// Recomputes the screen position and bounds against <paramref name="cam"/>. Returns nonzero when on screen.
    /// </summary>
    virtual int RecalcBounds(MCCamera* cam)
    {
        UpperLeft.X = 0.0f;
        UpperLeft.Y = 0.0f;
        LowerRight.X = 0.0f;
        LowerRight.Y = 0.0f;
        return 0;
    }

    /// <summary>Starts gesture <paramref name="gesture"/> (actors that animate by gesture).</summary>
    virtual int32_t SetGesture(uint32_t gesture) { return 0; }

    /// <summary>Sets the gesture the actor animates toward.</summary>
    virtual int32_t SetGestureGoal(int32_t gestureGoal) { return 0; }

    /// <summary>Scales the walk/move animation speed.</summary>
    virtual void SetVelocityPercentage(float percent) {}

    /// <summary>The current animation frame.</summary>
    virtual int32_t GetFrameNumber() { return 0; }

    /// <summary>The appearance's class (the top byte of its type id); 0 for the base.</summary>
    virtual uint32_t GetAppearanceClass() { return 0; }

    /// <summary>
    /// Where the owner is on screen through <paramref name="cam"/>, or the cached <see cref="ScreenPos"/> when
    /// <paramref name="cam"/> is null.
    /// </summary>
    MCVector2D GetScreenPos(MCCamera* cam);

    /// <summary>Draws a box around the appearance's bounds in <paramref name="color"/>.</summary>
    void DrawSelectBox(uint8_t color);

    /// <summary>Draws corner brackets around the appearance's bounds in <paramref name="color"/>.</summary>
    void DrawSelectBrackets(uint8_t color);

    /// <summary>Nonzero when the object is to be drawn this frame (set by the object's update).</summary>
    int32_t Visible = 0;
    /// <summary>Set by <c>recalcBounds</c> once the current shape's bounds are known.</summary>
    int32_t InView = 0;
    /// <summary>The object the appearance draws.</summary>
    MCGameObject* Owner = nullptr;
    /// <summary>The owner's position on screen (the camera's last projection).</summary>
    MCVector2D ScreenPos;
    /// <summary>Seconds left of the highlight box drawn when the object first becomes a target.</summary>
    float HighlightTime = 0.0f;
    /// <summary>Nonzero while the highlight box is showing.</summary>
    int32_t Highlighting = 0;
    /// <summary>Nonzero once the highlight has been shown (it shows only once).</summary>
    int32_t Highlighted = 0;
    /// <summary>The screen bounds' top-left corner.</summary>
    MCVector2D UpperLeft;
    /// <summary>The screen bounds' bottom-right corner.</summary>
    MCVector2D LowerRight;
};
