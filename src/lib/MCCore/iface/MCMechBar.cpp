#include "stdafx.h"
#include "iface/MCMechBar.h"
#include "camera/MCMainWindow.h"
#include "iface/MCTacticalInterface.h"
#include "lib/MCMsvcSort.h"
#include "object/MCMoverGroup.h"
#include "platform/MCRenderer.h"
#include "sound/MCSoundSystem.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>A button's place in <see cref="MCMechBar::PlaceButtons"/>'s sort.</summary>
    struct MCButtonSortEntry
    {
        /// <summary>The button's index, or -1 for an empty slot.</summary>
        int32_t Index = 0;
        int32_t Key = 0;
    };
}

auto MCMechBar::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* bitmapName) -> int32_t
{
    MCGuiObject::Init(xPos, yPos, width, height, bitmapName);
    // The bar draws on its parent: no bitmap of its own.
    MCRenderer::DestroyTexture(Port()->Bitmap());
    MCGuiPort::FreePixels(Port()->Bitmap()->Buffer);
    Port()->Bitmap()->Buffer = nullptr;
    ShowGuiWindow(false);
    Dancing = false;
    SpacingX = 0x34;
    SpacingY = 0x2e;
    return 0;
}

auto MCMechBar::Destroy() -> void
{
    CleanUp();
    MCGuiObject::Destroy();
}

auto MCMechBar::Display() -> void
{
    if (ShowWindow == 0)
    {
        return;
    }

    for (size_t i = 0; i < ChildList.size(); i++)
    {
        ChildList[i]->Display();
    }

    if (!Dancing)
    {
        // A separator after each lance icon shown, and around each button outside a lance.
        for (const MCGuiOwned<MCLanceIcon>& lanceIcon : LanceIcons)
        {
            if (lanceIcon != nullptr && lanceIcon->IsShowing() != 0)
            {
                const int32_t lanceRight = lanceIcon->Right();
                VfxLineDraw(Frame(), lanceRight - 1, 0xf, lanceRight - 1, Height() - 1, 0x10);
            }
        }

        for (const MCGuiOwned<MCFriendlyMechIcon>& button : Buttons)
        {
            button->DrawBox(0x35, -1, -1, -1, -1);

            if (button->IsShowing() != 0 && button->Lance == NoLance)
            {
                const int32_t buttonRight = button->Right();
                VfxLineDraw(Frame(), buttonRight, 0xf, buttonRight, Height() - 1, 0x10);
                VfxLineDraw(Frame(), 0, 0xf, buttonRight, 0xf, 0x10);
            }
        }
    }

    // Each button's frame: red on the video pilot, else yellow when selected, else white under the mouse.
    for (const MCGuiOwned<MCFriendlyMechIcon>& button : Buttons)
    {
        const int32_t buttonWidth = button->Width();
        const int32_t buttonHeight = button->Height();

        if (VideoId == button->PartId)
        {
            button->DrawBox(0xef, 1, 1, buttonWidth - 2, buttonHeight - 2);
        }
        else if (TacticalInterface()->IsSelected(button->PartId))
        {
            button->DrawBox(0x1f, 1, 1, buttonWidth - 2, buttonHeight - 2);
        }
        else if (HighlightId == button->PartId)
        {
            button->DrawBox(0xb, 1, 1, buttonWidth - 2, buttonHeight - 2);
        }
    }
}

auto MCMechBar::HandleEvent(MCGuiEvent* event) -> void
{
    // A broadcast (a resolution change): back to the bottom of the screen.
    if (event->Type == 0x12)
    {
        MoveTo(1, GuiSystem()->Height() - Height() - 1, false);
    }

    event->Target = MainHolder()->GetActivePane();
    TacticalInterface()->HandleEvent(event);
}

auto MCMechBar::Resize(int32_t width, int32_t height) -> void
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    if (width == WinWidth && height == WinHeight)
    {
        return;
    }

    WinWidth = width;
    WinHeight = height;
    FramePane->X1 = FramePane->X0 - 1 + width;
    FramePane->Y1 = FramePane->Y0 - 1 + height;
}

auto MCMechBar::CleanUp() -> void
{
    // Last to first, as the original removed them.
    while (!Buttons.empty())
    {
        Buttons.pop_back();
    }

    for (MCGuiOwned<MCLanceIcon>& lanceIcon : LanceIcons)
    {
        lanceIcon.reset();
    }
}

