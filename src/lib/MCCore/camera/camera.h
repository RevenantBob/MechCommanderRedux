#pragma once

#include "gui/awindow.h"
#include "lib/cvmath.h"

class aMainWindow;
class BaseObject;
class Camera;
class CameraList;
class FitIniFile;
class TerrainWindow;
struct _pane;
struct _window;

/// <summary>What a camera looks at. The enumerator names are the port's.</summary>
enum CameraClass : int32_t
{
    /// <summary>Not set up yet (Camera::init).</summary>
    NO_CAMERA = -1,
    /// <summary>A fixed position (the "Camera%d" blocks, or an object camera without a target).</summary>
    POSITION_CAMERA = 0,
    /// <summary>Follows its target object (the "ObjectCamera%d" blocks).</summary>
    OBJECT_CAMERA = 1,
};

/// <summary>
/// A camera's settings as the scenario hands them to <see cref="Camera::init(CamData*, int)"/>. The field names are
/// the port's; the FIT keys of <see cref="Camera::init(FitIniFile*, int, int32_t)"/> they stand for are given.
/// </summary>
#pragma pack(push, 1)
struct CamData
{
    /// <summary>"PixelScalar".</summary>
    float pixelScalar; // +0x00
    /// <summary>"ProjectionAngle", in degrees.</summary>
    float projectionAngle; // +0x04
    /// <summary>"CameraScale".</summary>
    int32_t cameraScale; // +0x08
    /// <summary>The window's height, added to top for the bottom edge.</summary>
    float windowHeight; // +0x0c
    /// <summary>The window's width, added to left for the right edge.</summary>
    float windowWidth; // +0x10
    /// <summary>"WindowLeft".</summary>
    uint32_t windowLeft; // +0x14
    /// <summary>"WindowTop".</summary>
    uint32_t windowTop; // +0x18
    /// <summary>Not read by Camera::init.</summary>
    int32_t unknown1C; // +0x1c
    /// <summary>"BackgroundColor".</summary>
    uint8_t backgroundColor; // +0x20
    uint8_t pad21[3];        // +0x21
    /// <summary>"HazeLevel".</summary>
    int32_t hazeLevel; // +0x24
    /// <summary>"Ready".</summary>
    uint8_t ready;    // +0x28
    uint8_t pad29[3]; // +0x29
    /// <summary>"PositionX", "PositionY", "PositionZ".</summary>
    float position[3]; // +0x2c
    /// <summary>"partNumber" of the target.</summary>
    int32_t partNumber; // +0x38
    /// <summary>"ObjectClassId" of the target.</summary>
    int32_t objectClassId; // +0x3c
};
#pragma pack(pop)
static_assert(sizeof(CamData) == 0x40);

/// <summary>The camera the scene is being rendered through (CameraList::renderView sets it per camera).</summary>
extern Camera* eye;
/// <summary>7 x 256 bytes: for each divisor of <c>scaleDivisors</c>, i / (divisor + 1) (Camera::buildScaleTable).
/// Freed with the camera list.</summary>
extern std::vector<uint8_t> scaleTable;
/// <summary>The main window that holds the camera panes (made by the first camera with "MainWindow" set).</summary>
extern aMainWindow* mainHolder;
/// <summary>"SwoopyCamOff": when set, cameras don't swoop to their target.</summary>
extern int32_t leaveSwoopyOff;
/// <summary>Debug switch: draw the camera's circle.</summary>
extern int32_t drawCameraCircle;
/// <summary>The pause shape (cleared by CameraList::destroy).</summary>
extern uint8_t* pauseShape;
/// <summary>The "asked" shape, drawn while the game waits on a question (cleared by CameraList::destroy).</summary>
/// <remarks>DAT_007f0934; the name is the port's.</remarks>
extern uint8_t* askedShape;
/// <summary>The current zoom's scale factor.</summary>
extern float currentScaleFactor;
/// <summary>The last zoom setting.</summary>
extern int32_t lastZoom;
/// <summary>The game's cameras.</summary>
extern CameraList* cameraList;
/// <summary>The pane everything is drawn into: the rendering camera's window (Camera::render), else the screen.
/// </summary>
/// <remarks>Its owner file is unknown (it sits in the bss after the camera and colour globals); the port defines it
/// in camera.cpp.</remarks>
extern _pane* globalPane;
/// <summary>The window of <see cref="globalPane"/>.</summary>
extern _window* globalWindow;

/// <summary>Zooms the main window's active pane.</summary>
/// <remarks>MCX.EXE @ 0x006ad930</remarks>
void ToggleZoom();

