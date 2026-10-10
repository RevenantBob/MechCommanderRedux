#include "stdafx.h"
#include "logistics/MCPilotPurchaseBlock.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiGlobals.h"
#include "gui/MCScrollPane.h"
#include "main/MCGamePaths.h"
#include "logistics/MCLogRows.h"
#include "logistics/MCPurPilotList.h"
#include "logistics/MCPurchaseDlg.h"
#include "logistics/MCPurchaseScreen.h"
#include "logistics/MCStoreRow.h"
#include "logistics/MCTicker.h"
#include "logistics/MCUnitLimits.h"
#include "main/MCLogistics.h"
#include "main/main.h"
#include "sound/MCSoundSystem.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>The pilot rows' drag.</summary>
    MCDragState PilotDrag;
}

MCPilotPurchaseBlock::~MCPilotPurchaseBlock()
{
    MCPilotPurchaseBlock::Destroy();
}

auto MCPilotPurchaseBlock::Init(MCPurPilotData* newPilot) -> void
{
    Pilot = newPilot;
    MCLogObject::InitWithoutPort(0, 0, 0x19a, 0x70);
}

auto MCPilotPurchaseBlock::Destroy() -> void
{
    Pilot = nullptr;
    MCLogObject::Destroy();
}

auto MCPilotPurchaseBlock::OnHireConfirmed(int32_t result) -> void
{
    if (result == 0)
    {
        return;
    }

    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen.get();
    MCPurPilotData* pilot = Pilot;
    float scrollPos = screen->UnitPane->ScrollPos;
    GlobalLogPtr->WarriorList->AddWarrior(pilot->FileName.data(), 1);
    GlobalLogPtr->ReorderWarriors();
    screen->CreatePilotInvBlock();
    screen->SetUpPilotInv(true, true);
    pilot->Health = 0;
    GlobalLogPtr->PurPilotList->SetPilotStatus(pilot->DescIndex, MCPurPilotData::Hired);
    screen->RemovePilot(Row);
    screen->SetUpPilotPurchase();
    ResourcePoints -= GlobalLogPtr->PurchaseDialog->UnitCost;
    SoundSystem()->PlayPilotSpeech(pilot->PilotAudio.data(), 2);
    screen->UnitPane->SetScrollPos(scrollPos);
}

auto MCPilotPurchaseBlock::HandleEvent(MCGuiEvent* event) -> void
{
    if (!MCStoreRow::PreHandleEvent(this, PilotDrag, event))
    {
        return;
    }

    int32_t type = event->Type;

    switch (type)
    {
        case 1:
        {
            if (PilotDrag.Carrying)
            {
                return;
            }

            [[fallthrough]];
        }
        case 3:
        {
            if (!PilotDrag.Dragging && OnRow(this, event))
            {
                // Pick the pilot up (left button drags, right button carries). The cursor stays shown.
                (type == 1 ? PilotDrag.Dragging : PilotDrag.Carrying) = true;
                SoundSystem()->PlayPilotSpeech(Pilot->PilotAudio.data(), 10);
                GuiSystem()->Grab(this);
                PilotDrag.Y = event->Y - 0x10;
                PilotDrag.X = event->X - 0x10;
                MCStoreRow::MakeDragIcon(PilotDrag, [this](MCLogPort* surface) { OnBeginDrag(surface); });
                return;
            }
            break;
        }

        case 4:
        {
            if (PilotDrag.Carrying)
            {
                return;
            }

            [[fallthrough]];
        }
        case 6:
        {
            if (PilotDrag.Dragging && type == 6)
            {
                break;
            }

            PilotDrag.Carrying = false;

            if (GuiSystem()->GrabbedObject() == nullptr)
            {
                break;
            }

            GuiSystem()->Release();
            PilotDrag.Dragging = false;
            MCDragIcon::Remove();

            if (type != 6 && !MCStoreRow::OverInventory(event))
            {
                PlayLogSound(MCStoreRow::OverStore(event) ? 0x34 : 0x33);
                return;
            }

            // Hire the pilot.
            if (ResourcePoints < Pilot->Cost)
            {
                PlayLogSound(0x33);
                ShowLogMessage(0x4d);
                return;
            }

            MCLogPort picture;
            picture.Load(std::format("{}logart\\pilot{:02}.tga", ArtPath, Pilot->NameIndex));
            OpenPurchaseDialog(2, Pilot->Cost, 1, Pilot->Callsign, {}, &picture,
                               [this](int32_t result, int32_t) { OnHireConfirmed(result); });
            break;
        }

        case 7:
        {
            if (PilotDrag.Dragging)
            {
                PilotDrag.Y = event->Y - 0xf;
                PilotDrag.X = event->X - 0xf;
                MCDragIcon::Current()->MoveTo(PilotDrag.X, PilotDrag.Y, false);
                return;
            }

            if (event->Key == 0)
            {
                GlobalLogPtr->Ticker->SetString(LoadGameString(0x30, 0xfe));
            }

            return;
        }

        default:
            break;
    }
}

