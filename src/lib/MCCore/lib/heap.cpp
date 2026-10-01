#include "stdafx.h"
#include "lib/heap.h"
#include "lib/aerror.h"
#include "lib/file.h"

UserHeap* systemHeap = nullptr;
UserHeap* guiHeap = nullptr;
HeapList* globalHeapList = nullptr;
void (*ErrorHandler)(int32_t, const char*, const char*) = Fatal;

MCSystemInfo* HeapManager::systemInfo = nullptr;

namespace
{
    // Win32 memory constants the dump text is keyed on.
    constexpr uint32_t MEM_COMMIT = 0x1000;
    constexpr uint32_t MEM_RESERVE = 0x2000;
    constexpr uint32_t MEM_FREE = 0x10000;
    constexpr uint32_t MEM_PRIVATE = 0x20000;
    constexpr uint32_t MEM_MAPPED = 0x40000;
    constexpr uint32_t MEM_IMAGE = 0x1000000;
    constexpr uint32_t PAGE_READWRITE = 0x04;
    constexpr uint32_t PAGE_GUARD = 0x100;
    constexpr uint32_t PAGE_NOCACHE = 0x200;

    /// <summary>The size of the block the original's UserHeap::malloc carves for a request.</summary>
    uint32_t OriginalBlockSize(uint32_t memSize)
    {
        uint32_t blockSize = (memSize + 0xb) & 0xfffffffc;

        if (blockSize < 0x10)
        {
            blockSize = 0x10;
        }

        return blockSize;
    }
}

//---------------------------------------------------------------------------
// HeapManager

HeapManager::HeapManager()
{
    init();
}

HeapManager::~HeapManager()
{
    destroy();
}

void HeapManager::destroy()
{
    // The original decommitted then released the reservation.
    if (totalSize != 0 && memReserved != 0 && heap != nullptr)
    {
        delete[] heap;
    }

    init();
}

void HeapManager::init()
{
    heap = nullptr;
    memReserved = 0;
    totalSize = 0;
    committedSize = 0;
    nxt = nullptr;
}

HeapManager::operator uint8_t*()
{
    return getHeapPtr();
}

uint8_t* HeapManager::getHeapPtr()
{
    if (memReserved == 0 || totalSize == 0 || committedSize == 0 || heap == nullptr)
    {
        return nullptr;
    }

    return heap;
}

int32_t HeapManager::createHeap(uint32_t memSize)
{
    if (systemInfo == nullptr)
    {
        systemInfo = new MCSystemInfo;
    }

    // VirtualAlloc(MEM_RESERVE). Committed pages read as zero, so the port's block is zeroed. Port fix: the block
    // is rounded up to whole pages, as VirtualAlloc's was. The game runs a little past the end of some of its heaps
    // (ByteFlag::setFlag takes column == columns on the last row), harmlessly in the page's slack in the original.
    const uint32_t pageSize = systemInfo->dwPageSize != 0 ? systemInfo->dwPageSize : 4096;
    const uint32_t reserveSize = (std::max<uint32_t>(memSize, 1) + pageSize - 1) / pageSize * pageSize;
    heap = new (std::nothrow) uint8_t[reserveSize]();

    if (heap == nullptr)
    {
        return RESERVE_FAILED;
    }

    memReserved = 1;
    totalSize = memSize;
    return 0;
}

int32_t HeapManager::commitHeap(uint32_t commitSize)
{
    if (commitSize == 0)
    {
        commitSize = totalSize;
    }

    if (totalSize < commitSize)
    {
        return COMMIT_TOO_LARGE;
    }

    const uint32_t memLeft = totalSize - committedSize;

    if (memLeft == 0)
    {
        return HEAP_ALL_COMMITTED;
    }

    if (memLeft < commitSize)
    {
        commitSize = memLeft;
    }

    if (heap == nullptr)
    {
        char msg[216];
        std::snprintf(msg, sizeof(msg), " Could Not Commit %u bytes in Heap of size %u ", commitSize, totalSize);
        ErrorHandler(0, msg, nullptr);
        return COMMIT_FAILED;
    }

    // Original behaviour: the committed size grows by whole pages plus one, even when commitSize is page-aligned.
    committedSize += (commitSize / systemInfo->dwPageSize + 1) * systemInfo->dwPageSize;
    whoMadeMe = 0;
    return 0;
}

