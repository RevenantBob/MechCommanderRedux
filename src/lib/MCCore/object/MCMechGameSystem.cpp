#include "stdafx.h"
#include "object/MCMechGameSystem.h"
#include "object/MCGameSystemReader.h"
#include "object/MCMoverGameSystem.h"
#include "sound/MCSoundSystem.h"

char MechSpeedStateArray[32] = {0, 0, 0, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1,  1,  1,  1,
                                2, 2, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1};
char MechStateByGesture[28] = {0, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 2, 2, 8, 7, 8, 7, 8, 7, 6, 7, 7, 7, 8, 0, 0, 0};
int32_t NumLocationCriticalSpaces[NumMechBodyLocations] = {6, 12, 12, 12, 12, 12, 6, 6};
int32_t MechHitSectionTable[5] = {1, 1, 0, 2, 1};
int32_t AdjClippedCell[8][2] = {{0, 0}, {0, 2}, {2, 2}, {2, 4}, {4, 4}, {4, 6}, {6, 6}, {6, 0}};
float RankVersusChassisCombatModifier[4][5] = {{0.0f, 0.0f, -5.0f, -15.0f, -25.0f},
                                               {0.0f, 5.0f, 0.0f, -5.0f, -15.0f},
                                               {0.0f, 10.0f, 5.0f, 0.0f, -5.0f},
                                               {0.0f, 15.0f, 10.0f, 5.0f, 0.0f}};
float WeaponFireModifiers[30] = {0.0f,   -10.0f, -20.0f, -98.0f, -85.0f, -88.0f, 50.0f, 0.0f, -5.0f, -15.0f,
                                 -25.0f, 5.0f,   0.0f,   -5.0f,  -15.0f, 10.0f,  5.0f,  0.0f, -5.0f, 15.0f,
                                 10.0f,  5.0f,   0.0f,   0.0f,   0.0f,   0.0f,   0.0f,  0.0f, 0.0f,  0.0f};
int32_t AttackerMoveModifier[9] = {0, 0, 10, 20, 10, 5, 30, 0, 0};
char CriticalHitTable[4] = {58, 83, 97, 100};
int32_t TargetMoveModifierTable[5][2] = {{6, 0}, {12, 1}, {18, 2}, {27, 3}, {999, 4}};
float MechClassWeights[5] = {0.0f, 35.0f, 55.0f, 75.0f, 100.0f};
char MechHitLocationTable[0x84] = {
    0x1e, 0x14, 0x19, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x14, 0x19, 0x19, 0x14, 0x0f, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x19, 0x00, 0x14, 0x0f, 0x00, 0x19, 0x00,
    0x00, 0x00, 0x00, 0x0f, 0x00, 0x19, 0x00, 0x14, 0x14, 0x14, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x14, 0x14, 0x00, 0x00, 0x14, 0x14, 0x14, 0x00, 0x00, 0x19, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x19,
    0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x2d,
    0x2d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2d, 0x2d, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00};
char MechTransferHitTable[8] = {1, -1, 1, 1, 2, 3, 2, 3};
char MechArmorToBodyLocation[12] = {0, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3, 0};
int32_t MechPilotCheckConditions[2] = {25, 25};
float MoveMarginOfError[2] = {5.0f, 10.0f};
int32_t MechSalvageChance = 100;
int32_t DefaultMechCrashAvoidSelf = 1;
int32_t DefaultMechCrashAvoidPath = 1;
int32_t DefaultMechCrashBlockSelf = 1;
int32_t DefaultMechCrashBlockPath = 1;
float DefaultMechCrashYieldTime = 2.0f;
int32_t DefaultMechJumpCost = 5000;
int32_t MechJumpOffsets[7] = {8, 16, 40, 56, 72, 88, 104};
int32_t MechPilotCheckTerrainEffect[0x40] = {};
float MechCollisionThreshold = 0.0f;
float ObjectCollisionThreshold = 0.0f;
float TonnageCollisionThreshold = 0.0f;
float TreeDeflection = 0.0f;
float MechPivotAngle = 0.0f;
float MechPivotThrottle = 0.0f;
MCGameObject* BadGuy = nullptr;
uint8_t FootPrints = 1;
float MineSplashRange = 0.0f;
float MineSplashDamage = 0.0f;
int32_t MineExplosion = 0;
float MineBaseDamage = 0.0f;
int32_t FriendlyDestroyed = 0;
int32_t EnemyDestroyed = 0;

auto LoadMechGameSystem(MCFitIniFile& sysFile) -> int32_t
{
    // Each check below is where the original returned the first error; what follows it ran only without one.
    MCGameSystemReader read(sysFile);
    read.Block("Mech:Class");
    read.Value("MaxLightMech", MechClassWeights[1]);
    // Original behaviour (OB-150): "MaxHeavyMech" is read for both the medium and the heavy bound.
    read.Value("MaxHeavyMech", MechClassWeights[2]);
    read.Value("MaxHeavyMech", MechClassWeights[3]);
    read.Block("Mech:Movement");

    if (read.Error() != 0)
    {
        return read.Error();
    }

    MCGameSystemReader::Optional(sysFile, "JumpCost", DefaultMechJumpCost);
    MCGameSystemReader::Optional(sysFile, "CrashAvoidSelf", DefaultMechCrashAvoidSelf);
    MCGameSystemReader::Optional(sysFile, "CrashAvoidPath", DefaultMechCrashAvoidPath);
    MCGameSystemReader::Optional(sysFile, "CrashBlockSelf", DefaultMechCrashBlockSelf);
    MCGameSystemReader::Optional(sysFile, "CrashBlockPath", DefaultMechCrashBlockPath);
    MCGameSystemReader::Optional(sysFile, "CrashYieldTime", DefaultMechCrashYieldTime);
    read.Array("PilotCheckConditions", MechPilotCheckConditions);
    read.Array("PilotCheckTerrainEffect", MechPilotCheckTerrainEffect);
    read.Block("Mech:FireWeapon");
    read.Array("AttackerMoveModifier", AttackerMoveModifier);
    read.Array("HitLocationTable", MechHitLocationTable);
    std::array<int32_t, 10> targetMoveModifiers{};
    read.Array("TargetMoveModifierTable", targetMoveModifiers);

    if (read.Error() != 0)
    {
        return read.Error();
    }

    for (size_t i = 0; i < targetMoveModifiers.size(); i++)
    {
        TargetMoveModifierTable[i / 2][i % 2] = targetMoveModifiers[i];
    }

    read.Block("Mech:Damage");
    read.Array("CriticalHitTable", CriticalHitTable);
    read.Array("MechTransferHitTable", MechTransferHitTable);
    read.Value("MechSalvageChance", MechSalvageChance);
    read.Block("Mech:Collision");
    read.Value("collisionThreshold", MechCollisionThreshold);
    read.Value("objectThreshold", ObjectCollisionThreshold);
    read.Value("tonnageThreshold", TonnageCollisionThreshold);
    read.Value("treeDeflection", TreeDeflection);
    read.Value("pivotAngle", MechPivotAngle);
    read.Value("pivotThrottle", MechPivotThrottle);
    return read.Error();
}
