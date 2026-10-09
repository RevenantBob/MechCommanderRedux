#include "stdafx.h"
#include "main/logistics.h"
#include "platform/MCInput.h"
#include "platform/MCDisplay.h"
#include "gui/afont.h"
#include "gui/asystem.h"
#include "gui/scrlpane.h"
#include "gui/updisp.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "lib/MCPacketFile.h"
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
#include "sound/MCSoundSystem.h"
#include "vfx/MCVfxFunctions.h"
#include "object/MCMasterComponent.h"
#include "object/MCBattleMech.h"
#include "object/MCBattleMechType.h"
#include "object/MCMechGameSystem.h"
#include "object/MCObjectType.h"
#include "platform/MCRegistry.h"
#include "platform/MCRenderer.h"
#include "object/MCObjectTypeManager.h"

/// <summary>Each mech name index's place in the logistics mech order (0x007977a4).</summary>
int32_t MechSort[24] = {23, 19, 13, 10, 0, 3, 2, 6, 9, 8, 15, 14, 18, 20, 4, 16, 1, 12, 5, 11, 21, 7, 17, 22};
char ObjectPakName[20] = "object2.pak";
std::type_identity_t<char[256]> HoldString{};
int LogCheatActive[7] = {};
/// <summary>The six multiplayer player colours (gamesys.fit's mPlayerColors).</summary>
int32_t MultiPlayerColors[6] = {};
std::type_identity_t<int32_t> LogCurCheatChar{};
std::type_identity_t<int> InDemo{};

namespace
{
    void* LogAlloc(size_t size)
    {
        return GlobalLogPtr->LogisticsBlocks->Allocate(static_cast<uint32_t>(size));
    }

    void LogFree(void* block)
    {
        GlobalLogPtr->LogisticsBlocks->Free(block);
    }

    /// <summary>A copy of <paramref name="text"/> in a logistics block.</summary>
    char* LogStrDup(const char* text)
    {
        const size_t size = std::strlen(text) + 1;
        auto* copy = static_cast<char*>(LogAlloc(size));

        if (copy != nullptr)
        {
            std::memcpy(copy, text, size);
        }

        return copy;
    }

    /// <summary>
    /// A LogMech, LogVehicle or LogWarrior record in a logistics block. Port fix: zeroed (the original's heap block
    /// held whatever was there before; the readers set what they use, but a few fields, such as a vehicle's
    /// <c>binarySize</c> or a mech's last critical slots, were left as found).
    /// </summary>
    template <class T> T* AllocRecord()
    {
        void* block = LogAlloc(sizeof(T));

        if (block != nullptr)
        {
            std::memset(block, 0, sizeof(T));
        }

        return static_cast<T*>(block);
    }

    /// <summary>String <paramref name="id"/> of the string table, copied into a logistics block.</summary>
    char* LoadLogString(uint32_t id)
    {
        char text[256];
        CLoadString(ThisInstance, id, text, 0xfe);
        return LogStrDup(text);
    }

    const MCMasterComponent& Component(uint8_t masterID)
    {
        return MasterComponentList[masterID];
    }

    /// <summary>
    /// The form of component <paramref name="masterID"/>. Port fix: an empty critical slot holds 0xff, one past the
    /// 255 components; the original read the form from past the end of the table there. The port gives 0.
    /// </summary>
    MCComponentForm SlotForm(uint8_t masterID)
    {
        return masterID < NumMasterComponents() ? MasterComponentList[masterID].Form : MCComponentForm::Simple;
    }

    /// <summary>
    /// Reads <c>Desc&lt;descIndex&gt;</c>'s DescString from the object description file, as "%fc4" (the colour code)
    /// and the text, in logistics blocks; null when the file has no such block.
    /// </summary>
    char* ReadDescription(int32_t descIndex)
    {
        MCFitIniFile file;
        char text[1024];
        std::snprintf(text, sizeof(text), "%s%s", ObjectPath, ObjectDesc);
        int32_t result = file.Open(text);
        Assert(result == 0, static_cast<uint32_t>(result), "Could not open description file");
        std::snprintf(text, sizeof(text), "Desc%d", descIndex);

        if (file.SeekBlock(text) != 0)
        {
            return nullptr;
        }

        result = file.ReadIdString("DescString", text, 0x3ff);
        Assert(result == 0 || static_cast<uint32_t>(result) == 0xfada0003, static_cast<uint32_t>(result),
               "Could not read description string");
        const size_t length = std::strlen(text);
        auto* description = static_cast<char*>(LogAlloc(length + 5));
        std::snprintf(description, length + 5, "%%fc4%s", text);
        description[length + 4] = 0;
        return description;
    }

    /// <summary>The file name part of <paramref name="path"/> without folder or extension (<c>_splitpath</c>'s fname).</summary>
    void SplitFileName(const char* path, char* fileName, size_t size)
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

    /// <summary>
    /// Writes fields into a record image of <c>size</c> bytes. The bytes no field covers are zero (the original's
    /// records had fields there that nothing read).
    /// </summary>
    class MCImageWriter
    {
    public:
        MCImageWriter(uint8_t* data, size_t size) : _Data(data) { std::memset(data, 0, size); }

        template <class T> void Field(size_t offset, T& value) { std::memcpy(_Data + offset, &value, sizeof(T)); }

        template <class T> void Pointer(size_t offset, T*&)
        {
            const uint32_t zero = 0;
            std::memcpy(_Data + offset, &zero, sizeof(zero));
        }

    private:
        uint8_t* _Data = nullptr;
    };

    /// <summary>Reads fields from a record image.</summary>
    class MCImageReader
    {
    public:
        explicit MCImageReader(const uint8_t* data) : _Data(data) {}

        template <class T> void Field(size_t offset, T& value) { std::memcpy(&value, _Data + offset, sizeof(T)); }

        template <class T> void Pointer(size_t, T*& value) { value = nullptr; }

    private:
        const uint8_t* _Data = nullptr;
    };

    constexpr size_t WarriorImageSize = 300;
    constexpr size_t MechImageSize = 600;
    constexpr size_t VehicleImageSize = 0xd0;
    constexpr size_t StatImageSize = 0x1c;

    template <class IO> void VisitPart(IO& io, MCLogPart& part)
    {
        io.Field(0x0, part.PartType);
        io.Field(0x4, part.ProfileName);
        io.Pointer(0x10, part.WeightClassName);
        io.Pointer(0x14, part.ChassisClassName);
        io.Pointer(0x18, part.FileName);
        io.Field(0x1c, part.NameIndex);
        io.Field(0x20, part.BinarySize);
        io.Field(0x24, part.CurTonnage);
        io.Pointer(0x28, part.IconName);
        io.Field(0x2c, part.Status);
        io.Field(0x30, part.Chassis);
        io.Field(0x34, part.ResourcePoints);
        io.Field(0x38, part.BaseResourcePoints);
        io.Field(0x3c, part.PartNumber);
        io.Field(0x40, part.DescIndex);
        io.Pointer(0x44, part.Description);
        io.Field(0x48, part.EngineTonnage);
        io.Field(0x4c, part.EngineRating);
        io.Field(0x50, part.ArmorType);
        io.Field(0x54, part.ArmorTonnage);
        io.Field(0x58, part.NumOther);
        io.Field(0x59, part.NumWeapons);
        io.Field(0x5a, part.NumAmmo);
        io.Field(0x6c, part.BattleRating);
        io.Field(0x74, part.Assigned);
        io.Field(0x78, part.Deployed);
        io.Field(0x7c, part.Required);
        io.Field(0x80, part.NotMineYet);
        io.Field(0x84, part.LocalPart);
        io.Field(0x88, part.CommanderID);
        io.Pointer(0x8c, part.Inventory);
        io.Pointer(0x90, part.BriefingBox);
        io.Field(0x94, part.DropLance);
        io.Field(0x98, part.DropSlot);
    }

    template <class IO> void VisitMech(IO& io, MCLogMech& mech)
    {
        VisitPart(io, mech);
        io.Pointer(0x9c, mech.ExtraName1);
        io.Pointer(0xa0, mech.ExtraName2);
        io.Pointer(0xa4, mech.MechName);
        io.Field(0xa8, mech.UsedTonnage);
        io.Field(0xac, mech.FreeTonnage);
        io.Field(0xb0, mech.WeaponTonnage);
        io.Field(0xb4, mech.PilotIndex);
        io.Field(0xbc, mech.NameVariant);
        io.Field(0xc0, mech.SellValue);
        io.Field(0xc4, mech.SortKey);
        io.Field(0xc8, mech.MaxRunSpeed);
        io.Field(0xc9, mech.Armor);
        io.Field(0xe0, mech.HasCase);
        io.Field(0x100, mech.Internals);
        io.Field(0x110, mech.ItemSlots);
        io.Field(0x230, mech.HotSpotNumber);
        io.Field(0x238, mech.ChassisBR);
        io.Field(0x23c, mech.PilotModifier);
        io.Field(0x240, mech.StatusValue);
        io.Pointer(0x244, mech.RepairBlock);
        io.Pointer(0x248, mech.InventoryBlock);
        io.Pointer(0x24c, mech.BriefBlock);
        io.Pointer(0x250, mech.NetworkPilot);
        io.Pointer(0x254, mech.Next);
    }

    template <class IO> void VisitVehicle(IO& io, MCLogVehicle& vehicle)
    {
        VisitPart(io, vehicle);
        io.Field(0x9c, vehicle.Crew);
        io.Field(0xa5, vehicle.MaxMoveSpeed);
        io.Field(0xa6, vehicle.CurInternalStructure);
        io.Field(0xab, vehicle.MaxArmorPoints);
        io.Field(0xb0, vehicle.CurArmorPoints);
        io.Field(0xb8, vehicle.VehicleResourcePoints);
        io.Field(0xbc, vehicle.BaseVehicleResourcePoints);
        io.Pointer(0xc0, vehicle.RepairBlock);
        io.Pointer(0xc4, vehicle.InventoryBlock);
        io.Pointer(0xc8, vehicle.BriefBlock);
        io.Pointer(0xcc, vehicle.Next);
    }

    template <class IO> void VisitWarrior(IO& io, MCLogWarrior& warrior)
    {
        io.Field(0x0, warrior.FileName);
        io.Pointer(0xc, warrior.Next);
        io.Field(0x10, warrior.BinarySize);
        io.Pointer(0x14, warrior.Name);
        io.Field(0x18, warrior.Id);
        io.Pointer(0x1c, warrior.Callsign);
        io.Pointer(0x20, warrior.Picture);
        io.Pointer(0x24, warrior.PilotVideo);
        io.Pointer(0x28, warrior.PilotAudio);
        io.Pointer(0x2c, warrior.Brain);
        io.Field(0x30, warrior.PaintScheme);
        io.Field(0x34, warrior.Rank);
        io.Field(0x38, warrior.NameIndex);
        io.Field(0x3c, warrior.DescIndex);
        io.Pointer(0x40, warrior.Description);
        io.Field(0x48, warrior.Personality);
        io.Field(0x4c, warrior.Skills);
        io.Field(0x50, warrior.OriginalSkills);
        io.Field(0x54, warrior.StartingSkills);
        io.Field(0x58, warrior.SkillPoints);
        io.Field(0x68, warrior.MechClass);
        io.Field(0x69, warrior.MechType);
        io.Field(0x6a, warrior.WeaponClass);
        io.Field(0x6b, warrior.WeaponTypes);
        io.Field(0x70, warrior.Wounds);
        io.Field(0x74, warrior.Health);
        io.Field(0x78, warrior.WarriorStatus);
        io.Field(0x80, warrior.DropLance);
        io.Field(0x84, warrior.DropSlot);
        io.Field(0x8c, warrior.Assigned);
        io.Field(0x90, warrior.Deployed);
        io.Field(0x94, warrior.Sold);
        io.Field(0x98, warrior.NotMineYet);
        io.Field(0x9c, warrior.Ejected);
        io.Pointer(0x128, warrior.InventoryBlock);
    }

    template <class IO> void VisitStat(IO& io, MCLogInventoryStat& stat)
    {
        io.Field(0x0, stat.StatID);
        io.Field(0x1, stat.Hits);
        io.Field(0xc, stat.Facing);
        io.Field(0x10, stat.Amount);
        io.Field(0x12, stat.Location);
        io.Field(0x14, stat.ItemNum);
        io.Pointer(0x18, stat.Next);
    }

    /// <summary>Copies <paramref name="text"/> with its terminator to <paramref name="data"/>; returns the end.</summary>
    uint8_t* PutString(uint8_t* data, const char* text)
    {
        const size_t size = std::strlen(text) + 1;
        std::memcpy(data, text, size);
        return data + size;
    }

    /// <summary>
    /// Reads a saved record's string (up to its terminator, at most 256 bytes) into a logistics block. Port fix: the
    /// original allocated the length without the terminator and copied the terminator past the block, and a string
    /// of 256 or more characters ran on past its buffer.
    /// </summary>
    char* ReadImageString(MCFile* file, const char* noMemory)
    {
        char text[257];
        int32_t length = 0;

        while (length < 0x100)
        {
            const uint8_t value = file->ReadByte();
            text[length] = static_cast<char>(value);

            if (value == 0)
            {
                break;
            }

            ++length;
        }

        text[length] = 0;
        char* copy = LogStrDup(text);
        Assert(copy != nullptr, 0, noMemory);
        return copy;
    }

    /// <summary>The size of a warrior's saved form: the record image and its six strings.</summary>
    size_t WarriorDataSize(const MCLogWarrior* warrior)
    {
        return WarriorImageSize + std::strlen(warrior->Name) + std::strlen(warrior->Callsign) +
               std::strlen(warrior->Picture) + std::strlen(warrior->PilotVideo) + std::strlen(warrior->PilotAudio) +
               std::strlen(warrior->Brain) + 6;
    }

    //-----------------------------------------------------------------------------------------------------------

    /// <summary>
    /// The copy with stat id <paramref name="statID"/>, walking <paramref name="list"/>'s items and, of each, its
    /// first <c>count</c> copies, as the stat lookups did.
    /// </summary>
    /// <remarks>
    /// Port fix (OB-091): the original read the first item's copy list before checking the list had items (a null read on an
    /// empty inventory), and walked <c>count</c> copies even when the copy list was shorter (the count can be set
    /// apart from the copies by <see cref="MCInventoryList::AddCountToItem"/>). The port stops at the end of either.
    /// </remarks>
    MCLogInventoryStat* FindStat(MCInventoryList* list, uint8_t statID, MCLogInventoryItem** owner = nullptr)
    {
        MCLogInventoryItem* item = list->Items;

        for (int32_t index = 0; index < list->NumItems && item != nullptr; ++index, item = item->Next)
        {
            MCLogInventoryStat* stat = item->Stats;

            for (int32_t copy = 0; copy < item->Count && stat != nullptr; ++copy, stat = stat->Next)
            {
                if (stat->StatID == statID)
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
    MCLogInventoryItem* FindItem(MCInventoryList* list, uint8_t masterID)
    {
        for (MCLogInventoryItem* item = list->Items; item != nullptr; item = item->Next)
        {
            if (item->MasterID <= masterID)
            {
                return item->MasterID == masterID ? item : nullptr;
            }
        }

        return nullptr;
    }
}

//---------------------------------------------------------------------------
// InventoryList

MCInventoryList::MCInventoryList()
{
    Items = nullptr;
    NumItems = 0;
    NextStatID = 0;
}

auto MCInventoryList::LoadDescription(int32_t index, MCLogInventoryItem* item) -> void
{
    if (item == nullptr)
    {
        item = GetItemInfo(index);
    }

    if (item->Description != nullptr)
    {
        return;
    }

    // The component's own description block (Desc<master id>).
    char* description = ReadDescription(item->MasterID);

    if (description != nullptr)
    {
        item->Description = description;
    }
}

auto MCInventoryList::CreateStat(uint8_t itemNum, uint8_t hits, uint8_t facing, int16_t amount, uint8_t location)
    -> MCLogInventoryStat*
{
    auto* stat = static_cast<MCLogInventoryStat*>(LogAlloc(sizeof(MCLogInventoryStat)));
    Assert(stat != nullptr, 0, " no RAM for Invntory stat ");
    std::memset(stat, 0, sizeof(MCLogInventoryStat));
    stat->StatID = NextStatID;
    stat->Hits = hits;
    ++NextStatID;
    stat->Facing = facing;
    stat->Amount = amount;
    stat->Location = location;
    stat->Next = nullptr;
    stat->ItemNum = itemNum;
    return stat;
}

auto MCInventoryList::GetItemInfo(int32_t index) -> MCLogInventoryItem*
{
    if (index >= NumItems || index < 0)
    {
        return nullptr;
    }

    MCLogInventoryItem* item = Items;

    for (; index > 0; --index)
    {
        item = item->Next;
    }

    return item;
}

auto MCInventoryList::AddCountToItem(int32_t count, int32_t masterID) -> void
{
    for (MCLogInventoryItem* item = Items; item != nullptr; item = item->Next)
    {
        if (static_cast<int32_t>(item->MasterID) <= masterID)
        {
            if (item->MasterID == masterID)
            {
                item->Count += count;

                if (item->Count < 0)
                {
                    item->Count = 0;
                }
            }

            return;
        }
    }
}

auto MCInventoryList::Destroy() -> void
{
    MCLogInventoryItem* item = Items;

    while (item != nullptr)
    {
        if (item->Description != nullptr)
        {
            LogFree(item->Description);
            item->Description = nullptr;
        }

        if (item->PurchaseBlock != nullptr)
        {
            delete item->PurchaseBlock;
            item->PurchaseBlock = nullptr;
        }

        if (item->InventoryBlock != nullptr)
        {
            delete item->InventoryBlock;
            item->InventoryBlock = nullptr;
        }

        MCLogInventoryStat* stat = item->Stats;

        while (stat != nullptr)
        {
            MCLogInventoryStat* next = stat->Next;
            LogFree(stat);
            stat = next;
        }

        MCLogInventoryItem* next = item->Next;
        LogFree(item);
        item = next;
    }

    Items = nullptr;
    NumItems = 0;
    NextStatID = 0;
}

namespace
{
    /// <summary>
    /// A new inventory item for <paramref name="masterID"/> holding <paramref name="stat"/>, with its purchase and
    /// inventory widgets unless <paramref name="widgets"/> is -1 (the part <see cref="MCInventoryList::AddItem"/>'s two
    /// paths share; each names the item a little differently).
    /// </summary>
    MCLogInventoryItem* NewInventoryItem(uint8_t masterID, MCLogInventoryStat* stat, int32_t widgets, size_t nameCopy)
    {
        auto* item = static_cast<MCLogInventoryItem*>(LogAlloc(sizeof(MCLogInventoryItem)));
        std::memset(item, 0, sizeof(MCLogInventoryItem));
        item->MasterID = masterID;
        const MCMasterComponent& master = Component(masterID);
        std::strncpy(item->Name, master.Name.c_str(), nameCopy);
        item->Name[0x1c] = 0;
        item->MasterValue = master.MasterID;
        // Ammunition counts as one item whatever the amount; anything else counts its amount.
        item->Count = master.Form == MCComponentForm::Ammo ? 1 : stat->Amount;
        item->Stats = stat;
        item->RangeIndex = 0;
        item->SortOrder = GlobalLogPtr->ComponentSort[masterID];

        for (int32_t index = 0; index < GlobalLogPtr->NumRangeSorted; ++index)
        {
            if (item->MasterID == GlobalLogPtr->RangeSortList[index])
            {
                item->RangeIndex = index;
                break;
            }
        }

        if (widgets != -1)
        {
            item->PurchaseBlock = new MCCompPurchaseBlock;
            item->PurchaseBlock->Init(item);
            item->PurchaseBlock->SortOrder = item->SortOrder;
            item->InventoryBlock = new MCCompInventoryBlock;
            item->InventoryBlock->Init(item);
            item->InventoryBlock->InventoryIndex = item->SortOrder;
        }
        else
        {
            item->PurchaseBlock = nullptr;
            item->InventoryBlock = nullptr;
        }

        item->Description = nullptr;
        return item;
    }
}

auto MCInventoryList::AddItem(uint8_t masterID, MCLogInventoryStat* stat, int32_t widgets) -> int32_t
{
    if (Items == nullptr)
    {
        // The first item: named from 29 characters and not given its description.
        MCLogInventoryItem* item = NewInventoryItem(masterID, stat, widgets, 0x1d);
        item->Next = nullptr;
        Items = item;
        ++NumItems;
        return stat->StatID;
    }

    // The list runs from the highest master id down; a copy of a component already there joins its copies, sorted
    // by item number.
    MCLogInventoryItem* previous = nullptr;
    MCLogInventoryItem* item = Items;

    do
    {
        if (item->MasterID <= masterID)
        {
            if (item->MasterID == masterID)
            {
                MCLogInventoryStat* copy = item->Stats;

                if (copy->ItemNum < stat->ItemNum)
                {
                    // Original behaviour: a copy numbered above the first goes in front of it; any other goes
                    // right after the first (the walk that follows never moves on), so the copies are not sorted.
                    stat->Next = copy;
                    ++item->Count;
                    item->Stats = stat;
                    return stat->StatID;
                }
                while (copy->Next != nullptr && copy->ItemNum < stat->ItemNum)
                {
                    copy = copy->Next;
                }

                stat->Next = copy->Next;
                copy->Next = stat;
                ++item->Count;
                return stat->StatID;
            }
            break;
        }

        previous = item;
        item = item->Next;
    } while (item != nullptr);

    MCLogInventoryItem* added = NewInventoryItem(masterID, stat, widgets, 0x1c);
    added->Next = item;
    LoadDescription(0, added);

    if (previous != nullptr)
    {
        previous->Next = added;
    }
    else
    {
        Items = added;
    }

    ++NumItems;
    return stat->StatID;
}

auto MCInventoryList::RemoveItem(uint8_t masterID, int32_t statID) -> int32_t
{
    MCLogInventoryItem* previous = nullptr;
    MCLogInventoryItem* item = Items;

    if (item == nullptr)
    {
        return -1;
    }
    while (masterID < item->MasterID)
    {
        previous = item;
        item = item->Next;

        if (item == nullptr)
        {
            return -1;
        }
    }

    if (item->MasterID != masterID)
    {
        return -1;
    }

    if (item->Count == 1)
    {
        // The last copy (whichever statID was asked for): the item goes with it.
        LogFree(item->Stats);

        if (previous == nullptr)
        {
            Items = item->Next;
        }
        else
        {
            previous->Next = item->Next;
        }

        delete item->PurchaseBlock;
        delete item->InventoryBlock;

        if (item->Description != nullptr)
        {
            LogFree(item->Description);
        }

        LogFree(item);
        --NumItems;
        return 0;
    }

    // One copy: the one numbered statID, or the first when statID is -1.
    MCLogInventoryStat* previousStat = nullptr;
    MCLogInventoryStat* stat = item->Stats;

    while (stat != nullptr)
    {
        if (statID < 0 || static_cast<int32_t>(stat->StatID) == statID)
        {
            if (static_cast<int32_t>(stat->StatID) == statID)
            {
                break;
            }

            stat = nullptr;
            break;
        }

        previousStat = stat;
        stat = stat->Next;
    }

    if (stat == nullptr)
    {
        if (statID != -1)
        {
            return -1;
        }

        stat = item->Stats;
        previousStat = nullptr;

        if (stat == nullptr)
        {
            return -1;
        }
    }

    if (previousStat == nullptr)
    {
        item->Stats = stat->Next;
    }
    else
    {
        previousStat->Next = stat->Next;
    }

    LogFree(stat);
    --item->Count;
    // Original behaviour: a copy removed this way still reports -1.
    return -1;
}

auto MCInventoryList::GetItemStatIndex(int32_t statID) -> MCLogInventoryItem*
{
    for (MCLogInventoryItem* item = Items; item != nullptr; item = item->Next)
    {
        for (MCLogInventoryStat* stat = item->Stats; stat != nullptr; stat = stat->Next)
        {
            if (static_cast<int32_t>(stat->StatID) == statID)
            {
                return item;
            }
        }
    }

    return nullptr;
}

auto MCInventoryList::GetItemStatID(uint8_t masterID, int32_t copy) -> int32_t
{
    MCLogInventoryItem* item = FindItem(this, masterID);

    if (item == nullptr || item->Count <= copy)
    {
        return -1;
    }

    MCLogInventoryStat* stat = item->Stats;

    for (; copy > 0; --copy)
    {
        stat = stat->Next;
    }

    return stat->StatID;
}

auto MCInventoryList::GetIndexFromMasterID(uint8_t masterID) -> int32_t
{
    MCLogInventoryItem* item = Items;

    for (int32_t index = 0; index < NumItems; ++index)
    {
        if (item->MasterID == masterID)
        {
            return index;
        }

        item = item->Next;
    }

    return -1;
}

auto MCInventoryList::GetMasterIDFromIndex(int32_t index) -> int32_t
{
    if (NumItems <= index)
    {
        return 0xff;
    }

    MCLogInventoryItem* item = Items;

    for (; index > 0; --index)
    {
        item = item->Next;
    }

    return item->MasterID;
}

auto MCInventoryList::GetMasterID(uint8_t statID) -> uint8_t
{
    MCLogInventoryItem* item = nullptr;

    if (FindStat(this, statID, &item) == nullptr)
    {
        return 0xff;
    }

    return item->MasterID;
}

auto MCInventoryList::GetFacing(uint8_t statID) -> uint8_t
{
    MCLogInventoryStat* stat = FindStat(this, statID);
    return stat != nullptr ? stat->Facing : 0xff;
}

auto MCInventoryList::GetAmount(uint8_t statID) -> int32_t
{
    MCLogInventoryStat* stat = FindStat(this, statID);
    return stat != nullptr ? stat->Amount : 0xff;
}

auto MCInventoryList::GetItemName(uint8_t masterID) -> char*
{
    // Original behaviour: the argument is used as a list position, not a master id.
    int32_t index = masterID;

    if (NumItems <= index)
    {
        return nullptr;
    }

    MCLogInventoryItem* item = Items;

    for (; index > 0; --index)
    {
        item = item->Next;
    }

    return item->Name;
}

auto MCInventoryList::GetItemCount(uint8_t masterID) -> int32_t
{
    MCLogInventoryItem* item = FindItem(this, masterID);
    return item != nullptr ? item->Count : 0;
}

auto MCInventoryList::HitItem(uint8_t statID, uint8_t hits) -> int32_t
{
    MCLogInventoryStat* stat = FindStat(this, statID);

    if (stat == nullptr)
    {
        return -1;
    }

    stat->Hits = hits;
    return 0;
}

auto MCInventoryList::SetStatLoc(uint8_t statID, int32_t location) -> int32_t
{
    MCLogInventoryStat* stat = FindStat(this, statID);

    if (stat == nullptr)
    {
        return -1;
    }

    stat->Location = static_cast<uint8_t>(location);
    return 0;
}

auto MCInventoryList::GetBinaryData(void* data) -> int32_t
{
    if (data != nullptr)
    {
        if (NumItems == 0)
        {
            return 4;
        }

        auto* out = static_cast<uint8_t*>(data);
        std::memcpy(out, &NumItems, 4);
        out += 4;
        MCLogInventoryItem* item = Items;

        for (int32_t index = NumItems; index != 0; --index)
        {
            *out = item->MasterID;
            std::memcpy(out + 1, &item->Count, 4);
            out += 5;
            MCLogInventoryStat* stat = item->Stats;

            for (int32_t copy = item->Count; copy != 0 && stat != nullptr; --copy)
            {
                MCImageWriter writer(out, StatImageSize);
                VisitStat(writer, *stat);
                out += StatImageSize;
                stat = stat->Next;
            }

            item = item->Next;
        }

        // Original behaviour (OB-086): the size written is not returned.
        return 4;
    }

    int32_t size = 4;

    // Port fix (OB-086): the original added the first item's copy count for every item.
    for (MCLogInventoryItem* item = Items; item != nullptr; item = item->Next)
    {
        size += 5 + item->Count * static_cast<int32_t>(StatImageSize);
    }

    return size;
}

namespace
{
    /// <summary>
    /// The four groups <see cref="MCInventoryList::SortRange"/> and <see cref="MCInventoryList::SortName"/> sort and join:
    /// the item positions of each kind (weapon type or form 7, 9, 8 and 2, in output order).
    /// </summary>
    struct MCSortGroups
    {
        std::vector<int32_t> Group2;
        std::vector<int32_t> Group7;
        std::vector<int32_t> Group8;
        std::vector<int32_t> Group9;
        int32_t Count2 = 0;
        int32_t Count7 = 0;
        int32_t Count8 = 0;
        int32_t Count9 = 0;

        /// <summary>
        /// Port fix: the original's four arrays (operator new, one slot per item) were uninitialised; the name sort
        /// reads past the group it fills (OB-087), so the port zeroes them.
        /// </summary>
        explicit MCSortGroups(int32_t size)
            : Group2(static_cast<size_t>(std::max(size, 1)))
            , Group7(Group2.size())
            , Group8(Group2.size())
            , Group9(Group2.size())
        {
        }

        void Add(int32_t kind, int32_t index)
        {
            switch (kind)
            {
                case 2:
                    Group2[Count2++] = index;
                    break;
                case 7:
                    Group7[Count7++] = index;
                    break;
                case 8:
                    Group8[Count8++] = index;
                    break;
                case 9:
                    Group9[Count9++] = index;
                    break;
                default:
                    break;
            }
        }

        /// <summary>The groups joined (7, 9, 8, 2) in a new array, or null when all are empty.</summary>
        int32_t* Join() const
        {
            const int32_t total = Count2 + Count7 + Count8 + Count9;

            // Port fix (OB-087): with nothing to sort the original returned an uninitialised or freed pointer.
            if (total == 0)
            {
                return nullptr;
            }

            auto* result = new int32_t[static_cast<size_t>(total)];
            int32_t* out = result;
            out = std::copy_n(Group7.data(), Count7, out);
            out = std::copy_n(Group9.data(), Count9, out);
            out = std::copy_n(Group8.data(), Count8, out);
            std::copy_n(Group2.data(), Count2, out);
            return result;
        }
    };

    /// <summary>
    /// The sorts' swap pass: for each position <c>p</c> below <paramref name="outerBound"/> - 1, every later position
    /// <c>q</c> below <paramref name="count"/> is swapped into <c>p</c> when <paramref name="before"/> says so.
    /// </summary>
    template <class Before> void SwapSort(std::vector<int32_t>& group, int32_t count, int32_t outerBound, Before before)
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

auto MCInventoryList::SortRange() -> int32_t*
{
    MCSortGroups groups(NumItems);

    // Original behaviour (OB-087): the last item is never sorted in.
    for (int32_t index = 0; index < NumItems - 1; ++index)
    {
        groups.Add(Component(static_cast<uint8_t>(GetMasterIDFromIndex(index))).WeaponType, index);
    }

    auto range = [this](int32_t index)
    { return Component(static_cast<uint8_t>(GetMasterIDFromIndex(index))).WeaponRange[3]; };
    auto shorter = [&](int32_t a, int32_t b) { return range(a) < range(b); };

    if (groups.Count7 > 1)
    {
        SwapSort(groups.Group7, groups.Count7, groups.Count7, shorter);
        // Original behaviour (OB-087): groups 9 and 8 are sorted only with group 7, and as far as its count.
        SwapSort(groups.Group9, groups.Count9, groups.Count7, shorter);
        SwapSort(groups.Group8, groups.Count8, groups.Count7, shorter);
    }

    if (groups.Count2 > 1)
    {
        SwapSort(groups.Group2, groups.Count2, groups.Count2, shorter);
    }

    return groups.Join();
}

auto MCInventoryList::SortName() -> int32_t*
{
    MCSortGroups groups(NumItems);

    // Original behaviour (OB-087): the last item is never sorted in.
    for (int32_t index = 0; index < NumItems - 1; ++index)
    {
        groups.Add(static_cast<int32_t>(Component(static_cast<uint8_t>(GetMasterIDFromIndex(index))).Form), index);
    }

    auto name = [this](int32_t index)
    { return Component(static_cast<uint8_t>(GetMasterIDFromIndex(index))).Name.c_str(); };
    auto earlier = [&](int32_t a, int32_t b) { return std::strcmp(name(b), name(a)) > 0; };

    if (groups.Count7 > 1)
    {
        // Original behaviour (OB-087): group 7 is ordered by the names of group 9's entries at the same positions.
        for (int32_t p = 0; p < groups.Count7 - 1; ++p)
        {
            const int32_t key = groups.Group9[static_cast<size_t>(p)];

            for (int32_t q = p + 1; q < groups.Count7; ++q)
            {
                if (std::strcmp(name(key), name(groups.Group9[static_cast<size_t>(q)])) > 0)
                {
                    std::swap(groups.Group7[static_cast<size_t>(p)], groups.Group7[static_cast<size_t>(q)]);
                }
            }
        }

        SwapSort(groups.Group9, groups.Count9, groups.Count7, earlier);
        SwapSort(groups.Group8, groups.Count8, groups.Count7, earlier);
    }

    if (groups.Count2 > 1)
    {
        SwapSort(groups.Group2, groups.Count2, groups.Count2, earlier);
    }

    return groups.Join();
}

//---------------------------------------------------------------------------
// LogWarriorList

MCLogWarriorList::MCLogWarriorList()
{
    Warriors = nullptr;
    NumWarriors = 0;
}

auto MCLogWarriorList::Destroy() -> void
{
    while (NumWarriors != 0)
    {
        RemoveWarriorAtIndex(0);
    }

    Warriors = nullptr;
}

auto MCLogWarriorList::AddWarrior(char* fileName, int sorted) -> int32_t
{
    MCFitIniFile file;
    std::string path;
    path = GamePath(WarriorPath, fileName, ".fit");
    const int32_t result = file.Open(path);
    Assert(result == 0, static_cast<uint32_t>(result), " could not open scenario file ");
    return AddWarrior(&file, sorted);
}

namespace
{
    /// <summary>
    /// Reads the parts of a pilot profile both <see cref="MCLogWarriorList::ReplaceWarrior"/> and
    /// <see cref="MCLogWarriorList::addWarrior(FitIniFile*, int)"/> read: the status flags from the current block, the
    /// personality, the skills (current, original, starting, points) and the rank.
    /// </summary>
    void ReadWarriorSkills(MCFitIniFile* file, MCLogWarrior* warrior)
    {
        if (file->ReadIdBoolean("Assigned", warrior->Assigned) != 0)
        {
            warrior->Assigned = 0;
        }

        if (file->ReadIdBoolean("Sold", warrior->Sold) != 0)
        {
            warrior->Sold = 0;
        }

        if (file->ReadIdBoolean("NotMineYet", warrior->NotMineYet) != 0)
        {
            warrior->NotMineYet = 0;
        }

        if (file->ReadIdBoolean("Ejected", warrior->Ejected) != 0)
        {
            warrior->Ejected = 0;
        }

        int32_t result = file->SeekBlock("PersonalityTraits");
        Assert(result == 0, 0, " Could not find PersonalityTraits Block ");
        result = file->ReadIdChar("Professionalism", warrior->Personality[0]);
        Assert(result == 0, static_cast<uint32_t>(result),
               " Could not find professionalism in PersonalityTraits Block ");
        result = file->ReadIdChar("Decorum", warrior->Personality[1]);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find decorum in PersonalityTraits Block ");
        result = file->ReadIdChar("Aggressiveness", warrior->Personality[2]);
        Assert(result == 0, static_cast<uint32_t>(result),
               " Could not find aggressiveness in PersonalityTraits Block ");
        result = file->ReadIdChar("Courage", warrior->Personality[3]);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find courage in PersonalityTraits Block ");

        result = file->SeekBlock("Skills");
        Assert(result == 0, 0, " Could not find Skills Block ");
        result = file->ReadIdChar("Piloting", warrior->Skills[0]);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Piloting in Skills Block ");
        result = file->ReadIdChar("Jumping", warrior->Skills[1]);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Jumping in Skills Block ");
        result = file->ReadIdChar("Sensors", warrior->Skills[2]);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Sensors in Skills Block ");
        result = file->ReadIdChar("Gunnery", warrior->Skills[3]);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Gunnery in Skills Block ");

        static const char* const skillNames[4] = {"Piloting", "Jumping", "Sensors", "Gunnery"};
        // The original and starting skills default to the current ones.
        const bool haveOriginal = file->SeekBlock("OriginalSkills") == 0;

        for (int32_t skill = 0; skill < 4; ++skill)
        {
            if (!haveOriginal || file->ReadIdChar(skillNames[skill], warrior->OriginalSkills[skill]) != 0)
            {
                warrior->OriginalSkills[skill] = warrior->Skills[skill];
            }
        }

        const bool haveStarting = file->SeekBlock("StartingSkills") == 0;

        for (int32_t skill = 0; skill < 4; ++skill)
        {
            if (!haveStarting || file->ReadIdChar(skillNames[skill], warrior->StartingSkills[skill]) != 0)
            {
                warrior->StartingSkills[skill] = warrior->Skills[skill];
            }
        }

        const bool havePoints = file->SeekBlock("SkillPoints") == 0;

        for (int32_t skill = 0; skill < 4; ++skill)
        {
            if (!havePoints || file->ReadIdFloat(skillNames[skill], warrior->SkillPoints[skill]) != 0)
            {
                warrior->SkillPoints[skill] = 0.0f;
            }
        }

        warrior->CalcRank();
    }

    /// <summary>Sets wounds and health from the Status block's Wounds; no health left means killed (and sold).</summary>
    void SetWounds(MCLogWarrior* warrior, char wounds)
    {
        warrior->Wounds = static_cast<float>(wounds);
        warrior->Health = 6.0f - warrior->Wounds;

        if (warrior->Health <= 0.0f)
        {
            warrior->WarriorStatus = 4;
            warrior->Sold = 1;
            warrior->Health = 0.0f;
        }
    }
}

auto MCLogWarriorList::ReplaceWarrior(MCPacketFile* file, int32_t index) -> int32_t
{
    MCFitIniFile profile;
    int32_t result = file->SeekPacket(index);
    Assert(result == 0, 0, " Unable to find warrior file ");
    result = profile.Open(file, static_cast<uint32_t>(file->GetPacketSize()));
    Assert(result == 0, 0, " Unable to open warrior file ");
    result = profile.SeekBlock("General");
    Assert(result == 0, static_cast<uint32_t>(result), " Bad Saved pilot file ");
    char callsign[256];
    result = profile.ReadIdString("Callsign", callsign, 0xff);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find CallSign in General Block ");

    // The warrior with that callsign takes the saved state.
    MCLogWarrior* warrior = Warriors;

    while (warrior != nullptr && std::strcmp(warrior->Callsign, callsign) != 0)
    {
        warrior = warrior->Next;
    }

    if (warrior == nullptr)
    {
        return 5;
    }

    warrior->Id = GlobalLogPtr->NextWarriorID++;
    ReadWarriorSkills(&profile, warrior);
    warrior->Deployed = 0;
    result = profile.SeekBlock("Status");
    Assert(result == 0, 0, " Could not find Status Block ");
    char wounds = 0;
    result = profile.ReadIdChar("Wounds", wounds);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find Wounds in Skills Block ");
    SetWounds(warrior, wounds);
    return 0;
}

auto MCLogWarriorList::AddWarrior(MCPacketFile* file, int32_t packet, int sorted) -> int32_t
{
    MCFitIniFile profile;
    int32_t result = file->SeekPacket(packet);
    Assert(result == 0, 0, " Unable to find warrior file ");
    result = profile.Open(file, static_cast<uint32_t>(file->GetPacketSize()));
    Assert(result == 0, 0, " Unable to open warrior file ");
    return AddWarrior(&profile, sorted);
}

auto MCLogWarriorList::AddWarrior(MCFitIniFile* file, int sorted) -> int32_t
{
    auto* warrior = AllocRecord<MCLogWarrior>();
    Assert(warrior != nullptr, 0, "Not enough memory for LogWarrior");
    warrior->Id = GlobalLogPtr->NextWarriorID++;
    warrior->NameIndex = 0;

    if (file->SeekBlock("General") != 0)
    {
        // A saved pilot list: a count, then each record's image and its six strings.
        // Original behaviour (OB-089): every record is read into this one warrior, which is never added to the list
        // (nor freed). Nothing in MCX.EXE writes such a file.
        file->Seek(0);
        int32_t count = file->ReadLong();
        std::array<uint8_t, WarriorImageSize> image{};

        while (count > 0)
        {
            file->Read(image.data(), static_cast<int32_t>(image.size()));
            MCImageReader reader(image.data());
            VisitWarrior(reader, *warrior);
            warrior->Name = ReadImageString(file, "Not enough memory for LogWarrior name");
            warrior->Callsign = ReadImageString(file, "Not enough memory for LogWarrior callsign");
            warrior->Picture = ReadImageString(file, "Not enough memory for LogWarrior photoFile");
            warrior->PilotVideo = ReadImageString(file, "Not enough memory for LogWarrior videoFile");
            warrior->PilotAudio = ReadImageString(file, "Not enough memory for LogWarrior audioFile");
            warrior->Brain = ReadImageString(file, "Not enough memory for LogWarrior brainFile");
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
    int32_t result = file->ReadIdString("Name", name, 0xff);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find Name in General Block ");
    result = file->ReadIdLong("NameIndex", warrior->NameIndex);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find NameIndex in Pilot General Block ");
    result = file->ReadIdString("Callsign", callsign, 0xff);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find NameIndex in Pilot General Block ");
    result = file->ReadIdString("Picture", picture, 0xff);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find Picture in Pilot General Block ");
    result = file->ReadIdString("pilotVideo", pilotVideo, 0xff);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find Pilotvideo in Pilot General Block ");
    result = file->ReadIdString("pilotAudio", pilotAudio, 0xff);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find Pilotaudio in Pilot General Block ");
    result = file->ReadIdString("Brain", brain, 0xff);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find Brain in Pilot General Block ");

    warrior->BinarySize =
        static_cast<uint32_t>(std::strlen(brain) + std::strlen(pilotAudio) + std::strlen(pilotVideo) +
                              std::strlen(picture) + std::strlen(callsign) + std::strlen(name) + 6 + WarriorImageSize);
    warrior->Name = LogStrDup(name);
    Assert(warrior->Name != nullptr, 0, "Not enough memory for LogWarrior name");
    warrior->Callsign = LogStrDup(callsign);
    Assert(warrior->Callsign != nullptr, 0, "Not enough memory for LogWarrior callsign");
    warrior->Picture = LogStrDup(picture);
    Assert(warrior->Picture != nullptr, 0, "Not enough memory for LogWarrior photoFile");
    warrior->PilotVideo = LogStrDup(pilotVideo);
    Assert(warrior->PilotVideo != nullptr, 0, "Not enough memory for LogWarrior videoFile");
    warrior->PilotAudio = LogStrDup(pilotAudio);
    Assert(warrior->PilotAudio != nullptr, 0, "Not enough memory for LogWarrior audioFile");
    warrior->Brain = LogStrDup(brain);
    Assert(warrior->Brain != nullptr, 0, "Not enough memory for LogWarrior brainFile");

    warrior->Description = nullptr;
    warrior->DescIndex = -1;
    file->ReadIdLong("DescIndex", warrior->DescIndex);
    warrior->LoadDescription(warrior->DescIndex);
    result = file->ReadIdLong("paintScheme", warrior->PaintScheme);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find paintScheme in General Block ");
    ReadWarriorSkills(file, warrior);

    if (file->SeekBlock("Affinities") == 0)
    {
        result = file->ReadIdChar("MechClass", warrior->MechClass);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find MechClass in Affinities Block ");
        result = file->ReadIdChar("MechType", warrior->MechType);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find MechType in Affinities Block ");
        result = file->ReadIdChar("WeaponClass", warrior->WeaponClass);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find WeaponClass in Affinities Block ");
        result = file->ReadIdUCharArray("WeaponTypes", reinterpret_cast<uint8_t*>(warrior->WeaponTypes), 2);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find WeaponTypes in Affinities Block ");
    }
    else
    {
        warrior->MechClass = 0;
        warrior->MechType = 0;
        warrior->WeaponClass = 0;
        warrior->WeaponTypes[0] = 0;
        warrior->WeaponTypes[1] = 0;
    }

    result = file->SeekBlock("Status");
    Assert(result == 0, 0, " Could not find Status Block ");
    char wounds = 0;
    result = file->ReadIdChar("Wounds", wounds);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find Wounds in Skills Block ");
    warrior->WarriorStatus = 0;
    SetWounds(warrior, wounds);
    warrior->DropLance = -1;
    warrior->DropSlot = -1;

    // A profile read from its own file is known by the file's base name (one read from a packet keeps none).
    if (file->GetParent() == nullptr)
    {
        SplitFileName(file->GetFilename().c_str(), warrior->FileName, sizeof(warrior->FileName));
    }

    AddWarrior(warrior, sorted);
    warrior->InventoryBlock = new MCPilotInventoryBlock;
    Assert(warrior->InventoryBlock != nullptr, 0, " Not enough memory for inventory block ");
    warrior->InventoryBlock->Init(warrior);
    return 0;
}

auto MCLogWarriorList::AddWarrior(MCLogWarrior* warrior, int sorted) -> int32_t
{
    if (sorted == 0)
    {
        warrior->Next = Warriors;
        Warriors = warrior;
        ++NumWarriors;
        return 0;
    }

    // By rank, and within a rank by callsign.
    MCLogWarrior* previous = nullptr;
    MCLogWarrior* current = Warriors;

    while (current != nullptr)
    {
        if (warrior->Rank <= current->Rank)
        {
            while (std::strcmp(current->Callsign, warrior->Callsign) < 0 && current->Rank == warrior->Rank)
            {
                previous = current;
                current = current->Next;

                if (current == nullptr)
                {
                    break;
                }
            }
            break;
        }

        previous = current;
        current = current->Next;
    }

    warrior->Next = current;

    if (previous != nullptr)
    {
        previous->Next = warrior;
    }
    else
    {
        Warriors = warrior;
    }

    ++NumWarriors;
    return 0;
}

auto MCLogWarriorList::ExtractWarrior(int32_t index, MCLogWarrior*& warrior) -> int32_t
{
    if (NumWarriors <= index)
    {
        return -1;
    }

    MCLogWarrior* previous = nullptr;
    MCLogWarrior* current = Warriors;

    for (; index > 0; --index)
    {
        previous = current;
        current = current->Next;
    }

    if (current == Warriors)
    {
        Warriors = current->Next;
    }
    else
    {
        previous->Next = current->Next;
    }

    --NumWarriors;
    warrior = current;
    return 0;
}

auto MCLogWarriorList::Heal(int32_t amount) -> void
{
    for (MCLogWarrior* warrior = Warriors; warrior != nullptr; warrior = warrior->Next)
    {
        if (warrior->WarriorStatus == 4 || warrior->Sold != 0)
        {
            continue;
        }

        if (warrior->Wounds < static_cast<float>(amount))
        {
            warrior->Wounds = 0.0f;
            warrior->Health = 6.0f;
        }
        else
        {
            warrior->Wounds -= static_cast<float>(amount);
            warrior->Health = 6.0f - warrior->Wounds;
        }
    }
}

auto MCLogWarriorList::RemoveWarriorAtIndex(int32_t index) -> int32_t
{
    MCLogWarrior* previous = nullptr;
    MCLogWarrior* warrior = Warriors;

    for (; index > 0; --index)
    {
        previous = warrior;
        warrior = warrior->Next;
    }

    return DeleteWarrior(warrior, previous);
}

auto MCLogWarriorList::RemoveWarrior(uint8_t index) -> int32_t
{
    // The warrior whose id is index.
    MCLogWarrior* previous = nullptr;
    MCLogWarrior* warrior = Warriors;

    while (warrior != nullptr && static_cast<uint32_t>(warrior->Id) != index)
    {
        previous = warrior;
        warrior = warrior->Next;
    }

    return DeleteWarrior(warrior, previous);
}

auto MCLogWarriorList::DeleteWarrior(MCLogWarrior* warrior, MCLogWarrior* previous) -> int32_t
{
    if (warrior == nullptr)
    {
        return -1;
    }

    for (char** text : {&warrior->Name, &warrior->Callsign, &warrior->Picture, &warrior->PilotVideo,
                        &warrior->PilotAudio, &warrior->Brain, &warrior->Description})
    {
        if (*text != nullptr)
        {
            LogFree(*text);
            *text = nullptr;
        }
    }

    if (warrior->InventoryBlock != nullptr)
    {
        delete warrior->InventoryBlock;
        warrior->InventoryBlock = nullptr;
    }

    if (warrior == Warriors)
    {
        Warriors = warrior->Next;
    }
    else if (previous != nullptr)
    {
        previous->Next = warrior->Next;
    }

    LogFree(warrior);
    --NumWarriors;
    return 0;
}

auto MCLogWarriorList::GetWarriorCount() -> int32_t
{
    return NumWarriors;
}

auto MCLogWarriorList::GetWarriorSize(uint32_t index) -> int32_t
{
    for (MCLogWarrior* warrior = Warriors; warrior != nullptr; warrior = warrior->Next)
    {
        if (static_cast<uint32_t>(warrior->Id) == index)
        {
            return static_cast<int32_t>(warrior->BinarySize);
        }
    }

    return 0;
}

auto MCLogWarriorList::GetWarriorProfile(uint32_t index, char* dest) -> int32_t
{
    for (MCLogWarrior* warrior = Warriors; warrior != nullptr; warrior = warrior->Next)
    {
        if (static_cast<uint32_t>(warrior->Id) == index)
        {
            std::strcpy(dest, warrior->FileName);
            return 0;
        }
    }

    return -1;
}

auto MCLogWarriorList::GetWarriorBrain(uint32_t index, char* dest) -> int32_t
{
    for (MCLogWarrior* warrior = Warriors; warrior != nullptr; warrior = warrior->Next)
    {
        if (static_cast<uint32_t>(warrior->Id) == index)
        {
            std::strcpy(dest, warrior->Brain);
            return 0;
        }
    }

    return -1;
}

auto MCLogWarriorList::GetID(int32_t index) -> int32_t
{
    if (NumWarriors <= index)
    {
        return -1;
    }

    MCLogWarrior* warrior = Warriors;

    for (; index > 0; --index)
    {
        warrior = warrior->Next;
    }

    return warrior->Id;
}

auto MCLogWarriorList::GetBinaryData(uint32_t index, void* data) -> int32_t
{
    MCLogWarrior* warrior = Warriors;

    while (warrior != nullptr && static_cast<uint32_t>(warrior->Id) != index)
    {
        warrior = warrior->Next;
    }

    if (warrior == nullptr)
    {
        return -1;
    }

    // Port fix (OB-089): the original copied binarySize bytes from the record (reading on past it) and put the
    // strings after them; the port writes the record's image and then the strings.
    auto* out = static_cast<uint8_t*>(data);
    MCImageWriter writer(out, WarriorImageSize);
    VisitWarrior(writer, *warrior);
    out += WarriorImageSize;
    out = PutString(out, warrior->Name);
    out = PutString(out, warrior->Callsign);
    out = PutString(out, warrior->Picture);
    out = PutString(out, warrior->PilotVideo);
    out = PutString(out, warrior->PilotAudio);
    PutString(out, warrior->Brain);
    return 0;
}

auto MCLogWarriorList::SaveWarriorText(char* fileName, int32_t index) -> int32_t
{
    if (NumWarriors <= index)
    {
        return -1;
    }

    MCLogWarrior* warrior = Warriors;

    for (int32_t count = index; count > 0; --count)
    {
        warrior = warrior->Next;
    }

    MCMissionLogisticsBridge bridge;
    return bridge.LogisticsWarriorProfileWriter(fileName, warrior);
}

auto MCLogWarriorList::GetWarriorInfo(int32_t index, MCLogWarrior*& warrior) -> int32_t
{
    warrior = nullptr;

    if (index >= NumWarriors)
    {
        return -1;
    }

    MCLogWarrior* current = Warriors;

    for (; index > 0; --index)
    {
        current = current->Next;
    }

    if (current == nullptr)
    {
        return -1;
    }

    warrior = current;
    return 0;
}

auto MCLogWarriorList::GetWarriorIndex(MCLogWarrior* warrior) -> int32_t
{
    int32_t index = 0;

    for (MCLogWarrior* current = Warriors; current != nullptr; current = current->Next, ++index)
    {
        if (current == warrior)
        {
            return index;
        }
    }

    return -1;
}

auto MCLogWarriorList::Exists(char* fileName) -> int
{
    for (MCLogWarrior* warrior = Warriors; warrior != nullptr; warrior = warrior->Next)
    {
        if (std::strcmp(warrior->Callsign, fileName) == 0)
        {
            return 1;
        }
    }

    return 0;
}

auto MCLogWarriorList::SaveWarriorBinary(char* fileName, int32_t index) -> int32_t
{
    MCFile file;
    char path[256];
    std::snprintf(path, sizeof(path), "%s%s", SavePath, fileName);
    file.Create(path);
    file.WriteLong(NumWarriors);
    // Port fix (OB-089): the original passed the address of its buffer pointer to getBinaryData (writing the record
    // over its own stack) and then wrote from the pointer that overwrote; the port writes from the buffer.
    auto writeWarrior = [&](uint32_t id)
    {
        const int32_t size = GetWarriorSize(id);
        MCLogWarrior* warrior = Warriors;

        while (warrior != nullptr && static_cast<uint32_t>(warrior->Id) != id)
        {
            warrior = warrior->Next;
        }

        std::vector<uint8_t> buffer(
            std::max<size_t>(static_cast<size_t>(size), warrior != nullptr ? WarriorDataSize(warrior) : 0));
        GetBinaryData(id, buffer.data());
        file.Write(buffer.data(), size);
    };

    if (index == -1)
    {
        for (int32_t warrior = 0; warrior < NumWarriors; ++warrior)
        {
            writeWarrior(static_cast<uint32_t>(GetID(warrior)));
        }

        file.Close();
        return NumWarriors;
    }

    const auto id = static_cast<uint32_t>(GetID(index));

    if (GetWarriorSize(id) == 0)
    {
        return -1;
    }

    writeWarrior(id);
    file.Close();
    return 0;
}

auto MCLogWarriorList::SetDeployed(int32_t index, int deployed) -> void
{
    int32_t position = 0;

    for (MCLogWarrior* warrior = Warriors; warrior != nullptr; warrior = warrior->Next, ++position)
    {
        if (position == index)
        {
            warrior->Deployed = deployed;
            return;
        }
    }
}

auto MCLogWarriorList::Reorder() -> void
{
    // Original behaviour (OB-088): the warrior at position first - 1 is carried forward by swapping it with the one
    // after it, but when a pair is not swapped the next comparison is with a warrior two or more places on, and
    // swapping those unlinks the ones between. Nothing in MCX.EXE calls it.
    if (NumWarriors > 1)
    {
        int32_t first = 1;

        do
        {
            MCLogWarrior* carried = nullptr;
            GetWarriorInfo(first - 1, carried);
            int32_t beforeCarried = first - 2;

            for (int32_t other = first; other < NumWarriors; ++other)
            {
                MCLogWarrior* next = nullptr;
                GetWarriorInfo(other, next);

                if (carried->Assigned == 0 && next->Rank <= carried->Rank)
                {
                    continue;
                }

                // The mechs piloted by the two swap pilots too.
                MCLogMech* carriedMech = nullptr;
                MCLogMech* otherMech = nullptr;
                MCLogMechList* mechs = GlobalLogPtr->MechList;

                for (int32_t index = 0; index < mechs->GetMechCount(); ++index)
                {
                    MCLogMech* mech = nullptr;
                    mechs->GetMechInfo(index, mech);

                    if (mech->PilotIndex == beforeCarried + 1)
                    {
                        carriedMech = mech;
                    }
                    else if (mech->PilotIndex == other)
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
                    carriedMech->PilotIndex = other;
                }

                if (otherMech != nullptr)
                {
                    otherMech->PilotIndex = beforeCarried + 1;
                }

                carried->Next = next->Next;
                next->Next = carried;

                if (carried == Warriors)
                {
                    Warriors = next;
                }
                else
                {
                    MCLogWarrior* previous = nullptr;
                    GetWarriorInfo(beforeCarried, previous);
                    previous->Next = next;
                }

                beforeCarried = other - 1;
            }

            ++first;
        } while (first - 1 < NumWarriors - 1);
    }

    for (int32_t index = 0; index < NumWarriors; ++index)
    {
        MCLogWarrior* warrior = nullptr;
        GetWarriorInfo(index, warrior);
        warrior->InventoryBlock->ListIndex = index;
    }
}

//---------------------------------------------------------------------------
// LogWarrior

auto MCLogWarrior::CalcRank() -> void
{
    // Evaluated in the x87's precision, as the original did.
    double weighted = 0.0;
    double totalWeight = 0.0;

    for (int32_t skill = 0; skill < 4; ++skill)
    {
        weighted = static_cast<double>(Skills[skill]) * SkillWeightings[skill] + weighted;
        totalWeight = totalWeight + SkillWeightings[skill];
    }

    for (int32_t level = 0; level < 4; ++level)
    {
        if (weighted / totalWeight < WarriorRankScale[level])
        {
            Rank = level;
            return;
        }
    }
}

auto MCLogWarrior::LoadDescription(int32_t index) -> void
{
    if (index < 0 || Description != nullptr)
    {
        return;
    }

    char* text = ReadDescription(DescIndex);

    if (text != nullptr)
    {
        Description = text;
    }
}

//---------------------------------------------------------------------------
// LogMech

namespace
{
    /// <summary>The weight class of a tonnage: 0 light (below 40), 1 medium, 2 heavy (60), 3 assault (80).</summary>
    int32_t WeightClass(float tonnage)
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

auto MCLogMech::CalcPilotModifier() -> int32_t
{
    MCLogWarriorList* pilots = GlobalLogPtr->AssignedWarriorList;

    if (PilotIndex < 0 || PilotIndex >= pilots->NumWarriors)
    {
        PilotModifier = 0;
        return 0;
    }

    MCLogWarrior* warrior = nullptr;
    pilots->GetWarriorInfo(PilotIndex, warrior);
    // A pilot ranked below the mech's weight class costs 0x400 a step; a better one gives nothing.
    PilotModifier = (warrior->Rank - WeightClass(CurTonnage)) * 0x400;

    if (PilotModifier > 0)
    {
        PilotModifier = 0;
    }

    return PilotModifier;
}

auto MCLogMech::CalcMechCost(int repaired) -> void
{
    ResourcePoints = BaseResourcePoints;

    for (MCLogInventoryItem* item = Inventory->Items; item != nullptr; item = item->Next)
    {
        const MCMasterComponent& master = Component(item->MasterID);
        const MCComponentForm form = master.Form;
        // Weapons, ammunition and equipment count only when repaired; the rest always.
        const bool fitted = form == MCComponentForm::WeaponEnergy || form == MCComponentForm::WeaponBallistic ||
                            form == MCComponentForm::WeaponMissile || form == MCComponentForm::Ammo ||
                            form == MCComponentForm::Sensor || form == MCComponentForm::Ecm ||
                            form == MCComponentForm::Probe || form == MCComponentForm::Jammer;

        if (repaired == 0 && fitted)
        {
            continue;
        }

        for (MCLogInventoryStat* stat = item->Stats; stat != nullptr; stat = stat->Next)
        {
            if (stat->Hits == 0)
            {
                ResourcePoints += master.ResourcePoints;
            }
        }
    }

    uint32_t internal = 0;

    for (const ArmorPoints& points : Internals)
    {
        internal += points.CurArmor;
    }

    uint32_t armorLeft = 0;

    for (const ArmorPoints& points : Armor)
    {
        armorLeft += points.CurArmor;
    }

    ResourcePoints = static_cast<int32_t>(static_cast<uint32_t>(ResourcePoints) + internal * 0x32 + armorLeft * 0x28);
}

auto MCLogMech::CalcBR() -> int32_t
{
    // The undamaged copies' battle ratings, summed in the x87's precision, plus the chassis's, truncated.
    double rating = 0.0;

    for (MCLogInventoryItem* item = Inventory->Items; item != nullptr; item = item->Next)
    {
        for (MCLogInventoryStat* stat = item->Stats; stat != nullptr; stat = stat->Next)
        {
            if (stat->Hits == 0)
            {
                rating += Component(item->MasterID).BattleRating;
            }
        }
    }

    BattleRating = static_cast<int32_t>(static_cast<double>(ChassisBR) + rating);
    return BattleRating;
}

auto MCLogMech::PlaceItem(uint8_t masterID, int32_t itemNum, int32_t hits) -> void
{
    // An empty critical slot's item number.
    constexpr uint8_t emptySlot = 0xff;
    constexpr int32_t maxSlots = 12;
    // The critical slots as one run: location * maxSlots + slot.
    ItemSlot* slots = &ItemSlots[0][0];
    const auto number = static_cast<uint8_t>(itemNum);
    const auto damage = static_cast<uint8_t>(hits);
    auto fill = [&](int32_t location, bool setMaster)
    {
        for (int32_t slot = 0; slot < NumLocationCriticalSpaces[location]; ++slot)
        {
            ItemSlot& entry = slots[location * maxSlots + slot];

            if (entry.Row == emptySlot)
            {
                entry.Row = number;
                entry.Column = damage;

                if (setMaster)
                {
                    entry.MasterID = masterID;
                }

                return;
            }
        }
    };

    auto holds = [&](int32_t location)
    {
        for (int32_t slot = 0; slot < NumLocationCriticalSpaces[location]; ++slot)
        {
            if (slots[location * maxSlots + slot].MasterID == masterID)
            {
                return true;
            }
        }

        return false;
    };

    if (masterID < 100)
    {
        switch (Component(masterID).Form)
        {
            case MCComponentForm::Cockpit:
            case MCComponentForm::Sensor:
            case MCComponentForm::LifeSupport:
            case MCComponentForm::Ecm:
            case MCComponentForm::Probe:
            {
                // Head equipment (the component is not recorded).
                fill(MechHead, false);
                return;
            }
            case MCComponentForm::Actuator:
            {
                if (masterID != 4 && masterID != 0x21)
                {
                    // Leg actuators: the left leg, or the right leg when the left already has one.
                    if (holds(MechLeftLeg))
                    {
                        fill(MechRightLeg, true);
                    }
                    else
                    {
                        fill(MechLeftLeg, true);
                    }

                    return;
                }

                // Arm actuators (4 and 0x21): the left arm, or the right arm when the left already has one.
                if (holds(MechLeftArm))
                {
                    fill(MechRightArm, true);
                }
                else
                {
                    fill(MechLeftArm, true);
                }

                return;
            }
            case MCComponentForm::Engine:
            case MCComponentForm::Gyroscope:
            {
                // Centre torso (the component is not recorded).
                fill(MechCenterTorso, false);
                return;
            }
            case MCComponentForm::JumpJet:
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
                        if (SlotForm(slots[location * maxSlots + slot].MasterID) == MCComponentForm::JumpJet)
                        {
                            ++count;
                        }
                    }

                    return count;
                };

                const int32_t location = countJets(MechRightLeg) < countJets(MechLeftLeg) ? MechRightLeg : MechLeftLeg;
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
    static constexpr int32_t smallWeaponOrder[4] = {MechLeftArm, MechRightArm, MechLeftTorso, MechRightTorso};
    static constexpr int32_t largeWeaponOrder[4] = {MechLeftTorso, MechRightTorso, MechLeftArm, MechRightArm};
    const bool large = GetWeaponLarge(masterID) != 0;
    int32_t location = -1;
    int32_t fewest = 0;

    for (const int32_t candidate : large ? largeWeaponOrder : smallWeaponOrder)
    {
        bool hasRoom = false;

        for (int32_t slot = 0; slot < NumLocationCriticalSpaces[candidate]; ++slot)
        {
            if (slots[candidate * maxSlots + slot].Row == emptySlot)
            {
                hasRoom = true;
                break;
            }
        }

        if (!hasRoom)
        {
            continue;
        }

        const int32_t count = large ? GetLargeWeaponCount(candidate) : GetSmallWeaponCount(candidate);

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

auto MCLogMech::GetWeaponLarge(uint8_t masterID) -> int32_t
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

auto MCLogMech::GetLargeWeaponCount(int32_t location) -> int32_t
{
    int32_t count = 0;

    for (const ItemSlot& slot : ItemSlots[location])
    {
        count += GetWeaponLarge(slot.MasterID);
    }

    return count;
}

auto MCLogMech::GetSmallWeaponCount(int32_t location) -> int32_t
{
    int32_t count = 0;

    for (const ItemSlot& slot : ItemSlots[location])
    {
        const MCComponentForm form = SlotForm(slot.MasterID);

        // Ammunition counts as a small weapon.
        if ((form == MCComponentForm::WeaponEnergy || form == MCComponentForm::WeaponBallistic ||
             form == MCComponentForm::WeaponMissile || form == MCComponentForm::Ammo) &&
            GetWeaponLarge(slot.MasterID) == 0)
        {
            ++count;
        }
    }

    return count;
}

auto MCLogMech::LoadDescription(int32_t index) -> void
{
    if (index < 0 || Description != nullptr)
    {
        return;
    }

    char* text = ReadDescription(DescIndex);

    if (text != nullptr)
    {
        Description = text;
    }
}

//---------------------------------------------------------------------------
// LogVehicle

auto MCLogVehicle::CalcVehicleCost() -> void
{
    VehicleResourcePoints = BaseVehicleResourcePoints;

    for (MCLogInventoryItem* item = Inventory->Items; item != nullptr; item = item->Next)
    {
        VehicleResourcePoints += Component(item->MasterID).ResourcePoints * item->Count;
    }
}

auto MCLogVehicle::LoadDescription(int32_t index) -> void
{
    if (index < 0 || Description != nullptr)
    {
        return;
    }

    char* text = ReadDescription(DescIndex);

    if (text != nullptr)
    {
        Description = text;
    }
}

//---------------------------------------------------------------------------
// LogMechList

MCLogMechList::MCLogMechList()
{
    Mechs = nullptr;
    NumMechs = 0;
}

auto MCLogMechList::Destroy() -> void
{
    while (NumMechs != 0)
    {
        RemoveMech(static_cast<uint8_t>(0));
    }

    Mechs = nullptr;
}

auto MCLogMechList::GetMechIndex(MCLogMech* mech) -> int32_t
{
    MCLogMech* current = Mechs;

    for (int32_t index = 0; index < NumMechs; ++index)
    {
        if (current == mech)
        {
            return index;
        }

        current = current->Next;
    }

    return -1;
}

auto MCLogMechList::AddMech(char* fileName, int required, int sorted, int widgets) -> MCLogMech*
{
    MCFitIniFile file;
    std::string path;
    path = GamePath(ProfilePath, fileName, ".fit");
    const int32_t result = file.Open(path);
    Assert(result == 0, 0, "(addMech) Could not open file");
    MCLogMech* mech = AddMech(&file, required, sorted, widgets);
    std::strncpy(mech->ProfileName, fileName, 9);
    file.Close();
    return mech;
}

auto MCLogMechList::ReplaceMech(MCPacketFile* file, int32_t index) -> int32_t
{
    MCFitIniFile profile;
    int32_t result = file->SeekPacket(index);
    Assert(result == 0, 0, " Unable to find Mech file ");
    result = profile.Open(file, static_cast<uint32_t>(file->GetPacketSize()));
    Assert(result == 0, 0, " Unable to open mech file ");
    result = profile.SeekBlock("General");
    Assert(result == 0, static_cast<uint32_t>(result), " Bad Saved Mech file ");
    int32_t pilot = 0;
    result = profile.ReadIdLong("Pilot", pilot);
    Assert(result == 0, static_cast<uint32_t>(result), " No Pilot in Saved Mech file ");
    profile.Close();

    // The mech flown by that pilot is replaced by the saved one.
    MCLogMech* previous = nullptr;
    MCLogMech* mech = Mechs;

    while (mech != nullptr && mech->PilotIndex != pilot)
    {
        previous = mech;
        mech = mech->Next;
    }

    if (mech == nullptr)
    {
        return 5;
    }

    if (mech->FileName != nullptr)
    {
        LogFree(mech->FileName);
        mech->FileName = nullptr;
    }

    if (mech->IconName != nullptr)
    {
        LogFree(mech->IconName);
        mech->IconName = nullptr;
    }

    if (mech->Inventory != nullptr)
    {
        mech->Inventory->Destroy();
        delete mech->Inventory;
        mech->Inventory = nullptr;
    }

    // Port fix (OB-090): the original freed the mech but not its widgets, which went on pointing at it (the repair
    // block from the repair screen's list), nor its other strings. The port removes and frees them.
    if (GlobalLogPtr->RepairScreen != nullptr && mech->RepairBlock != nullptr)
    {
        GlobalLogPtr->RepairScreen->UnitPane->RemoveChild(mech->RepairBlock);
    }

    delete mech->BriefingBox;
    delete mech->RepairBlock;
    delete mech->InventoryBlock;
    delete mech->BriefBlock;

    for (char* text : {mech->WeightClassName, mech->ChassisClassName, mech->ExtraName1, mech->ExtraName2,
                       mech->Description, mech->MechName})
    {
        if (text != nullptr)
        {
            LogFree(text);
        }
    }

    if (previous == nullptr)
    {
        Mechs = mech->Next;
    }
    else
    {
        previous->Next = mech->Next;
    }

    LogFree(mech);
    --NumMechs;
    AddMech(file, index);
    return 0;
}

auto MCLogMechList::AddMech(MCPacketFile* file, int32_t packet) -> MCLogMech*
{
    MCFitIniFile profile;
    int32_t result = file->SeekPacket(packet);
    Assert(result == 0, static_cast<uint32_t>(result), "Campaign file cannot find packet for mech. #1");
    result = profile.Open(file, static_cast<uint32_t>(file->GetPacketSize()));
    Assert(result == 0, static_cast<uint32_t>(result), "Campaign file cannot find packet for mech. #2");
    MCLogMech* mech = AddMech(&profile, 0, 1, 1);
    profile.Close();
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
    void CheckKey(int32_t result, int32_t number)
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
    void ReadChassisInternals(MCLogMech* mech)
    {
        MCPacketFile objects;
        char path[256];
        std::snprintf(path, sizeof(path), "%s%s", ObjectPath, ObjectPakName);
        const int32_t result = objects.Open(path);
        Assert(result == 0, 0, "(AddMech) could not open file 8");

        if (objects.SeekPacket(static_cast<int32_t>(mech->Chassis)) != 0)
        {
            return;
        }

        MCFitIniFile chassis;

        if (chassis.Open(&objects, static_cast<uint32_t>(objects.GetPacketSize())) != 0)
        {
            Fatal(static_cast<int32_t>(0xbeef0006));
        }

        if (chassis.SeekBlock("InternalStructure") != 0)
        {
            Fatal(static_cast<int32_t>(0xbeef0006));
        }

        for (int32_t location = 0; location < 8; ++location)
        {
            if (chassis.ReadIdUChar(MechLocationBlocks[location], mech->Internals[location].MaxArmor) != 0)
            {
                Fatal(static_cast<int32_t>(0xbeef0006));
            }
        }
    }
}

auto MCLogMechList::AddMech(MCFitIniFile* file, int required, int sorted, int widgets) -> MCLogMech*
{
    auto* mech = AllocRecord<MCLogMech>();
    Assert(mech != nullptr, 0, "Not enough memory for LogMech");
    mech->BriefBlock = nullptr;
    mech->NetworkPilot = nullptr;
    mech->LocalPart = 1;
    mech->PartType = 1;

    if (file->SeekBlock("General") != 0)
    {
        // A saved mech list: a count, then each record's image and its name and icon.
        // Original behaviour (OB-089): every record is read into this one mech, which is returned without being
        // added to the list (and without an inventory). Nothing in MCX.EXE writes such a file.
        file->Seek(0);
        int32_t count = file->ReadLong();
        std::array<uint8_t, MechImageSize> image{};

        while (count > 0)
        {
            file->Read(image.data(), static_cast<int32_t>(image.size()));
            MCImageReader reader(image.data());
            VisitMech(reader, *mech);
            mech->FileName = ReadImageString(file, "Not enough memory for LogMech name");
            mech->IconName = ReadImageString(file, "Not enough memory for LogMech icon");
            --count;
        }

        return mech;
    }

    mech->Inventory = new MCInventoryList;
    // Original behaviour: the check is on the mech, not on the list just made.
    Assert(mech != nullptr, 0, "Not enough memory for InventoryList");
    mech->PilotIndex = -1;

    int32_t result = file->SeekBlock("Header");
    Assert(result == 0, 0, "(AddMech) could not find Header in profile. 0");
    char text[256] = {};
    result = file->ReadIdString("FileType", text, 0x14);
    const int typeRead = result == 0;
    Assert(typeRead, 0, "(AddMech) could not find key in profile 1");

    // Original behaviour: a FileType other than MechProfile only fails when it could not be read at all.
    if (std::strcmp(text, "MechProfile") != 0)
    {
        Assert(typeRead, 0, "(AddMech) could not find key in profile 2");
    }

    result = file->SeekBlock("General");
    CheckKey(result, 3);
    file->ReadIdString("Name", text, 0x7f);
    mech->MechName = LogStrDup(text);
    CheckKey(file->ReadIdFloat("CurTonnage", mech->CurTonnage), 4);
    CheckKey(file->ReadIdChar("Status", mech->Status), 5);

    if (mech->Status == 1 || mech->Status == 2)
    {
        mech->Status = 0;
    }

    if (file->ReadIdLong("ResourcePoints", mech->ResourcePoints) != 0)
    {
        mech->ResourcePoints = 100;
    }

    result = file->ReadIdLong("NameIndex", mech->NameIndex);
    Assert(result == 0, static_cast<uint32_t>(result), "(AddMech) could not find NameIndex");
    result = file->ReadIdLong("NameVariant", mech->NameVariant);
    Assert(result == 0, static_cast<uint32_t>(result), "(AddMech) could not fine NameVariant");
    mech->BaseResourcePoints = mech->ResourcePoints;
    mech->Description = nullptr;
    mech->DescIndex = -1;
    file->ReadIdLong("DescIndex", mech->DescIndex);
    mech->LoadDescription(mech->DescIndex);
    mech->FileName = LoadLogString(static_cast<uint32_t>(mech->DescIndex + 300));
    CheckKey(file->ReadIdString("icon", text, 0x7f), 6);
    mech->IconName = LogStrDup(text);
    CheckKey(file->ReadIdULong("Chassis", mech->Chassis), 7);
    if (file->ReadIdLong("ChassisBR", mech->ChassisBR) != 0)
    {
        mech->ChassisBR = 100;
    }

    ReadChassisInternals(mech);

    if (file->ReadIdBoolean("Assigned", mech->Assigned) != 0)
    {
        mech->Assigned = 0;
    }

    if (file->ReadIdBoolean("Deployed", mech->Deployed) != 0)
    {
        mech->Deployed = 0;
    }

    if (file->ReadIdBoolean("Required", mech->Required) != 0)
    {
        mech->Required = 0;
    }

    if (file->ReadIdBoolean("NotMineYet", mech->NotMineYet) != 0)
    {
        mech->NotMineYet = 0;
    }

    if (file->ReadIdLong("Pilot", mech->PilotIndex) != 0)
    {
        mech->PilotIndex = -1;
    }

    CheckKey(file->ReadIdString("MechType", text, 0x28), 9);

    // The sort key: the name's place in the mech order, three variants apart.
    // Original behaviour: variant 1 sorts after variant 2 (1 gets +2, 2 gets +1).
    const int32_t order = MechSort[mech->NameIndex];
    mech->SortKey = order * 3;

    if (mech->NameVariant == 1)
    {
        mech->SortKey = order * 3 + 2;
    }
    else if (mech->NameVariant == 2)
    {
        mech->SortKey = order * 3 + 1;
    }

    mech->BinarySize =
        static_cast<uint32_t>(std::strlen(mech->IconName) + 1 + std::strlen(mech->FileName) + 1 + MechImageSize);

    CheckKey(file->SeekBlock("Engine"), 14);
    CheckKey(file->ReadIdFloat("Tonnage", mech->EngineTonnage), 15);
    CheckKey(file->ReadIdULong("Rating", mech->EngineRating), 16);
    CheckKey(file->ReadIdUChar("MaxRunSpeed", mech->MaxRunSpeed), 17);
    CheckKey(file->SeekBlock("Armor"), 18);
    CheckKey(file->ReadIdUChar("Type", mech->ArmorType), 19);
    CheckKey(file->ReadIdFloat("Tonnage", mech->ArmorTonnage), 20);
    CheckKey(file->SeekBlock("MaxArmorPoints"), 21);

    if (file->ReadIdLong("SellValue", mech->SellValue) != 0)
    {
        mech->SellValue = 0x32;
    }

    for (int32_t location = 0; location < 11; ++location)
    {
        CheckKey(file->ReadIdUChar(MechArmorKeys[location], mech->Armor[location].MaxArmor), 22 + location);
    }

    CheckKey(file->SeekBlock("CurArmorPoints"), 33);

    for (int32_t location = 0; location < 11; ++location)
    {
        uint8_t points = 0;
        CheckKey(file->ReadIdUChar(MechArmorKeys[location], points), 34 + location);
        mech->Armor[location].CurArmor = points;
    }

    CheckKey(file->SeekBlock("InventoryInfo"), 45);
    CheckKey(file->ReadIdUChar("NumOther", mech->NumOther), 46);
    CheckKey(file->ReadIdUChar("NumWeapons", mech->NumWeapons), 47);
    CheckKey(file->ReadIdUChar("NumAmmo", mech->NumAmmo), 48);
    // Original behaviour: 0xc0 bytes of the slots are cleared, the first five locations and a third of the sixth.
    std::memset(mech->ItemSlots, 0xff, 0xc0);

    mech->FreeTonnage = 0.0f;
    mech->WeaponTonnage = 0.0f;
    mech->UsedTonnage =
        static_cast<float>(static_cast<double>(mech->CurTonnage) * 0.1f + mech->ArmorTonnage + mech->EngineTonnage);
    MCInventoryList* inventory = mech->Inventory;
    char block[32];
    int32_t item = 0;
    const int32_t numOther = mech->NumOther;

    for (; item < numOther; ++item)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        CheckKey(file->SeekBlock(block), 49);
        uint8_t masterID = 0;
        CheckKey(file->ReadIdUChar("MasterID", masterID), 50);
        inventory->AddItem(masterID, inventory->CreateStat(static_cast<uint8_t>(item), 0, 0, 1, 0xff), -1);
        const MCMasterComponent& master = Component(masterID);
        mech->UsedTonnage += master.Tonnage;

        if (master.Form == MCComponentForm::Sensor || master.Form == MCComponentForm::Ecm ||
            master.Form == MCComponentForm::Probe || master.Form == MCComponentForm::Jammer)
        {
            mech->WeaponTonnage += master.Tonnage;
        }

        mech->ResourcePoints += master.ResourcePoints;
    }

    const int32_t weaponEnd = numOther + mech->NumWeapons;

    for (; item < weaponEnd; ++item)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        CheckKey(file->SeekBlock(block), 51);
        uint8_t masterID = 0;
        CheckKey(file->ReadIdUChar("MasterID", masterID), 52);
        uint8_t facesForward = 0;
        CheckKey(file->ReadIdUChar("FacesForward", facesForward), 53);
        inventory->AddItem(masterID, inventory->CreateStat(static_cast<uint8_t>(item), 0, facesForward, 1, 0xff), -1);
        const MCMasterComponent& master = Component(masterID);
        mech->UsedTonnage += master.Tonnage;
        mech->WeaponTonnage += master.Tonnage;
        mech->ResourcePoints += master.ResourcePoints;
    }

    const int32_t ammoEnd = numOther + mech->NumAmmo + mech->NumWeapons;

    for (; item < ammoEnd; ++item)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        CheckKey(file->SeekBlock(block), 54);
        uint8_t masterID = 0;
        CheckKey(file->ReadIdUChar("MasterID", masterID), 55);
        // The amount is read (as a long, else a byte) but not used: ammunition copies get -1.
        int32_t amount = 0;

        if (file->ReadIdLong("Amount", amount) != 0)
        {
            uint8_t smallAmount = 0;
            CheckKey(file->ReadIdUChar("Amount", smallAmount), 56);
        }

        inventory->AddItem(masterID, inventory->CreateStat(static_cast<uint8_t>(item), 0, 0, -1, 0xff), -1);
        const MCMasterComponent& master = Component(masterID);
        mech->UsedTonnage += master.Tonnage;
        mech->WeaponTonnage += master.Tonnage;
        mech->ResourcePoints += master.ResourcePoints;
    }

    mech->FreeTonnage = (mech->CurTonnage - mech->UsedTonnage) + mech->WeaponTonnage;

    for (int32_t location = 0; location < 8; ++location)
    {
        CheckKey(file->SeekBlock(MechLocationBlocks[location]), 57);
        uint8_t hasCase = 0;
        CheckKey(file->ReadIdUChar("CASE", hasCase), 58);
        mech->HasCase[location] = hasCase != 0 ? 1 : 0;
        CheckKey(file->ReadIdUChar("CurInternalStructure", mech->Internals[location].CurArmor), 59);
        CheckKey(file->ReadIdUChar("HotSpotNumber", mech->HotSpotNumber[location]), 60);

        // The critical slots only carry the damage of the copies they hold.
        for (int32_t space = 0; space < NumLocationCriticalSpaces[location]; ++space)
        {
            std::snprintf(block, sizeof(block), "Component:%d", space);
            uint8_t slot[2] = {};
            CheckKey(file->ReadIdUCharArray(block, slot, 2), 61);

            if (slot[0] < inventory->NextStatID && slot[1] != 0)
            {
                MCLogInventoryItem* owner = inventory->GetItemStatIndex(slot[0]);

                // Port fix: a copy number with no copy left (the original read a null item's master id).
                if (owner == nullptr)
                {
                    continue;
                }

                const MCComponentForm form = Component(owner->MasterID).Form;

                if (form == MCComponentForm::Weapon || form == MCComponentForm::WeaponEnergy ||
                    form == MCComponentForm::WeaponMissile || form == MCComponentForm::WeaponBallistic ||
                    form == MCComponentForm::Sensor || form == MCComponentForm::Engine ||
                    form == MCComponentForm::Ecm || form == MCComponentForm::Probe)
                {
                    inventory->HitItem(slot[0], slot[1]);
                }
            }
        }
    }

    mech->Deployed = 0;

    if (mech->Required == 0)
    {
        mech->Required = required;
    }

    // In by sort key when sorted, else at the front.
    MCLogMech* current = Mechs;

    if (sorted == 0 || current == nullptr)
    {
        Mechs = mech;
    }
    else
    {
        MCLogMech* previous = nullptr;

        do
        {
            if (mech->SortKey <= current->SortKey)
            {
                break;
            }

            previous = current;
            current = current->Next;
        } while (current != nullptr);

        if (previous == nullptr)
        {
            Mechs = mech;
        }
        else
        {
            previous->Next = mech;
        }
    }

    mech->Next = current;
    ++NumMechs;

    if (widgets == 0)
    {
        mech->RepairBlock = nullptr;
        mech->InventoryBlock = nullptr;
        mech->BriefingBox = nullptr;
    }
    else
    {
        mech->RepairBlock = new MCMechRepairBlock;
        Assert(mech->RepairBlock != nullptr, 0, " Not enough memory for repair block ");
        mech->RepairBlock->Init(mech);
        mech->InventoryBlock = new MCMechInventoryBlock;
        Assert(mech->InventoryBlock != nullptr, 0, " Not enough memory for inventory block ");
        mech->InventoryBlock->Init(mech);
        mech->BriefingBox = new MCBriefingBox;
        Assert(mech->BriefingBox != nullptr, 0, " Not enough memory for briefing block ");
        mech->BriefingBox->Init(mech, nullptr);
    }

    mech->CalcBR();
    mech->CalcPilotModifier();

    // The names shown: weight class (from the tonnage), chassis class (from the armor tonnage), the internal
    // structure's class and the jump jets' class.
    static constexpr uint32_t weightNames[4] = {0x4f, 0x50, 0x51, 0x52};
    mech->WeightClassName = LoadLogString(weightNames[WeightClass(mech->CurTonnage)]);

    uint32_t chassisName = 100;

    if (mech->ArmorTonnage > 2.0f)
    {
        chassisName = 0x4f;

        if (mech->ArmorTonnage > 7.0f)
        {
            chassisName = 0x65;

            if (mech->ArmorTonnage > 12.0f)
            {
                chassisName = mech->ArmorTonnage > 17.0f ? 0x66 : 0x51;
            }
        }
    }

    mech->ChassisClassName = LoadLogString(chassisName);

    int32_t structure = 0;

    for (const MCLogMech::ArmorPoints& points : mech->Internals)
    {
        structure += points.MaxArmor;
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

    mech->ExtraName1 = LoadLogString(structureName);

    int32_t jumpJets = 0;

    for (MCLogInventoryItem* entry = inventory->Items; entry != nullptr; entry = entry->Next)
    {
        if (Component(entry->MasterID).Form == MCComponentForm::JumpJet)
        {
            jumpJets = entry->Count;
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

    mech->ExtraName2 = LoadLogString(jumpName);
    return mech;
}

auto MCLogMechList::AddMech(MCLogMech* mech, int sorted) -> int32_t
{
    // In by tonnage when sorted, else at the front.
    MCLogMech* current = Mechs;
    MCLogMech* previous = nullptr;

    if (sorted != 0 && current != nullptr)
    {
        do
        {
            if (mech->CurTonnage <= current->CurTonnage)
            {
                break;
            }

            previous = current;
            current = current->Next;
        } while (current != nullptr);
    }

    if (previous != nullptr)
    {
        previous->Next = mech;
    }
    else
    {
        Mechs = mech;
    }

    mech->Next = current;
    ++NumMechs;
    return 0;
}

auto MCLogMechList::ExtractMech(int32_t index, MCLogMech*& mech) -> int32_t
{
    if (NumMechs <= index)
    {
        return -1;
    }

    MCLogMech* current = Mechs;

    if (index > 0)
    {
        MCLogMech* previous = nullptr;

        for (; index > 0; --index)
        {
            previous = current;
            current = current->Next;
        }

        previous->Next = current->Next;
        --NumMechs;
        mech = current;
        return 0;
    }

    Mechs = current->Next;
    --NumMechs;
    mech = current;
    return 0;
}

auto MCLogMechList::RemoveMech(uint8_t index) -> int32_t
{
    if (NumMechs <= index)
    {
        return -1;
    }

    MCLogMech* previous = nullptr;
    MCLogMech* mech = Mechs;

    for (int32_t count = index; count > 0; --count)
    {
        previous = mech;
        mech = mech->Next;
    }

    return DeleteMech(mech, previous);
}

auto MCLogMechList::RemoveMech(MCLogMech* mech) -> int32_t
{
    MCLogMech* previous = nullptr;
    MCLogMech* current = Mechs;

    while (current != mech)
    {
        if (current == nullptr)
        {
            return -1;
        }

        previous = current;
        current = current->Next;
    }

    if (current == nullptr)
    {
        return -1;
    }

    return DeleteMech(mech, previous);
}

auto MCLogMechList::DeleteMech(MCLogMech* mech, MCLogMech* previous) -> int32_t
{
    for (char** text : {&mech->WeightClassName, &mech->ChassisClassName, &mech->ExtraName1, &mech->ExtraName2})
    {
        if (*text != nullptr)
        {
            LogFree(*text);
            *text = nullptr;
        }
    }

    if (GlobalLogPtr->RepairScreen != nullptr)
    {
        GlobalLogPtr->RepairScreen->UnitPane->RemoveChild(mech->RepairBlock);
    }

    for (char** text : {&mech->FileName, &mech->IconName, &mech->Description})
    {
        if (*text != nullptr)
        {
            LogFree(*text);
            *text = nullptr;
        }
    }

    if (mech->Inventory != nullptr)
    {
        mech->Inventory->Destroy();
        delete mech->Inventory;
        mech->Inventory = nullptr;
    }

    if (mech->BriefingBox != nullptr)
    {
        delete mech->BriefingBox;
        mech->BriefingBox = nullptr;
    }

    if (mech->MechName != nullptr)
    {
        LogFree(mech->MechName);
        mech->MechName = nullptr;
    }

    if (mech->RepairBlock != nullptr)
    {
        delete mech->RepairBlock;
        mech->RepairBlock = nullptr;
    }

    if (mech->InventoryBlock != nullptr)
    {
        delete mech->InventoryBlock;
        mech->InventoryBlock = nullptr;
    }

    if (mech->BriefBlock != nullptr)
    {
        delete mech->BriefBlock;
        mech->BriefBlock = nullptr;
    }

    if (previous == nullptr)
    {
        Mechs = mech->Next;
    }
    else
    {
        previous->Next = mech->Next;
    }

    LogFree(mech);
    --NumMechs;
    return 0;
}

auto MCLogMechList::GetMechCount() -> int32_t
{
    return NumMechs;
}

auto MCLogMechList::GetMechSize(uint32_t index) -> int32_t
{
    if (static_cast<uint32_t>(NumMechs) <= index)
    {
        return 0;
    }

    MCLogMech* mech = Mechs;

    for (; static_cast<int32_t>(index) > 0; --index)
    {
        mech = mech->Next;
    }

    if (mech == nullptr)
    {
        return 0;
    }

    return mech->Inventory->GetBinaryData(nullptr) + static_cast<int32_t>(mech->BinarySize);
}

auto MCLogMechList::GetMechPilotIndex(int32_t index) -> int32_t
{
    if (index >= NumMechs)
    {
        return -1;
    }

    MCLogMech* mech = Mechs;

    for (; index > 0; --index)
    {
        mech = mech->Next;
    }

    return mech != nullptr ? mech->PilotIndex : -1;
}

auto MCLogMechList::GetMechInfo(int32_t index, MCLogMech*& mech) -> int32_t
{
    mech = nullptr;

    if (index >= NumMechs)
    {
        return -1;
    }

    MCLogMech* current = Mechs;

    for (; index > 0; --index)
    {
        current = current->Next;
    }

    if (current == nullptr)
    {
        return -1;
    }

    mech = current;
    return 0;
}

auto MCLogMechList::GetBinaryData(uint32_t index, void* data) -> int32_t
{
    if (static_cast<uint32_t>(NumMechs) <= index)
    {
        return -1;
    }

    MCLogMech* mech = Mechs;

    for (; static_cast<int32_t>(index) > 0; --index)
    {
        mech = mech->Next;
    }

    if (mech == nullptr)
    {
        return -1;
    }

    // Port fix (OB-089): the original copied binarySize bytes from the record (reading on past it) and put the
    // strings after them; the port writes the record's image, then the name, the icon and the inventory.
    auto* out = static_cast<uint8_t*>(data);
    MCImageWriter writer(out, MechImageSize);
    VisitMech(writer, *mech);
    out += MechImageSize;
    out = PutString(out, mech->FileName);
    out = PutString(out, mech->IconName);
    mech->Inventory->GetBinaryData(out);
    return 0;
}

auto MCLogMechList::SaveMechText(char* fileName, int32_t index) -> int32_t
{
    if (NumMechs <= index)
    {
        return -1;
    }

    MCLogMech* mech = Mechs;

    for (int32_t count = index; count > 0; --count)
    {
        mech = mech->Next;
    }

    MCMissionLogisticsBridge bridge;
    return bridge.LogisticsMechProfileWriter(fileName, mech, 0);
}

auto MCLogMechList::SaveMechBinary(char* fileName, int32_t index) -> int32_t
{
    MCFile file;
    char path[256];
    std::snprintf(path, sizeof(path), "%s%s.fit", SaveTempPath, fileName);
    file.Create(path);
    file.WriteLong(NumMechs);
    // Port fix (OB-089): the original handed getBinaryData the address of its buffer pointer; the port passes the
    // buffer.
    auto writeMech = [&](uint32_t mech)
    {
        const int32_t size = GetMechSize(mech);
        std::vector<uint8_t> buffer(static_cast<size_t>(std::max(size, 0)));
        GetBinaryData(mech, buffer.data());
        file.Write(buffer.data(), size);
    };

    if (index == -1)
    {
        for (int32_t mech = 0; mech < NumMechs; ++mech)
        {
            writeMech(static_cast<uint32_t>(mech));
        }

        file.Close();
        return NumMechs;
    }

    if (NumMechs <= index)
    {
        return -1;
    }

    writeMech(static_cast<uint32_t>(index));
    file.Close();
    return 0;
}

//---------------------------------------------------------------------------
// LogVehicleList

MCLogVehicleList::MCLogVehicleList()
{
    Vehicles = nullptr;
    NumVehicles = 0;
}

auto MCLogVehicleList::Destroy() -> void
{
    while (NumVehicles != 0)
    {
        RemoveVehicle(static_cast<uint8_t>(0));
    }

    Vehicles = nullptr;
}

auto MCLogVehicleList::GetVehicleIndex(MCLogVehicle* vehicle) -> int32_t
{
    MCLogVehicle* current = Vehicles;

    for (int32_t index = 0; index < NumVehicles; ++index)
    {
        if (current == vehicle)
        {
            return index;
        }

        current = current->Next;
    }

    return -1;
}

auto MCLogVehicleList::AddVehicle(char* fileName, int required, int sorted, int widgets) -> MCLogVehicle*
{
    (void)widgets; // Original behaviour: vehicles read by name always get their widgets.
    MCFitIniFile file;
    std::string path;
    path = GamePath(ProfilePath, fileName, ".fit");
    const int32_t result = file.Open(path);
    Assert(result == 0, static_cast<uint32_t>(result), " could not open vehicle Profile file ");
    MCLogVehicle* vehicle = AddVehicle(&file, required, sorted, 1);
    std::strncpy(vehicle->ProfileName, fileName, 9);
    file.Close();
    return vehicle;
}

auto MCLogVehicleList::ReplaceVehicle(MCPacketFile*, int32_t) -> int32_t
{
    return 0;
}

auto MCLogVehicleList::AddVehicle(MCPacketFile* file, int32_t packet) -> MCLogVehicle*
{
    MCFitIniFile profile;
    int32_t result = file->SeekPacket(packet);
    Assert(result == 0, 0, " Vehicle Packet Not Found ");
    result = profile.Open(file, static_cast<uint32_t>(file->GetPacketSize()));
    Assert(result == 0, 0, " Vehicle file could not open ");
    return AddVehicle(&profile, 0, 0, 1);
}

auto MCLogVehicleList::AddVehicle(MCFitIniFile* file, int required, int sorted, int widgets) -> MCLogVehicle*
{
    auto* vehicle = AllocRecord<MCLogVehicle>();
    Assert(vehicle != nullptr, 0, "Not enough memory for LogVehicle");
    vehicle->LocalPart = 1;
    vehicle->PartType = 2;

    if (file->SeekBlock("General") != 0)
    {
        // A saved vehicle list: a count, then each record's image and its name and icon.
        // Original behaviour (OB-089): every record is read into this one vehicle, which is returned without being
        // added to the list. Nothing in MCX.EXE writes such a file.
        file->Seek(0);
        int32_t count = file->ReadLong();
        std::array<uint8_t, VehicleImageSize> image{};

        while (count > 0)
        {
            file->Read(image.data(), static_cast<int32_t>(image.size()));
            MCImageReader reader(image.data());
            VisitVehicle(reader, *vehicle);
            vehicle->FileName = ReadImageString(file, "Not enough memory for LogVehicle");
            vehicle->IconName = ReadImageString(file, "Not enough memory for LogVehicle");
            --count;
        }

        return vehicle;
    }

    static const char* const locationBlocks[5] = {"Front", "Left", "Right", "Rear", "Turret"};
    int32_t result = file->SeekBlock("Header");
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 1");
    char text[256];
    result = file->ReadIdString("FileType", text, 0x7f);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 2");
    Assert(std::strcmp(text, "GroundVehicleProfile") == 0, 0, "Failed addVehicle - 2");
    result = file->SeekBlock("General");
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 3");
    vehicle->BriefBlock = nullptr;
    result = file->ReadIdLong("NameIndex", vehicle->NameIndex);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not read NameIndex in vehicle profile");
    result = file->ReadIdFloat("CurTonnage", vehicle->CurTonnage);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 4");
    result = file->ReadIdChar("Status", vehicle->Status);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 5");
    result = file->ReadIdULong("Chassis", vehicle->Chassis);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 6");
    {
        // Original behaviour: the object packet file is opened and closed again, unused.
        MCPacketFile objects;
        char path[256];
        std::snprintf(path, sizeof(path), "%s%s", ObjectPath, ObjectPakName);
        result = objects.Open(path);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 7");
    }

    // Port fix (OB-093): the original read up to 255 characters into the 9-byte crew field.
    result = file->ReadIdString("Crew", vehicle->Crew, sizeof(vehicle->Crew) - 1);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not read crew in vehicle profile");

    if (file->ReadIdLong("ResourcePoints", vehicle->VehicleResourcePoints) != 0)
    {
        vehicle->VehicleResourcePoints = 100;
    }

    vehicle->BaseVehicleResourcePoints = vehicle->VehicleResourcePoints;
    result = file->ReadIdString("icon", text, 0xff);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 7");
    vehicle->IconName = LogStrDup(text);

    if (file->ReadIdBoolean("Assigned", vehicle->Assigned) != 0)
    {
        vehicle->Assigned = 0;
    }

    if (file->ReadIdBoolean("Deployed", vehicle->Deployed) != 0)
    {
        vehicle->Deployed = 0;
    }

    if (file->ReadIdBoolean("Required", vehicle->Required) != 0)
    {
        vehicle->Required = 0;
    }

    vehicle->Description = nullptr;
    vehicle->DescIndex = -1;
    file->ReadIdLong("DescIndex", vehicle->DescIndex);
    vehicle->LoadDescription(vehicle->DescIndex);
    vehicle->FileName = LoadLogString(static_cast<uint32_t>(vehicle->DescIndex + 700));
    // Port fix (OB-089): the original never set a vehicle's saved size (its heap block's old contents stood).
    vehicle->BinarySize = static_cast<uint32_t>(std::strlen(vehicle->IconName) + 1 + std::strlen(vehicle->FileName) +
                                                1 + VehicleImageSize);

    result = file->SeekBlock("Engine");
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 9");
    result = file->ReadIdFloat("Tonnage", vehicle->EngineTonnage);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 10");
    result = file->ReadIdULong("Rating", vehicle->EngineRating);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 11");
    result = file->ReadIdUChar("MaxMoveSpeed", vehicle->MaxMoveSpeed);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 12");
    result = file->SeekBlock("Armor");
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 13");
    result = file->ReadIdUChar("Type", vehicle->ArmorType);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 14");
    result = file->ReadIdFloat("Tonnage", vehicle->ArmorTonnage);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 15");
    result = file->SeekBlock("InventoryInfo");
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 16");
    result = file->ReadIdUChar("NumOther", vehicle->NumOther);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 17");
    result = file->ReadIdUChar("NumWeapons", vehicle->NumWeapons);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 18");
    result = file->ReadIdUChar("NumAmmo", vehicle->NumAmmo);
    Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 19");
    auto* inventory = new MCInventoryList;
    vehicle->Inventory = inventory;
    Assert(inventory != nullptr, static_cast<uint32_t>(result), "Failed addVehicle - 19");

    char block[32];
    int32_t item = 0;
    const int32_t numOther = vehicle->NumOther;

    for (; item < numOther; ++item)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        result = file->SeekBlock(block);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 19a");
        uint8_t masterID = 0;
        result = file->ReadIdUChar("MasterID", masterID);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 19b");
        inventory->AddItem(masterID, inventory->CreateStat(static_cast<uint8_t>(item), 0, 0, 1, 0xff), -1);
    }

    const int32_t weaponEnd = numOther + vehicle->NumWeapons;

    for (; item < weaponEnd; ++item)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        result = file->SeekBlock(block);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 20");
        uint8_t masterID = 0;
        result = file->ReadIdUChar("MasterID", masterID);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 21");
        uint8_t facesForward = 0;
        result = file->ReadIdUChar("FacesForward", facesForward);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 22");
        inventory->AddItem(masterID, inventory->CreateStat(static_cast<uint8_t>(item), 0, facesForward, 1, 0xff), -1);
    }

    const int32_t ammoEnd = numOther + vehicle->NumAmmo + vehicle->NumWeapons;

    for (; item < ammoEnd; ++item)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        result = file->SeekBlock(block);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 23");
        uint8_t masterID = 0;
        result = file->ReadIdUChar("MasterID", masterID);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 24");
        int32_t amount = 0;

        if (file->ReadIdLong("Amount", amount) != 0)
        {
            uint8_t smallAmount = 0;
            result = file->ReadIdUChar("Amount", smallAmount);
            Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 25");
            amount = smallAmount;
        }

        inventory->AddItem(
            masterID, inventory->CreateStat(static_cast<uint8_t>(item), 0, 0, static_cast<int16_t>(amount), 0xff), -1);
    }

    for (int32_t location = 0; location < 5; ++location)
    {
        result = file->SeekBlock(locationBlocks[location]);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 26");
        result = file->ReadIdUChar("CurInternalStructure", vehicle->CurInternalStructure[location]);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 27");
        result = file->ReadIdUChar("MaxArmorPoints", vehicle->MaxArmorPoints[location]);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 28");
        result = file->ReadIdUChar("CurArmorPoints", vehicle->CurArmorPoints[location]);
        Assert(result == 0, static_cast<uint32_t>(result), "Failed addVehicle - 29");
    }

    vehicle->NotMineYet = 0;

    if (vehicle->Required == 0)
    {
        vehicle->Required = required;
    }

    vehicle->CalcVehicleCost();

    if (widgets == 0)
    {
        vehicle->InventoryBlock = nullptr;
        vehicle->RepairBlock = nullptr;
        vehicle->BriefingBox = nullptr;
    }
    else
    {
        vehicle->InventoryBlock = new MCVehicleInventoryBlock;
        Assert(vehicle->InventoryBlock != nullptr, 0, " Not enough memory for vehicleInvBlock block ");
        vehicle->InventoryBlock->Init(vehicle);
        vehicle->RepairBlock = new MCVehicleRepairBlock;
        Assert(vehicle->RepairBlock != nullptr, 0, " Not enough memory for repair block ");
        vehicle->RepairBlock->Init(vehicle);
        vehicle->BriefingBox = new MCBriefingBox;
        Assert(vehicle->BriefingBox != nullptr, 0, " Not enough memory for vehicle briefing block ");
        vehicle->BriefingBox->Init(nullptr, vehicle);
    }

    // In by tonnage when sorted, else at the front.
    MCLogVehicle* current = Vehicles;
    MCLogVehicle* previous = nullptr;

    if (sorted != 0 && current != nullptr)
    {
        do
        {
            if (vehicle->CurTonnage <= current->CurTonnage)
            {
                break;
            }

            previous = current;
            current = current->Next;
        } while (current != nullptr);
    }

    if (previous != nullptr)
    {
        previous->Next = vehicle;
    }
    else
    {
        Vehicles = vehicle;
    }

    vehicle->Next = current;
    ++NumVehicles;
    return vehicle;
}

auto MCLogVehicleList::RemoveVehicle(uint8_t index) -> int32_t
{
    if (NumVehicles <= index)
    {
        return -1;
    }

    MCLogVehicle* previous = nullptr;
    MCLogVehicle* vehicle = Vehicles;

    for (int32_t count = index; count > 0; --count)
    {
        previous = vehicle;
        vehicle = vehicle->Next;
    }

    return DeleteVehicle(vehicle, previous);
}

auto MCLogVehicleList::RemoveVehicle(MCLogVehicle* vehicle) -> int32_t
{
    MCLogVehicle* previous = nullptr;
    MCLogVehicle* current = Vehicles;

    while (current != vehicle)
    {
        if (current == nullptr)
        {
            return -1;
        }

        previous = current;
        current = current->Next;
    }

    if (current == nullptr)
    {
        return -1;
    }

    return DeleteVehicle(vehicle, previous);
}

auto MCLogVehicleList::DeleteVehicle(MCLogVehicle* vehicle, MCLogVehicle* previous) -> int32_t
{
    for (char** text : {&vehicle->FileName, &vehicle->IconName, &vehicle->Description})
    {
        if (*text != nullptr)
        {
            LogFree(*text);
            *text = nullptr;
        }
    }

    if (vehicle->Inventory != nullptr)
    {
        vehicle->Inventory->Destroy();
        delete vehicle->Inventory;
        vehicle->Inventory = nullptr;
    }

    if (vehicle->BriefingBox != nullptr)
    {
        delete vehicle->BriefingBox;
        vehicle->BriefingBox = nullptr;
    }

    if (vehicle->RepairBlock != nullptr)
    {
        delete vehicle->RepairBlock;
        vehicle->RepairBlock = nullptr;
    }

    if (vehicle->InventoryBlock != nullptr)
    {
        delete vehicle->InventoryBlock;
        vehicle->InventoryBlock = nullptr;
    }

    if (vehicle->BriefBlock != nullptr)
    {
        delete vehicle->BriefBlock;
        vehicle->BriefBlock = nullptr;
    }

    if (previous == nullptr)
    {
        Vehicles = vehicle->Next;
    }
    else
    {
        previous->Next = vehicle->Next;
    }

    LogFree(vehicle);
    --NumVehicles;
    return 0;
}

auto MCLogVehicleList::GetVehicleInfo(int32_t index, MCLogVehicle*& vehicle) -> int32_t
{
    vehicle = nullptr;

    if (index >= NumVehicles)
    {
        return -1;
    }

    MCLogVehicle* current = Vehicles;

    for (; index > 0; --index)
    {
        current = current->Next;
    }

    if (current == nullptr)
    {
        return -1;
    }

    vehicle = current;
    return 0;
}

auto MCLogVehicleList::GetVehicleCount() -> int32_t
{
    return NumVehicles;
}

auto MCLogVehicleList::GetVehicleSize(uint32_t index) -> int32_t
{
    if (static_cast<uint32_t>(NumVehicles) <= index)
    {
        return 0;
    }

    MCLogVehicle* vehicle = Vehicles;

    for (; static_cast<int32_t>(index) > 0; --index)
    {
        vehicle = vehicle->Next;
    }

    if (vehicle == nullptr)
    {
        return 0;
    }

    return vehicle->Inventory->GetBinaryData(nullptr) + static_cast<int32_t>(vehicle->BinarySize);
}

auto MCLogVehicleList::GetBinaryData(uint32_t index, void* data) -> int32_t
{
    if (static_cast<uint32_t>(NumVehicles) <= index)
    {
        return -1;
    }

    MCLogVehicle* vehicle = Vehicles;

    for (; static_cast<int32_t>(index) > 0; --index)
    {
        vehicle = vehicle->Next;
    }

    if (vehicle == nullptr)
    {
        return -1;
    }

    // Port fix (OB-089): as LogMechList::getBinaryData.
    auto* out = static_cast<uint8_t*>(data);
    MCImageWriter writer(out, VehicleImageSize);
    VisitVehicle(writer, *vehicle);
    out += VehicleImageSize;
    out = PutString(out, vehicle->FileName);
    out = PutString(out, vehicle->IconName);
    vehicle->Inventory->GetBinaryData(out);
    return 0;
}

auto MCLogVehicleList::SaveVehicleText(char* fileName, int32_t index) -> int32_t
{
    if (NumVehicles <= index)
    {
        return -1;
    }

    MCLogVehicle* vehicle = Vehicles;

    for (int32_t count = index; count > 0; --count)
    {
        vehicle = vehicle->Next;
    }

    MCMissionLogisticsBridge bridge;
    bridge.LogisticsVehicleProfileWriter(fileName, vehicle, 0);
    return 0;
}

auto MCLogVehicleList::SaveVehicleBinary(char* fileName, int32_t index) -> int32_t
{
    MCFile file;
    char path[256];
    std::snprintf(path, sizeof(path), "%s%s.fit", SaveTempPath, fileName);
    file.Create(path);
    file.WriteLong(NumVehicles);
    // Port fix (OB-089): as LogMechList::saveMechBinary.
    auto writeVehicle = [&](uint32_t vehicle)
    {
        const int32_t size = GetVehicleSize(vehicle);
        std::vector<uint8_t> buffer(static_cast<size_t>(std::max(size, 0)));
        GetBinaryData(vehicle, buffer.data());
        file.Write(buffer.data(), size);
    };

    if (index == -1)
    {
        for (int32_t vehicle = 0; vehicle < NumVehicles; ++vehicle)
        {
            writeVehicle(static_cast<uint32_t>(vehicle));
        }

        file.Close();
        return NumVehicles;
    }

    if (NumVehicles <= index)
    {
        return -1;
    }

    writeVehicle(static_cast<uint32_t>(index));
    file.Close();
    return 1;
}

//---------------------------------------------------------------------------
// DropSlot

//---------------------------------------------------------------------------
// Free functions

auto LogisticsCallback() -> void
{
}

namespace
{
    /// <summary>
    /// What the id comparers sort by: the 32-bit value at +0x8 of the part an element points to, which is the middle
    /// of <see cref="MCLogPart::ProfileName"/> (the comparers are unused; whatever id they meant is gone).
    /// </summary>
    int32_t PartSortValue(const void* element)
    {
        const MCLogPart* part = *static_cast<const MCLogPart* const*>(element);
        int32_t value = 0;
        std::memcpy(&value, part->ProfileName + 4, sizeof(value));
        return value;
    }
}

auto CompareLogMechIDs(const void* a, const void* b) -> int
{
    const int32_t first = PartSortValue(a);
    const int32_t second = PartSortValue(b);

    if (first < second)
    {
        return -1;
    }

    return second < first ? 1 : 0;
}

auto CompareLogVehicleIDs(const void* a, const void* b) -> int
{
    const int32_t first = PartSortValue(a);
    const int32_t second = PartSortValue(b);

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

auto MCMPPlayerLights::Init() -> void
{
    MCLogObject::Init(0xd8, 0, 1, 0x10, nullptr, nullptr);
    NumPlayers = 0;

    for (uint32_t& id : PlayerIDs)
    {
        id = 0;
    }

    for (int32_t& status : PlayerStatus)
    {
        status = 0;
    }

    BackgroundParent = nullptr;
    TimerRunning = 0;
    BlinkOn = 0;
    char fileName[256];
    // One light's width comes from the first player's light.
    LightsPort = new MCLogPort;
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsc_p1.tga", ArtPath);
    LightsPort->Init(fileName);
    LightWidth = LightsPort->Width();
    LightsPort->Destroy();
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsc_pn.tga", ArtPath);
    LightsPort->Init(fileName);
    ReadyPort = new MCLogPort;
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsc_pg.tga", ArtPath);
    ReadyPort->Init(fileName);
    BlinkPort = new MCLogPort;
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsc_pg1.tga", ArtPath);
    BlinkPort->Init(fileName);
}

auto MCMPPlayerLights::Destroy() -> void
{
    if (TimerRunning != 0)
    {
        Application->RemoveTimer(this, 3);
    }

    delete LightsPort;
    LightsPort = nullptr;
    delete ReadyPort;
    ReadyPort = nullptr;
    delete BlinkPort;
    BlinkPort = nullptr;
    MCLogObject::Destroy();
}

auto MCMPPlayerLights::SetNumPlayers(int32_t count) -> void
{
    NumPlayers = count;
    Resize(LightWidth * count, Height());
}

auto MCMPPlayerLights::SetPlayerID(int32_t light, uint32_t playerID) -> void
{
    if (light < NumPlayers)
    {
        PlayerIDs[light] = playerID;
    }
}

auto MCMPPlayerLights::SetPlayerStatus(uint32_t playerID, int32_t status) -> void
{
    int32_t light = 0;

    if (NumPlayers > 0)
    {
        while (light < NumPlayers && PlayerIDs[light] != playerID)
        {
            light++;
        }
    }

    // Original behaviour (OB-099): a player not in the session sets the status of the light after the last one. Port fix:
    // with six lights that index is past playerStatus (the original overwrote lightWidth), so it is skipped.
    if (status >= 0 && status < 3 && light < MAX_PLAYERS)
    {
        PlayerStatus[light] = status;
    }

    if (TimerRunning == 0 && status == 2)
    {
        Application->AddTimer(this, 3, 500, 0, 0, 0);
        TimerRunning = 1;
    }
}

auto MCMPPlayerLights::Draw() -> void
{
    MCPane* target = Lport()->Frame();

    for (int32_t light = 0; light < std::min(NumPlayers, MAX_PLAYERS); light++)
    {
        // The numbered light, then the status over it: 1 lit, 2 blinking (while the timer runs).
        MCLogPort* lightPort = LogArtf("%slogart\\lsc_p%d.tga", ArtPath, light + 1);

        if (lightPort == nullptr)
        {
            continue;
        }

        lightPort->CopyTo(target, lightPort->Width() * light, 0, 0);
        const int32_t status = PlayerStatus[light];

        if (status == 1)
        {
            if (MCLogPort* statusPort = LogArtf("%slogart\\lsc_ph.tga", ArtPath))
            {
                statusPort->CopyTo(target, lightPort->Width() * light, 2, 1);
            }
        }
        else if (status == 2 && TimerRunning != 0)
        {
            MCLogPort* blink = BlinkOn == 0 ? BlinkPort : ReadyPort;
            blink->CopyTo(target, LightWidth * light, 2, 1);
        }
    }
}

auto MCMPPlayerLights::HandleEvent(MCGuiEvent* event) -> void
{
    if (Parent == nullptr)
    {
        return;
    }

    if (event->Type == 0x13)
    {
        BlinkOn = BlinkOn == 0 ? 1 : 0;
    }

    // Pointing at a light shows its player's name on the ticker.
    const int32_t light = (event->X - 0xd8) / LightWidth;

    if (light >= 0 && light < NumPlayers && GlobalLogPtr->Ticker != nullptr && MPlayer != nullptr)
    {
        const uint32_t playerID = PlayerIDs[light];

        if (MPlayer->SessionManager->GetPlayer(playerID) != nullptr)
        {
            GlobalLogPtr->Ticker->SetString(MPlayer->SessionManager->GetPlayer(playerID)->Name);
        }
    }
}

namespace
{
    /// <summary>Screen element <paramref name="index"/> of <paramref name="screen"/> as a <typeparamref name="T"/>.</summary>
    template <typename T> T* ScreenElement(MCGenericScreen* screen, int32_t index)
    {
        return static_cast<T*>(screen->Elements[index]);
    }

    /// <summary>A new <paramref name="width"/> x <paramref name="height"/> port with its own bitmap.</summary>
    MCLogPort* NewPort(int32_t width, int32_t height)
    {
        auto* port = new MCLogPort;
        port->Init(width, height, 1);
        return port;
    }

    /// <summary>A new port loaded from the art file <paramref name="format"/> names (its <c>%s</c> is <paramref name="path"/>).</summary>
    MCLogPort* NewPort(const char* format, const char* path)
    {
        char fileName[256];
        std::snprintf(fileName, sizeof(fileName), format, path);
        auto* port = new MCLogPort;
        port->Init(fileName);
        return port;
    }

    /// <summary>Deletes <paramref name="port"/> and clears the pointer.</summary>
    void DeletePort(MCLogPort*& port)
    {
        delete port;
        port = nullptr;
    }

    /// <summary>
    /// Loads the whole of shape file <paramref name="fileName"/> into a logistics block (the repair and icon shapes).
    /// </summary>
    void* ReadShapeFile(MCFile& file, const char* sizeError)
    {
        const uint32_t length = file.GetLength();
        void* shapes = LogAlloc(length);
        Assert(shapes != nullptr, 0, "Not enough memory for mechrep buffer");
        const int32_t read = file.Read(static_cast<uint8_t*>(shapes), static_cast<int32_t>(length));
        Assert(static_cast<uint32_t>(read) == length, 0, sizeError);
        file.Close();
        MCRenderer::RegisterData(shapes, length, MCDataKind::Shapes);
        return shapes;
    }

    /// <summary>Opens shape file <paramref name="fileName"/> (asserting it exists) and loads it (<see cref="ReadShapeFile"/>).</summary>
    void* LoadShapeFile(MCFile& file, const char* fileName, const char* openError, const char* sizeError)
    {
        const int32_t result = file.Open(fileName);
        Assert(result == 0, static_cast<uint32_t>(result), openError);
        return ReadShapeFile(file, sizeError);
    }

    /// <summary>Opens screen ini <paramref name="name"/><c>.fit</c> under <c>artPath</c>.</summary>
    void OpenScreenFile(MCFitIniFile& file, const char* name, const char* missingError)
    {
        std::string fileName;
        fileName = GamePath(ArtPath, name, ".fit");
        const int32_t result = file.Open(fileName);
        Assert(result == 0, static_cast<uint32_t>(result), missingError);
    }

    /// <summary>Makes <paramref name="screen"/>'s elements from its ini file (<see cref="OpenScreenFile"/>).</summary>
    void InitSplashScreen(MCSplashScreen* screen, MCFitIniFile& file, const char* startError)
    {
        const int32_t result = screen->Init(&file);
        Assert(result == 0, static_cast<uint32_t>(result), startError);
    }

    /// <summary>Sets up a multiplayer screen's name field: the white font, the background, a 16-character buffer.</summary>
    MCLogTextObject* SetUpNameField(MCGenericScreen* screen, int32_t index)
    {
        auto* field = ScreenElement<MCLogTextObject>(screen, index);
        field->Font = MedWhiteFont;
        field->SetBackColor(0x10);
        return field;
    }

    /// <summary>Frees every name in a <c>net*.rsp</c> list and empties it.</summary>
    /// <remarks>
    /// The original walked the list with its cursor, which for the warrior names skips a link after each removal;
    /// removing the tail rewinds the cursor to the head, so every name is still freed.
    /// </remarks>
    void FreeNameList(MCFLinkedList<char>& list)
    {
        while (list.HeadLink != nullptr)
        {
            char* name = list.HeadLink->Data;
            LogFree(name);
            list.Del(name);
        }
    }

    /// <summary>Reads a <c>net*.rsp</c> list (one name per line, each a logistics block) from <c>profilePath</c>.</summary>
    void ReadNameList(MCFile& file, const char* name, const char* missingError, MCFLinkedList<char>& list)
    {
        std::string fileName;
        fileName = GamePath(ProfilePath, name, ".rsp");
        const int32_t result = file.Open(fileName);
        Assert(result == 0, static_cast<uint32_t>(result), missingError);

        while (true)
        {
            auto* line = static_cast<char*>(LogAlloc(0x29));
            file.ReadLine(reinterpret_cast<uint8_t*>(line), 0x28);

            if (file.Eof())
            {
                LogFree(line);
                break;
            }

            list.Add(line);
        }

        file.Close();
    }
}

auto MCLogistics::Init() -> void
{
    if (EmptyFile == nullptr)
    {
        EmptyFile = static_cast<char*>(std::malloc(0xff));
        CLoadString(ThisInstance, 0x381, EmptyFile, 0xfe);
    }

    AutoPlayMovie = 0;
    MessageBuffer = nullptr;
    GlobalLogPtr = this;
    CurrentScreen = nullptr;
    DragIcon = nullptr;
    PurMechList = nullptr;
    PurVehicleList = nullptr;
    PurPilotList = nullptr;
    MissionFileName = nullptr;
    CurrentInvTab = 0;
    CurrentMission = -1;
    NextWarriorID = 1;
    ResourcePoints = -9999;
    CampaignBriefingName = nullptr;
    OperationCinema = nullptr;
    MpWarriorList = nullptr;
    PlayerLights = nullptr;
    HammerDown = 0;

    static char logisticsTitle[0x400];
    std::snprintf(logisticsTitle, sizeof(logisticsTitle), "%s -- %s", AppName, "Logistics");

    // Port: SetWindowTextA -> the SDL window's title.
    if (MCDisplay* display = MCInput::Display())
    {
        display->SetTitle(logisticsTitle);
    }

    std::strcpy(WindowTitle, logisticsTitle);

    LogisticsBlocks = std::make_unique<MCBlockStore>();
    LogisticsState = 0;

    WorkPort0 = NewPort(0x1ab, 0x1ce);
    WorkPort1 = NewPort(0x1ab, 0x1ce);

    for (int32_t lance = 0; lance < 3; ++lance)
    {
        for (int32_t slot = 0; slot < 4; ++slot)
        {
            DeploySlots[lance][slot].Unit = -1;
            DeploySlots[lance][slot].Vehicle = -1;
            DeploySlotPlacements[lance][slot] = {};
        }
    }

    std::memset(LocalDropSlot, 0, sizeof(LocalDropSlot));
    PlayerColors[0] = 1;
    PlayerColors[1] = 3;
    PlayerColors[2] = 4;
    PlayerColors[3] = 2;
    PlayerColors[4] = 6;
    PlayerColors[5] = 5;

    MechList = new MCLogMechList;
    Assert(MechList != nullptr, 0, "Could not initialize mech list");
    WarriorList = new MCLogWarriorList;
    Assert(WarriorList != nullptr, 0, "Could not initialize warrior list");
    AssignedWarriorList = new MCLogWarriorList;
    Assert(AssignedWarriorList != nullptr, 0, "Could not initialize assigndWarrior list");
    VehicleList = new MCLogVehicleList;
    Assert(VehicleList != nullptr, 0, "Could not initialize vehicles list");

    for (int32_t player = 0; player < 3; ++player)
    {
        MpMechLists[0][player] = nullptr;
        MpVehicleLists[0][player] = nullptr;
        MpMechLists[1][player] = nullptr;
        MpVehicleLists[1][player] = nullptr;
    }

    MultiplayerInitialized = 0;
    DefaultPlanningTime = 0xf0;
    PlanningTime = 0xf0;
    MpMissionName = nullptr;
    ForceMechList = new MCLogMechList;
    Assert(ForceMechList != nullptr, 0, "Could not initialize assignedMech list");
    ForceVehicleList = new MCLogVehicleList;
    Assert(ForceVehicleList != nullptr, 0, "Could not initialize assignedVehicles list");
    ComponentInventory = new MCInventoryList;
    Assert(ComponentInventory != nullptr, 0, "Could not initialize inventory list");
    PurchaseComponents = new MCInventoryList;
    Assert(PurchaseComponents != nullptr, 0, "Could not initialize purchaseInventory list");

    InventoryIconPorts[0] = NewPort("%slogart\\lsciim.tga", ArtPath);
    InventoryIconPorts[1] = NewPort("%slogart\\lsciip.tga", ArtPath);
    InventoryIconPorts[2] = NewPort("%slogart\\lsciic.tga", ArtPath);
    InventoryIconPorts[3] = NewPort("%slogart\\lsciiv.tga", ArtPath);

    LoadScreen = new MCSplashScreen;
    SaveScreen = new MCSplashScreen;

    if (InDemo == 0)
    {
        ChatWindow = new MCLogChatWindow;
        ChatWindow->Init(7, 0x44, 0xbf, 0x101, 100000);
        ChatWindow->ShowGuiWindow(0);
        MultiplayerScreen = new MCSplashScreen;
        SerialScreen = new MCSplashScreen;
        LanScreen = new MCSplashScreen;
        ModemScreen = new MCSplashScreen;
        SessionScreen = new MCSessionScreen;
        ConnectScreen = new MCSplashScreen;
        PrefScreen = new MCSplashScreen;
    }
    else
    {
        ChatWindow = nullptr;
        PrefScreen = nullptr;
        ConnectScreen = nullptr;
        ModemScreen = nullptr;
        LanScreen = nullptr;
        SerialScreen = nullptr;
        MultiplayerScreen = nullptr;
        SessionScreen = nullptr;
    }

    MainScreen = new MCSplashScreen;
    RepairScreen = new MCRepairScreen;
    RepairScreen->Init();
    BriefingScreen = new MCBriefingScreen;
    BriefingScreen->Init();
    PurchaseScreen = new MCPurchaseScreen;
    PurchaseScreen->Init();
    CurrentScreen = MainScreen;
    LogisticsState = 1;

    {
        MCFitIniFile screenFile;
        OpenScreenFile(screenFile, "mainScreen", " No Splash Screen FIT File ");
        InitSplashScreen(MainScreen, screenFile, " Unable to start splash screen ");
        screenFile.Close();
    }

    if (InDemo == 0)
    {
        char userName[0x40];
        {
            MCFitIniFile screenFile;
            OpenScreenFile(screenFile, "mpscreen", " No Connection Screen FIT File ");
            InitSplashScreen(MultiplayerScreen, screenFile, " Unable to start connect screen ");
            screenFile.Close();
        }

        {
            MCFitIniFile screenFile;
            OpenScreenFile(screenFile, "lanscreen", " No LAN Screen FIT File ");
            MCSplashScreen* screen = LanScreen;
            InitSplashScreen(screen, screenFile, " Unable to start lanScreen screen ");
            screenFile.Close();
            screen->SetEventRoutine(LanScreenHandleEvent);
            auto* players = ScreenElement<MCLogScrollTextObject>(screen, 3);
            players->SetEventRoutine(PlayerListHandleEvent);
            players->FontIndex = 1;
            ScreenElement<MCGameList>(screen, 2)->FontIndex = 1;
            auto* nameField = ScreenElement<MCLogTextObject>(screen, 4);
            auto* gameField = ScreenElement<MCLogTextObject>(screen, 10);
            auto* playersField = ScreenElement<MCLogTextObject>(screen, 11);
            gameField->Font = MedWhiteFont;
            nameField->Font = MedWhiteFont;
            playersField->Font = MedWhiteFont;
            nameField->SetBackColor(0x10);
            gameField->SetBackColor(0x10);
            playersField->SetBackColor(0x10);
            nameField->InitBuffer(0x10, 0);
            gameField->InitBuffer(0x18, 0);
            uint32_t size = 0x3f;
            char* gameName;
            char gameText[0x200];

            if (MyGetUserName(userName, &size) == 0)
            {
                nameField->SetStringBuffer(const_cast<char*>("Player"));
                gameName = const_cast<char*>("Game");
            }
            else
            {
                nameField->SetStringBuffer(userName);
                char format[0x100];
                CLoadString(ThisInstance, 0x377, format, 0xfe);
                std::snprintf(gameText, sizeof(gameText), format, userName);
                gameField->InitBuffer(0x18, 0);
                gameName = gameText;
            }

            gameField->SetStringBuffer(gameName);
            playersField->InitBuffer(2, 3);
            playersField->SetStringBuffer(const_cast<char*>("6"));
            auto* joinButton = ScreenElement<MCLogButton>(screen, 6);
            joinButton->Disabled = 1;
            LanScreen->ShowBlock(0);
        }

        {
            MCFitIniFile screenFile;
            OpenScreenFile(screenFile, "modem", " No modem Screen FIT File ");
            MCSplashScreen* screen = ModemScreen;
            InitSplashScreen(screen, screenFile, " Unable to start modemScreen screen ");
            screenFile.Close();
            screen->SetEventRoutine(ModemScreenHandleEvent);
            auto* nameField = ScreenElement<MCLogTextObject>(screen, 4);
            auto* phoneField = ScreenElement<MCLogTextObject>(screen, 5);
            nameField->Font = MedWhiteFont;
            phoneField->Font = MedWhiteFont;
            nameField->SetBackColor(0x10);
            phoneField->SetBackColor(0x10);
            nameField->InitBuffer(0x10, 0);
            phoneField->InitBuffer(0x18, 0);
            uint32_t size = 0x3f;
            nameField->SetStringBuffer(MyGetUserName(userName, &size) == 0 ? const_cast<char*>("Player") : userName);
            auto* modems = ScreenElement<MCLogScrollTextObject>(screen, 10);
            modems->FontIndex = 1;
            modems->HighlightColor[0] = 0x14;
            modems->HighlightLine[0] = 0;
            modems->SetEventRoutine(ModemListHandleEvent);
            screen->ShowBlock(0);
        }

        {
            MCFitIniFile screenFile;
            // The serial screen reuses the modem screen's messages.
            OpenScreenFile(screenFile, "serial", " No modem Screen FIT File ");
            MCSplashScreen* screen = SerialScreen;
            InitSplashScreen(screen, screenFile, " Unable to start modemScreen screen ");
            screenFile.Close();
            MCLogTextObject* nameField = SetUpNameField(screen, 4);
            nameField->InitBuffer(0x10, 0);
            uint32_t size = 0x3f;
            nameField->SetStringBuffer(MyGetUserName(userName, &size) == 0 ? const_cast<char*>("Player") : userName);
            MCLogTextObject* portField = SetUpNameField(screen, 5);
            portField->InitBuffer(2, 1);
            portField->SetEventRoutine(ComPortTextHandleEvent);
            portField->SetStringBuffer(const_cast<char*>("1"));
            SerialScreen->SetEventRoutine(SerialScreenHandleEvent);
        }

        {
            MCFitIniFile screenFile;
            OpenScreenFile(screenFile, "readyroom", " No Ready Room Screen FIT File ");
            MCSplashScreen* screen = ConnectScreen;
            InitSplashScreen(screen, screenFile, " Unable to start readyRoomScreen screen ");
            screenFile.Close();
            auto* players = ScreenElement<MCLogScrollTextObject>(screen, 3);
            players->SetEventRoutine(ReadyRoomPlayerListHandleEvent);
            auto* goButton = ScreenElement<MCLogButton>(screen, 2);
            players->FontIndex = 1;
            goButton->Disabled = 1;
        }
    }

    MCFitIniFile loadScreenFile;
    OpenScreenFile(loadScreenFile, "loadScreen", " No Load Screen FIT File ");
    InitSplashScreen(LoadScreen, loadScreenFile, " Unable to start load screen ");
    LoadScreen->SetEventRoutine(LoadSaveScreenHandleEvent);
    MCFitIniFile saveScreenFile;
    OpenScreenFile(saveScreenFile, "saveScreen", " No Save Screen FIT File ");
    InitSplashScreen(SaveScreen, saveScreenFile, " Unable to start save screen ");
    SaveScreen->SetEventRoutine(LoadSaveScreenHandleEvent);

    if (InDemo == 0)
    {
        MCFitIniFile prefScreenFile;
        // The preferences screen reuses the save screen's messages.
        OpenScreenFile(prefScreenFile, "prefScreen", " No Save Screen FIT File ");
        InitSplashScreen(PrefScreen, prefScreenFile, " Unable to start save screen ");
        PrefScreen->SetEventRoutine(PrefScreenHandleEvent);
        // Port: the difficulty and renderer choices as drop-downs.
        AddPreferenceDropDowns(PrefScreen);
        SessionScreen->Init(0, 0, 0x280, 0x1e0, nullptr);
    }

    ShowLogScreen(0, 0);

    // Under the process ID, as aSystem::init sets it: copies of the game on one machine share the user folder.
    std::snprintf(SaveTempPath, sizeof(SaveTempPath), "%stemp\\%u\\", SavePath, MCPort::ProcessId());
    // Port fix (OB-094): the original allocated a File here, and a FitIniFile after the sort tables, and never used or
    // freed either.

    char line[256];
    MCFile file;
    std::snprintf(line, sizeof(line), "%slogart\\comp.rsp", ArtPath);
    int32_t result = file.Open(line);
    Assert(result == 0, 0, " could not open componant name file ");
    NumRangeSorted = 1;

    while (true)
    {
        file.ReadLine(reinterpret_cast<uint8_t*>(line), 0x28);

        if (file.Eof())
        {
            break;
        }

        ++NumRangeSorted;
    }

    RangeSortList = new uint32_t[NumRangeSorted];
    // Original behaviour: memclear was given the entry count as the byte count; every entry is read below anyway.
    std::memset(RangeSortList, 0, NumRangeSorted);
    file.Seek(0, 0);

    for (int32_t i = 0; i < NumRangeSorted; ++i)
    {
        file.ReadLine(reinterpret_cast<uint8_t*>(line), 0x28);
        RangeSortList[i] = static_cast<uint32_t>(std::atol(line));
    }

    file.Close();
    std::snprintf(line, sizeof(line), "%sobjsort.rsp", ObjectPath);
    result = file.Open(line);
    Assert(result == 0, 0, " could not open object sort file ");

    for (int32_t i = 0; i < 0x100; ++i)
    {
        file.ReadLine(reinterpret_cast<uint8_t*>(line), 0xfe);
        ComponentSort[i] = static_cast<int32_t>(std::atol(line));
    }

    file.Close();

    InvBlockPort = NewPort("%slogart\\invblock.tga", ArtPath);
    InvTabPorts[0] = nullptr;
    InvTabPorts[1] = nullptr;
    InvTabPorts[2] = nullptr;
    InvTabPorts[3] = nullptr;

    auto* nameTicker = new MCTicker;
    nameTicker->Init();
    Ticker = nameTicker;
    nameTicker->Init(3, 3, 0xcd, 1, CurrentScreen->Lport());
    nameTicker->SetScreen(CurrentScreen);
    CurrentScreen->AddChild(nameTicker);
    nameTicker->SetFont(MedWhiteFont);
    nameTicker->BringToFront(0);
    nameTicker->ShowGuiWindow(1);
    MCLogPort* tickerBack = NewPort(0xcd, MedWhiteFont->Height());
    VfxPaneWipe(tickerBack->Frame(), 0xed);
    nameTicker->SetBackPane(tickerBack);
    delete tickerBack;

    PurchaseDialog = new MCPurchaseDlg;
    PurchaseDialog->MCLogDialogBox::Init(0xe5, 0xa2, 0xb5, 0x9c);
    ScreenWindow->AddChild(PurchaseDialog);
    MessageDialog = new MCReusableDialog;
    MessageDialog->Init(0, 0, 4, 4, nullptr);
    ScreenWindow->AddChild(MessageDialog);
    QuestionDialog = new MCReusableDialog;
    QuestionDialog->Init(0, 0, 4, 4, nullptr);
    ScreenWindow->AddChild(QuestionDialog);
    RefitDialog = new MCRefitDialog;
    RefitDialog->Init(0, 0, 4, 4, nullptr);
    ScreenWindow->AddChild(RefitDialog);

    ResourceBackPort = NewPort(0x3d, 0xc);
    VfxPaneWipe(ResourceBackPort->Frame(), 0x10);
    ClockBackPort = NewPort(0x32, 0xc);
    VfxPaneWipe(ClockBackPort->Frame(), 0x10);
    RepairBackPort = NewPort("%slogart\\lsrupm00.tga", ArtPath);

    for (int32_t i = 0; i < 0x18; ++i)
    {
        std::snprintf(line, sizeof(line), "%smechrep%02d.shp", ArtPath, i);
        MechRepShapes[i] = LoadShapeFile(file, line, "could not open mechrep shape file", "unexpected mechrep size");
        std::snprintf(line, sizeof(line), "%smi%02d.shp", ArtPath, i);
        MechIconShapes[i] = LoadShapeFile(file, line, "could not open mechicon shape file", "unexpected mechicon size");
    }

    for (int32_t i = 0; i < 0x23; ++i)
    {
        std::snprintf(line, sizeof(line), "%svr1%02d.shp", ArtPath, i);

        if (file.Open(line) == 0)
        {
            VehicleRepShapes[i] = ReadShapeFile(file, "unexpected vhclrep size");
            std::snprintf(line, sizeof(line), "%svi1%02d.shp", ArtPath, i);

            if (file.Open(line) == 0)
            {
                VehicleIconShapes[i] = ReadShapeFile(file, "unexpected vhclrep size");
            }
            else
            {
                VehicleIconShapes[i] = nullptr;
            }
        }
        else
        {
            VehicleRepShapes[i] = nullptr;
            VehicleIconShapes[i] = nullptr;
        }
    }

    // Ten remap tables: each maps every colour to 0xff (transparent) except one, which it recolours.
    static constexpr struct
    {
        uint8_t From = 0;
        uint8_t To = 0;
    } lookasideColors[10] = {{0xe8, 0xe8}, {0xe8, 0xf2}, {0xe8, 0xeb}, {0xe8, 0xef}, {0xe8, 0x13},
                             {0xe6, 0xe6}, {0xe6, 0xf1}, {0xe6, 0xf4}, {0xe6, 0xed}, {0xe6, 0x13}};

    for (int32_t i = 0; i < 10; ++i)
    {
        std::memset(ShapeLookaside[i], 0xff, sizeof(ShapeLookaside[i]));
        ShapeLookaside[i][lookasideColors[i].From] = lookasideColors[i].To;
    }

    MCRenderer::RegisterData(ShapeLookaside, sizeof(ShapeLookaside), MCDataKind::Tables);

    RepairPorts[0] = NewPort("%slogart\\lsrupm03.tga", ArtPath);
    RepairPorts[1] = NewPort("%slogart\\lsrupm01.tga", ArtPath);
    RepairPorts[2] = NewPort("%slogart\\lsrupm04.tga", ArtPath);
    RepairPorts[3] = NewPort("%slogart\\lsrupm02.tga", ArtPath);
    RepairPorts[4] = NewPort("%slogart\\lsrupm06.tga", ArtPath);
    RepairPorts[5] = NewPort("%slogart\\lsrupm07.tga", ArtPath);
    PurchasePorts[0] = NewPort("%slogart\\lspcb05.tga", ArtPath);
    PurchasePorts[1] = NewPort("%slogart\\lspcb07.tga", ArtPath);
    PurchasePorts[2] = NewPort("%slogart\\lspcb06.tga", ArtPath);
    PurchasePorts[3] = NewPort("%slogart\\lspcb09.tga", ArtPath);
    ScreenButtonPorts[0][0] = NewPort("%slogart\\lscbn00.tga", ArtPath);
    ScreenButtonPorts[0][1] = NewPort("%slogart\\lscbh00.tga", ArtPath);
    ScreenButtonPorts[0][2] = NewPort("%slogart\\lscbg00.tga", ArtPath);
    ScreenButtonPorts[1][0] = NewPort("%sbn_exit.tga", ArtPath);
    ScreenButtonPorts[1][1] = NewPort("%sbh_exit.tga", ArtPath);
    ScreenButtonPorts[1][2] = NewPort("%sbg_exit.tga", ArtPath);
    ScreenButtonPorts[2][0] = NewPort("%slogart\\lscbn01.tga", ArtPath);
    ScreenButtonPorts[2][1] = NewPort("%slogart\\lscbh01.tga", ArtPath);
    ScreenButtonPorts[2][2] = NewPort("%slogart\\lscbg01.tga", ArtPath);
    ScreenButtonPorts[3][0] = NewPort("%slogart\\lscbn02.tga", ArtPath);
    ScreenButtonPorts[3][1] = NewPort("%slogart\\lscbh02.tga", ArtPath);
    ScreenButtonPorts[3][2] = NewPort("%slogart\\lscbg02.tga", ArtPath);
    ScreenButtonPorts[4][0] = NewPort("%slogart\\lscbn03.tga", ArtPath);
    ScreenButtonPorts[4][1] = NewPort("%slogart\\lscbh03.tga", ArtPath);
    ScreenButtonPorts[4][2] = NewPort("%slogart\\lscbg03.tga", ArtPath);

    std::snprintf(line, sizeof(line), "%sgamesys.fit", MissionPath);
    MCFitIniFile gameSystemFile;
    result = gameSystemFile.Open(line);
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't open gamesys.fit");
    result = gameSystemFile.SeekBlock("Warrior");
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't find Warrior block ");
    result = gameSystemFile.ReadIdFloat("SkillMax", MaxPilotSkill);
    Assert(result == 0, static_cast<uint32_t>(result),
           " Couldn't find SkillMax variable in Warrior block of gamesys.fit ");
    result = gameSystemFile.ReadIdFloat("SkillMin", MinPilotSkill);
    Assert(result == 0, static_cast<uint32_t>(result),
           " Couldn't find SkillMin variable in Warrior block of gamesys.fit ");
    gameSystemFile.ReadIdFloatArray("SkillWeightings", SkillWeightings, 4);
    result = gameSystemFile.ReadIdFloatArray("WarriorRankScale", WarriorRankScale, 4);
    Assert(result == 0, static_cast<uint32_t>(result),
           " Couldn't find WarriorRankScale variable in Warrior block of gamesys.fit ");
    result = gameSystemFile.SeekBlock("MultiPlayerColors");
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find Multiplayer Colors block ");
    result = gameSystemFile.ReadIdLongArray("mPlayerColors", MultiPlayerColors, 6);
    Assert(result == 0, static_cast<uint32_t>(result), " Could not find multiplayer colors data ");
    gameSystemFile.Close();

    ReadyRoomTicks = 0;

    if (LaunchedFromLobby == 0 || MPlayer == nullptr || Turn != 0)
    {
        LogisticsState = 1;
        CurrentScreen = MainScreen;
    }
    else
    {
        // Started from a lobby: straight to the ready room, whose cancel button quits the game.
        CurrentScreen = ConnectScreen;
        LogisticsState = 0xe;
        ScreenElement<MCLogButton>(ConnectScreen, 1)->Callback()->SetExec(KillTheGame);
    }

    ShowLogScreen(0, 0);
}

auto MCLogistics::InitializeMultiplayer() -> void
{
    Assert(MPlayer != nullptr, 0, "initializeMultiplayer failed: no MPlayer object.");

    if (MultiplayerInitialized != 0)
    {
        return;
    }

    MCMultiPlayer* multiPlayer = MPlayer;
    std::memset(multiPlayer->PlayerSessionCheckIn, 0, sizeof(multiPlayer->PlayerSessionCheckIn));
    multiPlayer->InLogistics = 1;

    if (MessageBuffer == nullptr)
    {
        MessageBuffer = static_cast<MCFIMessageHeader*>(LogAlloc(500));
    }

    uint32_t teammates[6];
    int32_t numTeammates;
    uint32_t opponents[6];
    int32_t numOpponents;
    SessionScreen->FillDpidArray(teammates, &numTeammates, 1);
    SessionScreen->FillDpidArray(opponents, &numOpponents, 0);

    for (int32_t i = 0; i < numTeammates; ++i)
    {
        MpMechLists[0][i] = new MCLogMechList;
        MpMechLists[0][i]->PlayerID = teammates[i];
        MpVehicleLists[0][i] = new MCLogVehicleList;
        MpVehicleLists[0][i]->PlayerID = teammates[i];
    }

    for (int32_t i = 0; i < numOpponents; ++i)
    {
        MpMechLists[1][i] = new MCLogMechList;
        MpMechLists[1][i]->PlayerID = opponents[i];
        MpVehicleLists[1][i] = new MCLogVehicleList;
        MpVehicleLists[1][i]->PlayerID = opponents[i];
    }

    MCFile file;
    ReadNameList(file, "netmechs", "File <netmechs.rsp> not found", NetMechNames);
    ReadNameList(file, "netwars", "File <netwars.rsp> not found", NetWarriorNames);
    ReadNameList(file, "netvhcls", "File <netvehicles.rsp> not found", NetVehicleNames);

    int32_t localIndex = -1;

    for (int32_t i = 0; i < numTeammates; ++i)
    {
        if (teammates[i] == MPlayer->SessionManager->MyPlayer->Id)
        {
            localIndex = i;
            break;
        }
    }

    Assert(localIndex != -1, 0, "Local player not in teammate list.");
    SetupSlotsForMultiplayer(localIndex, numTeammates);
    MpMissionName = static_cast<char*>(LogAlloc(0x80));
    MpWarriorList = new MCLogWarriorList;
    MPlayer->ChatCallback = LogisticsChatCallback;

    for (int32_t lance = 0; lance < 3; ++lance)
    {
        for (int32_t slot = 0; slot < 4; ++slot)
        {
            MCDropSlot* dropSlot = new MCDropSlot;

            if (dropSlot != nullptr)
            {
                dropSlot->Lance = lance;
                dropSlot->Slot = slot;
                dropSlot->Part = nullptr;
            }

            DropSlots[lance][slot] = dropSlot;
            dropSlot = new MCDropSlot;

            if (dropSlot != nullptr)
            {
                dropSlot->Lance = lance;
                dropSlot->Slot = slot;
                dropSlot->Part = nullptr;
            }

            OpponentDropSlots[lance][slot] = dropSlot;
        }
    }

    const int8_t techBase = MPlayer->HomeTeam == 0 ? SessionScreen->Team1TechBase : SessionScreen->Team2TechBase;
    std::strcpy(PurchaseFile, techBase == 1 ? "ispur" : "clanpur");
    MultiplayerInitialized = 1;
}

auto MCLogistics::SetupSlotsForMultiplayer(int playerIndex, int numPlayers) -> void
{
    const int32_t slotsEach = 12 / numPlayers;
    const int32_t first = slotsEach * playerIndex;
    std::memset(LocalDropSlot, 0, sizeof(LocalDropSlot));

    for (int32_t slot = first; slot < first + slotsEach; ++slot)
    {
        LocalDropSlot[slot] = 1;
    }
}

auto MCLogistics::DestroyMultiplayer() -> void
{
    if (MultiplayerInitialized == 0)
    {
        return;
    }

    LogFree(MpMissionName);
    MpMissionName = nullptr;

    if (MPlayer != nullptr)
    {
        MPlayer->ChatCallback = HandleAppChat;
    }

    FreeNameList(NetMechNames);
    FreeNameList(NetWarriorNames);
    FreeNameList(NetVehicleNames);

    if (MessageBuffer != nullptr)
    {
        LogFree(MessageBuffer);
        MessageBuffer = nullptr;
    }

    for (int32_t player = 0; player < 3; ++player)
    {
        // Original behaviour (OB-095): the third teammate's lists are never freed.
        if (player < 2)
        {
            if (MpMechLists[0][player] != nullptr)
            {
                MpMechLists[0][player]->Destroy();
                delete MpMechLists[0][player];
                MpMechLists[0][player] = nullptr;
            }

            if (MpVehicleLists[0][player] != nullptr)
            {
                MpVehicleLists[0][player]->Destroy();
                delete MpVehicleLists[0][player];
                MpVehicleLists[0][player] = nullptr;
            }
        }

        if (MpMechLists[1][player] != nullptr)
        {
            MpMechLists[1][player]->Destroy();
            delete MpMechLists[1][player];
            MpMechLists[1][player] = nullptr;
        }

        if (MpVehicleLists[1][player] != nullptr)
        {
            MpVehicleLists[1][player]->Destroy();
            delete MpVehicleLists[1][player];
            MpVehicleLists[1][player] = nullptr;
        }
    }

    if (MpWarriorList != nullptr)
    {
        MpWarriorList->Destroy();
        delete MpWarriorList;
    }

    MpWarriorList = nullptr;
    MultiplayerInitialized = 0;
}

auto MCLogistics::Destroy() -> void
{
    MCRenderer::UnregisterData(ShapeLookaside, sizeof(ShapeLookaside));

    if (PlayerLights != nullptr)
    {
        delete PlayerLights;
        PlayerLights = nullptr;
    }

    if (Ticker != nullptr)
    {
        delete Ticker;
        Ticker = nullptr;
    }

    if (ChatWindow != nullptr)
    {
        ChatWindow->Destroy();
        delete ChatWindow;
        ChatWindow = nullptr;
    }

    if (MultiplayerInitialized != 0)
    {
        DestroyMultiplayer();
    }

    if (CampaignBriefingName != nullptr)
    {
        LogFree(CampaignBriefingName);
        CampaignBriefingName = nullptr;
    }

    if (OperationCinema != nullptr)
    {
        LogFree(OperationCinema);
        OperationCinema = nullptr;
    }

    DeletePort(RepairBackPort);

    for (MCLogPort*& port : RepairPorts)
    {
        DeletePort(port);
    }

    for (MCLogPort*& port : PurchasePorts)
    {
        DeletePort(port);
    }

    DeletePort(WorkPort0);
    DeletePort(WorkPort1);

    for (auto& ports : ScreenButtonPorts)
    {
        for (MCLogPort*& port : ports)
        {
            DeletePort(port);
        }
    }

    for (MCLogPort*& port : InventoryIconPorts)
    {
        DeletePort(port);
    }

    for (int32_t i = 0; i < 0x18; ++i)
    {
        LogFree(MechRepShapes[i]);
        MechRepShapes[i] = nullptr;
        LogFree(MechIconShapes[i]);
        MechIconShapes[i] = nullptr;
    }

    for (int32_t i = 0; i < 0x23; ++i)
    {
        LogFree(VehicleRepShapes[i]);
        VehicleRepShapes[i] = nullptr;
        LogFree(VehicleIconShapes[i]);
        VehicleIconShapes[i] = nullptr;
    }

    DeletePort(ResourceBackPort);
    DeletePort(ClockBackPort);

    if (PurchaseDialog != nullptr)
    {
        ScreenWindow->RemoveChild(PurchaseDialog);
        delete PurchaseDialog;
        PurchaseDialog = nullptr;
    }

    if (MessageDialog != nullptr)
    {
        MessageDialog->Destroy();
        delete MessageDialog;
        MessageDialog = nullptr;
    }

    if (QuestionDialog != nullptr)
    {
        QuestionDialog->Destroy();
        delete QuestionDialog;
        QuestionDialog = nullptr;
    }

    if (RefitDialog != nullptr)
    {
        RefitDialog->Destroy();
        delete RefitDialog;
        RefitDialog = nullptr;
    }

    if (MissionFileName != nullptr)
    {
        LogFree(MissionFileName);
        MissionFileName = nullptr;
    }

    DeletePort(InvBlockPort);
    CurrentScreen = nullptr;

    if (VehicleList != nullptr)
    {
        VehicleList->Destroy();
        delete VehicleList;
        VehicleList = nullptr;
    }

    // In multiplayer the force lists stay: the mission reads them.
    if (ForceVehicleList != nullptr && MPlayer == nullptr)
    {
        ForceVehicleList->Destroy();
        delete ForceVehicleList;
        ForceVehicleList = nullptr;
    }

    if (PurMechList != nullptr)
    {
        PurMechList->Destroy();
        delete PurMechList;
        PurMechList = nullptr;
    }

    if (PurVehicleList != nullptr)
    {
        PurVehicleList->Destroy();
        delete PurVehicleList;
        PurVehicleList = nullptr;
    }

    if (PurchaseComponents != nullptr)
    {
        PurchaseComponents->Destroy();
        delete PurchaseComponents;
        PurchaseComponents = nullptr;
    }

    if (PurPilotList != nullptr)
    {
        PurPilotList->Destroy();
        delete PurPilotList;
        PurPilotList = nullptr;
    }

    if (ComponentInventory != nullptr)
    {
        ComponentInventory->Destroy();
        delete ComponentInventory;
        ComponentInventory = nullptr;
    }

    if (RangeSortList != nullptr)
    {
        delete[] RangeSortList;
        RangeSortList = nullptr;
    }

    if (WarriorList != nullptr)
    {
        WarriorList->Destroy();
        delete WarriorList;
        WarriorList = nullptr;
    }

    if (MechList != nullptr)
    {
        MechList->Destroy();
        delete MechList;
        MechList = nullptr;
    }

    if (AssignedWarriorList != nullptr)
    {
        AssignedWarriorList->Destroy();
        delete AssignedWarriorList;
        AssignedWarriorList = nullptr;
    }

    if (ForceMechList != nullptr && MPlayer == nullptr)
    {
        ForceMechList->Destroy();
        delete ForceMechList;
        ForceMechList = nullptr;
    }

    DeletePort(InvTabPorts[2]);
    DeletePort(InvTabPorts[0]);
    DeletePort(InvTabPorts[1]);
    DeletePort(InvTabPorts[3]);
    auto deleteScreen = [](auto*& screen)
    {
        if (screen != nullptr)
        {
            delete screen;
            screen = nullptr;
        }
    };

    deleteScreen(BriefingScreen);
    deleteScreen(PurchaseScreen);
    deleteScreen(RepairScreen);
    deleteScreen(MainScreen);

    if (InDemo == 0)
    {
        deleteScreen(MultiplayerScreen);
        deleteScreen(LanScreen);
        deleteScreen(ModemScreen);
        deleteScreen(SerialScreen);
        deleteScreen(ConnectScreen);
        deleteScreen(SessionScreen);
        deleteScreen(PrefScreen);
    }

    deleteScreen(LoadScreen);
    deleteScreen(SaveScreen);
    Application->SetCurrentObject(nullptr);
    ClearLogArt();
    LogisticsBlocks->Clear();
    LogisticsBlocks.reset();

    if (EmptyFile != nullptr)
    {
        std::free(EmptyFile);
        EmptyFile = nullptr;
    }
}

auto MCLogistics::ShowLogScreen(int show, int redraw) -> void
{
    if (redraw != 0)
    {
        char fileName[256];
        std::snprintf(fileName, sizeof(fileName), "%slogart\\lsrupm05.tga", ArtPath);
        Application->ActivatePaletteFromTga(fileName);
    }

    CurrentScreen->ShowGuiWindow(show);
}

auto MCLogistics::SetUpMainScreen(int fromMenu) -> int32_t
{
    if (CurrentScreen != nullptr)
    {
        ShowLogScreen(0, 0);
    }

    CurrentScreen = MainScreen;

    if (fromMenu == 0)
    {
        PreviousState = LogisticsState;
    }

    LogisticsState = 1;
    ShowLogScreen(1, 1);

    if (MPlayer != nullptr)
    {
        if (MultiplayerInitialized != 0)
        {
            DestroyMultiplayer();
        }

        delete MPlayer;
        MPlayer = nullptr;
        MCBriefingScreen* briefing = BriefingScreen;
        briefing->ChatBlinking = 0;

        if (briefing->ChatTimerOn != 0)
        {
            Application->RemoveTimer(briefing, 5);
        }

        if (PurchaseScreen->ChatBlinking != 0)
        {
            Application->RemoveTimer(PurchaseScreen, 7);
        }

        if (RepairScreen->ChatBlinking != 0)
        {
            Application->RemoveTimer(RepairScreen, 8);
        }

        briefing->BriefingBox = nullptr;

        if (GlobalLogPtr->PurchaseDialog != nullptr)
        {
            GlobalLogPtr->PurchaseDialog->ShowGuiWindow(0);
        }
    }

    if (ChatWindow != nullptr)
    {
        ChatWindow->Reset();
    }

    return 0;
}

auto MCLogistics::SetUpCampaignPurchasing(char* purchaseFileName, MCPacketFile* file) -> char*
{
    auto* purchasing = new MCFitIniFile;
    Assert(purchasing != nullptr, 0, " no RAM for scenario file ");
    char text[256];
    std::snprintf(text, sizeof(text), "%s%s.fit", MissionPath, purchaseFileName);
    int32_t result = purchasing->Open(text);
    Assert(result == 0, 0, " could not open purchasing file ");
    result = purchasing->SeekBlock("PurchaseCosts");
    Assert(result == 0, static_cast<uint32_t>(result), "Could not find PurchaseCosts block in purchasing file");
    result = purchasing->ReadIdLong("Armor", ArmorCost);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not read Armor in purchasing file");
    result = purchasing->ReadIdLong("Internal", InternalCost);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not read Internal in purchasing file");
    result = purchasing->ReadIdLong("Engine", EngineCost);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not read Engine in purchasing file");
    result = purchasing->ReadIdFloat("clan", ClanCostFactor);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not read clan in purchasing file.");
    result = purchasing->SeekBlock("PilotCosts");
    Assert(result == 0, static_cast<uint32_t>(result), "Could not find PilotCosts block in purchasing file");
    result = purchasing->ReadIdLong("Green", PilotCosts[0]);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not read Green pilot in purchasing file");
    result = purchasing->ReadIdLong("Regular", PilotCosts[1]);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not read Regular pilot in purchasing file");
    result = purchasing->ReadIdLong("Veteran", PilotCosts[2]);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not read Veteran pilot in purchasing file");
    result = purchasing->ReadIdLong("Elite", PilotCosts[3]);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not read Elite pilot in purchasing file.");

    if (CampaignBriefingName != nullptr)
    {
        LogFree(CampaignBriefingName);
        CampaignBriefingName = nullptr;
    }

    if (purchasing->SeekBlock("CampaignBriefing") == 0)
    {
        CampaignBriefingName = static_cast<char*>(LogAlloc(0x29));
        result = purchasing->ReadIdString("Filename", CampaignBriefingName, 0x29);
        Assert(result == 0, static_cast<uint32_t>(result), "Could not read Campaign Briefing cinema name");
    }

    std::snprintf(text, sizeof(text), "Mission%d", CurrentMission);
    purchasing->SeekBlock(text);
    auto* missionPurchaseFile = static_cast<char*>(LogAlloc(0xfa));
    result = purchasing->ReadIdString("PurchaseFile", missionPurchaseFile, 0xf9);
    Assert(result == 0, 0, " could not read PurchaseFile in purchasing file");

    if (OperationCinema != nullptr)
    {
        LogFree(OperationCinema);
        OperationCinema = nullptr;
    }

    OperationCinema = static_cast<char*>(LogAlloc(0x29));

    if (purchasing->ReadIdString("OperationCinema", OperationCinema, 0x29) != 0)
    {
        LogFree(OperationCinema);
        OperationCinema = nullptr;
    }

    AutoPlayMovie = 0;
    int32_t autoPlay;

    if (purchasing->ReadIdLong("AutoPlay", autoPlay) == 0 && autoPlay != 0)
    {
        AutoPlayMovie = 1;
    }

    if (purchasing->ReadIdLong("Operation", Operation) == 0)
    {
        MCBriefingScreen* briefing = BriefingScreen;
        delete briefing->OperationPicture;
        briefing->OperationPicture = new MCLogPort;
        Assert(briefing->OperationPicture != nullptr, 0, " Not enough memory for opPort ");
        std::snprintf(text, sizeof(text), CurPlanet == 0 ? "%slogart\\lsb_op%d.tga" : "%slogart\\mcxcard%d.tga",
                      ArtPath, Operation);
        briefing->OperationPicture->Init(text);
    }
    else
    {
        Operation = 0;
    }

    purchasing->Close();
    delete purchasing;
    SetUpPurchasing(file);
    return missionPurchaseFile;
}

namespace
{
    /// <summary>The counts read from a purchase file's Header block.</summary>
    struct MCPurchaseHeader
    {
        int32_t NumMechs = 0;
        int32_t NumVehicles = 0;
        int32_t NumComponents = 0;
        int32_t NumWarriors = 0;
        int32_t NumGifts = 0;
    };

    /// <summary>Reads the Header block of an open purchase file.</summary>
    MCPurchaseHeader ReadPurchaseHeader(MCFitIniFile& file, bool gifts)
    {
        MCPurchaseHeader header{};
        int32_t result = file.SeekBlock("Header");
        Assert(result == 0, 0, " could not find header block in purchasing file ");
        result = file.ReadIdLong("NumMechs", header.NumMechs);
        Assert(result == 0, 0, " could not read NumMechs in purchasing file ");
        result = file.ReadIdLong("NumVehicles", header.NumVehicles);
        Assert(result == 0, 0, " could not read NumVehicles in purchasing file ");
        result = file.ReadIdLong("NumComponants", header.NumComponents);
        Assert(result == 0, 0, " could not read NumComponants in purchasing file ");
        result = file.ReadIdLong("NumWarriors", header.NumWarriors);
        Assert(result == 0, 0, " could not read NumWarriors in purchasing file ");

        if (gifts && file.ReadIdLong("NumGifts", header.NumGifts) != 0)
        {
            header.NumGifts = 0;
        }

        return header;
    }

    /// <summary>Adds the Gift# blocks' mechs and vehicles (all "pv" profiles) to the player's lists.</summary>
    void ReadPurchaseGifts(MCFitIniFile& file, int32_t numGifts, MCLogMechList* mechs, MCLogVehicleList* vehicles)
    {
        char text[0x200];
        char fileName[12] = {};
        int32_t numAvailable = 0;

        for (int32_t gift = 0; gift < numGifts; ++gift)
        {
            std::snprintf(text, sizeof(text), "Gift%d", gift);
            int32_t result = file.SeekBlock(text);
            Assert(result == 0, 0, " could not find Gift block in purchasing file ");
            char giftType[4] = {};
            result = file.ReadIdString("GiftType", giftType, 2);
            Assert(result == 0, static_cast<uint32_t>(result), " No gift type ");
            result = file.ReadIdLong("NumAvailable", numAvailable);

            if (result == 0)
            {
                result = file.ReadIdString("Filename", fileName, 9);
            }

            Assert(result == 0, 0, "Error reading Gift data ");
            std::snprintf(text, sizeof(text), "pv%s", fileName);
            int required = 0;

            if (file.ReadIdBoolean("Required", required) != 0)
            {
                required = 0;
            }

            if (giftType[0] == 'V')
            {
                for (int32_t i = 0; i < numAvailable; ++i)
                {
                    vehicles->AddVehicle(text, required, 0, 1);
                }
            }
            else if (giftType[0] == 'M')
            {
                for (int32_t i = 0; i < numAvailable; ++i)
                {
                    mechs->AddMech(text, required, 1, 1);
                }
            }
        }
    }

    /// <summary>
    /// Adds the Mech# blocks (last first) to the shop. A variant's count and file carry over from the previous block
    /// when the block lacks them, as in the original (the port starts them at 0 and "").
    /// </summary>
    void ReadPurchaseMechs(MCFitIniFile& file, int32_t numMechs, MCPurMechList* mechs)
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
            const int32_t result = file.SeekBlock(text);
            Assert(result == 0, 0, " could not find mech block in purchasing file ");

            if (file.ReadIdLong("TypeAAvailable", countA) == 0)
            {
                file.ReadIdString("TypeAFile", fileA, 9);
            }

            if (file.ReadIdLong("TypeJAvailable", countJ) == 0)
            {
                file.ReadIdString("TypeJFile", fileJ, 9);
            }

            if (file.ReadIdLong("TypeWAvailable", countW) == 0)
            {
                file.ReadIdString("TypeWFile", fileW, 9);
            }

            mechs->AddMech(fileA, countA, fileJ, countJ, fileW, countW);
        }
    }

    /// <summary>
    /// Adds the Vehicle# blocks (last first) to the shop. <paramref name="prefix"/>: whether the file name gets "pv"
    /// in front (the multiplayer files; the campaign ones name the profile in full).
    /// </summary>
    void ReadPurchaseVehicles(MCFitIniFile& file, int32_t numVehicles, MCPurVehicleList* vehicles, bool prefix)
    {
        char text[0x200];
        char fileName[12] = {};
        int32_t numAvailable = 0;

        for (int32_t vehicle = numVehicles - 1; vehicle >= 0; --vehicle)
        {
            std::snprintf(text, sizeof(text), "Vehicle%d", vehicle);
            int32_t result = file.SeekBlock(text);
            Assert(result == 0, 0, " could not find vehicle block in purchasing file ");
            result = file.ReadIdLong("NumAvailable", numAvailable);

            if (result == 0)
            {
                result = file.ReadIdString("Filename", fileName, 9);
            }

            Assert(result == 0, 0, "Error reading Purchasing vehicle data ");

            if (prefix)
            {
                std::snprintf(text, sizeof(text), "pv%s", fileName);
                vehicles->AddVehicle(text, numAvailable);
            }
            else
            {
                vehicles->AddVehicle(fileName, numAvailable);
            }
        }
    }

    /// <summary>
    /// Adds the Componant# blocks to the shop's component list. <paramref name="blockError"/>: the campaign files'
    /// message has a typo ("omponent") the multiplayer one lacks.
    /// </summary>
    void ReadPurchaseComponents(MCFitIniFile& file, int32_t numComponents, MCInventoryList* components,
                                const char* blockError)
    {
        char text[0x100];

        for (int32_t component = 0; component < numComponents; ++component)
        {
            std::snprintf(text, sizeof(text), "Componant%d", component);
            int32_t result = file.SeekBlock(text);
            Assert(result == 0, static_cast<uint32_t>(result), blockError);
            uint8_t masterID = 0;
            result = file.ReadIdUChar("ComponantID", masterID);
            Assert(result == 0, static_cast<uint32_t>(result), " Could not find Purchasing Component masterID");
            int32_t numAvailable = 0;
            result = file.ReadIdLong("NumAvailable", numAvailable);
            Assert(result == 0, static_cast<uint32_t>(result), " Could not find Purchasing Component Num Available");
            MCLogInventoryStat* stat =
                components->CreateStat(static_cast<uint8_t>(component), 0, 0, static_cast<int16_t>(numAvailable), 0xff);
            // The widgets argument is the block number (a register the compiler left on the stack); it is never -1,
            // so every new item gets its widgets.
            components->AddItem(masterID, stat, component);
            components->LoadDescription(components->GetIndexFromMasterID(masterID), nullptr);
        }
    }

    /// <summary>
    /// Reads Warrior# block <paramref name="warrior"/>'s Profile (and Status when <paramref name="status"/> is set)
    /// and opens the profile's General block.
    /// </summary>
    void ReadPurchaseWarrior(MCFitIniFile& file, int32_t warrior, char* profile, int32_t* status,
                             MCFitIniFile& pilotFile)
    {
        char text[0x40];
        std::snprintf(text, sizeof(text), "Warrior%d", warrior);
        int32_t result = file.SeekBlock(text);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Purchasing Warrior block");
        result = file.ReadIdString("Profile", profile, 0x7f);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Purchasing Warrior profile");

        if (status != nullptr)
        {
            result = file.ReadIdLong("Status", *status);
            Assert(result == 0, static_cast<uint32_t>(result), " Could not find Purchasing Warrior Status");
        }

        std::string fileName;
        fileName = GamePath(WarriorPath, profile, ".fit");
        result = pilotFile.Open(fileName);
        Assert(result == 0, static_cast<uint32_t>(result), " could not open Purchasing Pilot profile file ");
        result = pilotFile.SeekBlock("General");
        Assert(result == 0, static_cast<uint32_t>(result), " Could find General Block in PIlot file ");
    }

    /// <summary>
    /// Replaces the shop's lists (mechs, vehicles, pilots, components) with new, empty ones.
    /// </summary>
    void ResetShop(MCLogistics* logistics)
    {
        if (logistics->PurMechList != nullptr)
        {
            logistics->PurMechList->Destroy();
            delete logistics->PurMechList;
        }

        logistics->PurMechList = new MCPurMechList;

        if (logistics->PurVehicleList != nullptr)
        {
            logistics->PurVehicleList->Destroy();
            delete logistics->PurVehicleList;
        }

        logistics->PurVehicleList = new MCPurVehicleList;

        if (logistics->PurPilotList != nullptr)
        {
            logistics->PurPilotList->Destroy();
            delete logistics->PurPilotList;
        }

        logistics->PurPilotList = new MCPurPilotList;

        if (logistics->PurchaseComponents != nullptr)
        {
            logistics->PurchaseComponents->Destroy();
            delete logistics->PurchaseComponents;
        }

        logistics->PurchaseComponents = new MCInventoryList;
        logistics->PurMechList->Init();
        logistics->PurVehicleList->Init();
        logistics->PurPilotList->First = nullptr;
        logistics->PurPilotList->Count = 0;
    }

    /// <summary>
    /// A pilot's status in the shop from the player's own copy of it (matched by DescIndex): 3 sold, 1 alive, 2 dead;
    /// -1 when the player has none. The last match wins.
    /// </summary>
    int32_t OwnPilotStatus(MCLogWarriorList* list, int32_t count, int32_t descIndex, int32_t status)
    {
        for (int32_t i = 0; i < count; ++i)
        {
            MCLogWarrior* warrior = nullptr;
            list->GetWarriorInfo(i, warrior);

            if (warrior->DescIndex == descIndex)
            {
                if (warrior->Sold != 0)
                {
                    status = 3;
                }
                else
                {
                    status = 0.0f < warrior->Health ? 1 : 2;
                }
            }
        }

        return status;
    }
}

auto MCLogistics::SetUpMPPurchasing(char* purchaseFileName) -> void
{
    auto* purchasing = new MCFitIniFile;
    ResetShop(this);
    MCPurMechList* mechs = PurMechList;
    MCPurVehicleList* vehicles = PurVehicleList;
    MCPurPilotList* pilots = PurPilotList;
    MCInventoryList* components = PurchaseComponents;

    char text[0x200];
    std::snprintf(text, sizeof(text), "%s%s.fit", MissionPath, purchaseFileName);
    const int32_t result = purchasing->Open(text);
    Assert(result == 0, 0, " could not open mission purchasing file ");
    const MCPurchaseHeader header = ReadPurchaseHeader(*purchasing, true);
    ReadPurchaseGifts(*purchasing, header.NumGifts, MechList, VehicleList);
    ReadPurchaseMechs(*purchasing, header.NumMechs, mechs);
    ReadPurchaseVehicles(*purchasing, header.NumVehicles, vehicles, true);
    ReadPurchaseComponents(*purchasing, header.NumComponents, components, " Could not find Purchasing Component block");
    MCLogWarriorList* warriors = WarriorList;

    for (int32_t warrior = 0; warrior < header.NumWarriors; ++warrior)
    {
        char profile[0x100];
        MCFitIniFile pilotFile;
        ReadPurchaseWarrior(*purchasing, warrior, profile, nullptr, pilotFile);
        char callsign[0x100];
        const int32_t callsignResult = pilotFile.ReadIdString("Callsign", callsign, 0xff);
        Assert(callsignResult == 0, static_cast<uint32_t>(callsignResult),
               " Could not find Callsign in General Block ");
        pilotFile.Close();

        // Only pilots the player does not already have are for hire.
        if (warriors->Exists(callsign) == 0 && AssignedWarriorList->Exists(callsign) == 0)
        {
            pilots->AddPilot(profile, 0);
        }
    }

    purchasing->Close();
    delete purchasing;
}

auto MCLogistics::SetUpPurchasing(MCPacketFile* file) -> void
{
    auto* purchasing = new MCFitIniFile;
    ResetShop(this);
    MCPurMechList* mechs = PurMechList;
    MCPurVehicleList* vehicles = PurVehicleList;
    MCPurPilotList* pilots = PurPilotList;
    MCInventoryList* components = PurchaseComponents;

    // The shop is the campaign file's last packet.
    file->SeekPacket(file->GetNumPackets() - 1);
    const int32_t size = file->GetPacketSize();
    Assert(size > 0, static_cast<uint32_t>(size), " Bad Purchase Data in Campaign File ");
    const int32_t result = purchasing->Open(file, static_cast<uint32_t>(size));
    Assert(result == 0, 0, " could not open mission purchasing file ");
    const MCPurchaseHeader header = ReadPurchaseHeader(*purchasing, true);
    ReadPurchaseGifts(*purchasing, header.NumGifts, MechList, VehicleList);
    ReadPurchaseMechs(*purchasing, header.NumMechs, mechs);
    ReadPurchaseVehicles(*purchasing, header.NumVehicles, vehicles, false);
    ReadPurchaseComponents(*purchasing, header.NumComponents, components, " Could not find Purchasing omponent block");
    MCLogWarriorList* warriors = WarriorList;

    for (int32_t warrior = 0; warrior < header.NumWarriors; ++warrior)
    {
        char profile[0x100];
        int32_t status = 0;
        MCFitIniFile pilotFile;
        ReadPurchaseWarrior(*purchasing, warrior, profile, &status, pilotFile);
        int32_t descIndex = 0;
        const int32_t descResult = pilotFile.ReadIdLong("DescIndex", descIndex);
        Assert(descResult == 0, static_cast<uint32_t>(descResult), " Could not find DescIndex in General Block ");
        pilotFile.Close();
        // A pilot the player has (or had) shows as sold, alive or dead; the pilot lists are read through the
        // mission's logistics object, which is this one.
        int32_t ownStatus = OwnPilotStatus(Mission->Logistics->WarriorList, warriors->NumWarriors, descIndex, -1);

        if (ownStatus == -1)
        {
            ownStatus = OwnPilotStatus(Mission->Logistics->AssignedWarriorList, AssignedWarriorList->NumWarriors,
                                       descIndex, -1);
        }

        pilots->AddPilot(profile, ownStatus != -1 ? ownStatus : status);
    }

    purchasing->Close();
    delete purchasing;
}

auto MCLogistics::SetUpOldPurchasing(char* purchaseFileName) -> void
{
    auto* purchasing = new MCFitIniFile;
    char text[0x200];
    std::snprintf(text, sizeof(text), "%s%s.fit", MissionPath, purchaseFileName);
    int32_t result = purchasing->Open(text);
    Assert(result == 0, 0, " could not open mission purchasing file ");
    const MCPurchaseHeader header = ReadPurchaseHeader(*purchasing, false);

    // Changes to the shop: the counts are added to what is there.
    char fileA[12] = {};
    int32_t countA = 0;
    int32_t countJ = 0;
    int32_t countW = 0;
    char unusedFile[12] = {};

    for (int32_t mech = header.NumMechs - 1; mech >= 0; --mech)
    {
        std::snprintf(text, sizeof(text), "Mech%d", mech);
        result = purchasing->SeekBlock(text);
        Assert(result == 0, 0, " could not find mech block in purchasing file ");

        if (purchasing->ReadIdLong("TypeAAvailable", countA) == 0)
        {
            purchasing->ReadIdString("TypeAFile", fileA, 9);
        }

        if (purchasing->ReadIdLong("TypeJAvailable", countJ) == 0)
        {
            purchasing->ReadIdString("TypeJFile", unusedFile, 9);
        }

        if (purchasing->ReadIdLong("TypeWAvailable", countW) == 0)
        {
            purchasing->ReadIdString("TypeWFile", unusedFile, 9);
        }

        PurMechList->ModMech(fileA, countA, countJ, countW);
    }

    char fileName[12] = {};
    int32_t numAvailable = 0;

    for (int32_t vehicle = header.NumVehicles - 1; vehicle >= 0; --vehicle)
    {
        std::snprintf(text, sizeof(text), "Vehicle%d", vehicle);
        result = purchasing->SeekBlock(text);
        Assert(result == 0, 0, " could not find vehicle block in purchasing file ");
        result = purchasing->ReadIdLong("NumAvailable", numAvailable);

        if (result == 0)
        {
            result = purchasing->ReadIdString("Filename", fileName, 9);
        }

        Assert(result == 0, 0, "Error reading Purchasing vehicle data ");
        std::snprintf(text, sizeof(text), "pv%s", fileName);
        PurVehicleList->ModVehicle(text, numAvailable);
    }

    MCInventoryList* components = PurchaseComponents;

    for (int32_t component = 0; component < header.NumComponents; ++component)
    {
        std::snprintf(text, sizeof(text), "Componant%d", component);
        result = purchasing->SeekBlock(text);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Purchasing omponent block");
        uint8_t masterID = 0;
        result = purchasing->ReadIdUChar("ComponantID", masterID);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Purchasing Component masterID");
        int32_t count = 0;
        result = purchasing->ReadIdLong("NumAvailable", count);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Purchasing Component Num Available");
        components->AddCountToItem(count, masterID);
    }

    MCPurPilotList* pilots = PurPilotList;

    for (int32_t warrior = 0; warrior < header.NumWarriors; ++warrior)
    {
        char profile[0x100];
        int32_t status = -1;
        MCFitIniFile pilotFile;
        ReadPurchaseWarrior(*purchasing, warrior, profile, &status, pilotFile);
        int32_t descIndex = 0;
        result = pilotFile.ReadIdLong("DescIndex", descIndex);
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find DescIndex in General Block ");
        pilotFile.Close();

        // Status 4 takes a pilot for hire off the shop; status 0 puts one back.
        for (int32_t i = 0; i < pilots->Count; ++i)
        {
            MCPurPilotData* pilot = nullptr;
            pilots->GetPilotInfo(i, pilot);

            if (pilot->DescIndex == descIndex)
            {
                if (pilot->Status == 0 && status == 4)
                {
                    pilots->SetPilotStatus(descIndex, 4);
                }

                if (pilot->Status == 4 && status == 0)
                {
                    pilots->SetPilotStatus(descIndex, 0);
                }
            }
        }
    }

    purchasing->Close();
    delete purchasing;
}

namespace
{
    /// <summary>Moves the name ticker onto <paramref name="screen"/>, at its top left.</summary>
    void MoveTicker(MCTicker* ticker, MCLogObject* screen)
    {
        if (ticker->Parent != nullptr)
        {
            ticker->Parent->RemoveChild(ticker);
        }

        screen->AddChild(ticker);
        ticker->SetPort(screen->Lport());
        ticker->SetScreen(screen);
        ticker->SetPos(3, 3);
    }

    /// <summary>Moves the multiplayer ready lights onto <paramref name="screen"/>, in front.</summary>
    void MoveLights(MCMPPlayerLights* lights, MCLogObject* screen)
    {
        lights->Parent->RemoveChild(lights);
        screen->AddChild(lights);
        lights->SetDepth(100);
    }

    /// <summary>
    /// Draws what <paramref name="pane"/> shows into <paramref name="dest"/> for a screen change: its background
    /// copy at (<paramref name="backX"/>, 1), its scrolled contents (through <paramref name="scratch"/>) at (0, 1)
    /// and its slider at the right edge.
    /// </summary>
    void DrawPaneForTransition(MCScrollPane* pane, MCLogPort* scratch, MCLogPort* dest, int32_t backX)
    {
        if (pane->BackgroundCopy != nullptr)
        {
            pane->BackgroundCopy->CopyTo(dest->Frame(), backX, 1, 1);
        }

        VfxPaneWipe(scratch->Frame(), 0xff);
        pane->DrawContentTo(scratch->Frame(), 0, 0);
        scratch->CopyTo(dest->Frame(), 0, 1, 1);
        pane->DrawSliderColumn(dest->Frame(), dest->Width() - 0xe, 1, true);
    }

    /// <summary>A scratch port the size of the purchase screen's unit pane (every screen's pane is that size).</summary>
    MCLogPort* NewPaneScratch(MCScrollPane* pane)
    {
        return NewPort(pane->Width(), pane->Height());
    }
}

auto MCLogistics::SetUpPurchaseScreen(int animate) -> int32_t
{
    MCBriefingScreen* briefing = BriefingScreen;
    briefing->StopSmackerMovies();
    Application->SetCurrentCursor(static_cast<MCCursorType>(0));

    if (MPlayer != nullptr)
    {
        if (briefing->ChatBlinking == 0)
        {
            if (PurchaseScreen->ChatBlinking != 0)
            {
                Application->RemoveTimer(PurchaseScreen, 7);
                // Original behaviour (OB-096): clears the repair screen's flag instead of the purchase screen's, so
                // the purchase screen's chat button does not blink again until the flag is cleared elsewhere.
                RepairScreen->ChatBlinking = 0;
            }
        }
        else if (PurchaseScreen->ChatBlinking == 0)
        {
            Application->AddTimer(PurchaseScreen, 7, 0xfa, 0, 0, 0);
            PurchaseScreen->ChatBlinking = 1;
        }
    }

    MCLogObject* previous = CurrentScreen;

    if (previous != nullptr)
    {
        ShowLogScreen(0, 0);
    }

    MoveTicker(Ticker, PurchaseScreen);
    CurrentScreen = PurchaseScreen;
    LogisticsState = 2;
    PurchaseScreen->DrawBackground();
    DrawScreenButtons();
    MCPurchaseScreen* screen = PurchaseScreen;

    switch (CurrentInvTab)
    {
        case 0:
        {
            screen->SetUpMechInv(1, 1);
            screen->SetUpMechPurchase();
            break;
        }
        case 1:
        {
            screen->SetUpPilotInv(1, 1);
            screen->SetUpPilotPurchase();
            break;
        }
        case 2:
        {
            screen->SetUpCompInv(1, 1);
            screen->SetUpCompPurchase();
            break;
        }
        case 3:
        {
            screen->SetUpVhclInv(1, 1);
            screen->SetUpVehiclePurchase();
            break;
        }
    }

    if (MPlayer != nullptr)
    {
        MoveLights(PlayerLights, PurchaseScreen);
    }

    ShowLogScreen(1, previous == RepairScreen || previous == BriefingScreen ? 0 : 1);

    if (animate != 0)
    {
        VfxPaneWipe(WorkPort0->Frame(), 0x10);
        VfxPaneWipe(WorkPort1->Frame(), 0x10);
        MCScrollPane* pane = PurchaseScreen->UnitPane;
        MCLogPort* scratch = NewPaneScratch(pane);
        DrawPaneForTransition(pane, scratch, WorkPort0, 0);
        int direction;

        if (previous == RepairScreen)
        {
            DrawPaneForTransition(RepairScreen->UnitPane, scratch, WorkPort1, 0);
            direction = 1;
        }
        else
        {
            MCLogPort* look = BriefingScreen->NewLookPicture();
            VfxPaneCopy(look->Frame(), 0xd3, 0x10, WorkPort1->Frame(), 0, 0, -1);
            delete look;
            direction = 0;
        }

        delete scratch;
        PurchaseScreen->UnitPane->ShowGuiWindow(0);
        Transition(WorkPort1, WorkPort0, direction);
        PurchaseScreen->UnitPane->ShowGuiWindow(1);
    }

    return 0;
}

auto MCLogistics::DrawScreenButtons() -> void
{
    MCLogObject* screen = CurrentScreen;

    if (screen != BriefingScreen && screen != PurchaseScreen && screen != RepairScreen)
    {
        return;
    }

    // (The original also made and freed an unused lPort here.)
    screen->Chrome()->HoveredButton = -1;
}

auto MCLogistics::HoverScreenButton(MCLogObject* screen, int32_t button) -> void
{
    if (MCLogScreenChrome* chrome = screen->Chrome(); chrome != nullptr)
    {
        chrome->HoveredButton = button;
    }
}

auto MCLogistics::DrawScreenChrome(MCLogObject* screen, MCPane* target) -> void
{
    const MCLogScreenChrome* chrome = screen->Chrome();

    // The ready lights' backing, under the screen's lights (the original's lights painted it into their parent).
    if (PlayerLights != nullptr && PlayerLights->Parent == screen)
    {
        if (MCLogPort* back = LogArtf("%slogart\\lsc_p0.tga", ArtPath))
        {
            back->CopyTo(target, 0xd3, 0, 0);
        }
    }

    if (screen == BriefingScreen || screen == PurchaseScreen || screen == RepairScreen)
    {
        // Button 0 is the main menu in single player, exit in multiplayer; the screen's own button is grayed, the one
        // under the mouse lit, and the briefing button blinks while the chat is unread.
        MCLogPort* const* const ports[4] = {MPlayer == nullptr ? ScreenButtonPorts[0] : ScreenButtonPorts[1],
                                            ScreenButtonPorts[2], ScreenButtonPorts[3], ScreenButtonPorts[4]};
        const int32_t own = screen == BriefingScreen ? 1 : (screen == PurchaseScreen ? 2 : 3);

        for (int32_t button = 0; button < 4; button++)
        {
            const int32_t top = 0x10 + button * 0x12;

            if (MCLogPort* face = ports[button][button == own ? 2 : 0]; face != nullptr)
            {
                face->CopyTo(target, 2, top, 0);
            }

            const bool blinking = button == 1 && chrome->BlinkLit && BriefingScreen->ChatBlinking != 0;

            if (button != own && (chrome->HoveredButton == button || blinking))
            {
                if (MCLogPort* lit = ports[button][1]; lit != nullptr)
                {
                    lit->CopyTo(target, 2, top, -1);
                }
            }
        }
    }

    if (Ticker != nullptr && Ticker->PaintScreen == screen)
    {
        Ticker->DrawLine(target);
    }

    // The resource points (not on the session screen) and the clock.
    if (screen != SessionScreen)
    {
        char text[44];
        ResourceFigureText(text, sizeof(text));
        VfxPaneCopy(ResourceBackPort->Frame(), 0, 0, target, 0x209, 2, -1);
        auto* bytes = reinterpret_cast<uint8_t*>(text);
        const int32_t textWidth = MedWhiteFont->Width(bytes);
        MedWhiteFont->WriteString(target, 0x244 - textWidth, 4, bytes, -1);
    }

    char time[sizeof(TimeString)];
    MCPort::StrTime(time);
    VfxPaneCopy(ClockBackPort->Frame(), 0, 0, target, 0x24c, 2, -1);
    MedWhiteFont->WriteString(target, 0x254, 4, reinterpret_cast<uint8_t*>(time), -1);
}

auto MCLogistics::SetUpBriefingScreen(int animate) -> int32_t
{
    Application->SetCurrentCursor(static_cast<MCCursorType>(0));
    MCLogObject* previous = CurrentScreen;

    if (previous != nullptr)
    {
        ShowLogScreen(0, 0);
    }

    MCBriefingScreen* briefing = BriefingScreen;
    MoveTicker(Ticker, briefing);
    briefing->SetUpDeploy();

    // The original blanked the local player's empty drop slots here (the screen draws them each frame), then the box
    // area under them.
    briefing->BlankBox();

    CurrentScreen = briefing;
    LogisticsState = 3;
    ShowLogScreen(1, previous == RepairScreen || previous == PurchaseScreen ? 0 : 1);
    briefing = BriefingScreen;
    briefing->MovieStarted = 0;
    briefing->SetUpMission();
    briefing->MissionPane->SetScrollPos(0.0f);
    briefing->DeployPane->SetScrollPos(0.0f);
    briefing->CalcTonnages();
    DrawScreenButtons();

    if (MPlayer == nullptr)
    {
        ChatWindow->ShowGuiWindow(0);
    }
    else
    {
        MCLogChatWindow* chat = ChatWindow;
        briefing = BriefingScreen;

        if (chat->Parent != briefing)
        {
            chat->Resize(0xe7);

            if (chat->Parent != nullptr)
            {
                chat->Parent->RemoveChild(chat);
            }

            briefing->AddChild(chat);
            chat->MoveTo(2, 0x65, 0);
        }

        MoveLights(PlayerLights, briefing);

        if (briefing->ChatBlinking != 0 && briefing->ChatTimerOn == 0)
        {
            Application->AddTimer(briefing, 5, 500, 0, 0, 0);
            briefing->ChatTimerOn = 1;
        }

        briefing->SetUpOperation();
    }

    // Show the briefing box of the first unit in the deploy pane.
    briefing = BriefingScreen;

    if (briefing->BriefingBox != nullptr)
    {
        briefing->RemoveChild(briefing->BriefingBox);
        briefing->BriefingBox = nullptr;
    }

    MCScrollPane* deployPane = briefing->DeployPane;

    if (deployPane->NumberOfChildren() != 0)
    {
        auto* block = static_cast<MCMechBriefBlock*>(deployPane->Child(0));
        MCBriefingBox* box = block->Mech != nullptr ? block->Mech->BriefingBox : block->Vehicle->BriefingBox;
        briefing->AddChild(box);
        briefing->BriefingBox = box;
        box->DrawBackground();
    }

    if (animate != 0)
    {
        MCLogPort* look = briefing->NewLookPicture();
        VfxPaneCopy(look->Frame(), 0xd3, 0x10, WorkPort0->Frame(), 0, 0, -1);
        delete look;
        MCLogPort* from = WorkPort1;
        VfxPaneWipe(from->Frame(), 0x10);
        MCLogPort* scratch = NewPaneScratch(PurchaseScreen->UnitPane);
        VfxPaneWipe(scratch->Frame(), 0xff);
        MCScrollPane* pane = previous == RepairScreen ? RepairScreen->UnitPane : PurchaseScreen->UnitPane;
        DrawPaneForTransition(pane, scratch, from, 1);
        delete scratch;
        Transition(from, WorkPort0, 1);
    }

    return 0;
}

auto MCLogistics::SetUpSessionScreen() -> int32_t
{
    if (MultiplayerInitialized != 0)
    {
        DestroyMultiplayer();
    }

    CurrentScreen->ShowGuiWindow(0);
    CurrentScreen = SessionScreen;
    ShowLogScreen(1, 1);
    LogisticsState = 8;
    SessionScreen->Activate(0);
    return 0;
}

auto MCLogistics::SetUpRepairScreen(int animate) -> int32_t
{
    MCBriefingScreen* briefing = BriefingScreen;
    briefing->StopSmackerMovies();
    Application->SetCurrentCursor(static_cast<MCCursorType>(0));

    if (MPlayer != nullptr)
    {
        MCRepairScreen* repair = RepairScreen;

        if (briefing->ChatBlinking == 0)
        {
            if (repair->ChatBlinking != 0)
            {
                Application->RemoveTimer(repair, 8);
                repair->ChatBlinking = 0;
            }
        }
        else if (repair->ChatBlinking == 0)
        {
            Application->AddTimer(repair, 8, 0xfa, 0, 0, 0);
            repair->ChatBlinking = 1;
        }
    }

    MCLogObject* previous = CurrentScreen;

    if (previous != nullptr)
    {
        ShowLogScreen(0, 0);
    }

    MoveTicker(Ticker, RepairScreen);
    CurrentScreen = RepairScreen;
    LogisticsState = 4;
    RepairScreen->DrawBackground();
    DrawScreenButtons();

    switch (CurrentInvTab)
    {
        case 0:
            RepairScreen->SetUpMechInv(1, 1);
            break;
        case 1:
            RepairScreen->SetUpPilotInv(1, 1);
            break;
        case 2:
            RepairScreen->SetUpCompInv(1, 1);
            break;
        case 3:
            RepairScreen->SetUpVhclInv(1, 1);
            break;
    }

    MCRepairScreen* repair = RepairScreen;

    if (repair->SelectedMech == nullptr && repair->SelectedVehicle == nullptr)
    {
        if (ForceMechList != nullptr)
        {
            repair->SelectMech(ForceMechList->Mechs);
        }
        else if (ForceVehicleList != nullptr)
        {
            repair->SelectVehicle(ForceVehicleList->Vehicles);
        }
    }

    if (MPlayer != nullptr)
    {
        MoveLights(PlayerLights, repair);
    }

    ShowLogScreen(1, previous == PurchaseScreen || previous == BriefingScreen ? 0 : 1);

    if (animate != 0)
    {
        VfxPaneWipe(WorkPort0->Frame(), 0x10);
        VfxPaneWipe(WorkPort1->Frame(), 0x10);
        MCLogPort* scratch = NewPaneScratch(PurchaseScreen->UnitPane);
        VfxPaneWipe(scratch->Frame(), 0xff);
        DrawPaneForTransition(RepairScreen->UnitPane, scratch, WorkPort0, 1);
        MCLogPort* to = WorkPort0;

        if (previous == PurchaseScreen)
        {
            DrawPaneForTransition(PurchaseScreen->UnitPane, scratch, WorkPort1, 0);
        }
        else
        {
            MCLogPort* look = BriefingScreen->NewLookPicture();
            VfxPaneCopy(look->Frame(), 0xd3, 0x10, WorkPort1->Frame(), 0, 0, -1);
            delete look;
        }

        RepairScreen->UnitPane->ShowGuiWindow(0);
        delete scratch;
        Transition(WorkPort1, to, 0);
        RepairScreen->UnitPane->ShowGuiWindow(1);
    }

    return 0;
}

auto MCLogistics::LoadQuickStart(MCFitIniFile* file) -> void
{
    const int32_t homeTeam = MPlayer->HomeTeam;
    CurDeployTonnage = 0;

    if (file->SeekBlock("HammerDown1") == 0)
    {
        HammerDown = 1;
    }

    char text[0x100];
    std::snprintf(text, sizeof(text), "Side%dUnits", homeTeam != 1 ? 1 : 0);
    int32_t result = file->SeekBlock(text);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not find SideUnit block in quickstart");

    for (int32_t slotIndex = 0; slotIndex < 12; ++slotIndex)
    {
        const int32_t lance = slotIndex / 4;
        const int32_t slot = slotIndex % 4;

        if (LocalDropSlot[slotIndex] == 0)
        {
            continue;
        }

        uint32_t value = 0;
        std::snprintf(text, sizeof(text), "Slot%dUnitData", slotIndex);
        result = file->ReadIdULong(text, value);
        Assert(result == 0, static_cast<uint32_t>(result), "could not read unitData in quickstart");

        if (value == 0)
        {
            continue;
        }

        std::snprintf(text, sizeof(text), "Slot%dType", slotIndex);
        result = file->ReadIdULong(text, value);
        Assert(result == 0, static_cast<uint32_t>(result), "could not read unitType in quickstart");
        std::snprintf(text, sizeof(text), "Slot%dUnitProfile", slotIndex);
        char profile[0x100];

        if (value < 3)
        {
            // A mech: it goes at the head of the force, so every other mech's pilot index moves up one.
            MCLogMechList* mechs = ForceMechList;

            for (MCLogMech* other = mechs->Mechs; other != nullptr; other = other->Next)
            {
                ++other->PilotIndex;
            }

            result = file->ReadIdString(text, profile, 0xfe);
            Assert(result == 0, static_cast<uint32_t>(result), "could not read mech profile string in quickstart");
            MCLogMech* mech = mechs->AddMech(profile, 0, 0, 1);
            mech->Assigned = 1;

            for (int32_t other = 0; other < 12; ++other)
            {
                if (DeploySlots[other / 4][other % 4].Unit >= 0)
                {
                    ++DeploySlots[other / 4][other % 4].Unit;
                }
            }

            std::snprintf(text, sizeof(text), "Slot%dPilotProfile", slotIndex);
            result = file->ReadIdString(text, profile, 0xfe);
            Assert(result == 0, static_cast<uint32_t>(result), "could not read pilot profile string in quickstart");
            MCLogWarriorList* warriors = AssignedWarriorList;
            warriors->AddWarrior(profile, 0);
            MCLogWarrior* warrior = warriors->Warriors;
            warrior->Assigned = 1;
            SetPilot(0, 0);
            const double tonnage = static_cast<double>(CurDeployTonnage) + mech->CurTonnage;

            if (HammerDown != 0 || tonnage <= static_cast<double>(MaxDeployTonnage))
            {
                CurDeployTonnage = static_cast<int32_t>(tonnage);
                SendAddMechMessage(mech, lance, slot);
                warrior->DropLance = lance;
                warrior->DropSlot = slot;
                DeploySlots[lance][slot].Unit = 0;
                mech->Deployed = 1;
                warrior->Deployed = 1;
                auto* block = new MCMechBriefBlock;
                MCBriefingScreen* briefing = BriefingScreen;
                mech->BriefBlock = block;
                block->Init(mech, briefing, briefing->SlotRects[slotIndex].left, briefing->SlotRects[slotIndex].top);
            }
        }
        else
        {
            result = file->ReadIdString(text, profile, 0xfe);
            Assert(result == 0, static_cast<uint32_t>(result), "could not read vehicle profile string in quickstart");
            MCLogVehicle* vehicle = ForceVehicleList->AddVehicle(profile, 0, 0, 1);
            vehicle->Assigned = 1;

            for (int32_t other = 0; other < 12; ++other)
            {
                if (DeploySlots[other / 4][other % 4].Vehicle >= 0)
                {
                    ++DeploySlots[other / 4][other % 4].Vehicle;
                }
            }

            const double tonnage = static_cast<double>(CurDeployTonnage) + vehicle->CurTonnage;

            if (HammerDown != 0 || tonnage <= static_cast<double>(MaxDeployTonnage))
            {
                CurDeployTonnage = static_cast<int32_t>(tonnage);
                SendAddVehicleMessage(vehicle, lance, slot);
                vehicle->Deployed = 1;
                DeploySlots[lance][slot].Vehicle = 0;
                auto* block = new MCMechBriefBlock;
                MCBriefingScreen* briefing = BriefingScreen;
                vehicle->BriefBlock = block;
                block->Init(vehicle, briefing, briefing->SlotRects[slotIndex].left, briefing->SlotRects[slotIndex].top);
            }
        }
    }

    int32_t index = 0;

    for (MCLogMech* mech = ForceMechList->Mechs; mech != nullptr; mech = mech->Next)
    {
        mech->RepairBlock->SlotIndex = index++;
    }

    for (MCLogVehicle* vehicle = ForceVehicleList->Vehicles; vehicle != nullptr; vehicle = vehicle->Next)
    {
        vehicle->RepairBlock->SlotIndex = index++;
    }
}

auto MCLogistics::SaveCampaign(char* fileName) -> int32_t
{
    // The original called the bridge with a stack address as this (it has no fields).
    MCMissionLogisticsBridge bridge;
    return bridge.LogisticsSaveGame(fileName);
}

auto MCLogistics::LoadCampaign(char* campaignFile, char* saveFile, int newCampaign, int loadForce) -> int32_t
{
    // The parameter names follow the original's use: campaignFile is the save's name and saveFile its extension.
    MCPacketFile packetFile;
    MCFitIniFile file;
    int quickStart = 0;
    BriefingScreen->ButtonsLocked = 0;
    char text[0x100];
    std::snprintf(text, sizeof(text), "%slogart\\lsrupm05.tga", ArtPath);
    Application->ActivatePaletteFromTga(text);

    // Start from empty lists and inventories.
    MechList->Destroy();
    ForceMechList->Destroy();
    WarriorList->Destroy();
    AssignedWarriorList->Destroy();
    VehicleList->Destroy();
    ForceVehicleList->Destroy();

    if (ComponentInventory != nullptr)
    {
        ComponentInventory->Destroy();
        delete ComponentInventory;
    }

    ComponentInventory = new MCInventoryList;

    if (PurchaseComponents != nullptr)
    {
        PurchaseComponents->Destroy();
        delete PurchaseComponents;
    }

    PurchaseComponents = new MCInventoryList;

    if (PurMechList != nullptr)
    {
        PurMechList->Destroy();
    }

    if (PurVehicleList != nullptr)
    {
        PurVehicleList->Destroy();
    }

    if (PurPilotList != nullptr)
    {
        PurPilotList->Destroy();
    }

    for (auto& lance : DeploySlots)
    {
        for (DeploySlot& slot : lance)
        {
            slot.Unit = -1;
            slot.Vehicle = -1;
        }
    }

    std::string path;
    path = GamePath(SavePath, campaignFile, saveFile);
    int32_t result = packetFile.Open(path);
    Assert(result == 0, 0, " campaign file NOT Valid! ");
    result = packetFile.SeekPacket(0);
    Assert(result == 0, 0, " could not find initial campaign file ");
    result = file.Open(&packetFile, packetFile.GetPacketSize());
    Assert(result == 0, 0, " could not open initial campaign file ");

    if (newCampaign == 0)
    {
        // The planet picks the master mission file (Solo play names it after the save).
        if (file.SeekBlock("Planet") == 0)
        {
            result = file.ReadIdLong("Setting", CurPlanet);
            Assert(result == 0, static_cast<uint32_t>(result), " could not find Setting in Planet Block ");
        }
        else
        {
            CurPlanet = 0;
        }

        if (Solo == 0)
        {
            std::strcpy(MissionName, CurPlanet == 0 ? "mechcmdr1" : "xmechcmdr1");
        }
        else
        {
            std::snprintf(MissionName, sizeof(MissionName), "campaign%s", campaignFile);
        }

        Mission->InitAgain(MissionName);
    }

    result = file.SeekBlock("General");
    Assert(result == 0, 0, " could not find General Block in campaign file ");

    if (MPlayer == nullptr)
    {
        result = file.ReadIdString("purchaseFile", PurchaseFile, 0x7f);
        Assert(result == 0, 0, " cound not read purchasing file in campain file ");

        if (PlayerLights != nullptr)
        {
            delete PlayerLights;
            PlayerLights = nullptr;
        }
    }
    else
    {
        // Port: the original allocated the FitIniFile (asserting it got the memory).
        MCFitIniFile purchasing;
        char purchaseName[12];
        std::strcpy(purchaseName, "purchase");

        // Original behaviour (OB-100): MainPurchaseFile is read only when the PurchaseInfo block is missing (from the
        // block the file was on); with the block there, "purchase" is used.
        if (file.SeekBlock("PurchaseInfo") != 0)
        {
            file.ReadIdString("MainPurchaseFile", purchaseName, 9);
        }

        std::snprintf(text, sizeof(text), "%s%s.fit", MissionPath, purchaseName);
        result = purchasing.Open(text);
        Assert(result == 0, 0, " could not open purchasing file ");
        result = purchasing.SeekBlock("PilotCosts");
        Assert(result == 0, static_cast<uint32_t>(result), "Could not find PilotCosts block in purchasing file");
        result = purchasing.ReadIdLong("Green", PilotCosts[0]);
        Assert(result == 0, static_cast<uint32_t>(result), "Could not read Green pilot in purchasing file");
        result = purchasing.ReadIdLong("Regular", PilotCosts[1]);
        Assert(result == 0, static_cast<uint32_t>(result), "Could not read Regular pilot in purchasing file");
        result = purchasing.ReadIdLong("Veteran", PilotCosts[2]);
        Assert(result == 0, static_cast<uint32_t>(result), "Could not read Veteran pilot in purchasing file");
        result = purchasing.ReadIdLong("Elite", PilotCosts[3]);
        Assert(result == 0, static_cast<uint32_t>(result), "Could not read Elite pilot in purchasing file.");
    }

    RepairScreen->SelectedMech = nullptr;
    RepairScreen->SelectedVehicle = nullptr;
    int32_t savedMission = 0;

    if (MPlayer == nullptr)
    {
        if (file.ReadIdLong("MissionNumber", savedMission) == 0)
        {
            CurrentMission = savedMission;
        }
        else
        {
            savedMission = -1;
        }

        result = file.SeekBlock("ResourcePoints");
        Assert(result == 0, 0, " could not find Resource Points ");
        uint32_t points = 0;
        result = file.ReadIdULong("numPoints", points);
        Assert(result == 0, 0, " Could not find resource points in campaign file ");
        ResourcePoints = static_cast<int32_t>(points);
    }
    else
    {
        result = file.SeekBlock("Multiplayer");
        Assert(result == 0, 0, "This is not a multiplayer file!");
        result = file.ReadIdString("MissionName", MpMissionName, 0x7f);
        Assert(result == 0, 0, "No mission file in save game file!");

        if (file.ReadIdULong("PlanningTime", PlanningTime) != 0)
        {
            PlanningTime = DefaultPlanningTime;
        }

        // The team's resource points (typed on the session screen) are shared among its players.
        const int32_t teamPlayers = MPlayer->PlayersOnHomeTeam()->Count;
        MCLogTextObject* pointsText = MPlayer->HomeTeam == 0 ? SessionScreen->Team1RPText : SessionScreen->Team2RPText;
        ResourcePoints = std::atoi(pointsText->Buffer) / teamPlayers;

        if (file.SeekBlock("MPQuickStart") == 0)
        {
            Mission->CurrentScenario = -1;
            Mission->CurrentMovie = 0;
            GetCurrentMission();
            LoadQuickStart(&file);
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

        if (file.SeekBlock("Warriors") == 0)
        {
            result = file.ReadIdULong("NumWarriors", count);
            Assert(result == 0, 0, " could not read warrior count ");
            numWarriors = count;

            for (int32_t index = 0; index < static_cast<int32_t>(count); index++)
            {
                std::snprintf(name, sizeof(name), "Warrior%d", index);
                result = file.SeekBlock(name);
                Assert(result == 0, 0, " could not find warrior block ");

                if (file.ReadIdString("Profile", name, 0x4f) == 0)
                {
                    WarriorList->AddWarrior(name, 1);
                }
                else
                {
                    uint32_t packet = 0;
                    result = file.ReadIdULong("PacketNum", packet);
                    Assert(result == 0, 0, " could not find warrior Data ");
                    WarriorList->AddWarrior(&packetFile, static_cast<int32_t>(packet + 1), 1);
                }
            }
        }

        if (file.SeekBlock("AssWarriors") == 0)
        {
            result = file.ReadIdULong("NumAssWarriors", count);
            Assert(result == 0, 0, " could not read Assigned warrior count ");
            const auto first = static_cast<int32_t>(numWarriors);

            for (int32_t index = first; index < first + static_cast<int32_t>(count); index++)
            {
                std::snprintf(name, sizeof(name), "Warrior%d", index);
                result = file.SeekBlock(name);
                Assert(result == 0, 0, " could not find warrior block ");

                if (file.ReadIdString("Profile", name, 0x4f) == 0)
                {
                    // Original behaviour: an assigned pilot given by profile joins the unassigned list.
                    WarriorList->AddWarrior(name, 1);
                }
                else
                {
                    uint32_t packet = 0;
                    result = file.ReadIdULong("PacketNum", packet);
                    Assert(result == 0, 0, " could not find warrior Data ");
                    AssignedWarriorList->AddWarrior(&packetFile, static_cast<int32_t>(packet + 1), 0);
                }
            }
        }

        uint32_t numMechs = 0;

        if (file.SeekBlock("Mechs") == 0)
        {
            result = file.ReadIdULong("NumMechs", count);
            Assert(result == 0, 0, " could not read mech count ");
            numMechs = count;

            for (int32_t index = 0; index < static_cast<int32_t>(count); index++)
            {
                std::snprintf(name, sizeof(name), "Mech%d", index);
                result = file.SeekBlock(name);
                Assert(result == 0, 0, " could not find mech block ");

                if (file.ReadIdString("Profile", name, 0x4f) == 0)
                {
                    int32_t available = 0;

                    if (file.ReadIdLong("NumAvailable", available) != 0)
                    {
                        available = 1;
                    }

                    for (int32_t copy = 0; copy < available; copy++)
                    {
                        MechList->AddMech(name, 0, 1, 1);
                    }
                }
                else
                {
                    uint32_t packet = 0;
                    result = file.ReadIdULong("PacketNum", packet);
                    Assert(result == 0, 0, " could not find Mech Data ");
                    int32_t available = 0;

                    if (file.ReadIdLong("NumAvailable", available) != 0)
                    {
                        available = 1;
                    }

                    for (int32_t copy = 0; copy < available; copy++)
                    {
                        MechList->AddMech(&packetFile, static_cast<int32_t>(packet + 1));
                    }
                }
            }
        }

        if (file.SeekBlock("AssMechs") == 0)
        {
            result = file.ReadIdULong("NumAssMechs", count);
            Assert(result == 0, 0, " could not read assigned mech count ");
            const auto first = static_cast<int32_t>(numMechs);

            for (int32_t index = first; index < first + static_cast<int32_t>(count); index++)
            {
                std::snprintf(name, sizeof(name), "Mech%d", index);
                result = file.SeekBlock(name);
                Assert(result == 0, 0, " could not find mech block ");

                if (file.ReadIdString("Profile", name, 0x4f) == 0)
                {
                    // Original behaviour: an assigned mech given by profile joins the unassigned list.
                    MechList->AddMech(name, 0, 1, 1);
                }
                else
                {
                    uint32_t packet = 0;
                    result = file.ReadIdULong("PacketNum", packet);
                    Assert(result == 0, 0, " could not find Mech Data ");
                    ForceMechList->AddMech(&packetFile, static_cast<int32_t>(packet + 1));
                }
            }
        }

        uint32_t numVehicles = 0;

        if (file.SeekBlock("Vehicles") == 0)
        {
            result = file.ReadIdULong("NumVehicles", count);
            Assert(result == 0, 0, " could not read vehicle count ");
            numVehicles = count;

            for (int32_t index = 0; index < static_cast<int32_t>(count); index++)
            {
                std::snprintf(name, sizeof(name), "Vehicle%d", index);
                result = file.SeekBlock(name);
                Assert(result == 0, 0, " could not find Vehicle block ");

                if (file.ReadIdString("Profile", name, 0x4f) == 0)
                {
                    int32_t available = 0;

                    if (file.ReadIdLong("NumAvailable", available) != 0)
                    {
                        available = 1;
                    }

                    for (int32_t copy = 0; copy < available; copy++)
                    {
                        VehicleList->AddVehicle(name, 0, 0, 1);
                    }
                }
                else
                {
                    uint32_t packet = 0;
                    result = file.ReadIdULong("PacketNum", packet);
                    Assert(result == 0, 0, " could not find vehicle Data ");
                    int32_t available = 0;

                    if (file.ReadIdLong("NumAvailable", available) != 0)
                    {
                        available = 1;
                    }

                    // The packet's [General] Assigned says which list the vehicle goes to.
                    int assigned = 0;
                    MCFitIniFile vehicleFile;
                    result = packetFile.SeekPacket(static_cast<int32_t>(packet + 1));
                    Assert(result == 0, 0, " Vehicle Packet Not Found ");
                    result = vehicleFile.Open(&packetFile, packetFile.GetPacketSize());
                    Assert(result == 0, 0, " Vehicle file could not open ");
                    result = vehicleFile.SeekBlock("General");
                    Assert(result == 0, static_cast<uint32_t>(result), "Failed General Block in Vehicle");

                    if (vehicleFile.ReadIdBoolean("Assigned", assigned) != 0)
                    {
                        assigned = 0;
                    }

                    for (int32_t copy = 0; copy < available; copy++)
                    {
                        result = packetFile.SeekPacket(static_cast<int32_t>(packet + 1));
                        Assert(result == 0, 0, " Vehicle Packet Not Found ");
                        MCLogVehicleList* list = assigned == 0 ? VehicleList : ForceVehicleList;
                        list->AddVehicle(&packetFile, static_cast<int32_t>(packet + 1));
                    }
                }
            }
        }

        if (file.SeekBlock("AssVehicles") == 0)
        {
            result = file.ReadIdULong("NumAssVehicles", count);
            Assert(result == 0, 0, " could not read vehicle count ");
            const auto first = static_cast<int32_t>(numVehicles);

            for (int32_t index = first; index < first + static_cast<int32_t>(count); index++)
            {
                std::snprintf(name, sizeof(name), "Vehicle%d", index);
                result = file.SeekBlock(name);
                Assert(result == 0, 0, " could not find Vehicle block ");

                if (file.ReadIdString("Profile", name, 0x4f) == 0)
                {
                    VehicleList->AddVehicle(name, 0, 0, 1);
                }
                else
                {
                    uint32_t packet = 0;
                    result = file.ReadIdULong("PacketNum", packet);
                    Assert(result == 0, 0, " could not find vehicle Data ");
                    MCLogVehicle* vehicle = ForceVehicleList->AddVehicle(&packetFile, static_cast<int32_t>(packet + 1));

                    if (loadForce != 0 || newCampaign != 0)
                    {
                        vehicle->Deployed = 0;
                    }
                }
            }
        }
    }

    // Every component the game knows (allcomp.fit), with no copies.
    {
        MCFitIniFile allComponents;
        std::string allPath;
        allPath = GamePath(ObjectPath, "allcomp", ".fit");
        result = allComponents.Open(allPath);
        Assert(result == 0, static_cast<uint32_t>(result), " Couldn't find allcomp.fit ");
        result = allComponents.SeekBlock("Components");
        Assert(result == 0, 0, " could not read component block ");
        uint32_t count = 0;
        result = allComponents.ReadIdULong("NumComponents", count);
        Assert(result == 0, 0, " could not read component count ");

        for (int32_t index = 0; index < static_cast<int32_t>(count); index++)
        {
            std::snprintf(text, sizeof(text), "Componant%d", index);
            result = allComponents.SeekBlock(text);
            Assert(result == 0, 0, " could not read component entry in allcomp ");
            uint8_t masterID = 0;
            result = allComponents.ReadIdUChar("ComponantID", masterID);
            Assert(result == 0, 0, " could not read component entry in allcomp ");
            MCLogInventoryStat* stat = ComponentInventory->CreateStat(static_cast<uint8_t>(index), 0, 0, 0, 0xff);
            ComponentInventory->AddItem(masterID, stat, index);
            ComponentInventory->LoadDescription(ComponentInventory->GetIndexFromMasterID(masterID), nullptr);
        }
    }

    // The save's components: the counts of the known ones, and any new ones.
    if (file.SeekBlock("Components") == 0)
    {
        uint32_t count = 0;
        result = file.ReadIdULong("NumComponents", count);
        Assert(result == 0, 0, " could not read component count ");

        for (int32_t index = 0; index < static_cast<int32_t>(count); index++)
        {
            // Original behaviour: a bad component entry ends the load here, returning the error.
            std::snprintf(text, sizeof(text), "Componant%d", index);
            result = file.SeekBlock(text);

            if (result != 0)
            {
                return result;
            }

            uint8_t masterID = 0;
            result = file.ReadIdUChar("ComponantID", masterID);

            if (result != 0)
            {
                return result;
            }

            int32_t available = 0;
            result = file.ReadIdLong("NumAvailable", available);

            if (result != 0)
            {
                return result;
            }

            MCInventoryList* inventory = ComponentInventory;

            if (inventory->GetIndexFromMasterID(masterID) == -1)
            {
                MCLogInventoryStat* stat =
                    inventory->CreateStat(static_cast<uint8_t>(index), 0, 0, static_cast<int16_t>(available), 0xff);
                inventory->AddItem(masterID, stat, index);
                inventory->LoadDescription(inventory->GetIndexFromMasterID(masterID), nullptr);
            }
            else
            {
                inventory->AddCountToItem(available, masterID);
            }
        }
    }

    // Time passes between missions: the pilots heal.
    if (newCampaign == 0 && loadForce == 0)
    {
        AssignedWarriorList->Heal(1);
        WarriorList->Heal(2);
    }

    if (CurrentMission == savedMission || newCampaign != 0 || MPlayer != nullptr)
    {
        Mission->CurrentScenario = CurrentMission;
        Mission->CurrentMovie = CurrentMission + 1;
        GetCurrentMission();
    }
    else
    {
        // Coming back from a mission: apply its results (the "<mission>.pkk" save the mission wrote).
        const char* resultName = CurrentMission - 1 == -1 ? Mission->Scenarios[Mission->CurrentScenario].data()
                                                          : Mission->Scenarios[CurrentMission - 1].data();
        std::string resultPath;
        resultPath = GamePath(SavePath, resultName, ".pkk");
        MCPacketFile resultFile;
        result = resultFile.Open(resultPath);

        if (result != 0)
        {
            return result;
        }

        result = resultFile.SeekPacket(0);
        Assert(result == 0, 0, " could not find mission result file ");
        // Port fix: the original reopened the campaign FitIniFile without closing it first.
        file.Close();
        result = file.Open(&resultFile, resultFile.GetPacketSize());
        Assert(result == 0, 0, " could not open mission result file ");
        result = file.SeekBlock("General");
        Assert(result == 0, 0, " could not find General Block in mission file ");
        result = file.ReadIdString("purchaseFile", PurchaseFile, 0x7f);
        Assert(result == 0, 0, " cound not read purchasing file in campain file ");
        result = file.SeekBlock("ResourcePoints");
        Assert(result == 0, 0, " could not find Resource Points ");
        uint32_t points = 0;
        result = file.ReadIdULong("numPoints", points);
        Assert(result == 0, 0, " Could not find resource points in mission file ");
        ResourcePoints = static_cast<int32_t>(points + static_cast<uint32_t>(ResourcePoints));

        uint32_t count = 0;
        char name[0x50];

        if (file.SeekBlock("Warriors") == 0)
        {
            result = file.ReadIdULong("NumWarriors", count);
            Assert(result == 0, 0, " could not read warrior count ");

            for (int32_t index = 0; index < static_cast<int32_t>(count); index++)
            {
                std::snprintf(name, sizeof(name), "Warrior%d", index);
                result = file.SeekBlock(name);
                Assert(result == 0, 0, " could not find warrior block ");

                if (file.ReadIdString("Profile", name, 0x4f) == 0)
                {
                    Assert(false, 0, " Somehow game write out a profile instead of a packet ! ");
                }
                else
                {
                    uint32_t packet = 0;
                    result = file.ReadIdULong("PacketNum", packet);
                    Assert(result == 0, 0, " could not find warrior Data ");
                    // A pilot not in the list (5) joins it.
                    MCLogWarriorList* pilots = AssignedWarriorList;

                    if (pilots->ReplaceWarrior(&resultFile, static_cast<int32_t>(packet + 1)) == 5)
                    {
                        pilots->AddWarrior(&resultFile, static_cast<int32_t>(packet + 1), 0);
                    }
                }
            }
        }

        if (file.SeekBlock("Mechs") == 0)
        {
            result = file.ReadIdULong("NumMechs", count);
            Assert(result == 0, 0, " could not read mech count ");

            for (int32_t index = 0; index < static_cast<int32_t>(count); index++)
            {
                std::snprintf(name, sizeof(name), "Mech%d", index);
                result = file.SeekBlock(name);
                Assert(result == 0, 0, " could not find mech block ");

                if (file.ReadIdString("Profile", name, 0x4f) == 0)
                {
                    ForceMechList->AddMech(name, 0, 1, 1);
                    continue;
                }

                int assigned = 1;
                uint32_t packet = 0;
                result = file.ReadIdULong("PacketNum", packet);
                Assert(result == 0, 0, " could not find Mech Data ");
                {
                    MCFitIniFile mechFile;
                    result = resultFile.SeekPacket(static_cast<int32_t>(packet + 1));
                    Assert(result == 0, static_cast<uint32_t>(result), "could not find mech packet in save file");
                    result = mechFile.Open(&resultFile, resultFile.GetPacketSize());
                    Assert(result == 0, static_cast<uint32_t>(result), "could not open mech packet in save file");
                    result = mechFile.SeekBlock("General");
                    Assert(result == 0, static_cast<uint32_t>(result),
                           "could not find [General] block in mech packet in save file");
                    result = mechFile.ReadIdBoolean("Assigned", assigned);
                    Assert(result == 0, static_cast<uint32_t>(result),
                           "could not find Assigned variable in [General] block in mech packet in save file");
                }

                // An assigned mech replaces its copy in the force; one not there (5), or an unassigned one, is added.
                MCLogMechList* list = MechList;

                if (assigned != 0)
                {
                    list = ForceMechList;

                    if (list->ReplaceMech(&resultFile, static_cast<int32_t>(packet + 1)) != 5)
                    {
                        continue;
                    }
                }

                list->AddMech(&resultFile, static_cast<int32_t>(packet + 1));
            }
        }

        // Force vehicles that were deployed are gone (the head of the list only).
        MCLogVehicleList* forceVehicles = ForceVehicleList;

        for (MCLogVehicle* vehicle = forceVehicles->Vehicles; vehicle != nullptr && vehicle->Deployed != 0;
             vehicle = forceVehicles->Vehicles)
        {
            forceVehicles->RemoveVehicle(vehicle);
        }

        // Salvaged mechs (not yet the player's) take the first pilot indexes; the others' move up past them.
        MCLogMechList* force = ForceMechList;
        int32_t salvaged = 0;

        for (int32_t index = 0; index < force->NumMechs; index++)
        {
            MCLogMech* mech = nullptr;
            force->GetMechInfo(index, mech);

            if (mech->NotMineYet != 0)
            {
                salvaged++;
            }
        }

        ShiftPilots(0, salvaged);
        force = ForceMechList;
        int32_t nextPilot = 0;

        for (int32_t index = 0; index < force->NumMechs; index++)
        {
            MCLogMech* mech = nullptr;
            force->GetMechInfo(index, mech);

            if (mech->NotMineYet != 0)
            {
                mech->NotMineYet = 0;
                mech->PilotIndex = nextPilot++;
            }
        }

        // Mechs whose pilot ejected (and lives) leave the force with their pilot; the scan restarts after each.
        for (int32_t index = 0; index < force->NumMechs; force = ForceMechList)
        {
            MCLogMech* mech = nullptr;
            force->GetMechInfo(index, mech);
            MCLogWarrior* pilot = nullptr;
            AssignedWarriorList->GetWarriorInfo(mech->PilotIndex, pilot);
            Assert(pilot != nullptr, 0, " Warrior in an assigned mech is NULL ");

            if (pilot->Ejected == 0 || pilot->Health <= 0.0f)
            {
                index++;
                continue;
            }

            force->ExtractMech(index, mech);
            AssignedWarriorList->ExtractWarrior(mech->PilotIndex, pilot);
            ShiftPilots(mech->PilotIndex, -1);
            mech->PilotIndex = -1;
            mech->Deployed = 0;
            mech->Assigned = 0;
            pilot->Ejected = 0;
            pilot->Assigned = 0;
            MechList->AddMech(mech, 1);
            mech->CalcPilotModifier();
            WarriorList->AddWarrior(pilot, 1);
            index = 0;
        }

        // The same for mechs whose pilot died.
        for (int32_t index = 0; index < ForceMechList->NumMechs;)
        {
            MCLogMech* mech = nullptr;
            ForceMechList->GetMechInfo(index, mech);
            MCLogWarrior* pilot = nullptr;
            AssignedWarriorList->GetWarriorInfo(mech->PilotIndex, pilot);
            Assert(pilot != nullptr, 0, " Warrior in an assigned mech is NULL ");

            if (pilot->Health != 0.0f)
            {
                index++;
                continue;
            }

            ForceMechList->ExtractMech(index, mech);
            AssignedWarriorList->ExtractWarrior(mech->PilotIndex, pilot);
            ShiftPilots(mech->PilotIndex, -1);
            mech->PilotIndex = -1;
            mech->Deployed = 0;
            mech->Assigned = 0;
            pilot->Ejected = 0;
            pilot->Assigned = 0;
            MechList->AddMech(mech, 1);
            WarriorList->AddWarrior(pilot, 1);
            mech->CalcPilotModifier();
            index = 0;
        }

        if (file.SeekBlock("Components") == 0)
        {
            result = file.ReadIdULong("NumComponents", count);
            Assert(result == 0, 0, " could not read component count ");

            for (int32_t index = 0; index < static_cast<int32_t>(count); index++)
            {
                std::snprintf(name, sizeof(name), "Componant%d", index);
                result = file.SeekBlock(name);
                Assert(result == 0, static_cast<uint32_t>(result), "Could not find Component Block");
                uint8_t masterID = 0;
                result = file.ReadIdUChar("ComponantID", masterID);
                Assert(result == 0, static_cast<uint32_t>(result), "Could not find Component Master ID");
                int32_t available = 0;
                result = file.ReadIdLong("NumAvailable", available);
                Assert(result == 0, static_cast<uint32_t>(result), "Could not find Component numAvailable");
                ComponentInventory->AddCountToItem(available, masterID);
            }
        }

        if (loadForce == 0)
        {
            AssignedWarriorList->Heal(1);
        }

        resultFile.Close();
    }

    if (MPlayer == nullptr)
    {
        // The purchase options of this point in the campaign, and an automatic save when a new mission starts.
        MCFitIniFile masterFile;
        std::string masterPath;
        masterPath = GamePath(MissionPath, MissionName, ".fit");
        result = masterFile.Open(masterPath);
        Assert(result == 0, 0, " could not open master mission file ");
        result = masterFile.SeekBlock("OpInfo");
        Assert(result == 0, 0, " could not find operation information in master mission file");
        int32_t operationNumber = 0;
        std::snprintf(text, sizeof(text), "Scenario%iOperation", LastLogisticsMissionState);
        result = masterFile.ReadIdLong(text, operationNumber);
        Assert(result == 0, 0, " could not find operation number in master mission file ");
        int32_t missionNumber = 0;
        std::snprintf(text, sizeof(text), "Scenario%iMission", LastLogisticsMissionState);
        result = masterFile.ReadIdLong(text, missionNumber);
        Assert(result == 0, 0, " could not find mission number in master mission file ");
        char* oldPurchaseFile = SetUpCampaignPurchasing(PurchaseFile, &packetFile);

        if (LastLogisticsMissionState < CurrentMission && loadForce == 0)
        {
            SetUpOldPurchasing(oldPurchaseFile);
            char format[200];
            CLoadString(ThisInstance, CurPlanet == 0 ? 0x37a : 0x386, format, 199);
            std::snprintf(text, sizeof(text), format, operationNumber, missionNumber);
            SaveCampaign(text);
        }

        LogFree(oldPurchaseFile);
        // Killed pilots leave the roster and can't be hired again.
        MCLogWarriorList* pilots = WarriorList;

        for (int32_t index = 0; index < pilots->NumWarriors;)
        {
            MCLogWarrior* pilot = nullptr;
            pilots->GetWarriorInfo(index, pilot);

            if (pilot == nullptr || pilot->WarriorStatus != 4)
            {
                index++;
                continue;
            }

            // Original behaviour (OB-098): the pilot is removed by its id used as a list position.
            pilots->RemoveWarrior(static_cast<uint8_t>(pilot->Id));
            PurPilotList->SetPilotStatus(pilot->DescIndex, 2);
            index = 0;
        }

        PurchaseScreen->CreatePurVehiclePane(0);
    }
    else
    {
        SetUpMPPurchasing(PurchaseFile);
        PurchaseScreen->CreatePurVehiclePane(0);
    }

    packetFile.Close();
    MCLogInvScreen* screen = RepairScreen;
    screen->CreateMechInvBlock();
    screen->CreatePilotInvBlock();
    screen->CreateCompInvBlock();
    screen->CreateVhclInvBlock();
    screen->SetUpMechInv(1, 1);
    screen->CreateVehiclePane();
    return 0;
}

auto MCLogistics::PrepareScenario(char* scenarioName, char* startFile) -> int32_t
{
    LastLogisticsMissionState = CurrentMission;

    if (MultiplayerInitialized != 0)
    {
        return PrepareMultiplayerScenario(scenarioName, startFile);
    }

    char text[0x100];
    int32_t result;
    {
        // The automatic "before the mission" save, named after the operation and mission.
        MCFitIniFile masterFile;
        std::string masterPath;
        masterPath = GamePath(MissionPath, MissionName, ".fit");
        result = masterFile.Open(masterPath);
        Assert(result == 0, 0, " could not open master mission file ");
        result = masterFile.SeekBlock("OpInfo");
        Assert(result == 0, 0, " could not find operation information in master mission file");
        int32_t operationNumber = 0;
        std::snprintf(text, sizeof(text), "Scenario%iOperation", LastLogisticsMissionState);
        result = masterFile.ReadIdLong(text, operationNumber);
        Assert(result == 0, 0, " could not find operation number in master mission file ");
        int32_t missionNumber = 0;
        std::snprintf(text, sizeof(text), "Scenario%iMission", LastLogisticsMissionState);
        result = masterFile.ReadIdLong(text, missionNumber);
        Assert(result == 0, 0, " could not find mission number in master mission file ");

        if (Solo == 0)
        {
            char format[200];
            CLoadString(ThisInstance, CurPlanet == 0 ? 0x37b : 0x387, format, 199);
            std::snprintf(text, sizeof(text), format, operationNumber, missionNumber);
            SaveCampaign(text);
        }

        // Write the deployed mechs (with their pilots) and vehicles as mech####/warr#### profiles.
        int32_t profileNumber = 0;

        for (int32_t lance = 0; lance < 3; lance++)
        {
            for (int32_t slot = 0; slot < 4; slot++)
            {
                const int32_t unit = DeploySlots[lance][slot].Unit;
                char profileName[0x20];

                if (unit < 0)
                {
                    const int32_t vehicleIndex = DeploySlots[lance][slot].Vehicle;

                    if (vehicleIndex < 0)
                    {
                        continue;
                    }

                    MCLogVehicle* vehicle = nullptr;
                    ForceVehicleList->GetVehicleInfo(vehicleIndex, vehicle);

                    if (vehicle == nullptr)
                    {
                        continue;
                    }

                    std::snprintf(profileName, sizeof(profileName), "mech%04d", profileNumber);
                    ForceVehicleList->SaveVehicleText(profileName, vehicleIndex);
                    profileNumber++;
                }
                else
                {
                    MCLogMech* mech = nullptr;
                    ForceMechList->GetMechInfo(unit, mech);

                    if (mech == nullptr)
                    {
                        continue;
                    }

                    std::snprintf(profileName, sizeof(profileName), "mech%04d", profileNumber);
                    ForceMechList->SaveMechText(profileName, unit);
                    char warriorName[0x20];
                    std::snprintf(warriorName, sizeof(warriorName), "warr%04d", profileNumber);
                    AssignedWarriorList->SaveWarriorText(warriorName, ForceMechList->GetMechPilotIndex(unit));
                    profileNumber++;
                }
            }
        }
    }

    // Copy the mission's scenario file into the start file, block by block, then add the player's force.
    // Port: the original allocated both FitIniFiles (asserting it got the memory).
    std::string inPath;
    inPath = GamePath(MissionPath, scenarioName, ".fit");
    MCFitIniFile in;
    result = in.Open(inPath);
    Assert(result == 0, static_cast<uint32_t>(result), " could not open logistics scenario file ");
    std::string outPath;
    outPath = GamePath(SaveTempPath, startFile, ".fit");
    MCFitIniFile out;
    result = out.Create(outPath);
    Assert(result == 0, static_cast<uint32_t>(result), " could not open scenario file ");

    // The values being copied; later reads that are not checked write whatever the last read left in them.
    int32_t longValue = 0;
    uint32_t ulongValue = 0;
    float floatValue = 0.0f;
    const auto check = [&](bool ok, const char* message) { Assert(ok, static_cast<uint32_t>(result), message); };
    const auto copyBlock = [&](const char* block, const char* findMessage, const char* writeMessage)
    {
        result = in.SeekBlock(block);
        check(result == 0, findMessage);
        result = out.WriteBlock(block);
        check(result > 0, writeMessage);
    };

    const auto copyLong = [&](const char* name, const char* findMessage, const char* writeMessage)
    {
        result = in.ReadIdLong(name, longValue);
        check(result == 0, findMessage);
        result = out.WriteIdLong(name, longValue);
        check(result > 0, writeMessage);
    };

    const auto copyULong = [&](const char* name, const char* findMessage, const char* writeMessage)
    {
        result = in.ReadIdULong(name, ulongValue);
        check(result == 0, findMessage);
        result = out.WriteIdULong(name, ulongValue);
        check(result > 0, writeMessage);
    };

    const auto copyFloat = [&](const char* name, const char* findMessage, const char* writeMessage)
    {
        result = in.ReadIdFloat(name, floatValue);
        check(result == 0, findMessage);
        result = out.WriteIdFloat(name, floatValue);
        check(result > 0, writeMessage);
    };

    const auto copyString = [&](const char* name, uint32_t maxLength, const char* findMessage, const char* writeMessage)
    {
        result = in.ReadIdString(name, text, maxLength);
        check(result == 0, findMessage);
        result = out.WriteIdString(name, text);
        check(result > 0, writeMessage);
    };

    const auto copyChar = [&](const char* name, const char* findMessage, const char* writeMessage)
    {
        char value = 0;
        result = in.ReadIdChar(name, value);
        check(result == 0, findMessage);
        result = out.WriteIdChar(name, value);
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

        if (in.ReadIdString(name, text, 0xff) != 0)
        {
            break;
        }

        result = out.WriteIdString(name, text);
        check(result > 0, " could not write library string in ABLibraries Block ");
    }

    copyBlock("Smoke Manager", " could not find Smoke Manager Block ", " could not write Smoke Manager Block ");
    int32_t numSmokeTypes = 0;
    result = in.ReadIdLong("NumSmokeTypes", numSmokeTypes);
    check(result == 0, " could not find NumSmokeTypes in Smoke Manager Block ");
    result = out.WriteIdLong("NumSmokeTypes", numSmokeTypes);
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

    if (in.SeekBlock("StatusWindow") == 0)
    {
        result = out.WriteBlock("StatusWindow");
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
    result = in.ReadIdUChar("scenarioTuneNum", tuneNumber);
    check(result == 0, " could not find ScenarioTuneNum in Music block in Scenario File ");
    result = out.WriteIdUChar("scenarioTuneNum", tuneNumber);
    check(result > 0, " could not write ScenarioTuneNum in Music block in Scenario File ");
    copyBlock("Artillery", " could not find Artillery block in Scenario File ",
              " could not write Artillery block in Scenario File ");

    // Original behaviour: the old format (NumStrikes) is not copied at all; the new one is copied without checks
    // after the first count. Port fix: the counts start at 0 (the original wrote leftovers when one was missing).
    if (in.ReadIdULong("NumStrikes", ulongValue) != 0)
    {
        int32_t strikes = 0;
        result = in.ReadIdLong("NumLargeStrikes", strikes);
        Assert(result == 0, 0, " Artillery is in neither of the two known states ");
        out.WriteIdLong("NumLargeStrikes", strikes);
        strikes = 0;
        in.ReadIdLong("NumSmallStrikes", strikes);
        out.WriteIdLong("NumSmallStrikes", strikes);
        strikes = 0;
        in.ReadIdLong("NumSensorStrikes", strikes);
        out.WriteIdLong("NumSensorStrikes", strikes);
        strikes = 0;
        in.ReadIdLong("NumCameraStrikes", strikes);
        out.WriteIdLong("NumCameraStrikes", strikes);
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
    in.ReadIdULong("SingleStep", ulongValue);
    out.WriteIdULong("SingleStep", ulongValue);
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
    in.ReadIdULong("Use90Pixel", ulongValue);
    out.WriteIdULong("Use90Pixel", ulongValue);
    copyBlock("TerrainSystem", " could not find TerrainSystem block ", " could not write TerrainSystem block ");
    copyString("TerrainFileName", 0x4f, " could not find TerrainFileName in TerrainSystem block ",
               " could not write TerrainFileName in TerrainSystem block ");
    // Original behaviour: without a TacMapGifName the terrain file name is written under its name.
    in.ReadIdString("TacMapGifName", text, 0x4f);
    out.WriteIdString("TacMapGifName", text);
    copyBlock("Script", " could not find Script Block ", " could not write Script Block ");
    copyString("ScenarioScript", 0x4f, " could not find ScenarioScript in Script Block ",
               " could not write ScenarioScript in Script Block ");

    // The computer-controlled parts and their pilots are copied renumbered from 1 (the player's parts are left
    // out; the force is added after them).
    result = in.SeekBlock("Warriors");
    Assert(result == 0, 0, " Could not find Warriors Block ");
    uint32_t numWarriors = 0;
    result = in.ReadIdULong("NumWarriors", numWarriors);
    check(result == 0, " Could not find NumWarriors in Warriors Block ");
    // Per warrior number: the part that uses it (first half) and its new number (second half, from numWarriors).
    // Port fix: the buffer is always made and cleared (with no warriors the original used the start file's name).
    std::vector<char> pilotMap(numWarriors * 2 + 2, 0);
    result = in.SeekBlock("Parts");
    check(result == 0, " Could not find Parts Block ");
    uint32_t numParts = 0;
    result = in.ReadIdULong("NumParts", numParts);
    check(result == 0, " Could not find NumParts in Parts Block ");
    char blockName[0x20];

    for (int32_t part = 1; part < static_cast<int32_t>(numParts + 1); part++)
    {
        std::snprintf(blockName, sizeof(blockName), "Part%d", part);
        result = in.SeekBlock(blockName);
        check(result == 0, " Could not find PartNumber Block ");
        int playerPart = 0;

        if (in.ReadIdBoolean("PlayerPart", playerPart) != 0 || playerPart == 0)
        {
            result = in.ReadIdULong("Pilot", ulongValue);
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
        result = in.SeekBlock(blockName);
        Assert(result == 0, warrior, " Could not find Warrior Number Block ");
        std::snprintf(blockName, sizeof(blockName), "Warrior%d", nextWarrior++);
        result = out.WriteBlock(blockName);
        Assert(result > 0, warrior, " Could not find Warrior Number Block ");
        result = in.ReadIdString("Profile", text, 99);
        Assert(result == 0, 0, " Could not find Warrior Profile in Warrior Number Block ");
        result = out.WriteIdString("Profile", text);
        Assert(result > 0, 0, " Could not write Warrior Profile in Warrior Number Block ");
        result = in.ReadIdString("Brain", text, 0x7f);
        check(result == 0, " Could not find Warrior Brain in Warrior Number Block ");
        result = out.WriteIdString("Brain", text);
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
        result = in.SeekBlock(blockName);
        check(result == 0, " Could not find PartNumber Block ");
        int playerPart = 0;

        if (in.ReadIdBoolean("PlayerPart", playerPart) == 0 && playerPart != 0)
        {
            continue;
        }

        partMap[part] = nextPart;
        std::snprintf(blockName, sizeof(blockName), "Part%d", nextPart++);
        result = out.WriteBlock(blockName);
        check(result > 0, " Could not write PartNumber Block ");
        copyULong("ObjectNumber", " Could not find ObjectNumber in PartNumber Block ",
                  " Could not write ObjectNumber in PartNumber Block ");
        copyULong("ControlType", " Could not find ControlType in PartNumber Block ",
                  " Could not write ControlType in PartNumber Block ");
        copyULong("ControlDataType", " Could not find ControlDataType in PartNumber Block ",
                  " Could not write ControlDataType in PartNumber Block ");
        copyString("ObjectProfile", 9, " Could not find ObjectProfile in PartNumber Block ",
                   " Could not write ObjectProfile in PartNumber Block ");
        result = in.ReadIdULong("Pilot", ulongValue);
        check(result == 0, " Could not find Pilot in PartNumber Block ");
        result =
            out.WriteIdULong("Pilot", static_cast<uint32_t>(static_cast<int32_t>(pilotMap[numWarriors + ulongValue])));
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

    if (in.SeekBlock("Elemental Carriers") == 0)
    {
        // Original behaviour: the carrier part numbers are not renumbered.
        result = out.WriteBlock("Elemental Carriers");
        check(result > 0, " Could not write Elemental Carriers Block ");
        int32_t numCarriers = 0;
        result = in.ReadIdLong("Carriers", numCarriers);
        check(result == 0, " Could not read carriers in elemental carriers block");
        result = out.WriteIdLong("Carriers", numCarriers);
        check(result > 0, " Could not write carriers in elemental carriers block");

        for (int32_t carrier = 0; carrier < numCarriers; carrier++)
        {
            std::snprintf(blockName, sizeof(blockName), "ECarrier%d", carrier);
            result = in.SeekBlock(blockName);
            check(result == 0, " Could not find carrier block");
            result = out.WriteBlock(blockName);
            check(result > 0, " Could not write carrier block");
            int32_t carrierPart = 0;
            result = in.ReadIdLong("Carrier", carrierPart);
            check(result == 0, " Could not read carrier in carrier block");
            Assert(carrierPart < firstForcePart, static_cast<uint32_t>(carrierPart),
                   "Illegal part number for elemental carrier");
            result = out.WriteIdLong("Carrier", carrierPart);
            check(result > 0, " Could not write carrier in carrier block");

            for (int32_t elemental = 0; elemental < 10; elemental++)
            {
                std::snprintf(blockName, sizeof(blockName), "Elemental%d", elemental);

                if (in.ReadIdLong(blockName, longValue) != 0)
                {
                    break;
                }

                result = out.WriteIdLong(blockName, longValue);
                check(result > 0, " Could not write elemental in carrier block");
            }
        }
    }

    result = in.SeekBlock("Objectives");
    check(result == 0, " Could not find Objective Block ");
    result = out.WriteBlock("Objectives");
    check(result > 0, " Could not write Objective Block ");

    if (in.ReadIdLong("TimeLeft", longValue) != 0)
    {
        longValue = -1;
    }

    result = out.WriteIdLong("TimeLeft", longValue);
    check(result > 0, " Could not write TimeLeft in Objective Block ");
    uint32_t numObjectives = 0;
    result = in.ReadIdULong("NumObjectives", numObjectives);
    check(result == 0, " Could not find numObjectives in Objective Block ");
    check(numObjectives < 9, " Too Many Objectives ");
    result = out.WriteIdULong("NumObjectives", numObjectives);
    check(result > 0, " Could not write numObjectives in Objective Block ");
    uint32_t numInnerSphereObjectives = 0;

    if (in.ReadIdULong("NumInnerSphereObjectives", numInnerSphereObjectives) != 0)
    {
        numInnerSphereObjectives = 0;
    }

    uint32_t numClanObjectives = 0;

    if (in.ReadIdULong("NumClanObjectives", numClanObjectives) != 0)
    {
        numClanObjectives = 0;
    }

    // Original behaviour: NumObjectives is written a second time.
    result = out.WriteIdULong("NumObjectives", numObjectives);
    check(result > 0, " Could not write numObjectives in Objective Block ");

    if (numInnerSphereObjectives != 0 || numClanObjectives != 0)
    {
        result = out.WriteIdULong("NumInnerSphereObjectives", numInnerSphereObjectives);
        check(result > 0, " Could not write numInnerSphereObjectives in Objective Block ");
        result = out.WriteIdULong("NumClanObjectives", numClanObjectives);
        check(result > 0, " Could not write numClanObjectives in Objective Block ");
    }

    for (uint32_t objective = 0; objective < numObjectives; objective++)
    {
        std::snprintf(blockName, sizeof(blockName), "Objective%d", objective);
        result = in.SeekBlock(blockName);
        Assert(result == 0, objective, " Could not find ObjectiveNumber Block ");
        result = out.WriteBlock(blockName);
        Assert(result > 0, objective, " Could not write ObjectiveNumber Block ");
        copyString("Name", 0xff, " Could not find Name in Objective Block ",
                   " Could not write Name in Objective Block ");
        copyULong("Type", " Could not find Type in Objective Block ", " Could not write Type in Objective Block ");
        copyFloat("TimeLeft", " Could not find TimeLeft in Objective Block ",
                  " Could not write TimeLeft in Objective Block ");
        copyULong("Status", " Could not find Status in Objective Block", " Could not write Status in Objective Block");

        if (in.ReadIdLong("Points", longValue) != 0)
        {
            longValue = 0;
        }

        result = out.WriteIdLong("Points", longValue);
        check(result > 0, " Could not write Points in Objective Block");

        if (in.ReadIdFloat("Radius", floatValue) != 0)
        {
            floatValue = 0.0f;
        }

        result = out.WriteIdFloat("Radius", floatValue);
        check(result > 0, " Could not write Radius in Objective Block");
    }

    result = in.SeekBlock("Teams");
    check(result == 0, " Could not find Teams Block in Scenario ");
    result = out.WriteBlock("Teams");
    check(result > 0, " Could not write Teams Block");
    int alliedTeam = 0;
    result = in.ReadIdBoolean("AlliedTeam", alliedTeam);
    check(result == 0, " Could not find Allied Team flag in Scenario ");
    result = out.WriteIdBoolean("AlliedTeam", alliedTeam);
    check(result > 0, " Could not write AlliedTeam Flag");
    int32_t mates[12];
    // The computer's commanders' groups, with their first five members renumbered.
    const auto copyGroups =
        [&](const char* format, const char* writeMessage, const char* findMatesMessage, const char* writeMatesMessage)
    {
        int32_t group = 0;
        std::snprintf(text, sizeof(text), format, group);

        while (in.SeekBlock(text) == 0)
        {
            result = out.WriteBlock(text);
            check(result > 0, writeMessage);

            for (int32_t& mate : mates)
            {
                mate = 0;
            }

            result = in.ReadIdLongArray("Mates", mates, 12);
            check(result == 0, findMatesMessage);

            for (int32_t mate = 0; mate < 5; mate++)
            {
                if (mates[mate] > 0)
                {
                    mates[mate] = partMap[mates[mate]];
                }
            }

            result = out.WriteIdLongArray("Mates", std::span(mates, 12));
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
    result = in.SeekBlock("Parts");
    check(result == 0, " Could not find Parts Block ");
    result = in.ReadIdULong("NumParts", ulongValue);
    check(result == 0, "Could not read Num Parts ");
    struct PartPlace
    {
        float X = 0;
        float Y = 0;
        float Rotation = 0;
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
        result = in.SeekBlock(text);
        check(result == 0, " Could not locate part block");
        char teamID = 0;
        result = in.ReadIdChar("TeamId", teamID);
        check(result == 0, "Could not read alignment");

        if (teamID != 0)
        {
            continue;
        }

        int32_t active = 0;
        result = in.ReadIdLong("Active", active);
        check(result == 0, " Could not read Active ");
        int capturable = 0;

        if (in.ReadIdBoolean("Capturable", capturable) != 0)
        {
            capturable = 0;
        }

        if (active != 0 && capturable == 0)
        {
            continue;
        }

        ravenParts[numRaven] = part;
        numRaven++;
        Assert(numRaven < 5, static_cast<uint32_t>(numRaven), " Too Many Inactive parts.  Only allowed 4!! ");
        result = in.ReadIdString("ObjectProfile", text, 9);
        check(result == 0, " Could not find ObjectProfile in PartNumber Block ");

        if (std::strstr(text, "v") == nullptr && std::strstr(text, "V") == nullptr)
        {
            // A mech, with its pilot.
            PartPlace& place = ravenMechPlaces[numRavenMechs];
            result = in.ReadIdFloat("PositionX", place.X);
            check(result == 0, " Could not find Raven Part Position ");
            result = in.ReadIdFloat("PositionY", place.Y);
            check(result == 0, " Could not find Raven Part Position ");
            result = in.ReadIdFloat("Rotation", place.Rotation);
            check(result == 0, " Could not find Raven Part Rotation ");
            ForceMechList->AddMech(text, 0, 0, 1);
            MCLogMech* mech = nullptr;
            ForceMechList->GetMechInfo(0, mech);
            mech->Assigned = 1;
            mech->Deployed = 1;
            mech->NotMineYet = 1;
            std::strncpy(mech->ProfileName, text, 9);
            ravenIsMech[numRaven - 1] = 1;
            numRavenMechs++;
            uint32_t pilot = 0;
            result = in.ReadIdULong("Pilot", pilot);
            check(result == 0, " No pilot for this part ");
            char pilotBlock[0x20];
            std::snprintf(pilotBlock, sizeof(pilotBlock), "Warrior%d", pilot);
            result = in.SeekBlock(pilotBlock);
            check(result == 0, " could not find pilot block for Raven System ");
            char pilotProfile[0x32];
            result = in.ReadIdString("Profile", pilotProfile, 0x31);
            check(result == 0, " could not find pilot profile for Raven System ");
            AssignedWarriorList->AddWarrior(pilotProfile, 0);
            MCLogWarrior* warrior = nullptr;
            AssignedWarriorList->GetWarriorInfo(0, warrior);
            warrior->NotMineYet = 1;
        }
        else
        {
            PartPlace& place = ravenVehiclePlaces[numRavenVehicles];
            result = in.ReadIdFloat("PositionX", place.X);
            check(result == 0, " Could not find Raven Part Position ");
            result = in.ReadIdFloat("PositionY", place.Y);
            check(result == 0, " Could not find Raven Part Position ");
            result = in.ReadIdFloat("Rotation", place.Rotation);
            check(result == 0, " Could not find Raven Part Rotation ");
            ForceVehicleList->AddVehicle(text, 0, 0, 1);
            MCLogVehicle* vehicle = nullptr;
            ForceVehicleList->GetVehicleInfo(0, vehicle);
            vehicle->Assigned = 1;
            vehicle->Deployed = 1;
            vehicle->NotMineYet = 1;
            std::strncpy(vehicle->ProfileName, text, 9);
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
            MCLogMech* mech = nullptr;
            ForceMechList->GetMechInfo(entry, mech);
            mech->PilotIndex = pilotIndex--;
        }
    }

    // They go in the third lance when it is empty.
    int32_t ravenLance = -1;
    int32_t lanceSum = 0;

    for (const DeploySlot& slot : DeploySlots[2])
    {
        lanceSum += slot.Unit + slot.Vehicle;
    }

    if (lanceSum == -8)
    {
        ravenLance = 2;
    }

    if (ravenParts[0] != -1)
    {
        Assert(ravenLance != -1, 0xffffffff, " No open lance for Raven System Vehicles/Mechs ");
    }

    int32_t mechIndex = numRaven - numRavenVehicles - 1;
    int32_t vehicleIndex = numRaven - numRavenMechs - 1;

    for (int32_t entry = numRaven - 1; entry >= 0; entry--)
    {
        if (ravenIsMech[entry] == 0)
        {
            DeploySlots[ravenLance][entry].Vehicle = vehicleIndex--;
        }
        else
        {
            DeploySlots[ravenLance][entry].Unit = mechIndex--;
        }
    }

    // The force's pilots (vehicle crews from their profiles). The salvage went to the head of the lists, so the
    // other lances' indexes are past it.
    int32_t warriorNumber = firstForceWarrior;

    for (int32_t lance = 0; lance < 3; lance++)
    {
        for (int32_t slot = 0; slot < 4; slot++)
        {
            const DeploySlot& deploy = DeploySlots[lance][slot];

            if (deploy.Unit < 0)
            {
                if (deploy.Vehicle < 0)
                {
                    continue;
                }

                const int32_t offset = lance != ravenLance ? numRavenVehicles : 0;
                std::snprintf(blockName, sizeof(blockName), "Warrior%d", warriorNumber++);
                result = out.WriteBlock(blockName);
                Assert(result > 0, static_cast<uint32_t>(lance), " Could not write Warrior Number Block ");
                MCFitIniFile crewFile;
                MCLogVehicle* vehicle = nullptr;
                ForceVehicleList->GetVehicleInfo(deploy.Vehicle + offset, vehicle);
                result = out.WriteIdString("Profile", vehicle->Crew);
                Assert(result > 0, 0, " Could not write Warrior Profile in Warrior Number Block ");
                std::snprintf(text, sizeof(text), "%s%s.fit", WarriorPath, vehicle->Crew);
                result = crewFile.Open(text);
                check(result == 0, " Could not open vehicle profile");
                result = crewFile.SeekBlock("General");
                check(result == 0, " Could not find General block in vehicle crew profile");
                result = crewFile.ReadIdString("Brain", text, 0xff);
                check(result == 0, " Could not read brain in vehicle crew profile");
                result = out.WriteIdString("Brain", text);
                check(result > 0, " Could not write Warrior Brain in Warrior Number Block ");
                crewFile.Close();
            }
            else
            {
                const int32_t offset = lance != ravenLance ? numRavenMechs : 0;
                std::snprintf(blockName, sizeof(blockName), "Warrior%d", warriorNumber++);
                result = out.WriteBlock(blockName);
                Assert(result > 0, static_cast<uint32_t>(lance), " Could not write Warrior Number Block ");
                const int32_t mech = deploy.Unit + offset;
                // Original behaviour: the pilot's id is passed where the profile and brain getters take a position.
                int32_t id = AssignedWarriorList->GetID(ForceMechList->GetMechPilotIndex(mech) + offset);
                AssignedWarriorList->GetWarriorProfile(static_cast<uint32_t>(id), text);
                result = out.WriteIdString("Profile", text);
                Assert(result > 0, 0, " Could not write Warrior Profile in Warrior Number Block ");
                id = AssignedWarriorList->GetID(ForceMechList->GetMechPilotIndex(mech) + offset);
                AssignedWarriorList->GetWarriorBrain(static_cast<uint32_t>(id), text);
                result = out.WriteIdString("Brain", text);
                check(result > 0, " Could not write Warrior Brain in Warrior Number Block ");
                MCLogWarrior* warrior = nullptr;
                AssignedWarriorList->GetWarriorInfo(ForceMechList->GetMechPilotIndex(mech), warrior);
                out.WriteIdBoolean("NotMineYet", warrior->NotMineYet);
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
            const DeploySlot& deploy = DeploySlots[lance][slot];

            if (deploy.Unit < 0 && deploy.Vehicle < 0)
            {
                continue;
            }

            std::snprintf(blockName, sizeof(blockName), "Part%d", partNumber);
            result = out.WriteBlock(blockName);
            check(result > 0, " Could not write PartNumber Block ");
            result = out.WriteIdULong("ControlType", 2);
            check(result > 0, " Could not write ControlType in PartNumber Block ");
            MCLogPart* part;

            if (deploy.Unit < 0)
            {
                MCLogVehicle* vehicle = nullptr;
                ForceVehicleList->GetVehicleInfo(deploy.Vehicle + (lance != ravenLance ? numRavenVehicles : 0),
                                                 vehicle);
                Assert(vehicle != nullptr, 0, " Could not get vehicle pointer ");
                result = out.WriteIdULong("ControlDataType", 2);
                check(result > 0, " Could not write ControlDataType in PartNumber Block ");
                part = vehicle;
            }
            else
            {
                MCLogMech* mech = nullptr;
                ForceMechList->GetMechInfo((lance != ravenLance ? numRavenMechs : 0) + deploy.Unit, mech);
                Assert(mech != nullptr, 0, " Could not get mech pointer ");
                result = out.WriteIdULong("ControlDataType", 1);
                check(result > 0, " Could not write ControlDataType in PartNumber Block ");
                part = mech;
            }

            // The part remembers its number (for the team's Mates below).
            part->PartNumber = partNumber++;
            result = out.WriteIdULong("ObjectNumber", part->Chassis);
            check(result > 0, " Could not write ObjectNumber in PartNumber Block ");
            result = out.WriteIdString("ObjectProfile", part->ProfileName);
            check(result > 0, " Could not write ObjectProfile in PartNumber Block ");
            result = out.WriteIdChar("TeamId", 0);
            check(result > 0, " Could not write TeamId in PartNumber Block ");
            result = out.WriteIdChar("CommanderId", 0);
            check(result > 0, " Could not write CommanderId in PartNumber Block ");
            result = out.WriteIdULong("Pilot", static_cast<uint32_t>(pilotNumber + firstForceWarrior));
            check(result > 0, " Could not write Pilot in PartNumber Block ");
            int32_t active;

            if (lance == ravenLance)
            {
                const PartPlace& place =
                    deploy.Unit < 0 ? ravenVehiclePlaces[deploy.Vehicle] : ravenMechPlaces[deploy.Unit];
                result = out.WriteIdFloat("PositionX", place.X);
                check(result > 0, " Could not write PositionX in PartNumber Block ");
                result = out.WriteIdFloat("PositionY", place.Y);
                check(result > 0, " Could not write PositionY in PartNumber Block ");
                result = out.WriteIdFloat("PositionZ", -1.0f);
                check(result > 0, " Could not write PositionZ in PartNumber Block ");
                result = out.WriteIdFloat("Rotation", place.Rotation);
                check(result > 0, " Could not write Rotation in PartNumber Block ");
                result = out.WriteIdULong("Gesture", 2);
                check(result > 0, " Could not write Gesture in PartNumber Block ");
                result = out.WriteIdFloat("Velocity", 0.0f);
                check(result > 0, " Could not write Velocity in PartNumber Block ");
                active = 0;
            }
            else
            {
                const DeploySlotInfo& info = DeploySlotPlacements[lance][slot];
                result = out.WriteIdFloat("PositionX", info.OffsetX + DropZonePositions[lance].X);
                check(result > 0, " Could not write PositionX in PartNumber Block ");
                result = out.WriteIdFloat("PositionY", info.OffsetY + DropZonePositions[lance].Y);
                check(result > 0, " Could not write PositionY in PartNumber Block ");
                result = out.WriteIdFloat("PositionZ", -1.0f);
                check(result > 0, " Could not write PositionZ in PartNumber Block ");
                result = out.WriteIdFloat("Rotation", info.Rotation);
                check(result > 0, " Could not write Rotation in PartNumber Block ");
                result = out.WriteIdULong("Gesture", 2);
                check(result > 0, " Could not write Gesture in PartNumber Block ");
                result = out.WriteIdFloat("Velocity", 0.0f);
                check(result > 0, " Could not write Velocity in PartNumber Block ");
                active = 1;
            }

            result = out.WriteIdLong("Active", active);
            check(result > 0, " Could not write Active Flag in PartNumber Block ");
            result = out.WriteIdLong("Exists", 1);
            check(result > 0, " Could not write Exists Flag in PartNumber Block ");
            result = out.WriteIdChar("MyIcon", 0);
            check(result > 0, " Could not write MyIcon in PartNumber Block ");
            pilotNumber++;
        }
    }

    result = in.SeekBlock("Warriors");
    Assert(result == 0, 0, " Could not find Warriors Block ");
    result = out.WriteBlock("Warriors");
    Assert(result > 0, 0, " Could not write Warriors Block ");
    uint8_t captureChance = 0;
    result = in.ReadIdUChar("CaptureChance", captureChance);
    Assert(result == 0, 0, " Could not read captureChance in Warriors Block ");
    result = out.WriteIdUChar("CaptureChance", captureChance);
    Assert(result > 0, 0, " Could not write captureChance in Warriors Block ");
    result = out.WriteIdULong("NumWarriors", static_cast<uint32_t>(warriorNumber - 1));
    check(result > 0, " Could not write NumWarriors in Warriors Block ");

    if (in.ReadIdString("BrainParameterFile", text, 0xff) == 0)
    {
        result = out.WriteIdString("BrainParameterFile", text);
        check(result > 0, " could not write BrainParameterFile in Warriors Block ");
    }

    result = out.WriteBlock("Parts");
    check(result > 0, " Could not write Parts Block ");
    result = out.WriteIdULong("NumParts", static_cast<uint32_t>(partNumber - 1));
    check(result > 0, " Could not write NumParts in Parts Block ");

    // The player's commander groups: one per lance in use, from the first one.
    const auto lanceUsed = [&](int32_t lance)
    {
        int32_t sum = 0;

        for (const DeploySlot& slot : DeploySlots[lance])
        {
            sum += slot.Unit + slot.Vehicle;
        }

        return sum != -8;
    };

    int32_t firstLance = 0;

    while (firstLance < 3 && !lanceUsed(firstLance))
    {
        firstLance++;
    }

    Assert(firstLance < 3, static_cast<uint32_t>(firstLance), " No Assigned Mechs for this Mission ");
    int32_t groupNumber = 0;

    for (int32_t lance = firstLance; lance < 3; lance++)
    {
        if (!lanceUsed(lance))
        {
            continue;
        }

        std::snprintf(text, sizeof(text), "Commander0Group:%d", groupNumber);
        result = out.WriteBlock(text);
        Assert(result > 0, static_cast<uint32_t>(lance), " could not write Commander0Groupx Team Block ");

        for (int32_t& mate : mates)
        {
            mate = 0;
        }

        int32_t numMates = 0;

        for (int32_t slot = 0; slot < 4; slot++)
        {
            const DeploySlot& deploy = DeploySlots[lance][slot];

            if (deploy.Unit < 0 && deploy.Vehicle < 0)
            {
                continue;
            }

            MCLogPart* part = nullptr;

            if (deploy.Unit < 0)
            {
                MCLogVehicle* vehicle = nullptr;
                ForceVehicleList->GetVehicleInfo((lance != ravenLance ? numRavenVehicles : 0) + deploy.Vehicle,
                                                 vehicle);
                part = vehicle;
            }
            else
            {
                MCLogMech* mech = nullptr;
                ForceMechList->GetMechInfo(deploy.Unit + (lance != ravenLance ? numRavenMechs : 0), mech);
                part = mech;
            }

            if (part != nullptr)
            {
                mates[numMates++] = part->PartNumber;
            }
        }

        result = out.WriteIdLongArray("Mates", std::span(mates, 12));
        check(result > 0, " could not write Mates in Inner Sphere Team Block ");
        groupNumber++;
    }

    if (in.SeekBlock("Trains") == 0)
    {
        int32_t numTrains = 0;
        result = in.ReadIdLong("NumTrains", numTrains);
        check(result == 0, " Could not read numTrains ");
        out.WriteBlock("Trains");
        out.WriteIdLong("NumTrains", numTrains);

        for (int32_t train = 0; train < numTrains; train++)
        {
            std::snprintf(blockName, sizeof(blockName), "Train%d", train);
            result = in.SeekBlock(blockName);
            check(result == 0, " could not find trainBlock ");
            int32_t numCars = 0;
            result = in.ReadIdLong("NumCars", numCars);
            check(result == 0, " could not find numCars ");
            out.WriteBlock(blockName);
            out.WriteIdLong("NumCars", numCars);

            for (int32_t car = 0; car < numCars; car++)
            {
                char carName[0x20];
                std::snprintf(carName, sizeof(carName), "Car%d", car);
                int32_t carPart = 0;
                result = in.ReadIdLong(carName, carPart);
                check(result == 0, " could not find carBlock ");
                out.WriteIdLong(carName, carPart);
            }
        }
    }

    in.Close();
    out.Close();

    // And the logistics state the mission reads back: start<n>.fit for the next mission.
    std::snprintf(text, sizeof(text), "start%d", CurrentMission + 1);
    MCMissionLogisticsBridge bridge;
    result = bridge.LogisticsStartingFitWriter(text, 0);
    Assert(result == 0, 0, " Could not save logistics data ");
    return 0;
}

auto MCLogistics::SetPilot(int32_t mechIndex, int32_t pilotIndex) -> void
{
    if (mechIndex >= ForceMechList->GetMechCount())
    {
        return;
    }

    MCLogMech* mech = nullptr;
    ForceMechList->GetMechInfo(mechIndex, mech);
    MCLogWarrior* warrior = nullptr;

    if (pilotIndex >= 0)
    {
        AssignedWarriorList->GetWarriorInfo(pilotIndex, warrior);
        warrior->InventoryBlock->Mech = mech;
        mech->PilotIndex = pilotIndex;
        mech->RepairBlock->SetPilotStats(nullptr);
        mech->RepairBlock->SetPilotHealth(nullptr);
        mech->CalcPilotModifier();
        return;
    }

    // A negative index takes the pilot off.
    AssignedWarriorList->GetWarriorInfo(mech->PilotIndex, warrior);
    warrior->InventoryBlock->Mech = nullptr;
    mech->RepairBlock->ClearPilot();
    mech->PilotIndex = pilotIndex;
    mech->CalcPilotModifier();
}

auto MCLogistics::ReorderMechs() -> void
{
    // Assigned mechs move from the mech list to the head of the force list.
    MCLogMechList* all = MechList;
    MCLogMech* previous = nullptr;

    for (MCLogMech* mech = all->Mechs; mech != nullptr;)
    {
        MCLogMech* next = mech->Next;

        if (mech->Assigned != 0)
        {
            if (previous == nullptr)
            {
                all->Mechs = next;
            }
            else
            {
                previous->Next = next;
            }

            MCLogMechList* force = ForceMechList;
            mech->Next = force->Mechs;
            force->Mechs = mech;
            all->NumMechs--;
            force->NumMechs++;
        }

        // Original behaviour (OB-097): the moved mech becomes the previous one, so a second assigned mech right
        // after it is unlinked through the force list.
        previous = mech;
        mech = next;
    }

    // Unassigned force mechs go back into the mech list in sortKey order. The scan position and the mech to insert
    // after carry over from one mech to the next.
    MCLogMechList* force = ForceMechList;
    MCLogMech* scan = all->Mechs;
    MCLogMech* insertAfter = nullptr;
    previous = nullptr;

    for (MCLogMech* mech = force->Mechs; mech != nullptr;)
    {
        MCLogMech* next = mech->Next;
        MCLogMech* before = previous;
        previous = mech;

        if (mech->Assigned == 0)
        {
            while (scan != nullptr && scan->SortKey < mech->SortKey)
            {
                insertAfter = scan;
                scan = scan->Next;
            }

            if (insertAfter == nullptr)
            {
                if (before == nullptr)
                {
                    force->Mechs = force->Mechs->Next;
                }
                else
                {
                    before->Next = next;
                }

                mech->Next = all->Mechs;
                all->Mechs = mech;
            }
            else
            {
                if (before == nullptr)
                {
                    force->Mechs = force->Mechs->Next;
                }
                else
                {
                    before->Next = next;
                }

                mech->Next = scan;
                insertAfter->Next = mech;
            }

            all->NumMechs++;
            force->NumMechs--;
        }

        mech = next;
    }

    int32_t index = 0;

    for (MCLogMech* mech = all->Mechs; mech != nullptr; mech = mech->Next)
    {
        mech->InventoryBlock->ListIndex = index++;
    }

    index = 0;

    for (MCLogMech* mech = force->Mechs; mech != nullptr; mech = mech->Next)
    {
        mech->InventoryBlock->ListIndex = index++;
    }
}

auto MCLogistics::ReorderVehicles() -> void
{
    // As reorderMechs, with the vehicle list sorted by tonnage.
    MCLogVehicleList* all = VehicleList;
    MCLogVehicle* previous = nullptr;

    for (MCLogVehicle* vehicle = all->Vehicles; vehicle != nullptr;)
    {
        MCLogVehicle* next = vehicle->Next;

        if (vehicle->Assigned != 0)
        {
            if (previous == nullptr)
            {
                all->Vehicles = next;
            }
            else
            {
                previous->Next = next;
            }

            MCLogVehicleList* force = ForceVehicleList;
            vehicle->Next = force->Vehicles;
            force->Vehicles = vehicle;
            all->NumVehicles--;
            force->NumVehicles++;
        }

        // Original behaviour (OB-097): as in reorderMechs.
        previous = vehicle;
        vehicle = next;
    }

    MCLogVehicleList* force = ForceVehicleList;
    MCLogVehicle* scan = all->Vehicles;
    MCLogVehicle* insertAfter = nullptr;
    previous = nullptr;

    for (MCLogVehicle* vehicle = force->Vehicles; vehicle != nullptr;)
    {
        MCLogVehicle* next = vehicle->Next;
        MCLogVehicle* before = previous;
        previous = vehicle;

        if (vehicle->Assigned == 0)
        {
            for (; scan != nullptr && scan->CurTonnage < vehicle->CurTonnage; scan = scan->Next)
            {
                insertAfter = scan;
            }

            if (insertAfter == nullptr)
            {
                if (before == nullptr)
                {
                    force->Vehicles = force->Vehicles->Next;
                }
                else
                {
                    before->Next = next;
                }

                vehicle->Next = all->Vehicles;
                all->Vehicles = vehicle;
            }
            else
            {
                if (before == nullptr)
                {
                    force->Vehicles = force->Vehicles->Next;
                }
                else
                {
                    before->Next = next;
                }

                vehicle->Next = scan;
                insertAfter->Next = vehicle;
            }

            force->NumVehicles--;
            all->NumVehicles++;
        }

        vehicle = next;
    }

    int32_t index = 0;

    for (MCLogVehicle* vehicle = all->Vehicles; vehicle != nullptr; vehicle = vehicle->Next)
    {
        vehicle->InventoryBlock->ListIndex = index++;
    }

    index = 0;

    for (MCLogVehicle* vehicle = force->Vehicles; vehicle != nullptr; vehicle = vehicle->Next)
    {
        vehicle->InventoryBlock->ListIndex = index++;
    }
}

auto MCLogistics::ReorderWarriors() -> void
{
    // Assigned pilots move into the assigned list in rank order.
    MCLogWarriorList* all = WarriorList;
    MCLogWarriorList* assigned = AssignedWarriorList;
    MCLogWarrior* scan = assigned->Warriors;
    MCLogWarrior* insertAfter = nullptr;
    MCLogWarrior* previous = nullptr;

    for (MCLogWarrior* warrior = all->Warriors; warrior != nullptr;)
    {
        MCLogWarrior* next = warrior->Next;
        MCLogWarrior* before = previous;
        previous = warrior;

        if (warrior->Assigned != 0)
        {
            while (scan != nullptr && scan->Rank < warrior->Rank)
            {
                insertAfter = scan;
                scan = scan->Next;
            }

            if (insertAfter == nullptr)
            {
                if (before == nullptr)
                {
                    all->Warriors = all->Warriors->Next;
                }
                else
                {
                    before->Next = next;
                }

                warrior->Next = assigned->Warriors;
                assigned->Warriors = warrior;
            }
            else
            {
                if (before == nullptr)
                {
                    all->Warriors = all->Warriors->Next;
                }
                else
                {
                    before->Next = next;
                }

                warrior->Next = scan;
                insertAfter->Next = warrior;
            }

            all->NumWarriors--;
            assigned->NumWarriors++;
        }

        // Original behaviour (OB-097): the moved pilot becomes the previous one, as in reorderMechs.
        warrior = next;
    }

    // Unassigned pilots go back into the pilot list by rank, then callsign.
    scan = all->Warriors;
    insertAfter = nullptr;
    previous = nullptr;

    for (MCLogWarrior* warrior = assigned->Warriors; warrior != nullptr;)
    {
        MCLogWarrior* next = warrior->Next;
        MCLogWarrior* before = previous;
        previous = warrior;

        if (warrior->Assigned == 0)
        {
            MCLogWarrior* after = insertAfter;

            while (scan != nullptr && scan->Rank < warrior->Rank)
            {
                insertAfter = scan;
                after = scan;
                scan = scan->Next;
            }

            if (scan != nullptr)
            {
                // Stopped on an equal or higher rank: pass the pilots of the same rank whose callsign sorts first.
                insertAfter = after;

                while (scan != nullptr && std::strcmp(scan->Callsign, warrior->Callsign) < 0 &&
                       scan->Rank == warrior->Rank)
                {
                    insertAfter = scan;
                    scan = scan->Next;
                }
            }

            if (insertAfter == nullptr)
            {
                if (before == nullptr)
                {
                    assigned->Warriors = assigned->Warriors->Next;
                }
                else
                {
                    before->Next = next;
                }

                MCLogWarrior* head = all->Warriors;
                all->Warriors = warrior;
                warrior->Next = head;
            }
            else
            {
                if (before == nullptr)
                {
                    assigned->Warriors = assigned->Warriors->Next;
                }
                else
                {
                    before->Next = next;
                }

                warrior->Next = scan;
                insertAfter->Next = warrior;
            }

            all->NumWarriors++;
            assigned->NumWarriors--;
        }

        warrior = next;
    }

    int32_t index = 0;

    for (MCLogWarrior* warrior = all->Warriors; warrior != nullptr; warrior = warrior->Next)
    {
        warrior->InventoryBlock->ListIndex = index++;
    }

    index = 0;

    for (MCLogWarrior* warrior = assigned->Warriors; warrior != nullptr; warrior = warrior->Next)
    {
        warrior->InventoryBlock->ListIndex = index++;
    }
}

auto MCLogistics::ShiftPilots(int32_t from, int32_t amount) -> void
{
    for (MCLogMech* mech = ForceMechList->Mechs; mech != nullptr; mech = mech->Next)
    {
        if (from <= mech->PilotIndex)
        {
            mech->PilotIndex += amount;
        }
    }
}

auto MCLogistics::RequiredAssigned() -> int
{
    if (MultiplayerInitialized != 0)
    {
        return 1;
    }

    int allThere = 1;

    // Every required mech needs a deployed mech of its chassis in the force, and every required force mech has to
    // be deployed.
    for (MCLogMech* mech = MechList->Mechs; mech != nullptr; mech = mech->Next)
    {
        if (mech->Required == 0)
        {
            continue;
        }

        MCLogMech* found = ForceMechList->Mechs;

        while (found != nullptr && !(mech->Chassis == found->Chassis && found->Deployed != 0))
        {
            found = found->Next;
        }

        if (found == nullptr)
        {
            allThere = 0;
        }
    }

    for (MCLogMech* mech = ForceMechList->Mechs; mech != nullptr; mech = mech->Next)
    {
        if (mech->Required != 0 && mech->Deployed == 0)
        {
            allThere = 0;
        }
    }

    if (allThere == 0)
    {
        return 0;
    }

    // The same for vehicles, except that a force vehicle of the chassis counts whether it is deployed or not.
    for (MCLogVehicle* vehicle = VehicleList->Vehicles; vehicle != nullptr; vehicle = vehicle->Next)
    {
        if (vehicle->Required == 0)
        {
            continue;
        }

        MCLogVehicle* found = ForceVehicleList->Vehicles;

        while (found != nullptr && vehicle->Chassis != found->Chassis)
        {
            found = found->Next;
        }

        if (found == nullptr)
        {
            allThere = 0;
        }
    }

    for (MCLogVehicle* vehicle = ForceVehicleList->Vehicles; vehicle != nullptr; vehicle = vehicle->Next)
    {
        if (vehicle->Required != 0 && vehicle->Deployed == 0)
        {
            allThere = 0;
        }
    }

    return allThere;
}

auto MCLogistics::GetCurrentMission() -> void
{
    // Port: the original allocated the FitIniFile and leaked it when the mission file would not open.
    MCFitIniFile file;
    std::string path;
    char* fileName = MPlayer == nullptr ? Mission->Scenarios[Mission->CurrentScenario].data() : MpMissionName;
    path = GamePath(MissionPath, fileName, ".fit");

    if (file.Open(path) != 0)
    {
        return;
    }

    int32_t result = file.SeekBlock("Campaign");
    Assert(result == 0, 0, " Could not find Campaign block in mission file ");
    result = file.ReadIdLong("MaxTonnage", MaxDeployTonnage);
    Assert(result == 0, 0, " Could not find MaxTonnage variable in mission file ");
    char briefingFile[0x80];

    if (MPlayer == nullptr)
    {
        result = file.ReadIdString("BriefingFile", briefingFile, 0x7f);
    }
    else
    {
        // Each player on the team gets an equal share of the tonnage.
        MaxDeployTonnage /= MPlayer->PlayersOnHomeTeam()->Count;
        char variable[64];
        std::snprintf(variable, sizeof(variable), "%s", MPlayer->HomeTeam == 0 ? "ISBriefingFile" : "ClanBriefingFile");
        result = file.ReadIdString(variable, briefingFile, 0x7f);
    }

    Assert(result == 0, 0, " Could not find BriefingFile variable in mission file ");

    // Format the briefing text into a port the width of the mission pane (at least 0xbf high), then copy it into
    // the briefing screen's mission port with a 2-pixel margin.
    MCBriefingScreen* briefing = BriefingScreen;
    const int32_t paneWidth = briefing->MissionPane->Width();
    char text[0x100];
    std::snprintf(text, sizeof(text), "%s%s", MissionPath, briefingFile);
    int32_t height = Application->TextFormatter.Init(text, nullptr, paneWidth - 0x11);
    auto* textPort = new MCLogPort;

    if (height < 0xbf)
    {
        height = 0xbf;
    }

    textPort->Init(paneWidth - 0x11, height, 1);
    VfxPaneWipe(textPort->Frame(), 0xff);
    Application->TextFormatter.Init(text, textPort, 0);
    delete briefing->MissionPort;
    briefing->MissionPort = new MCLogPort;
    briefing->MissionPort->Init(0xb3, height + 10, 1);
    VfxPaneWipe(briefing->MissionPort->Frame(), 0x10);
    VfxPaneCopy(textPort->Frame(), 0, 0, briefing->MissionPort->Frame(), 2, 2, -1);
    delete textPort;

    result = file.ReadIdString("MapFile", text, 0xff);
    Assert(result == 0, 0, " Could not find MapFile variable in mission file ");

    if (MissionFileName != nullptr)
    {
        LogFree(MissionFileName);
    }

    MissionFileName = LogStrDup(text);

    int32_t numDropZones = 0;
    result = file.ReadIdLong("NumDropZones", numDropZones);
    Assert(result == 0, 0, " Could not read NumDropZones variable in mission file ");

    if (MPlayer == nullptr)
    {
        for (int32_t& slot : LocalDropSlot)
        {
            slot = 0;
        }
    }

    Assert(numDropZones < 7, 0, "Too many drop zones");

    for (int32_t zone = 0; zone < numDropZones; zone++)
    {
        std::snprintf(text, sizeof(text), "DropZone%d", zone);
        file.SeekBlock(text);
        int32_t numSlots = 0;
        result = file.ReadIdLong("NumSlots", numSlots);
        Assert(result == 0, 0, " Could not read NumSlots variable in mission file ");

        // Single player: the zone's slots are the ones the player may fill (a zone is a lance of four).
        // Port fix: the original wrote the marks of a fourth or later zone past localDropSlot, over the drop zone
        // positions already read; the port only marks the three lances.
        if (MPlayer == nullptr && zone < 3)
        {
            for (int32_t slot = 0; slot < numSlots; slot++)
            {
                LocalDropSlot[zone * 4 + slot] = 1;
            }
        }

        result = file.ReadIdFloat("PositionX", DropZonePositions[zone].X);
        Assert(result == 0, 0, " Could not read PositionX variable in mission file ");
        result = file.ReadIdFloat("PositionY", DropZonePositions[zone].Y);
        Assert(result == 0, 0, " Could not read PositionY variable in mission file ");

        for (int32_t slot = 0; slot < 4; slot++)
        {
            if (MPlayer == nullptr && (zone >= 3 || LocalDropSlot[zone * 4 + slot] == 0))
            {
                break;
            }

            DeploySlotInfo& info = DeploySlotPlacements[zone][slot];
            std::snprintf(text, sizeof(text), "OffsetX%d", slot);
            result = file.ReadIdFloat(text, info.OffsetX);
            Assert(result == 0, 0, " Could not read OffsetX block in mission file ");
            std::snprintf(text, sizeof(text), "OffsetY%d", slot);
            result = file.ReadIdFloat(text, info.OffsetY);
            Assert(result == 0, 0, " Could not read OffsetY block in mission file ");
            std::snprintf(text, sizeof(text), "Rotation%d", slot);
            result = file.ReadIdFloat(text, info.Rotation);
            Assert(result == 0, 0, " Could not read Rotation block in mission file ");
        }
    }

    file.Close();
    BriefingScreen->DrawBackground();
}

namespace
{
    /// <summary>
    /// Port: the pane a screen change slides over the screen's right part (<see cref="MCLogistics::Transition"/>). The
    /// original wrote both pictures into its own picture each frame; it draws them from the slide's state instead.
    /// </summary>
    class MCTransitionWipe : public MCLogObject
    {
    public:
        /// <summary>Draws the two pictures as the slide stands (the original's loop body).</summary>
        void Draw() override
        {
            if (!Lport()->ViewOpen())
            {
                return;
            }

            MCPane* target = Lport()->Frame();

            if (Direction == 0)
            {
                From->CopyTo(target, 0, 0, 1);
                VfxPaneCopy(To->Frame(), 0x1ab - Offset, 0, target, 0, 0, -1);
            }
            else
            {
                To->CopyTo(target, 0, 0, 1);
                VfxPaneCopy(From->Frame(), Offset, 0, target, 0, 0, -1);
            }
        }

        /// <summary>The wipe draws itself each frame (its port is a view).</summary>
        bool DrawsLive() override { return true; }

        /// <summary>The screen shown before the change, and the one after.</summary>
        MCLogPort* From = nullptr;
        MCLogPort* To = nullptr;
        /// <summary>0: the new picture slides in from the right; otherwise the old one slides out to the left.</summary>
        int Direction = 0;
        /// <summary>How far the slide has gone, in pixels.</summary>
        int32_t Offset = 0;
    };
}

auto MCLogistics::Transition(MCLogPort* from, MCLogPort* to, int direction) -> void
{
    // A pane over the screen's right part, redrawn each frame for a quarter of a second: direction 0 slides the new
    // picture in from the right over the old one, any other slides the old one out to the left off the new one.
    auto* wipe = new MCTransitionWipe;
    wipe->Init(0xd3, 0x10, from->Width(), from->Height(), nullptr, nullptr);
    wipe->From = from;
    wipe->To = to;
    wipe->Direction = direction;
    CurrentScreen->AddChild(wipe);
    wipe->ShowGuiWindow(1);
    wipe->SetDepth(100);
    SoundSystem()->PlayDigitalSample(0x36, 1, nullptr, 0, 0);
    const int64_t frequency = MCPort::PerformanceFrequency();
    float elapsed = 0.0f;

    do
    {
        const int64_t start = MCPort::PerformanceCounter();
        wipe->Offset = static_cast<int32_t>(static_cast<double>(elapsed) * 4.0 * 427.0);
        UpdateDisplay(0, 0, 0, 0, 0);
        const int64_t end = MCPort::PerformanceCounter();
        // The original divided the low 32 bits of the tick difference by the low 32 bits of the frequency.
        const auto ticks = static_cast<uint32_t>(end - start);
        elapsed = static_cast<float>(static_cast<double>(ticks) / static_cast<int32_t>(frequency) + elapsed);
    } while (elapsed < 0.25);

    wipe->Destroy();
    delete wipe;
}

auto MCLogistics::Darken(int32_t amount, char* fadeTable, MCLogPort* port) -> void
{
    // Darkens row block amount (of the port's height) through the fade table; with no port, the repair screen's
    // unit pane (0x19d x 0x70 blocks).
    int32_t width;
    int32_t height;

    if (port == nullptr)
    {
        RepairScreen->UnitPane->GetDisplayPort(port);
        width = 0x19d;
        height = 0x70;
    }
    else
    {
        height = port->Height();
        width = port->Width();
    }

    DarkenRect(port, 0, height * amount, width, height, fadeTable);
}

void MCLogistics::DarkenRect(MCLogPort* port, int32_t xPos, int32_t yPos, int32_t width, int32_t height,
                             char* fadeTable)
{
    MCScreenVertex corners[4] = {};
    corners[0].X = xPos;
    corners[0].Y = yPos;
    corners[1].X = xPos + width - 1;
    corners[1].Y = yPos;
    corners[2].X = xPos + width - 1;
    corners[2].Y = yPos + height - 1;
    corners[3].X = xPos;
    corners[3].Y = yPos + height - 1;
    VfxTranslatePolygon(port->Frame(), std::span(corners, 4), reinterpret_cast<const uint8_t*>(fadeTable));
}

auto MCLogistics::ReIndexInventory() -> int32_t
{
    // Give each component with copies the next inventory row, in the order of the widgets' inventory indexes;
    // components with none get -1.
    const int32_t numItems = ComponentInventory->NumItems;
    int32_t row = 0;

    if (numItems < 1)
    {
        return 0;
    }

    for (int32_t index = 0; index < numItems; index++)
    {
        MCLogInventoryItem* item = ComponentInventory->Items;

        while (item != nullptr && item->InventoryBlock->InventoryIndex != index)
        {
            item = item->Next;
        }

        Assert(item != nullptr, 0, "Could not reindex player inventory. Probably an old savegame");

        if (item->Count == 0)
        {
            item->InventoryBlock->ListIndex = -1;
        }
        else
        {
            item->InventoryBlock->ListIndex = row++;
        }
    }

    return row;
}

auto MCLogistics::RemoveReorderPilotIndexes(MCLogMech* mech, int32_t removedPilot) -> void
{
    // No this is used (the original is a plain function at this address).
    for (; mech != nullptr; mech = mech->Next)
    {
        if (removedPilot < mech->PilotIndex)
        {
            mech->PilotIndex--;
        }
    }
}

namespace
{
    /// <summary>
    /// The text an old-iostream <c>ofstream</c> would have written: strings as they are, integers in decimal,
    /// doubles as <c>%.6g</c> (the default precision), and every line end as CR LF (text mode). The original wrote
    /// the multiplayer start file and nomechlist.log this way; the port collects the text and writes it through
    /// <see cref="MCFile"/>.
    /// </summary>
    class MCTextStream
    {
    public:
        MCTextStream& operator<<(const char* text)
        {
            _Text += text;
            return *this;
        }

        MCTextStream& operator<<(char character)
        {
            _Text += character;
            return *this;
        }

        MCTextStream& operator<<(int value)
        {
            _Text += std::to_string(value);
            return *this;
        }

        MCTextStream& operator<<(unsigned long value)
        {
            _Text += std::to_string(value);
            return *this;
        }

        MCTextStream& operator<<(double value)
        {
            char text[64];
            std::snprintf(text, sizeof(text), "%.6g", value);
            _Text += text;
            return *this;
        }

        /// <summary>Writes the text to <paramref name="fileName"/>, silently doing nothing when it can't be created.</summary>
        void WriteFile(const char* fileName) const
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

            MCFile file;

            if (file.Create(fileName) != 0)
            {
                return;
            }

            file.Write(reinterpret_cast<const uint8_t*>(text.data()), static_cast<int32_t>(text.size()));
            file.Close();
        }

    private:
        std::string _Text;
    };

#pragma pack(push, 1)
    /// <summary>
    /// A "deploy force" message (MPMSG_DEPLOY_FORCE): a mech or vehicle placed in a drop slot, with its pilot and
    /// components. Sent as <c>numItems * 2 + 0xd</c> bytes. The struct name is the port's.
    /// </summary>
    struct MCDeployForceMessage : public MCFIGuaranteedMessageHeader
    {
        /// <summary>
        /// Bit 0 a mech (else a vehicle), bit 1 the Clan side, bits 2-3 the mech's name variant, bits 4-5 the lance,
        /// bits 6-7 the slot.
        /// </summary>
        uint8_t Flags = 0;
        /// <summary>The part's name index (the variant is added from the flags for mechs).</summary>
        uint8_t NameIndex = 0;
        /// <summary>The pilot's name index (0xff for a vehicle).</summary>
        uint8_t PilotNameIndex = 0;
        uint8_t Padding = 0; // Fixed layout: deploy force message (never read; MCX.EXE sent 0xff)
        uint8_t NumItems = 0;
        /// <summary>Per component copy, its master id as a 16-bit value (low byte first).</summary>
        uint8_t Items[1]{};
    };

    static_assert(sizeof(MCDeployForceMessage) == 0xe);

    /// <summary>A "remove force" message (MPMSG_REMOVE_FORCE): a drop slot emptied. 10 bytes; the name is the port's.</summary>
    struct MCRemoveForceMessage : public MCFIGuaranteedMessageHeader
    {
        uint8_t Slot = 0;
        uint8_t Lance = 0;
    };

    static_assert(sizeof(MCRemoveForceMessage) == 10);
#pragma pack(pop)

    /// <summary>
    /// Item <paramref name="index"/> of a net name list (<c>netmechs.rsp</c>, ...), or null past the end, as the
    /// original walked the links.
    /// </summary>
    char* NetListItem(MCFLinkedList<char>& list, uint32_t index)
    {
        MCFLink<char>* link = list.HeadLink;

        if (link == nullptr)
        {
            return nullptr;
        }

        for (; index != 0; index--)
        {
            if (link->Next == nullptr)
            {
                return nullptr;
            }

            link = link->Next;
        }

        return link->Data;
    }

    /// <summary>Whether the local player's team is the given side's group (the flags' Clan bit picks the side).</summary>
    uint32_t SideGroupID(uint8_t flags)
    {
        return (flags & 2) == 0 ? MPlayer->InnerSphereGroupID : MPlayer->ClanGroupID;
    }

    /// <summary>Appends a copy of every component of <paramref name="inventory"/> to a deploy message.</summary>
    /// <returns>The number of copies written.</returns>
    uint32_t WriteDeployItems(MCDeployForceMessage* message, MCInventoryList* inventory)
    {
        uint32_t count = 0;

        for (MCLogInventoryItem* item = inventory->Items; item != nullptr; item = item->Next)
        {
            for (MCLogInventoryStat* stat = item->Stats; stat != nullptr; stat = stat->Next)
            {
                message->Items[count * 2] = item->MasterID;
                message->Items[count * 2 + 1] = 0;
                count++;
            }
        }

        return count;
    }

    /// <summary>Replaces the inventory of a part made from a deploy message with the message's components.</summary>
    void ReadDeployItems(MCLogPart* part, const MCDeployForceMessage* message)
    {
        if (part->Inventory != nullptr)
        {
            part->Inventory->Destroy();
            delete part->Inventory;
        }

        auto* inventory = new MCInventoryList;
        part->Inventory = inventory;
        const uint32_t numItems = message->NumItems;

        for (uint32_t index = 0; index < numItems; index++)
        {
            MCLogInventoryStat* stat = inventory->CreateStat(static_cast<uint8_t>(index), 0, 0, 1, 0xff);
            inventory->AddItem(message->Items[index * 2], stat, -1);
        }
    }
}

auto MCLogistics::HandleDeployForceMessage(uint32_t playerID, const void* message) -> void
{
    const auto* deploy = static_cast<const MCDeployForceMessage*>(message);
    int teammate = 0;

    if (MultiplayerInitialized == 0)
    {
        InitializeMultiplayer();
    }

    Assert(playerID != MPlayer->SessionManager->MyPlayer->Id, 0, "Got a deploy message from ourselves!");
    // The sender is a teammate when the message's side is ours.
    const uint32_t homeGroup = MPlayer->HomeTeamGroupID;

    if ((deploy->Flags & 2) == 0)
    {
        if (homeGroup == MPlayer->InnerSphereGroupID)
        {
            teammate = 1;
        }
    }
    else if (homeGroup == MPlayer->ClanGroupID)
    {
        teammate = 1;
    }

    Assert(homeGroup == MPlayer->InnerSphereGroupID || homeGroup == MPlayer->ClanGroupID, 0,
           "Local player is not on a team!");

    uint32_t lance = (deploy->Flags >> 4) & 3;
    uint32_t slot = deploy->Flags >> 6;
    MCLogPart* part;

    if ((deploy->Flags & 1) == 0)
    {
        MCLogVehicleList* list = FindMPVehicleList(playerID, teammate);
        part = AddVehicleFromNetworkMessage(list, reinterpret_cast<MCFIMessageHeader*>(const_cast<void*>(message)));
        part->LocalPart = 0;

        if (teammate != 0)
        {
            auto* vehicle = static_cast<MCLogVehicle*>(part);
            auto* block = new MCMechBriefBlock;
            MCBriefingScreen* briefing = BriefingScreen;
            vehicle->BriefBlock = block;
            const RECT& rect = briefing->SlotRects[lance * 4 + slot];
            block->Init(vehicle, briefing, rect.left, rect.top);
        }
    }
    else
    {
        MCLogMechList* list = FindMPMechList(playerID, teammate, nullptr);
        part = AddMechFromNetworkMessage(list, reinterpret_cast<MCFIMessageHeader*>(const_cast<void*>(message)));
        part->LocalPart = 0;

        if (teammate != 0)
        {
            auto* mech = static_cast<MCLogMech*>(part);
            mech->CalcStatus();
            auto* block = new MCMechBriefBlock;
            MCBriefingScreen* briefing = BriefingScreen;
            mech->BriefBlock = block;
            const RECT& rect = briefing->SlotRects[lance * 4 + slot];
            block->Init(mech, briefing, rect.left, rect.top);
        }
    }

    const int32_t commander = MPlayer->SessionManager->GetPlayer(playerID)->PlayerNumber;
    part->CommanderID = commander;
    Assert(commander != MPlayer->CheckInId, 0, "Wrong commander!");
    lance = (deploy->Flags >> 4) & 3;
    slot = deploy->Flags >> 6;
    part->DropLance = lance;
    part->DropSlot = slot;

    if (teammate == 0)
    {
        OpponentDropSlots[lance][slot]->Part = part;
        return;
    }

    DropSlots[lance][slot]->Part = part;
    BriefingScreen->MpCalcTonnages();
}

auto MCLogistics::HandleRemoveForceMessage(uint32_t playerID, const void* message) -> void
{
    const auto* remove = static_cast<const MCRemoveForceMessage*>(message);
    const int teammate = MPlayer->IsMyTeammate(playerID);
    RemoveForceAtDropSlot(remove->Slot + remove->Lance * 4, playerID, teammate);
}

auto MCLogistics::HandleChatMessage(uint32_t playerID, const void* message) -> void
{
    // Blink the chat button of the screen being shown (unless the briefing is on its operation tab, where the chat
    // is open), and play the chat sound.
    MCLogObject* shown = CurrentScreen;
    MCBriefingScreen* briefing = BriefingScreen;

    if (shown != briefing || briefing->CurrentTab != 1)
    {
        if (shown == briefing && briefing->ChatTimerOn == 0)
        {
            Application->AddTimer(briefing, 5, 0xfa, 0, 0, 0);
            briefing->ChatTimerOn = 1;
        }
        else if (shown == PurchaseScreen && PurchaseScreen->ChatBlinking == 0)
        {
            Application->AddTimer(PurchaseScreen, 7, 0xfa, 0, 0, 0);
            PurchaseScreen->ChatBlinking = 1;
        }
        else if (shown == RepairScreen && RepairScreen->ChatBlinking == 0)
        {
            Application->AddTimer(RepairScreen, 8, 0xfa, 0, 0, 0);
            RepairScreen->ChatBlinking = 1;
        }

        briefing->ChatBlinking = 1;
        SoundSystem()->PlayDigitalSample(0x14, 1, nullptr, 0, 0);
    }

    ChatWindow->HandleNetworkMessage(playerID, const_cast<void*>(message));
}

auto MCLogistics::SendRemoveForceMessage(int lance, int slot) -> void
{
    if (MultiplayerInitialized == 0 || MPlayer == nullptr)
    {
        return;
    }

    auto* message = reinterpret_cast<MCRemoveForceMessage*>(MessageBuffer);
    std::memset(message, 0, 8);
    message->Lance = static_cast<uint8_t>(lance);
    message->Slot = static_cast<uint8_t>(slot);
    message->Header = FIMSG_GUARANTEED | MPMSG_REMOVE_FORCE;
    MPlayer->SessionManager->SendMessageToGroup(0, message, sizeof(MCRemoveForceMessage));
}

auto MCLogistics::SendAddMechMessage(MCLogMech* mech, int lance, int slot) -> void
{
    if (MultiplayerInitialized == 0 || MPlayer == nullptr)
    {
        return;
    }

    auto* message = reinterpret_cast<MCDeployForceMessage*>(MessageBuffer);
    message->Header = 0;
    message->Tagger.Clear();
    message->Header = FIMSG_GUARANTEED | MPMSG_DEPLOY_FORCE;
    message->NameIndex = 0;
    message->PilotNameIndex = 0xff;
    message->Padding = 0; // Fixed layout: deploy force message
    message->NumItems = 0;
    message->Flags = 1;
    const uint32_t homeGroup = MPlayer->HomeTeamGroupID;
    Assert(homeGroup == MPlayer->InnerSphereGroupID || homeGroup == MPlayer->ClanGroupID, 0,
           "Local player is not on a team!");
    message->Flags = (MPlayer->HomeTeamGroupID != MPlayer->InnerSphereGroupID ? 2 : 0) + 1;
    MCLogWarrior* pilot = nullptr;
    AssignedWarriorList->GetWarriorInfo(mech->PilotIndex, pilot);

    if (mech->NameVariant < 4)
    {
        message->Flags = static_cast<uint8_t>((message->Flags & 0xf3) | (mech->NameVariant << 2));
    }

    if (lance < 4)
    {
        message->Flags = static_cast<uint8_t>((message->Flags & 0xcf) | (lance << 4));
    }

    if (slot < 4)
    {
        message->Flags = static_cast<uint8_t>((message->Flags & 0x3f) | (slot << 6));
    }

    message->NameIndex = static_cast<uint8_t>(mech->NameIndex);
    message->NumItems = mech->Inventory->NextStatID;
    message->PilotNameIndex = static_cast<uint8_t>(pilot->NameIndex);
    const uint32_t count = WriteDeployItems(message, mech->Inventory);
    message->NumItems = static_cast<uint8_t>(count);
    MPlayer->SessionManager->SendMessageToGroup(0, message, (count & 0xff) * 2 + 0xd);
}

auto MCLogistics::SendAddVehicleMessage(MCLogVehicle* vehicle, int lance, int slot) -> void
{
    if (MultiplayerInitialized == 0 || MPlayer == nullptr)
    {
        return;
    }

    auto* message = reinterpret_cast<MCDeployForceMessage*>(MessageBuffer);
    message->Header = 0;
    message->Tagger.Clear();
    message->NameIndex = 0;
    message->NumItems = 0;
    message->Flags = 0;
    message->Header = FIMSG_GUARANTEED | MPMSG_DEPLOY_FORCE;
    const uint8_t side = MPlayer->HomeTeamGroupID != MPlayer->InnerSphereGroupID ? 2 : 0;
    message->PilotNameIndex = 0xff;
    message->Padding = 0; // Fixed layout: deploy force message
    message->Flags = side;

    if (lance < 4)
    {
        message->Flags = static_cast<uint8_t>(side | (lance << 4));
    }

    if (slot < 4)
    {
        message->Flags = static_cast<uint8_t>((message->Flags & 0x3f) | (slot << 6));
    }

    message->NameIndex = static_cast<uint8_t>(vehicle->NameIndex);
    message->NumItems = vehicle->Inventory->NextStatID;
    const uint32_t count = WriteDeployItems(message, vehicle->Inventory);
    message->NumItems = static_cast<uint8_t>(count);
    MPlayer->SessionManager->SendMessageToGroup(0, message, (count & 0xff) * 2 + 0xd);
}

auto MCLogistics::HandleLostPlayer(uint32_t playerID, int) -> void
{
    // "<player> has left the game" (or, in a lobby game, the variant that ends it).
    char text[256];
    CLoadString(ThisInstance, LaunchedFromLobby == 0 || MPlayer == nullptr ? 0x35f : 0x365, text, 0xfe);
    std::snprintf(HoldString, sizeof(HoldString), "%s %s", MPlayer->SessionManager->GetPlayer(playerID)->Name, text);
    MCReusableDialog* dialog = MessageDialog;

    // A dialog already up whose button exits keeps showing; the message follows once it is answered.
    if (dialog->IsShowing() != 0 && dialog->OkButton->Callback()->Exec == DoExit)
    {
        dialog->Callback = LostPlayerHandler;
        return;
    }

    LostPlayerHandler(0);
}

auto MCLogistics::HandlePrepareScenarioMessage() -> void
{
    SoundSystem()->PlayDigitalSample(0x3a, 1, nullptr, 0, 0);
    Mission->StartScenario(MpMissionName);
}

auto MCLogistics::PrepareMultiplayerScenario(char* scenarioName, char* startFile) -> int32_t
{
    // The original wrote the start file through an ofstream; the port builds the same text (see TextStream).
    MCTextStream out;
    const int32_t homePlayers = MPlayer->PlayersOnHomeTeam()->Count;
    // The Inner Sphere side's drop slots come first; team 0 is the Inner Sphere, 1 the Clans.
    MCDropSlot** isSlots;
    MCDropSlot** clanSlots;
    int32_t ownTeam;
    int32_t otherTeam;
    char side[8];
    int32_t isPlayers;
    int32_t clanPlayers;

    if (MPlayer->HomeTeamGroupID == MPlayer->ClanGroupID)
    {
        clanSlots = &DropSlots[0][0];
        isSlots = &OpponentDropSlots[0][0];
        ownTeam = 1;
        otherTeam = 0;
        std::snprintf(side, sizeof(side), "Clan");
        clanPlayers = homePlayers;
        isPlayers = MPlayer->NumPlayers() - homePlayers;
    }
    else
    {
        isSlots = &DropSlots[0][0];
        clanSlots = &OpponentDropSlots[0][0];
        ownTeam = 0;
        otherTeam = 1;
        std::snprintf(side, sizeof(side), "IS");
        isPlayers = homePlayers;
        clanPlayers = MPlayer->NumPlayers() - homePlayers;
    }

    char outName[0x200];
    std::snprintf(outName, sizeof(outName), "%s%s.fit", SaveTempPath, startFile);
    char inName[0x200];
    std::snprintf(inName, sizeof(inName), "%s%s.fit", MissionPath, scenarioName);

    // The mission file up to its [Campaign] block goes over as it is.
    char line[0x200];
    {
        MCFile missionFile;
        int32_t result = missionFile.Open(inName);
        Assert(result == 0, static_cast<uint32_t>(result), "Could not open input mission file");

        while (missionFile.Eof() == 0)
        {
            missionFile.ReadLine(reinterpret_cast<uint8_t*>(line), 0x1ff);

            if (std::strstr(line, "[Campaign]") != nullptr || std::strstr(line, "FITend") != nullptr)
            {
                break;
            }

            out << line << '\n';
        }
    }

    // Port: the original allocated the FitIniFile.
    MCFitIniFile file;
    int32_t result = file.Open(inName);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not open input mission file");
    result = file.SeekBlock("Campaign");
    Assert(result == 0, static_cast<uint32_t>(result), "Could not find campaign block");
    out << "[Campaign]" << '\n';
    result = file.ReadIdString("MapFile", line, 0x1ff);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not find map file");
    out << "st MapFile = \"" << line << "\"" << '\n';
    int32_t value = 0;
    result = file.ReadIdLong("MaxTonnage", value);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not find max tonnage");
    // Each player on the local team gets an equal share.
    value /= homePlayers;
    out << "l MaxTonnage = " << value << '\n';
    result = file.ReadIdLong("NumDropZones", value);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not find numdropzones");
    out << "l NumDropZones = " << value << '\n';
    char variable[0x100];
    std::snprintf(variable, sizeof(variable), "%sBriefingFile", side);
    result = file.ReadIdString(variable, line, 0x1ff);
    Assert(result == 0, static_cast<uint32_t>(result), "Could not find briefing file");
    out << "st BriefingFile = \"" << line << "\"" << '\n' << '\n';

    // Each side's artillery, shared out among its players ([0] Inner Sphere, [1] Clans).
    int32_t largeStrikes[2];
    int32_t smallStrikes[2];
    int32_t sensorStrikes[2];
    int32_t cameraStrikes[2];
    result = file.SeekBlock("ISArtillery");
    Assert(result == 0, 0, "No [ISArtillery] section in mission file");
    file.ReadIdLong("NumLargeStrikes", value);
    largeStrikes[0] = isPlayers != 0 ? value / isPlayers : 0;
    file.ReadIdLong("NumSmallStrikes", value);
    smallStrikes[0] = isPlayers != 0 ? value / isPlayers : 0;
    file.ReadIdLong("NumSensorStrikes", value);
    sensorStrikes[0] = isPlayers != 0 ? value / isPlayers : 0;
    file.ReadIdLong("NumCameraStrikes", value);
    cameraStrikes[0] = isPlayers != 0 ? value / isPlayers : 0;
    result = file.SeekBlock("ClanArtillery");
    Assert(result == 0, 0, "No [Clan Artillery] section in mission file");
    result = file.ReadIdLong("NumLargeStrikes", value);
    Assert(result == 0, 0, "No Clan NumLargeStrikes section in mission file");
    largeStrikes[1] = clanPlayers != 0 ? value / clanPlayers : 0;
    result = file.ReadIdLong("NumSmallStrikes", value);
    Assert(result == 0, 0, "No Clan NumSmallStrikes section in mission file");
    smallStrikes[1] = clanPlayers != 0 ? value / clanPlayers : 0;
    result = file.ReadIdLong("NumSensorStrikes", value);
    Assert(result == 0, 0, "No Clan NumSensorStrikes section in mission file");
    sensorStrikes[1] = clanPlayers != 0 ? value / clanPlayers : 0;
    result = file.ReadIdLong("NumCameraStrikes", value);
    Assert(result == 0, 0, "No Clan NumCameraStrikes section in mission file");
    cameraStrikes[1] = clanPlayers != 0 ? value / clanPlayers : 0;

    // A commander block per player with its side's share.
    MCFLinkedList<MCFidpPlayer>* players = MPlayer->SessionManager->GetPlayers(nullptr);

    for (MCFLink<MCFidpPlayer>* link = players->HeadLink; link != nullptr && link->Data != nullptr; link = link->Next)
    {
        const int32_t sideIndex = link->Data->IsInGroup(MPlayer->InnerSphereGroupID) != 0 ? 0 : 1;
        out << "[Commander:" << static_cast<int>(link->Data->PlayerNumber) << "]" << '\n';
        out << "l NumSmallStrikes\t\t= " << smallStrikes[sideIndex] << '\n';
        out << "l NumLargeStrikes\t\t= " << largeStrikes[sideIndex] << '\n';
        out << "l NumSensorStrikes\t\t= " << sensorStrikes[sideIndex] << '\n';
        out << "l NumCameraDrones\t\t= " << cameraStrikes[sideIndex] << '\n' << '\n';
    }

    // The pilots: the local player's, then the other players' (numbered on).
    MCLogWarriorList* networkPilots = MpWarriorList;
    const int32_t numAssigned = AssignedWarriorList->NumWarriors;
    int32_t numWarriors = networkPilots->NumWarriors + numAssigned;
    int32_t warriorNumber = 1;

    for (MCLogWarrior* warrior = AssignedWarriorList->Warriors; warrior != nullptr; warrior = warrior->Next)
    {
        out << "[Warrior" << warriorNumber << "]" << '\n';
        out << "st Profile = \"" << warrior->FileName << "\"" << '\n';
        out << "st Brain = \"pbrain\"\n\n";
        warriorNumber++;
    }

    for (MCLogWarrior* warrior = networkPilots->Warriors; warrior != nullptr; warrior = warrior->Next)
    {
        out << "[Warrior" << warriorNumber << "]" << '\n';
        out << "st Profile = \"" << warrior->FileName << "\"\n";
        out << "st Brain = \"pbrain\"\n\n";
        warriorNumber++;
    }

    // The local player's deployed units go into their drop slots.
    for (int32_t index = 0; index < 12; index++)
    {
        const DeploySlot& deploy = DeploySlots[index / 4][index % 4];
        MCLogPart* part;

        if (deploy.Unit >= 0)
        {
            MCLogMech* mech = nullptr;
            ForceMechList->GetMechInfo(deploy.Unit, mech);
            MCDropSlot* slot = (&DropSlots[0][0])[index];
            Assert(slot->Part == nullptr, 0, "local/remote mech conflict");
            slot->Part = mech;
            part = mech;
        }
        else if (deploy.Vehicle >= 0)
        {
            MCLogVehicle* vehicle = nullptr;
            ForceVehicleList->GetVehicleInfo(deploy.Vehicle, vehicle);
            MCDropSlot* slot = (&DropSlots[0][0])[index];
            Assert(slot->Part == nullptr, 0, "local/remote vehicle conflict");
            slot->Part = vehicle;
            part = vehicle;
        }
        else
        {
            continue;
        }

        part->CommanderID = MPlayer->CheckInId;
    }

    // Every drop slot's unit as a part: a profile of its own ("part<n>") and its place in the side's drop zones.
    const int32_t controlType = MPlayer->IsServer != 0 ? 2 : 3;
    const uint32_t numHome = static_cast<uint32_t>(MPlayer->PlayersOnHomeTeam()->Count);
    const uint32_t numEnemy = static_cast<uint32_t>(MPlayer->PlayersOnEnemyTeam()->Count);
    Assert(numEnemy != 0, numEnemy, " No Enemy Team ");
    Assert(numHome != 0, numHome, " No Home Team ");
    const int32_t homeSlotsPerPlayer = 12 / static_cast<int32_t>(numHome);
    const int32_t enemySlotsPerPlayer = 12 / static_cast<int32_t>(numEnemy);
    // Each part's commander, by part number (ended by 0xff).
    int32_t partCommanders[0x40] = {};
    int32_t partNumber = 1;
    MCMissionLogisticsBridge bridge;

    for (int32_t zoneBase = 0; zoneBase < 6; zoneBase += 3)
    {
        MCDropSlot** table = zoneBase == 0 ? isSlots : clanSlots;
        const bool ownTable = table == &DropSlots[0][0];
        const int32_t slotsPerPlayer = ownTable ? homeSlotsPerPlayer : enemySlotsPerPlayer;
        const int32_t commanderBase = ownTable ? 0 : 3;

        for (int32_t index = 0; index < 12; index++)
        {
            const int32_t commander = index / slotsPerPlayer + commanderBase;
            MCDropSlot* slot = table[index];
            MCLogPart* part = slot->Part;

            if (part == nullptr)
            {
                continue;
            }

            char profileName[0x20];
            std::snprintf(profileName, sizeof(profileName), "part%d", partNumber);
            const int32_t partType = part->PartType;

            if (partType == 1)
            {
                bridge.LogisticsMechProfileWriter(profileName, static_cast<MCLogMech*>(part), 0);
            }
            else
            {
                bridge.LogisticsVehicleProfileWriter(profileName, static_cast<MCLogVehicle*>(part), 0);
            }

            out << "[Part" << partNumber << "]" << '\n';
            out << "ul ObjectNumber         = " << static_cast<unsigned long>(part->Chassis) << '\n';
            out << "ul ControlType          = " << controlType << '\n';
            out << "b PlayerPart            = " << (part->LocalPart != 0 ? "True" : "False") << '\n';
            out << "ul ControlDataType      = " << partType << '\n';
            out << "c MyIcon                = 0" << '\n';
            out << "c TeamId\t\t\t\t= " << (ownTable ? ownTeam : otherTeam) << '\n';
            const int32_t commanderID = part->CommanderID;
            out << "l CommanderId\t\t    = " << commanderID << '\n';
            out << "st ObjectProfile        = \"" << profileName << "\"" << '\n';
            out << "ul Gesture              = 2" << '\n';
            out << "l PaintScheme           = " << MultiPlayerColors[commander] << '\n';
            out << "f Velocity              = 0.0" << '\n';
            out << "l Active                = 1" << '\n';
            out << "l Exists                = 1" << '\n';
            const int32_t zone = zoneBase + slot->Lance;
            const DeploySlotInfo& info = DeploySlotPlacements[zone][slot->Slot];
            // The sums were made on the x87 and printed as doubles.
            out << "f PositionX             = "
                << static_cast<double>(info.OffsetX) + static_cast<double>(DropZonePositions[zone].X) << '\n';
            out << "f PositionY             = "
                << static_cast<double>(DropZonePositions[zone].Y) + static_cast<double>(info.OffsetY) << '\n';
            out << "f PositionZ             = -1.0" << '\n';
            out << "f Rotation              = " << static_cast<double>(info.Rotation) << '\n';

            if (partType == 1)
            {
                // A mech's pilot: the local player's by pilot index, another player's after the local ones.
                auto* mech = static_cast<MCLogMech*>(part);
                const int32_t pilot = mech->LocalPart != 0
                                          ? mech->PilotIndex + 1
                                          : networkPilots->GetWarriorIndex(mech->NetworkPilot) + numAssigned + 1;
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
        MCDropSlot** table = pass == 0 ? isSlots : clanSlots;
        int32_t groupNumber = 0;
        int32_t inGroup = 0;

        for (int32_t index = 0; index < 12;)
        {
            if (table[index]->Part != nullptr)
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
    out.WriteFile(outName);
    return 0;
}

namespace
{
    /// <summary>
    /// The logistics cheat codes as DirectInput scan codes, each ended by 0xff (0x00782210; the name is the port's):
    /// MITCHLOVESYOU, HEREITCOMES, POUNDOFFLESH, KEEPTHEHAMMERDOWN, ROCKANDROLLPEOPLE, INFO, COCKADOODLEDOO.
    /// </summary>
    const int16_t LogCheatCodes[7][18] = {
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
    int32_t MissionWarpActive = 0;
    /// <summary>The mission number being typed for the warp, -1 before the first digit (0x00797858; the name is the port's).</summary>
    int8_t MissionWarpNumber = -1;
}

auto MCLogistics::ProcessCheatCode(int16_t key) -> void
{
    int32_t position = LogCurCheatChar;

    if (InDemo != 0 || MPlayer != nullptr || CheatsOn == 0)
    {
        return;
    }

    int32_t matched = -1;

    if (MissionWarpActive != 0)
    {
        // Two digit keys (scan codes 2..11 for 1..0) pick the mission to jump to.
        if (MissionWarpNumber < 0)
        {
            if (key > 10)
            {
                MissionWarpNumber = 0;
                return;
            }

            MissionWarpNumber = static_cast<int8_t>(static_cast<char>(key) * 10 - 10);
            return;
        }

        if (key < 12)
        {
            MissionWarpNumber = static_cast<int8_t>(MissionWarpNumber + static_cast<char>(key) - 1);
            MCFitIniFile file;
            char text[256];
            std::snprintf(text, sizeof(text), "%s%s.fit", MissionPath, MissionName);
            file.Open(text);
            file.SeekBlock("OpInfo");
            // Count the operation's missions.
            int32_t count;
            int32_t index = 0;
            int32_t result;

            do
            {
                count = index + 1;
                std::snprintf(text, sizeof(text), "Scenario%dMission", index);
                int32_t value = 0;
                result = file.ReadIdLong(text, value);
                index = count;
            } while (result == 0);

            file.Close();
            MissionWarpActive = 0;

            if (count <= MissionWarpNumber)
            {
                MissionWarpNumber = -1;
                return;
            }

            SoundSystem()->PlayBettySample(4);
            CurrentMission = MissionWarpNumber;
            std::snprintf(text, sizeof(text), "start%d", CurrentMission);
            // The original called the bridge with a stack address as this (it has no fields).
            MCMissionLogisticsBridge bridge;
            bridge.LogisticsSaveGame(text);
            char extension[] = ".sav";
            LoadCampaign(text, extension, 0, 0);
            MissionWarpNumber = -1;
            SetUpBriefingScreen(1);
            return;
        }

        MissionWarpActive = 0;
        MissionWarpNumber = -1;
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

        if (key == LogCheatCodes[code][position])
        {
            if (LogCheatCodes[code][position + 1] == 0xff)
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
            SoundSystem()->PlayBettySample(4);

            for (MCLogMech* mech = ForceMechList->Mechs; mech != nullptr; mech = mech->Next)
            {
                MCMechRepairBlock* block = mech->RepairBlock;
                block->RepairArmor(-1);
                block->RepairInternal(-1);

                for (MCLogInventoryItem* item = mech->Inventory->Items; item != nullptr; item = item->Next)
                {
                    for (MCLogInventoryStat* stat = item->Stats; stat != nullptr; stat = stat->Next)
                    {
                        stat->Hits = 0;
                    }
                }

                block->SetArmorSlider(-1);
                block->SetInternalSlider(-1);
                block->SetEngineSlider(-1);
                block->DrawBackground(block->SlotIndex, nullptr);
            }
            break;
        }
        case 1:
        {
            // HEREITCOMES: one more of every component.
            SoundSystem()->PlayBettySample(4);

            for (MCLogInventoryItem* item = ComponentInventory->Items; item != nullptr; item = item->Next)
            {
                item->Count++;
            }

            MCLogInvScreen* screen = RepairScreen;

            if (CurrentScreen != screen)
            {
                screen = PurchaseScreen;
            }

            screen->CreateCompInvBlock();
            screen->SetUpCompInv(1, 1);
            break;
        }

        case 2:
        {
            // POUNDOFFLESH: a million resource points.
            SoundSystem()->PlayBettySample(4);
            ResourcePoints += 1000000;
            break;
        }
        case 4:
        {
            // ROCKANDROLLPEOPLE: no drop tonnage limit.
            SoundSystem()->PlayBettySample(4);
            HammerDown = 1;
            BriefingScreen->CalcTonnages();
            break;
        }
        case 5:
            // INFO: resets the warp number (but does not switch the warp on).
            MissionWarpNumber = -1;
            break;
        case 6:
            // COCKADOODLEDOO: put the logistics heap's free memory in the window title. The heap is gone, so the
            // code does nothing.
            break;

        case 7:
        {
            // Unreachable (there are seven codes, 0..6): makes the pilot called "rooster" (or named Scott) an elite
            // "Scott" at full health.
            int32_t pilotIndex = 0;
            MCLogWarrior* found = nullptr;
            int assignedPilot = 0;

            for (MCLogWarrior* warrior = WarriorList->Warriors; warrior != nullptr; warrior = warrior->Next)
            {
                if (std::strcmp(MCPort::StrLwr(warrior->Callsign), "rooster") == 0 ||
                    std::strcmp(warrior->Name, "Scott") == 0)
                {
                    found = warrior;
                    break;
                }
            }

            if (found == nullptr)
            {
                for (MCLogWarrior* warrior = AssignedWarriorList->Warriors; warrior != nullptr; warrior = warrior->Next)
                {
                    if (std::strcmp(MCPort::StrLwr(warrior->Callsign), "rooster") == 0 ||
                        std::strcmp(warrior->Name, "Scott") == 0)
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
            found->Skills[0] = skill;
            found->Skills[1] = skill;
            found->Skills[2] = skill;
            found->Skills[3] = skill;
            found->Health = 6.0f;
            found->Rank = 3;
            std::strcpy(found->Callsign, "Scott");

            if (assignedPilot == 0)
            {
                return;
            }

            MCLogMech* mech = ForceMechList->Mechs;

            while (mech->PilotIndex != pilotIndex)
            {
                mech = mech->Next;
            }

            mech->RepairBlock->DrawBackground(mech->RepairBlock->SlotIndex, nullptr);
            break;
        }

        default:
            // Case 3 (KEEPTHEHAMMERDOWN) does nothing in logistics.
            break;
    }
}

auto MCLogistics::FindMPMechList(uint32_t playerID, int teammate, int* listIndex) -> MCLogMechList*
{
    MCLogMechList** lists = teammate == 0 ? MpMechLists[1] : MpMechLists[0];
    MCLogMechList* found = nullptr;

    if (listIndex != nullptr)
    {
        *listIndex = -1;
    }

    int32_t index = 0;

    while (lists[index] == nullptr || lists[index]->PlayerID != playerID)
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
        MCTextStream log;
        log << "Deploying mech - isTeammate = " << teammate << '\n';
        log << "Player is " << MPlayer->SessionManager->GetPlayer(playerID)->Name
            << "with id: " << static_cast<unsigned long>(playerID) << '\n';

        for (int32_t i = 0; i < 3; i++)
        {
            log << "Sanity Check!!!" << '\n';
            log << "Friendly List DPID " << i << " = ";

            if (MpMechLists[0][i] == nullptr)
            {
                log << "NULL List!";
            }
            else
            {
                log << static_cast<int>(MpMechLists[0][i]->PlayerID);
            }

            log << '\n';
            log << "Enemy List DPID " << i << " = ";

            if (MpMechLists[1][i] == nullptr)
            {
                log << "NULL List!";
            }
            else
            {
                log << static_cast<int>(MpMechLists[1][i]->PlayerID);
            }

            log << '\n';
        }

        log.WriteFile("nomechlist.log");
    }

    Assert(found != nullptr, 0, " Could not find a List to add mech to.  Save nomechlist.log file!!!!!!!!! ");
    return found;
}

auto MCLogistics::FindMPVehicleList(uint32_t playerID, int teammate) -> MCLogVehicleList*
{
    MCLogVehicleList** lists = teammate == 0 ? MpVehicleLists[1] : MpVehicleLists[0];
    MCLogVehicleList* found = nullptr;

    for (int32_t index = 0; index < 3; index++)
    {
        if (lists[index] != nullptr && lists[index]->PlayerID == playerID)
        {
            found = lists[index];
            break;
        }
    }

    Assert(found != nullptr, 0, " Could not find a List to add vehicle to ");
    return found;
}

auto MCLogistics::AddReorderPilotIndexes(MCLogMech* mech) -> void
{
    for (; mech != nullptr; mech = mech->Next)
    {
        if (mech->PilotIndex >= 0)
        {
            mech->PilotIndex++;
        }
    }
}

auto MCLogistics::AddReorderPilotIndexes(MCLogVehicle*) -> void
{
}

auto MCLogistics::RemoveReorderPilotIndexes(MCLogVehicle*, MCLogVehicle*) -> void
{
}

auto MCLogistics::AddMechFromNetworkMessage(MCLogMechList* list, MCFIMessageHeader* message) -> MCLogPart*
{
    const auto* deploy = reinterpret_cast<const MCDeployForceMessage*>(message);
    // netmechs.rsp lists three variants per mech name.
    const uint32_t nameIndex = deploy->NameIndex * 3 + ((deploy->Flags & 0xc) >> 2);
    const uint32_t side = SideGroupID(deploy->Flags);
    char* mechName = NetListItem(NetMechNames, nameIndex);
    MCLogMech* mech = list->AddMech(mechName, 0, 1, MPlayer->HomeTeamGroupID == side ? 1 : 0);
    char* pilotName = NetListItem(NetWarriorNames, deploy->PilotNameIndex);
    // The pilot goes into the network pilot list (unsorted, so at its head).
    MCLogWarriorList* pilots = MpWarriorList;
    pilots->AddWarrior(pilotName, 0);
    MCLogWarrior* pilot = nullptr;
    pilots->GetWarriorInfo(0, pilot);
    mech->NetworkPilot = pilot;
    ReadDeployItems(mech, deploy);
    return mech;
}

auto MCLogistics::AddVehicleFromNetworkMessage(MCLogVehicleList* list, MCFIMessageHeader* message) -> MCLogPart*
{
    const auto* deploy = reinterpret_cast<const MCDeployForceMessage*>(message);
    const uint32_t side = SideGroupID(deploy->Flags);
    char* vehicleName = NetListItem(NetVehicleNames, deploy->NameIndex);
    MCLogVehicle* vehicle = list->AddVehicle(vehicleName, 0, 0, MPlayer->HomeTeamGroupID == side ? 1 : 0);
    ReadDeployItems(vehicle, deploy);
    return vehicle;
}

auto MCLogistics::RemoveForceAtDropSlot(int32_t slotIndex, uint32_t playerID, int teamTable) -> int
{
    MCLogMech* mech = nullptr;
    MCLogVehicle* vehicle = nullptr;
    const int32_t lance = slotIndex / 4;
    const int32_t slot = slotIndex % 4;
    MCDropSlot* dropSlot = teamTable == 0 ? OpponentDropSlots[lance][slot] : DropSlots[lance][slot];
    MCLogPart* part = dropSlot->Part;

    if (part == nullptr)
    {
        return 0;
    }

    if (part->PartType == 1)
    {
        mech = static_cast<MCLogMech*>(part);
    }
    else
    {
        vehicle = static_cast<MCLogVehicle*>(part);
    }

    dropSlot->Part = nullptr;

    if (teamTable != 0)
    {
        BriefingScreen->MpCalcTonnages();
        MCBriefingScreen* briefing = GlobalLogPtr->BriefingScreen;

        if (briefing->BriefingBox != nullptr)
        {
            briefing->RemoveChild(briefing->BriefingBox);
            GlobalLogPtr->BriefingScreen->BriefingBox = nullptr;
        }
    }

    if (mech == nullptr)
    {
        FindMPVehicleList(playerID, teamTable)->RemoveVehicle(vehicle);
    }
    else
    {
        // Original behaviour (OB-098): the pilot is removed by its id used as a list position.
        MpWarriorList->RemoveWarrior(static_cast<uint8_t>(mech->NetworkPilot->Id));
        FindMPMechList(playerID, teamTable, nullptr)->RemoveMech(mech);
    }

    // (The original painted the covered slot over a teammate's unit here; the screen draws its slots each frame.)
    return mech != nullptr || vehicle != nullptr ? 1 : 0;
}

auto CancelBool(int32_t) -> void
{
    if (LaunchedFromLobby != 0 && MPlayer != nullptr)
    {
        KillTheGame();
    }

    Cancel();
}

auto BackToSession() -> void
{
    GlobalLogPtr->SetUpSessionScreen();
}

auto BackToSessionBool(int32_t) -> void
{
    GlobalLogPtr->SetUpSessionScreen();
}

auto LostPlayerHandler(int32_t answer) -> void
{
    if (answer == 1)
    {
        DoExit();
        return;
    }

    // Show holdString (from handleLostPlayer) with an OK button; it closes itself after five seconds.
    MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
    dialog->SetText(HoldString);
    GlobalLogPtr->MessageDialog->SetTwoButton(0);
    dialog = GlobalLogPtr->MessageDialog;
    dialog->Callback = CancelBool;
    dialog->OkButton->Callback()->SetExec(nullptr);
    char upArt[] = "bh_okay.tga";
    char downArt[] = "bg_okay.tga";
    GlobalLogPtr->MessageDialog->OkButton->SetUpPicture(upArt);
    GlobalLogPtr->MessageDialog->OkButton->SetDownPicture(downArt);
    MCLogDialogButton* button = GlobalLogPtr->MessageDialog->OkButton;
    button->Disabled = 0;
    dialog = GlobalLogPtr->MessageDialog;
    dialog->Timeout = 5000;
    dialog->TimeoutResult = 1;
    dialog->Activate();
    GlobalLogPtr->MessageDialog->KeepCallbacks = 1;
}

auto LogisticsChatCallback(MCFidpMessage* message, void*) -> void
{
    GlobalLogPtr->HandleChatMessage(message->FromID, message->MessageBuffer);
}