int32_t HeapManager::decommitHeap(uint32_t decommitSize)
{
    if (decommitSize == 0)
    {
        decommitSize = totalSize;
    }

    if (committedSize < decommitSize)
    {
        decommitSize = committedSize;
    }

    committedSize -= (decommitSize / systemInfo->dwPageSize) * systemInfo->dwPageSize;
    // Original behaviour: VirtualFree was passed the committed size as the address, so nothing was decommitted.
    return 0;
}

int HeapManager::VMQuery(void* address, VMQUERY* query)
{
    *query = VMQUERY{};
    const uint8_t* at = static_cast<const uint8_t*>(address);

    if (heap == nullptr || at < heap || at >= heap + totalSize)
    {
        return 0;
    }

    // The block is one region of one committed read/write block.
    query->pvRgnBaseAddress = heap;
    query->dwRgnProtection = PAGE_READWRITE;
    query->dwRgnSize = totalSize;
    query->dwRgnStorage = MEM_PRIVATE;
    query->dwRgnBlocks = 1;
    query->dwRgnGuardBlks = 0;
    query->fRgnIsAStack = 0;
    query->pvBlkBaseAddress = heap;
    query->dwBlkProtection = PAGE_READWRITE;
    query->dwBlkSize = totalSize;
    query->dwBlkStorage = MEM_COMMIT;
    return 1;
}

const char* HeapManager::GetMemStorageText(uint32_t memStorage)
{
    switch (memStorage)
    {
        case MEM_FREE:
            return "Free   ";
        case MEM_RESERVE:
            return "Reserve";
        case MEM_IMAGE:
            return "Image  ";
        case MEM_MAPPED:
            return "Mapped ";
        case MEM_PRIVATE:
            return "Private";
        default:
            return "Unknown";
    }
}

char* HeapManager::GetProtectText(uint32_t protect, char* buffer, int showFlags)
{
    const char* text = "Unknown";

    switch (protect & ~(PAGE_GUARD | PAGE_NOCACHE))
    {
        case 0x01:
            text = "----";
            break;
        case 0x02:
            text = "-R--";
            break;
        case 0x04:
            text = "-RW-";
            break;
        case 0x08:
            text = "-RWC";
            break;
        case 0x10:
            text = "E---";
            break;
        case 0x20:
            text = "ER--";
            break;
        case 0x40:
            text = "ERW-";
            break;
        case 0x80:
            text = "ERWC";
            break;
    }

    std::strcpy(buffer, text);

    if (showFlags)
    {
        std::strcat(buffer, " ");
        std::strcat(buffer, (protect & PAGE_GUARD) ? "G" : "-");
        std::strcat(buffer, (protect & PAGE_NOCACHE) ? "N" : "-");
    }

    return buffer;
}

void HeapManager::ConstructRgnInfoLine(VMQUERY* query, char* buffer, int bufferSize)
{
    std::snprintf(buffer, static_cast<size_t>(bufferSize), "%08X     %s  %10u  ",
                  static_cast<uint32_t>(reinterpret_cast<uintptr_t>(query->pvRgnBaseAddress)),
                  GetMemStorageText(query->dwRgnStorage), query->dwRgnSize);

    if (query->dwRgnStorage != MEM_FREE)
    {
        std::snprintf(std::strchr(buffer, 0), static_cast<size_t>(bufferSize) - std::strlen(buffer), "%5u  ",
                      query->dwRgnBlocks);
        GetProtectText(query->dwRgnProtection, std::strchr(buffer, 0), 0);
    }

    std::strcat(buffer, "     ");

    // The original appended the owning module's file name and marked the default process heap here.
    if (query->fRgnIsAStack)
    {
        std::strcat(buffer, "Thread Stack");
    }

    std::strcat(buffer, "\n");
}