auto MCPilotPurchaseBlock::DrawBackground(int32_t) -> void
{
    PrepareInfoDescription(Pilot->Description);
}

auto MCPilotPurchaseBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    // A hired pilot (health cleared by OnHireConfirmed) is not drawn.
    if (Pilot->Health == 0)
    {
        return;
    }

    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen.get();
    std::unique_ptr<MCLogBlockPort> work = RowPicture(screen->PilotTabPort.get(), port, top, true);
    CopyArt(work.get(), 5, 4, std::format("lspflp{:02}.tga", Pilot->NameIndex));
    CopyArt(work.get(), 7, 0x26, std::format("pilot{:02}.tga", Pilot->NameIndex));
    std::string text = std::format("{}", Pilot->Cost);
    WriteText(YellowDropFont, work.get(), 0x1f, 0x12, text);

    // An out-of-range rank shows the price again.
    if (Pilot->Rank >= 0 && Pilot->Rank <= 3)
    {
        text = LoadGameString(0x70 + static_cast<uint32_t>(Pilot->Rank), 0xfe);
    }

    WriteText(YellowDropFont, work.get(), 0x9a, 0x2a, text);
    GlobalLogPtr->DrawPilotSkillBar(Pilot->Gunnery, 0x54, 0x22, 0, 0x36, 4, work.get());
    GlobalLogPtr->DrawPilotSkillBar(Pilot->Piloting, 0x54, 0x2b, 0, 0x36, 4, work.get());
    GlobalLogPtr->DrawPilotSkillBar(Pilot->Jumping, 0x54, 0x34, 0, 0x36, 4, work.get());
    GlobalLogPtr->DrawPilotSkillBar(Pilot->Sensors, 0x54, 0x3d, 0, 0x36, 4, work.get());
    // One pip per point of health.
    int32_t x = 0xe;

    for (int32_t pip = Pilot->Health; pip > 0; --pip, x += 3)
    {
        VfxPixelWrite(work->Frame(), x, 0x22, 0xcf);
        VfxPixelWrite(work->Frame(), x + 1, 0x22, 0xcf);
        VfxPixelWrite(work->Frame(), x + 1, 0x23, 0xee);
        VfxPixelWrite(work->Frame(), x, 0x23, 0xcf);
    }

    DrawInfoDescription(work.get(), 0xc6, 0x25, Pilot->Description, 8, 0x48);
}

auto MCPilotPurchaseBlock::OnBeginDrag(MCLogPort* surface) -> void
{
    // The square at (6, 0x25) of the row, over the pilot list's colour 0xff.
    VfxPaneWipe(surface->Frame(), 0xff);
    MCDragIcon::DrawFrom(surface, 6, 0x25, [this](MCLogPort* port) { DrawRow(port, 0); });
}
