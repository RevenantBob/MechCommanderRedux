#include "stdafx.h"
#include "logistics/MCStoreRow.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCScrollPane.h"
#include "logistics/MCLogRows.h"
#include "logistics/MCPurchaseScreen.h"
#include "logistics/MCRepairScreen.h"
#include "main/MCLogistics.h"

auto MCStoreRow::MakeDragIcon(const MCDragState& drag, const std::function<void(MCLogPort& surface)>& render) -> void
{
    std::shared_ptr<MCDragIcon> icon = MCDragIcon::Create();
    icon->Begin(drag.X, drag.Y, 0x20, 0x20, render);
    icon->ShowOn(*GlobalLog()->PurchaseScreen, drag.X, drag.Y);
}

auto MCStoreRow::OverInventory(MCGuiEvent& event) -> bool
{
    std::shared_ptr<MCScrollPane> size = GlobalLog()->RepairScreen->InventoryPane;
    return OverPaneInside(*GlobalLog()->PurchaseScreen->InventoryPane, *size, *size, event);
}

auto MCStoreRow::OverStore(MCGuiEvent& event) -> bool
{
    std::shared_ptr<MCScrollPane> size = GlobalLog()->RepairScreen->UnitPane;
    return OverPaneInside(*GlobalLog()->PurchaseScreen->UnitPane, *size, *size, event);
}

auto MCStoreRow::PreHandleEvent(MCGuiObject& row, const MCDragState& drag, MCGuiEvent& event) -> bool
{
    if (OnRepairScreen())
    {
        return false;
    }

    if (!row.Parent.expired() && drag.Idle() && (event.Type == 8 || event.Type == 9))
    {
        row.ParentObject()->HandleEvent(event);
        return false;
    }

    return true;
}
