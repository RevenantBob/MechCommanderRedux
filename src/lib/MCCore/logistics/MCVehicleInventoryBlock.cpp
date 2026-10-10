#include "stdafx.h"
#include "logistics/MCVehicleInventoryBlock.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiGlobals.h"
#include "gui/MCScrollPane.h"
#include "logistics/MCLogRows.h"
#include "logistics/MCVehicleRepairBlock.h"
#include "logistics/MCPurchaseDlg.h"
#include "logistics/MCPurchaseScreen.h"
#include "logistics/MCRepairScreen.h"
#include "logistics/MCTicker.h"
#include "logistics/MCUnitLimits.h"
#include "main/MCLogistics.h"
#include "main/main.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>The vehicle rows' drag.</summary>
    MCDragState VehicleDrag;
}

MCVehicleInventoryBlock::~MCVehicleInventoryBlock()
{
    MCVehicleInventoryBlock::Destroy();
}

auto MCVehicleInventoryBlock::Init(MCLogVehicle* logVehicle) -> void
{
    Vehicle = logVehicle;
    InitRow();
    ListIndex = Vehicle->NameIndex;
    WeightClassText = LoadGameString(WeightClassString(Vehicle->CurTonnage), 0xfe);
    ArmorText = LoadGameString(ArmorClassString(Vehicle->ArmorTonnage), 0xf);
}

auto MCVehicleInventoryBlock::Destroy() -> void
{
    Vehicle = nullptr;
    WeightClassText.clear();
    ArmorText.clear();
    PicturePort.reset();
    MCInventoryBlock::Destroy();
}

auto MCVehicleInventoryBlock::Draw() -> void
{
}

auto MCVehicleInventoryBlock::OfferSale() -> bool
{
    if (Vehicle->Required != 0)
    {
        return false;
    }

    PlayLogSound(0x34);
    int32_t price = SalePrice(Vehicle->VehicleResourcePoints);
    OpenPurchaseDialog(7, -price, 1, Vehicle->FileName, {}, PicturePort.get(),
                       [this](int32_t result, int32_t) { OnSellConfirmed(result); });
    return true;
}

auto MCVehicleInventoryBlock::OnSellConfirmed(int32_t result) -> void
{
    MCLogVehicle* sold = Vehicle;

    if (result == 0)
    {
        sold->Assigned = 0;
        GlobalLogPtr->ReorderVehicles();
        GlobalLogPtr->PurchaseScreen->CreateVhclInvBlock();
        GlobalLogPtr->PurchaseScreen->SetUpVhclInv(false, true);
        return;
    }

    int32_t index = GlobalLogPtr->ForceVehicleList->GetVehicleIndex(sold);
    GlobalLogPtr->ForceVehicleList->RemoveVehicle(static_cast<uint8_t>(index));
    GlobalLogPtr->ReorderVehicles();
    ResourcePoints -= GlobalLogPtr->PurchaseDialog->UnitCost;
}

