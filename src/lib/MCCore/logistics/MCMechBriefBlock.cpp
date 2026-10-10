#include "stdafx.h"
#include "logistics/MCMechBriefBlock.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiSystem.h"
#include "gui/MCScrollPane.h"
#include "logistics/MCBriefingBox.h"
#include "logistics/MCBriefingScreen.h"
#include "logistics/MCDragIcon.h"
#include "logistics/MCLogRows.h"
#include "logistics/MCTicker.h"
#include "main/MCGamePaths.h"
#include "main/MCLogistics.h"
#include "main/MCGameStrings.h"
#include "network/MCMultiPlayer.h"
#include "object/MCMasterComponent.h"
#include "platform/MCInput.h"
#include "sound/MCSoundSystem.h"
#include "vfx/MCVfxFunctions.h"
#include "lib/MCFatal.h"

namespace
{
    /// <summary>The drag of a unit block (one at a time).</summary>
    struct MCBriefDrag
    {
        /// <summary>Set while a block is dragged with the left button.</summary>
        bool Left = false;
        /// <summary>Set while a block is dragged with the right button.</summary>
        bool Right = false;
        /// <summary>The drag icon's position.</summary>
        int32_t X = 0;
        int32_t Y = 0;
        /// <summary>Set when the block was picked up from a drop slot, not the deploy pane.</summary>
        bool FromSlot = false;
    };

    /// <summary>The drag going on, if any.</summary>
    MCBriefDrag Drag;

    /// <summary>A block's size (a drop slot's).</summary>
    constexpr int32_t BlockWidth = 0x34;
    constexpr int32_t BlockHeight = 0x2e;

    /// <summary>The sounds of the drop slots: a unit placed (or back), refused, picked up.</summary>
    constexpr uint32_t PlacedSound = 0x34;
    constexpr uint32_t RefusedSound = 0x33;
    constexpr uint32_t PickedUpSound = 0x35;

    /// <summary>Puts string <paramref name="id"/> on the ticker.</summary>
    void ShowHelp(uint32_t id)
    {
        GlobalLogPtr->Ticker->SetString(LoadGameString(id, 0xfe));
    }

    /// <summary>The unit's tonnage added to (or, with <paramref name="sign"/> -1, taken from) <c>curDeployTonnage</c>.</summary>
    int32_t DeployTonnageWith(const MCLogPart* part, float sign)
    {
        return static_cast<int32_t>(static_cast<float>(CurDeployTonnage) + sign * part->CurTonnage);
    }

    /// <summary>Fills a <paramref name="width"/> by <paramref name="height"/> box of <paramref name="target"/> with <paramref name="color"/>.</summary>
    void FillArea(MCPane* target, int32_t xPos, int32_t yPos, int32_t width, int32_t height, int32_t color)
    {
        MCLogBlockPort box(target, xPos, yPos, width, height, false);
        VfxPaneWipe(box.Frame(), color);
    }

    /// <summary>Whether drop slot <paramref name="slot"/> of <paramref name="lance"/> is one the player can fill.</summary>
    bool LocalSlot(int32_t lance, int32_t slot)
    {
        return GlobalLogPtr->LocalDropSlot[lance * 4 + slot] != 0;
    }

    /// <summary>Drop slot <paramref name="slot"/> of <paramref name="lance"/>'s place on the screen.</summary>
    const RECT& SlotRect(int32_t lance, int32_t slot)
    {
        return GlobalLogPtr->BriefingScreen->SlotRects[static_cast<size_t>(lance * 4 + slot)];
    }

    /// <summary>The unit is too heavy for the slot: says so.</summary>
    void TooHeavy()
    {
        ShowLogMessage(0x4cu);
        SoundSystem()->PlayBettySample(0);
        GlobalLogPtr->BriefingScreen->DrawTonnageBar();
    }

    /// <summary>
    /// Why <paramref name="mech"/> can't drop: string 0x35d for a damaged engine, 0x364 for a destroyed location; 0
    /// when it can.
    /// </summary>
    uint32_t DropRefusal(MCLogMech* mech)
    {
        const auto& items = mech->Inventory->Items;
        const auto engine =
            std::ranges::find_if(items, [](const std::unique_ptr<MCLogInventoryItem>& item)
                                 { return MasterComponentList[item->MasterID].Form == MCComponentForm::Engine; });

        // Port fix: a mech without an engine item (the original dereferenced null) counts as undamaged.
        if (engine != items.end() && !(*engine)->Stats.empty() && (*engine)->Stats.front()->Hits != 0)
        {
            return 0x35d;
        }

        for (const auto& location : mech->Internals)
        {
            if (location.CurArmor == 0)
            {
                return 0x364;
            }
        }

        return 0;
    }
}

