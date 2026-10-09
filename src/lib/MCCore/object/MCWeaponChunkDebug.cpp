#include "stdafx.h"
#include "object/MCWeaponChunkDebug.h"
#include "ai/MCMoveSystem.h"
#include "lib/MCFile.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "object/MCObjectQueue.h"
#include "object/MCObjectSystem.h"
#include "object/MCWeaponFireChunk.h"
#include "object/MCWeaponHitChunk.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"

std::string ChunkDebugMsg;

namespace
{
    /// <summary>The object's line: a mover's debug status, else its class, with its part id.</summary>
    std::string ObjectLine(std::string_view role, MCBaseObject* object)
    {
        if (IsMoverClass(object->ObjectClass))
        {
            return std::format("{} = {} ({})\n", role, static_cast<MCMover*>(object)->DebugStatus, object->PartId);
        }

        return std::format("{} = objClass {} ({})\n", role, static_cast<int>(object->ObjectClass), object->PartId);
    }

    /// <summary>
    /// Appends the target line both debug routines print: the mover (roster index), terrain object, train car or
    /// camera drone (part id), or map point, or else three question marks with the terrain objects of the target's
    /// vertex listed.
    /// </summary>
    /// <param name="itemNumber">The item number used for the vertex listing (see OB-010).</param>
    /// <param name="cellC">The cell column used for a map point (see OB-010).</param>
    void AppendTargetLine(int8_t targetType, int32_t targetId, uint16_t cellR, uint16_t cellC, int8_t itemNumber,
                          bool hitChunk)
    {
        MCBaseObject* target = nullptr;
        bool haveTarget = false;

        if (targetType == 0)
        {
            target = MPlayer->MoverRoster[targetId];
            haveTarget = true;
        }
        else if (targetType == 1 || targetType == 2)
        {
            target = ObjectList()->FindObjectFromPart(targetId);
            haveTarget = true;
        }
        else if (targetType == 3)
        {
            if (hitChunk)
            {
                ChunkDebugMsg += "target point\n";
                return;
            }

            // The middle of the target cell, on the ground.
            const float halfSide = WorldUnitsMapSide * 0.5f;
            MCVector3D point;
            point.X = static_cast<float>((cellC + 0.5f) * static_cast<double>(MetersPerCell()) - halfSide);
            point.Y =
                static_cast<float>((static_cast<double>(halfSide) - cellR * static_cast<double>(MetersPerCell())) -
                                   static_cast<double>(MetersPerCell()) * 0.5f);
            point.Z = 0.0f;
            const float elevation = GameMap()->GetTerrainElevation(point);
            ChunkDebugMsg += std::format("target point = ({:f}, {:f}, {:f})\n", point.X, point.Y, elevation);
            return;
        }

        if (haveTarget && target != nullptr)
        {
            ChunkDebugMsg += ObjectLine("target", target);
            return;
        }

        ChunkDebugMsg += "target = ???\n";

        if (hitChunk || targetType != 1)
        {
            return;
        }

        // List the terrain objects on the target's vertex.
        const int32_t firstId = targetId - itemNumber;
        int32_t numObjects = 0;

        for (int32_t i = 0; i < 8; i++)
        {
            MCBaseObject* object = ObjectList()->FindObjectFromPart(firstId + i);

            if (object == nullptr)
            {
                continue;
            }

            numObjects++;
            ChunkDebugMsg +=
                std::format("    {}: objClass {} ({})\n", i, static_cast<int>(object->ObjectClass), object->PartId);
        }

        if (numObjects > 0)
        {
            ChunkDebugMsg += std::format("    There are {} terrain objects in this tile.\n", numObjects);
        }
    }

