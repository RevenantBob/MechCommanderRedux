#include "stdafx.h"
#include "object/mech.h"
#include "main/fixes.h"
#include "abl/abldbug.h"
#include "ai/move.h"
#include "ai/tacordr.h"
#include "appear/MCAppearanceType.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/MCCamera.h"
#include "camera/MCCameraList.h"
#include "engine/MCLineElement.h"
#include "engine/MCVfxElement.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCCraterManager.h"
#include "vfx/MCVfxFunctions.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "iface/iface.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "lib/MCPacketFile.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/aictrl.h"
#include "object/bridge.h"
#include "object/artlry.h"
#include "object/laser.h"
#include "object/bullet.h"
#include "object/cmponent.h"
#include "object/collsn.h"
#include "object/comndr.h"
#include "object/contact.h"
#include "object/debris.h"
#include "object/elemntl.h"
#include "object/explode.h"
#include "object/gvehicl.h"
#include "object/group.h"
#include "object/jet.h"
#include "object/mechctrl.h"
#include "object/mechdyn.h"
#include "object/netctrl.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/objtype.h"
#include "object/plyrctrl.h"
#include "object/prjlase.h"
#include "object/smoke.h"
#include "object/team.h"
#include "object/warrior.h"
#include "sound/radio.h"
#include "sound/soundsys.h"
#include "terrain/MCTerrain.h"
#include "sprite/MCMechActor.h"

char MechSpeedStateArray[32] = {0, 0, 0, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1,  1,  1,  1,
                                2, 2, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1};
char MechStateByGesture[28] = {0, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 2, 2, 8, 7, 8, 7, 8, 7, 6, 7, 7, 7, 8, 0, 0, 0};
int32_t NumLocationCriticalSpaces[NUM_MECH_BODY_LOCATIONS] = {6, 12, 12, 12, 12, 12, 6, 6};
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
// Each armor location's body location (the rear torso ones map to the torso).
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
int FriendlyDestroyed = 0;
int EnemyDestroyed = 0;

namespace
{
    /// <summary>Half pi, as MCX.EXE stores it.</summary>
    constexpr double HALF_PI = 0x1.921fb5443e88cp+0;
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;

    /// <summary>Turns a frame about its k axis (MC2's inline frame_of_ref::rotate_about_k).</summary>
    void RotateAboutK(MCFrameOfRef& frame, float s, float c)
    {
        const MCVector3D oldI = frame.I;
        frame.I = frame.I * c + frame.J * s;
        frame.J = frame.J * c - oldI * s;
    }

    void RotateAboutKUnroundedCos(MCFrameOfRef& frame, float s, double c)
    {
        const MCVector3D oldI = frame.I;
        const MCVector3D oldJ = frame.J;
        const double sd = static_cast<double>(s);
        frame.I.X = static_cast<float>(c * oldI.X + sd * oldJ.X);
        frame.I.Y = static_cast<float>(c * oldI.Y) + s * oldJ.Y;
        frame.I.Z = static_cast<float>(c * oldI.Z) + s * oldJ.Z;
        frame.J.X = static_cast<float>(c * oldJ.X) - s * oldI.X;
        frame.J.Y = static_cast<float>(c * oldJ.Y) - s * oldI.Y;
        frame.J.Z = static_cast<float>(c * oldJ.Z - static_cast<double>(s * oldI.Z));
    }

    /// <summary>
    /// Damage from bumping into <paramref name="other"/>: tonnage / 10 + 1/2 (an enemy) or tonnage / 100 + 1/2 (a
    /// friend), hitting <paramref name="victim"/> from <paramref name="other"/>'s side.
    /// </summary>
    void CollisionHit(MCGameObject* victim, MCGameObject* shooter, MCGameObject* tonnageOf, int32_t attackSource,
                      int friendly)
    {
        const int32_t hitLocation = victim->CalcHitLocation(shooter, -1, attackSource, 0);
        const float entryAngle = victim->RelFacingTo(shooter->GetPosition(), -1);
        const double scale = friendly == 0 ? 0.1 : 0.01;
        MCWeaponShotInfo shotInfo;
        shotInfo.Init(shooter, -1, static_cast<float>(tonnageOf->GetTonnage() * scale + 0.5), hitLocation, entryAngle);
        victim->HandleWeaponHit(&shotInfo, MPlayer != nullptr);
    }
}

auto LoadMechGameSystem(MCFitIniFile* sysFile) -> int32_t
{
    int32_t result = sysFile->SeekBlock("Mech:Class");

    if (result != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdFloat("MaxLightMech", MechClassWeights[1])) != 0)
    {
        return result;
    }

    // The original reads "MaxHeavyMech" for both the medium and the heavy bound.
    if ((result = sysFile->ReadIdFloat("MaxHeavyMech", MechClassWeights[2])) != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdFloat("MaxHeavyMech", MechClassWeights[3])) != 0)
    {
        return result;
    }

    if ((result = sysFile->SeekBlock("Mech:Movement")) != 0)
    {
        return result;
    }

    int32_t jumpCost = 0;

    if (sysFile->ReadIdLong("JumpCost", jumpCost) == 0)
    {
        DefaultMechJumpCost = jumpCost;
    }

    int32_t value = 0;

    if (sysFile->ReadIdLong("CrashAvoidSelf", value) == 0)
    {
        DefaultMechCrashAvoidSelf = value;
    }

    if (sysFile->ReadIdLong("CrashAvoidPath", value) == 0)
    {
        DefaultMechCrashAvoidPath = value;
    }

    if (sysFile->ReadIdLong("CrashBlockSelf", value) == 0)
    {
        DefaultMechCrashBlockSelf = value;
    }

    if (sysFile->ReadIdLong("CrashBlockPath", value) == 0)
    {
        DefaultMechCrashBlockPath = value;
    }

    float yieldTime = 0.0f;

    if (sysFile->ReadIdFloat("CrashYieldTime", yieldTime) == 0)
    {
        DefaultMechCrashYieldTime = yieldTime;
    }

    if ((result = sysFile->ReadIdLongArray("PilotCheckConditions", MechPilotCheckConditions, 2)) != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdLongArray("PilotCheckTerrainEffect", MechPilotCheckTerrainEffect, 0x40)) != 0)
    {
        return result;
    }

    if ((result = sysFile->SeekBlock("Mech:FireWeapon")) != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdLongArray("AttackerMoveModifier", AttackerMoveModifier, 9)) != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdCharArray("HitLocationTable", MechHitLocationTable, 0x84)) != 0)
    {
        return result;
    }

    int32_t targetMoveModifiers[10];

    if ((result = sysFile->ReadIdLongArray("TargetMoveModifierTable", targetMoveModifiers, 10)) != 0)
    {
        return result;
    }

    std::memcpy(TargetMoveModifierTable, targetMoveModifiers, sizeof(TargetMoveModifierTable));

    if ((result = sysFile->SeekBlock("Mech:Damage")) != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdCharArray("CriticalHitTable", CriticalHitTable, 4)) != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdCharArray("MechTransferHitTable", MechTransferHitTable, 8)) != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdLong("MechSalvageChance", MechSalvageChance)) != 0)
    {
        return result;
    }

    if ((result = sysFile->SeekBlock("Mech:Collision")) != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdFloat("collisionThreshold", MechCollisionThreshold)) != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdFloat("objectThreshold", ObjectCollisionThreshold)) != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdFloat("tonnageThreshold", TonnageCollisionThreshold)) != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdFloat("treeDeflection", TreeDeflection)) != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdFloat("pivotAngle", MechPivotAngle)) != 0)
    {
        return result;
    }

    return sysFile->ReadIdFloat("pivotThrottle", MechPivotThrottle);
}

//---------------------------------------------------------------------------
// BattleMechType
//---------------------------------------------------------------------------

auto MCBattleMechType::Init() -> void
{
    RightArmDebrisId = 0xffffffff;
    LeftArmDebrisId = 0xffffffff;
    DestroyedPiece = 0xffffffff;
    CrashAvoidSelf = DefaultMechCrashAvoidSelf;
    CrashAvoidPath = DefaultMechCrashAvoidPath;
    CrashBlockSelf = DefaultMechCrashBlockSelf;
    CrashBlockPath = DefaultMechCrashBlockPath;
    MechId = 0;
    Name.clear();
    MechType = 0;
    Chassis = 0;
    TonnageClass = 0.0f;
    EndoSteel = 0;
    InternalStructureTonnage = 0.0f;
    HotSpotData = nullptr;
    GestureHotSpots = nullptr;
    JumpData = nullptr;
    FootprintType = 1;
    GestureOutlines = nullptr;
    WeaponHotSpots = nullptr;
    NumFramesPerHotSpot = nullptr;
    NumWeapons = 0;
    NumOthers = 0;
    NumHotSpotPackets = 0;
    DynamicsType = nullptr;
    CrashYieldTime = DefaultMechCrashYieldTime;
    ExplDmg = 0.0f;
    ExplRad = 0.0f;
}

auto MCBattleMechType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    static const char* const bodyLocationNames[NUM_MECH_BODY_LOCATIONS] = {
        "Head", "CenterTorso", "LeftTorso", "RightTorso", "LeftArm", "RightArm", "LeftLeg", "RightLeg"};

    MCFitIniFile mechFile;
    int32_t result = mechFile.Open(objFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if ((result = mechFile.SeekBlock("Header")) != 0)
    {
        return result;
    }

    char fileType[128];

    if ((result = mechFile.ReadIdString("FileType", fileType, 127)) != 0)
    {
        return result;
    }

    if (std::strcmp(fileType, "MechType") != 0)
    {
        return -1;
    }

    if ((result = mechFile.SeekBlock("General")) != 0)
    {
        return result;
    }

    if ((result = mechFile.ReadIdULong("ID", MechId)) != 0)
    {
        return result;
    }

    // "Type" 0 is 1, 1 is -1.
    static constexpr uint8_t typeMap[2] = {1, 0xff};
    uint8_t type = 0;

    if ((result = mechFile.ReadIdUChar("Type", type)) != 0)
    {
        return result;
    }

    // Port fix: the original reads other values from past its two-entry table on the stack.
    MechType = type < 2 ? typeMap[type] : 0;
    char nameBuffer[128];
    mechFile.ReadIdString("Name", nameBuffer, 127);
    Name = nameBuffer;

    if ((result = mechFile.ReadIdUChar("Chassis", Chassis)) != 0)
    {
        return result;
    }

    if ((result = mechFile.ReadIdFloat("TonnageClass", TonnageClass)) != 0)
    {
        return result;
    }

    if (mechFile.ReadIdFloat("ExplosionRadius", ExplRad) != 0)
    {
        ExplRad = 0.0f;
    }

    if (mechFile.ReadIdFloat("ExplosionDamage", ExplDmg) != 0)
    {
        ExplDmg = 0.0f;
    }

    uint8_t endo = 0;

    if ((result = mechFile.ReadIdUChar("EndoSteel", endo)) != 0)
    {
        return result;
    }

    EndoSteel = endo;

    if ((result = mechFile.ReadIdFloat("InternalStructureTonnage", InternalStructureTonnage)) != 0)
    {
        return result;
    }

    if ((result = mechFile.SeekBlock("InternalStructure")) != 0)
    {
        return result;
    }

    for (int32_t location = 0; location < NUM_MECH_BODY_LOCATIONS; location++)
    {
        if ((result = mechFile.ReadIdUChar(bodyLocationNames[location], InternalStructure[location])) != 0)
        {
            return result;
        }
    }

    if ((result = mechFile.SeekBlock("Debris")) != 0)
    {
        return result;
    }

    if ((result = mechFile.ReadIdULong("RightArmPiece", RightArmDebrisId)) != 0)
    {
        return result;
    }

    if ((result = mechFile.ReadIdULong("LeftArmPiece", LeftArmDebrisId)) != 0)
    {
        return result;
    }

    if ((result = mechFile.ReadIdULong("DestroyedPiece", DestroyedPiece)) != 0)
    {
        return result;
    }

    if ((result = mechFile.SeekBlock("Dynamics")) != 0)
    {
        return result;
    }

    uint32_t dynamicsTypeId = 0;

    if ((result = mechFile.ReadIdULong("Type", dynamicsTypeId)) != 0)
    {
        return result;
    }

    if (dynamicsTypeId != 1)
    {
        return -0x5fffd;
    }

    DynamicsType = new MCMechDynamicsType;

    if (DynamicsType == nullptr)
    {
        return -0x5fffe;
    }

    if ((result = DynamicsType->Init(&mechFile)) != 0)
    {
        return result;
    }

    if (mechFile.SeekBlock("MovementSystem") == 0)
    {
        int32_t value = 0;

        if (mechFile.ReadIdLong("CrashAvoidSelf", value) == 0)
        {
            CrashAvoidSelf = value;
        }

        if (mechFile.ReadIdLong("CrashAvoidPath", value) == 0)
        {
            CrashAvoidPath = value;
        }

        if (mechFile.ReadIdLong("CrashBlockSelf", value) == 0)
        {
            CrashBlockSelf = value;
        }

        if (mechFile.ReadIdLong("CrashBlockPath", value) == 0)
        {
            CrashBlockPath = value;
        }

        float yieldTime = 0.0f;

        // The original stores the last long read ("CrashBlockPath"), not the yield time it just read.
        if (mechFile.ReadIdFloat("CrashYieldTime", yieldTime) == 0)
        {
            CrashYieldTime = static_cast<float>(value);
        }
    }

    if ((result = LoadHotSpots(&mechFile)) != 0)
    {
        return result;
    }

    return MCObjectType::Init(&mechFile);
}

auto MCBattleMechType::Destroy() -> void
{
    Name.clear();
    delete DynamicsType;
    DynamicsType = nullptr;
    MCObjectType::Destroy();
}

auto MCBattleMechType::HandleCollision(MCGameObject* collidee, MCGameObject* collider) -> int
{
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        return 0;
    }

    int friendly = 0;
    int collideeJumping = static_cast<MCMover*>(collidee)->IsJumping(nullptr);
    int colliderJumping = 0;
    uint32_t sampleId = 4;

    switch (collider->ObjectClass)
    {
        case BATTLEMECH:
        {
            if (collidee->GetPilot()->Alignment != collider->GetPilot()->Alignment)
            {
                MCMechWarrior* attackerPilot = collider->GetPilot();

                if (attackerPilot->CurTacOrder.Code == TACTICAL_ORDER_ATTACK_OBJECT)
                {
                    attackerPilot->NumRams++;
                }
                else if (attackerPilot->CurTacOrder.Code == TACTICAL_ORDER_JUMPTO_POINT &&
                         collidee->GetPilot()->CurTacOrder.GetJumpTarget() == collidee)
                {
                    collidee->GetPilot()->NumJumpAttacks++;
                }
            }

            colliderJumping = static_cast<MCMover*>(collider)->IsJumping(nullptr);
            auto* colliderMech = static_cast<MCBattleMech*>(collider);

            if (colliderJumping == 0 && colliderMech->JumpTime >= 0.0f)
            {
                colliderJumping = ScenarioTime - colliderMech->JumpTime < 0.5f ? 1 : 0;
            }

            [[fallthrough]];
        }

        case GROUNDVEHICLE:
        {
            bool jumpHit = true;

            if (collideeJumping == 0)
            {
                auto* collideeMech = static_cast<MCBattleMech*>(collidee);
                bool landing = false;

                if (collideeMech->JumpTime >= 0.0f)
                {
                    const float sinceJump = ScenarioTime - collideeMech->JumpTime;
                    collideeMech->JumpTime = -1.0f;

                    if (sinceJump < 0.5f)
                    {
                        collideeJumping = 1;
                        landing = true;
                    }
                    else
                    {
                        collideeJumping = 0;
                    }
                }

                if (!landing)
                {
                    jumpHit = colliderJumping != 0;
                }
            }

            if (collidee->GetPilot()->Alignment == collider->GetPilot()->Alignment)
            {
                friendly = 1;

                if (!jumpHit)
                {
                    return 0;
                }
            }
            else if (!jumpHit)
            {
                MCGameObject* collideeRamTarget = collidee->GetPilot()->CurTacOrder.GetRamTarget();
                MCGameObject* colliderRamTarget = collider->GetPilot()->CurTacOrder.GetRamTarget();

                if (collideeRamTarget != collider && colliderRamTarget != collidee)
                {
                    return 0;
                }
            }

            if (collidee->GetCollisionFreeFrom() == collider && ScenarioTime <= collidee->GetCollisionFreeTime())
            {
                return 0;
            }

            collidee->SetCollisionFreeFrom(collider);
            collidee->SetCollisionFreeTime(ScenarioTime + 2.0f);

            MCFrameOfRef frame = collidee->GetFrame();
            RotateAboutK(frame, static_cast<float>(std::sin(HALF_PI)), static_cast<float>(std::cos(HALF_PI)));
            collidee->SetFrame(frame);
            collidee->GetVelocity();

            if (jumpHit)
            {
                if (collideeJumping != 0)
                {
                    // The jumper lands on the other: both take the other's weight (the collider's, twice).
                    const int32_t hitLocation = collidee->CalcHitLocation(collider, -1, 3, 0);
                    const float entryAngle = collidee->RelFacingTo(collider->GetPosition(), -1);
                    MCWeaponShotInfo shotInfo;
                    shotInfo.Init(collider, -1,
                                  static_cast<float>(collider->GetTonnage() * (friendly == 0 ? 0.1 : 0.01) + 0.5),
                                  hitLocation, entryAngle);
                    collidee->HandleWeaponHit(&shotInfo, MPlayer != nullptr);
                    const int32_t otherHitLocation = collider->CalcHitLocation(collidee, -1, 2, 0);
                    const float otherEntryAngle = collider->RelFacingTo(collidee->GetPosition(), -1);
                    shotInfo.Init(collidee, -1,
                                  static_cast<float>(collider->GetTonnage() * (friendly == 0 ? 0.1 : 0.01) + 0.5),
                                  otherHitLocation, otherEntryAngle);
                    collider->HandleWeaponHit(&shotInfo, MPlayer != nullptr);
                    MCVector3D position = collider->GetPosition();
                    ::CreateExplosion(0x290, position, 0.0f, 0.0f);
                }
            }
            else
            {
                CollisionHit(collidee, collider, collider, 1, friendly);
            }

            static_cast<MCMover*>(collidee)->BounceToAdjCell();

            if (friendly != 0)
            {
                return 0;
            }
            break;
        }

        case ELEMENTAL:
        {
            if (collidee->GetPilot()->Alignment == collider->GetPilot()->Alignment)
            {
                return 0;
            }

            MCGameObject* collideeRamTarget = collidee->GetPilot()->CurTacOrder.GetRamTarget();
            MCGameObject* colliderRamTarget = collider->GetPilot()->CurTacOrder.GetRamTarget();

            if (collideeRamTarget != collider && colliderRamTarget != collidee)
            {
                return 0;
            }

            if (collidee->GetCollisionFreeFrom() == collider && ScenarioTime <= collidee->GetCollisionFreeTime())
            {
                return 0;
            }

            if (collider->IsMarine() != 0)
            {
                return 0;
            }

            collidee->SetCollisionFreeFrom(collider);
            collidee->SetCollisionFreeTime(ScenarioTime + 2.0f);
            collidee->GetVelocity();
            const int32_t hitLocation = collidee->CalcHitLocation(collider, -1, 1, 0);
            const float entryAngle = collidee->RelFacingTo(collider->GetPosition(), -1);
            MCWeaponShotInfo shotInfo;
            shotInfo.Init(collider, -1, ElmDamageOnImpact, hitLocation, entryAngle);
            collidee->HandleWeaponHit(&shotInfo, MPlayer != nullptr);
            sampleId = 0x1e;
            break;
        }

        case BUILDING:
        case TREEBUILDING:
        {
            if (collidee->GetCollisionFreeFrom() == collider && ScenarioTime <= collidee->GetCollisionFreeTime())
            {
                return 0;
            }

            collidee->SetCollisionFreeFrom(collider);
            collidee->SetCollisionFreeTime(ScenarioTime + 2.0f);
            const MCVector3D velocity = collidee->GetVelocity();

            const double speed = std::sqrt(
                (static_cast<double>(velocity.X) * velocity.X + static_cast<double>(velocity.Z) * velocity.Z) +
                static_cast<double>(velocity.Y) * velocity.Y);

            if (!(speed > MechCollisionThreshold))
            {
                static_cast<MCMover*>(collidee)->BounceToAdjCell();
            }

            const int32_t hitLocation = collidee->CalcHitLocation(collider, -1, 1, 0);
            const float entryAngle = collidee->RelFacingTo(collider->GetPosition(), -1);
            MCWeaponShotInfo shotInfo;
            shotInfo.Init(collider, -1, static_cast<float>(collider->GetTonnage() * 0.1 + 0.5), hitLocation,
                          entryAngle);
            collidee->HandleWeaponHit(&shotInfo, MPlayer != nullptr);
            collider->HandleWeaponHit(&shotInfo, MPlayer != nullptr);
            break;
        }

        case TREE:
        {
            if (collidee->GetCollisionFreeFrom() == collider && ScenarioTime <= collidee->GetCollisionFreeTime())
            {
                return 0;
            }

            collidee->SetCollisionFreeFrom(collider);
            collidee->SetCollisionFreeTime(ScenarioTime + 2.0f);
            MCFrameOfRef frame = collidee->GetFrame();
            collider->GetObjectType();
            double deflection = 0.0;

            if (TonnageClass < TonnageCollisionThreshold)
            {
                deflection = static_cast<double>(TonnageCollisionThreshold) / TonnageClass * TreeDeflection;
            }

            if (deflection > 0.0)
            {
                RotateAboutKUnroundedCos(frame, static_cast<float>(std::sin(deflection * DEGREES_TO_RADIANS)),
                                         std::cos(deflection * DEGREES_TO_RADIANS));
                collidee->SetFrame(frame);
            }
            break;
        }

        case TRAINCAR:
        {
            if (collidee->GetCollisionFreeFrom() == collider && ScenarioTime <= collidee->GetCollisionFreeTime())
            {
                return 0;
            }

            collidee->SetCollisionFreeFrom(collider);
            collidee->SetCollisionFreeTime(ScenarioTime + 2.0f);
            MCFrameOfRef frame = collidee->GetFrame();
            RotateAboutK(frame, static_cast<float>(std::sin(HALF_PI)), static_cast<float>(std::cos(HALF_PI)));
            collidee->SetFrame(frame);
            collidee->GetVelocity();
            static_cast<MCMover*>(collidee)->BounceToAdjCell();
            break;
        }

        default:
            return 0;
    }

    SoundSystem->PlayDigitalSample(sampleId, 1, collidee, 0, 0);
    return 0;
}

auto MCBattleMechType::HandleDestruction(MCGameObject* collidee, MCGameObject* collider) -> int
{
    auto* mech = static_cast<MCBattleMech*>(collidee);

    if (mech->GetPilot() == nullptr)
    {
        Fatal(0, " No Pilot in this mech! ");
    }

    if (mech->GetPoint() == mech)
    {
        mech->Group->SetPoint(nullptr);
    }

    if (mech->SensorSystem != nullptr)
    {
        mech->SensorSystem->Disable();
    }

    mech->DeathTimer = 0.8f;

    if (mech->Withdrawing != 0)
    {
        mech->GetPilot()->HandleAlarm(8, 0);
        TheInterface->RemoveMech(mech->PartId);
        return 1;
    }

    mech->GetPilot()->HandleAlarm(7, collider == nullptr ? 0 : collider->IdNumber);
    mech->Status = 2;
    mech->LyingDead = 0;
    mech->DeathExplosionDone = 0;

    for (int32_t location = 0; location < mech->NumBodyLocations; location++)
    {
        mech->DestroyBodyLocation(location);
    }

    if (mech->GetAlignment() == HomeTeam->Alignment)
    {
        FriendlyDestroyed = 1;
        return 1;
    }

    EnemyDestroyed = 1;
    return 1;
}

auto MCBattleMechType::LoadHotSpots(MCFitIniFile* mechFile) -> int32_t
{
    if (mechFile == nullptr)
    {
        return 0;
    }

    int32_t result = mechFile->SeekBlock("HotSpots");

    if (result != 0)
    {
        return result;
    }

    char hotSpotFileName[80];

    if ((result = mechFile->ReadIdString("HotSpotFileName", hotSpotFileName, 79)) != 0)
    {
        return result;
    }

    int32_t footprint = 0;

    if ((result = mechFile->ReadIdLong("FootprintType", footprint)) != 0)
    {
        return result;
    }

    FootprintType = footprint;

    std::string hotSpotPath;
    hotSpotPath = GamePath(ShapesPath, hotSpotFileName, ".hsp");
    std::string outlinePath;
    outlinePath = GamePath(ShapesPath, hotSpotFileName, ".out");
    std::string infoPath;
    infoPath = GamePath(ShapesPath, hotSpotFileName, ".inf");
    std::string jumpPath;
    jumpPath = GamePath(ShapesPath, hotSpotFileName, ".jmp");

    MCPacketFile hotSpotFile;

    if ((result = hotSpotFile.Open(hotSpotPath)) != 0)
    {
        return result;
    }

    MCPacketFile outlineFile;

    if ((result = outlineFile.Open(outlinePath)) != 0)
    {
        return result;
    }

    MCFitIniFile infoFile;

    if ((result = infoFile.Open(infoPath)) != 0)
    {
        return result;
    }

    MCFile jumpFile;

    if ((result = jumpFile.Open(jumpPath)) != 0)
    {
        return result;
    }

    if ((result = infoFile.SeekBlock("Info")) != 0)
    {
        return result;
    }

    if ((result = infoFile.ReadIdULong("numHotSpotPackets", NumHotSpotPackets)) != 0)
    {
        return result;
    }

    if ((result = infoFile.ReadIdULong("numWeapons", NumWeapons)) != 0)
    {
        return result;
    }

    if ((result = infoFile.ReadIdULong("numOthers", NumOthers)) != 0)
    {
        return result;
    }

    const uint32_t weaponCount = NumWeapons;
    WeaponHotSpots =
        static_cast<uint32_t*>(MCObjectTypeManager::ObjectTypeCache.Allocate(weaponCount * sizeof(uint32_t)));

    if (WeaponHotSpots == nullptr)
    {
        return -0x5fff4;
    }

    for (int32_t weapon = 0; weapon < static_cast<int32_t>(weaponCount); weapon++)
    {
        char entryName[20];
        std::sprintf(entryName, "weapon%d", weapon);

        if ((result = mechFile->ReadIdULong(entryName, WeaponHotSpots[weapon])) != 0)
        {
            return result;
        }
    }

    const uint32_t hotSpotDataSize = NumHotSpotPackets * 32;
    HotSpotData = static_cast<uint8_t*>(MCObjectTypeManager::ObjectTypeCache.Allocate(hotSpotDataSize));

    if (HotSpotData == nullptr)
    {
        return -0x5fff5;
    }

    std::memset(HotSpotData, 0, hotSpotDataSize);
    const int32_t dataPacket = static_cast<int32_t>(NumHotSpotPackets);

    if (hotSpotFile.SeekPacket(dataPacket) == 0)
    {
        if (static_cast<uint32_t>(hotSpotFile.GetPacketSize()) != hotSpotDataSize)
        {
            return -0x5fff3;
        }

        hotSpotFile.ReadPacket(dataPacket, HotSpotData);
    }

    // Port fix: pointer tables sized by the pointer, not the original's 4 bytes.
    const size_t tableSize = (static_cast<size_t>(dataPacket) + 1) * sizeof(uint8_t*);
    GestureHotSpots =
        static_cast<uint8_t**>(MCObjectTypeManager::ObjectTypeCache.Allocate(static_cast<uint32_t>(tableSize)));

    if (GestureHotSpots == nullptr)
    {
        return -0x5fff4;
    }

    std::memset(GestureHotSpots, 0, tableSize);
    const size_t outlineTableSize = (static_cast<size_t>(NumHotSpotPackets) + 1) * sizeof(uint8_t*);
    GestureOutlines =
        static_cast<uint8_t**>(MCObjectTypeManager::ObjectTypeCache.Allocate(static_cast<uint32_t>(outlineTableSize)));

    if (GestureOutlines == nullptr)
    {
        return -0x5fff1;
    }

    std::memset(GestureOutlines, 0, outlineTableSize);

    const int32_t numGestures = static_cast<int32_t>(NumHotSpotPackets);
    NumFramesPerHotSpot =
        static_cast<uint32_t*>(MCObjectTypeManager::ObjectTypeCache.Allocate((numGestures + 1) * sizeof(uint32_t)));
    std::vector<uint32_t> packetSizes(static_cast<size_t>(numGestures), 0);
    std::vector<uint32_t> outlineSizes(static_cast<size_t>(numGestures), 0);

    for (int32_t gesture = 0; gesture < numGestures; gesture++)
    {
        char blockName[20];
        std::sprintf(blockName, "Gesture%d", gesture);

        if ((result = infoFile.SeekBlock(blockName)) != 0 ||
            (result = infoFile.ReadIdULong("numFramesPerHotSpot", NumFramesPerHotSpot[gesture])) != 0)
        {
            return result;
        }

        if (hotSpotFile.SeekPacket(gesture) != 0)
        {
            return -0x5fff2;
        }

        GestureHotSpots[gesture] =
            static_cast<uint8_t*>(MCObjectTypeManager::ObjectTypeCache.Allocate(hotSpotFile.GetPacketSize()));

        if (GestureHotSpots[gesture] == nullptr)
        {
            return -0x5fff4;
        }

        hotSpotFile.ReadPacket(gesture, GestureHotSpots[gesture]);
        packetSizes[gesture] = static_cast<uint32_t>(hotSpotFile.GetPacketSize());

        if (outlineFile.SeekPacket(gesture) == 0 && outlineFile.GetPacketSize() != 0)
        {
            GestureOutlines[gesture] =
                static_cast<uint8_t*>(MCObjectTypeManager::ObjectTypeCache.Allocate(outlineFile.GetPacketSize()));

            if (GestureOutlines[gesture] == nullptr)
            {
                return -0x5fff1;
            }

            outlineFile.ReadPacket(gesture, GestureOutlines[gesture]);
            outlineSizes[gesture] = static_cast<uint32_t>(outlineFile.GetPacketSize());
        }
    }

    LayOutHotSpotPackets(packetSizes, outlineSizes);

    JumpData = static_cast<uint8_t*>(MCObjectTypeManager::ObjectTypeCache.Allocate(jumpFile.FileSize()));

    if (JumpData == nullptr)
    {
        return -0x5fff4;
    }

    std::memset(JumpData, 0, jumpFile.FileSize());
    jumpFile.Read(JumpData, static_cast<int32_t>(jumpFile.FileSize()));
    return 0;
}

