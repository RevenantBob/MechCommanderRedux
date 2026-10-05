#include "stdafx.h"
#include "sprite/sprtmgr.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/packet.h"
#include "lib/routines.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/mission.h"
#include "platform/MCRenderer.h"
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

    /// <summary>Makes a part table (25 null entries) and opens its part PAK into entry 0.</summary>
    auto openPartFile(std::vector<PacketFile*>& table, const char* name) -> int32_t
    {
        table.assign(NUM_MECH_PART_FILES + 1, nullptr);
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
        PacketFile** files = manager->spriteFiles.data();

        if (files[appearanceNum] != nullptr)
        {
            return files[appearanceNum];
        }

        PacketFile** files90 = manager->spriteFiles90.data();

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
                manager->freeDataRAM(shape);
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
    auto destroyPartTable(std::vector<PacketFile*>& table, bool checkParent) -> void
    {
        if (table.empty())
        {
            return;
        }

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
        table.clear();
    }

    /// <summary>Closes entry 0 of an appearance table, deletes every entry and frees the table.</summary>
    auto destroyAppearanceTable(std::vector<PacketFile*>& table, int32_t numAppearances) -> void
    {
        if (table.empty())
        {
            return;
        }

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
        table.clear();
    }
}

auto SpriteManager::init(char* spriteFileName) -> int32_t
{
    constexpr int32_t NO_RAM = static_cast<int32_t>(0xccdd0001);

    // Count the appearances in the preferred PAK ("<name>90.pak", or "<name>.pak" in the demo).
    const char* preferredExt = (InDemo == 0) ? "90.pak" : ".pak";
    auto* probe = new PacketFile;

    if (probe == nullptr)
    {
        return NO_RAM;
    }

    FullPathFileName fileName;
    fileName.init(spritePath, spriteFileName, preferredExt);
    int32_t result = probe->open(fileName, READ, 50);

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

    spriteFiles.assign(static_cast<size_t>(numFiles + 1), nullptr);
    spriteFiles90.assign(static_cast<size_t>(numFiles + 1), nullptr);

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

    // The 90-pixel tables exist (empty) even when their PAKs aren't opened.
    legFiles90.assign(NUM_MECH_PART_FILES + 1, nullptr);
    torsoFiles90.assign(NUM_MECH_PART_FILES + 1, nullptr);
    rArmFiles90.assign(NUM_MECH_PART_FILES + 1, nullptr);
    lArmFiles90.assign(NUM_MECH_PART_FILES + 1, nullptr);

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

    // What is still allocated (the shapes in the cache, the types' data) goes with the manager, as it went with the
    // original's heaps.
    for (const auto& [block, data] : shapeBlocks)
    {
        MCRenderer::UnregisterData(block);
    }

    shapeBlocks.clear();
    dataBlocks.clear();
    firstShape = nullptr;
    lastShape = nullptr;
}

auto SpriteManager::mallocShapeRAM(uint32_t size) -> void*
{
    if (size == 0)
    {
        return nullptr;
    }

    auto data = std::make_unique<uint8_t[]>(size);
    uint8_t* block = data.get();
    shapeBlocks.emplace(block, std::move(data));
    // Port: the block holds shapes a renderer may keep (a GPU atlas); freeShapeRAM unregisters it.
    MCRenderer::RegisterData(block, size, MCDataKind::Shapes);
    dumpedRecent = 0;
    return block;
}

auto SpriteManager::freeShapeRAM(void* block) -> void
{
    // As the original's heap: a block that isn't one of the manager's is ignored.
    if (const auto found = shapeBlocks.find(block); found != shapeBlocks.end())
    {
        MCRenderer::UnregisterData(block);
        shapeBlocks.erase(found);
    }
}

auto SpriteManager::mallocDataRAM(uint32_t size) -> void*
{
    if (size == 0)
    {
        return nullptr;
    }

    auto data = std::make_unique<uint8_t[]>(size);
    void* block = data.get();
    dataBlocks.emplace(block, std::move(data));
    return block;
}

auto SpriteManager::freeDataRAM(void* block) -> void
{
    dataBlocks.erase(block);
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
            table = use90 ? legFiles90.data() : legFiles.data();
            break;
        case 1:
            table = use90 ? torsoFiles90.data() : torsoFiles.data();
            break;
        case 2:
            table = use90 ? rArmFiles90.data() : rArmFiles.data();
            break;
        case 3:
            table = use90 ? lArmFiles90.data() : lArmFiles.data();
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
            table = legFiles.data();
            break;
        case 1:
            table = torsoFiles.data();
            break;
        case 2:
            table = rArmFiles.data();
            break;
        case 3:
            table = lArmFiles.data();
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
    std::vector<uint8_t> data(size);
    file->readPacket(static_cast<int32_t>(packetNum), data.data());
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
