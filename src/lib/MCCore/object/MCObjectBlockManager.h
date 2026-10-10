#pragma once

class MCObjectList;
class MCPacketFile;

/// <summary>One terrain object of a map's <c>.obj</c> block packet: its type, place, and damage and commander.</summary>
/// <remarks>Fixed layout: the <c>.obj</c> file's 11-byte record.</remarks>
#pragma pack(push, 1)
struct MCObjData
{
    /// <summary>The object's type number; -1 for none.</summary>
    int16_t ObjTypeNum = 0;
    /// <summary>The pixel offset on its vertex.</summary>
    int16_t PixelOffsetX = 0;
    int16_t PixelOffsetY = 0;
    /// <summary>The vertex in its block.</summary>
    int16_t VertexNumber = 0;
    /// <summary>The terrain block.</summary>
    int16_t BlockNumber = 0;
    /// <summary>Low nibble: placed destroyed (nonzero); high nibble: a building's tile (its commander).</summary>
    uint8_t Damage = 0;
};
#pragma pack(pop)

static_assert(sizeof(MCObjData) == 11);

/// <summary>
/// The terrain objects of a map, by terrain block: reads the map's <c>.obj</c> packet file (one packet per block of
/// <see cref="MCObjData"/> records) into a TBlk and an RBlk object list per block, and the <c>.bdg</c> file of misc
/// terrain objects (bridges, walls, forests).
/// </summary>
/// <remarks>Original source: <c>object\objblck.cpp</c>. The terrain owns it (<see cref="MCTerrain::ObjectBlocks"/>);
/// the object system's queue owns the lists it makes.</remarks>
class MCObjectBlockManager
{
    /// <summary>Only <see cref="Create"/> makes one.</summary>
    struct Key
    {
        explicit Key() = default;
    };

public:
    /// <summary>A manager with no file open.</summary>
    explicit MCObjectBlockManager(Key);

    /// <summary>Takes the block lists, with their objects, out of the object lists.</summary>
    ~MCObjectBlockManager();
    MCObjectBlockManager(const MCObjectBlockManager&) = delete;
    MCObjectBlockManager& operator=(const MCObjectBlockManager&) = delete;

    /// <summary>
    /// Opens <c>terrainPath\&lt;fileName&gt;.obj</c>, makes every block's object lists, then places the misc terrain
    /// objects of <c>&lt;fileName&gt;.bdg</c> (when there is one).
    /// </summary>
    static std::expected<std::unique_ptr<MCObjectBlockManager>, std::string> Create(std::string_view fileName);

    /// <summary>Updates every object of every block list (finding each by index from the list's start).</summary>
    void UpdateAllObjects();
    /// <summary>The map's <c>.obj</c> packet file.</summary>
    MCPacketFile* ObjectFile() const { return _ObjectFile.get(); }

private:
    /// <summary>Reads every block packet and makes its object lists.</summary>
    void LoadBlocks();
    /// <summary>
    /// Makes the TBlk and RBlk lists of block <paramref name="blockNumber"/> and fills them from its packet
    /// (<paramref name="packet"/>): trees in TBlk, other objects in RBlk, turrets and gates in the first object list.
    /// </summary>
    void SetupObjectQueue(uint32_t blockNumber, std::span<const uint8_t> packet);
    /// <summary>Places the misc terrain objects of the <c>.bdg</c> file, 16-byte records of block, vertex, kind and
    /// whether it starts destroyed.</summary>
    void LoadMiscTerrainObjects(std::span<const uint8_t> records);
    /// <summary>
    /// The next part id for an object on (<paramref name="blockNumber"/>, <paramref name="vertexNumber"/>): the
    /// vertex's base id plus how many are already on it, which is then counted (up to 7).
    /// </summary>
    int32_t NextPartId(int32_t blockNumber, int32_t vertexNumber);

    std::unique_ptr<MCPacketFile> _ObjectFile;
    /// <summary>Two lists per block: TBlk (trees and light walls) then RBlk (everything else).</summary>
    std::vector<MCObjectList*> _BlockLists;
    /// <summary>Objects placed on each map vertex so far (up to 7), for their part ids.</summary>
    std::vector<uint8_t> _VertexObjectCount;
};
