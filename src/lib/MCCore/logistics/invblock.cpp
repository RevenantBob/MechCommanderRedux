#include "stdafx.h"
#include "logistics/invblock.h"
#include "gui/afont.h"
#include "gui/mchwcursor.h"
#include "gui/scrlpane.h"
#include "lib/heap.h"
#include "logistics/logbri.h"
#include "logistics/logdlg.h"
#include "logistics/loggen.h"
#include "logistics/logmain.h"
#include "logistics/logpur.h"
#include "logistics/logrep.h"
#include "logistics/mrblock.h"
#include "logistics/purchase.h"
#include "logistics/ticker.h"
#include "main/logistics.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "object/cmponent.h"
#include "sound/soundsys.h"
#include "vfx/vfxfuncs.h"

int32_t numRestrictedComponents = 5;
int32_t restrictedComps[5] = {15, 37, 38, 42, 43};
LogMech* globalMechPtr = nullptr;
_LogInventoryItem* globalCompPtr = nullptr;
LogVehicle* globalVehicle = nullptr;
int Solo = 0;
int32_t iconFade[4][3] = {{242, 241, 240}, {245, 244, 243}, {239, 238, 237}, {19, 19, 19}};

namespace
{
    /// <summary>The drag state of one kind of inventory row.</summary>
    struct DragState
    {
        /// <summary>Nonzero while the row is dragged with the left button held (picked up by event 1).</summary>
        int32_t dragging = 0;
        /// <summary>Nonzero while the row is carried after a right-button pick-up (event 3).</summary>
        int32_t carrying = 0;
        /// <summary>Where the drag icon is (window coordinates).</summary>
        int32_t x = 0;
        int32_t y = 0;
    };

    /// <summary>The mech rows' drag (DAT_00808060 dragging, 64/68 x/y, 6c carrying).</summary>
    DragState mechDrag;
    /// <summary>The pilot rows' drag (DAT_00808074 dragging, 78/7c x/y, 80 carrying).</summary>
    DragState pilotDrag;
    /// <summary>The vehicle rows' drag (DAT_00808084 dragging, 88/8c x/y, 90 carrying).</summary>
    DragState vehicleDrag;
    /// <summary>The component rows' drag (DAT_00808094 dragging, 98 carrying, 9c/a0 x/y).</summary>
    DragState compDrag;

    /// <summary>
    /// The row the next <see cref="PilotInventoryBlock::init"/> takes (DAT_00808070). Never reset: the inventory
    /// screen renumbers the rows itself.
    /// </summary>
    int32_t nextPilotRow = 0;

    /// <summary>The pilot a sell dialog is open for (DAT_00808054; <see cref="PilotSellCallback"/>).</summary>
    LogWarrior* pilotToSell = nullptr;

    void* logAlloc(uint32_t size)
    {
        return globalLogPtr->logisticsHeap->malloc(size);
    }

    void logFree(void* block)
    {
        globalLogPtr->logisticsHeap->free(block);
    }

    void freePort(lPort*& port)
    {
        if (port != nullptr)
        {
            delete port;
        }

        port = nullptr;
    }

    void playSample(uint32_t sampleId)
    {
        soundSystem->playDigitalSample(sampleId, 1, nullptr, 0, 0);
    }

    /// <summary>A copy of <paramref name="text"/> on the logistics heap.</summary>
    char* heapString(const char* text)
    {
        size_t length = std::strlen(text) + 1;
        auto* copy = static_cast<char*>(logAlloc(static_cast<uint32_t>(length)));
        std::memcpy(copy, text, length);
        return copy;
    }

    /// <summary>Copies <paramref name="text"/> into a stat text field.</summary>
    /// <remarks>Port fix: bounded (the original <c>strcpy</c> would run into the next field).</remarks>
    template <size_t Size> void setText(char (&field)[Size], const char* text)
    {
        std::snprintf(field, Size, "%s", text);
    }

    /// <summary>
    /// Whether the event is over the pane's inside: right of its left edge, left of 0xd pixels short of its right
    /// edge (the scroll bar), between its top and bottom. The position comes from <paramref name="posPane"/>, the
    /// width and height from <paramref name="widthPane"/> and <paramref name="heightPane"/> (the component rows mix
    /// the inventory and the unit pane).
    /// </summary>
    bool overPane(aObject* posPane, aObject* widthPane, aObject* heightPane, aEvent* event)
    {
        return posPane->globalX() < event->x && event->x < posPane->globalX() + widthPane->width() - 0xd &&
               posPane->globalY() < event->y && event->y < posPane->globalY() + heightPane->height();
    }

    bool overPane(aObject* pane, aEvent* event)
    {
        return overPane(pane, pane, pane, event);
    }

    /// <summary>The logistics screen being shown, as the inventory screen it is when a row gets events.</summary>
    LogInvScreen* invScreen()
    {
        return static_cast<LogInvScreen*>(globalLogPtr->currentScreen);
    }

    bool onRepairScreen()
    {
        return globalLogPtr->currentScreen == globalLogPtr->repairScreen;
    }

    bool onPurchaseScreen()
    {
        return globalLogPtr->currentScreen == globalLogPtr->purchaseScreen;
    }

    /// <summary>
    /// Handles what every row does first: passes keys up to the parent when nothing is dragged, and when the mouse
    /// leaves the inventory pane vertically, clears the info block and hands the event to the screen.
    /// </summary>
    /// <returns>False when the event was handled (or the row is hidden) and the row should do nothing more.</returns>
    bool preHandleEvent(aObject* row, const DragState& drag, aEvent* event)
    {
        if (row->parent != nullptr && drag.dragging == 0 && drag.carrying == 0 &&
            (event->type == 8 || event->type == 9))
        {
            row->parent->handleEvent(event);
            return false;
        }

        LogInvScreen* screen = invScreen();

        if (drag.dragging == 0 && drag.carrying == 0)
        {
            aObject* pane = screen->inventoryPane;

            if (event->y < pane->globalY() || pane->globalY() + pane->height() < event->y)
            {
                screen->drawBlankInvInfoBlock(-1);
                screen->handleEvent(event);
                return false;
            }

            if (row->IsShowing() == 0)
            {
                return false;
            }
        }

        return true;
    }

    /// <summary>
    /// Makes the drag icon: a 0x20 square of the row at (2, 1) (<see cref="InventoryBlock::OnBeginDrag"/>), framed
    /// in colour 0xea, added to the screen and centred on the cursor.
    /// </summary>
    void makeDragIcon(DragState& drag, InventoryBlock* row, aEvent* event)
    {
        drag.x = 2;
        drag.y = row->listIndex * row->winHeight + 1;
        auto* icon = new DragIcon;
        globalLogPtr->dragIcon = icon;
        icon->Begin(drag.x, drag.y, 0x20, 0x20, [row](lPort* surface) { row->OnBeginDrag(surface); });
        invScreen()->addChild(globalLogPtr->dragIcon);
        globalLogPtr->dragIcon->moveTo(event->x - 0xf, event->y - 0xf, 0);
    }

    /// <summary>Shows the drag icon above everything.</summary>
    void raiseDragIcon()
    {
        globalLogPtr->dragIcon->ShowGUIWindow(1);
        globalLogPtr->dragIcon->setDepth(100);
    }

    /// <summary>Frees the drag icon.</summary>
    void deleteDragIcon()
    {
        if (globalLogPtr->dragIcon != nullptr)
        {
            delete globalLogPtr->dragIcon;
        }

        globalLogPtr->dragIcon = nullptr;
    }

    /// <summary>
    /// Copies the "drop here" art over the screen at (2, 0x18a): the blank info box of tab <paramref name="tab"/>
    /// (the original loaded the same picture, <c>logart\lscii?.tga</c>, again).
    /// </summary>
    void drawDropArt(LogInvScreen* screen, int32_t tab)
    {
        screen->info.Blank(tab);
    }

    /// <summary>Shows the one-button message dialog with string <paramref name="id"/>.</summary>
    /// <param name="okayArt">Nonzero: first give the OK button the "okay" pictures and enable it.</param>
    void showMessage(uint32_t id, bool okayArt)
    {
        char text[256];
        cLoadString(thisInstance, id, text, 0xfe);
        ReusableDialog* dialog = globalLogPtr->messageDialog;
        dialog->setText(text);
        globalLogPtr->messageDialog->setTwoButton(0);
        dialog = globalLogPtr->messageDialog;
        dialog->callback = nullptr;

        if (okayArt)
        {
            char upArt[] = "bh_okay.tga";
            char downArt[] = "bg_okay.tga";
            dialog->okButton->setUpPicture(upArt);
            globalLogPtr->messageDialog->okButton->setDownPicture(downArt);
            lDialogButton* button = globalLogPtr->messageDialog->okButton;
            button->disabled = 0;
            button->draw();
        }

        globalLogPtr->messageDialog->activate();
    }

