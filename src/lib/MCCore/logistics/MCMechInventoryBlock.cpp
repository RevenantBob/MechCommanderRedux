#include "stdafx.h"
#include "logistics/MCMechInventoryBlock.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiGlobals.h"
#include "gui/MCScrollPane.h"
#include "logistics/MCCompInventoryBlock.h"
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
    /// <summary>The mech rows' drag.</summary>
    MCDragState MechDrag;
}

MCMechInventoryBlock::~MCMechInventoryBlock()
{
    MCMechInventoryBlock::Destroy();
}

auto MCMechInventoryBlock::Init(MCLogMech* logMech) -> void
{
    DiagramPort.reset();
    Mech = logMech;
    InitRow();
    ListIndex = Mech->NameIndex;
}

auto MCMechInventoryBlock::Destroy() -> void
{
    DiagramPort.reset();
    Mech = nullptr;
    MCInventoryBlock::Destroy();
}

auto MCMechInventoryBlock::Draw() -> void
{
    if (!Enabled)
    {
        DrawDisabled();
    }
}

auto MCMechInventoryBlock::DrawBackground() -> void
{
    if (DiagramPort != nullptr)
    {
        return;
    }

    DiagramPort = std::make_unique<MCLogPort>();
    DiagramPort->Init(0x1c, 0x1e);
    VfxPaneWipe(DiagramPort->Frame(), 0x10);

    for (int32_t location = 0; location < 8; ++location)
    {
        GlobalLogPtr->DrawMechBodyLoc(Mech, location, DiagramPort.get(), 2, 0);
    }

    // The battle rating bar along the left edge: 26 pixels at 18010.
    int32_t bar = static_cast<int32_t>(static_cast<double>(Mech->BattleRating) * 0x1.d1c6674f499a1p-15 * 26.0);
    VfxLineDraw(DiagramPort->Frame(), 0, 0x1b, 0, 0x1b - bar, 0xe4);
    VfxLineDraw(DiagramPort->Frame(), 1, 0x1b, 1, 0x1b - bar, 0xe4);
}

auto MCMechInventoryBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    MCInventoryBlock::DrawRow(port, top);
    WriteText(YellowDropFont, port, 0x26, top + 7, Mech->FileName);
    WriteText(BlueDropFont, port, 0x26, top + 0x15,
              MCFormatPrintf(LoadGameString(0x4e, 0xfe).c_str(), static_cast<double>(Mech->CurTonnage),
                             Mech->WeightClassName));

    if (DiagramPort != nullptr)
    {
        DiagramPort->CopyTo(port->Frame(), 5, top + 2, true);
    }
}

