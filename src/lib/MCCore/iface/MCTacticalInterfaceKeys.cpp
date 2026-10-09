#include "stdafx.h"
#include "iface/MCTacticalInterface.h"
#include "ai/MCTacticalOrder.h"
#include "camera/MCCamera.h"
#include "camera/MCMainWindow.h"
#include "gui/atextbox.h"
#include "iface/MCCommandParser.h"
#include "iface/MCMechBar.h"
#include "main/main.h"
#include "mission/MCScenario.h"
#include "network/multplyr.h"
#include "object/MCBattleMech.h"
#include "object/MCForces.h"
#include "object/MCMover.h"
#include "object/MCMoverGroup.h"
#include "object/MCObjectQueue.h"
#include "object/MCObjectSystem.h"
#include "sound/MCSoundSystem.h"
#include "terrain/MCTacticalMap.h"

namespace
{
    /// <summary>
    /// <paramref name="key"/> (a scan code) with the modifiers <paramref name="event"/> carries, in a binding's form.
    /// </summary>
    uint32_t WithModifiers(uint32_t key, const MCGuiEvent* event)
    {
        key &= ~KeyShift;

        if (event->ShiftKey != 0)
        {
            key += KeyShift;
        }

        key &= ~KeyCtrl;

        if (event->CtrlKey != 0)
        {
            key += KeyCtrl;
        }

        key &= ~KeyAlt;

        if (event->AltKey != 0)
        {
            key += KeyAlt;
        }

        return key;
    }

    /// <summary>Whether <paramref name="key"/> is a forced-order key (either ctrl, or one of the four bindings).</summary>
    bool IsForcedOrderKey(const MCTacticalInterface& iface, int16_t scanCode, uint32_t key)
    {
        return scanCode == 0x1d || scanCode == 0x11d || key == iface.Key(MCKeyCommand::ForcedOrder) ||
               key == iface.Key(MCKeyCommand::ForcedRun) || key == iface.Key(MCKeyCommand::ForcedJump) ||
               key == iface.Key(MCKeyCommand::ForcedOrderAlternate);
    }

    /// <summary>Marks whether the mech bar movers draw their order lines (while a forced order is being given).</summary>
    void DrawOrderLines(MCTacticalInterface& iface, bool selectedOnly, int32_t draw)
    {
        auto mark = [draw](MCFriendlyMechIcon* button)
        {
            if (button != nullptr && button->Mover != nullptr && IsMoverClass(button->Mover->ObjectClass))
            {
                static_cast<MCMover*>(button->Mover)->DrawOrderLines = draw;
            }
        };

        if (selectedOnly)
        {
            for (int32_t partId : iface.SelectedMovers)
            {
                mark(iface.MechBar->GetButtonFromID(partId));
            }

            return;
        }

        for (const MCGuiOwned<MCFriendlyMechIcon>& button : iface.MechBar->Buttons)
        {
            mark(button.get());
        }
    }

    /// <summary>Brings up the tactical map (out of its hidden slide) on <paramref name="page"/>.</summary>
    void ShowTacticalMapPage(MCTacmapPage page)
    {
        TacticalMap()->HideMe(0);
        TacticalMap()->SetDisplayType(page);
    }

    /// <summary>The lance (0-3) <paramref name="key"/> names among the four bindings from <paramref name="first"/>, or -1.</summary>
    int32_t LanceKey(const MCTacticalInterface& iface, uint32_t key, MCKeyCommand first)
    {
        for (int32_t lance = 0; lance < 4; lance++)
        {
            if (key == iface.Key(static_cast<MCKeyCommand>(static_cast<int32_t>(first) + lance)))
            {
                return lance;
            }
        }

        return -1;
    }

    /// <summary>The artillery button (0-3) <paramref name="key"/> arms, or -1.</summary>
    int32_t ArtilleryKey(const MCTacticalInterface& iface, uint32_t key)
    {
        constexpr std::array<MCKeyCommand, 4> buttons = {MCKeyCommand::Artillery1, MCKeyCommand::Artillery2,
                                                         MCKeyCommand::Artillery3, MCKeyCommand::Artillery4};

        for (int32_t button = 0; button < 4; button++)
        {
            if (key == iface.Key(buttons[button]))
            {
                return button;
            }
        }

        return -1;
    }

    /// <summary>A key that picks a mode, with the mode; <c>Armed</c> modes need an armed mover selected.</summary>
    struct MCModeKey
    {
        MCKeyCommand Key = MCKeyCommand::Unused;
        MCInterfaceMode Mode = MCInterfaceMode::None;
        bool Armed = false;
    };
}