auto MCVehicleInventoryBlock::HandleEvent(MCGuiEvent* event) -> void
{
    if (!PreHandleEvent(VehicleDrag, event))
    {
        return;
    }

    MCLogInvScreen* screen = Screen();

    if (VehicleDrag.Idle())
    {
        // The info block: diagram, tonnage, classes, speed and description.
        screen->DrawBlankInvInfoBlock(-1);
        PrepareInfoDescription(Vehicle->Description);
        screen->ShowInfo(MCInvInfoBox::Kind::Vehicle, this);
    }

    auto forceFull = []
    { return GlobalLogPtr->ForceMechList->GetMechCount() + GlobalLogPtr->ForceVehicleList->GetVehicleCount() > 0xf; };

    auto backToInventory = [&]
    {
        Vehicle->Assigned = 0;
        GlobalLogPtr->ReorderVehicles();
        screen->CreateVhclInvBlock();
        screen->SetUpVhclInv(false, true);
    };

    // Into the force on the repair screen (the vehicle bumps the drop slots' vehicle indices).
    auto joinForce = [&]
    {
        PlayLogSound(0x34);
        GlobalLogPtr->RepairScreen->UnitPane->AddChild(Vehicle->RepairBlock.get());
        BumpDeploySlots(true);
        GlobalLogPtr->RepairScreen->AddVehicleToList(Vehicle);
        GlobalLogPtr->RepairScreen->SelectVehicle(Vehicle);
    };

    switch (event->Type)
    {
        case 1:
        {
            if (VehicleDrag.Carrying)
            {
                break;
            }

            [[fallthrough]];
        }
        case 3:
        {
            // Left button down drags the vehicle (it joins the force while dragged); right button down picks it up.
            if (VehicleDrag.Dragging)
            {
                break;
            }

            PlayLogSound(0x35);
            GuiSystem()->SetCursorVisible(false);
            GuiSystem()->Grab(this);
            MakeDragIcon(VehicleDrag, event);

            if (event->Type == 1)
            {
                VehicleDrag.Dragging = true;
                Vehicle->Assigned = 1;
                GlobalLogPtr->ReorderVehicles();
                screen->CreateVhclInvBlock();
                screen->SetUpVhclInv(false, false);
            }
            else
            {
                VehicleDrag.Carrying = true;
            }

            MCDragIcon::Current()->Raise();
            [[fallthrough]];
        }
        case 7:
        {
            // Mouse move: the dragged icon follows; otherwise the ticker shows the row's help.
            if (!VehicleDrag.Dragging)
            {
                if (event->Key == 0)
                {
                    GlobalLogPtr->Ticker->SetString(LoadGameString(OnPurchaseScreen() ? 0x2d : 0x47, 0xfe));
                }
            }
            else
            {
                VehicleDrag.Y = event->Y - 0xf;
                VehicleDrag.X = event->X - 0xf;
                MCDragIcon::Current()->MoveTo(VehicleDrag.X, VehicleDrag.Y, false);
            }
            break;
        }

        case 4:
        {
            // Left button up: drop the dragged vehicle.
            if (VehicleDrag.Carrying || !VehicleDrag.Dragging)
            {
                break;
            }

            GuiSystem()->SetCursorVisible(true);
            GuiSystem()->Release();
            VehicleDrag.Dragging = false;
            MCDragIcon::Remove();

            if (OnRepairScreen())
            {
                if (forceFull())
                {
                    PlayLogSound(0x33);
                    backToInventory();
                    ShowLogMessage(0x285, true);
                    break;
                }

                DrawDropArt(screen, 3);

                if (OverPaneInside(screen->UnitPane, event))
                {
                    joinForce();
                    break;
                }

                PlayLogSound(OverPaneInside(screen->InventoryPane, event) ? 0x34 : 0x33);
            }
            else if (OverPaneInside(screen->UnitPane, event))
            {
                // Dropped on the store: offer to sell it.
                if (OfferSale())
                {
                    break;
                }

                PlayLogSound(OverPaneInside(screen->InventoryPane, event) ? 0x34 : 0x33);
            }
            else
            {
                PlayLogSound(0x33);
            }

            backToInventory();
            break;
        }

        case 6:
        {
            // Right button up: put down the carried vehicle.
            if (VehicleDrag.Dragging)
            {
                break;
            }

            VehicleDrag.Carrying = false;

            if (GuiSystem()->GrabbedObject() == nullptr)
            {
                break;
            }

            GuiSystem()->SetCursorVisible(true);
            GuiSystem()->Release();
            MCDragIcon::Remove();
            Vehicle->Assigned = 1;
            GlobalLogPtr->ReorderVehicles();
            screen->CreateVhclInvBlock();
            screen->SetUpVhclInv(false, true);

            if (OnRepairScreen())
            {
                if (!forceFull())
                {
                    DrawDropArt(screen, 3);
                    joinForce();
                    break;
                }

                PlayLogSound(0x33);
                backToInventory();
                ShowLogMessage(0x285, true);
                break;
            }

            if (OfferSale())
            {
                break;
            }

            PlayLogSound(0x33);
            backToInventory();
            break;
        }

        default:
            break;
    }
}

auto MCVehicleInventoryBlock::DrawBackground() -> void
{
    if (PicturePort != nullptr)
    {
        return;
    }

    PicturePort = std::make_unique<MCLogPort>();
    PicturePort->Init(0x1c, 0x1e);
    VfxPaneWipe(PicturePort->Frame(), 0x10);

    for (int32_t location = 0; location < 5; ++location)
    {
        GlobalLogPtr->DrawVehicleBodyLoc(Vehicle, location, PicturePort.get(), 0, 0);
    }
}

auto MCVehicleInventoryBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    MCInventoryBlock::DrawRow(port, top);
    WriteText(YellowDropFont, port, 0x26, top + 7, Vehicle->FileName);
    WriteText(BlueDropFont, port, 0x26, top + 0x15,
              MCFormatPrintf(LoadGameString(0x53, 0xfe).c_str(), static_cast<double>(Vehicle->CurTonnage),
                             WeightClassText.c_str()));

    if (PicturePort != nullptr)
    {
        PicturePort->CopyTo(port->Frame(), 5, top + 2, true);
    }
}

auto MCVehicleInventoryBlock::DrawInfo(MCLogPort* port) -> void
{
    // The 0x1e square at (3, 2) of the row (its art and the diagram), as the original copied it out of the tab's
    // picture.
    MCLogBlockPort square(port->Frame(), 9, 0x191, 0x1e, 0x1e, false);
    VfxPaneCopy(GlobalLogPtr->InvBlockPort->Frame(), 3, 2, square.Frame(), 0, 0, -1);

    if (PicturePort != nullptr)
    {
        PicturePort->CopyTo(square.Frame(), 2, 0, true);
    }

    WriteText(YellowDropFont, port, 0x53, 0x193,
              std::format("{:.0f} {}", Vehicle->CurTonnage, LoadGameString(0x6e, 0xfe)));
    WriteText(YellowDropFont, port, 0x53, 0x19c, WeightClassText);
    WriteText(YellowDropFont, port, 0xa9, 0x193, ArmorText);
    WriteText(YellowDropFont, port, 0x53, 0x1a5, std::format("{} m/s", Vehicle->MaxMoveSpeed));
    DrawInfoDescription(port, 0xc3, 0x26, Vehicle->Description, 8, 0x1b3);
}
