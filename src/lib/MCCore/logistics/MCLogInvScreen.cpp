#include "stdafx.h"
#include "logistics/MCLogInvScreen.h"
#include "gui/MCGuiFont.h"
#include "gui/MCScrollPane.h"
#include "logistics/MCRepairScreen.h"
#include "logistics/MCInventoryBlock.h"
#include "logistics/MCMechInventoryBlock.h"
#include "logistics/MCPilotInventoryBlock.h"
#include "logistics/MCVehicleInventoryBlock.h"
#include "logistics/MCCompInventoryBlock.h"
#include "logistics/MCDragIcon.h"
#include "logistics/MCLogRows.h"
#include "main/MCGamePaths.h"
#include "logistics/MCPurchaseScreen.h"
#include "logistics/MCMechRepairBlock.h"
#include "logistics/MCVehicleRepairBlock.h"
#include "logistics/MCBriefingBox.h"
#include "logistics/MCPurMechList.h"
#include "logistics/MCPurPilotList.h"
#include "logistics/MCPurVehicleList.h"
#include "logistics/MCMechPurchaseBlock.h"
#include "logistics/MCPilotPurchaseBlock.h"
#include "logistics/MCVehiclePurchaseBlock.h"
#include "logistics/MCCompPurchaseBlock.h"
#include "logistics/MCUnitLimits.h"
#include "logistics/MCPurProfile.h"
#include "main/MCLogistics.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>The smallest inventory view: the inventory pane's height.</summary>
    constexpr int32_t MinInvPortHeight = 0x10d;
    /// <summary>The width of an inventory view.</summary>
    constexpr int32_t InvPortWidth = 0xab;

    /// <summary>Hides and removes every child of <paramref name="pane"/> (the blocks shown in it).</summary>
    void ClearPane(MCScrollPane* pane)
    {
        for (int32_t count = pane->NumberOfChildren(); count > 0; --count)
        {
            pane->Child(0)->ShowGuiWindow(false);
            pane->RemoveChild(pane->Child(0));
        }
    }

    /// <summary>Whether a row from <paramref name="top"/> of <paramref name="height"/> lines meets the open view's scissor.</summary>
    bool RowShown(const MCView& place, int32_t top, int32_t height)
    {
        const int32_t screenTop = place.OriginY + top;
        return screenTop <= place.Scissor.Y1 && place.Scissor.Y0 < screenTop + height;
    }

    /// <summary>The store's tabs, as <see cref="DrawStore"/> draws them.</summary>
    enum class MCStoreTab
    {
        Mechs,
        Vehicles,
        Components,
        Pilots
    };

    /// <summary>
    /// Draws a store tab into its view <paramref name="port"/>: its rows, each at its list row, over
    /// <paramref name="color"/> (what the rows' DrawBackground painted into the tab's picture).
    /// </summary>
    void DrawStore(MCStoreTab tab, int32_t color, MCGuiPort* port)
    {
        auto* view = static_cast<MCLogPort*>(port);
        VfxPaneWipe(view->Frame(), static_cast<uint32_t>(color));

        auto drawRow = [&](auto* block)
        {
            const int32_t top = block->Row * MCLogInvScreen::UnitBlockHeight;

            if (RowShown(view->View, top, MCLogInvScreen::UnitBlockHeight))
            {
                block->DrawRow(view, top);
            }
        };

        switch (tab)
        {
            case MCStoreTab::Mechs:
            {
                for (const auto& purMech : GlobalLogPtr->PurMechList->Mechs)
                {
                    drawRow(purMech->Block.get());
                }
                break;
            }

            case MCStoreTab::Vehicles:
            {
                for (const auto& purVehicle : GlobalLogPtr->PurVehicleList->Vehicles)
                {
                    drawRow(purVehicle->Block.get());
                }
                break;
            }

            case MCStoreTab::Components:
            {
                for (const std::unique_ptr<MCLogInventoryItem>& item : GlobalLogPtr->PurchaseComponents->Items)
                {
                    drawRow(item->PurchaseBlock.get());
                }
                break;
            }

            case MCStoreTab::Pilots:
            {
                for (const auto& pilot : GlobalLogPtr->PurPilotList->Pilots)
                {
                    if (pilot->Status == 0)
                    {
                        drawRow(pilot->Block.get());
                    }
                }
                break;
            }
        }
    }

    /// <summary>
    /// The view of store tab <paramref name="tab"/> in <paramref name="port"/>, <paramref name="width"/> x
    /// <paramref name="height"/> rows (at least the pane's) over <paramref name="color"/>. The original freed the
    /// tab's picture and made a new one; the view is kept and resized, as a pane may still show it.
    /// </summary>
    MCLogPort* StoreView(std::unique_ptr<MCLogPort>& port, MCStoreTab tab, MCScrollPane* pane, int32_t width,
                         int32_t height, int32_t color)
    {
        if (port == nullptr)
        {
            port = std::make_unique<MCLogPort>();
            port->DrawContent = [tab, color](MCGuiPort* view) { DrawStore(tab, color, view); };
        }

        port->InitView(width, std::max(height, pane->Height()));
        return port.get();
    }

    /// <summary>The column headers of the inventory tabs, drawn at (0xc4, 0x65).</summary>
    constexpr std::array<std::string_view, 4> InvHeaderArt = {"lscdwm.tga", "lscdwp.tga", "lscdwc.tga", "lscdwv.tga"};

    /// <summary>The inventory screens (the purchase and repair screens), for <see cref="MCLogInvScreen::Of"/>.</summary>
    std::vector<MCLogInvScreen*> InvScreens;

    /// <summary>
    /// Draws inventory tab <paramref name="tab"/> into its view <paramref name="port"/>: the rows its blocks drew into
    /// the tab's picture, each at its list row, over colour 0x10.
    /// </summary>
    void DrawInvTab(int32_t tab, MCGuiPort* port)
    {
        auto* view = static_cast<MCLogPort*>(port);
        VfxPaneWipe(view->Frame(), 0x10);
        const MCView& place = view->View;

        auto drawRow = [&](MCInventoryBlock* block)
        {
            const int32_t top = block->ListIndex * block->WinHeight;

            if (RowShown(place, top, block->WinHeight))
            {
                block->DrawRow(view, top);
            }
        };

        switch (tab)
        {
            case 0:
            {
                for (const std::unique_ptr<MCLogMech>& mech : GlobalLogPtr->MechList->Mechs)
                {
                    drawRow(mech->InventoryBlock.get());
                }
                break;
            }

            case 1:
            {
                for (const std::unique_ptr<MCLogWarrior>& warrior : GlobalLogPtr->WarriorList->Warriors)
                {
                    drawRow(warrior->InventoryBlock.get());
                }
                break;
            }

            case 2:
            {
                for (const std::unique_ptr<MCLogInventoryItem>& item : GlobalLogPtr->ComponentInventory->Items)
                {
                    if (item->InventoryBlock->ListIndex >= 0)
                    {
                        drawRow(item->InventoryBlock.get());
                    }
                }
                break;
            }

            default:
            {
                for (const std::unique_ptr<MCLogVehicle>& vehicle : GlobalLogPtr->VehicleList->Vehicles)
                {
                    drawRow(vehicle->InventoryBlock.get());
                }
                break;
            }
        }
    }

    /// <summary>A component's details in the info box (<see cref="MCInvInfoBox"/>).</summary>
    void DrawComponentInfo(MCInvInfoBox& info, MCLogPort* port)
    {
        MCLogPort* picture = LogScreenArt(std::format("lscicc{:02}.tga", info.ComponentPicture));

        // The repair screen's weapon list copied the picture opaque, the component tab keyed.
        if (info.InfoKind == MCInvInfoBox::Kind::RepairItem)
        {
            VfxPaneCopy(picture->Frame(), 0, 0, port->Frame(), 9, 0x191, -1);
        }
        else
        {
            picture->CopyTo(port->Frame(), 9, 0x191, true);
        }

        auto write = [&](int32_t y, const std::string& text)
        { YellowDropFont->WriteString(port->Frame(), 0x53, y, text); };
        write(0x1a5, info.RangeText);
        write(0x19c, info.DamageText);
        write(0x193, info.RecycleText);

        if (!info.Description.empty())
        {
            DrawInfoDescription(port, 0xc5, 0x26, info.Description.data(), 8, 0x1b3);
        }
    }

    /// <summary>
    /// The view of inventory tab <paramref name="tab"/>, <paramref name="width"/> x <paramref name="height"/>, which
    /// the tab is drawn into each frame (the original painted each block's row into a new picture). The same view is
    /// kept and resized: the original freed the old picture, but a pane could still show it until the tab was set up
    /// again.
    /// </summary>
    MCLogPort* NewTabView(int32_t tab, int32_t width, int32_t height)
    {
        std::unique_ptr<MCLogPort>& port = GlobalLogPtr->InvTabPorts[static_cast<size_t>(tab)];

        if (port == nullptr)
        {
            port = std::make_unique<MCLogPort>();
            port->DrawContent = [tab](MCGuiPort* view) { DrawInvTab(tab, view); };
        }

        port->InitView(width, height);
        return port.get();
    }

    /// <summary>The view of inventory tab <paramref name="tab"/> for <paramref name="count"/> blocks (at least the pane's height).</summary>
    MCLogPort* NewInvPort(int32_t tab, int32_t count)
    {
        return NewTabView(tab, InvPortWidth, std::max(count * MCLogInvScreen::InvBlockHeight, MinInvPortHeight));
    }

    /// <summary>
    /// The inventory tab art common to the SetUp*Inv functions: the blank info box of tab <paramref name="tab"/>
    /// when <paramref name="redrawTabs"/>, then the tab's column header.
    /// </summary>
    void DrawInvTabArt(MCLogInvScreen* screen, int32_t tab, bool redrawTabs)
    {
        if (redrawTabs)
        {
            screen->Info.Blank(tab);
        }

        screen->Info.Header = tab;
    }

    /// <summary>Shows inventory tab <paramref name="tab"/>'s view in both screens' inventory panes.</summary>
    void ShowInvTab(int32_t tab, bool resetScroll)
    {
        MCLogPort* view = GlobalLogPtr->InvTabPorts[static_cast<size_t>(tab)].get();
        GlobalLogPtr->PurchaseScreen->InventoryPane->SetDisplayPort(view, resetScroll);
        GlobalLogPtr->RepairScreen->InventoryPane->SetDisplayPort(view, resetScroll);
    }

    /// <summary>Takes the blocks out of both screens' inventory panes.</summary>
    void ClearInventoryPanes()
    {
        ClearPane(GlobalLogPtr->RepairScreen->InventoryPane);
        ClearPane(GlobalLogPtr->PurchaseScreen->InventoryPane);
    }

    /// <summary>Numbers the store's component blocks: row n goes to the item whose block has sort order n.</summary>
    void ReIndexPass()
    {
        const auto& items = GlobalLogPtr->PurchaseComponents->Items;
        int32_t row = 0;

        // Only sort orders 0..49 get a row, as in the original.
        for (int32_t order = 0; order < 50; ++order)
        {
            const auto item = std::ranges::find_if(items, [&](const std::unique_ptr<MCLogInventoryItem>& entry)
                                                   { return entry->PurchaseBlock->SortOrder == order; });

            if (item != items.end())
            {
                (*item)->PurchaseBlock->Row = row++;
            }
        }
    }

    /// <summary>Places a store block at row <paramref name="row"/> and shows it.</summary>
    template <typename T> void PlaceStoreBlock(T* block, int32_t row)
    {
        block->MoveTo(0, row * MCLogInvScreen::UnitBlockHeight, false);
        block->ShowGuiWindow(true);
        block->SetDepth(100);
    }
}

