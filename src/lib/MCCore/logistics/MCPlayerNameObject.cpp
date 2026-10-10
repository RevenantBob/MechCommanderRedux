#include "stdafx.h"
#include "logistics/MCPlayerNameObject.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiGlobals.h"
#include "linkup/MCFidpPlayer.h"
#include "linkup/MCSessionManager.h"
#include "network/MCMultiPlayer.h"
#include "vfx/MCVfxFunctions.h"

auto MCPlayerNameObject::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char*) -> int32_t
{
    PlayerName.clear();
    int32_t result = MCLogObject::Init(xPos, yPos, width, height);
    SetFont(LgWhiteFont);
    BackgroundColor = 0xff;
    return result;
}

auto MCPlayerNameObject::Destroy() -> void
{
    PlayerName.clear();
    MCLogObject::Destroy();
}

auto MCPlayerNameObject::Draw() -> void
{
    if (!_Port->ViewOpen())
    {
        return;
    }

    if (NumberArt != nullptr)
    {
        VfxPaneWipe(_Port->Frame(), static_cast<uint32_t>(NumberBack));
        NumberArt->CopyTo(_Port->Frame(), 1, 1, 0);
    }

    const auto color = static_cast<uint8_t>(BackgroundColor);
    const auto bottom = static_cast<int16_t>(Height() - 1);
    FillBox(0x14, 1, static_cast<int16_t>(Width() - 1), bottom, color);

    // The name is left out while it is dragged.
    if (Font != nullptr && GuiSystem()->GrabbedObject() != this)
    {
        Font->WriteString(_Port->Frame(), 0x1b, 2, PlayerName);
    }
}

auto MCPlayerNameObject::HandleEvent(MCGuiEvent* event) -> void
{
    switch (event->Type)
    {
        case 1:
        {
            if (Draggable)
            {
                GuiSystem()->Grab(this);
                StartDrag(std::min(event->X - X(), 0x14), event->Y - Y());
            }
            break;
        }
        case 4:
        {
            if (GuiSystem()->GrabbedObject() == this)
            {
                GuiSystem()->Release();
                ShowGuiWindow(false);
                // The original looks up the object under the drop and drops the result.
                ScreenWindow()->FindObject(event->X, event->Y);
                ShowGuiWindow(true);
                // Tell the session screen where the name was dropped.
                MCGuiEvent dropped;
                dropped.Clear();
                dropped.Type = 0x1d;
                dropped.Target = this;
                Parent->HandleEvent(&dropped);
            }
            break;
        }
        case 7:
        {
            if (GuiSystem()->GrabbedObject() == this)
            {
                MoveTo(event->X - DragStartX(), event->Y - DragStartY(), false);
            }
            break;
        }
        default:
            break;
    }

    MCGuiObject::HandleEvent(event);
}

auto MCPlayerNameObject::SetPlayerName(std::string_view name) -> void
{
    PlayerName = name;
}

auto MCPlayerNameObject::SetPlayerId(uint32_t newPlayerId) -> void
{
    PlayerId = newPlayerId;

    if (MultiPlayer() != nullptr && MultiPlayer()->SessionManager != nullptr &&
        MultiPlayer()->SessionManager->GetPlayer(newPlayerId) != nullptr)
    {
        SetPlayerName(MultiPlayer()->SessionManager->GetPlayer(newPlayerId)->Name);
    }
}

auto MCPlayerNameObject::SetFont(MCGuiFont* newFont) -> void
{
    Font = newFont;
}
