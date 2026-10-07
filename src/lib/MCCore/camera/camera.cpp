#include "stdafx.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "color/MCPalette.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCCraterManager.h"
#include "engine/MCFont.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "gui/awindow.h"
#include "iface/iface.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "linkup/sessionmanager.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/mech.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/objtype.h"
#include "object/team.h"
#include "platform/MCRenderer.h"
#include "sprite/sprtmgr.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "terrain/vertex.h"
#include "vfx/MCVfxFunctions.h"

MCOverlayTarget MCOverlay;
float MCFixedZoomHeight = 0.0f;
MCCamera* Eye = nullptr;
std::vector<uint8_t> ScaleTable;
MCMainWindow* MainHolder = nullptr;
int32_t LeaveSwoopyOff = 0;
int32_t DrawCameraCircle = 0;
uint8_t* PauseShape = nullptr;
float CurrentScaleFactor = 0.0f;
int32_t LastZoom = 0;
MCCameraList* CameraList = nullptr;
MCPane* GlobalPane = nullptr;
MCWindow* GlobalWindow = nullptr;
uint8_t* AskedShape = nullptr;

namespace
{
    /// <summary>Set while a view window's status bar is on screen.</summary>
    int32_t StatusBarDrawn = 0;
    /// <summary>The corners of the status bar last drawn.</summary>
    int32_t LastBarX0 = 0;
    int32_t LastBarY0 = 0;
    int32_t LastBarX1 = 0;
    int32_t LastBarY1 = 0;

    /// <summary>The divisors of the 7 rows of <see cref="ScaleTable"/> (0x00793edc).</summary>
    constexpr int32_t ScaleDivisors[7] = {9, 4, 3, 2, 1, -2, -4};

    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;
    /// <summary>60 degrees in radians, as MCX.EXE stores it (0x0077cd28).</summary>
    constexpr double SIXTY_DEGREES = 0x1.0c152382d45b2p+0;

    /// <summary>A font's line height: its height, scaled and rounded down when the font is scaled (an inline of the
    /// original's font.h).</summary>
    auto LineHeight(MCFont* font) -> uint8_t
    {
        uint8_t height = font->FontHeight;

        if (font->Scaled != 0)
        {
            height = static_cast<uint8_t>(static_cast<int32_t>(std::floor(static_cast<float>(height) * font->Scale)));
        }

        return height;
    }

    /// <summary>The view's size and the projection's sine and cosine (the same lines in Camera::init, render and
    /// viewWindow::resize).</summary>
    auto SetViewSize(MCCamera* camera, float width, float height) -> void
    {
        camera->ViewWidth = width;
        camera->ViewHeight = height;
        const double angle = static_cast<double>(camera->ProjectionAngle) * DEGREES_TO_RADIANS;
        camera->SinAngle = static_cast<float>(std::sin(angle));
        camera->CosAngle = static_cast<float>(std::cos(angle));
        camera->HalfWidth = camera->ViewWidth * 0.5f;
        camera->HalfHeight = camera->ViewHeight * 0.5f;
    }

    /// <summary>Loads a shape file of artPath into the object cache (the pause and asked shapes).</summary>
    auto LoadShape(const char* name, const char* errorMessage) -> uint8_t*
    {
        std::string shapeName;
        shapeName = GamePath(ArtPath, name, ".shp");
        MCFile shapeFile;
        const int32_t result = shapeFile.Open(shapeName);
        Assert(result == 0, static_cast<uint32_t>(result), errorMessage);
        const uint32_t size = shapeFile.FileSize();
        auto* shape = static_cast<uint8_t*>(MCObjectTypeManager::ObjectCache.Allocate(size));
        shapeFile.Read(shape, static_cast<int32_t>(size));
        shapeFile.Close();
        MCRenderer::RegisterData(shape, size, MCDataKind::Shapes);
        return shape;
    }

    /// <summary>Port: how much of the way to its target the zoom eases in a millisecond (about 70 ms to settle most of
    /// the way).</summary>
    constexpr double ZOOM_EASE_TIME = 70.0;

    /// <summary>
    /// Port: keeps the tactical map's zoom button (pushed when zoomed out, as the original flipped it) in step with
    /// the main view's zoom.
    /// </summary>
    auto SyncZoomButton(MCViewWindow* view) -> void
    {
        MCTacticalMap* map = MCTerrain::TerrainTacticalMap;

        if (map == nullptr || view != MCMainView() || map->ToolButtons[7] == nullptr)
        {
            return;
        }

        if ((map->ToolButtons[7]->Pushed != 0) != view->ZoomedOut())
        {
            map->ToggleZoom();
        }
    }

    /// <summary>Darkens the whole screen for the pause and asked overlays.</summary>
    auto DarkenScreen() -> void
    {
        uint8_t* hazePalette = GamePalette()->GetHazePalette(-7);
        MCScreenVertex vertices[4] = {};
        vertices[1].X = Application->Width() - 1;
        vertices[2].X = Application->Width() - 1;
        vertices[2].Y = Application->Height() - 1;
        vertices[3].Y = Application->Height() - 1;
        VfxTranslatePolygon(ScreenPort->Frame(), std::span(vertices, 4), hazePalette);
    }
}

//---------------------------------------------------------------------------
// viewWindow
//---------------------------------------------------------------------------

MCViewWindow::~MCViewWindow()
{
    // aTitleWindow's inline destructor.
    MCRenderer::RemoveUnderlay(this);
    MCRenderer::RemoveFrameSurface(&WorldWindow);
    MCGuiTitleWindow::Destroy();
}

auto MCViewWindow::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    const int32_t result = MCGuiObject::Init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    SelectionBox[1] = 0.0f;
    SelectionBox[0] = 0.0f;
    InterfaceWindow = 0;
    ObjectType = 5;
    Camera = nullptr;
    SelectionBox[3] = 0.0f;
    SelectionBox[2] = 0.0f;
    LastEvent.Target = nullptr;
    return 0;
}

