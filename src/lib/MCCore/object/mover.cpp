#include "stdafx.h"
#include "object/mover.h"
#include "ai/move.h"
#include "appear/appear.h"
#include "engine/bitflag.h"
#include "lib/aerror.h"
#include "lib/file.h"
#include "lib/inifile.h"
#include "logistics/logmain.h"
#include "main/logistics.h"
#include "main/main.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/cmponent.h"
#include "object/contact.h"
#include "object/group.h"
#include "object/control.h"
#include "object/dyn.h"
#include "object/elemntl.h"
#include "object/mech.h"
#include "object/objevnt.h"
#include "object/objque.h"
#include "object/objtype.h"
#include "object/object.h"
#include "object/sortlist.h"
#include "object/team.h"
#include "object/warrior.h"
#include "sound/soundsys.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"

float DelayedOrderTime = 1.0f;
int32_t StatusChunkUnpackErr = 0;
float FireArc[3] = {};
int32_t AntiMissileSystemStats[2][2] = {{1, 2}, {2, 2}};
float DamageRateFrequency = 10.0f;
float MaxVisualRadius = 0.0f;
int32_t TargetRolo = -1;
float EntryAngleTable[4] = {0.0f, 180.0f, -90.0f, 90.0f};
// Cleared at startup as WeaponFireChunk::init clears (MCX.EXE @ 0x00683d80): no hit location.
WeaponFireChunk CurMoverWeaponFireChunk = {0, 0, 0, 0, 0, {0, 0}, 0, 0, 0, 0, 0, 0, -1, 0};
int32_t Mover::numMovers = 0;
SortList* Mover::sortList = nullptr;
int32_t goalMap[GOALMAP_CELL_DIM][GOALMAP_CELL_DIM];
float WeaponRange[3] = {250.0f, 500.0f, 1000.0f};
float DefaultAttackRange = 75.0f;
int32_t NumRangeRatings = 31;
float RangeRatingIncrement = 30.0f;
float MaxStationaryTime = 0.0f;
float GroupOrderGoalOffset = 127.0f;
float MinRangeIncrement = 30.0f;
float MinRangeModIncrement = 10.0f;
float MaxWeaponRangeMod = 45.0f;
float DisableAttackModifier = 10.0f;
float DisableGunneryModifier = 5.0f;
float SalvageAttackModifier = 30.0f;
float PilotingCheckFactor = 1.0f;
int32_t hitLevel[2] = {10, 20};
int32_t ClusterSizeSRM = 2;
int32_t ClusterSizeLRM = 5;
float PilotCheckHalfRate = 5.0f;
uint8_t AttitudeEffect[6][6] = {{0x32, 0x4b, 0x05, 0xfe, 0x00, 0x03}, {0x28, 0x3c, 0x0a, 0xff, 0x01, 0x05},
                                {0x1e, 0x32, 0x0f, 0x00, 0x02, 0x0a}, {0x14, 0x28, 0x14, 0x01, 0x03, 0x0f},
                                {0x0a, 0x19, 0x19, 0x01, 0x04, 0x14}, {0x00, 0x00, 0x20, 0x02, 0x05, 0x80}};
float SensorBaseChance = 50.0f;
float SensorSkillFactor = 10.0f;
float SensorBlockingObjectModifier = -5.0f;
float SensorShutDownMechModifier = -50.0f;
float SensorRangeModifier[4][2] = {{0.25f, 50.0f}, {0.15f, 20.0f}, {0.35f, 0.0f}, {0.25f, -25.0f}};
float SensorSizeModifier[3][2] = {{25.0f, -10.0f}, {60.0f, 0.0f}, {999.0f, 15.0f}};
float SensorBlockingTerrain[2] = {-25.0f, -5.0f};
float SensorSkill = 0.0f;
float RefitRange = 0.0f;
float SkillTry[4] = {};
float RefitCostArray[3][2] = {};
float RefitTime = 0.0f;
float RefitAmount = 0.0f;
int32_t LongRangeMovementEnabled[3] = {};
float SkillSuccess[4] = {};
int32_t AimedFireHitTable[3] = {};
int32_t AimedFireAbort = 0;
float KillSkill[6] = {};
float WeaponHit = 0.0f;
int32_t PilotJumpMod = 0;
int32_t IncreaseCap = 0;

namespace
{
    /// <summary>The diagonal steps (row, column) that walk calcMoveGoal's diamond around the goal. Unnamed in
    /// MCX.EXE (0x00791968).</summary>
    const int32_t GoalRingStep[4][2] = {{1, 1}, {1, -1}, {-1, -1}, {-1, 1}};

    /// <summary>Whether <paramref name="object"/> is a mover (mech, vehicle, elemental or plain mover).</summary>
    bool IsMover(const BaseObject* object)
    {
        const ObjectClass objectClass = object->objectClass;
        return objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
               objectClass == MOVER;
    }

    /// <summary>Checks a chunk's target the way pack and unpack both do, setting StatusChunkUnpackErr.</summary>
    void CheckStatusChunkTarget(const StatusChunk* chunk)
    {
        switch (static_cast<int8_t>(chunk->targetType))
        {
            case 0:
            case 4:
                break;
            case 1:
            {
                const int32_t targetId = chunk->targetId;

                if (targetId < 0 || MPlayer->numMovers <= targetId)
                {
                    StatusChunkUnpackErr = 1;
                }

                if (MPlayer->moverRoster[targetId] == nullptr)
                {
                    StatusChunkUnpackErr = 2;
                }
                break;
            }

            case 2:
            {
                if (objectList->findObjectFromPart(chunk->targetId) == nullptr)
                {
                    StatusChunkUnpackErr = 3;
                }
                break;
            }
            case 3:
            {
                if (objectList->findObjectFromPart(chunk->targetId) == nullptr)
                {
                    StatusChunkUnpackErr = 4;
                }
                break;
            }
            default:
                StatusChunkUnpackErr = 5;
                break;
        }
    }

    /// <summary>Appends one chunk's fields to ChunkDebugMsg (DebugStatusChunk does it for each of its two).</summary>
    void AppendStatusChunk(const StatusChunk* chunk)
    {
        char line[512];
        const int8_t targetType = static_cast<int8_t>(chunk->targetType);
        BaseObject* target = nullptr;
        bool haveTarget = false;

        if (targetType == 1)
        {
            target = MPlayer->moverRoster[chunk->targetId];
            haveTarget = true;
        }
        else if (targetType == 2 || targetType == 3)
        {
            target = objectList->findObjectFromPart(chunk->targetId);
            haveTarget = true;
        }

        bool appendLine = true;

        if (haveTarget && target != nullptr)
        {
            if (IsMover(target))
            {
                std::snprintf(line, sizeof(line), "target = %s (%d)\n",
                              static_cast<Mover*>(target)->debugStatus.c_str(), target->partId);
            }
            else
            {
                std::snprintf(line, sizeof(line), "target = objClass %d (%d)\n", static_cast<int>(target->objectClass),
                              target->partId);
            }
        }
        else if (!haveTarget && targetType == 4)
        {
            // The middle of the target cell, on the ground.
            const float halfSide = worldUnitsMapSide * 0.5f;
            vector_3d point;
            point.x =
                static_cast<float>((chunk->targetCellRC[1] + 0.5f) * static_cast<double>(MetersPerCell) - halfSide);
            point.y = static_cast<float>(
                (static_cast<double>(halfSide) - chunk->targetCellRC[0] * static_cast<double>(MetersPerCell)) -
                static_cast<double>(MetersPerCell) * 0.5f);
            point.z = 0.0f;
            const float elevation = GameMap->getTerrainElevation(point);
            std::snprintf(line, sizeof(line), "target point = (%f, %f, %f)\n", static_cast<double>(point.x),
                          static_cast<double>(point.y), static_cast<double>(elevation));
        }
        else if (targetType == 0)
        {
            std::snprintf(line, sizeof(line), "target = NONE\n");
        }
        else
        {
            std::strcat(ChunkDebugMsg, "target = ???\n");
            appendLine = false;

            if (targetType == 2)
            {
                // List the terrain objects on the target's vertex.
                const int32_t firstId = chunk->targetId - static_cast<int8_t>(chunk->targetItemNumber);
                int32_t numObjects = 0;

                for (int32_t i = 0; i < 8; i++)
                {
                    BaseObject* object = objectList->findObjectFromPart(firstId + i);

                    if (object == nullptr)
                    {
                        continue;
                    }

                    numObjects++;
                    std::snprintf(line, sizeof(line), "    %d: objClass %d (%d)\n", i,
                                  static_cast<int>(object->objectClass), object->partId);
                    std::strcat(ChunkDebugMsg, line);
                }

                if (numObjects > 0)
                {
                    std::snprintf(line, sizeof(line), "    There are %d terrain objects in this tile.\n", numObjects);
                    appendLine = true;
                }
            }
        }

        if (appendLine)
        {
            std::strcat(ChunkDebugMsg, line);
        }

        std::snprintf(line, sizeof(line), "bodyState = %d\n", static_cast<int>(chunk->bodyState));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetType = %d\n", static_cast<int>(targetType));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetId = %d\n", chunk->targetId);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetBlockOrTrainNumber = %d\n", chunk->targetBlockOrTrainNumber);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetVertexOrCarNumber = %d\n", chunk->targetVertexOrCarNumber);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetItemNumber = %d\n",
                      static_cast<int>(static_cast<int8_t>(chunk->targetItemNumber)));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetCellRC = (%d, %d)\n", static_cast<int>(chunk->targetCellRC[0]),
                      static_cast<int>(chunk->targetCellRC[1]));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "ejectOrderGiven = %c\n", chunk->ejectOrderGiven != 0 ? 'T' : 'F');
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "jumpOrder = %c\n", chunk->jumpOrder != 0 ? 'T' : 'F');
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "data = %x\n", chunk->data);
        std::strcat(ChunkDebugMsg, line);
    }

    /// <summary>
    /// The jumping the path finders allow, as calcMovePath and calcEscapePath each repeat it: 8 offsets at no cost,
    /// or the mover's jump range when an AI mover in single player. An elemental away from its last target
    /// (or without one) sets JumpOnBlocked.
    /// </summary>
    void SetUpPathJumps(Mover* mover, int32_t& numOffsets, int32_t& jumpCost)
    {
        jumpCost = 0;
        numOffsets = 8;

        if (mover->pilot->onHomeTeam() == 0 && MPlayer == nullptr)
        {
            mover->getJumpRange(&numOffsets, &jumpCost);
        }

        if (mover->objectClass != ELEMENTAL)
        {
            return;
        }

        GameObject* lastTarget = mover->pilot->getLastTarget();

        if (lastTarget != nullptr)
        {
            vector_3d targetPosition = lastTarget->getPosition();

            if (mover->distanceFrom(targetPosition) < ElementalTargetNoJumpDistance)
            {
                jumpCost = 0;
                numOffsets = 8;
                return;
            }
        }

        JumpOnBlocked = 1;
    }

    /// <summary>The move level the local path finders use: the seconds a cell takes at <paramref name="speed"/>
    /// times 50, floored, as a 16-bit value.</summary>
    int32_t LocalPathMoveLevel(float speed)
    {
        const double cellMeters = static_cast<double>(metersPerWorldUnit) * Terrain::metersPerVertexDivMAPCELL_DIM;
        return static_cast<int16_t>(static_cast<int32_t>(std::floor(cellMeters / speed * 50.0)));
    }

    /// <summary>
    /// Calls <paramref name="visit"/>(tile, cellR, cellC) for the 3x3 cells around a cell, crossing into the
    /// neighbouring tiles, until it returns true. The original unrolls this per cell; it addresses a tile as
    /// width * tileR + tileC, so a column off either side wraps to the next or previous row, as here.
    /// </summary>
    /// <returns>Whether <paramref name="visit"/> stopped the walk.</returns>
    template <typename Visit>
    bool VisitCellsAround(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, Visit visit)
    {
        for (int32_t rowStep = -1; rowStep <= 1; rowStep++)
        {
            for (int32_t colStep = -1; colStep <= 1; colStep++)
            {
                int32_t row = cellR + rowStep;
                int32_t rowTile = tileR;

                if (row < 0)
                {
                    row += MAPCELL_DIM;
                    rowTile--;
                }
                else if (row >= MAPCELL_DIM)
                {
                    row -= MAPCELL_DIM;
                    rowTile++;
                }

                int32_t col = cellC + colStep;
                int32_t colTile = tileC;

                if (col < 0)
                {
                    col += MAPCELL_DIM;
                    colTile--;
                }
                else if (col >= MAPCELL_DIM)
                {
                    col -= MAPCELL_DIM;
                    colTile++;
                }

                const int32_t index = GameMap->width * rowTile + colTile;

                // Port fix: the original reads and writes before or past the map for a cell on its first or last row.
                if (index < 0 || index >= GameMap->width * GameMap->height)
                {
                    continue;
                }

                if (visit(GameMap->map[index], row, col))
                {
                    return true;
                }
            }
        }

        return false;
    }
}

//---------------------------------------------------------------------------
// StatusChunk
//---------------------------------------------------------------------------

auto StatusChunk::init() -> void
{
    bodyState = 0;
    targetType = 0;
    targetId = 0;
    targetBlockOrTrainNumber = 0;
    targetVertexOrCarNumber = 0;
    targetItemNumber = 0;
    targetCellRC[0] = -1;
    targetCellRC[1] = -1;
    ejectOrderGiven = 0;
    jumpOrder = 0;
    data = 0;
}

auto StatusChunk::build(Mover*) -> void
{
    bodyState = 0;
    data = 0;
}

auto StatusChunk::pack(Mover*) -> void
{
    // Bits, low to high: body state (3), jump order, eject order, target type (3), then the target.
    data = 0;
    bool packCell = jumpOrder != 0;

    if (!packCell)
    {
        switch (targetType)
        {
            case 1:
                data = static_cast<uint32_t>(targetId) << 3;
                break;
            case 2:
                data = ((static_cast<uint32_t>(targetBlockOrTrainNumber) << 9 |
                         static_cast<uint32_t>(targetVertexOrCarNumber))
                            << 3 |
                        static_cast<uint32_t>(static_cast<int8_t>(targetItemNumber)))
                       << 3;
                break;
            case 3:
                data = (static_cast<uint32_t>(targetBlockOrTrainNumber) << 8 |
                        static_cast<uint32_t>(targetVertexOrCarNumber))
                       << 3;
                break;
            case 4:
                packCell = true;
                break;
            default:
                break;
        }
    }

    if (packCell)
    {
        data = (static_cast<uint32_t>(static_cast<int32_t>(targetCellRC[0])) << 10 |
                static_cast<uint32_t>(static_cast<int32_t>(targetCellRC[1])))
               << 3;
    }

    data = (static_cast<uint32_t>(static_cast<int8_t>(targetType)) | data) * 2;

    if (ejectOrderGiven != 0)
    {
        data |= 1;
    }

    data <<= 1;

    if (jumpOrder != 0)
    {
        data |= 1;
    }

    data = bodyState | data << 3;
    CheckStatusChunkTarget(this);
}

auto StatusChunk::unpack(Mover*) -> void
{
    uint32_t bits = data;
    StatusChunkUnpackErr = 0;
    bodyState = bits & 7;
    jumpOrder = static_cast<int32_t>(bits >> 3 & 1);
    ejectOrderGiven = static_cast<int32_t>(bits >> 4 & 1);
    targetType = static_cast<uint8_t>(bits >> 5 & 7);

    bool unpackCell = jumpOrder != 0;

    if (!unpackCell)
    {
        switch (targetType)
        {
            case 1:
                targetId = static_cast<int32_t>(bits >> 8 & 0x1f);
                break;
            case 2:
            {
                // A terrain object: its block, vertex and item on the vertex.
                targetItemNumber = static_cast<uint8_t>(bits >> 8 & 7);
                const uint32_t block = bits >> 0x14 & 0xff;
                const uint32_t vertex = bits >> 0xb & 0x1ff;
                targetBlockOrTrainNumber = static_cast<int32_t>(block);
                targetVertexOrCarNumber = static_cast<int32_t>(vertex);
                targetId =
                    static_cast<int8_t>(targetItemNumber) + 0x1000 + static_cast<int32_t>((block * 400 + vertex) * 8);
                break;
            }

            case 3:
            {
                // A train car.
                const uint32_t train = bits >> 0x10 & 0xff;
                const uint32_t car = bits >> 8 & 0xff;
                targetVertexOrCarNumber = static_cast<int32_t>(car);
                targetBlockOrTrainNumber = static_cast<int32_t>(train);

                if (train == 0x80)
                {
                    targetId = static_cast<int32_t>(car + 0x802c8);
                }
                else
                {
                    targetId = static_cast<int32_t>(car + (train * 5 + 0x6400) * 0x14);
                }
                break;
            }

            case 4:
                unpackCell = true;
                break;
            default:
                break;
        }
    }

    if (unpackCell)
    {
        targetCellRC[1] = static_cast<int16_t>(bits >> 8 & 0x3ff);
        targetCellRC[0] = static_cast<int16_t>(bits >> 0x12 & 0x3ff);
    }

    CheckStatusChunkTarget(this);
}

