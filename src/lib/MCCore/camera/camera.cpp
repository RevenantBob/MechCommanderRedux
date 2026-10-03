#include "stdafx.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "color/color.h"
#include "engine/celement.h"
#include "engine/ceglist.h"
#include "engine/crater.h"
#include "engine/font.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "gui/awindow.h"
#include "iface/iface.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "lib/inifile.h"
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
#include "vfx/vfxfuncs.h"

MCOverlayTarget MCOverlay;
float MCFixedZoomHeight = 0.0f;
Camera* eye = nullptr;
uint8_t* scaleTable = nullptr;
aMainWindow* mainHolder = nullptr;
int32_t leaveSwoopyOff = 0;
int32_t drawCameraCircle = 0;
uint8_t* pauseShape = nullptr;
float currentScaleFactor = 0.0f;
int32_t lastZoom = 0;
CameraList* cameraList = nullptr;
_pane* globalPane = nullptr;
_window* globalWindow = nullptr;
uint8_t* askedShape = nullptr;

namespace
{
    /// <summary>Set while a view window's status bar is on screen (DAT_007f0938).</summary>
    int32_t statusBarDrawn = 0;
    /// <summary>The corners of the status bar last drawn (DAT_007f090c, DAT_007f0910, DAT_007f0508, DAT_007f0504).
    /// </summary>
    int32_t lastBarX0 = 0;
    int32_t lastBarY0 = 0;
    int32_t lastBarX1 = 0;
    int32_t lastBarY1 = 0;

    /// <summary>The divisors of the 7 rows of <see cref="scaleTable"/> (0x00793edc).</summary>
    constexpr int32_t scaleDivisors[7] = {9, 4, 3, 2, 1, -2, -4};

    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;
    /// <summary>60 degrees in radians, as MCX.EXE stores it (0x0077cd28).</summary>
    constexpr double SIXTY_DEGREES = 0x1.0c152382d45b2p+0;

    /// <summary>A font's line height: its height, scaled and rounded down when the font is scaled (an inline of the
    /// original's font.h).</summary>
    auto lineHeight(Font* font) -> uint8_t
    {
        uint8_t height = font->fontHeight;

        if (font->scaled != 0)
        {
            height = static_cast<uint8_t>(static_cast<int32_t>(std::floor(static_cast<float>(height) * font->scale)));
        }

        return height;
    }

    /// <summary>The view's size and the projection's sine and cosine (the same lines in Camera::init, render and
    /// viewWindow::resize).</summary>
    auto setViewSize(Camera* camera, float width, float height) -> void
    {
        camera->viewWidth = width;
        camera->viewHeight = height;
        const double angle = static_cast<double>(camera->projectionAngle) * DEGREES_TO_RADIANS;
        camera->sinAngle = static_cast<float>(std::sin(angle));
        camera->cosAngle = static_cast<float>(std::cos(angle));
        camera->halfWidth = camera->viewWidth * 0.5f;
        camera->halfHeight = camera->viewHeight * 0.5f;
    }

    /// <summary>Loads a shape file of artPath into the object cache (the pause and asked shapes).</summary>
    auto loadShape(const char* name, const char* errorMessage) -> uint8_t*
    {
        FullPathFileName shapeName;
        shapeName.init(artPath, name, ".shp");
        File shapeFile;
        const int32_t result = shapeFile.open(shapeName, READ, 50);
        Assert(result == 0, static_cast<uint32_t>(result), errorMessage);
        const uint32_t size = shapeFile.fileSize();
        auto* shape = static_cast<uint8_t*>(ObjectTypeManager::objectCache->malloc(size));
        shapeFile.read(shape, static_cast<int32_t>(size));
        shapeFile.close();
        return shape;
    }

    /// <summary>Port: how much of the way to its target the zoom eases in a millisecond (about 70 ms to settle most of
    /// the way).</summary>
    constexpr double ZOOM_EASE_TIME = 70.0;

    /// <summary>
    /// Port: keeps the tactical map's zoom button (pushed when zoomed out, as the original flipped it) in step with
    /// the main view's zoom.
    /// </summary>
    auto syncZoomButton(viewWindow* view) -> void
    {
        TacticalMap* map = Terrain::terrainTacticalMap;

        if (map == nullptr || view != MCMainView() || map->toolButtons[7] == nullptr)
        {
            return;
        }

        if ((map->toolButtons[7]->pushed != 0) != view->ZoomedOut())
        {
            map->toggleZoom();
        }
    }

    /// <summary>Darkens the whole screen for the pause and asked overlays.</summary>
    auto darkenScreen() -> void
    {
        uint8_t* hazePalette = gamePalette->getHazePalette(-7);
        SCRNVERTEX vertices[4] = {};
        vertices[1].x = application->width() - 1;
        vertices[2].x = application->width() - 1;
        vertices[2].y = application->height() - 1;
        vertices[3].y = application->height() - 1;
        VFX_translate_polygon(screenPort->frame(), 4, vertices, hazePalette);
    }
}

//---------------------------------------------------------------------------
// viewWindow
//---------------------------------------------------------------------------

viewWindow::~viewWindow()
{
    // aTitleWindow's inline destructor.
    MCRenderer::RemoveUnderlay(this);
    MCRenderer::RemoveFrameSurface(&WorldWindow);
    aTitleWindow::destroy();
}

auto viewWindow::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    const int32_t result = aObject::init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    selectionBox[1] = 0.0f;
    selectionBox[0] = 0.0f;
    interfaceWindow = 0;
    objectType = 5;
    camera = nullptr;
    selectionBox[3] = 0.0f;
    selectionBox[2] = 0.0f;
    lastEvent.target = nullptr;
    return 0;
}

auto viewWindow::destroy() -> void
{
    if (GetCamera() != nullptr)
    {
        cameraList->remove(GetCamera());
    }

    MCRenderer::RemoveUnderlay(this);
    MCRenderer::RemoveFrameSurface(&WorldWindow);
    WorldPixels = {};
    WorldWindow = {};
    WorldPane = {};
    aObject::destroy();
}

