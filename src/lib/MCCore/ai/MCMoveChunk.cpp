#include "stdafx.h"
#include "ai/MCMoveChunk.h"
#include "ai/MCMoveGeometry.h"
#include "ai/MCMovePath.h"
#include "ai/MCObjectMap.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "main/main.h"
#include "object/mech.h"
#include "object/mover.h"
#include "object/warrior.h"
#include "object/MCWeaponChunkDebug.h"

namespace
{
    /// <summary>The size of ChunkDebugMsg (object\gameobj.cpp).</summary>
    constexpr size_t ChunkDebugMsgSize = 0x1400;

    /// <summary>
    /// Copies path step <paramref name="step"/> into chunk step <paramref name="index"/>, with its direction: from
    /// the start cell for the first step, else the path step's own.
    /// </summary>
    /// <remarks>Inlined three times in MoveChunk::build.</remarks>
    void CopyChunkStep(MCMoveChunk& chunk, int32_t index, const MCPathStep& step)
    {
        std::array<int32_t, 4>& pos = chunk.StepPos[static_cast<size_t>(index)];
        pos = {step.TileR, step.TileC, step.CellR, step.CellC};

        if (index == 1)
        {
            const std::array<int32_t, 4>& start = chunk.StepPos[0];
            chunk.StepRelPos[0] = CellDirToCell(start[0], start[1], start[2], start[3], pos[0], pos[1], pos[2], pos[3]);
        }
        else
        {
            chunk.StepRelPos[static_cast<size_t>(index - 1)] = static_cast<int8_t>(step.Direction);
        }
    }

    /// <summary>Makes the chunk a single step at the mover's cell.</summary>
    void SetChunkAtMover(MCMoveChunk& chunk, MCMover* mover)
    {
        const MCObjectPosition* objPosition = mover->GetObjPosition();
        chunk.StepPos[0] = {objPosition->TileR, objPosition->TileC, objPosition->CellR, objPosition->CellC};
        chunk.StepRelPos = {};
        chunk.NumSteps = 1;
    }

    /// <summary>The text of one chunk for the debug dump.</summary>
    std::string ChunkText(std::string_view title, const MCMoveChunk& chunk)
    {
        std::string text(title);

        for (int32_t i = 0; i < MCMoveChunk::MaxSteps; i++)
        {
            const std::array<int32_t, 4>& pos = chunk.StepPos[static_cast<size_t>(i)];
            text += std::format("stepPos[{}] = ({}, {}, {}, {})\n", i, pos[0], pos[1], pos[2], pos[3]);
        }

        text += std::format("stepRelPos = {}, {}, {}\n", chunk.StepRelPos[0], chunk.StepRelPos[1], chunk.StepRelPos[2]);
        text += std::format("numSteps = {}\n", chunk.NumSteps);
        text += std::format("run = {}\n", chunk.Run != 0 ? 'T' : 'F');
        return text;
    }
}

auto DebugMoveChunk(MCMover* mover, const MCMoveChunk* chunk1, const MCMoveChunk* chunk2) -> void
{
    std::string text;

    if (mover != nullptr)
    {
        const MCVector3D position = mover->GetPosition();
        const MCObjectPosition* objPosition = mover->GetObjPosition();
        text += std::format("Mover = {} ({})\n", mover->DebugStatus, mover->PartId);
        text += std::format("Mover World Pos = ({:.4f}, {:.4f}, {:.4f})\n", position.X, position.Y, position.Z);
        text += std::format("Mover Obj Pos = [{}, {}, {}, {}]\n", objPosition->TileR, objPosition->TileC,
                            objPosition->CellR, objPosition->CellC);

        if (mover->GetPilot() == nullptr)
        {
            text += "NULL pilot!\n";
        }

        if (mover->ObjectClass == MCObjectClass::BattleMech && static_cast<MCBattleMech*>(mover)->InJump != 0)
        {
            int32_t tileR = 0;
            int32_t tileC = 0;
            int32_t cellR = 0;
            int32_t cellC = 0;
            WorldCoordToMapCoord(static_cast<MCBattleMech*>(mover)->JumpGoal, tileR, tileC, cellR, cellC);
            text += std::format("Jumping to [{}, {}, {}, {}]\n", tileR, tileC, cellR, cellC);
        }

        text += "\n";
    }

    if (chunk1 != nullptr)
    {
        text += ChunkText("CHUNK1\n", *chunk1);
    }

    if (chunk2 != nullptr)
    {
        text += ChunkText("\nCHUNK2\n", *chunk2);
    }

    ChunkDebugMsg = std::move(text);
    SaveChunkDebugMsg("mvchunk.dbg");
}

