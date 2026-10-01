#include "stdafx.h"
#include "logistics/logrep.h"
#include "gui/afont.h"
#include "gui/scrlpane.h"
#include "lib/aerror.h"
#include "lib/heap.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "logistics/logpur.h"
#include "logistics/logsession.h"
#include "logistics/mrblock.h"
#include "logistics/purchase.h"
#include "logistics/ticker.h"
#include "main/logistics.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "platform/MCInput.h"
#include "sound/soundsys.h"
#include "vfx/vfxfuncs.h"

int32_t resourceDisplayState = 0;

namespace
{
    /// <summary>The height of a unit block in the unit pane.</summary>
    constexpr int32_t UnitBlockHeight = 0x70;

    /// <summary>Set while the left button is down on the screen (DAT_0080867c); only written.</summary>
    int32_t mouseDown = 0;

    /// <summary>The blink phase of the briefing button's highlight, flipped by each timer event (DAT_00808678).</summary>
    int32_t briefingBlink = 0;

    /// <summary>A port the size of the unit pane's contents for every mech and vehicle of the force.</summary>
    lPort* newUnitPort(ScrollPane* pane)
    {
        auto* port = new lPort;
        int32_t height =
            (globalLogPtr->forceVehicleList->getVehicleCount() + globalLogPtr->forceMechList->getMechCount()) *
            UnitBlockHeight;

        if (height < pane->height())
        {
            height = pane->height();
        }

        port->init(pane->width() - 0xd, height, -1);
        return port;
    }

    /// <summary>
    /// Puts the force's repair blocks in order down the unit pane (mechs, then vehicles), scrolled as the pane is.
    /// </summary>
    void placeUnitBlocks(ScrollPane* pane)
    {
        int32_t row = 0;
        int32_t yPos = 0;

        for (LogMech* mech = globalLogPtr->forceMechList->mechs; mech != nullptr; mech = mech->next)
        {
            MechRepairBlock* block = mech->repairBlock;
            block->slotIndex = row;
            block->moveTo(0, yPos - pane->getScrollOffset(), 0);
            block->setDepth(100);
            ++row;
            yPos += UnitBlockHeight;
        }

        yPos = row * UnitBlockHeight;

        for (LogVehicle* vehicle = globalLogPtr->forceVehicleList->vehicles; vehicle != nullptr;
             vehicle = vehicle->next)
        {
            VehicleRepairBlock* block = vehicle->repairBlock;
            block->slotIndex = row;
            block->moveTo(0, yPos - pane->getScrollOffset(), 0);
            block->setDepth(100);
            ++row;
            yPos += UnitBlockHeight;
        }
    }

    /// <summary>
    /// Rebuilds the unit pane's port without the block in <paramref name="row"/>: the rows below it move up one.
    /// </summary>
    void removeUnitRow(ScrollPane* pane, int32_t row)
    {
        lPort* port = newUnitPort(pane);
        auto* work = new lPort;
        work->init(port->width(), port->height(), -1);
        VFX_pane_wipe(port->frame(), 0xff);
        VFX_pane_wipe(work->frame(), 0xff);
        lPort* oldPort = nullptr;
        pane->getDisplayPort(oldPort);
        VFX_pane_copy(oldPort->frame(), 0, 0, port->frame(), 0, 0, -1);
        VFX_pane_copy(oldPort->frame(), 0, (row + 1) * UnitBlockHeight, work->frame(), 0, 0, -1);
        VFX_pane_copy(work->frame(), 0, 0, port->frame(), 0, row * UnitBlockHeight, -1);
        pane->setDisplayPort(port, -1, 0);
        delete work;
        placeUnitBlocks(pane);
    }

    /// <summary>Puts string <paramref name="id"/> on the ticker.</summary>
    void showHelp(uint32_t id)
    {
        char text[256];
        cLoadString(thisInstance, id, text, 0xfe);
        globalLogPtr->ticker->setString(text);
    }
}

auto RepairScreen::init() -> void
{
    unknown4BC = -1;
    chatBlinking = 0;
    selectedMech = nullptr;
    selectedVehicle = nullptr;
    int32_t result = lObject::init(0, 0, 0x280, 0x1e0, nullptr, nullptr);
    Assert(result == 0, result, "Unable to init repair screen", nullptr);
    char fileName[256];
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsrbk00.tga", artPath);
    result = lport()->init(fileName);
    Assert(result == 0, result, "Unable to init repair screen image", nullptr);

    auto* pane = new ScrollPane;

    if (pane != nullptr)
    {
        pane->init();
    }

    inventoryPane = pane;
    Assert(pane != nullptr, 0, " Not enough memory for inventory", nullptr);
    pane->init(0xb8, 0x10d, 8, 0x6b, static_cast<char*>(nullptr));
    pane->setDisplayPort(nullptr, -1, -1);

    pane = new ScrollPane;

    if (pane != nullptr)
    {
        pane->init();
    }

    unitPane = pane;
    Assert(pane != nullptr, 0, "Not enough memory for vehicleScroll", nullptr);
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsrbk01.tga", artPath);
    pane->init(0x1aa, 0x1cc, 0xd3, 0x11, fileName);

    ShowGUIWindow(0);
    addChild(inventoryPane);
    addChild(unitPane);
    screenWindow->addChild(this);
}

