#include "stdafx.h"
#include "object/light.h"
#include "appear/MCAppearanceType.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/MCCamera.h"
#include "camera/MCCameraList.h"
#include "gui/asystem.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "main/main.h"
#include "sprite/MCVfxAppearance.h"
#include "terrain/MCTerrain.h"

//---------------------------------------------------------------------------
// LightType
//---------------------------------------------------------------------------

auto MCLightType::CreateInstance() -> MCBaseObject*
{
    auto* newLight = new MCLight;

    if (newLight == nullptr)
    {
        return nullptr;
    }

    if (newLight->Init(this) != 0)
    {
        return nullptr;
    }

    newLight->IdNumber = NextIdNumber++;
    return newLight;
}

auto MCLightType::Destroy() -> void
{
}

auto MCLightType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile lightFile;
    int32_t result = lightFile.Open(objFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if ((result = lightFile.SeekBlock("LightData")) != 0)
    {
        return result;
    }

    if ((result = lightFile.ReadIdBoolean("OneShotFlag", OneShotFlag)) != 0)
    {
        return result;
    }

    if ((result = lightFile.ReadIdFloat("AltitudeOffset", AltitudeOffset)) != 0)
    {
        return result;
    }

    return MCObjectType::Init(&lightFile);
}

auto MCLightType::HandleCollision(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

auto MCLightType::HandleDestruction(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Light
//---------------------------------------------------------------------------

auto MCLight::Init() -> void
{
    Appearance = nullptr;
    JustCreated = 1;
}

auto MCLight::OnScreen() -> int
{
    MCCamera* camera = CameraList()->FindCameraFromIDNumber(1);

    if (camera == nullptr || camera->Active == 0)
    {
        return 0;
    }

    MCVector2D screen100;
    MCVector2D screen50;

    if (Terrain() != nullptr)
    {
        Terrain()->ProjectTerrain(Position, screen100, screen50);
    }

    float screenY;

    if (camera->CameraScale == 1)
    {
        ScreenPos.X = (screen50.X - camera->ScreenUL50.X) + camera->HalfWidth;
        screenY = screen50.Y - camera->ScreenUL50.Y;
    }
    else
    {
        ScreenPos.X = (screen100.X - camera->ScreenUL.X) + camera->HalfWidth;
        screenY = screen100.Y - camera->ScreenUL.Y;
    }

    ScreenPos.Y = screenY + camera->HalfHeight;

    if (Appearance != nullptr && Appearance->RecalcBounds(camera) != 0)
    {
        WindowsVisible = Turn;
        return 1;
    }

    return 0;
}

auto MCLight::Update() -> int32_t
{
    if (Finished != 0)
    {
        return 1;
    }

    // The owner sets the position every frame; the light sits altitudeOffset above it.
    Position.Z = static_cast<MCLightType*>(ObjType)->AltitudeOffset + Position.Z;
    const int visibleNow = OnScreen();

    if (JustCreated != 0)
    {
        JustCreated = 0;
        CollisionsOn = 0;
    }

    Appearance->Visible = visibleNow;

    // A one-shot light stops drawing once its animation ends.
    if (Appearance->Update() == 0 && static_cast<MCLightType*>(ObjType)->OneShotFlag != 0)
    {
        Finished = 1;
    }

    return 1;
}

auto MCLight::Render() -> void
{
    if (GamePaused != 0)
    {
        OnScreen();
    }

    if (JustCreated == 0 && WindowsVisible == Turn && Finished == 0)
    {
        Appearance->Render(-500);
    }
}

auto MCLight::Destroy() -> void
{
    delete Appearance;
    Appearance = nullptr;
}

auto MCLight::Init(MCObjectType* objType) -> int32_t
{
    int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    JustCreated = 1;
    CollisionsOn = 0;
    MCAppearanceType* apprType = AppearanceTypeList()->GetAppearance(objType->AppearName);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0003);
    }

    if ((apprType->AppearanceNum & 0xff000000) != 0x2000000)
    {
        return static_cast<int32_t>(0xdcdc0005);
    }

    auto* vfxAppearance = new MCVfxAppearance;
    Appearance = vfxAppearance;

    if (vfxAppearance == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0004);
    }

    vfxAppearance->Init(nullptr, nullptr);

    if ((result = vfxAppearance->Init(apprType, this)) != 0)
    {
        return result;
    }

    ObjectClass = LIGHT;
    Finished = 0;
    return 0;
}
