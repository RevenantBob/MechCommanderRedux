#include "stdafx.h"
#include "logistics/logrep.h"
#include "gui/afont.h"
#include "gui/scrlpane.h"
#include "lib/MCFatal.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "logistics/logpur.h"
#include "logistics/logsession.h"
#include "logistics/mrblock.h"
#include "logistics/purchase.h"
#include "logistics/ticker.h"
#include "main/logistics.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "platform/MCInput.h"
#include "sound/MCSoundSystem.h"
#include "vfx/MCVfxFunctions.h"

int32_t ResourceDisplayState = 0;

namespace
{
    /// <summary>The height of a unit block in the unit pane.</summary>
    constexpr int32_t UnitBlockHeight = 0x70;

    /// <summary>Set while the left button is down on the screen (0x0080867c); only written.</summary>
    int32_t MouseDown = 0;

    /// <summary>The blink phase of the briefing button's highlight, flipped by each timer event (0x00808678).</summary>
    int32_t BriefingBlink = 0;

    /// <summary>
    /// Draws the unit pane's rows into its view <paramref name="port"/>: every mech and vehicle of the force at its
    /// block's row, over colour 0xff (what the blocks painted into the rows' picture).
    /// </summary>
    void DrawUnitRows(MCGuiPort* port)
    {
        auto* view = static_cast<MCLogPort*>(port);
        VfxPaneWipe(view->Frame(), 0xff);
        const MCView& place = view->View;

        auto drawRow = [&](auto* block)
        {
            const int32_t top = block->SlotIndex * UnitBlockHeight;
            const int32_t screenTop = place.OriginY + top;

            if (screenTop <= place.Scissor.Y1 && place.Scissor.Y0 < screenTop + UnitBlockHeight)
            {
                block->DrawRow(view, top);
            }
        };

        for (MCLogMech* mech = GlobalLogPtr->ForceMechList->Mechs; mech != nullptr; mech = mech->Next)
        {
            drawRow(mech->RepairBlock);
        }

        for (MCLogVehicle* vehicle = GlobalLogPtr->ForceVehicleList->Vehicles; vehicle != nullptr;
             vehicle = vehicle->Next)
        {
            drawRow(vehicle->RepairBlock);
        }
    }

    /// <summary>
    /// Puts the force's repair blocks in order down the unit pane (mechs, then vehicles), scrolled as the pane is.
    /// </summary>
    void PlaceUnitBlocks(MCScrollPane* pane)
    {
        int32_t row = 0;
        int32_t yPos = 0;

        for (MCLogMech* mech = GlobalLogPtr->ForceMechList->Mechs; mech != nullptr; mech = mech->Next)
        {
            MCMechRepairBlock* block = mech->RepairBlock;
            block->SlotIndex = row;
            block->MoveTo(0, yPos - pane->GetScrollOffset(), 0);
            block->SetDepth(100);
            ++row;
            yPos += UnitBlockHeight;
        }

        yPos = row * UnitBlockHeight;

        for (MCLogVehicle* vehicle = GlobalLogPtr->ForceVehicleList->Vehicles; vehicle != nullptr;
             vehicle = vehicle->Next)
        {
            MCVehicleRepairBlock* block = vehicle->RepairBlock;
            block->SlotIndex = row;
            block->MoveTo(0, yPos - pane->GetScrollOffset(), 0);
            block->SetDepth(100);
            ++row;
            yPos += UnitBlockHeight;
        }
    }

    /// <summary>
    /// Rebuilds the unit pane's port without the block in a row: the rows below it move up one (the original
    /// copied them up in a new picture; the view draws each row where its block is).
    /// </summary>
    void RemoveUnitRow(MCScrollPane* pane)
    {
        pane->SetDisplayPort(MCRepairScreen::NewUnitRowsView(pane), -1, 0);
        PlaceUnitBlocks(pane);
    }

    /// <summary>Puts string <paramref name="id"/> on the ticker.</summary>
    void ShowHelp(uint32_t id)
    {
        char text[256];
        CLoadString(ThisInstance, id, text, 0xfe);
        GlobalLogPtr->Ticker->SetString(text);
    }
}

auto MCRepairScreen::Init() -> void
{
    ChatBlinking = 0;
    SelectedMech = nullptr;
    SelectedVehicle = nullptr;
    int32_t result = MCLogObject::Init(0, 0, 0x280, 0x1e0, nullptr, nullptr);
    Assert(result == 0, result, "Unable to init repair screen");
    // The original loaded the background (lsrbk00) as the screen's picture; the screen draws it each frame.
    InitLive("lsrbk00.tga");
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

    pane = new MCScrollPane;

    if (pane != nullptr)
    {
        pane->Init();
    }

    UnitPane = pane;
    Assert(pane != nullptr, 0, "Not enough memory for vehicleScroll");
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsrbk01.tga", ArtPath);
    pane->Init(0x1aa, 0x1cc, 0xd3, 0x11, fileName);

    ShowGuiWindow(0);
    AddChild(InventoryPane);
    AddChild(UnitPane);
    ScreenWindow->AddChild(this);
}