auto viewWindow::handleEvent(aEvent* event) -> void
{
    lastEvent = *event;
    const int32_t type = event->type;

    switch (type)
    {
        case 0xd:
        {
            if (GetCamera() != nullptr)
            {
                GetCamera()->deactivate();
            }

            screenWindow->removeChild(this);
            return;
        }
        case 0x12:
        {
            globalPane = screenPort->frame();
            globalWindow = screenPort->frame()->window;
            break;
        }
        case 0x1a:
        {
            // Port: the zoom toggle goes between the closest and the furthest zoom; the camera stays at scale 100
            // (the original flipped it between 100 and 1, and to 1 while paused or asked).
            if (GetCamera() != nullptr && gamePaused == 0 && gameAsked == 0)
            {
                GetCamera()->forceUpdate = 1;
                Terrain::forceRedraw = 1;
                ToggleZoom();
            }
            break;
        }
        case 0x1c:
        {
            if (GetCamera() != nullptr)
            {
                GetCamera()->changeTarget(static_cast<BaseObject*>(nullptr), 0);
            }
            break;
        }
    }

    if (interfaceWindow != 0 && theInterface != nullptr && type != 9 && type != 8)
    {
        theInterface->handleEvent(event);
        aObject::handleEvent(event);
        return;
    }

    // A click swaps this view's target (or position) with the active pane's.
    if (type == 1 && mainHolder->GetActivePane() != nullptr)
    {
        Camera* activeCamera = mainHolder->GetActivePane()->GetCamera();
        Camera* thisCamera = GetCamera();

        if (activeCamera != nullptr && thisCamera != nullptr)
        {
            vector_3d activePosition;
            BaseObject* activeTarget = activeCamera->targetObject;

            if (activeTarget == nullptr)
            {
                activePosition = activeCamera->position;
            }

            if (thisCamera->targetObject == nullptr)
            {
                activeCamera->changeTarget(static_cast<BaseObject*>(nullptr), 0);
                activeCamera->setPosition(thisCamera->getPosition());
            }
            else
            {
                activeCamera->changeTarget(thisCamera->targetObject, 0);
            }

            if (activeTarget != nullptr)
            {
                thisCamera->changeTarget(activeTarget, 0);
                aObject::handleEvent(event);
                return;
            }

            thisCamera->changeTarget(static_cast<BaseObject*>(nullptr), 0);
            thisCamera->setPosition(activePosition);
        }
    }

    aObject::handleEvent(event);
}

auto viewWindow::resize(int32_t w, int32_t h) -> void
{
    if (gridAligned != 0)
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

    aObject::resize(w, h);

    if (GetCamera() == nullptr)
    {
        return;
    }

    if (leaveSwoopyOff == 0)
    {
        if (width() < 250 || height() < 250)
        {
            GetCamera()->swoopy = 0;
        }
        else
        {
            GetCamera()->swoopy = 1;
        }
    }

    Terrain::forceRedraw = 1;
    // Port: the camera's view is the world surface, which keeps the zoom and takes the new aspect.
    UpdateWorldSurface();
    setViewSize(camera, static_cast<float>(WorldWidth()), static_cast<float>(WorldHeight()));
}

auto viewWindow::display() -> void
{
    if (showWindow == 0 || (IsHidden() != 0 && hideOffset == 0))
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
    const _pane* shown = frame();
    MCRenderer::SetUnderlay(
        MCUnderlay{this, shown->window, MCRect{shown->x0, shown->y0, shown->x1, shown->y1}, &WorldWindow});
    VFX_pane_wipe(frame(), MCRenderer::UnderlayKey);

    if (scenario != nullptr && (scenarioEndTurn == -1 || turn < scenarioEndTurn))
    {
        scenario->render(this);

        if (GetCamera()->cameraId == 1 && MPlayer != nullptr && MPlayer->sessionManager != nullptr &&
            displayProfileData == 2)
        {
            char stats[256];

            if (MPlayer->sessionManager->GetStats(stats) == 0)
            {
                lineFont->scaled = 0;
                lineFont->scale = 1.0f;
                lineFont->print(180, 72, stats, 0xfe, globalPane);
            }
        }
    }

    if (winState != aSTATE_ICONIZED)
    {
        for (int32_t i = 0; i < numChildren; i++)
        {
            childList[i]->display();
        }
    }

    if (selectionBox[2] == 0.0f && selectionBox[3] == 0.0f)
    {
        // Erase the last bar.
        if (statusBarDrawn != 0)
        {
            statusBarDrawn = 0;
            AG_StatusBar(globalPane, lastBarX0, lastBarY0, lastBarX1, lastBarY1, 0x10a, lastBarX1 - lastBarX0);
        }
    }
    else
    {
        const int32_t y1 = static_cast<int32_t>(selectionBox[3]);
        const int32_t x1 = static_cast<int32_t>(selectionBox[2]);
        const int32_t y0 = static_cast<int32_t>(selectionBox[1]);
        const int32_t x0 = static_cast<int32_t>(selectionBox[0]);
        const int32_t length = static_cast<int32_t>(selectionBox[2] - selectionBox[0]);
        AG_StatusBar(globalPane, x0, y0, x1, y1, 0x109, length);
        lastBarX0 = x0;
        lastBarY0 = y0;
        lastBarX1 = x1;
        lastBarY1 = y1;
        statusBarDrawn = 1;
    }

    drawBox(0x10, -1, -1, -1, -1);
}

auto viewWindow::leave() -> void
{
    theInterface->HideTags();
    application->SetCurrentCursor(static_cast<CursorType>(0));
}

auto viewWindow::drawBox(uint8_t color, int32_t left, int32_t top, int32_t right, int32_t bottom) -> void
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
        right = width() - 1;
    }

    if (bottom == -1)
    {
        bottom = height() - 1;
    }

    VFX_line_draw(frame(), left, top, right, top, LD_DRAW, color);
    VFX_line_draw(frame(), left, top, left, bottom, LD_DRAW, color);
    VFX_line_draw(frame(), left, bottom, right, bottom, LD_DRAW, color);
    VFX_line_draw(frame(), right, top, right, bottom, LD_DRAW, color);
}

auto viewWindow::setWindowCamera(Camera* newCamera) -> void
{
    camera = newCamera;

    if (newCamera == nullptr)
    {
        return;
    }

    if (newCamera->cameraId == 1)
    {
        setBackColor(0xef);
    }
    else if (newCamera->cameraId == 2)
    {
        setBackColor(0xf8);
    }
}

auto viewWindow::WorldFrame() -> _pane*
{
    UpdateWorldSurface();
    return &WorldPane;
}

auto viewWindow::ZoomLimits(float& closest, float& furthest) -> void
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
    if (MCTerrainGridReach > 0.0 && width() > 0 && height() > 0)
    {
        const double aspect = static_cast<double>(width()) / static_cast<double>(height());
        const double reach =
            MCTerrainGridReach / (aspect / std::cos(MCTerrainViewAngle) + 1.0 / std::sin(MCTerrainViewAngle));
        furthest = static_cast<float>(std::min(static_cast<double>(furthest), reach));
        closest = std::min(closest, furthest);
    }
}

auto viewWindow::ZoomInHeight() -> float
{
    float closest = 0.0f;
    float furthest = 0.0f;
    ZoomLimits(closest, furthest);
    const float oneToOne = std::clamp(static_cast<float>(height()), closest, furthest);
    return oneToOne < furthest ? oneToOne : closest;
}

auto viewWindow::StartZoom() -> void
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
    ZoomHeight = ZoomStartsOut ? furthest : std::clamp(static_cast<float>(height()), closest, furthest);
    ZoomTarget = ZoomHeight;
    ZoomClock = MCPort::Milliseconds();
}