auto RepairScreen::destroy() -> void
{
    if (inventoryPane != nullptr)
    {
        // The inventory ports belong to the Logistics object.
        inventoryPane->setDisplayPort(nullptr, 0, -1);
        delete inventoryPane;
        inventoryPane = nullptr;
    }

    if (unitPane != nullptr)
    {
        unitPane->setDisplayPort(nullptr, -1, -1);
        delete unitPane;
        unitPane = nullptr;
    }

    lObject::destroy();
}

auto RepairScreen::selectMech(LogMech* mech) -> void
{
    LogMech* oldMech = globalLogPtr->repairScreen->selectedMech;
    globalLogPtr->repairScreen->selectedMech = mech;

    if (selectedVehicle != nullptr)
    {
        VehicleRepairBlock* block = selectedVehicle->repairBlock;
        selectedVehicle = nullptr;
        block->drawBackground(block->slotIndex, nullptr);
    }

    if (mech != nullptr)
    {
        mech->repairBlock->drawBackground(mech->repairBlock->slotIndex, nullptr);
    }

    if (oldMech != nullptr)
    {
        oldMech->repairBlock->drawBackground(oldMech->repairBlock->slotIndex, nullptr);
    }

    if (globalLogPtr->currentInvTab == 1)
    {
        setUpPilotInv(0, -1);
        return;
    }

    if (globalLogPtr->currentInvTab == 2)
    {
        setUpCompInv(0, -1);
    }
}

auto RepairScreen::selectVehicle(LogVehicle* vehicle) -> void
{
    LogVehicle* oldVehicle = globalLogPtr->repairScreen->selectedVehicle;
    globalLogPtr->repairScreen->selectedVehicle = vehicle;

    if (selectedMech != nullptr)
    {
        MechRepairBlock* block = selectedMech->repairBlock;
        selectedMech = nullptr;
        block->drawBackground(block->slotIndex, nullptr);
    }

    if (vehicle != nullptr)
    {
        vehicle->repairBlock->drawBackground(vehicle->repairBlock->slotIndex, nullptr);
    }

    if (oldVehicle != nullptr)
    {
        oldVehicle->repairBlock->drawBackground(oldVehicle->repairBlock->slotIndex, nullptr);
    }

    if (globalLogPtr->currentInvTab == 1)
    {
        setUpPilotInv(0, -1);
    }

    if (globalLogPtr->currentInvTab == 2)
    {
        setUpCompInv(0, -1);
    }
}

auto RepairScreen::addMechToList(LogMech*) -> void
{
    // The new mech is first in the force list: the old rows move down one block.
    ScrollPane* pane = unitPane;
    lPort* port = newUnitPort(globalLogPtr->repairScreen->unitPane);
    VFX_pane_wipe(port->frame(), 0x10);
    lPort* oldPort = nullptr;
    pane->getDisplayPort(oldPort);
    VFX_pane_copy(oldPort->frame(), 0, 0, port->frame(), 0, UnitBlockHeight, -1);
    pane->setDisplayPort(port, -1, 0);
    placeUnitBlocks(pane);
}

auto RepairScreen::addVehicleToList(LogVehicle* vehicle) -> void
{
    // The new vehicle is first in the vehicle list, right after the mechs: the vehicle rows move down one block.
    lPort* port = newUnitPort(globalLogPtr->repairScreen->unitPane);
    auto* work = new lPort;
    work->init(port->width(), port->height(), -1);
    VFX_pane_wipe(port->frame(), 0x10);
    VFX_pane_wipe(work->frame(), 0x10);
    ScrollPane* pane = unitPane;
    lPort* oldPort = nullptr;
    pane->getDisplayPort(oldPort);
    VFX_pane_copy(oldPort->frame(), 0, 0, port->frame(), 0, 0, -1);
    VFX_pane_copy(oldPort->frame(), 0, globalLogPtr->forceMechList->getMechCount() * UnitBlockHeight, work->frame(), 0,
                  0, -1);
    VFX_pane_copy(work->frame(), 0, 0, port->frame(), 0,
                  (globalLogPtr->forceMechList->getMechCount() + 1) * UnitBlockHeight, -1);
    pane->setDisplayPort(port, -1, 0);
    vehicle->repairBlock->drawBackground(globalLogPtr->forceMechList->getMechCount(), nullptr);
    delete work;
    placeUnitBlocks(pane);
}

