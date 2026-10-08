#pragma once

#include "lib/MCVector3D.h"

class MCBigGameObject;

/// <summary>
/// A weapon fired, as sent between multiplayer machines: the target (a mover by roster index, a terrain object by
/// block/vertex/item, a train car or camera drone, or a map cell), the weapon, whether it hit, and the missile
/// counts. <see cref="Pack"/> squeezes it into <see cref="Data"/>.
/// </summary>
/// <remarks>Original source: <c>object\gameobj.cpp</c>, <c>object\gameobj.h</c>. The packed word is wire format.</remarks>
class MCWeaponFireChunk
{
public:
    /// <summary>Clears the chunk (no hit location).</summary>
    void Init();
    /// <summary>
    /// Targets a mover: its multiplayer roster index, the weapon, hit or miss, the entry angle's quadrant, the
    /// missile counts and the hit location.
    /// </summary>
    void BuildMoverTarget(MCBigGameObject* target, int32_t weapon, int hitTarget, float angle, int32_t missiles,
                          int32_t missilesPastAMS, int32_t antiMissileShots, int32_t location);
    /// <summary>Targets a terrain object by its part id (split into block, vertex and item).</summary>
    void BuildTerrainTarget(MCBigGameObject* target, int32_t weapon, int hitTarget, int32_t missiles);
    /// <summary>Targets a train car by its part id (split into train and car).</summary>
    void BuildTrainTarget(MCBigGameObject* target, int32_t weapon, int hitTarget, float angle, int32_t missiles);
    /// <summary>Targets a camera drone (a "train" numbered 0x80).</summary>
    void BuildCameraDroneTarget(MCBigGameObject* target, int32_t weapon, int hitTarget, float angle, int32_t missiles);
    /// <summary>Targets a map location (its cell).</summary>
    void BuildLocationTarget(MCVector3D location, int32_t weapon, int hitTarget, int32_t missiles);
    /// <summary>Packs the fields into <see cref="Data"/>.</summary>
    void Pack();
    /// <summary>Unpacks <see cref="Data"/>; <paramref name="attacker"/> tells whether the weapon fires missiles.</summary>
    void Unpack(MCBigGameObject* attacker);
    /// <summary>Whether every field matches <paramref name="chunk"/> (logs the difference when not).</summary>
    int EqualTo(MCWeaponFireChunk* chunk);

    /// <summary>0 mover, 1 terrain object, 2 train car or camera drone, 3 map location.</summary>
    int8_t TargetType = 0;
    /// <summary>The mover's roster index, or the target's part id.</summary>
    int32_t TargetId = 0;
    /// <summary>Terrain: the block; train: the train number (0x80 for a camera drone).</summary>
    int32_t TargetBlockOrTrainNumber = 0;
    /// <summary>Terrain: the vertex; train: the car number (camera drone: its part id minus 0x802c8).</summary>
    int32_t TargetVertexOrCarNumber = 0;
    /// <summary>Terrain: the object's item number on its vertex (0 to 7).</summary>
    int8_t TargetItemNumber = 0;
    /// <summary>Location: the target cell's row and column.</summary>
    std::array<uint16_t, 2> TargetCell{};
    /// <summary>The weapon's index among the attacker's weapons.</summary>
    uint8_t WeaponIndex = 0;
    /// <summary>Nonzero when the shot hit.</summary>
    int32_t Hit = 0;
    /// <summary>The entry angle's quadrant: 0 front, 1 rear, 2 left, 3 right.</summary>
    int8_t EntryAngle = 0;
    /// <summary>Missiles fired (0 for a weapon that fires none).</summary>
    int8_t NumMissiles = 0;
    /// <summary>Missiles left after the target's anti-missile system (equal to numMissiles when none).</summary>
    int8_t NumMissilesPastAms = 0;
    /// <summary>Shots the target's anti-missile system fired.</summary>
    int8_t NumAntiMissileShots = 0;
    /// <summary>The body location hit (-1 to 11; -1 after <see cref="Init"/>).</summary>
    int8_t HitLocation = 0;
    /// <summary>The packed chunk.</summary>
    uint32_t Data = 0;
};
