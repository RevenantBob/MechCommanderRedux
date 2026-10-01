#pragma once

// Original source: mcx\lib\heap.cpp (and the inline methods of lib\heap.h). The game's memory heaps: HeapManager, a
// block of memory reserved and committed in pages (used directly as a raw buffer by bit flags, element pools, ...),
// and UserHeap, a malloc/free heap inside such a block (systemHeap, guiHeap, the ABL, sprite and mission heaps).
//
// Per docs/port/translation.md the heaps keep their interfaces but allocate from the C++ heap: a HeapManager's block
// is one zeroed allocation of its size; a UserHeap hands out separate allocations and keeps its size and the bytes in
// use only as bookkeeping, so coreLeft/totalCoreLeft still answer as the original's did while nothing runs out. The
// original's block allocator internals (HeapBlock headers, relink, unlink, sort, mergeWithLower @ 0x00648690..
// 0x00648830) and its virtual-memory queries (NTBug_VirtualQuery @ 0x00647330, VMQueryHelp @ 0x006475e0) have no
// counterpart.

/// <summary>Heap error: every byte of the heap is already committed.</summary>
inline constexpr int32_t HEAP_ALL_COMMITTED = static_cast<int32_t>(0xBADD0001);
/// <summary>Heap error: a consistency walk found a damaged block list.</summary>
inline constexpr int32_t HEAP_CHECK_FAILED = static_cast<int32_t>(0xBADD0005);
/// <summary>Heap error: asked to commit more than the heap's size.</summary>
inline constexpr int32_t COMMIT_TOO_LARGE = static_cast<int32_t>(0xBADD0007);
/// <summary>Heap error: the commit itself failed.</summary>
inline constexpr int32_t COMMIT_FAILED = static_cast<int32_t>(0xBADD0008);
/// <summary>Heap error: the heap's memory couldn't be reserved.</summary>
inline constexpr int32_t RESERVE_FAILED = static_cast<int32_t>(0xBADD0009);

/// <summary>What <see cref="HeapManager::heapType"/> answers.</summary>
inline constexpr uint8_t MASTER_HEAP = 0;
/// <summary>What <see cref="UserHeap::heapType"/> answers.</summary>
inline constexpr uint8_t USER_HEAP = 1;

/// <summary>The size of the heap list's table.</summary>
inline constexpr int32_t MAX_HEAPS = 256;

/// <summary>
/// The part of Win32's SYSTEM_INFO the heaps use: the page size commits are rounded to. The port fills it with the
/// values Windows 9x/NT gave on x86.
/// </summary>
struct MCSystemInfo
{
    /// <summary>Bytes per page (4096).</summary>
    uint32_t dwPageSize = 4096;
    /// <summary>Granularity of reservations (65536).</summary>
    uint32_t dwAllocationGranularity = 65536;
};

/// <summary>
/// What <see cref="HeapManager::VMQuery"/> tells about an address: its region and its block (Richter's VMQUERY, 0x2c
/// bytes in the original).
/// </summary>
struct VMQUERY
{
    /// <summary>Start of the region.</summary>
    void* pvRgnBaseAddress = nullptr; // +0x00
    /// <summary>The region's protection when reserved.</summary>
    uint32_t dwRgnProtection = 0; // +0x04
    /// <summary>The region's size.</summary>
    uint32_t dwRgnSize = 0; // +0x08
    /// <summary>MEM_PRIVATE, MEM_FREE, ...</summary>
    uint32_t dwRgnStorage = 0; // +0x0c
    /// <summary>Number of blocks in the region.</summary>
    uint32_t dwRgnBlocks = 0; // +0x10
    /// <summary>Number of guard blocks.</summary>
    uint32_t dwRgnGuardBlks = 0; // +0x14
    /// <summary>Whether the region is a thread stack.</summary>
    int32_t fRgnIsAStack = 0; // +0x18
    /// <summary>Start of the block.</summary>
    void* pvBlkBaseAddress = nullptr; // +0x1c
    /// <summary>The block's protection.</summary>
    uint32_t dwBlkProtection = 0; // +0x20
    /// <summary>The block's size.</summary>
    uint32_t dwBlkSize = 0; // +0x24
    /// <summary>MEM_COMMIT, MEM_RESERVE, MEM_FREE.</summary>
    uint32_t dwBlkStorage = 0; // +0x28
};