auto viewWindow::UpdateWorldSurface() -> void
{
    float closest = 0.0f;
    float furthest = 0.0f;
    ZoomLimits(closest, furthest);
    float shownHeight = ZoomStartsOut ? furthest : static_cast<float>(height());

    if (ZoomHeight > 0.0f)
    {
        ZoomHeight = std::clamp(ZoomHeight, closest, furthest);
        ZoomTarget = std::clamp(ZoomTarget, closest, furthest);
        shownHeight = ZoomHeight;
    }

    shownHeight = std::clamp(shownHeight, closest, furthest);
    const int32_t surfaceHeight = std::max(1, static_cast<int32_t>(std::lround(shownHeight)));
    const int32_t surfaceWidth = std::max(
        1, static_cast<int32_t>(std::lround(static_cast<double>(width()) * surfaceHeight / std::max(1, height()))));

    if (WorldPixels.empty() || surfaceWidth != WorldWidth() || surfaceHeight != WorldHeight())
    {
        WorldPixels.assign(static_cast<size_t>(surfaceWidth) * static_cast<size_t>(surfaceHeight), 0);
        WorldWindow.buffer = WorldPixels.data();
        WorldWindow.x_max = surfaceWidth - 1;
        WorldWindow.y_max = surfaceHeight - 1;
        WorldPane.window = &WorldWindow;
        WorldPane.x0 = 0;
        WorldPane.y0 = 0;
        WorldPane.x1 = surfaceWidth - 1;
        WorldPane.y1 = surfaceHeight - 1;
        Terrain::forceRedraw = 1;
        MCRenderer::AddFrameSurface(&WorldWindow);
    }
}

auto viewWindow::ZoomTo(float height) -> bool
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

    syncZoomButton(this);
    return true;
}

auto viewWindow::ZoomBy(float factor) -> bool
{
    StartZoom();
    return ZoomTo(ZoomTarget * factor);
}

auto viewWindow::ZoomedOut() -> bool
{
    StartZoom();
    float closest = 0.0f;
    float furthest = 0.0f;
    ZoomLimits(closest, furthest);
    return ZoomTarget > (ZoomInHeight() + furthest) * 0.5f;
}

auto viewWindow::ToggleZoom() -> void
{
    float closest = 0.0f;
    float furthest = 0.0f;
    ZoomLimits(closest, furthest);
    ZoomTo(ZoomedOut() ? ZoomInHeight() : furthest);
}

auto viewWindow::EaseZoom() -> void
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

auto viewWindow::WorldScaleX() -> float
{
    if (WorldPixels.empty())
    {
        UpdateWorldSurface();
    }

    return static_cast<float>(width()) / static_cast<float>(WorldWidth());
}

auto viewWindow::WorldScaleY() -> float
{
    if (WorldPixels.empty())
    {
        UpdateWorldSurface();
    }

    return static_cast<float>(height()) / static_cast<float>(WorldHeight());
}

auto viewWindow::ScreenToWorld(int32_t screenX, int32_t screenY) -> vector_2d
{
    if (WorldPixels.empty())
    {
        UpdateWorldSurface();
    }

    // The world pixel shown at the screen pixel's centre, as the display samples it.
    const float x = (static_cast<float>(screenX - globalX()) + 0.5f) / WorldScaleX();
    const float y = (static_cast<float>(screenY - globalY()) + 0.5f) / WorldScaleY();
    return vector_2d(std::floor(x), std::floor(y));
}

auto viewWindow::WorldToWindow(vector_2d point) -> vector_2d
{
    if (WorldPixels.empty())
    {
        UpdateWorldSurface();
    }

    return vector_2d(point.x * WorldScaleX(), point.y * WorldScaleY());
}

auto viewWindow::WorldToScreen(vector_2d point) -> vector_2d
{
    const vector_2d window = WorldToWindow(point);
    return vector_2d(window.x + static_cast<float>(globalX()), window.y + static_cast<float>(globalY()));
}

auto MCIsOverlayDepth(float depth) -> bool
{
    return depth == -50000.0f || depth == -40000.0f;
}

auto MCOverlayPoint(vector_2d point) -> vector_2d
{
    return vector_2d(MCOverlayX(point.x), MCOverlayY(point.y));
}

auto MCOverlayX(float x) -> float
{
    return x * MCOverlay.ScaleX;
}

auto MCOverlayY(float y) -> float
{
    return y * MCOverlay.ScaleY;
}

auto MCMainView() -> viewWindow*
{
    if (cameraList == nullptr)
    {
        return nullptr;
    }

    Camera* main = cameraList->findCameraFromIDNumber(1);
    return main != nullptr ? main->window : nullptr;
}

auto MCWindowPoint(aObject* window, int32_t screenX, int32_t screenY) -> vector_2d
{
    Camera* camera = window->GetCamera();

    if (camera != nullptr && camera->window == window)
    {
        return camera->window->ScreenToWorld(screenX, screenY);
    }

    return vector_2d(static_cast<float>(screenX - window->globalX()), static_cast<float>(screenY - window->globalY()));
}

auto ToggleZoom() -> void
{
    mainHolder->ZoomActivePane();
}

//---------------------------------------------------------------------------
// aMainWindow
//---------------------------------------------------------------------------

aMainWindow::~aMainWindow() = default;

auto aMainWindow::init() -> int32_t
{
    lastClockTime = -1.0f;
    return init(0, 0, application->width(), application->height(), nullptr);
}

auto aMainWindow::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    const int32_t result = aHolderObject::init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    clockPane = new aObject;

    if (clockPane == nullptr)
    {
        Fatal(0, "Not enough memory to allocate clock.");
    }

    lineFont->scale = 1.5f;
    lineFont->scaled = 1;
    int32_t clockHeight;

    if (whiteFont == nullptr)
    {
        clockHeight = 24;
    }
    else
    {
        clockHeight = lineHeight(lineFont) + 4;
    }

    clockPane->init(0, 0, 40, clockHeight, nullptr);
    clockPane->setBackColor(0x10);
    Retile();
    setDepth(-50);
    return 0;
}

auto aMainWindow::destroy() -> void
{
    if (clockPane != nullptr)
    {
        clockPane->destroy();
        delete clockPane;
        clockPane = nullptr;
    }

    aHolderObject::destroy();
}

auto aMainWindow::handleEvent(aEvent* event) -> void
{
    if (event->type == 0x12)
    {
        resize(application->width(), application->height());
    }

    aObject::handleEvent(event);
}