    /// <summary>A sale's price: half the value, except in a multiplayer or solo game.</summary>
    int32_t salePrice(int32_t value)
    {
        if (MPlayer == nullptr && Solo == 0)
        {
            value /= 2;
        }

        return value;
    }

    /// <summary>Opens the purchase dialog as a sale, on the current screen.</summary>
    void openSaleDialog(int32_t purchaseType, int32_t price, int32_t maxQuantity, char* title, char* subtitle,
                        lPort* picture, void (*callback)(int, int32_t))
    {
        globalLogPtr->purchaseDialog->init(purchaseType, -price, maxQuantity, title, subtitle, picture);
        globalLogPtr->purchaseDialog->setCallback(callback);
        globalLogPtr->purchaseDialog->setPort(globalLogPtr->currentScreen->lport());
        globalLogPtr->purchaseDialog->activate();
    }

    /// <summary>The string table index of the weight class of <paramref name="tonnage"/> (light .. assault).</summary>
    uint32_t weightClassString(float tonnage)
    {
        if (tonnage < 40.0f)
        {
            return 0x4f;
        }

        if (tonnage < 60.0f)
        {
            return 0x50;
        }

        if (tonnage < 80.0f)
        {
            return 0x51;
        }

        return 0x52;
    }

    /// <summary>The rank name (string table 0x70..0x73) into <paramref name="text"/>.</summary>
    /// <remarks>Port fix: an out-of-range rank leaves the text empty (the original kept whatever was in the buffer).</remarks>
    void loadRankName(int32_t rank, char* text)
    {
        text[0] = '\0';

        if (rank >= 0 && rank <= 3)
        {
            cLoadString(thisInstance, 0x70 + static_cast<uint32_t>(rank), text, 0xfe);
        }
    }

    /// <summary>A picture holding a copy of <paramref name="art"/>: where pilot and component rows are put together.</summary>
    lPort* newRowPicture(lPort* art)
    {
        auto* row = new lPort;
        row->init(art->width(), art->height(), 1);
        VFX_pane_copy(art->frame(), 0, 0, row->frame(), 0, 0, -1);
        return row;
    }

    void writeText(aFont* font, lPort* port, int32_t x, int32_t y, const char* text)
    {
        font->writeString(port->frame(), x, y, reinterpret_cast<uint8_t*>(const_cast<char*>(text)), -1);
    }

    /// <summary>
    /// The info box's picture of a mech or vehicle row: the 0x1e square at (3, 2) of the row (its art and
    /// <paramref name="picture"/>), drawn at (9, 0x191) of <paramref name="port"/>, as the original copied it out of
    /// the tab's picture.
    /// </summary>
    void drawRowPicture(lPort* picture, lPort* port)
    {
        auto* square = new lPort;
        square->init(0x1e, 0x1e, 1);
        VFX_pane_copy(globalLogPtr->invBlockPort->frame(), 3, 2, square->frame(), 0, 0, -1);

        if (picture != nullptr)
        {
            picture->copyTo(square->frame(), 2, 0, 1);
        }

        VFX_pane_copy(square->frame(), 0, 0, port->frame(), 9, 0x191, -1);
        delete square;
    }

    /// <summary>The damage state of a diagram location from its armor left (-1 = undamaged).</summary>
    int32_t damageState(int32_t percent, uint8_t internals)
    {
        if (percent == 0)
        {
            return internals == 0 ? 3 : 2;
        }

        if (percent < 0x1a)
        {
            return 2;
        }

        if (percent < 0x33)
        {
            return 1;
        }

        return percent < 0x4c ? 0 : -1;
    }

    /// <summary>
    /// Draws diagram shape <paramref name="location"/> of <paramref name="shapes"/> into a 0x19 x 0x1e port,
    /// recoloured for <paramref name="state"/>, and copies it into <paramref name="port"/>.
    /// </summary>
    void drawDiagram(void* shapes, int32_t location, int32_t state, lPort* diagram, lPort* port, int32_t xPos,
                     int32_t yPos)
    {
        diagram->init(0x19, 0x1e, 1);
        VFX_pane_wipe(diagram->frame(), 0xff);
        AG_shape_draw(diagram->frame(), shapes, location, 0, 0);

        if (state >= 0)
        {
            uint8_t* pixel = diagram->frame()->window->buffer;

            for (int32_t count = 0x2ee; count != 0; --count, ++pixel)
            {
                if (*pixel == 0xff)
                {
                    continue;
                }

                if (*pixel == 0xe7)
                {
                    *pixel = static_cast<uint8_t>(iconFade[state][2]);
                }
                else if (*pixel == 0xe8)
                {
                    *pixel = static_cast<uint8_t>(iconFade[state][1]);
                }
                else if (*pixel == 0xea)
                {
                    *pixel = static_cast<uint8_t>(iconFade[state][0]);
                }
            }
        }

        diagram->copyTo(port->frame(), xPos, yPos, 1);
    }

    /// <summary>
    /// Adds one to every filled drop zone slot's first (<paramref name="vehicle"/> false) or second field: a unit
    /// joining the force goes in front of the others.
    /// </summary>
    void bumpDeploySlots(bool vehicle)
    {
        for (auto& lance : globalLogPtr->deploySlots)
        {
            for (auto& slot : lance)
            {
                int32_t& value = vehicle ? slot.vehicle : slot.unit;

                if (value > -1)
                {
                    ++value;
                }
            }
        }
    }

    /// <summary>
    /// Mounts the dragged component on <paramref name="mech"/> (the one selected on the repair screen): undeploys
    /// it, and when it has the free tonnage adds the component (and a weapon's ammo) to its inventory.
    /// </summary>
    /// <returns>True when mounted; false after showing the "too heavy" message.</returns>
    bool mountComponent(_LogInventoryItem* item, LogMech* mech)
    {
        if (mech->deployed != 0)
        {
            mech->repairBlock->undeployMech();
        }

        uint8_t masterID = item->masterID;
        MasterComponent& component = MasterComponentList[masterID];
        bool withAmmo =
            component.form == COMPONENT_FORM_WEAPON_BALLISTIC || component.form == COMPONENT_FORM_WEAPON_MISSILE;
        double tons = component.tonnage;

        if (withAmmo)
        {
            tons += MasterComponentList[component.ammoMasterId].tonnage;
        }

        if (tons <= static_cast<double>(mech->curTonnage) - mech->usedTonnage)
        {
            playSample(0x34);
            _LogInventoryStat* stat = mech->inventory->createStat(masterID, 0, 0, 1, 0, 1, 0xff);
            mech->inventory->addItem(masterID, stat, -1);

            if (withAmmo)
            {
                uint8_t ammoID = component.ammoMasterId;
                stat = mech->inventory->createStat(masterID, 0, 0, 0, -1, -1, 0xff);
                mech->inventory->addItem(ammoID, stat, -1);
            }

            float added = static_cast<float>(tons);
            mech->usedTonnage += added;
            mech->weaponTonnage += added;
            mech->repairBlock->setInventory(nullptr);
            mech->calcBR();
            globalLogPtr->repairScreen->selectMech(mech);
            return true;
        }

        showMessage(99, true);
        return false;
    }
}

// Sell callbacks

auto MechSellCallback(int confirmed, int32_t) -> void
{
    Logistics* logistics = globalLogPtr;

    if (confirmed != 0)
    {
        InventoryList* spares = globalLogPtr->componentInventory;

        for (_LogInventoryItem* item = globalMechPtr->inventory->items; item != nullptr; item = item->next)
        {
            uint8_t masterID = item->masterID;
            int32_t form = MasterComponentList[masterID].form;

            if (form != COMPONENT_FORM_SENSOR && form != COMPONENT_FORM_WEAPON_ENERGY &&
                form != COMPONENT_FORM_WEAPON_BALLISTIC && form != COMPONENT_FORM_WEAPON_MISSILE &&
                form != COMPONENT_FORM_ECM && form != COMPONENT_FORM_PROBE && form != COMPONENT_FORM_JAMMER)
            {
                continue;
            }

            // Every undamaged copy goes back to the spare components.
            for (_LogInventoryStat* stat = item->stats; stat != nullptr; stat = stat->next)
            {
                if (stat->hits != 0)
                {
                    continue;
                }

                _LogInventoryItem* spare = spares->getItemInfo(spares->getIndexFromMasterID(masterID));

                if (spare == nullptr)
                {
                    _LogInventoryStat* newStat = spares->createStat(masterID, 0, 0, 1, 0, 0, 0xff);
                    spares->addItem(masterID, newStat, -1);
                    spare = spares->getItemInfo(spares->getIndexFromMasterID(masterID));
                    auto* block = new CompInventoryBlock;
                    spare->inventoryBlock = block;
                    block->init(spare);
                    spare->inventoryBlock->inventoryIndex = spares->numItems - 1;
                }

                ++spare->count;
            }
        }

        globalLogPtr->purchaseScreen->createCompInvBlock();
        int32_t index = globalLogPtr->forceMechList->getMechIndex(globalMechPtr);
        globalLogPtr->forceMechList->removeMech(static_cast<uint8_t>(index));
        globalLogPtr->reorderMechs();
        ResourcePoints -= globalLogPtr->purchaseDialog->unitCost;
        return;
    }

    globalMechPtr->assigned = 0;
    logistics->reorderMechs();
    globalLogPtr->purchaseScreen->createMechInvBlock();
    globalLogPtr->purchaseScreen->setUpMechInv(0, 1);
}

