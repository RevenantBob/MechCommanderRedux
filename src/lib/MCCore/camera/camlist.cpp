#include "stdafx.h"
#include "camera/camlist.h"
#include "lib/MCIDString.h"
#include "lib/MCFitIniFile.h"
#include "terrain/terrain.h"

char CameraPath[80] = "data\\cameras\\";

namespace
{
    /// <summary>The first 8 characters of <paramref name="name"/> equal the camera's name (strncmp's inline form).
    /// </summary>
    auto NameMatches(const char* name, const MCCamera* camera) -> bool
    {
        return strncmp(name, camera->Name, 8) == 0;
    }
}

auto MCCameraList::Destroy() -> void
{
    Destroying = 1;
    RemoveAll();
    CurrentClass = NO_CAMERA;
    CurrentCamera = nullptr;
    LastCamera = nullptr;
    ScaleTable = {};
    PauseShape = nullptr;
    AskedShape = nullptr;

    if (MainHolder != nullptr)
    {
        MainHolder->Destroy();
        delete MainHolder;
        MainHolder = nullptr;
    }
}

auto MCCameraList::Init(char* fileName) -> int32_t
{
    int32_t result = 0;
    std::string cameraName;
    cameraName = GamePath(CameraPath, fileName, ".fit");
    MCFitIniFile cameraFile;

    if ((result = cameraFile.Open(cameraName)) != 0)
    {
        return result;
    }

    if ((result = cameraFile.SeekBlock("CameraInfo")) != 0)
    {
        return result;
    }

    if ((result = cameraFile.ReadIdULong("NumCameras", NumCameras)) != 0)
    {
        return result;
    }

    if (NumCameras > 4)
    {
        return -0x3544fffb;
    }

    int32_t i = 0;

    if (static_cast<int32_t>(NumCameras) <= 0)
    {
        return 0;
    }

    do
    {
        char blockName[32];
        MCCamera* camera;
        int objectCamera;
        sprintf(blockName, "Camera%d", i);

        if (cameraFile.SeekBlock(blockName) == 0 && (camera = new MCCamera) != nullptr)
        {
            objectCamera = 0;
        }
        else
        {
            sprintf(blockName, "ObjectCamera%d", i);

            if (cameraFile.SeekBlock(blockName) != 0)
            {
                return -0x3544fffd;
            }

            camera = new MCCamera;
            objectCamera = 1;

            if (camera == nullptr)
            {
                return -0x3544fffd;
            }
        }

        i++;

        if ((result = camera->Init(&cameraFile, objectCamera, i)) != 0)
        {
            return result;
        }

        Add(camera);
    } while (i < static_cast<int32_t>(NumCameras));

    return 0;
}

auto MCCameraList::Add(MCCamera* camera) -> int32_t
{
    auto* node = new MCCameraNode;

    if (node != nullptr)
    {
        node->Camera = camera;
    }

    // MCX.EXE adds the node even when it could not be allocated.
    AddToTail(node);
    return 0;
}

auto MCCameraList::Remove(MCCamera* camera) -> int32_t
{
    if (Destroying != 0)
    {
        return 0;
    }

    MCLink* link = nullptr;
    MCLink* previous = nullptr;

    while (Traverse(link) != 0)
    {
        if (static_cast<MCCameraNode*>(link)->Camera == camera)
        {
            MCLinkedList::Destroy(link, previous);
            return 0;
        }

        previous = link;
    }

    return -0x3544ffff;
}

auto MCCameraList::Find(char* name) -> MCCamera*
{
    MCLink* link = nullptr;

    while (Traverse(link) != 0)
    {
        MCCamera* camera = static_cast<MCCameraNode*>(link)->Camera;
        MCPort::StrUpr(camera->Name);
        char wanted[8];
        strncpy(wanted, name, 7);
        wanted[7] = '\0';
        MCPort::StrUpr(wanted);

        if (NameMatches(wanted, camera))
        {
            return camera;
        }
    }

    return nullptr;
}

