#include "stdafx.h"
#include "logistics/MCPilotInventoryBlock.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiGlobals.h"
#include "gui/MCScrollPane.h"
#include "main/MCGamePaths.h"
#include "logistics/MCLogRows.h"
#include "logistics/MCMechRepairBlock.h"
#include "logistics/MCPurPilotList.h"
#include "logistics/MCPurchaseDlg.h"
#include "logistics/MCPurchaseScreen.h"
#include "logistics/MCRepairScreen.h"
#include "logistics/MCTicker.h"
#include "logistics/MCUnitLimits.h"
#include "main/MCLogistics.h"
#include "main/MCGameStrings.h"
#include "sound/MCSoundSystem.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>The pilot rows' drag.</summary>
    MCDragState PilotDrag;

    /// <summary>
    /// The row the next <see cref="MCPilotInventoryBlock::Init"/> takes. Never reset: the inventory screen renumbers the
    /// rows itself.
    /// </summary>
    int32_t NextPilotRow = 0;

    /// <summary>The rank name (string table 0x70..0x73); empty for a rank out of range.</summary>
    /// <remarks>Port fix: an out-of-range rank leaves the text empty (the original kept whatever was in the buffer).</remarks>
    std::string RankName(int32_t rank)
    {
        if (rank < 0 || rank > 3)
        {
            return {};
        }

        return LoadGameString(0x70 + static_cast<uint32_t>(rank), 0xfe);
    }
}

MCPilotInventoryBlock::~MCPilotInventoryBlock()
{
    MCPilotInventoryBlock::Destroy();
}

auto MCPilotInventoryBlock::Init(MCLogWarrior* logWarrior) -> void
{
    Mech = nullptr;
    Vehicle = nullptr;
    Warrior = logWarrior;
    InitRow();
    ListIndex = NextPilotRow;
    ++NextPilotRow;
    PortraitPort = std::make_unique<MCLogPort>();
    PortraitPort->Load(std::format("{}logart\\{}", ArtPath, Warrior->Picture));
}

auto MCPilotInventoryBlock::Destroy() -> void
{
    Warrior = nullptr;
    PortraitPort.reset();
    MCInventoryBlock::Destroy();
}

auto MCPilotInventoryBlock::Draw() -> void
{
    if (!Enabled)
    {
        DrawDisabled();
    }
}

auto MCPilotInventoryBlock::DrawBackground() -> void
{
    // On the repair screen a pilot can only go to the selected mech, and only when it has none.
    GreyedOut = false;

    if (OnRepairScreen())
    {
        MCLogMech* selected = GlobalLogPtr->RepairScreen->SelectedMech;
        GreyedOut = selected == nullptr || selected->PilotIndex >= 0;
    }

    MoveTo(0, WinHeight * ListIndex, false);
}

auto MCPilotInventoryBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    std::unique_ptr<MCLogBlockPort> row = RowPicture(GlobalLogPtr->InvBlockPort.get(), port, top, true);
    PortraitPort->CopyTo(row->Frame(), 3, 2, true);
    WriteText(YellowDropFont, row.get(), 0x26, 7, Warrior->Callsign);
    WriteText(BlueDropFont, row.get(), 0x26, 0x15,
              std::format("{} {}", RankName(Warrior->Rank), LoadGameString(0x287, 0xfe)));

    if (GreyedOut)
    {
        GlobalLogPtr->Darken(0, LogisticFadetable, row.get());
    }
}

auto MCPilotInventoryBlock::DrawInfo(MCLogPort* port) -> void
{
    GlobalLogPtr->DrawPilotSkillBar(Warrior, 3, 0x56, 0x192, 0, 0x36, WinHeight, port);
    GlobalLogPtr->DrawPilotSkillBar(Warrior, 0, 0x56, 0x19b, 0, 0x36, WinHeight, port);
    GlobalLogPtr->DrawPilotSkillBar(Warrior, 1, 0x56, 0x1a4, 0, 0x36, WinHeight, port);
    GlobalLogPtr->DrawPilotSkillBar(Warrior, 2, 0x56, 0x1ad, 0, 0x36, WinHeight, port);
    PortraitPort->CopyTo(port->Frame(), 9, 0x196, true);
    WriteText(YellowDropFont, port, 0x9c, 0x19a, RankName(Warrior->Rank));
    // One pip per point of health left.
    int32_t x = 0xf;

    for (int32_t pip = 0; static_cast<float>(pip) < Warrior->Health; ++pip, x += 3)
    {
        AGPixelWrite(port->Frame(), x, 0x192, 0xcf);
        AGPixelWrite(port->Frame(), x + 1, 0x192, 0xcf);
        AGPixelWrite(port->Frame(), x + 1, 0x193, 0xee);
        AGPixelWrite(port->Frame(), x, 0x193, 0xcf);
    }

    DrawInfoDescription(port, 0xc3, 0x22, Warrior->Description, 7, 0x1b8);
}