/// <summary>
/// A camera pane: an <see cref="aTitleWindow"/> that renders the scenario through its <see cref="Camera"/>, and
/// passes events to the interface when it is the main view.
/// </summary>
/// <remarks>Original source: <c>camera\camera.cpp</c>, <c>camera\camera.h</c>; 0x500 bytes.</remarks>
class viewWindow : public aTitleWindow
{
public:
    /// <remarks>MCX.EXE @ 0x006ae4f0 (vector deleting destructor)</remarks>
    ~viewWindow() override;
    /// <summary>aObject::init, clears the fields; depth 5.</summary>
    /// <remarks>MCX.EXE @ 0x006ad150</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <summary>Deactivates the camera, then aObject::destroy.</summary>
    /// <remarks>MCX.EXE @ 0x006ad1b0</remarks>
    void destroy() override;
    /// <summary>
    /// Keeps a copy of the event; handles close (deactivates the camera), resize (the global pane), zoom (toggles
    /// the camera scale 1/100) and follow-off; hands the rest to the interface when <see cref="interfaceWindow"/>
    /// is set; a click swaps this camera's view with the active pane's.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006ad1e0</remarks>
    void handleEvent(aEvent* event) override;
    /// <summary>Snaps a titled window's size to multiples of 40, then resizes the camera's view.</summary>
    /// <remarks>MCX.EXE @ 0x006ad4e0</remarks>
    void resize(int32_t w, int32_t h) override;
    /// <summary>Renders the scenario through the camera, the children, and the status bar (erasing the last one
    /// drawn when there is none).</summary>
    /// <remarks>MCX.EXE @ 0x006ad5f0</remarks>
    void display() override;
    /// <remarks>MCX.EXE @ 0x006ad800</remarks>
    void leave() override;
    /// <summary>Draws a one-pixel frame (-1 edges mean the window's).</summary>
    /// <remarks>MCX.EXE @ 0x006ad860</remarks>
    void drawBox(uint8_t color, int32_t left, int32_t top, int32_t right, int32_t bottom) override;
    using aTitleWindow::drawBox;
    /// <remarks>MCX.EXE @ 0x006ae4e0 (inline in <c>camera\camera.h</c>)</remarks>
    Camera* GetCamera() override { return camera; }
    /// <summary>Sets the camera; the background colour follows its id (1: 0xef, 2: 0xf8).</summary>
    /// <remarks>MCX.EXE @ 0x006ad820</remarks>
    void setWindowCamera(Camera* newCamera);

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
    _pane* WorldFrame();
    /// <summary>
    /// Sizes the world surface from <see cref="ZoomHeight"/> (the view's height until the zoom starts) and the view's
    /// aspect; it is cleared when its size changes.
    /// </summary>
    void UpdateWorldSurface();
    /// <summary>The world surface's width and height.</summary>
    int32_t WorldWidth() const { return WorldWindow.x_max + 1; }
    int32_t WorldHeight() const { return WorldWindow.y_max + 1; }
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
    vector_2d ScreenToWorld(int32_t screenX, int32_t screenY);
    /// <summary>The window point (relative to this view) a point of the world surface is shown at.</summary>
    vector_2d WorldToWindow(vector_2d point);
    /// <summary>The screen point a point of the world surface is shown at.</summary>
    vector_2d WorldToScreen(vector_2d point);