// MCLogInvScreen

MCLogInvScreen::~MCLogInvScreen()
{
    std::erase(InvScreens, this);
}

auto MCLogInvScreen::MakePanes(bool emptyUnitPane) -> void
{
    _OwnedInventoryPane = MCMakeGui<MCScrollPane>();
    InventoryPane = _OwnedInventoryPane.get();
    InventoryPane->Init(0xb8, 0x10d, 8, 0x6b, static_cast<MCLogPort*>(nullptr));
    InventoryPane->ClearDisplayPort();

    _OwnedUnitPane = MCMakeGui<MCScrollPane>();
    UnitPane = _OwnedUnitPane.get();
    UnitPane->Init(0x1aa, 0x1cc, 0xd3, 0x11, std::format("{}logart\\lsrbk01.tga", ArtPath).c_str());

    if (emptyUnitPane)
    {
        UnitPane->ClearDisplayPort();
    }

    AddChild(InventoryPane);
    AddChild(UnitPane);
}

auto MCLogInvScreen::FreePanes() -> void
{
    // The panes show the logistics screen's views, or the screen's own store views.
    for (MCGuiOwned<MCScrollPane>* pane : {&_OwnedInventoryPane, &_OwnedUnitPane})
    {
        if (*pane != nullptr)
        {
            (*pane)->ClearDisplayPort();
            pane->reset();
        }
    }

    InventoryPane = nullptr;
    UnitPane = nullptr;
}

