#pragma once

#include "object/MCBigGameObject.h"

class MCArmAppearance;
class MCBattleMech;
class MCSmoke;

/// <summary>
/// A mech's jump jet: an arm appearance that follows its owner's jump position, with a smoke trail and an optional
/// object kept on the ground below it.
/// </summary>
/// <remarks>Original source: <c>object\jet.cpp</c>, <c>object\jet.h</c>.</remarks>
class MCJet : public MCBigGameObject
{
public:
    MCJet();
    ~MCJet() override;

    /// <summary>Makes the arm appearance and the smoke and ground objects of the type.</summary>
    int32_t Init(MCObjectType* objType) override;
    using MCBigGameObject::Init;
    int32_t Kill() override { return 0; }
    /// <summary>
    /// Follows the owner's jump position, stops smoking once the owner comes down, and turns the flame toward the
    /// owner's heading.
    /// </summary>
    int32_t Update() override;
    void Render() override;

    /// <summary>Projects the jet to the screen; true when its appearance is visible to the main camera.</summary>
    bool IsVisible();
    /// <summary>The mech whose jump the jet follows.</summary>
    void SetOwner(MCBaseObject* newOwner);

    /// <summary>Set until the first update, which plays the sound and sets the position.</summary>
    bool JustCreated = true;
    /// <summary>The jumping mech.</summary>
    MCBattleMech* Owner = nullptr;
    /// <summary>The jump jet passed to BattleMech::getJumpPosition; nothing sets it.</summary>
    int32_t JetNumber = 0;
    /// <summary>The jet flame.</summary>
    std::unique_ptr<MCArmAppearance> Appearance;
    /// <summary>The smoke trail.</summary>
    std::unique_ptr<MCSmoke> Smoke;
    /// <summary>The altitude at the previous update, to tell when the owner starts coming down.</summary>
    float LastAltitude = 0;
    /// <summary>Set once the owner is landing and the jet is falling: the flame is no longer drawn and the smoke stops.</summary>
    bool Landing = false;
    /// <summary>The object of the type's groundObjectId, kept at terrain elevation under the jet.</summary>
    std::unique_ptr<MCGameObject> GroundObject;
    /// <summary>The draw rotation of the flame and smoke: 150 or -150 by the owner's heading.</summary>
    int32_t DrawRotation = 0;
};
