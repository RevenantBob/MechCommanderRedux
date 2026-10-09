#pragma once

class MCMover;

/// <summary>
/// A mover's state as a network status chunk: body state, target and orders, packed into <see cref="Data"/> to
/// send.
/// </summary>
/// <remarks>Original source: <c>object\mover.cpp</c> (init inline in <c>object\mover.h</c>). Field names follow
/// MechCommander 2's StatusChunk, which kept this layout.</remarks>
class MCStatusChunk
{
public:
    /// <summary>Clears everything; no target cell.</summary>
    void Reset() { *this = MCStatusChunk{}; }
    /// <summary>Packs the fields into <see cref="Data"/>.</summary>
    void Pack();
    /// <summary>Unpacks <see cref="Data"/> into the fields.</summary>
    void Unpack();
    /// <summary>Whether two chunks carry the same state (else dumps both to stchunk.dbg).</summary>
    bool EqualTo(const MCStatusChunk& chunk) const;

    /// <summary>The mover's body state.</summary>
    uint32_t BodyState = 0;
    /// <summary>What the target is (mover, terrain object, train car, location...).</summary>
    uint8_t TargetType = 0;
    /// <summary>The target's part id.</summary>
    int32_t TargetId = 0;
    /// <summary>A terrain target's block, or a train target's train.</summary>
    int32_t TargetBlockOrTrainNumber = 0;
    /// <summary>A terrain target's vertex, or a train target's car.</summary>
    int32_t TargetVertexOrCarNumber = 0;
    /// <summary>A terrain target's item on the vertex.</summary>
    uint8_t TargetItemNumber = 0;
    /// <summary>A location target's map cell (row, column); -1 for none.</summary>
    std::array<int16_t, 2> TargetCellRC = {-1, -1};
    /// <summary>Whether the pilot was ordered to eject.</summary>
    int32_t EjectOrderGiven = 0;
    /// <summary>A jump order.</summary>
    int32_t JumpOrder = 0;
    /// <summary>The packed chunk.</summary>
    uint32_t Data = 0;
};

/// <summary>Why the last <see cref="MCStatusChunk::Pack"/> or <see cref="MCStatusChunk::Unpack"/> found its target bad
/// (0 none; 1-2 mover, 3 terrain object, 4 train car, 5 bad type).</summary>
extern int32_t StatusChunkUnpackErr;

/// <summary>
/// Writes a mover's two status chunks, field by field, to ChunkDebugMsg and the file "stchunk.dbg", and points the
/// crash report at them (<see cref="MCStatusChunk::EqualTo"/> calls it on a mismatch).
/// </summary>
void DebugStatusChunk(MCMover* mover, const MCStatusChunk* chunk1, const MCStatusChunk* chunk2);