auto MCLogInvScreen::CreateVehiclePane() -> void
{
    MCScrollPane* pane = GlobalLogPtr->RepairScreen->UnitPane;
    pane->SetDisplayPort(MCRepairScreen::NewUnitRowsView(pane), true);
    int32_t row = 0;

    auto place = [&](auto* block)
    {
        UnitPane->AddChild(block);
        block->SlotIndex = row;
        block->MoveTo(0, row * UnitBlockHeight, false);
        block->ShowGuiWindow(true);
        block->SetDepth(100);
        block->DrawBackground(row, nullptr);
        ++row;
    };

    for (const std::unique_ptr<MCLogMech>& mech : GlobalLogPtr->ForceMechList->Mechs)
    {
        place(mech->RepairBlock.get());
    }

    for (const std::unique_ptr<MCLogVehicle>& vehicle : GlobalLogPtr->ForceVehicleList->Vehicles)
    {
        place(vehicle->RepairBlock.get());
    }
}

auto MCLogInvScreen::CreatePurVehiclePane(bool pilotsOnly) -> void
{
    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen.get();
    MCScrollPane* pane = screen->UnitPane;
    const int32_t width = pane->Width() - 0xd;

    if (!pilotsOnly)
    {
        // The store's mechs.
        pane->SetDisplayPort(StoreView(screen->PurMechPort, MCStoreTab::Mechs, pane, width,
                                       GlobalLogPtr->PurMechList->GetMechCount() * UnitBlockHeight, 0x10),
                             true);
        int32_t row = 0;

        for (const auto& purMech : GlobalLogPtr->PurMechList->Mechs)
        {
            MCMechPurchaseBlock* block = purMech->Block.get();
            block->Row = row;
            PlaceStoreBlock(block, row);
            block->DrawBackground(row);
            ++row;
        }

        // The store's vehicles.
        StoreView(screen->PurVehiclePort, MCStoreTab::Vehicles, pane, width,
                  GlobalLogPtr->PurVehicleList->GetVehicleCount() * UnitBlockHeight, 0x10);
        row = 0;

        for (const auto& purVehicle : GlobalLogPtr->PurVehicleList->Vehicles)
        {
            MCVehiclePurchaseBlock* block = purVehicle->Block.get();
            block->Row = row;
            PlaceStoreBlock(block, row);
            block->DrawBackground(row);
            ++row;
        }

        // The store's components, ordered by ReIndexComponents.
        StoreView(screen->PurCompPort, MCStoreTab::Components, pane, width,
                  GlobalLogPtr->PurchaseComponents->NumItems() * UnitBlockHeight, 0x10);
        ReIndexComponents();

        for (const std::unique_ptr<MCLogInventoryItem>& item : GlobalLogPtr->PurchaseComponents->Items)
        {
            MCCompPurchaseBlock* block = item->PurchaseBlock.get();
            PlaceStoreBlock(block, block->Row);
            block->DrawBackground(block->Row, item->PurchaseBlock->Item->MasterID);
        }
    }

    // The pilots for hire (those not yet hired), rebuilt every time. Original behaviour (OB-076): when only the
    // pilots were rebuilt, the old pilot picture was replaced without being freed (the view is kept and resized).
    int32_t visible = 0;

    for (const auto& pilot : GlobalLogPtr->PurPilotList->Pilots)
    {
        if (pilot->Status == 0)
        {
            ++visible;
        }
    }

    StoreView(screen->PurPilotPort, MCStoreTab::Pilots, pane, width, visible * UnitBlockHeight, 0xff);
    int32_t row = 0;

    for (const auto& pilot : GlobalLogPtr->PurPilotList->Pilots)
    {
        if (pilot->Status != 0)
        {
            continue;
        }

        MCPilotPurchaseBlock* block = pilot->Block.get();
        block->Row = row;
        PlaceStoreBlock(block, row);
        block->DrawBackground(row);
        ++row;
    }
}

