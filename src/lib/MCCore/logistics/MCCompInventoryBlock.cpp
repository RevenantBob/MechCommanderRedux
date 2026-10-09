#include "stdafx.h"
#include "logistics/MCCompInventoryBlock.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiGlobals.h"
#include "gui/MCScrollPane.h"
#include "main/MCGamePaths.h"
#include "logistics/MCLogRows.h"
#include "logistics/MCMechRepairBlock.h"
#include "logistics/MCPurchaseDlg.h"
#include "logistics/MCPurchaseScreen.h"
#include "logistics/MCRepairScreen.h"
#include "logistics/MCTicker.h"
#include "logistics/MCUnitLimits.h"
#include "main/logistics.h"
#include "main/main.h"
#include "object/MCMasterComponent.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>The component rows' drag.</summary>
    MCDragState CompDrag;

    /// <summary>The string table index of the range word of a weapon with long range <paramref name="range"/>.</summary>
    uint32_t RangeWord(float range)
    {
        return range < 76.0f ? 0x55 : range < 151.0f ? 0x50 : 0x6d;
    }

    /// <summary>
    /// Mounts the dragged component on <paramref name="mech"/> (the one selected on the repair screen): undeploys it,
    /// and when it has the free tonnage adds the component (and a weapon's ammo) to its inventory.
    /// </summary>
    /// <returns>True when mounted; false after showing the "too heavy" message.</returns>
    bool MountComponent(MCLogInventoryItem* item, MCLogMech* mech)
    {
        if (mech->Deployed != 0)
        {
            mech->RepairBlock->UndeployMech();
        }

        uint8_t masterID = item->MasterID;
        MCMasterComponent& component = MasterComponentList[masterID];
        bool withAmmo = UsesAmmo(masterID);
        double tons = component.Tonnage;

        if (withAmmo)
        {
            tons += MasterComponentList[component.AmmoMasterId].Tonnage;
        }

        if (tons > static_cast<double>(mech->CurTonnage) - mech->UsedTonnage)
        {
            ShowLogMessage(99, true);
            return false;
        }

        PlayLogSound(0x34);
        MCLogInventoryStat* stat = mech->Inventory->CreateStat(masterID, 0, 1, 1, 0xff);
        mech->Inventory->AddItem(masterID, stat, -1);

        if (withAmmo)
        {
            stat = mech->Inventory->CreateStat(masterID, 0, 0, -1, 0xff);
            mech->Inventory->AddItem(component.AmmoMasterId, stat, -1);
        }

        float added = static_cast<float>(tons);
        mech->UsedTonnage += added;
        mech->WeaponTonnage += added;
        mech->RepairBlock->SetInventory(nullptr);
        mech->CalcBR();
        GlobalLogPtr->RepairScreen->SelectMech(mech);
        return true;
    }
}

MCCompInventoryBlock::~MCCompInventoryBlock()
{
    MCCompInventoryBlock::Destroy();
}