auto aMainWindow::display() -> void
{
    if (showWindow == 0)
    {
        return;
    }

    if (IsHidden() != 0 && hideOffset == 0)
    {
        return;
    }

    aHolderObject::display();

    if (scenario->timeLimit == 0 || lastClockTime == actualTime)
    {
        return;
    }

    // The mission clock, once per time step.
    lastClockTime = actualTime;
    aObject* pane = clockPane;
    const int32_t color = pane->backColor();
    VFX_pane_wipe(pane->port()->frame(), color);
    pane->drawBox(0x1f, -1, -1, -1, -1);
    lineFont->scaled = 1;
    lineFont->scale = 1.5f;
    const uint8_t fontHeight = lineHeight(lineFont);
    const int32_t paneHeight = pane->height();

    char clock[12];
    int32_t textColor;
    const float timeLeft = static_cast<float>(scenario->timeLimit) - actualTime;

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

    const int32_t textWidth = lineFont->printWidth(clock, 0);
    const int32_t x = (pane->width() - textWidth) / 2 + 1;
    lineFont->print(x, (paneHeight - fontHeight) / 2 + 2, clock, textColor, pane->port()->frame());
}

auto aMainWindow::Retile() -> void
{
    aHolderObject::Retile();
    aObject* pane = GetActivePane();

    if (pane == nullptr)
    {
        return;
    }

    const int32_t clockWidth = clockPane->width();
    clockPane->moveTo(-2 - clockWidth + pane->right(), 2, 0);
}

auto aMainWindow::SetVertical(int on) -> void
{
    vertical = on;
    Retile();
}

auto aMainWindow::SetTiled(int tiled) -> void
{
    if (tiled != 0 && GetInactivePane() != nullptr)
    {
        GetInactivePane()->GetCamera()->activate();
    }

    aHolderObject::SetTiled(tiled);

    if (tiled == 0 && GetInactivePane() != nullptr)
    {
        GetInactivePane()->GetCamera()->deactivate();
    }
}

auto aMainWindow::ZoomActivePane() -> void
{
    if (activePane < 0)
    {
        return;
    }

    Camera* view = panes[activePane]->GetCamera();

    if (view == nullptr)
    {
        return;
    }

    // Port: between the closest and the furthest zoom, not paused or asked; the camera stays at scale 100 (the
    // original flipped it between 100 and 1, and to 1 while paused or asked).
    if (gamePaused == 0 && gameAsked == 0 && view->window != nullptr)
    {
        view->window->ToggleZoom();
    }

    view->forceUpdate = 1;
    Terrain::forceRedraw = 1;
}

auto aMainWindow::SetActivePane(aObject* pane) -> void
{
    aHolderObject::SetActivePane(pane);
    GetActivePane();
}

//---------------------------------------------------------------------------
// Camera
//---------------------------------------------------------------------------

auto Camera::operator new(size_t size) noexcept -> void*
{
    return cameraList->cameraHeap->malloc(static_cast<uint32_t>(size));
}

auto Camera::operator delete(void* ptr) -> void
{
    cameraList->cameraHeap->free(ptr);
}

auto Camera::getScaleFactor() -> float
{
    if (cameraScale != 1)
    {
        return 1.0f;
    }

    return 0.5f;
}

auto Camera::getPosition() -> vector_3d
{
    return position;
}

auto Camera::buildScaleTable() -> int32_t
{
    scaleTable = static_cast<uint8_t*>(cameraList->cameraHeap->malloc(0x700));

    if (scaleTable == nullptr)
    {
        return 0x12120001;
    }

    uint8_t* entry = scaleTable;

    for (const int32_t divisor : scaleDivisors)
    {
        for (int32_t i = 0; i < 256; i++)
        {
            *entry++ = static_cast<uint8_t>(i / (divisor + 1));
        }
    }

    return 0;
}

auto Camera::init(FitIniFile* cameraFile, int objectCamera, int32_t newCameraId) -> int32_t
{
    int32_t result;

    if (scaleTable == nullptr && (result = buildScaleTable()) != 0)
    {
        return result;
    }

    if ((result = cameraFile->readIdFloat("PixelScalar", pixelScalar)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->readIdFloat("ProjectionAngle", projectionAngle)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->readIdFloat("PositionX", position.x)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->readIdFloat("PositionY", position.y)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->readIdFloat("PositionZ", position.z)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->readIdUChar("BackgroundColor", backgroundColor)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->readIdUChar("Ready", ready)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->readIdLong("HazeLevel", hazeLevel)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->readIdLong("HazeInc", hazeInc)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->readIdLong("CameraScale", cameraScale)) != 0)
    {
        return result;
    }

    // Port: always the full-size (1x) art and layout; the zoom is the world surface's size (viewWindow), which starts
    // zoomed out where the camera started at scale 1.
    const bool startsZoomedOut = cameraScale == 1;
    cameraScale = 100;
    uint32_t windowLeft = 0;
    result = cameraFile->readIdULong("WindowLeft", windowLeft);

    if (result == 0)
    {
        uint32_t windowTop = 0;
        uint32_t windowRight = 0;
        uint32_t windowBottom = 0;

        if ((result = cameraFile->readIdULong("WindowTop", windowTop)) != 0)
        {
            return result;
        }

        if ((result = cameraFile->readIdULong("WindowRight", windowRight)) != 0)
        {
            return result;
        }

        if ((result = cameraFile->readIdULong("WindowBottom", windowBottom)) != 0)
        {
            return result;
        }

        int32_t isMainWindow = 0;

        if (cameraFile->readIdLong("MainWindow", isMainWindow) == 0)
        {
            mainWindow = isMainWindow;
        }
        else
        {
            mainWindow = 0;
        }

        auto* view = new viewWindow;
        window = view;

        if (view == nullptr)
        {
            return -0x303fffe;
        }

        const auto paneWidth = static_cast<int32_t>(windowRight - windowLeft);
        const auto paneHeight = static_cast<int32_t>(windowBottom - windowTop);
        aHolderObject* holder = nullptr;

        if (mainWindow == 0)
        {
            // Its own titled window holding the view.
            auto* titleWindow = new aEmptyTitleWindow;

            if (titleWindow == nullptr)
            {
                return -0x303fffe;
            }

            if ((result = titleWindow->init(static_cast<int32_t>(windowLeft), static_cast<int32_t>(windowTop),
                                            paneWidth, paneHeight, nullptr)) != 0)
            {
                return result;
            }

            if (titleWindow->titleBar != nullptr)
            {
                titleWindow->titleBar->showZoomButton(1);
            }

            if (titleWindow->resizeButton != nullptr)
            {
                titleWindow->resizeButton->ShowGUIWindow(1);
            }

            if (titleWindow->titleBar != nullptr)
            {
                titleWindow->titleBar->showCloseButton(1);
            }

            holder = titleWindow;
        }
        else if (mainHolder == nullptr)
        {
            mainHolder = new aMainWindow;

            if (mainHolder == nullptr)
            {
                return -0x303fffe;
            }

            if ((result = mainHolder->init()) != 0)
            {
                return result;
            }
        }

        if ((result = view->init(static_cast<int32_t>(windowLeft), static_cast<int32_t>(windowTop), paneWidth,
                                 paneHeight, nullptr)) != 0)
        {
            return result;
        }

        view->objectType = 4;

        if (mainWindow == 0)
        {
            holder->AddPane(view);
            view->moveTo(0, 0, 0);
        }
        else
        {
            if (ready != 0)
            {
                mainHolder->AddPane(view);
            }

            view->interfaceWindow = 1;
            view->objectType = 4;
        }

        cameraId = newCameraId;
        view->setWindowCamera(this);
        // Port: the view is the world surface.
        view->ZoomStartsOut = startsZoomedOut;
        view->UpdateWorldSurface();
        setViewSize(this, static_cast<float>(view->WorldWidth()), static_cast<float>(view->WorldHeight()));
    }
    else
    {
        // No window: the camera covers the global pane.
        window = nullptr;
        setViewSize(this, static_cast<float>(globalPane->x1 - globalPane->x0),
                    static_cast<float>(globalPane->y1 - globalPane->y0));
    }

    if (objectCamera == 0)
    {
        cameraClass = POSITION_CAMERA;
        swoopDone = 0;
        return 0;
    }

    if ((result = cameraFile->readIdLong("ObjectClassId", objectClassId)) != 0)
    {
        return result;
    }

    if ((result = cameraFile->readIdLong("partNumber", partNumber)) != 0)
    {
        return result;
    }

    if (cameraFile->readIdLong("SwoopyCamOff", leaveSwoopyOff) != 0)
    {
        leaveSwoopyOff = 0;
    }

    if (cameraFile->readIdLong("ScrollyCam", scrollyCam) != 0)
    {
        scrollyCam = 0;
    }

    if (cameraFile->readIdFloat("DistanceThreshold", distanceThreshold) != 0)
    {
        distanceThreshold = 10.0f;
    }

    // Original behaviour (OB-034): a missing MinScrollSpeed sets DistanceThreshold to 90 instead.
    if (cameraFile->readIdFloat("MinScrollSpeed", minScrollSpeed) != 0)
    {
        distanceThreshold = 90.0f;
    }

    if (cameraFile->readIdFloat("SpeedFactor", speedFactor) != 0)
    {
        speedFactor = 50.0f;
    }

    if (cameraFile->readIdFloat("CamDistance", camDistance) != 0)
    {
        camDistance = 50.0f;
    }

    if (cameraFile->readIdFloat("DistanceFactor", distanceFactor) != 0)
    {
        distanceFactor = 25.0f;
    }

    if (cameraFile->readIdFloat("CamSpeed", camSpeed) != 0)
    {
        camSpeed = 50.0f;
    }

    if (cameraFile->readIdFloat("JumpThreshold", jumpThreshold) != 0)
    {
        jumpThreshold = 250.0f;
    }

    cameraClass = OBJECT_CAMERA;
    swoopDone = 0;
    return 0;
}

