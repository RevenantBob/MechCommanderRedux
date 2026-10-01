#pragma once

class AppearanceType;
class PacketFile;
class Shape;
class UserHeap;

/// <summary>
/// The sprite cache: the heaps shapes and appearance data come from, the sprite PAKs (one per appearance, full size
/// and 90-pixel), the mech part PAKs (legs, torsos, right and left arms, each in both sizes), and the loaded shapes
/// in least-recently-used order.
/// </summary>
/// <remarks>
/// Original source: <c>sprite\sprtmgr.cpp</c>, 0x40 bytes. The main sprite PAK's packets are themselves PAKs (one
/// per appearance); each part PAK's packets likewise (one per mech).
/// </remarks>
class SpriteManager
{
public:
    /// <summary>Clears the heaps, the appearance tables and the shape list (the part tables are left to init).</summary>
    /// <remarks>MCX.EXE: inlined at its one `new` in Scenario::init.</remarks>
    SpriteManager()
    {
        dataHeap = nullptr;
        shapeHeap = nullptr;
        spriteFiles90 = nullptr;
        spriteFiles = nullptr;
        numAppearances = 0;
        lastShape = nullptr;
        firstShape = nullptr;
    }

    /// <summary>
    /// Creates the shape heap (<paramref name="shapeHeapSize"/> bytes) and the data heap
    /// (<paramref name="dataHeapSize"/>), opens the sprite PAK <paramref name="spriteFileName"/> (and its 90-pixel
    /// twin) and the mech part PAKs.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006427e0</remarks>
    int32_t init(uint32_t shapeHeapSize, uint32_t dataHeapSize, char* spriteFileName);

    /// <summary>Opens legs, torsos, rArms, lArms and their "90" versions.</summary>
    /// <remarks>MCX.EXE @ 0x00642b00</remarks>
    int32_t initMechPacketFiles();

    /// <summary>Closes the sprite PAKs.</summary>
    /// <remarks>
    /// MCX.EXE @ 0x00643190 (FUN_00643190, called from Scenario's teardown; no symbol, the name is the port's).
    /// </remarks>
    void destroy();

    /// <summary>Allocates shape (packet) memory.</summary>
    /// <remarks>MCX.EXE @ 0x00643630</remarks>
    void* mallocShapeRAM(uint32_t size);
    /// <remarks>MCX.EXE @ 0x00643650</remarks>
    void freeShapeRAM(void* block);
    /// <summary>Walks the shape heap (a debug check).</summary>
    /// <remarks>MCX.EXE @ 0x00643670</remarks>
    void walkShapeHeap();
    /// <summary>Walks the data heap (a debug check).</summary>
    /// <remarks>MCX.EXE @ 0x00643690</remarks>
    void walkDataHeap();
    /// <summary>Allocates appearance data (types' tables, <see cref="Shape"/> records, user lists).</summary>
    /// <remarks>MCX.EXE @ 0x006436b0</remarks>
    void* mallocDataRAM(uint32_t size);
    /// <remarks>MCX.EXE @ 0x006436d0</remarks>
    void freeDataRAM(void* block);

    /// <summary>
    /// Frees every shape not used this turn or the last (and every ownerless one). <paramref name="sizeNeeded"/> is
    /// ignored.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006436f0</remarks>
    void dumpLRU(int32_t sizeNeeded);

    /// <summary>Frees every shape.</summary>
    /// <remarks>MCX.EXE @ 0x00643790</remarks>
    void dumpALL();

    /// <summary>
    /// Loads packet <paramref name="packetNum"/> of appearance <paramref name="appearanceNum"/>'s PAK (the 90-pixel
    /// one when <paramref name="zoomedOut"/>) as a shape owned by <paramref name="owner"/>, used at
    /// <paramref name="turnUsed"/>. Frees old shapes when the heap is full; null (and a render restart) when it
    /// still doesn't fit.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006437d0</remarks>
    Shape* getShapeData(uint32_t appearanceNum, uint32_t packetNum, int32_t turnUsed, AppearanceType* owner,
                        int zoomedOut);

