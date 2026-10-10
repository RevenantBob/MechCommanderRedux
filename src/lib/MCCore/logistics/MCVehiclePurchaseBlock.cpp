#include "stdafx.h"
#include "logistics/MCVehiclePurchaseBlock.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiGlobals.h"
#include "logistics/MCLogRows.h"
#include "logistics/MCPurVehicleList.h"
#include "logistics/MCPurchaseDlg.h"
#include "logistics/MCPurchaseScreen.h"
#include "logistics/MCStoreRow.h"
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

MCVehiclePurchaseBlock::~MCVehiclePurchaseBlock()
{
    MCVehiclePurchaseBlock::Destroy();
}

auto MCVehiclePurchaseBlock::Init(MCPurVehicle* newPurVehicle) -> void
{
    PicturePort.reset();
    PurVehicle = newPurVehicle;
    MCLogObject::InitWithoutPort(0, 0, 0x19a, 0x70);
    const MCPurVehicleData* data = PurVehicle->Data.get();
    NameIndex = data->NameIndex;
    WeightClassText = LoadGameString(WeightClassString(data->CurTonnage), 0xf);
    ArmorText = LoadGameString(ArmorClassString(data->ArmorTonnage), 0xf);
}

auto MCVehiclePurchaseBlock::Destroy() -> void
{
    PurVehicle = nullptr;
    WeightClassText.clear();
    ArmorText.clear();
    PicturePort.reset();
    MCLogObject::Destroy();
}

auto MCVehiclePurchaseBlock::OnBuyConfirmed(int32_t result, int32_t quantity) -> void
{
    if (result == 0)
    {
        return;
    }

    MCPurVehicleData* data = PurVehicle->Data.get();

    for (int32_t count = quantity; count > 0; --count)
    {
        GlobalLogPtr->VehicleList->AddVehicle(data->FileName.data(), 0, 1, 1);
    }

    GlobalLogPtr->ReorderVehicles();
    GlobalLogPtr->PurchaseScreen->CreateVhclInvBlock();
    GlobalLogPtr->PurchaseScreen->SetUpVhclInv(true, true);
    data->NumAvailable -= quantity;
    DrawBackground(Row);
    ResourcePoints -= GlobalLogPtr->PurchaseDialog->UnitCost * quantity;
    CheckNumUnits();
}

auto MCVehiclePurchaseBlock::HandleEvent(MCGuiEvent* event) -> void
{
    int32_t localX = event->X - GlobalX();
    int32_t localY = event->Y - GlobalY();

    if (!MCStoreRow::PreHandleEvent(this, VehicleDrag, event))
    {
        return;
    }

    int32_t type = event->Type;

    switch (type)
    {
        case 1:
        {
            if (VehicleDrag.Carrying)
            {
                return;
            }

            [[fallthrough]];
        }
        case 3:
        {
            if ((VehicleDrag.Dragging && type == 3) || CheckMaxUnits())
            {
                return;
            }

            if ((localX < 0x94 || localX > 0xc9 || localY < 5 || localY > 0x15) && OnRow(this, event) &&
                PurVehicle->Data->NumAvailable != 0)
            {
                // Pick the vehicle up (left button drags, right button carries).
                (type == 1 ? VehicleDrag.Dragging : VehicleDrag.Carrying) = true;
                PlayLogSound(0x35);
                GuiSystem()->SetCursorVisible(false);
                GuiSystem()->Grab(this);
                VehicleDrag.X = event->X - 0x10;
                VehicleDrag.Y = event->Y - 0x10;
                MCStoreRow::MakeDragIcon(VehicleDrag, [this](MCLogPort* surface) { OnBeginDrag(surface); });
                return;
            }
            break;
        }

        case 4:
        {
            if (VehicleDrag.Carrying)
            {
                return;
            }

            [[fallthrough]];
        }
        case 6:
        {
            if (VehicleDrag.Dragging && type == 6)
            {
                return;
            }

            VehicleDrag.Carrying = false;

            if (GuiSystem()->GrabbedObject() == nullptr)
            {
                return;
            }

            GuiSystem()->Release();
            GuiSystem()->SetCursorVisible(true);
            VehicleDrag.Dragging = false;
            MCDragIcon::Remove();

            if (type != 6 && !MCStoreRow::OverInventory(event))
            {
                if (MCStoreRow::OverStore(event))
                {
                    PlayLogSound(0x34);
                    return;
                }
                break;
            }

            // Buy it.
            MCPurVehicleData* data = PurVehicle->Data.get();

            if (data->Cost > ResourcePoints)
            {
                PlayLogSound(0x33);
                ShowLogMessage(0x4d);
                return;
            }

            PlayLogSound(0x34);
            OpenPurchaseDialog(6, data->Cost, MaxPurchase(data->NumAvailable), data->Name, {}, PicturePort.get(),
                               [this](int32_t result, int32_t quantity) { OnBuyConfirmed(result, quantity); });
            return;
        }

        case 7:
        {
            if (VehicleDrag.Dragging)
            {
                VehicleDrag.Y = event->Y - 0xf;
                VehicleDrag.X = event->X - 0xf;
                MCDragIcon::Current()->MoveTo(VehicleDrag.X, VehicleDrag.Y, false);
                return;
            }

            if (event->Key == 0)
            {
                GlobalLogPtr->Ticker->SetString(LoadGameString(0x30, 0xfe));
            }

            return;
        }

        default:
            return;
    }

    PlayLogSound(0x33);
}

