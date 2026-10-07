#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class MCAppearance;
class MCFile;
class MCGameObject;

/// <summary>
/// The type of a <see cref="MCFire"/>: its damage, sound and light, how its animation loops, and the flame shapes that
/// make it up (each with its own offset and start delay, plus random spreads).
/// </summary>
/// <remarks>Original source: <c>object\fire.cpp</c>, <c>object\fire.h</c>; 0x6c bytes. Read from the "FireData" block.</remarks>
class MCFireType : public MCObjectType
{
public:
    MCFireType() { Init(); }
    ~MCFireType() override { Destroy(); }

    /// <summary>Resets the common type data and this type's fields (one shape, no arrays).</summary>
    void Init();
    /// <summary>Makes a <see cref="MCFire"/> of this type and gives it the next object id.</summary>
    MCBaseObject* CreateInstance() override;
    /// <summary>Frees the six per-shape arrays.</summary>
    void Destroy() override;
    /// <summary>
    /// Reads the "FireData" block, allocates the per-shape arrays and reads each shape's
    /// FireOffsetX/Y, FireDelay and their random spreads, then the common type data.
    /// </summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>
    /// A fire touching a building, tree, misc terrain object or tree building sets it alight one time in ten (host
    /// only in multiplayer, which then sends a light-on-fire chunk).
    /// </summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>Damage level (FIT "DmgLevel").</summary>
    uint32_t DmgLevel = 0;
    /// <summary>FIT "SoundEffectId"; 0xFFFFFFFF for none.</summary>
    uint32_t SoundEffectId = 0;
    /// <summary>Object type of the fire's light (FIT "LightObjectId"); -1 for none.</summary>
    uint32_t LightObjectId = 0;
    /// <summary>First frame of the looped part of the animation (FIT "startLoopFrame").</summary>
    uint32_t StartLoopFrame = 0;
    /// <summary>Last frame of the looped part (FIT "endLoopFrame").</summary>
    uint32_t EndLoopFrame = 0;
    /// <summary>How many times each shape loops (FIT "numLoops"); the start of each shape's loop count.</summary>
    uint32_t NumLoops = 0;
    /// <summary>The fire's extent radius once it burns out and turns on collision (FIT "maxExtentRadius", default 0).</summary>
    float MaxExtentRadius = 0;
    /// <summary>FIT "TimeToMaxExtent" (default 0); not used by fire.cpp.</summary>
    float TimeToMaxExtent = 0;
    /// <summary>How many flame shapes make up the fire (FIT "TotalFireShapes", default 1): the arrays' length.</summary>
    int32_t TotalFireShapes = 0;
    /// <summary>Per shape: X offset from the fire's position (FIT "FireOffsetX%d").</summary>
    std::unique_ptr<float[]> FireOffsetX;
    /// <summary>Per shape: Y offset (FIT "FireOffsetY%d").</summary>
    std::unique_ptr<float[]> FireOffsetY;
    /// <summary>Per shape: seconds before it starts (FIT "FireDelay%d").</summary>
    std::unique_ptr<float[]> FireDelay;
    /// <summary>Per shape: random spread of the X offset (FIT "FireRandomOffsetX%d").</summary>
    std::unique_ptr<int32_t[]> FireRandomOffsetX;
    /// <summary>Per shape: random spread of the Y offset (FIT "FireRandomOffsetY%d").</summary>
    std::unique_ptr<int32_t[]> FireRandomOffsetY;
    /// <summary>Per shape: random extra delay (FIT "FireRandomDelay%d").</summary>
    std::unique_ptr<int32_t[]> FireRandomDelay;
};

/// <summary>
/// A fire: several flame shapes (VFX appearances) that start after their delays, loop while they have burn time left,
/// then burn out. When burnt out it turns on collision to set neighbours alight, and damages or releases the object
/// that was burning. At most maxFiresBurning fires exist; starting another finishes the oldest.
/// </summary>
/// <remarks>Original source: <c>object\fire.cpp</c>, <c>object\fire.h</c>; 0xb4 bytes.</remarks>
class MCFire : public MCBigGameObject
{
public:
    MCFire() { Init(); }
    ~MCFire() override { Destroy(); }