auto MCCompInventoryBlock::Init(MCLogInventoryItem* newItem) -> void
{
    Item = newItem;
    InitRow();
    const MCMasterComponent& component = MasterComponentList[Item->MasterID];
    MCComponentForm form = component.Form;
    Tonnage = component.Tonnage;

    if (UsesAmmo(Item->MasterID))
    {
        Tonnage = MasterComponentList[component.AmmoMasterId].Tonnage + Tonnage;
    }

    WeightText = std::format("{:.1f} {}", Tonnage, LoadGameString(0x6e, 0xfe));
    float range = 0.0f;

    if (IsWeapon(form))
    {
        // Weapons: the long range as a word, damage and recycle time.
        range = component.WeaponRange[3];
        RangeText = LoadGameString(RangeWord(range), 0xfe);
        double damage = component.Damage;

        if (component.WeaponFlags == 4)
        {
            damage *= 3.0;
        }

        DamageText = std::format("{:.2f}", damage);
        RecycleText = std::format("{:.2f} s", component.RecycleTime);
    }
    else
    {
        std::string notApplicable = LoadGameString(0x6c, 0xfe);

        if (form == MCComponentForm::Probe)
        {
            RangeText = notApplicable;
        }
        else
        {
            // Original behaviour (OB-078): other equipment than ECM and sensors formats the item pointer's bits as the
            // range; for a heap address that is a denormal, so it read "0.0 m".
            const float shown =
                form == MCComponentForm::Ecm || form == MCComponentForm::Sensor ? component.RangeOrHeat : 0.0f;
            RangeText = std::format("{:.1f} m", shown);
        }

        DamageText = notApplicable;
        RecycleText = notApplicable;
    }

    // The icon: the range colour's block background, name, a caption and the component picture.
    const std::string_view background = range < 76.0f ? "greeninv.tga" : range < 151.0f ? "blueinv.tga" : "redinv.tga";
    const std::string backgroundPath = std::format("{}logart\\{}", ArtPath, background);
    IconPort = std::make_unique<MCLogPort>();
    IconPort->Load(backgroundPath);
    auto own = std::make_unique<MCLogPort>();
    own->Load(backgroundPath);
    SetOwnPort(std::move(own));
    WriteText(YellowDropFont, IconPort.get(), 0x26, 7, component.Name);
    WriteText(BlueDropFont, IconPort.get(), 0x26, 0x15, LoadGameString(0x37d, 0xfe));
    MCLogPort picture;
    picture.Load(std::format("{}logart\\lscicc{:02}.tga", ArtPath, Item->RangeIndex));
    picture.CopyTo(IconPort->Frame(), 3, 2, true);

    if (component.TechBase == 1)
    {
        // Clan technology: a small mark in the range colour.
        const float longRange = component.WeaponRange[3];
        int32_t color = longRange < 76.0f ? 0xe : longRange < 151.0f ? 0xe5 : 0xee;
        MCPane* frame = IconPort->Frame();
        VfxLineDraw(frame, 5, 7, 5, 8, color);
        VfxLineDraw(frame, 6, 5, 6, 8, color);
        VfxLineDraw(frame, 7, 4, 7, 8, color);
        VfxLineDraw(frame, 8, 5, 8, 8, color);
        VfxLineDraw(frame, 9, 7, 9, 8, color);
    }
}

auto MCCompInventoryBlock::Destroy() -> void
{
    Item = nullptr;
    IconPort.reset();
    MCInventoryBlock::Destroy();
}

auto MCCompInventoryBlock::CanMount(uint8_t masterID, float tonnage, const MCLogMech* mech) -> bool
{
    if (mech == nullptr || static_cast<double>(mech->CurTonnage) - mech->UsedTonnage < tonnage)
    {
        return false;
    }

    // One ECM, sensor or probe per mech.
    const MCComponentForm form = ComponentForm(masterID);

    if (IsEquipment(form))
    {
        for (MCLogInventoryItem* mounted = mech->Inventory->Items; mounted != nullptr; mounted = mounted->Next)
        {
            if (form == ComponentForm(mounted->MasterID))
            {
                return false;
            }
        }
    }

    // Some components only fit the mechs of name index 5, 0xe and 0x10.
    const bool restricted = std::ranges::contains(RestrictedComps, masterID);
    return !restricted || mech->NameIndex == 0xe || mech->NameIndex == 0x10 || mech->NameIndex == 5;
}

auto MCCompInventoryBlock::AddSpare(uint8_t masterID) -> MCLogInventoryItem*
{
    MCInventoryList* spares = GlobalLogPtr->ComponentInventory;
    MCLogInventoryStat* stat = spares->CreateStat(masterID, 0, 1, 0, 0xff);
    spares->AddItem(masterID, stat, -1);
    MCLogInventoryItem* item = spares->GetItemInfo(spares->GetIndexFromMasterID(masterID));
    MCInventoryList::MakeInventoryBlock(item)->InventoryIndex = spares->NumItems - 1;
    return item;
}

