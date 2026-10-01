#include "stdafx.h"
#include "sprite/sprtmgr.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/heap.h"
#include "lib/packet.h"
#include "lib/routines.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/mission.h"
#include "sprite/vfxshape.h"

SpriteManager* spriteManager = nullptr;
int use90PixelSprite = 0;
int gRestartRender = 0;
int dumpedRecent = 0;

namespace
{
    /// <summary>The number of per-mech entries after the part PAK in each part table.</summary>
    constexpr int32_t NUM_MECH_PART_FILES = 24;

    /// <summary>Opens the PAK <paramref name="name"/><paramref name="ext"/> from the sprite path, then the CD's.</summary>
    auto openSpriteFile(PacketFile* file, const char* name, const char* ext) -> int32_t
    {
        FullPathFileName fileName;
        fileName.init(spritePath, name, ext);
        int32_t result = file->open(fileName, READ, 50);

        if (result != 0)
        {
            FullPathFileName cdName;
            cdName.init(CDspritePath, name, ext);
            result = file->open(cdName, READ, 50);
        }

        return result;
    }

    /// <summary>
    /// Allocates a part table (25 entries from the system heap, cleared) and opens its part PAK into entry 0.
    /// </summary>
    auto openPartFile(PacketFile**& table, const char* name) -> int32_t
    {
        auto* file = new PacketFile;
        table[0] = file;

        if (file == nullptr)
        {
            return static_cast<int32_t>(0xface0004);
        }

        return openSpriteFile(file, name, ".pak");
    }

    /// <summary>
    /// The PAK of appearance <paramref name="appearanceNum"/>: the one already open, or a new child of the first
    /// sprite PAK that has a nonempty packet for it (the preferred PAK, then the other). Null when neither does or
    /// the open fails.
    /// </summary>
    auto appearanceFile(SpriteManager* manager, uint32_t appearanceNum) -> PacketFile*
    {
        PacketFile** files = manager->spriteFiles;

        if (files[appearanceNum] != nullptr)
        {
            return files[appearanceNum];
        }

        PacketFile** files90 = manager->spriteFiles90;

        if (files90[appearanceNum] != nullptr)
        {
            return files90[appearanceNum];
        }

        PacketFile* parent = files[0];
        PacketFile* parent90 = files90[0];
        const int32_t seekResult = parent->seekPacket(static_cast<int32_t>(appearanceNum));
        const int32_t seekResult90 = parent90->seekPacket(static_cast<int32_t>(appearanceNum));

        if (seekResult != 0 || seekResult90 != 0)
        {
            return nullptr;
        }

        const int32_t size = parent->getPacketSize();
        const int32_t size90 = parent90->getPacketSize();
        PacketFile** table = files;
        int32_t childSize = size;

        if (size == 0)
        {
            if (size90 == 0)
            {
                return nullptr;
            }

            table = files90;
            childSize = size90;
        }

        auto* file = new PacketFile;
        table[appearanceNum] = file;

        if (file == nullptr)
        {
            return nullptr;
        }

        if (file->open(table[0], static_cast<uint32_t>(childSize), 50) != 0)
        {
            return nullptr;
        }

        return file;
    }

    /// <summary>
    /// The PAK of mech <paramref name="mechNum"/> in part <paramref name="table"/>, opened as a child of the part PAK
    /// when first asked for. Null when the open fails (the new file stays in the table, as in the original).
    /// </summary>
    auto mechPartFile(PacketFile** table, uint32_t mechNum) -> PacketFile*
    {
        if (table[mechNum + 1] == nullptr)
        {
            auto* file = new PacketFile;
            table[mechNum + 1] = file;

            if (file == nullptr)
            {
                return nullptr;
            }

            if (table[0]->seekPacket(static_cast<int32_t>(mechNum)) != 0)
            {
                return nullptr;
            }

            const int32_t size = table[0]->getPacketSize();

            if (file->open(table[0], static_cast<uint32_t>(size), 50) != 0)
            {
                return nullptr;
            }
        }

        return table[mechNum + 1];
    }