auto StatusChunk::equalTo(StatusChunk* chunk) -> int
{
    if (bodyState != chunk->bodyState || ejectOrderGiven != chunk->ejectOrderGiven || jumpOrder != chunk->jumpOrder ||
        targetType != chunk->targetType || targetId != chunk->targetId || targetCellRC[0] != chunk->targetCellRC[0] ||
        targetCellRC[1] != chunk->targetCellRC[1])
    {
        DebugStatusChunk(nullptr, this, chunk);
        return 0;
    }

    return 1;
}

auto loadMoverGameSystem(FitIniFile* sysFile, float maxVisualRange) -> int32_t
{
    int32_t result = sysFile->seekBlock("Pathfinding");

    if (result != 0)
    {
        return result;
    }

    int32_t longRangeEnabled[3];
    result = sysFile->readIdLongArray("LongRangeMovementEnabled", longRangeEnabled, 3);

    if (result != 0)
    {
        return result;
    }

    for (int32_t i = 0; i < 3; i++)
    {
        LongRangeMovementEnabled[i] = longRangeEnabled[i] == 1 ? 1 : 0;
    }

    result = sysFile->readIdLong("SimplePathTileRange", SimpleMovePathRange);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("DelayedOrderTime", DelayedOrderTime);

    if (result != 0)
    {
        return result;
    }

    if (sysFile->readIdFloat("MoveTimeOut", MoveTimeOut) != 0)
    {
        MoveTimeOut = 30.0f;
    }

    if (sysFile->readIdFloat("MoveYieldTime", MoveYieldTime) != 0)
    {
        MoveYieldTime = 1.5f;
    }

    // Read twice, as the original does.
    if (sysFile->readIdLongArray("GroupMoveTrailLength", GroupMoveTrailLen, 2) != 0)
    {
        GroupMoveTrailLen[0] = 0;
        GroupMoveTrailLen[1] = 1;
    }

    if (sysFile->readIdLongArray("GroupMoveTrailLength", GroupMoveTrailLen, 2) != 0)
    {
        GroupMoveTrailLen[0] = 0;
        GroupMoveTrailLen[1] = 1;
    }

    result = sysFile->readIdFloat("GroupOrderGoalOffset", GroupOrderGoalOffset);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloatArray("MoveMarginOfError", MoveMarginOfError, 2);

    if (result != 0)
    {
        return result;
    }

    for (int32_t i = 0; i < NUM_OVERLAY_TYPES; i++)
    {
        OverlayWeightIndex[i] = i * MAPCELL_DIM * MAPCELL_DIM;
    }

    result =
        sysFile->readIdLongArray("OverlayCellCosts", OverlayWeightTable, NUM_MOVE_LEVELS * OVERLAY_WEIGHT_LEVEL_SIZE);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->seekBlock("OptimumRange");

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdLong("NumRangeRatings", NumRangeRatings);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("RangeRatingIncrement", RangeRatingIncrement);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("MinRangeIncrement", MinRangeIncrement);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("MinRangeModIncrement", MinRangeModIncrement);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("MaxWeaponRangeMod", MaxWeaponRangeMod);

    if (result != 0)
    {
        return result;
    }

    Assert(sysFile->seekBlock("Mover:General") == 0, 0, "Couldn't find Mover:General block in gamesys.fit");
    Assert(sysFile->readIdFloat("BlockCaptureRange", BlockCaptureRange) == 0, 0,
           "Couldn't find BlockCaptureRange in Mover:General block in gamesys.fit");
    Assert(sysFile->readIdFloat("RefitTime", RefitTime) == 0, 0,
           "Couldn't find RefitTime in Mover:General block in gamesys.fit");
    Assert(sysFile->readIdFloat("RefitRange", RefitRange) == 0, 0,
           "Couldn't find RefitRange in Mover:General block in gamesys.fit");
    Assert(sysFile->readIdFloat("RefitAmount", RefitAmount) == 0, 0,
           "Couldn't find RefitAmount in Mover:General block in gamesys.fit");
    Assert(sysFile->readIdFloat("RefitVehicleArmorCost", RefitCostArray[0][0]) == 0, 0,
           "Couldn't find RefitVehicleArmorCost in Mover:General block in gamesys.fit");
    Assert(sysFile->readIdFloat("RefitVehicleInternalCost", RefitCostArray[1][0]) == 0, 0,
           "Couldn't find RefitVehicleInternalCost in Mover:General block in gamesys.fit");
    Assert(sysFile->readIdFloat("RefitVehiclePointsToAmmo", RefitCostArray[2][0]) == 0, 0,
           "Couldn't find RefitVehiclePointsToAmmo in Mover:General block in gamesys.fit");
    Assert(sysFile->readIdFloat("RefitBayArmorCost", RefitCostArray[0][1]) == 0, 0,
           "Couldn't find RefitBayArmorCost in Mover:General block in gamesys.fit");
    Assert(sysFile->readIdFloat("RefitBayInternalCost", RefitCostArray[1][1]) == 0, 0,
           "Couldn't find RefitBayInternalCost in Mover:General block in gamesys.fit");
    Assert(sysFile->readIdFloat("RefitBayAmmoCost", RefitCostArray[2][1]) == 0, 0,
           "Couldn't find RefitBayAmmoCost in Mover:General block in gamesys.fit");

    result = sysFile->seekBlock("Mover:FireWeapon");

    if (result != 0)
    {
        return result;
    }

    // Missing modifiers keep the defaults; [7..22] also fill RankVersusChassisCombatModifier's columns 1..4.
    if (sysFile->readIdFloatArray("WeaponFireModifiers", WeaponFireModifiers, 30) == 0)
    {
        for (int32_t rank = 0; rank < 4; rank++)
        {
            for (int32_t chassis = 0; chassis < 4; chassis++)
            {
                RankVersusChassisCombatModifier[rank][chassis + 1] = WeaponFireModifiers[7 + rank * 4 + chassis];
            }
        }
    }

    result = sysFile->readIdFloatArray("FireArc", FireArc, 3);

    if (result != 0)
    {
        return result;
    }

    // Stored as half arcs.
    for (float& arc : FireArc)
    {
        arc = static_cast<float>(arc * 0.5);
    }

    result = sysFile->readIdLong("AimedFireAbort", AimedFireAbort);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdLongArray("AimedFireHitTable", AimedFireHitTable, 3);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("DisableAttackModifier", DisableAttackModifier);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("DisableGunneryModifier", DisableGunneryModifier);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("SalvageAttackModifier", SalvageAttackModifier);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("MaxStationaryTime", MaxStationaryTime);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->seekBlock("Mover:Damage");

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdLongArray("HitLevel", hitLevel, 2);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("PilotingCheckFactor", PilotingCheckFactor);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->seekBlock("Components");

    if (result != 0)
    {
        return result;
    }

    // The cluster sizes are read, then fixed at 2 and 5.
    result = sysFile->readIdLong("ClusterSizeSRM", ClusterSizeSRM);

    if (result != 0)
    {
        return result;
    }

    ClusterSizeSRM = 2;
    result = sysFile->readIdLong("ClusterSizeLRM", ClusterSizeLRM);

    if (result != 0)
    {
        return result;
    }

    ClusterSizeLRM = 5;
    result = sysFile->readIdLongArray("InnerSphereAntiMissile", AntiMissileSystemStats[0], 2);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdLongArray("ClanAntiMissile", AntiMissileSystemStats[1], 2);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->seekBlock("Warrior");

    if (result != 0)
    {
        return result;
    }

    if (sysFile->readIdFloat("DefaultAttackRadius", DefaultAttackRadius) != 0)
    {
        DefaultAttackRadius = 275.0f;
    }

    result = sysFile->readIdFloatArray("WarriorRankScale", WarriorRankScale, 4);

    if (result != 0)
    {
        return result;
    }

    char table[10];
    result = sysFile->readIdCharArray("ProfessionalismTable", table, 10);

    if (result != 0)
    {
        return result;
    }

    std::memcpy(ProfessionalismOffsetTable, table, sizeof(ProfessionalismOffsetTable));
    result = sysFile->readIdCharArray("DecorumTable", table, 10);

    if (result != 0)
    {
        return result;
    }

    std::memcpy(DecorumOffsetTable, table, sizeof(DecorumOffsetTable));
    result = sysFile->readIdCharArray("AmmoTable", table, 4);

    if (result != 0)
    {
        return result;
    }

    std::memcpy(AmmoConservationModifiers, table, sizeof(AmmoConservationModifiers));
    result = sysFile->readIdFloat("PilotCheckHalfRate", PilotCheckHalfRate);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdLongArray("PilotCheckModifiers", PilotCheckModifierTable, 2);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("DamageRateFrequency", DamageRateFrequency);

    if (result != 0)
    {
        return result;
    }

    char attitudeEffect[sizeof(AttitudeEffect)];
    result = sysFile->readIdCharArray("AttitudeEffect", attitudeEffect, sizeof(attitudeEffect));

    if (result != 0)
    {
        return result;
    }

    std::memcpy(AttitudeEffect, attitudeEffect, sizeof(AttitudeEffect));
    result = sysFile->readIdFloat("MovementUpdateFrequency", MovementUpdateFrequency);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("CombatUpdateFrequency", CombatUpdateFrequency);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("CommandUpdateFrequency", CommandUpdateFrequency);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("ContactUpdateFrequency", ContactUpdateFrequency);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("PilotCheckUpdateFrequency", PilotCheckUpdateFrequency);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloatArray("FireOddsTable", FireOddsTable, 5);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdLong("SkillIncreaseCap", IncreaseCap);
    Assert(result == 0, result, " Couldn't find SkillCap variable in Warrior block of gamesys.fit ");
    result = sysFile->readIdFloat("SkillMax", MaxPilotSkill);
    Assert(result == 0, result, " Couldn't find SkillMax variable in Warrior block of gamesys.fit ");
    result = sysFile->readIdFloat("SkillMin", MinPilotSkill);
    Assert(result == 0, result, " Couldn't find SkillMin variable in Warrior block of gamesys.fit ");
    result = sysFile->readIdLong("JumpSkillMod", PilotJumpMod);
    Assert(result == 0, result, " Couldn't find JumpSkillMod variable in Warrior block of gamesys.fit ");

    result = sysFile->seekBlock("Sensors");

    if (result != 0)
    {
        return result;
    }

    char automaticSuccess;
    result = sysFile->readIdChar("AutomaticSuccess", automaticSuccess);

    if (result != 0)
    {
        return result;
    }

    SensorAutomaticSuccess = automaticSuccess == 1 ? 1 : 0;
    result = sysFile->readIdCharArray("SensorSkillMoveRange", SensorSkillMoveRange, 4);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloatArray("SensorSkillMoveFactor", &SensorSkillMoveFactor[0][0], 8);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloatArray("SensorModifiers", SensorModifier, 8);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("BaseSensorRollTarget", SensorBaseChance);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("SensorSkillFactor", SensorSkillFactor);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("BlockingObjectModifier", SensorBlockingObjectModifier);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("ShutdownMech", SensorShutDownMechModifier);

    if (result != 0)
    {
        return result;
    }

    float rangeModifiers[8];
    result = sysFile->readIdFloatArray("SensorRangeModifier", rangeModifiers, 8);

    if (result != 0)
    {
        return result;
    }

    std::memcpy(SensorRangeModifier, rangeModifiers, sizeof(SensorRangeModifier));
    float sizeModifiers[6];
    result = sysFile->readIdFloatArray("SizeModifier", sizeModifiers, 6);

    if (result != 0)
    {
        return result;
    }

    std::memcpy(SensorSizeModifier, sizeModifiers, sizeof(SensorSizeModifier));
    // Read and dropped.
    int32_t sensorMasterIds[9];
    result = sysFile->readIdLongArray("SensorMasterIDs", sensorMasterIds, 9);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloatArray("BlockingTerrainModifiers", SensorBlockingTerrain, 2);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->seekBlock("Skills");

    if (result != 0)
    {
        Fatal(result, "Couldn't find skill block in gamesys.fit");
    }

    result = sysFile->readIdFloatArray("Skill Attempt", SkillTry, 4);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloatArray("Skill Success", SkillSuccess, 4);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("WeaponHit", WeaponHit);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->readIdFloatArray("KillSkillValues", KillSkill, 6);

    if (result != 0)
    {
        return result;
    }

    return sysFile->readIdFloat("Sensor Contact Skill", SensorSkill);
}

auto DebugStatusChunk(Mover* mover, StatusChunk* chunk1, StatusChunk* chunk2) -> void
{
    char line[512];
    ChunkDebugMsg[0] = '\0';

    if (mover == nullptr)
    {
        std::strcat(ChunkDebugMsg, "\nmover = ???\n");
    }
    else
    {
        std::snprintf(line, sizeof(line), "\nmover = %s (%d)\n", mover->debugStatus.c_str(), mover->partId);
        std::strcat(ChunkDebugMsg, line);
    }

    if (chunk1 != nullptr)
    {
        std::strcat(ChunkDebugMsg, "\nCHUNK1\n");
        AppendStatusChunk(chunk1);
    }

    if (chunk2 != nullptr)
    {
        std::strcat(ChunkDebugMsg, "\nCHUNK2\n");
        AppendStatusChunk(chunk2);
    }

    auto* file = new File;
    file->create("stchunk.dbg");
    file->writeString(ChunkDebugMsg);
    file->close();
    delete file;
    ExceptionGameMsg = ChunkDebugMsg;
}

auto getMoverFromPartId(int32_t partId) -> Mover*
{
    // Port fix: the original indexed MoverRoster from part id 0, reading the memory before it for ids under 0x200.
    if (partId >= 0x200 && partId < MAX_MOVER_PART_ID)
    {
        return static_cast<Mover*>(MoverRoster[partId - 0x200]);
    }

    return nullptr;
}

//---------------------------------------------------------------------------
// Mover: the header's inline functions
//---------------------------------------------------------------------------

auto Mover::lineOfSight(GameObject* target) -> int
{
    return team->lineOfSight(target->getPosition());
}

auto Mover::lineOfSight(vector_3d point) -> int
{
    return team->lineOfSight(point);
}

auto Mover::forcePilotingCheck() -> void
{
    if (pilotCheckModifier < 0)
    {
        pilotCheckModifier = 0;
    }
}

auto Mover::setAlignment(int32_t newAlignment) -> void
{
    BigGameObject::setAlignment(newAlignment);

    if (pilot != nullptr)
    {
        pilot->alignment = static_cast<int8_t>(newAlignment);
    }
}

auto Mover::relViewFacingTo(vector_3d goal) -> float
{
    return GameObject::relFacingTo(goal, -1);
}

auto Mover::getJumpRange(int32_t* numOffsets, int32_t* jumpCost) -> float
{
    if (numOffsets != nullptr)
    {
        *numOffsets = 8;
    }

    if (jumpCost != nullptr)
    {
        *jumpCost = 0;
    }

    return 0.0f;
}

auto Mover::calcSpriteSpeed(float, uint32_t, int32_t& state, int32_t& throttle) -> int32_t
{
    state = 0;
    throttle = 100;
    return -1;
}

auto Mover::getPositionFromHS(uint32_t) -> vector_3d
{
    vector_3d position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    return position;
}

//---------------------------------------------------------------------------
// Mover
//---------------------------------------------------------------------------