auto MCCameraList::Find(MCCameraClass cameraClass, char* name) -> MCCamera*
{
    MCLink* link = nullptr;

    while (Traverse(link) != 0)
    {
        MCCamera* camera = static_cast<MCCameraNode*>(link)->Camera;

        if (camera->CameraClass == cameraClass && (name == nullptr || NameMatches(name, camera)))
        {
            return camera;
        }
    }

    return nullptr;
}

auto MCCameraList::FindNext(MCCamera* camera, MCCameraClass cameraClass) -> MCCamera*
{
    // Find the camera, then look after it and wrap around to it.
    MCLink* link = nullptr;
    int more = Traverse(link);

    while (more != 0 && static_cast<MCCameraNode*>(link)->Camera != camera)
    {
        more = Traverse(link);
    }

    MCLink* start = link;

    while (Traverse(link) != 0)
    {
        if (static_cast<MCCameraNode*>(link)->Camera->CameraClass == cameraClass)
        {
            return static_cast<MCCameraNode*>(link)->Camera;
        }
    }

    link = nullptr;

    while (Traverse(link) != 0 && link != start)
    {
        if (static_cast<MCCameraNode*>(link)->Camera->CameraClass == cameraClass)
        {
            return static_cast<MCCameraNode*>(link)->Camera;
        }
    }

    return nullptr;
}

auto MCCameraList::RemoveAll() -> void
{
    MCLink* link = nullptr;

    while (Traverse(link) != 0)
    {
        MCCamera* camera = static_cast<MCCameraNode*>(link)->Camera;

        if (camera != nullptr)
        {
            delete camera;
        }

        MCLinkedList::Destroy(link, nullptr);
        link = nullptr;
    }

    CurrentCamera = nullptr;
}

auto MCCameraList::ActivateAllReady() -> MCCamera*
{
    CurrentCamera = nullptr;
    MCLink* link = nullptr;

    while (Traverse(link) != 0)
    {
        MCCamera* camera = static_cast<MCCameraNode*>(link)->Camera;

        if (camera->Ready != 0 && camera->Activate() == 0 && CurrentCamera == nullptr)
        {
            CurrentCamera = camera;
        }
    }

    return CurrentCamera;
}

auto MCCameraList::Activate(char* name) -> MCCamera*
{
    MCLink* link = nullptr;

    while (Traverse(link) != 0)
    {
        MCCamera* camera = static_cast<MCCameraNode*>(link)->Camera;

        if (NameMatches(name, camera))
        {
            if (camera->Active == 0 && camera->Activate() == 0 && CurrentCamera == nullptr)
            {
                CurrentCamera = camera;
            }

            return camera;
        }
    }

    return nullptr;
}

auto MCCameraList::Activate(MCCameraClass cameraClass, char* name) -> MCCamera*
{
    MCLink* link = nullptr;

    while (Traverse(link) != 0)
    {
        MCCamera* camera = static_cast<MCCameraNode*>(link)->Camera;

        if (camera->CameraClass != cameraClass || (name != nullptr && !NameMatches(name, camera)))
        {
            continue;
        }

        if (camera->Active == 0)
        {
            if (camera->Activate() != 0)
            {
                return nullptr;
            }

            if (CurrentCamera == nullptr)
            {
                CurrentCamera = camera;
            }
        }

        return camera;
    }

    return nullptr;
}

auto MCCameraList::DeactivateAll() -> void
{
    MCLink* link = nullptr;

    while (Traverse(link) != 0)
    {
        MCCamera* camera = static_cast<MCCameraNode*>(link)->Camera;

        if (camera->Active != 0)
        {
            camera->Deactivate();
        }
    }

    CurrentCamera = nullptr;
}

auto MCCameraList::Deactivate(char* name) -> void
{
    MCLink* link = nullptr;

    while (Traverse(link) != 0)
    {
        MCCamera* camera = static_cast<MCCameraNode*>(link)->Camera;

        if (NameMatches(name, camera) && camera->Active != 0)
        {
            camera->Deactivate();

            if (camera == CurrentCamera)
            {
                CurrentCamera = nullptr;
            }
        }
    }
}