    /// <summary>
    /// Reads the current packet (<paramref name="packetNum"/>, <paramref name="size"/> bytes) of
    /// <paramref name="file"/> into a new shape owned by <paramref name="owner"/> and appends it to the LRU list.
    /// Null (after emptying the cache and asking for the frame again) when it doesn't fit.
    /// </summary>
    auto loadShape(SpriteManager* manager, PacketFile* file, uint32_t packetNum, uint32_t size, int32_t turnUsed,
                   AppearanceType* owner, bool checkSize) -> Shape*
    {
        auto* shape = static_cast<Shape*>(manager->mallocDataRAM(sizeof(Shape)));

        if (shape == nullptr)
        {
            Fatal(-1, " No More Data Shape RAM ", nullptr);
        }

        auto* data = static_cast<uint8_t*>(manager->mallocShapeRAM(size));

        if (data == nullptr)
        {
            manager->dumpLRU(static_cast<int32_t>(size));
            data = static_cast<uint8_t*>(manager->mallocShapeRAM(size));

            if (data == nullptr)
            {
                // Faithful: the Shape record just allocated is not freed.
                manager->dumpALL();
                gRestartRender = 1;
                return nullptr;
            }
        }

        const int32_t sizeRead = file->readPacket(static_cast<int32_t>(packetNum), data);

        if (checkSize)
        {
            Assert(static_cast<uint32_t>(sizeRead) == size, static_cast<uint32_t>(sizeRead),
                   " Bad Packet in Shape file ");
        }

        shape->init(data, owner, static_cast<int32_t>(size));

        if (manager->firstShape == nullptr)
        {
            manager->lastShape = shape;
            manager->firstShape = shape;
        }
        else
        {
            manager->lastShape->next = shape;
            manager->lastShape = shape;
        }

        shape->next = nullptr;
        shape->lastTurnUsed = turnUsed;
        return shape;
    }

    /// <summary>Closes entry 0 of a part table, deletes every entry and frees the table.</summary>
    auto destroyPartTable(PacketFile**& table, bool checkParent) -> void
    {
        if (!checkParent || table[0] != nullptr)
        {
            table[0]->close();
        }

        for (int32_t i = 1; i <= NUM_MECH_PART_FILES; i++)
        {
            if (table[i] != nullptr)
            {
                delete table[i];
                table[i] = nullptr;
            }
        }

        delete table[0];
        table[0] = nullptr;
        systemHeap->free(table);
        table = nullptr;
    }

    /// <summary>Closes entry 0 of an appearance table and deletes every entry (the table itself stays).</summary>
    auto destroyAppearanceTable(PacketFile** table, int32_t numAppearances) -> void
    {
        table[0]->close();

        for (int32_t i = 1; i <= numAppearances; i++)
        {
            if (table[i] != nullptr)
            {
                delete table[i];
                table[i] = nullptr;
            }
        }

        delete table[0];
        table[0] = nullptr;
    }
}

