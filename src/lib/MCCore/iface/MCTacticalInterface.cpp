#include "stdafx.h"
#include "iface/MCTacticalInterface.h"
#include "camera/MCCamera.h"
#include "gui/MCGuiInput.h"
#include "camera/MCMainWindow.h"
#include "gui/MCFloatHelp.h"
#include "iface/MCCommandParser.h"
#include "iface/MCMechBar.h"
#include "iface/MCOrderSink.h"
#include "lib/MCFatal.h"
#include "lib/MCFitIniFile.h"
#include "logistics/logmain.h"
#include "mission/MCScenario.h"
#include "main/MCGameContext.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "object/MCArtillery.h"
#include "object/MCBattleMech.h"
#include "object/MCForces.h"
#include "object/MCMasterComponent.h"
#include "object/MCMechWarrior.h"
#include "object/MCMover.h"
#include "object/MCMoverGroup.h"
#include "object/MCObjectEvent.h"
#include "object/MCObjectQueue.h"
#include "object/MCObjectSystem.h"
#include "platform/MCFrameLog.h"
#include "sound/MCSoundSystem.h"
#include "terrain/MCTacticalMap.h"
#include "terrain/MCTerrain.h"

namespace
{
    /// <summary>
    /// The port's value for the original's <c>GetSystemMetrics(SM_CXDRAG)</c>, the drag distance used when
    /// <c>iface.fit</c> has none: Windows' default.
    /// </summary>
    constexpr int16_t SystemDragWidth = 4;

    /// <summary>A default key binding: the command, the scan code and the modifiers it needs.</summary>
    struct MCDefaultKey
    {
        MCKeyCommand Command = MCKeyCommand::Unused;
        uint32_t Code = 0;
        uint32_t Modifiers = 0;
    };

