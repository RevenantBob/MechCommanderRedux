#pragma once

class MCFitIniFile;

// The ground vehicles' settings from the game system file (gamesys.fit) and their fixed tables, with MCX.EXE's
// values until it is read.

/// <summary>A ground vehicle's armor locations, in the profile's order (indices into the mover's armor).</summary>
enum MCGroundVehicleLocation : int32_t
{
    GroundVehicleFront = 0,
    GroundVehicleLeft = 1,
    GroundVehicleRight = 2,
    GroundVehicleRear = 3,
    GroundVehicleTurret = 4,
};

/// <summary>Armor locations of a ground vehicle (the profile's format).</summary>
inline constexpr int32_t NumGroundVehicleLocations = 5;
/// <summary>Most seats a vehicle type may have ("Seats", asserted; the scenario fills at most four
/// passengers).</summary>
inline constexpr int32_t MaxGroundVehicleSeats = 4;
/// <summary>Terrain tile types per chassis row of <see cref="TileThrottleMultiplier"/>.</summary>
inline constexpr int32_t NumThrottleTileTypes = 59;
/// <summary>Overlay types per chassis row of <see cref="OverlayThrottleMultiplier"/>.</summary>
inline constexpr int32_t NumThrottleOverlayTypes = 75;

/// <summary>The effect object type of each weapon effect (MasterComponent::weaponEffect).</summary>
extern uint32_t WeaponFXTable[32];
/// <summary>"GroundVehicle:FireWeapon" "AttackerMoveModifier".</summary>
extern int32_t GroundVehicleAttackerMoveModifier[4];
/// <summary>"GroundVehicle:Damage" "CriticalHitTable".</summary>
extern int32_t GroundVehicleCriticalHitTable[11];
/// <summary>Throttle factor per chassis and terrain tile type (calcThrottleLimits); 1 everywhere.</summary>
extern float TileThrottleMultiplier[3][NumThrottleTileTypes];
/// <summary>Throttle factor per chassis and overlay type (calcThrottleLimits); 1 everywhere.</summary>
extern float OverlayThrottleMultiplier[3][NumThrottleOverlayTypes];
/// <summary>"GroundVehicle:Movement" "CrashAvoidSelf": the default for types that don't set it.</summary>
extern int32_t DefaultGroundVehicleCrashAvoidSelf;
/// <summary>"GroundVehicle:Movement" "CrashAvoidPath".</summary>
extern int32_t DefaultGroundVehicleCrashAvoidPath;
/// <summary>"GroundVehicle:Movement" "CrashBlockSelf".</summary>
extern int32_t DefaultGroundVehicleCrashBlockSelf;
/// <summary>"GroundVehicle:Movement" "CrashBlockPath".</summary>
extern int32_t DefaultGroundVehicleCrashBlockPath;
/// <summary>"GroundVehicle:Movement" "CrashYieldTime".</summary>
extern float DefaultGroundVehicleCrashYieldTime;
/// <summary>"GroundVehicle:Collision" "collisionThreshold".</summary>
extern float GvCollisionThreshold;
/// <summary>"GroundVehicle:Collision" "objectThreshold".</summary>
extern float GvObjectCollisionThreshold;
/// <summary>"GroundVehicle:Collision" "tonnageThreshold".</summary>
extern float GvTonnageCollisionThreshold;
/// <summary>"GroundVehicle:Collision" "treeDeflection" (the name is the port's).</summary>
extern float GvTreeDeflection;
/// <summary>"GroundVehicle:Movement" "SweeperSlowTime": how long a mine sweeper crawls after clearing a mine.</summary>
extern float GvSweepTime;
/// <summary>"GroundVehicle:Movement" "HillSpeedFactor".</summary>
extern float GvHillSpeedFactor;
/// <summary>Set by updateMovePath: 100 m in world units, or the distance left on reaching the path's end.
/// Nothing reads it.</summary>
extern float MaxVelocityMag;

/// <summary>
/// Reads the ground vehicle blocks of the game system file: attacker move modifiers, the critical hit table, the
/// collision thresholds and the movement defaults (crash avoidance, sweeper time, walk speed, hill factor).
/// </summary>
/// <returns>0, or the first FIT error.</returns>
int32_t LoadGroundVehicleGameSystem(MCFitIniFile& sysFile);