auto MCBattleMechType::CreateInstance() -> MCBaseObject*
{
    auto* newMech = new MCBattleMech;

    if (newMech == nullptr)
    {
        return nullptr;
    }

    if (newMech->Init(this) != 0)
    {
        return nullptr;
    }

    newMech->IdNumber = NextIdNumber++;
    return newMech;
}

//---------------------------------------------------------------------------
// BattleMech
//---------------------------------------------------------------------------

auto MCBattleMech::IsCrippled() -> int
{
    return LegStatus == 2 || LegStatus == 3 ? 1 : 0;
}

auto MCBattleMech::GetWeaponHeat(int32_t weaponIndex) -> float
{
    return MasterComponentList[Inventory[weaponIndex].MasterID].RangeOrHeat;
}

auto MCBattleMech::RelViewFacingTo(MCVector3D goal) -> float
{
    return RelFacingTo(goal, -1);
}

auto MCBattleMech::CanMove() -> int
{
    return LegStatus != 3 ? 1 : 0;
}

auto MCBattleMech::CanJump() -> int
{
    return NumJumpJets != 0 ? 1 : 0;
}

auto MCBattleMech::HandleStaticCollision() -> void
{
    const bool jumpFXOn =
        static_cast<MCMechActor*>(Appearance)->CurrentGesture != 0x14 && (JumpFX[0] != nullptr || JumpFX[1] != nullptr);

    if (!((CollisionsOn != 0 &&
           std::sqrt(Velocity.Z * Velocity.Z + Velocity.Y * Velocity.Y + Velocity.X * Velocity.X) > 0.0f) ||
          jumpFXOn))
    {
        return;
    }

    int32_t blockNumber = 0;
    int32_t vertexNumber = 0;
    GetBlockAndVertexNumber(blockNumber, vertexNumber);
    char listName[12];
    std::sprintf(listName, "TBlk%d", blockNumber);
    MCObjectQueueNode* list = ObjectList->Head;

    while (list != nullptr && list->operator==(listName) == 0)
    {
        list = list->Next;
    }

    // Port fix: the original reads the objects of a missing list through null.
    if (list == nullptr)
    {
        return;
    }

    for (MCBaseObject* object = list->Head; object != nullptr; object = object->Next)
    {
        auto* other = static_cast<MCGameObject*>(object);

        if (other->GetObjectType() == nullptr)
        {
            continue;
        }

        int collides = 0;
        int32_t otherBlock = -1;
        int32_t otherVertex = -1;

        switch (other->ObjectClass)
        {
            case BUILDING:
            case TREE:
            case TERRAINOBJECT:
            case TREEBUILDING:
            {
                other->GetBlockAndVertexNumber(otherBlock, otherVertex);
                collides = other->CollisionsOn;
                break;
            }
            case MISCTERRAINOBJECT:
            {
                GetBlockAndVertexNumber(otherBlock, otherVertex);

                if (static_cast<uint32_t>(static_cast<MCMiscTerrainObject*>(other)->TerrainObjectKind) > 6)
                {
                    collides = 1;
                }
                break;
            }
            default:
                break;
        }

        if (vertexNumber == otherVertex && collides != 0)
        {
            CollisionSystem->DetectStaticCollision(this, other);
        }
    }
}

auto MCBattleMech::Init() -> void
{
    ObjectClass = BATTLEMECH;
    Body = std::make_unique<MCBodyLocation[]>(8);
    NumBodyLocations = 8;

    for (int32_t location = 0; location < 8; location++)
    {
        BodyAt(location).HasCase = 0;
        BodyAt(location).TotalSpaces = 0;
        BodyAt(location).CriticalSpaces = nullptr;
        BodyAt(location).CurInternalStructure = 0.0f;
        BodyAt(location).HotSpotNumber = 0;
        BodyAt(location).MaxInternalStructure = 0;
        BodyAt(location).DamageState = 0;
    }

    Armor = std::make_unique<MCArmorLocation[]>(11);
    NumArmorLocations = 11;
    MechClass = 1;
    LegStatus = 0;
    TorsoStatus = 0;
    NumJumpJets = 0;
    JumpTime = -100.0f;
    InJump = 0;
    JumpGoal = MCVector3D(0.0f, 0.0f, 0.0f);
    CenterTorsoInjuredTime = -1.0f;
    HitFromBehindThisFrame = 0;
    HitFromFrontThisFrame = 0;
    TorsoRotation = 0.0f;
    LeftArmRotation = 0.0f;
    RightArmRotation = 0.0f;
    LeftArmBlownThisFrame = 0;
    RightArmBlownThisFrame = 0;
    SecondStepPrinted = 0;
    FirstStepPrinted = 0;
    LyingDead = 0;
    WreckDone = 0;
    StatusWindow = nullptr;
    BlipFrame = 0;
    OverlayWeightClass = 1;
    Captureable = 0;
    SteppedOnMine = 0;
}

auto MCBattleMech::Init(MCObjectType* objType) -> int32_t
{
    int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    auto* mechType = static_cast<MCBattleMechType*>(objType);
    CollisionsOn = 1;

    for (int32_t location = 0; location < 8; location++)
    {
        BodyAt(location).MaxInternalStructure = mechType->InternalStructure[location];
    }

    Chassis = mechType->Chassis;
    Alignment = mechType->MechType;
    EndoSteel = static_cast<int32_t>(mechType->EndoSteel);
    InternalStructureTonnage = mechType->InternalStructureTonnage;
    TonnageClass = mechType->TonnageClass;
    CrashAvoidSelf = mechType->CrashAvoidSelf;
    PathLockLevel = mechType->CrashBlockSelf;
    CrashAvoidPath = mechType->CrashAvoidPath;
    PathLockRange = mechType->CrashBlockPath;
    CrashYieldTime = mechType->CrashYieldTime;
    Control = nullptr;
    Dynamics = mechType->DynamicsType->CreateInstance();

    if (Dynamics == nullptr)
    {
        return -0x5fff8;
    }

    if ((result = Dynamics->Init(mechType->DynamicsType, this)) != 0)
    {
        return result;
    }

    MCAppearanceType* apprType = AppearanceTypeList()->GetAppearance(mechType->AppearName);

    if (apprType == nullptr)
    {
        return -0x5fff7;
    }

    auto* actor = new MCMechActor;
    Appearance = actor;

    if (actor == nullptr)
    {
        return -0x5ffff;
    }

    actor->OwnerMech = this;

    if ((apprType->AppearanceNum & 0xff000000) != 0x1000000)
    {
        return -0x5fff6;
    }

    if ((result = actor->Init(apprType, this)) != 0)
    {
        return result;
    }

    ObjectClass = BATTLEMECH;

    for (int32_t i = 0; i < 4; i++)
    {
        Smoke[i] = nullptr;
        SmokeHotSpot[i] = 0;
        SmokeTime[i] = 0.0f;
    }

    JumpFX[1] = nullptr;
    JumpFX[0] = nullptr;
    DistanceSinceMarkSeen = 1000.0f;
    return 0;
}

auto MCBattleMech::SetControl(uint32_t controlType, uint32_t controlData, int32_t controlParam) -> int32_t
{
    int32_t result = 0;

    switch (controlType)
    {
        case 1:
        {
            delete Control;
            auto* playerControl = new MCPlayerControl;
            Control = playerControl;

            if (playerControl == nullptr)
            {
                return -0x5fffc;
            }

            if ((result = playerControl->Init(this, 0)) != 0)
            {
                return result;
            }
            break;
        }

        case 2:
        {
            delete Control;
            auto* aiControl = new MCMechAIControl;
            Control = aiControl;

            if (aiControl == nullptr)
            {
                return -0x5fffc;
            }

            if ((result = aiControl->Init(this)) != 0)
            {
                return result;
            }
            break;
        }

        case 3:
        {
            delete Control;
            auto* netControl = new MCMechNetControl;
            Control = netControl;

            if (netControl == nullptr)
            {
                return -0x5fffc;
            }

            if ((result = netControl->Init(this)) != 0)
            {
                return result;
            }
            break;
        }

        default:
            return -0x5fffb;
    }

    if (controlData != 1 && controlData != 0xffffffff)
    {
        return -0x5fff9;
    }

    auto* mechControlData = new MCMechControlData;
    Control->ControlData = mechControlData;

    if (mechControlData == nullptr)
    {
        return -0x5fffa;
    }

    return mechControlData->Init(0);
}

auto MCBattleMech::Init(MCFitIniFile* mechFile) -> int32_t
{
    static const char* const bodyLocationNames[NUM_MECH_BODY_LOCATIONS] = {
        "Head", "CenterTorso", "LeftTorso", "RightTorso", "LeftArm", "RightArm", "LeftLeg", "RightLeg"};
    static const char* const armorLocationNames[NUM_MECH_ARMOR_LOCATIONS] = {
        "Head",    "CenterTorso", "LeftTorso",       "RightTorso",    "LeftArm",       "RightArm",
        "LeftLeg", "RightLeg",    "RearCenterTorso", "RearLeftTorso", "RearRightTorso"};

    int32_t result = mechFile->SeekBlock("Header");

    if (result != 0)
    {
        return result;
    }

    char fileType[128];

    if ((result = mechFile->ReadIdString("FileType", fileType, 127)) != 0)
    {
        return result;
    }

    if (std::strcmp(fileType, "MechProfile") != 0)
    {
        return -1;
    }

    if ((result = mechFile->SeekBlock("General")) != 0)
    {
        return result;
    }

    char nameBuffer[128];
    mechFile->ReadIdString("Name", nameBuffer, 127);
    DebugStatus = nameBuffer;
    if (mechFile->ReadIdLong("ChassisBR", ChassisBR) != 0)
    {
        ChassisBR = 100;
    }

    if ((result = mechFile->ReadIdFloat("CurTonnage", Tonnage)) != 0)
    {
        return result;
    }

    if (mechFile->ReadIdLong("DescIndex", DescIndex) != 0)
    {
        DescIndex = -1;
    }

    char ifaceNameBuffer[256];
    CLoadString(ThisInstance, DescIndex + 300, ifaceNameBuffer, 0xfe);
    IfaceName = ifaceNameBuffer;

    if ((result = mechFile->ReadIdLong("NameIndex", NameIndex)) != 0)
    {
        return result;
    }

    if ((result = mechFile->ReadIdLong("NameVariant", NameVariant)) != 0)
    {
        return result;
    }

    if (mechFile->ReadIdLong("Pilot", PilotId) != 0)
    {
        PilotId = -1;
    }

    Status = 0;

    if ((result = mechFile->ReadIdString("icon", IconName, 0x13)) != 0)
    {
        return result;
    }

    if (mechFile->ReadIdBoolean("NotMineYet", NotMineYet) != 0)
    {
        NotMineYet = 1;
    }

    if ((result = mechFile->SeekBlock("Engine")) != 0)
    {
        return result;
    }

    if ((result = mechFile->ReadIdFloat("Tonnage", EngineTonnage)) != 0)
    {
        return result;
    }

    if ((result = mechFile->ReadIdULong("Rating", EngineRating)) != 0)
    {
        return result;
    }

    uint8_t runSpeed = 0;

    if ((result = mechFile->ReadIdUChar("MaxRunSpeed", runSpeed)) != 0)
    {
        return result;
    }

    MaxRunSpeed = static_cast<float>(runSpeed);

    if (mechFile->SeekBlock("MovementSystem") == 0)
    {
        int32_t value = 0;

        if (mechFile->ReadIdLong("CrashAvoidSelf", value) == 0)
        {
            CrashAvoidSelf = value;
        }

        if (mechFile->ReadIdLong("CrashAvoidPath", value) == 0)
        {
            CrashAvoidPath = value;
        }

        if (mechFile->ReadIdLong("CrashBlockSelf", value) == 0)
        {
            PathLockLevel = value;
        }

        if (mechFile->ReadIdLong("CrashBlockPath", value) == 0)
        {
            PathLockRange = value;
        }

        float yieldTime = 0.0f;

        // As BattleMechType::init: the last long read is stored, not the yield time.
        if (mechFile->ReadIdFloat("CrashYieldTime", yieldTime) == 0)
        {
            CrashYieldTime = static_cast<float>(value);
        }
    }

    if ((result = mechFile->SeekBlock("Armor")) != 0)
    {
        return result;
    }

    if ((result = mechFile->ReadIdUChar("Type", ArmorType)) != 0)
    {
        return result;
    }

    if ((result = mechFile->ReadIdFloat("Tonnage", ArmorTonnage)) != 0)
    {
        return result;
    }

    if ((result = mechFile->SeekBlock("MaxArmorPoints")) != 0)
    {
        return result;
    }

    for (int32_t location = 0; location < NUM_MECH_ARMOR_LOCATIONS; location++)
    {
        if ((result = mechFile->ReadIdUChar(armorLocationNames[location], Armor[location].MaxArmor)) != 0)
        {
            return result;
        }
    }

    if ((result = mechFile->SeekBlock("CurArmorPoints")) != 0)
    {
        return result;
    }

    for (int32_t location = 0; location < NUM_MECH_ARMOR_LOCATIONS; location++)
    {
        uint8_t points = 0;

        if ((result = mechFile->ReadIdUChar(armorLocationNames[location], points)) != 0)
        {
            return result;
        }

        Armor[location].CurArmor = static_cast<float>(points);
    }

    if ((result = mechFile->SeekBlock("InventoryInfo")) != 0)
    {
        return result;
    }

    if ((result = mechFile->ReadIdUChar("NumOther", NumOther)) != 0)
    {
        return result;
    }

    if ((result = mechFile->ReadIdUChar("NumWeapons", NumWeapons)) != 0)
    {
        return result;
    }

    if ((result = mechFile->ReadIdUChar("NumAmmo", NumAmmos)) != 0)
    {
        return result;
    }

    const int32_t firstWeapon = NumOther;
    const int32_t firstAmmo = NumOther + NumWeapons;
    const int32_t numItems = NumAmmos + NumOther + NumWeapons;
    Inventory = std::make_unique<MCInventoryItem[]>(static_cast<size_t>(numItems));

    NumAntiMissileSystems = 0;
    char blockName[32];

    for (int32_t item = 0; item < firstWeapon; item++)
    {
        std::sprintf(blockName, "Item:%d", item);

        if ((result = mechFile->SeekBlock(blockName)) != 0)
        {
            return result;
        }

        MCInventoryItem& other = Inventory[item];

        if ((result = mechFile->ReadIdUChar("MasterID", other.MasterID)) != 0)
        {
            return result;
        }

        other.Health = MasterComponentList[other.MasterID].Health;
        other.Disabled = 0;
        other.Amount = 1;
        other.AmmoIndex = -1;
        other.ReadyTime = 0.0f;
        other.BodyLocation = 0xff;
        other.RangeRatings = nullptr;

        if (MasterComponentList[other.MasterID].Form == COMPONENT_FORM_JUMPJET)
        {
            NumJumpJets++;
        }
    }

    for (int32_t item = firstWeapon; item < firstAmmo; item++)
    {
        std::sprintf(blockName, "Item:%d", item);

        if ((result = mechFile->SeekBlock(blockName)) != 0)
        {
            return result;
        }

        MCInventoryItem& weapon = Inventory[item];

        if ((result = mechFile->ReadIdUChar("MasterID", weapon.MasterID)) != 0)
        {
            return result;
        }

        if ((result = mechFile->ReadIdUChar("FacesForward", weapon.FacesForward)) != 0)
        {
            return result;
        }

        const MCMasterComponent& component = MasterComponentList[weapon.MasterID];
        weapon.Health = component.Health;
        weapon.Disabled = 0;
        weapon.Amount = 1;
        weapon.AmmoIndex = -1;
        weapon.ReadyTime = 0.0f;
        weapon.BodyLocation = 0xff;
        // Damage per ten seconds, then scaled by the long range over 24.
        weapon.Effectiveness =
            static_cast<int16_t>(static_cast<int32_t>(component.Damage * 10.0 / component.RecycleTime));
        weapon.Effectiveness = static_cast<int16_t>(static_cast<int32_t>(
            static_cast<double>(component.WeaponRange[3]) * weapon.Effectiveness * static_cast<double>(1.0f / 24.0f)));
        weapon.RangeRatings = new float[NumRangeRatings * 2]();
        ObjectTypeManager->Load(
            static_cast<int32_t>(
                WeaponFXTable[static_cast<int8_t>(MasterComponentList[Inventory[item].MasterID].WeaponEffect)]),
            1);
    }

    for (int32_t item = firstAmmo; item < numItems; item++)
    {
        std::sprintf(blockName, "Item:%d", item);

        if ((result = mechFile->SeekBlock(blockName)) != 0)
        {
            return result;
        }

        MCInventoryItem& ammo = Inventory[item];

        if ((result = mechFile->ReadIdUChar("MasterID", ammo.MasterID)) != 0)
        {
            return result;
        }

        int32_t amount = 0;

        if (mechFile->ReadIdLong("Amount", amount) != 0)
        {
            uint8_t smallAmount = 0;

            if ((result = mechFile->ReadIdUChar("Amount", smallAmount)) != 0)
            {
                return result;
            }

            amount = smallAmount;
        }

        if (amount == -1)
        {
            amount = MasterComponentList[ammo.MasterID].LongValue;
        }

        ammo.Amount = static_cast<int16_t>(amount);
        ammo.AmmoIndex = -1;
        ammo.StartAmount = ammo.Amount;
        ammo.Health = MasterComponentList[ammo.MasterID].Health;
        ammo.Disabled = 0;
        ammo.ReadyTime = 0.0f;
        ammo.BodyLocation = 0xff;
        ammo.RangeRatings = nullptr;
    }

    for (int32_t location = 0; location < NUM_MECH_BODY_LOCATIONS; location++)
    {
        if ((result = mechFile->SeekBlock(bodyLocationNames[location])) != 0)
        {
            return result;
        }

        uint8_t hasCase = 0;

        if ((result = mechFile->ReadIdUChar("CASE", hasCase)) != 0)
        {
            return result;
        }

        MCBodyLocation& bodyLocation = BodyAt(location);
        bodyLocation.HasCase = hasCase;
        uint8_t internalStructure = 0;

        if ((result = mechFile->ReadIdUChar("CurInternalStructure", internalStructure)) != 0)
        {
            return result;
        }

        bodyLocation.CurInternalStructure = static_cast<float>(internalStructure);

        if ((result = mechFile->ReadIdUChar("HotSpotNumber", bodyLocation.HotSpotNumber)) != 0)
        {
            return result;
        }

        const float structureLeft =
            bodyLocation.CurInternalStructure / static_cast<float>(bodyLocation.MaxInternalStructure);

        if (structureLeft == 0.0f)
        {
            bodyLocation.DamageState = 2;
        }
        else if (structureLeft > 0.5f)
        {
            bodyLocation.DamageState = 0;
        }
        else
        {
            bodyLocation.DamageState = 1;
        }

        const int32_t numSpaces = NumLocationCriticalSpaces[location];
        bodyLocation.CriticalSpaces = new MCCriticalSpace[static_cast<size_t>(numSpaces)]();
        bodyLocation.TotalSpaces = 0;

        for (int32_t space = 0; space < numSpaces; space++)
        {
            char entryName[32];
            std::sprintf(entryName, "Component:%d", space);
            uint8_t entry[2];

            if ((result = mechFile->ReadIdUCharArray(entryName, entry, 2)) != 0)
            {
                return result;
            }

            MCCriticalSpace& criticalSpace = BodyAt(location).CriticalSpaces[space];
            criticalSpace.InventoryID = entry[0];
            criticalSpace.Hit = entry[1];

            if (entry[0] == 0xff)
            {
                continue;
            }

            MCInventoryItem& item = Inventory[entry[0]];
            item.BodyLocation = static_cast<uint8_t>(location);
            BodyAt(location).TotalSpaces += static_cast<int8_t>(MasterComponentList[item.MasterID].CriticalSpacesReq);
            const uint32_t masterID = item.MasterID;

            switch (MasterComponentList[masterID].Form)
            {
                case COMPONENT_FORM_COCKPIT:
                    Cockpit = entry[0];
                    break;
                case COMPONENT_FORM_SENSOR:
                {
                    Sensor = entry[0];
                    SensorSystem = SensorSystemManager->NewSensor();
                    SensorSystem->Owner = this;
                    SensorSystem->SetRange(MasterComponentList[Inventory[Sensor].MasterID].RangeOrHeat);
                    break;
                }
                case COMPONENT_FORM_ACTUATOR:
                {
                    if (static_cast<int32_t>(masterID) == MasterArmActuatorID)
                    {
                        if (location == MECH_BODY_LOCATION_LARM)
                        {
                            LeftArmActuator = entry[0];
                        }
                        else if (location == MECH_BODY_LOCATION_RARM)
                        {
                            RightArmActuator = entry[0];
                        }
                    }
                    else if (static_cast<int32_t>(masterID) == MasterLegActuatorID)
                    {
                        if (location == MECH_BODY_LOCATION_LLEG)
                        {
                            LeftLegActuator = entry[0];
                        }
                        else if (location == MECH_BODY_LOCATION_RLEG)
                        {
                            RightLegActuator = entry[0];
                        }
                    }
                    break;
                }
                case COMPONENT_FORM_ENGINE:
                    Engine = entry[0];
                    break;
                case COMPONENT_FORM_HEATSINK:
                case COMPONENT_FORM_WEAPON:
                case COMPONENT_FORM_WEAPON_ENERGY:
                case COMPONENT_FORM_WEAPON_MISSILE:
                    item.BodyLocation = static_cast<uint8_t>(location);
                    break;
                case COMPONENT_FORM_WEAPON_BALLISTIC:
                {
                    item.BodyLocation = static_cast<uint8_t>(location);

                    if (static_cast<int32_t>(masterID) == MasterClanAntiMissileSystemID ||
                        static_cast<int32_t>(masterID) == MasterInnerSphereAntiMissileSystemID)
                    {
                        if (NumAntiMissileSystems == 16)
                        {
                            Fatal(0, "Too many Anti-Missile Systems");
                        }

                        AntiMissileSystem[NumAntiMissileSystems] = entry[0];
                        NumAntiMissileSystems++;
                    }
                    break;
                }
                case COMPONENT_FORM_AMMO:
                    item.BodyLocation = static_cast<uint8_t>(location);
                    break;
                case COMPONENT_FORM_LIFESUPPORT:
                    LifeSupport = entry[0];
                    break;
                case COMPONENT_FORM_GYROSCOPE:
                    Gyro = entry[0];
                    break;
                case COMPONENT_FORM_ECM:
                    Ecm = entry[0];
                    break;
                case COMPONENT_FORM_PROBE:
                    Probe = entry[0];
                    break;
                case COMPONENT_FORM_JAMMER:
                    Jammer = entry[0];
                    break;
                default:
                    break;
            }
        }
    }

    CalcAmmoTotals();

    for (int32_t item = firstWeapon; item < firstAmmo; item++)
    {
        for (int32_t ammoType = 0; ammoType < NumAmmoTypes; ammoType++)
        {
            if (static_cast<int32_t>(MasterComponentList[Inventory[item].MasterID].AmmoMasterId) ==
                AmmoTypeTotal[ammoType].MasterId)
            {
                Inventory[item].AmmoIndex = static_cast<int16_t>(ammoType);
                break;
            }
        }
    }

    for (int32_t item = firstAmmo; item < numItems; item++)
    {
        for (int32_t ammoType = 0; ammoType < NumAmmoTypes; ammoType++)
        {
            if (static_cast<int32_t>(Inventory[item].MasterID) == AmmoTypeTotal[ammoType].MasterId)
            {
                Inventory[item].AmmoIndex = static_cast<int16_t>(ammoType);
                break;
            }
        }
    }

    for (int32_t item = 0; item < firstWeapon; item++)
    {
        const int32_t masterID = Inventory[item].MasterID;

        if (masterID != MasterClanAntiMissileSystemID && masterID != MasterInnerSphereAntiMissileSystemID)
        {
            continue;
        }

        for (int32_t ammoType = 0; ammoType < NumAmmoTypes; ammoType++)
        {
            if (static_cast<int32_t>(MasterComponentList[masterID].AmmoMasterId) == AmmoTypeTotal[ammoType].MasterId)
            {
                Inventory[item].AmmoIndex = static_cast<int16_t>(ammoType);
                break;
            }
        }
    }

    CalcLongestRangeWeapon();
    CalcLegStatus();
    CalcTorsoStatus();
    MaxCV = CalcCV(1);
    CurCV = CalcCV(0);
    MaxTargetDamage = CalcMaxTargetDamage();

    if (ObjType->ExplosionObject > 0)
    {
        ObjectTypeManager->Load(ObjType->ExplosionObject, 1);
    }

    MechClass = static_cast<uint8_t>(GetMechClass());
    return 0;
}

