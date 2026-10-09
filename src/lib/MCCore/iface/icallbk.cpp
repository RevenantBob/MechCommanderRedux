#include "stdafx.h"
#include "iface/icallbk.h"
#include "ai/MCTacticalOrder.h"
#include "camera/MCCamera.h"
#include "camera/MCMainWindow.h"
#include "gui/ahelp.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "iface/iface.h"
#include "iface/parser.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "object/MCMasterComponent.h"
#include "object/MCForces.h"
#include "object/MCBigGameObject.h"
#include "object/MCBattleMech.h"
#include "object/MCBattleMechType.h"
#include "object/MCMechGameSystem.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"
#include "object/MCMechWarrior.h"
#include "platform/MCFrameLog.h"
#include "sound/soundsys.h"
#include "terrain/MCTerrain.h"
#include "terrain/MCTacticalMap.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>Points the main window's active camera at the mover and shows the floating tags.</summary>
    /// <returns>False when there is no camera.</returns>
    bool CenterCameraOn(MCBaseObject* mover)
    {
        // Port fix: with no active pane the original used the icon itself as the camera, calling its resize
        // (slot 0x20) with the mover pointer.
        MCCamera* camera = nullptr;

        if (MainHolder()->GetActivePane() != nullptr)
        {
            camera = MainHolder()->GetActivePane()->GetCamera();
        }

        if (camera == nullptr)
        {
            return false;
        }

        camera->ChangeTarget(mover, 0);
        TheInterface->FloatingTags[0]->ShowGuiWindow(-1);
        return true;
    }

    /// <summary>
    /// A single-mover order: sent to the server in a multiplayer client, otherwise given to the mover straight away.
    /// </summary>
    void GiveMoverOrder(MCTacticalOrder& order, MCMover* mover, int32_t* partId)
    {
        if (MPlayer != nullptr && MPlayer->IsServer == 0)
        {
            MPlayer->SendPlayerOrder(0, &order, 0, 1, partId, 0, nullptr, 0);
        }
        else
        {
            mover->HandleTacticalOrder(order, 1, 0);
        }
    }
}

