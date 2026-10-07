#pragma once

#include "camera/camera.h"
#include "lib/MCLinkedList.h"

class MCGuiObject;

/// <summary>Where the camera files are ("data\cameras\").</summary>
/// <remarks>One of the 80-byte path globals at 0x007942ec.. (objectPath, missionPath, cameraPath, ...), for which the
/// binary kept no owner file; the port defines it in camlist.cpp.</remarks>
extern char CameraPath[80];

/// <summary>A camera's link in the <see cref="MCCameraList"/>.</summary>
/// <remarks>Original source: <c>camera\camlist.cpp</c>; 0x0c bytes.</remarks>
class MCCameraNode : public MCLink
{
public:
    ~MCCameraNode() override = default;

    /// <summary>The camera (owned by the list; removeAll deletes it).</summary>
    MCCamera* Camera = nullptr;
};

/// <summary>
/// The game's cameras: read from the camera FIT file ("CameraInfo", then "Camera%d" / "ObjectCamera%d" blocks),
/// with the active one and the one before it.
/// </summary>
/// <remarks>Original source: <c>camera\camlist.cpp</c>; 0x28 bytes.</remarks>
class MCCameraList : public MCLinkedList
{
public:
    ~MCCameraList() override { Destroy(); }

    /// <summary>
    /// Deletes the cameras; frees the scale table and clears the pause shape; destroys the main holder.
    /// </summary>
    void Destroy();
    /// <summary>
    /// Reads <paramref name="fileName"/> (in cameraPath): "NumCameras" (at most 4), and a camera per "Camera%d" or
    /// "ObjectCamera%d" block. (The original first made the camera heap, of the scenario's CameraHeapSize.)
    /// </summary>
    /// <returns>0, -0x3544fffb for too many cameras, -0x3544fffd for a missing block, or the error of the file or the
    /// camera.</returns>
    int32_t Init(char* fileName);
    /// <summary>Appends a node for <paramref name="camera"/>.</summary>
    int32_t Add(MCCamera* camera);
    /// <summary>Unlinks the camera's node (not while destroying).</summary>
    /// <returns>0, or -0x3544ffff when it isn't in the list.</returns>
    int32_t Remove(MCCamera* camera);
    /// <summary>The camera named <paramref name="name"/> (first 7 characters, case-insensitive).</summary>
    MCCamera* Find(char* name);
    /// <summary>The first camera of <paramref name="cameraClass"/>, named <paramref name="name"/> when given.</summary>
    MCCamera* Find(MCCameraClass cameraClass, char* name);
    /// <summary>The next camera of <paramref name="cameraClass"/> after <paramref name="camera"/>, wrapping around.
    /// </summary>
    MCCamera* FindNext(MCCamera* camera, MCCameraClass cameraClass);
    /// <summary>Deletes every camera and node; no current camera.</summary>
    void RemoveAll();
    /// <summary>Activates every ready camera; the first becomes current.</summary>
    MCCamera* ActivateAllReady();
    /// <summary>Activates the camera named <paramref name="name"/>.</summary>
    MCCamera* Activate(char* name);
    /// <summary>Activates the first camera of <paramref name="cameraClass"/> (named <paramref name="name"/> when
    /// given).</summary>
    MCCamera* Activate(MCCameraClass cameraClass, char* name);
    /// <summary>Deactivates every camera; no current camera.</summary>
    void DeactivateAll();
    /// <summary>Deactivates the camera named <paramref name="name"/>.</summary>
    void Deactivate(char* name);
    /// <summary>Switches to the first camera of another class.</summary>
    int32_t ChangeCamera(MCCameraClass cameraClass, char* name);
    /// <summary>Deactivates the current camera and activates <paramref name="camera"/>, remembering the old one.
    /// </summary>
    int32_t ChangeCamera(MCCamera* camera);
    /// <summary>Clears the render lists and renders every active camera whose window is <paramref name="window"/>,
    /// setting <see cref="Eye"/> for each.</summary>
    void RenderView(MCGuiObject* window);
    /// <summary>Updates the active cameras; one that reports -0x3544fffe is deactivated, and the previous camera
    /// takes over if it was current.</summary>
    int32_t Update();
    MCCamera* FindCameraFromIDNumber(int32_t cameraId);
    MCCamera* FindCameraFromObject(MCBaseObject* object);
    MCCamera* FindNextAvailable();
    MCCamera* FindTopCameraWindow();

    /// <summary>The current camera's class (-1 for none).</summary>
    MCCameraClass CurrentClass = NO_CAMERA;
    /// <summary>The camera that was current before (changeCamera).</summary>
    MCCamera* LastCamera = nullptr;
    /// <summary>The current camera.</summary>
    MCCamera* CurrentCamera = nullptr;
    /// <summary>"NumCameras".</summary>
    uint32_t NumCameras = 0;
    /// <summary>Set while destroy runs: remove then does nothing.</summary>
    int32_t Destroying = 0;
};
