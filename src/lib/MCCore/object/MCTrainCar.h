#pragma once

#include "object/MCBigGameObject.h"

class MCGVAppearance;
class MCTrain;

/// <summary>
/// One car of a <see cref="MCTrain"/>. The train moves its cars along the track; a car hit hard enough, or running
/// onto a mine or off the rails, derails and leaves the train, splitting it.
/// </summary>
/// <remarks>Original source: <c>object\train.cpp</c>, <c>object\train.h</c>.</remarks>
class MCTrainCar : public MCBigGameObject
{
public:
    MCTrainCar();
    ~MCTrainCar() override;

    using MCBigGameObject::SetPartId;

    /// <summary>Copies the type's name, hit points and tonnage, and makes the car's GV appearance.</summary>
    int32_t Init(MCObjectType* objType) override;
    using MCBigGameObject::Init;
    int32_t Kill() override { return 0; }
    /// <summary>Derails a car that has taken enough damage, wrecks it on a broken bridge, and advances the appearance.</summary>
    int32_t Update() override;
    /// <summary>Draws the car (or its blip as a sensor contact).</summary>
    void Render() override;
    MCAppearance* GetAppearance() override;
    /// <summary>Checks the car against the terrain objects of its terrain block's vertex (OB-022).</summary>
    void HandleStaticCollision() override;
    int OnScreen() override;
    /// <summary>Takes the damage; at no hit points left the car derails and leaves its train.</summary>
    int32_t HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <summary>A position at <paramref name="angle"/> and <paramref name="distance"/> relative to the car.</summary>
    MCVector3D RelativePosition(float angle, float distance, uint32_t flags) override;
    MCFrameOfRef GetFrame() override { return Frame; }
    void SetFrame(MCFrameOfRef& newFrame) override { Frame = newFrame; }
    /// <summary>The angle from the car's facing to <paramref name="goal"/>.</summary>
    float RelFacingTo(MCVector3D goal, int32_t bodyLocation) override;
    /// <summary>Whether the car's tile is seen by the home team.</summary>
    int IsRevealed() override;

    /// <summary>Sets the part id from the car's train number and its place in the train.</summary>
    void SetPartId(int32_t trainNumber, int32_t carNumber);
    /// <summary>Knocks the car off the rails, turning it by <paramref name="angle"/>; it may fall in or be wrecked.</summary>
    void Derail(float angle);
    /// <summary>Sets off a mine on the car's cell, damaging the car.</summary>
    void MineCheck();
    float GetMaxAccel();
    float GetMaxDecel();
    float GetMaxSpeed();

    /// <summary>The frame turned an eighth of a turn about its up axis (the facing the art is drawn at).</summary>
    static MCFrameOfRef TurnedFrame(const MCFrameOfRef& frame);

    /// <summary>The car's name, loaded from the type's string resource.</summary>
    std::string Name;
    /// <summary>The car's GV appearance.</summary>
    std::unique_ptr<MCGVAppearance> Appearance;
    /// <summary>The car's orientation.</summary>
    MCFrameOfRef Frame;
    /// <summary>The car's speed, copied from its train every update.</summary>
    float Speed = 0;
    /// <summary>Set once the car is off the rails; the train then stops moving it.</summary>
    bool Derailed = false;
    /// <summary>
    /// Set when the car is wrecked for good (fallen through a broken bridge in update, or derailed into water): no
    /// update or draw.
    /// </summary>
    bool Wrecked = false;
    /// <summary>Whether the car's cell is inside the map.</summary>
    bool OnMap = true;
    /// <summary>Set until the first update.</summary>
    bool JustCreated = true;
    /// <summary>Damage taken so far; past half the type's hit points the car may derail.</summary>
    float DamageTaken = 0;
    /// <summary>The entry angle of the last hit; the car derails by it.</summary>
    float LastHitAngle = 0;
    /// <summary>The train the car belongs to.</summary>
    MCTrain* Train = nullptr;
};
