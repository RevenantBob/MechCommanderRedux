#include "stdafx.h"
#include "object/mech.h"
#include "abl/abldbug.h"
#include "ai/move.h"
#include "ai/tacordr.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "engine/celine.h"
#include "engine/cevfx.h"
#include "engine/ceglist.h"
#include "engine/crater.h"
#include "vfx/vfxfuncs.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "iface/iface.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "lib/inifile.h"
#include "lib/packet.h"
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
#include "terrain/terrain.h"
#include "sprite/mactor.h"

char mechSpeedStateArray[32] = {0, 0, 0, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1,  1,  1,  1,
                                2, 2, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1};
char MechStateByGesture[28] = {0, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 2, 2, 8, 7, 8, 7, 8, 7, 6, 7, 7, 7, 8, 0, 0, 0};
int32_t NumLocationCriticalSpaces[NUM_MECH_BODY_LOCATIONS] = {6, 12, 12, 12, 12, 12, 6, 6};
int32_t MechHitSectionTable[5] = {1, 1, 0, 2, 1};
int32_t adjClippedCell[8][2] = {{0, 0}, {0, 2}, {2, 2}, {2, 4}, {4, 4}, {4, 6}, {6, 6}, {6, 0}};
// MCX.EXE @ 0x00790c50, after adjClippedCell; its users aren't known yet.
int32_t MechUnknown790C50 = 0;
// MCX.EXE @ 0x00790c54, between MechHitSectionTable and RankVersusChassisCombatModifier; its users aren't known yet.
float MechUnknownRanges[4] = {15.0f, 40.0f, 75.0f, 200.0f};
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
// MCX.EXE @ 0x00790e20: each armor location's body location (the rear torso ones map to the torso).
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
float mechCollisionThreshold = 0.0f;
float objectCollisionThreshold = 0.0f;
float tonnageCollisionThreshold = 0.0f;
float treeDeflection = 0.0f;
float mechPivotAngle = 0.0f;
float mechPivotThrottle = 0.0f;
GameObject* BadGuy = nullptr;
uint8_t footPrints = 1;
float MineSplashRange = 0.0f;
float MineSplashDamage = 0.0f;
int32_t MineExplosion = 0;
float MineBaseDamage = 0.0f;
int friendlyDestroyed = 0;
int enemyDestroyed = 0;

namespace
{
    /// <summary>Half pi, as MCX.EXE stores it (MCX.EXE @ 0x0077cb50).</summary>
    constexpr double HALF_PI = 0x1.921fb5443e88cp+0;
    /// <summary>Degrees to radians, as MCX.EXE stores it (MCX.EXE @ 0x0077c2a0; a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;

    /// <summary>Turns a frame about its k axis (MC2's inline frame_of_ref::rotate_about_k).</summary>
    void rotateAboutK(frame_of_ref& frame, float s, float c)
    {
        const vector_3d oldI = frame.i;
        frame.i = frame.i * c + frame.j * s;
        frame.j = frame.j * c - oldI * s;
    }

    /// <summary>
    /// Damage from bumping into <paramref name="other"/>: tonnage / 10 + 1/2 (an enemy) or tonnage / 100 + 1/2 (a
    /// friend), hitting <paramref name="victim"/> from <paramref name="other"/>'s side.
    /// </summary>
    void collisionHit(GameObject* victim, GameObject* shooter, GameObject* tonnageOf, int32_t attackSource,
                      int friendly)
    {
        const int32_t hitLocation = victim->calcHitLocation(shooter, -1, attackSource, 0);
        const float entryAngle = victim->relFacingTo(shooter->getPosition(), -1);
        const double scale = friendly == 0 ? 0.1 : 0.01;
        _WeaponShotInfo shotInfo;
        shotInfo.init(shooter, -1, static_cast<float>(tonnageOf->getTonnage() * scale + 0.5), hitLocation, entryAngle);
        victim->handleWeaponHit(&shotInfo, MPlayer != nullptr);
    }
}

auto loadMechGameSystem(FitIniFile* sysFile) -> int32_t
{
    int32_t result = sysFile->seekBlock("Mech:Class");

    if (result != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdFloat("MaxLightMech", MechClassWeights[1])) != 0)
    {
        return result;
    }

    // The original reads "MaxHeavyMech" for both the medium and the heavy bound.
    if ((result = sysFile->readIdFloat("MaxHeavyMech", MechClassWeights[2])) != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdFloat("MaxHeavyMech", MechClassWeights[3])) != 0)
    {
        return result;
    }

    if ((result = sysFile->seekBlock("Mech:Movement")) != 0)
    {
        return result;
    }

    int32_t jumpCost = 0;

    if (sysFile->readIdLong("JumpCost", jumpCost) == 0)
    {
        DefaultMechJumpCost = jumpCost;
    }

    int32_t value = 0;

    if (sysFile->readIdLong("CrashAvoidSelf", value) == 0)
    {
        DefaultMechCrashAvoidSelf = value;
    }

    if (sysFile->readIdLong("CrashAvoidPath", value) == 0)
    {
        DefaultMechCrashAvoidPath = value;
    }

    if (sysFile->readIdLong("CrashBlockSelf", value) == 0)
    {
        DefaultMechCrashBlockSelf = value;
    }

    if (sysFile->readIdLong("CrashBlockPath", value) == 0)
    {
        DefaultMechCrashBlockPath = value;
    }

    float yieldTime = 0.0f;

    if (sysFile->readIdFloat("CrashYieldTime", yieldTime) == 0)
    {
        DefaultMechCrashYieldTime = yieldTime;
    }

    if ((result = sysFile->readIdLongArray("PilotCheckConditions", MechPilotCheckConditions, 2)) != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdLongArray("PilotCheckTerrainEffect", MechPilotCheckTerrainEffect, 0x40)) != 0)
    {
        return result;
    }

    if ((result = sysFile->seekBlock("Mech:FireWeapon")) != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdLongArray("AttackerMoveModifier", AttackerMoveModifier, 9)) != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdCharArray("HitLocationTable", MechHitLocationTable, 0x84)) != 0)
    {
        return result;
    }

    int32_t targetMoveModifiers[10];

    if ((result = sysFile->readIdLongArray("TargetMoveModifierTable", targetMoveModifiers, 10)) != 0)
    {
        return result;
    }

    std::memcpy(TargetMoveModifierTable, targetMoveModifiers, sizeof(TargetMoveModifierTable));

    if ((result = sysFile->seekBlock("Mech:Damage")) != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdCharArray("CriticalHitTable", CriticalHitTable, 4)) != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdCharArray("MechTransferHitTable", MechTransferHitTable, 8)) != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdLong("MechSalvageChance", MechSalvageChance)) != 0)
    {
        return result;
    }

    if ((result = sysFile->seekBlock("Mech:Collision")) != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdFloat("collisionThreshold", mechCollisionThreshold)) != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdFloat("objectThreshold", objectCollisionThreshold)) != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdFloat("tonnageThreshold", tonnageCollisionThreshold)) != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdFloat("treeDeflection", treeDeflection)) != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdFloat("pivotAngle", mechPivotAngle)) != 0)
    {
        return result;
    }

    return sysFile->readIdFloat("pivotThrottle", mechPivotThrottle);
}

//---------------------------------------------------------------------------
// BattleMechType
//---------------------------------------------------------------------------

auto BattleMechType::init() -> void
{
    rightArmDebrisId = 0xffffffff;
    leftArmDebrisId = 0xffffffff;
    destroyedPiece = 0xffffffff;
    crashAvoidSelf = DefaultMechCrashAvoidSelf;
    crashAvoidPath = DefaultMechCrashAvoidPath;
    crashBlockSelf = DefaultMechCrashBlockSelf;
    crashBlockPath = DefaultMechCrashBlockPath;
    mechId = 0;
    name = nullptr;
    mechType = 0;
    chassis = 0;
    tonnageClass = 0.0f;
    endoSteel = 0;
    internalStructureTonnage = 0.0f;
    unknown50 = 0;
    hotSpotData = nullptr;
    gestureHotSpots = nullptr;
    jumpData = nullptr;
    footprintType = 1;
    gestureOutlines = nullptr;
    weaponHotSpots = nullptr;
    numFramesPerHotSpot = nullptr;
    numWeapons = 0;
    numOthers = 0;
    numHotSpotPackets = 0;
    dynamicsType = nullptr;
    crashYieldTime = DefaultMechCrashYieldTime;
    explDmg = 0.0f;
    explRad = 0.0f;
}

auto BattleMechType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    static const char* const bodyLocationNames[NUM_MECH_BODY_LOCATIONS] = {
        "Head", "CenterTorso", "LeftTorso", "RightTorso", "LeftArm", "RightArm", "LeftLeg", "RightLeg"};

    FitIniFile mechFile;
    int32_t result = mechFile.open(objFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if ((result = mechFile.seekBlock("Header")) != 0)
    {
        return result;
    }

    char fileType[128];

    if ((result = mechFile.readIdString("FileType", fileType, 127)) != 0)
    {
        return result;
    }

    if (std::strcmp(fileType, "MechType") != 0)
    {
        return -1;
    }

    if ((result = mechFile.seekBlock("General")) != 0)
    {
        return result;
    }

    if ((result = mechFile.readIdULong("ID", mechId)) != 0)
    {
        return result;
    }

    // "Type" 0 is 1, 1 is -1.
    static constexpr uint8_t typeMap[2] = {1, 0xff};
    uint8_t type = 0;

    if ((result = mechFile.readIdUChar("Type", type)) != 0)
    {
        return result;
    }

    // Port fix: the original reads other values from past its two-entry table on the stack.
    mechType = type < 2 ? typeMap[type] : 0;
    char nameBuffer[128];
    mechFile.readIdString("Name", nameBuffer, 127);
    name = static_cast<char*>(systemHeap->malloc(static_cast<uint32_t>(std::strlen(nameBuffer) + 1)));
    std::strcpy(name, nameBuffer);

    if ((result = mechFile.readIdUChar("Chassis", chassis)) != 0)
    {
        return result;
    }

    if ((result = mechFile.readIdFloat("TonnageClass", tonnageClass)) != 0)
    {
        return result;
    }

    if (mechFile.readIdFloat("ExplosionRadius", explRad) != 0)
    {
        explRad = 0.0f;
    }

    if (mechFile.readIdFloat("ExplosionDamage", explDmg) != 0)
    {
        explDmg = 0.0f;
    }

    uint8_t endo = 0;

    if ((result = mechFile.readIdUChar("EndoSteel", endo)) != 0)
    {
        return result;
    }

    endoSteel = endo;

    if ((result = mechFile.readIdFloat("InternalStructureTonnage", internalStructureTonnage)) != 0)
    {
        return result;
    }

    if ((result = mechFile.seekBlock("InternalStructure")) != 0)
    {
        return result;
    }

    for (int32_t location = 0; location < NUM_MECH_BODY_LOCATIONS; location++)
    {
        if ((result = mechFile.readIdUChar(bodyLocationNames[location], internalStructure[location])) != 0)
        {
            return result;
        }
    }

    if ((result = mechFile.seekBlock("Debris")) != 0)
    {
        return result;
    }

    if ((result = mechFile.readIdULong("RightArmPiece", rightArmDebrisId)) != 0)
    {
        return result;
    }

    if ((result = mechFile.readIdULong("LeftArmPiece", leftArmDebrisId)) != 0)
    {
        return result;
    }

    if ((result = mechFile.readIdULong("DestroyedPiece", destroyedPiece)) != 0)
    {
        return result;
    }

    if ((result = mechFile.seekBlock("Dynamics")) != 0)
    {
        return result;
    }

    uint32_t dynamicsTypeId = 0;

    if ((result = mechFile.readIdULong("Type", dynamicsTypeId)) != 0)
    {
        return result;
    }

    if (dynamicsTypeId != 1)
    {
        return -0x5fffd;
    }

    dynamicsType = new MechDynamicsType;

    if (dynamicsType == nullptr)
    {
        return -0x5fffe;
    }

    if ((result = dynamicsType->init(&mechFile)) != 0)
    {
        return result;
    }

    if (mechFile.seekBlock("MovementSystem") == 0)
    {
        int32_t value = 0;

        if (mechFile.readIdLong("CrashAvoidSelf", value) == 0)
        {
            crashAvoidSelf = value;
        }

        if (mechFile.readIdLong("CrashAvoidPath", value) == 0)
        {
            crashAvoidPath = value;
        }

        if (mechFile.readIdLong("CrashBlockSelf", value) == 0)
        {
            crashBlockSelf = value;
        }

        if (mechFile.readIdLong("CrashBlockPath", value) == 0)
        {
            crashBlockPath = value;
        }

        float yieldTime = 0.0f;

        // The original stores the last long read ("CrashBlockPath"), not the yield time it just read.
        if (mechFile.readIdFloat("CrashYieldTime", yieldTime) == 0)
        {
            crashYieldTime = static_cast<float>(value);
        }
    }

    if ((result = loadHotSpots(&mechFile)) != 0)
    {
        return result;
    }

    return ObjectType::init(&mechFile);
}

auto BattleMechType::destroy() -> void
{
    if (name != nullptr)
    {
        systemHeap->free(name);
        name = nullptr;
    }

    delete dynamicsType;
    dynamicsType = nullptr;
    ObjectType::destroy();
}

