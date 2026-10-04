#include "stdafx.h"
#include "logistics/logpur.h"
#include "gui/scrlpane.h"
#include "lib/aerror.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "logistics/purchase.h"
#include "logistics/ticker.h"
#include "main/logistics.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "platform/MCInput.h"
#include "sound/soundsys.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>Set while the left button is down on the screen (DAT_0080866c); only written.</summary>
    int32_t mouseDown = 0;

    /// <summary>The blink phase of the briefing button's highlight, flipped by each timer event (DAT_00808670).</summary>
    int32_t briefingBlink = 0;

    void freePort(lPort*& port)
    {
        if (port != nullptr)
        {
            delete port;
        }

        port = nullptr;
    }

    /// <summary>Puts string <paramref name="id"/> on the ticker.</summary>
    void showHelp(uint32_t id)
    {
        char text[256];
        cLoadString(thisInstance, id, text, 0xfe);
        globalLogPtr->ticker->setString(text);
    }
}

auto PurchaseScreen::init() -> void
{
    unknown4BC = 0;
    chatBlinking = 0;
    purMechPort = nullptr;
    purPilotPort = nullptr;
    purCompPort = nullptr;
    purVehiclePort = nullptr;
    int32_t result = lObject::init(0, 0, 0x280, 0x1e0, nullptr, nullptr);
    Assert(result == 0, result, "Unable to init purchase screen", nullptr);
    // The original loaded the background (lspbk00) as the screen's picture and pasted the mech inventory header into
    // it; the screen draws both each frame.
    initLive("lspbk00.tga");
    info.header = 0;
    char fileName[256];

    auto* pane = new ScrollPane;

    if (pane != nullptr)
    {
        pane->init();
    }

    inventoryPane = pane;
    Assert(pane != nullptr, 0, " Not enough memory for inventory", nullptr);
    pane->init(0xb8, 0x10d, 8, 0x6b, static_cast<char*>(nullptr));
    pane->setDisplayPort(nullptr, -1, -1);

    auto* store = new ScrollPane;

    if (store != nullptr)
    {
        store->init();
    }

    unitPane = store;
    Assert(store != nullptr, 0, "Not enough memory for vehicleScroll", nullptr);
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsrbk01.tga", artPath);
    store->init(0x1aa, 0x1cc, 0xd3, 0x11, fileName);
    store->setDisplayPort(nullptr, -1, -1);
    addChild(pane);
    addChild(unitPane);
    ShowGUIWindow(0);
    screenWindow->addChild(this);

    mechTabPort = new lPort;
    pilotTabPort = new lPort;
    compTabPort = new lPort;
    vehicleTabPort = new lPort;
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lspbim00.tga", artPath);
    mechTabPort->init(fileName);
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lspbip00.tga", artPath);
    pilotTabPort->init(fileName);
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lspbic00.tga", artPath);
    compTabPort->init(fileName);
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lspbiv00.tga", artPath);
    vehicleTabPort->init(fileName);
}

auto PurchaseScreen::destroy() -> void
{
    screenWindow->removeChild(this);
    freePort(mechTabPort);
    freePort(pilotTabPort);
    freePort(compTabPort);
    freePort(vehicleTabPort);
    freePort(purMechPort);
    freePort(purPilotPort);
    freePort(purCompPort);
    freePort(purVehiclePort);

    // The panes' ports were freed above or belong to the Logistics object.
    if (inventoryPane != nullptr)
    {
        inventoryPane->setDisplayPort(nullptr, 0, -1);
        delete inventoryPane;
        inventoryPane = nullptr;
    }

    if (unitPane != nullptr)
    {
        unitPane->setDisplayPort(nullptr, 0, -1);
        delete unitPane;
        unitPane = nullptr;
    }

    lObject::destroy();
}

auto PurchaseScreen::drawBackground() -> void
{
    // The background art was painted over everything the screen showed.
    chrome.Clear();
    info.Clear();
}

