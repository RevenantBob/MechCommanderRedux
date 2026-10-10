#include "stdafx.h"
#include "logistics/MCBriefingBox.h"
#include "gui/MCGuiEvent.h"
#include "logistics/MCLogRows.h"
#include "logistics/MCMechRepairBlock.h"
#include "logistics/MCVehicleRepairBlock.h"
#include "main/MCLogistics.h"
#include "logistics/MCBriefingScreen.h"

MCBriefingBox::~MCBriefingBox()
{
    MCBriefingBox::Destroy();
}

auto MCBriefingBox::Init(MCLogMech* logMech, MCLogVehicle* logVehicle) -> void
{
    Mech = logMech;
    Vehicle = logVehicle;
    MCLogObject::InitWithoutPort(0xd3, 0x16f, 0x1ab, 0x6f);
    InventoryPane.reset();

    if (logMech == nullptr)
    {
        return;
    }

    InventoryPane = MCMakeGui<MCScrollPane>();
    InventoryPane->Init(0x62, 0x58, 0x143, 0x11, static_cast<char*>(nullptr));
    logMech->RepairBlock->SetInventory(InventoryPane.get());
    AddChild(InventoryPane.get());
}

auto MCBriefingBox::Destroy() -> void
{
    InventoryPane.reset();
    MCLogObject::Destroy();
}

auto MCBriefingBox::DrawBackground() -> void
{
    // Port: the box is drawn each frame (PaintBox) by the briefing screen.
    if (Mech != nullptr)
    {
        Mech->RepairBlock->SetInventory(InventoryPane.get());
    }

    GlobalLogPtr->BriefingScreen->ShowBox(this);
}

auto MCBriefingBox::PaintBox(MCPane* target, int32_t xPos, int32_t yPos) -> void
{
    // The block paints in the briefing screen's layout (it looks at the current screen); a screen change's wipe draws
    // the box while another screen is current.
    MCLogObject* const current = GlobalLogPtr->CurrentScreen;
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->BriefingScreen.get();
    MCLogBlockPort work(target, xPos, yPos, 0x1ab, 0x6f, false);

    if (Mech == nullptr)
    {
        Vehicle->RepairBlock->DrawBackground(-1, &work);
    }
    else
    {
        Mech->RepairBlock->DrawBackground(-1, &work);
        // The weapon list and its slider, then the tonnage bar.
        MCLogBlockPort list(work.Frame(), 0x143, 0x11, 0x62, 0x58, false);
        InventoryPane->DrawContentTo(list.Frame(), 0, 0);
        InventoryPane->DrawSliderColumn(work.Frame(), InventoryPane->Width() + 0x136, 0x11, false);
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
    return OverPaneArea(pane, xPos, yPos) && pane->MouseWheel(steps, xPos, yPos);
}

auto MCBriefingBox::Draw() -> void
{
    // The original repainted the weapon list (scrolled) into the briefing screen's picture; the screen draws the whole
    // box each frame (PaintBox).
}

auto MCBriefingBox::Display() -> void
{
}
