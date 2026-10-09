#include "stdafx.h"
#include "logistics/MCStoreRow.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCScrollPane.h"
#include "logistics/MCLogRows.h"
#include "logistics/MCPurchaseScreen.h"
#include "logistics/MCRepairScreen.h"
#include "main/logistics.h"

auto MCStoreRow::MakeDragIcon(const MCDragState& drag, const std::function<void(MCLogPort* surface)>& render) -> void
{
    MCDragIcon* icon = MCDragIcon::Create();
    icon->Begin(drag.X, drag.Y, 0x20, 0x20, render);
    icon->ShowOn(GlobalLogPtr->PurchaseScreen, drag.X, drag.Y);
}

auto MCStoreRow::OverInventory(MCGuiEvent* event) -> bool
{
    MCScrollPane* size = GlobalLogPtr->RepairScreen->InventoryPane;
    return OverPaneInside(GlobalLogPtr->PurchaseScreen->InventoryPane, size, size, event);
}

auto MCStoreRow::OverStore(MCGuiEvent* event) -> bool
{
    MCScrollPane* size = GlobalLogPtr->RepairScreen->UnitPane;
    return OverPaneInside(GlobalLogPtr->PurchaseScreen->UnitPane, size, size, event);
}

auto MCStoreRow::PreHandleEvent(MCGuiObject* row, const MCDragState& drag, MCGuiEvent* event) -> bool
{
    if (OnRepairScreen())
    {
        return false;
    }

    if (row->Parent != nullptr && drag.Idle() && (event->Type == 8 || event->Type == 9))
    {
        row->Parent->HandleEvent(event);
        return false;
    }

    return true;
}