auto Camera::init(CamData* data, int objectCamera) -> int32_t
{
    int32_t result;

    if (scaleTable == nullptr && (result = buildScaleTable()) != 0)
    {
        return result;
    }

    pixelScalar = data->pixelScalar;
    position.y = data->position[1];
    ready = data->ready;
    projectionAngle = data->projectionAngle;
    position.x = data->position[0];
    position.z = data->position[2];
    backgroundColor = data->backgroundColor;
    hazeLevel = data->hazeLevel;
    // Port: always the full-size (1x) art and layout; the zoom is the world surface's size (viewWindow), which starts
    // zoomed out where the camera started at scale 1.
    const bool startsZoomedOut = data->cameraScale == 1;
    cameraScale = 100;
    const auto windowBottom =
        static_cast<int32_t>(static_cast<double>(data->windowTop) + static_cast<double>(data->windowHeight));
    const auto windowRight =
        static_cast<int32_t>(static_cast<double>(data->windowLeft) + static_cast<double>(data->windowWidth));

    auto* view = new viewWindow;
    const auto paneWidth = static_cast<uint32_t>(windowRight - static_cast<int32_t>(data->windowLeft));
    const auto paneHeight = static_cast<uint32_t>(windowBottom - static_cast<int32_t>(data->windowTop));
    window = view;

    if ((result = view->init(static_cast<int32_t>(data->windowLeft), static_cast<int32_t>(data->windowTop),
                             static_cast<int32_t>(paneWidth), static_cast<int32_t>(paneHeight), nullptr)) != 0)
    {
        return result;
    }

    view->setWindowCamera(this);

    if (leaveSwoopyOff == 0)
    {
        if (window->width() < 150 || window->height() < 150)
        {
            swoopy = 0;
        }
        else
        {
            swoopy = 1;
        }
    }

    // Port: the view is the world surface.
    view->ZoomStartsOut = startsZoomedOut;
    view->UpdateWorldSurface();
    setViewSize(this, static_cast<float>(view->WorldWidth()), static_cast<float>(view->WorldHeight()));
    terrainWindow = land->newWindow(this);

    if (terrainWindow == nullptr)
    {
        return 0;
    }

    if (objectCamera == 0)
    {
        cameraClass = POSITION_CAMERA;
    }
    else
    {
        partNumber = data->partNumber;
        objectClassId = data->objectClassId;
        cameraClass = OBJECT_CAMERA;
        // Follow the last mover of the home side that has a network player.
        ObjectQueueNode* mechList = homeTeam->alignment == 1 ? innerSphereMechList : clanMechList;
        BaseObject* current = nullptr;

        while (mechList->Traverse(current) != nullptr)
        {
            const ObjectClass objectClass = current->objectClass;

            if ((objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
                 objectClass == MOVER) &&
                static_cast<Mover*>(current)->netPlayerId != -1)
            {
                partNumber = current->partId;
            }
        }
    }

    swoopDone = 0;
    return 0;
}

auto Camera::init() -> void
{
    pixelScalar = 1.0f;
    viewWidth = 400.0f;
    viewHeight = 400.0f;
    position.y = 0.0f;
    position.x = 0.0f;
    projectionAngle = 30.0f;
    screenUL.y = 0.0f;
    screenUL.x = 0.0f;
    camSpeed = 50.0f;
    camDistance = 50.0f;
    position.z = 0.0f;
    screenUL50.y = 0.0f;
    screenUL50.x = 0.0f;
    lastTargetFacing.z = 0.0f;
    lastTargetFacing.y = 0.0f;
    lastTargetFacing.x = 0.0f;
    active = 0;
    ready = 0;
    cameraClass = NO_CAMERA;
    unknown54 = 0;
    hazeLevel = 4;
    hazeInc = -2;
    window = nullptr;
    cameraScale = 100;
    targetObject = nullptr;
    defaultTarget = nullptr;
    unknown84.z = 0.0f;
    unknown84.y = 0.0f;
    unknown84.x = 0.0f;
    targetChanged = 1;
    distanceFactor = 25.0f;
    terrainWindow = nullptr;
    swoopy = 0;
    swoopDone = 0;
    scrollyCam = 0;
    scrolling = 0;
    speedFactor = 10.0f;
    distanceThreshold = 10.0f;
    minScrollSpeed = 90.0f;
    forceUpdate = 0;
    scrollJumped = 0;
    jumpThreshold = 250.0f;
}

