#include "stdafx.h"
#include "logistics/MCMechPurchaseBlock.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiGlobals.h"
#include "logistics/MCLogRows.h"
#include "logistics/MCPurMechList.h"
#include "logistics/MCPurchaseDlg.h"
#include "logistics/MCPurchaseScreen.h"
#include "logistics/MCStoreRow.h"
#include "logistics/MCTicker.h"
#include "logistics/MCUnitLimits.h"
#include "main/MCLogistics.h"
#include "main/MCGameStrings.h"
#include "object/MCMasterComponent.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>The mech rows' drag.</summary>
    MCDragState MechDrag;

    /// <summary>The order the 8 body-location shapes of the mech diagram are drawn in.</summary>
    constexpr std::array<int32_t, 8> BodyTrans = {7, 6, 4, 5, 0, 1, 2, 3};

    /// <summary>The name art of each variant (A, W, J) in stock, and sold out.</summary>
    constexpr std::array<std::string_view, 3> VariantNameArt = {"lspflma", "lspflmw", "lspflmj"};
    constexpr std::array<std::string_view, 3> SoldOutNameArt = {"lspfdma", "lspfdmw", "lspfdmj"};

    /// <summary>
    /// The variant buttons (A, W, J): their x, and their pictures when shown, in stock and sold out
    /// (<c>lspbim&lt;n&gt;</c>).
    /// </summary>
    struct VariantButton
    {
        int32_t X;
        int32_t Shown;
        int32_t InStock;
        int32_t SoldOut;
    };

    constexpr std::array<VariantButton, 3> VariantButtons = {{{0x9a, 4, 1, 7}, {0xac, 5, 2, 8}, {0xbe, 6, 3, 9}}};
}

MCMechPurchaseBlock::~MCMechPurchaseBlock()
{
    MCMechPurchaseBlock::Destroy();
}

auto MCMechPurchaseBlock::Init(MCPurMech* newPurMech) -> void
{
    PicturePort.reset();
    DiagramPort.reset();
    PurMech = newPurMech;
    MCLogObject::InitWithoutPort(0, 0, 0x19a, 0x70);
    NameIndex = PurMech->Variants[static_cast<size_t>(CurVariant)]->NameIndex;
}

auto MCMechPurchaseBlock::Destroy() -> void
{
    PurMech = nullptr;
    PicturePort.reset();
    DiagramPort.reset();
    WeightClassText.clear();
    ArmorText.clear();
    InternalText.clear();
    MCLogObject::Destroy();
}

auto MCMechPurchaseBlock::OnBuyConfirmed(int32_t result, int32_t quantity) -> void
{
    if (result == 0 || quantity == 0)
    {
        return;
    }

    MCPurMechData* data = PurMech->Variants[static_cast<size_t>(CurVariant)].get();

    for (int32_t count = quantity; count > 0; --count)
    {
        GlobalLogPtr->MechList->AddMech(data->FileName.data(), 0, 1, 1);
    }

    GlobalLogPtr->ReorderMechs();
    GlobalLogPtr->PurchaseScreen->CreateMechInvBlock();
    GlobalLogPtr->PurchaseScreen->SetUpMechInv(true, true);
    data->NumAvailable -= quantity;
    DrawBackground(Row);
    ResourcePoints -= GlobalLogPtr->PurchaseDialog->UnitCost * quantity;
    CheckNumUnits();
}