auto Mover::init() -> void
{
    // The base objects' fields (their inits are inline and not called).
    idNumber = 0;
    position.y = 0.0f;
    next = nullptr;
    partId = -1;
    objType = nullptr;
    position.z = 0.0f;
    position.x = 0.0f;
    selected = 0;
    collisionsOn = 0;
    alignment = 0;
    status = 0;
    objectClass = MOVER;

    if (MPlayer == nullptr)
    {
        netName = nullptr;
    }
    else
    {
        netName = std::make_unique<char[]>(0x100);
        cLoadString(thisInstance, 0xb9, netName.get(), 0xfe);
    }

    cockpit = 0xff;
    engine = 0xff;
    lifeSupport = 0xff;
    sensor = 0xff;
    ecm = 0xff;
    probe = 0xff;
    jammer = 0xff;
    selected = 0;
    statusChunk.bodyState = 0;
    debugStatus.clear();
    pilot = nullptr;
    inventory = nullptr;
    sensorSystem = nullptr;
    ecmTracker = nullptr;
    jammerTracker = nullptr;
    curCV = 0;
    maxCV = 0;
    body = nullptr;
    numBodyLocations = 0;
    armor = nullptr;
    numArmorLocations = 0;
    damageRateTally = 0.0f;
    damageRateCheckTime = 1.0f;
    totalDamageTaken = 0.0f;
    status = 0;
    engineBlowTime = -1.0f;
    maxRunSpeed = 0.0f;
    shutDownThisFrame = 0;
    startUpThisFrame = 0;
    disableThisFrame = 0;
    team = nullptr;
    group = nullptr;
    selectionIndex = -1;
    pilotCheckModifier = -1;
    pilotingCheckPending = 0;
    lastWeaponEffectivenessCalc = 0.0f;
    lastOptimalRangeCalc = 0.0f;
    optimalRange = -1.0f;
    appearance = nullptr;
    control = nullptr;
    dynamics = nullptr;
    netOwnerID = 0;
    netPlayerId = -1;
    netRosterIndex = -1;
    newMoveChunk = 0;
    statusChunk.init();
    moveChunk.init();
    numWeaponFireChunks[1] = 0;
    numWeaponFireChunks[0] = 0;
    numCriticalHitChunks[1] = 0;
    numCriticalHitChunks[0] = 0;
    numRadioChunks[1] = 0;
    numRadioChunks[0] = 0;
    ejectOrderGiven = 0;
    deathTimer = 1.0f;
    withdrawing = 0;
    numMovers++;
    lastHustleTime = -999.0f;
    collisionsOn = 1;
    challenger = nullptr;

    if (sortList == nullptr)
    {
        sortList = new SortList;

        if (sortList == nullptr)
        {
            Fatal(0, " Unable to create Mover::sortList ");
        }

        sortList->init(100);
    }

    crashAvoidSelf = 1;
    crashAvoidPath = 1;
    pathLockLevel = 1;
    pathLockRange = 1;
    ammoTypeTotal = nullptr;
    refitBuddy = nullptr;
    crashYieldTime = 1.5f;
    numPathRangeLocks = 0;
    overlayWeightClass = 0;
    deselectTime = 0.0f;
    salvageRoll = -999;
    drawOrderLines = 0;
}

auto Mover::setPartId(int32_t newPartId) -> void
{
    partId = newPartId;

    // Port fix: the original stored ids under 0x200 before MoverRoster (see getMoverFromPartId).
    if (newPartId >= 0x200 && newPartId < MAX_MOVER_PART_ID)
    {
        MoverRoster[newPartId - 0x200] = this;
    }
}

auto Mover::setPartId(int32_t commanderId, int32_t groupId, int32_t index) -> void
{
    setPartId(index + 0x200 + (commanderId * 32 + groupId) * 12);
}

auto Mover::setPosition(vector_3d& newPosition) -> void
{
    // Kept on the map; a mover pushed off it (or into the corners, which the map's diamond cuts off) is destroyed
    // when the mover is withdrawing.
    const float halfSide = worldUnitsMapSide * 0.5f;
    const float negHalfSide = -halfSide;
    const float startX = newPosition.x;

    if (startX < negHalfSide)
    {
        newPosition.x = negHalfSide;
    }

    const float clampedX = newPosition.x;

    if (halfSide < clampedX)
    {
        newPosition.x = halfSide;
    }

    const float startY = newPosition.y;

    if (negHalfSide > startY)
    {
        newPosition.y = negHalfSide;
    }

    bool onMap = false;

    if (newPosition.y <= halfSide)
    {
        if (negHalfSide <= startY && halfSide >= clampedX && negHalfSide <= startX)
        {
            const double limit = static_cast<double>(Terrain::verticesBlockSide) * Terrain::blocksMapSide *
                                     Terrain::metersPerVertex * 0.5f -
                                 1300.0;
            const double diff = static_cast<double>(newPosition.y) - newPosition.x;
            const float sum = newPosition.x + newPosition.y;
            const float negLimit = static_cast<float>(-limit);
            onMap = !(diff > limit) && diff >= negLimit && !(sum > limit) && sum >= negLimit;
        }
    }
    else
    {
        newPosition.y = halfSide;
    }

    if (!onMap && withdrawing != 0)
    {
        objType->handleDestruction(this, nullptr);
    }

    position = newPosition;

    if (objPosition != nullptr)
    {
        GameObjectMap->updateObject(this, 0);
    }
}

auto Mover::setAwake(int awake) -> void
{
    flags &= 0xfe;

    if (awake == 0)
    {
        return;
    }

    flags |= 1;

    if (pilot != nullptr && static_cast<uint8_t>(status) == 5)
    {
        pilot->orderPowerUp(0, 2);
    }
}

auto Mover::relFacingDelta(vector_3d goalPos, vector_3d targetPos) -> float
{
    const float goalFacing = relFacingTo(goalPos, -1);
    const float targetFacing = relFacingTo(targetPos, -1);

    // The angle between the two facings, at most 180 when they're on opposite sides.
    if (goalFacing < 0.0f)
    {
        if (targetFacing >= 0.0f)
        {
            const float delta = targetFacing - goalFacing;
            return 180.0f < delta ? 180.0f : delta;
        }

        if (targetFacing < goalFacing)
        {
            return goalFacing - targetFacing;
        }
    }
    else
    {
        if (targetFacing < 0.0f)
        {
            const float delta = goalFacing - targetFacing;
            return 180.0f < delta ? 180.0f : delta;
        }

        if (targetFacing < goalFacing)
        {
            return goalFacing - targetFacing;
        }
    }

    return targetFacing - goalFacing;
}

namespace
{
    /// <summary>A quarter turn's half, as MCX.EXE stores it (MCX.EXE @ 0x0077c2e0; a hair over pi / 4).</summary>
    constexpr double EIGHTH_TURN = 0x1.921fb5443e88cp-1;
    /// <summary>Radians to degrees (MCX.EXE @ 0x0077c278).</summary>
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;
    /// <summary>Radians to degrees, the float-rounded copy (MCX.EXE @ 0x0077dfc0).</summary>
    constexpr double RADIANS_TO_DEGREES_F = 0x1.ca5dc2p+5;

    /// <summary>The frame turned an eighth of a turn about its up axis (the facing the art is drawn at).</summary>
    frame_of_ref TurnedFrame(const frame_of_ref& frame)
    {
        const float s = static_cast<float>(std::sin(EIGHTH_TURN));
        const float c = static_cast<float>(std::cos(EIGHTH_TURN));
        frame_of_ref turned = frame;
        turned.i = frame.i * c + frame.j * s;
        turned.j = frame.j * c - frame.i * s;
        return turned;
    }
}

auto Mover::relFacingTo(vector_3d goal, int32_t) -> float
{
    const float x = position.x;
    const float y = position.y;
    const frame_of_ref turned = TurnedFrame(frame);
    vector_3d facing;
    facing.x = -turned.j.x;
    facing.y = -turned.j.y;
    facing.z = -turned.j.z;

    vector_3d toGoal;
    toGoal.x = goal.x - x;
    toGoal.y = goal.y - y;
    toGoal.z = 0.0f;
    const double length =
        std::sqrt((static_cast<double>(toGoal.x) * toGoal.x + static_cast<double>(toGoal.y) * toGoal.y) +
                  static_cast<double>(toGoal.z) * toGoal.z);

    if (length != 0.0)
    {
        toGoal.x = static_cast<float>(toGoal.x / length);
        toGoal.y = static_cast<float>(toGoal.y / length);
        toGoal.z = static_cast<float>(toGoal.z / length);
    }

    const double cosine = static_cast<double>(toGoal.z) * facing.z + static_cast<double>(toGoal.y) * facing.y +
                          static_cast<double>(toGoal.x) * facing.x;
    const float angle = static_cast<float>(acosMatherr(cosine) * RADIANS_TO_DEGREES_F);

    // Negative to the left.
    if ((facing & toGoal).z >= 0.0f)
    {
        return -angle;
    }

    return angle;
}

auto Mover::getTerrainAngle() -> float
{
    return static_cast<float>(acosMatherr(static_cast<double>(terrainNormal.z)) * RADIANS_TO_DEGREES);
}

auto Mover::getVelocityTilt() -> float
{
    const frame_of_ref turned = TurnedFrame(frame);
    const double cosine = static_cast<double>(turned.j.z) * terrainNormal.z +
                          static_cast<double>(turned.j.y) * terrainNormal.y +
                          static_cast<double>(turned.j.x) * terrainNormal.x;
    return static_cast<float>(acosMatherr(cosine) * RADIANS_TO_DEGREES);
}

auto Mover::getFireArc() -> float
{
    switch (objectClass)
    {
        case BATTLEMECH:
            return FireArc[0];
        case GROUNDVEHICLE:
            return FireArc[1];
        case ELEMENTAL:
            return FireArc[2];
        default:
            return 60.0f;
    }
}

auto Mover::destroy() -> void
{
    netName.reset();

    if (sensorSystem != nullptr)
    {
        sensorSystemManager->freeSensor(sensorSystem);
        sensorSystem = nullptr;
    }

    if (ecmTracker != nullptr)
    {
        team->removeECM(ecmTracker);
        ecmTracker = nullptr;
    }

    if (jammerTracker != nullptr)
    {
        team->removeJammer(jammerTracker);
        jammerTracker = nullptr;
    }

    debugStatus.clear();

    if (body != nullptr)
    {
        for (int32_t i = 0; i < numBodyLocations; i++)
        {
            if (bodyAt(i).criticalSpaces != nullptr)
            {
                delete[] bodyAt(i).criticalSpaces;
                bodyAt(i).criticalSpaces = nullptr;
            }
        }

        body.reset();
        numBodyLocations = 0;
    }

    if (armor != nullptr)
    {
        armor.reset();
        numArmorLocations = 0;
    }

    if (inventory != nullptr)
    {
        for (uint32_t i = numOther; i < static_cast<uint32_t>(numOther) + numWeapons; i++)
        {
            if (inventory[i].rangeRatings != nullptr)
            {
                delete[] inventory[i].rangeRatings;
                inventory[i].rangeRatings = nullptr;
            }
        }

        inventory.reset();
    }

    numAmmoTypes = 0;

    if (ammoTypeTotal != nullptr)
    {
        ammoTypeTotal.reset();
    }

    if (potentialContact != nullptr)
    {
        potentialContactManager->remove(potentialContact);
        potentialContact = nullptr;
    }

    if (appearance != nullptr)
    {
        delete appearance;
    }

    appearance = nullptr;

    if (control != nullptr)
    {
        delete control;
    }

    control = nullptr;

    if (dynamics != nullptr)
    {
        delete dynamics;
    }

    dynamics = nullptr;

    numMovers--;

    if (numMovers == 0)
    {
        if (sortList != nullptr)
        {
            sortList->destroy();
            delete sortList;
        }

        sortList = nullptr;
    }
}

auto Mover::relativePosition(float angle, float distance, uint32_t flags) -> vector_3d
{
    // The point distance meters away at angle: flag 1, an absolute angle in radians; else degrees from the
    // mover's facing. The x87 keeps some of the sums below at extended precision, done here in double.
    const float reach = -(worldUnitsPerMeter * distance);
    const float x = position.x;
    const float y = position.y;
    double offsetX;
    float offsetY;

    if ((flags & 1) != 0)
    {
        const double sine = std::sin(static_cast<double>(angle));
        const float cosine = static_cast<float>(std::cos(static_cast<double>(angle)));
        offsetX = (sine + 0.0) * reach;
        offsetY = cosine * reach;
    }
    else
    {
        frame_of_ref turned = frame;
        const double radians = (static_cast<double>(angle) + 45.0) * 0x1.1df46a2526c7ap-6;
        const float s = static_cast<float>(std::sin(radians));
        const float c = static_cast<float>(std::cos(radians));
        const vector_3d oldI = turned.i;
        turned.i = turned.i * c + turned.j * s;
        turned.j = turned.j * c - oldI * s;
        const vector_3d offset = turned.j * reach;
        offsetX = offset.x;
        offsetY = offset.y;
    }

    const double targetX = offsetX + x;
    const float targetY = static_cast<float>(static_cast<double>(offsetY) + y);

    // Flag 2 walks from the mover out to the point; otherwise from the point back to the mover.
    vector_2d start;
    vector_2d end;

    if ((flags & 2) != 0)
    {
        end.x = static_cast<float>(targetX);
        start.x = x;
        start.y = y;
        end.y = targetY;
    }
    else
    {
        start.y = targetY;
        start.x = static_cast<float>(targetX);
        end.x = x;
        end.y = y;
    }

    // Half a map cell per step.
    const double deltaX = static_cast<double>(end.x) - start.x;
    const float deltaXf = static_cast<float>(deltaX);
    const float deltaY = end.y - start.y;
    const float length =
        static_cast<float>(std::sqrt(static_cast<double>(deltaY) * deltaY + static_cast<double>(deltaXf) * deltaXf));
    double directionX = deltaX;
    float directionY = deltaY;

    if (length != 0.0)
    {
        directionX = static_cast<double>(deltaXf) / length;
        directionY = static_cast<float>(static_cast<double>(deltaY) / length);
    }

    const float stepLength = static_cast<float>(static_cast<double>(Terrain::metersPerVertex) * 0.33333334f * 0.5);
    const float stepX = static_cast<float>(directionX * stepLength);
    const double stepYExact = static_cast<double>(directionY) * stepLength;
    const float stepY = static_cast<float>(stepYExact);

    if (std::sqrt(stepYExact * stepY + static_cast<double>(stepX) * stepX) == 0.0)
    {
        vector_3d result;
        result.x = x;
        result.y = y;
        result.z = 0.0f;
        return result;
    }

    const vector_2d span = start - end;
    const float maxDistance =
        static_cast<float>(std::sqrt(static_cast<double>(span.x) * span.x + static_cast<double>(span.y) * span.y));
    float traveled = 0.0f;
    vector_2d current = start;

    // Whether the cell under current is passable.
    auto cellPassable = [&]()
    {
        vector_3d point;
        point.x = current.x;
        point.y = current.y;
        point.z = 0.0f;
        int32_t tileR;
        int32_t tileC;
        int32_t cellR;
        int32_t cellC;
        GameMap->worldToMapPos(point, tileR, tileC, cellR, cellC);

        // Port fix: the walk can leave the map, where the original reads outside it. Off the map is impassable.
        if (!GameMap->onMap(tileR, tileC))
        {
            return 0u;
        }

        return GameMap->map[GameMap->width * tileR + tileC].getCellPassable(cellR, cellC);
    };

    uint32_t passable = cellPassable();
    vector_2d previous = start;
    // Walk until the cell changes kind (or the distance runs out); the answer is the step before.
    const uint32_t keepGoingWhile = (flags & 2) != 0 ? 1u : 0u;

    if ((passable != 0) == (keepGoingWhile != 0))
    {
        while (traveled < maxDistance)
        {
            previous = current;
            current.x = stepX + current.x;
            current.y = stepY + current.y;
            const double dx = static_cast<double>(current.x) - start.x;
            const double dy = static_cast<double>(current.y) - start.y;
            traveled = static_cast<float>(std::sqrt(dx * dx + dy * dy));
            passable = cellPassable();

            if ((passable != 0) != (keepGoingWhile != 0))
            {
                break;
            }
        }
    }

    vector_3d ground;
    ground.x = previous.x;
    ground.y = previous.y;
    ground.z = 0.0f;
    vector_3d result;
    result.x = previous.x;
    result.y = previous.y;
    result.z = GameMap->getTerrainElevation(ground);
    return result;
}

auto Mover::lineOfFire(GameObject* target) -> int
{
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    GameMap->worldToMapPos(target->getPosition(), tileR, tileC, cellR, cellC);
    target->clearLineOfFire();
    const int result = GameMap->lineOfFire(position, target->getPosition());
    target->restoreLineOfFire();
    return result;
}