auto MCPilotInventoryBlock::OfferSale() -> void
{
    MCLogPort picture;
    picture.Load(std::format("{}logart\\{}", ArtPath, Warrior->Picture));
    int32_t price = SalePrice(GlobalLogPtr->PilotCosts[Warrior->Rank]);
    OpenPurchaseDialog(3, -price, 1, Warrior->Callsign, LoadGameString(0x5f, 0xfe), &picture,
                       [this](int32_t result, int32_t) { OnSellConfirmed(result); });
}

auto MCPilotInventoryBlock::OnSellConfirmed(int32_t result) -> void
{
    MCLogWarrior* warrior = Warrior;

    if (result == 0)
    {
        warrior->Assigned = 0;
        GlobalLogPtr->ShiftPilots(warrior->InventoryBlock->ListIndex, -1);
        GlobalLogPtr->ReorderWarriors();
        GlobalLogPtr->PurchaseScreen->CreatePilotInvBlock();
        GlobalLogPtr->PurchaseScreen->SetUpPilotInv(false, true);
        return;
    }

    int32_t row = warrior->InventoryBlock->ListIndex;
    SoundSystem()->PlayPilotSpeech(warrior->PilotAudio, 2);
    warrior->Sold = 1;
    GlobalLogPtr->PurPilotList->SetPilotStatus(warrior->DescIndex, MCPurPilotData::SoldBack);
    GlobalLogPtr->AssignedWarriorList->RemoveWarrior(static_cast<uint8_t>(warrior->Id));
    GlobalLogPtr->ShiftPilots(row, -1);
    GlobalLogPtr->ReorderWarriors();
    ResourcePoints -= GlobalLogPtr->PurchaseDialog->UnitCost;
}

auto MCPilotInventoryBlock::BoardMech(MCLogMech* mech, int32_t row) -> void
{
    MCPilotInventoryBlock* block = Warrior->InventoryBlock.get();
    block->Mech = mech;
    int32_t pilotRow = block->ListIndex;
    GlobalLogPtr->SetPilot(row, pilotRow);
    mech->RepairBlock->DrawBR(nullptr);
    GlobalLogPtr->SetPilot(row, pilotRow);
    Screen()->CreatePilotInvBlock();
    Screen()->SetUpPilotInv(false, true);
    SoundSystem()->PlayPilotSpeech(Warrior->PilotAudio, 2);
}

auto MCPilotInventoryBlock::BackToInventory(MCGuiEvent* event) -> void
{
    MCLogInvScreen* screen = Screen();
    Warrior->Assigned = 0;
    GlobalLogPtr->ShiftPilots(Warrior->InventoryBlock->ListIndex, -1);
    GlobalLogPtr->ReorderWarriors();
    screen->CreatePilotInvBlock();
    screen->SetUpPilotInv(false, true);
    PlayLogSound(OverPaneInside(screen->InventoryPane, event) ? 0x34 : 0x33);
}