auto MCMechInventoryBlock::DrawInfo(MCLogPort* port) -> void
{
    // The 0x1e square at (3, 2) of the row (its art and the diagram), as the original copied it out of the tab's
    // picture.
    MCLogBlockPort square(port->Frame(), 9, 0x191, 0x1e, 0x1e, false);
    VfxPaneCopy(GlobalLogPtr->InvBlockPort->Frame(), 3, 2, square.Frame(), 0, 0, -1);

    if (DiagramPort != nullptr)
    {
        DiagramPort->CopyTo(square.Frame(), 2, 0, true);
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

auto MCMechInventoryBlock::DeleteDiagram() -> void
{
    DiagramPort.reset();
}

auto MCMechInventoryBlock::OfferSale() -> void
{
    if (Mech->Required != 0)
    {
        return;
    }

    std::string title =
        std::format("{:.0f} Ton {} 'Mech", Mech->CurTonnage, LoadGameString(WeightClassString(Mech->CurTonnage), 0xfe));
    Mech->CalcMechCost(0);
    int32_t price = SalePrice(Mech->ResourcePoints);
    // The dialog shows a copy of the diagram over the store's colour.
    MCLogPort picture;
    picture.Init(DiagramPort->Width(), DiagramPort->Height());
    VfxPaneWipe(picture.Frame(), 0x10);
    DiagramPort->CopyTo(picture.Frame(), 2, 0, true);
    OpenPurchaseDialog(1, -price, 1, Mech->FileName != nullptr ? Mech->FileName : "", title, &picture,
                       [this](int32_t result, int32_t) { OnSellConfirmed(result); });
}

auto MCMechInventoryBlock::OnSellConfirmed(int32_t result) -> void
{
    MCLogMech* sold = Mech;

    if (result == 0)
    {
        sold->Assigned = 0;
        GlobalLogPtr->ReorderMechs();
        GlobalLogPtr->PurchaseScreen->CreateMechInvBlock();
        GlobalLogPtr->PurchaseScreen->SetUpMechInv(false, true);
        return;
    }

    MCInventoryList* spares = GlobalLogPtr->ComponentInventory;

    for (MCLogInventoryItem* item = sold->Inventory->Items; item != nullptr; item = item->Next)
    {
        uint8_t masterID = item->MasterID;
        MCComponentForm form = MasterComponentList[masterID].Form;

        if (!IsWeapon(form) && !IsEquipment(form) && form != MCComponentForm::Jammer)
        {
            continue;
        }

        // Every undamaged copy goes back to the spare components.
        for (MCLogInventoryStat* stat = item->Stats; stat != nullptr; stat = stat->Next)
        {
            if (stat->Hits != 0)
            {
                continue;
            }

            MCLogInventoryItem* stockItem = spares->GetItemInfo(spares->GetIndexFromMasterID(masterID));

            if (stockItem == nullptr)
            {
                stockItem = MCCompInventoryBlock::AddSpare(masterID);
            }

            ++stockItem->Count;
        }
    }

    GlobalLogPtr->PurchaseScreen->CreateCompInvBlock();
    int32_t index = GlobalLogPtr->ForceMechList->GetMechIndex(sold);
    GlobalLogPtr->ForceMechList->RemoveMech(static_cast<uint8_t>(index));
    GlobalLogPtr->ReorderMechs();
    ResourcePoints -= GlobalLogPtr->PurchaseDialog->UnitCost;
}

auto MCMechInventoryBlock::HandleEvent(MCGuiEvent* event) -> void
{
    if (!PreHandleEvent(MechDrag, event))
    {
        return;
    }

    MCLogInvScreen* screen = Screen();

    if (MechDrag.Idle())
    {
        // The info block: the row's diagram, tonnage, classes, speed and description.
        screen->DrawBlankInvInfoBlock(-1);
        PrepareInfoDescription(Mech->Description);
        screen->ShowInfo(MCInvInfoBox::Kind::Mech, this);

        if (event->Type == 1)
        {
            // Left button down: drag the mech (it joins the force while dragged).
            PlayLogSound(0x35);
            GuiSystem()->SetCursorVisible(false);
            GuiSystem()->Grab(this);
            MechDrag.Dragging = true;
            MakeDragIcon(MechDrag, event);
            Mech->Assigned = 1;
            GlobalLogPtr->ReorderMechs();
            screen->CreateMechInvBlock();
            screen->SetUpMechInv(false, false);
            MCDragIcon::Current()->Raise();
        }
    }

    // The force is full at 16 units.
    auto forceFull = []
    { return GlobalLogPtr->ForceMechList->NumMechs + GlobalLogPtr->ForceVehicleList->NumVehicles > 0xf; };

    // Back to the inventory.
    auto backToInventory = [&]
    {
        Mech->Assigned = 0;
        GlobalLogPtr->ReorderMechs();
        screen->CreateMechInvBlock();
        screen->SetUpMechInv(false, true);
    };

    // Into the force on the repair screen.
    auto joinForce = [&]
    {
        BumpDeploySlots(false);
        GlobalLogPtr->RepairScreen->UnitPane->AddChild(Mech->RepairBlock);
        GlobalLogPtr->RepairScreen->AddMechToList(Mech);
        GlobalLogPtr->RepairScreen->SelectMech(Mech);
    };

    switch (event->Type)
    {
        case 3:
        {
            // Right button down: pick the mech up.
            if (MechDrag.Dragging)
            {
                break;
            }

            MechDrag.Carrying = true;
            PlayLogSound(0x35);
            GuiSystem()->SetCursorVisible(false);
            GuiSystem()->Grab(this);
            MakeDragIcon(MechDrag, event);
            MCDragIcon::Current()->Raise();
            break;
        }

        case 4:
        {
            // Left button up: drop the dragged mech.
            if (MechDrag.Carrying || !MechDrag.Dragging)
            {
                break;
            }

            GuiSystem()->SetCursorVisible(true);
            GuiSystem()->Release();
            MechDrag.Dragging = false;
            MCDragIcon::Remove();
            uint32_t sample = 0x33;

            if (OnRepairScreen())
            {
                DrawDropArt(screen, 0);

                if (OverPaneInside(screen->UnitPane, event))
                {
                    if (forceFull())
                    {
                        PlayLogSound(0x33);
                        backToInventory();
                        ShowLogMessage(0x285, false);
                    }
                    else
                    {
                        PlayLogSound(0x34);
                        joinForce();
                    }
                    break;
                }

                if (OverPaneInside(screen->InventoryPane, event))
                {
                    sample = 0x34;
                }
            }
            else
            {
                if (OverPaneInside(screen->UnitPane, event))
                {
                    // Dropped on the store: offer to sell it.
                    PlayLogSound(0x34);
                    OfferSale();
                    break;
                }

                if (OverPaneInside(screen->InventoryPane, event))
                {
                    sample = 0x34;
                }
            }

            PlayLogSound(sample);
            backToInventory();
            break;
        }

        case 6:
        {
            // Right button up: put down the carried mech.
            if (GuiSystem()->GrabbedObject() == nullptr || MechDrag.Dragging)
            {
                break;
            }

            PlayLogSound(0x34);
            MechDrag.Carrying = false;
            GuiSystem()->SetCursorVisible(true);
            GuiSystem()->Release();
            MCDragIcon::Remove();
            Mech->Assigned = 1;
            GlobalLogPtr->ReorderMechs();
            screen->CreateMechInvBlock();
            screen->SetUpMechInv(false, true);

            if (OnRepairScreen())
            {
                if (!forceFull())
                {
                    DrawDropArt(screen, 0);
                    joinForce();
                }
                else
                {
                    PlayLogSound(0x33);
                    backToInventory();
                    ShowLogMessage(0x285, true);
                }
                break;
            }

            OfferSale();
            break;
        }

        case 7:
        {
            // Mouse move: the dragged icon follows; otherwise the ticker shows the row's help.
            if (!MechDrag.Dragging)
            {
                if (event->Key == 0)
                {
                    GlobalLogPtr->Ticker->SetString(LoadGameString(OnPurchaseScreen() ? 0x2d : 0x31, 0xfe));
                }
            }
            else
            {
                MechDrag.X = event->X - 0xf;
                MechDrag.Y = event->Y - 0xf;
                MCDragIcon::Current()->MoveTo(MechDrag.X, MechDrag.Y, false);
            }
            break;
        }

        default:
            break;
    }
}