auto BattleMechType::handleCollision(GameObject* collidee, GameObject* collider) -> int
{
    if (MPlayer != nullptr && MPlayer->isServer == 0)
    {
        return 0;
    }

    int friendly = 0;
    int collideeJumping = static_cast<Mover*>(collidee)->isJumping(nullptr);
    int colliderJumping = 0;
    uint32_t sampleId = 4;

    switch (collider->objectClass)
    {
        case BATTLEMECH:
        {
            if (collidee->getPilot()->alignment != collider->getPilot()->alignment)
            {
                MechWarrior* attackerPilot = collider->getPilot();

                if (attackerPilot->curTacOrder.code == TACTICAL_ORDER_ATTACK_OBJECT)
                {
                    attackerPilot->numRams++;
                }
                else if (attackerPilot->curTacOrder.code == TACTICAL_ORDER_JUMPTO_POINT &&
                         collidee->getPilot()->curTacOrder.getJumpTarget() == collidee)
                {
                    collidee->getPilot()->numJumpAttacks++;
                }
            }

            colliderJumping = static_cast<Mover*>(collider)->isJumping(nullptr);
            auto* colliderMech = static_cast<BattleMech*>(collider);

            if (colliderJumping == 0 && colliderMech->jumpTime >= 0.0f)
            {
                colliderJumping = scenarioTime - colliderMech->jumpTime < 0.5f ? 1 : 0;
            }

            [[fallthrough]];
        }

        case GROUNDVEHICLE:
        {
            bool jumpHit = true;

            if (collideeJumping == 0)
            {
                auto* collideeMech = static_cast<BattleMech*>(collidee);
                bool landing = false;

                if (collideeMech->jumpTime >= 0.0f)
                {
                    const float sinceJump = scenarioTime - collideeMech->jumpTime;
                    collideeMech->jumpTime = -1.0f;

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

            if (collidee->getPilot()->alignment == collider->getPilot()->alignment)
            {
                friendly = 1;

                if (!jumpHit)
                {
                    return 0;
                }
            }
            else if (!jumpHit)
            {
                GameObject* collideeRamTarget = collidee->getPilot()->curTacOrder.getRamTarget();
                GameObject* colliderRamTarget = collider->getPilot()->curTacOrder.getRamTarget();

                if (collideeRamTarget != collider && colliderRamTarget != collidee)
                {
                    return 0;
                }
            }

            if (collidee->getCollisionFreeFrom() == collider && scenarioTime <= collidee->getCollisionFreeTime())
            {
                return 0;
            }

            collidee->setCollisionFreeFrom(collider);
            collidee->setCollisionFreeTime(scenarioTime + 2.0f);

            frame_of_ref frame = collidee->getFrame();
            rotateAboutK(frame, static_cast<float>(std::sin(HALF_PI)), static_cast<float>(std::cos(HALF_PI)));
            collidee->setFrame(frame);
            collidee->getVelocity();

            if (jumpHit)
            {
                if (collideeJumping != 0)
                {
                    // The jumper lands on the other: both take the other's weight (the collider's, twice).
                    const int32_t hitLocation = collidee->calcHitLocation(collider, -1, 3, 0);
                    const float entryAngle = collidee->relFacingTo(collider->getPosition(), -1);
                    _WeaponShotInfo shotInfo;
                    shotInfo.init(collider, -1,
                                  static_cast<float>(collider->getTonnage() * (friendly == 0 ? 0.1 : 0.01) + 0.5),
                                  hitLocation, entryAngle);
                    collidee->handleWeaponHit(&shotInfo, MPlayer != nullptr);
                    const int32_t otherHitLocation = collider->calcHitLocation(collidee, -1, 2, 0);
                    const float otherEntryAngle = collider->relFacingTo(collidee->getPosition(), -1);
                    shotInfo.init(collidee, -1,
                                  static_cast<float>(collider->getTonnage() * (friendly == 0 ? 0.1 : 0.01) + 0.5),
                                  otherHitLocation, otherEntryAngle);
                    collider->handleWeaponHit(&shotInfo, MPlayer != nullptr);
                    vector_3d position = collider->getPosition();
                    CreateExplosion(0x290, position, 0.0f, 0.0f);
                }
            }
            else
            {
                collisionHit(collidee, collider, collider, 1, friendly);
            }

            static_cast<Mover*>(collidee)->bounceToAdjCell();

            if (friendly != 0)
            {
                return 0;
            }
            break;
        }

        case ELEMENTAL:
        {
            if (collidee->getPilot()->alignment == collider->getPilot()->alignment)
            {
                return 0;
            }

            GameObject* collideeRamTarget = collidee->getPilot()->curTacOrder.getRamTarget();
            GameObject* colliderRamTarget = collider->getPilot()->curTacOrder.getRamTarget();

            if (collideeRamTarget != collider && colliderRamTarget != collidee)
            {
                return 0;
            }

            if (collidee->getCollisionFreeFrom() == collider && scenarioTime <= collidee->getCollisionFreeTime())
            {
                return 0;
            }

            if (collider->isMarine() != 0)
            {
                return 0;
            }

            collidee->setCollisionFreeFrom(collider);
            collidee->setCollisionFreeTime(scenarioTime + 2.0f);
            collidee->getVelocity();
            const int32_t hitLocation = collidee->calcHitLocation(collider, -1, 1, 0);
            const float entryAngle = collidee->relFacingTo(collider->getPosition(), -1);
            _WeaponShotInfo shotInfo;
            shotInfo.init(collider, -1, elmDamageOnImpact, hitLocation, entryAngle);
            collidee->handleWeaponHit(&shotInfo, MPlayer != nullptr);
            sampleId = 0x1e;
            break;
        }

        case BUILDING:
        case TREEBUILDING:
        {
            if (collidee->getCollisionFreeFrom() == collider && scenarioTime <= collidee->getCollisionFreeTime())
            {
                return 0;
            }

            collidee->setCollisionFreeFrom(collider);
            collidee->setCollisionFreeTime(scenarioTime + 2.0f);
            const vector_3d velocity = collidee->getVelocity();

            if (std::sqrt(velocity.y * velocity.y + velocity.z * velocity.z + velocity.x * velocity.x) <=
                mechCollisionThreshold)
            {
                static_cast<Mover*>(collidee)->bounceToAdjCell();
            }

            const int32_t hitLocation = collidee->calcHitLocation(collider, -1, 1, 0);
            const float entryAngle = collidee->relFacingTo(collider->getPosition(), -1);
            _WeaponShotInfo shotInfo;
            shotInfo.init(collider, -1, static_cast<float>(collider->getTonnage() * 0.1 + 0.5), hitLocation,
                          entryAngle);
            collidee->handleWeaponHit(&shotInfo, MPlayer != nullptr);
            collider->handleWeaponHit(&shotInfo, MPlayer != nullptr);
            break;
        }

        case TREE:
        {
            if (collidee->getCollisionFreeFrom() == collider && scenarioTime <= collidee->getCollisionFreeTime())
            {
                return 0;
            }

            collidee->setCollisionFreeFrom(collider);
            collidee->setCollisionFreeTime(scenarioTime + 2.0f);
            frame_of_ref frame = collidee->getFrame();
            collider->getObjectType();
            double deflection = 0.0;

            if (tonnageClass < tonnageCollisionThreshold)
            {
                deflection = static_cast<double>(tonnageCollisionThreshold) / tonnageClass * treeDeflection;
            }

            if (deflection > 0.0)
            {
                rotateAboutK(frame, static_cast<float>(std::sin(deflection * DEGREES_TO_RADIANS)),
                             static_cast<float>(std::cos(deflection * DEGREES_TO_RADIANS)));
                collidee->setFrame(frame);
            }
            break;
        }

        case TRAINCAR:
        {
            if (collidee->getCollisionFreeFrom() == collider && scenarioTime <= collidee->getCollisionFreeTime())
            {
                return 0;
            }

            collidee->setCollisionFreeFrom(collider);
            collidee->setCollisionFreeTime(scenarioTime + 2.0f);
            frame_of_ref frame = collidee->getFrame();
            rotateAboutK(frame, static_cast<float>(std::sin(HALF_PI)), static_cast<float>(std::cos(HALF_PI)));
            collidee->setFrame(frame);
            collidee->getVelocity();
            static_cast<Mover*>(collidee)->bounceToAdjCell();
            break;
        }

        default:
            return 0;
    }

    soundSystem->playDigitalSample(sampleId, 1, collidee, 0, 0);
    return 0;
}

auto BattleMechType::handleDestruction(GameObject* collidee, GameObject* collider) -> int
{
    auto* mech = static_cast<BattleMech*>(collidee);

    if (mech->getPilot() == nullptr)
    {
        Fatal(0, " No Pilot in this mech! ");
    }

    if (mech->getPoint() == mech)
    {
        mech->group->setPoint(nullptr);
    }

    if (mech->sensorSystem != nullptr)
    {
        mech->sensorSystem->disable();
    }

    mech->unknown794 = 0.8f;

    if (mech->unknown79C != 0)
    {
        mech->getPilot()->handleAlarm(8, 0);
        theInterface->RemoveMech(mech->partId);
        return 1;
    }

    mech->getPilot()->handleAlarm(7, collider == nullptr ? 0 : collider->idNumber);
    mech->status = 2;
    mech->unknown8EC = 0;
    mech->unknown798 = 0;

    for (int32_t location = 0; location < mech->numBodyLocations; location++)
    {
        mech->destroyBodyLocation(location);
    }

    if (mech->getAlignment() == homeTeam->alignment)
    {
        friendlyDestroyed = 1;
        return 1;
    }

    enemyDestroyed = 1;
    return 1;
}

auto BattleMechType::loadHotSpots(FitIniFile* mechFile) -> int32_t
{
    if (mechFile == nullptr)
    {
        return 0;
    }

    int32_t result = mechFile->seekBlock("HotSpots");

    if (result != 0)
    {
        return result;
    }

    char hotSpotFileName[80];

    if ((result = mechFile->readIdString("HotSpotFileName", hotSpotFileName, 79)) != 0)
    {
        return result;
    }

    int32_t footprint = 0;

    if ((result = mechFile->readIdLong("FootprintType", footprint)) != 0)
    {
        return result;
    }

    footprintType = footprint;

    FullPathFileName hotSpotPath;
    hotSpotPath.init(shapesPath, hotSpotFileName, ".hsp");
    FullPathFileName outlinePath;
    outlinePath.init(shapesPath, hotSpotFileName, ".out");
    FullPathFileName infoPath;
    infoPath.init(shapesPath, hotSpotFileName, ".inf");
    FullPathFileName jumpPath;
    jumpPath.init(shapesPath, hotSpotFileName, ".jmp");

    PacketFile hotSpotFile;

    if ((result = hotSpotFile.open(hotSpotPath, READ, 50)) != 0)
    {
        return result;
    }

    PacketFile outlineFile;

    if ((result = outlineFile.open(outlinePath, READ, 50)) != 0)
    {
        return result;
    }

    FitIniFile infoFile;

    if ((result = infoFile.open(infoPath, READ, 50)) != 0)
    {
        return result;
    }

    File jumpFile;

    if ((result = jumpFile.open(jumpPath, READ, 50)) != 0)
    {
        return result;
    }

    if ((result = infoFile.seekBlock("Info")) != 0)
    {
        return result;
    }

    if ((result = infoFile.readIdULong("numHotSpotPackets", numHotSpotPackets)) != 0)
    {
        return result;
    }

    if ((result = infoFile.readIdULong("numWeapons", numWeapons)) != 0)
    {
        return result;
    }

    if ((result = infoFile.readIdULong("numOthers", numOthers)) != 0)
    {
        return result;
    }

    const uint32_t weaponCount = numWeapons;
    weaponHotSpots = static_cast<uint32_t*>(ObjectTypeManager::objectTypeCache->malloc(weaponCount * sizeof(uint32_t)));

    if (weaponHotSpots == nullptr)
    {
        return -0x5fff4;
    }

    for (int32_t weapon = 0; weapon < static_cast<int32_t>(weaponCount); weapon++)
    {
        char entryName[20];
        std::sprintf(entryName, "weapon%d", weapon);

        if ((result = mechFile->readIdULong(entryName, weaponHotSpots[weapon])) != 0)
        {
            return result;
        }
    }

    const uint32_t hotSpotDataSize = numHotSpotPackets * 32;
    hotSpotData = static_cast<uint8_t*>(ObjectTypeManager::objectTypeCache->malloc(hotSpotDataSize));

    if (hotSpotData == nullptr)
    {
        return -0x5fff5;
    }

    std::memset(hotSpotData, 0, hotSpotDataSize);
    const int32_t dataPacket = static_cast<int32_t>(numHotSpotPackets);

    if (hotSpotFile.seekPacket(dataPacket) == 0)
    {
        if (static_cast<uint32_t>(hotSpotFile.getPacketSize()) != hotSpotDataSize)
        {
            return -0x5fff3;
        }

        hotSpotFile.readPacket(dataPacket, hotSpotData);
    }

    // Port fix: pointer tables sized by the pointer, not the original's 4 bytes.
    const size_t tableSize = (static_cast<size_t>(dataPacket) + 1) * sizeof(uint8_t*);
    gestureHotSpots =
        static_cast<uint8_t**>(ObjectTypeManager::objectTypeCache->malloc(static_cast<uint32_t>(tableSize)));

    if (gestureHotSpots == nullptr)
    {
        return -0x5fff4;
    }

    std::memset(gestureHotSpots, 0, tableSize);
    const size_t outlineTableSize = (static_cast<size_t>(numHotSpotPackets) + 1) * sizeof(uint8_t*);
    gestureOutlines =
        static_cast<uint8_t**>(ObjectTypeManager::objectTypeCache->malloc(static_cast<uint32_t>(outlineTableSize)));

    if (gestureOutlines == nullptr)
    {
        return -0x5fff1;
    }

    std::memset(gestureOutlines, 0, outlineTableSize);

    const int32_t numGestures = static_cast<int32_t>(numHotSpotPackets);
    numFramesPerHotSpot =
        static_cast<uint32_t*>(ObjectTypeManager::objectTypeCache->malloc((numGestures + 1) * sizeof(uint32_t)));
    numHotSpotsPerGesture.assign(static_cast<size_t>(numGestures), 0);

    for (int32_t gesture = 0; gesture < numGestures; gesture++)
    {
        char blockName[20];
        std::sprintf(blockName, "Gesture%d", gesture);

        if ((result = infoFile.seekBlock(blockName)) != 0 ||
            (result = infoFile.readIdULong("numFramesPerHotSpot", numFramesPerHotSpot[gesture])) != 0)
        {
            return result;
        }

        if (hotSpotFile.seekPacket(gesture) != 0)
        {
            return -0x5fff2;
        }

        gestureHotSpots[gesture] =
            static_cast<uint8_t*>(ObjectTypeManager::objectTypeCache->malloc(hotSpotFile.getPacketSize()));

        if (gestureHotSpots[gesture] == nullptr)
        {
            return -0x5fff4;
        }

        hotSpotFile.readPacket(gesture, gestureHotSpots[gesture]);

        if (numFramesPerHotSpot[gesture] != 0)
        {
            numHotSpotsPerGesture[gesture] =
                static_cast<uint32_t>(hotSpotFile.getPacketSize()) / (numFramesPerHotSpot[gesture] * 12);
        }

        if (outlineFile.seekPacket(gesture) == 0 && outlineFile.getPacketSize() != 0)
        {
            gestureOutlines[gesture] =
                static_cast<uint8_t*>(ObjectTypeManager::objectTypeCache->malloc(outlineFile.getPacketSize()));

            if (gestureOutlines[gesture] == nullptr)
            {
                return -0x5fff1;
            }

            outlineFile.readPacket(gesture, gestureOutlines[gesture]);
        }
    }

    jumpData = static_cast<uint8_t*>(ObjectTypeManager::objectTypeCache->malloc(jumpFile.fileSize()));

    if (jumpData == nullptr)
    {
        return -0x5fff4;
    }

    std::memset(jumpData, 0, jumpFile.fileSize());
    jumpFile.read(jumpData, static_cast<int32_t>(jumpFile.fileSize()));
    return 0;
}

auto BattleMechType::createInstance() -> BaseObject*
{
    auto* newMech = new BattleMech;

    if (newMech == nullptr)
    {
        return nullptr;
    }

    if (newMech->init(this) != 0)
    {
        return nullptr;
    }

    newMech->idNumber = NextIdNumber++;
    return newMech;
}

//---------------------------------------------------------------------------
// BattleMech
//---------------------------------------------------------------------------

auto BattleMech::isCrippled() -> int
{
    return legStatus == 2 || legStatus == 3 ? 1 : 0;
}

auto BattleMech::getWeaponHeat(int32_t weaponIndex) -> float
{
    return MasterComponentList[inventory[weaponIndex].masterID].rangeOrHeat;
}

auto BattleMech::relViewFacingTo(vector_3d goal) -> float
{
    return relFacingTo(goal, -1);
}

auto BattleMech::canMove() -> int
{
    return legStatus != 3 ? 1 : 0;
}

auto BattleMech::canJump() -> int
{
    return numJumpJets != 0 ? 1 : 0;
}

auto BattleMech::handleStaticCollision() -> void
{
    const bool jumpFXOn =
        static_cast<MechActor*>(appearance)->currentGesture != 0x14 && (jumpFX[0] != nullptr || jumpFX[1] != nullptr);

    if (!((collisionsOn != 0 &&
           std::sqrt(velocity.z * velocity.z + velocity.y * velocity.y + velocity.x * velocity.x) > 0.0f) ||
          jumpFXOn))
    {
        return;
    }

    int32_t blockNumber = 0;
    int32_t vertexNumber = 0;
    getBlockAndVertexNumber(blockNumber, vertexNumber);
    char listName[12];
    std::sprintf(listName, "TBlk%d", blockNumber);
    ObjectQueueNode* list = objectList->head;

    while (list != nullptr && list->operator==(listName) == 0)
    {
        list = list->next;
    }

    // Port fix: the original reads the objects of a missing list through null.
    if (list == nullptr)
    {
        return;
    }

    for (BaseObject* object = list->head; object != nullptr; object = object->next)
    {
        auto* other = static_cast<GameObject*>(object);

        if (other->getObjectType() == nullptr)
        {
            continue;
        }

        int collides = 0;
        int32_t otherBlock = -1;
        int32_t otherVertex = -1;

        switch (other->objectClass)
        {
            case BUILDING:
            case TREE:
            case TERRAINOBJECT:
            case TREEBUILDING:
            {
                other->getBlockAndVertexNumber(otherBlock, otherVertex);
                collides = other->collisionsOn;
                break;
            }
            case MISCTERRAINOBJECT:
            {
                getBlockAndVertexNumber(otherBlock, otherVertex);

                if (static_cast<uint32_t>(static_cast<MiscTerrainObject*>(other)->terrainObjectKind) > 6)
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
            collisionSystem->detectStaticCollision(this, other);
        }
    }
}

auto BattleMech::init() -> void
{
    objectClass = BATTLEMECH;
    body = static_cast<BodyLocation*>(ObjectTypeManager::objectCache->malloc(sizeof(BodyLocation) * 8));
    numBodyLocations = 8;

    for (int32_t location = 0; location < 8; location++)
    {
        bodyAt(location).hasCASE = 0;
        bodyAt(location).totalSpaces = 0;
        bodyAt(location).criticalSpaces = nullptr;
        bodyAt(location).curInternalStructure = 0.0f;
        bodyAt(location).hotSpotNumber = 0;
        bodyAt(location).maxInternalStructure = 0;
        bodyAt(location).damageState = 0;
    }

    armor = static_cast<ArmorLocation*>(ObjectTypeManager::objectCache->malloc(sizeof(ArmorLocation) * 11));
    numArmorLocations = 11;
    mechClass = 1;
    legStatus = 0;
    torsoStatus = 0;
    numJumpJets = 0;
    jumpTime = -100.0f;
    inJump = 0;
    jumpGoal = vector_3d(0.0f, 0.0f, 0.0f);
    unknown8C4 = -1.0f;
    unknown8C8 = 0;
    unknown8CC = 0;
    torsoRotation = 0.0f;
    leftArmRotation = 0.0f;
    rightArmRotation = 0.0f;
    pendingControl8D0 = 0;
    pendingControl8D4 = 0;
    unknown8D8 = 0;
    unknown8DC = 0;
    unknown8EC = 0;
    unknown8F0 = 0;
    statusWindow = nullptr;
    blipFrame = 0;
    overlayWeightClass = 1;
    captureable = 0;
    unknown948 = 0;
}

auto BattleMech::init(ObjectType* objType) -> int32_t
{
    int32_t result = GameObject::init(objType);

    if (result != 0)
    {
        return result;
    }

    auto* mechType = static_cast<BattleMechType*>(objType);
    collisionsOn = 1;

    for (int32_t location = 0; location < 8; location++)
    {
        bodyAt(location).maxInternalStructure = mechType->internalStructure[location];
    }

    chassis = mechType->chassis;
    alignment = mechType->mechType;
    endoSteel = static_cast<int32_t>(mechType->endoSteel);
    internalStructureTonnage = mechType->internalStructureTonnage;
    tonnageClass = mechType->tonnageClass;
    crashAvoidSelf = mechType->crashAvoidSelf;
    pathLockLevel = mechType->crashBlockSelf;
    crashAvoidPath = mechType->crashAvoidPath;
    pathLockRange = mechType->crashBlockPath;
    crashYieldTime = mechType->crashYieldTime;
    control = nullptr;
    dynamics = mechType->dynamicsType->createInstance();

    if (dynamics == nullptr)
    {
        return -0x5fff8;
    }

    if ((result = dynamics->init(mechType->dynamicsType, this)) != 0)
    {
        return result;
    }

    AppearanceType* apprType = appearanceTypeList->getAppearance(mechType->appearName, 0);

    if (apprType == nullptr)
    {
        return -0x5fff7;
    }

    auto* actor = new MechActor;
    appearance = actor;

    if (actor == nullptr)
    {
        return -0x5ffff;
    }

    actor->unknown38 = this;

    if ((apprType->appearanceNum & 0xff000000) != 0x1000000)
    {
        return -0x5fff6;
    }

    if ((result = actor->init(apprType, this)) != 0)
    {
        return result;
    }

    objectClass = BATTLEMECH;

    for (int32_t i = 0; i < 4; i++)
    {
        smoke[i] = nullptr;
        smokeHotSpot[i] = 0;
        smokeTime[i] = 0.0f;
    }

    jumpFX[1] = nullptr;
    jumpFX[0] = nullptr;
    unknown7C8 = 1000.0f;
    return 0;
}

auto BattleMech::setControl(uint32_t controlType, uint32_t controlData, int32_t controlParam) -> int32_t
{
    int32_t result = 0;

    switch (controlType)
    {
        case 1:
        {
            delete control;
            auto* playerControl = new PlayerControl;
            control = playerControl;

            if (playerControl == nullptr)
            {
                return -0x5fffc;
            }

            if ((result = playerControl->init(this, 0)) != 0)
            {
                return result;
            }
            break;
        }

        case 2:
        {
            delete control;
            auto* aiControl = new MechAIControl;
            control = aiControl;

            if (aiControl == nullptr)
            {
                return -0x5fffc;
            }

            if ((result = aiControl->init(this)) != 0)
            {
                return result;
            }
            break;
        }

        case 3:
        {
            delete control;
            auto* netControl = new MechNetControl;
            control = netControl;

            if (netControl == nullptr)
            {
                return -0x5fffc;
            }

            if ((result = netControl->init(this)) != 0)
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

    auto* mechControlData = new MechControlData;
    control->controlData = mechControlData;

    if (mechControlData == nullptr)
    {
        return -0x5fffa;
    }

    return mechControlData->init(0);
}

auto BattleMech::init(FitIniFile* mechFile) -> int32_t
{
    static const char* const bodyLocationNames[NUM_MECH_BODY_LOCATIONS] = {
        "Head", "CenterTorso", "LeftTorso", "RightTorso", "LeftArm", "RightArm", "LeftLeg", "RightLeg"};
    static const char* const armorLocationNames[NUM_MECH_ARMOR_LOCATIONS] = {
        "Head",    "CenterTorso", "LeftTorso",       "RightTorso",    "LeftArm",       "RightArm",
        "LeftLeg", "RightLeg",    "RearCenterTorso", "RearLeftTorso", "RearRightTorso"};

    int32_t result = mechFile->seekBlock("Header");

    if (result != 0)
    {
        return result;
    }

    char fileType[128];

    if ((result = mechFile->readIdString("FileType", fileType, 127)) != 0)
    {
        return result;
    }

    if (std::strcmp(fileType, "MechProfile") != 0)
    {
        return -1;
    }

    if ((result = mechFile->seekBlock("General")) != 0)
    {
        return result;
    }

    char nameBuffer[128];
    mechFile->readIdString("Name", nameBuffer, 127);
    debugStatus = static_cast<char*>(systemHeap->malloc(static_cast<uint32_t>(std::strlen(nameBuffer) + 1)));
    std::strcpy(debugStatus, nameBuffer);
    if (mechFile->readIdLong("ChassisBR", chassisBR) != 0)
    {
        chassisBR = 100;
    }

    if ((result = mechFile->readIdFloat("CurTonnage", tonnage)) != 0)
    {
        return result;
    }

    if (mechFile->readIdLong("DescIndex", descIndex) != 0)
    {
        descIndex = -1;
    }

    char ifaceNameBuffer[256];
    cLoadString(thisInstance, descIndex + 300, ifaceNameBuffer, 0xfe);
    ifaceName = static_cast<char*>(systemHeap->malloc(static_cast<uint32_t>(std::strlen(ifaceNameBuffer) + 1)));
    std::strcpy(ifaceName, ifaceNameBuffer);

    if ((result = mechFile->readIdLong("NameIndex", nameIndex)) != 0)
    {
        return result;
    }

    if ((result = mechFile->readIdLong("NameVariant", nameVariant)) != 0)
    {
        return result;
    }

    if (mechFile->readIdLong("Pilot", pilotId) != 0)
    {
        pilotId = -1;
    }

    status = 0;

    if ((result = mechFile->readIdString("icon", iconName, 0x13)) != 0)
    {
        return result;
    }

    if (mechFile->readIdBoolean("NotMineYet", notMineYet) != 0)
    {
        notMineYet = 1;
    }

    if ((result = mechFile->seekBlock("Engine")) != 0)
    {
        return result;
    }

    if ((result = mechFile->readIdFloat("Tonnage", engineTonnage)) != 0)
    {
        return result;
    }

    if ((result = mechFile->readIdULong("Rating", engineRating)) != 0)
    {
        return result;
    }

    uint8_t runSpeed = 0;

    if ((result = mechFile->readIdUChar("MaxRunSpeed", runSpeed)) != 0)
    {
        return result;
    }

    maxRunSpeed = static_cast<float>(runSpeed);

    if (mechFile->seekBlock("MovementSystem") == 0)
    {
        int32_t value = 0;

        if (mechFile->readIdLong("CrashAvoidSelf", value) == 0)
        {
            crashAvoidSelf = value;
        }

        if (mechFile->readIdLong("CrashAvoidPath", value) == 0)
        {
            crashAvoidPath = value;
        }

        if (mechFile->readIdLong("CrashBlockSelf", value) == 0)
        {
            pathLockLevel = value;
        }

        if (mechFile->readIdLong("CrashBlockPath", value) == 0)
        {
            pathLockRange = value;
        }

        float yieldTime = 0.0f;

        // As BattleMechType::init: the last long read is stored, not the yield time.
        if (mechFile->readIdFloat("CrashYieldTime", yieldTime) == 0)
        {
            crashYieldTime = static_cast<float>(value);
        }
    }

    if ((result = mechFile->seekBlock("Armor")) != 0)
    {
        return result;
    }

    if ((result = mechFile->readIdUChar("Type", armorType)) != 0)
    {
        return result;
    }

    if ((result = mechFile->readIdFloat("Tonnage", armorTonnage)) != 0)
    {
        return result;
    }

    if ((result = mechFile->seekBlock("MaxArmorPoints")) != 0)
    {
        return result;
    }

    for (int32_t location = 0; location < NUM_MECH_ARMOR_LOCATIONS; location++)
    {
        if ((result = mechFile->readIdUChar(armorLocationNames[location], armor[location].maxArmor)) != 0)
        {
            return result;
        }
    }

    if ((result = mechFile->seekBlock("CurArmorPoints")) != 0)
    {
        return result;
    }

    for (int32_t location = 0; location < NUM_MECH_ARMOR_LOCATIONS; location++)
    {
        uint8_t points = 0;

        if ((result = mechFile->readIdUChar(armorLocationNames[location], points)) != 0)
        {
            return result;
        }

        armor[location].curArmor = static_cast<float>(points);
    }

    if ((result = mechFile->seekBlock("InventoryInfo")) != 0)
    {
        return result;
    }

    if ((result = mechFile->readIdUChar("NumOther", numOther)) != 0)
    {
        return result;
    }

    if ((result = mechFile->readIdUChar("NumWeapons", numWeapons)) != 0)
    {
        return result;
    }

    if ((result = mechFile->readIdUChar("NumAmmo", numAmmos)) != 0)
    {
        return result;
    }

    const int32_t firstWeapon = numOther;
    const int32_t firstAmmo = numOther + numWeapons;
    const int32_t numItems = numAmmos + numOther + numWeapons;
    inventory = static_cast<InventoryItem*>(
        ObjectTypeManager::objectCache->malloc(static_cast<uint32_t>(numItems * sizeof(InventoryItem))));

    if (inventory == nullptr)
    {
        return -2;
    }

    numAntiMissileSystems = 0;
    char blockName[32];

    for (int32_t item = 0; item < firstWeapon; item++)
    {
        std::sprintf(blockName, "Item:%d", item);

        if ((result = mechFile->seekBlock(blockName)) != 0)
        {
            return result;
        }

        InventoryItem& other = inventory[item];

        if ((result = mechFile->readIdUChar("MasterID", other.masterID)) != 0)
        {
            return result;
        }

        other.health = MasterComponentList[other.masterID].health;
        other.disabled = 0;
        other.amount = 1;
        other.ammoIndex = -1;
        other.readyTime = 0.0f;
        other.bodyLocation = 0xff;
        other.rangeRatings = nullptr;

        if (MasterComponentList[other.masterID].form == COMPONENT_FORM_JUMPJET)
        {
            numJumpJets++;
        }
    }

    for (int32_t item = firstWeapon; item < firstAmmo; item++)
    {
        std::sprintf(blockName, "Item:%d", item);

        if ((result = mechFile->seekBlock(blockName)) != 0)
        {
            return result;
        }

        InventoryItem& weapon = inventory[item];

        if ((result = mechFile->readIdUChar("MasterID", weapon.masterID)) != 0)
        {
            return result;
        }

        if ((result = mechFile->readIdUChar("FacesForward", weapon.facesForward)) != 0)
        {
            return result;
        }

        const MasterComponent& component = MasterComponentList[weapon.masterID];
        weapon.health = component.health;
        weapon.disabled = 0;
        weapon.amount = 1;
        weapon.ammoIndex = -1;
        weapon.readyTime = 0.0f;
        weapon.bodyLocation = 0xff;
        // Damage per ten seconds, then scaled by the long range over 24.
        weapon.effectiveness =
            static_cast<int16_t>(static_cast<int32_t>(component.damage * 10.0 / component.recycleTime));
        weapon.effectiveness = static_cast<int16_t>(static_cast<int32_t>(
            static_cast<double>(component.weaponRange[3]) * weapon.effectiveness * static_cast<double>(1.0f / 24.0f)));
        weapon.rangeRatings =
            static_cast<float*>(ObjectTypeManager::objectCache->malloc(NumRangeRatings * 2 * sizeof(float)));

        if (weapon.rangeRatings == nullptr)
        {
            Fatal(0, " No RAM for Weapon Range Ratings ");
        }

        std::memset(weapon.rangeRatings, 0, NumRangeRatings * 2 * sizeof(float));
        objectTypeManager->load(
            static_cast<int32_t>(
                weaponFXTable[static_cast<int8_t>(MasterComponentList[inventory[item].masterID].weaponEffect)]),
            1);
    }

    for (int32_t item = firstAmmo; item < numItems; item++)
    {
        std::sprintf(blockName, "Item:%d", item);

        if ((result = mechFile->seekBlock(blockName)) != 0)
        {
            return result;
        }

        InventoryItem& ammo = inventory[item];

        if ((result = mechFile->readIdUChar("MasterID", ammo.masterID)) != 0)
        {
            return result;
        }

        int32_t amount = 0;

        if (mechFile->readIdLong("Amount", amount) != 0)
        {
            uint8_t smallAmount = 0;

            if ((result = mechFile->readIdUChar("Amount", smallAmount)) != 0)
            {
                return result;
            }

            amount = smallAmount;
        }

        if (amount == -1)
        {
            amount = MasterComponentList[ammo.masterID].longValue;
        }

        ammo.amount = static_cast<int16_t>(amount);
        ammo.ammoIndex = -1;
        ammo.startAmount = ammo.amount;
        ammo.health = MasterComponentList[ammo.masterID].health;
        ammo.disabled = 0;
        ammo.readyTime = 0.0f;
        ammo.bodyLocation = 0xff;
        ammo.rangeRatings = nullptr;
    }

    for (int32_t location = 0; location < NUM_MECH_BODY_LOCATIONS; location++)
    {
        if ((result = mechFile->seekBlock(bodyLocationNames[location])) != 0)
        {
            return result;
        }

        uint8_t hasCase = 0;

        if ((result = mechFile->readIdUChar("CASE", hasCase)) != 0)
        {
            return result;
        }

        BodyLocation& bodyLocation = bodyAt(location);
        bodyLocation.hasCASE = hasCase;
        uint8_t internalStructure = 0;

        if ((result = mechFile->readIdUChar("CurInternalStructure", internalStructure)) != 0)
        {
            return result;
        }

        bodyLocation.curInternalStructure = static_cast<float>(internalStructure);

        if ((result = mechFile->readIdUChar("HotSpotNumber", bodyLocation.hotSpotNumber)) != 0)
        {
            return result;
        }

        const float structureLeft =
            bodyLocation.curInternalStructure / static_cast<float>(bodyLocation.maxInternalStructure);

        if (structureLeft == 0.0f)
        {
            bodyLocation.damageState = 2;
        }
        else if (structureLeft > 0.5f)
        {
            bodyLocation.damageState = 0;
        }
        else
        {
            bodyLocation.damageState = 1;
        }

        const int32_t numSpaces = NumLocationCriticalSpaces[location];
        bodyLocation.criticalSpaces = static_cast<CriticalSpace*>(
            ObjectTypeManager::objectCache->malloc(static_cast<uint32_t>(numSpaces * sizeof(CriticalSpace))));
        bodyLocation.totalSpaces = 0;

        if (bodyLocation.criticalSpaces == nullptr)
        {
            return -3;
        }

        for (int32_t space = 0; space < numSpaces; space++)
        {
            char entryName[32];
            std::sprintf(entryName, "Component:%d", space);
            uint8_t entry[2];

            if ((result = mechFile->readIdUCharArray(entryName, entry, 2)) != 0)
            {
                return result;
            }

            CriticalSpace& criticalSpace = bodyAt(location).criticalSpaces[space];
            criticalSpace.inventoryID = entry[0];
            criticalSpace.hit = entry[1];

            if (entry[0] == 0xff)
            {
                continue;
            }

            InventoryItem& item = inventory[entry[0]];
            item.bodyLocation = static_cast<uint8_t>(location);
            bodyAt(location).totalSpaces += static_cast<int8_t>(MasterComponentList[item.masterID].criticalSpacesReq);
            const uint32_t masterID = item.masterID;

            switch (MasterComponentList[masterID].form)
            {
                case COMPONENT_FORM_COCKPIT:
                    cockpit = entry[0];
                    break;
                case COMPONENT_FORM_SENSOR:
                {
                    sensor = entry[0];
                    sensorSystem = sensorSystemManager->newSensor();
                    sensorSystem->owner = this;
                    sensorSystem->setRange(MasterComponentList[inventory[sensor].masterID].rangeOrHeat);
                    break;
                }
                case COMPONENT_FORM_ACTUATOR:
                {
                    if (static_cast<int32_t>(masterID) == MasterArmActuatorID)
                    {
                        if (location == MECH_BODY_LOCATION_LARM)
                        {
                            leftArmActuator = entry[0];
                        }
                        else if (location == MECH_BODY_LOCATION_RARM)
                        {
                            rightArmActuator = entry[0];
                        }
                    }
                    else if (static_cast<int32_t>(masterID) == MasterLegActuatorID)
                    {
                        if (location == MECH_BODY_LOCATION_LLEG)
                        {
                            leftLegActuator = entry[0];
                        }
                        else if (location == MECH_BODY_LOCATION_RLEG)
                        {
                            rightLegActuator = entry[0];
                        }
                    }
                    break;
                }
                case COMPONENT_FORM_ENGINE:
                    engine = entry[0];
                    break;
                case COMPONENT_FORM_HEATSINK:
                case COMPONENT_FORM_WEAPON:
                case COMPONENT_FORM_WEAPON_ENERGY:
                case COMPONENT_FORM_WEAPON_MISSILE:
                    item.bodyLocation = static_cast<uint8_t>(location);
                    break;
                case COMPONENT_FORM_WEAPON_BALLISTIC:
                {
                    item.bodyLocation = static_cast<uint8_t>(location);

                    if (static_cast<int32_t>(masterID) == MasterClanAntiMissileSystemID ||
                        static_cast<int32_t>(masterID) == MasterInnerSphereAntiMissileSystemID)
                    {
                        if (numAntiMissileSystems == 16)
                        {
                            Fatal(0, "Too many Anti-Missile Systems");
                        }

                        antiMissileSystem[numAntiMissileSystems] = entry[0];
                        numAntiMissileSystems++;
                    }
                    break;
                }
                case COMPONENT_FORM_AMMO:
                    item.bodyLocation = static_cast<uint8_t>(location);
                    break;
                case COMPONENT_FORM_LIFESUPPORT:
                    lifeSupport = entry[0];
                    break;
                case COMPONENT_FORM_GYROSCOPE:
                    gyro = entry[0];
                    break;
                case COMPONENT_FORM_ECM:
                    ecm = entry[0];
                    break;
                case COMPONENT_FORM_PROBE:
                    probe = entry[0];
                    break;
                case COMPONENT_FORM_JAMMER:
                    jammer = entry[0];
                    break;
                default:
                    break;
            }
        }
    }

    calcAmmoTotals();

    for (int32_t item = firstWeapon; item < firstAmmo; item++)
    {
        for (int32_t ammoType = 0; ammoType < numAmmoTypes; ammoType++)
        {
            if (static_cast<int32_t>(MasterComponentList[inventory[item].masterID].ammoMasterId) ==
                ammoTypeTotal[ammoType].masterId)
            {
                inventory[item].ammoIndex = static_cast<int16_t>(ammoType);
                break;
            }
        }
    }

    for (int32_t item = firstAmmo; item < numItems; item++)
    {
        for (int32_t ammoType = 0; ammoType < numAmmoTypes; ammoType++)
        {
            if (static_cast<int32_t>(inventory[item].masterID) == ammoTypeTotal[ammoType].masterId)
            {
                inventory[item].ammoIndex = static_cast<int16_t>(ammoType);
                break;
            }
        }
    }

    for (int32_t item = 0; item < firstWeapon; item++)
    {
        const int32_t masterID = inventory[item].masterID;

        if (masterID != MasterClanAntiMissileSystemID && masterID != MasterInnerSphereAntiMissileSystemID)
        {
            continue;
        }

        for (int32_t ammoType = 0; ammoType < numAmmoTypes; ammoType++)
        {
            if (static_cast<int32_t>(MasterComponentList[masterID].ammoMasterId) == ammoTypeTotal[ammoType].masterId)
            {
                inventory[item].ammoIndex = static_cast<int16_t>(ammoType);
                break;
            }
        }
    }

    calcLongestRangeWeapon();
    calcLegStatus();
    calcTorsoStatus();
    maxCV = calcCV(1);
    curCV = calcCV(0);
    maxTargetDamage = calcMaxTargetDamage();

    if (objType->explosionObject > 0)
    {
        objectTypeManager->load(objType->explosionObject, 1);
    }

    mechClass = static_cast<uint8_t>(getMechClass());
    return 0;
}

auto BattleMech::write(File* objFile) -> int32_t
{
    BigGameObject::write(objFile);
    objFile->writeString(debugStatus);
    objFile->writeString(iconName);
    objFile->writeByte(chassis);
    objFile->writeLong(endoSteel);
    objFile->writeFloat(tonnageClass);
    objFile->writeFloat(internalStructureTonnage);

    for (int32_t location = 0; location < NUM_MECH_BODY_LOCATIONS; location++)
    {
        const BodyLocation& bodyLocation = bodyAt(location);
        objFile->writeLong(bodyLocation.hasCASE);
        objFile->write(reinterpret_cast<const uint8_t*>(bodyLocation.criticalSpaces),
                       NumLocationCriticalSpaces[location] * 8);
        objFile->writeFloat(bodyLocation.curInternalStructure);
        objFile->writeByte(bodyLocation.maxInternalStructure);
        objFile->writeByte(bodyLocation.hotSpotNumber);
    }

    objFile->writeByte(armorType);
    objFile->writeFloat(armorTonnage);
    objFile->write(reinterpret_cast<const uint8_t*>(armor), 0x58);
    const int32_t otherCount = numOther;
    const int32_t weaponCount = numWeapons;
    const int32_t ammoCount = numAmmos;
    objFile->writeLong(otherCount);
    objFile->writeLong(weaponCount);
    objFile->writeLong(ammoCount);
    // Original behaviour (OB-007): each list is written from the start of the inventory (the pointer isn't advanced).
    const auto writeItems = [&](int32_t count)
    {
        for (int32_t i = 0; i < count; i++)
        {
            const InventoryItem& item = inventory[i];
            objFile->writeByte(item.masterID);
            objFile->writeByte(item.health);
            objFile->writeByte(item.disabled == 1 ? 1 : 0);
            objFile->writeByte(item.facesForward);
            objFile->writeShort(item.amount);
            objFile->writeByte(item.bodyLocation);
        }
    };

    writeItems(otherCount);
    writeItems(weaponCount);
    writeItems(ammoCount);
    objFile->writeByte(cockpit);
    objFile->writeByte(engine);
    objFile->writeByte(lifeSupport);
    objFile->writeByte(sensor);
    objFile->writeByte(ecm);
    objFile->writeByte(probe);
    objFile->writeByte(jammer);
    objFile->writeByte(static_cast<uint8_t>(numAntiMissileSystems));
    objFile->write(antiMissileSystem, 0x10);
    return objFile->writeFloat(maxRunSpeed);
}

auto BattleMech::calcCV(int calcMax) -> int32_t
{
    double cv = chassisBR;
    const int32_t numItems = numAmmos + numWeapons + numOther;

    for (int32_t i = 0; i < numItems; i++)
    {
        if (calcMax != 0 || inventory[i].disabled == 0)
        {
            cv += MasterComponentList[inventory[i].masterID].battleRating;
        }
    }

    return static_cast<int32_t>(cv);
}

auto BattleMech::calcLegStatus() -> int32_t
{
    const uint8_t leftLeg = bodyAt(MECH_BODY_LOCATION_LLEG).damageState;

    if (bodyAt(MECH_BODY_LOCATION_RLEG).damageState == 2)
    {
        if (leftLeg == 2)
        {
            legStatus = 3;

            if (pilot != nullptr)
            {
                pilot->triggerAlarm(6, 0x42);
                return legStatus;
            }

            return legStatus;
        }

        if (legStatus == 2)
        {
            return legStatus;
        }
    }
    else if (leftLeg != 2)
    {
        legStatus = 0;
        return legStatus;
    }

    pilot->radioMessage(0x1e, 0);
    legStatus = 2;
    return 2;
}

auto BattleMech::calcTorsoStatus() -> int32_t
{
    if (bodyAt(MECH_BODY_LOCATION_CTORSO).damageState == 1)
    {
        torsoStatus = 1;
        return 1;
    }

    torsoStatus = 0;
    return torsoStatus;
}

auto BattleMech::pilotingCheck(uint32_t situation, float modifier) -> void
{
    if ((MPlayer != nullptr && MPlayer->isServer == 0) || pilotingCheckPending != 0)
    {
        return;
    }

    double roll = RandomNumber(100);

    if ((situation & 2) != 0)
    {
        roll += 20.0;
    }

    if (bodyAt(MECH_BODY_LOCATION_RLEG).curInternalStructure == 0.0f ||
        bodyAt(MECH_BODY_LOCATION_LLEG).curInternalStructure == 0.0f)
    {
        roll += 100.0;
    }

    const InventoryItem& gyroItem = inventory[gyro];

    if (gyroItem.health == 0)
    {
        roll += 100.0;
    }
    else if (static_cast<int32_t>(gyroItem.health) < static_cast<int8_t>(MasterComponentList[gyroItem.masterID].health))
    {
        roll += 30.0;
    }

    if (inventory[leftLegActuator].health == 0)
    {
        roll += 10.0;
    }

    if (inventory[rightLegActuator].health == 0)
    {
        roll += 10.0;
    }

    if ((situation & 1) == 0)
    {
        const int failed = static_cast<double>(pilot->skills[MWS_PILOTING]) <= roll ? 1 : 0;
        pilotingCheckPending = failed;
        pilot->skillPoints[MWS_PILOTING] += SkillTry[0];

        if (failed == 0)
        {
            pilot->skillPoints[MWS_PILOTING] += SkillSuccess[0];
        }
    }
    else
    {
        const int failed = static_cast<double>(pilot->skills[MWS_JUMPING] + PilotJumpMod) <= roll ? 1 : 0;
        pilotingCheckPending = failed;
        pilot->skillPoints[MWS_JUMPING] += SkillTry[1];

        if (failed == 0)
        {
            pilot->skillPoints[MWS_JUMPING] += SkillSuccess[1];
        }
    }
}

auto BattleMech::canPowerUp() -> int
{
    return 1;
}

auto BattleMech::destroy() -> void
{
    systemHeap->free(ifaceName);
    ifaceName = nullptr;

    if (statusWindow != nullptr)
    {
        closeStatusWindow();
        statusWindow = nullptr;
    }
}

auto BattleMech::mineCheck() -> void
{
    if ((MPlayer != nullptr && MPlayer->isServer == 0) || isJumping(nullptr) != 0)
    {
        return;
    }

    ScenarioMap* map = GameMap;

    // The mine state bits of a tile's overlay: Inner Sphere 11..12, Clan 13..14; the spread counts 25..26, 27..28.
    if (unknown948 != 0)
    {
        const MapTile& tile = map->map[objPosition->tileR * map->width + objPosition->tileC];
        const uint32_t state = alignment == -1 ? tile.overlay >> 11 : tile.overlay >> 13;

        if ((state & 3) == 0)
        {
            unknown948 = 0;
            const int32_t tileR = objPosition->tileR;
            const int32_t tileC = objPosition->tileC;
            MapTile& here = map->map[map->width * tileR + tileC];

            if (getAlignment() == -1)
            {
                here.overlay = (here.overlay & 0xffffefff) | 0x800;
            }
            else
            {
                here.overlay = (here.overlay & 0xffffbfff) | 0x2000;
            }

            if (MPlayer != nullptr)
            {
                MPlayer->addMineChunk(tileR * 3, tileC * 3, alignment != -1 ? 1 : 0, 1, 0);
                map = GameMap;
            }
        }
    }

    const uint32_t mine =
        alignment == -1
            ? map->getInnerSphereMine(objPosition->tileR, objPosition->tileC, objPosition->cellR, objPosition->cellC)
            : map->getClanMine(objPosition->tileR, objPosition->tileC, objPosition->cellR, objPosition->cellC);

    if (mine == 0)
    {
        return;
    }

    int32_t firstRow = objPosition->tileR - 1;
    int32_t firstCol = objPosition->tileC - 1;

    if (firstRow < 0)
    {
        firstRow = 0;
    }

    if (firstCol < 0)
    {
        firstCol = 0;
    }

    const int32_t mapSide = Terrain::verticesBlockSide * Terrain::blocksMapSide;

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
            const bool inMap = row >= 0 && row < GameMap->height && col >= 0 && col < GameMap->width;
            Assert(inMap ? 1 : 0, 0, " Map Tile out of bounds ");

            // Port fix: the original goes on to touch the tile past the map's edge.
            if (!inMap)
            {
                continue;
            }

            MapTile& tile = GameMap->map[GameMap->width * row + col];
            const bool innerSphere = getAlignment() == -1;
            uint32_t count = ((innerSphere ? tile.overlay >> 25 : tile.overlay >> 27) & 3) + 1;

            if (count > 3)
            {
                count = 3;
            }

            if (getAlignment() == -1)
            {
                tile.overlay = (tile.overlay & 0xf9ffffff) | (count << 25);
            }
            else
            {
                tile.overlay = (tile.overlay & 0xe7ffffff) | (count << 27);
            }
        }
    }

    const int32_t tileR = objPosition->tileR;
    const int32_t tileC = objPosition->tileC;
    MapTile& here = GameMap->map[GameMap->width * tileR + tileC];

    if (getAlignment() == -1)
    {
        here.overlay |= 0x1800;
    }
    else
    {
        here.overlay |= 0x6000;
    }

    if (MPlayer != nullptr)
    {
        MPlayer->addMineChunk(tileR * 3 + objPosition->cellR, tileC * 3 + objPosition->cellC, alignment != -1 ? 1 : 0,
                              3, 2);
    }

    pilot->pausePath();
    vector_3d position = getPosition();
    CreateExplosion(MineExplosion, position, MineSplashDamage, worldUnitsPerMeter * MineSplashRange);
    const int32_t hitLocation = calcHitLocation(nullptr, -1, 3, 0);
    _WeaponShotInfo shotInfo;
    shotInfo.init(nullptr, -2, MineBaseDamage, hitLocation, 0.0f);
    handleWeaponHit(&shotInfo, MPlayer != nullptr);

    if (getPilot() != nullptr)
    {
        getPilot()->radioMessage(0x16, 1);
    }

    unknown948 = 1;
}

auto BattleMech::updateJump() -> int
{
    if (isJumping(nullptr) == 0)
    {
        return 0;
    }

    auto* actor = static_cast<MechActor*>(appearance);
    auto* controlData = static_cast<MechControlData*>(control->controlData);

    if (actor->inJump == 0 && actor->jumpSetup == 0)
    {
        // Landed.
        inJump = 0;
        jumpTime = scenarioTime;
        MovePath* path = pilot->getMovePath();
        pilot->resumePath();
        lastValidPosition = position;
        path->curStep++;
        pilotingCheck(1, 0.0f);
    }

    if (actor->unknown120 == 0)
    {
        if (MPlayer == nullptr || MPlayer->isServer != 0)
        {
            actor->setJumpParameters(jumpGoal, 0);

            if (static_cast<MechActor*>(appearance)->inTransition == 0)
            {
                appearance->setGestureGoal(6);
                controlData->throttle = 100;
            }
        }
        else if (distanceFrom(jumpGoal) > 8.0f)
        {
            actor->setJumpParameters(jumpGoal, 0);

            if (static_cast<MechActor*>(appearance)->inTransition == 0)
            {
                appearance->setGestureGoal(6);
                controlData->throttle = 100;
                return 1;
            }
        }

        return 1;
    }

    // Turn toward the landing point: within two degrees, pivot by the pivot angle.
    float turn = relFacingTo(jumpGoal, -1);

    if (turn >= -2.0f && turn <= 2.0f)
    {
        turn = turn < 0.0f ? -mechPivotAngle : mechPivotAngle;
    }

    unknown8F8 = turn;
    const float maxRate = static_cast<float>(
        static_cast<MechDynamicsType*>(static_cast<BattleMechType*>(objType)->dynamicsType)->maxMechYawRate);
    double rate = -(static_cast<double>(turn) / frameLength);

    if (rate > maxRate)
    {
        rate = maxRate;
    }
    else if (rate < -maxRate)
    {
        rate = -maxRate;
    }

    controlData->rotate = static_cast<int8_t>(static_cast<int32_t>(rate / maxRate * 64.0f));
    return 1;
}

auto BattleMech::pivotTo() -> int
{
    MechWarrior* warrior = pilot;
    MovePath* path = warrior->getMovePath();
    const int32_t moveStateGoal = warrior->moveOrders.moveStateGoal;
    const int32_t moveState = warrior->moveOrders.moveState;
    const int32_t run = MPlayer == nullptr || MPlayer->isServer != 0 ? warrior->moveOrders.run : moveChunk.run;
    int hasTarget = 0;
    GameObject* target = warrior->getLastTarget();
    float targetFacing = 0.0f;
    const float maxPivot =
        static_cast<float>(
            static_cast<MechDynamicsType*>(static_cast<BattleMechType*>(objType)->dynamicsType)->maxMechPivotRate) *
        frameLength;

    if (target == nullptr)
    {
        if (warrior->curTacOrder.code == TACTICAL_ORDER_ATTACK_POINT)
        {
            targetFacing = relFacingTo(warrior->attackOrders.targetPoint, -1);
            hasTarget = 1;
        }
    }
    else
    {
        targetFacing = relFacingTo(target->getPosition(), -1);
        hasTarget = 1;
    }

    // Starts the pivot: a turn of <paramref name="turn"/> degrees, no faster than the pivot rate.
    const auto pivot = [&](float turn) -> int
    {
        if (maxPivot < std::fabs(turn))
        {
            turn = turn <= 0.0f ? -maxPivot : maxPivot;
        }

        auto* controlData = static_cast<MechControlData*>(control->controlData);
        controlData->rotate = static_cast<int8_t>(static_cast<int32_t>(static_cast<double>(turn) / maxPivot * 64.0f));
        controlData->pivot = 1;
        updateTorso(turn);
        return 1;
    };

    const auto choosePivotDirection = [&]()
    {
        if (pivotDirection == 0xff)
        {
            pivotDirection = targetFacing >= 0.0f ? 1 : 0;
        }
    };

    const auto hasNextStep = [&]()
    { return path->numStepsWhenNotPaused >= 1 && path->curStep < path->numStepsWhenNotPaused; };

    if (moveState == MOVESTATE_PIVOT_FORWARD)
    {
        if (moveStateGoal == MOVESTATE_PIVOT_FORWARD || moveStateGoal == MOVESTATE_FORWARD)
        {
            if (!hasNextStep())
            {
                pilot->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
            }
            else
            {
                const vector_3d destination = path->stepList[path->curStep].destination;
                appearance->setGestureGoal(1);
                static_cast<MechControlData*>(control->controlData)->throttle = 100;
                const float stepFacing = relFacingTo(destination, -1);

                if (stepFacing < -15.0f || stepFacing > 15.0f)
                {
                    float turn = -stepFacing;

                    if (hasTarget != 0 && run == 0)
                    {
                        choosePivotDirection();

                        if (pivotDirection == 0)
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

                pilot->moveOrders.moveState = MOVESTATE_FORWARD;

                if (pilot->moveOrders.unknown1030 != 0)
                {
                    pilot->moveOrders.unknown1030 = 0;
                }
            }
        }
        else
        {
            pilot->moveOrders.moveState = MOVESTATE_FORWARD;
        }
    }
    else if (moveState == MOVESTATE_PIVOT_REVERSE)
    {
        if (moveStateGoal == MOVESTATE_PIVOT_REVERSE || moveStateGoal == MOVESTATE_REVERSE)
        {
            if (!hasNextStep())
            {
                pilot->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
            }
            else
            {
                const vector_3d destination = path->stepList[path->curStep].destination;
                appearance->setGestureGoal(1);
                static_cast<MechControlData*>(control->controlData)->throttle = 100;
                const float stepFacing = relFacingTo(destination, -1);

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
                        turnLeft = pivotDirection != 0;
                    }

                    return pivot(turnLeft ? -180.0f - stepFacing : 180.0f - stepFacing);
                }

                MechWarrior* orders = pilot;

                if (orders->moveOrders.unknown1030 != 0)
                {
                    orders->moveOrders.unknown1030 = 0;
                }

                if (moveStateGoal == MOVESTATE_REVERSE)
                {
                    orders->moveOrders.moveState = MOVESTATE_REVERSE;
                }
                else
                {
                    orders->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
                }
            }
        }
        else
        {
            pilot->moveOrders.moveState = MOVESTATE_FORWARD;
        }
    }
    else if (moveState != MOVESTATE_PIVOT_TARGET)
    {
        if (moveStateGoal == MOVESTATE_PIVOT_TARGET || moveStateGoal == MOVESTATE_PIVOT_FORWARD ||
            moveStateGoal == MOVESTATE_PIVOT_REVERSE)
        {
            pilot->moveOrders.moveState = moveStateGoal;
        }
    }
    else if (moveStateGoal != MOVESTATE_PIVOT_TARGET)
    {
        pilot->moveOrders.moveState = MOVESTATE_FORWARD;
    }
    else if (run == 0 && hasTarget != 0)
    {
        appearance->setGestureGoal(1);
        static_cast<MechControlData*>(control->controlData)->throttle = 100;
        const float fireArc = getFireArc();

        if (targetFacing < -fireArc || fireArc < targetFacing)
        {
            return pivot(-targetFacing);
        }

        pilot->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
    }
    else
    {
        pilot->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
    }

    MechWarrior* orders = pilot;

    if (!(orders->moveOrders.yieldTime > -1.0f || orders->moveOrders.waitForPointTime > -1.0f))
    {
        orders->resumePath();
    }

    pivotDirection = 0xff;
    return 0;
}

auto BattleMech::getSpeedState() -> int32_t
{
    return mechSpeedStateArray[static_cast<MechActor*>(appearance)->currentGesture];
}

auto BattleMech::updateMoveStateGoal() -> void
{
    MechWarrior* warrior = pilot;
    MovePath* path = warrior->getMovePath();
    const int32_t moveStateGoal = warrior->moveOrders.moveStateGoal;

    if (path->numSteps < 1)
    {
        if (moveStateGoal != MOVESTATE_PIVOT_TARGET && moveStateGoal != MOVESTATE_PIVOT_FORWARD &&
            moveStateGoal != MOVESTATE_PIVOT_REVERSE)
        {
            warrior->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
        }

        return;
    }

    const int32_t run = MPlayer == nullptr || MPlayer->isServer != 0 ? warrior->moveOrders.run : moveChunk.run;

    if (run != 0 || legStatus == 2)
    {
        warrior->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
        return;
    }

    vector_3d targetPosition;
    GameObject* target = warrior->getLastTarget();

    if (target == nullptr)
    {
        if (warrior->curTacOrder.code != TACTICAL_ORDER_ATTACK_POINT)
        {
            warrior->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
            return;
        }

        targetPosition = warrior->attackOrders.targetPoint;
    }
    else
    {
        targetPosition = target->getPosition();
    }

    if (path->numStepsWhenNotPaused <= 0 || path->curStep >= path->numStepsWhenNotPaused)
    {
        return;
    }

    const double delta = relFacingDelta(path->stepList[path->curStep].destination, targetPosition);
    MechWarrior* orders = pilot;
    const double torsoArc =
        static_cast<MechDynamicsType*>(static_cast<BattleMechType*>(objType)->dynamicsType)->maxTorsoYaw;

    if (orders->moveOrders.moveStateGoal == MOVESTATE_FORWARD)
    {
        // The target is behind: walk backward.
        if (torsoArc < delta && 180.0 - delta <= torsoArc && orders->moveOrders.unknown1030 == 0)
        {
            orders->moveOrders.unknown1030 = 1;
            orders->moveOrders.moveStateGoal = MOVESTATE_REVERSE;
        }
    }
    else if (torsoArc < 180.0 - delta && delta <= torsoArc && orders->moveOrders.unknown1030 == 0)
    {
        orders->moveOrders.unknown1030 = 1;
        orders->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
    }
}

auto BattleMech::updateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                int32_t& newGestureStateGoal, int32_t& newMoveState, int32_t& minThrottle,
                                int32_t& maxThrottle) -> int
{
    MechWarrior* warrior = pilot;
    auto* controlData = static_cast<MechControlData*>(control->controlData);
    auto* dynType = static_cast<MechDynamicsType*>(static_cast<BattleMechType*>(objType)->dynamicsType);
    MovePath* path = warrior->getMovePath();
    int running = legStatus == 0 && warrior->moveOrders.run != 0 ? 1 : 0;
    newThrottleSetting = static_cast<char>(controlData->throttle);
    newRotatePerSec = 0.0f;
    updateHustleTime();
    const bool hustling = scenarioTime < lastHustleTime + 2.0f;
    warrior = pilot;
    Mover* point = warrior->getPoint();
    const bool groupMove = warrior->curTacOrder.isGroupOrder() != 0 && warrior->curTacOrder.isMoveOrder() != 0;

    if (running == 0 && !hustling && point != nullptr && point->isDisabled() == 0 && point != this && groupMove)
    {
        // Keep pace with the group's point: wait (at most five seconds while walking) when ahead of it.
        MechWarrior* pointPilot = point->getPilot();
        pointPilot->getMovePath();
        const float pointDistanceLeft = pointPilot->getMoveDistanceLeft();

        if (pointDistanceLeft <= warrior->getMoveDistanceLeft())
        {
            warrior->moveOrders.waitForPointTime = -1.0f;

            if (warrior->moveOrders.yieldTime <= -1.0f)
            {
                warrior->resumePath();
            }
        }
        else
        {
            running = 0;
            const int32_t speedState = getSpeedState();
            warrior = pilot;

            if (speedState == 2)
            {
                if (warrior->moveOrders.waitForPointTime <= -1.0f)
                {
                    warrior->moveOrders.waitForPointTime = scenarioTime + 5.0f;
                }
            }
            else if (warrior->moveOrders.waitForPointTime < scenarioTime)
            {
                warrior->pausePath();
                warrior->moveOrders.waitForPointTime = 999999.0f;
            }
        }
    }
    else
    {
        warrior->moveOrders.waitForPointTime = -1.0f;
    }

    int result = 0;

    if (legStatus != 0 && legStatus != 1 && legStatus != 2)
    {
        newGestureStateGoal = 1;
        return 0;
    }

    if (path->numSteps < 1)
    {
        newGestureStateGoal = 1;
        return 0;
    }

    int32_t step = path->curStep;

    if (step == path->numSteps)
    {
        result = 1;

        if (warrior->moveOrders.pathType == 2 &&
            warrior->moveOrders.path[0]->globalStep < warrior->moveOrders.numGlobalSteps - 1)
        {
            result = 0;
        }

        if (warrior->moveOrders.path[0] != nullptr)
        {
            warrior->moveOrders.path[0]->clear();
        }

        return result;
    }

    vector_3d destination = path->stepList[step].destination;
    lastValidPosition = destination;
    const float distance = distanceFrom(destination);
    const int32_t numSteps = path->numSteps;
    const float margin = step == numSteps - 1 ? MoveMarginOfError[1] : MoveMarginOfError[0];

    if (margin <= distance)
    {
        if (static_cast<int8_t>(path->stepList[step].direction) > 7)
        {
            newGestureStateGoal = 6;
            return 0;
        }
    }
    else
    {
        // Reached the step: on to the next.
        step++;
        pilot->moveOrders.timeOfLastStep = scenarioTime;
        path->curStep = step;

        if (numSteps <= step)
        {
            warrior = pilot;
            result = 1;

            if (warrior->moveOrders.pathType == 2 &&
                warrior->moveOrders.path[0]->globalStep < warrior->moveOrders.numGlobalSteps - 1)
            {
                result = 0;
            }

            if (warrior->moveOrders.path[0] != nullptr)
            {
                warrior->moveOrders.path[0]->clear();
            }

            return result;
        }

        if (static_cast<int8_t>(path->stepList[step].direction) > 7)
        {
            newGestureStateGoal = 6;
            return 0;
        }

        destination = path->stepList[step].destination;
    }

    const float facing = relFacingTo(destination, -1);
    warrior = pilot;
    const int32_t moveState = warrior->moveOrders.moveState;
    const int32_t moveStateGoal = warrior->moveOrders.moveStateGoal;
    // Walking, the throttle creeps toward the ordered speed by tens.
    const auto walkThrottle = [&]() -> char
    {
        const char throttle = static_cast<char>(controlData->throttle);

        if (getBodyState() != 2)
        {
            return 100;
        }

        const char speed = static_cast<char>(pilot->moveOrders.speedThrottle);

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
            warrior->pausePath();

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

        if (legStatus == 2)
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
            const float maxTurn = static_cast<float>(dynType->maxMechYawRate) * frameLength;

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
                static_cast<int32_t>(std::floor(static_cast<double>(newRotatePerSec / maxTurn * 64.0f))));
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
            const float maxTurn = static_cast<float>(dynType->maxMechYawRate) * frameLength;
            char throttle;

            if (std::fabs(newRotatePerSec) <= maxTurn)
            {
                throttle = walkThrottle();
            }
            else
            {
                newRotatePerSec = newRotatePerSec <= 0.0f ? -maxTurn : maxTurn;
                throttle = static_cast<char>(controlData->throttle - 10);
            }

            newThrottleSetting = throttle;
            newRotate = static_cast<char>(static_cast<int32_t>(static_cast<double>(newRotatePerSec) / maxTurn * 64.0f));
            return result;
        }

        warrior->pausePath();

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
        warrior->pausePath();
        newMoveState = MOVESTATE_PIVOT_FORWARD;
    }
    else if (moveStateGoal == MOVESTATE_REVERSE || moveStateGoal == MOVESTATE_PIVOT_REVERSE)
    {
        warrior->pausePath();
        newMoveState = MOVESTATE_PIVOT_REVERSE;
    }

    return result;
}