auto MCPilotInventoryBlock::HandleEvent(MCGuiEvent* event) -> void
{
    if (!PreHandleEvent(PilotDrag, event))
    {
        return;
    }

    MCLogInvScreen* screen = Screen();

    if (PilotDrag.Idle())
    {
        // The info block: skills, portrait, rank, wounds and description.
        screen->DrawBlankInvInfoBlock(-1);
        PrepareInfoDescription(Warrior->Description);
        screen->ShowInfo(MCInvInfoBox::Kind::Pilot, this);
    }

    switch (event->Type)
    {
        case 1:
        {
            // Left button down: drag the pilot.
            if (GreyedOut || PilotDrag.Carrying)
            {
                break;
            }

            SoundSystem()->PlayPilotSpeech(Warrior->PilotAudio, 10);
            GuiSystem()->SetCursorVisible(false);
            GuiSystem()->Grab(this);
            PilotDrag.Dragging = true;
            MakeDragIcon(PilotDrag, event);
            Warrior->Assigned = 1;
            GlobalLogPtr->ReorderWarriors();
            GlobalLogPtr->ShiftPilots(Warrior->InventoryBlock->ListIndex, 1);
            screen->CreatePilotInvBlock();
            screen->SetUpPilotInv(false, false);
            MCDragIcon::Current()->Raise();
            break;
        }

        case 3:
        {
            // Right button down: pick the pilot up.
            if (GreyedOut || PilotDrag.Dragging)
            {
                break;
            }

            PilotDrag.Carrying = true;
            SoundSystem()->PlayPilotSpeech(Warrior->PilotAudio, 10);
            GuiSystem()->SetCursorVisible(false);
            GuiSystem()->Grab(this);
            MakeDragIcon(PilotDrag, event);
            MCDragIcon::Current()->Raise();
            break;
        }

        case 4:
        {
            // Left button up: drop the dragged pilot.
            if (PilotDrag.Carrying || GuiSystem()->GrabbedObject() == nullptr)
            {
                break;
            }

            GuiSystem()->SetCursorVisible(true);
            GuiSystem()->Release();
            PilotDrag.Dragging = false;
            MCDragIcon::Remove();
            DrawDropArt(screen, 1);

            if (OverPaneInside(screen->UnitPane, event))
            {
                if (OnPurchaseScreen())
                {
                    // Dropped on the store: offer to sell the pilot.
                    OfferSale();
                    break;
                }

                // Dropped on a mech: it must be the selected one.
                MCScrollPane* pane = screen->UnitPane;
                int32_t index = (event->Y - pane->GlobalY() + pane->GetScrollOffset()) / 0x70;

                if (index < pane->NumberOfChildren() && index < GlobalLogPtr->ForceMechList->GetMechCount())
                {
                    MCLogMech* target = nullptr;
                    GlobalLogPtr->ForceMechList->GetMechInfo(index, target);

                    if (target != nullptr && target == GlobalLogPtr->RepairScreen->SelectedMech)
                    {
                        BoardMech(target, index);
                        break;
                    }
                }
            }

            BackToInventory(event);
            break;
        }

        case 6:
        {
            // Right button up: put down the carried pilot.
            if (PilotDrag.Dragging)
            {
                break;
            }

            PilotDrag.Carrying = false;

            if (GuiSystem()->GrabbedObject() == nullptr)
            {
                break;
            }

            GuiSystem()->SetCursorVisible(true);
            GuiSystem()->Release();
            MCDragIcon::Remove();
            Warrior->Assigned = 1;
            GlobalLogPtr->ReorderWarriors();
            GlobalLogPtr->ShiftPilots(Warrior->InventoryBlock->ListIndex, 1);
            screen->CreatePilotInvBlock();
            screen->SetUpPilotInv(false, true);
            DrawDropArt(screen, 1);

            if (OnPurchaseScreen())
            {
                OfferSale();
                break;
            }

            // Onto the selected mech; with none, back to the inventory (the block is this one still: the lists were
            // only renumbered).
            if (MCLogMech* selected = GlobalLogPtr->RepairScreen->SelectedMech; selected != nullptr)
            {
                BoardMech(selected, selected->RepairBlock->SlotIndex);
                break;
            }

            BackToInventory(event);
            break;
        }

        case 7:
        {
            // Mouse move: the dragged icon follows; otherwise the ticker shows the row's help.
            if (!PilotDrag.Dragging)
            {
                if (event->Key == 0)
                {
                    GlobalLogPtr->Ticker->SetString(LoadGameString(OnPurchaseScreen() ? 0x2e : 0x33, 0xfe));
                }
            }
            else
            {
                PilotDrag.X = event->X - 0xf;
                PilotDrag.Y = event->Y - 0xf;
                MCDragIcon::Current()->MoveTo(PilotDrag.X, PilotDrag.Y, false);
            }
            break;
        }

        default:
            break;
    }
}
