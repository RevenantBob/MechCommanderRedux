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

MCSpriteManager* SpriteManager = nullptr;
int Use90PixelSprite = 0;
int GRestartRender = 0;
int DumpedRecent = 0;

namespace
{
    /// <summary>The number of per-mech entries after the part PAK in each part table.</summary>
    constexpr int32_t NUM_MECH_PART_FILES = 24;

    /// <summary>Opens the PAK <paramref name="name"/><paramref name="ext"/> from the sprite path, then the CD's.</summary>
    auto OpenSpriteFile(MCPacketFile* file, const char* name, const char* ext) -> int32_t
    {
        MCFullPathFileName fileName;
        fileName.Init(SpritePath, name, ext);
        int32_t result = file->Open(fileName, READ, 50);

        if (result != 0)
        {
            MCFullPathFileName cdName;
            cdName.Init(CDspritePath, name, ext);
            result = file->Open(cdName, READ, 50);
        }

        return result;
    }

    /// <summary>Makes a part table (25 null entries) and opens its part PAK into entry 0.</summary>
    auto OpenPartFile(std::vector<MCPacketFile*>& table, const char* name) -> int32_t
    {
        table.assign(NUM_MECH_PART_FILES + 1, nullptr);
        auto* file = new MCPacketFile;
        table[0] = file;

        if (file == nullptr)
        {
            return static_cast<int32_t>(0xface0004);
        }

        return OpenSpriteFile(file, name, ".pak");
    }

    /// <summary>
    /// The PAK of appearance <paramref name="appearanceNum"/>: the one already open, or a new child of the first
    /// sprite PAK that has a nonempty packet for it (the preferred PAK, then the other). Null when neither does or
    /// the open fails.
    /// </summary>
    auto AppearanceFile(MCSpriteManager* manager, uint32_t appearanceNum) -> MCPacketFile*
    {
        MCPacketFile** files = manager->SpriteFiles.data();

        if (files[appearanceNum] != nullptr)
        {
            return files[appearanceNum];
        }

        MCPacketFile** files90 = manager->SpriteFiles90.data();

        if (files90[appearanceNum] != nullptr)
        {
            return files90[appearanceNum];
        }

        MCPacketFile* parent = files[0];
        MCPacketFile* parent90 = files90[0];
        const int32_t seekResult = parent->SeekPacket(static_cast<int32_t>(appearanceNum));
        const int32_t seekResult90 = parent90->SeekPacket(static_cast<int32_t>(appearanceNum));

        if (seekResult != 0 || seekResult90 != 0)
        {
            return nullptr;
        }

        const int32_t size = parent->GetPacketSize();
        const int32_t size90 = parent90->GetPacketSize();
        MCPacketFile** table = files;
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

        auto* file = new MCPacketFile;
        table[appearanceNum] = file;

        if (file == nullptr)
        {
            return nullptr;
        }

        if (file->Open(table[0], static_cast<uint32_t>(childSize), 50) != 0)
        {
            return nullptr;
        }

        return file;
    }

