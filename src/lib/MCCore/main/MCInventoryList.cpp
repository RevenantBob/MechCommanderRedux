#include "stdafx.h"
#include "main/MCInventoryList.h"
#include "logistics/MCCompInventoryBlock.h"
#include "logistics/MCCompPurchaseBlock.h"
#include "logistics/MCPurProfile.h"
#include "main/MCLogistics.h"
#include "object/MCMasterComponent.h"

MCLogInventoryItem::MCLogInventoryItem() = default;

MCLogInventoryItem::~MCLogInventoryItem()
{
    PurchaseBlock.reset();
    InventoryBlock.reset();
}

MCInventoryList::~MCInventoryList()
{
    Clear();
}

auto MCInventoryList::FindStat(uint8_t statID, MCLogInventoryItem** owner) const -> MCLogInventoryStat*
{
    // Port fix (OB-091): the original read the first item's copy list before checking the list had items, and walked
    // Count copies even when the copy list was shorter; the port stops at the end of either.
    for (const std::unique_ptr<MCLogInventoryItem>& item : Items)
    {
        const size_t copies = std::min(item->Stats.size(), static_cast<size_t>(std::max(item->Count, 0)));

        for (size_t copy = 0; copy < copies; ++copy)
        {
            if (item->Stats[copy]->StatID == statID)
            {
                if (owner != nullptr)
                {
                    *owner = item.get();
                }

                return item->Stats[copy].get();
            }
        }
    }

    return nullptr;
}

auto MCInventoryList::FindItem(uint8_t masterID) const -> MCLogInventoryItem*
{
    for (const std::unique_ptr<MCLogInventoryItem>& item : Items)
    {
        if (item->MasterID <= masterID)
        {
            return item->MasterID == masterID ? item.get() : nullptr;
        }
    }

    return nullptr;
}

auto MCInventoryList::LoadDescription(int32_t index, MCLogInventoryItem* item) const -> void
{
    if (item == nullptr)
    {
        item = GetItemInfo(index);
    }

    if (item == nullptr || !item->Description.empty())
    {
        return;
    }

    // The component's own description block (Desc<master id>).
    item->Description = LoadDescriptionText(item->MasterID);
}

auto MCInventoryList::CreateStat(uint8_t itemNum, uint8_t hits, uint8_t facing, int16_t amount, uint8_t location)
    -> std::unique_ptr<MCLogInventoryStat>
{
    auto stat = std::make_unique<MCLogInventoryStat>();
    stat->StatID = NextStatID++;
    stat->Hits = hits;
    stat->Facing = facing;
    stat->Amount = amount;
    stat->Location = location;
    stat->ItemNum = itemNum;
    return stat;
}

auto MCInventoryList::GetItemInfo(int32_t index) const -> MCLogInventoryItem*
{
    if (index < 0 || index >= NumItems())
    {
        return nullptr;
    }

    return Items[static_cast<size_t>(index)].get();
}

auto MCInventoryList::AddCountToItem(int32_t count, int32_t masterID) -> void
{
    for (const std::unique_ptr<MCLogInventoryItem>& item : Items)
    {
        if (static_cast<int32_t>(item->MasterID) <= masterID)
        {
            if (item->MasterID == masterID)
            {
                item->Count = std::max(item->Count + count, 0);
            }

            return;
        }
    }
}

auto MCInventoryList::MakeInventoryBlock(MCLogInventoryItem* item) -> MCCompInventoryBlock*
{
    item->InventoryBlock = std::make_unique<MCCompInventoryBlock>();
    item->InventoryBlock->Init(item);
    return item->InventoryBlock.get();
}

auto MCInventoryList::Clear() -> void
{
    Items.clear();
    NextStatID = 0;
}

namespace
{
    /// <summary>
    /// A new inventory item for <paramref name="masterID"/> holding <paramref name="stat"/>, with its purchase and
    /// inventory widgets when <paramref name="widgets"/> is set.
    /// </summary>
    std::unique_ptr<MCLogInventoryItem> NewInventoryItem(uint8_t masterID, std::unique_ptr<MCLogInventoryStat> stat,
                                                         bool widgets)
    {
        auto item = std::make_unique<MCLogInventoryItem>();
        item->MasterID = masterID;
        const MCMasterComponent& master = MasterComponentList[masterID];
        item->Name = master.Name.substr(0, 0x1c);
        item->MasterValue = master.MasterID;
        // Ammunition counts as one item whatever the amount; anything else counts its amount.
        item->Count = master.Form == MCComponentForm::Ammo ? 1 : stat->Amount;
        item->Stats.push_back(std::move(stat));
        item->SortOrder = GlobalLogPtr->ComponentSort[masterID];
        const std::vector<uint32_t>& ranges = GlobalLogPtr->RangeSortList;

        if (const auto found = std::ranges::find(ranges, static_cast<uint32_t>(masterID)); found != ranges.end())
        {
            item->RangeIndex = static_cast<int32_t>(found - ranges.begin());
        }

        if (widgets)
        {
            item->PurchaseBlock = std::make_unique<MCCompPurchaseBlock>();
            item->PurchaseBlock->Init(item.get());
            item->PurchaseBlock->SortOrder = item->SortOrder;
            MCInventoryList::MakeInventoryBlock(item.get())->InventoryIndex = item->SortOrder;
        }

        return item;
    }
}

