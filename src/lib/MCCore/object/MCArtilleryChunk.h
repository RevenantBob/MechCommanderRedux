#pragma once

#include "lib/MCVector3D.h"

/// <summary>
/// A multiplayer message announcing an artillery strike: the commander, the strike type, the map cell and the
/// seconds to impact, packed into one 32-bit word.
/// </summary>
/// <remarks>
/// Original source: <c>object\artlry.cpp</c>. <see cref="Data"/> is what goes over the network: bits 0-2 commanderId,
/// 3-5 strikeType, 6-15 cellCol, 16-25 cellRow, 26-31 seconds + 1.
/// </remarks>
class MCArtilleryChunk
{
public:
    /// <summary>Fills the chunk for a strike at <paramref name="location"/> (converted to a map cell).</summary>
    void Build(int32_t commanderId, int32_t strikeType, MCVector3D location, int32_t seconds);
    /// <summary>Packs the fields into <see cref="Data"/>.</summary>
    void Pack();
    /// <summary>Unpacks <see cref="Data"/> into the fields.</summary>
    void Unpack();
    /// <summary>Whether the unpacked fields of both chunks match.</summary>
    int EqualTo(MCArtilleryChunk* chunk) const;

    int8_t CommanderId = 0;
    int8_t StrikeType = 0;
    int32_t CellRow = 0;
    int32_t CellCol = 0;
    /// <summary>Seconds to impact (-1 = the type's nominal time).</summary>
    int8_t Seconds = 0;
    /// <summary>The packed word sent over the network.</summary>
    uint32_t Data = 0;
};
