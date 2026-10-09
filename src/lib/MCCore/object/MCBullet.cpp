#include "stdafx.h"
#include "object/MCBullet.h"
#include "ai/MCMoveSystem.h"
#include "ai/MCScenarioMap.h"
#include "appear/MCAppearanceType.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/MCCamera.h"
#include "engine/MCCraterManager.h"
#include "lib/MCFrameOfRef.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/MCBulletType.h"
#include "object/MCExplosion.h"
#include "object/MCMechGameSystem.h"
#include "object/MCObjectDrawing.h"
#include "object/MCObjectSystem.h"
#include "object/MCSmoke.h"
#include "sound/MCSoundSystem.h"
#include "sprite/MCArmAppearance.h"
#include "terrain/MCTerrain.h"

namespace
{
    /// <summary>Radians to degrees, as MCX.EXE stores it.</summary>
    constexpr double RadiansToDegrees = 0x1.ca5dc1a6402aap+5;
} // namespace

MCBullet::MCBullet() = default;

MCBullet::~MCBullet() = default;

auto MCBullet::IsVisible() -> bool
{
    MCCamera* camera = ActiveMainCamera();

    if (camera == nullptr)
    {
        return false;
    }

    ScreenPos = ProjectToScreen(Position, *camera);

    if (Appearance != nullptr && Appearance->RecalcBounds(camera) == 0)
    {
        return false;
    }

    WindowsVisible = Turn;
    return true;
}

auto MCBullet::Update() -> int32_t
{
    const auto* bulletType = static_cast<MCBulletType*>(ObjType);

    if (JustCreated)
    {
        JustCreated = false;
        CollisionsOn = 0;

        if (Owner != nullptr)
        {
            Position = Owner->GetPositionFromHS(static_cast<uint32_t>(OwnerHotSpot));
        }

        if (bulletType->SoundEffectId != 0xffffffff)
        {
            SoundSystem()->PlayDigitalSample(bulletType->SoundEffectId, 1, this, 0, 0);
        }

        BulletPosition = Position;
        ClosestDistanceSq = 1.0e8f;
    }

    MCGameObject* shooter = Owner;

    if (shooter != nullptr)
    {
        Position = shooter->GetPositionFromHS(static_cast<uint32_t>(OwnerHotSpot));
    }

    const bool visibleNow = IsVisible();

    if (Appearance != nullptr)
    {
        Appearance->Visible = visibleNow ? 1 : 0;
        Appearance->Update();
    }

    // Fly toward the target (following it if it moves); once the ground distance stops shrinking, we're there.
    MCVector3D from = BulletPosition;

    if (Target != nullptr)
    {
        // Port fix (OB-017): follow the hot spot the hit effect plays at (the original used ownerHotSpot).
        const uint32_t hotSpot =
            Target->ObjectClass == MCObjectClass::Turret ? 0xffffffff : static_cast<uint32_t>(TargetHotSpot);
        SetTargetPosition(Target->GetPositionFromHS(hotSpot));
    }

    // Port fix: the original leaves the destination uninitialised when there is no target position.
    const MCVector3D to = TargetPosition.value_or(MCVector3D(0.0f, 0.0f, 0.0f));
    const double step = static_cast<double>(bulletType->Velocity) * FrameLength;
    const double dxWide = static_cast<double>(to.X) - from.X;
    const auto dx = static_cast<float>(dxWide);
    const float dy = to.Y - from.Y;
    const float dz = to.Z - from.Z;
    const double dxSq = static_cast<double>(dx) * dx;
    const double dySq = static_cast<double>(dy) * dy;
    const double groundDistanceSq = dxSq + dySq;
    const bool arrived = ClosestDistanceSq <= groundDistanceSq;

    if (!arrived)
    {
        ClosestDistanceSq = static_cast<float>(groundDistanceSq);
    }

    int32_t result = arrived ? 0 : 1;

    MCVector3D velocity;
    double unitX = dxWide;
    velocity.Y = dy;
    velocity.Z = dz;
    const double length = std::sqrt(static_cast<double>(dz) * dz + dxSq + dySq);
    const auto lengthF = static_cast<float>(length);

    if (length != 0.0)
    {
        unitX = static_cast<double>(dx) / lengthF;
        velocity.Y = static_cast<float>(static_cast<double>(dy) / lengthF);
        velocity.Z = static_cast<float>(static_cast<double>(dz) / lengthF);
    }

    velocity.X = static_cast<float>(unitX * step);
    velocity.Y = static_cast<float>(velocity.Y * step);
    velocity.Z = static_cast<float>(velocity.Z * step);
    BulletPosition.X += velocity.X;
    BulletPosition.Y += velocity.Y;
    BulletPosition.Z += velocity.Z;

    if (Smoke != nullptr)
    {
        result = 1;
        Smoke->SetOwnerPosition(BulletPosition);
        Smoke->SetOwnerVelocity(velocity);
        Smoke->Update();

        if (arrived)
        {
            Smoke->StopSmoking();
            result = 0;
        }
    }

    if (Light != nullptr)
    {
        MCVector3D lightPos = BulletPosition;
        Light->SetPosition(lightPos);
        Light->Update();
    }

    // A mech facing the other way draws the bullet mirrored.
    DrawRotation = -150;

    // Port fix: the original reads the owner's class without checking it for null.
    if (shooter != nullptr && shooter->ObjectClass == MCObjectClass::BattleMech)
    {
        const MCFrameOfRef frame = shooter->GetFrame();
        float cosFacing = UnitX.Y * frame.I.Y + UnitX.X * frame.I.X + UnitX.Z * frame.I.Z;

        if (cosFacing < -1.0f)
        {
            cosFacing = -1.0f;
        }

        if (1.0f < cosFacing)
        {
            cosFacing = 1.0f;
        }

        double facing = AcosMatherr(static_cast<double>(cosFacing)) * RadiansToDegrees;

        if (frame.I.Y < 0.0f)
        {
            facing = -facing;
        }

        DrawRotation = std::abs(facing) <= 90.0 ? -150 : 150;
    }

    if (result != 0)
    {
        return result;
    }

    // Arrived: apply the shots (in multiplayer only the server does, and sends them on).
    if (Target != nullptr && (MPlayer == nullptr || MPlayer->IsServer != 0))
    {
        const int sendChunks = MPlayer != nullptr ? 1 : 0;

        for (MCWeaponShotInfo& shot : Shots)
        {
            Target->HandleWeaponHit(&shot, sendChunks);
        }
    }

    std::unique_ptr<MCGameObject> effect = CreateObject(
        static_cast<int32_t>(Target == nullptr ? bulletType->BulletMissEffect : bulletType->BulletHitEffect));

    if (effect == nullptr)
    {
        return result;
    }

    if (Target != nullptr)
    {
        const uint32_t hotSpot =
            Target->ObjectClass == MCObjectClass::Turret ? 0xffffffff : static_cast<uint32_t>(TargetHotSpot);
        MCVector3D hitPos = Target->GetPositionFromHS(hotSpot);
        effect->SetPosition(hitPos);
    }
    else if (TargetPosition.has_value())
    {
        effect->SetPosition(*TargetPosition);
    }

    AddToDefaultList(std::move(effect));

    // A miss leaves a crater and sets off a live mine where it lands.
    if (Target == nullptr && TargetPosition.has_value())
    {
        CraterManager()->AddCrater(6, *TargetPosition, 1);

        int32_t tileR = 0;
        int32_t tileC = 0;
        int32_t cellR = 0;
        int32_t cellC = 0;
        GameMap()->WorldToMapPos(*TargetPosition, tileR, tileC, cellR, cellC);

        // Port fix: a miss can land off the map, where the original reads (and writes) outside it.
        if (!GameMap()->OnMap(tileR, tileC))
        {
            return result;
        }

        MCMapTile& tile = GameMap()->Map[GameMap()->Width * tileR + tileC];

        if ((tile.Overlay & 0x1800) == 0x1000 || (tile.Overlay & 0x6000) == 0x4000)
        {
            CreateExplosion(MineExplosion, *TargetPosition, MineSplashDamage, WorldUnitsPerMeter * MineSplashRange);
            tile.Overlay |= 0x1800;
            tile.Overlay |= 0x6000;
        }
    }

    return result;
}

