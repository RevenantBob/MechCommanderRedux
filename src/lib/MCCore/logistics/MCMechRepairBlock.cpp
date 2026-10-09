#include "stdafx.h"
#include "logistics/MCMechRepairBlock.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiGlobals.h"
#include "gui/MCUpdateDisplay.h"
#include "main/MCGamePaths.h"
#include "logistics/MCMechBriefBlock.h"
#include "logistics/MCCompInventoryBlock.h"
#include "logistics/MCDragIcon.h"
#include "logistics/MCLogRows.h"
#include "logistics/MCMechInventoryBlock.h"
#include "logistics/MCPurchaseScreen.h"
#include "logistics/MCRepairScreen.h"
#include "logistics/MCReusableDialog.h"
#include "logistics/MCTicker.h"
#include "logistics/MCUnitLimits.h"
#include "main/logistics.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "object/MCMasterComponent.h"
#include "sound/MCSoundSystem.h"
#include "vfx/MCVfxFunctions.h"
#include "logistics/MCBriefingScreen.h"

namespace
{
    /// <summary>What a mech row of the repair screen is doing with the mouse (one mouse: shared by every row).</summary>
    struct MCRepairDrag
    {
        /// <summary>The dragged item copy's item number.</summary>
        uint8_t ItemNum = 0;
        /// <summary>The dragged item's component.</summary>
        uint8_t MasterID = 0;
        /// <summary>Something is dragged with the left button held.</summary>
        bool LeftDrag = false;
        /// <summary>The drag icon's position.</summary>
        int32_t X = 0;
        int32_t Y = 0;
        /// <summary>Set by a right-button press, cleared by the release.</summary>
        bool RightHeld = false;
        /// <summary>The whole mech is dragged.</summary>
        bool Mech = false;
        /// <summary>An item is dragged out of the weapon list.</summary>
        bool Item = false;
        /// <summary>A repair slider is dragged; the GUI's <c>LastY</c> is the slider, <c>LastX</c> its position.</summary>
        bool Slider = false;
        /// <summary>Set when a click selects another mech; never read.</summary>
        bool MechSelected = false;
        /// <summary>A repair button is held (the buttons are redrawn on release).</summary>
        bool RepairButtonDown = false;
        /// <summary>The dragged item copy's damage.</summary>
        int32_t ItemHits = 0;
        /// <summary>The dragged item's inventory position, -1 when none.</summary>
        int32_t ItemIndex = -1;
    };

    MCRepairDrag Drag;

    void DrawLine(MCPane* pane, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t color)
    {
        VfxLineDraw(pane, x0, y0, x1, y1, color);
    }

    void WriteLine(MCGuiFont* font, MCPane* pane, int32_t x, int32_t y, std::string_view text)
    {
        font->WriteString(pane, x, y, text);
    }

    MCRepairScreen* RepairScreen()
    {
        return GlobalLogPtr->RepairScreen;
    }

    /// <summary>
    /// Shows a component's info under the inventory: its picture (<c>lscicc</c>), the range, damage and recycle texts
    /// of its inventory row (made when missing) and its description.
    /// </summary>
    void DrawItemInfo(MCLogInventoryItem* item, MCInventoryList* inventory)
    {
        if (item->InventoryBlock == nullptr)
        {
            MCInventoryList::MakeInventoryBlock(item);
            inventory->LoadDescription(0, item);
        }

        PrepareInfoDescription(item->Description);
        RepairScreen()->ShowComponentInfo(item->InventoryBlock, true);
    }

    /// <summary>Takes a mech's brief block off the briefing screen.</summary>
    void HideBriefBlock(MCGuiObject* block)
    {
        // Port fix: a block without a parent is skipped (the original called through the null parent).
        if (block->Parent != nullptr)
        {
            block->Parent->RemoveChild(block);
        }

        block->ShowGuiWindow(false);
    }

    /// <summary>Sums the current (or maximum) points of <paramref name="points"/>.</summary>
    int32_t SumPoints(std::span<const MCLogMech::ArmorPoints> points, bool maximum)
    {
        int32_t sum = 0;

        for (const MCLogMech::ArmorPoints& location : points)
        {
            sum += maximum ? location.MaxArmor : location.CurArmor;
        }

        return sum;
    }

    /// <summary>A ratio of the repair order: less than the ratio when not full, by <paramref name="factor"/>.</summary>
    float Discount(float ratio, double factor)
    {
        return static_cast<float>(static_cast<double>(ratio) - static_cast<double>(ratio) * factor);
    }
}

MCMechRepairBlock::~MCMechRepairBlock()
{
    MCMechRepairBlock::Destroy();
}

auto MCMechRepairBlock::StripUnrepaired() -> void
{
    MCLogMech* mech = Mech;
    bool finished = false;

    for (MCLogInventoryItem* item = mech->Inventory->Items; item != nullptr && !finished; item = item->Next)
    {
        const MCComponentForm form = ComponentForm(item->MasterID);

        if (IsWeapon(form) || IsEquipment(form))
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
                    finished = true;
                    break;
                }

                stat = item->Stats;
            }
        }

        if (item == nullptr)
        {
            break;
        }
    }

    DrawBackground(SlotIndex, nullptr);
    RepairScreen()->CreateCompInvBlock();
    RepairScreen()->SetUpCompInv(false, true);
}