auto MechIconHandleEvent(MCGuiObject* icon, MCGuiEvent* event) -> void
{
    auto* mechIcon = static_cast<MCMechIcon*>(icon);
    auto* mover = static_cast<MCMover*>(mechIcon->Mover);
    MCTacticalOrder order;
    order.Reset();

    // Port fix: the original reads the mover's part id before checking the mover is there.
    int32_t partId = mover != nullptr ? mover->PartId : 0;

    if (event->Type == 1)
    {
        Application->Grab(icon);
        return;
    }

    if (event->Type == 6)
    {
        CenterCameraOn(mover);
        return;
    }

    if (event->Type != 4 || Application->GrabbedObject() != icon || mover == nullptr)
    {
        return;
    }

    Application->Release();

    // An attack command (or aimed shot, which only a mech can take) makes the selection attack this mover.
    int32_t command = TheInterface->CurrentCommand;
    bool attackCommand = (command >= 0xb && command <= 0x10) || (command >= 0x17 && command <= 0x1e);

    if (attackCommand && TheInterface->AnySelected(-1) != 0)
    {
        command = TheInterface->CurrentCommand;

        if (mover->ObjectClass != MCObjectClass::BattleMech && command >= 0x17 && command <= 0x1e)
        {
            command = 0xb;
        }

        order.Reset(MCOrderOrigin::Player, MCTacticalOrderCode::AttackObject, 0);
        order.Target = mover;
        order.AttackParams.Type = 1;
        order.AttackParams.Method = 0;
        order.AttackParams.Range = -4;
        order.AttackParams.Pursue = -1;

        switch (command)
        {
            case 0xb:
                order.AttackParams.Range = -1;
                break;
            case 0xc:
                order.AttackParams.Range = 2;
                break;
            case 0xd:
                order.AttackParams.Range = 1;
                break;
            case 0xe:
                order.AttackParams.Range = 0;
                break;
            case 0xf:
                order.AttackParams.Pursue = 0;
                break;
        }

        TheInterface->GetCommandParser()->SendTacOrder(order, -1);
        TheInterface->UpdateInterface();
        return;
    }

    switch (TheInterface->CurrentCommand)
    {
        case 0:
        {
            // Select the mover; shift adds it to (or takes it out of) the selection.
            int32_t iconPartId = mechIcon->PartId;
            int addToExisting;

            if (event->ShiftKey == 0)
            {
                TheInterface->DeselectEnemy();
                TheInterface->ClearMechSelection();
                TheInterface->CommandParser->ClearSubjects();

                if (TheInterface->AnySelected(0) == 0)
                {
                    TacticalMap()->SetID(mechIcon->PartId);
                }

                iconPartId = mechIcon->PartId;
                TheInterface->SelectMech(iconPartId);
                addToExisting = 0;
            }
            else
            {
                if (TheInterface->IsSelected(iconPartId) != 0)
                {
                    TheInterface->DeselectMech(iconPartId);
                    TheInterface->CommandParser->RemoveSubject(iconPartId);
                    TheInterface->UpdateInterface();
                    return;
                }

                TheInterface->SelectMech(iconPartId);
                TheInterface->DeselectEnemy();
                addToExisting = -1;
            }

            TheInterface->CommandParser->AddSubject(iconPartId, addToExisting);
            SoundSystem->PlayDigitalSample(0x10, 1, nullptr, 0, 0);
            break;
        }

        case 1:
        {
            order.Reset(MCOrderOrigin::Player, MCTacticalOrderCode::Eject, 0);
            GiveMoverOrder(order, mover, &partId);
            TheInterface->UpdateInterface();
            return;
        }
        case 2:
        {
            order.Reset(MCOrderOrigin::Player, MCTacticalOrderCode::HoldFire, 0);
            GiveMoverOrder(order, mover, &partId);
            TheInterface->UpdateInterface();
            return;
        }
        case 9:
        {
            order.Reset(MCOrderOrigin::Player, MCTacticalOrderCode::Refit, 0);
            order.Target = mover;
            TheInterface->GetCommandParser()->SendTacOrder(order, 0);
            TheInterface->UpdateInterface();
            return;
        }
        case 0x13:
        {
            order.Reset(MCOrderOrigin::Player, MCTacticalOrderCode::Guard, 0);
            order.Target = mover;
            order.MoveParams.WayPath.Mode[0] = 0;
            order.MoveParams.Wait = -1;
            TheInterface->GetCommandParser()->SendTacOrder(order, -1);
            TheInterface->UpdateInterface();
            return;
        }
        case 0x15:
        {
            order.Reset(MCOrderOrigin::Player, MCTacticalOrderCode::PowerUp, 0);
            GiveMoverOrder(order, mover, &partId);
            TheInterface->UpdateInterface();
            return;
        }
        case 0x16:
        {
            order.Reset(MCOrderOrigin::Player, MCTacticalOrderCode::PowerDown, 0);
            GiveMoverOrder(order, mover, &partId);
            TheInterface->UpdateInterface();
            return;
        }
        case 0x29:
        case 0x2a:
        case 0x2b:
        case 0x2c:
        {
            // Make the selection lance (command - 0x29), with this mover as its point.
            SoundSystem->PlayBettySample(5);
            int32_t groupId = TheInterface->CurrentCommand - 0x29;

            if (TheInterface->IsSelected(mover->PartId) == 0)
            {
                TheInterface->SelectMech(mover->PartId);
            }

            std::vector<MCGameObject*> movers(static_cast<size_t>(TheInterface->NumSelectedMechs));
            // Port fix: the original leaves the point index unset when no selected button holds this mover.
            int32_t pointIndex = 0;

            for (int32_t i = 0; i < TheInterface->NumSelectedMechs; i++)
            {
                MCFriendlyMechIcon* button = TheInterface->MechBar->GetButtonFromID(TheInterface->SelectedMechs[i]);

                if (button == nullptr)
                {
                    continue;
                }

                movers[i] = static_cast<MCGameObject*>(button->Mover);

                if (button->IsPoint != 0)
                {
                    button->IsPoint = 0;
                }

                if (button->Mover == mover)
                {
                    button->IsPoint = -1;
                    pointIndex = i;
                }
            }

            TheInterface->SetUnit(groupId, TheInterface->NumSelectedMechs, movers.data(), pointIndex);
            TheInterface->MechBar->PlaceButtons(-1);

            TheInterface->CurrentCommand = 0;
            TheInterface->CommandOneShot = 0;
            Application->CursorHidden = 0;
            icon->Enter();
            TheInterface->SelectLance(HomeCommander()->GetGroup(groupId));
            TheInterface->CommandParser->AddSubject(HomeCommander()->GetGroup(groupId), -1);
            TheInterface->UpdateInterface();
            return;
        }

        case 0x33:
        {
            TacticalMap()->HideMe(0);
            TacticalMap()->SetDisplayType(MCTacmapPage::Info);
            TacticalMap()->SetID(mechIcon->PartId);
            TheInterface->UpdateInterface();
            return;
        }
        case 0x4a:
            CenterCameraOn(mover);
            break;
    }

    TheInterface->UpdateInterface();
}

auto PaintSelBox(MCGuiObject* box) -> void
{
    int32_t width = box->Width();
    int32_t height = box->Height();
    MCPane* pane = box->Port()->Frame();
    VfxPaneWipe(pane, 0xff);
    int32_t bottom = height - 1;
    int32_t right = width - 1;
    VfxLineDraw(pane, 0, bottom, right, bottom, 0);
    VfxLineDraw(pane, 0, 0, right, 0, 0);
    VfxLineDraw(pane, 0, 0, 0, bottom, 0);
    VfxLineDraw(pane, right, 0, right, bottom, 0);
}

