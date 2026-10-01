#include "stdafx.h"
#include "object/light.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "gui/asystem.h"
#include "lib/file.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "sprite/actor.h"
#include "terrain/terrain.h"

//---------------------------------------------------------------------------
// LightType
//---------------------------------------------------------------------------

auto LightType::createInstance() -> BaseObject*
{
    auto* newLight = new Light;

    if (newLight == nullptr)
    {
        return nullptr;
    }

    if (newLight->init(this) != 0)
    {
        return nullptr;
    }

    newLight->idNumber = NextIdNumber++;
    return newLight;
}

auto LightType::destroy() -> void
{
}

auto LightType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile lightFile;
    int32_t result = lightFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = lightFile.seekBlock("LightData")) != 0)
    {
        return result;
    }

    if ((result = lightFile.readIdBoolean("OneShotFlag", oneShotFlag)) != 0)
    {
        return result;
    }

    if ((result = lightFile.readIdFloat("AltitudeOffset", altitudeOffset)) != 0)
    {
        return result;
    }

    return ObjectType::init(&lightFile);
}

auto LightType::handleCollision(GameObject*, GameObject*) -> int
{
    return 0;
}

auto LightType::handleDestruction(GameObject*, GameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Light
//---------------------------------------------------------------------------

auto Light::init() -> void
{
    appearance = nullptr;
    justCreated = 1;
}

auto Light::onScreen() -> int
{
    Camera* camera = cameraList->findCameraFromIDNumber(1);

    if (camera == nullptr || camera->active == 0)
    {
        return 0;
    }

    vector_2d screen100;
    vector_2d screen50;

    if (land != nullptr)
    {
        land->projectTerrain(position, screen100, screen50);
    }

    float screenY;

    if (camera->cameraScale == 1)
    {
        screenPos.x = (screen50.x - camera->screenUL50.x) + camera->halfWidth;
        screenY = screen50.y - camera->screenUL50.y;
    }
    else
    {
        screenPos.x = (screen100.x - camera->screenUL.x) + camera->halfWidth;
        screenY = screen100.y - camera->screenUL.y;
    }

    screenPos.y = screenY + camera->halfHeight;

    if (appearance != nullptr && appearance->recalcBounds(camera) != 0)
    {
        windowsVisible = turn;
        return 1;
    }

    return 0;
}

auto Light::update() -> int32_t
{
    if (finished != 0)
    {
        return 1;
    }

    // The owner sets the position every frame; the light sits altitudeOffset above it.
    position.z = static_cast<LightType*>(objType)->altitudeOffset + position.z;
    const int visibleNow = onScreen();

    if (justCreated != 0)
    {
        justCreated = 0;
        collisionsOn = 0;
    }

    appearance->visible = visibleNow;

    // A one-shot light stops drawing once its animation ends.
    if (appearance->update() == 0 && static_cast<LightType*>(objType)->oneShotFlag != 0)
    {
        finished = 1;
    }

    return 1;
}

auto Light::render() -> void
{
    if (gamePaused != 0)
    {
        onScreen();
    }

    if (justCreated == 0 && windowsVisible == turn && finished == 0)
    {
        appearance->render(-500);
    }
}

auto Light::destroy() -> void
{
    delete appearance;
    appearance = nullptr;
}

auto Light::init(ObjectType* objType) -> int32_t
{
    int32_t result = GameObject::init(objType);

    if (result != 0)
    {
        return result;
    }

    justCreated = 1;
    collisionsOn = 0;
    AppearanceType* apprType = appearanceTypeList->getAppearance(objType->appearName, 0);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0003);
    }

    if ((apprType->appearanceNum & 0xff000000) != 0x2000000)
    {
        return static_cast<int32_t>(0xdcdc0005);
    }

    auto* vfxAppearance = new VFXAppearance;
    appearance = vfxAppearance;

    if (vfxAppearance == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0004);
    }

    vfxAppearance->init(nullptr, nullptr);

    if ((result = vfxAppearance->init(apprType, this)) != 0)
    {
        return result;
    }

    objectClass = LIGHT;
    finished = 0;
    return 0;
}