/// <summary>
/// A block of memory of a fixed size, reserved then committed. Classes that need a big raw buffer (bit flags, element
/// pools, terrain tables) use one directly through <see cref="getHeapPtr"/>; <see cref="UserHeap"/> builds a
/// malloc/free heap on it.
/// </summary>
/// <remarks>Original source: <c>lib\heap.cpp</c>, 0x1c bytes. The port's block is a zeroed C++ allocation.</remarks>
class HeapManager
{
public:
    /// <remarks>MCX.EXE @ 0x00646ff0</remarks>
    HeapManager();
    /// <remarks>MCX.EXE @ 0x00646f20</remarks>
    virtual ~HeapManager();
    HeapManager(const HeapManager&) = delete;
    HeapManager& operator=(const HeapManager&) = delete;

    /// <summary>What kind of heap this is (<see cref="MASTER_HEAP"/>).</summary>
    /// <remarks>MCX.EXE @ 0x00646f40</remarks>
    virtual uint8_t heapType() { return MASTER_HEAP; }

    /// <summary>Releases the block and clears the fields.</summary>
    /// <remarks>MCX.EXE @ 0x00646f50</remarks>
    void destroy();

    /// <summary>Clears the fields.</summary>
    /// <remarks>MCX.EXE @ 0x00647010</remarks>
    void init();

    /// <summary>The block, as <see cref="getHeapPtr"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00647050</remarks>
    operator uint8_t*();

    /// <summary>The block, or null unless it is reserved and committed.</summary>
    /// <remarks>MCX.EXE @ 0x00647070</remarks>
    uint8_t* getHeapPtr();

    /// <summary>Reserves a block of <paramref name="memSize"/> bytes.</summary>
    /// <returns>0, or <see cref="RESERVE_FAILED"/>.</returns>
    /// <remarks>MCX.EXE @ 0x006470b0</remarks>
    int32_t createHeap(uint32_t memSize);

    /// <summary>Commits <paramref name="commitSize"/> more bytes of the block (0: all of it).</summary>
    /// <returns>0, <see cref="COMMIT_TOO_LARGE"/>, <see cref="HEAP_ALL_COMMITTED"/> or <see cref="COMMIT_FAILED"/>.</returns>
    /// <remarks>MCX.EXE @ 0x00647130</remarks>
    int32_t commitHeap(uint32_t commitSize = 0);

    /// <summary>Decommits <paramref name="decommitSize"/> bytes (0: all of it).</summary>
    /// <remarks>MCX.EXE @ 0x00647290</remarks>
    int32_t decommitHeap(uint32_t decommitSize = 0);

    /// <summary>Writes the heap's memory map to <c>memdump.txt</c>.</summary>
    /// <remarks>MCX.EXE @ 0x00647b50. The original walked the whole process address space; the port the heap's block.</remarks>
    void MemoryDump();

    /// <summary>Who created the heap (a return address in the original; 0 in the port).</summary>
    /// <remarks>MCX.EXE @ 0x00649050</remarks>
    uint32_t owner() { return whoMadeMe; }

    /// <summary>The committed size.</summary>
    /// <remarks>MCX.EXE @ 0x00649070</remarks>
    uint32_t tSize() { return committedSize; }

    /// <summary>The page size and allocation granularity, filled on the first <see cref="createHeap"/>.</summary>
    static MCSystemInfo* systemInfo;

protected:
    /// <summary>Describes the memory at <paramref name="address"/>.</summary>
    /// <returns>Nonzero when the address lies in the heap's block.</returns>
    /// <remarks>MCX.EXE @ 0x006473d0</remarks>
    int VMQuery(void* address, VMQUERY* query);

    /// <summary>The name of a storage type (MEM_PRIVATE, ...).</summary>
    /// <remarks>MCX.EXE @ 0x00647770</remarks>
    const char* GetMemStorageText(uint32_t memStorage);

    /// <summary>The text of a page protection (<c>-RW-</c>, ...), with the guard/no-cache flags when asked.</summary>
    /// <remarks>MCX.EXE @ 0x00647800</remarks>
    char* GetProtectText(uint32_t protect, char* buffer, int showFlags);

