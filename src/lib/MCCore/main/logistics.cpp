#include "stdafx.h"
#include "main/logistics.h"
#include "platform/MCInput.h"
#include "platform/MCDisplay.h"
#include "gui/afont.h"
#include "gui/asystem.h"
#include "gui/scrlpane.h"
#include "gui/updisp.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "lib/inifile.h"
#include "lib/packet.h"
#include "lib/routines.h"
#include "linkup/dpmessage.h"
#include "linkup/dpplayer.h"
#include "linkup/linkedlist.hpp"
#include "linkup/sessionmanager.h"
#include "logistics/invblock.h"
#include "logistics/logbri.h"
#include "logistics/logdlg.h"
#include "logistics/loggen.h"
#include "logistics/logmain.h"
#include "logistics/logpur.h"
#include "logistics/logrep.h"
#include "logistics/logscrn.h"
#include "logistics/logsession.h"
#include "logistics/lport.h"
#include "logistics/misslog.h"
#include "logistics/mrblock.h"
#include "logistics/purchase.h"
#include "logistics/ticker.h"
#include "main/honorb.h"
#include "main/main.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "sound/soundsys.h"
#include "vfx/vfxfuncs.h"
#include "object/cmponent.h"
#include "object/mech.h"
#include "object/objtype.h"
#include "platform/MCRegistry.h"

/// <summary>Each mech name index's place in the logistics mech order (0x007977a4).</summary>
int32_t mechSort[24] = {23, 19, 13, 10, 0, 3, 2, 6, 9, 8, 15, 14, 18, 20, 4, 16, 1, 12, 5, 11, 21, 7, 17, 22};
uint32_t LogisticsHeapSize = 0xffffff;
char objectPakName[20] = "object2.pak";
std::type_identity_t<char[256]> holdString{};
int LogCheatActive[7] = {};
/// <summary>The six multiplayer player colours (gamesys.fit's mPlayerColors).</summary>
int32_t multiPlayerColors[6] = {};
std::type_identity_t<int32_t> LogCurCheatChar{};
std::type_identity_t<int> InDemo{};

namespace
{
    void* logAlloc(size_t size)
    {
        return globalLogPtr->logisticsHeap->malloc(static_cast<uint32_t>(size));
    }

    void logFree(void* block)
    {
        globalLogPtr->logisticsHeap->free(block);
    }

    /// <summary>A copy of <paramref name="text"/> on the logistics heap.</summary>
    char* logStrDup(const char* text)
    {
        const size_t size = std::strlen(text) + 1;
        auto* copy = static_cast<char*>(logAlloc(size));

        if (copy != nullptr)
        {
            std::memcpy(copy, text, size);
        }

        return copy;
    }

    /// <summary>
    /// A LogMech, LogVehicle or LogWarrior record on the logistics heap. Port fix: zeroed (the original's heap block
    /// held whatever was there before; the readers set what they use, but a few fields, such as a vehicle's
    /// <c>binarySize</c> or a mech's last critical slots, were left as found).
    /// </summary>
    template <class T> T* allocRecord()
    {
        void* block = logAlloc(sizeof(T));

        if (block != nullptr)
        {
            std::memset(block, 0, sizeof(T));
        }

        return static_cast<T*>(block);
    }

    /// <summary>String <paramref name="id"/> of the string table, copied onto the logistics heap.</summary>
    char* loadLogString(uint32_t id)
    {
        char text[256];
        cLoadString(thisInstance, id, text, 0xfe);
        return logStrDup(text);
    }

    const MasterComponent& component(uint8_t masterID)
    {
        return MasterComponentList[masterID];
    }

    /// <summary>
    /// The form of component <paramref name="masterID"/>. Port fix: an empty critical slot holds 0xff, one past the
    /// 255 components; the original read the form from past the end of the table there. The port gives 0.
    /// </summary>
    int32_t slotForm(uint8_t masterID)
    {
        return masterID < NumMasterComponents ? MasterComponentList[masterID].form : 0;
    }

    /// <summary>
    /// Reads <c>Desc&lt;descIndex&gt;</c>'s DescString from the object description file, as "%fc4" (the colour code)
    /// and the text, on the logistics heap; null when the file has no such block.
    /// </summary>
    char* readDescription(int32_t descIndex)
    {
        FitIniFile file;
        char text[1024];
        std::snprintf(text, sizeof(text), "%s%s", objectPath, objectDesc);
        int32_t result = file.open(text);
        Assert(result == 0, static_cast<uint32_t>(result), "Could not open description file");
        std::snprintf(text, sizeof(text), "Desc%d", descIndex);

        if (file.seekBlock(text) != 0)
        {
            return nullptr;
        }

        result = file.readIdString("DescString", text, 0x3ff);
        Assert(result == 0 || static_cast<uint32_t>(result) == 0xfada0003, static_cast<uint32_t>(result),
               "Could not read description string");
        const size_t length = std::strlen(text);
        auto* description = static_cast<char*>(logAlloc(length + 5));
        std::snprintf(description, length + 5, "%%fc4%s", text);
        description[length + 4] = 0;
        return description;
    }

    /// <summary>The file name part of <paramref name="path"/> without folder or extension (<c>_splitpath</c>'s fname).</summary>
    void splitFileName(const char* path, char* fileName, size_t size)
    {
        const char* start = path;

        for (const char* scan = path; *scan != 0; ++scan)
        {
            if (*scan == '\\' || *scan == '/' || *scan == ':')
            {
                start = scan + 1;
            }
        }

        const char* end = std::strrchr(start, '.');
        size_t length = end != nullptr ? static_cast<size_t>(end - start) : std::strlen(start);

        // Port fix: bounded to the destination (the original's fname went straight into the 12-byte field).
        if (length >= size)
        {
            length = size - 1;
        }

        std::memcpy(fileName, start, length);
        fileName[length] = 0;
    }

    //-----------------------------------------------------------------------------------------------------------
    // The saved ("binary") records. The original wrote and read LogWarrior, LogMech and LogVehicle as raw 32-bit
    // images with pointers in them; the port writes the same images field by field, pointers as 0, and reads them
    // back with the pointers null. Nothing in MCX.EXE reaches these paths (see OB-089).

    /// <summary>Writes fields into a record image.</summary>
    class ImageWriter
    {
    public:
        explicit ImageWriter(uint8_t* data) : _data(data) {}

        template <class T> void field(size_t offset, T& value) { std::memcpy(_data + offset, &value, sizeof(T)); }

        template <class T> void pointer(size_t offset, T*&)
        {
            const uint32_t zero = 0;
            std::memcpy(_data + offset, &zero, sizeof(zero));
        }

    private:
        uint8_t* _data;
    };

    /// <summary>Reads fields from a record image.</summary>
    class ImageReader
    {
    public:
        explicit ImageReader(const uint8_t* data) : _data(data) {}

        template <class T> void field(size_t offset, T& value) { std::memcpy(&value, _data + offset, sizeof(T)); }

        template <class T> void pointer(size_t, T*& value) { value = nullptr; }

    private:
        const uint8_t* _data;
    };

    constexpr size_t WarriorImageSize = 300;
    constexpr size_t MechImageSize = 600;
    constexpr size_t VehicleImageSize = 0xd0;
    constexpr size_t StatImageSize = 0x1c;

    template <class IO> void visitPart(IO& io, LogPart& part)
    {
        io.field(0x0, part.partType);
        io.field(0x4, part.profileName);
        io.pointer(0x10, part.weightClassName);
        io.pointer(0x14, part.chassisClassName);
        io.pointer(0x18, part.fileName);
        io.field(0x1c, part.nameIndex);
        io.field(0x20, part.binarySize);
        io.field(0x24, part.curTonnage);
        io.pointer(0x28, part.iconName);
        io.field(0x2c, part.status);
        io.field(0x30, part.chassis);
        io.field(0x34, part.resourcePoints);
        io.field(0x38, part.baseResourcePoints);
        io.field(0x3c, part.unknown3C);
        io.field(0x40, part.descIndex);
        io.pointer(0x44, part.description);
        io.field(0x48, part.engineTonnage);
        io.field(0x4c, part.engineRating);
        io.field(0x50, part.armorType);
        io.field(0x54, part.armorTonnage);
        io.field(0x58, part.numOther);
        io.field(0x59, part.numWeapons);
        io.field(0x5a, part.numAmmo);
        io.field(0x5b, part.unknown5B);
        io.field(0x68, part.unknown68);
        io.field(0x69, part.unknown69);
        io.field(0x6c, part.battleRating);
        io.field(0x70, part.unknown70);
        io.field(0x74, part.assigned);
        io.field(0x78, part.deployed);
        io.field(0x7c, part.required);
        io.field(0x80, part.notMineYet);
        io.field(0x84, part.localPart);
        io.field(0x88, part.commanderID);
        io.pointer(0x8c, part.inventory);
        io.pointer(0x90, part.briefingBox);
        io.field(0x94, part.dropLance);
        io.field(0x98, part.dropSlot);
    }

    template <class IO> void visitMech(IO& io, LogMech& mech)
    {
        visitPart(io, mech);
        io.pointer(0x9c, mech.extraName1);
        io.pointer(0xa0, mech.extraName2);
        io.pointer(0xa4, mech.mechName);
        io.field(0xa8, mech.usedTonnage);
        io.field(0xac, mech.freeTonnage);
        io.field(0xb0, mech.weaponTonnage);
        io.field(0xb4, mech.pilotIndex);
        io.field(0xb8, mech.unknownB8);
        io.field(0xbc, mech.nameVariant);
        io.field(0xc0, mech.sellValue);
        io.field(0xc4, mech.sortKey);
        io.field(0xc8, mech.maxRunSpeed);
        io.field(0xc9, mech.armor);
        io.field(0xdf, mech.unknownDF);
        io.field(0xe0, mech.hasCASE);
        io.field(0x100, mech.internals);
        io.field(0x110, mech.itemSlots);
        io.field(0x230, mech.hotSpotNumber);
        io.field(0x238, mech.chassisBR);
        io.field(0x23c, mech.pilotModifier);
        io.field(0x240, mech.statusValue);
        io.pointer(0x244, mech.repairBlock);
        io.pointer(0x248, mech.inventoryBlock);
        io.pointer(0x24c, mech.briefBlock);
        io.pointer(0x250, mech.networkPilot);
        io.pointer(0x254, mech.next);
    }

    template <class IO> void visitVehicle(IO& io, LogVehicle& vehicle)
    {
        visitPart(io, vehicle);
        io.field(0x9c, vehicle.crew);
        io.field(0xa5, vehicle.maxMoveSpeed);
        io.field(0xa6, vehicle.curInternalStructure);
        io.field(0xab, vehicle.maxArmorPoints);
        io.field(0xb0, vehicle.curArmorPoints);
        io.field(0xb5, vehicle.unknownB5);
        io.field(0xb8, vehicle.vehicleResourcePoints);
        io.field(0xbc, vehicle.baseVehicleResourcePoints);
        io.pointer(0xc0, vehicle.repairBlock);
        io.pointer(0xc4, vehicle.inventoryBlock);
        io.pointer(0xc8, vehicle.briefBlock);
        io.pointer(0xcc, vehicle.next);
    }

    template <class IO> void visitWarrior(IO& io, LogWarrior& warrior)
    {
        io.field(0x0, warrior.fileName);
        io.pointer(0xc, warrior.next);
        io.field(0x10, warrior.binarySize);
        io.pointer(0x14, warrior.name);
        io.field(0x18, warrior.id);
        io.pointer(0x1c, warrior.callsign);
        io.pointer(0x20, warrior.picture);
        io.pointer(0x24, warrior.pilotVideo);
        io.pointer(0x28, warrior.pilotAudio);
        io.pointer(0x2c, warrior.brain);
        io.field(0x30, warrior.paintScheme);
        io.field(0x34, warrior.rank);
        io.field(0x38, warrior.nameIndex);
        io.field(0x3c, warrior.descIndex);
        io.pointer(0x40, warrior.description);
        io.field(0x44, warrior.unknown44);
        io.field(0x48, warrior.personality);
        io.field(0x4c, warrior.skills);
        io.field(0x50, warrior.originalSkills);
        io.field(0x54, warrior.startingSkills);
        io.field(0x58, warrior.skillPoints);
        io.field(0x68, warrior.mechClass);
        io.field(0x69, warrior.mechType);
        io.field(0x6a, warrior.weaponClass);
        io.field(0x6b, warrior.weaponTypes);
        io.field(0x6d, warrior.unknown6D);
        io.field(0x70, warrior.wounds);
        io.field(0x74, warrior.health);
        io.field(0x78, warrior.warriorStatus);
        io.field(0x7c, warrior.unknown7C);
        io.field(0x80, warrior.dropLance);
        io.field(0x84, warrior.dropSlot);
        io.field(0x88, warrior.unknown88);
        io.field(0x8c, warrior.assigned);
        io.field(0x90, warrior.deployed);
        io.field(0x94, warrior.sold);
        io.field(0x98, warrior.notMineYet);
        io.field(0x9c, warrior.ejected);
        io.field(0xa0, warrior.unknownA0);
        io.pointer(0x128, warrior.inventoryBlock);
    }

    template <class IO> void visitStat(IO& io, _LogInventoryStat& stat)
    {
        io.field(0x0, stat.statID);
        io.field(0x1, stat.hits);
        io.field(0x4, stat.unknown04);
        io.field(0x8, stat.unknown08);
        io.field(0xc, stat.facing);
        io.field(0xe, stat.unknown0E);
        io.field(0x10, stat.amount);
        io.field(0x12, stat.location);
        io.field(0x14, stat.itemNum);
        io.pointer(0x18, stat.next);
    }

    /// <summary>Copies <paramref name="text"/> with its terminator to <paramref name="data"/>; returns the end.</summary>
    uint8_t* putString(uint8_t* data, const char* text)
    {
        const size_t size = std::strlen(text) + 1;
        std::memcpy(data, text, size);
        return data + size;
    }

    /// <summary>
    /// Reads a saved record's string (up to its terminator, at most 256 bytes) onto the logistics heap. Port fix: the
    /// original allocated the length without the terminator and copied the terminator past the block, and a string
    /// of 256 or more characters ran on past its buffer.
    /// </summary>
    char* readImageString(File* file, const char* noMemory)
    {
        char text[257];
        int32_t length = 0;

        while (length < 0x100)
        {
            const uint8_t value = file->readByte();
            text[length] = static_cast<char>(value);

            if (value == 0)
            {
                break;
            }

            ++length;
        }

        text[length] = 0;
        char* copy = logStrDup(text);
        Assert(copy != nullptr, 0, noMemory);
        return copy;
    }

    /// <summary>The size of a warrior's saved form: the record image and its six strings.</summary>
    size_t warriorDataSize(const LogWarrior* warrior)
    {
        return WarriorImageSize + std::strlen(warrior->name) + std::strlen(warrior->callsign) +
               std::strlen(warrior->picture) + std::strlen(warrior->pilotVideo) + std::strlen(warrior->pilotAudio) +
               std::strlen(warrior->brain) + 6;
    }

    //-----------------------------------------------------------------------------------------------------------

    /// <summary>
    /// The copy with stat id <paramref name="statID"/>, walking <paramref name="list"/>'s items and, of each, its
    /// first <c>count</c> copies, as the stat lookups did.
    /// </summary>
    /// <remarks>
    /// Port fix (OB-091): the original read the first item's copy list before checking the list had items (a null read on an
    /// empty inventory), and walked <c>count</c> copies even when the copy list was shorter (the count can be set
    /// apart from the copies by <see cref="InventoryList::addCountToItem"/>). The port stops at the end of either.
    /// </remarks>
    _LogInventoryStat* findStat(InventoryList* list, uint8_t statID, _LogInventoryItem** owner = nullptr)
    {
        _LogInventoryItem* item = list->items;

        for (int32_t index = 0; index < list->numItems && item != nullptr; ++index, item = item->next)
        {
            _LogInventoryStat* stat = item->stats;

            for (int32_t copy = 0; copy < item->count && stat != nullptr; ++copy, stat = stat->next)
            {
                if (stat->statID == statID)
                {
                    if (owner != nullptr)
                    {
                        *owner = item;
                    }

                    return stat;
                }
            }
        }

        return nullptr;
    }

    /// <summary>The item with master id <paramref name="masterID"/> (the list runs from the highest id down), or null.</summary>
    _LogInventoryItem* findItem(InventoryList* list, uint8_t masterID)
    {
        for (_LogInventoryItem* item = list->items; item != nullptr; item = item->next)
        {
            if (item->masterID <= masterID)
            {
                return item->masterID == masterID ? item : nullptr;
            }
        }

        return nullptr;
    }
}

//---------------------------------------------------------------------------
// InventoryList

InventoryList::InventoryList()
{
    items = nullptr;
    numItems = 0;
    nextStatID = 0;
}

auto InventoryList::operator new(size_t size) noexcept -> void*
{
    return logAlloc(size);
}

auto InventoryList::operator delete(void* ptr) -> void
{
    logFree(ptr);
}

auto InventoryList::loadDescription(int32_t index, _LogInventoryItem* item) -> void
{
    if (item == nullptr)
    {
        item = getItemInfo(index);
    }

    if (item->description != nullptr)
    {
        return;
    }

    // The component's own description block (Desc<master id>).
    char* description = readDescription(item->masterID);

    if (description != nullptr)
    {
        item->description = description;
    }
}

auto InventoryList::createStat(uint8_t itemNum, uint8_t hits, int unknown, uint8_t facing, int16_t unknown2,
                               int16_t amount, uint8_t location) -> _LogInventoryStat*
{
    auto* stat = static_cast<_LogInventoryStat*>(logAlloc(sizeof(_LogInventoryStat)));
    Assert(stat != nullptr, 0, " no RAM for Invntory stat ");
    std::memset(stat, 0, sizeof(_LogInventoryStat));
    stat->statID = nextStatID;
    stat->hits = hits;
    ++nextStatID;
    stat->unknown0E = unknown2;
    stat->unknown04 = unknown;
    stat->facing = facing;
    stat->amount = amount;
    stat->location = location;
    stat->next = nullptr;
    stat->itemNum = itemNum;
    return stat;
}

auto InventoryList::getItemInfo(int32_t index) -> _LogInventoryItem*
{
    if (index >= numItems || index < 0)
    {
        return nullptr;
    }

    _LogInventoryItem* item = items;

    for (; index > 0; --index)
    {
        item = item->next;
    }

    return item;
}

auto InventoryList::addCountToItem(int32_t count, int32_t masterID) -> void
{
    for (_LogInventoryItem* item = items; item != nullptr; item = item->next)
    {
        if (static_cast<int32_t>(item->masterID) <= masterID)
        {
            if (item->masterID == masterID)
            {
                item->count += count;

                if (item->count < 0)
                {
                    item->count = 0;
                }
            }

            return;
        }
    }
}

auto InventoryList::destroy() -> void
{
    _LogInventoryItem* item = items;

    while (item != nullptr)
    {
        if (item->description != nullptr)
        {
            logFree(item->description);
            item->description = nullptr;
        }

        if (item->purchaseBlock != nullptr)
        {
            delete item->purchaseBlock;
            item->purchaseBlock = nullptr;
        }

        if (item->inventoryBlock != nullptr)
        {
            delete item->inventoryBlock;
            item->inventoryBlock = nullptr;
        }

        _LogInventoryStat* stat = item->stats;

        while (stat != nullptr)
        {
            _LogInventoryStat* next = stat->next;
            logFree(stat);
            stat = next;
        }

        _LogInventoryItem* next = item->next;
        logFree(item);
        item = next;
    }

    items = nullptr;
    numItems = 0;
    nextStatID = 0;
}

namespace
{
    /// <summary>
    /// A new inventory item for <paramref name="masterID"/> holding <paramref name="stat"/>, with its purchase and
    /// inventory widgets unless <paramref name="widgets"/> is -1 (the part <see cref="InventoryList::addItem"/>'s two
    /// paths share; each names the item a little differently).
    /// </summary>
    _LogInventoryItem* newInventoryItem(uint8_t masterID, _LogInventoryStat* stat, int32_t widgets, size_t nameCopy)
    {
        auto* item = static_cast<_LogInventoryItem*>(logAlloc(sizeof(_LogInventoryItem)));
        std::memset(item, 0, sizeof(_LogInventoryItem));
        item->masterID = masterID;
        const MasterComponent& master = component(masterID);
        std::strncpy(item->name, master.name, nameCopy);
        item->name[0x1c] = 0;
        item->masterValue = master.masterID;
        // Ammunition counts as one item whatever the amount; anything else counts its amount.
        item->count = master.form == 10 ? 1 : stat->amount;
        item->stats = stat;
        item->rangeIndex = 0;
        item->sortOrder = globalLogPtr->componentSort[masterID];

        for (int32_t index = 0; index < globalLogPtr->numRangeSorted; ++index)
        {
            if (item->masterID == globalLogPtr->rangeSortList[index])
            {
                item->rangeIndex = index;
                break;
            }
        }

        if (widgets != -1)
        {
            item->purchaseBlock = new CompPurchaseBlock;
            item->purchaseBlock->init(item);
            item->purchaseBlock->sortOrder = item->sortOrder;
            item->inventoryBlock = new CompInventoryBlock;
            item->inventoryBlock->init(item);
            item->inventoryBlock->inventoryIndex = item->sortOrder;
        }
        else
        {
            item->purchaseBlock = nullptr;
            item->inventoryBlock = nullptr;
        }

        item->description = nullptr;
        return item;
    }
}

auto InventoryList::addItem(uint8_t masterID, _LogInventoryStat* stat, int32_t widgets) -> int32_t
{
    if (items == nullptr)
    {
        // The first item: named from 29 characters and not given its description.
        _LogInventoryItem* item = newInventoryItem(masterID, stat, widgets, 0x1d);
        item->next = nullptr;
        items = item;
        ++numItems;
        return stat->statID;
    }

    // The list runs from the highest master id down; a copy of a component already there joins its copies, sorted
    // by item number.
    _LogInventoryItem* previous = nullptr;
    _LogInventoryItem* item = items;

    do
    {
        if (item->masterID <= masterID)
        {
            if (item->masterID == masterID)
            {
                _LogInventoryStat* copy = item->stats;

                if (copy->itemNum < stat->itemNum)
                {
                    // Original behaviour: a copy numbered above the first goes in front of it; any other goes
                    // right after the first (the walk that follows never moves on), so the copies are not sorted.
                    stat->next = copy;
                    ++item->count;
                    item->stats = stat;
                    return stat->statID;
                }
                while (copy->next != nullptr && copy->itemNum < stat->itemNum)
                {
                    copy = copy->next;
                }

                stat->next = copy->next;
                copy->next = stat;
                ++item->count;
                return stat->statID;
            }
            break;
        }

        previous = item;
        item = item->next;
    } while (item != nullptr);

    _LogInventoryItem* added = newInventoryItem(masterID, stat, widgets, 0x1c);
    added->next = item;
    loadDescription(0, added);

    if (previous != nullptr)
    {
        previous->next = added;
    }
    else
    {
        items = added;
    }

    ++numItems;
    return stat->statID;
}

auto InventoryList::removeItem(uint8_t masterID, int32_t statID) -> int32_t
{
    _LogInventoryItem* previous = nullptr;
    _LogInventoryItem* item = items;

    if (item == nullptr)
    {
        return -1;
    }
    while (masterID < item->masterID)
    {
        previous = item;
        item = item->next;

        if (item == nullptr)
        {
            return -1;
        }
    }

    if (item->masterID != masterID)
    {
        return -1;
    }

    if (item->count == 1)
    {
        // The last copy (whichever statID was asked for): the item goes with it.
        logFree(item->stats);

        if (previous == nullptr)
        {
            items = item->next;
        }
        else
        {
            previous->next = item->next;
        }

        delete item->purchaseBlock;
        delete item->inventoryBlock;

        if (item->description != nullptr)
        {
            logFree(item->description);
        }

        logFree(item);
        --numItems;
        return 0;
    }

    // One copy: the one numbered statID, or the first when statID is -1.
    _LogInventoryStat* previousStat = nullptr;
    _LogInventoryStat* stat = item->stats;

    while (stat != nullptr)
    {
        if (statID < 0 || static_cast<int32_t>(stat->statID) == statID)
        {
            if (static_cast<int32_t>(stat->statID) == statID)
            {
                break;
            }

            stat = nullptr;
            break;
        }

        previousStat = stat;
        stat = stat->next;
    }

    if (stat == nullptr)
    {
        if (statID != -1)
        {
            return -1;
        }

        stat = item->stats;
        previousStat = nullptr;

        if (stat == nullptr)
        {
            return -1;
        }
    }

    if (previousStat == nullptr)
    {
        item->stats = stat->next;
    }
    else
    {
        previousStat->next = stat->next;
    }

    logFree(stat);
    --item->count;
    // Original behaviour: a copy removed this way still reports -1.
    return -1;
}

auto InventoryList::getItemStatIndex(int32_t statID) -> _LogInventoryItem*
{
    for (_LogInventoryItem* item = items; item != nullptr; item = item->next)
    {
        for (_LogInventoryStat* stat = item->stats; stat != nullptr; stat = stat->next)
        {
            if (static_cast<int32_t>(stat->statID) == statID)
            {
                return item;
            }
        }
    }

    return nullptr;
}

auto InventoryList::getItemStatID(uint8_t masterID, int32_t copy) -> int32_t
{
    _LogInventoryItem* item = findItem(this, masterID);

    if (item == nullptr || item->count <= copy)
    {
        return -1;
    }

    _LogInventoryStat* stat = item->stats;

    for (; copy > 0; --copy)
    {
        stat = stat->next;
    }

    return stat->statID;
}

auto InventoryList::getIndexFromMasterID(uint8_t masterID) -> int32_t
{
    _LogInventoryItem* item = items;

    for (int32_t index = 0; index < numItems; ++index)
    {
        if (item->masterID == masterID)
        {
            return index;
        }

        item = item->next;
    }

    return -1;
}

auto InventoryList::getMasterIDFromIndex(int32_t index) -> int32_t
{
    if (numItems <= index)
    {
        return 0xff;
    }

    _LogInventoryItem* item = items;

    for (; index > 0; --index)
    {
        item = item->next;
    }

    return item->masterID;
}

auto InventoryList::getMasterID(uint8_t statID) -> uint8_t
{
    _LogInventoryItem* item = nullptr;

    if (findStat(this, statID, &item) == nullptr)
    {
        return 0xff;
    }

    return item->masterID;
}

auto InventoryList::getFacing(uint8_t statID) -> uint8_t
{
    _LogInventoryStat* stat = findStat(this, statID);
    return stat != nullptr ? stat->facing : 0xff;
}

auto InventoryList::getAmount(uint8_t statID) -> int32_t
{
    _LogInventoryStat* stat = findStat(this, statID);
    return stat != nullptr ? stat->amount : 0xff;
}

auto InventoryList::getItemName(uint8_t masterID) -> char*
{
    // Original behaviour: the argument is used as a list position, not a master id.
    int32_t index = masterID;

    if (numItems <= index)
    {
        return nullptr;
    }

    _LogInventoryItem* item = items;

    for (; index > 0; --index)
    {
        item = item->next;
    }

    return item->name;
}

auto InventoryList::getItemCount(uint8_t masterID) -> int32_t
{
    _LogInventoryItem* item = findItem(this, masterID);
    return item != nullptr ? item->count : 0;
}

auto InventoryList::hitItem(uint8_t statID, uint8_t hits) -> int32_t
{
    _LogInventoryStat* stat = findStat(this, statID);

    if (stat == nullptr)
    {
        return -1;
    }

    stat->hits = hits;
    return 0;
}

auto InventoryList::setStatLoc(uint8_t statID, int32_t location) -> int32_t
{
    _LogInventoryStat* stat = findStat(this, statID);

    if (stat == nullptr)
    {
        return -1;
    }

    stat->location = static_cast<uint8_t>(location);
    return 0;
}

auto InventoryList::getBinaryData(void* data) -> int32_t
{
    if (data != nullptr)
    {
        if (numItems == 0)
        {
            return 4;
        }

        auto* out = static_cast<uint8_t*>(data);
        std::memcpy(out, &numItems, 4);
        out += 4;
        _LogInventoryItem* item = items;

        for (int32_t index = numItems; index != 0; --index)
        {
            *out = item->masterID;
            std::memcpy(out + 1, &item->count, 4);
            out += 5;
            _LogInventoryStat* stat = item->stats;

            for (int32_t copy = item->count; copy != 0 && stat != nullptr; --copy)
            {
                ImageWriter writer(out);
                visitStat(writer, *stat);
                out += StatImageSize;
                stat = stat->next;
            }

            item = item->next;
        }

        // Original behaviour (OB-086): the size written is not returned.
        return 4;
    }

    int32_t size = 4;

    // Port fix (OB-086): the original added the first item's copy count for every item.
    for (_LogInventoryItem* item = items; item != nullptr; item = item->next)
    {
        size += 5 + item->count * static_cast<int32_t>(StatImageSize);
    }

    return size;
}

namespace
{
    /// <summary>
    /// The four groups <see cref="InventoryList::sortRange"/> and <see cref="InventoryList::sortName"/> sort and join:
    /// the item positions of each kind (weapon type or form 7, 9, 8 and 2, in output order).
    /// </summary>
    struct SortGroups
    {
        std::vector<int32_t> group2;
        std::vector<int32_t> group7;
        std::vector<int32_t> group8;
        std::vector<int32_t> group9;
        int32_t count2 = 0;
        int32_t count7 = 0;
        int32_t count8 = 0;
        int32_t count9 = 0;

        /// <summary>
        /// Port fix: the original's four arrays (operator new, one slot per item) were uninitialised; the name sort
        /// reads past the group it fills (OB-087), so the port zeroes them.
        /// </summary>
        explicit SortGroups(int32_t size)
            : group2(static_cast<size_t>(std::max(size, 1)))
            , group7(group2.size())
            , group8(group2.size())
            , group9(group2.size())
        {
        }

        void add(int32_t kind, int32_t index)
        {
            switch (kind)
            {
                case 2:
                    group2[count2++] = index;
                    break;
                case 7:
                    group7[count7++] = index;
                    break;
                case 8:
                    group8[count8++] = index;
                    break;
                case 9:
                    group9[count9++] = index;
                    break;
                default:
                    break;
            }
        }

        /// <summary>The groups joined (7, 9, 8, 2) in a new array, or null when all are empty.</summary>
        int32_t* join() const
        {
            const int32_t total = count2 + count7 + count8 + count9;

            // Port fix (OB-087): with nothing to sort the original returned an uninitialised or freed pointer.
            if (total == 0)
            {
                return nullptr;
            }

            auto* result = new int32_t[static_cast<size_t>(total)];
            int32_t* out = result;
            out = std::copy_n(group7.data(), count7, out);
            out = std::copy_n(group9.data(), count9, out);
            out = std::copy_n(group8.data(), count8, out);
            std::copy_n(group2.data(), count2, out);
            return result;
        }
    };

    /// <summary>
    /// The sorts' swap pass: for each position <c>p</c> below <paramref name="outerBound"/> - 1, every later position
    /// <c>q</c> below <paramref name="count"/> is swapped into <c>p</c> when <paramref name="before"/> says so.
    /// </summary>
    template <class Before> void swapSort(std::vector<int32_t>& group, int32_t count, int32_t outerBound, Before before)
    {
        for (int32_t p = 0; p < outerBound - 1; ++p)
        {
            for (int32_t q = p + 1; q < count; ++q)
            {
                if (before(group[q], group[p]))
                {
                    std::swap(group[p], group[q]);
                }
            }
        }
    }
}

auto InventoryList::sortRange() -> int32_t*
{
    SortGroups groups(numItems);

    // Original behaviour (OB-087): the last item is never sorted in.
    for (int32_t index = 0; index < numItems - 1; ++index)
    {
        groups.add(component(static_cast<uint8_t>(getMasterIDFromIndex(index))).weaponType, index);
    }

    auto range = [this](int32_t index)
    { return component(static_cast<uint8_t>(getMasterIDFromIndex(index))).weaponRange[3]; };
    auto shorter = [&](int32_t a, int32_t b) { return range(a) < range(b); };

    if (groups.count7 > 1)
    {
        swapSort(groups.group7, groups.count7, groups.count7, shorter);
        // Original behaviour (OB-087): groups 9 and 8 are sorted only with group 7, and as far as its count.
        swapSort(groups.group9, groups.count9, groups.count7, shorter);
        swapSort(groups.group8, groups.count8, groups.count7, shorter);
    }

    if (groups.count2 > 1)
    {
        swapSort(groups.group2, groups.count2, groups.count2, shorter);
    }

    return groups.join();
}

auto InventoryList::sortName() -> int32_t*
{
    SortGroups groups(numItems);

    // Original behaviour (OB-087): the last item is never sorted in.
    for (int32_t index = 0; index < numItems - 1; ++index)
    {
        groups.add(component(static_cast<uint8_t>(getMasterIDFromIndex(index))).form, index);
    }

    auto name = [this](int32_t index) { return component(static_cast<uint8_t>(getMasterIDFromIndex(index))).name; };
    auto earlier = [&](int32_t a, int32_t b) { return std::strcmp(name(b), name(a)) > 0; };

    if (groups.count7 > 1)
    {
        // Original behaviour (OB-087): group 7 is ordered by the names of group 9's entries at the same positions.
        for (int32_t p = 0; p < groups.count7 - 1; ++p)
        {
            const int32_t key = groups.group9[static_cast<size_t>(p)];

            for (int32_t q = p + 1; q < groups.count7; ++q)
            {
                if (std::strcmp(name(key), name(groups.group9[static_cast<size_t>(q)])) > 0)
                {
                    std::swap(groups.group7[static_cast<size_t>(p)], groups.group7[static_cast<size_t>(q)]);
                }
            }
        }

        swapSort(groups.group9, groups.count9, groups.count7, earlier);
        swapSort(groups.group8, groups.count8, groups.count7, earlier);
    }

    if (groups.count2 > 1)
    {
        swapSort(groups.group2, groups.count2, groups.count2, earlier);
    }

    return groups.join();
}

//---------------------------------------------------------------------------
// LogWarriorList

LogWarriorList::LogWarriorList()
{
    warriors = nullptr;
    numWarriors = 0;
}

auto LogWarriorList::destroy() -> void
{
    while (numWarriors != 0)
    {
        removeWarriorAtIndex(0);
    }

    warriors = nullptr;
}

auto LogWarriorList::addWarrior(char* fileName, int sorted) -> int32_t
{
    FitIniFile file;
    FullPathFileName path;
    path.init(warriorPath, fileName, ".fit");
    const int32_t result = file.open(path);
    Assert(result == 0, static_cast<uint32_t>(result), " could not open scenario file ");
    return addWarrior(&file, sorted);
}

namespace
{
    /// <summary>
    /// Reads the parts of a pilot profile both <see cref="LogWarriorList::replaceWarrior"/> and
    /// <see cref="LogWarriorList::addWarrior(FitIniFile*, int)"/> read: the status flags from the current block, the
    /// personality, the skills (current, original, starting, points) and the rank.
    /// </summary>
    void readWarriorSkills(FitIniFile* file, LogWarrior* warrior)
    {
        if (file->readIdBoolean("Assigned", warrior->assigned) != 0)
        {
            warrior->assigned = 0;
        }

        if (file->readIdBoolean("Sold", warrior->sold) != 0)
        {
            warrior->sold = 0;
        }

        if (file->readIdBoolean("NotMineYet", warrior->notMineYet) != 0)
        {
            warrior->notMineYet = 0;
        }

        if (file->readIdBoolean("Ejected", warrior->ejected) != 0)
        {
            warrior->ejected = 0;
        }

        int32_t result = file->seekBlock("PersonalityTraits");
        Assert(result == 0, 0, " Could not find PersonalityTraits Block ");
        result = file->readIdChar("Professionalism", warrior->personality[0]);
        Assert(result == 0, static_cast<uint32_t>(result),
               " Could not find professionalism in PersonalityTraits Block ");
        result = file->readIdChar("Decorum", warrior->personality[1]);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find decorum in PersonalityTraits Block ");
        result = file->readIdChar("Aggressiveness", warrior->personality[2]);
        Assert(result == 0, static_cast<uint32_t>(result),
               " Could not find aggressiveness in PersonalityTraits Block ");
        result = file->readIdChar("Courage", warrior->personality[3]);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find courage in PersonalityTraits Block ");

        result = file->seekBlock("Skills");
        Assert(result == 0, 0, " Could not find Skills Block ");
        result = file->readIdChar("Piloting", warrior->skills[0]);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Piloting in Skills Block ");
        result = file->readIdChar("Jumping", warrior->skills[1]);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Jumping in Skills Block ");
        result = file->readIdChar("Sensors", warrior->skills[2]);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Sensors in Skills Block ");
        result = file->readIdChar("Gunnery", warrior->skills[3]);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Gunnery in Skills Block ");

        static const char* const skillNames[4] = {"Piloting", "Jumping", "Sensors", "Gunnery"};
        // The original and starting skills default to the current ones.
        const bool haveOriginal = file->seekBlock("OriginalSkills") == 0;

        for (int32_t skill = 0; skill < 4; ++skill)
        {
            if (!haveOriginal || file->readIdChar(skillNames[skill], warrior->originalSkills[skill]) != 0)
            {
                warrior->originalSkills[skill] = warrior->skills[skill];
            }
        }

        const bool haveStarting = file->seekBlock("StartingSkills") == 0;

        for (int32_t skill = 0; skill < 4; ++skill)
        {
            if (!haveStarting || file->readIdChar(skillNames[skill], warrior->startingSkills[skill]) != 0)
            {
                warrior->startingSkills[skill] = warrior->skills[skill];
            }
        }

        const bool havePoints = file->seekBlock("SkillPoints") == 0;

        for (int32_t skill = 0; skill < 4; ++skill)
        {
            if (!havePoints || file->readIdFloat(skillNames[skill], warrior->skillPoints[skill]) != 0)
            {
                warrior->skillPoints[skill] = 0.0f;
            }
        }

        warrior->calcRank();
    }

    /// <summary>Sets wounds and health from the Status block's Wounds; no health left means killed (and sold).</summary>
    void setWounds(LogWarrior* warrior, char wounds)
    {
        warrior->wounds = static_cast<float>(wounds);
        warrior->health = 6.0f - warrior->wounds;

        if (warrior->health <= 0.0f)
        {
            warrior->warriorStatus = 4;
            warrior->sold = 1;
            warrior->health = 0.0f;
        }
    }
}

auto LogWarriorList::replaceWarrior(PacketFile* file, int32_t index) -> int32_t
{
    FitIniFile profile;
    int32_t result = file->seekPacket(index);
    Assert(result == 0, 0, " Unable to find warrior file ");
    result = profile.open(file, static_cast<uint32_t>(file->getPacketSize()));
    Assert(result == 0, 0, " Unable to open warrior file ");
    result = profile.seekBlock("General");
    Assert(result == 0, static_cast<uint32_t>(result), " Bad Saved pilot file ");
    char callsign[256];
    result = profile.readIdString("Callsign", callsign, 0xff);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find CallSign in General Block ");

    // The warrior with that callsign takes the saved state.
    LogWarrior* warrior = warriors;

    while (warrior != nullptr && std::strcmp(warrior->callsign, callsign) != 0)
    {
        warrior = warrior->next;
    }

    if (warrior == nullptr)
    {
        return 5;
    }

    warrior->id = globalLogPtr->nextWarriorID++;
    readWarriorSkills(&profile, warrior);
    warrior->deployed = 0;
    result = profile.seekBlock("Status");
    Assert(result == 0, 0, " Could not find Status Block ");
    char wounds = 0;
    result = profile.readIdChar("Wounds", wounds);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find Wounds in Skills Block ");
    setWounds(warrior, wounds);
    return 0;
}

auto LogWarriorList::addWarrior(PacketFile* file, int32_t packet, int sorted) -> int32_t
{
    FitIniFile profile;
    int32_t result = file->seekPacket(packet);
    Assert(result == 0, 0, " Unable to find warrior file ");
    result = profile.open(file, static_cast<uint32_t>(file->getPacketSize()));
    Assert(result == 0, 0, " Unable to open warrior file ");
    return addWarrior(&profile, sorted);
}

auto LogWarriorList::addWarrior(FitIniFile* file, int sorted) -> int32_t
{
    auto* warrior = allocRecord<LogWarrior>();
    Assert(warrior != nullptr, 0, "Not enough memory for LogWarrior");
    warrior->unknown44 = 0;
    warrior->id = globalLogPtr->nextWarriorID++;
    warrior->nameIndex = 0;

    if (file->seekBlock("General") != 0)
    {
        // A saved pilot list: a count, then each record's image and its six strings.
        // Original behaviour (OB-089): every record is read into this one warrior, which is never added to the list
        // (nor freed). Nothing in MCX.EXE writes such a file.
        file->seek(0);
        int32_t count = file->readLong();
        std::array<uint8_t, WarriorImageSize> image{};

        while (count > 0)
        {
            file->read(image.data(), static_cast<int32_t>(image.size()));
            ImageReader reader(image.data());
            visitWarrior(reader, *warrior);
            warrior->name = readImageString(file, "Not enough memory for LogWarrior name");
            warrior->callsign = readImageString(file, "Not enough memory for LogWarrior callsign");
            warrior->picture = readImageString(file, "Not enough memory for LogWarrior photoFile");
            warrior->pilotVideo = readImageString(file, "Not enough memory for LogWarrior videoFile");
            warrior->pilotAudio = readImageString(file, "Not enough memory for LogWarrior audioFile");
            warrior->brain = readImageString(file, "Not enough memory for LogWarrior brainFile");
            --count;
        }

        return 0;
    }

    char name[256];
    char callsign[256];
    char picture[256];
    char pilotVideo[256];
    char pilotAudio[256];
    char brain[256];
    int32_t result = file->readIdString("Name", name, 0xff);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find Name in General Block ");
    result = file->readIdLong("NameIndex", warrior->nameIndex);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find NameIndex in Pilot General Block ");
    result = file->readIdString("Callsign", callsign, 0xff);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find NameIndex in Pilot General Block ");
    result = file->readIdString("Picture", picture, 0xff);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find Picture in Pilot General Block ");
    result = file->readIdString("pilotVideo", pilotVideo, 0xff);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find Pilotvideo in Pilot General Block ");
    result = file->readIdString("pilotAudio", pilotAudio, 0xff);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find Pilotaudio in Pilot General Block ");
    result = file->readIdString("Brain", brain, 0xff);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find Brain in Pilot General Block ");

    warrior->binarySize =
        static_cast<uint32_t>(std::strlen(brain) + std::strlen(pilotAudio) + std::strlen(pilotVideo) +
                              std::strlen(picture) + std::strlen(callsign) + std::strlen(name) + 6 + WarriorImageSize);
    warrior->name = logStrDup(name);
    Assert(warrior->name != nullptr, 0, "Not enough memory for LogWarrior name");
    warrior->callsign = logStrDup(callsign);
    Assert(warrior->callsign != nullptr, 0, "Not enough memory for LogWarrior callsign");
    warrior->picture = logStrDup(picture);
    Assert(warrior->picture != nullptr, 0, "Not enough memory for LogWarrior photoFile");
    warrior->pilotVideo = logStrDup(pilotVideo);
    Assert(warrior->pilotVideo != nullptr, 0, "Not enough memory for LogWarrior videoFile");
    warrior->pilotAudio = logStrDup(pilotAudio);
    Assert(warrior->pilotAudio != nullptr, 0, "Not enough memory for LogWarrior audioFile");
    warrior->brain = logStrDup(brain);
    Assert(warrior->brain != nullptr, 0, "Not enough memory for LogWarrior brainFile");

    warrior->description = nullptr;
    warrior->descIndex = -1;
    file->readIdLong("DescIndex", warrior->descIndex);
    warrior->loadDescription(warrior->descIndex);
    result = file->readIdLong("paintScheme", warrior->paintScheme);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find paintScheme in General Block ");
    readWarriorSkills(file, warrior);

    if (file->seekBlock("Affinities") == 0)
    {
        result = file->readIdChar("MechClass", warrior->mechClass);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find MechClass in Affinities Block ");
        result = file->readIdChar("MechType", warrior->mechType);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find MechType in Affinities Block ");
        result = file->readIdChar("WeaponClass", warrior->weaponClass);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find WeaponClass in Affinities Block ");
        result = file->readIdUCharArray("WeaponTypes", reinterpret_cast<uint8_t*>(warrior->weaponTypes), 2);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find WeaponTypes in Affinities Block ");
    }
    else
    {
        warrior->mechClass = 0;
        warrior->mechType = 0;
        warrior->weaponClass = 0;
        warrior->weaponTypes[0] = 0;
        warrior->weaponTypes[1] = 0;
    }

    result = file->seekBlock("Status");
    Assert(result == 0, 0, " Could not find Status Block ");
    char wounds = 0;
    result = file->readIdChar("Wounds", wounds);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find Wounds in Skills Block ");
    warrior->warriorStatus = 0;
    setWounds(warrior, wounds);
    warrior->unknown7C = 0;
    warrior->dropLance = -1;
    warrior->dropSlot = -1;
    warrior->unknown88 = 0;

    // A profile read from its own file is known by the file's base name (one read from a packet keeps none).
    if (file->getParent() == nullptr)
    {
        splitFileName(file->getFilename(), warrior->fileName, sizeof(warrior->fileName));
    }

    addWarrior(warrior, sorted);
    warrior->inventoryBlock = new PilotInventoryBlock;
    Assert(warrior->inventoryBlock != nullptr, 0, " Not enough memory for inventory block ");
    warrior->inventoryBlock->init(warrior);
    return 0;
}

auto LogWarriorList::addWarrior(LogWarrior* warrior, int sorted) -> int32_t
{
    if (sorted == 0)
    {
        warrior->next = warriors;
        warriors = warrior;
        ++numWarriors;
        return 0;
    }

    // By rank, and within a rank by callsign.
    LogWarrior* previous = nullptr;
    LogWarrior* current = warriors;

    while (current != nullptr)
    {
        if (warrior->rank <= current->rank)
        {
            while (std::strcmp(current->callsign, warrior->callsign) < 0 && current->rank == warrior->rank)
            {
                previous = current;
                current = current->next;

                if (current == nullptr)
                {
                    break;
                }
            }
            break;
        }

        previous = current;
        current = current->next;
    }

    warrior->next = current;

    if (previous != nullptr)
    {
        previous->next = warrior;
    }
    else
    {
        warriors = warrior;
    }

    ++numWarriors;
    return 0;
}

auto LogWarriorList::extractWarrior(int32_t index, LogWarrior*& warrior) -> int32_t
{
    if (numWarriors <= index)
    {
        return -1;
    }

    LogWarrior* previous = nullptr;
    LogWarrior* current = warriors;

    for (; index > 0; --index)
    {
        previous = current;
        current = current->next;
    }

    if (current == warriors)
    {
        warriors = current->next;
    }
    else
    {
        previous->next = current->next;
    }

    --numWarriors;
    warrior = current;
    return 0;
}

auto LogWarriorList::heal(int32_t amount) -> void
{
    for (LogWarrior* warrior = warriors; warrior != nullptr; warrior = warrior->next)
    {
        if (warrior->warriorStatus == 4 || warrior->sold != 0)
        {
            continue;
        }

        if (warrior->wounds < static_cast<float>(amount))
        {
            warrior->wounds = 0.0f;
            warrior->health = 6.0f;
        }
        else
        {
            warrior->wounds -= static_cast<float>(amount);
            warrior->health = 6.0f - warrior->wounds;
        }
    }
}

auto LogWarriorList::removeWarriorAtIndex(int32_t index) -> int32_t
{
    LogWarrior* previous = nullptr;
    LogWarrior* warrior = warriors;

    for (; index > 0; --index)
    {
        previous = warrior;
        warrior = warrior->next;
    }

    return deleteWarrior(warrior, previous);
}

auto LogWarriorList::removeWarrior(uint8_t index) -> int32_t
{
    // The warrior whose id is index.
    LogWarrior* previous = nullptr;
    LogWarrior* warrior = warriors;

    while (warrior != nullptr && static_cast<uint32_t>(warrior->id) != index)
    {
        previous = warrior;
        warrior = warrior->next;
    }

    return deleteWarrior(warrior, previous);
}

auto LogWarriorList::deleteWarrior(LogWarrior* warrior, LogWarrior* previous) -> int32_t
{
    if (warrior == nullptr)
    {
        return -1;
    }

    for (char** text : {&warrior->name, &warrior->callsign, &warrior->picture, &warrior->pilotVideo,
                        &warrior->pilotAudio, &warrior->brain, &warrior->description})
    {
        if (*text != nullptr)
        {
            logFree(*text);
            *text = nullptr;
        }
    }

    if (warrior->inventoryBlock != nullptr)
    {
        delete warrior->inventoryBlock;
        warrior->inventoryBlock = nullptr;
    }

    if (warrior == warriors)
    {
        warriors = warrior->next;
    }
    else if (previous != nullptr)
    {
        previous->next = warrior->next;
    }

    logFree(warrior);
    --numWarriors;
    return 0;
}

auto LogWarriorList::getWarriorCount() -> int32_t
{
    return numWarriors;
}

auto LogWarriorList::getWarriorSize(uint32_t index) -> int32_t
{
    for (LogWarrior* warrior = warriors; warrior != nullptr; warrior = warrior->next)
    {
        if (static_cast<uint32_t>(warrior->id) == index)
        {
            return static_cast<int32_t>(warrior->binarySize);
        }
    }

    return 0;
}

auto LogWarriorList::getWarriorProfile(uint32_t index, char* dest) -> int32_t
{
    for (LogWarrior* warrior = warriors; warrior != nullptr; warrior = warrior->next)
    {
        if (static_cast<uint32_t>(warrior->id) == index)
        {
            std::strcpy(dest, warrior->fileName);
            return 0;
        }
    }

    return -1;
}

auto LogWarriorList::getWarriorBrain(uint32_t index, char* dest) -> int32_t
{
    for (LogWarrior* warrior = warriors; warrior != nullptr; warrior = warrior->next)
    {
        if (static_cast<uint32_t>(warrior->id) == index)
        {
            std::strcpy(dest, warrior->brain);
            return 0;
        }
    }

    return -1;
}

auto LogWarriorList::getID(int32_t index) -> int32_t
{
    if (numWarriors <= index)
    {
        return -1;
    }

    LogWarrior* warrior = warriors;

    for (; index > 0; --index)
    {
        warrior = warrior->next;
    }

    return warrior->id;
}

auto LogWarriorList::getBinaryData(uint32_t index, void* data) -> int32_t
{
    LogWarrior* warrior = warriors;

    while (warrior != nullptr && static_cast<uint32_t>(warrior->id) != index)
    {
        warrior = warrior->next;
    }

    if (warrior == nullptr)
    {
        return -1;
    }

    // Port fix (OB-089): the original copied binarySize bytes from the record (reading on past it) and put the
    // strings after them; the port writes the record's image and then the strings.
    auto* out = static_cast<uint8_t*>(data);
    ImageWriter writer(out);
    visitWarrior(writer, *warrior);
    out += WarriorImageSize;
    out = putString(out, warrior->name);
    out = putString(out, warrior->callsign);
    out = putString(out, warrior->picture);
    out = putString(out, warrior->pilotVideo);
    out = putString(out, warrior->pilotAudio);
    putString(out, warrior->brain);
    return 0;
}

auto LogWarriorList::saveWarriorText(char* fileName, int32_t index) -> int32_t
{
    if (numWarriors <= index)
    {
        return -1;
    }

    LogWarrior* warrior = warriors;

    for (int32_t count = index; count > 0; --count)
    {
        warrior = warrior->next;
    }

    MissionLogisticsBridge bridge;
    return bridge.logisticsWarriorProfileWriter(fileName, warrior);
}

auto LogWarriorList::getWarriorInfo(int32_t index, LogWarrior*& warrior) -> int32_t
{
    warrior = nullptr;

    if (index >= numWarriors)
    {
        return -1;
    }

    LogWarrior* current = warriors;

    for (; index > 0; --index)
    {
        current = current->next;
    }

    if (current == nullptr)
    {
        return -1;
    }

    warrior = current;
    return 0;
}

auto LogWarriorList::getWarriorIndex(LogWarrior* warrior) -> int32_t
{
    int32_t index = 0;

    for (LogWarrior* current = warriors; current != nullptr; current = current->next, ++index)
    {
        if (current == warrior)
        {
            return index;
        }
    }

    return -1;
}

auto LogWarriorList::exists(char* fileName) -> int
{
    for (LogWarrior* warrior = warriors; warrior != nullptr; warrior = warrior->next)
    {
        if (std::strcmp(warrior->callsign, fileName) == 0)
        {
            return 1;
        }
    }

    return 0;
}

auto LogWarriorList::saveWarriorBinary(char* fileName, int32_t index) -> int32_t
{
    File file;
    char path[256];
    std::snprintf(path, sizeof(path), "%s%s", savePath, fileName);
    file.create(path);
    file.writeLong(numWarriors);
    // Port fix (OB-089): the original passed the address of its buffer pointer to getBinaryData (writing the record
    // over its own stack) and then wrote from the pointer that overwrote; the port writes from the buffer.
    auto writeWarrior = [&](uint32_t id)
    {
        const int32_t size = getWarriorSize(id);
        LogWarrior* warrior = warriors;

        while (warrior != nullptr && static_cast<uint32_t>(warrior->id) != id)
        {
            warrior = warrior->next;
        }

        std::vector<uint8_t> buffer(
            std::max<size_t>(static_cast<size_t>(size), warrior != nullptr ? warriorDataSize(warrior) : 0));
        getBinaryData(id, buffer.data());
        file.write(buffer.data(), size);
    };

    if (index == -1)
    {
        for (int32_t warrior = 0; warrior < numWarriors; ++warrior)
        {
            writeWarrior(static_cast<uint32_t>(getID(warrior)));
        }

        file.close();
        return numWarriors;
    }

    const auto id = static_cast<uint32_t>(getID(index));

    if (getWarriorSize(id) == 0)
    {
        return -1;
    }

    writeWarrior(id);
    file.close();
    return 0;
}

auto LogWarriorList::setDeployed(int32_t index, int deployed) -> void
{
    int32_t position = 0;

    for (LogWarrior* warrior = warriors; warrior != nullptr; warrior = warrior->next, ++position)
    {
        if (position == index)
        {
            warrior->deployed = deployed;
            return;
        }
    }
}

auto LogWarriorList::reorder() -> void
{
    // Original behaviour (OB-088): the warrior at position first - 1 is carried forward by swapping it with the one
    // after it, but when a pair is not swapped the next comparison is with a warrior two or more places on, and
    // swapping those unlinks the ones between. Nothing in MCX.EXE calls it.
    if (numWarriors > 1)
    {
        int32_t first = 1;

        do
        {
            LogWarrior* carried = nullptr;
            getWarriorInfo(first - 1, carried);
            int32_t beforeCarried = first - 2;

            for (int32_t other = first; other < numWarriors; ++other)
            {
                LogWarrior* next = nullptr;
                getWarriorInfo(other, next);

                if (carried->assigned == 0 && next->rank <= carried->rank)
                {
                    continue;
                }

                // The mechs piloted by the two swap pilots too.
                LogMech* carriedMech = nullptr;
                LogMech* otherMech = nullptr;
                LogMechList* mechs = globalLogPtr->mechList;

                for (int32_t index = 0; index < mechs->getMechCount(); ++index)
                {
                    LogMech* mech = nullptr;
                    mechs->getMechInfo(index, mech);

                    if (mech->pilotIndex == beforeCarried + 1)
                    {
                        carriedMech = mech;
                    }
                    else if (mech->pilotIndex == other)
                    {
                        otherMech = mech;
                    }

                    if (carriedMech != nullptr && otherMech != nullptr)
                    {
                        break;
                    }
                }

                if (carriedMech != nullptr)
                {
                    carriedMech->pilotIndex = other;
                }

                if (otherMech != nullptr)
                {
                    otherMech->pilotIndex = beforeCarried + 1;
                }

                carried->next = next->next;
                next->next = carried;

                if (carried == warriors)
                {
                    warriors = next;
                }
                else
                {
                    LogWarrior* previous = nullptr;
                    getWarriorInfo(beforeCarried, previous);
                    previous->next = next;
                }

                beforeCarried = other - 1;
            }

            ++first;
        } while (first - 1 < numWarriors - 1);
    }

    for (int32_t index = 0; index < numWarriors; ++index)
    {
        LogWarrior* warrior = nullptr;
        getWarriorInfo(index, warrior);
        warrior->inventoryBlock->listIndex = index;
    }
}

//---------------------------------------------------------------------------
// LogWarrior

auto LogWarrior::calcRank() -> void
{
    // Evaluated in the x87's precision, as the original did.
    double weighted = 0.0;
    double totalWeight = 0.0;

    for (int32_t skill = 0; skill < 4; ++skill)
    {
        weighted = static_cast<double>(skills[skill]) * SkillWeightings[skill] + weighted;
        totalWeight = totalWeight + SkillWeightings[skill];
    }

    for (int32_t level = 0; level < 4; ++level)
    {
        if (weighted / totalWeight < WarriorRankScale[level])
        {
            rank = level;
            return;
        }
    }
}

auto LogWarrior::loadDescription(int32_t index) -> void
{
    if (index < 0 || description != nullptr)
    {
        return;
    }

    char* text = readDescription(descIndex);

    if (text != nullptr)
    {
        description = text;
    }
}

//---------------------------------------------------------------------------
// LogMech

namespace
{
    /// <summary>The weight class of a tonnage: 0 light (below 40), 1 medium, 2 heavy (60), 3 assault (80).</summary>
    int32_t weightClass(float tonnage)
    {
        if (tonnage < 40.0f)
        {
            return 0;
        }

        if (tonnage < 60.0f)
        {
            return 1;
        }

        if (tonnage < 80.0f)
        {
            return 2;
        }

        return 3;
    }
}

auto LogMech::calcPilotModifier() -> int32_t
{
    LogWarriorList* pilots = globalLogPtr->assignedWarriorList;

    if (pilotIndex < 0 || pilotIndex >= pilots->numWarriors)
    {
        pilotModifier = 0;
        return 0;
    }

    LogWarrior* warrior = nullptr;
    pilots->getWarriorInfo(pilotIndex, warrior);
    // A pilot ranked below the mech's weight class costs 0x400 a step; a better one gives nothing.
    pilotModifier = (warrior->rank - weightClass(curTonnage)) * 0x400;

    if (pilotModifier > 0)
    {
        pilotModifier = 0;
    }

    return pilotModifier;
}

auto LogMech::calcMechCost(int repaired) -> void
{
    resourcePoints = baseResourcePoints;

    for (_LogInventoryItem* item = inventory->items; item != nullptr; item = item->next)
    {
        const MasterComponent& master = component(item->masterID);
        const int32_t form = master.form;
        // Weapons, ammunition and equipment count only when repaired; the rest always.
        const bool fitted = form == 7 || form == 8 || form == 9 || form == 10 || form == 2 || form == 0x10 ||
                            form == 0x11 || form == 0x12;

        if (repaired == 0 && fitted)
        {
            continue;
        }

        for (_LogInventoryStat* stat = item->stats; stat != nullptr; stat = stat->next)
        {
            if (stat->hits == 0)
            {
                resourcePoints += master.resourcePoints;
            }
        }
    }

    uint32_t internal = 0;

    for (const ArmorPoints& points : internals)
    {
        internal += points.curArmor;
    }

    uint32_t armorLeft = 0;

    for (const ArmorPoints& points : armor)
    {
        armorLeft += points.curArmor;
    }

    resourcePoints = static_cast<int32_t>(static_cast<uint32_t>(resourcePoints) + internal * 0x32 + armorLeft * 0x28);
}

auto LogMech::calcBR() -> int32_t
{
    // The undamaged copies' battle ratings, summed in the x87's precision, plus the chassis's, truncated.
    double rating = 0.0;

    for (_LogInventoryItem* item = inventory->items; item != nullptr; item = item->next)
    {
        for (_LogInventoryStat* stat = item->stats; stat != nullptr; stat = stat->next)
        {
            if (stat->hits == 0)
            {
                rating += component(item->masterID).battleRating;
            }
        }
    }

    battleRating = static_cast<int32_t>(static_cast<double>(chassisBR) + rating);
    return battleRating;
}

auto LogMech::placeItem(uint8_t masterID, int32_t itemNum, int32_t hits) -> void
{
    // An empty critical slot's item number.
    constexpr uint8_t emptySlot = 0xff;
    constexpr int32_t maxSlots = 12;
    // The critical slots as one run: location * maxSlots + slot.
    ItemSlot* slots = &itemSlots[0][0];
    const auto number = static_cast<uint8_t>(itemNum);
    const auto damage = static_cast<uint8_t>(hits);
    auto fill = [&](int32_t location, bool setMaster)
    {
        for (int32_t slot = 0; slot < NumLocationCriticalSpaces[location]; ++slot)
        {
            ItemSlot& entry = slots[location * maxSlots + slot];

            if (entry.row == emptySlot)
            {
                entry.row = number;
                entry.column = damage;

                if (setMaster)
                {
                    entry.masterID = masterID;
                }

                return;
            }
        }
    };

    auto holds = [&](int32_t location)
    {
        for (int32_t slot = 0; slot < NumLocationCriticalSpaces[location]; ++slot)
        {
            if (slots[location * maxSlots + slot].masterID == masterID)
            {
                return true;
            }
        }

        return false;
    };

    if (masterID < 100)
    {
        switch (component(masterID).form)
        {
            case COMPONENT_FORM_COCKPIT:
            case COMPONENT_FORM_SENSOR:
            case COMPONENT_FORM_LIFESUPPORT:
            case COMPONENT_FORM_ECM:
            case COMPONENT_FORM_PROBE:
            {
                // Head equipment (the component is not recorded).
                fill(MECH_BODY_LOCATION_HEAD, false);
                return;
            }
            case COMPONENT_FORM_ACTUATOR:
            {
                if (masterID != 4 && masterID != 0x21)
                {
                    // Leg actuators: the left leg, or the right leg when the left already has one.
                    if (holds(MECH_BODY_LOCATION_LLEG))
                    {
                        fill(MECH_BODY_LOCATION_RLEG, true);
                    }
                    else
                    {
                        fill(MECH_BODY_LOCATION_LLEG, true);
                    }

                    return;
                }

                // Arm actuators (4 and 0x21): the left arm, or the right arm when the left already has one.
                if (holds(MECH_BODY_LOCATION_LARM))
                {
                    fill(MECH_BODY_LOCATION_RARM, true);
                }
                else
                {
                    fill(MECH_BODY_LOCATION_LARM, true);
                }

                return;
            }
            case COMPONENT_FORM_ENGINE:
            case COMPONENT_FORM_GYROSCOPE:
            {
                // Centre torso (the component is not recorded).
                fill(MECH_BODY_LOCATION_CTORSO, false);
                return;
            }
            case COMPONENT_FORM_JUMPJET:
            {
                // Jump jets: the leg with fewer of them, the left on a tie.
                // OB-092 (fixed): MCX.EXE read both legs at one slot index that only moved on when the left leg's slot
                // held a jet (and took master ids 10..13 as the jets), then put this jet in every empty slot of the
                // chosen leg.
                auto countJets = [&](int32_t location)
                {
                    int32_t count = 0;

                    for (int32_t slot = 0; slot < NumLocationCriticalSpaces[location]; ++slot)
                    {
                        if (slotForm(slots[location * maxSlots + slot].masterID) == COMPONENT_FORM_JUMPJET)
                        {
                            ++count;
                        }
                    }

                    return count;
                };

                const int32_t location = countJets(MECH_BODY_LOCATION_RLEG) < countJets(MECH_BODY_LOCATION_LLEG)
                                             ? MECH_BODY_LOCATION_RLEG
                                             : MECH_BODY_LOCATION_LLEG;
                fill(location, true);
                return;
            }

            default:
                return;
        }
    }

    // Weapons: the arm or side torso with room and the fewest weapons of their size, ties going to the one searched
    // first. Small weapons search the arms first, large weapons the side torsos.
    // OB-092 (fixed): MCX.EXE started the large search from the left torso's count but with the left arm chosen, so
    // large weapons piled into the left arm, and it wrote into a full location's slot 12 (the next location's first).
    static constexpr int32_t smallWeaponOrder[4] = {MECH_BODY_LOCATION_LARM, MECH_BODY_LOCATION_RARM,
                                                    MECH_BODY_LOCATION_LTORSO, MECH_BODY_LOCATION_RTORSO};
    static constexpr int32_t largeWeaponOrder[4] = {MECH_BODY_LOCATION_LTORSO, MECH_BODY_LOCATION_RTORSO,
                                                    MECH_BODY_LOCATION_LARM, MECH_BODY_LOCATION_RARM};
    const bool large = getWeaponLarge(masterID) != 0;
    int32_t location = -1;
    int32_t fewest = 0;

    for (const int32_t candidate : large ? largeWeaponOrder : smallWeaponOrder)
    {
        bool hasRoom = false;

        for (int32_t slot = 0; slot < NumLocationCriticalSpaces[candidate]; ++slot)
        {
            if (slots[candidate * maxSlots + slot].row == emptySlot)
            {
                hasRoom = true;
                break;
            }
        }

        if (!hasRoom)
        {
            continue;
        }

        const int32_t count = large ? getLargeWeaponCount(candidate) : getSmallWeaponCount(candidate);

        if (location < 0 || count < fewest)
        {
            location = candidate;
            fewest = count;
        }
    }

    // No arm or side torso has room: the weapon gets no critical slot.
    if (location >= 0)
    {
        fill(location, true);
    }
}

auto LogMech::getWeaponLarge(uint8_t masterID) -> int32_t
{
    if ((masterID >= 100 && masterID <= 0x68) || (masterID >= 0x6e && masterID <= 0x71))
    {
        return 1;
    }

    switch (masterID)
    {
        case 0x79:
        case 0x7a:
        case 0x83:
        case 0x84:
        case 0x8d:
        case 0x8e:
        case 0x91:
        case 0x92:
        case 0x96:
        case 0x97:
        case 0x9a:
            return 1;
        default:
            return 0;
    }
}

auto LogMech::getLargeWeaponCount(int32_t location) -> int32_t
{
    int32_t count = 0;

    for (const ItemSlot& slot : itemSlots[location])
    {
        count += getWeaponLarge(slot.masterID);
    }

    return count;
}

auto LogMech::getSmallWeaponCount(int32_t location) -> int32_t
{
    int32_t count = 0;

    for (const ItemSlot& slot : itemSlots[location])
    {
        const int32_t form = slotForm(slot.masterID);

        // Ammunition counts as a small weapon.
        if ((form == COMPONENT_FORM_WEAPON_ENERGY || form == COMPONENT_FORM_WEAPON_BALLISTIC ||
             form == COMPONENT_FORM_WEAPON_MISSILE || form == COMPONENT_FORM_AMMO) &&
            getWeaponLarge(slot.masterID) == 0)
        {
            ++count;
        }
    }

    return count;
}

auto LogMech::loadDescription(int32_t index) -> void
{
    if (index < 0 || description != nullptr)
    {
        return;
    }

    char* text = readDescription(descIndex);

    if (text != nullptr)
    {
        description = text;
    }
}

//---------------------------------------------------------------------------
// LogVehicle

auto LogVehicle::calcVehicleCost() -> void
{
    vehicleResourcePoints = baseVehicleResourcePoints;

    for (_LogInventoryItem* item = inventory->items; item != nullptr; item = item->next)
    {
        vehicleResourcePoints += component(item->masterID).resourcePoints * item->count;
    }
}

auto LogVehicle::loadDescription(int32_t index) -> void
{
    if (index < 0 || description != nullptr)
    {
        return;
    }

    char* text = readDescription(descIndex);

    if (text != nullptr)
    {
        description = text;
    }
}

//---------------------------------------------------------------------------
// LogMechList

LogMechList::LogMechList()
{
    mechs = nullptr;
    numMechs = 0;
}

auto LogMechList::operator new(size_t size) noexcept -> void*
{
    return logAlloc(size);
}

auto LogMechList::operator delete(void* ptr) -> void
{
    logFree(ptr);
}

auto LogMechList::destroy() -> void
{
    while (numMechs != 0)
    {
        removeMech(static_cast<uint8_t>(0));
    }

    mechs = nullptr;
}

auto LogMechList::getMechIndex(LogMech* mech) -> int32_t
{
    LogMech* current = mechs;

    for (int32_t index = 0; index < numMechs; ++index)
    {
        if (current == mech)
        {
            return index;
        }

        current = current->next;
    }

    return -1;
}

auto LogMechList::addMech(char* fileName, int required, int sorted, int widgets) -> LogMech*
{
    FitIniFile file;
    FullPathFileName path;
    path.init(profilePath, fileName, ".fit");
    const int32_t result = file.open(path);
    Assert(result == 0, 0, "(addMech) Could not open file");
    LogMech* mech = addMech(&file, required, sorted, widgets);
    std::strncpy(mech->profileName, fileName, 9);
    file.close();
    return mech;
}

auto LogMechList::replaceMech(PacketFile* file, int32_t index) -> int32_t
{
    FitIniFile profile;
    int32_t result = file->seekPacket(index);
    Assert(result == 0, 0, " Unable to find Mech file ");
    result = profile.open(file, static_cast<uint32_t>(file->getPacketSize()));
    Assert(result == 0, 0, " Unable to open mech file ");
    result = profile.seekBlock("General");
    Assert(result == 0, static_cast<uint32_t>(result), " Bad Saved Mech file ");
    int32_t pilot = 0;
    result = profile.readIdLong("Pilot", pilot);
    Assert(result == 0, static_cast<uint32_t>(result), " No Pilot in Saved Mech file ");
    profile.close();

    // The mech flown by that pilot is replaced by the saved one.
    LogMech* previous = nullptr;
    LogMech* mech = mechs;

    while (mech != nullptr && mech->pilotIndex != pilot)
    {
        previous = mech;
        mech = mech->next;
    }

    if (mech == nullptr)
    {
        return 5;
    }

    if (mech->fileName != nullptr)
    {
        logFree(mech->fileName);
        mech->fileName = nullptr;
    }

    if (mech->iconName != nullptr)
    {
        logFree(mech->iconName);
        mech->iconName = nullptr;
    }

    if (mech->inventory != nullptr)
    {
        mech->inventory->destroy();
        delete mech->inventory;
        mech->inventory = nullptr;
    }

    // Port fix (OB-090): the original freed the mech but not its widgets, which went on pointing at it (the repair
    // block from the repair screen's list), nor its other strings. The port removes and frees them.
    if (globalLogPtr->repairScreen != nullptr && mech->repairBlock != nullptr)
    {
        globalLogPtr->repairScreen->unitPane->removeChild(mech->repairBlock);
    }

    delete mech->briefingBox;
    delete mech->repairBlock;
    delete mech->inventoryBlock;
    delete mech->briefBlock;

    for (char* text : {mech->weightClassName, mech->chassisClassName, mech->extraName1, mech->extraName2,
                       mech->description, mech->mechName})
    {
        if (text != nullptr)
        {
            logFree(text);
        }
    }

    if (previous == nullptr)
    {
        mechs = mech->next;
    }
    else
    {
        previous->next = mech->next;
    }

    logFree(mech);
    --numMechs;
    addMech(file, index);
    return 0;
}

auto LogMechList::addMech(PacketFile* file, int32_t packet) -> LogMech*
{
    FitIniFile profile;
    int32_t result = file->seekPacket(packet);
    Assert(result == 0, static_cast<uint32_t>(result), "Campaign file cannot find packet for mech. #1");
    result = profile.open(file, static_cast<uint32_t>(file->getPacketSize()));
    Assert(result == 0, static_cast<uint32_t>(result), "Campaign file cannot find packet for mech. #2");
    LogMech* mech = addMech(&profile, 0, 1, 1);
    profile.close();
    return mech;
}

namespace
{
    /// <summary>The profile block of each mech body location, in location order.</summary>
    const char* const MechLocationBlocks[8] = {"Head",    "CenterTorso", "LeftTorso", "RightTorso",
                                               "LeftArm", "RightArm",    "LeftLeg",   "RightLeg"};

    /// <summary>The profile keys of the eleven armor locations (the eight, then the three rear torso ones).</summary>
    const char* const MechArmorKeys[11] = {
        "Head",    "CenterTorso", "LeftTorso",       "RightTorso",    "LeftArm",       "RightArm",
        "LeftLeg", "RightLeg",    "RearCenterTorso", "RearLeftTorso", "RearRightTorso"};

    /// <summary>"(AddMech) could not find key in profile <paramref name="number"/>", the reader's numbered checks.</summary>
    void checkKey(int32_t result, int32_t number)
    {
        if (result == 0)
        {
            return;
        }

        char message[64];
        std::snprintf(message, sizeof(message), "(AddMech) could not find key in profile %d", number);
        Assert(0, 0, message);
    }

    /// <summary>
    /// Reads a chassis's maximum internal structure from its packet in the object packet file; any failure is fatal
    /// (0xbeef0006). A chassis with no packet keeps none.
    /// </summary>
    void readChassisInternals(LogMech* mech)
    {
        PacketFile objects;
        char path[256];
        std::snprintf(path, sizeof(path), "%s%s", objectPath, objectPakName);
        const int32_t result = objects.open(path);
        Assert(result == 0, 0, "(AddMech) could not open file 8");

        if (objects.seekPacket(static_cast<int32_t>(mech->chassis)) != 0)
        {
            return;
        }

        FitIniFile chassis;

        if (chassis.open(&objects, static_cast<uint32_t>(objects.getPacketSize())) != 0)
        {
            Fatal(static_cast<int32_t>(0xbeef0006), nullptr, nullptr);
        }

        if (chassis.seekBlock("InternalStructure") != 0)
        {
            Fatal(static_cast<int32_t>(0xbeef0006), nullptr, nullptr);
        }

        for (int32_t location = 0; location < 8; ++location)
        {
            if (chassis.readIdUChar(MechLocationBlocks[location], mech->internals[location].maxArmor) != 0)
            {
                Fatal(static_cast<int32_t>(0xbeef0006), nullptr, nullptr);
            }
        }
    }
}

auto LogMechList::addMech(FitIniFile* file, int required, int sorted, int widgets) -> LogMech*
{
    auto* mech = allocRecord<LogMech>();
    Assert(mech != nullptr, 0, "Not enough memory for LogMech");
    mech->briefBlock = nullptr;
    mech->networkPilot = nullptr;
    mech->localPart = 1;
    mech->partType = 1;

    if (file->seekBlock("General") != 0)
    {
        // A saved mech list: a count, then each record's image and its name and icon.
        // Original behaviour (OB-089): every record is read into this one mech, which is returned without being
        // added to the list (and without an inventory). Nothing in MCX.EXE writes such a file.
        file->seek(0);
        int32_t count = file->readLong();
        std::array<uint8_t, MechImageSize> image{};

        while (count > 0)
        {
            file->read(image.data(), static_cast<int32_t>(image.size()));
            ImageReader reader(image.data());
            visitMech(reader, *mech);
            mech->fileName = readImageString(file, "Not enough memory for LogMech name");
            mech->iconName = readImageString(file, "Not enough memory for LogMech icon");
            --count;
        }

        return mech;
    }

    mech->inventory = new InventoryList;
    // Original behaviour: the check is on the mech, not on the list just made.
    Assert(mech != nullptr, 0, "Not enough memory for InventoryList");
    mech->pilotIndex = -1;
    mech->unknownB8 = 0;

    int32_t result = file->seekBlock("Header");
    Assert(result == 0, 0, "(AddMech) could not find Header in profile. 0");
    char text[256] = {};
    result = file->readIdString("FileType", text, 0x14);
    const int typeRead = result == 0;
    Assert(typeRead, 0, "(AddMech) could not find key in profile 1");

    // Original behaviour: a FileType other than MechProfile only fails when it could not be read at all.
    if (std::strcmp(text, "MechProfile") != 0)
    {
        Assert(typeRead, 0, "(AddMech) could not find key in profile 2");
    }

    result = file->seekBlock("General");
    checkKey(result, 3);
    file->readIdString("Name", text, 0x7f);
    mech->mechName = logStrDup(text);
    checkKey(file->readIdFloat("CurTonnage", mech->curTonnage), 4);
    checkKey(file->readIdChar("Status", mech->status), 5);

    if (mech->status == 1 || mech->status == 2)
    {
        mech->status = 0;
    }

    if (file->readIdLong("ResourcePoints", mech->resourcePoints) != 0)
    {
        mech->resourcePoints = 100;
    }

    result = file->readIdLong("NameIndex", mech->nameIndex);
    Assert(result == 0, static_cast<uint32_t>(result), "(AddMech) could not find NameIndex");
    result = file->readIdLong("NameVariant", mech->nameVariant);
    Assert(result == 0, static_cast<uint32_t>(result), "(AddMech) could not fine NameVariant");
    mech->baseResourcePoints = mech->resourcePoints;
    mech->description = nullptr;
    mech->descIndex = -1;
    file->readIdLong("DescIndex", mech->descIndex);
    mech->loadDescription(mech->descIndex);
    mech->fileName = loadLogString(static_cast<uint32_t>(mech->descIndex + 300));
    checkKey(file->readIdString("icon", text, 0x7f), 6);
    mech->iconName = logStrDup(text);
    checkKey(file->readIdULong("Chassis", mech->chassis), 7);
    if (file->readIdLong("ChassisBR", mech->chassisBR) != 0)
    {
        mech->chassisBR = 100;
    }

    readChassisInternals(mech);

    if (file->readIdBoolean("Assigned", mech->assigned) != 0)
    {
        mech->assigned = 0;
    }

    if (file->readIdBoolean("Deployed", mech->deployed) != 0)
    {
        mech->deployed = 0;
    }

    if (file->readIdBoolean("Required", mech->required) != 0)
    {
        mech->required = 0;
    }

    if (file->readIdBoolean("NotMineYet", mech->notMineYet) != 0)
    {
        mech->notMineYet = 0;
    }

    if (file->readIdLong("Pilot", mech->pilotIndex) != 0)
    {
        mech->pilotIndex = -1;
    }

    checkKey(file->readIdString("MechType", text, 0x28), 9);

    // The sort key: the name's place in the mech order, three variants apart.
    // Original behaviour: variant 1 sorts after variant 2 (1 gets +2, 2 gets +1).
    const int32_t order = mechSort[mech->nameIndex];
    mech->sortKey = order * 3;

    if (mech->nameVariant == 1)
    {
        mech->sortKey = order * 3 + 2;
    }
    else if (mech->nameVariant == 2)
    {
        mech->sortKey = order * 3 + 1;
    }

    mech->binarySize =
        static_cast<uint32_t>(std::strlen(mech->iconName) + 1 + std::strlen(mech->fileName) + 1 + MechImageSize);

    checkKey(file->seekBlock("Engine"), 14);
    checkKey(file->readIdFloat("Tonnage", mech->engineTonnage), 15);
    checkKey(file->readIdULong("Rating", mech->engineRating), 16);
    checkKey(file->readIdUChar("MaxRunSpeed", mech->maxRunSpeed), 17);
    checkKey(file->seekBlock("Armor"), 18);
    checkKey(file->readIdUChar("Type", mech->armorType), 19);
    checkKey(file->readIdFloat("Tonnage", mech->armorTonnage), 20);
    checkKey(file->seekBlock("MaxArmorPoints"), 21);

    if (file->readIdLong("SellValue", mech->sellValue) != 0)
    {
        mech->sellValue = 0x32;
    }

    for (int32_t location = 0; location < 11; ++location)
    {
        checkKey(file->readIdUChar(MechArmorKeys[location], mech->armor[location].maxArmor), 22 + location);
    }

    checkKey(file->seekBlock("CurArmorPoints"), 33);

    for (int32_t location = 0; location < 11; ++location)
    {
        uint8_t points = 0;
        checkKey(file->readIdUChar(MechArmorKeys[location], points), 34 + location);
        mech->armor[location].curArmor = points;
    }

    checkKey(file->seekBlock("InventoryInfo"), 45);
    checkKey(file->readIdUChar("NumOther", mech->numOther), 46);
    checkKey(file->readIdUChar("NumWeapons", mech->numWeapons), 47);
    checkKey(file->readIdUChar("NumAmmo", mech->numAmmo), 48);
    // Original behaviour: 0xc0 bytes of the slots are cleared, the first five locations and a third of the sixth.
    std::memset(mech->itemSlots, 0xff, 0xc0);

    mech->freeTonnage = 0.0f;
    mech->weaponTonnage = 0.0f;
    mech->usedTonnage =
        static_cast<float>(static_cast<double>(mech->curTonnage) * 0.1f + mech->armorTonnage + mech->engineTonnage);
    InventoryList* inventory = mech->inventory;
    char block[32];
    int32_t item = 0;
    const int32_t numOther = mech->numOther;

    for (; item < numOther; ++item)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        checkKey(file->seekBlock(block), 49);
        uint8_t masterID = 0;
        checkKey(file->readIdUChar("MasterID", masterID), 50);
        inventory->addItem(masterID, inventory->createStat(static_cast<uint8_t>(item), 0, 0, 0, 0, 1, 0xff), -1);
        const MasterComponent& master = component(masterID);
        mech->usedTonnage += master.tonnage;

        if (master.form == 2 || master.form == 0x10 || master.form == 0x11 || master.form == 0x12)
        {
            mech->weaponTonnage += master.tonnage;
        }

        mech->resourcePoints += master.resourcePoints;
    }

    const int32_t weaponEnd = numOther + mech->numWeapons;

    for (; item < weaponEnd; ++item)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        checkKey(file->seekBlock(block), 51);
        uint8_t masterID = 0;
        checkKey(file->readIdUChar("MasterID", masterID), 52);
        uint8_t facesForward = 0;
        checkKey(file->readIdUChar("FacesForward", facesForward), 53);
        inventory->addItem(masterID, inventory->createStat(static_cast<uint8_t>(item), 0, 0, facesForward, 0, 1, 0xff),
                           -1);
        const MasterComponent& master = component(masterID);
        mech->usedTonnage += master.tonnage;
        mech->weaponTonnage += master.tonnage;
        mech->resourcePoints += master.resourcePoints;
    }

    const int32_t ammoEnd = numOther + mech->numAmmo + mech->numWeapons;

    for (; item < ammoEnd; ++item)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        checkKey(file->seekBlock(block), 54);
        uint8_t masterID = 0;
        checkKey(file->readIdUChar("MasterID", masterID), 55);
        // The amount is read (as a long, else a byte) but not used: ammunition copies get -1.
        int32_t amount = 0;

        if (file->readIdLong("Amount", amount) != 0)
        {
            uint8_t smallAmount = 0;
            checkKey(file->readIdUChar("Amount", smallAmount), 56);
        }

        inventory->addItem(masterID, inventory->createStat(static_cast<uint8_t>(item), 0, 0, 0, 0, -1, 0xff), -1);
        const MasterComponent& master = component(masterID);
        mech->usedTonnage += master.tonnage;
        mech->weaponTonnage += master.tonnage;
        mech->resourcePoints += master.resourcePoints;
    }

    mech->freeTonnage = (mech->curTonnage - mech->usedTonnage) + mech->weaponTonnage;

    for (int32_t location = 0; location < 8; ++location)
    {
        checkKey(file->seekBlock(MechLocationBlocks[location]), 57);
        uint8_t hasCase = 0;
        checkKey(file->readIdUChar("CASE", hasCase), 58);
        mech->hasCASE[location] = hasCase != 0 ? 1 : 0;
        checkKey(file->readIdUChar("CurInternalStructure", mech->internals[location].curArmor), 59);
        checkKey(file->readIdUChar("HotSpotNumber", mech->hotSpotNumber[location]), 60);

        // The critical slots only carry the damage of the copies they hold.
        for (int32_t space = 0; space < NumLocationCriticalSpaces[location]; ++space)
        {
            std::snprintf(block, sizeof(block), "Component:%d", space);
            uint8_t slot[2] = {};
            checkKey(file->readIdUCharArray(block, slot, 2), 61);

            if (slot[0] < inventory->nextStatID && slot[1] != 0)
            {
                _LogInventoryItem* owner = inventory->getItemStatIndex(slot[0]);

                // Port fix: a copy number with no copy left (the original read a null item's master id).
                if (owner == nullptr)
                {
                    continue;
                }

                const int32_t form = component(owner->masterID).form;

                if (form == 6 || form == 7 || form == 9 || form == 8 || form == 2 || form == 4 || form == 0x10 ||
                    form == 0x11)
                {
                    inventory->hitItem(slot[0], slot[1]);
                }
            }
        }
    }

    mech->unknown68 = 0;
    mech->deployed = 0;

    if (mech->required == 0)
    {
        mech->required = required;
    }

    // In by sort key when sorted, else at the front.
    LogMech* current = mechs;

    if (sorted == 0 || current == nullptr)
    {
        mechs = mech;
    }
    else
    {
        LogMech* previous = nullptr;

        do
        {
            if (mech->sortKey <= current->sortKey)
            {
                break;
            }

            previous = current;
            current = current->next;
        } while (current != nullptr);

        if (previous == nullptr)
        {
            mechs = mech;
        }
        else
        {
            previous->next = mech;
        }
    }

    mech->next = current;
    ++numMechs;

    if (widgets == 0)
    {
        mech->repairBlock = nullptr;
        mech->inventoryBlock = nullptr;
        mech->briefingBox = nullptr;
    }
    else
    {
        mech->repairBlock = new MechRepairBlock;
        Assert(mech->repairBlock != nullptr, 0, " Not enough memory for repair block ");
        mech->repairBlock->init(mech);
        mech->inventoryBlock = new MechInventoryBlock;
        Assert(mech->inventoryBlock != nullptr, 0, " Not enough memory for inventory block ");
        mech->inventoryBlock->init(mech);
        mech->briefingBox = new BriefingBox;
        Assert(mech->briefingBox != nullptr, 0, " Not enough memory for briefing block ");
        mech->briefingBox->init(mech, nullptr);
    }

    mech->calcBR();
    mech->calcPilotModifier();

    // The names shown: weight class (from the tonnage), chassis class (from the armor tonnage), the internal
    // structure's class and the jump jets' class.
    static constexpr uint32_t weightNames[4] = {0x4f, 0x50, 0x51, 0x52};
    mech->weightClassName = loadLogString(weightNames[weightClass(mech->curTonnage)]);

    uint32_t chassisName = 100;

    if (mech->armorTonnage > 2.0f)
    {
        chassisName = 0x4f;

        if (mech->armorTonnage > 7.0f)
        {
            chassisName = 0x65;

            if (mech->armorTonnage > 12.0f)
            {
                chassisName = mech->armorTonnage > 17.0f ? 0x66 : 0x51;
            }
        }
    }

    mech->chassisClassName = loadLogString(chassisName);

    int32_t structure = 0;

    for (const LogMech::ArmorPoints& points : mech->internals)
    {
        structure += points.maxArmor;
    }

    uint32_t structureName = 0x66;

    if (structure < 0x24)
    {
        structureName = 100;
    }
    else if (structure < 0x38)
    {
        structureName = 0x4f;
    }
    else if (structure < 0x51)
    {
        structureName = 0x65;
    }
    else if (structure < 0x79)
    {
        structureName = 0x51;
    }

    mech->extraName1 = loadLogString(structureName);

    int32_t jumpJets = 0;

    for (_LogInventoryItem* entry = inventory->items; entry != nullptr; entry = entry->next)
    {
        if (component(entry->masterID).form == 0xb)
        {
            jumpJets = entry->count;
        }
    }

    uint32_t jumpName = 0x6c;

    if (jumpJets != 0)
    {
        jumpJets = jumpJets * 2 / 3;

        if (jumpJets < 2)
        {
            jumpName = 0x56;
        }
        else if (jumpJets < 4)
        {
            jumpName = 0x55;
        }
        else
        {
            jumpName = 0x50;
        }
    }

    mech->extraName2 = loadLogString(jumpName);
    return mech;
}

auto LogMechList::addMech(LogMech* mech, int sorted) -> int32_t
{
    // In by tonnage when sorted, else at the front.
    LogMech* current = mechs;
    LogMech* previous = nullptr;

    if (sorted != 0 && current != nullptr)
    {
        do
        {
            if (mech->curTonnage <= current->curTonnage)
            {
                break;
            }

            previous = current;
            current = current->next;
        } while (current != nullptr);
    }

    if (previous != nullptr)
    {
        previous->next = mech;
    }
    else
    {
        mechs = mech;
    }

    mech->next = current;
    ++numMechs;
    return 0;
}

auto LogMechList::extractMech(int32_t index, LogMech*& mech) -> int32_t
{
    if (numMechs <= index)
    {
        return -1;
    }

    LogMech* current = mechs;

    if (index > 0)
    {
        LogMech* previous = nullptr;

        for (; index > 0; --index)
        {
            previous = current;
            current = current->next;
        }

        previous->next = current->next;
        --numMechs;
        mech = current;
        return 0;
    }

    mechs = current->next;
    --numMechs;
    mech = current;
    return 0;
}

auto LogMechList::removeMech(uint8_t index) -> int32_t
{
    if (numMechs <= index)
    {
        return -1;
    }

    LogMech* previous = nullptr;
    LogMech* mech = mechs;

    for (int32_t count = index; count > 0; --count)
    {
        previous = mech;
        mech = mech->next;
    }

    return deleteMech(mech, previous);
}

auto LogMechList::removeMech(LogMech* mech) -> int32_t
{
    LogMech* previous = nullptr;
    LogMech* current = mechs;

    while (current != mech)
    {
        if (current == nullptr)
        {
            return -1;
        }

        previous = current;
        current = current->next;
    }

    if (current == nullptr)
    {
        return -1;
    }

    return deleteMech(mech, previous);
}

auto LogMechList::deleteMech(LogMech* mech, LogMech* previous) -> int32_t
{
    for (char** text : {&mech->weightClassName, &mech->chassisClassName, &mech->extraName1, &mech->extraName2})
    {
        if (*text != nullptr)
        {
            logFree(*text);
            *text = nullptr;
        }
    }

    if (globalLogPtr->repairScreen != nullptr)
    {
        globalLogPtr->repairScreen->unitPane->removeChild(mech->repairBlock);
    }

    for (char** text : {&mech->fileName, &mech->iconName, &mech->description})
    {
        if (*text != nullptr)
        {
            logFree(*text);
            *text = nullptr;
        }
    }

    if (mech->inventory != nullptr)
    {
        mech->inventory->destroy();
        delete mech->inventory;
        mech->inventory = nullptr;
    }

    if (mech->briefingBox != nullptr)
    {
        delete mech->briefingBox;
        mech->briefingBox = nullptr;
    }

    if (mech->mechName != nullptr)
    {
        logFree(mech->mechName);
        mech->mechName = nullptr;
    }

    if (mech->repairBlock != nullptr)
    {
        delete mech->repairBlock;
        mech->repairBlock = nullptr;
    }

    if (mech->inventoryBlock != nullptr)
    {
        delete mech->inventoryBlock;
        mech->inventoryBlock = nullptr;
    }

    if (mech->briefBlock != nullptr)
    {
        delete mech->briefBlock;
        mech->briefBlock = nullptr;
    }

    if (previous == nullptr)
    {
        mechs = mech->next;
    }
    else
    {
        previous->next = mech->next;
    }

    logFree(mech);
    --numMechs;
    return 0;
}

auto LogMechList::getMechCount() -> int32_t
{
    return numMechs;
}

auto LogMechList::getMechSize(uint32_t index) -> int32_t
{
    if (static_cast<uint32_t>(numMechs) <= index)
    {
        return 0;
    }

    LogMech* mech = mechs;

    for (; static_cast<int32_t>(index) > 0; --index)
    {
        mech = mech->next;
    }

    if (mech == nullptr)
    {
        return 0;
    }

    return mech->inventory->getBinaryData(nullptr) + static_cast<int32_t>(mech->binarySize);
}

auto LogMechList::getMechPilotIndex(int32_t index) -> int32_t
{
    if (index >= numMechs)
    {
        return -1;
    }

    LogMech* mech = mechs;

    for (; index > 0; --index)
    {
        mech = mech->next;
    }

    return mech != nullptr ? mech->pilotIndex : -1;
}

auto LogMechList::getMechInfo(int32_t index, LogMech*& mech) -> int32_t
{
    mech = nullptr;

    if (index >= numMechs)
    {
        return -1;
    }

    LogMech* current = mechs;

    for (; index > 0; --index)
    {
        current = current->next;
    }

    if (current == nullptr)
    {
        return -1;
    }

    mech = current;
    return 0;
}

auto LogMechList::getBinaryData(uint32_t index, void* data) -> int32_t
{
    if (static_cast<uint32_t>(numMechs) <= index)
    {
        return -1;
    }

    LogMech* mech = mechs;

    for (; static_cast<int32_t>(index) > 0; --index)
    {
        mech = mech->next;
    }

    if (mech == nullptr)
    {
        return -1;
    }

    // Port fix (OB-089): the original copied binarySize bytes from the record (reading on past it) and put the
    // strings after them; the port writes the record's image, then the name, the icon and the inventory.
    auto* out = static_cast<uint8_t*>(data);
    ImageWriter writer(out);
    visitMech(writer, *mech);
    out += MechImageSize;
    out = putString(out, mech->fileName);
    out = putString(out, mech->iconName);
    mech->inventory->getBinaryData(out);
    return 0;
}

auto LogMechList::saveMechText(char* fileName, int32_t index) -> int32_t
{
    if (numMechs <= index)
    {
        return -1;
    }

    LogMech* mech = mechs;

    for (int32_t count = index; count > 0; --count)
    {
        mech = mech->next;
    }

    MissionLogisticsBridge bridge;
    return bridge.logisticsMechProfileWriter(fileName, mech, 0);
}

auto LogMechList::saveMechBinary(char* fileName, int32_t index) -> int32_t
{
    File file;
    char path[256];
    std::snprintf(path, sizeof(path), "%s%s.fit", saveTempPath, fileName);
    file.create(path);
    file.writeLong(numMechs);
    // Port fix (OB-089): the original handed getBinaryData the address of its buffer pointer; the port passes the
    // buffer.
    auto writeMech = [&](uint32_t mech)
    {
        const int32_t size = getMechSize(mech);
        std::vector<uint8_t> buffer(static_cast<size_t>(std::max(size, 0)));
        getBinaryData(mech, buffer.data());
        file.write(buffer.data(), size);
    };

    if (index == -1)
    {
        for (int32_t mech = 0; mech < numMechs; ++mech)
        {
            writeMech(static_cast<uint32_t>(mech));
        }

        file.close();
        return numMechs;
    }

    if (numMechs <= index)
    {
        return -1;
    }

    writeMech(static_cast<uint32_t>(index));
    file.close();
    return 0;
}

//---------------------------------------------------------------------------
// LogVehicleList

LogVehicleList::LogVehicleList()
{
    vehicles = nullptr;
    numVehicles = 0;
}

auto LogVehicleList::operator new(size_t size) noexcept -> void*
{
    return logAlloc(size);
}

auto LogVehicleList::operator delete(void* ptr) -> void
{
    logFree(ptr);
}

auto LogVehicleList::destroy() -> void
{
    while (numVehicles != 0)
    {
        removeVehicle(static_cast<uint8_t>(0));
    }

    vehicles = nullptr;
}

auto LogVehicleList::getVehicleIndex(LogVehicle* vehicle) -> int32_t
{
    LogVehicle* current = vehicles;

    for (int32_t index = 0; index < numVehicles; ++index)
    {
        if (current == vehicle)
        {
            return index;
        }

        current = current->next;
    }

    return -1;
}

auto LogVehicleList::addVehicle(char* fileName, int required, int sorted, int widgets) -> LogVehicle*
{
    (void)widgets; // Original behaviour: vehicles read by name always get their widgets.
    FitIniFile file;
    FullPathFileName path;
    path.init(profilePath, fileName, ".fit");
    const int32_t result = file.open(path);
    Assert(result == 0, static_cast<uint32_t>(result), " could not open vehicle Profile file ");
    LogVehicle* vehicle = addVehicle(&file, required, sorted, 1);
    std::strncpy(vehicle->profileName, fileName, 9);
    file.close();
    return vehicle;
}

auto LogVehicleList::replaceVehicle(PacketFile*, int32_t) -> int32_t
{
    return 0;
}

auto LogVehicleList::addVehicle(PacketFile* file, int32_t packet) -> LogVehicle*
{
    FitIniFile profile;
    int32_t result = file->seekPacket(packet);
    Assert(result == 0, 0, " Vehicle Packet Not Found ");
    result = profile.open(file, static_cast<uint32_t>(file->getPacketSize()));
    Assert(result == 0, 0, " Vehicle file could not open ");
    return addVehicle(&profile, 0, 0, 1);
}

auto LogVehicleList::addVehicle(FitIniFile* file, int required, int sorted, int widgets) -> LogVehicle*
{
    auto* vehicle = allocRecord<LogVehicle>();
    Assert(vehicle != nullptr, 0, "Not enough memory for LogVehicle");
    vehicle->localPart = 1;
    vehicle->partType = 2;

    if (file->seekBlock("General") != 0)
    {
        // A saved vehicle list: a count, then each record's image and its name and icon.
        // Original behaviour (OB-089): every record is read into this one vehicle, which is returned without being
        // added to the list. Nothing in MCX.EXE writes such a file.
        file->seek(0);
        int32_t count = file->readLong();
        std::array<uint8_t, VehicleImageSize> image{};

        while (count > 0)
        {
            file->read(image.data(), static_cast<int32_t>(image.size()));
            ImageReader reader(image.data());
            visitVehicle(reader, *vehicle);
            vehicle->fileName = readImageString(file, "Not enough memory for LogVehicle");
            vehicle->iconName = readImageString(file, "Not enough memory for LogVehicle");
            --count;
        }

        return vehicle;
    }

    static const char* const locationBlocks[5] = {"Front", "Left", "Right", "Rear", "Turret"};
    int32_t result = file->seekBlock("Header");
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 1");
    char text[256];
    result = file->readIdString("FileType", text, 0x7f);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 2");
    Assert(std::strcmp(text, "GroundVehicleProfile") == 0, 0, "Failed addVehicle - 2");
    result = file->seekBlock("General");
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 3");
    vehicle->briefBlock = nullptr;
    result = file->readIdLong("NameIndex", vehicle->nameIndex);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not read NameIndex in vehicle profile");
    result = file->readIdFloat("CurTonnage", vehicle->curTonnage);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 4");
    result = file->readIdChar("Status", vehicle->status);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 5");
    result = file->readIdULong("Chassis", vehicle->chassis);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 6");
    {
        // Original behaviour: the object packet file is opened and closed again, unused.
        PacketFile objects;
        char path[256];
        std::snprintf(path, sizeof(path), "%s%s", objectPath, objectPakName);
        result = objects.open(path);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 7");
    }

    // Port fix (OB-093): the original read up to 255 characters into the 9-byte crew field.
    result = file->readIdString("Crew", vehicle->crew, sizeof(vehicle->crew) - 1);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not read crew in vehicle profile");

    if (file->readIdLong("ResourcePoints", vehicle->vehicleResourcePoints) != 0)
    {
        vehicle->vehicleResourcePoints = 100;
    }

    vehicle->baseVehicleResourcePoints = vehicle->vehicleResourcePoints;
    result = file->readIdString("icon", text, 0xff);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 7");
    vehicle->iconName = logStrDup(text);

    if (file->readIdBoolean("Assigned", vehicle->assigned) != 0)
    {
        vehicle->assigned = 0;
    }

    if (file->readIdBoolean("Deployed", vehicle->deployed) != 0)
    {
        vehicle->deployed = 0;
    }

    if (file->readIdBoolean("Required", vehicle->required) != 0)
    {
        vehicle->required = 0;
    }

    vehicle->description = nullptr;
    vehicle->descIndex = -1;
    file->readIdLong("DescIndex", vehicle->descIndex);
    vehicle->loadDescription(vehicle->descIndex);
    vehicle->fileName = loadLogString(static_cast<uint32_t>(vehicle->descIndex + 700));
    // Port fix (OB-089): the original never set a vehicle's saved size (its heap block's old contents stood).
    vehicle->binarySize = static_cast<uint32_t>(std::strlen(vehicle->iconName) + 1 + std::strlen(vehicle->fileName) +
                                                1 + VehicleImageSize);

    result = file->seekBlock("Engine");
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 9");
    result = file->readIdFloat("Tonnage", vehicle->engineTonnage);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 10");
    result = file->readIdULong("Rating", vehicle->engineRating);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 11");
    result = file->readIdUChar("MaxMoveSpeed", vehicle->maxMoveSpeed);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 12");
    result = file->seekBlock("Armor");
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 13");
    result = file->readIdUChar("Type", vehicle->armorType);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 14");
    result = file->readIdFloat("Tonnage", vehicle->armorTonnage);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 15");
    result = file->seekBlock("InventoryInfo");
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 16");
    result = file->readIdUChar("NumOther", vehicle->numOther);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 17");
    result = file->readIdUChar("NumWeapons", vehicle->numWeapons);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 18");
    result = file->readIdUChar("NumAmmo", vehicle->numAmmo);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 19");
    auto* inventory = new InventoryList;
    vehicle->inventory = inventory;
    Assert(inventory != nullptr, static_cast<uint32_t>(result), "Failed addVehicle - 19");

    char block[32];
    int32_t item = 0;
    const int32_t numOther = vehicle->numOther;

    for (; item < numOther; ++item)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        result = file->seekBlock(block);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 19a");
        uint8_t masterID = 0;
        result = file->readIdUChar("MasterID", masterID);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 19b");
        inventory->addItem(masterID, inventory->createStat(static_cast<uint8_t>(item), 0, 0, 0, 0, 1, 0xff), -1);
    }

    const int32_t weaponEnd = numOther + vehicle->numWeapons;

    for (; item < weaponEnd; ++item)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        result = file->seekBlock(block);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 20");
        uint8_t masterID = 0;
        result = file->readIdUChar("MasterID", masterID);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 21");
        uint8_t facesForward = 0;
        result = file->readIdUChar("FacesForward", facesForward);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 22");
        inventory->addItem(masterID, inventory->createStat(static_cast<uint8_t>(item), 0, 0, facesForward, 0, 1, 0xff),
                           -1);
    }

    const int32_t ammoEnd = numOther + vehicle->numAmmo + vehicle->numWeapons;

    for (; item < ammoEnd; ++item)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        result = file->seekBlock(block);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 23");
        uint8_t masterID = 0;
        result = file->readIdUChar("MasterID", masterID);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 24");
        int32_t amount = 0;

        if (file->readIdLong("Amount", amount) != 0)
        {
            uint8_t smallAmount = 0;
            result = file->readIdUChar("Amount", smallAmount);
            Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 25");
            amount = smallAmount;
        }

        inventory->addItem(
            masterID, inventory->createStat(static_cast<uint8_t>(item), 0, 0, 0, 0, static_cast<int16_t>(amount), 0xff),
            -1);
    }

    for (int32_t location = 0; location < 5; ++location)
    {
        result = file->seekBlock(locationBlocks[location]);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 26");
        result = file->readIdUChar("CurInternalStructure", vehicle->curInternalStructure[location]);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 27");
        result = file->readIdUChar("MaxArmorPoints", vehicle->maxArmorPoints[location]);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 28");
        result = file->readIdUChar("CurArmorPoints", vehicle->curArmorPoints[location]);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 29");
    }

    vehicle->unknown68 = 0;
    vehicle->notMineYet = 0;

    if (vehicle->required == 0)
    {
        vehicle->required = required;
    }

    vehicle->calcVehicleCost();

    if (widgets == 0)
    {
        vehicle->inventoryBlock = nullptr;
        vehicle->repairBlock = nullptr;
        vehicle->briefingBox = nullptr;
    }
    else
    {
        vehicle->inventoryBlock = new VehicleInventoryBlock;
        Assert(vehicle->inventoryBlock != nullptr, 0, " Not enough memory for vehicleInvBlock block ");
        vehicle->inventoryBlock->init(vehicle);
        vehicle->repairBlock = new VehicleRepairBlock;
        Assert(vehicle->repairBlock != nullptr, 0, " Not enough memory for repair block ");
        vehicle->repairBlock->init(vehicle);
        vehicle->briefingBox = new BriefingBox;
        Assert(vehicle->briefingBox != nullptr, 0, " Not enough memory for vehicle briefing block ");
        vehicle->briefingBox->init(nullptr, vehicle);
    }

    // In by tonnage when sorted, else at the front.
    LogVehicle* current = vehicles;
    LogVehicle* previous = nullptr;

    if (sorted != 0 && current != nullptr)
    {
        do
        {
            if (vehicle->curTonnage <= current->curTonnage)
            {
                break;
            }

            previous = current;
            current = current->next;
        } while (current != nullptr);
    }

    if (previous != nullptr)
    {
        previous->next = vehicle;
    }
    else
    {
        vehicles = vehicle;
    }

    vehicle->next = current;
    ++numVehicles;
    return vehicle;
}

auto LogVehicleList::removeVehicle(uint8_t index) -> int32_t
{
    if (numVehicles <= index)
    {
        return -1;
    }

    LogVehicle* previous = nullptr;
    LogVehicle* vehicle = vehicles;

    for (int32_t count = index; count > 0; --count)
    {
        previous = vehicle;
        vehicle = vehicle->next;
    }

    return deleteVehicle(vehicle, previous);
}

auto LogVehicleList::removeVehicle(LogVehicle* vehicle) -> int32_t
{
    LogVehicle* previous = nullptr;
    LogVehicle* current = vehicles;

    while (current != vehicle)
    {
        if (current == nullptr)
        {
            return -1;
        }

        previous = current;
        current = current->next;
    }

    if (current == nullptr)
    {
        return -1;
    }

    return deleteVehicle(vehicle, previous);
}

auto LogVehicleList::deleteVehicle(LogVehicle* vehicle, LogVehicle* previous) -> int32_t
{
    for (char** text : {&vehicle->fileName, &vehicle->iconName, &vehicle->description})
    {
        if (*text != nullptr)
        {
            logFree(*text);
            *text = nullptr;
        }
    }

    if (vehicle->inventory != nullptr)
    {
        vehicle->inventory->destroy();
        delete vehicle->inventory;
        vehicle->inventory = nullptr;
    }

    if (vehicle->briefingBox != nullptr)
    {
        delete vehicle->briefingBox;
        vehicle->briefingBox = nullptr;
    }

    if (vehicle->repairBlock != nullptr)
    {
        delete vehicle->repairBlock;
        vehicle->repairBlock = nullptr;
    }

    if (vehicle->inventoryBlock != nullptr)
    {
        delete vehicle->inventoryBlock;
        vehicle->inventoryBlock = nullptr;
    }

    if (vehicle->briefBlock != nullptr)
    {
        delete vehicle->briefBlock;
        vehicle->briefBlock = nullptr;
    }

    if (previous == nullptr)
    {
        vehicles = vehicle->next;
    }
    else
    {
        previous->next = vehicle->next;
    }

    logFree(vehicle);
    --numVehicles;
    return 0;
}

auto LogVehicleList::getVehicleInfo(int32_t index, LogVehicle*& vehicle) -> int32_t
{
    vehicle = nullptr;

    if (index >= numVehicles)
    {
        return -1;
    }

    LogVehicle* current = vehicles;

    for (; index > 0; --index)
    {
        current = current->next;
    }

    if (current == nullptr)
    {
        return -1;
    }

    vehicle = current;
    return 0;
}

auto LogVehicleList::getVehicleCount() -> int32_t
{
    return numVehicles;
}

auto LogVehicleList::getVehicleSize(uint32_t index) -> int32_t
{
    if (static_cast<uint32_t>(numVehicles) <= index)
    {
        return 0;
    }

    LogVehicle* vehicle = vehicles;

    for (; static_cast<int32_t>(index) > 0; --index)
    {
        vehicle = vehicle->next;
    }

    if (vehicle == nullptr)
    {
        return 0;
    }

    return vehicle->inventory->getBinaryData(nullptr) + static_cast<int32_t>(vehicle->binarySize);
}

auto LogVehicleList::getBinaryData(uint32_t index, void* data) -> int32_t
{
    if (static_cast<uint32_t>(numVehicles) <= index)
    {
        return -1;
    }

    LogVehicle* vehicle = vehicles;

    for (; static_cast<int32_t>(index) > 0; --index)
    {
        vehicle = vehicle->next;
    }

    if (vehicle == nullptr)
    {
        return -1;
    }

    // Port fix (OB-089): as LogMechList::getBinaryData.
    auto* out = static_cast<uint8_t*>(data);
    ImageWriter writer(out);
    visitVehicle(writer, *vehicle);
    out += VehicleImageSize;
    out = putString(out, vehicle->fileName);
    out = putString(out, vehicle->iconName);
    vehicle->inventory->getBinaryData(out);
    return 0;
}

auto LogVehicleList::saveVehicleText(char* fileName, int32_t index) -> int32_t
{
    if (numVehicles <= index)
    {
        return -1;
    }

    LogVehicle* vehicle = vehicles;

    for (int32_t count = index; count > 0; --count)
    {
        vehicle = vehicle->next;
    }

    MissionLogisticsBridge bridge;
    bridge.logisticsVehicleProfileWriter(fileName, vehicle, 0);
    return 0;
}

auto LogVehicleList::saveVehicleBinary(char* fileName, int32_t index) -> int32_t
{
    File file;
    char path[256];
    std::snprintf(path, sizeof(path), "%s%s.fit", saveTempPath, fileName);
    file.create(path);
    file.writeLong(numVehicles);
    // Port fix (OB-089): as LogMechList::saveMechBinary.
    auto writeVehicle = [&](uint32_t vehicle)
    {
        const int32_t size = getVehicleSize(vehicle);
        std::vector<uint8_t> buffer(static_cast<size_t>(std::max(size, 0)));
        getBinaryData(vehicle, buffer.data());
        file.write(buffer.data(), size);
    };

    if (index == -1)
    {
        for (int32_t vehicle = 0; vehicle < numVehicles; ++vehicle)
        {
            writeVehicle(static_cast<uint32_t>(vehicle));
        }

        file.close();
        return numVehicles;
    }

    if (numVehicles <= index)
    {
        return -1;
    }

    writeVehicle(static_cast<uint32_t>(index));
    file.close();
    return 1;
}

//---------------------------------------------------------------------------
// DropSlot

auto DropSlot::operator new(size_t size) noexcept -> void*
{
    return logAlloc(size);
}

auto DropSlot::operator delete(void* ptr) -> void
{
    logFree(ptr);
}

//---------------------------------------------------------------------------
// Free functions

auto logisticsCallback() -> void
{
}

namespace
{
    /// <summary>
    /// What the id comparers sort by: the 32-bit value at +0x8 of the part an element points to, which is the middle
    /// of <see cref="LogPart::profileName"/> (the comparers are unused; whatever id they meant is gone).
    /// </summary>
    int32_t partSortValue(const void* element)
    {
        const LogPart* part = *static_cast<const LogPart* const*>(element);
        int32_t value = 0;
        std::memcpy(&value, part->profileName + 4, sizeof(value));
        return value;
    }
}

auto CompareLogMechIDs(const void* a, const void* b) -> int
{
    const int32_t first = partSortValue(a);
    const int32_t second = partSortValue(b);

    if (first < second)
    {
        return -1;
    }

    return second < first ? 1 : 0;
}

auto CompareLogVehicleIDs(const void* a, const void* b) -> int
{
    const int32_t first = partSortValue(a);
    const int32_t second = partSortValue(b);

    if (first < second)
    {
        return -1;
    }

    return second < first ? 1 : 0;
}

auto MyGetUserName(char* name, uint32_t* size) -> int
{
    // The name last entered on the multiplayer screens (HKLM\Software\FASA Interactive\MechCommander Expansion,
    // "Player Name"; the port's registry file), else the logged-in user's name.
    if (const std::optional<std::string> saved =
            MCRegistry::Read("Software\\FASA Interactive\\MechCommander Expansion", "Player Name"))
    {
        const auto needed = static_cast<uint32_t>(saved->size() + 1);

        if (needed <= *size)
        {
            *size = needed;
            std::memcpy(name, saved->c_str(), needed);
            return 1;
        }

        // Port fix: RegQueryValueExA set the size to the one it needed before failing, and GetUserNameA was then
        // given that larger size for the same buffer. The port keeps the caller's size.
    }

    return MCPort::GetUserName(name, size) ? 1 : 0;
}

//---------------------------------------------------------------------------
// MPPlayerLights

auto MPPlayerLights::init() -> void
{
    lObject::init(0xd8, 0, 1, 0x10, nullptr, nullptr);
    numPlayers = 0;

    for (uint32_t& id : playerIDs)
    {
        id = 0;
    }

    for (int32_t& status : playerStatus)
    {
        status = 0;
    }

    backgroundParent = nullptr;
    timerRunning = 0;
    blinkOn = 0;
    char fileName[256];
    // One light's width comes from the first player's light.
    lightsPort = new lPort;
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsc_p1.tga", artPath);
    lightsPort->init(fileName);
    lightWidth = lightsPort->width();
    lightsPort->destroy();
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsc_pn.tga", artPath);
    lightsPort->init(fileName);
    readyPort = new lPort;
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsc_pg.tga", artPath);
    readyPort->init(fileName);
    blinkPort = new lPort;
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsc_pg1.tga", artPath);
    blinkPort->init(fileName);
    draw();
}

auto MPPlayerLights::destroy() -> void
{
    if (timerRunning != 0)
    {
        application->RemoveTimer(this, 3);
    }

    delete lightsPort;
    lightsPort = nullptr;
    delete readyPort;
    readyPort = nullptr;
    delete blinkPort;
    blinkPort = nullptr;
    lObject::destroy();
}

auto MPPlayerLights::setNumPlayers(int32_t count) -> void
{
    numPlayers = count;
    resize(lightWidth * count, height());
}

auto MPPlayerLights::setPlayerID(int32_t light, uint32_t playerID) -> void
{
    if (light < numPlayers)
    {
        playerIDs[light] = playerID;
    }
}

auto MPPlayerLights::setPlayerStatus(uint32_t playerID, int32_t status) -> void
{
    int32_t light = 0;

    if (numPlayers > 0)
    {
        while (light < numPlayers && playerIDs[light] != playerID)
        {
            light++;
        }
    }

    // Original behaviour (OB-099): an unknown player sets the status of the light after the last one. Port fix:
    // with six lights that index is past playerStatus (the original overwrote lightWidth), so it is skipped.
    if (status >= 0 && status < 3 && light < MAX_PLAYERS)
    {
        playerStatus[light] = status;
        draw();
    }

    if (timerRunning == 0 && status == 2)
    {
        application->AddTimer(this, 3, 500, 0, 0, 0);
        timerRunning = 1;
    }
}

auto MPPlayerLights::draw() -> void
{
    if (!lport()->viewOpen())
    {
        Refresh();
        return;
    }

    _pane* target = lport()->frame();

    for (int32_t light = 0; light < shownPlayers; light++)
    {
        // The numbered light, then the status over it: 1 lit, 2 blinking (while the timer runs).
        lPort* lightPort = logArtf("%slogart\\lsc_p%d.tga", artPath, light + 1);

        if (lightPort == nullptr)
        {
            continue;
        }

        lightPort->copyTo(target, lightPort->width() * light, 0, 0);
        const int32_t status = shownStatus[light];

        if (status == 1)
        {
            if (lPort* statusPort = logArtf("%slogart\\lsc_ph.tga", artPath))
            {
                statusPort->copyTo(target, lightPort->width() * light, 2, 1);
            }
        }
        else if (status == 2 && shownTimerRunning != 0)
        {
            lPort* blink = shownBlinkOn == 0 ? blinkPort : readyPort;
            blink->copyTo(target, lightWidth * light, 2, 1);
        }
    }
}

auto MPPlayerLights::Refresh() -> void
{
    if (backgroundParent != parent)
    {
        if (LogScreenChrome* chrome = static_cast<lObject*>(parent)->Chrome(); chrome != nullptr)
        {
            // The parent screen shows the backing from now on (the original painted it into its picture).
            chrome->lightsBackShown = true;
        }
    }

    shownPlayers = std::min(numPlayers, MAX_PLAYERS);

    for (int32_t light = 0; light < MAX_PLAYERS; light++)
    {
        shownStatus[light] = playerStatus[light];
    }

    shownTimerRunning = timerRunning;
    shownBlinkOn = blinkOn;
}

auto MPPlayerLights::handleEvent(aEvent* event) -> void
{
    if (parent == nullptr)
    {
        return;
    }

    if (event->type == 0x13)
    {
        blinkOn = blinkOn == 0 ? 1 : 0;
        draw();
    }

    // Pointing at a light shows its player's name on the ticker.
    const int32_t light = (event->x - 0xd8) / lightWidth;

    if (light >= 0 && light < numPlayers && globalLogPtr->ticker != nullptr && MPlayer != nullptr)
    {
        const uint32_t playerID = playerIDs[light];

        if (MPlayer->sessionManager->GetPlayer(playerID) != nullptr)
        {
            globalLogPtr->ticker->setString(MPlayer->sessionManager->GetPlayer(playerID)->name);
        }
    }
}

namespace
{
    /// <summary>Screen element <paramref name="index"/> of <paramref name="screen"/> as a <typeparamref name="T"/>.</summary>
    template <typename T> T* screenElement(GenericScreen* screen, int32_t index)
    {
        return static_cast<T*>(screen->elements[index]);
    }

    /// <summary>A new <paramref name="width"/> x <paramref name="height"/> port with its own bitmap.</summary>
    lPort* newPort(int32_t width, int32_t height)
    {
        auto* port = new lPort;
        port->init(width, height, 1);
        return port;
    }

    /// <summary>A new port loaded from the art file <paramref name="format"/> names (its <c>%s</c> is <paramref name="path"/>).</summary>
    lPort* newPort(const char* format, const char* path)
    {
        char fileName[256];
        std::snprintf(fileName, sizeof(fileName), format, path);
        auto* port = new lPort;
        port->init(fileName);
        return port;
    }

    /// <summary>Deletes <paramref name="port"/> and clears the pointer.</summary>
    void deletePort(lPort*& port)
    {
        delete port;
        port = nullptr;
    }

    /// <summary>
    /// Loads the whole of shape file <paramref name="fileName"/> onto the logistics heap (the repair and icon shapes).
    /// </summary>
    void* readShapeFile(File& file, const char* sizeError)
    {
        const uint32_t length = file.getLength();
        void* shapes = logAlloc(length);
        Assert(shapes != nullptr, 0, "Not enough memory for mechrep buffer");
        const int32_t read = file.read(static_cast<uint8_t*>(shapes), static_cast<int32_t>(length));
        Assert(static_cast<uint32_t>(read) == length, 0, sizeError);
        file.close();
        return shapes;
    }

    /// <summary>Opens shape file <paramref name="fileName"/> (asserting it exists) and loads it (<see cref="readShapeFile"/>).</summary>
    void* loadShapeFile(File& file, const char* fileName, const char* openError, const char* sizeError)
    {
        const int32_t result = file.open(fileName);
        Assert(result == 0, static_cast<uint32_t>(result), openError);
        return readShapeFile(file, sizeError);
    }

    /// <summary>Opens screen ini <paramref name="name"/><c>.fit</c> under <c>artPath</c>.</summary>
    void openScreenFile(FitIniFile& file, const char* name, const char* missingError)
    {
        FullPathFileName fileName;
        fileName.init(artPath, name, ".fit");
        const int32_t result = file.open(fileName);
        Assert(result == 0, static_cast<uint32_t>(result), missingError);
    }

    /// <summary>Makes <paramref name="screen"/>'s elements from its ini file (<see cref="openScreenFile"/>).</summary>
    void initSplashScreen(MCSplashScreen* screen, FitIniFile& file, const char* startError)
    {
        const int32_t result = screen->init(&file);
        Assert(result == 0, static_cast<uint32_t>(result), startError);
    }

    /// <summary>Sets up a multiplayer screen's name field: the white font, the background, a 16-character buffer.</summary>
    lTextObject* setUpNameField(GenericScreen* screen, int32_t index)
    {
        auto* field = screenElement<lTextObject>(screen, index);
        field->font = medWhiteFont;
        field->setBackColor(0x10);
        return field;
    }

    /// <summary>Frees every name in a <c>net*.rsp</c> list and empties it.</summary>
    /// <remarks>
    /// The original walked the list with its cursor, which for the warrior names skips a link after each removal;
    /// removing the tail rewinds the cursor to the head, so every name is still freed.
    /// </remarks>
    void freeNameList(FLinkedList<char>& list)
    {
        while (list.head != nullptr)
        {
            char* name = list.head->data;
            logFree(name);
            list.Del(name);
        }
    }

    /// <summary>Reads a <c>net*.rsp</c> list (one name per line, each on the logistics heap) from <c>profilePath</c>.</summary>
    void readNameList(File& file, const char* name, const char* missingError, FLinkedList<char>& list)
    {
        FullPathFileName fileName;
        fileName.init(profilePath, name, ".rsp");
        const int32_t result = file.open(fileName);
        Assert(result == 0, static_cast<uint32_t>(result), missingError);

        while (true)
        {
            auto* line = static_cast<char*>(logAlloc(0x29));
            file.readLine(reinterpret_cast<uint8_t*>(line), 0x28);

            if (file.eof())
            {
                logFree(line);
                break;
            }

            list.Add(line);
        }

        file.close();
    }
}

auto Logistics::init() -> void
{
    if (EmptyFile == nullptr)
    {
        EmptyFile = static_cast<char*>(std::malloc(0xff));
        cLoadString(thisInstance, 0x381, EmptyFile, 0xfe);
    }

    autoPlayMovie = 0;
    messageBuffer = nullptr;
    globalLogPtr = this;
    currentScreen = nullptr;
    dragIcon = nullptr;
    purMechList = nullptr;
    purVehicleList = nullptr;
    purPilotList = nullptr;
    missionFileName = nullptr;
    currentInvTab = 0;
    currentMission = -1;
    unknown08 = 1;
    nextWarriorID = 1;
    ResourcePoints = -9999;
    campaignBriefingName = nullptr;
    operationCinema = nullptr;
    mpWarriorList = nullptr;
    playerLights = nullptr;
    hammerDown = 0;

    static char logisticsTitle[0x400];
    std::snprintf(logisticsTitle, sizeof(logisticsTitle), "%s -- %s", appName, "Logistics");

    // Port: SetWindowTextA -> the SDL window's title.
    if (MCDisplay* display = MCInput::Display())
    {
        display->SetTitle(logisticsTitle);
    }

    std::strcpy(WindowTitle, logisticsTitle);

    logisticsHeap = new UserHeap;
    const int32_t heapResult = logisticsHeap->init(LogisticsHeapSize, "logistics");
    Assert(heapResult == 0, 0, " Could not allocate logistics heap ");
    logisticsHeap->unknown2C = 1;
    unknown238 = 0;
    logisticsState = 0;
    unknown08 = 0;

    workPort0 = newPort(0x1ab, 0x1ce);
    workPort1 = newPort(0x1ab, 0x1ce);

    for (int32_t lance = 0; lance < 3; ++lance)
    {
        for (int32_t slot = 0; slot < 4; ++slot)
        {
            deploySlots[lance][slot].unit = -1;
            deploySlots[lance][slot].vehicle = -1;
            deploySlotInfo[lance][slot] = {};
        }
    }

    std::memset(localDropSlot, 0, sizeof(localDropSlot));
    playerColors[0] = 1;
    playerColors[1] = 3;
    playerColors[2] = 4;
    playerColors[3] = 2;
    playerColors[4] = 6;
    playerColors[5] = 5;

    mechList = new LogMechList;
    Assert(mechList != nullptr, 0, "Could not initialize mech list");
    warriorList = new LogWarriorList;
    Assert(warriorList != nullptr, 0, "Could not initialize warrior list");
    assignedWarriorList = new LogWarriorList;
    Assert(assignedWarriorList != nullptr, 0, "Could not initialize assigndWarrior list");
    vehicleList = new LogVehicleList;
    Assert(vehicleList != nullptr, 0, "Could not initialize vehicles list");

    for (int32_t player = 0; player < 3; ++player)
    {
        mpMechLists[0][player] = nullptr;
        mpVehicleLists[0][player] = nullptr;
        mpMechLists[1][player] = nullptr;
        mpVehicleLists[1][player] = nullptr;
    }

    multiplayerInitialized = 0;
    defaultPlanningTime = 0xf0;
    planningTime = 0xf0;
    mpMissionName = nullptr;
    forceMechList = new LogMechList;
    Assert(forceMechList != nullptr, 0, "Could not initialize assignedMech list");
    forceVehicleList = new LogVehicleList;
    Assert(forceVehicleList != nullptr, 0, "Could not initialize assignedVehicles list");
    componentInventory = new InventoryList;
    Assert(componentInventory != nullptr, 0, "Could not initialize inventory list");
    purchaseComponents = new InventoryList;
    Assert(purchaseComponents != nullptr, 0, "Could not initialize purchaseInventory list");

    inventoryIconPorts[0] = newPort("%slogart\\lsciim.tga", artPath);
    inventoryIconPorts[1] = newPort("%slogart\\lsciip.tga", artPath);
    inventoryIconPorts[2] = newPort("%slogart\\lsciic.tga", artPath);
    inventoryIconPorts[3] = newPort("%slogart\\lsciiv.tga", artPath);

    loadScreen = new MCSplashScreen;
    saveScreen = new MCSplashScreen;

    if (InDemo == 0)
    {
        chatWindow = new LogChatWindow;
        chatWindow->init(7, 0x44, 0xbf, 0x101, 100000);
        chatWindow->ShowGUIWindow(0);
        multiplayerScreen = new MCSplashScreen;
        serialScreen = new MCSplashScreen;
        lanScreen = new MCSplashScreen;
        modemScreen = new MCSplashScreen;
        sessionScreen = new SessionScreen;
        connectScreen = new MCSplashScreen;
        prefScreen = new MCSplashScreen;
    }
    else
    {
        chatWindow = nullptr;
        prefScreen = nullptr;
        connectScreen = nullptr;
        modemScreen = nullptr;
        lanScreen = nullptr;
        serialScreen = nullptr;
        multiplayerScreen = nullptr;
        sessionScreen = nullptr;
    }

    mainScreen = new MCSplashScreen;
    repairScreen = new RepairScreen;
    repairScreen->init();
    briefingScreen = new BriefingScreen;
    briefingScreen->init();
    purchaseScreen = new PurchaseScreen;
    purchaseScreen->init();
    currentScreen = mainScreen;
    logisticsState = 1;

    {
        FitIniFile screenFile;
        openScreenFile(screenFile, "mainScreen", " No Splash Screen FIT File ");
        initSplashScreen(mainScreen, screenFile, " Unable to start splash screen ");
        screenFile.close();
    }

    if (InDemo == 0)
    {
        char userName[0x40];
        {
            FitIniFile screenFile;
            openScreenFile(screenFile, "mpscreen", " No Connection Screen FIT File ");
            initSplashScreen(multiplayerScreen, screenFile, " Unable to start connect screen ");
            screenFile.close();
        }

        {
            FitIniFile screenFile;
            openScreenFile(screenFile, "lanscreen", " No LAN Screen FIT File ");
            MCSplashScreen* screen = lanScreen;
            initSplashScreen(screen, screenFile, " Unable to start lanScreen screen ");
            screenFile.close();
            screen->setEventRoutine(LanScreenHandleEvent);
            auto* players = screenElement<lScrollTextObject>(screen, 3);
            players->setEventRoutine(PlayerListHandleEvent);
            players->fontIndex = 1;
            screenElement<GameList>(screen, 2)->fontIndex = 1;
            auto* nameField = screenElement<lTextObject>(screen, 4);
            auto* gameField = screenElement<lTextObject>(screen, 10);
            auto* playersField = screenElement<lTextObject>(screen, 11);
            gameField->font = medWhiteFont;
            nameField->font = medWhiteFont;
            playersField->font = medWhiteFont;
            nameField->setBackColor(0x10);
            gameField->setBackColor(0x10);
            playersField->setBackColor(0x10);
            nameField->initBuffer(0x10, 0);
            gameField->initBuffer(0x18, 0);
            uint32_t size = 0x3f;
            char* gameName;
            char gameText[0x200];

            if (MyGetUserName(userName, &size) == 0)
            {
                nameField->setStringBuffer(const_cast<char*>("Player"));
                gameName = const_cast<char*>("Game");
            }
            else
            {
                nameField->setStringBuffer(userName);
                char format[0x100];
                cLoadString(thisInstance, 0x377, format, 0xfe);
                std::snprintf(gameText, sizeof(gameText), format, userName);
                gameField->initBuffer(0x18, 0);
                gameName = gameText;
            }

            gameField->setStringBuffer(gameName);
            playersField->initBuffer(2, 3);
            playersField->setStringBuffer(const_cast<char*>("6"));
            auto* joinButton = screenElement<lButton>(screen, 6);
            joinButton->disabled = 1;
            joinButton->draw();
            lanScreen->showBlock(0);
        }

        {
            FitIniFile screenFile;
            openScreenFile(screenFile, "modem", " No modem Screen FIT File ");
            MCSplashScreen* screen = modemScreen;
            initSplashScreen(screen, screenFile, " Unable to start modemScreen screen ");
            screenFile.close();
            screen->setEventRoutine(ModemScreenHandleEvent);
            auto* nameField = screenElement<lTextObject>(screen, 4);
            auto* phoneField = screenElement<lTextObject>(screen, 5);
            nameField->font = medWhiteFont;
            phoneField->font = medWhiteFont;
            nameField->setBackColor(0x10);
            phoneField->setBackColor(0x10);
            nameField->initBuffer(0x10, 0);
            phoneField->initBuffer(0x18, 0);
            uint32_t size = 0x3f;
            nameField->setStringBuffer(MyGetUserName(userName, &size) == 0 ? const_cast<char*>("Player") : userName);
            auto* modems = screenElement<lScrollTextObject>(screen, 10);
            modems->fontIndex = 1;
            modems->highlightColor[0] = 0x14;
            modems->highlightLine[0] = 0;
            modems->setEventRoutine(ModemListHandleEvent);
            screen->showBlock(0);
        }

        {
            FitIniFile screenFile;
            // The serial screen reuses the modem screen's messages.
            openScreenFile(screenFile, "serial", " No modem Screen FIT File ");
            MCSplashScreen* screen = serialScreen;
            initSplashScreen(screen, screenFile, " Unable to start modemScreen screen ");
            screenFile.close();
            lTextObject* nameField = setUpNameField(screen, 4);
            nameField->initBuffer(0x10, 0);
            uint32_t size = 0x3f;
            nameField->setStringBuffer(MyGetUserName(userName, &size) == 0 ? const_cast<char*>("Player") : userName);
            lTextObject* portField = setUpNameField(screen, 5);
            portField->initBuffer(2, 1);
            portField->setEventRoutine(ComPortTextHandleEvent);
            portField->setStringBuffer(const_cast<char*>("1"));
            serialScreen->setEventRoutine(SerialScreenHandleEvent);
        }

        {
            FitIniFile screenFile;
            openScreenFile(screenFile, "readyroom", " No Ready Room Screen FIT File ");
            MCSplashScreen* screen = connectScreen;
            initSplashScreen(screen, screenFile, " Unable to start readyRoomScreen screen ");
            screenFile.close();
            auto* players = screenElement<lScrollTextObject>(screen, 3);
            players->setEventRoutine(ReadyRoomPlayerListHandleEvent);
            auto* goButton = screenElement<lButton>(screen, 2);
            players->fontIndex = 1;
            goButton->disabled = 1;
            goButton->draw();
        }
    }

    FitIniFile loadScreenFile;
    openScreenFile(loadScreenFile, "loadScreen", " No Load Screen FIT File ");
    initSplashScreen(loadScreen, loadScreenFile, " Unable to start load screen ");
    loadScreen->setEventRoutine(LoadSaveScreenHandleEvent);
    FitIniFile saveScreenFile;
    openScreenFile(saveScreenFile, "saveScreen", " No Save Screen FIT File ");
    initSplashScreen(saveScreen, saveScreenFile, " Unable to start save screen ");
    saveScreen->setEventRoutine(LoadSaveScreenHandleEvent);

    if (InDemo == 0)
    {
        FitIniFile prefScreenFile;
        // The preferences screen reuses the save screen's messages.
        openScreenFile(prefScreenFile, "prefScreen", " No Save Screen FIT File ");
        initSplashScreen(prefScreen, prefScreenFile, " Unable to start save screen ");
        prefScreen->setEventRoutine(PrefScreenHandleEvent);
        // Port: the renderer choice.
        AddRendererPreference(prefScreen);
        sessionScreen->init(0, 0, 0x280, 0x1e0, nullptr);
    }

    showLogScreen(0, 0);

    // Under the process ID, as aSystem::init sets it: copies of the game on one machine share the user folder.
    std::snprintf(saveTempPath, sizeof(saveTempPath), "%stemp\\%u\\", savePath, MCPort::ProcessId());
    // Port fix (OB-094): the original allocated a File here, and a FitIniFile after the sort tables, and never used or
    // freed either.

    char line[256];
    File file;
    std::snprintf(line, sizeof(line), "%slogart\\comp.rsp", artPath);
    int32_t result = file.open(line);
    Assert(result == 0, 0, " could not open componant name file ");
    numRangeSorted = 1;

    while (true)
    {
        file.readLine(reinterpret_cast<uint8_t*>(line), 0x28);

        if (file.eof())
        {
            break;
        }

        ++numRangeSorted;
    }

    rangeSortList = new uint32_t[numRangeSorted];
    // Original behaviour: memclear was given the entry count as the byte count; every entry is read below anyway.
    memclear(rangeSortList, numRangeSorted);
    file.seek(0, 0);

    for (int32_t i = 0; i < numRangeSorted; ++i)
    {
        file.readLine(reinterpret_cast<uint8_t*>(line), 0x28);
        rangeSortList[i] = static_cast<uint32_t>(std::atol(line));
    }

    file.close();
    std::snprintf(line, sizeof(line), "%sobjsort.rsp", objectPath);
    result = file.open(line);
    Assert(result == 0, 0, " could not open object sort file ");

    for (int32_t i = 0; i < 0x100; ++i)
    {
        file.readLine(reinterpret_cast<uint8_t*>(line), 0xfe);
        componentSort[i] = static_cast<int32_t>(std::atol(line));
    }

    file.close();

    invBlockPort = newPort("%slogart\\invblock.tga", artPath);
    invTabPorts[0] = nullptr;
    invTabPorts[1] = nullptr;
    invTabPorts[2] = nullptr;
    invTabPorts[3] = nullptr;

    auto* nameTicker = new Ticker;
    nameTicker->init();
    ticker = nameTicker;
    nameTicker->init(3, 3, 0xcd, 1, currentScreen->lport());
    nameTicker->setScreen(currentScreen);
    currentScreen->addChild(nameTicker);
    nameTicker->setFont(medWhiteFont);
    nameTicker->bringToFront(0);
    nameTicker->ShowGUIWindow(1);
    lPort* tickerBack = newPort(0xcd, medWhiteFont->height());
    VFX_pane_wipe(tickerBack->frame(), 0xed);
    nameTicker->setBackPane(tickerBack);
    delete tickerBack;

    purchaseDialog = new PurchaseDlg;
    purchaseDialog->LogDialogBox::init(0xe5, 0xa2, 0xb5, 0x9c);
    screenWindow->addChild(purchaseDialog);
    messageDialog = new ReusableDialog;
    messageDialog->init(0, 0, 4, 4, nullptr);
    screenWindow->addChild(messageDialog);
    questionDialog = new ReusableDialog;
    questionDialog->init(0, 0, 4, 4, nullptr);
    screenWindow->addChild(questionDialog);
    refitDialog = new RefitDialog;
    refitDialog->init(0, 0, 4, 4, nullptr);
    screenWindow->addChild(refitDialog);

    resourceBackPort = newPort(0x3d, 0xc);
    VFX_pane_wipe(resourceBackPort->frame(), 0x10);
    clockBackPort = newPort(0x32, 0xc);
    VFX_pane_wipe(clockBackPort->frame(), 0x10);
    repairBackPort = newPort("%slogart\\lsrupm00.tga", artPath);

    for (int32_t i = 0; i < 0x18; ++i)
    {
        std::snprintf(line, sizeof(line), "%smechrep%02d.shp", artPath, i);
        mechRepShapes[i] = loadShapeFile(file, line, "could not open mechrep shape file", "unexpected mechrep size");
        std::snprintf(line, sizeof(line), "%smi%02d.shp", artPath, i);
        mechIconShapes[i] = loadShapeFile(file, line, "could not open mechicon shape file", "unexpected mechicon size");
    }

    for (int32_t i = 0; i < 0x23; ++i)
    {
        std::snprintf(line, sizeof(line), "%svr1%02d.shp", artPath, i);

        if (file.open(line) == 0)
        {
            vehicleRepShapes[i] = readShapeFile(file, "unexpected vhclrep size");
            std::snprintf(line, sizeof(line), "%svi1%02d.shp", artPath, i);

            if (file.open(line) == 0)
            {
                vehicleIconShapes[i] = readShapeFile(file, "unexpected vhclrep size");
            }
            else
            {
                vehicleIconShapes[i] = nullptr;
            }
        }
        else
        {
            vehicleRepShapes[i] = nullptr;
            vehicleIconShapes[i] = nullptr;
        }
    }

    // Ten remap tables: each maps every colour to 0xff (transparent) except one, which it recolours.
    static constexpr struct
    {
        uint8_t from;
        uint8_t to;
    } lookasideColors[10] = {{0xe8, 0xe8}, {0xe8, 0xf2}, {0xe8, 0xeb}, {0xe8, 0xef}, {0xe8, 0x13},
                             {0xe6, 0xe6}, {0xe6, 0xf1}, {0xe6, 0xf4}, {0xe6, 0xed}, {0xe6, 0x13}};

    for (int32_t i = 0; i < 10; ++i)
    {
        std::memset(shapeLookaside[i], 0xff, sizeof(shapeLookaside[i]));
        shapeLookaside[i][lookasideColors[i].from] = lookasideColors[i].to;
    }

    repairPorts[0] = newPort("%slogart\\lsrupm03.tga", artPath);
    repairPorts[1] = newPort("%slogart\\lsrupm01.tga", artPath);
    repairPorts[2] = newPort("%slogart\\lsrupm04.tga", artPath);
    repairPorts[3] = newPort("%slogart\\lsrupm02.tga", artPath);
    repairPorts[4] = newPort("%slogart\\lsrupm06.tga", artPath);
    repairPorts[5] = newPort("%slogart\\lsrupm07.tga", artPath);
    purchasePorts[0] = newPort("%slogart\\lspcb05.tga", artPath);
    purchasePorts[1] = newPort("%slogart\\lspcb07.tga", artPath);
    purchasePorts[2] = newPort("%slogart\\lspcb06.tga", artPath);
    purchasePorts[3] = newPort("%slogart\\lspcb09.tga", artPath);
    screenButtonPorts[0][0] = newPort("%slogart\\lscbn00.tga", artPath);
    screenButtonPorts[0][1] = newPort("%slogart\\lscbh00.tga", artPath);
    screenButtonPorts[0][2] = newPort("%slogart\\lscbg00.tga", artPath);
    screenButtonPorts[1][0] = newPort("%sbn_exit.tga", artPath);
    screenButtonPorts[1][1] = newPort("%sbh_exit.tga", artPath);
    screenButtonPorts[1][2] = newPort("%sbg_exit.tga", artPath);
    screenButtonPorts[2][0] = newPort("%slogart\\lscbn01.tga", artPath);
    screenButtonPorts[2][1] = newPort("%slogart\\lscbh01.tga", artPath);
    screenButtonPorts[2][2] = newPort("%slogart\\lscbg01.tga", artPath);
    screenButtonPorts[3][0] = newPort("%slogart\\lscbn02.tga", artPath);
    screenButtonPorts[3][1] = newPort("%slogart\\lscbh02.tga", artPath);
    screenButtonPorts[3][2] = newPort("%slogart\\lscbg02.tga", artPath);
    screenButtonPorts[4][0] = newPort("%slogart\\lscbn03.tga", artPath);
    screenButtonPorts[4][1] = newPort("%slogart\\lscbh03.tga", artPath);
    screenButtonPorts[4][2] = newPort("%slogart\\lscbg03.tga", artPath);

    std::snprintf(line, sizeof(line), "%sgamesys.fit", missionPath);
    FitIniFile gameSystemFile;
    result = gameSystemFile.open(line);
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't open gamesys.fit");
    result = gameSystemFile.seekBlock("Warrior");
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't find Warrior block ");
    result = gameSystemFile.readIdFloat("SkillMax", MaxPilotSkill);
    Assert(result == 0, static_cast<uint32_t>(result),
           " Couldn't find SkillMax variable in Warrior block of gamesys.fit ");
    result = gameSystemFile.readIdFloat("SkillMin", MinPilotSkill);
    Assert(result == 0, static_cast<uint32_t>(result),
           " Couldn't find SkillMin variable in Warrior block of gamesys.fit ");
    gameSystemFile.readIdFloatArray("SkillWeightings", SkillWeightings, 4);
    result = gameSystemFile.readIdFloatArray("WarriorRankScale", WarriorRankScale, 4);
    Assert(result == 0, static_cast<uint32_t>(result),
           " Couldn't find WarriorRankScale variable in Warrior block of gamesys.fit ");
    result = gameSystemFile.seekBlock("MultiPlayerColors");
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find Multiplayer Colors block ");
    result = gameSystemFile.readIdLongArray("mPlayerColors", multiPlayerColors, 6);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find multiplayer colors data ");
    gameSystemFile.close();

    readyRoomTicks = 0;

    if (launchedFromLobby == 0 || MPlayer == nullptr || turn != 0)
    {
        logisticsState = 1;
        currentScreen = mainScreen;
    }
    else
    {
        // Started from a lobby: straight to the ready room, whose cancel button quits the game.
        currentScreen = connectScreen;
        logisticsState = 0xe;
        screenElement<lButton>(connectScreen, 1)->callback()->setExec(killTheGame);
    }

    showLogScreen(0, 0);
}

auto Logistics::initializeMultiplayer() -> void
{
    Assert(MPlayer != nullptr, 0, "initializeMultiplayer failed: no MPlayer object.");

    if (multiplayerInitialized != 0)
    {
        return;
    }

    MultiPlayer* multiPlayer = MPlayer;
    std::memset(multiPlayer->playerSessionCheckIn, 0, sizeof(multiPlayer->playerSessionCheckIn));
    multiPlayer->inLogistics = 1;

    if (messageBuffer == nullptr)
    {
        messageBuffer = static_cast<FIMessageHeader*>(logAlloc(500));
    }

    uint32_t teammates[6];
    int32_t numTeammates;
    uint32_t opponents[6];
    int32_t numOpponents;
    sessionScreen->fillDPIDArray(teammates, &numTeammates, 1);
    sessionScreen->fillDPIDArray(opponents, &numOpponents, 0);

    for (int32_t i = 0; i < numTeammates; ++i)
    {
        mpMechLists[0][i] = new LogMechList;
        mpMechLists[0][i]->playerID = teammates[i];
        mpVehicleLists[0][i] = new LogVehicleList;
        mpVehicleLists[0][i]->playerID = teammates[i];
    }

    for (int32_t i = 0; i < numOpponents; ++i)
    {
        mpMechLists[1][i] = new LogMechList;
        mpMechLists[1][i]->playerID = opponents[i];
        mpVehicleLists[1][i] = new LogVehicleList;
        mpVehicleLists[1][i]->playerID = opponents[i];
    }

    File file;
    readNameList(file, "netmechs", "File <netmechs.rsp> not found", netMechNames);
    readNameList(file, "netwars", "File <netwars.rsp> not found", netWarriorNames);
    readNameList(file, "netvhcls", "File <netvehicles.rsp> not found", netVehicleNames);

    int32_t localIndex = -1;

    for (int32_t i = 0; i < numTeammates; ++i)
    {
        if (teammates[i] == MPlayer->sessionManager->myPlayer->id)
        {
            localIndex = i;
            break;
        }
    }

    Assert(localIndex != -1, 0, "Local player not in teammate list.");
    SetupSlotsForMultiplayer(localIndex, numTeammates);
    mpMissionName = static_cast<char*>(logAlloc(0x80));
    mpWarriorList = new LogWarriorList;
    MPlayer->chatCallback = LogisticsChatCallback;

    for (int32_t lance = 0; lance < 3; ++lance)
    {
        for (int32_t slot = 0; slot < 4; ++slot)
        {
            DropSlot* dropSlot = new DropSlot;

            if (dropSlot != nullptr)
            {
                dropSlot->lance = lance;
                dropSlot->slot = slot;
                dropSlot->part = nullptr;
            }

            dropSlots[lance][slot] = dropSlot;
            dropSlot = new DropSlot;

            if (dropSlot != nullptr)
            {
                dropSlot->lance = lance;
                dropSlot->slot = slot;
                dropSlot->part = nullptr;
            }

            opponentDropSlots[lance][slot] = dropSlot;
        }
    }

    const int8_t techBase = MPlayer->homeTeam == 0 ? sessionScreen->team1TechBase : sessionScreen->team2TechBase;
    std::strcpy(purchaseFile, techBase == 1 ? "ispur" : "clanpur");
    multiplayerInitialized = 1;
}

auto Logistics::SetupSlotsForMultiplayer(int playerIndex, int numPlayers) -> void
{
    const int32_t slotsEach = 12 / numPlayers;
    const int32_t first = slotsEach * playerIndex;
    std::memset(localDropSlot, 0, sizeof(localDropSlot));

    for (int32_t slot = first; slot < first + slotsEach; ++slot)
    {
        localDropSlot[slot] = 1;
    }
}

auto Logistics::destroyMultiplayer() -> void
{
    if (multiplayerInitialized == 0)
    {
        return;
    }

    logFree(mpMissionName);
    mpMissionName = nullptr;

    if (MPlayer != nullptr)
    {
        MPlayer->chatCallback = handleAppChat;
    }

    freeNameList(netMechNames);
    freeNameList(netWarriorNames);
    freeNameList(netVehicleNames);

    if (messageBuffer != nullptr)
    {
        logFree(messageBuffer);
        messageBuffer = nullptr;
    }

    for (int32_t player = 0; player < 3; ++player)
    {
        // Original behaviour (OB-095): the third teammate's lists are never freed.
        if (player < 2)
        {
            if (mpMechLists[0][player] != nullptr)
            {
                mpMechLists[0][player]->destroy();
                delete mpMechLists[0][player];
                mpMechLists[0][player] = nullptr;
            }

            if (mpVehicleLists[0][player] != nullptr)
            {
                mpVehicleLists[0][player]->destroy();
                delete mpVehicleLists[0][player];
                mpVehicleLists[0][player] = nullptr;
            }
        }

        if (mpMechLists[1][player] != nullptr)
        {
            mpMechLists[1][player]->destroy();
            delete mpMechLists[1][player];
            mpMechLists[1][player] = nullptr;
        }

        if (mpVehicleLists[1][player] != nullptr)
        {
            mpVehicleLists[1][player]->destroy();
            delete mpVehicleLists[1][player];
            mpVehicleLists[1][player] = nullptr;
        }
    }

    if (mpWarriorList != nullptr)
    {
        mpWarriorList->destroy();
        delete mpWarriorList;
    }

    mpWarriorList = nullptr;
    multiplayerInitialized = 0;
}

auto Logistics::destroy() -> void
{
    if (playerLights != nullptr)
    {
        delete playerLights;
        playerLights = nullptr;
    }

    if (ticker != nullptr)
    {
        delete ticker;
        ticker = nullptr;
    }

    if (chatWindow != nullptr)
    {
        chatWindow->destroy();
        delete chatWindow;
        chatWindow = nullptr;
    }

    if (multiplayerInitialized != 0)
    {
        destroyMultiplayer();
    }

    if (campaignBriefingName != nullptr)
    {
        logFree(campaignBriefingName);
        campaignBriefingName = nullptr;
    }

    if (operationCinema != nullptr)
    {
        logFree(operationCinema);
        operationCinema = nullptr;
    }

    deletePort(repairBackPort);

    for (lPort*& port : repairPorts)
    {
        deletePort(port);
    }

    for (lPort*& port : purchasePorts)
    {
        deletePort(port);
    }

    deletePort(workPort0);
    deletePort(workPort1);

    for (auto& ports : screenButtonPorts)
    {
        for (lPort*& port : ports)
        {
            deletePort(port);
        }
    }

    for (lPort*& port : inventoryIconPorts)
    {
        deletePort(port);
    }

    for (int32_t i = 0; i < 0x18; ++i)
    {
        logFree(mechRepShapes[i]);
        mechRepShapes[i] = nullptr;
        logFree(mechIconShapes[i]);
        mechIconShapes[i] = nullptr;
    }

    for (int32_t i = 0; i < 0x23; ++i)
    {
        logFree(vehicleRepShapes[i]);
        vehicleRepShapes[i] = nullptr;
        logFree(vehicleIconShapes[i]);
        vehicleIconShapes[i] = nullptr;
    }

    deletePort(resourceBackPort);
    deletePort(clockBackPort);

    if (purchaseDialog != nullptr)
    {
        screenWindow->removeChild(purchaseDialog);
        delete purchaseDialog;
        purchaseDialog = nullptr;
    }

    if (messageDialog != nullptr)
    {
        messageDialog->destroy();
        delete messageDialog;
        messageDialog = nullptr;
    }

    if (questionDialog != nullptr)
    {
        questionDialog->destroy();
        delete questionDialog;
        questionDialog = nullptr;
    }

    if (refitDialog != nullptr)
    {
        refitDialog->destroy();
        delete refitDialog;
        refitDialog = nullptr;
    }

    if (missionFileName != nullptr)
    {
        logFree(missionFileName);
        missionFileName = nullptr;
    }

    deletePort(invBlockPort);
    currentScreen = nullptr;

    if (vehicleList != nullptr)
    {
        vehicleList->destroy();
        delete vehicleList;
        vehicleList = nullptr;
    }

    // In multiplayer the force lists stay: the mission reads them.
    if (forceVehicleList != nullptr && MPlayer == nullptr)
    {
        forceVehicleList->destroy();
        delete forceVehicleList;
        forceVehicleList = nullptr;
    }

    if (purMechList != nullptr)
    {
        purMechList->destroy();
        delete purMechList;
        purMechList = nullptr;
    }

    if (purVehicleList != nullptr)
    {
        purVehicleList->destroy();
        delete purVehicleList;
        purVehicleList = nullptr;
    }

    if (purchaseComponents != nullptr)
    {
        purchaseComponents->destroy();
        delete purchaseComponents;
        purchaseComponents = nullptr;
    }

    if (purPilotList != nullptr)
    {
        purPilotList->destroy();
        delete purPilotList;
        purPilotList = nullptr;
    }

    if (componentInventory != nullptr)
    {
        componentInventory->destroy();
        delete componentInventory;
        componentInventory = nullptr;
    }

    if (rangeSortList != nullptr)
    {
        delete[] rangeSortList;
        rangeSortList = nullptr;
    }

    if (warriorList != nullptr)
    {
        warriorList->destroy();
        delete warriorList;
        warriorList = nullptr;
    }

    if (mechList != nullptr)
    {
        mechList->destroy();
        delete mechList;
        mechList = nullptr;
    }

    if (assignedWarriorList != nullptr)
    {
        assignedWarriorList->destroy();
        delete assignedWarriorList;
        assignedWarriorList = nullptr;
    }

    if (forceMechList != nullptr && MPlayer == nullptr)
    {
        forceMechList->destroy();
        delete forceMechList;
        forceMechList = nullptr;
    }

    deletePort(invTabPorts[2]);
    deletePort(invTabPorts[0]);
    deletePort(invTabPorts[1]);
    deletePort(invTabPorts[3]);
    auto deleteScreen = [](auto*& screen)
    {
        if (screen != nullptr)
        {
            delete screen;
            screen = nullptr;
        }
    };

    deleteScreen(briefingScreen);
    deleteScreen(purchaseScreen);
    deleteScreen(repairScreen);
    deleteScreen(mainScreen);

    if (InDemo == 0)
    {
        deleteScreen(multiplayerScreen);
        deleteScreen(lanScreen);
        deleteScreen(modemScreen);
        deleteScreen(serialScreen);
        deleteScreen(connectScreen);
        deleteScreen(sessionScreen);
        deleteScreen(prefScreen);
    }

    deleteScreen(loadScreen);
    deleteScreen(saveScreen);
    application->setCurrentObject(nullptr);
    ClearLogArt();
    delete logisticsHeap;
    logisticsHeap = nullptr;

    if (EmptyFile != nullptr)
    {
        std::free(EmptyFile);
        EmptyFile = nullptr;
    }
}

auto Logistics::showLogScreen(int show, int redraw) -> void
{
    if (redraw != 0)
    {
        char fileName[256];
        std::snprintf(fileName, sizeof(fileName), "%slogart\\lsrupm05.tga", artPath);
        application->activatePaletteFromTGA(fileName);
    }

    currentScreen->ShowGUIWindow(show);
}

auto Logistics::setUpMainScreen(int fromMenu) -> int32_t
{
    if (currentScreen != nullptr)
    {
        showLogScreen(0, 0);
    }

    currentScreen = mainScreen;

    if (fromMenu == 0)
    {
        previousState = logisticsState;
    }

    logisticsState = 1;
    showLogScreen(1, 1);

    if (MPlayer != nullptr)
    {
        if (multiplayerInitialized != 0)
        {
            destroyMultiplayer();
        }

        delete MPlayer;
        MPlayer = nullptr;
        BriefingScreen* briefing = briefingScreen;
        briefing->chatBlinking = 0;

        if (briefing->chatTimerOn != 0)
        {
            application->RemoveTimer(briefing, 5);
        }

        if (purchaseScreen->chatBlinking != 0)
        {
            application->RemoveTimer(purchaseScreen, 7);
        }

        if (repairScreen->chatBlinking != 0)
        {
            application->RemoveTimer(repairScreen, 8);
        }

        briefing->briefingBox = nullptr;

        if (globalLogPtr->purchaseDialog != nullptr)
        {
            globalLogPtr->purchaseDialog->ShowGUIWindow(0);
        }
    }

    if (chatWindow != nullptr)
    {
        chatWindow->reset();
    }

    return 0;
}

auto Logistics::setUpCampaignPurchasing(char* purchaseFileName, PacketFile* file) -> char*
{
    auto* purchasing = new FitIniFile;
    Assert(purchasing != nullptr, 0, " no RAM for scenario file ");
    char text[256];
    std::snprintf(text, sizeof(text), "%s%s.fit", missionPath, purchaseFileName);
    int32_t result = purchasing->open(text);
    Assert(result == 0, 0, " could not open purchasing file ");
    result = purchasing->seekBlock("PurchaseCosts");
    Assert(result == 0, static_cast<uint32_t>(result), "Could not find PurchaseCosts block in purchasing file");
    result = purchasing->readIdLong("Armor", armorCost);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not read Armor in purchasing file");
    result = purchasing->readIdLong("Internal", internalCost);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not read Internal in purchasing file");
    result = purchasing->readIdLong("Engine", engineCost);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not read Engine in purchasing file");
    result = purchasing->readIdFloat("clan", clanCostFactor);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not read clan in purchasing file.");
    result = purchasing->seekBlock("PilotCosts");
    Assert(result == 0, static_cast<uint32_t>(result), "Could not find PilotCosts block in purchasing file");
    result = purchasing->readIdLong("Green", pilotCosts[0]);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not read Green pilot in purchasing file");
    result = purchasing->readIdLong("Regular", pilotCosts[1]);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not read Regular pilot in purchasing file");
    result = purchasing->readIdLong("Veteran", pilotCosts[2]);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not read Veteran pilot in purchasing file");
    result = purchasing->readIdLong("Elite", pilotCosts[3]);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not read Elite pilot in purchasing file.");

    if (campaignBriefingName != nullptr)
    {
        logFree(campaignBriefingName);
        campaignBriefingName = nullptr;
    }

    if (purchasing->seekBlock("CampaignBriefing") == 0)
    {
        campaignBriefingName = static_cast<char*>(logAlloc(0x29));
        result = purchasing->readIdString("Filename", campaignBriefingName, 0x29);
        Assert(result == 0, static_cast<uint32_t>(result), "Could not read Campaign Briefing cinema name");
    }

    std::snprintf(text, sizeof(text), "Mission%d", currentMission);
    purchasing->seekBlock(text);
    auto* missionPurchaseFile = static_cast<char*>(logAlloc(0xfa));
    result = purchasing->readIdString("PurchaseFile", missionPurchaseFile, 0xf9);
    Assert(result == 0, 0, " could not read PurchaseFile in purchasing file");

    if (operationCinema != nullptr)
    {
        logFree(operationCinema);
        operationCinema = nullptr;
    }

    operationCinema = static_cast<char*>(logAlloc(0x29));

    if (purchasing->readIdString("OperationCinema", operationCinema, 0x29) != 0)
    {
        logFree(operationCinema);
        operationCinema = nullptr;
    }

    autoPlayMovie = 0;
    int32_t autoPlay;

    if (purchasing->readIdLong("AutoPlay", autoPlay) == 0 && autoPlay != 0)
    {
        autoPlayMovie = 1;
    }

    if (purchasing->readIdLong("Operation", operation) == 0)
    {
        BriefingScreen* briefing = briefingScreen;
        delete briefing->operationPicture;
        briefing->operationPicture = new lPort;
        Assert(briefing->operationPicture != nullptr, 0, " Not enough memory for opPort ");
        std::snprintf(text, sizeof(text), CurPlanet == 0 ? "%slogart\\lsb_op%d.tga" : "%slogart\\mcxcard%d.tga",
                      artPath, operation);
        briefing->operationPicture->init(text);
    }
    else
    {
        operation = 0;
    }

    purchasing->close();
    delete purchasing;
    setUpPurchasing(file);
    return missionPurchaseFile;
}

namespace
{
    /// <summary>The counts read from a purchase file's Header block.</summary>
    struct PurchaseHeader
    {
        int32_t numMechs;
        int32_t numVehicles;
        int32_t numComponents;
        int32_t numWarriors;
        int32_t numGifts;
    };

    /// <summary>Reads the Header block of an open purchase file.</summary>
    PurchaseHeader readPurchaseHeader(FitIniFile& file, bool gifts)
    {
        PurchaseHeader header{};
        int32_t result = file.seekBlock("Header");
        Assert(result == 0, 0, " could not find header block in purchasing file ");
        result = file.readIdLong("NumMechs", header.numMechs);
        Assert(result == 0, 0, " could not read NumMechs in purchasing file ");
        result = file.readIdLong("NumVehicles", header.numVehicles);
        Assert(result == 0, 0, " could not read NumVehicles in purchasing file ");
        result = file.readIdLong("NumComponants", header.numComponents);
        Assert(result == 0, 0, " could not read NumComponants in purchasing file ");
        result = file.readIdLong("NumWarriors", header.numWarriors);
        Assert(result == 0, 0, " could not read NumWarriors in purchasing file ");

        if (gifts && file.readIdLong("NumGifts", header.numGifts) != 0)
        {
            header.numGifts = 0;
        }

        return header;
    }

    /// <summary>Adds the Gift# blocks' mechs and vehicles (all "pv" profiles) to the player's lists.</summary>
    void readPurchaseGifts(FitIniFile& file, int32_t numGifts, LogMechList* mechs, LogVehicleList* vehicles)
    {
        char text[0x200];
        char fileName[12] = {};
        int32_t numAvailable = 0;

        for (int32_t gift = 0; gift < numGifts; ++gift)
        {
            std::snprintf(text, sizeof(text), "Gift%d", gift);
            int32_t result = file.seekBlock(text);
            Assert(result == 0, 0, " could not find Gift block in purchasing file ");
            char giftType[4] = {};
            result = file.readIdString("GiftType", giftType, 2);
            Assert(result == 0, static_cast<uint32_t>(result), " No gift type ");
            result = file.readIdLong("NumAvailable", numAvailable);

            if (result == 0)
            {
                result = file.readIdString("Filename", fileName, 9);
            }

            Assert(result == 0, 0, "Error reading Gift data ");
            std::snprintf(text, sizeof(text), "pv%s", fileName);
            int required = 0;

            if (file.readIdBoolean("Required", required) != 0)
            {
                required = 0;
            }

            if (giftType[0] == 'V')
            {
                for (int32_t i = 0; i < numAvailable; ++i)
                {
                    vehicles->addVehicle(text, required, 0, 1);
                }
            }
            else if (giftType[0] == 'M')
            {
                for (int32_t i = 0; i < numAvailable; ++i)
                {
                    mechs->addMech(text, required, 1, 1);
                }
            }
        }
    }

    /// <summary>
    /// Adds the Mech# blocks (last first) to the shop. A variant's count and file carry over from the previous block
    /// when the block lacks them, as in the original (the port starts them at 0 and "").
    /// </summary>
    void readPurchaseMechs(FitIniFile& file, int32_t numMechs, PurMechList* mechs)
    {
        char text[0x40];
        char fileA[12] = {};
        char fileJ[12] = {};
        char fileW[12] = {};
        int32_t countA = 0;
        int32_t countJ = 0;
        int32_t countW = 0;

        for (int32_t mech = numMechs - 1; mech >= 0; --mech)
        {
            std::snprintf(text, sizeof(text), "Mech%d", mech);
            const int32_t result = file.seekBlock(text);
            Assert(result == 0, 0, " could not find mech block in purchasing file ");

            if (file.readIdLong("TypeAAvailable", countA) == 0)
            {
                file.readIdString("TypeAFile", fileA, 9);
            }

            if (file.readIdLong("TypeJAvailable", countJ) == 0)
            {
                file.readIdString("TypeJFile", fileJ, 9);
            }

            if (file.readIdLong("TypeWAvailable", countW) == 0)
            {
                file.readIdString("TypeWFile", fileW, 9);
            }

            mechs->addMech(fileA, countA, fileJ, countJ, fileW, countW);
        }
    }

    /// <summary>
    /// Adds the Vehicle# blocks (last first) to the shop. <paramref name="prefix"/>: whether the file name gets "pv"
    /// in front (the multiplayer files; the campaign ones name the profile in full).
    /// </summary>
    void readPurchaseVehicles(FitIniFile& file, int32_t numVehicles, PurVehicleList* vehicles, bool prefix)
    {
        char text[0x200];
        char fileName[12] = {};
        int32_t numAvailable = 0;

        for (int32_t vehicle = numVehicles - 1; vehicle >= 0; --vehicle)
        {
            std::snprintf(text, sizeof(text), "Vehicle%d", vehicle);
            int32_t result = file.seekBlock(text);
            Assert(result == 0, 0, " could not find vehicle block in purchasing file ");
            result = file.readIdLong("NumAvailable", numAvailable);

            if (result == 0)
            {
                result = file.readIdString("Filename", fileName, 9);
            }

            Assert(result == 0, 0, "Error reading Purchasing vehicle data ");

            if (prefix)
            {
                std::snprintf(text, sizeof(text), "pv%s", fileName);
                vehicles->addVehicle(text, numAvailable);
            }
            else
            {
                vehicles->addVehicle(fileName, numAvailable);
            }
        }
    }

    /// <summary>
    /// Adds the Componant# blocks to the shop's component list. <paramref name="blockError"/>: the campaign files'
    /// message has a typo ("omponent") the multiplayer one lacks.
    /// </summary>
    void readPurchaseComponents(FitIniFile& file, int32_t numComponents, InventoryList* components,
                                const char* blockError)
    {
        char text[0x100];

        for (int32_t component = 0; component < numComponents; ++component)
        {
            std::snprintf(text, sizeof(text), "Componant%d", component);
            int32_t result = file.seekBlock(text);
            Assert(result == 0, static_cast<uint32_t>(result), blockError);
            uint8_t masterID = 0;
            result = file.readIdUChar("ComponantID", masterID);
            Assert(result == 0, static_cast<uint32_t>(result), " Could not find Purchasing Component masterID");
            int32_t numAvailable = 0;
            result = file.readIdLong("NumAvailable", numAvailable);
            Assert(result == 0, static_cast<uint32_t>(result), " Could not find Purchasing Component Num Available");
            _LogInventoryStat* stat =
                components->createStat(static_cast<uint8_t>(component), 0, 0, 0, static_cast<int16_t>(numAvailable),
                                       static_cast<int16_t>(numAvailable), 0xff);
            // The widgets argument is the block number (a register the compiler left on the stack); it is never -1,
            // so every new item gets its widgets.
            components->addItem(masterID, stat, component);
            components->loadDescription(components->getIndexFromMasterID(masterID), nullptr);
        }
    }

    /// <summary>
    /// Reads Warrior# block <paramref name="warrior"/>'s Profile (and Status when <paramref name="status"/> is set)
    /// and opens the profile's General block.
    /// </summary>
    void readPurchaseWarrior(FitIniFile& file, int32_t warrior, char* profile, int32_t* status, FitIniFile& pilotFile)
    {
        char text[0x40];
        std::snprintf(text, sizeof(text), "Warrior%d", warrior);
        int32_t result = file.seekBlock(text);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Purchasing Warrior block");
        result = file.readIdString("Profile", profile, 0x7f);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Purchasing Warrior profile");

        if (status != nullptr)
        {
            result = file.readIdLong("Status", *status);
            Assert(result == 0, static_cast<uint32_t>(result), " Could not find Purchasing Warrior Status");
        }

        FullPathFileName fileName;
        fileName.init(warriorPath, profile, ".fit");
        result = pilotFile.open(fileName);
        Assert(result == 0, static_cast<uint32_t>(result), " could not open Purchasing Pilot profile file ");
        result = pilotFile.seekBlock("General");
        Assert(result == 0, static_cast<uint32_t>(result), " Could find General Block in PIlot file ");
    }

    /// <summary>
    /// Replaces the shop's lists (mechs, vehicles, pilots, components) with new, empty ones.
    /// </summary>
    void resetShop(Logistics* logistics)
    {
        if (logistics->purMechList != nullptr)
        {
            logistics->purMechList->destroy();
            delete logistics->purMechList;
        }

        logistics->purMechList = new PurMechList;

        if (logistics->purVehicleList != nullptr)
        {
            logistics->purVehicleList->destroy();
            delete logistics->purVehicleList;
        }

        logistics->purVehicleList = new PurVehicleList;

        if (logistics->purPilotList != nullptr)
        {
            logistics->purPilotList->destroy();
            delete logistics->purPilotList;
        }

        logistics->purPilotList = new PurPilotList;

        if (logistics->purchaseComponents != nullptr)
        {
            logistics->purchaseComponents->destroy();
            delete logistics->purchaseComponents;
        }

        logistics->purchaseComponents = new InventoryList;
        logistics->purMechList->init();
        logistics->purVehicleList->init();
        logistics->purPilotList->first = nullptr;
        logistics->purPilotList->count = 0;
    }

    /// <summary>
    /// A pilot's status in the shop from the player's own copy of it (matched by DescIndex): 3 sold, 1 alive, 2 dead;
    /// -1 when the player has none. The last match wins.
    /// </summary>
    int32_t ownPilotStatus(LogWarriorList* list, int32_t count, int32_t descIndex, int32_t status)
    {
        for (int32_t i = 0; i < count; ++i)
        {
            LogWarrior* warrior = nullptr;
            list->getWarriorInfo(i, warrior);

            if (warrior->descIndex == descIndex)
            {
                if (warrior->sold != 0)
                {
                    status = 3;
                }
                else
                {
                    status = 0.0f < warrior->health ? 1 : 2;
                }
            }
        }

        return status;
    }
}

auto Logistics::setUpMPPurchasing(char* purchaseFileName) -> void
{
    auto* purchasing = new FitIniFile;
    resetShop(this);
    PurMechList* mechs = purMechList;
    PurVehicleList* vehicles = purVehicleList;
    PurPilotList* pilots = purPilotList;
    InventoryList* components = purchaseComponents;

    char text[0x200];
    std::snprintf(text, sizeof(text), "%s%s.fit", missionPath, purchaseFileName);
    const int32_t result = purchasing->open(text);
    Assert(result == 0, 0, " could not open mission purchasing file ");
    const PurchaseHeader header = readPurchaseHeader(*purchasing, true);
    readPurchaseGifts(*purchasing, header.numGifts, mechList, vehicleList);
    readPurchaseMechs(*purchasing, header.numMechs, mechs);
    readPurchaseVehicles(*purchasing, header.numVehicles, vehicles, true);
    readPurchaseComponents(*purchasing, header.numComponents, components, " Could not find Purchasing Component block");
    LogWarriorList* warriors = warriorList;

    for (int32_t warrior = 0; warrior < header.numWarriors; ++warrior)
    {
        char profile[0x100];
        FitIniFile pilotFile;
        readPurchaseWarrior(*purchasing, warrior, profile, nullptr, pilotFile);
        char callsign[0x100];
        const int32_t callsignResult = pilotFile.readIdString("Callsign", callsign, 0xff);
        Assert(callsignResult == 0, static_cast<uint32_t>(callsignResult),
               " Could not find Callsign in General Block ");
        pilotFile.close();

        // Only pilots the player does not already have are for hire.
        if (warriors->exists(callsign) == 0 && assignedWarriorList->exists(callsign) == 0)
        {
            pilots->addPilot(profile, 0);
        }
    }

    purchasing->close();
    delete purchasing;
}

auto Logistics::setUpPurchasing(PacketFile* file) -> void
{
    auto* purchasing = new FitIniFile;
    resetShop(this);
    PurMechList* mechs = purMechList;
    PurVehicleList* vehicles = purVehicleList;
    PurPilotList* pilots = purPilotList;
    InventoryList* components = purchaseComponents;

    // The shop is the campaign file's last packet.
    file->seekPacket(file->getNumPackets() - 1);
    const int32_t size = file->getPacketSize();
    Assert(size > 0, static_cast<uint32_t>(size), " Bad Purchase Data in Campaign File ");
    const int32_t result = purchasing->open(file, static_cast<uint32_t>(size), 0x32);
    Assert(result == 0, 0, " could not open mission purchasing file ");
    const PurchaseHeader header = readPurchaseHeader(*purchasing, true);
    readPurchaseGifts(*purchasing, header.numGifts, mechList, vehicleList);
    readPurchaseMechs(*purchasing, header.numMechs, mechs);
    readPurchaseVehicles(*purchasing, header.numVehicles, vehicles, false);
    readPurchaseComponents(*purchasing, header.numComponents, components, " Could not find Purchasing omponent block");
    LogWarriorList* warriors = warriorList;

    for (int32_t warrior = 0; warrior < header.numWarriors; ++warrior)
    {
        char profile[0x100];
        int32_t status = 0;
        FitIniFile pilotFile;
        readPurchaseWarrior(*purchasing, warrior, profile, &status, pilotFile);
        int32_t descIndex = 0;
        const int32_t descResult = pilotFile.readIdLong("DescIndex", descIndex);
        Assert(descResult == 0, static_cast<uint32_t>(descResult), " Could not find DescIndex in General Block ");
        pilotFile.close();
        // A pilot the player has (or had) shows as sold, alive or dead; the pilot lists are read through the
        // mission's logistics object, which is this one.
        int32_t ownStatus = ownPilotStatus(mission->logistics->warriorList, warriors->numWarriors, descIndex, -1);

        if (ownStatus == -1)
        {
            ownStatus = ownPilotStatus(mission->logistics->assignedWarriorList, assignedWarriorList->numWarriors,
                                       descIndex, -1);
        }

        pilots->addPilot(profile, ownStatus != -1 ? ownStatus : status);
    }

    purchasing->close();
    delete purchasing;
}

auto Logistics::setUpOldPurchasing(char* purchaseFileName) -> void
{
    auto* purchasing = new FitIniFile;
    char text[0x200];
    std::snprintf(text, sizeof(text), "%s%s.fit", missionPath, purchaseFileName);
    int32_t result = purchasing->open(text);
    Assert(result == 0, 0, " could not open mission purchasing file ");
    const PurchaseHeader header = readPurchaseHeader(*purchasing, false);

    // Changes to the shop: the counts are added to what is there.
    char fileA[12] = {};
    int32_t countA = 0;
    int32_t countJ = 0;
    int32_t countW = 0;
    char unusedFile[12] = {};

    for (int32_t mech = header.numMechs - 1; mech >= 0; --mech)
    {
        std::snprintf(text, sizeof(text), "Mech%d", mech);
        result = purchasing->seekBlock(text);
        Assert(result == 0, 0, " could not find mech block in purchasing file ");

        if (purchasing->readIdLong("TypeAAvailable", countA) == 0)
        {
            purchasing->readIdString("TypeAFile", fileA, 9);
        }

        if (purchasing->readIdLong("TypeJAvailable", countJ) == 0)
        {
            purchasing->readIdString("TypeJFile", unusedFile, 9);
        }

        if (purchasing->readIdLong("TypeWAvailable", countW) == 0)
        {
            purchasing->readIdString("TypeWFile", unusedFile, 9);
        }

        purMechList->modMech(fileA, countA, countJ, countW);
    }

    char fileName[12] = {};
    int32_t numAvailable = 0;

    for (int32_t vehicle = header.numVehicles - 1; vehicle >= 0; --vehicle)
    {
        std::snprintf(text, sizeof(text), "Vehicle%d", vehicle);
        result = purchasing->seekBlock(text);
        Assert(result == 0, 0, " could not find vehicle block in purchasing file ");
        result = purchasing->readIdLong("NumAvailable", numAvailable);

        if (result == 0)
        {
            result = purchasing->readIdString("Filename", fileName, 9);
        }

        Assert(result == 0, 0, "Error reading Purchasing vehicle data ");
        std::snprintf(text, sizeof(text), "pv%s", fileName);
        purVehicleList->modVehicle(text, numAvailable);
    }

    InventoryList* components = purchaseComponents;

    for (int32_t component = 0; component < header.numComponents; ++component)
    {
        std::snprintf(text, sizeof(text), "Componant%d", component);
        result = purchasing->seekBlock(text);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Purchasing omponent block");
        uint8_t masterID = 0;
        result = purchasing->readIdUChar("ComponantID", masterID);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Purchasing Component masterID");
        int32_t count = 0;
        result = purchasing->readIdLong("NumAvailable", count);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Purchasing Component Num Available");
        components->addCountToItem(count, masterID);
    }

    PurPilotList* pilots = purPilotList;

    for (int32_t warrior = 0; warrior < header.numWarriors; ++warrior)
    {
        char profile[0x100];
        int32_t status = -1;
        FitIniFile pilotFile;
        readPurchaseWarrior(*purchasing, warrior, profile, &status, pilotFile);
        int32_t descIndex = 0;
        result = pilotFile.readIdLong("DescIndex", descIndex);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find DescIndex in General Block ");
        pilotFile.close();

        // Status 4 takes a pilot for hire off the shop; status 0 puts one back.
        for (int32_t i = 0; i < pilots->count; ++i)
        {
            PurPilotData* pilot = nullptr;
            pilots->getPilotInfo(i, pilot);

            if (pilot->descIndex == descIndex)
            {
                if (pilot->status == 0 && status == 4)
                {
                    pilots->setPilotStatus(descIndex, 4);
                }

                if (pilot->status == 4 && status == 0)
                {
                    pilots->setPilotStatus(descIndex, 0);
                }
            }
        }
    }

    purchasing->close();
    delete purchasing;
}

namespace
{
    /// <summary>Moves the name ticker onto <paramref name="screen"/>, at its top left.</summary>
    void moveTicker(Ticker* ticker, lObject* screen)
    {
        if (ticker->parent != nullptr)
        {
            ticker->parent->removeChild(ticker);
        }

        screen->addChild(ticker);
        ticker->setPort(screen->lport());
        ticker->setScreen(screen);
        ticker->setPos(3, 3);
    }

    /// <summary>Moves the multiplayer ready lights onto <paramref name="screen"/>, in front, and draws them.</summary>
    void moveLights(MPPlayerLights* lights, lObject* screen)
    {
        lights->parent->removeChild(lights);
        screen->addChild(lights);
        lights->setDepth(100);
        lights->draw();
    }

    /// <summary>
    /// Draws what <paramref name="pane"/> shows into <paramref name="dest"/> for a screen change: its background
    /// copy at (<paramref name="backX"/>, 1), its scrolled contents (through <paramref name="scratch"/>) at (0, 1)
    /// and its slider at the right edge.
    /// </summary>
    void drawPaneForTransition(ScrollPane* pane, lPort* scratch, lPort* dest, int32_t backX)
    {
        if (pane->backgroundCopy != nullptr)
        {
            pane->backgroundCopy->copyTo(dest->frame(), backX, 1, 1);
        }

        VFX_pane_wipe(scratch->frame(), 0xff);
        pane->DrawContentTo(scratch->frame(), 0, 0);
        scratch->copyTo(dest->frame(), 0, 1, 1);
        pane->DrawSliderColumn(dest->frame(), dest->width() - 0xe, 1, true);
    }

    /// <summary>A scratch port the size of the purchase screen's unit pane (every screen's pane is that size).</summary>
    lPort* newPaneScratch(ScrollPane* pane)
    {
        return newPort(pane->width(), pane->height());
    }
}

auto Logistics::setUpPurchaseScreen(int animate) -> int32_t
{
    BriefingScreen* briefing = briefingScreen;
    briefing->StopSmackerMovies();
    application->SetCurrentCursor(static_cast<CursorType>(0));

    if (MPlayer != nullptr)
    {
        if (briefing->chatBlinking == 0)
        {
            if (purchaseScreen->chatBlinking != 0)
            {
                application->RemoveTimer(purchaseScreen, 7);
                // Original behaviour (OB-096): clears the repair screen's flag instead of the purchase screen's, so
                // the purchase screen's chat button does not blink again until the flag is cleared elsewhere.
                repairScreen->chatBlinking = 0;
            }
        }
        else if (purchaseScreen->chatBlinking == 0)
        {
            application->AddTimer(purchaseScreen, 7, 0xfa, 0, 0, 0);
            purchaseScreen->chatBlinking = 1;
        }
    }

    lObject* previous = currentScreen;

    if (previous != nullptr)
    {
        showLogScreen(0, 0);
    }

    moveTicker(ticker, purchaseScreen);
    currentScreen = purchaseScreen;
    logisticsState = 2;
    purchaseScreen->drawBackground();
    drawScreenButtons();
    PurchaseScreen* screen = purchaseScreen;

    switch (currentInvTab)
    {
        case 0:
        {
            screen->setUpMechInv(1, 1);
            screen->setUpMechPurchase();
            break;
        }
        case 1:
        {
            screen->setUpPilotInv(1, 1);
            screen->setUpPilotPurchase();
            break;
        }
        case 2:
        {
            screen->setUpCompInv(1, 1);
            screen->setUpCompPurchase();
            break;
        }
        case 3:
        {
            screen->setUpVhclInv(1, 1);
            screen->setUpVehiclePurchase();
            break;
        }
    }

    if (MPlayer != nullptr)
    {
        moveLights(playerLights, purchaseScreen);
    }

    showLogScreen(1, previous == repairScreen || previous == briefingScreen ? 0 : 1);

    if (animate != 0)
    {
        VFX_pane_wipe(workPort0->frame(), 0x10);
        VFX_pane_wipe(workPort1->frame(), 0x10);
        ScrollPane* pane = purchaseScreen->unitPane;
        lPort* scratch = newPaneScratch(pane);
        drawPaneForTransition(pane, scratch, workPort0, 0);
        int direction;

        if (previous == repairScreen)
        {
            drawPaneForTransition(repairScreen->unitPane, scratch, workPort1, 0);
            direction = 1;
        }
        else
        {
            lPort* look = briefingScreen->NewLookPicture();
            VFX_pane_copy(look->frame(), 0xd3, 0x10, workPort1->frame(), 0, 0, -1);
            delete look;
            direction = 0;
        }

        delete scratch;
        purchaseScreen->unitPane->ShowGUIWindow(0);
        transition(workPort1, workPort0, direction);
        purchaseScreen->unitPane->ShowGUIWindow(1);
    }

    return 0;
}

auto Logistics::drawScreenButtons() -> void
{
    lObject* screen = currentScreen;

    if (screen != briefingScreen && screen != purchaseScreen && screen != repairScreen)
    {
        return;
    }

    // (The original also made and freed an unused lPort here.)
    // Button 0 is the main menu in single player, exit in multiplayer; the current screen's button is grayed. The
    // screen keeps the faces (it painted them into its picture).
    LogScreenChrome* chrome = screen->Chrome();
    chrome->buttonFaces[0] = MPlayer == nullptr ? screenButtonPorts[0][0] : screenButtonPorts[1][0];
    chrome->buttonFaces[1] = screenButtonPorts[2][0];
    chrome->buttonFaces[2] = screenButtonPorts[3][0];
    chrome->buttonFaces[3] = screenButtonPorts[4][0];

    if (screen == briefingScreen)
    {
        chrome->buttonFaces[1] = screenButtonPorts[2][2];
    }
    else if (screen == purchaseScreen)
    {
        chrome->buttonFaces[2] = screenButtonPorts[3][2];
    }
    else
    {
        chrome->buttonFaces[3] = screenButtonPorts[4][2];
    }

    for (lPort*& overlay : chrome->buttonOverlays)
    {
        overlay = nullptr;
    }
}

auto Logistics::litScreenButton(lObject* screen, int32_t button, lPort* picture) -> void
{
    if (LogScreenChrome* chrome = screen->Chrome(); chrome != nullptr)
    {
        chrome->buttonOverlays[button] = picture;
    }
}

auto Logistics::drawScreenChrome(lObject* screen, _pane* target) -> void
{
    LogScreenChrome* chrome = screen->Chrome();

    if (chrome->lightsBackShown)
    {
        if (lPort* back = logArtf("%slogart\\lsc_p0.tga", artPath))
        {
            back->copyTo(target, 0xd3, 0, 0);
        }
    }

    for (int32_t button = 0; button < 4; button++)
    {
        const int32_t top = 0x10 + button * 0x12;

        if (chrome->buttonFaces[button] != nullptr)
        {
            chrome->buttonFaces[button]->copyTo(target, 2, top, 0);
        }

        if (chrome->buttonOverlays[button] != nullptr)
        {
            chrome->buttonOverlays[button]->copyTo(target, 2, top, -1);
        }
    }

    if (ticker != nullptr)
    {
        ticker->drawPainted(*chrome, target);
    }

    if (chrome->resourceShown)
    {
        VFX_pane_copy(resourceBackPort->frame(), 0, 0, target, 0x209, 2, -1);
        auto* bytes = reinterpret_cast<uint8_t*>(chrome->resourceText);
        const int32_t textWidth = medWhiteFont->width(bytes);
        medWhiteFont->writeString(target, 0x244 - textWidth, 4, bytes, -1);
    }

    if (chrome->clockShown)
    {
        VFX_pane_copy(clockBackPort->frame(), 0, 0, target, 0x24c, 2, -1);
        medWhiteFont->writeString(target, 0x254, 4, reinterpret_cast<uint8_t*>(chrome->clockText), -1);
    }
}

auto Logistics::setUpBriefingScreen(int animate) -> int32_t
{
    application->SetCurrentCursor(static_cast<CursorType>(0));
    lObject* previous = currentScreen;

    if (previous != nullptr)
    {
        showLogScreen(0, 0);
    }

    BriefingScreen* briefing = briefingScreen;
    moveTicker(ticker, briefing);
    briefing->setUpDeploy();

    // Blank the local player's empty drop slots, then the text area under them (the screen draws them each frame).
    for (int32_t lance = 0; lance < 3; ++lance)
    {
        for (int32_t slot = 0; slot < 4; ++slot)
        {
            const int32_t index = lance * 4 + slot;

            if (localDropSlot[index] != 0 && deploySlots[lance][slot].unit == deploySlots[lance][slot].vehicle)
            {
                briefing->CoverSlot(index, BriefingScreen::SlotCover::Blank);
            }
        }
    }

    briefing->BlankBox();

    currentScreen = briefing;
    logisticsState = 3;
    showLogScreen(1, previous == repairScreen || previous == purchaseScreen ? 0 : 1);
    briefing = briefingScreen;
    briefing->movieStarted = 0;
    briefing->setUpMission();
    briefing->missionPane->setScrollPos(0.0f);
    briefing->deployPane->setScrollPos(0.0f);
    briefing->calcTonnages();
    drawScreenButtons();

    if (MPlayer == nullptr)
    {
        chatWindow->ShowGUIWindow(0);
    }
    else
    {
        LogChatWindow* chat = chatWindow;
        briefing = briefingScreen;

        if (chat->parent != briefing)
        {
            chat->resize(0xe7);

            if (chat->parent != nullptr)
            {
                chat->parent->removeChild(chat);
            }

            briefing->addChild(chat);
            chat->moveTo(2, 0x65, 0);
        }

        moveLights(playerLights, briefing);

        if (briefing->chatBlinking != 0 && briefing->chatTimerOn == 0)
        {
            application->AddTimer(briefing, 5, 500, 0, 0, 0);
            briefing->chatTimerOn = 1;
        }

        briefing->setUpOperation();
    }

    // Show the briefing box of the first unit in the deploy pane.
    briefing = briefingScreen;

    if (briefing->briefingBox != nullptr)
    {
        briefing->removeChild(briefing->briefingBox);
        briefing->briefingBox = nullptr;
    }

    ScrollPane* deployPane = briefing->deployPane;

    if (deployPane->numberOfChildren() != 0)
    {
        auto* block = static_cast<MechBriefBlock*>(deployPane->child(0));
        BriefingBox* box = block->mech != nullptr ? block->mech->briefingBox : block->vehicle->briefingBox;
        briefing->addChild(box);
        briefing->briefingBox = box;
        box->drawBackground();
    }

    if (animate != 0)
    {
        lPort* look = briefing->NewLookPicture();
        VFX_pane_copy(look->frame(), 0xd3, 0x10, workPort0->frame(), 0, 0, -1);
        delete look;
        lPort* from = workPort1;
        VFX_pane_wipe(from->frame(), 0x10);
        lPort* scratch = newPaneScratch(purchaseScreen->unitPane);
        VFX_pane_wipe(scratch->frame(), 0xff);
        ScrollPane* pane = previous == repairScreen ? repairScreen->unitPane : purchaseScreen->unitPane;
        drawPaneForTransition(pane, scratch, from, 1);
        delete scratch;
        transition(from, workPort0, 1);
    }

    return 0;
}

auto Logistics::setUpSessionScreen() -> int32_t
{
    if (multiplayerInitialized != 0)
    {
        destroyMultiplayer();
    }

    currentScreen->ShowGUIWindow(0);
    currentScreen = sessionScreen;
    showLogScreen(1, 1);
    logisticsState = 8;
    sessionScreen->activate(0);
    return 0;
}

auto Logistics::setUpRepairScreen(int animate) -> int32_t
{
    BriefingScreen* briefing = briefingScreen;
    briefing->StopSmackerMovies();
    application->SetCurrentCursor(static_cast<CursorType>(0));

    if (MPlayer != nullptr)
    {
        RepairScreen* repair = repairScreen;

        if (briefing->chatBlinking == 0)
        {
            if (repair->chatBlinking != 0)
            {
                application->RemoveTimer(repair, 8);
                repair->chatBlinking = 0;
            }
        }
        else if (repair->chatBlinking == 0)
        {
            application->AddTimer(repair, 8, 0xfa, 0, 0, 0);
            repair->chatBlinking = 1;
        }
    }

    lObject* previous = currentScreen;

    if (previous != nullptr)
    {
        showLogScreen(0, 0);
    }

    moveTicker(ticker, repairScreen);
    currentScreen = repairScreen;
    logisticsState = 4;
    repairScreen->drawBackground();
    drawScreenButtons();

    switch (currentInvTab)
    {
        case 0:
            repairScreen->setUpMechInv(1, 1);
            break;
        case 1:
            repairScreen->setUpPilotInv(1, 1);
            break;
        case 2:
            repairScreen->setUpCompInv(1, 1);
            break;
        case 3:
            repairScreen->setUpVhclInv(1, 1);
            break;
    }

    RepairScreen* repair = repairScreen;

    if (repair->selectedMech == nullptr && repair->selectedVehicle == nullptr)
    {
        if (forceMechList != nullptr)
        {
            repair->selectMech(forceMechList->mechs);
        }
        else if (forceVehicleList != nullptr)
        {
            repair->selectVehicle(forceVehicleList->vehicles);
        }
    }

    if (MPlayer != nullptr)
    {
        moveLights(playerLights, repair);
    }

    showLogScreen(1, previous == purchaseScreen || previous == briefingScreen ? 0 : 1);

    if (animate != 0)
    {
        VFX_pane_wipe(workPort0->frame(), 0x10);
        VFX_pane_wipe(workPort1->frame(), 0x10);
        lPort* scratch = newPaneScratch(purchaseScreen->unitPane);
        VFX_pane_wipe(scratch->frame(), 0xff);
        drawPaneForTransition(repairScreen->unitPane, scratch, workPort0, 1);
        lPort* to = workPort0;

        if (previous == purchaseScreen)
        {
            drawPaneForTransition(purchaseScreen->unitPane, scratch, workPort1, 0);
        }
        else
        {
            lPort* look = briefingScreen->NewLookPicture();
            VFX_pane_copy(look->frame(), 0xd3, 0x10, workPort1->frame(), 0, 0, -1);
            delete look;
        }

        repairScreen->unitPane->ShowGUIWindow(0);
        delete scratch;
        transition(workPort1, to, 0);
        repairScreen->unitPane->ShowGUIWindow(1);
    }

    return 0;
}

auto Logistics::loadQuickStart(FitIniFile* file) -> void
{
    const int32_t homeTeam = MPlayer->homeTeam;
    curDeployTonnage = 0;

    if (file->seekBlock("HammerDown1") == 0)
    {
        hammerDown = 1;
    }

    char text[0x100];
    std::snprintf(text, sizeof(text), "Side%dUnits", homeTeam != 1 ? 1 : 0);
    int32_t result = file->seekBlock(text);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not find SideUnit block in quickstart");

    for (int32_t slotIndex = 0; slotIndex < 12; ++slotIndex)
    {
        const int32_t lance = slotIndex / 4;
        const int32_t slot = slotIndex % 4;

        if (localDropSlot[slotIndex] == 0)
        {
            continue;
        }

        uint32_t value = 0;
        std::snprintf(text, sizeof(text), "Slot%dUnitData", slotIndex);
        result = file->readIdULong(text, value);
        Assert(result == 0, static_cast<uint32_t>(result), "could not read unitData in quickstart");

        if (value == 0)
        {
            continue;
        }

        std::snprintf(text, sizeof(text), "Slot%dType", slotIndex);
        result = file->readIdULong(text, value);
        Assert(result == 0, static_cast<uint32_t>(result), "could not read unitType in quickstart");
        std::snprintf(text, sizeof(text), "Slot%dUnitProfile", slotIndex);
        char profile[0x100];

        if (value < 3)
        {
            // A mech: it goes at the head of the force, so every other mech's pilot index moves up one.
            LogMechList* mechs = forceMechList;

            for (LogMech* other = mechs->mechs; other != nullptr; other = other->next)
            {
                ++other->pilotIndex;
            }

            result = file->readIdString(text, profile, 0xfe);
            Assert(result == 0, static_cast<uint32_t>(result), "could not read mech profile string in quickstart");
            LogMech* mech = mechs->addMech(profile, 0, 0, 1);
            mech->assigned = 1;

            for (int32_t other = 0; other < 12; ++other)
            {
                if (deploySlots[other / 4][other % 4].unit >= 0)
                {
                    ++deploySlots[other / 4][other % 4].unit;
                }
            }

            std::snprintf(text, sizeof(text), "Slot%dPilotProfile", slotIndex);
            result = file->readIdString(text, profile, 0xfe);
            Assert(result == 0, static_cast<uint32_t>(result), "could not read pilot profile string in quickstart");
            LogWarriorList* warriors = assignedWarriorList;
            warriors->addWarrior(profile, 0);
            LogWarrior* warrior = warriors->warriors;
            warrior->assigned = 1;
            setPilot(0, 0);
            const double tonnage = static_cast<double>(curDeployTonnage) + mech->curTonnage;

            if (hammerDown != 0 || tonnage <= static_cast<double>(maxDeployTonnage))
            {
                curDeployTonnage = static_cast<int32_t>(tonnage);
                SendAddMechMessage(mech, lance, slot);
                warrior->dropLance = lance;
                warrior->dropSlot = slot;
                deploySlots[lance][slot].unit = 0;
                mech->deployed = 1;
                warrior->deployed = 1;
                auto* block = new MechBriefBlock;
                BriefingScreen* briefing = briefingScreen;
                mech->briefBlock = block;
                block->init(mech, briefing, briefing->slotRects[slotIndex].left, briefing->slotRects[slotIndex].top);
            }
        }
        else
        {
            result = file->readIdString(text, profile, 0xfe);
            Assert(result == 0, static_cast<uint32_t>(result), "could not read vehicle profile string in quickstart");
            LogVehicle* vehicle = forceVehicleList->addVehicle(profile, 0, 0, 1);
            vehicle->assigned = 1;

            for (int32_t other = 0; other < 12; ++other)
            {
                if (deploySlots[other / 4][other % 4].vehicle >= 0)
                {
                    ++deploySlots[other / 4][other % 4].vehicle;
                }
            }

            const double tonnage = static_cast<double>(curDeployTonnage) + vehicle->curTonnage;

            if (hammerDown != 0 || tonnage <= static_cast<double>(maxDeployTonnage))
            {
                curDeployTonnage = static_cast<int32_t>(tonnage);
                SendAddVehicleMessage(vehicle, lance, slot);
                vehicle->deployed = 1;
                deploySlots[lance][slot].vehicle = 0;
                auto* block = new MechBriefBlock;
                BriefingScreen* briefing = briefingScreen;
                vehicle->briefBlock = block;
                block->init(vehicle, briefing, briefing->slotRects[slotIndex].left, briefing->slotRects[slotIndex].top);
            }
        }
    }

    int32_t index = 0;

    for (LogMech* mech = forceMechList->mechs; mech != nullptr; mech = mech->next)
    {
        mech->repairBlock->slotIndex = index++;
    }

    for (LogVehicle* vehicle = forceVehicleList->vehicles; vehicle != nullptr; vehicle = vehicle->next)
    {
        vehicle->repairBlock->slotIndex = index++;
    }
}

auto Logistics::saveCampaign(char* fileName) -> int32_t
{
    // The original called the bridge with a stack address as this (it has no fields).
    MissionLogisticsBridge bridge;
    return bridge.logisticsSaveGame(fileName);
}

auto Logistics::loadCampaign(char* campaignFile, char* saveFile, int newCampaign, int loadForce) -> int32_t
{
    // The parameter names follow the original's use: campaignFile is the save's name and saveFile its extension.
    PacketFile packetFile;
    FitIniFile file;
    int quickStart = 0;
    briefingScreen->buttonsLocked = 0;
    char text[0x100];
    std::snprintf(text, sizeof(text), "%slogart\\lsrupm05.tga", artPath);
    application->activatePaletteFromTGA(text);

    // Start from empty lists and inventories.
    mechList->destroy();
    forceMechList->destroy();
    warriorList->destroy();
    assignedWarriorList->destroy();
    vehicleList->destroy();
    forceVehicleList->destroy();

    if (componentInventory != nullptr)
    {
        componentInventory->destroy();
        delete componentInventory;
    }

    componentInventory = new InventoryList;

    if (purchaseComponents != nullptr)
    {
        purchaseComponents->destroy();
        delete purchaseComponents;
    }

    purchaseComponents = new InventoryList;

    if (purMechList != nullptr)
    {
        purMechList->destroy();
    }

    if (purVehicleList != nullptr)
    {
        purVehicleList->destroy();
    }

    if (purPilotList != nullptr)
    {
        purPilotList->destroy();
    }

    for (auto& lance : deploySlots)
    {
        for (DeploySlot& slot : lance)
        {
            slot.unit = -1;
            slot.vehicle = -1;
        }
    }

    FullPathFileName path;
    path.init(savePath, campaignFile, saveFile);
    int32_t result = packetFile.open(path);
    Assert(result == 0, 0, " campaign file NOT Valid! ", nullptr);
    result = packetFile.seekPacket(0);
    Assert(result == 0, 0, " could not find initial campaign file ", nullptr);
    result = file.open(&packetFile, packetFile.getPacketSize());
    Assert(result == 0, 0, " could not open initial campaign file ", nullptr);

    if (newCampaign == 0)
    {
        // The planet picks the master mission file (Solo play names it after the save).
        if (file.seekBlock("Planet") == 0)
        {
            result = file.readIdLong("Setting", CurPlanet);
            Assert(result == 0, static_cast<uint32_t>(result), " could not find Setting in Planet Block ", nullptr);
        }
        else
        {
            CurPlanet = 0;
        }

        if (Solo == 0)
        {
            std::strcpy(missionName, CurPlanet == 0 ? "mechcmdr1" : "xmechcmdr1");
        }
        else
        {
            std::snprintf(missionName, sizeof(missionName), "campaign%s", campaignFile);
        }

        mission->initAgain(missionName);
    }

    result = file.seekBlock("General");
    Assert(result == 0, 0, " could not find General Block in campaign file ", nullptr);

    if (MPlayer == nullptr)
    {
        result = file.readIdString("purchaseFile", purchaseFile, 0x7f);
        Assert(result == 0, 0, " cound not read purchasing file in campain file ", nullptr);

        if (playerLights != nullptr)
        {
            delete playerLights;
            playerLights = nullptr;
        }
    }
    else
    {
        // Port: the original allocated the FitIniFile (asserting it got the memory).
        FitIniFile purchasing;
        char purchaseName[12];
        std::strcpy(purchaseName, "purchase");

        // Original behaviour (OB-100): MainPurchaseFile is read only when the PurchaseInfo block is missing (from the
        // block the file was on); with the block there, "purchase" is used.
        if (file.seekBlock("PurchaseInfo") != 0)
        {
            file.readIdString("MainPurchaseFile", purchaseName, 9);
        }

        std::snprintf(text, sizeof(text), "%s%s.fit", missionPath, purchaseName);
        result = purchasing.open(text);
        Assert(result == 0, 0, " could not open purchasing file ", nullptr);
        result = purchasing.seekBlock("PilotCosts");
        Assert(result == 0, static_cast<uint32_t>(result), "Could not find PilotCosts block in purchasing file",
               nullptr);
        result = purchasing.readIdLong("Green", pilotCosts[0]);
        Assert(result == 0, static_cast<uint32_t>(result), "Could not read Green pilot in purchasing file", nullptr);
        result = purchasing.readIdLong("Regular", pilotCosts[1]);
        Assert(result == 0, static_cast<uint32_t>(result), "Could not read Regular pilot in purchasing file", nullptr);
        result = purchasing.readIdLong("Veteran", pilotCosts[2]);
        Assert(result == 0, static_cast<uint32_t>(result), "Could not read Veteran pilot in purchasing file", nullptr);
        result = purchasing.readIdLong("Elite", pilotCosts[3]);
        Assert(result == 0, static_cast<uint32_t>(result), "Could not read Elite pilot in purchasing file.", nullptr);
    }

    repairScreen->selectedMech = nullptr;
    repairScreen->selectedVehicle = nullptr;
    int32_t savedMission = 0;

    if (MPlayer == nullptr)
    {
        if (file.readIdLong("MissionNumber", savedMission) == 0)
        {
            currentMission = savedMission;
        }
        else
        {
            savedMission = -1;
        }

        result = file.seekBlock("ResourcePoints");
        Assert(result == 0, 0, " could not find Resource Points ", nullptr);
        uint32_t points = 0;
        result = file.readIdULong("numPoints", points);
        Assert(result == 0, 0, " Could not find resource points in campaign file ", nullptr);
        ResourcePoints = static_cast<int32_t>(points);
    }
    else
    {
        result = file.seekBlock("Multiplayer");
        Assert(result == 0, 0, "This is not a multiplayer file!", nullptr);
        result = file.readIdString("MissionName", mpMissionName, 0x7f);
        Assert(result == 0, 0, "No mission file in save game file!", nullptr);

        if (file.readIdULong("PlanningTime", planningTime) != 0)
        {
            planningTime = defaultPlanningTime;
        }

        // The team's resource points (typed on the session screen) are shared among its players.
        const int32_t teamPlayers = MPlayer->playersOnHomeTeam()->count;
        lTextObject* pointsText = MPlayer->homeTeam == 0 ? sessionScreen->team1RPText : sessionScreen->team2RPText;
        ResourcePoints = std::atoi(pointsText->buffer) / teamPlayers;

        if (file.seekBlock("MPQuickStart") == 0)
        {
            mission->currentScenario = -1;
            mission->currentMovie = 0;
            getCurrentMission();
            loadQuickStart(&file);
            quickStart = 1;
        }
    }

    if (MPlayer == nullptr && quickStart == 0)
    {
        // The force: unassigned then assigned entries of each kind, numbered on from the unassigned ones. An entry
        // names a profile or a packet of this save.
        // Port fix: an assigned list whose unassigned block is missing starts at 0; the original started at a
        // leftover value (the extension pointer, the uninitialised count, or the mech loop's counter).
        uint32_t count = 0;
        uint32_t numWarriors = 0;
        char name[0x50];

        if (file.seekBlock("Warriors") == 0)
        {
            result = file.readIdULong("NumWarriors", count);
            Assert(result == 0, 0, " could not read warrior count ", nullptr);
            numWarriors = count;

            for (int32_t index = 0; index < static_cast<int32_t>(count); index++)
            {
                std::snprintf(name, sizeof(name), "Warrior%d", index);
                result = file.seekBlock(name);
                Assert(result == 0, 0, " could not find warrior block ", nullptr);

                if (file.readIdString("Profile", name, 0x4f) == 0)
                {
                    warriorList->addWarrior(name, 1);
                }
                else
                {
                    uint32_t packet = 0;
                    result = file.readIdULong("PacketNum", packet);
                    Assert(result == 0, 0, " could not find warrior Data ", nullptr);
                    warriorList->addWarrior(&packetFile, static_cast<int32_t>(packet + 1), 1);
                }
            }
        }

        if (file.seekBlock("AssWarriors") == 0)
        {
            result = file.readIdULong("NumAssWarriors", count);
            Assert(result == 0, 0, " could not read Assigned warrior count ", nullptr);
            const auto first = static_cast<int32_t>(numWarriors);

            for (int32_t index = first; index < first + static_cast<int32_t>(count); index++)
            {
                std::snprintf(name, sizeof(name), "Warrior%d", index);
                result = file.seekBlock(name);
                Assert(result == 0, 0, " could not find warrior block ", nullptr);

                if (file.readIdString("Profile", name, 0x4f) == 0)
                {
                    // Original behaviour: an assigned pilot given by profile joins the unassigned list.
                    warriorList->addWarrior(name, 1);
                }
                else
                {
                    uint32_t packet = 0;
                    result = file.readIdULong("PacketNum", packet);
                    Assert(result == 0, 0, " could not find warrior Data ", nullptr);
                    assignedWarriorList->addWarrior(&packetFile, static_cast<int32_t>(packet + 1), 0);
                }
            }
        }

        uint32_t numMechs = 0;

        if (file.seekBlock("Mechs") == 0)
        {
            result = file.readIdULong("NumMechs", count);
            Assert(result == 0, 0, " could not read mech count ", nullptr);
            numMechs = count;

            for (int32_t index = 0; index < static_cast<int32_t>(count); index++)
            {
                std::snprintf(name, sizeof(name), "Mech%d", index);
                result = file.seekBlock(name);
                Assert(result == 0, 0, " could not find mech block ", nullptr);

                if (file.readIdString("Profile", name, 0x4f) == 0)
                {
                    int32_t available = 0;

                    if (file.readIdLong("NumAvailable", available) != 0)
                    {
                        available = 1;
                    }

                    for (int32_t copy = 0; copy < available; copy++)
                    {
                        mechList->addMech(name, 0, 1, 1);
                    }
                }
                else
                {
                    uint32_t packet = 0;
                    result = file.readIdULong("PacketNum", packet);
                    Assert(result == 0, 0, " could not find Mech Data ", nullptr);
                    int32_t available = 0;

                    if (file.readIdLong("NumAvailable", available) != 0)
                    {
                        available = 1;
                    }

                    for (int32_t copy = 0; copy < available; copy++)
                    {
                        mechList->addMech(&packetFile, static_cast<int32_t>(packet + 1));
                    }
                }
            }
        }

        if (file.seekBlock("AssMechs") == 0)
        {
            result = file.readIdULong("NumAssMechs", count);
            Assert(result == 0, 0, " could not read assigned mech count ", nullptr);
            const auto first = static_cast<int32_t>(numMechs);

            for (int32_t index = first; index < first + static_cast<int32_t>(count); index++)
            {
                std::snprintf(name, sizeof(name), "Mech%d", index);
                result = file.seekBlock(name);
                Assert(result == 0, 0, " could not find mech block ", nullptr);

                if (file.readIdString("Profile", name, 0x4f) == 0)
                {
                    // Original behaviour: an assigned mech given by profile joins the unassigned list.
                    mechList->addMech(name, 0, 1, 1);
                }
                else
                {
                    uint32_t packet = 0;
                    result = file.readIdULong("PacketNum", packet);
                    Assert(result == 0, 0, " could not find Mech Data ", nullptr);
                    forceMechList->addMech(&packetFile, static_cast<int32_t>(packet + 1));
                }
            }
        }

        uint32_t numVehicles = 0;

        if (file.seekBlock("Vehicles") == 0)
        {
            result = file.readIdULong("NumVehicles", count);
            Assert(result == 0, 0, " could not read vehicle count ", nullptr);
            numVehicles = count;

            for (int32_t index = 0; index < static_cast<int32_t>(count); index++)
            {
                std::snprintf(name, sizeof(name), "Vehicle%d", index);
                result = file.seekBlock(name);
                Assert(result == 0, 0, " could not find Vehicle block ", nullptr);

                if (file.readIdString("Profile", name, 0x4f) == 0)
                {
                    int32_t available = 0;

                    if (file.readIdLong("NumAvailable", available) != 0)
                    {
                        available = 1;
                    }

                    for (int32_t copy = 0; copy < available; copy++)
                    {
                        vehicleList->addVehicle(name, 0, 0, 1);
                    }
                }
                else
                {
                    uint32_t packet = 0;
                    result = file.readIdULong("PacketNum", packet);
                    Assert(result == 0, 0, " could not find vehicle Data ", nullptr);
                    int32_t available = 0;

                    if (file.readIdLong("NumAvailable", available) != 0)
                    {
                        available = 1;
                    }

                    // The packet's [General] Assigned says which list the vehicle goes to.
                    int assigned = 0;
                    FitIniFile vehicleFile;
                    result = packetFile.seekPacket(static_cast<int32_t>(packet + 1));
                    Assert(result == 0, 0, " Vehicle Packet Not Found ", nullptr);
                    result = vehicleFile.open(&packetFile, packetFile.getPacketSize());
                    Assert(result == 0, 0, " Vehicle file could not open ", nullptr);
                    result = vehicleFile.seekBlock("General");
                    Assert(result == 0, static_cast<uint32_t>(result), "Failed General Block in Vehicle", nullptr);

                    if (vehicleFile.readIdBoolean("Assigned", assigned) != 0)
                    {
                        assigned = 0;
                    }

                    for (int32_t copy = 0; copy < available; copy++)
                    {
                        result = packetFile.seekPacket(static_cast<int32_t>(packet + 1));
                        Assert(result == 0, 0, " Vehicle Packet Not Found ", nullptr);
                        LogVehicleList* list = assigned == 0 ? vehicleList : forceVehicleList;
                        list->addVehicle(&packetFile, static_cast<int32_t>(packet + 1));
                    }
                }
            }
        }

        if (file.seekBlock("AssVehicles") == 0)
        {
            result = file.readIdULong("NumAssVehicles", count);
            Assert(result == 0, 0, " could not read vehicle count ", nullptr);
            const auto first = static_cast<int32_t>(numVehicles);

            for (int32_t index = first; index < first + static_cast<int32_t>(count); index++)
            {
                std::snprintf(name, sizeof(name), "Vehicle%d", index);
                result = file.seekBlock(name);
                Assert(result == 0, 0, " could not find Vehicle block ", nullptr);

                if (file.readIdString("Profile", name, 0x4f) == 0)
                {
                    vehicleList->addVehicle(name, 0, 0, 1);
                }
                else
                {
                    uint32_t packet = 0;
                    result = file.readIdULong("PacketNum", packet);
                    Assert(result == 0, 0, " could not find vehicle Data ", nullptr);
                    LogVehicle* vehicle = forceVehicleList->addVehicle(&packetFile, static_cast<int32_t>(packet + 1));

                    if (loadForce != 0 || newCampaign != 0)
                    {
                        vehicle->deployed = 0;
                    }
                }
            }
        }
    }

    // Every component the game knows (allcomp.fit), with no copies.
    {
        FitIniFile allComponents;
        FullPathFileName allPath;
        allPath.init(objectPath, "allcomp", ".fit");
        result = allComponents.open(allPath);
        Assert(result == 0, static_cast<uint32_t>(result), " Couldn't find allcomp.fit ", nullptr);
        result = allComponents.seekBlock("Components");
        Assert(result == 0, 0, " could not read component block ", nullptr);
        uint32_t count = 0;
        result = allComponents.readIdULong("NumComponents", count);
        Assert(result == 0, 0, " could not read component count ", nullptr);

        for (int32_t index = 0; index < static_cast<int32_t>(count); index++)
        {
            std::snprintf(text, sizeof(text), "Componant%d", index);
            result = allComponents.seekBlock(text);
            Assert(result == 0, 0, " could not read component entry in allcomp ", nullptr);
            uint8_t masterID = 0;
            result = allComponents.readIdUChar("ComponantID", masterID);
            Assert(result == 0, 0, " could not read component entry in allcomp ", nullptr);
            _LogInventoryStat* stat = componentInventory->createStat(static_cast<uint8_t>(index), 0, 0, 0, 0, 0, 0xff);
            componentInventory->addItem(masterID, stat, index);
            componentInventory->loadDescription(componentInventory->getIndexFromMasterID(masterID), nullptr);
        }
    }

    // The save's components: the counts of the known ones, and any new ones.
    if (file.seekBlock("Components") == 0)
    {
        uint32_t count = 0;
        result = file.readIdULong("NumComponents", count);
        Assert(result == 0, 0, " could not read component count ", nullptr);

        for (int32_t index = 0; index < static_cast<int32_t>(count); index++)
        {
            // Original behaviour: a bad component entry ends the load here, returning the error.
            std::snprintf(text, sizeof(text), "Componant%d", index);
            result = file.seekBlock(text);

            if (result != 0)
            {
                return result;
            }

            uint8_t masterID = 0;
            result = file.readIdUChar("ComponantID", masterID);

            if (result != 0)
            {
                return result;
            }

            int32_t available = 0;
            result = file.readIdLong("NumAvailable", available);

            if (result != 0)
            {
                return result;
            }

            InventoryList* inventory = componentInventory;

            if (inventory->getIndexFromMasterID(masterID) == -1)
            {
                _LogInventoryStat* stat =
                    inventory->createStat(static_cast<uint8_t>(index), 0, 0, 0, static_cast<int16_t>(available),
                                          static_cast<int16_t>(available), 0xff);
                inventory->addItem(masterID, stat, index);
                inventory->loadDescription(inventory->getIndexFromMasterID(masterID), nullptr);
            }
            else
            {
                inventory->addCountToItem(available, masterID);
            }
        }
    }

    // Time passes between missions: the pilots heal.
    if (newCampaign == 0 && loadForce == 0)
    {
        assignedWarriorList->heal(1);
        warriorList->heal(2);
    }

    if (currentMission == savedMission || newCampaign != 0 || MPlayer != nullptr)
    {
        mission->currentScenario = currentMission;
        mission->currentMovie = currentMission + 1;
        getCurrentMission();
    }
    else
    {
        // Coming back from a mission: apply its results (the "<mission>.pkk" save the mission wrote).
        const char* resultName = currentMission - 1 == -1 ? mission->scenarios[mission->currentScenario]
                                                          : mission->scenarios[currentMission - 1];
        FullPathFileName resultPath;
        resultPath.init(savePath, resultName, ".pkk");
        PacketFile resultFile;
        result = resultFile.open(resultPath);

        if (result != 0)
        {
            return result;
        }

        result = resultFile.seekPacket(0);
        Assert(result == 0, 0, " could not find mission result file ", nullptr);
        // Port fix: the original reopened the campaign FitIniFile without closing it first.
        file.close();
        result = file.open(&resultFile, resultFile.getPacketSize());
        Assert(result == 0, 0, " could not open mission result file ", nullptr);
        result = file.seekBlock("General");
        Assert(result == 0, 0, " could not find General Block in mission file ", nullptr);
        result = file.readIdString("purchaseFile", purchaseFile, 0x7f);
        Assert(result == 0, 0, " cound not read purchasing file in campain file ", nullptr);
        result = file.seekBlock("ResourcePoints");
        Assert(result == 0, 0, " could not find Resource Points ", nullptr);
        uint32_t points = 0;
        result = file.readIdULong("numPoints", points);
        Assert(result == 0, 0, " Could not find resource points in mission file ", nullptr);
        ResourcePoints = static_cast<int32_t>(points + static_cast<uint32_t>(ResourcePoints));

        uint32_t count = 0;
        char name[0x50];

        if (file.seekBlock("Warriors") == 0)
        {
            result = file.readIdULong("NumWarriors", count);
            Assert(result == 0, 0, " could not read warrior count ", nullptr);

            for (int32_t index = 0; index < static_cast<int32_t>(count); index++)
            {
                std::snprintf(name, sizeof(name), "Warrior%d", index);
                result = file.seekBlock(name);
                Assert(result == 0, 0, " could not find warrior block ", nullptr);

                if (file.readIdString("Profile", name, 0x4f) == 0)
                {
                    Assert(false, 0, " Somehow game write out a profile instead of a packet ! ", nullptr);
                }
                else
                {
                    uint32_t packet = 0;
                    result = file.readIdULong("PacketNum", packet);
                    Assert(result == 0, 0, " could not find warrior Data ", nullptr);
                    // A pilot not in the list (5) joins it.
                    LogWarriorList* pilots = assignedWarriorList;

                    if (pilots->replaceWarrior(&resultFile, static_cast<int32_t>(packet + 1)) == 5)
                    {
                        pilots->addWarrior(&resultFile, static_cast<int32_t>(packet + 1), 0);
                    }
                }
            }
        }

        if (file.seekBlock("Mechs") == 0)
        {
            result = file.readIdULong("NumMechs", count);
            Assert(result == 0, 0, " could not read mech count ", nullptr);

            for (int32_t index = 0; index < static_cast<int32_t>(count); index++)
            {
                std::snprintf(name, sizeof(name), "Mech%d", index);
                result = file.seekBlock(name);
                Assert(result == 0, 0, " could not find mech block ", nullptr);

                if (file.readIdString("Profile", name, 0x4f) == 0)
                {
                    forceMechList->addMech(name, 0, 1, 1);
                    continue;
                }

                int assigned = 1;
                uint32_t packet = 0;
                result = file.readIdULong("PacketNum", packet);
                Assert(result == 0, 0, " could not find Mech Data ", nullptr);
                {
                    FitIniFile mechFile;
                    result = resultFile.seekPacket(static_cast<int32_t>(packet + 1));
                    Assert(result == 0, static_cast<uint32_t>(result), "could not find mech packet in save file",
                           nullptr);
                    result = mechFile.open(&resultFile, resultFile.getPacketSize());
                    Assert(result == 0, static_cast<uint32_t>(result), "could not open mech packet in save file",
                           nullptr);
                    result = mechFile.seekBlock("General");
                    Assert(result == 0, static_cast<uint32_t>(result),
                           "could not find [General] block in mech packet in save file", nullptr);
                    result = mechFile.readIdBoolean("Assigned", assigned);
                    Assert(result == 0, static_cast<uint32_t>(result),
                           "could not find Assigned variable in [General] block in mech packet in save file", nullptr);
                }

                // An assigned mech replaces its copy in the force; one not there (5), or an unassigned one, is added.
                LogMechList* list = mechList;

                if (assigned != 0)
                {
                    list = forceMechList;

                    if (list->replaceMech(&resultFile, static_cast<int32_t>(packet + 1)) != 5)
                    {
                        continue;
                    }
                }

                list->addMech(&resultFile, static_cast<int32_t>(packet + 1));
            }
        }

        // Force vehicles that were deployed are gone (the head of the list only).
        LogVehicleList* forceVehicles = forceVehicleList;

        for (LogVehicle* vehicle = forceVehicles->vehicles; vehicle != nullptr && vehicle->deployed != 0;
             vehicle = forceVehicles->vehicles)
        {
            forceVehicles->removeVehicle(vehicle);
        }

        // Salvaged mechs (not yet the player's) take the first pilot indexes; the others' move up past them.
        LogMechList* force = forceMechList;
        int32_t salvaged = 0;

        for (int32_t index = 0; index < force->numMechs; index++)
        {
            LogMech* mech = nullptr;
            force->getMechInfo(index, mech);

            if (mech->notMineYet != 0)
            {
                salvaged++;
            }
        }

        shiftPilots(0, salvaged);
        force = forceMechList;
        int32_t nextPilot = 0;

        for (int32_t index = 0; index < force->numMechs; index++)
        {
            LogMech* mech = nullptr;
            force->getMechInfo(index, mech);

            if (mech->notMineYet != 0)
            {
                mech->notMineYet = 0;
                mech->pilotIndex = nextPilot++;
            }
        }

        // Mechs whose pilot ejected (and lives) leave the force with their pilot; the scan restarts after each.
        for (int32_t index = 0; index < force->numMechs; force = forceMechList)
        {
            LogMech* mech = nullptr;
            force->getMechInfo(index, mech);
            LogWarrior* pilot = nullptr;
            assignedWarriorList->getWarriorInfo(mech->pilotIndex, pilot);
            Assert(pilot != nullptr, 0, " Warrior in an assigned mech is NULL ", nullptr);

            if (pilot->ejected == 0 || pilot->health <= 0.0f)
            {
                index++;
                continue;
            }

            force->extractMech(index, mech);
            assignedWarriorList->extractWarrior(mech->pilotIndex, pilot);
            shiftPilots(mech->pilotIndex, -1);
            mech->pilotIndex = -1;
            mech->deployed = 0;
            mech->assigned = 0;
            pilot->ejected = 0;
            pilot->assigned = 0;
            mechList->addMech(mech, 1);
            mech->calcPilotModifier();
            warriorList->addWarrior(pilot, 1);
            index = 0;
        }

        // The same for mechs whose pilot died.
        for (int32_t index = 0; index < forceMechList->numMechs;)
        {
            LogMech* mech = nullptr;
            forceMechList->getMechInfo(index, mech);
            LogWarrior* pilot = nullptr;
            assignedWarriorList->getWarriorInfo(mech->pilotIndex, pilot);
            Assert(pilot != nullptr, 0, " Warrior in an assigned mech is NULL ", nullptr);

            if (pilot->health != 0.0f)
            {
                index++;
                continue;
            }

            forceMechList->extractMech(index, mech);
            assignedWarriorList->extractWarrior(mech->pilotIndex, pilot);
            shiftPilots(mech->pilotIndex, -1);
            mech->pilotIndex = -1;
            mech->deployed = 0;
            mech->assigned = 0;
            pilot->ejected = 0;
            pilot->assigned = 0;
            mechList->addMech(mech, 1);
            warriorList->addWarrior(pilot, 1);
            mech->calcPilotModifier();
            index = 0;
        }

        if (file.seekBlock("Components") == 0)
        {
            result = file.readIdULong("NumComponents", count);
            Assert(result == 0, 0, " could not read component count ", nullptr);

            for (int32_t index = 0; index < static_cast<int32_t>(count); index++)
            {
                std::snprintf(name, sizeof(name), "Componant%d", index);
                result = file.seekBlock(name);
                Assert(result == 0, static_cast<uint32_t>(result), "Could not find Component Block", nullptr);
                uint8_t masterID = 0;
                result = file.readIdUChar("ComponantID", masterID);
                Assert(result == 0, static_cast<uint32_t>(result), "Could not find Component Master ID", nullptr);
                int32_t available = 0;
                result = file.readIdLong("NumAvailable", available);
                Assert(result == 0, static_cast<uint32_t>(result), "Could not find Component numAvailable", nullptr);
                componentInventory->addCountToItem(available, masterID);
            }
        }

        if (loadForce == 0)
        {
            assignedWarriorList->heal(1);
        }

        resultFile.close();
    }

    if (MPlayer == nullptr)
    {
        // The purchase options of this point in the campaign, and an automatic save when a new mission starts.
        FitIniFile masterFile;
        FullPathFileName masterPath;
        masterPath.init(missionPath, missionName, ".fit");
        result = masterFile.open(masterPath);
        Assert(result == 0, 0, " could not open master mission file ", nullptr);
        result = masterFile.seekBlock("OpInfo");
        Assert(result == 0, 0, " could not find operation information in master mission file", nullptr);
        int32_t operationNumber = 0;
        std::snprintf(text, sizeof(text), "Scenario%iOperation", LastLogisticsMissionState);
        result = masterFile.readIdLong(text, operationNumber);
        Assert(result == 0, 0, " could not find operation number in master mission file ", nullptr);
        int32_t missionNumber = 0;
        std::snprintf(text, sizeof(text), "Scenario%iMission", LastLogisticsMissionState);
        result = masterFile.readIdLong(text, missionNumber);
        Assert(result == 0, 0, " could not find mission number in master mission file ", nullptr);
        char* oldPurchaseFile = setUpCampaignPurchasing(purchaseFile, &packetFile);

        if (LastLogisticsMissionState < currentMission && loadForce == 0)
        {
            setUpOldPurchasing(oldPurchaseFile);
            char format[200];
            cLoadString(thisInstance, CurPlanet == 0 ? 0x37a : 0x386, format, 199);
            std::snprintf(text, sizeof(text), format, operationNumber, missionNumber);
            saveCampaign(text);
        }

        logFree(oldPurchaseFile);
        // Killed pilots leave the roster and can't be hired again.
        LogWarriorList* pilots = warriorList;

        for (int32_t index = 0; index < pilots->numWarriors;)
        {
            LogWarrior* pilot = nullptr;
            pilots->getWarriorInfo(index, pilot);

            if (pilot == nullptr || pilot->warriorStatus != 4)
            {
                index++;
                continue;
            }

            // Original behaviour (OB-098): the pilot is removed by its id used as a list position.
            pilots->removeWarrior(static_cast<uint8_t>(pilot->id));
            purPilotList->setPilotStatus(pilot->descIndex, 2);
            index = 0;
        }

        purchaseScreen->createPurVehiclePane(0);
    }
    else
    {
        setUpMPPurchasing(purchaseFile);
        purchaseScreen->createPurVehiclePane(0);
    }

    packetFile.close();
    LogInvScreen* screen = repairScreen;
    screen->createMechInvBlock();
    screen->createPilotInvBlock();
    screen->createCompInvBlock();
    screen->createVhclInvBlock();
    screen->setUpMechInv(1, 1);
    screen->createVehiclePane();
    return 0;
}

auto Logistics::prepareScenario(char* scenarioName, char* startFile) -> int32_t
{
    LastLogisticsMissionState = currentMission;

    if (multiplayerInitialized != 0)
    {
        return prepareMultiplayerScenario(scenarioName, startFile);
    }

    char text[0x100];
    int32_t result;
    {
        // The automatic "before the mission" save, named after the operation and mission.
        FitIniFile masterFile;
        FullPathFileName masterPath;
        masterPath.init(missionPath, missionName, ".fit");
        result = masterFile.open(masterPath);
        Assert(result == 0, 0, " could not open master mission file ", nullptr);
        result = masterFile.seekBlock("OpInfo");
        Assert(result == 0, 0, " could not find operation information in master mission file", nullptr);
        int32_t operationNumber = 0;
        std::snprintf(text, sizeof(text), "Scenario%iOperation", LastLogisticsMissionState);
        result = masterFile.readIdLong(text, operationNumber);
        Assert(result == 0, 0, " could not find operation number in master mission file ", nullptr);
        int32_t missionNumber = 0;
        std::snprintf(text, sizeof(text), "Scenario%iMission", LastLogisticsMissionState);
        result = masterFile.readIdLong(text, missionNumber);
        Assert(result == 0, 0, " could not find mission number in master mission file ", nullptr);

        if (Solo == 0)
        {
            char format[200];
            cLoadString(thisInstance, CurPlanet == 0 ? 0x37b : 0x387, format, 199);
            std::snprintf(text, sizeof(text), format, operationNumber, missionNumber);
            saveCampaign(text);
        }

        // Write the deployed mechs (with their pilots) and vehicles as mech####/warr#### profiles.
        int32_t profileNumber = 0;

        for (int32_t lance = 0; lance < 3; lance++)
        {
            for (int32_t slot = 0; slot < 4; slot++)
            {
                const int32_t unit = deploySlots[lance][slot].unit;
                char profileName[0x20];

                if (unit < 0)
                {
                    const int32_t vehicleIndex = deploySlots[lance][slot].vehicle;

                    if (vehicleIndex < 0)
                    {
                        continue;
                    }

                    LogVehicle* vehicle = nullptr;
                    forceVehicleList->getVehicleInfo(vehicleIndex, vehicle);

                    if (vehicle == nullptr)
                    {
                        continue;
                    }

                    std::snprintf(profileName, sizeof(profileName), "mech%04d", profileNumber);
                    forceVehicleList->saveVehicleText(profileName, vehicleIndex);
                    profileNumber++;
                }
                else
                {
                    LogMech* mech = nullptr;
                    forceMechList->getMechInfo(unit, mech);

                    if (mech == nullptr)
                    {
                        continue;
                    }

                    std::snprintf(profileName, sizeof(profileName), "mech%04d", profileNumber);
                    forceMechList->saveMechText(profileName, unit);
                    char warriorName[0x20];
                    std::snprintf(warriorName, sizeof(warriorName), "warr%04d", profileNumber);
                    assignedWarriorList->saveWarriorText(warriorName, forceMechList->getMechPilotIndex(unit));
                    profileNumber++;
                }
            }
        }
    }

    // Copy the mission's scenario file into the start file, block by block, then add the player's force.
    // Port: the original allocated both FitIniFiles (asserting it got the memory).
    FullPathFileName inPath;
    inPath.init(missionPath, scenarioName, ".fit");
    FitIniFile in;
    result = in.open(inPath);
    Assert(result == 0, static_cast<uint32_t>(result), " could not open logistics scenario file ", nullptr);
    FullPathFileName outPath;
    outPath.init(saveTempPath, startFile, ".fit");
    FitIniFile out;
    result = out.create(outPath);
    Assert(result == 0, static_cast<uint32_t>(result), " could not open scenario file ", nullptr);

    // The values being copied; later reads that are not checked write whatever the last read left in them.
    int32_t longValue = 0;
    uint32_t ulongValue = 0;
    float floatValue = 0.0f;
    const auto check = [&](bool ok, const char* message)
    { Assert(ok, static_cast<uint32_t>(result), message, nullptr); };
    const auto copyBlock = [&](const char* block, const char* findMessage, const char* writeMessage)
    {
        result = in.seekBlock(block);
        check(result == 0, findMessage);
        result = out.writeBlock(block);
        check(result > 0, writeMessage);
    };

    const auto copyLong = [&](const char* name, const char* findMessage, const char* writeMessage)
    {
        result = in.readIdLong(name, longValue);
        check(result == 0, findMessage);
        result = out.writeIdLong(name, longValue);
        check(result > 0, writeMessage);
    };

    const auto copyULong = [&](const char* name, const char* findMessage, const char* writeMessage)
    {
        result = in.readIdULong(name, ulongValue);
        check(result == 0, findMessage);
        result = out.writeIdULong(name, ulongValue);
        check(result > 0, writeMessage);
    };

    const auto copyFloat = [&](const char* name, const char* findMessage, const char* writeMessage)
    {
        result = in.readIdFloat(name, floatValue);
        check(result == 0, findMessage);
        result = out.writeIdFloat(name, floatValue);
        check(result > 0, writeMessage);
    };

    const auto copyString = [&](const char* name, uint32_t maxLength, const char* findMessage, const char* writeMessage)
    {
        result = in.readIdString(name, text, maxLength);
        check(result == 0, findMessage);
        result = out.writeIdString(name, text);
        check(result > 0, writeMessage);
    };

    const auto copyChar = [&](const char* name, const char* findMessage, const char* writeMessage)
    {
        char value = 0;
        result = in.readIdChar(name, value);
        check(result == 0, findMessage);
        result = out.writeIdChar(name, value);
        check(result > 0, writeMessage);
    };

    copyBlock("ContactManager", " could not find ContactManager Block ", " could not write ContactManager Block ");
    copyLong("MaxContacts", " could not find MaxContacts in ContactManager Block ",
             " could not write MaxContacts in ContactManager Block ");
    copyBlock("PotentialContactManager", " could not find PotentialContactManager Block ",
              " could not write PotentialContactManager Block ");
    copyLong("MaxPotentialContacts", " could not find MaxPotentialContacts in PotentialContactManager Block ",
             " could not write MaxPotentialContacts in PotentialContactManager Block ");
    copyBlock("ABLibraries", " could not find ABLibraries Block ", " could not write ABLibraries Block ");

    for (int32_t index = 0;; index++)
    {
        char name[0x20];
        std::snprintf(name, sizeof(name), "Library%d", index);

        if (in.readIdString(name, text, 0xff) != 0)
        {
            break;
        }

        result = out.writeIdString(name, text);
        check(result > 0, " could not write library string in ABLibraries Block ");
    }

    copyBlock("Smoke Manager", " could not find Smoke Manager Block ", " could not write Smoke Manager Block ");
    int32_t numSmokeTypes = 0;
    result = in.readIdLong("NumSmokeTypes", numSmokeTypes);
    check(result == 0, " could not find NumSmokeTypes in Smoke Manager Block ");
    result = out.writeIdLong("NumSmokeTypes", numSmokeTypes);
    check(result > 0, " could not write NumSmokeTypes in Smoke Manager Block ");
    copyLong("MaxSmokesPerType", " could not find MaxSmokesPerType in Smoke Manager Block ",
             " could not write MaxSmokesPerType in Smoke Manager Block ");
    copyULong("SmokeSphereHeapSize", " could not find SmokeSphereHeapSize in Smoke Manager Block ",
              " could not write SmokeSphereHeapSize in Smoke Manager Block ");

    for (int32_t index = 0; index < numSmokeTypes; index++)
    {
        char name[0x20];
        std::snprintf(name, sizeof(name), "Smoke%d", index);
        copyBlock(name, " could not find smoke Block ", " could not write smoke Block ");
        copyLong("SmokeType", " could not find SmokeType in Smoke Block ",
                 " could not write SmokeType in Smoke Block ");
    }

    copyBlock("CollisionSystem", " could not find CollisionSystem Block ", " could not write CollisionSystem Block ");
    copyULong("XGridSize", " could not find XGridSize in CollisionSystem Block ",
              " could not write XGridSize in CollisionSystem Block ");
    copyULong("YGridSize", " could not find YGridSize in CollisionSystem Block ",
              " could not write YGridSize in CollisionSystem Block ");
    copyULong("GridRadius", " could not find GridRadius in CollisionSystem Block ",
              " could not write GridRadius in CollisionSystem Block ");
    copyULong("MaxObjects", " could not find MaxObjects in CollisionSystem Block ",
              " could not write MaxObjects in CollisionSystem Block ");
    copyULong("MaxCollisions", " could not find MaxCollisions in CollisionSystem Block ",
              " could not write MaxCollisions in CollisionSystem Block ");
    copyULong("MaxPending", " could not find MaxPending in CollisionSystem Block ",
              " could not write MaxPending in CollisionSystem Block ");
    copyULong("CollisionHeapSize", " could not find CollisionHeapSize in CollisionSystem Block ",
              " could not write CollisionHeapSize in CollisionSystem Block ");
    copyFloat("WarningDist", " could not find WarningDist in CollisionSystem Block ",
              " could not write WarningDist in CollisionSystem Block ");
    copyFloat("AlertTime", " could not find AlertTime in CollisionSystem Block ",
              " could not write AlertTime in CollisionSystem Block ");
    copyULong("NumAlerts", " could not find NumAlerts in CollisionSystem Block ",
              " could not write NumAlerts in CollisionSystem Block ");

    if (in.seekBlock("StatusWindow") == 0)
    {
        result = out.writeBlock("StatusWindow");
        check(result > 0, " could not write StatusWindow Block ");
        copyULong("PosX", " could not find PosX in StatusWindow Block ",
                  " could not write PosX in StatusWindow Block ");
        copyULong("PosY", " could not find PosY in StatusWindow Block ",
                  " could not write PosY in StatusWindow Block ");
        copyULong("Width", " could not find Width in StatusWindow Block ",
                  " could not write Width in StatusWindow Block ");
        copyULong("Height", " could not find Height in StatusWindow Block ",
                  " could not write Height in StatusWindow Block ");
    }

    copyBlock("PaletteSystem", " could not find PaletteSystem Block ", " could not write PaletteSystem Block ");
    copyString("PaletteSystem", 0xff, " could not find PaletteSystem in PaletteSystem Block ",
               " could not write PaletteSystem in PaletteSystem Block ");
    copyBlock("Music", " could not find Music block in Scenario File ",
              " could not write Music block in Scenario File ");
    uint8_t tuneNumber = 0;
    result = in.readIdUChar("scenarioTuneNum", tuneNumber);
    check(result == 0, " could not find ScenarioTuneNum in Music block in Scenario File ");
    result = out.writeIdUChar("scenarioTuneNum", tuneNumber);
    check(result > 0, " could not write ScenarioTuneNum in Music block in Scenario File ");
    copyBlock("Artillery", " could not find Artillery block in Scenario File ",
              " could not write Artillery block in Scenario File ");

    // Original behaviour: the old format (NumStrikes) is not copied at all; the new one is copied without checks
    // after the first count. Port fix: the counts start at 0 (the original wrote leftovers when one was missing).
    if (in.readIdULong("NumStrikes", ulongValue) != 0)
    {
        int32_t strikes = 0;
        result = in.readIdLong("NumLargeStrikes", strikes);
        Assert(result == 0, 0, " Artillery is in neither of the two known states ", nullptr);
        out.writeIdLong("NumLargeStrikes", strikes);
        strikes = 0;
        in.readIdLong("NumSmallStrikes", strikes);
        out.writeIdLong("NumSmallStrikes", strikes);
        strikes = 0;
        in.readIdLong("NumSensorStrikes", strikes);
        out.writeIdLong("NumSensorStrikes", strikes);
        strikes = 0;
        in.readIdLong("NumCameraStrikes", strikes);
        out.writeIdLong("NumCameraStrikes", strikes);
    }

    copyBlock("GameScale", " could not find GameScale block in Scenario File ",
              " could not write GameScale block in Scenario File ");
    copyFloat("WorldUnitsPerMeter", " could not find worldUnitsperMeter in GameScale block in Scenario File ",
              " could not write worldUnitsperMeter in GameScale block in Scenario File ");
    copyFloat("MetersPerWorldUnit", " could not find MetersperWorldUnit in GameScale block in Scenario File ",
              " could not write MetersperWorldUnit in GameScale block in Scenario File ");
    copyULong("Duration", " could not find Duration in GameScale block in Scenario File ",
              " could not write Duration in GameScale block in Scenario File ");
    copyFloat("CycleLength", " could not find CycleLength in GameScale block in Scenario File ",
              " could not write CycleLength in GameScale block in Scenario File ");
    // Original behaviour: without a SingleStep the Duration is written under its name.
    in.readIdULong("SingleStep", ulongValue);
    out.writeIdULong("SingleStep", ulongValue);
    copyBlock("ElementSystem", " could not find ElementSystem block in Scenario File ",
              " could not write ElementSystem block in Scenario File ");
    copyULong("ElementHeapSize", " could not find ElementHeapSize in ElementSystem block in Scenario File ",
              " could not write ElementHeapSize in ElementSystem block in Scenario File ");
    copyULong("MaxElements", " could not find MaxElements in ElementSystem block in Scenario File ",
              " could not write MaxElements in ElementSystem block in Scenario File ");
    copyULong("MaxGroups", " could not find MaxGroups in ElementSystem block in Scenario File ",
              " could not write MaxGroups in ElementSystem block in Scenario File ");
    copyBlock("SensorContactShape", " could not find SensorContactShape block in Scenario File ",
              " could not write SensorContactShape block in Scenario File ");
    copyString("shapeName", 0x4f, " could not find ShapeName in SensorContactShape block in Scenario File ",
               " could not write ShapeName in SensorContactShape block in Scenario File ");
    copyBlock("CraterSystem", " could not find CraterSystem Block in Scenario File ",
              " could not write CraterSystem Block in Scenario File ");
    copyLong("NumCraters", " could not find NumCraters in CraterSystem Block in Scenario File ",
             " could not write NumCraters in CraterSystem Block in Scenario File ");
    copyULong("CraterShapeSize", " could not find CraterShapeSize in CraterSystem Block in Scenario File ",
              " could not write CraterShapeSize in CraterSystem Block in Scenario File ");
    copyString("CraterFile", 0xf, " could not find CraterFile in CraterSystem Block in Scenario File ",
               " could not write CraterFile in CraterSystem Block in Scenario File ");
    copyBlock("CameraSystem", " could not Find CameraSystem Block ", " could not write CameraSystem Block ");
    copyULong("CameraHeapSize", " could not Find CameraHeapSize in CameraSystem Block ",
              " could not write CameraHeapSize in CameraSystem Block ");
    copyString("CameraFileName", 0x4f, " could not Find CameraFileName in CameraSystem Block ",
               " could not write CameraFileName in CameraSystem Block ");
    copyBlock("ObjectSystem", " could not Find ObjectSystem Block ", " could not write ObjectSystem Block ");
    copyULong("ObjectHeapSize", " could not Find objectHeapSize in ObjectSystem Block ",
              " could not write objectHeapSize in ObjectSystem Block ");
    copyULong("ObjectTypeHeapSize", " could not Find ObjectTypeHeapSzize in ObjectSystem Block ",
              " could not write ObjectTypeHeapSzize in ObjectSystem Block ");
    copyULong("NumObjects", " could not Find NumObjects in ObjectSystem Block ",
              " could not write NumObjects in ObjectSystem Block ");
    copyString("ObjectFileName", 0x4f, " could not Find ObjectFileName in ObjectSystem Block ",
               " could not write ObjectFileName in ObjectSystem Block ");
    copyBlock("SpriteSystem", " could not Find SpriteSystem Block ", " could not write SpriteSystem Block ");
    copyULong("SpriteHeapSize", " could not Find SpriteHeapSize in SpriteSystem Block ",
              " could not write SpriteHeapSize in SpriteSystem Block ");
    copyULong("SpriteManagerHeapSize", " could not Find SpriteManagerHeapSize in SpriteSystem Block ",
              " could not write SpriteManagerHeapSize in SpriteSystem Block ");
    copyULong("SpriteDataHeapSize", " could not Find SpriteDataHeapSize in SpriteSystem Block ",
              " could not write SpriteDataHeapSize in SpriteSystem Block ");
    copyString("SpriteFileName", 0x4f, " could not Find SpriteFileName in SpriteSystem Block ",
               " could not write SpriteFileName in SpriteSystem Block ");
    copyString("ShapeFileName", 0x4f, " could not Find ShapeFileName in SpriteSystem Block ",
               " could not write ShapeFileName in SpriteSystem Block ");
    copyBlock("SpriteManager", " could not Find SpriteManager Block ", " could not write SpriteManager Block ");
    copyULong("LegHeapSize", " could not Find LegHeapSize in SpriteManager Block ",
              " could not write LegHeapSize in SpriteManager Block ");
    copyULong("TorsoHeapSize", " could not Find TorsoHeapSize in SpriteManager Block ",
              " could not write TorsoHeapSize in SpriteManager Block ");
    copyULong("RightArmHeapSize", " could not Find RightArmHeapSize in SpriteManager Block ",
              " could not write RightArmHeapSize in SpriteManager Block ");
    copyULong("LeftArmHeapSize", " could not Find LeftArmHeapSize in SpriteManager Block ",
              " could not write LeftArmHeapSize in SpriteManager Block ");
    copyULong("TotalMechs", " could not Find TotalMechs in SpriteManager Block ",
              " could not write TotalMechs in SpriteManager Block ");
    // Original behaviour: without a Use90Pixel the TotalMechs value is written under its name.
    in.readIdULong("Use90Pixel", ulongValue);
    out.writeIdULong("Use90Pixel", ulongValue);
    copyBlock("TerrainSystem", " could not find TerrainSystem block ", " could not write TerrainSystem block ");
    copyString("TerrainFileName", 0x4f, " could not find TerrainFileName in TerrainSystem block ",
               " could not write TerrainFileName in TerrainSystem block ");
    // Original behaviour: without a TacMapGifName the terrain file name is written under its name.
    in.readIdString("TacMapGifName", text, 0x4f);
    out.writeIdString("TacMapGifName", text);
    copyBlock("Script", " could not find Script Block ", " could not write Script Block ");
    copyString("ScenarioScript", 0x4f, " could not find ScenarioScript in Script Block ",
               " could not write ScenarioScript in Script Block ");

    // The computer-controlled parts and their pilots are copied renumbered from 1 (the player's parts are left
    // out; the force is added after them).
    result = in.seekBlock("Warriors");
    Assert(result == 0, 0, " Could not find Warriors Block ", nullptr);
    uint32_t numWarriors = 0;
    result = in.readIdULong("NumWarriors", numWarriors);
    check(result == 0, " Could not find NumWarriors in Warriors Block ");
    // Per warrior number: the part that uses it (first half) and its new number (second half, from numWarriors).
    // Port fix: the buffer is always made and cleared (with no warriors the original used the start file's name).
    std::vector<char> pilotMap(numWarriors * 2 + 2, 0);
    result = in.seekBlock("Parts");
    check(result == 0, " Could not find Parts Block ");
    uint32_t numParts = 0;
    result = in.readIdULong("NumParts", numParts);
    check(result == 0, " Could not find NumParts in Parts Block ");
    char blockName[0x20];

    for (int32_t part = 1; part < static_cast<int32_t>(numParts + 1); part++)
    {
        std::snprintf(blockName, sizeof(blockName), "Part%d", part);
        result = in.seekBlock(blockName);
        check(result == 0, " Could not find PartNumber Block ");
        int playerPart = 0;

        if (in.readIdBoolean("PlayerPart", playerPart) != 0 || playerPart == 0)
        {
            result = in.readIdULong("Pilot", ulongValue);
            check(result == 0, " Could not find Pilot in PartNumber Block ");
            pilotMap[ulongValue] = static_cast<char>(part);
        }
    }

    int32_t nextWarrior = 1;

    for (uint32_t warrior = 1; static_cast<int32_t>(warrior) < static_cast<int32_t>(numWarriors + 1); warrior++)
    {
        if (pilotMap[warrior] == 0)
        {
            continue;
        }

        pilotMap[numWarriors + warrior] = static_cast<char>(nextWarrior);
        std::snprintf(blockName, sizeof(blockName), "Warrior%d", warrior);
        result = in.seekBlock(blockName);
        Assert(result == 0, warrior, " Could not find Warrior Number Block ", nullptr);
        std::snprintf(blockName, sizeof(blockName), "Warrior%d", nextWarrior++);
        result = out.writeBlock(blockName);
        Assert(result > 0, warrior, " Could not find Warrior Number Block ", nullptr);
        result = in.readIdString("Profile", text, 99);
        Assert(result == 0, 0, " Could not find Warrior Profile in Warrior Number Block ", nullptr);
        result = out.writeIdString("Profile", text);
        Assert(result > 0, 0, " Could not write Warrior Profile in Warrior Number Block ", nullptr);
        result = in.readIdString("Brain", text, 0x7f);
        check(result == 0, " Could not find Warrior Brain in Warrior Number Block ");
        result = out.writeIdString("Brain", text);
        check(result > 0, " Could not write Warrior Brain in Warrior Number Block ");
    }

    const int32_t firstForceWarrior = nextWarrior;

    // Old part number -> new part number (-1 for the player's parts).
    int32_t partMap[0x40];

    for (int32_t& entry : partMap)
    {
        entry = -1;
    }

    int32_t nextPart = 1;

    for (int32_t part = 1; part < static_cast<int32_t>(numParts + 1); part++)
    {
        std::snprintf(blockName, sizeof(blockName), "Part%d", part);
        result = in.seekBlock(blockName);
        check(result == 0, " Could not find PartNumber Block ");
        int playerPart = 0;

        if (in.readIdBoolean("PlayerPart", playerPart) == 0 && playerPart != 0)
        {
            continue;
        }

        partMap[part] = nextPart;
        std::snprintf(blockName, sizeof(blockName), "Part%d", nextPart++);
        result = out.writeBlock(blockName);
        check(result > 0, " Could not write PartNumber Block ");
        copyULong("ObjectNumber", " Could not find ObjectNumber in PartNumber Block ",
                  " Could not write ObjectNumber in PartNumber Block ");
        copyULong("ControlType", " Could not find ControlType in PartNumber Block ",
                  " Could not write ControlType in PartNumber Block ");
        copyULong("ControlDataType", " Could not find ControlDataType in PartNumber Block ",
                  " Could not write ControlDataType in PartNumber Block ");
        copyString("ObjectProfile", 9, " Could not find ObjectProfile in PartNumber Block ",
                   " Could not write ObjectProfile in PartNumber Block ");
        result = in.readIdULong("Pilot", ulongValue);
        check(result == 0, " Could not find Pilot in PartNumber Block ");
        result =
            out.writeIdULong("Pilot", static_cast<uint32_t>(static_cast<int32_t>(pilotMap[numWarriors + ulongValue])));
        check(result > 0, " Could not write Pilot in PartNumber Block ");
        copyFloat("PositionX", " Could not find PositionX in PartNumber Block ",
                  " Could not write PositionX in PartNumber Block ");
        copyFloat("PositionY", " Could not find PositionY in PartNumber Block ",
                  " Could not write PositionY in PartNumber Block ");
        copyFloat("PositionZ", " Could not find PositionZ in PartNumber Block ",
                  " Could not write PositionZ in PartNumber Block ");
        copyFloat("Rotation", " Could not find Rotation in PartNumber Block ",
                  " Could not write Rotation in PartNumber Block ");
        copyChar("TeamId", " Could not find TeamId in PartNumber Block ",
                 " Could not write TeamId in PartNumber Block ");
        copyChar("CommanderId", " Could not find CommanderId in PartNumber Block ",
                 " Could not write CommanderId in PartNumber Block ");
        copyULong("Gesture", " Could not find Gesture in PartNumber Block ",
                  " Could not write Gesture in PartNumber Block ");
        copyFloat("Velocity", " Could not find Velocity in PartNumber Block ",
                  " Could not write Velocity in PartNumber Block ");
        copyLong("Active", " Could not find Active Flag in PartNumber Block ",
                 " Could not write Active Flag in PartNumber Block ");
        copyLong("Exists", " Could not find Exists Flag in PartNumber Block ",
                 " Could not write Exists Flag in PartNumber Block ");
        copyChar("MyIcon", " Could not find MyIcon in PartNumber Block ",
                 " Could not write MyIcon in PartNumber Block ");
    }

    const int32_t firstForcePart = nextPart;

    if (in.seekBlock("Elemental Carriers") == 0)
    {
        // Original behaviour: the carrier part numbers are not renumbered.
        result = out.writeBlock("Elemental Carriers");
        check(result > 0, " Could not write Elemental Carriers Block ");
        int32_t numCarriers = 0;
        result = in.readIdLong("Carriers", numCarriers);
        check(result == 0, " Could not read carriers in elemental carriers block");
        result = out.writeIdLong("Carriers", numCarriers);
        check(result > 0, " Could not write carriers in elemental carriers block");

        for (int32_t carrier = 0; carrier < numCarriers; carrier++)
        {
            std::snprintf(blockName, sizeof(blockName), "ECarrier%d", carrier);
            result = in.seekBlock(blockName);
            check(result == 0, " Could not find carrier block");
            result = out.writeBlock(blockName);
            check(result > 0, " Could not write carrier block");
            int32_t carrierPart = 0;
            result = in.readIdLong("Carrier", carrierPart);
            check(result == 0, " Could not read carrier in carrier block");
            Assert(carrierPart < firstForcePart, static_cast<uint32_t>(carrierPart),
                   "Illegal part number for elemental carrier", nullptr);
            result = out.writeIdLong("Carrier", carrierPart);
            check(result > 0, " Could not write carrier in carrier block");

            for (int32_t elemental = 0; elemental < 10; elemental++)
            {
                std::snprintf(blockName, sizeof(blockName), "Elemental%d", elemental);

                if (in.readIdLong(blockName, longValue) != 0)
                {
                    break;
                }

                result = out.writeIdLong(blockName, longValue);
                check(result > 0, " Could not write elemental in carrier block");
            }
        }
    }

    result = in.seekBlock("Objectives");
    check(result == 0, " Could not find Objective Block ");
    result = out.writeBlock("Objectives");
    check(result > 0, " Could not write Objective Block ");

    if (in.readIdLong("TimeLeft", longValue) != 0)
    {
        longValue = -1;
    }

    result = out.writeIdLong("TimeLeft", longValue);
    check(result > 0, " Could not write TimeLeft in Objective Block ");
    uint32_t numObjectives = 0;
    result = in.readIdULong("NumObjectives", numObjectives);
    check(result == 0, " Could not find numObjectives in Objective Block ");
    check(numObjectives < 9, " Too Many Objectives ");
    result = out.writeIdULong("NumObjectives", numObjectives);
    check(result > 0, " Could not write numObjectives in Objective Block ");
    uint32_t numInnerSphereObjectives = 0;

    if (in.readIdULong("NumInnerSphereObjectives", numInnerSphereObjectives) != 0)
    {
        numInnerSphereObjectives = 0;
    }

    uint32_t numClanObjectives = 0;

    if (in.readIdULong("NumClanObjectives", numClanObjectives) != 0)
    {
        numClanObjectives = 0;
    }

    // Original behaviour: NumObjectives is written a second time.
    result = out.writeIdULong("NumObjectives", numObjectives);
    check(result > 0, " Could not write numObjectives in Objective Block ");

    if (numInnerSphereObjectives != 0 || numClanObjectives != 0)
    {
        result = out.writeIdULong("NumInnerSphereObjectives", numInnerSphereObjectives);
        check(result > 0, " Could not write numInnerSphereObjectives in Objective Block ");
        result = out.writeIdULong("NumClanObjectives", numClanObjectives);
        check(result > 0, " Could not write numClanObjectives in Objective Block ");
    }

    for (uint32_t objective = 0; objective < numObjectives; objective++)
    {
        std::snprintf(blockName, sizeof(blockName), "Objective%d", objective);
        result = in.seekBlock(blockName);
        Assert(result == 0, objective, " Could not find ObjectiveNumber Block ", nullptr);
        result = out.writeBlock(blockName);
        Assert(result > 0, objective, " Could not write ObjectiveNumber Block ", nullptr);
        copyString("Name", 0xff, " Could not find Name in Objective Block ",
                   " Could not write Name in Objective Block ");
        copyULong("Type", " Could not find Type in Objective Block ", " Could not write Type in Objective Block ");
        copyFloat("TimeLeft", " Could not find TimeLeft in Objective Block ",
                  " Could not write TimeLeft in Objective Block ");
        copyULong("Status", " Could not find Status in Objective Block", " Could not write Status in Objective Block");

        if (in.readIdLong("Points", longValue) != 0)
        {
            longValue = 0;
        }

        result = out.writeIdLong("Points", longValue);
        check(result > 0, " Could not write Points in Objective Block");

        if (in.readIdFloat("Radius", floatValue) != 0)
        {
            floatValue = 0.0f;
        }

        result = out.writeIdFloat("Radius", floatValue);
        check(result > 0, " Could not write Radius in Objective Block");
    }

    result = in.seekBlock("Teams");
    check(result == 0, " Could not find Teams Block in Scenario ");
    result = out.writeBlock("Teams");
    check(result > 0, " Could not write Teams Block");
    int alliedTeam = 0;
    result = in.readIdBoolean("AlliedTeam", alliedTeam);
    check(result == 0, " Could not find Allied Team flag in Scenario ");
    result = out.writeIdBoolean("AlliedTeam", alliedTeam);
    check(result > 0, " Could not write AlliedTeam Flag");
    int32_t mates[12];
    // The computer's commanders' groups, with their first five members renumbered.
    const auto copyGroups =
        [&](const char* format, const char* writeMessage, const char* findMatesMessage, const char* writeMatesMessage)
    {
        int32_t group = 0;
        std::snprintf(text, sizeof(text), format, group);

        while (in.seekBlock(text) == 0)
        {
            result = out.writeBlock(text);
            check(result > 0, writeMessage);

            for (int32_t& mate : mates)
            {
                mate = 0;
            }

            result = in.readIdLongArray("Mates", mates, 12);
            check(result == 0, findMatesMessage);

            for (int32_t mate = 0; mate < 5; mate++)
            {
                if (mates[mate] > 0)
                {
                    mates[mate] = partMap[mates[mate]];
                }
            }

            result = out.writeIdLongArray("Mates", mates, 12);
            check(result > 0, writeMatesMessage);
            group++;
            std::snprintf(text, sizeof(text), format, group);
        }
    };

    copyGroups("Commander1Group:%d", " could not write Commander1Groupx in Scenario File ",
               " could not find Mates in ClanTeam in Scenario File ",
               " could not write Mates in ClanTeam in Scenario File ");
    copyGroups("Commander2Group:%d", " could not write Commander2Groupx in Scenario File ",
               " could not find Mates in AlliedTeam in Scenario File ",
               " could not write Mates in AlliedTeam in Scenario File ");

    // The "Raven system": the mission's own inactive or capturable player-side parts (at most four) join the force
    // as salvage (not the player's yet) in a free lance.
    result = in.seekBlock("Parts");
    check(result == 0, " Could not find Parts Block ");
    result = in.readIdULong("NumParts", ulongValue);
    check(result == 0, "Could not read Num Parts ");
    struct PartPlace
    {
        float x;
        float y;
        float rotation;
    };

    int32_t ravenParts[5] = {-1, 0, 0, 0, 0};
    int32_t ravenIsMech[4] = {};
    PartPlace ravenMechPlaces[4] = {};
    PartPlace ravenVehiclePlaces[4] = {};
    int32_t numRaven = 0;
    int32_t numRavenMechs = 0;
    int32_t numRavenVehicles = 0;

    for (int32_t part = 1; part <= static_cast<int32_t>(ulongValue); part++)
    {
        std::snprintf(text, sizeof(text), "Part%d", part);
        result = in.seekBlock(text);
        check(result == 0, " Could not locate part block");
        char teamID = 0;
        result = in.readIdChar("TeamId", teamID);
        check(result == 0, "Could not read alignment");

        if (teamID != 0)
        {
            continue;
        }

        int32_t active = 0;
        result = in.readIdLong("Active", active);
        check(result == 0, " Could not read Active ");
        int capturable = 0;

        if (in.readIdBoolean("Capturable", capturable) != 0)
        {
            capturable = 0;
        }

        if (active != 0 && capturable == 0)
        {
            continue;
        }

        ravenParts[numRaven] = part;
        numRaven++;
        Assert(numRaven < 5, static_cast<uint32_t>(numRaven), " Too Many Inactive parts.  Only allowed 4!! ", nullptr);
        result = in.readIdString("ObjectProfile", text, 9);
        check(result == 0, " Could not find ObjectProfile in PartNumber Block ");

        if (std::strstr(text, "v") == nullptr && std::strstr(text, "V") == nullptr)
        {
            // A mech, with its pilot.
            PartPlace& place = ravenMechPlaces[numRavenMechs];
            result = in.readIdFloat("PositionX", place.x);
            check(result == 0, " Could not find Raven Part Position ");
            result = in.readIdFloat("PositionY", place.y);
            check(result == 0, " Could not find Raven Part Position ");
            result = in.readIdFloat("Rotation", place.rotation);
            check(result == 0, " Could not find Raven Part Rotation ");
            forceMechList->addMech(text, 0, 0, 1);
            LogMech* mech = nullptr;
            forceMechList->getMechInfo(0, mech);
            mech->assigned = 1;
            mech->deployed = 1;
            mech->notMineYet = 1;
            std::strncpy(mech->profileName, text, 9);
            ravenIsMech[numRaven - 1] = 1;
            numRavenMechs++;
            uint32_t pilot = 0;
            result = in.readIdULong("Pilot", pilot);
            check(result == 0, " No pilot for this part ");
            char pilotBlock[0x20];
            std::snprintf(pilotBlock, sizeof(pilotBlock), "Warrior%d", pilot);
            result = in.seekBlock(pilotBlock);
            check(result == 0, " could not find pilot block for Raven System ");
            char pilotProfile[0x32];
            result = in.readIdString("Profile", pilotProfile, 0x31);
            check(result == 0, " could not find pilot profile for Raven System ");
            assignedWarriorList->addWarrior(pilotProfile, 0);
            LogWarrior* warrior = nullptr;
            assignedWarriorList->getWarriorInfo(0, warrior);
            warrior->notMineYet = 1;
        }
        else
        {
            PartPlace& place = ravenVehiclePlaces[numRavenVehicles];
            result = in.readIdFloat("PositionX", place.x);
            check(result == 0, " Could not find Raven Part Position ");
            result = in.readIdFloat("PositionY", place.y);
            check(result == 0, " Could not find Raven Part Position ");
            result = in.readIdFloat("Rotation", place.rotation);
            check(result == 0, " Could not find Raven Part Rotation ");
            forceVehicleList->addVehicle(text, 0, 0, 1);
            LogVehicle* vehicle = nullptr;
            forceVehicleList->getVehicleInfo(0, vehicle);
            vehicle->assigned = 1;
            vehicle->deployed = 1;
            vehicle->notMineYet = 1;
            std::strncpy(vehicle->profileName, text, 9);
            numRavenVehicles++;
        }
    }

    // The salvaged mechs take the first pilot indexes (their pilots went to the head of the list).
    // Original behaviour: the mech is looked up by its entry among the salvaged parts, not among the mechs.
    int32_t pilotIndex = numRavenMechs - 1;

    for (int32_t entry = numRaven - 1; entry >= 0; entry--)
    {
        if (ravenIsMech[entry] != 0)
        {
            LogMech* mech = nullptr;
            forceMechList->getMechInfo(entry, mech);
            mech->pilotIndex = pilotIndex--;
        }
    }

    // They go in the third lance when it is empty.
    int32_t ravenLance = -1;
    int32_t lanceSum = 0;

    for (const DeploySlot& slot : deploySlots[2])
    {
        lanceSum += slot.unit + slot.vehicle;
    }

    if (lanceSum == -8)
    {
        ravenLance = 2;
    }

    if (ravenParts[0] != -1)
    {
        Assert(ravenLance != -1, 0xffffffff, " No open lance for Raven System Vehicles/Mechs ", nullptr);
    }

    int32_t mechIndex = numRaven - numRavenVehicles - 1;
    int32_t vehicleIndex = numRaven - numRavenMechs - 1;

    for (int32_t entry = numRaven - 1; entry >= 0; entry--)
    {
        if (ravenIsMech[entry] == 0)
        {
            deploySlots[ravenLance][entry].vehicle = vehicleIndex--;
        }
        else
        {
            deploySlots[ravenLance][entry].unit = mechIndex--;
        }
    }

    // The force's pilots (vehicle crews from their profiles). The salvage went to the head of the lists, so the
    // other lances' indexes are past it.
    int32_t warriorNumber = firstForceWarrior;

    for (int32_t lance = 0; lance < 3; lance++)
    {
        for (int32_t slot = 0; slot < 4; slot++)
        {
            const DeploySlot& deploy = deploySlots[lance][slot];

            if (deploy.unit < 0)
            {
                if (deploy.vehicle < 0)
                {
                    continue;
                }

                const int32_t offset = lance != ravenLance ? numRavenVehicles : 0;
                std::snprintf(blockName, sizeof(blockName), "Warrior%d", warriorNumber++);
                result = out.writeBlock(blockName);
                Assert(result > 0, static_cast<uint32_t>(lance), " Could not write Warrior Number Block ", nullptr);
                FitIniFile crewFile;
                LogVehicle* vehicle = nullptr;
                forceVehicleList->getVehicleInfo(deploy.vehicle + offset, vehicle);
                result = out.writeIdString("Profile", vehicle->crew);
                Assert(result > 0, 0, " Could not write Warrior Profile in Warrior Number Block ", nullptr);
                std::snprintf(text, sizeof(text), "%s%s.fit", warriorPath, vehicle->crew);
                result = crewFile.open(text);
                check(result == 0, " Could not open vehicle profile");
                result = crewFile.seekBlock("General");
                check(result == 0, " Could not find General block in vehicle crew profile");
                result = crewFile.readIdString("Brain", text, 0xff);
                check(result == 0, " Could not read brain in vehicle crew profile");
                result = out.writeIdString("Brain", text);
                check(result > 0, " Could not write Warrior Brain in Warrior Number Block ");
                crewFile.close();
            }
            else
            {
                const int32_t offset = lance != ravenLance ? numRavenMechs : 0;
                std::snprintf(blockName, sizeof(blockName), "Warrior%d", warriorNumber++);
                result = out.writeBlock(blockName);
                Assert(result > 0, static_cast<uint32_t>(lance), " Could not write Warrior Number Block ", nullptr);
                const int32_t mech = deploy.unit + offset;
                // Original behaviour: the pilot's id is passed where the profile and brain getters take a position.
                int32_t id = assignedWarriorList->getID(forceMechList->getMechPilotIndex(mech) + offset);
                assignedWarriorList->getWarriorProfile(static_cast<uint32_t>(id), text);
                result = out.writeIdString("Profile", text);
                Assert(result > 0, 0, " Could not write Warrior Profile in Warrior Number Block ", nullptr);
                id = assignedWarriorList->getID(forceMechList->getMechPilotIndex(mech) + offset);
                assignedWarriorList->getWarriorBrain(static_cast<uint32_t>(id), text);
                result = out.writeIdString("Brain", text);
                check(result > 0, " Could not write Warrior Brain in Warrior Number Block ");
                LogWarrior* warrior = nullptr;
                assignedWarriorList->getWarriorInfo(forceMechList->getMechPilotIndex(mech), warrior);
                out.writeIdBoolean("NotMineYet", warrior->notMineYet);
            }
        }
    }

    // The force's parts: at the drop zone's slot offsets (the salvage where it stands, inactive).
    int32_t pilotNumber = 0;
    int32_t partNumber = firstForcePart;

    for (int32_t lance = 0; lance < 3; lance++)
    {
        for (int32_t slot = 0; slot < 4; slot++)
        {
            const DeploySlot& deploy = deploySlots[lance][slot];

            if (deploy.unit < 0 && deploy.vehicle < 0)
            {
                continue;
            }

            std::snprintf(blockName, sizeof(blockName), "Part%d", partNumber);
            result = out.writeBlock(blockName);
            check(result > 0, " Could not write PartNumber Block ");
            result = out.writeIdULong("ControlType", 2);
            check(result > 0, " Could not write ControlType in PartNumber Block ");
            LogPart* part;

            if (deploy.unit < 0)
            {
                LogVehicle* vehicle = nullptr;
                forceVehicleList->getVehicleInfo(deploy.vehicle + (lance != ravenLance ? numRavenVehicles : 0),
                                                 vehicle);
                Assert(vehicle != nullptr, 0, " Could not get vehicle pointer ", nullptr);
                result = out.writeIdULong("ControlDataType", 2);
                check(result > 0, " Could not write ControlDataType in PartNumber Block ");
                part = vehicle;
            }
            else
            {
                LogMech* mech = nullptr;
                forceMechList->getMechInfo((lance != ravenLance ? numRavenMechs : 0) + deploy.unit, mech);
                Assert(mech != nullptr, 0, " Could not get mech pointer ", nullptr);
                result = out.writeIdULong("ControlDataType", 1);
                check(result > 0, " Could not write ControlDataType in PartNumber Block ");
                part = mech;
            }

            // The part remembers its number (for the team's Mates below).
            part->unknown3C = partNumber++;
            result = out.writeIdULong("ObjectNumber", part->chassis);
            check(result > 0, " Could not write ObjectNumber in PartNumber Block ");
            result = out.writeIdString("ObjectProfile", part->profileName);
            check(result > 0, " Could not write ObjectProfile in PartNumber Block ");
            result = out.writeIdChar("TeamId", static_cast<char>(part->unknown68));
            check(result > 0, " Could not write TeamId in PartNumber Block ");
            result = out.writeIdChar("CommanderId", static_cast<char>(part->unknown68));
            check(result > 0, " Could not write CommanderId in PartNumber Block ");
            result = out.writeIdULong("Pilot", static_cast<uint32_t>(pilotNumber + firstForceWarrior));
            check(result > 0, " Could not write Pilot in PartNumber Block ");
            int32_t active;

            if (lance == ravenLance)
            {
                const PartPlace& place =
                    deploy.unit < 0 ? ravenVehiclePlaces[deploy.vehicle] : ravenMechPlaces[deploy.unit];
                result = out.writeIdFloat("PositionX", place.x);
                check(result > 0, " Could not write PositionX in PartNumber Block ");
                result = out.writeIdFloat("PositionY", place.y);
                check(result > 0, " Could not write PositionY in PartNumber Block ");
                result = out.writeIdFloat("PositionZ", -1.0f);
                check(result > 0, " Could not write PositionZ in PartNumber Block ");
                result = out.writeIdFloat("Rotation", place.rotation);
                check(result > 0, " Could not write Rotation in PartNumber Block ");
                result = out.writeIdULong("Gesture", 2);
                check(result > 0, " Could not write Gesture in PartNumber Block ");
                result = out.writeIdFloat("Velocity", 0.0f);
                check(result > 0, " Could not write Velocity in PartNumber Block ");
                active = 0;
            }
            else
            {
                const DeploySlotInfo& info = deploySlotInfo[lance][slot];
                result = out.writeIdFloat("PositionX", info.offsetX + dropZonePositions[lance].x);
                check(result > 0, " Could not write PositionX in PartNumber Block ");
                result = out.writeIdFloat("PositionY", info.offsetY + dropZonePositions[lance].y);
                check(result > 0, " Could not write PositionY in PartNumber Block ");
                result = out.writeIdFloat("PositionZ", -1.0f);
                check(result > 0, " Could not write PositionZ in PartNumber Block ");
                result = out.writeIdFloat("Rotation", info.rotation);
                check(result > 0, " Could not write Rotation in PartNumber Block ");
                result = out.writeIdULong("Gesture", 2);
                check(result > 0, " Could not write Gesture in PartNumber Block ");
                result = out.writeIdFloat("Velocity", 0.0f);
                check(result > 0, " Could not write Velocity in PartNumber Block ");
                active = 1;
            }

            result = out.writeIdLong("Active", active);
            check(result > 0, " Could not write Active Flag in PartNumber Block ");
            result = out.writeIdLong("Exists", 1);
            check(result > 0, " Could not write Exists Flag in PartNumber Block ");
            result = out.writeIdChar("MyIcon", 0);
            check(result > 0, " Could not write MyIcon in PartNumber Block ");
            pilotNumber++;
        }
    }

    result = in.seekBlock("Warriors");
    Assert(result == 0, 0, " Could not find Warriors Block ", nullptr);
    result = out.writeBlock("Warriors");
    Assert(result > 0, 0, " Could not write Warriors Block ", nullptr);
    uint8_t captureChance = 0;
    result = in.readIdUChar("CaptureChance", captureChance);
    Assert(result == 0, 0, " Could not read captureChance in Warriors Block ", nullptr);
    result = out.writeIdUChar("CaptureChance", captureChance);
    Assert(result > 0, 0, " Could not write captureChance in Warriors Block ", nullptr);
    result = out.writeIdULong("NumWarriors", static_cast<uint32_t>(warriorNumber - 1));
    check(result > 0, " Could not write NumWarriors in Warriors Block ");

    if (in.readIdString("BrainParameterFile", text, 0xff) == 0)
    {
        result = out.writeIdString("BrainParameterFile", text);
        check(result > 0, " could not write BrainParameterFile in Warriors Block ");
    }

    result = out.writeBlock("Parts");
    check(result > 0, " Could not write Parts Block ");
    result = out.writeIdULong("NumParts", static_cast<uint32_t>(partNumber - 1));
    check(result > 0, " Could not write NumParts in Parts Block ");

    // The player's commander groups: one per lance in use, from the first one.
    const auto lanceUsed = [&](int32_t lance)
    {
        int32_t sum = 0;

        for (const DeploySlot& slot : deploySlots[lance])
        {
            sum += slot.unit + slot.vehicle;
        }

        return sum != -8;
    };

    int32_t firstLance = 0;

    while (firstLance < 3 && !lanceUsed(firstLance))
    {
        firstLance++;
    }

    Assert(firstLance < 3, static_cast<uint32_t>(firstLance), " No Assigned Mechs for this Mission ", nullptr);
    int32_t groupNumber = 0;

    for (int32_t lance = firstLance; lance < 3; lance++)
    {
        if (!lanceUsed(lance))
        {
            continue;
        }

        std::snprintf(text, sizeof(text), "Commander0Group:%d", groupNumber);
        result = out.writeBlock(text);
        Assert(result > 0, static_cast<uint32_t>(lance), " could not write Commander0Groupx Team Block ", nullptr);

        for (int32_t& mate : mates)
        {
            mate = 0;
        }

        int32_t numMates = 0;

        for (int32_t slot = 0; slot < 4; slot++)
        {
            const DeploySlot& deploy = deploySlots[lance][slot];

            if (deploy.unit < 0 && deploy.vehicle < 0)
            {
                continue;
            }

            LogPart* part = nullptr;

            if (deploy.unit < 0)
            {
                LogVehicle* vehicle = nullptr;
                forceVehicleList->getVehicleInfo((lance != ravenLance ? numRavenVehicles : 0) + deploy.vehicle,
                                                 vehicle);
                part = vehicle;
            }
            else
            {
                LogMech* mech = nullptr;
                forceMechList->getMechInfo(deploy.unit + (lance != ravenLance ? numRavenMechs : 0), mech);
                part = mech;
            }

            if (part != nullptr)
            {
                mates[numMates++] = part->unknown3C;
            }
        }

        result = out.writeIdLongArray("Mates", mates, 12);
        check(result > 0, " could not write Mates in Inner Sphere Team Block ");
        groupNumber++;
    }

    if (in.seekBlock("Trains") == 0)
    {
        int32_t numTrains = 0;
        result = in.readIdLong("NumTrains", numTrains);
        check(result == 0, " Could not read numTrains ");
        out.writeBlock("Trains");
        out.writeIdLong("NumTrains", numTrains);

        for (int32_t train = 0; train < numTrains; train++)
        {
            std::snprintf(blockName, sizeof(blockName), "Train%d", train);
            result = in.seekBlock(blockName);
            check(result == 0, " could not find trainBlock ");
            int32_t numCars = 0;
            result = in.readIdLong("NumCars", numCars);
            check(result == 0, " could not find numCars ");
            out.writeBlock(blockName);
            out.writeIdLong("NumCars", numCars);

            for (int32_t car = 0; car < numCars; car++)
            {
                char carName[0x20];
                std::snprintf(carName, sizeof(carName), "Car%d", car);
                int32_t carPart = 0;
                result = in.readIdLong(carName, carPart);
                check(result == 0, " could not find carBlock ");
                out.writeIdLong(carName, carPart);
            }
        }
    }

    in.close();
    out.close();

    // And the logistics state the mission reads back: start<n>.fit for the next mission.
    std::snprintf(text, sizeof(text), "start%d", currentMission + 1);
    MissionLogisticsBridge bridge;
    result = bridge.logisticsStartingFitWriter(text, 0);
    Assert(result == 0, 0, " Could not save logistics data ", nullptr);
    return 0;
}

auto Logistics::setPilot(int32_t mechIndex, int32_t pilotIndex) -> void
{
    if (mechIndex >= forceMechList->getMechCount())
    {
        return;
    }

    LogMech* mech = nullptr;
    forceMechList->getMechInfo(mechIndex, mech);
    LogWarrior* warrior = nullptr;

    if (pilotIndex >= 0)
    {
        assignedWarriorList->getWarriorInfo(pilotIndex, warrior);
        warrior->inventoryBlock->mech = mech;
        mech->pilotIndex = pilotIndex;
        mech->repairBlock->setPilotStats(nullptr);
        mech->repairBlock->setPilotHealth(nullptr);
        mech->calcPilotModifier();
        return;
    }

    // A negative index takes the pilot off.
    assignedWarriorList->getWarriorInfo(mech->pilotIndex, warrior);
    warrior->inventoryBlock->mech = nullptr;
    mech->repairBlock->clearPilot();
    mech->pilotIndex = pilotIndex;
    mech->calcPilotModifier();
}

auto Logistics::reorderMechs() -> void
{
    // Assigned mechs move from the mech list to the head of the force list.
    LogMechList* all = mechList;
    LogMech* previous = nullptr;

    for (LogMech* mech = all->mechs; mech != nullptr;)
    {
        LogMech* next = mech->next;

        if (mech->assigned != 0)
        {
            if (previous == nullptr)
            {
                all->mechs = next;
            }
            else
            {
                previous->next = next;
            }

            LogMechList* force = forceMechList;
            mech->next = force->mechs;
            force->mechs = mech;
            all->numMechs--;
            force->numMechs++;
        }

        // Original behaviour (OB-097): the moved mech becomes the previous one, so a second assigned mech right
        // after it is unlinked through the force list.
        previous = mech;
        mech = next;
    }

    // Unassigned force mechs go back into the mech list in sortKey order. The scan position and the mech to insert
    // after carry over from one mech to the next.
    LogMechList* force = forceMechList;
    LogMech* scan = all->mechs;
    LogMech* insertAfter = nullptr;
    previous = nullptr;

    for (LogMech* mech = force->mechs; mech != nullptr;)
    {
        LogMech* next = mech->next;
        LogMech* before = previous;
        previous = mech;

        if (mech->assigned == 0)
        {
            while (scan != nullptr && scan->sortKey < mech->sortKey)
            {
                insertAfter = scan;
                scan = scan->next;
            }

            if (insertAfter == nullptr)
            {
                if (before == nullptr)
                {
                    force->mechs = force->mechs->next;
                }
                else
                {
                    before->next = next;
                }

                mech->next = all->mechs;
                all->mechs = mech;
            }
            else
            {
                if (before == nullptr)
                {
                    force->mechs = force->mechs->next;
                }
                else
                {
                    before->next = next;
                }

                mech->next = scan;
                insertAfter->next = mech;
            }

            all->numMechs++;
            force->numMechs--;
        }

        mech = next;
    }

    int32_t index = 0;

    for (LogMech* mech = all->mechs; mech != nullptr; mech = mech->next)
    {
        mech->inventoryBlock->listIndex = index++;
    }

    index = 0;

    for (LogMech* mech = force->mechs; mech != nullptr; mech = mech->next)
    {
        mech->inventoryBlock->listIndex = index++;
    }
}

auto Logistics::reorderVehicles() -> void
{
    // As reorderMechs, with the vehicle list sorted by tonnage.
    LogVehicleList* all = vehicleList;
    LogVehicle* previous = nullptr;

    for (LogVehicle* vehicle = all->vehicles; vehicle != nullptr;)
    {
        LogVehicle* next = vehicle->next;

        if (vehicle->assigned != 0)
        {
            if (previous == nullptr)
            {
                all->vehicles = next;
            }
            else
            {
                previous->next = next;
            }

            LogVehicleList* force = forceVehicleList;
            vehicle->next = force->vehicles;
            force->vehicles = vehicle;
            all->numVehicles--;
            force->numVehicles++;
        }

        // Original behaviour (OB-097): as in reorderMechs.
        previous = vehicle;
        vehicle = next;
    }

    LogVehicleList* force = forceVehicleList;
    LogVehicle* scan = all->vehicles;
    LogVehicle* insertAfter = nullptr;
    previous = nullptr;

    for (LogVehicle* vehicle = force->vehicles; vehicle != nullptr;)
    {
        LogVehicle* next = vehicle->next;
        LogVehicle* before = previous;
        previous = vehicle;

        if (vehicle->assigned == 0)
        {
            for (; scan != nullptr && scan->curTonnage < vehicle->curTonnage; scan = scan->next)
            {
                insertAfter = scan;
            }

            if (insertAfter == nullptr)
            {
                if (before == nullptr)
                {
                    force->vehicles = force->vehicles->next;
                }
                else
                {
                    before->next = next;
                }

                vehicle->next = all->vehicles;
                all->vehicles = vehicle;
            }
            else
            {
                if (before == nullptr)
                {
                    force->vehicles = force->vehicles->next;
                }
                else
                {
                    before->next = next;
                }

                vehicle->next = scan;
                insertAfter->next = vehicle;
            }

            force->numVehicles--;
            all->numVehicles++;
        }

        vehicle = next;
    }

    int32_t index = 0;

    for (LogVehicle* vehicle = all->vehicles; vehicle != nullptr; vehicle = vehicle->next)
    {
        vehicle->inventoryBlock->listIndex = index++;
    }

    index = 0;

    for (LogVehicle* vehicle = force->vehicles; vehicle != nullptr; vehicle = vehicle->next)
    {
        vehicle->inventoryBlock->listIndex = index++;
    }
}

auto Logistics::reorderWarriors() -> void
{
    // Assigned pilots move into the assigned list in rank order.
    LogWarriorList* all = warriorList;
    LogWarriorList* assigned = assignedWarriorList;
    LogWarrior* scan = assigned->warriors;
    LogWarrior* insertAfter = nullptr;
    LogWarrior* previous = nullptr;

    for (LogWarrior* warrior = all->warriors; warrior != nullptr;)
    {
        LogWarrior* next = warrior->next;
        LogWarrior* before = previous;
        previous = warrior;

        if (warrior->assigned != 0)
        {
            while (scan != nullptr && scan->rank < warrior->rank)
            {
                insertAfter = scan;
                scan = scan->next;
            }

            if (insertAfter == nullptr)
            {
                if (before == nullptr)
                {
                    all->warriors = all->warriors->next;
                }
                else
                {
                    before->next = next;
                }

                warrior->next = assigned->warriors;
                assigned->warriors = warrior;
            }
            else
            {
                if (before == nullptr)
                {
                    all->warriors = all->warriors->next;
                }
                else
                {
                    before->next = next;
                }

                warrior->next = scan;
                insertAfter->next = warrior;
            }

            all->numWarriors--;
            assigned->numWarriors++;
        }

        // Original behaviour (OB-097): the moved pilot becomes the previous one, as in reorderMechs.
        warrior = next;
    }

    // Unassigned pilots go back into the pilot list by rank, then callsign.
    scan = all->warriors;
    insertAfter = nullptr;
    previous = nullptr;

    for (LogWarrior* warrior = assigned->warriors; warrior != nullptr;)
    {
        LogWarrior* next = warrior->next;
        LogWarrior* before = previous;
        previous = warrior;

        if (warrior->assigned == 0)
        {
            LogWarrior* after = insertAfter;

            while (scan != nullptr && scan->rank < warrior->rank)
            {
                insertAfter = scan;
                after = scan;
                scan = scan->next;
            }

            if (scan != nullptr)
            {
                // Stopped on an equal or higher rank: pass the pilots of the same rank whose callsign sorts first.
                insertAfter = after;

                while (scan != nullptr && std::strcmp(scan->callsign, warrior->callsign) < 0 &&
                       scan->rank == warrior->rank)
                {
                    insertAfter = scan;
                    scan = scan->next;
                }
            }

            if (insertAfter == nullptr)
            {
                if (before == nullptr)
                {
                    assigned->warriors = assigned->warriors->next;
                }
                else
                {
                    before->next = next;
                }

                LogWarrior* head = all->warriors;
                all->warriors = warrior;
                warrior->next = head;
            }
            else
            {
                if (before == nullptr)
                {
                    assigned->warriors = assigned->warriors->next;
                }
                else
                {
                    before->next = next;
                }

                warrior->next = scan;
                insertAfter->next = warrior;
            }

            all->numWarriors++;
            assigned->numWarriors--;
        }

        warrior = next;
    }

    int32_t index = 0;

    for (LogWarrior* warrior = all->warriors; warrior != nullptr; warrior = warrior->next)
    {
        warrior->inventoryBlock->listIndex = index++;
    }

    index = 0;

    for (LogWarrior* warrior = assigned->warriors; warrior != nullptr; warrior = warrior->next)
    {
        warrior->inventoryBlock->listIndex = index++;
    }
}

auto Logistics::shiftPilots(int32_t from, int32_t amount) -> void
{
    for (LogMech* mech = forceMechList->mechs; mech != nullptr; mech = mech->next)
    {
        if (from <= mech->pilotIndex)
        {
            mech->pilotIndex += amount;
        }
    }
}

auto Logistics::requiredAssigned() -> int
{
    if (multiplayerInitialized != 0)
    {
        return 1;
    }

    int allThere = 1;

    // Every required mech needs a deployed mech of its chassis in the force, and every required force mech has to
    // be deployed.
    for (LogMech* mech = mechList->mechs; mech != nullptr; mech = mech->next)
    {
        if (mech->required == 0)
        {
            continue;
        }

        LogMech* found = forceMechList->mechs;

        while (found != nullptr && !(mech->chassis == found->chassis && found->deployed != 0))
        {
            found = found->next;
        }

        if (found == nullptr)
        {
            allThere = 0;
        }
    }

    for (LogMech* mech = forceMechList->mechs; mech != nullptr; mech = mech->next)
    {
        if (mech->required != 0 && mech->deployed == 0)
        {
            allThere = 0;
        }
    }

    if (allThere == 0)
    {
        return 0;
    }

    // The same for vehicles, except that a force vehicle of the chassis counts whether it is deployed or not.
    for (LogVehicle* vehicle = vehicleList->vehicles; vehicle != nullptr; vehicle = vehicle->next)
    {
        if (vehicle->required == 0)
        {
            continue;
        }

        LogVehicle* found = forceVehicleList->vehicles;

        while (found != nullptr && vehicle->chassis != found->chassis)
        {
            found = found->next;
        }

        if (found == nullptr)
        {
            allThere = 0;
        }
    }

    for (LogVehicle* vehicle = forceVehicleList->vehicles; vehicle != nullptr; vehicle = vehicle->next)
    {
        if (vehicle->required != 0 && vehicle->deployed == 0)
        {
            allThere = 0;
        }
    }

    return allThere;
}

auto Logistics::getCurrentMission() -> void
{
    // Port: the original allocated the FitIniFile and leaked it when the mission file would not open.
    FitIniFile file;
    FullPathFileName path;
    char* fileName = MPlayer == nullptr ? mission->scenarios[mission->currentScenario] : mpMissionName;
    path.init(missionPath, fileName, ".fit");

    if (file.open(path) != 0)
    {
        return;
    }

    int32_t result = file.seekBlock("Campaign");
    Assert(result == 0, 0, " Could not find Campaign block in mission file ", nullptr);
    result = file.readIdLong("MaxTonnage", maxDeployTonnage);
    Assert(result == 0, 0, " Could not find MaxTonnage variable in mission file ", nullptr);
    char briefingFile[0x80];

    if (MPlayer == nullptr)
    {
        result = file.readIdString("BriefingFile", briefingFile, 0x7f);
    }
    else
    {
        // Each player on the team gets an equal share of the tonnage.
        maxDeployTonnage /= MPlayer->playersOnHomeTeam()->count;
        char variable[64];
        std::snprintf(variable, sizeof(variable), "%s", MPlayer->homeTeam == 0 ? "ISBriefingFile" : "ClanBriefingFile");
        result = file.readIdString(variable, briefingFile, 0x7f);
    }

    Assert(result == 0, 0, " Could not find BriefingFile variable in mission file ", nullptr);

    // Format the briefing text into a port the width of the mission pane (at least 0xbf high), then copy it into
    // the briefing screen's mission port with a 2-pixel margin.
    BriefingScreen* briefing = briefingScreen;
    const int32_t paneWidth = briefing->missionPane->width();
    char text[0x100];
    std::snprintf(text, sizeof(text), "%s%s", missionPath, briefingFile);
    int32_t height = application->textFormatter.init(text, nullptr, paneWidth - 0x11);
    auto* textPort = new lPort;

    if (height < 0xbf)
    {
        height = 0xbf;
    }

    textPort->init(paneWidth - 0x11, height, 1);
    VFX_pane_wipe(textPort->frame(), 0xff);
    application->textFormatter.init(text, textPort, 0);
    delete briefing->missionPort;
    briefing->missionPort = new lPort;
    briefing->missionPort->init(0xb3, height + 10, 1);
    VFX_pane_wipe(briefing->missionPort->frame(), 0x10);
    VFX_pane_copy(textPort->frame(), 0, 0, briefing->missionPort->frame(), 2, 2, -1);
    delete textPort;

    result = file.readIdString("MapFile", text, 0xff);
    Assert(result == 0, 0, " Could not find MapFile variable in mission file ", nullptr);

    if (missionFileName != nullptr)
    {
        logFree(missionFileName);
    }

    missionFileName = logStrDup(text);

    int32_t numDropZones = 0;
    result = file.readIdLong("NumDropZones", numDropZones);
    Assert(result == 0, 0, " Could not read NumDropZones variable in mission file ", nullptr);

    if (MPlayer == nullptr)
    {
        for (int32_t& slot : localDropSlot)
        {
            slot = 0;
        }
    }

    Assert(numDropZones < 7, 0, "Too many drop zones", nullptr);

    for (int32_t zone = 0; zone < numDropZones; zone++)
    {
        std::snprintf(text, sizeof(text), "DropZone%d", zone);
        file.seekBlock(text);
        int32_t numSlots = 0;
        result = file.readIdLong("NumSlots", numSlots);
        Assert(result == 0, 0, " Could not read NumSlots variable in mission file ", nullptr);

        // Single player: the zone's slots are the ones the player may fill (a zone is a lance of four).
        // Port fix: the original wrote the marks of a fourth or later zone past localDropSlot, over the drop zone
        // positions already read; the port only marks the three lances.
        if (MPlayer == nullptr && zone < 3)
        {
            for (int32_t slot = 0; slot < numSlots; slot++)
            {
                localDropSlot[zone * 4 + slot] = 1;
            }
        }

        result = file.readIdFloat("PositionX", dropZonePositions[zone].x);
        Assert(result == 0, 0, " Could not read PositionX variable in mission file ", nullptr);
        result = file.readIdFloat("PositionY", dropZonePositions[zone].y);
        Assert(result == 0, 0, " Could not read PositionY variable in mission file ", nullptr);

        for (int32_t slot = 0; slot < 4; slot++)
        {
            if (MPlayer == nullptr && (zone >= 3 || localDropSlot[zone * 4 + slot] == 0))
            {
                break;
            }

            DeploySlotInfo& info = deploySlotInfo[zone][slot];
            std::snprintf(text, sizeof(text), "OffsetX%d", slot);
            result = file.readIdFloat(text, info.offsetX);
            Assert(result == 0, 0, " Could not read OffsetX block in mission file ", nullptr);
            std::snprintf(text, sizeof(text), "OffsetY%d", slot);
            result = file.readIdFloat(text, info.offsetY);
            Assert(result == 0, 0, " Could not read OffsetY block in mission file ", nullptr);
            std::snprintf(text, sizeof(text), "Rotation%d", slot);
            result = file.readIdFloat(text, info.rotation);
            Assert(result == 0, 0, " Could not read Rotation block in mission file ", nullptr);
        }
    }

    file.close();
    briefingScreen->drawBackground();
}

namespace
{
    /// <summary>
    /// Port: the pane a screen change slides over the screen's right part (<see cref="Logistics::transition"/>). The
    /// original wrote both pictures into its own picture each frame; it draws them from the slide's state instead.
    /// </summary>
    class TransitionWipe : public lObject
    {
    public:
        /// <summary>Draws the two pictures as the slide stands (the original's loop body).</summary>
        void draw() override
        {
            if (!lport()->viewOpen())
            {
                return;
            }

            _pane* target = lport()->frame();

            if (direction == 0)
            {
                from->copyTo(target, 0, 0, 1);
                VFX_pane_copy(to->frame(), 0x1ab - offset, 0, target, 0, 0, -1);
            }
            else
            {
                to->copyTo(target, 0, 0, 1);
                VFX_pane_copy(from->frame(), offset, 0, target, 0, 0, -1);
            }
        }

        /// <summary>The wipe draws itself each frame (its port is a view).</summary>
        bool DrawsLive() override { return true; }

        /// <summary>The screen shown before the change, and the one after.</summary>
        lPort* from = nullptr;
        lPort* to = nullptr;
        /// <summary>0: the new picture slides in from the right; otherwise the old one slides out to the left.</summary>
        int direction = 0;
        /// <summary>How far the slide has gone, in pixels.</summary>
        int32_t offset = 0;
    };
}

auto Logistics::transition(lPort* from, lPort* to, int direction) -> void
{
    // A pane over the screen's right part, redrawn each frame for a quarter of a second: direction 0 slides the new
    // picture in from the right over the old one, any other slides the old one out to the left off the new one.
    auto* wipe = new TransitionWipe;
    wipe->init(0xd3, 0x10, from->width(), from->height(), nullptr, nullptr);
    wipe->from = from;
    wipe->to = to;
    wipe->direction = direction;
    currentScreen->addChild(wipe);
    wipe->ShowGUIWindow(1);
    wipe->setDepth(100);
    soundSystem->playDigitalSample(0x36, 1, nullptr, 0, 0);
    const int64_t frequency = MCPort::PerformanceFrequency();
    float elapsed = 0.0f;

    do
    {
        const int64_t start = MCPort::PerformanceCounter();
        wipe->offset = static_cast<int32_t>(static_cast<double>(elapsed) * 4.0 * 427.0);
        UpdateDisplay(0, 0, 0, 0, 0);
        const int64_t end = MCPort::PerformanceCounter();
        // The original divided the low 32 bits of the tick difference by the low 32 bits of the frequency.
        const auto ticks = static_cast<uint32_t>(end - start);
        elapsed = static_cast<float>(static_cast<double>(ticks) / static_cast<int32_t>(frequency) + elapsed);
    } while (elapsed < 0.25);

    wipe->destroy();
    delete wipe;
}

auto Logistics::darken(int32_t amount, char* fadeTable, lPort* port) -> void
{
    // Darkens row block amount (of the port's height) through the fade table; with no port, the repair screen's
    // unit pane (0x19d x 0x70 blocks).
    int32_t width;
    int32_t height;

    if (port == nullptr)
    {
        repairScreen->unitPane->getDisplayPort(port);
        width = 0x19d;
        height = 0x70;
    }
    else
    {
        height = port->height();
        width = port->width();
    }

    lPort* scratch = newPort(width, height);
    const int32_t yPos = height * amount;
    VFX_pane_copy(port->frame(), 0, yPos, scratch->frame(), 0, 0, -1);
    SCRNVERTEX corners[4] = {};
    corners[1].x = width - 1;
    corners[2].x = width - 1;
    corners[2].y = height - 1;
    corners[3].y = height - 1;
    VFX_translate_polygon(scratch->frame(), 4, corners, fadeTable);
    VFX_pane_copy(scratch->frame(), 0, 0, port->frame(), 0, yPos, -1);
    delete scratch;
}

auto Logistics::reIndexInventory() -> int32_t
{
    // Give each component with copies the next inventory row, in the order of the widgets' inventory indexes;
    // components with none get -1.
    const int32_t numItems = componentInventory->numItems;
    int32_t row = 0;

    if (numItems < 1)
    {
        return 0;
    }

    for (int32_t index = 0; index < numItems; index++)
    {
        _LogInventoryItem* item = componentInventory->items;

        while (item != nullptr && item->inventoryBlock->inventoryIndex != index)
        {
            item = item->next;
        }

        Assert(item != nullptr, 0, "Could not reindex player inventory. Probably an old savegame", nullptr);

        if (item->count == 0)
        {
            item->inventoryBlock->listIndex = -1;
        }
        else
        {
            item->inventoryBlock->listIndex = row++;
        }
    }

    return row;
}

auto Logistics::removeReorderPilotIndexes(LogMech* mech, int32_t removedPilot) -> void
{
    // No this is used (the original is a plain function at this address).
    for (; mech != nullptr; mech = mech->next)
    {
        if (removedPilot < mech->pilotIndex)
        {
            mech->pilotIndex--;
        }
    }
}

namespace
{
    /// <summary>
    /// The text an old-iostream <c>ofstream</c> would have written: strings as they are, integers in decimal,
    /// doubles as <c>%.6g</c> (the default precision), and every line end as CR LF (text mode). The original wrote
    /// the multiplayer start file and nomechlist.log this way; the port collects the text and writes it through
    /// <see cref="File"/>.
    /// </summary>
    class TextStream
    {
    public:
        TextStream& operator<<(const char* text)
        {
            _Text += text;
            return *this;
        }

        TextStream& operator<<(char character)
        {
            _Text += character;
            return *this;
        }

        TextStream& operator<<(int value)
        {
            _Text += std::to_string(value);
            return *this;
        }

        TextStream& operator<<(unsigned long value)
        {
            _Text += std::to_string(value);
            return *this;
        }

        TextStream& operator<<(double value)
        {
            char text[64];
            std::snprintf(text, sizeof(text), "%.6g", value);
            _Text += text;
            return *this;
        }

        /// <summary>Writes the text to <paramref name="fileName"/>, silently doing nothing when it can't be created.</summary>
        void writeFile(const char* fileName) const
        {
            std::string text;
            text.reserve(_Text.size() + _Text.size() / 16);

            for (const char character : _Text)
            {
                if (character == '\n')
                {
                    text += '\r';
                }

                text += character;
            }

            File file;

            if (file.create(fileName) != 0)
            {
                return;
            }

            file.write(reinterpret_cast<const uint8_t*>(text.data()), static_cast<int32_t>(text.size()));
            file.close();
        }

    private:
        std::string _Text;
    };

#pragma pack(push, 1)
    /// <summary>
    /// A "deploy force" message (MPMSG_DEPLOY_FORCE): a mech or vehicle placed in a drop slot, with its pilot and
    /// components. Sent as <c>numItems * 2 + 0xd</c> bytes. The struct name is the port's.
    /// </summary>
    struct DeployForceMessage : public FIGuaranteedMessageHeader
    {
        /// <summary>
        /// Bit 0 a mech (else a vehicle), bit 1 the Clan side, bits 2-3 the mech's name variant, bits 4-5 the lance,
        /// bits 6-7 the slot.
        /// </summary>
        uint8_t flags; // +0x8
        /// <summary>The part's name index (the variant is added from the flags for mechs).</summary>
        uint8_t nameIndex; // +0x9
        /// <summary>The pilot's name index (0xff for a vehicle).</summary>
        uint8_t pilotNameIndex; // +0xa
        /// <summary>Always 0xff.</summary>
        uint8_t unknown0B; // +0xb
        uint8_t numItems;  // +0xc
        /// <summary>Per component copy, its master id as a 16-bit value (low byte first).</summary>
        uint8_t items[1]; // +0xd
    };

    static_assert(sizeof(DeployForceMessage) == 0xe);

    /// <summary>A "remove force" message (MPMSG_REMOVE_FORCE): a drop slot emptied. 10 bytes; the name is the port's.</summary>
    struct RemoveForceMessage : public FIGuaranteedMessageHeader
    {
        uint8_t slot;  // +0x8
        uint8_t lance; // +0x9
    };

    static_assert(sizeof(RemoveForceMessage) == 10);
#pragma pack(pop)

    /// <summary>
    /// Item <paramref name="index"/> of a net name list (<c>netmechs.rsp</c>, ...), or null past the end, as the
    /// original walked the links.
    /// </summary>
    char* netListItem(FLinkedList<char>& list, uint32_t index)
    {
        FLink<char>* link = list.head;

        if (link == nullptr)
        {
            return nullptr;
        }

        for (; index != 0; index--)
        {
            if (link->next == nullptr)
            {
                return nullptr;
            }

            link = link->next;
        }

        return link->data;
    }

    /// <summary>Whether the local player's team is the given side's group (the flags' Clan bit picks the side).</summary>
    uint32_t sideGroupID(uint8_t flags)
    {
        return (flags & 2) == 0 ? MPlayer->innerSphereGroupID : MPlayer->clanGroupID;
    }

    /// <summary>Appends a copy of every component of <paramref name="inventory"/> to a deploy message.</summary>
    /// <returns>The number of copies written.</returns>
    uint32_t writeDeployItems(DeployForceMessage* message, InventoryList* inventory)
    {
        uint32_t count = 0;

        for (_LogInventoryItem* item = inventory->items; item != nullptr; item = item->next)
        {
            for (_LogInventoryStat* stat = item->stats; stat != nullptr; stat = stat->next)
            {
                message->items[count * 2] = item->masterID;
                message->items[count * 2 + 1] = 0;
                count++;
            }
        }

        return count;
    }

    /// <summary>Replaces the inventory of a part made from a deploy message with the message's components.</summary>
    void readDeployItems(LogPart* part, const DeployForceMessage* message)
    {
        if (part->inventory != nullptr)
        {
            part->inventory->destroy();
            delete part->inventory;
        }

        auto* inventory = new InventoryList;
        part->inventory = inventory;
        const uint32_t numItems = message->numItems;

        for (uint32_t index = 0; index < numItems; index++)
        {
            _LogInventoryStat* stat = inventory->createStat(static_cast<uint8_t>(index), 0, 0, 0, 0, 1, 0xff);
            inventory->addItem(message->items[index * 2], stat, -1);
        }
    }
}

auto Logistics::HandleDeployForceMessage(uint32_t playerID, const void* message) -> void
{
    const auto* deploy = static_cast<const DeployForceMessage*>(message);
    int teammate = 0;

    if (multiplayerInitialized == 0)
    {
        initializeMultiplayer();
    }

    Assert(playerID != MPlayer->sessionManager->myPlayer->id, 0, "Got a deploy message from ourselves!", nullptr);
    // The sender is a teammate when the message's side is ours.
    const uint32_t homeGroup = MPlayer->homeTeamGroupID;

    if ((deploy->flags & 2) == 0)
    {
        if (homeGroup == MPlayer->innerSphereGroupID)
        {
            teammate = 1;
        }
    }
    else if (homeGroup == MPlayer->clanGroupID)
    {
        teammate = 1;
    }

    Assert(homeGroup == MPlayer->innerSphereGroupID || homeGroup == MPlayer->clanGroupID, 0,
           "Local player is not on a team!", nullptr);

    uint32_t lance = (deploy->flags >> 4) & 3;
    uint32_t slot = deploy->flags >> 6;
    LogPart* part;

    if ((deploy->flags & 1) == 0)
    {
        LogVehicleList* list = FindMPVehicleList(playerID, teammate);
        part = AddVehicleFromNetworkMessage(list, reinterpret_cast<FIMessageHeader*>(const_cast<void*>(message)));
        part->localPart = 0;

        if (teammate != 0)
        {
            auto* vehicle = static_cast<LogVehicle*>(part);
            auto* block = new MechBriefBlock;
            BriefingScreen* briefing = briefingScreen;
            vehicle->briefBlock = block;
            const RECT& rect = briefing->slotRects[lance * 4 + slot];
            block->init(vehicle, briefing, rect.left, rect.top);
        }
    }
    else
    {
        LogMechList* list = FindMPMechList(playerID, teammate, nullptr);
        part = AddMechFromNetworkMessage(list, reinterpret_cast<FIMessageHeader*>(const_cast<void*>(message)));
        part->localPart = 0;

        if (teammate != 0)
        {
            auto* mech = static_cast<LogMech*>(part);
            mech->calcStatus();
            auto* block = new MechBriefBlock;
            BriefingScreen* briefing = briefingScreen;
            mech->briefBlock = block;
            const RECT& rect = briefing->slotRects[lance * 4 + slot];
            block->init(mech, briefing, rect.left, rect.top);
        }
    }

    const int32_t commander = MPlayer->sessionManager->GetPlayer(playerID)->playerNumber;
    part->commanderID = commander;
    Assert(commander != MPlayer->checkInId, 0, "Wrong commander!", nullptr);
    lance = (deploy->flags >> 4) & 3;
    slot = deploy->flags >> 6;
    part->dropLance = lance;
    part->dropSlot = slot;

    if (teammate == 0)
    {
        opponentDropSlots[lance][slot]->part = part;
        return;
    }

    dropSlots[lance][slot]->part = part;
    briefingScreen->mpCalcTonnages();
}

auto Logistics::HandleRemoveForceMessage(uint32_t playerID, const void* message) -> void
{
    const auto* remove = static_cast<const RemoveForceMessage*>(message);
    const int teammate = MPlayer->isMyTeammate(playerID);
    RemoveForceAtDropSlot(remove->slot + remove->lance * 4, playerID, teammate);
}

auto Logistics::HandleChatMessage(uint32_t playerID, const void* message) -> void
{
    // Blink the chat button of the screen being shown (unless the briefing is on its operation tab, where the chat
    // is open), and play the chat sound.
    lObject* shown = currentScreen;
    BriefingScreen* briefing = briefingScreen;

    if (shown != briefing || briefing->currentTab != 1)
    {
        if (shown == briefing && briefing->chatTimerOn == 0)
        {
            application->AddTimer(briefing, 5, 0xfa, 0, 0, 0);
            briefing->chatTimerOn = 1;
        }
        else if (shown == purchaseScreen && purchaseScreen->chatBlinking == 0)
        {
            application->AddTimer(purchaseScreen, 7, 0xfa, 0, 0, 0);
            purchaseScreen->chatBlinking = 1;
        }
        else if (shown == repairScreen && repairScreen->chatBlinking == 0)
        {
            application->AddTimer(repairScreen, 8, 0xfa, 0, 0, 0);
            repairScreen->chatBlinking = 1;
        }

        briefing->chatBlinking = 1;
        soundSystem->playDigitalSample(0x14, 1, nullptr, 0, 0);
    }

    chatWindow->handleNetworkMessage(playerID, const_cast<void*>(message));
}

auto Logistics::SendRemoveForceMessage(int lance, int slot) -> void
{
    if (multiplayerInitialized == 0 || MPlayer == nullptr)
    {
        return;
    }

    auto* message = reinterpret_cast<RemoveForceMessage*>(messageBuffer);
    std::memset(message, 0, 8);
    message->lance = static_cast<uint8_t>(lance);
    message->slot = static_cast<uint8_t>(slot);
    message->header = FIMSG_GUARANTEED | MPMSG_REMOVE_FORCE;
    MPlayer->sessionManager->SendMessageToGroup(0, message, sizeof(RemoveForceMessage));
}

auto Logistics::SendAddMechMessage(LogMech* mech, int lance, int slot) -> void
{
    if (multiplayerInitialized == 0 || MPlayer == nullptr)
    {
        return;
    }

    auto* message = reinterpret_cast<DeployForceMessage*>(messageBuffer);
    message->header = 0;
    message->tagger.Clear();
    message->header = FIMSG_GUARANTEED | MPMSG_DEPLOY_FORCE;
    message->nameIndex = 0;
    message->pilotNameIndex = 0xff;
    message->unknown0B = 0xff;
    message->numItems = 0;
    message->flags = 1;
    const uint32_t homeGroup = MPlayer->homeTeamGroupID;
    Assert(homeGroup == MPlayer->innerSphereGroupID || homeGroup == MPlayer->clanGroupID, 0,
           "Local player is not on a team!", nullptr);
    message->flags = (MPlayer->homeTeamGroupID != MPlayer->innerSphereGroupID ? 2 : 0) + 1;
    LogWarrior* pilot = nullptr;
    assignedWarriorList->getWarriorInfo(mech->pilotIndex, pilot);

    if (mech->nameVariant < 4)
    {
        message->flags = static_cast<uint8_t>((message->flags & 0xf3) | (mech->nameVariant << 2));
    }

    if (lance < 4)
    {
        message->flags = static_cast<uint8_t>((message->flags & 0xcf) | (lance << 4));
    }

    if (slot < 4)
    {
        message->flags = static_cast<uint8_t>((message->flags & 0x3f) | (slot << 6));
    }

    message->nameIndex = static_cast<uint8_t>(mech->nameIndex);
    message->numItems = mech->inventory->nextStatID;
    message->pilotNameIndex = static_cast<uint8_t>(pilot->nameIndex);
    const uint32_t count = writeDeployItems(message, mech->inventory);
    message->numItems = static_cast<uint8_t>(count);
    MPlayer->sessionManager->SendMessageToGroup(0, message, (count & 0xff) * 2 + 0xd);
}

auto Logistics::SendAddVehicleMessage(LogVehicle* vehicle, int lance, int slot) -> void
{
    if (multiplayerInitialized == 0 || MPlayer == nullptr)
    {
        return;
    }

    auto* message = reinterpret_cast<DeployForceMessage*>(messageBuffer);
    message->header = 0;
    message->tagger.Clear();
    message->nameIndex = 0;
    message->numItems = 0;
    message->flags = 0;
    message->header = FIMSG_GUARANTEED | MPMSG_DEPLOY_FORCE;
    const uint8_t side = MPlayer->homeTeamGroupID != MPlayer->innerSphereGroupID ? 2 : 0;
    message->pilotNameIndex = 0xff;
    message->unknown0B = 0xff;
    message->flags = side;

    if (lance < 4)
    {
        message->flags = static_cast<uint8_t>(side | (lance << 4));
    }

    if (slot < 4)
    {
        message->flags = static_cast<uint8_t>((message->flags & 0x3f) | (slot << 6));
    }

    message->nameIndex = static_cast<uint8_t>(vehicle->nameIndex);
    message->numItems = vehicle->inventory->nextStatID;
    const uint32_t count = writeDeployItems(message, vehicle->inventory);
    message->numItems = static_cast<uint8_t>(count);
    MPlayer->sessionManager->SendMessageToGroup(0, message, (count & 0xff) * 2 + 0xd);
}

auto Logistics::handleLostPlayer(uint32_t playerID, int) -> void
{
    // "<player> has left the game" (or, in a lobby game, the variant that ends it).
    char text[256];
    cLoadString(thisInstance, launchedFromLobby == 0 || MPlayer == nullptr ? 0x35f : 0x365, text, 0xfe);
    std::snprintf(holdString, sizeof(holdString), "%s %s", MPlayer->sessionManager->GetPlayer(playerID)->name, text);
    ReusableDialog* dialog = messageDialog;

    // A dialog already up whose button exits keeps showing; the message follows once it is answered.
    if (dialog->IsShowing() != 0 && dialog->okButton->callback()->exec == DoExit)
    {
        dialog->callback = LostPlayerHandler;
        return;
    }

    LostPlayerHandler(0);
}

auto Logistics::handlePrepareScenarioMessage() -> void
{
    soundSystem->playDigitalSample(0x3a, 1, nullptr, 0, 0);
    mission->StartScenario(mpMissionName);
}

auto Logistics::prepareMultiplayerScenario(char* scenarioName, char* startFile) -> int32_t
{
    // The original wrote the start file through an ofstream; the port builds the same text (see TextStream).
    TextStream out;
    const int32_t homePlayers = MPlayer->playersOnHomeTeam()->count;
    // The Inner Sphere side's drop slots come first; team 0 is the Inner Sphere, 1 the Clans.
    DropSlot** isSlots;
    DropSlot** clanSlots;
    int32_t ownTeam;
    int32_t otherTeam;
    char side[8];
    int32_t isPlayers;
    int32_t clanPlayers;

    if (MPlayer->homeTeamGroupID == MPlayer->clanGroupID)
    {
        clanSlots = &dropSlots[0][0];
        isSlots = &opponentDropSlots[0][0];
        ownTeam = 1;
        otherTeam = 0;
        std::snprintf(side, sizeof(side), "Clan");
        clanPlayers = homePlayers;
        isPlayers = MPlayer->numPlayers() - homePlayers;
    }
    else
    {
        isSlots = &dropSlots[0][0];
        clanSlots = &opponentDropSlots[0][0];
        ownTeam = 0;
        otherTeam = 1;
        std::snprintf(side, sizeof(side), "IS");
        isPlayers = homePlayers;
        clanPlayers = MPlayer->numPlayers() - homePlayers;
    }

    char outName[0x200];
    std::snprintf(outName, sizeof(outName), "%s%s.fit", saveTempPath, startFile);
    char inName[0x200];
    std::snprintf(inName, sizeof(inName), "%s%s.fit", missionPath, scenarioName);

    // The mission file up to its [Campaign] block goes over as it is.
    char line[0x200];
    {
        File missionFile;
        int32_t result = missionFile.open(inName);
        Assert(result == 0, static_cast<uint32_t>(result), "Could not open input mission file", nullptr);

        while (missionFile.eof() == 0)
        {
            missionFile.readLine(reinterpret_cast<uint8_t*>(line), 0x1ff);

            if (std::strstr(line, "[Campaign]") != nullptr || std::strstr(line, "FITend") != nullptr)
            {
                break;
            }

            out << line << '\n';
        }
    }

    // Port: the original allocated the FitIniFile.
    FitIniFile file;
    int32_t result = file.open(inName);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not open input mission file", nullptr);
    result = file.seekBlock("Campaign");
    Assert(result == 0, static_cast<uint32_t>(result), "Could not find campaign block", nullptr);
    out << "[Campaign]" << '\n';
    result = file.readIdString("MapFile", line, 0x1ff);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not find map file", nullptr);
    out << "st MapFile = \"" << line << "\"" << '\n';
    int32_t value = 0;
    result = file.readIdLong("MaxTonnage", value);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not find max tonnage", nullptr);
    // Each player on the local team gets an equal share.
    value /= homePlayers;
    out << "l MaxTonnage = " << value << '\n';
    result = file.readIdLong("NumDropZones", value);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not find numdropzones", nullptr);
    out << "l NumDropZones = " << value << '\n';
    char variable[0x100];
    std::snprintf(variable, sizeof(variable), "%sBriefingFile", side);
    result = file.readIdString(variable, line, 0x1ff);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not find briefing file", nullptr);
    out << "st BriefingFile = \"" << line << "\"" << '\n' << '\n';

    // Each side's artillery, shared out among its players ([0] Inner Sphere, [1] Clans).
    int32_t largeStrikes[2];
    int32_t smallStrikes[2];
    int32_t sensorStrikes[2];
    int32_t cameraStrikes[2];
    result = file.seekBlock("ISArtillery");
    Assert(result == 0, 0, "No [ISArtillery] section in mission file", nullptr);
    file.readIdLong("NumLargeStrikes", value);
    largeStrikes[0] = isPlayers != 0 ? value / isPlayers : 0;
    file.readIdLong("NumSmallStrikes", value);
    smallStrikes[0] = isPlayers != 0 ? value / isPlayers : 0;
    file.readIdLong("NumSensorStrikes", value);
    sensorStrikes[0] = isPlayers != 0 ? value / isPlayers : 0;
    file.readIdLong("NumCameraStrikes", value);
    cameraStrikes[0] = isPlayers != 0 ? value / isPlayers : 0;
    result = file.seekBlock("ClanArtillery");
    Assert(result == 0, 0, "No [Clan Artillery] section in mission file", nullptr);
    result = file.readIdLong("NumLargeStrikes", value);
    Assert(result == 0, 0, "No Clan NumLargeStrikes section in mission file", nullptr);
    largeStrikes[1] = clanPlayers != 0 ? value / clanPlayers : 0;
    result = file.readIdLong("NumSmallStrikes", value);
    Assert(result == 0, 0, "No Clan NumSmallStrikes section in mission file", nullptr);
    smallStrikes[1] = clanPlayers != 0 ? value / clanPlayers : 0;
    result = file.readIdLong("NumSensorStrikes", value);
    Assert(result == 0, 0, "No Clan NumSensorStrikes section in mission file", nullptr);
    sensorStrikes[1] = clanPlayers != 0 ? value / clanPlayers : 0;
    result = file.readIdLong("NumCameraStrikes", value);
    Assert(result == 0, 0, "No Clan NumCameraStrikes section in mission file", nullptr);
    cameraStrikes[1] = clanPlayers != 0 ? value / clanPlayers : 0;

    // A commander block per player with its side's share.
    FLinkedList<FIDPPlayer>* players = MPlayer->sessionManager->GetPlayers(nullptr);

    for (FLink<FIDPPlayer>* link = players->head; link != nullptr && link->data != nullptr; link = link->next)
    {
        const int32_t sideIndex = link->data->IsInGroup(MPlayer->innerSphereGroupID) != 0 ? 0 : 1;
        out << "[Commander:" << static_cast<int>(link->data->playerNumber) << "]" << '\n';
        out << "l NumSmallStrikes\t\t= " << smallStrikes[sideIndex] << '\n';
        out << "l NumLargeStrikes\t\t= " << largeStrikes[sideIndex] << '\n';
        out << "l NumSensorStrikes\t\t= " << sensorStrikes[sideIndex] << '\n';
        out << "l NumCameraDrones\t\t= " << cameraStrikes[sideIndex] << '\n' << '\n';
    }

    // The pilots: the local player's, then the other players' (numbered on).
    LogWarriorList* networkPilots = mpWarriorList;
    const int32_t numAssigned = assignedWarriorList->numWarriors;
    int32_t numWarriors = networkPilots->numWarriors + numAssigned;
    int32_t warriorNumber = 1;

    for (LogWarrior* warrior = assignedWarriorList->warriors; warrior != nullptr; warrior = warrior->next)
    {
        out << "[Warrior" << warriorNumber << "]" << '\n';
        out << "st Profile = \"" << warrior->fileName << "\"" << '\n';
        out << "st Brain = \"pbrain\"\n\n";
        warriorNumber++;
    }

    for (LogWarrior* warrior = networkPilots->warriors; warrior != nullptr; warrior = warrior->next)
    {
        out << "[Warrior" << warriorNumber << "]" << '\n';
        out << "st Profile = \"" << warrior->fileName << "\"\n";
        out << "st Brain = \"pbrain\"\n\n";
        warriorNumber++;
    }

    // The local player's deployed units go into their drop slots.
    for (int32_t index = 0; index < 12; index++)
    {
        const DeploySlot& deploy = deploySlots[index / 4][index % 4];
        LogPart* part;

        if (deploy.unit >= 0)
        {
            LogMech* mech = nullptr;
            forceMechList->getMechInfo(deploy.unit, mech);
            DropSlot* slot = (&dropSlots[0][0])[index];
            Assert(slot->part == nullptr, 0, "local/remote mech conflict", nullptr);
            slot->part = mech;
            part = mech;
        }
        else if (deploy.vehicle >= 0)
        {
            LogVehicle* vehicle = nullptr;
            forceVehicleList->getVehicleInfo(deploy.vehicle, vehicle);
            DropSlot* slot = (&dropSlots[0][0])[index];
            Assert(slot->part == nullptr, 0, "local/remote vehicle conflict", nullptr);
            slot->part = vehicle;
            part = vehicle;
        }
        else
        {
            continue;
        }

        part->commanderID = MPlayer->checkInId;
    }

    // Every drop slot's unit as a part: a profile of its own ("part<n>") and its place in the side's drop zones.
    const int32_t controlType = MPlayer->isServer != 0 ? 2 : 3;
    const uint32_t numHome = static_cast<uint32_t>(MPlayer->playersOnHomeTeam()->count);
    const uint32_t numEnemy = static_cast<uint32_t>(MPlayer->playersOnEnemyTeam()->count);
    Assert(numEnemy != 0, numEnemy, " No Enemy Team ", nullptr);
    Assert(numHome != 0, numHome, " No Home Team ", nullptr);
    const int32_t homeSlotsPerPlayer = 12 / static_cast<int32_t>(numHome);
    const int32_t enemySlotsPerPlayer = 12 / static_cast<int32_t>(numEnemy);
    // Each part's commander, by part number (ended by 0xff).
    int32_t partCommanders[0x40] = {};
    int32_t partNumber = 1;
    MissionLogisticsBridge bridge;

    for (int32_t zoneBase = 0; zoneBase < 6; zoneBase += 3)
    {
        DropSlot** table = zoneBase == 0 ? isSlots : clanSlots;
        const bool ownTable = table == &dropSlots[0][0];
        const int32_t slotsPerPlayer = ownTable ? homeSlotsPerPlayer : enemySlotsPerPlayer;
        const int32_t commanderBase = ownTable ? 0 : 3;

        for (int32_t index = 0; index < 12; index++)
        {
            const int32_t commander = index / slotsPerPlayer + commanderBase;
            DropSlot* slot = table[index];
            LogPart* part = slot->part;

            if (part == nullptr)
            {
                continue;
            }

            char profileName[0x20];
            std::snprintf(profileName, sizeof(profileName), "part%d", partNumber);
            const int32_t partType = part->partType;

            if (partType == 1)
            {
                bridge.logisticsMechProfileWriter(profileName, static_cast<LogMech*>(part), 0);
            }
            else
            {
                bridge.logisticsVehicleProfileWriter(profileName, static_cast<LogVehicle*>(part), 0);
            }

            out << "[Part" << partNumber << "]" << '\n';
            out << "ul ObjectNumber         = " << static_cast<unsigned long>(part->chassis) << '\n';
            out << "ul ControlType          = " << controlType << '\n';
            out << "b PlayerPart            = " << (part->localPart != 0 ? "True" : "False") << '\n';
            out << "ul ControlDataType      = " << partType << '\n';
            out << "c MyIcon                = 0" << '\n';
            out << "c TeamId\t\t\t\t= " << (ownTable ? ownTeam : otherTeam) << '\n';
            const int32_t commanderID = part->commanderID;
            out << "l CommanderId\t\t    = " << commanderID << '\n';
            out << "st ObjectProfile        = \"" << profileName << "\"" << '\n';
            out << "ul Gesture              = 2" << '\n';
            out << "l PaintScheme           = " << multiPlayerColors[commander] << '\n';
            out << "f Velocity              = 0.0" << '\n';
            out << "l Active                = 1" << '\n';
            out << "l Exists                = 1" << '\n';
            const int32_t zone = zoneBase + slot->lance;
            const DeploySlotInfo& info = deploySlotInfo[zone][slot->slot];
            // The sums were made on the x87 and printed as doubles.
            out << "f PositionX             = "
                << static_cast<double>(info.offsetX) + static_cast<double>(dropZonePositions[zone].x) << '\n';
            out << "f PositionY             = "
                << static_cast<double>(dropZonePositions[zone].y) + static_cast<double>(info.offsetY) << '\n';
            out << "f PositionZ             = -1.0" << '\n';
            out << "f Rotation              = " << static_cast<double>(info.rotation) << '\n';

            if (partType == 1)
            {
                // A mech's pilot: the local player's by pilot index, another player's after the local ones.
                auto* mech = static_cast<LogMech*>(part);
                const int32_t pilot = mech->localPart != 0
                                          ? mech->pilotIndex + 1
                                          : networkPilots->getWarriorIndex(mech->networkPilot) + numAssigned + 1;
                out << "ul Pilot                = " << pilot << '\n' << '\n';
            }
            else
            {
                // A vehicle gets a crew of its own.
                numWarriors++;
                out << "ul Pilot                = " << numWarriors << '\n' << '\n';
                out << "[Warrior" << numWarriors << "]" << '\n';
                out << "st Profile=\"PCREWB\"" << '\n';
                out << "st Brain=\"pbrain\"" << '\n' << '\n';
            }

            partCommanders[partNumber] = commanderID;
            partNumber++;
        }
    }

    out << "[Parts]" << '\n';
    out << "ul NumParts = " << static_cast<int>(partNumber - 1) << '\n';
    out << "b AlliedTeam = False" << '\n';
    out << "[Warriors]\n";
    out << "ul NumWarriors = " << numWarriors << '\n';
    out << "uc CaptureChance = 0\n\n";

    // A group per commander and lance: the numbers of the parts that follow each other with the same commander,
    // split at every four slots (the next group of the same commander counts up).
    partCommanders[partNumber] = 0xff;
    int32_t currentCommander = partCommanders[1];
    int32_t partsSeen = 0;

    for (int32_t pass = 0; pass < 2; pass++)
    {
        DropSlot** table = pass == 0 ? isSlots : clanSlots;
        int32_t groupNumber = 0;
        int32_t inGroup = 0;

        for (int32_t index = 0; index < 12;)
        {
            if (table[index]->part != nullptr)
            {
                partsSeen++;
                inGroup++;
            }

            index++;
            const int32_t nextCommander = partCommanders[partsSeen + 1];

            if (index % 4 != 0 && nextCommander == currentCommander)
            {
                continue;
            }

            if (inGroup > 0)
            {
                out << "[Commander" << currentCommander << "Group:" << groupNumber << "]" << '\n';
                out << "l[12] Mates             = ";
                int32_t written = 0;

                if (inGroup > 0)
                {
                    int32_t mate = partsSeen - inGroup + 1;

                    for (int32_t count = inGroup; count != 0; count--)
                    {
                        out << static_cast<int>(mate++) << ", ";
                    }

                    written = inGroup;
                }

                if (written < 11)
                {
                    for (int32_t count = 11 - written; count != 0; count--)
                    {
                        out << static_cast<int>(0) << ", ";
                    }
                }

                out << static_cast<int>(0) << '\n';

                if (nextCommander == currentCommander)
                {
                    groupNumber++;
                }
            }

            if (nextCommander != currentCommander)
            {
                groupNumber = 0;
            }

            currentCommander = nextCommander;
            inGroup = 0;
        }
    }

    out << '\n';
    out << "FITend" << '\n' << '\n';
    out.writeFile(outName);
    return 0;
}

namespace
{
    /// <summary>
    /// The logistics cheat codes as DirectInput scan codes, each ended by 0xff (0x00782210; the name is the port's):
    /// MITCHLOVESYOU, HEREITCOMES, POUNDOFFLESH, KEEPTHEHAMMERDOWN, ROCKANDROLLPEOPLE, INFO, COCKADOODLEDOO.
    /// </summary>
    const int16_t logCheatCodes[7][18] = {
        {50, 23, 20, 46, 35, 38, 24, 47, 18, 31, 21, 24, 22, 255, 0, 0, 0, 0},
        {35, 18, 19, 18, 23, 20, 46, 24, 50, 18, 31, 255, 0, 0, 0, 0, 0, 0},
        {25, 24, 22, 49, 32, 24, 33, 33, 38, 18, 31, 35, 255, 0, 0, 0, 0, 0},
        {37, 18, 18, 25, 20, 35, 18, 35, 30, 50, 50, 18, 19, 32, 24, 17, 49, 255},
        {19, 24, 46, 37, 30, 49, 32, 19, 24, 38, 38, 25, 18, 24, 25, 38, 18, 255},
        {23, 49, 33, 24, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {46, 24, 46, 37, 30, 32, 24, 24, 32, 38, 18, 32, 24, 24, 255, 0, 0, 0},
    };

    /// <summary>
    /// Set while the mission warp takes digits (0x00808654; the name is the port's). Nothing sets it, so the warp
    /// never runs.
    /// </summary>
    int32_t missionWarpActive = 0;
    /// <summary>The mission number being typed for the warp, -1 before the first digit (0x00797858; the name is the port's).</summary>
    int8_t missionWarpNumber = -1;
}

auto Logistics::processCheatCode(int16_t key) -> void
{
    int32_t position = LogCurCheatChar;

    if (InDemo != 0 || MPlayer != nullptr || cheatsOn == 0)
    {
        return;
    }

    int32_t matched = -1;

    if (missionWarpActive != 0)
    {
        // Two digit keys (scan codes 2..11 for 1..0) pick the mission to jump to.
        if (missionWarpNumber < 0)
        {
            if (key > 10)
            {
                missionWarpNumber = 0;
                return;
            }

            missionWarpNumber = static_cast<int8_t>(static_cast<char>(key) * 10 - 10);
            return;
        }

        if (key < 12)
        {
            missionWarpNumber = static_cast<int8_t>(missionWarpNumber + static_cast<char>(key) - 1);
            FitIniFile file;
            char text[256];
            std::snprintf(text, sizeof(text), "%s%s.fit", missionPath, missionName);
            file.open(text);
            file.seekBlock("OpInfo");
            // Count the operation's missions.
            int32_t count;
            int32_t index = 0;
            int32_t result;

            do
            {
                count = index + 1;
                std::snprintf(text, sizeof(text), "Scenario%dMission", index);
                int32_t value = 0;
                result = file.readIdLong(text, value);
                index = count;
            } while (result == 0);

            file.close();
            missionWarpActive = 0;

            if (count <= missionWarpNumber)
            {
                missionWarpNumber = -1;
                return;
            }

            soundSystem->playBettySample(4);
            currentMission = missionWarpNumber;
            std::snprintf(text, sizeof(text), "start%d", currentMission);
            // The original called the bridge with a stack address as this (it has no fields).
            MissionLogisticsBridge bridge;
            bridge.logisticsSaveGame(text);
            char extension[] = ".sav";
            loadCampaign(text, extension, 0, 0);
            missionWarpNumber = -1;
            setUpBriefingScreen(1);
            return;
        }

        missionWarpActive = 0;
        missionWarpNumber = -1;
        return;
    }

    // Every code still matching so far stays active; a code whose last key this is fires.
    if (LogCurCheatChar == 0)
    {
        for (int& active : LogCheatActive)
        {
            active = 1;
        }
    }

    for (int32_t code = 0; code < 7; code++)
    {
        if (LogCheatActive[code] == 0)
        {
            continue;
        }

        if (key == logCheatCodes[code][position])
        {
            if (logCheatCodes[code][position + 1] == 0xff)
            {
                position = 0;
                LogCurCheatChar = 0;
                LogCheatActive[code] = 0;
                matched = code;
                break;
            }
        }
        else
        {
            LogCheatActive[code] = 0;
        }
    }

    int32_t code = 0;

    for (; code < 7; code++)
    {
        if (LogCheatActive[code] != 0)
        {
            LogCurCheatChar = position + 1;
            break;
        }
    }

    if (code == 7)
    {
        LogCurCheatChar = 0;
    }

    switch (matched)
    {
        case 0:
        {
            // MITCHLOVESYOU: repairs the force completely.
            soundSystem->playBettySample(4);

            for (LogMech* mech = forceMechList->mechs; mech != nullptr; mech = mech->next)
            {
                MechRepairBlock* block = mech->repairBlock;
                block->repairArmor(-1);
                block->repairInternal(-1);

                for (_LogInventoryItem* item = mech->inventory->items; item != nullptr; item = item->next)
                {
                    for (_LogInventoryStat* stat = item->stats; stat != nullptr; stat = stat->next)
                    {
                        stat->hits = 0;
                    }
                }

                block->setArmorSlider(-1);
                block->setInternalSlider(-1);
                block->setEngineSlider(-1);
                block->drawBackground(block->slotIndex, nullptr);
            }
            break;
        }
        case 1:
        {
            // HEREITCOMES: one more of every component.
            soundSystem->playBettySample(4);

            for (_LogInventoryItem* item = componentInventory->items; item != nullptr; item = item->next)
            {
                item->count++;
            }

            LogInvScreen* screen = repairScreen;

            if (currentScreen != screen)
            {
                screen = purchaseScreen;
            }

            screen->createCompInvBlock();
            screen->setUpCompInv(1, 1);
            break;
        }

        case 2:
        {
            // POUNDOFFLESH: a million resource points.
            soundSystem->playBettySample(4);
            ResourcePoints += 1000000;
            break;
        }
        case 4:
        {
            // ROCKANDROLLPEOPLE: no drop tonnage limit.
            soundSystem->playBettySample(4);
            hammerDown = 1;
            briefingScreen->calcTonnages();
            break;
        }
        case 5:
            // INFO: resets the warp number (but does not switch the warp on).
            missionWarpNumber = -1;
            break;
        case 6:
        {
            // COCKADOODLEDOO: the logistics heap's free memory in the window title.
            char text[256];
            std::snprintf(text, sizeof(text), "FreeMemory: %d", static_cast<int>(logisticsHeap->totalCoreLeft()));

            // Port: SetWindowTextA -> the SDL window's title.
            if (MCDisplay* display = MCInput::Display())
            {
                display->SetTitle(text);
            }
            break;
        }

        case 7:
        {
            // Unreachable (there are seven codes, 0..6): makes the pilot called "rooster" (or named Scott) an elite
            // "Scott" at full health.
            int32_t pilotIndex = 0;
            LogWarrior* found = nullptr;
            int assignedPilot = 0;

            for (LogWarrior* warrior = warriorList->warriors; warrior != nullptr; warrior = warrior->next)
            {
                if (std::strcmp(MCPort::StrLwr(warrior->callsign), "rooster") == 0 ||
                    std::strcmp(warrior->name, "Scott") == 0)
                {
                    found = warrior;
                    break;
                }
            }

            if (found == nullptr)
            {
                for (LogWarrior* warrior = assignedWarriorList->warriors; warrior != nullptr; warrior = warrior->next)
                {
                    if (std::strcmp(MCPort::StrLwr(warrior->callsign), "rooster") == 0 ||
                        std::strcmp(warrior->name, "Scott") == 0)
                    {
                        found = warrior;
                        assignedPilot = 1;
                        break;
                    }

                    pilotIndex++;
                }

                if (found == nullptr)
                {
                    return;
                }
            }

            const auto skill = static_cast<char>(static_cast<int32_t>(MaxPilotSkill));
            found->skills[0] = skill;
            found->skills[1] = skill;
            found->skills[2] = skill;
            found->skills[3] = skill;
            found->health = 6.0f;
            found->rank = 3;
            std::strcpy(found->callsign, "Scott");

            if (assignedPilot == 0)
            {
                return;
            }

            LogMech* mech = forceMechList->mechs;

            while (mech->pilotIndex != pilotIndex)
            {
                mech = mech->next;
            }

            mech->repairBlock->drawBackground(mech->repairBlock->slotIndex, nullptr);
            break;
        }

        default:
            // Case 3 (KEEPTHEHAMMERDOWN) does nothing in logistics.
            break;
    }
}

auto Logistics::FindMPMechList(uint32_t playerID, int teammate, int* listIndex) -> LogMechList*
{
    LogMechList** lists = teammate == 0 ? mpMechLists[1] : mpMechLists[0];
    LogMechList* found = nullptr;

    if (listIndex != nullptr)
    {
        *listIndex = -1;
    }

    int32_t index = 0;

    while (lists[index] == nullptr || lists[index]->playerID != playerID)
    {
        index++;

        if (index > 2)
        {
            break;
        }
    }

    if (index <= 2)
    {
        found = lists[index];

        if (listIndex != nullptr)
        {
            *listIndex = index;
        }
    }

    if (found == nullptr)
    {
        // Write what is known about the lists to nomechlist.log for the bug report the assert asks for.
        TextStream log;
        log << "Deploying mech - isTeammate = " << teammate << '\n';
        log << "Player is " << MPlayer->sessionManager->GetPlayer(playerID)->name
            << "with id: " << static_cast<unsigned long>(playerID) << '\n';

        for (int32_t i = 0; i < 3; i++)
        {
            log << "Sanity Check!!!" << '\n';
            log << "Friendly List DPID " << i << " = ";

            if (mpMechLists[0][i] == nullptr)
            {
                log << "NULL List!";
            }
            else
            {
                log << static_cast<int>(mpMechLists[0][i]->playerID);
            }

            log << '\n';
            log << "Enemy List DPID " << i << " = ";

            if (mpMechLists[1][i] == nullptr)
            {
                log << "NULL List!";
            }
            else
            {
                log << static_cast<int>(mpMechLists[1][i]->playerID);
            }

            log << '\n';
        }

        log.writeFile("nomechlist.log");
    }

    Assert(found != nullptr, 0, " Could not find a List to add mech to.  Save nomechlist.log file!!!!!!!!! ", nullptr);
    return found;
}

auto Logistics::FindMPVehicleList(uint32_t playerID, int teammate) -> LogVehicleList*
{
    LogVehicleList** lists = teammate == 0 ? mpVehicleLists[1] : mpVehicleLists[0];
    LogVehicleList* found = nullptr;

    for (int32_t index = 0; index < 3; index++)
    {
        if (lists[index] != nullptr && lists[index]->playerID == playerID)
        {
            found = lists[index];
            break;
        }
    }

    Assert(found != nullptr, 0, " Could not find a List to add vehicle to ", nullptr);
    return found;
}

auto Logistics::addReorderPilotIndexes(LogMech* mech) -> void
{
    for (; mech != nullptr; mech = mech->next)
    {
        if (mech->pilotIndex >= 0)
        {
            mech->pilotIndex++;
        }
    }
}

auto Logistics::addReorderPilotIndexes(LogVehicle*) -> void
{
}

auto Logistics::removeReorderPilotIndexes(LogVehicle*, LogVehicle*) -> void
{
}

auto Logistics::AddMechFromNetworkMessage(LogMechList* list, FIMessageHeader* message) -> LogPart*
{
    const auto* deploy = reinterpret_cast<const DeployForceMessage*>(message);
    // netmechs.rsp lists three variants per mech name.
    const uint32_t nameIndex = deploy->nameIndex * 3 + ((deploy->flags & 0xc) >> 2);
    const uint32_t side = sideGroupID(deploy->flags);
    char* mechName = netListItem(netMechNames, nameIndex);
    LogMech* mech = list->addMech(mechName, 0, 1, MPlayer->homeTeamGroupID == side ? 1 : 0);
    char* pilotName = netListItem(netWarriorNames, deploy->pilotNameIndex);
    // The pilot goes into the network pilot list (unsorted, so at its head).
    LogWarriorList* pilots = mpWarriorList;
    pilots->addWarrior(pilotName, 0);
    LogWarrior* pilot = nullptr;
    pilots->getWarriorInfo(0, pilot);
    mech->networkPilot = pilot;
    readDeployItems(mech, deploy);
    return mech;
}

auto Logistics::AddVehicleFromNetworkMessage(LogVehicleList* list, FIMessageHeader* message) -> LogPart*
{
    const auto* deploy = reinterpret_cast<const DeployForceMessage*>(message);
    const uint32_t side = sideGroupID(deploy->flags);
    char* vehicleName = netListItem(netVehicleNames, deploy->nameIndex);
    LogVehicle* vehicle = list->addVehicle(vehicleName, 0, 0, MPlayer->homeTeamGroupID == side ? 1 : 0);
    readDeployItems(vehicle, deploy);
    return vehicle;
}

auto Logistics::RemoveForceAtDropSlot(int32_t slotIndex, uint32_t playerID, int teamTable) -> int
{
    LogMech* mech = nullptr;
    LogVehicle* vehicle = nullptr;
    const int32_t lance = slotIndex / 4;
    const int32_t slot = slotIndex % 4;
    DropSlot* dropSlot = teamTable == 0 ? opponentDropSlots[lance][slot] : dropSlots[lance][slot];
    LogPart* part = dropSlot->part;

    if (part == nullptr)
    {
        return 0;
    }

    if (part->partType == 1)
    {
        mech = static_cast<LogMech*>(part);
    }
    else
    {
        vehicle = static_cast<LogVehicle*>(part);
    }

    dropSlot->part = nullptr;

    if (teamTable != 0)
    {
        briefingScreen->mpCalcTonnages();
        BriefingScreen* briefing = globalLogPtr->briefingScreen;

        if (briefing->briefingBox != nullptr)
        {
            briefing->removeChild(briefing->briefingBox);
            globalLogPtr->briefingScreen->briefingBox = nullptr;
        }
    }

    if (mech == nullptr)
    {
        FindMPVehicleList(playerID, teamTable)->removeVehicle(vehicle);
    }
    else
    {
        // Original behaviour (OB-098): the pilot is removed by its id used as a list position.
        mpWarriorList->removeWarrior(static_cast<uint8_t>(mech->networkPilot->id));
        FindMPMechList(playerID, teamTable, nullptr)->removeMech(mech);
    }

    if (teamTable != 0)
    {
        // Draw the empty slot over it (the screen draws its slots each frame).
        briefingScreen->CoverSlot(slotIndex, BriefingScreen::SlotCover::Covered);
    }

    return mech != nullptr || vehicle != nullptr ? 1 : 0;
}

auto CancelBool(int32_t) -> void
{
    if (launchedFromLobby != 0 && MPlayer != nullptr)
    {
        killTheGame();
    }

    Cancel();
}

auto BackToSession() -> void
{
    globalLogPtr->setUpSessionScreen();
}

auto BackToSessionBool(int32_t) -> void
{
    globalLogPtr->setUpSessionScreen();
}

auto LostPlayerHandler(int32_t answer) -> void
{
    if (answer == 1)
    {
        DoExit();
        return;
    }

    // Show holdString (from handleLostPlayer) with an OK button; it closes itself after five seconds.
    ReusableDialog* dialog = globalLogPtr->messageDialog;
    dialog->setText(holdString);
    globalLogPtr->messageDialog->setTwoButton(0);
    dialog = globalLogPtr->messageDialog;
    dialog->callback = CancelBool;
    dialog->okButton->callback()->setExec(nullptr);
    char upArt[] = "bh_okay.tga";
    char downArt[] = "bg_okay.tga";
    globalLogPtr->messageDialog->okButton->setUpPicture(upArt);
    globalLogPtr->messageDialog->okButton->setDownPicture(downArt);
    lDialogButton* button = globalLogPtr->messageDialog->okButton;
    button->disabled = 0;
    button->draw();
    dialog = globalLogPtr->messageDialog;
    dialog->timeout = 5000;
    dialog->timeoutResult = 1;
    dialog->activate();
    globalLogPtr->messageDialog->keepCallbacks = 1;
}

auto LogisticsChatCallback(FIDPMessage* message, void*) -> void
{
    globalLogPtr->HandleChatMessage(message->fromID, message->messageBuffer);
}