auto PilotSellCallback(int confirmed, int32_t) -> void
{
    LogWarrior* warrior = pilotToSell;

    if (confirmed != 0)
    {
        int32_t row = warrior->inventoryBlock->listIndex;
        soundSystem->playPilotSpeech(warrior->pilotAudio, 2);
        warrior->sold = 1;
        globalLogPtr->purPilotList->setPilotStatus(warrior->descIndex, 3);
        globalLogPtr->assignedWarriorList->removeWarrior(static_cast<uint8_t>(warrior->id));
        globalLogPtr->shiftPilots(row, -1);
        globalLogPtr->reorderWarriors();
        ResourcePoints -= globalLogPtr->purchaseDialog->unitCost;
        return;
    }

    warrior->assigned = 0;
    globalLogPtr->shiftPilots(warrior->inventoryBlock->listIndex, -1);
    globalLogPtr->reorderWarriors();
    globalLogPtr->purchaseScreen->createPilotInvBlock();
    globalLogPtr->purchaseScreen->setUpPilotInv(0, 1);
}

auto CompSellCallback(int confirmed, int32_t quantity) -> void
{
    _LogInventoryItem* item = globalCompPtr;

    if (confirmed == 0)
    {
        return;
    }

    item->count -= quantity;

    if (item->count == 0)
    {
        globalLogPtr->purchaseScreen->createCompInvBlock();
        globalLogPtr->purchaseScreen->setUpCompInv(0, 1);
    }
    else
    {
        item->inventoryBlock->drawBackground();
    }

    ResourcePoints -= globalLogPtr->purchaseDialog->unitCost * quantity;
}

auto VehicleSellCallback(int confirmed, int32_t) -> void
{
    Logistics* logistics = globalLogPtr;

    if (confirmed != 0)
    {
        int32_t index = globalLogPtr->forceVehicleList->getVehicleIndex(globalVehicle);
        globalLogPtr->forceVehicleList->removeVehicle(static_cast<uint8_t>(index));
        globalLogPtr->reorderVehicles();
        ResourcePoints -= globalLogPtr->purchaseDialog->unitCost;
        return;
    }

    globalVehicle->assigned = 0;
    logistics->reorderVehicles();
    globalLogPtr->purchaseScreen->createVhclInvBlock();
    globalLogPtr->purchaseScreen->setUpVhclInv(0, 1);
}

// DragIcon

auto DragIcon::display() -> void
{
    if (IsShowing() == 0)
    {
        return;
    }

    // Port fix: with the system cursor, the icon rides on the cursor instead of being drawn into the frame, so
    // it follows the mouse as closely as the cursor does.
    if (MCHardwareCursorCarry(this, lport()->frame()))
    {
        return;
    }

    VFX_pane_copy(lport()->frame(), 0, 0, framePane, 0, 0, -1);
}

auto DragIcon::Begin(int32_t xPos, int32_t yPos, int32_t width, int32_t height,
                     const std::function<void(lPort* surface)>& render) -> void
{
    init(xPos, yPos, width, height, nullptr, nullptr);
    PANE* surface = lport()->frame();
    VFX_pane_wipe(surface, 0);
    render(lport());
    const int32_t right = width - 1;
    const int32_t bottom = height - 1;
    VFX_line_draw(surface, 0, 0, right, 0, LD_DRAW, 0xea);
    VFX_line_draw(surface, 0, 1, 0, bottom - 1, LD_DRAW, 0xea);
    VFX_line_draw(surface, right, 1, right, bottom - 1, LD_DRAW, 0xea);
    VFX_line_draw(surface, 0, bottom, right, bottom, LD_DRAW, 0xea);
}

auto DragIcon::DrawFrom(lPort* surface, int32_t xPos, int32_t yPos, const std::function<void(lPort* port)>& draw)
    -> void
{
    PANE* pane = surface->frame();
    const PANE whole = *pane;
    pane->x0 = whole.x0 - xPos;
    pane->y0 = whole.y0 - yPos;
    draw(surface);
    *pane = whole;
}

// InventoryBlock

InventoryBlock::~InventoryBlock()
{
    InventoryBlock::destroy();
}

auto InventoryBlock::init(int32_t xPos, int32_t yPos, lPort* port) -> void
{
    lObject::init(xPos, yPos, 0xad, 0x2b, nullptr, port);
    enabled = 1;
}

auto InventoryBlock::destroy() -> void
{
    LogInvScreen::ForgetInfoSource(this);
    lObject::destroy();
}

auto InventoryBlock::drawBackground(lPort* port) -> void
{
    VFX_pane_copy(globalLogPtr->invBlockPort->frame(), 0, 0, port->frame(), 0, listIndex * winHeight, -1);
}

auto InventoryBlock::drawDisabled() -> void
{
}

auto InventoryBlock::setEnabled(int32_t enable) -> void
{
    enabled = enable;
    draw();
}

auto InventoryBlock::DrawRow(lPort* port, int32_t top) -> void
{
    VFX_pane_copy(globalLogPtr->invBlockPort->frame(), 0, 0, port->frame(), 0, top, -1);
}

auto InventoryBlock::OnBeginDrag(lPort* surface) -> void
{
    VFX_pane_wipe(surface->frame(), 0x10);
    DragIcon::DrawFrom(surface, 2, 1, [this](lPort* port) { DrawRow(port, 0); });
}

// Info box

auto DrawInfoDescription(lPort* port, int32_t width, int32_t height, char* description, int32_t xPos, int32_t yPos)
    -> void
{
    if (description == nullptr)
    {
        return;
    }

    auto* picture = new lPort;
    picture->init(width, height, 1);
    VFX_pane_wipe(picture->frame(), 0xff);
    application->textFormatter.process(reinterpret_cast<uint8_t*>(description), picture, 0, 0);
    picture->copyTo(port->frame(), xPos, yPos, 1);
    delete picture;
}

auto PrepareInfoDescription(char* description) -> void
{
    if (description != nullptr)
    {
        description[3] = '9';
    }
}

// MechInventoryBlock

MechInventoryBlock::~MechInventoryBlock()
{
    MechInventoryBlock::destroy();
}

auto MechInventoryBlock::init(LogMech* logMech) -> void
{
    diagramPort = nullptr;
    mech = logMech;
    InventoryBlock::init(0, 0, globalLogPtr->purchaseScreen->lport());
    listIndex = mech->nameIndex;
}

auto MechInventoryBlock::destroy() -> void
{
    if (diagramPort != nullptr)
    {
        delete diagramPort;
        diagramPort = nullptr;
    }

    mech = nullptr;
    InventoryBlock::destroy();
}

auto MechInventoryBlock::draw() -> void
{
    if (enabled == 0)
    {
        drawDisabled();
    }
}

auto MechInventoryBlock::drawBackground() -> void
{
    LogMech* logMech = mech;

    if (diagramPort == nullptr)
    {
        auto* port = new lPort;
        diagramPort = port;
        port->init(0x1c, 0x1e, 1);
        VFX_pane_wipe(port->frame(), 0x10);

        for (int32_t location = 0; location < 8; ++location)
        {
            globalLogPtr->drawMechBodyLoc(logMech, location, port, 2, 0);
        }

        // The battle rating bar along the left edge: 26 pixels at 18010.
        int32_t bar = static_cast<int32_t>(static_cast<double>(logMech->battleRating) * 0x1.d1c6674f499a1p-15 * 26.0);
        VFX_line_draw(port->frame(), 0, 0x1b, 0, 0x1b - bar, LD_DRAW, 0xe4);
        VFX_line_draw(port->frame(), 1, 0x1b, 1, 0x1b - bar, LD_DRAW, 0xe4);
    }
}

auto MechInventoryBlock::DrawRow(lPort* port, int32_t top) -> void
{
    InventoryBlock::DrawRow(port, top);
    LogMech* logMech = mech;
    writeText(yellowDropFont, port, 0x26, top + 7, logMech->fileName);
    char format[256];
    char text[256];
    cLoadString(thisInstance, 0x4e, format, 0xfe);
    std::snprintf(text, sizeof(text), format, static_cast<double>(logMech->curTonnage), logMech->weightClassName);
    writeText(blueDropFont, port, 0x26, top + 0x15, text);

    if (diagramPort != nullptr)
    {
        diagramPort->copyTo(port->frame(), 5, top + 2, 1);
    }
}