auto MCCameraList::ChangeCamera(MCCameraClass cameraClass, char* name) -> int32_t
{
    if (CurrentClass != cameraClass)
    {
        MCCamera* camera = Find(cameraClass, name);

        if (camera != nullptr)
        {
            return ChangeCamera(camera);
        }
    }

    return 0;
}

auto MCCameraList::ChangeCamera(MCCamera* camera) -> int32_t
{
    int32_t result = 0;
    MCCamera* oldCamera = CurrentCamera;

    if (oldCamera != camera && camera != nullptr)
    {
        if (oldCamera != nullptr)
        {
            oldCamera->Deactivate();
        }

        result = camera->Activate();

        if (result == 0)
        {
            LastCamera = oldCamera;
            CurrentCamera = camera;
            CurrentClass = camera->CameraClass;
        }
    }

    return result;
}

auto MCCameraList::RenderView(MCGuiObject* window) -> void
{
    MCLink* link = nullptr;
    ClearMoverList();
    ClearBlockList();
    MCCamera* savedEye = Eye;

    while (Traverse(link) != 0)
    {
        Eye = static_cast<MCCameraNode*>(link)->Camera;

        if (Eye->Active != 0 && Eye->Window == window)
        {
            Eye->Render();
        }

        Eye = savedEye;
    }

    Eye = savedEye;
}

auto MCCameraList::Update() -> int32_t
{
    int32_t result = 0;
    MCLink* link = nullptr;

    while (Traverse(link) != 0)
    {
        MCCamera* camera = static_cast<MCCameraNode*>(link)->Camera;

        if (camera->Active == 0)
        {
            continue;
        }

        result = camera->Update();

        if (result != -0x3544fffe)
        {
            continue;
        }

        // The camera lost its target: back to the camera before it.
        camera->Deactivate();

        if (camera == CurrentCamera && LastCamera != nullptr && (result = LastCamera->Activate()) == 0)
        {
            CurrentCamera = LastCamera;
            LastCamera = nullptr;
        }
    }

    return result;
}

auto MCCameraList::FindCameraFromIDNumber(int32_t cameraId) -> MCCamera*
{
    MCLink* link = nullptr;

    while (Traverse(link) != 0)
    {
        if (static_cast<MCCameraNode*>(link)->Camera->CameraId == cameraId)
        {
            return static_cast<MCCameraNode*>(link)->Camera;
        }
    }

    return nullptr;
}

auto MCCameraList::FindCameraFromObject(MCBaseObject* object) -> MCCamera*
{
    if (object == nullptr)
    {
        return nullptr;
    }

    MCLink* link = nullptr;

    while (Traverse(link) != 0)
    {
        if (object == static_cast<MCCameraNode*>(link)->Camera->TargetObject)
        {
            return static_cast<MCCameraNode*>(link)->Camera;
        }
    }

    return nullptr;
}

auto MCCameraList::FindNextAvailable() -> MCCamera*
{
    MCLink* link = nullptr;

    while (Traverse(link) != 0)
    {
        if (static_cast<MCCameraNode*>(link)->Camera->Active == 0)
        {
            return static_cast<MCCameraNode*>(link)->Camera;
        }
    }

    return nullptr;
}

auto MCCameraList::FindTopCameraWindow() -> MCCamera*
{
    MCCamera* topCamera = nullptr;
    MCLink* link = nullptr;

    while (Traverse(link) != 0)
    {
        // Original behaviour (OB-035): the best camera so far is reset on every pass and then compared through
        // (a null pointer crash for any ready, active camera). Nothing calls this. Port fix: a null best takes the
        // camera, so it returns the last camera when that one is ready and active, else null.
        topCamera = nullptr;
        MCCamera* camera = static_cast<MCCameraNode*>(link)->Camera;

        if (camera->Ready == 0 || camera->Active == 0)
        {
            continue;
        }

        if (topCamera == nullptr || topCamera->Window->Depth() < camera->Window->Depth())
        {
            topCamera = camera;
        }
    }

    return topCamera;
}