    /// <summary>The drag-selection box's corners (x0 y0 where the drag started, x1 y1 where the mouse is), zeroed
    /// by init and when the drag ends; display draws it unless x1 and y1 are both 0.</summary>
    /// <remarks>Port: in the view's own (screen) coordinates, not the world surface's.</remarks>
    float selectionBox[4] = {}; // +0x4c0
    /// <summary>The camera shown.</summary>
    Camera* camera = nullptr; // +0x4d0
    /// <summary>Set for the main view: events go to the interface first.</summary>
    int32_t interfaceWindow = 0; // +0x4d4
    /// <summary>A copy of the last event handled.</summary>
    aEvent lastEvent; // +0x4d8

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
    _window WorldWindow{};
    _pane WorldPane{};
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
    _pane* Pane = nullptr;
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
vector_2d MCOverlayPoint(vector_2d point);
/// <summary>Port: <see cref="MCOverlayPoint"/> of one coordinate.</summary>
float MCOverlayX(float x);
float MCOverlayY(float y);
/// <summary>Port: the view of the main camera (camera 1), whose screen positions objects keep at index 0, or null.</summary>
viewWindow* MCMainView();
/// <summary>
/// Port: screen point (<paramref name="screenX"/>, <paramref name="screenY"/>) in <paramref name="window"/>'s
/// coordinates: for a camera's view, the point of its world surface under it (through the zoom), as the camera's
/// projections take it; for other windows, relative to the window's corner.
/// </summary>
vector_2d MCWindowPoint(aObject* window, int32_t screenX, int32_t screenY);

/// <summary>The screen-filling holder of the camera panes, with the mission clock pane.</summary>
/// <remarks>Original source: <c>camera\camera.cpp</c>; 0x4c8 bytes.</remarks>
class aMainWindow : public aHolderObject
{
public:
    /// <remarks>MCX.EXE @ 0x006ae520 (vector deleting destructor)</remarks>
    ~aMainWindow() override;
    /// <summary>Opens the holder over the whole application window.</summary>
    /// <remarks>MCX.EXE @ 0x006ad940 (inline in <c>camera\camera.h</c>)</remarks>
    int32_t init();
    /// <summary>aHolderObject::init, then makes the clock pane (40 wide, one line of lineFont).</summary>
    /// <remarks>MCX.EXE @ 0x006ad980</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <summary>Frees the clock pane, then aHolderObject::destroy.</summary>
    /// <remarks>MCX.EXE @ 0x006ada70</remarks>
    void destroy() override;
    /// <summary>Follows the application window's size on resize events.</summary>
    /// <remarks>MCX.EXE @ 0x006adab0</remarks>
    void handleEvent(aEvent* event) override;
    /// <summary>Draws the panes, and the mission clock once per time step when the scenario has a time limit.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006adaf0</remarks>
    void display() override;
    /// <summary>Deactivates the inactive pane's camera when tiling, then aHolderObject::SetTiled.</summary>
    /// <remarks>MCX.EXE @ 0x006add90</remarks>
    void SetTiled(int tiled) override;
    /// <summary>aHolderObject::Retile, then moves the clock pane to the active pane.</summary>
    /// <remarks>MCX.EXE @ 0x006add10</remarks>
    void Retile() override;
    /// <remarks>MCX.EXE @ 0x006add70</remarks>
    void SetVertical(int on) override;
    /// <remarks>MCX.EXE @ 0x006ade90</remarks>
    void SetActivePane(aObject* pane) override;
    /// <remarks>MCX.EXE @ 0x006ade00</remarks>
    void ZoomActivePane();
    using aHolderObject::init;

    /// <summary>The pane the mission clock is drawn in.</summary>
    aObject* clockPane = nullptr; // +0x4c0
    /// <summary>The time the clock was last drawn (-1 by init).</summary>
    float lastClockTime = -1.0f; // +0x4c4
};

/// <summary>
/// A camera: projects the terrain and objects into its <see cref="viewWindow"/>, either from a fixed position or
/// following (swooping to, scrolling after) a target object.
/// </summary>
/// <remarks>Original source: <c>camera\camera.cpp</c>, <c>camera\camera.h</c>; 0x100 bytes. The original's
/// destructor is not virtual and the vtable has none.</remarks>
class Camera
{
public:
    /// <summary>The inline constructor: clears the name, then the defaults of init().</summary>
    Camera() { init(); }
    /// <summary>The inline destructor (CameraList::removeAll): destroys and frees the window; part number 0,
    /// object class id -1.</summary>
    ~Camera()
    {
        if (window != nullptr)
        {
            window->destroy();
            delete window;
            window = nullptr;
        }

        partNumber = 0;
        objectClassId = -1;
    }

    /// <summary>
    /// Takes the settings from <paramref name="data"/>, makes the view window and the terrain window; an object
    /// camera targets the last mech with a part number on the home team's side.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006ae550</remarks>
    virtual int32_t init(CamData* data, int objectCamera);
    /// <summary>
    /// Reads the camera's FIT block: projection, position, colour, haze, scale, the window (made as a pane of the
    /// main holder, or its own titled window), and for an object camera its target and swoop/scroll settings.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006adf50</remarks>
    virtual int32_t init(FitIniFile* cameraFile, int objectCamera, int32_t cameraId);
    /// <summary>The defaults: scale 1, 400x400 view, 30 degrees, haze 4/-2, class NO_CAMERA.</summary>
    /// <remarks>MCX.EXE @ 0x006b0840 (inline in <c>camera\camera.h</c>)</remarks>
    virtual void init();
    /// <summary>Moves the camera after its target (swoop, scroll or jump), then projects the terrain.</summary>
    /// <returns>0, or -1 for an unknown camera class.</returns>
    /// <remarks>MCX.EXE @ 0x006aeb80</remarks>
    virtual int32_t update();
    /// <remarks>MCX.EXE @ 0x006af420</remarks>
    virtual void render();
    /// <summary>Marks a ready camera active, shows its window, makes its terrain window and retargets it.</summary>
    /// <remarks>MCX.EXE @ 0x006afa70</remarks>
    virtual int32_t activate();
    /// <summary>Not active; no target.</summary>
    /// <remarks>MCX.EXE @ 0x006afb20</remarks>
    virtual void deactivate();
    /// <summary>Targets the object with that part number (when nonzero and a scenario runs) or id.</summary>
    /// <remarks>MCX.EXE @ 0x006afbc0</remarks>
    virtual int32_t changeTarget(int32_t partNumber, int32_t objectId, int jumpTo);
    /// <summary>Targets <paramref name="target"/> (or the default target); jumps there, or updates first.</summary>
    /// <remarks>MCX.EXE @ 0x006afb30</remarks>
    virtual int32_t changeTarget(BaseObject* target, int jumpTo);

