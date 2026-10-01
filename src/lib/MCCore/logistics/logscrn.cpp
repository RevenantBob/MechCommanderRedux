#include "stdafx.h"
#include "logistics/logscrn.h"
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

    /// <summary>A port of <paramref name="height"/> rows (at least the pane's), 13 pixels narrower than the pane.</summary>
    lPort* newPanePort(ScrollPane* pane, int32_t height, int32_t color)
    {
        auto* port = new lPort;

        if (height < pane->height())
        {
            height = pane->height();
        }

        port->init(pane->width() - 0xd, height, -1);
        VFX_pane_wipe(port->frame(), color);
        return port;
    }

    /// <summary>An inventory port for <paramref name="count"/> blocks (at least the pane's height).</summary>
    lPort* newInvPort(int32_t count)
    {
        auto* port = new lPort;
        int32_t height = count * InvBlockHeight;

        if (height < MinInvPortHeight)
        {
            height = MinInvPortHeight;
        }

        port->init(InvPortWidth, height, -1);
        VFX_pane_wipe(port->frame(), 0x10);
        return port;
    }

    /// <summary>
    /// The inventory tab art common to the <c>setUp*Inv</c> functions: the tab strip <paramref name="tabArt"/> at
    /// (2, 0x18a) when <paramref name="redrawTabs"/>, then the header <paramref name="headerArt"/> at (0xc4, 0x65).
    /// </summary>
    void drawInvTabArt(lObject* screen, const char* tabArt, const char* headerArt, int redrawTabs)
    {
        char fileName[256];
        auto* port = new lPort;

        if (redrawTabs != 0)
        {
            std::snprintf(fileName, sizeof(fileName), "%slogart\\%s", artPath, tabArt);
            port->init(fileName);
            VFX_pane_copy(port->frame(), 0, 0, screen->lport()->frame(), 2, 0x18a, -1);
            port->destroy();
        }

        std::snprintf(fileName, sizeof(fileName), "%slogart\\%s", artPath, headerArt);
        port->init(fileName);
        VFX_pane_copy(port->frame(), 0, 0, screen->lport()->frame(), 0xc4, 0x65, -1);
        delete port;
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
    lPort* port = newPanePort(pane, count * UnitBlockHeight, 0xff);
    pane->setDisplayPort(port, -1, -1);

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

    if (redraw == 0)
    {
        freePort(screen->purCompPort);
        freePort(screen->purMechPort);
        freePort(screen->purPilotPort);
        freePort(screen->purVehiclePort);

        // The store's mechs.
        lPort* port = newPanePort(pane, globalLogPtr->purMechList->getMechCount() * UnitBlockHeight, 0x10);
        pane->setDisplayPort(port, 0, -1);
        screen->purMechPort = port;
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
        port = newPanePort(pane, globalLogPtr->purVehicleList->getVehicleCount() * UnitBlockHeight, 0x10);
        screen->purVehiclePort = port;
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
        auto* compPort = new lPort;
        int32_t height = globalLogPtr->purchaseComponents->numItems * UnitBlockHeight;

        if (height < pane->height())
        {
            height = pane->height();
        }

        freePort(screen->purCompPort);
        compPort->init(pane->width() - 0xd, height, -1);
        VFX_pane_wipe(compPort->frame(), 0x10);
        screen->purCompPort = compPort;
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
    // Original behaviour (OB-076): with redraw set, the old pilot port is replaced without being freed.
    int32_t visible = 0;

    for (PurPilotData* pilot = globalLogPtr->purPilotList->first; pilot != nullptr; pilot = pilot->next)
    {
        if (pilot->status == 0)
        {
            ++visible;
        }
    }

    lPort* port = newPanePort(pane, visible * UnitBlockHeight, 0xff);
    screen->purPilotPort = port;
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
    freePort(globalLogPtr->invTabPorts[0]);
    globalLogPtr->invTabPorts[0] = newInvPort(globalLogPtr->mechList->getMechCount());
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
    lPort* port = newInvPort(globalLogPtr->vehicleList->getVehicleCount());
    freePort(globalLogPtr->invTabPorts[3]);
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
    auto* port = new lPort;
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

    port->init(inventoryPane->width() - 0xd, height, -1);
    VFX_pane_wipe(port->frame(), 0x10);
    freePort(globalLogPtr->invTabPorts[1]);
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
        globalLogPtr->inventoryIconPorts[tab]->copyTo(globalLogPtr->currentScreen->lport()->frame(), 2, 0x18a, 0);
    }
}

auto LogInvScreen::createCompInvBlock() -> void
{
    lPort* port = newInvPort(globalLogPtr->reIndexInventory());
    freePort(globalLogPtr->invTabPorts[2]);
    globalLogPtr->invTabPorts[2] = port;

    for (_LogInventoryItem* item = globalLogPtr->componentInventory->items; item != nullptr; item = item->next)
    {
        item->inventoryBlock->drawBackground();
    }
}

