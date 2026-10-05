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
    bool centerCameraOn(BaseObject* mover)
    {
        // Port fix: with no active pane the original used the icon itself as the camera, calling its resize
        // (slot 0x20) with the mover pointer.
        Camera* camera = nullptr;

        if (mainHolder->GetActivePane() != nullptr)
        {
            camera = mainHolder->GetActivePane()->GetCamera();
        }

        if (camera == nullptr)
        {
            return false;
        }

        camera->changeTarget(mover, 0);
        theInterface->floatingTags[0]->ShowGUIWindow(-1);
        return true;
    }

    /// <summary>
    /// A single-mover order: sent to the server in a multiplayer client, otherwise given to the mover straight away.
    /// </summary>
    void giveMoverOrder(TacticalOrder& order, Mover* mover, int32_t* partId)
    {
        if (MPlayer != nullptr && MPlayer->isServer == 0)
        {
            MPlayer->sendPlayerOrder(0, &order, 0, 1, partId, 0, nullptr, 0);
        }
        else
        {
            mover->handleTacticalOrder(order, 1, 0);
        }
    }
}

auto mechIconHandleEvent(aObject* icon, aEvent* event) -> void
{
    auto* mechIcon = static_cast<aMechIcon*>(icon);
    auto* mover = static_cast<Mover*>(mechIcon->mover);
    TacticalOrder order;
    order.init();

    // Port fix: the original reads the mover's part id before checking the mover is there.
    int32_t partId = mover != nullptr ? mover->partId : 0;

    if (event->type == 1)
    {
        application->grab(icon);
        order.destroy();
        return;
    }

    if (event->type == 6)
    {
        centerCameraOn(mover);
        order.destroy();
        return;
    }

    if (event->type != 4 || application->grabbedObject() != icon || mover == nullptr)
    {
        order.destroy();
        return;
    }

    application->release();

    // An attack command (or aimed shot, which only a mech can take) makes the selection attack this mover.
    int32_t command = theInterface->currentCommand;
    bool attackCommand = (command >= 0xb && command <= 0x10) || (command >= 0x17 && command <= 0x1e);

    if (attackCommand && theInterface->AnySelected(-1) != 0)
    {
        command = theInterface->currentCommand;

        if (mover->objectClass != BATTLEMECH && command >= 0x17 && command <= 0x1e)
        {
            command = 0xb;
        }

        order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_ATTACK_OBJECT, 0);
        order.target = mover;
        order.attackParams.type = 1;
        order.attackParams.method = 0;
        order.attackParams.range = -4;
        order.attackParams.pursue = -1;

        switch (command)
        {
            case 0xb:
                order.attackParams.range = -1;
                break;
            case 0xc:
                order.attackParams.range = 2;
                break;
            case 0xd:
                order.attackParams.range = 1;
                break;
            case 0xe:
                order.attackParams.range = 0;
                break;
            case 0xf:
                order.attackParams.pursue = 0;
                break;
        }

        theInterface->GetCommandParser()->SendTacOrder(order, -1);
        theInterface->UpdateInterface();
        order.destroy();
        return;
    }

    switch (theInterface->currentCommand)
    {
        case 0:
        {
            // Select the mover; shift adds it to (or takes it out of) the selection.
            int32_t iconPartId = mechIcon->partId;
            int addToExisting;

            if (event->shiftKey == 0)
            {
                theInterface->DeselectEnemy();
                theInterface->ClearMechSelection();
                theInterface->commandParser->ClearSubjects();

                if (theInterface->AnySelected(0) == 0)
                {
                    Terrain::terrainTacticalMap->SetID(mechIcon->partId);
                }

                iconPartId = mechIcon->partId;
                theInterface->SelectMech(iconPartId);
                addToExisting = 0;
            }
            else
            {
                if (theInterface->IsSelected(iconPartId) != 0)
                {
                    theInterface->DeselectMech(iconPartId);
                    theInterface->commandParser->RemoveSubject(iconPartId);
                    theInterface->UpdateInterface();
                    order.destroy();
                    return;
                }

                theInterface->SelectMech(iconPartId);
                theInterface->DeselectEnemy();
                addToExisting = -1;
            }

            theInterface->commandParser->AddSubject(iconPartId, addToExisting);
            soundSystem->playDigitalSample(0x10, 1, nullptr, 0, 0);
            break;
        }

        case 1:
        {
            order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_EJECT, 0);
            giveMoverOrder(order, mover, &partId);
            theInterface->UpdateInterface();
            order.destroy();
            return;
        }
        case 2:
        {
            order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_HOLD_FIRE, 0);
            giveMoverOrder(order, mover, &partId);
            theInterface->UpdateInterface();
            order.destroy();
            return;
        }
        case 9:
        {
            order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_REFIT, 0);
            order.target = mover;
            theInterface->GetCommandParser()->SendTacOrder(order, 0);
            theInterface->UpdateInterface();
            order.destroy();
            return;
        }
        case 0x13:
        {
            order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_GUARD, 0);
            order.target = mover;
            order.moveParams.wayPath.mode[0] = 0;
            order.moveParams.wait = -1;
            theInterface->GetCommandParser()->SendTacOrder(order, -1);
            theInterface->UpdateInterface();
            order.destroy();
            return;
        }
        case 0x15:
        {
            order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_POWERUP, 0);
            giveMoverOrder(order, mover, &partId);
            theInterface->UpdateInterface();
            order.destroy();
            return;
        }
        case 0x16:
        {
            order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_POWERDOWN, 0);
            giveMoverOrder(order, mover, &partId);
            theInterface->UpdateInterface();
            order.destroy();
            return;
        }
        case 0x29:
        case 0x2a:
        case 0x2b:
        case 0x2c:
        {
            // Make the selection lance (command - 0x29), with this mover as its point.
            soundSystem->playBettySample(5);
            int32_t groupId = theInterface->currentCommand - 0x29;

            if (theInterface->IsSelected(mover->partId) == 0)
            {
                theInterface->SelectMech(mover->partId);
            }

            std::vector<GameObject*> movers(static_cast<size_t>(theInterface->numSelectedMechs));
            // Port fix: the original leaves the point index unset when no selected button holds this mover.
            int32_t pointIndex = 0;

            for (int32_t i = 0; i < theInterface->numSelectedMechs; i++)
            {
                FriendlyMechIcon* button = theInterface->mechBar->GetButtonFromID(theInterface->selectedMechs[i]);

                if (button == nullptr)
                {
                    continue;
                }

                movers[i] = static_cast<GameObject*>(button->mover);

                if (button->isPoint != 0)
                {
                    button->isPoint = 0;
                }

                if (button->mover == mover)
                {
                    button->isPoint = -1;
                    pointIndex = i;
                }
            }

            theInterface->setUnit(groupId, theInterface->numSelectedMechs, movers.data(), pointIndex);
            theInterface->mechBar->PlaceButtons(-1);

            theInterface->currentCommand = 0;
            theInterface->commandOneShot = 0;
            application->cursorHidden = 0;
            icon->enter();
            theInterface->SelectLance(HomeCommander->getGroup(groupId));
            theInterface->commandParser->AddSubject(HomeCommander->getGroup(groupId), -1);
            theInterface->UpdateInterface();
            order.destroy();
            return;
        }

        case 0x33:
        {
            Terrain::terrainTacticalMap->HideMe(0);
            Terrain::terrainTacticalMap->SetDisplayType(TACMAP_INFO);
            Terrain::terrainTacticalMap->SetID(mechIcon->partId);
            theInterface->UpdateInterface();
            order.destroy();
            return;
        }
        case 0x4a:
            centerCameraOn(mover);
            break;
    }

    theInterface->UpdateInterface();
    order.destroy();
}