auto MechInventoryBlock::DrawInfo(lPort* port) -> void
{
    drawRowPicture(diagramPort, port);
    char tons[32];
    char text[84];
    cLoadString(thisInstance, 0x6e, tons, 0x1e);
    LogMech* logMech = mech;
    std::snprintf(text, sizeof(text), "%.0f %s", static_cast<double>(logMech->curTonnage), tons);
    writeText(yellowDropFont, port, 0x53, 0x193, text);
    writeText(yellowDropFont, port, 0x53, 0x19c, logMech->weightClassName);
    writeText(yellowDropFont, port, 0xa8, 0x193, logMech->chassisClassName);
    writeText(yellowDropFont, port, 0xa8, 0x19c, logMech->extraName1);
    writeText(yellowDropFont, port, 0xa8, 0x1a5, logMech->extraName2);
    std::snprintf(text, sizeof(text), "%d m/s", logMech->maxRunSpeed);
    writeText(yellowDropFont, port, 0x53, 0x1a5, text);
    DrawInfoDescription(port, 0xc3, 0x26, logMech->description, 8, 0x1b3);
}

auto MechInventoryBlock::deleteDiagram() -> void
{
    if (diagramPort != nullptr)
    {
        delete diagramPort;
    }

    diagramPort = nullptr;
}

auto MechInventoryBlock::handleEvent(aEvent* event) -> void
{
    if (!preHandleEvent(this, mechDrag, event))
    {
        return;
    }

    LogInvScreen* screen = invScreen();
    bool idle = mechDrag.dragging == 0 && mechDrag.carrying == 0;

    if (idle)
    {
        // The info block: the row's diagram, tonnage, classes, speed and description.
        screen->drawBlankInvInfoBlock(-1);
        PrepareInfoDescription(mech->description);
        screen->ShowInfo(InvInfoBox::Kind::Mech, this);

        if (event->type == 1 && mechDrag.carrying == 0)
        {
            // Left button down: drag the mech (it joins the force while dragged).
            playSample(0x35);
            application->showCursor(0);
            application->grab(this);
            mechDrag.dragging = 1;
            makeDragIcon(mechDrag, this, event);
            mech->assigned = 1;
            globalLogPtr->reorderMechs();
            screen->createMechInvBlock();
            screen->setUpMechInv(0, 0);
            raiseDragIcon();
        }
    }

    char text[256];
    char title[372];

    switch (event->type)
    {
        case 3:
        {
            // Right button down: pick the mech up.
            if (mechDrag.dragging != 0)
            {
                break;
            }

            mechDrag.carrying = 1;
            playSample(0x35);
            application->showCursor(0);
            application->grab(this);
            makeDragIcon(mechDrag, this, event);
            raiseDragIcon();
            break;
        }

        case 4:
        {
            // Left button up: drop the dragged mech.
            if (mechDrag.carrying != 0 || mechDrag.dragging == 0)
            {
                break;
            }

            application->showCursor(1);
            application->release();
            mechDrag.dragging = 0;
            deleteDragIcon();
            uint32_t sample = 0x33;

            if (onRepairScreen())
            {
                drawDropArt(screen, 0);

                if (overPane(screen->unitPane, event))
                {
                    if (globalLogPtr->forceMechList->numMechs + globalLogPtr->forceVehicleList->numVehicles > 0xf)
                    {
                        // The force is full.
                        playSample(0x33);
                        mech->assigned = 0;
                        globalLogPtr->reorderMechs();
                        screen->createMechInvBlock();
                        screen->setUpMechInv(0, 1);
                        showMessage(0x285, false);
                    }
                    else
                    {
                        playSample(0x34);
                        bumpDeploySlots(false);
                        LogMech* logMech = mech;
                        globalLogPtr->repairScreen->unitPane->addChild(logMech->repairBlock);
                        globalLogPtr->repairScreen->addMechToList(logMech);
                        globalLogPtr->repairScreen->selectMech(logMech);
                    }
                    break;
                }

                if (overPane(screen->inventoryPane, event))
                {
                    sample = 0x34;
                }
            }
            else
            {
                if (overPane(screen->unitPane, event))
                {
                    // Dropped on the store: offer to sell it.
                    playSample(0x34);
                    LogMech* logMech = mech;
                    globalMechPtr = logMech;

                    if (logMech->required != 0)
                    {
                        break;
                    }

                    cLoadString(thisInstance, weightClassString(logMech->curTonnage), text, 0xfe);
                    std::snprintf(title, sizeof(title), "%.0f Ton %s 'Mech", static_cast<double>(logMech->curTonnage),
                                  text);
                    logMech->calcMechCost(0);
                    int32_t price = salePrice(logMech->resourcePoints);
                    auto* picture = new lPort;
                    lPort* diagram = diagramPort;
                    picture->init(diagram->width(), diagram->height(), 1);
                    VFX_pane_wipe(picture->frame(), 0x10);
                    diagram->copyTo(picture->frame(), 2, 0, 1);
                    openSaleDialog(1, price, 1, logMech->fileName, title, picture, MechSellCallback);
                    delete picture;
                    break;
                }

                if (overPane(screen->inventoryPane, event))
                {
                    sample = 0x34;
                }
            }

            // Back to the inventory.
            playSample(sample);
            mech->assigned = 0;
            globalLogPtr->reorderMechs();
            screen->createMechInvBlock();
            screen->setUpMechInv(0, 1);
            break;
        }

        case 6:
        {
            // Right button up: put down the carried mech.
            if (application->grabbedObject() == nullptr || mechDrag.dragging != 0)
            {
                break;
            }

            playSample(0x34);
            mechDrag.carrying = 0;
            application->showCursor(1);
            application->release();
            deleteDragIcon();
            LogMech* logMech = mech;
            logMech->assigned = 1;
            globalLogPtr->reorderMechs();
            screen->createMechInvBlock();
            screen->setUpMechInv(0, 1);

            if (onRepairScreen())
            {
                if (globalLogPtr->forceMechList->numMechs + globalLogPtr->forceVehicleList->numVehicles < 0x10)
                {
                    drawDropArt(screen, 0);
                    bumpDeploySlots(false);
                    globalLogPtr->repairScreen->unitPane->addChild(logMech->repairBlock);
                    globalLogPtr->repairScreen->addMechToList(logMech);
                    globalLogPtr->repairScreen->selectMech(logMech);
                }
                else
                {
                    playSample(0x33);
                    logMech->assigned = 0;
                    globalLogPtr->reorderMechs();
                    screen->createMechInvBlock();
                    screen->setUpMechInv(0, 1);
                    showMessage(0x285, true);
                }
                break;
            }

            globalMechPtr = logMech;

            if (logMech->required != 0)
            {
                break;
            }

            cLoadString(thisInstance, weightClassString(logMech->curTonnage), text, 0xfe);
            std::snprintf(title, sizeof(title), "%.0f Ton %s 'Mech", static_cast<double>(logMech->curTonnage), text);
            logMech->calcMechCost(0);
            int32_t price = salePrice(logMech->resourcePoints);
            auto* picture = new lPort;
            lPort* diagram = diagramPort;
            picture->init(diagram->width(), diagram->height(), 1);
            VFX_pane_wipe(picture->frame(), 0x10);
            diagram->copyTo(picture->frame(), 2, 0, 1);
            openSaleDialog(1, price, 1, logMech->fileName, title, picture, MechSellCallback);
            delete picture;
            break;
        }

        case 7:
        {
            // Mouse move: the dragged icon follows; otherwise the ticker shows the row's help.
            if (mechDrag.dragging == 0)
            {
                if (event->key == 0)
                {
                    cLoadString(thisInstance, onPurchaseScreen() ? 0x2d : 0x31, text, 0xfe);
                    globalLogPtr->ticker->setString(text);
                }
            }
            else
            {
                mechDrag.x = event->x - 0xf;
                mechDrag.y = event->y - 0xf;
                globalLogPtr->dragIcon->moveTo(mechDrag.x, mechDrag.y, 0);
            }
            break;
        }
    }
}

// Logistics diagrams (their code sits in invblock.cpp)

auto Logistics::drawMechBodyLoc(LogMech* mech, int32_t location, lPort* port, int32_t xPos, int32_t yPos) -> void
{
    const LogMech::ArmorPoints& armor = mech->armor[location];
    int32_t percent = static_cast<int32_t>(static_cast<double>(armor.curArmor) / armor.maxArmor * 100.0f);

    if (location >= 1 && location <= 3)
    {
        // A torso counts its weaker side, front or rear.
        const LogMech::ArmorPoints& rear = mech->armor[location + 7];
        int32_t rearPercent = static_cast<int32_t>(static_cast<double>(rear.curArmor) / rear.maxArmor * 100.0f);

        if (rearPercent < percent)
        {
            percent = rearPercent;
        }
    }

    int32_t state = damageState(percent, mech->internals[location].curArmor);
    auto* diagram = new lPort;
    drawDiagram(globalLogPtr->mechIconShapes[mech->nameIndex], location, state, diagram, port, xPos, yPos);
    delete diagram;
}

