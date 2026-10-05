#pragma once

#include "lib/cvmath.h"

class AppearanceType;
class Camera;
class GameObject;

/// <summary>
/// What an object looks like on screen: the base of every actor (mechs, vehicles, buildings, effects...). The
/// object owns one; each frame the object queue calls <see cref="update"/>, <see cref="recalcBounds"/> against the
/// camera and <see cref="render"/>, which adds draw elements to the global <c>ElementList</c>.
/// </summary>
/// <remarks>
/// Original source: <c>appear\appear.h</c> (inline virtuals) and <c>appear\appear.cpp</c>, 0x38 bytes. The constructor was inlined at every <c>new</c> site: it sets
/// <see cref="unknown04"/> to 0x70000000 and clears the rest.
/// </remarks>
class Appearance
{
public:
    /// <summary>The inlined constructor of every allocation site.</summary>
    Appearance() = default;
    /// <summary>Calls <see cref="destroy"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006512a0 (vector deleting destructor); slot 2</remarks>
    virtual ~Appearance() { init(nullptr, nullptr); }

    /// <summary>Binds the appearance to its type and its object.</summary>
    /// <remarks>MCX.EXE @ 0x00651220; slot 0</remarks>
    virtual int32_t init(AppearanceType* tree = nullptr, GameObject* obj = nullptr)
    {
        unknown04 = 0x70000000;
        owner = obj;
        visible = 0;
        return 0;
    }

    /// <summary>Resets the appearance (the base calls <see cref="init"/> with nothing).</summary>
    /// <remarks>MCX.EXE @ 0x00651240; slot 1</remarks>
    virtual void destroy() { init(nullptr, nullptr); }

    /// <summary>Advances animation. Returns nonzero when it wants to be drawn.</summary>
    /// <remarks>MCX.EXE @ 0x00651250; slot 3</remarks>
    virtual int32_t update() { return 0; }

    /// <summary>Adds the appearance's elements to the element list at <paramref name="depthFixup"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00651260; slot 4</remarks>
    virtual int32_t render(int32_t depthFixup = 0) { return 0; }

    /// <summary>The appearance's type (none for the base).</summary>
    /// <remarks>MCX.EXE @ 0x00651270; slot 5</remarks>
    virtual AppearanceType* getAppearanceType() { return nullptr; }

    /// <summary>Draws the damage/status bars over the object (nothing for the base).</summary>
    /// <remarks>MCX.EXE @ 0x006ab940; slot 6</remarks>
    virtual void drawBars();

    /// <summary>
    /// Recomputes the screen position and bounds against <paramref name="cam"/>. Returns nonzero when on screen.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00651280; slot 7</remarks>
    virtual int recalcBounds(Camera* cam)
    {
        upperLeft.x = 0.0f;
        upperLeft.y = 0.0f;
        lowerRight.x = 0.0f;
        lowerRight.y = 0.0f;
        return 0;
    }

    /// <summary>Starts gesture <paramref name="gesture"/> (actors that animate by gesture).</summary>
    /// <remarks>MCX.EXE @ 0x00651160; slot 8</remarks>
    virtual int32_t setGesture(uint32_t gesture) { return 0; }

    /// <summary>Sets the gesture the actor animates toward.</summary>
    /// <remarks>MCX.EXE @ 0x00651170; slot 9</remarks>
    virtual int32_t setGestureGoal(int32_t gestureGoal) { return 0; }

    /// <summary>Scales the walk/move animation speed.</summary>
    /// <remarks>MCX.EXE @ 0x00651180; slot 10</remarks>
    virtual void setVelocityPercentage(float percent) {}

    /// <summary>The current animation frame.</summary>
    /// <remarks>MCX.EXE @ 0x00651190; slot 11</remarks>
    virtual int32_t getFrameNumber() { return 0; }

    /// <summary>The appearance's class (the top byte of its type id); 0 for the base.</summary>
    /// <remarks>MCX.EXE @ 0x006511a0; slot 12</remarks>
    virtual uint32_t getAppearanceClass() { return 0; }

    /// <summary>
    /// Where the owner is on screen through <paramref name="cam"/>, or the cached <see cref="screenPos"/> when
    /// <paramref name="cam"/> is null.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006ab870</remarks>
    vector_2d getScreenPos(Camera* cam);

    /// <summary>Draws a box around the appearance's bounds in <paramref name="color"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006ab950</remarks>
    void drawSelectBox(uint8_t color);

    /// <summary>Draws corner brackets around the appearance's bounds in <paramref name="color"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006abd10</remarks>
    void drawSelectBrackets(uint8_t color);

    /// <summary>
    /// Set to 0x70000000 by the constructor and <see cref="init"/>; never read in MCX.EXE.
    /// </summary>
    int32_t unknown04 = 0x70000000; // +0x04
    /// <summary>Nonzero when the object is to be drawn this frame (set by the object's update).</summary>
    int32_t visible = 0; // +0x08
    /// <summary>Set by <c>recalcBounds</c> once the current shape's bounds are known.</summary>
    int32_t inView = 0; // +0x0c
    /// <summary>The object the appearance draws.</summary>
    GameObject* owner = nullptr; // +0x10
    /// <summary>The owner's position on screen (the camera's last projection).</summary>
    vector_2d screenPos; // +0x14
    /// <summary>Seconds left of the highlight box drawn when the object first becomes a target.</summary>
    float highlightTime = 0.0f; // +0x1c
    /// <summary>Nonzero while the highlight box is showing.</summary>
    int32_t highlighting = 0; // +0x20
    /// <summary>Nonzero once the highlight has been shown (it shows only once).</summary>
    int32_t highlighted = 0; // +0x24
    /// <summary>The screen bounds' top-left corner.</summary>
    vector_2d upperLeft; // +0x28
    /// <summary>The screen bounds' bottom-right corner.</summary>
    vector_2d lowerRight; // +0x30
};