auto RepairScreen::removeMechFromList(LogMech* mech) -> void
{
    removeUnitRow(unitPane, mech->repairBlock->slotIndex);
}

auto RepairScreen::removeVehicleFromList(LogVehicle* vehicle) -> void
{
    removeUnitRow(unitPane, vehicle->repairBlock->slotIndex);
}

auto RepairScreen::drawBackground() -> void
{
    auto* port = new lPort;
    char fileName[256];
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsrbk00.tga", artPath);
    port->init(fileName);
    VFX_pane_copy(port->frame(), 0, 0, lport()->frame(), 0, 0, -1);

    if (selectedMech != nullptr && selectedMech->repairBlock != nullptr)
    {
        selectedMech->repairBlock->drawButtons(nullptr);
    }

    delete port;
}

auto RepairScreen::handleEvent(aEvent* event) -> void
{
    if (globalLogPtr->currentScreen != this)
    {
        return;
    }

    int32_t xPos = event->x;
    int32_t yPos = event->y;
    uint8_t key = event->key;

    if (key == 0 && event->type != 0x13)
    {
        // The help line for whatever the mouse is over, and the highlighted screen button.
        globalLogPtr->drawScreenButtons();
        drawBlankInvInfoBlock(-1);
        POINT point{xPos, yPos};
        auto inside = [&point](int32_t left, int32_t top, int32_t right, int32_t bottom)
        {
            RECT area{left, top, right, bottom};
            return PtInRect(&area, point) != 0;
        };

        if (inside(2, 2, 0xd1, 0xd))
        {
            showHelp(0x1d);
        }
        else if (inside(2, 0x10, 0xd1, 0x21))
        {
            showHelp(0x286);
            lPort* highlight =
                MPlayer == nullptr ? globalLogPtr->screenButtonPorts[0][1] : globalLogPtr->screenButtonPorts[1][1];
            highlight->copyTo(lport()->frame(), 2, 0x10, -1);
        }
        else if (inside(2, 0x22, 0xd1, 0x33))
        {
            showHelp(0x1e);
            globalLogPtr->screenButtonPorts[2][1]->copyTo(lport()->frame(), 2, 0x22, -1);
        }
        else if (inside(2, 0x34, 0xd1, 0x45))
        {
            showHelp(0x41);
            globalLogPtr->screenButtonPorts[3][1]->copyTo(lport()->frame(), 2, 0x34, -1);
        }
        else if (inside(2, 0x46, 0xd1, 0x57))
        {
            showHelp(0x42);
        }
        else if (inside(0x20c, 2, 0x24d, 0xd))
        {
            showHelp(0x1f);
        }
        else if (inside(0x19b, 4, 0x24a, 0xf))
        {
            showHelp(0x1f);
        }
        else if (inside(0xc6, 0x66, 0xd1, 0x95))
        {
            showHelp(0x29);
        }
        else if (inside(0xc6, 0x97, 0xd1, 0xef))
        {
            showHelp(0x2a);
        }
        else if (inside(0xc6, 0xf1, 0xd1, 0x140))
        {
            showHelp(0x2b);
        }
        else if (inside(0xc6, 0x141, 0xcf, 0x17c))
        {
            showHelp(0x2c);
        }
        else if (inside(7, 0x6a, 0xb3, 0x178))
        {
            switch (globalLogPtr->currentInvTab)
            {
                case 0:
                    showHelp(0x31);
                    break;
                case 1:
                    showHelp(0x33);
                    break;
                case 2:
                    showHelp(0x32);
                    break;
                case 3:
                    showHelp(0x47);
                    break;
                default:
                {
                    // Port fix: the original put its uninitialised string buffer on the ticker.
                    char empty[1] = {};
                    globalLogPtr->ticker->setString(empty);
                    break;
                }
            }
        }
        else if (inside(0x270, 0x10, 0x27d, 0x1dd))
        {
            showHelp(0x2f);
        }
        else if (inside(0xd3, 0x10, 0x270, 0x1dd))
        {
            showHelp(0x43);
        }
        else
        {
            globalLogPtr->ticker->setString(nullptr);
        }
    }

    if (event->type == 1)
    {
        mouseDown = -1;
        POINT point{xPos - globalX(), yPos - globalY()};
        RECT area{2, 0x34, 0xd1, 0x45};

        if (PtInRect(&area, point) != 0)
        {
            globalLogPtr->setUpPurchaseScreen(-1);
            return;
        }

        area.top = 0x22;
        area.bottom = 0x32;

        if (PtInRect(&area, point) != 0)
        {
            globalLogPtr->setUpBriefingScreen(-1);
            return;
        }

        area.top = 0x10;
        area.bottom = 0x21;

        if (PtInRect(&area, point) != 0)
        {
            if (MPlayer != nullptr)
            {
                CheckExit();
                return;
            }

            soundSystem->playDigitalSample(0x36, 1, nullptr, 0, 0);
            globalLogPtr->setUpMainScreen(0);
            return;
        }

        // The inventory tabs.
        area = {0xc2, 0x67, 0xd1, 0x95};

        if (PtInRect(&area, point) != 0)
        {
            soundSystem->playDigitalSample(0x36, 1, nullptr, 0, 0);
            setUpMechInv(-1, -1);
            setUpMechPurchase();
        }

        area.top = 0x96;
        area.bottom = 0xf1;

        if (PtInRect(&area, point) != 0)
        {
            soundSystem->playDigitalSample(0x36, 1, nullptr, 0, 0);
            setUpPilotInv(-1, -1);
            setUpPilotPurchase();
        }

        area.top = 0xf2;
        area.bottom = 0x140;

        if (PtInRect(&area, point) != 0)
        {
            soundSystem->playDigitalSample(0x36, 1, nullptr, 0, 0);
            setUpCompInv(-1, -1);
            setUpCompPurchase();
        }

        area.top = 0x141;
        area.bottom = 0x17c;

        if (PtInRect(&area, point) != 0)
        {
            soundSystem->playDigitalSample(0x36, 1, nullptr, 0, 0);
            setUpVhclInv(-1, -1);
            setUpVehiclePurchase();
        }
    }

    if (event->type == 4)
    {
        mouseDown = 0;
        // The original fetches globalX() and globalY() here and drops them.
        globalX();
        globalY();
    }

    if (event->type == 9)
    {
        bool ctrlAlt = MCInput::GetAsyncKeyState(VK_CONTROL) != 0 && MCInput::GetAsyncKeyState(VK_MENU) != 0;

        // Original behaviour (OB-077): the key byte, zero-extended, is compared with -0x45 (VK_OEM_PLUS as a signed
        // char), so the Ctrl+Alt+= resource cheat never fires.
        if (MPlayer == nullptr && static_cast<int32_t>(key) == -0x45 && ctrlAlt)
        {
            ResourcePoints += 1000;
        }
    }

    if (event->type == 8)
    {
        globalLogPtr->processCheatCode(event->scanCode);
        return;
    }

    if (event->type == 0x13)
    {
        // Blink the briefing button.
        lPort* picture =
            briefingBlink == 0 ? globalLogPtr->screenButtonPorts[2][0] : globalLogPtr->screenButtonPorts[2][1];
        picture->copyTo(lport()->frame(), 2, 0x22, -1);
        briefingBlink = briefingBlink == 0 ? 1 : 0;
    }
}