auto MCRepairScreen::Destroy() -> void
{
    if (InventoryPane != nullptr)
    {
        // The inventory ports belong to the Logistics object.
        InventoryPane->SetDisplayPort(nullptr, 0, -1);
        delete InventoryPane;
        InventoryPane = nullptr;
    }

    if (UnitPane != nullptr)
    {
        UnitPane->SetDisplayPort(nullptr, -1, -1);
        delete UnitPane;
        UnitPane = nullptr;
    }

    MCLogObject::Destroy();
}

auto MCRepairScreen::SelectMech(MCLogMech* mech) -> void
{
    MCLogMech* oldMech = GlobalLogPtr->RepairScreen->SelectedMech;
    GlobalLogPtr->RepairScreen->SelectedMech = mech;

    if (SelectedVehicle != nullptr)
    {
        MCVehicleRepairBlock* block = SelectedVehicle->RepairBlock;
        SelectedVehicle = nullptr;
        block->DrawBackground(block->SlotIndex, nullptr);
    }

    if (mech != nullptr)
    {
        mech->RepairBlock->DrawBackground(mech->RepairBlock->SlotIndex, nullptr);
    }

    if (oldMech != nullptr)
    {
        oldMech->RepairBlock->DrawBackground(oldMech->RepairBlock->SlotIndex, nullptr);
    }

    if (GlobalLogPtr->CurrentInvTab == 1)
    {
        SetUpPilotInv(0, -1);
        return;
    }

    if (GlobalLogPtr->CurrentInvTab == 2)
    {
        SetUpCompInv(0, -1);
    }
}

auto MCRepairScreen::SelectVehicle(MCLogVehicle* vehicle) -> void
{
    MCLogVehicle* oldVehicle = GlobalLogPtr->RepairScreen->SelectedVehicle;
    GlobalLogPtr->RepairScreen->SelectedVehicle = vehicle;

    if (SelectedMech != nullptr)
    {
        MCMechRepairBlock* block = SelectedMech->RepairBlock;
        SelectedMech = nullptr;
        block->DrawBackground(block->SlotIndex, nullptr);
    }

    if (vehicle != nullptr)
    {
        vehicle->RepairBlock->DrawBackground(vehicle->RepairBlock->SlotIndex, nullptr);
    }

    if (oldVehicle != nullptr)
    {
        oldVehicle->RepairBlock->DrawBackground(oldVehicle->RepairBlock->SlotIndex, nullptr);
    }

    if (GlobalLogPtr->CurrentInvTab == 1)
    {
        SetUpPilotInv(0, -1);
    }

    if (GlobalLogPtr->CurrentInvTab == 2)
    {
        SetUpCompInv(0, -1);
    }
}

auto MCRepairScreen::AddMechToList(MCLogMech*) -> void
{
    // The new mech is first in the force list: the old rows move down one block (the original copied them down in a
    // new picture; the view draws each row where its block is).
    MCScrollPane* pane = UnitPane;
    pane->SetDisplayPort(NewUnitRowsView(pane), -1, 0);
    PlaceUnitBlocks(pane);
}

auto MCRepairScreen::AddVehicleToList(MCLogVehicle* vehicle) -> void
{
    // The new vehicle is first in the vehicle list, right after the mechs: the vehicle rows move down one block.
    MCScrollPane* pane = UnitPane;
    pane->SetDisplayPort(NewUnitRowsView(pane), -1, 0);
    vehicle->RepairBlock->DrawBackground(GlobalLogPtr->ForceMechList->GetMechCount(), nullptr);
    PlaceUnitBlocks(pane);
}

auto MCRepairScreen::RemoveMechFromList(MCLogMech* mech) -> void
{
    RemoveUnitRow(UnitPane);
}

auto MCRepairScreen::RemoveVehicleFromList(MCLogVehicle* vehicle) -> void
{
    RemoveUnitRow(UnitPane);
}

auto MCRepairScreen::NewUnitRowsView(MCScrollPane* pane) -> MCLogPort*
{
    auto* port = new MCLogPort;
    int32_t height = (GlobalLogPtr->ForceVehicleList->GetVehicleCount() + GlobalLogPtr->ForceMechList->GetMechCount()) *
                     UnitBlockHeight;

    if (height < pane->Height())
    {
        height = pane->Height();
    }

    port->InitView(pane->Width() - 0xd, height);
    port->DrawContent = DrawUnitRows;
    return port;
}

