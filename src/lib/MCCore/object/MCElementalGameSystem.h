#pragma once

class MCFitIniFile;

// The elementals' settings from the game system file (gamesys.fit), with MCX.EXE's values until it is read.

/// <summary>"Elemental:Collision" "DamageOnImpact".</summary>
extern float ElmDamageOnImpact;
/// <summary>"Elemental:Combat" "NoJumpRange": meters from its last target within which an elemental paths without
/// jumping (75).</summary>
extern float ElementalTargetNoJumpDistance;
/// <summary>Whether the old (pre-Terrain::projectTerrain) screen projection is used.</summary>
extern int UseOldProject;

/// <summary>Reads the "Elemental:Collision" and "Elemental:Combat" blocks of the game system file.</summary>
/// <returns>0, or the FIT error of the collision block.</returns>
int32_t LoadElementalGameSystem(MCFitIniFile& sysFile);