auto Camera::prepareBackground() -> void
{
}

auto Camera::inverseProject(vector_2d& screenPos, vector_3d& point) -> uint32_t
{
    TerrainWindow* terrain = terrainWindow;
    Vertex* vertex = terrain->vertexList;
    Vertex* closestVertex = nullptr;
    float distance = 1e7f;
    int32_t vertexIndex = 0;
    const int32_t screenX = static_cast<int16_t>(static_cast<int32_t>(std::floor(screenPos.x)));
    const int32_t screenY = static_cast<int16_t>(static_cast<int32_t>(std::floor(screenPos.y)));

    // The vertex nearest the point on screen.
    int32_t closest = 0x40000000;

    for (int32_t i = 0; i < terrain->numVertices; i++, vertex++)
    {
        const int32_t dx = screenX - vertex->px;
        const int32_t dy = screenY - vertex->py;
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
    TerrainBlock* foundBlock = nullptr;
    TerrainBlock* block = terrain->blockList;

    for (int32_t i = 0; i < terrain->numBlocks; i++, block++)
    {
        if (block->vertices[0] != closestVertex && block->vertices[1] != closestVertex &&
            block->vertices[2] != closestVertex && block->vertices[3] != closestVertex)
        {
            continue;
        }

        float cornerAngle[4];

        for (int32_t corner = 0; corner < 4; corner++)
        {
            vector_3d toCorner;
            toCorner.x = screenPos.x - static_cast<float>(block->vertices[corner]->px);
            toCorner.y = screenPos.y - static_cast<float>(block->vertices[corner]->py);
            toCorner.z = 0.0f;
            const float length = std::sqrt(toCorner.x * toCorner.x + toCorner.y * toCorner.y + toCorner.z * toCorner.z);

            if (length != 0.0f)
            {
                toCorner.x /= length;
                toCorner.y /= length;
                toCorner.z /= length;
            }

            const double degrees = acosMatherr(static_cast<double>(toCorner.x)) * RADIANS_TO_DEGREES;
            cornerAngle[corner] = static_cast<float>(degrees);

            if (toCorner.y < 0.0f)
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
        Vertex* corner = block->vertices[0];
        vertexIndex = static_cast<int32_t>(corner - terrain->vertexList);
        float dx = screenPos.x - static_cast<float>(corner->px);
        float dy = screenPos.y - static_cast<float>(corner->py);

        if (cameraScale == 1)
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
    const int32_t row = vertexIndex / Terrain::visibleVerticesPerSide;
    const int32_t col = vertexIndex % Terrain::visibleVerticesPerSide;
    const auto side = static_cast<float>(std::sin(turn) * distance / std::sin(SIXTY_DEGREES));
    point.x = static_cast<float>(std::cos(turn) * distance + std::cos(SIXTY_DEGREES) * side +
                                 static_cast<double>(col) * Terrain::metersPerVertex + terrain->topLeftX);
    point.y = static_cast<float>(static_cast<double>(terrain->topLeftY) -
                                 static_cast<double>(row) * Terrain::metersPerVertex - side);

    if (foundBlock == nullptr)
    {
        point.z = 0.0f;
        return 0;
    }

    point.z = static_cast<float>(foundBlock->vertices[0]->pVertex->elevation) * Terrain::metersPerElevLevel;
    return 0;
}

auto Camera::update() -> int32_t
{
    // Port: the zoom eases here, before the objects update: they place themselves on screen from the view's size
    // (screenPos, onScreen), so it must be this frame's size by then, as the terrain drawn later uses.
    if (window != nullptr)
    {
        window->EaseZoom();
        const _pane* surface = window->WorldFrame();
        const auto surfaceWidth = static_cast<float>(surface->x1 - surface->x0);
        const auto surfaceHeight = static_cast<float>(surface->y1 - surface->y0);

        if (surfaceWidth != viewWidth || surfaceHeight != viewHeight)
        {
            setViewSize(this, surfaceWidth, surfaceHeight);
        }
    }

    vector_3d newPosition = position;

    if (cameraClass == POSITION_CAMERA)
    {
        goto finish;
    }

    if (cameraClass != OBJECT_CAMERA)
    {
        return -1;
    }

    if (targetObject == nullptr)
    {
        goto finish;
    }

    {
        auto* target = static_cast<GameObject*>(targetObject);
        const vector_3d targetPosition = target->getPosition();
        frame_of_ref targetFrame = target->getFrame();
        vector_3d velocity = target->getVelocity();

        if (target->isRevealed() == 0)
        {
            changeTarget(static_cast<BaseObject*>(nullptr), 0);
            return 0;
        }

        // The target's facing: its frame turned by the torso (a mech) plus 45 degrees.
        float torso = 0.0f;

        if (target->objectClass == BATTLEMECH)
        {
            torso = static_cast<BattleMech*>(target)->torsoRotation;
        }

        const double radians = (static_cast<double>(torso) + 45.0) * DEGREES_TO_RADIANS;
        const float s = static_cast<float>(std::sin(radians));
        const float c = static_cast<float>(std::cos(radians));
        const vector_3d oldI = targetFrame.i;
        targetFrame.i = targetFrame.i * c + targetFrame.j * s;
        targetFrame.j = targetFrame.j * c - oldI * s;

        vector_3d step;

        if (targetChanged != 0)
        {
            if (scrollyCam == 0)
            {
                newPosition = targetPosition;
                goto moved;
            }

            // Start scrolling toward the new target.
            scrollStart = newPosition;
            scrollJumped = 0;
            step = targetPosition - newPosition;
            const auto length = static_cast<float>(step.magnitude());

            if (length != 0.0f)
            {
                step.x /= length;
                step.y /= length;
                step.z /= length;
            }

            scrolling = 1;
            newPosition.x += step.x * camSpeed * frameLength;
            newPosition.y += step.y * camSpeed * frameLength;
            newPosition.z += step.z * camSpeed * frameLength;
            targetChanged = 0;
            goto finish;
        }

        if (forceUpdate != 0 || (swoopy == 0 && (scrollyCam == 0 || scrolling == 0)))
        {
            newPosition = targetPosition;
            goto moved;
        }

        if (scrollyCam != 0 && scrolling != 0)
        {
            float speed;

            if (scrollJumped == 0)
            {
                const vector_3d toStart = scrollStart - newPosition;
                const float distanceToStart =
                    std::sqrt(toStart.x * toStart.x + toStart.z * toStart.z + toStart.y * toStart.y);
                const vector_3d toTarget = targetPosition - newPosition;
                step = toTarget;

                if (distanceToStart > jumpThreshold)
                {
                    // Too far: jump the rest.
                    if (std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z + toTarget.y * toTarget.y) <=
                        distanceToStart)
                    {
                        newPosition = targetPosition;
                    }
                    else
                    {
                        newPosition = toStart + targetPosition;
                    }

                    scrollJumped = 1;
                    goto finish;
                }

                speed = distanceToStart / jumpThreshold * speedFactor + camSpeed;
            }
            else
            {
                if (0.0f < distanceThreshold)
                {
                    scrolling = 0;
                    newPosition = targetPosition;
                    goto finish;
                }

                // Port fix: MCX.EXE steps along a direction left unset on this path (reachable only with a
                // DistanceThreshold of 0 or less); the port steps along none.
                step = vector_3d();
                speed = 0.0f / jumpThreshold * speedFactor + camSpeed;
            }

            const auto length = static_cast<float>(step.magnitude());

            if (length != 0.0f)
            {
                step.x /= length;
                step.y /= length;
                step.z /= length;
            }

            step.x = step.x * speed * frameLength;
            step.y = step.y * speed * frameLength;
            step.z = step.z * speed * frameLength;
            const vector_3d toTarget = targetPosition - newPosition;

            if (std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z + toTarget.y * toTarget.y) <
                std::sqrt(step.x * step.x + step.y * step.y + step.z * step.z))
            {
                newPosition = targetPosition;
                goto finish;
            }

            newPosition.x += step.x;
            newPosition.y += step.y;
            newPosition.z += step.z;
            goto finish;
        }

        // Swoop: close in on a point camDistance behind the target, slowing as it nears.
        if (std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z) == 0.0f &&
            swoopDone != 0)
        {
            goto finish;
        }

        {
            if (lastTargetFacing.x != targetFrame.j.x || lastTargetFacing.y != targetFrame.j.y ||
                lastTargetFacing.z != targetFrame.j.z)
            {
                lastTargetFacing = targetFrame.j;
            }

            const float behind = -camDistance;
            vector_3d offset;
            offset.x = behind * lastTargetFacing.x;
            offset.y = behind * lastTargetFacing.y;
            offset.z = behind * lastTargetFacing.z;
            const vector_3d goal = targetPosition + offset;
            velocity = goal - newPosition;
            const auto rate = static_cast<float>(velocity.magnitude() / distanceFactor);
            velocity.normalize();
            velocity *= camSpeed;
            velocity *= rate;
            velocity *= frameLength;
            newPosition += velocity;

            if (velocity.magnitude() < 0.5)
            {
                swoopDone = 1;
            }
        }

        goto finish;

    moved:
        if (terrainWindow != nullptr && (targetChanged != 0 || forceUpdate != 0))
        {
            terrainWindow->update(1);
        }

        targetChanged = 0;
    }

finish:
    lastScreenUL = screenUL;
    lastScreenUL50 = screenUL50;
    setPosition(newPosition);

    if (terrainWindow != nullptr && (targetChanged != 0 || forceUpdate != 0 || gamePaused != 0))
    {
        terrainWindow->update(1);
    }

    if (land != nullptr)
    {
        land->projectTerrain(position, screenUL, screenUL50);
    }

    targetChanged = 0;
    forceUpdate = 0;
    return 0;
}

