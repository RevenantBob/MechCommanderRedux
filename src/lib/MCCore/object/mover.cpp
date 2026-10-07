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
// Cleared at startup as WeaponFireChunk::init clears: no hit location.
MCWeaponFireChunk CurMoverWeaponFireChunk = {0, 0, 0, 0, 0, {0, 0}, 0, 0, 0, 0, 0, 0, -1, 0};
int32_t MCMover::NumMovers = 0;
MCSortList* MCMover::SortList = nullptr;
int32_t GoalMap[GOALMAP_CELL_DIM][GOALMAP_CELL_DIM];
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
int32_t HitLevel[2] = {10, 20};
int32_t ClusterSizeSrm = 2;
int32_t ClusterSizeLrm = 5;
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
    bool IsMover(const MCBaseObject* object)
    {
        const MCObjectClass objectClass = object->ObjectClass;
        return objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
               objectClass == MOVER;
    }

    /// <summary>Checks a chunk's target the way pack and unpack both do, setting StatusChunkUnpackErr.</summary>
    void CheckStatusChunkTarget(const MCStatusChunk* chunk)
    {
        switch (static_cast<int8_t>(chunk->TargetType))
        {
            case 0:
            case 4:
                break;
            case 1:
            {
                const int32_t targetId = chunk->TargetId;

                if (targetId < 0 || MPlayer->NumMovers <= targetId)
                {
                    StatusChunkUnpackErr = 1;
                }

                if (MPlayer->MoverRoster[targetId] == nullptr)
                {
                    StatusChunkUnpackErr = 2;
                }
                break;
            }

            case 2:
            {
                if (ObjectList->FindObjectFromPart(chunk->TargetId) == nullptr)
                {
                    StatusChunkUnpackErr = 3;
                }
                break;
            }
            case 3:
            {
                if (ObjectList->FindObjectFromPart(chunk->TargetId) == nullptr)
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
    void AppendStatusChunk(const MCStatusChunk* chunk)
    {
        char line[512];
        const int8_t targetType = static_cast<int8_t>(chunk->TargetType);
        MCBaseObject* target = nullptr;
        bool haveTarget = false;

        if (targetType == 1)
        {
            target = MPlayer->MoverRoster[chunk->TargetId];
            haveTarget = true;
        }
        else if (targetType == 2 || targetType == 3)
        {
            target = ObjectList->FindObjectFromPart(chunk->TargetId);
            haveTarget = true;
        }

        bool appendLine = true;

        if (haveTarget && target != nullptr)
        {
            if (IsMover(target))
            {
                std::snprintf(line, sizeof(line), "target = %s (%d)\n",
                              static_cast<MCMover*>(target)->DebugStatus.c_str(), target->PartId);
            }
            else
            {
                std::snprintf(line, sizeof(line), "target = objClass %d (%d)\n", static_cast<int>(target->ObjectClass),
                              target->PartId);
            }
        }
        else if (!haveTarget && targetType == 4)
        {
            // The middle of the target cell, on the ground.
            const float halfSide = WorldUnitsMapSide * 0.5f;
            MCVector3D point;
            point.X =
                static_cast<float>((chunk->TargetCellRC[1] + 0.5f) * static_cast<double>(MetersPerCell) - halfSide);
            point.Y = static_cast<float>(
                (static_cast<double>(halfSide) - chunk->TargetCellRC[0] * static_cast<double>(MetersPerCell)) -
                static_cast<double>(MetersPerCell) * 0.5f);
            point.Z = 0.0f;
            const float elevation = GameMap->GetTerrainElevation(point);
            std::snprintf(line, sizeof(line), "target point = (%f, %f, %f)\n", static_cast<double>(point.X),
                          static_cast<double>(point.Y), static_cast<double>(elevation));
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
                const int32_t firstId = chunk->TargetId - static_cast<int8_t>(chunk->TargetItemNumber);
                int32_t numObjects = 0;

                for (int32_t i = 0; i < 8; i++)
                {
                    MCBaseObject* object = ObjectList->FindObjectFromPart(firstId + i);

                    if (object == nullptr)
                    {
                        continue;
                    }

                    numObjects++;
                    std::snprintf(line, sizeof(line), "    %d: objClass %d (%d)\n", i,
                                  static_cast<int>(object->ObjectClass), object->PartId);
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

        std::snprintf(line, sizeof(line), "bodyState = %d\n", static_cast<int>(chunk->BodyState));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetType = %d\n", static_cast<int>(targetType));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetId = %d\n", chunk->TargetId);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetBlockOrTrainNumber = %d\n", chunk->TargetBlockOrTrainNumber);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetVertexOrCarNumber = %d\n", chunk->TargetVertexOrCarNumber);
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetItemNumber = %d\n",
                      static_cast<int>(static_cast<int8_t>(chunk->TargetItemNumber)));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "targetCellRC = (%d, %d)\n", static_cast<int>(chunk->TargetCellRC[0]),
                      static_cast<int>(chunk->TargetCellRC[1]));
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "ejectOrderGiven = %c\n", chunk->EjectOrderGiven != 0 ? 'T' : 'F');
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "jumpOrder = %c\n", chunk->JumpOrder != 0 ? 'T' : 'F');
        std::strcat(ChunkDebugMsg, line);
        std::snprintf(line, sizeof(line), "data = %x\n", chunk->Data);
        std::strcat(ChunkDebugMsg, line);
    }

    /// <summary>
    /// The jumping the path finders allow, as calcMovePath and calcEscapePath each repeat it: 8 offsets at no cost,
    /// or the mover's jump range when an AI mover in single player. An elemental away from its last target
    /// (or without one) sets JumpOnBlocked.
    /// </summary>
    void SetUpPathJumps(MCMover* mover, int32_t& numOffsets, int32_t& jumpCost)
    {
        jumpCost = 0;
        numOffsets = 8;

        if (mover->Pilot->OnHomeTeam() == 0 && MPlayer == nullptr)
        {
            mover->GetJumpRange(&numOffsets, &jumpCost);
        }

        if (mover->ObjectClass != ELEMENTAL)
        {
            return;
        }

        MCGameObject* lastTarget = mover->Pilot->GetLastTarget();

        if (lastTarget != nullptr)
        {
            MCVector3D targetPosition = lastTarget->GetPosition();

            if (mover->DistanceFrom(targetPosition) < ElementalTargetNoJumpDistance)
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
        const double cellMeters = static_cast<double>(MetersPerWorldUnit) * MCTerrain::MetersPerVertexDivMapcellDim;
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

                const int32_t index = GameMap->Width * rowTile + colTile;

                // Port fix: the original reads and writes before or past the map for a cell on its first or last row.
                if (index < 0 || index >= GameMap->Width * GameMap->Height)
                {
                    continue;
                }

                if (visit(GameMap->Map[index], row, col))
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

auto MCStatusChunk::Init() -> void
{
    BodyState = 0;
    TargetType = 0;
    TargetId = 0;
    TargetBlockOrTrainNumber = 0;
    TargetVertexOrCarNumber = 0;
    TargetItemNumber = 0;
    TargetCellRC[0] = -1;
    TargetCellRC[1] = -1;
    EjectOrderGiven = 0;
    JumpOrder = 0;
    Data = 0;
}

auto MCStatusChunk::Build(MCMover*) -> void
{
    BodyState = 0;
    Data = 0;
}

auto MCStatusChunk::Pack(MCMover*) -> void
{
    // Bits, low to high: body state (3), jump order, eject order, target type (3), then the target.
    Data = 0;
    bool packCell = JumpOrder != 0;

    if (!packCell)
    {
        switch (TargetType)
        {
            case 1:
                Data = static_cast<uint32_t>(TargetId) << 3;
                break;
            case 2:
                Data = ((static_cast<uint32_t>(TargetBlockOrTrainNumber) << 9 |
                         static_cast<uint32_t>(TargetVertexOrCarNumber))
                            << 3 |
                        static_cast<uint32_t>(static_cast<int8_t>(TargetItemNumber)))
                       << 3;
                break;
            case 3:
                Data = (static_cast<uint32_t>(TargetBlockOrTrainNumber) << 8 |
                        static_cast<uint32_t>(TargetVertexOrCarNumber))
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
        Data = (static_cast<uint32_t>(static_cast<int32_t>(TargetCellRC[0])) << 10 |
                static_cast<uint32_t>(static_cast<int32_t>(TargetCellRC[1])))
               << 3;
    }

    Data = (static_cast<uint32_t>(static_cast<int8_t>(TargetType)) | Data) * 2;

    if (EjectOrderGiven != 0)
    {
        Data |= 1;
    }

    Data <<= 1;

    if (JumpOrder != 0)
    {
        Data |= 1;
    }

    Data = BodyState | Data << 3;
    CheckStatusChunkTarget(this);
}

auto MCStatusChunk::Unpack(MCMover*) -> void
{
    uint32_t bits = Data;
    StatusChunkUnpackErr = 0;
    BodyState = bits & 7;
    JumpOrder = static_cast<int32_t>(bits >> 3 & 1);
    EjectOrderGiven = static_cast<int32_t>(bits >> 4 & 1);
    TargetType = static_cast<uint8_t>(bits >> 5 & 7);

    bool unpackCell = JumpOrder != 0;

    if (!unpackCell)
    {
        switch (TargetType)
        {
            case 1:
                TargetId = static_cast<int32_t>(bits >> 8 & 0x1f);
                break;
            case 2:
            {
                // A terrain object: its block, vertex and item on the vertex.
                TargetItemNumber = static_cast<uint8_t>(bits >> 8 & 7);
                const uint32_t block = bits >> 0x14 & 0xff;
                const uint32_t vertex = bits >> 0xb & 0x1ff;
                TargetBlockOrTrainNumber = static_cast<int32_t>(block);
                TargetVertexOrCarNumber = static_cast<int32_t>(vertex);
                TargetId =
                    static_cast<int8_t>(TargetItemNumber) + 0x1000 + static_cast<int32_t>((block * 400 + vertex) * 8);
                break;
            }

            case 3:
            {
                // A train car.
                const uint32_t train = bits >> 0x10 & 0xff;
                const uint32_t car = bits >> 8 & 0xff;
                TargetVertexOrCarNumber = static_cast<int32_t>(car);
                TargetBlockOrTrainNumber = static_cast<int32_t>(train);

                if (train == 0x80)
                {
                    TargetId = static_cast<int32_t>(car + 0x802c8);
                }
                else
                {
                    TargetId = static_cast<int32_t>(car + (train * 5 + 0x6400) * 0x14);
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
        TargetCellRC[1] = static_cast<int16_t>(bits >> 8 & 0x3ff);
        TargetCellRC[0] = static_cast<int16_t>(bits >> 0x12 & 0x3ff);
    }

    CheckStatusChunkTarget(this);
}

auto MCStatusChunk::EqualTo(MCStatusChunk* chunk) -> int
{
    if (BodyState != chunk->BodyState || EjectOrderGiven != chunk->EjectOrderGiven || JumpOrder != chunk->JumpOrder ||
        TargetType != chunk->TargetType || TargetId != chunk->TargetId || TargetCellRC[0] != chunk->TargetCellRC[0] ||
        TargetCellRC[1] != chunk->TargetCellRC[1])
    {
        DebugStatusChunk(nullptr, this, chunk);
        return 0;
    }

    return 1;
}

auto LoadMoverGameSystem(MCFitIniFile* sysFile, float maxVisualRange) -> int32_t
{
    int32_t result = sysFile->SeekBlock("Pathfinding");

    if (result != 0)
    {
        return result;
    }

    int32_t longRangeEnabled[3];
    result = sysFile->ReadIdLongArray("LongRangeMovementEnabled", longRangeEnabled, 3);

    if (result != 0)
    {
        return result;
    }

    for (int32_t i = 0; i < 3; i++)
    {
        LongRangeMovementEnabled[i] = longRangeEnabled[i] == 1 ? 1 : 0;
    }

    result = sysFile->ReadIdLong("SimplePathTileRange", SimpleMovePathRange);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("DelayedOrderTime", DelayedOrderTime);

    if (result != 0)
    {
        return result;
    }

    if (sysFile->ReadIdFloat("MoveTimeOut", MoveTimeOut) != 0)
    {
        MoveTimeOut = 30.0f;
    }

    if (sysFile->ReadIdFloat("MoveYieldTime", MoveYieldTime) != 0)
    {
        MoveYieldTime = 1.5f;
    }

    // Read twice, as the original does.
    if (sysFile->ReadIdLongArray("GroupMoveTrailLength", GroupMoveTrailLen, 2) != 0)
    {
        GroupMoveTrailLen[0] = 0;
        GroupMoveTrailLen[1] = 1;
    }

    if (sysFile->ReadIdLongArray("GroupMoveTrailLength", GroupMoveTrailLen, 2) != 0)
    {
        GroupMoveTrailLen[0] = 0;
        GroupMoveTrailLen[1] = 1;
    }

    result = sysFile->ReadIdFloat("GroupOrderGoalOffset", GroupOrderGoalOffset);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloatArray("MoveMarginOfError", MoveMarginOfError, 2);

    if (result != 0)
    {
        return result;
    }

    for (int32_t i = 0; i < NUM_OVERLAY_TYPES; i++)
    {
        OverlayWeightIndex[i] = i * MAPCELL_DIM * MAPCELL_DIM;
    }

    result =
        sysFile->ReadIdLongArray("OverlayCellCosts", OverlayWeightTable, NUM_MOVE_LEVELS * OVERLAY_WEIGHT_LEVEL_SIZE);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->SeekBlock("OptimumRange");

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdLong("NumRangeRatings", NumRangeRatings);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("RangeRatingIncrement", RangeRatingIncrement);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("MinRangeIncrement", MinRangeIncrement);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("MinRangeModIncrement", MinRangeModIncrement);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("MaxWeaponRangeMod", MaxWeaponRangeMod);

    if (result != 0)
    {
        return result;
    }

    Assert(sysFile->SeekBlock("Mover:General") == 0, 0, "Couldn't find Mover:General block in gamesys.fit");
    Assert(sysFile->ReadIdFloat("BlockCaptureRange", BlockCaptureRange) == 0, 0,
           "Couldn't find BlockCaptureRange in Mover:General block in gamesys.fit");
    Assert(sysFile->ReadIdFloat("RefitTime", RefitTime) == 0, 0,
           "Couldn't find RefitTime in Mover:General block in gamesys.fit");
    Assert(sysFile->ReadIdFloat("RefitRange", RefitRange) == 0, 0,
           "Couldn't find RefitRange in Mover:General block in gamesys.fit");
    Assert(sysFile->ReadIdFloat("RefitAmount", RefitAmount) == 0, 0,
           "Couldn't find RefitAmount in Mover:General block in gamesys.fit");
    Assert(sysFile->ReadIdFloat("RefitVehicleArmorCost", RefitCostArray[0][0]) == 0, 0,
           "Couldn't find RefitVehicleArmorCost in Mover:General block in gamesys.fit");
    Assert(sysFile->ReadIdFloat("RefitVehicleInternalCost", RefitCostArray[1][0]) == 0, 0,
           "Couldn't find RefitVehicleInternalCost in Mover:General block in gamesys.fit");
    Assert(sysFile->ReadIdFloat("RefitVehiclePointsToAmmo", RefitCostArray[2][0]) == 0, 0,
           "Couldn't find RefitVehiclePointsToAmmo in Mover:General block in gamesys.fit");
    Assert(sysFile->ReadIdFloat("RefitBayArmorCost", RefitCostArray[0][1]) == 0, 0,
           "Couldn't find RefitBayArmorCost in Mover:General block in gamesys.fit");
    Assert(sysFile->ReadIdFloat("RefitBayInternalCost", RefitCostArray[1][1]) == 0, 0,
           "Couldn't find RefitBayInternalCost in Mover:General block in gamesys.fit");
    Assert(sysFile->ReadIdFloat("RefitBayAmmoCost", RefitCostArray[2][1]) == 0, 0,
           "Couldn't find RefitBayAmmoCost in Mover:General block in gamesys.fit");

    result = sysFile->SeekBlock("Mover:FireWeapon");

    if (result != 0)
    {
        return result;
    }

    // Missing modifiers keep the defaults; [7..22] also fill RankVersusChassisCombatModifier's columns 1..4.
    if (sysFile->ReadIdFloatArray("WeaponFireModifiers", WeaponFireModifiers, 30) == 0)
    {
        for (int32_t rank = 0; rank < 4; rank++)
        {
            for (int32_t chassis = 0; chassis < 4; chassis++)
            {
                RankVersusChassisCombatModifier[rank][chassis + 1] = WeaponFireModifiers[7 + rank * 4 + chassis];
            }
        }
    }

    result = sysFile->ReadIdFloatArray("FireArc", FireArc, 3);

    if (result != 0)
    {
        return result;
    }

    // Stored as half arcs.
    for (float& arc : FireArc)
    {
        arc = static_cast<float>(arc * 0.5);
    }

    result = sysFile->ReadIdLong("AimedFireAbort", AimedFireAbort);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdLongArray("AimedFireHitTable", AimedFireHitTable, 3);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("DisableAttackModifier", DisableAttackModifier);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("DisableGunneryModifier", DisableGunneryModifier);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("SalvageAttackModifier", SalvageAttackModifier);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("MaxStationaryTime", MaxStationaryTime);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->SeekBlock("Mover:Damage");

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdLongArray("HitLevel", HitLevel, 2);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("PilotingCheckFactor", PilotingCheckFactor);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->SeekBlock("Components");

    if (result != 0)
    {
        return result;
    }

    // The cluster sizes are read, then fixed at 2 and 5.
    result = sysFile->ReadIdLong("ClusterSizeSRM", ClusterSizeSrm);

    if (result != 0)
    {
        return result;
    }

    ClusterSizeSrm = 2;
    result = sysFile->ReadIdLong("ClusterSizeLRM", ClusterSizeLrm);

    if (result != 0)
    {
        return result;
    }

    ClusterSizeLrm = 5;
    result = sysFile->ReadIdLongArray("InnerSphereAntiMissile", AntiMissileSystemStats[0], 2);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdLongArray("ClanAntiMissile", AntiMissileSystemStats[1], 2);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->SeekBlock("Warrior");

    if (result != 0)
    {
        return result;
    }

    if (sysFile->ReadIdFloat("DefaultAttackRadius", DefaultAttackRadius) != 0)
    {
        DefaultAttackRadius = 275.0f;
    }

    result = sysFile->ReadIdFloatArray("WarriorRankScale", WarriorRankScale, 4);

    if (result != 0)
    {
        return result;
    }

    char table[10];
    result = sysFile->ReadIdCharArray("ProfessionalismTable", table, 10);

    if (result != 0)
    {
        return result;
    }

    std::memcpy(ProfessionalismOffsetTable, table, sizeof(ProfessionalismOffsetTable));
    result = sysFile->ReadIdCharArray("DecorumTable", table, 10);

    if (result != 0)
    {
        return result;
    }

    std::memcpy(DecorumOffsetTable, table, sizeof(DecorumOffsetTable));
    result = sysFile->ReadIdCharArray("AmmoTable", table, 4);

    if (result != 0)
    {
        return result;
    }

    std::memcpy(AmmoConservationModifiers, table, sizeof(AmmoConservationModifiers));
    result = sysFile->ReadIdFloat("PilotCheckHalfRate", PilotCheckHalfRate);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdLongArray("PilotCheckModifiers", PilotCheckModifierTable, 2);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("DamageRateFrequency", DamageRateFrequency);

    if (result != 0)
    {
        return result;
    }

    char attitudeEffect[sizeof(AttitudeEffect)];
    result = sysFile->ReadIdCharArray("AttitudeEffect", attitudeEffect, sizeof(attitudeEffect));

    if (result != 0)
    {
        return result;
    }

    std::memcpy(AttitudeEffect, attitudeEffect, sizeof(AttitudeEffect));
    result = sysFile->ReadIdFloat("MovementUpdateFrequency", MovementUpdateFrequency);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("CombatUpdateFrequency", CombatUpdateFrequency);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("CommandUpdateFrequency", CommandUpdateFrequency);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("ContactUpdateFrequency", ContactUpdateFrequency);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("PilotCheckUpdateFrequency", PilotCheckUpdateFrequency);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloatArray("FireOddsTable", FireOddsTable, 5);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdLong("SkillIncreaseCap", IncreaseCap);
    Assert(result == 0, result, " Couldn't find SkillCap variable in Warrior block of gamesys.fit ");
    result = sysFile->ReadIdFloat("SkillMax", MaxPilotSkill);
    Assert(result == 0, result, " Couldn't find SkillMax variable in Warrior block of gamesys.fit ");
    result = sysFile->ReadIdFloat("SkillMin", MinPilotSkill);
    Assert(result == 0, result, " Couldn't find SkillMin variable in Warrior block of gamesys.fit ");
    result = sysFile->ReadIdLong("JumpSkillMod", PilotJumpMod);
    Assert(result == 0, result, " Couldn't find JumpSkillMod variable in Warrior block of gamesys.fit ");

    result = sysFile->SeekBlock("Sensors");

    if (result != 0)
    {
        return result;
    }

    char automaticSuccess;
    result = sysFile->ReadIdChar("AutomaticSuccess", automaticSuccess);

    if (result != 0)
    {
        return result;
    }

    SensorAutomaticSuccess = automaticSuccess == 1 ? 1 : 0;
    result = sysFile->ReadIdCharArray("SensorSkillMoveRange", SensorSkillMoveRange, 4);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloatArray("SensorSkillMoveFactor", &SensorSkillMoveFactor[0][0], 8);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloatArray("SensorModifiers", SensorModifier, 8);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("BaseSensorRollTarget", SensorBaseChance);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("SensorSkillFactor", SensorSkillFactor);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("BlockingObjectModifier", SensorBlockingObjectModifier);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("ShutdownMech", SensorShutDownMechModifier);

    if (result != 0)
    {
        return result;
    }

    float rangeModifiers[8];
    result = sysFile->ReadIdFloatArray("SensorRangeModifier", rangeModifiers, 8);

    if (result != 0)
    {
        return result;
    }

    std::memcpy(SensorRangeModifier, rangeModifiers, sizeof(SensorRangeModifier));
    float sizeModifiers[6];
    result = sysFile->ReadIdFloatArray("SizeModifier", sizeModifiers, 6);

    if (result != 0)
    {
        return result;
    }

    std::memcpy(SensorSizeModifier, sizeModifiers, sizeof(SensorSizeModifier));
    // Read and dropped.
    int32_t sensorMasterIds[9];
    result = sysFile->ReadIdLongArray("SensorMasterIDs", sensorMasterIds, 9);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloatArray("BlockingTerrainModifiers", SensorBlockingTerrain, 2);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->SeekBlock("Skills");

    if (result != 0)
    {
        Fatal(result, "Couldn't find skill block in gamesys.fit");
    }

    result = sysFile->ReadIdFloatArray("Skill Attempt", SkillTry, 4);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloatArray("Skill Success", SkillSuccess, 4);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloat("WeaponHit", WeaponHit);

    if (result != 0)
    {
        return result;
    }

    result = sysFile->ReadIdFloatArray("KillSkillValues", KillSkill, 6);

    if (result != 0)
    {
        return result;
    }

    return sysFile->ReadIdFloat("Sensor Contact Skill", SensorSkill);
}

auto DebugStatusChunk(MCMover* mover, MCStatusChunk* chunk1, MCStatusChunk* chunk2) -> void
{
    char line[512];
    ChunkDebugMsg[0] = '\0';

    if (mover == nullptr)
    {
        std::strcat(ChunkDebugMsg, "\nmover = ???\n");
    }
    else
    {
        std::snprintf(line, sizeof(line), "\nmover = %s (%d)\n", mover->DebugStatus.c_str(), mover->PartId);
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

    auto* file = new MCFile;
    file->Create("stchunk.dbg");
    file->WriteString(ChunkDebugMsg);
    file->Close();
    delete file;
    ExceptionGameMsg = ChunkDebugMsg;
}

auto GetMoverFromPartId(int32_t partId) -> MCMover*
{
    // Port fix: the original indexed MoverRoster from part id 0, reading the memory before it for ids under 0x200.
    if (partId >= 0x200 && partId < MAX_MOVER_PART_ID)
    {
        return static_cast<MCMover*>(MoverRoster[partId - 0x200]);
    }

    return nullptr;
}

//---------------------------------------------------------------------------
// Mover: the header's inline functions
//---------------------------------------------------------------------------

auto MCMover::LineOfSight(MCGameObject* target) -> int
{
    return Team->LineOfSight(target->GetPosition());
}

auto MCMover::LineOfSight(MCVector3D point) -> int
{
    return Team->LineOfSight(point);
}

auto MCMover::ForcePilotingCheck() -> void
{
    if (PilotCheckModifier < 0)
    {
        PilotCheckModifier = 0;
    }
}

auto MCMover::SetAlignment(int32_t newAlignment) -> void
{
    MCBigGameObject::SetAlignment(newAlignment);

    if (Pilot != nullptr)
    {
        Pilot->Alignment = static_cast<int8_t>(newAlignment);
    }
}

auto MCMover::RelViewFacingTo(MCVector3D goal) -> float
{
    return MCGameObject::RelFacingTo(goal, -1);
}

auto MCMover::GetJumpRange(int32_t* numOffsets, int32_t* jumpCost) -> float
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

auto MCMover::CalcSpriteSpeed(float, uint32_t, int32_t& state, int32_t& throttle) -> int32_t
{
    state = 0;
    throttle = 100;
    return -1;
}

auto MCMover::GetPositionFromHS(uint32_t) -> MCVector3D
{
    MCVector3D position;
    position.X = 0.0f;
    position.Y = 0.0f;
    position.Z = 0.0f;
    return position;
}

//---------------------------------------------------------------------------
// Mover
//---------------------------------------------------------------------------

auto MCMover::Init() -> void
{
    // The base objects' fields (their inits are inline and not called).
    IdNumber = 0;
    Position.Y = 0.0f;
    Next = nullptr;
    PartId = -1;
    ObjType = nullptr;
    Position.Z = 0.0f;
    Position.X = 0.0f;
    Selected = 0;
    CollisionsOn = 0;
    Alignment = 0;
    Status = 0;
    ObjectClass = MOVER;

    if (MPlayer == nullptr)
    {
        NetName = nullptr;
    }
    else
    {
        NetName = std::make_unique<char[]>(0x100);
        CLoadString(ThisInstance, 0xb9, NetName.get(), 0xfe);
    }

    Cockpit = 0xff;
    Engine = 0xff;
    LifeSupport = 0xff;
    Sensor = 0xff;
    Ecm = 0xff;
    Probe = 0xff;
    Jammer = 0xff;
    Selected = 0;
    StatusChunk.BodyState = 0;
    DebugStatus.clear();
    Pilot = nullptr;
    Inventory = nullptr;
    SensorSystem = nullptr;
    EcmTracker = nullptr;
    JammerTracker = nullptr;
    CurCV = 0;
    MaxCV = 0;
    Body = nullptr;
    NumBodyLocations = 0;
    Armor = nullptr;
    NumArmorLocations = 0;
    DamageRateTally = 0.0f;
    DamageRateCheckTime = 1.0f;
    TotalDamageTaken = 0.0f;
    Status = 0;
    EngineBlowTime = -1.0f;
    MaxRunSpeed = 0.0f;
    ShutDownThisFrame = 0;
    StartUpThisFrame = 0;
    DisableThisFrame = 0;
    Team = nullptr;
    Group = nullptr;
    SelectionIndex = -1;
    PilotCheckModifier = -1;
    PilotingCheckPending = 0;
    LastWeaponEffectivenessCalc = 0.0f;
    LastOptimalRangeCalc = 0.0f;
    OptimalRange = -1.0f;
    Appearance = nullptr;
    Control = nullptr;
    Dynamics = nullptr;
    NetOwnerID = 0;
    NetPlayerId = -1;
    NetRosterIndex = -1;
    NewMoveChunk = 0;
    StatusChunk.Init();
    MoveChunk.Init();
    NumWeaponFireChunks[1] = 0;
    NumWeaponFireChunks[0] = 0;
    NumCriticalHitChunks[1] = 0;
    NumCriticalHitChunks[0] = 0;
    NumRadioChunks[1] = 0;
    NumRadioChunks[0] = 0;
    EjectOrderGiven = 0;
    DeathTimer = 1.0f;
    Withdrawing = 0;
    NumMovers++;
    LastHustleTime = -999.0f;
    CollisionsOn = 1;
    Challenger = nullptr;

    if (SortList == nullptr)
    {
        SortList = new MCSortList;

        if (SortList == nullptr)
        {
            Fatal(0, " Unable to create Mover::sortList ");
        }

        SortList->Init(100);
    }

    CrashAvoidSelf = 1;
    CrashAvoidPath = 1;
    PathLockLevel = 1;
    PathLockRange = 1;
    AmmoTypeTotal = nullptr;
    RefitBuddy = nullptr;
    CrashYieldTime = 1.5f;
    NumPathRangeLocks = 0;
    OverlayWeightClass = 0;
    DeselectTime = 0.0f;
    SalvageRoll = -999;
    DrawOrderLines = 0;
}

auto MCMover::SetPartId(int32_t newPartId) -> void
{
    PartId = newPartId;

    // Port fix: the original stored ids under 0x200 before MoverRoster (see getMoverFromPartId).
    if (newPartId >= 0x200 && newPartId < MAX_MOVER_PART_ID)
    {
        MoverRoster[newPartId - 0x200] = this;
    }
}

auto MCMover::SetPartId(int32_t commanderId, int32_t groupId, int32_t index) -> void
{
    SetPartId(index + 0x200 + (commanderId * 32 + groupId) * 12);
}

auto MCMover::SetPosition(MCVector3D& newPosition) -> void
{
    // Kept on the map; a mover pushed off it (or into the corners, which the map's diamond cuts off) is destroyed
    // when the mover is withdrawing.
    const float halfSide = WorldUnitsMapSide * 0.5f;
    const float negHalfSide = -halfSide;
    const float startX = newPosition.X;

    if (startX < negHalfSide)
    {
        newPosition.X = negHalfSide;
    }

    const float clampedX = newPosition.X;

    if (halfSide < clampedX)
    {
        newPosition.X = halfSide;
    }

    const float startY = newPosition.Y;

    if (negHalfSide > startY)
    {
        newPosition.Y = negHalfSide;
    }

    bool onMap = false;

    if (newPosition.Y <= halfSide)
    {
        if (negHalfSide <= startY && halfSide >= clampedX && negHalfSide <= startX)
        {
            const double limit = static_cast<double>(MCTerrain::VerticesBlockSide) * MCTerrain::BlocksMapSide *
                                     MCTerrain::MetersPerVertex * 0.5f -
                                 1300.0;
            const double diff = static_cast<double>(newPosition.Y) - newPosition.X;
            const float sum = newPosition.X + newPosition.Y;
            const float negLimit = static_cast<float>(-limit);
            onMap = !(diff > limit) && diff >= negLimit && !(sum > limit) && sum >= negLimit;
        }
    }
    else
    {
        newPosition.Y = halfSide;
    }

    if (!onMap && Withdrawing != 0)
    {
        ObjType->HandleDestruction(this, nullptr);
    }

    Position = newPosition;

    if (ObjPosition != nullptr)
    {
        GameObjectMap->UpdateObject(this, 0);
    }
}

auto MCMover::SetAwake(int awake) -> void
{
    Flags &= 0xfe;

    if (awake == 0)
    {
        return;
    }

    Flags |= 1;

    if (Pilot != nullptr && static_cast<uint8_t>(Status) == 5)
    {
        Pilot->OrderPowerUp(0, 2);
    }
}

auto MCMover::RelFacingDelta(MCVector3D goalPos, MCVector3D targetPos) -> float
{
    const float goalFacing = RelFacingTo(goalPos, -1);
    const float targetFacing = RelFacingTo(targetPos, -1);

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
    /// <summary>A quarter turn's half, as MCX.EXE stores it (a hair over pi / 4).</summary>
    constexpr double EIGHTH_TURN = 0x1.921fb5443e88cp-1;
    /// <summary>Radians to degrees.</summary>
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;
    /// <summary>Radians to degrees, the float-rounded copy.</summary>
    constexpr double RADIANS_TO_DEGREES_F = 0x1.ca5dc2p+5;

    /// <summary>The frame turned an eighth of a turn about its up axis (the facing the art is drawn at).</summary>
    MCFrameOfRef TurnedFrame(const MCFrameOfRef& frame)
    {
        const float s = static_cast<float>(std::sin(EIGHTH_TURN));
        const float c = static_cast<float>(std::cos(EIGHTH_TURN));
        MCFrameOfRef turned = frame;
        turned.I = frame.I * c + frame.J * s;
        turned.J = frame.J * c - frame.I * s;
        return turned;
    }
}

auto MCMover::RelFacingTo(MCVector3D goal, int32_t) -> float
{
    const float x = Position.X;
    const float y = Position.Y;
    const MCFrameOfRef turned = TurnedFrame(Frame);
    MCVector3D facing;
    facing.X = -turned.J.X;
    facing.Y = -turned.J.Y;
    facing.Z = -turned.J.Z;

    MCVector3D toGoal;
    toGoal.X = goal.X - x;
    toGoal.Y = goal.Y - y;
    toGoal.Z = 0.0f;
    const double length =
        std::sqrt((static_cast<double>(toGoal.X) * toGoal.X + static_cast<double>(toGoal.Y) * toGoal.Y) +
                  static_cast<double>(toGoal.Z) * toGoal.Z);

    if (length != 0.0)
    {
        toGoal.X = static_cast<float>(toGoal.X / length);
        toGoal.Y = static_cast<float>(toGoal.Y / length);
        toGoal.Z = static_cast<float>(toGoal.Z / length);
    }

    const double cosine = static_cast<double>(toGoal.Z) * facing.Z + static_cast<double>(toGoal.Y) * facing.Y +
                          static_cast<double>(toGoal.X) * facing.X;
    const float angle = static_cast<float>(AcosMatherr(cosine) * RADIANS_TO_DEGREES_F);

    // Negative to the left.
    if ((facing & toGoal).Z >= 0.0f)
    {
        return -angle;
    }

    return angle;
}

auto MCMover::GetTerrainAngle() -> float
{
    return static_cast<float>(AcosMatherr(static_cast<double>(TerrainNormal.Z)) * RADIANS_TO_DEGREES);
}

auto MCMover::GetVelocityTilt() -> float
{
    const MCFrameOfRef turned = TurnedFrame(Frame);
    const double cosine = static_cast<double>(turned.J.Z) * TerrainNormal.Z +
                          static_cast<double>(turned.J.Y) * TerrainNormal.Y +
                          static_cast<double>(turned.J.X) * TerrainNormal.X;
    return static_cast<float>(AcosMatherr(cosine) * RADIANS_TO_DEGREES);
}

auto MCMover::GetFireArc() -> float
{
    switch (ObjectClass)
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

auto MCMover::Destroy() -> void
{
    NetName.reset();

    if (SensorSystem != nullptr)
    {
        SensorSystemManager->FreeSensor(SensorSystem);
        SensorSystem = nullptr;
    }

    if (EcmTracker != nullptr)
    {
        Team->RemoveEcm(EcmTracker);
        EcmTracker = nullptr;
    }

    if (JammerTracker != nullptr)
    {
        Team->RemoveJammer(JammerTracker);
        JammerTracker = nullptr;
    }

    DebugStatus.clear();

    if (Body != nullptr)
    {
        for (int32_t i = 0; i < NumBodyLocations; i++)
        {
            if (BodyAt(i).CriticalSpaces != nullptr)
            {
                delete[] BodyAt(i).CriticalSpaces;
                BodyAt(i).CriticalSpaces = nullptr;
            }
        }

        Body.reset();
        NumBodyLocations = 0;
    }

    if (Armor != nullptr)
    {
        Armor.reset();
        NumArmorLocations = 0;
    }

    if (Inventory != nullptr)
    {
        for (uint32_t i = NumOther; i < static_cast<uint32_t>(NumOther) + NumWeapons; i++)
        {
            if (Inventory[i].RangeRatings != nullptr)
            {
                delete[] Inventory[i].RangeRatings;
                Inventory[i].RangeRatings = nullptr;
            }
        }

        Inventory.reset();
    }

    NumAmmoTypes = 0;

    if (AmmoTypeTotal != nullptr)
    {
        AmmoTypeTotal.reset();
    }

    if (PotentialContact != nullptr)
    {
        PotentialContactManager->Remove(PotentialContact);
        PotentialContact = nullptr;
    }

    if (Appearance != nullptr)
    {
        delete Appearance;
    }

    Appearance = nullptr;

    if (Control != nullptr)
    {
        delete Control;
    }

    Control = nullptr;

    if (Dynamics != nullptr)
    {
        delete Dynamics;
    }

    Dynamics = nullptr;

    NumMovers--;

    if (NumMovers == 0)
    {
        if (SortList != nullptr)
        {
            SortList->Destroy();
            delete SortList;
        }

        SortList = nullptr;
    }
}

auto MCMover::RelativePosition(float angle, float distance, uint32_t flags) -> MCVector3D
{
    // The point distance meters away at angle: flag 1, an absolute angle in radians; else degrees from the
    // mover's facing. The x87 keeps some of the sums below at extended precision, done here in double.
    const float reach = -(WorldUnitsPerMeter * distance);
    const float x = Position.X;
    const float y = Position.Y;
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
        MCFrameOfRef turned = Frame;
        const double radians = (static_cast<double>(angle) + 45.0) * 0x1.1df46a2526c7ap-6;
        const float s = static_cast<float>(std::sin(radians));
        const float c = static_cast<float>(std::cos(radians));
        const MCVector3D oldI = turned.I;
        turned.I = turned.I * c + turned.J * s;
        turned.J = turned.J * c - oldI * s;
        const MCVector3D offset = turned.J * reach;
        offsetX = offset.X;
        offsetY = offset.Y;
    }

    const double targetX = offsetX + x;
    const float targetY = static_cast<float>(static_cast<double>(offsetY) + y);

    // Flag 2 walks from the mover out to the point; otherwise from the point back to the mover.
    MCVector2D start;
    MCVector2D end;

    if ((flags & 2) != 0)
    {
        end.X = static_cast<float>(targetX);
        start.X = x;
        start.Y = y;
        end.Y = targetY;
    }
    else
    {
        start.Y = targetY;
        start.X = static_cast<float>(targetX);
        end.X = x;
        end.Y = y;
    }

    // Half a map cell per step.
    const double deltaX = static_cast<double>(end.X) - start.X;
    const float deltaXf = static_cast<float>(deltaX);
    const float deltaY = end.Y - start.Y;
    const float length =
        static_cast<float>(std::sqrt(static_cast<double>(deltaY) * deltaY + static_cast<double>(deltaXf) * deltaXf));
    double directionX = deltaX;
    float directionY = deltaY;

    if (length != 0.0)
    {
        directionX = static_cast<double>(deltaXf) / length;
        directionY = static_cast<float>(static_cast<double>(deltaY) / length);
    }

    const float stepLength = static_cast<float>(static_cast<double>(MCTerrain::MetersPerVertex) * 0.33333334f * 0.5);
    const float stepX = static_cast<float>(directionX * stepLength);
    const double stepYExact = static_cast<double>(directionY) * stepLength;
    const float stepY = static_cast<float>(stepYExact);

    if (std::sqrt(stepYExact * stepY + static_cast<double>(stepX) * stepX) == 0.0)
    {
        MCVector3D result;
        result.X = x;
        result.Y = y;
        result.Z = 0.0f;
        return result;
    }

    const MCVector2D span = start - end;
    const float maxDistance =
        static_cast<float>(std::sqrt(static_cast<double>(span.X) * span.X + static_cast<double>(span.Y) * span.Y));
    float traveled = 0.0f;
    MCVector2D current = start;

    // Whether the cell under current is passable.
    auto cellPassable = [&]()
    {
        MCVector3D point;
        point.X = current.X;
        point.Y = current.Y;
        point.Z = 0.0f;
        int32_t tileR;
        int32_t tileC;
        int32_t cellR;
        int32_t cellC;
        GameMap->WorldToMapPos(point, tileR, tileC, cellR, cellC);

        // Port fix: the walk can leave the map, where the original reads outside it. Off the map is impassable.
        if (!GameMap->OnMap(tileR, tileC))
        {
            return 0u;
        }

        return GameMap->Map[GameMap->Width * tileR + tileC].GetCellPassable(cellR, cellC);
    };

    uint32_t passable = cellPassable();
    MCVector2D previous = start;
    // Walk until the cell changes kind (or the distance runs out); the answer is the step before.
    const uint32_t keepGoingWhile = (flags & 2) != 0 ? 1u : 0u;

    if ((passable != 0) == (keepGoingWhile != 0))
    {
        while (traveled < maxDistance)
        {
            previous = current;
            current.X = stepX + current.X;
            current.Y = stepY + current.Y;
            const double dx = static_cast<double>(current.X) - start.X;
            const double dy = static_cast<double>(current.Y) - start.Y;
            traveled = static_cast<float>(std::sqrt(dx * dx + dy * dy));
            passable = cellPassable();

            if ((passable != 0) != (keepGoingWhile != 0))
            {
                break;
            }
        }
    }

    MCVector3D ground;
    ground.X = previous.X;
    ground.Y = previous.Y;
    ground.Z = 0.0f;
    MCVector3D result;
    result.X = previous.X;
    result.Y = previous.Y;
    result.Z = GameMap->GetTerrainElevation(ground);
    return result;
}

auto MCMover::LineOfFire(MCGameObject* target) -> int
{
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    GameMap->WorldToMapPos(target->GetPosition(), tileR, tileC, cellR, cellC);
    target->ClearLineOfFire();
    const int result = GameMap->LineOfFire(Position, target->GetPosition());
    target->RestoreLineOfFire();
    return result;
}

auto MCMover::LineOfFire(MCVector3D point) -> int
{
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    GameMap->WorldToMapPos(point, tileR, tileC, cellR, cellC);
    return GameMap->LineOfFire(Position, point);
}

auto MCMover::LineOfSensor(MCGameObject* target, int32_t& sensorResult, int32_t& losResult) -> void
{
    // Eye to eye, ten meters up; neither blocks itself.
    MCVector3D start;
    start.X = Position.X;
    start.Y = Position.Y;
    start.Z = static_cast<float>(static_cast<double>(WorldUnitsPerMeter) * 10.0 + Position.Z);
    const MCVector3D targetPosition = target->GetPosition();
    MCVector3D end;
    end.X = targetPosition.X;
    end.Y = targetPosition.Y;
    end.Z = static_cast<float>(static_cast<double>(WorldUnitsPerMeter) * 10.0 + targetPosition.Z);
    SetUseMe(0);
    target->SetUseMe(0);
    GameMap->LineOfSensor(start, end, sensorResult, losResult);
    SetUseMe(1);
    target->SetUseMe(1);
}

auto MCMover::HandleEvent(MCObjectEvent* event) -> int32_t
{
    switch (event->Type)
    {
        case 0:
        {
            // Interface events.
            switch (event->Id)
            {
                case 0x1c:
                {
                    Selected = 1;
                    SelectionIndex = event->SelectionIndex;
                    return 0;
                }
                case 0x1d:
                {
                    SetSelected(0);
                    SelectionIndex = -1;
                    return 0;
                }
                case 0x1e:
                case 0x1f:
                {
                    return 0;
                }
                default:
                {
                    if (event->Id < 0 || event->Id > 0x1b)
                    {
                        Fatal(2, " Bad ObjectEvent GUI Code ");
                    }

                    return 0;
                }
            }
        }
        case 1:
        {
            if (event->Id != 6 && event->Id != 7)
            {
                Fatal(0, " Bad ObjectEvent Message Code ");
            }

            return 0;
        }
        case 2:
        {
            if (event->Id < 0 || event->Id > 8)
            {
                Fatal(3, " Bad ObjectEvent Combat Code ");
            }

            return 0;
        }
        default:
        {
            char message[256];
            std::snprintf(message, sizeof(message), "Mover::handleEvent->Bad ObjectEvent Type (%d)", event->Type);
            Fatal(1, message);
        }
    }
}

auto MCMover::HandleTacticalOrder(MCTacticalOrder tacOrder, int32_t priority, int queuePlayerOrder) -> int32_t
{
    if (queuePlayerOrder != 0)
    {
        tacOrder.Pack(nullptr, nullptr);
    }

    // A client checks the order survives packing (the result isn't used).
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        tacOrder.Pack(nullptr, nullptr);
        MCTacticalOrder check;
        check.Init();
        check.Data[0] = tacOrder.Data[0];
        check.Data[1] = tacOrder.Data[1];
        check.Unpack();
        check.Destroy();
    }

    int32_t radioMessageId = -1;
    int giveOrder = 1;
    bool checkCanMove = false;

    switch (tacOrder.Code)
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
            const int32_t delay = SelectionIndex;

            if (delay != -1)
            {
                tacOrder.DelayedTime = static_cast<float>(delay) * DelayedOrderTime + ScenarioTime;
            }

            if (IsDisabled() != 0 && CanMove() == 0)
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
            int canJumpThere = ObjectClass == BATTLEMECH ? 1 : 0;
            MCGameObject* target = tacOrder.Target;

            if (target != nullptr && IsMover(target) && target->GetTeam() == GetTeam())
            {
                canJumpThere = 0;
            }

            const float jumpRange = GetJumpRange(nullptr, nullptr);
            MCVector3D jumpGoal = tacOrder.GetWayPoint(0);

            if (jumpRange < DistanceFrom(jumpGoal))
            {
                canJumpThere = 0;
            }

            bool cellOpen = true;

            if (ObjectClass == BATTLEMECH)
            {
                int32_t tileR;
                int32_t tileC;
                int32_t cellR;
                int32_t cellC;
                GameMap->WorldToMapPos(tacOrder.GetWayPoint(0), tileR, tileC, cellR, cellC);
                // Port fix: the player's jump point can be off the map, where the original reads outside it.
                cellOpen = GameMap->OnMap(tileR, tileC) &&
                           GameMap->Map[GameMap->Width * tileR + tileC].GetCellPassable(cellR, cellC) != 0;
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
            if (tacOrder.AttackParams.Method == 1)
            {
                tacOrder.Code = TACTICAL_ORDER_JUMPTO_OBJECT;
                tacOrder.MoveParams.Wait = 0;
                tacOrder.MoveParams.WayPath.Mode[0] = 0;

                if (tacOrder.Target != nullptr)
                {
                    tacOrder.SetWayPoint(0, tacOrder.Target->GetPosition());
                }
            }
            break;
        }
        default:
        {
            char message[256];
            std::snprintf(message, sizeof(message), "Mover::handleTacticalOrder->Bad TacOrder Code (%d)",
                          static_cast<int>(tacOrder.Code));
            Assert(0, 1, message);
            tacOrder.Destroy();
            return 1;
        }
    }

    if (checkCanMove && IsDisabled() != 0 && CanMove() == 0)
    {
        radioMessageId = 0x1f;
        giveOrder = 0;
    }

    MCMechWarrior* vehiclePilot = Pilot;

    if (vehiclePilot != nullptr)
    {
        vehiclePilot->RadioMessage(radioMessageId, 1);
    }

    if (MPlayer != nullptr)
    {
        tacOrder.SetId(vehiclePilot);
    }

    if (giveOrder != 0)
    {
        switch (tacOrder.Origin)
        {
            case 0:
            {
                if (queuePlayerOrder != 0)
                {
                    vehiclePilot->AddQueuedTacOrder(tacOrder);
                    vehiclePilot->TacOrderQueueExecuting = 1;
                    tacOrder.Destroy();
                    return 0;
                }

                vehiclePilot->SetPlayerTacOrder(tacOrder, 0);
                break;
            }
            case 1:
            {
                vehiclePilot->SetGeneralTacOrder(tacOrder);
                tacOrder.Destroy();
                return 0;
            }
            case 2:
            {
                vehiclePilot->SetAlarmTacOrder(tacOrder, priority);
                tacOrder.Destroy();
                return 0;
            }
            default:
                break;
        }
    }

    tacOrder.Destroy();
    return 0;
}

auto MCMover::ReduceAntiMissileAmmo(int32_t numShots) -> void
{
    if (numShots > 0)
    {
        ReduceAmmo(MasterComponentList[Inventory[AntiMissileSystem[0]].MasterID].AmmoMasterId, numShots);
    }
}

auto MCMover::FireAntiMissileSystem(int32_t numMissiles, int32_t& antiMissileShots) -> int32_t
{
    for (int32_t i = 0; i < NumAntiMissileSystems; i++)
    {
        const MCInventoryItem& system = Inventory[AntiMissileSystem[i]];

        if (numMissiles <= 0 || AmmoTypeTotal[system.AmmoIndex].CurAmount <= 0)
        {
            continue;
        }

        // Each volley stops one to six missiles.
        const int32_t clan = system.MasterID == MasterClanAntiMissileSystemID ? 1 : 0;

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

auto MCMover::PilotingCheck(uint32_t, float) -> void
{
    PilotingCheckPending = 0;
}

auto MCMover::UpdateDamageTakenRate() -> void
{
    if (!(DamageRateCheckTime < ScenarioTime))
    {
        return;
    }

    const int32_t damageRate = static_cast<int32_t>(static_cast<double>(DamageRateTally) / DamageRateFrequency);

    if (damageRate > 10)
    {
        Pilot->TriggerAlarm(2, static_cast<uint32_t>(damageRate));
    }

    DamageRateTally = 0.0f;
    DamageRateCheckTime = DamageRateFrequency + DamageRateCheckTime;
}

auto MCMover::SetTeam(MCTeam* newTeam) -> int32_t
{
    Team = newTeam;
    SetAlignment(newTeam->Alignment);

    if (SensorSystem != nullptr)
    {
        SensorSystem->SetTeam(Team);
        SensorSystem->ScanFrequency = ContactUpdateFrequency;
    }

    if (Team != nullptr)
    {
        if (Ecm != 0xff)
        {
            EcmTracker = Team->AddEcm(this, Inventory[Ecm].MasterID);
        }

        if (Jammer != 0xff)
        {
            JammerTracker = Team->AddJammer(this, Inventory[Jammer].MasterID);
        }
    }

    if (Pilot != nullptr)
    {
        Pilot->SetTeam(newTeam);
    }

    return 0;
}

auto MCMover::SetGroup(MCMoverGroup* newGroup) -> int32_t
{
    Group = newGroup;

    if (newGroup != nullptr && Pilot != nullptr)
    {
        Pilot->ClearCurTacOrder(0, 0);
        Pilot->OrderState = ORDERSTATE_GENERAL;
    }

    return 0;
}

auto MCMover::SetPilot(MCMechWarrior* newPilot) -> void
{
    Pilot = newPilot;

    if (SensorSystem != nullptr)
    {
        SensorSystem->SetRange(SensorSystem->Range);
    }

    newPilot->Alignment = static_cast<int8_t>(Alignment);
    newPilot->SetVehicle(this);
}

auto MCMover::GetPoint() -> MCMover*
{
    if (Group != nullptr)
    {
        return Group->GetPoint();
    }

    return nullptr;
}

auto MCMover::ClearWeaponFireChunks(int32_t which) -> int32_t
{
    const int32_t numChunks = NumWeaponFireChunks[which];
    NumWeaponFireChunks[which] = 0;
    return numChunks;
}

auto MCMover::AddWeaponFireChunk(int32_t which, MCWeaponFireChunk* chunk) -> int32_t
{
    if (NumWeaponFireChunks[which] == MAX_WEAPONFIRE_CHUNKS)
    {
        Fatal(0, " Mover::addWeaponFireChunk--Too many weaponfire chunks ");
    }

    chunk->Pack();
    WeaponFireChunks[which][NumWeaponFireChunks[which]] = chunk->Data;
    NumWeaponFireChunks[which]++;
    return NumWeaponFireChunks[which];
}

auto MCMover::AddWeaponFireChunks(int32_t which, uint32_t* packedChunkBuffer, int32_t numChunks) -> int32_t
{
    if (NumWeaponFireChunks[which] + numChunks > MAX_WEAPONFIRE_CHUNKS - 1)
    {
        Fatal(0, " Mover::addWeaponFireChunks--Too many weaponfire chunks ");
    }

    for (int32_t i = 0; i < numChunks; i++)
    {
        WeaponFireChunks[which][NumWeaponFireChunks[which]] = packedChunkBuffer[i];
        NumWeaponFireChunks[which]++;
        // Unpacked into a scratch chunk (the result isn't kept).
        MCWeaponFireChunk chunk;
        chunk.Init();
        chunk.Data = packedChunkBuffer[i];
        chunk.Unpack(this);
    }

    return NumWeaponFireChunks[which];
}

auto MCMover::GrabWeaponFireChunks(int32_t which, uint32_t* packedChunkBuffer, int32_t maxChunks) -> int32_t
{
    const int32_t numChunks = NumWeaponFireChunks[which];
    const int32_t numGrabbed = maxChunks < numChunks ? maxChunks : numChunks;

    for (int32_t i = 0; i < numGrabbed; i++)
    {
        packedChunkBuffer[i] = WeaponFireChunks[which][i];
    }

    NumWeaponFireChunks[which] = numChunks - numGrabbed;
    return numGrabbed;
}

auto MCMover::UpdateWeaponFireChunks(int32_t which) -> int32_t
{
    // Replays the weapon fire the network sent: each chunk's shot, on its target.
    for (int32_t i = 0; i < NumWeaponFireChunks[which]; i++)
    {
        MCWeaponFireChunk chunk = {0, 0, 0, 0, 0, {0, 0}, 0, 0, 0, 0, 0, 0, -1, 0};
        chunk.Data = WeaponFireChunks[which][i];
        chunk.Unpack(this);
        CurMoverWeaponFireChunk = chunk;

        const int32_t weaponIndex = NumOther + chunk.WeaponIndex;

        if (IsWeaponIndex(weaponIndex) == 0)
        {
            continue;
        }

        TargetRolo = chunk.TargetType;

        switch (chunk.TargetType)
        {
            case 0:
            case 1:
            case 2:
            {
                MCBaseObject* target = nullptr;
                const char* missing = nullptr;

                if (chunk.TargetType == 0)
                {
                    target = MPlayer->MoverRoster[chunk.TargetId];
                    missing = " Mover.updateWeaponFireChunks: NULL Mover Target (save wfchunk.dbg file) ";
                }
                else
                {
                    target = ObjectList->FindObjectFromPart(chunk.TargetId);
                    missing = chunk.TargetType == 1
                                  ? " Mover.updateWeaponFireChunks: NULL Terrain Target (save wfchunk.dbg file) "
                                  : " Mover.updateWeaponFireChunks: NULL Special Target (save wfchunk.dbg file) ";
                }

                if (target == nullptr)
                {
                    DebugWeaponFireChunk(&chunk, nullptr, this);
                    Assert(0, 0, missing);
                }

                HandleWeaponFire(weaponIndex, static_cast<MCGameObject*>(target), nullptr, chunk.Hit,
                                 EntryAngleTable[chunk.EntryAngle], chunk.NumMissiles, chunk.NumMissilesPastAms,
                                 chunk.NumAntiMissileShots, chunk.HitLocation);
                break;
            }

            case 3:
            {
                // A point on the ground: the middle of the target cell.
                const float halfSide = WorldUnitsMapSide * 0.5f;
                MCVector3D point;
                point.X =
                    static_cast<float>((chunk.TargetCell[1] + 0.5f) * static_cast<double>(MetersPerCell) - halfSide);
                point.Y = static_cast<float>(
                    (static_cast<double>(halfSide) - chunk.TargetCell[0] * static_cast<double>(MetersPerCell)) -
                    static_cast<double>(MetersPerCell) * 0.5f);
                point.Z = 0.0f;
                point.Z = GameMap->GetTerrainElevation(point);
                HandleWeaponFire(weaponIndex, nullptr, &point, chunk.Hit, 0.0f, chunk.NumMissiles,
                                 chunk.NumMissilesPastAms, 0, 0);
                break;
            }

            default:
                Fatal(0, " Mover.updateWeaponFireChunks: bad targetType ");
        }
    }

    NumWeaponFireChunks[which] = 0;
    return 0;
}

auto MCMover::ClearCriticalHitChunks(int32_t which) -> int32_t
{
    const int32_t numChunks = NumCriticalHitChunks[which];
    NumCriticalHitChunks[which] = 0;
    return numChunks;
}

auto MCMover::AddCriticalHitChunk(int32_t which, int32_t bodyLocation, int32_t criticalSpace) -> int32_t
{
    if (NumCriticalHitChunks[which] == MAX_WEAPONFIRE_CHUNKS)
    {
        Fatal(0, " Mover::addCriticalHitChunk--Too many criticalhit chunks ");
    }

    CriticalHitChunks[which][NumCriticalHitChunks[which]] = static_cast<uint8_t>(bodyLocation * 16 + criticalSpace);
    NumCriticalHitChunks[which]++;
    return NumCriticalHitChunks[which];
}

auto MCMover::AddCriticalHitChunks(int32_t which, uint8_t* packedChunkBuffer, int32_t numChunks) -> int32_t
{
    if (NumCriticalHitChunks[which] + numChunks > MAX_WEAPONFIRE_CHUNKS - 1)
    {
        Fatal(0, " Mover::addCriticalHitChunks--Too many criticalhit chunks ");
    }

    std::memcpy(&CriticalHitChunks[which][NumCriticalHitChunks[which]], packedChunkBuffer,
                static_cast<size_t>(numChunks));
    NumCriticalHitChunks[which] += numChunks;
    return NumCriticalHitChunks[which];
}

auto MCMover::GrabCriticalHitChunks(int32_t which, uint8_t* packedChunkBuffer) -> int32_t
{
    const int32_t numChunks = NumCriticalHitChunks[which];

    if (numChunks > 0)
    {
        std::memcpy(packedChunkBuffer, CriticalHitChunks[which], static_cast<size_t>(numChunks));
    }

    return numChunks;
}

auto MCMover::UpdateCriticalHitChunks(int32_t which) -> int32_t
{
    NumCriticalHitChunks[which] = 0;
    return 0;
}

auto MCMover::ClearRadioChunks(int32_t which) -> int32_t
{
    const int32_t numChunks = NumRadioChunks[which];
    NumRadioChunks[which] = 0;
    return numChunks;
}

auto MCMover::AddRadioChunk(int32_t which, uint8_t msg) -> int32_t
{
    if (NumRadioChunks[which] == MAX_RADIO_CHUNKS)
    {
        return MAX_RADIO_CHUNKS;
    }

    RadioChunks[which][NumRadioChunks[which]] = msg;
    NumRadioChunks[which]++;
    return NumRadioChunks[which];
}

auto MCMover::AddRadioChunks(int32_t which, uint8_t* packedChunkBuffer, int32_t numChunks) -> int32_t
{
    for (int32_t i = 0; i < numChunks; i++)
    {
        AddRadioChunk(which, packedChunkBuffer[i]);
    }

    return NumRadioChunks[which];
}

auto MCMover::GrabRadioChunks(int32_t which, uint8_t* packedChunkBuffer) -> int32_t
{
    const int32_t numChunks = NumRadioChunks[which];

    if (numChunks > 0)
    {
        std::memcpy(packedChunkBuffer, RadioChunks[which], static_cast<size_t>(numChunks));
    }

    return numChunks;
}

auto MCMover::UpdateRadioChunks(int32_t which) -> int32_t
{
    if (NetPlayerId >= 0)
    {
        for (int32_t i = 0; i < NumRadioChunks[which]; i++)
        {
            PlayMessage(static_cast<MCRadioMessageType>(RadioChunks[which][i]), 0);
        }
    }

    NumRadioChunks[which] = 0;
    return 0;
}

auto MCMover::PlayMessage(MCRadioMessageType messageId, int propogateIfMultiplayer) -> void
{
    if (Pilot != nullptr)
    {
        Pilot->RadioMessage(messageId, propogateIfMultiplayer);
    }
}

namespace
{
    /// <summary>Whether <paramref name="bits"/> shows any corner of the tile the object stands on.</summary>
    int TileVisible(MCByteFlag* bits, const MCObjectPosition* objPosition)
    {
        const uint32_t row = static_cast<uint32_t>(objPosition->TileR);
        const uint32_t col = static_cast<uint32_t>(objPosition->TileC);

        if (bits->GetFlag(row, col) != 0)
        {
            return 1;
        }

        if (bits->GetFlag(row + 1, col) != 0)
        {
            return 1;
        }

        if (bits->GetFlag(row + 1, col + 1) != 0)
        {
            return 1;
        }

        return bits->GetFlag(row, col + 1) != 0 ? 1 : 0;
    }
}

auto MCMover::IsRevealed() -> int
{
    // The home side's visibility bits (the names are the original's, swapped).
    MCByteFlag* bits = HomeTeam->Alignment != -1 ? MCTerrain::TerrainVisibleBits : MCTerrain::ClanVisibleBits;
    return TileVisible(bits, ObjPosition);
}

auto MCMover::EnemyRevealed() -> int
{
    MCByteFlag* bits = HomeTeam->Alignment != -1 ? MCTerrain::ClanVisibleBits : MCTerrain::TerrainVisibleBits;
    return TileVisible(bits, ObjPosition);
}

auto MCMover::GetDamageClass(int32_t& damageClass, int& shutDown) -> void
{
    const double quotient = static_cast<double>(CurCV) / MaxCV;
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

    shutDown = Status == 5 ? 1 : 0;
}

auto MCMover::GetInventoryDamage(int32_t itemIndex) -> int32_t
{
    if (itemIndex >= NumAmmos + NumWeapons + NumOther)
    {
        return 0;
    }

    const MCInventoryItem& item = Inventory[itemIndex];
    return static_cast<int8_t>(MasterComponentList[item.MasterID].Health) - item.Health;
}

auto MCMover::GetEcmEffect() -> float
{
    if (Ecm != 0xff && Inventory[Ecm].Disabled == 0)
    {
        return MasterComponentList[Inventory[Ecm].MasterID].Damage;
    }

    return 0.0f;
}

auto MCMover::GetProbeEffect() -> float
{
    if (Probe != 0xff && Inventory[Probe].Disabled == 0)
    {
        return MasterComponentList[Inventory[Probe].MasterID].RangeOrHeat;
    }

    return 0.0f;
}

auto MCMover::GetVisualRange() -> float
{
    return GetProbeEffect() + MaxVisualRadius;
}

auto MCMover::CalcOffsetMoveGoal(MCVector3D target, MCVector3D offset, MCVector3D& goal) -> int32_t
{
    // Half a map cell per step, from the offset point toward the target.
    float directionX = target.X - offset.X;
    float directionY = target.Y - offset.Y;
    const float length = static_cast<float>(
        std::sqrt(static_cast<double>(directionX) * directionX + static_cast<double>(directionY) * directionY));

    if (length != 0.0f)
    {
        directionX = directionX / length;
        directionY = directionY / length;
    }

    const float stepX =
        static_cast<float>(static_cast<double>(directionX) * MCTerrain::MetersPerVertexDivMapcellDim * 0.5);
    const float stepY =
        static_cast<float>(static_cast<double>(directionY) * MCTerrain::MetersPerVertexDivMapcellDim * 0.5);

    if (std::sqrt(static_cast<double>(stepX) * stepX + static_cast<double>(stepY) * stepY) == 0.0)
    {
        goal = target;
        return 0;
    }

    MCVector3D away = offset - target;
    const float maxDistance = static_cast<float>(away.Magnitude());
    float x = offset.X;
    float y = offset.Y;

    // Whether the cell under (x, y) is passable.
    auto cellPassable = [&]()
    {
        MCVector3D point;
        point.X = x;
        point.Y = y;
        point.Z = 0.0f;
        int32_t tileR;
        int32_t tileC;
        int32_t cellR;
        int32_t cellC;
        GameMap->WorldToMapPos(point, tileR, tileC, cellR, cellC);

        // Port fix: the walk can leave the map, where the original reads outside it. Off the map is impassable.
        if (!GameMap->OnMap(tileR, tileC))
        {
            return 0u;
        }

        return GameMap->Map[GameMap->Width * tileR + tileC].GetCellPassable(cellR, cellC);
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
            const double dx = static_cast<double>(x) - target.X;
            const double dy = static_cast<double>(y) - target.Y;
            traveled = static_cast<float>(std::sqrt(dx * dx + dy * dy));
        } while (lastPassable == 0);
    }

    MCVector3D ground;
    ground.X = x;
    ground.Y = y;
    ground.Z = 0.0f;
    goal.X = x;
    goal.Y = y;
    goal.Z = GameMap->GetTerrainElevation(ground);
    return 0;
}

auto MCMover::SetChallenger(MCGameObject* newChallenger) -> void
{
    Challenger = newChallenger;
}

auto MCMover::GetChallenger() -> MCGameObject*
{
    MCGameObject* current = Challenger;

    if (current != nullptr && current->IsDisabled() != 0)
    {
        Challenger = nullptr;
        return nullptr;
    }

    return current;
}

auto MCMover::CalcMoveGoal(MCGameObject* target, MCVector3D moveGoal, int32_t isGroup, int32_t offsetIndex,
                           int32_t groupSize, int32_t pointIndex, MCVector3D& newGoal, uint32_t params) -> int32_t
{
    // 0x800: no fire range ring and no bonus around the goal itself.
    const uint32_t noRangeRing = (params >> 11) & 1;

    // 0x20: go straight to the goal.
    if ((params & 0x20) != 0)
    {
        newGoal = moveGoal;
        return 0;
    }

    int32_t* goal = &GoalMap[0][0];
    std::memset(GoalMap, 0, sizeof(GoalMap));

    // 0x400: one and a half vertices toward the goal.
    if ((params & 0x400) != 0)
    {
        const float dx = moveGoal.X - Position.X;
        const float dy = moveGoal.Y - Position.Y;
        const double length = std::sqrt(static_cast<double>(dy) * dy + static_cast<double>(dx) * dx);

        if (length <= 0.0)
        {
            return 0;
        }

        const float lengthF = static_cast<float>(length);
        const double stepLength = static_cast<double>(MCTerrain::MetersPerVertex) * 1.5;
        MCVector3D step;
        step.X = static_cast<float>(static_cast<double>(dx / lengthF) * stepLength);
        step.Y = static_cast<float>(stepLength * (dy / lengthF));
        step.Z = 0.0f;
        // The original takes the elevation of the step itself, not of the point it reaches.
        const float elevation = GameMap->GetTerrainElevation(step);
        MCVector3D stepGoal;
        stepGoal.X = step.X + Position.X;
        stepGoal.Y = step.Y + Position.Y;
        stepGoal.Z = elevation + Position.Z;
        CalcOffsetMoveGoal(Position, stepGoal, newGoal);
        return 0;
    }

    // The map covers the 13x13 tiles around the goal's tile.
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    GameMap->WorldToMapPos(moveGoal, tileR, tileC, cellR, cellC);
    const int32_t goalCellR = tileR * MAPCELL_DIM + cellR;
    const int32_t goalCellC = tileC * MAPCELL_DIM + cellC;
    const int32_t mapTileR0 = tileR - 6;
    const int32_t mapTileC0 = tileC - 6;
    const int32_t mapCellR0 = mapTileR0 * MAPCELL_DIM;
    const int32_t mapCellC0 = mapTileC0 * MAPCELL_DIM;

    if (target == nullptr)
    {
        CalcOffsetMoveGoal(Position, moveGoal, newGoal);
        return 0;
    }

    const MCObjectClass targetClass = target->ObjectClass;

    if (targetClass == BATTLEMECH || targetClass == GROUNDVEHICLE || targetClass == ELEMENTAL || targetClass == MOVER)
    {
        // The facing is computed and dropped.
        MCVector3D targetPosition = target->GetPosition();
        targetPosition.Y = static_cast<float>(targetPosition.Y + 50.0);
        target->RelFacingTo(targetPosition, -1);
    }

    const int32_t* overlayWeights = &OverlayWeightTable[OverlayWeightClass * OVERLAY_WEIGHT_LEVEL_SIZE];

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

    const int32_t orderCode = Pilot->CurTacOrder.Code;
    const bool attacking = orderCode == TACTICAL_ORDER_ATTACK_OBJECT || orderCode == TACTICAL_ORDER_GUARD;
    int32_t attackRange = -5;

    if (attacking)
    {
        attackRange = Pilot->CurTacOrder.AttackParams.Range;
    }

    int32_t ringRange = 2;

    if (attacking)
    {
        const float cellMeters = MetersPerWorldUnit * MCTerrain::MetersPerVertexDivMapcellDim;

        // Within the longest fire range (less 3 cells) is good.
        if (noRangeRing == 0 && attackRange != 0 && attackRange != 1 && attackRange != 2)
        {
            int32_t radius = static_cast<int32_t>(static_cast<double>(GetFireRange(-2)) / cellMeters) - 3;

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
        const float orderFireRange = Pilot->OrderFireRange;

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

            int32_t radius = static_cast<int32_t>(static_cast<double>(MaxMinRange) / cellMeters + 1.0f);

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
                GoalMap[ringRow][ringCol] += 500;
            }
        }
    }

    // Where the mover stands, clamped to the map.
    int32_t myTileR;
    int32_t myTileC;
    int32_t myCellR;
    int32_t myCellC;
    GameMap->WorldToMapPos(Position, myTileR, myTileC, myCellR, myCellC);
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
        GoalMap[myRow][myCol] -= 10000;
    }

    // Farther from the mover is worse.
    for (int32_t row = 0; row < GOALMAP_CELL_DIM; row++)
    {
        for (int32_t col = 0; col < GOALMAP_CELL_DIM; col++)
        {
            const int32_t rowDistance = row > myRow ? row - myRow : myRow - row;
            const int32_t colDistance = col > myCol ? col - myCol : myCol - col;
            GoalMap[row][col] -= rowDistance + colDistance;
        }
    }

    // The cells the group mates are heading for.
    MCMover* movers[MAX_MOVERGROUP_COUNT];

    if (Group != nullptr)
    {
        const int32_t numMovers = Group->GetMovers(movers);

        for (int32_t i = 0; i < numMovers; i++)
        {
            if (movers[i] == this)
            {
                continue;
            }

            MCMechWarrior* matePilot = movers[i]->GetPilot();

            if (matePilot == nullptr || matePilot->MoveOrders.PathType == 0)
            {
                continue;
            }

            int32_t mateCellR;
            int32_t mateCellC;
            WorldCoordToMapCell(matePilot->MoveOrders.OriginalGlobalGoal[1], mateCellR, mateCellC);
            mateCellR -= mapCellR0;
            mateCellC -= mapCellC0;

            if (mateCellR > -1 && mateCellR < GOALMAP_CELL_DIM && mateCellC > -1 && mateCellC < GOALMAP_CELL_DIM)
            {
                GoalMap[mateCellR][mateCellC] -= 100;
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
            int32_t* block = &GoalMap[tileRow * MAPCELL_DIM][cellCol];

            if (mapR <= -1 || mapR >= GameMap->Height || mapC <= -1 || mapC >= GameMap->Width)
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

            Assert(mapR < GameMap->Height && mapC < GameMap->Width, 0, " Map Tile out of bounds ");
            MCMapTile tile = GameMap->Map[GameMap->Width * mapR + mapC];

            for (int32_t row = 0; row < MAPCELL_DIM; row++)
            {
                for (int32_t col = 0; col < MAPCELL_DIM; col++)
                {
                    if (tile.GetCellPassable(row, col) == 0)
                    {
                        block[row * GOALMAP_CELL_DIM + col] -= 10000;
                    }
                }
            }

            const uint32_t overlayType = tile.Overlay & 0x7f;

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
        int32_t Row = 0;
        int32_t Col = 0;
        int32_t Value = 0;
    };

    GoalCandidate best[20];

    for (GoalCandidate& candidate : best)
    {
        candidate = {-1, -1, -999999};
    }

    for (int32_t index = 0; index < GOALMAP_CELL_DIM * GOALMAP_CELL_DIM; index++)
    {
        const int32_t value = goal[index];

        if (index >= 20 && value <= best[19].Value)
        {
            continue;
        }

        int32_t slot = 18;

        while (slot > -1 && value >= best[slot].Value)
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
    if (ObjectClass == ELEMENTAL && Group != nullptr)
    {
        const int32_t numMovers = Group->GetMovers(movers);

        for (int32_t i = 0; i < numMovers; i++)
        {
            if (movers[i] == this)
            {
                break;
            }

            MCMechWarrior* matePilot = movers[i]->GetPilot();

            if (matePilot != nullptr)
            {
                matePilot->GetLastTarget();
            }
        }
    }

    // The best cell with a line of fire to the goal (else the 20th).
    const double halfMapSide = static_cast<double>(WorldUnitsMapSide) * 0.5f;
    int32_t goalRow = mapTileR0;
    int32_t goalCol = mapTileC0;
    target->ClearLineOfFire();

    for (int32_t i = 0; i < 20; i++)
    {
        goalCol = best[i].Col;
        goalRow = best[i].Row;
        MCVector3D cellCenter;
        cellCenter.X =
            static_cast<float>((static_cast<double>(goalCol + mapCellC0) + 0.5f) * MetersPerCell - halfMapSide);
        cellCenter.Y = static_cast<float>((halfMapSide - static_cast<double>(goalRow + mapCellR0) * MetersPerCell) -
                                          static_cast<double>(MetersPerCell) * 0.5f);
        cellCenter.Z = 0.0f;

        if (GameMap->LineOfFire(cellCenter, moveGoal) != 0)
        {
            break;
        }
    }

    goalRow += mapCellR0;
    goalCol += mapCellC0;
    target->RestoreLineOfFire();

    newGoal.X = static_cast<float>((static_cast<double>(goalCol) + 0.5) * MetersPerCell - halfMapSide);
    newGoal.Y = static_cast<float>(halfMapSide - (static_cast<double>(goalRow) + 0.5) * MetersPerCell);
    newGoal.Z = Land->GetTerrainElevation(newGoal);
    CalcOffsetMoveGoal(Position, newGoal, newGoal);
    return 0;
}

auto MCMover::CalcMovePath(MCMovePath* path, int32_t pathType, MCVector3D start, MCVector3D goal, int32_t* goalCell,
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
    GameMap->WorldToMapPos(start, startTileR, startTileC, startCellR, startCellC);
    int32_t goalTileR;
    int32_t goalTileC;
    int32_t goalCellR;
    int32_t goalCellC;
    GameMap->WorldToMapPos(goal, goalTileR, goalTileC, goalCellR, goalCellC);
    path->Clear();

    int32_t numOffsets;
    int32_t jumpCost;
    int32_t* overlayWeights = &OverlayWeightTable[OverlayWeightClass * OVERLAY_WEIGHT_LEVEL_SIZE];

    if (pathType == 1)
    {
        // A simple path: the window of SimpleMovePathRange tiles around the start.
        int32_t uLr = startTileR - SimpleMovePathRange;

        if (uLr < 0)
        {
            uLr = 0;
        }

        int32_t uLc = startTileC - SimpleMovePathRange;

        if (uLc < 0)
        {
            uLc = 0;
        }

        if (MaxRunSpeed == 0.0f)
        {
            return 0;
        }

        const int32_t moveLevel = LocalPathMoveLevel(MaxRunSpeed);

        if (moveLevel <= 0)
        {
            return 0;
        }

        SetUpPathJumps(this, numOffsets, jumpCost);
        const int32_t dim = SimpleMovePathRange * 2 + 1;
        PathFindMap->SetUp(GameMap, uLr, uLc, dim, dim, &start, (startTileR - uLr) * MAPCELL_DIM + startCellR,
                           (startTileC - uLc) * MAPCELL_DIM + startCellC, goal,
                           (goalTileR - uLr) * MAPCELL_DIM + goalCellR, (goalTileC - uLc) * MAPCELL_DIM + goalCellC,
                           overlayWeights, moveLevel, jumpCost, numOffsets, params);
        DebugMovePathType = 1;
        // The caller's goalCell is left alone.
        int32_t simpleGoalCell[2];
        const int32_t result = PathFindMap->CalcPath(path, nullptr, simpleGoalCell);
        JumpOnBlocked = 0;
        return result;
    }

    // Within the start's sector of the global map.
    if (MaxRunSpeed == 0.0f)
    {
        return 0;
    }

    const int32_t moveLevel = static_cast<int32_t>(static_cast<double>(MetersPerWorldUnit) *
                                                   MCTerrain::MetersPerVertexDivMapcellDim / MaxRunSpeed * 50.0);

    if (moveLevel <= 0)
    {
        return 0;
    }

    const int32_t sectorDim = GlobalMoveMap->SectorDim;
    const int32_t uLr = (startTileR / sectorDim) * sectorDim;
    const int32_t uLc = (startTileC / sectorDim) * sectorDim;
    SetUpPathJumps(this, numOffsets, jumpCost);
    PathFindMap->SetUp(GameMap, uLr, uLc, GlobalMoveMap->SectorDim, GlobalMoveMap->SectorDim, &start,
                       (startTileR - uLr) * MAPCELL_DIM + startCellR, (startTileC - uLc) * MAPCELL_DIM + startCellC,
                       goal, (goalTileR - uLr) * MAPCELL_DIM + goalCellR, (goalTileC - uLc) * MAPCELL_DIM + goalCellC,
                       overlayWeights, moveLevel, jumpCost, numOffsets, params);
    DebugMovePathType = pathType;
    const int32_t result = PathFindMap->CalcPath(path, nullptr, goalCell);
    JumpOnBlocked = 0;
    return result;
}

auto MCMover::CalcEscapePath(MCMovePath* path, MCVector3D start, MCVector3D goal, int32_t* goalCell, uint32_t params,
                             MCVector3D& escapeGoal) -> int32_t
{
    escapeGoal.X = -999999.0f;
    escapeGoal.Y = -999999.0f;
    escapeGoal.Z = -999999.0f;

    if (PathFindMap == nullptr)
    {
        Fatal(0, " No PathFindMap in Mover::calcMovePath ");
    }

    int32_t startTileR;
    int32_t startTileC;
    int32_t startCellR;
    int32_t startCellC;
    GameMap->WorldToMapPos(start, startTileR, startTileC, startCellR, startCellC);
    int32_t goalTileR;
    int32_t goalTileC;
    int32_t goalCellR;
    int32_t goalCellC;
    GameMap->WorldToMapPos(goal, goalTileR, goalTileC, goalCellR, goalCellC);
    path->Clear();

    int32_t uLr = startTileR - SimpleMovePathRange;

    if (uLr < 0)
    {
        uLr = 0;
    }

    int32_t uLc = startTileC - SimpleMovePathRange;

    if (uLc < 0)
    {
        uLc = 0;
    }

    if (MaxRunSpeed == 0.0f)
    {
        return 0;
    }

    const int32_t moveLevel = LocalPathMoveLevel(MaxRunSpeed);

    if (moveLevel <= 0)
    {
        return 0;
    }

    int32_t numOffsets;
    int32_t jumpCost;
    SetUpPathJumps(this, numOffsets, jumpCost);
    const int32_t dim = SimpleMovePathRange * 2 + 1;
    FindingEscapePath = 1;
    PathFindMap->SetUp(GameMap, uLr, uLc, dim, dim, &start, (startTileR - uLr) * MAPCELL_DIM + startCellR,
                       (startTileC - uLc) * MAPCELL_DIM + startCellC, goal, (goalTileR - uLr) * MAPCELL_DIM + goalCellR,
                       (goalTileC - uLc) * MAPCELL_DIM + goalCellC,
                       &OverlayWeightTable[OverlayWeightClass * OVERLAY_WEIGHT_LEVEL_SIZE], moveLevel, jumpCost,
                       numOffsets, params);
    DebugMovePathType = 0;
    // goalCell is unused: the escape goal cell goes to a local.
    int32_t escapeGoalCell[2];
    const int32_t result = PathFindMap->CalcEscapePath(path, &escapeGoal, escapeGoalCell);
    JumpOnBlocked = 0;
    FindingEscapePath = 0;
    return result;
}

auto MCMover::GetAdjacentCellPathLocked(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, int32_t dir) -> int
{
    const int32_t* adjCell = AdjCellTable[cellR * MAPCELL_DIM + cellC][dir];
    return GameMap->Map[(adjCell[0] + tileR) * GameMap->Width + adjCell[1] + tileC].GetCellPathLocked(adjCell[2],
                                                                                                      adjCell[3]) != 0;
}

auto MCMover::GetPathLocked(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, int32_t diameter) -> int
{
    if (diameter == 1)
    {
        return GameMap->Map[GameMap->Width * tileR + tileC].GetCellPathLocked(cellR, cellC) != 0;
    }

    if (diameter != 3)
    {
        if (diameter == 5)
        {
            return 0;
        }

        Fatal(0, " Bad PathLock Radius ");
    }

    return VisitCellsAround(tileR, tileC, cellR, cellC, [](MCMapTile& tile, int32_t row, int32_t col)
                            { return tile.GetCellPathLocked(row, col) != 0; });
}

auto MCMover::SetPathLock(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC, int set, int32_t diameter) -> void
{
    const uint32_t locked = set != 0 ? 1 : 0;

    if (diameter == 1)
    {
        GameMap->Map[GameMap->Width * tileR + tileC].SetCellPathLocked(cellR, cellC, locked);
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
                     [locked](MCMapTile& tile, int32_t row, int32_t col)
                     {
                         tile.SetCellPathLocked(row, col, locked);
                         return false;
                     });
}

auto MCMover::GetPathRangeLock(int32_t range, int* reachedEnd) -> int
{
    MCMovePath* path = Pilot->GetMovePath();

    if (path != nullptr)
    {
        return path->IsLocked(-1, range, reachedEnd);
    }

    return 0;
}

auto MCMover::SetPathRangeLock(int set, int32_t range) -> int32_t
{
    MCMovePath* path = Pilot->GetMovePath();

    if (set == 0)
    {
        for (int32_t i = 0; i < NumPathRangeLocks; i++)
        {
            const int32_t* lock = PathRangeLocks[i];
            GameMap->Map[lock[0] * GameMap->Width + lock[1]].SetCellPathLocked(lock[2], lock[3], 0);
        }

        NumPathRangeLocks = 0;
        return 0;
    }

    if (NumPathRangeLocks > 0)
    {
        SetPathRangeLock(0, 0);
    }

    if (path == nullptr || path->NumSteps <= 0)
    {
        return 0;
    }

    int32_t lastStep = path->CurStep + range;

    if (path->NumStepsWhenNotPaused <= lastStep)
    {
        lastStep = path->NumStepsWhenNotPaused;
    }

    NumPathRangeLocks = 0;

    for (int32_t step = path->CurStep; step < lastStep; step++)
    {
        const MCPathStep& pathStep = path->StepList[step];
        MCMapTile& tile = GameMap->Map[pathStep.TileR * GameMap->Width + pathStep.TileC];

        // Someone else holds the cell: the cells locked so far stay locked.
        if (tile.GetCellPathLocked(pathStep.CellR, pathStep.CellC) != 0)
        {
            return -1;
        }

        tile.SetCellPathLocked(pathStep.CellR, pathStep.CellC, 1);
        PathRangeLocks[NumPathRangeLocks][0] = pathStep.TileR;
        PathRangeLocks[NumPathRangeLocks][1] = pathStep.TileC;
        PathRangeLocks[NumPathRangeLocks][2] = pathStep.CellR;
        PathRangeLocks[NumPathRangeLocks][3] = pathStep.CellC;
        NumPathRangeLocks++;
    }

    return 0;
}

auto MCMover::UpdatePathLock(int set) -> void
{
    // Not while a mech is in the air.
    if (ObjectClass == BATTLEMECH && static_cast<MCBattleMech*>(this)->InJump != 0)
    {
        return;
    }

    MCObjectPosition* objectPosition = ObjPosition;

    if (objectPosition != nullptr)
    {
        SetPathLock(objectPosition->TileR, objectPosition->TileC, objectPosition->CellR, objectPosition->CellC, set,
                    PathLockLevel);
    }

    Pilot->GetMovePath();

    if (set == 0 || Pilot->MoveOrders.YieldTime <= -1.0f)
    {
        SetPathRangeLock(set, PathLockRange);
    }
}

auto MCMover::GetPathRangeBlocked(int32_t range, int* reachedEnd) -> int
{
    MCMovePath* path = Pilot->GetMovePath();

    if (path != nullptr)
    {
        return path->IsBlocked(-1, range, reachedEnd);
    }

    return 0;
}

auto MCMover::UpdateHustleTime() -> void
{
    const MCObjectPosition* objectPosition = ObjPosition;

    switch (GameMap->Map[objectPosition->TileR * GameMap->Width + objectPosition->TileC].Overlay & 0x7f)
    {
        case 0x25:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x37:
        case 0x38:
        case 0x39:
        case 0x3a:
            LastHustleTime = ScenarioTime;
            break;
        default:
            break;
    }
}

auto MCMover::BounceToAdjCell() -> int32_t
{
    // The first neighbour that is passable, affordable and not path locked.
    int32_t dir = 0;
    int32_t adjTileR;
    int32_t adjTileC;
    int32_t adjCellR;
    int32_t adjCellC;

    while (true)
    {
        const MCObjectPosition* objectPosition = ObjPosition;
        const int32_t* adjCell = AdjCellTable[objectPosition->CellR * MAPCELL_DIM + objectPosition->CellC][dir];
        adjTileR = adjCell[0] + objectPosition->TileR;
        adjTileC = adjCell[1] + objectPosition->TileC;
        adjCellR = adjCell[2];
        adjCellC = adjCell[3];
        // The tile's words are read before the overlay weight is.
        MCMapTile tile = GameMap->Map[GameMap->Width * adjTileR + adjTileC];
        uint32_t passable = tile.GetCellPassable(adjCellR, adjCellC);

        if (GameMap->GetOverlayWeight(adjTileR, adjTileC, adjCellR, adjCellC, this) > 9999)
        {
            passable = 0;
        }

        if (tile.GetCellPathLocked(adjCellR, adjCellC) == 0 && passable != 0)
        {
            break;
        }

        dir++;

        if (dir > 7)
        {
            return -1;
        }
    }

    const MCObjectPosition* objectPosition = ObjPosition;
    const uint32_t wasLocked =
        GameMap->Map[objectPosition->TileR * GameMap->Width + objectPosition->TileC].GetCellPathLocked(
            objectPosition->CellR, objectPosition->CellC);

    if (wasLocked != 0)
    {
        UpdatePathLock(0);
    }

    const double halfMapSide = static_cast<double>(WorldUnitsMapSide) * 0.5f;
    MCVector3D cellCenter;
    cellCenter.X = static_cast<float>((static_cast<double>(adjCellC + adjTileC * MAPCELL_DIM) + 0.5f) * MetersPerCell -
                                      halfMapSide);
    cellCenter.Y =
        static_cast<float>((halfMapSide - static_cast<double>(adjCellR + adjTileR * MAPCELL_DIM) * MetersPerCell) -
                           static_cast<double>(MetersPerCell) * 0.5f);
    cellCenter.Z = 0.0f;
    SetPosition(cellCenter);
    GameObjectMap->UpdateObject(this, 0);
    Pilot->PausePath();

    if (wasLocked != 0)
    {
        UpdatePathLock(1);
    }

    return dir;
}

auto MCMover::CalcMovePath(MCMovePath* path, MCVector3D start, int32_t thruArea, int32_t goalDoor, MCVector3D finalGoal,
                           MCVector3D* goal, int32_t* goalCell, uint32_t params) -> int32_t
{
    if (PathFindMap == nullptr)
    {
        Fatal(0, " No PathFindMap in Mover::calcMovePath ");
    }

    // Within the sector of the area the path goes through.
    const MCGlobalMapArea& area = GlobalMoveMap->Areas[thruArea];
    const int32_t uLr = area.SectorR * GlobalMoveMap->SectorDim;
    const int32_t uLc = area.SectorC * GlobalMoveMap->SectorDim;
    path->Clear();

    if (MaxRunSpeed == 0.0f)
    {
        return 0;
    }

    const int32_t moveLevel = static_cast<int32_t>(static_cast<double>(MetersPerWorldUnit) *
                                                   MCTerrain::MetersPerVertexDivMapcellDim / MaxRunSpeed * 50.0);

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
    GameMap->WorldToMapPos(start, startTileR, startTileC, startCellR, startCellC);
    const int32_t sectorDim = GlobalMoveMap->SectorDim;

    if (PathFindMap->SetUp(GameMap, uLr, uLc, sectorDim, sectorDim, &start,
                           (startTileR - uLr) * MAPCELL_DIM + startCellR, (startTileC - uLc) * MAPCELL_DIM + startCellC,
                           thruArea, goalDoor, finalGoal,
                           &OverlayWeightTable[OverlayWeightClass * OVERLAY_WEIGHT_LEVEL_SIZE], moveLevel, jumpCost,
                           numOffsets, params) == -1)
    {
        JumpOnBlocked = 0;
        return -999;
    }

    const int32_t result = PathFindMap->CalcPath(path, goal, goalCell);
    JumpOnBlocked = 0;
    return result;
}

auto MCMover::GetContacts(int32_t* contactList, int32_t contactCriteria, int32_t sortType) -> int32_t
{
    return Team->GetContacts(this, contactList, contactCriteria, sortType);
}

auto MCMover::WeaponLocked(int32_t weaponIndex, MCVector3D targetPosition) -> float
{
    return RelFacingTo(targetPosition, -1);
}

auto MCMover::WeaponInRange(int32_t weaponIndex, float metersToTarget) -> int32_t
{
    const MCMasterComponent& weapon = MasterComponentList[Inventory[weaponIndex].MasterID];

    if (metersToTarget <= weapon.WeaponRange[0])
    {
        return 0;
    }

    if (metersToTarget <= weapon.WeaponRange[1])
    {
        return 2;
    }

    if (metersToTarget <= weapon.WeaponRange[2])
    {
        return 3;
    }

    return weapon.WeaponRange[3] < metersToTarget ? 0 : 4;
}

auto MCMover::GetWeaponsReady(int32_t* list, int32_t listSize) -> int32_t
{
    int32_t numReady = 0;

    if (listSize == -1)
    {
        for (int32_t i = NumOther; i < NumOther + NumWeapons; i++)
        {
            if (IsWeaponReady(i) != 0)
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

            if (IsWeaponReady(weaponIndex) != 0)
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

auto MCMover::GetWeaponsLocked(int32_t* list, int32_t listSize) -> int32_t
{
    MCGameObject* target = Pilot->GetLastTarget();

    if (target == nullptr)
    {
        return -2;
    }

    const MCVector3D targetPosition = target->GetPosition();
    int32_t numLocked = 0;
    const float fireArc = GetFireArc();
    const float negFireArc = -fireArc;

    if (listSize == -1)
    {
        for (int32_t i = NumOther; i < NumOther + NumWeapons; i++)
        {
            const float facing = WeaponLocked(i, targetPosition);

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
            const float facing = WeaponLocked(weaponIndex, targetPosition);

            if (negFireArc <= facing && facing <= fireArc)
            {
                list[numLocked] = weaponIndex;
                numLocked++;
            }
        }
    }

    return numLocked;
}

auto MCMover::GetWeaponsInRange(int32_t* list, int32_t listSize, float orderFireRange) -> int32_t
{
    MCGameObject* target = Pilot->GetLastTarget();

    if (target == nullptr)
    {
        return -2;
    }

    MCVector3D targetPosition = target->GetPosition();
    const float metersToTarget = static_cast<float>(DistanceFrom(targetPosition));
    int32_t numInRange = 0;

    if (listSize == -1)
    {
        for (int32_t i = NumOther; i < NumOther + NumWeapons; i++)
        {
            if (WeaponInRange(i, metersToTarget) != 0)
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

            if (WeaponInRange(weaponIndex, metersToTarget) != 0)
            {
                list[numInRange] = weaponIndex;
                numInRange++;
            }
        }
    }

    return numInRange;
}

auto MCMover::GetWeaponShots(int32_t weaponIndex) -> int32_t
{
    if (IsWeaponIndex(weaponIndex) == 0)
    {
        return -1;
    }

    // Weapons without ammo (energy) never run out.
    if (MasterComponentList[Inventory[weaponIndex].MasterID].MissileType == 0)
    {
        return 9999;
    }

    return AmmoTypeTotal[Inventory[weaponIndex].AmmoIndex].CurAmount;
}

auto MCMover::GetWeaponAmmoLevel(int32_t weaponIndex) -> float
{
    if (IsWeaponIndex(weaponIndex) == 0)
    {
        return -1.0f;
    }

    const MCAmmoTally& ammo = AmmoTypeTotal[Inventory[weaponIndex].AmmoIndex];
    return static_cast<float>(static_cast<double>(ammo.CurAmount) / ammo.StartAmount);
}

auto MCMover::CalcWeaponEffectiveness(int setMax) -> void
{
    int32_t effectiveness = 0;
    LastWeaponEffectivenessCalc = ScenarioTime;
    float gunneryFactor = 1.0f;

    if (Pilot != nullptr)
    {
        gunneryFactor = static_cast<float>(static_cast<double>(Pilot->Skills[MWS_GUNNERY]) * 0.02);
    }

    for (int32_t i = NumOther; i < NumOther + NumWeapons; i++)
    {
        if (setMax != 0 || (Inventory[i].Disabled == 0 && GetWeaponShots(i) > 0))
        {
            effectiveness =
                static_cast<int32_t>(static_cast<double>(Inventory[i].Effectiveness) * gunneryFactor + effectiveness);
        }
    }

    if (setMax != 0)
    {
        MaxWeaponEffectiveness = static_cast<float>(effectiveness);
        return;
    }

    WeaponEffectiveness = static_cast<float>(effectiveness);

    if (effectiveness == 0)
    {
        PlayMessage(static_cast<MCRadioMessageType>(0x23), 0);
    }
    else if (static_cast<double>(effectiveness) < static_cast<double>(MaxWeaponEffectiveness) * 0.5f)
    {
        PlayMessage(static_cast<MCRadioMessageType>(0x22), 0);
    }
}

auto MCMover::CalcWeaponRangeRatings() -> void
{
    for (int32_t i = NumOther; i < NumOther + NumWeapons; i++)
    {
        if (NumRangeRatings <= 0)
        {
            continue;
        }

        const double gunnery = Pilot->Skills[MWS_GUNNERY];
        const MCMasterComponent& weapon = MasterComponentList[Inventory[i].MasterID];
        float* rating = Inventory[i].RangeRatings;

        for (int32_t step = 0; step < NumRangeRatings; step++)
        {
            const float range = static_cast<float>(static_cast<double>(step) * RangeRatingIncrement);
            // Out of the weapon's range: 1000 less.
            double value = gunnery;

            if (!(weapon.WeaponRange[0] < range) ||
                (weapon.WeaponRange[1] < range && weapon.WeaponRange[2] < range && weapon.WeaponRange[3] < range))
            {
                value -= 1000.0;
            }

            rating[0] = static_cast<float>(value);
            rating[1] = static_cast<float>(weapon.Damage * value * 10.0 / weapon.RecycleTime);
            rating += 2;
        }
    }
}

auto MCMover::CalcAmmoTotals() -> void
{
    NumAmmoTypes = 0;

    if (NumWeapons == 0)
    {
        return;
    }

    // One type per weapon ammo (9999 rounds for a weapon without ammo), then the bins' rounds.
    MCAmmoTally tally[100];
    const int32_t firstWeapon = NumOther;
    const int32_t numWeaponItems = NumWeapons;
    const int32_t firstAmmo = firstWeapon + numWeaponItems;

    for (int32_t i = firstWeapon; i < firstAmmo; i++)
    {
        const MCMasterComponent& weapon = MasterComponentList[Inventory[i].MasterID];
        const int32_t numTypes = NumAmmoTypes;
        int32_t type = 0;

        while (type < numTypes && tally[type].MasterId != weapon.AmmoMasterId)
        {
            type++;
        }

        if (type < numTypes)
        {
            continue;
        }

        tally[numTypes].MasterId = weapon.AmmoMasterId;
        const int32_t rounds = weapon.MissileType == 0 ? 9999 : 0;
        tally[numTypes].CurAmount = rounds;
        tally[numTypes].StartAmount = rounds;
        NumAmmoTypes = static_cast<int8_t>(numTypes + 1);
    }

    const int32_t numTypes = NumAmmoTypes;

    for (int32_t i = firstAmmo; i < firstWeapon + NumAmmos + numWeaponItems; i++)
    {
        for (int32_t type = 0; type < numTypes; type++)
        {
            if (tally[type].MasterId == Inventory[i].MasterID)
            {
                tally[type].CurAmount += Inventory[i].Amount;
                tally[type].StartAmount += Inventory[i].Amount;
                break;
            }
        }
    }

    AmmoTypeTotal = std::make_unique<MCAmmoTally[]>(static_cast<size_t>(numTypes));
    std::copy_n(tally, numTypes, AmmoTypeTotal.get());
}

auto MCMover::CalcOptimalRange(MCGameObject* target) -> int
{
    const float oldRange = OptimalRange;
    LastOptimalRangeCalc = ScenarioTime;

    if (target == nullptr)
    {
        target = GetPilot()->GetLastTarget();
    }

    const float fireRange = GetFireRange(-2);

    // Outranging a mover target: stay just inside the longest range.
    if (target != nullptr && IsMover(target) && static_cast<MCMover*>(target)->LongestRangeWeapon != 0xff &&
        !(fireRange <= static_cast<MCMover*>(target)->GetFireRange(-2)))
    {
        OptimalRange = static_cast<float>(static_cast<double>(fireRange) - 10.0);
        return OptimalRange != oldRange ? 1 : 0;
    }

    // Else the range step whose summed ratings (then damage rates, then the farthest step) are best.
    auto setItem = [](int32_t index, float value, int32_t id)
    {
        if (index > -1 && index < SortList->NumItems)
        {
            SortList->List[index].Id = id;
            SortList->List[index].Value = value;
        }
    };

    int32_t numWorking = 0;
    SortList->Clear(1);

    for (int32_t step = 0; step < NumRangeRatings; step++)
    {
        float total = 0.0f;

        for (int32_t i = NumOther; i < NumOther + NumWeapons; i++)
        {
            if (Inventory[i].Disabled == 0 && GetWeaponShots(i) > 0)
            {
                if (step == 0)
                {
                    numWorking++;
                }

                total += Inventory[i].RangeRatings[step * 2];
            }
        }

        setItem(step, total, step);
    }

    if (NumRangeRatings <= 0 || numWorking == 0)
    {
        OptimalRange = 0.0f;
        return oldRange != 0.0f ? 1 : 0;
    }

    SortList->Sort(1);
    int32_t bestStep = SortList->List[0].Id;

    if (SortList->List[1].Value == SortList->List[0].Value)
    {
        SortList->Clear(1);

        for (int32_t step = 0; step < NumRangeRatings; step++)
        {
            float total = 0.0f;

            for (int32_t i = NumOther; i < NumOther + NumWeapons; i++)
            {
                if (Inventory[i].Disabled == 0 && GetWeaponShots(i) > 0)
                {
                    total += Inventory[i].RangeRatings[step * 2 + 1];
                }
            }

            setItem(step, total, step);
        }

        SortList->Sort(1);
        const MCSortListNode* node = SortList->List.get();
        bestStep = node[0].Id;

        if (node[1].Value == node[0].Value)
        {
            const float bestValue = node[0].Value;

            do
            {
                if (bestStep < node->Id)
                {
                    bestStep = node->Id;
                }

                node++;
            } while (node->Value == bestValue);
        }
    }

    OptimalRange = static_cast<float>(bestStep) * RangeRatingIncrement;
    return OptimalRange != oldRange ? 1 : 0;
}

auto MCMover::CalcLongestRangeWeapon() -> int32_t
{
    float longestRange = 0.0f;
    float shortestRange = 1000000.0f;
    LongestRangeWeapon = 0xff;
    ShortestRangeWeapon = 0xff;
    MaxMinRange = 0.0f;

    for (int32_t i = NumOther; i < NumOther + NumWeapons; i++)
    {
        if (Inventory[i].Disabled != 0 || GetWeaponShots(i) <= 0)
        {
            continue;
        }

        const MCMasterComponent& weapon = MasterComponentList[Inventory[i].MasterID];

        if (longestRange < weapon.WeaponRange[3])
        {
            LongestRangeWeapon = static_cast<uint8_t>(i);
            longestRange = weapon.WeaponRange[3];
        }

        if (weapon.WeaponRange[1] < shortestRange)
        {
            ShortestRangeWeapon = static_cast<uint8_t>(i);
            shortestRange = weapon.WeaponRange[1];
        }

        if (MaxMinRange < weapon.WeaponRange[0])
        {
            MaxMinRange = weapon.WeaponRange[0];
        }
    }

    return LongestRangeWeapon;
}

auto MCMover::GetFireRange(int32_t which) -> float
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
            if (LongestRangeWeapon != 0xff)
            {
                return MasterComponentList[Inventory[LongestRangeWeapon].MasterID].WeaponRange[3];
            }
            break;
        }
        case -1:
            return OptimalRange;
        default:
            break;
    }

    return -1.0f;
}

auto MCMover::GetMaxFireRange() -> float
{
    return GetFireRange(-2);
}

auto MCMover::IsWeaponIndex(int32_t itemIndex) -> int
{
    return NumOther <= itemIndex && itemIndex < NumOther + NumWeapons ? 1 : 0;
}

auto MCMover::IsWeaponMissile(int32_t weaponIndex) -> int
{
    return MasterComponentList[Inventory[weaponIndex].MasterID].Form == COMPONENT_FORM_WEAPON_MISSILE ? 1 : 0;
}

auto MCMover::IsWeaponReady(int32_t weaponIndex) -> int
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

auto MCMover::IsWeaponWorking(int32_t weaponIndex) -> int
{
    if (Inventory[weaponIndex].Disabled != 0)
    {
        return 0;
    }

    return GetWeaponShots(weaponIndex) != 0 ? 1 : 0;
}

auto MCMover::StartWeaponRecycle(int32_t weaponIndex) -> void
{
    Inventory[weaponIndex].ReadyTime = MasterComponentList[Inventory[weaponIndex].MasterID].RecycleTime + ScenarioTime;
}

auto MCMover::TallyAmmo(int32_t ammoMasterId) -> int32_t
{
    int32_t total = 0;
    const int32_t firstAmmo = NumOther + NumWeapons;

    for (int32_t i = firstAmmo; i < firstAmmo + NumAmmos; i++)
    {
        if (Inventory[i].MasterID == ammoMasterId)
        {
            total += Inventory[i].Amount;
        }
    }

    return total;
}

auto MCMover::NeedsRefit(int armorOnly) -> int
{
    // Only a mech without a refit vehicle on the way.
    if (RefitBuddy != nullptr || ObjectClass != BATTLEMECH)
    {
        return 0;
    }

    if (armorOnly == 0)
    {
        for (int32_t i = 0; i < NumArmorLocations; i++)
        {
            if (i < NumBodyLocations)
            {
                // A destroyed arm needs neither structure nor armor.
                if ((i == MECH_BODY_LOCATION_LARM || i == MECH_BODY_LOCATION_RARM) && BodyAt(i).DamageState == 2)
                {
                    continue;
                }

                if (BodyAt(i).CurInternalStructure < static_cast<float>(BodyAt(i).MaxInternalStructure))
                {
                    return 1;
                }
            }

            if (Armor[i].CurArmor < static_cast<float>(Armor[i].MaxArmor))
            {
                return 1;
            }
        }
    }

    for (int32_t i = 0; i < NumAmmoTypes; i++)
    {
        if (AmmoTypeTotal[i].CurAmount < AmmoTypeTotal[i].StartAmount)
        {
            return 1;
        }
    }

    return 0;
}

auto MCMover::ReduceAmmo(int32_t ammoMasterId, int32_t amount) -> int32_t
{
    // From the bins in order.
    int32_t left = amount;
    const int32_t firstAmmo = NumOther + NumWeapons;

    for (int32_t i = firstAmmo; i < firstAmmo + NumAmmos; i++)
    {
        if (Inventory[i].MasterID != ammoMasterId)
        {
            continue;
        }

        if (left < Inventory[i].Amount)
        {
            Inventory[i].Amount = static_cast<int16_t>(Inventory[i].Amount - left);
            break;
        }

        left -= Inventory[i].Amount;
        Inventory[i].Amount = 0;
    }

    // Out of this ammo: the weapons, their effectiveness and the optimal range change.
    for (int32_t i = 0; i < NumAmmoTypes; i++)
    {
        if (AmmoTypeTotal[i].MasterId != ammoMasterId)
        {
            continue;
        }

        const int32_t rounds = AmmoTypeTotal[i].CurAmount - amount;
        AmmoTypeTotal[i].CurAmount = rounds;

        if (rounds < 1)
        {
            AmmoTypeTotal[i].CurAmount = 0;
            CalcLongestRangeWeapon();
            CalcWeaponEffectiveness(0);
            CalcOptimalRange(nullptr);
        }

        return amount;
    }

    return amount;
}

auto MCMover::DeductWeaponShot(int32_t weaponIndex, int32_t ammoAmount) -> void
{
    if (ammoAmount > 0)
    {
        ReduceAmmo(MasterComponentList[Inventory[weaponIndex].MasterID].AmmoMasterId, ammoAmount);
    }
}

auto MCMover::SortWeapons(int32_t* weaponList, int32_t* valueList, int32_t listSize, int32_t sortType, int skillCheck)
    -> int32_t
{
    MCMechWarrior* myPilot = Pilot;
    MCGameObject* target = myPilot->GetLastTarget();

    if (target == nullptr)
    {
        return -2;
    }

    int32_t aimLocation = -1;

    if (myPilot != nullptr && myPilot->CurTacOrder.IsCombatOrder() != 0)
    {
        aimLocation = myPilot->CurTacOrder.AttackParams.AimLocation;
    }

    // Best attack chance first; only sort type 0 is known (the id goes in before the type is checked).
    auto setId = [](int32_t index, int32_t id)
    {
        if (index > -1 && index < SortList->NumItems)
        {
            SortList->List[index].Id = id;
        }
    };

    auto setValue = [](int32_t index, float value)
    {
        if (index > -1 && index < SortList->NumItems)
        {
            SortList->List[index].Value = value;
        }
    };

    SortList->Clear(1);

    if (listSize == -1)
    {
        for (int32_t i = NumOther; i < NumOther + NumWeapons; i++)
        {
            setId(i - NumOther, i);

            if (sortType != 0)
            {
                return -3;
            }

            setValue(i - NumOther, CalcAttackChance(target, aimLocation, ScenarioTime, i, 0.0f, nullptr, nullptr));
        }

        SortList->Sort(1);
        listSize = NumWeapons;

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

                chance = CalcAttackChance(target, aimLocation, ScenarioTime, weaponList[i], 0.0f, nullptr, nullptr);
            }

            setValue(i, chance);
        }

        SortList->Sort(1);
    }

    for (int32_t i = 0; i < listSize; i++)
    {
        weaponList[i] = SortList->List[i].Id;
        valueList[i] = static_cast<int32_t>(SortList->List[i].Value);
    }

    return 0;
}

auto MCMover::CalcAttackChance(MCGameObject* target, int32_t aimLocation, float targetTime, int32_t weaponIndex,
                               float modifiers, int32_t* range, MCVector3D* targetPoint) -> float
{
    if (weaponIndex < NumOther || NumOther + NumWeapons <= weaponIndex)
    {
        return -9999.0f;
    }

    MCVector3D targetPosition;

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
        targetPosition = target->GetPosition();
    }

    float gunnery = static_cast<float>(Pilot->Skills[MWS_GUNNERY]);

    if (MPlayer == nullptr)
    {
        if (GetAlignment() == HomeTeam->Alignment)
        {
            gunnery = ApplyDifficultySkill(gunnery, 1);
        }
        else if (MPlayer == nullptr && GetAlignment() != HomeTeam->Alignment)
        {
            gunnery = ApplyDifficultySkill(gunnery, 0);
        }
    }

    const float metersToTarget = static_cast<float>(DistanceFrom(targetPosition));

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
    const MCMasterComponent& weapon = MasterComponentList[Inventory[weaponIndex].MasterID];
    float rangeModifier;

    if (!(weapon.WeaponRange[0] < metersToTarget))
    {
        return -1.0f;
    }

    if (!(weapon.WeaponRange[1] < metersToTarget))
    {
        rangeModifier = WeaponFireModifiers[0];
    }
    else if (!(weapon.WeaponRange[2] < metersToTarget))
    {
        rangeModifier = WeaponFireModifiers[1];
    }
    else if (!(weapon.WeaponRange[3] < metersToTarget))
    {
        rangeModifier = WeaponFireModifiers[2];
    }
    else
    {
        return -1.0f;
    }

    modifiers = rangeModifier + modifiers;

    // Port fix: the original reads a null target's class when aimLocation isn't -1.
    const bool mechTarget = target != nullptr && target->ObjectClass == BATTLEMECH;

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
            GetVelocity();
            const MCVector3D targetVelocity = target->GetVelocity();

            if (target != StationaryTarget)
            {
                StationaryTarget = target;
                StationaryTime = 0.0f;
            }
            else
            {
                // A target holding still gets easier, up to MaxStationaryTime.
                const double x = targetVelocity.X;
                const double y = targetVelocity.Y;
                const double z = targetVelocity.Z;

                if (std::sqrt(x * x + y * y + z * z) == 0.0)
                {
                    StationaryTime = FrameLength + StationaryTime;
                }
                else
                {
                    StationaryTime = 0.0f;
                }

                if (StationaryTime != 0.0f)
                {
                    double stationaryFactor = 1.0;

                    if (StationaryTime < MaxStationaryTime)
                    {
                        stationaryFactor = static_cast<double>(StationaryTime) / MaxStationaryTime;
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

auto MCMover::AmmoExplosion(int32_t ammoIndex) -> void
{
    Pilot->Injure(2.0f, 1);
    Assert(ammoIndex < NumOther + NumWeapons + NumAmmos, ammoIndex, " Ammo Index out of range ");
    Assert(NumOther + NumWeapons <= ammoIndex, ammoIndex, " Ammo Index too low ");
    MCInventoryItem& bin = Inventory[ammoIndex];
    const int32_t hitLocation = bin.BodyLocation;
    const int32_t rounds = bin.Amount;
    float damage = static_cast<float>(
        static_cast<double>(static_cast<int32_t>(MasterComponentList[bin.MasterID].Damage)) * rounds);

    if (damage > 254.0f)
    {
        damage = 254.0f;
    }

    bin.Amount = 0;
    const int16_t typeIndex = bin.AmmoIndex;

    if (typeIndex == -1)
    {
        Fatal(-1, " Bad Ammo Index in Ammo Explosion ");
    }

    Assert(typeIndex < NumAmmoTypes, typeIndex, " Too Many Ammo Types ");
    Assert(typeIndex > -1, typeIndex, " not enough Ammo Types ");
    AmmoTypeTotal[typeIndex].CurAmount -= rounds;
    MCWeaponShotInfo shotInfo;
    shotInfo.Init(nullptr, bin.MasterID, damage, hitLocation, 0.0f);
    HandleWeaponHit(&shotInfo, 0);
}

auto MCMover::Disable(uint32_t cause) -> void
{
    if (IsDisabled() != 0)
    {
        return;
    }

    if (Pilot != nullptr)
    {
        Pilot->HandleAlarm(6, cause);
    }

    Status = 1;
    DisableThisFrame = 1;

    if (Alignment == HomeTeam->Alignment)
    {
        FriendlyDestroyed = 1;
    }
    else
    {
        // An enemy mech is salvage, unless the roll (or the cause) blows it apart.
        if (MPlayer == nullptr && ObjectClass == BATTLEMECH)
        {
            if (SalvageRoll == -999)
            {
                SalvageRoll = RollDice(MechSalvageChance);
            }

            if (cause == 3 || cause == 2)
            {
                if (SalvageRoll == 0 && CantBlowSalvage == 0)
                {
                    for (int32_t i = 0; i < NumBodyLocations; i++)
                    {
                        DestroyBodyLocation(i);
                    }

                    Status = 2;
                }
                else
                {
                    MCTerrain::TerrainTacticalMap->AddSalvage(this);
                }
            }
            else if (CantBlowSalvage == 0 && SalvageRoll == 0)
            {
                for (int32_t i = 0; i < NumBodyLocations; i++)
                {
                    DestroyBodyLocation(i);
                }

                Status = 2;
                MCTerrain::TerrainTacticalMap->RemoveSalvage(this, 1);
            }
        }

        EnemyDestroyed = 1;
    }

    if (SensorSystem != nullptr)
    {
        SensorSystem->Disable();
    }
}

auto MCMover::ShutDown() -> void
{
    if (IsDisabled() == 0 && Status != 5 && Status != 4)
    {
        Status = 4;
        ShutDownThisFrame = 1;
    }
}

auto MCMover::StartUp() -> void
{
    if (IsDisabled() == 0 && Status != 3 && Status != 0)
    {
        Status = 3;
        StartUpThisFrame = 1;
    }
}

auto MCMover::IsWithdrawing() -> int
{
    return Pilot->CurTacOrder.Code == TACTICAL_ORDER_WITHDRAW ? 1 : 0;
}

auto MCMover::GetGroupId() -> int32_t
{
    if (Group != nullptr)
    {
        return Group->GetId();
    }

    return -1;
}

auto MCMover::GetVitalInfo(void* vitalInfo) -> int32_t
{
    int32_t size = MCBigGameObject::GetVitalInfo(nullptr);
    size = static_cast<int32_t>(DebugStatus.size() + 1) + size + (static_cast<int32_t>(std::strlen(IconName) + 1) - 2) +
           (NumAmmos + NumWeapons + 9 + NumOther) * 0x1c;

    for (const int32_t criticalSpaces : NumLocationCriticalSpaces)
    {
        size += criticalSpaces * 8;
    }

    if (vitalInfo != nullptr)
    {
        MCBigGameObject::GetVitalInfo(vitalInfo);
    }

    return size;
}

auto MCMover::SetSelected(int32_t newSelected) -> void
{
    // Deselection takes a second (not for network players' movers).
    if (newSelected == 0 && NetPlayerId < 0)
    {
        DeselectTime = ScenarioTime + 1.0f;
        return;
    }

    Selected = newSelected;
    DeselectTime = 0.0f;
}