    /// <summary>
    /// Appends one weapon fire chunk's fields. <paramref name="first"/> is the first chunk of the pair, whose cell
    /// column and item number the original prints for the second one too.
    /// </summary>
    void AppendWeaponFireChunk(const MCWeaponFireChunk* chunk, const MCWeaponFireChunk* first)
    {
        // Original behaviour (OB-010): the second chunk's target point and terrain listing use the first chunk's
        // cell column and item number.
        const uint16_t cellC = first->TargetCell[1];
        AppendTargetLine(chunk->TargetType, chunk->TargetId, chunk->TargetCell[0], cellC, first->TargetItemNumber,
                         false);

        ChunkDebugMsg += std::format("targetType = {}\n", static_cast<int>(chunk->TargetType));
        ChunkDebugMsg += std::format("targetId = {}\n", chunk->TargetId);
        ChunkDebugMsg += std::format("targetBlockOrTrainNumber = {}\n", chunk->TargetBlockOrTrainNumber);
        ChunkDebugMsg += std::format("targetVertexOrCarNumber = {}\n", chunk->TargetVertexOrCarNumber);
        ChunkDebugMsg += std::format("targetItemNumber = {}\n", static_cast<int>(chunk->TargetItemNumber));
        ChunkDebugMsg += std::format("targetCellRC = ({}, {})\n", chunk->TargetCell[0], cellC);
        ChunkDebugMsg += std::format("weaponIndex = {}\n", chunk->WeaponIndex);
        ChunkDebugMsg += std::format("hit = {}\n", chunk->Hit != 0 ? 'T' : 'N');
        ChunkDebugMsg += std::format("entryAngle = {}\n", static_cast<int>(chunk->EntryAngle));
        ChunkDebugMsg += std::format("numMissiles = {}\n", static_cast<int>(chunk->NumMissiles));
        ChunkDebugMsg += std::format("numMissilesHit = {}\n", static_cast<int>(chunk->NumMissilesPastAms));
        ChunkDebugMsg += std::format("numAntiMissiles = {}\n", static_cast<int>(chunk->NumAntiMissileShots));
        ChunkDebugMsg += std::format("hitLocation = {}\n", static_cast<int>(chunk->HitLocation));
        ChunkDebugMsg += std::format("data = {:x}\n", chunk->Data);
    }

    /// <summary>Appends one weapon hit chunk's fields.</summary>
    void AppendWeaponHitChunk(const MCWeaponHitChunk* chunk)
    {
        AppendTargetLine(chunk->TargetType, chunk->TargetId, 0, 0, 0, true);
        ChunkDebugMsg += std::format("targetType = {}\n", static_cast<int>(chunk->TargetType));
        ChunkDebugMsg += std::format("targetId = {}\n", chunk->TargetId);
        ChunkDebugMsg += std::format("targetBlockOrTrainNumber = {}\n", chunk->TargetBlockOrTrainNumber);
        ChunkDebugMsg += std::format("targetVertexOrCarNumber = {}\n", chunk->TargetVertexOrCarNumber);
        ChunkDebugMsg += std::format("targetItemNumber = {}\n", static_cast<int>(chunk->TargetItemNumber));
        ChunkDebugMsg += std::format("cause = {}\n", static_cast<int>(chunk->Cause));
        ChunkDebugMsg += std::format("damage = {:f}\n", chunk->Damage);
        ChunkDebugMsg += std::format("hitLocation = {}\n", static_cast<int>(chunk->HitLocation));
        ChunkDebugMsg += std::format("entryAngle = {}\n", static_cast<int>(chunk->EntryAngle));
        ChunkDebugMsg += std::format("refit = {}\n", chunk->Refit != 0 ? "TRUE" : "FALSE");
    }
}

auto SaveChunkDebugMsg(std::string_view fileName) -> void
{
    MCFile file;
    file.Create(fileName);
    file.WriteString(ChunkDebugMsg);
    file.Close();
    ExceptionGameMsg = ChunkDebugMsg.data();
}

auto DebugWeaponFireChunk(MCWeaponFireChunk* chunk1, MCWeaponFireChunk* chunk2, MCGameObject* attacker) -> void
{
    ChunkDebugMsg = attacker == nullptr ? "attacker = ???\n" : ObjectLine("attacker", attacker);

    if (chunk1 != nullptr)
    {
        ChunkDebugMsg += "\nCHUNK1\n";
        AppendWeaponFireChunk(chunk1, chunk1);
    }

    if (chunk2 != nullptr)
    {
        ChunkDebugMsg += "\nCHUNK2\n";
        // Port fix: the original reads chunk1's fields here even when chunk1 is null (no caller passes that).
        AppendWeaponFireChunk(chunk2, chunk1 != nullptr ? chunk1 : chunk2);
    }

    SaveChunkDebugMsg("wfchunk.dbg");
}

auto DebugWeaponHitChunk(MCWeaponHitChunk* chunk1, MCWeaponHitChunk* chunk2) -> void
{
    ChunkDebugMsg.clear();

    if (chunk1 != nullptr)
    {
        ChunkDebugMsg += "\nCHUNK1\n";
        AppendWeaponHitChunk(chunk1);
    }

    if (chunk2 != nullptr)
    {
        ChunkDebugMsg += "\nCHUNK2\n";
        AppendWeaponHitChunk(chunk2);
    }

    SaveChunkDebugMsg("whchunk.dbg");
}