auto MCMoveChunk::Build(MCMover* mover, MCMovePath* path1, MCMovePath* path2) -> void
{
    SetChunkAtMover(*this, mover);

    if (path1 != nullptr && path1->NumSteps > 0)
    {
        const int32_t pathSteps = path1->NumSteps;
        const int32_t curStep = path1->CurStep;
        int32_t index = 1;
        bool roomLeft = true;

        if (curStep < pathSteps)
        {
            const MCPathStep& current = path1->StepList[static_cast<size_t>(curStep)];
            bool onCurrentStep = StepPos[0][0] == current.TileR && StepPos[0][1] == current.TileC &&
                                 StepPos[0][2] == current.CellR && StepPos[0][3] == current.CellC;

            if (!onCurrentStep && CellDirToCell(StepPos[0][0], StepPos[0][1], StepPos[0][2], StepPos[0][3],
                                                current.TileR, current.TileC, current.CellR, current.CellC) == -2)
            {
                onCurrentStep = true; // not next to the current step: start the chunk from it
            }

            int32_t firstStep;
            int32_t stepsLeft;

            if (onCurrentStep)
            {
                StepPos[0] = {current.TileR, current.TileC, current.CellR, current.CellC};
                firstStep = curStep + 1;
                stepsLeft = pathSteps - firstStep;
            }
            else
            {
                firstStep = curStep;
                stepsLeft = pathSteps - curStep;
            }

            roomLeft = stepsLeft < MaxSteps - 1;
            NumSteps = roomLeft ? stepsLeft : MaxSteps - 1;

            for (int32_t i = 0; i < NumSteps; i++)
            {
                CopyChunkStep(*this, index++, path1->StepList[static_cast<size_t>(firstStep + i)]);
            }

            NumSteps++;
        }

        const bool nextLeg = path2 != nullptr && path1->GlobalStep >= 0 && path2->GlobalStep == path1->GlobalStep + 1;

        if (roomLeft && nextLeg && path2->NumStepsWhenNotPaused > 0 && NumSteps < MaxSteps)
        {
            const int32_t extraSteps = std::min(MaxSteps - NumSteps, path2->NumSteps);

            for (int32_t i = 0; i < extraSteps; i++)
            {
                Assert(index < MaxSteps, static_cast<uint32_t>(index),
                       " MoveChunk.build: path2 and bad curStep > MOVECHUNK_NUM_STEPS ");

                // Port fix: the original writes past stepPos[] when the assert above fails.
                if (index >= MaxSteps)
                {
                    break;
                }

                CopyChunkStep(*this, index++, path2->StepList[static_cast<size_t>(i)]);
            }

            NumSteps = extraSteps + NumSteps;
            Assert(NumSteps < MaxSteps + 1, static_cast<uint32_t>(index),
                   " MoveChunk.build: path2 and bad curStep > MOVECHUNK_NUM_STEPS ");
        }
    }

    if (NumSteps < 1 || NumSteps > MaxSteps)
    {
        SetChunkAtMover(*this, mover);
    }

    Run = mover->GetPilot()->MoveOrders.Run;

    if (Run != 0 && mover->ObjectClass == MCObjectClass::BattleMech)
    {
        Run = static_cast<MCBattleMech*>(mover)->LegStatus == 0 ? 1 : 0;
    }

    Data = 0;
}

