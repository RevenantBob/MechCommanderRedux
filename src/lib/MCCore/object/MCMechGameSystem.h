#pragma once

class MCFitIniFile;
class MCGameObject;

// The mechs' settings from the game system file (gamesys.fit) and their fixed tables, with MCX.EXE's values until it
// is read.

/// <summary>A mech's body locations, in the mech file's order (indices into the mover's body).</summary>
enum MCMechBodyLocation : int32_t
{
    MechHead = 0,
    MechCenterTorso = 1,
    MechLeftTorso = 2,
    MechRightTorso = 3,
    MechLeftArm = 4,
    MechRightArm = 5,
    MechLeftLeg = 6,
    MechRightLeg = 7,
};

/// <summary>Body locations of a mech (the mech file's format).</summary>
inline constexpr int32_t NumMechBodyLocations = 8;
/// <summary>Armor locations of a mech: the body locations plus the three rear torso ones (the mech file's
/// format).</summary>
inline constexpr int32_t NumMechArmorLocations = 11;

/// <summary>Critical spaces of each body location.</summary>
extern int32_t NumLocationCriticalSpaces[NumMechBodyLocations];
/// <summary>Percent chance a disabled enemy mech leaves salvage (100 by default; the scenario may set it).</summary>
extern int32_t MechSalvageChance;
/// <summary>"MoveMarginOfError" (5, 10), read by loadMoverGameSystem.</summary>
extern float MoveMarginOfError[2];
/// <summary>Attack modifiers by rank and chassis; [r][1..4] are WeaponFireModifiers[7 + 4r..] (loadMoverGameSystem).</summary>
extern float RankVersusChassisCombatModifier[4][5];
/// <summary>The body location a hit on each hit section lands on.</summary>
extern int32_t MechHitSectionTable[5];
/// <summary>Each armor location's body location (the rear torso ones map to their torso).</summary>
extern char MechArmorToBodyLocation[12];
/// <summary>The two orthogonal neighbour directions of each path direction (crashAvoidanceSystem, diagonals).</summary>
extern int32_t AdjClippedCell[8][2];
/// <summary>"AttackerMoveModifier".</summary>
extern int32_t AttackerMoveModifier[9];
/// <summary>"CriticalHitTable".</summary>
extern char CriticalHitTable[4];
/// <summary>"TargetMoveModifierTable": (speed, modifier) pairs.</summary>
extern int32_t TargetMoveModifierTable[5][2];
/// <summary>The tonnage bounds of the mech classes ("MaxLightMech", "MaxHeavyMech"); read, but nothing uses them
/// (getMechClass has its own).</summary>
extern float MechClassWeights[5];
/// <summary>"HitLocationTable".</summary>
extern char MechHitLocationTable[0x84];
/// <summary>"MechTransferHitTable": where a hit on a destroyed location goes.</summary>
extern char MechTransferHitTable[8];
/// <summary>"PilotCheckConditions".</summary>
extern int32_t MechPilotCheckConditions[2];
/// <summary>"PilotCheckTerrainEffect", by terrain type.</summary>
extern int32_t MechPilotCheckTerrainEffect[0x40];
/// <summary>"CrashAvoidSelf" of "Mech:Movement", the mech types' default.</summary>
extern int32_t DefaultMechCrashAvoidSelf;
/// <summary>"CrashAvoidPath" of "Mech:Movement".</summary>
extern int32_t DefaultMechCrashAvoidPath;
/// <summary>"CrashBlockSelf" of "Mech:Movement".</summary>
extern int32_t DefaultMechCrashBlockSelf;
/// <summary>"CrashBlockPath" of "Mech:Movement".</summary>
extern int32_t DefaultMechCrashBlockPath;
/// <summary>"CrashYieldTime" of "Mech:Movement".</summary>
extern float DefaultMechCrashYieldTime;
/// <summary>Jump offsets (getJumpRange) by jump jets fitted, the last for six or more (the name is the port's).</summary>
extern int32_t MechJumpOffsets[7];
/// <summary>"JumpCost".</summary>
extern int32_t DefaultMechJumpCost;
/// <summary>"collisionThreshold": a slower mech bouncing off an object stops.</summary>
extern float MechCollisionThreshold;
/// <summary>"objectThreshold".</summary>
extern float ObjectCollisionThreshold;
/// <summary>"tonnageThreshold": mechs under it are deflected by trees.</summary>
extern float TonnageCollisionThreshold;
/// <summary>"treeDeflection", in degrees at the threshold tonnage.</summary>
extern float TreeDeflection;
/// <summary>"pivotAngle".</summary>
extern float MechPivotAngle;
/// <summary>"pivotThrottle".</summary>
extern float MechPivotThrottle;
/// <summary>The last mech to hit another (the debug log of internal component hits names it).</summary>
extern MCGameObject* BadGuy;
/// <summary>Speed state by gesture.</summary>
extern char MechSpeedStateArray[32];
/// <summary>Body state by gesture.</summary>
extern char MechStateByGesture[28];
/// <summary>Whether mechs leave footprints (1).</summary>
extern uint8_t FootPrints;
/// <summary>The splash radius of a mine, in meters.</summary>
extern float MineSplashRange;
/// <summary>The splash damage of a mine.</summary>
extern float MineSplashDamage;
/// <summary>The explosion object type of a mine.</summary>
extern int32_t MineExplosion;
/// <summary>The "Mine" block's "BaseDamage": a mine's hit on the mech stepping on it (the name is the port's).</summary>
extern float MineBaseDamage;

/// <summary>
/// Reads the "Mech:Class", "Mech:Movement", "Mech:FireWeapon", "Mech:Damage" and "Mech:Collision" blocks of the
/// game system file.
/// </summary>
/// <returns>0, or the first FIT error.</returns>
int32_t LoadMechGameSystem(MCFitIniFile& sysFile);
