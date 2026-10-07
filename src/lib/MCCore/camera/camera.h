#pragma once

#include "gui/awindow.h"
#include "lib/cvmath.h"

class MCMainWindow;
class MCBaseObject;
class MCCamera;
class MCCameraList;
class MCFitIniFile;
class MCTerrainWindow;
struct MCPane;
struct MCWindow;

/// <summary>What a camera looks at. The enumerator names are the port's.</summary>
enum MCCameraClass : int32_t
{
    /// <summary>Not set up yet (Camera::init).</summary>
    NO_CAMERA = -1,
    /// <summary>A fixed position (the "Camera%d" blocks, or an object camera without a target).</summary>
    POSITION_CAMERA = 0,
    /// <summary>Follows its target object (the "ObjectCamera%d" blocks).</summary>
    OBJECT_CAMERA = 1,
};

/// <summary>
/// A camera's settings as the scenario hands them to <see cref="MCCamera::init(CamData*, int)"/>. The field names are
/// the port's; the FIT keys of <see cref="MCCamera::init(FitIniFile*, int, int32_t)"/> they stand for are given.
/// </summary>
struct MCCamData
{
    /// <summary>"PixelScalar".</summary>
    float PixelScalar;
    /// <summary>"ProjectionAngle", in degrees.</summary>
    float ProjectionAngle;
    /// <summary>"CameraScale".</summary>
    int32_t CameraScale;
    /// <summary>The window's height, added to top for the bottom edge.</summary>
    float WindowHeight;
    /// <summary>The window's width, added to left for the right edge.</summary>
    float WindowWidth;
    /// <summary>"WindowLeft".</summary>
    uint32_t WindowLeft;
    /// <summary>"WindowTop".</summary>
    uint32_t WindowTop;
    /// <summary>"BackgroundColor".</summary>
    uint8_t BackgroundColor;
    /// <summary>"HazeLevel".</summary>
    int32_t HazeLevel;
    /// <summary>"Ready".</summary>
    uint8_t Ready;
    /// <summary>"PositionX", "PositionY", "PositionZ".</summary>
    float Position[3];
    /// <summary>"partNumber" of the target.</summary>
    int32_t PartNumber;
    /// <summary>"ObjectClassId" of the target.</summary>
    int32_t ObjectClassId;
};

/// <summary>The camera the scene is being rendered through (CameraList::renderView sets it per camera).</summary>
extern MCCamera* Eye;
/// <summary>7 x 256 bytes: for each divisor of <c>scaleDivisors</c>, i / (divisor + 1) (Camera::buildScaleTable).
/// Freed with the camera list.</summary>
extern std::vector<uint8_t> ScaleTable;
/// <summary>The main window that holds the camera panes (made by the first camera with "MainWindow" set).</summary>
extern MCMainWindow* MainHolder;
/// <summary>"SwoopyCamOff": when set, cameras don't swoop to their target.</summary>
extern int32_t LeaveSwoopyOff;
/// <summary>Debug switch: draw the camera's circle.</summary>
extern int32_t DrawCameraCircle;
/// <summary>The pause shape (cleared by CameraList::destroy).</summary>
extern uint8_t* PauseShape;
/// <summary>The "asked" shape, drawn while the game waits on a question (cleared by CameraList::destroy).</summary>
/// <remarks>The name is the port's (the binary kept no symbol for it).</remarks>
extern uint8_t* AskedShape;
/// <summary>The current zoom's scale factor.</summary>
extern float CurrentScaleFactor;
/// <summary>The last zoom setting.</summary>
extern int32_t LastZoom;
/// <summary>The game's cameras.</summary>
extern MCCameraList* CameraList;
/// <summary>The pane everything is drawn into: the rendering camera's window (Camera::render), else the screen.
/// </summary>
/// <remarks>The binary kept no owner file for it (it sits in the bss after the camera and colour globals); the port
/// defines it in camera.cpp.</remarks>
extern MCPane* GlobalPane;
/// <summary>The window of <see cref="GlobalPane"/>.</summary>
extern MCWindow* GlobalWindow;