auto MCViewWindow::Destroy() -> void
{
    if (GetCamera() != nullptr)
    {
        CameraList->Remove(GetCamera());
    }

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

            ScreenWindow->RemoveChild(this);
            return;
        }
        case 0x12:
        {
            GlobalPane = ScreenPort->Frame();
            GlobalWindow = ScreenPort->Frame()->Window;
            break;
        }
        case 0x1a:
        {
            // Port: the zoom toggle goes between the closest and the furthest zoom; the camera stays at scale 100
            // (the original flipped it between 100 and 1, and to 1 while paused or asked).
            if (GetCamera() != nullptr && GamePaused == 0 && GameAsked == 0)
            {
                GetCamera()->ForceUpdate = 1;
                MCTerrain::ForceRedraw = 1;
                ToggleZoom();
            }
            break;
        }
        case 0x1c:
        {
            if (GetCamera() != nullptr)
            {
                GetCamera()->ChangeTarget(static_cast<MCBaseObject*>(nullptr), 0);
            }
            break;
        }
    }

    if (InterfaceWindow != 0 && TheInterface != nullptr && type != 9 && type != 8)
    {
        TheInterface->HandleEvent(event);
        MCGuiObject::HandleEvent(event);
        return;
    }

    // A click swaps this view's target (or position) with the active pane's.
    if (type == 1 && MainHolder->GetActivePane() != nullptr)
    {
        MCCamera* activeCamera = MainHolder->GetActivePane()->GetCamera();
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
                activeCamera->ChangeTarget(static_cast<MCBaseObject*>(nullptr), 0);
                activeCamera->SetPosition(thisCamera->GetPosition());
            }
            else
            {
                activeCamera->ChangeTarget(thisCamera->TargetObject, 0);
            }

            if (activeTarget != nullptr)
            {
                thisCamera->ChangeTarget(activeTarget, 0);
                MCGuiObject::HandleEvent(event);
                return;
            }

            thisCamera->ChangeTarget(static_cast<MCBaseObject*>(nullptr), 0);
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

    if (LeaveSwoopyOff == 0)
    {
        if (Width() < 250 || Height() < 250)
        {
            GetCamera()->Swoopy = 0;
        }
        else
        {
            GetCamera()->Swoopy = 1;
        }
    }

    MCTerrain::ForceRedraw = 1;
    // Port: the camera's view is the world surface, which keeps the zoom and takes the new aspect.
    UpdateWorldSurface();
    SetViewSize(Camera, static_cast<float>(WorldWidth()), static_cast<float>(WorldHeight()));
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
    // drawn when the scenario no longer renders). The zoom eased in Camera::update.
    StartZoom();
    ZoomShown = true;
    UpdateWorldSurface();
    const MCPane* shown = Frame();
    MCRenderer::SetUnderlay(
        MCUnderlay{this, shown->Window, MCRect{shown->X0, shown->Y0, shown->X1, shown->Y1}, &WorldWindow});
    VfxPaneWipe(Frame(), MCRenderer::UnderlayKey);

    if (Scenario != nullptr && (ScenarioEndTurn == -1 || Turn < ScenarioEndTurn))
    {
        Scenario->Render(this);

        if (GetCamera()->CameraId == 1 && MPlayer != nullptr && MPlayer->SessionManager != nullptr &&
            DisplayProfileData == 2)
        {
            char stats[256];

            if (MPlayer->SessionManager->GetStats(stats) == 0)
            {
                LineFont->Scaled = 0;
                LineFont->Scale = 1.0f;
                LineFont->Print(180, 72, stats, 0xfe, GlobalPane);
            }
        }
    }

    if (WinState != aSTATE_ICONIZED)
    {
        for (int32_t i = 0; i < NumChildren; i++)
        {
            ChildList[i]->Display();
        }
    }

    if (SelectionBox[2] == 0.0f && SelectionBox[3] == 0.0f)
    {
        // Erase the last bar.
        if (StatusBarDrawn != 0)
        {
            StatusBarDrawn = 0;
            AGStatusBar(GlobalPane, LastBarX0, LastBarY0, LastBarX1, LastBarY1, 0x10a, LastBarX1 - LastBarX0);
        }
    }
    else
    {
        const int32_t y1 = static_cast<int32_t>(SelectionBox[3]);
        const int32_t x1 = static_cast<int32_t>(SelectionBox[2]);
        const int32_t y0 = static_cast<int32_t>(SelectionBox[1]);
        const int32_t x0 = static_cast<int32_t>(SelectionBox[0]);
        const int32_t length = static_cast<int32_t>(SelectionBox[2] - SelectionBox[0]);
        AGStatusBar(GlobalPane, x0, y0, x1, y1, 0x109, length);
        LastBarX0 = x0;
        LastBarY0 = y0;
        LastBarX1 = x1;
        LastBarY1 = y1;
        StatusBarDrawn = 1;
    }

    DrawBox(0x10, -1, -1, -1, -1);
}

auto MCViewWindow::Leave() -> void
{
    TheInterface->HideTags();
    Application->SetCurrentCursor(static_cast<MCCursorType>(0));
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
        MCTerrain::ForceRedraw = 1;
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

    ZoomHeight += static_cast<float>((ZoomTarget - ZoomHeight) * (1.0 - std::exp(-elapsed / ZOOM_EASE_TIME)));

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
    if (CameraList == nullptr)
    {
        return nullptr;
    }

    MCCamera* main = CameraList->FindCameraFromIDNumber(1);
    return main != nullptr ? main->Window : nullptr;
}

auto MCWindowPoint(MCGuiObject* window, int32_t screenX, int32_t screenY) -> MCVector2D
{
    MCCamera* camera = window->GetCamera();

    if (camera != nullptr && camera->Window == window)
    {
        return camera->Window->ScreenToWorld(screenX, screenY);
    }

    return MCVector2D(static_cast<float>(screenX - window->GlobalX()), static_cast<float>(screenY - window->GlobalY()));
}

auto ToggleZoom() -> void
{
    MainHolder->ZoomActivePane();
}

//---------------------------------------------------------------------------
// aMainWindow
//---------------------------------------------------------------------------

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

    ClockPane = new MCGuiObject;

    if (ClockPane == nullptr)
    {
        Fatal(0, "Not enough memory to allocate clock.");
    }

    LineFont->Scale = 1.5f;
    LineFont->Scaled = 1;
    int32_t clockHeight;

    if (WhiteFont == nullptr)
    {
        clockHeight = 24;
    }
    else
    {
        clockHeight = LineHeight(LineFont.get()) + 4;
    }

    ClockPane->Init(0, 0, 40, clockHeight, nullptr);
    ClockPane->SetBackColor(0x10);
    Retile();
    SetDepth(-50);
    return 0;
}