void HeapManager::ConstructBlkInfoLine(VMQUERY* query, char* buffer, int bufferSize)
{
    std::snprintf(buffer, static_cast<size_t>(bufferSize), "   %08X  %s  %10u         ",
                  static_cast<uint32_t>(reinterpret_cast<uintptr_t>(query->pvBlkBaseAddress)),
                  GetMemStorageText(query->dwBlkStorage), query->dwBlkSize);

    if (query->dwBlkStorage != MEM_FREE)
    {
        GetProtectText(query->dwBlkProtection, std::strchr(buffer, 0), 1);
    }

    std::strcat(buffer, "\n");
}

void HeapManager::MemoryDump()
{
    std::FILE* dump = std::fopen("memdump.txt", "w+");

    if (dump == nullptr)
    {
        return;
    }

    char line[200];
    uint8_t* address = heap;
    VMQUERY query;
    int ok = VMQuery(address, &query);

    while (ok)
    {
        ConstructRgnInfoLine(&query, line, sizeof(line));
        std::fwrite(line, 1, std::strlen(line), dump);

        for (uint32_t block = 0; ok && block < query.dwRgnBlocks; ++block)
        {
            ConstructBlkInfoLine(&query, line, sizeof(line));
            std::fwrite(line, 1, std::strlen(line), dump);
            address += query.dwBlkSize;

            if (block < query.dwRgnBlocks - 1)
            {
                ok = VMQuery(address, &query);
            }
        }

        address = static_cast<uint8_t*>(query.pvRgnBaseAddress) + query.dwRgnSize;
        ok = VMQuery(address, &query);
    }

    std::fclose(dump);
}

//---------------------------------------------------------------------------
// UserHeap

UserHeap::UserHeap() = default;

UserHeap::~UserHeap()
{
    destroy();
}

int32_t UserHeap::init(uint32_t memSize, const char* heapId)
{
    if (heapId == nullptr)
    {
        heapName = nullptr;
    }
    else
    {
        heapName = static_cast<char*>(std::malloc(std::strlen(heapId) + 1));
        std::strcpy(heapName, heapId);
    }

    // The original reserved and committed memSize bytes here and laid one free block over them. The port keeps only
    // the sizes: blocks come from the C++ heap.
    if (systemInfo == nullptr)
    {
        systemInfo = new MCSystemInfo;
    }

    memReserved = 1;
    totalSize = memSize;
    committedSize = (memSize / systemInfo->dwPageSize + 1) * systemInfo->dwPageSize;
    whoMadeMe = 0;

    // The first free block spanned the block less a 16-byte end sentinel, rounded down to 4; its payload is 8 less.
    _InitialFree = memSize >= 0x10 ? ((memSize - 0x10) & 0xfffffffc) : 0;
    _UsedBytes = 0;
    heapState = 0;
    heapSize = memSize;
    return 0;
}

void UserHeap::destroy()
{
    for (auto& [block, size] : _Blocks)
    {
        std::free(block);
    }

    _Blocks.clear();
    _UsedBytes = 0;
    _InitialFree = 0;

    HeapManager::destroy();
    heapStart = nullptr;
    heapEnd = nullptr;
    firstNearBlock = nullptr;
    heapSize = 0;

    if (heapName != nullptr)
    {
        std::free(heapName);
        heapName = nullptr;
    }

    heapState = 0;
}

uint32_t UserHeap::totalCoreLeft()
{
    if (heapState != 0)
    {
        return 0;
    }

    if (_InitialFree <= _UsedBytes + 8)
    {
        return 0;
    }

    return _InitialFree - _UsedBytes - 8;
}

uint32_t UserHeap::coreLeft()
{
    // The original answered with the biggest free block; the port's space doesn't fragment.
    return totalCoreLeft();
}

