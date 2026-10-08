#pragma once

class MCGameObject;

/// <summary>
/// One shot's effect on its target: who fired, with what, how much damage, where it hits and from which side.
/// </summary>
/// <remarks>Original source: <c>object\gameobj.cpp</c>.</remarks>
struct MCWeaponShotInfo
{
    /// <summary>
    /// Fills the record. In single player the damage is scaled by the difficulty (<c>applyDifficultyWeapon</c>);
    /// in a multiplayer game with packed damage it is rounded to quarter points and the angle to a quadrant.
    /// </summary>
    void Init(MCGameObject* shooter, int32_t weaponMasterId, float shotDamage, int32_t shotHitLocation,
              float shotEntryAngle);
    /// <summary>Sets the damage (rounded to quarter points when multiplayer packs it).</summary>
    void SetDamage(float shotDamage);
    /// <summary>Sets the entry angle (snapped to 0, -90, 90 or 180 when multiplayer packs it).</summary>
    void SetEntryAngle(float shotEntryAngle);

    /// <summary>Who fired.</summary>
    MCGameObject* Attacker = nullptr;
    /// <summary>The weapon's master component id (an index into <c>MasterComponentList</c>).</summary>
    int32_t MasterId = 0;
    /// <summary>Damage points (0 to 255).</summary>
    float Damage = 0;
    /// <summary>The body location hit (-1: none).</summary>
    int32_t HitLocation = 0;
    /// <summary>The angle the shot comes from, relative to the target's facing, in degrees.</summary>
    float EntryAngle = 0;
};

/// <summary>
/// The quadrant an entry angle comes from, as the chunks pack it: 0 front (-45 to 45), 2 left (-135 to -45), 3 right
/// (45 to 135), 1 rear.
/// </summary>
int8_t AngleQuadrant(float angle);
/// <summary>An entry angle snapped to its quadrant's middle (0, -90, 90 or 180), for packed multiplayer damage.</summary>
float SnapAngle(float angle);
/// <summary>Damage rounded down to quarter points, as the multiplayer chunks carry it.</summary>
float QuarterPoints(float damage);