auto SpriteManager::init(uint32_t newShapeHeapSize, uint32_t dataHeapSize, char* spriteFileName) -> int32_t
{
    constexpr int32_t NO_RAM = static_cast<int32_t>(0xccdd0001);

    dataHeapSize += 0x7d000;
    shapeHeapSize = newShapeHeapSize;
    shapeHeap = new UserHeap;

    if (shapeHeap == nullptr)
    {
        return NO_RAM;
    }

    int32_t result = shapeHeap->init(newShapeHeapSize, nullptr);

    if (result != 0)
    {
        return result;
    }

    shapeHeap->unknown2C = 0;

    dataHeap = new UserHeap;

    if (dataHeap == nullptr)
    {
        return NO_RAM;
    }

    result = dataHeap->init(dataHeapSize, nullptr);

    if (result != 0)
    {
        return result;
    }

    // Count the appearances in the preferred PAK ("<name>90.pak", or "<name>.pak" in the demo).
    const char* preferredExt = (InDemo == 0) ? "90.pak" : ".pak";
    auto* probe = new PacketFile;

    if (probe == nullptr)
    {
        return NO_RAM;
    }

    FullPathFileName fileName;
    fileName.init(spritePath, spriteFileName, preferredExt);
    result = probe->open(fileName, READ, 50);

    if (result != 0)
    {
        // Faithful: the CD retry always adds "90.pak", even in the demo.
        FullPathFileName cdName;
        cdName.init(CDspritePath, spriteFileName, "90.pak");
        result = probe->open(cdName, READ, 50);

        if (result != 0)
        {
            return result;
        }
    }

    const int32_t numFiles = probe->getNumPackets() + 1;
    numAppearances = numFiles;
    probe->close();
    delete probe;

    const uint32_t tableSize = static_cast<uint32_t>(numFiles + 1) * sizeof(PacketFile*);
    spriteFiles = static_cast<PacketFile**>(dataHeap->malloc(tableSize));

    if (spriteFiles == nullptr)
    {
        return NO_RAM;
    }

    memclear(spriteFiles, tableSize);
    spriteFiles90 = static_cast<PacketFile**>(dataHeap->malloc(tableSize));

    if (spriteFiles90 == nullptr)
    {
        return NO_RAM;
    }

    memclear(spriteFiles90, tableSize);

    spriteFiles[0] = new PacketFile;

    if (spriteFiles[0] == nullptr)
    {
        return NO_RAM;
    }

    result = spriteFiles[0]->open(fileName, READ, numFiles);

    if (result != 0)
    {
        FullPathFileName cdName;
        cdName.init(CDspritePath, spriteFileName, "90.pak");
        result = spriteFiles[0]->open(cdName, READ, numFiles);

        if (result != 0)
        {
            return result;
        }
    }

    FullPathFileName fileName2;
    fileName2.init(spritePath, spriteFileName, ".pak");
    spriteFiles90[0] = new PacketFile;

    if (spriteFiles90[0] == nullptr)
    {
        return NO_RAM;
    }

    result = spriteFiles90[0]->open(fileName2, READ, numFiles);

    if (result != 0)
    {
        FullPathFileName cdName;
        cdName.init(CDspritePath, spriteFileName, ".pak");
        result = spriteFiles90[0]->open(cdName, READ, numFiles);

        if (result != 0)
        {
            return result;
        }
    }

    return initMechPacketFiles();
}

auto SpriteManager::initMechPacketFiles() -> int32_t
{
    constexpr int32_t NO_RAM = static_cast<int32_t>(0xface0004);
    constexpr uint32_t TABLE_SIZE = (NUM_MECH_PART_FILES + 1) * sizeof(PacketFile*);

    legFiles = static_cast<PacketFile**>(systemHeap->malloc(TABLE_SIZE));

    if (legFiles == nullptr)
    {
        return NO_RAM;
    }

    torsoFiles = static_cast<PacketFile**>(systemHeap->malloc(TABLE_SIZE));

    if (torsoFiles == nullptr)
    {
        return NO_RAM;
    }

    rArmFiles = static_cast<PacketFile**>(systemHeap->malloc(TABLE_SIZE));

    if (rArmFiles == nullptr)
    {
        return NO_RAM;
    }

    lArmFiles = static_cast<PacketFile**>(systemHeap->malloc(TABLE_SIZE));

    if (lArmFiles == nullptr)
    {
        return NO_RAM;
    }

    memclear(legFiles, TABLE_SIZE);
    memclear(torsoFiles, TABLE_SIZE);
    memclear(rArmFiles, TABLE_SIZE);
    memclear(lArmFiles, TABLE_SIZE);

    int32_t result = openPartFile(legFiles, "legs");

    if (result == 0)
    {
        result = openPartFile(torsoFiles, "torsos");
    }

    if (result == 0)
    {
        result = openPartFile(rArmFiles, "rArms");
    }

    if (result == 0)
    {
        result = openPartFile(lArmFiles, "lArms");
    }

    if (result != 0)
    {
        return result;
    }

    legFiles90 = static_cast<PacketFile**>(systemHeap->malloc(TABLE_SIZE));
    torsoFiles90 = static_cast<PacketFile**>(systemHeap->malloc(TABLE_SIZE));

    if (torsoFiles90 == nullptr)
    {
        return NO_RAM;
    }

    rArmFiles90 = static_cast<PacketFile**>(systemHeap->malloc(TABLE_SIZE));
    lArmFiles90 = static_cast<PacketFile**>(systemHeap->malloc(TABLE_SIZE));
    memclear(legFiles90, TABLE_SIZE);
    memclear(torsoFiles90, TABLE_SIZE);
    memclear(rArmFiles90, TABLE_SIZE);
    memclear(lArmFiles90, TABLE_SIZE);

    if (use90PixelSprite == 0)
    {
        return 0;
    }

    result = openPartFile(legFiles90, "legs90");

    if (result == 0)
    {
        result = openPartFile(torsoFiles90, "torsos90");
    }

    if (result == 0)
    {
        result = openPartFile(rArmFiles90, "rArms90");
    }

    if (result == 0)
    {
        result = openPartFile(lArmFiles90, "lArms90");
    }

    return result;
}