auto Mover::lineOfFire(vector_3d point) -> int
{
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    GameMap->worldToMapPos(point, tileR, tileC, cellR, cellC);
    return GameMap->lineOfFire(position, point);
}

auto Mover::lineOfSensor(GameObject* target, int32_t& sensorResult, int32_t& losResult) -> void
{
    // Eye to eye, ten meters up; neither blocks itself.
    vector_3d start;
    start.x = position.x;
    start.y = position.y;
    start.z = static_cast<float>(static_cast<double>(worldUnitsPerMeter) * 10.0 + position.z);
    const vector_3d targetPosition = target->getPosition();
    vector_3d end;
    end.x = targetPosition.x;
    end.y = targetPosition.y;
    end.z = static_cast<float>(static_cast<double>(worldUnitsPerMeter) * 10.0 + targetPosition.z);
    setUseMe(0);
    target->setUseMe(0);
    GameMap->lineOfSensor(start, end, sensorResult, losResult);
    setUseMe(1);
    target->setUseMe(1);
}

auto Mover::handleEvent(ObjectEvent* event) -> int32_t
{
    switch (event->type)
    {
        case 0:
        {
            // Interface events.
            switch (event->id)
            {
                case 0x1c:
                {
                    selected = 1;
                    selectionIndex = event->selectionIndex;
                    return 0;
                }
                case 0x1d:
                {
                    setSelected(0);
                    selectionIndex = -1;
                    return 0;
                }
                case 0x1e:
                case 0x1f:
                {
                    return 0;
                }
                default:
                {
                    if (event->id < 0 || event->id > 0x1b)
                    {
                        Fatal(2, " Bad ObjectEvent GUI Code ");
                    }

                    return 0;
                }
            }
        }
        case 1:
        {
            if (event->id != 6 && event->id != 7)
            {
                Fatal(0, " Bad ObjectEvent Message Code ");
            }

            return 0;
        }
        case 2:
        {
            if (event->id < 0 || event->id > 8)
            {
                Fatal(3, " Bad ObjectEvent Combat Code ");
            }

            return 0;
        }
        default:
        {
            char message[256];
            std::snprintf(message, sizeof(message), "Mover::handleEvent->Bad ObjectEvent Type (%d)", event->type);
            Fatal(1, message);
        }
    }
}

auto Mover::handleTacticalOrder(TacticalOrder tacOrder, int32_t priority, int queuePlayerOrder) -> int32_t
{
    if (queuePlayerOrder != 0)
    {
        tacOrder.pack(nullptr, nullptr);
    }

    // A client checks the order survives packing (the result isn't used).
    if (MPlayer != nullptr && MPlayer->isServer == 0)
    {
        tacOrder.pack(nullptr, nullptr);
        TacticalOrder check;
        check.init();
        check.data[0] = tacOrder.data[0];
        check.data[1] = tacOrder.data[1];
        check.unpack();
        check.destroy();
    }

    int32_t radioMessageId = -1;
    int giveOrder = 1;
    bool checkCanMove = false;

    switch (tacOrder.code)
    {
        case TACTICAL_ORDER_WAIT:
        case TACTICAL_ORDER_ESCORT:
        case TACTICAL_ORDER_FOLLOW:
        case TACTICAL_ORDER_GUARD:
        case TACTICAL_ORDER_STOP:
        case TACTICAL_ORDER_POWERUP:
        case TACTICAL_ORDER_POWERDOWN:
        case TACTICAL_ORDER_WAYPOINTS_DONE:
        case TACTICAL_ORDER_EJECT:
        case TACTICAL_ORDER_ATTACK_POINT:
        case TACTICAL_ORDER_HOLD_FIRE:
        case TACTICAL_ORDER_WITHDRAW:
        case TACTICAL_ORDER_CAPTURE:
        case TACTICAL_ORDER_REFIT:
        case TACTICAL_ORDER_GETFIXED:
        case TACTICAL_ORDER_LOAD_INTO_CARRIER:
        case TACTICAL_ORDER_DEPLOY_ELEMENTALS:
            break;
        case TACTICAL_ORDER_MOVETO_POINT:
        {
            // A group member's delayed start.
            const int32_t delay = selectionIndex;

            if (delay != -1)
            {
                tacOrder.delayedTime = static_cast<float>(delay) * DelayedOrderTime + scenarioTime;
            }

            if (isDisabled() != 0 && canMove() == 0)
            {
                radioMessageId = 0x1f;
                giveOrder = 0;
            }
            break;
        }

        case TACTICAL_ORDER_JUMPTO_POINT:
        case TACTICAL_ORDER_JUMPTO_OBJECT:
        {
            // Only mechs jump, not onto their own side, within range, onto an open cell.
            int canJumpThere = objectClass == BATTLEMECH ? 1 : 0;
            GameObject* target = tacOrder.target;

            if (target != nullptr && IsMover(target) && target->getTeam() == getTeam())
            {
                canJumpThere = 0;
            }

            const float jumpRange = getJumpRange(nullptr, nullptr);
            vector_3d jumpGoal = tacOrder.getWayPoint(0);

            if (jumpRange < distanceFrom(jumpGoal))
            {
                canJumpThere = 0;
            }

            bool cellOpen = true;

            if (objectClass == BATTLEMECH)
            {
                int32_t tileR;
                int32_t tileC;
                int32_t cellR;
                int32_t cellC;
                GameMap->worldToMapPos(tacOrder.getWayPoint(0), tileR, tileC, cellR, cellC);
                // Port fix: the player's jump point can be off the map, where the original reads outside it.
                cellOpen = GameMap->onMap(tileR, tileC) &&
                           GameMap->map[GameMap->width * tileR + tileC].getCellPassable(cellR, cellC) != 0;
            }

            if (!cellOpen || canJumpThere == 0)
            {
                radioMessageId = 0x1b;
                giveOrder = 0;
            }

            checkCanMove = true;
            break;
        }

        case TACTICAL_ORDER_MOVETO_OBJECT:
        case TACTICAL_ORDER_TRAVERSE_PATH:
        case TACTICAL_ORDER_PATROL_PATH:
            checkCanMove = true;
            break;
        case TACTICAL_ORDER_ATTACK_OBJECT:
        {
            // An attack by jumping (method 1) becomes a jump onto the target.
            if (tacOrder.attackParams.method == 1)
            {
                tacOrder.code = TACTICAL_ORDER_JUMPTO_OBJECT;
                tacOrder.moveParams.wait = 0;
                tacOrder.moveParams.wayPath.mode[0] = 0;

                if (tacOrder.target != nullptr)
                {
                    tacOrder.setWayPoint(0, tacOrder.target->getPosition());
                }
            }
            break;
        }
        default:
        {
            char message[256];
            std::snprintf(message, sizeof(message), "Mover::handleTacticalOrder->Bad TacOrder Code (%d)",
                          static_cast<int>(tacOrder.code));
            Assert(0, 1, message);
            tacOrder.destroy();
            return 1;
        }
    }

    if (checkCanMove && isDisabled() != 0 && canMove() == 0)
    {
        radioMessageId = 0x1f;
        giveOrder = 0;
    }

    MechWarrior* vehiclePilot = pilot;

    if (vehiclePilot != nullptr)
    {
        vehiclePilot->radioMessage(radioMessageId, 1);
    }

    if (MPlayer != nullptr)
    {
        tacOrder.setId(vehiclePilot);
    }

    if (giveOrder != 0)
    {
        switch (tacOrder.origin)
        {
            case 0:
            {
                if (queuePlayerOrder != 0)
                {
                    vehiclePilot->addQueuedTacOrder(tacOrder);
                    vehiclePilot->tacOrderQueueExecuting = 1;
                    tacOrder.destroy();
                    return 0;
                }

                vehiclePilot->setPlayerTacOrder(tacOrder, 0);
                break;
            }
            case 1:
            {
                vehiclePilot->setGeneralTacOrder(tacOrder);
                tacOrder.destroy();
                return 0;
            }
            case 2:
            {
                vehiclePilot->setAlarmTacOrder(tacOrder, priority);
                tacOrder.destroy();
                return 0;
            }
            default:
                break;
        }
    }

    tacOrder.destroy();
    return 0;
}

auto Mover::reduceAntiMissileAmmo(int32_t numShots) -> void
{
    if (numShots > 0)
    {
        reduceAmmo(MasterComponentList[inventory[antiMissileSystem[0]].masterID].ammoMasterId, numShots);
    }
}

auto Mover::fireAntiMissileSystem(int32_t numMissiles, int32_t& antiMissileShots) -> int32_t
{
    for (int32_t i = 0; i < numAntiMissileSystems; i++)
    {
        const InventoryItem& system = inventory[antiMissileSystem[i]];

        if (numMissiles <= 0 || ammoTypeTotal[system.ammoIndex].curAmount <= 0)
        {
            continue;
        }

        // Each volley stops one to six missiles.
        const int32_t clan = system.masterID == MasterClanAntiMissileSystemID ? 1 : 0;

        for (int32_t volley = 0; volley < AntiMissileSystemStats[clan][0]; volley++)
        {
            numMissiles += -1 - RandomNumber(6);
        }

        antiMissileShots = (RandomNumber(6) + 1) * AntiMissileSystemStats[clan][1];
    }

    if (numMissiles < 0)
    {
        numMissiles = 0;
    }

    return numMissiles;
}

auto Mover::pilotingCheck(uint32_t, float) -> void
{
    pilotingCheckPending = 0;
}

auto Mover::updateDamageTakenRate() -> void
{
    if (!(damageRateCheckTime < scenarioTime))
    {
        return;
    }

    const int32_t damageRate = static_cast<int32_t>(static_cast<double>(damageRateTally) / DamageRateFrequency);

    if (damageRate > 10)
    {
        pilot->triggerAlarm(2, static_cast<uint32_t>(damageRate));
    }

    damageRateTally = 0.0f;
    damageRateCheckTime = DamageRateFrequency + damageRateCheckTime;
}

auto Mover::setTeam(Team* newTeam) -> int32_t
{
    team = newTeam;
    setAlignment(newTeam->alignment);

    if (sensorSystem != nullptr)
    {
        sensorSystem->setTeam(team);
        sensorSystem->scanFrequency = ContactUpdateFrequency;
    }

    if (team != nullptr)
    {
        if (ecm != 0xff)
        {
            ecmTracker = team->addECM(this, inventory[ecm].masterID);
        }

        if (jammer != 0xff)
        {
            jammerTracker = team->addJammer(this, inventory[jammer].masterID);
        }
    }

    if (pilot != nullptr)
    {
        pilot->setTeam(newTeam);
    }

    return 0;
}

auto Mover::setGroup(MoverGroup* newGroup) -> int32_t
{
    group = newGroup;

    if (newGroup != nullptr && pilot != nullptr)
    {
        pilot->clearCurTacOrder(0, 0);
        pilot->orderState = ORDERSTATE_GENERAL;
    }

    return 0;
}

auto Mover::setPilot(MechWarrior* newPilot) -> void
{
    pilot = newPilot;

    if (sensorSystem != nullptr)
    {
        sensorSystem->setRange(sensorSystem->range);
    }

    newPilot->alignment = static_cast<int8_t>(alignment);
    newPilot->setVehicle(this);
}

auto Mover::getPoint() -> Mover*
{
    if (group != nullptr)
    {
        return group->getPoint();
    }

    return nullptr;
}

auto Mover::clearWeaponFireChunks(int32_t which) -> int32_t
{
    const int32_t numChunks = numWeaponFireChunks[which];
    numWeaponFireChunks[which] = 0;
    return numChunks;
}

auto Mover::addWeaponFireChunk(int32_t which, WeaponFireChunk* chunk) -> int32_t
{
    if (numWeaponFireChunks[which] == MAX_WEAPONFIRE_CHUNKS)
    {
        Fatal(0, " Mover::addWeaponFireChunk--Too many weaponfire chunks ");
    }

    chunk->pack();
    weaponFireChunks[which][numWeaponFireChunks[which]] = chunk->data;
    numWeaponFireChunks[which]++;
    return numWeaponFireChunks[which];
}

auto Mover::addWeaponFireChunks(int32_t which, uint32_t* packedChunkBuffer, int32_t numChunks) -> int32_t
{
    if (numWeaponFireChunks[which] + numChunks > MAX_WEAPONFIRE_CHUNKS - 1)
    {
        Fatal(0, " Mover::addWeaponFireChunks--Too many weaponfire chunks ");
    }

    for (int32_t i = 0; i < numChunks; i++)
    {
        weaponFireChunks[which][numWeaponFireChunks[which]] = packedChunkBuffer[i];
        numWeaponFireChunks[which]++;
        // Unpacked into a scratch chunk (the result isn't kept).
        WeaponFireChunk chunk;
        chunk.init();
        chunk.data = packedChunkBuffer[i];
        chunk.unpack(this);
    }

    return numWeaponFireChunks[which];
}

auto Mover::grabWeaponFireChunks(int32_t which, uint32_t* packedChunkBuffer, int32_t maxChunks) -> int32_t
{
    const int32_t numChunks = numWeaponFireChunks[which];
    const int32_t numGrabbed = maxChunks < numChunks ? maxChunks : numChunks;

    for (int32_t i = 0; i < numGrabbed; i++)
    {
        packedChunkBuffer[i] = weaponFireChunks[which][i];
    }

    numWeaponFireChunks[which] = numChunks - numGrabbed;
    return numGrabbed;
}

auto Mover::updateWeaponFireChunks(int32_t which) -> int32_t
{
    // Replays the weapon fire the network sent: each chunk's shot, on its target.
    for (int32_t i = 0; i < numWeaponFireChunks[which]; i++)
    {
        WeaponFireChunk chunk = {0, 0, 0, 0, 0, {0, 0}, 0, 0, 0, 0, 0, 0, -1, 0};
        chunk.data = weaponFireChunks[which][i];
        chunk.unpack(this);
        CurMoverWeaponFireChunk = chunk;

        const int32_t weaponIndex = numOther + chunk.weaponIndex;

        if (isWeaponIndex(weaponIndex) == 0)
        {
            continue;
        }

        TargetRolo = chunk.targetType;

        switch (chunk.targetType)
        {
            case 0:
            case 1:
            case 2:
            {
                BaseObject* target = nullptr;
                const char* missing = nullptr;

                if (chunk.targetType == 0)
                {
                    target = MPlayer->moverRoster[chunk.targetId];
                    missing = " Mover.updateWeaponFireChunks: NULL Mover Target (save wfchunk.dbg file) ";
                }
                else
                {
                    target = objectList->findObjectFromPart(chunk.targetId);
                    missing = chunk.targetType == 1
                                  ? " Mover.updateWeaponFireChunks: NULL Terrain Target (save wfchunk.dbg file) "
                                  : " Mover.updateWeaponFireChunks: NULL Special Target (save wfchunk.dbg file) ";
                }

                if (target == nullptr)
                {
                    DebugWeaponFireChunk(&chunk, nullptr, this);
                    Assert(0, 0, missing);
                }

                handleWeaponFire(weaponIndex, static_cast<GameObject*>(target), nullptr, chunk.hit,
                                 EntryAngleTable[chunk.entryAngle], chunk.numMissiles, chunk.numMissilesPastAMS,
                                 chunk.numAntiMissileShots, chunk.hitLocation);
                break;
            }

            case 3:
            {
                // A point on the ground: the middle of the target cell.
                const float halfSide = worldUnitsMapSide * 0.5f;
                vector_3d point;
                point.x =
                    static_cast<float>((chunk.targetCell[1] + 0.5f) * static_cast<double>(MetersPerCell) - halfSide);
                point.y = static_cast<float>(
                    (static_cast<double>(halfSide) - chunk.targetCell[0] * static_cast<double>(MetersPerCell)) -
                    static_cast<double>(MetersPerCell) * 0.5f);
                point.z = 0.0f;
                point.z = GameMap->getTerrainElevation(point);
                handleWeaponFire(weaponIndex, nullptr, &point, chunk.hit, 0.0f, chunk.numMissiles,
                                 chunk.numMissilesPastAMS, 0, 0);
                break;
            }

            default:
                Fatal(0, " Mover.updateWeaponFireChunks: bad targetType ");
        }
    }

    numWeaponFireChunks[which] = 0;
    return 0;
}

auto Mover::clearCriticalHitChunks(int32_t which) -> int32_t
{
    const int32_t numChunks = numCriticalHitChunks[which];
    numCriticalHitChunks[which] = 0;
    return numChunks;
}

