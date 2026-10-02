#include "stdafx.h"
#include "logistics/purchase.h"
#include "gui/afont.h"
#include "gui/scrlpane.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/heap.h"
#include "lib/inifile.h"
#include "logistics/invblock.h"
#include "logistics/logbri.h"
#include "logistics/logdlg.h"
#include "logistics/loggen.h"
#include "logistics/logmain.h"
#include "logistics/logpur.h"
#include "logistics/logrep.h"
#include "logistics/ticker.h"
#include "main/logistics.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "object/cmponent.h"
#include "object/mech.h"
#include "object/objtype.h"
#include "sound/soundsys.h"
#include "vfx/vfxfuncs.h"

int32_t ResourcePoints = 0;
MechPurchaseBlock* globalMechPurchaseBlock = nullptr;
PilotPurchaseBlock* globalPilotPurchaseBlock = nullptr;
_LogInventoryItem* globalItemPtr = nullptr;
VehiclePurchaseBlock* globalVehicleBlockPtr = nullptr;
int32_t bodyTrans[8] = {7, 6, 4, 5, 0, 1, 2, 3};
char objectDesc[] = "desc.fit";

namespace
{
    /// <summary>The drag state of one kind of shop row.</summary>
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

    /// <summary>The mech rows' drag (DAT_008086f0 dragging, f4/f8 x/y, fc carrying).</summary>
    DragState mechDrag;
    /// <summary>The vehicle rows' drag (DAT_00808704 dragging, 708/70c x/y, 710 carrying).</summary>
    DragState vehicleDrag;
    /// <summary>The component rows' drag (DAT_00808718 dragging, 71c/720 x/y, 728 carrying).</summary>
    DragState compDrag;
    /// <summary>The pilot rows' drag (DAT_0080872c dragging, 730/734 x/y, 738 carrying).</summary>
    DragState pilotDrag;

    /// <summary>The body location blocks of a mech profile, in location order.</summary>
    const char* const bodyLocationNames[8] = {"Head",    "CenterTorso", "LeftTorso", "RightTorso",
                                              "LeftArm", "RightArm",    "LeftLeg",   "RightLeg"};

    /// <summary>The armor locations of a mech profile's MaxArmorPoints and CurArmorPoints blocks.</summary>
    const char* const armorLocationNames[11] = {
        "Head",    "CenterTorso", "LeftTorso",       "RightTorso",    "LeftArm",       "RightArm",
        "LeftLeg", "RightLeg",    "RearCenterTorso", "RearLeftTorso", "RearRightTorso"};

    void* logAlloc(uint32_t size)
    {
        return globalLogPtr->logisticsHeap->malloc(size);
    }