auto SpriteManager::destroy() -> void
{
    destroyAppearanceTable(spriteFiles, numAppearances);
    destroyAppearanceTable(spriteFiles90, numAppearances);

    destroyPartTable(torsoFiles, false);
    destroyPartTable(legFiles, false);
    destroyPartTable(lArmFiles, false);
    destroyPartTable(rArmFiles, false);
    // The 90-pixel part PAKs are only open when use90PixelSprite was set.
    destroyPartTable(torsoFiles90, true);
    destroyPartTable(legFiles90, true);
    destroyPartTable(lArmFiles90, true);
    destroyPartTable(rArmFiles90, true);

    // Faithful: spriteFiles90's table is not freed (it goes with the heap).
    dataHeap->free(spriteFiles);
    spriteFiles = nullptr;
    delete dataHeap;
    dataHeap = nullptr;
    delete shapeHeap;
    shapeHeap = nullptr;
}

auto SpriteManager::mallocShapeRAM(uint32_t size) -> void*
{
    void* block = shapeHeap->malloc(size);
    dumpedRecent = 0;
    return block;
}

auto SpriteManager::freeShapeRAM(void* block) -> void
{
    shapeHeap->free(block);
}

auto SpriteManager::walkShapeHeap() -> void
{
    spriteManager->shapeHeap->walkHeap(0, 0, nullptr);
}

auto SpriteManager::walkDataHeap() -> void
{
    spriteManager->dataHeap->walkHeap(0, 0, nullptr);
}

auto SpriteManager::mallocDataRAM(uint32_t size) -> void*
{
    return dataHeap->malloc(size);
}

auto SpriteManager::freeDataRAM(void* block) -> void
{
    dataHeap->free(block);
}

auto SpriteManager::dumpLRU(int32_t) -> void
{
    Shape* prev = nullptr;
    Shape* shape = firstShape;

    while (shape != nullptr)
    {
        if (shape->owner != nullptr && turn <= shape->lastTurnUsed + 1)
        {
            prev = shape;
            shape = shape->next;
            continue;
        }

        if (shape == firstShape)
        {
            firstShape = shape->next;
            shape->destroy();
            freeDataRAM(shape);
            shape = firstShape;
            prev = nullptr;
        }
        else if (shape == lastShape)
        {
            lastShape = prev;
            prev->next = nullptr;
            shape->destroy();
            freeDataRAM(shape);
            break;
        }
        else
        {
            prev->next = shape->next;
            shape->destroy();
            freeDataRAM(shape);
            shape = prev->next;
        }
    }

    dumpedRecent = 1;
}

