#include "stdafx.h"
#include "object/MCStatusChunk.h"
#include "network/MCMultiPlayer.h"
#include "object/MCMover.h"
#include "object/MCObjectQueue.h"
#include "object/MCObjectSystem.h"
#include "object/MCWeaponChunkDebug.h"
#include "terrain/MCTerrain.h"

int32_t StatusChunkUnpackErr = 0;

namespace
{
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

                // The original read the roster at an id past it too.
                if (targetId < 0 || MultiPlayer()->NumMovers <= targetId)
                {
                    StatusChunkUnpackErr = 1;
                }
                else if (MultiPlayer()->MoverRoster[targetId] == nullptr)
                {
                    StatusChunkUnpackErr = 2;
                }
                break;
            }

            case 2:
            {
                if (ObjectList()->FindObjectFromPart(chunk->TargetId) == nullptr)
                {
                    StatusChunkUnpackErr = 3;
                }
                break;
            }
            case 3:
            {
                if (ObjectList()->FindObjectFromPart(chunk->TargetId) == nullptr)
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
        std::string line;
        const int8_t targetType = static_cast<int8_t>(chunk->TargetType);
        MCBaseObject* target = nullptr;
        bool haveTarget = false;

        if (targetType == 1)
        {
            // An id past the roster has no mover (the original read past it).
            if (chunk->TargetId >= 0 && chunk->TargetId < std::ssize(MultiPlayer()->MoverRoster))
            {
                target = MultiPlayer()->MoverRoster[chunk->TargetId];
            }

            haveTarget = true;
        }
        else if (targetType == 2 || targetType == 3)
        {
            target = ObjectList()->FindObjectFromPart(chunk->TargetId);
            haveTarget = true;
        }

        bool appendLine = true;

        if (haveTarget && target != nullptr)
        {
            if (IsMoverClass(target->ObjectClass))
            {
                line = std::format("target = {} ({})\n", static_cast<MCMover*>(target)->DebugStatus, target->PartId);
            }
            else
            {
                line =
                    std::format("target = objClass {} ({})\n", static_cast<int>(target->ObjectClass), target->PartId);
            }
        }
        else if (!haveTarget && targetType == 4)
        {
            // The middle of the target cell, on the ground.
            const float halfSide = WorldUnitsMapSide * 0.5f;
            MCVector3D point;
            point.X =
                static_cast<float>((chunk->TargetCellRC[1] + 0.5f) * static_cast<double>(MetersPerCell()) - halfSide);
            point.Y = static_cast<float>(
                (static_cast<double>(halfSide) - chunk->TargetCellRC[0] * static_cast<double>(MetersPerCell())) -
                static_cast<double>(MetersPerCell()) * 0.5f);
            point.Z = 0.0f;
            const float elevation = GameMap()->GetTerrainElevation(point);
            line = std::format("target point = ({:f}, {:f}, {:f})\n", point.X, point.Y, elevation);
        }
        else if (targetType == 0)
        {
            line = "target = NONE\n";
        }
        else
        {
            ChunkDebugMsg += "target = ???\n";
            appendLine = false;

            if (targetType == 2)
            {
                // List the terrain objects on the target's vertex.
                const int32_t firstId = chunk->TargetId - static_cast<int8_t>(chunk->TargetItemNumber);
                int32_t numObjects = 0;

                for (int32_t i = 0; i < 8; i++)
                {
                    MCBaseObject* object = ObjectList()->FindObjectFromPart(firstId + i);

                    if (object == nullptr)
                    {
                        continue;
                    }

                    numObjects++;
                    ChunkDebugMsg += std::format("    {}: objClass {} ({})\n", i, static_cast<int>(object->ObjectClass),
                                                 object->PartId);
                }

                if (numObjects > 0)
                {
                    line = std::format("    There are {} terrain objects in this tile.\n", numObjects);
                    appendLine = true;
                }
            }
        }

        if (appendLine)
        {
            ChunkDebugMsg += line;
        }

        ChunkDebugMsg += std::format("bodyState = {}\n", static_cast<int>(chunk->BodyState));
        ChunkDebugMsg += std::format("targetType = {}\n", static_cast<int>(targetType));
        ChunkDebugMsg += std::format("targetId = {}\n", chunk->TargetId);
        ChunkDebugMsg += std::format("targetBlockOrTrainNumber = {}\n", chunk->TargetBlockOrTrainNumber);
        ChunkDebugMsg += std::format("targetVertexOrCarNumber = {}\n", chunk->TargetVertexOrCarNumber);
        ChunkDebugMsg +=
            std::format("targetItemNumber = {}\n", static_cast<int>(static_cast<int8_t>(chunk->TargetItemNumber)));
        ChunkDebugMsg += std::format("targetCellRC = ({}, {})\n", static_cast<int>(chunk->TargetCellRC[0]),
                                     static_cast<int>(chunk->TargetCellRC[1]));
        ChunkDebugMsg += std::format("ejectOrderGiven = {}\n", chunk->EjectOrderGiven != 0 ? 'T' : 'F');
        ChunkDebugMsg += std::format("jumpOrder = {}\n", chunk->JumpOrder != 0 ? 'T' : 'F');
        ChunkDebugMsg += std::format("data = {:x}\n", chunk->Data);
    }
}

auto MCStatusChunk::Pack() -> void
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

auto MCStatusChunk::Unpack() -> void
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

auto MCStatusChunk::EqualTo(const MCStatusChunk& chunk) const -> bool
{
    if (BodyState != chunk.BodyState || EjectOrderGiven != chunk.EjectOrderGiven || JumpOrder != chunk.JumpOrder ||
        TargetType != chunk.TargetType || TargetId != chunk.TargetId || TargetCellRC != chunk.TargetCellRC)
    {
        DebugStatusChunk(nullptr, this, &chunk);
        return false;
    }

    return true;
}

auto DebugStatusChunk(MCMover* mover, const MCStatusChunk* chunk1, const MCStatusChunk* chunk2) -> void
{
    ChunkDebugMsg = mover == nullptr ? std::string("\nmover = ???\n")
                                     : std::format("\nmover = {} ({})\n", mover->DebugStatus, mover->PartId);

    if (chunk1 != nullptr)
    {
        ChunkDebugMsg += "\nCHUNK1\n";
        AppendStatusChunk(chunk1);
    }

    if (chunk2 != nullptr)
    {
        ChunkDebugMsg += "\nCHUNK2\n";
        AppendStatusChunk(chunk2);
    }

    SaveChunkDebugMsg("stchunk.dbg");
}