auto MCMechBriefBlock::Create(MCLogMech* mech, MCLogObject* parent, int32_t xPos, int32_t yPos) -> MCMechBriefBlock*
{
    mech->BriefBlock = std::make_unique<MCMechBriefBlock>();
    MCMechBriefBlock* block = mech->BriefBlock.get();
    block->Mech = mech;
    block->Attach(parent, xPos, yPos);
    return block;
}

auto MCMechBriefBlock::Create(MCLogVehicle* vehicle, MCLogObject* parent, int32_t xPos, int32_t yPos)
    -> MCMechBriefBlock*
{
    vehicle->BriefBlock = std::make_unique<MCMechBriefBlock>();
    MCMechBriefBlock* block = vehicle->BriefBlock.get();
    block->Vehicle = vehicle;
    block->Attach(parent, xPos, yPos);
    return block;
}

auto MCMechBriefBlock::Discard(std::unique_ptr<MCMechBriefBlock>& block) -> void
{
    block.reset();
}

auto MCMechBriefBlock::Attach(MCLogObject* parent, int32_t xPos, int32_t yPos) -> void
{
    MCLogObject::InitWithoutPort(xPos, yPos, BlockWidth, BlockHeight);
    parent->AddChild(this);
    DrawBackground();
}

auto MCMechBriefBlock::Destroy() -> void
{
    Mech = nullptr;
    Vehicle = nullptr;
    MCLogObject::Destroy();
}

auto MCMechBriefBlock::HandleEvent(MCGuiEvent* event) -> void
{
    if (!Drag.Left && !Drag.Right)
    {
        if (event->X < GlobalX() || event->X > GlobalX() + 1 + Width())
        {
            return;
        }

        if (event->Y < GlobalY() || event->Y > GlobalY() + 1 + Height())
        {
            return;
        }
    }

    // The parent is always a logistics object (the briefing screen or the deploy pane).
    auto* owner = static_cast<MCLogObject*>(Parent);

    if (owner != nullptr && !Drag.Left && (event->Type == 8 || event->Type == 9))
    {
        owner->HandleEvent(event);
        return;
    }

    switch (event->Type)
    {
        case 1:
        {
            if (!Drag.Right)
            {
                PickUp(event);
            }

            return;
        }
        case 3:
        {
            if (!Drag.Left)
            {
                Drag.Right = true;
                PickUp(event);
            }

            return;
        }
        case 7:
        {
            if (Drag.Left)
            {
                Drag.Y = event->Y - 0x17;
                Drag.X = event->X - 0x1a;
                MCDragIcon::Current()->MoveTo(Drag.X, Drag.Y, false);
                return;
            }

            if (GuiSystem()->GrabbedObject() == nullptr && event->Key == 0)
            {
                ShowHelp(GlobalX() < 0xd1 ? 0x22 : 0x21);
            }

            return;
        }
        case 4:
        case 6:
        {
            // A release ends the drag of its own button.
            if ((event->Type == 6 && Drag.Left) || (event->Type == 4 && Drag.Right))
            {
                return;
            }

            if (GuiSystem()->GrabbedObject() == nullptr)
            {
                Drag.Right = false;
                return;
            }

            Drop(event);
            return;
        }

        default:
            return;
    }
}

auto MCMechBriefBlock::PlaceInEmptySlot(int32_t lance, int32_t slot) -> bool
{
    auto& deploy = GlobalLogPtr->DeploySlots[lance][slot];

    if (Mech != nullptr)
    {
        if (!MCBriefingScreen::FitsTonnage(Mech) && GlobalLogPtr->HammerDown == 0)
        {
            return false;
        }

        GlobalLogPtr->SendAddMechMessage(Mech, lance, slot);
        SoundSystem()->PlayDigitalSample(PlacedSound, 1, nullptr, 0, 0);
        deploy.Unit = GlobalLogPtr->ForceMechList->GetMechIndex(Mech);
        CurDeployTonnage = DeployTonnageWith(Mech, 1.0f);
        Mech->Deployed = true;
        GlobalLogPtr->AssignedWarriorList->SetDeployed(Mech->PilotIndex, true);
        return true;
    }

    if (!MCBriefingScreen::FitsTonnage(Vehicle) && GlobalLogPtr->HammerDown == 0)
    {
        return false;
    }

    GlobalLogPtr->SendAddVehicleMessage(Vehicle, lance, slot);
    SoundSystem()->PlayDigitalSample(PlacedSound, 1, nullptr, 0, 0);
    deploy.Vehicle = GlobalLogPtr->ForceVehicleList->GetVehicleIndex(Vehicle);
    CurDeployTonnage = DeployTonnageWith(Vehicle, 1.0f);
    Vehicle->Deployed = true;
    return true;
}