    /// <summary>
    /// The bindings <see cref="MCTacticalInterface::Init"/> sets, in its order. Codes are DirectInput scan codes (0x1xx
    /// = extended keys).
    /// </summary>
    constexpr MCDefaultKey DefaultKeys[] = {
        {MCKeyCommand::Eject, 0x147, 0},
        {MCKeyCommand::Stop, 0xe, 0},
        {MCKeyCommand::Run, 0x39, 0},
        {MCKeyCommand::ForcedOrder, 0x43, 0},
        {MCKeyCommand::ForcedRun, 0x44, 0},
        {MCKeyCommand::ForcedJump, 0x57, 0},
        {MCKeyCommand::ForcedOrderAlternate, 0x58, 0},
        {MCKeyCommand::RunAlternate, 0x39, KeyCtrl},
        {MCKeyCommand::Info, 0x17, 0},
        {MCKeyCommand::AttackOptimalRange, 0x18, 0},
        {MCKeyCommand::AttackConservingAmmo, 0x1e, 0},
        {MCKeyCommand::AttackLongRange, 0x26, 0},
        {MCKeyCommand::AttackMediumRange, 0x32, 0},
        {MCKeyCommand::AttackShortRange, 0x1f, 0},
        {MCKeyCommand::AttackFromPosition, 0x2e, 0},
        {MCKeyCommand::Jump, 0x24, 0},
        {MCKeyCommand::JumpAlternate, 0x24, KeyCtrl},
        {MCKeyCommand::LayMines, 0x21, 0},
        {MCKeyCommand::LayMinesAlternate, 0x21, KeyCtrl},
        {MCKeyCommand::Guard, 0x22, 0},
        {MCKeyCommand::Unbound, 0xffffffffu, 0},
        {MCKeyCommand::PowerUp, 0x149, 0},
        {MCKeyCommand::PowerDown, 0x151, 0},
        {MCKeyCommand::AimHead, 0x48, 0},
        {MCKeyCommand::AimLeftTorso, 0x47, 0},
        {MCKeyCommand::AimRightTorso, 0x49, 0},
        {MCKeyCommand::AimCenterTorso, 0x4c, 0},
        {MCKeyCommand::AimLeftArm, 0x4b, 0},
        {MCKeyCommand::AimRightArm, 0x4d, 0},
        {MCKeyCommand::AimLeftLeg, 0x4f, 0},
        {MCKeyCommand::AimRightLeg, 0x51, 0},
        {MCKeyCommand::CameraFollow, 0x14, 0},
        {MCKeyCommand::Artillery1, 2, 0},
        {MCKeyCommand::Artillery2, 3, 0},
        {MCKeyCommand::Artillery3, 4, 0},
        {MCKeyCommand::Artillery4, 5, 0},
        {MCKeyCommand::DebugStrike, 0x30, 0},
        {MCKeyCommand::LinkLance1, 0x3b, KeyCtrl},
        {MCKeyCommand::LinkLance2, 0x3c, KeyCtrl},
        {MCKeyCommand::LinkLance3, 0x3d, KeyCtrl},
        {MCKeyCommand::LinkLance4, 0x3e, KeyCtrl},
        {MCKeyCommand::SelectLance1, 0x3b, 0},
        {MCKeyCommand::SelectLance2, 0x3c, 0},
        {MCKeyCommand::SelectLance3, 0x3d, 0},
        {MCKeyCommand::SelectLance4, 0x3e, 0},
        {MCKeyCommand::AddLance1, 0x3b, KeyShift},
        {MCKeyCommand::AddLance2, 0x3c, KeyShift},
        {MCKeyCommand::AddLance3, 0x3d, KeyShift},
        {MCKeyCommand::AddLance4, 0x3e, KeyShift},
        {MCKeyCommand::BreakLances, 0x3f, 0},
        {MCKeyCommand::ZoomIn, 0x4e, 0},
        {MCKeyCommand::ZoomOut, 0x4a, 0},
        {MCKeyCommand::ZoomInAlternate, 0xd, 0},
        {MCKeyCommand::ZoomOutAlternate, 0xc, 0},
        {MCKeyCommand::TacticalMapZoomIn, 0x4e, KeyCtrl},
        {MCKeyCommand::TacticalMapZoomOut, 0x4a, KeyCtrl},
        {MCKeyCommand::TacticalMapZoomInAlternate, 0xd, KeyCtrl},
        {MCKeyCommand::TacticalMapZoomOutAlternate, 0xc, KeyCtrl},
        {MCKeyCommand::HoldTacticalMap, 0x38, KeyAlt},
        {MCKeyCommand::MapPage, 0x32, KeyAlt},
        {MCKeyCommand::SalvagePage, 0x1f, KeyAlt},
        {MCKeyCommand::InfoPage, 0x20, KeyAlt},
        {MCKeyCommand::MissionPage, 0x30, KeyAlt},
        {MCKeyCommand::ScrollUp, 0x148, 0},
        {MCKeyCommand::ScrollDown, 0x150, 0},
        {MCKeyCommand::ScrollLeft, 0x14b, 0},
        {MCKeyCommand::ScrollRight, 0x14d, 0},
        {MCKeyCommand::TacticalMapScrollUp, 0x148, KeyCtrl},
        {MCKeyCommand::TacticalMapScrollDown, 0x150, KeyCtrl},
        {MCKeyCommand::TacticalMapScrollLeft, 0x14b, KeyCtrl},
        {MCKeyCommand::TacticalMapScrollRight, 0x14d, KeyCtrl},
        {MCKeyCommand::TogglePalette, 0xf, 0},
        {MCKeyCommand::SelectVisible, 0x12, 0},
        {MCKeyCommand::Chat, 0x1c, 0},
    };

    /// <summary>
    /// Sets a binding's key code, keeping its modifier bits, then sets <paramref name="modifiers"/> (the original
    /// assigns a bitfield's members one by one).
    /// </summary>
    void BindKey(uint32_t& key, uint32_t code, uint32_t modifiers)
    {
        key = (key & (KeyShift | KeyCtrl | KeyAlt)) + code;

        if (modifiers != 0)
        {
            key = (key & ~modifiers) + modifiers;
        }
    }

    /// <summary>
    /// The strike types <see cref="MCTacticalInterface::CallStrike"/> takes (artillery object type numbers), each with
    /// the <c>CallArtillery</c> strike type for team 0 and team 1.
    /// </summary>
    struct MCStrikeTypeEntry
    {
        int32_t ObjectType = 0;
        std::array<int32_t, 2> StrikeType{};
    };

    constexpr MCStrikeTypeEntry StrikeTypes[] = {
        {0xf9, {0, 4}},  {0xf8, {1, 5}},  {0xfa, {2, 6}},  {0x1fc, {0, 4}},
        {0x1fb, {1, 5}}, {0x1fd, {2, 6}}, {0x204, {3, 7}},
    };

