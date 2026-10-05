#include "stdafx.h"
#include "iface/statwin.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "object/cmponent.h"
#include "object/mover.h"
#include "object/object.h"
#include "object/objque.h"
#include "vfx/vfxfuncs.h"

auto InfoWindow::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, int32_t objectPartId) -> int32_t
{
    int32_t result = aTitleWindow::init(xPos, yPos, width, height, nullptr);

    if (result != 0)
    {
        return result;
    }

    aTitleBar* bar = titleBar;

    if (bar != nullptr)
    {
        bar->showCloseButton(-1);
    }

    if (resizeButton != nullptr)
    {
        resizeButton->ShowGUIWindow(-1);
    }

    // The original calls this even when there is no title bar.
    bar->showZoomButtons(0);
    setBackColor(2);
    partId = objectPartId;
    lastUpdateTime = MCPort::Milliseconds();
    return 0;
}

auto InfoWindow::display() -> void
{
    if (IsShowing() == 0)
    {
        return;
    }

    if (IsHidden() != 0 && hideOffset == 0)
    {
        return;
    }

    // Port: the original redrew its picture every 500 ms here; the window draws itself each frame.
    aObject::display();
}

auto DrawMechInfo(aObject* window) -> void
{
    _pane* pane = window->port()->frame();
    const char* locationNames[9] = {"Head",      "Center Torso", "Left Torso", "Right Torso", "Left Arm",
                                    "Right Arm", "Left Leg",     "Right Leg",  nullptr};
    char text[256];

    VFX_pane_wipe(pane, window->backColor());

    if (objectList == nullptr)
    {
        return;
    }

    int32_t partId = static_cast<InfoWindow*>(window)->partId;
    auto* mover = static_cast<Mover*>(objectList->findObjectFromPart(partId));

    if (mover == nullptr)
    {
        return;
    }

    sprintf(text, "Damaged bits of %s", mover->debugStatus.c_str());
    whiteFont->writeString(pane, 2, 10, reinterpret_cast<uint8_t*>(text), -1);

    int32_t yPos = 0x1e;
    int32_t numItems = mover->numOther + mover->numAmmos + mover->numWeapons;
    InventoryItem* item = mover->inventory.get();

    for (; numItems != 0; numItems--, item++)
    {
        if (item->disabled == 0)
        {
            continue;
        }

        sprintf(text, "%s %s", locationNames[item->bodyLocation], MasterComponentList[item->masterID].abbreviation);
        whiteFont->writeString(pane, 10, yPos, reinterpret_cast<uint8_t*>(text), -1);
        yPos += 10;
    }
}