auto MCMechBriefBlock::Settle(int32_t lance, int32_t slot) -> void
{
    MCBriefingScreen* screen = GlobalLogPtr->BriefingScreen.get();

    if (Parent != nullptr)
    {
        Parent->RemoveChild(this);
    }

    screen->AddChild(this);
    const RECT& area = SlotRect(lance, slot);
    MoveTo(area.left, area.top, 0);
    DrawBackground();
    screen->CalcTonnages();
}

auto MCMechBriefBlock::DropInFreeSlot() -> bool
{
    // Lance 1, 2 or 3 with its key held; else any.
    int32_t lance = -1;

    if ((MCInput::GetAsyncKeyState('1') & 0x8000) != 0)
    {
        lance = 0;
    }
    else if ((MCInput::GetAsyncKeyState('2') & 0x8000) != 0)
    {
        lance = 1;
    }
    else if ((MCInput::GetAsyncKeyState('3') & 0x8000) != 0)
    {
        lance = 2;
    }

    const int32_t from = lance < 0 ? 0 : lance;
    const int32_t to = lance < 0 ? 3 : lance + 1;

    for (int32_t l = from; l < to; l++)
    {
        for (int32_t s = 0; s < 4; s++)
        {
            if (!LocalSlot(l, s))
            {
                // Any lance: the slots the player can fill may have gaps; the lance asked for ends at its first.
                if (lance < 0)
                {
                    continue;
                }
                break;
            }

            const auto& deploy = GlobalLogPtr->DeploySlots[l][s];

            if (deploy.Unit != -1 || deploy.Vehicle != -1)
            {
                continue;
            }

            if (!PlaceInEmptySlot(l, s))
            {
                TooHeavy();
                return false;
            }

            Settle(l, s);
            return true;
        }
    }

    // Port fix (OB-083): with no key held and no free slot the original went on to search a fourth lance past the end
    // of the slot tables (and, after a refusal, the later lances' slots against an uninitialised point).
    return false;
}