void* UserHeap::malloc(uint32_t memSize)
{
    if (memSize == 0)
    {
        return nullptr;
    }

    void* block = std::malloc(memSize);

    if (block == nullptr)
    {
        return nullptr;
    }

    const uint32_t blockSize = OriginalBlockSize(memSize);
    _Blocks.emplace(block, blockSize);
    _UsedBytes += blockSize;
    return block;
}

int32_t UserHeap::free(void* memBlock)
{
    if (memBlock == nullptr)
    {
        return 0;
    }

    const auto found = _Blocks.find(memBlock);

    // Original behaviour: a pointer outside the heap is ignored and 0 returned.
    if (found == _Blocks.end())
    {
        return 0;
    }

    const uint32_t blockSize = found->second;
    _UsedBytes -= blockSize;
    _Blocks.erase(found);
    std::free(memBlock);
    return static_cast<int32_t>(blockSize);
}

void* UserHeap::calloc(uint32_t)
{
    return nullptr;
}

void UserHeap::walkHeap(int printIt, int skipAllocated, const char*)
{
    // The original checked each block's links (setting HEAP_CHECK_FAILED and calling ErrorHandler on damage); the
    // port's blocks have no links to damage, so it only reports.
    if (heapSize == 0 || heapState != 0)
    {
        return;
    }

    if (!printIt)
    {
        return;
    }

    char line[256];

    for (const auto& [block, size] : _Blocks)
    {
        if (skipAllocated)
        {
            continue;
        }

        std::snprintf(line, sizeof(line), "%s block at DS:%08X, size = %u \n", "Allocated",
                      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(block)), size);
        SDL_Log("%s", line);
    }

    std::snprintf(line, sizeof(line), "%s block at DS:%08X, size = %u \n", "Free", 0u, totalCoreLeft() + 8);
    SDL_Log("%s", line);
}

bool UserHeap::owns(const void* memBlock) const
{
    return _Blocks.contains(const_cast<void*>(memBlock));
}

//---------------------------------------------------------------------------
// HeapList

void HeapList::addHeap(HeapManager* newHeap)
{
    for (int32_t i = 0; i < MAX_HEAPS; ++i)
    {
        if (heapRecords[i] == nullptr)
        {
            heapRecords[i] = newHeap;
            return;
        }
    }
}

void HeapList::removeHeap(HeapManager* oldHeap)
{
    for (int32_t i = 0; i < MAX_HEAPS; ++i)
    {
        if (heapRecords[i] == oldHeap)
        {
            heapRecords[i] = nullptr;
            return;
        }
    }
}

void HeapList::dumpLog()
{
    File log;
    log.create("heap.dump.log");
    // The map file of the build was never opened in MCX.EXE, so "Made in Function" lines never appear.
    File mapFile;
    const int mapFileMissing = 0;

    char line[1024];
    char mapLine[516];
    int32_t heapNumber = 1;
    uint32_t totalCommitted = 0;
    uint32_t totalFree = 0;

    for (int32_t i = 0; i < MAX_HEAPS; ++i)
    {
        HeapManager* current = heapRecords[i];

        if (current == nullptr)
        {
            std::snprintf(line, sizeof(line), "ListNo: %d  is Freed", i);
            log.writeLine(line);
        }
        else
        {
            std::snprintf(line, sizeof(line), "ListNo: %d     Heap: %d     Type: %d     Made by: %08X", i, heapNumber,
                          current->heapType(), current->owner());
            log.writeLine(line);

            if (mapFileMissing == 0 && getStringFromMap(mapFile, current->owner(), mapLine) != 0)
            {
                std::snprintf(line, sizeof(line), "Made in Function : %s", mapLine);
                log.writeLine(line);
            }

            std::snprintf(line, sizeof(line), "HeapSize: %d     HeapStart: %08X", current->tSize(),
                          static_cast<uint32_t>(reinterpret_cast<uintptr_t>(current->getHeapPtr())));
            log.writeLine(line);
            totalCommitted += current->tSize();

            if (current->heapType() == USER_HEAP)
            {
                UserHeap* userHeap = static_cast<UserHeap*>(current);
                std::snprintf(line, sizeof(line), "TotalCoreLeft: %d     CoreLeft: %d", userHeap->totalCoreLeft(),
                              userHeap->coreLeft());
                log.writeLine(line);
                const uint32_t totalLeft = userHeap->totalCoreLeft();
                const double percentFree =
                    static_cast<double>(1.0f - static_cast<float>(current->tSize() - userHeap->coreLeft()) /
                                                   static_cast<float>(current->tSize()));
                const double fragLevel =
                    totalLeft != 0
                        ? static_cast<double>(static_cast<float>(userHeap->coreLeft()) / static_cast<float>(totalLeft))
                        : 0.0;
                std::snprintf(line, sizeof(line), "Frag Level: %f       PercentFree: %f", fragLevel, percentFree);
                log.writeLine(line);
                totalFree += userHeap->coreLeft();
            }

            ++heapNumber;
        }

        std::snprintf(line, sizeof(line), "---------------------------");
        log.writeLine(line);
    }

    std::snprintf(line, sizeof(line), "Total Committed Memory: %d      Total Free in Commit: %d", totalCommitted,
                  totalFree);
    log.writeLine(line);
    std::snprintf(line, sizeof(line), "---------------------------");
    log.writeLine(line);
    log.close();
}