auto MCRepairScreen::DrawBackground() -> void
{
    // The background art was painted over everything the screen showed.
    ScreenChrome.Clear();
    Info.Clear();

    if (SelectedMech != nullptr && SelectedMech->RepairBlock != nullptr)
    {
        SelectedMech->RepairBlock->DrawButtons(nullptr);
    }
}

auto MCRepairScreen::HandleEvent(MCGuiEvent* event) -> void
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

        if (inside(2, 2, 0xd1, 0xd))
        {
            ShowHelp(0x1d);
        }
        else if (inside(2, 0x10, 0xd1, 0x21))
        {
            ShowHelp(0x286);
            GlobalLogPtr->HoverScreenButton(this, 0);
        }
        else if (inside(2, 0x22, 0xd1, 0x33))
        {
            ShowHelp(0x1e);
            GlobalLogPtr->HoverScreenButton(this, 1);
        }
        else if (inside(2, 0x34, 0xd1, 0x45))
        {
            ShowHelp(0x41);
            GlobalLogPtr->HoverScreenButton(this, 2);
        }
        else if (inside(2, 0x46, 0xd1, 0x57))
        {
            ShowHelp(0x42);
        }
        else if (inside(0x20c, 2, 0x24d, 0xd))
        {
            ShowHelp(0x1f);
        }
        else if (inside(0x19b, 4, 0x24a, 0xf))
        {
            ShowHelp(0x1f);
        }
        else if (inside(0xc6, 0x66, 0xd1, 0x95))
        {
            ShowHelp(0x29);
        }
        else if (inside(0xc6, 0x97, 0xd1, 0xef))
        {
            ShowHelp(0x2a);
        }
        else if (inside(0xc6, 0xf1, 0xd1, 0x140))
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
                    ShowHelp(0x31);
                    break;
                case 1:
                    ShowHelp(0x33);
                    break;
                case 2:
                    ShowHelp(0x32);
                    break;
                case 3:
                    ShowHelp(0x47);
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
            ShowHelp(0x43);
        }
        else
        {
            GlobalLogPtr->Ticker->SetString(nullptr);
        }
    }

    if (event->Type == 1)
    {
        MouseDown = -1;
        POINT point{xPos - GlobalX(), yPos - GlobalY()};
        RECT area{2, 0x34, 0xd1, 0x45};

        if (PtInRect(&area, point) != 0)
        {
            GlobalLogPtr->SetUpPurchaseScreen(-1);
            return;
        }

        area.top = 0x22;
        area.bottom = 0x32;

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
    }

    if (event->Type == 4)
    {
        MouseDown = 0;
        // The original fetches globalX() and globalY() here and drops them.
        GlobalX();
        GlobalY();
    }

    if (event->Type == 9)
    {
        bool ctrlAlt = MCInput::GetAsyncKeyState(VK_CONTROL) != 0 && MCInput::GetAsyncKeyState(VK_MENU) != 0;

        // Original behaviour (OB-077): the key byte, zero-extended, is compared with -0x45 (VK_OEM_PLUS as a signed
        // char), so the Ctrl+Alt+= resource cheat never fires.
        if (MPlayer == nullptr && static_cast<int32_t>(key) == -0x45 && ctrlAlt)
        {
            ResourcePoints += 1000;
        }
    }

    if (event->Type == 8)
    {
        GlobalLogPtr->ProcessCheatCode(event->ScanCode);
        return;
    }

    if (event->Type == 0x13)
    {
        // Blink the briefing button.
        ScreenChrome.BlinkLit = BriefingBlink != 0;
        BriefingBlink = BriefingBlink == 0 ? 1 : 0;
    }
}

auto MCRepairScreen::ShowGuiWindow(int show) -> void
{
    ShowWindow = show;
    InventoryPane->ShowGuiWindow(show);
    UnitPane->ShowGuiWindow(show);
}

auto MCRepairScreen::Display() -> void
{
    MCLogObject::Display();
    MCLogObject* screen = GlobalLogPtr->CurrentScreen;

    if (screen != GlobalLogPtr->RepairScreen && screen != GlobalLogPtr->PurchaseScreen &&
        screen != GlobalLogPtr->BriefingScreen && screen != GlobalLogPtr->SessionScreen)
    {
        return;
    }

    // The figure and the clock are drawn by the screens each frame (Logistics::drawScreenChrome).
    MCPort::StrTime(GlobalLogPtr->TimeString);
}

auto ResourceFigureText(char* text, size_t size) -> void
{
    // The original left the text uninitialised for a resourceDisplayState the switch doesn't list.
    text[0] = '\0';

    switch (ResourceDisplayState)
    {
        case 0:
            std::snprintf(text, size, "%d", ResourcePoints);
            break;
        // States 1 and 2 showed the logistics heap's free memory and largest free block; the heap is gone, so they
        // show nothing.
        default:
            break;
    }
}
