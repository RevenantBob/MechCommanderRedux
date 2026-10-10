#include "stdafx.h"
#include "object/MCDebris.h"
#include "appear/MCAppearanceType.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/MCCamera.h"
#include "gui/MCGuiSystem.h"
#include "lib/MCDice.h"
#include "main/MCMissionGlobals.h"
#include "object/MCDebrisType.h"
#include "object/MCObjectDrawing.h"
#include "sprite/MCArmAppearance.h"

MCDebris::MCDebris()
{
    Frame.I = UnitX;
    Frame.J = UnitY;
    Frame.K = UnitZ;
}

MCDebris::~MCDebris() = default;

auto MCDebris::GetAppearance() -> MCAppearance*
{
    return Appearance.get();
}

auto MCDebris::OnScreen() -> int
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

auto MCDebris::Update() -> int32_t
{
    const auto* debrisType = static_cast<MCDebrisType*>(ObjType);

    if (JustCreated)
    {
        // The direction set by the creator becomes a velocity of armFallVelMag plus a random share.
        const float speed = static_cast<float>(RandomNumber(static_cast<int32_t>(debrisType->ArmFallVelRange))) +
                            debrisType->ArmFallVelMag;
        DecelRate = debrisType->ArmFallDecelRate;
        JustCreated = false;
        Velocity.X = speed * Velocity.X;
        Velocity.Y = speed * Velocity.Y;
        Velocity.Z = speed * Velocity.Z;
        Visible = OnScreen();
    }

    // Slide along the ground; once the fall animation is over, slow down (by decelRate) until stopped.
    if (!Stopped)
    {
        const float moveX = Velocity.X * FrameLength * WorldUnitsPerMeter;
        const float moveY = WorldUnitsPerMeter * FrameLength * Velocity.Y;

        if (FallDone)
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
                Stopped = true;
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
            FallDone = true;
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

    if (WindowsVisible == Turn && !JustCreated && Appearance != nullptr)
    {
        Appearance->Render(0);
    }
}

auto MCDebris::Init(MCObjectType* objType) -> int32_t
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
        return static_cast<int32_t>(0xdebb0002);
    }

    Appearance = std::make_unique<MCArmAppearance>();
    Appearance->Init(nullptr, nullptr);
    Appearance->OwnerObject = nullptr;

    if ((apprType->AppearanceNum & 0xff000000) != 0x6000000)
    {
        return static_cast<int32_t>(0xdebb0004);
    }

    if (const int32_t result = Appearance->Init(apprType, this); result != 0)
    {
        return result;
    }

    ObjectClass = MCObjectClass::Debris;
    return 0;
}

auto MCDebris::SetPaintScheme(int32_t paintScheme) -> void
{
    Appearance->FadeTableIndex = paintScheme;
}