auto Logistics::drawVehicleBodyLoc(LogVehicle* vehicle, int32_t location, lPort* port, int32_t xPos, int32_t yPos)
    -> void
{
    auto* diagram = new lPort;
    uint8_t maxArmor = vehicle->maxArmorPoints[location];

    if (maxArmor == 0)
    {
        // Port fix: the original returns here without freeing the port.
        delete diagram;
        return;
    }

    int32_t percent = static_cast<int32_t>(static_cast<double>(vehicle->curArmorPoints[location]) / maxArmor * 100.0f);
    int32_t state = damageState(percent, vehicle->curInternalStructure[location]);
    drawDiagram(globalLogPtr->vehicleIconShapes[vehicle->nameIndex], location, state, diagram, port, xPos, yPos);
    delete diagram;
}

// PilotInventoryBlock

PilotInventoryBlock::~PilotInventoryBlock()
{
    PilotInventoryBlock::destroy();
}

auto PilotInventoryBlock::init(LogWarrior* logWarrior) -> void
{
    mech = nullptr;
    vehicle = nullptr;
    warrior = logWarrior;
    InventoryBlock::init(0, 0, globalLogPtr->purchaseScreen->lport());
    listIndex = nextPilotRow;
    ++nextPilotRow;
    char fileName[256];
    std::snprintf(fileName, sizeof(fileName), "%slogart\\%02s", artPath, warrior->picture);
    auto* port = new lPort;
    portraitPort = port;
    port->init(fileName);
}

auto PilotInventoryBlock::destroy() -> void
{
    warrior = nullptr;

    if (portraitPort != nullptr)
    {
        delete portraitPort;
        portraitPort = nullptr;
    }

    InventoryBlock::destroy();
}

auto PilotInventoryBlock::draw() -> void
{
    if (enabled == 0)
    {
        drawDisabled();
    }
}

auto PilotInventoryBlock::drawBackground() -> void
{
    // On the repair screen a pilot can only go to the selected mech, and only when it has none.
    if (onRepairScreen())
    {
        LogMech* selected = globalLogPtr->repairScreen->selectedMech;
        greyedOut = 1;

        if (selected != nullptr && selected->pilotIndex < 0)
        {
            greyedOut = 0;
        }
    }
    else
    {
        greyedOut = 0;
    }

    moveTo(0, winHeight * listIndex, 0);
}

auto PilotInventoryBlock::DrawRow(lPort* port, int32_t top) -> void
{
    lPort* row = newRowPicture(globalLogPtr->invBlockPort);
    portraitPort->copyTo(row->frame(), 3, 2, 1);
    writeText(yellowDropFont, row, 0x26, 7, warrior->callsign);
    char text[256];
    char pilot[256];
    loadRankName(warrior->rank, text);
    cLoadString(thisInstance, 0x287, pilot, 0xfe);
    std::strcat(text, " ");
    std::strcat(text, pilot);
    writeText(blueDropFont, row, 0x26, 0x15, text);

    if (greyedOut != 0)
    {
        globalLogPtr->darken(0, g_logistic_fadetable, row);
    }

    row->copyTo(port->frame(), 0, top, 1);
    delete row;
}

auto PilotInventoryBlock::DrawInfo(lPort* port) -> void
{
    LogWarrior* logWarrior = warrior;
    globalLogPtr->drawPilotSkillBar(logWarrior, 3, 0x56, 0x192, 0, 0x36, winHeight, port);
    globalLogPtr->drawPilotSkillBar(logWarrior, 0, 0x56, 0x19b, 0, 0x36, winHeight, port);
    globalLogPtr->drawPilotSkillBar(logWarrior, 1, 0x56, 0x1a4, 0, 0x36, winHeight, port);
    globalLogPtr->drawPilotSkillBar(logWarrior, 2, 0x56, 0x1ad, 0, 0x36, winHeight, port);
    portraitPort->copyTo(port->frame(), 9, 0x196, 1);
    char text[256];
    loadRankName(logWarrior->rank, text);
    writeText(yellowDropFont, port, 0x9c, 0x19a, text);
    // One pip per point of health left.
    int32_t x = 0xf;

    for (int32_t pip = 0; static_cast<float>(pip) < logWarrior->health; ++pip, x += 3)
    {
        AG_pixel_write(port->frame(), x, 0x192, 0xcf);
        AG_pixel_write(port->frame(), x + 1, 0x192, 0xcf);
        AG_pixel_write(port->frame(), x + 1, 0x193, 0xee);
        AG_pixel_write(port->frame(), x, 0x193, 0xcf);
    }

    DrawInfoDescription(port, 0xc3, 0x22, logWarrior->description, 7, 0x1b8);
}

auto PilotInventoryBlock::handleEvent(aEvent* event) -> void
{
    if (!preHandleEvent(this, pilotDrag, event))
    {
        return;
    }

    LogInvScreen* screen = invScreen();
    bool idle = pilotDrag.dragging == 0 && pilotDrag.carrying == 0;
    char text[256];

    if (idle)
    {
        // The info block: skills, portrait, rank, wounds and description.
        screen->drawBlankInvInfoBlock(-1);
        PrepareInfoDescription(warrior->description);
        screen->ShowInfo(InvInfoBox::Kind::Pilot, this);
    }

    switch (event->type)
    {
        case 1:
        {
            // Left button down: drag the pilot.
            if (greyedOut != 0 || pilotDrag.carrying != 0)
            {
                break;
            }

            soundSystem->playPilotSpeech(warrior->pilotAudio, 10);
            application->showCursor(0);
            application->grab(this);
            pilotDrag.dragging = 1;
            makeDragIcon(pilotDrag, this, event);
            LogWarrior* logWarrior = warrior;
            logWarrior->assigned = 1;
            globalLogPtr->reorderWarriors();
            globalLogPtr->shiftPilots(logWarrior->inventoryBlock->listIndex, 1);
            screen->createPilotInvBlock();
            screen->setUpPilotInv(0, 0);
            raiseDragIcon();
            break;
        }

        case 3:
        {
            // Right button down: pick the pilot up.
            if (greyedOut != 0 || pilotDrag.dragging != 0)
            {
                break;
            }

            pilotDrag.carrying = 1;
            soundSystem->playPilotSpeech(warrior->pilotAudio, 10);
            application->showCursor(0);
            application->grab(this);
            makeDragIcon(pilotDrag, this, event);
            raiseDragIcon();
            break;
        }

        case 4:
        {
            // Left button up: drop the dragged pilot.
            if (pilotDrag.carrying != 0 || application->grabbedObject() == nullptr)
            {
                break;
            }

            application->showCursor(1);
            application->release();
            pilotDrag.dragging = 0;
            deleteDragIcon();
            drawDropArt(screen, 1);

            if (overPane(screen->unitPane, event))
            {
                LogWarrior* logWarrior = warrior;

                if (onPurchaseScreen())
                {
                    // Dropped on the store: offer to sell the pilot.
                    pilotToSell = logWarrior;
                    auto* picture = new lPort;
                    std::snprintf(text, sizeof(text), "%slogart\\%s", artPath, logWarrior->picture);
                    picture->init(text);
                    int32_t price = salePrice(globalLogPtr->pilotCosts[logWarrior->rank]);
                    cLoadString(thisInstance, 0x5f, text, 0xfe);
                    globalLogPtr->purchaseDialog->init(3, -price, 1, logWarrior->callsign, text, picture);
                    delete picture;
                    globalLogPtr->purchaseDialog->setPort(globalLogPtr->currentScreen->lport());
                    globalLogPtr->purchaseDialog->setCallback(PilotSellCallback);
                    globalLogPtr->purchaseDialog->activate();
                    break;
                }

                // Dropped on a mech: it must be the selected one.
                ScrollPane* pane = screen->unitPane;
                int32_t index = (event->y - pane->globalY() + pane->getScrollOffset()) / 0x70;

                if (index < pane->numberOfChildren() && index < globalLogPtr->forceMechList->getMechCount())
                {
                    LogMech* target = nullptr;
                    globalLogPtr->forceMechList->getMechInfo(index, target);

                    if (target != nullptr && target == globalLogPtr->repairScreen->selectedMech)
                    {
                        PilotInventoryBlock* block = logWarrior->inventoryBlock;
                        block->mech = target;
                        int32_t row = block->listIndex;
                        globalLogPtr->setPilot(index, row);
                        target->repairBlock->drawBR(nullptr);
                        globalLogPtr->setPilot(index, row);
                        screen->createPilotInvBlock();
                        screen->setUpPilotInv(0, 1);
                        soundSystem->playPilotSpeech(logWarrior->pilotAudio, 2);
                        break;
                    }
                }
            }

            // Back to the inventory.
            LogWarrior* logWarrior = warrior;
            logWarrior->assigned = 0;
            globalLogPtr->shiftPilots(logWarrior->inventoryBlock->listIndex, -1);
            globalLogPtr->reorderWarriors();
            screen->createPilotInvBlock();
            screen->setUpPilotInv(0, 1);
            playSample(overPane(screen->inventoryPane, event) ? 0x34 : 0x33);
            break;
        }

        case 6:
        {
            // Right button up: put down the carried pilot.
            if (pilotDrag.dragging != 0)
            {
                break;
            }

            pilotDrag.carrying = 0;

            if (application->grabbedObject() == nullptr)
            {
                break;
            }

            application->showCursor(1);
            application->release();
            deleteDragIcon();
            LogWarrior* logWarrior = warrior;
            logWarrior->assigned = 1;
            globalLogPtr->reorderWarriors();
            globalLogPtr->shiftPilots(logWarrior->inventoryBlock->listIndex, 1);
            screen->createPilotInvBlock();
            screen->setUpPilotInv(0, 1);
            drawDropArt(screen, 1);

            if (onPurchaseScreen())
            {
                pilotToSell = logWarrior;
                auto* picture = new lPort;
                std::snprintf(text, sizeof(text), "%slogart\\%s", artPath, logWarrior->picture);
                picture->init(text);
                int32_t price = salePrice(globalLogPtr->pilotCosts[logWarrior->rank]);
                cLoadString(thisInstance, 0x5f, text, 0xfe);
                globalLogPtr->purchaseDialog->init(3, -price, 1, logWarrior->callsign, text, picture);
                delete picture;
                globalLogPtr->purchaseDialog->setPort(globalLogPtr->currentScreen->lport());
                globalLogPtr->purchaseDialog->setCallback(PilotSellCallback);
                globalLogPtr->purchaseDialog->activate();
                break;
            }

            LogMech* selected = globalLogPtr->repairScreen->selectedMech;

            if (selected != nullptr)
            {
                // Onto the selected mech.
                PilotInventoryBlock* block = logWarrior->inventoryBlock;
                block->mech = selected;
                int32_t row = block->listIndex;
                globalLogPtr->setPilot(selected->repairBlock->slotIndex, row);
                MechRepairBlock* repairBlock = selected->repairBlock;
                repairBlock->drawBR(nullptr);
                globalLogPtr->setPilot(repairBlock->slotIndex, row);
                screen->createPilotInvBlock();
                screen->setUpPilotInv(0, 1);
                soundSystem->playPilotSpeech(logWarrior->pilotAudio, 2);
                break;
            }

            // No mech selected: back to the inventory. The block is this one still (the lists were only renumbered).
            logWarrior = warrior;
            logWarrior->assigned = 0;
            globalLogPtr->shiftPilots(logWarrior->inventoryBlock->listIndex, -1);
            globalLogPtr->reorderWarriors();
            screen->createPilotInvBlock();
            screen->setUpPilotInv(0, 1);
            playSample(overPane(screen->inventoryPane, event) ? 0x34 : 0x33);
            break;
        }

        case 7:
        {
            // Mouse move: the dragged icon follows; otherwise the ticker shows the row's help.
            if (pilotDrag.dragging == 0)
            {
                if (event->key == 0)
                {
                    cLoadString(thisInstance, onPurchaseScreen() ? 0x2e : 0x33, text, 0xfe);
                    globalLogPtr->ticker->setString(text);
                }
            }
            else
            {
                pilotDrag.x = event->x - 0xf;
                pilotDrag.y = event->y - 0xf;
                globalLogPtr->dragIcon->moveTo(pilotDrag.x, pilotDrag.y, 0);
            }
            break;
        }
    }
}