/// <summary>Zooms the main window's active pane.</summary>
void ToggleZoom();

/// <summary>
/// A camera pane: an <see cref="MCGuiTitleWindow"/> that renders the scenario through its <see cref="MCCamera"/>, and
/// passes events to the interface when it is the main view.
/// </summary>
/// <remarks>Original source: <c>camera\camera.cpp</c>, <c>camera\camera.h</c>; 0x500 bytes.</remarks>
class MCViewWindow : public MCGuiTitleWindow
{
public:
    ~MCViewWindow() override;
    /// <summary>aObject::init, clears the fields; depth 5.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <summary>Deactivates the camera, then aObject::destroy.</summary>
    void Destroy() override;
    /// <summary>
    /// Keeps a copy of the event; handles close (deactivates the camera), resize (the global pane), zoom (toggles
    /// the camera scale 1/100) and follow-off; hands the rest to the interface when <see cref="InterfaceWindow"/>
    /// is set; a click swaps this camera's view with the active pane's.
    /// </summary>
    void HandleEvent(MCGuiEvent* event) override;
    /// <summary>Snaps a titled window's size to multiples of 40, then resizes the camera's view.</summary>
    void Resize(int32_t w, int32_t h) override;
    /// <summary>Renders the scenario through the camera, the children, and the status bar (erasing the last one
    /// drawn when there is none).</summary>
    void Display() override;
    void Leave() override;
    /// <summary>Draws a one-pixel frame (-1 edges mean the window's).</summary>
    void DrawBox(uint8_t color, int32_t left, int32_t top, int32_t right, int32_t bottom) override;
    using MCGuiTitleWindow::DrawBox;
    MCCamera* GetCamera() override { return Camera; }
    /// <summary>Sets the camera; the background colour follows its id (1: 0xef, 2: 0xf8).</summary>
    void SetWindowCamera(MCCamera* newCamera);

    // Port: the world surface and the zoom. The camera draws the world into a surface of its own, at 1x (camera
    // scale 100), as tall as the zoom asks: ZoomHeight game pixels, 480 closest to 2160 furthest, whatever the
    // window's size. The surface has the view's aspect, is the screen's underlay over the view's rectangle (the
    // display scales it there), and the view's rectangle of the screen is left in the key colour for it to show
    // through. Unit overlays (health bars, selection marks) are drawn on the screen at its own scale.

    /// <summary>The closest zoom: the view shows 480 lines of the world (the original's 640x480 at camera scale 100).</summary>
    static constexpr float ZoomClosest = 480.0f;
    /// <summary>The furthest zoom: 2160 lines of the world.</summary>
    static constexpr float ZoomFurthest = 2160.0f;

