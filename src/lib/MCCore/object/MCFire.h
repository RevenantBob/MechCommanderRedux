#pragma once

#include "object/MCBigGameObject.h"

class MCVfxAppearance;

/// <summary>One flame of a <see cref="MCFire"/>: its VFX appearance, where it stands and how long it burns.</summary>
struct MCFireFlame
{
    MCFireFlame();
    ~MCFireFlame();
    MCFireFlame(MCFireFlame&&) noexcept;
    MCFireFlame& operator=(MCFireFlame&&) noexcept;

    /// <summary>The flame's VFX appearance.</summary>
    std::unique_ptr<MCVfxAppearance> Appearance;
    /// <summary>Its offset from the fire's position.</summary>
    MCVector3D Offset;
    /// <summary>Seconds before it starts.</summary>
    float StartDelay = 0;
    /// <summary>
    /// Loops left (from the type's numLoops); 999 while burn time remains, 2 for the end sequence, 0 once finished.
    /// </summary>
    int32_t LoopsLeft = 0;
    /// <summary>Seconds of burning left (starts at the mission's MaxFireBurnTime).</summary>
    float TimeLeftToBurn = 0;
};

/// <summary>
/// A fire: several flames (VFX appearances) that start after their delays, loop while they have burn time left,
/// then burn out. When burnt out it turns on collision to set its neighbours alight, and damages (a forest) and lets go
/// of the object that was burning. The mission's fires share a ring of slots (<see cref="MCEffectSystem"/>): starting
/// one finishes the oldest.
/// </summary>
/// <remarks>Original source: <c>object\fire.cpp</c>, <c>object\fire.h</c>.</remarks>
class MCFire : public MCBigGameObject
{
public:
    MCFire();
    /// <summary>Leaves the ring of fires burning and the contacts, and lets go of the object that was burning.</summary>
    ~MCFire() override;

    /// <summary>
    /// Makes each flame's VFX appearance with its randomised offset, delay and burn time, takes a slot in the ring of
    /// fires (finishing the fire that had it) and creates the light.
    /// </summary>
    int32_t Init(MCObjectType* objType) override;
    using MCBigGameObject::Init;
    int32_t Kill() override { return 0; }
    /// <summary>
    /// Counts down the flames' delays and burn times; once burnt out and every flame finished, damages or lets go of
    /// the burning object and leaves the ring.
    /// </summary>
    /// <returns>1 while burning, 0 when the fire is done.</returns>
    int32_t Update() override;
    /// <summary>
    /// Draws the visible flames, stepping their loops; a fire the player can't see but has a contact on is drawn as
    /// a sensor blip (sized by its tonnage) instead.
    /// </summary>
    void Render() override;
    /// <summary>Checks the static objects of the terrain blocks around the fire for collisions (OB-151).</summary>
    void HandleStaticCollision() override;
    float GetExtentRadius() override { return ExtentRadius; }
    void SetExtentRadius(float newRadius) override { ExtentRadius = newRadius; }
    /// <summary>Whether any of the four map cells at the fire is visible to the home team.</summary>
    int IsRevealed() override;

    /// <summary>
    /// Projects flame <paramref name="flameIndex"/> to the screen; true when it is visible to the main camera (always
    /// true in multiplayer).
    /// </summary>
    bool IsVisible(size_t flameIndex);
    /// <summary>Burns the fire out at once: every flame to its end state, no delays or burn time left.</summary>
    void FinishFireNow();
    /// <summary>Adds <paramref name="extraTime"/> to each flame's burn time, unless that would pass MaxFireBurnTime.</summary>
    void AddTimeLeftToBurn(float extraTime);

    /// <summary>The flames (the type's shapes).</summary>
    std::vector<MCFireFlame> Flames;
    /// <summary>The appearance class (top byte of the appearance type's id); must be 2 (VFX).</summary>
    uint32_t AppearanceClass = 0;
    /// <summary>Set until the first update, which makes the fire a sensor contact.</summary>
    bool JustCreated = false;
    /// <summary>The fire's extent radius (0 until it burns out, then the type's maxExtentRadius).</summary>
    float ExtentRadius = 0;
    /// <summary>Set once the fire is burning out: collision is on and it ends when every flame is finished.</summary>
    bool BurningOut = false;
    /// <summary>
    /// The object that is burning (set by the objects that light it); when the fire ends it is damaged (a forest) and
    /// told (<c>KillFireObject</c>).
    /// </summary>
    MCGameObject* BurningObject = nullptr;
    /// <summary>The fire's light, kept at its position.</summary>
    std::unique_ptr<MCGameObject> Light;
    /// <summary>The turn a flame was last visible on screen; flames are drawn only on that turn.</summary>
    int32_t LastVisibleTurn = 0;
};

/// <summary>
/// The fire burning on an object. The object owns a new fire until it sends it to the object lists to burn
/// (<see cref="Burn"/>); from then on the lists own it and the object only points at it, until the fire ends and
/// tells the object (its <c>KillFireObject</c> calls <see cref="BurntOut"/>).
/// </summary>
/// <remarks>
/// Port: MCX.EXE's objects kept a bare pointer, and a fire put itself on the first object list on its first update.
/// A fire that is lit but never updated (an object set alight by a spreading fire; OB-154) stays with its object, and
/// goes with it.
/// </remarks>
class MCFireLink
{
public:
    MCFireLink() = default;
    /// <summary>Deletes a fire it owns; a listed fire no longer points back at the object.</summary>
    ~MCFireLink();
    MCFireLink(const MCFireLink&) = delete;
    MCFireLink& operator=(const MCFireLink&) = delete;

    /// <summary>The fire, or null.</summary>
    MCFire* Get() const { return _Fire; }
    MCFire* operator->() const { return _Fire; }
    bool operator==(std::nullptr_t) const { return _Fire == nullptr; }

    /// <summary>Takes a new fire (the object then sets it up, in the order its code always did).</summary>
    /// <returns>The fire.</returns>
    MCFire* Light(std::unique_ptr<MCFire> fire);
    /// <summary>Hands the fire (if it still owns it) to the first object list and runs its first update.</summary>
    void Burn();
    /// <summary>The fire ended: forgets it.</summary>
    void BurntOut();

private:
    /// <summary>The fire while the object owns it.</summary>
    std::unique_ptr<MCFire> _Owned;
    /// <summary>The fire, owned here or by the object lists.</summary>
    MCFire* _Fire = nullptr;
};