auto MCMechBriefBlock::DropInSlotAt(POINT point, int32_t firstLance) -> bool
{
    for (int32_t lance = firstLance; lance < 3; lance++)
    {
        for (int32_t slot = 0; slot < 4; slot++)
        {
            if (!LocalSlot(lance, slot) || PtInRect(&SlotRect(lance, slot), point) == 0)
            {
                continue;
            }

            auto& deploy = GlobalLogPtr->DeploySlots[lance][slot];
            bool fits;

            if (deploy.Unit > -1)
            {
                // A mech was there: it leaves the force's drop. (Original behaviour (OB-166): its pilot stays marked deployed,
                // and a mech put in its place isn't marked.)
                MCLogMech* occupant = nullptr;
                GlobalLogPtr->ForceMechList->GetMechInfo(deploy.Unit, occupant);
                CurDeployTonnage = DeployTonnageWith(occupant, -1.0f);
                occupant->Deployed = 0;
                Discard(occupant->BriefBlock);
                deploy.Unit = -1;

                if (Mech != nullptr)
                {
                    fits = MCBriefingScreen::FitsTonnage(Mech);

                    if (fits)
                    {
                        GlobalLogPtr->SendAddMechMessage(Mech, lance, slot);
                        SoundSystem()->PlayDigitalSample(PlacedSound, 1, nullptr, 0, 0);
                        deploy.Unit = GlobalLogPtr->ForceMechList->GetMechIndex(Mech);
                        CurDeployTonnage = DeployTonnageWith(Mech, 1.0f);
                        Mech->Deployed = true;
                    }
                }
                else
                {
                    fits = MCBriefingScreen::FitsTonnage(Vehicle);

                    if (fits)
                    {
                        GlobalLogPtr->SendAddVehicleMessage(Vehicle, lance, slot);
                        SoundSystem()->PlayDigitalSample(PlacedSound, 1, nullptr, 0, 0);
                        deploy.Unit = -1;
                        deploy.Vehicle = GlobalLogPtr->ForceVehicleList->GetVehicleIndex(Vehicle);
                        CurDeployTonnage = DeployTonnageWith(Vehicle, 1.0f);
                        Vehicle->Deployed = true;
                    }
                }
            }
            else if (deploy.Vehicle > -1)
            {
                // A vehicle was there.
                MCLogVehicle* occupant = nullptr;
                GlobalLogPtr->ForceVehicleList->GetVehicleInfo(deploy.Vehicle, occupant);
                CurDeployTonnage = DeployTonnageWith(occupant, -1.0f);
                occupant->Deployed = 0;
                Discard(occupant->BriefBlock);
                deploy.Vehicle = -1;

                if (Mech != nullptr)
                {
                    fits = MCBriefingScreen::FitsTonnage(Mech);

                    if (fits)
                    {
                        GlobalLogPtr->SendAddMechMessage(Mech, lance, slot);
                        SoundSystem()->PlayDigitalSample(PlacedSound, 1, nullptr, 0, 0);
                        deploy.Vehicle = -1;
                        deploy.Unit = GlobalLogPtr->ForceMechList->GetMechIndex(Mech);
                        CurDeployTonnage = DeployTonnageWith(Mech, 1.0f);
                        Mech->Deployed = true;
                        GlobalLogPtr->AssignedWarriorList->SetDeployed(Mech->PilotIndex, true);
                    }
                }
                else
                {
                    fits = MCBriefingScreen::FitsTonnage(Vehicle);

                    if (fits)
                    {
                        GlobalLogPtr->SendAddVehicleMessage(Vehicle, lance, slot);
                        SoundSystem()->PlayDigitalSample(PlacedSound, 1, nullptr, 0, 0);
                        deploy.Vehicle = GlobalLogPtr->ForceVehicleList->GetVehicleIndex(Vehicle);
                        CurDeployTonnage = DeployTonnageWith(Vehicle, 1.0f);
                        Vehicle->Deployed = true;
                    }
                }
            }
            else
            {
                fits = PlaceInEmptySlot(lance, slot);
            }

            // (The slots don't overlap: no other slot is under the point.)
            if (!fits)
            {
                TooHeavy();
                return false;
            }

            Settle(lance, slot);
            return true;
        }
    }

    return false;
}

auto MCMechBriefBlock::Drop(MCGuiEvent* event) -> void
{
    MCBriefingScreen* screen = GlobalLogPtr->BriefingScreen.get();
    const bool rightButton = event->Type == 6;
    GuiSystem()->Release();
    Drag.Right = false;
    Drag.Left = false;
    MCDragIcon::Remove();

    // A right-button drop from the deploy pane goes to a free slot; a left-button drop to the slot under the mouse,
    // whatever was there going back to the pane.
    bool placed = false;

    if (rightButton)
    {
        placed = !Drag.FromSlot && DropInFreeSlot();
    }
    else
    {
        placed = DropInSlotAt(POINT{event->X, event->Y}, 0);
    }

    if (placed)
    {
        screen->SetUpDeploy();
        return;
    }

    // Nowhere to go: back to the deploy pane (a refusal sound when dropped off the pane).
    uint32_t sound = PlacedSound;

    if (!Drag.FromSlot)
    {
        MCScrollPane* pane = screen->DeployPane.get();
        const bool overPane = event->X > pane->GlobalX() && event->X < pane->GlobalX() + pane->Width() &&
                              event->Y > pane->GlobalY() && event->Y < pane->GlobalY() + pane->Height();

        if (!overPane)
        {
            sound = RefusedSound;
        }
    }

    SoundSystem()->PlayDigitalSample(sound, 1, nullptr, 0, 0);

    if (Mech != nullptr)
    {
        Mech->Deployed = 0;
        GlobalLogPtr->AssignedWarriorList->SetDeployed(Mech->PilotIndex, 0);
    }
    else
    {
        Vehicle->Deployed = 0;
    }

    if (Parent != nullptr)
    {
        Parent->RemoveChild(this);
    }

    screen->DeployPane->AddChild(this);
    screen->SetUpDeploy();
}

