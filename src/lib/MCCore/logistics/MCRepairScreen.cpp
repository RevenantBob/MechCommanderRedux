#include "stdafx.h"
#include "logistics/MCRepairScreen.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCScrollPane.h"
#include "lib/MCFatal.h"
#include "logistics/MCTicker.h"
#include "logistics/MCMainMenu.h"
#include "logistics/MCPurchaseScreen.h"
#include "logistics/MCLogToolButton.h"
#include "logistics/MCLogChatInput.h"
#include "logistics/MCPlayerNameObject.h"
#include "logistics/MCSessionScreen.h"
#include "logistics/MCMechRepairBlock.h"
#include "logistics/MCVehicleRepairBlock.h"
#include "logistics/MCBriefingBox.h"
#include "logistics/MCLogRows.h"
#include "logistics/MCPurMechList.h"
#include "logistics/MCPurPilotList.h"
#include "logistics/MCPurVehicleList.h"
#include "logistics/MCMechPurchaseBlock.h"
#include "logistics/MCPilotPurchaseBlock.h"
#include "logistics/MCVehiclePurchaseBlock.h"
#include "logistics/MCCompPurchaseBlock.h"
#include "logistics/MCUnitLimits.h"
#include "logistics/MCPurProfile.h"
#include "main/logistics.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "platform/MCInput.h"
#include "vfx/MCVfxFunctions.h"
#include "logistics/MCBriefingScreen.h"

int32_t ResourceDisplayState = 0;

namespace
{
    /// <summary>The sample the screen's tabs and buttons play.</summary>
    constexpr uint32_t TabSample = 0x36;
    /// <summary>
    /// The key the Ctrl+Alt resource cheat compares with: VK_OEM_PLUS as a signed char. Original behaviour (OB-077):
    /// the key byte is zero-extended, so the cheat never fires.
    /// </summary>
    constexpr int32_t ResourceCheatKey = -0x45;
    /// <summary>The resource points the cheat would add.</summary>
    constexpr int32_t ResourceCheatPoints = 1000;

    /// <summary>Set while the left button is down on the screen; only written.</summary>
    bool MouseDown = false;

    /// <summary>The blink phase of the briefing button's highlight, flipped by each timer event.</summary>
    bool BriefingBlink = false;

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
            const int32_t top = block->SlotIndex * MCLogInvScreen::UnitBlockHeight;
            const int32_t screenTop = place.OriginY + top;

            if (screenTop <= place.Scissor.Y1 && place.Scissor.Y0 < screenTop + MCLogInvScreen::UnitBlockHeight)
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

        auto place = [&](auto* block)
        {
            block->SlotIndex = row;
            block->MoveTo(0, row * MCLogInvScreen::UnitBlockHeight - pane->GetScrollOffset(), false);
            block->SetDepth(100);
            ++row;
        };

        for (MCLogMech* mech = GlobalLogPtr->ForceMechList->Mechs; mech != nullptr; mech = mech->Next)
        {
            place(mech->RepairBlock);
        }

        for (MCLogVehicle* vehicle = GlobalLogPtr->ForceVehicleList->Vehicles; vehicle != nullptr;
             vehicle = vehicle->Next)
        {
            place(vehicle->RepairBlock);
        }
    }

    /// <summary>
    /// Gives the unit pane a new rows view and puts the blocks at their rows (the original copied the rows about in a
    /// new picture; the view draws each row where its block is).
    /// </summary>
    void RebuildUnitRows(MCScrollPane* pane)
    {
        pane->SetDisplayPort(MCRepairScreen::NewUnitRowsView(pane), false);
        PlaceUnitBlocks(pane);
    }

    /// <summary>Puts string <paramref name="id"/> on the ticker.</summary>
    void ShowHelp(uint32_t id)
    {
        GlobalLogPtr->Ticker->SetString(LoadGameString(id, 0xfe));
    }
}

auto MCRepairScreen::Init() -> void
{
    ChatBlinking = false;
    SelectedMech = nullptr;
    SelectedVehicle = nullptr;
    const int32_t result = MCLogObject::Init(0, 0, 0x280, 0x1e0);
    Assert(result == 0, static_cast<uint32_t>(result), "Unable to init repair screen");
    // The original loaded the background (lsrbk00) as the screen's picture; the screen draws it each frame.
    InitLive("lsrbk00.tga");
    MakePanes(false);
    ShowGuiWindow(false);
    ScreenWindow()->AddChild(this);
}

auto MCRepairScreen::Destroy() -> void
{
    FreePanes();
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
        SetUpPilotInv(false, true);
        return;
    }

    if (GlobalLogPtr->CurrentInvTab == 2)
    {
        SetUpCompInv(false, true);
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
        SetUpPilotInv(false, true);
    }

    if (GlobalLogPtr->CurrentInvTab == 2)
    {
        SetUpCompInv(false, true);
    }
}

auto MCRepairScreen::AddMechToList([[maybe_unused]] MCLogMech* mech) -> void
{
    RebuildUnitRows(UnitPane);
}

auto MCRepairScreen::AddVehicleToList(MCLogVehicle* vehicle) -> void
{
    UnitPane->SetDisplayPort(NewUnitRowsView(UnitPane), false);
    vehicle->RepairBlock->DrawBackground(GlobalLogPtr->ForceMechList->GetMechCount(), nullptr);
    PlaceUnitBlocks(UnitPane);
}