auto BattleMech::setNextMovePath(char& newThrottleSetting, int32_t& newGestureStateGoal) -> void
{
    MechWarrior* warrior = pilot;

    if (warrior->playerOrderFromQueue != 0 && warrior->curTacOrder.isMoveOrder() != 0)
    {
        if (warrior->moveOrders.path[0] != nullptr)
        {
            warrior->moveOrders.path[0]->clear();
        }

        return;
    }

    warrior->clearMoveOrders();
    newGestureStateGoal = 1;
}

auto BattleMech::updateTorso(float newRotatePerSec) -> void
{
    MechWarrior* warrior = pilot;
    GameObject* target = warrior->getLastTarget();
    double facing;

    if (target != nullptr)
    {
        facing = static_cast<double>(relFacingTo(target->getPosition(), -1)) + torsoRotation + newRotatePerSec;
    }
    else if (warrior->curTacOrder.code == TACTICAL_ORDER_ATTACK_POINT)
    {
        facing =
            static_cast<double>(relFacingTo(warrior->getAttackTargetPoint(), -1)) + torsoRotation + newRotatePerSec;
    }
    else
    {
        facing = torsoRotation;
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
    auto* dynType = static_cast<MechDynamicsType*>(static_cast<BattleMechType*>(objType)->dynamicsType);
    const float maxTurn = static_cast<float>(dynType->maxTorsoYawRate) * frameLength;

    if (maxTurn < std::fabs(turn))
    {
        turn = turn < 0.0 ? -maxTurn : maxTurn;
    }

    static_cast<MechControlData*>(control->controlData)->torsoRotate =
        static_cast<int8_t>(static_cast<int32_t>(turn / maxTurn * 64.0f));
}

auto BattleMech::setControlSettings(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                    int32_t& newGestureStateGoal, int32_t& minThrottle, int32_t& maxThrottle) -> void
{
    auto* actor = static_cast<MechActor*>(appearance);

    if (inJump != 0 && actor->inJump == 0)
    {
        inJump = 0;
        pilot->resumePath();
    }

    if (newGestureStateGoal == 6)
    {
        // The path's step is a jump.
        MechWarrior* warrior = pilot;
        MovePath* path = warrior->getMovePath();
        warrior->pausePath();
        jumpGoal = path->stepList[path->curStep].destination;
        actor->setJumpParameters(jumpGoal, 0);
    }

    bool startJump = false;

    if (MPlayer == nullptr || MPlayer->isServer != 0)
    {
        MechWarrior* warrior = pilot;

        if (warrior->curTacOrder.isJumpOrder() != 0 && inJump == 0)
        {
            const float* point = warrior->curTacOrder.moveParams.wayPath.points;
            jumpGoal = vector_3d(point[0], point[1], point[2]);
            newGestureStateGoal = 6;
            startJump = true;
        }
    }
    else if (statusChunk.jumpOrder != 0 && inJump == 0)
    {
        mapCellToWorldPos(statusChunk.targetCellRC[0], statusChunk.targetCellRC[1], jumpGoal);

        if (distanceFrom(jumpGoal) > 8.0f)
        {
            newGestureStateGoal = 6;
            startJump = true;
        }
    }

    if (startJump)
    {
        actor->setJumpParameters(jumpGoal, 0);
    }

    const int32_t gestureGoal = newGestureStateGoal;
    auto* controlData = static_cast<MechControlData*>(control->controlData);

    if (gestureGoal != -1 && static_cast<MechActor*>(appearance)->inTransition == 0)
    {
        auto* mechActor = static_cast<MechActor*>(appearance);

        if (mechActor->setGestureGoal(gestureGoal) == 0)
        {
            if (gestureGoal == 6)
            {
                inJump = 1;
            }

            if (gestureGoal != 2)
            {
                controlData->throttle = 100;
            }
        }
        else if (mechActor->inTransition == 0 && mechActor->currentStateGesture == 2)
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

                controlData->throttle = newThrottleSetting;
            }
        }
    }

    if (newRotate != 0)
    {
        controlData->rotate = newRotate;
    }
}

