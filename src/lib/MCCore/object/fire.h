#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class Appearance;
class File;
class GameObject;

/// <summary>
/// The type of a <see cref="Fire"/>: its damage, sound and light, how its animation loops, and the flame shapes that
/// make it up (each with its own offset and start delay, plus random spreads).
/// </summary>
/// <remarks>Original source: <c>object\fire.cpp</c>, <c>object\fire.h</c>; 0x6c bytes. Read from the "FireData" block.</remarks>
class FireType : public ObjectType
{
public:
    FireType() { init(); }
    /// <remarks>MCX.EXE @ 0x006907d0 (vector deleting destructor)</remarks>
    ~FireType() override { destroy(); }

    /// <summary>Resets the common type data and this type's fields (one shape, no arrays).</summary>
    /// <remarks>MCX.EXE @ 0x00690780 (inline in <c>object\fire.h</c>)</remarks>
    void init();
    /// <summary>Makes a <see cref="Fire"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x006604b0</remarks>
    BaseObject* createInstance() override;
    /// <summary>Frees the six per-shape arrays.</summary>
    /// <remarks>MCX.EXE @ 0x00660640</remarks>
    void destroy() override;
    /// <summary>
    /// Reads the "FireData" block, allocates the per-shape arrays (from systemHeap) and reads each shape's
    /// FireOffsetX/Y, FireDelay and their random spreads, then the common type data.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006606c0</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <summary>
    /// A fire touching a building, tree, misc terrain object or tree building sets it alight one time in ten (host
    /// only in multiplayer, which then sends a light-on-fire chunk).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00660ad0</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <remarks>MCX.EXE @ 0x00660c40</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>Damage level (FIT "DmgLevel").</summary>
    uint32_t dmgLevel; // +0x30
    /// <summary>FIT "SoundEffectId"; 0xFFFFFFFF for none.</summary>
    uint32_t soundEffectId; // +0x34
    /// <summary>Object type of the fire's light (FIT "LightObjectId"); -1 for none.</summary>
    uint32_t lightObjectId; // +0x38
    /// <summary>First frame of the looped part of the animation (FIT "startLoopFrame").</summary>
    uint32_t startLoopFrame; // +0x3c
    /// <summary>Last frame of the looped part (FIT "endLoopFrame").</summary>
    uint32_t endLoopFrame; // +0x40
    /// <summary>How many times each shape loops (FIT "numLoops"); the start of each shape's loop count.</summary>
    uint32_t numLoops; // +0x44
    /// <summary>The fire's extent radius once it burns out and turns on collision (FIT "maxExtentRadius", default 0).</summary>
    float maxExtentRadius; // +0x48
    /// <summary>FIT "TimeToMaxExtent" (default 0); not used by fire.cpp.</summary>
    float timeToMaxExtent; // +0x4c
    /// <summary>How many flame shapes make up the fire (FIT "TotalFireShapes", default 1): the arrays' length.</summary>
    int32_t totalFireShapes; // +0x50
    /// <summary>Per shape: X offset from the fire's position (FIT "FireOffsetX%d").</summary>
    float* fireOffsetX; // +0x54
    /// <summary>Per shape: Y offset (FIT "FireOffsetY%d").</summary>
    float* fireOffsetY; // +0x58
    /// <summary>Per shape: seconds before it starts (FIT "FireDelay%d").</summary>
    float* fireDelay; // +0x5c
    /// <summary>Per shape: random spread of the X offset (FIT "FireRandomOffsetX%d").</summary>
    int32_t* fireRandomOffsetX; // +0x60
    /// <summary>Per shape: random spread of the Y offset (FIT "FireRandomOffsetY%d").</summary>
    int32_t* fireRandomOffsetY; // +0x64
    /// <summary>Per shape: random extra delay (FIT "FireRandomDelay%d").</summary>
    int32_t* fireRandomDelay; // +0x68
};

/// <summary>
/// A fire: several flame shapes (VFX appearances) that start after their delays, loop while they have burn time left,
/// then burn out. When burnt out it turns on collision to set neighbours alight, and damages or releases the object
/// that was burning. At most maxFiresBurning fires exist; starting another finishes the oldest.
/// </summary>
/// <remarks>Original source: <c>object\fire.cpp</c>, <c>object\fire.h</c>; 0xb4 bytes.</remarks>
class Fire : public BigGameObject
{
public:
    Fire() { init(); }
    /// <remarks>MCX.EXE @ 0x006605f0 (vector deleting destructor)</remarks>
    ~Fire() override { destroy(); }