    /// <summary>The world surface's pane (what the camera draws into), sized for the zoom first.</summary>
    MCPane* WorldFrame();
    /// <summary>
    /// Sizes the world surface from <see cref="ZoomHeight"/> (the view's height until the zoom starts) and the view's
    /// aspect; it is cleared when its size changes.
    /// </summary>
    void UpdateWorldSurface();
    /// <summary>The world surface's width and height.</summary>
    int32_t WorldWidth() const { return WorldWindow.XMax + 1; }
    int32_t WorldHeight() const { return WorldWindow.YMax + 1; }
    /// <summary>The closest and furthest zoom for this view: 480 and 2160, or less where the terrain grid can't cover
    /// a surface that large (a very wide view).</summary>
    void ZoomLimits(float& closest, float& furthest);
    /// <summary>Moves the zoom toward <paramref name="height"/> (eased over a few frames), within the limits.</summary>
    /// <returns>Whether the target moved.</returns>
    bool ZoomTo(float height);
    /// <summary>Zooms out by <paramref name="factor"/> (in when below 1) from the current target.</summary>
    bool ZoomBy(float factor);
    /// <summary>
    /// The zoom the toggle (and a view's start) zooms in to: one world pixel per screen pixel, within the limits, or
    /// the closest zoom when that is already the furthest.
    /// </summary>
    float ZoomInHeight();
    /// <summary>Whether the target is nearer the furthest zoom than <see cref="ZoomInHeight"/> (the original's
    /// zoomed-out camera scale 1).</summary>
    bool ZoomedOut();
    /// <summary>The zoom toggle (the original flipped the camera between scales 100 and 1): to
    /// <see cref="ZoomInHeight"/> when zoomed out, else to the furthest zoom.</summary>
    void ToggleZoom();
    /// <summary>Eases <see cref="ZoomHeight"/> toward the target: called once a frame by Camera::update, before the
    /// objects place themselves on screen.</summary>
    void EaseZoom();
    /// <summary>Sets the zoom the first time it is needed: one world pixel per screen pixel, within the limits.</summary>
    void StartZoom();
    /// <summary>Screen pixels per world surface pixel, across and down.</summary>
    float WorldScaleX();
    float WorldScaleY();
    /// <summary>The point of the world surface under screen point (<paramref name="screenX"/>, <paramref name="screenY"/>).</summary>
    MCVector2D ScreenToWorld(int32_t screenX, int32_t screenY);
    /// <summary>The window point (relative to this view) a point of the world surface is shown at.</summary>
    MCVector2D WorldToWindow(MCVector2D point);
    /// <summary>The screen point a point of the world surface is shown at.</summary>
    MCVector2D WorldToScreen(MCVector2D point);

    /// <summary>The drag-selection box's corners (x0 y0 where the drag started, x1 y1 where the mouse is), zeroed
    /// by init and when the drag ends; display draws it unless x1 and y1 are both 0.</summary>
    /// <remarks>Port: in the view's own (screen) coordinates, not the world surface's.</remarks>
    float SelectionBox[4] = {};
    /// <summary>The camera shown.</summary>
    MCCamera* Camera = nullptr;
    /// <summary>Set for the main view: events go to the interface first.</summary>
    int32_t InterfaceWindow = 0;
    /// <summary>A copy of the last event handled.</summary>
    MCGuiEvent LastEvent;

    /// <summary>Port: how many lines of the world the view shows now (0 until the first frame sets it).</summary>
    float ZoomHeight = 0.0f;
    /// <summary>Port: the zoom <see cref="ZoomHeight"/> eases toward.</summary>
    float ZoomTarget = 0.0f;
    /// <summary>Port: when the zoom last eased (MCPort::Milliseconds).</summary>
    uint32_t ZoomClock = 0;
    /// <summary>Port: the view starts at the furthest zoom (its camera's "CameraScale" was 1, zoomed out).</summary>
    bool ZoomStartsOut = false;
    /// <summary>Port: the view has been shown; until then a zoom change takes effect at once, not eased.</summary>
    bool ZoomShown = false;
    /// <summary>Port: the world surface's pixels, window and pane.</summary>
    std::vector<uint8_t> WorldPixels;
    MCWindow WorldWindow{};
    MCPane WorldPane{};
};

/// <summary>
/// Port: the unit overlays. Health bars, selection marks and strike timers (the elements at depth -50000 and -40000)
/// are drawn on the screen, over the view's rectangle, at the screen's scale: their positions follow the world through
/// the zoom, their sizes don't. While a camera renders, this is the view's pane on the screen and the scale from the
/// camera's world surface to it.
/// </summary>
struct MCOverlayTarget
{
    /// <summary>The pane the overlays draw into (null outside Camera::render).</summary>
    MCPane* Pane = nullptr;
    /// <summary>Screen pixels per world surface pixel.</summary>
    float ScaleX = 1.0f;
    float ScaleY = 1.0f;
};

/// <summary>Port: where the overlays of the camera rendering go.</summary>
extern MCOverlayTarget MCOverlay;