auto Camera::render() -> void
{
    MaxObjectsDrawn = 0;
    _pane* savedPane = globalPane;
    _window* savedWindow = globalWindow;

    // Port: the world goes into the view's world surface; the unit overlays onto the screen over the view (they are
    // drawn there at the screen's scale).
    const MCOverlayTarget savedOverlay = MCOverlay;
    MCOverlay = MCOverlayTarget{globalPane, 1.0f, 1.0f};

    if (window != nullptr)
    {
        globalPane = window->WorldFrame();
        globalWindow = globalPane->window;
        MCOverlay = MCOverlayTarget{window->frame(), window->WorldScaleX(), window->WorldScaleY()};
    }

    prepareBackground();

    const auto paneWidth = static_cast<float>(globalPane->x1 - globalPane->x0);

    if (paneWidth != viewWidth || static_cast<float>(globalPane->y1 - globalPane->y0) != viewHeight)
    {
        setViewSize(this, static_cast<float>(globalPane->x1 - globalPane->x0),
                    static_cast<float>(globalPane->y1 - globalPane->y0));
    }

    const int32_t scale = cameraScale;
    const int32_t zoomedIn = scale != 1 ? 1 : 0;

    if (zoomedIn != lastZoom)
    {
        spriteManager->dumpALL();
    }

    lastZoom = zoomedIn;

    ElementPool::reset();
    ElementList->reset();
    terrainWindow->render(hazeLevel, 1);

    if (drawTerrainGrid != 0)
    {
        land->drawLines();
    }

    craterManager->render();
    objectList->render();

    if (gRestartRender != 0)
    {
        gRestartRender = 0;
        ElementPool::reset();
        ElementList->reset();
        craterManager->render();
        objectList->render();
    }

    ElementList->sort();
    ElementList->draw();

    if (drawCameraCircle != 0)
    {
        // A dot on the terrain under the last mouse event.
        vector_2d mouse;
        aObject* eventTarget = window->lastEvent.target;

        if (eventTarget == nullptr)
        {
            mouse.y = 0.0f;
            mouse.x = 0.0f;
        }
        else
        {
            // Port: through the zoom, into the world surface.
            mouse = window->ScreenToWorld(window->lastEvent.x, window->lastEvent.y);
        }

        vector_3d ground;
        inverseProject(mouse, ground);
        const float zoom = cameraScale == 1 ? 0.5f : 1.0f;
        const vector_3d fromEye = ground - position;
        const float dx = fromEye.x * zoom;
        const float dy = fromEye.y * zoom;
        const float dz = fromEye.z * zoom;
        const float circleX = dy * cosAngle + dx * cosAngle + halfWidth;
        const float circleY = dx * sinAngle + halfHeight - dy * sinAngle - dz;
        const auto centerX = static_cast<int16_t>(static_cast<int32_t>(std::floor(circleX)));
        const auto centerY = static_cast<int16_t>(static_cast<int32_t>(std::floor(circleY)));
        AG_ellipse_fill(globalPane, centerX, centerY, 3, 3, 0xfe);
    }

    // Port: the pause and asked shapes are drawn on the screen over the view, at its scale (centred as the original
    // centred them in the view).
    _pane* overlayPane = MCOverlay.Pane;
    const auto overlayHalfWidth = static_cast<float>(overlayPane->x1 - overlayPane->x0) * 0.5f;
    const auto overlayHalfHeight = static_cast<float>(overlayPane->y1 - overlayPane->y0) * 0.5f;

    if (gamePaused != 0)
    {
        darkenScreen();

        if (pauseShape == nullptr)
        {
            pauseShape = loadShape("pause", " Could not find Pause Shape ");
        }

        AG_shape_draw(overlayPane, pauseShape, 0, static_cast<int32_t>(overlayHalfWidth), 60);
    }

    if (gameAsked != 0)
    {
        darkenScreen();

        if (askedShape == nullptr)
        {
            askedShape = loadShape("asked", " Could not find Asked Shape ");
        }

        const auto y = static_cast<int32_t>(overlayHalfHeight);
        AG_shape_draw(overlayPane, askedShape, 0, static_cast<int32_t>(overlayHalfWidth), y);
    }

    if (window != nullptr)
    {
        globalPane = savedPane;
        globalWindow = savedWindow;
    }

    MCOverlay = savedOverlay;

    currentScaleFactor = scale != 1 ? 1.0f : 0.5f;
}

