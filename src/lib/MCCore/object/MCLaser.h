#pragma once

#include "object/MCBigGameObject.h"
#include "object/MCObjectWatcher.h"
#include "object/MCWeaponShotInfo.h"

/// <summary>
/// A laser (or PPC) beam drawn from its source's hot spot to its target, through the type's colour stages; when the
/// stages (or the PPC's hit frame) are reached it applies its shot to the target and creates the hit or miss effect.
/// </summary>
/// <remarks>Original source: <c>object\laser.cpp</c>, <c>object\laser.h</c>.</remarks>
class MCLaser : public MCBigGameObject
{
public:
    MCLaser();
    /// <summary>Stops watching the target and source.</summary>
    ~MCLaser() override;

    /// <summary>Takes the type (its init result unread, as the original's); object class Laser.</summary>
    int32_t Init(MCObjectType* objType) override;
    using MCBigGameObject::Init;
    int32_t Kill() override { return 0; }
    /// <summary>
    /// Steps the beam through its colour stages (the enemy set when the source's alignment isn't 1) or, for a PPC,
    /// its effect frames; applies the shot to the target once (host only in multiplayer).
    /// </summary>
    /// <returns>1 while the beam lasts, 0 when done.</returns>
    int32_t Update() override;
    /// <summary>
    /// Draws the beam (polygons in the stage colours, or the PPC shape stretched along it); creates the hit or miss
    /// effect (and a crater on a miss) once.
    /// </summary>
    void Render() override;

    /// <summary>Sets the point the beam ends at.</summary>
    void SetTargetPosition(MCVector3D position) { TargetPosition = position; }
    /// <summary>
    /// Watches <paramref name="source"/>, fires from its hot spot <paramref name="sourceHotSpot"/> at
    /// <paramref name="targetPos"/>, and copies <paramref name="shotInfo"/> (if any) as the shot to apply.
    /// </summary>
    void Connect(MCGameObject* source, MCVector3D targetPos, MCWeaponShotInfo* shotInfo, int32_t sourceHotSpot);

    /// <summary>The current colour stage; 0xff before the first update.</summary>
    uint8_t CurrentStage = 0xff;
    /// <summary>Seconds left in the current stage.</summary>
    float StageTimeLeft = 0;
    /// <summary>The current stage's outer (cool) colour.</summary>
    uint32_t CoolColor = 0;
    /// <summary>The current stage's core (hot) colour; the core is drawn only when it differs from the outer.</summary>
    uint32_t HotColor = 0;
    /// <summary>The object that fired.</summary>
    MCBaseObjectWatcher Source;
    /// <summary>The source's hot spot the beam starts at.</summary>
    int32_t SourceHotSpot = 0;
    /// <summary>The object hit; the shot is applied to it.</summary>
    MCBaseObjectWatcher Target;
    /// <summary>
    /// The target's hot spot that was hit, set by the launchers. The original never read it (OB-017); the beam ends there.
    /// </summary>
    int32_t TargetHotSpot = 0;
    /// <summary>Where the beam ends, once set.</summary>
    std::optional<MCVector3D> TargetPosition;
    /// <summary>The shot applied to the target.</summary>
    MCWeaponShotInfo ShotInfo{.MasterId = -1, .HitLocation = -1};
    /// <summary>Set once the hit or miss effect has been created.</summary>
    bool HitEffectCreated = false;
    /// <summary>The current frame of the PPC effect.</summary>
    int32_t PpcFrame = 0;
    /// <summary>Time left in the PPC effect's animation period (restarts at the type's animPPC).</summary>
    float PpcAnimTimeLeft = 0;
    /// <summary>Time left in the current PPC frame (restarts at the type's lengthPPC).</summary>
    float PpcFrameTimeLeft = 0;
    /// <summary>Set until the first update (a PPC's plays the sound).</summary>
    bool JustCreated = true;
    /// <summary>Set once the shot has been applied to the target.</summary>
    bool DamageApplied = false;
};