auto PaintSelBox(aObject* box) -> void
{
    int32_t width = box->width();
    int32_t height = box->height();
    _pane* pane = box->port()->frame();
    VFX_pane_wipe(pane, 0xff);
    int32_t bottom = height - 1;
    int32_t right = width - 1;
    VFX_line_draw(pane, 0, bottom, right, bottom, LD_DRAW, 0);
    VFX_line_draw(pane, 0, 0, right, 0, LD_DRAW, 0);
    VFX_line_draw(pane, 0, 0, 0, bottom, LD_DRAW, 0);
    VFX_line_draw(pane, right, 0, right, bottom, LD_DRAW, 0);
}

auto interfaceHandleEvent(aObject*, aEvent* event) -> void
{
    theInterface->handleEvent(event);
}

auto UpdateMouseStateCallback() -> void
{
    if (turn > 1)
    {
        MCFrameLog::Scope part("logic.mouseState");
        theInterface->UpdateMouseState(nullptr);
    }
}

auto HealAll() -> void
{
    aMechBar* bar = theInterface->mechBar;

    for (int16_t i = 0; i < static_cast<int16_t>(bar->layout.numButtons); i++)
    {
        if (bar->getButton(i) == nullptr)
        {
            continue;
        }

        auto* mover = static_cast<Mover*>(bar->getButton(i)->mover);

        if (mover == nullptr || mover->isDisabled() != 0)
        {
            continue;
        }

        for (int32_t j = 0; j < mover->numArmorLocations; j++)
        {
            mover->armor[j].curArmor = static_cast<float>(mover->armor[j].maxArmor);
        }

        for (int32_t j = 0; j < mover->numBodyLocations; j++)
        {
            mover->bodyAt(j).curInternalStructure = static_cast<float>(mover->bodyAt(j).maxInternalStructure);
        }

        if (mover->objectClass == BATTLEMECH)
        {
            static_cast<BattleMech*>(mover)->calcLegStatus();
            static_cast<BattleMech*>(mover)->calcTorsoStatus();
        }

        uint32_t firstWeapon = mover->numOther;

        for (uint32_t j = firstWeapon; j < firstWeapon + mover->numWeapons; j++)
        {
            mover->inventory[j].disabled = 0;
        }

        for (int32_t j = 0; j < mover->numAmmoTypes; j++)
        {
            if (mover->ammoTypeTotal[j].curAmount != 9999)
            {
                mover->ammoTypeTotal[j].curAmount = mover->ammoTypeTotal[j].startAmount;
            }
        }
    }
}