auto MCLogInvScreen::CreateMechInvBlock() -> void
{
    NewInvPort(0, GlobalLogPtr->MechList->GetMechCount());
    int32_t index = 0;

    for (const std::unique_ptr<MCLogMech>& mech : GlobalLogPtr->MechList->Mechs)
    {
        MCMechInventoryBlock* block = mech->InventoryBlock.get();
        block->ListIndex = index++;
        block->DrawBackground();
    }
}

auto MCLogInvScreen::CreateVhclInvBlock() -> void
{
    NewInvPort(3, GlobalLogPtr->VehicleList->GetVehicleCount());
    int32_t index = 0;

    for (const std::unique_ptr<MCLogVehicle>& vehicle : GlobalLogPtr->VehicleList->Vehicles)
    {
        MCVehicleInventoryBlock* block = vehicle->InventoryBlock.get();
        block->ListIndex = index++;
        block->DrawBackground();
    }
}

auto MCLogInvScreen::CreatePilotInvBlock() -> void
{
    const auto& warriors = GlobalLogPtr->WarriorList->Warriors;
    const int32_t height =
        warriors.empty() ? 0
                         : warriors.front()->InventoryBlock->Height() * GlobalLogPtr->WarriorList->GetWarriorCount();
    NewTabView(1, InventoryPane->Width() - 0xd, std::max(height, InventoryPane->Height()));
    int32_t index = 0;

    for (const std::unique_ptr<MCLogWarrior>& warrior : warriors)
    {
        MCPilotInventoryBlock* block = warrior->InventoryBlock.get();
        block->ListIndex = index++;
        block->DrawBackground();
    }
}