    /// <summary>Each live mech bar mover, in the bar's order.</summary>
    template <typename Visit> void ForEachLiveBarMover(const MCMechBar& bar, Visit visit)
    {
        for (const MCGuiOwned<MCFriendlyMechIcon>& button : bar.Buttons)
        {
            auto* mover = static_cast<MCMover*>(button->Mover);

            if (mover != nullptr && mover->IsDisabled() == 0)
            {
                visit(*mover);
            }
        }
    }
}

auto TacticalInterface() -> MCTacticalInterface*
{
    return MCGameContext::Current().TacticalInterface();
}

auto UpdateMouseStateCallback() -> void
{
    if (Turn > 1)
    {
        MCFrameLog::Scope part("logic.mouseState");
        TacticalInterface()->UpdateMouseState(nullptr);
    }
}

MCTacticalInterface::MCTacticalInterface() = default;

MCTacticalInterface::~MCTacticalInterface()
{
    // As the original's destroy: the bar, the parser, then the tags.
    MechBar.reset();
    CommandParser.reset();

    for (MCGuiOwned<MCFloatHelp>& tag : FloatingTags)
    {
        tag.reset();
    }

    ReserveIcons.clear();

    if (_ScrollCallback != nullptr && GuiSystem() != nullptr)
    {
        GuiSystem()->RemoveCallback(_ScrollCallback.get());
    }
}

auto MCTacticalInterface::Init() -> int32_t
{
    ScrollDirection = -1;
    TacScrollDirection = -1;

    // The mech bar, along the bottom of the screen.
    MechBar = MCMakeGui<MCMechBar>();
    MechBar->Init(0, 0, 1, 1, nullptr);
    MechBar->Resize(0x280, 0x3d);
    MechBar->ShowGuiWindow(0);
    ScreenWindow()->AddChild(MechBar.get());
    MechBar->MoveTo(0, GuiSystem()->Height() - MechBar->Height() - 1, 0);
    MechBar->SetDepth(0x4b);

    // iface.fit's parameters.
    MCFitIniFile ifaceFile;

    if (const int32_t result = ifaceFile.Open(GamePath(InterfacePath, "iface", ".fit")); result != 0)
    {
        return result;
    }

    if (const int32_t result = ifaceFile.SeekBlock("Parameters"); result != 0)
    {
        return result;
    }

    DragDistance = ifaceFile.Read<int16_t>("Drag Distance").value_or(SystemDragWidth);
    ScrollSpeed = ifaceFile.Read<int16_t>("Scroll Speed").value_or(ScrollSpeed);
    TacScrollSpeed = ifaceFile.Read<int16_t>("Tac Scroll Speed").value_or(TacScrollSpeed);
    ScrollStart = ifaceFile.Read<int16_t>("Scroll Start").value_or(ScrollStart);
    ShuffleFrames = ifaceFile.Read<int16_t>("Shuffle Frames").value_or(0xf);
    ifaceFile.Close();

    // The floating tags.
    for (MCGuiOwned<MCFloatHelp>& tag : FloatingTags)
    {
        tag = MCMakeGui<MCFloatHelp>();
        Assert(tag->Init(0, 0, 10, 10, nullptr) == 0, 0, "Error initializing floating tags");
        tag->SetBackColor(0xf4);
        ScreenWindow()->AddChild(tag.get());
        tag->SetDepth(0x28);
        tag->ShowGuiWindow(0);
    }

    // The default key bindings.
    for (const MCDefaultKey& binding : DefaultKeys)
    {
        BindKey(Keys[static_cast<size_t>(binding.Command)], binding.Code, binding.Modifiers);
    }

    RotateKey = 0x38;
    SelectedEnemy = nullptr;
    CursorOffset = 0;
    ForcingOrder = false;
    ForcedOrder = MCForcedOrder::None;
    return 0;
}

auto MCTacticalInterface::HandleEvent(MCGuiEvent* event) -> void
{
    // Mission messages and keys.
    switch (event->Type)
    {
        case 0x1404:
        {
            if (Scenario() != nullptr)
            {
                Scenario()->CreatePartObject(event->Data);
            }

            return;
        }
        case 0x1403:
        {
            if (Scenario() != nullptr)
            {
                Scenario()->DestroyPartObject(event->Data);
            }

            return;
        }
        case 0x1402:
        {
            RemoveMech(event->Data);
            return;
        }
        case 9:
        {
            HandleKeyDown(event);
            return;
        }
        case 8:
        {
            HandleKeyUp(event);
            return;
        }
        default:
        {
            if (event->Type > 8)
            {
                return;
            }

            HandleMouse(event);
            return;
        }
    }
}