/// <summary>
/// Port-only (tests): when above 0, every view's zoom is fixed at this many lines of the world (both limits), so a run
/// shows, and updates, the same area whatever the window.
/// </summary>
extern float MCFixedZoomHeight;

/// <summary>Port: whether an element at <paramref name="depth"/> is a unit overlay (-50000 or -40000).</summary>
bool MCIsOverlayDepth(float depth);
/// <summary>Port: a point of the rendering camera's world surface, in the overlays' pane.</summary>
MCVector2D MCOverlayPoint(MCVector2D point);
/// <summary>Port: <see cref="MCOverlayPoint"/> of one coordinate.</summary>
float MCOverlayX(float x);
float MCOverlayY(float y);
/// <summary>Port: the view of the main camera (camera 1), whose screen positions objects keep at index 0, or null.</summary>
MCViewWindow* MCMainView();
/// <summary>
/// Port: screen point (<paramref name="screenX"/>, <paramref name="screenY"/>) in <paramref name="window"/>'s
/// coordinates: for a camera's view, the point of its world surface under it (through the zoom), as the camera's
/// projections take it; for other windows, relative to the window's corner.
/// </summary>
MCVector2D MCWindowPoint(MCGuiObject* window, int32_t screenX, int32_t screenY);

/// <summary>The screen-filling holder of the camera panes, with the mission clock pane.</summary>
/// <remarks>Original source: <c>camera\camera.cpp</c>; 0x4c8 bytes.</remarks>
class MCMainWindow : public MCGuiHolderObject
{
public:
    ~MCMainWindow() override;
    /// <summary>Opens the holder over the whole application window.</summary>
    int32_t Init();
    /// <summary>aHolderObject::init, then makes the clock pane (40 wide, one line of lineFont).</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <summary>Frees the clock pane, then aHolderObject::destroy.</summary>
    void Destroy() override;
    /// <summary>Follows the application window's size on resize events.</summary>
    void HandleEvent(MCGuiEvent* event) override;
    /// <summary>Draws the panes, and the mission clock once per time step when the scenario has a time limit.
    /// </summary>
    void Display() override;
    /// <summary>Deactivates the inactive pane's camera when tiling, then aHolderObject::SetTiled.</summary>
    void SetTiled(int tiled) override;
    /// <summary>aHolderObject::Retile, then moves the clock pane to the active pane.</summary>
    void Retile() override;
    void SetVertical(int on) override;
    void SetActivePane(MCGuiObject* pane) override;
    void ZoomActivePane();
    using MCGuiHolderObject::Init;

    /// <summary>The pane the mission clock is drawn in.</summary>
    MCGuiObject* ClockPane = nullptr;
    /// <summary>The time the clock was last drawn (-1 by init).</summary>
    float LastClockTime = -1.0f;
};

/// <summary>
/// A camera: projects the terrain and objects into its <see cref="MCViewWindow"/>, either from a fixed position or
/// following (swooping to, scrolling after) a target object.
/// </summary>
/// <remarks>Original source: <c>camera\camera.cpp</c>, <c>camera\camera.h</c>; 0x100 bytes. The original's
/// destructor is not virtual and the vtable has none.</remarks>
class MCCamera
{
public:
    /// <summary>The inline constructor: clears the name, then the defaults of init().</summary>
    MCCamera() { Init(); }
    /// <summary>The inline destructor (CameraList::removeAll): destroys and frees the window; part number 0,
    /// object class id -1.</summary>
    ~MCCamera()
    {
        if (Window != nullptr)
        {
            Window->Destroy();
            delete Window;
            Window = nullptr;
        }

        PartNumber = 0;
        ObjectClassId = -1;
    }