    void logFree(void* block)
    {
        globalLogPtr->logisticsHeap->free(block);
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

    void writeText(aFont* font, lPort* port, int32_t x, int32_t y, const char* text)
    {
        font->writeString(port->frame(), x, y, reinterpret_cast<uint8_t*>(const_cast<char*>(text)), -1);
    }

    /// <summary>Loads "<c>artPath</c>logart\..." (<paramref name="format"/> with the art path and a number) into <paramref name="port"/>.</summary>
    void loadArt(lPort* port, const char* format, int32_t number)
    {
        char fileName[256];
        std::snprintf(fileName, sizeof(fileName), format, artPath, number);
        port->init(fileName);
    }

    /// <summary>Shows the one-button message dialog with string <paramref name="id"/> and the "okay" button art.</summary>
    void showMessage(uint32_t id)
    {
        char text[256];
        cLoadString(thisInstance, id, text, 0xfe);
        ReusableDialog* dialog = globalLogPtr->messageDialog;
        dialog->setText(text);
        globalLogPtr->messageDialog->setTwoButton(0);
        dialog = globalLogPtr->messageDialog;
        dialog->callback = nullptr;
        char upArt[] = "bh_okay.tga";
        char downArt[] = "bg_okay.tga";
        dialog->okButton->setUpPicture(upArt);
        globalLogPtr->messageDialog->okButton->setDownPicture(downArt);
        lDialogButton* button = globalLogPtr->messageDialog->okButton;
        button->disabled = 0;
        button->draw();
        globalLogPtr->messageDialog->activate();
    }

    /// <summary>How many mechs and vehicles the player owns (inventory and force).</summary>
    int32_t numUnits()
    {
        return globalLogPtr->forceVehicleList->numVehicles + globalLogPtr->forceMechList->numMechs +
               globalLogPtr->vehicleList->numVehicles + globalLogPtr->mechList->numMechs;
    }

    /// <summary>How many units a purchase may buy: the room left under 50 units, or the stock when that is less.</summary>
    int32_t maxPurchase(int32_t available)
    {
        int32_t room = 0x32 - globalLogPtr->forceVehicleList->numVehicles - globalLogPtr->forceMechList->numMechs -
                       globalLogPtr->vehicleList->numVehicles - globalLogPtr->mechList->numMechs;

        if (available < room && available > -1)
        {
            room = available;
        }

        return room;
    }

    /// <summary>Opens the purchase dialog on the current screen.</summary>
    void openPurchaseDialog(int32_t purchaseType, int32_t cost, int32_t maxQuantity, char* title, char* subtitle,
                            lPort* picture, void (*callback)(int, int32_t))
    {
        globalLogPtr->purchaseDialog->init(purchaseType, cost, maxQuantity, title, subtitle, picture);
        globalLogPtr->purchaseDialog->setPort(globalLogPtr->currentScreen->lport());
        globalLogPtr->purchaseDialog->setCallback(callback);
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

    /// <summary>The string table index of the armor rating of <paramref name="armorTonnage"/>.</summary>
    uint32_t armorClassString(float armorTonnage)
    {
        if (armorTonnage <= 2.0f)
        {
            return 100;
        }

        if (armorTonnage <= 7.0f)
        {
            return 0x4f;
        }

        if (armorTonnage <= 12.0f)
        {
            return 0x65;
        }

        if (armorTonnage <= 17.0f)
        {
            return 0x51;
        }

        return 0x66;
    }

    /// <summary>
    /// Whether the event is over a pane's inside (0xd pixels short of its right edge). The shop rows take the
    /// position from the purchase screen's pane and the size from the repair screen's.
    /// </summary>
    bool overPane(aObject* posPane, aObject* sizePane, aEvent* event)
    {
        return posPane->globalX() < event->x && event->x < posPane->globalX() + sizePane->width() - 0xd &&
               posPane->globalY() < event->y && event->y < posPane->globalY() + sizePane->height();
    }

    bool overInventory(aEvent* event)
    {
        return overPane(globalLogPtr->purchaseScreen->inventoryPane, globalLogPtr->repairScreen->inventoryPane, event);
    }

    bool overStore(aEvent* event)
    {
        return overPane(globalLogPtr->purchaseScreen->unitPane, globalLogPtr->repairScreen->unitPane, event);
    }

    /// <summary>Whether the event is on <paramref name="row"/> (edges included).</summary>
    bool onRow(aObject* row, aEvent* event)
    {
        return row->globalX() <= event->x && event->x <= row->globalX() + row->width() && row->globalY() <= event->y &&
               event->y <= row->globalY() + row->height();
    }

    /// <summary>
    /// Makes the drag icon: a 0x20 square copied from the store pane at (6, row top + <paramref name="yOffset"/>),
    /// framed in colour 0xea, added to the purchase screen at (<paramref name="drag"/>.x, .y).
    /// </summary>
    void makeDragIcon(const DragState& drag, int32_t row, int32_t yOffset)
    {
        auto* icon = new DragIcon;
        globalLogPtr->dragIcon = icon;
        icon->init(drag.x, drag.y, 0x20, 0x20, nullptr, nullptr);
        lPort* display = nullptr;
        globalLogPtr->purchaseScreen->unitPane->getDisplayPort(display);
        PANE* frame = globalLogPtr->dragIcon->lport()->frame();
        VFX_pane_copy(display->frame(), 6, row * 0x70 + yOffset, frame, 0, 0, -1);
        VFX_line_draw(globalLogPtr->dragIcon->lport()->frame(), 0, 0, 0x1f, 0, LD_DRAW, 0xea);
        VFX_line_draw(globalLogPtr->dragIcon->lport()->frame(), 0, 1, 0, 0x1e, LD_DRAW, 0xea);
        VFX_line_draw(globalLogPtr->dragIcon->lport()->frame(), 0x1f, 1, 0x1f, 0x1e, LD_DRAW, 0xea);
        VFX_line_draw(globalLogPtr->dragIcon->lport()->frame(), 0, 0x1f, 0x1f, 0x1f, LD_DRAW, 0xea);
        globalLogPtr->purchaseScreen->addChild(globalLogPtr->dragIcon);
        globalLogPtr->dragIcon->ShowGUIWindow(1);
        globalLogPtr->dragIcon->setDepth(100);
        globalLogPtr->dragIcon->moveTo(drag.x, drag.y, 0);
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
    /// Reads description <paramref name="descIndex"/> of the object description file: "%fc4" (a colour code) and
    /// the text, on the logistics heap. Null when the file has no such block.
    /// </summary>
    char* loadDescriptionText(int32_t descIndex)
    {
        auto* file = new FitIniFile;
        char text[1024];
        std::snprintf(text, sizeof(text), "%s%s", objectPath, objectDesc);
        int32_t result = file->open(text, READ, 0x32);
        Assert(result == 0, result, "Could not open description file", nullptr);
        std::snprintf(text, sizeof(text), "Desc%d", descIndex);
        char* description = nullptr;

        if (file->seekBlock(text) == 0)
        {
            result = file->readIdString("DescString", text, 0x3ff);
            Assert(result == 0 || static_cast<uint32_t>(result) == 0xfada0003, result,
                   "Could not read description string", nullptr);
            size_t length = std::strlen(text) + 1;
            description = static_cast<char*>(logAlloc(static_cast<uint32_t>(length + 4)));
            std::snprintf(description, length + 4, "%%fc4%s", text);
            description[length + 3] = '\0';
        }

        delete file;
        return description;
    }

    /// <summary>
    /// Opens a profile: <paramref name="dir"/> name.fit (tried twice), then the save-temp folder's name.fit, and for
    /// a mech or pilot the save-temp folder's bare name.
    /// </summary>
    void openProfile(FitIniFile* file, FullPathFileName& path, const char* dir, const char* name, bool bareName,
                     const char* error)
    {
        path.init(dir, name, ".fit");

        if (file->open(static_cast<char*>(path), READ, 0x32) == 0)
        {
            return;
        }

        path.init(profilePath, name, ".fit");

        if (file->open(static_cast<char*>(path), READ, 0x32) == 0)
        {
            return;
        }

        path.init(saveTempPath, name, ".fit");
        int32_t result = file->open(static_cast<char*>(path), READ, 0x32);

        if (result == 0 || !bareName)
        {
            Assert(result == 0, result, error, nullptr);
            return;
        }

        path.init(saveTempPath, name, nullptr);
        result = file->open(static_cast<char*>(path), READ, 0x32);
        Assert(result == 0, result, error, nullptr);
    }

    /// <summary>
    /// Reads the Item:n blocks of a profile's inventory (NumOther others, NumWeapons weapons, NumAmmo ammo) into
    /// <paramref name="inventory"/>.
    /// </summary>
    /// <returns>The sum of the items' resource points.</returns>
    int32_t readInventory(FitIniFile* file, InventoryList* inventory, uint8_t numOther, uint8_t numWeapons,
                          uint8_t numAmmo)
    {
        int32_t cost = 0;
        char block[32];
        int32_t item = 0;

        for (; item < numOther; ++item)
        {
            std::snprintf(block, sizeof(block), "Item:%d", item);
            int32_t result = file->seekBlock(block);
            Assert(result == 0, result, "Could not read 'other' item in mech file", nullptr);
            uint8_t masterID = 0;
            result = file->readIdUChar("MasterID", masterID);
            Assert(result == 0, result, "Could not read 'other' item's MasterID in mech file", nullptr);
            _LogInventoryStat* stat = inventory->createStat(static_cast<uint8_t>(item), 0, 0, 0, 0, 1, 0xff);
            inventory->addItem(masterID, stat, -1);
            cost += MasterComponentList[masterID].resourcePoints;
        }

        for (; item < numOther + numWeapons; ++item)
        {
            std::snprintf(block, sizeof(block), "Item:%d", item);
            int32_t result = file->seekBlock(block);
            Assert(result == 0, result, "Could not read 'weapon' item in mech file", nullptr);
            uint8_t masterID = 0;
            result = file->readIdUChar("MasterID", masterID);
            Assert(result == 0, result, "Could not read 'weapon' item's MasterID in mech file", nullptr);
            uint8_t facesForward = 0;
            result = file->readIdUChar("FacesForward", facesForward);
            Assert(result == 0, result, "Could not read 'weapon' item's FacesForward in mech file", nullptr);
            _LogInventoryStat* stat = inventory->createStat(static_cast<uint8_t>(item), 0, 0, facesForward, 0, 1, 0xff);
            inventory->addItem(masterID, stat, -1);
            cost += MasterComponentList[masterID].resourcePoints;
        }

        for (; item < numOther + numWeapons + numAmmo; ++item)
        {
            std::snprintf(block, sizeof(block), "Item:%d", item);
            int32_t result = file->seekBlock(block);
            Assert(result == 0, result, "Could not read 'ammo' item in mech file", nullptr);
            uint8_t masterID = 0;
            result = file->readIdUChar("MasterID", masterID);
            Assert(result == 0, result, "Could not read 'ammo' item's MasterID in mech file", nullptr);
            int32_t amount = 0;

            if (file->readIdLong("Amount", amount) != 0)
            {
                uint8_t smallAmount = 0;
                result = file->readIdUChar("Amount", smallAmount);
                Assert(result == 0, result, "Could not read 'ammo' item's Amount in mech file", nullptr);
                amount = smallAmount;
            }

            _LogInventoryStat* stat =
                inventory->createStat(static_cast<uint8_t>(item), 0, 0, 0, 0, static_cast<int16_t>(amount), 0xff);
            inventory->addItem(masterID, stat, -1);
            cost += MasterComponentList[masterID].resourcePoints;
        }

        return cost;
    }

    /// <summary>
    /// Writes a unit's weapons (short, medium, then long range, as "count name") and its sensors, ECM and probes
    /// down the right of the row in the green font.
    /// </summary>
    /// <param name="text">The row's text buffer (the caller may show what is left in it).</param>
    void drawInventoryList(InventoryList* inventory, lPort* port, char* text, size_t textSize)
    {
        std::vector<int32_t> shortRange;
        std::vector<int32_t> mediumRange;
        std::vector<int32_t> longRange;
        int32_t index = 0;

        for (_LogInventoryItem* item = inventory->items; item != nullptr; item = item->next, ++index)
        {
            const MasterComponent& component = MasterComponentList[item->masterID];
            int32_t form = component.form;

            if (form != COMPONENT_FORM_WEAPON_ENERGY && form != COMPONENT_FORM_WEAPON_BALLISTIC &&
                form != COMPONENT_FORM_WEAPON_MISSILE && form != COMPONENT_FORM_WEAPON)
            {
                continue;
            }

            if (component.weaponRange[3] < 76.0f)
            {
                shortRange.push_back(index);
            }
            else if (component.weaponRange[3] < 151.0f)
            {
                mediumRange.push_back(index);
            }
            else
            {
                longRange.push_back(index);
            }
        }

        int32_t line = 0;

        for (const auto* group : {&shortRange, &mediumRange, &longRange})
        {
            for (int32_t position : *group)
            {
                _LogInventoryItem* item = inventory->getItemInfo(position);
                std::snprintf(text, textSize, "%d %s", item->count, item->name);
                writeText(greenFont, port, 0x14a, (greenFont->height() + 1) * line + 4, text);
                ++line;
            }
        }

        for (_LogInventoryItem* item = inventory->items; item != nullptr; item = item->next)
        {
            int32_t form = MasterComponentList[item->masterID].form;

            if (form == COMPONENT_FORM_SENSOR || form == COMPONENT_FORM_ECM || form == COMPONENT_FORM_PROBE)
            {
                std::snprintf(text, textSize, "%d %s", item->count, item->name);
                writeText(greenFont, port, 0x14a, (greenFont->height() + 1) * line + 4, text);
                ++line;
            }
        }
    }

    /// <summary>Formats a description with the SMUTI text formatter into <paramref name="scratch"/> and copies it into <paramref name="port"/>.</summary>
    void drawDescription(lPort* scratch, int32_t width, int32_t height, char* description, lPort* port, int32_t x,
                         int32_t y)
    {
        scratch->destroy();
        scratch->init(width, height, 1);
        VFX_pane_wipe(scratch->frame(), 0xff);
        description[3] = '9';
        application->textFormatter.process(reinterpret_cast<uint8_t*>(description), scratch, 0, 0);
        scratch->copyTo(port->frame(), x, y, 1);
    }

    /// <summary>A text field set from a string table entry, on the logistics heap.</summary>
    /// <remarks>Port fix: the previous text is freed (the rows' drawBackground made a new one on every draw).</remarks>
    void setHeapText(char*& field, const char* text)
    {
        if (field != nullptr)
        {
            logFree(field);
        }

        field = heapString(text);
    }
}

// Unit limits and purchase callbacks

auto checkMaxUnits() -> int
{
    if (numUnits() > 0x31)
    {
        playSample(0x33);
        showMessage(0x374);
        return 1;
    }

    return 0;
}

auto checkNumUnits() -> void
{
    int32_t units = numUnits();

    if (units <= 0x27)
    {
        return;
    }

    char text[256];

    if (units == 0x32)
    {
        cLoadString(thisInstance, 0x374, text, 0xfe);
    }
    else
    {
        char format[256];
        cLoadString(thisInstance, 0x372, format, 0xfe);
        std::snprintf(text, sizeof(text), format, units, 0x32);
    }

    playSample(0x33);
    ReusableDialog* dialog = globalLogPtr->messageDialog;
    dialog->setText(text);
    globalLogPtr->messageDialog->setTwoButton(0);
    dialog = globalLogPtr->messageDialog;
    dialog->callback = nullptr;
    char upArt[] = "bh_okay.tga";
    char downArt[] = "bg_okay.tga";
    dialog->okButton->setUpPicture(upArt);
    globalLogPtr->messageDialog->okButton->setDownPicture(downArt);
    lDialogButton* button = globalLogPtr->messageDialog->okButton;
    button->disabled = 0;
    button->draw();
    globalLogPtr->messageDialog->activate();
}

auto MechPurchaseCallback(int confirmed, int32_t quantity) -> void
{
    if (confirmed == 0 || globalMechPurchaseBlock == nullptr || quantity == 0)
    {
        return;
    }

    MechPurchaseBlock* block = globalMechPurchaseBlock;

    for (int32_t count = quantity; count > 0; --count)
    {
        PurMechData* data = block->purMech->variants[block->curVariant];
        globalLogPtr->mechList->addMech(data->fileName, 0, 1, 1);
    }

    globalLogPtr->reorderMechs();
    globalLogPtr->purchaseScreen->createMechInvBlock();
    globalLogPtr->purchaseScreen->setUpMechInv(1, 1);
    globalMechPurchaseBlock->purMech->variants[globalMechPurchaseBlock->curVariant]->numAvailable -= quantity;
    globalMechPurchaseBlock->drawBackground(globalMechPurchaseBlock->row);
    ResourcePoints -= globalLogPtr->purchaseDialog->unitCost * quantity;
    checkNumUnits();
}

auto PilotPurchaseCallback(int confirmed, int32_t) -> void
{
    if (confirmed == 0)
    {
        return;
    }

    float scrollPos = globalLogPtr->purchaseScreen->unitPane->scrollPos;
    globalLogPtr->warriorList->addWarrior(globalPilotPurchaseBlock->pilot->fileName, 1);
    globalLogPtr->reorderWarriors();
    globalLogPtr->purchaseScreen->createPilotInvBlock();
    globalLogPtr->purchaseScreen->setUpPilotInv(1, 1);
    PurPilotData* pilot = globalPilotPurchaseBlock->pilot;
    pilot->health = 0;
    globalLogPtr->purPilotList->setPilotStatus(pilot->descIndex, 1);
    globalLogPtr->purchaseScreen->removePilot(globalPilotPurchaseBlock->pilot->block->row);
    globalLogPtr->purchaseScreen->setUpPilotPurchase();
    ResourcePoints -= globalLogPtr->purchaseDialog->unitCost;
    soundSystem->playPilotSpeech(globalPilotPurchaseBlock->pilot->pilotAudio, 2);
    globalLogPtr->purchaseScreen->unitPane->setScrollPos(scrollPos);
}

auto CompPurchaseCallback(int confirmed, int32_t quantity) -> void
{
    _LogInventoryItem* bought = globalItemPtr;

    if (confirmed == 0)
    {
        return;
    }

    bought->count -= quantity;
    bought->purchaseBlock->drawBackground(bought->purchaseBlock->row, bought->masterID);
    InventoryList* spares = globalLogPtr->componentInventory;
    _LogInventoryItem* spare = spares->items;

    while (spare != nullptr && spare->masterID != globalItemPtr->masterID)
    {
        spare = spare->next;
    }

    if (spare != nullptr)
    {
        if (spare->count == 0)
        {
            spare->count = quantity;
            globalLogPtr->purchaseScreen->createCompInvBlock();
            globalLogPtr->purchaseScreen->setUpCompInv(0, 1);
        }
        else
        {
            spare->count += quantity;
            spare->inventoryBlock->drawBackground();
        }
    }
    else
    {
        // A new spare component: its first copy and its inventory row.
        _LogInventoryStat* stat = spares->createStat(spares->nextStatID, 0, 0, 0, 0, 1, 0xff);
        globalLogPtr->componentInventory->addItem(globalItemPtr->masterID, stat, -1);
        InventoryList* list = globalLogPtr->componentInventory;
        spare = list->getItemInfo(list->getIndexFromMasterID(globalItemPtr->masterID));
        spare->count = quantity;
        auto* block = new CompInventoryBlock;
        spare->inventoryBlock = block;
        block->init(spare);
        spare->inventoryBlock->inventoryIndex = globalLogPtr->componentInventory->numItems - 1;
        globalLogPtr->purchaseScreen->createCompInvBlock();
        globalLogPtr->purchaseScreen->setUpCompInv(0, 1);
    }

    ResourcePoints -= globalLogPtr->purchaseDialog->unitCost * quantity;
}

auto VehiclePurchaseCallback(int confirmed, int32_t quantity) -> void
{
    if (confirmed == 0)
    {
        return;
    }

    for (int32_t count = quantity; count > 0; --count)
    {
        globalLogPtr->vehicleList->addVehicle(globalVehicleBlockPtr->purVehicle->data->fileName, 0, 1, 1);
    }

    globalLogPtr->reorderVehicles();
    globalLogPtr->purchaseScreen->createVhclInvBlock();
    globalLogPtr->purchaseScreen->setUpVhclInv(1, 1);
    VehiclePurchaseBlock* block = globalVehicleBlockPtr;
    block->purVehicle->data->numAvailable -= quantity;
    block->drawBackground(block->row);
    ResourcePoints -= globalLogPtr->purchaseDialog->unitCost * quantity;
    checkNumUnits();
}

// PurMechList

PurMechList::PurMechList()
{
    init();
}

auto PurMechList::init() -> void
{
    first = nullptr;
    count = 0;
}

auto PurMechList::destroy() -> void
{
    for (PurMech* purMech = first; purMech != nullptr; purMech = first)
    {
        first = purMech->next;

        for (PurMechData*& data : purMech->variants)
        {
            if (data == nullptr)
            {
                continue;
            }

            if (data->inventory != nullptr)
            {
                data->inventory->destroy();
                delete data->inventory;
                data->inventory = nullptr;
            }

            if (data->description != nullptr)
            {
                logFree(data->description);
                data->description = nullptr;
            }

            logFree(data);
            data = nullptr;
        }

        if (purMech->block != nullptr)
        {
            delete purMech->block;
            purMech->block = nullptr;
        }

        delete purMech;
    }

    first = nullptr;
    count = 0;
}

auto PurMechList::addMech(PurMech* purMech, char* fileName, int32_t variant) -> int32_t
{
    FullPathFileName path;
    auto* file = new FitIniFile;
    Assert(file != nullptr, 0, " no RAM for mech file ", nullptr);
    openProfile(file, path, profilePath, fileName, true, " could not open mech file ");

    void* memory = logAlloc(sizeof(PurMechData));
    Assert(memory != nullptr, 0, "Not enough memory for LogMech", nullptr);
    auto* data = new (memory) PurMechData;
    std::strncpy(data->fileName, fileName, 0xb);
    data->inventory = new InventoryList;
    Assert(data != nullptr, 0, "Not enough memory for InventoryList", nullptr);

    int32_t result = file->seekBlock("Header");
    Assert(result == 0, result, "Could not find header in mech file", nullptr);
    char fileType[20];
    result = file->readIdString("FileType", fileType, 0x14);
    Assert(result == 0, result, "Could not find filetype string in mech file", nullptr);
    Assert(std::strcmp(fileType, "MechProfile") == 0, 0, "File is not a mech file", nullptr);
    result = file->seekBlock("General");
    Assert(result == 0, result, "Could not find general block in mech file", nullptr);
    result = file->readIdFloat("CurTonnage", data->curTonnage);
    Assert(result == 0, result, "Could not find curTonnage in mech file", nullptr);
    char text[256];
    result = file->readIdString("MechType", text, 0x28);
    Assert(result == 0, result, "Could not read MechType in mech file", nullptr);
    result = file->readIdLong("NameIndex", data->nameIndex);
    Assert(result == 0, result, " AddPurMech: could not find NameIndex ", nullptr);
    std::strcpy(data->name, text);

    if (file->readIdLong("ResourcePoints", data->cost) != 0)
    {
        data->cost = 100;
    }

    if (file->readIdLong("ChassisBR", data->chassisBR) != 0)
    {
        data->chassisBR = 100;
    }

    data->description = nullptr;
    data->descIndex = -1;
    file->readIdLong("DescIndex", data->descIndex);
    data->loadDescription(data->descIndex);
    cLoadString(thisInstance, static_cast<uint32_t>(data->descIndex + 300), text, 0x28);
    std::strncpy(data->name, text, 0x28);
    data->name[0x28] = '\0';

    result = file->seekBlock("Engine");
    Assert(result == 0, result, "Could not find Engine block in mech file", nullptr);
    result = file->readIdUChar("MaxRunSpeed", data->maxRunSpeed);
    Assert(result == 0, result, "Could not read MaxRunSpeed in mech file", nullptr);
    result = file->seekBlock("Armor");
    Assert(result == 0, result, "Could not find Armor block in mech file", nullptr);
    result = file->readIdFloat("Tonnage", data->armorTonnage);
    Assert(result == 0, result, "Could not read Tonnage in mech file", nullptr);
    result = file->seekBlock("MaxArmorPoints");
    Assert(result == 0, result, "Could not find MaxArmorPoints block in mech file", nullptr);

    for (int32_t location = 0; location < 11; ++location)
    {
        result = file->readIdUChar(armorLocationNames[location], data->armor[location].maxArmor);
        Assert(result == 0, result, "Could not read armor in maxArmor block in mech file", nullptr);
    }

    result = file->seekBlock("CurArmorPoints");
    Assert(result == 0, result, "Could not find CurArmorPoints block in mech file", nullptr);
    int32_t armorPoints = 0;

    for (int32_t location = 0; location < 11; ++location)
    {
        result = file->readIdUChar(armorLocationNames[location], data->armor[location].curArmor);
        Assert(result == 0, result, "Could not read armor in curArmorPoins block in mech file", nullptr);
        armorPoints += data->armor[location].curArmor;
    }

    // Each armor point adds 40 resource points.
    data->cost += armorPoints * 0x28;

    result = file->seekBlock("InventoryInfo");
    Assert(result == 0, result, "Could not find InventoryInfo block in vehicle file", nullptr);
    result = file->readIdUChar("NumOther", data->numOther);
    Assert(result == 0, result, "Could not read NumOther in mech file", nullptr);
    result = file->readIdUChar("NumWeapons", data->numWeapons);
    Assert(result == 0, result, "Could not read NumWeapons in mech file", nullptr);
    result = file->readIdUChar("NumAmmo", data->numAmmo);
    Assert(result == 0, result, "Could not read NumAmmo in mech file", nullptr);
    std::memset(data->criticalSlots, 0xff, sizeof(data->criticalSlots));
    data->cost += readInventory(file, data->inventory, data->numOther, data->numWeapons, data->numAmmo);

    // The body locations: internal structure and the critical slots (component copy, damage).
    float internalPoints = 0.0f;

    for (int32_t location = 0; location < 8; ++location)
    {
        result = file->seekBlock(bodyLocationNames[location]);
        Assert(result == 0, result, "Could not find BodyLocation block in mech file", nullptr);
        result = file->readIdUChar("CurInternalStructure", data->curInternalStructure[location]);
        Assert(result == 0, result, "Could not read CurInternalStructure in mech file", nullptr);
        internalPoints = static_cast<float>(data->curInternalStructure[location]) + internalPoints;

        for (int32_t slot = 0; slot < NumLocationCriticalSpaces[location]; ++slot)
        {
            std::snprintf(text, sizeof(text), "Component:%d", slot);
            uint8_t component[2] = {};
            result = file->readIdUCharArray(text, component, 2);
            Assert(result == 0, result, "Could not read component in mech file", nullptr);
            data->criticalSlots[location][slot].masterId = component[0];
            data->criticalSlots[location][slot].damage = component[1];

            if (component[0] != 0xff)
            {
                if (component[1] != 0)
                {
                    data->inventory->hitItem(component[0], component[1]);
                }

                // The slot index is passed as the location (faithful).
                data->inventory->setStatLoc(component[0], slot);
            }
        }
    }

    // Each internal structure point adds 50 resource points.
    data->cost = static_cast<int32_t>(static_cast<double>(internalPoints) * 50.0f + data->cost);
    data->calcBR();
    purMech->variants[variant] = data;
    file->close();
    delete file;
    return 0;
}

auto PurMechList::addMech(char* fileName0, int32_t count0, char* fileName2, int32_t count2, char* fileName1,
                          int32_t count1) -> int32_t
{
    auto* purMech = new PurMech;
    Assert(purMech != nullptr, 0, " Not enough memory to allocate PurMech", nullptr);
    auto* block = new MechPurchaseBlock;
    purMech->block = block;
    Assert(block != nullptr, 0, " Not enough memory for repair block ", nullptr);
    block->curVariant = -1;
    addMech(purMech, fileName0, 0);
    purMech->variants[0]->numAvailable = count0;
    addMech(purMech, fileName1, 1);
    purMech->variants[1]->numAvailable = count1;
    addMech(purMech, fileName2, 2);
    purMech->variants[2]->numAvailable = count2;

    // Show the first variant in stock.
    if (count0 == 0 && count1 != 0)
    {
        purMech->block->curVariant = 1;
    }
    else if (count0 == 0 && count2 != 0)
    {
        purMech->block->curVariant = 2;
    }
    else
    {
        purMech->block->curVariant = 0;
    }

    purMech->block->init(purMech);
    purMech->next = first;
    first = purMech;
    ++count;
    return 0;
}

auto PurMechList::modMech(char* fileName, int32_t delta0, int32_t delta2, int32_t delta1) -> int32_t
{
    for (int32_t index = 0; index < count; ++index)
    {
        PurMech* purMech = nullptr;
        getMechInfo(index, purMech);

        if (std::strcmp(purMech->variants[0]->fileName, fileName) != 0)
        {
            continue;
        }

        int32_t& stock0 = purMech->variants[0]->numAvailable;
        stock0 += delta0;

        if (stock0 < 0)
        {
            stock0 = 0;
        }

        int32_t& stock1 = purMech->variants[1]->numAvailable;
        stock1 += delta1;

        if (stock1 < 0)
        {
            stock1 = 0;
        }

        int32_t& stock2 = purMech->variants[2]->numAvailable;
        stock2 += delta2;

        if (stock2 < 0)
        {
            stock2 = 0;
        }

        return 0;
    }

    return -1;
}

auto PurMechList::removeMech(uint8_t) -> int32_t
{
    return 0;
}

auto PurMechList::getMechInfo(int32_t index, PurMech*& purMech) -> int32_t
{
    if (count <= index)
    {
        return -1;
    }

    PurMech* node = first;

    for (; index > 0; --index)
    {
        node = node->next;
    }

    purMech = node;
    return 0;
}

auto PurMechList::getMechCount() -> int32_t
{
    return count;
}

// MechPurchaseBlock

MechPurchaseBlock::~MechPurchaseBlock()
{
    MechPurchaseBlock::destroy();
}

auto MechPurchaseBlock::init(PurMech* newPurMech) -> void
{
    picturePort = nullptr;
    diagramPort = nullptr;
    purMech = newPurMech;
    lObject::init(0, 0, 0x19a, 0x70, nullptr, globalLogPtr->purchaseScreen->lport());
    nameIndex = purMech->variants[curVariant]->nameIndex;
}

auto MechPurchaseBlock::destroy() -> void
{
    purMech = nullptr;

    if (picturePort != nullptr)
    {
        delete picturePort;
        picturePort = nullptr;
    }

    if (diagramPort != nullptr)
    {
        delete diagramPort;
        diagramPort = nullptr;
    }

    logFree(weightClassText);
    weightClassText = nullptr;
    logFree(armorText);
    armorText = nullptr;
    logFree(internalText);
    internalText = nullptr;
    lObject::destroy();
}

auto MechPurchaseBlock::handleEvent(aEvent* event) -> void
{
    // The position within the row.
    int32_t localX = event->x - globalX();
    int32_t localY = event->y - globalY();

    if (globalLogPtr->currentScreen == globalLogPtr->repairScreen)
    {
        return;
    }

    char text[256];
    bool showHelp = false;

    if (parent == nullptr)
    {
        showHelp = mechDrag.dragging == 0;
    }
    else if (mechDrag.dragging == 0)
    {
        if (mechDrag.carrying == 0 && (event->type == 8 || event->type == 9))
        {
            parent->handleEvent(event);
            return;
        }

        showHelp = true;
    }

    if (showHelp && event->key == 0)
    {
        // The ticker explains the variant buttons, the stock bar or the row.
        POINT point = {localX, localY};
        RECT buttonA = {0x9a, 5, 0xab, 0x15};
        RECT buttonW = {0xac, 5, 0xbd, 0x15};
        RECT buttonJ = {0xbe, 5, 0xd0, 0x15};
        RECT bar = {0xd9, 5, 0xe1, 0x6a};
        uint32_t id = 0x30;

        if (PtInRect(&buttonA, point))
        {
            id = 0x44;
        }
        else if (PtInRect(&buttonW, point))
        {
            id = 0x45;
        }
        else if (PtInRect(&buttonJ, point))
        {
            id = 0x46;
        }
        else if (PtInRect(&bar, point))
        {
            id = 0x37;
        }

        cLoadString(thisInstance, id, text, 0xfe);
        globalLogPtr->ticker->setString(text);
    }

    int32_t type = event->type;

    switch (type)
    {
        case 1:
        {
            if (mechDrag.carrying != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 3:
        {
            if (mechDrag.dragging != 0 || checkMaxUnits() != 0)
            {
                break;
            }

            if (localX > 0x9a && localX < 0xd0 && localY > 5 && localY < 0x16)
            {
                // A variant button.
                curVariant = localX < 0xac ? 0 : localX < 0xbe ? 1 : 2;
                drawBackground(row);
                playSample(0xf);
                return;
            }

            if (onRow(this, event) && purMech->variants[curVariant]->numAvailable != 0)
            {
                // Pick the mech up (left button drags, right button carries).
                if (type == 1)
                {
                    mechDrag.dragging = 1;
                }
                else
                {
                    mechDrag.carrying = 1;
                }

                playSample(0x35);
                application->showCursor(0);
                application->grab(this);
                mechDrag.x = event->x - 0x10;
                mechDrag.y = event->y - 0x10;
                makeDragIcon(mechDrag, row, 0x21);
                return;
            }

            playSample(0x33);
            return;
        }

        case 4:
        {
            if (mechDrag.carrying != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 6:
        {
            if (mechDrag.dragging != 0 && type == 6)
            {
                return;
            }

            if (application->grabbedObject() == nullptr)
            {
                return;
            }

            application->showCursor(1);
            application->release();
            mechDrag.carrying = 0;
            mechDrag.dragging = 0;
            deleteDragIcon();

            if (type != 6 && !overInventory(event))
            {
                // Dropped back on the store: nothing happens.
                playSample(overStore(event) ? 0x34 : 0x33);
                return;
            }

            // Buy it.
            PurMechData* data = purMech->variants[curVariant];

            if (ResourcePoints < data->cost)
            {
                playSample(0x33);
                showMessage(0x4d);
                return;
            }

            playSample(0x34);
            globalMechPurchaseBlock = this;
            cLoadString(thisInstance, weightClassString(data->curTonnage), text, 0xfe);
            char title[512];
            std::snprintf(title, sizeof(title), "%.0f Ton %s 'Mech", static_cast<double>(data->curTonnage), text);
            openPurchaseDialog(0, data->cost, maxPurchase(data->numAvailable), data->name, title, diagramPort,
                               MechPurchaseCallback);
            break;
        }

        case 7:
        {
            if (mechDrag.dragging != 0)
            {
                mechDrag.x = event->x - 0xf;
                mechDrag.y = event->y - 0xf;
                globalLogPtr->dragIcon->moveTo(mechDrag.x, mechDrag.y, 0);
            }
            break;
        }
    }
}

auto MechPurchaseBlock::draw() -> void
{
}

auto MechPurchaseBlock::drawBackground(int32_t rowIndex) -> void
{
    PurchaseScreen* screen = globalLogPtr->purchaseScreen;
    auto* scratch = new lPort;
    auto* work = new lPort;
    ownPort = work;
    work->init(screen->mechTabPort->width(), screen->mechTabPort->height(), 1);
    screen->mechTabPort->copyTo(work->frame(), 0, 0, 0);
    PurMechData* data = purMech->variants[curVariant];
    loadArt(scratch, "%slogart\\lspflma%02d.tga", data->nameIndex);
    scratch->copyTo(work->frame(), 5, 4, 1);
    scratch->destroy();
    loadArt(scratch, "%slogart\\lscdsm%02d.tga", data->nameIndex);
    scratch->copyTo(work->frame(), 0x13a, 6, 1);
    delete scratch;

    if (picturePort == nullptr)
    {
        // The mech's picture: its shadow (shapes 0xb..0x12 through the shadow table), then the mech (0..7).
        auto* picture = new lPort;
        picturePort = picture;
        picture->init(0x4b, 100, 1);
        VFX_pane_wipe(picture->frame(), 0x10);
        VFX_shape_lookaside(globalLogPtr->shapeLookaside[5]);

        for (int32_t shape = 0; shape < 8; ++shape)
        {
            VFX_shape_translate_draw(picture->frame(), globalLogPtr->mechRepShapes[data->nameIndex], shape + 0xb, 0, 0);
        }

        VFX_shape_lookaside(globalLogPtr->shapeLookaside[0]);

        for (int32_t shape = 0; shape < 8; ++shape)
        {
            VFX_shape_translate_draw(picture->frame(), globalLogPtr->mechRepShapes[data->nameIndex], shape, 0, 0);
        }
    }

    lPort* picture = picturePort;
    auto* spare = new lPort;

    if (diagramPort == nullptr)
    {
        auto* diagram = new lPort;
        diagramPort = diagram;
        diagram->init(0x1e, 0x1e, 1);
        VFX_pane_wipe(diagram->frame(), 0x10);

        for (int32_t location : bodyTrans)
        {
            AG_shape_draw(diagram->frame(), globalLogPtr->mechIconShapes[data->nameIndex], location, 3, 0);
        }
    }

    picture->copyTo(work->frame(), 0xed, 6, 1);

    // The class texts.
    char text[256];
    cLoadString(thisInstance, weightClassString(data->curTonnage), text, 0xfe);
    setHeapText(weightClassText, text);
    data = purMech->variants[curVariant];
    cLoadString(thisInstance, armorClassString(data->armorTonnage), text, 0xfe);
    setHeapText(armorText, text);
    int32_t internals = 0;

    for (uint8_t points : purMech->variants[curVariant]->curInternalStructure)
    {
        internals += points;
    }

    uint32_t internalId = internals < 0x24   ? 100u
                          : internals < 0x38 ? 0x4fu
                          : internals < 0x51 ? 0x65u
                          : internals < 0x79 ? 0x51u
                                             : 0x66u;
    cLoadString(thisInstance, internalId, text, 0xfe);
    setHeapText(internalText, text);
    char tons[32];
    cLoadString(thisInstance, 0x6e, tons, 0x1e);
    data = purMech->variants[curVariant];
    std::snprintf(text, sizeof(text), "%.0f %s", static_cast<double>(data->curTonnage), tons);
    lPort* row0 = ownPort;
    writeText(yellowDropFont, row0, 0x51, 0x23, text);
    writeText(yellowDropFont, row0, 0x51, 0x2c, weightClassText);
    writeText(yellowDropFont, row0, 0xa7, 0x23, armorText);
    writeText(yellowDropFont, row0, 0xa7, 0x2c, internalText);
    std::snprintf(text, sizeof(text), "%d m/s", data->maxRunSpeed);
    writeText(yellowDropFont, row0, 0x51, 0x35, text);
    setBar();

    // The weapons and equipment, and the jump jets' rating.
    int32_t jumpJets = 0;

    for (_LogInventoryItem* item = purMech->variants[curVariant]->inventory->items; item != nullptr; item = item->next)
    {
        if (MasterComponentList[item->masterID].form == COMPONENT_FORM_JUMPJET)
        {
            jumpJets = item->count;
        }
    }

    drawInventoryList(purMech->variants[curVariant]->inventory, ownPort, text, sizeof(text));

    // An unlisted rating (jump jets beyond 8) shows whatever the text buffer last held.
    if (jumpJets == 0)
    {
        cLoadString(thisInstance, 0x6c, text, 0xfe);
    }
    else
    {
        switch (jumpJets * 2 / 3)
        {
            case 0:
            case 1:
                cLoadString(thisInstance, 0x56, text, 0xfe);
                break;
            case 2:
            case 3:
                cLoadString(thisInstance, 0x55, text, 0xfe);
                break;
            case 4:
                cLoadString(thisInstance, 0x50, text, 0xfe);
                break;
            case 5:
                cLoadString(thisInstance, 0x6d, text, 0xfe);
                break;
            default:
                break;
        }
    }

    writeText(yellowDropFont, row0, 0xa7, 0x35, text);

    // The battle rating bar: 80 pixels at 18010, bottom at y 0x58.
    PANE* frame = row0->frame();
    int32_t bar = static_cast<int32_t>(static_cast<double>(purMech->variants[curVariant]->battleRating) *
                                       0x1.d1c6674f499a1p-15 * 80.0);
    int32_t top = 0x57 - bar;
    int32_t barTop = 0x58 - bar;
    VFX_line_draw(frame, 0xdb, 0x58, 0xdf, 0x58, LD_DRAW, 0xe5);
    VFX_line_draw(row0->frame(), 0xda, top, 0xe0, top, LD_DRAW, 0xe3);
    VFX_line_draw(row0->frame(), 0xda, 0x57, 0xda, barTop, LD_DRAW, 0xe3);
    VFX_line_draw(row0->frame(), 0xe0, 0x57, 0xe0, barTop, LD_DRAW, 0xe5);

    for (int32_t x = 0xdb; x <= 0xdf; ++x)
    {
        VFX_line_draw(row0->frame(), x, 0x57, x, barTop, LD_DRAW, 0xe4);
    }

    VFX_line_draw(row0->frame(), 0xdb, 0x56 - bar, 0xdf, 0x56 - bar, LD_DRAW, 0x10);
    VFX_pixel_write(row0->frame(), 0xdf, 0x57, 0xe5);
    VFX_pixel_write(row0->frame(), 0xdb, barTop, 0xe3);
    VFX_pixel_write(row0->frame(), 0xda, top, 0x10);
    VFX_pixel_write(row0->frame(), 0xe0, top, 0x10);

    lPort* display = nullptr;
    screen->unitPane->getDisplayPort(display);
    VFX_pane_copy(row0->frame(), 0, 0, display->frame(), 0, winHeight * rowIndex, -1);
    delete row0;
    ownPort = nullptr;

    // The variant buttons (A, W, J), and the variant's name art over the row.
    globalLogPtr->purchaseScreen->unitPane->getDisplayPort(display);
    int32_t variant = curVariant;
    PurMech* mech = purMech;
    int32_t y = rowIndex * 0x70;
    char fileName[256];

    if (variant == 0)
    {
        loadArt(spare, "%slogart\\lspflma%02d.tga", mech->variants[0]->nameIndex);
        spare->copyTo(display->frame(), 5, y + 4, 1);
        spare->destroy();
        std::snprintf(fileName, sizeof(fileName), "%slogart\\lspbim04.tga", artPath);
    }
    else
    {
        std::snprintf(fileName, sizeof(fileName),
                      mech->variants[0]->numAvailable == 0 ? "%slogart\\lspbim07.tga" : "%slogart\\lspbim01.tga",
                      artPath);
    }

    spare->init(fileName);
    spare->copyTo(display->frame(), 0x9a, y + 5, 1);

    if (variant == 2)
    {
        loadArt(spare, "%slogart\\lspflmj%02d.tga", mech->variants[2]->nameIndex);
        spare->copyTo(display->frame(), 5, y + 4, 1);
        spare->destroy();
        std::snprintf(fileName, sizeof(fileName), "%slogart\\lspbim06.tga", artPath);
    }
    else
    {
        std::snprintf(fileName, sizeof(fileName),
                      mech->variants[2]->numAvailable == 0 ? "%slogart\\lspbim09.tga" : "%slogart\\lspbim03.tga",
                      artPath);
    }

    spare->init(fileName);
    spare->copyTo(display->frame(), 0xbe, y + 5, 1);

    if (variant == 1)
    {
        loadArt(spare, "%slogart\\lspflmw%02d.tga", mech->variants[1]->nameIndex);
        spare->copyTo(display->frame(), 5, y + 4, 1);
        spare->destroy();
        std::snprintf(fileName, sizeof(fileName), "%slogart\\lspbim05.tga", artPath);
    }
    else
    {
        std::snprintf(fileName, sizeof(fileName),
                      mech->variants[1]->numAvailable == 0 ? "%slogart\\lspbim08.tga" : "%slogart\\lspbim02.tga",
                      artPath);
    }

    spare->init(fileName);
    spare->copyTo(display->frame(), 0xac, y + 5, 1);
    spare->destroy();

    PurMechData* shown = mech->variants[variant];

    if (shown->numAvailable != 0)
    {
        VFX_pane_copy(diagramPort->frame(), 0, 0, display->frame(), 7, y + 0x22, -1);
    }
    else
    {
        // Sold out: the "sold out" name art, a blank diagram and picture, and the sold-out mark.
        static const char* const soldOut[3] = {"%slogart\\lspfdma%02d.tga", "%slogart\\lspfdmw%02d.tga",
                                               "%slogart\\lspfdmj%02d.tga"};
        loadArt(spare, soldOut[variant], shown->nameIndex);
        spare->copyTo(display->frame(), 5, y + 4, 1);
        spare->destroy();
        auto* blank = new lPort;
        blank->init(0x1e, 0x1e, 1);
        VFX_pane_wipe(blank->frame(), 0x10);
        VFX_pane_copy(blank->frame(), 0, 0, display->frame(), 7, y + 0x22, -1);
        blank->destroy();
        blank->init(0x4b, 100, 1);
        VFX_pane_wipe(blank->frame(), 0x10);
        VFX_pane_copy(blank->frame(), 0, 0, display->frame(), 0xed, y + 5, -1);
        blank->destroy();
        AG_shape_draw(display->frame(), globalLogPtr->mechRepShapes[shown->nameIndex], 0x13, 0xed, y + 6);
        delete blank;
    }

    // Stock and price.
    if (mech->variants[variant]->numAvailable < 0)
    {
        char format[256];
        cLoadString(thisInstance, 0x385, format, 0xfe);
        std::snprintf(text, sizeof(text), format, mech->variants[variant]->numAvailable);
    }
    else
    {
        std::snprintf(text, sizeof(text), "%d", mech->variants[variant]->numAvailable);
    }

    writeText(yellowDropFont, display, 0x29, y + 0x12, text);
    std::snprintf(text, sizeof(text), "%d", mech->variants[variant]->cost);
    writeText(yellowDropFont, display, 0x52, y + 0x12, text);
    PurMechData* current = mech->variants[variant];

    if (current->description == nullptr && current->descIndex > -1)
    {
        current->loadDescription(current->descIndex);
    }

    if (mech->variants[variant]->description != nullptr)
    {
        drawDescription(spare, 0xc6, 0x25, mech->variants[variant]->description, display, 6, y + 0x44);
    }

    delete spare;
}

auto MechPurchaseBlock::setBar() -> void
{
}

// PurVehicleList

PurVehicleList::PurVehicleList()
{
    init();
}

auto PurVehicleList::init() -> void
{
    first = nullptr;
    count = 0;
}

auto PurVehicleList::destroy() -> void
{
    for (PurVehicle* vehicle = first; vehicle != nullptr; vehicle = first)
    {
        PurVehicleData* data = vehicle->data;
        first = vehicle->next;

        if (data != nullptr)
        {
            if (data->name != nullptr)
            {
                logFree(data->name);
                data->name = nullptr;
            }

            if (data->inventory != nullptr)
            {
                data->inventory->destroy();
                delete data->inventory;
                data->inventory = nullptr;
            }

            if (data->description != nullptr)
            {
                logFree(data->description);
                data->description = nullptr;
            }
        }

        if (vehicle->block != nullptr)
        {
            delete vehicle->block;
            vehicle->block = nullptr;
        }

        delete vehicle->data;
        logFree(vehicle);
    }

    first = nullptr;
    count = 0;
}

auto PurVehicleList::modVehicle(char* fileName, int32_t delta) -> int32_t
{
    for (int32_t index = 0; index < count; ++index)
    {
        PurVehicle* vehicle = nullptr;
        getVehicleInfo(index, vehicle);

        if (std::strcmp(vehicle->data->fileName, fileName) != 0)
        {
            continue;
        }

        int32_t& stock = vehicle->data->numAvailable;
        stock += delta;

        if (stock < 0)
        {
            stock = 0;
        }

        return 0;
    }

    return -1;
}

auto PurVehicleList::addVehicle(char* fileName, int32_t numAvailable) -> int32_t
{
    FullPathFileName path;
    auto* file = new FitIniFile;
    Assert(file != nullptr, 0, " no RAM for scenario file ", nullptr);
    openProfile(file, path, profilePath, fileName, false, " could not open vehicle file in scenario ");

    void* memory = logAlloc(sizeof(PurVehicle));
    Assert(memory != nullptr, 0, "Not enough memory for LogVehicle", nullptr);
    auto* vehicle = new (memory) PurVehicle;
    auto* data = new PurVehicleData;
    vehicle->data = data;

    if (file->seekBlock("General") != 0)
    {
        // Port fix: a file without a General block is a raw record the original read over the 0xc-byte
        // PurVehicle (0xd0 bytes per record) and then never added; the port just drops it.
        delete data;
        logFree(vehicle);
        delete file;
        return 0;
    }

    int32_t result = file->seekBlock("Header");

    if (result != 0)
    {
        Assert(0, result, "Could not find Header block in vehicle file", nullptr);
    }

    char text[256];
    result = file->readIdString("FileType", text, 0x7f);
    Assert(result == 0, result, "Could not read FileType in vehicle file", nullptr);
    Assert(std::strcmp(text, "GroundVehicleProfile") == 0, 0, "File is not a vehicle file", nullptr);
    result = file->seekBlock("General");
    Assert(result == 0, result, "Could not find General block in vehicle file", nullptr);
    result = file->readIdLong("NameIndex", data->nameIndex);
    Assert(result == 0, result, "Could not read NameIndex in vehicle file", nullptr);
    result = file->readIdFloat("CurTonnage", data->curTonnage);
    Assert(result == 0, result, "Could not read CurTonnage in vehicle file", nullptr);

    if (file->readIdLong("ResourcePoints", data->baseCost) != 0)
    {
        data->baseCost = 100;
    }

    data->descIndex = -1;
    data->description = nullptr;
    file->readIdLong("DescIndex", data->descIndex);
    data->loadDescription(data->descIndex);
    cLoadString(thisInstance, static_cast<uint32_t>(data->descIndex + 700), text, 0x7f);
    data->name = heapString(text);

    result = file->seekBlock("Engine");
    Assert(result == 0, result, "Could not find engine block in vehicle file", nullptr);
    result = file->readIdUChar("MaxMoveSpeed", data->maxMoveSpeed);
    Assert(result == 0, result, "Could not read MaxMoveSpeed in vehicle file", nullptr);
    result = file->seekBlock("Armor");
    Assert(result == 0, result, "Could not find armor block in vehicle file", nullptr);
    result = file->readIdFloat("Tonnage", data->armorTonnage);
    Assert(result == 0, result, "Could not read Tonnage in vehicle file", nullptr);
    result = file->seekBlock("InventoryInfo");
    Assert(result == 0, result, "Could not find InventoryInfo block in vehicle file", nullptr);
    result = file->readIdUChar("NumOther", data->numOther);
    Assert(result == 0, result, "Could not read NumOther in vehicle file", nullptr);
    result = file->readIdUChar("NumWeapons", data->numWeapons);
    Assert(result == 0, result, "Could not read NumWeapons in vehicle file", nullptr);
    result = file->readIdUChar("NumAmmo", data->numAmmo);
    Assert(result == 0, result, "Could not read NumAmmo in vehicle file", nullptr);
    data->inventory = new InventoryList;
    Assert(data->inventory != nullptr, result, " invalid vehicle file: no inventory ", nullptr);
    readInventory(file, data->inventory, data->numOther, data->numWeapons, data->numAmmo);
    data->numAvailable = numAvailable;
    std::strncpy(data->fileName, fileName, 9);

    auto* block = new VehiclePurchaseBlock;
    vehicle->block = block;
    block->init(vehicle);
    vehicle->calcVehicleCost();

    // Insert in tonnage order (before the first that is as heavy or heavier).
    PurVehicle* previous = nullptr;
    PurVehicle* node = first;

    while (node != nullptr && node->data->curTonnage < vehicle->data->curTonnage)
    {
        previous = node;
        node = node->next;
    }

    if (previous == nullptr)
    {
        first = vehicle;
    }
    else
    {
        previous->next = vehicle;
    }

    vehicle->next = node;
    ++count;
    file->close();
    delete file;
    return 0;
}

auto PurVehicleList::removeVehicle(uint8_t) -> int32_t
{
    return 0;
}

auto PurVehicleList::getVehicleInfo(int32_t index, PurVehicle*& vehicle) -> int32_t
{
    if (count <= index)
    {
        return -1;
    }

    PurVehicle* node = first;

    for (; index > 0; --index)
    {
        node = node->next;
    }

    vehicle = node;
    return 0;
}

auto PurVehicleList::getVehicleCount() -> int32_t
{
    return count;
}

// VehiclePurchaseBlock

VehiclePurchaseBlock::~VehiclePurchaseBlock()
{
    VehiclePurchaseBlock::destroy();
}

auto VehiclePurchaseBlock::init(PurVehicle* newPurVehicle) -> void
{
    picturePort = nullptr;
    purVehicle = newPurVehicle;
    lObject::init(0, 0, 0x19a, 0x70, nullptr, globalLogPtr->purchaseScreen->lport());
    PurVehicleData* data = purVehicle->data;
    nameIndex = data->nameIndex;
    char text[256];
    cLoadString(thisInstance, weightClassString(data->curTonnage), text, 0xf);
    weightClassText = heapString(text);
    cLoadString(thisInstance, armorClassString(purVehicle->data->armorTonnage), text, 0xf);
    armorText = heapString(text);
}

auto VehiclePurchaseBlock::destroy() -> void
{
    // The work port is only ever alive inside drawBackground.
    ownPort = nullptr;
    purVehicle = nullptr;
    logFree(weightClassText);
    weightClassText = nullptr;
    logFree(armorText);
    armorText = nullptr;

    if (picturePort != nullptr)
    {
        delete picturePort;
        picturePort = nullptr;
    }

    lObject::destroy();
}

auto VehiclePurchaseBlock::handleEvent(aEvent* event) -> void
{
    int32_t localX = event->x - globalX();
    int32_t localY = event->y - globalY();

    if (globalLogPtr->currentScreen == globalLogPtr->repairScreen)
    {
        return;
    }

    if (parent != nullptr && vehicleDrag.dragging == 0 && vehicleDrag.carrying == 0 &&
        (event->type == 8 || event->type == 9))
    {
        parent->handleEvent(event);
        return;
    }

    int32_t type = event->type;
    char text[256];

    switch (type)
    {
        case 1:
        {
            if (vehicleDrag.carrying != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 3:
        {
            if ((vehicleDrag.dragging != 0 && type == 3) || checkMaxUnits() != 0)
            {
                return;
            }

            if ((localX < 0x94 || localX > 0xc9 || localY < 5 || localY > 0x15) && onRow(this, event) &&
                purVehicle->data->numAvailable != 0)
            {
                // Pick the vehicle up (left button drags, right button carries).
                if (type == 1)
                {
                    vehicleDrag.dragging = 1;
                }
                else
                {
                    vehicleDrag.carrying = 1;
                }

                playSample(0x35);
                application->showCursor(0);
                application->grab(this);
                vehicleDrag.x = event->x - 0x10;
                vehicleDrag.y = event->y - 0x10;
                makeDragIcon(vehicleDrag, row, 0x21);
                return;
            }
            break;
        }

        case 4:
        {
            if (vehicleDrag.carrying != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 6:
        {
            if (vehicleDrag.dragging != 0 && type == 6)
            {
                return;
            }

            vehicleDrag.carrying = 0;

            if (application->grabbedObject() == nullptr)
            {
                return;
            }

            application->release();
            application->showCursor(1);
            vehicleDrag.dragging = 0;
            deleteDragIcon();

            if (type != 6 && !overInventory(event))
            {
                if (overStore(event))
                {
                    playSample(0x34);
                    return;
                }
                break;
            }

            // Buy it.
            PurVehicleData* data = purVehicle->data;
            int32_t cost = data->cost;

            if (cost > ResourcePoints)
            {
                playSample(0x33);
                showMessage(0x4d);
                return;
            }

            playSample(0x34);
            int32_t maxQuantity = maxPurchase(data->numAvailable);
            globalVehicleBlockPtr = this;
            openPurchaseDialog(6, cost, maxQuantity, data->name, nullptr, picturePort, VehiclePurchaseCallback);
            return;
        }

        case 7:
        {
            if (vehicleDrag.dragging != 0)
            {
                vehicleDrag.y = event->y - 0xf;
                vehicleDrag.x = event->x - 0xf;
                globalLogPtr->dragIcon->moveTo(vehicleDrag.x, vehicleDrag.y, 0);
                return;
            }

            if (event->key == 0)
            {
                cLoadString(thisInstance, 0x30, text, 0xfe);
                globalLogPtr->ticker->setString(text);
            }

            return;
        }

        default:
            return;
    }

    playSample(0x33);
}

auto VehiclePurchaseBlock::drawBackground(int32_t rowIndex) -> void
{
    PurchaseScreen* screen = globalLogPtr->purchaseScreen;
    auto* scratch = new lPort;
    auto* work = new lPort;
    ownPort = work;
    work->init(screen->vehicleTabPort->width(), screen->vehicleTabPort->height(), 1);
    screen->vehicleTabPort->copyTo(work->frame(), 0, 0, 0);
    char tons[256];
    cLoadString(thisInstance, 0x6e, tons, 0xfe);
    PurVehicleData* data = purVehicle->data;
    char text[256];
    std::snprintf(text, sizeof(text), "%.0f %s", static_cast<double>(data->curTonnage), tons);
    writeText(yellowDropFont, work, 0x52, 0x24, text);
    writeText(yellowDropFont, work, 0x52, 0x2d, weightClassText);
    writeText(yellowDropFont, work, 0xa7, 0x24, armorText);
    std::snprintf(text, sizeof(text), "%d m/s", data->maxMoveSpeed);
    writeText(yellowDropFont, work, 0x52, 0x36, text);
    drawInventoryList(data->inventory, work, text, sizeof(text));

    const char* stock = text;
    char soldOutText[256];

    if (data->numAvailable == 0)
    {
        // Sold out: the "sold out" name art, a blank picture and the sold-out mark.
        loadArt(scratch, "%slogart\\lspfdv%02d.tga", data->nameIndex);
        scratch->copyTo(work->frame(), 5, 4, 1);
        scratch->destroy();
        scratch->init(0x4b, 100, 1);
        VFX_pane_wipe(scratch->frame(), 0x10);
        scratch->copyTo(work->frame(), 0xed, 6, -1);
        AG_shape_draw(work->frame(), globalLogPtr->vehicleRepShapes[data->nameIndex], 6, 0xed, 6);
        stock = "0";
    }
    else
    {
        int32_t index = data->nameIndex;
        loadArt(scratch, "%slogart\\lspflv%02d.tga", index);
        scratch->copyTo(work->frame(), 5, 4, 1);

        if (picturePort != nullptr)
        {
            delete picturePort;
        }

        // The picture, drawn into a port the original parked in picturePort before freeing it.
        auto* picture = new lPort;
        picturePort = picture;
        picture->init(0x4b, 100, 1);
        VFX_pane_wipe(picture->frame(), 0x10);
        VFX_shape_lookaside(globalLogPtr->shapeLookaside[0]);

        for (int32_t shape = 0; shape < 5; ++shape)
        {
            VFX_shape_translate_draw(picture->frame(), globalLogPtr->vehicleRepShapes[index], shape, 0, 0);
        }

        picture->copyTo(work->frame(), 0xed, 6, 1);
        delete picture;
        // The diagram, on the row and kept for the purchase dialog.
        auto* diagram = new lPort;
        picturePort = diagram;
        diagram->init(0x1e, 0x1e, 1);
        VFX_pane_wipe(diagram->frame(), 0x10);

        for (int32_t location = 0; location < 5; ++location)
        {
            AG_shape_draw(work->frame(), globalLogPtr->vehicleIconShapes[index], location, 9, 0x22);
            AG_shape_draw(diagram->frame(), globalLogPtr->vehicleIconShapes[index], location, 4, 0);
        }

        if (data->numAvailable < 1)
        {
            cLoadString(thisInstance, 0x385, soldOutText, 0xfe);
            stock = soldOutText;
        }
        else
        {
            std::snprintf(text, sizeof(text), "%d", data->numAvailable);
        }
    }

    writeText(yellowDropFont, work, 0x25, 0x12, stock);
    std::snprintf(text, sizeof(text), "%d", data->cost);
    writeText(yellowDropFont, work, 0x52, 0x12, text);

    if (data->description != nullptr)
    {
        drawDescription(scratch, 0xc6, 0x26, data->description, work, 6, 0x43);
    }

    VFX_pane_copy(work->frame(), 0, 0, screen->purVehiclePort->frame(), 0, winHeight * rowIndex, -1);
    delete scratch;
    delete work;
    // Port fix: the original left ownPort pointing at the freed work port.
    ownPort = nullptr;
}

auto VehiclePurchaseBlock::setBar() -> void
{
}

// CompPurchaseBlock

CompPurchaseBlock::~CompPurchaseBlock()
{
    CompPurchaseBlock::destroy();
}

auto CompPurchaseBlock::init(_LogInventoryItem* newItem) -> void
{
    item = newItem;
    lObject::init(0, 0, 0x19a, 0x70, nullptr, globalLogPtr->purchaseScreen->lport());
    const MasterComponent& component = MasterComponentList[item->masterID];
    int32_t form = component.form;
    char format[256];
    cLoadString(thisInstance, 0x27f, format, 0xfe);
    std::snprintf(weightText, sizeof(weightText), format, static_cast<double>(component.tonnage));
    char text[256];

    if (form != COMPONENT_FORM_WEAPON_ENERGY && form != COMPONENT_FORM_WEAPON_BALLISTIC &&
        form != COMPONENT_FORM_WEAPON_MISSILE)
    {
        cLoadString(thisInstance, 0x6c, text, 0xfe);

        if (form == COMPONENT_FORM_PROBE)
        {
            std::snprintf(rangeText, sizeof(rangeText), "%s", text);
        }
        else
        {
            // Original behaviour (OB-078): other equipment than ECM and sensors formats the item pointer's bits as
            // the range, which read "0.0 m".
            float range = 0.0f;

            if (form == COMPONENT_FORM_ECM || form == COMPONENT_FORM_SENSOR)
            {
                range = MasterComponentList[item->masterID].rangeOrHeat;
            }

            std::snprintf(rangeText, sizeof(rangeText), "%.1f m", static_cast<double>(range));
        }

        std::snprintf(damageText, sizeof(damageText), "%s", text);
        std::snprintf(recycleText, sizeof(recycleText), "%s", text);
        return;
    }

    // Weapons: range, damage and recycle time with their rating words.
    float range = component.weaponRange[3];
    cLoadString(thisInstance, range < 76.0f ? 0x55 : range < 151.0f ? 0x50 : 0x6d, text, 0xfe);
    std::snprintf(rangeText, sizeof(rangeText), "%s", text);
    float damage = component.damage;

    if (component.weaponFlags == 4)
    {
        damage = static_cast<float>(damage * 3.0);
    }

    uint32_t damageId = damage < 1.0f   ? 100u
                        : damage < 3.0f ? 0x4fu
                        : damage < 5.0f ? 0x65u
                        : damage < 7.0f ? 0x51u
                        : damage < 9.0f ? 0x66u
                                        : 0x67u;
    cLoadString(thisInstance, damageId, text, 0xfe);
    std::snprintf(damageText, sizeof(damageText), "%.2f (%s)", static_cast<double>(damage), text);
    float recycle = component.recycleTime;
    uint32_t recycleId = recycle < 2.0f   ? 0x68u
                         : recycle < 3.0f ? 0x69u
                         : recycle < 5.0f ? 0x65u
                         : recycle < 8.0f ? 0x6au
                                          : 0x6bu;
    cLoadString(thisInstance, recycleId, text, 0xfe);
    std::snprintf(recycleText, sizeof(recycleText), "%.2f s (%s)", static_cast<double>(recycle), text);
}

auto CompPurchaseBlock::destroy() -> void
{
    lObject::destroy();
}

auto CompPurchaseBlock::handleEvent(aEvent* event) -> void
{
    if (globalLogPtr->currentScreen == globalLogPtr->repairScreen)
    {
        return;
    }

    if (parent != nullptr && compDrag.dragging == 0 && compDrag.carrying == 0 && (event->type == 8 || event->type == 9))
    {
        parent->handleEvent(event);
        return;
    }

    int32_t type = event->type;
    char text[256];

    switch (type)
    {
        case 1:
        case 3:
        {
            if (compDrag.dragging != 0 || compDrag.carrying != 0)
            {
                return;
            }

            if (onRow(this, event) && item->count != 0)
            {
                // Pick the component up (left button drags, right button carries). The cursor stays shown.
                if (type == 1)
                {
                    compDrag.dragging = 1;
                }
                else
                {
                    compDrag.carrying = 1;
                }

                playSample(0x35);
                application->showCursor(1);
                application->grab(this);
                compDrag.y = event->y - 0x10;
                compDrag.x = event->x - 0x10;
                makeDragIcon(compDrag, row, 0x21);
                return;
            }
            break;
        }

        case 4:
        {
            if (compDrag.carrying != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 6:
        {
            if (compDrag.dragging != 0 && type == 6)
            {
                return;
            }

            if (application->grabbedObject() == nullptr)
            {
                return;
            }

            application->showCursor(1);
            application->release();
            compDrag.dragging = 0;
            compDrag.carrying = 0;
            deleteDragIcon();

            if (type != 6 && !overInventory(event))
            {
                if (overStore(event))
                {
                    playSample(0x34);
                    return;
                }
                break;
            }

            // Buy some.
            _LogInventoryItem* bought = item;
            MasterComponent& component = MasterComponentList[bought->masterID];

            if (component.resourcePoints <= ResourcePoints)
            {
                playSample(0x34);
                globalItemPtr = bought;
                auto* picture = new lPort;
                loadArt(picture, "%slogart\\lscicc%02d.tga", bought->rangeIndex);
                globalLogPtr->purchaseDialog->init(4, component.resourcePoints, bought->count, component.name, nullptr,
                                                   picture);
                delete picture;
                globalLogPtr->purchaseDialog->setPort(globalLogPtr->currentScreen->lport());
                globalLogPtr->purchaseDialog->setCallback(CompPurchaseCallback);
                globalLogPtr->purchaseDialog->activate();
                return;
            }

            showMessage(0x4d);
            break;
        }

        case 7:
        {
            if (compDrag.dragging != 0)
            {
                compDrag.y = event->y - 0xf;
                compDrag.x = event->x - 0xf;
                globalLogPtr->dragIcon->moveTo(compDrag.x, compDrag.y, 0);
                return;
            }

            if (event->key == 0)
            {
                cLoadString(thisInstance, 0x30, text, 0xfe);
                globalLogPtr->ticker->setString(text);
            }

            return;
        }

        default:
            return;
    }

    playSample(0x33);
}

auto CompPurchaseBlock::drawBackground(int32_t rowIndex, int32_t) -> void
{
    PurchaseScreen* screen = globalLogPtr->purchaseScreen;
    auto* work = new lPort;
    ownPort = work;
    work->init(screen->compTabPort->width(), screen->compTabPort->height(), 1);
    screen->compTabPort->copyTo(work->frame(), 0, 0, 0);
    _LogInventoryItem* shown = item;
    char text[1024];

    if (shown->count < 0)
    {
        char format[256];
        cLoadString(thisInstance, 0x385, format, 0xfe);
        std::snprintf(text, sizeof(text), format, shown->count);
    }
    else
    {
        std::snprintf(text, sizeof(text), "%d", shown->count);
    }

    writeText(yellowDropFont, work, 0x26, 0x12, text);
    std::snprintf(text, sizeof(text), "%d", MasterComponentList[shown->masterID].resourcePoints);
    writeText(yellowDropFont, work, 0x52, 0x12, text);

    auto* scratch = new lPort;
    int32_t picture = item->rangeIndex;
    const char* nameArt;

    if (item->count == 0)
    {
        // Sold out: a blank icon, the "sold out" picture and name art.
        scratch->init(0x1e, 0x1e, 1);
        VFX_pane_wipe(scratch->frame(), 0x10);
        scratch->copyTo(work->frame(), 7, 0x22, 1);
        scratch->destroy();
        loadArt(scratch, "%slogart\\lspidc%02d.tga", picture);
        scratch->copyTo(work->frame(), 0xed, 6, 1);
        nameArt = "%slogart\\lspfdc%02d.tga";
    }
    else
    {
        loadArt(scratch, "%slogart\\lscicc%02d.tga", picture);
        scratch->copyTo(work->frame(), 7, 0x22, 1);
        scratch->destroy();
        loadArt(scratch, "%slogart\\lspilc%02d.tga", picture);
        scratch->copyTo(work->frame(), 0xed, 6, 1);
        nameArt = "%slogart\\lspflc%02d.tga";
    }

    char fileName[256];
    std::snprintf(fileName, sizeof(fileName), nameArt, artPath, picture);
    scratch->destroy();
    scratch->init(fileName);
    scratch->copyTo(work->frame(), 5, 4, 1);
    writeText(yellowDropFont, work, 0x52, 0x35, rangeText);
    writeText(yellowDropFont, ownPort, 0x52, 0x2c, damageText);
    writeText(yellowDropFont, ownPort, 0x52, 0x23, recycleText);

    if (item->description != nullptr)
    {
        drawDescription(scratch, 0xc6, 0x25, item->description, ownPort, 6, 0x44);
    }

    lPort* row0 = ownPort;
    row0->copyTo(screen->purCompPort->frame(), 0, winHeight * rowIndex, 1);
    delete scratch;
    delete row0;
    ownPort = nullptr;
}

// PilotPurchaseBlock

PilotPurchaseBlock::~PilotPurchaseBlock()
{
    PilotPurchaseBlock::destroy();
}

auto PilotPurchaseBlock::init(PurPilotData* newPilot) -> void
{
    pilot = newPilot;
    lObject::init(0, 0, 0x19a, 0x70, nullptr, globalLogPtr->purchaseScreen->lport());
}

auto PilotPurchaseBlock::destroy() -> void
{
    pilot = nullptr;
    lObject::destroy();
}

auto PilotPurchaseBlock::handleEvent(aEvent* event) -> void
{
    if (globalLogPtr->currentScreen == globalLogPtr->repairScreen)
    {
        return;
    }

    if (parent != nullptr && pilotDrag.dragging == 0 && pilotDrag.carrying == 0 &&
        (event->type == 8 || event->type == 9))
    {
        parent->handleEvent(event);
        return;
    }

    int32_t type = event->type;
    char text[256];

    switch (type)
    {
        case 1:
        {
            if (pilotDrag.carrying != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 3:
        {
            if (pilotDrag.dragging == 0 && onRow(this, event))
            {
                // Pick the pilot up (left button drags, right button carries). The cursor stays shown.
                if (type == 1)
                {
                    pilotDrag.dragging = 1;
                }
                else
                {
                    pilotDrag.carrying = 1;
                }

                soundSystem->playPilotSpeech(pilot->pilotAudio, 10);
                application->grab(this);
                pilotDrag.y = event->y - 0x10;
                pilotDrag.x = event->x - 0x10;
                makeDragIcon(pilotDrag, row, 0x25);
                return;
            }
            break;
        }

        case 4:
        {
            if (pilotDrag.carrying != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 6:
        {
            if (pilotDrag.dragging != 0 && type == 6)
            {
                break;
            }

            pilotDrag.carrying = 0;

            if (application->grabbedObject() == nullptr)
            {
                break;
            }

            application->release();
            pilotDrag.dragging = 0;
            deleteDragIcon();

            if (type != 6 && !overInventory(event))
            {
                playSample(overStore(event) ? 0x34 : 0x33);
                return;
            }

            // Hire the pilot.
            PurPilotData* hired = pilot;

            if (ResourcePoints < hired->cost)
            {
                playSample(0x33);
                showMessage(0x4d);
                return;
            }

            globalPilotPurchaseBlock = this;
            auto* picture = new lPort;
            loadArt(picture, "%slogart\\pilot%02d.tga", hired->nameIndex);
            globalLogPtr->purchaseDialog->init(2, hired->cost, 1, hired->callsign, nullptr, picture);
            delete picture;
            globalLogPtr->purchaseDialog->setPort(globalLogPtr->currentScreen->lport());
            globalLogPtr->purchaseDialog->setCallback(PilotPurchaseCallback);
            globalLogPtr->purchaseDialog->activate();
            break;
        }

        case 7:
        {
            if (pilotDrag.dragging != 0)
            {
                pilotDrag.y = event->y - 0xf;
                pilotDrag.x = event->x - 0xf;
                globalLogPtr->dragIcon->moveTo(pilotDrag.x, pilotDrag.y, 0);
                return;
            }

            if (event->key == 0)
            {
                cLoadString(thisInstance, 0x30, text, 0xfe);
                globalLogPtr->ticker->setString(text);
            }

            return;
        }
    }
}

auto PilotPurchaseBlock::drawBackground(int32_t rowIndex) -> void
{
    PurPilotData* shown = pilot;

    // A hired pilot (health cleared by PilotPurchaseCallback) is not drawn.
    if (shown->health == 0)
    {
        return;
    }

    PurchaseScreen* screen = globalLogPtr->purchaseScreen;
    auto* work = new lPort;
    ownPort = work;
    work->init(screen->pilotTabPort->width(), screen->pilotTabPort->height(), 1);
    screen->pilotTabPort->copyTo(work->frame(), 0, 0, 0);
    auto* scratch = new lPort;
    loadArt(scratch, "%slogart\\lspflp%02d.tga", shown->nameIndex);
    scratch->copyTo(work->frame(), 5, 4, 1);
    scratch->destroy();
    loadArt(scratch, "%slogart\\pilot%02d.tga", shown->nameIndex);
    scratch->copyTo(work->frame(), 7, 0x26, 1);
    scratch->destroy();
    char text[256];
    std::snprintf(text, sizeof(text), "%d", shown->cost);
    writeText(yellowDropFont, work, 0x1f, 0x12, text);

    // An out-of-range rank shows the text buffer's last contents (the price).
    if (shown->rank >= 0 && shown->rank <= 3)
    {
        cLoadString(thisInstance, 0x70 + static_cast<uint32_t>(shown->rank), text, 0xfe);
    }

    writeText(yellowDropFont, work, 0x9a, 0x2a, text);
    globalLogPtr->drawPilotSkillBar(shown->gunnery, 0x54, 0x22, 0, 0x36, 4, work);
    globalLogPtr->drawPilotSkillBar(shown->piloting, 0x54, 0x2b, 0, 0x36, 4, work);
    globalLogPtr->drawPilotSkillBar(shown->jumping, 0x54, 0x34, 0, 0x36, 4, work);
    globalLogPtr->drawPilotSkillBar(shown->sensors, 0x54, 0x3d, 0, 0x36, 4, work);
    // One pip per point of health.
    int32_t x = 0xe;

    for (int32_t pip = pilot->health; pip > 0; --pip, x += 3)
    {
        VFX_pixel_write(work->frame(), x, 0x22, 0xcf);
        VFX_pixel_write(work->frame(), x + 1, 0x22, 0xcf);
        VFX_pixel_write(work->frame(), x + 1, 0x23, 0xee);
        VFX_pixel_write(work->frame(), x, 0x23, 0xcf);
    }

    if (pilot->description != nullptr)
    {
        drawDescription(scratch, 0xc6, 0x25, pilot->description, work, 8, 0x48);
    }

    work->copyTo(screen->purPilotPort->frame(), 0, winHeight * rowIndex, 1);
    delete scratch;
    delete work;
    ownPort = nullptr;
}

// PurPilotList

auto PurPilotList::destroy() -> void
{
    for (PurPilotData* node = first; node != nullptr; node = first)
    {
        PilotPurchaseBlock* block = node->block;
        first = node->next;

        if (block != nullptr)
        {
            delete block;
            node->block = nullptr;
        }

        if (node->description != nullptr)
        {
            logFree(node->description);
            node->description = nullptr;
        }

        delete node;
    }

    first = nullptr;
    count = 0;
}

auto PurPilotList::addPilot(char* fileName, int32_t status) -> int32_t
{
    FullPathFileName path;
    auto* file = new FitIniFile;
    Assert(file != nullptr, 0, " no RAM for pilot file ", nullptr);
    openProfile(file, path, warriorPath, fileName, true, " could not open scenario file ");

    auto* data = new PurPilotData;
    auto* block = new PilotPurchaseBlock;
    data->block = block;
    block->init(data);
    std::strncpy(data->fileName, fileName, sizeof(data->fileName));
    int32_t result = file->seekBlock("General");
    Assert(result == 0, result, " could not find general block in pilot file ", nullptr);
    result = file->readIdLong("NameIndex", data->nameIndex);
    Assert(result == 0, result, "could not read NameIndex in pilot profile", nullptr);
    char callsign[0x80];
    result = file->readIdString("Callsign", callsign, 0x14);
    Assert(result == 0, result, " could not read callsign in pilot file ", nullptr);
    std::strcpy(data->callsign, callsign);
    result = file->readIdString("pilotAudio", data->pilotAudio, 0xff);
    Assert(result == 0, result, " Could not find pilotAudio in General Block ", nullptr);
    data->descIndex = -1;
    data->description = nullptr;
    file->readIdLong("DescIndex", data->descIndex);
    data->loadDescription(data->descIndex);
    result = file->seekBlock("Skills");
    Assert(result == 0, result, " could not find skills block in pilot file ", nullptr);
    result = file->readIdChar("Piloting", data->piloting);
    Assert(result == 0, result, " could not read Piloting in pilot file ", nullptr);
    result = file->readIdChar("Gunnery", data->gunnery);
    Assert(result == 0, result, " could not read Gunnery in pilot file ", nullptr);
    result = file->readIdChar("Jumping", data->jumping);
    Assert(result == 0, result, " could not read Jumping in pilot file ", nullptr);
    result = file->readIdChar("Sensors", data->sensors);
    Assert(result == 0, result, " could not read Sensors in pilot file ", nullptr);
    result = file->seekBlock("Status");
    Assert(result == 0, result, " could not find status block in pilot file ", nullptr);
    result = file->readIdChar("Wounds", data->health);
    Assert(result == 0, result, " could not read Wounds in pilot file ", nullptr);
    data->health = static_cast<char>(6 - data->health);
    data->rank = 0;
    data->calcRank();
    data->cost = globalLogPtr->pilotCosts[data->rank];
    data->status = status;

    // Insert by rank, then (among the pilots from there on) by callsign.
    PurPilotData* previous = nullptr;
    PurPilotData* node = first;

    while (node != nullptr && node->rank < data->rank)
    {
        previous = node;
        node = node->next;
    }
    while (node != nullptr && std::strcmp(node->callsign, data->callsign) < 0)
    {
        previous = node;
        node = node->next;
    }

    data->next = node;

    if (previous != nullptr)
    {
        previous->next = data;
    }
    else
    {
        first = data;
    }

    ++count;
    file->close();
    delete file;
    return 0;
}

auto PurPilotList::removePilot(int32_t index) -> int32_t
{
    if (count <= index)
    {
        return -1;
    }

    PurPilotData* previous = nullptr;
    PurPilotData* node = first;

    for (; index > 0; --index)
    {
        previous = node;
        node = node->next;
    }

    if (previous != nullptr)
    {
        previous->next = node->next;
    }
    else
    {
        first = node->next;
    }

    delete node;
    --count;
    return 0;
}

auto PurPilotList::setPilotStatus(int32_t pilotId, int32_t status) -> void
{
    PurPilotData* node = first;

    for (int32_t index = 0; index < count; ++index, node = node->next)
    {
        if (node->descIndex == pilotId)
        {
            node->status = status;
            return;
        }
    }
}

auto PurPilotList::getPilotInfo(int32_t index, PurPilotData*& pilot) -> int32_t
{
    if (count <= index)
    {
        return -1;
    }

    PurPilotData* node = first;

    for (; index > 0; --index)
    {
        node = node->next;
    }

    pilot = node;
    return 0;
}

auto PurPilotList::getVisiblePilotCount() -> int32_t
{
    int32_t visible = 0;

    for (PurPilotData* node = first; node != nullptr; node = node->next)
    {
        if (node->status == 0)
        {
            ++visible;
        }
    }

    return visible;
}

// Data records

auto PurMechData::loadDescription(int32_t index) -> void
{
    // The original loops three times over this same record; only the first pass can load.
    if (index < 0)
    {
        return;
    }

    if (description == nullptr)
    {
        description = loadDescriptionText(descIndex);
    }
}

auto PurMechData::calcBR() -> int32_t
{
    battleRating = chassisBR;

    for (_LogInventoryItem* item = inventory->items; item != nullptr; item = item->next)
    {
        battleRating = static_cast<int32_t>(
            static_cast<double>(MasterComponentList[item->masterID].battleRating) * item->count + battleRating);
    }

    return battleRating;
}

auto PurPilotData::calcRank() -> void
{
    // The skills weighted (piloting, jumping, sensors, gunnery); the rank is the first scale entry above it.
    double weighted =
        (static_cast<double>(gunnery) * SkillWeightings[3] + static_cast<double>(sensors) * SkillWeightings[2] +
         static_cast<double>(jumping) * SkillWeightings[1] + static_cast<double>(piloting) * SkillWeightings[0]) /
        (static_cast<double>(SkillWeightings[3]) + SkillWeightings[2] + SkillWeightings[1] + SkillWeightings[0]);

    for (int32_t level = 0; level < 4; ++level)
    {
        if (weighted < WarriorRankScale[level])
        {
            rank = level;
            return;
        }
    }
}

auto PurPilotData::loadDescription(int32_t index) -> void
{
    if (index > -1 && description == nullptr)
    {
        description = loadDescriptionText(descIndex);
    }
}

auto PurVehicleData::loadDescription(int32_t index) -> void
{
    if (index > -1 && description == nullptr)
    {
        description = loadDescriptionText(descIndex);
    }
}

auto PurVehicle::calcVehicleCost() -> void
{
    PurVehicleData* vehicle = data;
    vehicle->cost = vehicle->baseCost;

    for (_LogInventoryItem* item = vehicle->inventory->items; item != nullptr; item = item->next)
    {
        vehicle->cost += MasterComponentList[item->masterID].resourcePoints * item->count;
    }
}