auto Camera::activate() -> int32_t
{
    if (ready != 0)
    {
        if (active != 0)
        {
            return 0;
        }

        if (ready != 0)
        {
            active = 1;
        }
    }

    if (window != nullptr && window->parent != nullptr)
    {
        screenWindow->addChild(window->parent);
    }

    // Port fix: MCX.EXE draws the window unguarded; a camera without one ("WindowLeft" missing) would crash.
    if (window != nullptr)
    {
        window->draw();
    }

    if (land != nullptr && terrainWindow == nullptr)
    {
        terrainWindow = land->newWindow(this);

        if (terrainWindow == nullptr)
        {
            return -1;
        }

        terrainWindow->update(0);
    }

    if (active != 0 && cameraClass == OBJECT_CAMERA)
    {
        changeTarget(partNumber, objectClassId, 1);
    }

    return 0;
}

auto Camera::deactivate() -> void
{
    active = 0;
    targetObject = nullptr;
}

auto Camera::changeTarget(BaseObject* target, int jumpTo) -> int32_t
{
    targetObject = target;

    if (target == nullptr && (target = defaultTarget) == nullptr)
    {
        cameraClass = POSITION_CAMERA;
    }
    else
    {
        cameraClass = OBJECT_CAMERA;
    }

    targetChanged = 1;
    Terrain::forceRedraw = 1;

    if (jumpTo == 0)
    {
        update();
    }
    else if (target != nullptr)
    {
        position = static_cast<GameObject*>(target)->getPosition();
    }

    if (terrainWindow != nullptr)
    {
        terrainWindow->update(1);
    }

    return 0;
}

auto Camera::changeTarget(int32_t newPartNumber, int32_t objectId, int jumpTo) -> int32_t
{
    if (newPartNumber == 0)
    {
        if (objectId != -1)
        {
            targetObject = objectList->findObjectId(objectId);
        }
    }
    else if (scenario != nullptr)
    {
        targetObject = objectList->findObjectFromPart(newPartNumber);
    }

    BaseObject* target = targetObject;
    cameraClass = target != nullptr ? OBJECT_CAMERA : POSITION_CAMERA;
    targetChanged = 1;
    Terrain::forceRedraw = 1;

    if (jumpTo == 0)
    {
        update();
    }
    else if (target != nullptr)
    {
        position = static_cast<GameObject*>(target)->getPosition();
    }

    if (terrainWindow != nullptr)
    {
        terrainWindow->update(1);
    }

    return 0;
}

auto Camera::vertexProject(int32_t blockNum, int32_t vertexNum, vector_2d& screenPos) -> int
{
    if (blockNum < 0)
    {
        blockNum = 0;
    }

    if (blockNum >= Terrain::totalBlocks)
    {
        blockNum = Terrain::totalBlocks - 1;
    }

    if (vertexNum < 0)
    {
        vertexNum = 0;
    }

    if (vertexNum >= verticesPerBlock)
    {
        vertexNum = verticesPerBlock - 1;
    }

    const int32_t index = Terrain::blockOffsets[blockNum] + vertexNum;

    if (Terrain::screenPosX[index] == 0x11111111)
    {
        screenPos.y = 10000.0f;
        screenPos.x = 10000.0f;
        return 0;
    }

    screenPos.x = static_cast<float>(Terrain::screenPosX[index]);
    screenPos.y = static_cast<float>(Terrain::screenPosY[index]);
    return 1;
}

auto Camera::scrollCamera(int32_t dx, int32_t dy) -> void
{
    const vector_3d oldPosition = position;

    if (targetObject != nullptr)
    {
        changeTarget(static_cast<BaseObject*>(nullptr), 0);
    }

    vector_3d newPosition;
    newPosition.x = static_cast<float>(dy) + static_cast<float>(dx) + oldPosition.x;
    newPosition.y = (static_cast<float>(dx) + oldPosition.y) - static_cast<float>(dy);
    newPosition.z = oldPosition.z;
    setPosition(newPosition);
}

auto Camera::setPosition(vector_3d newPosition) -> void
{
    // The map is a diamond on screen: keep the camera inside it, a margin (smaller toward the corners) from the
    // edges.
    const float halfMap =
        static_cast<float>(Terrain::verticesBlockSide * Terrain::blocksMapSide) * Terrain::metersPerVertex * 0.5f;
    position = newPosition;
    const float x = newPosition.x;
    const float y = newPosition.y;
    const float z = newPosition.z;
    const float toRight = std::sqrt(z * z + y * y + (x - halfMap) * (x - halfMap));
    const float toTop = std::sqrt(z * z + (y - halfMap) * (y - halfMap) + x * x);
    const float toLeft = std::sqrt(z * z + y * y + (x + halfMap) * (x + halfMap));
    const float toBottom = std::sqrt(z * z + x * x + (y + halfMap) * (y + halfMap));

    // Near a corner the margin shrinks with the distance to it.
    float margin;

    if (cameraScale == 1)
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
            position.y = 0.0f;
            position.x = -limit;
        }
        else if (alongAbove)
        {
            position.y = limit;
            position.x = 0.0f;
        }
        else
        {
            position.x = static_cast<float>((along - limit) * 0.5);
            position.y = position.x + limit;
        }

        return;
    }

    if (acrossBelow)
    {
        if (alongBelow)
        {
            position.y = -limit;
            position.x = 0.0f;
        }
        else if (alongAbove)
        {
            position.x = limit;
            position.y = 0.0f;
        }
        else
        {
            position.x = static_cast<float>((along + limit) * 0.5);
            position.y = position.x - limit;
        }

        return;
    }

    if (alongAbove)
    {
        position.x = static_cast<float>(((x - y) + limit) * 0.5);
        position.y = limit - position.x;
        return;
    }

    if (alongBelow)
    {
        position.x = static_cast<float>(((x - y) - limit) * 0.5);
        position.y = -position.x - limit;
        return;
    }

    // Inside: sit on the ground. (MCX.EXE also has an unreachable " Impossible Camera Clip Situation " Fatal.)
    if (land != nullptr)
    {
        position.z = land->getTerrainElevation(position);
    }
}

//---------------------------------------------------------------------------
// TerrainCamera
//---------------------------------------------------------------------------

auto TerrainCamera::init(FitIniFile* cameraFile) -> int32_t
{
    return Camera::init(cameraFile, 0, 0);
}
