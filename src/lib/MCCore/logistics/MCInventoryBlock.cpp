#include "stdafx.h"
#include "logistics/MCInventoryBlock.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCScrollPane.h"
#include "logistics/MCLogInvScreen.h"
#include "main/MCLogistics.h"
#include "network/MCMultiPlayer.h"
#include "vfx/MCVfxFunctions.h"

MCInventoryBlock::~MCInventoryBlock()
{
    MCInventoryBlock::Destroy();
}

auto MCInventoryBlock::InitRow() -> void
{
    MCLogObject::InitWithoutPort(0, 0, 0xad, 0x2b);
    Enabled = true;
}

auto MCInventoryBlock::Destroy() -> void
{
    MCLogInvScreen::ForgetInfoSource(this);
    MCLogObject::Destroy();
}

auto MCInventoryBlock::DrawBackground(MCLogPort* port) -> void
{
    VfxPaneCopy(GlobalLogPtr->InvBlockPort->Frame(), 0, 0, port->Frame(), 0, ListIndex * WinHeight, -1);
}

auto MCInventoryBlock::DrawDisabled() -> void
{
}

auto MCInventoryBlock::SetEnabled(bool enable) -> void
{
    Enabled = enable;
    Draw();
}

auto MCInventoryBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    VfxPaneCopy(GlobalLogPtr->InvBlockPort->Frame(), 0, 0, port->Frame(), 0, top, -1);
}

auto MCInventoryBlock::OnBeginDrag(MCLogPort* surface) -> void
{
    VfxPaneWipe(surface->Frame(), 0x10);
    MCDragIcon::DrawFrom(surface, 2, 1, [this](MCLogPort* port) { DrawRow(port, 0); });
}

auto MCInventoryBlock::PreHandleEvent(const MCDragState& drag, MCGuiEvent* event) -> bool
{
    if (Parent != nullptr && drag.Idle() && (event->Type == 8 || event->Type == 9))
    {
        Parent->HandleEvent(event);
        return false;
    }

    MCLogInvScreen* screen = Screen();

    if (drag.Idle())
    {
        MCGuiObject* pane = screen->InventoryPane;

        if (event->Y < pane->GlobalY() || pane->GlobalY() + pane->Height() < event->Y)
        {
            screen->DrawBlankInvInfoBlock(-1);
            screen->HandleEvent(event);
            return false;
        }

        if (IsShowing() == 0)
        {
            return false;
        }
    }

    return true;
}

auto MCInventoryBlock::MakeDragIcon(MCDragState& drag, MCGuiEvent* event) -> void
{
    drag.X = 2;
    drag.Y = ListIndex * WinHeight + 1;
    MCDragIcon* icon = MCDragIcon::Create();
    icon->Begin(drag.X, drag.Y, 0x20, 0x20, [this](MCLogPort* surface) { OnBeginDrag(surface); });
    Screen()->AddChild(icon);
    icon->MoveTo(event->X - 0xf, event->Y - 0xf, false);
}

auto MCInventoryBlock::Screen() -> MCLogInvScreen*
{
    return static_cast<MCLogInvScreen*>(GlobalLogPtr->CurrentScreen);
}

auto MCInventoryBlock::DrawDropArt(MCLogInvScreen* screen, int32_t tab) -> void
{
    screen->Info.Blank(tab);
}

auto MCInventoryBlock::SalePrice(int32_t value) -> int32_t
{
    if (MultiPlayer() == nullptr && !Solo)
    {
        value /= 2;
    }

    return value;
}

auto MCInventoryBlock::BumpDeploySlots(bool vehicle) -> void
{
    for (auto& lance : GlobalLogPtr->DeploySlots)
    {
        for (auto& slot : lance)
        {
            int32_t& value = vehicle ? slot.Vehicle : slot.Unit;

            if (value > -1)
            {
                ++value;
            }
        }
    }
}