auto MCMoveChunk::Build(MCMover*, MCVector3D jumpGoal) -> void
{
    WorldCoordToMapCoord(jumpGoal, StepPos[0][0], StepPos[0][1], StepPos[0][2], StepPos[0][3]);
    StepRelPos = {};
    NumSteps = 1;
    Run = 0;
}

auto MCMoveChunk::Pack(MCMover* mover) -> void
{
    const int32_t stepCount = NumSteps;
    uint32_t packed = static_cast<uint32_t>(StepPos[0][1] * MapCellDim + StepPos[0][3]) << 3 |
                      static_cast<uint32_t>(StepPos[0][0] * MapCellDim + StepPos[0][2]) << 13 |
                      static_cast<uint32_t>(stepCount * 2 - 2);

    if (Run != 0)
    {
        packed |= 1;
    }

    // Original behaviour (OB-029): a direction of -2 (MoveChunk::build found no neighbour) spills into the upper
    // bits.
    packed = ((packed << 3 | static_cast<uint32_t>(StepRelPos[0])) << 3 | static_cast<uint32_t>(StepRelPos[1])) << 3 |
             static_cast<uint32_t>(StepRelPos[2]);
    Data = packed;

    if (stepCount < 1 || stepCount > MaxSteps)
    {
        DebugMoveChunk(mover, this, nullptr);
        Assert(false, static_cast<uint32_t>(NumSteps),
               std::format(" MoveChunk.pack: bad numSteps {} (save mvchunk.dbg file) ", NumSteps));
    }
}

auto MCMoveChunk::Unpack(MCMover*) -> bool
{
    const uint32_t packed = Data;
    StepRelPos[2] = static_cast<int32_t>(packed & 7);
    StepRelPos[1] = static_cast<int32_t>((packed >> 3) & 7);
    StepRelPos[0] = static_cast<int32_t>((packed >> 6) & 7);
    NumSteps = static_cast<int32_t>((packed >> 10) & 3) + 1;
    Run = static_cast<int32_t>((packed >> 9) & 1);
    const uint32_t cellCol = (packed >> 12) & 0x3ff;
    StepPos[0][1] = static_cast<int32_t>(cellCol / MapCellDim);
    StepPos[0][3] = static_cast<int32_t>(cellCol % MapCellDim);
    const uint32_t cellRow = packed >> 22;
    Data = cellRow; // as the original: the packed word is left holding the start cell row
    StepPos[0][0] = static_cast<int32_t>(cellRow / MapCellDim);
    StepPos[0][2] = static_cast<int32_t>(cellRow % MapCellDim);

    for (int32_t i = 0; i < NumSteps - 1; i++)
    {
        const int32_t dir = StepRelPos[static_cast<size_t>(i)];

        if (dir < 0 || dir > 7)
        {
            return false;
        }

        const std::array<int32_t, 4>& from = StepPos[static_cast<size_t>(i)];
        const std::array<int32_t, 4>& adj =
            AdjCellTable[static_cast<size_t>(from[2] * MapCellDim + from[3])][static_cast<size_t>(dir)];
        StepPos[static_cast<size_t>(i + 1)] = {adj[0] + from[0], adj[1] + from[1], adj[2], adj[3]};
    }

    return true;
}

auto MCMoveChunk::EqualTo(MCMover* mover, const MCMoveChunk* chunk) const -> bool
{
    bool equal = NumSteps == chunk->NumSteps && Run == chunk->Run;

    for (int32_t i = 0; equal && i < NumSteps; i++)
    {
        equal = StepPos[static_cast<size_t>(i)] == chunk->StepPos[static_cast<size_t>(i)];
    }

    for (int32_t i = 0; equal && i < NumSteps - 1; i++)
    {
        equal = StepRelPos[static_cast<size_t>(i)] == chunk->StepRelPos[static_cast<size_t>(i)];
    }

    if (!equal)
    {
        DebugMoveChunk(mover, this, chunk);
    }

    return equal;
}