    /// <summary>A memory-dump line for a region.</summary>
    /// <remarks>MCX.EXE @ 0x006479b0</remarks>
    void ConstructRgnInfoLine(VMQUERY* query, char* buffer, int bufferSize);

    /// <summary>A memory-dump line for a block.</summary>
    /// <remarks>MCX.EXE @ 0x00647ad0</remarks>
    void ConstructBlkInfoLine(VMQUERY* query, char* buffer, int bufferSize);

public:
    /// <summary>The block (reserved address in the original).</summary>
    uint8_t* heap = nullptr; // +0x04
    /// <summary>Nonzero once the block is reserved.</summary>
    int32_t memReserved = 0; // +0x08
    /// <summary>The block's size.</summary>
    uint32_t totalSize = 0; // +0x0c
    /// <summary>Bytes committed so far (page-rounded).</summary>
    uint32_t committedSize = 0; // +0x10
    /// <summary>Who made it (the caller's return address in the original).</summary>
    uint32_t whoMadeMe = 0; // +0x14
    /// <summary>Next heap in a chain; cleared by <see cref="init"/>, read only by the heap dump.</summary>
    HeapManager* nxt = nullptr; // +0x18
};

/// <summary>
/// A malloc/free heap of a fixed size (systemHeap, guiHeap and the per-system heaps sized from SYSTEM.CFG and the
/// mission files).
/// </summary>
/// <remarks>
/// Original source: <c>lib\heap.cpp</c>, 0x38 bytes. The original carved blocks with 8-byte headers out of the
/// HeapManager block, kept free blocks in a size-sorted ring and failed when none fit. The port allocates each block
/// from the C++ heap and never fails for lack of room; the heap's size and the (original-rounded) bytes in use are kept
/// so <see cref="coreLeft"/> and <see cref="totalCoreLeft"/> read as before.
/// </remarks>
class UserHeap : public HeapManager
{
public:
    /// <remarks>MCX.EXE @ 0x00647cd0</remarks>
    UserHeap();
    /// <remarks>MCX.EXE @ 0x00647ed0</remarks>
    ~UserHeap() override;

    /// <summary>What kind of heap this is (<see cref="USER_HEAP"/>).</summary>
    /// <remarks>MCX.EXE @ 0x00647d30</remarks>
    uint8_t heapType() override { return USER_HEAP; }

    /// <summary>Sets the heap up with <paramref name="memSize"/> bytes, named <paramref name="heapId"/> for reports.</summary>
    /// <returns>0, or the reason it failed.</returns>
    /// <remarks>MCX.EXE @ 0x00647d40</remarks>
    int32_t init(uint32_t memSize, const char* heapId = nullptr);

    /// <summary>Frees every block and clears the heap.</summary>
    /// <remarks>MCX.EXE @ 0x00647f00</remarks>
    void destroy();

    /// <summary>The free bytes in the heap (the sum of the free blocks' payloads in the original).</summary>
    /// <remarks>MCX.EXE @ 0x00647f70</remarks>
    uint32_t totalCoreLeft();

    /// <summary>The largest free block's payload (the whole free space in the port, which doesn't fragment).</summary>
    /// <remarks>MCX.EXE @ 0x00647fd0</remarks>
    uint32_t coreLeft();

    /// <summary>Allocates <paramref name="memSize"/> bytes.</summary>
    /// <returns>The memory, or null for a size of 0.</returns>
    /// <remarks>MCX.EXE @ 0x00648020</remarks>
    void* malloc(uint32_t memSize);

    /// <summary>Frees a block of this heap.</summary>
    /// <returns>The size of the block freed, or 0 when <paramref name="memBlock"/> isn't one of this heap's.</returns>
    /// <remarks>MCX.EXE @ 0x00648230</remarks>
    int32_t free(void* memBlock);

    /// <summary>
    /// Original behaviour: never implemented in MCX.EXE, it returns null whatever the size.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006483b0</remarks>
    void* calloc(uint32_t memSize);

    /// <summary>
    /// Checks the heap and, when <paramref name="printIt"/> is set, logs its blocks (the free ones only unless
    /// <paramref name="skipAllocated"/> is 0).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006483c0</remarks>
    void walkHeap(int printIt = 0, int skipAllocated = 0, const char* mesg = nullptr);

