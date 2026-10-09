#include "stdafx.h"
#include "logistics/logpur.h"
#include "gui/scrlpane.h"
#include "lib/MCFatal.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "logistics/purchase.h"
#include "logistics/ticker.h"
#include "main/logistics.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "platform/MCInput.h"
#include "sound/MCSoundSystem.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>Set while the left button is down on the screen (0x0080866c); only written.</summary>
    int32_t MouseDown = 0;

    /// <summary>The blink phase of the briefing button's highlight, flipped by each timer event (0x00808670).</summary>
    int32_t BriefingBlink = 0;

    void FreePort(MCLogPort*& port)
    {
        if (port != nullptr)
        {
            delete port;
        }

        port = nullptr;
    }

    /// <summary>Puts string <paramref name="id"/> on the ticker.</summary>
    void ShowHelp(uint32_t id)
    {
        char text[256];
        CLoadString(ThisInstance, id, text, 0xfe);
        GlobalLogPtr->Ticker->SetString(text);
    }
}

auto MCPurchaseScreen::Init() -> void
{
    ChatBlinking = 0;
    PurMechPort = nullptr;
    PurPilotPort = nullptr;
    PurCompPort = nullptr;
    PurVehiclePort = nullptr;
    int32_t result = MCLogObject::Init(0, 0, 0x280, 0x1e0, nullptr, nullptr);
    Assert(result == 0, result, "Unable to init purchase screen");
    // The original loaded the background (lspbk00) as the screen's picture and pasted the mech inventory header into
    // it; the screen draws both each frame.
    InitLive("lspbk00.tga");
    Info.Header = 0;
    char fileName[256];

    auto* pane = new MCScrollPane;

    if (pane != nullptr)
    {
        pane->Init();
    }

    InventoryPane = pane;
    Assert(pane != nullptr, 0, " Not enough memory for inventory");
    pane->Init(0xb8, 0x10d, 8, 0x6b, static_cast<char*>(nullptr));
    pane->SetDisplayPort(nullptr, -1, -1);

    auto* store = new MCScrollPane;

    if (store != nullptr)
    {
        store->Init();
    }

    UnitPane = store;
    Assert(store != nullptr, 0, "Not enough memory for vehicleScroll");
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsrbk01.tga", ArtPath);
    store->Init(0x1aa, 0x1cc, 0xd3, 0x11, fileName);
    store->SetDisplayPort(nullptr, -1, -1);
    AddChild(pane);
    AddChild(UnitPane);
    ShowGuiWindow(0);
    ScreenWindow()->AddChild(this);

    MechTabPort = new MCLogPort;
    PilotTabPort = new MCLogPort;
    CompTabPort = new MCLogPort;
    VehicleTabPort = new MCLogPort;
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lspbim00.tga", ArtPath);
    MechTabPort->Init(fileName);
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lspbip00.tga", ArtPath);
    PilotTabPort->Init(fileName);
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lspbic00.tga", ArtPath);
    CompTabPort->Init(fileName);
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lspbiv00.tga", ArtPath);
    VehicleTabPort->Init(fileName);
}

auto MCPurchaseScreen::Destroy() -> void
{
    ScreenWindow()->RemoveChild(this);
    FreePort(MechTabPort);
    FreePort(PilotTabPort);
    FreePort(CompTabPort);
    FreePort(VehicleTabPort);
    FreePort(PurMechPort);
    FreePort(PurPilotPort);
    FreePort(PurCompPort);
    FreePort(PurVehiclePort);

    // The panes' ports were freed above or belong to the Logistics object.
    if (InventoryPane != nullptr)
    {
        InventoryPane->SetDisplayPort(nullptr, 0, -1);
        delete InventoryPane;
        InventoryPane = nullptr;
    }

    if (UnitPane != nullptr)
    {
        UnitPane->SetDisplayPort(nullptr, 0, -1);
        delete UnitPane;
        UnitPane = nullptr;
    }

    MCLogObject::Destroy();
}

auto MCPurchaseScreen::DrawBackground() -> void
{
    // The background art was painted over everything the screen showed.
    ScreenChrome.Clear();
    Info.Clear();
}

