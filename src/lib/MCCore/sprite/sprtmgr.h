#pragma once

class MCAppearanceType;
class MCPacketFile;
class MCShape;

/// <summary>
/// The sprite cache: the blocks shapes and appearance data live in, the sprite PAKs (one per appearance, full size
/// and 90-pixel), the mech part PAKs (legs, torsos, right and left arms, each in both sizes), and the loaded shapes
/// in least-recently-used order.
/// </summary>
/// <remarks>
/// Original source: <c>sprite\sprtmgr.cpp</c>, 0x40 bytes. The main sprite PAK's packets are themselves PAKs (one
/// per appearance); each part PAK's packets likewise (one per mech).
/// </remarks>
class MCSpriteManager
{
public:
    /// <summary>
    /// Opens the sprite PAK <paramref name="spriteFileName"/> (and its 90-pixel twin) and the mech part PAKs. The
    /// original made its shape and data heaps here, sized by the scenario.
    /// </summary>
    int32_t Init(char* spriteFileName);

    /// <summary>Opens legs, torsos, rArms, lArms and their "90" versions.</summary>
    int32_t InitMechPacketFiles();

    /// <summary>Closes the sprite PAKs and frees every block still allocated.</summary>
    /// <remarks>Called from the scenario's teardown.</remarks>
    void Destroy();

    /// <summary>Allocates zeroed shape (packet) memory, registered with the renderers; null for a size of 0.</summary>
    void* MallocShapeRam(uint32_t size);
    /// <summary>Unregisters and frees a block of <see cref="MallocShapeRam"/> (anything else is ignored).</summary>
    void FreeShapeRam(void* block);
    /// <summary>
    /// Allocates zeroed appearance data (types' tables, <see cref="MCShape"/> records, user lists); null for a size of
    /// 0.
    /// </summary>
    void* MallocDataRam(uint32_t size);
    /// <summary>Frees a block of <see cref="MallocDataRam"/> (anything else is ignored).</summary>
    void FreeDataRam(void* block);

    /// <summary>
    /// Frees every shape not used this turn or the last (and every ownerless one). <paramref name="sizeNeeded"/> is
    /// ignored.
    /// </summary>
    void DumpLru(int32_t sizeNeeded);

    /// <summary>Frees every shape.</summary>
    void DumpAll();

    /// <summary>
    /// Loads packet <paramref name="packetNum"/> of appearance <paramref name="appearanceNum"/>'s PAK (the 90-pixel
    /// one when <paramref name="zoomedOut"/>) as a shape owned by <paramref name="owner"/>, used at
    /// <paramref name="turnUsed"/>. Frees old shapes when the heap is full; null (and a render restart) when it
    /// still doesn't fit.
    /// </summary>
    MCShape* GetShapeData(uint32_t appearanceNum, uint32_t packetNum, int32_t turnUsed, MCAppearanceType* owner,
                          int zoomedOut);

    /// <summary>
    /// The same for a mech part: <paramref name="part"/> 0 legs, 1 torso, 2 right arm, 3 left arm of mech
    /// <paramref name="mechNum"/>.
    /// </summary>
    MCShape* GetMechShapeData(uint32_t mechNum, uint32_t packetNum, int32_t part, int32_t turnUsed,
                              MCAppearanceType* owner, int zoomedOut);

    /// <summary>Opens the part PAKs of mech <paramref name="mechNum"/> ahead of use.</summary>
    void TouchMechShapeData(uint32_t mechNum, uint32_t packetNum, int32_t part);

    /// <summary>The size of packet <paramref name="packetNum"/> of appearance <paramref name="appearanceNum"/>'s PAK.</summary>
    int32_t GetShapeSize(uint32_t appearanceNum, uint32_t packetNum);

    /// <summary>The number of packets in appearance <paramref name="appearanceNum"/>'s PAK (opening it).</summary>
    int32_t GetNumShapes(uint32_t appearanceNum);

    /// <summary>
    /// The blocks of <see cref="MallocShapeRam"/> (the original's shape heap). Phase 3 gives each block its owner.
    /// </summary>
    std::unordered_map<void*, std::unique_ptr<uint8_t[]>> ShapeBlocks;
    /// <summary>The blocks of <see cref="MallocDataRam"/> (the original's data heap).</summary>
    std::unordered_map<void*, std::unique_ptr<uint8_t[]>> DataBlocks;
    /// <summary>The number of appearances in the sprite PAK.</summary>
    int32_t NumAppearances = 0;
    /// <summary>
    /// [0] the preferred sprite PAK ("&lt;name&gt;90.pak", "&lt;name&gt;.pak" in the demo), then each appearance's
    /// own PAK once opened from it.
    /// </summary>
    std::vector<MCPacketFile*> SpriteFiles;
    /// <summary>
    /// [0] "&lt;name&gt;.pak", the fallback for appearances whose packet is empty in the preferred PAK; then their
    /// own PAKs. (The port's names: the original picks between the two by packet size, not by zoom.)
    /// </summary>
    std::vector<MCPacketFile*> SpriteFiles90;
    /// <summary>[0] legs.pak, then each mech's legs PAK (25 entries).</summary>
    std::vector<MCPacketFile*> LegFiles;
    /// <summary>[0] torsos.pak, then each mech's.</summary>
    std::vector<MCPacketFile*> TorsoFiles;
    /// <summary>[0] rArms.pak, then each mech's.</summary>
    std::vector<MCPacketFile*> RArmFiles;
    /// <summary>[0] lArms.pak, then each mech's.</summary>
    std::vector<MCPacketFile*> LArmFiles;
    /// <summary>legs90.pak and each mech's.</summary>
    std::vector<MCPacketFile*> LegFiles90;
    /// <summary>torsos90.pak and each mech's.</summary>
    std::vector<MCPacketFile*> TorsoFiles90;
    /// <summary>rArms90.pak and each mech's.</summary>
    std::vector<MCPacketFile*> RArmFiles90;
    /// <summary>lArms90.pak and each mech's.</summary>
    std::vector<MCPacketFile*> LArmFiles90;
    /// <summary>The oldest loaded shape.</summary>
    MCShape* FirstShape = nullptr;
    /// <summary>The newest loaded shape.</summary>
    MCShape* LastShape = nullptr;
};

/// <summary>The game's sprite manager.</summary>
extern MCSpriteManager* SpriteManager;
/// <summary>Nonzero to use the 90-pixel sprites when zoomed out.</summary>
extern int Use90PixelSprite;
/// <summary>Set when the shape cache had to be emptied mid-frame: the frame is rendered again.</summary>
extern int GRestartRender;
/// <summary>Set when shapes were freed; cleared by the next shape allocation.</summary>
extern int DumpedRecent;