auto BattleMech::updateMovement() -> void
{
    auto* controlData = static_cast<MechControlData*>(control->controlData);
    int32_t minThrottle = 0x23;
    int32_t maxThrottle = 100;
    // A fall: gesture 7 or 8 (at random unless forced).
    const auto fallGesture = [&]() -> int32_t
    {
        int32_t gesture = 8 - (RandomNumber(2) != 0 ? 1 : 0);

        if (unknown8C8 != 0)
        {
            gesture = 7;
        }
        else if (unknown8CC != 0)
        {
            gesture = 8;
        }

        return gesture;
    };

    if (disableThisFrame != 0)
    {
        if (appearance->setGestureGoal(fallGesture()) == 0)
        {
            disableThisFrame = 0;
            shutDownThisFrame = 0;
            startUpThisFrame = 0;
            unknown8CC = 0;
            unknown8C8 = 0;
        }

        controlData->throttle = static_cast<int8_t>(maxThrottle);
        return;
    }

    if (shutDownThisFrame != 0)
    {
        const int32_t result = appearance->setGestureGoal(0);
        soundSystem->playDigitalSample(0x3c, 1, this, 0, 0);

        if (result == 0 || result == -0x1521ffff)
        {
            shutDownThisFrame = 0;
            startUpThisFrame = 0;

            if (result == -0x1521ffff)
            {
                status = 5;
            }
        }

        controlData->throttle = static_cast<int8_t>(maxThrottle);
        return;
    }

    if (startUpThisFrame != 0)
    {
        const int32_t result = appearance->setGestureGoal(1);
        soundSystem->playDigitalSample(0x3d, 1, this, 0, 0);

        if (result == 0 || result == -0x1521ffff)
        {
            startUpThisFrame = 0;
            shutDownThisFrame = 0;

            if (result == -0x1521ffff)
            {
                status = 0;
            }
        }

        controlData->throttle = static_cast<int8_t>(maxThrottle);
        return;
    }

    if (status == 4 || status == 5 || status == 1)
    {
        return;
    }

    if (isCaptured() != 0 || unknown170 > -1.0f)
    {
        return;
    }

    if (pilotingCheckPending != 0)
    {
        const int32_t result = appearance->setGestureGoal(fallGesture());

        if (result == 0 || result == -0x1521ffff)
        {
            pilotingCheckPending = 0;
        }

        controlData->throttle = static_cast<int8_t>(maxThrottle);
        return;
    }

    if (updateJump() != 0)
    {
        return;
    }

    float newRotatePerSec = static_cast<float>(pivotTo());

    if (newRotatePerSec != 0.0f)
    {
        return;
    }

    char newRotate = 0;
    char newThrottleSetting = -1;
    int32_t newGestureStateGoal = -1;
    int32_t newMoveState = -1;
    updateMoveStateGoal();

    if (updateMovePath(newRotate, newThrottleSetting, newRotatePerSec, newGestureStateGoal, newMoveState, minThrottle,
                       maxThrottle) != 0)
    {
        setNextMovePath(newThrottleSetting, newGestureStateGoal);
    }

    if (newMoveState != -1)
    {
        pilot->moveOrders.moveState = newMoveState;
    }

    setControlSettings(newRotate, newThrottleSetting, newRotatePerSec, newGestureStateGoal, minThrottle, maxThrottle);
    updateTorso(newRotatePerSec);
}

namespace
{
    /// <summary>
    /// The sine and cosine of a facing snapped to the sprites' 32 directions (-45 and 45 are exact), as
    /// getPositionFromHS and getJumpPosition turn their offsets.
    /// </summary>
    void snappedFacing(double facing, double& s, double& c)
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

    /// <summary>Radians to degrees, as MCX.EXE stores it (MCX.EXE @ 0x0077c278).</summary>
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;
}

auto BattleMech::getPositionFromHS(uint32_t hotSpot) -> vector_3d
{
    auto* mechType = static_cast<BattleMechType*>(objType);

    if (mechType->numOthers + mechType->numWeapons <= hotSpot)
    {
        hotSpot = 0;
    }

    auto* actor = static_cast<MechActor*>(appearance);
    const uint32_t gesture = actor->getHotSpotIndex(static_cast<uint32_t>(actor->currentGesture));
    int32_t frameNumber = actor->currentFrame[0];
    const auto* offsets = reinterpret_cast<const float*>(mechType->gestureHotSpots[gesture]);
    const int32_t numFrames = static_cast<int32_t>(mechType->numFramesPerHotSpot[gesture]);

    if (numFrames <= frameNumber)
    {
        frameNumber = numFrames - 1;
    }

    // Port fix: some packets hold fewer hot spots than numWeapons + numOthers (cm.hsp's gestures 0-14 hold 3 of 6),
    // and the original reads past them into the heap. Read hot spot 0's offset, as the range check above does (the
    // mount's turn below still uses the real hot spot).
    uint32_t dataHotSpot = hotSpot;

    if (gesture < mechType->numHotSpotsPerGesture.size() && mechType->numHotSpotsPerGesture[gesture] <= dataHotSpot)
    {
        dataHotSpot = 0;
    }

    const int32_t index = numFrames * static_cast<int32_t>(dataHotSpot) + frameNumber;
    const float offsetX = offsets[index * 3];
    const float offsetY = offsets[index * 3 + 1];
    const float offsetZ = offsets[index * 3 + 2];

    // The body's facing, plus the torso's (and an arm's) for the weapons mounted on them.
    float facing = static_cast<float>(frame.my_acos(UnitX.z * frame.i.z + UnitX.y * frame.i.y + UnitX.x * frame.i.x) *
                                      RADIANS_TO_DEGREES);

    if (frame.i.y < 0.0f)
    {
        facing = -facing;
    }

    double turned = facing;

    if (hotSpot < mechType->numWeapons)
    {
        switch (mechType->weaponHotSpots[hotSpot])
        {
            case 1:
                turned = static_cast<double>(facing) + torsoRotation;
                break;
            case 2:
                turned = static_cast<double>(leftArmRotation) + torsoRotation + facing;
                break;
            case 3:
                turned = static_cast<double>(rightArmRotation) + torsoRotation + facing;
                break;
            default:
                break;
        }
    }
    else if (hotSpot < mechType->numWeapons + 3)
    {
        turned = static_cast<double>(facing) + torsoRotation;
    }

    double s;
    double c;
    snappedFacing(turned, s, c);
    vector_3d result;
    result.x = static_cast<float>(c * offsetX + s * offsetY) * 20.0f + position.x;
    result.z = offsetZ * 20.0f + position.z;
    result.y = static_cast<float>((c * offsetY - s * offsetX) * 20.0f + position.y);
    return result;
}

auto BattleMech::onScreen() -> int
{
    Camera* camera = cameraList->findCameraFromIDNumber(1);
    screenPos.y = 0.0f;
    screenPos.x = 0.0f;

    if (camera == nullptr || camera->active == 0)
    {
        return 0;
    }

    float screenY;

    if (useOldProject == 0)
    {
        vector_2d screen100;
        vector_2d screen50;

        if (land != nullptr)
        {
            land->projectTerrain(position, screen100, screen50);
        }

        if (camera->cameraScale == 1)
        {
            screenPos.x = (screen50.x - camera->screenUL50.x) + camera->halfWidth;
            screenY = screen50.y - camera->screenUL50.y;
        }
        else
        {
            screenPos.x = (screen100.x - camera->screenUL.x) + camera->halfWidth;
            screenY = screen100.y - camera->screenUL.y;
        }

        screenY += camera->halfHeight;
    }
    else
    {
        const float scale = camera->cameraScale != 1 ? 1.0f : 0.5f;
        vector_3d relative(position.x - camera->position.x, position.y - camera->position.y,
                           position.z - camera->position.z);
        relative *= scale;
        screenPos.x = relative.y * camera->cosAngle + relative.x * camera->cosAngle + camera->halfWidth;
        screenY = ((relative.x * camera->sinAngle + camera->halfHeight) - relative.y * camera->sinAngle) - relative.z;
    }

    screenPos.y = screenY;

    if (appearance != nullptr && appearance->recalcBounds(camera) != 0)
    {
        windowsVisible = turn;
        return 1;
    }

    return 0;
}

auto BattleMech::createJumpFX() -> void
{
    if (jumpFX[0] != nullptr || jumpFX[1] != nullptr)
    {
        return;
    }

    jumpFX[0] = createObject(0x1c6);
    static_cast<Jet*>(jumpFX[0])->setOwner(this);
    jumpFX[1] = createObject(0x1c6);
    static_cast<Jet*>(jumpFX[1])->setOwner(this);
    craterManager->addCrater(7, position, 0);
}

auto BattleMech::endJumpFX() -> void
{
    if (jumpFX[0] == nullptr && jumpFX[1] == nullptr)
    {
        return;
    }

    delete jumpFX[0];
    jumpFX[0] = nullptr;
    delete jumpFX[1];
    jumpFX[1] = nullptr;
}

auto BattleMech::getJumpPosition(int32_t jet) -> vector_3d
{
    if (jet < 0 || jet > 1)
    {
        jet = 0;
    }

    auto* actor = static_cast<MechActor*>(appearance);
    const int32_t frameNumber = actor->currentFrame[0];
    const int32_t numFrames = static_cast<int32_t>(actor->getNumFramesInGesture(0x14));
    const int32_t index = numFrames * jet + frameNumber;
    const auto* offsets = reinterpret_cast<const float*>(static_cast<BattleMechType*>(objType)->jumpData);
    const float offsetX = offsets[index * 3];
    const float offsetY = offsets[index * 3 + 1];
    const float offsetZ = offsets[index * 3 + 2];
    float facing = static_cast<float>(frame.my_acos(UnitX.z * frame.i.z + UnitX.y * frame.i.y + UnitX.x * frame.i.x) *
                                      RADIANS_TO_DEGREES);

    if (frame.i.y < 0.0f)
    {
        facing = -facing;
    }

    double s;
    double c;
    snappedFacing(facing, s, c);
    vector_3d base = position;

    if (actor->unknownE8 != nullptr)
    {
        // Lifted along the mech's up axis by the jump's height this frame.
        const float height = actor->unknownE8[frameNumber] * 30.0f;
        base.x = frame.k.x * height + base.x;
        base.y = base.y + frame.k.y * height;
        base.z = base.z + height * frame.k.z;
    }

    vector_3d result;
    result.x = base.x + static_cast<float>(c * offsetX + offsetY * s) * 20.0f;
    result.z = offsetZ * 20.0f + base.z;
    result.y = static_cast<float>((offsetY * c - s * offsetX) * 20.0f) + base.y;
    return result;
}

auto BattleMech::crashAvoidanceSystem() -> int
{
    if (MPlayer != nullptr && MPlayer->isServer == 0)
    {
        return 0;
    }

    MechWarrior* warrior = pilot;
    MovePath* path = warrior->getMovePath();

    if (path->numStepsWhenNotPaused == 0)
    {
        return 0;
    }

    if (warrior->moveOrders.waitForPointTime > 999990.0f)
    {
        return 0;
    }

    // A look a frame ahead along the frame turned by a quarter pi (its result is unused).
    const float speed = -static_cast<MechActor*>(appearance)->getVelocityMagnitude();
    frame_of_ref ahead = frame;
    rotateAboutK(ahead, static_cast<float>(std::sin(HALF_PI / 2.0)), static_cast<float>(std::cos(HALF_PI / 2.0)));
    vector_3d lookAhead(ahead.j.x * speed * frameLength * worldUnitsPerMeter + position.x,
                        ahead.j.y * speed * frameLength * worldUnitsPerMeter + position.y,
                        worldUnitsPerMeter * 0.0f + position.z);
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    GameMap->worldToMapPos(lookAhead, tileR, tileC, cellR, cellC);

    int cornerBlocked = 0;
    const int32_t direction = static_cast<int8_t>(path->stepList[path->curStep].direction);

    if (direction == 1 || direction == 3 || direction == 5 || direction == 7)
    {
        // A diagonal step: blocked when both cells beside it are locked.
        const int first = getAdjacentCellPathLocked(objPosition->tileR, objPosition->tileC, objPosition->cellR,
                                                    objPosition->cellC, adjClippedCell[direction][0]);
        const int second = getAdjacentCellPathLocked(objPosition->tileR, objPosition->tileC, objPosition->cellR,
                                                     objPosition->cellC, adjClippedCell[direction][1]);
        cornerBlocked = first != 0 && second != 0 ? 1 : 0;
    }

    int lockReachedEnd = 0;
    int blockReachedEnd = 0;
    const int locked = getPathRangeLock(crashAvoidPath, &lockReachedEnd);
    const int blocked = getPathRangeBlocked(crashAvoidPath, &blockReachedEnd);
    const int32_t closedGates = path->crossesClosedGate(-1, 2);
    warrior = pilot;
    const bool clear = locked == 0 && blocked == 0 && cornerBlocked == 0 && closedGates < 1;

    if (warrior->moveOrders.yieldTime > -1.0f)
    {
        // Yielding: go on once the way is clear.
        if (clear)
        {
            warrior->resumePath();
            warrior->moveOrders.yieldTime = -1.0f;
            return 0;
        }

        warrior->pausePath();
        return 1;
    }

    if (clear)
    {
        return 0;
    }

    if (lockReachedEnd == 0 && blockReachedEnd == 0)
    {
        warrior->pausePath();
        warrior->moveOrders.yieldTime = scenarioTime + crashYieldTime;
        control->controlData->brake();
        return 1;
    }

    warrior->reachedPathEnd();
    control->controlData->brake();
    return 1;
}