    /// <summary>
    /// Takes the settings from <paramref name="data"/>, makes the view window and the terrain window; an object
    /// camera targets the last mech with a part number on the home team's side.
    /// </summary>
    virtual int32_t Init(MCCamData* data, int objectCamera);
    /// <summary>
    /// Reads the camera's FIT block: projection, position, colour, haze, scale, the window (made as a pane of the
    /// main holder, or its own titled window), and for an object camera its target and swoop/scroll settings.
    /// </summary>
    virtual int32_t Init(MCFitIniFile* cameraFile, int objectCamera, int32_t cameraId);
    /// <summary>The defaults: scale 1, 400x400 view, 30 degrees, haze 4/-2, class NO_CAMERA.</summary>
    virtual void Init();
    /// <summary>Moves the camera after its target (swoop, scroll or jump), then projects the terrain.</summary>
    /// <returns>0, or -1 for a camera class it doesn't handle.</returns>
    virtual int32_t Update();
    virtual void Render();
    /// <summary>Marks a ready camera active, shows its window, makes its terrain window and retargets it.</summary>
    virtual int32_t Activate();
    /// <summary>Not active; no target.</summary>
    virtual void Deactivate();
    /// <summary>Targets the object with that part number (when nonzero and a scenario runs) or id.</summary>
    virtual int32_t ChangeTarget(int32_t partNumber, int32_t objectId, int jumpTo);
    /// <summary>Targets <paramref name="target"/> (or the default target); jumps there, or updates first.</summary>
    virtual int32_t ChangeTarget(MCBaseObject* target, int jumpTo);

    /// <summary>0.5 at camera scale 1 (zoomed out), 1 otherwise.</summary>
    float GetScaleFactor();
    MCVector3D GetPosition();
    /// <summary>Allocates and fills <see cref="ScaleTable"/>.</summary>
    /// <returns>0, or 0x12120001 out of memory.</returns>
    int32_t BuildScaleTable();
    /// <summary>Does nothing.</summary>
    void PrepareBackground();
    /// <summary>The world point under a screen point.</summary>
    uint32_t InverseProject(MCVector2D& screenPos, MCVector3D& point);
    /// <summary>The screen position of vertex <paramref name="vertexNum"/> of block <paramref name="blockNum"/>
    /// (both clamped); 0 and 10000,10000 when it isn't on screen.</summary>
    int VertexProject(int32_t blockNum, int32_t vertexNum, MCVector2D& screenPos);
    /// <summary>Drops the target and moves the camera by a screen-aligned offset.</summary>
    void ScrollCamera(int32_t dx, int32_t dy);
    void SetPosition(MCVector3D newPosition);