auto MCCompInventoryBlock::OfferSale() -> void
{
    PlayLogSound(0x34);
    MCLogPort picture;
    picture.Load(std::format("{}logart\\lscicc{:02}.tga", ArtPath, Item->RangeIndex));
    const MCMasterComponent& component = MasterComponentList[Item->MasterID];
    int32_t price = SalePrice(component.ResourcePoints);
    OpenPurchaseDialog(5, -price, Item->Count, component.Name, {}, &picture,
                       [this](int32_t result, int32_t quantity) { OnSellConfirmed(result, quantity); });
}

auto MCCompInventoryBlock::OnSellConfirmed(int32_t result, int32_t quantity) -> void
{
    if (result == 0)
    {
        return;
    }

    MCLogInventoryItem* sold = Item;
    sold->Count -= quantity;

    if (sold->Count == 0)
    {
        GlobalLogPtr->PurchaseScreen->CreateCompInvBlock();
        GlobalLogPtr->PurchaseScreen->SetUpCompInv(false, true);
    }
    else
    {
        sold->InventoryBlock->DrawBackground();
    }

    ResourcePoints -= GlobalLogPtr->PurchaseDialog->UnitCost * quantity;
}

auto MCCompInventoryBlock::HandleEvent(MCGuiEvent* event) -> void
{
    if (!PreHandleEvent(CompDrag, event))
    {
        return;
    }

    MCLogInvScreen* screen = Screen();

    if (CompDrag.Idle())
    {
        // The info block: picture, range, damage, recycle time and description.
        screen->DrawBlankInvInfoBlock(-1);
        PrepareInfoDescription(Item->Description);
        screen->ShowComponentInfo(this, false);
    }

    // Where a drop that didn't mount or sell ends: the sound, then the copy goes back to the row.
    auto returnToInventory = [&](uint32_t sample)
    {
        PlayLogSound(sample);

        if (!OnPurchaseScreen() && !CompDrag.Carrying)
        {
            ++Item->Count;
        }

        if (Item->Count > 1)
        {
            DrawBackground();
        }
        else
        {
            screen->CreateCompInvBlock();
            screen->SetUpCompInv(false, true);
        }

        CompDrag.Carrying = false;
    };

    switch (event->Type)
    {
        case 1:
        {
            // Left button down: drag one copy (off the row on the repair screen).
            if (CantMount || CompDrag.Carrying)
            {
                break;
            }

            PlayLogSound(0x35);
            GuiSystem()->SetCursorVisible(false);
            GuiSystem()->Grab(this);
            CompDrag.Dragging = true;
            MakeDragIcon(CompDrag, event);

            if (screen != GlobalLogPtr->PurchaseScreen)
            {
                if (--Item->Count != 0)
                {
                    DrawBackground();
                }
                else
                {
                    screen->CreateCompInvBlock();
                    screen->SetUpCompInv(false, false);
                }
            }

            MCDragIcon::Current()->Raise();
            break;
        }

        case 3:
        {
            // Right button down: pick one copy up.
            if (CantMount || CompDrag.Dragging)
            {
                break;
            }

            CompDrag.Carrying = true;
            PlayLogSound(0x35);
            GuiSystem()->SetCursorVisible(false);
            GuiSystem()->Grab(this);
            MakeDragIcon(CompDrag, event);

            if (screen != GlobalLogPtr->PurchaseScreen && --Item->Count != 0)
            {
                DrawBackground();
            }

            MCDragIcon::Current()->Raise();
            break;
        }

        case 4:
        {
            // Left button up: drop the dragged copy.
            if (!CompDrag.Dragging)
            {
                break;
            }

            GuiSystem()->SetCursorVisible(true);
            GuiSystem()->Release();
            CompDrag.Dragging = false;
            MCDragIcon::Remove();

            if (OnRepairScreen())
            {
                DrawDropArt(screen, 2);
                MCScrollPane* unitPane = screen->UnitPane;

                if (OverPaneInside(unitPane, event))
                {
                    // Onto a mech: it must be the selected one.
                    int32_t index = (event->Y - unitPane->GlobalY() + unitPane->GetScrollOffset()) / 0x70;

                    if (index < unitPane->NumberOfChildren() && index < GlobalLogPtr->ForceMechList->NumMechs)
                    {
                        MCLogMech* target = nullptr;
                        GlobalLogPtr->ForceMechList->GetMechInfo(index, target);

                        if (target != nullptr && target == GlobalLogPtr->RepairScreen->SelectedMech &&
                            MountComponent(Item, target))
                        {
                            break;
                        }
                    }

                    returnToInventory(0x33);
                    break;
                }

                returnToInventory(OverPaneInside(screen->InventoryPane, screen->InventoryPane, screen->UnitPane, event)
                                      ? 0x34
                                      : 0x33);
                break;
            }

            if (OverPaneInside(screen->UnitPane, event))
            {
                OfferSale();
                break;
            }

            returnToInventory(OverPaneInside(screen->InventoryPane, screen->UnitPane, screen->UnitPane, event) ? 0x34
                                                                                                               : 0x33);
            break;
        }

        case 6:
        {
            // Right button up: put down the carried copy.
            if (!CompDrag.Carrying)
            {
                break;
            }

            GuiSystem()->SetCursorVisible(true);
            GuiSystem()->Release();
            MCDragIcon::Remove();
            screen->CreateCompInvBlock();
            screen->SetUpCompInv(false, true);

            if (OnRepairScreen())
            {
                // Onto the selected mech.
                // Original behaviour (OB-079): when it can't be mounted, the copy taken off the row at the pick-up is
                // not given back (the count is only restored while nothing is carried), so it is lost.
                MCLogMech* selected = GlobalLogPtr->RepairScreen->SelectedMech;

                if (selected != nullptr && MountComponent(Item, selected))
                {
                    CompDrag.Carrying = false;
                    break;
                }

                returnToInventory(0x33);
                break;
            }

            OfferSale();
            CompDrag.Carrying = false;
            break;
        }

        case 7:
        {
            // Mouse move: the dragged icon follows (a carried one only updates its position); otherwise the ticker
            // shows the row's help.
            if (CompDrag.Dragging)
            {
                CompDrag.Y = event->Y - 0xf;
                CompDrag.X = event->X - 0xf;
                MCDragIcon::Current()->MoveTo(CompDrag.X, CompDrag.Y, false);
            }
            else if (CompDrag.Carrying)
            {
                CompDrag.X = event->X - 0xf;
                CompDrag.Y = event->Y - 0xf;
            }
            else if (event->Key == 0)
            {
                GlobalLogPtr->Ticker->SetString(LoadGameString(OnPurchaseScreen() ? 0x2d : 0x32, 0xfe));
            }
            break;
        }

        default:
            break;
    }
}

auto MCCompInventoryBlock::DrawBackground() -> void
{
    if (ListIndex < 0)
    {
        ShowGuiWindow(false);
        return;
    }

    ShowGuiWindow(true);
    CantMount = !OnPurchaseScreen() && !CanMount(Item->MasterID, Tonnage, GlobalLogPtr->RepairScreen->SelectedMech);
}

auto MCCompInventoryBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    // Put together in place, as the original did in the block's own picture, then copied opaque.
    std::unique_ptr<MCLogBlockPort> row = RowPicture(IconPort.get(), port, top, false);
    WriteText(BlueDropFont, row.get(), 0x67, 0x15, std::format("{}", Item->Count));

    if (CantMount)
    {
        GlobalLogPtr->Darken(0, LogisticFadetable, row.get());
    }
}