auto BattleMech::netUpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                   int32_t& newGestureStateGoal, int32_t& newMoveState, int32_t& minThrottle,
                                   int32_t& maxThrottle) -> int
{
    auto* dynType = static_cast<MechDynamicsType*>(static_cast<BattleMechType*>(objType)->dynamicsType);
    auto* controlData = static_cast<MechControlData*>(control->controlData);
    MovePath* path = pilot->getMovePath();
    const int running = legStatus == 0 && moveChunk.run != 0 ? 1 : 0;
    newThrottleSetting = static_cast<char>(controlData->throttle);
    newRotatePerSec = 0.0f;

    if ((legStatus != 0 && legStatus != 1 && legStatus != 2) || path->numSteps < 1)
    {
        newGestureStateGoal = 1;
        return 0;
    }

    int32_t step = path->curStep;

    if (step == path->numSteps)
    {
        return 1;
    }

    vector_3d destination = path->stepList[step].destination;
    lastValidPosition = destination;
    const float distance = distanceFrom(destination);
    const int32_t numSteps = path->numSteps;
    const float margin = step == numSteps - 1 ? MoveMarginOfError[1] : MoveMarginOfError[0];

    if (margin <= distance)
    {
        if (static_cast<int8_t>(path->stepList[step].direction) > 7)
        {
            newGestureStateGoal = 6;
            return 0;
        }
    }
    else
    {
        step++;
        pilot->moveOrders.timeOfLastStep = scenarioTime;
        path->curStep = step;

        if (numSteps <= step)
        {
            return 1;
        }

        if (static_cast<int8_t>(path->stepList[step].direction) > 7)
        {
            newGestureStateGoal = 6;
            return 0;
        }

        destination = path->stepList[step].destination;
    }

    const float facing = relFacingTo(destination, -1);
    MechWarrior* warrior = pilot;
    const int32_t moveState = warrior->moveOrders.moveState;
    const int32_t moveStateGoal = warrior->moveOrders.moveStateGoal;
    const auto walkThrottle = [&]() -> char
    {
        const char throttle = static_cast<char>(controlData->throttle);

        if (getBodyState() != 2)
        {
            return 100;
        }

        const char speed = static_cast<char>(pilot->moveOrders.speedThrottle);

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
    const float maxRate = static_cast<float>(dynType->maxMechYawRate);

    if (moveState == MOVESTATE_FORWARD && moveStateGoal == MOVESTATE_FORWARD)
    {
        if (legStatus == 2)
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

        newRotatePerSec = -(facing / frameLength);

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
        newRotatePerSec = facing >= 0.0f ? -((facing - 180.0f) / frameLength) : -((facing + 180.0f) / frameLength);

        if (newRotatePerSec > maxRate)
        {
            newRotatePerSec = maxRate;
            newThrottleSetting = static_cast<char>(controlData->throttle - 10);
        }
        else if (newRotatePerSec < -maxRate)
        {
            newRotatePerSec = -maxRate;
            newThrottleSetting = static_cast<char>(controlData->throttle - 10);
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

    warrior->pausePath();
    newMoveState = pivotState;
    return 0;
}

auto BattleMech::netUpdateMovement() -> void
{
    auto* controlData = static_cast<MechControlData*>(control->controlData);
    int32_t minThrottle = 0x23;
    int32_t maxThrottle = 100;
    const int32_t bodyState = getBodyState();
    MovePath* path = pilot->getMovePath();
    vector_3d destination = path->stepList[path->curStep].destination;
    const float distance = distanceFrom(destination);

    if (path->numStepsWhenNotPaused > 0 && bodyState == 0)
    {
        startUpThisFrame = 1;
    }

    if (path->numSteps - 1 <= path->curStep && distance < MoveMarginOfError[1])
    {
        // At the end of the path: take up the body state the server sent.
        startUpThisFrame = 0;
        int32_t gesture = -1;

        switch (statusChunk.bodyState)
        {
            case 1:
            {
                if (bodyState != 1)
                {
                    if (bodyState == 0)
                    {
                        soundSystem->playDigitalSample(0x3d, 1, this, 0, 0);
                    }

                    gesture = 1;
                }
                break;
            }
            case 2:
            {
                if (bodyState != 0)
                {
                    soundSystem->playDigitalSample(0x3c, 1, this, 0, 0);
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
            pilot->clearMoveOrders();
            appearance->setGestureGoal(gesture);
            controlData->throttle = static_cast<int8_t>(maxThrottle);
            return;
        }
    }

    if (disableThisFrame != 0)
    {
        int32_t gesture = 8 - (RandomNumber(2) != 0 ? 1 : 0);

        if (unknown8C8 != 0)
        {
            gesture = 7;
        }
        else if (unknown8CC != 0)
        {
            gesture = 8;
        }

        if (appearance->setGestureGoal(gesture) == 0)
        {
            disableThisFrame = 0;
            shutDownThisFrame = 0;
            startUpThisFrame = 0;
            unknown8CC = 0;
            unknown8C8 = 0;
        }

        controlData->throttle = static_cast<int8_t>(maxThrottle);
        return;
    }

    if (shutDownThisFrame != 0)
    {
        const int32_t result = appearance->setGestureGoal(0);

        if (result == 0 || result == -0x1521ffff)
        {
            shutDownThisFrame = 0;
            startUpThisFrame = 0;

            if (result == -0x1521ffff)
            {
                status = 5;
            }
        }

        controlData->throttle = static_cast<int8_t>(maxThrottle);
        return;
    }

    if (startUpThisFrame != 0)
    {
        const int32_t result = appearance->setGestureGoal(1);

        if (result == 0 || result == -0x1521ffff)
        {
            startUpThisFrame = 0;
            shutDownThisFrame = 0;

            if (result == -0x1521ffff)
            {
                status = 0;
            }
        }

        controlData->throttle = static_cast<int8_t>(maxThrottle);
        return;
    }

    if (status == 4 || status == 5 || status == 1 || isCaptured() != 0 || unknown170 > -1.0f)
    {
        return;
    }

    if (updateJump() != 0)
    {
        return;
    }

    float newRotatePerSec = static_cast<float>(pivotTo());

    if (newRotatePerSec != 0.0f)
    {
        return;
    }

    char newRotate = 0;
    char newThrottleSetting = -1;
    int32_t newGestureStateGoal = -1;
    int32_t newMoveState = -1;
    updateMoveStateGoal();
    netUpdateMovePath(newRotate, newThrottleSetting, newRotatePerSec, newGestureStateGoal, newMoveState, minThrottle,
                      maxThrottle);

    if (newMoveState != -1)
    {
        pilot->moveOrders.moveState = newMoveState;
    }

    setControlSettings(newRotate, newThrottleSetting, newRotatePerSec, newGestureStateGoal, minThrottle, maxThrottle);
    updateTorso(newRotatePerSec);
}

namespace
{
    /// <summary>Pi, as MCX.EXE stores it (a hair under the true value).</summary>
    constexpr double MCX_PI = 0x1.921fb5443e88cp+1;

    /// <summary>
    /// Turns (<paramref name="x"/>, <paramref name="y"/>) by <paramref name="degrees"/> (MC2's inline Rotate): 45 and
    /// -45 exactly, otherwise the sine at full precision and the cosine through a float angle.
    /// </summary>
    void rotateXY(float& x, float& y, float degrees)
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
    void rotateXYHalf(float& x, float& y)
    {
        const double s = std::sin(MCX_PI);
        const double c = std::cos(MCX_PI);
        const double oldX = x;
        x = static_cast<float>(c * x + s * y);
        y = static_cast<float>(c * y - s * oldX);
    }

    /// <summary>A frame's facing in degrees from the world's x axis, negative when its i axis points to -y.</summary>
    float frameFacing(frame_of_ref& frame)
    {
        float facing = static_cast<float>(
            frame.my_acos(UnitX.z * frame.i.z + UnitX.y * frame.i.y + UnitX.x * frame.i.x) * RADIANS_TO_DEGREES);

        if (frame.i.y < 0.0f)
        {
            facing = -facing;
        }

        return facing;
    }

    /// <summary>
    /// Throws off an arm (debris type <paramref name="debrisId"/>): framed the torso's way, flying sideways at a
    /// random angle from <paramref name="angle"/>, painted as the mech.
    /// </summary>
    void throwArm(BattleMech* mech, uint32_t debrisId, float angle)
    {
        GameObject* piece = createObject(static_cast<int32_t>(debrisId));

        if (piece == nullptr)
        {
            return;
        }

        frame_of_ref armFrame = mech->frame;
        const double torso = static_cast<double>(mech->torsoRotation) * DEGREES_TO_RADIANS;
        rotateAboutK(armFrame, static_cast<float>(std::sin(torso)), static_cast<float>(std::cos(torso)));
        piece->setFrame(armFrame);
        vector_3d flight = mech->frame.j;
        const float length = std::sqrt(flight.x * flight.x + flight.y * flight.y + flight.z * flight.z);

        if (length != 0.0f)
        {
            flight.x = flight.x / length;
            flight.y = flight.y / length;
            flight.z = flight.z / length;
        }

        auto* debris = static_cast<Debris*>(piece);
        debris->randomAngle(angle);
        rotateXY(flight.x, flight.y, angle);

        if (frameFacing(armFrame) >= 0.0f)
        {
            rotateXYHalf(flight.x, flight.y);
        }

        piece->setVelocity(flight);
        piece->setPosition(mech->position);
        debris->setPaintScheme(static_cast<MechActor*>(mech->appearance)->fadeTableIndex);

        if (objectList->head != nullptr)
        {
            objectList->head->addNode(piece);
        }
    }

    /// <summary>
    /// Leaves a footprint at hot spot offset (<paramref name="offsetX"/>, <paramref name="offsetY"/>) turned by
    /// -<paramref name="angle"/> degrees, rotation <paramref name="direction"/> (of 16), with a step sound.
    /// </summary>
    void makeFootprint(BattleMech* mech, float offsetX, float offsetY, float angle, int32_t direction)
    {
        rotateXY(offsetX, offsetY, -angle);
        vector_3d printPos;
        printPos.z = mech->position.z;
        printPos.x = offsetX * 20.0f + mech->position.x;
        printPos.y = offsetY * 20.0f + mech->position.y;
        craterManager->addCrater(static_cast<BattleMechType*>(mech->objType)->footprintType, printPos, direction);
        soundSystem->playDigitalSample(0xd, 1, mech, 0, 0);
    }

    /// <summary>A footprint's rotation (of 16) for <paramref name="degrees"/>.</summary>
    int32_t footprintDirection(float degrees)
    {
        auto direction = static_cast<int32_t>(std::floor(static_cast<double>(degrees * (1.0f / 22.5f))));

        if (direction < 0)
        {
            direction += 16;
        }

        return direction;
    }
}

auto BattleMech::update() -> int32_t
{
    terrainNormal = land->getTerrainNormal(position);
    updatePathLock(0);

    if (isDestroyed() != 0 || isDisabled() != 0)
    {
        collisionsOn = 0;
    }

    if (unknown79C != 0 && pilot->status == 2)
    {
        collisionsOn = 0;
        return 1;
    }

    auto* actor = static_cast<MechActor*>(appearance);

    if (isDestroyed() != 0)
    {
        if (jumpFX[0] != nullptr || jumpFX[1] != nullptr)
        {
            endJumpFX();
        }

        int32_t result = dynamics->update();

        if (result != 1)
        {
            return result;
        }

        // The wreck keeps sliding along its frame's j axis turned an eighth of a circle.
        const float speed = -actor->getVelocityMagnitude();
        frame_of_ref turned = frame;
        rotateAboutK(turned, static_cast<float>(std::sin(HALF_PI / 2.0)), static_cast<float>(std::cos(HALF_PI / 2.0)));
        velocity.x = speed * turned.j.x;
        velocity.y = speed * turned.j.y;
        velocity.z = speed * turned.j.z;
        vector_3d newPosition;
        newPosition.x = velocity.x * frameLength * worldUnitsPerMeter + position.x;
        newPosition.y = velocity.y * frameLength * worldUnitsPerMeter + position.y;
        newPosition.z = velocity.z * frameLength * worldUnitsPerMeter + position.z;
        setPosition(newPosition);
        const int visibleNow = onScreen();

        if (actor != nullptr)
        {
            actor->setGestureGoal(8);
            actor->visible = visibleNow;
            actor->setCombatMode(0);
            result = actor->update();

            if (result != 1)
            {
                return result;
            }
        }

        // Once the death animation is done, it blows up and leaves a crater.
        if (unknown8EC != 0 || (unknown8EC = actor->unknown160) != 0)
        {
            unknown794 -= frameLength;

            if (unknown794 < 0.4 && unknown798 == 0)
            {
                auto* mechType = static_cast<BattleMechType*>(objType);
                mechType->createExplosion(position, mechType->explDmg, mechType->explRad);
                unknown798 = 1;
                return 1;
            }

            if (unknown794 < 0.0 && unknown8F0 == 0)
            {
                actor->unknown18C = 1;
                craterManager->addCrater(6, position, 0);
                theInterface->RemoveMech(partId);
                unknown8F0 = 1;
                return 1;
            }
        }
    }
    else
    {
        if (getAwake() != 0 && isDisabled() == 0 && scenario->godMode == 0 && Terrain::metersPerVertex <= unknown7C8)
        {
            // Every vertex travelled, the mech marks what it sees.
            if (alignment == 1)
            {
                land->markSeen(position, frame.j, 360.0f, getProbeEffect() + scenario->maxVisualRange, 1);
            }
            else if (alignment == -1)
            {
                land->markSeen(position, frame.j, 360.0f, getProbeEffect() + scenario->maxVisualRange, 2);
            }

            unknown7C8 = 0.0f;
        }

        if (deselectTime != 0.0f && deselectTime < scenarioTime)
        {
            deselectTime = 0.0f;
            selected = 0;
        }

        int32_t result = control->update();

        if (result != 1)
        {
            return result;
        }

        if (getAwake() == 0 && actor->setGestureGoal(0) == 0)
        {
            shutDownThisFrame = 0;
        }

        result = dynamics->update();

        if (result != 1)
        {
            return result;
        }

        int avoiding = 0;

        if (isDisabled() == 0)
        {
            // The original looks at the pilot's attack order here and does nothing with it.
            if (getPilot()->curTacOrder.code == TACTICAL_ORDER_ATTACK_OBJECT &&
                getPilot()->curTacOrder.attackParams.method == 2)
            {
                getPilot();
            }

            avoiding = crashAvoidanceSystem();
        }

        float speed = 0.0f;

        if (avoiding == 0)
        {
            speed = actor->getVelocityMagnitude();
        }

        const int32_t gesture = actor->currentGesture;
        frame_of_ref turned = frame;
        speed = -speed;

        if (gesture == 20)
        {
            // Jumping: the actor's jump velocity.
            float jumpSpeed = 0.0f;

            if (actor->unknown120 != 0)
            {
                jumpSpeed = actor->getVelocityMagnitude();
            }

            velocity.x = jumpSpeed * actor->unknown100.x;
            velocity.y = jumpSpeed * actor->unknown100.y;
            velocity.z = jumpSpeed * actor->unknown100.z;
        }
        else
        {
            rotateAboutK(turned, static_cast<float>(std::sin(HALF_PI / 2.0)),
                         static_cast<float>(std::cos(HALF_PI / 2.0)));
            velocity.y = turned.j.y * speed;
            velocity.x = turned.j.x * speed;
            velocity.z = turned.j.z * speed;

            if (jumpFX[0] != nullptr || jumpFX[1] != nullptr)
            {
                endJumpFX();
            }
        }

        const float velocityZ = velocity.z;
        vector_3d move;
        move.x = velocity.x * frameLength * worldUnitsPerMeter;
        move.y = velocity.y * frameLength * worldUnitsPerMeter;
        velocity.z = 0.0f;
        move.z = velocityZ * frameLength * worldUnitsPerMeter;

        if (unknown20C != 0)
        {
            // A new move chunk: warp to its first step when too far off.
            if (statusChunk.jumpOrder == 0)
            {
                const int32_t tileR = moveChunk.stepPos[0][0];
                vector_3d stepPos;
                mapTileCellToWorldPos(tileR, moveChunk.stepPos[0][1], moveChunk.stepPos[0][2], moveChunk.stepPos[0][3],
                                      stepPos);
                // Original behaviour (OB-006): measures z against 0, not the mech's elevation.
                const float dx = position.x - stepPos.x;
                const float dz = -stepPos.z;
                const float dy = position.y - stepPos.y;

                if (WarpFactor < std::sqrt(dx * dx + dz * dz + dy * dy))
                {
                    move.x = stepPos.x - position.x;
                    move.y = stepPos.y - position.y;
                    move.z = stepPos.z;
                }

                if (tileR < 0 || GameMap->height <= tileR || moveChunk.stepPos[0][1] < 0 ||
                    GameMap->width <= moveChunk.stepPos[0][1])
                {
                    Fatal(0, " mech.update: newMoveChunk stepPos not on map! ", nullptr);
                }
            }

            unknown20C = 0;
        }

        vector_3d newPosition;
        newPosition.x = move.x + position.x;
        newPosition.y = move.y + position.y;
        newPosition.z = move.z + position.z;
        setPosition(newPosition);
        unknown7C8 = std::sqrt(move.x * move.x + move.z * move.z + move.y * move.y) + unknown7C8;

        if (isDisabled() == 0)
        {
            updatePathLock(1);
        }

        mineCheck();
        position.z = land->getTerrainElevation(position);

        // Arms blown off this frame fly off to the side they were on.
        const float facing = frameFacing(frame);
        auto* controlData = static_cast<MechControlData*>(control->controlData);
        auto* mechType = static_cast<BattleMechType*>(objType);

        if (controlData->unknown18 != 0)
        {
            if (0.0f <= facing + torsoRotation)
            {
                throwArm(this, mechType->leftArmDebrisId, -180.0f);
            }
            else
            {
                throwArm(this, mechType->rightArmDebrisId, 0.0f);
            }

            actor->unknown12C = 1;
        }

        if (controlData->unknown14 != 0)
        {
            if (0.0f <= facing + torsoRotation)
            {
                throwArm(this, mechType->rightArmDebrisId, 0.0f);
            }
            else
            {
                throwArm(this, mechType->leftArmDebrisId, -180.0f);
            }

            actor->unknown130 = 1;
        }

        const int visibleNow = onScreen();

        if (unknown79C != 0 && visibleNow == 0 && pilot->status != 2)
        {
            objType->handleDestruction(this, nullptr);
        }

        if (actor != nullptr)
        {
            actor->visible = visibleNow;
            actor->setMovePath(pilot->getMovePath());
            int combat = 1;

            if (pilot->getLastTarget() == nullptr && pilot->curTacOrder.code != TACTICAL_ORDER_ATTACK_OBJECT &&
                pilot->curTacOrder.code != TACTICAL_ORDER_ATTACK_POINT)
            {
                combat = 0;
            }

            actor->setCombatMode(combat);
            actor->update();

            if (isJumping(nullptr) == 0)
            {
                if (isDestroyed() == 0 && isDisabled() == 0)
                {
                    collisionsOn = 1;
                }
            }
            else
            {
                collisionsOn = 0;
            }
        }

        // Footprints: each foot prints once when its hot spot packet's frame comes round (within two frames), and
        // is re-armed by the walking gestures once past it.
        if (visibleNow != 0 && footPrints != 0 && gesture != 20 && isRevealed() != 0)
        {
            const int32_t gestureNow = actor->currentGesture;
            const uint32_t packetIndex = actor->getHotSpotIndex(static_cast<uint32_t>(gestureNow));
            const int32_t frameNow = actor->currentFrame[0];

            if (static_cast<int32_t>(packetIndex) <= static_cast<int32_t>(mechType->numHotSpotPackets) &&
                mechType->hotSpotData != nullptr)
            {
                const auto* packet = reinterpret_cast<const int32_t*>(mechType->hotSpotData + packetIndex * 0x20);
                const auto* offsets = reinterpret_cast<const float*>(packet);
                const int walking = gestureNow == 4 || gestureNow == 7 || gestureNow == 11;
                // A mirrored actor swaps the feet's offsets. (The original also checks, dead, for a half turn.)
                const int mirrored = actor->reverse[0] != 0;
                const float* firstOffset = mirrored ? offsets + 1 : offsets + 5;
                const float* secondOffset = mirrored ? offsets + 5 : offsets + 1;

                if (packet[4] + 2 < frameNow || frameNow < packet[4] - 2)
                {
                    if (walking)
                    {
                        unknown8D8 = 0;
                    }
                }
                else if (unknown8D8 == 0)
                {
                    unknown8D8 = 1;
                    const float stepFacing = frameFacing(frame);
                    const auto snapped = static_cast<int32_t>(std::floor(static_cast<double>(stepFacing * 0.025f)));
                    const float angle = static_cast<float>(snapped) * 40.0f;
                    makeFootprint(this, firstOffset[0], firstOffset[1], angle, footprintDirection(angle));
                }

                if (packet[0] + 2 < frameNow || frameNow < packet[0] - 2)
                {
                    if (walking)
                    {
                        unknown8DC = 0;
                    }
                }
                else if (unknown8DC == 0)
                {
                    unknown8DC = 1;
                    const float stepFacing = frameFacing(frame);
                    const int32_t direction = footprintDirection(stepFacing);
                    const auto snapped = static_cast<int32_t>(std::floor(static_cast<double>(stepFacing * 0.025f)));
                    makeFootprint(this, secondOffset[0], secondOffset[1], static_cast<float>(snapped) * 40.0f,
                                  direction);
                }
            }
        }

        if (jumpFX[0] != nullptr)
        {
            jumpFX[0]->update();
        }

        if (jumpFX[1] != nullptr)
        {
            jumpFX[1]->update();
        }
    }

    for (int32_t i = 0; i < 4; i++)
    {
        if (smoke[i] == nullptr)
        {
            continue;
        }

        smokeTime[i] -= frameLength;

        if (0.0 <= smokeTime[i])
        {
            smoke[i]->setOwner(this);
            smoke[i]->setOwnerPosition(getPositionFromHS(static_cast<uint32_t>(smokeHotSpot[i])));
            smoke[i]->ownerHotSpot = static_cast<uint32_t>(smokeHotSpot[i]);
            smoke[i]->setOwnerVelocity(velocity);
            smoke[i]->unknownB0 = -50;
            smoke[i]->update();
        }
        else
        {
            delete smoke[i];
            smoke[i] = nullptr;
        }
    }

    // Original behaviour (OB-005): adds the map's top edge to y here rather than subtracting.
    const float blockSize = static_cast<float>(Terrain::verticesBlockSide) * Terrain::metersPerVertex;
    const float blockColumn = (position.x - Terrain::mapTopLeft3d100.x) / blockSize;
    const auto blockRow =
        static_cast<int32_t>(std::floor(static_cast<double>((Terrain::mapTopLeft3d100.y + position.y) / blockSize)));
    const auto column = static_cast<int32_t>(std::floor(static_cast<double>(blockColumn)));
    addMoverToList(column + blockRow * Terrain::blocksMapSide);
    return 1;
}

namespace
{
    /// <summary>A world point on <see cref="eye"/>'s screen (the camera's inline projection).</summary>
    vector_2d eyeProject(const vector_3d& point)
    {
        const float scale = eye->cameraScale != 1 ? 1.0f : 0.5f;
        const float dy = point.y - eye->position.y;
        const float dz = point.z - eye->position.z;
        const float sx = (point.x - eye->position.x) * scale;
        const float sy = dy * scale;
        vector_2d screen;
        screen.x = sx * eye->cosAngle + sy * eye->cosAngle + eye->halfWidth;
        screen.y = ((sx * eye->sinAngle + eye->halfHeight) - sy * eye->sinAngle) - scale * dz;
        return screen;
    }
}

auto BattleMech::render() -> void
{
    if (gamePaused != 0)
    {
        onScreen();
    }

    if (unknown79C != 0 && pilot->status == 2)
    {
        return;
    }

    auto* actor = static_cast<MechActor*>(appearance);
    int tagged = 0;
    int drawMech = 0;

    if (alignment == homeTeam->alignment)
    {
        if (windowsVisible == turn)
        {
            if (getAwake() == 0)
            {
                if (isRevealed() != 0)
                {
                    actor->render(0);
                    drawMech = 1;
                }
            }
            else
            {
                actor->render(inJump != 0 ? -150 : 0);
                drawMech = 1;
            }
        }
    }
    else
    {
        const int32_t contactType = getContactType(homeTeam->id, tagged);

        if (contactType == 1)
        {
            if (windowsVisible == turn)
            {
                actor->render(inJump != 0 ? -150 : 0);
                drawMech = 1;
            }
        }
        else if (contactType == 2)
        {
            // A sensor contact: a blip sized by tonnage, at the zoom's scale.
            const int zoomedOut = eye->cameraScale == 1;
            int32_t shapeIndex;
            const char* shapeName;

            if (50.0f < getTonnage())
            {
                shapeIndex = zoomedOut ? 1 : 0;
                shapeName = zoomedOut ? "mblip1" : "mblip2";
            }
            else if (35.0f < getTonnage())
            {
                shapeIndex = zoomedOut ? 3 : 2;
                shapeName = zoomedOut ? "mblip3" : "mblip4";
            }
            else
            {
                shapeIndex = zoomedOut ? 5 : 4;
                shapeName = zoomedOut ? "mblip5" : "mblip6";
            }

            uint8_t* shape = scenario->sensorContactShapes[shapeIndex];

            if (shape != nullptr)
            {
                if (VFX_shape_count(shape) <= blipFrame)
                {
                    if (soundSystem != nullptr && useSound != 0)
                    {
                        soundSystem->playDigitalSample(0x14, 1, this, 0, 1);
                    }

                    blipFrame = 0;
                }

                ElementList->openGroup(-100000, 1);
                auto* element = new VFXElement(shape, screenPos.x, screenPos.y, blipFrame, 0, nullptr, 0, 1);
                std::strcpy(element->name, shapeName);
                ElementList->add(element);
                blipTime = frameLength + blipTime;

                if (0.067 < blipTime)
                {
                    blipFrame = static_cast<int32_t>(blipTime * (1.0 / 0.067) + blipFrame + 0.5);
                    blipTime = 0.0f;
                }
            }
        }
    }

    if (drawMech != 0)
    {
        for (int32_t i = 0; i < 4; i++)
        {
            if (smoke[i] != nullptr)
            {
                smoke[i]->render();
            }
        }

        if (jumpFX[0] != nullptr)
        {
            jumpFX[0]->render();
        }

        if (jumpFX[1] != nullptr)
        {
            jumpFX[1]->render();
        }
    }

    if (drawTerrainGrid != 0)
    {
        // Debug: the move path's steps as lines.
        MovePath* path = pilot->getMovePath();
        Assert(path != nullptr, 0, " NULL move path--bad thing ", nullptr);
        const int32_t numSteps = path->numSteps;

        for (int32_t i = 0; i < numSteps; i++)
        {
            if (i == numSteps - 1)
            {
                continue;
            }

            vector_3d from = path->stepList[i].destination;
            vector_3d to = path->stepList[i + 1].destination;
            from.z = land->getTerrainElevation(from);
            to.z = land->getTerrainElevation(to);
            vector_2d fromScreen = eyeProject(from);
            vector_2d toScreen = eyeProject(to);
            ElementList->openGroup(-100000, 1);
            ElementList->add(new LineElement(fromScreen, toScreen, 0xfc, nullptr, -100000, -1));
        }
    }

    // The selected mech's queued orders: waypoint markers, joined by lines when the queue is drawn as a path.
    if (getCommanderId() == HomeCommander->getId() && waypointMarkers != nullptr && selected != 0 && pilot != nullptr &&
        pilot->getTacOrderQueue(nullptr) > 0)
    {
        TacticalOrder tacOrder;
        tacOrder.init();
        _QueuedTacOrder queue[MAX_QUEUED_TACORDERS_PER_WARRIOR];
        const int32_t numOrders = pilot->getTacOrderQueue(queue);
        vector_2d fromScreen = eyeProject(position);
        const int32_t drawLines = unknown89C;

        for (int32_t i = 0; i < numOrders; i++)
        {
            vector_2d toScreen = eyeProject(queue[i].point);
            tacOrder.data[0] = queue[i].packedData[0];
            tacOrder.data[1] = queue[i].packedData[1];
            tacOrder.unpack();
            int32_t marker;

            if (tacOrder.code == TACTICAL_ORDER_JUMPTO_POINT || tacOrder.code == TACTICAL_ORDER_JUMPTO_OBJECT)
            {
                marker = 4;
            }
            else
            {
                marker = tacOrder.moveParams.wayPath.mode[0] << 1;
            }

            if (drawLines != 0)
            {
                ElementList->openGroup(-99999, 1);
                ElementList->add(new LineElement(fromScreen, toScreen, 0xeb, nullptr, -100000, -1));
                fromScreen = toScreen;
                marker++;
            }

            const int32_t bounds = VFX_shape_bounds(waypointMarkers, marker);
            ElementList->openGroup(-100000, 1);
            auto* element =
                new VFXElement(waypointMarkers, static_cast<float>((bounds >> 16) / 2) + toScreen.x,
                               toScreen.y - static_cast<float>(bounds >> 1 & 0x7fff), marker, 1, nullptr, 1, 0);
            std::strcpy(element->name, "mwp");
            ElementList->add(element);
        }

        tacOrder.destroy();
    }
}

auto BattleMech::relFacingTo(vector_3d goal, int32_t bodyPart) -> float
{
    float facing = Mover::relFacingTo(goal, -1);

    switch (bodyPart)
    {
        case 0:
        case 1:
        case 2:
        case 3:
        case 8:
        case 9:
        case 10:
            facing += torsoRotation;
            break;
        case 4:
            facing = leftArmRotation + torsoRotation + facing;
            break;
        case 5:
            facing = rightArmRotation + torsoRotation + facing;
            break;
        default:
            break;
    }

    if (facing < -180.0)
    {
        return static_cast<float>(facing + 360.0);
    }

    if (180.0f < facing)
    {
        facing = static_cast<float>(facing - 360.0);
    }

    return facing;
}

auto BattleMech::getBodyState() -> int32_t
{
    return MechStateByGesture[static_cast<MechActor*>(appearance)->currentGesture];
}

auto BattleMech::isWeaponReady(int32_t weaponIndex) -> int
{
    if (inventory[weaponIndex].disabled != 0)
    {
        return 0;
    }

    if (scenarioTime < inventory[weaponIndex].readyTime)
    {
        return 0;
    }

    return 1;
}

auto BattleMech::calcAttackChance(GameObject* target, int32_t aimLocation, float targetTime, int32_t weaponIndex,
                                  float modifiers, int32_t* range, vector_3d* targetPoint) -> float
{
    if (weaponIndex < numOther || numOther + numWeapons <= weaponIndex)
    {
        return -1000.0f;
    }

    if (pilot != nullptr)
    {
        modifiers += RankVersusChassisCombatModifier[static_cast<int8_t>(pilot->rank)][static_cast<int8_t>(mechClass)];
    }

    return Mover::calcAttackChance(target, aimLocation, targetTime, weaponIndex, modifiers, range, targetPoint);
}

auto BattleMech::calcHitLocation(GameObject* attacker, int32_t weaponIndex, int32_t attackSource, int32_t attackType)
    -> int32_t
{
    int32_t row = MechHitSectionTable[attackSource];
    double angle;

    if (attacker == nullptr)
    {
        angle = static_cast<double>(RandomNumber(360) - 180);
    }
    else
    {
        angle = relFacingTo(attacker->getPosition(), -1);
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
        if (static_cast<MechActor*>(getAppearance())->currentStateGesture == 7)
        {
            side = 0;
            row = 1;
        }

        if (static_cast<MechActor*>(getAppearance())->currentStateGesture == 8)
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

auto BattleMech::transferHitLocation(int32_t hitLocation) -> int32_t
{
    Assert(hitLocation >= 0 && hitLocation < NUM_MECH_BODY_LOCATIONS, 0,
           "(hitLocation >= 0) && (hitLocation < NUM_MECH_BODY_LOCATIONS)", "L:\\mcx\\Object\\Mech.cpp");
    return MechTransferHitTable[hitLocation];
}

auto BattleMech::startJump(vector_3d jumpGoal) -> int32_t
{
    this->jumpGoal.x = jumpGoal.x;
    this->jumpGoal.z = jumpGoal.z;
    inJump = 1;
    this->jumpGoal.y = jumpGoal.y;
    return 0;
}

auto BattleMech::isJumping(vector_3d* jumpGoal) -> int
{
    if (jumpGoal != nullptr)
    {
        *jumpGoal = this->jumpGoal;
    }

    return inJump;
}

auto BattleMech::getJumpRange(int32_t* numOffsets, int32_t* jumpCost) -> float
{
    if (numOffsets != nullptr)
    {
        *numOffsets = MechJumpOffsets[numJumpJets < 7 ? numJumpJets : 6];
    }

    if (jumpCost != nullptr)
    {
        *jumpCost = numJumpJets != 0 ? DefaultMechJumpCost : 0;
    }

    return static_cast<float>(numJumpJets) * Terrain::metersPerVertex * static_cast<float>(2.0 / 3.0);
}

auto BattleMech::handleEjection() -> int
{
    if (pilot == nullptr || (pilot->status != 0 && pilot->status != 1))
    {
        return 1;
    }

    getPilot()->eject();
    unknown790 = 1;
    destroyBodyLocation(MECH_BODY_LOCATION_HEAD);
    // The ejection seat's beam, from the cockpit hot spot up and away.
    GameObject* beam = createObject(0x1e4);

    if (beam != nullptr)
    {
        auto* mechType = static_cast<BattleMechType*>(objType);
        vector_3d cockpit = getPositionFromHS(mechType->numWeapons + 1);
        beam->setPosition(cockpit);
        cockpit.x = static_cast<float>(cockpit.x - 1000.0);
        cockpit.y = static_cast<float>(cockpit.y + 1000.0);
        cockpit.z = static_cast<float>(cockpit.z + 300.0);
        static_cast<ProjectileLaser*>(beam)->connect(this, cockpit, nullptr,
                                                     static_cast<int32_t>(mechType->numWeapons + 1));

        if (objectList->head != nullptr)
        {
            objectList->head->addNode(beam);
        }
    }

    disable(3);
    theInterface->RemoveMech(partId);

    if (alignment == homeTeam->alignment)
    {
        friendlyDestroyed = 1;
        return 1;
    }

    enemyDestroyed = 1;
    return 1;
}

auto BattleMech::hitInventoryItem(int32_t itemIndex, int setupOnly) -> int
{
    static const char* const locationNames[NUM_MECH_BODY_LOCATIONS] = {"HEAD", "CTORSO", "LTORSO", "RTORSO",
                                                                       "LARM", "RARM",   "LLEG",   "RLEG"};
    InventoryItem& item = inventory[itemIndex];
    item.health--;
    const uint32_t masterId = item.masterID;
    const uint32_t location = item.bodyLocation;

    if (GameSystemWindow != nullptr && setupOnly == 0 && location <= 7)
    {
        // Reported only while the location's armor (front and rear, for the torso) still stands.
        int report = 0;

        switch (location)
        {
            case 1:
                report = armor[1].curArmor > 0.0f && armor[8].curArmor > 0.0f;
                break;
            case 2:
                report = armor[2].curArmor > 0.0f && armor[9].curArmor > 0.0f;
                break;
            case 3:
                report = armor[3].curArmor > 0.0f && armor[10].curArmor > 0.0f;
                break;
            default:
                report = armor[location].curArmor > 0.0f;
                break;
        }

        if (report != 0)
        {
            char line[200];
            GameSystemWindow->print(const_cast<char*>(""));
            GameSystemWindow->print(const_cast<char*>("***********************************"));
            std::snprintf(line, sizeof(line), "INTERNAL COMPONENT HIT: %s (%s)", debugStatus, pilot->name);
            GameSystemWindow->print(line);
            const char* attackerName = BadGuy != nullptr ? static_cast<Mover*>(BadGuy)->debugStatus : "???";
            std::snprintf(line, sizeof(line), "%s in %s by %s", MasterComponentList[masterId].name,
                          locationNames[location], attackerName);
            GameSystemWindow->print(line);
        }
    }

    const MasterComponent& component = MasterComponentList[masterId];

    if (component.form == 3 || component.form == 0xe)
    {
        pilotingCheck(0, 0.0f);
    }

    const auto disableLevel = static_cast<int8_t>(component.disableLevel);

    if (getInventoryDamage(itemIndex) == disableLevel)
    {
        // Disabled: the component stops working; smoke from the hot spot above the weapons it sits nearest.
        int32_t smokeSpot = 1;
        item.disabled = 1;

        switch (component.form)
        {
            case 0:
            case 1:
                smokeSpot = 2;
                break;
            case 2:
            {
                if (sensorSystem != nullptr)
                {
                    sensorSystem->disable();
                }
                break;
            }
            case 4:
            {
                smokeSpot = 1;
                unknown170 = static_cast<float>(scenarioTime + 5.0);
                disable(1);
                break;
            }
            case 6:
            case 7:
            case 8:
            case 9:
            {
                calcWeaponEffectiveness(0);

                if (longestRangeWeapon == static_cast<uint32_t>(itemIndex) ||
                    shortestRangeWeapon == static_cast<uint32_t>(itemIndex))
                {
                    calcLongestRangeWeapon();
                }

                calcOptimalRange(nullptr);
                [[fallthrough]];
            }
            case 3:
                smokeSpot = 0;
                break;
            case 0xc:
                bodyAt(item.bodyLocation).hasCASE = 0;
                break;
            case 0xf:
            case 0x13:
                smokeSpot = 1;
                break;
            case 0x10:
            {
                team->removeECM(ecmTracker);
                ecmTracker = nullptr;
                break;
            }
            case 0x12:
            {
                team->removeJammer(jammerTracker);
                jammerTracker = nullptr;
                break;
            }
            default:
                break;
        }

        if (setupOnly == 0)
        {
            if (useSound != 0)
            {
                soundSystem->playDigitalSample(0x13, 1, this, 0, 0);
            }

            GameObject* sparks = createObject(0x3f);

            if (sparks != nullptr)
            {
                vector_3d sparkPos = getPositionFromHS(static_cast<BattleMechType*>(objType)->numWeapons + smokeSpot);
                sparks->setPosition(sparkPos);

                if (objectList->head != nullptr)
                {
                    objectList->head->addNode(sparks);
                }
            }

            for (int32_t i = 0; i < 4; i++)
            {
                if (smoke[i] == nullptr)
                {
                    smoke[i] = static_cast<Smoke*>(createObject(0x1c2));
                    smokeHotSpot[i] =
                        RandomNumber(static_cast<int32_t>(static_cast<BattleMechType*>(objType)->numWeapons));
                    smokeTime[i] = 15.0f;
                    break;
                }
            }
        }
    }

    if (inventory[itemIndex].health == 0)
    {
        // Destroyed: the cockpit hurts the pilot, a leg actuator trips the mech, ammunition explodes.
        switch (component.form)
        {
            case 1:
                pilot->injure(6.0f, 0);
                break;
            case 3:
            {
                if (location == MECH_BODY_LOCATION_LLEG || location == MECH_BODY_LOCATION_RLEG)
                {
                    pilotingCheck(0, 100.0f);
                    return 0;
                }
                break;
            }
            case 10:
            {
                ammoExplosion(itemIndex);
                return 0;
            }
            default:
                break;
        }
    }

    return 0;
}

auto BattleMech::destroyBodyLocation(int32_t location) -> void
{
    BodyLocation& bodyLocation = bodyAt(location);

    if (bodyLocation.damageState == 2)
    {
        return;
    }

    bodyLocation.curInternalStructure = 0.0f;
    bodyLocation.damageState = 2;

    if (location == MECH_BODY_LOCATION_LLEG || location == MECH_BODY_LOCATION_RLEG)
    {
        calcLegStatus();
        pilotingCheck(0, 100.0f);
    }
    else if (location == MECH_BODY_LOCATION_CTORSO)
    {
        calcTorsoStatus();
    }

    // Everything in it is lost.
    for (int32_t i = 0; i < NumLocationCriticalSpaces[location]; i++)
    {
        CriticalSpace& space = bodyAt(location).criticalSpaces[i];

        if (space.hit == 0 && static_cast<int8_t>(space.inventoryID) != -1)
        {
            space.hit = 1;
            hitInventoryItem(static_cast<int8_t>(space.inventoryID), 0);
        }
    }

    switch (location)
    {
        case MECH_BODY_LOCATION_CTORSO:
        {
            // The center torso gone destroys the mech, unless it was already ruled dead.
            if (unknown8C4 < scenarioTime && (unknown170 <= -1.0f || unknown170 < scenarioTime))
            {
                disable(0);
                return;
            }

            pilot->handleAlarm(6, 0);
            objType->handleDestruction(this, nullptr);
            return;
        }
        case MECH_BODY_LOCATION_RTORSO:
        {
            destroyBodyLocation(MECH_BODY_LOCATION_RARM);
            return;
        }
        case MECH_BODY_LOCATION_LTORSO:
        {
            destroyBodyLocation(MECH_BODY_LOCATION_LARM);
            return;
        }
        case MECH_BODY_LOCATION_LARM:
        {
            pendingControl8D0 = 1;
            return;
        }
        case MECH_BODY_LOCATION_RARM:
        {
            pendingControl8D4 = 1;
            return;
        }
        default:
            return;
    }
}

auto BattleMech::calcCriticalHit(int32_t hitLocation) -> void
{
    if (MPlayer != nullptr && MPlayer->isServer == 0)
    {
        return;
    }

    const int32_t location = MechArmorToBodyLocation[hitLocation];

    // Port fix: the original tests body[hitLocation].totalSpaces, reading past the eight body locations for a rear
    // torso hit (8..10); the body location the hit maps to is tested instead.
    if (bodyAt(location).totalSpaces == 0)
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
            destroyBodyLocation(location);

            if (MPlayer != nullptr)
            {
                addCriticalHitChunk(0, location, 15);
            }

            return;
        }

        numCriticalHits = 3;
    }

    do
    {
        BodyLocation& bodyLocation = bodyAt(location);
        int32_t spaceRoll = RandomNumber(bodyLocation.totalSpaces);
        int32_t space = 0;

        for (; space < numSpaces; space++)
        {
            const uint8_t item = bodyLocation.criticalSpaces[space].inventoryID;
            int32_t size = 0;

            if (item != 0xff)
            {
                size = static_cast<int8_t>(MasterComponentList[inventory[item].masterID].criticalSpacesReq);
            }

            if (spaceRoll < size)
            {
                break;
            }

            spaceRoll -= size;
        }

        Assert(location >= 0 && location <= 7, static_cast<uint32_t>(location), " Bad bodyLocation in CriticalHit ",
               nullptr);
        Assert(space >= 0 && space < NumLocationCriticalSpaces[location], static_cast<uint32_t>(space),
               " Bad Critical Hit Space ", nullptr);
        CriticalSpace& criticalSpace = bodyLocation.criticalSpaces[space];
        criticalSpace.hit = 1;
        hitInventoryItem(static_cast<int8_t>(criticalSpace.inventoryID), 0);

        if (MPlayer != nullptr)
        {
            addCriticalHitChunk(0, location, space);
        }
    } while (--numCriticalHits != 0);
}

auto BattleMech::handleCriticalHit(int32_t bodyLocation, int32_t criticalSpace) -> void
{
    if (criticalSpace == 15)
    {
        destroyBodyLocation(bodyLocation);
        return;
    }

    hitInventoryItem(static_cast<int8_t>(bodyAt(bodyLocation).criticalSpaces[criticalSpace].inventoryID), 0);
}

auto BattleMech::updateCriticalHitChunks(int32_t which) -> int32_t
{
    for (int32_t i = 0; i < numCriticalHitChunks[which]; i++)
    {
        const uint8_t chunk = criticalHitChunks[which][i];
        handleCriticalHit(chunk >> 4, chunk & 0xf);
    }

    numCriticalHitChunks[which] = 0;
    return 0;
}

auto BattleMech::buildStatusChunk() -> int32_t
{
    statusChunk.targetCellRC[0] = -1;
    statusChunk.targetCellRC[1] = -1;
    statusChunk.bodyState = 0;
    statusChunk.targetType = 0;
    statusChunk.targetId = 0;
    statusChunk.targetBlockOrTrainNumber = 0;
    statusChunk.targetVertexOrCarNumber = 0;
    statusChunk.targetItemNumber = 0;
    statusChunk.ejectOrderGiven = 0;
    statusChunk.jumpOrder = 0;
    statusChunk.data = 0;

    // The body state: 1 standing up, 2 standing, 3 and 4 fallen, 0 otherwise.
    const uint32_t bodyState = static_cast<uint32_t>(getBodyState());

    if (static_cast<MechActor*>(appearance)->currentGesture == 1 || bodyState < 9)
    {
        switch (bodyState)
        {
            case 1:
                statusChunk.bodyState = 1;
                break;
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
                statusChunk.bodyState = 0;
                break;
            case 7:
                statusChunk.bodyState = 4;
                break;
            case 8:
                statusChunk.bodyState = 3;
                break;
            default:
                statusChunk.bodyState = 2;
                break;
        }
    }
    else
    {
        statusChunk.bodyState = 0;
    }

    if (pilot != nullptr)
    {
        if (inJump == 0)
        {
            GameObject* target = pilot->getLastTarget();

            if (target != nullptr)
            {
                const int32_t targetClass = target->objectClass;

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
                        statusChunk.targetType = 2;
                        statusChunk.targetId = target->partId;
                        const int32_t terrainPart = target->partId - 0x1000;
                        statusChunk.targetBlockOrTrainNumber = terrainPart / 0xc80;
                        const int32_t inBlock = terrainPart % 0xc80;
                        statusChunk.targetVertexOrCarNumber = inBlock / 8;
                        statusChunk.targetItemNumber = static_cast<uint8_t>(inBlock % 8);
                        break;
                    }

                    case BATTLEMECH:
                    case GROUNDVEHICLE:
                    case ELEMENTAL:
                    {
                        statusChunk.targetType = 1;
                        statusChunk.targetId = static_cast<Mover*>(target)->netRosterIndex;
                        break;
                    }
                    case CAMERADRONE:
                    {
                        statusChunk.targetType = 3;
                        statusChunk.targetId = target->partId;
                        statusChunk.targetBlockOrTrainNumber = 0x80;
                        statusChunk.targetVertexOrCarNumber = target->partId - 0x802c8;
                        break;
                    }
                    case TRAINCAR:
                    {
                        statusChunk.targetType = 3;
                        statusChunk.targetId = target->partId;
                        const int32_t trainPart = target->partId - 0x7d000;
                        statusChunk.targetBlockOrTrainNumber = trainPart / 100;
                        statusChunk.targetVertexOrCarNumber = trainPart % 100;
                        break;
                    }

                    default:
                        Fatal(targetClass, " BattleMech.buildStatusChunk: bad target type ", nullptr);
                }
            }
        }
        else
        {
            statusChunk.jumpOrder = 1;
            int32_t cellR = 0;
            int32_t cellC = 0;
            worldCoordToMapCell(jumpGoal, cellR, cellC);
            statusChunk.targetCellRC[0] = static_cast<int16_t>(cellR);
            statusChunk.targetCellRC[1] = static_cast<int16_t>(cellC);
        }
    }

    statusChunk.ejectOrderGiven = unknown790;
    statusChunk.pack(this);

    // Checks the chunk unpacks to what was packed.
    StatusChunk check;
    check.data = statusChunk.data;
    check.StatusChunk::unpack(this);

    if (statusChunk.equalTo(&check) == 0)
    {
        Fatal(0, " BAD status chunk in mech: save stchunk.dbg file! ", nullptr);
    }

    return 0;
}

auto BattleMech::handleStatusChunk(int32_t updateAge, uint32_t chunk) -> int32_t
{
    statusChunk.targetCellRC[0] = -1;
    statusChunk.targetCellRC[1] = -1;
    statusChunk.data = 0;
    statusChunk.bodyState = 0;
    statusChunk.targetType = 0;
    statusChunk.targetId = 0;
    statusChunk.targetBlockOrTrainNumber = 0;
    statusChunk.targetVertexOrCarNumber = 0;
    statusChunk.targetItemNumber = 0;
    statusChunk.ejectOrderGiven = 0;
    statusChunk.jumpOrder = 0;
    statusChunk.data = chunk;
    statusChunk.unpack(this);

    if (StatusChunkUnpackErr != 0)
    {
        return 0;
    }

    int32_t targetPartId = 0;

    if (statusChunk.jumpOrder == 0 && static_cast<int8_t>(statusChunk.targetType) > 0)
    {
        if (statusChunk.targetType == 1)
        {
            targetPartId = MPlayer->moverRoster[statusChunk.targetId]->partId;
        }
        else if (statusChunk.targetType < 4)
        {
            targetPartId = statusChunk.targetId;
        }
    }

    if (pilot == nullptr)
    {
        return 0;
    }

    GameObject* target = nullptr;
    int keepTarget = 0;

    if (targetPartId != 0)
    {
        GameObject* lastTarget = pilot->getLastTarget();

        if (lastTarget != nullptr && lastTarget->partId == targetPartId)
        {
            keepTarget = 1;
        }
        else
        {
            target = static_cast<GameObject*>(objectList->findObjectFromPart(targetPartId));
        }
    }

    if (keepTarget == 0)
    {
        pilot->setLastTarget(target, 0, 0);
    }

    if (unknown790 == 0 && statusChunk.ejectOrderGiven != 0)
    {
        unknown790 = 1;
        handleEjection();
    }

    return 0;
}

auto BattleMech::buildMoveChunk() -> int32_t
{
    moveChunk.init();

    if (pilot != nullptr)
    {
        pilot->getMovePath();
        moveChunk.build(this, pilot->moveOrders.path[0], pilot->moveOrders.path[1]);
    }

    moveChunk.pack(this);

    // Checks the chunk unpacks to what was packed; a chunk that can't is replaced by an empty one.
    MoveChunk check;
    check.stepPos[0][0] = -1;
    check.stepPos[0][1] = -1;
    check.run = 0;
    check.numSteps = 0;
    check.data = moveChunk.data;
    check.unpack(this);

    if (MoveChunkUnpackErr == 0)
    {
        if (moveChunk.equalTo(this, &check) == 0)
        {
            Fatal(0, " Bad mech movechunk: save mvchunk.dbg file! ", nullptr);
        }
    }
    else
    {
        moveChunk.init();
        moveChunk.build(this, nullptr, nullptr);
        moveChunk.pack(this);
    }

    return 0;
}

auto BattleMech::handleMoveChunk(uint32_t chunk) -> int32_t
{
    moveChunk.init();
    moveChunk.data = chunk;
    moveChunk.unpack(this);

    if (MoveChunkUnpackErr == 0)
    {
        MovePath* path = getPilot()->getMovePath();
        path->setMoveChunk(&moveChunk);

        // Skip ahead to the step nearest the mech.
        if (path->numStepsWhenNotPaused > 1)
        {
            int32_t step = path->numStepsWhenNotPaused;

            do
            {
                step--;

                if (step < 1)
                {
                    break;
                }
            } while (MapCellDiagonal < distanceFrom(path->stepList[step].destination));

            path->curStep = step;
        }

        unknown20C = 1;
    }

    return 0;
}

auto BattleMech::injureBodyLocation(int32_t bodyLocation, float damage) -> int
{
    BodyLocation& location = bodyAt(bodyLocation);

    if (bodyLocation == MECH_BODY_LOCATION_CTORSO && unknown8C4 < 0.0)
    {
        unknown8C4 = scenarioTime;
    }

    if (damage <= location.curInternalStructure)
    {
        location.curInternalStructure -= damage;
    }
    else
    {
        location.curInternalStructure = 0.0f;
    }

    if (0.0f < location.curInternalStructure)
    {
        location.damageState =
            0.5 < location.curInternalStructure / static_cast<float>(location.maxInternalStructure) ? 0 : 1;

        if (bodyLocation == MECH_BODY_LOCATION_LLEG || bodyLocation == MECH_BODY_LOCATION_RLEG)
        {
            calcLegStatus();
        }
        else if (bodyLocation == MECH_BODY_LOCATION_CTORSO)
        {
            calcTorsoStatus();
            calcCriticalHit(MECH_BODY_LOCATION_CTORSO);
            return 0;
        }

        calcCriticalHit(bodyLocation);
        return 0;
    }

    destroyBodyLocation(bodyLocation);
    return 1;
}

auto BattleMech::weaponLocked(int32_t weaponIndex, vector_3d targetPosition) -> float
{
    return relFacingTo(targetPosition, inventory[weaponIndex].bodyLocation);
}

namespace
{
    /// <summary>The rear armor location behind a body location (the centre's, the left side's or the right's).</summary>
    int32_t rearArmorLocation(int32_t bodyLocation)
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
    void transferHit(BattleMech* mech, _WeaponShotInfo* shotInfo, int32_t bodyLocation)
    {
        _WeaponShotInfo transferInfo = *shotInfo;
        transferInfo.hitLocation = mech->transferHitLocation(bodyLocation);

        if (MPlayer == nullptr)
        {
            mech->handleWeaponHit(&transferInfo, 0);
        }
        else if (MPlayer->isServer != 0)
        {
            mech->handleWeaponHit(&transferInfo, 1);
        }
    }
}

auto BattleMech::handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if ((MPlayer == nullptr && CantHitMe != 0 && pilot->onHomeTeam() != 0) || shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->addWeaponHitChunk(this, shotInfo, 0);
    }

    BadGuy = shotInfo->attacker;

    if (shotInfo->damage <= 0.0f)
    {
        return 0;
    }

    const int32_t hitLocation = shotInfo->hitLocation;

    if (hitLocation == -1)
    {
        return 0;
    }

    if (isDestroyed() != 0)
    {
        return 0;
    }

    const _WeaponShotInfo originalShot = *shotInfo;
    const int wasDisabled = isDisabled();
    // Ammunition hits (and cause -4) go straight for the internal structure.
    int internalHit = shotInfo->masterId == -4;

    if (shotInfo->masterId > 0)
    {
        internalHit = MasterComponentList[shotInfo->masterId].form == 10;
    }

    damageRateTally = shotInfo->damage + damageRateTally;
    totalDamageTaken = shotInfo->damage + totalDamageTaken;
    // Which way the mech would fall.
    const float angle = torsoRotation + shotInfo->entryAngle;

    if (!(angle < -90.0 || 90.0 < angle))
    {
        unknown8CC = 1;
    }
    else if (angle <= -91.0 || 91.0 <= angle)
    {
        unknown8C8 = 1;
    }

    const int32_t bodyLocation = MechArmorToBodyLocation[hitLocation];

    if (bodyLocation == MECH_BODY_LOCATION_HEAD && 2.0f <= shotInfo->damage)
    {
        pilot->injure(1.0f, 1);
    }

    if (armor[hitLocation].curArmor <= 0.0f || internalHit != 0)
    {
        BodyLocation& location = bodyAt(bodyLocation);
        int caseHit = 0;

        if (location.curInternalStructure <= 0.0f)
        {
            if (internalHit == 0 || location.hasCASE == 0)
            {
                if (bodyLocation != MECH_BODY_LOCATION_CTORSO && bodyLocation != MECH_BODY_LOCATION_HEAD)
                {
                    transferHit(this, shotInfo, bodyLocation);
                }
            }
            else
            {
                caseHit = 1;
            }
        }
        else if (shotInfo->damage < location.curInternalStructure)
        {
            injureBodyLocation(bodyLocation, shotInfo->damage);
        }
        else
        {
            const float internalStructure = location.curInternalStructure;
            shotInfo->setDamage(shotInfo->damage - internalStructure);
            injureBodyLocation(bodyLocation, internalStructure);

            if (0.0f < shotInfo->damage)
            {
                if (internalHit != 0 && bodyAt(bodyLocation).hasCASE != 0)
                {
                    caseHit = 1;
                }
                else if (bodyLocation != MECH_BODY_LOCATION_CTORSO && bodyLocation != MECH_BODY_LOCATION_HEAD)
                {
                    transferHit(this, shotInfo, bodyLocation);
                }
            }
        }

        if (caseHit != 0)
        {
            // CASE vents the rest out the back.
            const int32_t rear = rearArmorLocation(bodyLocation);
            int holed = 0;

            if (shotInfo->damage <= armor[rear].curArmor)
            {
                armor[rear].curArmor -= shotInfo->damage;
            }
            else
            {
                armor[rear].curArmor = 0.0f;
                holed = 1;
            }

            shotInfo->setDamage(0.0f);

            if (holed != 0)
            {
                playMessage(RADIO_ARMOR_HOLED, 0);
            }
        }
    }
    else if (shotInfo->damage <= armor[hitLocation].curArmor)
    {
        armor[hitLocation].curArmor -= shotInfo->damage;
    }
    else
    {
        // Through the armor.
        shotInfo->setDamage(shotInfo->damage - armor[hitLocation].curArmor);
        armor[shotInfo->hitLocation].curArmor = 0.0f;
        const float internalStructure = bodyAt(bodyLocation).curInternalStructure;

        if (shotInfo->damage < internalStructure)
        {
            injureBodyLocation(bodyLocation, shotInfo->damage);
        }
        else
        {
            shotInfo->setDamage(shotInfo->damage - internalStructure);
            injureBodyLocation(bodyLocation, internalStructure);

            if (0.0f < shotInfo->damage && bodyLocation != MECH_BODY_LOCATION_CTORSO &&
                bodyLocation != MECH_BODY_LOCATION_HEAD)
            {
                transferHit(this, shotInfo, bodyLocation);
            }
        }

        playMessage(RADIO_ARMOR_HOLED, 0);
    }

    GameObject* attacker = shotInfo->attacker;
    auto triggerId = static_cast<uint32_t>(shotInfo->masterId);
    int32_t alarm = 1;

    if (attacker == nullptr)
    {
        if (shotInfo->masterId == -4 || shotInfo->masterId >= 0)
        {
            triggerId = 0;
        }
    }
    else
    {
        triggerId = static_cast<uint32_t>(attacker->partId);

        if (shotInfo->masterId < 0 && shotInfo->masterId != -4)
        {
            alarm = 10;
        }
    }

    pilot->triggerAlarm(alarm, triggerId);
    curCV = calcCV(0);

    if (wasDisabled == 0 && isDisabled() != 0 && attacker != nullptr &&
        (attacker->objectClass == BATTLEMECH || attacker->objectClass == GROUNDVEHICLE ||
         attacker->objectClass == ELEMENTAL || attacker->objectClass == MOVER))
    {
        attacker->getPilot()->triggerAlarm(12, static_cast<uint32_t>(partId));
    }

    shotInfo->init(originalShot.attacker, originalShot.masterId, originalShot.damage, originalShot.hitLocation,
                   originalShot.entryAngle);
    return 0;
}

namespace
{
    /// <summary>Ammo count that marks a weapon as never running out.</summary>
    constexpr int32_t UNLIMITED_SHOTS = 10000;

    /// <summary>Packs a weapon fire chunk, checks that it unpacks the same, queues it and logs it.</summary>
    void sendWeaponFireChunk(BattleMech* mech, WeaponFireChunk& chunk, GameObject* target)
    {
        chunk.pack();
        WeaponFireChunk check;
        check.init();
        check.data = chunk.data;
        check.unpack(mech);

        if (chunk.equalTo(&check) == 0)
        {
            Fatal(0, " Mech.fireWeapon: Bad WeaponFireChunk (save wfchunk.dbg file now) ", nullptr);
        }

        mech->addWeaponFireChunk(0, &chunk);
        LogWeaponFireChunk(&chunk, mech, target);
    }

    /// <summary>
    /// Builds and sends the chunk for a shot at <paramref name="target"/> (a mover, train car, camera drone or
    /// terrain object) or, when it is null, at <paramref name="point"/>.
    /// </summary>
    void sendTargetFireChunk(BattleMech* mech, GameObject* target, vector_3d* point, int32_t weapon, int hit,
                             float entryAngle, int32_t missiles, int32_t missilesPastAMS, int32_t antiMissileShots,
                             int32_t hitLocation)
    {
        WeaponFireChunk chunk;
        chunk.init();
        auto* bigTarget = static_cast<BigGameObject*>(target);

        if (target == nullptr)
        {
            chunk.buildLocationTarget(*point, weapon, hit, missiles);
        }
        else if (target->objectClass == BATTLEMECH || target->objectClass == GROUNDVEHICLE ||
                 target->objectClass == ELEMENTAL || target->objectClass == MOVER)
        {
            chunk.buildMoverTarget(bigTarget, weapon, hit, entryAngle, missiles, missilesPastAMS, antiMissileShots,
                                   hitLocation);
        }
        else if (target->objectClass == TRAINCAR)
        {
            chunk.buildTrainTarget(bigTarget, weapon, hit, entryAngle, missiles);
        }
        else if (target->objectClass == CAMERADRONE)
        {
            chunk.buildCameraDroneTarget(bigTarget, weapon, hit, entryAngle, missiles);
        }
        else
        {
            chunk.buildTerrainTarget(bigTarget, weapon, hit, missiles);
        }

        sendWeaponFireChunk(mech, chunk, target);
    }

    /// <summary>A shot's damage must survive the chunk's quarter-point rounding.</summary>
    void checkDamageRound(const _WeaponShotInfo& shot)
    {
        const auto quarters = static_cast<int32_t>(shot.damage * 4.0);
        Assert(shot.damage == quarters * 0.25 ? 1 : 0, 0, " WeaponHitChunk.build: damage round error ", nullptr);
    }

    /// <summary>A shot with no effect object sets off a live mine where it lands.</summary>
    void checkMineAt(vector_3d& point)
    {
        int32_t tileR = 0;
        int32_t tileC = 0;
        int32_t cellR = 0;
        int32_t cellC = 0;
        GameMap->worldToMapPos(point, tileR, tileC, cellR, cellC);

        // Port fix: a miss can land off the map, where the original reads (and writes) outside it.
        if (!GameMap->onMap(tileR, tileC))
        {
            return;
        }

        MapTile& tile = GameMap->map[GameMap->width * tileR + tileC];

        if ((tile.overlay & 0x1800) == 0x1000 || (tile.overlay & 0x6000) == 0x4000)
        {
            CreateExplosion(MineExplosion, point, MineSplashDamage, worldUnitsPerMeter * MineSplashRange);
            tile.overlay |= 0x1800;
            tile.overlay |= 0x6000;
        }
    }

    /// <summary>
    /// Sends a weapon effect on its way, at <paramref name="target"/> (from hot spot to hot spot) or, when it is
    /// null, at <paramref name="point"/>, carrying <paramref name="shot"/>; then adds it to the weapon list.
    /// </summary>
    void launchWeaponFX(BattleMech* mech, GameObject* fx, GameObject* target, vector_3d* point, _WeaponShotInfo& shot,
                        int32_t sourceHotSpot, int32_t targetHotSpot)
    {
        if (fx->objectClass == BULLET)
        {
            auto* bullet = static_cast<Bullet*>(fx);

            if (bullet->numShots != 5)
            {
                bullet->shotInfo[bullet->numShots++].init(shot.attacker, shot.masterId, shot.damage, shot.hitLocation,
                                                          shot.entryAngle);
            }

            if (target == nullptr)
            {
                bullet->connect(mech, *point, sourceHotSpot);
            }
            else
            {
                bullet->owner = mech;
                bullet->target = target;
                bullet->ownerHotSpot = sourceHotSpot;
                bullet->targetHotSpot = targetHotSpot;
            }
        }
        else if (fx->objectClass == LASER)
        {
            auto* laser = static_cast<Laser*>(fx);

            if (target == nullptr)
            {
                laser->connect(mech, *point, &shot, sourceHotSpot);
            }
            else
            {
                laser->source.setWatcher(mech);
                laser->target.setWatcher(target);
                laser->sourceHotSpot = sourceHotSpot;
                laser->targetHotSpot = targetHotSpot;
                laser->shotInfo.init(shot.attacker, shot.masterId, shot.damage, shot.hitLocation, shot.entryAngle);
            }
        }
        else
        {
            auto* projectile = static_cast<ProjectileLaser*>(fx);

            if (target == nullptr)
            {
                projectile->connect(mech, *point, &shot, sourceHotSpot);
            }
            else
            {
                projectile->owner = mech;
                projectile->target = target;
                projectile->ownerHotSpot = sourceHotSpot;
                projectile->targetHotSpot = targetHotSpot;
                projectile->shotInfo.init(shot.attacker, shot.masterId, shot.damage, shot.hitLocation, shot.entryAngle);
            }
        }

        weaponList->addNode(fx);
    }

    /// <summary>Firing gives a mech away to the other side's mechs within visual range.</summary>
    void revealFiring(BattleMech* mech)
    {
        ObjectQueueNode* enemies = nullptr;
        uint8_t seenBy = 0;

        if (mech->alignment == 1)
        {
            enemies = clanMechList;
            seenBy = 2;
        }
        else if (mech->alignment == -1)
        {
            enemies = innerSphereMechList;
            seenBy = 1;
        }

        if (enemies == nullptr)
        {
            return;
        }

        for (BaseObject* enemy = enemies->head; enemy != nullptr; enemy = enemy->next)
        {
            vector_3d enemyPosition = static_cast<GameObject*>(enemy)->getPosition();

            if (mech->distanceFrom(enemyPosition) < scenario->maxVisualRange)
            {
                land->markRadiusSeen(mech->position, mech->frame.j, 360.0f, scenario->fireVisualRange, seenBy);
                return;
            }
        }
    }

    /// <summary>Where a missed shot lands: scattered up to <paramref name="scatter"/> about the aim point.</summary>
    /// <param name="centred">Missiles scatter both ways; other shots (as the original computes them) only one.</param>
    vector_3d missPoint(GameObject* target, vector_3d* targetPoint, float scatter, int centred)
    {
        vector_3d miss;
        miss.x = scatter;
        miss.y = scatter;
        miss.z = 0.0f;
        const auto offsetX = static_cast<float>(RandomNumber(static_cast<int32_t>(miss.x + miss.x)) - miss.x);
        const auto offsetY = static_cast<float>(RandomNumber(static_cast<int32_t>(miss.y + miss.y)) - miss.y);
        const auto offsetZ = static_cast<float>(RandomNumber(static_cast<int32_t>(miss.z + miss.z)) - miss.z);

        if (centred != 0)
        {
            miss.x = offsetX;
            miss.y = offsetY;
        }
        else
        {
            miss.x = miss.x + offsetX;
            miss.y = miss.y + offsetY;
        }

        miss.z = miss.z + offsetZ;
        const vector_3d base = target != nullptr ? target->getPosition() : *targetPoint;
        miss.x += base.x;
        miss.y += base.y;
        miss.z += base.z;
        return miss;
    }
}

auto BattleMech::fireWeapon(GameObject* target, float targetTime, int32_t weaponIndex, int32_t attackType,
                            int32_t aimLocation, vector_3d* targetPoint) -> int32_t
{
    if (status == 5 || status == 4 || status == 1 || status == 2 || getBodyState() == 0 || getBodyState() == 7 ||
        getBodyState() == 8)
    {
        return 1;
    }

    if (isWeaponReady(weaponIndex) == 0)
    {
        return 3;
    }

    float distance;

    if (target == nullptr)
    {
        if (targetPoint == nullptr || lineOfSight(*targetPoint) == 0)
        {
            return 4;
        }

        distance = distanceFrom(*targetPoint);
    }
    else
    {
        // A camera drone can't be shot for two seconds after launch.
        if (target->objectClass == CAMERADRONE && scenarioTime < static_cast<CameraDrone*>(target)->launchTime + 2.0)
        {
            return 4;
        }

        if (target->isDestroyed() != 0)
        {
            return 4;
        }

        if (lineOfSight(target) == 0)
        {
            return 4;
        }

        vector_3d targetPosition = target->getPosition();
        distance = distanceFrom(targetPosition);
    }

    const int32_t inRange = weaponInRange(weaponIndex, distance);

    if ((MPlayer == nullptr || MPlayer->isServer != 0) && inRange == 0)
    {
        return 4;
    }

    const MasterComponent& weapon = MasterComponentList[inventory[weaponIndex].masterID];

    if (weapon.missileType != 2 && weapon.missileType != 1 && weapon.missileType != 3)
    {
        // Direct fire needs a clear line.
        if (target == nullptr)
        {
            if (targetPoint == nullptr || lineOfFire(*targetPoint) == 0)
            {
                return 4;
            }
        }
        else if (lineOfFire(target) == 0)
        {
            return 4;
        }
    }

    const int32_t numShots = getWeaponShots(weaponIndex);

    if (numShots == 0)
    {
        return 4;
    }

    float entryAngle = 0.0f;

    if (target != nullptr)
    {
        entryAngle = target->relFacingTo(position, -1);
    }

    const int isStreak = weapon.weaponFlags & 1;
    int32_t range = 0;
    int32_t hitChance =
        static_cast<int32_t>(calcAttackChance(target, aimLocation, targetTime, weaponIndex, 0.0f, &range, targetPoint));
    const int32_t hitRoll = RandomNumber(100);

    if (target != nullptr)
    {
        float points = SkillTry[MWS_GUNNERY];

        if (target->getAlignment() == -1)
        {
            pilot->numSkillUses[MWS_GUNNERY][1]++;
        }
        else
        {
            points = SkillTry[MWS_GUNNERY] * 0.1f;
        }

        pilot->skillPoints[MWS_GUNNERY] = points + pilot->skillPoints[MWS_GUNNERY];
    }

    // Aimed shots only from a standing mech.
    if (aimLocation != -1 && 0.0 < getVelocity().magnitude())
    {
        hitChance = 0;
    }

    int32_t hitLocation = -2;

    if (target != nullptr && hitRoll < hitChance)
    {
        float points = SkillSuccess[MWS_GUNNERY];

        if (target->getAlignment() == -1)
        {
            pilot->numSkillSuccesses[MWS_GUNNERY][1]++;
        }
        else
        {
            points = SkillSuccess[MWS_GUNNERY] * 0.1f;
        }

        pilot->skillPoints[MWS_GUNNERY] = points + pilot->skillPoints[MWS_GUNNERY];

        if (aimLocation != -1)
        {
            hitLocation = aimLocation;
        }
    }

    MechWarrior* targetPilot = nullptr;

    if (target != nullptr && (target->objectClass == BATTLEMECH || target->objectClass == GROUNDVEHICLE ||
                              target->objectClass == ELEMENTAL || target->objectClass == MOVER))
    {
        targetPilot = target->getPilot();
        targetPilot->updateAttackerStatus(static_cast<uint32_t>(partId), scenarioTime);
    }

    startWeaponRecycle(weaponIndex);

    const int32_t chunkWeapon = weaponIndex - numOther;

    if (hitRoll < hitChance)
    {
        if (numShots != UNLIMITED_SHOTS)
        {
            deductWeaponShot(weaponIndex, 1);
        }

        InventoryItem& item = inventory[weaponIndex];
        const MasterComponent& fired = MasterComponentList[item.masterID];

        if (fired.form == 9)
        {
            // Missiles: a streak fires them all, anything else about half; anti-missile systems take some out.
            const int32_t rackSize = fired.numMissiles;
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
                missilesLeft = target->fireAntiMissileSystem(missiles, antiMissileShots);

                if (antiMissileShots > 0)
                {
                    target->reduceAntiMissileAmmo(antiMissileShots);
                }
            }

            int32_t targetHotSpot = 0;
            const uint8_t weaponEffect = fired.weaponEffect;
            const int32_t sourceHotSpot = bodyAt(item.bodyLocation).hotSpotNumber;

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
                        hitLocation = target->calcHitLocation(this, weaponIndex, 0, attackType);
                    }

                    if (target->objectClass == BATTLEMECH)
                    {
                        // Port fix: the original reads body[hitLocation], past the eight body locations for a rear torso hit
                        // (8..10); the torso it maps to is read instead.
                        targetHotSpot = static_cast<BattleMech*>(target)
                                            ->bodyAt(MechArmorToBodyLocation[hitLocation])
                                            .hotSpotNumber;
                    }
                }

                Assert(hitLocation != -2 ? 1 : 0, 0, " Mech.FireWeapon: Bad Hit Location ", nullptr);
                _WeaponShotInfo shot;
                shot.init(this, item.masterID, fired.damage * static_cast<float>(missilesLeft), hitLocation,
                          entryAngle);
                checkDamageRound(shot);

                if (MPlayer != nullptr && MPlayer->isServer != 0)
                {
                    sendTargetFireChunk(this, target, targetPoint, chunkWeapon, 1, entryAngle, missiles, missilesLeft,
                                        antiMissileShots, hitLocation);
                }

                GameObject* fx = createObject(static_cast<int32_t>(weaponFXTable[weaponEffect]));

                if (fx == nullptr)
                {
                    if (targetPoint != nullptr)
                    {
                        checkMineAt(*targetPoint);
                    }
                }
                else
                {
                    launchWeaponFX(this, fx, target, targetPoint, shot, sourceHotSpot, targetHotSpot);

                    if (target == nullptr)
                    {
                        pilot->clearCurTacOrder(1, 0);
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
                hitLocation = target->calcHitLocation(this, weaponIndex, 0, attackType);
            }

            Assert(hitLocation != -2 ? 1 : 0, 1, " Mech.FireWeapon: Bad Hit Location ", nullptr);
            _WeaponShotInfo shot;
            shot.init(this, item.masterID, fired.damage, hitLocation, entryAngle);

            if (MPlayer != nullptr && MPlayer->isServer != 0)
            {
                sendTargetFireChunk(this, target, targetPoint, chunkWeapon, 1, entryAngle, 0, 0, 0, hitLocation);
            }

            GameObject* fx = createObject(static_cast<int32_t>(weaponFXTable[fired.weaponEffect]));

            if (fx == nullptr)
            {
                if (targetPoint != nullptr)
                {
                    checkMineAt(*targetPoint);
                }
            }
            else
            {
                const int32_t sourceHotSpot = bodyAt(item.bodyLocation).hotSpotNumber;
                int32_t targetHotSpot = 0;

                if (target != nullptr && target->objectClass == BATTLEMECH)
                {
                    // Port fix: the original reads body[hitLocation], past the eight body locations for a rear torso hit
                    // (8..10); the torso it maps to is read instead.
                    targetHotSpot =
                        static_cast<BattleMech*>(target)->bodyAt(MechArmorToBodyLocation[hitLocation]).hotSpotNumber;
                }

                launchWeaponFX(this, fx, target, targetPoint, shot, sourceHotSpot, targetHotSpot);
            }

            if (target == nullptr)
            {
                pilot->clearCurTacOrder(1, 0);
            }
        }
    }
    else if (isStreak == 0)
    {
        // A miss (a streak doesn't fire without a lock): the shot lands somewhere near.
        if (numShots != UNLIMITED_SHOTS)
        {
            deductWeaponShot(weaponIndex, 1);
        }

        InventoryItem& item = inventory[weaponIndex];
        const MasterComponent& fired = MasterComponentList[item.masterID];
        const float scatter = target != nullptr ? 25.0f : 5.0f;
        _WeaponShotInfo shot;
        vector_3d landing;
        int launch = 1;

        if (fired.form == 9)
        {
            const int32_t rackSize = fired.numMissiles;
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
                shot.init(this, item.masterID, fired.damage * static_cast<float>(missiles), -1, entryAngle);
                checkDamageRound(shot);
                landing = missPoint(target, targetPoint, scatter, 1);

                if (MPlayer != nullptr && MPlayer->isServer != 0)
                {
                    sendTargetFireChunk(this, nullptr, &landing, chunkWeapon, 0, 0.0f, missiles, 0, 0, 0);
                }
            }
            else
            {
                launch = 0;
            }
        }
        else
        {
            shot.init(this, item.masterID, fired.damage, -1, entryAngle);
            landing = missPoint(target, targetPoint, scatter, 0);

            if (MPlayer != nullptr && MPlayer->isServer != 0)
            {
                sendTargetFireChunk(this, nullptr, &landing, chunkWeapon, 0, 0.0f, 0, 0, 0, 0);
            }
        }

        if (launch != 0)
        {
            GameObject* fx = createObject(static_cast<int32_t>(weaponFXTable[fired.weaponEffect]));

            if (fx != nullptr)
            {
                launchWeaponFX(this, fx, nullptr, &landing, shot, bodyAt(item.bodyLocation).hotSpotNumber, 0);
            }
            else
            {
                checkMineAt(landing);
            }
        }
    }

    if (targetPilot != nullptr)
    {
        targetPilot->triggerAlarm(0, static_cast<uint32_t>(partId));
    }

    revealFiring(this);

    if (group != nullptr)
    {
        group->handleMateFiredWeapon(static_cast<uint32_t>(partId));
    }

    return 0;
}