auto MCBattleMech::Write(MCFile* objFile) -> int32_t
{
    MCBigGameObject::Write(objFile);
    objFile->WriteString(DebugStatus.c_str());
    objFile->WriteString(IconName);
    objFile->WriteByte(Chassis);
    objFile->WriteLong(EndoSteel);
    objFile->WriteFloat(TonnageClass);
    objFile->WriteFloat(InternalStructureTonnage);

    for (int32_t location = 0; location < NUM_MECH_BODY_LOCATIONS; location++)
    {
        const MCBodyLocation& bodyLocation = BodyAt(location);
        objFile->WriteLong(bodyLocation.HasCase);
        objFile->Write(reinterpret_cast<const uint8_t*>(bodyLocation.CriticalSpaces),
                       NumLocationCriticalSpaces[location] * 8);
        objFile->WriteFloat(bodyLocation.CurInternalStructure);
        objFile->WriteByte(bodyLocation.MaxInternalStructure);
        objFile->WriteByte(bodyLocation.HotSpotNumber);
    }

    objFile->WriteByte(ArmorType);
    objFile->WriteFloat(ArmorTonnage);
    objFile->Write(reinterpret_cast<const uint8_t*>(Armor.get()), 0x58);
    const int32_t otherCount = NumOther;
    const int32_t weaponCount = NumWeapons;
    const int32_t ammoCount = NumAmmos;
    objFile->WriteLong(otherCount);
    objFile->WriteLong(weaponCount);
    objFile->WriteLong(ammoCount);
    // Original behaviour (OB-007): each list is written from the start of the inventory (the pointer isn't advanced).
    const auto writeItems = [&](int32_t count)
    {
        for (int32_t i = 0; i < count; i++)
        {
            const MCInventoryItem& item = Inventory[i];
            objFile->WriteByte(item.MasterID);
            objFile->WriteByte(item.Health);
            objFile->WriteByte(item.Disabled == 1 ? 1 : 0);
            objFile->WriteByte(item.FacesForward);
            objFile->WriteShort(item.Amount);
            objFile->WriteByte(item.BodyLocation);
        }
    };

    writeItems(otherCount);
    writeItems(weaponCount);
    writeItems(ammoCount);
    objFile->WriteByte(Cockpit);
    objFile->WriteByte(Engine);
    objFile->WriteByte(LifeSupport);
    objFile->WriteByte(Sensor);
    objFile->WriteByte(Ecm);
    objFile->WriteByte(Probe);
    objFile->WriteByte(Jammer);
    objFile->WriteByte(static_cast<uint8_t>(NumAntiMissileSystems));
    objFile->Write(AntiMissileSystem, 0x10);
    return objFile->WriteFloat(MaxRunSpeed);
}

auto MCBattleMech::CalcCV(int calcMax) -> int32_t
{
    double cv = ChassisBR;
    const int32_t numItems = NumAmmos + NumWeapons + NumOther;

    for (int32_t i = 0; i < numItems; i++)
    {
        if (calcMax != 0 || Inventory[i].Disabled == 0)
        {
            cv += MasterComponentList[Inventory[i].MasterID].BattleRating;
        }
    }

    return static_cast<int32_t>(cv);
}

auto MCBattleMech::CalcLegStatus() -> int32_t
{
    const uint8_t leftLeg = BodyAt(MECH_BODY_LOCATION_LLEG).DamageState;

    if (BodyAt(MECH_BODY_LOCATION_RLEG).DamageState == 2)
    {
        if (leftLeg == 2)
        {
            LegStatus = 3;

            if (Pilot != nullptr)
            {
                Pilot->TriggerAlarm(6, 0x42);
                return LegStatus;
            }

            return LegStatus;
        }

        if (LegStatus == 2)
        {
            return LegStatus;
        }
    }
    else if (leftLeg != 2)
    {
        LegStatus = 0;
        return LegStatus;
    }

    Pilot->RadioMessage(0x1e, 0);
    LegStatus = 2;
    return 2;
}

auto MCBattleMech::CalcTorsoStatus() -> int32_t
{
    if (BodyAt(MECH_BODY_LOCATION_CTORSO).DamageState == 1)
    {
        TorsoStatus = 1;
        return 1;
    }

    TorsoStatus = 0;
    return TorsoStatus;
}

auto MCBattleMech::PilotingCheck(uint32_t situation, float modifier) -> void
{
    if ((MPlayer != nullptr && MPlayer->IsServer == 0) || PilotingCheckPending != 0)
    {
        return;
    }

    double roll = RandomNumber(100);

    if ((situation & 2) != 0)
    {
        roll += 20.0;
    }

    if (BodyAt(MECH_BODY_LOCATION_RLEG).CurInternalStructure == 0.0f ||
        BodyAt(MECH_BODY_LOCATION_LLEG).CurInternalStructure == 0.0f)
    {
        roll += 100.0;
    }

    const MCInventoryItem& gyroItem = Inventory[Gyro];

    if (gyroItem.Health == 0)
    {
        roll += 100.0;
    }
    else if (static_cast<int32_t>(gyroItem.Health) < static_cast<int8_t>(MasterComponentList[gyroItem.MasterID].Health))
    {
        roll += 30.0;
    }

    if (Inventory[LeftLegActuator].Health == 0)
    {
        roll += 10.0;
    }

    if (Inventory[RightLegActuator].Health == 0)
    {
        roll += 10.0;
    }

    if ((situation & 1) == 0)
    {
        const int failed = static_cast<double>(Pilot->Skills[MWS_PILOTING]) <= roll ? 1 : 0;
        PilotingCheckPending = failed;
        Pilot->SkillPoints[MWS_PILOTING] += SkillTry[0];

        if (failed == 0)
        {
            Pilot->SkillPoints[MWS_PILOTING] += SkillSuccess[0];
        }
    }
    else
    {
        const int failed = static_cast<double>(Pilot->Skills[MWS_JUMPING] + PilotJumpMod) <= roll ? 1 : 0;
        PilotingCheckPending = failed;
        Pilot->SkillPoints[MWS_JUMPING] += SkillTry[1];

        if (failed == 0)
        {
            Pilot->SkillPoints[MWS_JUMPING] += SkillSuccess[1];
        }
    }
}

auto MCBattleMech::CanPowerUp() -> int
{
    return 1;
}

auto MCBattleMech::Destroy() -> void
{
    IfaceName.clear();

    if (StatusWindow != nullptr)
    {
        CloseStatusWindow();
        StatusWindow = nullptr;
    }
}

auto MCBattleMech::MineCheck() -> void
{
    if ((MPlayer != nullptr && MPlayer->IsServer == 0) || IsJumping(nullptr) != 0)
    {
        return;
    }

    MCScenarioMap* map = GameMap;

    // The mine state bits of a tile's overlay: Inner Sphere 11..12, Clan 13..14; the spread counts 25..26, 27..28.
    if (SteppedOnMine != 0)
    {
        const MCMapTile& tile = map->Map[ObjPosition->TileR * map->Width + ObjPosition->TileC];
        const uint32_t state = Alignment == -1 ? tile.Overlay >> 11 : tile.Overlay >> 13;

        if ((state & 3) == 0)
        {
            SteppedOnMine = 0;
            const int32_t tileR = ObjPosition->TileR;
            const int32_t tileC = ObjPosition->TileC;
            MCMapTile& here = map->Map[map->Width * tileR + tileC];

            if (GetAlignment() == -1)
            {
                here.Overlay = (here.Overlay & 0xffffefff) | 0x800;
            }
            else
            {
                here.Overlay = (here.Overlay & 0xffffbfff) | 0x2000;
            }

            if (MPlayer != nullptr)
            {
                MPlayer->AddMineChunk(tileR * 3, tileC * 3, Alignment != -1 ? 1 : 0, 1, 0);
                map = GameMap;
            }
        }
    }

    const uint32_t mine =
        Alignment == -1
            ? map->GetInnerSphereMine(ObjPosition->TileR, ObjPosition->TileC, ObjPosition->CellR, ObjPosition->CellC)
            : map->GetClanMine(ObjPosition->TileR, ObjPosition->TileC, ObjPosition->CellR, ObjPosition->CellC);

    if (mine == 0)
    {
        return;
    }

    int32_t firstRow = ObjPosition->TileR - 1;
    int32_t firstCol = ObjPosition->TileC - 1;

    if (firstRow < 0)
    {
        firstRow = 0;
    }

    if (firstCol < 0)
    {
        firstCol = 0;
    }

    const int32_t mapSide = MCTerrain::VerticesBlockSide * MCTerrain::BlocksMapSide;

    if (mapSide <= firstCol + 3)
    {
        firstCol = mapSide - 1;
    }

    if (mapSide <= firstRow + 3)
    {
        firstRow = mapSide - 1;
    }

    for (int32_t row = firstRow; row < firstRow + 3; row++)
    {
        for (int32_t col = firstCol; col < firstCol + 3; col++)
        {
            const bool inMap = row >= 0 && row < GameMap->Height && col >= 0 && col < GameMap->Width;
            Assert(inMap ? 1 : 0, 0, " Map Tile out of bounds ");

            // Port fix: the original goes on to touch the tile past the map's edge.
            if (!inMap)
            {
                continue;
            }

            MCMapTile& tile = GameMap->Map[GameMap->Width * row + col];
            const bool innerSphere = GetAlignment() == -1;
            uint32_t count = ((innerSphere ? tile.Overlay >> 25 : tile.Overlay >> 27) & 3) + 1;

            if (count > 3)
            {
                count = 3;
            }

            if (GetAlignment() == -1)
            {
                tile.Overlay = (tile.Overlay & 0xf9ffffff) | (count << 25);
            }
            else
            {
                tile.Overlay = (tile.Overlay & 0xe7ffffff) | (count << 27);
            }
        }
    }

    const int32_t tileR = ObjPosition->TileR;
    const int32_t tileC = ObjPosition->TileC;
    MCMapTile& here = GameMap->Map[GameMap->Width * tileR + tileC];

    if (GetAlignment() == -1)
    {
        here.Overlay |= 0x1800;
    }
    else
    {
        here.Overlay |= 0x6000;
    }

    if (MPlayer != nullptr)
    {
        MPlayer->AddMineChunk(tileR * 3 + ObjPosition->CellR, tileC * 3 + ObjPosition->CellC, Alignment != -1 ? 1 : 0,
                              3, 2);
    }

    Pilot->PausePath();
    MCVector3D position = GetPosition();
    CreateExplosion(MineExplosion, position, MineSplashDamage, WorldUnitsPerMeter * MineSplashRange);
    const int32_t hitLocation = CalcHitLocation(nullptr, -1, 3, 0);
    MCWeaponShotInfo shotInfo;
    shotInfo.Init(nullptr, -2, MineBaseDamage, hitLocation, 0.0f);
    HandleWeaponHit(&shotInfo, MPlayer != nullptr);

    if (GetPilot() != nullptr)
    {
        GetPilot()->RadioMessage(0x16, 1);
    }

    SteppedOnMine = 1;
}

auto MCBattleMech::UpdateJump() -> int
{
    if (IsJumping(nullptr) == 0)
    {
        return 0;
    }

    auto* actor = static_cast<MCMechActor*>(Appearance);
    auto* controlData = static_cast<MCMechControlData*>(Control->ControlData);

    if (actor->InJump == 0 && actor->JumpSetup == 0)
    {
        // Landed.
        InJump = 0;
        JumpTime = ScenarioTime;
        MCMovePath* path = Pilot->GetMovePath();
        Pilot->ResumePath();
        LastValidPosition = Position;
        path->CurStep++;
        PilotingCheck(1, 0.0f);
    }

    if (actor->Airborne == 0)
    {
        if (MPlayer == nullptr || MPlayer->IsServer != 0)
        {
            actor->SetJumpParameters(JumpGoal);

            if (static_cast<MCMechActor*>(Appearance)->InTransition == 0)
            {
                Appearance->SetGestureGoal(6);
                controlData->Throttle = 100;
            }
        }
        else if (DistanceFrom(JumpGoal) > 8.0f)
        {
            actor->SetJumpParameters(JumpGoal);

            if (static_cast<MCMechActor*>(Appearance)->InTransition == 0)
            {
                Appearance->SetGestureGoal(6);
                controlData->Throttle = 100;
                return 1;
            }
        }

        return 1;
    }

    // Turn toward the landing point: within two degrees, pivot by the pivot angle.
    float turn = RelFacingTo(JumpGoal, -1);

    if (turn >= -2.0f && turn <= 2.0f)
    {
        turn = turn < 0.0f ? -MechPivotAngle : MechPivotAngle;
    }

    const float maxRate = static_cast<float>(
        static_cast<MCMechDynamicsType*>(static_cast<MCBattleMechType*>(ObjType)->DynamicsType)->MaxMechYawRate);
    double rate = -(static_cast<double>(turn) / FrameLength);

    if (rate > maxRate)
    {
        rate = maxRate;
    }
    else if (rate < -maxRate)
    {
        rate = -maxRate;
    }

    controlData->Rotate = static_cast<int8_t>(static_cast<int32_t>(rate / maxRate * 64.0f));
    return 1;
}

auto MCBattleMech::PivotTo() -> int
{
    MCMechWarrior* warrior = Pilot;
    MCMovePath* path = warrior->GetMovePath();
    const int32_t moveStateGoal = warrior->MoveOrders.MoveStateGoal;
    const int32_t moveState = warrior->MoveOrders.MoveState;
    const int32_t run = MPlayer == nullptr || MPlayer->IsServer != 0 ? warrior->MoveOrders.Run : MoveChunk.Run;
    int hasTarget = 0;
    MCGameObject* target = warrior->GetLastTarget();
    float targetFacing = 0.0f;
    const float maxPivot =
        static_cast<float>(
            static_cast<MCMechDynamicsType*>(static_cast<MCBattleMechType*>(ObjType)->DynamicsType)->MaxMechPivotRate) *
        FrameLength;

    if (target == nullptr)
    {
        if (warrior->CurTacOrder.Code == TACTICAL_ORDER_ATTACK_POINT)
        {
            targetFacing = RelFacingTo(warrior->AttackOrders.TargetPoint, -1);
            hasTarget = 1;
        }
    }
    else
    {
        targetFacing = RelFacingTo(target->GetPosition(), -1);
        hasTarget = 1;
    }

    // Starts the pivot: a turn of <paramref name="turn"/> degrees, no faster than the pivot rate.
    const auto pivot = [&](float turn) -> int
    {
        if (maxPivot < std::fabs(turn))
        {
            turn = turn <= 0.0f ? -maxPivot : maxPivot;
        }

        auto* controlData = static_cast<MCMechControlData*>(Control->ControlData);
        controlData->Rotate = static_cast<int8_t>(static_cast<int32_t>(static_cast<double>(turn) / maxPivot * 64.0f));
        controlData->Pivot = 1;
        UpdateTorso(turn);
        return 1;
    };

    const auto choosePivotDirection = [&]()
    {
        if (PivotDirection == 0xff)
        {
            PivotDirection = targetFacing >= 0.0f ? 1 : 0;
        }
    };

    const auto hasNextStep = [&]()
    { return path->NumStepsWhenNotPaused >= 1 && path->CurStep < path->NumStepsWhenNotPaused; };

    if (moveState == MOVESTATE_PIVOT_FORWARD)
    {
        if (moveStateGoal == MOVESTATE_PIVOT_FORWARD || moveStateGoal == MOVESTATE_FORWARD)
        {
            if (!hasNextStep())
            {
                Pilot->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
            }
            else
            {
                const MCVector3D destination = path->StepList[path->CurStep].Destination;
                Appearance->SetGestureGoal(1);
                static_cast<MCMechControlData*>(Control->ControlData)->Throttle = 100;
                const float stepFacing = RelFacingTo(destination, -1);

                if (stepFacing < -15.0f || stepFacing > 15.0f)
                {
                    float turn = -stepFacing;

                    if (hasTarget != 0 && run == 0)
                    {
                        choosePivotDirection();

                        if (PivotDirection == 0)
                        {
                            if (stepFacing >= 0.0f)
                            {
                                turn = 360.0f - stepFacing;
                            }
                        }
                        else if (stepFacing < 0.0f)
                        {
                            turn = -360.0f - stepFacing;
                        }
                    }

                    return pivot(turn);
                }

                Pilot->MoveOrders.MoveState = MOVESTATE_FORWARD;

                if (Pilot->MoveOrders.MoveStateGoalChanged != 0)
                {
                    Pilot->MoveOrders.MoveStateGoalChanged = 0;
                }
            }
        }
        else
        {
            Pilot->MoveOrders.MoveState = MOVESTATE_FORWARD;
        }
    }
    else if (moveState == MOVESTATE_PIVOT_REVERSE)
    {
        if (moveStateGoal == MOVESTATE_PIVOT_REVERSE || moveStateGoal == MOVESTATE_REVERSE)
        {
            if (!hasNextStep())
            {
                Pilot->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
            }
            else
            {
                const MCVector3D destination = path->StepList[path->CurStep].Destination;
                Appearance->SetGestureGoal(1);
                static_cast<MCMechControlData*>(Control->ControlData)->Throttle = 100;
                const float stepFacing = RelFacingTo(destination, -1);

                if (stepFacing > -165.0f && stepFacing < 165.0f)
                {
                    bool turnLeft;

                    if (hasTarget == 0 || run != 0)
                    {
                        turnLeft = stepFacing < 0.0f;
                    }
                    else
                    {
                        choosePivotDirection();
                        turnLeft = PivotDirection != 0;
                    }

                    return pivot(turnLeft ? -180.0f - stepFacing : 180.0f - stepFacing);
                }

                MCMechWarrior* orders = Pilot;

                if (orders->MoveOrders.MoveStateGoalChanged != 0)
                {
                    orders->MoveOrders.MoveStateGoalChanged = 0;
                }

                if (moveStateGoal == MOVESTATE_REVERSE)
                {
                    orders->MoveOrders.MoveState = MOVESTATE_REVERSE;
                }
                else
                {
                    orders->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
                }
            }
        }
        else
        {
            Pilot->MoveOrders.MoveState = MOVESTATE_FORWARD;
        }
    }
    else if (moveState != MOVESTATE_PIVOT_TARGET)
    {
        if (moveStateGoal == MOVESTATE_PIVOT_TARGET || moveStateGoal == MOVESTATE_PIVOT_FORWARD ||
            moveStateGoal == MOVESTATE_PIVOT_REVERSE)
        {
            Pilot->MoveOrders.MoveState = moveStateGoal;
        }
    }
    else if (moveStateGoal != MOVESTATE_PIVOT_TARGET)
    {
        Pilot->MoveOrders.MoveState = MOVESTATE_FORWARD;
    }
    else if (run == 0 && hasTarget != 0)
    {
        Appearance->SetGestureGoal(1);
        static_cast<MCMechControlData*>(Control->ControlData)->Throttle = 100;
        const float fireArc = GetFireArc();

        if (targetFacing < -fireArc || fireArc < targetFacing)
        {
            return pivot(-targetFacing);
        }

        Pilot->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
    }
    else
    {
        Pilot->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
    }

    MCMechWarrior* orders = Pilot;

    if (!(orders->MoveOrders.YieldTime > -1.0f || orders->MoveOrders.WaitForPointTime > -1.0f))
    {
        orders->ResumePath();
    }

    PivotDirection = 0xff;
    return 0;
}

auto MCBattleMech::GetSpeedState() -> int32_t
{
    return MechSpeedStateArray[static_cast<MCMechActor*>(Appearance)->CurrentGesture];
}

auto MCBattleMech::UpdateMoveStateGoal() -> void
{
    MCMechWarrior* warrior = Pilot;
    MCMovePath* path = warrior->GetMovePath();
    const int32_t moveStateGoal = warrior->MoveOrders.MoveStateGoal;

    if (path->NumSteps < 1)
    {
        if (moveStateGoal != MOVESTATE_PIVOT_TARGET && moveStateGoal != MOVESTATE_PIVOT_FORWARD &&
            moveStateGoal != MOVESTATE_PIVOT_REVERSE)
        {
            warrior->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
        }

        return;
    }

    const int32_t run = MPlayer == nullptr || MPlayer->IsServer != 0 ? warrior->MoveOrders.Run : MoveChunk.Run;

    if (run != 0 || LegStatus == 2)
    {
        warrior->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
        return;
    }

    MCVector3D targetPosition;
    MCGameObject* target = warrior->GetLastTarget();

    if (target == nullptr)
    {
        if (warrior->CurTacOrder.Code != TACTICAL_ORDER_ATTACK_POINT)
        {
            warrior->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
            return;
        }

        targetPosition = warrior->AttackOrders.TargetPoint;
    }
    else
    {
        targetPosition = target->GetPosition();
    }

    if (path->NumStepsWhenNotPaused <= 0 || path->CurStep >= path->NumStepsWhenNotPaused)
    {
        return;
    }

    const double delta = RelFacingDelta(path->StepList[path->CurStep].Destination, targetPosition);
    MCMechWarrior* orders = Pilot;
    const double torsoArc =
        static_cast<MCMechDynamicsType*>(static_cast<MCBattleMechType*>(ObjType)->DynamicsType)->MaxTorsoYaw;

    if (orders->MoveOrders.MoveStateGoal == MOVESTATE_FORWARD)
    {
        // The target is behind: walk backward.
        if (torsoArc < delta && 180.0 - delta <= torsoArc && orders->MoveOrders.MoveStateGoalChanged == 0)
        {
            orders->MoveOrders.MoveStateGoalChanged = 1;
            orders->MoveOrders.MoveStateGoal = MOVESTATE_REVERSE;
        }
    }
    else if (torsoArc < 180.0 - delta && delta <= torsoArc && orders->MoveOrders.MoveStateGoalChanged == 0)
    {
        orders->MoveOrders.MoveStateGoalChanged = 1;
        orders->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
    }
}

auto MCBattleMech::UpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                  int32_t& newGestureStateGoal, int32_t& newMoveState, int32_t& minThrottle,
                                  int32_t& maxThrottle) -> int
{
    MCMechWarrior* warrior = Pilot;
    auto* controlData = static_cast<MCMechControlData*>(Control->ControlData);
    auto* dynType = static_cast<MCMechDynamicsType*>(static_cast<MCBattleMechType*>(ObjType)->DynamicsType);
    MCMovePath* path = warrior->GetMovePath();
    int running = LegStatus == 0 && warrior->MoveOrders.Run != 0 ? 1 : 0;
    newThrottleSetting = static_cast<char>(controlData->Throttle);
    newRotatePerSec = 0.0f;
    UpdateHustleTime();
    const bool hustling = static_cast<double>(ScenarioTime) < static_cast<double>(LastHustleTime) + 2.0;
    warrior = Pilot;
    MCMover* point = warrior->GetPoint();
    const bool groupMove = warrior->CurTacOrder.IsGroupOrder() != 0 && warrior->CurTacOrder.IsMoveOrder() != 0;

    if (running == 0 && !hustling && point != nullptr && point->IsDisabled() == 0 && point != this && groupMove)
    {
        // Keep pace with the group's point: wait (at most five seconds while walking) when ahead of it.
        MCMechWarrior* pointPilot = point->GetPilot();
        pointPilot->GetMovePath();
        const float pointDistanceLeft = pointPilot->GetMoveDistanceLeft();

        if (pointDistanceLeft <= warrior->GetMoveDistanceLeft())
        {
            warrior->MoveOrders.WaitForPointTime = -1.0f;

            if (warrior->MoveOrders.YieldTime <= -1.0f)
            {
                warrior->ResumePath();
            }
        }
        else
        {
            running = 0;
            const int32_t speedState = GetSpeedState();
            warrior = Pilot;

            if (speedState == 2)
            {
                if (warrior->MoveOrders.WaitForPointTime <= -1.0f)
                {
                    warrior->MoveOrders.WaitForPointTime = ScenarioTime + 5.0f;
                }
            }
            else if (warrior->MoveOrders.WaitForPointTime < ScenarioTime)
            {
                warrior->PausePath();
                warrior->MoveOrders.WaitForPointTime = 999999.0f;
            }
        }
    }
    else
    {
        warrior->MoveOrders.WaitForPointTime = -1.0f;
    }

    int result = 0;

    if (LegStatus != 0 && LegStatus != 1 && LegStatus != 2)
    {
        newGestureStateGoal = 1;
        return 0;
    }

    if (path->NumSteps < 1)
    {
        newGestureStateGoal = 1;
        return 0;
    }

    int32_t step = path->CurStep;

    if (step == path->NumSteps)
    {
        result = 1;

        if (warrior->MoveOrders.PathType == 2 &&
            warrior->MoveOrders.Path[0]->GlobalStep < warrior->MoveOrders.NumGlobalSteps - 1)
        {
            result = 0;
        }

        if (warrior->MoveOrders.Path[0] != nullptr)
        {
            warrior->MoveOrders.Path[0]->Clear();
        }

        return result;
    }

    MCVector3D destination = path->StepList[step].Destination;
    LastValidPosition = destination;
    const auto distance = static_cast<float>(DistanceFrom(destination));
    const int32_t numSteps = path->NumSteps;
    const float margin = step == numSteps - 1 ? MoveMarginOfError[1] : MoveMarginOfError[0];

    if (margin <= distance)
    {
        if (static_cast<int8_t>(path->StepList[step].Direction) > 7)
        {
            newGestureStateGoal = 6;
            return 0;
        }
    }
    else
    {
        // Reached the step: on to the next.
        step++;
        Pilot->MoveOrders.TimeOfLastStep = ScenarioTime;
        path->CurStep = step;

        if (numSteps <= step)
        {
            warrior = Pilot;
            result = 1;

            if (warrior->MoveOrders.PathType == 2 &&
                warrior->MoveOrders.Path[0]->GlobalStep < warrior->MoveOrders.NumGlobalSteps - 1)
            {
                result = 0;
            }

            if (warrior->MoveOrders.Path[0] != nullptr)
            {
                warrior->MoveOrders.Path[0]->Clear();
            }

            return result;
        }

        if (static_cast<int8_t>(path->StepList[step].Direction) > 7)
        {
            newGestureStateGoal = 6;
            return 0;
        }

        destination = path->StepList[step].Destination;
    }

    const float facing = RelFacingTo(destination, -1);
    warrior = Pilot;
    const int32_t moveState = warrior->MoveOrders.MoveState;
    const int32_t moveStateGoal = warrior->MoveOrders.MoveStateGoal;
    // Walking, the throttle creeps toward the ordered speed by tens.
    const auto walkThrottle = [&]() -> char
    {
        const char throttle = static_cast<char>(controlData->Throttle);

        if (GetBodyState() != 2)
        {
            return 100;
        }

        const char speed = static_cast<char>(Pilot->MoveOrders.SpeedThrottle);

        if (throttle < speed - 10)
        {
            return static_cast<char>(throttle + 10);
        }

        if (speed <= throttle && speed + 10 <= throttle)
        {
            return static_cast<char>(throttle - 10);
        }

        return speed;
    };

    if (moveState == MOVESTATE_FORWARD)
    {
        if (moveStateGoal != MOVESTATE_FORWARD)
        {
            warrior->PausePath();

            if (moveStateGoal == MOVESTATE_REVERSE || moveStateGoal == MOVESTATE_PIVOT_REVERSE)
            {
                newMoveState = MOVESTATE_PIVOT_REVERSE;
            }
            else if (moveStateGoal == MOVESTATE_PIVOT_FORWARD)
            {
                newMoveState = MOVESTATE_PIVOT_FORWARD;
            }
            else
            {
                newMoveState = MOVESTATE_FORWARD;
            }

            return result;
        }

        if (LegStatus == 2)
        {
            newGestureStateGoal = 5;
            newThrottleSetting = 100;
        }
        else if (running == 0)
        {
            newGestureStateGoal = 2;
        }
        else
        {
            newThrottleSetting = 100;
            newGestureStateGoal = 3;
        }

        if (facing < -5.0f || facing > 5.0f)
        {
            const float turn = -facing;
            newRotatePerSec = turn;
            const float maxTurn = static_cast<float>(dynType->MaxMechYawRate) * FrameLength;

            if (std::fabs(turn) <= maxTurn)
            {
                if (newGestureStateGoal == 2)
                {
                    newThrottleSetting = walkThrottle();
                }
            }
            else
            {
                newRotatePerSec = turn <= 0.0f ? -maxTurn : maxTurn;
            }

            newRotate = static_cast<char>(
                static_cast<int32_t>(std::floor(static_cast<double>(newRotatePerSec) / maxTurn * 64.0)));
        }

        return result;
    }

    if (moveState == MOVESTATE_REVERSE)
    {
        if (moveStateGoal == MOVESTATE_REVERSE)
        {
            newGestureStateGoal = 4;
            const float turn = facing >= 0.0f ? facing - 180.0f : facing + 180.0f;
            newRotatePerSec = -turn;
            const float maxTurn = static_cast<float>(dynType->MaxMechYawRate) * FrameLength;
            char throttle;

            if (std::fabs(newRotatePerSec) <= maxTurn)
            {
                throttle = walkThrottle();
            }
            else
            {
                newRotatePerSec = newRotatePerSec <= 0.0f ? -maxTurn : maxTurn;
                throttle = static_cast<char>(controlData->Throttle - 10);
            }

            newThrottleSetting = throttle;
            newRotate = static_cast<char>(static_cast<int32_t>(static_cast<double>(newRotatePerSec) / maxTurn * 64.0f));
            return result;
        }

        warrior->PausePath();

        if (moveStateGoal == MOVESTATE_FORWARD || moveStateGoal == MOVESTATE_PIVOT_FORWARD)
        {
            newMoveState = MOVESTATE_PIVOT_FORWARD;
        }
        else if (moveStateGoal == MOVESTATE_PIVOT_REVERSE)
        {
            newMoveState = MOVESTATE_PIVOT_REVERSE;
        }
        else
        {
            newMoveState = MOVESTATE_FORWARD;
        }

        return result;
    }

    if (moveStateGoal == MOVESTATE_FORWARD || moveStateGoal == MOVESTATE_PIVOT_FORWARD)
    {
        warrior->PausePath();
        newMoveState = MOVESTATE_PIVOT_FORWARD;
    }
    else if (moveStateGoal == MOVESTATE_REVERSE || moveStateGoal == MOVESTATE_PIVOT_REVERSE)
    {
        warrior->PausePath();
        newMoveState = MOVESTATE_PIVOT_REVERSE;
    }

    return result;
}