    /// <summary>The heap's error state (0, or <see cref="HEAP_CHECK_FAILED"/>).</summary>
    /// <remarks>MCX.EXE @ 0x00648670</remarks>
    int32_t getLastError() { return heapState; }

    /// <summary>Whether <paramref name="memBlock"/> was allocated by this heap and not freed yet (port helper).</summary>
    bool owns(const void* memBlock) const;

    /// <summary>Unused by the port (the first block of the heap in the original).</summary>
    void* heapStart = nullptr; // +0x1c
    /// <summary>Unused by the port (the end sentinel block in the original).</summary>
    void* heapEnd = nullptr; // +0x20
    /// <summary>Unused by the port (the free-block ring in the original).</summary>
    void* firstNearBlock = nullptr; // +0x24
    /// <summary>The heap's size; nonzero once <see cref="init"/> succeeded (callers test it).</summary>
    uint32_t heapSize = 0; // +0x28
    /// <summary>Cleared by the constructor; no other use is known.</summary>
    int32_t unknown2C = 0; // +0x2c
    /// <summary>The heap's error state.</summary>
    int32_t heapState = 0; // +0x30
    /// <summary>The heap's name, for reports.</summary>
    char* heapName = nullptr; // +0x34

private:
    /// <summary>Size of the free space when the heap is empty (the original's first free block).</summary>
    uint32_t _InitialFree = 0;
    /// <summary>Bytes in use, counted in the original's block sizes (payload + 8 rounded to 4, at least 16).</summary>
    uint32_t _UsedBytes = 0;
    /// <summary>The live blocks and their original-rounded sizes.</summary>
    std::unordered_map<void*, uint32_t> _Blocks;
};

/// <summary>A table of heaps for the heap report (<see cref="dumpLog"/>).</summary>
/// <remarks>Original source: <c>lib\heap.cpp</c>, 0x400 bytes (256 pointers).</remarks>
class HeapList
{
public:
    /// <summary>Puts <paramref name="newHeap"/> in the first free slot (nothing when the table is full).</summary>
    /// <remarks>MCX.EXE @ 0x006488f0</remarks>
    void addHeap(HeapManager* newHeap);

    /// <summary>Clears <paramref name="oldHeap"/>'s slot.</summary>
    /// <remarks>MCX.EXE @ 0x00648940</remarks>
    void removeHeap(HeapManager* oldHeap);

    /// <summary>Writes every heap's size, free space and fragmentation to <c>heap.dump.log</c>.</summary>
    /// <remarks>MCX.EXE @ 0x00648c80</remarks>
    void dumpLog();

    /// <summary>The heaps (null for a free slot).</summary>
    HeapManager* heapRecords[MAX_HEAPS] = {}; // +0x000
};

class File;

/// <summary>Parses hexadecimal digits after a two-character prefix (<c>0x1F</c>); 0 when a digit isn't hex.</summary>
/// <remarks>MCX.EXE @ 0x00648990</remarks>
uint32_t textToLong(const char* num);

/// <summary>Writes <paramref name="num"/> as eight hex digits into <paramref name="result"/> when it fits.</summary>
/// <returns>0.</returns>
/// <remarks>MCX.EXE @ 0x00648a90</remarks>
int32_t longToText(char* result, int32_t num, uint32_t bufLen);

/// <summary>
/// Looks an address up in a linker map file (<paramref name="mapFile"/>) and copies the symbol line that contains it
/// into <paramref name="result"/>.
/// </summary>
/// <returns>The length of the line found, or 0.</returns>
/// <remarks>MCX.EXE @ 0x00648b00</remarks>
int32_t getStringFromMap(File& mapFile, uint32_t addr, char* result);

/// <summary>The heap everything that isn't in a more specific heap allocates from (SYSTEM.CFG systemHeapSize).</summary>
extern UserHeap* systemHeap;
/// <summary>The GUI's heap (SYSTEM.CFG guiHeapSize).</summary>
extern UserHeap* guiHeap;
/// <summary>The heaps registered for the heap report.</summary>
extern HeapList* globalHeapList;
/// <summary>Called when a heap fails; <c>Fatal</c> by default.</summary>
extern void (*ErrorHandler)(int32_t errorCode, const char* errorMessage, const char* errorMessage2);
