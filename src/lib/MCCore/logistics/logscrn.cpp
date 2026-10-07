#include "stdafx.h"
#include "logistics/logscrn.h"
#include "gui/afont.h"
#include "gui/scrlpane.h"
#include "lib/MCFatal.h"
#include "linkup/dpplayer.h"
#include "linkup/sessionmanager.h"
#include "logistics/invblock.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "logistics/logpur.h"
#include "logistics/logrep.h"
#include "logistics/logsession.h"
#include "logistics/mrblock.h"
#include "logistics/purchase.h"
#include "main/logistics.h"
#include "network/multplyr.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>The height of a unit block in the unit and store panes.</summary>
    constexpr int32_t UnitBlockHeight = 0x70;
    /// <summary>The height of an inventory block.</summary>
    constexpr int32_t InvBlockHeight = 0x2b;
    /// <summary>The smallest inventory port: the inventory pane's height.</summary>
    constexpr int32_t MinInvPortHeight = 0x10d;
    /// <summary>The width of an inventory port.</summary>
    constexpr int32_t InvPortWidth = 0xab;

    void FreePort(MCLogPort*& port)
    {
        if (port != nullptr)
        {
            delete port;
        }

        port = nullptr;
    }

    /// <summary>Hides and removes every child of <paramref name="pane"/> (the blocks shown in it).</summary>
    void ClearPane(MCScrollPane* pane)
    {
        for (int32_t count = pane->NumberOfChildren(); count > 0; --count)
        {
            pane->Child(0)->ShowGuiWindow(0);
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
    /// <paramref name="color"/> (what the rows' <c>drawBackground</c> painted into the tab's picture).
    /// </summary>
    void DrawStore(MCStoreTab tab, int32_t color, MCGuiPort* port)
    {
        auto* view = static_cast<MCLogPort*>(port);
        VfxPaneWipe(view->Frame(), color);

        auto drawRow = [&](auto* block)
        {
            const int32_t top = block->Row * UnitBlockHeight;

            if (RowShown(view->View, top, UnitBlockHeight))
            {
                block->DrawRow(view, top);
            }
        };

        switch (tab)
        {
            case MCStoreTab::Mechs:
            {
                for (MCPurMech* purMech = GlobalLogPtr->PurMechList->First; purMech != nullptr; purMech = purMech->Next)
                {
                    drawRow(purMech->Block);
                }
                break;
            }

            case MCStoreTab::Vehicles:
            {
                for (MCPurVehicle* purVehicle = GlobalLogPtr->PurVehicleList->First; purVehicle != nullptr;
                     purVehicle = purVehicle->Next)
                {
                    drawRow(purVehicle->Block);
                }
                break;
            }

            case MCStoreTab::Components:
            {
                for (MCLogInventoryItem* item = GlobalLogPtr->PurchaseComponents->Items; item != nullptr;
                     item = item->Next)
                {
                    drawRow(item->PurchaseBlock);
                }
                break;
            }

            case MCStoreTab::Pilots:
            {
                for (MCPurPilotData* pilot = GlobalLogPtr->PurPilotList->First; pilot != nullptr; pilot = pilot->Next)
                {
                    if (pilot->Status == 0)
                    {
                        drawRow(pilot->Block);
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
    MCLogPort* StoreView(MCLogPort*& port, MCStoreTab tab, MCScrollPane* pane, int32_t width, int32_t height,
                         int32_t color)
    {
        if (height < pane->Height())
        {
            height = pane->Height();
        }

        if (port == nullptr)
        {
            port = new MCLogPort;
            port->DrawContent = [tab, color](MCGuiPort* view) { DrawStore(tab, color, view); };
        }

        port->InitView(width, height);
        return port;
    }

    /// <summary>The column headers of the inventory tabs, drawn at (0xc4, 0x65).</summary>
    constexpr const char* InvHeaderArt[4] = {"lscdwm.tga", "lscdwp.tga", "lscdwc.tga", "lscdwv.tga"};

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
                for (MCLogMech* mech = GlobalLogPtr->MechList->Mechs; mech != nullptr; mech = mech->Next)
                {
                    drawRow(mech->InventoryBlock);
                }
                break;
            }

            case 1:
            {
                for (MCLogWarrior* warrior = GlobalLogPtr->WarriorList->Warriors; warrior != nullptr;
                     warrior = warrior->Next)
                {
                    drawRow(warrior->InventoryBlock);
                }
                break;
            }

            case 2:
            {
                for (MCLogInventoryItem* item = GlobalLogPtr->ComponentInventory->Items; item != nullptr;
                     item = item->Next)
                {
                    if (item->InventoryBlock->ListIndex >= 0)
                    {
                        drawRow(item->InventoryBlock);
                    }
                }
                break;
            }

            default:
            {
                for (MCLogVehicle* vehicle = GlobalLogPtr->VehicleList->Vehicles; vehicle != nullptr;
                     vehicle = vehicle->Next)
                {
                    drawRow(vehicle->InventoryBlock);
                }
                break;
            }
        }
    }

    /// <summary>A component's details in the info box (<see cref="MCInvInfoBox"/>).</summary>
    void DrawComponentInfo(MCInvInfoBox& info, MCLogPort* port)
    {
        if (MCLogPort* picture = LogArtf("%slogart\\lscicc%02d.tga", ArtPath, info.ComponentPicture))
        {
            // The repair screen's weapon list copied the picture opaque, the component tab keyed.
            if (info.InfoKind == MCInvInfoBox::Kind::RepairItem)
            {
                VfxPaneCopy(picture->Frame(), 0, 0, port->Frame(), 9, 0x191, -1);
            }
            else
            {
                picture->CopyTo(port->Frame(), 9, 0x191, 1);
            }
        }

        auto write = [&](int32_t y, char* text)
        { YellowDropFont->WriteString(port->Frame(), 0x53, y, reinterpret_cast<uint8_t*>(text), -1); };
        write(0x1a5, info.RangeText);
        write(0x19c, info.DamageText);
        write(0x193, info.RecycleText);

        if (!info.Description.empty())
        {
            DrawInfoDescription(port, 0xc5, 0x26, info.Description.data(), 8, 0x1b3);
        }
    }

    /// <summary>
    /// The port of inventory tab <paramref name="tab"/>, <paramref name="width"/> x <paramref name="height"/>: a view
    /// the tab is drawn into each frame (the original painted each block's row into a new picture).
    /// </summary>
    MCLogPort* NewTabView(int32_t tab, int32_t width, int32_t height)
    {
        // The same view is kept and resized: the original freed the old picture, but a pane could still show it
        // until the tab was set up again.
        MCLogPort* port = GlobalLogPtr->InvTabPorts[tab];

        if (port == nullptr)
        {
            port = new MCLogPort;
            port->DrawContent = [tab](MCGuiPort* view) { DrawInvTab(tab, view); };
        }

        port->InitView(width, height);
        return port;
    }

    /// <summary>The port of inventory tab <paramref name="tab"/> for <paramref name="count"/> blocks (at least the pane's height).</summary>
    MCLogPort* NewInvPort(int32_t tab, int32_t count)
    {
        int32_t height = count * InvBlockHeight;

        if (height < MinInvPortHeight)
        {
            height = MinInvPortHeight;
        }

        return NewTabView(tab, InvPortWidth, height);
    }

    /// <summary>
    /// The inventory tab art common to the <c>setUp*Inv</c> functions: the tab strip (the blank info box of tab
    /// <paramref name="tab"/>) at (2, 0x18a) when <paramref name="redrawTabs"/>, then the tab's column header at
    /// (0xc4, 0x65).
    /// </summary>
    void DrawInvTabArt(MCLogInvScreen* screen, int32_t tab, int redrawTabs)
    {
        if (redrawTabs != 0)
        {
            screen->Info.Blank(tab);
        }

        screen->Info.Header = tab;
    }

    /// <summary>Numbers the store's component blocks: row n goes to the item whose block has sort order n.</summary>
    void ReIndexPass()
    {
        MCLogInventoryItem* first = GlobalLogPtr->PurchaseComponents->Items;
        int32_t row = 0;

        for (int32_t order = 0; order < 50; ++order)
        {
            MCLogInventoryItem* item = first;

            while (item != nullptr && item->PurchaseBlock->SortOrder != order)
            {
                item = item->Next;
            }

            if (item != nullptr)
            {
                item->PurchaseBlock->Row = row++;
            }
        }
    }
}

// LogInvScreen

auto MCLogInvScreen::CreateVehiclePane() -> void
{
    int32_t row = 0;
    MCScrollPane* pane = GlobalLogPtr->RepairScreen->UnitPane;
    int32_t count = GlobalLogPtr->ForceVehicleList->GetVehicleCount() + GlobalLogPtr->ForceMechList->GetMechCount();
    pane->SetDisplayPort(MCRepairScreen::NewUnitRowsView(pane), -1, -1);

    int32_t yPos = 0;

    for (MCLogMech* mech = GlobalLogPtr->ForceMechList->Mechs; mech != nullptr; mech = mech->Next)
    {
        MCMechRepairBlock* block = mech->RepairBlock;
        UnitPane->AddChild(block);
        block->SlotIndex = row;
        block->MoveTo(0, yPos, 0);
        block->ShowGuiWindow(-1);
        block->SetDepth(100);
        block->DrawBackground(row, nullptr);
        ++row;
        yPos += UnitBlockHeight;
    }

    yPos = row * UnitBlockHeight;

    for (MCLogVehicle* vehicle = GlobalLogPtr->ForceVehicleList->Vehicles; vehicle != nullptr; vehicle = vehicle->Next)
    {
        MCVehicleRepairBlock* block = vehicle->RepairBlock;
        UnitPane->AddChild(block);
        block->SlotIndex = row;
        block->MoveTo(0, yPos, 0);
        block->ShowGuiWindow(-1);
        block->SetDepth(100);
        block->DrawBackground(row, nullptr);
        ++row;
        yPos += UnitBlockHeight;
    }
}

auto MCLogInvScreen::CreatePurVehiclePane(int redraw) -> void
{
    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen;
    MCScrollPane* pane = screen->UnitPane;
    int32_t row = 0;

    const int32_t width = pane->Width() - 0xd;

    if (redraw == 0)
    {
        // The store's mechs.
        MCLogPort* port = StoreView(screen->PurMechPort, MCStoreTab::Mechs, pane, width,
                                    GlobalLogPtr->PurMechList->GetMechCount() * UnitBlockHeight, 0x10);
        pane->SetDisplayPort(port, 0, -1);
        MCPurMech* purMech = GlobalLogPtr->PurMechList->First;

        if (GlobalLogPtr->PurMechList->GetMechCount() > 0)
        {
            int32_t yPos = 0;

            do
            {
                MCMechPurchaseBlock* block = purMech->Block;
                block->Row = row;
                block->MoveTo(0, yPos, 0);
                block->ShowGuiWindow(-1);
                block->SetDepth(100);
                block->DrawBackground(row);
                purMech = purMech->Next;
                yPos += UnitBlockHeight;
                ++row;
            } while (row < GlobalLogPtr->PurMechList->GetMechCount());
        }

        // The store's vehicles.
        StoreView(screen->PurVehiclePort, MCStoreTab::Vehicles, pane, width,
                  GlobalLogPtr->PurVehicleList->GetVehicleCount() * UnitBlockHeight, 0x10);
        row = 0;
        int32_t yPos = 0;

        for (MCPurVehicle* purVehicle = GlobalLogPtr->PurVehicleList->First; purVehicle != nullptr;
             purVehicle = purVehicle->Next)
        {
            MCVehiclePurchaseBlock* block = purVehicle->Block;
            block->Row = row;
            block->MoveTo(0, yPos, 0);
            block->ShowGuiWindow(-1);
            block->SetDepth(100);
            block->DrawBackground(row);
            ++row;
            yPos += UnitBlockHeight;
        }

        // The store's components, ordered by reIndexComponents.
        StoreView(screen->PurCompPort, MCStoreTab::Components, pane, width,
                  GlobalLogPtr->PurchaseComponents->NumItems * UnitBlockHeight, 0x10);
        ReIndexComponents();

        for (MCLogInventoryItem* item = GlobalLogPtr->PurchaseComponents->Items; item != nullptr; item = item->Next)
        {
            MCCompPurchaseBlock* block = item->PurchaseBlock;
            block->MoveTo(0, block->Row * UnitBlockHeight, 0);
            block->ShowGuiWindow(-1);
            block->SetDepth(100);
            block->DrawBackground(block->Row, item->PurchaseBlock->Item->MasterID);
        }
    }

    // The pilots for hire (those not yet hired), rebuilt every time.
    // Original behaviour (OB-076): with redraw set, the old pilot port was replaced without being freed (the view is
    // kept and resized).
    int32_t visible = 0;

    for (MCPurPilotData* pilot = GlobalLogPtr->PurPilotList->First; pilot != nullptr; pilot = pilot->Next)
    {
        if (pilot->Status == 0)
        {
            ++visible;
        }
    }

    StoreView(screen->PurPilotPort, MCStoreTab::Pilots, pane, width, visible * UnitBlockHeight, 0xff);
    row = 0;
    int32_t yPos = 0;

    for (MCPurPilotData* pilot = GlobalLogPtr->PurPilotList->First; pilot != nullptr; pilot = pilot->Next)
    {
        if (pilot->Status != 0)
        {
            continue;
        }

        MCPilotPurchaseBlock* block = pilot->Block;
        block->Row = row;
        block->MoveTo(0, yPos, 0);
        block->ShowGuiWindow(-1);
        block->SetDepth(100);
        block->DrawBackground(row);
        ++row;
        yPos += UnitBlockHeight;
    }
}

auto MCLogInvScreen::CreateMechInvBlock() -> void
{
    GlobalLogPtr->InvTabPorts[0] = NewInvPort(0, GlobalLogPtr->MechList->GetMechCount());
    int32_t index = 0;

    for (MCLogMech* mech = GlobalLogPtr->MechList->Mechs; mech != nullptr; mech = mech->Next)
    {
        MCMechInventoryBlock* block = mech->InventoryBlock;
        block->ListIndex = index;
        block->DrawBackground();
        ++index;
    }
}

auto MCLogInvScreen::CreateVhclInvBlock() -> void
{
    MCLogPort* port = NewInvPort(3, GlobalLogPtr->VehicleList->GetVehicleCount());
    GlobalLogPtr->InvTabPorts[3] = port;
    int32_t index = 0;

    for (MCLogVehicle* vehicle = GlobalLogPtr->VehicleList->Vehicles; vehicle != nullptr; vehicle = vehicle->Next)
    {
        MCVehicleInventoryBlock* block = vehicle->InventoryBlock;
        block->ListIndex = index;
        block->DrawBackground();
        ++index;
    }
}

auto MCLogInvScreen::CreatePilotInvBlock() -> void
{
    int32_t height = 0;
    MCLogWarrior* first = GlobalLogPtr->WarriorList->Warriors;

    if (first != nullptr)
    {
        height = first->InventoryBlock->Height() * GlobalLogPtr->WarriorList->NumWarriors;
    }

    if (height < InventoryPane->Height())
    {
        height = InventoryPane->Height();
    }

    MCLogPort* port = NewTabView(1, InventoryPane->Width() - 0xd, height);
    GlobalLogPtr->InvTabPorts[1] = port;
    int32_t index = 0;

    for (MCLogWarrior* warrior = first; warrior != nullptr; warrior = warrior->Next)
    {
        MCPilotInventoryBlock* block = warrior->InventoryBlock;
        block->ListIndex = index;
        ++index;
        block->DrawBackground();
    }
}

auto MCLogInvScreen::DrawBlankInvInfoBlock(int32_t tab) -> void
{
    if (tab < 0)
    {
        tab = GlobalLogPtr->CurrentInvTab;
    }

    if (tab >= 0 && tab <= 3)
    {
        if (MCLogInvScreen* screen = Of(GlobalLogPtr->CurrentScreen))
        {
            screen->Info.Blank(tab);
        }
        else
        {
            GlobalLogPtr->InventoryIconPorts[tab]->CopyTo(GlobalLogPtr->CurrentScreen->Lport()->Frame(), 2, 0x18a, 0);
        }
    }
}

auto MCLogInvScreen::CreateCompInvBlock() -> void
{
    MCLogPort* port = NewInvPort(2, GlobalLogPtr->ReIndexInventory());
    GlobalLogPtr->InvTabPorts[2] = port;

    for (MCLogInventoryItem* item = GlobalLogPtr->ComponentInventory->Items; item != nullptr; item = item->Next)
    {
        item->InventoryBlock->DrawBackground();
    }
}

auto MCLogInvScreen::SetUpMechInv(int scrollPos, int redrawTabs) -> void
{
    GlobalLogPtr->CurrentInvTab = 0;
    DrawInvTabArt(this, 0, redrawTabs);
    ClearPane(GlobalLogPtr->RepairScreen->InventoryPane);
    ClearPane(GlobalLogPtr->PurchaseScreen->InventoryPane);

    for (MCLogMech* mech = GlobalLogPtr->MechList->Mechs; mech != nullptr; mech = mech->Next)
    {
        MCMechInventoryBlock* block = mech->InventoryBlock;
        InventoryPane->AddChild(block);
        block->ShowGuiWindow(-1);
        block->BringToFront(0);
        block->MoveTo(0, block->ListIndex * InvBlockHeight, 0);
    }

    GlobalLogPtr->PurchaseScreen->InventoryPane->SetDisplayPort(GlobalLogPtr->InvTabPorts[0], 0, scrollPos);
    GlobalLogPtr->RepairScreen->InventoryPane->SetDisplayPort(GlobalLogPtr->InvTabPorts[0], 0, scrollPos);
}

auto MCLogInvScreen::SetUpMechPurchase() -> void
{
    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen;

    if (GlobalLogPtr->CurrentScreen != screen)
    {
        return;
    }

    ClearPane(screen->UnitPane);
    screen->UnitPane->SetDisplayPort(screen->PurMechPort, 0, -1);
    int32_t row = 0;

    for (MCPurMech* purMech = GlobalLogPtr->PurMechList->First; purMech != nullptr; purMech = purMech->Next)
    {
        MCMechPurchaseBlock* block = purMech->Block;
        UnitPane->AddChild(block);
        block->ShowGuiWindow(-1);
        block->MoveTo(0, block->Height() * row, 0);
        block->BringToFront(0);
        // Show the first variant still on sale.
        int32_t variant;

        if (purMech->Variants[0]->NumAvailable != 0)
        {
            variant = 0;
        }
        else if (purMech->Variants[1]->NumAvailable != 0)
        {
            variant = 1;
        }
        else
        {
            variant = purMech->Variants[2]->NumAvailable != 0 ? 2 : 0;
        }

        if (variant != block->CurVariant)
        {
            block->CurVariant = variant;
            block->DrawBackground(row);
        }

        ++row;
    }
}

auto MCLogInvScreen::SetUpPilotInv(int scrollPos, int redrawTabs) -> void
{
    GlobalLogPtr->CurrentInvTab = 1;
    DrawInvTabArt(this, 1, redrawTabs);
    ClearPane(GlobalLogPtr->RepairScreen->InventoryPane);
    ClearPane(GlobalLogPtr->PurchaseScreen->InventoryPane);
    int32_t yPos = 0;

    for (MCLogWarrior* warrior = GlobalLogPtr->WarriorList->Warriors; warrior != nullptr; warrior = warrior->Next)
    {
        // The assigned pilots come last and aren't shown.
        if (warrior->Assigned != 0)
        {
            break;
        }

        MCPilotInventoryBlock* block = warrior->InventoryBlock;
        InventoryPane->AddChild(block);
        block->ShowGuiWindow(-1);
        block->BringToFront(0);
        block->DrawBackground();

        if (scrollPos != 0)
        {
            block->MoveTo(0, yPos, 0);
        }

        yPos += InvBlockHeight;
    }

    GlobalLogPtr->PurchaseScreen->InventoryPane->SetDisplayPort(GlobalLogPtr->InvTabPorts[1], 0, scrollPos);
    GlobalLogPtr->RepairScreen->InventoryPane->SetDisplayPort(GlobalLogPtr->InvTabPorts[1], 0, scrollPos);
}

auto MCLogInvScreen::SetUpCompInv(int scrollPos, int redrawTabs) -> void
{
    GlobalLogPtr->CurrentInvTab = 2;
    DrawInvTabArt(this, 2, redrawTabs);
    ClearPane(GlobalLogPtr->RepairScreen->InventoryPane);
    ClearPane(GlobalLogPtr->PurchaseScreen->InventoryPane);

    for (MCLogInventoryItem* item = GlobalLogPtr->ComponentInventory->Items; item != nullptr; item = item->Next)
    {
        MCCompInventoryBlock* block = item->InventoryBlock;

        if (block->ListIndex < 0)
        {
            continue;
        }

        InventoryPane->AddChild(block);
        block->ShowGuiWindow(-1);
        block->BringToFront(0);
        block->MoveTo(0, block->ListIndex * InvBlockHeight, 0);
        block->DrawBackground();
    }

    GlobalLogPtr->PurchaseScreen->InventoryPane->SetDisplayPort(GlobalLogPtr->InvTabPorts[2], 0, scrollPos);
    GlobalLogPtr->RepairScreen->InventoryPane->SetDisplayPort(GlobalLogPtr->InvTabPorts[2], 0, scrollPos);
}

auto MCLogInvScreen::SetUpVhclInv(int scrollPos, int redrawTabs) -> void
{
    GlobalLogPtr->CurrentInvTab = 3;
    DrawInvTabArt(this, 3, redrawTabs);
    ClearPane(GlobalLogPtr->RepairScreen->InventoryPane);
    ClearPane(GlobalLogPtr->PurchaseScreen->InventoryPane);
    int32_t yPos = 0;

    for (MCLogVehicle* vehicle = GlobalLogPtr->VehicleList->Vehicles; vehicle != nullptr; vehicle = vehicle->Next)
    {
        MCVehicleInventoryBlock* block = vehicle->InventoryBlock;
        InventoryPane->AddChild(block);
        block->ShowGuiWindow(-1);
        block->BringToFront(0);
        block->MoveTo(0, yPos, 0);
        yPos += InvBlockHeight;
    }

    GlobalLogPtr->PurchaseScreen->InventoryPane->SetDisplayPort(GlobalLogPtr->InvTabPorts[3], 0, scrollPos);
    GlobalLogPtr->RepairScreen->InventoryPane->SetDisplayPort(GlobalLogPtr->InvTabPorts[3], 0, scrollPos);
}

auto MCLogInvScreen::SetUpVehiclePurchase() -> void
{
    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen;

    if (GlobalLogPtr->CurrentScreen != screen)
    {
        return;
    }

    ClearPane(screen->UnitPane);
    int32_t row = 0;

    for (MCPurVehicle* purVehicle = GlobalLogPtr->PurVehicleList->First; purVehicle != nullptr;
         purVehicle = purVehicle->Next)
    {
        MCVehiclePurchaseBlock* block = purVehicle->Block;
        UnitPane->AddChild(block);
        block->ShowGuiWindow(-1);
        block->MoveTo(0, block->Height() * row, 0);
        block->BringToFront(0);
        ++row;
    }

    screen->UnitPane->SetDisplayPort(screen->PurVehiclePort, 0, -1);
}

auto MCLogInvScreen::SetUpPilotPurchase() -> void
{
    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen;

    if (GlobalLogPtr->CurrentScreen != screen)
    {
        return;
    }

    ClearPane(screen->UnitPane);
    int32_t row = 0;

    for (MCPurPilotData* pilot = GlobalLogPtr->PurPilotList->First; pilot != nullptr; pilot = pilot->Next)
    {
        if (pilot->Status != 0)
        {
            continue;
        }

        MCPilotPurchaseBlock* block = pilot->Block;
        UnitPane->AddChild(block);
        block->ShowGuiWindow(-1);
        block->MoveTo(0, block->Height() * row, 0);
        block->BringToFront(0);
        ++row;
    }

    screen->UnitPane->SetDisplayPort(screen->PurPilotPort, 0, -1);
}

auto MCLogInvScreen::ReIndexComponents() -> void
{
    // The original runs the same pass twice.
    ReIndexPass();
    ReIndexPass();
}

auto MCLogInvScreen::SetUpCompPurchase() -> void
{
    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen;

    if (GlobalLogPtr->CurrentScreen != screen)
    {
        return;
    }

    ClearPane(screen->UnitPane);
    ReIndexComponents();

    for (MCLogInventoryItem* item = GlobalLogPtr->PurchaseComponents->Items; item != nullptr; item = item->Next)
    {
        MCCompPurchaseBlock* block = item->PurchaseBlock;
        UnitPane->AddChild(block);
        block->ShowGuiWindow(-1);
        block->MoveTo(0, block->Height() * block->Row, 0);
        block->BringToFront(0);
    }

    screen->UnitPane->SetDisplayPort(screen->PurCompPort, 0, -1);
}

auto MCLogInvScreen::RemovePilot(int32_t pilotIndex) -> void
{
    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen;
    MCScrollPane* pane = screen->UnitPane;
    int32_t count = GlobalLogPtr->PurPilotList->GetVisiblePilotCount();
    // The original copied the old rows into a new picture, closing the gap: the rows below the removed one moved up
    // a block, and the rest was wiped. The view draws the remaining pilots at their new rows.
    MCLogPort* port =
        StoreView(screen->PurPilotPort, MCStoreTab::Pilots, pane, pane->Width() - 0x10, count * UnitBlockHeight, 0xff);
    screen->UnitPane->SetDisplayPort(port, -1, -1);

    // Move the blocks from the removed one on up a row.
    for (MCPurPilotData* pilot = GlobalLogPtr->PurPilotList->First; pilot != nullptr; pilot = pilot->Next)
    {
        if (pilotIndex <= pilot->Block->Row)
        {
            int32_t yPos = pilotIndex * UnitBlockHeight;

            for (; pilot != nullptr; pilot = pilot->Next)
            {
                MCPilotPurchaseBlock* block = pilot->Block;
                yPos += UnitBlockHeight;
                block->Row = pilotIndex;
                ++pilotIndex;
                block->MoveTo(0, yPos, 0);
            }
            break;
        }
    }

    int32_t row = 0;

    for (MCPurPilotData* pilot = GlobalLogPtr->PurPilotList->First; pilot != nullptr; pilot = pilot->Next)
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
        if (MCLogPort* art = LogArtf("%slogart\\%s", ArtPath, BackgroundArt))
        {
            VfxPaneCopy(art->Frame(), 0, 0, Lport()->Frame(), 0, 0, -1);
        }

        DrawInfo(Lport());
        GlobalLogPtr->DrawScreenChrome(this, Lport()->Frame());
    }

    MCLogObject::Draw();
}

auto MCLogInvScreen::InitLive(const char* artName) -> void
{
    BackgroundArt = artName;
    InvScreens.push_back(this);
}

MCLogInvScreen::~MCLogInvScreen()
{
    std::erase(InvScreens, this);
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
    std::memcpy(Info.RangeText, block->RangeText, sizeof(Info.RangeText));
    std::memcpy(Info.DamageText, block->DamageText, sizeof(Info.DamageText));
    std::memcpy(Info.RecycleText, block->RecycleText, sizeof(Info.RecycleText));
    Info.Description = block->Item->Description != nullptr ? block->Item->Description : "";
}

auto MCLogInvScreen::DrawInfo(MCLogPort* port) -> void
{
    if (Info.Header >= 0)
    {
        if (MCLogPort* header = LogArtf("%slogart\\%s", ArtPath, InvHeaderArt[Info.Header]))
        {
            VfxPaneCopy(header->Frame(), 0, 0, port->Frame(), 0xc4, 0x65, -1);
        }
    }

    if (Info.Art < 0)
    {
        return;
    }

    GlobalLogPtr->InventoryIconPorts[Info.Art]->CopyTo(port->Frame(), 2, 0x18a, 0);

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
    for (MCLogInvScreen* invScreen : InvScreens)
    {
        if (invScreen == screen)
        {
            return invScreen;
        }
    }

    return nullptr;
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

// LogChatWindow

auto MCLogChatWindow::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, int32_t historySize) -> void
{
    // The original wiped its picture to the key and pasted the frame (lsbdw04) along the bottom; draw shows the frame.
    MCLogObject::Init(xPos, yPos, width, height, nullptr, nullptr);
    SetTransparent(-1);
    this->HistorySize = historySize;

    char fileName[256];
    FramePort = new MCLogPort;
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsbdw04.tga", ArtPath);
    FramePort->Init(fileName);

    auto* pane = new MCScrollPane;

    if (pane != nullptr)
    {
        pane->Init();
    }

    HistoryPane = pane;
    Assert(pane != nullptr, 0, "Not enough memory for chat scroll");
    pane->Init(0xb8, height - FramePort->Height() - 7, 6, 6, static_cast<char*>(nullptr));
    AddChild(pane);
    pane->ShowGuiWindow(-1);

    // The history (wiped to 0x10, then written along the bottom as lines come) is drawn from lines.
    Lines.clear();
    pane->SetDisplayPort(NewHistoryView(pane->Lport()->Width(), historySize / pane->Lport()->Width()), -1, -1);
    pane->SetScrollPos(100.0f);

    ChatInput = new MCLogChatInput;
    ChatInput->Init(6, height - 0x21, 0xb8, 0x1a, nullptr);
    AddChild(ChatInput);
    ChatInput->ShowGuiWindow(-1);
}

MCLogChatWindow::~MCLogChatWindow()
{
    Destroy();
}

auto MCLogChatWindow::ShowGuiWindow(int show) -> void
{
    ShowWindow = show;
}

auto MCLogChatWindow::Destroy() -> void
{
    delete HistoryPane;
    HistoryPane = nullptr;
    delete ChatInput;
    ChatInput = nullptr;
    FreePort(FramePort);
    MCLogObject::Destroy();
}

auto MCLogChatWindow::HandleNetworkMessage(uint32_t fromPlayerId, void* message) -> void
{
    auto* bytes = static_cast<char*>(message);
    // Team messages are in colour 6, messages to all in 4.
    ProcessChatString(fromPlayerId, bytes + 9, bytes[8] != 0 ? 6 : 4);
}

auto MCLogChatWindow::ProcessChatString(uint32_t fromPlayerId, char* string, int32_t textColor) -> void
{
    const char* name = "?";

    if (fromPlayerId != 0)
    {
        name = MPlayer->SessionManager->GetPlayer(fromPlayerId)->Name;
    }

    if (textColor == -1)
    {
        textColor = 6;
    }

    MCFidpPlayer* player = MPlayer->SessionManager->GetPlayer(fromPlayerId);
    // Port fix: a sender no longer in the session (the original read its player number through null) takes player 0's colour.
    int32_t playerNumber = player != nullptr ? player->PlayerNumber : 0;
    char line[2048];
    std::snprintf(line, sizeof(line), "%%fc%d%s: %%fc%d%s", GlobalLogPtr->PlayerColors[playerNumber], name, textColor,
                  string);
    AddLine(line);
}

auto MCLogChatWindow::AddLine(const char* line) -> void
{
    // The original moved the history picture up by the text's height, wiped the strip along the bottom and wrote the
    // text there; the history keeps the line and draws it so each frame.
    MCScrollPane* pane = HistoryPane;
    std::string text = line;
    const int32_t used =
        Application->TextFormatter.Process(reinterpret_cast<uint8_t*>(text.data()), nullptr, pane->Lport()->Width(), 0);
    Lines.push_back(HistoryLine{std::move(text), used});

    // A line whose strip moved off the top shows nothing any more.
    int32_t above = 0;
    size_t first = Lines.size();

    while (first > 0 && above < pane->Lport()->Height())
    {
        first--;
        above += Lines[first].Used;
    }

    Lines.erase(Lines.begin(), Lines.begin() + static_cast<std::ptrdiff_t>(first));
}

auto MCLogChatWindow::DrawHistory(MCGuiPort* port, const std::vector<HistoryLine>& lines) -> void
{
    MCPane* frame = port->Frame();
    const int32_t portWidth = port->Width();
    const int32_t portHeight = port->Height();
    VfxPaneWipe(frame, 0x10);

    // How far each line moved up: the heights of the lines after it.
    int32_t moved = 0;

    for (const HistoryLine& line : lines)
    {
        moved += line.Used;
    }

    const MCRect scissor = port->View.Scissor;

    for (const HistoryLine& line : lines)
    {
        moved -= line.Used;
        const int32_t bottom = portHeight - 1 - moved;
        const int32_t top = bottom - line.Used;

        // The picture ended at its last row when the line was written: nothing of it lies below that.
        port->View.Scissor.Y1 = std::min(scissor.Y1, port->View.OriginY + bottom);

        if (port->View.Open())
        {
            MCPane strip = *frame;
            strip.X0 = 0;
            strip.Y0 = top;
            strip.X1 = portWidth - 1;
            strip.Y1 = bottom;
            VfxPaneWipe(&strip, 0x10);
            std::string text = line.Text;
            Application->TextFormatter.Process(reinterpret_cast<uint8_t*>(text.data()), port, 0, top);
        }

        port->View.Scissor = scissor;
    }
}

auto MCLogChatWindow::NewHistoryView(int32_t width, int32_t height) -> MCLogPort*
{
    auto* view = new MCLogPort;
    view->InitView(width, height);
    view->DrawContent = [this](MCGuiPort* port) { DrawHistory(port, Lines); };
    return view;
}

auto MCLogChatWindow::Draw() -> void
{
    if (Lport()->ViewOpen())
    {
        FramePort->CopyTo(Lport()->Frame(), 0, Height() - FramePort->Height(), -1);
    }

    MCLogObject::Draw();
}

auto MCLogChatWindow::HandleEvent(MCGuiEvent* event) -> void
{
    // The original fetches x() and y() here and drops them.
    X();
    Y();

    if (event->Type == 9 && Parent != nullptr)
    {
        Parent->HandleEvent(event);
    }
}

auto MCLogChatWindow::Resize(int32_t height) -> void
{
    // The original wiped its picture and pasted the frame at the new bottom (draw shows it there).
    MCLogObject::Resize(Width(), height);
    ChatInput->MoveTo(6, height - 0x21, 0);

    // Keep the history across the new pane (the original copied its picture into a new one).
    MCLogPort* oldHistory = HistoryPane->ContentPort;
    MCLogPort* history = NewHistoryView(oldHistory->Width(), oldHistory->Height());
    delete HistoryPane;

    auto* pane = new MCScrollPane;

    if (pane != nullptr)
    {
        pane->Init();
    }

    HistoryPane = pane;
    Assert(pane != nullptr, 0, "Not enough memory for chat scroll");
    pane->Init(0xb8, height - FramePort->Height() - 7, 6, 6, static_cast<char*>(nullptr));
    pane->SetDisplayPort(history, -1, -1);
    AddChild(pane);
    HistoryPane->SetScrollPos(100.0f);
    HistoryPane->ShowGuiWindow(-1);
}

auto MCLogChatWindow::Reset() -> void
{
    MCScrollPane* pane = HistoryPane;
    MCLogPort* oldHistory = pane->ContentPort;
    Lines.clear();
    pane->SetDisplayPort(NewHistoryView(oldHistory->Width(), oldHistory->Height()), -1, -1);
    ChatInput->Text[0] = 0;
}