auto MCBattleMech::SetNextMovePath(char& newThrottleSetting, int32_t& newGestureStateGoal) -> void
{
    MCMechWarrior* warrior = Pilot;

    if (warrior->PlayerOrderFromQueue != 0 && warrior->CurTacOrder.IsMoveOrder() != 0)
    {
        if (warrior->MoveOrders.Path[0] != nullptr)
        {
            warrior->MoveOrders.Path[0]->Clear();
        }

        return;
    }

    warrior->ClearMoveOrders();
    newGestureStateGoal = 1;
}

auto MCBattleMech::UpdateTorso(float newRotatePerSec) -> void
{
    MCMechWarrior* warrior = Pilot;
    MCGameObject* target = warrior->GetLastTarget();
    double facing;

    if (target != nullptr)
    {
        facing = static_cast<double>(RelFacingTo(target->GetPosition(), -1)) + TorsoRotation + newRotatePerSec;
    }
    else if (warrior->CurTacOrder.Code == TACTICAL_ORDER_ATTACK_POINT)
    {
        facing =
            static_cast<double>(RelFacingTo(warrior->GetAttackTargetPoint(), -1)) + TorsoRotation + newRotatePerSec;
    }
    else
    {
        facing = TorsoRotation;
    }

    if (facing < -180.0)
    {
        facing += 360.0;
    }
    else if (facing > 180.0f)
    {
        facing -= 360.0;
    }

    if (facing >= -2.0 && facing <= 2.0)
    {
        return;
    }

    double turn = -facing;
    auto* dynType = static_cast<MCMechDynamicsType*>(static_cast<MCBattleMechType*>(ObjType)->DynamicsType);
    const float maxTurn = static_cast<float>(dynType->MaxTorsoYawRate) * FrameLength;

    if (maxTurn < std::fabs(turn))
    {
        turn = turn < 0.0 ? -maxTurn : maxTurn;
    }

    static_cast<MCMechControlData*>(Control->ControlData)->TorsoRotate =
        static_cast<int8_t>(static_cast<int32_t>(turn / maxTurn * 64.0f));
}

auto MCBattleMech::SetControlSettings(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                      int32_t& newGestureStateGoal, int32_t& minThrottle, int32_t& maxThrottle) -> void
{
    auto* actor = static_cast<MCMechActor*>(Appearance);

    if (InJump != 0 && actor->InJump == 0)
    {
        InJump = 0;
        Pilot->ResumePath();
    }

    if (newGestureStateGoal == 6)
    {
        // The path's step is a jump.
        MCMechWarrior* warrior = Pilot;
        MCMovePath* path = warrior->GetMovePath();
        warrior->PausePath();
        JumpGoal = path->StepList[path->CurStep].Destination;
        actor->SetJumpParameters(JumpGoal);
    }

    bool startJump = false;

    if (MPlayer == nullptr || MPlayer->IsServer != 0)
    {
        MCMechWarrior* warrior = Pilot;

        if (warrior->CurTacOrder.IsJumpOrder() != 0 && InJump == 0)
        {
            const float* point = warrior->CurTacOrder.MoveParams.WayPath.Points;
            JumpGoal = MCVector3D(point[0], point[1], point[2]);
            newGestureStateGoal = 6;
            startJump = true;
        }
    }
    else if (StatusChunk.JumpOrder != 0 && InJump == 0)
    {
        MapCellToWorldPos(StatusChunk.TargetCellRC[0], StatusChunk.TargetCellRC[1], JumpGoal);

        if (DistanceFrom(JumpGoal) > 8.0f)
        {
            newGestureStateGoal = 6;
            startJump = true;
        }
    }

    if (startJump)
    {
        actor->SetJumpParameters(JumpGoal);
    }

    const int32_t gestureGoal = newGestureStateGoal;
    auto* controlData = static_cast<MCMechControlData*>(Control->ControlData);

    if (gestureGoal != -1 && static_cast<MCMechActor*>(Appearance)->InTransition == 0)
    {
        auto* mechActor = static_cast<MCMechActor*>(Appearance);

        if (mechActor->SetGestureGoal(gestureGoal) == 0)
        {
            if (gestureGoal == 6)
            {
                InJump = 1;
            }

            if (gestureGoal != 2)
            {
                controlData->Throttle = 100;
            }
        }
        else if (mechActor->InTransition == 0 && mechActor->CurrentStateGesture == 2)
        {
            // Walking: the throttle stays within the limits.
            if (newThrottleSetting != -1)
            {
                if (newThrottleSetting < minThrottle)
                {
                    newThrottleSetting = static_cast<char>(minThrottle);
                }
                else if (maxThrottle < newThrottleSetting)
                {
                    newThrottleSetting = static_cast<char>(maxThrottle);
                }

                controlData->Throttle = newThrottleSetting;
            }
        }
    }

    if (newRotate != 0)
    {
        controlData->Rotate = newRotate;
    }
}

auto MCBattleMech::UpdateMovement() -> void
{
    auto* controlData = static_cast<MCMechControlData*>(Control->ControlData);
    int32_t minThrottle = 0x23;
    int32_t maxThrottle = 100;
    // A fall: gesture 7 or 8 (at random unless forced).
    const auto fallGesture = [&]() -> int32_t
    {
        int32_t gesture = 8 - (RandomNumber(2) != 0 ? 1 : 0);

        if (HitFromBehindThisFrame != 0)
        {
            gesture = 7;
        }
        else if (HitFromFrontThisFrame != 0)
        {
            gesture = 8;
        }

        return gesture;
    };

    if (DisableThisFrame != 0)
    {
        if (Appearance->SetGestureGoal(fallGesture()) == 0)
        {
            DisableThisFrame = 0;
            ShutDownThisFrame = 0;
            StartUpThisFrame = 0;
            HitFromFrontThisFrame = 0;
            HitFromBehindThisFrame = 0;
        }

        controlData->Throttle = static_cast<int8_t>(maxThrottle);
        return;
    }

    if (ShutDownThisFrame != 0)
    {
        const int32_t result = Appearance->SetGestureGoal(0);
        SoundSystem->PlayDigitalSample(0x3c, 1, this, 0, 0);

        if (result == 0 || result == -0x1521ffff)
        {
            ShutDownThisFrame = 0;
            StartUpThisFrame = 0;

            if (result == -0x1521ffff)
            {
                Status = 5;
            }
        }

        controlData->Throttle = static_cast<int8_t>(maxThrottle);
        return;
    }

    if (StartUpThisFrame != 0)
    {
        const int32_t result = Appearance->SetGestureGoal(1);
        SoundSystem->PlayDigitalSample(0x3d, 1, this, 0, 0);

        if (result == 0 || result == -0x1521ffff)
        {
            StartUpThisFrame = 0;
            ShutDownThisFrame = 0;

            if (result == -0x1521ffff)
            {
                Status = 0;
            }
        }

        controlData->Throttle = static_cast<int8_t>(maxThrottle);
        return;
    }

    if (Status == 4 || Status == 5 || Status == 1)
    {
        return;
    }

    if (IsCaptured() != 0 || EngineBlowTime > -1.0f)
    {
        return;
    }

    if (PilotingCheckPending != 0)
    {
        const int32_t result = Appearance->SetGestureGoal(fallGesture());

        if (result == 0 || result == -0x1521ffff)
        {
            PilotingCheckPending = 0;
        }

        controlData->Throttle = static_cast<int8_t>(maxThrottle);
        return;
    }

    if (UpdateJump() != 0)
    {
        return;
    }

    float newRotatePerSec = static_cast<float>(PivotTo());

    if (newRotatePerSec != 0.0f)
    {
        return;
    }

    char newRotate = 0;
    char newThrottleSetting = -1;
    int32_t newGestureStateGoal = -1;
    int32_t newMoveState = -1;
    UpdateMoveStateGoal();

    if (UpdateMovePath(newRotate, newThrottleSetting, newRotatePerSec, newGestureStateGoal, newMoveState, minThrottle,
                       maxThrottle) != 0)
    {
        SetNextMovePath(newThrottleSetting, newGestureStateGoal);
    }

    if (newMoveState != -1)
    {
        Pilot->MoveOrders.MoveState = newMoveState;
    }

    SetControlSettings(newRotate, newThrottleSetting, newRotatePerSec, newGestureStateGoal, minThrottle, maxThrottle);
    UpdateTorso(newRotatePerSec);
}

namespace
{
    /// <summary>
    /// The sine and cosine of a facing snapped to the sprites' 32 directions (-45 and 45 are exact), as
    /// getPositionFromHS and getJumpPosition turn their offsets.
    /// </summary>
    void SnappedFacing(double facing, double& s, double& c)
    {
        const float rotation = -(static_cast<float>(static_cast<int32_t>(facing * (1.0 / 11.25))) * 11.25f);

        if (rotation == 45.0f)
        {
            s = 0.70710677f;
            c = 0.70710677f;
        }
        else if (rotation == -45.0f)
        {
            s = -0.70710677f;
            c = 0.70710677f;
        }
        else
        {
            s = std::sin(static_cast<double>(rotation) * DEGREES_TO_RADIANS);
            c = static_cast<float>(std::cos(static_cast<double>(static_cast<float>(rotation * DEGREES_TO_RADIANS))));
        }
    }

    /// <summary>Radians to degrees, as MCX.EXE stores it.</summary>
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;

    double ExactFrameFacing(const MCFrameOfRef& frame)
    {
        const float cosine = UnitX.Z * frame.I.Z + UnitX.Y * frame.I.Y + UnitX.X * frame.I.X;
        double facing = frame.MyAcos(cosine) * RADIANS_TO_DEGREES;

        if (frame.I.Y < 0.0f)
        {
            facing = -facing;
        }

        return facing;
    }
}

auto MCBattleMechType::LayOutHotSpotPackets(const std::vector<uint32_t>& packetSizes,
                                            const std::vector<uint32_t>& outlineSizes) -> void
{
    struct Block
    {
        size_t Size = 0;
        const uint8_t* Bytes = nullptr;
    };

    const auto blockTotal = [](size_t size)
    {
        const size_t total = (size + 0xb) & ~static_cast<size_t>(3);
        return total < 0x10 ? static_cast<size_t>(0x10) : total;
    };

    const int32_t numGestures = static_cast<int32_t>(NumHotSpotPackets);
    const size_t pointerTable = static_cast<size_t>(numGestures) * 4 + 4;
    std::vector<Block> blocks;
    blocks.push_back({static_cast<size_t>(NumWeapons) * 4, nullptr});
    blocks.push_back({static_cast<size_t>(numGestures) * 32, HotSpotData});
    blocks.push_back({pointerTable, nullptr});
    blocks.push_back({pointerTable, nullptr});
    blocks.push_back({pointerTable, nullptr});
    std::vector<size_t> packetBlock(static_cast<size_t>(numGestures), 0);

    for (int32_t gesture = 0; gesture < numGestures; gesture++)
    {
        packetBlock[gesture] = blocks.size();
        blocks.push_back({packetSizes[gesture], GestureHotSpots[gesture]});

        if (outlineSizes[gesture] != 0)
        {
            blocks.push_back({outlineSizes[gesture], GestureOutlines[gesture]});
        }
    }

    const size_t declared = static_cast<size_t>(NumWeapons) + NumOthers;
    HotSpotPackets.assign(static_cast<size_t>(numGestures), {});
    HotSpotPacketShippedFloats.assign(static_cast<size_t>(numGestures), 0);

    for (int32_t gesture = 0; gesture < numGestures; gesture++)
    {
        const size_t self = packetBlock[gesture];
        const Block& own = blocks[self];
        const size_t shipped = own.Size / sizeof(float);
        HotSpotPacketShippedFloats[gesture] = static_cast<uint32_t>(shipped);
        const size_t wanted = std::max(shipped, static_cast<size_t>(NumFramesPerHotSpot[gesture]) * declared * 3);
        std::vector<float>& packet = HotSpotPackets[gesture];
        packet.assign(3 + wanted, 0.0f);

        if (self + 1 < blocks.size() && blocks[self + 1].Bytes != nullptr && blocks[self + 1].Size >= sizeof(float))
        {
            std::memcpy(&packet[0], blocks[self + 1].Bytes + blocks[self + 1].Size - sizeof(float), sizeof(float));
        }

        std::memcpy(&packet[3], own.Bytes, shipped * sizeof(float));

        std::vector<uint8_t> tail(blockTotal(own.Size) - 8 - own.Size, 0);
        const size_t tailBytes = (wanted - shipped) * sizeof(float);

        for (size_t above = self; above-- > 0 && tail.size() < tailBytes;)
        {
            const Block& block = blocks[above];
            tail.insert(tail.end(), 8, 0);

            if (block.Bytes != nullptr)
            {
                tail.insert(tail.end(), block.Bytes, block.Bytes + block.Size);
            }
            else
            {
                tail.insert(tail.end(), block.Size, 0);
            }

            tail.insert(tail.end(), blockTotal(block.Size) - 8 - block.Size, 0);
        }

        tail.resize(tailBytes, 0);

        if (tailBytes != 0)
        {
            std::memcpy(&packet[3 + shipped], tail.data(), tailBytes);
        }

        GestureHotSpots[gesture] = reinterpret_cast<uint8_t*>(&packet[3]);
    }
}

auto MCBattleMech::GetPositionFromHS(uint32_t hotSpot) -> MCVector3D
{
    auto* mechType = static_cast<MCBattleMechType*>(ObjType);

    if (mechType->NumOthers + mechType->NumWeapons <= hotSpot)
    {
        hotSpot = 0;
    }

    auto* actor = static_cast<MCMechActor*>(Appearance);
    const uint32_t gesture = actor->GetHotSpotIndex(static_cast<uint32_t>(actor->CurrentGesture));
    int32_t frameNumber = actor->CurrentFrame[0];
    const auto* offsets = reinterpret_cast<const float*>(mechType->GestureHotSpots[gesture]);
    const int32_t numFrames = static_cast<int32_t>(mechType->NumFramesPerHotSpot[gesture]);

    if (numFrames <= frameNumber)
    {
        frameNumber = numFrames - 1;
    }

    // Port fix: some packets hold fewer hot spots than numWeapons + numOthers (cm.hsp's gestures 0-14 hold 3 of 6),
    // and the original reads past them into the heap. Read hot spot 0's offset, as the range check above does (the
    // mount's turn below still uses the real hot spot).
    uint32_t dataHotSpot = hotSpot;

#if MCREDUX_FIX_SHORT_HOTSPOT_PACKETS
    if (numFrames > 0 && gesture < mechType->HotSpotPacketShippedFloats.size() &&
        mechType->HotSpotPacketShippedFloats[gesture] / (static_cast<uint32_t>(numFrames) * 3) <= dataHotSpot)
    {
        dataHotSpot = 0;
    }
#endif

    const int32_t index = numFrames * static_cast<int32_t>(dataHotSpot) + frameNumber;
    const float offsetX = offsets[index * 3];
    const float offsetY = offsets[index * 3 + 1];
    const float offsetZ = offsets[index * 3 + 2];

    // The body's facing, plus the torso's (and an arm's) for the weapons mounted on them.
    const double exactFacing = ExactFrameFacing(Frame);
    const float facing = static_cast<float>(exactFacing);
    double turned = exactFacing;

    if (hotSpot < mechType->NumWeapons)
    {
        switch (mechType->WeaponHotSpots[hotSpot])
        {
            case 1:
                turned = static_cast<double>(facing) + TorsoRotation;
                break;
            case 2:
                turned = static_cast<double>(LeftArmRotation) + TorsoRotation + facing;
                break;
            case 3:
                turned = static_cast<double>(RightArmRotation) + TorsoRotation + facing;
                break;
            default:
                break;
        }
    }
    else if (hotSpot < mechType->NumWeapons + 3)
    {
        turned = static_cast<double>(facing) + TorsoRotation;
    }

    double s;
    double c;
    SnappedFacing(turned, s, c);
    MCVector3D result;
    result.X = static_cast<float>(c * offsetX + s * offsetY) * 20.0f + Position.X;
    result.Z = offsetZ * 20.0f + Position.Z;
    result.Y = static_cast<float>((c * offsetY - s * offsetX) * 20.0f + Position.Y);
    return result;
}

auto MCBattleMech::OnScreen() -> int
{
    MCCamera* camera = CameraList()->FindCameraFromIDNumber(1);
    ScreenPos.Y = 0.0f;
    ScreenPos.X = 0.0f;

    if (camera == nullptr || camera->Active == 0)
    {
        return 0;
    }

    float screenY;

    if (UseOldProject == 0)
    {
        MCVector2D screen100;
        MCVector2D screen50;

        if (Terrain() != nullptr)
        {
            Terrain()->ProjectTerrain(Position, screen100, screen50);
        }

        if (camera->CameraScale == 1)
        {
            ScreenPos.X = (screen50.X - camera->ScreenUL50.X) + camera->HalfWidth;
            screenY = screen50.Y - camera->ScreenUL50.Y;
        }
        else
        {
            ScreenPos.X = (screen100.X - camera->ScreenUL.X) + camera->HalfWidth;
            screenY = screen100.Y - camera->ScreenUL.Y;
        }

        screenY += camera->HalfHeight;
    }
    else
    {
        const float scale = camera->CameraScale != 1 ? 1.0f : 0.5f;
        MCVector3D relative(Position.X - camera->Position.X, Position.Y - camera->Position.Y,
                            Position.Z - camera->Position.Z);
        relative *= scale;
        ScreenPos.X = relative.Y * camera->CosAngle + relative.X * camera->CosAngle + camera->HalfWidth;
        screenY = ((relative.X * camera->SinAngle + camera->HalfHeight) - relative.Y * camera->SinAngle) - relative.Z;
    }

    ScreenPos.Y = screenY;

    if (Appearance != nullptr && Appearance->RecalcBounds(camera) != 0)
    {
        WindowsVisible = Turn;
        return 1;
    }

    return 0;
}

auto MCBattleMech::CreateJumpFX() -> void
{
    if (JumpFX[0] != nullptr || JumpFX[1] != nullptr)
    {
        return;
    }

    JumpFX[0] = CreateObject(0x1c6);
    static_cast<MCJet*>(JumpFX[0])->SetOwner(this);
    JumpFX[1] = CreateObject(0x1c6);
    static_cast<MCJet*>(JumpFX[1])->SetOwner(this);
    CraterManager()->AddCrater(7, Position, 0);
}

auto MCBattleMech::EndJumpFX() -> void
{
    if (JumpFX[0] == nullptr && JumpFX[1] == nullptr)
    {
        return;
    }

    delete JumpFX[0];
    JumpFX[0] = nullptr;
    delete JumpFX[1];
    JumpFX[1] = nullptr;
}

auto MCBattleMech::GetJumpPosition(int32_t jet) -> MCVector3D
{
    if (jet < 0 || jet > 1)
    {
        jet = 0;
    }

    auto* actor = static_cast<MCMechActor*>(Appearance);
    const int32_t frameNumber = actor->CurrentFrame[0];
    const int32_t numFrames = static_cast<int32_t>(actor->GetNumFramesInGesture(0x14));
    const int32_t index = numFrames * jet + frameNumber;
    const auto* offsets = reinterpret_cast<const float*>(static_cast<MCBattleMechType*>(ObjType)->JumpData);
    const float offsetX = offsets[index * 3];
    const float offsetY = offsets[index * 3 + 1];
    const float offsetZ = offsets[index * 3 + 2];
    const double facing = ExactFrameFacing(Frame);
    double s;
    double c;
    SnappedFacing(facing, s, c);
    MCVector3D base = Position;

    if (actor->FrameHeights != nullptr)
    {
        // Lifted along the mech's up axis by the jump's height this frame.
        const float height = actor->FrameHeights[frameNumber] * 30.0f;
        base.X = Frame.K.X * height + base.X;
        base.Y = base.Y + Frame.K.Y * height;
        base.Z = base.Z + height * Frame.K.Z;
    }

    MCVector3D result;
    result.X = base.X + static_cast<float>(c * offsetX + offsetY * s) * 20.0f;
    result.Z = offsetZ * 20.0f + base.Z;
    result.Y = static_cast<float>((offsetY * c - s * offsetX) * 20.0f) + base.Y;
    return result;
}

auto MCBattleMech::CrashAvoidanceSystem() -> int
{
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        return 0;
    }

    MCMechWarrior* warrior = Pilot;
    MCMovePath* path = warrior->GetMovePath();

    if (path->NumStepsWhenNotPaused == 0)
    {
        return 0;
    }

    if (static_cast<double>(warrior->MoveOrders.WaitForPointTime) > 999990.0)
    {
        return 0;
    }

    // A look a frame ahead along the frame turned by a quarter pi (its result is unused).
    const float speed = -static_cast<MCMechActor*>(Appearance)->GetVelocityMagnitude();
    MCFrameOfRef ahead = Frame;
    RotateAboutK(ahead, static_cast<float>(std::sin(HALF_PI / 2.0)), static_cast<float>(std::cos(HALF_PI / 2.0)));
    MCVector3D lookAhead(ahead.J.X * speed * FrameLength * WorldUnitsPerMeter + Position.X,
                         ahead.J.Y * speed * FrameLength * WorldUnitsPerMeter + Position.Y,
                         WorldUnitsPerMeter * 0.0f + Position.Z);
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    GameMap->WorldToMapPos(lookAhead, tileR, tileC, cellR, cellC);

    int cornerBlocked = 0;
    const int32_t direction = static_cast<int8_t>(path->StepList[path->CurStep].Direction);

    if (direction == 1 || direction == 3 || direction == 5 || direction == 7)
    {
        // A diagonal step: blocked when both cells beside it are locked.
        const int first = GetAdjacentCellPathLocked(ObjPosition->TileR, ObjPosition->TileC, ObjPosition->CellR,
                                                    ObjPosition->CellC, AdjClippedCell[direction][0]);
        const int second = GetAdjacentCellPathLocked(ObjPosition->TileR, ObjPosition->TileC, ObjPosition->CellR,
                                                     ObjPosition->CellC, AdjClippedCell[direction][1]);
        cornerBlocked = first != 0 && second != 0 ? 1 : 0;
    }

    int lockReachedEnd = 0;
    int blockReachedEnd = 0;
    const int locked = GetPathRangeLock(CrashAvoidPath, &lockReachedEnd);
    const int blocked = GetPathRangeBlocked(CrashAvoidPath, &blockReachedEnd);
    const int32_t closedGates = path->CrossesClosedGate(-1, 2);
    warrior = Pilot;
    const bool clear = locked == 0 && blocked == 0 && cornerBlocked == 0 && closedGates < 1;

    if (warrior->MoveOrders.YieldTime > -1.0f)
    {
        // Yielding: go on once the way is clear.
        if (clear)
        {
            warrior->ResumePath();
            warrior->MoveOrders.YieldTime = -1.0f;
            return 0;
        }

        warrior->PausePath();
        return 1;
    }

    if (clear)
    {
        return 0;
    }

    if (lockReachedEnd == 0 && blockReachedEnd == 0)
    {
        warrior->PausePath();
        warrior->MoveOrders.YieldTime = ScenarioTime + CrashYieldTime;
        Control->ControlData->Brake();
        return 1;
    }

    warrior->ReachedPathEnd();
    Control->ControlData->Brake();
    return 1;
}