auto MCMechBriefBlock::PickUp(MCGuiEvent* event) -> void
{
    MCBriefingScreen* screen = GlobalLogPtr->BriefingScreen.get();
    auto* owner = static_cast<MCLogObject*>(Parent);

    // Its briefing shows.
    MCBriefingBox* shown = screen->BriefingBox;

    if (shown != nullptr && shown->Parent != nullptr)
    {
        shown->Parent->RemoveChild(shown);
    }

    MCLogPart* part = Mech != nullptr ? static_cast<MCLogPart*>(Mech) : Vehicle;
    MCBriefingBox* box = part->BriefingBox.get();
    screen->AddChild(box);
    screen->BriefingBox = box;
    box->DrawBackground();

    if (part->LocalPart == 0 || screen->ButtonsLocked)
    {
        return;
    }

    const bool fromSlot = owner == screen;
    Drag.FromSlot = fromSlot;
    SoundSystem()->PlayDigitalSample(PickedUpSound, 1, nullptr, 0, 0);
    GuiSystem()->Grab(this);

    if (event->Type == 1)
    {
        Drag.Left = true;
    }

    Drag.X = GlobalX();
    Drag.Y = GlobalY();
    MCDragIcon* icon = MCDragIcon::Create();
    icon->Begin(Drag.X, Drag.Y, BlockWidth, BlockHeight, [this](MCLogPort* surface) { OnBeginDrag(surface); });

    if (fromSlot)
    {
        // Out of its slot (which shows empty again): the unit leaves the drop.
        screen->LiftFromSlot(this);
        bool found = false;

        for (int32_t lance = 0; lance < 3 && !found; lance++)
        {
            for (int32_t slot = 0; slot < 4; slot++)
            {
                if (!LocalSlot(lance, slot) || PtInRect(&SlotRect(lance, slot), POINT{event->X, event->Y}) == 0)
                {
                    continue;
                }

                auto& deploy = GlobalLogPtr->DeploySlots[lance][slot];

                if (Mech == nullptr)
                {
                    Vehicle->Deployed = 0;
                    deploy.Vehicle = -1;
                    CurDeployTonnage = DeployTonnageWith(Vehicle, -1.0f);
                }
                else
                {
                    Mech->Deployed = 0;
                    deploy.Unit = -1;
                    CurDeployTonnage = DeployTonnageWith(Mech, -1.0f);
                    GlobalLogPtr->AssignedWarriorList->SetDeployed(Mech->PilotIndex, 0);
                }

                screen->CalcTonnages();
                GlobalLogPtr->SendRemoveForceMessage(lance, slot);
                found = true;
                break;
            }
        }

        Assert(found, 0, "Could not find the slot this item occupied");
    }
    else
    {
        // Out of the deploy pane (setUpDeploy rebuilds it without the block). A mech with a damaged engine or a
        // destroyed location can't drop.
        if (Mech == nullptr)
        {
            Vehicle->Deployed = true;
        }
        else
        {
            if (const uint32_t refusal = DropRefusal(Mech); refusal != 0)
            {
                const std::string text = LoadGameString(refusal, 0xfe);
                GuiSystem()->Release();
                Drag.Left = false;
                Drag.Right = false;
                screen->SetUpDeploy();
                ShowLogMessage(text);
                return;
            }

            Mech->Deployed = true;
            GlobalLogPtr->AssignedWarriorList->SetDeployed(Mech->PilotIndex, true);
        }

        screen->SetUpDeploy();
    }

    screen->AddChild(MCDragIcon::Current());
    MCDragIcon::Current()->Raise();
}

auto MCMechBriefBlock::DrawBackground() -> void
{
    // Port: the block is drawn each frame by its parent (PaintBlock): the screen into its slot, or the deploy pane.
    if (Parent != nullptr && Parent == GlobalLogPtr->BriefingScreen.get())
    {
        GlobalLogPtr->BriefingScreen->PlaceInSlot(this);
    }
}

auto MCMechBriefBlock::OnBeginDrag(MCLogPort* surface) -> void
{
    MCPane* target = surface->Frame();
    MCBriefingScreen* screen = GlobalLogPtr->BriefingScreen.get();

    if (Parent != nullptr && Parent == screen)
    {
        // In a drop slot: framed, over the empty slot.
        FillArea(target, 0, 0, BlockWidth, BlockHeight, 0x10);

        if (screen->EmptySlot != nullptr)
        {
            screen->EmptySlot->CopyTo(target, 0, 0, true);
        }

        PaintBlock(target, 0, 0, true);
        return;
    }

    // The deploy pane is colour 0x10 around its blocks.
    VfxPaneWipe(target, 0x10);
    PaintBlock(target, 0, 0, false);
}

