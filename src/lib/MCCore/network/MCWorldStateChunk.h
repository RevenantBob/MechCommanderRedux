#pragma once

// Original source: mcx\network\multplyr.cpp (WorldStateChunk).

class MCGameObject;
class MCVector3D;

/// <summary>
/// A world event the server tells the clients about, packed into one 32-bit word: a mine laid or set off, a terrain
/// object set on fire, an artillery strike, a mission-script message, a pilot's kill.
/// </summary>
/// <remarks>
/// Only <see cref="Data"/> goes over the network. Packing, by type (bit 0 is the lowest; all start with the 4-bit type):
/// mine: tileCol 4-13, tileRow 14-23, param2 (mine state) 24-26, param1 (team) 27;
/// terrain fire: item 4-6, vertexNum 7-15, blockNum 16-23, param1 (seconds) 24-31;
/// artillery: tileCol 4-13, tileRow 14-23, param2 + 1 (seconds) 24-28, param1 (strike type) 29-31;
/// script message: param1 (message) 4-11, param2 + 32000 (value) 12-27;
/// pilot kill: param2 (kill kind) 4-6, param1 (mover roster index) 7-11.
/// </remarks>
class MCWorldStateChunk
{
public:
    /// <summary>A mine laid, set off or cleared.</summary>
    static constexpr int8_t Mine = 0;
    /// <summary>A terrain object set on fire.</summary>
    static constexpr int8_t TerrainFire = 1;
    /// <summary>2-7: an artillery strike called by commander (type - 2).</summary>
    static constexpr int8_t Artillery = 2;
    /// <summary>The last artillery type (commander 5).</summary>
    static constexpr int8_t LastArtillery = 7;
    static constexpr int8_t MissionScriptMessage = 8;
    static constexpr int8_t PilotKillStat = 9;
    /// <summary>The number of types (the tally's size).</summary>
    static constexpr int8_t NumTypes = 10;

    /// <summary>
    /// A mine at map cell (<paramref name="tileRow"/>, <paramref name="tileCol"/>) of team <paramref name="teamId"/>
    /// (0-2; 1 is the Clan layout): <paramref name="mineState"/> 0-3, where 3 (exploded) adds
    /// <paramref name="explosionType"/> 0-2.
    /// </summary>
    void BuildMine(int32_t tileRow, int32_t tileCol, int32_t teamId, int32_t mineState, int32_t explosionType);

    /// <summary>
    /// Terrain object <paramref name="object"/> (its part id split into block, vertex and item) set on fire for
    /// <paramref name="seconds"/> (0-255).
    /// </summary>
    void BuildTerrainFire(const MCGameObject& object, int32_t seconds);

    /// <summary>
    /// An artillery strike of <paramref name="strikeType"/> (0-7) by commander <paramref name="commanderId"/> (0-5) at
    /// <paramref name="location"/>, landing in <paramref name="seconds"/> (-1-30).
    /// </summary>
    void BuildArtillery(int32_t commanderId, int32_t strikeType, const MCVector3D& location, int32_t seconds);

    /// <summary>Mission-script message <paramref name="message"/> (0-255) with <paramref name="value"/> (±32000).</summary>
    void BuildMissionScriptMessage(int32_t message, int32_t value);

    /// <summary>
    /// A kill of kind <paramref name="killType"/> (0-7) by the mover at <paramref name="moverIndex"/> of the roster of
    /// <paramref name="numMovers"/>.
    /// </summary>
    void BuildPilotKillStat(int32_t moverIndex, int32_t killType, int32_t numMovers);

    /// <summary>Packs the fields into <see cref="Data"/>.</summary>
    void Pack();

    /// <summary>Unpacks <see cref="Data"/> into the fields (fatal on a type it doesn't list).</summary>
    void Unpack();

    /// <summary>Whether every unpacked field matches <paramref name="chunk"/>'s.</summary>
    bool EqualTo(const MCWorldStateChunk& chunk) const;

    /// <summary>The type (<see cref="Mine"/> ... <see cref="PilotKillStat"/>).</summary>
    int8_t Type = Mine;
    int16_t TileRow = -1;
    int16_t TileCol = -1;
    /// <summary>Terrain fire: the object's part id (0x1000 + blockNum * 0xc80 + vertexNum * 8 + item).</summary>
    int32_t ObjectWid = 0;
    int32_t BlockNum = 0;
    int32_t VertexNum = 0;
    int8_t Item = 0;
    /// <summary>The first value (see the type).</summary>
    int32_t Param1 = 0;
    /// <summary>The second value (see the type).</summary>
    int32_t Param2 = 0;
    /// <summary>The packed word sent over the network.</summary>
    uint32_t Data = 0;
};