namespace
{
    /// <summary>Weapon effects in flight beyond which a network shot shows none.</summary>
    constexpr int32_t MAX_NETWORK_WEAPON_FX = 200;

    /// <summary>How many weapon effects are in flight.</summary>
    int32_t countWeaponFX()
    {
        int32_t count = 0;

        for (BaseObject* fx = weaponList->head; fx != nullptr; fx = fx->next)
        {
            count++;
        }

        return count;
    }
}

auto BattleMech::handleWeaponFire(int32_t weaponIndex, GameObject* target, vector_3d* targetPoint, int hit,
                                  float entryAngle, int32_t numMissiles, int32_t missilesPastAMS,
                                  int32_t antiMissileShots, int32_t hitLocation) -> int32_t
{
    const int32_t numShots = getWeaponShots(weaponIndex);
    startWeaponRecycle(weaponIndex);
    InventoryItem& item = inventory[weaponIndex];
    const int isStreak = MasterComponentList[item.masterID].weaponFlags & 1;
    const MasterComponent& fired = MasterComponentList[item.masterID];
    _WeaponShotInfo shot;

    if (hit == 0)
    {
        Assert(target == nullptr ? 1 : 0, 0, " Mech.handleWeaponFire: target should be NULL with network miss! ",
               nullptr);
        Assert(targetPoint != nullptr ? 1 : 0, 0, " Mech.handleWeaponFire: MUST have targetpoint with network miss! ",
               nullptr);

        if (isStreak != 0)
        {
            CurMoverWeaponFireChunk.unpack(this);
            DebugWeaponFireChunk(&CurMoverWeaponFireChunk, nullptr, this);
            Assert(0, 0, " Mech.handleWeaponFire: streaks shouldn't miss! ", nullptr);
        }

        if (numShots != 9999)
        {
            deductWeaponShot(weaponIndex, 1);
        }

        if (fired.form == 9)
        {
            if (numMissiles != 0)
            {
                GameObject* fx = nullptr;

                if (countWeaponFX() < MAX_NETWORK_WEAPON_FX)
                {
                    fx = createObject(static_cast<int32_t>(weaponFXTable[fired.weaponEffect]));
                }

                if (fx != nullptr)
                {
                    const int32_t sourceHotSpot = bodyAt(item.bodyLocation).hotSpotNumber;
                    shot.init(this, item.masterID, fired.damage * static_cast<float>(numMissiles), -1, entryAngle);
                    checkDamageRound(shot);
                    launchWeaponFX(this, fx, nullptr, targetPoint, shot, sourceHotSpot, 0);
                }
                else if (targetPoint != nullptr)
                {
                    checkMineAt(*targetPoint);
                }
            }
        }
        else
        {
            shot.init(this, item.masterID, fired.damage, -1, entryAngle);
            GameObject* fx = nullptr;

            if (countWeaponFX() < MAX_NETWORK_WEAPON_FX)
            {
                fx = createObject(static_cast<int32_t>(weaponFXTable[fired.weaponEffect]));
            }

            if (fx != nullptr)
            {
                launchWeaponFX(this, fx, nullptr, targetPoint, shot, bodyAt(item.bodyLocation).hotSpotNumber, 0);
            }
            else if (targetPoint != nullptr)
            {
                checkMineAt(*targetPoint);
            }
        }
    }
    else
    {
        if (numShots != 9999)
        {
            deductWeaponShot(weaponIndex, 1);
        }

        if (fired.form == 9)
        {
            if (antiMissileShots > 0)
            {
                target->reduceAntiMissileAmmo(antiMissileShots);
            }

            int32_t targetHotSpot = 0;
            const int32_t sourceHotSpot = bodyAt(item.bodyLocation).hotSpotNumber;

            if (missilesPastAMS > 0)
            {
                GameObject* fx = nullptr;

                if (countWeaponFX() < MAX_NETWORK_WEAPON_FX)
                {
                    fx = createObject(static_cast<int32_t>(weaponFXTable[fired.weaponEffect]));
                }

                if (fx != nullptr)
                {
                    Assert(hitLocation != -2 ? 1 : 0, static_cast<uint32_t>(TargetRolo),
                           " Mech.handleWeaponFire: Bad Hit Location ", nullptr);

                    if (target != nullptr && target->objectClass == BATTLEMECH)
                    {
                        // Port fix: the original reads body[hitLocation], past the eight body locations for a rear torso hit
                        // (8..10); the torso it maps to is read instead.
                        targetHotSpot = static_cast<BattleMech*>(target)
                                            ->bodyAt(MechArmorToBodyLocation[hitLocation])
                                            .hotSpotNumber;
                    }

                    shot.init(this, item.masterID, fired.damage * static_cast<float>(missilesPastAMS), hitLocation,
                              entryAngle);
                    checkDamageRound(shot);
                    launchWeaponFX(this, fx, target, targetPoint, shot, sourceHotSpot, targetHotSpot);

                    if (target == nullptr)
                    {
                        pilot->clearCurTacOrder(1, 0);
                    }
                }
                else if (targetPoint != nullptr)
                {
                    checkMineAt(*targetPoint);
                }
            }
        }
        else
        {
            shot.init(this, item.masterID, fired.damage, hitLocation, entryAngle);
            GameObject* fx = nullptr;

            if (countWeaponFX() < MAX_NETWORK_WEAPON_FX)
            {
                fx = createObject(static_cast<int32_t>(weaponFXTable[fired.weaponEffect]));
            }

            if (fx != nullptr)
            {
                const int32_t sourceHotSpot = bodyAt(item.bodyLocation).hotSpotNumber;
                int32_t targetHotSpot = 0;

                if (target != nullptr && target->objectClass == BATTLEMECH)
                {
                    // Port fix: the original reads body[hitLocation], past the eight body locations for a rear torso hit
                    // (8..10); the torso it maps to is read instead.
                    targetHotSpot =
                        static_cast<BattleMech*>(target)->bodyAt(MechArmorToBodyLocation[hitLocation]).hotSpotNumber;
                }

                launchWeaponFX(this, fx, target, targetPoint, shot, sourceHotSpot, targetHotSpot);

                if (target == nullptr)
                {
                    pilot->clearCurTacOrder(1, 0);
                }
            }
            else if (targetPoint != nullptr)
            {
                checkMineAt(*targetPoint);
            }
        }
    }

    if (target != nullptr && (target->objectClass == BATTLEMECH || target->objectClass == GROUNDVEHICLE ||
                              target->objectClass == ELEMENTAL || target->objectClass == MOVER))
    {
        MechWarrior* targetPilot = target->getPilot();
        targetPilot->updateAttackerStatus(static_cast<uint32_t>(partId), scenarioTime);
        targetPilot->triggerAlarm(0, static_cast<uint32_t>(partId));
    }

    revealFiring(this);

    if (group != nullptr)
    {
        group->handleMateFiredWeapon(static_cast<uint32_t>(partId));
    }

    return 0;
}