auto MCVehiclePurchaseBlock::DrawBackground(int32_t) -> void
{
    MCPurVehicleData* data = PurVehicle->Data.get();

    if (data->NumAvailable != 0)
    {
        // The diagram, kept for the purchase dialog (the original parked the row's picture in picturePort first).
        PicturePort = std::make_unique<MCLogPort>();
        PicturePort->Init(0x1e, 0x1e);
        VfxPaneWipe(PicturePort->Frame(), 0x10);

        for (int32_t location = 0; location < 5; ++location)
        {
            AGShapeDraw(PicturePort->Frame(), GlobalLogPtr->VehicleIconShapes[data->NameIndex].Data(), location, 4, 0);
        }
    }

    PrepareInfoDescription(data->Description);
}

auto MCVehiclePurchaseBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen.get();
    std::unique_ptr<MCLogBlockPort> work = RowPicture(screen->VehicleTabPort.get(), port, top, false);
    MCPurVehicleData* data = PurVehicle->Data.get();
    WriteText(YellowDropFont, work.get(), 0x52, 0x24,
              std::format("{:.0f} {}", data->CurTonnage, LoadGameString(0x6e, 0xfe)));
    WriteText(YellowDropFont, work.get(), 0x52, 0x2d, WeightClassText);
    WriteText(YellowDropFont, work.get(), 0xa7, 0x24, ArmorText);
    WriteText(YellowDropFont, work.get(), 0x52, 0x36, std::format("{} m/s", data->MaxMoveSpeed));
    DrawInventoryList(data->Inventory.get(), work.get());
    std::string stock;

    if (data->NumAvailable == 0)
    {
        // Sold out: the "sold out" name art, a blank picture and the sold-out mark.
        CopyArt(work.get(), 5, 4, std::format("lspfdv{:02}.tga", data->NameIndex));
        WipeBox(work.get(), 0xed, 6, 0x4b, 100, 0x10);
        AGShapeDraw(work->Frame(), GlobalLogPtr->VehicleRepShapes[data->NameIndex].Data(), 6, 0xed, 6);
        stock = "0";
    }
    else
    {
        int32_t index = data->NameIndex;
        CopyArt(work.get(), 5, 4, std::format("lspflv{:02}.tga", index));
        // The picture.
        MCLogBlockPort picture(work->Frame(), 0xed, 6, 0x4b, 100, true);
        VfxPaneWipe(picture.Frame(), 0x10);
        VfxShapeLookaside(GlobalLogPtr->ShapeLookaside[0].data());

        for (int32_t shape = 0; shape < 5; ++shape)
        {
            VfxShapeTranslateDraw(picture.Frame(), GlobalLogPtr->VehicleRepShapes[index].Data(), shape, 0, 0);
        }

        for (int32_t location = 0; location < 5; ++location)
        {
            AGShapeDraw(work->Frame(), GlobalLogPtr->VehicleIconShapes[index].Data(), location, 9, 0x22);
        }

        // A negative stock shows the "unlimited" mark.
        stock = data->NumAvailable < 1 ? LoadGameString(0x385, 0xfe) : std::format("{}", data->NumAvailable);
    }

    WriteText(YellowDropFont, work.get(), 0x25, 0x12, stock);
    WriteText(YellowDropFont, work.get(), 0x52, 0x12, std::format("{}", data->Cost));
    DrawInfoDescription(work.get(), 0xc6, 0x26, data->Description, 6, 0x43);
}

auto MCVehiclePurchaseBlock::OnBeginDrag(MCLogPort* surface) -> void
{
    // The square at (6, 0x21) of the row, over the store's colour 0x10.
    VfxPaneWipe(surface->Frame(), 0x10);
    MCDragIcon::DrawFrom(surface, 6, 0x21, [this](MCLogPort* port) { DrawRow(port, 0); });
}

auto MCVehiclePurchaseBlock::SetBar() -> void
{
}