auto LogInvScreen::setUpMechInv(int scrollPos, int redrawTabs) -> void
{
    globalLogPtr->currentInvTab = 0;
    drawInvTabArt(this, "lsciim.tga", "lscdwm.tga", redrawTabs);
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
    drawInvTabArt(this, "lsciip.tga", "lscdwp.tga", redrawTabs);
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
    drawInvTabArt(this, "lsciic.tga", "lscdwc.tga", redrawTabs);
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
    drawInvTabArt(this, "lsciiv.tga", "lscdwv.tga", redrawTabs);
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
    auto* port = new lPort;
    auto* work = new lPort;
    int32_t count = globalLogPtr->purPilotList->getVisiblePilotCount();
    int32_t used = count * UnitBlockHeight;
    int32_t height = used;

    if (height < pane->height())
    {
        height = pane->height();
    }

    port->init(pane->width() - 0x10, height, -1);
    work->init(pane->width() - 0x10, height, -1);

    if (height == pane->height())
    {
        VFX_pane_wipe(work->frame(), 0xff);
    }

    // Copy the old pilot rows, then close the gap: the rows below the removed one move up a block.
    VFX_pane_copy(screen->purPilotPort->frame(), 0, 0, port->frame(), 0, 0, -1);

    if (pilotIndex < count)
    {
        VFX_pane_copy(screen->purPilotPort->frame(), 0, (pilotIndex + 1) * UnitBlockHeight, work->frame(), 0, 0, -1);
        VFX_pane_copy(work->frame(), 0, 0, port->frame(), 0, pilotIndex * UnitBlockHeight, -1);
    }

    if (count < 5)
    {
        VFX_pane_wipe(work->frame(), 0xff);
        VFX_pane_copy(work->frame(), 0, 0, port->frame(), 0, used, -1);
    }

    screen->purPilotPort = port;
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

    delete work;

    int32_t row = 0;

    for (PurPilotData* pilot = globalLogPtr->purPilotList->first; pilot != nullptr; pilot = pilot->next)
    {
        if (pilot->status == 0)
        {
            pilot->block->row = row++;
        }
    }
}

// LogChatWindow

auto LogChatWindow::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, int32_t historySize) -> void
{
    lObject::init(xPos, yPos, width, height, nullptr, nullptr);
    VFX_pane_wipe(lport()->frame(), 0xff);
    SetTransparent(-1);
    this->historySize = historySize;
    unknown4C8 = 0;

    char fileName[256];
    framePort = new lPort;
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsbdw04.tga", artPath);
    framePort->init(fileName);
    framePort->copyTo(ownPort->frame(), 0, height - framePort->height(), -1);

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

    auto* history = new lPort;
    history->init(pane->lport()->width(), historySize / pane->lport()->width(), -1);
    VFX_pane_wipe(history->frame(), 0x10);
    pane->setDisplayPort(history, -1, -1);
    pane->setScrollPos(100.0f);

    chatInput = new lChatInput;
    chatInput->init(6, height - 0x21, 0xb8, 0x1a, nullptr);
    addChild(chatInput);
    chatInput->ShowGUIWindow(-1);
    chatInput->draw();
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

    // Scroll the history up by the new text's height and write it along the bottom.
    ScrollPane* pane = historyPane;
    auto* text = reinterpret_cast<uint8_t*>(line);
    int32_t used = application->textFormatter.process(text, nullptr, pane->lport()->width(), 0);
    uint8_t* pixels = pane->lport()->bitmap()->buffer;
    int32_t portWidth = pane->lport()->width();
    int32_t portHeight = pane->lport()->height();
    std::memmove(pixels, pixels + portWidth * used, static_cast<size_t>((portHeight - used) * portWidth));
    _pane bottom = *pane->lport()->frame();
    bottom.x0 = 0;
    bottom.y0 = portHeight - used - 1;
    bottom.x1 = portWidth - 1;
    bottom.y1 = portHeight - 1;
    VFX_pane_wipe(&bottom, 0x10);
    application->textFormatter.process(text, pane->lport(), 0, portHeight - used - 1);
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
    lObject::resize(width(), height);
    VFX_pane_wipe(lport()->frame(), 0xff);
    framePort->copyTo(ownPort->frame(), 0, height - framePort->height(), -1);
    chatInput->moveTo(6, height - 0x21, 0);

    // Keep the history across the new pane.
    auto* history = new lPort;
    lPort* oldHistory = historyPane->contentPort;
    history->init(oldHistory->width(), oldHistory->height(), -1);
    oldHistory->copyTo(history->frame(), 0, 0, -1);
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
    auto* history = new lPort;
    ScrollPane* pane = historyPane;
    lPort* oldHistory = pane->contentPort;
    history->init(oldHistory->width(), oldHistory->height(), -1);
    VFX_pane_wipe(history->frame(), 0x10);
    pane->setDisplayPort(history, -1, -1);
    chatInput->text[0] = 0;
}