auto MCMechBriefBlock::PaintBlock(MCPane* target, int32_t xPos, int32_t yPos, bool framed) -> void
{
    // Drawn in place: the original put the block together in a picture of its art and copied it keyed on 0xff.
    MCLogPort* art = LogScreenArt(Mech == nullptr ? "lscupv00.tga" : "lscupm00.tga");
    MCLogBlockPort port(target, xPos, yPos, art != nullptr ? art->Width() : 1, art != nullptr ? art->Height() : 1,
                        true);

    if (art != nullptr)
    {
        VfxPaneCopy(art->Frame(), 0, 0, port.Frame(), 0, 0, -1);
    }

    // The name centred at the top.
    auto writeName = [&](std::string_view name)
    {
        const std::string text(name);
        const int32_t textWidth = GreenFont->Width(text.c_str());
        GreenFont->WriteString(port.Frame(), (Width() - textWidth) / 2, 3, text.c_str(), -1);
    };

    if (Mech == nullptr)
    {
        writeName(Vehicle->FileName);
        MCLogBlockPort body(port.Frame(), 0, 0, port.Width(), port.Height(), true);

        for (int32_t location = 0; location < 5; location++)
        {
            GlobalLogPtr->DrawVehicleBodyLoc(Vehicle, location, &body, 0xd, 0xe);
        }
    }
    else
    {
        MCLogWarrior* warrior = nullptr;

        if (Mech->LocalPart == 0)
        {
            warrior = Mech->NetworkPilot;
        }
        else
        {
            GlobalLogPtr->AssignedWarriorList->GetWarriorInfo(Mech->PilotIndex, warrior);
        }

        // Port fix: a mech without a pilot shows no callsign (the original read through null).
        writeName(warrior != nullptr ? warrior->Callsign : "");

        {
            MCLogBlockPort body(port.Frame(), 2, 0xe, 0x19, 0x1e, true);
            VfxPaneWipe(body.Frame(), 0x10);

            for (int32_t location = 0; location < 8; location++)
            {
                GlobalLogPtr->DrawMechBodyLoc(Mech, location, &body, 0, 0);
            }
        }

        // The pilot's picture.
        const std::string picture = warrior == nullptr ? std::format("{}pilot{:02}.tga", ArtPath, Mech->PilotIndex)
                                                       : std::format("{}{}", ArtPath, warrior->Picture);

        if (MCLogPort* pilot = LogArt(picture))
        {
            pilot->CopyTo(port.Frame(), 0x1c, 0xe, true);
        }

        // The mech's status bar (green, yellow, red) and the pilot's health bar.
        VfxLineDraw(port.Frame(), 3, 0xb, 0x19, 0xb, 0x12);
        VfxLineDraw(port.Frame(), 3, 0xc, 0x19, 0xc, 0x12);
        const float status = Mech->StatusValue;

        if (status != 0.0f)
        {
            uint32_t color;

            if (!(status < 0.5))
            {
                color = 0xb;
            }
            else if (status > 0.2)
            {
                color = 0xf2;
            }
            else
            {
                color = 0xef;
            }

            const int32_t barEnd = static_cast<int32_t>(status * 23.0f) + 3;
            VfxLineDraw(port.Frame(), 3, 0xb, barEnd, 0xb, color);
            VfxLineDraw(port.Frame(), 3, 0xc, barEnd, 0xc, color);
        }

        if (warrior != nullptr && warrior->Health < 6.0f)
        {
            const auto healthEnd = static_cast<int32_t>(warrior->Health * 3.0f + 31.0f);
            VfxLineDraw(port.Frame(), healthEnd, 0xb, 0x2f, 0xb, 0x10);
            VfxLineDraw(port.Frame(), healthEnd, 0xc, 0x2f, 0xc, 0x10);
        }
    }

    if (MultiPlayer() != nullptr)
    {
        // Another player's unit is darkened.
        const int32_t index = Mech != nullptr ? GlobalLogPtr->ForceMechList->GetMechIndex(Mech)
                                              : GlobalLogPtr->ForceVehicleList->GetVehicleIndex(Vehicle);

        if (index < 0)
        {
            GlobalLogPtr->Darken(0, LogisticFadetable, &port);
        }
    }

    if (framed)
    {
        // In a slot: a bevelled frame.
        VfxLineDraw(port.Frame(), 0, 0, Width() - 1, 0, 0x32);
        VfxLineDraw(port.Frame(), 0, 1, 0, Height() - 2, 0x32);
        VfxLineDraw(port.Frame(), 0, Height() - 1, Width() - 1, Height() - 1, 0x15);
        VfxLineDraw(port.Frame(), Width() - 1, 0, Width() - 1, Height() - 2, 0x15);
    }
}