// VehicleInventoryBlock

VehicleInventoryBlock::~VehicleInventoryBlock()
{
    VehicleInventoryBlock::destroy();
}

auto VehicleInventoryBlock::init(LogVehicle* logVehicle) -> void
{
    vehicle = logVehicle;
    InventoryBlock::init(0, 0, globalLogPtr->purchaseScreen->lport());
    listIndex = vehicle->nameIndex;
    char text[256];
    cLoadString(thisInstance, weightClassString(vehicle->curTonnage), text, 0xfe);
    weightClassText = heapString(text);
    // The armor rating from the chassis (armor) tonnage.
    float armor = vehicle->armorTonnage;
    uint32_t id;

    if (armor <= 2.0f)
    {
        id = 100;
    }
    else if (armor <= 7.0f)
    {
        id = 0x4f;
    }
    else if (armor <= 12.0f)
    {
        id = 0x65;
    }
    else if (armor <= 17.0f)
    {
        id = 0x51;
    }
    else
    {
        id = 0x66;
    }

    cLoadString(thisInstance, id, text, 0xf);
    armorText = heapString(text);
}

auto VehicleInventoryBlock::destroy() -> void
{
    vehicle = nullptr;
    logFree(weightClassText);
    weightClassText = nullptr;
    logFree(armorText);
    armorText = nullptr;
    InventoryBlock::destroy();
}

auto VehicleInventoryBlock::draw() -> void
{
}

auto VehicleInventoryBlock::handleEvent(aEvent* event) -> void
{
    if (!preHandleEvent(this, vehicleDrag, event))
    {
        return;
    }

    LogInvScreen* screen = invScreen();
    bool idle = vehicleDrag.dragging == 0 && vehicleDrag.carrying == 0;
    char text[256];

    if (idle)
    {
        // The info block: diagram, tonnage, classes, speed and description.
        screen->drawBlankInvInfoBlock(-1);
        PrepareInfoDescription(vehicle->description);
        screen->ShowInfo(InvInfoBox::Kind::Vehicle, this);
    }

    switch (event->type)
    {
        case 1:
        {
            if (vehicleDrag.carrying != 0)
            {
                break;
            }

            [[fallthrough]];
        }
        case 3:
        {
            // Left button down drags the vehicle (it joins the force while dragged); right button down picks it up.
            if (vehicleDrag.dragging != 0)
            {
                break;
            }

            playSample(0x35);
            application->showCursor(0);
            application->grab(this);
            makeDragIcon(vehicleDrag, this, event);

            if (event->type == 1)
            {
                vehicleDrag.dragging = 1;
                vehicle->assigned = 1;
                globalLogPtr->reorderVehicles();
                screen->createVhclInvBlock();
                screen->setUpVhclInv(0, 0);
            }
            else
            {
                vehicleDrag.carrying = 1;
            }

            raiseDragIcon();
            [[fallthrough]];
        }
        case 7:
        {
            // Mouse move: the dragged icon follows; otherwise the ticker shows the row's help.
            if (vehicleDrag.dragging == 0)
            {
                if (event->key == 0)
                {
                    cLoadString(thisInstance, onPurchaseScreen() ? 0x2d : 0x47, text, 0xfe);
                    globalLogPtr->ticker->setString(text);
                }
            }
            else
            {
                vehicleDrag.y = event->y - 0xf;
                vehicleDrag.x = event->x - 0xf;
                globalLogPtr->dragIcon->moveTo(vehicleDrag.x, vehicleDrag.y, 0);
            }
            break;
        }

        case 4:
        {
            // Left button up: drop the dragged vehicle.
            if (vehicleDrag.carrying != 0 || vehicleDrag.dragging == 0)
            {
                break;
            }

            application->showCursor(1);
            application->release();
            vehicleDrag.dragging = 0;
            deleteDragIcon();
            LogVehicle* logVehicle = vehicle;

            if (onRepairScreen())
            {
                if (globalLogPtr->forceMechList->numMechs + globalLogPtr->forceVehicleList->numVehicles > 0xf)
                {
                    // The force is full.
                    playSample(0x33);
                    logVehicle->assigned = 0;
                    globalLogPtr->reorderVehicles();
                    screen->createVhclInvBlock();
                    screen->setUpVhclInv(0, 1);
                    showMessage(0x285, true);
                    break;
                }

                drawDropArt(screen, 3);

                if (overPane(screen->unitPane, event))
                {
                    playSample(0x34);
                    globalLogPtr->repairScreen->unitPane->addChild(logVehicle->repairBlock);
                    // The vehicle path bumps the drop slots' second field.
                    bumpDeploySlots(true);
                    globalLogPtr->repairScreen->addVehicleToList(logVehicle);
                    globalLogPtr->repairScreen->selectVehicle(logVehicle);
                    break;
                }

                if (overPane(screen->inventoryPane, event))
                {
                    playSample(0x34);
                    logVehicle->assigned = 0;
                }
                else
                {
                    playSample(0x33);
                    logVehicle->assigned = 0;
                }
            }
            else if (overPane(screen->unitPane, event))
            {
                globalVehicle = logVehicle;

                if (logVehicle->required == 0)
                {
                    // Dropped on the store: offer to sell it.
                    playSample(0x34);
                    int32_t price = salePrice(logVehicle->vehicleResourcePoints);
                    openSaleDialog(7, price, 1, logVehicle->fileName, nullptr, picturePort, VehicleSellCallback);
                    break;
                }

                playSample(overPane(screen->inventoryPane, event) ? 0x34 : 0x33);
                logVehicle->assigned = 0;
            }
            else
            {
                playSample(0x33);
                logVehicle->assigned = 0;
            }

            globalLogPtr->reorderVehicles();
            screen->createVhclInvBlock();
            screen->setUpVhclInv(0, 1);
            break;
        }

        case 6:
        {
            // Right button up: put down the carried vehicle.
            if (vehicleDrag.dragging != 0)
            {
                break;
            }

            vehicleDrag.carrying = 0;

            if (application->grabbedObject() == nullptr)
            {
                break;
            }

            application->showCursor(1);
            application->release();
            deleteDragIcon();
            LogVehicle* logVehicle = vehicle;
            logVehicle->assigned = 1;
            globalLogPtr->reorderVehicles();
            screen->createVhclInvBlock();
            screen->setUpVhclInv(0, 1);

            if (onRepairScreen())
            {
                if (globalLogPtr->forceMechList->numMechs + globalLogPtr->forceVehicleList->numVehicles < 0x10)
                {
                    drawDropArt(screen, 3);
                    playSample(0x34);
                    globalLogPtr->repairScreen->unitPane->addChild(logVehicle->repairBlock);
                    bumpDeploySlots(true);
                    globalLogPtr->repairScreen->addVehicleToList(logVehicle);
                    globalLogPtr->repairScreen->selectVehicle(logVehicle);
                    break;
                }

                playSample(0x33);
                logVehicle->assigned = 0;
                globalLogPtr->reorderVehicles();
                screen->createVhclInvBlock();
                screen->setUpVhclInv(0, 1);
                showMessage(0x285, true);
                break;
            }

            globalVehicle = logVehicle;

            if (logVehicle->required == 0)
            {
                playSample(0x34);
                int32_t price = salePrice(logVehicle->vehicleResourcePoints);
                openSaleDialog(7, price, 1, logVehicle->fileName, nullptr, picturePort, VehicleSellCallback);
                break;
            }

            playSample(0x33);
            logVehicle->assigned = 0;
            globalLogPtr->reorderVehicles();
            screen->createVhclInvBlock();
            screen->setUpVhclInv(0, 1);
            break;
        }
    }
}