auto MCMechRepairBlock::Init(MCLogMech* logMech) -> void
{
    Mech = logMech;
    ShortRangeWeapons.clear();
    MediumRangeWeapons.clear();
    LongRangeWeapons.clear();
    Equipment.clear();
    ItemHits.clear();
    DragPort.reset();
    MCLogObject::InitWithoutPort(0, 0, 0x19a, 0x70);
    ListPosition = Mech->NameIndex;

    InventoryPane = MCMakeGui<MCScrollPane>();
    InventoryPane->Init(0x62, 0x58, 0x135, 0x11, static_cast<char*>(nullptr));
    AddChild(InventoryPane.get());

    SliderArtPort = std::make_unique<MCLogPort>();
    SliderArtPort->Load(std::format("{}logart\\lsrupm05.tga", ArtPath));

    // The sliders' scales: 61 pixels over the total maximum; the starting points are the current values.
    int32_t totalArmor = 0;

    for (size_t i = 0; i < StartArmor.size(); ++i)
    {
        totalArmor += Mech->Armor[i].MaxArmor;
        StartArmor[i] = Mech->Armor[i].CurArmor;
    }

    ArmorPixelScale = static_cast<float>(61.0 / static_cast<double>(totalArmor));
    int32_t totalInternal = 0;

    for (size_t i = 0; i < StartInternal.size(); ++i)
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
    InventoryPane.reset();
    SliderArtPort.reset();
    ShortRangeWeapons.clear();
    MediumRangeWeapons.clear();
    LongRangeWeapons.clear();
    Equipment.clear();
    ItemHits.clear();
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
            unit = -1;
            logMech->Deployed = 0;
            return;
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

    if (OnPurchaseScreen())
    {
        return;
    }

    if (!Drag.LeftDrag)
    {
        if (Parent != nullptr && !Drag.RightHeld && (event->Type == 8 || event->Type == 9))
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

            GlobalLogPtr->Ticker->SetString(stringId != 0 ? LoadGameString(stringId, 0xfe) : std::string());
        }
    }

    MCLogWarrior* warrior = nullptr;
    GlobalLogPtr->AssignedWarriorList->GetWarriorInfo(Mech->PilotIndex, warrior);
    int32_t eventType = event->Type;

    switch (eventType)
    {
        case 1:
        {
            if (Drag.RightHeld)
            {
                return;
            }
            break;
        }

        case 3:
        {
            if (Drag.LeftDrag || DebugFunction1(localX, localY) != 0)
            {
                return;
            }

            Drag.RightHeld = true;
            break;
        }

        case 4:
        {
            if (Drag.RightHeld)
            {
                return;
            }

            [[fallthrough]];
        }
        case 6:
        {
            if (Drag.LeftDrag && eventType == 6)
            {
                return;
            }

            HandleDrop(event, eventType);
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
                    MCLogInventoryItem* item = GetItemFromScrollPane(line, Drag.ItemNum);

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
                    DragPort = std::make_unique<MCLogPort>();
                    DragPort->Init(0x1c, 0x1e);
                    VfxPaneWipe(DragPort->Frame(), 0x10);

                    for (int32_t location = 0; location < 8; ++location)
                    {
                        GlobalLogPtr->DrawMechBodyLoc(Mech, location, DragPort.get(), 2, 0);
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

            if (Drag.LeftDrag)
            {
                Drag.X = eventX - 0xf;
                Drag.Y = eventY - 0xf;
                MCDragIcon::Current()->MoveTo(Drag.X, Drag.Y, false);
                return;
            }

            if (Drag.Slider)
            {
                DragSlider(localX);
            }

            return;
        }

        default:
            return;
    }

    // A button press (1 or 3).
    if (RepairScreen()->SelectedMech != Mech)
    {
        RepairScreen()->SelectMech(Mech);
        Drag.MechSelected = true;
        Drag.RightHeld = false;
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
        MCLogInventoryItem* item = GetItemFromScrollPane(line, Drag.ItemNum);

        if (item == nullptr)
        {
            PlayLogSound(0x33);
            return;
        }

        GuiSystem()->SetCursorVisible(false);
        Drag.MasterID = item->MasterID;
        Drag.Item = true;
        PlayLogSound(0x35);
        GuiSystem()->Grab(this);

        if (eventType == 1)
        {
            Drag.LeftDrag = true;
        }

        SetUpItemDragIcon(item, Drag.ItemNum, event);
        return;
    }

    if (0 < localX && localX < 100 && 0x1c < localY && localY < 0x6d && Mech->PilotIndex >= 0)
    {
        // Pick up the pilot: the portrait becomes the drag icon and its place is blanked.
        SoundSystem()->PlayPilotSpeech(warrior->PilotAudio, 10);
        GuiSystem()->SetCursorVisible(false);
        GuiSystem()->Grab(this);

        if (eventType == 1)
        {
            Drag.LeftDrag = true;
        }

        MCDragIcon* icon = MCDragIcon::Create();
        icon->Begin(eventX - 0xf, eventY - 0xf, 0x20, 0x20, [this](MCLogPort* surface) { OnBeginDragPilot(surface); });
        ClearPilot();
        Drag.X = GlobalX() + 5;
        Drag.Y = GlobalY() + 0x26;
        icon->ShowOn(RepairScreen(), eventX - 0xf, eventY - 0xf);
        return;
    }

    if (localX < 0xe7)
    {
        // Pick up the whole mech.
        if (GlobalX() <= eventX && eventX <= GlobalX() + Width() && GlobalY() <= eventY &&
            eventY <= GlobalY() + Height())
        {
            PlayLogSound(0x35);
            GuiSystem()->SetCursorVisible(false);
            GuiSystem()->Grab(this);
            Drag.Mech = true;

            if (eventType == 1)
            {
                Drag.LeftDrag = true;
            }

            Drag.Y = eventY - 0x10;
            Drag.X = eventX - 0x10;
            MCDragIcon* icon = MCDragIcon::Create();
            icon->Begin(Drag.X, Drag.Y, 0x20, 0x20, [this](MCLogPort* surface) { OnBeginDragMech(surface); });
            icon->ShowOn(RepairScreen(), Drag.X, Drag.Y);
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
        if (localX < 0xea || 299 < localX || RepairScreen()->SelectedMech != Mech)
        {
            return;
        }

        Drag.RepairButtonDown = true;

        if (0x17 <= localY && localY <= 0x26 && CanRepairStructure)
        {
            RepairStructure();
        }
        else if (3 <= localY && localY <= 0x12 && CanRepairItems)
        {
            RepairItems();
        }

        return;
    }

    PlayLogSound(0x35);
    LastY = slider;
    Drag.Slider = true;
    LastX = sliderPos;
    GuiSystem()->Grab(this);
}

auto MCMechRepairBlock::HandleDrop(MCGuiEvent* event, int32_t eventType) -> void
{
    Drag.RightHeld = false;

    if (Drag.RepairButtonDown && Mech == RepairScreen()->SelectedMech)
    {
        DrawButtons(nullptr);
        Drag.RepairButtonDown = false;
    }

    if (GuiSystem()->GrabbedObject() == nullptr)
    {
        return;
    }

    GuiSystem()->SetCursorVisible(true);
    GuiSystem()->Release();
    Drag.LeftDrag = false;
    DrawButtons(nullptr);
    MCDragIcon::Remove();
    bool droppedOnInventory = eventType == 6 || OverPaneInside(RepairScreen()->InventoryPane, event);

    if (!Drag.Item)
    {
        if (Drag.Slider)
        {
            Drag.Slider = false;
            GuiSystem()->Release();
            DrawArmorSlider(nullptr);
            DrawInternalSlider(nullptr);
            DrawEngineSlider(nullptr);
            DrawStatusBar();
            DrawButtons(nullptr);
            return;
        }

        if (!Drag.Mech)
        {
            // The pilot: dropped on the inventory, it leaves the mech.
            GuiSystem()->Release();
            Drag.LeftDrag = false;
            MCDragIcon::Remove();
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
                RepairScreen()->SetUpPilotInv(true, true);
                SoundSystem()->PlayPilotSpeech(pilot->PilotAudio, 2);
                return;
            }

            PlayLogSound(0x33);
            pilotsMech->RepairBlock->DrawBackground(pilotsMech->RepairBlock->SlotIndex, nullptr);
            return;
        }

        // The whole mech: dropped on the inventory, it leaves the force.
        Drag.Mech = false;

        if (!droppedOnInventory)
        {
            PlayLogSound(0x33);
            return;
        }

        if (Mech->Deployed != 0)
        {
            UndeployMech();
        }

        RepairScreen()->SelectMech(nullptr);
        PlayLogSound(0x34);
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
            RepairScreen()->SetUpPilotInv(true, true);
        }

        MCMechBriefBlock* brief = leaving->BriefBlock;

        if (brief != nullptr && brief->Parent != nullptr)
        {
            brief->Parent->RemoveChild(brief);
            brief->ShowGuiWindow(false);
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
        RepairScreen()->SetUpMechInv(true, true);
        return;
    }

    // An item from the weapon list.
    Drag.Item = false;
    RepairScreen()->DrawBlankInvInfoBlock(-1);

    if (droppedOnInventory)
    {
        // Into the component inventory (a damaged item is thrown away).
        if (Mech->Deployed != 0)
        {
            UndeployMech();
        }

        PlayLogSound(0x34);

        if (Drag.ItemHits == 0)
        {
            MCInventoryList* components = GlobalLogPtr->ComponentInventory;
            MCLogInventoryItem* item = components->GetItemInfo(components->GetIndexFromMasterID(Drag.MasterID));

            if (item == nullptr)
            {
                item = MCCompInventoryBlock::AddSpare(Drag.MasterID);
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
                RepairScreen()->SetUpCompInv(false, true);
            }
        }
    }
    else
    {
        // Back into the selected mech (with a weapon's ammo).
        MCLogMech* target = RepairScreen()->SelectedMech;
        bool withAmmo = UsesAmmo(Drag.MasterID);
        PlayLogSound(0x34);
        MCInventoryList* inventory = target->Inventory;
        MCLogInventoryStat* stat = inventory->CreateStat(Drag.ItemNum, static_cast<uint8_t>(Drag.ItemHits), 0, 1, 0xff);
        inventory->AddItem(Drag.MasterID, stat, -1);
        float tonnage = MasterComponentList[Drag.MasterID].Tonnage;

        if (withAmmo)
        {
            uint8_t ammo = MasterComponentList[Drag.MasterID].AmmoMasterId;
            stat = inventory->CreateStat(inventory->NextStatID, 0, 0, -1, 0xff);
            inventory->AddItem(ammo, stat, -1);
            tonnage = MasterComponentList[ammo].Tonnage + tonnage;
        }

        target->UsedTonnage = tonnage + target->UsedTonnage;
        target->WeaponTonnage = tonnage + target->WeaponTonnage;
        target->CalcBR();
        target->RepairBlock->DrawBackground(target->RepairBlock->SlotIndex, nullptr);
        DrawStatusBar();
        RepairScreen()->SetUpCompInv(false, true);
    }

    Drag.ItemIndex = -1;
    DrawButtons(nullptr);
}

