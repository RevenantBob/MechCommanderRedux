#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class Appearance;
class BattleMech;
class File;
class GameObject;
class Smoke;

/// <summary>The type of a <see cref="Jet"/>: its sound and the objects it trails.</summary>
/// <remarks>
/// Original source: <c>object\jet.cpp</c>, 0x3c bytes. Its vtable (0x0077f414) directly follows ArtilleryType's.
/// </remarks>
class JetType : public ObjectType
{
public:
    /// <summary>Sets the three object ids to -1.</summary>
    /// <remarks>Inline in ObjectTypeManager::load (case 0x10).</remarks>
    JetType();
    /// <remarks>MCX.EXE @ 0x00690960 (vector deleting destructor)</remarks>
    ~JetType() override { destroy(); }

    /// <summary>Makes a <see cref="Jet"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x00672860</remarks>
    BaseObject* createInstance() override;
    /// <remarks>MCX.EXE @ 0x00672980</remarks>
    void destroy() override;
    /// <summary>Reads SoundEffectId and SmokeObjectId from the "JetData" block (if present), then the common data.</summary>
    /// <remarks>MCX.EXE @ 0x00672990</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <remarks>MCX.EXE @ 0x00672a50</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <remarks>MCX.EXE @ 0x00672a60</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>Sample played when the jet starts (FIT "SoundEffectId"); 0xFFFFFFFF for none.</summary>
    uint32_t soundEffectId = 0; // +0x30
    /// <summary>Object type of the <see cref="Smoke"/> trail the jet creates (FIT "SmokeObjectId"); -1 for none.</summary>
    uint32_t smokeObjectId = 0; // +0x34
    /// <summary>
    /// Object type of a second object the jet creates and keeps on the terrain under itself; -1 for none. Never read
    /// from the FIT, so always -1 in practice.
    /// </summary>
    uint32_t groundObjectId = 0; // +0x38
};

/// <summary>
/// A mech's jump jet: an arm appearance that follows its owner's jump position, with a smoke trail and an optional
/// object kept on the ground below it.
/// </summary>
/// <remarks>Original source: <c>object\jet.cpp</c>, <c>object\jet.h</c>; 0xa8 bytes.</remarks>
class Jet : public BigGameObject
{
public:
    /// <summary>Zeroes the pointers and drawRotation and sets justCreated (inline in JetType::createInstance).</summary>
    Jet();
    /// <remarks>MCX.EXE @ 0x00672930 (vector deleting destructor)</remarks>
    ~Jet() override { destroy(); }

    /// <summary>Empty in the original; the constructor's inline field setup precedes it.</summary>
    /// <remarks>MCX.EXE @ 0x00672900 (inline in <c>object\jet.h</c>)</remarks>
    void init() override;
    /// <summary>Makes the arm appearance and the smoke and ground objects of the type.</summary>
    /// <remarks>MCX.EXE @ 0x00672e30</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Destroys the appearance, the smoke and the ground object.</summary>
    /// <remarks>MCX.EXE @ 0x00672de0</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x00672910</remarks>
    int32_t kill() override { return 0; }
    /// <summary>
    /// Follows the owner's jump position, stops smoking once the owner comes down, and turns the flame toward the
    /// owner's heading.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00672b20</remarks>
    int32_t update() override;
    /// <remarks>MCX.EXE @ 0x00672d80</remarks>
    void render() override;

    /// <remarks>MCX.EXE @ 0x00672920</remarks>
    virtual Appearance* getAppearancePtr() { return nullptr; }

    /// <summary>Projects the jet to the screen; true when its appearance is visible to the main camera.</summary>
    /// <remarks>MCX.EXE @ 0x00672a70</remarks>
    int isVisible();
    /// <summary>The mech whose jump the jet follows.</summary>
    /// <remarks>MCX.EXE @ 0x00672f60</remarks>
    void setOwner(BaseObject* newOwner);

    /// <summary>Set by the constructor and init; the first update clears it, plays the sound and sets the position.</summary>
    int32_t justCreated = 0; // +0x84
    /// <summary>The jumping mech (<see cref="setOwner"/> stores a BaseObject; every use is as a BattleMech).</summary>
    BattleMech* owner = nullptr; // +0x88
    /// <summary>The jump jet passed to BattleMech::getJumpPosition; zeroed by the constructor and never set.</summary>
    int32_t jetNumber = 0; // +0x8c
    /// <summary>The jet flame (an ArmAppearance).</summary>
    Appearance* appearance = nullptr; // +0x90
    /// <summary>The smoke trail.</summary>
    Smoke* smoke = nullptr; // +0x94
    /// <summary>The altitude at the previous update, to tell when the owner starts coming down.</summary>
    float lastAltitude = 0; // +0x98
    /// <summary>Set once the owner is landing and the jet is falling: the flame is no longer drawn and the smoke stops.</summary>
    int32_t landing = 0; // +0x9c
    /// <summary>The object of the type's groundObjectId, kept at terrain elevation under the jet.</summary>
    GameObject* groundObject = nullptr; // +0xa0
    /// <summary>The draw rotation of the flame and smoke: 150 or -150 by the owner's heading.</summary>
    int32_t drawRotation = 0; // +0xa4
};