auto MCLogInvScreen::DrawBlankInvInfoBlock(int32_t tab) -> void
{
    if (tab < 0)
    {
        tab = GlobalLogPtr->CurrentInvTab;
    }

    if (tab < 0 || tab > 3)
    {
        return;
    }

    if (MCLogInvScreen* screen = Of(GlobalLogPtr->CurrentScreen))
    {
        screen->Info.Blank(tab);
    }
    else
    {
        GlobalLogPtr->InventoryIconPorts[tab]->CopyTo(GlobalLogPtr->CurrentScreen->Lport()->Frame(), 2, 0x18a, false);
    }
}

auto MCLogInvScreen::CreateCompInvBlock() -> void
{
    NewInvPort(2, GlobalLogPtr->ReIndexInventory());

    for (const std::unique_ptr<MCLogInventoryItem>& item : GlobalLogPtr->ComponentInventory->Items)
    {
        item->InventoryBlock->DrawBackground();
    }
}

auto MCLogInvScreen::SetUpMechInv(bool resetScroll, bool redrawTabs) -> void
{
    GlobalLogPtr->CurrentInvTab = 0;
    DrawInvTabArt(this, 0, redrawTabs);
    ClearInventoryPanes();

    for (const std::unique_ptr<MCLogMech>& mech : GlobalLogPtr->MechList->Mechs)
    {
        MCMechInventoryBlock* block = mech->InventoryBlock.get();
        InventoryPane->AddChild(block);
        block->ShowGuiWindow(true);
        block->BringToFront(0);
        block->MoveTo(0, block->ListIndex * InvBlockHeight, false);
    }

    ShowInvTab(0, resetScroll);
}

