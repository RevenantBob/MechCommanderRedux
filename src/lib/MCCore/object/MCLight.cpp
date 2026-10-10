#include "stdafx.h"
#include "object/MCLight.h"
#include "appear/MCAppearanceType.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/MCCamera.h"
#include "gui/MCGuiSystem.h"
#include "main/MCMissionGlobals.h"
#include "object/MCLightType.h"
#include "object/MCObjectDrawing.h"
#include "sprite/MCVfxAppearance.h"

MCLight::MCLight() = default;

MCLight::~MCLight() = default;

auto MCLight::OnScreen() -> int
{
    MCCamera* camera = ActiveMainCamera();

    if (camera == nullptr)
    {
        return 0;
    }

    ScreenPos = ProjectToScreen(Position, *camera);

    if (Appearance != nullptr && Appearance->RecalcBounds(camera) != 0)
    {
        WindowsVisible = Turn;
        return 1;
    }

    return 0;
}

auto MCLight::Update() -> int32_t
{
    if (Finished)
    {
        return 1;
    }

    // The owner sets the position every frame; the light sits altitudeOffset above it.
    const auto* lightType = static_cast<MCLightType*>(ObjType);
    Position.Z = lightType->AltitudeOffset + Position.Z;
    const int visibleNow = OnScreen();

    if (JustCreated)
    {
        JustCreated = false;
        CollisionsOn = 0;
    }

    Appearance->Visible = visibleNow;

    // A one-shot light stops drawing once its animation ends.
    if (Appearance->Update() == 0 && lightType->OneShotFlag)
    {
        Finished = true;
    }

    return 1;
}

auto MCLight::Render() -> void
{
    if (GamePaused != 0)
    {
        OnScreen();
    }

    if (!JustCreated && WindowsVisible == Turn && !Finished)
    {
        Appearance->Render(-500);
    }
}

auto MCLight::Init(MCObjectType* objType) -> int32_t
{
    if (const int32_t result = MCGameObject::Init(objType); result != 0)
    {
        return result;
    }

    JustCreated = true;
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

    Appearance = std::make_unique<MCVfxAppearance>();
    Appearance->Init(nullptr, nullptr);

    if (const int32_t result = Appearance->Init(apprType, this); result != 0)
    {
        return result;
    }

    ObjectClass = MCObjectClass::Light;
    Finished = false;
    return 0;
}
