#pragma once

class MCBigGameObject;
class MCGameObject;
struct MCWeaponShotInfo;

/// <summary>
/// A weapon hit, as sent between multiplayer machines: the target (a mover by roster index, a terrain object, a
/// train car or camera drone), the damage, its cause, the hit location and entry angle.
/// </summary>
/// <remarks>Original source: <c>object\gameobj.cpp</c>. The packed word is wire format.</remarks>
class MCWeaponHitChunk
{
public:
    void BuildMoverTarget(MCBigGameObject* target, int32_t hitCause, float hitDamage, int32_t location, float angle,
                          int isRefit);
    void BuildTerrainTarget(MCBigGameObject* target, float hitDamage);
    void BuildTrainTarget(MCBigGameObject* target, float hitDamage, float angle);
    void BuildCameraDroneTarget(MCBigGameObject* target, float hitDamage, float angle);
    /// <summary>Builds the chunk for <paramref name="target"/> from a shot.</summary>
    void Build(MCGameObject* target, MCWeaponShotInfo* shotInfo, int isRefit);
    /// <summary>Packs the fields into <see cref="Data"/> (damage in quarter points).</summary>
    void Pack();
    /// <summary>Unpacks <see cref="Data"/>.</summary>
    void Unpack();
    /// <summary>Whether every field matches <paramref name="chunk"/> (logs the difference when not).</summary>
    int EqualTo(MCWeaponHitChunk* chunk);

    /// <summary>0 mover, 1 terrain object, 2 train car or camera drone.</summary>
    int8_t TargetType = 0;
    /// <summary>The mover's roster index, or the target's part id.</summary>
    int32_t TargetId = 0;
    /// <summary>Terrain: the block; train: the train number (0x80 for a camera drone).</summary>
    int32_t TargetBlockOrTrainNumber = 0;
    /// <summary>Terrain: the vertex; train: the car number.</summary>
    int32_t TargetVertexOrCarNumber = 0;
    /// <summary>Terrain: the object's item number on its vertex.</summary>
    int8_t TargetItemNumber = 0;
    /// <summary>
    /// What caused the hit (-7 to 0): 0 or the shot's master id adjusted by <see cref="Build"/> (-4 for a
    /// component whose form is ammo).
    /// </summary>
    int8_t Cause = 0;
    /// <summary>Damage points.</summary>
    float Damage = 0;
    /// <summary>The body location hit (-1 to 11).</summary>
    int8_t HitLocation = 0;
    /// <summary>The entry angle's quadrant: 0 front, 1 rear, 2 left, 3 right.</summary>
    int8_t EntryAngle = 0;
    /// <summary>Nonzero when the "hit" is a refit (repairs rather than damages).</summary>
    int32_t Refit = 0;
    /// <summary>The packed chunk.</summary>
    uint32_t Data = 0;
};