auto MCBattleMech::NetUpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                     int32_t& newGestureStateGoal, int32_t& newMoveState, int32_t& minThrottle,
                                     int32_t& maxThrottle) -> int
{
    auto* dynType = static_cast<MCMechDynamicsType*>(static_cast<MCBattleMechType*>(ObjType)->DynamicsType);
    auto* controlData = static_cast<MCMechControlData*>(Control->ControlData);
    MCMovePath* path = Pilot->GetMovePath();
    const int running = LegStatus == 0 && MoveChunk.Run != 0 ? 1 : 0;
    newThrottleSetting = static_cast<char>(controlData->Throttle);
    newRotatePerSec = 0.0f;

    if ((LegStatus != 0 && LegStatus != 1 && LegStatus != 2) || path->NumSteps < 1)
    {
        newGestureStateGoal = 1;
        return 0;
    }

    int32_t step = path->CurStep;

    if (step == path->NumSteps)
    {
        return 1;
    }

    MCVector3D destination = path->StepList[step].Destination;
    LastValidPosition = destination;
    const auto distance = static_cast<float>(DistanceFrom(destination));
    const int32_t numSteps = path->NumSteps;
    const float margin = step == numSteps - 1 ? MoveMarginOfError[1] : MoveMarginOfError[0];

    if (margin <= distance)
    {
        if (static_cast<int8_t>(path->StepList[step].Direction) > 7)
        {
            newGestureStateGoal = 6;
            return 0;
        }
    }
    else
    {
        step++;
        Pilot->MoveOrders.TimeOfLastStep = ScenarioTime;
        path->CurStep = step;

        if (numSteps <= step)
        {
            return 1;
        }

        if (static_cast<int8_t>(path->StepList[step].Direction) > 7)
        {
            newGestureStateGoal = 6;
            return 0;
        }

        destination = path->StepList[step].Destination;
    }

    const float facing = RelFacingTo(destination, -1);
    MCMechWarrior* warrior = Pilot;
    const int32_t moveState = warrior->MoveOrders.MoveState;
    const int32_t moveStateGoal = warrior->MoveOrders.MoveStateGoal;
    const auto walkThrottle = [&]() -> char
    {
        const char throttle = static_cast<char>(controlData->Throttle);

        if (GetBodyState() != 2)
        {
            return 100;
        }

        const char speed = static_cast<char>(Pilot->MoveOrders.SpeedThrottle);

        if (throttle < speed - 10)
        {
            return static_cast<char>(throttle + 10);
        }

        if (speed <= throttle && speed + 10 <= throttle)
        {
            return static_cast<char>(throttle - 10);
        }

        return speed;
    };

    // The turn per second is limited to the yaw rate (not scaled by the frame here).
    const float maxRate = static_cast<float>(dynType->MaxMechYawRate);

    if (moveState == MOVESTATE_FORWARD && moveStateGoal == MOVESTATE_FORWARD)
    {
        if (LegStatus == 2)
        {
            newGestureStateGoal = 5;
            newThrottleSetting = 100;
        }
        else if (running == 0)
        {
            newGestureStateGoal = 2;
        }
        else
        {
            newThrottleSetting = 100;
            newGestureStateGoal = 3;
        }

        if (facing >= -5.0f && facing <= 5.0f)
        {
            return 0;
        }

        newRotatePerSec = -(facing / FrameLength);

        if (newRotatePerSec > maxRate)
        {
            newRotatePerSec = maxRate;
        }
        else if (newRotatePerSec < -maxRate)
        {
            newRotatePerSec = -maxRate;
        }
        else if (newGestureStateGoal == 2)
        {
            newThrottleSetting = walkThrottle();
        }

        newRotate = static_cast<char>(static_cast<int32_t>(static_cast<double>(newRotatePerSec) / maxRate * 64.0f));
        return 0;
    }

    if (moveState == MOVESTATE_REVERSE && moveStateGoal == MOVESTATE_REVERSE)
    {
        newGestureStateGoal = 4;
        newRotatePerSec = facing >= 0.0f ? -((facing - 180.0f) / FrameLength) : -((facing + 180.0f) / FrameLength);

        if (newRotatePerSec > maxRate)
        {
            newRotatePerSec = maxRate;
            newThrottleSetting = static_cast<char>(controlData->Throttle - 10);
        }
        else if (newRotatePerSec < -maxRate)
        {
            newRotatePerSec = -maxRate;
            newThrottleSetting = static_cast<char>(controlData->Throttle - 10);
        }
        else
        {
            newThrottleSetting = walkThrottle();
        }

        newRotate = static_cast<char>(static_cast<int32_t>(static_cast<double>(newRotatePerSec) / maxRate * 64.0f));
        return 0;
    }

    // Otherwise pivot: forward for goals 1 and 3, backward for 2 and 4; from forward or reverse, any other goal
    // stops.
    int32_t pivotState;

    if (moveStateGoal == MOVESTATE_FORWARD || moveStateGoal == MOVESTATE_PIVOT_FORWARD)
    {
        pivotState = MOVESTATE_PIVOT_FORWARD;
    }
    else if (moveStateGoal == MOVESTATE_REVERSE || moveStateGoal == MOVESTATE_PIVOT_REVERSE)
    {
        pivotState = MOVESTATE_PIVOT_REVERSE;
    }
    else if (moveState == MOVESTATE_FORWARD || moveState == MOVESTATE_REVERSE)
    {
        pivotState = MOVESTATE_FORWARD;
    }
    else
    {
        return 0;
    }

    warrior->PausePath();
    newMoveState = pivotState;
    return 0;
}

auto MCBattleMech::NetUpdateMovement() -> void
{
    auto* controlData = static_cast<MCMechControlData*>(Control->ControlData);
    int32_t minThrottle = 0x23;
    int32_t maxThrottle = 100;
    const int32_t bodyState = GetBodyState();
    MCMovePath* path = Pilot->GetMovePath();
    MCVector3D destination = path->StepList[path->CurStep].Destination;
    const auto distance = static_cast<float>(DistanceFrom(destination));

    if (path->NumStepsWhenNotPaused > 0 && bodyState == 0)
    {
        StartUpThisFrame = 1;
    }

    if (path->NumSteps - 1 <= path->CurStep && distance < MoveMarginOfError[1])
    {
        // At the end of the path: take up the body state the server sent.
        StartUpThisFrame = 0;
        int32_t gesture = -1;

        switch (StatusChunk.BodyState)
        {
            case 1:
            {
                if (bodyState != 1)
                {
                    if (bodyState == 0)
                    {
                        SoundSystem->PlayDigitalSample(0x3d, 1, this, 0, 0);
                    }

                    gesture = 1;
                }
                break;
            }
            case 2:
            {
                if (bodyState != 0)
                {
                    SoundSystem->PlayDigitalSample(0x3c, 1, this, 0, 0);
                    gesture = 0;
                }
                break;
            }
            case 3:
            {
                if (bodyState != 8)
                {
                    gesture = 8;
                }
                break;
            }
            case 4:
            {
                if (bodyState != 7)
                {
                    gesture = 7;
                }
                break;
            }
            default:
                break;
        }

        if (gesture != -1)
        {
            Pilot->ClearMoveOrders();
            Appearance->SetGestureGoal(gesture);
            controlData->Throttle = static_cast<int8_t>(maxThrottle);
            return;
        }
    }

    if (DisableThisFrame != 0)
    {
        int32_t gesture = 8 - (RandomNumber(2) != 0 ? 1 : 0);

        if (HitFromBehindThisFrame != 0)
        {
            gesture = 7;
        }
        else if (HitFromFrontThisFrame != 0)
        {
            gesture = 8;
        }

        if (Appearance->SetGestureGoal(gesture) == 0)
        {
            DisableThisFrame = 0;
            ShutDownThisFrame = 0;
            StartUpThisFrame = 0;
            HitFromFrontThisFrame = 0;
            HitFromBehindThisFrame = 0;
        }

        controlData->Throttle = static_cast<int8_t>(maxThrottle);
        return;
    }

    if (ShutDownThisFrame != 0)
    {
        const int32_t result = Appearance->SetGestureGoal(0);

        if (result == 0 || result == -0x1521ffff)
        {
            ShutDownThisFrame = 0;
            StartUpThisFrame = 0;

            if (result == -0x1521ffff)
            {
                Status = 5;
            }
        }

        controlData->Throttle = static_cast<int8_t>(maxThrottle);
        return;
    }

    if (StartUpThisFrame != 0)
    {
        const int32_t result = Appearance->SetGestureGoal(1);

        if (result == 0 || result == -0x1521ffff)
        {
            StartUpThisFrame = 0;
            ShutDownThisFrame = 0;

            if (result == -0x1521ffff)
            {
                Status = 0;
            }
        }

        controlData->Throttle = static_cast<int8_t>(maxThrottle);
        return;
    }

    if (Status == 4 || Status == 5 || Status == 1 || IsCaptured() != 0 || EngineBlowTime > -1.0f)
    {
        return;
    }

    if (UpdateJump() != 0)
    {
        return;
    }

    float newRotatePerSec = static_cast<float>(PivotTo());

    if (newRotatePerSec != 0.0f)
    {
        return;
    }

    char newRotate = 0;
    char newThrottleSetting = -1;
    int32_t newGestureStateGoal = -1;
    int32_t newMoveState = -1;
    UpdateMoveStateGoal();
    NetUpdateMovePath(newRotate, newThrottleSetting, newRotatePerSec, newGestureStateGoal, newMoveState, minThrottle,
                      maxThrottle);

    if (newMoveState != -1)
    {
        Pilot->MoveOrders.MoveState = newMoveState;
    }

    SetControlSettings(newRotate, newThrottleSetting, newRotatePerSec, newGestureStateGoal, minThrottle, maxThrottle);
    UpdateTorso(newRotatePerSec);
}

namespace
{
    /// <summary>Pi, as MCX.EXE stores it (a hair under the true value).</summary>
    constexpr double MCX_PI = 0x1.921fb5443e88cp+1;

    /// <summary>
    /// Turns (<paramref name="x"/>, <paramref name="y"/>) by <paramref name="degrees"/> (MC2's inline Rotate): 45 and
    /// -45 exactly, otherwise the sine at full precision and the cosine through a float angle.
    /// </summary>
    void RotateXY(float& x, float& y, float degrees)
    {
        double s;
        double c;

        if (degrees == 45.0f)
        {
            s = 0.70710677f;
            c = 0.70710677f;
        }
        else if (degrees == -45.0f)
        {
            s = -0.70710677f;
            c = 0.70710677f;
        }
        else
        {
            s = std::sin(static_cast<double>(degrees) * DEGREES_TO_RADIANS);
            c = static_cast<float>(std::cos(static_cast<double>(static_cast<float>(degrees * DEGREES_TO_RADIANS))));
        }

        const double oldX = x;
        x = static_cast<float>(c * x + s * y);
        y = static_cast<float>(c * y - s * oldX);
    }

    /// <summary>Turns (<paramref name="x"/>, <paramref name="y"/>) half a circle, by MCX.EXE's pi.</summary>
    void RotateXYHalf(float& x, float& y)
    {
        const double s = std::sin(MCX_PI);
        const double c = std::cos(MCX_PI);
        const double oldX = x;
        x = static_cast<float>(c * x + s * y);
        y = static_cast<float>(c * y - s * oldX);
    }

    /// <summary>A frame's facing in degrees from the world's x axis, negative when its i axis points to -y.</summary>
    float FrameFacing(MCFrameOfRef& frame)
    {
        return static_cast<float>(ExactFrameFacing(frame));
    }

    /// <summary>
    /// Throws off an arm (debris type <paramref name="debrisId"/>): framed the torso's way, flying sideways at a
    /// random angle from <paramref name="angle"/>, painted as the mech.
    /// </summary>
    void ThrowArm(MCBattleMech* mech, uint32_t debrisId, float angle)
    {
        MCGameObject* piece = CreateObject(static_cast<int32_t>(debrisId));

        if (piece == nullptr)
        {
            return;
        }

        MCFrameOfRef armFrame = mech->Frame;
        const double torso = static_cast<double>(mech->TorsoRotation) * DEGREES_TO_RADIANS;
        RotateAboutK(armFrame, static_cast<float>(std::sin(torso)), static_cast<float>(std::cos(torso)));
        piece->SetFrame(armFrame);
        MCVector3D flight = mech->Frame.J;
        const float length = std::sqrt(flight.X * flight.X + flight.Y * flight.Y + flight.Z * flight.Z);

        if (length != 0.0f)
        {
            flight.X = flight.X / length;
            flight.Y = flight.Y / length;
            flight.Z = flight.Z / length;
        }

        auto* debris = static_cast<MCDebris*>(piece);
        debris->RandomAngle(angle);
        RotateXY(flight.X, flight.Y, angle);

        if (FrameFacing(armFrame) >= 0.0f)
        {
            RotateXYHalf(flight.X, flight.Y);
        }

        piece->SetVelocity(flight);
        piece->SetPosition(mech->Position);
        debris->SetPaintScheme(static_cast<MCMechActor*>(mech->Appearance)->FadeTableIndex);

        if (ObjectList->Head != nullptr)
        {
            ObjectList->Head->AddNode(piece);
        }
    }

    /// <summary>
    /// Leaves a footprint at hot spot offset (<paramref name="offsetX"/>, <paramref name="offsetY"/>) turned by
    /// -<paramref name="angle"/> degrees, rotation <paramref name="direction"/> (of 16), with a step sound.
    /// </summary>
    void MakeFootprint(MCBattleMech* mech, float offsetX, float offsetY, float angle, int32_t direction)
    {
        RotateXY(offsetX, offsetY, -angle);
        MCVector3D printPos;
        printPos.Z = mech->Position.Z;
        printPos.X = offsetX * 20.0f + mech->Position.X;
        printPos.Y = offsetY * 20.0f + mech->Position.Y;
        CraterManager()->AddCrater(static_cast<MCBattleMechType*>(mech->ObjType)->FootprintType, printPos, direction);
        SoundSystem->PlayDigitalSample(0xd, 1, mech, 0, 0);
    }

    /// <summary>A footprint's rotation (of 16) for <paramref name="degrees"/>.</summary>
    int32_t FootprintDirection(float degrees)
    {
        auto direction = static_cast<int32_t>(std::floor(static_cast<double>(degrees * (1.0f / 22.5f))));

        if (direction < 0)
        {
            direction += 16;
        }

        return direction;
    }
}

auto MCBattleMech::Update() -> int32_t
{
    TerrainNormal = Terrain()->GetTerrainNormal(Position);
    UpdatePathLock(0);

    if (IsDestroyed() != 0 || IsDisabled() != 0)
    {
        CollisionsOn = 0;
    }

    if (Withdrawing != 0 && Pilot->Status == 2)
    {
        CollisionsOn = 0;
        return 1;
    }

    auto* actor = static_cast<MCMechActor*>(Appearance);

    if (IsDestroyed() != 0)
    {
        if (JumpFX[0] != nullptr || JumpFX[1] != nullptr)
        {
            EndJumpFX();
        }

        int32_t result = Dynamics->Update();

        if (result != 1)
        {
            return result;
        }

        // The wreck keeps sliding along its frame's j axis turned an eighth of a circle.
        const float speed = -actor->GetVelocityMagnitude();
        MCFrameOfRef turned = Frame;
        RotateAboutK(turned, static_cast<float>(std::sin(HALF_PI / 2.0)), static_cast<float>(std::cos(HALF_PI / 2.0)));
        Velocity.X = speed * turned.J.X;
        Velocity.Y = speed * turned.J.Y;
        Velocity.Z = speed * turned.J.Z;
        MCVector3D newPosition;
        newPosition.X =
            static_cast<float>(static_cast<double>(Velocity.X) * FrameLength * WorldUnitsPerMeter + Position.X);
        newPosition.Y = Velocity.Y * FrameLength * WorldUnitsPerMeter + Position.Y;
        newPosition.Z = Velocity.Z * FrameLength * WorldUnitsPerMeter + Position.Z;
        SetPosition(newPosition);
        const int visibleNow = OnScreen();

        if (actor != nullptr)
        {
            actor->SetGestureGoal(8);
            actor->Visible = visibleNow;
            actor->SetCombatMode(0);
            result = actor->Update();

            if (result != 1)
            {
                return result;
            }
        }

        // Once the death animation is done, it blows up and leaves a crater.
        if (LyingDead != 0 || (LyingDead = actor->LyingStill) != 0)
        {
            DeathTimer -= FrameLength;

            if (DeathTimer < 0.4 && DeathExplosionDone == 0)
            {
                auto* mechType = static_cast<MCBattleMechType*>(ObjType);
                mechType->CreateExplosion(Position, mechType->ExplDmg, mechType->ExplRad);
                DeathExplosionDone = 1;
                return 1;
            }

            if (DeathTimer < 0.0 && WreckDone == 0)
            {
                actor->Wrecked = 1;
                CraterManager()->AddCrater(6, Position, 0);
                TheInterface->RemoveMech(PartId);
                WreckDone = 1;
                return 1;
            }
        }
    }
    else
    {
        if (GetAwake() != 0 && IsDisabled() == 0 && Scenario->GodMode == 0 &&
            MCTerrain::MetersPerVertex <= DistanceSinceMarkSeen)
        {
            // Every vertex travelled, the mech marks what it sees.
            if (Alignment == 1)
            {
                Terrain()->MarkSeen(Position, Frame.J, 360.0f, GetProbeEffect() + Scenario->MaxVisualRange, 1);
            }
            else if (Alignment == -1)
            {
                Terrain()->MarkSeen(Position, Frame.J, 360.0f, GetProbeEffect() + Scenario->MaxVisualRange, 2);
            }

            DistanceSinceMarkSeen = 0.0f;
        }

        if (DeselectTime != 0.0f && DeselectTime < ScenarioTime)
        {
            DeselectTime = 0.0f;
            Selected = 0;
        }

        int32_t result = Control->Update();

        if (result != 1)
        {
            return result;
        }

        if (GetAwake() == 0 && actor->SetGestureGoal(0) == 0)
        {
            ShutDownThisFrame = 0;
        }

        result = Dynamics->Update();

        if (result != 1)
        {
            return result;
        }

        int avoiding = 0;

        if (IsDisabled() == 0)
        {
            // The original looks at the pilot's attack order here and does nothing with it.
            if (GetPilot()->CurTacOrder.Code == TACTICAL_ORDER_ATTACK_OBJECT &&
                GetPilot()->CurTacOrder.AttackParams.Method == 2)
            {
                GetPilot();
            }

            avoiding = CrashAvoidanceSystem();
        }

        float speed = 0.0f;

        if (avoiding == 0)
        {
            speed = actor->GetVelocityMagnitude();
        }

        const int32_t gesture = actor->CurrentGesture;
        MCFrameOfRef turned = Frame;
        speed = -speed;

        if (gesture == 20)
        {
            // Jumping: the actor's jump velocity.
            float jumpSpeed = 0.0f;

            if (actor->Airborne != 0)
            {
                jumpSpeed = actor->GetVelocityMagnitude();
            }

            Velocity.X = jumpSpeed * actor->JumpDirection.X;
            Velocity.Y = jumpSpeed * actor->JumpDirection.Y;
            Velocity.Z = jumpSpeed * actor->JumpDirection.Z;
        }
        else
        {
            RotateAboutK(turned, static_cast<float>(std::sin(HALF_PI / 2.0)),
                         static_cast<float>(std::cos(HALF_PI / 2.0)));
            Velocity.Y = turned.J.Y * speed;
            Velocity.X = turned.J.X * speed;
            Velocity.Z = turned.J.Z * speed;

            if (JumpFX[0] != nullptr || JumpFX[1] != nullptr)
            {
                EndJumpFX();
            }
        }

        const float velocityZ = Velocity.Z;
        MCVector3D move;
        move.X = static_cast<float>(static_cast<double>(Velocity.X) * FrameLength * WorldUnitsPerMeter);
        move.Y = Velocity.Y * FrameLength * WorldUnitsPerMeter;
        Velocity.Z = 0.0f;
        move.Z = velocityZ * FrameLength * WorldUnitsPerMeter;

        if (NewMoveChunk != 0)
        {
            // A new move chunk: warp to its first step when too far off.
            if (StatusChunk.JumpOrder == 0)
            {
                const int32_t tileR = MoveChunk.StepPos[0][0];
                MCVector3D stepPos;
                MapTileCellToWorldPos(tileR, MoveChunk.StepPos[0][1], MoveChunk.StepPos[0][2], MoveChunk.StepPos[0][3],
                                      stepPos);
                // Original behaviour (OB-006): measures z against 0, not the mech's elevation.
                const float dx = Position.X - stepPos.X;
                const float dz = -stepPos.Z;
                const float dy = Position.Y - stepPos.Y;

                if (WarpFactor < std::sqrt(dx * dx + dz * dz + dy * dy))
                {
                    move.X = stepPos.X - Position.X;
                    move.Y = stepPos.Y - Position.Y;
                    move.Z = stepPos.Z;
                }

                if (tileR < 0 || GameMap->Height <= tileR || MoveChunk.StepPos[0][1] < 0 ||
                    GameMap->Width <= MoveChunk.StepPos[0][1])
                {
                    Fatal(0, " mech.update: newMoveChunk stepPos not on map! ");
                }
            }

            NewMoveChunk = 0;
        }

        MCVector3D newPosition;
        newPosition.X = move.X + Position.X;
        newPosition.Y = move.Y + Position.Y;
        newPosition.Z = move.Z + Position.Z;
        SetPosition(newPosition);
        DistanceSinceMarkSeen =
            static_cast<float>(std::sqrt((static_cast<double>(move.Y) * move.Y + static_cast<double>(move.Z) * move.Z) +
                                         static_cast<double>(move.X) * move.X) +
                               DistanceSinceMarkSeen);

        if (IsDisabled() == 0)
        {
            UpdatePathLock(1);
        }

        MineCheck();
        Position.Z = Terrain()->GetTerrainElevation(Position);

        // Arms blown off this frame fly off to the side they were on.
        const float facing = FrameFacing(Frame);
        auto* controlData = static_cast<MCMechControlData*>(Control->ControlData);
        auto* mechType = static_cast<MCBattleMechType*>(ObjType);

        if (controlData->BlowRightArm != 0)
        {
            if (0.0f <= facing + TorsoRotation)
            {
                ThrowArm(this, mechType->LeftArmDebrisId, -180.0f);
            }
            else
            {
                ThrowArm(this, mechType->RightArmDebrisId, 0.0f);
            }

            actor->RightArmGone = 1;
        }

        if (controlData->BlowLeftArm != 0)
        {
            if (0.0f <= facing + TorsoRotation)
            {
                ThrowArm(this, mechType->RightArmDebrisId, 0.0f);
            }
            else
            {
                ThrowArm(this, mechType->LeftArmDebrisId, -180.0f);
            }

            actor->LeftArmGone = 1;
        }

        const int visibleNow = OnScreen();

        if (Withdrawing != 0 && visibleNow == 0 && Pilot->Status != 2)
        {
            ObjType->HandleDestruction(this, nullptr);
        }

        if (actor != nullptr)
        {
            actor->Visible = visibleNow;
            actor->SetMovePath(Pilot->GetMovePath());
            int combat = 1;

            if (Pilot->GetLastTarget() == nullptr && Pilot->CurTacOrder.Code != TACTICAL_ORDER_ATTACK_OBJECT &&
                Pilot->CurTacOrder.Code != TACTICAL_ORDER_ATTACK_POINT)
            {
                combat = 0;
            }

            actor->SetCombatMode(combat);
            actor->Update();

            if (IsJumping(nullptr) == 0)
            {
                if (IsDestroyed() == 0 && IsDisabled() == 0)
                {
                    CollisionsOn = 1;
                }
            }
            else
            {
                CollisionsOn = 0;
            }
        }

        // Footprints: each foot prints once when its hot spot packet's frame comes round (within two frames), and
        // is re-armed by the walking gestures once past it.
        if (visibleNow != 0 && FootPrints != 0 && gesture != 20 && IsRevealed() != 0)
        {
            const int32_t gestureNow = actor->CurrentGesture;
            const uint32_t packetIndex = actor->GetHotSpotIndex(static_cast<uint32_t>(gestureNow));
            const int32_t frameNow = actor->CurrentFrame[0];

            if (static_cast<int32_t>(packetIndex) <= static_cast<int32_t>(mechType->NumHotSpotPackets) &&
                mechType->HotSpotData != nullptr)
            {
                const auto* packet = reinterpret_cast<const int32_t*>(mechType->HotSpotData + packetIndex * 0x20);
                const auto* offsets = reinterpret_cast<const float*>(packet);
                const int walking = gestureNow == 4 || gestureNow == 7 || gestureNow == 11;
                // A mirrored actor swaps the feet's offsets. (The original also checks, dead, for a half turn.)
                const int mirrored = actor->Reverse[0] != 0;
                const float* firstOffset = mirrored ? offsets + 1 : offsets + 5;
                const float* secondOffset = mirrored ? offsets + 5 : offsets + 1;

                if (packet[4] + 2 < frameNow || frameNow < packet[4] - 2)
                {
                    if (walking)
                    {
                        SecondStepPrinted = 0;
                    }
                }
                else if (SecondStepPrinted == 0)
                {
                    SecondStepPrinted = 1;
                    const float stepFacing = FrameFacing(Frame);
                    const auto snapped = static_cast<int32_t>(std::floor(static_cast<double>(stepFacing * 0.025f)));
                    const float angle = static_cast<float>(snapped) * 40.0f;
                    MakeFootprint(this, firstOffset[0], firstOffset[1], angle, FootprintDirection(angle));
                }

                if (packet[0] + 2 < frameNow || frameNow < packet[0] - 2)
                {
                    if (walking)
                    {
                        FirstStepPrinted = 0;
                    }
                }
                else if (FirstStepPrinted == 0)
                {
                    FirstStepPrinted = 1;
                    const float stepFacing = FrameFacing(Frame);
                    const int32_t direction = FootprintDirection(stepFacing);
                    const auto snapped = static_cast<int32_t>(std::floor(static_cast<double>(stepFacing * 0.025f)));
                    MakeFootprint(this, secondOffset[0], secondOffset[1], static_cast<float>(snapped) * 40.0f,
                                  direction);
                }
            }
        }

        if (JumpFX[0] != nullptr)
        {
            JumpFX[0]->Update();
        }

        if (JumpFX[1] != nullptr)
        {
            JumpFX[1]->Update();
        }
    }

    for (int32_t i = 0; i < 4; i++)
    {
        if (Smoke[i] == nullptr)
        {
            continue;
        }

        SmokeTime[i] -= FrameLength;

        if (0.0 <= SmokeTime[i])
        {
            Smoke[i]->SetOwner(this);
            Smoke[i]->SetOwnerPosition(GetPositionFromHS(static_cast<uint32_t>(SmokeHotSpot[i])));
            Smoke[i]->OwnerHotSpot = static_cast<uint32_t>(SmokeHotSpot[i]);
            Smoke[i]->SetOwnerVelocity(Velocity);
            Smoke[i]->DepthBias = -50;
            Smoke[i]->Update();
        }
        else
        {
            delete Smoke[i];
            Smoke[i] = nullptr;
        }
    }

    return 1;
}

namespace
{
    /// <summary>A world point on <see cref="Eye"/>'s screen (the camera's inline projection).</summary>
    MCVector2D EyeProject(const MCVector3D& point)
    {
        const float scale = Eye->CameraScale != 1 ? 1.0f : 0.5f;
        const float dy = point.Y - Eye->Position.Y;
        const float dz = point.Z - Eye->Position.Z;
        const float sx = (point.X - Eye->Position.X) * scale;
        const float sy = dy * scale;
        MCVector2D screen;
        screen.X = sx * Eye->CosAngle + sy * Eye->CosAngle + Eye->HalfWidth;
        screen.Y = ((sx * Eye->SinAngle + Eye->HalfHeight) - sy * Eye->SinAngle) - scale * dz;
        return screen;
    }
}