auto Mover::addCriticalHitChunk(int32_t which, int32_t bodyLocation, int32_t criticalSpace) -> int32_t
{
    if (numCriticalHitChunks[which] == MAX_WEAPONFIRE_CHUNKS)
    {
        Fatal(0, " Mover::addCriticalHitChunk--Too many criticalhit chunks ");
    }

    criticalHitChunks[which][numCriticalHitChunks[which]] = static_cast<uint8_t>(bodyLocation * 16 + criticalSpace);
    numCriticalHitChunks[which]++;
    return numCriticalHitChunks[which];
}

auto Mover::addCriticalHitChunks(int32_t which, uint8_t* packedChunkBuffer, int32_t numChunks) -> int32_t
{
    if (numCriticalHitChunks[which] + numChunks > MAX_WEAPONFIRE_CHUNKS - 1)
    {
        Fatal(0, " Mover::addCriticalHitChunks--Too many criticalhit chunks ");
    }

    std::memcpy(&criticalHitChunks[which][numCriticalHitChunks[which]], packedChunkBuffer,
                static_cast<size_t>(numChunks));
    numCriticalHitChunks[which] += numChunks;
    return numCriticalHitChunks[which];
}

auto Mover::grabCriticalHitChunks(int32_t which, uint8_t* packedChunkBuffer) -> int32_t
{
    const int32_t numChunks = numCriticalHitChunks[which];

    if (numChunks > 0)
    {
        std::memcpy(packedChunkBuffer, criticalHitChunks[which], static_cast<size_t>(numChunks));
    }

    return numChunks;
}

auto Mover::updateCriticalHitChunks(int32_t which) -> int32_t
{
    numCriticalHitChunks[which] = 0;
    return 0;
}

auto Mover::clearRadioChunks(int32_t which) -> int32_t
{
    const int32_t numChunks = numRadioChunks[which];
    numRadioChunks[which] = 0;
    return numChunks;
}

auto Mover::addRadioChunk(int32_t which, uint8_t msg) -> int32_t
{
    if (numRadioChunks[which] == MAX_RADIO_CHUNKS)
    {
        return MAX_RADIO_CHUNKS;
    }

    radioChunks[which][numRadioChunks[which]] = msg;
    numRadioChunks[which]++;
    return numRadioChunks[which];
}

auto Mover::addRadioChunks(int32_t which, uint8_t* packedChunkBuffer, int32_t numChunks) -> int32_t
{
    for (int32_t i = 0; i < numChunks; i++)
    {
        addRadioChunk(which, packedChunkBuffer[i]);
    }

    return numRadioChunks[which];
}

auto Mover::grabRadioChunks(int32_t which, uint8_t* packedChunkBuffer) -> int32_t
{
    const int32_t numChunks = numRadioChunks[which];

    if (numChunks > 0)
    {
        std::memcpy(packedChunkBuffer, radioChunks[which], static_cast<size_t>(numChunks));
    }

    return numChunks;
}

auto Mover::updateRadioChunks(int32_t which) -> int32_t
{
    if (netPlayerId >= 0)
    {
        for (int32_t i = 0; i < numRadioChunks[which]; i++)
        {
            playMessage(static_cast<RadioMessageType>(radioChunks[which][i]), 0);
        }
    }

    numRadioChunks[which] = 0;
    return 0;
}

auto Mover::playMessage(RadioMessageType messageId, int propogateIfMultiplayer) -> void
{
    if (pilot != nullptr)
    {
        pilot->radioMessage(messageId, propogateIfMultiplayer);
    }
}

namespace
{
    /// <summary>Whether <paramref name="bits"/> shows any corner of the tile the object stands on.</summary>
    int TileVisible(ByteFlag* bits, const _ObjectPosition* objPosition)
    {
        const uint32_t row = static_cast<uint32_t>(objPosition->tileR);
        const uint32_t col = static_cast<uint32_t>(objPosition->tileC);

        if (bits->getFlag(row, col) != 0)
        {
            return 1;
        }

        if (bits->getFlag(row + 1, col) != 0)
        {
            return 1;
        }

        if (bits->getFlag(row + 1, col + 1) != 0)
        {
            return 1;
        }

        return bits->getFlag(row, col + 1) != 0 ? 1 : 0;
    }
}

auto Mover::isRevealed() -> int
{
    // The home side's visibility bits (the names are the original's, swapped).
    ByteFlag* bits = homeTeam->alignment != -1 ? Terrain::terrainVisibleBits : Terrain::ClanVisibleBits;
    return TileVisible(bits, objPosition);
}

auto Mover::enemyRevealed() -> int
{
    ByteFlag* bits = homeTeam->alignment != -1 ? Terrain::ClanVisibleBits : Terrain::terrainVisibleBits;
    return TileVisible(bits, objPosition);
}

auto Mover::getDamageClass(int32_t& damageClass, int& shutDown) -> void
{
    const double quotient = static_cast<double>(curCV) / maxCV;
    const float health = static_cast<float>(quotient);

    if (quotient > 0.9)
    {
        damageClass = 0;
    }
    else if (health > 0.75)
    {
        damageClass = 1;
    }
    else if (health > 0.5)
    {
        damageClass = 2;
    }
    else if (health > 0.1)
    {
        damageClass = 3;
    }
    else
    {
        damageClass = 4;
    }

    shutDown = status == 5 ? 1 : 0;
}

auto Mover::getInventoryDamage(int32_t itemIndex) -> int32_t
{
    if (itemIndex >= numAmmos + numWeapons + numOther)
    {
        return 0;
    }

    const InventoryItem& item = inventory[itemIndex];
    return static_cast<int8_t>(MasterComponentList[item.masterID].health) - item.health;
}

auto Mover::getEcmEffect() -> float
{
    if (ecm != 0xff && inventory[ecm].disabled == 0)
    {
        return MasterComponentList[inventory[ecm].masterID].damage;
    }

    return 0.0f;
}

auto Mover::getProbeEffect() -> float
{
    if (probe != 0xff && inventory[probe].disabled == 0)
    {
        return MasterComponentList[inventory[probe].masterID].rangeOrHeat;
    }

    return 0.0f;
}

auto Mover::getVisualRange() -> float
{
    return getProbeEffect() + MaxVisualRadius;
}

auto Mover::calcOffsetMoveGoal(vector_3d target, vector_3d offset, vector_3d& goal) -> int32_t
{
    // Half a map cell per step, from the offset point toward the target.
    float directionX = target.x - offset.x;
    float directionY = target.y - offset.y;
    const float length = static_cast<float>(
        std::sqrt(static_cast<double>(directionX) * directionX + static_cast<double>(directionY) * directionY));

    if (length != 0.0f)
    {
        directionX = directionX / length;
        directionY = directionY / length;
    }

    const float stepX =
        static_cast<float>(static_cast<double>(directionX) * Terrain::metersPerVertexDivMAPCELL_DIM * 0.5);
    const float stepY =
        static_cast<float>(static_cast<double>(directionY) * Terrain::metersPerVertexDivMAPCELL_DIM * 0.5);

    if (std::sqrt(static_cast<double>(stepX) * stepX + static_cast<double>(stepY) * stepY) == 0.0)
    {
        goal = target;
        return 0;
    }

    vector_3d away = offset - target;
    const float maxDistance = static_cast<float>(away.magnitude());
    float x = offset.x;
    float y = offset.y;

    // Whether the cell under (x, y) is passable.
    auto cellPassable = [&]()
    {
        vector_3d point;
        point.x = x;
        point.y = y;
        point.z = 0.0f;
        int32_t tileR;
        int32_t tileC;
        int32_t cellR;
        int32_t cellC;
        GameMap->worldToMapPos(point, tileR, tileC, cellR, cellC);

        // Port fix: the walk can leave the map, where the original reads outside it. Off the map is impassable.
        if (!GameMap->onMap(tileR, tileC))
        {
            return 0u;
        }

        return GameMap->map[GameMap->width * tileR + tileC].getCellPassable(cellR, cellC);
    };

    // Off a blocked cell: step on until the point before was open (so one step past the first open cell).
    if (cellPassable() == 0)
    {
        float traveled = 0.0f;
        uint32_t lastPassable;

        do
        {
            if (maxDistance <= traveled)
            {
                break;
            }

            lastPassable = cellPassable();
            x = stepX + x;
            y = stepY + y;
            const double dx = static_cast<double>(x) - target.x;
            const double dy = static_cast<double>(y) - target.y;
            traveled = static_cast<float>(std::sqrt(dx * dx + dy * dy));
        } while (lastPassable == 0);
    }

    vector_3d ground;
    ground.x = x;
    ground.y = y;
    ground.z = 0.0f;
    goal.x = x;
    goal.y = y;
    goal.z = GameMap->getTerrainElevation(ground);
    return 0;
}

auto Mover::setChallenger(GameObject* newChallenger) -> void
{
    challenger = newChallenger;
}

auto Mover::getChallenger() -> GameObject*
{
    GameObject* current = challenger;

    if (current != nullptr && current->isDisabled() != 0)
    {
        challenger = nullptr;
        return nullptr;
    }

    return current;
}

auto Mover::calcMoveGoal(GameObject* target, vector_3d moveGoal, int32_t isGroup, int32_t offsetIndex,
                         int32_t groupSize, int32_t pointIndex, vector_3d& newGoal, uint32_t params) -> int32_t
{
    // 0x800: no fire range ring and no bonus around the goal itself.
    const uint32_t noRangeRing = (params >> 11) & 1;

    // 0x20: go straight to the goal.
    if ((params & 0x20) != 0)
    {
        newGoal = moveGoal;
        return 0;
    }

    int32_t* goal = &goalMap[0][0];
    std::memset(goalMap, 0, sizeof(goalMap));

    // 0x400: one and a half vertices toward the goal.
    if ((params & 0x400) != 0)
    {
        const float dx = moveGoal.x - position.x;
        const float dy = moveGoal.y - position.y;
        const double length = std::sqrt(static_cast<double>(dy) * dy + static_cast<double>(dx) * dx);

        if (length <= 0.0)
        {
            return 0;
        }

        const float lengthF = static_cast<float>(length);
        const double stepLength = static_cast<double>(Terrain::metersPerVertex) * 1.5;
        vector_3d step;
        step.x = static_cast<float>(static_cast<double>(dx / lengthF) * stepLength);
        step.y = static_cast<float>(stepLength * (dy / lengthF));
        step.z = 0.0f;
        // The original takes the elevation of the step itself, not of the point it reaches.
        const float elevation = GameMap->getTerrainElevation(step);
        vector_3d stepGoal;
        stepGoal.x = step.x + position.x;
        stepGoal.y = step.y + position.y;
        stepGoal.z = elevation + position.z;
        calcOffsetMoveGoal(position, stepGoal, newGoal);
        return 0;
    }

    // The map covers the 13x13 tiles around the goal's tile.
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    GameMap->worldToMapPos(moveGoal, tileR, tileC, cellR, cellC);
    const int32_t goalCellR = tileR * MAPCELL_DIM + cellR;
    const int32_t goalCellC = tileC * MAPCELL_DIM + cellC;
    const int32_t mapTileR0 = tileR - 6;
    const int32_t mapTileC0 = tileC - 6;
    const int32_t mapCellR0 = mapTileR0 * MAPCELL_DIM;
    const int32_t mapCellC0 = mapTileC0 * MAPCELL_DIM;

    if (target == nullptr)
    {
        calcOffsetMoveGoal(position, moveGoal, newGoal);
        return 0;
    }

    const ObjectClass targetClass = target->objectClass;

    if (targetClass == BATTLEMECH || targetClass == GROUNDVEHICLE || targetClass == ELEMENTAL || targetClass == MOVER)
    {
        // The facing is computed and dropped.
        vector_3d targetPosition = target->getPosition();
        targetPosition.y = static_cast<float>(targetPosition.y + 50.0);
        target->relFacingTo(targetPosition, -1);
    }

    const int32_t* overlayWeights = &OverlayWeightTable[overlayWeightClass * OVERLAY_WEIGHT_LEVEL_SIZE];

    // Adds amount to the square of cells within radius of the goal.
    auto addSquare = [&](int32_t radius, int32_t amount)
    {
        const int32_t rowEnd = (radius - mapCellR0) + 1 + goalCellR;
        const int32_t colStart = (goalCellC - radius) - mapCellC0;
        const int32_t colEnd = (radius - mapCellC0) + 1 + goalCellC;

        for (int32_t row = (goalCellR - radius) - mapCellR0; row < rowEnd; row++)
        {
            const int32_t rowIndex = row * GOALMAP_CELL_DIM;

            for (int32_t col = colStart; col < colEnd; col++)
            {
                if (rowIndex > -1 && rowIndex < GOALMAP_CELL_DIM * GOALMAP_CELL_DIM && col > -1 &&
                    col < GOALMAP_CELL_DIM)
                {
                    goal[rowIndex + col] += amount;
                }
            }
        }
    };

    const int32_t orderCode = pilot->curTacOrder.code;
    const bool attacking = orderCode == TACTICAL_ORDER_ATTACK_OBJECT || orderCode == TACTICAL_ORDER_GUARD;
    int32_t attackRange = -5;

    if (attacking)
    {
        attackRange = pilot->curTacOrder.attackParams.range;
    }

    int32_t ringRange = 2;

    if (attacking)
    {
        const float cellMeters = metersPerWorldUnit * Terrain::metersPerVertexDivMAPCELL_DIM;

        // Within the longest fire range (less 3 cells) is good.
        if (noRangeRing == 0 && attackRange != 0 && attackRange != 1 && attackRange != 2)
        {
            int32_t radius = static_cast<int32_t>(static_cast<double>(getFireRange(-2)) / cellMeters) - 3;

            if (radius < 1)
            {
                radius = 1;
            }
            else if (radius > 19)
            {
                radius = 19;
            }

            addSquare(radius, 25);
        }

        ringRange = 0;
        const float orderFireRange = pilot->orderFireRange;

        if (orderFireRange > 0.0f)
        {
            // An ordered fire range: the ring at that range, and nothing within the weapons' minimum range.
            ringRange = static_cast<int32_t>(static_cast<double>(orderFireRange) / cellMeters);

            if (noRangeRing == 0)
            {
                ringRange -= 3;
            }

            if (ringRange < 1)
            {
                ringRange = 1;
            }
            else if (ringRange > 19)
            {
                ringRange = 19;
            }

            int32_t radius = static_cast<int32_t>(static_cast<double>(maxMinRange) / cellMeters + 1.0f);

            if (radius > 19)
            {
                radius = 19;
            }

            addSquare(radius, -525);
        }
        else if (orderFireRange == -1.0f)
        {
            ringRange = 2;
        }
    }

    // A diamond of cells ringRange from the goal.
    int32_t ringRow = (goalCellR - ringRange) - mapCellR0;
    int32_t ringCol = goalCellC - mapCellC0;

    for (int32_t side = 0; side < 4; side++)
    {
        for (int32_t count = 0; count < ringRange; count++)
        {
            ringRow += GoalRingStep[side][0];
            ringCol += GoalRingStep[side][1];

            if (ringRow > -1 && ringRow < GOALMAP_CELL_DIM && ringCol > -1 && ringCol < GOALMAP_CELL_DIM)
            {
                goalMap[ringRow][ringCol] += 500;
            }
        }
    }

    // Where the mover stands, clamped to the map.
    int32_t myTileR;
    int32_t myTileC;
    int32_t myCellR;
    int32_t myCellC;
    GameMap->worldToMapPos(position, myTileR, myTileC, myCellR, myCellC);
    int32_t myRow = myCellR + (myTileR - mapTileR0) * MAPCELL_DIM;
    int32_t myCol = myCellC + (myTileC - mapTileC0) * MAPCELL_DIM;

    if (myRow < 0)
    {
        myRow = 0;
    }
    else if (myRow >= GOALMAP_CELL_DIM)
    {
        myRow = GOALMAP_CELL_DIM - 1;
    }

    if (myCol < 0)
    {
        myCol = 0;
    }
    else if (myCol >= GOALMAP_CELL_DIM)
    {
        myCol = GOALMAP_CELL_DIM - 1;
    }

    // The 3x3 cells of the goal.
    if (noRangeRing == 0)
    {
        for (int32_t row = -1; row < 2; row++)
        {
            for (int32_t col = -1; col < 2; col++)
            {
                goal[(goalCellR - mapCellR0 + row) * GOALMAP_CELL_DIM + (goalCellC - mapCellC0) + col] += 100;
            }
        }
    }

    // 0x8: somewhere other than where the mover stands.
    if ((params & 0x8) != 0)
    {
        goalMap[myRow][myCol] -= 10000;
    }

    // Farther from the mover is worse.
    for (int32_t row = 0; row < GOALMAP_CELL_DIM; row++)
    {
        for (int32_t col = 0; col < GOALMAP_CELL_DIM; col++)
        {
            const int32_t rowDistance = row > myRow ? row - myRow : myRow - row;
            const int32_t colDistance = col > myCol ? col - myCol : myCol - col;
            goalMap[row][col] -= rowDistance + colDistance;
        }
    }

    // The cells the group mates are heading for.
    Mover* movers[MAX_MOVERGROUP_COUNT];

    if (group != nullptr)
    {
        const int32_t numMovers = group->getMovers(movers);

        for (int32_t i = 0; i < numMovers; i++)
        {
            if (movers[i] == this)
            {
                continue;
            }

            MechWarrior* matePilot = movers[i]->getPilot();

            if (matePilot == nullptr || matePilot->moveOrders.pathType == 0)
            {
                continue;
            }

            int32_t mateCellR;
            int32_t mateCellC;
            worldCoordToMapCell(matePilot->moveOrders.originalGlobalGoal[1], mateCellR, mateCellC);
            mateCellR -= mapCellR0;
            mateCellC -= mapCellC0;

            if (mateCellR > -1 && mateCellR < GOALMAP_CELL_DIM && mateCellC > -1 && mateCellC < GOALMAP_CELL_DIM)
            {
                goalMap[mateCellR][mateCellC] -= 100;
            }
        }
    }

    // Off the map and impassable cells are out; overlays cost their weight.
    for (int32_t tileRow = 0; tileRow * MAPCELL_DIM < GOALMAP_CELL_DIM; tileRow++)
    {
        const int32_t mapR = tileRow + mapTileR0;
        int32_t mapC = mapTileC0;

        for (int32_t cellCol = 0; cellCol < GOALMAP_CELL_DIM; cellCol += MAPCELL_DIM, mapC++)
        {
            int32_t* block = &goalMap[tileRow * MAPCELL_DIM][cellCol];

            if (mapR <= -1 || mapR >= GameMap->height || mapC <= -1 || mapC >= GameMap->width)
            {
                for (int32_t row = 0; row < MAPCELL_DIM; row++)
                {
                    for (int32_t col = 0; col < MAPCELL_DIM; col++)
                    {
                        block[row * GOALMAP_CELL_DIM + col] -= 10000;
                    }
                }

                continue;
            }

            Assert(mapR < GameMap->height && mapC < GameMap->width, 0, " Map Tile out of bounds ");
            MapTile tile = GameMap->map[GameMap->width * mapR + mapC];

            for (int32_t row = 0; row < MAPCELL_DIM; row++)
            {
                for (int32_t col = 0; col < MAPCELL_DIM; col++)
                {
                    if (tile.getCellPassable(row, col) == 0)
                    {
                        block[row * GOALMAP_CELL_DIM + col] -= 10000;
                    }
                }
            }

            const uint32_t overlayType = tile.overlay & 0x7f;

            if (overlayType != 0)
            {
                const int32_t* weight = overlayWeights + OverlayWeightIndex[overlayType];

                for (int32_t row = 0; row < MAPCELL_DIM; row++)
                {
                    for (int32_t col = 0; col < MAPCELL_DIM; col++)
                    {
                        block[row * GOALMAP_CELL_DIM + col] -= *weight++;
                    }
                }
            }
        }
    }

    // The 20 best cells, best first. A cell better than only the last goes in last.
    struct GoalCandidate
    {
        int32_t row = 0;
        int32_t col = 0;
        int32_t value = 0;
    };

    GoalCandidate best[20];

    for (GoalCandidate& candidate : best)
    {
        candidate = {-1, -1, -999999};
    }

    for (int32_t index = 0; index < GOALMAP_CELL_DIM * GOALMAP_CELL_DIM; index++)
    {
        const int32_t value = goal[index];

        if (index >= 20 && value <= best[19].value)
        {
            continue;
        }

        int32_t slot = 18;

        while (slot > -1 && value >= best[slot].value)
        {
            slot--;
        }

        if (slot < 18)
        {
            std::memmove(&best[slot + 2], &best[slot + 1], (18 - slot) * sizeof(GoalCandidate));
        }

        best[slot + 1] = {index / GOALMAP_CELL_DIM, index % GOALMAP_CELL_DIM, value};
    }

    // An elemental asks its group mates ahead of it for their last targets, and drops the answers.
    if (objectClass == ELEMENTAL && group != nullptr)
    {
        const int32_t numMovers = group->getMovers(movers);

        for (int32_t i = 0; i < numMovers; i++)
        {
            if (movers[i] == this)
            {
                break;
            }

            MechWarrior* matePilot = movers[i]->getPilot();

            if (matePilot != nullptr)
            {
                matePilot->getLastTarget();
            }
        }
    }

    // The best cell with a line of fire to the goal (else the 20th).
    const double halfMapSide = static_cast<double>(worldUnitsMapSide) * 0.5f;
    int32_t goalRow = mapTileR0;
    int32_t goalCol = mapTileC0;
    target->clearLineOfFire();

    for (int32_t i = 0; i < 20; i++)
    {
        goalCol = best[i].col;
        goalRow = best[i].row;
        vector_3d cellCenter;
        cellCenter.x =
            static_cast<float>((static_cast<double>(goalCol + mapCellC0) + 0.5f) * MetersPerCell - halfMapSide);
        cellCenter.y = static_cast<float>((halfMapSide - static_cast<double>(goalRow + mapCellR0) * MetersPerCell) -
                                          static_cast<double>(MetersPerCell) * 0.5f);
        cellCenter.z = 0.0f;

        if (GameMap->lineOfFire(cellCenter, moveGoal) != 0)
        {
            break;
        }
    }

    goalRow += mapCellR0;
    goalCol += mapCellC0;
    target->restoreLineOfFire();

    newGoal.x = static_cast<float>((static_cast<double>(goalCol) + 0.5) * MetersPerCell - halfMapSide);
    newGoal.y = static_cast<float>(halfMapSide - (static_cast<double>(goalRow) + 0.5) * MetersPerCell);
    newGoal.z = land->getTerrainElevation(newGoal);
    calcOffsetMoveGoal(position, newGoal, newGoal);
    return 0;
}

