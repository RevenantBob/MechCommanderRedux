#include "stdafx.h"
#include "camera/MCViewWindow.h"
#include "camera/MCCamera.h"
#include "camera/MCCameraList.h"
#include "camera/MCMainWindow.h"
#include "engine/MCFont.h"
#include "gui/MCGuiPort.h"
#include "gui/MCGuiSystem.h"
#include "iface/MCTacticalInterface.h"
#include "linkup/sessionmanager.h"
#include "logistics/MCConnectMenu.h"
#include "main/MCMissionGlobals.h"
#include "mission/MCScenario.h"
#include "network/multplyr.h"
#include "platform/MCRenderer.h"
#include "terrain/MCTacticalMap.h"
#include "terrain/MCTerrain.h"
#include "vfx/MCVfxFunctions.h"

MCOverlayTarget MCOverlay;
float MCFixedZoomHeight = 0.0f;

namespace
{
    /// <summary>Set while a view window's status bar is on screen.</summary>
    bool StatusBarDrawn = false;
    /// <summary>The corners of the status bar last drawn.</summary>
    int32_t LastBarX0 = 0;
    int32_t LastBarY0 = 0;
    int32_t LastBarX1 = 0;
    int32_t LastBarY1 = 0;

    /// <summary>Port: how much of the way to its target the zoom eases in a millisecond (about 70 ms to settle most of
    /// the way).</summary>
    constexpr double ZoomEaseTime = 70.0;

    /// <summary>
    /// Port: keeps the tactical map's zoom button (pushed when zoomed out, as the original flipped it) in step with
    /// the main view's zoom.
    /// </summary>
    void SyncZoomButton(MCViewWindow* view)
    {
        MCTacticalMap* map = TacticalMap();

        if (map == nullptr || view != MCMainView() || map->ToolButtons[7] == nullptr)
        {
            return;
        }

        if ((map->ToolButtons[7]->Pushed != 0) != view->ZoomedOut())
        {
            map->ToggleZoom();
        }
    }
}

MCViewWindow::~MCViewWindow()
{
    // aTitleWindow's inline destructor.
    MCRenderer::RemoveUnderlay(this);
    MCRenderer::RemoveFrameSurface(&WorldWindow);
    MCGuiTitleWindow::Destroy();
}

auto MCViewWindow::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) -> int32_t
{
    const int32_t result = MCGuiObject::Init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    SelectionBox = {};
    InterfaceWindow = false;
    ObjectType = 5;
    Camera = nullptr;
    LastEvent.Target = nullptr;
    return 0;
}

auto MCViewWindow::Destroy() -> void
{
    MCRenderer::RemoveUnderlay(this);
    MCRenderer::RemoveFrameSurface(&WorldWindow);
    WorldPixels = {};
    WorldWindow = {};
    WorldPane = {};
    MCGuiObject::Destroy();
}

auto MCViewWindow::HandleEvent(MCGuiEvent* event) -> void
{
    LastEvent = *event;
    const int32_t type = event->Type;

    switch (type)
    {
        case 0xd:
        {
            if (GetCamera() != nullptr)
            {
                GetCamera()->Deactivate();
            }

            ScreenWindow()->RemoveChild(this);
            return;
        }
        case 0x12:
        {
            GlobalPane = ScreenPort()->Frame();
            GlobalWindow = ScreenPort()->Frame()->Window;
            break;
        }
        case 0x1a:
        {
            // Port: the zoom toggle goes between the closest and the furthest zoom; the camera stays at scale 100
            // (the original flipped it between 100 and 1, and to 1 while paused or asked).
            if (GetCamera() != nullptr && GamePaused == 0 && GameAsked == 0)
            {
                GetCamera()->ForceUpdate = true;
                MCTerrain::ForceRedraw = true;
                ToggleZoom();
            }
            break;
        }
        case 0x1c:
        {
            if (GetCamera() != nullptr)
            {
                GetCamera()->ChangeTarget(static_cast<MCBaseObject*>(nullptr), false);
            }
            break;
        }
    }

    if (InterfaceWindow && TacticalInterface() != nullptr && type != 9 && type != 8)
    {
        TacticalInterface()->HandleEvent(event);
        MCGuiObject::HandleEvent(event);
        return;
    }

    // A click swaps this view's target (or position) with the active pane's.
    if (type == 1 && MainHolder()->GetActivePane() != nullptr)
    {
        MCCamera* activeCamera = MainHolder()->GetActivePane()->GetCamera();
        MCCamera* thisCamera = GetCamera();

        if (activeCamera != nullptr && thisCamera != nullptr)
        {
            MCVector3D activePosition;
            MCBaseObject* activeTarget = activeCamera->TargetObject;

            if (activeTarget == nullptr)
            {
                activePosition = activeCamera->Position;
            }

            if (thisCamera->TargetObject == nullptr)
            {
                activeCamera->ChangeTarget(static_cast<MCBaseObject*>(nullptr), false);
                activeCamera->SetPosition(thisCamera->GetPosition());
            }
            else
            {
                activeCamera->ChangeTarget(thisCamera->TargetObject, false);
            }

            if (activeTarget != nullptr)
            {
                thisCamera->ChangeTarget(activeTarget, false);
                MCGuiObject::HandleEvent(event);
                return;
            }

            thisCamera->ChangeTarget(static_cast<MCBaseObject*>(nullptr), false);
            thisCamera->SetPosition(activePosition);
        }
    }

    MCGuiObject::HandleEvent(event);
}