auto MCPurchaseScreen::HandleEvent(MCGuiEvent* event) -> void
{
    if (GlobalLogPtr->CurrentScreen != this)
    {
        return;
    }

    int32_t xPos = event->X;
    int32_t yPos = event->Y;
    uint8_t key = event->Key;

    if (key == 0 && event->Type != 0x13)
    {
        // The help line for whatever the mouse is over, and the highlighted screen button.
        GlobalLogPtr->DrawScreenButtons();
        DrawBlankInvInfoBlock(-1);
        POINT point{xPos, yPos};
        auto inside = [&point](int32_t left, int32_t top, int32_t right, int32_t bottom)
        {
            RECT area{left, top, right, bottom};
            return PtInRect(&area, point) != 0;
        };

        if (inside(2, 2, 0xd0, 0xd))
        {
            ShowHelp(0x1d);
        }
        else if (inside(2, 0x10, 0xd0, 0x21))
        {
            ShowHelp(0x286);
            GlobalLogPtr->HoverScreenButton(this, 0);
        }
        else if (inside(2, 0x22, 0xd0, 0x33))
        {
            ShowHelp(0x1e);
            GlobalLogPtr->HoverScreenButton(this, 1);
        }
        else if (inside(2, 0x34, 0xd0, 0x45))
        {
            ShowHelp(0x41);
        }
        else if (inside(2, 0x46, 0xd0, 0x57))
        {
            ShowHelp(0x42);
            GlobalLogPtr->HoverScreenButton(this, 3);
        }
        else if (inside(0x20c, 2, 0x24d, 0xd))
        {
            ShowHelp(0x1f);
        }
        else if (inside(0xc6, 0x66, 0xcf, 0x95))
        {
            ShowHelp(0x29);
        }
        else if (inside(0xc6, 0x97, 0xcf, 0xef))
        {
            ShowHelp(0x2a);
        }
        else if (inside(0xc6, 0xf1, 0xcf, 0x140))
        {
            ShowHelp(0x2b);
        }
        else if (inside(0xc6, 0x141, 0xcf, 0x17c))
        {
            ShowHelp(0x2c);
        }
        else if (inside(7, 0x6a, 0xb3, 0x178))
        {
            switch (GlobalLogPtr->CurrentInvTab)
            {
                case 0:
                case 2:
                case 3:
                    ShowHelp(0x2d);
                    break;
                case 1:
                    ShowHelp(0x2e);
                    break;
                default:
                {
                    // Port fix: the original put its uninitialised string buffer on the ticker.
                    char empty[1] = {};
                    GlobalLogPtr->Ticker->SetString(empty);
                    break;
                }
            }
        }
        else if (inside(0x270, 0x10, 0x27d, 0x1dd))
        {
            ShowHelp(0x2f);
        }
        else if (inside(0xd3, 0x10, 0x270, 0x1dd))
        {
            ShowHelp(0x30);
        }
        else
        {
            GlobalLogPtr->Ticker->SetString(nullptr);
        }
    }

    switch (event->Type)
    {
        case 1:
        {
            MouseDown = -1;
            POINT point{xPos - GlobalX(), yPos - GlobalY()};
            RECT area{2, 0x46, 0xd1, 0x57};

            if (PtInRect(&area, point) != 0)
            {
                GlobalLogPtr->SetUpRepairScreen(-1);
                return;
            }

            area.top = 0x22;
            area.bottom = 0x33;

            if (PtInRect(&area, point) != 0)
            {
                GlobalLogPtr->SetUpBriefingScreen(-1);
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

                SoundSystem()->PlayDigitalSample(0x36, 1, nullptr, 0, 0);
                GlobalLogPtr->SetUpMainScreen(0);
                return;
            }

            // The inventory tabs.
            area = {0xc2, 0x67, 0xd1, 0x95};

            if (PtInRect(&area, point) != 0)
            {
                SoundSystem()->PlayDigitalSample(0x36, 1, nullptr, 0, 0);
                SetUpMechInv(-1, -1);
                SetUpMechPurchase();
            }

            area.top = 0x96;
            area.bottom = 0xf1;

            if (PtInRect(&area, point) != 0)
            {
                SoundSystem()->PlayDigitalSample(0x36, 1, nullptr, 0, 0);
                SetUpPilotInv(-1, -1);
                SetUpPilotPurchase();
            }

            area.top = 0xf2;
            area.bottom = 0x140;

            if (PtInRect(&area, point) != 0)
            {
                SoundSystem()->PlayDigitalSample(0x36, 1, nullptr, 0, 0);
                SetUpCompInv(-1, -1);
                SetUpCompPurchase();
            }

            area.top = 0x141;
            area.bottom = 0x17c;

            if (PtInRect(&area, point) != 0)
            {
                SoundSystem()->PlayDigitalSample(0x36, 1, nullptr, 0, 0);
                SetUpVhclInv(-1, -1);
                SetUpVehiclePurchase();
            }
            break;
        }

        case 4:
        {
            MouseDown = 0;
            // The original fetches globalX() and globalY() here and drops them.
            GlobalX();
            GlobalY();
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
            GlobalLogPtr->ProcessCheatCode(event->ScanCode);
            break;
        case 0x13:
        {
            // Blink the briefing button.
            ScreenChrome.BlinkLit = BriefingBlink != 0;
            BriefingBlink = BriefingBlink == 0 ? 1 : 0;
            break;
        }

        default:
            break;
    }
}

auto MCPurchaseScreen::ShowGuiWindow(bool show) -> void
{
    ShowWindow = show;
    InventoryPane->ShowGuiWindow(show);
    UnitPane->ShowGuiWindow(show);
}