auto MCTacticalInterface::ZoomIn(float factor, bool sound) -> void
{
    // Port: the view shows fewer lines of the world (the original switched the camera to scale 100); the tactical
    // map's zoom button follows.
    if (Eye == nullptr || Eye->View() == nullptr || GamePaused != 0 || GameAsked != 0)
    {
        return;
    }

    if (Eye->View()->ZoomBy(1.0f / factor))
    {
        if (sound)
        {
            SoundSystem()->PlayDigitalSample(0x44, 1, nullptr, 0, 0);
        }

        Eye->ForceUpdate = 1;
        MCTerrain::ForceRedraw = 1;
    }
}

auto MCTacticalInterface::ZoomOut(float factor, bool sound) -> void
{
    // Port: the view shows more lines of the world (the original switched the camera to scale 1); the tactical map's
    // zoom button follows.
    if (Eye == nullptr || Eye->View() == nullptr || GamePaused != 0 || GameAsked != 0)
    {
        return;
    }

    if (Eye->View()->ZoomBy(factor))
    {
        if (sound)
        {
            SoundSystem()->PlayDigitalSample(0x45, 1, nullptr, 0, 0);
        }

        Eye->ForceUpdate = 1;
        MCTerrain::ForceRedraw = 1;
    }
}

auto MCTacticalInterface::StartScenario() -> void
{
    CommandParser = std::make_unique<MCCommandParser>(*this);
    MouseTarget = MCMouseTarget::NotYetUpdated;
    MouseObject = nullptr;
    SetMode(MCInterfaceMode::None);
    UpdateInterface();

    if (MainHolder() != nullptr)
    {
        MainHolder()->ShowGuiWindow(1);
    }

    if (_ScrollCallback == nullptr)
    {
        _ScrollCallback = std::make_unique<MCGuiCallback>();
        _ScrollCallback->SetExec(ScrollScreen);
        GuiSystem()->AddCallback(_ScrollCallback.get());
    }

    SelectedEnemy = nullptr;
    ::TacticalMap()->SetDepth(0x50);

    if (MCFriendlyMechIcon* first = MechBar->GetButton(0); first != nullptr)
    {
        ::TacticalMap()->SetID(first->PartId);
    }

    MechBar->ShowGuiWindow(1);
    MechBar->InitLances();
    DragTarget = nullptr;
    ForcingOrder = false;
    // The salvage page's key (alt+S by default) becomes alt+C, the chat page, in multiplayer.
    BindKey(Keys[static_cast<size_t>(MCKeyCommand::SalvagePage)], MPlayer != nullptr ? 0x2e : 0x1f, 0);
}

auto MCTacticalInterface::EndScenario() -> void
{
    ClearMechSelection();
    _HighlightedObject = nullptr;
    CommandParser.reset();
    MechBar->CleanUp();
    MechBar->ShowGuiWindow(0);
    HideTags();

    if (MainHolder() != nullptr)
    {
        MainHolder()->ShowGuiWindow(0);
    }

    GuiSystem()->RemoveCallback(_ScrollCallback.get());
    _ScrollCallback.reset();
    ReserveIcons.clear();
}

auto MCTacticalInterface::AddMech(int32_t partId, int32_t lance, bool active, bool onBar) -> void
{
    auto icon = MCMakeGui<MCFriendlyMechIcon>();
    icon->Init(0, 0, 0x34, 0x2e, nullptr);
    icon->SetID(partId);
    icon->Active = active;
    icon->ShowGuiWindow(active ? 1 : 0);

    if (!onBar)
    {
        icon->Lance = NoLance;
        ReserveIcons.push_back(std::move(icon));
        return;
    }

    icon->Lance = lance;
    MCFriendlyMechIcon* button = icon.get();

    if (MechBar->AddButton(std::move(icon)))
    {
        MechBar->Draw();
        button->SetEventRoutine(MechIconHandleEvent);
    }
}