    /// <summary>Zeroes every field.</summary>
    void Init() override;
    /// <summary>
    /// Allocates the per-shape arrays, makes each shape's VFX appearance with its randomised offset, delay and burn
    /// time, takes a slot in maxFiresList (finishing the fire that had it) and creates the light.
    /// </summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Destroys the appearances and light and frees the per-shape arrays.</summary>
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>
    /// On the first update moves the fire to the end of the object list; counts down the shapes' delays and burn
    /// times; once burnt out and every shape finished, damages or releases the burning object and leaves maxFiresList.
    /// </summary>
    /// <returns>1 while burning, 0 when the fire is done.</returns>
    int32_t Update() override;
    /// <summary>
    /// Draws the visible shapes, stepping their loops; a fire the player can't see but has a contact on is drawn as
    /// a sensor blip (sized by getTonnage) instead.
    /// </summary>
    void Render() override;
    /// <summary>Checks the static objects of the 3x3 terrain blocks around the fire for collisions.</summary>
    void HandleStaticCollision() override;
    float GetExtentRadius() override { return ExtentRadius; }
    void SetExtentRadius(float newRadius) override { ExtentRadius = newRadius; }
    /// <summary>Whether any of the four map cells at the fire is visible to the home team.</summary>
    int IsRevealed() override;

    /// <summary>The first shape's appearance.</summary>
    virtual MCAppearance* GetAppearancePtr() { return Appearances[0]; }

    /// <summary>
    /// Projects shape <paramref name="shapeIndex"/> to the screen; true when it is visible to the main camera (always
    /// true in multiplayer).
    /// </summary>
    int IsVisible(int32_t shapeIndex);
    /// <summary>Burns the fire out at once: every shape to its end state, no delays or burn time left.</summary>
    void FinishFireNow();
    /// <summary>Adds <paramref name="extraTime"/> to each shape's burn time, unless that would pass maxFireBurnTime.</summary>
    void AddTimeLeftToBurn(float extraTime);

    /// <summary>Per shape: its VFX appearance (totalFireShapes entries).</summary>
    std::unique_ptr<MCAppearance*[]> Appearances;
    /// <summary>The appearance class (top byte of the appearance type's id); must be 2 (VFX).</summary>
    uint32_t AppearanceClass = 0;
    /// <summary>Set by init; the first update clears it and moves the fire to the end of the object list.</summary>
    int32_t JustCreated = 0;
    /// <summary>
    /// Per shape: loops left (from the type's numLoops); 999 while burn time remains, 2 for the end sequence, 0 once
    /// finished.
    /// </summary>
    std::unique_ptr<int32_t[]> LoopsLeft;
    /// <summary>Per shape: seconds of burning left (starts at maxFireBurnTime).</summary>
    std::unique_ptr<float[]> TimeLeftToBurn;
    /// <summary>The fire's extent radius (0 until it burns out, then the type's maxExtentRadius).</summary>
    float ExtentRadius = 0;
    /// <summary>Set once the fire is burning out: collision is on and it ends when every shape is finished.</summary>
    int32_t BurningOut = 0;
    /// <summary>
    /// The object that is burning (set by the objects' lightOnFire, not in fire.cpp); when the fire ends it is
    /// damaged (a class 0x18 object in state 6) and released (its vtable slot 25, killFireObject).
    /// </summary>
    MCGameObject* BurningObject = nullptr;
    /// <summary>Per shape: its offset from the fire's position.</summary>
    std::unique_ptr<MCVector3D[]> ShapeOffsets;
    /// <summary>Per shape: seconds before it starts.</summary>
    std::unique_ptr<float[]> StartDelays;
    /// <summary>The fire's light, kept at its position.</summary>
    MCGameObject* Light = nullptr;
    /// <summary>The turn a shape was last visible on screen; shapes are drawn only on that turn.</summary>
    int32_t LastVisibleTurn = 0;

    /// <summary>The fires burning, maxFiresBurning entries (made by the first fire).</summary>
    static std::unique_ptr<MCFire*[]> MaxFiresList;
};

/// <summary>The longest a fire's shape can burn, in seconds.</summary>
extern float MaxFireBurnTime;
/// <summary>The most fires that can burn at once: the length of Fire::maxFiresList.</summary>
extern int32_t MaxFiresBurning;
/// <summary>The slot of Fire::maxFiresList the newest fire took.</summary>
extern int32_t CurrentFireIndex;