    /// <summary>Zeroes every field.</summary>
    /// <remarks>MCX.EXE @ 0x00660560 (inline in <c>object\fire.h</c>)</remarks>
    void init() override;
    /// <summary>
    /// Allocates the per-shape arrays, makes each shape's VFX appearance with its randomised offset, delay and burn
    /// time, takes a slot in maxFiresList (finishing the fire that had it) and creates the light.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00661880</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Destroys the appearances and light and frees the per-shape arrays.</summary>
    /// <remarks>MCX.EXE @ 0x006617a0</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x006605b0</remarks>
    int32_t kill() override { return 0; }
    /// <summary>
    /// On the first update moves the fire to the end of the object list; counts down the shapes' delays and burn
    /// times; once burnt out and every shape finished, damages or releases the burning object and leaves maxFiresList.
    /// </summary>
    /// <returns>1 while burning, 0 when the fire is done.</returns>
    /// <remarks>MCX.EXE @ 0x00661120</remarks>
    int32_t update() override;
    /// <summary>
    /// Draws the visible shapes, stepping their loops; a fire the player can't see but has a contact on is drawn as
    /// a sensor blip (sized by getTonnage) instead.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006613a0</remarks>
    void render() override;
    /// <summary>Checks the static objects of the 3x3 terrain blocks around the fire for collisions.</summary>
    /// <remarks>MCX.EXE @ 0x00660c50</remarks>
    void handleStaticCollision() override;
    /// <remarks>MCX.EXE @ 0x006605d0</remarks>
    float getExtentRadius() override { return extentRadius; }
    /// <remarks>MCX.EXE @ 0x006605e0</remarks>
    void setExtentRadius(float newRadius) override { extentRadius = newRadius; }
    /// <summary>Whether any of the four map cells at the fire is visible to the home team.</summary>
    /// <remarks>MCX.EXE @ 0x00661050</remarks>
    int isRevealed() override;

    /// <summary>The first shape's appearance.</summary>
    /// <remarks>MCX.EXE @ 0x006605c0</remarks>
    virtual Appearance* getAppearancePtr() { return appearances[0]; }

    /// <summary>
    /// Projects shape <paramref name="shapeIndex"/> to the screen; true when it is visible to the main camera (always
    /// true in multiplayer).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00660e90</remarks>
    int isVisible(int32_t shapeIndex);
    /// <summary>Burns the fire out at once: every shape to its end state, no delays or burn time left.</summary>
    /// <remarks>MCX.EXE @ 0x00660fb0</remarks>
    void finishFireNow();
    /// <summary>Adds <paramref name="extraTime"/> to each shape's burn time, unless that would pass maxFireBurnTime.</summary>
    /// <remarks>MCX.EXE @ 0x00661010</remarks>
    void addTimeLeftToBurn(float extraTime);

    /// <summary>Per shape: its VFX appearance (totalFireShapes entries, from systemHeap).</summary>
    Appearance** appearances; // +0x84
    /// <summary>The appearance class (top byte of the appearance type's id); must be 2 (VFX).</summary>
    uint32_t appearanceClass; // +0x88
    /// <summary>Set by init; the first update clears it and moves the fire to the end of the object list.</summary>
    int32_t justCreated; // +0x8c
    /// <summary>
    /// Per shape: loops left (from the type's numLoops); 999 while burn time remains, 2 for the end sequence, 0 once
    /// finished.
    /// </summary>
    int32_t* loopsLeft; // +0x90
    /// <summary>Per shape: seconds of burning left (starts at maxFireBurnTime).</summary>
    float* timeLeftToBurn; // +0x94
    /// <summary>The fire's extent radius (0 until it burns out, then the type's maxExtentRadius).</summary>
    float extentRadius; // +0x98
    /// <summary>Set once the fire is burning out: collision is on and it ends when every shape is finished.</summary>
    int32_t burningOut; // +0x9c
    /// <summary>
    /// The object that is burning (set by the objects' lightOnFire, not in fire.cpp); when the fire ends it is
    /// damaged (a class 0x18 object in state 6) and released (its vtable slot 25, killFireObject).
    /// </summary>
    GameObject* burningObject; // +0xa0
    /// <summary>Per shape: its offset from the fire's position (from systemHeap).</summary>
    vector_3d* shapeOffsets; // +0xa4
    /// <summary>Per shape: seconds before it starts.</summary>
    float* startDelays; // +0xa8
    /// <summary>The fire's light, kept at its position.</summary>
    GameObject* light; // +0xac
    /// <summary>The turn a shape was last visible on screen; shapes are drawn only on that turn.</summary>
    int32_t lastVisibleTurn; // +0xb0

    /// <summary>The fires burning, maxFiresBurning entries (from systemHeap, made by the first fire).</summary>
    static Fire** maxFiresList;
};

/// <summary>The longest a fire's shape can burn, in seconds.</summary>
extern float maxFireBurnTime;
/// <summary>The most fires that can burn at once: the length of Fire::maxFiresList.</summary>
extern int32_t maxFiresBurning;
/// <summary>The slot of Fire::maxFiresList the newest fire took.</summary>
extern int32_t currentFireIndex;