auto MCLogInvScreen::SetUpMechPurchase() -> void
{
    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen.get();

    if (GlobalLogPtr->CurrentScreen != screen)
    {
        return;
    }

    ClearPane(screen->UnitPane);
    screen->UnitPane->SetDisplayPort(screen->PurMechPort.get(), true);
    int32_t row = 0;

    for (const auto& purMech : GlobalLogPtr->PurMechList->Mechs)
    {
        MCMechPurchaseBlock* block = purMech->Block.get();
        UnitPane->AddChild(block);
        block->ShowGuiWindow(true);
        block->MoveTo(0, block->Height() * row, false);
        block->BringToFront(0);
        // Show the first variant still on sale.
        int32_t variant = 0;

        if (purMech->Variants[0]->NumAvailable == 0)
        {
            if (purMech->Variants[1]->NumAvailable != 0)
            {
                variant = 1;
            }
            else if (purMech->Variants[2]->NumAvailable != 0)
            {
                variant = 2;
            }
        }

        if (variant != block->CurVariant)
        {
            block->CurVariant = variant;
            block->DrawBackground(row);
        }

        ++row;
    }
}

auto MCLogInvScreen::SetUpPilotInv(bool resetScroll, bool redrawTabs) -> void
{
    GlobalLogPtr->CurrentInvTab = 1;
    DrawInvTabArt(this, 1, redrawTabs);
    ClearInventoryPanes();
    int32_t yPos = 0;

    for (const std::unique_ptr<MCLogWarrior>& warrior : GlobalLogPtr->WarriorList->Warriors)
    {
        // The assigned pilots come last and aren't shown.
        if (warrior->Assigned != 0)
        {
            break;
        }

        MCPilotInventoryBlock* block = warrior->InventoryBlock.get();
        InventoryPane->AddChild(block);
        block->ShowGuiWindow(true);
        block->BringToFront(0);
        block->DrawBackground();

        if (resetScroll)
        {
            block->MoveTo(0, yPos, false);
        }

        yPos += InvBlockHeight;
    }

    ShowInvTab(1, resetScroll);
}

auto MCLogInvScreen::SetUpCompInv(bool resetScroll, bool redrawTabs) -> void
{
    GlobalLogPtr->CurrentInvTab = 2;
    DrawInvTabArt(this, 2, redrawTabs);
    ClearInventoryPanes();

    for (const std::unique_ptr<MCLogInventoryItem>& item : GlobalLogPtr->ComponentInventory->Items)
    {
        MCCompInventoryBlock* block = item->InventoryBlock.get();

        if (block->ListIndex < 0)
        {
            continue;
        }

        InventoryPane->AddChild(block);
        block->ShowGuiWindow(true);
        block->BringToFront(0);
        block->MoveTo(0, block->ListIndex * InvBlockHeight, false);
        block->DrawBackground();
    }

    ShowInvTab(2, resetScroll);
}