auto MCMechBar::AddButton(MCGuiOwned<MCFriendlyMechIcon> button) -> bool
{
    if (Buttons.size() >= MaxButtons)
    {
        return false;
    }

    button->MoveTo((SpacingX + 1) * static_cast<int32_t>(Buttons.size()) + 2, 0xf, false);
    AddChild(button.get());
    Buttons.push_back(std::move(button));
    return true;
}

auto MCMechBar::GetButton(size_t index) const -> MCFriendlyMechIcon*
{
    return index < Buttons.size() ? Buttons[index].get() : nullptr;
}

auto MCMechBar::GetButtonFromID(int32_t partId) const -> MCFriendlyMechIcon*
{
    const auto found = std::ranges::find_if(Buttons, [partId](const MCGuiOwned<MCFriendlyMechIcon>& button)
                                            { return button->PartId == partId; });
    return found != Buttons.end() ? found->get() : nullptr;
}

auto MCMechBar::PlaceButtons(bool animate) -> void
{
    // Sort the buttons by lance, then by x; a lance's point first; inactive movers last. The sort runs over all
    // twelve slots, the empty ones first, so equal keys come out as in the original (rule R5).
    std::array<MCButtonSortEntry, MaxButtons> order;

    for (size_t i = 0; i < MaxButtons; i++)
    {
        MCFriendlyMechIcon* button = GetButton(i);

        if (button == nullptr)
        {
            order[i] = {-1, -999999};
            continue;
        }

        int32_t key = button->X() + button->Lance * 10000;

        if (button->IsPoint)
        {
            key -= 1000;
        }

        if (!button->Active)
        {
            key += 100000;
        }

        order[i] = {static_cast<int32_t>(i), key};
    }

    MCMsvcSort(std::span<MCButtonSortEntry>(order),
               [](const MCButtonSortEntry& a, const MCButtonSortEntry& b)
               {
                   if (a.Key == b.Key)
                   {
                       return 0;
                   }

                   return b.Key < a.Key ? 1 : -1;
               });

    for (const MCGuiOwned<MCLanceIcon>& lanceIcon : LanceIcons)
    {
        if (lanceIcon != nullptr)
        {
            lanceIcon->ShowTest();
        }
    }

    // Each lance starts with its icon, one pixel after the previous group; an inactive mover goes off the bar.
    int32_t nextX = 0;
    int32_t lastLance = -1;

    for (const MCButtonSortEntry& entry : order)
    {
        if (entry.Index < 0)
        {
            continue;
        }

        MCFriendlyMechIcon* button = Buttons[entry.Index].get();
        const int32_t buttonLance = button->Lance;
        int32_t xPos = nextX;

        if (buttonLance != lastLance || buttonLance == NoLance)
        {
            if (nextX != 0)
            {
                xPos = nextX + 1;
            }

            lastLance = buttonLance;

            if (buttonLance < static_cast<int32_t>(NumLances) && LanceIcons[buttonLance] != nullptr)
            {
                LanceIcons[buttonLance]->MoveTo(xPos, 2, false);
            }

            xPos++;
        }

        button->TargetX = button->Active ? xPos : xPos + 1000;
        button->ShuffleStep = (xPos - button->X()) / TacticalInterface()->ShuffleFrames;
        nextX = xPos - 1 + SpacingX;
    }

    if (!animate)
    {
        for (const MCGuiOwned<MCFriendlyMechIcon>& button : Buttons)
        {
            button->MoveTo(button->TargetX, button->Y(), false);
        }

        const int32_t linkWidth = SpacingX - 1;

        for (const MCGuiOwned<MCLanceIcon>& lanceIcon : LanceIcons)
        {
            lanceIcon->Resize(lanceIcon->GetNumActiveMovers() * linkWidth + 3, lanceIcon->Height());
        }

        return;
    }

    if (_DanceCallback == nullptr)
    {
        _DanceStep = 0;
        _DanceFrames = 0;
        Dancing = true;
        _DanceCallback = std::make_unique<MCGuiCallback>();
        _DanceCallback->SetExec([] { TacticalInterface()->MechBar->Dance(); });
        GuiSystem()->AddCallback(_DanceCallback.get());
        SoundSystem()->PlayDigitalSample(0x42, 1, nullptr, false, false);
    }
}