auto RepairScreen::ShowGUIWindow(int show) -> void
{
    showWindow = show;
    inventoryPane->ShowGUIWindow(show);
    unitPane->ShowGUIWindow(show);
}

auto RepairScreen::display() -> void
{
    lObject::display();
    lObject* screen = globalLogPtr->currentScreen;

    if (screen != globalLogPtr->repairScreen && screen != globalLogPtr->purchaseScreen &&
        screen != globalLogPtr->briefingScreen && screen != globalLogPtr->sessionScreen)
    {
        return;
    }

    // Port fix: the original left the text uninitialised for an unknown resourceDisplayState.
    char text[44] = {};

    switch (resourceDisplayState)
    {
        case 0:
            std::snprintf(text, sizeof(text), "%d", ResourcePoints);
            break;
        case 1:
            std::snprintf(text, sizeof(text), "%d", globalLogPtr->logisticsHeap->totalCoreLeft());
            break;
        case 2:
            std::snprintf(text, sizeof(text), "%d", globalLogPtr->logisticsHeap->coreLeft());
            break;
        default:
            break;
    }

    if (globalLogPtr->currentScreen != globalLogPtr->sessionScreen)
    {
        VFX_pane_copy(globalLogPtr->resourceBackPort->frame(), 0, 0, globalLogPtr->currentScreen->lport()->frame(),
                      0x209, 2, -1);
        auto* bytes = reinterpret_cast<uint8_t*>(text);
        int32_t textWidth = medWhiteFont->width(bytes);
        medWhiteFont->writeString(globalLogPtr->currentScreen->lport()->frame(), 0x244 - textWidth, 4, bytes, -1);
    }

    VFX_pane_copy(globalLogPtr->clockBackPort->frame(), 0, 0, globalLogPtr->currentScreen->lport()->frame(), 0x24c, 2,
                  -1);
    MCPort::StrTime(globalLogPtr->timeString);
    medWhiteFont->writeString(globalLogPtr->currentScreen->lport()->frame(), 0x254, 4,
                              reinterpret_cast<uint8_t*>(globalLogPtr->timeString), -1);
}