    /// <summary>
    /// The PAK of mech <paramref name="mechNum"/> in part <paramref name="table"/>, opened as a child of the part PAK
    /// when first asked for. Null when the open fails (the new file stays in the table, as in the original).
    /// </summary>
    auto MechPartFile(MCPacketFile** table, uint32_t mechNum) -> MCPacketFile*
    {
        if (table[mechNum + 1] == nullptr)
        {
            auto* file = new MCPacketFile;
            table[mechNum + 1] = file;

            if (file == nullptr)
            {
                return nullptr;
            }

            if (table[0]->SeekPacket(static_cast<int32_t>(mechNum)) != 0)
            {
                return nullptr;
            }

            const int32_t size = table[0]->GetPacketSize();

            if (file->Open(table[0], static_cast<uint32_t>(size), 50) != 0)
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
    auto LoadShape(MCSpriteManager* manager, MCPacketFile* file, uint32_t packetNum, uint32_t size, int32_t turnUsed,
                   MCAppearanceType* owner, bool checkSize) -> MCShape*
    {
        auto* shape = static_cast<MCShape*>(manager->MallocDataRam(sizeof(MCShape)));

        if (shape == nullptr)
        {
            Fatal(-1, " No More Data Shape RAM ", nullptr);
        }

        auto* data = static_cast<uint8_t*>(manager->MallocShapeRam(size));

        if (data == nullptr)
        {
            manager->DumpLru(static_cast<int32_t>(size));
            data = static_cast<uint8_t*>(manager->MallocShapeRam(size));

            if (data == nullptr)
            {
                manager->FreeDataRam(shape);
                manager->DumpAll();
                GRestartRender = 1;
                return nullptr;
            }
        }

        const int32_t sizeRead = file->ReadPacket(static_cast<int32_t>(packetNum), data);

        if (checkSize)
        {
            Assert(static_cast<uint32_t>(sizeRead) == size, static_cast<uint32_t>(sizeRead),
                   " Bad Packet in Shape file ");
        }

        shape->Init(data, owner, static_cast<int32_t>(size));

        if (manager->FirstShape == nullptr)
        {
            manager->LastShape = shape;
            manager->FirstShape = shape;
        }
        else
        {
            manager->LastShape->Next = shape;
            manager->LastShape = shape;
        }

        shape->Next = nullptr;
        shape->LastTurnUsed = turnUsed;
        return shape;
    }

    /// <summary>Closes entry 0 of a part table, deletes every entry and frees the table.</summary>
    auto DestroyPartTable(std::vector<MCPacketFile*>& table, bool checkParent) -> void
    {
        if (table.empty())
        {
            return;
        }

        if (!checkParent || table[0] != nullptr)
        {
            table[0]->Close();
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
    auto DestroyAppearanceTable(std::vector<MCPacketFile*>& table, int32_t numAppearances) -> void
    {
        if (table.empty())
        {
            return;
        }

        table[0]->Close();

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

auto MCSpriteManager::Init(char* spriteFileName) -> int32_t
{
    constexpr int32_t noRam = static_cast<int32_t>(0xccdd0001);

    // Count the appearances in the preferred PAK ("<name>90.pak", or "<name>.pak" in the demo).
    const char* preferredExt = (InDemo == 0) ? "90.pak" : ".pak";
    auto* probe = new MCPacketFile;

    if (probe == nullptr)
    {
        return noRam;
    }

    MCFullPathFileName fileName;
    fileName.Init(SpritePath, spriteFileName, preferredExt);
    int32_t result = probe->Open(fileName, READ, 50);

    if (result != 0)
    {
        // Faithful: the CD retry always adds "90.pak", even in the demo.
        MCFullPathFileName cdName;
        cdName.Init(CDspritePath, spriteFileName, "90.pak");
        result = probe->Open(cdName, READ, 50);

        if (result != 0)
        {
            return result;
        }
    }

    const int32_t numFiles = probe->GetNumPackets() + 1;
    NumAppearances = numFiles;
    probe->Close();
    delete probe;

    SpriteFiles.assign(static_cast<size_t>(numFiles + 1), nullptr);
    SpriteFiles90.assign(static_cast<size_t>(numFiles + 1), nullptr);

    SpriteFiles[0] = new MCPacketFile;

    if (SpriteFiles[0] == nullptr)
    {
        return noRam;
    }

    result = SpriteFiles[0]->Open(fileName, READ, numFiles);

    if (result != 0)
    {
        MCFullPathFileName cdName;
        cdName.Init(CDspritePath, spriteFileName, "90.pak");
        result = SpriteFiles[0]->Open(cdName, READ, numFiles);

        if (result != 0)
        {
            return result;
        }
    }

    MCFullPathFileName fileName2;
    fileName2.Init(SpritePath, spriteFileName, ".pak");
    SpriteFiles90[0] = new MCPacketFile;

    if (SpriteFiles90[0] == nullptr)
    {
        return noRam;
    }

    result = SpriteFiles90[0]->Open(fileName2, READ, numFiles);

    if (result != 0)
    {
        MCFullPathFileName cdName;
        cdName.Init(CDspritePath, spriteFileName, ".pak");
        result = SpriteFiles90[0]->Open(cdName, READ, numFiles);

        if (result != 0)
        {
            return result;
        }
    }

    return InitMechPacketFiles();
}

auto MCSpriteManager::InitMechPacketFiles() -> int32_t
{
    int32_t result = OpenPartFile(LegFiles, "legs");

    if (result == 0)
    {
        result = OpenPartFile(TorsoFiles, "torsos");
    }

    if (result == 0)
    {
        result = OpenPartFile(RArmFiles, "rArms");
    }

    if (result == 0)
    {
        result = OpenPartFile(LArmFiles, "lArms");
    }

    if (result != 0)
    {
        return result;
    }

    // The 90-pixel tables exist (empty) even when their PAKs aren't opened.
    LegFiles90.assign(NUM_MECH_PART_FILES + 1, nullptr);
    TorsoFiles90.assign(NUM_MECH_PART_FILES + 1, nullptr);
    RArmFiles90.assign(NUM_MECH_PART_FILES + 1, nullptr);
    LArmFiles90.assign(NUM_MECH_PART_FILES + 1, nullptr);

    if (Use90PixelSprite == 0)
    {
        return 0;
    }

    result = OpenPartFile(LegFiles90, "legs90");

    if (result == 0)
    {
        result = OpenPartFile(TorsoFiles90, "torsos90");
    }

    if (result == 0)
    {
        result = OpenPartFile(RArmFiles90, "rArms90");
    }

    if (result == 0)
    {
        result = OpenPartFile(LArmFiles90, "lArms90");
    }

    return result;
}

auto MCSpriteManager::Destroy() -> void
{
    DestroyAppearanceTable(SpriteFiles, NumAppearances);
    DestroyAppearanceTable(SpriteFiles90, NumAppearances);

    DestroyPartTable(TorsoFiles, false);
    DestroyPartTable(LegFiles, false);
    DestroyPartTable(LArmFiles, false);
    DestroyPartTable(RArmFiles, false);
    // The 90-pixel part PAKs are only open when use90PixelSprite was set.
    DestroyPartTable(TorsoFiles90, true);
    DestroyPartTable(LegFiles90, true);
    DestroyPartTable(LArmFiles90, true);
    DestroyPartTable(RArmFiles90, true);

    // What is still allocated (the shapes in the cache, the types' data) goes with the manager, as it went with the
    // original's heaps.
    for (const auto& [block, data] : ShapeBlocks)
    {
        MCRenderer::UnregisterData(block);
    }

    ShapeBlocks.clear();
    DataBlocks.clear();
    FirstShape = nullptr;
    LastShape = nullptr;
}

auto MCSpriteManager::MallocShapeRam(uint32_t size) -> void*
{
    if (size == 0)
    {
        return nullptr;
    }

    auto data = std::make_unique<uint8_t[]>(size);
    uint8_t* block = data.get();
    ShapeBlocks.emplace(block, std::move(data));
    // Port: the block holds shapes a renderer may keep (a GPU atlas); freeShapeRAM unregisters it.
    MCRenderer::RegisterData(block, size, MCDataKind::Shapes);
    DumpedRecent = 0;
    return block;
}

auto MCSpriteManager::FreeShapeRam(void* block) -> void
{
    // As the original's heap: a block that isn't one of the manager's is ignored.
    if (const auto found = ShapeBlocks.find(block); found != ShapeBlocks.end())
    {
        MCRenderer::UnregisterData(block);
        ShapeBlocks.erase(found);
    }
}

auto MCSpriteManager::MallocDataRam(uint32_t size) -> void*
{
    if (size == 0)
    {
        return nullptr;
    }

    auto data = std::make_unique<uint8_t[]>(size);
    void* block = data.get();
    DataBlocks.emplace(block, std::move(data));
    return block;
}

auto MCSpriteManager::FreeDataRam(void* block) -> void
{
    DataBlocks.erase(block);
}

auto MCSpriteManager::DumpLru(int32_t) -> void
{
    MCShape* prev = nullptr;
    MCShape* shape = FirstShape;

    while (shape != nullptr)
    {
        if (shape->Owner != nullptr && Turn <= shape->LastTurnUsed + 1)
        {
            prev = shape;
            shape = shape->Next;
            continue;
        }

        if (shape == FirstShape)
        {
            FirstShape = shape->Next;
            shape->Destroy();
            FreeDataRam(shape);
            shape = FirstShape;
            prev = nullptr;
        }
        else if (shape == LastShape)
        {
            LastShape = prev;
            prev->Next = nullptr;
            shape->Destroy();
            FreeDataRam(shape);
            break;
        }
        else
        {
            prev->Next = shape->Next;
            shape->Destroy();
            FreeDataRam(shape);
            shape = prev->Next;
        }
    }

    DumpedRecent = 1;
}

auto MCSpriteManager::DumpAll() -> void
{
    MCShape* shape = FirstShape;

    while (shape != nullptr)
    {
        FirstShape = shape->Next;
        shape->Destroy();
        FreeDataRam(shape);
        shape = FirstShape;
    }

    DumpedRecent = 1;
}

auto MCSpriteManager::GetShapeData(uint32_t appearanceNum, uint32_t packetNum, int32_t turnUsed,
                                   MCAppearanceType* owner, int) -> MCShape*
{
    // Faithful: zoomedOut is ignored; the 90-pixel PAK is the preferred one and the other the fallback.
    MCPacketFile* file = AppearanceFile(this, appearanceNum);

    if (file == nullptr)
    {
        return nullptr;
    }

    if (file->SeekPacket(static_cast<int32_t>(packetNum)) != 0)
    {
        return nullptr;
    }

    const uint32_t size = static_cast<uint32_t>(file->GetPacketSize());

    if (size == 0)
    {
        return nullptr;
    }

    return LoadShape(this, file, packetNum, size, turnUsed, owner, true);
}

auto MCSpriteManager::GetMechShapeData(uint32_t mechNum, uint32_t packetNum, int32_t part, int32_t turnUsed,
                                       MCAppearanceType* owner, int zoomedOut) -> MCShape*
{
    MCPacketFile** table = nullptr;
    const bool use90 = Use90PixelSprite != 0 && zoomedOut != 0;

    switch (part)
    {
        case 0:
            table = use90 ? LegFiles90.data() : LegFiles.data();
            break;
        case 1:
            table = use90 ? TorsoFiles90.data() : TorsoFiles.data();
            break;
        case 2:
            table = use90 ? RArmFiles90.data() : RArmFiles.data();
            break;
        case 3:
            table = use90 ? LArmFiles90.data() : LArmFiles.data();
            break;
        default:
            // Port fix: the original seeks a null file here.
            return nullptr;
    }

    MCPacketFile* file = MechPartFile(table, mechNum);

    if (file == nullptr)
    {
        return nullptr;
    }

    if (file->SeekPacket(static_cast<int32_t>(packetNum)) != 0)
    {
        return nullptr;
    }

    const uint32_t size = static_cast<uint32_t>(file->GetPacketSize());

    if (size == 0)
    {
        return nullptr;
    }

    return LoadShape(this, file, packetNum, size, turnUsed, owner, false);
}

auto MCSpriteManager::TouchMechShapeData(uint32_t mechNum, uint32_t packetNum, int32_t part) -> void
{
    MCPacketFile** table = nullptr;

    switch (part)
    {
        case 0:
            table = LegFiles.data();
            break;
        case 1:
            table = TorsoFiles.data();
            break;
        case 2:
            table = RArmFiles.data();
            break;
        case 3:
            table = LArmFiles.data();
            break;
        default:
            // Port fix: the original seeks a null file here.
            return;
    }

    MCPacketFile* file = MechPartFile(table, mechNum);

    if (file == nullptr)
    {
        return;
    }

    if (file->SeekPacket(static_cast<int32_t>(packetNum)) != 0)
    {
        return;
    }

    const uint32_t size = static_cast<uint32_t>(file->GetPacketSize());

    if (size == 0)
    {
        return;
    }

    // Read the packet once so the file cache holds it, then drop it.
    std::vector<uint8_t> data(size);
    file->ReadPacket(static_cast<int32_t>(packetNum), data.data());
}

auto MCSpriteManager::GetShapeSize(uint32_t appearanceNum, uint32_t packetNum) -> int32_t
{
    MCPacketFile* file = AppearanceFile(this, appearanceNum);

    if (file == nullptr)
    {
        return 0;
    }

    if (file->SeekPacket(static_cast<int32_t>(packetNum)) != 0)
    {
        return 0;
    }

    return file->GetPacketSize();
}

auto MCSpriteManager::GetNumShapes(uint32_t appearanceNum) -> int32_t
{
    MCPacketFile* file = AppearanceFile(this, appearanceNum);

    if (file == nullptr)
    {
        return 0;
    }

    return file->GetNumPackets();
}
