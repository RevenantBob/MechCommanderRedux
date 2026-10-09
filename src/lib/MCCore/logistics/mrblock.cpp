#include "stdafx.h"
#include "logistics/mrblock.h"
#include "gui/afont.h"
#include "gui/scrlpane.h"
#include "gui/updisp.h"
#include "lib/MCFatal.h"
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
#include "object/MCMasterComponent.h"
#include "sound/MCSoundSystem.h"
#include "vfx/MCVfxFunctions.h"

// 0x008009d0 is AlphaTable row 0x100 (AlphaTable is at 0x007f09d0): the alpha colour greyed-out rows darken through.
char* LogisticFadetable = reinterpret_cast<char*>(AlphaTable.data()) + 0x100 * 256;

namespace
{
    /// <summary>
    /// The repair block whose missing replacement parts the refit dialog is showing (0x00808694). The original names it
    /// <c>globalMechPurchaseBlock</c>, like purchase.cpp's <c>MechPurchaseBlock*</c>; the port renames this one.
    /// </summary>
    MCMechRepairBlock* RefitBlock = nullptr;

    /// <summary>The dragged item copy's <c>itemNum</c> (0x0080868c).</summary>
    uint8_t DragItemNum = 0;
    /// <summary>The dragged item's component (0x0080868d).</summary>
    uint8_t DragMasterID = 0;
    /// <summary>-1 while something is dragged with the left button held (0x00808698).</summary>
    int32_t LeftDrag = 0;
    /// <summary>The mech block's drag icon position (0x0080869c / 0x008086a0).</summary>
    int32_t DragX = 0;
    int32_t DragY = 0;
    /// <summary>Set by a right-button press, cleared by the release (0x008086a8).</summary>
    int32_t RightHeld = 0;
    /// <summary>-1 while the whole mech is dragged (0x008086ac).</summary>
    int32_t DraggingMech = 0;
    /// <summary>-1 while an item is dragged out of the weapon list (0x008086b0).</summary>
    int32_t DraggingItem = 0;
    /// <summary>-1 while a repair slider is dragged; <c>lastY</c> is the slider, <c>lastX</c> its position (0x008086b4).</summary>
    int32_t DraggingSlider = 0;
    /// <summary>Set to -1 when a click selects another mech; never read (0x008086b8).</summary>
    int32_t MechSelected = 0;
    /// <summary>-1 while a repair button is held (the buttons are redrawn on release; 0x008086bc).</summary>
    int32_t RepairButtonDown = 0;
    /// <summary>The dragged item copy's damage (0x008086c0).</summary>
    int32_t DragItemHits = 0;
    /// <summary>The dragged item's inventory index (0x0079fff8, -1 when none).</summary>
    int32_t DragItemIndex = -1;

    /// <summary>-1 while the vehicle is dragged with the left button held (0x008086c4).</summary>
    int32_t VehicleLeftDrag = 0;
    /// <summary>The vehicle block's drag icon position (0x008086c8 / 0x008086cc).</summary>
    int32_t VehicleDragX = 0;
    int32_t VehicleDragY = 0;
    /// <summary>Set by a right-button press on a vehicle, cleared by the release (0x008086d0).</summary>
    int32_t VehicleRightHeld = 0;
    /// <summary>-1 while the vehicle is dragged (0x008086d4).</summary>
    int32_t DraggingVehicle = 0;
    /// <summary>A pilot dropped on a vehicle (0x008086d8). Nothing sets it: vehicles take no pilots.</summary>
    MCLogWarrior* VehiclePilot = nullptr;
    /// <summary>The pilot index the vehicle release shifts the pilots from (0x007a0000, always -1).</summary>
    int32_t VehiclePilotShift = -1;

    void* LogAlloc(uint32_t size)
    {
        return GlobalLogPtr->LogisticsBlocks->Allocate(size);
    }

    void LogFree(void* block)
    {
        GlobalLogPtr->LogisticsBlocks->Free(block);
    }

    void PlaySample(uint32_t sampleId)
    {
        SoundSystem()->PlayDigitalSample(sampleId, 1, nullptr, 0, 0);
    }

    void WriteText(MCGuiFont* font, MCPane* pane, int32_t x, int32_t y, const char* text)
    {
        font->WriteString(pane, x, y, reinterpret_cast<uint8_t*>(const_cast<char*>(text)), -1);
    }

    void DrawLine(MCPane* pane, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t color)
    {
        VfxLineDraw(pane, x0, y0, x1, y1, color);
    }

    MCRepairScreen* RepairScreen()
    {
        return GlobalLogPtr->RepairScreen;
    }

    /// <summary>The port the repair screen's unit rows are drawn into.</summary>
    MCLogPort* UnitRowsPort()
    {
        MCLogPort* port = nullptr;
        RepairScreen()->UnitPane->GetDisplayPort(port);
        return port;
    }

    MCComponentForm ComponentForm(uint8_t masterID)
    {
        return MasterComponentList[masterID].Form;
    }

    bool IsWeapon(MCComponentForm form)
    {
        return form == MCComponentForm::WeaponEnergy || form == MCComponentForm::WeaponBallistic ||
               form == MCComponentForm::WeaponMissile;
    }

    bool IsEquipment(MCComponentForm form)
    {
        return form == MCComponentForm::Sensor || form == MCComponentForm::Ecm || form == MCComponentForm::Probe;
    }

    /// <summary>A weapon with its own ammo (ballistic and missile): moving it moves an ammo item too.</summary>
    bool UsesAmmo(uint8_t masterID)
    {
        return ComponentForm(masterID) == MCComponentForm::WeaponBallistic ||
               ComponentForm(masterID) == MCComponentForm::WeaponMissile;
    }

    /// <summary>
    /// A weapon's worth in the condition figures: its damage per 10 seconds, as a short, times its long range over 24,
    /// as a short.
    /// </summary>
    int16_t WeaponValue(const MCMasterComponent& component)
    {
        auto perTen = static_cast<int16_t>(static_cast<int32_t>(static_cast<double>(component.Damage) * 10.0 /
                                                                static_cast<double>(component.RecycleTime)));
        return static_cast<int16_t>(
            static_cast<int32_t>(static_cast<double>(perTen) * static_cast<double>(component.WeaponRange[3]) *
                                 static_cast<double>(0.041666668f)));
    }

    /// <summary>
    /// The damage state of a location from its percentage left: 1 above 75, 2 above 50, 3 above 25, 4 above 0, 0 when
    /// gone.
    /// </summary>
    int32_t DamageState(uint32_t percent)
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
    uint32_t PercentLeft(uint8_t current, uint8_t maximum)
    {
        if (maximum == 0)
        {
            return 0;
        }

        return static_cast<uint32_t>(current) * 100 / maximum;
    }

    /// <summary>The colour table of damage state <paramref name="state"/> for armor (the internal structure's is 5 on).</summary>
    uint8_t* ArmorLookaside(int32_t state)
    {
        static constexpr int32_t tables[5] = {4, 0, 1, 2, 3};
        return GlobalLogPtr->ShapeLookaside[tables[state]];
    }

    uint8_t* InternalLookaside(int32_t state)
    {
        static constexpr int32_t tables[5] = {9, 5, 6, 7, 8};
        return GlobalLogPtr->ShapeLookaside[tables[state]];
    }

    /// <summary>
    /// Draws the tonnage bar at <paramref name="xPos"/>: a 0x35-pixel frame over rows 9..12 of <paramref name="pane"/>,
    /// filled <paramref name="fill"/> pixels.
    /// </summary>
    void DrawTonnageBar(MCPane* pane, int32_t xPos, int32_t fill)
    {
        DrawLine(pane, xPos + 1, 9, xPos + 0x35, 9, 0xed);
        DrawLine(pane, xPos, 10, xPos + 0x35, 10, 0xce);
        DrawLine(pane, xPos, 11, xPos + 0x35, 11, 0xce);
        DrawLine(pane, xPos + 1, 12, xPos + 0x35, 12, 0xae);
        DrawLine(pane, xPos + 0x36, 10, xPos + 0x36, 11, 0xae);

        if (fill == 0)
        {
            return;
        }

        int32_t end = fill + xPos;
        DrawLine(pane, xPos + 1, 9, end, 9, 0xe3);
        DrawLine(pane, xPos, 10, xPos, 11, 0xe3);
        DrawLine(pane, xPos + 1, 12, end, 12, 0xe5);
        DrawLine(pane, end + 1, 10, end + 1, 11, 0xe5);
        DrawLine(pane, xPos + 1, 10, end, 10, 0xe4);
        DrawLine(pane, xPos + 1, 11, end, 11, 0xe4);
        DrawLine(pane, end + 2, 10, end + 2, 11, 0x10);
        VfxPixelWrite(pane, end + 1, 9, 0x10);
        VfxPixelWrite(pane, end + 1, 12, 0x10);
    }

    /// <summary>The pixel offset of a scroll pane's view into its content.</summary>
    int32_t ScrollPixels(MCScrollPane* pane)
    {
        return static_cast<int32_t>(static_cast<double>(pane->ScrollPos) * static_cast<double>(pane->ScrollUnit));
    }

    /// <summary>Whether a point is over the pane, scroll bar included (the area its clicks go to).</summary>
    bool OverPane(MCGuiObject* pane, int32_t xPos, int32_t yPos)
    {
        return pane != nullptr && pane->GlobalX() <= xPos && xPos <= pane->GlobalX() + pane->Width() &&
               pane->GlobalY() <= yPos && yPos <= pane->GlobalY() + pane->Height();
    }

    /// <summary>Whether the event is inside the pane, left of its scroll bar (all edges excluded).</summary>
    bool OverPaneInside(MCGuiObject* pane, MCGuiEvent* event)
    {
        return pane->GlobalX() < event->X && event->X < pane->GlobalX() + pane->Width() - 0xd &&
               pane->GlobalY() < event->Y && event->Y < pane->GlobalY() + pane->Height();
    }

    void DeleteDragIcon()
    {
        if (GlobalLogPtr->DragIcon != nullptr)
        {
            delete GlobalLogPtr->DragIcon;
        }

        GlobalLogPtr->DragIcon = nullptr;
    }

    /// <summary>
    /// Shows a component's info under the inventory: its picture (<c>lscicc</c>), the range, damage and recycle texts
    /// of its inventory block (made when missing) and its description.
    /// </summary>
    void DrawItemInfo(MCLogInventoryItem* item, MCInventoryList* inventory)
    {
        if (item->InventoryBlock == nullptr)
        {
            auto* block = new MCCompInventoryBlock;
            item->InventoryBlock = block;
            block->Init(item);
            inventory->LoadDescription(0, item);
        }

        PrepareInfoDescription(item->Description);
        RepairScreen()->ShowComponentInfo(item->InventoryBlock, true);
    }

    /// <summary>Shows a message box with string <paramref name="id"/> and an enabled OK button.</summary>
    void ShowMessage(uint32_t id)
    {
        char text[256];
        CLoadString(ThisInstance, id, text, 0xfe);
        GlobalLogPtr->MessageDialog->SetText(text);
        GlobalLogPtr->MessageDialog->SetTwoButton(0);
        MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
        dialog->Callback = nullptr;
        char upArt[] = "bh_okay.tga";
        char downArt[] = "bg_okay.tga";
        dialog->OkButton->SetUpPicture(upArt);
        GlobalLogPtr->MessageDialog->OkButton->SetDownPicture(downArt);
        MCLogDialogButton* button = GlobalLogPtr->MessageDialog->OkButton;
        button->Disabled = 0;
        GlobalLogPtr->MessageDialog->Activate();
    }

    /// <summary>Takes a mech's brief block off the briefing screen.</summary>
    void HideBriefBlock(MCGuiObject* block)
    {
        // Port fix: a block without a parent is skipped (the original called through the null parent).
        if (block->Parent != nullptr)
        {
            block->Parent->RemoveChild(block);
        }

        block->ShowGuiWindow(0);
    }

    /// <summary>Sums the current (or maximum) points of <paramref name="count"/> locations.</summary>
    int32_t SumPoints(const MCLogMech::ArmorPoints* points, int32_t count, bool maximum)
    {
        int32_t sum = 0;

        for (int32_t i = 0; i < count; ++i)
        {
            sum += maximum ? points[i].MaxArmor : points[i].CurArmor;
        }

        return sum;
    }
}

MCMechRepairBlock::~MCMechRepairBlock()
{
    MCMechRepairBlock::Destroy();
}

