#include "stdafx.h"
#include "camera/MCMainWindow.h"
#include "camera/MCCamera.h"
#include "camera/MCCameraList.h"
#include "engine/MCFont.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "lib/MCFatal.h"
#include "mission/scenario.h"
#include "terrain/MCTerrain.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>A font's line height: its height, scaled and rounded down when the font is scaled (an inline of the
    /// original's font.h).</summary>
    uint8_t LineHeight(const MCFont& font)
    {
        uint8_t height = font.FontHeight;

        if (font.Scaled != 0)
        {
            height = static_cast<uint8_t>(static_cast<int32_t>(std::floor(static_cast<float>(height) * font.Scale)));
        }

        return height;
    }
}

auto MainHolder() -> MCMainWindow*
{
    MCCameraList* list = CameraList();
    return list != nullptr ? list->MainHolder() : nullptr;
}

auto ToggleZoom() -> void
{
    MainHolder()->ZoomActivePane();
}

MCMainWindow::~MCMainWindow() = default;

auto MCMainWindow::Init() -> int32_t
{
    LastClockTime = -1.0f;
    return Init(0, 0, Application->Width(), Application->Height(), nullptr);
}

auto MCMainWindow::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    const int32_t result = MCGuiHolderObject::Init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    ClockPane = MCMakeGui<MCGuiObject>();
    LineFont->Scale = 1.5f;
    LineFont->Scaled = 1;
    const int32_t clockHeight = WhiteFont == nullptr ? 24 : LineHeight(*LineFont) + 4;
    ClockPane->Init(0, 0, 40, clockHeight, nullptr);
    ClockPane->SetBackColor(0x10);
    Retile();
    SetDepth(-50);
    return 0;
}

auto MCMainWindow::Destroy() -> void
{
    ClockPane.reset();
    MCGuiHolderObject::Destroy();
}

auto MCMainWindow::HandleEvent(MCGuiEvent* event) -> void
{
    if (event->Type == 0x12)
    {
        Resize(Application->Width(), Application->Height());
    }

    MCGuiObject::HandleEvent(event);
}

auto MCMainWindow::Display() -> void
{
    if (ShowWindow == 0 || (IsHidden() != 0 && HideOffset == 0))
    {
        return;
    }

    MCGuiHolderObject::Display();

    if (Scenario->TimeLimit == 0 || LastClockTime == ActualTime)
    {
        return;
    }

    // The mission clock, once per time step.
    LastClockTime = ActualTime;
    MCGuiObject* pane = ClockPane.get();
    VfxPaneWipe(pane->Port()->Frame(), pane->BackColor());
    pane->DrawBox(0x1f, -1, -1, -1, -1);
    LineFont->Scaled = 1;
    LineFont->Scale = 1.5f;
    const uint8_t fontHeight = LineHeight(*LineFont);
    const int32_t paneHeight = pane->Height();
    std::string clock;
    int32_t textColor = 0x1f;
    const float timeLeft = static_cast<float>(Scenario->TimeLimit) - ActualTime;

    if (timeLeft < 0.0f)
    {
        clock = "00:00";
        textColor = 0xef;
    }
    else
    {
        const auto minutes = static_cast<int16_t>(static_cast<int32_t>(std::floor(timeLeft * 0.016666668f)));
        const auto seconds =
            static_cast<int16_t>(static_cast<int32_t>(std::floor(std::fmod(static_cast<double>(timeLeft), 60.0))));
        clock = std::format("{:02}:{:02}", minutes, seconds);
    }

    const int32_t textWidth = LineFont->PrintWidth(clock, 0);
    const int32_t x = (pane->Width() - textWidth) / 2 + 1;
    LineFont->Print(x, (paneHeight - fontHeight) / 2 + 2, clock, textColor, pane->Port()->Frame());
}

auto MCMainWindow::Retile() -> void
{
    MCGuiHolderObject::Retile();
    MCGuiObject* pane = GetActivePane();

    if (pane == nullptr)
    {
        return;
    }

    ClockPane->MoveTo(-2 - ClockPane->Width() + pane->Right(), 2, 0);
}

auto MCMainWindow::SetVertical(int on) -> void
{
    Vertical = on;
    Retile();
}

auto MCMainWindow::SetTiled(int tiled) -> void
{
    if (tiled != 0 && GetInactivePane() != nullptr)
    {
        GetInactivePane()->GetCamera()->Activate();
    }

    MCGuiHolderObject::SetTiled(tiled);

    if (tiled == 0 && GetInactivePane() != nullptr)
    {
        GetInactivePane()->GetCamera()->Deactivate();
    }
}

auto MCMainWindow::ZoomActivePane() -> void
{
    if (ActivePane < 0)
    {
        return;
    }

    MCCamera* view = Panes[ActivePane]->GetCamera();

    if (view == nullptr)
    {
        return;
    }

    // Port: between the closest and the furthest zoom, not paused or asked; the camera stays at scale 100 (the
    // original flipped it between 100 and 1, and to 1 while paused or asked).
    if (GamePaused == 0 && GameAsked == 0 && view->View() != nullptr)
    {
        view->View()->ToggleZoom();
    }

    view->ForceUpdate = true;
    MCTerrain::ForceRedraw = true;
}

auto MCMainWindow::SetActivePane(MCGuiObject* pane) -> void
{
    MCGuiHolderObject::SetActivePane(pane);
}
