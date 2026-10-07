#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class MCAppearance;
class MCBattleMech;
class MCFile;
class MCGameObject;
class MCSmoke;

/// <summary>The type of a <see cref="MCJet"/>: its sound and the objects it trails.</summary>
/// <remarks>
/// Original source: <c>object\jet.cpp</c>, 0x3c bytes. Its vtable (0x0077f414) directly follows ArtilleryType's.
/// </remarks>
class MCJetType : public MCObjectType
{
public:
    /// <summary>Sets the three object ids to -1.</summary>
    /// <remarks>Inline in ObjectTypeManager::load (case 0x10).</remarks>
    MCJetType();
    ~MCJetType() override { Destroy(); }

    /// <summary>Makes a <see cref="MCJet"/> of this type and gives it the next object id.</summary>
    MCBaseObject* CreateInstance() override;
    void Destroy() override;
    /// <summary>Reads SoundEffectId and SmokeObjectId from the "JetData" block (if present), then the common data.</summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>Sample played when the jet starts (FIT "SoundEffectId"); 0xFFFFFFFF for none.</summary>
    uint32_t SoundEffectId = 0;
    /// <summary>Object type of the <see cref="MCSmoke"/> trail the jet creates (FIT "SmokeObjectId"); -1 for none.</summary>
    uint32_t SmokeObjectId = 0;
    /// <summary>
    /// Object type of a second object the jet creates and keeps on the terrain under itself; -1 for none. Never read
    /// from the FIT, so always -1 in practice.
    /// </summary>
    uint32_t GroundObjectId = 0;
};

/// <summary>
/// A mech's jump jet: an arm appearance that follows its owner's jump position, with a smoke trail and an optional
/// object kept on the ground below it.
/// </summary>
/// <remarks>Original source: <c>object\jet.cpp</c>, <c>object\jet.h</c>; 0xa8 bytes.</remarks>
class MCJet : public MCBigGameObject
{
public:
    /// <summary>Zeroes the pointers and drawRotation and sets justCreated (inline in JetType::createInstance).</summary>
    MCJet();
    ~MCJet() override { Destroy(); }

    /// <summary>Empty in the original; the constructor's inline field setup precedes it.</summary>
    void Init() override;
    /// <summary>Makes the arm appearance and the smoke and ground objects of the type.</summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Destroys the appearance, the smoke and the ground object.</summary>
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>
    /// Follows the owner's jump position, stops smoking once the owner comes down, and turns the flame toward the
    /// owner's heading.
    /// </summary>
    int32_t Update() override;
    void Render() override;

    virtual MCAppearance* GetAppearancePtr() { return nullptr; }

    /// <summary>Projects the jet to the screen; true when its appearance is visible to the main camera.</summary>
    int IsVisible();
    /// <summary>The mech whose jump the jet follows.</summary>
    void SetOwner(MCBaseObject* newOwner);

    /// <summary>Set by the constructor and init; the first update clears it, plays the sound and sets the position.</summary>
    int32_t JustCreated = 0;
    /// <summary>The jumping mech (<see cref="SetOwner"/> stores a BaseObject; every use is as a BattleMech).</summary>
    MCBattleMech* Owner = nullptr;
    /// <summary>The jump jet passed to BattleMech::getJumpPosition; zeroed by the constructor and never set.</summary>
    int32_t JetNumber = 0;
    /// <summary>The jet flame (an ArmAppearance).</summary>
    MCAppearance* Appearance = nullptr;
    /// <summary>The smoke trail.</summary>
    MCSmoke* Smoke = nullptr;
    /// <summary>The altitude at the previous update, to tell when the owner starts coming down.</summary>
    float LastAltitude = 0;
    /// <summary>Set once the owner is landing and the jet is falling: the flame is no longer drawn and the smoke stops.</summary>
    int32_t Landing = 0;
    /// <summary>The object of the type's groundObjectId, kept at terrain elevation under the jet.</summary>
    MCGameObject* GroundObject = nullptr;
    /// <summary>The draw rotation of the flame and smoke: 150 or -150 by the owner's heading.</summary>
    int32_t DrawRotation = 0;
};
