#include "stdafx.h"
#include "object/debris.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "gui/asystem.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "sprite/armactor.h"
#include "terrain/terrain.h"

//---------------------------------------------------------------------------
// DebrisType
//---------------------------------------------------------------------------

MCDebrisType::MCDebrisType()
{
    ArmFallYaw = 0.0f;
    ArmFallYawRange = 0.0f;
    ArmFallVelMag = 0.0f;
    ArmFallVelRange = 0.0f;
    ArmFallDecelRate = 0.0f;
}

auto MCDebrisType::CreateInstance() -> MCBaseObject*
{
    auto* newDebris = new MCDebris;

    if (newDebris == nullptr)
    {
        return nullptr;
    }

    if (newDebris->Init(this) != 0)
    {
        return nullptr;
    }

    newDebris->IdNumber = NextIdNumber++;
    return newDebris;
}

auto MCDebrisType::Destroy() -> void
{
}

auto MCDebrisType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile debrisFile;
    int32_t result = debrisFile.Open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = debrisFile.SeekBlock("ArmFall")) != 0)
    {
        return result;
    }

    if ((result = debrisFile.ReadIdFloat("ArmFallYaw", ArmFallYaw)) != 0)
    {
        return result;
    }

    if ((result = debrisFile.ReadIdFloat("ArmFallYawRange", ArmFallYawRange)) != 0)
    {
        return result;
    }

    if ((result = debrisFile.ReadIdFloat("ArmFallVelMag", ArmFallVelMag)) != 0)
    {
        return result;
    }

    if ((result = debrisFile.ReadIdFloat("ArmFallVelRange", ArmFallVelRange)) != 0)
    {
        return result;
    }

    if ((result = debrisFile.ReadIdFloat("ArmFallDecelRate", ArmFallDecelRate)) != 0)
    {
        return result;
    }

    return MCObjectType::Init(&debrisFile);
}

auto MCDebrisType::HandleCollision(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

auto MCDebrisType::HandleDestruction(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Debris
//---------------------------------------------------------------------------

MCDebris::MCDebris()
{
    Frame.I = UnitX;
    Frame.J = UnitY;
    Frame.K = UnitZ;
    JustCreated = 1;
    Visible = 1;
    Velocity.X = 0.0f;
    Appearance = nullptr;
    DecelRate = 0.0f;
    Velocity.Z = 0.0f;
    Velocity.Y = 0.0f;
    Stopped = 0;
    FallDone = 0;
}

auto MCDebris::Init() -> void
{
}

auto MCDebris::OnScreen() -> int
{
    MCCamera* camera = CameraList->FindCameraFromIDNumber(1);

    if (camera == nullptr || camera->Active == 0)
    {
        return 0;
    }

    MCVector2D screen100;
    MCVector2D screen50;

    if (Land != nullptr)
    {
        Land->ProjectTerrain(Position, screen100, screen50);
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

auto MCDebris::Update() -> int32_t
{
    const auto* debrisType = static_cast<MCDebrisType*>(ObjType);

    if (JustCreated != 0)
    {
        // The direction set by the creator becomes a velocity of armFallVelMag plus a random share.
        const float speed = static_cast<float>(RandomNumber(static_cast<int32_t>(debrisType->ArmFallVelRange))) +
                            debrisType->ArmFallVelMag;
        DecelRate = debrisType->ArmFallDecelRate;
        JustCreated = 0;
        Velocity.X = speed * Velocity.X;
        Velocity.Y = speed * Velocity.Y;
        Velocity.Z = speed * Velocity.Z;
        Visible = OnScreen();
    }

    // Slide along the ground; once the fall animation is over, slow down (by decelRate) until stopped.
    if (Stopped == 0)
    {
        const float moveX = Velocity.X * FrameLength * WorldUnitsPerMeter;
        const float moveY = WorldUnitsPerMeter * FrameLength * Velocity.Y;

        if (FallDone != 0)
        {
            const float change = FrameLength * DecelRate;
            float dirX = Velocity.X;
            float dirY = Velocity.Y;
            float dirZ = Velocity.Z;
            const float speed = std::sqrt(dirX * dirX + dirZ * dirZ + dirY * dirY);

            if (speed != 0.0f)
            {
                dirX /= speed;
                dirY /= speed;
                dirZ /= speed;
            }

            Velocity.Z = 0.0f;
            Velocity.X = dirX * change + Velocity.X;
            Velocity.Y = dirY * change + Velocity.Y;
            Velocity.Z = dirZ * change + Velocity.Z;

            if (std::sqrt(Velocity.Z * Velocity.Z + Velocity.Y * Velocity.Y + Velocity.X * Velocity.X) <= 0.0f)
            {
                Velocity.Z = 0.0f;
                Velocity.Y = 0.0f;
                Velocity.X = 0.0f;
                Stopped = 1;
            }
        }

        Position.X = moveX + Position.X;
        Position.Y = moveY + Position.Y;
    }

    Visible = OnScreen();

    if (Appearance != nullptr)
    {
        Appearance->Visible = Visible;

        if (Appearance->Update() == 0)
        {
            FallDone = 1;
        }
    }

    return 1;
}

auto MCDebris::RandomAngle(float& angle) -> void
{
    const auto* debrisType = static_cast<MCDebrisType*>(ObjType);
    const float yaw = debrisType->ArmFallYaw + angle;
    angle = yaw;

    if (RollDice(50) != 0)
    {
        angle = static_cast<float>(RandomNumber(static_cast<int32_t>(debrisType->ArmFallYawRange))) + yaw;
    }
    else
    {
        angle = yaw - static_cast<float>(RandomNumber(static_cast<int32_t>(debrisType->ArmFallYawRange)));
    }
}

auto MCDebris::Render() -> void
{
    if (GamePaused != 0)
    {
        OnScreen();
    }

    if (WindowsVisible == Turn && JustCreated == 0 && Appearance != nullptr)
    {
        Appearance->Render(0);
    }
}

auto MCDebris::Destroy() -> void
{
    delete Appearance;
    Appearance = nullptr;
}

auto MCDebris::Init(MCObjectType* objType) -> int32_t
{
    int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    JustCreated = 1;
    CollisionsOn = 0;
    MCAppearanceType* apprType = AppearanceTypeList->GetAppearance(objType->AppearName, 0);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdebb0002);
    }

    auto* armAppearance = new MCArmAppearance;
    Appearance = armAppearance;

    if (armAppearance == nullptr)
    {
        return static_cast<int32_t>(0xdebb0003);
    }

    armAppearance->Init(nullptr, nullptr);
    armAppearance->OwnerObject = nullptr;

    if ((apprType->AppearanceNum & 0xff000000) != 0x6000000)
    {
        return static_cast<int32_t>(0xdebb0004);
    }

    if ((result = armAppearance->Init(apprType, this)) != 0)
    {
        return result;
    }

    ObjectClass = DEBRIS;
    return 0;
}

auto MCDebris::SetPaintScheme(int32_t paintScheme) -> void
{
    static_cast<MCArmAppearance*>(Appearance)->FadeTableIndex = paintScheme;
}
