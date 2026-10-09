#include "stdafx.h"
#include "logistics/MCDragIcon.h"
#include "gui/MCHardwareCursor.h"
#include "main/logistics.h"
#include "vfx/MCVfxFunctions.h"

auto MCDragIcon::Display() -> void
{
    if (IsShowing() == 0)
    {
        return;
    }

    // Port fix: with the system cursor, the icon rides on the cursor instead of being drawn into the frame, so
    // it follows the mouse as closely as the cursor does.
    if (MCHardwareCursorCarry(this, Lport()->Frame()))
    {
        return;
    }

    VfxPaneCopy(Lport()->Frame(), 0, 0, FramePane.get(), 0, 0, -1);
}

auto MCDragIcon::Begin(int32_t xPos, int32_t yPos, int32_t width, int32_t height,
                       const std::function<void(MCLogPort* surface)>& render) -> void
{
    Init(xPos, yPos, width, height);
    MCPane* surface = Lport()->Frame();
    VfxPaneWipe(surface, 0);
    render(Lport());
    const int32_t right = width - 1;
    const int32_t bottom = height - 1;
    VfxLineDraw(surface, 0, 0, right, 0, 0xea);
    VfxLineDraw(surface, 0, 1, 0, bottom - 1, 0xea);
    VfxLineDraw(surface, right, 1, right, bottom - 1, 0xea);
    VfxLineDraw(surface, 0, bottom, right, bottom, 0xea);
}

auto MCDragIcon::Raise() -> void
{
    ShowGuiWindow(true);
    SetDepth(100);
}

auto MCDragIcon::ShowOn(MCGuiObject* screen, int32_t xPos, int32_t yPos) -> void
{
    screen->AddChild(this);
    Raise();
    MoveTo(xPos, yPos, false);
}

auto MCDragIcon::DrawFrom(MCLogPort* surface, int32_t xPos, int32_t yPos,
                          const std::function<void(MCLogPort* port)>& draw) -> void
{
    MCPane* pane = surface->Frame();
    const MCPane whole = *pane;
    pane->X0 = whole.X0 - xPos;
    pane->Y0 = whole.Y0 - yPos;
    draw(surface);
    *pane = whole;
}

auto MCDragIcon::Create() -> MCDragIcon*
{
    GlobalLogPtr->DragIcon = MCMakeGui<MCDragIcon>();
    return GlobalLogPtr->DragIcon.get();
}

auto MCDragIcon::Current() -> MCDragIcon*
{
    return GlobalLogPtr->DragIcon.get();
}

auto MCDragIcon::Remove() -> void
{
    GlobalLogPtr->DragIcon.reset();
}
