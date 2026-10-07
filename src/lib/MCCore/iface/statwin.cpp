#include "stdafx.h"
#include "iface/statwin.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "object/cmponent.h"
#include "object/mover.h"
#include "object/object.h"
#include "object/objque.h"
#include "vfx/vfxfuncs.h"

auto MCInfoWindow::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, int32_t objectPartId) -> int32_t
{
    int32_t result = MCGuiTitleWindow::Init(xPos, yPos, width, height, nullptr);

    if (result != 0)
    {
        return result;
    }

    MCGuiTitleBar* bar = TitleBar;

    if (bar != nullptr)
    {
        bar->ShowCloseButton(-1);
    }

    if (ResizeButton != nullptr)
    {
        ResizeButton->ShowGuiWindow(-1);
    }

    // The original calls this even when there is no title bar.
    bar->ShowZoomButtons(0);
    SetBackColor(2);
    PartId = objectPartId;
    LastUpdateTime = MCPort::Milliseconds();
    return 0;
}

auto MCInfoWindow::Display() -> void
{
    if (IsShowing() == 0)
    {
        return;
    }

    if (IsHidden() != 0 && HideOffset == 0)
    {
        return;
    }

    // Port: the original redrew its picture every 500 ms here; the window draws itself each frame.
    MCGuiObject::Display();
}

auto DrawMechInfo(MCGuiObject* window) -> void
{
    MCPane* pane = window->Port()->Frame();
    const char* locationNames[9] = {"Head",      "Center Torso", "Left Torso", "Right Torso", "Left Arm",
                                    "Right Arm", "Left Leg",     "Right Leg",  nullptr};
    char text[256];

    VfxPaneWipe(pane, window->BackColor());

    if (ObjectList == nullptr)
    {
        return;
    }

    int32_t partId = static_cast<MCInfoWindow*>(window)->PartId;
    auto* mover = static_cast<MCMover*>(ObjectList->FindObjectFromPart(partId));

    if (mover == nullptr)
    {
        return;
    }

    sprintf(text, "Damaged bits of %s", mover->DebugStatus.c_str());
    WhiteFont->WriteString(pane, 2, 10, reinterpret_cast<uint8_t*>(text), -1);

    int32_t yPos = 0x1e;
    int32_t numItems = mover->NumOther + mover->NumAmmos + mover->NumWeapons;
    MCInventoryItem* item = mover->Inventory.get();

    for (; numItems != 0; numItems--, item++)
    {
        if (item->Disabled == 0)
        {
            continue;
        }

        sprintf(text, "%s %s", locationNames[item->BodyLocation], MasterComponentList[item->MasterID].Abbreviation);
        WhiteFont->WriteString(pane, 10, yPos, reinterpret_cast<uint8_t*>(text), -1);
        yPos += 10;
    }
}