auto DeadEye() -> void
{
    aMechBar* bar = theInterface->mechBar;

    for (int16_t i = 0; i < static_cast<int16_t>(bar->layout.numButtons); i++)
    {
        if (bar->getButton(i) == nullptr)
        {
            continue;
        }

        auto* mover = static_cast<Mover*>(bar->getButton(i)->mover);

        if (mover != nullptr && mover->isDisabled() == 0 && mover->getPilot() != nullptr)
        {
            mover->getPilot()->skills[MWS_GUNNERY] = 120;
        }
    }
}

auto Test3() -> void
{
    aMechBar* bar = theInterface->mechBar;

    for (int16_t i = 0; i < static_cast<int16_t>(bar->layout.numButtons); i++)
    {
        if (bar->getButton(i) == nullptr)
        {
            continue;
        }

        auto* mover = static_cast<Mover*>(bar->getButton(i)->mover);

        if (mover == nullptr || mover->isDisabled() != 0)
        {
            continue;
        }

        uint32_t firstWeapon = mover->numOther;
        uint32_t endWeapon = firstWeapon + mover->numWeapons;

        if (mover->ammoTypeTotal == nullptr)
        {
            // No ammunition: every weapon becomes component 0x9a, which needs none.
            for (uint32_t j = firstWeapon; j < endWeapon; j++)
            {
                InventoryItem& item = mover->inventory[j];
                item.masterID = 0x9a;
                item.health = MasterComponentList[0x9a].health;
                item.amount = -1;
                item.disabled = 0;
                item.ammoIndex = -1;
            }
        }
        else
        {
            // Every weapon becomes component 0x70, fed from the first ammunition type (refilled to 333).
            for (uint32_t j = firstWeapon; j < endWeapon; j++)
            {
                InventoryItem& item = mover->inventory[j];
                item.masterID = 0x70;
                item.health = MasterComponentList[0x70].health;
                item.amount = static_cast<int16_t>(MasterComponentList[item.masterID].longValue);
                item.disabled = 0;
                item.ammoIndex = 0;
            }

            AmmoTally* ammo = mover->ammoTypeTotal.get();
            ammo->masterId = MasterComponentList[0x70].ammoMasterId;
            ammo->curAmount = 0x14d;
            ammo->startAmount = 0x14d;
        }

        mover->calcWeaponEffectiveness(-1);
        mover->calcWeaponEffectiveness(0);
        mover->calcWeaponRangeRatings();
    }
}