    /// <summary>0.5 at camera scale 1 (zoomed out), 1 otherwise.</summary>
    /// <remarks>MCX.EXE @ 0x0063df30 (inline in <c>camera\camera.h</c>)</remarks>
    float getScaleFactor();
    /// <remarks>MCX.EXE @ 0x006ad4b0 (inline in <c>camera\camera.h</c>)</remarks>
    vector_3d getPosition();
    /// <summary>Allocates and fills <see cref="scaleTable"/>.</summary>
    /// <returns>0, or 0x12120001 out of memory.</returns>
    /// <remarks>MCX.EXE @ 0x006adef0</remarks>
    int32_t buildScaleTable();
    /// <summary>Does nothing.</summary>
    /// <remarks>MCX.EXE @ 0x006ae7c0</remarks>
    void prepareBackground();
    /// <summary>The world point under a screen point.</summary>
    /// <remarks>MCX.EXE @ 0x006ae7d0</remarks>
    uint32_t inverseProject(vector_2d& screenPos, vector_3d& point);
    /// <summary>The screen position of vertex <paramref name="vertexNum"/> of block <paramref name="blockNum"/>
    /// (both clamped); 0 and 10000,10000 when it isn't on screen.</summary>
    /// <remarks>MCX.EXE @ 0x006afc80</remarks>
    int vertexProject(int32_t blockNum, int32_t vertexNum, vector_2d& screenPos);
    /// <summary>Drops the target and moves the camera by a screen-aligned offset.</summary>
    /// <remarks>MCX.EXE @ 0x006afd10</remarks>
    void scrollCamera(int32_t dx, int32_t dy);
    /// <remarks>MCX.EXE @ 0x006afd90</remarks>
    void setPosition(vector_3d newPosition);