auto SpriteManager::dumpALL() -> void
{
    Shape* shape = firstShape;

    while (shape != nullptr)
    {
        firstShape = shape->next;
        shape->destroy();
        freeDataRAM(shape);
        shape = firstShape;
    }

    dumpedRecent = 1;
}

auto SpriteManager::getShapeData(uint32_t appearanceNum, uint32_t packetNum, int32_t turnUsed, AppearanceType* owner,
                                 int) -> Shape*
{
    // Faithful: zoomedOut is ignored; the 90-pixel PAK is the preferred one and the other the fallback.
    PacketFile* file = appearanceFile(this, appearanceNum);

    if (file == nullptr)
    {
        return nullptr;
    }

    if (file->seekPacket(static_cast<int32_t>(packetNum)) != 0)
    {
        return nullptr;
    }

    const uint32_t size = static_cast<uint32_t>(file->getPacketSize());

    if (size == 0)
    {
        return nullptr;
    }

    return loadShape(this, file, packetNum, size, turnUsed, owner, true);
}

auto SpriteManager::getMechShapeData(uint32_t mechNum, uint32_t packetNum, int32_t part, int32_t turnUsed,
                                     AppearanceType* owner, int zoomedOut) -> Shape*
{
    PacketFile** table = nullptr;
    const bool use90 = use90PixelSprite != 0 && zoomedOut != 0;

    switch (part)
    {
        case 0:
            table = use90 ? legFiles90 : legFiles;
            break;
        case 1:
            table = use90 ? torsoFiles90 : torsoFiles;
            break;
        case 2:
            table = use90 ? rArmFiles90 : rArmFiles;
            break;
        case 3:
            table = use90 ? lArmFiles90 : lArmFiles;
            break;
        default:
            // Port fix: the original seeks a null file here.
            return nullptr;
    }

    PacketFile* file = mechPartFile(table, mechNum);

    if (file == nullptr)
    {
        return nullptr;
    }

    if (file->seekPacket(static_cast<int32_t>(packetNum)) != 0)
    {
        return nullptr;
    }

    const uint32_t size = static_cast<uint32_t>(file->getPacketSize());

    if (size == 0)
    {
        return nullptr;
    }

    return loadShape(this, file, packetNum, size, turnUsed, owner, false);
}

auto SpriteManager::touchMechShapeData(uint32_t mechNum, uint32_t packetNum, int32_t part) -> void
{
    PacketFile** table = nullptr;

    switch (part)
    {
        case 0:
            table = legFiles;
            break;
        case 1:
            table = torsoFiles;
            break;
        case 2:
            table = rArmFiles;
            break;
        case 3:
            table = lArmFiles;
            break;
        default:
            // Port fix: the original seeks a null file here.
            return;
    }

    PacketFile* file = mechPartFile(table, mechNum);

    if (file == nullptr)
    {
        return;
    }

    if (file->seekPacket(static_cast<int32_t>(packetNum)) != 0)
    {
        return;
    }

    const uint32_t size = static_cast<uint32_t>(file->getPacketSize());

    if (size == 0)
    {
        return;
    }

    // Read the packet once so the file cache holds it, then drop it.
    void* data = mallocShapeRAM(size);
    Assert(data != nullptr, size, " Preloader Crapped out.  Again. ");
    file->readPacket(static_cast<int32_t>(packetNum), static_cast<uint8_t*>(data));
    freeShapeRAM(data);
}

auto SpriteManager::getShapeSize(uint32_t appearanceNum, uint32_t packetNum) -> int32_t
{
    PacketFile* file = appearanceFile(this, appearanceNum);

    if (file == nullptr)
    {
        return 0;
    }

    if (file->seekPacket(static_cast<int32_t>(packetNum)) != 0)
    {
        return 0;
    }

    return file->getPacketSize();
}

auto SpriteManager::getNumShapes(uint32_t appearanceNum) -> int32_t
{
    PacketFile* file = appearanceFile(this, appearanceNum);

    if (file == nullptr)
    {
        return 0;
    }

    return file->getNumPackets();
}