auto MCBattleMech::Render() -> void
{
    if (GamePaused != 0)
    {
        OnScreen();
    }

    if (Withdrawing != 0 && Pilot->Status == 2)
    {
        return;
    }

    auto* actor = static_cast<MCMechActor*>(Appearance);
    int tagged = 0;
    int drawMech = 0;

    if (Alignment == HomeTeam->Alignment)
    {
        if (WindowsVisible == Turn)
        {
            if (GetAwake() == 0)
            {
                if (IsRevealed() != 0)
                {
                    actor->Render(0);
                    drawMech = 1;
                }
            }
            else
            {
                actor->Render(InJump != 0 ? -150 : 0);
                drawMech = 1;
            }
        }
    }
    else
    {
        const int32_t contactType = GetContactType(HomeTeam->Id, tagged);

        if (contactType == 1)
        {
            if (WindowsVisible == Turn)
            {
                actor->Render(InJump != 0 ? -150 : 0);
                drawMech = 1;
            }
        }
        else if (contactType == 2)
        {
            // A sensor contact: a blip sized by tonnage, at the zoom's scale.
            const int zoomedOut = Eye->CameraScale == 1;
            int32_t shapeIndex;
            const char* shapeName;

            if (50.0f < GetTonnage())
            {
                shapeIndex = zoomedOut ? 1 : 0;
                shapeName = zoomedOut ? "mblip1" : "mblip2";
            }
            else if (35.0f < GetTonnage())
            {
                shapeIndex = zoomedOut ? 3 : 2;
                shapeName = zoomedOut ? "mblip3" : "mblip4";
            }
            else
            {
                shapeIndex = zoomedOut ? 5 : 4;
                shapeName = zoomedOut ? "mblip5" : "mblip6";
            }

            uint8_t* shape = Scenario->SensorContactShapes[shapeIndex];

            if (shape != nullptr)
            {
                if (VfxShapeCount(shape) <= BlipFrame)
                {
                    if (SoundSystem != nullptr && UseSound != 0)
                    {
                        SoundSystem->PlayDigitalSample(0x14, 1, this, 0, 1);
                    }

                    BlipFrame = 0;
                }

                ElementList()->OpenGroup(-100000, 1);
                auto* element =
                    ElementList()->Make<MCVfxElement>(shape, ScreenPos.X, ScreenPos.Y, BlipFrame, 0, nullptr, 0);
                ElementList()->Add(element);
                BlipTime = FrameLength + BlipTime;

                if (0.067 < BlipTime)
                {
                    BlipFrame = static_cast<int32_t>(BlipTime * (1.0 / 0.067) + BlipFrame + 0.5);
                    BlipTime = 0.0f;
                }
            }
        }
    }

    if (drawMech != 0)
    {
        for (int32_t i = 0; i < 4; i++)
        {
            if (Smoke[i] != nullptr)
            {
                Smoke[i]->Render();
            }
        }

        if (JumpFX[0] != nullptr)
        {
            JumpFX[0]->Render();
        }

        if (JumpFX[1] != nullptr)
        {
            JumpFX[1]->Render();
        }
    }

    if (DrawTerrainGrid != 0)
    {
        // Debug: the move path's steps as lines.
        MCMovePath* path = Pilot->GetMovePath();
        Assert(path != nullptr, 0, " NULL move path--bad thing ");
        const int32_t numSteps = path->NumSteps;

        for (int32_t i = 0; i < numSteps; i++)
        {
            if (i == numSteps - 1)
            {
                continue;
            }

            MCVector3D from = path->StepList[i].Destination;
            MCVector3D to = path->StepList[i + 1].Destination;
            from.Z = Terrain()->GetTerrainElevation(from);
            to.Z = Terrain()->GetTerrainElevation(to);
            MCVector2D fromScreen = EyeProject(from);
            MCVector2D toScreen = EyeProject(to);
            ElementList()->OpenGroup(-100000, 1);
            ElementList()->Add(ElementList()->Make<MCLineElement>(fromScreen, toScreen, 0xfc, nullptr, -100000, -1));
        }
    }

    // The selected mech's queued orders: waypoint markers, joined by lines when the queue is drawn as a path.
    if (GetCommanderId() == HomeCommander->GetId() && WaypointMarkers != nullptr && Selected != 0 && Pilot != nullptr &&
        Pilot->GetTacOrderQueue(nullptr) > 0)
    {
        MCTacticalOrder tacOrder;
        tacOrder.Init();
        MCQueuedTacOrder queue[MAX_QUEUED_TACORDERS_PER_WARRIOR];
        const int32_t numOrders = Pilot->GetTacOrderQueue(queue);
        MCVector2D fromScreen = EyeProject(Position);
        const int32_t drawLines = DrawOrderLines;

        for (int32_t i = 0; i < numOrders; i++)
        {
            MCVector2D toScreen = EyeProject(queue[i].Point);
            tacOrder.Data[0] = queue[i].PackedData[0];
            tacOrder.Data[1] = queue[i].PackedData[1];
            tacOrder.Unpack();
            int32_t marker;

            if (tacOrder.Code == TACTICAL_ORDER_JUMPTO_POINT || tacOrder.Code == TACTICAL_ORDER_JUMPTO_OBJECT)
            {
                marker = 4;
            }
            else
            {
                marker = tacOrder.MoveParams.WayPath.Mode[0] << 1;
            }

            if (drawLines != 0)
            {
                ElementList()->OpenGroup(-99999, 1);
                ElementList()->Add(
                    ElementList()->Make<MCLineElement>(fromScreen, toScreen, 0xeb, nullptr, -100000, -1));
                fromScreen = toScreen;
                marker++;
            }

            const int32_t bounds = VfxShapeBounds(WaypointMarkers, marker);
            ElementList()->OpenGroup(-100000, 1);
            auto* element = ElementList()->Make<MCVfxElement>(
                WaypointMarkers, static_cast<float>((bounds >> 16) / 2) + toScreen.X,
                toScreen.Y - static_cast<float>(bounds >> 1 & 0x7fff), marker, 1, nullptr, 1);
            ElementList()->Add(element);
        }

        tacOrder.Destroy();
    }
}

auto MCBattleMech::RelFacingTo(MCVector3D goal, int32_t bodyPart) -> float
{
    double facing = MCMover::RelFacingTo(goal, -1);

    switch (bodyPart)
    {
        case 0:
        case 1:
        case 2:
        case 3:
        case 8:
        case 9:
        case 10:
            facing += TorsoRotation;
            break;
        case 4:
            facing += static_cast<double>(LeftArmRotation) + TorsoRotation;
            break;
        case 5:
            facing += static_cast<double>(RightArmRotation) + TorsoRotation;
            break;
        default:
            break;
    }

    if (facing < -180.0)
    {
        return static_cast<float>(facing + 360.0);
    }

    if (facing > 180.0f)
    {
        facing -= 360.0;
    }

    return static_cast<float>(facing);
}

auto MCBattleMech::GetBodyState() -> int32_t
{
    return MechStateByGesture[static_cast<MCMechActor*>(Appearance)->CurrentGesture];
}

auto MCBattleMech::IsWeaponReady(int32_t weaponIndex) -> int
{
    if (Inventory[weaponIndex].Disabled != 0)
    {
        return 0;
    }

    if (ScenarioTime < Inventory[weaponIndex].ReadyTime)
    {
        return 0;
    }

    return 1;
}

auto MCBattleMech::CalcAttackChance(MCGameObject* target, int32_t aimLocation, float targetTime, int32_t weaponIndex,
                                    float modifiers, int32_t* range, MCVector3D* targetPoint) -> float
{
    if (weaponIndex < NumOther || NumOther + NumWeapons <= weaponIndex)
    {
        return -1000.0f;
    }

    if (Pilot != nullptr)
    {
        modifiers += RankVersusChassisCombatModifier[static_cast<int8_t>(Pilot->Rank)][static_cast<int8_t>(MechClass)];
    }

    return MCMover::CalcAttackChance(target, aimLocation, targetTime, weaponIndex, modifiers, range, targetPoint);
}

auto MCBattleMech::CalcHitLocation(MCGameObject* attacker, int32_t weaponIndex, int32_t attackSource,
                                   int32_t attackType) -> int32_t
{
    int32_t row = MechHitSectionTable[attackSource];
    double angle;

    if (attacker == nullptr)
    {
        angle = static_cast<double>(RandomNumber(360) - 180);
    }
    else
    {
        angle = RelFacingTo(attacker->GetPosition(), -1);
    }

    // The side the shot comes from: front, rear, left, right.
    int32_t side;

    if (!(angle < -45.0 || 45.0 < angle))
    {
        side = 0;
    }
    else if (-135.0 < angle && angle < -45.0)
    {
        side = 2;
    }
    else if (45.0 < angle && angle < 135.0f)
    {
        side = 3;
    }
    else
    {
        side = 1;
    }

    if (attackSource == 3)
    {
        // A mech lying down is hit from above or below.
        if (static_cast<MCMechActor*>(GetAppearance())->CurrentStateGesture == 7)
        {
            side = 0;
            row = 1;
        }

        if (static_cast<MCMechActor*>(GetAppearance())->CurrentStateGesture == 8)
        {
            side = 1;
            row = 1;
        }
    }

    int32_t roll = RandomNumber(100);
    int32_t location = 0;

    do
    {
        const int32_t chance = MechHitLocationTable[(side + row * 4) * NUM_MECH_ARMOR_LOCATIONS + location];

        if (roll < chance)
        {
            return location;
        }

        roll -= chance;
        location++;
    } while (location < NUM_MECH_ARMOR_LOCATIONS);

    return location;
}

auto MCBattleMech::TransferHitLocation(int32_t hitLocation) -> int32_t
{
    if (hitLocation < 0 || hitLocation >= NUM_MECH_BODY_LOCATIONS)
    {
        Assert(false, 0, "(hitLocation >= 0) && (hitLocation < NUM_MECH_BODY_LOCATIONS)", "L:\\mcx\\Object\\Mech.cpp");
    }

    return MechTransferHitTable[hitLocation];
}

auto MCBattleMech::StartJump(MCVector3D jumpGoal) -> int32_t
{
    this->JumpGoal.X = jumpGoal.X;
    this->JumpGoal.Z = jumpGoal.Z;
    InJump = 1;
    this->JumpGoal.Y = jumpGoal.Y;
    return 0;
}

auto MCBattleMech::IsJumping(MCVector3D* jumpGoal) -> int
{
    if (jumpGoal != nullptr)
    {
        *jumpGoal = this->JumpGoal;
    }

    return InJump;
}

auto MCBattleMech::GetJumpRange(int32_t* numOffsets, int32_t* jumpCost) -> float
{
    if (numOffsets != nullptr)
    {
        *numOffsets = MechJumpOffsets[NumJumpJets < 7 ? NumJumpJets : 6];
    }

    if (jumpCost != nullptr)
    {
        *jumpCost = NumJumpJets != 0 ? DefaultMechJumpCost : 0;
    }

    return static_cast<float>(static_cast<double>(NumJumpJets) * MCTerrain::MetersPerVertex * (2.0 / 3.0));
}

auto MCBattleMech::HandleEjection() -> int
{
    if (Pilot == nullptr || (Pilot->Status != 0 && Pilot->Status != 1))
    {
        return 1;
    }

    GetPilot()->Eject();
    EjectOrderGiven = 1;
    DestroyBodyLocation(MECH_BODY_LOCATION_HEAD);
    // The ejection seat's beam, from the cockpit hot spot up and away.
    MCGameObject* beam = CreateObject(0x1e4);

    if (beam != nullptr)
    {
        auto* mechType = static_cast<MCBattleMechType*>(ObjType);
        MCVector3D cockpit = GetPositionFromHS(mechType->NumWeapons + 1);
        beam->SetPosition(cockpit);
        cockpit.X = static_cast<float>(cockpit.X - 1000.0);
        cockpit.Y = static_cast<float>(cockpit.Y + 1000.0);
        cockpit.Z = static_cast<float>(cockpit.Z + 300.0);
        static_cast<MCProjectileLaser*>(beam)->Connect(this, cockpit, nullptr,
                                                       static_cast<int32_t>(mechType->NumWeapons + 1));

        if (ObjectList->Head != nullptr)
        {
            ObjectList->Head->AddNode(beam);
        }
    }

    Disable(3);
    TheInterface->RemoveMech(PartId);

    if (Alignment == HomeTeam->Alignment)
    {
        FriendlyDestroyed = 1;
        return 1;
    }

    EnemyDestroyed = 1;
    return 1;
}

auto MCBattleMech::HitInventoryItem(int32_t itemIndex, int setupOnly) -> int
{
    static const char* const locationNames[NUM_MECH_BODY_LOCATIONS] = {"HEAD", "CTORSO", "LTORSO", "RTORSO",
                                                                       "LARM", "RARM",   "LLEG",   "RLEG"};
    MCInventoryItem& item = Inventory[itemIndex];
    item.Health--;
    const uint32_t masterId = item.MasterID;
    const uint32_t location = item.BodyLocation;

    if (GameSystemWindow != nullptr && setupOnly == 0 && location <= 7)
    {
        // Reported only while the location's armor (front and rear, for the torso) still stands.
        int report = 0;

        switch (location)
        {
            case 1:
                report = Armor[1].CurArmor > 0.0f && Armor[8].CurArmor > 0.0f;
                break;
            case 2:
                report = Armor[2].CurArmor > 0.0f && Armor[9].CurArmor > 0.0f;
                break;
            case 3:
                report = Armor[3].CurArmor > 0.0f && Armor[10].CurArmor > 0.0f;
                break;
            default:
                report = Armor[location].CurArmor > 0.0f;
                break;
        }

        if (report != 0)
        {
            char line[200];
            GameSystemWindow->Print(const_cast<char*>(""));
            GameSystemWindow->Print(const_cast<char*>("***********************************"));
            std::snprintf(line, sizeof(line), "INTERNAL COMPONENT HIT: %s (%s)", DebugStatus.c_str(), Pilot->Name);
            GameSystemWindow->Print(line);
            const char* attackerName = BadGuy != nullptr ? static_cast<MCMover*>(BadGuy)->DebugStatus.c_str() : "???";
            std::snprintf(line, sizeof(line), "%s in %s by %s", MasterComponentList[masterId].Name,
                          locationNames[location], attackerName);
            GameSystemWindow->Print(line);
        }
    }

    const MCMasterComponent& component = MasterComponentList[masterId];

    if (component.Form == 3 || component.Form == 0xe)
    {
        PilotingCheck(0, 0.0f);
    }

    const auto disableLevel = static_cast<int8_t>(component.DisableLevel);

    if (GetInventoryDamage(itemIndex) == disableLevel)
    {
        // Disabled: the component stops working; smoke from the hot spot above the weapons it sits nearest.
        int32_t smokeSpot = 1;
        item.Disabled = 1;

        switch (component.Form)
        {
            case 0:
            case 1:
                smokeSpot = 2;
                break;
            case 2:
            {
                if (SensorSystem != nullptr)
                {
                    SensorSystem->Disable();
                }
                break;
            }
            case 4:
            {
                smokeSpot = 1;
                EngineBlowTime = static_cast<float>(ScenarioTime + 5.0);
                Disable(1);
                break;
            }
            case 6:
            case 7:
            case 8:
            case 9:
            {
                CalcWeaponEffectiveness(0);

                if (LongestRangeWeapon == static_cast<uint32_t>(itemIndex) ||
                    ShortestRangeWeapon == static_cast<uint32_t>(itemIndex))
                {
                    CalcLongestRangeWeapon();
                }

                CalcOptimalRange(nullptr);
                [[fallthrough]];
            }
            case 3:
                smokeSpot = 0;
                break;
            case 0xc:
                BodyAt(item.BodyLocation).HasCase = 0;
                break;
            case 0xf:
            case 0x13:
                smokeSpot = 1;
                break;
            case 0x10:
            {
                Team->RemoveEcm(EcmTracker);
                EcmTracker = nullptr;
                break;
            }
            case 0x12:
            {
                Team->RemoveJammer(JammerTracker);
                JammerTracker = nullptr;
                break;
            }
            default:
                break;
        }

        if (setupOnly == 0)
        {
            if (UseSound != 0)
            {
                SoundSystem->PlayDigitalSample(0x13, 1, this, 0, 0);
            }

            MCGameObject* sparks = CreateObject(0x3f);

            if (sparks != nullptr)
            {
                MCVector3D sparkPos =
                    GetPositionFromHS(static_cast<MCBattleMechType*>(ObjType)->NumWeapons + smokeSpot);
                sparks->SetPosition(sparkPos);

                if (ObjectList->Head != nullptr)
                {
                    ObjectList->Head->AddNode(sparks);
                }
            }

            for (int32_t i = 0; i < 4; i++)
            {
                if (Smoke[i] == nullptr)
                {
                    Smoke[i] = static_cast<MCSmoke*>(CreateObject(0x1c2));
                    SmokeHotSpot[i] =
                        RandomNumber(static_cast<int32_t>(static_cast<MCBattleMechType*>(ObjType)->NumWeapons));
                    SmokeTime[i] = 15.0f;
                    break;
                }
            }
        }
    }

    if (Inventory[itemIndex].Health == 0)
    {
        // Destroyed: the cockpit hurts the pilot, a leg actuator trips the mech, ammunition explodes.
        switch (component.Form)
        {
            case 1:
                Pilot->Injure(6.0f, 0);
                break;
            case 3:
            {
                if (location == MECH_BODY_LOCATION_LLEG || location == MECH_BODY_LOCATION_RLEG)
                {
                    PilotingCheck(0, 100.0f);
                    return 0;
                }
                break;
            }
            case 10:
            {
                AmmoExplosion(itemIndex);
                return 0;
            }
            default:
                break;
        }
    }

    return 0;
}

auto MCBattleMech::DestroyBodyLocation(int32_t location) -> void
{
    MCBodyLocation& bodyLocation = BodyAt(location);

    if (bodyLocation.DamageState == 2)
    {
        return;
    }

    bodyLocation.CurInternalStructure = 0.0f;
    bodyLocation.DamageState = 2;

    if (location == MECH_BODY_LOCATION_LLEG || location == MECH_BODY_LOCATION_RLEG)
    {
        CalcLegStatus();
        PilotingCheck(0, 100.0f);
    }
    else if (location == MECH_BODY_LOCATION_CTORSO)
    {
        CalcTorsoStatus();
    }

    // Everything in it is lost.
    for (int32_t i = 0; i < NumLocationCriticalSpaces[location]; i++)
    {
        MCCriticalSpace& space = BodyAt(location).CriticalSpaces[i];

        if (space.Hit == 0 && static_cast<int8_t>(space.InventoryID) != -1)
        {
            space.Hit = 1;
            HitInventoryItem(static_cast<int8_t>(space.InventoryID), 0);
        }
    }

    switch (location)
    {
        case MECH_BODY_LOCATION_CTORSO:
        {
            // The center torso gone destroys the mech, unless it was already ruled dead.
            if (CenterTorsoInjuredTime < ScenarioTime && (EngineBlowTime <= -1.0f || EngineBlowTime < ScenarioTime))
            {
                Disable(0);
                return;
            }

            Pilot->HandleAlarm(6, 0);
            ObjType->HandleDestruction(this, nullptr);
            return;
        }
        case MECH_BODY_LOCATION_RTORSO:
        {
            DestroyBodyLocation(MECH_BODY_LOCATION_RARM);
            return;
        }
        case MECH_BODY_LOCATION_LTORSO:
        {
            DestroyBodyLocation(MECH_BODY_LOCATION_LARM);
            return;
        }
        case MECH_BODY_LOCATION_LARM:
        {
            LeftArmBlownThisFrame = 1;
            return;
        }
        case MECH_BODY_LOCATION_RARM:
        {
            RightArmBlownThisFrame = 1;
            return;
        }
        default:
            return;
    }
}

auto MCBattleMech::CalcCriticalHit(int32_t hitLocation) -> void
{
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        return;
    }

    const int32_t location = MechArmorToBodyLocation[hitLocation];

    // Port fix: the original tests body[hitLocation].totalSpaces, reading past the eight body locations for a rear
    // torso hit (8..10); the body location the hit maps to is tested instead.
    if (BodyAt(location).TotalSpaces == 0)
    {
        return;
    }

    const int32_t numSpaces = NumLocationCriticalSpaces[location];
    const int32_t roll = RandomNumber(100);

    if (roll < CriticalHitTable[0])
    {
        return;
    }

    int32_t numCriticalHits;

    if (roll < CriticalHitTable[1])
    {
        numCriticalHits = 1;
    }
    else if (roll < CriticalHitTable[2])
    {
        numCriticalHits = 2;
    }
    else
    {
        // The worst roll blows a head, arm or leg clean off; a torso takes three hits.
        if (location < MECH_BODY_LOCATION_CTORSO || MECH_BODY_LOCATION_RTORSO < location)
        {
            DestroyBodyLocation(location);

            if (MPlayer != nullptr)
            {
                AddCriticalHitChunk(0, location, 15);
            }

            return;
        }

        numCriticalHits = 3;
    }

    do
    {
        MCBodyLocation& bodyLocation = BodyAt(location);
        int32_t spaceRoll = RandomNumber(bodyLocation.TotalSpaces);
        int32_t space = 0;

        for (; space < numSpaces; space++)
        {
            const uint8_t item = bodyLocation.CriticalSpaces[space].InventoryID;
            int32_t size = 0;

            if (item != 0xff)
            {
                size = static_cast<int8_t>(MasterComponentList[Inventory[item].MasterID].CriticalSpacesReq);
            }

            if (spaceRoll < size)
            {
                break;
            }

            spaceRoll -= size;
        }

        Assert(location >= 0 && location <= 7, static_cast<uint32_t>(location), " Bad bodyLocation in CriticalHit ");
        Assert(space >= 0 && space < NumLocationCriticalSpaces[location], static_cast<uint32_t>(space),
               " Bad Critical Hit Space ");
        MCCriticalSpace& criticalSpace = bodyLocation.CriticalSpaces[space];
        criticalSpace.Hit = 1;
        HitInventoryItem(static_cast<int8_t>(criticalSpace.InventoryID), 0);

        if (MPlayer != nullptr)
        {
            AddCriticalHitChunk(0, location, space);
        }
    } while (--numCriticalHits != 0);
}

auto MCBattleMech::HandleCriticalHit(int32_t bodyLocation, int32_t criticalSpace) -> void
{
    if (criticalSpace == 15)
    {
        DestroyBodyLocation(bodyLocation);
        return;
    }

    HitInventoryItem(static_cast<int8_t>(BodyAt(bodyLocation).CriticalSpaces[criticalSpace].InventoryID), 0);
}

auto MCBattleMech::UpdateCriticalHitChunks(int32_t which) -> int32_t
{
    for (int32_t i = 0; i < NumCriticalHitChunks[which]; i++)
    {
        const uint8_t chunk = CriticalHitChunks[which][i];
        HandleCriticalHit(chunk >> 4, chunk & 0xf);
    }

    NumCriticalHitChunks[which] = 0;
    return 0;
}

auto MCBattleMech::BuildStatusChunk() -> int32_t
{
    StatusChunk.TargetCellRC[0] = -1;
    StatusChunk.TargetCellRC[1] = -1;
    StatusChunk.BodyState = 0;
    StatusChunk.TargetType = 0;
    StatusChunk.TargetId = 0;
    StatusChunk.TargetBlockOrTrainNumber = 0;
    StatusChunk.TargetVertexOrCarNumber = 0;
    StatusChunk.TargetItemNumber = 0;
    StatusChunk.EjectOrderGiven = 0;
    StatusChunk.JumpOrder = 0;
    StatusChunk.Data = 0;

    // The body state: 1 standing up, 2 standing, 3 and 4 fallen, 0 otherwise.
    const uint32_t bodyState = static_cast<uint32_t>(GetBodyState());

    if (static_cast<MCMechActor*>(Appearance)->CurrentGesture == 1 || bodyState < 9)
    {
        switch (bodyState)
        {
            case 1:
                StatusChunk.BodyState = 1;
                break;
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
                StatusChunk.BodyState = 0;
                break;
            case 7:
                StatusChunk.BodyState = 4;
                break;
            case 8:
                StatusChunk.BodyState = 3;
                break;
            default:
                StatusChunk.BodyState = 2;
                break;
        }
    }
    else
    {
        StatusChunk.BodyState = 0;
    }

    if (Pilot != nullptr)
    {
        if (InJump == 0)
        {
            MCGameObject* target = Pilot->GetLastTarget();

            if (target != nullptr)
            {
                const int32_t targetClass = target->ObjectClass;

                switch (targetClass)
                {
                    case 1:
                    case BUILDING:
                    case DEBRIS:
                    case TREE:
                    case TERRAINOBJECT:
                    case 0x17:
                    case MISCTERRAINOBJECT:
                    case JET:
                    case TREEBUILDING:
                    case TURRET:
                    case GATE:
                    case LIGHT:
                    {
                        // A terrain object: its block, vertex and item from the part id.
                        StatusChunk.TargetType = 2;
                        StatusChunk.TargetId = target->PartId;
                        const int32_t terrainPart = target->PartId - 0x1000;
                        StatusChunk.TargetBlockOrTrainNumber = terrainPart / 0xc80;
                        const int32_t inBlock = terrainPart % 0xc80;
                        StatusChunk.TargetVertexOrCarNumber = inBlock / 8;
                        StatusChunk.TargetItemNumber = static_cast<uint8_t>(inBlock % 8);
                        break;
                    }

                    case BATTLEMECH:
                    case GROUNDVEHICLE:
                    case ELEMENTAL:
                    {
                        StatusChunk.TargetType = 1;
                        StatusChunk.TargetId = static_cast<MCMover*>(target)->NetRosterIndex;
                        break;
                    }
                    case CAMERADRONE:
                    {
                        StatusChunk.TargetType = 3;
                        StatusChunk.TargetId = target->PartId;
                        StatusChunk.TargetBlockOrTrainNumber = 0x80;
                        StatusChunk.TargetVertexOrCarNumber = target->PartId - 0x802c8;
                        break;
                    }
                    case TRAINCAR:
                    {
                        StatusChunk.TargetType = 3;
                        StatusChunk.TargetId = target->PartId;
                        const int32_t trainPart = target->PartId - 0x7d000;
                        StatusChunk.TargetBlockOrTrainNumber = trainPart / 100;
                        StatusChunk.TargetVertexOrCarNumber = trainPart % 100;
                        break;
                    }

                    default:
                        Fatal(targetClass, " BattleMech.buildStatusChunk: bad target type ");
                }
            }
        }
        else
        {
            StatusChunk.JumpOrder = 1;
            int32_t cellR = 0;
            int32_t cellC = 0;
            WorldCoordToMapCell(JumpGoal, cellR, cellC);
            StatusChunk.TargetCellRC[0] = static_cast<int16_t>(cellR);
            StatusChunk.TargetCellRC[1] = static_cast<int16_t>(cellC);
        }
    }

    StatusChunk.EjectOrderGiven = EjectOrderGiven;
    StatusChunk.Pack(this);

    // Checks the chunk unpacks to what was packed.
    MCStatusChunk check;
    check.Data = StatusChunk.Data;
    check.MCStatusChunk::Unpack(this);

    if (StatusChunk.EqualTo(&check) == 0)
    {
        Fatal(0, " BAD status chunk in mech: save stchunk.dbg file! ");
    }

    return 0;
}

auto MCBattleMech::HandleStatusChunk(int32_t updateAge, uint32_t chunk) -> int32_t
{
    StatusChunk.TargetCellRC[0] = -1;
    StatusChunk.TargetCellRC[1] = -1;
    StatusChunk.Data = 0;
    StatusChunk.BodyState = 0;
    StatusChunk.TargetType = 0;
    StatusChunk.TargetId = 0;
    StatusChunk.TargetBlockOrTrainNumber = 0;
    StatusChunk.TargetVertexOrCarNumber = 0;
    StatusChunk.TargetItemNumber = 0;
    StatusChunk.EjectOrderGiven = 0;
    StatusChunk.JumpOrder = 0;
    StatusChunk.Data = chunk;
    StatusChunk.Unpack(this);

    if (StatusChunkUnpackErr != 0)
    {
        return 0;
    }

    int32_t targetPartId = 0;

    if (StatusChunk.JumpOrder == 0 && static_cast<int8_t>(StatusChunk.TargetType) > 0)
    {
        if (StatusChunk.TargetType == 1)
        {
            targetPartId = MPlayer->MoverRoster[StatusChunk.TargetId]->PartId;
        }
        else if (StatusChunk.TargetType < 4)
        {
            targetPartId = StatusChunk.TargetId;
        }
    }

    if (Pilot == nullptr)
    {
        return 0;
    }

    MCGameObject* target = nullptr;
    int keepTarget = 0;

    if (targetPartId != 0)
    {
        MCGameObject* lastTarget = Pilot->GetLastTarget();

        if (lastTarget != nullptr && lastTarget->PartId == targetPartId)
        {
            keepTarget = 1;
        }
        else
        {
            target = static_cast<MCGameObject*>(ObjectList->FindObjectFromPart(targetPartId));
        }
    }

    if (keepTarget == 0)
    {
        Pilot->SetLastTarget(target, 0, 0);
    }

    if (EjectOrderGiven == 0 && StatusChunk.EjectOrderGiven != 0)
    {
        EjectOrderGiven = 1;
        HandleEjection();
    }

    return 0;
}

