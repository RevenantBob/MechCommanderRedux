#pragma once

class PacketFile;
class UserHeap;
struct ObjectQueueNode;

/// <summary>
/// The terrain objects of a map, by terrain block: reads the map's <c>.obj</c> packet file (one packet per block of
/// 11-byte records: type, pixel offset, vertex, block, damage/commander flags) into a TBlk and an RBlk object list
/// per block, and the <c>.bdg</c> file of misc terrain objects (bridges, walls, forests).
/// </summary>
/// <remarks>
/// Original source: <c>object\objblck.cpp</c>; 0x18 bytes, allocated by Terrain::init (the inline constructor zeroes
/// every field). The global is <c>objBlockManager</c> (terrain\terrain.h).
/// </remarks>
class ObjectBlockManager
{
public:
    /// <summary>Destroys every object list, the heap and the packet file.</summary>
    /// <remarks>MCX.EXE @ 0x0068d120</remarks>
    void destroy();
    /// <summary>
    /// Opens <c>terrainPath\&lt;fileName&gt;.obj</c>, sizes the heap from its packet count, sets up every block's
    /// object lists (update(1)), then places the misc terrain objects of <c>&lt;fileName&gt;.bdg</c>.
    /// </summary>
    /// <returns>0, a file error, or 0xbaaa00nn when out of memory.</returns>
    /// <remarks>MCX.EXE @ 0x0068d180</remarks>
    int32_t init(const char* fileName);
    /// <summary>With <paramref name="reload"/>, reads every block packet and sets up its object lists.</summary>
    /// <remarks>MCX.EXE @ 0x0068dbc0</remarks>
    int32_t update(int reload);
    /// <summary>Updates every object of every block list (finding each by index from the list head).</summary>
    /// <remarks>MCX.EXE @ 0x0068dc90</remarks>
    void updateAllObjects();
    /// <summary>Unlinks every block list from objectList and destroys it with its objects.</summary>
    /// <remarks>MCX.EXE @ 0x0068dd90</remarks>
    void destroyAllObjects();

protected:
    /// <summary>
    /// Makes the TBlk and RBlk lists of block <paramref name="listIndex"/> / 2 and fills them from the block's
    /// packet (in <see cref="objectData"/>, <paramref name="packetSize"/> bytes): trees in TBlk, other objects in
    /// RBlk, turrets and gates in the first object list.
    /// </summary>
    /// <returns>0, or 0xbaaa001d when the block's lists already exist.</returns>
    /// <remarks>MCX.EXE @ 0x0068d720</remarks>
    int32_t setupObjectQueue(uint32_t listIndex, uint32_t packetSize);

public:
    /// <summary>Size of <see cref="objectHeap"/>: 0x5600 plus 0x18 per block.</summary>
    int32_t heapSize = 0; // +0x00
    /// <summary>The heap the lists table and the packet buffer come from.</summary>
    UserHeap* objectHeap = nullptr; // +0x04
    /// <summary>Two lists per block: TBlk (trees and light walls) then RBlk (everything else).</summary>
    ObjectQueueNode** objectLists = nullptr; // +0x08
    /// <summary>Zeroed by the constructor; not used in objblck.cpp.</summary>
    int32_t unknown0C = 0; // +0x0c
    /// <summary>The current block packet (22000 bytes).</summary>
    uint8_t* objectData = nullptr; // +0x10
    /// <summary>The map's <c>.obj</c> packet file.</summary>
    PacketFile* objectFile = nullptr; // +0x14
};