auto MCLogInvScreen::SetUpVhclInv(bool resetScroll, bool redrawTabs) -> void
{
    GlobalLogPtr->CurrentInvTab = 3;
    DrawInvTabArt(this, 3, redrawTabs);
    ClearInventoryPanes();
    int32_t yPos = 0;

    for (const std::unique_ptr<MCLogVehicle>& vehicle : GlobalLogPtr->VehicleList->Vehicles)
    {
        MCVehicleInventoryBlock* block = vehicle->InventoryBlock.get();
        InventoryPane->AddChild(block);
        block->ShowGuiWindow(true);
        block->BringToFront(0);
        block->MoveTo(0, yPos, false);
        yPos += InvBlockHeight;
    }

    ShowInvTab(3, resetScroll);
}

auto MCLogInvScreen::SetUpVehiclePurchase() -> void
{
    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen.get();

    if (GlobalLogPtr->CurrentScreen != screen)
    {
        return;
    }

    ClearPane(screen->UnitPane);
    int32_t row = 0;

    for (const auto& purVehicle : GlobalLogPtr->PurVehicleList->Vehicles)
    {
        MCVehiclePurchaseBlock* block = purVehicle->Block.get();
        UnitPane->AddChild(block);
        block->ShowGuiWindow(true);
        block->MoveTo(0, block->Height() * row, false);
        block->BringToFront(0);
        ++row;
    }

    screen->UnitPane->SetDisplayPort(screen->PurVehiclePort.get(), true);
}

auto MCLogInvScreen::SetUpPilotPurchase() -> void
{
    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen.get();

    if (GlobalLogPtr->CurrentScreen != screen)
    {
        return;
    }

    ClearPane(screen->UnitPane);
    int32_t row = 0;

    for (const auto& pilot : GlobalLogPtr->PurPilotList->Pilots)
    {
        if (pilot->Status != 0)
        {
            continue;
        }

        MCPilotPurchaseBlock* block = pilot->Block.get();
        UnitPane->AddChild(block);
        block->ShowGuiWindow(true);
        block->MoveTo(0, block->Height() * row, false);
        block->BringToFront(0);
        ++row;
    }

    screen->UnitPane->SetDisplayPort(screen->PurPilotPort.get(), true);
}

auto MCLogInvScreen::ReIndexComponents() -> void
{
    // The original runs the same pass twice.
    ReIndexPass();
    ReIndexPass();
}

auto MCLogInvScreen::SetUpCompPurchase() -> void
{
    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen.get();

    if (GlobalLogPtr->CurrentScreen != screen)
    {
        return;
    }

    ClearPane(screen->UnitPane);
    ReIndexComponents();

    for (const std::unique_ptr<MCLogInventoryItem>& item : GlobalLogPtr->PurchaseComponents->Items)
    {
        MCCompPurchaseBlock* block = item->PurchaseBlock.get();
        UnitPane->AddChild(block);
        block->ShowGuiWindow(true);
        block->MoveTo(0, block->Height() * block->Row, false);
        block->BringToFront(0);
    }

    screen->UnitPane->SetDisplayPort(screen->PurCompPort.get(), true);
}

