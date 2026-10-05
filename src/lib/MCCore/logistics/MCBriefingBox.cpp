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

auto MCBriefingBox::Init(std::shared_ptr<MCLogMech> logMech, std::shared_ptr<MCLogVehicle> logVehicle) -> void
{
    _Mech = logMech;
    _Vehicle = logVehicle;
    MCLogObject::InitWithoutPort(0xd3, 0x16f, 0x1ab, 0x6f);
    InventoryPane.reset();

    if (logMech == nullptr)
    {
        return;
    }

    InventoryPane = MCMakeGui<MCScrollPane>();
    InventoryPane->Init(0x62, 0x58, 0x143, 0x11, std::string_view());
    logMech->RepairBlock->SetInventory(*InventoryPane);
    AddChild(InventoryPane);
}

auto MCBriefingBox::Destroy() -> void
{
    InventoryPane.reset();
    MCLogObject::Destroy();
}

auto MCBriefingBox::DrawBackground() -> void
{
    // Port: the box is drawn each frame (PaintBox) by the briefing screen.
    if (Mech() != nullptr)
    {
        Mech()->RepairBlock->SetInventory(*InventoryPane);
    }

    GlobalLog()->BriefingScreen->ShowBox(WeakAs<MCBriefingBox>());
}

auto MCBriefingBox::PaintBox(const MCPane& target, int32_t xPos, int32_t yPos) const -> void
{
    const std::shared_ptr<MCLogMech> mech = Mech();
    // The block paints in the briefing screen's layout (it looks at the current screen); a screen change's wipe draws
    // the box while another screen is current.
    const std::shared_ptr<MCLogObject> current = GlobalLog()->CurrentScreen();
    GlobalLog()->SetCurrentScreen(GlobalLog()->BriefingScreen);
    MCLogBlockPort work(target, xPos, yPos, 0x1ab, 0x6f, false);

    if (mech == nullptr)
    {
        Vehicle()->RepairBlock->DrawBackground(-1, work);
    }
    else
    {
        mech->RepairBlock->DrawBackground(-1, work);
        // The weapon list and its slider, then the tonnage bar.
        MCLogBlockPort list(work.Frame(), 0x143, 0x11, 0x62, 0x58, false);
        InventoryPane->DrawContentTo(list.Frame(), 0, 0);
        InventoryPane->DrawSliderColumn(work.Frame(), InventoryPane->Width() + 0x136, 0x11, false);
        int32_t fill = static_cast<int32_t>(static_cast<double>(mech->WeaponTonnage) / mech->FreeTonnage * 55.0);
        DrawTonnageBar(work.Frame(), 0x16c, fill);
    }

    GlobalLog()->SetCurrentScreen(current);
    MCLogistics::Darken(0, LogisticFadetable(), work);
}

auto MCBriefingBox::HandleEvent(MCGuiEvent& event) -> void
{
    std::shared_ptr<MCGuiObject> pane = Child(0);

    if (!Parent.expired() && (event.Type == 8 || event.Type == 9))
    {
        ParentObject()->HandleEvent(event);
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
    std::shared_ptr<MCGuiObject> pane = Child(0);
    return OverPaneArea(*pane, xPos, yPos) && pane->MouseWheel(steps, xPos, yPos);
}

auto MCBriefingBox::Draw() -> void
{
    // The original repainted the weapon list (scrolled) into the briefing screen's picture; the screen draws the whole
    // box each frame (PaintBox).
}

auto MCBriefingBox::Display() -> void
{
}
