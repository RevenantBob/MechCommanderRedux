#pragma once

#include "lib/MCVector3D.h"

class MCMover;
class MCMovePath;

/// <summary>
/// A mover's move path in the compact form sent over the network: up to four steps, each a tile and cell, with the
/// direction from each step to the next, and all of it packed into one 32-bit word.
/// </summary>
/// <remarks>Original source: <c>ai\move.cpp</c> (init inline in <c>ai\move.h</c>).</remarks>
class MCMoveChunk
{
public:
    /// <summary>Steps a chunk carries: the packed word stores the count - 1 in 2 bits (a wire format limit).</summary>
    static constexpr int32_t MaxSteps = 4;

    /// <summary>Makes the chunk empty (no start cell, no steps, walking).</summary>
    void Reset()
    {
        StepPos[0][0] = -1;
        StepPos[0][1] = -1;
        NumSteps = 0;
        Run = 0;
        Data = 0;
    }

    /// <summary>Builds the chunk from the mover's current path (and the one after it).</summary>
    void Build(MCMover* mover, MCMovePath* path1, MCMovePath* path2);
    /// <summary>Builds a one-step chunk to a jump destination.</summary>
    void Build(MCMover* mover, MCVector3D jumpGoal);
    /// <summary>Packs the steps into <see cref="Data"/>.</summary>
    void Pack(MCMover* mover);
    /// <summary>Unpacks <see cref="Data"/>.</summary>
    /// <returns>Whether the chunk was good (a step count of 1..4, and directions 0..7).</returns>
    bool Unpack(MCMover* mover);
    /// <summary>Whether two chunks hold the same steps (else dumps both to mvchunk.dbg).</summary>
    bool EqualTo(MCMover* mover, const MCMoveChunk* chunk) const;

    /// <summary>Per step: tileR, tileC, cellR, cellC.</summary>
    std::array<std::array<int32_t, 4>, MaxSteps> StepPos{};
    /// <summary>Direction (0-7) from each step to the next.</summary>
    std::array<int32_t, MaxSteps - 1> StepRelPos{};
    int32_t NumSteps = 0;
    int32_t Run = 0;
    /// <summary>The packed chunk: start cell row/col, numSteps - 1, run, then the three step directions.</summary>
    uint32_t Data = 0;
};

/// <summary>Writes one or two chunks and the mover's state to mvchunk.dbg (and the crash report's game text).</summary>
void DebugMoveChunk(MCMover* mover, const MCMoveChunk* chunk1, const MCMoveChunk* chunk2);