auto MCBullet::Render() -> void
{
    const bool firstFrame = JustCreated;

    if (!firstFrame && Appearance != nullptr)
    {
        Appearance->Render(DrawRotation);
    }

    if (Smoke != nullptr && !firstFrame)
    {
        Smoke->Render();
    }

    if (Light != nullptr)
    {
        Light->Render();
    }
}

auto MCBullet::Init(MCObjectType* objType) -> int32_t
{
    if (const int32_t result = MCGameObject::Init(objType); result != 0)
    {
        return result;
    }

    JustCreated = true;

    if (MCAppearanceType* apprType = AppearanceTypeList()->GetAppearance(objType->AppearName); apprType != nullptr)
    {
        if ((apprType->AppearanceNum & 0xff000000) != 0x6000000)
        {
            return static_cast<int32_t>(0xdcdc0010);
        }

        Appearance = std::make_unique<MCArmAppearance>();
        Appearance->Init(nullptr, nullptr);
        Appearance->OwnerObject = nullptr;

        if (const int32_t result = Appearance->Init(apprType, this); result != 0)
        {
            return result;
        }

        Appearance->OwnerObject = this;
    }

    const auto* bulletType = static_cast<MCBulletType*>(objType);

    if (static_cast<int32_t>(bulletType->SmokeObjectId) != -1)
    {
        Smoke = CreateObjectAs<MCSmoke>(static_cast<int32_t>(bulletType->SmokeObjectId));
    }

    if (static_cast<int32_t>(bulletType->LightObjectId) != -1)
    {
        Light = CreateObject(static_cast<int32_t>(bulletType->LightObjectId));
    }

    ObjectClass = MCObjectClass::Bullet;
    return 0;
}

auto MCBullet::Connect(MCGameObject* source, MCVector3D targetPos, int32_t sourceHotSpot) -> void
{
    Owner = source;
    OwnerHotSpot = sourceHotSpot;
    SetTargetPosition(targetPos);
}