auto Mover::calcMovePath(MovePath* path, int32_t pathType, vector_3d start, vector_3d goal, int32_t* goalCell,
                         uint32_t params) -> int32_t
{
    if (PathFindMap == nullptr)
    {
        Fatal(0, " No PathFindMap in Mover::calcMovePath ");
    }

    int32_t startTileR;
    int32_t startTileC;
    int32_t startCellR;
    int32_t startCellC;
    GameMap->worldToMapPos(start, startTileR, startTileC, startCellR, startCellC);
    int32_t goalTileR;
    int32_t goalTileC;
    int32_t goalCellR;
    int32_t goalCellC;
    GameMap->worldToMapPos(goal, goalTileR, goalTileC, goalCellR, goalCellC);
    path->clear();

    int32_t numOffsets;
    int32_t jumpCost;
    int32_t* overlayWeights = &OverlayWeightTable[overlayWeightClass * OVERLAY_WEIGHT_LEVEL_SIZE];

    if (pathType == 1)
    {
        // A simple path: the window of SimpleMovePathRange tiles around the start.
        int32_t ULr = startTileR - SimpleMovePathRange;

        if (ULr < 0)
        {
            ULr = 0;
        }

        int32_t ULc = startTileC - SimpleMovePathRange;

        if (ULc < 0)
        {
            ULc = 0;
        }

        if (maxRunSpeed == 0.0f)
        {
            return 0;
        }

        const int32_t moveLevel = LocalPathMoveLevel(maxRunSpeed);

        if (moveLevel <= 0)
        {
            return 0;
        }

        SetUpPathJumps(this, numOffsets, jumpCost);
        const int32_t dim = SimpleMovePathRange * 2 + 1;
        PathFindMap->setUp(GameMap, ULr, ULc, dim, dim, &start, (startTileR - ULr) * MAPCELL_DIM + startCellR,
                           (startTileC - ULc) * MAPCELL_DIM + startCellC, goal,
                           (goalTileR - ULr) * MAPCELL_DIM + goalCellR, (goalTileC - ULc) * MAPCELL_DIM + goalCellC,
                           overlayWeights, moveLevel, jumpCost, numOffsets, params);
        DebugMovePathType = 1;
        // The caller's goalCell is left alone.
        int32_t simpleGoalCell[2];
        const int32_t result = PathFindMap->calcPath(path, nullptr, simpleGoalCell);
        JumpOnBlocked = 0;
        return result;
    }

    // Within the start's sector of the global map.
    if (maxRunSpeed == 0.0f)
    {
        return 0;
    }

    const int32_t moveLevel = static_cast<int32_t>(static_cast<double>(metersPerWorldUnit) *
                                                   Terrain::metersPerVertexDivMAPCELL_DIM / maxRunSpeed * 50.0);

    if (moveLevel <= 0)
    {
        return 0;
    }

    const int32_t sectorDim = GlobalMoveMap->sectorDim;
    const int32_t ULr = (startTileR / sectorDim) * sectorDim;
    const int32_t ULc = (startTileC / sectorDim) * sectorDim;
    SetUpPathJumps(this, numOffsets, jumpCost);
    PathFindMap->setUp(GameMap, ULr, ULc, GlobalMoveMap->sectorDim, GlobalMoveMap->sectorDim, &start,
                       (startTileR - ULr) * MAPCELL_DIM + startCellR, (startTileC - ULc) * MAPCELL_DIM + startCellC,
                       goal, (goalTileR - ULr) * MAPCELL_DIM + goalCellR, (goalTileC - ULc) * MAPCELL_DIM + goalCellC,
                       overlayWeights, moveLevel, jumpCost, numOffsets, params);
    DebugMovePathType = pathType;
    const int32_t result = PathFindMap->calcPath(path, nullptr, goalCell);
    JumpOnBlocked = 0;
    return result;
}

auto Mover::calcEscapePath(MovePath* path, vector_3d start, vector_3d goal, int32_t* goalCell, uint32_t params,
                           vector_3d& escapeGoal) -> int32_t
{
    escapeGoal.x = -999999.0f;
    escapeGoal.y = -999999.0f;
    escapeGoal.z = -999999.0f;

    if (PathFindMap == nullptr)
    {
        Fatal(0, " No PathFindMap in Mover::calcMovePath ");
    }

    int32_t startTileR;
    int32_t startTileC;
    int32_t startCellR;
    int32_t startCellC;
    GameMap->worldToMapPos(start, startTileR, startTileC, startCellR, startCellC);
    int32_t goalTileR;
    int32_t goalTileC;
    int32_t goalCellR;
    int32_t goalCellC;
    GameMap->worldToMapPos(goal, goalTileR, goalTileC, goalCellR, goalCellC);
    path->clear();

    int32_t ULr = startTileR - SimpleMovePathRange;

    if (ULr < 0)
    {
        ULr = 0;
    }

    int32_t ULc = startTileC - SimpleMovePathRange;

    if (ULc < 0)
    {
        ULc = 0;
    }

    if (maxRunSpeed == 0.0f)
    {
        return 0;
    }

    const int32_t moveLevel = LocalPathMoveLevel(maxRunSpeed);

    if (moveLevel <= 0)
    {
        return 0;
    }

    int32_t numOffsets;
    int32_t jumpCost;
    SetUpPathJumps(this, numOffsets, jumpCost);
    const int32_t dim = SimpleMovePathRange * 2 + 1;
    FindingEscapePath = 1;
    PathFindMap->setUp(GameMap, ULr, ULc, dim, dim, &start, (startTileR - ULr) * MAPCELL_DIM + startCellR,
                       (startTileC - ULc) * MAPCELL_DIM + startCellC, goal, (goalTileR - ULr) * MAPCELL_DIM + goalCellR,
                       (goalTileC - ULc) * MAPCELL_DIM + goalCellC,
                       &OverlayWeightTable[overlayWeightClass * OVERLAY_WEIGHT_LEVEL_SIZE], moveLevel, jumpCost,
                       numOffsets, params);
    DebugMovePathType = 0;
    // goalCell is unused: the escape goal cell goes to a local.
    int32_t escapeGoalCell[2];
    const int32_t result = PathFindMap->calcEscapePath(path, &escapeGoal, escapeGoalCell);
    JumpOnBlocked = 0;
    FindingEscapePath = 0;
    return result;
}

auto Mover::getAdjacentCellPathLocked(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, int32_t dir) -> int
{
    const int32_t* adjCell = adjCellTable[cellR * MAPCELL_DIM + cellC][dir];
    return GameMap->map[(adjCell[0] + tileR) * GameMap->width + adjCell[1] + tileC].getCellPathLocked(adjCell[2],
                                                                                                      adjCell[3]) != 0;
}

auto Mover::getPathLocked(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, int32_t diameter) -> int
{
    if (diameter == 1)
    {
        return GameMap->map[GameMap->width * tileR + tileC].getCellPathLocked(cellR, cellC) != 0;
    }

    if (diameter != 3)
    {
        if (diameter == 5)
        {
            return 0;
        }

        Fatal(0, " Bad PathLock Radius ");
    }

    return VisitCellsAround(tileR, tileC, cellR, cellC, [](MapTile& tile, int32_t row, int32_t col)
                            { return tile.getCellPathLocked(row, col) != 0; });
}

auto Mover::setPathLock(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, int set, int32_t diameter) -> void
{
    const uint32_t locked = set != 0 ? 1 : 0;

    if (diameter == 1)
    {
        GameMap->map[GameMap->width * tileR + tileC].setCellPathLocked(cellR, cellC, locked);
        return;
    }

    if (diameter != 3)
    {
        if (diameter != 5)
        {
            Fatal(0, " Bad PathLock Diameter ");
        }

        return;
    }

    VisitCellsAround(tileR, tileC, cellR, cellC,
                     [locked](MapTile& tile, int32_t row, int32_t col)
                     {
                         tile.setCellPathLocked(row, col, locked);
                         return false;
                     });
}

auto Mover::getPathRangeLock(int32_t range, int* reachedEnd) -> int
{
    MovePath* path = pilot->getMovePath();

    if (path != nullptr)
    {
        return path->isLocked(-1, range, reachedEnd);
    }

    return 0;
}

auto Mover::setPathRangeLock(int set, int32_t range) -> int32_t
{
    MovePath* path = pilot->getMovePath();

    if (set == 0)
    {
        for (int32_t i = 0; i < numPathRangeLocks; i++)
        {
            const int32_t* lock = pathRangeLocks[i];
            GameMap->map[lock[0] * GameMap->width + lock[1]].setCellPathLocked(lock[2], lock[3], 0);
        }

        numPathRangeLocks = 0;
        return 0;
    }

    if (numPathRangeLocks > 0)
    {
        setPathRangeLock(0, 0);
    }

    if (path == nullptr || path->numSteps <= 0)
    {
        return 0;
    }

    int32_t lastStep = path->curStep + range;

    if (path->numStepsWhenNotPaused <= lastStep)
    {
        lastStep = path->numStepsWhenNotPaused;
    }

    numPathRangeLocks = 0;

    for (int32_t step = path->curStep; step < lastStep; step++)
    {
        const PathStep& pathStep = path->stepList[step];
        MapTile& tile = GameMap->map[pathStep.tileR * GameMap->width + pathStep.tileC];

        // Someone else holds the cell: the cells locked so far stay locked.
        if (tile.getCellPathLocked(pathStep.cellR, pathStep.cellC) != 0)
        {
            return -1;
        }

        tile.setCellPathLocked(pathStep.cellR, pathStep.cellC, 1);
        pathRangeLocks[numPathRangeLocks][0] = pathStep.tileR;
        pathRangeLocks[numPathRangeLocks][1] = pathStep.tileC;
        pathRangeLocks[numPathRangeLocks][2] = pathStep.cellR;
        pathRangeLocks[numPathRangeLocks][3] = pathStep.cellC;
        numPathRangeLocks++;
    }

    return 0;
}

