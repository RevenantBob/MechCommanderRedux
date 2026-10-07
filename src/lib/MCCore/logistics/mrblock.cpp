#include "stdafx.h"
#include "logistics/mrblock.h"
#include "gui/afont.h"
#include "gui/scrlpane.h"
#include "gui/updisp.h"
#include "lib/aerror.h"
#include "logistics/invblock.h"
#include "logistics/logbri.h"
#include "logistics/logdlg.h"
#include "logistics/loggen.h"
#include "logistics/logmain.h"
#include "logistics/logpur.h"
#include "logistics/logrep.h"
#include "logistics/logscrn.h"
#include "logistics/purchase.h"
#include "logistics/ticker.h"
#include "main/logistics.h"
#include "main/main.h"
#include "mission/mission.h"
#include "network/multplyr.h"
#include "object/cmponent.h"
#include "sound/soundsys.h"
#include "vfx/vfxfuncs.h"

// 0x008009d0 is AlphaTable row 0x100 (AlphaTable is at 0x007f09d0): the alpha colour greyed-out rows darken through.
char* g_logistic_fadetable = AlphaTable + 0x100 * 256;

namespace
{
    /// <summary>
    /// The repair block whose missing replacement parts the refit dialog is showing (0x00808694). The original names it
    /// <c>globalMechPurchaseBlock</c>, like purchase.cpp's <c>MechPurchaseBlock*</c>; the port renames this one.
    /// </summary>
    MechRepairBlock* refitBlock = nullptr;

    /// <summary>The dragged item copy's <c>itemNum</c> (0x0080868c).</summary>
    uint8_t dragItemNum = 0;
    /// <summary>The dragged item's component (0x0080868d).</summary>
    uint8_t dragMasterID = 0;
    /// <summary>-1 while something is dragged with the left button held (0x00808698).</summary>
    int32_t leftDrag = 0;
    /// <summary>The mech block's drag icon position (0x0080869c / 0x008086a0).</summary>
    int32_t dragX = 0;
    int32_t dragY = 0;
    /// <summary>Set by a right-button press, cleared by the release (0x008086a8).</summary>
    int32_t rightHeld = 0;
    /// <summary>-1 while the whole mech is dragged (0x008086ac).</summary>
    int32_t draggingMech = 0;
    /// <summary>-1 while an item is dragged out of the weapon list (0x008086b0).</summary>
    int32_t draggingItem = 0;
    /// <summary>-1 while a repair slider is dragged; <c>lastY</c> is the slider, <c>lastX</c> its position (0x008086b4).</summary>
    int32_t draggingSlider = 0;
    /// <summary>Set to -1 when a click selects another mech; never read (0x008086b8).</summary>
    int32_t mechSelected = 0;
    /// <summary>-1 while a repair button is held (the buttons are redrawn on release; 0x008086bc).</summary>
    int32_t repairButtonDown = 0;
    /// <summary>The dragged item copy's damage (0x008086c0).</summary>
    int32_t dragItemHits = 0;
    /// <summary>The dragged item's inventory index (0x0079fff8, -1 when none).</summary>
    int32_t dragItemIndex = -1;

    /// <summary>-1 while the vehicle is dragged with the left button held (0x008086c4).</summary>
    int32_t vehicleLeftDrag = 0;
    /// <summary>The vehicle block's drag icon position (0x008086c8 / 0x008086cc).</summary>
    int32_t vehicleDragX = 0;
    int32_t vehicleDragY = 0;
    /// <summary>Set by a right-button press on a vehicle, cleared by the release (0x008086d0).</summary>
    int32_t vehicleRightHeld = 0;
    /// <summary>-1 while the vehicle is dragged (0x008086d4).</summary>
    int32_t draggingVehicle = 0;
    /// <summary>A pilot dropped on a vehicle (0x008086d8). Nothing sets it: vehicles take no pilots.</summary>
    LogWarrior* vehiclePilot = nullptr;
    /// <summary>The pilot index the vehicle release shifts the pilots from (0x007a0000, always -1).</summary>
    int32_t vehiclePilotShift = -1;

    void* logAlloc(uint32_t size)
    {
        return globalLogPtr->logisticsBlocks->Allocate(size);
    }

    void logFree(void* block)
    {
        globalLogPtr->logisticsBlocks->Free(block);
    }

    void playSample(uint32_t sampleId)
    {
        soundSystem->playDigitalSample(sampleId, 1, nullptr, 0, 0);
    }

    void writeText(aFont* font, PANE* pane, int32_t x, int32_t y, const char* text)
    {
        font->writeString(pane, x, y, reinterpret_cast<uint8_t*>(const_cast<char*>(text)), -1);
    }

    void drawLine(PANE* pane, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t color)
    {
        VFX_line_draw(pane, x0, y0, x1, y1, LD_DRAW, color);
    }

    RepairScreen* repairScreen()
    {
        return globalLogPtr->repairScreen;
    }

    /// <summary>The port the repair screen's unit rows are drawn into.</summary>
    lPort* unitRowsPort()
    {
        lPort* port = nullptr;
        repairScreen()->unitPane->getDisplayPort(port);
        return port;
    }

    int32_t componentForm(uint8_t masterID)
    {
        return MasterComponentList[masterID].form;
    }

    bool isWeapon(int32_t form)
    {
        return form == 7 || form == 8 || form == 9;
    }

    bool isEquipment(int32_t form)
    {
        return form == 2 || form == 0x10 || form == 0x11;
    }

    /// <summary>A weapon with its own ammo (forms 8 and 9): moving it moves an ammo item too.</summary>
    bool usesAmmo(uint8_t masterID)
    {
        return componentForm(masterID) == 8 || componentForm(masterID) == 9;
    }

    /// <summary>
    /// A weapon's worth in the condition figures: its damage per 10 seconds, as a short, times its long range over 24,
    /// as a short.
    /// </summary>
    int16_t weaponValue(const MasterComponent& component)
    {
        auto perTen = static_cast<int16_t>(static_cast<int32_t>(static_cast<double>(component.damage) * 10.0 /
                                                                static_cast<double>(component.recycleTime)));
        return static_cast<int16_t>(
            static_cast<int32_t>(static_cast<double>(perTen) * static_cast<double>(component.weaponRange[3]) *
                                 static_cast<double>(0.041666668f)));
    }

    /// <summary>
    /// The damage state of a location from its percentage left: 1 above 75, 2 above 50, 3 above 25, 4 above 0, 0 when
    /// gone.
    /// </summary>
    int32_t damageState(uint32_t percent)
    {
        if (percent >= 0x33)
        {
            return percent < 0x4c ? 2 : 1;
        }

        if (percent >= 0x1a)
        {
            return 3;
        }

        return percent != 0 ? 4 : 0;
    }

    /// <summary>The percentage of <paramref name="maximum"/> that <paramref name="current"/> is.</summary>
    /// <remarks>Port fix: a zero maximum counts as 0% (the original divided by zero).</remarks>
    uint32_t percentLeft(uint8_t current, uint8_t maximum)
    {
        if (maximum == 0)
        {
            return 0;
        }

        return static_cast<uint32_t>(current) * 100 / maximum;
    }

    /// <summary>The colour table of damage state <paramref name="state"/> for armor (the internal structure's is 5 on).</summary>
    uint8_t* armorLookaside(int32_t state)
    {
        static constexpr int32_t Tables[5] = {4, 0, 1, 2, 3};
        return globalLogPtr->shapeLookaside[Tables[state]];
    }

    uint8_t* internalLookaside(int32_t state)
    {
        static constexpr int32_t Tables[5] = {9, 5, 6, 7, 8};
        return globalLogPtr->shapeLookaside[Tables[state]];
    }

    /// <summary>
    /// Draws the tonnage bar at <paramref name="xPos"/>: a 0x35-pixel frame over rows 9..12 of <paramref name="pane"/>,
    /// filled <paramref name="fill"/> pixels.
    /// </summary>
    void drawTonnageBar(PANE* pane, int32_t xPos, int32_t fill)
    {
        drawLine(pane, xPos + 1, 9, xPos + 0x35, 9, 0xed);
        drawLine(pane, xPos, 10, xPos + 0x35, 10, 0xce);
        drawLine(pane, xPos, 11, xPos + 0x35, 11, 0xce);
        drawLine(pane, xPos + 1, 12, xPos + 0x35, 12, 0xae);
        drawLine(pane, xPos + 0x36, 10, xPos + 0x36, 11, 0xae);

        if (fill == 0)
        {
            return;
        }

        int32_t end = fill + xPos;
        drawLine(pane, xPos + 1, 9, end, 9, 0xe3);
        drawLine(pane, xPos, 10, xPos, 11, 0xe3);
        drawLine(pane, xPos + 1, 12, end, 12, 0xe5);
        drawLine(pane, end + 1, 10, end + 1, 11, 0xe5);
        drawLine(pane, xPos + 1, 10, end, 10, 0xe4);
        drawLine(pane, xPos + 1, 11, end, 11, 0xe4);
        drawLine(pane, end + 2, 10, end + 2, 11, 0x10);
        VFX_pixel_write(pane, end + 1, 9, 0x10);
        VFX_pixel_write(pane, end + 1, 12, 0x10);
    }

    /// <summary>The pixel offset of a scroll pane's view into its content.</summary>
    int32_t scrollPixels(ScrollPane* pane)
    {
        return static_cast<int32_t>(static_cast<double>(pane->scrollPos) * static_cast<double>(pane->scrollUnit));
    }

    /// <summary>Whether a point is over the pane, scroll bar included (the area its clicks go to).</summary>
    bool overPane(aObject* pane, int32_t xPos, int32_t yPos)
    {
        return pane != nullptr && pane->globalX() <= xPos && xPos <= pane->globalX() + pane->width() &&
               pane->globalY() <= yPos && yPos <= pane->globalY() + pane->height();
    }

    /// <summary>Whether the event is inside the pane, left of its scroll bar (all edges excluded).</summary>
    bool overPaneInside(aObject* pane, aEvent* event)
    {
        return pane->globalX() < event->x && event->x < pane->globalX() + pane->width() - 0xd &&
               pane->globalY() < event->y && event->y < pane->globalY() + pane->height();
    }

    void deleteDragIcon()
    {
        if (globalLogPtr->dragIcon != nullptr)
        {
            delete globalLogPtr->dragIcon;
        }

        globalLogPtr->dragIcon = nullptr;
    }

    /// <summary>
    /// Shows a component's info under the inventory: its picture (<c>lscicc</c>), the range, damage and recycle texts
    /// of its inventory block (made when missing) and its description.
    /// </summary>
    void drawItemInfo(_LogInventoryItem* item, InventoryList* inventory)
    {
        if (item->inventoryBlock == nullptr)
        {
            auto* block = new CompInventoryBlock;
            item->inventoryBlock = block;
            block->init(item);
            inventory->loadDescription(0, item);
        }

        PrepareInfoDescription(item->description);
        repairScreen()->ShowComponentInfo(item->inventoryBlock, true);
    }

    /// <summary>Shows a message box with string <paramref name="id"/> and an enabled OK button.</summary>
    void showMessage(uint32_t id)
    {
        char text[256];
        cLoadString(thisInstance, id, text, 0xfe);
        globalLogPtr->messageDialog->setText(text);
        globalLogPtr->messageDialog->setTwoButton(0);
        ReusableDialog* dialog = globalLogPtr->messageDialog;
        dialog->callback = nullptr;
        char upArt[] = "bh_okay.tga";
        char downArt[] = "bg_okay.tga";
        dialog->okButton->setUpPicture(upArt);
        globalLogPtr->messageDialog->okButton->setDownPicture(downArt);
        lDialogButton* button = globalLogPtr->messageDialog->okButton;
        button->disabled = 0;
        globalLogPtr->messageDialog->activate();
    }

    /// <summary>Takes a mech's brief block off the briefing screen.</summary>
    void hideBriefBlock(aObject* block)
    {
        // Port fix: a block without a parent is skipped (the original called through the null parent).
        if (block->parent != nullptr)
        {
            block->parent->removeChild(block);
        }

        block->ShowGUIWindow(0);
    }

    /// <summary>Sums the current (or maximum) points of <paramref name="count"/> locations.</summary>
    int32_t sumPoints(const LogMech::ArmorPoints* points, int32_t count, bool maximum)
    {
        int32_t sum = 0;

        for (int32_t i = 0; i < count; ++i)
        {
            sum += maximum ? points[i].maxArmor : points[i].curArmor;
        }

        return sum;
    }
}

MechRepairBlock::~MechRepairBlock()
{
    MechRepairBlock::destroy();
}

auto RefitItemCallback() -> void
{
    LogMech* mech = refitBlock->mech;

    for (_LogInventoryItem* item = mech->inventory->items; item != nullptr; item = item->next)
    {
        switch (componentForm(item->masterID))
        {
            case 2:
            case 7:
            case 8:
            case 9:
            case 0x10:
            case 0x11:
            {
                // Every damaged copy the player could not replace leaves the mech.
                _LogInventoryStat* stat = item->stats;

                while (stat != nullptr)
                {
                    if (stat->hits == 0)
                    {
                        stat = stat->next;
                        continue;
                    }

                    uint8_t masterID = item->masterID;

                    // Original behaviour (OB-081): the loop moves on to the next item before the removal and walks its
                    // stats whatever its form.
                    if (item->count == 1)
                    {
                        item = item->next;
                    }

                    float tonnage = MasterComponentList[masterID].tonnage;

                    if (usesAmmo(masterID))
                    {
                        uint8_t ammo = MasterComponentList[masterID].ammoMasterId;
                        tonnage = MasterComponentList[ammo].tonnage + tonnage;
                        mech->inventory->removeItem(ammo, -1);
                    }

                    uint8_t statID = stat->statID;
                    mech->usedTonnage -= tonnage;
                    mech->weaponTonnage -= tonnage;
                    mech->inventory->removeItem(masterID, statID);

                    if (item == nullptr)
                    {
                        goto done;
                    }

                    stat = item->stats;
                }
                break;
            }

            default:
                break;
        }

        if (item == nullptr)
        {
            break;
        }
    }
done:
    refitBlock->drawBackground(refitBlock->slotIndex, nullptr);
    repairScreen()->createCompInvBlock();
    repairScreen()->setUpCompInv(0, -1);
    refitBlock = nullptr;
}

