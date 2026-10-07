#pragma once

class MCPacketFile;
struct MCObjectQueueNode;

/// <summary>
/// The terrain objects of a map, by terrain block: reads the map's <c>.obj</c> packet file (one packet per block of
/// 11-byte records: type, pixel offset, vertex, block, damage/commander flags) into a TBlk and an RBlk object list
/// per block, and the <c>.bdg</c> file of misc terrain objects (bridges, walls, forests).
/// </summary>
/// <remarks>
/// Original source: <c>object\objblck.cpp</c>; 0x18 bytes, allocated by Terrain::init (the inline constructor zeroes
/// every field). The terrain owns it (<see cref="MCTerrain::ObjectBlocks"/>).
/// </remarks>
class MCObjectBlockManager
{
public:
    /// <summary>Destroys every object list, the heap and the packet file.</summary>
    void Destroy();
    /// <summary>
    /// Opens <c>terrainPath\&lt;fileName&gt;.obj</c>, sizes heapSize from its packet count, sets up every block's
    /// object lists (update(1)), then places the misc terrain objects of <c>&lt;fileName&gt;.bdg</c>.
    /// </summary>
    /// <returns>0, a file error, or 0xbaaa00nn when out of memory.</returns>
    int32_t Init(const char* fileName);
    /// <summary>With <paramref name="reload"/>, reads every block packet and sets up its object lists.</summary>
    int32_t Update(int reload);
    /// <summary>Updates every object of every block list (finding each by index from the list head).</summary>
    void UpdateAllObjects();
    /// <summary>Unlinks every block list from objectList and destroys it with its objects.</summary>
    void DestroyAllObjects();

protected:
    /// <summary>
    /// Makes the TBlk and RBlk lists of block <paramref name="listIndex"/> / 2 and fills them from the block's
    /// packet (in <see cref="ObjectData"/>, <paramref name="packetSize"/> bytes): trees in TBlk, other objects in
    /// RBlk, turrets and gates in the first object list.
    /// </summary>
    /// <returns>0, or 0xbaaa001d when the block's lists already exist.</returns>
    int32_t SetupObjectQueue(uint32_t listIndex, uint32_t packetSize);

public:
    /// <summary>The original's heap size, 0x5600 plus 0x18 per block (unused).</summary>
    int32_t HeapSize = 0;
    /// <summary>Two lists per block: TBlk (trees and light walls) then RBlk (everything else).</summary>
    std::unique_ptr<MCObjectQueueNode*[]> ObjectLists;
    /// <summary>The current block packet (22000 bytes).</summary>
    std::unique_ptr<uint8_t[]> ObjectData;
    /// <summary>The map's <c>.obj</c> packet file.</summary>
    MCPacketFile* ObjectFile = nullptr;
};