    /// <summary>
    /// The same for a mech part: <paramref name="part"/> 0 legs, 1 torso, 2 right arm, 3 left arm of mech
    /// <paramref name="mechNum"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00643a00</remarks>
    Shape* getMechShapeData(uint32_t mechNum, uint32_t packetNum, int32_t part, int32_t turnUsed, AppearanceType* owner,
                            int zoomedOut);

    /// <summary>Opens the part PAKs of mech <paramref name="mechNum"/> ahead of use.</summary>
    /// <remarks>MCX.EXE @ 0x00643cf0</remarks>
    void touchMechShapeData(uint32_t mechNum, uint32_t packetNum, int32_t part);

    /// <summary>The size of packet <paramref name="packetNum"/> of appearance <paramref name="appearanceNum"/>'s PAK.</summary>
    /// <remarks>MCX.EXE @ 0x00643e40</remarks>
    int32_t getShapeSize(uint32_t appearanceNum, uint32_t packetNum);

    /// <summary>The number of packets in appearance <paramref name="appearanceNum"/>'s PAK (opening it).</summary>
    /// <remarks>MCX.EXE @ 0x00643f80 (FUN_00643f80: no symbol; the name is the port's).</remarks>
    int32_t getNumShapes(uint32_t appearanceNum);

    /// <summary>The shapes' heap.</summary>
    UserHeap* shapeHeap; // +0x00
    /// <summary>The appearance data heap.</summary>
    UserHeap* dataHeap; // +0x04
    /// <summary>The number of appearances in the sprite PAK.</summary>
    int32_t numAppearances; // +0x08
    /// <summary>The shape heap's size.</summary>
    uint32_t shapeHeapSize; // +0x0c
    /// <summary>
    /// [0] the preferred sprite PAK ("&lt;name&gt;90.pak", "&lt;name&gt;.pak" in the demo), then each appearance's
    /// own PAK once opened from it.
    /// </summary>
    PacketFile** spriteFiles; // +0x10
    /// <summary>
    /// [0] "&lt;name&gt;.pak", the fallback for appearances whose packet is empty in the preferred PAK; then their
    /// own PAKs. (The port's names: the original picks between the two by packet size, not by zoom.)
    /// </summary>
    PacketFile** spriteFiles90; // +0x14
    /// <summary>[0] legs.pak, then each mech's legs PAK (25 entries).</summary>
    PacketFile** legFiles; // +0x18
    /// <summary>[0] torsos.pak, then each mech's.</summary>
    PacketFile** torsoFiles; // +0x1c
    /// <summary>[0] rArms.pak, then each mech's.</summary>
    PacketFile** rArmFiles; // +0x20
    /// <summary>[0] lArms.pak, then each mech's.</summary>
    PacketFile** lArmFiles; // +0x24
    /// <summary>legs90.pak and each mech's.</summary>
    PacketFile** legFiles90; // +0x28
    /// <summary>torsos90.pak and each mech's.</summary>
    PacketFile** torsoFiles90; // +0x2c
    /// <summary>rArms90.pak and each mech's.</summary>
    PacketFile** rArmFiles90; // +0x30
    /// <summary>lArms90.pak and each mech's.</summary>
    PacketFile** lArmFiles90; // +0x34
    /// <summary>The oldest loaded shape.</summary>
    Shape* firstShape; // +0x38
    /// <summary>The newest loaded shape.</summary>
    Shape* lastShape; // +0x3c
};

/// <summary>The game's sprite manager.</summary>
extern SpriteManager* spriteManager;
/// <summary>Nonzero to use the 90-pixel sprites when zoomed out.</summary>
extern int use90PixelSprite;
/// <summary>Set when the shape cache had to be emptied mid-frame: the frame is rendered again.</summary>
extern int gRestartRender;
/// <summary>Set when shapes were freed; cleared by the next shape allocation.</summary>
extern int dumpedRecent;