auto MCLogInvScreen::RemovePilot(int32_t pilotIndex) -> void
{
    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen.get();
    MCScrollPane* pane = screen->UnitPane;
    const int32_t count = GlobalLogPtr->PurPilotList->GetVisiblePilotCount();
    // The original copied the old rows into a new picture, closing the gap: the rows below the removed one moved up
    // a block, and the rest was wiped. The view draws the remaining pilots at their new rows.
    MCLogPort* port =
        StoreView(screen->PurPilotPort, MCStoreTab::Pilots, pane, pane->Width() - 0x10, count * UnitBlockHeight, 0xff);
    screen->UnitPane->SetDisplayPort(port, true);

    // Move the blocks from the removed one on up a row.
    const auto& pilots = GlobalLogPtr->PurPilotList->Pilots;

    for (size_t index = 0; index < pilots.size(); ++index)
    {
        if (pilotIndex <= pilots[index]->Block->Row)
        {
            int32_t yPos = pilotIndex * UnitBlockHeight;

            for (; index < pilots.size(); ++index)
            {
                MCPilotPurchaseBlock* block = pilots[index]->Block.get();
                yPos += UnitBlockHeight;
                block->Row = pilotIndex;
                ++pilotIndex;
                block->MoveTo(0, yPos, false);
            }
            break;
        }
    }

    int32_t row = 0;

    for (const auto& pilot : GlobalLogPtr->PurPilotList->Pilots)
    {
        if (pilot->Status == 0)
        {
            pilot->Block->Row = row++;
        }
    }
}

auto MCLogInvScreen::Draw() -> void
{
    if (Lport()->ViewOpen())
    {
        VfxPaneCopy(LogScreenArt(BackgroundArt)->Frame(), 0, 0, Lport()->Frame(), 0, 0, -1);
        DrawInfo(Lport());
        GlobalLogPtr->DrawScreenChrome(this, Lport()->Frame());
    }

    MCLogObject::Draw();
}

auto MCLogInvScreen::InitLive(std::string_view artName) -> void
{
    BackgroundArt = artName;
    InvScreens.push_back(this);
}

auto MCLogInvScreen::ShowInfo(MCInvInfoBox::Kind kind, MCLogObject* source) -> void
{
    Info.InfoKind = kind;
    Info.Source = source;
}

auto MCLogInvScreen::ShowComponentInfo(MCCompInventoryBlock* block, bool repairItem) -> void
{
    Info.InfoKind = repairItem ? MCInvInfoBox::Kind::RepairItem : MCInvInfoBox::Kind::Component;
    Info.Source = nullptr;
    Info.ComponentPicture = block->Item->RangeIndex;
    Info.RangeText = block->RangeText;
    Info.DamageText = block->DamageText;
    Info.RecycleText = block->RecycleText;
    Info.Description = block->Item->Description;
}

auto MCLogInvScreen::DrawInfo(MCLogPort* port) -> void
{
    if (Info.Header >= 0)
    {
        MCLogPort* header = LogScreenArt(InvHeaderArt[static_cast<size_t>(Info.Header)]);
        VfxPaneCopy(header->Frame(), 0, 0, port->Frame(), 0xc4, 0x65, -1);
    }

    if (Info.Art < 0)
    {
        return;
    }

    GlobalLogPtr->InventoryIconPorts[Info.Art]->CopyTo(port->Frame(), 2, 0x18a, false);

    switch (Info.InfoKind)
    {
        case MCInvInfoBox::Kind::None:
        {
            break;
        }

        case MCInvInfoBox::Kind::Component:
        case MCInvInfoBox::Kind::RepairItem:
        {
            DrawComponentInfo(Info, port);
            break;
        }

        case MCInvInfoBox::Kind::RepairMech:
        {
            static_cast<MCMechRepairBlock*>(Info.Source)->DrawInfo(port);
            break;
        }

        default:
        {
            static_cast<MCInventoryBlock*>(Info.Source)->DrawInfo(port);
            break;
        }
    }
}

auto MCLogInvScreen::Of(MCGuiObject* screen) -> MCLogInvScreen*
{
    const auto found = std::ranges::find(InvScreens, screen);
    return found != InvScreens.end() ? *found : nullptr;
}

auto MCLogInvScreen::ForgetInfoSource(MCLogObject* source) -> void
{
    for (MCLogInvScreen* screen : InvScreens)
    {
        if (screen->Info.Source == source)
        {
            screen->Info.InfoKind = MCInvInfoBox::Kind::None;
            screen->Info.Source = nullptr;
        }
    }
}