//---------------------------------------------------------------------------
// Map-file helpers

uint32_t textToLong(const char* num)
{
    uint32_t result = 0;
    const char* hexDigits = num + 2;
    int32_t digit = static_cast<int32_t>(std::strlen(hexDigits));
    int32_t power = 0;

    while (--digit >= 0)
    {
        const uint8_t c = static_cast<uint8_t>(std::toupper(static_cast<uint8_t>(hexDigits[digit])));
        uint32_t value;

        if (c >= 'A' && c <= 'F')
        {
            value = static_cast<uint32_t>(c - 'A' + 10);
        }
        else if (c >= '0' && c <= '9')
        {
            value = static_cast<uint32_t>(c - '0');
        }
        else
        {
            return 0;
        }

        result += value << ((power << 2) & 0x1f);
        ++power;
    }

    return result;
}

int32_t longToText(char* result, int32_t num, uint32_t bufLen)
{
    char temp[252];
    std::snprintf(temp, sizeof(temp), "%08X", static_cast<uint32_t>(num));
    const size_t length = std::strlen(temp);

    if (length < bufLen)
    {
        std::memcpy(result, temp, length);
        result[length] = 0;
    }

    return 0;
}

int32_t getStringFromMap(File& mapFile, uint32_t addr, char* result)
{
    // Port fix: the original looped forever when the map file wasn't open (as in MCX.EXE's only caller).
    if (!mapFile.isOpen())
    {
        return 0;
    }

    char addressText[12];
    longToText(addressText, static_cast<int32_t>(addr - 0x601000), 9);

    uint8_t line[512] = {};
    char previous[512] = {};
    mapFile.seek(0);
    mapFile.readLine(line, 0x1ff);

    while (std::strstr(reinterpret_cast<char*>(line), "  Address") == nullptr)
    {
        if (mapFile.eof())
        {
            return 0;
        }

        mapFile.readLine(line, 0x1ff);
    }

    mapFile.readLine(line, 0x1ff);
    mapFile.readLine(line, 0x1ff);

    // A symbol line is " 0001:00001234       ?name@@..."; the offset starts at column 6.
    const char* offsetText = reinterpret_cast<char*>(line) + 6;
    std::strncpy(previous, offsetText, 0x1fe);

    while (std::strstr(reinterpret_cast<char*>(line), "0001:") != nullptr)
    {
        if (MCPort::StrNICmp(offsetText, addressText, 8) > 0)
        {
            std::strncpy(result, previous, 0x1fe);
            return static_cast<int32_t>(std::strlen(result));
        }

        std::strncpy(previous, offsetText, 0x1fe);

        if (mapFile.eof())
        {
            break;
        }

        mapFile.readLine(line, 0x1ff);
    }

    return 0;
}