auto VehicleInventoryBlock::drawBackground() -> void
{
    LogVehicle* logVehicle = vehicle;

    if (picturePort == nullptr)
    {
        auto* port = new lPort;
        picturePort = port;
        port->init(0x1c, 0x1e, 1);
        VFX_pane_wipe(port->frame(), 0x10);

        for (int32_t location = 0; location < 5; ++location)
        {
            globalLogPtr->drawVehicleBodyLoc(logVehicle, location, port, 0, 0);
        }
    }
}

auto VehicleInventoryBlock::DrawRow(lPort* port, int32_t top) -> void
{
    InventoryBlock::DrawRow(port, top);
    LogVehicle* logVehicle = vehicle;
    writeText(yellowDropFont, port, 0x26, top + 7, logVehicle->fileName);
    char format[256];
    char text[256];
    cLoadString(thisInstance, 0x53, format, 0xfe);
    std::snprintf(text, sizeof(text), format, static_cast<double>(logVehicle->curTonnage), weightClassText);
    writeText(blueDropFont, port, 0x26, top + 0x15, text);

    if (picturePort != nullptr)
    {
        picturePort->copyTo(port->frame(), 5, top + 2, 1);
    }
}

auto VehicleInventoryBlock::DrawInfo(lPort* port) -> void
{
    drawRowPicture(picturePort, port);
    char tons[256];
    char text[256];
    cLoadString(thisInstance, 0x6e, tons, 0xfe);
    std::snprintf(text, sizeof(text), "%.0f %s", static_cast<double>(vehicle->curTonnage), tons);
    writeText(yellowDropFont, port, 0x53, 0x193, text);
    writeText(yellowDropFont, port, 0x53, 0x19c, weightClassText);
    writeText(yellowDropFont, port, 0xa9, 0x193, armorText);
    std::snprintf(text, sizeof(text), "%d m/s", vehicle->maxMoveSpeed);
    writeText(yellowDropFont, port, 0x53, 0x1a5, text);
    DrawInfoDescription(port, 0xc3, 0x26, vehicle->description, 8, 0x1b3);
}

// CompInventoryBlock

CompInventoryBlock::~CompInventoryBlock()
{
    CompInventoryBlock::destroy();
}

auto CompInventoryBlock::init(_LogInventoryItem* newItem) -> void
{
    item = newItem;
    InventoryBlock::init(0, 0, globalLogPtr->purchaseScreen->lport());
    MasterComponent* list = MasterComponentList;
    MasterComponent& component = list[item->masterID];
    int32_t form = component.form;
    tonnage = component.tonnage;

    if (form == COMPONENT_FORM_WEAPON_BALLISTIC || form == COMPONENT_FORM_WEAPON_MISSILE)
    {
        tonnage = list[component.ammoMasterId].tonnage + tonnage;
    }

    char text[256];
    cLoadString(thisInstance, 0x6e, text, 0xfe);
    std::snprintf(weightText, sizeof(weightText), "%.1f %s", static_cast<double>(tonnage), text);

    if (form == COMPONENT_FORM_WEAPON_ENERGY || form == COMPONENT_FORM_WEAPON_BALLISTIC ||
        form == COMPONENT_FORM_WEAPON_MISSILE)
    {
        // Weapons: the long range as a word, damage and recycle time.
        float range = component.weaponRange[3];
        cLoadString(thisInstance, range < 76.0f ? 0x55 : range < 151.0f ? 0x50 : 0x6d, text, 0xfe);
        setText(rangeText, text);
        double damage = component.damage;

        if (component.weaponFlags == 4)
        {
            damage *= 3.0;
        }

        std::snprintf(damageText, sizeof(damageText), "%.2f", damage);
        std::snprintf(recycleText, sizeof(recycleText), "%.2f s", static_cast<double>(component.recycleTime));
    }
    else
    {
        cLoadString(thisInstance, 0x6c, text, 0xfe);

        if (form == COMPONENT_FORM_PROBE)
        {
            setText(rangeText, text);
        }
        else
        {
            // Original behaviour (OB-078): other equipment than ECM and sensors formats the item pointer's bits as
            // the range; for a heap address that is a denormal, so it read "0.0 m".
            float range = 0.0f;

            if (form == COMPONENT_FORM_ECM || form == COMPONENT_FORM_SENSOR)
            {
                range = component.rangeOrHeat;
            }

            std::snprintf(rangeText, sizeof(rangeText), "%.1f m", static_cast<double>(range));
        }

        setText(damageText, text);
        setText(recycleText, text);
    }

    // The icon: the range colour's block background, name, a caption and the component picture.
    MasterComponent& again = MasterComponentList[item->masterID];
    form = again.form;
    float range = 0.0f;
    tonnage = again.tonnage;

    if (form == COMPONENT_FORM_WEAPON_BALLISTIC || form == COMPONENT_FORM_WEAPON_ENERGY ||
        form == COMPONENT_FORM_WEAPON_MISSILE)
    {
        range = again.weaponRange[3];
    }

    if (form == COMPONENT_FORM_WEAPON_BALLISTIC || form == COMPONENT_FORM_WEAPON_MISSILE)
    {
        tonnage = MasterComponentList[again.ammoMasterId].tonnage + tonnage;
    }

    auto* icon = new lPort;
    iconPort = icon;
    auto* own = new lPort;
    ownPort = own;
    const char* background = range < 76.0f ? "greeninv.tga" : range < 151.0f ? "blueinv.tga" : "redinv.tga";
    std::snprintf(text, sizeof(text), "%slogart\\%s", artPath, background);
    icon->init(text);
    own->init(text);
    writeText(yellowDropFont, icon, 0x26, 7, again.name);
    cLoadString(thisInstance, 0x37d, text, 0xfe);
    writeText(blueDropFont, icon, 0x26, 0x15, text);
    std::snprintf(text, sizeof(text), "%slogart\\lscicc%02d.tga", artPath, item->rangeIndex);
    auto* picture = new lPort;
    picture->init(text);
    picture->copyTo(icon->frame(), 3, 2, 1);
    delete picture;

    if (again.techBase == 1)
    {
        // Clan technology: a small mark in the range colour.
        int32_t color = again.weaponRange[3] < 76.0f ? 0xe : again.weaponRange[3] < 151.0f ? 0xe5 : 0xee;
        VFX_line_draw(icon->frame(), 5, 7, 5, 8, LD_DRAW, color);
        VFX_line_draw(icon->frame(), 6, 5, 6, 8, LD_DRAW, color);
        VFX_line_draw(icon->frame(), 7, 4, 7, 8, LD_DRAW, color);
        VFX_line_draw(icon->frame(), 8, 5, 8, 8, LD_DRAW, color);
        VFX_line_draw(icon->frame(), 9, 7, 9, 8, LD_DRAW, color);
    }
}

