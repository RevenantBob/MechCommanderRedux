#include "stdafx.h"
#include "iface/icallbk.h"
#include "ai/tacordr.h"
#include "camera/camera.h"
#include "gui/ahelp.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "iface/iface.h"
#include "iface/parser.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "object/cmponent.h"
#include "object/comndr.h"
#include "object/gameobj.h"
#include "object/mech.h"
#include "object/mover.h"
#include "object/warrior.h"
#include "platform/MCFrameLog.h"
#include "sound/soundsys.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>Points the main window's active camera at the mover and shows the floating tags.</summary>
    /// <returns>False when there is no camera.</returns>
    bool CenterCameraOn(MCBaseObject* mover)
    {
        // Port fix: with no active pane the original used the icon itself as the camera, calling its resize
        // (slot 0x20) with the mover pointer.
        MCCamera* camera = nullptr;

        if (MainHolder->GetActivePane() != nullptr)
        {
            camera = MainHolder->GetActivePane()->GetCamera();
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
    order.Init();

    // Port fix: the original reads the mover's part id before checking the mover is there.
    int32_t partId = mover != nullptr ? mover->PartId : 0;

    if (event->Type == 1)
    {
        Application->Grab(icon);
        order.Destroy();
        return;
    }

    if (event->Type == 6)
    {
        CenterCameraOn(mover);
        order.Destroy();
        return;
    }

    if (event->Type != 4 || Application->GrabbedObject() != icon || mover == nullptr)
    {
        order.Destroy();
        return;
    }

    Application->Release();

    // An attack command (or aimed shot, which only a mech can take) makes the selection attack this mover.
    int32_t command = TheInterface->CurrentCommand;
    bool attackCommand = (command >= 0xb && command <= 0x10) || (command >= 0x17 && command <= 0x1e);

    if (attackCommand && TheInterface->AnySelected(-1) != 0)
    {
        command = TheInterface->CurrentCommand;

        if (mover->ObjectClass != BATTLEMECH && command >= 0x17 && command <= 0x1e)
        {
            command = 0xb;
        }

        order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_ATTACK_OBJECT, 0);
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
        order.Destroy();
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
                    MCTerrain::TerrainTacticalMap->SetID(mechIcon->PartId);
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
                    order.Destroy();
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
            order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_EJECT, 0);
            GiveMoverOrder(order, mover, &partId);
            TheInterface->UpdateInterface();
            order.Destroy();
            return;
        }
        case 2:
        {
            order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_HOLD_FIRE, 0);
            GiveMoverOrder(order, mover, &partId);
            TheInterface->UpdateInterface();
            order.Destroy();
            return;
        }
        case 9:
        {
            order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_REFIT, 0);
            order.Target = mover;
            TheInterface->GetCommandParser()->SendTacOrder(order, 0);
            TheInterface->UpdateInterface();
            order.Destroy();
            return;
        }
        case 0x13:
        {
            order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_GUARD, 0);
            order.Target = mover;
            order.MoveParams.WayPath.Mode[0] = 0;
            order.MoveParams.Wait = -1;
            TheInterface->GetCommandParser()->SendTacOrder(order, -1);
            TheInterface->UpdateInterface();
            order.Destroy();
            return;
        }
        case 0x15:
        {
            order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_POWERUP, 0);
            GiveMoverOrder(order, mover, &partId);
            TheInterface->UpdateInterface();
            order.Destroy();
            return;
        }
        case 0x16:
        {
            order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_POWERDOWN, 0);
            GiveMoverOrder(order, mover, &partId);
            TheInterface->UpdateInterface();
            order.Destroy();
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
            TheInterface->SelectLance(HomeCommander->GetGroup(groupId));
            TheInterface->CommandParser->AddSubject(HomeCommander->GetGroup(groupId), -1);
            TheInterface->UpdateInterface();
            order.Destroy();
            return;
        }

        case 0x33:
        {
            MCTerrain::TerrainTacticalMap->HideMe(0);
            MCTerrain::TerrainTacticalMap->SetDisplayType(TACMAP_INFO);
            MCTerrain::TerrainTacticalMap->SetID(mechIcon->PartId);
            TheInterface->UpdateInterface();
            order.Destroy();
            return;
        }
        case 0x4a:
            CenterCameraOn(mover);
            break;
    }

    TheInterface->UpdateInterface();
    order.Destroy();
}

auto PaintSelBox(MCGuiObject* box) -> void
{
    int32_t width = box->Width();
    int32_t height = box->Height();
    MCPane* pane = box->Port()->Frame();
    VfxPaneWipe(pane, 0xff);
    int32_t bottom = height - 1;
    int32_t right = width - 1;
    VfxLineDraw(pane, 0, bottom, right, bottom, LD_DRAW, 0);
    VfxLineDraw(pane, 0, 0, right, 0, LD_DRAW, 0);
    VfxLineDraw(pane, 0, 0, 0, bottom, LD_DRAW, 0);
    VfxLineDraw(pane, right, 0, right, bottom, LD_DRAW, 0);
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

        for (int32_t j = 0; j < mover->NumArmorLocations; j++)
        {
            mover->Armor[j].CurArmor = static_cast<float>(mover->Armor[j].MaxArmor);
        }

        for (int32_t j = 0; j < mover->NumBodyLocations; j++)
        {
            mover->BodyAt(j).CurInternalStructure = static_cast<float>(mover->BodyAt(j).MaxInternalStructure);
        }

        if (mover->ObjectClass == BATTLEMECH)
        {
            static_cast<MCBattleMech*>(mover)->CalcLegStatus();
            static_cast<MCBattleMech*>(mover)->CalcTorsoStatus();
        }

        uint32_t firstWeapon = mover->NumOther;

        for (uint32_t j = firstWeapon; j < firstWeapon + mover->NumWeapons; j++)
        {
            mover->Inventory[j].Disabled = 0;
        }

        for (int32_t j = 0; j < mover->NumAmmoTypes; j++)
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
            mover->GetPilot()->Skills[MWS_GUNNERY] = 120;
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

        if (mover->AmmoTypeTotal == nullptr)
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

            MCAmmoTally* ammo = mover->AmmoTypeTotal.get();
            ammo->MasterId = MasterComponentList[0x70].AmmoMasterId;
            ammo->CurAmount = 0x14d;
            ammo->StartAmount = 0x14d;
        }

        mover->CalcWeaponEffectiveness(-1);
        mover->CalcWeaponEffectiveness(0);
        mover->CalcWeaponRangeRatings();
    }
}
