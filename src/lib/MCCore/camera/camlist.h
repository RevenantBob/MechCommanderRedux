#pragma once

#include "camera/camera.h"
#include "lib/llist.h"

class aObject;

/// <summary>Where the camera files are ("data\cameras\").</summary>
/// <remarks>One of the 80-byte path globals at 0x007942ec.. (objectPath, missionPath, cameraPath, ...), whose owner
/// file is unknown; the port defines it in camlist.cpp.</remarks>
extern char cameraPath[80];

/// <summary>A camera's link in the <see cref="CameraList"/>.</summary>
/// <remarks>Original source: <c>camera\camlist.cpp</c>; 0x0c bytes.</remarks>
class CameraNode : public Link
{
public:
    /// <remarks>MCX.EXE @ 0x006b09a0 (vector deleting destructor)</remarks>
    ~CameraNode() override = default;

    /// <summary>The camera (owned by the list; removeAll deletes it).</summary>
    Camera* camera = nullptr; // +0x08
};

/// <summary>
/// The game's cameras: read from the camera FIT file ("CameraInfo", then "Camera%d" / "ObjectCamera%d" blocks),
/// with the active one and the one before it.
/// </summary>
/// <remarks>Original source: <c>camera\camlist.cpp</c>; 0x28 bytes.</remarks>
class CameraList : public LinkedList
{
public:
    /// <remarks>MCX.EXE @ 0x00736820 (vector deleting destructor)</remarks>
    ~CameraList() override { destroy(); }

    /// <summary>
    /// Deletes the cameras; frees the scale table and clears the pause shape; destroys the main holder.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b0330</remarks>
    void destroy();
    /// <summary>
    /// Reads <paramref name="fileName"/> (in cameraPath): "NumCameras" (at most 4), and a camera per "Camera%d" or
    /// "ObjectCamera%d" block. (The original first made the camera heap, of the scenario's CameraHeapSize.)
    /// </summary>
    /// <returns>0, -0x3544fffb for too many cameras, -0x3544fffd for a missing block, or the error of the file or the
    /// camera.</returns>
    /// <remarks>MCX.EXE @ 0x006b03c0</remarks>
    int32_t init(char* fileName);
    /// <summary>Appends a node for <paramref name="camera"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006b0950</remarks>
    int32_t add(Camera* camera);
    /// <summary>Unlinks the camera's node (not while destroying).</summary>
    /// <returns>0, or -0x3544ffff when it isn't in the list.</returns>
    /// <remarks>MCX.EXE @ 0x006b09d0</remarks>
    int32_t remove(Camera* camera);
    /// <summary>The camera named <paramref name="name"/> (first 7 characters, case-insensitive).</summary>
    /// <remarks>MCX.EXE @ 0x006b0a30</remarks>
    Camera* find(char* name);
    /// <summary>The first camera of <paramref name="cameraClass"/>, named <paramref name="name"/> when given.</summary>
    /// <remarks>MCX.EXE @ 0x006b0b20</remarks>
    Camera* find(CameraClass cameraClass, char* name);
    /// <summary>The next camera of <paramref name="cameraClass"/> after <paramref name="camera"/>, wrapping around.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b0c10</remarks>
    Camera* findNext(Camera* camera, CameraClass cameraClass);
    /// <summary>Deletes every camera and node; no current camera.</summary>
    /// <remarks>MCX.EXE @ 0x006b0cc0</remarks>
    void removeAll();
    /// <summary>Activates every ready camera; the first becomes current.</summary>
    /// <remarks>MCX.EXE @ 0x006b0d60</remarks>
    Camera* activateAllReady();
    /// <summary>Activates the camera named <paramref name="name"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006b0dd0</remarks>
    Camera* activate(char* name);
    /// <summary>Activates the first camera of <paramref name="cameraClass"/> (named <paramref name="name"/> when
    /// given).</summary>
    /// <remarks>MCX.EXE @ 0x006b0ee0</remarks>
    Camera* activate(CameraClass cameraClass, char* name);
    /// <summary>Deactivates every camera; no current camera.</summary>
    /// <remarks>MCX.EXE @ 0x006b1000</remarks>
    void deactivateAll();
    /// <summary>Deactivates the camera named <paramref name="name"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006b1050</remarks>
    void deactivate(char* name);
    /// <summary>Switches to the first camera of another class.</summary>
    /// <remarks>MCX.EXE @ 0x006b1150</remarks>
    int32_t changeCamera(CameraClass cameraClass, char* name);
    /// <summary>Deactivates the current camera and activates <paramref name="camera"/>, remembering the old one.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b1190</remarks>
    int32_t changeCamera(Camera* camera);
    /// <summary>Clears the render lists and renders every active camera whose window is <paramref name="window"/>,
    /// setting <see cref="eye"/> for each.</summary>
    /// <remarks>MCX.EXE @ 0x006b11e0</remarks>
    void renderView(aObject* window);
    /// <summary>Updates the active cameras; one that reports -0x3544fffe is deactivated, and the previous camera
    /// takes over if it was current.</summary>
    /// <remarks>MCX.EXE @ 0x006b1260</remarks>
    int32_t update();
    /// <remarks>MCX.EXE @ 0x006b12f0</remarks>
    Camera* findCameraFromIDNumber(int32_t cameraId);
    /// <remarks>MCX.EXE @ 0x006b1340</remarks>
    Camera* findCameraFromObject(BaseObject* object);
    /// <remarks>MCX.EXE @ 0x006b1390</remarks>
    Camera* findNextAvailable();
    /// <remarks>MCX.EXE @ 0x006b13e0</remarks>
    Camera* findTopCameraWindow();

    /// <summary>The current camera's class (-1 for none).</summary>
    CameraClass currentClass = NO_CAMERA; // +0x0c
    /// <summary>The camera that was current before (changeCamera).</summary>
    Camera* lastCamera = nullptr; // +0x10
    /// <summary>The current camera.</summary>
    Camera* currentCamera = nullptr; // +0x14
    /// <summary>"NumCameras".</summary>
    uint32_t numCameras = 0; // +0x20
    /// <summary>Set while destroy runs: remove then does nothing.</summary>
    int32_t destroying = 0; // +0x24
};