auto RefitItemCallback() -> void
{
    MCLogMech* mech = RefitBlock->Mech;

    for (MCLogInventoryItem* item = mech->Inventory->Items; item != nullptr; item = item->Next)
    {
        switch (ComponentForm(item->MasterID))
        {
            case MCComponentForm::Sensor:
            case MCComponentForm::WeaponEnergy:
            case MCComponentForm::WeaponBallistic:
            case MCComponentForm::WeaponMissile:
            case MCComponentForm::Ecm:
            case MCComponentForm::Probe:
            {
                // Every damaged copy the player could not replace leaves the mech.
                MCLogInventoryStat* stat = item->Stats;

                while (stat != nullptr)
                {
                    if (stat->Hits == 0)
                    {
                        stat = stat->Next;
                        continue;
                    }

                    uint8_t masterID = item->MasterID;

                    // Original behaviour (OB-081): the loop moves on to the next item before the removal and walks its
                    // stats whatever its form.
                    if (item->Count == 1)
                    {
                        item = item->Next;
                    }

                    float tonnage = MasterComponentList[masterID].Tonnage;

                    if (UsesAmmo(masterID))
                    {
                        uint8_t ammo = MasterComponentList[masterID].AmmoMasterId;
                        tonnage = MasterComponentList[ammo].Tonnage + tonnage;
                        mech->Inventory->RemoveItem(ammo, -1);
                    }

                    uint8_t statID = stat->StatID;
                    mech->UsedTonnage -= tonnage;
                    mech->WeaponTonnage -= tonnage;
                    mech->Inventory->RemoveItem(masterID, statID);

                    if (item == nullptr)
                    {
                        goto done;
                    }

                    stat = item->Stats;
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
    RefitBlock->DrawBackground(RefitBlock->SlotIndex, nullptr);
    RepairScreen()->CreateCompInvBlock();
    RepairScreen()->SetUpCompInv(0, -1);
    RefitBlock = nullptr;
}

auto MCMechRepairBlock::Init(MCLogMech* logMech) -> void
{
    Mech = logMech;
    MCScrollPane* rows = RepairScreen()->UnitPane;
    ShortRangeWeapons = nullptr;
    NumShortRangeWeapons = 0;
    MediumRangeWeapons = nullptr;
    NumMediumRangeWeapons = 0;
    LongRangeWeapons = nullptr;
    NumLongRangeWeapons = 0;
    Equipment = nullptr;
    NumEquipment = 0;
    ItemHits = nullptr;
    NumItems = 0;
    DragPort = nullptr;
    MCLogPort* rowsPort = nullptr;
    rows->GetDisplayPort(rowsPort);
    MCLogObject::Init(0, 0, 0x19a, 0x70, nullptr, rowsPort);
    ListPosition = Mech->NameIndex;

    auto* pane = new MCScrollPane;

    if (pane != nullptr)
    {
        pane->Init();
    }

    InventoryPane = pane;
    pane->Init(0x62, 0x58, 0x135, 0x11, static_cast<char*>(nullptr));
    AddChild(pane);

    char fileName[256];
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsrupm05.tga", ArtPath);
    SliderArtPort = new MCLogPort;
    SliderArtPort->Init(fileName);

    // The sliders' scales: 61 pixels over the total maximum; the starting points are the current values.
    int32_t totalArmor = 0;

    for (int32_t i = 0; i < 11; ++i)
    {
        totalArmor += Mech->Armor[i].MaxArmor;
        StartArmor[i] = Mech->Armor[i].CurArmor;
    }

    ArmorPixelScale = static_cast<float>(61.0 / static_cast<double>(totalArmor));
    int32_t totalInternal = 0;

    for (int32_t i = 0; i < 8; ++i)
    {
        totalInternal += Mech->Internals[i].MaxArmor;
        StartInternal[i] = Mech->Internals[i].CurArmor;
    }

    InternalPixelScale = static_cast<float>(61.0 / static_cast<double>(totalInternal));
    SetEngineSlider(-1);
    SetArmorSlider(-1);
    SetInternalSlider(-1);

    MCLogInventoryItem* item = Mech->Inventory->Items;

    while (ComponentForm(item->MasterID) != MCComponentForm::Engine)
    {
        item = item->Next;
    }

    EngineStat = item->Stats;
    ArmorSliderStart = ArmorSliderPos;
    InternalSliderStart = InternalSliderPos;
    EngineSliderStart = EngineSliderPos;
}

auto MCMechRepairBlock::Destroy() -> void
{
    MCLogInvScreen::ForgetInfoSource(this);

    if (InventoryPane != nullptr)
    {
        delete InventoryPane;
    }

    InventoryPane = nullptr;

    if (SliderArtPort != nullptr)
    {
        delete SliderArtPort;
        SliderArtPort = nullptr;
    }

    if (ShortRangeWeapons != nullptr)
    {
        LogFree(ShortRangeWeapons);
        NumShortRangeWeapons = 0;
        ShortRangeWeapons = nullptr;
    }

    if (MediumRangeWeapons != nullptr)
    {
        LogFree(MediumRangeWeapons);
        NumMediumRangeWeapons = 0;
        MediumRangeWeapons = nullptr;
    }

    if (LongRangeWeapons != nullptr)
    {
        LogFree(LongRangeWeapons);
        NumLongRangeWeapons = 0;
        LongRangeWeapons = nullptr;
    }

    if (Equipment != nullptr)
    {
        LogFree(Equipment);
        NumEquipment = 0;
        Equipment = nullptr;
    }

    if (ItemHits != nullptr)
    {
        LogFree(ItemHits);
        NumItems = 0;
        ItemHits = nullptr;
    }

    MCLogObject::Destroy();
}

auto MCMechRepairBlock::UndeployMech() -> void
{
    MCLogMech* logMech = Mech;

    if (logMech->Deployed == 0)
    {
        return;
    }

    for (int32_t lance = 0; lance < 3; ++lance)
    {
        for (int32_t slot = 0; slot < 4; ++slot)
        {
            int32_t& unit = GlobalLogPtr->DeploySlots[lance][slot].Unit;

            if (unit < 0 || unit != SlotIndex)
            {
                continue;
            }

            if (MPlayer != nullptr)
            {
                GlobalLogPtr->SendRemoveForceMessage(lance, slot);
            }

            HideBriefBlock(logMech->BriefBlock);
            // Both loops end after the first match.
            slot = 5;
            lance = 5;
            unit = -1;
        }
    }

    logMech->Deployed = 0;
}

auto MCMechRepairBlock::HandleEvent(MCGuiEvent* event) -> void
{
    int32_t eventX = event->X;
    int32_t localX = eventX - GlobalX();
    int32_t eventY = event->Y;
    int32_t localY = eventY - GlobalY();

    if (GlobalLogPtr->CurrentScreen == GlobalLogPtr->PurchaseScreen)
    {
        return;
    }

    if (LeftDrag == 0)
    {
        if (Parent != nullptr && RightHeld == 0 && (event->Type == 8 || event->Type == 9))
        {
            Parent->HandleEvent(event);
            return;
        }

        if (event->Key == 0)
        {
            // The ticker explains what the cursor is over.
            static constexpr struct
            {
                RECT Area{};
                uint32_t StringId = 0;
            } helpAreas[] = {
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
                stringId = Mech->PilotIndex < 0 ? 0x35 : 0x36;
            }
            else
            {
                for (const auto& help : helpAreas)
                {
                    if (PtInRect(&help.Area, point) != 0)
                    {
                        stringId = help.StringId;
                        break;
                    }
                }
            }

            if (stringId != 0)
            {
                char text[256];
                CLoadString(ThisInstance, stringId, text, 0xfe);
                GlobalLogPtr->Ticker->SetString(text);
            }
            else
            {
                GlobalLogPtr->Ticker->SetString(nullptr);
            }
        }
    }

    MCLogMech* logMech = Mech;
    MCLogWarrior* warrior = nullptr;
    GlobalLogPtr->AssignedWarriorList->GetWarriorInfo(logMech->PilotIndex, warrior);
    int32_t eventType = event->Type;

    switch (eventType)
    {
        case 1:
        {
            if (RightHeld != 0)
            {
                return;
            }
            break;
        }

        case 3:
        {
            if (LeftDrag != 0)
            {
                return;
            }

            if (DebugFunction1(localX, localY) != 0)
            {
                return;
            }

            RightHeld = 1;
            break;
        }

        case 4:
        {
            if (RightHeld != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 6:
        {
            if (LeftDrag != 0 && eventType == 6)
            {
                return;
            }

            RightHeld = 0;

            if (RepairButtonDown != 0 && logMech == RepairScreen()->SelectedMech)
            {
                DrawButtons(nullptr);
                RepairButtonDown = 0;
            }

            if (Application->GrabbedObject() == nullptr)
            {
                return;
            }

            Application->SetCursorVisible(-1);
            Application->Release();
            LeftDrag = 0;
            DrawButtons(nullptr);
            DeleteDragIcon();
            MCGuiObject* inventory = RepairScreen()->InventoryPane;
            bool droppedOnInventory = eventType == 6 || OverPaneInside(inventory, event);

            if (DraggingItem == 0)
            {
                if (DraggingSlider != 0)
                {
                    DraggingSlider = 0;
                    Application->Release();
                    DrawArmorSlider(nullptr);
                    DrawInternalSlider(nullptr);
                    DrawEngineSlider(nullptr);
                    DrawStatusBar();
                    DrawButtons(nullptr);
                    return;
                }

                if (DraggingMech == 0)
                {
                    // The pilot: dropped on the inventory, it leaves the mech.
                    Application->Release();
                    LeftDrag = 0;
                    DeleteDragIcon();
                    MCLogMech* pilotsMech = Mech;
                    MCLogWarrior* pilot = nullptr;
                    GlobalLogPtr->AssignedWarriorList->GetWarriorInfo(pilotsMech->PilotIndex, pilot);

                    if (pilot != nullptr && droppedOnInventory)
                    {
                        if (pilotsMech->Deployed != 0)
                        {
                            UndeployMech();
                        }

                        MCLogMech* current = Mech;
                        GlobalLogPtr->ShiftPilots(current->PilotIndex, -1);
                        int32_t row = SlotIndex;
                        pilot->Assigned = 0;
                        GlobalLogPtr->SetPilot(row, -1);
                        GlobalLogPtr->ReorderWarriors();
                        current->CalcBR();
                        current->CalcPilotModifier();
                        current->RepairBlock->DrawBackground(current->RepairBlock->SlotIndex, nullptr);
                        DrawBackground(row, nullptr);
                        RepairScreen()->CreatePilotInvBlock();
                        RepairScreen()->SetUpPilotInv(-1, -1);
                        SoundSystem()->PlayPilotSpeech(pilot->PilotAudio, 2);
                        return;
                    }

                    PlaySample(0x33);
                    pilotsMech->RepairBlock->DrawBackground(pilotsMech->RepairBlock->SlotIndex, nullptr);
                    return;
                }

                // The whole mech: dropped on the inventory, it leaves the force.
                DraggingMech = 0;

                if (!droppedOnInventory)
                {
                    PlaySample(0x33);
                    return;
                }

                if (Mech->Deployed != 0)
                {
                    UndeployMech();
                }

                RepairScreen()->SelectMech(nullptr);
                PlaySample(0x34);
                MCLogMech* leaving = Mech;
                int32_t pilotIndex = leaving->PilotIndex;

                if (pilotIndex >= 0)
                {
                    MCLogWarrior* pilot = nullptr;
                    GlobalLogPtr->AssignedWarriorList->GetWarriorInfo(pilotIndex, pilot);
                    pilot->Assigned = 0;
                    GlobalLogPtr->ShiftPilots(pilotIndex, -1);
                    GlobalLogPtr->SetPilot(SlotIndex, -1);
                    GlobalLogPtr->ReorderWarriors();
                    RepairScreen()->CreatePilotInvBlock();
                    RepairScreen()->SetUpPilotInv(-1, -1);
                }

                MCMechBriefBlock* brief = leaving->BriefBlock;

                if (brief != nullptr && brief->Parent != nullptr)
                {
                    brief->Parent->RemoveChild(brief);
                    brief->ShowGuiWindow(0);
                }

                for (auto& lance : GlobalLogPtr->DeploySlots)
                {
                    for (auto& slot : lance)
                    {
                        if (slot.Unit < 0)
                        {
                            continue;
                        }

                        if (slot.Unit == SlotIndex)
                        {
                            slot.Unit = -1;
                        }
                        else if (SlotIndex < slot.Unit)
                        {
                            slot.Unit = slot.Unit - 1;
                        }
                    }
                }

                leaving->Deployed = 0;
                leaving->Assigned = 0;

                if (leaving == RepairScreen()->SelectedMech)
                {
                    RepairScreen()->SelectedMech = nullptr;
                }

                GlobalLogPtr->ReorderMechs();
                MCMechRepairBlock* block = leaving->RepairBlock;

                if (block != nullptr && block->Parent != nullptr)
                {
                    block->Parent->RemoveChild(block);
                }

                RepairScreen()->RemoveMechFromList(leaving);
                leaving->InventoryBlock->DeleteDiagram();
                RepairScreen()->CreateMechInvBlock();
                RepairScreen()->SetUpMechInv(-1, -1);
                return;
            }

            // An item from the weapon list.
            DraggingItem = 0;
            RepairScreen()->DrawBlankInvInfoBlock(-1);

            if (droppedOnInventory)
            {
                // Into the component inventory (a damaged item is thrown away).
                if (Mech->Deployed != 0)
                {
                    UndeployMech();
                }

                PlaySample(0x34);

                if (DragItemHits == 0)
                {
                    MCInventoryList* components = GlobalLogPtr->ComponentInventory;
                    MCLogInventoryItem* item = components->GetItemInfo(components->GetIndexFromMasterID(DragMasterID));

                    if (item == nullptr)
                    {
                        MCLogInventoryStat* stat = components->CreateStat(DragMasterID, 0, 1, 0, 0xff);
                        components->AddItem(DragMasterID, stat, -1);
                        item = components->GetItemInfo(components->GetIndexFromMasterID(DragMasterID));
                        auto* block = new MCCompInventoryBlock;
                        item->InventoryBlock = block;
                        block->Init(item);
                        item->InventoryBlock->InventoryIndex = GlobalLogPtr->ComponentInventory->NumItems - 1;
                    }

                    if (item->Count != 0)
                    {
                        ++item->Count;
                        item->InventoryBlock->DrawBackground();
                    }
                    else
                    {
                        item->Count = 1;
                        RepairScreen()->CreateCompInvBlock();
                        RepairScreen()->SetUpCompInv(0, -1);
                    }
                }
            }
            else
            {
                // Back into the selected mech (with a weapon's ammo).
                MCLogMech* target = RepairScreen()->SelectedMech;
                bool withAmmo = UsesAmmo(DragMasterID);
                PlaySample(0x34);
                MCInventoryList* inventory = target->Inventory;
                MCLogInventoryStat* stat =
                    inventory->CreateStat(DragItemNum, static_cast<uint8_t>(DragItemHits), 0, 1, 0xff);
                inventory->AddItem(DragMasterID, stat, -1);
                float tonnage = MasterComponentList[DragMasterID].Tonnage;

                if (withAmmo)
                {
                    uint8_t ammo = MasterComponentList[DragMasterID].AmmoMasterId;
                    stat = inventory->CreateStat(inventory->NextStatID, 0, 0, -1, 0xff);
                    inventory->AddItem(ammo, stat, -1);
                    tonnage = MasterComponentList[ammo].Tonnage + tonnage;
                }

                target->UsedTonnage = tonnage + target->UsedTonnage;
                target->WeaponTonnage = tonnage + target->WeaponTonnage;
                target->CalcBR();
                target->RepairBlock->DrawBackground(target->RepairBlock->SlotIndex, nullptr);
                DrawStatusBar();
                RepairScreen()->SetUpCompInv(0, -1);
            }

            DragItemIndex = -1;
            DrawButtons(nullptr);
            return;
        }

        case 7:
        {
            auto* pane = static_cast<MCScrollPane*>(Child(0));

            if (pane->GlobalX() < eventX)
            {
                if (eventX < pane->GlobalX() + pane->Width() - 0xd && pane->GlobalY() < eventY &&
                    eventY < pane->GlobalY() + pane->Height())
                {
                    // Over the weapon list: show the item's info.
                    int32_t line = (eventY - pane->GlobalY() + pane->GetScrollOffset()) / (GreenFont->Height() + 2);
                    MCLogInventoryItem* item = GetItemFromScrollPane(pane, line, &DragItemNum);

                    if (item != nullptr)
                    {
                        RepairScreen()->DrawBlankInvInfoBlock(2);
                        DrawItemInfo(item, Mech->Inventory);
                        return;
                    }
                }
            }
            else if (eventX < pane->GlobalX())
            {
                // Over the mech: show its info.
                RepairScreen()->DrawBlankInvInfoBlock(0);

                if (DragPort == nullptr)
                {
                    DragPort = new MCLogPort;
                    DragPort->Init(0x1c, 0x1e, -1);
                    VfxPaneWipe(DragPort->Frame(), 0x10);

                    for (int32_t location = 0; location < 8; ++location)
                    {
                        GlobalLogPtr->DrawMechBodyLoc(Mech, location, DragPort, 2, 0);
                    }

                    // The battle rating bar along the left edge: 26 pixels at 18010.
                    int32_t bar =
                        static_cast<int32_t>(static_cast<double>(Mech->BattleRating) * (1.0 / 18010.0) * 26.0);
                    DrawLine(DragPort->Frame(), 0, 0x1b, 0, 0x1b - bar, 0xe4);
                    DrawLine(DragPort->Frame(), 1, 0x1b, 1, 0x1b - bar, 0xe4);
                }

                PrepareInfoDescription(Mech->Description);
                RepairScreen()->ShowInfo(MCInvInfoBox::Kind::RepairMech, this);
            }

            if (LeftDrag != 0)
            {
                DragX = eventX - 0xf;
                DragY = eventY - 0xf;
                GlobalLogPtr->DragIcon->MoveTo(DragX, DragY, 0);
                return;
            }

            if (DraggingSlider == 0)
            {
                return;
            }

            // Dragging a repair slider: undo the drag so far, then repair up to the new position as far as the resource
            // points go.
            if (Mech->Deployed != 0)
            {
                UndeployMech();
            }

            if (DragPort != nullptr)
            {
                delete DragPort;
                DragPort = nullptr;
            }

            int32_t position = localX;

            if (position > 0x127)
            {
                position = 0x127;
            }

            int32_t newLastX = position;

            if (LastY == 0)
            {
                if (position < ArmorSliderStart)
                {
                    position = ArmorSliderStart;
                    DrawArmorSlider(nullptr);
                    DrawDamageDiagram(nullptr);
                    DrawStatusBar();
                }

                MCLogMech* repaired = Mech;
                int32_t before = SumPoints(repaired->Armor, 11, false);

                for (int32_t i = 0; i < 11; ++i)
                {
                    repaired->Armor[i].CurArmor = static_cast<uint8_t>(StartArmor[i]);
                }

                int32_t restored = SumPoints(repaired->Armor, 11, false);
                int32_t cost = GlobalLogPtr->ArmorCost;
                ResourcePoints = ResourcePoints + (before - restored) * cost;
                int32_t points;

                if (position < 0x127)
                {
                    points = static_cast<int32_t>(static_cast<double>(position - ArmorSliderStart) /
                                                  static_cast<double>(ArmorPixelScale));
                }
                else
                {
                    points = SumPoints(repaired->Armor, 11, true) - restored;
                }

                int32_t affordable = ResourcePoints / cost;

                if (points < affordable)
                {
                    RepairArmor(points);
                    ArmorSliderPos = position;
                    ResourcePoints = ResourcePoints - GlobalLogPtr->ArmorCost * points;
                }
                else
                {
                    RepairArmor(affordable);
                    ArmorSliderPos = static_cast<int32_t>(
                        static_cast<double>(affordable) * static_cast<double>(ArmorPixelScale) + ArmorSliderStart);
                    ResourcePoints = ResourcePoints - GlobalLogPtr->ArmorCost * affordable;
                }

                DrawArmorSlider(nullptr);
                DrawDamageDiagram(nullptr);
                DrawStatusBar();
                newLastX = position;
            }
            else if (LastY == 1)
            {
                if (position < InternalSliderStart)
                {
                    // Original behaviour (OB-080): the armor slider is the one redrawn.
                    position = InternalSliderStart;
                    DrawArmorSlider(nullptr);
                    DrawDamageDiagram(nullptr);
                    DrawStatusBar();
                }

                MCLogMech* repaired = Mech;
                int32_t before = SumPoints(repaired->Internals, 8, false);

                for (int32_t i = 0; i < 8; ++i)
                {
                    repaired->Internals[i].CurArmor = static_cast<uint8_t>(StartInternal[i]);
                }

                int32_t restored = SumPoints(repaired->Internals, 8, false);
                int32_t cost = GlobalLogPtr->InternalCost;
                ResourcePoints = ResourcePoints + (before - restored) * cost;
                int32_t points;

                if (position < 0x127)
                {
                    points = static_cast<int32_t>(static_cast<double>(position - InternalSliderStart) /
                                                  static_cast<double>(InternalPixelScale));
                }
                else
                {
                    points = SumPoints(repaired->Internals, 8, true) - restored;
                }

                int32_t affordable = ResourcePoints / cost;

                if (points < affordable)
                {
                    RepairInternal(points);
                    InternalSliderPos = position;
                    ResourcePoints = ResourcePoints - GlobalLogPtr->InternalCost * points;
                    DrawInternalSlider(nullptr);
                }
                else
                {
                    RepairInternal(affordable);
                    InternalSliderPos =
                        static_cast<int32_t>(static_cast<double>(affordable) * static_cast<double>(InternalPixelScale) +
                                             InternalSliderStart);
                    ResourcePoints = ResourcePoints - GlobalLogPtr->InternalCost * affordable;
                    DrawInternalSlider(nullptr);
                }

                DrawDamageDiagram(nullptr);
                DrawStatusBar();
                newLastX = position;
            }
            else if (LastY == 2)
            {
                if (EngineStat->Hits != 0 && Mech->Deployed != 0)
                {
                    // An inline undeploy without the network message (never reached: the mech was undeployed above).
                    for (int32_t lance = 0; lance < 3; ++lance)
                    {
                        for (int32_t slot = 0; slot < 4; ++slot)
                        {
                            int32_t& unit = GlobalLogPtr->DeploySlots[lance][slot].Unit;

                            if (unit < 0 || unit != SlotIndex)
                            {
                                continue;
                            }

                            HideBriefBlock(Mech->BriefBlock);
                            slot = 5;
                            lance = 5;
                            unit = -1;
                        }
                    }

                    Mech->Deployed = 0;
                }

                int32_t target = position;

                if (position < EngineSliderStart)
                {
                    // Original behaviour (OB-080): the armor slider is the one redrawn.
                    DrawArmorSlider(nullptr);
                    DrawDamageDiagram(nullptr);
                    DrawStatusBar();
                    target = EngineSliderStart;
                }

                // The engine repairs in whole damage levels; each costs engineCost.
                int32_t cost = GlobalLogPtr->EngineCost;
                uint8_t& hits = EngineStat->Hits;
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

                EngineSliderPos = newPosition;
                DrawEngineSlider(nullptr);
                DrawStatusBar();
                newLastX = newPosition;
            }

            LastX = newLastX;
            Mech->CalcBR();
            DrawBR(nullptr);
            return;
        }

        default:
            return;
    }

    // A button press (1 or 3).
    if (RepairScreen()->SelectedMech != Mech)
    {
        RepairScreen()->SelectMech(Mech);
        MechSelected = -1;
        RightHeld = 0;
        return;
    }

    auto* pane = static_cast<MCScrollPane*>(Child(0));

    if (pane->GlobalX() - 0xd + pane->Width() <= eventX && eventX <= pane->GlobalX() + pane->Width() &&
        pane->GlobalY() <= eventY && eventY <= pane->GlobalY() + pane->Height())
    {
        // The weapon list's scroll bar.
        pane->HandleEvent(event);
        DrawInventory(nullptr);
        return;
    }

    if (pane->GlobalX() < eventX && eventX < pane->GlobalX() + pane->Width() - 0xd && pane->GlobalY() < eventY &&
        eventY < pane->GlobalY() + pane->Height())
    {
        // Pick an item out of the weapon list.
        int32_t line = (eventY - pane->GlobalY() + pane->GetScrollOffset()) / (GreenFont->Height() + 2);
        MCLogInventoryItem* item = GetItemFromScrollPane(pane, line, &DragItemNum);

        if (item == nullptr)
        {
            PlaySample(0x33);
            return;
        }

        Application->SetCursorVisible(0);
        DragMasterID = item->MasterID;
        DraggingItem = -1;
        PlaySample(0x35);
        Application->Grab(this);

        if (eventType == 1)
        {
            LeftDrag = -1;
        }

        SetUpItemDragIcon(item, DragItemNum, event, &DragItemIndex, &DragItemHits);
        return;
    }

    if (0 < localX && localX < 100 && 0x1c < localY && localY < 0x6d && Mech->PilotIndex >= 0)
    {
        // Pick up the pilot: the portrait becomes the drag icon and its place is blanked.
        SoundSystem()->PlayPilotSpeech(warrior->PilotAudio, 10);
        Application->SetCursorVisible(0);
        Application->Grab(this);

        if (eventType == 1)
        {
            LeftDrag = -1;
        }

        DragY = WinHeight * SlotIndex + 0x25;
        DragX = 5;
        auto* icon = new MCDragIcon;
        GlobalLogPtr->DragIcon = icon;
        icon->Begin(eventX - 0xf, eventY - 0xf, 0x20, 0x20, [this](MCLogPort* surface) { OnBeginDragPilot(surface); });
        ClearPilot();
        RepairScreen()->AddChild(GlobalLogPtr->DragIcon);
        GlobalLogPtr->DragIcon->ShowGuiWindow(-1);
        GlobalLogPtr->DragIcon->SetDepth(100);
        DragX = DragX + GlobalX();
        DragY = GlobalY() + 0x26;
        GlobalLogPtr->DragIcon->MoveTo(eventX - 0xf, eventY - 0xf, 0);
        return;
    }

    if (localX < 0xe7)
    {
        // Pick up the whole mech.
        if (GlobalX() <= eventX && eventX <= GlobalX() + Width() && GlobalY() <= eventY &&
            eventY <= GlobalY() + Height())
        {
            PlaySample(0x35);
            Application->SetCursorVisible(0);
            Application->Grab(this);
            DraggingMech = -1;

            if (eventType == 1)
            {
                LeftDrag = -1;
            }

            DragY = eventY - 0x10;
            DragX = eventX - 0x10;
            auto* icon = new MCDragIcon;
            GlobalLogPtr->DragIcon = icon;
            icon->Begin(DragX, DragY, 0x20, 0x20, [this](MCLogPort* surface) { OnBeginDragMech(surface); });
            RepairScreen()->AddChild(GlobalLogPtr->DragIcon);
            GlobalLogPtr->DragIcon->ShowGuiWindow(-1);
            GlobalLogPtr->DragIcon->SetDepth(100);
            GlobalLogPtr->DragIcon->MoveTo(DragX, DragY, 0);
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

    if (ArmorSliderPos - 2 <= localX && localX <= ArmorSliderPos + 6 && 0x36 <= localY && localY <= 0x3d)
    {
        slider = 0;
        sliderPos = ArmorSliderPos;
    }
    else if (InternalSliderPos - 2 <= localX && localX <= InternalSliderPos + 6 && 0x4b <= localY && localY <= 0x52)
    {
        slider = 1;
        sliderPos = InternalSliderPos;
    }
    else if (EngineSliderPos - 2 <= localX && localX <= EngineSliderPos + 6 && 0x60 <= localY && localY <= 0x67)
    {
        slider = 2;
        sliderPos = EngineSliderPos;
    }
    else
    {
        // The repair buttons.
        if (localX < 0xea || 299 < localX)
        {
            return;
        }

        MCLogMech* repaired = Mech;

        if (RepairScreen()->SelectedMech != repaired)
        {
            return;
        }

        RepairButtonDown = -1;
        bool shortOfPoints = false;

        if (localY < 0x17 || 0x26 < localY || CanRepairStructure == 0)
        {
            if (localY < 3 || 0x12 < localY || CanRepairItems == 0)
            {
                return;
            }

            // Repair items: replace every damaged copy from the component inventory; list the ones missing.
            if (repaired->Deployed != 0)
            {
                UndeployMech();
            }

            char missing[256];
            missing[0] = '\0';
            RepairButtonDown = -1;
            PlaySample(0x35);
            _PressedButton = 1;
            UpdateDisplay(0, 0, 0, 0, 0);
            bool anyMissing = false;

            for (MCLogInventoryItem* item = Mech->Inventory->Items; item != nullptr; item = item->Next)
            {
                MCComponentForm form = ComponentForm(item->MasterID);

                for (MCLogInventoryStat* stat = item->Stats; stat != nullptr; stat = stat->Next)
                {
                    switch (form)
                    {
                        case MCComponentForm::Sensor:
                        case MCComponentForm::WeaponEnergy:
                        case MCComponentForm::WeaponBallistic:
                        case MCComponentForm::WeaponMissile:
                        case MCComponentForm::Ecm:
                        case MCComponentForm::Probe:
                        {
                            if (stat->Hits == 0)
                            {
                                break;
                            }

                            MCInventoryList* components = GlobalLogPtr->ComponentInventory;
                            MCLogInventoryItem* stockItem =
                                components->GetItemInfo(components->GetIndexFromMasterID(item->MasterID));

                            if (stockItem == nullptr || stockItem->Count == 0)
                            {
                                // Port fix: the name comes from the mech's own item when the inventory has none (the
                                // original read the name through the null item), and the list is bounded.
                                const char* name = stockItem != nullptr ? stockItem->Name : item->Name;
                                char entry[64];
                                std::snprintf(entry, sizeof(entry), anyMissing ? ",%s" : "%s", name);
                                std::strncat(missing, entry, sizeof(missing) - std::strlen(missing) - 1);
                                anyMissing = true;
                            }
                            else
                            {
                                --stockItem->Count;
                                stockItem->InventoryBlock->DrawBackground();
                                stat->Hits = 0;
                            }
                            break;
                        }

                        default:
                        {
                            if (form != MCComponentForm::Engine && stat->Hits != 0)
                            {
                                stat->Hits = 0;
                            }
                            break;
                        }
                    }
                }
            }

            GlobalLogPtr->ReIndexInventory();

            if (anyMissing)
            {
                // Ask whether to strip the damaged items that have no replacement (RefitItemCallback).
                Application->Release();
                LeftDrag = 0;
                RefitBlock = this;
                MCRefitDialog* dialog = GlobalLogPtr->RefitDialog;
                dialog->SetText(missing);
                dialog = GlobalLogPtr->RefitDialog;
                dialog->Callback = nullptr;
                dialog->SetTwoButton(-1);
                char okUp[] = "bh_okay.tga";
                char okDown[] = "bg_okay.tga";
                char cancelUp[] = "bh_cancl.tga";
                char cancelDown[] = "bg_cancl.tga";
                GlobalLogPtr->RefitDialog->OkButton->SetUpPicture(okUp);
                GlobalLogPtr->RefitDialog->OkButton->SetDownPicture(okDown);
                GlobalLogPtr->RefitDialog->OkButton->Callback()->SetExec(RefitItemCallback);
                GlobalLogPtr->RefitDialog->CancelButton->SetUpPicture(cancelUp);
                GlobalLogPtr->RefitDialog->CancelButton->SetDownPicture(cancelDown);
                GlobalLogPtr->RefitDialog->Activate();
                PlaySample(0x33);
                DrawBackground(SlotIndex, nullptr);
                return;
            }

            RepairScreen()->CreateCompInvBlock();
            RepairScreen()->SetUpCompInv(0, -1);
            DrawBackground(SlotIndex, nullptr);
            return;
        }

        // Repair the structure: the engine level by level, then internal structure, then armor.
        if (repaired->Deployed != 0)
        {
            UndeployMech();
        }

        if (DragPort != nullptr)
        {
            delete DragPort;
            DragPort = nullptr;
        }

        PlaySample(0x35);
        _PressedButton = 2;
        UpdateDisplay(0, 0, 0, 0, 0);
        MCLogInventoryStat* engine = EngineStat;

        if (engine->Hits != 0)
        {
            int32_t cost = GlobalLogPtr->EngineCost;

            do
            {
                if (ResourcePoints < cost)
                {
                    shortOfPoints = true;
                    goto repaired;
                }

                ResourcePoints = ResourcePoints - cost;
                --engine->Hits;
                Mech->Status = 0;
            } while (engine->Hits != 0);
        }

        {
            MCLogMech* target = Mech;
            int32_t points = SumPoints(target->Internals, 8, true) - SumPoints(target->Internals, 8, false);

            if (ResourcePoints < points * GlobalLogPtr->InternalCost)
            {
                points = ResourcePoints / GlobalLogPtr->InternalCost;
                RepairInternal(points);
                shortOfPoints = true;
            }
            else
            {
                RepairInternal(-1);
            }

            ResourcePoints = ResourcePoints - GlobalLogPtr->InternalCost * points;

            if (!shortOfPoints)
            {
                target = Mech;
                points = SumPoints(target->Armor, 11, true) - SumPoints(target->Armor, 11, false);

                if (ResourcePoints < GlobalLogPtr->ArmorCost * points)
                {
                    points = ResourcePoints / GlobalLogPtr->ArmorCost;
                    RepairArmor(points);
                    shortOfPoints = true;
                }
                else
                {
                    RepairArmor(-1);
                }

                ResourcePoints = ResourcePoints - GlobalLogPtr->ArmorCost * points;
            }
        }
    repaired:
        SetArmorSlider(-1);
        SetInternalSlider(-1);
        SetEngineSlider(-1);
        DrawBackground(SlotIndex, nullptr);

        if (shortOfPoints)
        {
            Application->Release();
            LeftDrag = 0;
            ShowMessage(0x57);
            PlaySample(0x33);
        }

        DrawBackground(SlotIndex, nullptr);
        return;
    }

    PlaySample(0x35);
    LastY = slider;
    DraggingSlider = -1;
    LastX = sliderPos;
    Application->Grab(this);
}

auto MCMechRepairBlock::DrawBackground(int32_t row, MCLogPort* port) -> void
{
    const bool framed =
        GlobalLogPtr->CurrentScreen == GlobalLogPtr->RepairScreen && RepairScreen()->SelectedMech == Mech;
    const bool hasPilot = Mech->PilotIndex >= 0 || Mech->NetworkPilot != nullptr;

    if (port == nullptr)
    {
        // The repair screen's rows are drawn each frame (DrawRow) from the state; the pilot is back in place and no
        // button shows pressed. The rest is what the paint did besides painting (the weapon lists, the battle rating).
        _PilotLifted = false;
        _PressedButton = 0;

        if (hasPilot)
        {
            SetPilotStats(nullptr);
            SetPilotHealth(nullptr);
        }

        DrawButtons(nullptr);
        DrawDamageDiagram(nullptr);
        DrawBR(nullptr);
        DrawArmorSlider(nullptr);
        DrawInternalSlider(nullptr);
        DrawEngineSlider(nullptr);
        SetInventory(nullptr);
        DrawInventory(nullptr);
        return;
    }

    // The briefing box's picture.
    PaintBase(port, 0, row < 0, framed);

    if (hasPilot)
    {
        SetPilotStats(port);
        SetPilotHealth(port);
    }

    DrawButtons(port);
    DrawDamageDiagram(port);
    DrawBR(port);
    DrawArmorSlider(port);
    DrawInternalSlider(port);
    DrawEngineSlider(port);
}

auto MCMechRepairBlock::PaintBase(MCLogPort* port, int32_t top, bool briefing, bool framed) -> void
{
    MCLogPort* rowArt = briefing ? LogArtf("%slogart\\lsbbkm00.tga", ArtPath) : GlobalLogPtr->RepairBackPort;

    if (rowArt == nullptr)
    {
        return;
    }

    // Drawn in place: the original put the base together in a new picture (zeroed, as the port's heap gives it) and
    // copied it as it is.
    MCLogBlockPort back(port->Frame(), 0, top, rowArt->Width(), rowArt->Height(), false);

    if (briefing)
    {
        VfxPaneCopy(rowArt->Frame(), 0, 0, back.Frame(), 0, 0, -1);
    }
    else
    {
        VfxPaneWipe(back.Frame(), 0);
        rowArt->CopyTo(back.Frame(), 0, 0, -1);
    }

    MCLogMech* logMech = Mech;
    const char* chassisArt = nullptr;

    if (logMech->NameVariant == 0)
    {
        chassisArt = "%slogart\\lscflma%02d.tga";
    }
    else if (logMech->NameVariant == 1)
    {
        chassisArt = "%slogart\\lscflmw%02d.tga";
    }
    else if (logMech->NameVariant == 2)
    {
        chassisArt = "%slogart\\lscflmj%02d.tga";
    }

    if (MCLogPort* art = chassisArt != nullptr ? LogArtf(chassisArt, ArtPath, logMech->NameIndex) : nullptr)
    {
        art->CopyTo(back.Frame(), 5, 4, -1);
    }

    if (MCLogPort* art = LogArtf("%slogart\\lscdsm%02d.tga", ArtPath, logMech->NameIndex))
    {
        art->CopyTo(back.Frame(), briefing ? 0xdc : 0xd6, 8, -1);
    }

    char format[256];
    char text[92];
    CLoadString(ThisInstance, 0x4e, format, 0xfe);
    std::snprintf(text, sizeof(text), format, static_cast<double>(logMech->CurTonnage), logMech->WeightClassName);
    WriteText(BlueDropFont, back.Frame(), 6, 0x12, text);

    if (framed)
    {
        // The selected mech's frame.
        DrawLine(back.Frame(), 1, 0, Width() + 1, 0, 0xf2);
        DrawLine(back.Frame(), 1, 0, 1, Height() - 2, 0xf2);
        DrawLine(back.Frame(), 1, Height() - 2, Width() + 1, Height() - 2, 0xf2);
        DrawLine(back.Frame(), Width() + 1, 0, Width() + 1, Height() - 2, 0xf2);
    }
}

auto MCMechRepairBlock::DrawButtons(MCLogPort* port) -> void
{
    bool onRows = port == nullptr;

    // The buttons are live when there is something to repair. (Painting the briefing box, the original found
    // nothing to repair: it only looked for the repair screen's rows.)
    CanRepairItems = onRows && ItemsDamaged() ? 1 : 0;
    CanRepairStructure = onRows && StructureDamaged() ? 1 : 0;

    if (onRows)
    {
        // The rows draw their buttons each frame (DrawRow).
        return;
    }

    PaintButtons(port, 0, false, CanRepairItems, CanRepairStructure);
}

auto MCMechRepairBlock::ItemsDamaged() const -> bool
{
    // Any weapon or equipment copy damaged.
    for (MCLogInventoryItem* item = Mech->Inventory->Items; item != nullptr; item = item->Next)
    {
        MCComponentForm form = ComponentForm(item->MasterID);

        if (!IsWeapon(form) && !IsEquipment(form) && form != MCComponentForm::Jammer)
        {
            continue;
        }

        for (MCLogInventoryStat* stat = item->Stats; stat != nullptr; stat = stat->Next)
        {
            if (stat->Hits != 0)
            {
                return true;
            }
        }
    }

    return false;
}

auto MCMechRepairBlock::StructureDamaged() const -> bool
{
    // The engine, the internal structure or the armor damaged.
    return EngineStat->Hits != 0 || SumPoints(Mech->Internals, 8, false) != SumPoints(Mech->Internals, 8, true) ||
           SumPoints(Mech->Armor, 11, false) != SumPoints(Mech->Armor, 11, true);
}

auto MCMechRepairBlock::ShowsInventory() const -> bool
{
    // In multiplayer, only the player's own mechs (and the one in the briefing box) list their weapons.
    return MPlayer == nullptr || GlobalLogPtr->ForceMechList->GetMechIndex(Mech) >= 0 ||
           Mech->BriefingBox == GlobalLogPtr->BriefingScreen->BriefingBox;
}

auto MCMechRepairBlock::PaintButtons(MCLogPort* port, int32_t top, bool onRows, int32_t items, int32_t structure)
    -> void
{
    // Drawn in place: the original painted a scratch picture (wiped to 0xff) and copied it here keyed on 0xff.
    MCLogBlockPort work(port->Frame(), 0, top, Width(), Height(), true);

    if (items == 0)
    {
        GlobalLogPtr->RepairPorts[4]->CopyTo(work.Frame(), onRows ? 0xea : 0xf8, 3, -1);
    }
    else
    {
        GlobalLogPtr->RepairPorts[1]->CopyTo(work.Frame(), 0xea, 3, -1);
    }

    if (structure == 0)
    {
        GlobalLogPtr->RepairPorts[5]->CopyTo(work.Frame(), onRows ? 0xea : 0xf8, 0x17, -1);
    }
    else
    {
        GlobalLogPtr->RepairPorts[3]->CopyTo(work.Frame(), 0xea, 0x17, -1);
    }
}

auto MCMechRepairBlock::DrawDamageDiagram(MCLogPort* port) -> void
{
    if (port != nullptr)
    {
        PaintDiagram(port, 0, 0x8e);
    }
}

auto MCMechRepairBlock::PaintDiagram(MCLogPort* port, int32_t top, int32_t xPos) -> void
{
    MCLogBlockPort blank(port->Frame(), xPos, top + 8, 0x4b, 100, true);
    VfxPaneWipe(blank.Frame(), 0x10);

    // The internal structure (shape frames 11..18), then the armor over it (0..7), coloured by damage state.
    int32_t state[8];

    for (int32_t location = 0; location < 8; ++location)
    {
        state[location] =
            DamageState(PercentLeft(Mech->Internals[location].CurArmor, Mech->Internals[location].MaxArmor));
    }

    for (int32_t shade = 0; shade < 5; ++shade)
    {
        VfxShapeLookaside(InternalLookaside(shade));

        for (int32_t location = 0; location < 8; ++location)
        {
            if (state[location] == shade)
            {
                VfxShapeTranslateDraw(port->Frame(), GlobalLogPtr->MechRepShapes[Mech->NameIndex], location + 0xb, xPos,
                                      top + 8);
            }
        }
    }

    for (int32_t location = 0; location < 8; ++location)
    {
        state[location] = DamageState(PercentLeft(Mech->Armor[location].CurArmor, Mech->Armor[location].MaxArmor));
    }

    for (int32_t shade = 0; shade < 5; ++shade)
    {
        VfxShapeLookaside(ArmorLookaside(shade));

        for (int32_t location = 0; location < 8; ++location)
        {
            if (state[location] == shade)
            {
                VfxShapeTranslateDraw(port->Frame(), GlobalLogPtr->MechRepShapes[Mech->NameIndex], location, xPos,
                                      top + 8);
            }
        }
    }
}

auto MCMechRepairBlock::DrawBR(MCLogPort* port) -> void
{
    Mech->CalcBR();

    if (port != nullptr)
    {
        PaintBR(port, 0);
    }
}

auto MCMechRepairBlock::PaintBR(MCLogPort* port, int32_t top) -> void
{
    // Drawn in place: the original painted a scratch picture (wiped to 0xff) and copied it here keyed on 0xff.
    MCLogBlockPort work(port->Frame(), 0, top, Width(), Height(), true);

    // The battle rating bar: 80 pixels at 18010, rising from y 0x58; the pilot modifier adds to it (or eats into it).
    int32_t rating = Mech->BattleRating;
    int32_t clamped = static_cast<int32_t>(rating > 18010.0 ? 18010.0 : static_cast<double>(rating));
    int32_t bar = static_cast<int32_t>(static_cast<double>(clamped) * (1.0 / 18010.0) * 80.0);

    if (MCLogPort* art = LogArtf("%slogart\\lsrupm09.tga", ArtPath))
    {
        art->CopyTo(work.Frame(), 0x72, 5, 0);
    }

    MCPane* pane = work.Frame();
    int32_t barTop = 0x57 - bar;
    DrawLine(pane, 0x74, 0x58, 0x78, 0x58, 0xe5);
    DrawLine(pane, 0x74, barTop, 0x78, barTop, 0xe3);
    int32_t top0 = 0x58 - bar;
    DrawLine(pane, 0x73, 0x57, 0x73, top0, 0xe3);
    DrawLine(pane, 0x79, 0x57, 0x79, top0, 0xe5);

    for (int32_t x = 0x74; x <= 0x78; ++x)
    {
        DrawLine(pane, x, 0x57, x, top0, 0xe4);
    }

    DrawLine(pane, 0x74, 0x56 - bar, 0x78, 0x56 - bar, 0x10);
    VfxPixelWrite(pane, 0x73, barTop, 0x10);
    VfxPixelWrite(pane, 0x79, barTop, 0x10);

    int32_t modifier = Mech->PilotModifier;

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
            DrawLine(pane, 0x73, barTop, 0x73, extensionTop, 0xec);

            for (int32_t x = 0x74; x <= 0x78; ++x)
            {
                DrawLine(pane, x, barTop, x, extensionTop, 0xc);
            }

            DrawLine(pane, 0x79, barTop, 0x79, extensionTop, 0x54);
            DrawLine(pane, 0x74, extensionTop - 1, 0x78, extensionTop - 1, 0xec);
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
            DrawLine(pane, 0x74, barTop, 0x78, barTop, 0xae);
            int32_t bottom = taken - bar + 0x58;
            DrawLine(pane, 0x73, top0, 0x73, bottom, 0xae);

            for (int32_t x = 0x74; x <= 0x78; ++x)
            {
                DrawLine(pane, x, top0, x, bottom, 0xce);
            }

            DrawLine(pane, 0x79, top0, 0x79, bottom, 0xed);
        }
    }
}

auto MCMechRepairBlock::DrawArmorSlider(MCLogPort* port) -> void
{
    DrawSlider(port, 0);
}

auto MCMechRepairBlock::DrawSlider(MCLogPort* port, int32_t slider) -> void
{
    if (port != nullptr)
    {
        PaintSlider(port, 0, slider, true);
    }
}

auto MCMechRepairBlock::PaintSlider(MCLogPort* port, int32_t top, int32_t slider, bool briefing) -> void
{
    int32_t start = 0;
    int32_t position = 0;
    int32_t yPos = 0;

    if (slider == 0)
    {
        start = ArmorSliderStart;
        position = ArmorSliderPos;
        yPos = 0x37;
    }
    else if (slider == 1)
    {
        start = InternalSliderStart;
        position = InternalSliderPos;
        yPos = 0x4c;
    }
    else if (slider == 2)
    {
        start = EngineSliderStart;
        position = EngineSliderPos;
        yPos = 0x61;
    }

    // Drawn in place: the original painted a scratch picture (wiped to 0xff) and copied it here keyed on 0xff.
    MCLogBlockPort work(port->Frame(), 0, top, Width(), Height(), true);

    if (MCLogPort* art = LogArtf("%slogart\\lsrupm%d.tga", ArtPath, slider + 0xb))
    {
        art->CopyTo(work.Frame(), briefing ? 0xf8 : 0xea, yPos, -1);
    }

    // The track left of the knob: the part repaired before (grey), then the part repaired by this drag (red).
    int32_t shift = briefing ? 0xe : 0;
    MCPane* pane = work.Frame();

    if (0xec < position)
    {
        int32_t left = shift + 0xec;
        int32_t startX = start + shift;

        if (left < startX - 1)
        {
            DrawLine(pane, left, yPos + 1, startX - 1, yPos + 1, 0xe3);

            for (int32_t y = yPos + 2; y <= yPos + 6; ++y)
            {
                DrawLine(pane, left, y, startX - 1, y, 0xe4);
            }

            DrawLine(pane, left, yPos + 7, startX - 1, yPos + 7, 0xe5);
            DrawLine(pane, shift + 0xeb, yPos + 2, shift + 0xeb, yPos + 6, 0xe3);
        }

        if (start < position)
        {
            int32_t right = shift - 1 + position;
            int32_t from = startX < 0xec ? 0xec : startX;
            DrawLine(pane, from, yPos + 1, right, yPos + 1, 0xec);

            for (int32_t y = yPos + 2; y <= yPos + 6; ++y)
            {
                DrawLine(pane, from, y, right, y, 0xc);
            }

            DrawLine(pane, from, yPos + 7, right, yPos + 7, 0x87);
            DrawLine(pane, from - 1, yPos + 2, from - 1, yPos + 6, 0xec);
        }
    }

    VfxPaneCopy(SliderArtPort->Frame(), 0, 0, pane, shift + position, yPos, -1);
}

auto MCMechRepairBlock::DrawInternalSlider(MCLogPort* port) -> void
{
    DrawSlider(port, 1);
}

auto MCMechRepairBlock::DrawEngineSlider(MCLogPort* port) -> void
{
    DrawSlider(port, 2);
}

auto MCMechRepairBlock::Draw() -> void
{
    if (RepairScreen()->SelectedMech == Mech)
    {
        DrawInventory(nullptr);
    }
}

auto MCMechRepairBlock::DrawInventory(MCLogPort* port) -> void
{
    if (port != nullptr && Mech->Assigned != 0)
    {
        PaintInventory(port, 0);
    }
}

auto MCMechRepairBlock::PaintInventory(MCLogPort* port, int32_t top) -> void
{
    MCLogBlockPort work(port->Frame(), 0x135, top + 0x11, 0x62, 0x58, false);
    auto* pane = static_cast<MCScrollPane*>(Child(0));
    pane->DrawContentTo(work.Frame(), 0, 0);
    pane->DrawSliderColumn(port->Frame(), pane->Width() + 0x128, top + 0x11, false);
}

auto MCMechRepairBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    // Put together as the original painted it into the rows' picture (over its colour 0xff): framed when selected,
    // darkened when another mech is.
    MCLogBlockPort row(port->Frame(), 0, top, 0x19d, 0x70, false);
    VfxPaneWipe(row.Frame(), 0xff);
    const bool selected = RepairScreen()->SelectedMech == Mech;
    PaintBase(&row, 0, false, selected);
    MCLogMech* logMech = Mech;
    const float status = logMech->CalcStatus();

    if (logMech->PilotIndex >= 0 || logMech->NetworkPilot != nullptr)
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

    if (logMech->Assigned != 0)
    {
        PaintInventory(&row, 0);
    }

    if (!selected)
    {
        GlobalLogPtr->Darken(0, LogisticFadetable, &row);
    }

    if (_PilotLifted)
    {
        if (MCLogPort* blank = LogArtf("%slogart\\lsrupm10.tga", ArtPath))
        {
            blank->CopyTo(port->Frame(), 6, top + 0x21, -1);
        }
    }

    // A repair button held while its repair runs.
    if (_PressedButton != 0)
    {
        const bool items = _PressedButton == 1;
        GlobalLogPtr->RepairPorts[items ? 0 : 2]->CopyTo(port->Frame(), 0xea, top + (items ? 3 : 0x17), -1);
    }
}

auto MCMechRepairBlock::OnBeginDragPilot(MCLogPort* surface) -> void
{
    VfxPaneWipe(surface->Frame(), 0xff);
    MCDragIcon::DrawFrom(surface, 5, 0x25, [this](MCLogPort* port) { DrawRow(port, 0); });
}

auto MCMechRepairBlock::OnBeginDragMech(MCLogPort* surface) -> void
{
    VfxPaneWipe(surface->Frame(), 0x10);

    for (int32_t location = 0; location < 8; ++location)
    {
        GlobalLogPtr->DrawMechBodyLoc(Mech, location, surface, 2, 1);
    }
}

auto MCMechRepairBlock::OnBeginDragItem(MCLogPort* surface, MCLogInventoryItem* item) -> void
{
    if (MCLogPort* art = LogArtf("%slogart\\lscicc%02d.tga", ArtPath, item->RangeIndex))
    {
        art->CopyTo(surface->Frame(), 1, 1, -1);
    }
}

auto MCMechRepairBlock::MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) -> bool
{
    auto* pane = static_cast<MCScrollPane*>(Child(0));

    if (!OverPane(pane, xPos, yPos) || !pane->MouseWheel(steps, xPos, yPos))
    {
        return false;
    }

    // As a click on the list's scroll bar.
    DrawInventory(nullptr);
    return true;
}

auto MCLogMech::CalcStatus() -> float
{
    StatusValue = 0.0f;

    if (PilotIndex < 0 && NetworkPilot == nullptr)
    {
        return 0.0f;
    }

    MCLogInventoryItem* item = Inventory->Items;
    float pilotFactor = 0.0f;
    MCLogWarrior* warrior = nullptr;

    if (LocalPart == 0)
    {
        warrior = NetworkPilot;
    }
    else
    {
        GlobalLogPtr->AssignedWarriorList->GetWarriorInfo(PilotIndex, warrior);
    }

    if (warrior != nullptr)
    {
        pilotFactor = static_cast<float>(static_cast<double>(warrior->Skills[3]) * 0.02);
    }

    // The firepower left: the undamaged weapons' worth over all weapons' worth (0/0 without a pilot).
    double working = 0.0;
    double total = 0.0;

    for (; item != nullptr; item = item->Next)
    {
        const MCMasterComponent& component = MasterComponentList[item->MasterID];

        if (!IsWeapon(component.Form) || item->Stats == nullptr)
        {
            continue;
        }

        int16_t value = WeaponValue(component);

        for (MCLogInventoryStat* stat = item->Stats; stat != nullptr; stat = stat->Next)
        {
            if (stat->Hits == 0)
            {
                working += static_cast<double>(value) * pilotFactor;
            }

            total += static_cast<double>(value) * pilotFactor;
        }
    }

    auto firepower = static_cast<float>(working / total);

    if (std::isnan(firepower) || firepower == 0.0f)
    {
        return StatusValue;
    }

    // The body: the head's armor, the weaker of two torso locations, the legs' and the side torsos' armor.
    auto head = static_cast<float>(static_cast<double>(Armor[0].CurArmor) / Armor[0].MaxArmor * 0.6 + 0.4);
    double weakCurrent = Armor[1].CurArmor;
    double weakMaximum = Armor[1].MaxArmor;

    if (static_cast<double>(Armor[8].CurArmor) < weakCurrent)
    {
        weakCurrent = Armor[8].CurArmor;
        weakMaximum = Armor[8].MaxArmor;
    }

    auto legs = static_cast<float>(static_cast<double>(Armor[5].CurArmor + Armor[4].CurArmor) /
                                       (Armor[5].MaxArmor + Armor[4].MaxArmor) * 0.25 +
                                   0.75);
    double sides = static_cast<double>(Armor[9].CurArmor + Armor[10].CurArmor + Armor[3].CurArmor + Armor[2].CurArmor) /
                       (Armor[10].MaxArmor + Armor[9].MaxArmor + Armor[3].MaxArmor + Armor[2].MaxArmor) * 0.25 +
                   0.75;
    double body = sides * ((weakCurrent / weakMaximum + 1.0) * 0.5) * legs * legs * head;

    if (std::isnan(body) || body == 0.0)
    {
        return StatusValue;
    }

    // The pilot's wounds.
    static constexpr float woundFactors[7] = {1.0f, 0.95f, 0.85f, 0.75f, 0.5f, 0.3f, 0.0f};
    float woundFactor = head;

    if (warrior != nullptr)
    {
        auto wounds = static_cast<int32_t>(warrior->Wounds);
        // Port fix: wounds past the table read 0 (the original read the stack beyond it).
        woundFactor = wounds >= 0 && wounds < 7 ? woundFactors[wounds] : 0.0f;
    }

    StatusValue = static_cast<float>(static_cast<double>(woundFactor) * body * firepower);
    return StatusValue;
}

auto MCMechRepairBlock::DrawStatusBar(MCLogPort* port) -> void
{
    float status = Mech->CalcStatus();
    bool repairLayout = GlobalLogPtr->CurrentScreen != GlobalLogPtr->BriefingScreen;

    // The rows draw their status bar each frame (DrawRow).
    if (port == nullptr)
    {
        return;
    }

    PaintStatusBar(port, 0, status, repairLayout);
}

auto MCMechRepairBlock::PaintStatusBar(MCLogPort* port, int32_t top, float status, bool repairLayout) -> void
{
    // Drawn in place: the original painted a scratch picture (wiped to 0xff) and copied it here keyed on 0xff.
    MCLogBlockPort work(port->Frame(), 0, top, Width(), Height(), true);
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
        DrawLine(work.Frame(), left, y, right, y, 0x13);
    }

    if (status != 0.0f)
    {
        auto end = static_cast<int32_t>(static_cast<double>(status) * 74.0 + (repairLayout ? 136.0 : 142.0));

        for (int32_t y = 2; y <= 5; ++y)
        {
            DrawLine(work.Frame(), left, y, end, y, color);
        }
    }
}

auto MCMechRepairBlock::SetEngineSlider(int32_t value) -> void
{
    if (value >= 0)
    {
        return;
    }

    MCLogInventoryItem* item = Mech->Inventory->GetItemInfo(0);

    while (ComponentForm(item->MasterID) != MCComponentForm::Engine)
    {
        item = item->Next;
    }

    MCLogInventoryStat* engine = item->Stats;

    if (engine->Hits > 3)
    {
        engine->Hits = 3;
    }

    switch (engine->Hits)
    {
        case 0:
            EngineSliderPos = 0x127;
            break;
        case 1:
            EngineSliderPos = 0x113;
            break;
        case 2:
            EngineSliderPos = 0xff;
            break;
        case 3:
            EngineSliderPos = 0xeb;
            break;
        default:
            break;
    }
}

auto MCMechRepairBlock::SetInternalSlider(int32_t value) -> void
{
    if (value >= 0)
    {
        return;
    }

    int32_t current = SumPoints(Mech->Internals, 8, false);
    int32_t maximum = SumPoints(Mech->Internals, 8, true);
    InternalSliderPos =
        0xea - static_cast<int32_t>(static_cast<double>(current) / maximum * static_cast<double>(-61.0f));
}

auto MCMechRepairBlock::SetArmorSlider(int32_t value) -> void
{
    if (value >= 0)
    {
        return;
    }

    int32_t current = SumPoints(Mech->Armor, 11, false);
    int32_t maximum = SumPoints(Mech->Armor, 11, true);
    ArmorSliderPos = 0xea - static_cast<int32_t>(static_cast<double>(current) / maximum * static_cast<double>(-61.0f));
}

auto MCMechRepairBlock::ClearPilot() -> void
{
    _PilotLifted = true;
}

auto MCMechRepairBlock::SetPilotStats(MCLogPort* port) -> void
{
    MCLogMech* logMech = Mech;

    if (logMech->PilotIndex < 0 && logMech->NetworkPilot == nullptr)
    {
        return;
    }

    float status = Mech->CalcStatus();
    bool repairLayout = GlobalLogPtr->CurrentScreen != GlobalLogPtr->BriefingScreen;

    // The rows draw their pilot each frame (DrawRow).
    if (port == nullptr)
    {
        return;
    }

    PaintPilot(port, 0, status, repairLayout);
}

auto MCMechRepairBlock::PaintPilot(MCLogPort* port, int32_t top, float status, bool repairLayout) -> void
{
    MCLogMech* logMech = Mech;
    MCLogWarrior* warrior = nullptr;

    if (logMech->LocalPart == 0)
    {
        warrior = logMech->NetworkPilot;
    }
    else
    {
        GlobalLogPtr->AssignedWarriorList->GetWarriorInfo(logMech->PilotIndex, warrior);
    }

    // The status bar, then the portrait.
    PaintStatusBar(port, top, status, repairLayout);
    MCLogPort* portrait = warrior == nullptr ? LogArtf("%slogart\\pilot%02d.tga", ArtPath, logMech->PilotIndex)
                                             : LogArtf("%slogart\\%s", ArtPath, warrior->Picture);

    if (portrait != nullptr)
    {
        portrait->CopyTo(port->Frame(), 6, top + 0x26, -1);
    }

    // Port fix: without a pilot record the texts are skipped (the original read them through the null pointer).
    if (warrior == nullptr)
    {
        return;
    }

    char text[256];
    WriteText(YellowDropFont, port->Frame(), 0x2d, top + 0x2a, warrior->Callsign);

    // An out-of-range rank shows the portrait's file name, which the text buffer last held.
    std::snprintf(text, sizeof(text), "%slogart\\%s", ArtPath, warrior->Picture);

    if (warrior->Rank >= 0 && warrior->Rank <= 3)
    {
        CLoadString(ThisInstance, 0x70 + static_cast<uint32_t>(warrior->Rank), text, 0xfe);
    }

    WriteText(YellowDropFont, port->Frame(), 0x2d, top + 0x3c, text);
    GlobalLogPtr->DrawPilotSkillBar(warrior, 3, 0x2e, top + 0x4a, 0, 0x36, WinHeight, port);
    GlobalLogPtr->DrawPilotSkillBar(warrior, 0, 0x2e, top + 0x53, 0, 0x36, WinHeight, port);
    GlobalLogPtr->DrawPilotSkillBar(warrior, 1, 0x2e, top + 0x5c, 0, 0x36, WinHeight, port);
    GlobalLogPtr->DrawPilotSkillBar(warrior, 2, 0x2e, top + 0x65, 0, 0x36, WinHeight, port);

    // One 2x2 pip per point of health.
    auto pips = static_cast<int32_t>(warrior->Health);
    int32_t xPos = 0xc;

    for (; pips > 0; --pips)
    {
        AGPixelWrite(port->Frame(), xPos, top + 0x22, 0xcf);
        AGPixelWrite(port->Frame(), xPos + 1, top + 0x22, 0xcf);
        AGPixelWrite(port->Frame(), xPos + 1, top + 0x23, 0xee);
        AGPixelWrite(port->Frame(), xPos, top + 0x23, 0xcf);
        xPos += 3;
    }
}

auto MCLogistics::DrawPilotSkillBar(MCLogWarrior* warrior, int32_t skill, int32_t xPos, int32_t yPos, int32_t row,
                                    int32_t width, int32_t rowHeight, MCLogPort* port) -> void
{
    DrawPilotSkillBar(warrior->Skills[skill], xPos, yPos, row, width, rowHeight, port);
}

auto MCLogistics::DrawPilotSkillBar(int32_t value, int32_t xPos, int32_t yPos, int32_t row, int32_t width,
                                    int32_t rowHeight, MCLogPort* port) -> void
{
    MCPane* pane = port->Frame();
    int32_t top = row * rowHeight + yPos;
    int32_t right = xPos + width;
    // The empty bar.
    DrawLine(pane, xPos, top, right - 1, top, 0x32);
    DrawLine(pane, xPos, top + 1, xPos, top + 2, 0x32);
    DrawLine(pane, xPos + 1, top + 3, right - 1, top + 3, 0x14);
    DrawLine(pane, right, top + 1, right, top + 2, 0x14);
    DrawLine(pane, xPos + 1, top + 1, right - 1, top + 1, 0x33);
    DrawLine(pane, xPos + 1, top + 2, right - 1, top + 2, 0x33);

    if (value == 0)
    {
        return;
    }

    // The filled part.
    double scale =
        static_cast<double>(width) / (static_cast<double>(MaxPilotSkill) - static_cast<double>(MinPilotSkill));
    int32_t end =
        static_cast<int32_t>((static_cast<double>(value) - static_cast<double>(MinPilotSkill)) * scale) + xPos;
    DrawLine(pane, xPos + 1, top, end - 1, top, 0xe3);
    DrawLine(pane, xPos, top + 1, xPos, top + 2, 0xe3);
    DrawLine(pane, xPos + 1, top + 3, end - 1, top + 3, 0xe5);
    DrawLine(pane, end, top + 1, end, top + 2, 0xe5);
    DrawLine(pane, xPos + 1, top + 1, end - 1, top + 1, 0xe4);
    DrawLine(pane, xPos + 1, top + 2, end - 1, top + 2, 0xe4);
    DrawLine(pane, end + 1, top + 1, end + 1, top + 2, 0x10);
    VfxPixelWrite(pane, end, top, 0x10);
    VfxPixelWrite(pane, end, top + 3, 0x10);
}

auto MCMechRepairBlock::SetPilotHealth(MCLogPort*) -> void
{
    // Drawn with the pilot's stats (PaintPilot).
}

auto MCMechRepairBlock::SetMechStats() -> void
{
}

auto MCMechRepairBlock::SetWeaponLists() -> void
{
    if (ShortRangeWeapons != nullptr)
    {
        LogFree(ShortRangeWeapons);
        NumShortRangeWeapons = 0;
        ShortRangeWeapons = nullptr;
    }

    if (MediumRangeWeapons != nullptr)
    {
        LogFree(MediumRangeWeapons);
        NumMediumRangeWeapons = 0;
        MediumRangeWeapons = nullptr;
    }

    if (LongRangeWeapons != nullptr)
    {
        LogFree(LongRangeWeapons);
        NumLongRangeWeapons = 0;
        LongRangeWeapons = nullptr;
    }

    if (Equipment != nullptr)
    {
        LogFree(Equipment);
        NumEquipment = 0;
        Equipment = nullptr;
    }

    if (ItemHits != nullptr)
    {
        LogFree(ItemHits);
        NumItems = 0;
        ItemHits = nullptr;
    }

    // Weapons go by long range: under 76 short, under 151 medium, else long.
    MCLogInventoryItem* items = Mech->Inventory->Items;

    for (MCLogInventoryItem* item = items; item != nullptr; item = item->Next)
    {
        const MCMasterComponent& component = MasterComponentList[item->MasterID];

        if (IsWeapon(component.Form))
        {
            if (component.WeaponRange[3] < 76.0f)
            {
                NumShortRangeWeapons += item->Count;
            }
            else if (component.WeaponRange[3] < 151.0f)
            {
                NumMediumRangeWeapons += item->Count;
            }
            else
            {
                NumLongRangeWeapons += item->Count;
            }
        }
        else if (IsEquipment(component.Form))
        {
            NumEquipment += item->Count;
        }
    }

    if (NumShortRangeWeapons != 0)
    {
        ShortRangeWeapons = static_cast<int32_t*>(LogAlloc(NumShortRangeWeapons * 4));
    }

    if (NumMediumRangeWeapons != 0)
    {
        MediumRangeWeapons = static_cast<int32_t*>(LogAlloc(NumMediumRangeWeapons * 4));
    }

    if (NumLongRangeWeapons != 0)
    {
        LongRangeWeapons = static_cast<int32_t*>(LogAlloc(NumLongRangeWeapons * 4));
    }

    if (NumEquipment != 0)
    {
        Equipment = static_cast<int32_t*>(LogAlloc(NumEquipment * 4));
    }

    NumItems = NumLongRangeWeapons + NumMediumRangeWeapons + NumShortRangeWeapons + NumEquipment;

    if (NumItems != 0)
    {
        ItemHits = static_cast<int32_t*>(LogAlloc(NumItems * 4));
    }

    // One entry per copy: the item's index in the inventory, and the copy's damage in itemHits.
    int32_t shortCount = 0;
    int32_t mediumCount = 0;
    int32_t longCount = 0;
    int32_t equipmentCount = 0;
    int32_t index = 0;

    for (MCLogInventoryItem* item = items; item != nullptr; item = item->Next, ++index)
    {
        const MCMasterComponent& component = MasterComponentList[item->MasterID];
        MCLogInventoryStat* stat = item->Stats;

        if (IsWeapon(component.Form))
        {
            if (component.WeaponRange[3] < 76.0f)
            {
                for (int32_t copy = 0; copy < item->Count; ++copy)
                {
                    ShortRangeWeapons[shortCount] = index;
                    ItemHits[shortCount] = stat->Hits;
                    ++shortCount;
                    stat = stat->Next;
                }
            }
            else if (component.WeaponRange[3] < 151.0f)
            {
                for (int32_t copy = 0; copy < item->Count; ++copy)
                {
                    MediumRangeWeapons[mediumCount] = index;
                    ItemHits[NumShortRangeWeapons + mediumCount] = stat->Hits;
                    ++mediumCount;
                    stat = stat->Next;
                }
            }
            else
            {
                for (int32_t copy = 0; copy < item->Count; ++copy)
                {
                    LongRangeWeapons[longCount] = index;
                    ItemHits[NumShortRangeWeapons + NumMediumRangeWeapons + longCount] = stat->Hits;
                    ++longCount;
                    stat = stat->Next;
                }
            }
        }
        else if (IsEquipment(component.Form))
        {
            for (int32_t copy = 0; copy < item->Count; ++copy)
            {
                Equipment[equipmentCount] = index;
                ItemHits[NumLongRangeWeapons + NumShortRangeWeapons + NumMediumRangeWeapons + equipmentCount] =
                    stat->Hits;
                ++equipmentCount;
                stat = stat->Next;
            }
        }
    }

    SortWeaponList(ShortRangeWeapons, NumShortRangeWeapons);
    SortWeaponList(MediumRangeWeapons, NumMediumRangeWeapons);
    SortWeaponList(LongRangeWeapons, NumLongRangeWeapons);
}

auto MCMechRepairBlock::SortWeaponList(int32_t* list, int32_t count) -> void
{
    int32_t base = 0;

    if (list == MediumRangeWeapons)
    {
        base = NumShortRangeWeapons;
    }
    else if (list == LongRangeWeapons)
    {
        base = NumMediumRangeWeapons + NumShortRangeWeapons;
    }

    // A selection sort by damage, lowest first; the hits follow their entries.
    MCInventoryList* inventory = Mech->Inventory;

    for (int32_t i = 0; i < count - 1; ++i)
    {
        for (int32_t j = i + 1; j < count; ++j)
        {
            int32_t first = list[i];
            float firstDamage = MasterComponentList[inventory->GetMasterIDFromIndex(first)].Damage;
            int32_t second = list[j];

            if (MasterComponentList[inventory->GetMasterIDFromIndex(second)].Damage < firstDamage)
            {
                list[i] = second;
                list[j] = first;
                std::swap(ItemHits[base + i], ItemHits[base + j]);
            }
        }
    }
}

auto MCMechRepairBlock::GetInvItem(int32_t* list, int32_t index, uint8_t* itemNum) -> MCLogInventoryItem*
{
    int32_t earlier = index - 1;

    while (earlier >= 0 && list[earlier] == list[index])
    {
        --earlier;
    }

    int32_t copy = index - earlier - 1;
    MCLogInventoryItem* item = Mech->Inventory->GetItemInfo(list[index]);
    MCLogInventoryStat* stat = item->Stats;

    for (; copy > 0; --copy)
    {
        stat = stat->Next;
    }

    *itemNum = static_cast<uint8_t>(stat->ItemNum);
    return item;
}

auto MCMechRepairBlock::RepairArmor(int32_t sliderPos) -> void
{
    MCLogMech* logMech = Mech;

    if (sliderPos < 0)
    {
        for (auto& location : logMech->Armor)
        {
            location.CurArmor = location.MaxArmor;
        }

        return;
    }

    // The head first, then point by point to the most damaged location (the cockpit, center torso and rear
    // locations count as more damaged than they are).
    int32_t headMissing = logMech->Armor[0].MaxArmor - logMech->Armor[0].CurArmor;

    if (headMissing != 0)
    {
        logMech->Armor[0].CurArmor += static_cast<uint8_t>(sliderPos < headMissing ? sliderPos : headMissing);
        sliderPos -= headMissing;
    }

    if (sliderPos <= 0)
    {
        return;
    }

    auto weighted = [&](int32_t location) -> float
    {
        auto ratio = static_cast<float>(static_cast<double>(logMech->Armor[location].CurArmor) /
                                        logMech->Armor[location].MaxArmor);
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

        uint8_t current = ++logMech->Armor[chosen].CurArmor;
        uint8_t maximum = logMech->Armor[chosen].MaxArmor;

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

auto MCMechRepairBlock::RepairInternal(int32_t sliderPos) -> void
{
    MCLogMech* logMech = Mech;

    if (sliderPos < 0)
    {
        for (auto& location : logMech->Internals)
        {
            location.CurArmor = location.MaxArmor;
        }

        return;
    }

    // Point by point to the most damaged location (weighted like the armor).
    static constexpr double discountByLocation[8] = {0.3, 0.5, 0.2, 0.2, 0.0, 0.0, 0.4, 0.4};
    auto discount = [](float ratio, double factor) -> float
    { return static_cast<float>(static_cast<double>(ratio) - static_cast<double>(ratio) * factor); };
    float ratios[8];

    for (int32_t location = 0; location < 8; ++location)
    {
        ratios[location] = static_cast<float>(static_cast<double>(logMech->Internals[location].CurArmor) /
                                              logMech->Internals[location].MaxArmor);

        if (discountByLocation[location] != 0.0 && ratios[location] < 1.0f)
        {
            ratios[location] = discount(ratios[location], discountByLocation[location]);
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

        uint8_t current = ++logMech->Internals[chosen].CurArmor;
        uint8_t maximum = logMech->Internals[chosen].MaxArmor;

        if (current < maximum)
        {
            auto ratio = static_cast<float>(static_cast<double>(current) / maximum);
            ratios[chosen] = discountByLocation[chosen] != 0.0 ? discount(ratio, discountByLocation[chosen]) : ratio;
        }
        else
        {
            ratios[chosen] = 1.0f;
        }
    }
}

auto MCMechRepairBlock::SetInventory(MCScrollPane* pane) -> void
{
    if (MPlayer != nullptr)
    {
        MCLogMech* logMech = Mech;

        if (GlobalLogPtr->ForceMechList->GetMechIndex(logMech) < 0 &&
            logMech->BriefingBox != GlobalLogPtr->BriefingScreen->BriefingBox)
        {
            return;
        }
    }

    if (pane == nullptr)
    {
        pane = InventoryPane;
    }

    auto* content = new MCLogPort;
    int32_t lines = 0;

    for (MCLogInventoryItem* item = Mech->Inventory->Items; item != nullptr; item = item->Next)
    {
        MCComponentForm form = ComponentForm(item->MasterID);

        if (IsWeapon(form) || IsEquipment(form))
        {
            lines += item->Count;
        }
    }

    int32_t lineHeight = GreenFont->Height() + 2;
    int32_t contentHeight = lineHeight * (lines + 4) + 4;

    if (contentHeight < pane->Height())
    {
        contentHeight = pane->Height();
    }

    // Port: the list is drawn into the pane each frame (DrawWeaponList) from the lists made here.
    content->InitView(pane->Width() - 0xd, contentHeight);
    SetWeaponLists();
    content->DrawContent = [this](MCGuiPort* view) { DrawWeaponList(static_cast<MCLogPort*>(view)); };
    pane->SetDisplayPort(content, -1, 0);

    // The tonnage bar (the weapons' weight against the free weight): the rows draw theirs each frame; the original
    // also painted one into the briefing screen's picture for the box, which the box (drawn after) covered.
}

auto MCMechRepairBlock::PaintTonnage(MCLogPort* port, int32_t top) -> void
{
    // Drawn in place: the original painted a scratch picture (wiped to 0xff) and copied it here keyed on 0xff.
    MCLogBlockPort work(port->Frame(), 0, top, Width(), Height(), true);
    int32_t fill = static_cast<int32_t>(static_cast<double>(Mech->WeaponTonnage) / Mech->FreeTonnage * 53.0);
    DrawTonnageBar(work.Frame(), 0x15e, fill);
}

auto MCMechRepairBlock::DrawWeaponList(MCLogPort* content) -> void
{
    MCPane* frame = content->Frame();
    int32_t lineHeight = GreenFont->Height() + 2;
    VfxPaneWipe(frame, 0x10);

    // The four headings: a coloured stripe (the first three with a black line under it) and the title.
    auto heading = [&](int32_t top, uint8_t color, bool underline)
    {
        int32_t y = top;

        for (; y < GreenFont->Height() + 2 + top; ++y)
        {
            DrawLine(frame, 0, y, content->Width() - 2, y, color);
        }

        if (underline)
        {
            DrawLine(frame, 0, y, content->Width() - 2, y, 0x10);
        }
    };

    char text[256];
    heading(0, 0xe, true);
    CLoadString(ThisInstance, 0x55, text, 0xfe);
    WriteText(WhiteFont, frame, 1, 2, text);
    int32_t before = NumShortRangeWeapons + 1;
    heading(lineHeight * before + 1, 0xe5, true);
    CLoadString(ThisInstance, 0x50, text, 0xfe);
    WriteText(WhiteFont, frame, 1, lineHeight * before + 3, text);
    int32_t shortAndMedium = NumMediumRangeWeapons + NumShortRangeWeapons;
    heading(lineHeight * (shortAndMedium + 2) + 2, 0xee, true);
    CLoadString(ThisInstance, 0x6d, text, 0xfe);
    WriteText(WhiteFont, frame, 1, lineHeight * (shortAndMedium + 2) + 4, text);
    int32_t weapons = NumLongRangeWeapons + NumMediumRangeWeapons + NumShortRangeWeapons;
    heading(lineHeight * (weapons + 3) + 3, 0x14, false);
    CLoadString(ThisInstance, 0x6f, text, 0xfe);
    WriteText(WhiteFont, frame, 1, lineHeight * (weapons + 3) + 5, text);

    // The entries: a range glyph (clan technology has its own) and the name, grey when damaged.
    MCInventoryList* inventory = Mech->Inventory;
    auto weaponLine = [&](int32_t entry, int32_t hitsIndex, char glyph, int32_t line, int32_t gap)
    {
        const MCMasterComponent& component = MasterComponentList[inventory->GetMasterIDFromIndex(entry)];
        char name[64];
        std::snprintf(name, sizeof(name), "%c %s", component.TechBase != 1 ? glyph + 0x5e : glyph,
                      component.Name.c_str());

        if (ItemHits[hitsIndex] == 0)
        {
            WriteText(BlueFont, frame, 2, (GreenFont->Height() + 2) * line + gap, name);
        }
        else
        {
            WriteText(GreyFont, frame, 2, (GreyFont->Height() + 2) * line + gap, name);
        }
    };

    int32_t line = 1;

    for (int32_t i = 0; i < NumShortRangeWeapons; ++i)
    {
        weaponLine(ShortRangeWeapons[i], i, 0x1d, line++, 2);
    }

    ++line;

    for (int32_t i = 0; i < NumMediumRangeWeapons; ++i)
    {
        weaponLine(MediumRangeWeapons[i], NumShortRangeWeapons + i, 0x1e, line++, 3);
    }

    ++line;

    for (int32_t i = 0; i < NumLongRangeWeapons; ++i)
    {
        weaponLine(LongRangeWeapons[i], shortAndMedium + i, 0x1f, line++, 4);
    }

    for (int32_t i = 0; i < NumEquipment; ++i)
    {
        ++line;
        const MCMasterComponent& component = MasterComponentList[inventory->GetMasterIDFromIndex(Equipment[i])];
        MCGuiFont* font = ItemHits[weapons + i] == 0 ? BlueFont : GreyFont;
        WriteText(font, frame, 2, (GreenFont->Height() + 2) * line + 5, component.Name.c_str());
    }
}

auto MCMechRepairBlock::GetItemFromScrollPane(MCScrollPane* pane, int32_t line, uint8_t* itemNum) -> MCLogInventoryItem*
{
    (void)pane;
    // The lines: heading, short-range weapons, heading, medium, heading, long, heading, equipment.
    int32_t shortEnd = NumShortRangeWeapons;

    if (line <= shortEnd)
    {
        if (line != 0)
        {
            return GetInvItem(ShortRangeWeapons, line - 1, itemNum);
        }

        return nullptr;
    }

    int32_t mediumEnd = NumMediumRangeWeapons + 1 + shortEnd;

    if (line <= mediumEnd)
    {
        int32_t entry = line - 1 - shortEnd;

        if (entry != 0)
        {
            return GetInvItem(MediumRangeWeapons, entry - 1, itemNum);
        }

        return nullptr;
    }

    int32_t longEnd = NumLongRangeWeapons + NumMediumRangeWeapons + 2 + shortEnd;

    if (line <= longEnd)
    {
        int32_t entry = line - 2 - NumMediumRangeWeapons - shortEnd;

        if (entry != 0)
        {
            return GetInvItem(LongRangeWeapons, entry - 1, itemNum);
        }

        return nullptr;
    }

    if (line <= NumEquipment + NumLongRangeWeapons + NumMediumRangeWeapons + 3 + shortEnd)
    {
        int32_t entry = line - 3 - NumLongRangeWeapons - NumMediumRangeWeapons - shortEnd;

        if (entry != 0)
        {
            return GetInvItem(Equipment, entry - 1, itemNum);
        }
    }

    return nullptr;
}

auto MCMechRepairBlock::SetUpItemDragIcon(MCLogInventoryItem* item, uint8_t itemNum, MCGuiEvent* event,
                                          int32_t* inventoryIndex, int32_t* hits) -> void
{
    int32_t eventX = event->X;
    int32_t eventY = event->Y;
    uint8_t masterID = item->MasterID;
    auto* icon = new MCDragIcon;
    GlobalLogPtr->DragIcon = icon;
    // Finds the dragged copy (the result is not used; the loop below looks again).
    MCLogInventoryStat* found = item->Stats;

    while (static_cast<uint32_t>(found->ItemNum) != itemNum)
    {
        found = found->Next;
    }

    RepairScreen()->DrawBlankInvInfoBlock(2);
    icon->Begin(eventX - 0x10, eventY - 0x10, 0x20, 0x20,
                [this, item](MCLogPort* surface) { OnBeginDragItem(surface, item); });

    float tonnage = MasterComponentList[masterID].Tonnage;

    if (UsesAmmo(masterID))
    {
        uint8_t ammo = MasterComponentList[masterID].AmmoMasterId;
        tonnage = MasterComponentList[ammo].Tonnage + tonnage;
        Mech->Inventory->RemoveItem(ammo, -1);
    }

    MCLogMech* logMech = Mech;
    MCInventoryList* inventory = logMech->Inventory;
    int32_t index = inventory->GetIndexFromMasterID(masterID);
    *inventoryIndex = index;
    MCLogInventoryStat* stat = inventory->GetItemInfo(index)->Stats;
    logMech->UsedTonnage -= tonnage;
    logMech->WeaponTonnage -= tonnage;

    for (; stat != nullptr; stat = stat->Next)
    {
        if (static_cast<uint32_t>(stat->ItemNum) != itemNum)
        {
            continue;
        }

        *hits = stat->Hits;
        if (stat->Hits != 0)
        {
            GlobalLogPtr->Darken(0, LogisticFadetable, GlobalLogPtr->DragIcon->Lport());
        }

        DrawItemInfo(item, logMech->Inventory);
        inventory->RemoveItem(masterID, stat->StatID);
        RepairScreen()->SetUpCompInv(0, 0);
        SetInventory(nullptr);
        DrawInventory(nullptr);
        Mech->CalcBR();
        DrawBR(nullptr);
        DrawStatusBar();
        DrawButtons(nullptr);
        RepairScreen()->AddChild(GlobalLogPtr->DragIcon);
        GlobalLogPtr->DragIcon->ShowGuiWindow(-1);
        GlobalLogPtr->DragIcon->SetDepth(100);
        return;
    }
}

auto MCMechRepairBlock::DrawInfo(MCLogPort* port) -> void
{
    if (DragPort != nullptr)
    {
        DragPort->CopyTo(port->Frame(), 0xb, 0x191, -1);
    }

    char tons[32];
    CLoadString(ThisInstance, 0x6e, tons, 0x1e);
    MCLogMech* shown = Mech;
    char text[84];
    std::snprintf(text, sizeof(text), "%.0f %s", static_cast<double>(shown->CurTonnage), tons);
    WriteText(YellowDropFont, port->Frame(), 0x53, 0x193, text);
    WriteText(YellowDropFont, port->Frame(), 0x53, 0x19c, shown->WeightClassName);
    WriteText(YellowDropFont, port->Frame(), 0xa8, 0x193, shown->ChassisClassName);
    WriteText(YellowDropFont, port->Frame(), 0xa8, 0x19c, shown->ExtraName1);
    WriteText(YellowDropFont, port->Frame(), 0xa8, 0x1a5, shown->ExtraName2);
    std::snprintf(text, sizeof(text), "%d m/s", shown->MaxRunSpeed);
    WriteText(YellowDropFont, port->Frame(), 0x53, 0x1a5, text);
    DrawInfoDescription(port, 0xc3, 0x26, shown->Description, 8, 0x1b3);
}

auto MCMechRepairBlock::DebugFunction1(int32_t arg1, int32_t arg2) -> int
{
    (void)arg1;
    (void)arg2;
    return 0;
}

MCVehicleRepairBlock::~MCVehicleRepairBlock()
{
    MCVehicleRepairBlock::Destroy();
}

auto MCVehicleRepairBlock::Init(MCLogVehicle* logVehicle) -> void
{
    Vehicle = logVehicle;
    MCLogPort* rowsPort = UnitRowsPort();
    MCLogObject::Init(0, 0, 0x19a, 0x70, nullptr, rowsPort);
}

auto MCVehicleRepairBlock::Destroy() -> void
{
    MCLogObject::Destroy();
}

auto MCVehicleRepairBlock::HandleEvent(MCGuiEvent* event) -> void
{
    if (GlobalLogPtr->CurrentScreen == GlobalLogPtr->PurchaseScreen)
    {
        return;
    }

    if (Parent != nullptr && VehicleLeftDrag == 0 && VehicleRightHeld == 0 && (event->Type == 8 || event->Type == 9))
    {
        Parent->HandleEvent(event);
        return;
    }

    GlobalX();
    GlobalY();
    int32_t eventType = event->Type;

    switch (eventType)
    {
        case 1:
        {
            if (VehicleRightHeld != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 3:
        {
            if (VehicleLeftDrag != 0)
            {
                break;
            }

            if (RepairScreen()->SelectedVehicle != Vehicle)
            {
                RepairScreen()->SelectVehicle(Vehicle);
                return;
            }

            int32_t eventX = event->X;
            int32_t eventY = event->Y;

            if (GlobalX() <= eventX && eventX <= GlobalX() + Width() && GlobalY() <= eventY &&
                eventY <= GlobalY() + Height())
            {
                // Pick up the vehicle.
                PlaySample(0x35);
                Application->SetCursorVisible(0);
                Application->Grab(this);
                DraggingVehicle = -1;

                if (eventType == 1)
                {
                    VehicleLeftDrag = -1;
                }
                else
                {
                    VehicleRightHeld = 1;
                }

                VehicleDragY = eventY - 0xf;
                VehicleDragX = eventX - 0xf;
                auto* icon = new MCDragIcon;
                GlobalLogPtr->DragIcon = icon;
                icon->Begin(VehicleDragX, VehicleDragY, 0x1e, 0x1e,
                            [this](MCLogPort* surface) { OnBeginDrag(surface); });
                RepairScreen()->AddChild(GlobalLogPtr->DragIcon);
                GlobalLogPtr->DragIcon->ShowGuiWindow(-1);
                GlobalLogPtr->DragIcon->SetDepth(100);
                GlobalLogPtr->DragIcon->MoveTo(VehicleDragX, VehicleDragY, 0);
                return;
            }

            PlaySample(0x33);
            return;
        }

        case 4:
        {
            if (VehicleRightHeld != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 6:
        {
            if (VehicleLeftDrag != 0 && eventType == 6)
            {
                return;
            }

            VehicleRightHeld = 0;

            if (Application->GrabbedObject() == nullptr)
            {
                return;
            }

            Application->SetCursorVisible(-1);
            PlaySample(0x34);
            MCGuiObject* inventory = RepairScreen()->InventoryPane;

            if (DraggingVehicle != 0)
            {
                // Dropped on the inventory, the vehicle leaves the force.
                DraggingVehicle = 0;
                Application->Release();
                VehicleLeftDrag = 0;
                DeleteDragIcon();

                if (eventType != 6 && !OverPaneInside(inventory, event))
                {
                    return;
                }

                for (auto& lance : GlobalLogPtr->DeploySlots)
                {
                    for (auto& slot : lance)
                    {
                        if (slot.Vehicle < 0)
                        {
                            continue;
                        }

                        if (slot.Vehicle == SlotIndex - GlobalLogPtr->ForceMechList->GetMechCount())
                        {
                            MCLogVehicle* leaving = Vehicle;
                            Assert(leaving != nullptr, 0, "Vehicle is NULL");
                            MCMechBriefBlock* brief = leaving->BriefBlock;
                            Assert(brief != nullptr, 0, "vehicleBrief is NULL");

                            if (brief->Parent != nullptr)
                            {
                                brief->Parent->RemoveChild(brief);
                            }

                            brief->ShowGuiWindow(0);
                            slot.Vehicle = -1;
                        }
                        else
                        {
                            slot.Vehicle = slot.Vehicle - 1;
                        }
                    }
                }

                MCLogVehicle* leaving = Vehicle;
                leaving->Deployed = 0;
                leaving->Assigned = 0;

                if (leaving == RepairScreen()->SelectedVehicle)
                {
                    RepairScreen()->SelectedVehicle = nullptr;
                }

                GlobalLogPtr->ReorderVehicles();
                MCVehicleRepairBlock* block = leaving->RepairBlock;

                if (block->Parent != nullptr)
                {
                    block->Parent->RemoveChild(block);
                }

                RepairScreen()->RemoveVehicleFromList(leaving);
                RepairScreen()->CreateVhclInvBlock();
                RepairScreen()->SetUpVhclInv(-1, -1);
                return;
            }

            Application->Release();
            VehicleLeftDrag = 0;
            DeleteDragIcon();

            if (eventType == 6 || OverPaneInside(inventory, event))
            {
                GlobalLogPtr->ReorderWarriors();
                RepairScreen()->CreatePilotInvBlock();
                RepairScreen()->SetUpPilotInv(-1, -1);
                GlobalLogPtr->ShiftPilots(VehiclePilotShift, -1);
                DrawBackground(SlotIndex, nullptr);

                // Port fix: no pilot, no speech (nothing ever sets vehiclePilot; the original read through it).
                if (VehiclePilot != nullptr)
                {
                    SoundSystem()->PlayPilotSpeech(VehiclePilot->PilotAudio, 2);
                }

                VehiclePilot = nullptr;
            }
            else if (VehiclePilot != nullptr)
            {
                MCLogWarrior* pilot = VehiclePilot;
                pilot->Assigned = -1;
                pilot->InventoryBlock->Vehicle = Vehicle;
                DrawBackground(SlotIndex, nullptr);
                return;
            }
            break;
        }

        case 7:
        {
            if (VehicleLeftDrag != 0)
            {
                VehicleDragY = event->Y - 0xf;
                VehicleDragX = event->X - 0xf;
                GlobalLogPtr->DragIcon->MoveTo(VehicleDragX, VehicleDragY, 0);
            }

            if (event->Key == 0)
            {
                char text[256];
                CLoadString(ThisInstance, 0x34, text, 0xfe);
                GlobalLogPtr->Ticker->SetString(text);
                return;
            }
            break;
        }

        default:
            break;
    }
}

auto MCVehicleRepairBlock::DrawDamageDiagram(MCLogPort* port) -> void
{
    MCLogVehicle* logVehicle = Vehicle;
    int32_t state[5];

    for (int32_t location = 0; location < 5; ++location)
    {
        state[location] =
            DamageState(PercentLeft(logVehicle->CurArmorPoints[location], logVehicle->MaxArmorPoints[location]));
    }

    for (int32_t shade = 0; shade < 5; ++shade)
    {
        VfxShapeLookaside(ArmorLookaside(shade));

        for (int32_t location = 0; location < 5; ++location)
        {
            if (state[location] == shade)
            {
                VfxShapeTranslateDraw(port->Frame(), GlobalLogPtr->VehicleRepShapes[logVehicle->NameIndex], location, 0,
                                      0);
            }
        }
    }
}

auto MCVehicleRepairBlock::DrawBackground(int32_t row, MCLogPort* port) -> void
{
    if (row >= 0)
    {
        // The repair screen's rows are drawn each frame (DrawRow).
        return;
    }

    PaintRow(port, 0, true, false);
}

auto MCVehicleRepairBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    // Framed while selected (the original's frame stayed until the row was painted again).
    PaintRow(port, top, false, RepairScreen()->SelectedVehicle == Vehicle);
}

auto MCVehicleRepairBlock::OnBeginDrag(MCLogPort* surface) -> void
{
    VfxPaneWipe(surface->Frame(), 0x10);

    for (int32_t location = 0; location < 5; ++location)
    {
        GlobalLogPtr->DrawVehicleBodyLoc(Vehicle, location, surface, 2, 0);
    }
}

auto MCVehicleRepairBlock::PaintRow(MCLogPort* port, int32_t top, bool briefing, bool framed) -> void
{
    MCLogPort* rowArt = LogArtf(briefing ? "%slogart\\lsbbkv00.tga" : "%slogart\\lsrupv00.tga", ArtPath);

    if (rowArt == nullptr)
    {
        return;
    }

    MCLogBlockPort back(port->Frame(), 0, top, rowArt->Width(), rowArt->Height(), true);
    VfxPaneCopy(rowArt->Frame(), 0, 0, back.Frame(), 0, 0, -1);

    if (MCLogPort* art = LogArtf("%slogart\\lscflv%02d.tga", ArtPath, Vehicle->NameIndex))
    {
        art->CopyTo(back.Frame(), 5, 4, -1);
    }

    // The damage diagram, drawn over a copy of the vehicle's mask.
    if (MCLogPort* maskArt = LogArtf("%slogart\\vmask%02d.tga", ArtPath, Vehicle->NameIndex))
    {
        MCLogBlockPort mask(back.Frame(), briefing ? 0x124 : 0x11d, 8, maskArt->Width(), maskArt->Height(), true);
        VfxPaneCopy(maskArt->Frame(), 0, 0, mask.Frame(), 0, 0, -1);
        DrawDamageDiagram(&mask);
    }

    SetBar(&back, briefing ? 0x124 : 0x11d);

    // The equipment and weapons, one "count name" line each.
    MCLogVehicle* logVehicle = Vehicle;
    MCInventoryList* inventory = logVehicle->Inventory;
    int32_t line = 0;
    char text[1024];

    for (int32_t index = 0; index < inventory->NumItems; ++index)
    {
        MCComponentForm form = MasterComponentList[inventory->GetMasterIDFromIndex(index)].Form;

        if (IsEquipment(form) || IsWeapon(form) || form == MCComponentForm::Weapon)
        {
            MCLogInventoryItem* item = logVehicle->Inventory->GetItemInfo(index);
            std::snprintf(text, sizeof(text), "%d %s", item->Count, item->Name);
            WriteText(GreenFont, back.Frame(), 0x8e, (GreenFont->Height() + 1) * line + 0x16, text);
            ++line;
        }

        inventory = logVehicle->Inventory;
    }

    char format[256];
    char fileName[256];
    CLoadString(ThisInstance, 0x53, format, 0xfe);
    std::snprintf(fileName, sizeof(fileName), format, static_cast<double>(logVehicle->CurTonnage),
                  logVehicle->InventoryBlock->WeightClassText);
    WriteText(BlueFont, back.Frame(), 6, 0x11, fileName);
    std::snprintf(fileName, sizeof(fileName), "%d m/s", logVehicle->MaxMoveSpeed);
    WriteText(YellowDropFont, back.Frame(), 6, 0x29, fileName);
    WriteText(YellowDropFont, back.Frame(), 6, 0x3b, logVehicle->InventoryBlock->WeightClassText);

    if (!briefing)
    {
        if (framed)
        {
            // The selected vehicle's frame.
            DrawLine(back.Frame(), 1, 0, Width(), 0, 0xf2);
            DrawLine(back.Frame(), 1, 0, 1, Height() - 3, 0xf2);
            DrawLine(back.Frame(), 1, Height() - 3, Width(), Height() - 3, 0xf2);
            DrawLine(back.Frame(), Width(), 0, Width(), Height() - 3, 0xf2);
        }
        else
        {
            GlobalLogPtr->Darken(0, LogisticFadetable, &back);
        }
    }
}

auto MCVehicleRepairBlock::SetBar(MCLogPort* port, int32_t xPos) -> void
{
    MCLogVehicle* logVehicle = Vehicle;
    // The firepower left (1 without weapons), times each location's armor (at least 40% each).
    double working = 0.0;
    double total = 0.0;
    double firepower = 1.0;
    MCLogInventoryItem* item = logVehicle->Inventory->Items;

    if (item != nullptr)
    {
        for (; item != nullptr; item = item->Next)
        {
            const MCMasterComponent& component = MasterComponentList[item->MasterID];

            if (!IsWeapon(component.Form) || item->Stats == nullptr)
            {
                continue;
            }

            int16_t value = WeaponValue(component);

            for (MCLogInventoryStat* stat = item->Stats; stat != nullptr; stat = stat->Next)
            {
                if (stat->Hits == 0)
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
        return static_cast<double>(logVehicle->CurArmorPoints[location]) / logVehicle->MaxArmorPoints[location] * 0.6 +
               0.4;
    };

    double last = 1.0;

    if (static_cast<float>(logVehicle->MaxArmorPoints[4]) != 0.0f)
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
        DrawLine(port->Frame(), xPos, y, end, y, color);
    }
}

auto MCVehicleRepairBlock::SetPilotStats() -> void
{
}

auto MCVehicleRepairBlock::SetPilotHealth(int32_t health, MCLogPort* port) -> void
{
    (void)health;
    (void)port;
}

auto MCVehicleRepairBlock::ClearPilot() -> void
{
}

MCBriefingBox::~MCBriefingBox()
{
    MCBriefingBox::Destroy();
}

auto MCBriefingBox::Init(MCLogMech* logMech, MCLogVehicle* logVehicle) -> void
{
    Mech = logMech;
    Vehicle = logVehicle;
    MCLogObject::Init(0xd3, 0x16f, 0x1ab, 0x6f, nullptr, RepairScreen()->Lport());

    if (logMech == nullptr)
    {
        InventoryPane = nullptr;
        return;
    }

    auto* pane = new MCScrollPane;

    if (pane != nullptr)
    {
        pane->Init();
    }

    InventoryPane = pane;
    pane->Init(0x62, 0x58, 0x143, 0x11, static_cast<char*>(nullptr));
    logMech->RepairBlock->SetInventory(pane);
    AddChild(pane);
}

auto MCBriefingBox::Destroy() -> void
{
    if (InventoryPane != nullptr)
    {
        delete InventoryPane;
        InventoryPane = nullptr;
    }

    MCLogObject::Destroy();
}

auto MCBriefingBox::DrawBackground() -> void
{
    // Port: the box is drawn each frame (PaintBox) by the briefing screen.
    if (Mech != nullptr)
    {
        Mech->RepairBlock->SetInventory(InventoryPane);
    }

    GlobalLogPtr->BriefingScreen->ShowBox(this);
}

auto MCBriefingBox::PaintBox(MCPane* target, int32_t xPos, int32_t yPos) -> void
{
    // The block paints in the briefing screen's layout (it looks at the current screen); a screen change's wipe draws
    // the box while another screen is current.
    MCLogObject* const current = GlobalLogPtr->CurrentScreen;
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->BriefingScreen;
    MCLogBlockPort work(target, xPos, yPos, 0x1ab, 0x6f, false);

    if (Mech == nullptr)
    {
        Vehicle->RepairBlock->DrawBackground(-1, &work);
    }
    else
    {
        MCMechRepairBlock* block = Mech->RepairBlock;
        block->DrawBackground(-1, &work);
        MCScrollPane* pane = InventoryPane;
        // The weapon list and its slider, then the tonnage bar.
        MCLogBlockPort list(work.Frame(), 0x143, 0x11, 0x62, 0x58, false);
        pane->DrawContentTo(list.Frame(), 0, 0);
        pane->DrawSliderColumn(work.Frame(), pane->Width() + 0x136, 0x11, false);
        int32_t fill = static_cast<int32_t>(static_cast<double>(Mech->WeaponTonnage) / Mech->FreeTonnage * 55.0);
        DrawTonnageBar(work.Frame(), 0x16c, fill);
    }

    GlobalLogPtr->CurrentScreen = current;
    GlobalLogPtr->Darken(0, LogisticFadetable, &work);
}

auto MCBriefingBox::DrawVehicleBackground() -> void
{
}

auto MCBriefingBox::HandleEvent(MCGuiEvent* event) -> void
{
    MCGuiObject* pane = Child(0);

    if (Parent != nullptr && (event->Type == 8 || event->Type == 9))
    {
        Parent->HandleEvent(event);
        return;
    }

    if (pane != nullptr)
    {
        pane->HandleEvent(event);
    }
}

auto MCBriefingBox::MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) -> bool
{
    // The pane redraws the box (its parent) when it scrolls.
    MCGuiObject* pane = Child(0);
    return OverPane(pane, xPos, yPos) && pane->MouseWheel(steps, xPos, yPos);
}

auto MCBriefingBox::Draw() -> void
{
    // The original repainted the weapon list (scrolled) into the briefing screen's picture; the screen draws the
    // whole box each frame (PaintBox).
}

auto MCBriefingBox::Display() -> void
{
}
