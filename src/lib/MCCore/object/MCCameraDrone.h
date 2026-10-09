#pragma once

#include "object/MCBigGameObject.h"

class MCGVAppearance;
class MCObjectEvent;

/// <summary>
/// The spotter drone a sensor probe launches: flies an outward square spiral of map tiles around its start,
/// revealing the terrain it passes over.
/// </summary>
/// <remarks>Original source: <c>object\artlry.cpp</c>, <c>object\artlry.h</c>.</remarks>
class MCCameraDrone : public MCBigGameObject
{
public:
    /// <summary>
    /// The part id of the first drone of a mission; the next ones count up from it.
    /// </summary>
    static constexpr int32_t FirstPartId = 0x802c8;
    /// <summary>
    /// How many drones a mission can launch (Fatal past it): kept, the drones' part id range (the weapon chunks carry
    /// a drone as its offset from <see cref="FirstPartId"/>).
    /// </summary>
    static constexpr int32_t MaxDrones = 1000;

    /// <summary>Starts in the world frame.</summary>
    MCCameraDrone();
    ~MCCameraDrone() override;

    /// <summary>Makes the GV appearance, takes the type's speed, hit points and CV, and sets the frame.</summary>
    int32_t Init(MCObjectType* objType) override;
    using MCBigGameObject::Init;
    int32_t Kill() override { return 0; }
    /// <summary>Flies toward the target tile, reveals the terrain around it and picks the next tile on arrival.</summary>
    int32_t Update() override;
    void Render() override;
    MCAppearance* GetAppearance() override;
    /// <summary>Selected by the mouse-over event (0x1c), deselected by 0x1d.</summary>
    int32_t HandleEvent(MCObjectEvent* event) override;
    int OnScreen() override;
    /// <summary>Takes the damage off the hit points; at none left the drone is destroyed and explodes.</summary>
    int32_t HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    MCFrameOfRef GetFrame() override { return Frame; }

    /// <summary>Turns the frame and steps the spiral: the next leg's direction and, every other leg, its length.</summary>
    void FindNextTargetTile();

    /// <summary>The spiral leg's direction, 0-3 (-1 before the first).</summary>
    int8_t SpiralDirection = -1;
    /// <summary>The spiral leg's length in tiles.</summary>
    int8_t SpiralLength = 1;
    /// <summary>The map tile being flown to.</summary>
    int32_t TargetTileRow = -1;
    int32_t TargetTileCol = -1;
    /// <summary>Meters per second (from the type).</summary>
    float MaxVelocity = 0;
    /// <summary>Hit points left.</summary>
    int32_t HitPoints = 0;
    /// <summary>The drone's orientation.</summary>
    MCFrameOfRef Frame;
    /// <summary>Scenario time the drone was launched (-1 until then).</summary>
    float LaunchTime = -1.0f;
    /// <summary>The drone's GV appearance.</summary>
    std::unique_ptr<MCGVAppearance> Appearance;
};

/// <summary>How many camera drones the mission has launched (each takes the next part id).</summary>
extern int32_t NumCameraDrones;