auto MCViewWindow::Resize(int32_t w, int32_t h) -> void
{
    if (GridAligned != 0)
    {
        w -= w % 40;

        if (w == 0)
        {
            w = 40;
        }

        h -= h % 40;

        if (h == 0)
        {
            h = 40;
        }
    }

    MCGuiObject::Resize(w, h);

    if (GetCamera() == nullptr)
    {
        return;
    }

    if (!LeaveSwoopyOff)
    {
        GetCamera()->Swoopy = Width() >= 250 && Height() >= 250;
    }

    MCTerrain::ForceRedraw = true;
    // Port: the camera's view is the world surface, which keeps the zoom and takes the new aspect.
    UpdateWorldSurface();
    Camera->SetViewSize(static_cast<float>(WorldWidth()), static_cast<float>(WorldHeight()));
}

auto MCViewWindow::Display() -> void
{
    if (ShowWindow == 0 || (IsHidden() != 0 && HideOffset == 0))
    {
        // Port: a view not shown shows no world.
        MCRenderer::RemoveUnderlay(this);
        return;
    }

    // Port: the world shows through the view's rectangle of the screen, from its surface (kept from the last frame
    // drawn when the scenario no longer renders). The zoom eased in MCCamera::Update.
    StartZoom();
    ZoomShown = true;
    UpdateWorldSurface();
    const MCPane* shown = Frame();
    MCRenderer::SetUnderlay(
        MCUnderlay{this, shown->Window, MCRect{shown->X0, shown->Y0, shown->X1, shown->Y1}, &WorldWindow});
    VfxPaneWipe(Frame(), MCRenderer::UnderlayKey);

    if (Scenario() != nullptr && (ScenarioEndTurn == -1 || Turn < ScenarioEndTurn))
    {
        Scenario()->Render(this);

        if (GetCamera()->CameraId == 1 && MPlayer != nullptr && MPlayer->SessionManager != nullptr &&
            DisplayProfileData == 2)
        {
            char stats[256];

            if (MPlayer->SessionManager->GetStats(stats) == 0)
            {
                LineFont()->Scaled = 0;
                LineFont()->Scale = 1.0f;
                LineFont()->Print(180, 72, stats, 0xfe, GlobalPane);
            }
        }
    }

    if (WinState != MCGuiWindowState::Iconized)
    {
        DisplayChildren();
    }

    if (SelectionBox[2] == 0.0f && SelectionBox[3] == 0.0f)
    {
        // Erase the last bar.
        if (StatusBarDrawn)
        {
            StatusBarDrawn = false;
            AGStatusBar(GlobalPane, LastBarX0, LastBarY0, LastBarX1, LastBarY1, 0x10a, LastBarX1 - LastBarX0);
        }
    }
    else
    {
        const auto y1 = static_cast<int32_t>(SelectionBox[3]);
        const auto x1 = static_cast<int32_t>(SelectionBox[2]);
        const auto y0 = static_cast<int32_t>(SelectionBox[1]);
        const auto x0 = static_cast<int32_t>(SelectionBox[0]);
        const auto length = static_cast<int32_t>(SelectionBox[2] - SelectionBox[0]);
        AGStatusBar(GlobalPane, x0, y0, x1, y1, 0x109, length);
        LastBarX0 = x0;
        LastBarY0 = y0;
        LastBarX1 = x1;
        LastBarY1 = y1;
        StatusBarDrawn = true;
    }

    DrawBox(0x10, -1, -1, -1, -1);
}

auto MCViewWindow::Leave() -> void
{
    TacticalInterface()->HideTags();
    GuiSystem()->SetCurrentCursor(static_cast<MCCursorType>(0));
}

