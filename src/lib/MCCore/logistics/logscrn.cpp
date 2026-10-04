#include "stdafx.h"
#include "logistics/logscrn.h"
#include "gui/afont.h"
#include "gui/scrlpane.h"
#include "lib/aerror.h"
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
#include "vfx/vfxfuncs.h"

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

    void freePort(lPort*& port)
    {
        if (port != nullptr)
        {
            delete port;
        }

        port = nullptr;
    }

    /// <summary>Hides and removes every child of <paramref name="pane"/> (the blocks shown in it).</summary>
    void clearPane(ScrollPane* pane)
    {
        for (int32_t count = pane->numberOfChildren(); count > 0; --count)
        {
            pane->child(0)->ShowGUIWindow(0);
            pane->removeChild(pane->child(0));
        }
    }

    /// <summary>Whether a row from <paramref name="top"/> of <paramref name="height"/> lines meets the open view's scissor.</summary>
    bool rowShown(const MCView& place, int32_t top, int32_t height)
    {
        const int32_t screenTop = place.OriginY + top;
        return screenTop <= place.Scissor.Y1 && place.Scissor.Y0 < screenTop + height;
    }

    /// <summary>The store's tabs, as <see cref="drawStore"/> draws them.</summary>
    enum class StoreTab
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
    void drawStore(StoreTab tab, int32_t color, aPort* port)
    {
        auto* view = static_cast<lPort*>(port);
        VFX_pane_wipe(view->frame(), color);

        auto drawRow = [&](auto* block)
        {
            const int32_t top = block->row * UnitBlockHeight;

            if (rowShown(view->view, top, UnitBlockHeight))
            {
                block->DrawRow(view, top);
            }
        };

        switch (tab)
        {
            case StoreTab::Mechs:
            {
                for (PurMech* purMech = globalLogPtr->purMechList->first; purMech != nullptr; purMech = purMech->next)
                {
                    drawRow(purMech->block);
                }
                break;
            }

            case StoreTab::Vehicles:
            {
                for (PurVehicle* purVehicle = globalLogPtr->purVehicleList->first; purVehicle != nullptr;
                     purVehicle = purVehicle->next)
                {
                    drawRow(purVehicle->block);
                }
                break;
            }

            case StoreTab::Components:
            {
                for (_LogInventoryItem* item = globalLogPtr->purchaseComponents->items; item != nullptr;
                     item = item->next)
                {
                    drawRow(item->purchaseBlock);
                }
                break;
            }

            case StoreTab::Pilots:
            {
                for (PurPilotData* pilot = globalLogPtr->purPilotList->first; pilot != nullptr; pilot = pilot->next)
                {
                    if (pilot->status == 0)
                    {
                        drawRow(pilot->block);
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
    lPort* storeView(lPort*& port, StoreTab tab, ScrollPane* pane, int32_t width, int32_t height, int32_t color)
    {
        if (height < pane->height())
        {
            height = pane->height();
        }

        if (port == nullptr)
        {
            port = new lPort;
            port->DrawContent = [tab, color](aPort* view) { drawStore(tab, color, view); };
        }

        port->initView(width, height);
        return port;
    }

    /// <summary>The column headers of the inventory tabs, drawn at (0xc4, 0x65).</summary>
    constexpr const char* InvHeaderArt[4] = {"lscdwm.tga", "lscdwp.tga", "lscdwc.tga", "lscdwv.tga"};

    /// <summary>The inventory screens (the purchase and repair screens), for <see cref="LogInvScreen::Of"/>.</summary>
    std::vector<LogInvScreen*> invScreens;

    /// <summary>
    /// Draws inventory tab <paramref name="tab"/> into its view <paramref name="port"/>: the rows its blocks drew into
    /// the tab's picture, each at its list row, over colour 0x10.
    /// </summary>
    void drawInvTab(int32_t tab, aPort* port)
    {
        auto* view = static_cast<lPort*>(port);
        VFX_pane_wipe(view->frame(), 0x10);
        const MCView& place = view->view;

        auto drawRow = [&](InventoryBlock* block)
        {
            const int32_t top = block->listIndex * block->winHeight;

            if (rowShown(place, top, block->winHeight))
            {
                block->DrawRow(view, top);
            }
        };

        switch (tab)
        {
            case 0:
            {
                for (LogMech* mech = globalLogPtr->mechList->mechs; mech != nullptr; mech = mech->next)
                {
                    drawRow(mech->inventoryBlock);
                }
                break;
            }

            case 1:
            {
                for (LogWarrior* warrior = globalLogPtr->warriorList->warriors; warrior != nullptr;
                     warrior = warrior->next)
                {
                    drawRow(warrior->inventoryBlock);
                }
                break;
            }

            case 2:
            {
                for (_LogInventoryItem* item = globalLogPtr->componentInventory->items; item != nullptr;
                     item = item->next)
                {
                    if (item->inventoryBlock->listIndex >= 0)
                    {
                        drawRow(item->inventoryBlock);
                    }
                }
                break;
            }

            default:
            {
                for (LogVehicle* vehicle = globalLogPtr->vehicleList->vehicles; vehicle != nullptr;
                     vehicle = vehicle->next)
                {
                    drawRow(vehicle->inventoryBlock);
                }
                break;
            }
        }
    }

    /// <summary>A component's details in the info box (<see cref="InvInfoBox"/>).</summary>
    void drawComponentInfo(InvInfoBox& info, lPort* port)
    {
        if (lPort* picture = logArtf("%slogart\\lscicc%02d.tga", artPath, info.componentPicture))
        {
            // The repair screen's weapon list copied the picture opaque, the component tab keyed.
            if (info.kind == InvInfoBox::Kind::RepairItem)
            {
                VFX_pane_copy(picture->frame(), 0, 0, port->frame(), 9, 0x191, -1);
            }
            else
            {
                picture->copyTo(port->frame(), 9, 0x191, 1);
            }
        }

        auto write = [&](int32_t y, char* text)
        { yellowDropFont->writeString(port->frame(), 0x53, y, reinterpret_cast<uint8_t*>(text), -1); };
        write(0x1a5, info.rangeText);
        write(0x19c, info.damageText);
        write(0x193, info.recycleText);

        if (!info.description.empty())
        {
            DrawInfoDescription(port, 0xc5, 0x26, info.description.data(), 8, 0x1b3);
        }
    }

    /// <summary>
    /// The port of inventory tab <paramref name="tab"/>, <paramref name="width"/> x <paramref name="height"/>: a view
    /// the tab is drawn into each frame (the original painted each block's row into a new picture).
    /// </summary>
    lPort* newTabView(int32_t tab, int32_t width, int32_t height)
    {
        // The same view is kept and resized: the original freed the old picture, but a pane could still show it
        // until the tab was set up again.
        lPort* port = globalLogPtr->invTabPorts[tab];

        if (port == nullptr)
        {
            port = new lPort;
            port->DrawContent = [tab](aPort* view) { drawInvTab(tab, view); };
        }

        port->initView(width, height);
        return port;
    }

    /// <summary>The port of inventory tab <paramref name="tab"/> for <paramref name="count"/> blocks (at least the pane's height).</summary>
    lPort* newInvPort(int32_t tab, int32_t count)
    {
        int32_t height = count * InvBlockHeight;

        if (height < MinInvPortHeight)
        {
            height = MinInvPortHeight;
        }

        return newTabView(tab, InvPortWidth, height);
    }

    /// <summary>
    /// The inventory tab art common to the <c>setUp*Inv</c> functions: the tab strip (the blank info box of tab
    /// <paramref name="tab"/>) at (2, 0x18a) when <paramref name="redrawTabs"/>, then the tab's column header at
    /// (0xc4, 0x65).
    /// </summary>
    void drawInvTabArt(LogInvScreen* screen, int32_t tab, int redrawTabs)
    {
        if (redrawTabs != 0)
        {
            screen->info.Blank(tab);
        }

        screen->info.header = tab;
    }

    /// <summary>Numbers the store's component blocks: row n goes to the item whose block has sort order n.</summary>
    void reIndexPass()
    {
        _LogInventoryItem* first = globalLogPtr->purchaseComponents->items;
        int32_t row = 0;

        for (int32_t order = 0; order < 50; ++order)
        {
            _LogInventoryItem* item = first;

            while (item != nullptr && item->purchaseBlock->sortOrder != order)
            {
                item = item->next;
            }

            if (item != nullptr)
            {
                item->purchaseBlock->row = row++;
            }
        }
    }
}

// LogInvScreen

auto LogInvScreen::createVehiclePane() -> void
{
    int32_t row = 0;
    ScrollPane* pane = globalLogPtr->repairScreen->unitPane;
    int32_t count = globalLogPtr->forceVehicleList->getVehicleCount() + globalLogPtr->forceMechList->getMechCount();
    pane->setDisplayPort(RepairScreen::NewUnitRowsView(pane), -1, -1);

    int32_t yPos = 0;

    for (LogMech* mech = globalLogPtr->forceMechList->mechs; mech != nullptr; mech = mech->next)
    {
        MechRepairBlock* block = mech->repairBlock;
        unitPane->addChild(block);
        block->slotIndex = row;
        block->moveTo(0, yPos, 0);
        block->ShowGUIWindow(-1);
        block->setDepth(100);
        block->drawBackground(row, nullptr);
        ++row;
        yPos += UnitBlockHeight;
    }

    yPos = row * UnitBlockHeight;

    for (LogVehicle* vehicle = globalLogPtr->forceVehicleList->vehicles; vehicle != nullptr; vehicle = vehicle->next)
    {
        VehicleRepairBlock* block = vehicle->repairBlock;
        unitPane->addChild(block);
        block->slotIndex = row;
        block->moveTo(0, yPos, 0);
        block->ShowGUIWindow(-1);
        block->setDepth(100);
        block->drawBackground(row, nullptr);
        ++row;
        yPos += UnitBlockHeight;
    }
}

auto LogInvScreen::createPurVehiclePane(int redraw) -> void
{
    PurchaseScreen* screen = globalLogPtr->purchaseScreen;
    ScrollPane* pane = screen->unitPane;
    int32_t row = 0;

    const int32_t width = pane->width() - 0xd;

    if (redraw == 0)
    {
        // The store's mechs.
        lPort* port = storeView(screen->purMechPort, StoreTab::Mechs, pane, width,
                                globalLogPtr->purMechList->getMechCount() * UnitBlockHeight, 0x10);
        pane->setDisplayPort(port, 0, -1);
        PurMech* purMech = globalLogPtr->purMechList->first;

        if (globalLogPtr->purMechList->getMechCount() > 0)
        {
            int32_t yPos = 0;

            do
            {
                MechPurchaseBlock* block = purMech->block;
                block->row = row;
                block->moveTo(0, yPos, 0);
                block->ShowGUIWindow(-1);
                block->setDepth(100);
                block->drawBackground(row);
                purMech = purMech->next;
                yPos += UnitBlockHeight;
                ++row;
            } while (row < globalLogPtr->purMechList->getMechCount());
        }

        // The store's vehicles.
        storeView(screen->purVehiclePort, StoreTab::Vehicles, pane, width,
                  globalLogPtr->purVehicleList->getVehicleCount() * UnitBlockHeight, 0x10);
        row = 0;
        int32_t yPos = 0;

        for (PurVehicle* purVehicle = globalLogPtr->purVehicleList->first; purVehicle != nullptr;
             purVehicle = purVehicle->next)
        {
            VehiclePurchaseBlock* block = purVehicle->block;
            block->row = row;
            block->moveTo(0, yPos, 0);
            block->ShowGUIWindow(-1);
            block->setDepth(100);
            block->drawBackground(row);
            ++row;
            yPos += UnitBlockHeight;
        }

        // The store's components, ordered by reIndexComponents.
        storeView(screen->purCompPort, StoreTab::Components, pane, width,
                  globalLogPtr->purchaseComponents->numItems * UnitBlockHeight, 0x10);
        reIndexComponents();

        for (_LogInventoryItem* item = globalLogPtr->purchaseComponents->items; item != nullptr; item = item->next)
        {
            CompPurchaseBlock* block = item->purchaseBlock;
            block->moveTo(0, block->row * UnitBlockHeight, 0);
            block->ShowGUIWindow(-1);
            block->setDepth(100);
            block->drawBackground(block->row, item->purchaseBlock->item->masterID);
        }
    }

    // The pilots for hire (those not yet hired), rebuilt every time.
    // Original behaviour (OB-076): with redraw set, the old pilot port was replaced without being freed (the view is
    // kept and resized).
    int32_t visible = 0;

    for (PurPilotData* pilot = globalLogPtr->purPilotList->first; pilot != nullptr; pilot = pilot->next)
    {
        if (pilot->status == 0)
        {
            ++visible;
        }
    }

    storeView(screen->purPilotPort, StoreTab::Pilots, pane, width, visible * UnitBlockHeight, 0xff);
    row = 0;
    int32_t yPos = 0;

    for (PurPilotData* pilot = globalLogPtr->purPilotList->first; pilot != nullptr; pilot = pilot->next)
    {
        if (pilot->status != 0)
        {
            continue;
        }

        PilotPurchaseBlock* block = pilot->block;
        block->row = row;
        block->moveTo(0, yPos, 0);
        block->ShowGUIWindow(-1);
        block->setDepth(100);
        block->drawBackground(row);
        ++row;
        yPos += UnitBlockHeight;
    }
}

auto LogInvScreen::createMechInvBlock() -> void
{
    globalLogPtr->invTabPorts[0] = newInvPort(0, globalLogPtr->mechList->getMechCount());
    int32_t index = 0;

    for (LogMech* mech = globalLogPtr->mechList->mechs; mech != nullptr; mech = mech->next)
    {
        MechInventoryBlock* block = mech->inventoryBlock;
        block->listIndex = index;
        block->drawBackground();
        ++index;
    }
}

auto LogInvScreen::createVhclInvBlock() -> void
{
    lPort* port = newInvPort(3, globalLogPtr->vehicleList->getVehicleCount());
    globalLogPtr->invTabPorts[3] = port;
    int32_t index = 0;

    for (LogVehicle* vehicle = globalLogPtr->vehicleList->vehicles; vehicle != nullptr; vehicle = vehicle->next)
    {
        VehicleInventoryBlock* block = vehicle->inventoryBlock;
        block->listIndex = index;
        block->drawBackground();
        ++index;
    }
}

auto LogInvScreen::createPilotInvBlock() -> void
{
    int32_t height = 0;
    LogWarrior* first = globalLogPtr->warriorList->warriors;

    if (first != nullptr)
    {
        height = first->inventoryBlock->height() * globalLogPtr->warriorList->numWarriors;
    }

    if (height < inventoryPane->height())
    {
        height = inventoryPane->height();
    }

    lPort* port = newTabView(1, inventoryPane->width() - 0xd, height);
    globalLogPtr->invTabPorts[1] = port;
    int32_t index = 0;

    for (LogWarrior* warrior = first; warrior != nullptr; warrior = warrior->next)
    {
        PilotInventoryBlock* block = warrior->inventoryBlock;
        block->listIndex = index;
        ++index;
        block->drawBackground();
    }
}

auto LogInvScreen::drawBlankInvInfoBlock(int32_t tab) -> void
{
    if (tab < 0)
    {
        tab = globalLogPtr->currentInvTab;
    }

    if (tab >= 0 && tab <= 3)
    {
        if (LogInvScreen* screen = Of(globalLogPtr->currentScreen))
        {
            screen->info.Blank(tab);
        }
        else
        {
            globalLogPtr->inventoryIconPorts[tab]->copyTo(globalLogPtr->currentScreen->lport()->frame(), 2, 0x18a, 0);
        }
    }
}

auto LogInvScreen::createCompInvBlock() -> void
{
    lPort* port = newInvPort(2, globalLogPtr->reIndexInventory());
    globalLogPtr->invTabPorts[2] = port;

    for (_LogInventoryItem* item = globalLogPtr->componentInventory->items; item != nullptr; item = item->next)
    {
        item->inventoryBlock->drawBackground();
    }
}

auto LogInvScreen::setUpMechInv(int scrollPos, int redrawTabs) -> void
{
    globalLogPtr->currentInvTab = 0;
    drawInvTabArt(this, 0, redrawTabs);
    clearPane(globalLogPtr->repairScreen->inventoryPane);
    clearPane(globalLogPtr->purchaseScreen->inventoryPane);

    for (LogMech* mech = globalLogPtr->mechList->mechs; mech != nullptr; mech = mech->next)
    {
        MechInventoryBlock* block = mech->inventoryBlock;
        inventoryPane->addChild(block);
        block->ShowGUIWindow(-1);
        block->bringToFront(0);
        block->moveTo(0, block->listIndex * InvBlockHeight, 0);
    }

    globalLogPtr->purchaseScreen->inventoryPane->setDisplayPort(globalLogPtr->invTabPorts[0], 0, scrollPos);
    globalLogPtr->repairScreen->inventoryPane->setDisplayPort(globalLogPtr->invTabPorts[0], 0, scrollPos);
}

auto LogInvScreen::setUpMechPurchase() -> void
{
    PurchaseScreen* screen = globalLogPtr->purchaseScreen;

    if (globalLogPtr->currentScreen != screen)
    {
        return;
    }

    clearPane(screen->unitPane);
    screen->unitPane->setDisplayPort(screen->purMechPort, 0, -1);
    int32_t row = 0;

    for (PurMech* purMech = globalLogPtr->purMechList->first; purMech != nullptr; purMech = purMech->next)
    {
        MechPurchaseBlock* block = purMech->block;
        unitPane->addChild(block);
        block->ShowGUIWindow(-1);
        block->moveTo(0, block->height() * row, 0);
        block->bringToFront(0);
        // Show the first variant still on sale.
        int32_t variant;

        if (purMech->variants[0]->numAvailable != 0)
        {
            variant = 0;
        }
        else if (purMech->variants[1]->numAvailable != 0)
        {
            variant = 1;
        }
        else
        {
            variant = purMech->variants[2]->numAvailable != 0 ? 2 : 0;
        }

        if (variant != block->curVariant)
        {
            block->curVariant = variant;
            block->drawBackground(row);
        }

        ++row;
    }
}

auto LogInvScreen::setUpPilotInv(int scrollPos, int redrawTabs) -> void
{
    globalLogPtr->currentInvTab = 1;
    drawInvTabArt(this, 1, redrawTabs);
    clearPane(globalLogPtr->repairScreen->inventoryPane);
    clearPane(globalLogPtr->purchaseScreen->inventoryPane);
    int32_t yPos = 0;

    for (LogWarrior* warrior = globalLogPtr->warriorList->warriors; warrior != nullptr; warrior = warrior->next)
    {
        // The assigned pilots come last and aren't shown.
        if (warrior->assigned != 0)
        {
            break;
        }

        PilotInventoryBlock* block = warrior->inventoryBlock;
        inventoryPane->addChild(block);
        block->ShowGUIWindow(-1);
        block->bringToFront(0);
        block->drawBackground();

        if (scrollPos != 0)
        {
            block->moveTo(0, yPos, 0);
        }

        yPos += InvBlockHeight;
    }

    globalLogPtr->purchaseScreen->inventoryPane->setDisplayPort(globalLogPtr->invTabPorts[1], 0, scrollPos);
    globalLogPtr->repairScreen->inventoryPane->setDisplayPort(globalLogPtr->invTabPorts[1], 0, scrollPos);
}

auto LogInvScreen::setUpCompInv(int scrollPos, int redrawTabs) -> void
{
    globalLogPtr->currentInvTab = 2;
    drawInvTabArt(this, 2, redrawTabs);
    clearPane(globalLogPtr->repairScreen->inventoryPane);
    clearPane(globalLogPtr->purchaseScreen->inventoryPane);

    for (_LogInventoryItem* item = globalLogPtr->componentInventory->items; item != nullptr; item = item->next)
    {
        CompInventoryBlock* block = item->inventoryBlock;

        if (block->listIndex < 0)
        {
            continue;
        }

        inventoryPane->addChild(block);
        block->ShowGUIWindow(-1);
        block->bringToFront(0);
        block->moveTo(0, block->listIndex * InvBlockHeight, 0);
        block->drawBackground();
    }

    globalLogPtr->purchaseScreen->inventoryPane->setDisplayPort(globalLogPtr->invTabPorts[2], 0, scrollPos);
    globalLogPtr->repairScreen->inventoryPane->setDisplayPort(globalLogPtr->invTabPorts[2], 0, scrollPos);
}

auto LogInvScreen::setUpVhclInv(int scrollPos, int redrawTabs) -> void
{
    globalLogPtr->currentInvTab = 3;
    drawInvTabArt(this, 3, redrawTabs);
    clearPane(globalLogPtr->repairScreen->inventoryPane);
    clearPane(globalLogPtr->purchaseScreen->inventoryPane);
    int32_t yPos = 0;

    for (LogVehicle* vehicle = globalLogPtr->vehicleList->vehicles; vehicle != nullptr; vehicle = vehicle->next)
    {
        VehicleInventoryBlock* block = vehicle->inventoryBlock;
        inventoryPane->addChild(block);
        block->ShowGUIWindow(-1);
        block->bringToFront(0);
        block->moveTo(0, yPos, 0);
        yPos += InvBlockHeight;
    }

    globalLogPtr->purchaseScreen->inventoryPane->setDisplayPort(globalLogPtr->invTabPorts[3], 0, scrollPos);
    globalLogPtr->repairScreen->inventoryPane->setDisplayPort(globalLogPtr->invTabPorts[3], 0, scrollPos);
}

auto LogInvScreen::setUpVehiclePurchase() -> void
{
    PurchaseScreen* screen = globalLogPtr->purchaseScreen;

    if (globalLogPtr->currentScreen != screen)
    {
        return;
    }

    clearPane(screen->unitPane);
    int32_t row = 0;

    for (PurVehicle* purVehicle = globalLogPtr->purVehicleList->first; purVehicle != nullptr;
         purVehicle = purVehicle->next)
    {
        VehiclePurchaseBlock* block = purVehicle->block;
        unitPane->addChild(block);
        block->ShowGUIWindow(-1);
        block->moveTo(0, block->height() * row, 0);
        block->bringToFront(0);
        ++row;
    }

    screen->unitPane->setDisplayPort(screen->purVehiclePort, 0, -1);
}

auto LogInvScreen::setUpPilotPurchase() -> void
{
    PurchaseScreen* screen = globalLogPtr->purchaseScreen;

    if (globalLogPtr->currentScreen != screen)
    {
        return;
    }

    clearPane(screen->unitPane);
    int32_t row = 0;

    for (PurPilotData* pilot = globalLogPtr->purPilotList->first; pilot != nullptr; pilot = pilot->next)
    {
        if (pilot->status != 0)
        {
            continue;
        }

        PilotPurchaseBlock* block = pilot->block;
        unitPane->addChild(block);
        block->ShowGUIWindow(-1);
        block->moveTo(0, block->height() * row, 0);
        block->bringToFront(0);
        ++row;
    }

    screen->unitPane->setDisplayPort(screen->purPilotPort, 0, -1);
}

auto LogInvScreen::reIndexComponents() -> void
{
    // The original runs the same pass twice.
    reIndexPass();
    reIndexPass();
}

auto LogInvScreen::setUpCompPurchase() -> void
{
    PurchaseScreen* screen = globalLogPtr->purchaseScreen;

    if (globalLogPtr->currentScreen != screen)
    {
        return;
    }

    clearPane(screen->unitPane);
    reIndexComponents();

    for (_LogInventoryItem* item = globalLogPtr->purchaseComponents->items; item != nullptr; item = item->next)
    {
        CompPurchaseBlock* block = item->purchaseBlock;
        unitPane->addChild(block);
        block->ShowGUIWindow(-1);
        block->moveTo(0, block->height() * block->row, 0);
        block->bringToFront(0);
    }

    screen->unitPane->setDisplayPort(screen->purCompPort, 0, -1);
}

auto LogInvScreen::removePilot(int32_t pilotIndex) -> void
{
    PurchaseScreen* screen = globalLogPtr->purchaseScreen;
    ScrollPane* pane = screen->unitPane;
    int32_t count = globalLogPtr->purPilotList->getVisiblePilotCount();
    // The original copied the old rows into a new picture, closing the gap: the rows below the removed one moved up
    // a block, and the rest was wiped. The view draws the remaining pilots at their new rows.
    lPort* port =
        storeView(screen->purPilotPort, StoreTab::Pilots, pane, pane->width() - 0x10, count * UnitBlockHeight, 0xff);
    screen->unitPane->setDisplayPort(port, -1, -1);

    // Move the blocks from the removed one on up a row.
    for (PurPilotData* pilot = globalLogPtr->purPilotList->first; pilot != nullptr; pilot = pilot->next)
    {
        if (pilotIndex <= pilot->block->row)
        {
            int32_t yPos = pilotIndex * UnitBlockHeight;

            for (; pilot != nullptr; pilot = pilot->next)
            {
                PilotPurchaseBlock* block = pilot->block;
                yPos += UnitBlockHeight;
                block->row = pilotIndex;
                ++pilotIndex;
                block->moveTo(0, yPos, 0);
            }
            break;
        }
    }

    int32_t row = 0;

    for (PurPilotData* pilot = globalLogPtr->purPilotList->first; pilot != nullptr; pilot = pilot->next)
    {
        if (pilot->status == 0)
        {
            pilot->block->row = row++;
        }
    }
}

auto LogInvScreen::draw() -> void
{
    if (lport()->viewOpen())
    {
        if (lPort* art = logArtf("%slogart\\%s", artPath, backgroundArt))
        {
            VFX_pane_copy(art->frame(), 0, 0, lport()->frame(), 0, 0, -1);
        }

        DrawInfo(lport());
        globalLogPtr->drawScreenChrome(this, lport()->frame());
    }

    lObject::draw();
}

auto LogInvScreen::initLive(const char* artName) -> void
{
    backgroundArt = artName;
    invScreens.push_back(this);
}

LogInvScreen::~LogInvScreen()
{
    std::erase(invScreens, this);
}

auto LogInvScreen::ShowInfo(InvInfoBox::Kind kind, lObject* source) -> void
{
    info.kind = kind;
    info.source = source;
}

auto LogInvScreen::ShowComponentInfo(CompInventoryBlock* block, bool repairItem) -> void
{
    info.kind = repairItem ? InvInfoBox::Kind::RepairItem : InvInfoBox::Kind::Component;
    info.source = nullptr;
    info.componentPicture = block->item->rangeIndex;
    std::memcpy(info.rangeText, block->rangeText, sizeof(info.rangeText));
    std::memcpy(info.damageText, block->damageText, sizeof(info.damageText));
    std::memcpy(info.recycleText, block->recycleText, sizeof(info.recycleText));
    info.description = block->item->description != nullptr ? block->item->description : "";
}

auto LogInvScreen::DrawInfo(lPort* port) -> void
{
    if (info.header >= 0)
    {
        if (lPort* header = logArtf("%slogart\\%s", artPath, InvHeaderArt[info.header]))
        {
            VFX_pane_copy(header->frame(), 0, 0, port->frame(), 0xc4, 0x65, -1);
        }
    }

    if (info.art < 0)
    {
        return;
    }

    globalLogPtr->inventoryIconPorts[info.art]->copyTo(port->frame(), 2, 0x18a, 0);

    switch (info.kind)
    {
        case InvInfoBox::Kind::None:
        {
            break;
        }

        case InvInfoBox::Kind::Component:
        case InvInfoBox::Kind::RepairItem:
        {
            drawComponentInfo(info, port);
            break;
        }

        case InvInfoBox::Kind::RepairMech:
        {
            static_cast<MechRepairBlock*>(info.source)->DrawInfo(port);
            break;
        }

        default:
        {
            static_cast<InventoryBlock*>(info.source)->DrawInfo(port);
            break;
        }
    }
}

auto LogInvScreen::Of(aObject* screen) -> LogInvScreen*
{
    for (LogInvScreen* invScreen : invScreens)
    {
        if (invScreen == screen)
        {
            return invScreen;
        }
    }

    return nullptr;
}

auto LogInvScreen::ForgetInfoSource(lObject* source) -> void
{
    for (LogInvScreen* screen : invScreens)
    {
        if (screen->info.source == source)
        {
            screen->info.kind = InvInfoBox::Kind::None;
            screen->info.source = nullptr;
        }
    }
}

// LogChatWindow

auto LogChatWindow::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, int32_t historySize) -> void
{
    // The original wiped its picture to the key and pasted the frame (lsbdw04) along the bottom; draw shows the frame.
    lObject::init(xPos, yPos, width, height, nullptr, nullptr);
    SetTransparent(-1);
    this->historySize = historySize;
    unknown4C8 = 0;

    char fileName[256];
    framePort = new lPort;
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsbdw04.tga", artPath);
    framePort->init(fileName);

    auto* pane = new ScrollPane;

    if (pane != nullptr)
    {
        pane->init();
    }

    historyPane = pane;
    Assert(pane != nullptr, 0, "Not enough memory for chat scroll", nullptr);
    pane->init(0xb8, height - framePort->height() - 7, 6, 6, static_cast<char*>(nullptr));
    addChild(pane);
    pane->ShowGUIWindow(-1);

    // The history (wiped to 0x10, then written along the bottom as lines come) is drawn from lines.
    lines.clear();
    pane->setDisplayPort(NewHistoryView(pane->lport()->width(), historySize / pane->lport()->width()), -1, -1);
    pane->setScrollPos(100.0f);

    chatInput = new lChatInput;
    chatInput->init(6, height - 0x21, 0xb8, 0x1a, nullptr);
    addChild(chatInput);
    chatInput->ShowGUIWindow(-1);
}

LogChatWindow::~LogChatWindow()
{
    destroy();
}

auto LogChatWindow::ShowGUIWindow(int show) -> void
{
    showWindow = show;
}

auto LogChatWindow::destroy() -> void
{
    delete historyPane;
    historyPane = nullptr;
    delete chatInput;
    chatInput = nullptr;
    freePort(framePort);
    lObject::destroy();
}

auto LogChatWindow::handleNetworkMessage(uint32_t fromPlayerId, void* message) -> void
{
    auto* bytes = static_cast<char*>(message);
    // Team messages are in colour 6, messages to all in 4.
    processChatString(fromPlayerId, bytes + 9, bytes[8] != 0 ? 6 : 4);
}

auto LogChatWindow::processChatString(uint32_t fromPlayerId, char* string, int32_t textColor) -> void
{
    const char* name = "?";

    if (fromPlayerId != 0)
    {
        name = MPlayer->sessionManager->GetPlayer(fromPlayerId)->name;
    }

    if (textColor == -1)
    {
        textColor = 6;
    }

    FIDPPlayer* player = MPlayer->sessionManager->GetPlayer(fromPlayerId);
    // Port fix: an unknown sender (the original read its player number through null) takes player 0's colour.
    int32_t playerNumber = player != nullptr ? player->playerNumber : 0;
    char line[2048];
    std::snprintf(line, sizeof(line), "%%fc%d%s: %%fc%d%s", globalLogPtr->playerColors[playerNumber], name, textColor,
                  string);
    AddLine(line);
}

auto LogChatWindow::AddLine(const char* line) -> void
{
    // The original moved the history picture up by the text's height, wiped the strip along the bottom and wrote the
    // text there; the history keeps the line and draws it so each frame.
    ScrollPane* pane = historyPane;
    std::string text = line;
    const int32_t used =
        application->textFormatter.process(reinterpret_cast<uint8_t*>(text.data()), nullptr, pane->lport()->width(), 0);
    lines.push_back(HistoryLine{std::move(text), used});

    // A line whose strip moved off the top shows nothing any more.
    int32_t above = 0;
    size_t first = lines.size();

    while (first > 0 && above < pane->lport()->height())
    {
        first--;
        above += lines[first].Used;
    }

    lines.erase(lines.begin(), lines.begin() + static_cast<std::ptrdiff_t>(first));
}

auto LogChatWindow::DrawHistory(aPort* port, const std::vector<HistoryLine>& lines) -> void
{
    _pane* frame = port->frame();
    const int32_t portWidth = port->width();
    const int32_t portHeight = port->height();
    VFX_pane_wipe(frame, 0x10);

    // How far each line moved up: the heights of the lines after it.
    int32_t moved = 0;

    for (const HistoryLine& line : lines)
    {
        moved += line.Used;
    }

    const MCRect scissor = port->view.Scissor;

    for (const HistoryLine& line : lines)
    {
        moved -= line.Used;
        const int32_t bottom = portHeight - 1 - moved;
        const int32_t top = bottom - line.Used;

        // The picture ended at its last row when the line was written: nothing of it lies below that.
        port->view.Scissor.Y1 = std::min(scissor.Y1, port->view.OriginY + bottom);

        if (port->view.Open())
        {
            _pane strip = *frame;
            strip.x0 = 0;
            strip.y0 = top;
            strip.x1 = portWidth - 1;
            strip.y1 = bottom;
            VFX_pane_wipe(&strip, 0x10);
            std::string text = line.Text;
            application->textFormatter.process(reinterpret_cast<uint8_t*>(text.data()), port, 0, top);
        }

        port->view.Scissor = scissor;
    }
}

auto LogChatWindow::NewHistoryView(int32_t width, int32_t height) -> lPort*
{
    auto* view = new lPort;
    view->initView(width, height);
    view->DrawContent = [this](aPort* port) { DrawHistory(port, lines); };
    return view;
}

auto LogChatWindow::draw() -> void
{
    if (lport()->viewOpen())
    {
        framePort->copyTo(lport()->frame(), 0, height() - framePort->height(), -1);
    }

    lObject::draw();
}

auto LogChatWindow::handleEvent(aEvent* event) -> void
{
    // The original fetches x() and y() here and drops them.
    x();
    y();

    if (event->type == 9 && parent != nullptr)
    {
        parent->handleEvent(event);
    }
}

auto LogChatWindow::resize(int32_t height) -> void
{
    // The original wiped its picture and pasted the frame at the new bottom (draw shows it there).
    lObject::resize(width(), height);
    chatInput->moveTo(6, height - 0x21, 0);

    // Keep the history across the new pane (the original copied its picture into a new one).
    lPort* oldHistory = historyPane->contentPort;
    lPort* history = NewHistoryView(oldHistory->width(), oldHistory->height());
    delete historyPane;

    auto* pane = new ScrollPane;

    if (pane != nullptr)
    {
        pane->init();
    }

    historyPane = pane;
    Assert(pane != nullptr, 0, "Not enough memory for chat scroll", nullptr);
    pane->init(0xb8, height - framePort->height() - 7, 6, 6, static_cast<char*>(nullptr));
    pane->setDisplayPort(history, -1, -1);
    addChild(pane);
    historyPane->setScrollPos(100.0f);
    historyPane->ShowGUIWindow(-1);
}

auto LogChatWindow::reset() -> void
{
    ScrollPane* pane = historyPane;
    lPort* oldHistory = pane->contentPort;
    lines.clear();
    pane->setDisplayPort(NewHistoryView(oldHistory->width(), oldHistory->height()), -1, -1);
    chatInput->text[0] = 0;
}