auto MCBattleMech::BuildMoveChunk() -> int32_t
{
    MoveChunk.Init();

    if (Pilot != nullptr)
    {
        Pilot->GetMovePath();
        MoveChunk.Build(this, Pilot->MoveOrders.Path[0], Pilot->MoveOrders.Path[1]);
    }

    MoveChunk.Pack(this);

    // Checks the chunk unpacks to what was packed; a chunk that can't is replaced by an empty one.
    MCMoveChunk check;
    check.StepPos[0][0] = -1;
    check.StepPos[0][1] = -1;
    check.Run = 0;
    check.NumSteps = 0;
    check.Data = MoveChunk.Data;
    check.Unpack(this);

    if (MoveChunkUnpackErr == 0)
    {
        if (MoveChunk.EqualTo(this, &check) == 0)
        {
            Fatal(0, " Bad mech movechunk: save mvchunk.dbg file! ");
        }
    }
    else
    {
        MoveChunk.Init();
        MoveChunk.Build(this, nullptr, nullptr);
        MoveChunk.Pack(this);
    }

    return 0;
}

auto MCBattleMech::HandleMoveChunk(uint32_t chunk) -> int32_t
{
    MoveChunk.Init();
    MoveChunk.Data = chunk;
    MoveChunk.Unpack(this);

    if (MoveChunkUnpackErr == 0)
    {
        MCMovePath* path = GetPilot()->GetMovePath();
        path->SetMoveChunk(&MoveChunk);

        // Skip ahead to the step nearest the mech.
        if (path->NumStepsWhenNotPaused > 1)
        {
            int32_t step = path->NumStepsWhenNotPaused;

            do
            {
                step--;

                if (step < 1)
                {
                    break;
                }
            } while (MapCellDiagonal < DistanceFrom(path->StepList[step].Destination));

            path->CurStep = step;
        }

        NewMoveChunk = 1;
    }

    return 0;
}

auto MCBattleMech::InjureBodyLocation(int32_t bodyLocation, float damage) -> int
{
    MCBodyLocation& location = BodyAt(bodyLocation);

    if (bodyLocation == MECH_BODY_LOCATION_CTORSO && CenterTorsoInjuredTime < 0.0)
    {
        CenterTorsoInjuredTime = ScenarioTime;
    }

    if (damage <= location.CurInternalStructure)
    {
        location.CurInternalStructure -= damage;
    }
    else
    {
        location.CurInternalStructure = 0.0f;
    }

    if (0.0f < location.CurInternalStructure)
    {
        location.DamageState =
            0.5 < location.CurInternalStructure / static_cast<float>(location.MaxInternalStructure) ? 0 : 1;

        if (bodyLocation == MECH_BODY_LOCATION_LLEG || bodyLocation == MECH_BODY_LOCATION_RLEG)
        {
            CalcLegStatus();
        }
        else if (bodyLocation == MECH_BODY_LOCATION_CTORSO)
        {
            CalcTorsoStatus();
            CalcCriticalHit(MECH_BODY_LOCATION_CTORSO);
            return 0;
        }

        CalcCriticalHit(bodyLocation);
        return 0;
    }

    DestroyBodyLocation(bodyLocation);
    return 1;
}

auto MCBattleMech::WeaponLocked(int32_t weaponIndex, MCVector3D targetPosition) -> float
{
    return RelFacingTo(targetPosition, Inventory[weaponIndex].BodyLocation);
}

namespace
{
    /// <summary>The rear armor location behind a body location (the centre's, the left side's or the right's).</summary>
    int32_t RearArmorLocation(int32_t bodyLocation)
    {
        switch (bodyLocation)
        {
            case MECH_BODY_LOCATION_LTORSO:
            case MECH_BODY_LOCATION_LARM:
            case MECH_BODY_LOCATION_LLEG:
                return 9;
            case MECH_BODY_LOCATION_RTORSO:
            case MECH_BODY_LOCATION_RARM:
            case MECH_BODY_LOCATION_RLEG:
                return 10;
            default:
                return 8;
        }
    }

    /// <summary>Passes what is left of a shot on to the location a destroyed one transfers to.</summary>
    void TransferHit(MCBattleMech* mech, MCWeaponShotInfo* shotInfo, int32_t bodyLocation)
    {
        MCWeaponShotInfo transferInfo = *shotInfo;
        transferInfo.HitLocation = mech->TransferHitLocation(bodyLocation);

        if (MPlayer == nullptr)
        {
            mech->HandleWeaponHit(&transferInfo, 0);
        }
        else if (MPlayer->IsServer != 0)
        {
            mech->HandleWeaponHit(&transferInfo, 1);
        }
    }
}

auto MCBattleMech::HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if ((MPlayer == nullptr && CantHitMe != 0 && Pilot->OnHomeTeam() != 0) || shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->AddWeaponHitChunk(this, shotInfo, 0);
    }

    BadGuy = shotInfo->Attacker;

    if (shotInfo->Damage <= 0.0f)
    {
        return 0;
    }

    const int32_t hitLocation = shotInfo->HitLocation;

    if (hitLocation == -1)
    {
        return 0;
    }

    if (IsDestroyed() != 0)
    {
        return 0;
    }

    const MCWeaponShotInfo originalShot = *shotInfo;
    const int wasDisabled = IsDisabled();
    // Ammunition hits (and cause -4) go straight for the internal structure.
    int internalHit = shotInfo->MasterId == -4;

    if (shotInfo->MasterId > 0)
    {
        internalHit = MasterComponentList[shotInfo->MasterId].Form == 10;
    }

    DamageRateTally = shotInfo->Damage + DamageRateTally;
    TotalDamageTaken = shotInfo->Damage + TotalDamageTaken;
    // Which way the mech would fall.
    const float angle = TorsoRotation + shotInfo->EntryAngle;

    if (!(angle < -90.0 || 90.0 < angle))
    {
        HitFromFrontThisFrame = 1;
    }
    else if (angle <= -91.0 || 91.0 <= angle)
    {
        HitFromBehindThisFrame = 1;
    }

    const int32_t bodyLocation = MechArmorToBodyLocation[hitLocation];

    if (bodyLocation == MECH_BODY_LOCATION_HEAD && 2.0f <= shotInfo->Damage)
    {
        Pilot->Injure(1.0f, 1);
    }

    if (Armor[hitLocation].CurArmor <= 0.0f || internalHit != 0)
    {
        MCBodyLocation& location = BodyAt(bodyLocation);
        int caseHit = 0;

        if (location.CurInternalStructure <= 0.0f)
        {
            if (internalHit == 0 || location.HasCase == 0)
            {
                if (bodyLocation != MECH_BODY_LOCATION_CTORSO && bodyLocation != MECH_BODY_LOCATION_HEAD)
                {
                    TransferHit(this, shotInfo, bodyLocation);
                }
            }
            else
            {
                caseHit = 1;
            }
        }
        else if (shotInfo->Damage < location.CurInternalStructure)
        {
            InjureBodyLocation(bodyLocation, shotInfo->Damage);
        }
        else
        {
            const float internalStructure = location.CurInternalStructure;
            shotInfo->SetDamage(shotInfo->Damage - internalStructure);
            InjureBodyLocation(bodyLocation, internalStructure);

            if (0.0f < shotInfo->Damage)
            {
                if (internalHit != 0 && BodyAt(bodyLocation).HasCase != 0)
                {
                    caseHit = 1;
                }
                else if (bodyLocation != MECH_BODY_LOCATION_CTORSO && bodyLocation != MECH_BODY_LOCATION_HEAD)
                {
                    TransferHit(this, shotInfo, bodyLocation);
                }
            }
        }

        if (caseHit != 0)
        {
            // CASE vents the rest out the back.
            const int32_t rear = RearArmorLocation(bodyLocation);
            int holed = 0;

            if (shotInfo->Damage <= Armor[rear].CurArmor)
            {
                Armor[rear].CurArmor -= shotInfo->Damage;
            }
            else
            {
                Armor[rear].CurArmor = 0.0f;
                holed = 1;
            }

            shotInfo->SetDamage(0.0f);

            if (holed != 0)
            {
                PlayMessage(RADIO_ARMOR_HOLED, 0);
            }
        }
    }
    else if (shotInfo->Damage <= Armor[hitLocation].CurArmor)
    {
        Armor[hitLocation].CurArmor -= shotInfo->Damage;
    }
    else
    {
        // Through the armor.
        shotInfo->SetDamage(shotInfo->Damage - Armor[hitLocation].CurArmor);
        Armor[shotInfo->HitLocation].CurArmor = 0.0f;
        const float internalStructure = BodyAt(bodyLocation).CurInternalStructure;

        if (shotInfo->Damage < internalStructure)
        {
            InjureBodyLocation(bodyLocation, shotInfo->Damage);
        }
        else
        {
            shotInfo->SetDamage(shotInfo->Damage - internalStructure);
            InjureBodyLocation(bodyLocation, internalStructure);

            if (0.0f < shotInfo->Damage && bodyLocation != MECH_BODY_LOCATION_CTORSO &&
                bodyLocation != MECH_BODY_LOCATION_HEAD)
            {
                TransferHit(this, shotInfo, bodyLocation);
            }
        }

        PlayMessage(RADIO_ARMOR_HOLED, 0);
    }

    MCGameObject* attacker = shotInfo->Attacker;
    auto triggerId = static_cast<uint32_t>(shotInfo->MasterId);
    int32_t alarm = 1;

    if (attacker == nullptr)
    {
        if (shotInfo->MasterId == -4 || shotInfo->MasterId >= 0)
        {
            triggerId = 0;
        }
    }
    else
    {
        triggerId = static_cast<uint32_t>(attacker->PartId);

        if (shotInfo->MasterId < 0 && shotInfo->MasterId != -4)
        {
            alarm = 10;
        }
    }

    Pilot->TriggerAlarm(alarm, triggerId);
    CurCV = CalcCV(0);

    if (wasDisabled == 0 && IsDisabled() != 0 && attacker != nullptr &&
        (attacker->ObjectClass == BATTLEMECH || attacker->ObjectClass == GROUNDVEHICLE ||
         attacker->ObjectClass == ELEMENTAL || attacker->ObjectClass == MOVER))
    {
        attacker->GetPilot()->TriggerAlarm(12, static_cast<uint32_t>(PartId));
    }

    shotInfo->Init(originalShot.Attacker, originalShot.MasterId, originalShot.Damage, originalShot.HitLocation,
                   originalShot.EntryAngle);
    return 0;
}

namespace
{
    /// <summary>Ammo count that marks a weapon as never running out.</summary>
    constexpr int32_t UNLIMITED_SHOTS = 9999;

    /// <summary>Packs a weapon fire chunk, checks that it unpacks the same, queues it and logs it.</summary>
    void SendWeaponFireChunk(MCBattleMech* mech, MCWeaponFireChunk& chunk, MCGameObject* target)
    {
        chunk.Pack();
        MCWeaponFireChunk check;
        check.Init();
        check.Data = chunk.Data;
        check.Unpack(mech);

        if (chunk.EqualTo(&check) == 0)
        {
            Fatal(0, " Mech.fireWeapon: Bad WeaponFireChunk (save wfchunk.dbg file now) ");
        }

        mech->AddWeaponFireChunk(0, &chunk);
        LogWeaponFireChunk(&chunk, mech, target);
    }

    /// <summary>
    /// Builds and sends the chunk for a shot at <paramref name="target"/> (a mover, train car, camera drone or
    /// terrain object) or, when it is null, at <paramref name="point"/>.
    /// </summary>
    void SendTargetFireChunk(MCBattleMech* mech, MCGameObject* target, MCVector3D* point, int32_t weapon, int hit,
                             float entryAngle, int32_t missiles, int32_t missilesPastAMS, int32_t antiMissileShots,
                             int32_t hitLocation)
    {
        MCWeaponFireChunk chunk;
        chunk.Init();
        auto* bigTarget = static_cast<MCBigGameObject*>(target);

        if (target == nullptr)
        {
            chunk.BuildLocationTarget(*point, weapon, hit, missiles);
        }
        else if (target->ObjectClass == BATTLEMECH || target->ObjectClass == GROUNDVEHICLE ||
                 target->ObjectClass == ELEMENTAL || target->ObjectClass == MOVER)
        {
            chunk.BuildMoverTarget(bigTarget, weapon, hit, entryAngle, missiles, missilesPastAMS, antiMissileShots,
                                   hitLocation);
        }
        else if (target->ObjectClass == TRAINCAR)
        {
            chunk.BuildTrainTarget(bigTarget, weapon, hit, entryAngle, missiles);
        }
        else if (target->ObjectClass == CAMERADRONE)
        {
            chunk.BuildCameraDroneTarget(bigTarget, weapon, hit, entryAngle, missiles);
        }
        else
        {
            chunk.BuildTerrainTarget(bigTarget, weapon, hit, missiles);
        }

        SendWeaponFireChunk(mech, chunk, target);
    }

    /// <summary>A shot's damage must survive the chunk's quarter-point rounding.</summary>
    void CheckDamageRound(const MCWeaponShotInfo& shot)
    {
        const auto quarters = static_cast<int32_t>(shot.Damage * 4.0);
        Assert(shot.Damage == quarters * 0.25 ? 1 : 0, 0, " WeaponHitChunk.build: damage round error ");
    }

    /// <summary>A shot with no effect object sets off a live mine where it lands.</summary>
    void CheckMineAt(MCVector3D& point)
    {
        int32_t tileR = 0;
        int32_t tileC = 0;
        int32_t cellR = 0;
        int32_t cellC = 0;
        GameMap->WorldToMapPos(point, tileR, tileC, cellR, cellC);

        // Port fix: a miss can land off the map, where the original reads (and writes) outside it.
        if (!GameMap->OnMap(tileR, tileC))
        {
            return;
        }

        MCMapTile& tile = GameMap->Map[GameMap->Width * tileR + tileC];

        if ((tile.Overlay & 0x1800) == 0x1000 || (tile.Overlay & 0x6000) == 0x4000)
        {
            CreateExplosion(MineExplosion, point, MineSplashDamage, WorldUnitsPerMeter * MineSplashRange);
            tile.Overlay |= 0x1800;
            tile.Overlay |= 0x6000;
        }
    }

    /// <summary>
    /// Sends a weapon effect on its way, at <paramref name="target"/> (from hot spot to hot spot) or, when it is
    /// null, at <paramref name="point"/>, carrying <paramref name="shot"/>; then adds it to the weapon list.
    /// </summary>
    void LaunchWeaponFX(MCBattleMech* mech, MCGameObject* fx, MCGameObject* target, MCVector3D* point,
                        MCWeaponShotInfo& shot, int32_t sourceHotSpot, int32_t targetHotSpot)
    {
        if (fx->ObjectClass == BULLET)
        {
            auto* bullet = static_cast<MCBullet*>(fx);

            if (bullet->NumShots != 5)
            {
                bullet->ShotInfo[bullet->NumShots++].Init(shot.Attacker, shot.MasterId, shot.Damage, shot.HitLocation,
                                                          shot.EntryAngle);
            }

            if (target == nullptr)
            {
                bullet->Connect(mech, *point, sourceHotSpot);
            }
            else
            {
                bullet->Owner = mech;
                bullet->Target = target;
                bullet->OwnerHotSpot = sourceHotSpot;
                bullet->TargetHotSpot = targetHotSpot;
            }
        }
        else if (fx->ObjectClass == LASER)
        {
            auto* laser = static_cast<MCLaser*>(fx);

            if (target == nullptr)
            {
                laser->Connect(mech, *point, &shot, sourceHotSpot);
            }
            else
            {
                laser->Source.SetWatcher(mech);
                laser->Target.SetWatcher(target);
                laser->SourceHotSpot = sourceHotSpot;
                laser->TargetHotSpot = targetHotSpot;
                laser->ShotInfo.Init(shot.Attacker, shot.MasterId, shot.Damage, shot.HitLocation, shot.EntryAngle);
            }
        }
        else
        {
            auto* projectile = static_cast<MCProjectileLaser*>(fx);

            if (target == nullptr)
            {
                projectile->Connect(mech, *point, &shot, sourceHotSpot);
            }
            else
            {
                projectile->Owner = mech;
                projectile->Target = target;
                projectile->OwnerHotSpot = sourceHotSpot;
                projectile->TargetHotSpot = targetHotSpot;
                projectile->ShotInfo.Init(shot.Attacker, shot.MasterId, shot.Damage, shot.HitLocation, shot.EntryAngle);
            }
        }

        WeaponList->AddNode(fx);
    }

    /// <summary>Firing gives a mech away to the other side's mechs within visual range.</summary>
    void RevealFiring(MCBattleMech* mech)
    {
        MCObjectQueueNode* enemies = nullptr;
        uint8_t seenBy = 0;

        if (mech->Alignment == 1)
        {
            enemies = ClanMechList;
            seenBy = 2;
        }
        else if (mech->Alignment == -1)
        {
            enemies = InnerSphereMechList;
            seenBy = 1;
        }

        if (enemies == nullptr)
        {
            return;
        }

        for (MCBaseObject* enemy = enemies->Head; enemy != nullptr; enemy = enemy->Next)
        {
            MCVector3D enemyPosition = static_cast<MCGameObject*>(enemy)->GetPosition();

            if (mech->DistanceFrom(enemyPosition) < Scenario->MaxVisualRange)
            {
                Terrain()->MarkRadiusSeen(mech->Position, mech->Frame.J, 360.0f, Scenario->FireVisualRange, seenBy);
                return;
            }
        }
    }

    /// <summary>Where a missed shot lands: scattered up to <paramref name="scatter"/> about the aim point.</summary>
    /// <param name="centred">Missiles scatter both ways; other shots (as the original computes them) only one.</param>
    MCVector3D MissPoint(MCGameObject* target, MCVector3D* targetPoint, float scatter, int centred)
    {
        MCVector3D miss;
        miss.X = scatter;
        miss.Y = scatter;
        miss.Z = 0.0f;
        const auto offsetX = static_cast<float>(RandomNumber(static_cast<int32_t>(miss.X + miss.X)) - miss.X);
        const auto offsetY = static_cast<float>(RandomNumber(static_cast<int32_t>(miss.Y + miss.Y)) - miss.Y);
        const auto offsetZ = static_cast<float>(RandomNumber(static_cast<int32_t>(miss.Z + miss.Z)) - miss.Z);

        if (centred != 0)
        {
            miss.X = offsetX;
            miss.Y = offsetY;
        }
        else
        {
            miss.X = miss.X + offsetX;
            miss.Y = miss.Y + offsetY;
        }

        miss.Z = miss.Z + offsetZ;
        const MCVector3D base = target != nullptr ? target->GetPosition() : *targetPoint;
        miss.X += base.X;
        miss.Y += base.Y;
        miss.Z += base.Z;
        return miss;
    }
}

auto MCBattleMech::FireWeapon(MCGameObject* target, float targetTime, int32_t weaponIndex, int32_t attackType,
                              int32_t aimLocation, MCVector3D* targetPoint) -> int32_t
{
    if (Status == 5 || Status == 4 || Status == 1 || Status == 2 || GetBodyState() == 0 || GetBodyState() == 7 ||
        GetBodyState() == 8)
    {
        return 1;
    }

    if (IsWeaponReady(weaponIndex) == 0)
    {
        return 3;
    }

    float distance;

    if (target == nullptr)
    {
        if (targetPoint == nullptr || LineOfSight(*targetPoint) == 0)
        {
            return 4;
        }

        distance = static_cast<float>(DistanceFrom(*targetPoint));
    }
    else
    {
        // A camera drone can't be shot for two seconds after launch.
        if (target->ObjectClass == CAMERADRONE && ScenarioTime < static_cast<MCCameraDrone*>(target)->LaunchTime + 2.0)
        {
            return 4;
        }

        if (target->IsDestroyed() != 0)
        {
            return 4;
        }

        if (LineOfSight(target) == 0)
        {
            return 4;
        }

        MCVector3D targetPosition = target->GetPosition();
        distance = static_cast<float>(DistanceFrom(targetPosition));
    }

    const int32_t inRange = WeaponInRange(weaponIndex, distance);

    if ((MPlayer == nullptr || MPlayer->IsServer != 0) && inRange == 0)
    {
        return 4;
    }

    const MCMasterComponent& weapon = MasterComponentList[Inventory[weaponIndex].MasterID];

    if (weapon.MissileType != 2 && weapon.MissileType != 1 && weapon.MissileType != 3)
    {
        // Direct fire needs a clear line.
        if (target == nullptr)
        {
            if (targetPoint == nullptr || LineOfFire(*targetPoint) == 0)
            {
                return 4;
            }
        }
        else if (LineOfFire(target) == 0)
        {
            return 4;
        }
    }

    const int32_t numShots = GetWeaponShots(weaponIndex);

    if (numShots == 0)
    {
        return 4;
    }

    float entryAngle = 0.0f;

    if (target != nullptr)
    {
        entryAngle = target->RelFacingTo(Position, -1);
    }

    const int isStreak = weapon.WeaponFlags & 1;
    int32_t range = 0;
    int32_t hitChance =
        static_cast<int32_t>(CalcAttackChance(target, aimLocation, targetTime, weaponIndex, 0.0f, &range, targetPoint));
    const int32_t hitRoll = RandomNumber(100);

    if (target != nullptr)
    {
        float points = SkillTry[MWS_GUNNERY];

        if (target->GetAlignment() == -1)
        {
            Pilot->NumSkillUses[MWS_GUNNERY][1]++;
        }
        else
        {
            points = SkillTry[MWS_GUNNERY] * 0.1f;
        }

        Pilot->SkillPoints[MWS_GUNNERY] = points + Pilot->SkillPoints[MWS_GUNNERY];
    }

    // Aimed shots only from a standing mech.
    if (aimLocation != -1 && 0.0 < GetVelocity().Magnitude())
    {
        hitChance = 0;
    }

    int32_t hitLocation = -2;

    if (target != nullptr && hitRoll < hitChance)
    {
        float points = SkillSuccess[MWS_GUNNERY];

        if (target->GetAlignment() == -1)
        {
            Pilot->NumSkillSuccesses[MWS_GUNNERY][1]++;
        }
        else
        {
            points = SkillSuccess[MWS_GUNNERY] * 0.1f;
        }

        Pilot->SkillPoints[MWS_GUNNERY] = points + Pilot->SkillPoints[MWS_GUNNERY];

        if (aimLocation != -1)
        {
            hitLocation = aimLocation;
        }
    }

    MCMechWarrior* targetPilot = nullptr;

    if (target != nullptr && (target->ObjectClass == BATTLEMECH || target->ObjectClass == GROUNDVEHICLE ||
                              target->ObjectClass == ELEMENTAL || target->ObjectClass == MOVER))
    {
        targetPilot = target->GetPilot();
        targetPilot->UpdateAttackerStatus(static_cast<uint32_t>(PartId), ScenarioTime);
    }

    StartWeaponRecycle(weaponIndex);

    const int32_t chunkWeapon = weaponIndex - NumOther;

    if (hitRoll < hitChance)
    {
        if (numShots != UNLIMITED_SHOTS)
        {
            DeductWeaponShot(weaponIndex, 1);
        }

        MCInventoryItem& item = Inventory[weaponIndex];
        const MCMasterComponent& fired = MasterComponentList[item.MasterID];

        if (fired.Form == 9)
        {
            // Missiles: a streak fires them all, anything else about half; anti-missile systems take some out.
            const int32_t rackSize = fired.NumMissiles;
            int32_t missiles = rackSize;

            if (isStreak == 0)
            {
                missiles = static_cast<int32_t>((rackSize + 1.0) * 0.5);

                if (missiles < 1)
                {
                    missiles = 1;
                }

                if (rackSize < missiles)
                {
                    missiles = rackSize;
                }
            }

            int32_t antiMissileShots = 0;
            int32_t missilesLeft = missiles;

            if (target != nullptr)
            {
                missilesLeft = target->FireAntiMissileSystem(missiles, antiMissileShots);

                if (antiMissileShots > 0)
                {
                    target->ReduceAntiMissileAmmo(antiMissileShots);
                }
            }

            int32_t targetHotSpot = 0;
            const uint8_t weaponEffect = fired.WeaponEffect;
            const int32_t sourceHotSpot = BodyAt(item.BodyLocation).HotSpotNumber;

            if (missilesLeft > 0)
            {
                if (target == nullptr)
                {
                    hitLocation = -1;
                }
                else
                {
                    if (aimLocation == -1)
                    {
                        hitLocation = target->CalcHitLocation(this, weaponIndex, 0, attackType);
                    }

                    if (target->ObjectClass == BATTLEMECH)
                    {
                        // Port fix: the original reads body[hitLocation], past the eight body locations for a rear torso hit
                        // (8..10); the torso it maps to is read instead.
                        targetHotSpot = static_cast<MCBattleMech*>(target)
                                            ->BodyAt(MechArmorToBodyLocation[hitLocation])
                                            .HotSpotNumber;
                    }
                }

                Assert(hitLocation != -2 ? 1 : 0, 0, " Mech.FireWeapon: Bad Hit Location ");
                MCWeaponShotInfo shot;
                shot.Init(this, item.MasterID, fired.Damage * static_cast<float>(missilesLeft), hitLocation,
                          entryAngle);
                CheckDamageRound(shot);

                if (MPlayer != nullptr && MPlayer->IsServer != 0)
                {
                    SendTargetFireChunk(this, target, targetPoint, chunkWeapon, 1, entryAngle, missiles, missilesLeft,
                                        antiMissileShots, hitLocation);
                }

                MCGameObject* fx = CreateObject(static_cast<int32_t>(WeaponFXTable[weaponEffect]));

                if (fx == nullptr)
                {
                    if (targetPoint != nullptr)
                    {
                        CheckMineAt(*targetPoint);
                    }
                }
                else
                {
                    LaunchWeaponFX(this, fx, target, targetPoint, shot, sourceHotSpot, targetHotSpot);

                    if (target == nullptr)
                    {
                        Pilot->ClearCurTacOrder(1, 0);
                    }
                }
            }
        }
        else
        {
            if (target == nullptr)
            {
                hitLocation = -1;
            }
            else if (aimLocation == -1)
            {
                hitLocation = target->CalcHitLocation(this, weaponIndex, 0, attackType);
            }

            Assert(hitLocation != -2 ? 1 : 0, 1, " Mech.FireWeapon: Bad Hit Location ");
            MCWeaponShotInfo shot;
            shot.Init(this, item.MasterID, fired.Damage, hitLocation, entryAngle);

            if (MPlayer != nullptr && MPlayer->IsServer != 0)
            {
                SendTargetFireChunk(this, target, targetPoint, chunkWeapon, 1, entryAngle, 0, 0, 0, hitLocation);
            }

            MCGameObject* fx = CreateObject(static_cast<int32_t>(WeaponFXTable[fired.WeaponEffect]));

            if (fx == nullptr)
            {
                if (targetPoint != nullptr)
                {
                    CheckMineAt(*targetPoint);
                }
            }
            else
            {
                const int32_t sourceHotSpot = BodyAt(item.BodyLocation).HotSpotNumber;
                int32_t targetHotSpot = 0;

                if (target != nullptr && target->ObjectClass == BATTLEMECH)
                {
                    // Port fix: the original reads body[hitLocation], past the eight body locations for a rear torso hit
                    // (8..10); the torso it maps to is read instead.
                    targetHotSpot =
                        static_cast<MCBattleMech*>(target)->BodyAt(MechArmorToBodyLocation[hitLocation]).HotSpotNumber;
                }

                LaunchWeaponFX(this, fx, target, targetPoint, shot, sourceHotSpot, targetHotSpot);
            }

            if (target == nullptr)
            {
                Pilot->ClearCurTacOrder(1, 0);
            }
        }
    }
    else if (isStreak == 0)
    {
        // A miss (a streak doesn't fire without a lock): the shot lands somewhere near.
        if (numShots != UNLIMITED_SHOTS)
        {
            DeductWeaponShot(weaponIndex, 1);
        }

        MCInventoryItem& item = Inventory[weaponIndex];
        const MCMasterComponent& fired = MasterComponentList[item.MasterID];
        const float scatter = target != nullptr ? 25.0f : 5.0f;
        MCWeaponShotInfo shot;
        MCVector3D landing;
        int launch = 1;

        if (fired.Form == 9)
        {
            const int32_t rackSize = fired.NumMissiles;
            int32_t missiles = static_cast<int32_t>(rackSize * 0.5 + 0.5);

            if (missiles < 1)
            {
                missiles = 1;
            }

            if (rackSize < missiles)
            {
                missiles = rackSize;
            }

            if (missiles > 0)
            {
                shot.Init(this, item.MasterID, fired.Damage * static_cast<float>(missiles), -1, entryAngle);
                CheckDamageRound(shot);
                landing = MissPoint(target, targetPoint, scatter, 1);

                if (MPlayer != nullptr && MPlayer->IsServer != 0)
                {
                    SendTargetFireChunk(this, nullptr, &landing, chunkWeapon, 0, 0.0f, missiles, 0, 0, 0);
                }
            }
            else
            {
                launch = 0;
            }
        }
        else
        {
            shot.Init(this, item.MasterID, fired.Damage, -1, entryAngle);
            landing = MissPoint(target, targetPoint, scatter, 0);

            if (MPlayer != nullptr && MPlayer->IsServer != 0)
            {
                SendTargetFireChunk(this, nullptr, &landing, chunkWeapon, 0, 0.0f, 0, 0, 0, 0);
            }
        }

        if (launch != 0)
        {
            MCGameObject* fx = CreateObject(static_cast<int32_t>(WeaponFXTable[fired.WeaponEffect]));

            if (fx != nullptr)
            {
                LaunchWeaponFX(this, fx, nullptr, &landing, shot, BodyAt(item.BodyLocation).HotSpotNumber, 0);
            }
            else
            {
                CheckMineAt(landing);
            }
        }
    }

    if (targetPilot != nullptr)
    {
        targetPilot->TriggerAlarm(0, static_cast<uint32_t>(PartId));
    }

    RevealFiring(this);

    if (Group != nullptr)
    {
        Group->HandleMateFiredWeapon(static_cast<uint32_t>(PartId));
    }

    return 0;
}