auto MCTacticalInterface::HandleKeyDown(MCGuiEvent* event) -> void
{
    // Fold right alt/ctrl onto the left ones, then add the modifiers.
    const int16_t scanCode = event->ScanCode;
    uint32_t key = static_cast<uint32_t>(static_cast<int32_t>(scanCode));

    if (scanCode == 0x138 || scanCode == 0x11d)
    {
        key = static_cast<uint32_t>(static_cast<int16_t>(scanCode - 0x100));
    }

    key = WithModifiers(key, event);
    auto is = [this, key](MCKeyCommand command) { return key == Key(command); };

    if (Scenario() != nullptr)
    {
        if ((event->CtrlKey != 0 && (scanCode == 0x1d || scanCode == 0x11d)) || is(MCKeyCommand::ForcedOrder) ||
            is(MCKeyCommand::ForcedRun) || is(MCKeyCommand::ForcedJump) || is(MCKeyCommand::ForcedOrderAlternate))
        {
            if (ForcedOrder == MCForcedOrder::None && !ForcingOrder)
            {
                ForcedOrder = MCForcedOrder::Move;
            }

            ForcingOrder = true;
            DrawOrderLines(*this, true, 1);
        }

        // The keys that pick a mode, in the order the original tested them (after the map, page and zoom keys).
        auto sendToSelection = [this](MCTacticalOrderCode code)
        {
            if (AnySelected() && CommandParser != nullptr)
            {
                MCTacticalOrder order;
                order.Reset();
                order.Reset(MCOrderOrigin::Player, code, 0);
                CommandParser->SendTacOrder(order, false);
            }
        };

        MCTacticalMap* tacMap = ::TacticalMap();
        const int32_t lanceToSelect = LanceKey(*this, key, MCKeyCommand::SelectLance1);
        const int32_t lanceToAdd = LanceKey(*this, key, MCKeyCommand::AddLance1);
        const int32_t artillery = ArtilleryKey(*this, key);

        if (is(MCKeyCommand::ScrollUp))
        {
            ScrollDirection = 0;
        }
        else if (is(MCKeyCommand::ScrollDown))
        {
            ScrollDirection = 4;
        }
        else if (is(MCKeyCommand::ScrollLeft))
        {
            ScrollDirection = 6;
        }
        else if (is(MCKeyCommand::ScrollRight))
        {
            ScrollDirection = 2;
        }
        else if (is(MCKeyCommand::TacticalMapScrollUp) || is(MCKeyCommand::TacticalMapScrollDown) ||
                 is(MCKeyCommand::TacticalMapScrollLeft) || is(MCKeyCommand::TacticalMapScrollRight))
        {
            if (tacMap != nullptr)
            {
                ShowTacticalMapPage(MCTacmapPage::Map);
                TacScrollDirection = is(MCKeyCommand::TacticalMapScrollUp)     ? 0
                                     : is(MCKeyCommand::TacticalMapScrollDown) ? 4
                                     : is(MCKeyCommand::TacticalMapScrollLeft) ? 6
                                                                               : 2;
            }
        }
        else if (is(MCKeyCommand::HoldTacticalMap))
        {
            TacMapShown = true;
        }
        else if (is(MCKeyCommand::TacticalMapZoomIn) || is(MCKeyCommand::TacticalMapZoomInAlternate))
        {
            if (tacMap != nullptr)
            {
                ShowTacticalMapPage(MCTacmapPage::Map);
                APostMessage(tacMap, 0x1a);
            }
        }
        else if (is(MCKeyCommand::TacticalMapZoomOut) || is(MCKeyCommand::TacticalMapZoomOutAlternate))
        {
            if (tacMap != nullptr)
            {
                ShowTacticalMapPage(MCTacmapPage::Map);
                APostMessage(tacMap, 0x1b);
            }
        }
        else if (is(MCKeyCommand::MapPage))
        {
            TacMapShown = false;

            if (tacMap != nullptr)
            {
                ShowTacticalMapPage(MCTacmapPage::Map);
            }
        }
        else if (is(MCKeyCommand::SalvagePage))
        {
            TacMapShown = false;

            if (tacMap != nullptr)
            {
                ShowTacticalMapPage(MCTacmapPage::Salvage);
            }
        }
        else if (MPlayer != nullptr && is(MCKeyCommand::Chat))
        {
            // The chat line.
            TacMapShown = false;

            if (tacMap != nullptr && Scenario() != nullptr && EventsToMissionResultsScreen == 0 && GameAsked == 0)
            {
                ShowTacticalMapPage(MCTacmapPage::Salvage);
                MCGuiObject* chatInput = tacMap->ChatWindow->ChatInput;

                if (Application->TextObject() != chatInput)
                {
                    Application->SetText(chatInput);
                    FirstReturn = 1;
                }
            }
        }
        else if (is(MCKeyCommand::MissionPage))
        {
            TacMapShown = false;

            if (tacMap != nullptr)
            {
                ShowTacticalMapPage(MCTacmapPage::Mission);
            }
        }
        else if (is(MCKeyCommand::InfoPage))
        {
            TacMapShown = false;

            if (tacMap != nullptr)
            {
                ShowTacticalMapPage(MCTacmapPage::Info);
            }
        }
        else if (is(MCKeyCommand::ZoomIn) || is(MCKeyCommand::ZoomInAlternate))
        {
            ZoomIn();
        }
        else if (is(MCKeyCommand::ZoomOut) || is(MCKeyCommand::ZoomOutAlternate))
        {
            ZoomOut();
        }
        else if (is(MCKeyCommand::Stop) && Application->GrabbedObject() == nullptr)
        {
            SetMode(MCInterfaceMode::Stop);
            sendToSelection(MCTacticalOrderCode::Stop);
        }
        else if (is(MCKeyCommand::PowerUp))
        {
            SetMode(MCInterfaceMode::PowerUp);
            sendToSelection(MCTacticalOrderCode::PowerUp);
        }
        else if (is(MCKeyCommand::PowerDown))
        {
            SetMode(MCInterfaceMode::PowerDown);
            sendToSelection(MCTacticalOrderCode::PowerDown);
        }
        else if (is(MCKeyCommand::Eject))
        {
            SetMode(MCInterfaceMode::Eject);
        }
        else if (is(MCKeyCommand::Run) || is(MCKeyCommand::RunAlternate) || is(MCKeyCommand::ForcedRun))
        {
            SetMode(MCInterfaceMode::Run);
        }
        else if (is(MCKeyCommand::Info))
        {
            SetMode(MCInterfaceMode::Info);
        }
        else
        {
            static constexpr MCModeKey modeKeys[] = {
                {MCKeyCommand::AttackOptimalRange, MCInterfaceMode::AttackOptimalRange, true},
                {MCKeyCommand::AttackLongRange, MCInterfaceMode::AttackLongRange, true},
                {MCKeyCommand::AttackMediumRange, MCInterfaceMode::AttackMediumRange, true},
                {MCKeyCommand::AttackConservingAmmo, MCInterfaceMode::AttackConservingAmmo, true},
                {MCKeyCommand::AttackShortRange, MCInterfaceMode::AttackShortRange, true},
                {MCKeyCommand::AttackFromPosition, MCInterfaceMode::AttackFromPosition, true},
                {MCKeyCommand::Jump, MCInterfaceMode::Jump, false},
                {MCKeyCommand::JumpAlternate, MCInterfaceMode::Jump, false},
                {MCKeyCommand::ForcedJump, MCInterfaceMode::Jump, false},
                {MCKeyCommand::Guard, MCInterfaceMode::Guard, false},
                {MCKeyCommand::AimHead, MCInterfaceMode::AimHead, true},
                {MCKeyCommand::AimLeftTorso, MCInterfaceMode::AimLeftTorso, true},
                {MCKeyCommand::AimRightTorso, MCInterfaceMode::AimRightTorso, true},
                {MCKeyCommand::AimCenterTorso, MCInterfaceMode::AimCenterTorso, true},
                {MCKeyCommand::AimLeftArm, MCInterfaceMode::AimLeftArm, true},
                {MCKeyCommand::AimRightArm, MCInterfaceMode::AimRightArm, true},
                {MCKeyCommand::AimLeftLeg, MCInterfaceMode::AimLeftLeg, true},
                {MCKeyCommand::AimRightLeg, MCInterfaceMode::AimRightLeg, true},
            };

            const auto modeKey = std::ranges::find_if(modeKeys, [&](const MCModeKey& entry)
                                                      { return is(entry.Key) && (!entry.Armed || AnySelected(true)); });

            if (modeKey != std::end(modeKeys))
            {
                SetMode(modeKey->Mode);
            }
            else if (is(MCKeyCommand::BreakLances) && AnySelected())
            {
                // Break up the selected movers' lances; a point takes its whole lance with it.
                for (int32_t partId : SelectedMovers)
                {
                    MCFriendlyMechIcon* button = MechBar->GetButtonFromID(partId);

                    if (button == nullptr)
                    {
                        continue;
                    }

                    if (button->IsPoint)
                    {
                        for (const MCGuiOwned<MCFriendlyMechIcon>& other : MechBar->Buttons)
                        {
                            if (other.get() != button && other->Lance == button->Lance)
                            {
                                other->Lance = NoLance;
                            }
                        }
                    }

                    button->Lance = NoLance;

                    // Port fix: the original read the group of a null mover when the button was missing.
                    if (auto* member = static_cast<MCMover*>(button->Mover);
                        member != nullptr && member->Group != nullptr)
                    {
                        member->Group->Remove(member);
                    }
                }

                // Original behaviour (OB-157): each deselected lance leaves the list, so the walk skips every other one.
                for (size_t i = 0; i < SelectedLances.size(); i++)
                {
                    DeselectLance(SelectedLances[i]);
                }

                MechBar->PlaceButtons(true);
            }
            else if (lanceToSelect >= 0)
            {
                MCMoverGroup* lance = HomeCommander()->GetGroup(lanceToSelect);

                if (lance->NumMovers() > 0)
                {
                    ClearMechSelection();
                    SelectLance(lance);
                    CommandParser->AddSubject(lance);
                    SoundSystem()->PlayDigitalSample(0x30, 1, nullptr, 0, 0);
                }
            }
            else if (lanceToAdd >= 0)
            {
                MCMoverGroup* lance = HomeCommander()->GetGroup(lanceToAdd);

                if (lance->NumMovers() > 0)
                {
                    SelectLance(lance);
                    CommandParser->AddSubject(lance);
                    SoundSystem()->PlayDigitalSample(0x30, 1, nullptr, 0, 0);
                }
            }
            else if (is(MCKeyCommand::CameraFollow))
            {
                SetMode(MCInterfaceMode::CameraFollow);
            }
            else if (tacMap != nullptr && artillery >= 0)
            {
                tacMap->ActivateArtillery(artillery, 1);
            }
            else if (BunnyStrikesOn != 0 && is(MCKeyCommand::DebugStrike))
            {
                SetMode(MCInterfaceMode::DebugStrike);
            }
            else
            {
                if (is(MCKeyCommand::TogglePalette))
                {
                    TogglePalette();
                }

                // Laying mines: one selected mech that can.
                if ((is(MCKeyCommand::LayMines) || is(MCKeyCommand::LayMinesAlternate)) && SelectedMovers.size() == 1)
                {
                    MCBaseObject* selected = ObjectList()->FindObjectFromPart(SelectedMovers[0]);

                    if (selected == nullptr || selected->ObjectClass != MCObjectClass::BattleMech ||
                        !static_cast<MCBattleMech*>(selected)->SecondStepPrinted ||
                        !static_cast<MCBattleMech*>(selected)->FirstStepPrinted)
                    {
                        return;
                    }

                    SetMode(MCInterfaceMode::LayMines);
                }
            }
        }
    }

    // A key other than a lance-link key ends the hidden-cursor state of a lance link.
    if (Application->CursorHidden != 0 &&
        Application->CursorShape == static_cast<int32_t>(MCInterfaceCursor::LinkLance) && !IsLanceLink(CurrentMode))
    {
        Application->CursorHidden = 0;
    }
}