auto Mover::updatePathLock(int set) -> void
{
    // Not while a mech is in the air.
    if (objectClass == BATTLEMECH && static_cast<BattleMech*>(this)->inJump != 0)
    {
        return;
    }

    ObjectPosition* objectPosition = objPosition;

    if (objectPosition != nullptr)
    {
        setPathLock(objectPosition->tileR, objectPosition->tileC, objectPosition->cellR, objectPosition->cellC, set,
                    pathLockLevel);
    }

    pilot->getMovePath();

    if (set == 0 || pilot->moveOrders.yieldTime <= -1.0f)
    {
        setPathRangeLock(set, pathLockRange);
    }
}

auto Mover::getPathRangeBlocked(int32_t range, int* reachedEnd) -> int
{
    MovePath* path = pilot->getMovePath();

    if (path != nullptr)
    {
        return path->isBlocked(-1, range, reachedEnd);
    }

    return 0;
}

auto Mover::updateHustleTime() -> void
{
    const ObjectPosition* objectPosition = objPosition;

    switch (GameMap->map[objectPosition->tileR * GameMap->width + objectPosition->tileC].overlay & 0x7f)
    {
        case 0x25:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x37:
        case 0x38:
        case 0x39:
        case 0x3a:
            lastHustleTime = scenarioTime;
            break;
        default:
            break;
    }
}

auto Mover::bounceToAdjCell() -> int32_t
{
    // The first neighbour that is passable, affordable and not path locked.
    int32_t dir = 0;
    int32_t adjTileR;
    int32_t adjTileC;
    int32_t adjCellR;
    int32_t adjCellC;

    while (true)
    {
        const ObjectPosition* objectPosition = objPosition;
        const int32_t* adjCell = adjCellTable[objectPosition->cellR * MAPCELL_DIM + objectPosition->cellC][dir];
        adjTileR = adjCell[0] + objectPosition->tileR;
        adjTileC = adjCell[1] + objectPosition->tileC;
        adjCellR = adjCell[2];
        adjCellC = adjCell[3];
        // The tile's words are read before the overlay weight is.
        MapTile tile = GameMap->map[GameMap->width * adjTileR + adjTileC];
        uint32_t passable = tile.getCellPassable(adjCellR, adjCellC);

        if (GameMap->getOverlayWeight(adjTileR, adjTileC, adjCellR, adjCellC, this) > 9999)
        {
            passable = 0;
        }

        if (tile.getCellPathLocked(adjCellR, adjCellC) == 0 && passable != 0)
        {
            break;
        }

        dir++;

        if (dir > 7)
        {
            return -1;
        }
    }

    const ObjectPosition* objectPosition = objPosition;
    const uint32_t wasLocked =
        GameMap->map[objectPosition->tileR * GameMap->width + objectPosition->tileC].getCellPathLocked(
            objectPosition->cellR, objectPosition->cellC);

    if (wasLocked != 0)
    {
        updatePathLock(0);
    }

    const double halfMapSide = static_cast<double>(worldUnitsMapSide) * 0.5f;
    vector_3d cellCenter;
    cellCenter.x = static_cast<float>((static_cast<double>(adjCellC + adjTileC * MAPCELL_DIM) + 0.5f) * MetersPerCell -
                                      halfMapSide);
    cellCenter.y =
        static_cast<float>((halfMapSide - static_cast<double>(adjCellR + adjTileR * MAPCELL_DIM) * MetersPerCell) -
                           static_cast<double>(MetersPerCell) * 0.5f);
    cellCenter.z = 0.0f;
    setPosition(cellCenter);
    GameObjectMap->updateObject(this, 0);
    pilot->pausePath();

    if (wasLocked != 0)
    {
        updatePathLock(1);
    }

    return dir;
}

auto Mover::calcMovePath(MovePath* path, vector_3d start, int32_t thruArea, int32_t goalDoor, vector_3d finalGoal,
                         vector_3d* goal, int32_t* goalCell, uint32_t params) -> int32_t
{
    if (PathFindMap == nullptr)
    {
        Fatal(0, " No PathFindMap in Mover::calcMovePath ");
    }

    // Within the sector of the area the path goes through.
    const GlobalMapArea& area = GlobalMoveMap->areas[thruArea];
    const int32_t ULr = area.sectorR * GlobalMoveMap->sectorDim;
    const int32_t ULc = area.sectorC * GlobalMoveMap->sectorDim;
    path->clear();

    if (maxRunSpeed == 0.0f)
    {
        return 0;
    }

    const int32_t moveLevel = static_cast<int32_t>(static_cast<double>(metersPerWorldUnit) *
                                                   Terrain::metersPerVertexDivMAPCELL_DIM / maxRunSpeed * 50.0);

    if (moveLevel <= 0)
    {
        return 0;
    }

    int32_t numOffsets;
    int32_t jumpCost;
    SetUpPathJumps(this, numOffsets, jumpCost);
    int32_t startTileR;
    int32_t startTileC;
    int32_t startCellR;
    int32_t startCellC;
    GameMap->worldToMapPos(start, startTileR, startTileC, startCellR, startCellC);
    const int32_t sectorDim = GlobalMoveMap->sectorDim;

    if (PathFindMap->setUp(GameMap, ULr, ULc, sectorDim, sectorDim, &start,
                           (startTileR - ULr) * MAPCELL_DIM + startCellR, (startTileC - ULc) * MAPCELL_DIM + startCellC,
                           thruArea, goalDoor, finalGoal,
                           &OverlayWeightTable[overlayWeightClass * OVERLAY_WEIGHT_LEVEL_SIZE], moveLevel, jumpCost,
                           numOffsets, params) == -1)
    {
        JumpOnBlocked = 0;
        return -999;
    }

    const int32_t result = PathFindMap->calcPath(path, goal, goalCell);
    JumpOnBlocked = 0;
    return result;
}

auto Mover::getContacts(int32_t* contactList, int32_t contactCriteria, int32_t sortType) -> int32_t
{
    return team->getContacts(this, contactList, contactCriteria, sortType);
}

auto Mover::weaponLocked(int32_t weaponIndex, vector_3d targetPosition) -> float
{
    return relFacingTo(targetPosition, -1);
}

auto Mover::weaponInRange(int32_t weaponIndex, float metersToTarget) -> int32_t
{
    const MasterComponent& weapon = MasterComponentList[inventory[weaponIndex].masterID];

    if (metersToTarget <= weapon.weaponRange[0])
    {
        return 0;
    }

    if (metersToTarget <= weapon.weaponRange[1])
    {
        return 2;
    }

    if (metersToTarget <= weapon.weaponRange[2])
    {
        return 3;
    }

    return weapon.weaponRange[3] < metersToTarget ? 0 : 4;
}

auto Mover::getWeaponsReady(int32_t* list, int32_t listSize) -> int32_t
{
    int32_t numReady = 0;

    if (listSize == -1)
    {
        for (int32_t i = numOther; i < numOther + numWeapons; i++)
        {
            if (isWeaponReady(i) != 0)
            {
                if (list != nullptr)
                {
                    list[numReady] = i;
                }

                numReady++;
            }
        }
    }
    else
    {
        for (int32_t i = 0; i < listSize; i++)
        {
            const int32_t weaponIndex = list[i];

            if (isWeaponReady(weaponIndex) != 0)
            {
                if (list != nullptr)
                {
                    list[numReady] = weaponIndex;
                }

                numReady++;
            }
        }
    }

    return numReady;
}

auto Mover::getWeaponsLocked(int32_t* list, int32_t listSize) -> int32_t
{
    GameObject* target = pilot->getLastTarget();

    if (target == nullptr)
    {
        return -2;
    }

    const vector_3d targetPosition = target->getPosition();
    int32_t numLocked = 0;
    const float fireArc = getFireArc();
    const float negFireArc = -fireArc;

    if (listSize == -1)
    {
        for (int32_t i = numOther; i < numOther + numWeapons; i++)
        {
            const float facing = weaponLocked(i, targetPosition);

            if (negFireArc <= facing && facing <= fireArc)
            {
                list[numLocked] = i;
                numLocked++;
            }
        }
    }
    else
    {
        for (int32_t i = 0; i < listSize; i++)
        {
            const int32_t weaponIndex = list[i];
            const float facing = weaponLocked(weaponIndex, targetPosition);

            if (negFireArc <= facing && facing <= fireArc)
            {
                list[numLocked] = weaponIndex;
                numLocked++;
            }
        }
    }

    return numLocked;
}

auto Mover::getWeaponsInRange(int32_t* list, int32_t listSize, float orderFireRange) -> int32_t
{
    GameObject* target = pilot->getLastTarget();

    if (target == nullptr)
    {
        return -2;
    }

    vector_3d targetPosition = target->getPosition();
    const float metersToTarget = static_cast<float>(distanceFrom(targetPosition));
    int32_t numInRange = 0;

    if (listSize == -1)
    {
        for (int32_t i = numOther; i < numOther + numWeapons; i++)
        {
            if (weaponInRange(i, metersToTarget) != 0)
            {
                list[numInRange] = i;
                numInRange++;
            }
        }
    }
    else
    {
        for (int32_t i = 0; i < listSize; i++)
        {
            const int32_t weaponIndex = list[i];

            if (weaponInRange(weaponIndex, metersToTarget) != 0)
            {
                list[numInRange] = weaponIndex;
                numInRange++;
            }
        }
    }

    return numInRange;
}

auto Mover::getWeaponShots(int32_t weaponIndex) -> int32_t
{
    if (isWeaponIndex(weaponIndex) == 0)
    {
        return -1;
    }

    // Weapons without ammo (energy) never run out.
    if (MasterComponentList[inventory[weaponIndex].masterID].missileType == 0)
    {
        return 9999;
    }

    return ammoTypeTotal[inventory[weaponIndex].ammoIndex].curAmount;
}

auto Mover::getWeaponAmmoLevel(int32_t weaponIndex) -> float
{
    if (isWeaponIndex(weaponIndex) == 0)
    {
        return -1.0f;
    }

    const AmmoTally& ammo = ammoTypeTotal[inventory[weaponIndex].ammoIndex];
    return static_cast<float>(static_cast<double>(ammo.curAmount) / ammo.startAmount);
}

auto Mover::calcWeaponEffectiveness(int setMax) -> void
{
    int32_t effectiveness = 0;
    lastWeaponEffectivenessCalc = scenarioTime;
    float gunneryFactor = 1.0f;

    if (pilot != nullptr)
    {
        gunneryFactor = static_cast<float>(static_cast<double>(pilot->skills[MWS_GUNNERY]) * 0.02);
    }

    for (int32_t i = numOther; i < numOther + numWeapons; i++)
    {
        if (setMax != 0 || (inventory[i].disabled == 0 && getWeaponShots(i) > 0))
        {
            effectiveness =
                static_cast<int32_t>(static_cast<double>(inventory[i].effectiveness) * gunneryFactor + effectiveness);
        }
    }

    if (setMax != 0)
    {
        maxWeaponEffectiveness = static_cast<float>(effectiveness);
        return;
    }

    weaponEffectiveness = static_cast<float>(effectiveness);

    if (effectiveness == 0)
    {
        playMessage(static_cast<RadioMessageType>(0x23), 0);
    }
    else if (static_cast<double>(effectiveness) < static_cast<double>(maxWeaponEffectiveness) * 0.5f)
    {
        playMessage(static_cast<RadioMessageType>(0x22), 0);
    }
}

auto Mover::calcWeaponRangeRatings() -> void
{
    for (int32_t i = numOther; i < numOther + numWeapons; i++)
    {
        if (NumRangeRatings <= 0)
        {
            continue;
        }

        const double gunnery = pilot->skills[MWS_GUNNERY];
        const MasterComponent& weapon = MasterComponentList[inventory[i].masterID];
        float* rating = inventory[i].rangeRatings;

        for (int32_t step = 0; step < NumRangeRatings; step++)
        {
            const float range = static_cast<float>(static_cast<double>(step) * RangeRatingIncrement);
            // Out of the weapon's range: 1000 less.
            double value = gunnery;

            if (!(weapon.weaponRange[0] < range) ||
                (weapon.weaponRange[1] < range && weapon.weaponRange[2] < range && weapon.weaponRange[3] < range))
            {
                value -= 1000.0;
            }

            rating[0] = static_cast<float>(value);
            rating[1] = static_cast<float>(weapon.damage * value * 10.0 / weapon.recycleTime);
            rating += 2;
        }
    }
}

auto Mover::calcAmmoTotals() -> void
{
    numAmmoTypes = 0;

    if (numWeapons == 0)
    {
        return;
    }

    // One type per weapon ammo (9999 rounds for a weapon without ammo), then the bins' rounds.
    AmmoTally tally[100];
    const int32_t firstWeapon = numOther;
    const int32_t numWeaponItems = numWeapons;
    const int32_t firstAmmo = firstWeapon + numWeaponItems;

    for (int32_t i = firstWeapon; i < firstAmmo; i++)
    {
        const MasterComponent& weapon = MasterComponentList[inventory[i].masterID];
        const int32_t numTypes = numAmmoTypes;
        int32_t type = 0;

        while (type < numTypes && tally[type].masterId != weapon.ammoMasterId)
        {
            type++;
        }

        if (type < numTypes)
        {
            continue;
        }

        tally[numTypes].masterId = weapon.ammoMasterId;
        const int32_t rounds = weapon.missileType == 0 ? 9999 : 0;
        tally[numTypes].curAmount = rounds;
        tally[numTypes].startAmount = rounds;
        numAmmoTypes = static_cast<int8_t>(numTypes + 1);
    }

    const int32_t numTypes = numAmmoTypes;

    for (int32_t i = firstAmmo; i < firstWeapon + numAmmos + numWeaponItems; i++)
    {
        for (int32_t type = 0; type < numTypes; type++)
        {
            if (tally[type].masterId == inventory[i].masterID)
            {
                tally[type].curAmount += inventory[i].amount;
                tally[type].startAmount += inventory[i].amount;
                break;
            }
        }
    }

    ammoTypeTotal = std::make_unique<AmmoTally[]>(static_cast<size_t>(numTypes));
    std::copy_n(tally, numTypes, ammoTypeTotal.get());
}

auto Mover::calcOptimalRange(GameObject* target) -> int
{
    const float oldRange = optimalRange;
    lastOptimalRangeCalc = scenarioTime;

    if (target == nullptr)
    {
        target = getPilot()->getLastTarget();
    }

    const float fireRange = getFireRange(-2);

    // Outranging a mover target: stay just inside the longest range.
    if (target != nullptr && IsMover(target) && static_cast<Mover*>(target)->longestRangeWeapon != 0xff &&
        !(fireRange <= static_cast<Mover*>(target)->getFireRange(-2)))
    {
        optimalRange = static_cast<float>(static_cast<double>(fireRange) - 10.0);
        return optimalRange != oldRange ? 1 : 0;
    }

    // Else the range step whose summed ratings (then damage rates, then the farthest step) are best.
    auto setItem = [](int32_t index, float value, int32_t id)
    {
        if (index > -1 && index < sortList->numItems)
        {
            sortList->list[index].id = id;
            sortList->list[index].value = value;
        }
    };

    int32_t numWorking = 0;
    sortList->clear(1);

    for (int32_t step = 0; step < NumRangeRatings; step++)
    {
        float total = 0.0f;

        for (int32_t i = numOther; i < numOther + numWeapons; i++)
        {
            if (inventory[i].disabled == 0 && getWeaponShots(i) > 0)
            {
                if (step == 0)
                {
                    numWorking++;
                }

                total += inventory[i].rangeRatings[step * 2];
            }
        }

        setItem(step, total, step);
    }

    if (NumRangeRatings <= 0 || numWorking == 0)
    {
        optimalRange = 0.0f;
        return oldRange != 0.0f ? 1 : 0;
    }

    sortList->sort(1);
    int32_t bestStep = sortList->list[0].id;

    if (sortList->list[1].value == sortList->list[0].value)
    {
        sortList->clear(1);

        for (int32_t step = 0; step < NumRangeRatings; step++)
        {
            float total = 0.0f;

            for (int32_t i = numOther; i < numOther + numWeapons; i++)
            {
                if (inventory[i].disabled == 0 && getWeaponShots(i) > 0)
                {
                    total += inventory[i].rangeRatings[step * 2 + 1];
                }
            }

            setItem(step, total, step);
        }

        sortList->sort(1);
        const SortListNode* node = sortList->list.get();
        bestStep = node[0].id;

        if (node[1].value == node[0].value)
        {
            const float bestValue = node[0].value;

            do
            {
                if (bestStep < node->id)
                {
                    bestStep = node->id;
                }

                node++;
            } while (node->value == bestValue);
        }
    }

    optimalRange = static_cast<float>(bestStep) * RangeRatingIncrement;
    return optimalRange != oldRange ? 1 : 0;
}