auto MCRepairScreen::RemoveMechFromList([[maybe_unused]] MCLogMech* mech) -> void
{
    RebuildUnitRows(UnitPane);
}

auto MCRepairScreen::RemoveVehicleFromList([[maybe_unused]] MCLogVehicle* vehicle) -> void
{
    RebuildUnitRows(UnitPane);
}

auto MCRepairScreen::NewUnitRowsView(MCScrollPane* pane) -> std::unique_ptr<MCLogPort>
{
    auto port = std::make_unique<MCLogPort>();
    const int32_t rows =
        GlobalLogPtr->ForceVehicleList->GetVehicleCount() + GlobalLogPtr->ForceMechList->GetMechCount();
    port->InitView(pane->Width() - 0xd, std::max(rows * UnitBlockHeight, pane->Height()));
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

    const int32_t xPos = event->X;
    const int32_t yPos = event->Y;
    const uint8_t key = event->Key;

    if (key == 0 && event->Type != MCGuiEventType::Timer)
    {
        // The help line for whatever the mouse is over, and the highlighted screen button.
        GlobalLogPtr->DrawScreenButtons();
        DrawBlankInvInfoBlock(-1);
        const POINT point{xPos, yPos};
        auto inside = [&point](int32_t left, int32_t top, int32_t right, int32_t bottom)
        {
            const RECT area{left, top, right, bottom};
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
        else if (inside(0x20c, 2, 0x24d, 0xd) || inside(0x19b, 4, 0x24a, 0xf))
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
            // The inventory tab's help (the original put its unset string buffer on the ticker for another tab).
            static constexpr std::array<uint32_t, 4> tabHelp = {0x31, 0x33, 0x32, 0x47};
            const int32_t tab = GlobalLogPtr->CurrentInvTab;

            if (tab >= 0 && tab < 4)
            {
                ShowHelp(tabHelp[static_cast<size_t>(tab)]);
            }
            else
            {
                GlobalLogPtr->Ticker->SetString({});
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
            GlobalLogPtr->Ticker->SetString({});
        }
    }

    if (event->Type == MCGuiEventType::LeftButtonDown)
    {
        MouseDown = true;
        const POINT point{xPos - GlobalX(), yPos - GlobalY()};
        auto inside = [&point](int32_t left, int32_t top, int32_t right, int32_t bottom)
        {
            const RECT area{left, top, right, bottom};
            return PtInRect(&area, point) != 0;
        };

        if (inside(2, 0x34, 0xd1, 0x45))
        {
            GlobalLogPtr->SetUpPurchaseScreen(-1);
            return;
        }

        if (inside(2, 0x22, 0xd1, 0x32))
        {
            GlobalLogPtr->SetUpBriefingScreen(-1);
            return;
        }

        if (inside(2, 0x10, 0xd1, 0x21))
        {
            if (MPlayer != nullptr)
            {
                CheckExit();
                return;
            }

            PlayLogSound(TabSample);
            GlobalLogPtr->SetUpMainScreen(0);
            return;
        }

        // The inventory tabs.
        if (inside(0xc2, 0x67, 0xd1, 0x95))
        {
            PlayLogSound(TabSample);
            SetUpMechInv(true, true);
            SetUpMechPurchase();
        }

        if (inside(0xc2, 0x96, 0xd1, 0xf1))
        {
            PlayLogSound(TabSample);
            SetUpPilotInv(true, true);
            SetUpPilotPurchase();
        }

        if (inside(0xc2, 0xf2, 0xd1, 0x140))
        {
            PlayLogSound(TabSample);
            SetUpCompInv(true, true);
            SetUpCompPurchase();
        }

        if (inside(0xc2, 0x141, 0xd1, 0x17c))
        {
            PlayLogSound(TabSample);
            SetUpVhclInv(true, true);
            SetUpVehiclePurchase();
        }
    }

    if (event->Type == MCGuiEventType::LeftButtonUp)
    {
        MouseDown = false;
    }

    if (event->Type == MCGuiEventType::KeyDown)
    {
        const bool ctrlAlt = MCInput::GetAsyncKeyState(VK_CONTROL) != 0 && MCInput::GetAsyncKeyState(VK_MENU) != 0;

        if (MPlayer == nullptr && static_cast<int32_t>(key) == ResourceCheatKey && ctrlAlt)
        {
            ResourcePoints += ResourceCheatPoints;
        }
    }

    if (event->Type == MCGuiEventType::KeyUp)
    {
        GlobalLogPtr->ProcessCheatCode(event->ScanCode);
        return;
    }

    if (event->Type == MCGuiEventType::Timer)
    {
        // Blink the briefing button.
        ScreenChrome.BlinkLit = BriefingBlink;
        BriefingBlink = !BriefingBlink;
    }
}

auto MCRepairScreen::ShowGuiWindow(bool show) -> void
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

    // The figure and the clock are drawn by the screens each frame (Logistics::DrawScreenChrome).
    MCPort::StrTime(GlobalLogPtr->TimeString);
}

auto ResourceFigureText() -> std::string
{
    // States 1 and 2 showed the logistics heap's free memory and largest free block; the heap is gone, so they show
    // nothing (the original left the text unset for another state).
    return ResourceDisplayState == 0 ? std::format("{}", ResourcePoints) : std::string();
}