auto MCTacticalInterface::HandleKeyUp(MCGuiEvent* event) -> void
{
    // Only the right alt is folded onto the left one here.
    const int16_t scanCode = event->ScanCode;
    uint32_t key = static_cast<uint32_t>(static_cast<int32_t>(scanCode));

    if (scanCode == 0x138)
    {
        key = 0x38;
    }

    key = WithModifiers(key, event);

    if (IsForcedOrderKey(*this, scanCode, key))
    {
        ForcingOrder = false;
        ForcedOrder = MCForcedOrder::None;
        DrawOrderLines(*this, false, 0);
    }

    if (event->Target != nullptr && event->Target->Parent == MainHolder())
    {
        return;
    }

    if (!IsLanceLink(CurrentMode))
    {
        SetMode(MCInterfaceMode::None);
    }

    ScrollDirection = -1;
    TacScrollDirection = -1;

    if (scanCode == RotateKey && Eye != nullptr)
    {
        ScrollWait = 0;
    }

    if (MCTacticalMap* tacMap = ::TacticalMap(); tacMap != nullptr)
    {
        if (static_cast<int16_t>(key) == static_cast<int16_t>(Key(MCKeyCommand::HoldTacticalMap)) && TacMapShown)
        {
            tacMap->HideMe(tacMap->IsHidden() == 0);
        }

        if (const int32_t artillery = ArtilleryKey(*this, key); artillery >= 0)
        {
            tacMap->ActivateArtillery(artillery, 0);
        }
    }

    // The lance-link keys (ctrl+F1-F4) wait for a click on the lance's point.
    if (const int32_t lance = LanceKey(*this, key, MCKeyCommand::LinkLance1); lance >= 0 && AnySelected())
    {
        SetMode(static_cast<MCInterfaceMode>(static_cast<int32_t>(MCInterfaceMode::LinkLance1) + lance));
        Application->SetCurrentCursor(static_cast<MCCursorType>(MCInterfaceCursor::LinkLance));
        Application->CursorHidden = 1;
    }

    if (key == Key(MCKeyCommand::SelectVisible) && Scenario() != nullptr)
    {
        SelectVisibleMechs();
    }
}