auto MCTacticalInterface::ActivateMech(int32_t partId) -> void
{
    if (MCFriendlyMechIcon* icon = MechBar->GetButtonFromID(partId); icon != nullptr)
    {
        icon->Active = true;
        icon->ShowGuiWindow(1);
        icon->SetID(icon->PartId);
        MechBar->PlaceButtons(true);
    }
}

auto MCTacticalInterface::RemoveMech(int32_t partId) -> void
{
    // The original narrowed the part id to 16 bits for this check.
    if (IsOurs(static_cast<int16_t>(partId)))
    {
        MCFriendlyMechIcon* icon = MechBar->GetButtonFromID(partId);

        if (icon != nullptr)
        {
            if (!icon->IsPoint)
            {
                // Drop it from the selection (OB-156 fixed) and tell the mover.
                std::erase(SelectedMovers, partId);

                if (ObjectList() != nullptr)
                {
                    if (MCBaseObject* object = ObjectList()->FindObjectFromPart(partId); object != nullptr)
                    {
                        MCObjectEvent deselect;
                        deselect.Init(0x1d, nullptr);
                        object->HandleEvent(&deselect);
                    }
                }
            }
            else
            {
                // The lance's point died: once no member is left standing, the lance goes from the selection.
                MCLanceIcon* lanceIcon = MechBar->GetLanceIconFromID(icon->Lance);
                Assert(lanceIcon != nullptr, partId, " InterfaceObject.RemoveMech: NULL lanceIcon ");
                lanceIcon->Linked = false;
                MCMoverGroup* group = lanceIcon->Group;
                const bool anyStanding =
                    std::ranges::any_of(group->Movers, [](MCMover* member) { return member->IsDisabled() == 0; });

                if (!anyStanding)
                {
                    DeselectLance(group);
                    CommandParser->RemoveSubject(group);
                }
            }
        }

        if (CommandParser != nullptr)
        {
            CommandParser->RemoveSubject(partId);
        }
    }

    if (SelectedEnemy != nullptr && SelectedEnemy->PartId == partId)
    {
        SelectedEnemy = nullptr;
    }
}

auto MCTacticalInterface::UpdateInterface() -> void
{
    MechBar->Draw();
}

auto MCTacticalInterface::CallStrike(int32_t strikeType, MCVector3D* position, MCGameObject* target, bool forCommander,
                                     bool forClans, float delay) -> void
{
    const auto entry = std::ranges::find(StrikeTypes, strikeType, &MCStrikeTypeEntry::ObjectType);

    if (entry == std::end(StrikeTypes))
    {
        return;
    }

    MCVector3D location;

    if (position != nullptr)
    {
        location = *position;
    }
    else
    {
        if (target == nullptr)
        {
            return;
        }

        location = target->GetPosition();
    }

    int32_t commanderId = 0;
    MCTeam* team = InnerSphereTeam();

    if (forCommander)
    {
        commanderId = HomeCommander()->GetId();
        team = HomeCommander()->GetTeam();
    }
    else if (forClans)
    {
        if (MPlayer != nullptr)
        {
            Fatal(0, " Iface.CallStrike: Need more info than clanStrike in MPlayer ");
        }

        team = ClanTeam();
        commanderId = 1;
    }

    // Artillery and sensor strikes must be aimed at a point the team can see; the others can go anywhere.
    const int32_t artilleryType = entry->StrikeType[team->Id];
    const bool needsSight = artilleryType == 0 || artilleryType == 1 || artilleryType == 4 || artilleryType == 5;

    if (needsSight && team->LineOfSight(location) == 0)
    {
        SoundSystem()->PlayDigitalSample(0x33, 1, nullptr, 0, 0);
        return;
    }

    const auto seconds = static_cast<int32_t>(delay);

    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        MPlayer->SendPlayerArtillery(MPlayer->ServerID, artilleryType, location, seconds);
        return;
    }

    CallArtillery(commanderId, artilleryType, location, seconds, 0);
}

auto MCTacticalInterface::HideTags() -> void
{
    for (const MCGuiOwned<MCFloatHelp>& tag : FloatingTags)
    {
        tag->ShowGuiWindow(0);
    }
}