auto InterfaceHandleEvent(MCGuiObject*, MCGuiEvent* event) -> void
{
    TheInterface->HandleEvent(event);
}

auto UpdateMouseStateCallback() -> void
{
    if (Turn > 1)
    {
        MCFrameLog::Scope part("logic.mouseState");
        TheInterface->UpdateMouseState(nullptr);
    }
}

auto HealAll() -> void
{
    MCMechBar* bar = TheInterface->MechBar;

    for (int16_t i = 0; i < static_cast<int16_t>(bar->Layout.NumButtons); i++)
    {
        if (bar->GetButton(i) == nullptr)
        {
            continue;
        }

        auto* mover = static_cast<MCMover*>(bar->GetButton(i)->Mover);

        if (mover == nullptr || mover->IsDisabled() != 0)
        {
            continue;
        }

        for (int32_t j = 0; j < mover->NumArmorLocations(); j++)
        {
            mover->Armor[j].CurArmor = static_cast<float>(mover->Armor[j].MaxArmor);
        }

        for (int32_t j = 0; j < mover->NumBodyLocations(); j++)
        {
            mover->BodyAt(j).CurInternalStructure = static_cast<float>(mover->BodyAt(j).MaxInternalStructure);
        }

        if (mover->ObjectClass == MCObjectClass::BattleMech)
        {
            static_cast<MCBattleMech*>(mover)->CalcLegStatus();
            static_cast<MCBattleMech*>(mover)->CalcTorsoStatus();
        }

        uint32_t firstWeapon = mover->NumOther;

        for (uint32_t j = firstWeapon; j < firstWeapon + mover->NumWeapons; j++)
        {
            mover->Inventory[j].Disabled = 0;
        }

        for (int32_t j = 0; j < mover->NumAmmoTypes(); j++)
        {
            if (mover->AmmoTypeTotal[j].CurAmount != 9999)
            {
                mover->AmmoTypeTotal[j].CurAmount = mover->AmmoTypeTotal[j].StartAmount;
            }
        }
    }
}

auto DeadEye() -> void
{
    MCMechBar* bar = TheInterface->MechBar;

    for (int16_t i = 0; i < static_cast<int16_t>(bar->Layout.NumButtons); i++)
    {
        if (bar->GetButton(i) == nullptr)
        {
            continue;
        }

        auto* mover = static_cast<MCMover*>(bar->GetButton(i)->Mover);

        if (mover != nullptr && mover->IsDisabled() == 0 && mover->GetPilot() != nullptr)
        {
            mover->GetPilot()->Skills[SkillGunnery] = 120;
        }
    }
}

auto Test3() -> void
{
    MCMechBar* bar = TheInterface->MechBar;

    for (int16_t i = 0; i < static_cast<int16_t>(bar->Layout.NumButtons); i++)
    {
        if (bar->GetButton(i) == nullptr)
        {
            continue;
        }

        auto* mover = static_cast<MCMover*>(bar->GetButton(i)->Mover);

        if (mover == nullptr || mover->IsDisabled() != 0)
        {
            continue;
        }

        uint32_t firstWeapon = mover->NumOther;
        uint32_t endWeapon = firstWeapon + mover->NumWeapons;

        if (mover->AmmoTypeTotal.empty())
        {
            // No ammunition: every weapon becomes component 0x9a, which needs none.
            for (uint32_t j = firstWeapon; j < endWeapon; j++)
            {
                MCInventoryItem& item = mover->Inventory[j];
                item.MasterID = 0x9a;
                item.Health = MasterComponentList[0x9a].Health;
                item.Amount = -1;
                item.Disabled = 0;
                item.AmmoIndex = -1;
            }
        }
        else
        {
            // Every weapon becomes component 0x70, fed from the first ammunition type (refilled to 333).
            for (uint32_t j = firstWeapon; j < endWeapon; j++)
            {
                MCInventoryItem& item = mover->Inventory[j];
                item.MasterID = 0x70;
                item.Health = MasterComponentList[0x70].Health;
                item.Amount = static_cast<int16_t>(MasterComponentList[item.MasterID].LongValue);
                item.Disabled = 0;
                item.AmmoIndex = 0;
            }

            MCAmmoTally* ammo = mover->AmmoTypeTotal.data();
            ammo->MasterId = MasterComponentList[0x70].AmmoMasterId;
            ammo->CurAmount = 0x14d;
            ammo->StartAmount = 0x14d;
        }

        mover->CalcWeaponEffectiveness(-1);
        mover->CalcWeaponEffectiveness(0);
        mover->CalcWeaponRangeRatings();
    }
}