auto MCViewWindow::DrawBox(uint8_t color, int32_t left, int32_t top, int32_t right, int32_t bottom) -> void
{
    if (left == -1)
    {
        left = 0;
    }

    if (top == -1)
    {
        top = 0;
    }

    if (right == -1)
    {
        right = Width() - 1;
    }

    if (bottom == -1)
    {
        bottom = Height() - 1;
    }

    VfxLineDraw(Frame(), left, top, right, top, color);
    VfxLineDraw(Frame(), left, top, left, bottom, color);
    VfxLineDraw(Frame(), left, bottom, right, bottom, color);
    VfxLineDraw(Frame(), right, top, right, bottom, color);
}

auto MCViewWindow::SetWindowCamera(MCCamera* newCamera) -> void
{
    Camera = newCamera;

    if (newCamera == nullptr)
    {
        return;
    }

    if (newCamera->CameraId == 1)
    {
        SetBackColor(0xef);
    }
    else if (newCamera->CameraId == 2)
    {
        SetBackColor(0xf8);
    }
}

auto MCViewWindow::WorldFrame() -> MCPane*
{
    UpdateWorldSurface();
    return &WorldPane;
}

auto MCViewWindow::ZoomLimits(float& closest, float& furthest) -> void
{
    closest = ZoomClosest;
    furthest = ZoomFurthest;

    if (MCFixedZoomHeight > 0.0f)
    {
        closest = MCFixedZoomHeight;
        furthest = MCFixedZoomHeight;
        return;
    }

    // The terrain grid covers a surface as far as MCTerrainGridReach (width / cos + height / sin of the view angle):
    // a very wide view zooms out less.
    if (MCTerrainGridReach > 0.0 && Width() > 0 && Height() > 0)
    {
        const double aspect = static_cast<double>(Width()) / static_cast<double>(Height());
        const double reach =
            MCTerrainGridReach / (aspect / std::cos(MCTerrainViewAngle) + 1.0 / std::sin(MCTerrainViewAngle));
        furthest = static_cast<float>(std::min(static_cast<double>(furthest), reach));
        closest = std::min(closest, furthest);
    }
}

auto MCViewWindow::ZoomInHeight() -> float
{
    float closest = 0.0f;
    float furthest = 0.0f;
    ZoomLimits(closest, furthest);
    const float oneToOne = std::clamp(static_cast<float>(Height()), closest, furthest);
    return oneToOne < furthest ? oneToOne : closest;
}

auto MCViewWindow::StartZoom() -> void
{
    if (ZoomHeight > 0.0f)
    {
        return;
    }

    // A view starts at one world pixel per screen pixel where it can, or zoomed out when its camera started at scale
    // 1 (by then its size is the one the main window gave it).
    float closest = 0.0f;
    float furthest = 0.0f;
    ZoomLimits(closest, furthest);
    ZoomHeight = ZoomStartsOut ? furthest : std::clamp(static_cast<float>(Height()), closest, furthest);
    ZoomTarget = ZoomHeight;
    ZoomClock = MCPort::Milliseconds();
}

auto MCViewWindow::UpdateWorldSurface() -> void
{
    float closest = 0.0f;
    float furthest = 0.0f;
    ZoomLimits(closest, furthest);
    float shownHeight = ZoomStartsOut ? furthest : static_cast<float>(Height());

    if (ZoomHeight > 0.0f)
    {
        ZoomHeight = std::clamp(ZoomHeight, closest, furthest);
        ZoomTarget = std::clamp(ZoomTarget, closest, furthest);
        shownHeight = ZoomHeight;
    }

    shownHeight = std::clamp(shownHeight, closest, furthest);
    const int32_t surfaceHeight = std::max(1, static_cast<int32_t>(std::lround(shownHeight)));
    const int32_t surfaceWidth = std::max(
        1, static_cast<int32_t>(std::lround(static_cast<double>(Width()) * surfaceHeight / std::max(1, Height()))));

    if (WorldPixels.empty() || surfaceWidth != WorldWidth() || surfaceHeight != WorldHeight())
    {
        WorldPixels.assign(static_cast<size_t>(surfaceWidth) * static_cast<size_t>(surfaceHeight), 0);
        WorldWindow.Buffer = WorldPixels.data();
        WorldWindow.XMax = surfaceWidth - 1;
        WorldWindow.YMax = surfaceHeight - 1;
        WorldPane.Window = &WorldWindow;
        WorldPane.X0 = 0;
        WorldPane.Y0 = 0;
        WorldPane.X1 = surfaceWidth - 1;
        WorldPane.Y1 = surfaceHeight - 1;
        MCTerrain::ForceRedraw = true;
        MCRenderer::AddFrameSurface(&WorldWindow);
    }
}

