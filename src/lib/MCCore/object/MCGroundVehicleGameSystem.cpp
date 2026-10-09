#include "stdafx.h"
#include "object/MCGroundVehicleGameSystem.h"
#include "object/MCGameSystemReader.h"
#include "object/MCGroundVehicleDynamics.h"
#include "object/MCMoverGameSystem.h"

int32_t GroundVehicleAttackerMoveModifier[4] = {};
int32_t GroundVehicleCriticalHitTable[11] = {};
float TileThrottleMultiplier[3][NumThrottleTileTypes] = {};
float OverlayThrottleMultiplier[3][NumThrottleOverlayTypes] = {};

namespace
{
    /// <summary>Fills the throttle tables with MCX.EXE's initial data: every entry is 1.0 (no tile or overlay
    /// slows a vehicle).</summary>
    const bool ThrottleTablesFilled = []
    {
        std::fill_n(&TileThrottleMultiplier[0][0], 3 * NumThrottleTileTypes, 1.0f);
        std::fill_n(&OverlayThrottleMultiplier[0][0], 3 * NumThrottleOverlayTypes, 1.0f);
        return true;
    }();
}

int32_t DefaultGroundVehicleCrashAvoidSelf = 1;
int32_t DefaultGroundVehicleCrashAvoidPath = 1;
int32_t DefaultGroundVehicleCrashBlockSelf = 1;
int32_t DefaultGroundVehicleCrashBlockPath = 1;
float DefaultGroundVehicleCrashYieldTime = 2.0f;
uint32_t WeaponFXTable[32] = {455, 461, 462, 188, 467, 468, 0xffffffff, 458, 14,  190, 191, 192, 456, 463, 464, 457,
                              465, 466, 459, 460, 879, 880, 881,        882, 883, 884, 885, 886, 887, 888, 889, 890};
float GvCollisionThreshold = 0.0f;
float GvObjectCollisionThreshold = 0.0f;
float GvTonnageCollisionThreshold = 0.0f;
float GvTreeDeflection = 0.0f;
float GvSweepTime = 0.0f;
float GvHillSpeedFactor = 0.0f;
float MaxVelocityMag = 0.0f;

auto LoadGroundVehicleGameSystem(MCFitIniFile& sysFile) -> int32_t
{
    // Each check below is where the original returned the first error; what follows it ran only without one.
    MCGameSystemReader read(sysFile);
    read.Block("GroundVehicle:FireWeapon");
    read.Array("AttackerMoveModifier", GroundVehicleAttackerMoveModifier);
    read.Block("GroundVehicle:Damage");
    read.Array("CriticalHitTable", GroundVehicleCriticalHitTable);
    read.Block("GroundVehicle:Collision");
    read.Value("collisionThreshold", GvCollisionThreshold);
    read.Value("objectThreshold", GvObjectCollisionThreshold);
    read.Value("tonnageThreshold", GvTonnageCollisionThreshold);
    read.Value("treeDeflection", GvTreeDeflection);
    read.Block("GroundVehicle:Movement");

    if (read.Error() != 0)
    {
        return read.Error();
    }

    MCGameSystemReader::Optional(sysFile, "CrashAvoidSelf", DefaultGroundVehicleCrashAvoidSelf);
    MCGameSystemReader::Optional(sysFile, "CrashAvoidPath", DefaultGroundVehicleCrashAvoidPath);
    MCGameSystemReader::Optional(sysFile, "CrashBlockSelf", DefaultGroundVehicleCrashBlockSelf);
    MCGameSystemReader::Optional(sysFile, "CrashBlockPath", DefaultGroundVehicleCrashBlockPath);
    MCGameSystemReader::Optional(sysFile, "CrashYieldTime", DefaultGroundVehicleCrashYieldTime);
    read.Value("SweeperSlowTime", GvSweepTime);
    read.Value("WalkSpeed", GvWalkSpeed);
    read.Value("HillSpeedFactor", GvHillSpeedFactor);
    return read.Error();
}
