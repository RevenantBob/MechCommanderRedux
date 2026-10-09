#include "stdafx.h"
#include "iface/MCLanceIcon.h"
#include "ai/MCTacticalOrder.h"
#include "gui/ahelp.h"
#include "iface/MCCommandParser.h"
#include "iface/MCFriendlyMechIcon.h"
#include "iface/MCMechBar.h"
#include "iface/MCOrderSink.h"
#include "iface/MCTacticalInterface.h"
#include "lib/MCFatal.h"
#include "object/MCForces.h"
#include "object/MCMover.h"
#include "object/MCMoverGroup.h"
#include "sound/MCSoundSystem.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>Loads one of the badge's images (fatal when it is missing).</summary>
    MCGuiOwned<MCGuiPort> LoadLanceImage(const std::string& name, const char* failure)
    {
        auto image = MCMakeGui<MCGuiPort>();
        Assert(image->Init(const_cast<char*>(name.c_str())) == 0, 0, failure);
        return image;
    }
}

auto MCLanceIcon::Init(int16_t lanceNumber) -> int32_t
{
    const int32_t result = MCGuiObject::Init(0, 2, 0x25, 0xd, nullptr);

    if (result != 0)
    {
        return result;
    }

    NumberImage = LoadLanceImage(std::format("guiubf{}.tga", lanceNumber + 1), "Can't load lance icon's gold star");
    FirstLinkImage = LoadLanceImage("guiub01.tga", "Can't load lance icon's first curve");
    ShortLinkImage = LoadLanceImage("guiub02.tga", "Can't load lance icon's short line");
    LongLinkImage = LoadLanceImage("guiub03.tga", "Can't load lance icon's long line");
    LastLinkImage = LoadLanceImage("guiub04.tga", "Can't load lance icon's second curve");
    Group = HomeCommander()->GetGroup(lanceNumber);
    LanceId = Group->GetId();
    return result;
}

auto MCLanceIcon::Destroy() -> void
{
    NumberImage.reset();
    FirstLinkImage.reset();
    ShortLinkImage.reset();
    LongLinkImage.reset();
    LastLinkImage.reset();
}

auto MCLanceIcon::Display() -> void
{
    if (ShowWindow == 0)
    {
        return;
    }

    MCPane* pane = FramePane;
    NumberImage->CopyTo(pane, 0, 0, 1);
    const int32_t numberWidth = NumberImage->Width();
    FirstLinkImage->CopyTo(pane, numberWidth, 0, 1);
    const int32_t linkStart = numberWidth + FirstLinkImage->Width();
    ShortLinkImage->CopyTo(pane, linkStart, 4, 1);
    int32_t xPos = linkStart + ShortLinkImage->Width();
    int32_t linkEnd = xPos;

    for (int32_t i = 1; i < GetNumActiveMovers(); i++)
    {
        LongLinkImage->CopyTo(FramePane, xPos, 4, 1);
        xPos += LongLinkImage->Width();
        linkEnd += LongLinkImage->Width();
    }

    LastLinkImage->CopyTo(pane, xPos, 4, 1);

    // The lance colour under the links.
    MCPane bar = *pane;
    bar.X0 = pane->X0 + linkStart;
    bar.Y1 = pane->Y0 + 10;
    bar.X1 = pane->X0 - 1 + linkEnd;
    bar.Y0 = pane->Y0 + 8;
    VfxPaneWipe(&bar, LanceColors[LanceId]);
}

auto MCLanceIcon::HandleEvent(MCGuiEvent* event) -> void
{
    if (event->Type == 1)
    {
        Application->Grab(this);
        return;
    }

    if (event->Type != 4 || Application->GrabbedObject() != this)
    {
        return;
    }

    Application->Release();
    MCTacticalInterface* iface = TacticalInterface();

    // The eject, power down and power up modes go to the whole lance.
    const MCInterfaceMode mode = iface->CurrentMode;

    if (mode == MCInterfaceMode::Eject || mode == MCInterfaceMode::PowerUp || mode == MCInterfaceMode::PowerDown)
    {
        MCTacticalOrderCode code = MCTacticalOrderCode::PowerDown;

        if (mode == MCInterfaceMode::Eject)
        {
            code = MCTacticalOrderCode::Eject;
        }
        else if (mode == MCInterfaceMode::PowerUp)
        {
            code = MCTacticalOrderCode::PowerUp;
        }

        MCTacticalOrder order;
        order.Reset();
        order.Reset(MCOrderOrigin::Player, code, 1);
        iface->Orders().Give(*Group, order, nullptr);
        iface->UpdateInterface();
        return;
    }

    // Otherwise the click selects the lance; shift adds it to the selection or takes it out.
    if (event->ShiftKey == 0)
    {
        iface->DeselectEnemy();
        iface->ClearMechSelection();
        iface->CommandParser->ClearSubjects();
        SoundSystem()->PlayDigitalSample(0x30, 1, nullptr, 0, 0);
        iface->SelectLance(Group);
        iface->CommandParser->AddSubject(Group);
        iface->UpdateInterface();
        return;
    }

    if (iface->IsSelected(Group))
    {
        iface->DeselectLance(Group);
        iface->CommandParser->RemoveSubject(Group);
        iface->UpdateInterface();
        return;
    }

    iface->DeselectEnemy();
    SoundSystem()->PlayDigitalSample(0x30, 1, nullptr, 0, 0);
    iface->SelectLance(Group);
    iface->CommandParser->AddSubject(Group);
    iface->UpdateInterface();
}

auto MCLanceIcon::Enter() -> void
{
    MCTacticalInterface* iface = TacticalInterface();

    for (int32_t i = 0; i < GetNumActiveMovers(); i++)
    {
        // The lance's movers, by their place in it: an active button's mover past the twelfth would have no tag.
        MCFloatHelp* tag = static_cast<uint8_t>(i) < iface->FloatingTags.size()
                               ? iface->FloatingTags[static_cast<uint8_t>(i)].get()
                               : nullptr;
        MCMover* member = Group->Movers[i];

        if (member == nullptr || member->OnScreen() == 0)
        {
            continue;
        }

        MCFriendlyMechIcon* button = iface->MechBar->GetButtonFromID(member->PartId);

        if (button == nullptr || !button->Active || member->GetPilot() == nullptr)
        {
            continue;
        }

        MCTacticalInterface::TagMover(*tag, *member);
        tag->ShowGuiWindow(1);
    }

    MCGuiObject::Enter();
}

auto MCLanceIcon::Leave() -> void
{
    TacticalInterface()->HideTags();
    MCGuiObject::Leave();
}

auto MCLanceIcon::ShowTest() -> void
{
    ShowGuiWindow(Group != nullptr && GetNumActiveMovers() > 0 ? 1 : 0);
}

auto MCLanceIcon::GetNumActiveMovers() const -> int32_t
{
    return static_cast<int32_t>(std::ranges::count_if(TacticalInterface()->MechBar->Buttons,
                                                      [this](const MCGuiOwned<MCFriendlyMechIcon>& button)
                                                      { return button->Lance == LanceId && button->Active; }));
}