auto MCInventoryList::AddItem(uint8_t masterID, std::unique_ptr<MCLogInventoryStat> stat, bool widgets) -> int32_t
{
    const int32_t statID = stat->StatID;

    if (Items.empty())
    {
        // The first item is not given its description.
        Items.push_back(NewInventoryItem(masterID, std::move(stat), widgets));
        return statID;
    }

    // The list runs from the highest master id down; a copy of a component already there joins its copies.
    auto position = Items.begin();

    while (position != Items.end() && (*position)->MasterID > masterID)
    {
        ++position;
    }

    if (position != Items.end() && (*position)->MasterID == masterID)
    {
        MCLogInventoryItem& item = **position;
        // Original behaviour: a copy numbered above the first goes in front of it; any other goes right after the
        // first (the original's walk to the sorted place never moves on), so the copies are not sorted.
        const bool inFront = item.Stats.empty() || item.Stats.front()->ItemNum < stat->ItemNum;
        item.Stats.insert(item.Stats.begin() + (inFront ? 0 : 1), std::move(stat));
        ++item.Count;
        return statID;
    }

    MCLogInventoryItem* added = Items.insert(position, NewInventoryItem(masterID, std::move(stat), widgets))->get();
    LoadDescription(0, added);
    return statID;
}

auto MCInventoryList::RemoveItem(uint8_t masterID, int32_t statID) -> int32_t
{
    auto position = Items.begin();

    while (position != Items.end() && masterID < (*position)->MasterID)
    {
        ++position;
    }

    if (position == Items.end() || (*position)->MasterID != masterID)
    {
        return -1;
    }

    MCLogInventoryItem& item = **position;

    if (item.Count == 1)
    {
        // The last copy (whichever statID was asked for): the item goes with it.
        Items.erase(position);
        return 0;
    }

    // One copy: the one numbered statID, or the first when statID is -1.
    auto copy = item.Stats.begin();

    if (statID >= 0)
    {
        copy = std::ranges::find_if(item.Stats, [&](const std::unique_ptr<MCLogInventoryStat>& stat)
                                    { return static_cast<int32_t>(stat->StatID) == statID; });
    }
    else if (statID != -1)
    {
        return -1;
    }

    if (copy == item.Stats.end())
    {
        return -1;
    }

    item.Stats.erase(copy);
    --item.Count;
    // Original behaviour: a copy removed this way still reports -1.
    return -1;
}

auto MCInventoryList::GetItemStatIndex(int32_t statID) const -> MCLogInventoryItem*
{
    for (const std::unique_ptr<MCLogInventoryItem>& item : Items)
    {
        for (const std::unique_ptr<MCLogInventoryStat>& stat : item->Stats)
        {
            if (static_cast<int32_t>(stat->StatID) == statID)
            {
                return item.get();
            }
        }
    }

    return nullptr;
}

auto MCInventoryList::GetItemStatID(uint8_t masterID, int32_t copy) const -> int32_t
{
    const MCLogInventoryItem* item = FindItem(masterID);

    // Port fix: a count above the copies (AddCountToItem) read past the copy list.
    if (item == nullptr || copy < 0 || item->Count <= copy || static_cast<size_t>(copy) >= item->Stats.size())
    {
        return -1;
    }

    return item->Stats[static_cast<size_t>(copy)]->StatID;
}

auto MCInventoryList::GetIndexFromMasterID(uint8_t masterID) const -> int32_t
{
    const auto found = std::ranges::find_if(Items, [&](const std::unique_ptr<MCLogInventoryItem>& item)
                                            { return item->MasterID == masterID; });
    return found != Items.end() ? static_cast<int32_t>(found - Items.begin()) : -1;
}

auto MCInventoryList::GetMasterIDFromIndex(int32_t index) const -> int32_t
{
    // A negative position read the first item, as the original's walk did.
    if (index >= NumItems() || Items.empty())
    {
        return 0xff;
    }

    return Items[static_cast<size_t>(std::max(index, 0))]->MasterID;
}

auto MCInventoryList::GetMasterID(uint8_t statID) const -> uint8_t
{
    MCLogInventoryItem* item = nullptr;
    return FindStat(statID, &item) != nullptr ? item->MasterID : 0xff;
}

auto MCInventoryList::GetFacing(uint8_t statID) const -> uint8_t
{
    const MCLogInventoryStat* stat = FindStat(statID);
    return stat != nullptr ? stat->Facing : 0xff;
}

auto MCInventoryList::GetItemCount(uint8_t masterID) const -> int32_t
{
    const MCLogInventoryItem* item = FindItem(masterID);
    return item != nullptr ? item->Count : 0;
}

auto MCInventoryList::HitItem(uint8_t statID, uint8_t hits) -> int32_t
{
    MCLogInventoryStat* stat = FindStat(statID);

    if (stat == nullptr)
    {
        return -1;
    }

    stat->Hits = hits;
    return 0;
}

auto MCInventoryList::SetStatLoc(uint8_t statID, int32_t location) -> int32_t
{
    MCLogInventoryStat* stat = FindStat(statID);

    if (stat == nullptr)
    {
        return -1;
    }

    stat->Location = static_cast<uint8_t>(location);
    return 0;
}