auto PurchaseScreen::handleEvent(aEvent* event) -> void
{
    if (globalLogPtr->currentScreen != this)
    {
        return;
    }

    int32_t xPos = event->x;
    int32_t yPos = event->y;
    uint8_t key = event->key;

    if (key == 0 && event->type != 0x13)
    {
        // The help line for whatever the mouse is over, and the highlighted screen button.
        globalLogPtr->drawScreenButtons();
        drawBlankInvInfoBlock(-1);
        POINT point{xPos, yPos};
        auto inside = [&point](int32_t left, int32_t top, int32_t right, int32_t bottom)
        {
            RECT area{left, top, right, bottom};
            return PtInRect(&area, point) != 0;
        };

        if (inside(2, 2, 0xd0, 0xd))
        {
            showHelp(0x1d);
        }
        else if (inside(2, 0x10, 0xd0, 0x21))
        {
            showHelp(0x286);
            globalLogPtr->hoverScreenButton(this, 0);
        }
        else if (inside(2, 0x22, 0xd0, 0x33))
        {
            showHelp(0x1e);
            globalLogPtr->hoverScreenButton(this, 1);
        }
        else if (inside(2, 0x34, 0xd0, 0x45))
        {
            showHelp(0x41);
        }
        else if (inside(2, 0x46, 0xd0, 0x57))
        {
            showHelp(0x42);
            globalLogPtr->hoverScreenButton(this, 3);
        }
        else if (inside(0x20c, 2, 0x24d, 0xd))
        {
            showHelp(0x1f);
        }
        else if (inside(0xc6, 0x66, 0xcf, 0x95))
        {
            showHelp(0x29);
        }
        else if (inside(0xc6, 0x97, 0xcf, 0xef))
        {
            showHelp(0x2a);
        }
        else if (inside(0xc6, 0xf1, 0xcf, 0x140))
        {
            showHelp(0x2b);
        }
        else if (inside(0xc6, 0x141, 0xcf, 0x17c))
        {
            showHelp(0x2c);
        }
        else if (inside(7, 0x6a, 0xb3, 0x178))
        {
            switch (globalLogPtr->currentInvTab)
            {
                case 0:
                case 2:
                case 3:
                    showHelp(0x2d);
                    break;
                case 1:
                    showHelp(0x2e);
                    break;
                default:
                {
                    // Port fix: the original put its uninitialised string buffer on the ticker.
                    char empty[1] = {};
                    globalLogPtr->ticker->setString(empty);
                    break;
                }
            }
        }
        else if (inside(0x270, 0x10, 0x27d, 0x1dd))
        {
            showHelp(0x2f);
        }
        else if (inside(0xd3, 0x10, 0x270, 0x1dd))
        {
            showHelp(0x30);
        }
        else
        {
            globalLogPtr->ticker->setString(nullptr);
        }
    }

    switch (event->type)
    {
        case 1:
        {
            mouseDown = -1;
            POINT point{xPos - globalX(), yPos - globalY()};
            RECT area{2, 0x46, 0xd1, 0x57};

            if (PtInRect(&area, point) != 0)
            {
                globalLogPtr->setUpRepairScreen(-1);
                return;
            }

            area.top = 0x22;
            area.bottom = 0x33;

            if (PtInRect(&area, point) != 0)
            {
                globalLogPtr->setUpBriefingScreen(-1);
                return;
            }

            area.top = 0x10;
            area.bottom = 0x21;

            if (PtInRect(&area, point) != 0)
            {
                if (MPlayer != nullptr)
                {
                    CheckExit();
                    return;
                }

                soundSystem->playDigitalSample(0x36, 1, nullptr, 0, 0);
                globalLogPtr->setUpMainScreen(0);
                return;
            }

            // The inventory tabs.
            area = {0xc2, 0x67, 0xd1, 0x95};

            if (PtInRect(&area, point) != 0)
            {
                soundSystem->playDigitalSample(0x36, 1, nullptr, 0, 0);
                setUpMechInv(-1, -1);
                setUpMechPurchase();
            }

            area.top = 0x96;
            area.bottom = 0xf1;

            if (PtInRect(&area, point) != 0)
            {
                soundSystem->playDigitalSample(0x36, 1, nullptr, 0, 0);
                setUpPilotInv(-1, -1);
                setUpPilotPurchase();
            }

            area.top = 0xf2;
            area.bottom = 0x140;

            if (PtInRect(&area, point) != 0)
            {
                soundSystem->playDigitalSample(0x36, 1, nullptr, 0, 0);
                setUpCompInv(-1, -1);
                setUpCompPurchase();
            }

            area.top = 0x141;
            area.bottom = 0x17c;

            if (PtInRect(&area, point) != 0)
            {
                soundSystem->playDigitalSample(0x36, 1, nullptr, 0, 0);
                setUpVhclInv(-1, -1);
                setUpVehiclePurchase();
            }
            break;
        }

        case 4:
        {
            mouseDown = 0;
            // The original fetches globalX() and globalY() here and drops them.
            globalX();
            globalY();
            break;
        }
        case 9:
        {
            bool ctrlAlt = MCInput::GetAsyncKeyState(VK_CONTROL) != 0 && MCInput::GetAsyncKeyState(VK_MENU) != 0;

            // Original behaviour (OB-077): the zero-extended key never equals -0x45, so the cheat never fires.
            if (MPlayer == nullptr && static_cast<int32_t>(key) == -0x45 && ctrlAlt)
            {
                ResourcePoints += 1000;
            }
            break;
        }

        case 8:
            globalLogPtr->processCheatCode(event->scanCode);
            break;
        case 0x13:
        {
            // Blink the briefing button.
            chrome.blinkLit = briefingBlink != 0;
            briefingBlink = briefingBlink == 0 ? 1 : 0;
            break;
        }

        default:
            break;
    }
}

auto PurchaseScreen::ShowGUIWindow(int show) -> void
{
    showWindow = show;
    inventoryPane->ShowGUIWindow(show);
    unitPane->ShowGUIWindow(show);
}
