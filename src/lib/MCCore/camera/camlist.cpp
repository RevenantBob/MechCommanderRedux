#include "stdafx.h"
#include "camera/camlist.h"
#include "lib/cident.h"
#include "lib/inifile.h"
#include "terrain/terrain.h"

char cameraPath[80] = "data\\cameras\\";

namespace
{
    /// <summary>The first 8 characters of <paramref name="name"/> equal the camera's name (strncmp's inline form).
    /// </summary>
    auto nameMatches(const char* name, const Camera* camera) -> bool
    {
        return strncmp(name, camera->name, 8) == 0;
    }
}

auto CameraList::destroy() -> void
{
    destroying = 1;
    removeAll();
    currentClass = NO_CAMERA;
    currentCamera = nullptr;
    lastCamera = nullptr;
    scaleTable = {};
    pauseShape = nullptr;
    askedShape = nullptr;

    if (mainHolder != nullptr)
    {
        mainHolder->destroy();
        delete mainHolder;
        mainHolder = nullptr;
    }
}

auto CameraList::init(char* fileName) -> int32_t
{
    int32_t result = 0;
    FullPathFileName cameraName;
    cameraName.init(cameraPath, fileName, ".fit");
    FitIniFile cameraFile;

    if ((result = cameraFile.open(cameraName, READ, 50)) != 0)
    {
        return result;
    }

    if ((result = cameraFile.seekBlock("CameraInfo")) != 0)
    {
        return result;
    }

    if ((result = cameraFile.readIdULong("NumCameras", numCameras)) != 0)
    {
        return result;
    }

    if (numCameras > 4)
    {
        return -0x3544fffb;
    }

    int32_t i = 0;

    if (static_cast<int32_t>(numCameras) <= 0)
    {
        return 0;
    }

    do
    {
        char blockName[32];
        Camera* camera;
        int objectCamera;
        sprintf(blockName, "Camera%d", i);

        if (cameraFile.seekBlock(blockName) == 0 && (camera = new Camera) != nullptr)
        {
            objectCamera = 0;
        }
        else
        {
            sprintf(blockName, "ObjectCamera%d", i);

            if (cameraFile.seekBlock(blockName) != 0)
            {
                return -0x3544fffd;
            }

            camera = new Camera;
            objectCamera = 1;

            if (camera == nullptr)
            {
                return -0x3544fffd;
            }
        }

        i++;

        if ((result = camera->init(&cameraFile, objectCamera, i)) != 0)
        {
            return result;
        }

        add(camera);
    } while (i < static_cast<int32_t>(numCameras));

    return 0;
}

auto CameraList::add(Camera* camera) -> int32_t
{
    auto* node = new CameraNode;

    if (node != nullptr)
    {
        node->camera = camera;
    }

    // MCX.EXE adds the node even when it could not be allocated.
    AddToTail(node);
    return 0;
}

auto CameraList::remove(Camera* camera) -> int32_t
{
    if (destroying != 0)
    {
        return 0;
    }

    Link* link = nullptr;
    Link* previous = nullptr;

    while (Traverse(link) != 0)
    {
        if (static_cast<CameraNode*>(link)->camera == camera)
        {
            Destroy(link, previous);
            return 0;
        }

        previous = link;
    }

    return -0x3544ffff;
}

auto CameraList::find(char* name) -> Camera*
{
    Link* link = nullptr;

    while (Traverse(link) != 0)
    {
        Camera* camera = static_cast<CameraNode*>(link)->camera;
        MCPort::StrUpr(camera->name);
        char wanted[8];
        strncpy(wanted, name, 7);
        wanted[7] = '\0';
        MCPort::StrUpr(wanted);

        if (nameMatches(wanted, camera))
        {
            return camera;
        }
    }

    return nullptr;
}

auto CameraList::find(CameraClass cameraClass, char* name) -> Camera*
{
    Link* link = nullptr;

    while (Traverse(link) != 0)
    {
        Camera* camera = static_cast<CameraNode*>(link)->camera;

        if (camera->cameraClass == cameraClass && (name == nullptr || nameMatches(name, camera)))
        {
            return camera;
        }
    }

    return nullptr;
}

auto CameraList::findNext(Camera* camera, CameraClass cameraClass) -> Camera*
{
    // Find the camera, then look after it and wrap around to it.
    Link* link = nullptr;
    int more = Traverse(link);

    while (more != 0 && static_cast<CameraNode*>(link)->camera != camera)
    {
        more = Traverse(link);
    }

    Link* start = link;

    while (Traverse(link) != 0)
    {
        if (static_cast<CameraNode*>(link)->camera->cameraClass == cameraClass)
        {
            return static_cast<CameraNode*>(link)->camera;
        }
    }

    link = nullptr;

    while (Traverse(link) != 0 && link != start)
    {
        if (static_cast<CameraNode*>(link)->camera->cameraClass == cameraClass)
        {
            return static_cast<CameraNode*>(link)->camera;
        }
    }

    return nullptr;
}

auto CameraList::removeAll() -> void
{
    Link* link = nullptr;

    while (Traverse(link) != 0)
    {
        Camera* camera = static_cast<CameraNode*>(link)->camera;

        if (camera != nullptr)
        {
            delete camera;
        }

        Destroy(link, nullptr);
        link = nullptr;
    }

    currentCamera = nullptr;
}

auto CameraList::activateAllReady() -> Camera*
{
    currentCamera = nullptr;
    Link* link = nullptr;

    while (Traverse(link) != 0)
    {
        Camera* camera = static_cast<CameraNode*>(link)->camera;

        if (camera->ready != 0 && camera->activate() == 0 && currentCamera == nullptr)
        {
            currentCamera = camera;
        }
    }

    return currentCamera;
}

