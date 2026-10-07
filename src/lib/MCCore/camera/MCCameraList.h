#pragma once

#include "camera/MCCamera.h"
#include "gui/MCGuiOwned.h"

class MCGuiObject;
class MCMainWindow;

/// <summary>
/// The game's cameras, read from the camera FIT file ("CameraInfo", then "Camera%d" / "ObjectCamera%d" blocks), and
/// the main window that holds their panes. The scenario installs one in the game context (<see cref="CameraList"/>),
/// then loads it.
/// </summary>
class MCCameraList
{
public:
    /// <summary>The most cameras a camera file may list (a camera window per view of the original's interface).</summary>
    static constexpr uint32_t MaxCameras = 4;

    MCCameraList();

    /// <summary>Deletes the cameras, clears the pause and asked shapes and destroys the main holder.</summary>
    ~MCCameraList();

    MCCameraList(const MCCameraList&) = delete;
    MCCameraList& operator=(const MCCameraList&) = delete;

    /// <summary>
    /// Reads <paramref name="fileName"/> (in the camera path): "NumCameras" (at most <see cref="MaxCameras"/>), and
    /// a camera per "Camera%d" or "ObjectCamera%d" block.
    /// </summary>
    std::expected<void, std::string> Load(std::string_view fileName);

    /// <summary>Appends <paramref name="camera"/>.</summary>
    void Add(std::unique_ptr<MCCamera> camera);

    /// <summary>The main holder, made (and opened over the whole screen) the first time a camera asks.</summary>
    std::expected<MCMainWindow*, std::string> EnsureMainHolder();

    /// <summary>The main holder (null until a main-window camera makes it).</summary>
    MCMainWindow* MainHolder() const { return _MainHolder.get(); }

    /// <summary>Activates every ready camera; the first becomes current.</summary>
    MCCamera* ActivateAllReady();

    /// <summary>Clears the drawn blocks and renders every active camera whose window is <paramref name="window"/>,
    /// setting <see cref="Eye"/> for each.</summary>
    void RenderView(MCGuiObject* window);

    /// <summary>Updates the active cameras.</summary>
    void Update();

    /// <summary>The camera with id <paramref name="cameraId"/>, or null.</summary>
    MCCamera* FindCameraFromIDNumber(int32_t cameraId);

    /// <summary>The cameras, in the file's order.</summary>
    const std::vector<std::unique_ptr<MCCamera>>& Cameras() const { return _Cameras; }

    /// <summary>The current camera.</summary>
    MCCamera* CurrentCamera = nullptr;

private:
    /// <summary>The cameras, in the file's order.</summary>
    std::vector<std::unique_ptr<MCCamera>> _Cameras;
    /// <summary>The main window holding the main cameras' panes.</summary>
    MCGuiOwned<MCMainWindow> _MainHolder;
};

/// <summary>The scenario's cameras (null outside a mission).</summary>
MCCameraList* CameraList();

/// <summary>Where the camera files are ("data\cameras\").</summary>
extern std::string CameraPath;