auto MCMechBar::InitLances() -> void
{
    for (size_t i = 0; i < NumLances; i++)
    {
        LanceIcons[i] = MCMakeGui<MCLanceIcon>();
        LanceIcons[i]->Init(static_cast<int16_t>(i));
        AddChild(LanceIcons[i].get());
    }

    PlaceButtons(false);
}

auto MCMechBar::GetLanceIconFromID(int32_t lanceId) const -> MCLanceIcon*
{
    for (const MCGuiOwned<MCLanceIcon>& lanceIcon : LanceIcons)
    {
        if (lanceIcon != nullptr && lanceIcon->LanceId == lanceId)
        {
            return lanceIcon.get();
        }
    }

    return nullptr;
}

auto MCMechBar::Dance() -> void
{
    // Three phases: the lance icons drop out of sight while the buttons that move left rise; the buttons slide to
    // their places; then the lance icons come back up and the buttons drop into line.
    const int32_t step = LanceIcons[0]->Height() / 3 + 1;
    const int16_t shuffleFrames = TacticalInterface()->ShuffleFrames;

    if (_DanceFrames == 3)
    {
        _DanceStep++;
    }

    if (_DanceFrames == shuffleFrames + 3)
    {
        for (const MCGuiOwned<MCFriendlyMechIcon>& button : Buttons)
        {
            button->MoveTo(button->TargetX, button->Y(), false);
        }

        _DanceStep++;
    }

    if (_DanceStep == 0)
    {
        for (const MCGuiOwned<MCLanceIcon>& lanceIcon : LanceIcons)
        {
            lanceIcon->MoveTo(lanceIcon->X(), lanceIcon->Y() + step, false);
            lanceIcon->SetDepth(-10);
        }

        for (const MCGuiOwned<MCFriendlyMechIcon>& button : Buttons)
        {
            if (button->TargetX < button->X())
            {
                button->MoveTo(button->X(), button->Y() - step, false);
                button->SetDepth(10);
            }
        }
    }
    else if (_DanceStep == 1)
    {
        for (const MCGuiOwned<MCFriendlyMechIcon>& button : Buttons)
        {
            button->MoveTo(button->X() + button->ShuffleStep, button->Y(), false);
        }

        for (const MCGuiOwned<MCLanceIcon>& lanceIcon : LanceIcons)
        {
            lanceIcon->ShowGuiWindow(false);
        }
    }
    else if (_DanceStep == 2)
    {
        for (const MCGuiOwned<MCLanceIcon>& lanceIcon : LanceIcons)
        {
            if (lanceIcon->Group != nullptr)
            {
                lanceIcon->NumActiveMovers = lanceIcon->GetNumActiveMovers();
            }

            lanceIcon->MoveTo(lanceIcon->X(), lanceIcon->Y() - step, false);
            lanceIcon->ShowTest();
        }

        for (const MCGuiOwned<MCFriendlyMechIcon>& button : Buttons)
        {
            if (button->Y() != 0xf)
            {
                button->MoveTo(button->X(), button->Y() + step, false);
            }
        }

        if (shuffleFrames + 6 <= _DanceFrames)
        {
            // Done: everything at rest, the lance icons sized to their lances.
            for (const MCGuiOwned<MCFriendlyMechIcon>& button : Buttons)
            {
                button->SetDepth(0);
            }

            for (const MCGuiOwned<MCLanceIcon>& lanceIcon : LanceIcons)
            {
                lanceIcon->SetDepth(0);
                lanceIcon->MoveTo(lanceIcon->X(), 2, false);
                lanceIcon->Resize(lanceIcon->GetNumActiveMovers() * (SpacingX - 1) + 3, lanceIcon->Height());
            }

            Dancing = false;
            GuiSystem()->RemoveCallback(_DanceCallback.get());
            Draw();
            SoundSystem()->PlayDigitalSample(0x43, 1, nullptr, false, false);
            _DanceFrames++;
            // The callback running this goes last (aCallback's run allows it, OB-109).
            _DanceCallback.reset();
            return;
        }
    }

    _DanceFrames++;
}