auto MCMainWindow::Destroy() -> void
{
    if (ClockPane != nullptr)
    {
        ClockPane->Destroy();
        delete ClockPane;
        ClockPane = nullptr;
    }

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
    if (ShowWindow == 0)
    {
        return;
    }

    if (IsHidden() != 0 && HideOffset == 0)
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
    MCGuiObject* pane = ClockPane;
    const int32_t color = pane->BackColor();
    VfxPaneWipe(pane->Port()->Frame(), color);
    pane->DrawBox(0x1f, -1, -1, -1, -1);
    LineFont->Scaled = 1;
    LineFont->Scale = 1.5f;
    const uint8_t fontHeight = LineHeight(LineFont.get());
    const int32_t paneHeight = pane->Height();

    char clock[12];
    int32_t textColor;
    const float timeLeft = static_cast<float>(Scenario->TimeLimit) - ActualTime;

    if (timeLeft < 0.0f)
    {
        sprintf(clock, "00:00");
        textColor = 0xef;
    }
    else
    {
        const auto minutes = static_cast<int16_t>(static_cast<int32_t>(std::floor(timeLeft * 0.016666668f)));
        const auto seconds =
            static_cast<int16_t>(static_cast<int32_t>(std::floor(std::fmod(static_cast<double>(timeLeft), 60.0))));
        sprintf(clock, "%02i:%02i", static_cast<int>(minutes), static_cast<int>(seconds));
        textColor = 0x1f;
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

    const int32_t clockWidth = ClockPane->Width();
    ClockPane->MoveTo(-2 - clockWidth + pane->Right(), 2, 0);
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
    if (GamePaused == 0 && GameAsked == 0 && view->Window != nullptr)
    {
        view->Window->ToggleZoom();
    }

    view->ForceUpdate = 1;
    MCTerrain::ForceRedraw = 1;
}

auto MCMainWindow::SetActivePane(MCGuiObject* pane) -> void
{
    MCGuiHolderObject::SetActivePane(pane);
    GetActivePane();
}

//---------------------------------------------------------------------------
// Camera
//---------------------------------------------------------------------------

auto MCCamera::GetScaleFactor() -> float
{
    if (CameraScale != 1)
    {
        return 1.0f;
    }

    return 0.5f;
}

auto MCCamera::GetPosition() -> MCVector3D
{
    return Position;
}

auto MCCamera::BuildScaleTable() -> int32_t
{
    ScaleTable.assign(0x700, 0);
    uint8_t* entry = ScaleTable.data();

    for (const int32_t divisor : ScaleDivisors)
    {
        for (int32_t i = 0; i < 256; i++)
        {
            *entry++ = static_cast<uint8_t>(i / (divisor + 1));
        }
    }

    return 0;
}

auto MCCamera::Init(MCFitIniFile* cameraFile, int objectCamera, int32_t newCameraId) -> int32_t
{
    int32_t result;

    if (ScaleTable.empty() && (result = BuildScaleTable()) != 0)
    {
        return result;
    }

    if ((result = cameraFile->ReadIdFloat("PixelScalar", PixelScalar)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->ReadIdFloat("ProjectionAngle", ProjectionAngle)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->ReadIdFloat("PositionX", Position.X)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->ReadIdFloat("PositionY", Position.Y)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->ReadIdFloat("PositionZ", Position.Z)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->ReadIdUChar("BackgroundColor", BackgroundColor)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->ReadIdUChar("Ready", Ready)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->ReadIdLong("HazeLevel", HazeLevel)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->ReadIdLong("HazeInc", HazeInc)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->ReadIdLong("CameraScale", CameraScale)) != 0)
    {
        return result;
    }

    // Port: always the full-size (1x) art and layout; the zoom is the world surface's size (viewWindow), which starts
    // zoomed out where the camera started at scale 1.
    const bool startsZoomedOut = CameraScale == 1;
    CameraScale = 100;
    uint32_t windowLeft = 0;
    result = cameraFile->ReadIdULong("WindowLeft", windowLeft);

    if (result == 0)
    {
        uint32_t windowTop = 0;
        uint32_t windowRight = 0;
        uint32_t windowBottom = 0;

        if ((result = cameraFile->ReadIdULong("WindowTop", windowTop)) != 0)
        {
            return result;
        }

        if ((result = cameraFile->ReadIdULong("WindowRight", windowRight)) != 0)
        {
            return result;
        }

        if ((result = cameraFile->ReadIdULong("WindowBottom", windowBottom)) != 0)
        {
            return result;
        }

        int32_t isMainWindow = 0;

        if (cameraFile->ReadIdLong("MainWindow", isMainWindow) == 0)
        {
            MainWindow = isMainWindow;
        }
        else
        {
            MainWindow = 0;
        }

        auto* view = new MCViewWindow;
        Window = view;

        if (view == nullptr)
        {
            return -0x303fffe;
        }

        const auto paneWidth = static_cast<int32_t>(windowRight - windowLeft);
        const auto paneHeight = static_cast<int32_t>(windowBottom - windowTop);
        MCGuiHolderObject* holder = nullptr;

        if (MainWindow == 0)
        {
            // Its own titled window holding the view.
            auto* titleWindow = new MCGuiEmptyTitleWindow;

            if (titleWindow == nullptr)
            {
                return -0x303fffe;
            }

            if ((result = titleWindow->Init(static_cast<int32_t>(windowLeft), static_cast<int32_t>(windowTop),
                                            paneWidth, paneHeight, nullptr)) != 0)
            {
                return result;
            }

            if (titleWindow->TitleBar != nullptr)
            {
                titleWindow->TitleBar->ShowZoomButton(1);
            }

            if (titleWindow->ResizeButton != nullptr)
            {
                titleWindow->ResizeButton->ShowGuiWindow(1);
            }

            if (titleWindow->TitleBar != nullptr)
            {
                titleWindow->TitleBar->ShowCloseButton(1);
            }

            holder = titleWindow;
        }
        else if (MainHolder == nullptr)
        {
            MainHolder = new MCMainWindow;

            if (MainHolder == nullptr)
            {
                return -0x303fffe;
            }

            if ((result = MainHolder->Init()) != 0)
            {
                return result;
            }
        }

        if ((result = view->Init(static_cast<int32_t>(windowLeft), static_cast<int32_t>(windowTop), paneWidth,
                                 paneHeight, nullptr)) != 0)
        {
            return result;
        }

        view->ObjectType = 4;

        if (MainWindow == 0)
        {
            holder->AddPane(view);
            view->MoveTo(0, 0, 0);
        }
        else
        {
            if (Ready != 0)
            {
                MainHolder->AddPane(view);
            }

            view->InterfaceWindow = 1;
            view->ObjectType = 4;
        }

        CameraId = newCameraId;
        view->SetWindowCamera(this);
        // Port: the view is the world surface.
        view->ZoomStartsOut = startsZoomedOut;
        view->UpdateWorldSurface();
        SetViewSize(this, static_cast<float>(view->WorldWidth()), static_cast<float>(view->WorldHeight()));
    }
    else
    {
        // No window: the camera covers the global pane.
        Window = nullptr;
        SetViewSize(this, static_cast<float>(GlobalPane->X1 - GlobalPane->X0),
                    static_cast<float>(GlobalPane->Y1 - GlobalPane->Y0));
    }

    if (objectCamera == 0)
    {
        CameraClass = POSITION_CAMERA;
        SwoopDone = 0;
        return 0;
    }

    if ((result = cameraFile->ReadIdLong("ObjectClassId", ObjectClassId)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->ReadIdLong("partNumber", PartNumber)) != 0)
    {
        return result;
    }

    if (cameraFile->ReadIdLong("SwoopyCamOff", LeaveSwoopyOff) != 0)
    {
        LeaveSwoopyOff = 0;
    }

    if (cameraFile->ReadIdLong("ScrollyCam", ScrollyCam) != 0)
    {
        ScrollyCam = 0;
    }

    if (cameraFile->ReadIdFloat("DistanceThreshold", DistanceThreshold) != 0)
    {
        DistanceThreshold = 10.0f;
    }

    // Original behaviour (OB-034): a missing MinScrollSpeed sets DistanceThreshold to 90 instead.
    if (cameraFile->ReadIdFloat("MinScrollSpeed", MinScrollSpeed) != 0)
    {
        DistanceThreshold = 90.0f;
    }

    if (cameraFile->ReadIdFloat("SpeedFactor", SpeedFactor) != 0)
    {
        SpeedFactor = 50.0f;
    }

    if (cameraFile->ReadIdFloat("CamDistance", CamDistance) != 0)
    {
        CamDistance = 50.0f;
    }

    if (cameraFile->ReadIdFloat("DistanceFactor", DistanceFactor) != 0)
    {
        DistanceFactor = 25.0f;
    }

    if (cameraFile->ReadIdFloat("CamSpeed", CamSpeed) != 0)
    {
        CamSpeed = 50.0f;
    }

    if (cameraFile->ReadIdFloat("JumpThreshold", JumpThreshold) != 0)
    {
        JumpThreshold = 250.0f;
    }

    CameraClass = OBJECT_CAMERA;
    SwoopDone = 0;
    return 0;
}

auto MCCamera::Init(MCCamData* data, int objectCamera) -> int32_t
{
    int32_t result;

    if (ScaleTable.empty() && (result = BuildScaleTable()) != 0)
    {
        return result;
    }

    PixelScalar = data->PixelScalar;
    Position.Y = data->Position[1];
    Ready = data->Ready;
    ProjectionAngle = data->ProjectionAngle;
    Position.X = data->Position[0];
    Position.Z = data->Position[2];
    BackgroundColor = data->BackgroundColor;
    HazeLevel = data->HazeLevel;
    // Port: always the full-size (1x) art and layout; the zoom is the world surface's size (viewWindow), which starts
    // zoomed out where the camera started at scale 1.
    const bool startsZoomedOut = data->CameraScale == 1;
    CameraScale = 100;
    const auto windowBottom =
        static_cast<int32_t>(static_cast<double>(data->WindowTop) + static_cast<double>(data->WindowHeight));
    const auto windowRight =
        static_cast<int32_t>(static_cast<double>(data->WindowLeft) + static_cast<double>(data->WindowWidth));

    auto* view = new MCViewWindow;
    const auto paneWidth = static_cast<uint32_t>(windowRight - static_cast<int32_t>(data->WindowLeft));
    const auto paneHeight = static_cast<uint32_t>(windowBottom - static_cast<int32_t>(data->WindowTop));
    Window = view;

    if ((result = view->Init(static_cast<int32_t>(data->WindowLeft), static_cast<int32_t>(data->WindowTop),
                             static_cast<int32_t>(paneWidth), static_cast<int32_t>(paneHeight), nullptr)) != 0)
    {
        return result;
    }

    view->SetWindowCamera(this);

    if (LeaveSwoopyOff == 0)
    {
        if (Window->Width() < 150 || Window->Height() < 150)
        {
            Swoopy = 0;
        }
        else
        {
            Swoopy = 1;
        }
    }

    // Port: the view is the world surface.
    view->ZoomStartsOut = startsZoomedOut;
    view->UpdateWorldSurface();
    SetViewSize(this, static_cast<float>(view->WorldWidth()), static_cast<float>(view->WorldHeight()));
    TerrainWindow = Land->NewWindow(this);

    if (TerrainWindow == nullptr)
    {
        return 0;
    }

    if (objectCamera == 0)
    {
        CameraClass = POSITION_CAMERA;
    }
    else
    {
        PartNumber = data->PartNumber;
        ObjectClassId = data->ObjectClassId;
        CameraClass = OBJECT_CAMERA;
        // Follow the last mover of the home side that has a network player.
        MCObjectQueueNode* mechList = HomeTeam->Alignment == 1 ? InnerSphereMechList : ClanMechList;
        MCBaseObject* current = nullptr;

        while (mechList->Traverse(current) != nullptr)
        {
            const MCObjectClass objectClass = current->ObjectClass;

            if ((objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
                 objectClass == MOVER) &&
                static_cast<MCMover*>(current)->NetPlayerId != -1)
            {
                PartNumber = current->PartId;
            }
        }
    }

    SwoopDone = 0;
    return 0;
}

auto MCCamera::Init() -> void
{
    PixelScalar = 1.0f;
    ViewWidth = 400.0f;
    ViewHeight = 400.0f;
    Position.Y = 0.0f;
    Position.X = 0.0f;
    ProjectionAngle = 30.0f;
    ScreenUL.Y = 0.0f;
    ScreenUL.X = 0.0f;
    CamSpeed = 50.0f;
    CamDistance = 50.0f;
    Position.Z = 0.0f;
    ScreenUL50.Y = 0.0f;
    ScreenUL50.X = 0.0f;
    LastTargetFacing.Z = 0.0f;
    LastTargetFacing.Y = 0.0f;
    LastTargetFacing.X = 0.0f;
    Active = 0;
    Ready = 0;
    CameraClass = NO_CAMERA;
    HazeLevel = 4;
    HazeInc = -2;
    Window = nullptr;
    CameraScale = 100;
    TargetObject = nullptr;
    DefaultTarget = nullptr;
    TargetChanged = 1;
    DistanceFactor = 25.0f;
    TerrainWindow = nullptr;
    Swoopy = 0;
    SwoopDone = 0;
    ScrollyCam = 0;
    Scrolling = 0;
    SpeedFactor = 10.0f;
    DistanceThreshold = 10.0f;
    MinScrollSpeed = 90.0f;
    ForceUpdate = 0;
    ScrollJumped = 0;
    JumpThreshold = 250.0f;
}

auto MCCamera::PrepareBackground() -> void
{
}

auto MCCamera::InverseProject(MCVector2D& screenPos, MCVector3D& point) -> uint32_t
{
    MCTerrainWindow* terrain = TerrainWindow;
    MCVertex* vertex = terrain->VertexList;
    MCVertex* closestVertex = nullptr;
    float distance = 1e7f;
    int32_t vertexIndex = 0;
    const int32_t screenX = static_cast<int16_t>(static_cast<int32_t>(std::floor(screenPos.X)));
    const int32_t screenY = static_cast<int16_t>(static_cast<int32_t>(std::floor(screenPos.Y)));

    // The vertex nearest the point on screen.
    int32_t closest = 0x40000000;

    for (int32_t i = 0; i < terrain->NumVertices; i++, vertex++)
    {
        const int32_t dx = screenX - vertex->Px;
        const int32_t dy = screenY - vertex->Py;
        const int32_t distanceSquared = dy * dy + dx * dx;

        if (distanceSquared < closest)
        {
            closest = distanceSquared;
            closestVertex = vertex;
            vertexIndex = i;
        }
    }

    // The block around it that holds the point: the corners' angles from the point turn by at most 180 degrees
    // between neighbours. Measure from its first corner.
    float angle = 0.0f;
    MCTerrainBlock* foundBlock = nullptr;
    MCTerrainBlock* block = terrain->BlockList;

    for (int32_t i = 0; i < terrain->NumBlocks; i++, block++)
    {
        if (block->Vertices[0] != closestVertex && block->Vertices[1] != closestVertex &&
            block->Vertices[2] != closestVertex && block->Vertices[3] != closestVertex)
        {
            continue;
        }

        float cornerAngle[4];

        for (int32_t corner = 0; corner < 4; corner++)
        {
            MCVector3D toCorner;
            toCorner.X = screenPos.X - static_cast<float>(block->Vertices[corner]->Px);
            toCorner.Y = screenPos.Y - static_cast<float>(block->Vertices[corner]->Py);
            toCorner.Z = 0.0f;
            const float length = std::sqrt(toCorner.X * toCorner.X + toCorner.Y * toCorner.Y + toCorner.Z * toCorner.Z);

            if (length != 0.0f)
            {
                toCorner.X /= length;
                toCorner.Y /= length;
                toCorner.Z /= length;
            }

            const double degrees = AcosMatherr(static_cast<double>(toCorner.X)) * RADIANS_TO_DEGREES;
            cornerAngle[corner] = static_cast<float>(degrees);

            if (toCorner.Y < 0.0f)
            {
                cornerAngle[corner] = static_cast<float>(360.0 - degrees);
            }
        }

        float turn0 = cornerAngle[1] - cornerAngle[0];

        if (turn0 < 0.0f)
        {
            turn0 = static_cast<float>(turn0 + 360.0);
        }

        float turn1 = cornerAngle[2] - cornerAngle[1];

        if (turn1 < 0.0f)
        {
            turn1 = static_cast<float>(turn1 + 360.0);
        }

        float turn2 = cornerAngle[3] - cornerAngle[2];

        if (turn2 < 0.0f)
        {
            turn2 = static_cast<float>(turn2 + 360.0);
        }

        float turn3 = cornerAngle[0] - cornerAngle[3];

        if (turn3 < 0.0f)
        {
            turn3 = static_cast<float>(turn3 + 360.0);
        }

        if (turn0 > 180.0f || turn1 > 180.0f || turn2 > 180.0f || turn3 > 180.0f)
        {
            continue;
        }

        foundBlock = block;
        MCVertex* corner = block->Vertices[0];
        vertexIndex = static_cast<int32_t>(corner - terrain->VertexList);
        float dx = screenPos.X - static_cast<float>(corner->Px);
        float dy = screenPos.Y - static_cast<float>(corner->Py);

        if (CameraScale == 1)
        {
            dx += dx;
            dy += dy;
        }

        if (dy != 0.0f)
        {
            angle = static_cast<float>(std::atan(static_cast<double>(dx / dy)) * RADIANS_TO_DEGREES);
        }
        else
        {
            angle = 90.0f;
        }

        distance = std::sqrt(dy * dy + dx * dx);
        break;
    }

    // Back from the corner's screen offset to the world, on the corner's row and column of the window's grid.
    // Original behaviour (OB-103): with no block found, the distance is still 1e7, far off the map.
    const double turn = (60.0 - static_cast<double>(angle)) * DEGREES_TO_RADIANS;
    const int32_t row = vertexIndex / MCTerrain::VisibleVerticesPerSide;
    const int32_t col = vertexIndex % MCTerrain::VisibleVerticesPerSide;
    const auto side = static_cast<float>(std::sin(turn) * distance / std::sin(SIXTY_DEGREES));
    point.X = static_cast<float>(std::cos(turn) * distance + std::cos(SIXTY_DEGREES) * side +
                                 static_cast<double>(col) * MCTerrain::MetersPerVertex + terrain->TopLeftX);
    point.Y = static_cast<float>(static_cast<double>(terrain->TopLeftY) -
                                 static_cast<double>(row) * MCTerrain::MetersPerVertex - side);

    if (foundBlock == nullptr)
    {
        point.Z = 0.0f;
        return 0;
    }

    point.Z = static_cast<float>(foundBlock->Vertices[0]->PVertex->Elevation) * MCTerrain::MetersPerElevLevel;
    return 0;
}

auto MCCamera::Update() -> int32_t
{
    // Port: the zoom eases here, before the objects update: they place themselves on screen from the view's size
    // (screenPos, onScreen), so it must be this frame's size by then, as the terrain drawn later uses.
    if (Window != nullptr)
    {
        Window->EaseZoom();
        const MCPane* surface = Window->WorldFrame();
        const auto surfaceWidth = static_cast<float>(surface->X1 - surface->X0);
        const auto surfaceHeight = static_cast<float>(surface->Y1 - surface->Y0);

        if (surfaceWidth != ViewWidth || surfaceHeight != ViewHeight)
        {
            SetViewSize(this, surfaceWidth, surfaceHeight);
        }
    }

    MCVector3D newPosition = Position;

    if (CameraClass == POSITION_CAMERA)
    {
        goto finish;
    }

    if (CameraClass != OBJECT_CAMERA)
    {
        return -1;
    }

    if (TargetObject == nullptr)
    {
        goto finish;
    }

    {
        auto* target = static_cast<MCGameObject*>(TargetObject);
        const MCVector3D targetPosition = target->GetPosition();
        MCFrameOfRef targetFrame = target->GetFrame();
        MCVector3D velocity = target->GetVelocity();

        if (target->IsRevealed() == 0)
        {
            ChangeTarget(static_cast<MCBaseObject*>(nullptr), 0);
            return 0;
        }

        // The target's facing: its frame turned by the torso (a mech) plus 45 degrees.
        float torso = 0.0f;

        if (target->ObjectClass == BATTLEMECH)
        {
            torso = static_cast<MCBattleMech*>(target)->TorsoRotation;
        }

        const double radians = (static_cast<double>(torso) + 45.0) * DEGREES_TO_RADIANS;
        const float s = static_cast<float>(std::sin(radians));
        const float c = static_cast<float>(std::cos(radians));
        const MCVector3D oldI = targetFrame.I;
        targetFrame.I = targetFrame.I * c + targetFrame.J * s;
        targetFrame.J = targetFrame.J * c - oldI * s;

        MCVector3D step;

        if (TargetChanged != 0)
        {
            if (ScrollyCam == 0)
            {
                newPosition = targetPosition;
                goto moved;
            }

            // Start scrolling toward the new target.
            ScrollStart = newPosition;
            ScrollJumped = 0;
            step = targetPosition - newPosition;
            const auto length = static_cast<float>(step.Magnitude());

            if (length != 0.0f)
            {
                step.X /= length;
                step.Y /= length;
                step.Z /= length;
            }

            Scrolling = 1;
            newPosition.X += step.X * CamSpeed * FrameLength;
            newPosition.Y += step.Y * CamSpeed * FrameLength;
            newPosition.Z += step.Z * CamSpeed * FrameLength;
            TargetChanged = 0;
            goto finish;
        }

        if (ForceUpdate != 0 || (Swoopy == 0 && (ScrollyCam == 0 || Scrolling == 0)))
        {
            newPosition = targetPosition;
            goto moved;
        }

        if (ScrollyCam != 0 && Scrolling != 0)
        {
            float speed;

            if (ScrollJumped == 0)
            {
                const MCVector3D toStart = ScrollStart - newPosition;
                const float distanceToStart =
                    std::sqrt(toStart.X * toStart.X + toStart.Z * toStart.Z + toStart.Y * toStart.Y);
                const MCVector3D toTarget = targetPosition - newPosition;
                step = toTarget;

                if (distanceToStart > JumpThreshold)
                {
                    // Too far: jump the rest.
                    if (std::sqrt(toTarget.X * toTarget.X + toTarget.Z * toTarget.Z + toTarget.Y * toTarget.Y) <=
                        distanceToStart)
                    {
                        newPosition = targetPosition;
                    }
                    else
                    {
                        newPosition = toStart + targetPosition;
                    }

                    ScrollJumped = 1;
                    goto finish;
                }

                speed = distanceToStart / JumpThreshold * SpeedFactor + CamSpeed;
            }
            else
            {
                if (0.0f < DistanceThreshold)
                {
                    Scrolling = 0;
                    newPosition = targetPosition;
                    goto finish;
                }

                // Port fix: MCX.EXE steps along a direction left unset on this path (reachable only with a
                // DistanceThreshold of 0 or less); the port steps along none.
                step = MCVector3D();
                speed = 0.0f / JumpThreshold * SpeedFactor + CamSpeed;
            }

            const auto length = static_cast<float>(step.Magnitude());

            if (length != 0.0f)
            {
                step.X /= length;
                step.Y /= length;
                step.Z /= length;
            }

            step.X = step.X * speed * FrameLength;
            step.Y = step.Y * speed * FrameLength;
            step.Z = step.Z * speed * FrameLength;
            const MCVector3D toTarget = targetPosition - newPosition;

            if (std::sqrt(toTarget.X * toTarget.X + toTarget.Z * toTarget.Z + toTarget.Y * toTarget.Y) <
                std::sqrt(step.X * step.X + step.Y * step.Y + step.Z * step.Z))
            {
                newPosition = targetPosition;
                goto finish;
            }

            newPosition.X += step.X;
            newPosition.Y += step.Y;
            newPosition.Z += step.Z;
            goto finish;
        }

        // Swoop: close in on a point camDistance behind the target, slowing as it nears.
        if (std::sqrt(velocity.X * velocity.X + velocity.Y * velocity.Y + velocity.Z * velocity.Z) == 0.0f &&
            SwoopDone != 0)
        {
            goto finish;
        }

        {
            if (LastTargetFacing.X != targetFrame.J.X || LastTargetFacing.Y != targetFrame.J.Y ||
                LastTargetFacing.Z != targetFrame.J.Z)
            {
                LastTargetFacing = targetFrame.J;
            }

            const float behind = -CamDistance;
            MCVector3D offset;
            offset.X = behind * LastTargetFacing.X;
            offset.Y = behind * LastTargetFacing.Y;
            offset.Z = behind * LastTargetFacing.Z;
            const MCVector3D goal = targetPosition + offset;
            velocity = goal - newPosition;
            const auto rate = static_cast<float>(velocity.Magnitude() / DistanceFactor);
            velocity.Normalize();
            velocity *= CamSpeed;
            velocity *= rate;
            velocity *= FrameLength;
            newPosition += velocity;

            if (velocity.Magnitude() < 0.5)
            {
                SwoopDone = 1;
            }
        }

        goto finish;

    moved:
        if (TerrainWindow != nullptr && (TargetChanged != 0 || ForceUpdate != 0))
        {
            TerrainWindow->Update(1);
        }

        TargetChanged = 0;
    }

finish:
    LastScreenUL = ScreenUL;
    LastScreenUL50 = ScreenUL50;
    SetPosition(newPosition);

    if (TerrainWindow != nullptr && (TargetChanged != 0 || ForceUpdate != 0 || GamePaused != 0))
    {
        TerrainWindow->Update(1);
    }

    if (Land != nullptr)
    {
        Land->ProjectTerrain(Position, ScreenUL, ScreenUL50);
    }

    TargetChanged = 0;
    ForceUpdate = 0;
    return 0;
}

auto MCCamera::Render() -> void
{
    MCPane* savedPane = GlobalPane;
    MCWindow* savedWindow = GlobalWindow;

    // Port: the world goes into the view's world surface; the unit overlays onto the screen over the view (they are
    // drawn there at the screen's scale).
    const MCOverlayTarget savedOverlay = MCOverlay;
    MCOverlay = MCOverlayTarget{GlobalPane, 1.0f, 1.0f};

    if (Window != nullptr)
    {
        GlobalPane = Window->WorldFrame();
        GlobalWindow = GlobalPane->Window;
        MCOverlay = MCOverlayTarget{Window->Frame(), Window->WorldScaleX(), Window->WorldScaleY()};
    }

    PrepareBackground();

    const auto paneWidth = static_cast<float>(GlobalPane->X1 - GlobalPane->X0);

    if (paneWidth != ViewWidth || static_cast<float>(GlobalPane->Y1 - GlobalPane->Y0) != ViewHeight)
    {
        SetViewSize(this, static_cast<float>(GlobalPane->X1 - GlobalPane->X0),
                    static_cast<float>(GlobalPane->Y1 - GlobalPane->Y0));
    }

    const int32_t scale = CameraScale;
    const int32_t zoomedIn = scale != 1 ? 1 : 0;

    if (zoomedIn != LastZoom)
    {
        SpriteManager->DumpAll();
    }

    LastZoom = zoomedIn;

    ElementList()->Reset();
    TerrainWindow->Render(HazeLevel, 1);

    if (DrawTerrainGrid != 0)
    {
        Land->DrawLines();
    }

    CraterManager()->Render();
    ObjectList->Render();

    if (GRestartRender != 0)
    {
        GRestartRender = 0;
        ElementList()->Reset();
        CraterManager()->Render();
        ObjectList->Render();
    }

    ElementList()->Sort();
    ElementList()->Draw();

    if (DrawCameraCircle != 0)
    {
        // A dot on the terrain under the last mouse event.
        MCVector2D mouse;
        MCGuiObject* eventTarget = Window->LastEvent.Target;

        if (eventTarget == nullptr)
        {
            mouse.Y = 0.0f;
            mouse.X = 0.0f;
        }
        else
        {
            // Port: through the zoom, into the world surface.
            mouse = Window->ScreenToWorld(Window->LastEvent.X, Window->LastEvent.Y);
        }

        MCVector3D ground;
        InverseProject(mouse, ground);
        const float zoom = CameraScale == 1 ? 0.5f : 1.0f;
        const MCVector3D fromEye = ground - Position;
        const float dx = fromEye.X * zoom;
        const float dy = fromEye.Y * zoom;
        const float dz = fromEye.Z * zoom;
        const float circleX = dy * CosAngle + dx * CosAngle + HalfWidth;
        const float circleY = dx * SinAngle + HalfHeight - dy * SinAngle - dz;
        const auto centerX = static_cast<int16_t>(static_cast<int32_t>(std::floor(circleX)));
        const auto centerY = static_cast<int16_t>(static_cast<int32_t>(std::floor(circleY)));
        AGEllipseFill(GlobalPane, centerX, centerY, 3, 3, 0xfe);
    }

    // Port: the pause and asked shapes are drawn on the screen over the view, at its scale (centred as the original
    // centred them in the view).
    MCPane* overlayPane = MCOverlay.Pane;
    const auto overlayHalfWidth = static_cast<float>(overlayPane->X1 - overlayPane->X0) * 0.5f;
    const auto overlayHalfHeight = static_cast<float>(overlayPane->Y1 - overlayPane->Y0) * 0.5f;

    if (GamePaused != 0)
    {
        DarkenScreen();

        if (PauseShape == nullptr)
        {
            PauseShape = LoadShape("pause", " Could not find Pause Shape ");
        }

        AGShapeDraw(overlayPane, PauseShape, 0, static_cast<int32_t>(overlayHalfWidth), 60);
    }

    if (GameAsked != 0)
    {
        DarkenScreen();

        if (AskedShape == nullptr)
        {
            AskedShape = LoadShape("asked", " Could not find Asked Shape ");
        }

        const auto y = static_cast<int32_t>(overlayHalfHeight);
        AGShapeDraw(overlayPane, AskedShape, 0, static_cast<int32_t>(overlayHalfWidth), y);
    }

    if (Window != nullptr)
    {
        GlobalPane = savedPane;
        GlobalWindow = savedWindow;
    }

    MCOverlay = savedOverlay;

    CurrentScaleFactor = scale != 1 ? 1.0f : 0.5f;
}

auto MCCamera::Activate() -> int32_t
{
    if (Ready != 0)
    {
        if (Active != 0)
        {
            return 0;
        }

        if (Ready != 0)
        {
            Active = 1;
        }
    }

    if (Window != nullptr && Window->Parent != nullptr)
    {
        ScreenWindow->AddChild(Window->Parent);
    }

    // Port fix: MCX.EXE draws the window unguarded; a camera without one ("WindowLeft" missing) would crash.
    if (Window != nullptr)
    {
        Window->Draw();
    }

    if (Land != nullptr && TerrainWindow == nullptr)
    {
        TerrainWindow = Land->NewWindow(this);

        if (TerrainWindow == nullptr)
        {
            return -1;
        }

        TerrainWindow->Update(0);
    }

    if (Active != 0 && CameraClass == OBJECT_CAMERA)
    {
        ChangeTarget(PartNumber, ObjectClassId, 1);
    }

    return 0;
}

auto MCCamera::Deactivate() -> void
{
    Active = 0;
    TargetObject = nullptr;
}

auto MCCamera::ChangeTarget(MCBaseObject* target, int jumpTo) -> int32_t
{
    TargetObject = target;

    if (target == nullptr && (target = DefaultTarget) == nullptr)
    {
        CameraClass = POSITION_CAMERA;
    }
    else
    {
        CameraClass = OBJECT_CAMERA;
    }

    TargetChanged = 1;
    MCTerrain::ForceRedraw = 1;

    if (jumpTo == 0)
    {
        Update();
    }
    else if (target != nullptr)
    {
        Position = static_cast<MCGameObject*>(target)->GetPosition();
    }

    if (TerrainWindow != nullptr)
    {
        TerrainWindow->Update(1);
    }

    return 0;
}

auto MCCamera::ChangeTarget(int32_t newPartNumber, int32_t objectId, int jumpTo) -> int32_t
{
    if (newPartNumber == 0)
    {
        if (objectId != -1)
        {
            TargetObject = ObjectList->FindObjectId(objectId);
        }
    }
    else if (Scenario != nullptr)
    {
        TargetObject = ObjectList->FindObjectFromPart(newPartNumber);
    }

    MCBaseObject* target = TargetObject;
    CameraClass = target != nullptr ? OBJECT_CAMERA : POSITION_CAMERA;
    TargetChanged = 1;
    MCTerrain::ForceRedraw = 1;

    if (jumpTo == 0)
    {
        Update();
    }
    else if (target != nullptr)
    {
        Position = static_cast<MCGameObject*>(target)->GetPosition();
    }

    if (TerrainWindow != nullptr)
    {
        TerrainWindow->Update(1);
    }

    return 0;
}

auto MCCamera::VertexProject(int32_t blockNum, int32_t vertexNum, MCVector2D& screenPos) -> int
{
    if (blockNum < 0)
    {
        blockNum = 0;
    }

    if (blockNum >= MCTerrain::TotalBlocks)
    {
        blockNum = MCTerrain::TotalBlocks - 1;
    }

    if (vertexNum < 0)
    {
        vertexNum = 0;
    }

    if (vertexNum >= VerticesPerBlock)
    {
        vertexNum = VerticesPerBlock - 1;
    }

    const int32_t index = MCTerrain::BlockOffsets[blockNum] + vertexNum;

    if (MCTerrain::ScreenPosX[index] == 0x11111111)
    {
        screenPos.Y = 10000.0f;
        screenPos.X = 10000.0f;
        return 0;
    }

    screenPos.X = static_cast<float>(MCTerrain::ScreenPosX[index]);
    screenPos.Y = static_cast<float>(MCTerrain::ScreenPosY[index]);
    return 1;
}

auto MCCamera::ScrollCamera(int32_t dx, int32_t dy) -> void
{
    const MCVector3D oldPosition = Position;

    if (TargetObject != nullptr)
    {
        ChangeTarget(static_cast<MCBaseObject*>(nullptr), 0);
    }

    MCVector3D newPosition;
    newPosition.X = static_cast<float>(dy) + static_cast<float>(dx) + oldPosition.X;
    newPosition.Y = (static_cast<float>(dx) + oldPosition.Y) - static_cast<float>(dy);
    newPosition.Z = oldPosition.Z;
    SetPosition(newPosition);
}

auto MCCamera::SetPosition(MCVector3D newPosition) -> void
{
    // The map is a diamond on screen: keep the camera inside it, a margin (smaller toward the corners) from the
    // edges.
    const float halfMap =
        static_cast<float>(MCTerrain::VerticesBlockSide * MCTerrain::BlocksMapSide) * MCTerrain::MetersPerVertex * 0.5f;
    Position = newPosition;
    const float x = newPosition.X;
    const float y = newPosition.Y;
    const float z = newPosition.Z;
    const float toRight = std::sqrt(z * z + y * y + (x - halfMap) * (x - halfMap));
    const float toTop = std::sqrt(z * z + (y - halfMap) * (y - halfMap) + x * x);
    const float toLeft = std::sqrt(z * z + y * y + (x + halfMap) * (x + halfMap));
    const float toBottom = std::sqrt(z * z + x * x + (y + halfMap) * (y + halfMap));

    // Near a corner the margin shrinks with the distance to it.
    float margin;

    if (CameraScale == 1)
    {
        float cornerOffset = 550.0f;

        if (toRight < 1850.0f)
        {
            cornerOffset = toRight - 1300.0f;
        }
        else if (toTop < 1850.0f)
        {
            cornerOffset = toTop - 1300.0f;
        }
        else if (toLeft < 1850.0f)
        {
            cornerOffset = toLeft - 1300.0f;
        }
        else if (toBottom < 1850.0f)
        {
            cornerOffset = toBottom - 1300.0f;
        }

        if (cornerOffset > 0.0f)
        {
            margin = 1300.0f - cornerOffset * 1.3636364f;
        }
        else
        {
            margin = 1300.0f;
        }
    }
    else
    {
        float cornerOffset = 375.0f;

        if (toRight < 1675.0f)
        {
            cornerOffset = toRight - 1300.0f;
        }
        else if (toTop < 1675.0f)
        {
            cornerOffset = toTop - 1300.0f;
        }
        else if (toLeft < 1675.0f)
        {
            cornerOffset = toLeft - 1300.0f;
        }
        else if (toBottom < 1675.0f)
        {
            cornerOffset = toBottom - 1300.0f;
        }

        if (cornerOffset > 0.0f)
        {
            margin = 1300.0f - cornerOffset * 2.4666667f;
        }
        else
        {
            margin = 1300.0f;
        }
    }

    const float limit = halfMap - margin;

    // y - x and x + y measure along the diamond's axes.
    const float across = y - x;
    const float along = x + y;
    const bool acrossAbove = across > limit;
    const bool acrossBelow = across < -limit;
    const bool alongAbove = along > limit;
    const bool alongBelow = along < -limit;

    if (acrossAbove)
    {
        if (alongBelow)
        {
            Position.Y = 0.0f;
            Position.X = -limit;
        }
        else if (alongAbove)
        {
            Position.Y = limit;
            Position.X = 0.0f;
        }
        else
        {
            Position.X = static_cast<float>((along - limit) * 0.5);
            Position.Y = Position.X + limit;
        }

        return;
    }

    if (acrossBelow)
    {
        if (alongBelow)
        {
            Position.Y = -limit;
            Position.X = 0.0f;
        }
        else if (alongAbove)
        {
            Position.X = limit;
            Position.Y = 0.0f;
        }
        else
        {
            Position.X = static_cast<float>((along + limit) * 0.5);
            Position.Y = Position.X - limit;
        }

        return;
    }

    if (alongAbove)
    {
        Position.X = static_cast<float>(((x - y) + limit) * 0.5);
        Position.Y = limit - Position.X;
        return;
    }

    if (alongBelow)
    {
        Position.X = static_cast<float>(((x - y) - limit) * 0.5);
        Position.Y = -Position.X - limit;
        return;
    }

    // Inside: sit on the ground. (MCX.EXE also has an unreachable " Impossible Camera Clip Situation " Fatal.)
    if (Land != nullptr)
    {
        Position.Z = Land->GetTerrainElevation(Position);
    }
}

//---------------------------------------------------------------------------
// TerrainCamera
//---------------------------------------------------------------------------

auto MCTerrainCamera::Init(MCFitIniFile* cameraFile) -> int32_t
{
    return MCCamera::Init(cameraFile, 0, 0);
}