auto MCTacticalInterface::SetUnit(int32_t groupId, std::span<MCMover*> movers, int32_t pointIndex) -> void
{
    const auto numMovers = static_cast<int32_t>(movers.size());
    HomeCommander()->SetGroup(groupId, numMovers, movers.data(), pointIndex);

    if (MPlayer != nullptr)
    {
        MPlayer->SendPlayerMoverGroup(MPlayer->AllPlayerGroupID, groupId, numMovers, movers.data(), pointIndex);
    }

    // Relink every icon to its lance and mark the points.
    for (const MCGuiOwned<MCFriendlyMechIcon>& button : MechBar->Buttons)
    {
        button->Lance = NoLance;
    }

    for (int32_t lance = 0; lance < static_cast<int32_t>(MCMechBar::NumLances); lance++)
    {
        MCMoverGroup* group = HomeCommander()->GetGroup(lance);

        for (MCMover* member : group->Movers)
        {
            if (member != nullptr)
            {
                MechBar->GetButtonFromID(member->PartId)->Lance = lance;
            }
        }

        if (MCMover* point = group->GetPoint(); point != nullptr)
        {
            MechBar->GetButtonFromID(point->PartId)->IsPoint = true;
        }

        // Original behaviour: marks the lance icon of groupId each time, not that of the lance just relinked.
        MechBar->GetLanceIconFromID(groupId)->Linked = true;
    }
}

auto MCTacticalInterface::SetPoint(int32_t partId, bool isPoint) -> void
{
    if (MCFriendlyMechIcon* button = MechBar->GetButtonFromID(partId); button != nullptr)
    {
        button->IsPoint = isPoint;
    }
}

auto MCTacticalInterface::GetMechIconFromID(int32_t partId) const -> MCFriendlyMechIcon*
{
    if (MCFriendlyMechIcon* icon = MechBar->GetButtonFromID(partId); icon != nullptr)
    {
        return icon;
    }

    const auto found = std::ranges::find_if(ReserveIcons, [partId](const MCGuiOwned<MCFriendlyMechIcon>& icon)
                                            { return icon->PartId == partId; });
    return found != ReserveIcons.end() ? found->get() : nullptr;
}

auto MCTacticalInterface::Orders() const -> MCOrderSink&
{
    return _OrderSink != nullptr ? *_OrderSink : GameOrderSink();
}

auto MCTacticalInterface::TagMover(MCFloatHelp& tag, MCGameObject& mover) -> void
{
    std::string text =
        std::format("{}\n{}", mover.GetPilot()->Callsign, MCPrintfText(static_cast<MCMover&>(mover).GetIfaceName()));
    tag.HelpObject = &mover;
    tag.SetBackColor(0);
    tag.TextColor = 0xb;
    tag.SetHelpText(text.data());
}

auto MCTacticalInterface::CheatHealAll() -> void
{
    ForEachLiveBarMover(*MechBar,
                        [](MCMover& mover)
                        {
                            for (int32_t j = 0; j < mover.NumArmorLocations(); j++)
                            {
                                mover.Armor[j].CurArmor = static_cast<float>(mover.Armor[j].MaxArmor);
                            }

                            for (int32_t j = 0; j < mover.NumBodyLocations(); j++)
                            {
                                mover.BodyAt(j).CurInternalStructure =
                                    static_cast<float>(mover.BodyAt(j).MaxInternalStructure);
                            }

                            if (mover.ObjectClass == MCObjectClass::BattleMech)
                            {
                                static_cast<MCBattleMech&>(mover).CalcLegStatus();
                                static_cast<MCBattleMech&>(mover).CalcTorsoStatus();
                            }

                            const uint32_t firstWeapon = mover.NumOther;

                            for (uint32_t j = firstWeapon; j < firstWeapon + mover.NumWeapons; j++)
                            {
                                mover.Inventory[j].Disabled = 0;
                            }

                            for (int32_t j = 0; j < mover.NumAmmoTypes(); j++)
                            {
                                if (mover.AmmoTypeTotal[j].CurAmount != 9999)
                                {
                                    mover.AmmoTypeTotal[j].CurAmount = mover.AmmoTypeTotal[j].StartAmount;
                                }
                            }
                        });
}

auto MCTacticalInterface::CheatDeadEye() -> void
{
    ForEachLiveBarMover(*MechBar,
                        [](MCMover& mover)
                        {
                            if (mover.GetPilot() != nullptr)
                            {
                                mover.GetPilot()->Skills[SkillGunnery] = 120;
                            }
                        });
}