auto CompInventoryBlock::destroy() -> void
{
    item = nullptr;

    if (iconPort != nullptr)
    {
        delete iconPort;
        iconPort = nullptr;
    }

    if (ownPort != nullptr)
    {
        delete ownPort;
        ownPort = nullptr;
    }

    InventoryBlock::destroy();
}

auto CompInventoryBlock::handleEvent(aEvent* event) -> void
{
    if (!preHandleEvent(this, compDrag, event))
    {
        return;
    }

    LogInvScreen* screen = invScreen();
    char text[256];

    if (compDrag.carrying == 0 && compDrag.dragging == 0)
    {
        // The info block: picture, range, damage, recycle time and description.
        screen->drawBlankInvInfoBlock(-1);
        PrepareInfoDescription(item->description);
        screen->ShowComponentInfo(this, false);
    }

    // Where a drop that didn't mount or sell ends: the sound, then the copy goes back to the row.
    auto returnToInventory = [&](uint32_t sample)
    {
        playSample(sample);

        if (!onPurchaseScreen() && compDrag.carrying == 0)
        {
            ++item->count;
        }

        if (item->count > 1)
        {
            drawBackground();
        }
        else
        {
            screen->createCompInvBlock();
            screen->setUpCompInv(0, 1);
        }

        compDrag.carrying = 0;
    };

    auto openSale = [&]()
    {
        playSample(0x34);
        auto* picture = new lPort;
        _LogInventoryItem* sold = item;
        std::snprintf(text, sizeof(text), "%slogart\\lscicc%02d.tga", artPath, sold->rangeIndex);
        picture->init(text);
        MasterComponent& component = MasterComponentList[sold->masterID];
        int32_t price = salePrice(component.resourcePoints);
        globalCompPtr = sold;
        globalLogPtr->purchaseDialog->init(5, -price, sold->count, component.name, nullptr, picture);
        delete picture;
        globalLogPtr->purchaseDialog->setPort(globalLogPtr->currentScreen->lport());
        globalLogPtr->purchaseDialog->setCallback(CompSellCallback);
        globalLogPtr->purchaseDialog->activate();
    };

    switch (event->type)
    {
        case 1:
        {
            // Left button down: drag one copy (off the row on the repair screen).
            if (cantMount != 0 || compDrag.carrying != 0)
            {
                break;
            }

            playSample(0x35);
            application->showCursor(0);
            application->grab(this);
            compDrag.dragging = 1;
            makeDragIcon(compDrag, this, event);

            if (screen != globalLogPtr->purchaseScreen)
            {
                if (--item->count != 0)
                {
                    drawBackground();
                }
                else
                {
                    screen->createCompInvBlock();
                    screen->setUpCompInv(0, 0);
                }
            }

            raiseDragIcon();
            break;
        }

        case 3:
        {
            // Right button down: pick one copy up.
            if (cantMount != 0 || compDrag.dragging != 0)
            {
                break;
            }

            compDrag.carrying = 1;
            playSample(0x35);
            application->showCursor(0);
            application->grab(this);
            makeDragIcon(compDrag, this, event);

            if (screen != globalLogPtr->purchaseScreen && --item->count != 0)
            {
                drawBackground();
            }

            raiseDragIcon();
            break;
        }

        case 4:
        {
            // Left button up: drop the dragged copy.
            if (compDrag.dragging == 0)
            {
                break;
            }

            application->showCursor(1);
            application->release();
            compDrag.dragging = 0;
            deleteDragIcon();

            if (onRepairScreen())
            {
                drawDropArt(screen, 2);
                ScrollPane* unitPane = screen->unitPane;

                if (overPane(unitPane, event))
                {
                    // Onto a mech: it must be the selected one.
                    int32_t index = (event->y - unitPane->globalY() + unitPane->getScrollOffset()) / 0x70;

                    if (index < unitPane->numberOfChildren() && index < globalLogPtr->forceMechList->numMechs)
                    {
                        LogMech* target = nullptr;
                        globalLogPtr->forceMechList->getMechInfo(index, target);

                        if (target != nullptr && target == globalLogPtr->repairScreen->selectedMech &&
                            mountComponent(item, target))
                        {
                            break;
                        }
                    }

                    returnToInventory(0x33);
                    break;
                }

                returnToInventory(
                    overPane(screen->inventoryPane, screen->inventoryPane, screen->unitPane, event) ? 0x34 : 0x33);
                break;
            }

            if (overPane(screen->unitPane, event))
            {
                openSale();
                break;
            }

            returnToInventory(overPane(screen->inventoryPane, screen->unitPane, screen->unitPane, event) ? 0x34 : 0x33);
            break;
        }

        case 6:
        {
            // Right button up: put down the carried copy.
            if (compDrag.carrying == 0)
            {
                break;
            }

            application->showCursor(1);
            application->release();
            deleteDragIcon();
            screen->createCompInvBlock();
            screen->setUpCompInv(0, 1);

            if (onRepairScreen())
            {
                // Onto the selected mech.
                // Original behaviour (OB-079): when it can't be mounted, the copy taken off the row at the pick-up is
                // not given back (the count is only restored while nothing is carried), so it is lost.
                LogMech* selected = globalLogPtr->repairScreen->selectedMech;

                if (selected != nullptr && mountComponent(item, selected))
                {
                    compDrag.carrying = 0;
                    break;
                }

                returnToInventory(0x33);
                break;
            }

            openSale();
            compDrag.carrying = 0;
            break;
        }

        case 7:
        {
            // Mouse move: the dragged icon follows (a carried one only updates its position); otherwise the ticker
            // shows the row's help.
            if (compDrag.dragging != 0)
            {
                compDrag.y = event->y - 0xf;
                compDrag.x = event->x - 0xf;
                globalLogPtr->dragIcon->moveTo(compDrag.x, compDrag.y, 0);
            }
            else if (compDrag.carrying != 0)
            {
                compDrag.x = event->x - 0xf;
                compDrag.y = event->y - 0xf;
            }
            else if (event->key == 0)
            {
                cLoadString(thisInstance, onPurchaseScreen() ? 0x2d : 0x32, text, 0xfe);
                globalLogPtr->ticker->setString(text);
            }
            break;
        }
    }
}

auto CompInventoryBlock::drawBackground() -> void
{
    if (listIndex < 0)
    {
        ShowGUIWindow(0);
        return;
    }

    ShowGUIWindow(1);
    cantMount = 0;

    if (!onPurchaseScreen())
    {
        // On the repair screen: can it go on the selected mech?
        LogMech* selected = globalLogPtr->repairScreen->selectedMech;
        uint32_t masterID = item->masterID;
        int32_t form = MasterComponentList[masterID].form;

        if (selected == nullptr)
        {
            cantMount = 1;
        }
        else if (static_cast<double>(selected->curTonnage) - selected->usedTonnage < tonnage)
        {
            cantMount = 1;
        }
        else
        {
            // One ECM, sensor or probe per mech.
            if (form == COMPONENT_FORM_ECM || form == COMPONENT_FORM_SENSOR || form == COMPONENT_FORM_PROBE)
            {
                for (_LogInventoryItem* mounted = selected->inventory->items; mounted != nullptr;
                     mounted = mounted->next)
                {
                    if (form == MasterComponentList[mounted->masterID].form)
                    {
                        cantMount = 1;
                    }
                }
            }

            // Some components only fit the mechs of name index 5, 0xe and 0x10.
            for (int32_t i = 0; cantMount == 0 && i < numRestrictedComponents; ++i)
            {
                if (restrictedComps[i] == static_cast<int32_t>(masterID) && selected->nameIndex != 0xe &&
                    selected->nameIndex != 0x10 && selected->nameIndex != 5)
                {
                    cantMount = 1;
                }
            }
        }
    }
}

auto CompInventoryBlock::DrawRow(lPort* port, int32_t top) -> void
{
    // Put together apart, as the original did in the block's own picture, then copied opaque.
    lPort* row = newRowPicture(iconPort);
    char text[256];
    std::snprintf(text, sizeof(text), "%d", item->count);
    writeText(blueDropFont, row, 0x67, 0x15, text);

    if (cantMount != 0)
    {
        globalLogPtr->darken(0, g_logistic_fadetable, row);
    }

    VFX_pane_copy(row->frame(), 0, 0, port->frame(), 0, top, -1);
    delete row;
}
