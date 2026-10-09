#include "stdafx.h"
#include "object/MCArtilleryChunk.h"
#include "ai/MCMoveSystem.h"

auto MCArtilleryChunk::Build(int32_t newCommanderId, int32_t newStrikeType, MCVector3D location, int32_t newSeconds)
    -> void
{
    CommanderId = static_cast<int8_t>(newCommanderId);
    StrikeType = static_cast<int8_t>(newStrikeType);
    WorldCoordToMapCell(location, CellRow, CellCol);
    Seconds = static_cast<int8_t>(newSeconds);
    Data = 0;
}

auto MCArtilleryChunk::Pack() -> void
{
    // The signed fields are sign-extended, as in the original.
    Data = static_cast<uint32_t>(((((CellRow << 10) | CellCol) << 3 | static_cast<int32_t>(StrikeType)) << 3) |
                                 ((static_cast<int32_t>(Seconds) + 1) * 0x4000000) | static_cast<int32_t>(CommanderId));
}

auto MCArtilleryChunk::Unpack() -> void
{
    CommanderId = static_cast<int8_t>(Data & 7);
    StrikeType = static_cast<int8_t>((Data >> 3) & 7);
    CellCol = static_cast<int32_t>((Data >> 6) & 0x3ff);
    CellRow = static_cast<int32_t>((Data >> 16) & 0x3ff);
    Seconds = static_cast<int8_t>(static_cast<uint8_t>(Data >> 26) - 1);
}

auto MCArtilleryChunk::EqualTo(MCArtilleryChunk* chunk) -> int
{
    return CommanderId == chunk->CommanderId && StrikeType == chunk->StrikeType && CellRow == chunk->CellRow &&
                   CellCol == chunk->CellCol && Seconds == chunk->Seconds
               ? 1
               : 0;
}