auto MCViewWindow::ZoomTo(float height) -> bool
{
    float closest = 0.0f;
    float furthest = 0.0f;
    ZoomLimits(closest, furthest);
    StartZoom();
    const float target = std::clamp(height, closest, furthest);

    if (std::fabs(target - ZoomTarget) < 0.5f)
    {
        return false;
    }

    if (ZoomHeight == ZoomTarget)
    {
        ZoomClock = MCPort::Milliseconds();
    }

    ZoomTarget = target;

    // Before the view is first shown (the mission start's zoom toggle), the zoom takes effect at once, as the
    // original's camera scale switched.
    if (!ZoomShown)
    {
        ZoomHeight = target;
    }

    SyncZoomButton(this);
    return true;
}

auto MCViewWindow::ZoomBy(float factor) -> bool
{
    StartZoom();
    return ZoomTo(ZoomTarget * factor);
}

auto MCViewWindow::ZoomedOut() -> bool
{
    StartZoom();
    float closest = 0.0f;
    float furthest = 0.0f;
    ZoomLimits(closest, furthest);
    return ZoomTarget > (ZoomInHeight() + furthest) * 0.5f;
}

auto MCViewWindow::ToggleZoom() -> void
{
    float closest = 0.0f;
    float furthest = 0.0f;
    ZoomLimits(closest, furthest);
    ZoomTo(ZoomedOut() ? ZoomInHeight() : furthest);
}

auto MCViewWindow::EaseZoom() -> void
{
    StartZoom();
    const uint32_t now = MCPort::Milliseconds();
    const auto elapsed = static_cast<double>(std::min<uint32_t>(now - ZoomClock, 100));
    ZoomClock = now;

    if (ZoomHeight == ZoomTarget)
    {
        return;
    }

    ZoomHeight += static_cast<float>((ZoomTarget - ZoomHeight) * (1.0 - std::exp(-elapsed / ZoomEaseTime)));

    if (std::fabs(ZoomTarget - ZoomHeight) < 0.5f)
    {
        ZoomHeight = ZoomTarget;
    }
}

auto MCViewWindow::WorldScaleX() -> float
{
    if (WorldPixels.empty())
    {
        UpdateWorldSurface();
    }

    return static_cast<float>(Width()) / static_cast<float>(WorldWidth());
}

auto MCViewWindow::WorldScaleY() -> float
{
    if (WorldPixels.empty())
    {
        UpdateWorldSurface();
    }

    return static_cast<float>(Height()) / static_cast<float>(WorldHeight());
}

auto MCViewWindow::ScreenToWorld(int32_t screenX, int32_t screenY) -> MCVector2D
{
    if (WorldPixels.empty())
    {
        UpdateWorldSurface();
    }

    // The world pixel shown at the screen pixel's centre, as the display samples it.
    const float x = (static_cast<float>(screenX - GlobalX()) + 0.5f) / WorldScaleX();
    const float y = (static_cast<float>(screenY - GlobalY()) + 0.5f) / WorldScaleY();
    return MCVector2D(std::floor(x), std::floor(y));
}

auto MCViewWindow::WorldToWindow(MCVector2D point) -> MCVector2D
{
    if (WorldPixels.empty())
    {
        UpdateWorldSurface();
    }

    return MCVector2D(point.X * WorldScaleX(), point.Y * WorldScaleY());
}

auto MCViewWindow::WorldToScreen(MCVector2D point) -> MCVector2D
{
    const MCVector2D window = WorldToWindow(point);
    return MCVector2D(window.X + static_cast<float>(GlobalX()), window.Y + static_cast<float>(GlobalY()));
}

auto MCIsOverlayDepth(float depth) -> bool
{
    return depth == -50000.0f || depth == -40000.0f;
}

auto MCOverlayPoint(MCVector2D point) -> MCVector2D
{
    return MCVector2D(MCOverlayX(point.X), MCOverlayY(point.Y));
}

auto MCOverlayX(float x) -> float
{
    return x * MCOverlay.ScaleX;
}

auto MCOverlayY(float y) -> float
{
    return y * MCOverlay.ScaleY;
}

auto MCMainView() -> MCViewWindow*
{
    if (CameraList() == nullptr)
    {
        return nullptr;
    }

    MCCamera* main = CameraList()->FindCameraFromIDNumber(1);
    return main != nullptr ? main->View() : nullptr;
}

auto MCWindowPoint(MCGuiObject* window, int32_t screenX, int32_t screenY) -> MCVector2D
{
    MCCamera* camera = window->GetCamera();

    if (camera != nullptr && camera->View() == window)
    {
        return camera->View()->ScreenToWorld(screenX, screenY);
    }

    return MCVector2D(static_cast<float>(screenX - window->GlobalX()), static_cast<float>(screenY - window->GlobalY()));
}