    /// <summary>"PixelScalar" (1 by init).</summary>
    float PixelScalar = 1.0f;
    /// <summary>"ProjectionAngle" in degrees (30 by init).</summary>
    float ProjectionAngle = 30.0f;
    /// <summary>sin of the projection angle.</summary>
    float SinAngle = 0.0f;
    /// <summary>cos of the projection angle.</summary>
    float CosAngle = 0.0f;
    /// <summary>The view's width in pixels (400 by init).</summary>
    float ViewWidth = 400.0f;
    /// <summary>The view's height in pixels (400 by init).</summary>
    float ViewHeight = 400.0f;
    /// <summary>Half the view's width.</summary>
    float HalfWidth = 0.0f;
    /// <summary>Half the view's height.</summary>
    float HalfHeight = 0.0f;
    /// <summary>
    /// The camera position projected at full zoom (Terrain::projectTerrain's screen100): subtracted from a
    /// projected point to place it on screen.
    /// </summary>
    MCVector2D ScreenUL;
    /// <summary>The camera position projected at half zoom (projectTerrain's screen50), used when zoomed out.</summary>
    MCVector2D ScreenUL50;
    /// <summary><see cref="ScreenUL"/> before the last update (TerrainWindow::render takes the scroll from it).</summary>
    MCVector2D LastScreenUL;
    /// <summary><see cref="ScreenUL50"/> before the last update.</summary>
    MCVector2D LastScreenUL50;
    /// <summary>What the camera looks at.</summary>
    MCCameraClass CameraClass = NO_CAMERA;
    /// <summary>"BackgroundColor".</summary>
    uint8_t BackgroundColor = 0;
    /// <summary>"HazeLevel" (4 by init).</summary>
    int32_t HazeLevel = 4;
    /// <summary>"HazeInc" (-2 by init).</summary>
    int32_t HazeInc = -2;
    /// <summary>"MainWindow": the view is a pane of <see cref="MainHolder"/> rather than its own window.</summary>
    int32_t MainWindow = 0;
    /// <summary>The object followed.</summary>
    MCBaseObject* TargetObject = nullptr;
    /// <summary>"partNumber" of the target.</summary>
    int32_t PartNumber = 0;
    /// <summary>"ObjectClassId" of the target.</summary>
    int32_t ObjectClassId = 0;
    /// <summary>The target to fall back on when changeTarget is given none.</summary>
    MCBaseObject* DefaultTarget = nullptr;
    /// <summary>The target's facing when last followed.</summary>
    MCVector3D LastTargetFacing;
    /// <summary>Where a scroll toward the target started.</summary>
    MCVector3D ScrollStart;
    /// <summary>Set once the scroll has jumped the rest of the way.</summary>
    int32_t ScrollJumped = 0;
    /// <summary>"JumpThreshold": beyond this distance the camera jumps (250).</summary>
    float JumpThreshold = 250.0f;
    /// <summary>"CamSpeed" (50).</summary>
    float CamSpeed = 50.0f;
    /// <summary>"CamDistance": how far behind the target a swoop settles (50).</summary>
    float CamDistance = 50.0f;
    /// <summary>Set by changeTarget: the next update moves straight to the target.</summary>
    int32_t TargetChanged = 1;
    /// <summary>Forces a terrain window update on the next update (zoom).</summary>
    int32_t ForceUpdate = 0;
    /// <summary>"DistanceFactor" (25).</summary>
    float DistanceFactor = 25.0f;
    /// <summary>The terrain drawn through this camera.</summary>
    MCTerrainWindow* TerrainWindow = nullptr;
    /// <summary>"PositionX/Y/Z": where the camera looks.</summary>
    MCVector3D Position;
    /// <summary>The name CameraList::find matches (upper-cased, 7 characters).</summary>
    char Name[8] = {};
    /// <summary>1-based index in the camera file.</summary>
    int32_t CameraId = 0;
    /// <summary>"Ready": activate may make it active.</summary>
    uint8_t Ready = 0;
    /// <summary>Whether it is active (rendering).</summary>
    int32_t Active = 0;
    /// <summary>The pane it renders into.</summary>
    MCViewWindow* Window = nullptr;
    /// <summary>"CameraScale": 100, or 1 zoomed out (100 by init).</summary>
    int32_t CameraScale = 100;
    /// <summary>Swoops to the target: set when SwoopyCamOff is clear and the window is at least 150x150.</summary>
    int32_t Swoopy = 0;
    /// <summary>Set once a swoop has settled.</summary>
    int32_t SwoopDone = 0;
    /// <summary>"ScrollyCam": scroll to a new target rather than jump.</summary>
    int32_t ScrollyCam = 0;
    /// <summary>Set while scrolling to the target.</summary>
    int32_t Scrolling = 0;
    /// <summary>"SpeedFactor" (10 by init, 50 when missing).</summary>
    float SpeedFactor = 10.0f;
    /// <summary>"DistanceThreshold" (10).</summary>
    float DistanceThreshold = 10.0f;
    /// <summary>"MinScrollSpeed" (90).</summary>
    float MinScrollSpeed = 90.0f;
};

/// <summary>A camera read from a FIT file with no target. Never made by the game.</summary>
/// <remarks>Original source: <c>camera\camera.cpp</c>.</remarks>
class MCTerrainCamera : public MCCamera
{
public:
    /// <summary>Camera::init as a position camera.</summary>
    virtual int32_t Init(MCFitIniFile* cameraFile);
    using MCCamera::Init;
};