auto MechRepairBlock::init(LogMech* logMech) -> void
{
    mech = logMech;
    ScrollPane* rows = repairScreen()->unitPane;
    shortRangeWeapons = nullptr;
    numShortRangeWeapons = 0;
    mediumRangeWeapons = nullptr;
    numMediumRangeWeapons = 0;
    longRangeWeapons = nullptr;
    numLongRangeWeapons = 0;
    equipment = nullptr;
    numEquipment = 0;
    itemHits = nullptr;
    numItems = 0;
    dragPort = nullptr;
    lPort* rowsPort = nullptr;
    rows->getDisplayPort(rowsPort);
    lObject::init(0, 0, 0x19a, 0x70, nullptr, rowsPort);
    listPosition = mech->nameIndex;

    auto* pane = new ScrollPane;

    if (pane != nullptr)
    {
        pane->init();
    }

    inventoryPane = pane;
    pane->init(0x62, 0x58, 0x135, 0x11, static_cast<char*>(nullptr));
    addChild(pane);

    char fileName[256];
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsrupm05.tga", artPath);
    sliderArtPort = new lPort;
    sliderArtPort->init(fileName);

    // The sliders' scales: 61 pixels over the total maximum; the starting points are the current values.
    int32_t totalArmor = 0;

    for (int32_t i = 0; i < 11; ++i)
    {
        totalArmor += mech->armor[i].maxArmor;
        startArmor[i] = mech->armor[i].curArmor;
    }

    armorPixelScale = static_cast<float>(61.0 / static_cast<double>(totalArmor));
    int32_t totalInternal = 0;

    for (int32_t i = 0; i < 8; ++i)
    {
        totalInternal += mech->internals[i].maxArmor;
        startInternal[i] = mech->internals[i].curArmor;
    }

    internalPixelScale = static_cast<float>(61.0 / static_cast<double>(totalInternal));
    setEngineSlider(-1);
    setArmorSlider(-1);
    setInternalSlider(-1);

    _LogInventoryItem* item = mech->inventory->items;

    while (componentForm(item->masterID) != 4)
    {
        item = item->next;
    }

    engineStat = item->stats;
    armorSliderStart = armorSliderPos;
    internalSliderStart = internalSliderPos;
    engineSliderStart = engineSliderPos;
}

auto MechRepairBlock::destroy() -> void
{
    LogInvScreen::ForgetInfoSource(this);

    if (inventoryPane != nullptr)
    {
        delete inventoryPane;
    }

    inventoryPane = nullptr;

    if (sliderArtPort != nullptr)
    {
        delete sliderArtPort;
        sliderArtPort = nullptr;
    }

    if (shortRangeWeapons != nullptr)
    {
        logFree(shortRangeWeapons);
        numShortRangeWeapons = 0;
        shortRangeWeapons = nullptr;
    }

    if (mediumRangeWeapons != nullptr)
    {
        logFree(mediumRangeWeapons);
        numMediumRangeWeapons = 0;
        mediumRangeWeapons = nullptr;
    }

    if (longRangeWeapons != nullptr)
    {
        logFree(longRangeWeapons);
        numLongRangeWeapons = 0;
        longRangeWeapons = nullptr;
    }

    if (equipment != nullptr)
    {
        logFree(equipment);
        numEquipment = 0;
        equipment = nullptr;
    }

    if (itemHits != nullptr)
    {
        logFree(itemHits);
        numItems = 0;
        itemHits = nullptr;
    }

    lObject::destroy();
}

auto MechRepairBlock::undeployMech() -> void
{
    LogMech* logMech = mech;

    if (logMech->deployed == 0)
    {
        return;
    }

    for (int32_t lance = 0; lance < 3; ++lance)
    {
        for (int32_t slot = 0; slot < 4; ++slot)
        {
            int32_t& unit = globalLogPtr->deploySlots[lance][slot].unit;

            if (unit < 0 || unit != slotIndex)
            {
                continue;
            }

            if (MPlayer != nullptr)
            {
                globalLogPtr->SendRemoveForceMessage(lance, slot);
            }

            hideBriefBlock(logMech->briefBlock);
            // Both loops end after the first match.
            slot = 5;
            lance = 5;
            unit = -1;
        }
    }

    logMech->deployed = 0;
}