    /// <summary>"PixelScalar" (1 by init).</summary>
    float pixelScalar = 1.0f; // +0x04
    /// <summary>"ProjectionAngle" in degrees (30 by init).</summary>
    float projectionAngle = 30.0f; // +0x08
    /// <summary>sin of the projection angle.</summary>
    float sinAngle = 0.0f; // +0x0c
    /// <summary>cos of the projection angle.</summary>
    float cosAngle = 0.0f; // +0x10
    /// <summary>The view's width in pixels (400 by init).</summary>
    float viewWidth = 400.0f; // +0x14
    /// <summary>The view's height in pixels (400 by init).</summary>
    float viewHeight = 400.0f; // +0x18
    /// <summary>Half the view's width.</summary>
    float halfWidth = 0.0f; // +0x1c
    /// <summary>Half the view's height.</summary>
    float halfHeight = 0.0f; // +0x20
    /// <summary>
    /// The camera position projected at full zoom (Terrain::projectTerrain's screen100): subtracted from a
    /// projected point to place it on screen.
    /// </summary>
    vector_2d screenUL; // +0x24
    /// <summary>The camera position projected at half zoom (projectTerrain's screen50), used when zoomed out.</summary>
    vector_2d screenUL50; // +0x2c
    /// <summary><see cref="screenUL"/> before the last update (TerrainWindow::render takes the scroll from it).</summary>
    vector_2d lastScreenUL; // +0x34
    /// <summary><see cref="screenUL50"/> before the last update.</summary>
    vector_2d lastScreenUL50; // +0x3c
    /// <summary>What the camera looks at.</summary>
    CameraClass cameraClass = NO_CAMERA; // +0x44
    /// <summary>"BackgroundColor".</summary>
    uint8_t backgroundColor = 0; // +0x48
    /// <summary>"HazeLevel" (4 by init).</summary>
    int32_t hazeLevel = 4; // +0x4c
    /// <summary>"HazeInc" (-2 by init).</summary>
    int32_t hazeInc = -2; // +0x50
    /// <summary>Zeroed by init.</summary>
    int32_t unknown54 = 0; // +0x54
    /// <summary>"MainWindow": the view is a pane of <see cref="mainHolder"/> rather than its own window.</summary>
    int32_t mainWindow = 0; // +0x58
    /// <summary>The object followed.</summary>
    BaseObject* targetObject = nullptr; // +0x5c
    /// <summary>"partNumber" of the target.</summary>
    int32_t partNumber = 0; // +0x60
    /// <summary>"ObjectClassId" of the target.</summary>
    int32_t objectClassId = 0; // +0x64
    /// <summary>The target to fall back on when changeTarget is given none.</summary>
    BaseObject* defaultTarget = nullptr; // +0x68
    /// <summary>Not accessed through Camera.</summary>
    int32_t unknown6C[3] = {}; // +0x6c
    /// <summary>The target's facing when last followed.</summary>
    vector_3d lastTargetFacing; // +0x78
    /// <summary>Zeroed by init; never read.</summary>
    vector_3d unknown84; // +0x84
    /// <summary>Where a scroll toward the target started.</summary>
    vector_3d scrollStart; // +0x90
    /// <summary>Set once the scroll has jumped the rest of the way.</summary>
    int32_t scrollJumped = 0; // +0x9c
    /// <summary>"JumpThreshold": beyond this distance the camera jumps (250).</summary>
    float jumpThreshold = 250.0f; // +0xa0
    /// <summary>"CamSpeed" (50).</summary>
    float camSpeed = 50.0f; // +0xa4
    /// <summary>"CamDistance": how far behind the target a swoop settles (50).</summary>
    float camDistance = 50.0f; // +0xa8
    /// <summary>Set by changeTarget: the next update moves straight to the target.</summary>
    int32_t targetChanged = 1; // +0xac
    /// <summary>Forces a terrain window update on the next update (zoom).</summary>
    int32_t forceUpdate = 0; // +0xb0
    /// <summary>"DistanceFactor" (25).</summary>
    float distanceFactor = 25.0f; // +0xb4
    /// <summary>The terrain drawn through this camera.</summary>
    TerrainWindow* terrainWindow = nullptr; // +0xb8
    /// <summary>"PositionX/Y/Z": where the camera looks.</summary>
    vector_3d position; // +0xbc
    /// <summary>The name CameraList::find matches (upper-cased, 7 characters).</summary>
    char name[8] = {}; // +0xc8
    /// <summary>1-based index in the camera file.</summary>
    int32_t cameraId = 0; // +0xd0
    /// <summary>"Ready": activate may make it active.</summary>
    uint8_t ready = 0; // +0xd4
    /// <summary>Whether it is active (rendering).</summary>
    int32_t active = 0; // +0xd8
    /// <summary>The pane it renders into.</summary>
    viewWindow* window = nullptr; // +0xdc
    /// <summary>"CameraScale": 100, or 1 zoomed out (100 by init).</summary>
    int32_t cameraScale = 100; // +0xe0
    /// <summary>Swoops to the target: set when SwoopyCamOff is clear and the window is at least 150x150.</summary>
    int32_t swoopy = 0; // +0xe4
    /// <summary>Set once a swoop has settled.</summary>
    int32_t swoopDone = 0; // +0xe8
    /// <summary>"ScrollyCam": scroll to a new target rather than jump.</summary>
    int32_t scrollyCam = 0; // +0xec
    /// <summary>Set while scrolling to the target.</summary>
    int32_t scrolling = 0; // +0xf0
    /// <summary>"SpeedFactor" (10 by init, 50 when missing).</summary>
    float speedFactor = 10.0f; // +0xf4
    /// <summary>"DistanceThreshold" (10).</summary>
    float distanceThreshold = 10.0f; // +0xf8
    /// <summary>"MinScrollSpeed" (90).</summary>
    float minScrollSpeed = 90.0f; // +0xfc
};

/// <summary>A camera read from a FIT file with no target. Never made by the game.</summary>
/// <remarks>Original source: <c>camera\camera.cpp</c>.</remarks>
class TerrainCamera : public Camera
{
public:
    /// <summary>Camera::init as a position camera.</summary>
    /// <remarks>MCX.EXE @ 0x006b02d0</remarks>
    virtual int32_t init(FitIniFile* cameraFile);
    using Camera::init;
};