auto MCMechRepairBlock::DragSlider(int32_t localX) -> void
{
    // Dragging a repair slider: undo the drag so far, then repair up to the new position as far as the resource points
    // go.
    if (Mech->Deployed != 0)
    {
        UndeployMech();
    }

    DragPort.reset();
    int32_t position = std::min(localX, SliderRight);
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
        int32_t before = SumPoints(repaired->Armor, false);

        for (size_t i = 0; i < StartArmor.size(); ++i)
        {
            repaired->Armor[i].CurArmor = static_cast<uint8_t>(StartArmor[i]);
        }

        int32_t restored = SumPoints(repaired->Armor, false);
        int32_t cost = GlobalLogPtr->ArmorCost;
        ResourcePoints = ResourcePoints + (before - restored) * cost;
        int32_t points;

        if (position < SliderRight)
        {
            points = static_cast<int32_t>(static_cast<double>(position - ArmorSliderStart) /
                                          static_cast<double>(ArmorPixelScale));
        }
        else
        {
            points = SumPoints(repaired->Armor, true) - restored;
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
        int32_t before = SumPoints(repaired->Internals, false);

        for (size_t i = 0; i < StartInternal.size(); ++i)
        {
            repaired->Internals[i].CurArmor = static_cast<uint8_t>(StartInternal[i]);
        }

        int32_t restored = SumPoints(repaired->Internals, false);
        int32_t cost = GlobalLogPtr->InternalCost;
        ResourcePoints = ResourcePoints + (before - restored) * cost;
        int32_t points;

        if (position < SliderRight)
        {
            points = static_cast<int32_t>(static_cast<double>(position - InternalSliderStart) /
                                          static_cast<double>(InternalPixelScale));
        }
        else
        {
            points = SumPoints(repaired->Internals, true) - restored;
        }

        int32_t affordable = ResourcePoints / cost;

        if (points < affordable)
        {
            RepairInternal(points);
            InternalSliderPos = position;
            ResourcePoints = ResourcePoints - GlobalLogPtr->InternalCost * points;
        }
        else
        {
            RepairInternal(affordable);
            InternalSliderPos = static_cast<int32_t>(
                static_cast<double>(affordable) * static_cast<double>(InternalPixelScale) + InternalSliderStart);
            ResourcePoints = ResourcePoints - GlobalLogPtr->InternalCost * affordable;
        }

        DrawInternalSlider(nullptr);
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

        // The engine repairs in whole damage levels; each costs EngineCost.
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
            newPosition = SliderRight;
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
}

auto MCMechRepairBlock::RepairItems() -> void
{
    // Replace every damaged copy from the component inventory; list the ones missing.
    if (Mech->Deployed != 0)
    {
        UndeployMech();
    }

    std::string missing;
    Drag.RepairButtonDown = true;
    PlayLogSound(0x35);
    _PressedButton = 1;
    UpdateDisplay(false, false, 0, false, 0);

    for (MCLogInventoryItem* item = Mech->Inventory->Items; item != nullptr; item = item->Next)
    {
        MCComponentForm form = ComponentForm(item->MasterID);

        for (MCLogInventoryStat* stat = item->Stats; stat != nullptr; stat = stat->Next)
        {
            if (!IsWeapon(form) && !IsEquipment(form))
            {
                if (form != MCComponentForm::Engine && stat->Hits != 0)
                {
                    stat->Hits = 0;
                }

                continue;
            }

            if (stat->Hits == 0)
            {
                continue;
            }

            MCInventoryList* components = GlobalLogPtr->ComponentInventory;
            MCLogInventoryItem* stockItem = components->GetItemInfo(components->GetIndexFromMasterID(item->MasterID));

            if (stockItem == nullptr || stockItem->Count == 0)
            {
                // Port fix: the name comes from the mech's own item when the inventory has none (the original read
                // the name through the null item).
                const char* name = stockItem != nullptr ? stockItem->Name : item->Name;
                missing += missing.empty() ? name : std::format(",{}", name);
                continue;
            }

            --stockItem->Count;
            stockItem->InventoryBlock->DrawBackground();
            stat->Hits = 0;
        }
    }

    GlobalLogPtr->ReIndexInventory();

    if (!missing.empty())
    {
        // Ask whether to strip the damaged items that have no replacement (StripUnrepaired).
        GuiSystem()->Release();
        Drag.LeftDrag = false;
        MCRefitDialog* dialog = GlobalLogPtr->RefitDialog;
        dialog->SetText(missing);
        dialog->Callback = nullptr;
        dialog->SetTwoButton(true);
        dialog->OkButton->SetUpPicture("bh_okay.tga");
        dialog->OkButton->SetDownPicture("bg_okay.tga");
        dialog->OkButton->Callback()->SetExec([this] { StripUnrepaired(); });
        dialog->CancelButton->SetUpPicture("bh_cancl.tga");
        dialog->CancelButton->SetDownPicture("bg_cancl.tga");
        dialog->Activate();
        PlayLogSound(0x33);
        DrawBackground(SlotIndex, nullptr);
        return;
    }

    RepairScreen()->CreateCompInvBlock();
    RepairScreen()->SetUpCompInv(false, true);
    DrawBackground(SlotIndex, nullptr);
}

auto MCMechRepairBlock::RepairStructure() -> void
{
    // The engine level by level, then internal structure, then armor, as far as the resource points go.
    if (Mech->Deployed != 0)
    {
        UndeployMech();
    }

    DragPort.reset();
    PlayLogSound(0x35);
    _PressedButton = 2;
    UpdateDisplay(false, false, 0, false, 0);
    bool shortOfPoints = false;
    MCLogInventoryStat* engine = EngineStat;
    const int32_t engineCost = GlobalLogPtr->EngineCost;

    while (engine->Hits != 0 && !shortOfPoints)
    {
        if (ResourcePoints < engineCost)
        {
            shortOfPoints = true;
            break;
        }

        ResourcePoints = ResourcePoints - engineCost;
        --engine->Hits;
        Mech->Status = 0;
    }

    if (!shortOfPoints)
    {
        int32_t points = SumPoints(Mech->Internals, true) - SumPoints(Mech->Internals, false);

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
    }

    if (!shortOfPoints)
    {
        int32_t points = SumPoints(Mech->Armor, true) - SumPoints(Mech->Armor, false);

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

    SetArmorSlider(-1);
    SetInternalSlider(-1);
    SetEngineSlider(-1);
    DrawBackground(SlotIndex, nullptr);

    if (shortOfPoints)
    {
        GuiSystem()->Release();
        Drag.LeftDrag = false;
        ShowLogMessage(0x57);
        PlayLogSound(0x33);
    }

    DrawBackground(SlotIndex, nullptr);
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
    MCLogPort* rowArt = briefing ? LogScreenArt("lsbbkm00.tga") : GlobalLogPtr->RepairBackPort;

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
        rowArt->CopyTo(back.Frame(), 0, 0, true);
    }

    MCLogMech* logMech = Mech;
    static constexpr std::array<std::string_view, 3> chassisArt = {"lscflma", "lscflmw", "lscflmj"};

    if (logMech->NameVariant >= 0 && logMech->NameVariant < 3)
    {
        CopyArt(&back, 5, 4,
                std::format("{}{:02}.tga", chassisArt[static_cast<size_t>(logMech->NameVariant)], logMech->NameIndex));
    }

    if (MCLogPort* art = LogScreenArt(std::format("lscdsm{:02}.tga", logMech->NameIndex)))
    {
        art->CopyTo(back.Frame(), briefing ? 0xdc : 0xd6, 8, true);
    }

    WriteLine(BlueDropFont, back.Frame(), 6, 0x12,
              MCFormatPrintf(LoadGameString(0x4e, 0xfe).c_str(), static_cast<double>(logMech->CurTonnage),
                             logMech->WeightClassName));

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

    // The buttons are live when there is something to repair. (Painting the briefing box, the original found nothing
    // to repair: it only looked for the repair screen's rows.)
    CanRepairItems = onRows && ItemsDamaged();
    CanRepairStructure = onRows && StructureDamaged();

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
    return EngineStat->Hits != 0 || SumPoints(Mech->Internals, false) != SumPoints(Mech->Internals, true) ||
           SumPoints(Mech->Armor, false) != SumPoints(Mech->Armor, true);
}

auto MCMechRepairBlock::ShowsInventory() const -> bool
{
    // In multiplayer, only the player's own mechs (and the one in the briefing box) list their weapons.
    return MPlayer == nullptr || GlobalLogPtr->ForceMechList->GetMechIndex(Mech) >= 0 ||
           Mech->BriefingBox == GlobalLogPtr->BriefingScreen->BriefingBox;
}

auto MCMechRepairBlock::PaintButtons(MCLogPort* port, int32_t top, bool onRows, bool items, bool structure) -> void
{
    // Drawn in place: the original painted a scratch picture (wiped to 0xff) and copied it here keyed on 0xff.
    MCLogBlockPort work(port->Frame(), 0, top, Width(), Height(), true);

    if (!items)
    {
        GlobalLogPtr->RepairPorts[4]->CopyTo(work.Frame(), onRows ? 0xea : 0xf8, 3, true);
    }
    else
    {
        GlobalLogPtr->RepairPorts[1]->CopyTo(work.Frame(), 0xea, 3, true);
    }

    if (!structure)
    {
        GlobalLogPtr->RepairPorts[5]->CopyTo(work.Frame(), onRows ? 0xea : 0xf8, 0x17, true);
    }
    else
    {
        GlobalLogPtr->RepairPorts[3]->CopyTo(work.Frame(), 0xea, 0x17, true);
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

    // The internal structure (shape frames 11..18), then the armor over it (0..7), coloured by damage shade.
    std::array<int32_t, 8> shade = {};

    for (size_t location = 0; location < shade.size(); ++location)
    {
        shade[location] =
            RepairShade(PercentLeft(Mech->Internals[location].CurArmor, Mech->Internals[location].MaxArmor));
    }

    for (int32_t table = 0; table < 5; ++table)
    {
        VfxShapeLookaside(InternalShadeTable(table));

        for (int32_t location = 0; location < 8; ++location)
        {
            if (shade[static_cast<size_t>(location)] == table)
            {
                VfxShapeTranslateDraw(port->Frame(), GlobalLogPtr->MechRepShapes[Mech->NameIndex], location + 0xb, xPos,
                                      top + 8);
            }
        }
    }

    for (size_t location = 0; location < shade.size(); ++location)
    {
        shade[location] = RepairShade(PercentLeft(Mech->Armor[location].CurArmor, Mech->Armor[location].MaxArmor));
    }

    for (int32_t table = 0; table < 5; ++table)
    {
        VfxShapeLookaside(ArmorShadeTable(table));

        for (int32_t location = 0; location < 8; ++location)
        {
            if (shade[static_cast<size_t>(location)] == table)
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

    if (MCLogPort* art = LogScreenArt("lsrupm09.tga"))
    {
        art->CopyTo(work.Frame(), 0x72, 5, false);
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

    if (MCLogPort* art = LogScreenArt(std::format("lsrupm{}.tga", slider + 0xb)))
    {
        art->CopyTo(work.Frame(), briefing ? 0xf8 : 0xea, yPos, true);
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
    const float status = Mech->CalcStatus();

    if (Mech->PilotIndex >= 0 || Mech->NetworkPilot != nullptr)
    {
        PaintPilot(&row, 0, status, true);
    }
    else
    {
        // OB-134 (fixed): without a pilot the original showed the status bar only once a slider or repair had
        // painted it.
        PaintStatusBar(&row, 0, status, true);
    }

    PaintButtons(&row, 0, true, ItemsDamaged(), StructureDamaged());
    PaintDiagram(&row, 0, 0x88);
    PaintBR(&row, 0);
    PaintSlider(&row, 0, 0, false);
    PaintSlider(&row, 0, 1, false);
    PaintSlider(&row, 0, 2, false);

    if (ShowsInventory())
    {
        PaintTonnage(&row, 0);
    }

    if (Mech->Assigned != 0)
    {
        PaintInventory(&row, 0);
    }

    if (!selected)
    {
        GlobalLogPtr->Darken(0, LogisticFadetable, &row);
    }

    if (_PilotLifted)
    {
        if (MCLogPort* blank = LogScreenArt("lsrupm10.tga"))
        {
            blank->CopyTo(port->Frame(), 6, top + 0x21, true);
        }
    }

    // A repair button held while its repair runs.
    if (_PressedButton != 0)
    {
        const bool items = _PressedButton == 1;
        GlobalLogPtr->RepairPorts[items ? 0 : 2]->CopyTo(port->Frame(), 0xea, top + (items ? 3 : 0x17), true);
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
    if (MCLogPort* art = LogScreenArt(std::format("lscicc{:02}.tga", item->RangeIndex)))
    {
        art->CopyTo(surface->Frame(), 1, 1, true);
    }
}

auto MCMechRepairBlock::MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) -> bool
{
    auto* pane = static_cast<MCScrollPane*>(Child(0));

    if (!OverPaneArea(pane, xPos, yPos) || !pane->MouseWheel(steps, xPos, yPos))
    {
        return false;
    }

    // As a click on the list's scroll bar.
    DrawInventory(nullptr);
    return true;
}

auto MCMechRepairBlock::DrawStatusBar(MCLogPort* port) -> void
{
    float status = Mech->CalcStatus();
    bool repairLayout = GlobalLogPtr->CurrentScreen != GlobalLogPtr->BriefingScreen;

    // The rows draw their status bar each frame (DrawRow).
    if (port != nullptr)
    {
        PaintStatusBar(port, 0, status, repairLayout);
    }
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

    static constexpr std::array<int32_t, 4> positions = {SliderRight, 0x113, 0xff, 0xeb};
    EngineSliderPos = positions[engine->Hits];
}

auto MCMechRepairBlock::SetInternalSlider(int32_t value) -> void
{
    if (value >= 0)
    {
        return;
    }

    int32_t current = SumPoints(Mech->Internals, false);
    int32_t maximum = SumPoints(Mech->Internals, true);
    InternalSliderPos =
        SliderLeft - static_cast<int32_t>(static_cast<double>(current) / maximum * static_cast<double>(-61.0f));
}

auto MCMechRepairBlock::SetArmorSlider(int32_t value) -> void
{
    if (value >= 0)
    {
        return;
    }

    int32_t current = SumPoints(Mech->Armor, false);
    int32_t maximum = SumPoints(Mech->Armor, true);
    ArmorSliderPos =
        SliderLeft - static_cast<int32_t>(static_cast<double>(current) / maximum * static_cast<double>(-61.0f));
}

auto MCMechRepairBlock::ClearPilot() -> void
{
    _PilotLifted = true;
}

auto MCMechRepairBlock::SetPilotStats(MCLogPort* port) -> void
{
    if (Mech->PilotIndex < 0 && Mech->NetworkPilot == nullptr)
    {
        return;
    }

    float status = Mech->CalcStatus();
    bool repairLayout = GlobalLogPtr->CurrentScreen != GlobalLogPtr->BriefingScreen;

    // The rows draw their pilot each frame (DrawRow).
    if (port != nullptr)
    {
        PaintPilot(port, 0, status, repairLayout);
    }
}

auto MCMechRepairBlock::PaintPilot(MCLogPort* port, int32_t top, float status, bool repairLayout) -> void
{
    MCLogWarrior* warrior = nullptr;

    if (Mech->LocalPart == 0)
    {
        warrior = Mech->NetworkPilot;
    }
    else
    {
        GlobalLogPtr->AssignedWarriorList->GetWarriorInfo(Mech->PilotIndex, warrior);
    }

    // The status bar, then the portrait.
    PaintStatusBar(port, top, status, repairLayout);
    MCLogPort* portrait = warrior == nullptr ? LogScreenArt(std::format("pilot{:02}.tga", Mech->PilotIndex))
                                             : LogScreenArt(warrior->Picture);

    if (portrait != nullptr)
    {
        portrait->CopyTo(port->Frame(), 6, top + 0x26, true);
    }

    // Port fix: without a pilot record the texts are skipped (the original read them through the null pointer).
    if (warrior == nullptr)
    {
        return;
    }

    WriteLine(YellowDropFont, port->Frame(), 0x2d, top + 0x2a, warrior->Callsign);

    // An out-of-range rank shows the portrait's file name, which the text buffer last held.
    std::string rank = std::format("{}logart\\{}", ArtPath, warrior->Picture);

    if (warrior->Rank >= 0 && warrior->Rank <= 3)
    {
        rank = LoadGameString(0x70 + static_cast<uint32_t>(warrior->Rank), 0xfe);
    }

    WriteLine(YellowDropFont, port->Frame(), 0x2d, top + 0x3c, rank);
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

auto MCMechRepairBlock::SetPilotHealth(MCLogPort*) -> void
{
    // Drawn with the pilot's stats (PaintPilot).
}

auto MCMechRepairBlock::SetMechStats() -> void
{
}

auto MCMechRepairBlock::SetWeaponLists() -> void
{
    ShortRangeWeapons.clear();
    MediumRangeWeapons.clear();
    LongRangeWeapons.clear();
    Equipment.clear();

    // One entry per copy, the item's position in the inventory; weapons go by long range: under 76 short, under 151
    // medium, else long. The copies' damage, by list.
    std::array<std::vector<int32_t>, 4> hits;
    int32_t index = 0;

    for (MCLogInventoryItem* item = Mech->Inventory->Items; item != nullptr; item = item->Next, ++index)
    {
        const MCMasterComponent& component = MasterComponentList[item->MasterID];
        size_t list = 0;

        if (IsWeapon(component.Form))
        {
            list = component.WeaponRange[3] < 76.0f ? 0 : component.WeaponRange[3] < 151.0f ? 1 : 2;
        }
        else if (IsEquipment(component.Form))
        {
            list = 3;
        }
        else
        {
            continue;
        }

        std::vector<int32_t>& entries =
            std::array{&ShortRangeWeapons, &MediumRangeWeapons, &LongRangeWeapons, &Equipment}[list][0];
        MCLogInventoryStat* stat = item->Stats;

        for (int32_t copy = 0; copy < item->Count; ++copy)
        {
            entries.push_back(index);
            hits[list].push_back(stat->Hits);
            stat = stat->Next;
        }
    }

    for (size_t list = 0; list < 3; ++list)
    {
        std::vector<int32_t>& entries = std::array{&ShortRangeWeapons, &MediumRangeWeapons, &LongRangeWeapons}[list][0];
        SortByDamage(entries, hits[list], *Mech->Inventory);
    }

    ItemHits.clear();

    for (const std::vector<int32_t>& listHits : hits)
    {
        ItemHits.insert(ItemHits.end(), listHits.begin(), listHits.end());
    }
}

auto MCMechRepairBlock::SortByDamage(std::span<int32_t> entries, std::span<int32_t> hits, MCInventoryList& inventory)
    -> void
{
    // An exchange sort by damage, lowest first; the hits follow their entries.
    auto damage = [&inventory](int32_t entry)
    { return MasterComponentList[inventory.GetMasterIDFromIndex(entry)].Damage; };

    for (size_t i = 0; i + 1 < entries.size(); ++i)
    {
        for (size_t j = i + 1; j < entries.size(); ++j)
        {
            if (damage(entries[j]) < damage(entries[i]))
            {
                std::swap(entries[i], entries[j]);
                std::swap(hits[i], hits[j]);
            }
        }
    }
}

auto MCMechRepairBlock::GetInvItem(const std::vector<int32_t>& list, int32_t index, uint8_t& itemNum)
    -> MCLogInventoryItem*
{
    int32_t earlier = index - 1;

    while (earlier >= 0 && list[static_cast<size_t>(earlier)] == list[static_cast<size_t>(index)])
    {
        --earlier;
    }

    int32_t copy = index - earlier - 1;
    MCLogInventoryItem* item = Mech->Inventory->GetItemInfo(list[static_cast<size_t>(index)]);
    MCLogInventoryStat* stat = item->Stats;

    for (; copy > 0; --copy)
    {
        stat = stat->Next;
    }

    itemNum = static_cast<uint8_t>(stat->ItemNum);
    return item;
}

auto MCMechRepairBlock::RepairArmor(int32_t points) -> void
{
    MCLogMech* logMech = Mech;

    if (points < 0)
    {
        for (auto& location : logMech->Armor)
        {
            location.CurArmor = location.MaxArmor;
        }

        return;
    }

    // The head first, then point by point to the most damaged location (the cockpit, center torso and rear locations
    // count as more damaged than they are).
    int32_t headMissing = logMech->Armor[0].MaxArmor - logMech->Armor[0].CurArmor;

    if (headMissing != 0)
    {
        logMech->Armor[0].CurArmor += static_cast<uint8_t>(points < headMissing ? points : headMissing);
        points -= headMissing;
    }

    if (points <= 0)
    {
        return;
    }

    // How much more damaged than its share each location counts (none for the arms, legs and side torsos).
    static constexpr std::array<double, 11> discount = {0.0, 0.2, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.4, 0.3, 0.3};
    std::array<float, 11> ratios = {};

    for (size_t location = 1; location < ratios.size(); ++location)
    {
        float ratio = static_cast<float>(static_cast<double>(logMech->Armor[location].CurArmor) /
                                         logMech->Armor[location].MaxArmor);
        ratios[location] = discount[location] != 0.0 && ratio < 1.0f ? Discount(ratio, discount[location]) : ratio;
    }

    size_t chosen = 1;

    for (; points != 0; --points)
    {
        for (size_t location = 1; location < ratios.size(); ++location)
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
            ratios[chosen] = discount[chosen] != 0.0 ? Discount(ratio, discount[chosen]) : ratio;
        }
        else
        {
            ratios[chosen] = 1.0f;
        }
    }
}

auto MCMechRepairBlock::RepairInternal(int32_t points) -> void
{
    MCLogMech* logMech = Mech;

    if (points < 0)
    {
        for (auto& location : logMech->Internals)
        {
            location.CurArmor = location.MaxArmor;
        }

        return;
    }

    // Point by point to the most damaged location (weighted like the armor).
    static constexpr std::array<double, 8> discount = {0.3, 0.5, 0.2, 0.2, 0.0, 0.0, 0.4, 0.4};
    std::array<float, 8> ratios = {};

    for (size_t location = 0; location < ratios.size(); ++location)
    {
        ratios[location] = static_cast<float>(static_cast<double>(logMech->Internals[location].CurArmor) /
                                              logMech->Internals[location].MaxArmor);

        if (discount[location] != 0.0 && ratios[location] < 1.0f)
        {
            ratios[location] = Discount(ratios[location], discount[location]);
        }
    }

    size_t chosen = 1;

    for (; points > 0; --points)
    {
        for (size_t location = 0; location < ratios.size(); ++location)
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
            ratios[chosen] = discount[chosen] != 0.0 ? Discount(ratio, discount[chosen]) : ratio;
        }
        else
        {
            ratios[chosen] = 1.0f;
        }
    }
}

auto MCMechRepairBlock::SetInventory(MCScrollPane* pane) -> void
{
    if (MPlayer != nullptr && GlobalLogPtr->ForceMechList->GetMechIndex(Mech) < 0 &&
        Mech->BriefingBox != GlobalLogPtr->BriefingScreen->BriefingBox)
    {
        return;
    }

    if (pane == nullptr)
    {
        pane = InventoryPane.get();
    }

    auto content = std::make_unique<MCLogPort>();
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
    int32_t contentHeight = std::max(lineHeight * (lines + 4) + 4, pane->Height());

    // Port: the list is drawn into the pane each frame (DrawWeaponList) from the lists made here.
    content->InitView(pane->Width() - 0xd, contentHeight);
    SetWeaponLists();
    content->DrawContent = [this](MCGuiPort* view) { DrawWeaponList(static_cast<MCLogPort*>(view)); };
    pane->SetDisplayPort(std::move(content), false);

    // The tonnage bar (the weapons' weight against the free weight): the rows draw theirs each frame; the original also
    // painted one into the briefing screen's picture for the box, which the box (drawn after) covered.
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
    auto heading = [&](int32_t top, uint8_t color, bool underline, uint32_t title, int32_t titleY)
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

        WriteLine(WhiteFont, frame, 1, titleY, LoadGameString(title, 0xfe));
    };

    const auto numShort = static_cast<int32_t>(ShortRangeWeapons.size());
    const auto numMedium = static_cast<int32_t>(MediumRangeWeapons.size());
    const auto numLong = static_cast<int32_t>(LongRangeWeapons.size());
    heading(0, 0xe, true, 0x55, 2);
    heading(lineHeight * (numShort + 1) + 1, 0xe5, true, 0x50, lineHeight * (numShort + 1) + 3);
    const int32_t shortAndMedium = numShort + numMedium;
    heading(lineHeight * (shortAndMedium + 2) + 2, 0xee, true, 0x6d, lineHeight * (shortAndMedium + 2) + 4);
    const int32_t weapons = shortAndMedium + numLong;
    heading(lineHeight * (weapons + 3) + 3, 0x14, false, 0x6f, lineHeight * (weapons + 3) + 5);

    // The entries: a range glyph (clan technology has its own) and the name, grey when damaged.
    MCInventoryList* inventory = Mech->Inventory;
    auto weaponLine = [&](int32_t entry, size_t hitsIndex, char glyph, int32_t line, int32_t gap)
    {
        const MCMasterComponent& component = MasterComponentList[inventory->GetMasterIDFromIndex(entry)];
        const char shown = component.TechBase != 1 ? static_cast<char>(glyph + 0x5e) : glyph;
        std::string name = std::format("{} {}", shown, component.Name);

        if (ItemHits[hitsIndex] == 0)
        {
            WriteLine(BlueFont, frame, 2, (GreenFont->Height() + 2) * line + gap, name);
        }
        else
        {
            WriteLine(GreyFont, frame, 2, (GreyFont->Height() + 2) * line + gap, name);
        }
    };

    int32_t line = 1;
    size_t hitsIndex = 0;

    for (int32_t entry : ShortRangeWeapons)
    {
        weaponLine(entry, hitsIndex++, 0x1d, line++, 2);
    }

    ++line;

    for (int32_t entry : MediumRangeWeapons)
    {
        weaponLine(entry, hitsIndex++, 0x1e, line++, 3);
    }

    ++line;

    for (int32_t entry : LongRangeWeapons)
    {
        weaponLine(entry, hitsIndex++, 0x1f, line++, 4);
    }

    for (int32_t entry : Equipment)
    {
        ++line;
        const MCMasterComponent& component = MasterComponentList[inventory->GetMasterIDFromIndex(entry)];
        MCGuiFont* font = ItemHits[hitsIndex++] == 0 ? BlueFont : GreyFont;
        WriteLine(font, frame, 2, (GreenFont->Height() + 2) * line + 5, component.Name);
    }
}

auto MCMechRepairBlock::GetItemFromScrollPane(int32_t line, uint8_t& itemNum) -> MCLogInventoryItem*
{
    // The lines: heading, short-range weapons, heading, medium, heading, long, heading, equipment.
    const auto numShort = static_cast<int32_t>(ShortRangeWeapons.size());
    const auto numMedium = static_cast<int32_t>(MediumRangeWeapons.size());
    const auto numLong = static_cast<int32_t>(LongRangeWeapons.size());
    const auto numEquipment = static_cast<int32_t>(Equipment.size());

    if (line <= numShort)
    {
        return line != 0 ? GetInvItem(ShortRangeWeapons, line - 1, itemNum) : nullptr;
    }

    if (line <= numMedium + 1 + numShort)
    {
        int32_t entry = line - 1 - numShort;
        return entry != 0 ? GetInvItem(MediumRangeWeapons, entry - 1, itemNum) : nullptr;
    }

    if (line <= numLong + numMedium + 2 + numShort)
    {
        int32_t entry = line - 2 - numMedium - numShort;
        return entry != 0 ? GetInvItem(LongRangeWeapons, entry - 1, itemNum) : nullptr;
    }

    if (line <= numEquipment + numLong + numMedium + 3 + numShort)
    {
        int32_t entry = line - 3 - numLong - numMedium - numShort;

        if (entry != 0)
        {
            return GetInvItem(Equipment, entry - 1, itemNum);
        }
    }

    return nullptr;
}

auto MCMechRepairBlock::SetUpItemDragIcon(MCLogInventoryItem* item, uint8_t itemNum, MCGuiEvent* event) -> void
{
    int32_t eventX = event->X;
    int32_t eventY = event->Y;
    uint8_t masterID = item->MasterID;
    MCDragIcon* icon = MCDragIcon::Create();
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

    MCInventoryList* inventory = Mech->Inventory;
    int32_t index = inventory->GetIndexFromMasterID(masterID);
    Drag.ItemIndex = index;
    MCLogInventoryStat* stat = inventory->GetItemInfo(index)->Stats;
    Mech->UsedTonnage -= tonnage;
    Mech->WeaponTonnage -= tonnage;

    for (; stat != nullptr; stat = stat->Next)
    {
        if (static_cast<uint32_t>(stat->ItemNum) != itemNum)
        {
            continue;
        }

        Drag.ItemHits = stat->Hits;

        if (stat->Hits != 0)
        {
            GlobalLogPtr->Darken(0, LogisticFadetable, icon->Lport());
        }

        DrawItemInfo(item, Mech->Inventory);
        inventory->RemoveItem(masterID, stat->StatID);
        RepairScreen()->SetUpCompInv(false, false);
        SetInventory(nullptr);
        DrawInventory(nullptr);
        Mech->CalcBR();
        DrawBR(nullptr);
        DrawStatusBar();
        DrawButtons(nullptr);
        RepairScreen()->AddChild(icon);
        icon->Raise();
        return;
    }
}

auto MCMechRepairBlock::DrawInfo(MCLogPort* port) -> void
{
    if (DragPort != nullptr)
    {
        DragPort->CopyTo(port->Frame(), 0xb, 0x191, true);
    }

    WriteText(YellowDropFont, port, 0x53, 0x193,
              std::format("{:.0f} {}", Mech->CurTonnage, LoadGameString(0x6e, 0x1e)));
    WriteText(YellowDropFont, port, 0x53, 0x19c, Mech->WeightClassName);
    WriteText(YellowDropFont, port, 0xa8, 0x193, Mech->ChassisClassName);
    WriteText(YellowDropFont, port, 0xa8, 0x19c, Mech->ExtraName1);
    WriteText(YellowDropFont, port, 0xa8, 0x1a5, Mech->ExtraName2);
    WriteText(YellowDropFont, port, 0x53, 0x1a5, std::format("{} m/s", Mech->MaxRunSpeed));
    DrawInfoDescription(port, 0xc3, 0x26, Mech->Description, 8, 0x1b3);
}

auto MCMechRepairBlock::DebugFunction1(int32_t, int32_t) -> int
{
    return 0;
}
