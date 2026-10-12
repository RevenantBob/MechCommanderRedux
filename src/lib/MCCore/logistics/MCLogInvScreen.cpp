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
    void ClearPane(MCScrollPane& pane)
    {
        for (int32_t count = pane.NumberOfChildren(); count > 0; --count)
        {
            pane.Child(0)->ShowGuiWindow(false);
            pane.RemoveChild(*(pane.Child(0)));
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
    void DrawStore(MCStoreTab tab, int32_t color, MCGuiPort& port)
    {
        MCLogPort& view = static_cast<MCLogPort&>(port);
        VfxPaneWipe(view.Frame(), static_cast<uint32_t>(color));

        auto drawRow = [&](auto& block)
        {
            const int32_t top = block.Row * MCLogInvScreen::UnitBlockHeight;

            if (RowShown(view.View(), top, MCLogInvScreen::UnitBlockHeight))
            {
                block.DrawRow(view, top);
            }
        };

        switch (tab)
        {
            case MCStoreTab::Mechs:
            {
                for (const auto& purMech : GlobalLog()->PurMechList->Mechs)
                {
                    drawRow(*purMech->Block);
                }
                break;
            }

            case MCStoreTab::Vehicles:
            {
                for (const auto& purVehicle : GlobalLog()->PurVehicleList->Vehicles)
                {
                    drawRow(*purVehicle->Block);
                }
                break;
            }

            case MCStoreTab::Components:
            {
                for (const std::shared_ptr<MCLogInventoryItem>& item : GlobalLog()->PurchaseComponents->Items)
                {
                    drawRow(*item->PurchaseBlock);
                }
                break;
            }

            case MCStoreTab::Pilots:
            {
                for (const auto& pilot : GlobalLog()->PurPilotList->Pilots)
                {
                    if (pilot->Status == 0)
                    {
                        drawRow(*pilot->Block);
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
    std::shared_ptr<MCLogPort> StoreView(std::shared_ptr<MCLogPort>& port, MCStoreTab tab, MCScrollPane& pane,
                                         int32_t width, int32_t height, int32_t color)
    {
        if (port == nullptr)
        {
            port = std::make_shared<MCLogPort>();
            port->DrawContent = [tab, color](MCGuiPort& view) { DrawStore(tab, color, view); };
        }

        port->InitView(width, std::max(height, pane.Height()));
        return port;
    }

    /// <summary>The column headers of the inventory tabs, drawn at (0xc4, 0x65).</summary>
    constexpr std::array<std::string_view, 4> InvHeaderArt = {"lscdwm.tga", "lscdwp.tga", "lscdwc.tga", "lscdwv.tga"};

    /// <summary>The inventory screens (the purchase and repair screens), for <see cref="MCLogInvScreen::Of"/>.</summary>
    std::vector<std::weak_ptr<MCLogInvScreen>> InvScreens;

    /// <summary>
    /// Draws inventory tab <paramref name="tab"/> into its view <paramref name="port"/>: the rows its blocks drew into
    /// the tab's picture, each at its list row, over colour 0x10.
    /// </summary>
    void DrawInvTab(int32_t tab, MCGuiPort& port)
    {
        MCLogPort& view = static_cast<MCLogPort&>(port);
        VfxPaneWipe(view.Frame(), 0x10);
        const MCView& place = view.View();

        auto drawRow = [&](MCInventoryBlock& block)
        {
            const int32_t top = block.ListIndex * block.WinHeight;

            if (RowShown(place, top, block.WinHeight))
            {
                block.DrawRow(view, top);
            }
        };

        switch (tab)
        {
            case 0:
            {
                for (const std::shared_ptr<MCLogMech>& mech : GlobalLog()->MechList->Mechs)
                {
                    drawRow(*mech->InventoryBlock);
                }
                break;
            }

            case 1:
            {
                for (const std::shared_ptr<MCLogWarrior>& warrior : GlobalLog()->WarriorList->Warriors)
                {
                    drawRow(*warrior->InventoryBlock);
                }
                break;
            }

            case 2:
            {
                for (const std::shared_ptr<MCLogInventoryItem>& item : GlobalLog()->ComponentInventory->Items)
                {
                    if (item->InventoryBlock->ListIndex >= 0)
                    {
                        drawRow(*item->InventoryBlock);
                    }
                }
                break;
            }

            default:
            {
                for (const std::shared_ptr<MCLogVehicle>& vehicle : GlobalLog()->VehicleList->Vehicles)
                {
                    drawRow(*vehicle->InventoryBlock);
                }
                break;
            }
        }
    }

    /// <summary>A component's details in the info box (<see cref="MCInvInfoBox"/>).</summary>
    void DrawComponentInfo(MCInvInfoBox& info, MCLogPort& port)
    {
        const std::shared_ptr<MCLogPort> picture = LogScreenArt(std::format("lscicc{:02}.tga", info.ComponentPicture));

        // The repair screen's weapon list copied the picture opaque, the component tab keyed.
        if (info.InfoKind == MCInvInfoBox::Kind::RepairItem)
        {
            VfxPaneCopy(picture->Frame(), 0, 0, port.Frame(), 9, 0x191, -1);
        }
        else
        {
            picture->CopyTo(port.Frame(), 9, 0x191, true);
        }

        auto write = [&](int32_t y, const std::string& text)
        { YellowDropFont->WriteString(port.Frame(), 0x53, y, text); };
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
    std::shared_ptr<MCLogPort> NewTabView(int32_t tab, int32_t width, int32_t height)
    {
        std::shared_ptr<MCLogPort>& port = GlobalLog()->InvTabPorts[static_cast<size_t>(tab)];

        if (port == nullptr)
        {
            port = std::make_shared<MCLogPort>();
            port->DrawContent = [tab](MCGuiPort& view) { DrawInvTab(tab, view); };
        }

        port->InitView(width, height);
        return port;
    }

    /// <summary>The view of inventory tab <paramref name="tab"/> for <paramref name="count"/> blocks (at least the pane's height).</summary>
    std::shared_ptr<MCLogPort> NewInvPort(int32_t tab, int32_t count)
    {
        return NewTabView(tab, InvPortWidth, std::max(count * MCLogInvScreen::InvBlockHeight, MinInvPortHeight));
    }

    /// <summary>
    /// The inventory tab art common to the SetUp*Inv functions: the blank info box of tab <paramref name="tab"/>
    /// when <paramref name="redrawTabs"/>, then the tab's column header.
    /// </summary>
    void DrawInvTabArt(MCLogInvScreen& screen, int32_t tab, bool redrawTabs)
    {
        if (redrawTabs)
        {
            screen.Info.Blank(tab);
        }

        screen.Info.Header = tab;
    }

    /// <summary>Shows inventory tab <paramref name="tab"/>'s view in both screens' inventory panes.</summary>
    void ShowInvTab(int32_t tab, bool resetScroll)
    {
        const std::shared_ptr<MCLogPort> view = GlobalLog()->InvTabPorts[static_cast<size_t>(tab)];
        GlobalLog()->PurchaseScreen->InventoryPane->SetDisplayPort(view, resetScroll);
        GlobalLog()->RepairScreen->InventoryPane->SetDisplayPort(view, resetScroll);
    }

    /// <summary>Takes the blocks out of both screens' inventory panes.</summary>
    void ClearInventoryPanes()
    {
        ClearPane(*GlobalLog()->RepairScreen->InventoryPane);
        ClearPane(*GlobalLog()->PurchaseScreen->InventoryPane);
    }

    /// <summary>Numbers the store's component blocks: row n goes to the item whose block has sort order n.</summary>
    void ReIndexPass()
    {
        const auto& items = GlobalLog()->PurchaseComponents->Items;
        int32_t row = 0;

        // Only sort orders 0..49 get a row, as in the original.
        for (int32_t order = 0; order < 50; ++order)
        {
            const auto item = std::ranges::find_if(items, [&](const std::shared_ptr<MCLogInventoryItem>& entry)
                                                   { return entry->PurchaseBlock->SortOrder == order; });

            if (item != items.end())
            {
                (*item)->PurchaseBlock->Row = row++;
            }
        }
    }

    /// <summary>Places a store block at row <paramref name="row"/> and shows it.</summary>
    template <typename T> void PlaceStoreBlock(T& block, int32_t row)
    {
        block.MoveTo(0, row * MCLogInvScreen::UnitBlockHeight, false);
        block.ShowGuiWindow(true);
        block.SetDepth(100);
    }
}

// MCLogInvScreen

MCLogInvScreen::~MCLogInvScreen()
{
    // This screen's entry has expired by now.
    std::erase_if(InvScreens, [](const std::weak_ptr<MCLogInvScreen>& screen) { return screen.expired(); });
}

auto MCLogInvScreen::MakePanes(bool emptyUnitPane) -> void
{
    InventoryPane = MCMakeGui<MCScrollPane>();
    InventoryPane->Init(0xb8, 0x10d, 8, 0x6b, std::nullopt);
    InventoryPane->ClearDisplayPort();

    UnitPane = MCMakeGui<MCScrollPane>();
    UnitPane->Init(0x1aa, 0x1cc, 0xd3, 0x11, std::format("{}logart\\lsrbk01.tga", ArtPath).c_str());

    if (emptyUnitPane)
    {
        UnitPane->ClearDisplayPort();
    }

    AddChild(InventoryPane->weak_from_this());
    AddChild(UnitPane->weak_from_this());
}

auto MCLogInvScreen::FreePanes() -> void
{
    // The panes show the logistics screen's views, or the screen's own store views.
    for (MCGuiOwned<MCScrollPane>& pane : {std::ref(InventoryPane), std::ref(UnitPane)})
    {
        if (pane != nullptr)
        {
            pane->ClearDisplayPort();
            pane.reset();
        }
    }
}

auto MCLogInvScreen::CreateVehiclePane() -> void
{
    std::shared_ptr<MCScrollPane> pane = GlobalLog()->RepairScreen->UnitPane;
    pane->SetDisplayPort(MCRepairScreen::NewUnitRowsView(*pane), true);
    int32_t row = 0;

    auto place = [&](auto& block)
    {
        UnitPane->AddChild(block.weak_from_this());
        block.SlotIndex = row;
        block.MoveTo(0, row * UnitBlockHeight, false);
        block.ShowGuiWindow(true);
        block.SetDepth(100);
        block.DrawBackground(row, std::nullopt);
        ++row;
    };

    for (const std::shared_ptr<MCLogMech>& mech : GlobalLog()->ForceMechList->Mechs)
    {
        place(*mech->RepairBlock);
    }

    for (const std::shared_ptr<MCLogVehicle>& vehicle : GlobalLog()->ForceVehicleList->Vehicles)
    {
        place(*vehicle->RepairBlock);
    }
}

auto MCLogInvScreen::CreatePurVehiclePane(bool pilotsOnly) -> void
{
    MCPurchaseScreen& screen = *GlobalLog()->PurchaseScreen;
    std::shared_ptr<MCScrollPane> pane = screen.UnitPane;
    const int32_t width = pane->Width() - 0xd;

    if (!pilotsOnly)
    {
        // The store's mechs.
        pane->SetDisplayPort(StoreView(screen.PurMechPort, MCStoreTab::Mechs, *pane, width,
                                       GlobalLog()->PurMechList->GetMechCount() * UnitBlockHeight, 0x10),
                             true);
        int32_t row = 0;

        for (const auto& purMech : GlobalLog()->PurMechList->Mechs)
        {
            MCMechPurchaseBlock& block = *purMech->Block;
            block.Row = row;
            PlaceStoreBlock(block, row);
            block.DrawBackground(row);
            ++row;
        }

        // The store's vehicles.
        StoreView(screen.PurVehiclePort, MCStoreTab::Vehicles, *pane, width,
                  GlobalLog()->PurVehicleList->GetVehicleCount() * UnitBlockHeight, 0x10);
        row = 0;

        for (const auto& purVehicle : GlobalLog()->PurVehicleList->Vehicles)
        {
            MCVehiclePurchaseBlock& block = *purVehicle->Block;
            block.Row = row;
            PlaceStoreBlock(block, row);
            block.DrawBackground(row);
            ++row;
        }

        // The store's components, ordered by ReIndexComponents.
        StoreView(screen.PurCompPort, MCStoreTab::Components, *pane, width,
                  GlobalLog()->PurchaseComponents->NumItems() * UnitBlockHeight, 0x10);
        ReIndexComponents();

        for (const std::shared_ptr<MCLogInventoryItem>& item : GlobalLog()->PurchaseComponents->Items)
        {
            MCCompPurchaseBlock& block = *item->PurchaseBlock;
            PlaceStoreBlock(block, block.Row);
            block.DrawBackground(block.Row, item->MasterID);
        }
    }

    // The pilots for hire (those not yet hired), rebuilt every time. Original behaviour (OB-076): when only the
    // pilots were rebuilt, the old pilot picture was replaced without being freed (the view is kept and resized).
    int32_t visible = 0;

    for (const auto& pilot : GlobalLog()->PurPilotList->Pilots)
    {
        if (pilot->Status == 0)
        {
            ++visible;
        }
    }

    StoreView(screen.PurPilotPort, MCStoreTab::Pilots, *pane, width, visible * UnitBlockHeight, 0xff);
    int32_t row = 0;

    for (const auto& pilot : GlobalLog()->PurPilotList->Pilots)
    {
        if (pilot->Status != 0)
        {
            continue;
        }

        MCPilotPurchaseBlock& block = *pilot->Block;
        block.Row = row;
        PlaceStoreBlock(block, row);
        block.DrawBackground(row);
        ++row;
    }
}

auto MCLogInvScreen::CreateMechInvBlock() -> void
{
    NewInvPort(0, GlobalLog()->MechList->GetMechCount());
    int32_t index = 0;

    for (const std::shared_ptr<MCLogMech>& mech : GlobalLog()->MechList->Mechs)
    {
        MCMechInventoryBlock& block = *mech->InventoryBlock;
        block.ListIndex = index++;
        block.DrawBackground();
    }
}

auto MCLogInvScreen::CreateVhclInvBlock() -> void
{
    NewInvPort(3, GlobalLog()->VehicleList->GetVehicleCount());
    int32_t index = 0;

    for (const std::shared_ptr<MCLogVehicle>& vehicle : GlobalLog()->VehicleList->Vehicles)
    {
        MCVehicleInventoryBlock& block = *vehicle->InventoryBlock;
        block.ListIndex = index++;
        block.DrawBackground();
    }
}

auto MCLogInvScreen::CreatePilotInvBlock() const -> void
{
    const auto& warriors = GlobalLog()->WarriorList->Warriors;
    const int32_t height =
        warriors.empty() ? 0 : warriors.front()->InventoryBlock->Height() * GlobalLog()->WarriorList->GetWarriorCount();
    NewTabView(1, InventoryPane->Width() - 0xd, std::max(height, InventoryPane->Height()));
    int32_t index = 0;

    for (const std::shared_ptr<MCLogWarrior>& warrior : warriors)
    {
        MCPilotInventoryBlock& block = *warrior->InventoryBlock;
        block.ListIndex = index++;
        block.DrawBackground();
    }
}

auto MCLogInvScreen::DrawBlankInvInfoBlock(int32_t tab) -> void
{
    if (tab < 0)
    {
        tab = GlobalLog()->CurrentInvTab;
    }

    if (tab < 0 || tab > 3)
    {
        return;
    }

    if (std::shared_ptr<MCLogInvScreen> screen = Of(*GlobalLog()->CurrentScreen()))
    {
        screen->Info.Blank(tab);
    }
    else
    {
        GlobalLog()->InventoryIconPorts[tab]->CopyTo(GlobalLog()->CurrentScreen()->Lport()->Frame(), 2, 0x18a, false);
    }
}

auto MCLogInvScreen::CreateCompInvBlock() -> void
{
    NewInvPort(2, GlobalLog()->ReIndexInventory());

    for (const std::shared_ptr<MCLogInventoryItem>& item : GlobalLog()->ComponentInventory->Items)
    {
        item->InventoryBlock->DrawBackground();
    }
}

auto MCLogInvScreen::SetUpMechInv(bool resetScroll, bool redrawTabs) -> void
{
    GlobalLog()->CurrentInvTab = 0;
    DrawInvTabArt(*this, 0, redrawTabs);
    ClearInventoryPanes();

    for (const std::shared_ptr<MCLogMech>& mech : GlobalLog()->MechList->Mechs)
    {
        MCMechInventoryBlock& block = *mech->InventoryBlock;
        InventoryPane->AddChild(block.weak_from_this());
        block.ShowGuiWindow(true);
        block.BringToFront(false);
        block.MoveTo(0, block.ListIndex * InvBlockHeight, false);
    }

    ShowInvTab(0, resetScroll);
}

auto MCLogInvScreen::SetUpMechPurchase() const -> void
{
    MCPurchaseScreen& screen = *GlobalLog()->PurchaseScreen;

    if (GlobalLog()->CurrentScreen().get() != &screen)
    {
        return;
    }

    ClearPane(*screen.UnitPane);
    screen.UnitPane->SetDisplayPort(screen.PurMechPort, true);
    int32_t row = 0;

    for (const auto& purMech : GlobalLog()->PurMechList->Mechs)
    {
        MCMechPurchaseBlock& block = *purMech->Block;
        UnitPane->AddChild(block.weak_from_this());
        block.ShowGuiWindow(true);
        block.MoveTo(0, block.Height() * row, false);
        block.BringToFront(false);
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

        if (variant != block.CurVariant)
        {
            block.CurVariant = variant;
            block.DrawBackground(row);
        }

        ++row;
    }
}

auto MCLogInvScreen::SetUpPilotInv(bool resetScroll, bool redrawTabs) -> void
{
    GlobalLog()->CurrentInvTab = 1;
    DrawInvTabArt(*this, 1, redrawTabs);
    ClearInventoryPanes();
    int32_t yPos = 0;

    for (const std::shared_ptr<MCLogWarrior>& warrior : GlobalLog()->WarriorList->Warriors)
    {
        // The assigned pilots come last and aren't shown.
        if (warrior->Assigned != 0)
        {
            break;
        }

        MCPilotInventoryBlock& block = *warrior->InventoryBlock;
        InventoryPane->AddChild(block.weak_from_this());
        block.ShowGuiWindow(true);
        block.BringToFront(false);
        block.DrawBackground();

        if (resetScroll)
        {
            block.MoveTo(0, yPos, false);
        }

        yPos += InvBlockHeight;
    }

    ShowInvTab(1, resetScroll);
}

auto MCLogInvScreen::SetUpCompInv(bool resetScroll, bool redrawTabs) -> void
{
    GlobalLog()->CurrentInvTab = 2;
    DrawInvTabArt(*this, 2, redrawTabs);
    ClearInventoryPanes();

    for (const std::shared_ptr<MCLogInventoryItem>& item : GlobalLog()->ComponentInventory->Items)
    {
        MCCompInventoryBlock& block = *item->InventoryBlock;

        if (block.ListIndex < 0)
        {
            continue;
        }

        InventoryPane->AddChild(block.weak_from_this());
        block.ShowGuiWindow(true);
        block.BringToFront(false);
        block.MoveTo(0, block.ListIndex * InvBlockHeight, false);
        block.DrawBackground();
    }

    ShowInvTab(2, resetScroll);
}

auto MCLogInvScreen::SetUpVhclInv(bool resetScroll, bool redrawTabs) -> void
{
    GlobalLog()->CurrentInvTab = 3;
    DrawInvTabArt(*this, 3, redrawTabs);
    ClearInventoryPanes();
    int32_t yPos = 0;

    for (const std::shared_ptr<MCLogVehicle>& vehicle : GlobalLog()->VehicleList->Vehicles)
    {
        MCVehicleInventoryBlock& block = *vehicle->InventoryBlock;
        InventoryPane->AddChild(block.weak_from_this());
        block.ShowGuiWindow(true);
        block.BringToFront(false);
        block.MoveTo(0, yPos, false);
        yPos += InvBlockHeight;
    }

    ShowInvTab(3, resetScroll);
}

auto MCLogInvScreen::SetUpVehiclePurchase() const -> void
{
    MCPurchaseScreen& screen = *GlobalLog()->PurchaseScreen;

    if (GlobalLog()->CurrentScreen().get() != &screen)
    {
        return;
    }

    ClearPane(*screen.UnitPane);
    int32_t row = 0;

    for (const auto& purVehicle : GlobalLog()->PurVehicleList->Vehicles)
    {
        MCVehiclePurchaseBlock& block = *purVehicle->Block;
        UnitPane->AddChild(block.weak_from_this());
        block.ShowGuiWindow(true);
        block.MoveTo(0, block.Height() * row, false);
        block.BringToFront(false);
        ++row;
    }

    screen.UnitPane->SetDisplayPort(screen.PurVehiclePort, true);
}

auto MCLogInvScreen::SetUpPilotPurchase() const -> void
{
    MCPurchaseScreen& screen = *GlobalLog()->PurchaseScreen;

    if (GlobalLog()->CurrentScreen().get() != &screen)
    {
        return;
    }

    ClearPane(*screen.UnitPane);
    int32_t row = 0;

    for (const auto& pilot : GlobalLog()->PurPilotList->Pilots)
    {
        if (pilot->Status != 0)
        {
            continue;
        }

        MCPilotPurchaseBlock& block = *pilot->Block;
        UnitPane->AddChild(block.weak_from_this());
        block.ShowGuiWindow(true);
        block.MoveTo(0, block.Height() * row, false);
        block.BringToFront(false);
        ++row;
    }

    screen.UnitPane->SetDisplayPort(screen.PurPilotPort, true);
}

auto MCLogInvScreen::ReIndexComponents() -> void
{
    // The original runs the same pass twice.
    ReIndexPass();
    ReIndexPass();
}

auto MCLogInvScreen::SetUpCompPurchase() const -> void
{
    MCPurchaseScreen& screen = *GlobalLog()->PurchaseScreen;

    if (GlobalLog()->CurrentScreen().get() != &screen)
    {
        return;
    }

    ClearPane(*screen.UnitPane);
    ReIndexComponents();

    for (const std::shared_ptr<MCLogInventoryItem>& item : GlobalLog()->PurchaseComponents->Items)
    {
        MCCompPurchaseBlock& block = *item->PurchaseBlock;
        UnitPane->AddChild(block.weak_from_this());
        block.ShowGuiWindow(true);
        block.MoveTo(0, block.Height() * block.Row, false);
        block.BringToFront(false);
    }

    screen.UnitPane->SetDisplayPort(screen.PurCompPort, true);
}

auto MCLogInvScreen::RemovePilot(int32_t pilotIndex) -> void
{
    MCPurchaseScreen& screen = *GlobalLog()->PurchaseScreen;
    std::shared_ptr<MCScrollPane> pane = screen.UnitPane;
    const int32_t count = GlobalLog()->PurPilotList->GetVisiblePilotCount();
    // The original copied the old rows into a new picture, closing the gap: the rows below the removed one moved up
    // a block, and the rest was wiped. The view draws the remaining pilots at their new rows.
    const std::shared_ptr<MCLogPort> port =
        StoreView(screen.PurPilotPort, MCStoreTab::Pilots, *pane, pane->Width() - 0x10, count * UnitBlockHeight, 0xff);
    screen.UnitPane->SetDisplayPort(port, true);

    // Move the blocks from the removed one on up a row.
    const auto& pilots = GlobalLog()->PurPilotList->Pilots;

    for (size_t index = 0; index < pilots.size(); ++index)
    {
        if (pilotIndex <= pilots[index]->Block->Row)
        {
            int32_t yPos = pilotIndex * UnitBlockHeight;

            for (; index < pilots.size(); ++index)
            {
                MCPilotPurchaseBlock& block = *pilots[index]->Block;
                yPos += UnitBlockHeight;
                block.Row = pilotIndex;
                ++pilotIndex;
                block.MoveTo(0, yPos, false);
            }
            break;
        }
    }

    int32_t row = 0;

    for (const auto& pilot : GlobalLog()->PurPilotList->Pilots)
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
        DrawInfo(*Lport());
        GlobalLog()->DrawScreenChrome(*this, Lport()->Frame());
    }

    MCLogObject::Draw();
}

auto MCLogInvScreen::InitLive(std::string_view artName) -> void
{
    BackgroundArt = artName;
    InvScreens.push_back(std::static_pointer_cast<MCLogInvScreen>(shared_from_this()));
}

auto MCLogInvScreen::ShowInfo(MCInvInfoBox::Kind kind, std::weak_ptr<MCGuiObject> source) -> void
{
    Info.InfoKind = kind;
    Info.Source = std::move(source);
}

auto MCLogInvScreen::ShowComponentInfo(const MCCompInventoryBlock& block, bool repairItem) -> void
{
    const std::shared_ptr<MCLogInventoryItem> item = block.Item();
    Info.InfoKind = repairItem ? MCInvInfoBox::Kind::RepairItem : MCInvInfoBox::Kind::Component;
    Info.Source.reset();
    Info.ComponentPicture = item->RangeIndex;
    Info.RangeText = block.RangeText;
    Info.DamageText = block.DamageText;
    Info.RecycleText = block.RecycleText;
    Info.Description = item->Description;
}

auto MCLogInvScreen::DrawInfo(MCLogPort& port) -> void
{
    if (Info.Header >= 0)
    {
        const std::shared_ptr<MCLogPort> header = LogScreenArt(InvHeaderArt[static_cast<size_t>(Info.Header)]);
        VfxPaneCopy(header->Frame(), 0, 0, port.Frame(), 0xc4, 0x65, -1);
    }

    if (Info.Art < 0)
    {
        return;
    }

    GlobalLog()->InventoryIconPorts[Info.Art]->CopyTo(port.Frame(), 2, 0x18a, false);

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
            if (const std::shared_ptr<MCGuiObject> source = Info.Source.lock())
            {
                static_cast<MCMechRepairBlock&>(*source).DrawInfo(port);
            }

            break;
        }

        default:
        {
            if (const std::shared_ptr<MCGuiObject> source = Info.Source.lock())
            {
                static_cast<MCInventoryBlock&>(*source).DrawInfo(port);
            }

            break;
        }
    }
}

auto MCLogInvScreen::Of(const MCGuiObject& screen) -> std::shared_ptr<MCLogInvScreen>
{
    for (const std::weak_ptr<MCLogInvScreen>& entry : InvScreens)
    {
        if (MCRefersTo(entry, screen))
        {
            return entry.lock();
        }
    }

    return nullptr;
}

auto MCLogInvScreen::ForgetInfoSource(const MCLogObject& source) -> void
{
    for (const std::weak_ptr<MCLogInvScreen>& entry : InvScreens)
    {
        const std::shared_ptr<MCLogInvScreen> screen = entry.lock();

        if (screen != nullptr && MCRefersTo(screen->Info.Source, source))
        {
            screen->Info.InfoKind = MCInvInfoBox::Kind::None;
            screen->Info.Source.reset();
        }
    }
}