auto MCMechPurchaseBlock::HandleEvent(MCGuiEvent* event) -> void
{
    // The position within the row.
    int32_t localX = event->X - GlobalX();
    int32_t localY = event->Y - GlobalY();

    if (OnRepairScreen())
    {
        return;
    }

    bool showHelp = false;

    if (Parent == nullptr)
    {
        showHelp = !MechDrag.Dragging;
    }
    else if (!MechDrag.Dragging)
    {
        if (!MechDrag.Carrying && (event->Type == 8 || event->Type == 9))
        {
            Parent->HandleEvent(event);
            return;
        }

        showHelp = true;
    }

    if (showHelp && event->Key == 0)
    {
        // The ticker explains the variant buttons, the battle rating bar or the row.
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

        GlobalLogPtr->Ticker->SetString(LoadGameString(id, 0xfe));
    }

    int32_t type = event->Type;

    switch (type)
    {
        case 1:
        {
            if (MechDrag.Carrying)
            {
                return;
            }

            [[fallthrough]];
        }
        case 3:
        {
            if (MechDrag.Dragging || CheckMaxUnits())
            {
                break;
            }

            if (localX > 0x9a && localX < 0xd0 && localY > 5 && localY < 0x16)
            {
                // A variant button.
                CurVariant = localX < 0xac ? 0 : localX < 0xbe ? 1 : 2;
                DrawBackground(Row);
                PlayLogSound(0xf);
                return;
            }

            if (OnRow(this, event) && PurMech->Variants[static_cast<size_t>(CurVariant)]->NumAvailable != 0)
            {
                // Pick the mech up (left button drags, right button carries).
                (type == 1 ? MechDrag.Dragging : MechDrag.Carrying) = true;
                PlayLogSound(0x35);
                GuiSystem()->SetCursorVisible(false);
                GuiSystem()->Grab(this);
                MechDrag.X = event->X - 0x10;
                MechDrag.Y = event->Y - 0x10;
                MCStoreRow::MakeDragIcon(MechDrag, [this](MCLogPort* surface) { OnBeginDrag(surface); });
                return;
            }

            PlayLogSound(0x33);
            return;
        }

        case 4:
        {
            if (MechDrag.Carrying)
            {
                return;
            }

            [[fallthrough]];
        }
        case 6:
        {
            if (MechDrag.Dragging && type == 6)
            {
                return;
            }

            if (GuiSystem()->GrabbedObject() == nullptr)
            {
                return;
            }

            GuiSystem()->SetCursorVisible(true);
            GuiSystem()->Release();
            MechDrag.Carrying = false;
            MechDrag.Dragging = false;
            MCDragIcon::Remove();

            if (type != 6 && !MCStoreRow::OverInventory(event))
            {
                // Dropped back on the store: nothing happens.
                PlayLogSound(MCStoreRow::OverStore(event) ? 0x34 : 0x33);
                return;
            }

            // Buy it.
            MCPurMechData* data = PurMech->Variants[static_cast<size_t>(CurVariant)].get();

            if (ResourcePoints < data->Cost)
            {
                PlayLogSound(0x33);
                ShowLogMessage(0x4d);
                return;
            }

            PlayLogSound(0x34);
            std::string title = std::format("{:.0f} Ton {} 'Mech", data->CurTonnage,
                                            LoadGameString(WeightClassString(data->CurTonnage), 0xfe));
            OpenPurchaseDialog(0, data->Cost, MaxPurchase(data->NumAvailable), data->Name, title, DiagramPort.get(),
                               [this](int32_t result, int32_t quantity) { OnBuyConfirmed(result, quantity); });
            break;
        }

        case 7:
        {
            if (MechDrag.Dragging)
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

auto MCMechPurchaseBlock::Draw() -> void
{
}

auto MCMechPurchaseBlock::DrawBackground(int32_t) -> void
{
    MCPurMechData* data = PurMech->Variants[static_cast<size_t>(CurVariant)].get();

    if (PicturePort == nullptr)
    {
        // The mech's picture: its shadow (shapes 0xb..0x12 through the shadow table), then the mech (0..7).
        PicturePort = std::make_unique<MCLogPort>();
        PicturePort->Init(0x4b, 100);
        VfxPaneWipe(PicturePort->Frame(), 0x10);
        VfxShapeLookaside(GlobalLogPtr->ShapeLookaside[5].data());

        for (int32_t shape = 0; shape < 8; ++shape)
        {
            VfxShapeTranslateDraw(PicturePort->Frame(), GlobalLogPtr->MechRepShapes[data->NameIndex].Data(),
                                  shape + 0xb, 0, 0);
        }

        VfxShapeLookaside(GlobalLogPtr->ShapeLookaside[0].data());

        for (int32_t shape = 0; shape < 8; ++shape)
        {
            VfxShapeTranslateDraw(PicturePort->Frame(), GlobalLogPtr->MechRepShapes[data->NameIndex].Data(), shape, 0,
                                  0);
        }
    }

    if (DiagramPort == nullptr)
    {
        DiagramPort = std::make_unique<MCLogPort>();
        DiagramPort->Init(0x1e, 0x1e);
        VfxPaneWipe(DiagramPort->Frame(), 0x10);

        for (int32_t location : BodyTrans)
        {
            AGShapeDraw(DiagramPort->Frame(), GlobalLogPtr->MechIconShapes[data->NameIndex].Data(), location, 3, 0);
        }
    }

    // The class texts.
    WeightClassText = LoadGameString(WeightClassString(data->CurTonnage), 0xfe);
    ArmorText = LoadGameString(ArmorClassString(data->ArmorTonnage), 0xfe);
    int32_t internals = 0;

    for (uint8_t points : data->CurInternalStructure)
    {
        internals += points;
    }

    uint32_t internalId = internals < 0x24   ? 100u
                          : internals < 0x38 ? 0x4fu
                          : internals < 0x51 ? 0x65u
                          : internals < 0x79 ? 0x51u
                                             : 0x66u;
    InternalText = LoadGameString(internalId, 0xfe);
    SetBar();

    if (data->Description.empty() && data->DescIndex > -1)
    {
        data->LoadDescription(data->DescIndex);
    }

    PrepareInfoDescription(data->Description);
}

auto MCMechPurchaseBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen.get();
    std::unique_ptr<MCLogBlockPort> row = RowPicture(screen->MechTabPort.get(), port, top, false);
    MCPurMechData* data = PurMech->Variants[static_cast<size_t>(CurVariant)].get();
    CopyArt(row.get(), 5, 4, std::format("lspflma{:02}.tga", data->NameIndex));
    CopyArt(row.get(), 0x13a, 6, std::format("lscdsm{:02}.tga", data->NameIndex));

    if (PicturePort != nullptr)
    {
        PicturePort->CopyTo(row->Frame(), 0xed, 6, true);
    }

    WriteText(YellowDropFont, row.get(), 0x51, 0x23,
              std::format("{:.0f} {}", data->CurTonnage, LoadGameString(0x6e, 0x1e)));
    WriteText(YellowDropFont, row.get(), 0x51, 0x2c, WeightClassText);
    WriteText(YellowDropFont, row.get(), 0xa7, 0x23, ArmorText);
    WriteText(YellowDropFont, row.get(), 0xa7, 0x2c, InternalText);
    std::string text = std::format("{} m/s", data->MaxRunSpeed);
    WriteText(YellowDropFont, row.get(), 0x51, 0x35, text);

    // The weapons and equipment, and the jump jets' rating.
    int32_t jumpJets = 0;

    for (const std::unique_ptr<MCLogInventoryItem>& item : data->Inventory->Items)
    {
        if (MasterComponentList[item->MasterID].Form == MCComponentForm::JumpJet)
        {
            jumpJets = item->Count;
        }
    }

    if (std::optional<std::string> last = DrawInventoryList(data->Inventory.get(), row.get()))
    {
        text = *last;
    }

    // An unlisted rating (jump jets beyond 8) shows the line written last (an item, or the speed).
    if (jumpJets == 0)
    {
        text = LoadGameString(0x6c, 0xfe);
    }
    else
    {
        switch (jumpJets * 2 / 3)
        {
            case 0:
            case 1:
                text = LoadGameString(0x56, 0xfe);
                break;
            case 2:
            case 3:
                text = LoadGameString(0x55, 0xfe);
                break;
            case 4:
                text = LoadGameString(0x50, 0xfe);
                break;
            case 5:
                text = LoadGameString(0x6d, 0xfe);
                break;
            default:
                break;
        }
    }

    WriteText(YellowDropFont, row.get(), 0xa7, 0x35, text);

    // The battle rating bar: 80 pixels at 18010, bottom at y 0x58.
    MCPane* frame = row->Frame();
    int32_t bar = static_cast<int32_t>(static_cast<double>(data->BattleRating) * 0x1.d1c6674f499a1p-15 * 80.0);
    int32_t barTop = 0x58 - bar;
    int32_t topLine = 0x57 - bar;
    VfxLineDraw(frame, 0xdb, 0x58, 0xdf, 0x58, 0xe5);
    VfxLineDraw(frame, 0xda, topLine, 0xe0, topLine, 0xe3);
    VfxLineDraw(frame, 0xda, 0x57, 0xda, barTop, 0xe3);
    VfxLineDraw(frame, 0xe0, 0x57, 0xe0, barTop, 0xe5);

    for (int32_t x = 0xdb; x <= 0xdf; ++x)
    {
        VfxLineDraw(frame, x, 0x57, x, barTop, 0xe4);
    }

    VfxLineDraw(frame, 0xdb, 0x56 - bar, 0xdf, 0x56 - bar, 0x10);
    VfxPixelWrite(frame, 0xdf, 0x57, 0xe5);
    VfxPixelWrite(frame, 0xdb, barTop, 0xe3);
    VfxPixelWrite(frame, 0xda, topLine, 0x10);
    VfxPixelWrite(frame, 0xe0, topLine, 0x10);
    row.reset();

    // The variant buttons (A, W, J, drawn A, J, W), and the shown variant's name art over the row.
    const size_t shownVariant = static_cast<size_t>(CurVariant);

    for (size_t variant : {size_t{0}, size_t{2}, size_t{1}})
    {
        const VariantButton& button = VariantButtons[variant];
        int32_t art = button.InStock;

        if (variant == shownVariant)
        {
            CopyArt(port, 5, top + 4,
                    std::format("{}{:02}.tga", VariantNameArt[variant], PurMech->Variants[variant]->NameIndex));
            art = button.Shown;
        }
        else if (PurMech->Variants[variant]->NumAvailable == 0)
        {
            art = button.SoldOut;
        }

        CopyArt(port, button.X, top + 5, std::format("lspbim{:02}.tga", art));
    }

    MCPurMechData* shown = PurMech->Variants[shownVariant].get();

    if (shown->NumAvailable != 0)
    {
        if (DiagramPort != nullptr)
        {
            VfxPaneCopy(DiagramPort->Frame(), 0, 0, port->Frame(), 7, top + 0x22, -1);
        }
    }
    else
    {
        // Sold out: the "sold out" name art, a blank diagram and picture, and the sold-out mark.
        CopyArt(port, 5, top + 4, std::format("{}{:02}.tga", SoldOutNameArt[shownVariant], shown->NameIndex));
        WipeBox(port, 7, top + 0x22, 0x1e, 0x1e, 0x10);
        WipeBox(port, 0xed, top + 5, 0x4b, 100, 0x10);
        AGShapeDraw(port->Frame(), GlobalLogPtr->MechRepShapes[shown->NameIndex].Data(), 0x13, 0xed, top + 6);
    }

    // Stock and price.
    const std::string stock = shown->NumAvailable < 0
                                  ? MCFormatPrintf(LoadGameString(0x385, 0xfe).c_str(), shown->NumAvailable)
                                  : std::format("{}", shown->NumAvailable);
    WriteText(YellowDropFont, port, 0x29, top + 0x12, stock);
    WriteText(YellowDropFont, port, 0x52, top + 0x12, std::format("{}", shown->Cost));
    DrawInfoDescription(port, 0xc6, 0x25, shown->Description, 6, top + 0x44);
}

auto MCMechPurchaseBlock::OnBeginDrag(MCLogPort* surface) -> void
{
    // The square at (6, 0x21) of the row, over the store's colour 0x10.
    VfxPaneWipe(surface->Frame(), 0x10);
    MCDragIcon::DrawFrom(surface, 6, 0x21, [this](MCLogPort* port) { DrawRow(port, 0); });
}

auto MCMechPurchaseBlock::SetBar() -> void
{
}