auto BattleMech::calcMaxSpeed() -> float
{
    auto* actor = static_cast<MechActor*>(appearance);

    if (legStatus == 0)
    {
        return actor->getVelocityOfGesture(7);
    }

    if (legStatus < 2)
    {
        return actor->getVelocityOfGesture(4);
    }

    if (legStatus == 2)
    {
        return actor->getVelocityOfGesture(11);
    }

    return 0.0f;
}

auto BattleMech::calcSlowSpeed() -> float
{
    if (legStatus < 2)
    {
        return static_cast<float>(maxRunSpeed * 0.25);
    }

    if (legStatus == 2)
    {
        return static_cast<float>(maxRunSpeed * 0.2);
    }

    return 0.0f;
}

auto BattleMech::calcModerateSpeed() -> float
{
    if (legStatus < 2)
    {
        return static_cast<float>(maxRunSpeed * 0.4);
    }

    if (legStatus == 2)
    {
        return static_cast<float>(maxRunSpeed * 0.3);
    }

    return 0.0f;
}

auto BattleMech::calcSpriteSpeed(float speed, uint32_t flags, int32_t& state, int32_t& throttle) -> int32_t
{
    auto* actor = static_cast<MechActor*>(appearance);
    state = 3;
    throttle = 100;
    const float walkSpeed = actor->getVelocityOfGesture(4);
    const float runSpeed = actor->getVelocityOfGesture(7);

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
        throttle = static_cast<int32_t>(speed / walkSpeed * 100.0);
        return 0;
    }

    if (speed < runSpeed)
    {
        if ((flags & 1) != 0)
        {
            state = 2;
            throttle = static_cast<int32_t>(speed / walkSpeed * 100.0);
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

auto BattleMech::openStatusWindow(int32_t left, int32_t top, int32_t right, int32_t bottom) -> int32_t
{
    auto* window = new MechStatusWindow;
    statusWindow = window;
    window->init(left, top, right, bottom, this);
    statusWindow->setBackColor(0);
    statusWindow->draw();
    screenWindow->addChild(statusWindow);

    if (pilot != nullptr)
    {
        pilot->openStatusWindow(left + 30, top + 30, right, bottom);
    }

    return 0;
}

MechStatusWindow::~MechStatusWindow()
{
    // The inline ~aTitleWindow.
    aTitleWindow::destroy();
}

auto BattleMech::closeStatusWindow() -> int32_t
{
    if (pilot != nullptr)
    {
        pilot->closeStatusWindow();
    }

    // The window is destroyed, not deleted.
    statusWindow->destroy();
    statusWindow = nullptr;
    return 0;
}

auto BattleMech::getVitalInfo(void* vitalInfo) -> int32_t
{
    const int32_t size = Mover::getVitalInfo(nullptr);

    if (vitalInfo != nullptr)
    {
        Mover::getVitalInfo(vitalInfo);
    }

    return size + 6;
}

auto BattleMech::isCaptureable() -> int
{
    if (captureable != 0 && alignment == homeTeam->alignment && isDestroyed() == 0)
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
    float weaponDamageRate(const MasterComponent& weapon)
    {
        int32_t clusterSize = 1;
        int32_t numClusters = 1;

        if (weapon.form == 9 && (weapon.missileType == 1 || weapon.missileType == 2))
        {
            clusterSize = weapon.missileType == 1 ? ClusterSizeSRM : ClusterSizeLRM;
            numClusters = weapon.numMissiles / 2 / clusterSize;

            if (weapon.numMissiles / 2 % clusterSize != 0)
            {
                numClusters++;
            }
        }

        return static_cast<float>(clusterSize * numClusters) * weapon.damage * 10.0f / weapon.recycleTime;
    }
}

auto BattleMech::calcMaxTargetDamage() -> float
{
    float total = 0.0f;

    for (int32_t i = numOther; i < numOther + numWeapons; i++)
    {
        const float damage = weaponDamageRate(MasterComponentList[inventory[i].masterID]) * 100.0f;

        if (0.0f < damage)
        {
            total = damage + total;
        }
    }

    maxTargetDamage = total;
    return total;
}

auto BattleMech::calcExpectedTargetDamage(GameObject* target) -> float
{
    float total = 0.0f;

    if (getPilot() == nullptr)
    {
        return 0.0f;
    }

    GameObject* aimTarget;
    float targetTime;

    if (target == nullptr)
    {
        aimTarget = getPilot()->getLastTarget();

        if (aimTarget == nullptr)
        {
            return 0.0f;
        }

        targetTime = getPilot()->lastTargetTime;
    }
    else
    {
        targetTime = getPilot()->getLastTarget() == target ? getPilot()->lastTargetTime : 0.0f;
        aimTarget = target;
    }

    vector_3d targetPosition = aimTarget->getPosition();
    const float distance = distanceFrom(targetPosition);

    if (getFireRange(-2) < distance)
    {
        return 0.0f;
    }

    for (int32_t i = numOther; i < numOther + numWeapons; i++)
    {
        if (isWeaponWorking(i) == 0)
        {
            continue;
        }

        const float damageRate = weaponDamageRate(MasterComponentList[inventory[i].masterID]);
        int32_t aimLocation = -1;

        if (pilot != nullptr && pilot->curTacOrder.isCombatOrder() != 0)
        {
            aimLocation = pilot->curTacOrder.attackParams.aimLocation;
        }

        int32_t range = 0;
        const double expected =
            static_cast<double>(calcAttackChance(aimTarget, aimLocation, targetTime, i, 0.0f, &range, nullptr)) *
            damageRate;

        if (0.0 < expected)
        {
            total = static_cast<float>(expected + total);
        }
    }

    maxTargetDamage = total;
    return total;
}

auto BattleMech::isWeaponWorking(int32_t weaponIndex) -> int
{
    if (inventory[weaponIndex].disabled != 0)
    {
        return 0;
    }

    return getWeaponShots(weaponIndex) != 0 ? 1 : 0;
}

auto BattleMech::getTotalEffectiveness() -> float
{
    const float weaponRatio = weaponEffectiveness / maxWeaponEffectiveness;
    float armorFactor = 0.0f;

    if (isDestroyed() == 0 && isDisabled() == 0)
    {
        // Head, arms, centre torso (the worse of front and back) and side torsos (front and back), each as a share
        // of its full armor.
        const ArmorLocation* locations = armor;
        const float head = locations[MECH_BODY_LOCATION_HEAD].curArmor /
                               static_cast<float>(locations[MECH_BODY_LOCATION_HEAD].maxArmor) * 0.6f +
                           0.4f;
        float centre = locations[MECH_BODY_LOCATION_CTORSO].curArmor;
        uint8_t centreMax = locations[MECH_BODY_LOCATION_CTORSO].maxArmor;

        if (locations[8].curArmor < centre)
        {
            centreMax = locations[8].maxArmor;
            centre = locations[8].curArmor;
        }

        const float arms = (locations[MECH_BODY_LOCATION_RARM].curArmor + locations[MECH_BODY_LOCATION_LARM].curArmor) /
                           static_cast<float>(locations[MECH_BODY_LOCATION_RARM].maxArmor +
                                              locations[MECH_BODY_LOCATION_LARM].maxArmor);
        const float armFactor = arms * 0.25f + 0.75f;
        const float sides =
            (locations[10].curArmor + locations[9].curArmor + locations[MECH_BODY_LOCATION_RTORSO].curArmor +
             locations[MECH_BODY_LOCATION_LTORSO].curArmor) /
            static_cast<float>(locations[10].maxArmor + locations[9].maxArmor +
                               locations[MECH_BODY_LOCATION_RTORSO].maxArmor +
                               locations[MECH_BODY_LOCATION_LTORSO].maxArmor);
        armorFactor = armFactor * (arms * 0.4f + 0.6f) * (centre / static_cast<float>(centreMax) + 1.0f) * 0.5f *
                      (sides * 0.25f + 0.75f) * head;
    }

    // Wounds wear the pilot down.
    const float woundFactor[7] = {1.0f, 0.95f, 0.85f, 0.75f, 0.5f, 0.3f, 0.0f};
    auto wounds = static_cast<uint32_t>(static_cast<int32_t>(std::floor(getPilot()->wounds)));

    if (6 < wounds)
    {
        wounds = 6;
    }

    return woundFactor[wounds] * armorFactor * weaponRatio;
}

auto BattleMech::damageLoadedComponents() -> void
{
    for (int32_t location = 0; location < NUM_MECH_BODY_LOCATIONS; location++)
    {
        for (int32_t i = 0; i < NumLocationCriticalSpaces[location]; i++)
        {
            const CriticalSpace& space = bodyAt(location).criticalSpaces[i];

            if (space.hit != 0)
            {
                hitInventoryItem(static_cast<int8_t>(space.inventoryID), 1);
            }
        }
    }
}

auto MechStatusWindow::init(int32_t x, int32_t y, int32_t w, int32_t h, BattleMech* newMech) -> void
{
    aTitleWindow::init(x, y, w, h, nullptr);

    if (titleBar != nullptr)
    {
        titleBar->showCloseButton(1);
    }

    mech = newMech;
}

auto MechStatusWindow::handleEvent(aEvent* event) -> void
{
    if (event->type == 0xd)
    {
        mech->closeStatusWindow();
    }

    aObject::handleEvent(event);
}

auto MechStatusWindow::resize(int32_t w, int32_t h) -> void
{
    aTitleWindow::resize(w, h);
}

namespace
{
    /// <summary>Alignment names, by alignment + 1.</summary>
    const char* const AlignmentNames[3] = {"Clan", "Neutral", "Inner Sphere"};
}

auto MechStatusWindow::display() -> void
{
    static const char* const statusNames[6] = {"Normal",      "Disabled",      "Destroyed",
                                               "Starting Up", "Shutting Down", "Shut Down"};
    static const char* const armorNames[11] = {
        "Head:",     "Center Torso:", "Left Torso:",        "Right Torso:",     "Left Arm:",        "Right Arm:",
        "Left Leg:", "Right Leg:",    "Rear Center Torso:", "Rear Left Torso:", "Rear Right torso:"};
    static const char* const locationNames[8] = {"HEAD", "CTORSO", "LTORSO", "RTORSO", "LARM", "RARM", "LLEG", "RLEG"};
    static const char* const damageNames[3] = {"No Damage", "Partial Damage", "Destroyed"};
    VFX_pane_wipe(displayPort->frame(), backgroundColor);
    BattleMech* shown = mech;

    if (shown != nullptr)
    {
        char line[256];
        std::snprintf(line, sizeof(line), "%s %s (%s)", AlignmentNames[shown->getAlignment() + 1], shown->debugStatus,
                      shown->getPilot()->callsign);
        setTitle(line);
        aPort* port = displayPort;
        systemFont->writeString(port->frame(), 2, 10, reinterpret_cast<uint8_t*>(const_cast<char*>("Status:")), -1);
        std::snprintf(line, sizeof(line), "%s", statusNames[static_cast<uint8_t>(shown->status)]);
        systemFont->writeString(port->frame(), 100, 10, reinterpret_cast<uint8_t*>(line), -1);
        systemFont->writeString(port->frame(), 2, 0x14, reinterpret_cast<uint8_t*>(const_cast<char*>("Combat Value:")),
                                -1);
        const int32_t maxCV = shown->getMaxCV();
        std::snprintf(line, sizeof(line), "%d/%d", shown->getCurCV(), maxCV);
        systemFont->writeString(port->frame(), 100, 0x14, reinterpret_cast<uint8_t*>(line), -1);

        for (int32_t i = 0; i < NUM_MECH_BODY_LOCATIONS; i++)
        {
            const int32_t y = 0x50 + i * 10;
            const ArmorLocation& armorLocation = shown->armor[i];
            const BodyLocation& bodyLocation = shown->bodyAt(i);
            systemFont->writeString(port->frame(), 2, y, reinterpret_cast<uint8_t*>(const_cast<char*>(armorNames[i])),
                                    -1);
            // Port fix: the original passes the armor and structure as doubles to %d.
            std::snprintf(line, sizeof(line), "A(%d/%d), IS(%d/%d), %s%s", static_cast<int32_t>(armorLocation.curArmor),
                          armorLocation.maxArmor, static_cast<int32_t>(bodyLocation.curInternalStructure),
                          bodyLocation.maxInternalStructure, damageNames[bodyLocation.damageState],
                          bodyLocation.hasCASE != 0 ? " [CASE]" : "");
            systemFont->writeString(port->frame(), 100, y, reinterpret_cast<uint8_t*>(line), -1);
        }

        for (int32_t i = 0; i < 3; i++)
        {
            const int32_t y = 0xa0 + i * 10;
            const ArmorLocation& armorLocation = shown->armor[NUM_MECH_BODY_LOCATIONS + i];
            systemFont->writeString(
                port->frame(), 2, y,
                reinterpret_cast<uint8_t*>(const_cast<char*>(armorNames[NUM_MECH_BODY_LOCATIONS + i])), -1);
            // Port fix: as above.
            std::snprintf(line, sizeof(line), "A(%d/%d)", static_cast<int32_t>(armorLocation.curArmor),
                          armorLocation.maxArmor);
            systemFont->writeString(port->frame(), 100, y, reinterpret_cast<uint8_t*>(line), -1);
        }

        systemFont->writeString(port->frame(), 2, 200, reinterpret_cast<uint8_t*>(const_cast<char*>("Inventory:")), -1);
        systemFont->writeString(port->frame(), 0xc, 0xd2, reinterpret_cast<uint8_t*>(const_cast<char*>("Weapons:")),
                                -1);
        const int32_t numOther = shown->numOther;
        const int32_t numWeapons = shown->numWeapons;

        for (int32_t i = numOther; i < numOther + numWeapons; i++)
        {
            const InventoryItem& item = shown->inventory[i];
            const double ready = item.readyTime <= scenarioTime ? 0.0 : item.readyTime - scenarioTime;
            const MasterComponent& component = MasterComponentList[item.masterID];
            // Port fix: the original passes the whole ammo tally by value to AMMO(%d), which misaligns RDY too.
            std::snprintf(line, sizeof(line), "%s: [%s] ID(%d), H(%d/%d), AMMO(%d), RDY(%.2f)%s", component.name,
                          locationNames[item.bodyLocation], item.masterID, item.health,
                          static_cast<int8_t>(component.criticalSpacesReq),
                          shown->ammoTypeTotal[item.ammoIndex + 1].curAmount, ready,
                          item.disabled != 0 ? " DISABLED" : "");
            systemFont->writeString(port->frame(), 0x16, 0xdc + (i - numOther) * 10, reinterpret_cast<uint8_t*>(line),
                                    -1);
        }

        systemFont->writeString(port->frame(), 0xc, (numWeapons * 5 + 0x6e) * 2,
                                reinterpret_cast<uint8_t*>(const_cast<char*>("Misc:")), -1);
        int32_t y = (numWeapons * 5 + 0x73) * 2;

        for (int32_t i = 0; i < numOther; i++)
        {
            const InventoryItem& item = shown->inventory[i];
            const MasterComponent& component = MasterComponentList[item.masterID];
            std::snprintf(line, sizeof(line), "%s: [%s] ID(%d), H(%d/%d)%s", component.name,
                          locationNames[item.bodyLocation], item.masterID, item.health,
                          static_cast<int8_t>(component.criticalSpacesReq), item.disabled != 0 ? " DISABLED" : "");
            systemFont->writeString(port->frame(), 0x16, y, reinterpret_cast<uint8_t*>(line), -1);
            y += 10;
        }
    }

    aObject::display();
}

auto MechStatusWindow::draw() -> void
{
    if (mech != nullptr)
    {
        char title[256];
        std::snprintf(title, sizeof(title), "%s %s (%s)", AlignmentNames[mech->getAlignment() + 1], mech->debugStatus,
                      mech->getPilot()->callsign);
        setTitle(title);
    }

    aTitleWindow::draw();
}
