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
/// Allocated from the camera heap.</summary>
extern uint8_t* scaleTable;
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

    /// <summary>The drag-selection box's corners (x0 y0 where the drag started, x1 y1 where the mouse is), zeroed
    /// by init and when the drag ends; display draws it unless x1 and y1 are both 0.</summary>
    float selectionBox[4] = {}; // +0x4c0
    /// <summary>The camera shown.</summary>
    Camera* camera = nullptr; // +0x4d0
    /// <summary>Set for the main view: events go to the interface first.</summary>
    int32_t interfaceWindow = 0; // +0x4d4
    /// <summary>A copy of the last event handled.</summary>
    aEvent lastEvent; // +0x4d8
};

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
/// <remarks>Original source: <c>camera\camera.cpp</c>, <c>camera\camera.h</c>; 0x100 bytes. Allocated from the
/// camera list's heap. The original's destructor is not virtual and the vtable has none.</remarks>
class Camera
{
public:
    /// <summary>Allocates from the camera list's heap.</summary>
    /// <remarks>MCX.EXE @ 0x006adeb0</remarks>
    static void* operator new(size_t size) noexcept;
    /// <summary>Frees into the camera list's heap.</summary>
    /// <remarks>MCX.EXE @ 0x006aded0</remarks>
    static void operator delete(void* ptr);
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