auto CameraList::activate(char* name) -> Camera*
{
    Link* link = nullptr;

    while (Traverse(link) != 0)
    {
        Camera* camera = static_cast<CameraNode*>(link)->camera;

        if (nameMatches(name, camera))
        {
            if (camera->active == 0 && camera->activate() == 0 && currentCamera == nullptr)
            {
                currentCamera = camera;
            }

            return camera;
        }
    }

    return nullptr;
}

auto CameraList::activate(CameraClass cameraClass, char* name) -> Camera*
{
    Link* link = nullptr;

    while (Traverse(link) != 0)
    {
        Camera* camera = static_cast<CameraNode*>(link)->camera;

        if (camera->cameraClass != cameraClass || (name != nullptr && !nameMatches(name, camera)))
        {
            continue;
        }

        if (camera->active == 0)
        {
            if (camera->activate() != 0)
            {
                return nullptr;
            }

            if (currentCamera == nullptr)
            {
                currentCamera = camera;
            }
        }

        return camera;
    }

    return nullptr;
}

auto CameraList::deactivateAll() -> void
{
    Link* link = nullptr;

    while (Traverse(link) != 0)
    {
        Camera* camera = static_cast<CameraNode*>(link)->camera;

        if (camera->active != 0)
        {
            camera->deactivate();
        }
    }

    currentCamera = nullptr;
}

auto CameraList::deactivate(char* name) -> void
{
    Link* link = nullptr;

    while (Traverse(link) != 0)
    {
        Camera* camera = static_cast<CameraNode*>(link)->camera;

        if (nameMatches(name, camera) && camera->active != 0)
        {
            camera->deactivate();

            if (camera == currentCamera)
            {
                currentCamera = nullptr;
            }
        }
    }
}

auto CameraList::changeCamera(CameraClass cameraClass, char* name) -> int32_t
{
    if (currentClass != cameraClass)
    {
        Camera* camera = find(cameraClass, name);

        if (camera != nullptr)
        {
            return changeCamera(camera);
        }
    }

    return 0;
}

auto CameraList::changeCamera(Camera* camera) -> int32_t
{
    int32_t result = 0;
    Camera* oldCamera = currentCamera;

    if (oldCamera != camera && camera != nullptr)
    {
        if (oldCamera != nullptr)
        {
            oldCamera->deactivate();
        }

        result = camera->activate();

        if (result == 0)
        {
            lastCamera = oldCamera;
            currentCamera = camera;
            currentClass = camera->cameraClass;
        }
    }

    return result;
}

auto CameraList::renderView(aObject* window) -> void
{
    Link* link = nullptr;
    clearMoverList();
    clearList();
    Camera* savedEye = eye;

    while (Traverse(link) != 0)
    {
        eye = static_cast<CameraNode*>(link)->camera;

        if (eye->active != 0 && eye->window == window)
        {
            eye->render();
        }

        eye = savedEye;
    }

    eye = savedEye;
}

auto CameraList::update() -> int32_t
{
    int32_t result = 0;
    Link* link = nullptr;

    while (Traverse(link) != 0)
    {
        Camera* camera = static_cast<CameraNode*>(link)->camera;

        if (camera->active == 0)
        {
            continue;
        }

        result = camera->update();

        if (result != -0x3544fffe)
        {
            continue;
        }

        // The camera lost its target: back to the camera before it.
        camera->deactivate();

        if (camera == currentCamera && lastCamera != nullptr && (result = lastCamera->activate()) == 0)
        {
            currentCamera = lastCamera;
            lastCamera = nullptr;
        }
    }

    return result;
}

auto CameraList::findCameraFromIDNumber(int32_t cameraId) -> Camera*
{
    Link* link = nullptr;

    while (Traverse(link) != 0)
    {
        if (static_cast<CameraNode*>(link)->camera->cameraId == cameraId)
        {
            return static_cast<CameraNode*>(link)->camera;
        }
    }

    return nullptr;
}

auto CameraList::findCameraFromObject(BaseObject* object) -> Camera*
{
    if (object == nullptr)
    {
        return nullptr;
    }

    Link* link = nullptr;

    while (Traverse(link) != 0)
    {
        if (object == static_cast<CameraNode*>(link)->camera->targetObject)
        {
            return static_cast<CameraNode*>(link)->camera;
        }
    }

    return nullptr;
}

auto CameraList::findNextAvailable() -> Camera*
{
    Link* link = nullptr;

    while (Traverse(link) != 0)
    {
        if (static_cast<CameraNode*>(link)->camera->active == 0)
        {
            return static_cast<CameraNode*>(link)->camera;
        }
    }

    return nullptr;
}

auto CameraList::findTopCameraWindow() -> Camera*
{
    Camera* topCamera = nullptr;
    Link* link = nullptr;

    while (Traverse(link) != 0)
    {
        // Original behaviour (OB-035): the best camera so far is reset on every pass and then compared through
        // (a null pointer crash for any ready, active camera). Nothing calls this. Port fix: a null best takes the
        // camera, so it returns the last camera when that one is ready and active, else null.
        topCamera = nullptr;
        Camera* camera = static_cast<CameraNode*>(link)->camera;

        if (camera->ready == 0 || camera->active == 0)
        {
            continue;
        }

        if (topCamera == nullptr || topCamera->window->depth() < camera->window->depth())
        {
            topCamera = camera;
        }
    }

    return topCamera;
}