auto Mover::calcLongestRangeWeapon() -> int32_t
{
    float longestRange = 0.0f;
    float shortestRange = 1000000.0f;
    longestRangeWeapon = 0xff;
    shortestRangeWeapon = 0xff;
    maxMinRange = 0.0f;

    for (int32_t i = numOther; i < numOther + numWeapons; i++)
    {
        if (inventory[i].disabled != 0 || getWeaponShots(i) <= 0)
        {
            continue;
        }

        const MasterComponent& weapon = MasterComponentList[inventory[i].masterID];

        if (longestRange < weapon.weaponRange[3])
        {
            longestRangeWeapon = static_cast<uint8_t>(i);
            longestRange = weapon.weaponRange[3];
        }

        if (weapon.weaponRange[1] < shortestRange)
        {
            shortestRangeWeapon = static_cast<uint8_t>(i);
            shortestRange = weapon.weaponRange[1];
        }

        if (maxMinRange < weapon.weaponRange[0])
        {
            maxMinRange = weapon.weaponRange[0];
        }
    }

    return longestRangeWeapon;
}

auto Mover::getFireRange(int32_t which) -> float
{
    switch (which)
    {
        case 0:
            return WeaponRange[0];
        case 1:
            return WeaponRange[1];
        case 2:
            return WeaponRange[2];
        case -4:
            return DefaultAttackRange;
        case -3:
            return 0.0f;
        case -2:
        {
            if (longestRangeWeapon != 0xff)
            {
                return MasterComponentList[inventory[longestRangeWeapon].masterID].weaponRange[3];
            }
            break;
        }
        case -1:
            return optimalRange;
        default:
            break;
    }

    return -1.0f;
}

auto Mover::getMaxFireRange() -> float
{
    return getFireRange(-2);
}

auto Mover::isWeaponIndex(int32_t itemIndex) -> int
{
    return numOther <= itemIndex && itemIndex < numOther + numWeapons ? 1 : 0;
}

auto Mover::isWeaponMissile(int32_t weaponIndex) -> int
{
    return MasterComponentList[inventory[weaponIndex].masterID].form == COMPONENT_FORM_WEAPON_MISSILE ? 1 : 0;
}

auto Mover::isWeaponReady(int32_t weaponIndex) -> int
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

auto Mover::isWeaponWorking(int32_t weaponIndex) -> int
{
    if (inventory[weaponIndex].disabled != 0)
    {
        return 0;
    }

    return getWeaponShots(weaponIndex) != 0 ? 1 : 0;
}

auto Mover::startWeaponRecycle(int32_t weaponIndex) -> void
{
    inventory[weaponIndex].readyTime = MasterComponentList[inventory[weaponIndex].masterID].recycleTime + scenarioTime;
}

auto Mover::tallyAmmo(int32_t ammoMasterId) -> int32_t
{
    int32_t total = 0;
    const int32_t firstAmmo = numOther + numWeapons;

    for (int32_t i = firstAmmo; i < firstAmmo + numAmmos; i++)
    {
        if (inventory[i].masterID == ammoMasterId)
        {
            total += inventory[i].amount;
        }
    }

    return total;
}

auto Mover::needsRefit(int armorOnly) -> int
{
    // Only a mech without a refit vehicle on the way.
    if (refitBuddy != nullptr || objectClass != BATTLEMECH)
    {
        return 0;
    }

    if (armorOnly == 0)
    {
        for (int32_t i = 0; i < numArmorLocations; i++)
        {
            if (i < numBodyLocations)
            {
                // A destroyed arm needs neither structure nor armor.
                if ((i == MECH_BODY_LOCATION_LARM || i == MECH_BODY_LOCATION_RARM) && bodyAt(i).damageState == 2)
                {
                    continue;
                }

                if (bodyAt(i).curInternalStructure < static_cast<float>(bodyAt(i).maxInternalStructure))
                {
                    return 1;
                }
            }

            if (armor[i].curArmor < static_cast<float>(armor[i].maxArmor))
            {
                return 1;
            }
        }
    }

    for (int32_t i = 0; i < numAmmoTypes; i++)
    {
        if (ammoTypeTotal[i].curAmount < ammoTypeTotal[i].startAmount)
        {
            return 1;
        }
    }

    return 0;
}

auto Mover::reduceAmmo(int32_t ammoMasterId, int32_t amount) -> int32_t
{
    // From the bins in order.
    int32_t left = amount;
    const int32_t firstAmmo = numOther + numWeapons;

    for (int32_t i = firstAmmo; i < firstAmmo + numAmmos; i++)
    {
        if (inventory[i].masterID != ammoMasterId)
        {
            continue;
        }

        if (left < inventory[i].amount)
        {
            inventory[i].amount = static_cast<int16_t>(inventory[i].amount - left);
            break;
        }

        left -= inventory[i].amount;
        inventory[i].amount = 0;
    }

    // Out of this ammo: the weapons, their effectiveness and the optimal range change.
    for (int32_t i = 0; i < numAmmoTypes; i++)
    {
        if (ammoTypeTotal[i].masterId != ammoMasterId)
        {
            continue;
        }

        const int32_t rounds = ammoTypeTotal[i].curAmount - amount;
        ammoTypeTotal[i].curAmount = rounds;

        if (rounds < 1)
        {
            ammoTypeTotal[i].curAmount = 0;
            calcLongestRangeWeapon();
            calcWeaponEffectiveness(0);
            calcOptimalRange(nullptr);
        }

        return amount;
    }

    return amount;
}

auto Mover::deductWeaponShot(int32_t weaponIndex, int32_t ammoAmount) -> void
{
    if (ammoAmount > 0)
    {
        reduceAmmo(MasterComponentList[inventory[weaponIndex].masterID].ammoMasterId, ammoAmount);
    }
}

auto Mover::sortWeapons(int32_t* weaponList, int32_t* valueList, int32_t listSize, int32_t sortType, int skillCheck)
    -> int32_t
{
    MechWarrior* myPilot = pilot;
    GameObject* target = myPilot->getLastTarget();

    if (target == nullptr)
    {
        return -2;
    }

    int32_t aimLocation = -1;

    if (myPilot != nullptr && myPilot->curTacOrder.isCombatOrder() != 0)
    {
        aimLocation = myPilot->curTacOrder.attackParams.aimLocation;
    }

    // Best attack chance first; only sort type 0 is known (the id goes in before the type is checked).
    auto setId = [](int32_t index, int32_t id)
    {
        if (index > -1 && index < sortList->numItems)
        {
            sortList->list[index].id = id;
        }
    };

    auto setValue = [](int32_t index, float value)
    {
        if (index > -1 && index < sortList->numItems)
        {
            sortList->list[index].value = value;
        }
    };

    sortList->clear(1);

    if (listSize == -1)
    {
        for (int32_t i = numOther; i < numOther + numWeapons; i++)
        {
            setId(i - numOther, i);

            if (sortType != 0)
            {
                return -3;
            }

            setValue(i - numOther, calcAttackChance(target, aimLocation, scenarioTime, i, 0.0f, nullptr, nullptr));
        }

        sortList->sort(1);
        listSize = numWeapons;

        if (listSize == 0)
        {
            return 0;
        }
    }
    else
    {
        for (int32_t i = 0; i < listSize; i++)
        {
            setId(i, weaponList[i]);
            float chance;

            if (weaponList[i] == -1)
            {
                chance = -999.0f;
            }
            else
            {
                if (sortType != 0)
                {
                    return -3;
                }

                chance = calcAttackChance(target, aimLocation, scenarioTime, weaponList[i], 0.0f, nullptr, nullptr);
            }

            setValue(i, chance);
        }

        sortList->sort(1);
    }

    for (int32_t i = 0; i < listSize; i++)
    {
        weaponList[i] = sortList->list[i].id;
        valueList[i] = static_cast<int32_t>(sortList->list[i].value);
    }

    return 0;
}

auto Mover::calcAttackChance(GameObject* target, int32_t aimLocation, float targetTime, int32_t weaponIndex,
                             float modifiers, int32_t* range, vector_3d* targetPoint) -> float
{
    if (weaponIndex < numOther || numOther + numWeapons <= weaponIndex)
    {
        return -9999.0f;
    }

    vector_3d targetPosition;

    if (target == nullptr)
    {
        if (targetPoint == nullptr)
        {
            return -9999.0f;
        }

        targetPosition = *targetPoint;
    }
    else
    {
        targetPosition = target->getPosition();
    }

    float gunnery = static_cast<float>(pilot->skills[MWS_GUNNERY]);

    if (MPlayer == nullptr)
    {
        if (getAlignment() == homeTeam->alignment)
        {
            gunnery = applyDifficultySkill(gunnery, 1);
        }
        else if (MPlayer == nullptr && getAlignment() != homeTeam->alignment)
        {
            gunnery = applyDifficultySkill(gunnery, 0);
        }
    }

    const float metersToTarget = static_cast<float>(distanceFrom(targetPosition));

    if (range != nullptr)
    {
        if (metersToTarget <= WeaponRange[0])
        {
            *range = 0;
        }
        else if (metersToTarget <= WeaponRange[1])
        {
            *range = 1;
        }
        else
        {
            *range = 2;
        }
    }

    // Out of the weapon's range: -1.
    const MasterComponent& weapon = MasterComponentList[inventory[weaponIndex].masterID];
    float rangeModifier;

    if (!(weapon.weaponRange[0] < metersToTarget))
    {
        return -1.0f;
    }

    if (!(weapon.weaponRange[1] < metersToTarget))
    {
        rangeModifier = WeaponFireModifiers[0];
    }
    else if (!(weapon.weaponRange[2] < metersToTarget))
    {
        rangeModifier = WeaponFireModifiers[1];
    }
    else if (!(weapon.weaponRange[3] < metersToTarget))
    {
        rangeModifier = WeaponFireModifiers[2];
    }
    else
    {
        return -1.0f;
    }

    modifiers = rangeModifier + modifiers;

    // Port fix: the original reads a null target's class when aimLocation isn't -1.
    const bool mechTarget = target != nullptr && target->objectClass == BATTLEMECH;

    if (aimLocation > -1 && mechTarget)
    {
        switch (aimLocation)
        {
            case 0:
                modifiers = WeaponFireModifiers[3] + modifiers;
                break;
            case 1:
            case 2:
            case 3:
                modifiers = WeaponFireModifiers[4] + modifiers;
                break;
            case 4:
            case 5:
            case 6:
            case 7:
                modifiers = WeaponFireModifiers[5] + modifiers;
                break;
            default:
                break;
        }
    }

    // An aimed shot at a mech skips the target's movement.
    if (!(aimLocation != -1 && mechTarget) && target != nullptr)
    {
        if (IsMover(target))
        {
            getVelocity();
            const vector_3d targetVelocity = target->getVelocity();

            if (target != stationaryTarget)
            {
                stationaryTarget = target;
                stationaryTime = 0.0f;
            }
            else
            {
                // A target holding still gets easier, up to MaxStationaryTime.
                const double x = targetVelocity.x;
                const double y = targetVelocity.y;
                const double z = targetVelocity.z;

                if (std::sqrt(x * x + y * y + z * z) == 0.0)
                {
                    stationaryTime = frameLength + stationaryTime;
                }
                else
                {
                    stationaryTime = 0.0f;
                }

                if (stationaryTime != 0.0f)
                {
                    double stationaryFactor = 1.0;

                    if (stationaryTime < MaxStationaryTime)
                    {
                        stationaryFactor = static_cast<double>(stationaryTime) / MaxStationaryTime;
                    }

                    modifiers = static_cast<float>(WeaponFireModifiers[23] * stationaryFactor + modifiers);
                }
            }
        }
        else
        {
            modifiers = WeaponFireModifiers[6] + modifiers;
        }
    }

    return static_cast<float>((static_cast<double>(modifiers) + 100.0f) * 0.01 * gunnery);
}

auto Mover::ammoExplosion(int32_t ammoIndex) -> void
{
    pilot->injure(2.0f, 1);
    Assert(ammoIndex < numOther + numWeapons + numAmmos, ammoIndex, " Ammo Index out of range ");
    Assert(numOther + numWeapons <= ammoIndex, ammoIndex, " Ammo Index too low ");
    InventoryItem& bin = inventory[ammoIndex];
    const int32_t hitLocation = bin.bodyLocation;
    const int32_t rounds = bin.amount;
    float damage = static_cast<float>(
        static_cast<double>(static_cast<int32_t>(MasterComponentList[bin.masterID].damage)) * rounds);

    if (damage > 254.0f)
    {
        damage = 254.0f;
    }

    bin.amount = 0;
    const int16_t typeIndex = bin.ammoIndex;

    if (typeIndex == -1)
    {
        Fatal(-1, " Bad Ammo Index in Ammo Explosion ");
    }

    Assert(typeIndex < numAmmoTypes, typeIndex, " Too Many Ammo Types ");
    Assert(typeIndex > -1, typeIndex, " not enough Ammo Types ");
    ammoTypeTotal[typeIndex].curAmount -= rounds;
    _WeaponShotInfo shotInfo;
    shotInfo.init(nullptr, bin.masterID, damage, hitLocation, 0.0f);
    handleWeaponHit(&shotInfo, 0);
}

auto Mover::disable(uint32_t cause) -> void
{
    if (isDisabled() != 0)
    {
        return;
    }

    if (pilot != nullptr)
    {
        pilot->handleAlarm(6, cause);
    }

    status = 1;
    disableThisFrame = 1;

    if (alignment == homeTeam->alignment)
    {
        friendlyDestroyed = 1;
    }
    else
    {
        // An enemy mech is salvage, unless the roll (or the cause) blows it apart.
        if (MPlayer == nullptr && objectClass == BATTLEMECH)
        {
            if (salvageRoll == -999)
            {
                salvageRoll = RollDice(MechSalvageChance);
            }

            if (cause == 3 || cause == 2)
            {
                if (salvageRoll == 0 && CantBlowSalvage == 0)
                {
                    for (int32_t i = 0; i < numBodyLocations; i++)
                    {
                        destroyBodyLocation(i);
                    }

                    status = 2;
                }
                else
                {
                    Terrain::terrainTacticalMap->AddSalvage(this);
                }
            }
            else if (CantBlowSalvage == 0 && salvageRoll == 0)
            {
                for (int32_t i = 0; i < numBodyLocations; i++)
                {
                    destroyBodyLocation(i);
                }

                status = 2;
                Terrain::terrainTacticalMap->RemoveSalvage(this, 1);
            }
        }

        enemyDestroyed = 1;
    }

    if (sensorSystem != nullptr)
    {
        sensorSystem->disable();
    }
}

auto Mover::shutDown() -> void
{
    if (isDisabled() == 0 && status != 5 && status != 4)
    {
        status = 4;
        shutDownThisFrame = 1;
    }
}

auto Mover::startUp() -> void
{
    if (isDisabled() == 0 && status != 3 && status != 0)
    {
        status = 3;
        startUpThisFrame = 1;
    }
}

auto Mover::isWithdrawing() -> int
{
    return pilot->curTacOrder.code == TACTICAL_ORDER_WITHDRAW ? 1 : 0;
}

auto Mover::getGroupId() -> int32_t
{
    if (group != nullptr)
    {
        return group->getId();
    }

    return -1;
}

auto Mover::getVitalInfo(void* vitalInfo) -> int32_t
{
    int32_t size = BigGameObject::getVitalInfo(nullptr);
    size = static_cast<int32_t>(debugStatus.size() + 1) + size + (static_cast<int32_t>(std::strlen(iconName) + 1) - 2) +
           (numAmmos + numWeapons + 9 + numOther) * 0x1c;

    for (const int32_t criticalSpaces : NumLocationCriticalSpaces)
    {
        size += criticalSpaces * 8;
    }

    if (vitalInfo != nullptr)
    {
        BigGameObject::getVitalInfo(vitalInfo);
    }

    return size;
}

auto Mover::setSelected(int32_t newSelected) -> void
{
    // Deselection takes a second (not for network players' movers).
    if (newSelected == 0 && netPlayerId < 0)
    {
        deselectTime = scenarioTime + 1.0f;
        return;
    }

    selected = newSelected;
    deselectTime = 0.0f;
}