auto MechRepairBlock::handleEvent(aEvent* event) -> void
{
    int32_t eventX = event->x;
    int32_t localX = eventX - globalX();
    int32_t eventY = event->y;
    int32_t localY = eventY - globalY();

    if (globalLogPtr->currentScreen == globalLogPtr->purchaseScreen)
    {
        return;
    }

    if (leftDrag == 0)
    {
        if (parent != nullptr && rightHeld == 0 && (event->type == 8 || event->type == 9))
        {
            parent->handleEvent(event);
            return;
        }

        if (event->key == 0)
        {
            // The ticker explains what the cursor is over.
            static constexpr struct
            {
                RECT area{};
                uint32_t stringId = 0;
            } HelpAreas[] = {
                {{0x72, 5, 0x7a, 0x6a}, 0x37},   {{0x88, 2, 0xd2, 5}, 0x38},      {{0xea, 3, 299, 0x12}, 0x39},
                {{0xea, 0x17, 299, 0x26}, 0x3a}, {{0xea, 0x37, 299, 0x3f}, 0x3b}, {{0xea, 0x4c, 299, 0x54}, 0x3c},
                {{0xea, 0x61, 299, 0x69}, 0x3d}, {{0x134, 6, 0x195, 0xf}, 0x3e},  {{0x134, 0x11, 0x195, 0x69}, 0x3f},
                {{0x81, 6, 0xe6, 0x6c}, 0x40},
            };

            POINT point = {localX, localY};
            RECT pilotArea = {1, 0x1c, 0x6a, 0x6c};
            uint32_t stringId = 0;

            if (PtInRect(&pilotArea, point) != 0)
            {
                stringId = mech->pilotIndex < 0 ? 0x35 : 0x36;
            }
            else
            {
                for (const auto& help : HelpAreas)
                {
                    if (PtInRect(&help.area, point) != 0)
                    {
                        stringId = help.stringId;
                        break;
                    }
                }
            }

            if (stringId != 0)
            {
                char text[256];
                cLoadString(thisInstance, stringId, text, 0xfe);
                globalLogPtr->ticker->setString(text);
            }
            else
            {
                globalLogPtr->ticker->setString(nullptr);
            }
        }
    }

    LogMech* logMech = mech;
    LogWarrior* warrior = nullptr;
    globalLogPtr->assignedWarriorList->getWarriorInfo(logMech->pilotIndex, warrior);
    int32_t eventType = event->type;

    switch (eventType)
    {
        case 1:
        {
            if (rightHeld != 0)
            {
                return;
            }
            break;
        }

        case 3:
        {
            if (leftDrag != 0)
            {
                return;
            }

            if (DebugFunction1(localX, localY) != 0)
            {
                return;
            }

            rightHeld = 1;
            break;
        }

        case 4:
        {
            if (rightHeld != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 6:
        {
            if (leftDrag != 0 && eventType == 6)
            {
                return;
            }

            rightHeld = 0;

            if (repairButtonDown != 0 && logMech == repairScreen()->selectedMech)
            {
                drawButtons(nullptr);
                repairButtonDown = 0;
            }

            if (application->grabbedObject() == nullptr)
            {
                return;
            }

            application->showCursor(-1);
            application->release();
            leftDrag = 0;
            drawButtons(nullptr);
            deleteDragIcon();
            aObject* inventory = repairScreen()->inventoryPane;
            bool droppedOnInventory = eventType == 6 || overPaneInside(inventory, event);

            if (draggingItem == 0)
            {
                if (draggingSlider != 0)
                {
                    draggingSlider = 0;
                    application->release();
                    drawArmorSlider(nullptr);
                    drawInternalSlider(nullptr);
                    drawEngineSlider(nullptr);
                    drawStatusBar();
                    drawButtons(nullptr);
                    return;
                }

                if (draggingMech == 0)
                {
                    // The pilot: dropped on the inventory, it leaves the mech.
                    application->release();
                    leftDrag = 0;
                    deleteDragIcon();
                    LogMech* pilotsMech = mech;
                    LogWarrior* pilot = nullptr;
                    globalLogPtr->assignedWarriorList->getWarriorInfo(pilotsMech->pilotIndex, pilot);

                    if (pilot != nullptr && droppedOnInventory)
                    {
                        if (pilotsMech->deployed != 0)
                        {
                            undeployMech();
                        }

                        LogMech* current = mech;
                        globalLogPtr->shiftPilots(current->pilotIndex, -1);
                        int32_t row = slotIndex;
                        pilot->assigned = 0;
                        globalLogPtr->setPilot(row, -1);
                        globalLogPtr->reorderWarriors();
                        current->calcBR();
                        current->calcPilotModifier();
                        current->repairBlock->drawBackground(current->repairBlock->slotIndex, nullptr);
                        drawBackground(row, nullptr);
                        repairScreen()->createPilotInvBlock();
                        repairScreen()->setUpPilotInv(-1, -1);
                        soundSystem->playPilotSpeech(pilot->pilotAudio, 2);
                        return;
                    }

                    playSample(0x33);
                    pilotsMech->repairBlock->drawBackground(pilotsMech->repairBlock->slotIndex, nullptr);
                    return;
                }

                // The whole mech: dropped on the inventory, it leaves the force.
                draggingMech = 0;

                if (!droppedOnInventory)
                {
                    playSample(0x33);
                    return;
                }

                if (mech->deployed != 0)
                {
                    undeployMech();
                }

                repairScreen()->selectMech(nullptr);
                playSample(0x34);
                LogMech* leaving = mech;
                int32_t pilotIndex = leaving->pilotIndex;

                if (pilotIndex >= 0)
                {
                    LogWarrior* pilot = nullptr;
                    globalLogPtr->assignedWarriorList->getWarriorInfo(pilotIndex, pilot);
                    pilot->assigned = 0;
                    globalLogPtr->shiftPilots(pilotIndex, -1);
                    globalLogPtr->setPilot(slotIndex, -1);
                    globalLogPtr->reorderWarriors();
                    repairScreen()->createPilotInvBlock();
                    repairScreen()->setUpPilotInv(-1, -1);
                }

                MechBriefBlock* brief = leaving->briefBlock;

                if (brief != nullptr && brief->parent != nullptr)
                {
                    brief->parent->removeChild(brief);
                    brief->ShowGUIWindow(0);
                }

                for (auto& lance : globalLogPtr->deploySlots)
                {
                    for (auto& slot : lance)
                    {
                        if (slot.unit < 0)
                        {
                            continue;
                        }

                        if (slot.unit == slotIndex)
                        {
                            slot.unit = -1;
                        }
                        else if (slotIndex < slot.unit)
                        {
                            slot.unit = slot.unit - 1;
                        }
                    }
                }

                leaving->deployed = 0;
                leaving->assigned = 0;

                if (leaving == repairScreen()->selectedMech)
                {
                    repairScreen()->selectedMech = nullptr;
                }

                globalLogPtr->reorderMechs();
                MechRepairBlock* block = leaving->repairBlock;

                if (block != nullptr && block->parent != nullptr)
                {
                    block->parent->removeChild(block);
                }

                repairScreen()->removeMechFromList(leaving);
                leaving->inventoryBlock->deleteDiagram();
                repairScreen()->createMechInvBlock();
                repairScreen()->setUpMechInv(-1, -1);
                return;
            }

            // An item from the weapon list.
            draggingItem = 0;
            repairScreen()->drawBlankInvInfoBlock(-1);

            if (droppedOnInventory)
            {
                // Into the component inventory (a damaged item is thrown away).
                if (mech->deployed != 0)
                {
                    undeployMech();
                }

                playSample(0x34);

                if (dragItemHits == 0)
                {
                    InventoryList* components = globalLogPtr->componentInventory;
                    _LogInventoryItem* item = components->getItemInfo(components->getIndexFromMasterID(dragMasterID));

                    if (item == nullptr)
                    {
                        _LogInventoryStat* stat = components->createStat(dragMasterID, 0, 1, 0, 0xff);
                        components->addItem(dragMasterID, stat, -1);
                        item = components->getItemInfo(components->getIndexFromMasterID(dragMasterID));
                        auto* block = new CompInventoryBlock;
                        item->inventoryBlock = block;
                        block->init(item);
                        item->inventoryBlock->inventoryIndex = globalLogPtr->componentInventory->numItems - 1;
                    }

                    if (item->count != 0)
                    {
                        ++item->count;
                        item->inventoryBlock->drawBackground();
                    }
                    else
                    {
                        item->count = 1;
                        repairScreen()->createCompInvBlock();
                        repairScreen()->setUpCompInv(0, -1);
                    }
                }
            }
            else
            {
                // Back into the selected mech (with a weapon's ammo).
                LogMech* target = repairScreen()->selectedMech;
                bool withAmmo = usesAmmo(dragMasterID);
                playSample(0x34);
                InventoryList* inventory = target->inventory;
                _LogInventoryStat* stat =
                    inventory->createStat(dragItemNum, static_cast<uint8_t>(dragItemHits), 0, 1, 0xff);
                inventory->addItem(dragMasterID, stat, -1);
                float tonnage = MasterComponentList[dragMasterID].tonnage;

                if (withAmmo)
                {
                    uint8_t ammo = MasterComponentList[dragMasterID].ammoMasterId;
                    stat = inventory->createStat(inventory->nextStatID, 0, 0, -1, 0xff);
                    inventory->addItem(ammo, stat, -1);
                    tonnage = MasterComponentList[ammo].tonnage + tonnage;
                }

                target->usedTonnage = tonnage + target->usedTonnage;
                target->weaponTonnage = tonnage + target->weaponTonnage;
                target->calcBR();
                target->repairBlock->drawBackground(target->repairBlock->slotIndex, nullptr);
                drawStatusBar();
                repairScreen()->setUpCompInv(0, -1);
            }

            dragItemIndex = -1;
            drawButtons(nullptr);
            return;
        }

        case 7:
        {
            auto* pane = static_cast<ScrollPane*>(child(0));

            if (pane->globalX() < eventX)
            {
                if (eventX < pane->globalX() + pane->width() - 0xd && pane->globalY() < eventY &&
                    eventY < pane->globalY() + pane->height())
                {
                    // Over the weapon list: show the item's info.
                    int32_t line = (eventY - pane->globalY() + pane->getScrollOffset()) / (greenFont->height() + 2);
                    _LogInventoryItem* item = getItemFromScrollPane(pane, line, &dragItemNum);

                    if (item != nullptr)
                    {
                        repairScreen()->drawBlankInvInfoBlock(2);
                        drawItemInfo(item, mech->inventory);
                        return;
                    }
                }
            }
            else if (eventX < pane->globalX())
            {
                // Over the mech: show its info.
                repairScreen()->drawBlankInvInfoBlock(0);

                if (dragPort == nullptr)
                {
                    dragPort = new lPort;
                    dragPort->init(0x1c, 0x1e, -1);
                    VFX_pane_wipe(dragPort->frame(), 0x10);

                    for (int32_t location = 0; location < 8; ++location)
                    {
                        globalLogPtr->drawMechBodyLoc(mech, location, dragPort, 2, 0);
                    }

                    // The battle rating bar along the left edge: 26 pixels at 18010.
                    int32_t bar =
                        static_cast<int32_t>(static_cast<double>(mech->battleRating) * (1.0 / 18010.0) * 26.0);
                    drawLine(dragPort->frame(), 0, 0x1b, 0, 0x1b - bar, 0xe4);
                    drawLine(dragPort->frame(), 1, 0x1b, 1, 0x1b - bar, 0xe4);
                }

                PrepareInfoDescription(mech->description);
                repairScreen()->ShowInfo(InvInfoBox::Kind::RepairMech, this);
            }

            if (leftDrag != 0)
            {
                dragX = eventX - 0xf;
                dragY = eventY - 0xf;
                globalLogPtr->dragIcon->moveTo(dragX, dragY, 0);
                return;
            }

            if (draggingSlider == 0)
            {
                return;
            }

            // Dragging a repair slider: undo the drag so far, then repair up to the new position as far as the resource
            // points go.
            if (mech->deployed != 0)
            {
                undeployMech();
            }

            if (dragPort != nullptr)
            {
                delete dragPort;
                dragPort = nullptr;
            }

            int32_t position = localX;

            if (position > 0x127)
            {
                position = 0x127;
            }

            int32_t newLastX = position;

            if (lastY == 0)
            {
                if (position < armorSliderStart)
                {
                    position = armorSliderStart;
                    drawArmorSlider(nullptr);
                    drawDamageDiagram(nullptr);
                    drawStatusBar();
                }

                LogMech* repaired = mech;
                int32_t before = sumPoints(repaired->armor, 11, false);

                for (int32_t i = 0; i < 11; ++i)
                {
                    repaired->armor[i].curArmor = static_cast<uint8_t>(startArmor[i]);
                }

                int32_t restored = sumPoints(repaired->armor, 11, false);
                int32_t cost = globalLogPtr->armorCost;
                ResourcePoints = ResourcePoints + (before - restored) * cost;
                int32_t points;

                if (position < 0x127)
                {
                    points = static_cast<int32_t>(static_cast<double>(position - armorSliderStart) /
                                                  static_cast<double>(armorPixelScale));
                }
                else
                {
                    points = sumPoints(repaired->armor, 11, true) - restored;
                }

                int32_t affordable = ResourcePoints / cost;

                if (points < affordable)
                {
                    repairArmor(points);
                    armorSliderPos = position;
                    ResourcePoints = ResourcePoints - globalLogPtr->armorCost * points;
                }
                else
                {
                    repairArmor(affordable);
                    armorSliderPos = static_cast<int32_t>(
                        static_cast<double>(affordable) * static_cast<double>(armorPixelScale) + armorSliderStart);
                    ResourcePoints = ResourcePoints - globalLogPtr->armorCost * affordable;
                }

                drawArmorSlider(nullptr);
                drawDamageDiagram(nullptr);
                drawStatusBar();
                newLastX = position;
            }
            else if (lastY == 1)
            {
                if (position < internalSliderStart)
                {
                    // Original behaviour (OB-080): the armor slider is the one redrawn.
                    position = internalSliderStart;
                    drawArmorSlider(nullptr);
                    drawDamageDiagram(nullptr);
                    drawStatusBar();
                }

                LogMech* repaired = mech;
                int32_t before = sumPoints(repaired->internals, 8, false);

                for (int32_t i = 0; i < 8; ++i)
                {
                    repaired->internals[i].curArmor = static_cast<uint8_t>(startInternal[i]);
                }

                int32_t restored = sumPoints(repaired->internals, 8, false);
                int32_t cost = globalLogPtr->internalCost;
                ResourcePoints = ResourcePoints + (before - restored) * cost;
                int32_t points;

                if (position < 0x127)
                {
                    points = static_cast<int32_t>(static_cast<double>(position - internalSliderStart) /
                                                  static_cast<double>(internalPixelScale));
                }
                else
                {
                    points = sumPoints(repaired->internals, 8, true) - restored;
                }

                int32_t affordable = ResourcePoints / cost;

                if (points < affordable)
                {
                    repairInternal(points);
                    internalSliderPos = position;
                    ResourcePoints = ResourcePoints - globalLogPtr->internalCost * points;
                    drawInternalSlider(nullptr);
                }
                else
                {
                    repairInternal(affordable);
                    internalSliderPos =
                        static_cast<int32_t>(static_cast<double>(affordable) * static_cast<double>(internalPixelScale) +
                                             internalSliderStart);
                    ResourcePoints = ResourcePoints - globalLogPtr->internalCost * affordable;
                    drawInternalSlider(nullptr);
                }

                drawDamageDiagram(nullptr);
                drawStatusBar();
                newLastX = position;
            }
            else if (lastY == 2)
            {
                if (engineStat->hits != 0 && mech->deployed != 0)
                {
                    // An inline undeploy without the network message (never reached: the mech was undeployed above).
                    for (int32_t lance = 0; lance < 3; ++lance)
                    {
                        for (int32_t slot = 0; slot < 4; ++slot)
                        {
                            int32_t& unit = globalLogPtr->deploySlots[lance][slot].unit;

                            if (unit < 0 || unit != slotIndex)
                            {
                                continue;
                            }

                            hideBriefBlock(mech->briefBlock);
                            slot = 5;
                            lance = 5;
                            unit = -1;
                        }
                    }

                    mech->deployed = 0;
                }

                int32_t target = position;

                if (position < engineSliderStart)
                {
                    // Original behaviour (OB-080): the armor slider is the one redrawn.
                    drawArmorSlider(nullptr);
                    drawDamageDiagram(nullptr);
                    drawStatusBar();
                    target = engineSliderStart;
                }

                // The engine repairs in whole damage levels; each costs engineCost.
                int32_t cost = globalLogPtr->engineCost;
                uint8_t& hits = engineStat->hits;
                int32_t newPosition;

                if (target < 0xf5)
                {
                    newPosition = 0xeb;
                    uint8_t old = hits;
                    hits = 3;
                    ResourcePoints = ResourcePoints + (3 - static_cast<int32_t>(old)) * cost;
                }
                else if (target < 0x109)
                {
                    newPosition = 0xff;
                    int32_t change = (2 - static_cast<int32_t>(hits)) * cost;

                    if (change < 0 && ResourcePoints < -change)
                    {
                        return;
                    }

                    hits = 2;
                    ResourcePoints = ResourcePoints + change;
                }
                else if (target < 0x11d)
                {
                    newPosition = 0x113;
                    int32_t change = (1 - static_cast<int32_t>(hits)) * cost;

                    if (change < 0 && ResourcePoints < -change)
                    {
                        return;
                    }

                    hits = 1;
                    ResourcePoints = ResourcePoints + change;
                }
                else
                {
                    newPosition = 0x127;
                    int32_t price = cost * static_cast<int32_t>(hits);

                    if (ResourcePoints < price)
                    {
                        return;
                    }

                    ResourcePoints = ResourcePoints - price;
                    hits = 0;
                }

                engineSliderPos = newPosition;
                drawEngineSlider(nullptr);
                drawStatusBar();
                newLastX = newPosition;
            }

            lastX = newLastX;
            mech->calcBR();
            drawBR(nullptr);
            return;
        }

        default:
            return;
    }

    // A button press (1 or 3).
    if (repairScreen()->selectedMech != mech)
    {
        repairScreen()->selectMech(mech);
        mechSelected = -1;
        rightHeld = 0;
        return;
    }

    auto* pane = static_cast<ScrollPane*>(child(0));

    if (pane->globalX() - 0xd + pane->width() <= eventX && eventX <= pane->globalX() + pane->width() &&
        pane->globalY() <= eventY && eventY <= pane->globalY() + pane->height())
    {
        // The weapon list's scroll bar.
        pane->handleEvent(event);
        drawInventory(nullptr);
        return;
    }

    if (pane->globalX() < eventX && eventX < pane->globalX() + pane->width() - 0xd && pane->globalY() < eventY &&
        eventY < pane->globalY() + pane->height())
    {
        // Pick an item out of the weapon list.
        int32_t line = (eventY - pane->globalY() + pane->getScrollOffset()) / (greenFont->height() + 2);
        _LogInventoryItem* item = getItemFromScrollPane(pane, line, &dragItemNum);

        if (item == nullptr)
        {
            playSample(0x33);
            return;
        }

        application->showCursor(0);
        dragMasterID = item->masterID;
        draggingItem = -1;
        playSample(0x35);
        application->grab(this);

        if (eventType == 1)
        {
            leftDrag = -1;
        }

        setUpItemDragIcon(item, dragItemNum, event, &dragItemIndex, &dragItemHits);
        return;
    }

    if (0 < localX && localX < 100 && 0x1c < localY && localY < 0x6d && mech->pilotIndex >= 0)
    {
        // Pick up the pilot: the portrait becomes the drag icon and its place is blanked.
        soundSystem->playPilotSpeech(warrior->pilotAudio, 10);
        application->showCursor(0);
        application->grab(this);

        if (eventType == 1)
        {
            leftDrag = -1;
        }

        dragY = winHeight * slotIndex + 0x25;
        dragX = 5;
        auto* icon = new DragIcon;
        globalLogPtr->dragIcon = icon;
        icon->Begin(eventX - 0xf, eventY - 0xf, 0x20, 0x20, [this](lPort* surface) { OnBeginDragPilot(surface); });
        clearPilot();
        repairScreen()->addChild(globalLogPtr->dragIcon);
        globalLogPtr->dragIcon->ShowGUIWindow(-1);
        globalLogPtr->dragIcon->setDepth(100);
        dragX = dragX + globalX();
        dragY = globalY() + 0x26;
        globalLogPtr->dragIcon->moveTo(eventX - 0xf, eventY - 0xf, 0);
        return;
    }

    if (localX < 0xe7)
    {
        // Pick up the whole mech.
        if (globalX() <= eventX && eventX <= globalX() + width() && globalY() <= eventY &&
            eventY <= globalY() + height())
        {
            playSample(0x35);
            application->showCursor(0);
            application->grab(this);
            draggingMech = -1;

            if (eventType == 1)
            {
                leftDrag = -1;
            }

            dragY = eventY - 0x10;
            dragX = eventX - 0x10;
            auto* icon = new DragIcon;
            globalLogPtr->dragIcon = icon;
            icon->Begin(dragX, dragY, 0x20, 0x20, [this](lPort* surface) { OnBeginDragMech(surface); });
            repairScreen()->addChild(globalLogPtr->dragIcon);
            globalLogPtr->dragIcon->ShowGUIWindow(-1);
            globalLogPtr->dragIcon->setDepth(100);
            globalLogPtr->dragIcon->moveTo(dragX, dragY, 0);
        }

        return;
    }

    if (MPlayer != nullptr)
    {
        return;
    }

    // The sliders.
    int32_t slider;
    int32_t sliderPos;

    if (armorSliderPos - 2 <= localX && localX <= armorSliderPos + 6 && 0x36 <= localY && localY <= 0x3d)
    {
        slider = 0;
        sliderPos = armorSliderPos;
    }
    else if (internalSliderPos - 2 <= localX && localX <= internalSliderPos + 6 && 0x4b <= localY && localY <= 0x52)
    {
        slider = 1;
        sliderPos = internalSliderPos;
    }
    else if (engineSliderPos - 2 <= localX && localX <= engineSliderPos + 6 && 0x60 <= localY && localY <= 0x67)
    {
        slider = 2;
        sliderPos = engineSliderPos;
    }
    else
    {
        // The repair buttons.
        if (localX < 0xea || 299 < localX)
        {
            return;
        }

        LogMech* repaired = mech;

        if (repairScreen()->selectedMech != repaired)
        {
            return;
        }

        repairButtonDown = -1;
        bool shortOfPoints = false;

        if (localY < 0x17 || 0x26 < localY || canRepairStructure == 0)
        {
            if (localY < 3 || 0x12 < localY || canRepairItems == 0)
            {
                return;
            }

            // Repair items: replace every damaged copy from the component inventory; list the ones missing.
            if (repaired->deployed != 0)
            {
                undeployMech();
            }

            char missing[256];
            missing[0] = '\0';
            repairButtonDown = -1;
            playSample(0x35);
            pressedButton = 1;
            UpdateDisplay(0, 0, 0, 0, 0);
            bool anyMissing = false;

            for (_LogInventoryItem* item = mech->inventory->items; item != nullptr; item = item->next)
            {
                int32_t form = componentForm(item->masterID);

                for (_LogInventoryStat* stat = item->stats; stat != nullptr; stat = stat->next)
                {
                    switch (form)
                    {
                        case 2:
                        case 7:
                        case 8:
                        case 9:
                        case 0x10:
                        case 0x11:
                        {
                            if (stat->hits == 0)
                            {
                                break;
                            }

                            InventoryList* components = globalLogPtr->componentInventory;
                            _LogInventoryItem* stockItem =
                                components->getItemInfo(components->getIndexFromMasterID(item->masterID));

                            if (stockItem == nullptr || stockItem->count == 0)
                            {
                                // Port fix: the name comes from the mech's own item when the inventory has none (the
                                // original read the name through the null item), and the list is bounded.
                                const char* name = stockItem != nullptr ? stockItem->name : item->name;
                                char entry[64];
                                std::snprintf(entry, sizeof(entry), anyMissing ? ",%s" : "%s", name);
                                std::strncat(missing, entry, sizeof(missing) - std::strlen(missing) - 1);
                                anyMissing = true;
                            }
                            else
                            {
                                --stockItem->count;
                                stockItem->inventoryBlock->drawBackground();
                                stat->hits = 0;
                            }
                            break;
                        }

                        default:
                        {
                            if (form != 4 && stat->hits != 0)
                            {
                                stat->hits = 0;
                            }
                            break;
                        }
                    }
                }
            }

            globalLogPtr->reIndexInventory();

            if (anyMissing)
            {
                // Ask whether to strip the damaged items that have no replacement (RefitItemCallback).
                application->release();
                leftDrag = 0;
                refitBlock = this;
                RefitDialog* dialog = globalLogPtr->refitDialog;
                dialog->setText(missing);
                dialog = globalLogPtr->refitDialog;
                dialog->callback = nullptr;
                dialog->setTwoButton(-1);
                char okUp[] = "bh_okay.tga";
                char okDown[] = "bg_okay.tga";
                char cancelUp[] = "bh_cancl.tga";
                char cancelDown[] = "bg_cancl.tga";
                globalLogPtr->refitDialog->okButton->setUpPicture(okUp);
                globalLogPtr->refitDialog->okButton->setDownPicture(okDown);
                globalLogPtr->refitDialog->okButton->callback()->setExec(RefitItemCallback);
                globalLogPtr->refitDialog->cancelButton->setUpPicture(cancelUp);
                globalLogPtr->refitDialog->cancelButton->setDownPicture(cancelDown);
                globalLogPtr->refitDialog->activate();
                playSample(0x33);
                drawBackground(slotIndex, nullptr);
                return;
            }

            repairScreen()->createCompInvBlock();
            repairScreen()->setUpCompInv(0, -1);
            drawBackground(slotIndex, nullptr);
            return;
        }

        // Repair the structure: the engine level by level, then internal structure, then armor.
        if (repaired->deployed != 0)
        {
            undeployMech();
        }

        if (dragPort != nullptr)
        {
            delete dragPort;
            dragPort = nullptr;
        }

        playSample(0x35);
        pressedButton = 2;
        UpdateDisplay(0, 0, 0, 0, 0);
        _LogInventoryStat* engine = engineStat;

        if (engine->hits != 0)
        {
            int32_t cost = globalLogPtr->engineCost;

            do
            {
                if (ResourcePoints < cost)
                {
                    shortOfPoints = true;
                    goto repaired;
                }

                ResourcePoints = ResourcePoints - cost;
                --engine->hits;
                mech->status = 0;
            } while (engine->hits != 0);
        }

        {
            LogMech* target = mech;
            int32_t points = sumPoints(target->internals, 8, true) - sumPoints(target->internals, 8, false);

            if (ResourcePoints < points * globalLogPtr->internalCost)
            {
                points = ResourcePoints / globalLogPtr->internalCost;
                repairInternal(points);
                shortOfPoints = true;
            }
            else
            {
                repairInternal(-1);
            }

            ResourcePoints = ResourcePoints - globalLogPtr->internalCost * points;

            if (!shortOfPoints)
            {
                target = mech;
                points = sumPoints(target->armor, 11, true) - sumPoints(target->armor, 11, false);

                if (ResourcePoints < globalLogPtr->armorCost * points)
                {
                    points = ResourcePoints / globalLogPtr->armorCost;
                    repairArmor(points);
                    shortOfPoints = true;
                }
                else
                {
                    repairArmor(-1);
                }

                ResourcePoints = ResourcePoints - globalLogPtr->armorCost * points;
            }
        }
    repaired:
        setArmorSlider(-1);
        setInternalSlider(-1);
        setEngineSlider(-1);
        drawBackground(slotIndex, nullptr);

        if (shortOfPoints)
        {
            application->release();
            leftDrag = 0;
            showMessage(0x57);
            playSample(0x33);
        }

        drawBackground(slotIndex, nullptr);
        return;
    }

    playSample(0x35);
    lastY = slider;
    draggingSlider = -1;
    lastX = sliderPos;
    application->grab(this);
}

auto MechRepairBlock::drawBackground(int32_t row, lPort* port) -> void
{
    const bool framed =
        globalLogPtr->currentScreen == globalLogPtr->repairScreen && repairScreen()->selectedMech == mech;
    const bool hasPilot = mech->pilotIndex >= 0 || mech->networkPilot != nullptr;

    if (port == nullptr)
    {
        // The repair screen's rows are drawn each frame (DrawRow) from the state; the pilot is back in place and no
        // button shows pressed. The rest is what the paint did besides painting (the weapon lists, the battle rating).
        pilotLifted = false;
        pressedButton = 0;

        if (hasPilot)
        {
            setPilotStats(nullptr);
            setPilotHealth(nullptr);
        }

        drawButtons(nullptr);
        drawDamageDiagram(nullptr);
        drawBR(nullptr);
        drawArmorSlider(nullptr);
        drawInternalSlider(nullptr);
        drawEngineSlider(nullptr);
        setInventory(nullptr);
        drawInventory(nullptr);
        return;
    }

    // The briefing box's picture.
    PaintBase(port, 0, row < 0, framed);

    if (hasPilot)
    {
        setPilotStats(port);
        setPilotHealth(port);
    }

    drawButtons(port);
    drawDamageDiagram(port);
    drawBR(port);
    drawArmorSlider(port);
    drawInternalSlider(port);
    drawEngineSlider(port);
}

auto MechRepairBlock::PaintBase(lPort* port, int32_t top, bool briefing, bool framed) -> void
{
    lPort* rowArt = briefing ? logArtf("%slogart\\lsbbkm00.tga", artPath) : globalLogPtr->repairBackPort;

    if (rowArt == nullptr)
    {
        return;
    }

    // Drawn in place: the original put the base together in a new picture (zeroed, as the port's heap gives it) and
    // copied it as it is.
    lBlockPort back(port->frame(), 0, top, rowArt->width(), rowArt->height(), false);

    if (briefing)
    {
        VFX_pane_copy(rowArt->frame(), 0, 0, back.frame(), 0, 0, -1);
    }
    else
    {
        VFX_pane_wipe(back.frame(), 0);
        rowArt->copyTo(back.frame(), 0, 0, -1);
    }

    LogMech* logMech = mech;
    const char* chassisArt = nullptr;

    if (logMech->nameVariant == 0)
    {
        chassisArt = "%slogart\\lscflma%02d.tga";
    }
    else if (logMech->nameVariant == 1)
    {
        chassisArt = "%slogart\\lscflmw%02d.tga";
    }
    else if (logMech->nameVariant == 2)
    {
        chassisArt = "%slogart\\lscflmj%02d.tga";
    }

    if (lPort* art = chassisArt != nullptr ? logArtf(chassisArt, artPath, logMech->nameIndex) : nullptr)
    {
        art->copyTo(back.frame(), 5, 4, -1);
    }

    if (lPort* art = logArtf("%slogart\\lscdsm%02d.tga", artPath, logMech->nameIndex))
    {
        art->copyTo(back.frame(), briefing ? 0xdc : 0xd6, 8, -1);
    }

    char format[256];
    char text[92];
    cLoadString(thisInstance, 0x4e, format, 0xfe);
    std::snprintf(text, sizeof(text), format, static_cast<double>(logMech->curTonnage), logMech->weightClassName);
    writeText(blueDropFont, back.frame(), 6, 0x12, text);

    if (framed)
    {
        // The selected mech's frame.
        drawLine(back.frame(), 1, 0, width() + 1, 0, 0xf2);
        drawLine(back.frame(), 1, 0, 1, height() - 2, 0xf2);
        drawLine(back.frame(), 1, height() - 2, width() + 1, height() - 2, 0xf2);
        drawLine(back.frame(), width() + 1, 0, width() + 1, height() - 2, 0xf2);
    }
}

auto MechRepairBlock::drawButtons(lPort* port) -> void
{
    bool onRows = port == nullptr;

    // The buttons are live when there is something to repair. (Painting the briefing box, the original found
    // nothing to repair: it only looked for the repair screen's rows.)
    canRepairItems = onRows && ItemsDamaged() ? 1 : 0;
    canRepairStructure = onRows && StructureDamaged() ? 1 : 0;

    if (onRows)
    {
        // The rows draw their buttons each frame (DrawRow).
        return;
    }

    PaintButtons(port, 0, false, canRepairItems, canRepairStructure);
}

auto MechRepairBlock::ItemsDamaged() const -> bool
{
    // Any weapon or equipment copy damaged.
    for (_LogInventoryItem* item = mech->inventory->items; item != nullptr; item = item->next)
    {
        int32_t form = componentForm(item->masterID);

        if (!isWeapon(form) && !isEquipment(form) && form != 0x12)
        {
            continue;
        }

        for (_LogInventoryStat* stat = item->stats; stat != nullptr; stat = stat->next)
        {
            if (stat->hits != 0)
            {
                return true;
            }
        }
    }

    return false;
}

auto MechRepairBlock::StructureDamaged() const -> bool
{
    // The engine, the internal structure or the armor damaged.
    return engineStat->hits != 0 || sumPoints(mech->internals, 8, false) != sumPoints(mech->internals, 8, true) ||
           sumPoints(mech->armor, 11, false) != sumPoints(mech->armor, 11, true);
}

auto MechRepairBlock::ShowsInventory() const -> bool
{
    // In multiplayer, only the player's own mechs (and the one in the briefing box) list their weapons.
    return MPlayer == nullptr || globalLogPtr->forceMechList->getMechIndex(mech) >= 0 ||
           mech->briefingBox == globalLogPtr->briefingScreen->briefingBox;
}

auto MechRepairBlock::PaintButtons(lPort* port, int32_t top, bool onRows, int32_t items, int32_t structure) -> void
{
    // Drawn in place: the original painted a scratch picture (wiped to 0xff) and copied it here keyed on 0xff.
    lBlockPort work(port->frame(), 0, top, width(), height(), true);

    if (items == 0)
    {
        globalLogPtr->repairPorts[4]->copyTo(work.frame(), onRows ? 0xea : 0xf8, 3, -1);
    }
    else
    {
        globalLogPtr->repairPorts[1]->copyTo(work.frame(), 0xea, 3, -1);
    }

    if (structure == 0)
    {
        globalLogPtr->repairPorts[5]->copyTo(work.frame(), onRows ? 0xea : 0xf8, 0x17, -1);
    }
    else
    {
        globalLogPtr->repairPorts[3]->copyTo(work.frame(), 0xea, 0x17, -1);
    }
}

auto MechRepairBlock::drawDamageDiagram(lPort* port) -> void
{
    if (port != nullptr)
    {
        PaintDiagram(port, 0, 0x8e);
    }
}

auto MechRepairBlock::PaintDiagram(lPort* port, int32_t top, int32_t xPos) -> void
{
    lBlockPort blank(port->frame(), xPos, top + 8, 0x4b, 100, true);
    VFX_pane_wipe(blank.frame(), 0x10);

    // The internal structure (shape frames 11..18), then the armor over it (0..7), coloured by damage state.
    int32_t state[8];

    for (int32_t location = 0; location < 8; ++location)
    {
        state[location] =
            damageState(percentLeft(mech->internals[location].curArmor, mech->internals[location].maxArmor));
    }

    for (int32_t shade = 0; shade < 5; ++shade)
    {
        VFX_shape_lookaside(internalLookaside(shade));

        for (int32_t location = 0; location < 8; ++location)
        {
            if (state[location] == shade)
            {
                VFX_shape_translate_draw(port->frame(), globalLogPtr->mechRepShapes[mech->nameIndex], location + 0xb,
                                         xPos, top + 8);
            }
        }
    }

    for (int32_t location = 0; location < 8; ++location)
    {
        state[location] = damageState(percentLeft(mech->armor[location].curArmor, mech->armor[location].maxArmor));
    }

    for (int32_t shade = 0; shade < 5; ++shade)
    {
        VFX_shape_lookaside(armorLookaside(shade));

        for (int32_t location = 0; location < 8; ++location)
        {
            if (state[location] == shade)
            {
                VFX_shape_translate_draw(port->frame(), globalLogPtr->mechRepShapes[mech->nameIndex], location, xPos,
                                         top + 8);
            }
        }
    }
}

auto MechRepairBlock::drawBR(lPort* port) -> void
{
    mech->calcBR();

    if (port != nullptr)
    {
        PaintBR(port, 0);
    }
}

auto MechRepairBlock::PaintBR(lPort* port, int32_t top) -> void
{
    // Drawn in place: the original painted a scratch picture (wiped to 0xff) and copied it here keyed on 0xff.
    lBlockPort work(port->frame(), 0, top, width(), height(), true);

    // The battle rating bar: 80 pixels at 18010, rising from y 0x58; the pilot modifier adds to it (or eats into it).
    int32_t rating = mech->battleRating;
    int32_t clamped = static_cast<int32_t>(rating > 18010.0 ? 18010.0 : static_cast<double>(rating));
    int32_t bar = static_cast<int32_t>(static_cast<double>(clamped) * (1.0 / 18010.0) * 80.0);

    if (lPort* art = logArtf("%slogart\\lsrupm09.tga", artPath))
    {
        art->copyTo(work.frame(), 0x72, 5, 0);
    }

    PANE* pane = work.frame();
    int32_t barTop = 0x57 - bar;
    drawLine(pane, 0x74, 0x58, 0x78, 0x58, 0xe5);
    drawLine(pane, 0x74, barTop, 0x78, barTop, 0xe3);
    int32_t top0 = 0x58 - bar;
    drawLine(pane, 0x73, 0x57, 0x73, top0, 0xe3);
    drawLine(pane, 0x79, 0x57, 0x79, top0, 0xe5);

    for (int32_t x = 0x74; x <= 0x78; ++x)
    {
        drawLine(pane, x, 0x57, x, top0, 0xe4);
    }

    drawLine(pane, 0x74, 0x56 - bar, 0x78, 0x56 - bar, 0x10);
    VFX_pixel_write(pane, 0x73, barTop, 0x10);
    VFX_pixel_write(pane, 0x79, barTop, 0x10);

    int32_t modifier = mech->pilotModifier;

    if (modifier > 0)
    {
        // A good pilot: a red extension on top (up to the 18010 mark, and at least 7 pixels down from the top).
        double extra = clamped + modifier <= 18010.0 ? static_cast<double>(modifier) : 18010.0 - clamped;
        int32_t added = static_cast<int32_t>(static_cast<double>(static_cast<int32_t>(extra)) * (1.0 / 18010.0) * 80.0);
        int32_t extensionTop = 0x58 - added - bar;

        if (extensionTop < 7)
        {
            extensionTop = 0x52 - bar;
        }

        if (extensionTop != 0)
        {
            drawLine(pane, 0x73, barTop, 0x73, extensionTop, 0xec);

            for (int32_t x = 0x74; x <= 0x78; ++x)
            {
                drawLine(pane, x, barTop, x, extensionTop, 0xc);
            }

            drawLine(pane, 0x79, barTop, 0x79, extensionTop, 0x54);
            drawLine(pane, 0x74, extensionTop - 1, 0x78, extensionTop - 1, 0xec);
        }
    }
    else if (modifier < 0)
    {
        // A poor pilot: the top of the bar turns dark.
        int32_t taken = ~static_cast<int32_t>(static_cast<double>(modifier) * (1.0 / 18010.0) * 80.0);

        if (taken > bar)
        {
            taken = bar;
        }

        if (taken != 0)
        {
            drawLine(pane, 0x74, barTop, 0x78, barTop, 0xae);
            int32_t bottom = taken - bar + 0x58;
            drawLine(pane, 0x73, top0, 0x73, bottom, 0xae);

            for (int32_t x = 0x74; x <= 0x78; ++x)
            {
                drawLine(pane, x, top0, x, bottom, 0xce);
            }

            drawLine(pane, 0x79, top0, 0x79, bottom, 0xed);
        }
    }
}

auto MechRepairBlock::drawArmorSlider(lPort* port) -> void
{
    drawSlider(port, 0);
}

auto MechRepairBlock::drawSlider(lPort* port, int32_t slider) -> void
{
    if (port != nullptr)
    {
        PaintSlider(port, 0, slider, true);
    }
}

auto MechRepairBlock::PaintSlider(lPort* port, int32_t top, int32_t slider, bool briefing) -> void
{
    int32_t start = 0;
    int32_t position = 0;
    int32_t yPos = 0;

    if (slider == 0)
    {
        start = armorSliderStart;
        position = armorSliderPos;
        yPos = 0x37;
    }
    else if (slider == 1)
    {
        start = internalSliderStart;
        position = internalSliderPos;
        yPos = 0x4c;
    }
    else if (slider == 2)
    {
        start = engineSliderStart;
        position = engineSliderPos;
        yPos = 0x61;
    }

    // Drawn in place: the original painted a scratch picture (wiped to 0xff) and copied it here keyed on 0xff.
    lBlockPort work(port->frame(), 0, top, width(), height(), true);

    if (lPort* art = logArtf("%slogart\\lsrupm%d.tga", artPath, slider + 0xb))
    {
        art->copyTo(work.frame(), briefing ? 0xf8 : 0xea, yPos, -1);
    }

    // The track left of the knob: the part repaired before (grey), then the part repaired by this drag (red).
    int32_t shift = briefing ? 0xe : 0;
    PANE* pane = work.frame();

    if (0xec < position)
    {
        int32_t left = shift + 0xec;
        int32_t startX = start + shift;

        if (left < startX - 1)
        {
            drawLine(pane, left, yPos + 1, startX - 1, yPos + 1, 0xe3);

            for (int32_t y = yPos + 2; y <= yPos + 6; ++y)
            {
                drawLine(pane, left, y, startX - 1, y, 0xe4);
            }

            drawLine(pane, left, yPos + 7, startX - 1, yPos + 7, 0xe5);
            drawLine(pane, shift + 0xeb, yPos + 2, shift + 0xeb, yPos + 6, 0xe3);
        }

        if (start < position)
        {
            int32_t right = shift - 1 + position;
            int32_t from = startX < 0xec ? 0xec : startX;
            drawLine(pane, from, yPos + 1, right, yPos + 1, 0xec);

            for (int32_t y = yPos + 2; y <= yPos + 6; ++y)
            {
                drawLine(pane, from, y, right, y, 0xc);
            }

            drawLine(pane, from, yPos + 7, right, yPos + 7, 0x87);
            drawLine(pane, from - 1, yPos + 2, from - 1, yPos + 6, 0xec);
        }
    }

    VFX_pane_copy(sliderArtPort->frame(), 0, 0, pane, shift + position, yPos, -1);
}

auto MechRepairBlock::drawInternalSlider(lPort* port) -> void
{
    drawSlider(port, 1);
}

auto MechRepairBlock::drawEngineSlider(lPort* port) -> void
{
    drawSlider(port, 2);
}

auto MechRepairBlock::draw() -> void
{
    if (repairScreen()->selectedMech == mech)
    {
        drawInventory(nullptr);
    }
}

auto MechRepairBlock::drawInventory(lPort* port) -> void
{
    if (port != nullptr && mech->assigned != 0)
    {
        PaintInventory(port, 0);
    }
}

auto MechRepairBlock::PaintInventory(lPort* port, int32_t top) -> void
{
    lBlockPort work(port->frame(), 0x135, top + 0x11, 0x62, 0x58, false);
    auto* pane = static_cast<ScrollPane*>(child(0));
    pane->DrawContentTo(work.frame(), 0, 0);
    pane->DrawSliderColumn(port->frame(), pane->width() + 0x128, top + 0x11, false);
}

auto MechRepairBlock::DrawRow(lPort* port, int32_t top) -> void
{
    // Put together as the original painted it into the rows' picture (over its colour 0xff): framed when selected,
    // darkened when another mech is.
    lBlockPort row(port->frame(), 0, top, 0x19d, 0x70, false);
    VFX_pane_wipe(row.frame(), 0xff);
    const bool selected = repairScreen()->selectedMech == mech;
    PaintBase(&row, 0, false, selected);
    LogMech* logMech = mech;
    const float status = logMech->calcStatus();

    if (logMech->pilotIndex >= 0 || logMech->networkPilot != nullptr)
    {
        PaintPilot(&row, 0, status, true);
    }
    else
    {
        // OB-134 (fixed): without a pilot the original showed the status bar only once a slider or repair had
        // painted it.
        PaintStatusBar(&row, 0, status, true);
    }

    PaintButtons(&row, 0, true, ItemsDamaged() ? 1 : 0, StructureDamaged() ? 1 : 0);
    PaintDiagram(&row, 0, 0x88);
    PaintBR(&row, 0);
    PaintSlider(&row, 0, 0, false);
    PaintSlider(&row, 0, 1, false);
    PaintSlider(&row, 0, 2, false);

    if (ShowsInventory())
    {
        PaintTonnage(&row, 0);
    }

    if (logMech->assigned != 0)
    {
        PaintInventory(&row, 0);
    }

    if (!selected)
    {
        globalLogPtr->darken(0, g_logistic_fadetable, &row);
    }

    if (pilotLifted)
    {
        if (lPort* blank = logArtf("%slogart\\lsrupm10.tga", artPath))
        {
            blank->copyTo(port->frame(), 6, top + 0x21, -1);
        }
    }

    // A repair button held while its repair runs.
    if (pressedButton != 0)
    {
        const bool items = pressedButton == 1;
        globalLogPtr->repairPorts[items ? 0 : 2]->copyTo(port->frame(), 0xea, top + (items ? 3 : 0x17), -1);
    }
}

auto MechRepairBlock::OnBeginDragPilot(lPort* surface) -> void
{
    VFX_pane_wipe(surface->frame(), 0xff);
    DragIcon::DrawFrom(surface, 5, 0x25, [this](lPort* port) { DrawRow(port, 0); });
}

auto MechRepairBlock::OnBeginDragMech(lPort* surface) -> void
{
    VFX_pane_wipe(surface->frame(), 0x10);

    for (int32_t location = 0; location < 8; ++location)
    {
        globalLogPtr->drawMechBodyLoc(mech, location, surface, 2, 1);
    }
}

auto MechRepairBlock::OnBeginDragItem(lPort* surface, _LogInventoryItem* item) -> void
{
    if (lPort* art = logArtf("%slogart\\lscicc%02d.tga", artPath, item->rangeIndex))
    {
        art->copyTo(surface->frame(), 1, 1, -1);
    }
}

auto MechRepairBlock::MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) -> bool
{
    auto* pane = static_cast<ScrollPane*>(child(0));

    if (!overPane(pane, xPos, yPos) || !pane->MouseWheel(steps, xPos, yPos))
    {
        return false;
    }

    // As a click on the list's scroll bar.
    drawInventory(nullptr);
    return true;
}

auto LogMech::calcStatus() -> float
{
    statusValue = 0.0f;

    if (pilotIndex < 0 && networkPilot == nullptr)
    {
        return 0.0f;
    }

    _LogInventoryItem* item = inventory->items;
    float pilotFactor = 0.0f;
    LogWarrior* warrior = nullptr;

    if (localPart == 0)
    {
        warrior = networkPilot;
    }
    else
    {
        globalLogPtr->assignedWarriorList->getWarriorInfo(pilotIndex, warrior);
    }

    if (warrior != nullptr)
    {
        pilotFactor = static_cast<float>(static_cast<double>(warrior->skills[3]) * 0.02);
    }

    // The firepower left: the undamaged weapons' worth over all weapons' worth (0/0 without a pilot).
    double working = 0.0;
    double total = 0.0;

    for (; item != nullptr; item = item->next)
    {
        const MasterComponent& component = MasterComponentList[item->masterID];

        if (!isWeapon(component.form) || item->stats == nullptr)
        {
            continue;
        }

        int16_t value = weaponValue(component);

        for (_LogInventoryStat* stat = item->stats; stat != nullptr; stat = stat->next)
        {
            if (stat->hits == 0)
            {
                working += static_cast<double>(value) * pilotFactor;
            }

            total += static_cast<double>(value) * pilotFactor;
        }
    }

    auto firepower = static_cast<float>(working / total);

    if (std::isnan(firepower) || firepower == 0.0f)
    {
        return statusValue;
    }

    // The body: the head's armor, the weaker of two torso locations, the legs' and the side torsos' armor.
    auto head = static_cast<float>(static_cast<double>(armor[0].curArmor) / armor[0].maxArmor * 0.6 + 0.4);
    double weakCurrent = armor[1].curArmor;
    double weakMaximum = armor[1].maxArmor;

    if (static_cast<double>(armor[8].curArmor) < weakCurrent)
    {
        weakCurrent = armor[8].curArmor;
        weakMaximum = armor[8].maxArmor;
    }

    auto legs = static_cast<float>(static_cast<double>(armor[5].curArmor + armor[4].curArmor) /
                                       (armor[5].maxArmor + armor[4].maxArmor) * 0.25 +
                                   0.75);
    double sides = static_cast<double>(armor[9].curArmor + armor[10].curArmor + armor[3].curArmor + armor[2].curArmor) /
                       (armor[10].maxArmor + armor[9].maxArmor + armor[3].maxArmor + armor[2].maxArmor) * 0.25 +
                   0.75;
    double body = sides * ((weakCurrent / weakMaximum + 1.0) * 0.5) * legs * legs * head;

    if (std::isnan(body) || body == 0.0)
    {
        return statusValue;
    }

    // The pilot's wounds.
    static constexpr float WoundFactor[7] = {1.0f, 0.95f, 0.85f, 0.75f, 0.5f, 0.3f, 0.0f};
    float woundFactor = head;

    if (warrior != nullptr)
    {
        auto wounds = static_cast<int32_t>(warrior->wounds);
        // Port fix: wounds past the table read 0 (the original read the stack beyond it).
        woundFactor = wounds >= 0 && wounds < 7 ? WoundFactor[wounds] : 0.0f;
    }

    statusValue = static_cast<float>(static_cast<double>(woundFactor) * body * firepower);
    return statusValue;
}

auto MechRepairBlock::drawStatusBar(lPort* port) -> void
{
    float status = mech->calcStatus();
    bool repairLayout = globalLogPtr->currentScreen != globalLogPtr->briefingScreen;

    // The rows draw their status bar each frame (DrawRow).
    if (port == nullptr)
    {
        return;
    }

    PaintStatusBar(port, 0, status, repairLayout);
}

auto MechRepairBlock::PaintStatusBar(lPort* port, int32_t top, float status, bool repairLayout) -> void
{
    // Drawn in place: the original painted a scratch picture (wiped to 0xff) and copied it here keyed on 0xff.
    lBlockPort work(port->frame(), 0, top, width(), height(), true);
    uint8_t color;

    if (status < 0.5f)
    {
        color = status > 0.2 ? 0xf2 : 0xef;
    }
    else
    {
        color = 0xb;
    }

    int32_t left = repairLayout ? 0x88 : 0x8e;
    int32_t right = repairLayout ? 0xd2 : 0xd8;

    for (int32_t y = 2; y <= 5; ++y)
    {
        drawLine(work.frame(), left, y, right, y, 0x13);
    }

    if (status != 0.0f)
    {
        auto end = static_cast<int32_t>(static_cast<double>(status) * 74.0 + (repairLayout ? 136.0 : 142.0));

        for (int32_t y = 2; y <= 5; ++y)
        {
            drawLine(work.frame(), left, y, end, y, color);
        }
    }
}

auto MechRepairBlock::setEngineSlider(int32_t value) -> void
{
    if (value >= 0)
    {
        return;
    }

    _LogInventoryItem* item = mech->inventory->getItemInfo(0);

    while (componentForm(item->masterID) != 4)
    {
        item = item->next;
    }

    _LogInventoryStat* engine = item->stats;

    if (engine->hits > 3)
    {
        engine->hits = 3;
    }

    switch (engine->hits)
    {
        case 0:
            engineSliderPos = 0x127;
            break;
        case 1:
            engineSliderPos = 0x113;
            break;
        case 2:
            engineSliderPos = 0xff;
            break;
        case 3:
            engineSliderPos = 0xeb;
            break;
        default:
            break;
    }
}

auto MechRepairBlock::setInternalSlider(int32_t value) -> void
{
    if (value >= 0)
    {
        return;
    }

    int32_t current = sumPoints(mech->internals, 8, false);
    int32_t maximum = sumPoints(mech->internals, 8, true);
    internalSliderPos =
        0xea - static_cast<int32_t>(static_cast<double>(current) / maximum * static_cast<double>(-61.0f));
}

auto MechRepairBlock::setArmorSlider(int32_t value) -> void
{
    if (value >= 0)
    {
        return;
    }

    int32_t current = sumPoints(mech->armor, 11, false);
    int32_t maximum = sumPoints(mech->armor, 11, true);
    armorSliderPos = 0xea - static_cast<int32_t>(static_cast<double>(current) / maximum * static_cast<double>(-61.0f));
}

auto MechRepairBlock::clearPilot() -> void
{
    pilotLifted = true;
}

auto MechRepairBlock::setPilotStats(lPort* port) -> void
{
    LogMech* logMech = mech;

    if (logMech->pilotIndex < 0 && logMech->networkPilot == nullptr)
    {
        return;
    }

    float status = mech->calcStatus();
    bool repairLayout = globalLogPtr->currentScreen != globalLogPtr->briefingScreen;

    // The rows draw their pilot each frame (DrawRow).
    if (port == nullptr)
    {
        return;
    }

    PaintPilot(port, 0, status, repairLayout);
}

auto MechRepairBlock::PaintPilot(lPort* port, int32_t top, float status, bool repairLayout) -> void
{
    LogMech* logMech = mech;
    LogWarrior* warrior = nullptr;

    if (logMech->localPart == 0)
    {
        warrior = logMech->networkPilot;
    }
    else
    {
        globalLogPtr->assignedWarriorList->getWarriorInfo(logMech->pilotIndex, warrior);
    }

    // The status bar, then the portrait.
    PaintStatusBar(port, top, status, repairLayout);
    lPort* portrait = warrior == nullptr ? logArtf("%slogart\\pilot%02d.tga", artPath, logMech->pilotIndex)
                                         : logArtf("%slogart\\%s", artPath, warrior->picture);

    if (portrait != nullptr)
    {
        portrait->copyTo(port->frame(), 6, top + 0x26, -1);
    }

    // Port fix: without a pilot record the texts are skipped (the original read them through the null pointer).
    if (warrior == nullptr)
    {
        return;
    }

    char text[256];
    writeText(yellowDropFont, port->frame(), 0x2d, top + 0x2a, warrior->callsign);

    // An out-of-range rank shows the portrait's file name, which the text buffer last held.
    std::snprintf(text, sizeof(text), "%slogart\\%s", artPath, warrior->picture);

    if (warrior->rank >= 0 && warrior->rank <= 3)
    {
        cLoadString(thisInstance, 0x70 + static_cast<uint32_t>(warrior->rank), text, 0xfe);
    }

    writeText(yellowDropFont, port->frame(), 0x2d, top + 0x3c, text);
    globalLogPtr->drawPilotSkillBar(warrior, 3, 0x2e, top + 0x4a, 0, 0x36, winHeight, port);
    globalLogPtr->drawPilotSkillBar(warrior, 0, 0x2e, top + 0x53, 0, 0x36, winHeight, port);
    globalLogPtr->drawPilotSkillBar(warrior, 1, 0x2e, top + 0x5c, 0, 0x36, winHeight, port);
    globalLogPtr->drawPilotSkillBar(warrior, 2, 0x2e, top + 0x65, 0, 0x36, winHeight, port);

    // One 2x2 pip per point of health.
    auto pips = static_cast<int32_t>(warrior->health);
    int32_t xPos = 0xc;

    for (; pips > 0; --pips)
    {
        AG_pixel_write(port->frame(), xPos, top + 0x22, 0xcf);
        AG_pixel_write(port->frame(), xPos + 1, top + 0x22, 0xcf);
        AG_pixel_write(port->frame(), xPos + 1, top + 0x23, 0xee);
        AG_pixel_write(port->frame(), xPos, top + 0x23, 0xcf);
        xPos += 3;
    }
}

auto Logistics::drawPilotSkillBar(LogWarrior* warrior, int32_t skill, int32_t xPos, int32_t yPos, int32_t row,
                                  int32_t width, int32_t rowHeight, lPort* port) -> void
{
    drawPilotSkillBar(warrior->skills[skill], xPos, yPos, row, width, rowHeight, port);
}

auto Logistics::drawPilotSkillBar(int32_t value, int32_t xPos, int32_t yPos, int32_t row, int32_t width,
                                  int32_t rowHeight, lPort* port) -> void
{
    PANE* pane = port->frame();
    int32_t top = row * rowHeight + yPos;
    int32_t right = xPos + width;
    // The empty bar.
    drawLine(pane, xPos, top, right - 1, top, 0x32);
    drawLine(pane, xPos, top + 1, xPos, top + 2, 0x32);
    drawLine(pane, xPos + 1, top + 3, right - 1, top + 3, 0x14);
    drawLine(pane, right, top + 1, right, top + 2, 0x14);
    drawLine(pane, xPos + 1, top + 1, right - 1, top + 1, 0x33);
    drawLine(pane, xPos + 1, top + 2, right - 1, top + 2, 0x33);

    if (value == 0)
    {
        return;
    }

    // The filled part.
    double scale =
        static_cast<double>(width) / (static_cast<double>(MaxPilotSkill) - static_cast<double>(MinPilotSkill));
    int32_t end =
        static_cast<int32_t>((static_cast<double>(value) - static_cast<double>(MinPilotSkill)) * scale) + xPos;
    drawLine(pane, xPos + 1, top, end - 1, top, 0xe3);
    drawLine(pane, xPos, top + 1, xPos, top + 2, 0xe3);
    drawLine(pane, xPos + 1, top + 3, end - 1, top + 3, 0xe5);
    drawLine(pane, end, top + 1, end, top + 2, 0xe5);
    drawLine(pane, xPos + 1, top + 1, end - 1, top + 1, 0xe4);
    drawLine(pane, xPos + 1, top + 2, end - 1, top + 2, 0xe4);
    drawLine(pane, end + 1, top + 1, end + 1, top + 2, 0x10);
    VFX_pixel_write(pane, end, top, 0x10);
    VFX_pixel_write(pane, end, top + 3, 0x10);
}

auto MechRepairBlock::setPilotHealth(lPort*) -> void
{
    // Drawn with the pilot's stats (PaintPilot).
}

auto MechRepairBlock::setMechStats() -> void
{
}

auto MechRepairBlock::setWeaponLists() -> void
{
    if (shortRangeWeapons != nullptr)
    {
        logFree(shortRangeWeapons);
        numShortRangeWeapons = 0;
        shortRangeWeapons = nullptr;
    }

    if (mediumRangeWeapons != nullptr)
    {
        logFree(mediumRangeWeapons);
        numMediumRangeWeapons = 0;
        mediumRangeWeapons = nullptr;
    }

    if (longRangeWeapons != nullptr)
    {
        logFree(longRangeWeapons);
        numLongRangeWeapons = 0;
        longRangeWeapons = nullptr;
    }

    if (equipment != nullptr)
    {
        logFree(equipment);
        numEquipment = 0;
        equipment = nullptr;
    }

    if (itemHits != nullptr)
    {
        logFree(itemHits);
        numItems = 0;
        itemHits = nullptr;
    }

    // Weapons go by long range: under 76 short, under 151 medium, else long.
    _LogInventoryItem* items = mech->inventory->items;

    for (_LogInventoryItem* item = items; item != nullptr; item = item->next)
    {
        const MasterComponent& component = MasterComponentList[item->masterID];

        if (isWeapon(component.form))
        {
            if (component.weaponRange[3] < 76.0f)
            {
                numShortRangeWeapons += item->count;
            }
            else if (component.weaponRange[3] < 151.0f)
            {
                numMediumRangeWeapons += item->count;
            }
            else
            {
                numLongRangeWeapons += item->count;
            }
        }
        else if (isEquipment(component.form))
        {
            numEquipment += item->count;
        }
    }

    if (numShortRangeWeapons != 0)
    {
        shortRangeWeapons = static_cast<int32_t*>(logAlloc(numShortRangeWeapons * 4));
    }

    if (numMediumRangeWeapons != 0)
    {
        mediumRangeWeapons = static_cast<int32_t*>(logAlloc(numMediumRangeWeapons * 4));
    }

    if (numLongRangeWeapons != 0)
    {
        longRangeWeapons = static_cast<int32_t*>(logAlloc(numLongRangeWeapons * 4));
    }

    if (numEquipment != 0)
    {
        equipment = static_cast<int32_t*>(logAlloc(numEquipment * 4));
    }

    numItems = numLongRangeWeapons + numMediumRangeWeapons + numShortRangeWeapons + numEquipment;

    if (numItems != 0)
    {
        itemHits = static_cast<int32_t*>(logAlloc(numItems * 4));
    }

    // One entry per copy: the item's index in the inventory, and the copy's damage in itemHits.
    int32_t shortCount = 0;
    int32_t mediumCount = 0;
    int32_t longCount = 0;
    int32_t equipmentCount = 0;
    int32_t index = 0;

    for (_LogInventoryItem* item = items; item != nullptr; item = item->next, ++index)
    {
        const MasterComponent& component = MasterComponentList[item->masterID];
        _LogInventoryStat* stat = item->stats;

        if (isWeapon(component.form))
        {
            if (component.weaponRange[3] < 76.0f)
            {
                for (int32_t copy = 0; copy < item->count; ++copy)
                {
                    shortRangeWeapons[shortCount] = index;
                    itemHits[shortCount] = stat->hits;
                    ++shortCount;
                    stat = stat->next;
                }
            }
            else if (component.weaponRange[3] < 151.0f)
            {
                for (int32_t copy = 0; copy < item->count; ++copy)
                {
                    mediumRangeWeapons[mediumCount] = index;
                    itemHits[numShortRangeWeapons + mediumCount] = stat->hits;
                    ++mediumCount;
                    stat = stat->next;
                }
            }
            else
            {
                for (int32_t copy = 0; copy < item->count; ++copy)
                {
                    longRangeWeapons[longCount] = index;
                    itemHits[numShortRangeWeapons + numMediumRangeWeapons + longCount] = stat->hits;
                    ++longCount;
                    stat = stat->next;
                }
            }
        }
        else if (isEquipment(component.form))
        {
            for (int32_t copy = 0; copy < item->count; ++copy)
            {
                equipment[equipmentCount] = index;
                itemHits[numLongRangeWeapons + numShortRangeWeapons + numMediumRangeWeapons + equipmentCount] =
                    stat->hits;
                ++equipmentCount;
                stat = stat->next;
            }
        }
    }

    sortWeaponList(shortRangeWeapons, numShortRangeWeapons);
    sortWeaponList(mediumRangeWeapons, numMediumRangeWeapons);
    sortWeaponList(longRangeWeapons, numLongRangeWeapons);
}

auto MechRepairBlock::sortWeaponList(int32_t* list, int32_t count) -> void
{
    int32_t base = 0;

    if (list == mediumRangeWeapons)
    {
        base = numShortRangeWeapons;
    }
    else if (list == longRangeWeapons)
    {
        base = numMediumRangeWeapons + numShortRangeWeapons;
    }

    // A selection sort by damage, lowest first; the hits follow their entries.
    InventoryList* inventory = mech->inventory;

    for (int32_t i = 0; i < count - 1; ++i)
    {
        for (int32_t j = i + 1; j < count; ++j)
        {
            int32_t first = list[i];
            float firstDamage = MasterComponentList[inventory->getMasterIDFromIndex(first)].damage;
            int32_t second = list[j];

            if (MasterComponentList[inventory->getMasterIDFromIndex(second)].damage < firstDamage)
            {
                list[i] = second;
                list[j] = first;
                std::swap(itemHits[base + i], itemHits[base + j]);
            }
        }
    }
}

auto MechRepairBlock::getInvItem(int32_t* list, int32_t index, uint8_t* itemNum) -> _LogInventoryItem*
{
    int32_t earlier = index - 1;

    while (earlier >= 0 && list[earlier] == list[index])
    {
        --earlier;
    }

    int32_t copy = index - earlier - 1;
    _LogInventoryItem* item = mech->inventory->getItemInfo(list[index]);
    _LogInventoryStat* stat = item->stats;

    for (; copy > 0; --copy)
    {
        stat = stat->next;
    }

    *itemNum = static_cast<uint8_t>(stat->itemNum);
    return item;
}

auto MechRepairBlock::repairArmor(int32_t sliderPos) -> void
{
    LogMech* logMech = mech;

    if (sliderPos < 0)
    {
        for (auto& location : logMech->armor)
        {
            location.curArmor = location.maxArmor;
        }

        return;
    }

    // The head first, then point by point to the most damaged location (the cockpit, center torso and rear
    // locations count as more damaged than they are).
    int32_t headMissing = logMech->armor[0].maxArmor - logMech->armor[0].curArmor;

    if (headMissing != 0)
    {
        logMech->armor[0].curArmor += static_cast<uint8_t>(sliderPos < headMissing ? sliderPos : headMissing);
        sliderPos -= headMissing;
    }

    if (sliderPos <= 0)
    {
        return;
    }

    auto weighted = [&](int32_t location) -> float
    {
        auto ratio = static_cast<float>(static_cast<double>(logMech->armor[location].curArmor) /
                                        logMech->armor[location].maxArmor);
        return ratio;
    };

    auto discount = [](float ratio, double factor) -> float
    { return static_cast<float>(static_cast<double>(ratio) - static_cast<double>(ratio) * factor); };
    float ratios[11] = {};

    for (int32_t location = 2; location <= 7; ++location)
    {
        ratios[location] = weighted(location);
    }

    ratios[1] = weighted(1);

    if (ratios[1] < 1.0f)
    {
        ratios[1] = discount(ratios[1], 0.2);
    }

    ratios[8] = weighted(8);

    if (ratios[8] < 1.0f)
    {
        ratios[8] = discount(ratios[8], 0.4);
    }

    ratios[9] = weighted(9);

    if (ratios[9] < 1.0f)
    {
        ratios[9] = discount(ratios[9], 0.3);
    }

    ratios[10] = weighted(10);

    if (ratios[10] < 1.0f)
    {
        ratios[10] = discount(ratios[10], 0.3);
    }

    int32_t chosen = 1;

    for (; sliderPos != 0; --sliderPos)
    {
        for (int32_t location = 1; location <= 10; ++location)
        {
            if (ratios[location] < ratios[chosen])
            {
                chosen = location;
            }
        }

        uint8_t current = ++logMech->armor[chosen].curArmor;
        uint8_t maximum = logMech->armor[chosen].maxArmor;

        if (current < maximum)
        {
            auto ratio = static_cast<float>(static_cast<double>(current) / maximum);
            ratios[chosen] = ratio;

            if (chosen == 1)
            {
                ratios[chosen] = discount(ratio, 0.2);
            }
            else if (chosen == 8)
            {
                ratios[chosen] = discount(ratio, 0.4);
            }
            else if (chosen == 9 || chosen == 10)
            {
                ratios[chosen] = discount(ratio, 0.3);
            }
        }
        else
        {
            ratios[chosen] = 1.0f;
        }
    }
}

auto MechRepairBlock::repairInternal(int32_t sliderPos) -> void
{
    LogMech* logMech = mech;

    if (sliderPos < 0)
    {
        for (auto& location : logMech->internals)
        {
            location.curArmor = location.maxArmor;
        }

        return;
    }

    // Point by point to the most damaged location (weighted like the armor).
    static constexpr double Discount[8] = {0.3, 0.5, 0.2, 0.2, 0.0, 0.0, 0.4, 0.4};
    auto discount = [](float ratio, double factor) -> float
    { return static_cast<float>(static_cast<double>(ratio) - static_cast<double>(ratio) * factor); };
    float ratios[8];

    for (int32_t location = 0; location < 8; ++location)
    {
        ratios[location] = static_cast<float>(static_cast<double>(logMech->internals[location].curArmor) /
                                              logMech->internals[location].maxArmor);

        if (Discount[location] != 0.0 && ratios[location] < 1.0f)
        {
            ratios[location] = discount(ratios[location], Discount[location]);
        }
    }

    int32_t chosen = 1;

    for (; sliderPos > 0; --sliderPos)
    {
        for (int32_t location = 0; location < 8; ++location)
        {
            if (ratios[location] < ratios[chosen])
            {
                chosen = location;
            }
        }

        uint8_t current = ++logMech->internals[chosen].curArmor;
        uint8_t maximum = logMech->internals[chosen].maxArmor;

        if (current < maximum)
        {
            auto ratio = static_cast<float>(static_cast<double>(current) / maximum);
            ratios[chosen] = Discount[chosen] != 0.0 ? discount(ratio, Discount[chosen]) : ratio;
        }
        else
        {
            ratios[chosen] = 1.0f;
        }
    }
}

auto MechRepairBlock::setInventory(ScrollPane* pane) -> void
{
    if (MPlayer != nullptr)
    {
        LogMech* logMech = mech;

        if (globalLogPtr->forceMechList->getMechIndex(logMech) < 0 &&
            logMech->briefingBox != globalLogPtr->briefingScreen->briefingBox)
        {
            return;
        }
    }

    if (pane == nullptr)
    {
        pane = inventoryPane;
    }

    auto* content = new lPort;
    int32_t lines = 0;

    for (_LogInventoryItem* item = mech->inventory->items; item != nullptr; item = item->next)
    {
        int32_t form = componentForm(item->masterID);

        if (isWeapon(form) || isEquipment(form))
        {
            lines += item->count;
        }
    }

    int32_t lineHeight = greenFont->height() + 2;
    int32_t contentHeight = lineHeight * (lines + 4) + 4;

    if (contentHeight < pane->height())
    {
        contentHeight = pane->height();
    }

    // Port: the list is drawn into the pane each frame (DrawWeaponList) from the lists made here.
    content->initView(pane->width() - 0xd, contentHeight);
    setWeaponLists();
    content->DrawContent = [this](aPort* view) { DrawWeaponList(static_cast<lPort*>(view)); };
    pane->setDisplayPort(content, -1, 0);

    // The tonnage bar (the weapons' weight against the free weight): the rows draw theirs each frame; the original
    // also painted one into the briefing screen's picture for the box, which the box (drawn after) covered.
}

auto MechRepairBlock::PaintTonnage(lPort* port, int32_t top) -> void
{
    // Drawn in place: the original painted a scratch picture (wiped to 0xff) and copied it here keyed on 0xff.
    lBlockPort work(port->frame(), 0, top, width(), height(), true);
    int32_t fill = static_cast<int32_t>(static_cast<double>(mech->weaponTonnage) / mech->freeTonnage * 53.0);
    drawTonnageBar(work.frame(), 0x15e, fill);
}

auto MechRepairBlock::DrawWeaponList(lPort* content) -> void
{
    PANE* frame = content->frame();
    int32_t lineHeight = greenFont->height() + 2;
    VFX_pane_wipe(frame, 0x10);

    // The four headings: a coloured stripe (the first three with a black line under it) and the title.
    auto heading = [&](int32_t top, uint8_t color, bool underline)
    {
        int32_t y = top;

        for (; y < greenFont->height() + 2 + top; ++y)
        {
            drawLine(frame, 0, y, content->width() - 2, y, color);
        }

        if (underline)
        {
            drawLine(frame, 0, y, content->width() - 2, y, 0x10);
        }
    };

    char text[256];
    heading(0, 0xe, true);
    cLoadString(thisInstance, 0x55, text, 0xfe);
    writeText(whiteFont, frame, 1, 2, text);
    int32_t before = numShortRangeWeapons + 1;
    heading(lineHeight * before + 1, 0xe5, true);
    cLoadString(thisInstance, 0x50, text, 0xfe);
    writeText(whiteFont, frame, 1, lineHeight * before + 3, text);
    int32_t shortAndMedium = numMediumRangeWeapons + numShortRangeWeapons;
    heading(lineHeight * (shortAndMedium + 2) + 2, 0xee, true);
    cLoadString(thisInstance, 0x6d, text, 0xfe);
    writeText(whiteFont, frame, 1, lineHeight * (shortAndMedium + 2) + 4, text);
    int32_t weapons = numLongRangeWeapons + numMediumRangeWeapons + numShortRangeWeapons;
    heading(lineHeight * (weapons + 3) + 3, 0x14, false);
    cLoadString(thisInstance, 0x6f, text, 0xfe);
    writeText(whiteFont, frame, 1, lineHeight * (weapons + 3) + 5, text);

    // The entries: a range glyph (clan technology has its own) and the name, grey when damaged.
    InventoryList* inventory = mech->inventory;
    auto weaponLine = [&](int32_t entry, int32_t hitsIndex, char glyph, int32_t line, int32_t gap)
    {
        const MasterComponent& component = MasterComponentList[inventory->getMasterIDFromIndex(entry)];
        char name[64];
        std::snprintf(name, sizeof(name), "%c %s", component.techBase != 1 ? glyph + 0x5e : glyph, component.name);

        if (itemHits[hitsIndex] == 0)
        {
            writeText(blueFont, frame, 2, (greenFont->height() + 2) * line + gap, name);
        }
        else
        {
            writeText(greyFont, frame, 2, (greyFont->height() + 2) * line + gap, name);
        }
    };

    int32_t line = 1;

    for (int32_t i = 0; i < numShortRangeWeapons; ++i)
    {
        weaponLine(shortRangeWeapons[i], i, 0x1d, line++, 2);
    }

    ++line;

    for (int32_t i = 0; i < numMediumRangeWeapons; ++i)
    {
        weaponLine(mediumRangeWeapons[i], numShortRangeWeapons + i, 0x1e, line++, 3);
    }

    ++line;

    for (int32_t i = 0; i < numLongRangeWeapons; ++i)
    {
        weaponLine(longRangeWeapons[i], shortAndMedium + i, 0x1f, line++, 4);
    }

    for (int32_t i = 0; i < numEquipment; ++i)
    {
        ++line;
        const MasterComponent& component = MasterComponentList[inventory->getMasterIDFromIndex(equipment[i])];
        aFont* font = itemHits[weapons + i] == 0 ? blueFont : greyFont;
        writeText(font, frame, 2, (greenFont->height() + 2) * line + 5, component.name);
    }
}

auto MechRepairBlock::getItemFromScrollPane(ScrollPane* pane, int32_t line, uint8_t* itemNum) -> _LogInventoryItem*
{
    (void)pane;
    // The lines: heading, short-range weapons, heading, medium, heading, long, heading, equipment.
    int32_t shortEnd = numShortRangeWeapons;

    if (line <= shortEnd)
    {
        if (line != 0)
        {
            return getInvItem(shortRangeWeapons, line - 1, itemNum);
        }

        return nullptr;
    }

    int32_t mediumEnd = numMediumRangeWeapons + 1 + shortEnd;

    if (line <= mediumEnd)
    {
        int32_t entry = line - 1 - shortEnd;

        if (entry != 0)
        {
            return getInvItem(mediumRangeWeapons, entry - 1, itemNum);
        }

        return nullptr;
    }

    int32_t longEnd = numLongRangeWeapons + numMediumRangeWeapons + 2 + shortEnd;

    if (line <= longEnd)
    {
        int32_t entry = line - 2 - numMediumRangeWeapons - shortEnd;

        if (entry != 0)
        {
            return getInvItem(longRangeWeapons, entry - 1, itemNum);
        }

        return nullptr;
    }

    if (line <= numEquipment + numLongRangeWeapons + numMediumRangeWeapons + 3 + shortEnd)
    {
        int32_t entry = line - 3 - numLongRangeWeapons - numMediumRangeWeapons - shortEnd;

        if (entry != 0)
        {
            return getInvItem(equipment, entry - 1, itemNum);
        }
    }

    return nullptr;
}

auto MechRepairBlock::setUpItemDragIcon(_LogInventoryItem* item, uint8_t itemNum, aEvent* event,
                                        int32_t* inventoryIndex, int32_t* hits) -> void
{
    int32_t eventX = event->x;
    int32_t eventY = event->y;
    uint8_t masterID = item->masterID;
    auto* icon = new DragIcon;
    globalLogPtr->dragIcon = icon;
    // Finds the dragged copy (the result is not used; the loop below looks again).
    _LogInventoryStat* found = item->stats;

    while (static_cast<uint32_t>(found->itemNum) != itemNum)
    {
        found = found->next;
    }

    repairScreen()->drawBlankInvInfoBlock(2);
    icon->Begin(eventX - 0x10, eventY - 0x10, 0x20, 0x20,
                [this, item](lPort* surface) { OnBeginDragItem(surface, item); });

    float tonnage = MasterComponentList[masterID].tonnage;

    if (usesAmmo(masterID))
    {
        uint8_t ammo = MasterComponentList[masterID].ammoMasterId;
        tonnage = MasterComponentList[ammo].tonnage + tonnage;
        mech->inventory->removeItem(ammo, -1);
    }

    LogMech* logMech = mech;
    InventoryList* inventory = logMech->inventory;
    int32_t index = inventory->getIndexFromMasterID(masterID);
    *inventoryIndex = index;
    _LogInventoryStat* stat = inventory->getItemInfo(index)->stats;
    logMech->usedTonnage -= tonnage;
    logMech->weaponTonnage -= tonnage;

    for (; stat != nullptr; stat = stat->next)
    {
        if (static_cast<uint32_t>(stat->itemNum) != itemNum)
        {
            continue;
        }

        *hits = stat->hits;
        if (stat->hits != 0)
        {
            globalLogPtr->darken(0, g_logistic_fadetable, globalLogPtr->dragIcon->lport());
        }

        drawItemInfo(item, logMech->inventory);
        inventory->removeItem(masterID, stat->statID);
        repairScreen()->setUpCompInv(0, 0);
        setInventory(nullptr);
        drawInventory(nullptr);
        mech->calcBR();
        drawBR(nullptr);
        drawStatusBar();
        drawButtons(nullptr);
        repairScreen()->addChild(globalLogPtr->dragIcon);
        globalLogPtr->dragIcon->ShowGUIWindow(-1);
        globalLogPtr->dragIcon->setDepth(100);
        return;
    }
}

auto MechRepairBlock::DrawInfo(lPort* port) -> void
{
    if (dragPort != nullptr)
    {
        dragPort->copyTo(port->frame(), 0xb, 0x191, -1);
    }

    char tons[32];
    cLoadString(thisInstance, 0x6e, tons, 0x1e);
    LogMech* shown = mech;
    char text[84];
    std::snprintf(text, sizeof(text), "%.0f %s", static_cast<double>(shown->curTonnage), tons);
    writeText(yellowDropFont, port->frame(), 0x53, 0x193, text);
    writeText(yellowDropFont, port->frame(), 0x53, 0x19c, shown->weightClassName);
    writeText(yellowDropFont, port->frame(), 0xa8, 0x193, shown->chassisClassName);
    writeText(yellowDropFont, port->frame(), 0xa8, 0x19c, shown->extraName1);
    writeText(yellowDropFont, port->frame(), 0xa8, 0x1a5, shown->extraName2);
    std::snprintf(text, sizeof(text), "%d m/s", shown->maxRunSpeed);
    writeText(yellowDropFont, port->frame(), 0x53, 0x1a5, text);
    DrawInfoDescription(port, 0xc3, 0x26, shown->description, 8, 0x1b3);
}

auto MechRepairBlock::DebugFunction1(int32_t arg1, int32_t arg2) -> int
{
    (void)arg1;
    (void)arg2;
    return 0;
}

VehicleRepairBlock::~VehicleRepairBlock()
{
    VehicleRepairBlock::destroy();
}

auto VehicleRepairBlock::init(LogVehicle* logVehicle) -> void
{
    vehicle = logVehicle;
    lPort* rowsPort = unitRowsPort();
    lObject::init(0, 0, 0x19a, 0x70, nullptr, rowsPort);
}

auto VehicleRepairBlock::destroy() -> void
{
    lObject::destroy();
}

auto VehicleRepairBlock::handleEvent(aEvent* event) -> void
{
    if (globalLogPtr->currentScreen == globalLogPtr->purchaseScreen)
    {
        return;
    }

    if (parent != nullptr && vehicleLeftDrag == 0 && vehicleRightHeld == 0 && (event->type == 8 || event->type == 9))
    {
        parent->handleEvent(event);
        return;
    }

    globalX();
    globalY();
    int32_t eventType = event->type;

    switch (eventType)
    {
        case 1:
        {
            if (vehicleRightHeld != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 3:
        {
            if (vehicleLeftDrag != 0)
            {
                break;
            }

            if (repairScreen()->selectedVehicle != vehicle)
            {
                repairScreen()->selectVehicle(vehicle);
                return;
            }

            int32_t eventX = event->x;
            int32_t eventY = event->y;

            if (globalX() <= eventX && eventX <= globalX() + width() && globalY() <= eventY &&
                eventY <= globalY() + height())
            {
                // Pick up the vehicle.
                playSample(0x35);
                application->showCursor(0);
                application->grab(this);
                draggingVehicle = -1;

                if (eventType == 1)
                {
                    vehicleLeftDrag = -1;
                }
                else
                {
                    vehicleRightHeld = 1;
                }

                vehicleDragY = eventY - 0xf;
                vehicleDragX = eventX - 0xf;
                auto* icon = new DragIcon;
                globalLogPtr->dragIcon = icon;
                icon->Begin(vehicleDragX, vehicleDragY, 0x1e, 0x1e, [this](lPort* surface) { OnBeginDrag(surface); });
                repairScreen()->addChild(globalLogPtr->dragIcon);
                globalLogPtr->dragIcon->ShowGUIWindow(-1);
                globalLogPtr->dragIcon->setDepth(100);
                globalLogPtr->dragIcon->moveTo(vehicleDragX, vehicleDragY, 0);
                return;
            }

            playSample(0x33);
            return;
        }

        case 4:
        {
            if (vehicleRightHeld != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 6:
        {
            if (vehicleLeftDrag != 0 && eventType == 6)
            {
                return;
            }

            vehicleRightHeld = 0;

            if (application->grabbedObject() == nullptr)
            {
                return;
            }

            application->showCursor(-1);
            playSample(0x34);
            aObject* inventory = repairScreen()->inventoryPane;

            if (draggingVehicle != 0)
            {
                // Dropped on the inventory, the vehicle leaves the force.
                draggingVehicle = 0;
                application->release();
                vehicleLeftDrag = 0;
                deleteDragIcon();

                if (eventType != 6 && !overPaneInside(inventory, event))
                {
                    return;
                }

                for (auto& lance : globalLogPtr->deploySlots)
                {
                    for (auto& slot : lance)
                    {
                        if (slot.vehicle < 0)
                        {
                            continue;
                        }

                        if (slot.vehicle == slotIndex - globalLogPtr->forceMechList->getMechCount())
                        {
                            LogVehicle* leaving = vehicle;
                            Assert(leaving != nullptr, 0, "Vehicle is NULL", nullptr);
                            MechBriefBlock* brief = leaving->briefBlock;
                            Assert(brief != nullptr, 0, "vehicleBrief is NULL", nullptr);

                            if (brief->parent != nullptr)
                            {
                                brief->parent->removeChild(brief);
                            }

                            brief->ShowGUIWindow(0);
                            slot.vehicle = -1;
                        }
                        else
                        {
                            slot.vehicle = slot.vehicle - 1;
                        }
                    }
                }

                LogVehicle* leaving = vehicle;
                leaving->deployed = 0;
                leaving->assigned = 0;

                if (leaving == repairScreen()->selectedVehicle)
                {
                    repairScreen()->selectedVehicle = nullptr;
                }

                globalLogPtr->reorderVehicles();
                VehicleRepairBlock* block = leaving->repairBlock;

                if (block->parent != nullptr)
                {
                    block->parent->removeChild(block);
                }

                repairScreen()->removeVehicleFromList(leaving);
                repairScreen()->createVhclInvBlock();
                repairScreen()->setUpVhclInv(-1, -1);
                return;
            }

            application->release();
            vehicleLeftDrag = 0;
            deleteDragIcon();

            if (eventType == 6 || overPaneInside(inventory, event))
            {
                globalLogPtr->reorderWarriors();
                repairScreen()->createPilotInvBlock();
                repairScreen()->setUpPilotInv(-1, -1);
                globalLogPtr->shiftPilots(vehiclePilotShift, -1);
                drawBackground(slotIndex, nullptr);

                // Port fix: no pilot, no speech (nothing ever sets vehiclePilot; the original read through it).
                if (vehiclePilot != nullptr)
                {
                    soundSystem->playPilotSpeech(vehiclePilot->pilotAudio, 2);
                }

                vehiclePilot = nullptr;
            }
            else if (vehiclePilot != nullptr)
            {
                LogWarrior* pilot = vehiclePilot;
                pilot->assigned = -1;
                pilot->inventoryBlock->vehicle = vehicle;
                drawBackground(slotIndex, nullptr);
                return;
            }
            break;
        }

        case 7:
        {
            if (vehicleLeftDrag != 0)
            {
                vehicleDragY = event->y - 0xf;
                vehicleDragX = event->x - 0xf;
                globalLogPtr->dragIcon->moveTo(vehicleDragX, vehicleDragY, 0);
            }

            if (event->key == 0)
            {
                char text[256];
                cLoadString(thisInstance, 0x34, text, 0xfe);
                globalLogPtr->ticker->setString(text);
                return;
            }
            break;
        }

        default:
            break;
    }
}

auto VehicleRepairBlock::drawDamageDiagram(lPort* port) -> void
{
    LogVehicle* logVehicle = vehicle;
    int32_t state[5];

    for (int32_t location = 0; location < 5; ++location)
    {
        state[location] =
            damageState(percentLeft(logVehicle->curArmorPoints[location], logVehicle->maxArmorPoints[location]));
    }

    for (int32_t shade = 0; shade < 5; ++shade)
    {
        VFX_shape_lookaside(armorLookaside(shade));

        for (int32_t location = 0; location < 5; ++location)
        {
            if (state[location] == shade)
            {
                VFX_shape_translate_draw(port->frame(), globalLogPtr->vehicleRepShapes[logVehicle->nameIndex], location,
                                         0, 0);
            }
        }
    }
}

auto VehicleRepairBlock::drawBackground(int32_t row, lPort* port) -> void
{
    if (row >= 0)
    {
        // The repair screen's rows are drawn each frame (DrawRow).
        return;
    }

    PaintRow(port, 0, true, false);
}

auto VehicleRepairBlock::DrawRow(lPort* port, int32_t top) -> void
{
    // Framed while selected (the original's frame stayed until the row was painted again).
    PaintRow(port, top, false, repairScreen()->selectedVehicle == vehicle);
}

auto VehicleRepairBlock::OnBeginDrag(lPort* surface) -> void
{
    VFX_pane_wipe(surface->frame(), 0x10);

    for (int32_t location = 0; location < 5; ++location)
    {
        globalLogPtr->drawVehicleBodyLoc(vehicle, location, surface, 2, 0);
    }
}

auto VehicleRepairBlock::PaintRow(lPort* port, int32_t top, bool briefing, bool framed) -> void
{
    lPort* rowArt = logArtf(briefing ? "%slogart\\lsbbkv00.tga" : "%slogart\\lsrupv00.tga", artPath);

    if (rowArt == nullptr)
    {
        return;
    }

    lBlockPort back(port->frame(), 0, top, rowArt->width(), rowArt->height(), true);
    VFX_pane_copy(rowArt->frame(), 0, 0, back.frame(), 0, 0, -1);

    if (lPort* art = logArtf("%slogart\\lscflv%02d.tga", artPath, vehicle->nameIndex))
    {
        art->copyTo(back.frame(), 5, 4, -1);
    }

    // The damage diagram, drawn over a copy of the vehicle's mask.
    if (lPort* maskArt = logArtf("%slogart\\vmask%02d.tga", artPath, vehicle->nameIndex))
    {
        lBlockPort mask(back.frame(), briefing ? 0x124 : 0x11d, 8, maskArt->width(), maskArt->height(), true);
        VFX_pane_copy(maskArt->frame(), 0, 0, mask.frame(), 0, 0, -1);
        drawDamageDiagram(&mask);
    }

    setBar(&back, briefing ? 0x124 : 0x11d);

    // The equipment and weapons, one "count name" line each.
    LogVehicle* logVehicle = vehicle;
    InventoryList* inventory = logVehicle->inventory;
    int32_t line = 0;
    char text[1024];

    for (int32_t index = 0; index < inventory->numItems; ++index)
    {
        int32_t form = MasterComponentList[inventory->getMasterIDFromIndex(index)].form;

        if (isEquipment(form) || isWeapon(form) || form == 6)
        {
            _LogInventoryItem* item = logVehicle->inventory->getItemInfo(index);
            std::snprintf(text, sizeof(text), "%d %s", item->count, item->name);
            writeText(greenFont, back.frame(), 0x8e, (greenFont->height() + 1) * line + 0x16, text);
            ++line;
        }

        inventory = logVehicle->inventory;
    }

    char format[256];
    char fileName[256];
    cLoadString(thisInstance, 0x53, format, 0xfe);
    std::snprintf(fileName, sizeof(fileName), format, static_cast<double>(logVehicle->curTonnage),
                  logVehicle->inventoryBlock->weightClassText);
    writeText(blueFont, back.frame(), 6, 0x11, fileName);
    std::snprintf(fileName, sizeof(fileName), "%d m/s", logVehicle->maxMoveSpeed);
    writeText(yellowDropFont, back.frame(), 6, 0x29, fileName);
    writeText(yellowDropFont, back.frame(), 6, 0x3b, logVehicle->inventoryBlock->weightClassText);

    if (!briefing)
    {
        if (framed)
        {
            // The selected vehicle's frame.
            drawLine(back.frame(), 1, 0, width(), 0, 0xf2);
            drawLine(back.frame(), 1, 0, 1, height() - 3, 0xf2);
            drawLine(back.frame(), 1, height() - 3, width(), height() - 3, 0xf2);
            drawLine(back.frame(), width(), 0, width(), height() - 3, 0xf2);
        }
        else
        {
            globalLogPtr->darken(0, g_logistic_fadetable, &back);
        }
    }
}

auto VehicleRepairBlock::setBar(lPort* port, int32_t xPos) -> void
{
    LogVehicle* logVehicle = vehicle;
    // The firepower left (1 without weapons), times each location's armor (at least 40% each).
    double working = 0.0;
    double total = 0.0;
    double firepower = 1.0;
    _LogInventoryItem* item = logVehicle->inventory->items;

    if (item != nullptr)
    {
        for (; item != nullptr; item = item->next)
        {
            const MasterComponent& component = MasterComponentList[item->masterID];

            if (!isWeapon(component.form) || item->stats == nullptr)
            {
                continue;
            }

            int16_t value = weaponValue(component);

            for (_LogInventoryStat* stat = item->stats; stat != nullptr; stat = stat->next)
            {
                if (stat->hits == 0)
                {
                    working += value;
                }

                total += value;
            }
        }

        if (total != 0.0)
        {
            firepower = working / total;
        }
    }

    auto armorFactor = [&](int32_t location)
    {
        return static_cast<double>(logVehicle->curArmorPoints[location]) / logVehicle->maxArmorPoints[location] * 0.6 +
               0.4;
    };

    double last = 1.0;

    if (static_cast<float>(logVehicle->maxArmorPoints[4]) != 0.0f)
    {
        last = armorFactor(4);
    }

    double status = last * armorFactor(3) * armorFactor(2) * armorFactor(1) * armorFactor(0) * firepower;
    uint8_t color;

    if (!(status >= 0.5))
    {
        color = status > 0.2 ? 0xf2 : 0xef;
    }
    else
    {
        color = 0xb;
    }

    auto end = static_cast<int32_t>(status * 74.0 + xPos);

    for (int32_t y = 2; y <= 5; ++y)
    {
        drawLine(port->frame(), xPos, y, end, y, color);
    }
}

auto VehicleRepairBlock::setPilotStats() -> void
{
}

auto VehicleRepairBlock::setPilotHealth(int32_t health, lPort* port) -> void
{
    (void)health;
    (void)port;
}

auto VehicleRepairBlock::clearPilot() -> void
{
}

BriefingBox::~BriefingBox()
{
    BriefingBox::destroy();
}

auto BriefingBox::init(LogMech* logMech, LogVehicle* logVehicle) -> void
{
    mech = logMech;
    vehicle = logVehicle;
    lObject::init(0xd3, 0x16f, 0x1ab, 0x6f, nullptr, repairScreen()->lport());

    if (logMech == nullptr)
    {
        inventoryPane = nullptr;
        return;
    }

    auto* pane = new ScrollPane;

    if (pane != nullptr)
    {
        pane->init();
    }

    inventoryPane = pane;
    pane->init(0x62, 0x58, 0x143, 0x11, static_cast<char*>(nullptr));
    logMech->repairBlock->setInventory(pane);
    addChild(pane);
}

auto BriefingBox::destroy() -> void
{
    if (inventoryPane != nullptr)
    {
        delete inventoryPane;
        inventoryPane = nullptr;
    }

    lObject::destroy();
}

auto BriefingBox::drawBackground() -> void
{
    // Port: the box is drawn each frame (PaintBox) by the briefing screen.
    if (mech != nullptr)
    {
        mech->repairBlock->setInventory(inventoryPane);
    }

    globalLogPtr->briefingScreen->ShowBox(this);
}

auto BriefingBox::PaintBox(PANE* target, int32_t xPos, int32_t yPos) -> void
{
    // The block paints in the briefing screen's layout (it looks at the current screen); a screen change's wipe draws
    // the box while another screen is current.
    lObject* const current = globalLogPtr->currentScreen;
    globalLogPtr->currentScreen = globalLogPtr->briefingScreen;
    lBlockPort work(target, xPos, yPos, 0x1ab, 0x6f, false);

    if (mech == nullptr)
    {
        vehicle->repairBlock->drawBackground(-1, &work);
    }
    else
    {
        MechRepairBlock* block = mech->repairBlock;
        block->drawBackground(-1, &work);
        ScrollPane* pane = inventoryPane;
        // The weapon list and its slider, then the tonnage bar.
        lBlockPort list(work.frame(), 0x143, 0x11, 0x62, 0x58, false);
        pane->DrawContentTo(list.frame(), 0, 0);
        pane->DrawSliderColumn(work.frame(), pane->width() + 0x136, 0x11, false);
        int32_t fill = static_cast<int32_t>(static_cast<double>(mech->weaponTonnage) / mech->freeTonnage * 55.0);
        drawTonnageBar(work.frame(), 0x16c, fill);
    }

    globalLogPtr->currentScreen = current;
    globalLogPtr->darken(0, g_logistic_fadetable, &work);
}

auto BriefingBox::drawVehicleBackground() -> void
{
}

auto BriefingBox::handleEvent(aEvent* event) -> void
{
    aObject* pane = child(0);

    if (parent != nullptr && (event->type == 8 || event->type == 9))
    {
        parent->handleEvent(event);
        return;
    }

    if (pane != nullptr)
    {
        pane->handleEvent(event);
    }
}

auto BriefingBox::MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) -> bool
{
    // The pane redraws the box (its parent) when it scrolls.
    aObject* pane = child(0);
    return overPane(pane, xPos, yPos) && pane->MouseWheel(steps, xPos, yPos);
}

auto BriefingBox::draw() -> void
{
    // The original repainted the weapon list (scrolled) into the briefing screen's picture; the screen draws the
    // whole box each frame (PaintBox).
}

auto BriefingBox::display() -> void
{
}