namespace
{
    /// <summary>Weapon effects in flight beyond which a network shot shows none.</summary>
    constexpr int32_t MAX_NETWORK_WEAPON_FX = 200;

    /// <summary>How many weapon effects are in flight.</summary>
    int32_t CountWeaponFX()
    {
        int32_t count = 0;

        for (MCBaseObject* fx = WeaponList->Head; fx != nullptr; fx = fx->Next)
        {
            count++;
        }

        return count;
    }
}

auto MCBattleMech::HandleWeaponFire(int32_t weaponIndex, MCGameObject* target, MCVector3D* targetPoint, int hit,
                                    float entryAngle, int32_t numMissiles, int32_t missilesPastAMS,
                                    int32_t antiMissileShots, int32_t hitLocation) -> int32_t
{
    const int32_t numShots = GetWeaponShots(weaponIndex);
    StartWeaponRecycle(weaponIndex);
    MCInventoryItem& item = Inventory[weaponIndex];
    const int isStreak = MasterComponentList[item.MasterID].WeaponFlags & 1;
    const MCMasterComponent& fired = MasterComponentList[item.MasterID];
    MCWeaponShotInfo shot;

    if (hit == 0)
    {
        Assert(target == nullptr ? 1 : 0, 0, " Mech.handleWeaponFire: target should be NULL with network miss! ");
        Assert(targetPoint != nullptr ? 1 : 0, 0, " Mech.handleWeaponFire: MUST have targetpoint with network miss! ");

        if (isStreak != 0)
        {
            CurMoverWeaponFireChunk.Unpack(this);
            DebugWeaponFireChunk(&CurMoverWeaponFireChunk, nullptr, this);
            Assert(0, 0, " Mech.handleWeaponFire: streaks shouldn't miss! ");
        }

        if (numShots != 9999)
        {
            DeductWeaponShot(weaponIndex, 1);
        }

        if (fired.Form == 9)
        {
            if (numMissiles != 0)
            {
                MCGameObject* fx = nullptr;

                if (CountWeaponFX() < MAX_NETWORK_WEAPON_FX)
                {
                    fx = CreateObject(static_cast<int32_t>(WeaponFXTable[fired.WeaponEffect]));
                }

                if (fx != nullptr)
                {
                    const int32_t sourceHotSpot = BodyAt(item.BodyLocation).HotSpotNumber;
                    shot.Init(this, item.MasterID, fired.Damage * static_cast<float>(numMissiles), -1, entryAngle);
                    CheckDamageRound(shot);
                    LaunchWeaponFX(this, fx, nullptr, targetPoint, shot, sourceHotSpot, 0);
                }
                else if (targetPoint != nullptr)
                {
                    CheckMineAt(*targetPoint);
                }
            }
        }
        else
        {
            shot.Init(this, item.MasterID, fired.Damage, -1, entryAngle);
            MCGameObject* fx = nullptr;

            if (CountWeaponFX() < MAX_NETWORK_WEAPON_FX)
            {
                fx = CreateObject(static_cast<int32_t>(WeaponFXTable[fired.WeaponEffect]));
            }

            if (fx != nullptr)
            {
                LaunchWeaponFX(this, fx, nullptr, targetPoint, shot, BodyAt(item.BodyLocation).HotSpotNumber, 0);
            }
            else if (targetPoint != nullptr)
            {
                CheckMineAt(*targetPoint);
            }
        }
    }
    else
    {
        if (numShots != 9999)
        {
            DeductWeaponShot(weaponIndex, 1);
        }

        if (fired.Form == 9)
        {
            if (antiMissileShots > 0)
            {
                target->ReduceAntiMissileAmmo(antiMissileShots);
            }

            int32_t targetHotSpot = 0;
            const int32_t sourceHotSpot = BodyAt(item.BodyLocation).HotSpotNumber;

            if (missilesPastAMS > 0)
            {
                MCGameObject* fx = nullptr;

                if (CountWeaponFX() < MAX_NETWORK_WEAPON_FX)
                {
                    fx = CreateObject(static_cast<int32_t>(WeaponFXTable[fired.WeaponEffect]));
                }

                if (fx != nullptr)
                {
                    Assert(hitLocation != -2 ? 1 : 0, static_cast<uint32_t>(TargetRolo),
                           " Mech.handleWeaponFire: Bad Hit Location ");

                    if (target != nullptr && target->ObjectClass == BATTLEMECH)
                    {
                        // Port fix: the original reads body[hitLocation], past the eight body locations for a rear torso hit
                        // (8..10); the torso it maps to is read instead.
                        targetHotSpot = static_cast<MCBattleMech*>(target)
                                            ->BodyAt(MechArmorToBodyLocation[hitLocation])
                                            .HotSpotNumber;
                    }

                    shot.Init(this, item.MasterID, fired.Damage * static_cast<float>(missilesPastAMS), hitLocation,
                              entryAngle);
                    CheckDamageRound(shot);
                    LaunchWeaponFX(this, fx, target, targetPoint, shot, sourceHotSpot, targetHotSpot);

                    if (target == nullptr)
                    {
                        Pilot->ClearCurTacOrder(1, 0);
                    }
                }
                else if (targetPoint != nullptr)
                {
                    CheckMineAt(*targetPoint);
                }
            }
        }
        else
        {
            shot.Init(this, item.MasterID, fired.Damage, hitLocation, entryAngle);
            MCGameObject* fx = nullptr;

            if (CountWeaponFX() < MAX_NETWORK_WEAPON_FX)
            {
                fx = CreateObject(static_cast<int32_t>(WeaponFXTable[fired.WeaponEffect]));
            }

            if (fx != nullptr)
            {
                const int32_t sourceHotSpot = BodyAt(item.BodyLocation).HotSpotNumber;
                int32_t targetHotSpot = 0;

                if (target != nullptr && target->ObjectClass == BATTLEMECH)
                {
                    // Port fix: the original reads body[hitLocation], past the eight body locations for a rear torso hit
                    // (8..10); the torso it maps to is read instead.
                    targetHotSpot =
                        static_cast<MCBattleMech*>(target)->BodyAt(MechArmorToBodyLocation[hitLocation]).HotSpotNumber;
                }

                LaunchWeaponFX(this, fx, target, targetPoint, shot, sourceHotSpot, targetHotSpot);

                if (target == nullptr)
                {
                    Pilot->ClearCurTacOrder(1, 0);
                }
            }
            else if (targetPoint != nullptr)
            {
                CheckMineAt(*targetPoint);
            }
        }
    }

    if (target != nullptr && (target->ObjectClass == BATTLEMECH || target->ObjectClass == GROUNDVEHICLE ||
                              target->ObjectClass == ELEMENTAL || target->ObjectClass == MOVER))
    {
        MCMechWarrior* targetPilot = target->GetPilot();
        targetPilot->UpdateAttackerStatus(static_cast<uint32_t>(PartId), ScenarioTime);
        targetPilot->TriggerAlarm(0, static_cast<uint32_t>(PartId));
    }

    RevealFiring(this);

    if (Group != nullptr)
    {
        Group->HandleMateFiredWeapon(static_cast<uint32_t>(PartId));
    }

    return 0;
}

auto MCBattleMech::CalcMaxSpeed() -> float
{
    auto* actor = static_cast<MCMechActor*>(Appearance);

    if (LegStatus == 0)
    {
        return actor->GetVelocityOfGesture(7);
    }

    if (LegStatus < 2)
    {
        return actor->GetVelocityOfGesture(4);
    }

    if (LegStatus == 2)
    {
        return actor->GetVelocityOfGesture(11);
    }

    return 0.0f;
}

auto MCBattleMech::CalcSlowSpeed() -> float
{
    if (LegStatus < 2)
    {
        return static_cast<float>(MaxRunSpeed * 0.25);
    }

    if (LegStatus == 2)
    {
        return static_cast<float>(MaxRunSpeed * 0.2);
    }

    return 0.0f;
}

auto MCBattleMech::CalcModerateSpeed() -> float
{
    if (LegStatus < 2)
    {
        return static_cast<float>(MaxRunSpeed * 0.4);
    }

    if (LegStatus == 2)
    {
        return static_cast<float>(MaxRunSpeed * 0.3);
    }

    return 0.0f;
}

auto MCBattleMech::CalcSpriteSpeed(float speed, uint32_t flags, int32_t& state, int32_t& throttle) -> int32_t
{
    auto* actor = static_cast<MCMechActor*>(Appearance);
    state = 3;
    throttle = 100;
    const float walkSpeed = actor->GetVelocityOfGesture(4);
    const float runSpeed = actor->GetVelocityOfGesture(7);

    if (speed == 0.0)
    {
        state = 1;
        return 0;
    }

    if (speed < walkSpeed * 0.5)
    {
        state = 2;
        throttle = 50;
        return 1;
    }

    if (speed <= walkSpeed)
    {
        state = 2;
        throttle = static_cast<int32_t>(static_cast<double>(speed) / walkSpeed * 100.0);
        return 0;
    }

    if (speed < runSpeed)
    {
        if ((flags & 1) != 0)
        {
            state = 2;
            throttle = static_cast<int32_t>(static_cast<double>(speed) / walkSpeed * 100.0);
            return 2;
        }

        state = 3;
        return 2;
    }

    if (runSpeed < speed)
    {
        state = 3;
        return 3;
    }

    return 0;
}

auto MCBattleMech::OpenStatusWindow(int32_t left, int32_t top, int32_t right, int32_t bottom) -> int32_t
{
    auto* window = new MCMechStatusWindow;
    StatusWindow = window;
    window->Init(left, top, right, bottom, this);
    StatusWindow->SetBackColor(0);
    StatusWindow->Draw();
    ScreenWindow->AddChild(StatusWindow);

    if (Pilot != nullptr)
    {
        Pilot->OpenStatusWindow(left + 30, top + 30, right, bottom);
    }

    return 0;
}

MCMechStatusWindow::~MCMechStatusWindow()
{
    // The inline ~aTitleWindow.
    MCGuiTitleWindow::Destroy();
}

auto MCBattleMech::CloseStatusWindow() -> int32_t
{
    if (Pilot != nullptr)
    {
        Pilot->CloseStatusWindow();
    }

    // The window is destroyed, not deleted.
    StatusWindow->Destroy();
    StatusWindow = nullptr;
    return 0;
}

auto MCBattleMech::GetVitalInfo(void* vitalInfo) -> int32_t
{
    const int32_t size = MCMover::GetVitalInfo(nullptr);

    if (vitalInfo != nullptr)
    {
        MCMover::GetVitalInfo(vitalInfo);
    }

    return size + 6;
}

auto MCBattleMech::IsCaptureable() -> int
{
    if (Captureable != 0 && Alignment == HomeTeam->Alignment && IsDestroyed() == 0)
    {
        return 1;
    }

    return 0;
}

namespace
{
    /// <summary>
    /// A weapon's damage per ten seconds: its damage times the missiles that land (in whole clusters for SRMs and
    /// LRMs, half the rack), over its recycle time.
    /// </summary>
    float WeaponDamageRate(const MCMasterComponent& weapon)
    {
        int32_t clusterSize = 1;
        int32_t numClusters = 1;

        if (weapon.Form == 9 && (weapon.MissileType == 1 || weapon.MissileType == 2))
        {
            clusterSize = weapon.MissileType == 1 ? ClusterSizeSrm : ClusterSizeLrm;
            numClusters = weapon.NumMissiles / 2 / clusterSize;

            if (weapon.NumMissiles / 2 % clusterSize != 0)
            {
                numClusters++;
            }
        }

        return static_cast<float>(clusterSize * numClusters) * weapon.Damage * 10.0f / weapon.RecycleTime;
    }
}

auto MCBattleMech::CalcMaxTargetDamage() -> float
{
    float total = 0.0f;

    for (int32_t i = NumOther; i < NumOther + NumWeapons; i++)
    {
        const float damage = WeaponDamageRate(MasterComponentList[Inventory[i].MasterID]) * 100.0f;

        if (0.0f < damage)
        {
            total = damage + total;
        }
    }

    MaxTargetDamage = total;
    return total;
}

auto MCBattleMech::CalcExpectedTargetDamage(MCGameObject* target) -> float
{
    float total = 0.0f;

    if (GetPilot() == nullptr)
    {
        return 0.0f;
    }

    MCGameObject* aimTarget;
    float targetTime;

    if (target == nullptr)
    {
        aimTarget = GetPilot()->GetLastTarget();

        if (aimTarget == nullptr)
        {
            return 0.0f;
        }

        targetTime = GetPilot()->LastTargetTime;
    }
    else
    {
        targetTime = GetPilot()->GetLastTarget() == target ? GetPilot()->LastTargetTime : 0.0f;
        aimTarget = target;
    }

    MCVector3D targetPosition = aimTarget->GetPosition();
    const auto distance = static_cast<float>(DistanceFrom(targetPosition));

    if (GetFireRange(-2) < distance)
    {
        return 0.0f;
    }

    for (int32_t i = NumOther; i < NumOther + NumWeapons; i++)
    {
        if (IsWeaponWorking(i) == 0)
        {
            continue;
        }

        const float damageRate = WeaponDamageRate(MasterComponentList[Inventory[i].MasterID]);
        int32_t aimLocation = -1;

        if (Pilot != nullptr && Pilot->CurTacOrder.IsCombatOrder() != 0)
        {
            aimLocation = Pilot->CurTacOrder.AttackParams.AimLocation;
        }

        int32_t range = 0;
        const double expected =
            static_cast<double>(CalcAttackChance(aimTarget, aimLocation, targetTime, i, 0.0f, &range, nullptr)) *
            damageRate;

        if (0.0 < expected)
        {
            total = static_cast<float>(expected + total);
        }
    }

    MaxTargetDamage = total;
    return total;
}

auto MCBattleMech::IsWeaponWorking(int32_t weaponIndex) -> int
{
    if (Inventory[weaponIndex].Disabled != 0)
    {
        return 0;
    }

    return GetWeaponShots(weaponIndex) != 0 ? 1 : 0;
}

auto MCBattleMech::GetTotalEffectiveness() -> float
{
    const float weaponRatio = WeaponEffectiveness / MaxWeaponEffectiveness;
    float armorFactor = 0.0f;

    if (IsDestroyed() == 0 && IsDisabled() == 0)
    {
        // Head, arms, centre torso (the worse of front and back) and side torsos (front and back), each as a share
        // of its full armor.
        const MCArmorLocation* locations = Armor.get();
        const float head = locations[MECH_BODY_LOCATION_HEAD].CurArmor /
                               static_cast<float>(locations[MECH_BODY_LOCATION_HEAD].MaxArmor) * 0.6f +
                           0.4f;
        float centre = locations[MECH_BODY_LOCATION_CTORSO].CurArmor;
        uint8_t centreMax = locations[MECH_BODY_LOCATION_CTORSO].MaxArmor;

        if (locations[8].CurArmor < centre)
        {
            centreMax = locations[8].MaxArmor;
            centre = locations[8].CurArmor;
        }

        const float arms = (locations[MECH_BODY_LOCATION_RARM].CurArmor + locations[MECH_BODY_LOCATION_LARM].CurArmor) /
                           static_cast<float>(locations[MECH_BODY_LOCATION_RARM].MaxArmor +
                                              locations[MECH_BODY_LOCATION_LARM].MaxArmor);
        const float armFactor = arms * 0.25f + 0.75f;
        const float sides =
            (locations[10].CurArmor + locations[9].CurArmor + locations[MECH_BODY_LOCATION_RTORSO].CurArmor +
             locations[MECH_BODY_LOCATION_LTORSO].CurArmor) /
            static_cast<float>(locations[10].MaxArmor + locations[9].MaxArmor +
                               locations[MECH_BODY_LOCATION_RTORSO].MaxArmor +
                               locations[MECH_BODY_LOCATION_LTORSO].MaxArmor);
        armorFactor = armFactor * (arms * 0.4f + 0.6f) * (centre / static_cast<float>(centreMax) + 1.0f) * 0.5f *
                      (sides * 0.25f + 0.75f) * head;
    }

    // Wounds wear the pilot down.
    const float woundFactor[7] = {1.0f, 0.95f, 0.85f, 0.75f, 0.5f, 0.3f, 0.0f};
    auto wounds = static_cast<uint32_t>(static_cast<int32_t>(std::floor(GetPilot()->Wounds)));

    if (6 < wounds)
    {
        wounds = 6;
    }

    return woundFactor[wounds] * armorFactor * weaponRatio;
}

auto MCBattleMech::DamageLoadedComponents() -> void
{
    for (int32_t location = 0; location < NUM_MECH_BODY_LOCATIONS; location++)
    {
        for (int32_t i = 0; i < NumLocationCriticalSpaces[location]; i++)
        {
            const MCCriticalSpace& space = BodyAt(location).CriticalSpaces[i];

            if (space.Hit != 0)
            {
                HitInventoryItem(static_cast<int8_t>(space.InventoryID), 1);
            }
        }
    }
}

auto MCMechStatusWindow::Init(int32_t x, int32_t y, int32_t w, int32_t h, MCBattleMech* newMech) -> void
{
    MCGuiTitleWindow::Init(x, y, w, h, nullptr);

    if (TitleBar != nullptr)
    {
        TitleBar->ShowCloseButton(1);
    }

    Mech = newMech;
}

auto MCMechStatusWindow::HandleEvent(MCGuiEvent* event) -> void
{
    if (event->Type == 0xd)
    {
        Mech->CloseStatusWindow();
    }

    MCGuiObject::HandleEvent(event);
}

auto MCMechStatusWindow::Resize(int32_t w, int32_t h) -> void
{
    MCGuiTitleWindow::Resize(w, h);
}

namespace
{
    /// <summary>Alignment names, by alignment + 1.</summary>
    const char* const AlignmentNames[3] = {"Clan", "Neutral", "Inner Sphere"};
}

auto MCMechStatusWindow::Display() -> void
{
    static const char* const statusNames[6] = {"Normal",      "Disabled",      "Destroyed",
                                               "Starting Up", "Shutting Down", "Shut Down"};
    static const char* const armorNames[11] = {
        "Head:",     "Center Torso:", "Left Torso:",        "Right Torso:",     "Left Arm:",        "Right Arm:",
        "Left Leg:", "Right Leg:",    "Rear Center Torso:", "Rear Left Torso:", "Rear Right torso:"};
    static const char* const locationNames[8] = {"HEAD", "CTORSO", "LTORSO", "RTORSO", "LARM", "RARM", "LLEG", "RLEG"};
    static const char* const damageNames[3] = {"No Damage", "Partial Damage", "Destroyed"};
    VfxPaneWipe(DisplayPort->Frame(), BackgroundColor);
    MCBattleMech* shown = Mech;

    if (shown != nullptr)
    {
        char line[256];
        std::snprintf(line, sizeof(line), "%s %s (%s)", AlignmentNames[shown->GetAlignment() + 1],
                      shown->DebugStatus.c_str(), shown->GetPilot()->Callsign);
        SetTitle(line);
        MCGuiPort* port = DisplayPort;
        SystemFont->WriteString(port->Frame(), 2, 10, reinterpret_cast<uint8_t*>(const_cast<char*>("Status:")), -1);
        std::snprintf(line, sizeof(line), "%s", statusNames[static_cast<uint8_t>(shown->Status)]);
        SystemFont->WriteString(port->Frame(), 100, 10, reinterpret_cast<uint8_t*>(line), -1);
        SystemFont->WriteString(port->Frame(), 2, 0x14, reinterpret_cast<uint8_t*>(const_cast<char*>("Combat Value:")),
                                -1);
        const int32_t maxCV = shown->GetMaxCV();
        std::snprintf(line, sizeof(line), "%d/%d", shown->GetCurCV(), maxCV);
        SystemFont->WriteString(port->Frame(), 100, 0x14, reinterpret_cast<uint8_t*>(line), -1);

        for (int32_t i = 0; i < NUM_MECH_BODY_LOCATIONS; i++)
        {
            const int32_t y = 0x50 + i * 10;
            const MCArmorLocation& armorLocation = shown->Armor[i];
            const MCBodyLocation& bodyLocation = shown->BodyAt(i);
            SystemFont->WriteString(port->Frame(), 2, y, reinterpret_cast<uint8_t*>(const_cast<char*>(armorNames[i])),
                                    -1);
            // Port fix: the original passes the armor and structure as doubles to %d.
            std::snprintf(line, sizeof(line), "A(%d/%d), IS(%d/%d), %s%s", static_cast<int32_t>(armorLocation.CurArmor),
                          armorLocation.MaxArmor, static_cast<int32_t>(bodyLocation.CurInternalStructure),
                          bodyLocation.MaxInternalStructure, damageNames[bodyLocation.DamageState],
                          bodyLocation.HasCase != 0 ? " [CASE]" : "");
            SystemFont->WriteString(port->Frame(), 100, y, reinterpret_cast<uint8_t*>(line), -1);
        }

        for (int32_t i = 0; i < 3; i++)
        {
            const int32_t y = 0xa0 + i * 10;
            const MCArmorLocation& armorLocation = shown->Armor[NUM_MECH_BODY_LOCATIONS + i];
            SystemFont->WriteString(
                port->Frame(), 2, y,
                reinterpret_cast<uint8_t*>(const_cast<char*>(armorNames[NUM_MECH_BODY_LOCATIONS + i])), -1);
            // Port fix: as above.
            std::snprintf(line, sizeof(line), "A(%d/%d)", static_cast<int32_t>(armorLocation.CurArmor),
                          armorLocation.MaxArmor);
            SystemFont->WriteString(port->Frame(), 100, y, reinterpret_cast<uint8_t*>(line), -1);
        }

        SystemFont->WriteString(port->Frame(), 2, 200, reinterpret_cast<uint8_t*>(const_cast<char*>("Inventory:")), -1);
        SystemFont->WriteString(port->Frame(), 0xc, 0xd2, reinterpret_cast<uint8_t*>(const_cast<char*>("Weapons:")),
                                -1);
        const int32_t numOther = shown->NumOther;
        const int32_t numWeapons = shown->NumWeapons;

        for (int32_t i = numOther; i < numOther + numWeapons; i++)
        {
            const MCInventoryItem& item = shown->Inventory[i];
            const double ready = item.ReadyTime <= ScenarioTime ? 0.0 : item.ReadyTime - ScenarioTime;
            const MCMasterComponent& component = MasterComponentList[item.MasterID];
            // Port fix: the original passes the whole ammo tally by value to AMMO(%d), which misaligns RDY too.
            std::snprintf(line, sizeof(line), "%s: [%s] ID(%d), H(%d/%d), AMMO(%d), RDY(%.2f)%s", component.Name,
                          locationNames[item.BodyLocation], item.MasterID, item.Health,
                          static_cast<int8_t>(component.CriticalSpacesReq),
                          shown->AmmoTypeTotal[item.AmmoIndex + 1].CurAmount, ready,
                          item.Disabled != 0 ? " DISABLED" : "");
            SystemFont->WriteString(port->Frame(), 0x16, 0xdc + (i - numOther) * 10, reinterpret_cast<uint8_t*>(line),
                                    -1);
        }

        SystemFont->WriteString(port->Frame(), 0xc, (numWeapons * 5 + 0x6e) * 2,
                                reinterpret_cast<uint8_t*>(const_cast<char*>("Misc:")), -1);
        int32_t y = (numWeapons * 5 + 0x73) * 2;

        for (int32_t i = 0; i < numOther; i++)
        {
            const MCInventoryItem& item = shown->Inventory[i];
            const MCMasterComponent& component = MasterComponentList[item.MasterID];
            std::snprintf(line, sizeof(line), "%s: [%s] ID(%d), H(%d/%d)%s", component.Name,
                          locationNames[item.BodyLocation], item.MasterID, item.Health,
                          static_cast<int8_t>(component.CriticalSpacesReq), item.Disabled != 0 ? " DISABLED" : "");
            SystemFont->WriteString(port->Frame(), 0x16, y, reinterpret_cast<uint8_t*>(line), -1);
            y += 10;
        }
    }

    MCGuiObject::Display();
}

auto MCMechStatusWindow::Draw() -> void
{
    if (Mech != nullptr)
    {
        char title[256];
        std::snprintf(title, sizeof(title), "%s %s (%s)", AlignmentNames[Mech->GetAlignment() + 1],
                      Mech->DebugStatus.c_str(), Mech->GetPilot()->Callsign);
        SetTitle(title);
    }

    MCGuiTitleWindow::Draw();
}
